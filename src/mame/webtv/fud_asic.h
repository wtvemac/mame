// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_FUD_ASIC_H
#define MAME_MACHINE_FUD_ASIC_H

#pragma once

#include "cpu/mips/mips3.h"

constexpr uint32_t GPIO_TUNER0_LOCK = 0x00000002;
constexpr uint32_t GPIO_TUNER1_LOCK = 0x00000004;

constexpr uint32_t TUNER_LOCK_HZ       = 2;

class fud_asic_device : public device_t
{

public:

	fud_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t gpio_base_address, uint32_t clock = 0);

	void cntl_map(address_map &map);
	void gpio_map(address_map &map);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	required_device<mips3_device> m_hostcpu;

	emu_timer *tuner_lock_timer = nullptr;
	TIMER_CALLBACK_MEMBER(set_tuner_lock);

	uint32_t m_fud_cntl;

	uint32_t cntl_0008_r();          //
	void cntl_0008_w(uint32_t data); //
	uint32_t cntl_000c_r();          //
	void cntl_000c_w(uint32_t data); //

	uint32_t m_gpio_base;
	uint32_t m_gpio_intenable;
	uint32_t m_gpio_intstat;
	
	void set_gpio_irq(uint32_t mask, int state);

	uint32_t gpio_002c_r();          // GPIO_INTSTAT_S/GPIO_INTEN_S? (read)
	void gpio_002c_w(uint32_t data); // GPIO_INTSTAT_S/GPIO_INTEN_S? (write)
	uint32_t gpio_0030_r();          // GPIO_INTSTAT_C (read)
	void gpio_0030_w(uint32_t data); // GPIO_INTSTAT_C (write)
	uint32_t gpio_0034_r();          // GPIO_INTEN_S (read)
	void gpio_0034_w(uint32_t data); // GPIO_INTEN_S (write)
	uint32_t gpio_003c_r();          // GPIO_INTEN_C (read)
	void gpio_003c_w(uint32_t data); // GPIO_INTEN_C (write)

};

DECLARE_DEVICE_TYPE(FUD_ASIC, fud_asic_device)

#endif // MAME_MACHINE_FUD_ASIC_H