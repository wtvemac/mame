// license:BSD-3-Clause
// copyright-holders:FairPlay137,wtvemac

/***********************************************************************************************

    solo_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The technical specifications that this implementation is based on can be found here:
    http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf

************************************************************************************************/

#ifndef MAME_MACHINE_SOLO_ASIC_H
#define MAME_MACHINE_SOLO_ASIC_H

#pragma once

#include "diserial.h"
#include "bus/rs232/rs232.h"
#include "cpu/mips/mips3.h"
#include "wtvir.h"
#include "machine/ds2401.h"
#include "machine/i2cmem.h"
#include "machine/ins8250.h"
#include "bus/ata/ataintf.h"
#include "sound/dac.h"
#include "speaker.h"
#include "machine/watchdog.h"

constexpr uint32_t SYSCONFIG_PAL = 1 << 3;  // use PAL mode

constexpr uint32_t EMUCONFIG_PBUFF0          = 0;      // Render the screen using data exactly at nstart. Only seen in the prealpha bootrom.
constexpr uint32_t EMUCONFIG_PBUFF1          = 1 << 0; // Render the screen using data one buffer length beyond nstart. Seems to be what they settled on.

constexpr uint32_t CHPCNTL_WDENAB_MASK     =  3 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ0     =  0 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ1     =  1 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ2     =  2 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ3     =  3 << 30;
constexpr uint32_t CHPCNTL_AUDCLKDIV_MASK  =  15 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_EXTC  =  0 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV1  =  1 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV2  =  2 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV3  =  3 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV4  =  4 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV5  =  5 << 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_DIV6  =  6 << 26;

constexpr uint32_t ERR_LOWWRITE = 1 << 7; // low memory write fence error
constexpr uint32_t ERR_F1READ   = 1 << 6; // BUS_FENADDR1 read fence check error
constexpr uint32_t ERR_F1WRITE  = 1 << 5; // BUS_FENADDR1 write fence check error
constexpr uint32_t ERR_F2READ   = 1 << 4; // BUS_FENADDR2 read fence check error
constexpr uint32_t ERR_F2WRITE  = 1 << 3; // BUS_FENADDR2 write fence check error
constexpr uint32_t ERR_TIMEOUT  = 1 << 2; // io timeout error
constexpr uint32_t ERR_OW       = 1 << 0; // double-fault

constexpr uint32_t RESETCAUSE_SOFTWARE = 1 << 2;
constexpr uint32_t RESETCAUSE_WATCHDOG = 1 << 1;
constexpr uint32_t RESETCAUSE_SWITCH   = 1 << 0;

constexpr uint32_t WATCHDOG_TIMER_USEC = 1000000;
constexpr uint16_t TCOMPARE_TIMER_USEC = 10000;

constexpr uint32_t BUS_INT_VIDEO = 1 << 7; // putUnit, gfxUnit, vidUnit interrupt
constexpr uint32_t BUS_INT_AUDIO = 1 << 6; // Soft mode, divUnit and audio in/out interrupt
constexpr uint32_t BUS_INT_RIO   = 1 << 5; // modem IRQ
constexpr uint32_t BUS_INT_DEV   = 1 << 4; // IR data ready to read
constexpr uint32_t BUS_INT_TIMER = 1 << 3; // Timer interrupt (TCOUNT == TCOMPARE)
constexpr uint32_t BUS_INT_FENCE = 1 << 2; // Fence error

constexpr uint32_t BUS_INT_AUD_SMODEMIN  = 1 << 6; // Soft modem DMA in
constexpr uint32_t BUS_INT_AUD_SMODEMOUT = 1 << 5; // Soft modem DMA out
constexpr uint32_t BUS_INT_AUD_DIVUNIT   = 1 << 4; // divUnit audio
constexpr uint32_t BUS_INT_AUD_AUDDMAIN  = 1 << 3; // Audio in
constexpr uint32_t BUS_INT_AUD_AUDDMAOUT = 1 << 2; // Audio out

constexpr uint32_t BUS_INT_DEV_GPIO      = 1 << 6;
constexpr uint32_t BUS_INT_DEV_UART      = 1 << 5;
constexpr uint32_t BUS_INT_DEV_SMARTCARD = 1 << 4;
constexpr uint32_t BUS_INT_DEV_PARPORT   = 1 << 3;
constexpr uint32_t BUS_INT_DEV_IRIN      = 1 << 2;
constexpr uint32_t BUS_INT_DEV_IROUT     = 1 << 2;

constexpr uint32_t BUS_INT_VID_DIVUNIT = 1 << 5;
constexpr uint32_t BUS_INT_VID_GFXUNIT = 1 << 4;
constexpr uint32_t BUS_INT_VID_POTUNIT = 1 << 3;
constexpr uint32_t BUS_INT_VID_VIDUNIT = 1 << 2;

constexpr uint32_t BUS_INT_RIO_DEVICE3 = 1 << 5;
constexpr uint32_t BUS_INT_RIO_DEVICE2 = 1 << 4;
constexpr uint32_t BUS_INT_RIO_DEVICE1 = 1 << 3;
constexpr uint32_t BUS_INT_RIO_DEVICE0 = 1 << 2;

constexpr uint32_t BUS_INT_TIM_SYSTIMER = 1 << 3;
constexpr uint32_t BUS_INT_TIM_BUSTOUT  = 1 << 2;

constexpr uint16_t VID_Y_BLACK         = 0x10;
constexpr uint16_t VID_Y_WHITE         = 0xeb;
constexpr uint16_t VID_Y_RANGE         = (VID_Y_WHITE - VID_Y_BLACK);
constexpr uint16_t VID_UV_OFFSET       = 0x80;
constexpr uint8_t  VID_BYTES_PER_PIXEL = 2;

constexpr uint32_t VID_DMACNTL_ITRLEN = 1 << 3; // interlaced video in DMA channel
constexpr uint32_t VID_DMACNTL_DMAEN  = 1 << 2; // DMA channel enabled
constexpr uint32_t VID_DMACNTL_NV     = 1 << 1; // DMA next registers are valid
constexpr uint32_t VID_DMACNTL_NVF    = 1 << 0; // DMA next registers are always valid

constexpr uint32_t VID_INT_DMA = 1 << 2; // vidUnit DMA completion

constexpr uint32_t GFX_FCNTL_EN          = 1 << 7; // gfxUnit processing enable
constexpr uint32_t GFX_FCNTL_DELTATIME   = 1 << 6; // dx calculation correction
constexpr uint32_t GFX_FCNTL_WAITDISABLE = 1 << 5; // "should always be set to 0"
constexpr uint32_t GFX_FCNTL_WRITEBACKEN = 1 << 4; // 1=Use write-back operation. 0=use ping-pong operation
constexpr uint32_t GFX_FCNTL_FTB         = 1 << 3; // "must always be programmed as 1 for proper write-back operation"
constexpr uint32_t GFX_FCNTL_SOFTRESET   = 1 << 0; // Soft reset gfxUnit

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

constexpr uint32_t POT_DEFAULT_XTAL    = NTSC_SCREEN_XTAL;
constexpr uint32_t POT_DEFAULT_HTOTAL  = NTSC_SCREEN_HTOTAL;
constexpr uint32_t POT_DEFAULT_HSTART  = NTSC_SCREEN_HSTART;
constexpr uint32_t POT_DEFAULT_HBSTART = NTSC_SCREEN_HBSTART;
constexpr uint32_t POT_DEFAULT_HSIZE   = NTSC_SCREEN_HSIZE;
constexpr uint32_t POT_DEFAULT_VTOTAL  = NTSC_SCREEN_VTOTAL;
constexpr uint32_t POT_DEFAULT_VSTART  = NTSC_SCREEN_VSTART;
constexpr uint32_t POT_DEFAULT_VBSTART = NTSC_SCREEN_VBSTART;
constexpr uint32_t POT_DEFAULT_VSIZE   = NTSC_SCREEN_VSIZE;
// This is always 0x37 or 0x77 on SOLO for some reason (even on hardware)
// This is needed to correct the HSTART value.
constexpr uint32_t POT_VIDUNIT_HSTART_OFFSET  = 0x77;
constexpr uint32_t POT_GFXUNIT_HSTART_OFFSET  = 0x37;

constexpr uint32_t POT_DEFAULT_COLOR   = (VID_UV_OFFSET << 0x10) | (VID_Y_BLACK << 0x08) | VID_UV_OFFSET;

constexpr uint32_t POT_FCNTL_USEGFX444    = 1 << 11; // Use 4:4:4 data from gfxUnit when source from dveUnit
constexpr uint32_t POT_FCNTL_DVECCS       = 1 << 10; // Select wich edge of CrCbSel used to latch GFX->DVE interp
constexpr uint32_t POT_FCNTL_DVEHALFSHIFT = 1 << 9;  // Shift pipeline to dveUnit 1/2 pixel (debug bit)
constexpr uint32_t POT_FCNTL_HINT2XFLINE  = 1 << 8;  // hint is in 2x field lines (off = 1x frame lines)
constexpr uint32_t POT_FCNTL_SOUTEN       = 1 << 7;  // Enable video sync outpuit pins  (DVE_TEN is set needs to be set)
constexpr uint32_t POT_FCNTL_DOUTEN       = 1 << 6;  // Enable video output pins (DVE_TEN is set needs to be set)
constexpr uint32_t POT_FCNTL_HALFSHIFT    = 1 << 5;  // Shifts the external encoder pixel pipeline 1/2 pixel (debug bit)
constexpr uint32_t POT_FCNTL_CRCBINVERT   = 1 << 4;  // invert MSB Cb and Cb
constexpr uint32_t POT_FCNTL_USEGFX       = 1 << 3;  // Use gfxUnit as the video source, rather than vidUnit
constexpr uint32_t POT_FCNTL_SOFTRESET    = 1 << 2;  // Soft reset potUnit
constexpr uint32_t POT_FCNTL_PROGRESSIVE  = 1 << 1;  // progressive video enabled
constexpr uint32_t POT_FCNTL_EN           = 1 << 0;  // potUnit output enable

constexpr uint32_t GFX_INT_RANGEINT_WBEOFL = 1 << 4; // Writeback has finished field
constexpr uint32_t GFX_INT_RANGEINT_OOT    = 1 << 3; // qfxUnit ran out of time compositing line
constexpr uint32_t GFX_INT_RANGEINT_WBEOF  = 1 << 2; // Writeback has finished frame

constexpr uint32_t POT_INT_VSYNCE = 1 << 5; // even field VSYNC
constexpr uint32_t POT_INT_VSYNCO = 1 << 4; // odd field VSYNC
constexpr uint32_t POT_INT_HSYNC  = 1 << 3; // HSYNC on line specified by VID_HINTLINE
constexpr uint32_t POT_INT_SHIFT  = 1 << 2; // when shiftage occures (no valid pixels from vidUnit when read)

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

constexpr uint8_t MODFW_NULL_RESULT             = 0x00000000;
constexpr uint8_t MODFW_RBR_ACK                 = 0x2e;
constexpr uint8_t MODFW_LSR_READY               = 0x21;
constexpr uint8_t MODFW_MSG_IDX_FLUSH0          = 0x2;
constexpr uint8_t MODFW_MSG_IDX_FLUSH1          = 0x1c;

constexpr uint8_t modfw_message[] = "\x0a\x0a""Download Modem Firmware ..""\x0d\x0a""Modem Firmware Successfully Loaded""\x0d\x0a";

constexpr uint32_t PEKOE_BYTE_AVAILABLE         = 0x00000001;
constexpr uint32_t PEKOE_CAN_SEND_BYTE          = 0x00000020;

class solo_asic_device : public device_t, public device_serial_interface, public device_video_interface
{
public:
	// construction/destruction
	solo_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void bus_unit_map(address_map &map);
	void rom_unit_map(address_map &map);
	void aud_unit_map(address_map &map);
	void vid_unit_map(address_map &map);
	void dev_unit_map(address_map &map);
	void mem_unit_map(address_map &map);
	void gfx_unit_map(address_map &map);
	void dve_unit_map(address_map &map);
	void div_unit_map(address_map &map);
	void pot_unit_map(address_map &map);
	void suc_unit_map(address_map &map);
	void mod_unit_map(address_map &map);

	void hardware_modem_map(address_map &map);
	void ide_map(address_map &map);
	void hanide_map(address_map &map);
	void pekoe_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_serial_id(T &&tag) { m_serial_id.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_nvram(T &&tag) { m_nvram.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_ata(T &&tag) { m_ata.set_tag(std::forward<T>(tag)); }
	void set_chipid(uint32_t chpid) { m_chpid = chpid; }

	void irq_ide_w(int state);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

protected:
	// device-level overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_resolve_objects() override;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

	uint32_t m_chpcntl;
	uint32_t m_chpid = 0x03120000;
	uint8_t m_wdenable;

	uint32_t m_fence1_addr;
	uint32_t m_fence1_mask;
	uint32_t m_fence2_addr;
	uint32_t m_fence2_mask;
	uint32_t m_tcompare;
	uint32_t m_resetcause;

	uint8_t m_bus_intenable;
	uint8_t m_bus_intstat;
	uint8_t m_busgpio_intenable;
	uint8_t m_busgpio_intstat;
	uint8_t m_busaud_intenable;
	uint8_t m_busaud_intstat;
	uint8_t m_busdev_intenable;
	uint8_t m_busdev_intstat;
	uint8_t m_busvid_intenable;
	uint8_t m_busvid_intstat;
	uint8_t m_busrio_intenable;
	uint8_t m_busrio_intstat;
	uint8_t m_bustim_intenable;
	uint8_t m_bustim_intstat;

	uint8_t m_errenable;
	uint8_t m_errstat;

	uint32_t m_memcntl;
	uint32_t m_memrefcnt;
	uint32_t m_memdata;
	uint32_t m_memcmd;
	uint32_t m_memtiming;

	uint8_t m_nvcntl;

	uint32_t m_ledstate;

	uint8_t m_fcntl;

	uint32_t m_vid_nstart;
	uint32_t m_vid_nsize;
	uint32_t m_vid_dmacntl;
	uint32_t m_vid_cstart;
	uint32_t m_vid_csize;
	uint32_t m_vid_ccnt;
	uint32_t m_vid_cline;
	uint32_t m_vid_vdata;
	uint32_t m_vid_intenable;
	uint32_t m_vid_intstat;

	uint32_t m_gfx_cntl;
	uint32_t m_gfx_activelines;
	uint32_t m_gfx_wbdstart;
	uint32_t m_gfx_wbdlsize;
	uint32_t m_gfx_intenable;
	uint32_t m_gfx_intstat;

	uint32_t m_div_intenable;
	uint32_t m_div_intstat;

	uint8_t m_pot_cntl;
	uint32_t m_pot_hintline;
	uint32_t m_pot_vstart;
	uint32_t m_pot_vsize;
	uint32_t m_pot_blank_color;
	uint32_t m_pot_hstart;
	uint32_t m_pot_hsize;
	uint32_t m_pot_intenable;
	uint32_t m_pot_intstat;

	// Values set from software are corrected then stored here to draw the actual screen.
	uint32_t m_vid_draw_nstart;
	uint32_t m_pot_draw_hstart;
	uint32_t m_pot_draw_hsize;
	uint32_t m_pot_draw_vstart;
	uint32_t m_pot_draw_vsize;
	uint32_t m_pot_draw_blank_color;
	uint32_t m_pot_draw_hintline;

	uint8_t m_aud_clkdiv;
	uint32_t m_aud_cstart;
	uint32_t m_aud_csize;
	uint32_t m_aud_cend;
	uint32_t m_aud_cconfig;
	uint32_t m_aud_ccnt;
	uint32_t m_aud_nstart;
	uint32_t m_aud_nsize;
	uint32_t m_aud_nconfig;
	uint32_t m_aud_dmacntl;

	uint32_t m_rom_cntl0;
	uint32_t m_rom_cntl1;

	uint32_t dev_idcntl;
	uint8_t dev_id_state;
	uint8_t dev_id_bit;
	uint8_t dev_id_bitidx;

	uint16_t m_smrtcrd_serial_bitmask = 0x0;
	uint16_t m_smrtcrd_serial_rxdata = 0x0;

	uint8_t modem_txbuff[MBUFF_MAX_SIZE];
	uint32_t modem_txbuff_size;
	uint32_t modem_txbuff_index;
	bool modfw_mode;
	uint32_t modfw_message_index;
	bool modfw_will_flush;
	bool modfw_will_ack;
	bool do7e_hack;
private:
	required_device<mips3_device> m_hostcpu;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;
	required_device<wtvir_sejin_device> m_irkbdc;
	required_device<screen_device> m_screen;

	required_device_array<dac_word_interface, 2> m_dac;
	required_device<speaker_device> m_lspeaker;
	required_device<speaker_device> m_rspeaker;

	required_device<ns16550_device> m_modem_uart;

	required_device<watchdog_timer_device> m_watchdog;

	required_ioport m_sys_config;
	required_ioport m_emu_config;

	output_finder<> m_power_led;
	output_finder<> m_connect_led;
	output_finder<> m_message_led;

	optional_device<ata_interface_device> m_ata;

	emu_timer *dac_update_timer = nullptr;
	TIMER_CALLBACK_MEMBER(dac_update);

	emu_timer *modem_buffer_timer = nullptr;
	TIMER_CALLBACK_MEMBER(flush_modem_buffer);

	emu_timer *compare_timer = nullptr;
	TIMER_CALLBACK_MEMBER(timer_irq);

	bool m_aud_dma_ongoing;

	int m_serial_id_tx;

	void vblank_irq(int state);
	void irq_modem_w(int state);
	void irq_keyboard_w(int state);
	void set_audio_irq(uint8_t mask, int state);
	void set_dev_irq(uint8_t mask, int state);
	void set_rio_irq(uint8_t mask, int state);
	void set_video_irq(uint8_t mask, uint8_t sub_mask, int state);
	void set_timer_irq(uint8_t mask, int state);
	void set_bus_irq(uint8_t mask, int state);

	void validate_active_area();
	void watchdog_enable(int state);
	void pixel_buffer_index_update();

	uint32_t gfxunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	uint32_t vidunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	
	/* busUnit registers */

	uint32_t reg_0000_r();          // BUS_CHIPID (read-only)
	uint32_t reg_0004_r();          // BUS_CHPCNTL (read)
	void reg_0004_w(uint32_t data); // BUS_CHPCNTL (write)
	uint32_t reg_0008_r();          // BUS_INTSTAT (read)
	uint32_t reg_0108_r();          // BUS_INTSTAT (clear read)
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
	uint32_t reg_0048_r();          // BUS_TCOUNT (read)
	void reg_0048_w(uint32_t data); // BUS_TCOUNT (write)
	uint32_t reg_004c_r();          // BUS_TCOMPARE (read)
	void reg_004c_w(uint32_t data); // BUS_TCOMPARE (write)
	uint32_t reg_005c_r();          // BUS_GPINTEN_S (read)
	void reg_005c_w(uint32_t data); // BUS_GPINTEN_S (write)
	uint32_t reg_015c_r();          // BUS_GPINTEN_C (read)
	void reg_015c_w(uint32_t data); // BUS_GPINTEN_C (write)
	uint32_t reg_0058_r();          // BUS_GPINTSTAT (read)
	uint32_t reg_0060_r();          // BUS_GPINTSTAT_S (read)
	void reg_0060_w(uint32_t data); // BUS_GPINTSTAT_S (write)
	uint32_t reg_0158_r();          // BUS_GPINTSTAT_C (read)
	void reg_0158_w(uint32_t data); // BUS_GPINTSTAT_C (write)
	uint32_t reg_0070_r();          // BUS_AUDINTEN_S (read)
	void reg_0070_w(uint32_t data); // BUS_AUDINTEN_S (write)
	uint32_t reg_0170_r();          // BUS_AUDINTEN_C (read)
	void reg_0170_w(uint32_t data); // BUS_AUDINTEN_C (write)
	uint32_t reg_0068_r();          // BUS_AUDINTSTAT (read)
	uint32_t reg_006c_r();          // BUS_AUDINTSTAT_S (read)
	void reg_006c_w(uint32_t data); // BUS_AUDINTSTAT_S (write)
	uint32_t reg_0168_r();          // BUS_AUDINTSTAT_C (read)
	void reg_0168_w(uint32_t data); // BUS_AUDINTSTAT_C (write)
	uint32_t reg_007c_r();          // BUS_DEVINTEN_S (read)
	void reg_007c_w(uint32_t data); // BUS_DEVINTEN_S (write)
	uint32_t reg_017c_r();          // BUS_DEVINTEN_C (read)
	void reg_017c_w(uint32_t data); // BUS_DEVINTEN_C (write)
	uint32_t reg_0074_r();          // BUS_DEVINTSTAT (read)
	uint32_t reg_0078_r();          // BUS_DEVINTSTAT_S (read)
	void reg_0078_w(uint32_t data); // BUS_DEVINTSTAT_S (write)
	uint32_t reg_0174_r();          // BUS_DEVINTSTAT_C (read)
	void reg_0174_w(uint32_t data); // BUS_DEVINTSTAT_C (write)
	uint32_t reg_0088_r();          // BUS_VIDINTEN_S (read)
	void reg_0088_w(uint32_t data); // BUS_VIDINTEN_S (write)
	uint32_t reg_0188_r();          // BUS_VIDINTEN_C (read)
	void reg_0188_w(uint32_t data); // BUS_VIDINTEN_C (write)
	uint32_t reg_0080_r();          // BUS_VIDINTSTAT (read)
	uint32_t reg_0084_r();          // BUS_VIDINTSTAT_S (read)
	void reg_0084_w(uint32_t data); // BUS_VIDINTSTAT_S (write)
	uint32_t reg_0180_r(); // BUS_VIDINTSTAT_C (read)
	void reg_0180_w(uint32_t data); // BUS_VIDINTSTAT_C (write)
	uint32_t reg_0098_r();          // BUS_RIOINTEN_S (read)
	void reg_0098_w(uint32_t data); // BUS_RIOINTEN_S (write)
	uint32_t reg_0198_r();          // BUS_RIOINTEN_C (read)
	void reg_0198_w(uint32_t data); // BUS_RIOINTEN_C (write)
	uint32_t reg_008c_r();          // BUS_RIOINTSTAT (read)
	uint32_t reg_0090_r();          // BUS_RIOINTSTAT_S (read)
	void reg_0090_w(uint32_t data); // BUS_RIOINTSTAT_S (write)
	uint32_t reg_018c_r();          // BUS_RIOINTSTAT_C (read)
	void reg_018c_w(uint32_t data); // BUS_RIOINTSTAT_C (write)
	uint32_t reg_00a4_r();          // BUS_TIMINTEN_S (read)
	void reg_00a4_w(uint32_t data); // BUS_TIMINTEN_S (write)
	uint32_t reg_01a4_r();          // BUS_TIMINTEN_C (read)
	void reg_01a4_w(uint32_t data); // BUS_TIMINTEN_C (write)
	uint32_t reg_009c_r();          // BUS_TIMINTSTAT (read)
	uint32_t reg_00a0_r();          // BUS_TIMINTSTAT_S (read)
	void reg_00a0_w(uint32_t data); // BUS_TIMINTSTAT_S (write)
	uint32_t reg_019c_r();          // BUS_TIMINTSTAT_C (read)
	void reg_019c_w(uint32_t data); // BUS_TIMINTSTAT_C (write)
	uint32_t reg_00a8_r();          // BUS_RESETCAUSE (read)
	void reg_00a8_w(uint32_t data); // BUS_RESETCAUSE (write)
	void reg_00ac_w(uint32_t data); // BUS_RESETCAUSE_C (write)

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
	uint32_t reg_3038_r();          // VID_INTSTAT (read)
	void reg_3138_w(uint32_t data); // VID_INTSTAT (clear)
	uint32_t reg_303c_r();          // VID_INTEN_S (read)
	void reg_303c_w(uint32_t data); // VID_INTEN_S (write)
	void reg_313c_w(uint32_t data); // VID_INTEN_C (clear)
	uint32_t reg_3040_r();          // VID_VDATA (read)
	void reg_3040_w(uint32_t data); // VID_VDATA (write)

	/* devUnit registers */

	uint32_t reg_4000_r();          // DEV_IROLD (read-only)
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
	uint32_t reg_4020_r();          // DEV_IRIN_SAMPLE (read)
	void reg_4020_w(uint32_t data); // DEV_IRIN_SAMPLE (write)
	uint32_t reg_4024_r();          // DEV_IRIN_REJECT_INT (read)
	void reg_4024_w(uint32_t data); // DEV_IRIN_REJECT_INT (write)
	uint32_t reg_4028_r();          // DEV_IRIN_TRANS_DATA (read)
	uint32_t reg_402c_r();          // DEV_IRIN_STATCNTL (read)
	void reg_402c_w(uint32_t data); // DEV_IRIN_STATCNTL (write)

	

	// The boot ROM seems to write to register 4018, which is not mentioned anywhere in the documentation.

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

	/* gfxUnit registers */

	uint32_t reg_6004_r();          // GFX_CONTROL (read)
	void reg_6004_w(uint32_t data); // GFX_CONTROL (write)
	uint32_t reg_6010_r();          // GFX_OOTYCOUNT (read)
	void reg_6010_w(uint32_t data); // GFX_OOTYCOUNT (write)
	uint32_t reg_6014_r();          // GFX_CELSBASE (read)
	void reg_6014_w(uint32_t data); // GFX_CELSBASE (write)
	uint32_t reg_6018_r();          // GFX_YMAPBASE (read)
	void reg_6018_w(uint32_t data); // GFX_YMAPBASE (write)
	uint32_t reg_601c_r();          // GFX_CELSBASEMASTER (read)
	void reg_601c_w(uint32_t data); // GFX_CELSBASEMASTER (write)
	uint32_t reg_6020_r();          // GFX_YMAPBASEMASTER (read)
	void reg_6020_w(uint32_t data); // GFX_YMAPBASEMASTER (write)
	uint32_t reg_6024_r();          // GFX_INITCOLOR (read)
	void reg_6024_w(uint32_t data); // GFX_INITCOLOR (write)
	uint32_t reg_6028_r();          // GFX_YCOUNTERINlT (read)
	void reg_6028_w(uint32_t data); // GFX_YCOUNTERINlT (write)
	uint32_t reg_602c_r();          // GFX_PAUSECYCLES (read)
	void reg_602c_w(uint32_t data); // GFX_PAUSECYCLES (write)
	uint32_t reg_6030_r();          // GFX_OOTCELSBASE (read)
	void reg_6030_w(uint32_t data); // GFX_OOTCELSBASE (write)
	uint32_t reg_6034_r();          // GFX_OOTYMAPBASE (read)
	void reg_6034_w(uint32_t data); // GFX_OOTYMAPBASE (write)
	uint32_t reg_6038_r();          // GFX_OOTCELSOFFSET (read)
	void reg_6038_w(uint32_t data); // GFX_OOTCELSOFFSET (write)
	uint32_t reg_603c_r();          // GFX_OOTYMAPCOUNT (read)
	void reg_603c_w(uint32_t data); // GFX_OOTYMAPCOUNT (write)
	uint32_t reg_6040_r();          // GFX_TERMCYCLECOUNT (read)
	void reg_6040_w(uint32_t data); // GFX_TERMCYCLECOUNT (write)
	uint32_t reg_6044_r();          // GFX_HCOUNTERINIT (read)
	void reg_6044_w(uint32_t data); // GFX_HCOUNTERINIT (write)
	uint32_t reg_6048_r();          // GFX_BLANKLINES (read)
	void reg_6048_w(uint32_t data); // GFX_BLANKLINES (write)
	uint32_t reg_604c_r();          // GFX_ACTIVELINES (read)
	void reg_604c_w(uint32_t data); // GFX_ACTIVELINES (write)
	uint32_t reg_6060_r();          // GFX_INTEN (read)
	void reg_6060_w(uint32_t data); // GFX_INTEN (write)
	void reg_6064_w(uint32_t data); // GFX_INTEN_C (write-only)
	uint32_t reg_6068_r();          // GFX_INTSTAT (read)
	void reg_6068_w(uint32_t data); // GFX_INTSTAT (write)
	void reg_606c_w(uint32_t data); // GFX_INTSTAT_C (write-only)
	uint32_t reg_6080_r();          // GFX_WBDSTART (read)
	void reg_6080_w(uint32_t data); // GFX_WBDSTART (write)
	uint32_t reg_6084_r();          // GFX_WBDLSIZE (read)
	void reg_6084_w(uint32_t data); // GFX_WBDLSIZE (write)
	uint32_t reg_608c_r();          // GFX_WBSTRIDE (read)
	void reg_608c_w(uint32_t data); // GFX_WBSTRIDE (write)
	uint32_t reg_6090_r();          // GFX_WBDCONFIG (read)
	void reg_6090_w(uint32_t data); // GFX_WBDCONFIG (write)
	uint32_t reg_6094_r();          // GFX_WBDSTART (read)
	void reg_6094_w(uint32_t data); // GFX_WBDSTART (write)

	/* dveUnit registers */


	/* divUnit registers */


	/* potUnit registers */

	uint32_t reg_9080_r();          // POT_VSTART (read)
	void reg_9080_w(uint32_t data); // POT_VSTART (write)
	uint32_t reg_9084_r();          // POT_VSIZE (read)
	void reg_9084_w(uint32_t data); // POT_VSIZE (write)
	uint32_t reg_9088_r();          // POT_BLNKCOL (read)
	void reg_9088_w(uint32_t data); // POT_BLNKCOL (write)
	uint32_t reg_908c_r();          // POT_HSTART (read)
	void reg_908c_w(uint32_t data); // POT_HSTART (write)
	uint32_t reg_9090_r();          // POT_HSIZE (read)
	void reg_9090_w(uint32_t data); // POT_HSIZE (write)
	uint32_t reg_9094_r();          // POT_CNTL (read)
	void reg_9094_w(uint32_t data); // POT_CNTL (write)
	uint32_t reg_9098_r();          // POT_HINTLINE (read)
	void reg_9098_w(uint32_t data); // POT_HINTLINE (write)
	uint32_t reg_909c_r();          // POT_INTEN   (read)
	void reg_909c_w(uint32_t data); // POT_INTEN_S (write)
	void reg_90a4_w(uint32_t data); // POT_INTEN_C (write-only)
	uint32_t reg_90a0_r();          // POT_INTSTAT (read)
	void reg_90a8_w(uint32_t data); // POT_INTSTAT_C (write)
	uint32_t reg_90a8_r();          // POT_INTSTAT_C (read)
	uint32_t reg_90ac_r();          // POT_CLINE (read)

	/* sucUnit registers */

	uint32_t reg_a000_r();          // SUCGPU_TFFHR (read)
	void reg_a000_w(uint32_t data); // SUCGPU_TFFHR (write)
	uint32_t reg_a00c_r();          // SUCGPU_TFFCNT (read)
	uint32_t reg_a010_r();          // SUCGPU_TFFMAX (read)
	uint32_t reg_aab8_r();          // SUCSC0_GPIOVAL (read)

	/* modUnit registers */

	/* Hardware modem registers */

	uint32_t reg_modem_0000_r();          // Modem I/O port base   (RBR/DLL read)
	void reg_modem_0000_w(uint32_t data); // Modem I/O port base   (THR/DLL write)
	uint32_t reg_modem_0004_r();          // Modem I/O port base+1 (IER/DLM read)
	void reg_modem_0004_w(uint32_t data); // Modem I/O port base+1 (IER/DLM write)
	uint32_t reg_modem_0008_r();          // Modem I/O port base+2 (IIR/FCR read)
	void reg_modem_0008_w(uint32_t data); // Modem I/O port base+2 (IIR/FCR write)
	uint32_t reg_modem_000c_r();          // Modem I/O port base+3 (LCR read)
	void reg_modem_000c_w(uint32_t data); // Modem I/O port base+3 (LCR write)
	uint32_t reg_modem_0010_r();          // Modem I/O port base+4 (MCR read)
	void reg_modem_0010_w(uint32_t data); // Modem I/O port base+4 (MCR write)
	uint32_t reg_modem_0014_r();          // Modem I/O port base+5 (LSR read)
	void reg_modem_0014_w(uint32_t data); // Modem I/O port base+5 (LSR write)
	uint32_t reg_modem_0018_r();          // Modem I/O port base+6 (MSR read)
	void reg_modem_0018_w(uint32_t data); // Modem I/O port base+6 (MSR write)
	uint32_t reg_modem_001c_r();          // Modem I/O port base+7 (SCR read)
	void reg_modem_001c_w(uint32_t data); // Modem I/O port base+7 (SCR write)

	/* IDE registers */

	uint32_t reg_ide_000000_r();          // IDE I/O port cs0[0] (data read)
	void reg_ide_000000_w(uint32_t data); // IDE I/O port cs0[0] (data write)
	uint32_t reg_ide_000004_r();          // IDE I/O port cs0[1] (error read)
	void reg_ide_000004_w(uint32_t data); // IDE I/O port cs0[1] (feature write)
	uint32_t reg_ide_000008_r();          // IDE I/O port cs0[2] (sector count read)
	void reg_ide_000008_w(uint32_t data); // IDE I/O port cs0[2] (sector count write)
	uint32_t reg_ide_00000c_r();          // IDE I/O port cs0[3] (sector number read)
	void reg_ide_00000c_w(uint32_t data); // IDE I/O port cs0[3] (sector number write)
	uint32_t reg_ide_000010_r();          // IDE I/O port cs0[4] (cylinder low read)
	void reg_ide_000010_w(uint32_t data); // IDE I/O port cs0[4] (cylinder low write)
	uint32_t reg_ide_000014_r();          // IDE I/O port cs0[5] (cylinder high read)
	void reg_ide_000014_w(uint32_t data); // IDE I/O port cs0[5] (cylinder high write)
	uint32_t reg_ide_000018_r();          // IDE I/O port cs0[6] (drive/head read)
	void reg_ide_000018_w(uint32_t data); // IDE I/O port cs0[6] (drive/head write)
	uint32_t reg_ide_00001c_r();          // IDE I/O port cs0[7] (drive/head read)
	void reg_ide_00001c_w(uint32_t data); // IDE I/O port cs0[7] (drive/head write)
	uint32_t reg_ide_400018_r();          // IDE I/O port cs1[6] (altstatus read)
	void reg_ide_400018_w(uint32_t data); // IDE I/O port cs1[6] (device control write)
	uint32_t reg_ide_40001c_r();          // IDE I/O port cs1[7] (device address read)
	void reg_ide_40001c_w(uint32_t data); // IDE I/O port cs1[7] (device address write)

	/* pekoe registers */

	uint32_t reg_pekoe_0000_r();          // Pekoe data (read)
	void reg_pekoe_0000_w(uint32_t data); // Pekoe data (write)
	void reg_pekoe_0004_w(uint32_t data); // Pekoe ??? (write)
	void reg_pekoe_0008_w(uint32_t data); // Pekoe ??? (write)
	void reg_pekoe_000c_w(uint32_t data); // Pekoe configure? (write)
	void reg_pekoe_0010_w(uint32_t data); // Pekoe ??? (write)
	uint32_t reg_pekoe_0014_r();          // Pekoe status? (read)
};

DECLARE_DEVICE_TYPE(SOLO_ASIC, solo_asic_device)

#endif