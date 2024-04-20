// license:BSD-3-Clause
// copyright-holders:FairPlay137

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
#include "machine/8042kbdc.h"
#include "machine/at_keybc.h"
#include "machine/ds2401.h"
#include "machine/i2cmem.h"

#define SYSCONFIG_ROMTYP0    1 << 31 // ROM bank 0 is present
#define SYSCONFIG_ROMMODE0   1 << 30 // ROM bank 0 supports page mode
#define SYSCONFIG_ROMTYP1    1 << 27 // ROM bank 1 is present
#define SYSCONFIG_ROMMODE1   1 << 26 // ROM bank 1 supports page mode
#define SYSCONFIG_AUDDACMODE 1 << 17 // use external DAC clock
#define SYSCONFIG_VIDCLKSRC  1 << 16 // use external video encoder clock
#define SYSCONFIG_CPUBUFF    1 << 13 // 0=50% output buffer strength, 1=83% output buffer strength
#define SYSCONFIG_NTSC       1 << 11 // use NTSC mode

#define EMUCONFIG_PBUFF0         0      // Render the screen using data exactly at nstart. Only seen in the prealpha bootrom.
#define EMUCONFIG_PBUFF1         1 << 0 // Render the screen using data one buffer length beyond nstart. Seems to be what they settled on.
#define EMUCONFIG_BANGSERIAL     3 << 2
#define EMUCONFIG_BANGSERIAL_V1  1 << 2
#define EMUCONFIG_BANGSERIAL_V2  1 << 3
#define EMUCONFIG_SCREEN_UPDATES 1 << 4

#define ERR_F1READ  1 << 6 // BUS_FENADDR1 read fence check error
#define ERR_F1WRITE 1 << 5 // BUS_FENADDR1 write fence check error
#define ERR_F2READ  1 << 4 // BUS_FENADDR2 read fence check error
#define ERR_F2WRITE 1 << 3 // BUS_FENADDR2 write fence check error
#define ERR_TIMEOUT 1 << 2 // io timeout error
#define ERR_OW      1 << 0 // double-fault

#define BUS_INT_VIDINT 1 << 7 // vidUnit interrupt (program should read VID_INTSTAT)
#define BUS_INT_DEVKBD 1 << 6 // keyboard IRQ
#define BUS_INT_DEVMOD 1 << 5 // modem IRQ
#define BUS_INT_DEVIR  1 << 4 // IR data ready to read
#define BUS_INT_DEVSMC 1 << 3 // SmartCard inserted
#define BUS_INT_AUDDMA 1 << 2 // audUnit DMA completion

#define NTSC_SCREEN_WIDTH  640
#define NTSC_SCREEN_HEIGHT 480
#define NTSC_SCREEN_HZ     60
#define PAL_SCREEN_WIDTH   768
#define PAL_SCREEN_HEIGHT  560
#define PAL_SCREEN_HZ      50

#define VID_Y_BLACK         0x10
#define VID_Y_WHITE         0xeb
#define VID_Y_RANGE         (VID_Y_WHITE - VID_Y_BLACK)
#define VID_UV_OFFSET       0x80
#define VID_BYTES_PER_PIXEL 2
#define VID_DEFAULT_WIDTH   NTSC_SCREEN_WIDTH
#define VID_DEFAULT_HEIGHT  NTSC_SCREEN_HEIGHT
#define VID_DEFAULT_HZ      NTSC_SCREEN_HZ
#define VID_DEFAULT_HSTART  0
#define VID_DEFAULT_HSIZE   560
#define VID_DEFAULT_VSTART  0
#define VID_DEFAULT_VSIZE   420
#define VID_DEFAULT_COLOR   (VID_UV_OFFSET << 0x10) | (VID_Y_BLACK << 0x08) | VID_UV_OFFSET;

#define VID_INT_FIDO   1 << 6 // TODO: docs don't have info on FIDO mode! figure this out!
#define VID_INT_VSYNCE 1 << 5 // even field VSYNC
#define VID_INT_VSYNCO 1 << 4 // odd field VSYNC
#define VID_INT_HSYNC  1 << 3 // HSYNC on line specified by VID_HINTLINE
#define VID_INT_DMA    1 << 2 // vidUnit DMA completion

#define VID_FCNTL_UVSELSWAP  1 << 7 // UV is swapped. 1=YCbYCr, 0=YCrYCb
#define VID_FCNTL_CRCBINVERT 1 << 6 // invert MSB Cb and Cb
#define VID_FCNTL_FIDO       1 << 5 // enable FIDO mode. details unknown
#define VID_FCNTL_GAMMA      1 << 4 // enable gamma correction
#define VID_FCNTL_BLNKCOLEN  1 << 3 // enable VID_BLANK color
#define VID_FCNTL_INTERLACE  1 << 2 // interlaced video enabled
#define VID_FCNTL_PAL        1 << 1 // PAL mode enabled
#define VID_FCNTL_VIDENAB    1 << 0 // video output enable

#define VID_DMACNTL_ITRLEN 1 << 3 // interlaced video in DMA channel
#define VID_DMACNTL_DMAEN  1 << 2 // DMA channel enabled
#define VID_DMACNTL_NV     1 << 1 // DMA next registers are valid
#define VID_DMACNTL_NVF    1 << 0 // DMA next registers are always valid

#define NVCNTL_SCL      1 << 3
#define NVCNTL_WRITE_EN 1 << 2
#define NVCNTL_SDA_W    1 << 1
#define NVCNTL_SDA_R    1 << 0

class spot_asic_device : public device_t, public device_serial_interface, public device_video_interface
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
	template <typename T> void set_nvram(T &&tag) { m_nvram.set_tag(std::forward<T>(tag)); }

    uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

    void pixel_buffer_index_update();
protected:
	// device-level overrides
    virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;
    void activate_ntsc_screen();
    void activate_pal_screen();

    uint32_t m_fence1_lower_addr;
    uint32_t m_fence1_upper_addr;
    uint32_t m_fence2_lower_addr;
    uint32_t m_fence2_upper_addr;

    uint8_t m_intenable;
    uint8_t m_intstat;
    
    uint8_t m_errenable;
    uint8_t m_errstat;

    uint16_t m_timeout_count;
    uint16_t m_timeout_compare;

    uint32_t m_memcntl;
    uint32_t m_memrefcnt;
    uint32_t m_memdata;
    uint32_t m_memtiming;

    uint8_t m_nvcntl;

    uint32_t m_ledstate;

    uint8_t m_fcntl;

    uint32_t m_vid_nstart;
    uint32_t m_vid_nsize;
    uint32_t m_vid_dmacntl;
    uint32_t m_vid_hstart;
    uint32_t m_vid_hsize;
    uint32_t m_vid_vstart;
    uint32_t m_vid_vsize;
    uint32_t m_vid_fcntl;
    uint32_t m_vid_blank_color;
    uint32_t m_vid_cstart;
    uint32_t m_vid_csize;
    uint32_t m_vid_ccnt;
    uint32_t m_vid_cline;
    uint32_t m_vid_hintline;
    uint32_t m_vid_intenable;
    uint32_t m_vid_intstat;

    uint32_t m_vid_drawstart;
    uint32_t m_vid_drawvsize;

    uint16_t m_smrtcrd_serial_bitmask = 0x0;
    uint16_t m_smrtcrd_serial_rxdata = 0x0;
private:
    required_device<mips3_device> m_hostcpu;
    required_device<ds2401_device> m_serial_id;
    required_device<i2cmem_device> m_nvram;
    required_device<kbdc8042_device> m_kbdc;
	required_device<screen_device> m_screen;
    
	required_ioport m_sys_config;
	required_ioport m_emu_config;

	output_finder<> m_power_led;
	output_finder<> m_connect_led;
	output_finder<> m_message_led;

    void vblank_irq(int state);
	void irq_keyboard_w(int state);
    
    emu_timer *m_sys_timer;
    //emu_timer *m_watchdog_timer;
    
    uint32_t m_compare_armed;

    int m_serial_id_tx;

    void set_bus_irq(uint8_t mask, int state);
    void set_vid_irq(uint8_t mask, int state);

    void validate_active_area();
    void spot_update_cycle_counting();

    TIMER_CALLBACK_MEMBER(sys_timer_callback);
    //TIMER_CALLBACK_MEMBER(watchdog_timer_callback);
    
    TIMER_CALLBACK_MEMBER(vid_dma_complete);
    //TIMER_CALLBACK_MEMBER(aud_dma_complete);

    /* busUnit registers */

    uint32_t reg_0000_r(); // BUS_CHIPID (read-only)
    uint32_t reg_0004_r(); // BUS_CHPCNTL (read)
    void reg_0004_w(uint32_t data); // BUS_CHPCNTL (write)
    uint32_t reg_0008_r(); // BUS_INTSTAT (read)
    void reg_0108_w(uint32_t data); // BUS_INTSTAT (clear)
    uint32_t reg_000c_r(); // BUS_INTEN (read)
    void reg_000c_w(uint32_t data); // BUS_INTEN (set)
    void reg_010c_w(uint32_t data); // BUS_INTEN (clear)
    uint32_t reg_0010_r(); // BUS_ERRSTAT (read)
    void reg_0110_w(uint32_t data); // BUS_ERRSTAT (clear)
    uint32_t reg_0014_r(); // BUS_ERREN_S (read)
    void reg_0014_w(uint32_t data); // BUS_ERREN_S (write)
    void reg_0114_w(uint32_t data); // BUS_ERREN_C (clear)
    uint32_t reg_0018_r(); // BUS_ERRADDR (read-only)
    void reg_0118_w(uint32_t data); // BUS_WDREG_C (clear)
    uint32_t reg_001c_r(); // BUS_FENADDR1 (read)
    void reg_001c_w(uint32_t data); // BUS_FENADDR1 (write)
    uint32_t reg_0020_r(); // BUS_FENMASK1 (read)
    void reg_0020_w(uint32_t data); // BUS_FENMASK1 (write)
    uint32_t reg_0024_r(); // BUS_FENADDR1 (read)
    void reg_0024_w(uint32_t data); // BUS_FENADDR1 (write)
    uint32_t reg_0028_r(); // BUS_FENMASK2 (read)
    void reg_0028_w(uint32_t data); // BUS_FENMASK2 (write)
    
    /* romUnit registers */

    uint32_t reg_1000_r(); // ROM_SYSCONF (read-only)
    uint32_t reg_1004_r(); // ROM_CNTL0 (read)
    void reg_1004_w(uint32_t data); // ROM_CNTL0 (write)
    uint32_t reg_1008_r(); // ROM_CNTL1 (read)
    void reg_1008_w(uint32_t data); // ROM_CNTL1 (write)

    /* audUnit registers */

    uint32_t reg_2000_r(); // AUD_CSTART (read-only)
    uint32_t reg_2004_r(); // AUD_CSIZE (read-only)
    uint32_t reg_2008_r(); // AUD_CCONFIG (read)
    void reg_2008_w(uint32_t data); // AUD_CCONFIG (write)
    uint32_t reg_200c_r(); // AUD_CCNT (read-only)
    uint32_t reg_2010_r(); // AUD_NSTART (read)
    void reg_2010_w(uint32_t data); // AUD_NSTART (write)
    uint32_t reg_2014_r(); // AUD_NSIZE (read)
    void reg_2014_w(uint32_t data); // AUD_NSIZE (write)
    uint32_t reg_2018_r(); // AUD_NCONFIG (read)
    void reg_2018_w(uint32_t data); // AUD_NCONFIG (write)
    uint32_t reg_201c_r(); // AUD_DMACNTL (read)
    void reg_201c_w(uint32_t data); // AUD_DMACNTL (write)

    /* vidUnit registers */

    uint32_t reg_3000_r(); // VID_CSTART (read-only)
    uint32_t reg_3004_r(); // VID_CSIZE (read-only)
    uint32_t reg_3008_r(); // VID_CCNT (read-only)
    uint32_t reg_300c_r(); // VID_NSTART (read)
    void reg_300c_w(uint32_t data); // VID_NSTART (write)
    uint32_t reg_3010_r(); // VID_NSIZE (read)
    void reg_3010_w(uint32_t data); // VID_NSIZE (write)
    uint32_t reg_3014_r(); // VID_DMACNTL (read)
    void reg_3014_w(uint32_t data); // VID_DMACNTL (write)
    uint32_t reg_3018_r(); // VID_FCNTL (read)
    void reg_3018_w(uint32_t data); // VID_FCNTL (write)
    uint32_t reg_301c_r(); // VID_BLNKCOL (read)
    void reg_301c_w(uint32_t data); // VID_BLNKCOL (write)
    uint32_t reg_3020_r(); // VID_HSTART (read)
    void reg_3020_w(uint32_t data); // VID_HSTART (write)
    uint32_t reg_3024_r(); // VID_HSIZE (read)
    void reg_3024_w(uint32_t data); // VID_HSIZE (write)
    uint32_t reg_3028_r(); // VID_VSTART (read)
    void reg_3028_w(uint32_t data); // VID_VSTART (write)
    uint32_t reg_302c_r(); // VID_VSIZE (read)
    void reg_302c_w(uint32_t data); // VID_VSIZE (write)
    uint32_t reg_3030_r(); // VID_HINTLINE (read)
    void reg_3030_w(uint32_t data); // VID_HINTLINE (write)
    uint32_t reg_3034_r(); // VID_CLINE (read-only)
    uint32_t reg_3038_r(); // VID_INTSTAT (read)
    void reg_3138_w(uint32_t data); // VID_INTSTAT (clear)
    uint32_t reg_303c_r(); // VID_INTEN_S (read)
    void reg_303c_w(uint32_t data); // VID_INTEN_S (write)
    void reg_313c_w(uint32_t data); // VID_INTEN_C (clear)

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

    uint32_t reg_4040_r(); // DEV_MOD0 (read)
    void reg_4040_w(uint32_t data); // DEV_MOD0 (write)
    uint32_t reg_4044_r(); // DEV_MOD1 (read)
    void reg_4044_w(uint32_t data); // DEV_MOD1 (write)
    uint32_t reg_4048_r(); // DEV_MOD2 (read)
    void reg_4048_w(uint32_t data); // DEV_MOD2 (write)
    uint32_t reg_404c_r(); // DEV_MOD3 (read)
    void reg_404c_w(uint32_t data); // DEV_MOD3 (write)
    uint32_t reg_4050_r(); // DEV_MOD4 (read)
    void reg_4050_w(uint32_t data); // DEV_MOD4 (write)
    uint32_t reg_4054_r(); // DEV_MOD5 (read)
    void reg_4054_w(uint32_t data); // DEV_MOD5 (write)
    uint32_t reg_4058_r(); // DEV_MOD6 (read)
    void reg_4058_w(uint32_t data); // DEV_MOD6 (write)
    uint32_t reg_405c_r(); // DEV_MOD7 (read)
    void reg_405c_w(uint32_t data); // DEV_MOD7 (write)

    /* memUnit registers */

    uint32_t reg_5000_r(); // MEM_CNTL (read)
    void reg_5000_w(uint32_t data); // MEM_CNTL (write)
    uint32_t reg_5004_r(); // MEM_REFCNT (read)
    void reg_5004_w(uint32_t data); // MEM_REFCNT (write)
    uint32_t reg_5008_r(); // MEM_DATA (read)
    void reg_5008_w(uint32_t data); // MEM_DATA (write)
    uint32_t reg_500c_r(); // MEM_CMD (defined as write-only, but software reads from it???)
    void reg_500c_w(uint32_t data); // MEM_CMD (write-only)
    uint32_t reg_5010_r(); // MEM_TIMING (read)
    void reg_5010_w(uint32_t data); // MEM_TIMING (write)
};

DECLARE_DEVICE_TYPE(SPOT_ASIC, spot_asic_device)

#endif