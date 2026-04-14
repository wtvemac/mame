// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_MSNTV2_FPANEL_H
#define MAME_WEBTV_MSNTV2_FPANEL_H

#pragma once

#include "bus/centronics/ctronics.h"
#include "ecp_lpt.h"
#include "lpc47m192_kbdc.h"
#include "machine/pckeybrd.h"

class pic12f629_irkbd_device : public pc_keyboard_device, public device_nvram_interface
{

public:

	// ID/VER min is 0x010d. ID below was from a RM4100 box
	static constexpr uint16_t CONFIG_USER_ID0  = 0x0000;
	static constexpr uint16_t CONFIG_USER_ID1  = 0x0001; // older 0x0001 from stored code in BIOS
	static constexpr uint16_t CONFIG_USER_ID2  = 0x0001; // older 0x0000 from stored code in BIOS
	static constexpr uint16_t CONFIG_USER_ID3  = 0x0008; // older 0x000d from stored code in BIOS

	static constexpr uint16_t CONFIG_SETTINGS  = 0x014c;
	static constexpr uint16_t CONFIG_DEVICE_ID = 0x3e00;

	static constexpr uint32_t SRAM_DATA_MEMORY_SIZE      = 0x00000040;
	static constexpr uint32_t EEPROM_DATA_MEMORY_SIZE    = 0x00000080; // Bytes
	static constexpr uint32_t EEPROM_PROGRAM_MEMORY_SIZE = 0x00000400; // Words
	static constexpr uint32_t EEPROM_CONFIG_MEMORY_SIZE  = 0x00000020; // Words

	enum in_mode_t : uint8_t
	{
		COMMAND        = 0x01,
		TYPEMATIC_RATE = 0x02,
		SCANCODE_SET   = 0x04,
		LED_STATE      = 0x05
	};

	enum data_stage_t : uint8_t
	{
		IDLE              = 0,
		READ_COMMAND      = 1,
		PROCESS_PARAMETER = 2
	};

	static constexpr uint32_t PROG_COMMAND_BIT_SIZE            = 6;
	static constexpr uint32_t PROG_COMMAND_MASK                = 0x3f;
	static constexpr uint32_t PROG_DEFAULT_PARAMETER_BIT_SIZE  = 14 + 2;
	static constexpr uint32_t PROG_PARAMETER_SHIFT             = 1;
	static constexpr uint32_t PROG_PARAMETER_MASK              = 0x3fff; // 14-bit word
	enum prog_cmd_t : uint8_t
	{
		LOAD_CONFIGURATION     = 0x00,
		LOAD_PROGRAM_MEMORY    = 0x02,
		LOAD_DATA_MEMORY       = 0x03,

		READ_PROGRAM_MEMORY    = 0x04,
		READ_DATA_MEMORY       = 0x05,

		INCREMENT_ADDRESS      = 0x06,

		BEGIN_PROGRAM_INT_TIME = 0x08,
		BEGIN_PROGRAM_EXT_TIME = 0x18,

		ERASE_PROGRAM_MEMORY   = 0x09,
		ERASE_DATA_MEMORY      = 0x0b,

		NOP                    = 0x3f // Internal command state
	};

	enum data_location_t : uint8_t
	{
		NONE           = 0,
		PROGRAM_MEMORY = 1,
		DATA_MEMORY    = 2,
		CONFIGURATION  = 3
	};

	pic12f629_irkbd_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto out_w_callback() { return m_out_w_cb.bind(); }
	void in_w(uint8_t data);
	void send_keypresses(int state);

	void set_gp0(int state);
	void set_gp1(int state);
	void set_gp2(int state);
	void set_gp3(int state);
	void set_gp4(int state);
	void set_gp5(int state);

	auto gp0_callback() { return m_gp0_cb.bind(); }
	auto gp1_callback() { return m_gp1_cb.bind(); }
	auto gp2_callback() { return m_gp2_cb.bind(); }
	auto gp3_callback() { return m_gp3_cb.bind(); }
	auto gp4_callback() { return m_gp4_cb.bind(); }
	auto gp5_callback() { return m_gp5_cb.bind(); }

protected:

	virtual void device_reset() override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;

	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	virtual void nvram_default() override ATTR_COLD;
	virtual bool nvram_read(util::read_stream &file) override ATTR_COLD;
	virtual bool nvram_write(util::write_stream &file) override ATTR_COLD;

private:

	enum kbd_cmd_t : uint8_t
	{
		KBD_CMD_SET_LEDS            = 0xed,
		KBD_CMD_ECHO                = 0xee,
		KBD_CMD_SET_SCANCODE_SET    = 0xf0,
		KBD_CMD_IDENTIFY            = 0xf2,
		KBD_CMD_SET_TRYPEMATIC_RATE = 0xf3,
		KBD_CMD_ENABLE_SCANNING     = 0xf4,
		KBD_CMD_DISABLE_SCANNING    = 0xf5,
		KBD_CMD_RESET               = 0xff,
	};

	enum kbd_response_t : uint8_t
	{
		KBD_RESPONSE_SELF_TEST_PASSED = 0xaa,
		KBD_RESPONSE_ACK              = 0xfa,
		KBD_RESPONSE_ECHO             = 0xee
	};

	static constexpr uint32_t SCANCODE_TRANSLATE_TABLE_SIZE = 128;

	typedef struct
	{
		const char* makeseq;
		const char* breakseq;
	} scancode_sequence_t;

	optional_ioport_array<8> m_ioport;

	devcb_write8 m_out_w_cb;

	devcb_write_line m_gp0_cb;
	devcb_write_line m_gp1_cb;
	devcb_write_line m_gp2_cb;
	devcb_write_line m_gp3_cb;
	devcb_write_line m_gp4_cb;
	devcb_write_line m_gp5_cb;

	in_mode_t m_in_mode;

	uint8_t m_typematic_rate;
	uint8_t m_scancode_set;
	uint8_t m_led_state;
	uint8_t m_keys_pressed;

	static constexpr uint8_t BREAK_MARKER = 0xf0;
	const char* IDLE_SEQUENCE = "\xe0\xf0\x14"; // Essentially RCtrl break

	scancode_sequence_t translated_scancodes[pic12f629_irkbd_device::SCANCODE_TRANSLATE_TABLE_SIZE] = {
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x00 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x01 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x02 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x03 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x04 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x05 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x06 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x07 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x08 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x09 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0a */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x0f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x10 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x11 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x12 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x13 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x14 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x15 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x16 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x17 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x18 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x19 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1a */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x1f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x20 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x21 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x22 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x23 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x24 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x25 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x26 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x27 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x28 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x29 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2a */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x2f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x30 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x31 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x32 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x33 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x34 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x35 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x36 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x37 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x38 */
		{ .makeseq = "\x3a",     .breakseq = "\xf0\x3a"                 }, /* 0x39: M */
		{ .makeseq = "\x58",     .breakseq = "\xf0\x58"                 }, /* 0x3a: Caps */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x3b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x3c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x3d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x3e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x3f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x40 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x41 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x42 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x43 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x44 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x45 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x46 */
		{ .makeseq = "\x45",     .breakseq = "\xf0\x45"                 }, /* 0x47: 0 ) */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x48 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x49 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4a */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x4f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x50 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x51 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x52 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x53 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x54 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x55 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x56 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x57 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x58 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x59 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5a */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5c */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x5f */
		{ .makeseq = "\x00",     .breakseq = "\x14\x15\xf0\x15\xf0\x14" }, /* 0x60: MSNTV2: Power */
		{ .makeseq = "\xe0\x10", .breakseq = "\xe0\xf0\x10"             }, /* 0x61: MSNTV2: Search */
		{ .makeseq = "\xe0\x18", .breakseq = "\xe0\xf0\x18"             }, /* 0x62: MSNTV2: Favorites */
		{ .makeseq = "\xe0\x1d", .breakseq = "\xe0\xf0\x1d"             }, /* 0x63: Print */
		{ .makeseq = "\xe0\x20", .breakseq = "\xe0\xf0\x20"             }, /* 0x64: Refresh */
		{ .makeseq = "\xe0\x38", .breakseq = "\xe0\xf0\x38"             }, /* 0x65: Back */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x66 */
		{ .makeseq = "\xe0\x3a", .breakseq = "\xe0\xf0\x3a"             }, /* 0x67: MSNTV2: Home */
		{ .makeseq = "\xe0\x48", .breakseq = "\xe0\xf0\x48"             }, /* 0x68: MSNTV2: Mail */
		{ .makeseq = "\xe0\x50", .breakseq = "\xe0\xf0\x50"             }, /* 0x69: MSNTV2: Player */
		{ .makeseq = "\xe0\x5a", .breakseq = "\xe0\xf0\x5a"             }, /* 0x6a: OK/Keypad Enter */
		{ .makeseq = "\xe0\x6b", .breakseq = "\xe0\xf0\x6b"             }, /* 0x6b: Left */
		{ .makeseq = "\xe0\x6c", .breakseq = "\xe0\xf0\x6c"             }, /* 0x6c: Jump To Top/Home */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x6d */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x6e */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x6f */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x70 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x71 */
		{ .makeseq = "\xe0\x72", .breakseq = "\xe0\xf0\x72"             }, /* 0x72: Down */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x73 */
		{ .makeseq = "\xe0\x74", .breakseq = "\xe0\xf0\x74"             }, /* 0x74: Right */
		{ .makeseq = "\xe0\x75", .breakseq = "\xe0\xf0\x75"             }, /* 0x75: Up */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x76 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x77 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x78 */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x79 */
		{ .makeseq = "\xe0\x7a", .breakseq = "\xe0\xf0\x7a"             }, /* 0x7a: Page Down */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x7b */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x7c */
		{ .makeseq = "\xe0\x7d", .breakseq = "\xe0\xf0\x7d"             }, /* 0x7d: Page Up */
		{ .makeseq = nullptr,    .breakseq = nullptr                    }, /* 0x7e */
		{ .makeseq = "\x83",     .breakseq = "\xf0\x83"                 }, /* 0x7f: MSNTV2: Menu/F7 */
	};

	uint16_t m_data_mem[pic12f629_irkbd_device::EEPROM_DATA_MEMORY_SIZE];
	uint16_t m_program_mem[pic12f629_irkbd_device::EEPROM_PROGRAM_MEMORY_SIZE];
	uint16_t m_config_mem[pic12f629_irkbd_device::EEPROM_CONFIG_MEMORY_SIZE];

	// Vdd/Power Supply
	// Vss/Ground
	// GP0/CIN+/ICSPDAT: used as the DATA function in programming mode
	// GP1/CIN-/Vref/ICSPCLK: used as the CLOCK function in programming mode
	// GP2/COUT/T0CK/INT
	// GP3/MCLR/Vpp: used to set the programming mode
	// GP4/T1G/OSC2/CLKOUT
	// GP5/T1CK/OSC1/CLKIN

	int m_gp0;
	int m_gp1;
	int m_gp2;
	int m_gp3;
	int m_gp4;
	int m_gp5;

	data_stage_t m_prog_cmd_stage;
	uint8_t m_prog_curr_cmd;
	uint16_t m_prog_curr_param;
	bool m_prog_is_param_in;
	uint8_t m_prog_cmd_bit_index;
	uint8_t m_prog_param_bit_size;

	data_location_t m_prog_data_location;
	uint16_t m_prog_data_index;

	void in_command_w(uint8_t data);
	void in_typematic_rate_w(uint8_t data);
	void in_scancode_set_w(uint8_t data);
	void in_led_state_w(uint8_t data);
	
	void reset_programming_state();
	void reset_command_state();
	void advance_programming_state();
	void read_command();
	void prepare_parameter_state();
	void process_parameter();
	uint16_t load_data();
	void program_data(uint16_t data);
	void execute_command();

	void scancode_insert_sequence(const char *codes);
	void scancode_insert(int code, int pressed);
	virtual void standard_scancode_insert(int code, int pressed) override;
	virtual void extended_scancode_insert(int code, int pressed) override;

};

class msntv2_fpanel_device : public device_t, public device_centronics_peripheral_interface
{

public:

	static constexpr uint32_t PIC_GP3_PDATA_SHIFT = 6;
	static constexpr uint32_t PIC_GP3_PDATA_MASK  = 1 << msntv2_fpanel_device::PIC_GP3_PDATA_SHIFT;

	msntv2_fpanel_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto keyboard_out_w_callback() { return m_irkbd->out_w_callback(); }
	void keyboard_in_w(uint8_t data) { m_irkbd->in_w(data); }

protected:

	virtual void device_reset() override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void input_data0(int state) override;
	virtual void input_data1(int state) override;
	virtual void input_data2(int state) override;
	virtual void input_data3(int state) override;
	virtual void input_data4(int state) override;
	virtual void input_data5(int state) override;
	virtual void input_data6(int state) override;
	virtual void input_data7(int state) override;

	virtual void input_strobe(int state) override;
	virtual void input_select(int state) override;
	virtual void input_autofd(int state) override;

private:

	required_device<pic12f629_irkbd_device> m_irkbd;

	output_finder<> m_power_led;
	output_finder<> m_connect_led;
	output_finder<> m_message_led;

	int m_strobe;
	uint8_t m_pdata;

	void out_gp3();

};

DECLARE_DEVICE_TYPE(PIC12F629_IRKBD, pic12f629_irkbd_device)
DECLARE_DEVICE_TYPE(MSNTV2_FPANEL, msntv2_fpanel_device)

#endif // MAME_MACHINEMAME_WEBTV_MSNTV2_FPANEL_H_LPC47M192_H
