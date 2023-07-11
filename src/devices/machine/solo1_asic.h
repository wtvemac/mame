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

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
    
protected:
	// device-level overrides
    virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
    /* Begin busUnit registers */
    uint32_t m_bus_chip_id; // SOLO chip ID
    uint32_t m_bus_chip_cntl;

    uint32_t m_bus_int_status;
    uint32_t m_bus_int_enable;

    uint32_t m_bus_err_status;
    uint32_t m_bus_err_enable;
    uint32_t m_bus_err_address;
    
    uint32_t m_bus_wd_reset_val;

    uint32_t m_bus_lomem_rdprot_addr;
    uint32_t m_bus_lomem_rdprot_mask;
    uint32_t m_bus_lomem_wrprot_addr;
    uint32_t m_bus_lomem_wrprot_mask;
    
    uint32_t m_bus_tmr_count;
    uint32_t m_bus_tmr_compare;

    uint32_t m_bus_gpio_int_status; // GPIO interrupt status
    uint32_t m_bus_gpio_int_enable; // GPIO interrupt enable
    uint32_t m_bus_gpio_int_polling; // GPIO interrupt polling
    
    uint32_t m_bus_aud_int_status; // audUnit interrupt status
    uint32_t m_bus_aud_int_enable; // audUnit interrupt enable
    
    uint32_t m_bus_dev_int_status; // devUnit interrupt status
    uint32_t m_bus_dev_int_enable; // devUnit interrupt enable
    
    uint32_t m_bus_vid_int_status; // vidUnit interrupt status
    uint32_t m_bus_vid_int_enable; // vidUnit interrupt enable
    
    uint32_t m_bus_rio_int_status; // RIO bus interrupt status
    uint32_t m_bus_rio_int_enable; // RIO bus interrupt enable
    uint32_t m_bus_rio_int_polling; // RIO bus interrupt polling
    
    uint32_t m_bus_tim_int_status; // Timing interrupt status
    uint32_t m_bus_tim_int_enable; // Timing interrupt enable
    
    uint32_t m_bus_reset_cause;
    
    uint32_t m_bus_java1_fence_addr_l;
    uint32_t m_bus_java1_fence_addr_h;
    uint32_t m_bus_java2_fence_addr_l;
    uint32_t m_bus_java2_fence_addr_h;

    uint32_t m_bus_memsize; // Any attempted RAM accesses above this address will trigger a bus error
    uint32_t m_bus_fence_cntl;

    uint32_t m_bus_bootmode;
    uint32_t m_bus_use_bootmode;

    /* End busUnit registers */

    /* Begin memUnit registers */

    uint32_t m_mem_timing; // SDRAM timing parameters
    uint32_t m_mem_cntl;
    uint32_t m_mem_burp; // Memory access arbitration control
    uint32_t m_mem_refresh_cntl; // SDRAM refresh control
    uint32_t m_mem_cmd; // SDRAM commands

    /* End memUnit registers */

    required_device<mips3_device> m_hostcpu;

    uint32_t dma_bus_r(offs_t offset);
    void dma_bus_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_rio_r(offs_t offset);
    //void dma_rio_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_aud_r(offs_t offset);
    //void dma_aud_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_vid_r(offs_t offset);
    //void dma_vid_w(offs_t offset, uint32_t data);

    //uint32_t dma_dev_r(offs_t offset);
    //void dma_dev_w(offs_t offset, uint32_t data);
    
    uint32_t dma_mem_r(offs_t offset);
    void dma_mem_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_gfx_r(offs_t offset);
    //void dma_gfx_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_dve_r(offs_t offset);
    //void dma_dve_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_div_r(offs_t offset);
    //void dma_div_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_pot_r(offs_t offset);
    //void dma_pot_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_suc_r(offs_t offset);
    //void dma_suc_w(offs_t offset, uint32_t data);
    
    //uint32_t dma_mod_r(offs_t offset);
    //void dma_mod_w(offs_t offset, uint32_t data);
};

DECLARE_DEVICE_TYPE(SOLO1_ASIC, solo1_asic_device)

#endif // MAME_MACHINE_SOLO1_ASIC_H
