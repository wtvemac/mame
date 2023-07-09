/***********************************************************************************************

    solo1_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

************************************************************************************************/

#ifndef MAME_MACHINE_SOLO1_ASIC_H
#define MAME_MACHINE_SOLO1_ASIC_H

#pragma once

#include "cpu/mips/mips3.h"

class solo1_asic_device : public device_t
{
public:
	// construction/destruction
	solo1_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

    void regs_map(address_map &map);
    
protected:
	// device-level overrides
    virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
    uint32_t m_chip_id; // SOLO chip ID
    uint32_t m_chip_cntl;

    uint32_t m_int_status;
    uint32_t m_int_enable;

    uint32_t m_err_status;
    uint32_t m_err_enable;
    uint32_t m_err_address;
    
    uint32_t m_wd_reset_val;

    uint32_t m_lomem_rdprot_addr;
    uint32_t m_lomem_rdprot_mask;
    uint32_t m_lomem_wrprot_addr;
    uint32_t m_lomem_wrprot_mask;
    
    uint32_t m_tmr_count;
    uint32_t m_tmr_compare;

    uint32_t m_gpio_int_status; // GPIO interrupt status
    uint32_t m_gpio_int_enable; // GPIO interrupt enable
    uint32_t m_gpio_int_polling; // GPIO interrupt polling
    
    uint32_t m_aud_int_status; // audUnit interrupt status
    uint32_t m_aud_int_enable; // audUnit interrupt enable
    
    uint32_t m_dev_int_status; // devUnit interrupt status
    uint32_t m_dev_int_enable; // devUnit interrupt enable
    
    uint32_t m_vid_int_status; // vidUnit interrupt status
    uint32_t m_vid_int_enable; // vidUnit interrupt enable
    
    uint32_t m_rio_int_status; // RIO bus interrupt status
    uint32_t m_rio_int_enable; // RIO bus interrupt enable
    uint32_t m_rio_int_polling; // RIO bus interrupt polling
    
    uint32_t m_tim_int_status; // Timing interrupt status
    uint32_t m_tim_int_enable; // Timing interrupt enable
    
    uint32_t m_reset_cause;
    
    uint32_t m_java1_fence_addr_l;
    uint32_t m_java1_fence_addr_h;
    uint32_t m_java2_fence_addr_l;
    uint32_t m_java2_fence_addr_h;

    uint32_t m_memsize; // Any attempted RAM accesses above this address will trigger a bus error
    uint32_t m_fence_cntl;

    uint32_t m_bootmode;
    uint32_t m_use_bootmode;

    required_device<mips3_device> m_hostcpu;

    enum
    {
        SOLO_REGION_BUS,
        SOLO_REGION_RIO, // RIO bus
        SOLO_REGION_AUD, // Audio
        SOLO_REGION_VID, // Video
        SOLO_REGION_DEV, // I/O (printer, IR, I2C, GPIO, LEDs)
        SOLO_REGION_MEM, // Memory
        SOLO_REGION_GFX, // Graphics engine
        SOLO_REGION_DVE, // Digital video encoder
        SOLO_REGION_DIV, // Digital video interface
        SOLO_REGION_POT, // Pixel output
        SOLO_REGION_SUC, // Smart Card and UART
        SOLO_REGION_MOD  // Modem
    };

    uint32_t dma_bus_r(offs_t offset);
    void dma_bus_w(offs_t offset, uint32_t data);
    
    uint32_t dma_unk_r(offs_t offset);
    void dma_unk_w(offs_t offset, uint32_t data);
};

DECLARE_DEVICE_TYPE(SOLO1_ASIC, solo1_asic_device)

#endif // MAME_MACHINE_SOLO1_ASIC_H
