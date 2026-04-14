// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_LPC47M192_H
#define MAME_WEBTV_LPC47M192_H

#pragma once

#include "bus/isa/isa.h"
#include "ecp_lpt.h"
#include "machine/ins8250.h"
#include "lpc47m192_kbdc.h"

class lpc47m192_device : public device_t, public device_isa16_card_interface
{

public:

	using sopts_func_t = std::function<void(device_slot_interface&)>;

	static constexpr uint8_t DEVICE_ID  = 0x60; // MSNTV2 only suparallels a SuperIO with 0x51 or 0x60 (checked at ~0x829D14 in v1.387 bios). 0x60=retail, 0x51=codenamed "rita"?
	static constexpr uint8_t DEVICE_REV = 0x02;
	
	static constexpr uint8_t RUNTIME_BLOCK_SIZE = 0x80;

	static constexpr uint16_t IOADDR_MIN           = 0x0100;
	static constexpr uint16_t IOADDR_MASK          = 0x0fff;
	static constexpr uint8_t IOADDR_CONFIG_ALIGN   = 0x02;
	static constexpr uint8_t IOADDR_FDD_ALIGN      = 0x08;
	static constexpr uint8_t IOADDR_PARALLEL_ALIGN    = 0x04;
	static constexpr uint8_t IOADDR_SERIAL1_ALIGN  = 0x08;
	static constexpr uint8_t IOADDR_SERIAL2_ALIGN  = 0x08;
	static constexpr uint8_t IOADDR_GAME_ALIGN     = 0x01;
	static constexpr uint8_t IOADDR_RUNTIME_ALIGN  = lpc47m192_device::RUNTIME_BLOCK_SIZE;
	static constexpr uint8_t IOADDR_MIDI_ALIGN     = 0x02;

	static constexpr uint8_t OPTS_SYSOPT_LOW     = 0;
	static constexpr uint8_t OPTS_SYSOPT_HIGH    = 1;

	static constexpr uint16_t DEFAULT_CONFIG_IOADDR1 = 0x002e; // Most common, sysopt pin is 0
	static constexpr uint16_t DEFAULT_CONFIG_IOADDR2 = 0x004e; // Sysopt pin is 1
	static constexpr uint16_t DEFAULT_FDD_IOADDR     = 0x03f0;
	static constexpr uint16_t DEFAULT_PARALLEL_IOADDR   = 0x0000;
	static constexpr uint16_t DEFAULT_SERIAL1_IOADDR = 0x0000;
	static constexpr uint16_t DEFAULT_SERIAL2_IOADDR = 0x0000;
	static constexpr uint16_t DEFAULT_KBD_IOADDR     = 0x0060;
	static constexpr uint16_t DEFAULT_A20_IOADDR     = 0x0092;
	static constexpr uint16_t DEFAULT_GAME_IOADDR    = 0x0000;
	static constexpr uint16_t DEFAULT_RUNTIME_IOADDR = 0x0000;
	static constexpr uint16_t DEFAULT_MIDI_IOADDR    = 0x0330;

	static constexpr uint16_t DEFAULT_FDD_INT     = 0x0006;
	static constexpr uint16_t DEFAULT_PARALLEL_INT   = 0x0000;
	static constexpr uint16_t DEFAULT_SERIAL1_INT = 0x0000;
	static constexpr uint16_t DEFAULT_SERIAL2_INT = 0x0000;
	static constexpr uint16_t DEFAULT_KBD_PRI_INT = 0x0000;
	static constexpr uint16_t DEFAULT_KBD_SEC_INT = 0x0000;
	static constexpr uint16_t DEFAULT_MIDI_INT    = 0x0000;

	static constexpr uint16_t DEFAULT_FDD_DMA_CHAN   = 0x0002;
	static constexpr uint16_t DEFAULT_PARALLEL_DMA_CHAN = 0x0004;

	static constexpr uint8_t CONFIG_STATE_ENTER           = 0x55;
	static constexpr uint8_t CONFIG_STATE_EXIT            = 0xaa;
	// Global Config
	static constexpr uint8_t CONFIG_GLOBAL_CONFIG_CONTROL = 0x02;
	static constexpr uint8_t CONFIG_GLOBAL_LDEVICE_SEL    = 0x07;
	static constexpr uint8_t CONFIG_GLOBAL_DEVICE_ID      = 0x20;
	static constexpr uint8_t CONFIG_GLOBAL_DEVICE_REV     = 0x21;
	static constexpr uint8_t CONFIG_GLOBAL_POWER_CONTROL  = 0x22;
	static constexpr uint8_t CONFIG_GLOBAL_POWER_MGMT     = 0x23;
	static constexpr uint8_t CONFIG_GLOBAL_OSC            = 0x24;
	static constexpr uint8_t CONFIG_GLOBAL_IOADDR_LOW     = 0x26;
	static constexpr uint8_t CONFIG_GLOBAL_IOADDR_HIGH    = 0x27;
	static constexpr uint8_t CONFIG_GLOBAL_TEST6          = 0x2a;
	static constexpr uint8_t CONFIG_GLOBAL_TEST4          = 0x2b;
	static constexpr uint8_t CONFIG_GLOBAL_TEST5          = 0x2c;
	static constexpr uint8_t CONFIG_GLOBAL_TEST1          = 0x2d;
	static constexpr uint8_t CONFIG_GLOBAL_TEST2          = 0x2e;
	static constexpr uint8_t CONFIG_GLOBAL_TEST3          = 0x2f;
	// Logical Device 0 (Floppy Disk Drive)
	static constexpr uint8_t CONFIG_FDD_ENABLED           = 0x30;
	static constexpr uint8_t CONFIG_FDD_IOADDR_HIGH       = 0x60;
	static constexpr uint8_t CONFIG_FDD_IOADDR_LOW        = 0x61;
	static constexpr uint8_t CONFIG_FDD_INT_SEL           = 0x70;
	static constexpr uint8_t CONFIG_FDD_DMA_CHAN_SEL      = 0x74;
	static constexpr uint8_t CONFIG_FDD_MODE              = 0xf0;
	static constexpr uint8_t CONFIG_FDD_OPTION            = 0xf1;
	static constexpr uint8_t CONFIG_FDD_TYPE              = 0xf2;
	static constexpr uint8_t CONFIG_FDD_0                 = 0xf4;
	static constexpr uint8_t CONFIG_FDD_1                 = 0xf5;
	// Logical Device 3 (Parallel Port)
	static constexpr uint8_t CONFIG_PARALLEL_ENABLED         = 0x30;
	static constexpr uint8_t CONFIG_PARALLEL_IOADDR_HIGH     = 0x60;
	static constexpr uint8_t CONFIG_PARALLEL_IOADDR_LOW      = 0x61;
	static constexpr uint8_t CONFIG_PARALLEL_INT_SEL         = 0x70;
	static constexpr uint8_t CONFIG_PARALLEL_DMA_CHAN_SEL    = 0x74;
	static constexpr uint8_t CONFIG_PARALLEL_MODE1           = 0xf0;
	static constexpr uint8_t CONFIG_PARALLEL_MODE2           = 0xf1;
	// Logical Device 4 (Serial Port 1/coma/COM1/ttyS0)
	static constexpr uint8_t CONFIG_SERIAL1_ENABLED       = 0x30;
	static constexpr uint8_t CONFIG_SERIAL1_IOADDR_HIGH   = 0x60;
	static constexpr uint8_t CONFIG_SERIAL1_IOADDR_LOW    = 0x61;
	static constexpr uint8_t CONFIG_SERIAL1_INT_SEL       = 0x70;
	static constexpr uint8_t CONFIG_SERIAL1_MODE          = 0xf0;
	// Logical Device 5 (Serial Port 2/coma/COM2/ttyS1)
	static constexpr uint8_t CONFIG_SERIAL2_ENABLED       = 0x30;
	static constexpr uint8_t CONFIG_SERIAL2_IOADDR_HIGH   = 0x60;
	static constexpr uint8_t CONFIG_SERIAL2_IOADDR_LOW    = 0x61;
	static constexpr uint8_t CONFIG_SERIAL2_INT_SEL       = 0x70;
	static constexpr uint8_t CONFIG_SERIAL2_MODE          = 0xf0;
	static constexpr uint8_t CONFIG_SERIAL2_IR_OPTIONS    = 0xf1;
	static constexpr uint8_t CONFIG_SERIAL2_IR_HDTIMEOUT  = 0xf2;
	// Logical Device 7 (Keyboard)
	static constexpr uint8_t CONFIG_KBD_ENABLED           = 0x30;
	static constexpr uint8_t CONFIG_KBD_PRI_INT_SEL       = 0x70;
	static constexpr uint8_t CONFIG_KBD_SEC_INT_SEL       = 0x72;
	static constexpr uint8_t CONFIG_KBD_RESET_AND_A20_SEL = 0xf0;
	// Logical Device 9 (Game Port)
	static constexpr uint8_t CONFIG_GAME_ENABLED          = 0x30;
	static constexpr uint8_t CONFIG_GAME_IOADDR_HIGH      = 0x60;
	static constexpr uint8_t CONFIG_GAME_IOADDR_LOW       = 0x61;
	// Logical Device A (Runtime Registers)
	static constexpr uint8_t CONFIG_RUNTIME_ENABLED       = 0x30;
	static constexpr uint8_t CONFIG_RUNTIME_IOADDR_HIGH   = 0x60;
	static constexpr uint8_t CONFIG_RUNTIME_IOADDR_LOW    = 0x61;
	static constexpr uint8_t CONFIG_RUNTIME_CLOCKI32      = 0xf0;
	// Logical Device B (MIDI/MPU-401)
	static constexpr uint8_t CONFIG_MIDI_ENABLED          = 0x30;
	static constexpr uint8_t CONFIG_MIDI_IOADDR_HIGH      = 0x60;
	static constexpr uint8_t CONFIG_MIDI_IOADDR_LOW       = 0x61;
	static constexpr uint8_t CONFIG_MIDI_INT_SEL          = 0x70;

	static constexpr uint8_t LDEVICE_FDD     = 0x00;
	static constexpr uint8_t LDEVICE_parallel   = 0x03;
	static constexpr uint8_t LDEVICE_SERIAL1 = 0x04;
	static constexpr uint8_t LDEVICE_SERIAL2 = 0x05;
	static constexpr uint8_t LDEVICE_KBD     = 0x07;
	static constexpr uint8_t LDEVICE_GAME    = 0x09;
	static constexpr uint8_t LDEVICE_RUNTIME = 0x0a;
	static constexpr uint8_t LDEVICE_MIDI    = 0x0b;

	static constexpr uint8_t PCONTROL_FDD_POWER     = 1 << 0;
	static constexpr uint8_t PCONTROL_GAME_POWER    = 1 << 2;
	static constexpr uint8_t PCONTROL_PARALLEL_POWER   = 1 << 3;
	static constexpr uint8_t PCONTROL_SERIAL1_POWER = 1 << 4;
	static constexpr uint8_t PCONTROL_SERIAL2_POWER = 1 << 5;
	static constexpr uint8_t PCONTROL_MIDI_POWER    = 1 << 6;
	static constexpr uint8_t PCONTROL_MASK          = 0x7d;

	static constexpr uint8_t PMGMT_PARALLEL_ENABLED   = 1 << 3;
	static constexpr uint8_t PMGMT_SERIAL1_ENABLED = 1 << 4;
	static constexpr uint8_t PMGMT_SERIAL2_ENABLED = 1 << 5;
	static constexpr uint8_t PMGMT_MIDI_ENABLED    = 1 << 6;
	static constexpr uint8_t PMGMT_MASK            = 0x78;

	static constexpr uint8_t OSC_PLL_ENABLED           = 1 << 1;
	static constexpr uint8_t OSC_OFF_AND_BRG_CLOCK_OFF = 3 << 2; // Documentation is obsure about the OSC and BRG clock bits, assuming 0b11 is off otherwise on
	static constexpr uint8_t OSC_OFF_AND_BRG_CLOCK_ON  = 1 << 2;
	static constexpr uint8_t OSC_16BIT_ADDRESS_QUAL    = 1 << 6; // Otherwise using 12-bit address qualification
	static constexpr uint8_t OSC_MASK                  = 0x4e;
	static constexpr uint8_t OSC_DEFAULT               = lpc47m192_device::OSC_OFF_AND_BRG_CLOCK_ON | lpc47m192_device::OSC_16BIT_ADDRESS_QUAL;

	static constexpr uint8_t SERIAL1_MODE_MIDI_ENABLED = 1 << 0;
	static constexpr uint8_t SERIAL1_MODE_HIGH_SPEED   = 1 << 1;
	static constexpr uint8_t SERIAL1_MODE_SHARE_IRQ    = 1 << 7;

	static constexpr uint8_t SERIAL2_MODE_MIDI_ENABLED = 1 << 0;
	static constexpr uint8_t SERIAL2_MODE_HIGH_SPEED   = 1 << 1;
	static constexpr uint8_t SERIAL2_MODE_TXD2_MODE    = 1 << 5;

	static constexpr uint8_t RUNTIME_PME_STS_REG              = 0x00;
	static constexpr uint8_t RUNTIME_PME_EN_REG               = 0x02;
	static constexpr uint8_t RUNTIME_PME_STS1_REG             = 0x04;
	static constexpr uint8_t RUNTIME_PME_STS2_REG             = 0x05;
	static constexpr uint8_t RUNTIME_PME_STS3_REG             = 0x06;
	static constexpr uint8_t RUNTIME_PME_STS4_REG             = 0x07;
	static constexpr uint8_t RUNTIME_PME_STS5_REG             = 0x08;
	static constexpr uint8_t RUNTIME_PME_EN1_REG              = 0x0a;
	static constexpr uint8_t RUNTIME_PME_EN2_REG              = 0x0b;
	static constexpr uint8_t RUNTIME_PME_EN3_REG              = 0x0c;
	static constexpr uint8_t RUNTIME_PME_EN4_REG              = 0x0d;
	static constexpr uint8_t RUNTIME_PME_EN5_REG              = 0x0e;
	static constexpr uint8_t RUNTIME_SMI_STS1_REG             = 0x10;
	static constexpr uint8_t RUNTIME_SMI_STS2_REG             = 0x11;
	static constexpr uint8_t RUNTIME_SMI_STS3_REG             = 0x12;
	static constexpr uint8_t RUNTIME_SMI_STS4_REG             = 0x13;
	static constexpr uint8_t RUNTIME_SMI_STS5_REG             = 0x14;
	static constexpr uint8_t RUNTIME_SMI_EN1_REG              = 0x16;
	static constexpr uint8_t RUNTIME_SMI_EN2_REG              = 0x17;
	static constexpr uint8_t RUNTIME_SMI_EN3_REG              = 0x18;
	static constexpr uint8_t RUNTIME_SMI_EN4_REG              = 0x19;
	static constexpr uint8_t RUNTIME_SMI_EN5_REG              = 0x1a;
	static constexpr uint8_t RUNTIME_MSC_STS_REG              = 0x1c;
	static constexpr uint8_t RUNTIME_FORCE_DISK_CHANGE_REG    = 0x1e;
	static constexpr uint8_t RUNTIME_FDD_RATE_REG             = 0x1f;
	static constexpr uint8_t RUNTIME_SERIAL1_FIFO_CONTROL_REG = 0x20;
	static constexpr uint8_t RUNTIME_SERIAL2_FIFO_CONTROL_REG = 0x21;
	static constexpr uint8_t RUNTIME_DEVICE_DISABLE_REG       = 0x22;
	static constexpr uint8_t RUNTIME_GP10_REG                 = 0x23;
	static constexpr uint8_t RUNTIME_GP11_REG                 = 0x24;
	static constexpr uint8_t RUNTIME_GP12_REG                 = 0x25;
	static constexpr uint8_t RUNTIME_GP13_REG                 = 0x26;
	static constexpr uint8_t RUNTIME_GP14_REG                 = 0x27;
	static constexpr uint8_t RUNTIME_GP15_REG                 = 0x28;
	static constexpr uint8_t RUNTIME_GP16_REG                 = 0x29;
	static constexpr uint8_t RUNTIME_GP17_REG                 = 0x2a;
	static constexpr uint8_t RUNTIME_GP20_REG                 = 0x2b;
	static constexpr uint8_t RUNTIME_GP21_REG                 = 0x2c;
	static constexpr uint8_t RUNTIME_GP22_REG                 = 0x2d;
	static constexpr uint8_t RUNTIME_GP24_REG                 = 0x2f;
	static constexpr uint8_t RUNTIME_GP25_REG                 = 0x30;
	static constexpr uint8_t RUNTIME_GP26_REG                 = 0x31;
	static constexpr uint8_t RUNTIME_GP27_REG                 = 0x32;
	static constexpr uint8_t RUNTIME_GP30_REG                 = 0x33;
	static constexpr uint8_t RUNTIME_GP31_REG                 = 0x34;
	static constexpr uint8_t RUNTIME_GP32_REG                 = 0x35;
	static constexpr uint8_t RUNTIME_GP33_REG                 = 0x36;
	static constexpr uint8_t RUNTIME_GP34_REG                 = 0x37; //
	static constexpr uint8_t RUNTIME_GP35_REG                 = 0x38;
	static constexpr uint8_t RUNTIME_GP36_REG                 = 0x39;
	static constexpr uint8_t RUNTIME_GP37_REG                 = 0x3a;
	static constexpr uint8_t RUNTIME_GP40_REG                 = 0x3b;
	static constexpr uint8_t RUNTIME_GP41_REG                 = 0x3c;
	static constexpr uint8_t RUNTIME_GP42_REG                 = 0x3d;
	static constexpr uint8_t RUNTIME_GP43_REG                 = 0x3e;
	static constexpr uint8_t RUNTIME_GP50_REG                 = 0x3f;
	static constexpr uint8_t RUNTIME_GP51_REG                 = 0x40;
	static constexpr uint8_t RUNTIME_GP52_REG                 = 0x41;
	static constexpr uint8_t RUNTIME_GP53_REG                 = 0x42;
	static constexpr uint8_t RUNTIME_GP54_REG                 = 0x43;
	static constexpr uint8_t RUNTIME_GP55_REG                 = 0x44;
	static constexpr uint8_t RUNTIME_GP56_REG                 = 0x45;
	static constexpr uint8_t RUNTIME_GP57_REG                 = 0x46;
	static constexpr uint8_t RUNTIME_GP60_REG                 = 0x47;
	static constexpr uint8_t RUNTIME_GP61_REG                 = 0x48;
	static constexpr uint8_t RUNTIME_GP1_REG                  = 0x4b;
	static constexpr uint8_t RUNTIME_GP2_REG                  = 0x4c; //
	static constexpr uint8_t RUNTIME_GP3_REG                  = 0x4d;
	static constexpr uint8_t RUNTIME_GP4_REG                  = 0x4e; //
	static constexpr uint8_t RUNTIME_GP5_REG                  = 0x4f;
	static constexpr uint8_t RUNTIME_GP6_REG                  = 0x50;
	static constexpr uint8_t RUNTIME_FAN1_REG                 = 0x56;
	static constexpr uint8_t RUNTIME_FAN2_REG                 = 0x57;
	static constexpr uint8_t RUNTIME_FAN_CONTROL_REG          = 0x58;
	static constexpr uint8_t RUNTIME_FAN1_TACH_REG            = 0x59;
	static constexpr uint8_t RUNTIME_FAN2_TACH_REG            = 0x5a;
	static constexpr uint8_t RUNTIME_FAN1_PRELOAD_REG         = 0x5b;
	static constexpr uint8_t RUNTIME_FAN2_PRELOAD_REG         = 0x5c;
	static constexpr uint8_t RUNTIME_LED1_REG                 = 0x5d;
	static constexpr uint8_t RUNTIME_LED2_REG                 = 0x5e;
	static constexpr uint8_t RUNTIME_KBD_SCAN_CODE_REG        = 0x5f;

	static constexpr uint8_t REG92_ALT_A20_HIGH   = 1 << 1;
	static constexpr uint8_t REG92_ALT_RESET_HIGH = 1 << 0;
	static constexpr uint8_t REG92_DEFAULT_VALUE  = 0x24;

	lpc47m192_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void set_sysopt(uint8_t sysopt) { m_sysopt = sysopt; }
	void connect_pport_centronics(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option);
	void connect_coma_rs232(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option);
	void connect_comb_rs232(machine_config &config, const char* tag, lpc47m192_device::sopts_func_t slot_options_func, const char* default_option);
	void connect_kbdc_mouse(machine_config &config, const char* tag);
	void connect_kbdc_keyboard(machine_config &config, const char* tag);

	auto irq1_callback() { return m_irq1_cb.bind(); }
	auto irq2_callback() { return m_irq2_cb.bind(); }
	auto irq3_callback() { return m_irq3_cb.bind(); }
	auto irq4_callback() { return m_irq4_cb.bind(); }
	auto irq5_callback() { return m_irq5_cb.bind(); }
	auto irq6_callback() { return m_irq6_cb.bind(); }
	auto irq7_callback() { return m_irq7_cb.bind(); }
	auto irq8_callback() { return m_irq8_cb.bind(); }
	auto irq9_callback() { return m_irq9_cb.bind(); }
	auto irq10_callback() { return m_irq10_cb.bind(); }
	auto irq11_callback() { return m_irq11_cb.bind(); }
	auto irq12_callback() { return m_irq12_cb.bind(); }
	auto irq13_callback() { return m_irq13_cb.bind(); }
	auto irq14_callback() { return m_irq14_cb.bind(); }
	auto irq15_callback() { return m_irq15_cb.bind(); }
	auto system_reset_callback() { return m_system_reset_cb.bind(); }
	auto gate_a20_callback() { return m_gate_a20_cb.bind(); }

	auto parallel_data0_callback() { return m_par_h2dd0_w_cb.bind(); }
	auto parallel_data1_callback() { return m_par_h2dd1_w_cb.bind(); }
	auto parallel_data2_callback() { return m_par_h2dd2_w_cb.bind(); }
	auto parallel_data3_callback() { return m_par_h2dd3_w_cb.bind(); }
	auto parallel_data4_callback() { return m_par_h2dd4_w_cb.bind(); }
	auto parallel_data5_callback() { return m_par_h2dd5_w_cb.bind(); }
	auto parallel_data6_callback() { return m_par_h2dd6_w_cb.bind(); }
	auto parallel_data7_callback() { return m_par_h2dd7_w_cb.bind(); }
	auto parallel_control0_callback() { return m_par_h2dc0_w_cb.bind(); }
	auto parallel_control1_callback() { return m_par_h2dc1_w_cb.bind(); }
	auto parallel_control2_callback() { return m_par_h2dc2_w_cb.bind(); }
	auto parallel_control3_callback() { return m_par_h2dc3_w_cb.bind(); }
	auto parallel_control4_callback() { return m_par_h2dc4_w_cb.bind(); }
	template <unsigned BITIDX> void parallel_status_wbit(int state) { m_parallel->dev2hst_status_wbit<BITIDX>(state); }
	template <unsigned BITIDX> void parallel_control_wbit(int state) { m_parallel->dev2hst_cntl_wbit<BITIDX>(state); }
	auto parallel_control_irq_enable_w(int state) { m_parallel->hst2dev_cntl_irq_enable_w(state); }

	auto serial1_txd_callback() { return m_s1_txd_w_cb.bind(); }
	auto serial1_dtr_callback() { return m_s1_dtr_w_cb.bind(); }
	auto serial1_rts_callback() { return m_s1_rts_w_cb.bind(); }
	void serial1_rx_w(int state) { m_serial[0]->rx_w(state); }
	void serial1_dcd_w(int state) { m_serial[0]->dcd_w(state); }
	void serial1_dsr_w(int state) { m_serial[0]->dsr_w(state); }
	void serial1_ri_w(int state) { m_serial[0]->ri_w(state); }
	void serial1_cts_w(int state) { m_serial[0]->cts_w(state); }

	auto serial2_txd_callback() { return m_s2_txd_w_cb.bind(); }
	auto serial2_dtr_callback() { return m_s2_dtr_w_cb.bind(); }
	auto serial2_rts_callback() { return m_s2_rts_w_cb.bind(); }
	void serial2_rx_w(int state) { m_serial[1]->rx_w(state); }
	void serial2_dcd_w(int state) { m_serial[1]->dcd_w(state); }
	void serial2_dsr_w(int state) { m_serial[1]->dsr_w(state); }
	void serial2_ri_w(int state) { m_serial[1]->ri_w(state); }
	void serial2_cts_w(int state) { m_serial[1]->cts_w(state); }

	void keyboard_out_w(uint8_t data) { m_kbdc->keyboard_out_w(data); }
	auto keyboard_in_w_callback() { return m_kbdc_kbd_in_w_cb.bind(); }

	void mouse_out_w(uint8_t data) { m_kbdc->mouse_out_w(data); }
	auto mouse_in_w_callback() { return m_kbdc_mse_in_w_cb.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void remap(int space_id, offs_t start, offs_t end) override ATTR_COLD;

	void io_map(address_map &map) ATTR_COLD;

private:

	required_device_array<ns16550_device, 2> m_serial;
	required_device<ecp_lpt_device> m_parallel;
	required_device<lpc47m192_kbdc_device> m_kbdc;
	optional_device<at_keyboard_device> m_kbd;

	devcb_write_line m_irq1_cb;
	devcb_write_line m_irq2_cb;
	devcb_write_line m_irq3_cb;
	devcb_write_line m_irq4_cb;
	devcb_write_line m_irq5_cb;
	devcb_write_line m_irq6_cb;
	devcb_write_line m_irq7_cb;
	devcb_write_line m_irq8_cb;
	devcb_write_line m_irq9_cb;
	devcb_write_line m_irq10_cb;
	devcb_write_line m_irq11_cb;
	devcb_write_line m_irq12_cb;
	devcb_write_line m_irq13_cb;
	devcb_write_line m_irq14_cb;
	devcb_write_line m_irq15_cb;
	devcb_write_line m_system_reset_cb;
	devcb_write_line m_gate_a20_cb;

	devcb_write_line m_par_h2dd0_w_cb;
	devcb_write_line m_par_h2dd1_w_cb;
	devcb_write_line m_par_h2dd2_w_cb;
	devcb_write_line m_par_h2dd3_w_cb;
	devcb_write_line m_par_h2dd4_w_cb;
	devcb_write_line m_par_h2dd5_w_cb;
	devcb_write_line m_par_h2dd6_w_cb;
	devcb_write_line m_par_h2dd7_w_cb;
	devcb_write_line m_par_h2dc0_w_cb;
	devcb_write_line m_par_h2dc1_w_cb;
	devcb_write_line m_par_h2dc2_w_cb;
	devcb_write_line m_par_h2dc3_w_cb;
	devcb_write_line m_par_h2dc4_w_cb;

	devcb_write_line m_s1_txd_w_cb;
	devcb_write_line m_s1_dtr_w_cb;
	devcb_write_line m_s1_rts_w_cb;

	devcb_write_line m_s2_txd_w_cb;
	devcb_write_line m_s2_dtr_w_cb;
	devcb_write_line m_s2_rts_w_cb;

	devcb_write32 m_kbdc_kbd_in_w_cb;

	devcb_write32 m_kbdc_mse_in_w_cb;

	uint8_t m_sysopt;

	bool m_config_enabled;
	bool m_pending_remap;
	uint8_t m_config_state;

	uint8_t m_config_control;
	uint8_t m_selected_ldevice;
	uint8_t m_device_id;
	uint8_t m_device_rev;
	uint8_t m_power_control;
	uint8_t m_power_mgmt;
	uint8_t m_osc;
	uint16_t m_config_initial_ioaddr;
	uint16_t m_config_ioaddr;

	bool m_fdd_enabled;
	uint16_t m_fdd_ioaddr;
	uint8_t m_fdd_int_sel;
	uint8_t m_fdd_dma_chan_sel;
	uint8_t m_fdd_mode;
	uint8_t m_fdd_option;
	uint8_t m_fdd_type;
	uint8_t m_fdd_0; // ??
	uint8_t m_fdd_1; // ??

	bool m_parallel_enabled;
	uint16_t m_parallel_ioaddr;
	uint8_t m_parallel_int_sel;
	uint8_t m_parallel_dma_chan_sel;
	uint8_t m_parallel_mode1;
	uint8_t m_parallel_mode2;

	bool m_serial1_enabled;
	uint16_t m_serial1_ioaddr;
	uint8_t m_serial1_int_sel;
	uint8_t m_serial1_mode;

	bool m_serial2_enabled;
	uint16_t m_serial2_ioaddr;
	uint8_t m_serial2_int_sel;
	uint8_t m_serial2_mode;
	uint8_t m_serial2_ir_options;
	uint8_t m_serial2_ir_hdtimeout;

	bool m_kbd_enabled;
	uint16_t m_kbd_ioaddr;
	uint16_t m_a20_ioaddr;
	uint8_t m_kbd_pri_int_sel;
	uint8_t m_kbd_sec_int_sel;
	uint8_t m_kbd_reset_and_a20_sel;

	bool m_game_enabled;
	uint16_t m_game_ioaddr;

	bool m_runtime_enabled;
	uint16_t m_runtime_ioaddr;
	uint8_t m_runtime_clocki32;

	bool m_midi_enabled;
	uint16_t m_midi_ioaddr;
	uint8_t m_midi_int_sel;

	uint8_t m_runtime_block[RUNTIME_BLOCK_SIZE];

	void set_default_values();

	void config_w(uint8_t data);
	void index_w(uint8_t data);
	uint8_t config_data_fdd_r();
	void config_data_fdd_w(uint8_t data);
	uint8_t config_data_parallel_r();
	void config_data_parallel_w(uint8_t data);
	uint8_t config_data_serial1_r();
	void config_data_serial1_w(uint8_t data);
	uint8_t config_data_serial2_r();
	void config_data_serial2_w(uint8_t data);
	uint8_t config_data_kbd_r();
	void config_data_kbd_w(uint8_t data);
	uint8_t config_data_game_r();
	void config_data_game_w(uint8_t data);
	uint8_t config_data_runtime_r();
	void config_data_runtime_w(uint8_t data);
	uint8_t config_data_midi_r();
	void config_data_midi_w(uint8_t data);
	uint8_t config_data_ldevice_r();
	void config_data_ldevice_w(uint8_t data);
	uint8_t config_data_r();
	void config_data_w(uint8_t data);
	uint8_t data_r();
	void data_w(uint8_t data);
	uint8_t io92_r();
	void io92_w(uint8_t data);

	void irq_w(uint8_t irqno, int state);
	void system_reset_w(int state);
	void gate_a20_w(int state);
	void irq_parallel_w(int state);
	void irq_serial1_w(int state);
	void irq_serial2_w(int state);
	void irq_keyboard_w(int state);
	void irq_mouse_w(int state);

	uint8_t runtime_r(offs_t offset);
	void runtime_w(offs_t offset, uint8_t data);

};

DECLARE_DEVICE_TYPE(LPC47M192, lpc47m192_device);

#endif // MAME_WEBTV_LPC47M192_H
