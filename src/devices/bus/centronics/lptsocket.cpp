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

DEFINE_DEVICE_TYPE(LPT_SOCKET, lpt_socket_device, "lpt_socket", "Parallel port socket")

lpt_socket_device::lpt_socket_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, LPT_SOCKET, tag, owner, clock),
	device_centronics_peripheral_interface(mconfig, *this),
	m_stream(*this, "stream"),
	m_exchange_timer(nullptr),
	m_data(0x00),
	m_control(0x00),
	m_drive(DRIVE_ALL),
	m_dirty(true),
	m_peripheral{},
	m_have_presented(false),
	m_connected(false),
	m_hello_sent(false),
	m_mode(MODE_UNSELECTED),
	m_got_peripheral_state(false),
	m_exchanging(false),
	m_barrier_id(0),
	m_next_deadline(NO_DEADLINE),
	m_service_time(0),
	m_completed_time(NO_DEADLINE),
	m_local_running(false),
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
	const uint64_t time = to_ns(machine().time());
	if (m_completed_time != NO_DEADLINE && time < m_completed_time)
		return m_completed_time;
	else
		return time;
}

bool lpt_socket_device::before_sync_due(uint64_t time) const
{
	return time < std::min(m_next_deadline, m_service_time);
}

void lpt_socket_device::stream_mounted(int state)
{
	// Every mount is a new session.
	m_connected = state != 0;
	m_hello_sent = false;
	m_mode = MODE_UNSELECTED;
	m_got_peripheral_state = false;
	m_dirty = true;
	m_send_queue.clear();
	m_local_events.clear();
	m_engine.reset();
	m_local_running = false;
	m_have_presented = false;
	m_receive_pos = 0;
	m_receive_size = 0;
	m_next_deadline = NO_DEADLINE;
	m_service_time = 0;
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

void lpt_socket_device::change_data_bit(uint32_t bit, int state)
{
	const uint8_t data = (m_data & ~(1 << bit)) | (state ? (1 << bit) : 0);
	m_dirty |= data != m_data;
	m_data = data;

	// A byte arrives a bit at a time but goes out whole.
	if (m_connected && !m_exchanging && m_mode != MODE_LOCAL_HANDSHAKE)
		m_exchange_timer->adjust(attotime::zero);
}

void lpt_socket_device::change_control(uint32_t bit, int state)
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
	if (machine().side_effects_disabled() || m_local_running || m_exchanging)
		return;

	const uint64_t time = emulated_ns();
	if (m_mode == MODE_LOCAL_HANDSHAKE && m_engine.deadline() <= time)
		service();

	if (m_got_peripheral_state && m_send_queue.empty() && !m_dirty && before_sync_due(time))
		return;

	exchange();
}

TIMER_CALLBACK_MEMBER(lpt_socket_device::exchange_tick)
{
	service();
}

void lpt_socket_device::service()
{
	// The engine's own timer runs locally when it's due before the next sync else sync.
	const uint64_t time = emulated_ns();
	if (m_mode == MODE_LOCAL_HANDSHAKE && m_engine.deadline() <= time && before_sync_due(time) && run_local(true))
	{
		queue_host_state();
		schedule_exchange();
	}
	else
	{
		exchange();
	}
}

bool lpt_socket_device::run_local(bool timer)
{
	const uint64_t time = emulated_ns();
	if (!before_sync_due(time) || m_local_events.size() + EVENT_BYTES > MAX_EVENT_BYTES || (!timer && m_engine.deadline() <= time))
		return false;

	const pins host = host_pins();
	uint32_t checksum = FNV_OFFSET_BASIS;
	uint16_t output_count = 0;

	// Host changes made in reaction to the emitted lines mark m_dirty for another step.
	m_local_running = true;
	m_dirty = false;
	const bool handled = m_engine.step(host, time, timer, [this, &checksum, &output_count] (const pins &output)
	{
		for (uint8_t byte : output)
			checksum = (checksum ^ byte) * FNV_PRIME;
		output_count++;
		present(output.data());
	});
	m_local_running = false;

	if (!handled)
	{
		m_dirty = true;
		return false;
	}

	const size_t pos = m_local_events.size();
	m_local_events.resize(pos + EVENT_BYTES);

	uint8_t *const event = m_local_events.data() + pos;
	put_u64be(event + EVENT_TIME, time);
	std::copy(host.begin(), host.end(), event + EVENT_HOST);
	put_u16be(event + EVENT_OUTPUT_COUNT, output_count);
	event[EVENT_KIND] = timer ? EVENT_KIND_TIMER : EVENT_KIND_HOST;
	event[EVENT_RESERVED] = 0;
	put_u32be(event + EVENT_CHECKSUM, checksum);
	std::copy(m_peripheral.begin(), m_peripheral.end(), event + EVENT_PERIPHERAL);

	m_completed_time = time;
	return true;
}

void lpt_socket_device::flush_local()
{
	if (m_local_events.empty())
		return;

	uint8_t *const payload = queue_frame(FRAME_HOST_LOCAL_BATCH, 0, emulated_ns(), BATCH_GENERATION_BYTES + m_local_events.size());
	put_u32be(payload, m_engine.generation());

	std::copy(m_local_events.begin(), m_local_events.end(), payload + BATCH_GENERATION_BYTES);

	m_local_events.clear();
}

void lpt_socket_device::write_header(uint8_t *frame, uint8_t type, uint32_t id, uint64_t time, uint32_t size)
{
	put_u16be(frame + HEADER_LENGTH, size);
	frame[HEADER_TYPE] = type;
	frame[HEADER_FLAGS] = 0;
	put_u32be(frame + HEADER_ID, id);
	put_u64be(frame + HEADER_TIME, time);
}

uint8_t *lpt_socket_device::queue_frame(uint8_t type, uint32_t id, uint64_t time, uint32_t size)
{
	if (m_send_queue.size() + HEADER_BYTES + size > MAX_QUEUED_BYTES)
		throw emu_fatalerror("Parallel socket: outgoing event queue overflow");

	const size_t pos = m_send_queue.size();
	m_send_queue.resize(pos + HEADER_BYTES + size);

	uint8_t *const frame = m_send_queue.data() + pos;
	write_header(frame, type, id, time, size);

	return frame + HEADER_BYTES;
}

void lpt_socket_device::queue_host_state()
{
	if (!m_connected || !m_dirty || m_local_running)
		return;

	// Handled locally if the engine can take it, with nothing else waiting to go out first.
	if (m_mode == MODE_LOCAL_HANDSHAKE && !m_exchanging && m_send_queue.empty())
	{
		const uint64_t previous_deadline = m_engine.deadline();
		uint32_t rounds = 0;
		while (m_dirty && run_local())
		{
			rounds++;
			if (rounds > MAX_ROUNDS)
				throw emu_fatalerror("Parallel socket: non-quiescent local handshake");
		}

		if (!m_dirty)
		{
			// The sync timer is already set; only a new engine timer moves it.
			if (m_engine.deadline() != previous_deadline)
				schedule_exchange();
			return;
		}
	}

	flush_local();

	const pins host = host_pins();
	std::copy(host.begin(), host.end(), queue_frame(FRAME_HOST_PIN_STATE, 0, emulated_ns(), STATE_BYTES));
	m_dirty = false;

	if (!m_exchanging)
		m_exchange_timer->adjust(attotime::zero);
}

void lpt_socket_device::schedule_exchange()
{
	const uint64_t time = emulated_ns();
	const uint64_t next = std::min({ m_next_deadline, m_engine.deadline(), m_service_time });
	const uint64_t delay = next > time ? std::min<uint64_t>(SERVICE_INTERVAL_NS, next - time) : 0;

	if (!m_exchange_timer->enabled() || to_ns(m_exchange_timer->expire()) != time + delay)
		m_exchange_timer->adjust(attotime::from_nsec(delay));
}

void lpt_socket_device::exchange()
{
	if (!m_connected || m_exchanging || machine().side_effects_disabled())
		return;

	flush_local();
	m_exchanging = true;

	// Presenting the endpoint's lines can make the controller change its pins at the same (emulated) time.
	// Those changes are queued rather than sent from inside the callbacks, and go out in another round here. Rounds repeat until the cable settles.
	uint32_t rounds = 0;
	do
	{
		rounds++;
		if (rounds > MAX_ROUNDS)
			throw emu_fatalerror("Parallel socket: non-quiescent zero-time handshake");

		const uint64_t time = emulated_ns();
		m_barrier_id++;
		const uint32_t id = m_barrier_id;
		const osd_ticks_t deadline = osd_ticks() + STALL_TIMEOUT_SECONDS * osd_ticks_per_second();

		if (!m_hello_sent)
		{
			// HELLO goes out on its own, ahead of anything queued before the first sync, and the endpoint picks a mode before any pin traffic.
			std::vector<uint8_t> pending;
			pending.swap(m_send_queue);
			std::copy_n(HELLO_PAYLOAD, sizeof(HELLO_PAYLOAD), queue_frame(FRAME_HELLO, id, time, sizeof(HELLO_PAYLOAD)));
			send(m_send_queue.data(), m_send_queue.size(), deadline);
			m_send_queue = std::move(pending);
			m_hello_sent = true;

			uint32_t requests = 0;
			while (m_mode == MODE_UNSELECTED)
			{
				requests++;
				if (requests > MAX_RESPONSES)
					throw emu_fatalerror("Parallel socket: too many mode requests");

				receive_response(id, time, deadline, true);
			}
		}

		m_engine.suspend();
		queue_host_state();
		queue_frame(FRAME_HOST_SYNC_REQUEST, id, time, 0);

		send(m_send_queue.data(), m_send_queue.size(), deadline);
		m_send_queue.clear();

		uint32_t responses = 0;
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

	const uint64_t interval = (m_mode == MODE_LOCAL_HANDSHAKE) ? SERVICE_INTERVAL_NS : RAW_SERVICE_INTERVAL_NS;
	m_service_time = (m_completed_time < NO_DEADLINE - interval) ? (m_completed_time + interval) : NO_DEADLINE;

	m_engine.observe(host_pins(), m_peripheral);
	schedule_exchange();
}

void lpt_socket_device::reply_mode(uint8_t requested, uint32_t id, uint64_t time, osd_ticks_t deadline)
{
	// Sent straight away: the endpoint waits for it before anything else.
	const bool accepted = requested == MODE_PIN_PASSTHROUGH || requested == MODE_LOCAL_HANDSHAKE;

	std::array<uint8_t, HEADER_BYTES + MODE_BYTES> reply;
	write_header(reply.data(), accepted ? FRAME_MODE_ACK : FRAME_MODE_NAK, id, time, MODE_BYTES);
	reply[HEADER_BYTES] = requested;
	send(reply.data(), reply.size(), deadline);

	if (accepted)
	{
		m_mode = requested;
		m_engine.reset();
	}
}

bool lpt_socket_device::receive_response(uint32_t id, uint64_t time, osd_ticks_t deadline, bool negotiating)
{
	// Returns true on PERIPHERAL_SYNC_REPLY, the last record of a sync. While negotiating, only SET_MODE is allowed.
	std::array<uint8_t, HEADER_BYTES> header;
	receive(header.data(), header.size(), deadline);

	const uint32_t size = get_u16be(header.data() + HEADER_LENGTH);
	if (header[HEADER_FLAGS] || get_u32be(header.data() + HEADER_ID) != id || get_u64be(header.data() + HEADER_TIME) != time)
		throw emu_fatalerror("Parallel socket: invalid response ordering, flags or timestamp");

	// Checked before reading, so a bad length can't make it read past the payload buffer.
	if (size > m_response_payload.size())
		throw emu_fatalerror("Parallel socket: oversized response");

	uint8_t *const payload = m_response_payload.data();
	receive(payload, size, deadline);

	const uint8_t type = header[HEADER_TYPE];
	if (type == FRAME_SET_MODE && size == MODE_BYTES)
	{
		reply_mode(payload[0], id, time, deadline);

		return false;
	}

	if (negotiating)
	{
		throw emu_fatalerror("Parallel socket: SET_MODE required before pin activity");
	}
	else if (type == FRAME_ENGINE_CONFIG && m_mode == MODE_LOCAL_HANDSHAKE && m_got_peripheral_state)
	{
		if (!m_engine.configure(payload, size, host_pins(), m_peripheral))
			throw emu_fatalerror("Parallel socket: invalid local engine configuration");
		else
			return false;
	}
	else if (type == FRAME_PERIPHERAL_PIN_STATE && size == STATE_BYTES && !(payload[STATE_LINES] & ~STATUS_MASK) && !payload[STATE_RESERVED])
	{
		m_got_peripheral_state = true;
		present(payload);

		return false;
	}
	else
	{
		if (type == FRAME_PERIPHERAL_SYNC_REPLY && size == SYNC_REPLY_BYTES)
		{
			if (!m_got_peripheral_state)
				throw emu_fatalerror("Parallel socket: initial PERIPHERAL_PIN_STATE required");

			m_next_deadline = get_u64be(payload);
			if (m_next_deadline <= time)
				throw emu_fatalerror("Parallel socket: deadline must advance time");

			return true;
		}
		else
		{
			throw emu_fatalerror("Parallel socket: unexpected response type/size");
		}
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

void lpt_socket_device::back_off(uint32_t &idle, bool writing)
{
	const std::error_condition err = m_stream->stream_wait(writing, std::chrono::milliseconds(1));
	if (!err || would_block(err))
		return;
	else if (err != std::errc::not_supported)
		throw emu_fatalerror("Parallel socket: readiness wait failed (%s)", err.message());

	// Socket streams wait for readiness. This is for a stream that can't.
	idle++;
	if (idle <= SPIN_YIELDS)
		std::this_thread::yield();
	else
		std::this_thread::sleep_for(std::chrono::microseconds(BACKOFF_USEC));
}

void lpt_socket_device::send(const uint8_t *buffer, uint32_t size, osd_ticks_t deadline)
{
	uint32_t idle = 0;
	while (size)
	{
		check_stall(deadline);

		uint32_t actual = 0;
		const std::error_condition err = m_stream->stream_write(buffer, size, actual);
		if (would_block(err))
		{
			back_off(idle, true);
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
	uint32_t idle = 0;
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
			back_off(idle, false);
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
	// Only lines that change are signalled, except the first time, when they all are.
	const uint8_t previous_data = (m_peripheral[STATE_DATA] & m_peripheral[STATE_DRIVE]) | ~m_peripheral[STATE_DRIVE];
	const uint8_t previous_status = m_peripheral[STATE_LINES];
	std::copy_n(state, STATE_BYTES, m_peripheral.begin());

	// Data lines the endpoint doesn't drive read high.
	const uint8_t drive = state[STATE_DRIVE];
	const uint8_t data = (state[STATE_DATA] & drive) | ~drive;
	const uint8_t status = state[STATE_LINES];
	const uint8_t changed_data = m_have_presented ? (data ^ previous_data) : 0xff;
	const uint8_t changed_status = m_have_presented ? (status ^ previous_status) : STATUS_MASK;
	m_have_presented = true;

	if (BIT(changed_data, 0))
		output_data0(BIT(data, 0));
	if (BIT(changed_data, 1))
		output_data1(BIT(data, 1));
	if (BIT(changed_data, 2))
		output_data2(BIT(data, 2));
	if (BIT(changed_data, 3))
		output_data3(BIT(data, 3));
	if (BIT(changed_data, 4))
		output_data4(BIT(data, 4));
	if (BIT(changed_data, 5))
		output_data5(BIT(data, 5));
	if (BIT(changed_data, 6))
		output_data6(BIT(data, 6));
	if (BIT(changed_data, 7))
		output_data7(BIT(data, 7));

	// nAck last: it's the clock of the peripheral-to-host handshakes, so the data and the other
	// status lines have to be there first.
	if (BIT(changed_status, STATUS_BIT_PERROR))
		output_perror(BIT(status, STATUS_BIT_PERROR));
	if (BIT(changed_status, STATUS_BIT_SELECT))
		output_select(BIT(status, STATUS_BIT_SELECT));
	if (BIT(changed_status, STATUS_BIT_FAULT))
		output_fault(BIT(status, STATUS_BIT_FAULT));
	if (BIT(changed_status, STATUS_BIT_BUSY))
		output_busy(BIT(status, STATUS_BIT_BUSY));
	if (BIT(changed_status, STATUS_BIT_ACK))
		output_ack(BIT(status, STATUS_BIT_ACK));
}