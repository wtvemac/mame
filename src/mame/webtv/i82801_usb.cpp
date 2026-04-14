// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82801_usb.h"

DEFINE_DEVICE_TYPE(I82801_USB_UHCI, i82801_usb_uhci_device, "i82801_usb_uhci", "i82801 ICH4 USB 1.1 (UHCI)")
DEFINE_DEVICE_TYPE(I82801_USB_EHCI, i82801_usb_ehci_device, "i82801_usb_ehci", "i82801 ICH4 USB 2.0 (EHCI)")

i82801_usb_uhci_device::i82801_usb_uhci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id, uint32_t revision)
	: pci_device(mconfig, I82801_USB_UHCI, tag, owner, clock),
	m_pirq_w_cb(*this)
{
	// 0x0c: Serial Bus Controller; 0x0300: 0x03xx=USB Controller, 0xxx00=UHCI (USB1) Controller
	uint32_t device_class = 0x0c0300;

	set_ids(main_id, revision, device_class, 0x00000000);

	intr_pin = i82801_lpc_device::INT_PIN_A;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_A;

	set_multifunction_device(true);
}

void i82801_usb_uhci_device::device_start()
{
	pci_device::device_start();

	i82801_usb_uhci_device::set_default_values();
}

void i82801_usb_uhci_device::device_reset()
{
	pci_device::device_reset();

	i82801_usb_uhci_device::set_default_values();
}

void i82801_usb_uhci_device::set_default_values()
{
	m_usb_iobase = i82801_usb_uhci_device::DEFAULT_USB_IO_BASE;
	m_usb_cmd = 0x0000;
	m_usb_sts = 0x0020;
	m_usb_intr = 0x0000;
	m_usb_frnum = 0x0000;
	m_usb_frbaseadd = 0x00000000;
	m_usb_sofmod = 0x40;
	m_usb_portsc0 = 0x0080;
	m_usb_portsc1 = 0x0080;
}

void i82801_usb_uhci_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	uint16_t iobase = (m_usb_iobase & i82801_usb_uhci_device::USB_IO_BASE_MASK);
	if(iobase != 0x0000)
		io_space->install_device(iobase, iobase + (i82801_usb_uhci_device::USB_IO_SIZE - 1), *this, &i82801_usb_uhci_device::io_map);
}

void i82801_usb_uhci_device::config_map(address_map &map)
{
	pci_device::config_map(map);

	// 0x20-0x23: Base Address Register
	map(0x20, 0x23).rw(FUNC(i82801_usb_uhci_device::usb_iobase_r), FUNC(i82801_usb_uhci_device::usb_iobase_w));
	// 0x3c: Interrupt line
	// 0x3d: Interrupt pin
	// 0x60: USB Release number
	map(0x60, 0x60).r(FUNC(i82801_usb_uhci_device::relnum_r));
	// 0xc0-0xc1: USB Legacy Keyboard/Mouse Control
	// 0xc4: USB Resume Enable
}

void i82801_usb_uhci_device::io_map(address_map &map)
{
	// 0x00-0x01: USB Command
	map(0x00, 0x01).rw(FUNC(i82801_usb_uhci_device::usb_cmd_r), FUNC(i82801_usb_uhci_device::usb_cmd_w));
	// 0x02-0x03: USB Status
	map(0x02, 0x03).rw(FUNC(i82801_usb_uhci_device::usb_sts_r), FUNC(i82801_usb_uhci_device::usb_sts_w));
	// 0x04-0x05: USB Interrupt Enable
	map(0x04, 0x05).rw(FUNC(i82801_usb_uhci_device::usb_ier_r), FUNC(i82801_usb_uhci_device::usb_ier_w));
	// 0x06-0x07: USB Frame Number
	map(0x06, 0x07).rw(FUNC(i82801_usb_uhci_device::usb_frnum_r), FUNC(i82801_usb_uhci_device::usb_frnum_w));
	// 0x08-0x0b: USB Frame List Base Address
	map(0x08, 0x0b).rw(FUNC(i82801_usb_uhci_device::usb_frbaseadd_r), FUNC(i82801_usb_uhci_device::usb_frbaseadd_w));
	// 0x0c: USB Start of Frame Modify
	map(0x0c, 0x0c).rw(FUNC(i82801_usb_uhci_device::usb_sofmod_r), FUNC(i82801_usb_uhci_device::usb_sofmod_w));
	// 0x10-0x11: Port 0 Status/Control
	map(0x10, 0x11).rw(FUNC(i82801_usb_uhci_device::usb_portsc0_r), FUNC(i82801_usb_uhci_device::usb_portsc0_w));
	// 0x12-0x13: Port 1 Status/Control
	map(0x12, 0x13).rw(FUNC(i82801_usb_uhci_device::usb_portsc1_r), FUNC(i82801_usb_uhci_device::usb_portsc1_w));
}

void i82801_usb_uhci_device::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	pci_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

void i82801_usb_uhci_device::set_subsystem_id(uint32_t _subsystem_id)
{
	subsystem_id = _subsystem_id;
}

uint32_t i82801_usb_uhci_device::usb_iobase_r()
{
	return m_usb_iobase;
}

void i82801_usb_uhci_device::usb_iobase_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_usb_uhci_device::USB_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_usb_iobase = (m_usb_iobase & (~i82801_usb_uhci_device::USB_IO_BASE_MASK)) | iobase;
		i82801_usb_uhci_device::remap_cb();
	}
}

uint8_t i82801_usb_uhci_device::relnum_r()
{
	return 0x10;
}

uint16_t i82801_usb_uhci_device::usb_cmd_r()
{
	return m_usb_cmd;
}

void i82801_usb_uhci_device::usb_cmd_w(uint16_t data)
{
	m_usb_cmd = data;
}

uint16_t i82801_usb_uhci_device::usb_sts_r()
{
	return m_usb_sts;
}

void i82801_usb_uhci_device::usb_sts_w(uint16_t data)
{
	m_usb_sts &= (~data);
}

uint16_t i82801_usb_uhci_device::usb_ier_r()
{
	return m_usb_intr;
}

void i82801_usb_uhci_device::usb_ier_w(uint16_t data)
{
	m_usb_intr = data;
}

uint16_t i82801_usb_uhci_device::usb_frnum_r()
{
	return m_usb_frnum;
}

void i82801_usb_uhci_device::usb_frnum_w(uint16_t data)
{
	m_usb_frnum = data;
}


uint32_t i82801_usb_uhci_device::usb_frbaseadd_r()
{
	return m_usb_frbaseadd;
}

void i82801_usb_uhci_device::usb_frbaseadd_w(uint32_t data)
{
	m_usb_frbaseadd = data;
}

uint8_t i82801_usb_uhci_device::usb_sofmod_r()
{
	return m_usb_sofmod;
}

void i82801_usb_uhci_device::usb_sofmod_w(uint8_t data)
{
	m_usb_sofmod = data;
}

uint16_t i82801_usb_uhci_device::usb_portsc0_r()
{
	return m_usb_portsc0 | 0x80 /* Always needs |0x80 */;
}

void i82801_usb_uhci_device::usb_portsc0_w(uint16_t data)
{
	m_usb_portsc0 = data;
}

uint16_t i82801_usb_uhci_device::usb_portsc1_r()
{
	return m_usb_portsc1 | 0x80 /* Always needs |0x80 */;
}

void i82801_usb_uhci_device::usb_portsc1_w(uint16_t data)
{
	m_usb_portsc1 = data;
}

i82801_usb_ehci_device::i82801_usb_ehci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id, uint32_t revision)
	: pci_device(mconfig, I82801_USB_EHCI, tag, owner, clock),
	m_pirq_w_cb(*this)
{
	// 0x0c: Serial Bus Controller; 0x0320: 0x03xx=USB Controller, 0xxx20=EHCI (USB2) Controller
	uint32_t device_class = 0x0c0320;

	set_ids(main_id, revision, device_class, 0x00000000);
}

void i82801_usb_ehci_device::device_start()
{
	pci_device::device_start();
}

void i82801_usb_ehci_device::device_reset()
{
	pci_device::device_reset();

	m_usb_membase = i82801_usb_ehci_device::DEFAULT_USB_MEM_BASE;

	i82801_usb_ehci_device::set_default_values();
}

void i82801_usb_ehci_device::set_default_values()
{
	m_ccr_hcsparams = 0x00000000;
	m_ccr_hcsparams |= 1 << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_DPN_SHIFT;
	m_ccr_hcsparams |= 3 << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NCC_SHIFT;
	m_ccr_hcsparams |= 2 << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPCC_SHIFT;
	m_ccr_hcsparams |= 6 << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPORTS_SHIFT;

	m_ccr_hccparams = 0x00000000;
	m_ccr_hccparams |= i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_DEFAULT_EECP << i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_EECP_SHIFT;
	m_ccr_hccparams |= i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_IST_DEFAULT_MF_HOLD << i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_IST_MF_HOLD_SHIFT;
	m_ccr_hccparams |= i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_64BIT_CAP;

	m_cor_cmd = i82801_usb_ehci_device::USB_EHCI_CMD_INT_THRE_8MF;
	m_cor_sts = i82801_usb_ehci_device::USB_EHCI_STS_HCHALT;
	m_cor_ier = 0x00000000;
	m_cor_fidx = 0x00000000;
	m_cor_ctrl_seg = 0x00000000;
	m_cor_plist_base = 0x00000000;
	m_cor_plist_addr = 0x00000000;
	m_cor_cnfg_flag = 0x00000000;
	m_cor_port0_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;
	m_cor_port1_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;
	m_cor_port2_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;
	m_cor_port3_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;
	m_cor_port4_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;
	m_cor_port5_sts = i82801_usb_ehci_device::USB_EHCI_PORT_COWN | i82801_usb_ehci_device::USB_EHCI_PORT_PWR;

}

void i82801_usb_ehci_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	uint32_t membase = (m_usb_membase & i82801_usb_ehci_device::USB_MEM_BASE_MASK);
	if(membase > 0)
		memory_space->install_device(membase, membase + (i82801_usb_ehci_device::USB_MEM_MAP_SIZE - 1), *this, &i82801_usb_ehci_device::mem_map);

}

void i82801_usb_ehci_device::config_map(address_map &map)
{
	pci_device::config_map(map);

	// 0x10-0x13: Memory Base Address Register
	map(0x10, 0x13).rw(FUNC(i82801_usb_ehci_device::usb_membase_r), FUNC(i82801_usb_ehci_device::usb_membase_w));
	// 0x2c-0x2d: Subsystem Vendor ID
	map(0x2c, 0x2d).w(FUNC(i82801_usb_ehci_device::subvendor_w));
	// 0x2e-0x2f: Subsystem ID
	map(0x2e, 0x2f).w(FUNC(i82801_usb_ehci_device::subsystem_w));
	// 0x34: Capabilities Pointer
	// 0x3c: Interrupt line
	// 0x3d: Interrupt pin
	// 0x51: Next Item Pointer #1
	// 0x58: Debug Port Capability ID
	// 0x59: Next Item Pointer #2
	// 0x51-0x5b: Debug Port Base Offset
	// 0x60: USB Release number
	map(0x60, 0x60).r(FUNC(i82801_usb_ehci_device::relnum_r));
	// 0x61: Frame Length Adjustment
	// 0x68-0x6b: USB EHCI Legacy Support Extended
	// 0x6c-0x6f: USB EHCI Legacy Support Control/Status
	// 0x70-0x73: Intel Specific USB EHCI SMI
	// 0x80: Access Control
	map(0x80, 0x80).rw(FUNC(i82801_usb_ehci_device::access_cntl_r), FUNC(i82801_usb_ehci_device::access_cntl_w));
	// 0xdc-0xdf: USB HS Reference Voltage Register
}

void i82801_usb_ehci_device::mem_map(address_map &map)
{
	// Host Controller Capability Registers:
	map(0x0000, (i82801_usb_ehci_device::USB_CCR_MEM_MAP_SIZE - 1)).m(FUNC(i82801_usb_ehci_device::ccr_mem_map));

	// Host Controller Operational Registers:
	map(i82801_usb_ehci_device::USB_CCR_MEM_MAP_SIZE, (i82801_usb_ehci_device::USB_MEM_MAP_SIZE - 1)).m(FUNC(i82801_usb_ehci_device::cor_mem_map));
}

void i82801_usb_ehci_device::ccr_mem_map(address_map &map)
{
	// 0x00: Capabilities Registers Length
	map(0x00, 0x00).r(FUNC(i82801_usb_ehci_device::ccr_caplength_r));
	// 0x02-0x03: Host Controller Interface Version Number
	// 0x04-0x07: Host Controller Structural Parameters
	map(0x04, 0x07).rw(FUNC(i82801_usb_ehci_device::ccr_hcsparams_r), FUNC(i82801_usb_ehci_device::ccr_hcsparams_w));
	// 0x08-0x0b: Host Controller Capability Parameters
	map(0x08, 0x0b).r(FUNC(i82801_usb_ehci_device::ccr_hccparams_r));
}

void i82801_usb_ehci_device::cor_mem_map(address_map &map)
{
	// 0x00-0x03: USB EHCI Command
	map(0x00, 0x03).rw(FUNC(i82801_usb_ehci_device::cor_cmd_r), FUNC(i82801_usb_ehci_device::cor_cmd_w));
	// 0x04-0x07: USB EHCI Status
	map(0x04, 0x07).rw(FUNC(i82801_usb_ehci_device::cor_sts_r), FUNC(i82801_usb_ehci_device::cor_sts_w));
	// 0x08-0x0b: USB EHCI Interrupt Enable
	map(0x08, 0x0b).rw(FUNC(i82801_usb_ehci_device::cor_ier_r), FUNC(i82801_usb_ehci_device::cor_ier_w));
	// 0x0c-0x0f: USB EHCI Frame Index
	map(0x0c, 0x0f).rw(FUNC(i82801_usb_ehci_device::cor_fidx_r), FUNC(i82801_usb_ehci_device::cor_fidx_w));
	// 0x10-0x13: Control Data Structure Segment
	map(0x10, 0x13).rw(FUNC(i82801_usb_ehci_device::cor_ctrl_seg_r), FUNC(i82801_usb_ehci_device::cor_ctrl_seg_w));
	// 0x14-0x17: Period Frame List Base Address
	map(0x14, 0x17).rw(FUNC(i82801_usb_ehci_device::cor_plist_base_r), FUNC(i82801_usb_ehci_device::cor_plist_base_w));
	// 0x18-0x1b: Next Asynchronous List Address
	map(0x18, 0x1b).rw(FUNC(i82801_usb_ehci_device::cor_alist_addr_r), FUNC(i82801_usb_ehci_device::cor_alist_addr_w));
	// 0x40-0x43: Configure Flag Register
	map(0x40, 0x43).rw(FUNC(i82801_usb_ehci_device::cor_cnfg_flag_r), FUNC(i82801_usb_ehci_device::cor_cnfg_flag_w));
	// 0x44-0x47: Port 0 Status and Control
	map(0x44, 0x47).rw(FUNC(i82801_usb_ehci_device::cor_port0_sts_r), FUNC(i82801_usb_ehci_device::cor_port0_cnt_w));
	// 0x48-0x4b: Port 1 Status and Control
	map(0x48, 0x4b).rw(FUNC(i82801_usb_ehci_device::cor_port1_sts_r), FUNC(i82801_usb_ehci_device::cor_port1_cnt_w));
	// 0x4c-0x4f: Port 2 Status and Control
	map(0x4c, 0x4f).rw(FUNC(i82801_usb_ehci_device::cor_port2_sts_r), FUNC(i82801_usb_ehci_device::cor_port2_cnt_w));
	// 0x50-0x53: Port 3 Status and Control
	map(0x50, 0x53).rw(FUNC(i82801_usb_ehci_device::cor_port3_sts_r), FUNC(i82801_usb_ehci_device::cor_port3_cnt_w));
	// 0x54-0x57: Port 4 Status and Control
	map(0x54, 0x57).rw(FUNC(i82801_usb_ehci_device::cor_port4_sts_r), FUNC(i82801_usb_ehci_device::cor_port4_cnt_w));
	// 0x58-0x5b: Port 5 Status and Control
	map(0x58, 0x5b).rw(FUNC(i82801_usb_ehci_device::cor_port5_sts_r), FUNC(i82801_usb_ehci_device::cor_port5_cnt_w));

	// 0x60-0x73: Debug Port Registers
	// Config for this is hardwired into config reg 0x5a-0x5b
	map(0x60, 0x60 + (i82801_usb_ehci_device::USB_DBG_MEM_MAP_SIZE - 1)).m(FUNC(i82801_usb_ehci_device::dbg_mem_map));
	
}

void i82801_usb_ehci_device::dbg_mem_map(address_map &map)
{
	// 0x00-0x03: Control/Status
	// 0x04-0x07: USB PIDs
	// 0x08-0x0b: Data Buffer (Bytes 3:0)
	// 0x0c-0x0f: Data Buffer (Bytes 7:4)
	// 0x10-0x13: Config Register
}

void i82801_usb_ehci_device::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	pci_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

uint32_t i82801_usb_ehci_device::usb_membase_r()
{
	return m_usb_membase;
}

void i82801_usb_ehci_device::usb_membase_w(uint32_t data)
{
	uint32_t membase = (data & i82801_usb_ehci_device::USB_MEM_BASE_MASK);
	if(membase != 0x00000000)
	{
		m_usb_membase = (m_usb_membase & (~i82801_usb_ehci_device::USB_MEM_BASE_MASK)) | membase;
		i82801_usb_ehci_device::remap_cb();
	}
}

void i82801_usb_ehci_device::subvendor_w(uint16_t data)
{
	if(m_access_cntl & USB_ACNTL_WRT_RDONLY)
		subsystem_id = (subsystem_id & 0x0000ffff) | data << 0x10;
}

void i82801_usb_ehci_device::subsystem_w(uint16_t data)
{
	if(m_access_cntl & USB_ACNTL_WRT_RDONLY)
		subsystem_id = (subsystem_id & 0xffff0000) | data;
}

uint8_t i82801_usb_ehci_device::relnum_r()
{
	return 0x20;
}

uint8_t i82801_usb_ehci_device::access_cntl_r()
{
	return m_access_cntl;
}

void i82801_usb_ehci_device::access_cntl_w(uint8_t data)
{
	m_access_cntl = data;
}

uint8_t i82801_usb_ehci_device::ccr_caplength_r()
{
	return i82801_usb_ehci_device::USB_CCR_MEM_MAP_SIZE;
}

uint32_t i82801_usb_ehci_device::ccr_hcsparams_r()
{
	return m_ccr_hcsparams;
}

void i82801_usb_ehci_device::ccr_hcsparams_w(uint32_t data)
{
	m_ccr_hcsparams = (m_ccr_hcsparams & (~i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_WMASK);
}

uint32_t i82801_usb_ehci_device::ccr_hccparams_r()
{
	return m_ccr_hccparams;
}

uint32_t i82801_usb_ehci_device::cor_cmd_r()
{
	return m_cor_cmd;
}

void i82801_usb_ehci_device::cor_cmd_w(uint32_t data)
{
	m_cor_cmd = (m_cor_cmd & (~i82801_usb_ehci_device::USB_EHCI_CMD_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_CMD_WMASK);

	if(m_cor_cmd & USB_EHCI_CMD_RESET)
	{
		m_cor_cmd &= (~USB_EHCI_CMD_RESET);
		i82801_usb_ehci_device::set_default_values();
	}
	else if(m_cor_cmd & USB_EHCI_CMD_RUN)
	{
		m_cor_sts &= (~USB_EHCI_STS_HCHALT);
	}
	else
	{
		m_cor_sts |= USB_EHCI_STS_HCHALT;
	}
}

uint32_t i82801_usb_ehci_device::cor_sts_r()
{
	return m_cor_sts;
}

void i82801_usb_ehci_device::cor_sts_w(uint32_t data)
{
	data = (data & (~i82801_usb_ehci_device::USB_EHCI_STS_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_STS_WMASK);
	m_cor_sts &= (~data);
}

uint32_t i82801_usb_ehci_device::cor_ier_r()
{
	return m_cor_ier;
}

void i82801_usb_ehci_device::cor_ier_w(uint32_t data)
{
	m_cor_ier = data;
}

uint32_t i82801_usb_ehci_device::cor_fidx_r()
{
	return m_cor_fidx;
}

void i82801_usb_ehci_device::cor_fidx_w(uint32_t data)
{
	m_cor_fidx = data;
}

uint32_t i82801_usb_ehci_device::cor_ctrl_seg_r()
{
	return m_cor_ctrl_seg;
}

void i82801_usb_ehci_device::cor_ctrl_seg_w(uint32_t data)
{
	m_cor_ctrl_seg = data;
}

uint32_t i82801_usb_ehci_device::cor_plist_base_r()
{
	return m_cor_plist_base;
}

void i82801_usb_ehci_device::cor_plist_base_w(uint32_t data)
{
	m_cor_plist_base = data;
}

uint32_t i82801_usb_ehci_device::cor_alist_addr_r()
{
	return m_cor_plist_addr;
}

void i82801_usb_ehci_device::cor_alist_addr_w(uint32_t data)
{
	m_cor_plist_addr = data;
}

uint32_t i82801_usb_ehci_device::cor_cnfg_flag_r()
{
	return m_cor_cnfg_flag;
}

void i82801_usb_ehci_device::cor_cnfg_flag_w(uint32_t data)
{
	if((data ^ m_cor_cnfg_flag) & USB_EHCI_CNFG_READY)
	{
		if(data & USB_EHCI_CNFG_READY)
		{
			m_cor_port0_sts &= (~USB_EHCI_PORT_COWN);
			m_cor_port1_sts &= (~USB_EHCI_PORT_COWN);
			m_cor_port2_sts &= (~USB_EHCI_PORT_COWN);
			m_cor_port3_sts &= (~USB_EHCI_PORT_COWN);
			m_cor_port4_sts &= (~USB_EHCI_PORT_COWN);
			m_cor_port5_sts &= (~USB_EHCI_PORT_COWN);
		}
		else
		{
			m_cor_port0_sts |= USB_EHCI_PORT_COWN;
			m_cor_port1_sts |= USB_EHCI_PORT_COWN;
			m_cor_port2_sts |= USB_EHCI_PORT_COWN;
			m_cor_port3_sts |= USB_EHCI_PORT_COWN;
			m_cor_port4_sts |= USB_EHCI_PORT_COWN;
			m_cor_port5_sts |= USB_EHCI_PORT_COWN;
		}
	}

	m_cor_cnfg_flag = data;
}

uint32_t i82801_usb_ehci_device::cor_port0_sts_r()
{
	return m_cor_port0_sts;
}

void i82801_usb_ehci_device::cor_port0_cnt_w(uint32_t data)
{
	m_cor_port0_sts = (m_cor_port0_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}

uint32_t i82801_usb_ehci_device::cor_port1_sts_r()
{
	return m_cor_port1_sts;
}

void i82801_usb_ehci_device::cor_port1_cnt_w(uint32_t data)
{
	m_cor_port1_sts = (m_cor_port1_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}

uint32_t i82801_usb_ehci_device::cor_port2_sts_r()
{
	return m_cor_port2_sts;
}

void i82801_usb_ehci_device::cor_port2_cnt_w(uint32_t data)
{
	m_cor_port2_sts = (m_cor_port2_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}

uint32_t i82801_usb_ehci_device::cor_port3_sts_r()
{
	return m_cor_port3_sts;
}

void i82801_usb_ehci_device::cor_port3_cnt_w(uint32_t data)
{
	m_cor_port3_sts = (m_cor_port3_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}

uint32_t i82801_usb_ehci_device::cor_port4_sts_r()
{
	return m_cor_port4_sts;
}

void i82801_usb_ehci_device::cor_port4_cnt_w(uint32_t data)
{
	m_cor_port4_sts = (m_cor_port4_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}

uint32_t i82801_usb_ehci_device::cor_port5_sts_r()
{
	return m_cor_port5_sts;
}

void i82801_usb_ehci_device::cor_port5_cnt_w(uint32_t data)
{
	m_cor_port5_sts = (m_cor_port5_sts & (~i82801_usb_ehci_device::USB_EHCI_PORT_WMASK)) | (data & i82801_usb_ehci_device::USB_EHCI_PORT_WMASK);
}
