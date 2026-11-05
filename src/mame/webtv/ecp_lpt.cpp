// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"

#include "ecp_lpt.h"

DEFINE_DEVICE_TYPE(ECP_LPT, ecp_lpt_device, "ecp_lpt", "IEEE 1284 SPP/EPP/ECP LPT Controller")

ecp_lpt_device::ecp_lpt_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, ECP_LPT, tag, owner, clock),
	m_irq_handler(*this),
	m_dev2hst_pdata(*this, "dev2hst_pdata"),
	m_hst2dev_pdata_w(*this),
	m_hst2dev_cntl_w(*this),
	m_solo_interface(false)
{
	//
}

void ecp_lpt_device::device_start()
{
	m_hst2dev_pdata = 0x00;
	m_hst2dev_cntl = 0x00;
	m_dev2hst_cntl = 0x00;
	m_irq_state = false;

	m_dev2hst_status = 0x00;
	if (m_solo_interface)
		m_dev2hst_status = STATUS_PULLED_UP;

	solo_reset_registers();
	std::fill(std::begin(m_fifo), std::end(m_fifo), 0);
	m_reverse_latch = 0;
	m_hs_running = false;
	m_hs_rerun = false;

	m_hs_timer = timer_alloc(FUNC(ecp_lpt_device::hs_timer_tick), this);

	save_item(NAME(m_hst2dev_pdata));
	save_item(NAME(m_dev2hst_status));
	save_item(NAME(m_hst2dev_cntl));
	save_item(NAME(m_dev2hst_cntl));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_solo_data));
	save_item(NAME(m_solo_ctrl));
	save_item(NAME(m_solo_mode));
	save_item(NAME(m_solo_fifo_thresh));
	save_item(NAME(m_solo_ien));
	save_item(NAME(m_solo_ist));
	save_item(NAME(m_solo_enable));
	save_item(NAME(m_fifo));
	save_item(NAME(m_fifo_head));
	save_item(NAME(m_fifo_count));
	save_item(NAME(m_reverse_latch));
	save_item(NAME(m_hs_state));
	save_item(NAME(m_hs_mode));
	save_item(NAME(m_hs_data));
	save_item(NAME(m_hs_strobe_n));
	save_item(NAME(m_hs_autofd_n));
	save_item(NAME(m_hs_terminating));
	save_item(NAME(m_hs_delay_done));
}

void ecp_lpt_device::device_reset()
{
	m_irq_state = false;

	if (m_solo_interface)
	{
		m_hs_timer->adjust(attotime::never);

		solo_reset_registers();
		m_hs_running = false;
		m_hs_rerun = false;

		solo_update_pins();
		m_irq_handler(CLEAR_LINE);
	}
	else
	{
		m_hst2dev_pdata = 0x00;
		m_dev2hst_status = 0x00;
		m_hst2dev_cntl = 0x00;
		m_dev2hst_cntl = 0x00;

		data_w(SPP_DEFAULT_VALUE);
		control_w(~lpt_spp_cntl::INITIALISE);
	}
}

void ecp_lpt_device::solo_reset_registers()
{
	m_solo_data = 0x00;
	m_solo_ctrl = SOLO_CTRL_IDLE;
	m_solo_mode = SOLO_MODE_SOFTWARE;
	m_solo_fifo_thresh = 0;
	m_solo_ien = 0;
	m_solo_ist = 0;
	m_solo_enable = 0;

	fifo_reset();

	m_hs_state = HS_IDLE;
	m_hs_mode = SOLO_MODE_SOFTWARE;
	m_hs_data = 0x00;
	m_hs_strobe_n = true;
	m_hs_autofd_n = true;
	m_hs_terminating = false;
	m_hs_delay_done = false;
}

void ecp_lpt_device::device_add_mconfig(machine_config &config)
{
	INPUT_BUFFER(config, m_dev2hst_pdata);
}

void ecp_lpt_device::connect_centronics(centronics_device &centronics)
{
	centronics.set_data_input_buffer(m_dev2hst_pdata);

	centronics.fault_handler().set(*this, FUNC(ecp_lpt_device::dev2hst_status_wbit<STATUS_BIT_FAULT>));
	centronics.select_handler().set(*this, FUNC(ecp_lpt_device::dev2hst_status_wbit<STATUS_BIT_SELECT>));
	centronics.perror_handler().set(*this, FUNC(ecp_lpt_device::dev2hst_status_wbit<STATUS_BIT_PERROR>));
	centronics.ack_handler().set(*this, FUNC(ecp_lpt_device::dev2hst_status_wbit<STATUS_BIT_ACK>));
	centronics.busy_handler().set(*this, FUNC(ecp_lpt_device::dev2hst_status_wbit<STATUS_BIT_BUSY>));

	hst2dev_pdata_bit_handler<0>().set(centronics, FUNC(centronics_device::write_data0));
	hst2dev_pdata_bit_handler<1>().set(centronics, FUNC(centronics_device::write_data1));
	hst2dev_pdata_bit_handler<2>().set(centronics, FUNC(centronics_device::write_data2));
	hst2dev_pdata_bit_handler<3>().set(centronics, FUNC(centronics_device::write_data3));
	hst2dev_pdata_bit_handler<4>().set(centronics, FUNC(centronics_device::write_data4));
	hst2dev_pdata_bit_handler<5>().set(centronics, FUNC(centronics_device::write_data5));
	hst2dev_pdata_bit_handler<6>().set(centronics, FUNC(centronics_device::write_data6));
	hst2dev_pdata_bit_handler<7>().set(centronics, FUNC(centronics_device::write_data7));

	hst2dev_cntl_bit_handler<CNTL_BIT_STROBE>().set(centronics, FUNC(centronics_device::write_strobe));
	hst2dev_cntl_bit_handler<CNTL_BIT_AUTOFD>().set(centronics, FUNC(centronics_device::write_autofd));
	hst2dev_cntl_bit_handler<CNTL_BIT_INIT>().set(centronics, FUNC(centronics_device::write_init));
	hst2dev_cntl_bit_handler<CNTL_BIT_SELECT_IN>().set(centronics, FUNC(centronics_device::write_select_in));
}

void ecp_lpt_device::drive_data_pins(uint8_t data)
{
	signal_changed_bits(m_hst2dev_pdata_w, m_hst2dev_pdata, data);
	m_hst2dev_pdata = data;
}

void ecp_lpt_device::drive_cntl_pins(uint8_t levels)
{
	const uint8_t data = (m_hst2dev_cntl & ~PIN_CNTL_MASK) | (levels & PIN_CNTL_MASK);
	signal_changed_bits(m_hst2dev_cntl_w, m_hst2dev_cntl, data);
	m_hst2dev_cntl = data;
}

void ecp_lpt_device::status_changed(uint8_t old_status)
{
	if (!m_solo_interface)
		return;

	const uint8_t rose = ~old_status & m_dev2hst_status;
	const uint8_t fell = old_status & ~m_dev2hst_status;

	switch (m_solo_mode)
	{
		case SOLO_MODE_SOFTWARE:
		{
			if (rose & lpt_spp_status::ACK)
				solo_set_ist(SOLO_INT_ACKDONE);
			break;
		}

		case SOLO_MODE_COMPAT:
		{
			// Paper out or a fault while the FIFO is printing.
			const bool exception = (rose & lpt_spp_status::PERROR) || (fell & lpt_spp_status::ERROR);
			if (exception)
				solo_set_ist(SOLO_INT_CFIFOEXCP);
			break;
		}

		case SOLO_MODE_ECP:
			if (fell & lpt_spp_status::ERROR)
				solo_set_ist(SOLO_INT_ECPREVERSE);
			break;
	}

	solo_hs_run();
}

// PC SPP interface

uint8_t ecp_lpt_device::read(offs_t offset)
{
	switch (offset)
	{
		case SPP_PDATA_OFFSET:
			return data_r();

		case SPP_STATUS_OFFSET:
			return status_r();

		case SPP_CNTL_OFFSET:
			return control_r();

		default:
			return SPP_DEFAULT_VALUE;
	}
}

void ecp_lpt_device::write(offs_t offset, uint8_t data)
{
	switch (offset)
	{
		case SPP_PDATA_OFFSET:
			data_w(data);
			break;

		case SPP_CNTL_OFFSET:
			control_w(data);
			break;

		default:
			break; // the status register is read-only
	}
}

uint8_t ecp_lpt_device::data_r()
{
	return m_hst2dev_pdata & m_dev2hst_pdata->read();
}

void ecp_lpt_device::data_w(uint8_t data)
{
	drive_data_pins(data);
}

uint8_t ecp_lpt_device::status_r()
{
	return m_dev2hst_status;
}

uint8_t ecp_lpt_device::control_r()
{
	const uint8_t data = m_hst2dev_cntl & m_dev2hst_cntl & SPP_CNTL_WMASK;
	return ~data;
}

void ecp_lpt_device::control_w(uint8_t data)
{
	// The control register is inverted from the pins, except for nInit.
	data = ~(data ^ lpt_spp_cntl::INITIALISE);

	signal_changed_bits(m_hst2dev_cntl_w, m_hst2dev_cntl, data);
	m_hst2dev_cntl = data;
}

void ecp_lpt_device::hst2dev_cntl_irq_enable_w(int)
{
	set_parallel_irq();
}

void ecp_lpt_device::set_parallel_irq()
{
	const bool ack = m_dev2hst_status & lpt_spp_status::ACK;
	const bool irq_enable = m_hst2dev_cntl & lpt_spp_cntl::IRQ_ENABLE;
	const bool new_irq_state = irq_enable || ack;

	if (m_irq_state == new_irq_state)
		return;

	if (new_irq_state)
		m_irq_handler(CLEAR_LINE);
	else
		m_irq_handler(ASSERT_LINE);

	m_irq_state = new_irq_state;
}

void ecp_lpt_device::solo_map(address_map &map)
{
	map(0x00, 0x03).rw(FUNC(ecp_lpt_device::solo_data_r), FUNC(ecp_lpt_device::solo_data_w));         // DEV_PPORT_DATA
	map(0x04, 0x07).rw(FUNC(ecp_lpt_device::solo_ctrl_r), FUNC(ecp_lpt_device::solo_ctrl_w));         // DEV_PPORT_CTRL
	map(0x08, 0x0b).r(FUNC(ecp_lpt_device::solo_stat_r));                                             // DEV_PPORT_STAT
	map(0x0c, 0x0f).rw(FUNC(ecp_lpt_device::solo_cnfg_r), FUNC(ecp_lpt_device::solo_cnfg_w));         // DEV_PPORT_CNFG
	map(0x10, 0x13).rw(FUNC(ecp_lpt_device::solo_fifoctrl_r), FUNC(ecp_lpt_device::solo_fifoctrl_w)); // DEV_PPORT_FIFOCTRL
	map(0x14, 0x17).r(FUNC(ecp_lpt_device::solo_fifostat_r));                                         // DEV_PPORT_FIFOSTAT
	map(0x18, 0x1b).w(FUNC(ecp_lpt_device::solo_timeout_w));                                          // DEV_PPORT_TIMEOUT
	map(0x1c, 0x1f).r(FUNC(ecp_lpt_device::solo_stat2_r));                                            // DEV_PPORT_STAT2
	map(0x20, 0x23).rw(FUNC(ecp_lpt_device::solo_ien_r), FUNC(ecp_lpt_device::solo_ien_w));           // DEV_PPORT_IEN
	map(0x24, 0x27).r(FUNC(ecp_lpt_device::solo_ist_r));                                              // DEV_PPORT_IST
	map(0x28, 0x2b).w(FUNC(ecp_lpt_device::solo_clrint_w));                                           // DEV_PPORT_CLRINT
	map(0x2c, 0x2f).rw(FUNC(ecp_lpt_device::solo_enable_r), FUNC(ecp_lpt_device::solo_enable_w));     // DEV_PPORT_ENABLE
}

bool ecp_lpt_device::solo_dir_out() const
{
	return (m_solo_ctrl & SOLO_CTRL_DIR) != 0;
}

bool ecp_lpt_device::solo_ecp_reverse() const
{
	return m_solo_mode == SOLO_MODE_ECP && !(m_solo_ctrl & SOLO_CTRL_INIT_N);
}

bool ecp_lpt_device::solo_forward_in_flight() const
{
	return m_hs_state == HS_FWD_SETUP || m_hs_state == HS_FWD_STROBE || m_hs_state == HS_FWD_RECOVER;
}

bool ecp_lpt_device::solo_reverse_in_flight() const
{
	return m_hs_state >= HS_REV_WAIT_CLK;
}

uint32_t ecp_lpt_device::solo_data_r()
{
	if (!solo_ecp_reverse())
	{
		return m_hst2dev_pdata & peripheral_data();
	}
	else if (m_fifo_count == 0)
	{
		return m_reverse_latch;
	}
	else if (machine().side_effects_disabled())
	{
		return m_fifo[m_fifo_head];
	}
	else
	{
		const uint8_t data = fifo_pop();

		// The peripheral may be waiting for room.
		solo_hs_run();

		return data;
	}
}

void ecp_lpt_device::solo_data_w(uint32_t data)
{
	data &= SOLO_DATA_MASK;

	if (m_solo_mode == SOLO_MODE_SOFTWARE)
	{
		m_solo_data = data;
		solo_update_pins();
	}
	else if (!solo_ecp_reverse())
	{
		fifo_push(data);
		solo_hs_run();
	}
}

uint32_t ecp_lpt_device::solo_ctrl_r()
{
	return m_solo_ctrl;
}

void ecp_lpt_device::solo_ctrl_w(uint32_t data)
{
	const uint32_t old_ctrl = m_solo_ctrl;
	m_solo_ctrl = data & SOLO_CTRL_MASK;

	const bool direction_changed = ((old_ctrl ^ m_solo_ctrl) & SOLO_CTRL_INIT_N) != 0;
	if (m_solo_mode == SOLO_MODE_ECP && direction_changed)
	{
		fifo_reset();
		if (solo_reverse_in_flight())
			m_hs_state = HS_IDLE;
	}

	solo_update_pins();
	solo_hs_run();
}

uint32_t ecp_lpt_device::solo_stat_r()
{
	const uint8_t status = m_dev2hst_status;
	uint32_t stat = 0;

	if (status & lpt_spp_status::BUSY)
		stat |= SOLO_STAT_BUSY;
	if (status & lpt_spp_status::ACK)
		stat |= SOLO_STAT_ACK_N;
	if (status & lpt_spp_status::ERROR)
		stat |= SOLO_STAT_FAULT_N;
	if (status & lpt_spp_status::SELECT)
		stat |= SOLO_STAT_SELECT;
	if (status & lpt_spp_status::PERROR)
		stat |= SOLO_STAT_PERROR;

	return stat;
}

uint32_t ecp_lpt_device::solo_cnfg_r()
{
	return m_solo_mode;
}

void ecp_lpt_device::solo_cnfg_w(uint32_t data)
{
	const uint32_t old_mode = m_solo_mode;
	m_solo_mode = data & SOLO_MODE_MASK;

	if (m_solo_mode == old_mode)
		return;

	if (old_mode == SOLO_MODE_SOFTWARE)
	{
		m_hs_data = m_solo_data;
		m_hs_strobe_n = (m_solo_ctrl & SOLO_CTRL_STB_N) != 0;
		m_hs_autofd_n = true;
	}

	if (solo_reverse_in_flight())
	{
		m_hs_state = HS_IDLE;
	}
	else if (m_hs_state != HS_IDLE && m_solo_mode != m_hs_mode)
	{
		// A forward byte is on its way: it completes in the old mode, and FIFOSTAT.TIP shows it.
		m_hs_terminating = true;
	}

	solo_update_pins();
	solo_hs_run();
}

uint32_t ecp_lpt_device::solo_fifoctrl_r()
{
	return m_solo_fifo_thresh;
}

void ecp_lpt_device::solo_fifoctrl_w(uint32_t data)
{
	m_solo_fifo_thresh = data & SOLO_FIFOCTRL_THRESH;

	if (data & SOLO_FIFOCTRL_RESET)
	{
		fifo_reset();
		solo_hs_run();
	}
}

uint32_t ecp_lpt_device::solo_fifostat_r()
{
	uint32_t stat = fifo_free() & SOLO_FIFOSTAT_AVAIL;
	if (m_hs_terminating)
		stat |= SOLO_FIFOSTAT_TIP;
	return stat;
}

void ecp_lpt_device::solo_timeout_w(uint32_t data)
{
	if (!(data & SOLO_TIMEOUT_ABORT) || !solo_forward_in_flight())
		return;

	m_hs_timer->adjust(attotime::never);
	m_hs_state = HS_IDLE;
	m_hs_strobe_n = true;
	m_hs_terminating = false;

	solo_update_pins();
	solo_hs_run();
}

uint32_t ecp_lpt_device::solo_stat2_r()
{
	const uint8_t pins = m_hst2dev_cntl;
	uint32_t stat = 0;

	if (pins & PIN_STROBE_N)
		stat |= SOLO_CTRL_STB_N;
	if (pins & PIN_INIT_N)
		stat |= SOLO_CTRL_INIT_N;
	if (pins & PIN_AUTOFD_N)
		stat |= SOLO_CTRL_AUTOFD_N;
	if (pins & PIN_SELECTIN_N)
		stat |= SOLO_CTRL_SELIN_N;

	return stat;
}

uint32_t ecp_lpt_device::solo_ien_r()
{
	return m_solo_ien;
}

void ecp_lpt_device::solo_ien_w(uint32_t data)
{
	m_solo_ien = data & SOLO_INT_MASK;
	solo_update_irq();
}

uint32_t ecp_lpt_device::solo_ist_r()
{
	return m_solo_ist;
}

void ecp_lpt_device::solo_clrint_w(uint32_t data)
{
	m_solo_ist &= ~(data & SOLO_INT_MASK);
	solo_update_irq();
}

uint32_t ecp_lpt_device::solo_enable_r()
{
	return m_solo_enable;
}

void ecp_lpt_device::solo_enable_w(uint32_t data)
{
	m_solo_enable = data & SOLO_ENABLE_ON;
	solo_update_irq();
	solo_hs_run();
}

void ecp_lpt_device::fifo_push(uint8_t data)
{
	if (m_fifo_count >= FIFO_SIZE)
		return;

	m_fifo[(m_fifo_head + m_fifo_count) % FIFO_SIZE] = data;
	m_fifo_count++;
}

uint8_t ecp_lpt_device::fifo_pop()
{
	const uint8_t data = m_fifo[m_fifo_head];
	m_fifo_head = (m_fifo_head + 1) % FIFO_SIZE;
	m_fifo_count--;
	return data;
}

void ecp_lpt_device::fifo_reset()
{
	m_fifo_head = 0;
	m_fifo_count = 0;
}

void ecp_lpt_device::solo_update_pins()
{
	const bool hardware = m_solo_mode != SOLO_MODE_SOFTWARE || m_hs_terminating;

	bool strobe_n = (m_solo_ctrl & SOLO_CTRL_STB_N) != 0;
	if (hardware)
		strobe_n = m_hs_strobe_n;

	bool autofd_n = (m_solo_ctrl & SOLO_CTRL_AUTOFD_N) != 0;
	if (m_solo_mode == SOLO_MODE_ECP)
	{
		// Forward: HostAck low flags a command byte. Reverse: HostAck is the state machine's
		if (solo_ecp_reverse())
			autofd_n = m_hs_autofd_n;
		else
			autofd_n = !(m_solo_ctrl & SOLO_CTRL_ECPCMND);
	}

	uint8_t levels = 0;
	if (strobe_n)
		levels |= PIN_STROBE_N;
	if (autofd_n)
		levels |= PIN_AUTOFD_N;
	if (m_solo_ctrl & SOLO_CTRL_INIT_N)
		levels |= PIN_INIT_N;
	if (m_solo_ctrl & SOLO_CTRL_SELIN_N)
		levels |= PIN_SELECTIN_N;

	uint8_t data = DATA_RELEASED;
	if (solo_dir_out() && hardware)
		data = m_hs_data;
	else if (solo_dir_out())
		data = m_solo_data;

	drive_data_pins(data);
	drive_cntl_pins(levels);
}

void ecp_lpt_device::solo_set_ist(uint32_t mask)
{
	m_solo_ist |= mask;
	solo_update_irq();
}

void ecp_lpt_device::solo_update_irq()
{
	const bool new_irq_state = m_solo_enable && (m_solo_ist & m_solo_ien) != 0;
	if (m_irq_state == new_irq_state)
		return;

	m_irq_state = new_irq_state;
	if (new_irq_state)
		m_irq_handler(ASSERT_LINE);
	else
		m_irq_handler(CLEAR_LINE);
}

void ecp_lpt_device::solo_hw_transferred()
{
	// Only transfers between the port and the peripheral count towards the threshold.
	if (fifo_free() == m_solo_fifo_thresh)
		solo_set_ist(SOLO_INT_THRESH);
}

void ecp_lpt_device::solo_hs_delay()
{
	m_hs_delay_done = false;
	m_hs_timer->adjust(HS_STEP_TIME);
}

TIMER_CALLBACK_MEMBER(ecp_lpt_device::hs_timer_tick)
{
	m_hs_delay_done = true;
	solo_hs_run();
}

void ecp_lpt_device::solo_hs_run()
{
	if (!m_solo_interface)
		return;

	if (m_hs_running)
	{
		m_hs_rerun = true;
		return;
	}

	m_hs_running = true;
	do
	{
		m_hs_rerun = false;
		solo_hs_step();
	} while (m_hs_rerun);
	m_hs_running = false;
}

void ecp_lpt_device::solo_hs_next_step()
{
	if (m_hs_mode == SOLO_MODE_COMPAT)
		solo_hs_delay();
	else
		m_hs_rerun = true;
}

void ecp_lpt_device::solo_hs_set_autofd(bool autofd_n, uint8_t next_state)
{
	m_hs_autofd_n = autofd_n;
	m_hs_state = next_state;
	solo_update_pins();
	m_hs_rerun = true;
}

void ecp_lpt_device::solo_hs_idle(bool busy)
{
	if (m_hs_terminating)
	{
		m_hs_terminating = false;
		solo_update_pins();
	}

	if (!m_solo_enable || m_solo_mode == SOLO_MODE_SOFTWARE)
		return;

	if (solo_ecp_reverse())
	{
		// HostAck low: ready for the peripheral's first byte.
		m_hs_mode = m_solo_mode;
		solo_hs_set_autofd(false, HS_REV_WAIT_CLK);
	}
	else if (m_fifo_count > 0 && !busy)
	{
		m_hs_mode = m_solo_mode;
		m_hs_data = fifo_pop();
		m_hs_state = HS_FWD_SETUP;
		solo_hw_transferred();
		solo_update_pins();
		solo_hs_delay();
	}
}

void ecp_lpt_device::solo_hs_step()
{
	const bool busy = (m_dev2hst_status & lpt_spp_status::BUSY) != 0;
	const bool ack_n = (m_dev2hst_status & lpt_spp_status::ACK) != 0;
	const bool compat = m_hs_mode == SOLO_MODE_COMPAT;
	const bool reverse_on = solo_ecp_reverse() && m_solo_enable;

	switch (m_hs_state)
	{
		case HS_IDLE:
			solo_hs_idle(busy);
			break;

		case HS_FWD_SETUP:
		{
			if (!m_hs_delay_done)
				break;

			m_hs_strobe_n = false;
			m_hs_state = HS_FWD_STROBE;
			solo_update_pins();
			solo_hs_next_step();
			break;
		}

		case HS_FWD_STROBE:
		{
			// Compatibility: a fixed-width pulse. ECP: hold HostClk low until PeriphAck rises.
			bool strobe_done = busy;
			if (compat)
				strobe_done = m_hs_delay_done;
			if (!strobe_done)
				break;

			m_hs_strobe_n = true;
			m_hs_state = HS_FWD_RECOVER;
			solo_update_pins();
			solo_hs_next_step();
			break;
		}

		case HS_FWD_RECOVER:
		{
			const bool pulse_recovering = compat && !m_hs_delay_done;
			if (pulse_recovering || busy)
				break;

			m_hs_state = HS_IDLE;
			m_hs_rerun = true;
			break;
		}

		case HS_REV_WAIT_CLK:
		{
			if (!reverse_on)
			{
				m_hs_state = HS_IDLE;
				m_hs_rerun = true;
				break;
			}
			if (ack_n)
				break;

			m_reverse_latch = peripheral_data();
			if (!busy)
			{
				// Busy low flags a command byte (channel address or RLE count): dropped.
				solo_hs_set_autofd(true, HS_REV_WAIT_CLK_HI);
				break;
			}

			m_hs_state = HS_REV_WAIT_ROOM;
			m_hs_rerun = true;
			break;
		}

		case HS_REV_WAIT_ROOM:
		{
			if (!reverse_on)
			{
				m_hs_state = HS_IDLE;
				m_hs_rerun = true;
				break;
			}

			// FIFO full: hold HostAck low, which stalls the peripheral until software reads.
			if (m_fifo_count >= FIFO_SIZE)
				break;

			fifo_push(m_reverse_latch);
			solo_hw_transferred();
			solo_hs_set_autofd(true, HS_REV_WAIT_CLK_HI);
			break;
		}

		case HS_REV_WAIT_CLK_HI:
		{
			if (!ack_n)
				break;

			solo_hs_set_autofd(false, HS_REV_WAIT_CLK);
			break;
		}
	}
}
