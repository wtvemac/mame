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

constexpr uint32_t BOOTMODE_BIG_ENDIAN = 1 << 8;

constexpr uint32_t WATCHDOG_TIMER_USEC = 1000000;
constexpr uint16_t TCOMPARE_TIMER_USEC = 50000;

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
constexpr uint32_t POT_VIDUNIT_VSTART_OFFSET  = 0x20;
constexpr uint32_t POT_GFXUNIT_HSTART_OFFSET  = 0x37;
constexpr uint32_t POT_GFXUNIT_VSTART_OFFSET  = 0x00;

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

constexpr uint32_t HAN_INT_IDE     = 1 << 2;
constexpr uint32_t HAN_INT_MSG_OUT = 1 << 1;
constexpr uint32_t HAN_INT_MSG_IN  = 1 << 0;

constexpr uint32_t HAN_MSGCNTL_BUFF_RESET   = 1 << 5;
constexpr uint32_t HAN_MSGCNTL_BUFF_RELEASE = 1 << 3;
constexpr uint32_t HAN_MSGCNTL_DATA_READ    = 1 << 2;
constexpr uint32_t HAN_MSGCNTL_DATA_VALID   = 1 << 1;
constexpr uint32_t HAN_MSGCNTL_DATA_WAITING = 1 << 1;
constexpr uint32_t HAN_MSGCNTL_BUFF_OWN     = 1 << 0;

constexpr uint8_t HAN_STARTUP_BEGIN           = 0;
constexpr uint8_t HAN_STARTUP_SEND_RESTART    = 1;
constexpr uint8_t HAN_STARTUP_RESTART_OK      = 2;
constexpr uint8_t HAN_STARTUP_SEND_RESET      = 3;
constexpr uint8_t HAN_STARTUP_DONE            = 4;

enum han_msgtype_t : uint16_t
{
	IPC_CLASS_TUNER   = 0x0000,
	IPC_CLASS_DMA     = 0x0001,
	IPC_CLASS_CA      = 0x0002,
	IPC_CLASS_DIAG    = 0x0003,
	IPC_CLASS_MAILBOX = 0x0004,
	IPC_CLASS_RESTART = 0x00f0,
};

enum han_diag_msgsubtype_t : uint16_t
{
	DIAG_ECHO_MB_MSG          = 0x0000,
	DIAG_GET_RX_ERROR_COUNT   = 0x0001,
	DIAG_RESET_RX_ERROR_COUNT = 0x0002,
	DIAG_GET_TX_ERROR_COUNT   = 0x0003,
	DIAG_RESET_TX_ERROR_COUNT = 0x0004,
};

enum han_mailbox_msgsubtype_t : uint16_t
{
	EPC2SRA_EPG_CHANGED                     = 0x0401,
	EPC2SRA_CHANNEL_CHANGED                 = 0x0402,
	EPC2SRA_NOTIFY_FORCED_STANDBY           = 0x0403,
	EPC2SRA_NOTIFY_ALARM                    = 0x0404,
	EPC2SRA_CLEAR_ALARM                     = 0x0405,
	EPC2SRA_BURNING_FLASH                   = 0x0406,
	EPC2SRA_RESET                           = 0x0407,
	EPC2SRA_FREE_PERIOD_OVER                = 0x0408,
	EPC2SRA_GOT_AGC_NO_LOCK                 = 0x0409,
	EPC2SRA_DOWNLOAD_AVAILABLE              = 0x040a,
	EPC2SRA_NO_PHONE                        = 0x040b,
	EPC2SRA_PPV_AUTHORIZATION               = 0x040c,
	EPC2SRA_SW_UPDATE_AVAILABLE             = 0x040d,
	EPC2SRA_PHONE_STATUS                    = 0x040e,
	EPC2SRA_SMART_CARD_NOT_AUTHORIZED       = 0x040f,
	EPC2SRA_MUSIC_TITLE_CHANGED             = 0x0410,
	EPC2SRA_CLOSED_CAPTION_DATA             = 0x0411,
	EPC2SRA_CHANNEL_CHANGE_STATUS           = 0x0412,
	EPC2SRA_VIDEO_TEST_RESULTS              = 0x0413,
	EPC2SRA_MAIN_UNIT_TEST_RESULTS          = 0x0414,
	EPC2SRA_ECM_NOT_AVAILABLE               = 0x0415,
	EPC2SRA_SWITCH_VERIFIED                 = 0x0416,
	EPC2SRA_PAT_PMT_ERROR                   = 0x0417,
	EPC2SRA_IPPV_JEOPARDY                   = 0x0418,
	EPC2SRA_UPDATE_PHONE_STATUS             = 0x0419,
	EPC2SRA_NVM_TEST_RESULT                 = 0x041a,
	EPC2SRA_SEND_APPLICATION_VERSION_ID     = 0x041b,
	EPC2SRA_NEW_CHANNEL_MAP_AVAILABLE       = 0x041c,
	EPC2SRA_GENERIC_POPUP                   = 0x041d,
	EPC2SRA_GENERIC_SCREEN                  = 0x041e,
	EPC2SRA_UNUSED_8                        = 0x041f,
	EPC2SRA_UNUSED_9                        = 0x0420,
	EPC2SRA_UNUSED_10                       = 0x0421,
	EPC2SRA_UNUSED_11                       = 0x0422,
	EPC2SRA_UNUSED_12                       = 0x0423,
	EPC2SRA_UNUSED_13                       = 0x0424,
	EPC2SRA_UNUSED_14                       = 0x0425,
	EPC2SRA_UNUSED_15                       = 0x0426,
	EPC2SRA_MAIL                            = 0x0427,
	SRA2EPC_SET_VERSION_ID                  = 0x0428,
	SRA2EPC_SEND_CLOSED_CAPTION             = 0x0429,
	SRA2EPC_STOP_CLOSED_CAPTION             = 0x042a,
	SRA2EPC_GET_ALARM_STATUS                = 0x042b,
	SRA2EPC_GET_STATUS                      = 0x042c,
	SRA2EPC_GET_FRONT_END_LOCK_STATUS       = 0x042d,
	SRA2EPC_TUNE_TO_TRANSPONDER             = 0x042e,
	SRA2EPC_GET_CURRENT_TRANSPONDER         = 0x042f,
	SRA2EPC_GET_CURRENT_NETWORK             = 0x0430,
	SRA2EPC_STOP_CHANNEL                    = 0x0431,
	SRA2EPC_CHANGE_CHANNEL                  = 0x0432,
	SRA2EPC_GET_CURRENT_CHANNEL             = 0x0433,
	SRA2EPC_GET_SYSTEM_INFO                 = 0x0434,
	SRA2EPC_GET_NVRAM                       = 0x0435,
	SRA2EPC_GET_MEMORY                      = 0x0436,
	SRA2EPC_GET_DIAGNOSTICS                 = 0x0437,
	SRA2EPC_GET_ZIP_CODE                    = 0x0438,
	SRA2EPC_PUT_ZIP_CODE                    = 0x0439,
	SRA2EPC_GO_TO_STANDBY                   = 0x043a,
	SRA2EPC_RETURN_FROM_STANDBY             = 0x043b,
	SRA2EPC_GET_ALT_AUDIO                   = 0x043c,
	SRA2EPC_CHANGE_TO_ALT_AUDIO             = 0x043d,
	SRA2EPC_SET_PREFERRED_AUDIO             = 0x043e,
	SRA2EPC_GET_PREFERRED_AUDIO             = 0x043f,
	SRA2EPC_TEST_PHONE_LINE                 = 0x0440,
	SRA2EPC_GET_SIGNAL_STRENGTH             = 0x0441,
	SRA2EPC_GET_POINTING_INFO               = 0x0442,
	SRA2EPC_PURCHASE_PPV                    = 0x0443,
	SRA2EPC_START_SIGNAL_STRENGTH_SESSION   = 0x0444,
	SRA2EPC_END_SIGNAL_STRENGTH_SESSION     = 0x0445,
	SRA2EPC_ANALOG_AUDIO_SOURCE_SELECT      = 0x0446,
	SRA2EPC_ANALOG_VIDEO_SOURCE_SELECT      = 0x0447,
	SRA2EPC_DIGITAL_AUDIO_SOURCE_SELECT     = 0x0448,
	SRA2EPC_DIGITAL_VIDEO_SOURCE_SELECT     = 0x0449,
	SRA2EPC_GET_TIME                        = 0x044a,
	SRA2EPC_GET_LOCAL_TIME                  = 0x044b,
	SRA2EPC_RESET_MAILBOX                   = 0x044c,
	SRA2EPC_RESET_EPC                       = 0x044d,
	SRA2EPC_PERFORM_VIDEO_TEST              = 0x044e,
	SRA2EPC_PERFORM_MAIN_UNIT_TEST          = 0x044f,
	SRA2EPC_GET_MUSIC_TITLE                 = 0x0450,
	SRA2EPC_GET_SW_UPDATE                   = 0x0451,
	SRA2EPC_CLEAR_NVM                       = 0x0452,
	SRA2EPC_TOGGLE_UI                       = 0x0453,
	SRA2EPC_CHECK_SWITCH                    = 0x0454,
	SRA2EPC_GET_SWITCH_INFO                 = 0x0455,
	SRA2EPC_GET_SWITCH_MATRIX               = 0x0456,
	SRA2EPC_GET_SWITCH_TEST_STEP            = 0x0457,
	SRA2EPC_GET_EPG_START_TIME              = 0x0458,
	SRA2EPC_CHECK_XPONDER_NUM               = 0x0459,
	SRA2EPC_SET_CURRENT_SATELLITE           = 0x045a,
	SRA2EPC_GET_EPC_STATE                   = 0x045b,
	SRA2EPC_SET_ASPECT_RATIO                = 0x045c,
	SRA2EPC_GET_ASPECT_RATIO                = 0x045d,
	SRA2EPC_RETUNE_CHANNEL                  = 0x045e,
	SRA2EPC_GET_NETWORK_HIDDEN_STATUS       = 0x045f,
	SRA2EPC_UPDATE_PHONE_STATUS             = 0x0460,
	SRA2EPC_TEST_NVM                        = 0x0461,
	SRA2EPC_APPLICATION_VERSION_ID          = 0x0462,
	SRA2EPC_GET_CHANNEL_MAP                 = 0x0463,
	SRA2EPC_RESET_PURCASE_HISTORY           = 0x0464,
	SRA2EPC_UPDATE_EPG                      = 0x0465,
	SRA2EPC_DISH_500_INSTALL                = 0x0466,
	SRA2EPC_UNUSED_13                       = 0x0467,
	SRA2EPC_UNUSED_14                       = 0x0468,
	SRA2EPC_UNUSED_15                       = 0x0469,
	SRA2EPC_GET_LOCAL_OFFSET                = 0x046a,
	PC2EPC_REGISTER_DATA_APPLICATION        = 0x046b,
	PC2EPC_DATA_APP_TUNE_REQUEST            = 0x046c,
	PC2EPC_START_SIGNAL_STRENGTH_SESSION    = 0x046d,
	PC2EPC_END_SIGNAL_STRENGTH_SESSION      = 0x046e,
	PC2EPC_GET_SIGNAL_STRENGTH              = 0x046f,
	PC2EPC_CLEAR_NVM                        = 0x0470,
	PC2EPC_GET_AUTH_DATA_SERVICES           = 0x0471,
	PC2EPC_SET_MEMORY                       = 0x0472,
	PC2EPC_GET_TRANSPONDER_INFO             = 0x0473,
	EPC2PC_DATA_REC_STATUS                  = 0x0474,
	PC2EPC_GET_DATA_TUNE_INFO               = 0x0475,
	PC2EPC_GET_DATA                         = 0x0476,
	EPC2PC_GENERIC_DATA                     = 0x0477,
	PC2EPC_CANCEL_DATA_APP_TUNE_REQUEST     = 0x0478,
	PC2EPC_GET_ACTIVE_DATA_DOWNLOADS        = 0x0479,
	PC2EPC_GET_CURRENT_DATA_TAGS            = 0x047a,
	EPC2PC_DMA_CHANNEL_ERROR                = 0x047b,
	PC2EPC_UNUSED_11                        = 0x047c,
	PC2EPC_UNUSED_12                        = 0x047d,
	PC2EPC_UNUSED_13                        = 0x047e,
	PC2EPC_UNUSED_14                        = 0x047f,
	PC2EPC_UNUSED_15                        = 0x0480,
	PC2EPC_UNUSED_16                        = 0x0481,
	PC2EPC_UNREGISTER_DATA_APPLICATION      = 0x0482,
	EPC2SRC_SEND_PRESENT_FOLLOWING          = 0x0483,
	EPC2SRC_PURCHASE_PPV                    = 0x0484,
	EPC2SRC_GET_SUBSCRIPTION_ACCESS         = 0x0485,
	EPC2SRC_GET_EVENT_ACCESS                = 0x0486,
	EPC2SRC_GET_PURCHASE_HISTORY            = 0x0487,
	EPC2SRC_SEND_TIME_SLICE                 = 0x0488,
	EPC2SRC_STOP_EPG                        = 0x0489,
	EPC2SRC_MODEM_RESPONSE                  = 0x048a,
	EPC2SRC_MODEM_DATA                      = 0x048b,
	EPC2SRC_GET_SERVICE_LIST                = 0x048c,
	EPC2SRC_EPC_SHUTTING_DOWN               = 0x048d,
	EPC2SRC_EPC_OPERATIONAL                 = 0x048e,
	EPC2SRC_TUNE_REQUEST                    = 0x048f,
	EPC2SRC_GET_NVM_VARIABLE                = 0x0490,
	EPC2SRC_SET_NVM_VARIABLE                = 0x0491,
	EPC2SRC_TUNE_TO_EPG                     = 0x0492,
	EPC2SRC_PHONE_STATUS                    = 0x0493,
	EPC2SRC_START_DMA_CHANNEL               = 0x0494,
	EPC2SRC_STOP_DMA_CHANNEL                = 0x0495,
	EPC2SRC_SET_MEMORY                      = 0x0496,
	EPC2SRC_GET_PIDS                        = 0x0497,
	EPC2SRC_GET_SERVICE_ATTRIBS             = 0x0498,
	EPC2SRC_SEND_ASSOC_TAG_DESCRIPTORS      = 0x0499,
	EPC2SRC_AC3_SETUP                       = 0x049a,
	EPC2SRC_GET_DATA_TAGS                   = 0x049b,
	EPC2SRC_SEND_DATA_TAGS                  = 0x049c,
	EPC2SRC_UPDATE_CHANNEL_LIST             = 0x049d,
	EPC2SRC_GET_VIRTUAL_SUBSCRIPTION_RIGHTS = 0x049e,
	EPC2SRC_SEND_EPG                        = 0x049f,
	EPC2SRC_UNUSED_11                       = 0x04a0,
	EPC2SRC_UNUSED_12                       = 0x04a1,
	EPC2SRC_UNUSED_13                       = 0x04a2,
	EPC2SRC_EEIT_PIDS                       = 0x04a3,
	EPC2SRC_GET_SUID_LIST                   = 0x04a4,
	EPC2SRC_UNUSED_16                       = 0x04a5,
	EPC2SRC_GET_EPG_STATUS                  = 0x04a6,
	SRC2EPC_EPG_STATUS                      = 0x04a7,
	SRC2EPC_TUNE_REQUEST_RESULT             = 0x04a8,
	SRC2EPC_NEW_PRESENT_FOLLOWING           = 0x04a9,
	SRC2EPC_PURCHASE_STATUS                 = 0x04aa,
	SRC2EPC_SUBSCRIPTION_ACCESS_RIGHTS      = 0x04ab,
	SRC2EPC_EVENT_ACCESS_RIGHTS             = 0x04ac,
	SRC2EPC_PURCHASE_HISTORY                = 0x04ad,
	SRC2EPC_EPG_TIME_SLICE_NOT_AVAIL        = 0x04ae,
	SRC2EPC_EPG_TIME_SLICE_COMPLETE         = 0x04af,
	SRC2EPC_EPG_SECTION                     = 0x04b0,
	SRC2EPC_MODEM_COMMAND                   = 0x04b1,
	SRC2EPC_MODEM_DATA                      = 0x04b2,
	SRC2EPC_SERVICE_LIST                    = 0x04b3,
	SRC2EPC_MODEM_CNTL                      = 0x04b4,
	SRC2EPC_SIGNAL_STRENGTH                 = 0x04b5,
	SRC2EPC_MAIL                            = 0x04b6,
	SRC2EPC_NO_DOWNLOAD_AVAILABLE           = 0x04b7,
	SRC2EPC_EPG_NOT_AVAILABLE               = 0x04b8,
	SRC2EPC_SERVICE_LIST_NOT_AVAILABLE      = 0x04b9,
	SRC2EPC_NEW_SERVICE_LIST_AVAIL          = 0x04ba,
	SRC2EPC_IN_STANDBY                      = 0x04bb,
	SRC2EPC_RETURN_FROM_STANDBY             = 0x04bc,
	SRC2EPC_PF_SECTION                      = 0x04bd,
	SRC2EPC_TEST_PHONE                      = 0x04be,
	SRC2EPC_DATA_FILE_SECTION               = 0x04bf,
	SRC2EPC_VIDEO_TEST_RESULTS              = 0x04c0,
	SRC2EPC_MAIN_UNIT_TEST_RESULTS          = 0x04c1,
	SRC2EPC_SWITCH_VERIFIED                 = 0x04c2,
	SRC2EPC_NVM_WRITE_FAILURE               = 0x04c3,
	SRC2EPC_GET_PIDS                        = 0x04c4,
	SRC2EPC_RESET_APPLICATION               = 0x04c5,
	SRC2EPC_ASSOC_TAG_DESCRIPTORS_AVAILABLE = 0x04c6,
	SRC2EPC_ASSOC_TAG_DESCRIPTORS_REQUEST   = 0x04c7,
	SRC2EPC_GET_AC3_SETUP                   = 0x04c8,
	SRC2EPC_DATA_TAGS                       = 0x04c9,
	SRC2EPC_IRB_WATCHDOG_FIRED              = 0x04ca,
	SRC2EPC_EPG_COMPLETE                    = 0x04cb,
	SRC2EPC_UNUSED_9                        = 0x04cc,
	SRC2EPC_UNUSED_10                       = 0x04cd,
	SRC2EPC_UNUSED_11                       = 0x04ce,
	SRC2EPC_UNUSED_12                       = 0x04cf,
	SRC2EPC_UNUSED_13                       = 0x04d0,
	SRC2EPC_UNUSED_14                       = 0x04d1,
	SRC2EPC_UNUSED_15                       = 0x04d2,
	SRC2EPC_UNUSED_16                       = 0x04d3,
	SRC2EPC_UNUSED_17                       = 0x04d4,
	WEB2EPC_PURCHASE_PAY_PER_VIEW           = 0x04d5,
	WEB2EPC_GET_EVENT_ACCESS_RIGHTS         = 0x04d6,
	EPC2WEB_EVENT_ACCESS_RIGHTS             = 0x04d7,
	WEB2EPC_GET_SUBSCRIPTION_ACCESS_RIGHTS  = 0x04d8,
	EPC2WEB_SUBSCRIPTION_ACCESS_RIGHTS      = 0x04d9,
	EPC2WEB_FRONT_PANEL_KEY                 = 0x04da,
	WEB2EPC_GET_AUDIO_SAMPLE_RATE           = 0x04db,
	WEB2EPC_IRB_SOURCE_SELECT               = 0x04dc,
	WEB2EPC_HARD_RESET                      = 0x04dd,
	WEB2SRC_DELAY_MODE_SETUP                = 0x04de,
	WEB2SRC_DELAY_MODE_PLAYBACK             = 0x04df,
	WEB2SRC_AC3_INIT_DATA                   = 0x04e0,
	SRC2WEB_RESET_AC3_PTRS                  = 0x04e1,
	WEB2SRC_AC3_PTRS_RESET                  = 0x04e2,
	WEB2SRC_GET_CAPABILITIES                = 0x04e3,
	SRC2WEB_GET_VERSIONS                    = 0x04e4,
	SRC2WEB_REQUEST_RESET                   = 0x04e5,
	WEB2SRC_FREEZE_FRAME                    = 0x04e6,
	WEB2SRC_CLEANUP_PREDATOR_EFFECT         = 0x04e7,
	SRC2WEB_CHECK_FOR_PREDATOR_EFFECT       = 0x04e8,
	WEB2SRC_TUNE_TO_WEB_CHAN                = 0x04e9,
	WEB2SRC_NEED_UPDATE                     = 0x04ea,
	WEB2SRC_RESET_DECODER                   = 0x04eb,
	EPC2WEB_ERROR_DIAG                      = 0x04ec,
	SRC2WEB_FROZEN_PTS                      = 0x04ed,
};

constexpr uint32_t HAN_MSGIRQ_HACK_HZ = 60;
constexpr uint32_t HAN_REBOOT_WAIT_MS = 2000;
constexpr uint16_t HAN_MSGBUFF_SIZE = 0x100;
constexpr uint32_t HAN_MSGSIZE_INDEX = 0x0000;
constexpr uint32_t HAN_MSGTYPE_INDEX = 0x0001;
constexpr uint32_t HAN_MSGSUBTYPE_INDEX = 0x0002;
constexpr uint32_t HAN_MSG_START_INDEX = 0x0003;

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
	void han_map(address_map &map);
	void pekoe_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_serial_id(T &&tag) { m_serial_id.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_nvram(T &&tag) { m_nvram.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_ata(T &&tag) { m_ata.set_tag(std::forward<T>(tag)); }
	void set_chipid(uint32_t chpid) { m_chpid = chpid; }

	void irq_ide1_w(int state);
	void irq_ide2_w(int state);

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
	uint32_t m_bootmode;
	uint32_t m_use_bootmode;

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
	uint32_t m_div_dmacntl;
	uint32_t m_div_nextcfg;
	uint32_t m_div_currcfg;

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

	bool m_han_enabled = false;
	uint32_t m_han_intenable;
	uint32_t m_han_intstat;
	uint32_t m_han_msgbuff_status;
	bool m_han_need_in_int;
	uint16_t m_han_msgbuff[HAN_MSGBUFF_SIZE >> 1];
	uint32_t m_han_msgbuff_index;
	uint8_t m_han_startup_step;

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

	emu_timer *han_message_timer = nullptr;
	TIMER_CALLBACK_MEMBER(check_han_message_state);

	emu_timer *han_reboot_timer = nullptr;
	TIMER_CALLBACK_MEMBER(han_reboot);

	bool m_aud_dma_ongoing;

	int m_serial_id_tx;

	bool send_han_message(uint16_t msg_type, uint16_t msg_subtype, uint8_t* msg, uint16_t msg_size);
	bool have_queued_han_message();
	bool can_send_han_message();
	uint32_t arrange_han_data(uint32_t data);

	void vblank_irq(int state);
	void irq_modem_w(int state);
	void irq_keyboard_w(int state);
	void set_audio_irq(uint8_t mask, int state);
	void set_dev_irq(uint8_t mask, int state);
	void set_rio_irq(uint8_t mask, int state);
	void set_video_irq(uint8_t mask, uint8_t sub_mask, int state);
	void set_timer_irq(uint8_t mask, int state);
	void set_han_irq(uint32_t mask, int state);
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
	void reg_0008_w(uint32_t data); // BUS_INTSTAT_S (write)
	uint32_t reg_0108_r();          // BUS_INTSTAT (read)
	void reg_0108_w(uint32_t data); // BUS_INTSTAT_C (write)
	uint32_t reg_0050_r();          // BUS_INTSTATRAW (read)
	uint32_t reg_000c_r();          // BUS_INTEN (read)
	void reg_000c_w(uint32_t data); // BUS_INTEN_S (write)
	uint32_t reg_010c_r();          // BUS_INTEN (read)
	void reg_010c_w(uint32_t data); // BUS_INTEN_C (write)
	uint32_t reg_0010_r();          // BUS_ERRSTAT (read)
	void reg_0010_w(uint32_t data); // BUS_ERRSTAT_S (write)
	uint32_t reg_0110_r();          // BUS_ERRSTAT (read)
	void reg_0110_w(uint32_t data); // BUS_ERRSTAT_C (write)
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
	uint32_t reg_00c8_r();          // BUS_BOOTMODE (read)
	void reg_00c8_w(uint32_t data); // BUS_BOOTMODE (write)
	uint32_t reg_00cc_r();          // BUS_USEBOOTMODE (read)
	void reg_00cc_w(uint32_t data); // BUS_USEBOOTMODE (write)


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

	uint32_t reg_8004_r();
	void reg_8004_w(uint32_t data);
	uint32_t reg_801c_r();
	void reg_801c_w(uint32_t data);
	uint32_t reg_8038_r();
	void reg_8038_w(uint32_t data);

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

	/* Han registers */

	uint32_t reg_han_0000_r();          // HAN_INTSTAT_C (read)
	void reg_han_0000_w(uint32_t data); // HAN_INTSTAT_C (write)
	uint32_t reg_han_0004_r();          // HAN_INTEN_S (read)
	void reg_han_0004_w(uint32_t data); // HAN_INTEN_S (write)

	uint32_t reg_han_0010_r();          // HAN_ (read)
	void reg_han_0010_w(uint32_t data); // HAN_ (write)
	uint32_t reg_han_0080_r();          // HAN_ (read)
	void reg_han_0080_w(uint32_t data); // HAN_ (write)
	uint32_t reg_han_0084_r();          // HAN_FIFO? (read)
	void reg_han_0084_w(uint32_t data); // HAN_FIFO? (write)
};

DECLARE_DEVICE_TYPE(SOLO_ASIC, solo_asic_device)

#endif