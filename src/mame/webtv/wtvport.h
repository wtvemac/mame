// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

//
// WebTV defines up to 7 WebTV Port slots (0-6). Each device is given to 8MB of memory each.
//
// Device Slot 0: SOLO=0x04800000(base)-0x04ffffff(end), SPOT=0x00800000(base)-0x00ffffff(end)
// Device Slot 1: SOLO=0x05000000(base)-0x057fffff(end), SPOT=0x01000000(base)-0x017fffff(end)
// Device Slot 2: SOLO=0x05800000(base)-0x05ffffff(end), SPOT=0x01800000(base)-0x01ffffff(end)
// Device Slot 3: SOLO=0x06000000(base)-0x067fffff(end), SPOT=0x02000000(base)-0x027fffff(end)
// Device Slot 4: SOLO=0x06800000(base)-0x06ffffff(end), SPOT=0x02800000(base)-0x02ffffff(end)
// Device Slot 5: SOLO=0x07000000(base)-0x077fffff(end), SPOT=0x03000000(base)-0x037fffff(end)
// Device Slot 6: SOLO=0x07800000(base)-0x07ffffff(end), SPOT=0x03800000(base)-0x03ffffff(end)
//
// SOLO boxes can address slot 0-2 easily, SPOT all 7 devices seem to be easily addressable.
//
// Devices are daisy chained and can live directly on the board (Italian box appears to have Pekoe on board) and/or from a WebTV port on the box
//
// The WebTV OS probes each slot by reading a device ID at base+0x007ffff0, a read would time out on real hardware. Simplified WebTV Port device map based on what I've seen:
//
// base+0x00000000: Code entry point/ROM base, jumped to via JumpToPronto. Seems to allow for a max of a 4MB ROM.
// base+0x00000028: Needs to be 0x44696167 ('Diag') if this is a valid Pronto (device id=1) ROM
// base+0x0000002c: Needs to be 0xbb969e98 (~'Diag') if this is a valid Pronto (device id=1) ROM
// base+0x007ffff0: Device ID (u32), low byte (+0x03) is what the OS reads from ProbeExpansionBus
//                  Device ID of 1 is a Pronto debug board with a ShastaII chip. Probably is or is similar to what is in Pekoe2_Schematic.pdf.
//                                 Also the Goober ROM in the pre-alpha source code is probably an early version of this before the WebTV Port existed (it sits in the BootROM address space)
//                  Device ID of 2 is the retail printer board with a ShastaII chip. No ROM is jumped to with this but there is a ROM footprint on the board.
//
// The board registers seem to sit within base+0x007fffc0 and base+0x007fffff
//
// The device id and a lot of the registers seem to come from the ShastaII chip
//


#ifndef MAME_WEBTV_WTVPORT_H
#define MAME_WEBTV_WTVPORT_H

#pragma once

class device_wtvport_card_interface;

class wtvport_device : public device_t
{

public:

	static constexpr uint32_t SLOT_COUNT = 7;
	static constexpr offs_t   SLOT_SIZE  = 0x00800000; // 8MB

	wtvport_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	template <typename T> void set_space(T &&tag, int spacenum, offs_t base) { m_space.set_tag(std::forward<T>(tag), spacenum); m_base = base; }

	auto irq_callback() { return m_irq_cb.bind(); }
	auto diag_callback() { return m_diag_cb.bind(); }
	auto timeout_callback() { return m_timeout_cb.bind(); }

	address_space &space() const { return *m_space; }
	offs_t slot_base(uint32_t slot) const { return m_base + slot * SLOT_SIZE; }

	void set_slot_irq(uint32_t slot, int state);
	void set_slot_diag(uint32_t slot, int state);

	// Puts a slot back to empty (accesses time out). For a card that can be switched off at reset.
	void install_empty(uint32_t slot);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	required_address_space m_space;
	offs_t m_base;

	devcb_write_line m_irq_cb;
	devcb_write_line m_diag_cb;
	devcb_write32 m_timeout_cb;

	uint32_t m_irq_state;
	uint32_t m_diag_state;

	uint32_t empty_r(offs_t offset, uint32_t mem_mask);
	void empty_w(offs_t offset, uint32_t data, uint32_t mem_mask);

};

class wtvport_slot_device : public device_t, public device_single_card_slot_interface<device_wtvport_card_interface>
{

public:

	template <typename T, typename U>
	wtvport_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, T &&port_tag, uint32_t slot, U &&opts, const char *dflt) :
		wtvport_slot_device(mconfig, tag, owner)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
		m_port.set_tag(std::forward<T>(port_tag));
		m_slot = slot;
	}

	wtvport_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	wtvport_device &port() const { return *m_port; }
	uint32_t slot() const { return m_slot; }

protected:

	virtual void device_start() override ATTR_COLD;

private:

	required_device<wtvport_device> m_port;
	uint32_t m_slot;

};

class device_wtvport_card_interface : public device_interface
{

public:

	virtual ~device_wtvport_card_interface();

	// For a card that's built in rather than plugged into a wtvport_slot_device
	template <typename T> void set_wtvport(T &&port_tag, uint32_t slot) { m_fixed_port.set_tag(std::forward<T>(port_tag)); m_slot = slot; }

protected:

	device_wtvport_card_interface(const machine_config &mconfig, device_t &device);

	virtual void interface_pre_start() override;

	wtvport_device &port() const { return *m_port; }
	uint32_t slot() const { return m_slot; }
	offs_t base() const { return m_port->slot_base(m_slot); }

	void set_irq(int state) { m_port->set_slot_irq(m_slot, state); }
	void set_diag(int state) { m_port->set_slot_diag(m_slot, state); }

private:

	optional_device<wtvport_device> m_fixed_port;
	wtvport_device *m_port;
	uint32_t m_slot;

};

DECLARE_DEVICE_TYPE(WTVPORT, wtvport_device)
DECLARE_DEVICE_TYPE(WTVPORT_SLOT, wtvport_slot_device)

#endif // MAME_WEBTV_WTVPORT_H
