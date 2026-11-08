// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvport.h"

DEFINE_DEVICE_TYPE(WTVPORT, wtvport_device, "wtvport", "WebTV Port")
DEFINE_DEVICE_TYPE(WTVPORT_SLOT, wtvport_slot_device, "wtvport_slot", "WebTV Port Slot")

wtvport_device::wtvport_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVPORT, tag, owner, clock),
	m_space(*this, finder_base::DUMMY_TAG, -1),
	m_base(0),
	m_irq_cb(*this),
	m_diag_cb(*this),
	m_timeout_cb(*this)
{
}

void wtvport_device::device_start()
{
	m_irq_state = 0;
	m_diag_state = 0;

	save_item(NAME(m_irq_state));
	save_item(NAME(m_diag_state));

	// Nothing answers in an empty slot. Devices install their own handler over this when they start.
	m_space->install_readwrite_handler(m_base, m_base + (SLOT_COUNT * SLOT_SIZE) - 1, emu::rw_delegate(*this, FUNC(wtvport_device::empty_r)), emu::rw_delegate(*this, FUNC(wtvport_device::empty_w)));
}

void wtvport_device::device_reset()
{
	m_irq_state = 0;
	m_diag_state = 0;
	m_irq_cb(CLEAR_LINE);
	m_diag_cb(CLEAR_LINE);
}

uint32_t wtvport_device::empty_r(offs_t offset, uint32_t mem_mask)
{
	if (!machine().side_effects_disabled())
		m_timeout_cb(m_base + (offset << 2), 0);

	return 0xffffffff;
}

void wtvport_device::empty_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	if (!machine().side_effects_disabled())
		m_timeout_cb(m_base + (offset << 2), 1);
}

void wtvport_device::set_slot_irq(uint32_t slot, int state)
{
	const uint32_t old_state = m_irq_state;

	if (state)
		m_irq_state |= (1 << slot);
	else
		m_irq_state &= ~(1 << slot);

	if ((old_state != 0) != (m_irq_state != 0))
		m_irq_cb(m_irq_state != 0 ? ASSERT_LINE : CLEAR_LINE);
}

void wtvport_device::set_slot_diag(uint32_t slot, int state)
{
	const uint32_t old_state = m_diag_state;

	if (state)
		m_diag_state |= (1 << slot);
	else
		m_diag_state &= ~(1 << slot);

	if ((old_state != 0) != (m_diag_state != 0))
		m_diag_cb(m_diag_state != 0 ? ASSERT_LINE : CLEAR_LINE);
}

wtvport_slot_device::wtvport_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVPORT_SLOT, tag, owner, clock),
	device_single_card_slot_interface<device_wtvport_card_interface>(mconfig, *this),
	m_port(*this, finder_base::DUMMY_TAG),
	m_slot(0)
{
}

void wtvport_slot_device::device_start()
{
}

device_wtvport_card_interface::device_wtvport_card_interface(const machine_config &mconfig, device_t &device) :
	device_interface(device, "wtvport"),
	m_fixed_port(device, finder_base::DUMMY_TAG),
	m_port(nullptr),
	m_slot(0)
{
}

device_wtvport_card_interface::~device_wtvport_card_interface()
{
}

void device_wtvport_card_interface::interface_pre_start()
{
	wtvport_slot_device *const slot = dynamic_cast<wtvport_slot_device *>(device().owner());

	if (slot)
	{
		m_port = &slot->port();
		m_slot = slot->slot();
	}
	else if (m_fixed_port)
	{
		m_port = m_fixed_port.target();
	}
	else
	{
		throw emu_fatalerror("%s: WebTV Port card must be in a WebTV Port slot or placed with set_wtvport()\n", device().tag());
	}

	if (m_slot >= wtvport_device::SLOT_COUNT)
		throw emu_fatalerror("%s: WebTV Port slot %u is out of range\n", device().tag(), m_slot);

	// The port installs the empty-slot handlers. Devices install over them.
	if (!m_port->started())
		throw device_missing_dependencies();
}
