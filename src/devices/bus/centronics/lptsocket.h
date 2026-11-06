// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_BUS_CENTRONICS_LPTSOCKET_H
#define MAME_BUS_CENTRONICS_LPTSOCKET_H

#pragma once

#include "ctronics.h"
#include "lptlink.h"
#include "imagedev/bitbngr.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

// The local handshake engine (TOOT mode 0x02).
// This is meant to improve performance by reducing round trips.
// The endpoint (peripheral) programs this with electrical rules etc... and we handle the handshake locally rather than blasting the socket with data that's predictable.
class lpt_handshake_engine
{

public:

	// Same layout as the HOST_PIN_STATE/PERIPHERAL_PIN_STATE payloads
	using pins = std::array<uint8_t, 4>;
	static constexpr uint32_t PIN_DATA = 0;
	static constexpr uint32_t PIN_LINES = 1; // control (host) or status (peripheral)
	static constexpr uint32_t PIN_DRIVE = 2;
	static constexpr uint32_t PIN_RESERVED = 3;

	static constexpr uint32_t FIFO_COUNT = 8;
	static constexpr uint32_t FIFO_BYTES = 4096;
	static constexpr uint32_t MAX_RULES = 256;
	static constexpr uint32_t RULE_BYTES = 48;
	static constexpr uint64_t NEVER = std::numeric_limits<uint64_t>::max();

	void reset()
	{
		m_rule_count = 0;
		m_state = 0;
		m_timer = NEVER;

		// Only bytes below 'used' are ever read, so emptying the FIFOs is enough
		for (fifo &queue : m_fifo)
		{
			queue.head = 0;
			queue.used = 0;
			queue.capacity = 0;
		}
	}

	// Installs an ENGINE_CONFIG payload. All or nothing: an invalid one changes nothing.
	bool configure(const uint8_t *config, uint32_t size, const pins &host, const pins &output)
	{
		if (size < CONFIG_HEADER_BYTES || config[CONFIG_VERSION] != CONFIG_FORMAT_VERSION || config[CONFIG_STATE] >= STATE_COUNT)
			return false;

		const uint32_t rule_count = get_u16(config + CONFIG_RULE_COUNT);
		const uint32_t fifo_start = CONFIG_HEADER_BYTES + rule_count * RULE_BYTES;
		if (rule_count > MAX_RULES || size < fifo_start + FIFO_COUNT * FIFO_SNAPSHOT_HEADER_BYTES)
			return false;

		const uint8_t *const rules = config + CONFIG_HEADER_BYTES;
		rule scratch;
		for (uint32_t i = 0; i < rule_count; i++)
		{
			if (!decode_rule(rules + i * RULE_BYTES, scratch))
				return false;
		}

		uint32_t pos = fifo_start;
		for (uint32_t i = 0; i < FIFO_COUNT; i++)
		{
			if (size - pos < FIFO_SNAPSHOT_HEADER_BYTES)
				return false;

			const uint32_t used = get_u16(config + pos + FIFO_SNAPSHOT_USED);
			const uint32_t capacity = get_u16(config + pos + FIFO_SNAPSHOT_CAPACITY);
			pos += FIFO_SNAPSHOT_HEADER_BYTES;
			if (capacity > FIFO_BYTES || used > capacity || used > size - pos)
				return false;
			pos += used;
		}
		if (pos != size)
			return false;

		// Everything checked out: install it.
		reset();
		m_rule_count = rule_count;
		m_state = config[CONFIG_STATE];
		m_generation = get_u32(config + CONFIG_GENERATION);
		for (uint32_t i = 0; i < rule_count; i++)
			decode_rule(rules + i * RULE_BYTES, m_rules[i]);

		pos = fifo_start;
		for (fifo &queue : m_fifo)
		{
			queue.used = get_u16(config + pos + FIFO_SNAPSHOT_USED);
			queue.capacity = get_u16(config + pos + FIFO_SNAPSHOT_CAPACITY);
			pos += FIFO_SNAPSHOT_HEADER_BYTES;
			std::copy_n(config + pos, queue.used, queue.data.begin());
			pos += queue.used;
		}

		m_host = host;
		m_output = output;

		return true;
	}

	// Stops local handling until the next configure.
	void suspend()
	{
		m_rule_count = 0;
		m_timer = NEVER;
	}

	uint64_t deadline() const
	{
		return m_timer;
	}

	uint32_t generation() const
	{
		return m_generation;
	}
	
	uint32_t available(uint32_t index) const
	{
		return m_fifo[index].used;
	}

	const pins &output() const
	{
		return m_output;
	}

	// The lines as they are after a sync.
	void observe(const pins &host, const pins &output)
	{
		m_host = host;
		m_output = output;
	}

	// Runs the first rule that matches a host update (timer = false) or the engine's timer (timer = true) at time.
	// emit(pins) gets each peripheral pin state the actions produce.
	// returns false with nothing changed. When no rule can handle it the caller has to sync with the endpoint instead.
	template <typename Emit>
	bool step(const pins &host, uint64_t time, bool timer, Emit emit)
	{
		if (timer && (m_timer == NEVER || time < m_timer))
			return false;
		else if (!timer && time >= m_timer)
			return false; // the timer is due first

		const uint8_t rose = ~m_host[PIN_LINES] & host[PIN_LINES];
		const uint8_t fell = m_host[PIN_LINES] & ~host[PIN_LINES];

		uint8_t occupied = 0;
		for (uint32_t i = 0; i < FIFO_COUNT; i++)
		{
			if (m_fifo[i].used)
				occupied |= 1 << i;
		}

		for (uint32_t i = 0; i < m_rule_count; i++)
		{
			const rule &candidate = m_rules[i];
			if (!rule_matches(candidate, host, rose, fell, occupied, timer))
				continue;

			if (candidate.trigger == TRIGGER_FALLBACK || !actions_fit(candidate, time))
				return false;

			m_host = host;
			m_state = candidate.next_state;
			if (timer)
				m_timer = NEVER; // one shot

			run_actions(candidate, host, time, emit);

			return true;
		}

		return false;
	}

private:

	// ENGINE_CONFIG: header, the rules, then FIFO_COUNT FIFO snapshots
	static constexpr uint32_t CONFIG_VERSION        = 0;
	static constexpr uint32_t CONFIG_STATE          = 1; // initial state
	static constexpr uint32_t CONFIG_RULE_COUNT     = 2; // u16
	static constexpr uint32_t CONFIG_GENERATION     = 4; // u32, echoed by HOST_LOCAL_BATCH
	static constexpr uint32_t CONFIG_HEADER_BYTES   = 8;
	static constexpr  uint8_t CONFIG_FORMAT_VERSION = 1;

	// FIFO snapshot: used (u16), capacity (u16), then the used bytes
	static constexpr uint32_t FIFO_SNAPSHOT_USED         = 0;
	static constexpr uint32_t FIFO_SNAPSHOT_CAPACITY     = 2;
	static constexpr uint32_t FIFO_SNAPSHOT_HEADER_BYTES = 4;

	// Rule on the wire (only decode_rule reads these)
	static constexpr uint32_t RULE_STATE          =  0;
	static constexpr uint32_t RULE_NEXT_STATE     =  1;
	static constexpr uint32_t RULE_CONTROL_MASK   =  2;
	static constexpr uint32_t RULE_CONTROL_VALUE  =  3;
	static constexpr uint32_t RULE_DATA_MASK      =  4;
	static constexpr uint32_t RULE_DATA_VALUE     =  5;
	static constexpr uint32_t RULE_DRIVE_MASK     =  6;
	static constexpr uint32_t RULE_DRIVE_VALUE    =  7;
	static constexpr uint32_t RULE_ROSE           =  8;
	static constexpr uint32_t RULE_FELL           =  9;
	static constexpr uint32_t RULE_FIFOS_NONEMPTY = 10;
	static constexpr uint32_t RULE_FIFOS_EMPTY    = 11;
	static constexpr uint32_t RULE_ACTION_COUNT   = 12;
	static constexpr uint32_t RULE_TRIGGER        = 13;
	static constexpr uint32_t RULE_RESERVED       = 14; // (2 bytes)
	static constexpr uint32_t RULE_ACTIONS        = 16;

	// Action on the wire: opcode, then the arguments the spec calls a, b and c (only decode_action reads these)
	static constexpr uint32_t ACTION_BYTES  = 4;
	static constexpr uint32_t ACTION_OPCODE = 0;
	static constexpr uint32_t ACTION_A      = 1;
	static constexpr uint32_t ACTION_B      = 2;
	static constexpr uint32_t ACTION_C      = 3;

	static constexpr uint32_t STATE_COUNT  = 16;
	static constexpr uint32_t MAX_ACTIONS  = 8;
	static constexpr  uint8_t CONTROL_MASK = 0x0f;
	static constexpr  uint8_t STATUS_MASK  = 0x1f;
	static constexpr  uint8_t DRIVE_ALL    = 0xff;

	enum trigger_type : uint8_t
	{
		TRIGGER_HOST     = 0, // a host update
		TRIGGER_FALLBACK = 1, // sync with the endpoint; has no actions
		TRIGGER_TIMER    = 2  // the engine's timer
	};

	enum opcode : uint8_t
	{
		OP_NONE          = 0, // an unused slot
		OP_SET_DATA      = 1, // put data on the lines with drive
		OP_SET_STATUS    = 2, // set the status_mask lines to status_value
		OP_SUPPLY        = 3, // put fifo's head on the lines with drive, without consuming it
		OP_POP           = 4, // drop fifo's head
		OP_PUSH          = 5, // add the host's data (or data, if literal) to fifo
		OP_TIMER         = 6, // fire the timer delay_ns from now
		OP_SUPPLY_STATUS = 7  // fifo's head on the lines, all driven, and the status_mask lines set to status_value, as one change
	};

	struct action
	{
		opcode op = OP_NONE;
		uint8_t fifo = 0;
		uint8_t data = 0;
		uint8_t drive = 0;
		uint8_t status_mask = 0;
		uint8_t status_value = 0;
		bool literal = false;
		uint32_t delay_ns = 0;
	};

	// Matches when every condition holds
	struct rule
	{
		uint8_t state = 0;
		uint8_t next_state = 0;
		uint8_t control_mask = 0;
		uint8_t control_value = 0;
		uint8_t data_mask = 0;
		uint8_t data_value = 0;
		uint8_t drive_mask = 0;
		uint8_t drive_value = 0;
		uint8_t rose = 0;            // control lines that must have risen
		uint8_t fell = 0;            // control lines that must have fallen
		uint8_t fifos_nonempty = 0;  // a bit per FIFO
		uint8_t fifos_empty = 0;
		trigger_type trigger = TRIGGER_HOST;
		uint8_t action_count = 0;
		std::array<action, MAX_ACTIONS> actions{};
	};

	struct fifo
	{
		std::array<uint8_t, FIFO_BYTES> data{};
		uint32_t head = 0;
		uint32_t used = 0;
		uint32_t capacity = 0;
	};

	static uint32_t get_u16(const uint8_t *p)
	{
		return (uint32_t(p[0]) << 8) | p[1];
	}

	static uint32_t get_u24(const uint8_t *p)
	{
		return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
	}

	static uint32_t get_u32(const uint8_t *p)
	{
		return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
	}

	// Decodes a rule off the wire. false if it's invalid
	static bool decode_rule(const uint8_t *bytes, rule &out)
	{
		out = rule();
		out.state = bytes[RULE_STATE];
		out.next_state = bytes[RULE_NEXT_STATE];
		out.control_mask = bytes[RULE_CONTROL_MASK];
		out.control_value = bytes[RULE_CONTROL_VALUE];
		out.data_mask = bytes[RULE_DATA_MASK];
		out.data_value = bytes[RULE_DATA_VALUE];
		out.drive_mask = bytes[RULE_DRIVE_MASK];
		out.drive_value = bytes[RULE_DRIVE_VALUE];
		out.rose = bytes[RULE_ROSE];
		out.fell = bytes[RULE_FELL];
		out.fifos_nonempty = bytes[RULE_FIFOS_NONEMPTY];
		out.fifos_empty = bytes[RULE_FIFOS_EMPTY];
		out.action_count = bytes[RULE_ACTION_COUNT];

		if (out.state >= STATE_COUNT || out.next_state >= STATE_COUNT)
			return false;
		else if ((out.control_mask & ~CONTROL_MASK) || (out.control_value & ~out.control_mask))
			return false;
		else if ((out.data_value & ~out.data_mask) || (out.drive_value & ~out.drive_mask))
			return false;
		else if (((out.rose | out.fell) & ~CONTROL_MASK) || (out.rose & out.fell))
			return false;
		else if (out.fifos_nonempty & out.fifos_empty)
			return false;
		else if (out.action_count > MAX_ACTIONS || bytes[RULE_TRIGGER] > TRIGGER_TIMER || bytes[RULE_RESERVED] || bytes[RULE_RESERVED + 1])
			return false;

		out.trigger = trigger_type(bytes[RULE_TRIGGER]);
		if (out.trigger == TRIGGER_FALLBACK && out.action_count)
			return false;

		for (uint32_t i = 0; i < MAX_ACTIONS; i++)
		{
			const uint8_t *const action_bytes = bytes + RULE_ACTIONS + i * ACTION_BYTES;
			if (i < out.action_count)
			{
				if (!decode_action(action_bytes, out.actions[i]))
					return false;
			}
			else if (action_bytes[ACTION_OPCODE] | action_bytes[ACTION_A] | action_bytes[ACTION_B] | action_bytes[ACTION_C])
			{
				return false; // unused slots are zero
			}
		}

		return true;
	}

	// Decodes an action off the wire. Each opcode says what its a, b and c are, and which have to be zero.
	static bool decode_action(const uint8_t *bytes, action &out)
	{
		const uint8_t a = bytes[ACTION_A];
		const uint8_t b = bytes[ACTION_B];
		const uint8_t c = bytes[ACTION_C];

		out = action();
		switch (bytes[ACTION_OPCODE])
		{
			case OP_SET_DATA:
				out.op = OP_SET_DATA;
				out.data = a;
				out.drive = b;
				return !c;

			case OP_SET_STATUS:
				out.op = OP_SET_STATUS;
				out.status_mask = a;
				out.status_value = b;
				return !(a & ~STATUS_MASK) && !(b & ~a) && !c;

			case OP_SUPPLY:
				out.op = OP_SUPPLY;
				out.fifo = a;
				out.drive = b;
				return a < FIFO_COUNT && !c;

			case OP_POP:
				out.op = OP_POP;
				out.fifo = a;
				return a < FIFO_COUNT && !b && !c;

			case OP_PUSH:
				out.op = OP_PUSH;
				out.fifo = a;
				out.literal = b;
				out.data = c;
				return a < FIFO_COUNT && b <= 1 && (b || !c);

			case OP_TIMER:
				out.op = OP_TIMER;
				out.delay_ns = get_u24(bytes + ACTION_A);
				return out.delay_ns != 0;

			case OP_SUPPLY_STATUS:
				out.op = OP_SUPPLY_STATUS;
				out.fifo = a;
				out.status_mask = b;
				out.status_value = c;
				return a < FIFO_COUNT && !(b & ~STATUS_MASK) && !(c & ~b);

			default:
				return false;
		}
	}

	bool rule_matches(const rule &candidate, const pins &host, uint8_t rose, uint8_t fell, uint8_t occupied, bool timer) const
	{
		return candidate.state == m_state
				&& (host[PIN_LINES] & candidate.control_mask) == candidate.control_value
				&& (host[PIN_DATA] & candidate.data_mask) == candidate.data_value
				&& (host[PIN_DRIVE] & candidate.drive_mask) == candidate.drive_value
				&& (rose & candidate.rose) == candidate.rose
				&& (fell & candidate.fell) == candidate.fell
				&& (occupied & candidate.fifos_nonempty) == candidate.fifos_nonempty
				&& !(occupied & candidate.fifos_empty)
				&& timer == (candidate.trigger == TRIGGER_TIMER);
	}

	// Whether every action can run. FIFO bytes to read, FIFO room to write, a timer that fits.
	bool actions_fit(const rule &candidate, uint64_t time) const
	{
		std::array<uint32_t, FIFO_COUNT> used;
		for (uint32_t i = 0; i < FIFO_COUNT; i++)
			used[i] = m_fifo[i].used;

		for (uint32_t i = 0; i < candidate.action_count; i++)
		{
			const action &act = candidate.actions[i];
			switch (act.op)
			{
				case OP_SUPPLY:
				case OP_SUPPLY_STATUS:
					if (!used[act.fifo])
						return false;
					break;

				case OP_POP:
					if (!used[act.fifo])
						return false;
					used[act.fifo]--;
					break;

				case OP_PUSH:
					if (used[act.fifo]++ >= m_fifo[act.fifo].capacity)
						return false;
					break;

				case OP_TIMER:
					if (time >= NEVER - act.delay_ns)
						return false;
					break;

				default:
					break;
			}
		}

		return true;
	}

	// Each action that changes the peripheral's lines emits them so no transition is lost.
	template <typename Emit>
	void run_actions(const rule &candidate, const pins &host, uint64_t time, Emit &emit)
	{
		for (uint32_t i = 0; i < candidate.action_count; i++)
		{
			const action &act = candidate.actions[i];
			const pins previous = m_output;

			switch (act.op)
			{
				case OP_SET_DATA:
					m_output[PIN_DATA] = act.data;
					m_output[PIN_DRIVE] = act.drive;
					break;

				case OP_SET_STATUS:
					m_output[PIN_LINES] = (m_output[PIN_LINES] & ~act.status_mask) | act.status_value;
					break;

				case OP_SUPPLY:
					m_output[PIN_DATA] = fifo_head(act.fifo);
					m_output[PIN_DRIVE] = act.drive;
					break;

				case OP_POP:
					m_fifo[act.fifo].head = (m_fifo[act.fifo].head + 1) % FIFO_BYTES;
					m_fifo[act.fifo].used--;
					break;

				case OP_PUSH:
				{
					fifo &queue = m_fifo[act.fifo];
					queue.data[(queue.head + queue.used) % FIFO_BYTES] = act.literal ? act.data : host[PIN_DATA];
					queue.used++;
					break;
				}

				case OP_TIMER:
					m_timer = time + act.delay_ns;
					break;

				case OP_SUPPLY_STATUS:
					m_output[PIN_DATA] = fifo_head(act.fifo);
					m_output[PIN_DRIVE] = DRIVE_ALL;
					m_output[PIN_LINES] = (m_output[PIN_LINES] & ~act.status_mask) | act.status_value;
					break;

				default:
					break;
			}

			if (m_output != previous)
				emit(m_output);
		}
	}

	uint8_t fifo_head(uint32_t index) const
	{
		return m_fifo[index].data[m_fifo[index].head];
	}

	std::array<fifo, FIFO_COUNT> m_fifo{};
	std::array<rule, MAX_RULES> m_rules{};
	pins m_host {}; // host lines as of the last handled update or sync
	pins m_output {}; // peripheral lines the engine drives
	uint32_t m_rule_count = 0;
	uint8_t m_state = 0;
	uint32_t m_generation = 0;
	uint64_t m_timer = NEVER; // when the one-shot timer fires, emulated ns

};

class lpt_socket_device :
	public device_t,
	public device_centronics_peripheral_interface,
	public device_ecp_lpt_link_interface
{

public:

	lpt_socket_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// device_ecp_lpt_link_interface
	virtual void host_data_dir(bool host_drives) override;
	virtual void host_sample_lines() override;
	virtual void host_output_changed() override { queue_host_state(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	// device_centronics_peripheral_interface
	virtual void input_strobe(int state) override { change_control(CONTROL_BIT_STROBE, state); }
	virtual void input_autofd(int state) override { change_control(CONTROL_BIT_AUTOFD, state); }
	virtual void input_init(int state) override { change_control(CONTROL_BIT_INIT, state); }
	virtual void input_select_in(int state) override { change_control(CONTROL_BIT_SELECT_IN, state); }
	virtual void input_data0(int state) override { change_data_bit(0, state); }
	virtual void input_data1(int state) override { change_data_bit(1, state); }
	virtual void input_data2(int state) override { change_data_bit(2, state); }
	virtual void input_data3(int state) override { change_data_bit(3, state); }
	virtual void input_data4(int state) override { change_data_bit(4, state); }
	virtual void input_data5(int state) override { change_data_bit(5, state); }
	virtual void input_data6(int state) override { change_data_bit(6, state); }
	virtual void input_data7(int state) override { change_data_bit(7, state); }

private:

	using pins = lpt_handshake_engine::pins;

	// Frame header, big endian: payload length (u16), type, flags, barrier ID (u32), emulated ns (u64)
	static constexpr uint32_t HEADER_BYTES  = 16;
	static constexpr uint32_t HEADER_LENGTH = 0;
	static constexpr uint32_t HEADER_TYPE   = 2;
	static constexpr uint32_t HEADER_FLAGS  = 3;
	static constexpr uint32_t HEADER_ID     = 4;
	static constexpr uint32_t HEADER_TIME   = 8;

	enum frame_type : uint8_t
	{
		FRAME_HELLO                 = 0x01, // host: protocol version and capabilities
		FRAME_HOST_PIN_STATE        = 0x02, // host: host pin levels, ID 0
		FRAME_HOST_SYNC_REQUEST     = 0x03, // host: process everything up to the header's time
		FRAME_MODE_ACK              = 0x04, // host: requested mode accepted
		FRAME_MODE_NAK              = 0x05, // host: requested mode rejected
		FRAME_HOST_LOCAL_BATCH      = 0x06, // host: what the local engine did, for the endpoint to replay; ID 0
		FRAME_SET_MODE              = 0x81, // peripheral: requested mode, one byte
		FRAME_PERIPHERAL_PIN_STATE  = 0x82, // peripheral: peripheral pin levels
		FRAME_PERIPHERAL_SYNC_REPLY = 0x83, // peripheral: sync done; u64 time of its next change of its own
		FRAME_ENGINE_CONFIG         = 0x84  // peripheral: rules and FIFOs for the local engine
	};

	// HELLO: "TOOT", u16 protocol version 1, u32 capabilities (bit 0: local handshake engine)
	static constexpr uint8_t HELLO_PAYLOAD[] = { 'T', 'O', 'O', 'T', 0, 1, 0, 0, 0, 1 };

	enum mode : uint8_t
	{
		MODE_UNSELECTED      = 0x00, // no SET_MODE accepted yet
		MODE_PIN_PASSTHROUGH = 0x01,
		MODE_LOCAL_HANDSHAKE = 0x02
	};

	static constexpr uint32_t MODE_BYTES = 1;

	// HOST_PIN_STATE and PERIPHERAL_PIN_STATE payloads
	static constexpr uint32_t STATE_BYTES    = std::tuple_size<pins>::value;
	static constexpr uint32_t STATE_DATA     = lpt_handshake_engine::PIN_DATA;
	static constexpr uint32_t STATE_LINES    = lpt_handshake_engine::PIN_LINES;
	static constexpr uint32_t STATE_DRIVE    = lpt_handshake_engine::PIN_DRIVE;
	static constexpr uint32_t STATE_RESERVED = lpt_handshake_engine::PIN_RESERVED;

	// HOST_PIN_STATE control bits, pin levels
	static constexpr uint32_t CONTROL_BIT_STROBE    = 0;
	static constexpr uint32_t CONTROL_BIT_AUTOFD    = 1;
	static constexpr uint32_t CONTROL_BIT_INIT      = 2;
	static constexpr uint32_t CONTROL_BIT_SELECT_IN = 3;

	// PERIPHERAL_PIN_STATE status bits, pin levels
	static constexpr uint32_t STATUS_BIT_ACK    = 0;
	static constexpr uint32_t STATUS_BIT_BUSY   = 1;
	static constexpr uint32_t STATUS_BIT_PERROR = 2;
	static constexpr uint32_t STATUS_BIT_SELECT = 3;
	static constexpr uint32_t STATUS_BIT_FAULT  = 4;
	static constexpr uint8_t  STATUS_MASK       = 0x1f;
	static constexpr uint8_t  STATUS_RELEASED   = STATUS_MASK; // nothing attached: all pulled up

	// Drive masks: a set bit is a data line that side drives; released lines pull high.
	static constexpr uint8_t DRIVE_ALL  = 0xff;
	static constexpr uint8_t DRIVE_NONE = 0x00;

	static constexpr uint32_t SYNC_REPLY_BYTES = 8;
	static constexpr uint64_t NO_DEADLINE      = UINT64_MAX;

	// HOST_LOCAL_BATCH: u32 generation of the engine's configuration, then up to 2047 event records
	static constexpr uint32_t BATCH_GENERATION_BYTES = 4;
	static constexpr uint32_t EVENT_BYTES            = 24;
	static constexpr uint32_t MAX_EVENT_BYTES        = 2047 * EVENT_BYTES;

	// Event record
	static constexpr uint32_t EVENT_TIME         = 0;  // u64 emulated ns
	static constexpr uint32_t EVENT_HOST         = 8;  // host pin state
	static constexpr uint32_t EVENT_OUTPUT_COUNT = 12; // u16 peripheral pin states emitted
	static constexpr uint32_t EVENT_KIND         = 14;
	static constexpr uint32_t EVENT_RESERVED     = 15;
	static constexpr uint32_t EVENT_CHECKSUM     = 16; // u32 FNV-1a of the emitted pin states
	static constexpr uint32_t EVENT_PERIPHERAL   = 20; // peripheral pin state afterwards
	static constexpr uint8_t  EVENT_KIND_HOST    = 0;
	static constexpr uint8_t  EVENT_KIND_TIMER   = 1;

	static constexpr uint32_t FNV_OFFSET_BASIS = 2166136261U;
	static constexpr uint32_t FNV_PRIME        = 16777619U;

	// Limits
	static constexpr size_t   MAX_QUEUED_BYTES      = 0x10000; // outgoing frames
	static constexpr size_t   RECEIVE_BUFFER_BYTES  = 0x10000;
	static constexpr uint32_t MAX_RESPONSE_PAYLOAD  = 49152;   // ENGINE_CONFIG is the big one
	static constexpr uint32_t MAX_RESPONSES         = 4096;    // records per sync, and SET_MODEs per HELLO
	static constexpr uint32_t MAX_ROUNDS            = 4096;    // syncs (or local steps) at one emulated time
	static constexpr uint32_t STALL_TIMEOUT_SECONDS = 5;       // wall time for one sync

	// The longest gap between syncs, emulated ns.
	// Local handshakes can batch for 1 ms
	// pin passthrough keeps 250 us
	static constexpr uint64_t SERVICE_INTERVAL_NS     = 1'000'000;
	static constexpr uint64_t RAW_SERVICE_INTERVAL_NS = 250'000;

	// For a stream with no readiness wait: a local endpoint usually answers within a few yields after that, back off with sleeps (which can wake much later than asked).
	static constexpr uint32_t SPIN_YIELDS  = 32;
	static constexpr uint32_t BACKOFF_USEC = 20;

	required_device<bitbanger_device> m_stream;

	void stream_mounted(int state);
	void change_data_bit(uint32_t bit, int state);
	void change_control(uint32_t bit, int state);
	pins host_pins() const { return { m_data, m_control, m_drive, 0 }; }
	bool before_sync_due(uint64_t time) const;

	// local handshake engine
	void service();
	bool run_local(bool timer = false);
	void flush_local();

	// syncs
	void queue_host_state();
	uint8_t *queue_frame(uint8_t type, uint32_t id, uint64_t time, uint32_t size);
	static void write_header(uint8_t *frame, uint8_t type, uint32_t id, uint64_t time, uint32_t size);
	void schedule_exchange();
	void exchange();
	bool receive_response(uint32_t id, uint64_t time, osd_ticks_t deadline, bool negotiating = false);
	void reply_mode(uint8_t requested, uint32_t id, uint64_t time, osd_ticks_t deadline);
	void present(const uint8_t *state);

	// socket I/O
	void send(const uint8_t *buffer, uint32_t size, osd_ticks_t deadline);
	void receive(uint8_t *buffer, uint32_t size, osd_ticks_t deadline);
	static void check_stall(osd_ticks_t deadline);
	static bool would_block(const std::error_condition &err);
	void back_off(uint32_t &idle, bool writing);

	static uint64_t to_ns(const attotime &time) { return time.as_ticks(1'000'000'000); }
	uint64_t emulated_ns() const;

	TIMER_CALLBACK_MEMBER(exchange_tick);

	emu_timer *m_exchange_timer;

	// what the host drives
	uint8_t m_data;
	uint8_t m_control;
	uint8_t m_drive;
	bool m_dirty; // changed since the last HOST_PIN_STATE (or local step)

	// what the host is shown
	pins m_peripheral;
	bool m_have_presented; // m_peripheral is on the lines

	// session
	bool m_connected;
	bool m_hello_sent;
	uint8_t m_mode;
	bool m_got_peripheral_state; // the endpoint has sent its first PERIPHERAL_PIN_STATE
	bool m_exchanging;
	uint32_t m_barrier_id;
	uint64_t m_next_deadline; // the endpoint's next change of its own, emulated ns
	uint64_t m_service_time; // next periodic sync, emulated ns; local traffic never postpones it
	uint64_t m_completed_time;  // last time committed to the endpoint, locally or by a sync, emulated ns

	// local handshake engine
	lpt_handshake_engine m_engine;
	bool m_local_running; // inside m_engine.step
	std::vector<uint8_t> m_local_events;

	std::vector<uint8_t> m_send_queue;
	std::array<uint8_t, RECEIVE_BUFFER_BYTES> m_receive_buffer;
	uint32_t m_receive_pos;
	uint32_t m_receive_size;
	std::array<uint8_t, MAX_RESPONSE_PAYLOAD> m_response_payload;

};

DECLARE_DEVICE_TYPE(LPT_SOCKET, lpt_socket_device)

#endif // MAME_BUS_CENTRONICS_LPTSOCKET_H
