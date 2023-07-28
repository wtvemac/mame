/***********************************************************************************************

    solo1_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The technical specifications that this implementation is based on can be found here:
    http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf

************************************************************************************************/

#ifndef MAME_MACHINE_SOLO1_ASIC_H
#define MAME_MACHINE_SOLO1_ASIC_H

#pragma once

#include "diserial.h"

#include "cpu/mips/mips3.h"
#include "solo1_asic_vid.h"

class solo1_asic_device : public device_t, public device_serial_interface
{
public:
	// construction/destruction
	solo1_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

    void regs_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }

    void set_aud_int_flag(uint32_t value);
    void set_vid_int_flag(uint32_t value);
    void set_rio_int_flag(uint32_t value);

    uint32_t reg_bus_r(offs_t offset);
    void reg_bus_w(offs_t offset, uint32_t data);
    
    //uint32_t reg_rio_r(offs_t offset);
    //void reg_rio_w(offs_t offset, uint32_t data);

    uint32_t reg_dev_r(offs_t offset);
    void reg_dev_w(offs_t offset, uint32_t data);
    
    uint32_t reg_mem_r(offs_t offset);
    void reg_mem_w(offs_t offset, uint32_t data);
    
    //uint32_t reg_suc_r(offs_t offset);
    //void reg_suc_w(offs_t offset, uint32_t data);
    
protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

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

private:
    required_device<mips3_device> m_hostcpu;

    emu_timer *m_sys_timer;
    //emu_timer *m_watchdog_timer;
    
    uint32_t m_compare_armed;

    void solo1_update_cycle_counting();

    TIMER_CALLBACK_MEMBER(sys_timer_callback);
    //TIMER_CALLBACK_MEMBER(watchdog_timer_callback);

    /* Begin devUnit registers */

    uint32_t m_dev_irold;
    uint32_t m_dev_led;
    uint32_t m_dev_id_chip_cntl;
    uint32_t m_dev_iic_cntl;

    uint32_t m_dev_gpio_in;
    uint32_t m_dev_gpio_out;
    uint32_t m_dev_gpio_en;

    uint32_t m_dev_ir_in_sample_int;
    uint32_t m_dev_ir_in_reject_int;
    uint32_t m_dev_ir_in_transition_data;
    uint32_t m_dev_ir_in_status_cntl;
    
    uint32_t m_dev_ir_out_fifo;
    uint32_t m_dev_ir_out_status;
    uint32_t m_dev_ir_out_period;
    uint32_t m_dev_ir_out_on;
    uint32_t m_dev_ir_out_current_period;
    uint32_t m_dev_ir_out_current_on;
    uint32_t m_dev_ir_out_current_count;

    uint32_t m_dev_parallel_data;
    uint32_t m_dev_parallel_ctrl;
    uint32_t m_dev_parallel_status;
    uint32_t m_dev_parallel_cnfg;
    uint32_t m_dev_parallel_fifo_ctrl;
    uint32_t m_dev_parallel_fifo_status;
    uint32_t m_dev_parallel_timeout;
    uint32_t m_dev_parallel_stat2;
    uint32_t m_dev_parallel_int_enable;
    uint32_t m_dev_parallel_int_status;
    uint32_t m_dev_parallel_clr_int;
    uint32_t m_dev_parallel_enable;
    
    uint32_t m_dev_diag;
    uint32_t m_dev_devdiag;

    /* End devUnit registers */

    /* Begin memUnit registers */

    uint32_t m_mem_timing; // SDRAM timing parameters
    uint32_t m_mem_cntl;
    uint32_t m_mem_burp; // Memory access arbitration control
    uint32_t m_mem_refresh_cntl; // SDRAM refresh control
    uint32_t m_mem_cmd; // SDRAM commands

    /* End memUnit registers */

    /* Begin sucUnit registers */

    uint32_t m_sucgpu_tff_hr; // TX FIFO data
    uint32_t m_sucgpu_tff_hrsrw; // TX FIFO data (debug)
    uint32_t m_sucgpu_tff_trg; // TX FIFO trigger level
    uint32_t m_sucgpu_tff_cnt; // Current number of entries in TX FIFO
    uint32_t m_sucgpu_tff_max; // Maximum TX FIFO depth
    uint32_t m_sucgpu_tff_ctl;
    uint32_t m_sucgpu_tff_sta;
    uint32_t m_sucgpu_tff_gcr;

    /* End sucUnit registers */
    
};

DECLARE_DEVICE_TYPE(SOLO1_ASIC, solo1_asic_device)

#endif // MAME_MACHINE_SOLO1_ASIC_H
