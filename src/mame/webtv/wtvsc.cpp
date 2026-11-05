// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// The card side of a sucUnit Smart Card interface

#include "emu.h"
#include "wtvsc.h"

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
	m_echo(0)
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
}

void wtvsc_device::device_reset()
{
	clear();
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
