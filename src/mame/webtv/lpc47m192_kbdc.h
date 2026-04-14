// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_LPC47M192_KBDC_H
#define MAME_WEBTV_LPC47M192_KBDC_H

#pragma once

#include "machine/pckeybrd.h"

class lpc47m192_kbdc_device : public device_t
{

public:

	static constexpr uint8_t KBDC_RAM_SIZE     = 0x20;
	static constexpr uint8_t DEVOUT_QUEUE_SIZE = 8;

	static constexpr uint8_t KBDC_FIRMWARE_VERSION = 0x35;

	static constexpr uint32_t BUFFER_CHECK_TIME = 1000; // Time in microseconds
	
	/*
	* │7│6│5│4│3│2│1│0│  8042 Status Register
	*  │ │ │ │ │ │ │ └──── output data register (60h) has data for system
	*  │ │ │ │ │ │ └───── input register (60h/64h) has data for 8042
	*  │ │ │ │ │ └────── user definable via firmware (unknown on MSNTV2's controller firmware, assuming self test pass)
	*  │ │ │ │ └─────── data in input register is from command_64w, otherwise data_60w
	*  │ │ │ └──────── user definable via firmware (unknown on MSNTV2's controller firmware, assuming keyboard not locked)
	*  │ │ └───────── user definable via firmware (unknown on MSNTV2's controller firmware, assuming, assuming output data is from mouse)
	*  │ └────────── user definable via firmware (unknown on MSNTV2's controller firmware, assuming transmit timeout error)
	*  └─────────── user definable via firmware (ounknown on MSNTV2's controller firmware, assuming parity error)
	*/
	static constexpr uint8_t KBDC_STATUS_OUTPUT_FULL    = 1 << 0;
	static constexpr uint8_t KBDC_STATUS_INPUT_FULL     = 1 << 1;
	static constexpr uint8_t KBDC_STATUS_SELFTEST_PASS  = 1 << 2;
	static constexpr uint8_t KBDC_STATUS_INPUT_IS_CMD   = 1 << 3;
	static constexpr uint8_t KBDC_STATUS_KEYBD_PASS     = 1 << 4;
	static constexpr uint8_t KBDC_STATUS_MOUSE_OUT      = 1 << 5;
	static constexpr uint8_t KBDC_STATUS_IN_TIMEOUT     = 1 << 6;
	static constexpr uint8_t KBDC_STATUS_OUT_PARITY_ERR = 1 << 7;

	/*
	* This is sometimes reffered to as the "command byte".
	* It's offset 0 in the controller's RAM in this controller. Original 8042 only had this byte stored (no RAM).
	*
	* │7│6│5│4│3│2│1│0│	8042 Command Byte
	*  │ │ │ │ │ │ │ └──── 1=enable output register full interrupt
	*  │ │ │ │ │ │ └───── reserved, should be 0
	*  │ │ │ │ │ └────── 1=set system flag/POST pass, 0=cleared on reset
	*  │ │ │ │ └─────── 1=override keyboard inhibit, 0=allow inhibit
	*  │ │ │ └──────── disable PS/2 port 1 (driving clock line low)
	*  │ │ └───────── disable PS/2 port 2 (drives clock line low)
	*  │ └────────── IBM scancode translation 0=AT, 1=PC/XT
	*  └─────────── reserved, should be 0
	*/
	static constexpr uint8_t KBDC_CONFIG_RAM_OFFSET      = 0;
	static constexpr uint8_t KBDC_CONFIG_KEYBD_INT_EN    = 1 << 0;
	static constexpr uint8_t KBDC_CONFIG_MOUSE_INT_EN    = 1 << 1;
	static constexpr uint8_t KBDC_CONFIG_SYSTEM_FLAG     = 1 << 2;
	static constexpr uint8_t KBDC_CONFIG_KEYBD_CLOCK_OFF = 1 << 4;
	static constexpr uint8_t KBDC_CONFIG_MOUSE_CLOCK_OFF = 1 << 5;
	static constexpr uint8_t KBDC_CONFIG_PCXT_EN         = 1 << 6;

	enum kbdc_cmd_t : uint8_t
	{
		KBDC_CMD_READ_RAM_MIN            = 0x00,
		KBDC_CMD_READ_RAM_MAX            = 0x3f,
		KBDC_CMD_WRITE_RAM_MIN           = 0x60,
		KBDC_CMD_WRITE_RAM_MAX           = 0x7f,
		KBDC_CMD_GET_FIRMWARE_VERSION    = 0xa1,
		KBDC_CMD_PS2_MOUSE_DISABLE       = 0xa7,
		KBDC_CMD_PS2_MOUSE_ENABLE        = 0xa8,
		KBDC_CMD_PS2_MOUSE_TEST          = 0xa9,
		KBDC_CMD_SELF_TEST               = 0xaa,
		KBDC_CMD_PS2_KEYBD_TEST          = 0xab,
		KBDC_CMD_DIAG_DUMP               = 0xac,
		KBDC_CMD_PS2_KEYBD_DISABLE       = 0xad,
		KBDC_CMD_PS2_KEYBD_ENABLE        = 0xae,
		KBDC_CMD_CNTL_INPUT_READ         = 0xc0,
		KBDC_CMD_CNTL_INPUT_HNIBBLE_POLL = 0xc1,
		KBDC_CMD_CNTL_INPUT_LNIBBLE_POLL = 0xc2,
		KBDC_CMD_CNTL_OUTPUT_READ        = 0xd0,
		KBDC_CMD_CNTL_OUTPUT_WRITE       = 0xd1,
		KBDC_CMD_PS2_KEYBD_OUTPUT_WRITE  = 0xd2,
		KBDC_CMD_PS2_MOUSE_OUTPUT_WRITE  = 0xd3,
		KBDC_CMD_PS2_MOUSE_INPUT_WRITE   = 0xd4,
		KBDC_CMD_PULSE_MIN               = 0xf0,
		KBDC_CMD_PULSE_MAX               = 0xff,
	};

	enum kbdc_self_test_result_t : uint8_t
	{
		KBDC_SELF_TEST_PASSED = 0x55,
		KBDC_SELF_TEST_FAILED = 0xfc,
	};

	enum kbdc_ps2_test_result_t : uint8_t
	{
		KBDC_PS2_TEST_PASSED          = 0x00,
		KBDC_PS2_TEST_CLK_STUCK_LOW   = 0x01,
		KBDC_PS2_TEST_CLK_STUCK_HIGH  = 0x02,
		KBDC_PS2_TEST_DATA_STUCK_LOW  = 0x03,
		KBDC_PS2_TEST_DATA_STUCK_HIGH = 0x04,
		KBDC_PS2_TEST_DEAD_PORT       = 0xff,
	};

	enum kbdc_data_60w_dest_t : uint8_t
	{
		KBDC_DATA_60W_TO_PS2_KEYBD_IN  = 0x00,
		KBDC_DATA_60W_TO_PS2_MOUSE_IN  = 0x01,
		KBDC_DATA_60W_TO_RAM           = 0x02,
		KBDC_DATA_60W_TO_CNTL_OUT      = 0x03,
		KBDC_DATA_60W_TO_PS2_KEYBD_OUT = 0x04,
		KBDC_DATA_60W_TO_PS2_MOUSE_OUT = 0x05,
	};

	enum kbdc_ps2_response_t : uint8_t
	{
		KBDC_PS2_RESPONSE_SELF_TEST_PASS   = 0xaa,
		KBDC_PS2_RESPONSE_SELF_TEST_FAILED = 0xfc,
		KBDC_PS2_RESPONSE_ECHO             = 0xee,
		KBDC_PS2_RESPONSE_ACK              = 0xfa,
		KBDC_PS2_RESPONSE_ERROR_RESEND     = 0xfe,
		KBDC_PS2_RESPONSE_NOP              = 0xff,
	};

	lpc47m192_kbdc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	uint8_t data_60r(offs_t offset);
	void data_60w(offs_t offset, uint8_t data);
	uint8_t status_64r(offs_t offset);
	void command_64w(offs_t offset, uint8_t data);

	void keyboard_out_w(uint8_t data);
	auto keyboard_in_w_callback() { return m_kbd_in_w_cb.bind(); }
	void keyboard_enable(bool enable) { m_keyboard_enabled = enable; }

	void mouse_out_w(uint8_t data);
	auto mouse_in_w_callback() { return m_mse_in_w_cb.bind(); }
	void mouse_enable(bool enable) { m_mouse_enabled = enable; }

	auto system_reset_callback() { return m_system_reset_cb.bind(); }
	auto gate_a20_callback() { return m_gate_a20_cb.bind(); }
	auto keybd_output_buffer_full_callback() { return m_keybd_output_buffer_full_cb.bind(); }
	auto mouse_output_buffer_full_callback() { return m_mouse_output_buffer_full_cb.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	devcb_write8 m_kbd_in_w_cb;

	devcb_write8 m_mse_in_w_cb;

	devcb_write_line m_system_reset_cb;
	devcb_write_line m_gate_a20_cb;
	devcb_write_line m_keybd_output_buffer_full_cb;
	devcb_write_line m_mouse_output_buffer_full_cb;

	emu_timer *m_buffer_checker;
	TIMER_CALLBACK_MEMBER(buffer_check);

	// Uses to translate Scan Code Set 2 to Scan Code Set 1 when bit 6 of the command byte is set
	static constexpr uint8_t XT_TRANSLATE_EXTENDED_MARKER = 0xe0;
	static constexpr uint8_t XT_TRANSLATE_AT_BREAK_MARKER = 0xf0;
	static constexpr uint8_t XT_TRANSLATE_BREAK_MODIFIER  = 0x80;
	static constexpr uint16_t XT_TRANSLATE_TABLE_SIZE     = 256;
	const uint8_t m_xt_translate_table[lpc47m192_kbdc_device::XT_TRANSLATE_TABLE_SIZE] = {
		0x00, 0x43, 0x00, 0x3f, 0x3d, 0x3b, 0x3c, 0x57, // 0x00 - 0x07
		0x00, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x58, // 0x08 - 0x0f
		0x65, 0x38, 0x2a, 0x00, 0x1d, 0x10, 0x02, 0x00, // 0x10 - 0x17
		0x66, 0x00, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x00, // 0x18 - 0x1f
		0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x00, // 0x20 - 0x27
		0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x00, // 0x28 - 0x2f
		0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x00, // 0x30 - 0x37
		0x6a, 0x00, 0x32, 0x24, 0x16, 0x08, 0x09, 0x00, // 0x38 - 0x3f
		0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x00, // 0x40 - 0x47
		0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x00, // 0x48 - 0x4f
		0x6d, 0x00, 0x28, 0x00, 0x1a, 0x0d, 0x00, 0x00, // 0x50 - 0x57
		0x3a, 0x36, 0x1c, 0x1b, 0x00, 0x2b, 0x00, 0x00, // 0x58 - 0x5f
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, // 0x60 - 0x67
		0x00, 0x4f, 0x00, 0x4b, 0x47, 0x00, 0x00, 0x00, // 0x68 - 0x6f
		0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x00, // 0x70 - 0x77
		0x00, 0x4e, 0x51, 0x4a, 0x00, 0x49, 0x46, 0x00, // 0x78 - 0x7f
		0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, // 0x80 - 0x8f
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x90 - 0x9f
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xa0 - 0xaf
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xb0 - 0xbf
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xc0 - 0xcf
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xd0 - 0xdf
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xe0 - 0xef
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 0xf0 - 0xff
	};
	bool m_xt_translate_hit_break;
	bool m_xt_translate_hit_extended;

	typedef struct
	{
		uint8_t scancode;
		bool mouse;
	} queue_item_t;

	queue_item_t m_devout_queue[lpc47m192_kbdc_device::DEVOUT_QUEUE_SIZE];
	uint8_t m_devout_head;
	uint8_t m_devout_tail;

	uint8_t m_data_in;

	uint8_t m_ram[KBDC_RAM_SIZE];
	uint8_t m_ram_offset;

	uint8_t m_status;
	kbdc_data_60w_dest_t m_60w_dest;

	bool m_keyboard_enabled;
	bool m_mouse_enabled;

	bool translate_to_pcxt(uint8_t* data);
	uint8_t queue_size();
	bool queue_empty();
	void enqueue_output(uint8_t data, bool mouse = false);
	uint8_t dequeue_output();
	void set_output_irq(queue_item_t item, int state);
	void enqueue_input(uint8_t data, bool is_command);
	uint8_t dequeue_input();
	void set_device_missing(bool mouse = false);
	void reset_config_flags();
	void set_config_flags(uint8_t command_bits);
	void clear_config_flags(uint8_t command_bits);
	bool check_config_flags(uint8_t command_bits);
	uint8_t read_internal_ram(offs_t offset);
	void write_internal_ram(offs_t offset, uint8_t data);


};

DECLARE_DEVICE_TYPE(LPC47M192_KBDC, lpc47m192_kbdc_device)

#endif // MAME_WEBTV_LPC47M192_KBDC_H
