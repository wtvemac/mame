// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"

#include "lptsocket.h"

#include "multibyte.h"
#include "osdcore.h"

#include <algorithm>
#include <chrono>
#include <thread>

DEFINE_DEVICE_TYPE(LPT_SOCKET, lpt_socket_device, "lpt_socket", "Parallel port socket (pin passthrough)")

lpt_socket_device::lpt_socket_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, LPT_SOCKET, tag, owner, clock),
	device_centronics_peripheral_interface(mconfig, *this),
	m_stream(*this, "stream"),
	m_exchange_timer(nullptr),
	m_data(0x00),
	m_control(0x00),
	m_drive(DRIVE_ALL),
	m_dirty(true),
	m_connected(false),
	m_hello_sent(false),
	m_got_peripheral_state(false),
	m_exchanging(false),
	m_barrier_id(0),
	m_next_deadline(NO_DEADLINE),
	m_completed_time(NO_DEADLINE),
	m_receive_pos(0),
	m_receive_size(0)
{
}

void lpt_socket_device::device_add_mconfig(machine_config &config)
{
	BITBANGER(config, m_stream);
	m_stream->set_stream_mode(true);
	m_stream->mounted_callback().set(FUNC(lpt_socket_device::stream_mounted));
}

void lpt_socket_device::device_start()
{
	m_exchange_timer = timer_alloc(FUNC(lpt_socket_device::exchange_tick), this);

	// The far end is a live session, so nothing here goes in a save state.
}

void lpt_socket_device::device_stop()
{
	m_connected = false;
}

uint64_t lpt_socket_device::emulated_ns() const
{
	return machine().time().as_ticks(1'000'000'000);
}

void lpt_socket_device::stream_mounted(int state)
{
	// Every mount is a new session.
	m_connected = state != 0;
	m_hello_sent = false;
	m_got_peripheral_state = false;
	m_dirty = true;
	m_send_queue.clear();
	m_receive_pos = 0;
	m_receive_size = 0;
	m_next_deadline = NO_DEADLINE;
	m_completed_time = NO_DEADLINE;
	m_barrier_id = 0;

	if (m_exchange_timer)
		m_exchange_timer->adjust(m_connected ? attotime::zero : attotime::never);

	if (!m_connected)
	{
		// Back to an empty slot.
		const uint8_t released[STATE_BYTES] = { 0xff, STATUS_RELEASED, DRIVE_NONE, 0 };
		present(released);
	}
}

void lpt_socket_device::change_data_bit(unsigned bit, int state)
{
	const uint8_t data = (m_data & ~(1 << bit)) | (state ? (1 << bit) : 0);
	m_dirty |= data != m_data;
	m_data = data;

	if (m_connected && !m_exchanging)
		m_exchange_timer->adjust(attotime::zero);
}

void lpt_socket_device::change_control(unsigned bit, int state)
{
	// Sent by host_output_changed once the controller has set all of its lines.
	const uint8_t control = (m_control & ~(1 << bit)) | (state ? (1 << bit) : 0);
	m_dirty |= control != m_control;
	m_control = control;
}

void lpt_socket_device::host_data_dir(bool host_drives)
{
	// The levels from before the turnaround go out as their own record.
	queue_host_state();

	const uint8_t drive = host_drives ? DRIVE_ALL : DRIVE_NONE;
	m_dirty |= drive != m_drive;
	m_drive = drive;

	queue_host_state();
}

void lpt_socket_device::host_sample_lines()
{
	// Settled lines read again at the same emulated time don't need another round trip.
	// The endpoint's own changes still wait until the next exchange()
	if (m_send_queue.empty() && !m_dirty && m_completed_time == emulated_ns())
		return;

	exchange();
}

TIMER_CALLBACK_MEMBER(lpt_socket_device::exchange_tick)
{
	exchange();
}

void lpt_socket_device::queue_frame(uint8_t type, uint32_t id, uint64_t time, const uint8_t *payload, uint32_t size)
{
	if (m_send_queue.size() + HEADER_BYTES + size > MAX_QUEUED_BYTES)
		throw emu_fatalerror("Parallel socket: outgoing event queue overflow");

	const size_t pos = m_send_queue.size();
	m_send_queue.resize(pos + HEADER_BYTES + size);

	uint8_t *const frame = m_send_queue.data() + pos;
	put_u16be(frame + HEADER_LENGTH, size);
	frame[HEADER_TYPE] = type;
	frame[HEADER_FLAGS] = 0;
	put_u32be(frame + HEADER_ID, id);
	put_u64be(frame + HEADER_TIME, time);

	if (size)
		std::copy_n(payload, size, frame + HEADER_BYTES);
}

void lpt_socket_device::queue_host_state()
{
	if (!m_connected || !m_dirty)
		return;

	const uint8_t state[STATE_BYTES] = { m_data, m_control, m_drive, 0 };
	queue_frame(FRAME_HOST_PIN_STATE, 0, emulated_ns(), state, sizeof(state));
	m_dirty = false;

	if (!m_exchanging)
		m_exchange_timer->adjust(attotime::zero);
}

void lpt_socket_device::exchange()
{
	if (!m_connected || m_exchanging || machine().side_effects_disabled())
		return;

	m_exchanging = true;

	// Presenting the endpoint's lines can make the controller change its pins at the same (emulated) time.
	// Those changes are queued rather than sent from inside the callbacks.
	unsigned rounds = 0;
	do
	{
		rounds++;
		if (rounds > MAX_ROUNDS)
			throw emu_fatalerror("Parallel socket: non-quiescent zero-time handshake");

		const uint64_t time = emulated_ns();
		m_barrier_id++;
		const uint32_t id = m_barrier_id;

		if (!m_hello_sent)
		{
			// HELLO goes ahead of any HOST_PIN_STATE records queued before the first exchange.
			std::vector<uint8_t> pending;
			pending.swap(m_send_queue);
			queue_frame(FRAME_HELLO, id, time, HELLO_PAYLOAD, sizeof(HELLO_PAYLOAD));
			m_send_queue.insert(m_send_queue.end(), pending.begin(), pending.end());
			m_hello_sent = true;
		}

		queue_host_state();
		queue_frame(FRAME_HOST_SYNC_REQUEST, id, time, nullptr, 0);

		const osd_ticks_t deadline = osd_ticks() + STALL_TIMEOUT_SECONDS * osd_ticks_per_second();
		send(m_send_queue.data(), m_send_queue.size(), deadline);
		m_send_queue.clear();

		unsigned responses = 0;
		bool complete = false;
		while (!complete)
		{
			responses++;
			if (responses > MAX_RESPONSES)
				throw emu_fatalerror("Parallel socket: more than %u responses to one barrier", MAX_RESPONSES);

			complete = receive_response(id, time, deadline);
		}
	} while (!m_send_queue.empty() || m_dirty);

	m_exchanging = false;
	m_completed_time = emulated_ns();

	// Next exchange at the endpoint's next change, or the service interval, whichever is first.
	const uint64_t delay = std::min<uint64_t>(SERVICE_INTERVAL_NS, m_next_deadline - m_completed_time);
	m_exchange_timer->adjust(attotime::from_nsec(delay));
}

bool lpt_socket_device::receive_response(uint32_t id, uint64_t time, osd_ticks_t deadline)
{
	// Returns true on PERIPHERAL_SYNC_REPLY, the last record of the exchange.
	std::array<uint8_t, HEADER_BYTES> header;
	receive(header.data(), header.size(), deadline);

	const uint32_t size = get_u16be(header.data() + HEADER_LENGTH);
	if (header[HEADER_FLAGS] || get_u32be(header.data() + HEADER_ID) != id || get_u64be(header.data() + HEADER_TIME) != time)
		throw emu_fatalerror("Parallel socket: invalid response ordering, flags or timestamp");

	// Checked before reading, so a bad length can't make it read past the payload buffer.
	std::array<uint8_t, MAX_RESPONSE_PAYLOAD> payload;
	if (size > payload.size())
		throw emu_fatalerror("Parallel socket: oversized response");

	receive(payload.data(), size, deadline);

	const uint8_t type = header[HEADER_TYPE];
	if (type == FRAME_PERIPHERAL_PIN_STATE && size == STATE_BYTES && !(payload[STATE_LINES] & STATUS_UNUSED) && !payload[STATE_RESERVED])
	{
		m_got_peripheral_state = true;
		present(payload.data());
		return false;
	}

	if (type == FRAME_PERIPHERAL_SYNC_REPLY && size == SYNC_REPLY_BYTES)
	{
		if (!m_got_peripheral_state)
			throw emu_fatalerror("Parallel socket: initial PERIPHERAL_PIN_STATE required");

		m_next_deadline = get_u64be(payload.data());
		if (m_next_deadline <= time)
			throw emu_fatalerror("Parallel socket: deadline must advance time");

		return true;
	}
	else
	{
		throw emu_fatalerror("Parallel socket: unexpected response type/size");
	}
}

void lpt_socket_device::check_stall(osd_ticks_t deadline)
{
	if (osd_ticks() >= deadline)
		throw emu_fatalerror("Parallel socket: endpoint stalled for %u seconds (session cannot be replayed)", STALL_TIMEOUT_SECONDS);
}

bool lpt_socket_device::would_block(const std::error_condition &err)
{
	return err == std::errc::operation_would_block || err == std::errc::resource_unavailable_try_again || err == std::errc::interrupted;
}

void lpt_socket_device::back_off(unsigned &idle)
{
	idle++;
	if (idle <= SPIN_YIELDS)
		std::this_thread::yield();
	else
		std::this_thread::sleep_for(std::chrono::microseconds(BACKOFF_USEC));
}

void lpt_socket_device::send(const uint8_t *buffer, uint32_t size, osd_ticks_t deadline)
{
	unsigned idle = 0;
	while (size)
	{
		check_stall(deadline);

		uint32_t actual = 0;
		const std::error_condition err = m_stream->stream_write(buffer, size, actual);
		if (would_block(err))
		{
			back_off(idle);
			continue;
		}

		if (err)
			throw emu_fatalerror("Parallel socket: write failed (%s)", err.message());
		if (!actual)
			throw emu_fatalerror("Parallel socket: connection closed or zero-length write");

		idle = 0;
		buffer += actual;
		size -= actual;
	}
}

void lpt_socket_device::receive(uint8_t *buffer, uint32_t size, osd_ticks_t deadline)
{
	unsigned idle = 0;
	while (size)
	{
		check_stall(deadline);

		// What's left of the last read first.
		if (m_receive_pos < m_receive_size)
		{
			const uint32_t chunk = std::min(size, m_receive_size - m_receive_pos);
			std::copy_n(m_receive_buffer.data() + m_receive_pos, chunk, buffer);
			m_receive_pos += chunk;
			buffer += chunk;
			size -= chunk;
			continue;
		}

		uint32_t actual = 0;
		const std::error_condition err = m_stream->stream_read(m_receive_buffer.data(), m_receive_buffer.size(), actual);
		if (would_block(err))
		{
			back_off(idle);
			continue;
		}

		if (err)
			throw emu_fatalerror("Parallel socket: read failed (%s)", err.message());
		if (!actual)
			throw emu_fatalerror("Parallel socket: connection closed");

		idle = 0;
		m_receive_pos = 0;
		m_receive_size = actual;
	}
}

void lpt_socket_device::present(const uint8_t *state)
{
	// Data lines the endpoint doesn't drive read high.
	const uint8_t drive = state[STATE_DRIVE];
	const uint8_t data = (state[STATE_DATA] & drive) | ~drive;
	output_data0(BIT(data, 0));
	output_data1(BIT(data, 1));
	output_data2(BIT(data, 2));
	output_data3(BIT(data, 3));
	output_data4(BIT(data, 4));
	output_data5(BIT(data, 5));
	output_data6(BIT(data, 6));
	output_data7(BIT(data, 7));

	// nAck last: it's the clock of the peripheral-to-host handshakes, so the data and the other
	// status lines have to be there first.
	const uint8_t status = state[STATE_LINES];
	output_perror(BIT(status, STATUS_BIT_PERROR));
	output_select(BIT(status, STATUS_BIT_SELECT));
	output_fault(BIT(status, STATUS_BIT_FAULT));
	output_busy(BIT(status, STATUS_BIT_BUSY));
	output_ack(BIT(status, STATUS_BIT_ACK));
}
