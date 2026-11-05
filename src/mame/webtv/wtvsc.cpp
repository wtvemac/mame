// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// The card side of a sucUnit Smart Card interface

#include "emu.h"
#include "wtvsc.h"

#include <bit>

DEFINE_DEVICE_TYPE(WTVSC, wtvsc_device, "wtvsc", "WebTV Smart Card Reader")

wtvsc_device::wtvsc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVSC, tag, owner, clock),
	m_bitb(*this, "bitb"),
	m_config(*this, "CONFIG"),
	m_mounted_cb(*this),
	m_internal_card(false),
	m_active(false),
	m_mounted(false),
	m_have_rx(false),
	m_rx(0),
	m_echo(0),
	m_line_reset(false),
	m_line_clock(false),
	m_line_host_enable(false),
	m_line_host_data(true),
	m_line_card_data(true),
	m_line_host_bit(LINE_IDLE),
	m_line_host_count(0),
	m_line_host_shift(0),
	m_line_card_bit(LINE_IDLE),
	m_line_card_count(0),
	m_line_card_frame(0),
	m_line_first(true),
	m_line_inverse(false)
{
}

static INPUT_PORTS_START(wtvsc)
	PORT_START("CONFIG")
	PORT_CONFNAME(0x03, 0x01, "Smart Card Reader Reset Line")
	PORT_CONFSETTING(   0x00, "None")
	PORT_CONFSETTING(   0x01, "RTS")
	PORT_CONFSETTING(   0x02, "DTR")
	PORT_CONFNAME(0x04, 0x00, "Smart Card Reader Reset Polarity")
	PORT_CONFSETTING(   0x00, "Asserted Holds Reset")
	PORT_CONFSETTING(   0x04, "Deasserted Holds Reset")
	PORT_CONFNAME(0x18, 0x00, "Smart Card Reader Detect Line")
	PORT_CONFSETTING(   0x00, "None (Always Inserted)")
	PORT_CONFSETTING(   0x08, "CTS")
	PORT_CONFSETTING(   0x10, "DCD")
	PORT_CONFSETTING(   0x18, "DSR")
	PORT_CONFNAME(0x20, 0x00, "Smart Card Reader Detect Polarity")
	PORT_CONFSETTING(   0x00, "Asserted When Inserted")
	PORT_CONFSETTING(   0x20, "Deasserted When Inserted")
	PORT_CONFNAME(0x40, 0x40, "Smart Card Reader Echoes Sent Bytes")
	PORT_CONFSETTING(   0x00, DEF_STR(No))
	PORT_CONFSETTING(   0x40, DEF_STR(Yes))
INPUT_PORTS_END

ioport_constructor wtvsc_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(wtvsc);
}

void wtvsc_device::device_add_mconfig(machine_config &config)
{
	BITBANGER(config, m_bitb, 0);
	m_bitb->mounted_callback().set(FUNC(wtvsc_device::bitb_mounted));
}

void wtvsc_device::device_start()
{
	save_item(NAME(m_active));
	save_item(NAME(m_have_rx));
	save_item(NAME(m_rx));
	save_item(NAME(m_echo));
	save_item(NAME(m_line_reset));
	save_item(NAME(m_line_clock));
	save_item(NAME(m_line_host_enable));
	save_item(NAME(m_line_host_data));
	save_item(NAME(m_line_card_data));
	save_item(NAME(m_line_host_bit));
	save_item(NAME(m_line_host_count));
	save_item(NAME(m_line_host_shift));
	save_item(NAME(m_line_card_bit));
	save_item(NAME(m_line_card_count));
	save_item(NAME(m_line_card_frame));
	save_item(NAME(m_line_first));
	save_item(NAME(m_line_inverse));
}

void wtvsc_device::device_reset()
{
	clear();

	m_line_reset = false;
	m_line_clock = false;
	m_line_host_enable = false;
	m_line_host_data = true;
	line_idle();
}

bool wtvsc_device::present()
{
	if (!m_mounted)
		return m_internal_card;

	const uint32_t config = m_config->read();

	uint32_t line;
	switch (config & CONFIG_DETECT_MASK)
	{
		case CONFIG_DETECT_CTS:
			line = bitbanger_device::LINE_CTS;
			break;
		case CONFIG_DETECT_DCD:
			line = bitbanger_device::LINE_DCD;
			break;
		case CONFIG_DETECT_DSR:
			line = bitbanger_device::LINE_DSR;
			break;
		default:
			return true;
	}

	// No modem lines (e.g. a pty or a file), so there's no way to tell
	uint32_t lines;
	if (m_bitb->get_lines(lines))
		return true;

	return bool(lines & line) != bool(config & CONFIG_DETECT_INVERT);
}

void wtvsc_device::clear()
{
	m_active = false;
	m_ca_card.clear();

	set_reset(true);

	m_have_rx = false;
	m_echo = 0;
}

void wtvsc_device::start()
{
	m_active = true;

	if (m_mounted)
	{
		// Drop anything left over from before the reset so the ATR is first
		m_bitb->purge();
		drop_input();

		set_reset(false);
	}
	else if (m_internal_card)
	{
		m_ca_card.start();
	}
}

void wtvsc_device::write(uint8_t data, bool inverse_convention)
{
	if (!m_mounted)
	{
		// Commands are taken as they were written to the FIFO
		if (m_internal_card)
			m_ca_card.write(data);
	}
	else
	{
		// The reader is a direct convention UART
		m_bitb->output(inverse_convention ? inverse(data) : data);

		if (m_config->read() & CONFIG_ECHO)
			m_echo++;
	}
}

bool wtvsc_device::peek(uint8_t &data, bool inverse_convention)
{
	if (!m_mounted)
	{
		if (!m_internal_card || !m_ca_card.m_reply_count)
			return false;

		// Replies are the decoded inverse convention bytes
		const uint8_t reply = m_ca_card.m_reply[m_ca_card.m_reply_head];
		data = inverse_convention ? reply : inverse(reply);
	}
	else
	{
		while (!m_have_rx)
		{
			uint8_t byte;
			if (!m_bitb->input(&byte, 1))
				return false;

			if (m_echo)
			{
				m_echo--;
			}
			else
			{
				m_rx = byte;
				m_have_rx = true;
			}
		}

		data = inverse_convention ? inverse(m_rx) : m_rx;

	}

	return true;
}

void wtvsc_device::pop()
{
	if (m_mounted)
	{
		m_have_rx = false;
	}
	else if (m_internal_card)
	{
		uint8_t data;
		m_ca_card.take(data);
	}
}

void wtvsc_device::bitb_mounted(int state)
{
	const bool active = m_active;

	m_ca_card.clear();
	m_mounted = state;
	m_have_rx = false;
	m_echo = 0;
	line_idle();

	// Hold the card in reset until the interface activates it
	if (m_mounted)
		set_reset(true);

	// A card swapped in while the interface is active needs a new ATR
	if (active)
		start();

	m_mounted_cb(state);
}

void wtvsc_device::set_reset(bool reset)
{
	if (!m_mounted)
		return;

	const uint32_t config = m_config->read();

	uint32_t line;
	switch (config & CONFIG_RESET_MASK)
	{
		case CONFIG_RESET_RTS:
			line = bitbanger_device::LINE_RTS;
			break;
		case CONFIG_RESET_DTR:
			line = bitbanger_device::LINE_DTR;
			break;
		default:
			return;
	}

	const bool asserted = reset != bool(config & CONFIG_RESET_INVERT);

	// Readers without modem lines only reset when opened
	m_bitb->set_lines(line, asserted ? line : 0);
}

void wtvsc_device::drop_input()
{
	uint8_t buffer[64];
	while (m_bitb->input(buffer, sizeof(buffer)))
	{
	}

	m_have_rx = false;
	m_echo = 0;
}

void wtvsc_device::line_reset_w(int state)
{
	if (bool(state) == m_line_reset)
		return;

	m_line_reset = state;
	line_idle();

	if (m_line_reset)
		start();
	else
		clear();
}

void wtvsc_device::line_clock_w(int state)
{
	const bool rising = state && !m_line_clock;
	m_line_clock = state;

	if (rising && m_line_reset && m_mounted)
		line_clock_edge();
}

void wtvsc_device::line_data_w(int enable, int state)
{
	m_line_host_enable = enable;
	m_line_host_data = state;
}

int wtvsc_device::line_data_r()
{
	// Open drain, either side can pull the line low
	return (!m_line_host_enable || m_line_host_data) && m_line_card_data;
}

void wtvsc_device::line_idle()
{
	m_line_card_data = true;
	m_line_host_bit = LINE_IDLE;
	m_line_host_count = 0;
	m_line_host_shift = 0;
	m_line_card_bit = LINE_IDLE;
	m_line_card_count = 0;
	m_line_card_frame = 0;
	m_line_first = true;
	m_line_inverse = false;
}

void wtvsc_device::line_clock_edge()
{
	const bool host_line = !m_line_host_enable || m_line_host_data;

	// Host to card: sample in the middle of each etu after the start bit's falling edge
	if (m_line_host_bit == LINE_IDLE)
	{
		// The card listens unless it's sending a character (guard time included)
		if (!host_line && (m_line_card_bit == LINE_IDLE || m_line_card_bit > LINE_PARITY_BIT))
		{
			m_line_host_bit = 0;
			m_line_host_count = LINE_ETU_CLOCKS / 2;
			m_line_host_shift = 0;
		}
	}
	else if (m_line_host_bit == LINE_HOST_WAIT_HIGH)
	{
		if (host_line)
			m_line_host_bit = LINE_IDLE;
	}
	else if (!--m_line_host_count)
	{
		m_line_host_count = LINE_ETU_CLOCKS;

		if (m_line_host_bit == 0 && host_line)
		{
			// Glitch, not a start bit
			m_line_host_bit = LINE_IDLE;
		}
		else if (m_line_host_bit < LINE_PARITY_BIT)
		{
			if (m_line_host_bit)
				m_line_host_shift |= uint16_t(host_line) << (m_line_host_bit - 1);

			m_line_host_bit++;
		}
		else
		{
			// The reader makes its own parity, the line byte is sent as is
			m_bitb->output(uint8_t(m_line_host_shift));
			if (m_config->read() & CONFIG_ECHO)
				m_echo++;

			m_line_host_bit = LINE_HOST_WAIT_HIGH;
		}
	}

	// Card to host: one character at a time while the host isn't sending
	if (m_line_card_bit == LINE_IDLE)
	{
		uint8_t data;
		if (m_line_host_bit == LINE_IDLE && host_line && peek(data, false))
		{
			pop();

			// The reader gives the line as a direct convention byte, so the TS of an inverse
			// card reads 0x03 and its parity (inverted with the data) comes out odd.
			if (m_line_first)
			{
				m_line_inverse = data == TS_INVERSE;
				m_line_first = false;
			}

			const bool parity = (std::popcount(data) & 1) != m_line_inverse;

			m_line_card_frame = uint16_t(data) << 1 | uint16_t(parity) << LINE_PARITY_BIT;
			m_line_card_bit = 0;
			m_line_card_count = LINE_ETU_CLOCKS;
			m_line_card_data = false;
		}
	}
	else if (!--m_line_card_count)
	{
		m_line_card_count = LINE_ETU_CLOCKS;
		m_line_card_bit++;

		if (m_line_card_bit <= LINE_PARITY_BIT)
		{
			m_line_card_data = BIT(m_line_card_frame, m_line_card_bit);
		}
		else if (m_line_card_bit < LINE_CARD_END)
		{
			m_line_card_data = true;
		}
		else
		{
			m_line_card_data = true;
			m_line_card_bit = LINE_IDLE;
		}
	}
}
