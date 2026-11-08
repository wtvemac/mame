// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvport_pronto.h"

DEFINE_DEVICE_TYPE(WTVPORT_PRONTO, wtvport_pronto_device, "wtvport_pronto", "WebTV Port Pronto Debug Board")

ROM_START(wtvport_pronto)
	// The ROM for this board is unknown to me.
	// I modified Goober code to at least test something. Goober=what I think is a precursor to this board that sits within the BootROM address space.
	// A debug build wont boot this unless it has 0x44696167 'Diag' @ +0x28 and 0xbb969e98 ~'Diag' @ +0x2c
	ROM_REGION32_BE(0x40000, "rom", ROMREGION_ERASEFF)
	ROM_LOAD("pronto.o", 0x000000, 0x040000, NO_DUMP)
ROM_END

static INPUT_PORTS_START(wtvport_pronto)
	PORT_START("CONFIG")
	PORT_CONFNAME(0x01, 0x00, "Boot into Pronto ROM (diag line)") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(wtvport_pronto_device::diag_changed), 0)
	PORT_CONFSETTING(0x00, DEF_STR(Off))
	PORT_CONFSETTING(0x01, DEF_STR(On))

	// This might be the SW0600 "interrupt switch" on the Pekoe2 board (MAN_INT_N). Needs to be active to boot into pronto.o
	PORT_START("BUTTON")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_OTHER) PORT_NAME("Boot into Pronto ROM Button") PORT_CHANGED_MEMBER(DEVICE_SELF, FUNC(wtvport_pronto_device::diag_changed), 0)
INPUT_PORTS_END

wtvport_pronto_device::wtvport_pronto_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVPORT_PRONTO, tag, owner, clock),
	device_wtvport_card_interface(mconfig, *this),
	m_pekoe(*this, "pekoe"),
	m_stream(*this, "stream"),
	m_config(*this, "CONFIG"),
	m_button(*this, "BUTTON")
{
}

void wtvport_pronto_device::device_add_mconfig(machine_config &config)
{
	WTV_PEKOEDBG(config, m_pekoe);
	BITBANGER(config, m_stream, 0);
}

const tiny_rom_entry *wtvport_pronto_device::device_rom_region() const
{
	return ROM_NAME(wtvport_pronto);
}

ioport_constructor wtvport_pronto_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(wtvport_pronto);
}

void wtvport_pronto_device::device_start()
{
	m_parallel_cntl = 0x0;
	m_parallel_data = 0x0;
	m_strobe_state = STROBE_IDLE;
	m_strobe_high_reads = 0;
	m_rx_buffer.fill(0);
	m_rx_head = 0;
	m_rx_count = 0;

	save_item(NAME(m_parallel_cntl));
	save_item(NAME(m_parallel_data));
	save_item(NAME(m_strobe_state));
	save_item(NAME(m_strobe_high_reads));
	save_item(NAME(m_rx_buffer));
	save_item(NAME(m_rx_head));
	save_item(NAME(m_rx_count));

	port().space().install_device(base(), base() + wtvport_device::SLOT_SIZE - 1, *this, &wtvport_pronto_device::map);
}

void wtvport_pronto_device::device_reset()
{
	m_parallel_cntl = 0x0;
	m_strobe_state = STROBE_IDLE;
	m_strobe_high_reads = 0;

	wtvport_pronto_device::update_diag();
}

INPUT_CHANGED_MEMBER(wtvport_pronto_device::diag_changed)
{
	wtvport_pronto_device::update_diag();
}

void wtvport_pronto_device::update_diag()
{
	// The diag line is held while the switch is on or the button is down.
	const bool asserted = (m_config->read() & CONFIG_DIAG) || (m_button->read() & BUTTON_DIAG);

	set_diag(asserted ? ASSERT_LINE : CLEAR_LINE);
}

void wtvport_pronto_device::map(address_map &map)
{
	map(0x000000, 0x7fffff).noprw();

	map(0x000000, ROM_SIZE - 1).mirror(ROM_MIRROR).rom().region("rom", 0);

	map(0x700000, 0x70001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));

	map(0x7fffd0, 0x7fffd3).rw(FUNC(wtvport_pronto_device::parallel_cntl_r), FUNC(wtvport_pronto_device::parallel_cntl_w));
	map(0x7fffe0, 0x7fffe3).r(FUNC(wtvport_pronto_device::parallel_data_r));
	map(0x7fffe8, 0x7fffeb).r(FUNC(wtvport_pronto_device::parallel_status_r));
	map(0x7fffec, 0x7fffef).w(FUNC(wtvport_pronto_device::parallel_ack_w));
	map(0x7ffff0, 0x7ffff3).r(FUNC(wtvport_pronto_device::device_id_r));
}

uint32_t wtvport_pronto_device::parallel_cntl_r()
{
	return m_parallel_cntl;
}

void wtvport_pronto_device::parallel_cntl_w(uint32_t data)
{
	m_parallel_cntl = data;
}

uint32_t wtvport_pronto_device::parallel_data_r()
{
	return m_parallel_data;
}

uint32_t wtvport_pronto_device::parallel_status_r()
{
	if (machine().side_effects_disabled())
		return (m_strobe_state == STROBE_HIGH) ? PARALLEL_STATUS_STROBE : 0x0;

	if (m_strobe_state == STROBE_IDLE)
	{
		uint8_t data;
		if (!wtvport_pronto_device::next_rx_byte(data))
			return 0x0;

		m_parallel_data = data;
		m_strobe_state = STROBE_HIGH;
		m_strobe_high_reads = 0;
	}

	if (m_strobe_state == STROBE_HIGH)
	{
		if (++m_strobe_high_reads >= STROBE_HIGH_READS)
			m_strobe_state = STROBE_LOW;

		return PARALLEL_STATUS_STROBE;
	}

	return 0x0;
}

void wtvport_pronto_device::parallel_ack_w(uint32_t data)
{
	// The WebTV OS acks once the strobe has dropped, then reads the data. The latch holds until the next strobe.
	if (m_strobe_state == STROBE_LOW)
		m_strobe_state = STROBE_IDLE;
}

uint32_t wtvport_pronto_device::device_id_r()
{
	return DEVICE_ID;
}

bool wtvport_pronto_device::next_rx_byte(uint8_t &data)
{
	if (m_rx_count == 0)
	{
		m_rx_head = 0;
		m_rx_count = m_stream->input(m_rx_buffer.data(), RX_BUFFER_SIZE);

		if (m_rx_count == 0)
			return false;
	}

	data = m_rx_buffer[m_rx_head++];
	m_rx_count--;

	return true;
}
