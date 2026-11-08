// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvport_printer.h"

DEFINE_DEVICE_TYPE(WTVPORT_PRINTER, wtvport_printer_device, "wtvport_printer", "WebTV Port Printer Adapter (MPR975)")

wtvport_printer_device::wtvport_printer_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVPORT_PRINTER, tag, owner, clock),
	device_wtvport_card_interface(mconfig, *this),
	m_pport(*this, "pport"),
	m_led(*this, "wtvport_printer_led"),
	m_pport_connected(false)
{
}

void wtvport_printer_device::device_add_mconfig(machine_config &config)
{
	ECP_LPT(config, m_pport);
	m_pport->set_solo_interface(true);
	m_pport->irq_handler().set(FUNC(wtvport_printer_device::pport_irq_w));
}

void wtvport_printer_device::connect_machine_pport(device_t *device, const char *pport_tag)
{
	wtvport_printer_device &board = downcast<wtvport_printer_device &>(*device);
	device_t &root = board.mconfig().root_device();

	centronics_device *const centronics = root.subdevice<centronics_device>(pport_tag);
	if (!centronics)
		throw emu_fatalerror("%s: no centronics slot '%s' for the WebTV Port printer board\n", board.tag(), pport_tag);

	for (device_t &other : device_enumerator(root))
	{
		wtvport_printer_device *const other_board = dynamic_cast<wtvport_printer_device *>(&other);
		if (other_board && other_board != &board && other_board->m_pport_connected)
			return;
	}

	board.m_pport.lookup()->connect_centronics(*centronics);
	board.m_pport_connected = true;
}

void wtvport_printer_device::device_start()
{
	m_board_cntl = 0x0;

	save_item(NAME(m_board_cntl));

	port().space().install_device(base(), base() + wtvport_device::SLOT_SIZE - 1, *this, &wtvport_printer_device::map);
}

void wtvport_printer_device::device_reset()
{
	m_board_cntl = 0x0;
	m_led = 0;
}

void wtvport_printer_device::map(address_map &map)
{
	map(0x000000, 0x7fffff).noprw();

	map(PPORT_OFFSET + 0x00, PPORT_OFFSET + 0x2f).m(m_pport, FUNC(ecp_lpt_device::solo_map));

	map(BOARD_CNTL_OFFSET, BOARD_CNTL_OFFSET + 3).rw(FUNC(wtvport_printer_device::board_cntl_r), FUNC(wtvport_printer_device::board_cntl_w));
	map(DEVICE_ID_OFFSET, DEVICE_ID_OFFSET + 3).r(FUNC(wtvport_printer_device::device_id_r));
}

void wtvport_printer_device::pport_irq_w(int state)
{
	wtvport_printer_device::set_irq(state);
}

uint32_t wtvport_printer_device::board_cntl_r()
{
	return m_board_cntl;
}

void wtvport_printer_device::board_cntl_w(uint32_t data)
{
	m_board_cntl = data;
	m_led = (m_board_cntl & BOARD_CNTL_LED) ? 1 : 0;
}

uint32_t wtvport_printer_device::device_id_r()
{
	return DEVICE_ID;
}
