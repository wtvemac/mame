// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82801_USB_H
#define MAME_WEBTV_I82801_USB_H

#pragma once

#include "machine/pci.h"
#include "i82801_lpc.h"

class i82801_usb_uhci_device : public pci_device
{

public:

	static constexpr uint32_t USB_IO_SIZE      = 0x18;

	static constexpr uint32_t USB_BASE_IS_IO   = 0x00000001;
	static constexpr uint32_t USB_IO_BASE_MASK = 0x0000ffe0;

	static constexpr uint32_t DEFAULT_USB_IO_BASE  = i82801_usb_uhci_device::USB_BASE_IS_IO;

	i82801_usb_uhci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id = 0x808624c2, uint32_t revision = 0x00);

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

	void set_subsystem_id(uint32_t _subsystem_id);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

private:

	devcb_write8 m_pirq_w_cb;

	uint8_t m_pirq_pin;

	uint32_t m_usb_iobase;
	uint16_t m_usb_cmd;
	uint16_t m_usb_sts;
	uint16_t m_usb_intr;
	uint16_t m_usb_frnum;
	uint32_t m_usb_frbaseadd;
	uint8_t m_usb_sofmod;
	uint16_t m_usb_portsc0;
	uint16_t m_usb_portsc1;

	void set_default_values();

	void io_map(address_map &map);

	uint32_t usb_iobase_r();
	void usb_iobase_w(uint32_t data);
	uint8_t relnum_r();
	uint16_t usb_cmd_r();
	void usb_cmd_w(uint16_t data);
	uint16_t usb_sts_r();
	void usb_sts_w(uint16_t data);
	uint16_t usb_ier_r();
	void usb_ier_w(uint16_t data);
	uint16_t usb_frnum_r();
	void usb_frnum_w(uint16_t data);
	uint32_t usb_frbaseadd_r();
	void usb_frbaseadd_w(uint32_t data);
	uint8_t usb_sofmod_r();
	void usb_sofmod_w(uint8_t data);
	uint16_t usb_portsc0_r();
	void usb_portsc0_w(uint16_t data);
	uint16_t usb_portsc1_r();
	void usb_portsc1_w(uint16_t data);

};

class i82801_usb_ehci_device : public pci_device
{

public:

	static constexpr uint32_t USB_BASE_IS_IO       = 1 << 0;
	static constexpr uint32_t USB_BASE_IS_MEM      = 0 << 0;
	static constexpr uint32_t USB_MEM_BASE_MASK    = 0xfffffc00;
	static constexpr uint32_t USB_MEM_TYPE_MASK    = 0x00000006;
	static constexpr uint32_t USB_MEM_TYPE_ANY     = 0 << 2;
	static constexpr uint32_t USB_MEM_PREFETCHABLE = 1 << 3;

	static constexpr uint32_t DEFAULT_USB_MEM_BASE = i82801_usb_ehci_device::USB_MEM_TYPE_ANY | i82801_usb_ehci_device::USB_BASE_IS_MEM;

	static constexpr uint16_t USB_MEM_MAP_SIZE     = 0x400;
	static constexpr uint8_t USB_CCR_MEM_MAP_SIZE  = 0x20;
	static constexpr uint8_t USB_DBG_MEM_MAP_SIZE  = 0x14;
	
	static constexpr uint32_t USB_ACNTL_WRT_RDONLY = 1 << 0;

	static constexpr uint32_t USB_EHCI_HCSPARAMS_DPN_SHIFT    = 20;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_DPN_MASK     = 0x0f << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_DPN_SHIFT;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NCC_SHIFT    = 12;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NCC_MASK     = 0x0f << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NCC_SHIFT;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NPCC_SHIFT   = 8;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NPCC_MASK    = 0x0f << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPCC_SHIFT;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NPORTS_SHIFT = 0;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_NPORTS_MASK  = 0x0f << i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPCC_SHIFT;
	static constexpr uint32_t USB_EHCI_HCSPARAMS_WMASK        = i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_DPN_MASK | i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPCC_SHIFT | i82801_usb_ehci_device::USB_EHCI_HCSPARAMS_NPORTS_MASK;

	static constexpr uint32_t USB_EHCI_HCCPARAMS_DEFAULT_EECP        = 0x68;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_EECP_SHIFT          = 8;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_EECP_MASK           = 0xff << i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_EECP_SHIFT;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_IST_CACHE           = 1 << 7;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_IST_DEFAULT_MF_HOLD = 4;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_IST_MF_HOLD_SHIFT   = 4;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_IST_MD_HOLD_MASK    = 0x07 << i82801_usb_ehci_device::USB_EHCI_HCCPARAMS_IST_MF_HOLD_SHIFT;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_ASPC                = 1 << 2;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_PFLF                = 1 << 1;
	static constexpr uint32_t USB_EHCI_HCCPARAMS_64BIT_CAP           = 1 << 0;

	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_MASK   = 0xff << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_1MF    =    1 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_2MF    =    2 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_4MF    =    4 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_8MF    =    8 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_16MF   =   16 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_32MF   =   32 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_THRE_64MF   =   64 << 16;
	static constexpr uint32_t USB_EHCI_CMD_INT_AADB        =    1 << 6;
	static constexpr uint32_t USB_EHCI_CMD_ASCHED_EN       =    1 << 5;
	static constexpr uint32_t USB_EHCI_CMD_PSCHED_EN       =    1 << 4;
	static constexpr uint32_t USB_EHCI_CMD_FLIST_SIZE_MASK = 0x03 << 2;
	static constexpr uint32_t USB_EHCI_CMD_FLIST_SIZE_256  =    2 << 2;
	static constexpr uint32_t USB_EHCI_CMD_FLIST_SIZE_512  =    1 << 2;
	static constexpr uint32_t USB_EHCI_CMD_FLIST_SIZE_1024 =    0 << 2;
	static constexpr uint32_t USB_EHCI_CMD_RESET           =    1 << 1;
	static constexpr uint32_t USB_EHCI_CMD_RUNSTOP_MASK    = 0x01 << 1;
	static constexpr uint32_t USB_EHCI_CMD_RUN             =    1 << 1;
	static constexpr uint32_t USB_EHCI_CMD_STOP            =    0 << 1;
	static constexpr uint32_t USB_EHCI_CMD_WMASK           = 0x00ff0073;

	static constexpr uint32_t USB_EHCI_STS_ASYNCSCHSTS = 1 << 15;
	static constexpr uint32_t USB_EHCI_STS_PERDSCHDSTS = 1 << 14;
	static constexpr uint32_t USB_EHCI_STS_RECLAIM     = 1 << 13;
	static constexpr uint32_t USB_EHCI_STS_HCHALT      = 1 << 12;
	static constexpr uint32_t USB_EHCI_STS_ASYNCADVINT = 1 <<  5;
	static constexpr uint32_t USB_EHCI_STS_HOSTERR     = 1 <<  4;
	static constexpr uint32_t USB_EHCI_STS_FRMROLOVR   = 1 <<  3;
	static constexpr uint32_t USB_EHCI_STS_PRTCNG      = 1 <<  2;
	static constexpr uint32_t USB_EHCI_STS_USBERRINT   = 1 <<  1;
	static constexpr uint32_t USB_EHCI_STS_USBINT      = 1 <<  0;
	static constexpr uint32_t USB_EHCI_STS_WMASK       = 0x0000002f;

	static constexpr uint32_t USB_EHCI_INT_ASYNCADV  = 1 << 5;
	static constexpr uint32_t USB_EHCI_INT_SYSERR    = 1 << 4;
	static constexpr uint32_t USB_EHCI_INT_FROLLOVER = 1 << 3;
	static constexpr uint32_t USB_EHCI_INT_PCHANGE   = 1 << 2;
	static constexpr uint32_t USB_EHCI_INT_USBERR    = 1 << 1;
	static constexpr uint32_t USB_EHCI_INT_USBINT    = 1 << 0;

	static constexpr uint32_t USB_EHCI_CNFG_READY = 1 << 0;

	static constexpr uint32_t USB_EHCI_PORT_COWN  = 1 << 13;
	static constexpr uint32_t USB_EHCI_PORT_PWR   = 1 << 12;
	static constexpr uint32_t USB_EHCI_PORT_WMASK = 0x7fe1c4;

	i82801_usb_ehci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id = 0x808624cd, uint32_t revision = 0x00);

	auto pirq_callback() { return m_pirq_w_cb.bind(); }

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

private:

	devcb_write8 m_pirq_w_cb;

	uint8_t m_pirq_pin;

	uint32_t m_usb_membase;

	uint8_t m_access_cntl;

	uint32_t m_ccr_hcsparams;
	uint32_t m_ccr_hccparams;

	uint32_t m_cor_cmd;
	uint32_t m_cor_sts;
	uint32_t m_cor_ier;
	uint32_t m_cor_fidx;
	uint32_t m_cor_ctrl_seg;
	uint32_t m_cor_plist_base;
	uint32_t m_cor_plist_addr;
	uint32_t m_cor_cnfg_flag;
	uint32_t m_cor_port0_sts;
	uint32_t m_cor_port1_sts;
	uint32_t m_cor_port2_sts;
	uint32_t m_cor_port3_sts;
	uint32_t m_cor_port4_sts;
	uint32_t m_cor_port5_sts;

	void set_default_values();

	void mem_map(address_map &map);
	void ccr_mem_map(address_map &map);
	void cor_mem_map(address_map &map);
	void dbg_mem_map(address_map &map);

	uint32_t usb_membase_r();
	void usb_membase_w(uint32_t data);
	void subvendor_w(uint16_t data);
	void subsystem_w(uint16_t data);
	uint8_t relnum_r();
	uint8_t access_cntl_r();
	void access_cntl_w(uint8_t data);
	uint8_t ccr_caplength_r();
	uint32_t ccr_hcsparams_r();
	void ccr_hcsparams_w(uint32_t data);
	uint32_t ccr_hccparams_r();
	uint32_t cor_cmd_r();
	void cor_cmd_w(uint32_t data);
	uint32_t cor_sts_r();
	void cor_sts_w(uint32_t data);
	uint32_t cor_ier_r();
	void cor_ier_w(uint32_t data);
	uint32_t cor_fidx_r();
	void cor_fidx_w(uint32_t data);
	uint32_t cor_ctrl_seg_r();
	void cor_ctrl_seg_w(uint32_t data);
	uint32_t cor_plist_base_r();
	void cor_plist_base_w(uint32_t data);
	uint32_t cor_alist_addr_r();
	void cor_alist_addr_w(uint32_t data);
	uint32_t cor_cnfg_flag_r();
	void cor_cnfg_flag_w(uint32_t data);
	uint32_t cor_port0_sts_r();
	void cor_port0_cnt_w(uint32_t data);
	uint32_t cor_port1_sts_r();
	void cor_port1_cnt_w(uint32_t data);
	uint32_t cor_port2_sts_r();
	void cor_port2_cnt_w(uint32_t data);
	uint32_t cor_port3_sts_r();
	void cor_port3_cnt_w(uint32_t data);
	uint32_t cor_port4_sts_r();
	void cor_port4_cnt_w(uint32_t data);
	uint32_t cor_port5_sts_r();
	void cor_port5_cnt_w(uint32_t data);

};

DECLARE_DEVICE_TYPE(I82801_USB_UHCI, i82801_usb_uhci_device)
DECLARE_DEVICE_TYPE(I82801_USB_EHCI, i82801_usb_ehci_device)

#endif // MAME_WEBTV_I82801_USB_H
