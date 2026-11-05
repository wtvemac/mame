// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVLPTHOST_H
#define MAME_WEBTV_WTVLPTHOST_H

#pragma once

#include "bus/centronics/ctronics.h"
#include "ecp_lpt.h"

#include <string>

// Linux only (ppdev): elsewhere the device doesn't exist.
#if defined(__linux__)

class wtv_lpt_host_device :
	public device_t,
	public device_centronics_peripheral_interface,
	public device_ecp_lpt_link_interface,
	public device_image_interface
{

public:

	wtv_lpt_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// device_image_interface
	virtual std::pair<std::error_condition, std::string> call_load() override;
	virtual void call_unload() override;
	virtual bool core_opens_image_file() const noexcept override { return false; }
	virtual bool is_readable() const noexcept override { return true; }
	virtual bool is_writeable() const noexcept override { return true; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual const char *file_extensions() const noexcept override { return ""; }
	virtual const char *image_type_name() const noexcept override { return "hostlpt"; }
	virtual const char *image_brief_type_name() const noexcept override { return "hlpt"; }

	// device_ecp_lpt_link_interface
	virtual void host_data_dir(bool host_drives) override;
	virtual void host_sample_lines() override;

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

	// device_centronics_peripheral_interface
	virtual void input_strobe(int state) override { change_control(PIN_STROBE_N, state); }
	virtual void input_data0(int state) override { change_data_bit(0, state); }
	virtual void input_data1(int state) override { change_data_bit(1, state); }
	virtual void input_data2(int state) override { change_data_bit(2, state); }
	virtual void input_data3(int state) override { change_data_bit(3, state); }
	virtual void input_data4(int state) override { change_data_bit(4, state); }
	virtual void input_data5(int state) override { change_data_bit(5, state); }
	virtual void input_data6(int state) override { change_data_bit(6, state); }
	virtual void input_data7(int state) override { change_data_bit(7, state); }
	virtual void input_autofd(int state) override { change_control(PIN_AUTOFD_N, state); }
	virtual void input_init(int state) override { change_control(PIN_INIT_N, state); }
	virtual void input_select_in(int state) override { change_control(PIN_SELECTIN_N, state); }

private:

	// box-driven control pin levels, as ecp_lpt lays them out
	static constexpr uint8_t PIN_STROBE_N = ecp_lpt_device::PIN_STROBE_N;
	static constexpr uint8_t PIN_AUTOFD_N = ecp_lpt_device::PIN_AUTOFD_N;
	static constexpr uint8_t PIN_INIT_N = ecp_lpt_device::PIN_INIT_N;
	static constexpr uint8_t PIN_SELECTIN_N = ecp_lpt_device::PIN_SELECTIN_N;

	// peripheral-driven status pin levels, as ecp_lpt lays them out
	static constexpr uint8_t STATUS_FAULT_N = ecp_lpt_device::ERROR;
	static constexpr uint8_t STATUS_SELECT = ecp_lpt_device::SELECT;
	static constexpr uint8_t STATUS_PERROR = ecp_lpt_device::PERROR;
	static constexpr uint8_t STATUS_ACK_N = ecp_lpt_device::ACK;
	static constexpr uint8_t STATUS_BUSY = ecp_lpt_device::BUSY;
	static constexpr uint8_t STATUS_PULLED_UP = STATUS_FAULT_N | STATUS_SELECT | STATUS_PERROR | STATUS_ACK_N | STATUS_BUSY;
	static constexpr uint8_t DATA_RELEASED = 0xff;

	// How often the peripheral's lines are read when the box isn't reading them itself. The fast
	// rate holds for ACTIVE_HOLD after the last change on either side.
	static constexpr attotime ACTIVE_POLL = attotime::from_usec(4);
	static constexpr attotime IDLE_POLL = attotime::from_usec(500);
	static constexpr attotime ACTIVE_HOLD = attotime::from_msec(20);

	bool port_open() const { return m_fd >= 0; }

	void change_data_bit(int bit, int state);
	void change_control(uint8_t pin, int state);
	void mark_active();
	void flush_data();
	void write_control();
	void apply_data_dir();
	void sample_lines(bool force = false);
	void present(uint8_t data, uint8_t status, bool force);
	void port_failed(const char *what);
	void close_port();

	TIMER_CALLBACK_MEMBER(poll_tick);
	TIMER_CALLBACK_MEMBER(flush_tick);

	int m_fd;

	// what the box drives
	uint8_t m_box_data;
	uint8_t m_box_control;
	bool m_box_drives_data;

	// what's on the host port
	uint8_t m_port_data;
	bool m_port_data_valid;
	bool m_port_reverse;
	bool m_port_can_reverse;

	// what the box has been shown
	uint8_t m_shown_data;
	uint8_t m_shown_status;

	attotime m_active_until;

	emu_timer *m_poll_timer;
	emu_timer *m_flush_timer;

};

DECLARE_DEVICE_TYPE(WTV_LPT_HOST, wtv_lpt_host_device)

#endif // defined(__linux__)

#endif // MAME_WEBTV_WTVLPTHOST_H
