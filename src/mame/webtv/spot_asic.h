/***********************************************************************************************

    spot_asic.cpp

    WebTV Networks Inc. SPOT ASIC

    This ASIC controls most of the I/O on the 1st generation WebTV hardware. It is also referred
    to as FIDO on the actual chip that implements the SPOT logic.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

************************************************************************************************/

#ifndef MAME_MACHINE_SPOT_ASIC_H
#define MAME_MACHINE_SPOT_ASIC_H

#pragma once

#include "diserial.h"

#include "cpu/mips/mips3.h"

#define ERR_F1READ  1 << 6
#define ERR_F1WRITE 1 << 5
#define ERR_F2READ  1 << 4
#define ERR_F2WRITE 1 << 3
#define ERR_TIMEOUT 1 << 2
#define ERR_OW      1 << 0 // double-fault

class spot_asic_device : public device_t, public device_serial_interface
{
public:
	// construction/destruction
	spot_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
    
    void bus_unit_map(address_map &map);
    void rom_unit_map(address_map &map);
    void aud_unit_map(address_map &map);
    void vid_unit_map(address_map &map);
    void dev_unit_map(address_map &map);
    void mem_unit_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

    uint32_t m_fence1_lower_addr;
    uint32_t m_fence1_upper_addr;
    uint32_t m_fence2_lower_addr;
    uint32_t m_fence2_upper_addr;

    uint8_t m_intenable;
    uint8_t m_intstat;
    
    uint8_t m_errenable;
    uint8_t m_errstat;

private:
    required_device<mips3_device> m_hostcpu;

    emu_timer *m_sys_timer;
    //emu_timer *m_watchdog_timer;
    
    uint32_t m_compare_armed;

    void spot_update_cycle_counting();

    TIMER_CALLBACK_MEMBER(sys_timer_callback);
    //TIMER_CALLBACK_MEMBER(watchdog_timer_callback);

    // BUS_CHIPID (read-only) - Indicates the SPOT chip revision
    uint32_t reg_0000_r();

    // BUS_CHIPID (R/W)
    uint32_t reg_0004_r();
    void reg_0004_w(uint32_t data);

    // BUS_INTSTAT (read-only)
    uint32_t reg_0008_r();
    // BUS_INTSTAT (clear)
    void reg_0108_w(uint32_t data);

    // BUS_INTEN (R/Set)
    uint32_t reg_000c_r();
    void reg_000c_w(uint32_t data);
    // BUS_INTEN (clear)
    void reg_010c_w(uint32_t data);
    
    // ROM_SYSCONF (read-only) - Contains various system parameters.
    uint32_t reg_1000_r();
};

DECLARE_DEVICE_TYPE(SPOT_ASIC, spot_asic_device)

#endif