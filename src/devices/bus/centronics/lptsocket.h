// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_BUS_CENTRONICS_LPTSOCKET_H
#define MAME_BUS_CENTRONICS_LPTSOCKET_H

#pragma once

#include "ctronics.h"
#include "lptlink.h"
#include "imagedev/bitbngr.h"

#include <array>
#include <vector>

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

	// Frame header, big endian: payload length (u16), type, flags, barrier ID (u32), emulated ns (u64)
	static constexpr uint32_t HEADER_BYTES  = 16;
	static constexpr uint32_t HEADER_LENGTH = 0;
	static constexpr uint32_t HEADER_TYPE   = 2;
	static constexpr uint32_t HEADER_FLAGS  = 3;
	static constexpr uint32_t HEADER_ID     = 4;
	static constexpr uint32_t HEADER_TIME   = 8;

	enum frame_type : uint8_t
	{
		FRAME_HELLO                   = 0x01, // host: protocol version
		FRAME_HOST_PIN_STATE          = 0x02, // host: host pin levels, ID 0
		FRAME_HOST_SYNC_REQUEST       = 0x03, // host: process all changes through the header timestamp
		FRAME_PERIPHERAL_PIN_STATE    = 0x82, // peripheral: peripheral pin levels
		FRAME_PERIPHERAL_SYNC_REPLY   = 0x83  // peripheral: sync processed; u64 next autonomous output time
	};

	static constexpr uint8_t HELLO_PAYLOAD[] = { 'T', 'O', 'O', 'T', '1', 0, 0, 0 };

	// HOST_PIN_STATE and PERIPHERAL_PIN_STATE payloads: data, control (HOST_PIN_STATE) or status (PERIPHERAL_PIN_STATE), drive mask, reserved
	static constexpr uint32_t STATE_BYTES    = 4;
	static constexpr uint32_t STATE_DATA     = 0;
	static constexpr uint32_t STATE_LINES    = 1;
	static constexpr uint32_t STATE_DRIVE    = 2;
	static constexpr uint32_t STATE_RESERVED = 3;

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
	static constexpr uint8_t  STATUS_UNUSED     = 0xe0;
	static constexpr uint8_t  STATUS_RELEASED   = 0x1f; // nothing attached: all pulled up

	// Drive masks: a set bit is a data line that side drives; released lines pull high.
	static constexpr uint8_t DRIVE_ALL  = 0xff;
	static constexpr uint8_t DRIVE_NONE = 0x00;

	static constexpr uint32_t SYNC_REPLY_BYTES       = 8;
	static constexpr uint32_t MAX_RESPONSE_PAYLOAD = 8;
	static constexpr uint64_t NO_DEADLINE          = UINT64_MAX;

	// Limits
	static constexpr size_t MAX_QUEUED_BYTES        = 0x10000; // outgoing frames
	static constexpr size_t RECEIVE_BUFFER_BYTES    = 0x10000;
	static constexpr uint32_t MAX_RESPONSES         = 4096;    // records per barrier
	static constexpr uint32_t MAX_ROUNDS            = 4096;    // exchanges at one emulated time
	static constexpr uint32_t STALL_TIMEOUT_SECONDS = 5;       // wall time for one exchange

	// The longest gap between exchanges, emulated ns.
	static constexpr uint64_t SERVICE_INTERVAL_NS = 250'000;

	// A local endpoint usually answers within a few yields; after that, back off with sleeps (which can wake much later than asked)
	static constexpr uint32_t SPIN_YIELDS  = 32;
	static constexpr uint32_t BACKOFF_USEC = 20;

	required_device<bitbanger_device> m_stream;

	void stream_mounted(int state);
	void change_data_bit(uint32_t bit, int state);
	void change_control(uint32_t bit, int state);
	void queue_host_state();
	void queue_frame(uint8_t type, uint32_t id, uint64_t time, const uint8_t *payload, uint32_t size);
	void exchange();
	bool receive_response(uint32_t id, uint64_t time, osd_ticks_t deadline);
	void send(const uint8_t *buffer, uint32_t size, osd_ticks_t deadline);
	void receive(uint8_t *buffer, uint32_t size, osd_ticks_t deadline);
	static void check_stall(osd_ticks_t deadline);
	static bool would_block(const std::error_condition &err);
	static void back_off(uint32_t &idle);
	void present(const uint8_t *state);
	uint64_t emulated_ns() const;

	TIMER_CALLBACK_MEMBER(exchange_tick);

	emu_timer *m_exchange_timer;

	// what the host drives
	uint8_t m_data;
	uint8_t m_control;
	uint8_t m_drive;
	bool m_dirty; // changed since the last HOST_PIN_STATE record

	// session
	bool m_connected;
	bool m_hello_sent;
	bool m_got_peripheral_state; // the endpoint has sent its first PERIPHERAL_PIN_STATE
	bool m_exchanging;
	uint32_t m_barrier_id;
	uint64_t m_next_deadline;  // the endpoint's next change of its own, emulated ns
	uint64_t m_completed_time; // when the last exchange finished, emulated ns

	std::vector<uint8_t> m_send_queue;
	std::array<uint8_t, RECEIVE_BUFFER_BYTES> m_receive_buffer;
	uint32_t m_receive_pos;
	uint32_t m_receive_size;

};

DECLARE_DEVICE_TYPE(LPT_SOCKET, lpt_socket_device)

#endif // MAME_BUS_CENTRONICS_LPTSOCKET_H
