// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

//
// Retail WebTV Port printer/parallel port board for the original classic boxes (has WebTV Port device id = 2)
//
// Register layout:
//
// These seem to function exactly the same as SOLO's built-in parallel port registers and seem to live on the ShastaII ASIC.
//
//   base+0x740000: data
//   base+0x740004: control
//   base+0x740008: status
//
//   base+0x7fffcc: board control: FindCard writes 7, bit 3 = parallel LED
//   base+0x7ffff0: device ID (2)
//
// This will expose a -pport option for SPOT boxes. On SOLO boxes -pport will go to the built-in parallel port.
//

#ifndef MAME_WEBTV_WTVPORT_PRINTER_H
#define MAME_WEBTV_WTVPORT_PRINTER_H

#pragma once

#include "wtvport.h"
#include "ecp_lpt.h"

class wtvport_printer_device : public device_t, public device_wtvport_card_interface
{

public:

	wtvport_printer_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// Allows this slot to pick up wtvmame's -pport option
	static void connect_machine_pport(device_t *device, const char *pport_tag);

protected:

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	static constexpr uint32_t DEVICE_ID = 0x00000002;

	static constexpr offs_t PPORT_OFFSET      = 0x740000;
	static constexpr offs_t BOARD_CNTL_OFFSET = 0x7fffcc;
	static constexpr offs_t DEVICE_ID_OFFSET  = 0x7ffff0;

	static constexpr uint32_t BOARD_CNTL_LED = 0x00000008;

	required_device<ecp_lpt_device> m_pport;
	output_finder<> m_led;

	bool m_pport_connected;
	uint32_t m_board_cntl;

	void map(address_map &map) ATTR_COLD;

	void pport_irq_w(int state);

	uint32_t board_cntl_r();
	void board_cntl_w(uint32_t data);
	uint32_t device_id_r();

};

DECLARE_DEVICE_TYPE(WTVPORT_PRINTER, wtvport_printer_device)

#endif // MAME_WEBTV_WTVPORT_PRINTER_H
