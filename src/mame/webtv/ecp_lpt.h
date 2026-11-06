// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_ECP_LPT_H
#define MAME_WEBTV_ECP_LPT_H

#pragma once

#include "bus/centronics/ctronics.h"
#include "bus/centronics/lptlink.h"
#include "machine/buffer.h"


class ecp_lpt_device : public device_t
{

public:

	// bit numbers of the peripheral-driven status lines (PC SPP status register layout)
	static constexpr unsigned STATUS_BIT_FAULT  = 3;
	static constexpr unsigned STATUS_BIT_SELECT = 4;
	static constexpr unsigned STATUS_BIT_PERROR = 5;
	static constexpr unsigned STATUS_BIT_ACK    = 6;
	static constexpr unsigned STATUS_BIT_BUSY   = 7;

	// bit numbers of the host-driven control lines (PC SPP control pin layout)
	static constexpr unsigned CNTL_BIT_STROBE    = 0;
	static constexpr unsigned CNTL_BIT_AUTOFD    = 1;
	static constexpr unsigned CNTL_BIT_INIT      = 2;
	static constexpr unsigned CNTL_BIT_SELECT_IN = 3;

	// PC SPP status register bits, also the internal layout of the peripheral-driven lines
	enum lpt_spp_status : uint8_t
	{
		TIMEOUT = 1 << 0,
		ERROR   = 1 << STATUS_BIT_FAULT, // nFault
		SELECT  = 1 << STATUS_BIT_SELECT,
		PERROR  = 1 << STATUS_BIT_PERROR,
		ACK     = 1 << STATUS_BIT_ACK, // nAck
		BUSY    = 1 << STATUS_BIT_BUSY
	};

	enum lpt_spp_cntl : uint8_t
	{
		BIDIRECTIONAL = 1 << 5,
		IRQ_ENABLE    = 1 << 4,
		SELECT_IN     = 1 << CNTL_BIT_SELECT_IN,
		INITIALISE    = 1 << CNTL_BIT_INIT,
		AUTOFEED      = 1 << CNTL_BIT_AUTOFD,
		STROBE        = 1 << CNTL_BIT_STROBE
	};

	static constexpr uint8_t SPP_PDATA_OFFSET  = 0;
	static constexpr uint8_t SPP_STATUS_OFFSET = 1;
	static constexpr uint8_t SPP_CNTL_OFFSET   = 2;

	static constexpr uint8_t SPP_DEFAULT_VALUE = 0xff;
	static constexpr uint8_t SPP_CNTL_WMASK    = 0x3f;

	// Host-driven control pin levels (internal layout, matches the SPP control pins)
	static constexpr uint8_t PIN_STROBE_N   = 1 << CNTL_BIT_STROBE;
	static constexpr uint8_t PIN_AUTOFD_N   = 1 << CNTL_BIT_AUTOFD;
	static constexpr uint8_t PIN_INIT_N     = 1 << CNTL_BIT_INIT;
	static constexpr uint8_t PIN_SELECTIN_N = 1 << CNTL_BIT_SELECT_IN;
	static constexpr uint8_t PIN_CNTL_MASK  = 0x0f;

	// SOLO stuff

	// DEV_PPORT_DATA
	static constexpr uint32_t SOLO_DATA_MASK = 0xff;

	// DEV_PPORT_CTRL
	static constexpr uint32_t SOLO_CTRL_STB_N    = 1 << 0;
	static constexpr uint32_t SOLO_CTRL_INIT_N   = 1 << 1;
	static constexpr uint32_t SOLO_CTRL_AUTOFD_N = 1 << 2;
	static constexpr uint32_t SOLO_CTRL_SELIN_N  = 1 << 3;
	static constexpr uint32_t SOLO_CTRL_DIR      = 1 << 4; // 1: SOLO drives D0-D7
	static constexpr uint32_t SOLO_CTRL_ECPCMND  = 1 << 5;
	static constexpr uint32_t SOLO_CTRL_MASK     = 0x3f;
	static constexpr uint32_t SOLO_CTRL_IDLE     = SOLO_CTRL_STB_N | SOLO_CTRL_INIT_N | SOLO_CTRL_AUTOFD_N | SOLO_CTRL_SELIN_N;

	// DEV_PPORT_STAT
	static constexpr uint32_t SOLO_STAT_BUSY    = 1 << 0;
	static constexpr uint32_t SOLO_STAT_ACK_N   = 1 << 1;
	static constexpr uint32_t SOLO_STAT_FAULT_N = 1 << 2;
	static constexpr uint32_t SOLO_STAT_SELECT  = 1 << 3;
	static constexpr uint32_t SOLO_STAT_PERROR  = 1 << 4;

	// DEV_PPORT_CNFG
	static constexpr uint32_t SOLO_MODE_SOFTWARE = 0;
	static constexpr uint32_t SOLO_MODE_COMPAT   = 1;
	static constexpr uint32_t SOLO_MODE_ECP      = 2;
	static constexpr uint32_t SOLO_MODE_MASK     = 0x3;

	// DEV_PPORT_FIFOCTRL / FIFOSTAT
	static constexpr uint32_t SOLO_FIFOCTRL_RESET  = 1 << 5;
	static constexpr uint32_t SOLO_FIFOCTRL_THRESH = 0x1f;
	static constexpr uint32_t SOLO_FIFOSTAT_TIP    = 1 << 5;
	static constexpr uint32_t SOLO_FIFOSTAT_AVAIL  = 0x1f;

	// DEV_PPORT_TIMEOUT / ENABLE
	static constexpr uint32_t SOLO_TIMEOUT_ABORT = 1 << 0;
	static constexpr uint32_t SOLO_ENABLE_ON     = 1 << 0;

	// DEV_PPORT_IEN / IST / CLRINT
	static constexpr uint32_t SOLO_INT_THRESH     = 1 << 0;
	static constexpr uint32_t SOLO_INT_ECPREVERSE = 1 << 2;
	static constexpr uint32_t SOLO_INT_ACKDONE    = 1 << 3;
	static constexpr uint32_t SOLO_INT_CFIFOEXCP  = 1 << 4;
	static constexpr uint32_t SOLO_INT_MASK       = 0x1d;

	static constexpr uint8_t FIFO_SIZE = 16;

	ecp_lpt_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// Selects the SOLO register interface instead of the PC (MSNTV2) SPP one (read/write).
	void set_solo_interface(bool solo)
	{
		m_solo_interface = solo;
	}

	// Wires this port to a centronics slot
	void connect_centronics(centronics_device &centronics);

	auto irq_handler()
	{
		return m_irq_handler.bind();
	}

	// PC SPP interface (MSNTV2)
	uint8_t read(offs_t offset);
	void write(offs_t offset, uint8_t data);

	uint8_t data_r();
	void data_w(uint8_t data);
	uint8_t status_r();
	uint8_t control_r();
	void control_w(uint8_t data);

	// SOLO interface
	void solo_map(address_map &map) ATTR_COLD;

	uint32_t solo_data_r();
	void solo_data_w(uint32_t data);
	uint32_t solo_ctrl_r();
	void solo_ctrl_w(uint32_t data);
	uint32_t solo_stat_r();
	uint32_t solo_cnfg_r();
	void solo_cnfg_w(uint32_t data);
	uint32_t solo_fifoctrl_r();
	void solo_fifoctrl_w(uint32_t data);
	uint32_t solo_fifostat_r();
	void solo_timeout_w(uint32_t data);
	uint32_t solo_stat2_r();
	uint32_t solo_ien_r();
	void solo_ien_w(uint32_t data);
	uint32_t solo_ist_r();
	void solo_clrint_w(uint32_t data);
	uint32_t solo_enable_r();
	void solo_enable_w(uint32_t data);

	template <unsigned BITIDX> auto hst2dev_pdata_bit_handler()
	{
		return m_hst2dev_pdata_w[BITIDX].bind();
	}

	template <unsigned BITIDX> auto hst2dev_cntl_bit_handler()
	{
		return m_hst2dev_cntl_w[BITIDX].bind();
	}

	template <unsigned BITIDX> void dev2hst_status_wbit(int state)
	{
		const uint8_t old_status = m_dev2hst_status;
		change_bit(state, BITIDX, &m_dev2hst_status);
		if (old_status != m_dev2hst_status)
			status_changed(old_status);
	}

	template <unsigned BITIDX> void dev2hst_cntl_wbit(int state)
	{
		change_bit(state, BITIDX, &m_dev2hst_cntl);
	}

	void hst2dev_cntl_irq_enable_w(int state);

	template <typename T> static void change_bit(int state, uint8_t bit, T *data)
	{
		if (state)
			*data = *data | (1 << bit);
		else
			*data = *data & ~(1 << bit);
	}

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:

	// Undriven status lines read high through the board's pull-ups
	static constexpr uint8_t STATUS_PULLED_UP = 0xff;
	static constexpr uint8_t DATA_RELEASED    = 0xff;

	// The spec's "750ns timer"
	static constexpr attotime HS_STEP_TIME = attotime::from_nsec(750);

	// Hardware handshake states (SOLO Compatibility FIFO and ECP modes)
	enum handshake_state : uint8_t
	{
		HS_IDLE = 0,
		HS_FWD_SETUP,      // data on the lines, waiting out the setup time
		HS_FWD_STROBE,     // nStrobe low
		HS_FWD_RECOVER,    // nStrobe high again, waiting for the peripheral to finish
		HS_REV_WAIT_CLK,   // ECP reverse: nAutoFd (HostAck) low, waiting for nAck (PeriphClk) low
		HS_REV_WAIT_ROOM,  // ECP reverse: byte latched but the FIFO is full
		HS_REV_WAIT_CLK_HI // ECP reverse: nAutoFd high, waiting for nAck high
	};

	devcb_write_line m_irq_handler;

	required_device<input_buffer_device> m_dev2hst_pdata;
	devcb_write_line::array<8> m_hst2dev_pdata_w;
	uint8_t m_hst2dev_pdata;

	uint8_t m_dev2hst_status;

	devcb_write_line::array<8> m_hst2dev_cntl_w;
	uint8_t m_hst2dev_cntl;
	uint8_t m_dev2hst_cntl;

	bool m_irq_state;

	bool m_solo_interface;

	uint32_t m_solo_data;
	uint32_t m_solo_ctrl;
	uint32_t m_solo_mode;
	uint32_t m_solo_fifo_thresh;
	uint32_t m_solo_ien;
	uint32_t m_solo_ist;
	uint32_t m_solo_enable;

	uint8_t m_fifo[FIFO_SIZE];
	uint8_t m_fifo_head;
	uint8_t m_fifo_count;
	uint8_t m_reverse_latch; // last byte the peripheral clocked in, ECP reverse

	uint8_t m_hs_state;
	uint32_t m_hs_mode; // the mode the byte in flight started in
	uint8_t m_hs_data;
	bool m_hs_strobe_n; // nStrobe level the hardware drives
	bool m_hs_autofd_n; // nAutoFd (HostAck) level the hardware drives in ECP reverse
	bool m_hs_terminating; // a forward byte finishes in the old mode after a mode change
	bool m_hs_running;
	bool m_hs_rerun;
	bool m_hs_delay_done;

	template <typename T> void signal_changed_bits(T &&lines, uint8_t old_data, uint8_t new_data)
	{
		const uint8_t changed = old_data ^ new_data;
		for (int i = 0; i < 8; i++)
		{
			if (BIT(changed, i))
				lines[i](BIT(new_data, i));
		}
	}

	void drive_data_pins(uint8_t data);
	void drive_cntl_pins(uint8_t levels);
	uint8_t peripheral_data()
	{
		return m_dev2hst_pdata->read();
	}
	void status_changed(uint8_t old_status);
	void set_parallel_irq();

	// device_ecp_lpt_link_interface card, if the slot holds one
	void link_data_dir(bool host_drives);
	void link_sample_lines();

	centronics_device *m_centronics;
	device_ecp_lpt_link_interface *m_link;
	int m_link_data_dir; // last direction told to m_link, -1 = not told yet

	// SOLO engine
	bool solo_dir_out() const;
	bool solo_ecp_reverse() const;
	bool solo_forward_in_flight() const;
	bool solo_reverse_in_flight() const;
	uint8_t fifo_free() const
	{
		return FIFO_SIZE - m_fifo_count;
	}
	void fifo_push(uint8_t data);
	uint8_t fifo_pop();
	void fifo_reset();
	void solo_reset_registers();
	void solo_update_pins();
	void solo_set_ist(uint32_t mask);
	void solo_update_irq();
	void solo_hw_transferred();
	void solo_hs_run();
	void solo_hs_step();
	void solo_hs_idle(bool busy);
	void solo_hs_next_step();
	void solo_hs_set_autofd(bool autofd_n, uint8_t next_state);
	void solo_hs_delay();

	emu_timer *m_hs_timer;
	TIMER_CALLBACK_MEMBER(hs_timer_tick);
};

DECLARE_DEVICE_TYPE(ECP_LPT, ecp_lpt_device)

#endif // MAME_WEBTV_ECP_LPT_H
