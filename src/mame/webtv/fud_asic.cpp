// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "fud_asic.h"

DEFINE_DEVICE_TYPE(FUD_ASIC, fud_asic_device, "fud_asic_device", "WebTV FUD ASIC")

fud_asic_device::fud_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t gpio_base_address, uint32_t clock) :
	device_t(mconfig, FUD_ASIC, tag, owner, clock),
	m_hostcpu(*owner, "maincpu")
{
	m_gpio_base = gpio_base_address;
}

void fud_asic_device::device_start()
{
	tuner_lock_timer = timer_alloc(FUNC(fud_asic_device::set_tuner_lock), this);

	save_item(NAME(m_fud_cntl));
	save_item(NAME(m_gpio_base));
	save_item(NAME(m_gpio_intenable));
	save_item(NAME(m_gpio_intstat));
}

void fud_asic_device::device_reset()
{
	m_fud_cntl = 0x0;
	m_gpio_intenable = 0x0;
	m_gpio_intstat = 0x0;
}

void fud_asic_device::cntl_map(address_map &map)
{
	map(0x008, 0x00b).rw(FUNC(fud_asic_device::cntl_0008_r), FUNC(fud_asic_device::cntl_0008_w));
	map(0x00c, 0x00f).rw(FUNC(fud_asic_device::cntl_000c_r), FUNC(fud_asic_device::cntl_000c_w));
}

void fud_asic_device::gpio_map(address_map &map)
{
	map(0x02c, 0x02f).rw(FUNC(fud_asic_device::gpio_002c_r), FUNC(fud_asic_device::gpio_002c_w));
	map(0x030, 0x033).rw(FUNC(fud_asic_device::gpio_0030_r), FUNC(fud_asic_device::gpio_0030_w));
	map(0x034, 0x037).rw(FUNC(fud_asic_device::gpio_0034_r), FUNC(fud_asic_device::gpio_0034_w));
	map(0x03c, 0x03f).rw(FUNC(fud_asic_device::gpio_003c_r), FUNC(fud_asic_device::gpio_003c_w));
}

TIMER_CALLBACK_MEMBER(fud_asic_device::set_tuner_lock)
{
	uint32_t gpio_tuner_lock = GPIO_TUNER0_LOCK | GPIO_TUNER1_LOCK;

	m_gpio_intenable |= gpio_tuner_lock;

	if (!(m_gpio_intstat & gpio_tuner_lock))
		fud_asic_device::set_gpio_irq(gpio_tuner_lock, ASSERT_LINE);
}

void fud_asic_device::set_gpio_irq(uint32_t mask, int state)
{
	if (m_gpio_intenable & mask)
	{
		if (state)
		{
			m_gpio_intstat |= mask;

			m_hostcpu->set_input_line(MIPS3_IRQ2, ASSERT_LINE);
		}
		else
		{
			m_gpio_intstat &= (~mask);

			if(m_gpio_intstat == 0x00)
				m_hostcpu->set_input_line(MIPS3_IRQ2, CLEAR_LINE);
		}
	}
}

uint32_t fud_asic_device::cntl_0008_r()
{
	return 0x00000000;
}

void fud_asic_device::cntl_0008_w(uint32_t data)
{
	m_fud_cntl = data;
}

uint32_t fud_asic_device::cntl_000c_r()
{
	if (m_fud_cntl == 0x80009818)
	{
		return 0x000cf000;
	}
	else if (m_fud_cntl == 0x80000018)
	{
		tuner_lock_timer->adjust(attotime::from_hz(TUNER_LOCK_HZ), 0, attotime::from_hz(TUNER_LOCK_HZ));

		return m_gpio_base;
	}
	else
	{
		return 0x80201414;
	}
}

void fud_asic_device::cntl_000c_w(uint32_t data)
{
	//
}

uint32_t fud_asic_device::gpio_002c_r()
{
	return m_gpio_intenable;
}

void fud_asic_device::gpio_002c_w(uint32_t data)
{
	//
}

uint32_t fud_asic_device::gpio_0030_r()
{
	return m_gpio_intstat;
}

void fud_asic_device::gpio_0030_w(uint32_t data)
{
	fud_asic_device::set_gpio_irq(data, CLEAR_LINE);
}

uint32_t fud_asic_device::gpio_0034_r()
{
	return m_gpio_intenable;
}

void fud_asic_device::gpio_0034_w(uint32_t data)
{
	m_gpio_intenable |= data;
}

uint32_t fud_asic_device::gpio_003c_r()
{
	return m_gpio_intenable;
}

void fud_asic_device::gpio_003c_w(uint32_t data)
{
	fud_asic_device::gpio_0030_w(data);

	m_gpio_intenable &= (~data);
}
