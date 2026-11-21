// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "utv_ohci.h"

#include <limits>

DEFINE_DEVICE_TYPE(UTV_OHCI, utv_ohci_device, "utv_ohci", "UltimateTV USB OHCI controller")

utv_ohci_device::utv_ohci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id, uint32_t revision)
	: pci_device(mconfig, UTV_OHCI, tag, owner, clock),
	m_irq_cb(*this),
	m_ports(*this, "usb%u", 1U),
	m_frame_timer(nullptr),
	m_dma(nullptr),
	m_frame_number(0),
	m_done_counter(7),
	m_budget(0),
	m_irq_state(false)
{
	intr_pin = 1; // INTA#, routed to FUD GPIO bit 5

	// 0x0c: Serial Bus Controller ; 0x0310=USB Controller, OHCI Controller 
	uint32_t device_class = 0x0c0310;

	set_ids(main_id, revision, device_class, 0x00000000);
}

void utv_ohci_device::device_add_mconfig(machine_config &config)
{
	WTVUSB_PORT(config, m_ports[0], wtvusb_devices, "adm8511");
	WTVUSB_PORT(config, m_ports[1], wtvusb_devices, nullptr);
}

void utv_ohci_device::device_start()
{
	pci_device::device_start();

	add_map(0x1000, M_MEM, FUNC(utv_ohci_device::regs_map));

	m_frame_timer = timer_alloc(FUNC(utv_ohci_device::frame_tick), this);

	std::fill(std::begin(m_regs), std::end(m_regs), 0);

	save_item(NAME(m_regs));
	save_item(STRUCT_MEMBER(m_port, enabled));
	save_item(STRUCT_MEMBER(m_port, suspended));
	save_item(STRUCT_MEMBER(m_port, resetting));
	save_item(STRUCT_MEMBER(m_port, change));
	save_item(STRUCT_MEMBER(m_port, reset_ms));
	save_item(NAME(m_frame_number));
	save_item(NAME(m_done_counter));
	save_item(NAME(m_irq_state));
}

void utv_ohci_device::device_reset()
{
	pci_device::device_reset();

	// The host bridge sets the bus master space in its device_start so it's resolved here.
	m_dma = get_pci_busmaster_space();

	utv_ohci_device::hc_reset();
	m_regs[utv_ohci_device::HC_CONTROL] = utv_ohci_device::HCFS_RESET << 6;

	m_regs[utv_ohci_device::HC_RH_DESCRIPTOR_A] = (1 << 24) | 0x1000 | 0x0200 | utv_ohci_device::PORT_COUNT; // POTPGT=2ms, NOCP, NPS (always powered)
	m_regs[utv_ohci_device::HC_RH_DESCRIPTOR_B] = 0;
	m_regs[utv_ohci_device::HC_RH_STATUS] = 0;

	for (uint8_t port_idx = 0; port_idx < PORT_COUNT; port_idx++)
	{
		m_port[port_idx] = root_port_t {
			.enabled   = false,
			.suspended = false,
			.resetting = false,
			.change    = 0,
			.reset_ms  = 0
		};

		if (wtvusb_device_interface *function = port_function(port_idx))
		{
			m_port[port_idx].change = utv_ohci_device::PORT_CSC;
			function->usb_bus_reset();
		}
	}

	m_irq_state = false;
	m_irq_cb(0);

	m_frame_timer->adjust(attotime::from_msec(1), 0, attotime::from_msec(1));
}

void utv_ohci_device::hc_reset()
{
	// Software reset (HcCommandStatus.HCR) root hub registers are left alone.

	for (uint32_t reg = utv_ohci_device::HC_REVISION; reg < utv_ohci_device::HC_RH_DESCRIPTOR_A; reg++)
		m_regs[reg] = 0;

	m_regs[utv_ohci_device::HC_REVISION] = 0x10;
	m_regs[utv_ohci_device::HC_CONTROL] = HCFS_SUSPEND << 6;
	m_regs[utv_ohci_device::HC_FM_INTERVAL] = 0x2edf;
	m_regs[utv_ohci_device::HC_LS_THRESHOLD] = 0x0628;

	m_frame_number = 0;
	m_done_counter = 7;

	utv_ohci_device::update_irq();
}

void utv_ohci_device::regs_map(address_map &map)
{
	map(0x000, 0xfff).rw(FUNC(utv_ohci_device::regs_r), FUNC(utv_ohci_device::regs_w));
}

uint32_t utv_ohci_device::regs_r(offs_t offset)
{
	uint32_t data = 0;

	if (offset >= utv_ohci_device::HC_RH_PORT_STATUS && offset < utv_ohci_device::HC_RH_PORT_STATUS + utv_ohci_device::PORT_COUNT)
	{
		data = utv_ohci_device::port_status(offset - utv_ohci_device::HC_RH_PORT_STATUS);
	}
	else if (offset == utv_ohci_device::HC_INTERRUPT_DISABLE)
	{
		data = m_regs[utv_ohci_device::HC_INTERRUPT_ENABLE];
	}
	else if (offset == utv_ohci_device::HC_FM_NUMBER)
	{
		data = m_frame_number;
	}
	else if (offset == utv_ohci_device::HC_FM_REMAINING)
	{
		const uint32_t interval = m_regs[utv_ohci_device::HC_FM_INTERVAL] & 0x3fff;

		double fraction = m_frame_timer->elapsed().as_double() * 1000.0;
		fraction = std::clamp(fraction, 0.0, 1.0);

		data = (interval - uint32_t(interval * fraction)) | (m_regs[utv_ohci_device::HC_FM_INTERVAL] & 0x80000000);
	}
	else if (offset < utv_ohci_device::HC_RH_PORT_STATUS)
	{
		data = m_regs[offset];
	}

	return data;
}

void utv_ohci_device::regs_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	if (offset >= utv_ohci_device::HC_RH_PORT_STATUS)
	{
		if (offset < utv_ohci_device::HC_RH_PORT_STATUS + utv_ohci_device::PORT_COUNT)
			port_write(offset - utv_ohci_device::HC_RH_PORT_STATUS, data & mem_mask);
	}
	else
	{
		const uint32_t merged = (m_regs[offset] & ~mem_mask) | (data & mem_mask);
		data &= mem_mask;

		switch (offset)
		{
			case HC_CONTROL:
			{
				const uint32_t old_hcfs = (m_regs[utv_ohci_device::HC_CONTROL] & utv_ohci_device::CONTROL_HCFS) >> 6;
				m_regs[utv_ohci_device::HC_CONTROL] = merged & utv_ohci_device::CONTROL_MASK;
				const uint32_t new_hcfs = (m_regs[utv_ohci_device::HC_CONTROL] & utv_ohci_device::CONTROL_HCFS) >> 6;

				if (new_hcfs != old_hcfs)
					set_hcfs(new_hcfs);

				break;
			}

			case utv_ohci_device::HC_COMMAND_STATUS:
			{
				if (data & utv_ohci_device::COMMAND_HCR)
					utv_ohci_device::hc_reset();
				else
					// The controller always belongs to the HCD (InterruptRouting is never set) so OCR has nothing to do.
					m_regs[utv_ohci_device::HC_COMMAND_STATUS] |= data & (utv_ohci_device::COMMAND_CLF | utv_ohci_device::COMMAND_BLF);
				break;
			}

			case utv_ohci_device::HC_INTERRUPT_STATUS:
				utv_ohci_device::set_irq(data, CLEAR_LINE);
				break;

			case utv_ohci_device::HC_INTERRUPT_ENABLE:
				m_regs[utv_ohci_device::HC_INTERRUPT_ENABLE] |= data & (utv_ohci_device::INT_MASK | utv_ohci_device::INT_MIE);
				update_irq();
				break;

			case utv_ohci_device::HC_INTERRUPT_DISABLE:
				m_regs[utv_ohci_device::HC_INTERRUPT_ENABLE] &= ~data;
				update_irq();
				break;

			case utv_ohci_device::HC_HCCA:
				m_regs[utv_ohci_device::HC_HCCA] = merged & 0xffffff00;
				break;

			case utv_ohci_device::HC_CONTROL_HEAD_ED:
			case utv_ohci_device::HC_CONTROL_CURRENT_ED:
			case utv_ohci_device::HC_BULK_HEAD_ED:
			case utv_ohci_device::HC_BULK_CURRENT_ED:
				m_regs[offset] = merged & ~0xf;
				break;

			case utv_ohci_device::HC_FM_INTERVAL:
				m_regs[utv_ohci_device::HC_FM_INTERVAL] = merged & 0xffff3fff;
				break;

			case utv_ohci_device::HC_PERIODIC_START:
				m_regs[utv_ohci_device::HC_PERIODIC_START] = merged & 0x3fff;
				break;

			case utv_ohci_device::HC_LS_THRESHOLD:
				m_regs[utv_ohci_device::HC_LS_THRESHOLD] = merged & 0x0fff;
				break;

			case utv_ohci_device::HC_RH_DESCRIPTOR_A:
				m_regs[utv_ohci_device::HC_RH_DESCRIPTOR_A] = (merged & 0xff001b00) | utv_ohci_device::PORT_COUNT;
				break;

			case utv_ohci_device::HC_RH_DESCRIPTOR_B:
				m_regs[utv_ohci_device::HC_RH_DESCRIPTOR_B] = merged;
				break;

			case utv_ohci_device::HC_RH_STATUS:
			{
				if (data & 0x80000000) // ClearRemoteWakeupEnable
					m_regs[utv_ohci_device::HC_RH_STATUS] &= ~0x00008000;

				if (data & 0x00008000) // SetRemoteWakeupEnable
					m_regs[utv_ohci_device::HC_RH_STATUS] |= 0x00008000;

				if (data & 0x00020000) // OverCurrentIndicatorChange
					m_regs[utv_ohci_device::HC_RH_STATUS] &= ~0x00020000;

				// Global power (LPS/LPSC) is ignored (the ports are always powered)
				break;
			}

			default:
				// HcRevision, HcPeriodCurrentED, HcDoneHead, HcFmRemaining and HcFmNumber are read only.
				break;
		}
	}
}

void utv_ohci_device::set_hcfs(uint32_t hcfs)
{
	if (hcfs == HCFS_RESET)
	{
		// USBRESET resets the root hub ports
		for (uint8_t port_idx = 0; port_idx < PORT_COUNT; port_idx++)
		{
			m_port[port_idx].enabled = false;
			m_port[port_idx].suspended = false;
			m_port[port_idx].resetting = false;
		}
	}
	else if (hcfs == HCFS_OPERATIONAL)
	{
		// Report connections that happened while the HCD wasn't looking (like before a software reset)
		for (uint8_t port_idx = 0; port_idx < PORT_COUNT; port_idx++)
		{
			if (m_port[port_idx].change)
				utv_ohci_device::set_irq(INT_RHSC, ASSERT_LINE);
		}
	}
}

void utv_ohci_device::set_irq(uint32_t mask, int status)
{
	if (status)
		m_regs[HC_INTERRUPT_STATUS] |= mask;
	else
		m_regs[HC_INTERRUPT_STATUS] &= ~mask;

	update_irq();
}

void utv_ohci_device::update_irq()
{
	const uint32_t ints_enabled = m_regs[HC_INTERRUPT_ENABLE];

	const bool state = (ints_enabled & INT_MIE) && (m_regs[HC_INTERRUPT_STATUS] & (ints_enabled & INT_MASK));

	if (state != m_irq_state)
	{
		m_irq_state = state;

		m_irq_cb(state ? ASSERT_LINE : CLEAR_LINE);
	}
}

//
// Root hub functions
//

wtvusb_device_interface *utv_ohci_device::port_function(uint8_t port_idx) const
{
	return m_ports[port_idx]->function();
}

uint32_t utv_ohci_device::port_status(uint8_t port_idx) const
{
	const root_port_t &p = m_port[port_idx];

	wtvusb_device_interface *function = utv_ohci_device::port_function(port_idx);

	uint32_t status = utv_ohci_device::PORT_PPS | p.change;

	if (function)
	{
		status |= utv_ohci_device::PORT_CCS;

		if (function->usb_low_speed())
			status |= utv_ohci_device::PORT_LSDA;
	}

	if (p.enabled)
		status |= utv_ohci_device::PORT_PES;

	if (p.suspended)
		status |= utv_ohci_device::PORT_PSS;

	if (p.resetting)
		status |= utv_ohci_device::PORT_PRS;

	return status;
}

void utv_ohci_device::port_change(uint8_t port_idx, uint32_t change)
{
	m_port[port_idx].change |= change;

	utv_ohci_device::set_irq(utv_ohci_device::INT_RHSC, ASSERT_LINE);
}

void utv_ohci_device::port_write(uint8_t port_idx, uint32_t data)
{
	root_port_t &p = m_port[port_idx];

	const bool connected = (utv_ohci_device::port_function(port_idx) != nullptr);

	// Writing 1 to a change bit clears it
	p.change &= ~(data & utv_ohci_device::PORT_CHANGE_MASK);

	if (data & utv_ohci_device::PORT_CCS) // ClearPortEnable
		p.enabled = false;

	if (data & utv_ohci_device::PORT_PES) // SetPortEnable
	{
		if (connected)
			p.enabled = true;
		else
			utv_ohci_device::port_change(port_idx, utv_ohci_device::PORT_CSC);
	}

	if (data & utv_ohci_device::PORT_PSS) // SetPortSuspend
	{
		if (connected)
			p.suspended = true;
		else
			utv_ohci_device::port_change(port_idx, utv_ohci_device::PORT_CSC);
	}

	if (data & utv_ohci_device::PORT_POCI) // ClearSuspendStatus (resume)
	{
		if (p.suspended)
		{
			p.suspended = false;

			utv_ohci_device::port_change(port_idx, utv_ohci_device::PORT_PSSC);
		}
	}

	if (data & utv_ohci_device::PORT_PRS) // SetPortReset
	{
		if (connected)
		{
			p.resetting = true;
			p.reset_ms = utv_ohci_device::PORT_RESET_MS;
		}
		else
		{
			utv_ohci_device::port_change(port_idx, utv_ohci_device::PORT_CSC);
		}
	}

	// SetPortPower / ClearPortPower are ignored
}

wtvusb_device_interface *utv_ohci_device::find_function(uint8_t address) const
{
	for (uint8_t port_idx = 0; port_idx < utv_ohci_device::PORT_COUNT; port_idx++)
	{
		const root_port_t &p = m_port[port_idx];

		wtvusb_device_interface *function = utv_ohci_device::port_function(port_idx);

		if (function && p.enabled && !p.suspended && !p.resetting && function->usb_address() == address)
			return function;
	}

	return nullptr;
}

//
// DMA access, OHCI structures are little endian.
//

uint32_t utv_ohci_device::dma_r32(uint32_t address)
{
	uint32_t data = m_dma->read_dword(address & ~3);

	return (m_dma->endianness() == ENDIANNESS_BIG) ? swapendian_int32(data) : data;
}

void utv_ohci_device::dma_w32(uint32_t address, uint32_t data)
{
	if (m_dma->endianness() == ENDIANNESS_BIG)
		data = swapendian_int32(data);

	m_dma->write_dword(address & ~3, data);
}

uint8_t utv_ohci_device::dma_r8(uint32_t address)
{
	return utv_ohci_device::dma_r32(address) >> ((address & 3) * 8);
}

void utv_ohci_device::dma_w8(uint32_t address, uint8_t data)
{
	const unsigned shift = (address & 3) * 8;

	utv_ohci_device::dma_w32(address, (utv_ohci_device::dma_r32(address) & ~(0xffU << shift)) | (uint32_t(data) << shift));
}

//
// Frame processing
//

TIMER_CALLBACK_MEMBER(utv_ohci_device::frame_tick)
{
	for (uint8_t port_idx = 0; port_idx < utv_ohci_device::PORT_COUNT; port_idx++)
	{
		root_port_t &p = m_port[port_idx];

		if (p.resetting && --p.reset_ms <= 0)
		{
			p.resetting = false;
			p.suspended = false;
			p.enabled = (utv_ohci_device::port_function(port_idx) != nullptr);

			if (wtvusb_device_interface *function = utv_ohci_device::port_function(port_idx))
				function->usb_bus_reset();

			utv_ohci_device::port_change(port_idx, utv_ohci_device::PORT_PRSC);
		}
	}

	if (((m_regs[utv_ohci_device::HC_CONTROL] & utv_ohci_device::CONTROL_HCFS) >> 6) != utv_ohci_device::HCFS_OPERATIONAL)
		return;

	const uint32_t hcca = m_regs[utv_ohci_device::HC_HCCA];

	m_frame_number++;
	if ((m_frame_number & 0x7fff) == 0)
		m_regs[utv_ohci_device::HC_INTERRUPT_STATUS] |= utv_ohci_device::INT_FNO;

	if (hcca)
		utv_ohci_device::dma_w32(hcca + 0x80, m_frame_number); // HccaFrameNumber, HccaPad1 = 0

	m_regs[utv_ohci_device::HC_INTERRUPT_STATUS] |= INT_SF;

	// Done queue writeback, delayed by the smallest DelayInterrupt of the TDs retired since the last one
	if (m_done_counter != 7 && m_done_counter > 0)
		m_done_counter--;

	if (m_done_counter == 0 && m_regs[utv_ohci_device::HC_DONE_HEAD] && hcca && !(m_regs[utv_ohci_device::HC_INTERRUPT_STATUS] & utv_ohci_device::INT_WDH))
	{
		const uint32_t inten = m_regs[utv_ohci_device::HC_INTERRUPT_ENABLE] & utv_ohci_device::INT_MASK & ~utv_ohci_device::INT_WDH;
		const uint32_t others = (m_regs[utv_ohci_device::HC_INTERRUPT_STATUS] & inten) ? 1 : 0;

		utv_ohci_device::dma_w32(hcca + 0x84, m_regs[utv_ohci_device::HC_DONE_HEAD] | others);

		m_regs[utv_ohci_device::HC_DONE_HEAD] = 0;
		m_regs[utv_ohci_device::HC_INTERRUPT_STATUS] |= utv_ohci_device::INT_WDH;

		m_done_counter = 7;
	}

	m_budget = utv_ohci_device::FRAME_BUDGET;

	if (hcca && (m_regs[utv_ohci_device::HC_CONTROL] & utv_ohci_device::CONTROL_PLE))
		utv_ohci_device::process_periodic();

	utv_ohci_device::process_async(utv_ohci_device::HC_CONTROL_HEAD_ED, utv_ohci_device::HC_CONTROL_CURRENT_ED, utv_ohci_device::COMMAND_CLF, utv_ohci_device::CONTROL_CLE);
	utv_ohci_device::process_async(utv_ohci_device::HC_BULK_HEAD_ED, utv_ohci_device::HC_BULK_CURRENT_ED, utv_ohci_device::COMMAND_BLF, utv_ohci_device::CONTROL_BLE);

	utv_ohci_device::update_irq();
}

void utv_ohci_device::process_periodic()
{
	uint32_t ed = utv_ohci_device::dma_r32(m_regs[utv_ohci_device::HC_HCCA] + (m_frame_number & 0x1f) * 4) & ~0xf;

	for (uint32_t ed_idx = 0; ed && ed_idx < utv_ohci_device::MAX_ED_COUNT; ed_idx++)
	{
		const uint32_t ed0 = utv_ohci_device::dma_r32(ed);

		m_regs[utv_ohci_device::HC_PERIOD_CURRENT_ED] = ed;

		if (ed0 & (1 << 15))
		{
			// Isochronous EDs aren't supported so they are skipped (they sit at the end of the periodic list)
			if (!(m_regs[utv_ohci_device::HC_CONTROL] & utv_ohci_device::CONTROL_IE))
				break;
		}
		else
		{
			utv_ohci_device::process_endpoint_descriptor(ed, 1);
		}

		ed = utv_ohci_device::dma_r32(ed + 12) & ~0xf;
	}

	m_regs[utv_ohci_device::HC_PERIOD_CURRENT_ED] = 0;
}

void utv_ohci_device::process_async(uint32_t head_reg, uint32_t current_reg, uint32_t filled, uint32_t enable)
{
	if (!(m_regs[utv_ohci_device::HC_CONTROL] & enable))
		return;
	else if (!(m_regs[utv_ohci_device::HC_COMMAND_STATUS] & filled))
		return;

	m_regs[utv_ohci_device::HC_COMMAND_STATUS] &= ~filled;

	bool pending = false;
	uint32_t ed = m_regs[head_reg];

	for (uint32_t ed_idx = 0; ed && ed_idx < utv_ohci_device::MAX_ED_COUNT; ed_idx++)
	{
		m_regs[current_reg] = ed;

		if (utv_ohci_device::process_endpoint_descriptor(ed, std::numeric_limits<uint16_t>::max()))
			pending = true;

		ed = utv_ohci_device::dma_r32(ed + 12) & ~0xf;
	}

	m_regs[current_reg] = 0;

	// The controller sets the filled bit when it finds work, so NAKed TDs are retried next frame
	if (pending)
		m_regs[utv_ohci_device::HC_COMMAND_STATUS] |= filled;
}

bool utv_ohci_device::process_endpoint_descriptor(uint32_t ed_address, uint16_t max_packets)
{
	const uint32_t ed0 = utv_ohci_device::dma_r32(ed_address);

	if (ed0 & (1 << 14)) // sKip
		return false;

	const uint32_t tailp = utv_ohci_device::dma_r32(ed_address + 4) & ~0xf;
	uint32_t headp = utv_ohci_device::dma_r32(ed_address + 8);
	uint16_t packets = 0;

	while (!(headp & 1) && (headp & ~0xf) != tailp)
	{
		if (m_budget <= 0 || packets >= max_packets)
			// ED still has TDs to run.
			return true;

		switch (process_transfer_descriptor(ed_address, ed0, headp, packets, max_packets))
		{
			case utv_ohci_device::TD_WAITING:
				// ED still has TDs to run.
				return true;

			case utv_ohci_device::TD_HALTED:
				return false;

			case utv_ohci_device::TD_RETIRED:
				break;
		}
	}

	return false;
}

utv_ohci_device::td_result_t utv_ohci_device::process_transfer_descriptor(uint32_t ed_address, uint32_t ed0, uint32_t &headp, uint16_t &packets, uint16_t max_packets)
{
	const uint32_t td = headp & ~0xf;
	const uint32_t d0 = utv_ohci_device::dma_r32(td);
	const uint32_t cbp = utv_ohci_device::dma_r32(td + 4);
	const uint32_t next_td = utv_ohci_device::dma_r32(td + 8) & ~0xf;
	const uint32_t be = utv_ohci_device::dma_r32(td + 12);

	const uint8_t function_address = ed0 & 0x7f;
	const int endpoint = (ed0 >> 7) & 0xf;
	const uint32_t direction = (ed0 >> 11) & 3;

	int max_packet = (ed0 >> 16) & 0x7ff;
	if (max_packet == 0)
		max_packet = 8;

	const bool rounding = BIT(d0, 18);
	const uint32_t t = (d0 >> 24) & 3;
	uint8_t toggle = (t & 2) ? (t & 1) : BIT(headp, 1);

	auto finish = [&] (uint8_t cc, uint32_t new_cbp, bool error) -> td_result_t
	{
		utv_ohci_device::retire_transfer_descriptor(td, d0, cc, toggle, new_cbp, next_td, error);

		headp = next_td | (toggle << 1) | (error ? 1 : 0);

		utv_ohci_device::dma_w32(ed_address + 8, headp);

		return error ? utv_ohci_device::TD_HALTED : utv_ohci_device::TD_RETIRED;
	};

	wtvusb_device_interface::usb_pid_t pid;

	uint32_t dp = (direction == 1 || direction == 2) ? direction : ((d0 >> 19) & 3);
	if (dp == 3)
		return finish(0x7, cbp, true); // UnexpectedPID

	pid = wtvusb_device_interface::usb_pid_t(dp); // 0=setup, 1=out, 2=in

	if (td != m_last_logged_td)
		m_last_logged_td = td;

	wtvusb_device_interface *function = utv_ohci_device::find_function(function_address);
	if (!function)
		return finish(utv_ohci_device::CC_DEVICE_NOT_RESPONDING, cbp, true);

	uint32_t remaining = 0;
	if (cbp)
	{
		if (((cbp ^ be) & ~0xfffU) == 0)
			remaining = (be >= cbp) ? (be - cbp + 1) : 0;
		else
			remaining = (0x1000 - (cbp & 0xfff)) + (be & 0xfff) + 1;
	}

	uint32_t current = cbp;
	bool progressed = false;
	uint8_t buffer[0x800];

	auto advance = [be] (uint32_t address) -> uint32_t
	{
		address++;

		if ((address & 0xfff) == 0)
			address = be & ~0xfffU; // second page

		return address;
	};

	while (true)
	{
		const int length = std::min<uint32_t>(max_packet, remaining);

		if (pid != wtvusb_device_interface::PID_IN)
		{
			uint32_t address = current;
			for (int i = 0; i < length; i++)
			{
				buffer[i] = utv_ohci_device::dma_r8(address);
				address = advance(address);
			}
		}

		int result = function->usb_transaction(pid, endpoint, buffer, length);

		if (result == wtvusb_device_interface::USB_NAK)
		{
			if (progressed)
			{
				utv_ohci_device::dma_w32(td, (d0 & ~(3U << 24)) | ((2U | toggle) << 24));
				utv_ohci_device::dma_w32(td + 4, current);
			}

			return utv_ohci_device::TD_WAITING;
		}
		else if (result == wtvusb_device_interface::USB_STALL)
		{
			return finish(CC_STALL, current, true);
		}
		else if (result > length)
		{
			return finish(CC_DATA_OVERRUN, current, true);
		}
		else
		{
			const int moved = (pid == wtvusb_device_interface::PID_IN) ? result : length;

			if (pid == wtvusb_device_interface::PID_IN)
			{
				for (int i = 0; i < moved; i++)
				{
					utv_ohci_device::dma_w8(current, buffer[i]);
					current = advance(current);
				}
			}
			else
			{
				for (int i = 0; i < moved; i++)
					current = advance(current);
			}

			remaining -= moved;
			toggle ^= 1;
			packets++;
			m_budget -= std::max(moved, 8);
			progressed = true;

			if (pid == wtvusb_device_interface::PID_IN && moved < length)
			{
				// Short packet, fine if bufferRounding is set, otherwise an underrun
				if (rounding)
					return finish(utv_ohci_device::CC_NO_ERROR, current, false);
				else
					return finish(utv_ohci_device::CC_DATA_UNDERRUN, current, true);
			}
			else if (remaining == 0)
			{
				return finish(utv_ohci_device::CC_NO_ERROR, 0, false);
			}
			else if (packets >= max_packets || m_budget <= 0)
			{
				utv_ohci_device::dma_w32(td, (d0 & ~(3U << 24)) | ((2U | toggle) << 24));
				utv_ohci_device::dma_w32(td + 4, current);

				return utv_ohci_device::TD_WAITING;
			}
		}
	}
}

void utv_ohci_device::retire_transfer_descriptor(uint32_t td_address, uint32_t d0, uint8_t cc, uint8_t toggle, uint32_t cbp, uint32_t next_td, bool error)
{
	// ConditionCode, ErrorCount = 0, DataToggle from the TD
	d0 = (d0 & 0x00ffffff) | (uint32_t(cc) << 28) | ((2U | toggle) << 24);
	if (cc == CC_DEVICE_NOT_RESPONDING)
		d0 |= 3U << 26;

	utv_ohci_device::dma_w32(td_address, d0);
	utv_ohci_device::dma_w32(td_address + 4, cbp);
	utv_ohci_device::dma_w32(td_address + 8, m_regs[HC_DONE_HEAD]);

	m_regs[HC_DONE_HEAD] = td_address;

	const uint8_t delay = (d0 >> 21) & 7;

	if (error)
		m_done_counter = 0;
	else if (delay != 7)
		m_done_counter = std::min(m_done_counter, delay);
}
