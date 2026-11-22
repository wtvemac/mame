// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_UTV_PCI_HOST_H
#define MAME_WEBTV_UTV_PCI_HOST_H

#pragma once

#include "machine/pci.h"

class utv_pci_host_device : public pci_host_device
{
public:

	// PCI memory BAR
	static constexpr uint32_t MEMORY_WINDOW_START = 0x11000000;
	static constexpr uint32_t MEMORY_WINDOW_SIZE  = 192 * 1024 * 1024; // 192MB
	static constexpr uint32_t MEMORY_WINDOW_END   = MEMORY_WINDOW_START + (MEMORY_WINDOW_SIZE - 1);

	// PCI's "I/O" BAR as physical memory addresses (MIPS doesn't have I/O ports like x86)
	static constexpr uint32_t IO_OFFSET       = 0x10000000;
	static constexpr uint32_t IO_WINDOW_START = IO_OFFSET + 0x1000; // skipping over the host config ports @ 0xcfX
	static constexpr uint32_t IO_WINDOW_SIZE  = 60 * 1024; // 60kB. UTV nk.exe only scans 32kB of this up to 32 PCI devices.
	static constexpr uint32_t IO_WINDOW_END   = IO_WINDOW_START + (IO_WINDOW_SIZE - 1);

	template <typename T>
	utv_pci_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&cpu_tag, uint32_t main_id = 0x14140000, uint8_t revision = 0x00, uint32_t subdevice_id = 0x00000000)
		: utv_pci_host_device(mconfig, tag, owner, clock)
	{
		// These IDs aren't known to me but don't appear to matter on the software side.
		// default main_id = 0x1414 (vendor = Microsoft) + 0x0000 (device id = 0)
		set_ids_host(main_id, revision, subdevice_id);

		set_cpu_tag(std::forward<T>(cpu_tag));
	}

	utv_pci_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_cpu_tag(T &&tag) { m_hostcpumem.set_tag(std::forward<T>(tag)); }

	void config_access_map(address_map &map) ATTR_COLD;

protected:

	virtual void device_start() override ATTR_COLD;

private:

	required_device<device_memory_interface> m_hostcpumem;
};

DECLARE_DEVICE_TYPE(UTV_PCI_HOST, utv_pci_host_device)

#endif // MAME_WEBTV_UTV_PCI_HOST_H
