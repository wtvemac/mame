// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

//
// "Pronto" or Pekoe2 WebTV Port debug board [Pekoe2_Schematic.pdf] (has WebTV Port device id = 1)
//
// Register layout:
//
//   base+0x000000: Code/ROM, this is jumped to from a debug ROM if this has 0x44696167 'Diag' @ +0x28 and 0xbb969e98 ~'Diag' @ +0x2c
//
//   base+0x700000-0x70001f: Pekoe UART, follows wtvdbg_pekoe_device and is also on the Italian box
//
// The parallel port is receive only, there doesn't seem to be bidirectional control. Because of this a bitbanger serial device is attached to this rather than a centronics device.
//
//   base+0x7fffd0: Parallel control
//   base+0x7fffe0: Parallel data (low byte).
//   base+0x7fffe8: Parallel status, bit 0 = host strobe.
//   base+0x7fffec: Parallel ack
//
// There's a PCMIA slot in the Pekoe2_Schematic.pdf schematic but it is unknown to me where the registers for that lands.
//
//   base+0x7ffff0: device ID (1)
//

#ifndef MAME_WEBTV_WTVPORT_PEKOE2_H
#define MAME_WEBTV_WTVPORT_PEKOE2_H

#pragma once

#include "wtvport.h"
#include "wtvdbg.h"
#include "imagedev/bitbngr.h"

#include <array>

class wtvport_pekoe2_device : public device_t, public device_wtvport_card_interface
{

public:

	wtvport_pekoe2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	DECLARE_INPUT_CHANGED_MEMBER(diag_changed);

protected:

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	static constexpr uint32_t DEVICE_ID = 0x00000001;

	static constexpr offs_t ROM_SIZE   = 0x00040000;
	static constexpr offs_t ROM_MIRROR = 0x003c0000;

	static constexpr uint32_t PARALLEL_STATUS_STROBE = 0x00000001;
	// The OS wants to see the strobe high on two reads in a row, then low on two reads in a row.
	static constexpr uint32_t STROBE_HIGH_READS      = 2;

	static constexpr uint32_t RX_BUFFER_SIZE = 0x1000;

	static constexpr uint32_t CONFIG_DIAG = 0x01;
	static constexpr uint32_t BUTTON_DIAG = 0x01;

	enum strobe_state_t : uint8_t
	{
		STROBE_IDLE, // no byte presented
		STROBE_HIGH, // byte latched, strobe asserted
		STROBE_LOW   // strobe dropped, waiting for an ack of 0x00
	};

	required_device<wtvdbg_pekoe_device> m_pekoe;
	required_device<bitbanger_device> m_stream;
	required_ioport m_config;
	required_ioport m_button;

	void update_diag();

	uint32_t m_parallel_cntl;
	uint8_t m_parallel_data;
	uint8_t m_strobe_state;
	uint32_t m_strobe_high_reads;

	std::array<uint8_t, RX_BUFFER_SIZE> m_rx_buffer;
	uint32_t m_rx_head;
	uint32_t m_rx_count;

	void map(address_map &map) ATTR_COLD;

	uint32_t parallel_cntl_r();
	void parallel_cntl_w(uint32_t data);
	uint32_t parallel_data_r();
	uint32_t parallel_status_r();
	void parallel_ack_w(uint32_t data);
	uint32_t device_id_r();

	bool next_rx_byte(uint8_t &data);

};

DECLARE_DEVICE_TYPE(WTVPORT_PEKOE2, wtvport_pekoe2_device)

#endif // MAME_WEBTV_WTVPORT_PEKOE2_H
