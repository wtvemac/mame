// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#ifndef MAME_WEBTV_SPOT_ASIC_H
#define MAME_WEBTV_SPOT_ASIC_H

#pragma once

#include "diserial.h"
#include "bus/rs232/rs232.h"
#include "cpu/mips/mips3.h"
#include "wtvdbg.h"
#include "machine/8042kbdc.h"
#include "machine/at_keybc.h"
#include "machine/ds2401.h"
#include "machine/i2cmem.h"
#include "machine/ins8250.h"
#include "sound/dac.h"
#include "speaker.h"
#include "machine/watchdog.h"

constexpr uint32_t SYSCONFIG_ROMTYP0    = 1 << 31; // ROM bank 0 is present
constexpr uint32_t SYSCONFIG_ROMMODE0   = 1 << 30; // ROM bank 0 supports page mode
constexpr uint32_t SYSCONFIG_ROMTYP1    = 1 << 27; // ROM bank 1 is present
constexpr uint32_t SYSCONFIG_ROMMODE1   = 1 << 26; // ROM bank 1 supports page mode
constexpr uint32_t SYSCONFIG_AUDDACMODE = 1 << 17; // use external DAC clock
constexpr uint32_t SYSCONFIG_VIDCLKSRC  = 1 << 16; // use external video encoder clock
constexpr uint32_t SYSCONFIG_CPUBUFF    = 1 << 13; // 0=50% output buffer strength, 1=83% output buffer strength
constexpr uint32_t SYSCONFIG_NTSC       = 1 << 11; // use NTSC mode

constexpr uint32_t CHPCNTL_WDENAB_MASK     =  3 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ0     =  0 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ1     =  1 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ2     =  2 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ3     =  3 << 30;
constexpr uint32_t CHPCNTL_AUDCLKDIV_SHIFT = 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_MASK  = 0x0f << CHPCNTL_AUDCLKDIV_SHIFT;

constexpr uint32_t ERR_F1READ  = 1 << 6; // BUS_FENADDR1 read fence check error
constexpr uint32_t ERR_F1WRITE = 1 << 5; // BUS_FENADDR1 write fence check error
constexpr uint32_t ERR_F2READ  = 1 << 4; // BUS_FENADDR2 read fence check error
constexpr uint32_t ERR_F2WRITE = 1 << 3; // BUS_FENADDR2 write fence check error
constexpr uint32_t ERR_TIMEOUT = 1 << 2; // io timeout error
constexpr uint32_t ERR_OW      = 1 << 0; // double-fault

constexpr uint32_t WATCHDOG_TIMER_USEC = 1000000;

constexpr uint32_t BUS_INT_VIDINT = 1 << 7; // vidUnit interrupt (program should read VID_INTSTAT)
constexpr uint32_t BUS_INT_DEVKBD = 1 << 6; // keyboard IRQ
constexpr uint32_t BUS_INT_DEVMOD = 1 << 5; // modem IRQ
constexpr uint32_t BUS_INT_DEVIR  = 1 << 4; // IR data ready to read
constexpr uint32_t BUS_INT_DEVSMC = 1 << 3; // SmartCard inserted
constexpr uint32_t BUS_INT_AUDDMA = 1 << 2; // audUnit DMA completion

// These are guessed pixel clocks. They were chosen because they cause expected behaviour in emulation.

constexpr uint32_t NTSC_SCREEN_XTAL    = 18393540; // Pixel clock. 480 lines and 640 "pixes" per line @ 60Hz
constexpr uint32_t NTSC_SCREEN_HTOTAL  = 640;      // Total pixels per line (total screen width)
constexpr uint32_t NTSC_SCREEN_HSTART  = 40;       // How many pixel before the active screen starts
constexpr uint32_t NTSC_SCREEN_HSIZE   = 560;      // How many pixels to draw (active screen width)
constexpr uint32_t NTSC_SCREEN_HBSTART = 640;      // How many pixels before the blanking interval starts
constexpr uint32_t NTSC_SCREEN_VTOTAL  = 480;      // Total lines (total screen height)
constexpr uint32_t NTSC_SCREEN_VSTART  = 30;       // How many lines before the active screen starts
constexpr uint32_t NTSC_SCREEN_VSIZE   = 420;      // How many lines to draw (active screen height)
constexpr uint32_t NTSC_SCREEN_VBSTART = 480;      // How many lines before the blanking interval starts

constexpr uint32_t PAL_SCREEN_XTAL    = 21465500; // Pixel clock. 560 lines and 768 "pixes" per line @ 50Hz
constexpr uint32_t PAL_SCREEN_HTOTAL  = 768;      // Total pixels per line (total screen width)
constexpr uint32_t PAL_SCREEN_HSTART  = 72;       // How many pixel before the active screen starts
constexpr uint32_t PAL_SCREEN_HSIZE   = 624;      // How many pixels to draw (active screen width)
constexpr uint32_t PAL_SCREEN_HBSTART = 768;      // How many pixels before the blanking interval starts
constexpr uint32_t PAL_SCREEN_VTOTAL  = 560;      // Total lines (total screen height)
constexpr uint32_t PAL_SCREEN_VSTART  = 40;       // How many lines before the active screen starts
constexpr uint32_t PAL_SCREEN_VSIZE   = 480;      // How many lines to draw (active screen height)
constexpr uint32_t PAL_SCREEN_VBSTART = 560;      // How many lines before the blanking interval starts

constexpr uint32_t VID_DEFAULT_XTAL    = NTSC_SCREEN_XTAL;
constexpr uint32_t VID_DEFAULT_HTOTAL  = NTSC_SCREEN_HTOTAL;
constexpr uint32_t VID_DEFAULT_HSTART  = NTSC_SCREEN_HSTART;
constexpr uint32_t VID_DEFAULT_HBSTART = NTSC_SCREEN_HBSTART;
constexpr uint32_t VID_DEFAULT_HSIZE   = NTSC_SCREEN_HSIZE;
constexpr uint32_t VID_DEFAULT_VTOTAL  = NTSC_SCREEN_VTOTAL;
constexpr uint32_t VID_DEFAULT_VSTART  = NTSC_SCREEN_VSTART;
constexpr uint32_t VID_DEFAULT_VBSTART = NTSC_SCREEN_VBSTART;
constexpr uint32_t VID_DEFAULT_VSIZE   = NTSC_SCREEN_VSIZE;
// This is always 0x77 on SPOT for some reason (even on hardware)
// This is needed to correct the HSTART value.
constexpr uint32_t VID_HSTART_OFFSET  = 0x77;
constexpr uint32_t VID_VSTART_OFFSET  = 0x00;

constexpr uint16_t VID_Y_BLACK         = 0x10;
constexpr uint16_t VID_Y_WHITE         = 0xeb;
constexpr uint16_t VID_Y_RANGE         = (VID_Y_WHITE - VID_Y_BLACK);
constexpr uint16_t VID_UV_OFFSET       = 0x80;
constexpr uint8_t  VID_BYTES_PER_PIXEL = 2;
constexpr uint32_t VID_DEFAULT_COLOR   = (VID_UV_OFFSET << 0x10) | (VID_Y_BLACK << 0x08) | VID_UV_OFFSET;

constexpr uint32_t VID_INT_FIDO   = 1 << 6; // TODO: docs don't have info on FIDO mode! figure this out!
constexpr uint32_t VID_INT_VSYNCE = 1 << 5; // even field VSYNC
constexpr uint32_t VID_INT_VSYNCO = 1 << 4; // odd field VSYNC
constexpr uint32_t VID_INT_HSYNC  = 1 << 3; // HSYNC on line specified by VID_HINTLINE
constexpr uint32_t VID_INT_DMA    = 1 << 2; // vidUnit DMA completion

constexpr uint32_t VID_FCNTL_UVSELSWAP  = 1 << 7; // UV is swapped. 1=YCbYCr, 0=YCrYCb
constexpr uint32_t VID_FCNTL_CRCBINVERT = 1 << 6; // invert MSB Cb and Cb
constexpr uint32_t VID_FCNTL_FIDO       = 1 << 5; // enable FIDO mode. details unknown
constexpr uint32_t VID_FCNTL_GAMMA      = 1 << 4; // enable gamma correction
constexpr uint32_t VID_FCNTL_BLNKCOLEN  = 1 << 3; // enable VID_BLANK color
constexpr uint32_t VID_FCNTL_INTERLACE  = 1 << 2; // interlaced video enabled
constexpr uint32_t VID_FCNTL_PAL        = 1 << 1; // PAL mode enabled
constexpr uint32_t VID_FCNTL_VIDENAB    = 1 << 0; // video output enable

constexpr uint32_t VID_DMACNTL_ITRLEN = 1 << 3; // interlaced video in DMA channel
constexpr uint32_t VID_DMACNTL_DMAEN  = 1 << 2; // DMA channel enabled
constexpr uint32_t VID_DMACNTL_NV     = 1 << 1; // DMA next registers are valid
constexpr uint32_t VID_DMACNTL_NVF    = 1 << 0; // DMA next registers are always valid

constexpr uint32_t AUD_CONFIG_16BIT_STEREO = 0;
constexpr uint32_t AUD_CONFIG_16BIT_MONO   = 1;
constexpr uint32_t AUD_CONFIG_8BIT_STEREO  = 2;
constexpr uint32_t AUD_CONFIG_8BIT_MONO    = 3;

constexpr uint32_t AUD_DEFAULT_CLK = 44100;
constexpr float    AUD_OUTPUT_GAIN = 1.0;

constexpr uint32_t AUD_DMACNTL_DMAEN  = 1 << 2; // audUnit DMA channel enabled
constexpr uint32_t AUD_DMACNTL_NV     = 1 << 1; // audUnit DMA next registers are valid
constexpr uint32_t AUD_DMACNTL_NVF    = 1 << 0; // audUnit DMA next registers are always valid

constexpr uint32_t NVCNTL_SCL      = 1 << 3;
constexpr uint32_t NVCNTL_WRITE_EN = 1 << 2;
constexpr uint32_t NVCNTL_SDA_W    = 1 << 1;
constexpr uint32_t NVCNTL_SDA_R    = 1 << 0;

constexpr uint8_t  INS8250_LSR_TSRE = 0x40;
constexpr uint8_t  INS8250_LSR_THRE = 0x20;
constexpr uint16_t MBUFF_MAX_SIZE   = 0x800;
constexpr uint16_t MBUFF_FLUSH_TIME = 100;  // time is in microseconds

constexpr uint8_t SSID_STATE_IDLE               = 0x0;
constexpr uint8_t SSID_STATE_RESET              = 0x1;
constexpr uint8_t SSID_STATE_PRESENCE           = 0x2;
constexpr uint8_t SSID_STATE_COMMAND            = 0x3;
constexpr uint8_t SSID_STATE_READROM            = 0x4;
constexpr uint8_t SSID_STATE_READROM_PULSESTART = 0x5;
constexpr uint8_t SSID_STATE_READROM_PULSEEND   = 0x6;
constexpr uint8_t SSID_STATE_READROM_BIT        = 0x7;

class spot_asic_device : public device_t, public device_serial_interface, public device_video_interface
{

public:

	spot_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, uint32_t chip_id = 0, uint32_t sys_config = 0);

	void map(address_map &map);
	void bus_unit_map(address_map &map);
	void rom_unit_map(address_map &map);
	void aud_unit_map(address_map &map);
	void vid_unit_map(address_map &map);
	void dev_unit_map(address_map &map);
	void mem_unit_map(address_map &map);

	template <typename T> void set_serial_id(T &&tag) { m_serial_id.set_tag(std::forward<T>(tag)); }

	auto sda_r_callback() { return m_iic_sda_in_cb.bind(); }
	auto sda_w_callback() { return m_iic_sda_out_cb.bind(); }

	uint8_t sda_r();
	void sda_w(uint8_t state);
	uint8_t scl_r();
	void scl_w(uint8_t state);

protected:

	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

	uint32_t m_chip_id = 0x01010000;
	uint32_t m_sys_config = 0x00000000;

	uint32_t m_chpcntl;
	uint8_t m_wdenable;

	uint32_t m_fence1_addr;
	uint32_t m_fence1_mask;
	uint32_t m_fence2_addr;
	uint32_t m_fence2_mask;

	uint8_t m_intenable;
	uint8_t m_intstat;

	uint8_t m_errenable;
	uint8_t m_errstat;

	uint16_t m_timeout_count;
	uint16_t m_timeout_compare;

	uint32_t m_memcntl;
	uint32_t m_memrefcnt;
	uint32_t m_memdata;
	uint32_t m_memcmd;
	uint32_t m_memtiming;

	uint8_t m_iiccntl;
	uint8_t m_iic_sda;
	uint8_t m_iic_scl;

	uint32_t m_ledstate;

	uint8_t m_fcntl;

	uint32_t m_vid_nstart;
	uint32_t m_vid_nsize;
	uint32_t m_vid_dmacntl;
	uint32_t m_vid_hstart;
	uint32_t m_vid_hsize;
	uint32_t m_vid_vstart;
	uint32_t m_vid_vsize;
	uint8_t m_vid_fcntl;
	uint32_t m_vid_blank_color;
	uint32_t m_vid_cstart;
	uint32_t m_vid_csize;
	uint32_t m_vid_ccnt;
	uint32_t m_vid_cline;
	uint32_t m_vid_hintline;
	uint32_t m_vid_intenable;
	uint32_t m_vid_intstat;

	// Values set from software are corrected then stored here to draw the actual screen.
	uint32_t m_vid_draw_nstart;
	uint32_t m_vid_draw_hstart;
	uint32_t m_vid_draw_hsize;
	uint32_t m_vid_draw_vstart;
	uint32_t m_vid_draw_vsize;
	uint32_t m_vid_draw_blank_color;

	uint8_t m_aud_clkdiv;
	uint32_t m_aud_ocstart;
	uint32_t m_aud_ocsize;
	uint32_t m_aud_ocend;
	uint32_t m_aud_occonfig;
	uint32_t m_aud_occnt;
	bool m_aud_ocvalid;
	uint32_t m_aud_onstart;
	uint32_t m_aud_onsize;
	uint32_t m_aud_onconfig;
	uint32_t m_aud_odmacntl;

	uint32_t m_rom_cntl0;
	uint32_t m_rom_cntl1;

	uint32_t dev_idcntl;
	uint8_t dev_id_state;
	uint8_t dev_id_bit;
	uint8_t dev_id_bitidx;

	uint8_t modem_txbuff[MBUFF_MAX_SIZE];
	uint32_t modem_txbuff_size;
	uint32_t modem_txbuff_index;

private:

	required_device<mips3_device> m_hostcpu;
	required_shared_ptr<uint32_t> m_hostram;
	required_device<ds2401_device> m_serial_id;
	required_device<kbdc8042_device> m_kbdc;
	required_device<at_keyboard_device> m_kbd;
	required_device<screen_device> m_screen;

	required_device_array<dac_word_interface, 2> m_dac;
	required_device<speaker_device> m_lspeaker;
	required_device<speaker_device> m_rspeaker;

	required_device<ns16550_device> m_modem_uart;

	required_device<wtvdbg_rs232_device> m_debug_uart;
	required_device<watchdog_timer_device> m_watchdog;

	output_finder<> m_power_led;
	output_finder<> m_connect_led;
	output_finder<> m_message_led;

	devcb_read8 m_iic_sda_in_cb;
	devcb_write8 m_iic_sda_out_cb;

	emu_timer *play_aout_timer = nullptr;
	TIMER_CALLBACK_MEMBER(play_aout_samples);

	emu_timer *modem_buffer_timer = nullptr;
	TIMER_CALLBACK_MEMBER(flush_modem_buffer);

	void irq_uart_w(int state);
	void vblank_irq(int state);
	void irq_keyboard_w(int state);
	void irq_smartcard_w(int state);
	void irq_audio_w(int state);
	void irq_modem_w(int state);

	void set_bus_irq(uint8_t mask, int state);
	void set_vid_irq(uint8_t mask, int state);

	void validate_active_area();
	void spot_update_cycle_counting();
	void watchdog_enable(int state);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	/* busUnit registers */

	uint32_t reg_0000_r();          // BUS_CHIPID (read-only)
	uint32_t reg_0004_r();          // BUS_CHPCNTL (read)
	void reg_0004_w(uint32_t data); // BUS_CHPCNTL (write)
	uint32_t reg_0008_r();          // BUS_INTSTAT (read)
	void reg_0108_w(uint32_t data); // BUS_INTSTAT (clear)
	uint32_t reg_000c_r();          // BUS_INTEN (read)
	void reg_000c_w(uint32_t data); // BUS_INTEN (set)
	uint32_t reg_010c_r();          // BUS_INTEN (read)
	void reg_010c_w(uint32_t data); // BUS_INTEN (clear)
	uint32_t reg_0010_r();          // BUS_ERRSTAT (read)
	uint32_t reg_0110_r();          // BUS_ERRSTAT (read)
	void reg_0110_w(uint32_t data); // BUS_ERRSTAT (clear)
	uint32_t reg_0014_r();          // BUS_ERREN_S (read)
	void reg_0014_w(uint32_t data); // BUS_ERREN_S (write)
	uint32_t reg_0114_r();          // BUS_ERREN_C (read)
	void reg_0114_w(uint32_t data); // BUS_ERREN_C (clear)
	uint32_t reg_0018_r();          // BUS_ERRADDR (read-only)
	void reg_0118_w(uint32_t data); // BUS_WDREG_C (clear)
	uint32_t reg_001c_r();          // BUS_FENADDR1 (read)
	void reg_001c_w(uint32_t data); // BUS_FENADDR1 (write)
	uint32_t reg_0020_r();          // BUS_FENMASK1 (read)
	void reg_0020_w(uint32_t data); // BUS_FENMASK1 (write)
	uint32_t reg_0024_r();          // BUS_FENADDR1 (read)
	void reg_0024_w(uint32_t data); // BUS_FENADDR1 (write)
	uint32_t reg_0028_r();          // BUS_FENMASK2 (read)
	void reg_0028_w(uint32_t data); // BUS_FENMASK2 (write)

	/* romUnit registers */

	uint32_t reg_1000_r();          // ROM_SYSCONF (read-only)
	uint32_t reg_1004_r();          // ROM_CNTL0 (read)
	void reg_1004_w(uint32_t data); // ROM_CNTL0 (write)
	uint32_t reg_1008_r();          // ROM_CNTL1 (read)
	void reg_1008_w(uint32_t data); // ROM_CNTL1 (write)

	/* audUnit registers */

	uint32_t reg_2000_r();          // AUD_CSTART (read-only)
	uint32_t reg_2004_r();          // AUD_CSIZE (read-only)
	uint32_t reg_2008_r();          // AUD_CCONFIG (read)
	void reg_2008_w(uint32_t data); // AUD_CCONFIG (write)
	uint32_t reg_200c_r();          // AUD_CCNT (read-only)
	uint32_t reg_2010_r();          // AUD_NSTART (read)
	void reg_2010_w(uint32_t data); // AUD_NSTART (write)
	uint32_t reg_2014_r();          // AUD_NSIZE (read)
	void reg_2014_w(uint32_t data); // AUD_NSIZE (write)
	uint32_t reg_2018_r();          // AUD_NCONFIG (read)
	void reg_2018_w(uint32_t data); // AUD_NCONFIG (write)
	uint32_t reg_201c_r();          // AUD_DMACNTL (read)
	void reg_201c_w(uint32_t data); // AUD_DMACNTL (write)

	/* vidUnit registers */

	uint32_t reg_3000_r();          // VID_CSTART (read-only)
	uint32_t reg_3004_r();          // VID_CSIZE (read-only)
	uint32_t reg_3008_r();          // VID_CCNT (read-only)
	uint32_t reg_300c_r();          // VID_NSTART (read)
	void reg_300c_w(uint32_t data); // VID_NSTART (write)
	uint32_t reg_3010_r();          // VID_NSIZE (read)
	void reg_3010_w(uint32_t data); // VID_NSIZE (write)
	uint32_t reg_3014_r();          // VID_DMACNTL (read)
	void reg_3014_w(uint32_t data); // VID_DMACNTL (write)
	uint32_t reg_3018_r();          // VID_FCNTL (read)
	void reg_3018_w(uint32_t data); // VID_FCNTL (write)
	uint32_t reg_301c_r();          // VID_BLNKCOL (read)
	void reg_301c_w(uint32_t data); // VID_BLNKCOL (write)
	uint32_t reg_3020_r();          // VID_HSTART (read)
	void reg_3020_w(uint32_t data); // VID_HSTART (write)
	uint32_t reg_3024_r();          // VID_HSIZE (read)
	void reg_3024_w(uint32_t data); // VID_HSIZE (write)
	uint32_t reg_3028_r();          // VID_VSTART (read)
	void reg_3028_w(uint32_t data); // VID_VSTART (write)
	uint32_t reg_302c_r();          // VID_VSIZE (read)
	void reg_302c_w(uint32_t data); // VID_VSIZE (write)
	uint32_t reg_3030_r();          // VID_HINTLINE (read)
	void reg_3030_w(uint32_t data); // VID_HINTLINE (write)
	uint32_t reg_3034_r();          // VID_CLINE (read-only)
	uint32_t reg_3038_r();          // VID_INTSTAT (read)
	void reg_3138_w(uint32_t data); // VID_INTSTAT (clear)
	uint32_t reg_303c_r();          // VID_INTEN_S (read)
	void reg_303c_w(uint32_t data); // VID_INTEN_S (write)
	void reg_313c_w(uint32_t data); // VID_INTEN_C (clear)

	/* devUnit registers */

	uint32_t reg_4000_r();          // DEV_IRDATA (read-only)
	uint32_t reg_4004_r();          // DEV_LED (read)
	void reg_4004_w(uint32_t data); // DEV_LED (write)
	uint32_t reg_4008_r();          // DEV_IDCNTL (read)
	void reg_4008_w(uint32_t data); // DEV_IDCNTL (write)
	uint32_t reg_400c_r();          // DEV_NVCNTL (read)
	void reg_400c_w(uint32_t data); // DEV_NVCNTL (write)
	uint32_t reg_4010_r();          // DEV_SCCNTL (read)
	void reg_4010_w(uint32_t data); // DEV_SCCNTL (write)
	uint32_t reg_4014_r();          // DEV_EXTTIME (read)
	void reg_4014_w(uint32_t data); // DEV_EXTTIME (write)
	uint32_t reg_4018_r();          // DEV_ (read)
	void reg_4018_w(uint32_t data); // DEV_ (write)

	// The boot ROM seems to write to register 4018, which is not mentioned anywhere in the documentation.

	uint32_t reg_4020_r();          // DEV_KBD0 (read)
	void reg_4020_w(uint32_t data); // DEV_KBD0 (write)
	uint32_t reg_4024_r();          // DEV_KBD1 (read)
	void reg_4024_w(uint32_t data); // DEV_KBD1 (write)
	uint32_t reg_4028_r();          // DEV_KBD2 (read)
	void reg_4028_w(uint32_t data); // DEV_KBD2 (write)
	uint32_t reg_402c_r();          // DEV_KBD3 (read)
	void reg_402c_w(uint32_t data); // DEV_KBD3 (write)
	uint32_t reg_4030_r();          // DEV_KBD4 (read)
	void reg_4030_w(uint32_t data); // DEV_KBD4 (write)
	uint32_t reg_4034_r();          // DEV_KBD5 (read)
	void reg_4034_w(uint32_t data); // DEV_KBD5 (write)
	uint32_t reg_4038_r();          // DEV_KBD6 (read)
	void reg_4038_w(uint32_t data); // DEV_KBD6 (write)
	uint32_t reg_403c_r();          // DEV_KBD7 (read)
	void reg_403c_w(uint32_t data); // DEV_KBD7 (write)

	uint32_t reg_4040_r();          // DEV_MOD0 (read)
	void reg_4040_w(uint32_t data); // DEV_MOD0 (write)
	uint32_t reg_4044_r();          // DEV_MOD1 (read)
	void reg_4044_w(uint32_t data); // DEV_MOD1 (write)
	uint32_t reg_4048_r();          // DEV_MOD2 (read)
	void reg_4048_w(uint32_t data); // DEV_MOD2 (write)
	uint32_t reg_404c_r();          // DEV_MOD3 (read)
	void reg_404c_w(uint32_t data); // DEV_MOD3 (write)
	uint32_t reg_4050_r();          // DEV_MOD4 (read)
	void reg_4050_w(uint32_t data); // DEV_MOD4 (write)
	uint32_t reg_4054_r();          // DEV_MOD5 (read)
	void reg_4054_w(uint32_t data); // DEV_MOD5 (write)
	uint32_t reg_4058_r();          // DEV_MOD6 (read)
	void reg_4058_w(uint32_t data); // DEV_MOD6 (write)
	uint32_t reg_405c_r();          // DEV_MOD7 (read)
	void reg_405c_w(uint32_t data); // DEV_MOD7 (write)

	/* memUnit registers */

	uint32_t reg_5000_r();          // MEM_CNTL (read)
	void reg_5000_w(uint32_t data); // MEM_CNTL (write)
	uint32_t reg_5004_r();          // MEM_REFCNT (read)
	void reg_5004_w(uint32_t data); // MEM_REFCNT (write)
	uint32_t reg_5008_r();          // MEM_DATA (read)
	void reg_5008_w(uint32_t data); // MEM_DATA (write)
	uint32_t reg_500c_r();          // MEM_CMD (read)
	void reg_500c_w(uint32_t data); // MEM_CMD (write-only)
	uint32_t reg_5010_r();          // MEM_TIMING (read)
	void reg_5010_w(uint32_t data); // MEM_TIMING (write)

};

DECLARE_DEVICE_TYPE(SPOT_ASIC, spot_asic_device)

#endif // MAME_WEBTV_SPOT_ASIC_H