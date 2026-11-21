// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// UltimateTV PCI open host controller interface (driver is ohci.dll)
// This has a 2 port (wtvusb_port_device) root hub

// ohci.dll scans bus 0 for class 0c (serial bus controller)/03 (USB controller)/10 (OHCI), sets the latency timer, maps 128 bytes of BAR0 and sets up its interrupt routine (FUD GPIO bit 5)

// Supported:
//   control,
//   bulk and interrupt transfers (general TDs)
//   NAK retry
//   STALL/halt
//   short packets
//   done queue and its interrupt delay
//   root hub port reset/enable/suspend
//
// Isochronous EDs are skipped

#ifndef MAME_WEBTV_UTV_OHCI_H
#define MAME_WEBTV_UTV_OHCI_H

#pragma once

#include "machine/pci.h"
#include "wtvusb.h"

class utv_ohci_device : public pci_device
{

public:

	static constexpr uint8_t PORT_COUNT = 2;

	utv_ohci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, uint32_t main_id = 0x10330035, uint32_t revision = 0x41);

	auto irq_callback() { return m_irq_cb.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:

	// Register indexes
	enum : uint32_t
	{
		HC_REVISION = 0,
		HC_CONTROL,
		HC_COMMAND_STATUS,
		HC_INTERRUPT_STATUS,
		HC_INTERRUPT_ENABLE,
		HC_INTERRUPT_DISABLE,
		HC_HCCA,
		HC_PERIOD_CURRENT_ED,
		HC_CONTROL_HEAD_ED,
		HC_CONTROL_CURRENT_ED,
		HC_BULK_HEAD_ED,
		HC_BULK_CURRENT_ED,
		HC_DONE_HEAD,
		HC_FM_INTERVAL,
		HC_FM_REMAINING,
		HC_FM_NUMBER,
		HC_PERIODIC_START,
		HC_LS_THRESHOLD,
		HC_RH_DESCRIPTOR_A,
		HC_RH_DESCRIPTOR_B,
		HC_RH_STATUS,
		HC_RH_PORT_STATUS
	};

	// HcControl
	static constexpr uint32_t CONTROL_CBSR = 0x003;
	static constexpr uint32_t CONTROL_PLE  = 0x004;
	static constexpr uint32_t CONTROL_IE   = 0x008;
	static constexpr uint32_t CONTROL_CLE  = 0x010;
	static constexpr uint32_t CONTROL_BLE  = 0x020;
	static constexpr uint32_t CONTROL_HCFS = 0x0c0;
	static constexpr uint32_t CONTROL_IR   = 0x100;
	static constexpr uint32_t CONTROL_RWC  = 0x200;
	static constexpr uint32_t CONTROL_RWE  = 0x400;
	static constexpr uint32_t CONTROL_MASK = 0x7ff;

	static constexpr uint32_t HCFS_RESET       = 0;
	static constexpr uint32_t HCFS_RESUME      = 1;
	static constexpr uint32_t HCFS_OPERATIONAL = 2;
	static constexpr uint32_t HCFS_SUSPEND     = 3;

	// HcCommandStatus
	static constexpr uint32_t COMMAND_HCR = 0x01;
	static constexpr uint32_t COMMAND_CLF = 0x02;
	static constexpr uint32_t COMMAND_BLF = 0x04;
	static constexpr uint32_t COMMAND_OCR = 0x08;

	// HcInterruptStatus / Enable
	static constexpr uint32_t INT_SO   = 0x00000001;
	static constexpr uint32_t INT_WDH  = 0x00000002;
	static constexpr uint32_t INT_SF   = 0x00000004;
	static constexpr uint32_t INT_RD   = 0x00000008;
	static constexpr uint32_t INT_UE   = 0x00000010;
	static constexpr uint32_t INT_FNO  = 0x00000020;
	static constexpr uint32_t INT_RHSC = 0x00000040;
	static constexpr uint32_t INT_OC   = 0x40000000;
	static constexpr uint32_t INT_MIE  = 0x80000000;
	static constexpr uint32_t INT_MASK = 0x4000007f;

	// HcRhPortStatus
	static constexpr uint32_t PORT_CCS   = 0x00000001;
	static constexpr uint32_t PORT_PES   = 0x00000002;
	static constexpr uint32_t PORT_PSS   = 0x00000004;
	static constexpr uint32_t PORT_POCI  = 0x00000008;
	static constexpr uint32_t PORT_PRS   = 0x00000010;
	static constexpr uint32_t PORT_PPS   = 0x00000100;
	static constexpr uint32_t PORT_LSDA  = 0x00000200;
	static constexpr uint32_t PORT_CSC   = 0x00010000;
	static constexpr uint32_t PORT_PESC  = 0x00020000;
	static constexpr uint32_t PORT_PSSC  = 0x00040000;
	static constexpr uint32_t PORT_OCIC  = 0x00080000;
	static constexpr uint32_t PORT_PRSC  = 0x00100000;
	static constexpr uint32_t PORT_CHANGE_MASK = 0x001f0000;

	// Completion codes
	static constexpr uint8_t CC_NO_ERROR              = 0x0;
	static constexpr uint8_t CC_STALL                 = 0x4;
	static constexpr uint8_t CC_DEVICE_NOT_RESPONDING = 0x5;
	static constexpr uint8_t CC_DATA_OVERRUN          = 0x8;
	static constexpr uint8_t CC_DATA_UNDERRUN         = 0x9;

	// USB full speed carries about 1500 bytes per 1ms frame
	static constexpr uint32_t FRAME_BUDGET = 19 * 64;
	static constexpr uint32_t PORT_RESET_MS = 10;
	static constexpr uint32_t MAX_ED_COUNT = 256;

	enum td_result_t
	{
		TD_RETIRED, // completed, the next TD on the ED may run
		TD_WAITING, // NAK or the frame ran out of time, retried next frame
		TD_HALTED   // completed with an error, the ED is halted
	};

	struct root_port_t
	{
		bool enabled;
		bool suspended;
		bool resetting;
		uint32_t change;
		int reset_ms;
	};

	void regs_map(address_map &map) ATTR_COLD;
	uint32_t regs_r(offs_t offset);
	void regs_w(offs_t offset, uint32_t data, uint32_t mem_mask);

	TIMER_CALLBACK_MEMBER(frame_tick);

	void hc_reset();
	void set_hcfs(uint32_t hcfs);
	void set_irq(uint32_t mask, int status);
	void update_irq();

	wtvusb_device_interface *port_function(uint8_t port_idx) const;
	uint32_t port_status(uint8_t port_idx) const;
	void port_write(uint8_t port_idx, uint32_t data);
	void port_change(uint8_t port_idx, uint32_t change);
	wtvusb_device_interface *find_function(uint8_t address) const;

	uint32_t dma_r32(uint32_t address);
	void dma_w32(uint32_t address, uint32_t data);
	uint8_t dma_r8(uint32_t address);
	void dma_w8(uint32_t address, uint8_t data);

	void process_periodic();
	void process_async(uint32_t head_reg, uint32_t current_reg, uint32_t filled, uint32_t enable);
	bool process_endpoint_descriptor(uint32_t ed_address, uint16_t max_packets);
	td_result_t process_transfer_descriptor(uint32_t ed_address, uint32_t ed0, uint32_t &headp, uint16_t &packets, uint16_t max_packets);
	void retire_transfer_descriptor(uint32_t td_address, uint32_t d0, uint8_t cc, uint8_t toggle, uint32_t cbp, uint32_t next_td, bool error);

	devcb_write_line m_irq_cb;
	required_device_array<wtvusb_port_device, PORT_COUNT> m_ports;

	emu_timer *m_frame_timer;
	address_space *m_dma;

	uint32_t m_regs[HC_RH_PORT_STATUS];
	root_port_t m_port[PORT_COUNT];
	uint16_t m_frame_number;
	uint32_t m_last_logged_td = 0; // LOG_TD only logs a TD once, not on every NAK retry
	uint8_t m_done_counter;
	int m_budget;
	bool m_irq_state;

};

DECLARE_DEVICE_TYPE(UTV_OHCI, utv_ohci_device)

#endif // MAME_WEBTV_UTV_OHCI_H
