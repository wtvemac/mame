// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/bus/isa/

// Description here

#include "emu.h"
#include "lpc47m192.h"

DEFINE_DEVICE_TYPE(LPC47M192_KBDC,  lpc47m192_kbdc_device,  "lpc47m192_kbdc", "SMSC LPC47M192 8042-like Keyboard/Mouse Controller")

lpc47m192_kbdc_device::lpc47m192_kbdc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, LPC47M192_KBDC, tag, owner, clock),
	m_kbd_in_w_cb(*this),
	m_mse_in_w_cb(*this),
	m_system_reset_cb(*this),
	m_gate_a20_cb(*this),
	m_keybd_output_buffer_full_cb(*this),
	m_mouse_output_buffer_full_cb(*this)
{
}

void lpc47m192_kbdc_device::device_start()
{
	m_buffer_checker = timer_alloc(FUNC(lpc47m192_kbdc_device::buffer_check), this);
}

void lpc47m192_kbdc_device::device_reset()
{
	m_xt_translate_hit_break = false;
	m_xt_translate_hit_extended = false;

	m_devout_head = 0;
	m_devout_tail = 0;

	std::fill(std::begin(m_ram), std::end(m_ram), 0);
	m_ram_offset = 0x00;

	m_status = lpc47m192_kbdc_device::KBDC_STATUS_SELFTEST_PASS | lpc47m192_kbdc_device::KBDC_STATUS_KEYBD_PASS;

	lpc47m192_kbdc_device::reset_config_flags();
}

uint8_t status_check = 0x00;
uint32_t status_repeat = 0;

uint8_t lpc47m192_kbdc_device::data_60r(offs_t offset)
{
	status_check = 0x00;

	return lpc47m192_kbdc_device::dequeue_output();
}

void lpc47m192_kbdc_device::data_60w(offs_t offset, uint8_t data)
{
	status_check = 0x00;

	lpc47m192_kbdc_device::enqueue_input(data, false);

	uint8_t data_in = lpc47m192_kbdc_device::dequeue_input();

	switch(m_60w_dest)
	{
		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_RAM:
		{
			if(m_ram_offset == 0)
			{
				//machine().debug_break();
			}

			lpc47m192_kbdc_device::write_internal_ram(m_ram_offset, data_in);
			break;
		}

		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_CNTL_OUT:
			break;

		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_KEYBD_OUT:
			lpc47m192_kbdc_device::enqueue_output(data_in, false);
			break;

		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_MOUSE_OUT:
			lpc47m192_kbdc_device::enqueue_output(data_in, true);
			break;

		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_KEYBD_IN:
		default:
			if(m_keyboard_enabled)
				m_kbd_in_w_cb(data_in);
			else
				lpc47m192_kbdc_device::set_device_missing(false);
			break;

		case kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_MOUSE_IN:
			if(m_mouse_enabled)
				m_mse_in_w_cb(data_in);
			else
				lpc47m192_kbdc_device::set_device_missing(true);
			break;
	}

	m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_KEYBD_IN;
}

uint8_t lpc47m192_kbdc_device::status_64r(offs_t offset)
{
	if(status_check != m_status)
		status_check = m_status;
	else
		status_repeat++;

	return m_status;
}

void lpc47m192_kbdc_device::command_64w(offs_t offset, uint8_t data)
{
	status_check = 0x00;

	lpc47m192_kbdc_device::enqueue_input(data, true);

	uint8_t data_in = lpc47m192_kbdc_device::dequeue_input();

	switch(data_in)
	{
		case lpc47m192_kbdc_device::KBDC_CMD_READ_RAM_MIN ... lpc47m192_kbdc_device::KBDC_CMD_READ_RAM_MAX:
			m_ram_offset = data_in & (lpc47m192_kbdc_device::KBDC_RAM_SIZE - 1);
			if(m_ram_offset == 0)
			{
				//machine().debug_break();
			}
			lpc47m192_kbdc_device::enqueue_output(lpc47m192_kbdc_device::read_internal_ram(data_in));
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_WRITE_RAM_MIN ... lpc47m192_kbdc_device::KBDC_CMD_WRITE_RAM_MAX:
			m_ram_offset = data_in & (lpc47m192_kbdc_device::KBDC_RAM_SIZE - 1);
			m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_RAM;
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_DIAG_DUMP:
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_GET_FIRMWARE_VERSION:
			lpc47m192_kbdc_device::enqueue_output(lpc47m192_kbdc_device::KBDC_FIRMWARE_VERSION);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_SELF_TEST:
			lpc47m192_kbdc_device::reset_config_flags();
			lpc47m192_kbdc_device::enqueue_output(lpc47m192_kbdc_device::KBDC_SELF_TEST_PASSED);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_CNTL_INPUT_READ: ///
			lpc47m192_kbdc_device::enqueue_output(0x00);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_CNTL_INPUT_HNIBBLE_POLL: ///
			lpc47m192_kbdc_device::enqueue_output(0x00);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_CNTL_INPUT_LNIBBLE_POLL: ///
			lpc47m192_kbdc_device::enqueue_output(0x00);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_KEYBD_ENABLE:
			lpc47m192_kbdc_device::clear_config_flags(lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_CLOCK_OFF);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_KEYBD_DISABLE:
			lpc47m192_kbdc_device::set_config_flags(lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_CLOCK_OFF);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_KEYBD_TEST:
			lpc47m192_kbdc_device::enqueue_output(lpc47m192_kbdc_device::KBDC_PS2_TEST_PASSED);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_CNTL_OUTPUT_READ: ///
			lpc47m192_kbdc_device::enqueue_output(0x00);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_CNTL_OUTPUT_WRITE: ///
			m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_CNTL_OUT;
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_KEYBD_OUTPUT_WRITE:
			m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_KEYBD_OUT;
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_MOUSE_OUTPUT_WRITE:
			m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_KEYBD_OUT;
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_MOUSE_INPUT_WRITE:
			m_60w_dest = kbdc_data_60w_dest_t::KBDC_DATA_60W_TO_PS2_MOUSE_IN;
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_MOUSE_ENABLE:
			lpc47m192_kbdc_device::clear_config_flags(lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_CLOCK_OFF);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_MOUSE_DISABLE:
			lpc47m192_kbdc_device::set_config_flags(lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_CLOCK_OFF);
			break;

		case lpc47m192_kbdc_device::KBDC_CMD_PS2_MOUSE_TEST:
			lpc47m192_kbdc_device::enqueue_output(lpc47m192_kbdc_device::KBDC_PS2_TEST_PASSED);

		case lpc47m192_kbdc_device::KBDC_CMD_PULSE_MIN ... lpc47m192_kbdc_device::KBDC_CMD_PULSE_MAX:
			break;

		default:
			break;
	}
}

bool lpc47m192_kbdc_device::translate_to_pcxt(uint8_t* data)
{
	if(*data == lpc47m192_kbdc_device::XT_TRANSLATE_EXTENDED_MARKER)
	{
		m_xt_translate_hit_extended = true;

		return true;
	}
	else if(*data == lpc47m192_kbdc_device::XT_TRANSLATE_AT_BREAK_MARKER)
	{
		m_xt_translate_hit_break = true;

		return false;
	}
	else
	{
		uint8_t new_data = m_xt_translate_table[*data & (lpc47m192_kbdc_device::XT_TRANSLATE_TABLE_SIZE - 1)];

		if(new_data != 0x00)
			*data = new_data;

		if(m_xt_translate_hit_break)
			*data |= lpc47m192_kbdc_device::XT_TRANSLATE_BREAK_MODIFIER;

		m_xt_translate_hit_extended = false;
		m_xt_translate_hit_break = false;

		return true;
	}

}

void lpc47m192_kbdc_device::keyboard_out_w(uint8_t data)
{
	if(m_keyboard_enabled)
	{
		if(m_ram[lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET] & lpc47m192_kbdc_device::KBDC_CONFIG_PCXT_EN)
		{
			if(!lpc47m192_kbdc_device::translate_to_pcxt(&data))
				return;
		}

		lpc47m192_kbdc_device::enqueue_output(data, false);
	}
}

void lpc47m192_kbdc_device::mouse_out_w(uint8_t data)
{
	if(m_mouse_enabled)
		lpc47m192_kbdc_device::enqueue_output(data, true);
}

void lpc47m192_kbdc_device::set_device_missing(bool mouse)
{
	lpc47m192_kbdc_device::enqueue_output(kbdc_ps2_response_t::KBDC_PS2_RESPONSE_ERROR_RESEND, mouse);

	if(mouse)
		m_status |= lpc47m192_kbdc_device::KBDC_STATUS_MOUSE_OUT;

	m_status |= lpc47m192_kbdc_device::KBDC_STATUS_IN_TIMEOUT;
}


void lpc47m192_kbdc_device::reset_config_flags()
{
	lpc47m192_kbdc_device::write_internal_ram(
		lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET,
		lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_CLOCK_OFF | lpc47m192_kbdc_device::KBDC_CONFIG_MOUSE_CLOCK_OFF
	);
}

uint8_t lpc47m192_kbdc_device::queue_size()
{
	if(m_devout_tail > m_devout_head)
		return (lpc47m192_kbdc_device::DEVOUT_QUEUE_SIZE - (m_devout_tail - m_devout_head));
	else
		return (m_devout_head - m_devout_tail);
}

bool lpc47m192_kbdc_device::queue_empty()
{
	return (m_devout_head == m_devout_tail);
}

void lpc47m192_kbdc_device::enqueue_output(uint8_t data, bool mouse)
{

	queue_item_t item = {
		.scancode = data,
		.mouse = mouse
	};

	m_devout_queue[m_devout_head++ & (lpc47m192_kbdc_device::DEVOUT_QUEUE_SIZE - 1)] = item;
	m_status |= lpc47m192_kbdc_device::KBDC_STATUS_OUTPUT_FULL;

	lpc47m192_kbdc_device::set_output_irq(item, ASSERT_LINE);
}

uint8_t lpc47m192_kbdc_device::dequeue_output()
{
	if(lpc47m192_kbdc_device::queue_empty())
	{
		return 0x00;
	}
	else
	{
		queue_item_t item = m_devout_queue[m_devout_tail++ & (lpc47m192_kbdc_device::DEVOUT_QUEUE_SIZE - 1)];

		lpc47m192_kbdc_device::set_output_irq(item, CLEAR_LINE);

		m_buffer_checker->adjust(attotime::from_usec(lpc47m192_kbdc_device::BUFFER_CHECK_TIME));

		return item.scancode;
	}

}

void lpc47m192_kbdc_device::set_output_irq(queue_item_t item, int state)
{
	if(state == ASSERT_LINE)
	{
		m_status |= lpc47m192_kbdc_device::KBDC_STATUS_OUTPUT_FULL;

		if(item.mouse)
			m_status |= lpc47m192_kbdc_device::KBDC_STATUS_MOUSE_OUT;
		else
			m_status &= (~(lpc47m192_kbdc_device::KBDC_STATUS_MOUSE_OUT));
	}
	else
	{
		m_status &= (~lpc47m192_kbdc_device::KBDC_STATUS_OUTPUT_FULL);
		m_status &= (~(lpc47m192_kbdc_device::KBDC_STATUS_MOUSE_OUT));
	}

	if(item.mouse)
	{
		if(m_ram[lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET] & lpc47m192_kbdc_device::KBDC_CONFIG_MOUSE_INT_EN)
			m_mouse_output_buffer_full_cb(state);
	}
	else
	{
		if(m_ram[lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET] & lpc47m192_kbdc_device::KBDC_CONFIG_KEYBD_INT_EN)
			m_keybd_output_buffer_full_cb(state);
	}
}

void lpc47m192_kbdc_device::enqueue_input(uint8_t data, bool is_command)
{
	m_status &= (~(lpc47m192_kbdc_device::KBDC_STATUS_IN_TIMEOUT));

	if(is_command)
		m_status |= lpc47m192_kbdc_device::KBDC_STATUS_INPUT_IS_CMD;
	else
		m_status &= (~lpc47m192_kbdc_device::KBDC_STATUS_INPUT_IS_CMD);

	if(!(m_status & lpc47m192_kbdc_device::KBDC_STATUS_INPUT_FULL))
	{
		m_status |= lpc47m192_kbdc_device::KBDC_STATUS_INPUT_FULL;

		m_data_in = data;
	}
}

uint8_t lpc47m192_kbdc_device::dequeue_input()
{
	uint8_t item = m_data_in;

	m_status &= (~lpc47m192_kbdc_device::KBDC_STATUS_INPUT_FULL);
	m_data_in = 0x00;

	return item;
}

void lpc47m192_kbdc_device::set_config_flags(uint8_t command_bits)
{
	uint8_t command_byte = lpc47m192_kbdc_device::read_internal_ram(lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET);

	command_byte |= command_bits;

	lpc47m192_kbdc_device::write_internal_ram(lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET, command_byte);
}

void lpc47m192_kbdc_device::clear_config_flags(uint8_t command_bits)
{
	uint8_t command_byte = lpc47m192_kbdc_device::read_internal_ram(lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET);

	command_byte &= (~command_bits);

	lpc47m192_kbdc_device::write_internal_ram(lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET, command_byte);
}

bool lpc47m192_kbdc_device::check_config_flags(uint8_t command_bits)
{
	uint8_t command_byte = lpc47m192_kbdc_device::read_internal_ram(lpc47m192_kbdc_device::KBDC_CONFIG_RAM_OFFSET);

	return ((command_byte & command_bits) == command_bits);
}

uint8_t lpc47m192_kbdc_device::read_internal_ram(offs_t offset)
{
	offset &= (lpc47m192_kbdc_device::KBDC_RAM_SIZE - 1);

	return m_ram[offset];
}

void lpc47m192_kbdc_device::write_internal_ram(offs_t offset, uint8_t data)
{
	offset &= (lpc47m192_kbdc_device::KBDC_RAM_SIZE - 1);

	m_ram[offset] = data;
}

TIMER_CALLBACK_MEMBER(lpc47m192_kbdc_device::buffer_check)
{
	if(!lpc47m192_kbdc_device::queue_empty())
	{
		queue_item_t next_item = m_devout_queue[m_devout_tail & (lpc47m192_kbdc_device::DEVOUT_QUEUE_SIZE - 1)];

		lpc47m192_kbdc_device::set_output_irq(next_item, ASSERT_LINE);
	}
}