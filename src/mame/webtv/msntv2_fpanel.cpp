// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "msntv2_fpanel.h"

DEFINE_DEVICE_TYPE(PIC12F629_IRKBD, pic12f629_irkbd_device, "m2_pic12f629_device", "PIC12F629 IR Keyboard Receiver")
DEFINE_DEVICE_TYPE(MSNTV2_FPANEL, msntv2_fpanel_device, "msntv2_fpanel_device", "MSNTV2 Front Panel Interface")

pic12f629_irkbd_device::pic12f629_irkbd_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	pc_keyboard_device(mconfig, PIC12F629_IRKBD, tag, owner, clock),
	device_nvram_interface(mconfig, *this),
	m_ioport(*this, "msntv2_irkbd%u", 0),
	m_out_w_cb(*this),
	m_gp0_cb(*this),
	m_gp1_cb(*this),
	m_gp2_cb(*this),
	m_gp3_cb(*this),
	m_gp4_cb(*this),
	m_gp5_cb(*this)
{
	pc_keyboard_device::m_type = pc_keyboard_device::KEYBOARD_TYPE::AT;
}

void pic12f629_irkbd_device::device_start()
{
	pc_keyboard_device::device_start();
}

void pic12f629_irkbd_device::device_reset()
{
	pc_keyboard_device::device_reset();

	m_keys_pressed = 0;

	m_gp0 = 0;
	m_gp1 = 0;
	m_gp2 = 0;
	m_gp3 = 0;
	m_gp4 = 0;
	m_gp5 = 0;

	pic12f629_irkbd_device::reset_programming_state();
}

void pic12f629_irkbd_device::nvram_default()
{
	std::fill(std::begin(m_data_mem),    std::end(m_data_mem),    0xffff);
	std::fill(std::begin(m_program_mem), std::end(m_program_mem), 0xffff);
	std::fill(std::begin(m_config_mem), std::end(m_config_mem), 0xffff);

	m_config_mem[0] = pic12f629_irkbd_device::CONFIG_USER_ID0;
	m_config_mem[1] = pic12f629_irkbd_device::CONFIG_USER_ID1;
	m_config_mem[2] = pic12f629_irkbd_device::CONFIG_USER_ID2;
	m_config_mem[3] = pic12f629_irkbd_device::CONFIG_USER_ID3;
	// Reserved
	// Reserved
	m_config_mem[6] = pic12f629_irkbd_device::CONFIG_DEVICE_ID;
	m_config_mem[7] = pic12f629_irkbd_device::CONFIG_SETTINGS;
}

bool pic12f629_irkbd_device::nvram_read(util::read_stream &file)
{
	std::error_condition err;
	size_t bytes_read;

	size_t data_memory_bytes = pic12f629_irkbd_device::EEPROM_DATA_MEMORY_SIZE;
	size_t program_memory_bytes = pic12f629_irkbd_device::EEPROM_PROGRAM_MEMORY_SIZE << 1;
	size_t config_memory_bytes = pic12f629_irkbd_device::EEPROM_CONFIG_MEMORY_SIZE << 1;

	std::tie(err, bytes_read) = util::read(file, m_data_mem, data_memory_bytes);
	if (!err && (bytes_read == data_memory_bytes))
	{
		std::tie(err, bytes_read) = util::read(file, m_program_mem, program_memory_bytes);
		if (!err && (bytes_read == program_memory_bytes))
		{
			std::tie(err, bytes_read) = util::read(file, m_config_mem, config_memory_bytes);

			return (!err && (bytes_read == config_memory_bytes));
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool pic12f629_irkbd_device::nvram_write(util::write_stream &file)
{
	std::error_condition err;
	size_t bytes_written;

	size_t data_memory_bytes = pic12f629_irkbd_device::EEPROM_DATA_MEMORY_SIZE;
	size_t program_memory_bytes = pic12f629_irkbd_device::EEPROM_PROGRAM_MEMORY_SIZE << 1;
	size_t config_memory_bytes = pic12f629_irkbd_device::EEPROM_CONFIG_MEMORY_SIZE << 1;

	std::tie(err, bytes_written) = util::write(file, m_data_mem, data_memory_bytes);
	if (!err && (bytes_written == data_memory_bytes))
	{
		std::tie(err, bytes_written) = util::write(file, m_program_mem, program_memory_bytes);
		if (!err && (bytes_written == program_memory_bytes))
		{
			std::tie(err, bytes_written) = util::write(file, m_config_mem, config_memory_bytes);

			return (!err && (bytes_written == config_memory_bytes));
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

void pic12f629_irkbd_device::in_command_w(uint8_t data)
{
	switch(data)
	{
		case kbd_cmd_t::KBD_CMD_SET_LEDS:
			m_in_mode = in_mode_t::LED_STATE;
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;

		case kbd_cmd_t::KBD_CMD_ECHO:
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ECHO);
			break;

		case kbd_cmd_t::KBD_CMD_SET_SCANCODE_SET:
			m_in_mode = in_mode_t::SCANCODE_SET;
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;

		case kbd_cmd_t::KBD_CMD_IDENTIFY:
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			// 0xab then 0x83 => MF2 keybaord 
			m_out_w_cb(0xab);
			m_out_w_cb(0x83);
			break;

		case kbd_cmd_t::KBD_CMD_SET_TRYPEMATIC_RATE:
			m_in_mode = in_mode_t::TYPEMATIC_RATE;
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;

		case kbd_cmd_t::KBD_CMD_ENABLE_SCANNING:
			clear_buffer();
			enable(1);
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;

		case kbd_cmd_t::KBD_CMD_DISABLE_SCANNING:
			clear_buffer();
			enable(0);
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;

		case kbd_cmd_t::KBD_CMD_RESET:
			clear_buffer();
			reset();
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_SELF_TEST_PASSED);
			break;

		default:
			m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);
			break;
	}
}

void pic12f629_irkbd_device::in_typematic_rate_w(uint8_t data)
{
	m_typematic_rate = data;

	m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);

	m_in_mode = in_mode_t::COMMAND;
}

void pic12f629_irkbd_device::in_scancode_set_w(uint8_t data)
{
	m_scancode_set = data; // Not used

	m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);

	m_in_mode = in_mode_t::COMMAND;
}

void pic12f629_irkbd_device::in_led_state_w(uint8_t data)
{
	m_led_state = data; // Not used

	m_out_w_cb(kbd_response_t::KBD_RESPONSE_ACK);

	m_in_mode = in_mode_t::COMMAND;
}

void pic12f629_irkbd_device::in_w(uint8_t data)
{
	switch(m_in_mode)
	{
		case in_mode_t::TYPEMATIC_RATE:
			pic12f629_irkbd_device::in_typematic_rate_w(data);
			break;

		case in_mode_t::SCANCODE_SET:
			pic12f629_irkbd_device::in_scancode_set_w(data);
			break;

		case in_mode_t::LED_STATE:
			pic12f629_irkbd_device::in_led_state_w(data);
			break;

		case in_mode_t::COMMAND:
		default:
			pic12f629_irkbd_device::in_command_w(data);
			break;
	}
}

void pic12f629_irkbd_device::send_keypresses(int state)
{
	if(state)
	{
		while(uint8_t data = read())
			m_out_w_cb(data);
	}
}

void pic12f629_irkbd_device::set_gp0(int state)
{
	m_gp0 = state;
}

void pic12f629_irkbd_device::set_gp1(int state)
{
	m_gp1 = state;

	if(m_gp3 && m_gp1)
		pic12f629_irkbd_device::advance_programming_state();
}

void pic12f629_irkbd_device::set_gp2(int state)
{
	m_gp2 = state;
}

void pic12f629_irkbd_device::set_gp3(int state)
{
	m_gp3 = state;

	if(m_gp3)
		pic12f629_irkbd_device::reset_programming_state();
}

void pic12f629_irkbd_device::set_gp4(int state)
{
	m_gp4 = state;
}

void pic12f629_irkbd_device::set_gp5(int state)
{
	m_gp5 = state;
}

void pic12f629_irkbd_device::reset_programming_state()
{
	pic12f629_irkbd_device::reset_command_state();
	m_prog_is_param_in = false;
	m_prog_curr_param = 0x0000;
	m_prog_data_location = data_location_t::PROGRAM_MEMORY;
	m_prog_data_index = 0x0000;
}

void pic12f629_irkbd_device::reset_command_state()
{
	m_prog_cmd_stage = data_stage_t::READ_COMMAND;
	m_prog_cmd_bit_index = 0;
	m_prog_curr_cmd = prog_cmd_t::NOP;
	m_prog_param_bit_size = 0;
}

void pic12f629_irkbd_device::advance_programming_state()
{
	switch(m_prog_cmd_stage)
	{
		case data_stage_t::READ_COMMAND:
			pic12f629_irkbd_device::read_command();
			break;

		case data_stage_t::PROCESS_PARAMETER:
			pic12f629_irkbd_device::process_parameter();
			break;

		default:
			break;
	}
}

void pic12f629_irkbd_device::read_command()
{
	m_prog_curr_cmd >>= 1;
	m_prog_curr_cmd |= (m_gp0 & 0x1) << (pic12f629_irkbd_device::PROG_COMMAND_BIT_SIZE - 1);

	m_prog_cmd_bit_index++;

	if(m_prog_cmd_bit_index >= pic12f629_irkbd_device::PROG_COMMAND_BIT_SIZE)
	{
		m_prog_cmd_bit_index = 0;

		m_prog_curr_cmd = (~m_prog_curr_cmd) & pic12f629_irkbd_device::PROG_COMMAND_MASK;

		pic12f629_irkbd_device::prepare_parameter_state();

		if(m_prog_param_bit_size == 0)
			pic12f629_irkbd_device::execute_command();
		else
			m_prog_cmd_stage = data_stage_t::PROCESS_PARAMETER;
	}
}

void pic12f629_irkbd_device::prepare_parameter_state()
{
	switch(m_prog_curr_cmd)
	{
		case prog_cmd_t::LOAD_DATA_MEMORY:
			m_prog_param_bit_size = pic12f629_irkbd_device::PROG_DEFAULT_PARAMETER_BIT_SIZE;
			m_prog_is_param_in = true;
			m_prog_curr_param = 0x0000;
			m_prog_data_location = data_location_t::DATA_MEMORY;
			break;
		case prog_cmd_t::LOAD_PROGRAM_MEMORY:
			m_prog_param_bit_size = pic12f629_irkbd_device::PROG_DEFAULT_PARAMETER_BIT_SIZE;
			m_prog_is_param_in = true;
			m_prog_curr_param = 0x0000;
			if(m_prog_data_location == data_location_t::DATA_MEMORY)
				m_prog_data_location = data_location_t::PROGRAM_MEMORY;
			break;
		case prog_cmd_t::LOAD_CONFIGURATION:
			m_prog_param_bit_size = pic12f629_irkbd_device::PROG_DEFAULT_PARAMETER_BIT_SIZE;
			m_prog_is_param_in = true;
			m_prog_curr_param = 0x0000;
			m_prog_data_location = data_location_t::CONFIGURATION;
			m_prog_data_index = 0x0000;
			break;

		case prog_cmd_t::READ_PROGRAM_MEMORY:
			m_prog_param_bit_size = pic12f629_irkbd_device::PROG_DEFAULT_PARAMETER_BIT_SIZE;
			m_prog_is_param_in = false;
			m_prog_curr_param = ((~pic12f629_irkbd_device::load_data()) & pic12f629_irkbd_device::PROG_PARAMETER_MASK) << pic12f629_irkbd_device::PROG_PARAMETER_SHIFT;
			if(m_prog_data_location == data_location_t::DATA_MEMORY)
				m_prog_data_location = data_location_t::PROGRAM_MEMORY;
			break;

		case prog_cmd_t::READ_DATA_MEMORY:
			m_prog_param_bit_size = pic12f629_irkbd_device::PROG_DEFAULT_PARAMETER_BIT_SIZE;
			m_prog_is_param_in = false;
			m_prog_curr_param = ((~pic12f629_irkbd_device::load_data()) & pic12f629_irkbd_device::PROG_PARAMETER_MASK) << pic12f629_irkbd_device::PROG_PARAMETER_SHIFT;
			m_prog_data_location = data_location_t::DATA_MEMORY;
			break;

		case prog_cmd_t::INCREMENT_ADDRESS:
		case prog_cmd_t::BEGIN_PROGRAM_INT_TIME:
		case prog_cmd_t::BEGIN_PROGRAM_EXT_TIME:
		case prog_cmd_t::ERASE_DATA_MEMORY:
		case prog_cmd_t::ERASE_PROGRAM_MEMORY:
		default:
			m_prog_param_bit_size = 0;
			break;
	}
}

void pic12f629_irkbd_device::process_parameter()
{
	if(m_prog_is_param_in)
	{
		m_prog_curr_param >>= 1;
		m_prog_curr_param |= (m_gp0 & 0x1) << (m_prog_param_bit_size - 1);
	}
	else
	{
		m_gp4_cb(m_prog_curr_param & 0x1);
		m_prog_curr_param >>= 1;
	}

	m_prog_cmd_bit_index++;

	if(m_prog_cmd_bit_index >= m_prog_param_bit_size)
	{
		m_prog_curr_param = (~m_prog_curr_param) & ((1 << m_prog_param_bit_size) - 1);

		pic12f629_irkbd_device::execute_command();
	}
}

uint16_t pic12f629_irkbd_device::load_data()
{
	switch(m_prog_data_location)
	{
		case DATA_MEMORY:
			return m_data_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_DATA_MEMORY_SIZE - 1)];

		case PROGRAM_MEMORY:
			return m_program_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_PROGRAM_MEMORY_SIZE - 1)];

		case CONFIGURATION:
			return m_config_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_CONFIG_MEMORY_SIZE - 1)];

		default:
			return 0x0000;
	}
}

void pic12f629_irkbd_device::program_data(uint16_t data)
{
	switch(m_prog_data_location)
	{
		case DATA_MEMORY:
			m_data_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_DATA_MEMORY_SIZE - 1)] = data;
			break;

		case PROGRAM_MEMORY:
			m_program_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_PROGRAM_MEMORY_SIZE - 1)] = data;
			break;

		case CONFIGURATION:
			m_config_mem[m_prog_data_index & (pic12f629_irkbd_device::EEPROM_CONFIG_MEMORY_SIZE - 1)] = data;
			break;

		default:
			break;
	}
}

void pic12f629_irkbd_device::execute_command()
{
	switch(m_prog_curr_cmd)
	{
		case prog_cmd_t::BEGIN_PROGRAM_INT_TIME:
		case prog_cmd_t::BEGIN_PROGRAM_EXT_TIME:
			pic12f629_irkbd_device::program_data(m_prog_curr_param >> pic12f629_irkbd_device::PROG_PARAMETER_SHIFT);
			break;

		case prog_cmd_t::ERASE_PROGRAM_MEMORY:
			std::fill(std::begin(m_program_mem), std::end(m_program_mem), 0xffff);
			break;

		case prog_cmd_t::ERASE_DATA_MEMORY:
			std::fill(std::begin(m_data_mem),    std::end(m_data_mem),    0xffff);
			break;

		case prog_cmd_t::INCREMENT_ADDRESS:
			m_prog_data_index++;
			break;

		case prog_cmd_t::LOAD_CONFIGURATION:
		case prog_cmd_t::LOAD_PROGRAM_MEMORY:
		case prog_cmd_t::LOAD_DATA_MEMORY:
		case prog_cmd_t::READ_PROGRAM_MEMORY:
		case prog_cmd_t::READ_DATA_MEMORY:
		default:
			break;
	}

	pic12f629_irkbd_device::reset_command_state();
}

void pic12f629_irkbd_device::scancode_insert_sequence(const char *codes)
{
	for (int i = 0; codes[i]; i++)
		queue_insert(codes[i]);
}

void pic12f629_irkbd_device::scancode_insert(int code, int pressed)
{
	bool had_pressed = false;

	if (pressed)
	{
		m_keys_pressed++;

		if(translated_scancodes[code & (pic12f629_irkbd_device::SCANCODE_TRANSLATE_TABLE_SIZE - 1)].makeseq != nullptr)
			pic12f629_irkbd_device::scancode_insert_sequence(translated_scancodes[code & (pic12f629_irkbd_device::SCANCODE_TRANSLATE_TABLE_SIZE - 1)].makeseq);
		else
			queue_insert(code);
	}
	else
	{
		if(m_keys_pressed > 0)
			had_pressed = true;

		m_keys_pressed--;

		if(translated_scancodes[code & (pic12f629_irkbd_device::SCANCODE_TRANSLATE_TABLE_SIZE - 1)].breakseq != nullptr)
		{
			pic12f629_irkbd_device::scancode_insert_sequence(translated_scancodes[code & (pic12f629_irkbd_device::SCANCODE_TRANSLATE_TABLE_SIZE - 1)].breakseq);
		}
		else
		{
			queue_insert(pic12f629_irkbd_device::BREAK_MARKER);
			queue_insert(code);
		}
	}

	if(had_pressed && m_keys_pressed == 0)
	{
		pic12f629_irkbd_device::scancode_insert_sequence(pic12f629_irkbd_device::IDLE_SEQUENCE);
	}
}

void pic12f629_irkbd_device::standard_scancode_insert(int code, int pressed)
{
	pic12f629_irkbd_device::scancode_insert(code, pressed);
}

void pic12f629_irkbd_device::extended_scancode_insert(int code, int pressed)
{
	pic12f629_irkbd_device::scancode_insert(code, pressed);
}

static INPUT_PORTS_START(msntv2_irkbd)

	PORT_START("pc_keyboard_0")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x00 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Recent/F9")      PORT_CODE(KEYCODE_F10)         PORT_CHAR(UCHAR_MAMEKEY(F10))                              /* 0x01 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x02 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Save/F5")        PORT_CODE(KEYCODE_F9)          PORT_CHAR(UCHAR_MAMEKEY(F9))                               /* 0x03 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Fav #3/F3")                                                                                                /* 0x04 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Fav #1/F1")                                                                                                /* 0x05 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Fav #2/F2")                                                                                                /* 0x06 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Photos/F12")     PORT_CODE(KEYCODE_F8)          PORT_CHAR(UCHAR_MAMEKEY(F8))                               /* 0x07 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x08 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Music/F10")      PORT_CODE(KEYCODE_F6)          PORT_CHAR(UCHAR_MAMEKEY(F6))                               /* 0x09 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Msgr/F8")                                                                                                  /* 0x0a */ 
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Resize Page/F6")                                                                                           /* 0x0b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Type www/F4")    PORT_CODE(KEYCODE_F7)          PORT_CHAR(UCHAR_MAMEKEY(F7))                               /* 0x0c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Tab")                    PORT_CODE(KEYCODE_TAB)         PORT_CHAR(0x09)                                            /* 0x0d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("` ~")                    PORT_CODE(KEYCODE_TILDE)       PORT_CHAR('`')                        PORT_CHAR('~')       /* 0x0e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x0f */

	PORT_START("pc_keyboard_1")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x10 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Alt")                     PORT_CODE(KEYCODE_LALT)       PORT_CODE(KEYCODE_RALT)                                    /* 0x11 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Shift")              PORT_CODE(KEYCODE_LSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(LSHIFT))                           /* 0x12 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x13 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Ctrl")                    PORT_CODE(KEYCODE_LCONTROL)   PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))                         /* 0x14 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q")                       PORT_CODE(KEYCODE_Q)          PORT_CHAR('Q')                                             /* 0x15 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1 !")                     PORT_CODE(KEYCODE_1)          PORT_CHAR('1')                        PORT_CHAR('!')       /* 0x16 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x17 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x18 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x19 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z")                       PORT_CODE(KEYCODE_Z)          PORT_CHAR('Z')                                             /* 0x1a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S")                       PORT_CODE(KEYCODE_S)          PORT_CHAR('S')                                             /* 0x1b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A")                       PORT_CODE(KEYCODE_A)          PORT_CHAR('A')                                             /* 0x1c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W")                       PORT_CODE(KEYCODE_W)          PORT_CHAR('W')                                             /* 0x1d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2 @")                     PORT_CODE(KEYCODE_2)          PORT_CHAR('2')                        PORT_CHAR('@')       /* 0x1e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x1f */

	PORT_START("pc_keyboard_2")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x20 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C")                       PORT_CODE(KEYCODE_C)          PORT_CHAR('C')                                             /* 0x21 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X")                       PORT_CODE(KEYCODE_X)          PORT_CHAR('X')                                             /* 0x22 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D")                       PORT_CODE(KEYCODE_D)          PORT_CHAR('D')                                             /* 0x23 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E")                       PORT_CODE(KEYCODE_E)          PORT_CHAR('E')                                             /* 0x24 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4 $")                     PORT_CODE(KEYCODE_4)          PORT_CHAR('4')                        PORT_CHAR('$')       /* 0x25 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3 #")                     PORT_CODE(KEYCODE_3)          PORT_CHAR('3')                        PORT_CHAR('#')       /* 0x26 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x27 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x28 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Space")                   PORT_CODE(KEYCODE_SPACE)      PORT_CHAR(' ')                                             /* 0x29 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V")                       PORT_CODE(KEYCODE_V)          PORT_CHAR('V')                                             /* 0x2a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F")                       PORT_CODE(KEYCODE_F)          PORT_CHAR('F')                                             /* 0x2b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T")                       PORT_CODE(KEYCODE_T)          PORT_CHAR('T')                                             /* 0x2c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R")                       PORT_CODE(KEYCODE_R)          PORT_CHAR('R')                                             /* 0x2d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5 %")                     PORT_CODE(KEYCODE_5)          PORT_CHAR('5')                        PORT_CHAR('%')       /* 0x2e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x2f */

	PORT_START("pc_keyboard_3")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x30 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N")                       PORT_CODE(KEYCODE_N)          PORT_CHAR('N')                                             /* 0x31 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B")                       PORT_CODE(KEYCODE_B)          PORT_CHAR('B')                                             /* 0x32 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")                       PORT_CODE(KEYCODE_H)          PORT_CHAR('H')                                             /* 0x33 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G")                       PORT_CODE(KEYCODE_G)          PORT_CHAR('G')                                             /* 0x34 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y")                       PORT_CODE(KEYCODE_Y)          PORT_CHAR('Y')                                             /* 0x35 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6 ^")                     PORT_CODE(KEYCODE_6)          PORT_CHAR('6')                        PORT_CHAR('^')       /* 0x36 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x37 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x38 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M")                       PORT_CODE(KEYCODE_M)          PORT_CHAR('M')                                             /* 0x39 -> 0x3a */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Caps")                    PORT_CODE(KEYCODE_CAPSLOCK)                                                              /* 0x3a -> 0x58 */ // This should be M but this is a workaround because pc_keyboard_device assumes scan code set 1 in polling()
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J")                       PORT_CODE(KEYCODE_J)          PORT_CHAR('J')                                             /* 0x3b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("U")                       PORT_CODE(KEYCODE_U)          PORT_CHAR('U')                                             /* 0x3c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7 &")                     PORT_CODE(KEYCODE_7)          PORT_CHAR('7')                        PORT_CHAR('&')       /* 0x3d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8 *")                     PORT_CODE(KEYCODE_8)          PORT_CHAR('8')                        PORT_CHAR('*')       /* 0x3e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x3f */

	PORT_START("pc_keyboard_4")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x40 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(", <")                     PORT_CODE(KEYCODE_COMMA)      PORT_CHAR(',')                        PORT_CHAR('<')       /* 0x41 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K")                       PORT_CODE(KEYCODE_K)          PORT_CHAR('K')                                             /* 0x42 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I")                       PORT_CODE(KEYCODE_I)          PORT_CHAR('I')                                             /* 0x43 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O")                       PORT_CODE(KEYCODE_O)          PORT_CHAR('O')                                             /* 0x44 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x45 */  // This should be 0 but this is a workaround because pc_keyboard_device assumes scan code set 1 in polling()
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9 (")                     PORT_CODE(KEYCODE_9)          PORT_CHAR('9')                        PORT_CHAR('(')       /* 0x46 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0 )")                     PORT_CODE(KEYCODE_0)          PORT_CHAR('0')                        PORT_CHAR(')')       /* 0x47 -> 0x45 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x48 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(". >")                     PORT_CODE(KEYCODE_STOP)       PORT_CHAR('.')                        PORT_CHAR('>')       /* 0x49 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/ ?")                     PORT_CODE(KEYCODE_SLASH)      PORT_CHAR('/')                        PORT_CHAR('?')       /* 0x4a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L")                       PORT_CODE(KEYCODE_L)          PORT_CHAR('L')                                             /* 0x4b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("; :")                     PORT_CODE(KEYCODE_COLON)      PORT_CHAR(';')                        PORT_CHAR(':')       /* 0x4c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P")                       PORT_CODE(KEYCODE_P)          PORT_CHAR('P')                                             /* 0x4d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("- _")                     PORT_CODE(KEYCODE_MINUS)      PORT_CHAR('-')                        PORT_CHAR('_')       /* 0x4e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x4f */

	PORT_START("pc_keyboard_5")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x50 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x51 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("' \"")                    PORT_CODE(KEYCODE_QUOTE)      PORT_CHAR('\'')                       PORT_CHAR('\"')      /* 0x52 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x53 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("[ {")                     PORT_CODE(KEYCODE_OPENBRACE)  PORT_CHAR('[')                        PORT_CHAR('{')       /* 0x54 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("= +")                     PORT_CODE(KEYCODE_EQUALS)     PORT_CHAR('=')                        PORT_CHAR('+')       /* 0x55 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x56 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x57 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x58 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Shift")             PORT_CODE(KEYCODE_RSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))                           /* 0x59 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Enter")                   PORT_CODE(KEYCODE_ENTER)      PORT_CHAR(0x0d)                                            /* 0x5a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("] }")                     PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']')                        PORT_CHAR('}')       /* 0x5b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x5c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("\\ |")                    PORT_CODE(KEYCODE_BACKSLASH)  PORT_CHAR('\\')                       PORT_CHAR('|')       /* 0x5d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x5e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x5f */

	PORT_START("pc_keyboard_6")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Power")           PORT_CODE(KEYCODE_F1)         PORT_CHAR(UCHAR_MAMEKEY(F1))                               /* 0x60 -> [Ctrl+Q] 0x14, 0x15, 0xf0, 0x15, 0xf0, 0x14 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Search")          PORT_CODE(KEYCODE_F3)         PORT_CHAR(UCHAR_MAMEKEY(F3))                               /* 0x61 -> 0xe0, 0x10 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Favorites")       PORT_CODE(KEYCODE_F2)         PORT_CHAR(UCHAR_MAMEKEY(F2))                               /* 0x62 -> 0xe0, 0x18 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Print")                                                                                                            /* 0x63 -> 0xe0, 0x1d */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Refresh")                                                                                                          /* 0x64 -> 0xe0, 0x20 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Back")                    PORT_CODE(KEYCODE_1_PAD)      PORT_CODE(KEYCODE_END)                                     /* 0x65 -> 0xe0, 0x38 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("<-- Delete")              PORT_CODE(KEYCODE_BACKSPACE)  PORT_CHAR(0x08)                                            /* 0x66 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Home")            PORT_CODE(KEYCODE_7_PAD)      PORT_CODE(KEYCODE_HOME)                                    /* 0x67 -> 0xe0, 0x3a */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Mail")            PORT_CODE(KEYCODE_F4)         PORT_CHAR(UCHAR_MAMEKEY(F4))                               /* 0x68 -> 0xe0, 0x48 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Player")          PORT_CODE(KEYCODE_F12)        PORT_CHAR(UCHAR_MAMEKEY(F12))                              /* 0x69 -> 0xe0, 0x50 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("OK/Keypad Enter")                                                                                                  /* 0x6a -> 0xe0, 0x5a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left")                    PORT_CODE(KEYCODE_LEFT)       PORT_CODE(KEYCODE_4_PAD)                                   /* 0x6b -> 0xe0, 0x68 */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Jump To Top/Home")                                                                                                 /* 0x6c -> 0xe0, 0x6c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x6d [Used for the MSNTV2's Senjin idle sequence: 0xe0, 0xf0, 0x14 */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x6e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x6f */

	PORT_START("pc_keyboard_7")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x70 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x71 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Down")                    PORT_CODE(KEYCODE_DOWN)       PORT_CODE(KEYCODE_2_PAD)                                   /* 0x72 -> 0xe0, 0x72 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x73 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right")                   PORT_CODE(KEYCODE_RIGHT)      PORT_CODE(KEYCODE_6_PAD)                                   /* 0x74 -> 0xe0, 0x74 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Up")                      PORT_CODE(KEYCODE_UP)         PORT_CODE(KEYCODE_8_PAD)                                   /* 0x75 -> 0xe0, 0x75 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Esc")                     PORT_CODE(KEYCODE_ESC)                                                                   /* 0x76 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x70 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Videos/F11")      PORT_CODE(KEYCODE_F5)         PORT_CHAR(UCHAR_MAMEKEY(F5))                               /* 0x78 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Volume +")                PORT_CODE(KEYCODE_INSERT)     PORT_CODE(KEYCODE_PLUS_PAD)                                /* 0x79 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Page Down")               PORT_CODE(KEYCODE_PGDN)       PORT_CODE(KEYCODE_3_PAD)                                   /* 0x7a -> 0xe0, 0x7a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Volume -")                PORT_CODE(KEYCODE_DEL)        PORT_CODE(KEYCODE_MINUS_PAD)          PORT_CHAR(0x7f)      /* 0x7b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x7c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Page Up")                 PORT_CODE(KEYCODE_PGUP)       PORT_CODE(KEYCODE_9_PAD)                                   /* 0x7d -> 0xe0, 0x7d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                                 /* 0x7e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("MSNTV2: Menu/F7")         PORT_CODE(KEYCODE_F11)        PORT_CHAR(UCHAR_MAMEKEY(F11))                              /* 0x7f -> 0x83 */

INPUT_PORTS_END

ioport_constructor pic12f629_irkbd_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(msntv2_irkbd);
}

/*

Below is the Intel HEX format of the PIC12F629 code pulled from the v1.387 BIOS. It IDs itself as 0x010d (likely just a version number).

This is fetched in memory from sub_0083f244 at *0x00b59e10 and is parsed in sub_0083e7cc and the result is stored *0x008e1a80.
It then is programmed to the PIC12F629 microcontroller on the board.

I'm not executing PIC code directly to simplify the MAME side (no need to create and pull in a PIC processor).

pic12f629_irkbd_device just tries to satisfy OS software expectations since the PIC code never seemed to change.

:020000040000FA
:100000008316FF2390000729D20003088312D30030
:100010003F18EE28BF18D228BF19CC28851EEE281D
:10002000BF15CC01CC2882070034003400340034E2
:100030000E340C3400340034003458342234D534B7
:1000400005341E341B341D340034043421340A3486
:10005000063426342334243411340D341A34D6347F
:10006000A03416341C34153432342C342A3434344D
:100070002E3425342B342D340034763400341434AB
:100080000034D0340034D1340034123459340034C4
:100090000034003479347B34293466345A34D7340C
:1000A000BA3490345D340034003400340034003409
:1000B00000340034D9340034EB340034EC34F534FB
:1000C000E9340034B8340034F2340034D234F1343A
:1000D000D334C834FD340034F43400340034FA34FA
:1000E000D434F134003400340034D834493400348A
:1000F000983446344B3444344A3454340034523403
:100100004E3445344C344D3400345B344134003487
:1001100055343E3442344334313435343A34333454
:1001200036343D343B343C34820734341B341A3487
:1001300044342D341C3415344D341D34313482078D
:10014000B8340034D6340034003400340034003481
:1001500000340034003400340034003400340034FF
:100160005A340034D4340034BA340034D334003434
:10017000F434FD34F5345A34EB34FA34F234820773
:100180001234593414341134CC0A05304C0203199A
:100190003F15EE28BF01EE28851AC4283F1DCA2846
:1001A000BF1481013E303E020319EE280130851E46
:1001B00000303E0601390319EB283E08203C031DA0
:1001C000E5280E300102031CEE283E0884000108D9
:1001D0008000BE0AEE280B1DF0283F1481010B1190
:1001E000CD0A0319CE0AC0308E008F030C105308BD
:1001F0008300D20E520E090020308B13BE00BF01C7
:10020000A0018B1708005630D400D5000800EA3052
:100210008500831285018316FE308500043081003D
:1002200001308C008312CA01C701CB0129220321AE
:10023000FC209001C0308E00FF308F008C01101424
:100240000C1081018B010B17FC20CB1B66224E0882
:1002500003103F1B4E0D043C031839298F30CB058A
:10026000C71F3729FC2044179130C5008322CA01DB
:10027000C7016400013085008501051EB2235408C2
:10028000550203194F2954088B13840000088B175B
:10029000D40A0023803903193929D40339290321C9
:1002A0002008031D56293F1C252924291F30200220
:1002B00003187029A0302102031C7029D1302102BB
:1002C000031870293C303E02031C7029A721031D2E
:1002D000702922308B13BE00BF013F178B172529D1
:1002E0003E303E020319BF173F18BF1748302002A7
:1002F000031C872959302002031887291030210256
:10030000031C87291E30210203188729D721BF1F0C
:1003100025292429400884000008C8000E3048021E
:10032000031CA5291E3048020318A529C00A40084D
:1003300084000008C8003D3CC20DC60AC00A490836
:100340004602031C8A2903150800031108003F1404
:10035000C6012030C000C2012230C0000630C900F2
:100360008A21031D08004208C300C2010D30C900E4
:100370008A2103304202031D08003B3043029830BB
:100380000319CC291F3043020318A52930304B052F
:10039000031DCF2943089F20C500C40183222922C1
:1003A0001030CB07CB1EA329CF30CB05A3291F309C
:1003B000C000C801C101C201C601CF012E224608FA
:1003C000031DEA2949080319062A3E034002031DBA
:1003D000DE29292A093046020318F7294318292A59
:1003E00049080310031D0314C20CCF07062A411C41
:1003F000FD29193042020319052A4F08013949061F
:100400000319292A2E22491C292AC60AC60A0B309A
:100410004602031CDE294208411CC4004118C500E5
:10042000C10AC11C242A4408BF39943C031D292A4F
:10043000FC204508031D1E2ACB17212A1320C500C6
:100440008322C501C10108003E034002031DDB29D0
:10045000292A8B13CD01CE018B1708004808031DF4
:10046000512A3E0340020319572AC00A2030400295
:10047000031D3D2A0330C800512A0130C800203036
:10048000C01C2A30C90040088B13840000088B1759
:10049000C30049084302031C512AC80A1830C9077F
:1004A000492AC80301304005C900C30108000130D2
:1004B000C900C3000800D03045029420C5004A0896
:1004C00045020319642AC216421708000F304B0573
:1004D00003197F2AC2010130C1004B0841050319ED
:1004E000782ACB064208BF20C500D430C400832A36
:1004F0000310C10DC20A04304202031D6D2ACB1342
:10050000CA01CB010800C2010130C1004208BF206E
:100510004502031D8E2A42174216962A0310C10D6A
:10052000C20A04304202031D862AC101F03045058B
:10053000D03C03195B2245080319E82A9030450294
:10054000031CA52A7F30C505C21758304502031980
:100550004217441FB72A4A0845020319CA01F0305E
:10056000C705421EEB2A4109CB05CA01DA2A14301D
:10057000C21AF3224108421ACB044A084502031961
:10058000C52AC701421FC717CE2A421BC701073021
:1005900047020F390319CE2AC70AE82A4508CA00BC
:1005A000C21FD42AE030F3224508F322421AE82A77
:1005B000C71BE82AC21FDE2AE030F322F030F32204
:1005C0004508F322C21EE82AF030F3221430F32249
:1005D0002922C5010800C71FE82ADA2AD100F32220
:1005E000F030F32251088B13D2005508603C0319F8
:1005F000FE2A5508840052088000D50A8B1708008F
:10060000D1006400051E012B04304E23072B051E6C
:10061000012B051DFF34CF010830C60000302B230D
:10062000051E2A2B0000142B5108CF062B23051E74
:100630002A2BD10CC60B142B00004F092B23051EAF
:100640002A2B222B232BFF302B2304304E23282B45
:1006500000000034FE340139031DFE300319FA3066
:1006600083168500831285018316362B372B382B92
:1006700000000508EE3985008312850183160430D9
:100680004E23422B00000508EE3910388500482B18
:10069000FE308500831200004D2B0800FF3E031D35
:1006A0004E2B0800D0016400051E532B0519AA34F7
:1006B0000830C600CF0104304E235E2B00009B2380
:1006C000051EBB340310D00CD004CF06C60B5F2B25
:1006D00000009B23051ECC34CF066E2B6F2B000031
:1006E0009B23051EDD34003A0319CF010319DD34C5
:1006F0008316FA308500831285017E2B7F2B000044
:100700008316EA30850083128501831604304E2358
:10071000892B0000FA308500831285018316902B07
:10072000912B922BFE3085008312CF1FFF340130B6
:100730004E239A2B00349C2B9D2B00008316EE3009
:10074000850083128501831604304E23A72B0000F9
:10075000FE308500831202304E23AE2B00000519B7
:10076000803400345223003E0319BB2BAA3C0319EA
:100770000800AA3E0800ED3050020319E02BF030CB
:1007800050020319E02BF23050020319D42BF3303E
:1007900050020319E02BEE3050020319E82BFF3012
:1007A00050020319DA2BE62BFA300023AB3000237A
:1007B0008330E92BFA300023051EDC2BAA30E92B0D
:1007C000FA3000236400051AE22B5223FA30E92B99
:0407D000EE30002BDC
:084000000000010000000D00AA
:02400E004C3F25
:00000001FF

*/

msntv2_fpanel_device::msntv2_fpanel_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, MSNTV2_FPANEL, tag, owner, clock),
	device_centronics_peripheral_interface(mconfig, *this),
	m_irkbd(*this, "irkbd"),
	m_power_led(*this, "power_led"),
	m_connect_led(*this, "connect_led"),
	m_message_led(*this, "message_led")
{
}

void msntv2_fpanel_device::device_start()
{
	m_power_led.resolve();
	m_connect_led.resolve();
	m_message_led.resolve();
}

void msntv2_fpanel_device::device_reset()
{
	m_pdata = 0x00;
	m_strobe = 0;

	m_power_led = 0;
	m_connect_led = 0;
	m_message_led = 0;
}

void msntv2_fpanel_device::device_add_mconfig(machine_config &config)
{
	PIC12F629_IRKBD(config, m_irkbd, 0);
	m_irkbd->gp4_callback().set(FUNC(msntv2_fpanel_device::output_busy));
}

void msntv2_fpanel_device::input_data0(int state)
{
	ecp_lpt_device::change_bit(state, 0, &m_pdata);

	m_power_led = !state;
}

void msntv2_fpanel_device::input_data1(int state)
{
	ecp_lpt_device::change_bit(state, 1, &m_pdata);

	m_connect_led = !state;
}

void msntv2_fpanel_device::input_data2(int state)
{
	ecp_lpt_device::change_bit(state, 2, &m_pdata);

	m_message_led = !state;
}

void msntv2_fpanel_device::input_data3(int state)
{
	ecp_lpt_device::change_bit(state, 3, &m_pdata);
}

void msntv2_fpanel_device::input_data4(int state)
{
	ecp_lpt_device::change_bit(state, 4, &m_pdata);
}

void msntv2_fpanel_device::input_data5(int state)
{
	ecp_lpt_device::change_bit(state, 5, &m_pdata);
}

void msntv2_fpanel_device::input_data6(int state)
{
	ecp_lpt_device::change_bit(state, 6, &m_pdata);

	msntv2_fpanel_device::out_gp3();
}

void msntv2_fpanel_device::input_data7(int state)
{
	ecp_lpt_device::change_bit(state, 7, &m_pdata);
}

void msntv2_fpanel_device::input_strobe(int state)
{
	m_strobe = state;

	msntv2_fpanel_device::out_gp3();
}

void msntv2_fpanel_device::input_select(int state)
{
	m_irkbd->set_gp1(state);
}

void msntv2_fpanel_device::input_autofd(int state)
{
	m_irkbd->set_gp0(state);
}

void msntv2_fpanel_device::out_gp3()
{
	if(m_strobe)
		m_irkbd->set_gp3((m_pdata >> msntv2_fpanel_device::PIC_GP3_PDATA_SHIFT) & 0x1);
}

