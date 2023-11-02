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
#include "machine/ds2401.h"

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
	template <typename T> void set_serial_id(T &&tag) { m_serial_id.set_tag(std::forward<T>(tag)); }

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

    uint32_t m_memcntl;
    uint32_t m_memrefcnt;
    uint32_t m_memdata;
    uint32_t m_memtiming;

private:
    required_device<mips3_device> m_hostcpu;
    required_device<ds2401_device> m_serial_id;

    emu_timer *m_sys_timer;
    //emu_timer *m_watchdog_timer;
    
    uint32_t m_compare_armed;

    int m_serial_id_tx;

    void spot_update_cycle_counting();

    TIMER_CALLBACK_MEMBER(sys_timer_callback);
    //TIMER_CALLBACK_MEMBER(watchdog_timer_callback);

    /* busUnit registers */

    uint32_t reg_0000_r(); // BUS_CHIPID (read-only)
    uint32_t reg_0004_r(); // BUS_CHPCNTL (read)
    void reg_0004_w(uint32_t data); // BUS_CHPCNTL (write)
    uint32_t reg_0008_r(); // BUS_INTSTAT (read)
    void reg_0108_w(uint32_t data); // BUS_INTSTAT (clear)
    uint32_t reg_000c_r(); // BUS_INTEN (read)
    void reg_000c_w(uint32_t data); // BUS_INTEN (set)
    void reg_010c_w(uint32_t data); // BUS_INTEN (clear)
    
    /* romUnit registers */

    uint32_t reg_1000_r(); // ROM_SYSCONF (read-only)
    uint32_t reg_1004_r(); // ROM_CNTL0 (read)
    void reg_1004_w(uint32_t data); // ROM_CNTL0 (write)
    uint32_t reg_1008_r(); // ROM_CNTL1 (read)
    void reg_1008_w(uint32_t data); // ROM_CNTL1 (write)

    /* audUnit registers */

    /* vidUnit registers */

    /* devUnit registers */

    uint32_t reg_4000_r(); // DEV_IRDATA (read-only)
    uint32_t reg_4004_r(); // DEV_LED (read)
    void reg_4004_w(uint32_t data); // DEV_LED (write)
    uint32_t reg_4008_r(); // DEV_IDCNTL (read)
    void reg_4008_w(uint32_t data); // DEV_IDCNTL (write)
    uint32_t reg_400c_r(); // DEV_NVCNTL (read)
    void reg_400c_w(uint32_t data); // DEV_NVCNTL (write)
    uint32_t reg_4010_r(); // DEV_SCCNTL (read)
    void reg_4010_w(uint32_t data); // DEV_SCCNTL (write)
    uint32_t reg_4014_r(); // DEV_EXTTIME (read)
    void reg_4014_w(uint32_t data); // DEV_EXTTIME (write)
    
    // The boot ROM seems to write to register 4018, which is a reserved register according to the documentation.

    uint32_t reg_4020_r(); // DEV_KBD0 (read)
    void reg_4020_w(uint32_t data); // DEV_KBD0 (write)
    uint32_t reg_4024_r(); // DEV_KBD1 (read)
    void reg_4024_w(uint32_t data); // DEV_KBD1 (write)
    uint32_t reg_4028_r(); // DEV_KBD2 (read)
    void reg_4028_w(uint32_t data); // DEV_KBD2 (write)
    uint32_t reg_402c_r(); // DEV_KBD3 (read)
    void reg_402c_w(uint32_t data); // DEV_KBD3 (write)
    uint32_t reg_4030_r(); // DEV_KBD4 (read)
    void reg_4030_w(uint32_t data); // DEV_KBD4 (write)
    uint32_t reg_4034_r(); // DEV_KBD5 (read)
    void reg_4034_w(uint32_t data); // DEV_KBD5 (write)
    uint32_t reg_4038_r(); // DEV_KBD6 (read)
    void reg_4038_w(uint32_t data); // DEV_KBD6 (write)
    uint32_t reg_403c_r(); // DEV_KBD7 (read)
    void reg_403c_w(uint32_t data); // DEV_KBD7 (write)

    /* memUnit registers */

    uint32_t reg_5000_r(); // MEM_CNTL (read)
    void reg_5000_w(uint32_t data); // MEM_CNTL (write)
    uint32_t reg_5004_r(); // MEM_REFCNT (read)
    void reg_5004_w(uint32_t data); // MEM_REFCNT (write)
    uint32_t reg_5008_r(); // MEM_DATA (read)
    void reg_5008_w(uint32_t data); // MEM_DATA (write)
    void reg_500c_w(uint32_t data); // MEM_CMD (write-only)
    uint32_t reg_5010_r(); // MEM_TIMING (read)
    void reg_5010_w(uint32_t data); // MEM_TIMING (write)
};

DECLARE_DEVICE_TYPE(SPOT_ASIC, spot_asic_device)

#endif