// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "ps2_mouse.h"

DEFINE_DEVICE_TYPE(PS2_MOUSE, ps2_mouse_device, "ps2_mouse", "PS/2 Mouse Device")

ps2_mouse_device::ps2_mouse_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, PS2_MOUSE, tag, owner, clock),
	m_mousex_port(*this, "MOUSEX"),
	m_mousey_port(*this, "MOUSEY"),
	m_mousebtn_port(*this, "MOUSEBTN"),
	m_out_w_cb(*this)
{
}

void ps2_mouse_device::device_start()
{
	m_input_timer = timer_alloc(FUNC(ps2_mouse_device::poll_mouse), this);
}

void ps2_mouse_device::device_reset()
{
	ps2_mouse_device::reset();
}

void ps2_mouse_device::reset()
{
	ps2_mouse_device::enable_scanning(false);

	ps2_mouse_device::set_defaults();
}

void ps2_mouse_device::set_defaults()
{
	m_in_mode = in_mode_t::COMMAND;
	m_poll_rate = ps2_mouse_device::DEFAULT_POLL_RATE_HZ;
	m_resolution = 3;
	m_mouse_x = 0;
	m_mouse_y = 0;
	m_mouse_btn = 0;
}

void ps2_mouse_device::enable_scanning(bool enable)
{
	if (enable)
		m_input_timer->adjust(attotime::from_hz(m_poll_rate), 0, attotime::from_hz(m_poll_rate));
	else
		m_input_timer->adjust(attotime::never);
}

void ps2_mouse_device::in_command_w(uint8_t data)
{
	switch(data)
	{
		case 0xff: // Reset
			ps2_mouse_device::reset();
			m_out_w_cb(0xfa);
			m_out_w_cb(0xaa); // self-test good
			m_out_w_cb(0x00); // device id
			break;

		case 0xf2: // Get id
			m_out_w_cb(0xfa);
			m_out_w_cb(0x00); // device id
			break;

		case 0xf3: // Set sample rate
			m_in_mode = in_mode_t::POLL_RATE;
			m_out_w_cb(0xfa);
			break;

		case 0xf4: // Enable reporting
			ps2_mouse_device::enable_scanning(true);
			m_out_w_cb(0xfa);
			break;

		case 0xf5: // Disable reporting
			ps2_mouse_device::enable_scanning(false);
			m_out_w_cb(0xfa);
			break;

		case 0xf6: // Set defaults
			m_out_w_cb(0xfa);
			break;

		case 0xe6: // Set scaling 1:1
			m_out_w_cb(0xfa);
			break;

		case 0xe7: // Set scaling 2:1
			m_out_w_cb(0xfa);
			break;

		case 0xe8: // Set resolution
			m_resolution = in_mode_t::RESOLUTION;
			m_out_w_cb(0xfa);
			break;

		case 0xe9: // Status request
			m_out_w_cb(0xfa);
			m_out_w_cb(0x00); // mode
			m_out_w_cb(m_resolution);
			m_out_w_cb(m_poll_rate);
			break;

		default: // Error out?
			m_out_w_cb(0xfa);
			break;
	}
}

void ps2_mouse_device::in_poll_rate_w(uint8_t data)
{
	m_poll_rate = data;

	m_out_w_cb(0xfa);

	m_in_mode = in_mode_t::COMMAND;
}

void ps2_mouse_device::in_resolution_w(uint8_t data)
{
	m_resolution = data; // Not used

	m_out_w_cb(0xfa);

	m_in_mode = in_mode_t::COMMAND;
}

void ps2_mouse_device::in_w(uint8_t data)
{
	switch(m_in_mode)
	{
		case in_mode_t::POLL_RATE:
			ps2_mouse_device::in_poll_rate_w(data);
			break;

		case in_mode_t::RESOLUTION:
			ps2_mouse_device::in_resolution_w(data);
			break;

		case in_mode_t::COMMAND:
		default:
			ps2_mouse_device::in_command_w(data);
			break;
	}
}

TIMER_CALLBACK_MEMBER(ps2_mouse_device::poll_mouse)
{
	uint16_t x = m_mousex_port->read();
	uint16_t y = m_mousey_port->read();
	uint8_t buttons = m_mousebtn_port->read();

	uint16_t old_mouse_x = m_mouse_x;
	uint16_t old_mouse_y = m_mouse_y;
	uint16_t old_mouse_btn = m_mouse_btn;

	if(m_mouse_x == 0xffff)
	{
		old_mouse_x = x & 0x3ff;
		old_mouse_y = y & 0x3ff;
		old_mouse_btn = buttons;
	}

	m_mouse_x = x & 0x3ff;
	m_mouse_y = y & 0x3ff;
	m_mouse_btn = buttons;

	uint16_t dx = m_mouse_x - old_mouse_x;
	uint16_t dy = old_mouse_y - m_mouse_y;

	if (true||dx != 0 || dy != 0 || buttons != old_mouse_btn)
	{
		//m_out_w_cb(buttons | 0x08 | (BIT(dx, 8) << 4) | (BIT(dy, 8) << 5));
		//m_out_w_cb(dx & 0xff);
		//m_out_w_cb(dy & 0xff);
	}
}

INPUT_PORTS_START(ps2_mouse)
	PORT_START("MOUSEX")
	PORT_BIT(0x3ff, 0x000, IPT_MOUSE_X) PORT_SENSITIVITY(100) PORT_MINMAX(0x000, 0x3ff) PORT_KEYDELTA(2)

	PORT_START("MOUSEY")
	PORT_BIT(0x3ff, 0x000, IPT_MOUSE_Y) PORT_SENSITIVITY(100) PORT_MINMAX(0x000, 0x3ff) PORT_KEYDELTA(2)

	PORT_START("MOUSEBTN")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_CODE(MOUSECODE_BUTTON1) PORT_NAME("Mouse Button 1")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_CODE(MOUSECODE_BUTTON2) PORT_NAME("Mouse Button 2")
	PORT_BIT(0xfc, IP_ACTIVE_HIGH, IPT_UNUSED)
INPUT_PORTS_END

ioport_constructor ps2_mouse_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(ps2_mouse);
}