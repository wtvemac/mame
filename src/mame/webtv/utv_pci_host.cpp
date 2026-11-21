// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "utv_pci_host.h"

DEFINE_DEVICE_TYPE(UTV_PCI_HOST, utv_pci_host_device, "utv_pci_host", "UltimateTV PCI host bridge")

utv_pci_host_device::utv_pci_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_host_device(mconfig, UTV_PCI_HOST, tag, owner, clock),
	m_hostcpumem(*this, finder_base::DUMMY_TAG)
{
	//
}

void utv_pci_host_device::device_start()
{
	pci_host_device::device_start();

	address_space &space = m_hostcpumem->space(AS_PROGRAM);

	set_spaces(&space, &space, &space);

	memory_window_start = MEMORY_WINDOW_START;
	memory_window_end = MEMORY_WINDOW_END;
	memory_offset = 0;

	io_window_start = IO_WINDOW_START;
	io_window_end = IO_WINDOW_END;
	io_offset = IO_OFFSET;
}

void utv_pci_host_device::config_access_map(address_map &map)
{
	// The UTV usually reads @ 0xf0000cfX which collapses to 0x10000cfX on the CPU side.

	// Typically: 0xcf8-0xcfb or 0x10000cf8-0x10000cfb / 0x00800cf8-0x00800cfb
	map(0x08, 0x0b).rw(FUNC(utv_pci_host_device::config_address_r), FUNC(utv_pci_host_device::config_address_w));
	// Typically: 0xcfc-0xcff or 0x10000cfc-0x10000cff / 0x00800cfc-0x00800cff
	map(0x0c, 0x0f).rw(FUNC(utv_pci_host_device::config_data_r), FUNC(utv_pci_host_device::config_data_w));
}
