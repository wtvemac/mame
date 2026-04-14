// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/bus/isa/

// Description here

#include "emu.h"
#include "lpc47m192.h"
#include "bus/centronics/ctronics.h"
#include "bus/rs232/null_modem.h"
#include "machine/pckeybrd.h"
#include "ps2_mouse.h"

DEFINE_DEVICE_TYPE(LPC47M192, lpc47m192_device, "lpc47m192", "SMSC LPC47M192 Super I/O")

lpc47m192_device::lpc47m192_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, LPC47M192, tag, owner, clock),
	device_isa16_card_interface(mconfig, *this),
	m_serial(*this, "serial%d", 1),
	m_parallel(*this, "parallel"),
	m_kbdc(*this, "kbdc"),
	m_kbd(*this, finder_base::DUMMY_TAG),
	m_irq1_cb(*this),
	m_irq2_cb(*this),
	m_irq3_cb(*this),
	m_irq4_cb(*this),
	m_irq5_cb(*this),
	m_irq6_cb(*this),
	m_irq7_cb(*this),
	m_irq8_cb(*this),
	m_irq9_cb(*this),
	m_irq10_cb(*this),
	m_irq11_cb(*this),
	m_irq12_cb(*this),
	m_irq13_cb(*this),
	m_irq14_cb(*this),
	m_irq15_cb(*this),
	m_system_reset_cb(*this),
	m_gate_a20_cb(*this),
	m_par_h2dd0_w_cb(*this),
	m_par_h2dd1_w_cb(*this),
	m_par_h2dd2_w_cb(*this),
	m_par_h2dd3_w_cb(*this),
	m_par_h2dd4_w_cb(*this),
	m_par_h2dd5_w_cb(*this),
	m_par_h2dd6_w_cb(*this),
	m_par_h2dd7_w_cb(*this),
	m_par_h2dc0_w_cb(*this),
	m_par_h2dc1_w_cb(*this),
	m_par_h2dc2_w_cb(*this),
	m_par_h2dc3_w_cb(*this),
	m_par_h2dc4_w_cb(*this),
	m_s1_txd_w_cb(*this),
	m_s1_dtr_w_cb(*this),
	m_s1_rts_w_cb(*this),
	m_s2_txd_w_cb(*this),
	m_s2_dtr_w_cb(*this),
	m_s2_rts_w_cb(*this),
	m_kbdc_kbd_in_w_cb(*this),
	m_kbdc_mse_in_w_cb(*this)
{
	m_sysopt = lpc47m192_device::OPTS_SYSOPT_LOW;
}

void lpc47m192_device::device_start()
{
	lpc47m192_device::set_isa_device();

	lpc47m192_device::set_default_values();

	m_kbdc->keyboard_enable(!m_kbdc_kbd_in_w_cb.isunset());
	m_kbdc->mouse_enable(!m_kbdc_mse_in_w_cb.isunset());
}

void lpc47m192_device::device_reset()
{
	lpc47m192_device::set_default_values();
}

void lpc47m192_device::device_add_mconfig(machine_config &config)
{
	// FDC

	// parallel port controller
	ECP_LPT(config, m_parallel, 0);
	m_parallel->irq_handler().set(FUNC(lpc47m192_device::irq_parallel_w));
	m_parallel->hst2dev_pdata_bit_handler<0>().set([this] (int state) { m_par_h2dd0_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<1>().set([this] (int state) { m_par_h2dd1_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<2>().set([this] (int state) { m_par_h2dd2_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<3>().set([this] (int state) { m_par_h2dd3_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<4>().set([this] (int state) { m_par_h2dd4_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<5>().set([this] (int state) { m_par_h2dd5_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<6>().set([this] (int state) { m_par_h2dd6_w_cb(state); });
	m_parallel->hst2dev_pdata_bit_handler<7>().set([this] (int state) { m_par_h2dd7_w_cb(state); });
	m_parallel->hst2dev_cntl_bit_handler<0>().set([this] (int state) { m_par_h2dc0_w_cb(state); });
	m_parallel->hst2dev_cntl_bit_handler<1>().set([this] (int state) { m_par_h2dc1_w_cb(state); });
	m_parallel->hst2dev_cntl_bit_handler<2>().set([this] (int state) { m_par_h2dc2_w_cb(state); });
	m_parallel->hst2dev_cntl_bit_handler<3>().set([this] (int state) { m_par_h2dc3_w_cb(state); });
	m_parallel->hst2dev_cntl_bit_handler<4>().set([this] (int state) { m_par_h2dc4_w_cb(state); });

	// SERIAL1 port controller
	NS16550(config, m_serial[0], XTAL(1'843'200));
	m_serial[0]->out_int_callback().set(FUNC(lpc47m192_device::irq_serial1_w));
	m_serial[0]->out_tx_callback().set([this] (int state) { m_s1_txd_w_cb(state); });
	m_serial[0]->out_dtr_callback().set([this] (int state) { m_s1_dtr_w_cb(state); });
	m_serial[0]->out_rts_callback().set([this] (int state) { m_s1_rts_w_cb(state); });

	// SERIAL2 port controller
	NS16550(config, m_serial[1], XTAL(1'843'200));
	m_serial[1]->out_int_callback().set(FUNC(lpc47m192_device::irq_serial2_w));
	m_serial[1]->out_tx_callback().set([this] (int state) { m_s2_txd_w_cb(state); });
	m_serial[1]->out_dtr_callback().set([this] (int state) { m_s2_dtr_w_cb(state); });
	m_serial[1]->out_rts_callback().set([this] (int state) { m_s2_rts_w_cb(state); });

	// Keyboard port controller
	LPC47M192_KBDC(config, m_kbdc);
	m_kbdc->system_reset_callback().set(FUNC(lpc47m192_device::system_reset_w));
	m_kbdc->gate_a20_callback().set(FUNC(lpc47m192_device::gate_a20_w));
	m_kbdc->keybd_output_buffer_full_callback().set(FUNC(lpc47m192_device::irq_keyboard_w));
	m_kbdc->mouse_output_buffer_full_callback().set(FUNC(lpc47m192_device::irq_mouse_w));
	m_kbdc->keyboard_in_w_callback().set([this] (uint8_t data) { m_kbdc_kbd_in_w_cb(data); });
	m_kbdc->mouse_in_w_callback().set([this] (uint8_t data)  { m_kbdc_mse_in_w_cb(data); });

	// GAME port controller

	// MIDI port controller
}

void lpc47m192_device::connect_pport_centronics(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option)
{
	centronics_device &centronics(CENTRONICS(config, tag, slot_options_func, default_option));

	centronics.set_data_input_buffer("lpc47m192:parallel:dev2hst_pdata");

	lpc47m192_device::parallel_data0_callback().set(tag, FUNC(centronics_device::write_data0));
	lpc47m192_device::parallel_data1_callback().set(tag, FUNC(centronics_device::write_data1));
	lpc47m192_device::parallel_data2_callback().set(tag, FUNC(centronics_device::write_data2));
	lpc47m192_device::parallel_data3_callback().set(tag, FUNC(centronics_device::write_data3));
	lpc47m192_device::parallel_data4_callback().set(tag, FUNC(centronics_device::write_data4));
	lpc47m192_device::parallel_data5_callback().set(tag, FUNC(centronics_device::write_data5));
	lpc47m192_device::parallel_data6_callback().set(tag, FUNC(centronics_device::write_data6));
	lpc47m192_device::parallel_data7_callback().set(tag, FUNC(centronics_device::write_data7));

	centronics.fault_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_status_wbit<3>));
	centronics.select_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_status_wbit<4>));
	centronics.perror_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_status_wbit<5>));
	centronics.ack_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_status_wbit<6>));
	centronics.busy_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_status_wbit<7>));

	centronics.strobe_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_control_wbit<0>));
	centronics.autofd_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_control_wbit<1>));
	centronics.init_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_control_wbit<2>));
	centronics.select_in_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_control_wbit<3>));

	lpc47m192_device::parallel_control0_callback().set(tag, FUNC(centronics_device::write_strobe));
	lpc47m192_device::parallel_control1_callback().set(tag, FUNC(centronics_device::write_autofd));
	lpc47m192_device::parallel_control2_callback().set(tag, FUNC(centronics_device::write_init));
	lpc47m192_device::parallel_control3_callback().set(tag, FUNC(centronics_device::write_select));
	lpc47m192_device::parallel_control4_callback().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::parallel_control_irq_enable_w));
}

void lpc47m192_device::connect_coma_rs232(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option)
{
	rs232_port_device &serial1_rs232(RS232_PORT(config, tag, slot_options_func, default_option));

	serial1_rs232.rxd_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial1_rx_w));
	serial1_rs232.dcd_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial1_dcd_w));
	serial1_rs232.dsr_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial1_dsr_w));
	serial1_rs232.ri_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial1_ri_w));
	serial1_rs232.cts_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial1_cts_w));

	lpc47m192_device::serial1_txd_callback().set(tag, FUNC(rs232_port_device::write_txd));
	lpc47m192_device::serial1_dtr_callback().set(tag, FUNC(rs232_port_device::write_dtr));
	lpc47m192_device::serial1_rts_callback().set(tag, FUNC(rs232_port_device::write_rts));
}

void lpc47m192_device::connect_comb_rs232(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option)
{
	rs232_port_device &serial2_rs232(RS232_PORT(config, tag, slot_options_func, default_option));

	serial2_rs232.rxd_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial2_rx_w));
	serial2_rs232.dcd_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial2_dcd_w));
	serial2_rs232.dsr_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial2_dsr_w));
	serial2_rs232.ri_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial2_ri_w));
	serial2_rs232.cts_handler().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::serial2_cts_w));

	lpc47m192_device::serial2_txd_callback().set(tag, FUNC(rs232_port_device::write_txd));
	lpc47m192_device::serial2_dtr_callback().set(tag, FUNC(rs232_port_device::write_dtr));
	lpc47m192_device::serial2_rts_callback().set(tag, FUNC(rs232_port_device::write_rts));
}

void lpc47m192_device::connect_kbdc_keyboard(machine_config &config, const char* tag)
{
	m_kbd.set_tag(tag);
	AT_KEYB(config, m_kbd, pc_keyboard_device::KEYBOARD_TYPE::AT, 1);
	m_kbd->keypress().set(
		[this] (int state)
		{
			if(state)
			{
				while(uint8_t data = m_kbd->read())
				{
					m_kbdc->keyboard_out_w(data);
				}
			}
		}
	);
	lpc47m192_device::keyboard_in_w_callback().set(m_kbd, FUNC(at_keyboard_device::write));
}

void lpc47m192_device::connect_kbdc_mouse(machine_config &config, const char* tag)
{
	ps2_mouse_device &mse(PS2_MOUSE(config, tag));
	mse.out_w_callback().set(lpc47m192_device::tag(), FUNC(lpc47m192_device::mouse_out_w));
	lpc47m192_device::mouse_in_w_callback().set(tag, FUNC(ps2_mouse_device::in_w));
}

void lpc47m192_device::set_default_values()
{
	m_config_enabled = false;

	m_config_control = 0x00;
	m_selected_ldevice = 0x00;
	m_device_id = lpc47m192_device::DEVICE_ID;
	m_device_rev = lpc47m192_device::DEVICE_REV;
	m_power_control = 0x00;
	m_power_mgmt = 0x00;
	m_osc = lpc47m192_device::OSC_DEFAULT;
	if(m_sysopt == lpc47m192_device::OPTS_SYSOPT_HIGH)
		m_config_initial_ioaddr = lpc47m192_device::DEFAULT_CONFIG_IOADDR2;
	else
		m_config_initial_ioaddr = lpc47m192_device::DEFAULT_CONFIG_IOADDR1;
	m_config_ioaddr = m_config_initial_ioaddr;

	m_fdd_enabled = false;
	m_fdd_ioaddr = lpc47m192_device::DEFAULT_FDD_IOADDR;
	m_fdd_int_sel = lpc47m192_device::DEFAULT_FDD_INT;
	m_fdd_dma_chan_sel = lpc47m192_device::DEFAULT_FDD_DMA_CHAN;
	m_fdd_mode = 0x00;
	m_fdd_option = 0x00;
	m_fdd_type = 0x00;
	m_fdd_0 = 0x00;
	m_fdd_1 = 0x00;

	m_parallel_enabled = false;
	m_parallel_ioaddr = lpc47m192_device::DEFAULT_PARALLEL_IOADDR;
	m_parallel_int_sel = lpc47m192_device::DEFAULT_PARALLEL_INT;
	m_parallel_dma_chan_sel = lpc47m192_device::DEFAULT_PARALLEL_DMA_CHAN;
	m_parallel_mode1 = 0x00;
	m_parallel_mode2 = 0x00;

	m_serial1_enabled = false;
	m_serial1_ioaddr = lpc47m192_device::DEFAULT_SERIAL1_IOADDR;
	m_serial1_int_sel = lpc47m192_device::DEFAULT_SERIAL1_INT;
	m_serial1_mode = 0x00;

	m_serial2_enabled = false;
	m_serial2_ioaddr = lpc47m192_device::DEFAULT_SERIAL2_IOADDR;
	m_serial2_int_sel = lpc47m192_device::DEFAULT_SERIAL2_INT;
	m_serial2_mode = 0x00;
	m_serial2_ir_options = 0x00;
	m_serial2_ir_hdtimeout = 0x00;

	m_kbd_enabled = false;
	m_kbd_ioaddr = lpc47m192_device::DEFAULT_KBD_IOADDR;
	m_kbd_pri_int_sel = lpc47m192_device::DEFAULT_KBD_PRI_INT;
	m_kbd_sec_int_sel = lpc47m192_device::DEFAULT_KBD_SEC_INT;
	m_a20_ioaddr = lpc47m192_device::DEFAULT_A20_IOADDR;
	m_kbd_reset_and_a20_sel = 0x00;

	m_game_enabled = false;
	m_game_ioaddr = lpc47m192_device::DEFAULT_GAME_IOADDR;

	m_runtime_enabled = false;
	m_runtime_ioaddr = lpc47m192_device::DEFAULT_RUNTIME_IOADDR;
	m_runtime_clocki32 = 0x00;

	m_midi_enabled = false;
	m_midi_ioaddr = lpc47m192_device::DEFAULT_MIDI_IOADDR;
	m_midi_int_sel = lpc47m192_device::DEFAULT_MIDI_INT;

	std::fill(std::begin(m_runtime_block), std::end(m_runtime_block), 0);

	lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
}

void lpc47m192_device::remap(int space_id, offs_t start, offs_t end)
{
	if (space_id == AS_IO)
		m_isa->install_device(0x0000, 0xffff, *this, &lpc47m192_device::io_map);
}

void lpc47m192_device::io_map(address_map &map)
{
	if(m_config_ioaddr < lpc47m192_device::IOADDR_MIN)
		m_config_ioaddr = m_config_initial_ioaddr;

	map(m_config_ioaddr + 0, m_config_ioaddr + 0).w(FUNC(lpc47m192_device::index_w));
	map(m_config_ioaddr + 1, m_config_ioaddr + 1).rw(FUNC(lpc47m192_device::data_r), FUNC(lpc47m192_device::data_w));

	//if(m_fdd_enabled && m_fdd_ioaddr >= lpc47m192_device::IOADDR_MIN)

	if(m_parallel_enabled && m_parallel_ioaddr >= lpc47m192_device::IOADDR_MIN)
	{
		map(m_parallel_ioaddr + 0, m_parallel_ioaddr + 7).rw(m_parallel, FUNC(ecp_lpt_device::read), FUNC(ecp_lpt_device::write));
		map(m_parallel_ioaddr + 0x400, m_parallel_ioaddr + 0x403).lrw8(
			NAME(
				[] (offs_t offset) -> uint8_t
				{
					return 0x00;
				}
			),
			NAME(
				[] (offs_t offset, uint8_t data)
				{
					//
				}
			)
		);
	}

	if(m_serial1_enabled && m_serial1_ioaddr >= lpc47m192_device::IOADDR_MIN)
		map(m_serial1_ioaddr + 0, m_serial1_ioaddr + 7).rw(m_serial[0], FUNC(ns16450_device::ins8250_r), FUNC(ns16450_device::ins8250_w));

	if(m_serial2_enabled && m_serial2_ioaddr >= lpc47m192_device::IOADDR_MIN)
		map(m_serial2_ioaddr + 0, m_serial2_ioaddr + 7).rw(m_serial[1], FUNC(ns16450_device::ins8250_r), FUNC(ns16450_device::ins8250_w));

	if(m_kbd_enabled)
	{
		map(m_kbd_ioaddr + 0, m_kbd_ioaddr + 0).rw(m_kbdc, FUNC(lpc47m192_kbdc_device::data_60r), FUNC(lpc47m192_kbdc_device::data_60w));
		map(m_kbd_ioaddr + 4, m_kbd_ioaddr + 4).rw(m_kbdc, FUNC(lpc47m192_kbdc_device::status_64r), FUNC(lpc47m192_kbdc_device::command_64w));
	}

	map(m_a20_ioaddr + 0, m_a20_ioaddr + 0).rw(FUNC(lpc47m192_device::io92_r), FUNC(lpc47m192_device::io92_w));

	//if(m_game_enabled && m_game_ioaddr >= lpc47m192_device::IOADDR_MIN)

	if(m_runtime_enabled && m_runtime_ioaddr >= lpc47m192_device::IOADDR_MIN)
	{
		map(m_runtime_ioaddr + 0, m_runtime_ioaddr + (RUNTIME_BLOCK_SIZE - 1)).rw(FUNC(lpc47m192_device::runtime_r), FUNC(lpc47m192_device::runtime_w));
	}

	//if(m_midi_enabled && m_midi_ioaddr >= lpc47m192_device::IOADDR_MIN)
}

void lpc47m192_device::config_w(uint8_t data)
{
	if(data == lpc47m192_device::CONFIG_STATE_EXIT)
	{
		m_config_state = 0x00;
		m_config_enabled = false;

		if(m_pending_remap)
		{
			lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			m_pending_remap = false;
		}
	}
	else
	{
		m_config_state = data;
	}
}

void lpc47m192_device::index_w(uint8_t data)
{
	if (!m_config_enabled)
		m_config_enabled = (data == lpc47m192_device::CONFIG_STATE_ENTER);
	else
		lpc47m192_device::config_w(data);
}

uint8_t lpc47m192_device::config_data_fdd_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_FDD_ENABLED:
			return m_fdd_enabled;

		case lpc47m192_device::CONFIG_FDD_IOADDR_HIGH:
			return ((m_fdd_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_FDD_IOADDR_LOW:
			return ((m_fdd_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_FDD_INT_SEL:
			return m_fdd_int_sel;

		case lpc47m192_device::CONFIG_FDD_DMA_CHAN_SEL:
			return m_fdd_dma_chan_sel;

		case lpc47m192_device::CONFIG_FDD_MODE:
			return m_fdd_mode;

		case lpc47m192_device::CONFIG_FDD_OPTION:
			return m_fdd_option;

		case lpc47m192_device::CONFIG_FDD_TYPE:
			return m_fdd_type;

		case lpc47m192_device::CONFIG_FDD_0:
			return m_fdd_0;

		case lpc47m192_device::CONFIG_FDD_1:
			return m_fdd_1;
	}

	return 0x00;
}

void lpc47m192_device::config_data_fdd_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_FDD_ENABLED:
			m_fdd_enabled = data & 0x01;
			if(m_fdd_enabled)
				logerror("%s: WARNING: wants to activate fdd port @ 0x%04x to 0x%04x\n", tag(), m_fdd_ioaddr, m_fdd_ioaddr + lpc47m192_device::IOADDR_FDD_ALIGN);
			break;

		case lpc47m192_device::CONFIG_FDD_IOADDR_HIGH:
			m_fdd_ioaddr = (m_fdd_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_fdd_ioaddr &= (~(lpc47m192_device::IOADDR_FDD_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_FDD_IOADDR_LOW:
			m_fdd_ioaddr = (m_fdd_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_fdd_ioaddr &= (~(lpc47m192_device::IOADDR_FDD_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_FDD_INT_SEL:
			m_fdd_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_FDD_DMA_CHAN_SEL:
			m_fdd_dma_chan_sel = data;
			break;

		case lpc47m192_device::CONFIG_FDD_MODE:
			m_fdd_mode = data;
			break;

		case lpc47m192_device::CONFIG_FDD_OPTION:
			m_fdd_option = data;
			break;

		case lpc47m192_device::CONFIG_FDD_TYPE:
			m_fdd_type = data;
			break;

		case lpc47m192_device::CONFIG_FDD_0:
			m_fdd_0 = data;
			break;

		case lpc47m192_device::CONFIG_FDD_1:
			m_fdd_1 = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_parallel_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_PARALLEL_ENABLED:
			return m_parallel_enabled;

		case lpc47m192_device::CONFIG_PARALLEL_IOADDR_HIGH:
			return ((m_parallel_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_PARALLEL_IOADDR_LOW:
			return ((m_parallel_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_PARALLEL_INT_SEL:
			return m_parallel_int_sel;

		case lpc47m192_device::CONFIG_PARALLEL_DMA_CHAN_SEL:
			return m_parallel_dma_chan_sel;

		case lpc47m192_device::CONFIG_PARALLEL_MODE1:
			return m_parallel_mode1;

		case lpc47m192_device::CONFIG_PARALLEL_MODE2:
			return m_parallel_mode2;
	}

	return 0x00;
}

void lpc47m192_device::config_data_parallel_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_PARALLEL_ENABLED:
			m_parallel_enabled = data & 0x01;
			if(m_parallel_enabled)
			{
				logerror("%s: NOTE: activating parallel @ 0x%04x to 0x%04x\n", tag(), m_parallel_ioaddr, m_parallel_ioaddr + lpc47m192_device::IOADDR_PARALLEL_ALIGN);
				lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			}
			break;

		case lpc47m192_device::CONFIG_PARALLEL_IOADDR_HIGH:
			m_parallel_ioaddr = (m_parallel_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_parallel_ioaddr &= (~(lpc47m192_device::IOADDR_PARALLEL_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_PARALLEL_IOADDR_LOW:
			m_parallel_ioaddr = (m_parallel_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_parallel_ioaddr &= (~(lpc47m192_device::IOADDR_PARALLEL_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_PARALLEL_INT_SEL:
			m_parallel_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_PARALLEL_DMA_CHAN_SEL:
			m_parallel_dma_chan_sel = data;
			break;

		case lpc47m192_device::CONFIG_PARALLEL_MODE1:
			m_parallel_mode1 = data;
			break;

		case lpc47m192_device::CONFIG_PARALLEL_MODE2:
			m_parallel_mode2 = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_serial1_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_SERIAL1_ENABLED:
			return m_serial1_enabled;

		case lpc47m192_device::CONFIG_SERIAL1_IOADDR_HIGH:
			return ((m_serial1_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_SERIAL1_IOADDR_LOW:
			return ((m_serial1_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_SERIAL1_INT_SEL:
			return m_serial1_int_sel;

		case lpc47m192_device::CONFIG_SERIAL1_MODE:
			return m_serial1_mode;
	}

	return 0x00;
}

void lpc47m192_device::config_data_serial1_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_SERIAL1_ENABLED:
			m_serial1_enabled = data & 0x01;
			if(m_serial1_enabled)
			{
				logerror("%s: NOTE: activating serial1 port @ 0x%04x to 0x%04x\n", tag(), m_serial1_ioaddr, m_serial1_ioaddr + lpc47m192_device::IOADDR_SERIAL1_ALIGN);
				lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			}
			break;

		case lpc47m192_device::CONFIG_SERIAL1_IOADDR_HIGH:
			m_serial1_ioaddr = (m_serial1_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_serial1_ioaddr &= (~(lpc47m192_device::IOADDR_SERIAL1_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_SERIAL1_IOADDR_LOW:
			m_serial1_ioaddr = (m_serial1_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_serial1_ioaddr &= (~(lpc47m192_device::IOADDR_SERIAL1_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_SERIAL1_INT_SEL:
			m_serial1_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_SERIAL1_MODE:
			m_serial1_mode = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_serial2_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_SERIAL2_ENABLED:
			return m_serial2_enabled;

		case lpc47m192_device::CONFIG_SERIAL2_IOADDR_HIGH:
			return ((m_serial2_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_SERIAL2_IOADDR_LOW:
			return ((m_serial2_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_SERIAL2_INT_SEL:
			return m_serial2_int_sel;

		case lpc47m192_device::CONFIG_SERIAL2_MODE:
			return m_serial2_mode;

		case lpc47m192_device::CONFIG_SERIAL2_IR_OPTIONS:
			return m_serial2_ir_options;

		case lpc47m192_device::CONFIG_SERIAL2_IR_HDTIMEOUT:
			return m_serial2_ir_hdtimeout;
	}

	return 0x00;
}

void lpc47m192_device::config_data_serial2_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_SERIAL2_ENABLED:
			m_serial2_enabled = data & 0x01;
			if(m_serial2_enabled)
			{
				logerror("%s: NOTE: activating serial2 port @ 0x%04x to 0x%04x\n", tag(), m_serial2_ioaddr, m_serial2_ioaddr + lpc47m192_device::IOADDR_SERIAL2_ALIGN);
				lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			}
			break;

		case lpc47m192_device::CONFIG_SERIAL2_IOADDR_HIGH:
			m_serial2_ioaddr = (m_serial2_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_serial2_ioaddr &= (~(lpc47m192_device::IOADDR_SERIAL2_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_SERIAL2_IOADDR_LOW:
			m_serial2_ioaddr = (m_serial2_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_serial2_ioaddr &= (~(lpc47m192_device::IOADDR_SERIAL2_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_SERIAL2_INT_SEL:
			m_serial2_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_SERIAL2_MODE:
			m_serial2_mode = data;
			break;

		case lpc47m192_device::CONFIG_SERIAL2_IR_OPTIONS:
			m_serial2_ir_options = data;
			break;

		case lpc47m192_device::CONFIG_SERIAL2_IR_HDTIMEOUT:
			m_serial2_ir_hdtimeout = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_kbd_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_KBD_ENABLED:
			return m_kbd_enabled;

		case lpc47m192_device::CONFIG_KBD_PRI_INT_SEL:
			return m_kbd_pri_int_sel;

		case lpc47m192_device::CONFIG_KBD_SEC_INT_SEL:
			return m_kbd_sec_int_sel;

		case lpc47m192_device::CONFIG_KBD_RESET_AND_A20_SEL:
			return m_kbd_reset_and_a20_sel;
	}

	return 0x00;
}

void lpc47m192_device::config_data_kbd_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_KBD_ENABLED:
			m_kbd_enabled = data & 0x01;
			if(m_kbd_enabled)
			{
				logerror("%s: NOTE: activating kbd port @ 0x60, 0x64\n", tag());
				lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			}
			break;

		case lpc47m192_device::CONFIG_KBD_PRI_INT_SEL:
			m_kbd_pri_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_KBD_SEC_INT_SEL:
			m_kbd_sec_int_sel = data;
			break;

		case lpc47m192_device::CONFIG_KBD_RESET_AND_A20_SEL:
			m_kbd_reset_and_a20_sel = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_game_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_GAME_ENABLED:
			return m_game_enabled;

		case lpc47m192_device::CONFIG_GAME_IOADDR_HIGH:
			return ((m_game_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_GAME_IOADDR_LOW:
			return ((m_game_ioaddr >> 0x00) & 0xff);
	}

	return 0x00;
}

void lpc47m192_device::config_data_game_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_GAME_ENABLED:
			m_game_enabled = data & 0x01;
			if(m_game_enabled)
				logerror("%s: WARNING: wants to activate game port @ 0x%04x to 0x%04x\n", tag(), m_game_ioaddr, m_game_ioaddr + lpc47m192_device::IOADDR_MIDI_ALIGN);
			break;

		case lpc47m192_device::CONFIG_GAME_IOADDR_HIGH:
			m_game_ioaddr = (m_game_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_game_ioaddr &= (~(lpc47m192_device::IOADDR_MIDI_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_GAME_IOADDR_LOW:
			m_game_ioaddr = (m_game_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_game_ioaddr &= (~(lpc47m192_device::IOADDR_MIDI_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;
	}
}

uint8_t lpc47m192_device::config_data_runtime_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_RUNTIME_ENABLED:
			return m_midi_enabled;

		case lpc47m192_device::CONFIG_RUNTIME_IOADDR_HIGH:
			return ((m_runtime_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_RUNTIME_IOADDR_LOW:
			return ((m_runtime_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_RUNTIME_CLOCKI32:
			return m_runtime_clocki32;
	}
	return 0x00;
}

void lpc47m192_device::config_data_runtime_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_RUNTIME_ENABLED:
			m_runtime_enabled = data & 0x01;
			if(m_runtime_enabled)
			{
				logerror("%s: NOTE: activating runtime (pme, leds, fan control etc...) port @ 0x%04x to 0x%04x\n", tag(), m_runtime_ioaddr, m_runtime_ioaddr + lpc47m192_device::IOADDR_RUNTIME_ALIGN);
				lpc47m192_device::remap(AS_IO, 0x0000, 0xffff);
			}
			break;

		case lpc47m192_device::CONFIG_RUNTIME_IOADDR_HIGH:
			m_runtime_ioaddr = (m_runtime_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_runtime_ioaddr &= (~(lpc47m192_device::IOADDR_RUNTIME_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_RUNTIME_IOADDR_LOW:
			m_runtime_ioaddr = (m_runtime_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_runtime_ioaddr &= (~(lpc47m192_device::IOADDR_RUNTIME_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_RUNTIME_CLOCKI32:
			m_runtime_clocki32 = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_midi_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_MIDI_ENABLED:
			return m_midi_enabled;

		case lpc47m192_device::CONFIG_MIDI_IOADDR_HIGH:
			return ((m_midi_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_MIDI_IOADDR_LOW:
			return ((m_midi_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_MIDI_INT_SEL:
			return m_midi_int_sel;
	}

	return 0x00;
}

void lpc47m192_device::config_data_midi_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_MIDI_ENABLED:
			m_midi_enabled = data & 0x01;
			if(m_midi_enabled)
				logerror("%s: WARNING: wants to activate midi port @ 0x%04x to 0x%04x\n", tag(), m_midi_ioaddr, m_midi_ioaddr + lpc47m192_device::IOADDR_MIDI_ALIGN);
			break;

		case lpc47m192_device::CONFIG_MIDI_IOADDR_HIGH:
			m_midi_ioaddr = (m_midi_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_midi_ioaddr &= (~(lpc47m192_device::IOADDR_MIDI_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_MIDI_IOADDR_LOW:
			m_midi_ioaddr = (m_midi_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_midi_ioaddr &= (~(lpc47m192_device::IOADDR_MIDI_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_MIDI_INT_SEL:
			m_midi_int_sel = data;
			break;
	}
}

uint8_t lpc47m192_device::config_data_ldevice_r()
{
	switch(m_selected_ldevice)
	{
		case LDEVICE_FDD:
			return lpc47m192_device::config_data_fdd_r();

		case LDEVICE_parallel:
			return lpc47m192_device::config_data_parallel_r();

		case LDEVICE_SERIAL1:
			return lpc47m192_device::config_data_serial1_r();

		case LDEVICE_SERIAL2:
			return lpc47m192_device::config_data_serial2_r();

		case LDEVICE_KBD:
			return lpc47m192_device::config_data_kbd_r();

		case LDEVICE_GAME:
			return lpc47m192_device::config_data_game_r();

		case LDEVICE_RUNTIME:
			return lpc47m192_device::config_data_runtime_r();

		case LDEVICE_MIDI:
			return lpc47m192_device::config_data_midi_r();

		default:
			return 0x00;
	}
}

void lpc47m192_device::config_data_ldevice_w(uint8_t data)
{
	switch(m_selected_ldevice)
	{
		case LDEVICE_FDD:
			lpc47m192_device::config_data_fdd_w(data);
			break;

		case LDEVICE_parallel:
			lpc47m192_device::config_data_parallel_w(data);
			break;

		case LDEVICE_SERIAL1:
			lpc47m192_device::config_data_serial1_w(data);
			break;

		case LDEVICE_SERIAL2:
			lpc47m192_device::config_data_serial2_w(data);
			break;

		case LDEVICE_KBD:
			lpc47m192_device::config_data_kbd_w(data);
			break;

		case LDEVICE_GAME:
			lpc47m192_device::config_data_game_w(data);
			break;

		case LDEVICE_RUNTIME:
			lpc47m192_device::config_data_runtime_w(data);
			break;

		case LDEVICE_MIDI:
			lpc47m192_device::config_data_midi_w(data);
			break;

		default:
			break;
	}
}

uint8_t lpc47m192_device::config_data_r()
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_GLOBAL_CONFIG_CONTROL:
			return m_config_control; // write only but adding read anyway.

		case lpc47m192_device::CONFIG_GLOBAL_LDEVICE_SEL:
			return m_selected_ldevice;

		case lpc47m192_device::CONFIG_GLOBAL_DEVICE_ID:
			return m_device_id;

		case lpc47m192_device::CONFIG_GLOBAL_DEVICE_REV:
			return m_device_rev;

		case lpc47m192_device::CONFIG_GLOBAL_POWER_CONTROL:
			return m_power_control;

		case lpc47m192_device::CONFIG_GLOBAL_POWER_MGMT:
			return m_power_mgmt;

		case lpc47m192_device::CONFIG_GLOBAL_OSC:
			return m_osc;

		case lpc47m192_device::CONFIG_GLOBAL_IOADDR_LOW:
			return ((m_config_ioaddr >> 0x00) & 0xff);

		case lpc47m192_device::CONFIG_GLOBAL_IOADDR_HIGH:
			return ((m_config_ioaddr >> 0x08) & 0xff);

		case lpc47m192_device::CONFIG_GLOBAL_TEST6:
			return 0x00;
		case lpc47m192_device::CONFIG_GLOBAL_TEST5:
			return 0x00;
		case lpc47m192_device::CONFIG_GLOBAL_TEST1:
			return 0x00;
		case lpc47m192_device::CONFIG_GLOBAL_TEST2:
			return 0x00;
		case lpc47m192_device::CONFIG_GLOBAL_TEST3:
			return 0x00;

		default:
			return lpc47m192_device::config_data_ldevice_r();
	}
}

void lpc47m192_device::config_data_w(uint8_t data)
{
	switch(m_config_state)
	{
		case lpc47m192_device::CONFIG_GLOBAL_CONFIG_CONTROL:
			m_config_control = data;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_LDEVICE_SEL:
			m_selected_ldevice = data;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_DEVICE_ID:
			// nop
			break;

		case lpc47m192_device::CONFIG_GLOBAL_DEVICE_REV:
			// nop
			break;

		case lpc47m192_device::CONFIG_GLOBAL_POWER_CONTROL:
			m_power_control = data & lpc47m192_device::PCONTROL_MASK;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_POWER_MGMT:
			m_power_mgmt = data & lpc47m192_device::PMGMT_MASK;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_OSC:
			m_osc = data & lpc47m192_device::OSC_MASK;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_IOADDR_LOW:
			m_config_ioaddr = (m_config_ioaddr & 0xff00) | ((uint16_t)data << 0x00);
			m_config_ioaddr &= (~(lpc47m192_device::IOADDR_CONFIG_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_IOADDR_HIGH:
			m_config_ioaddr = (m_config_ioaddr & 0x00ff) | ((uint16_t)data << 0x08);
			m_config_ioaddr &= (~(lpc47m192_device::IOADDR_CONFIG_ALIGN - 1)) & lpc47m192_device::IOADDR_MASK;
			m_pending_remap = true;
			break;

		case lpc47m192_device::CONFIG_GLOBAL_TEST6:
			break;
		case lpc47m192_device::CONFIG_GLOBAL_TEST5:
			break;
		case lpc47m192_device::CONFIG_GLOBAL_TEST1:
			break;
		case lpc47m192_device::CONFIG_GLOBAL_TEST2:
			break;
		case lpc47m192_device::CONFIG_GLOBAL_TEST3:
			break;

		default:
			lpc47m192_device::config_data_ldevice_w(data);
			break;
	}
}

uint8_t lpc47m192_device::data_r()
{
	if (m_config_enabled)
		return lpc47m192_device::config_data_r();
	else
		return 0x00;
}

void lpc47m192_device::data_w(uint8_t data)
{
	if (m_config_enabled)
		lpc47m192_device::config_data_w(data);
}

uint8_t lpc47m192_device::io92_r()
{
	return REG92_DEFAULT_VALUE;
}

void lpc47m192_device::io92_w(uint8_t data)
{
	if(data & lpc47m192_device::REG92_ALT_A20_HIGH)
		lpc47m192_device::gate_a20_w(ASSERT_LINE);
	else
		lpc47m192_device::gate_a20_w(CLEAR_LINE);

	if(data & lpc47m192_device::REG92_ALT_RESET_HIGH)
	{
		lpc47m192_device::system_reset_w(ASSERT_LINE);

		popmessage("Software reset fired");

		machine().schedule_soft_reset();
	}
	else
	{
		lpc47m192_device::system_reset_w(CLEAR_LINE);
	}
}

void lpc47m192_device::irq_w(uint8_t irqno, int state)
{
	switch(irqno)
	{
		case 1:
			m_irq1_cb(state);
			break;

		case 2:
			m_irq2_cb(state);
			break;

		case 3:
			m_irq3_cb(state);
			break;

		case 4:
			m_irq4_cb(state);
			break;

		case 5:
			m_irq5_cb(state);
			break;

		case 6:
			m_irq6_cb(state);
			break;

		case 7:
			m_irq7_cb(state);
			break;

		case 8:
			m_irq9_cb(state);
			break;

		case 9:
			m_irq9_cb(state);
			break;

		case 10:
			m_irq10_cb(state);
			break;

		case 11:
			m_irq11_cb(state);
			break;

		case 12:
			m_irq12_cb(state);
			break;

		case 13:
			m_irq13_cb(state);
			break;

		case 14:
			m_irq14_cb(state);
			break;

		case 15:
			m_irq15_cb(state);
			break;

		default:
			break;
	}
}

void lpc47m192_device::irq_parallel_w(int state)
{
	if(m_parallel_int_sel > 0)
		lpc47m192_device::irq_w(m_parallel_int_sel, state);
}

void lpc47m192_device::irq_serial1_w(int state)
{
	if(m_serial1_int_sel > 0)
		lpc47m192_device::irq_w(m_serial1_int_sel, state);
	else if(m_serial1_mode & SERIAL1_MODE_SHARE_IRQ && m_serial2_int_sel > 0)
		lpc47m192_device::irq_w(m_serial2_int_sel, state);
}

void lpc47m192_device::irq_serial2_w(int state)
{
	if(m_serial2_int_sel > 0)
		lpc47m192_device::irq_w(m_serial2_int_sel, state);
	else if(m_serial1_mode & SERIAL1_MODE_SHARE_IRQ && m_serial1_int_sel > 0)
		lpc47m192_device::irq_w(m_serial1_int_sel, state);
}

void lpc47m192_device::irq_keyboard_w(int state)
{
	if(m_kbd_pri_int_sel > 0)
		lpc47m192_device::irq_w(m_kbd_pri_int_sel, state);
}

void lpc47m192_device::irq_mouse_w(int state)
{
	if(m_kbd_pri_int_sel > 0)
		lpc47m192_device::irq_w(m_kbd_pri_int_sel, state);
	/*
	if(m_kbd_sec_int_sel > 0)
		lpc47m192_device::irq_w(m_kbd_sec_int_sel, state);
	*/
}

void lpc47m192_device::system_reset_w(int state)
{
	m_system_reset_cb(state);
}

void lpc47m192_device::gate_a20_w(int state)
{
	m_gate_a20_cb(state);
}

uint8_t lpc47m192_device::runtime_r(offs_t offset)
{
	switch(offset)
	{
		default:
			return m_runtime_block[offset & (RUNTIME_BLOCK_SIZE - 1)];
	}
}

void lpc47m192_device::runtime_w(offs_t offset, uint8_t data)
{
	switch(offset)
	{
		default:
			m_runtime_block[offset & (RUNTIME_BLOCK_SIZE - 1)] = data;
			break;
	}
}
