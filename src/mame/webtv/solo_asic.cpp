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

    The SOLO ASIC is split into multiple "units", of which this implementation currently only
    emulates the busUnit, the memUnit, and the devUnit.

    The rioUnit (0xA4001xxx) provides a shared interface to the ROM, asynchronous devices (including
    the modem, the IDE hard drive, and the IDE CD-ROM), and synchronous devices which plug into the
    WebTV Port connector (which did not see much use other than for the FIDO/FCS printer interface).

    The audUnit (0xA4002xxx) handles audio DMA.

    The vidUnit (0xA4003xxx) handles video DMA.

    The devUnit (0xA4004xxx) handles GPIO, IR input, IR blaster output, front panel LEDs, and the
    parallel port.

    The memUnit (0xA4005xxx) handles memory timing and other memory-related operations. These
    registers are only emulated for completeness; they do not currently have an effect on the
    emulation.

    The gfxUnit (0xA4006xxx) is responsible for accelerated graphics.

    The dveUnit (0xA4007xxx) is responsible for digital video encoding.

    The divUnit (0xA4008xxx) is responsible for video input decoding.

    The potUnit (0xA4009xxx) handles low-level video output.

    The sucUnit (0xA400Axxx) handles serial I/O for both the RS232 port and the SmartCard reader.

    The modUnit (0xA400Bxxx) handles softmodem I/O. It's basically a stripped down audUnit.

****************************************************************************************************/

// EMAC: this probably should be split out into separate files, this one is bloated and will continue to grow as we implement new SOLO features.

#include "emu.h"

#include "machine/input_merger.h"
#include "render.h"
#include "solo_asic.h"
#include "screen.h"
#include "main.h"
#include "machine.h"
#include "config.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_READS       (1U << 2)
#define LOG_WRITES      (1U << 3)
#define LOG_ERRORS      (1U << 4)
#define LOG_I2C_IGNORES (1U << 5)
#define LOG_DEFAULT     (LOG_READS | LOG_WRITES | LOG_ERRORS | LOG_I2C_IGNORES | LOG_UNKNOWN)

#define VERBOSE         (LOG_DEFAULT)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(SOLO_ASIC, solo_asic_device, "solo_asic", "WebTV SOLO ASIC")

solo_asic_device::solo_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO_ASIC, tag, owner, clock),
	device_serial_interface(mconfig, *this),
	device_video_interface(mconfig, *this),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_serial_id(*this, finder_base::DUMMY_TAG),
	m_nvram(*this, finder_base::DUMMY_TAG),
	m_irkbdc(*this, "irkbdc"),
	m_screen(*this, "screen"),
	m_dac(*this, "dac%u", 0),
	m_lspeaker(*this, "lspeaker"),
	m_rspeaker(*this, "rspeaker"),
    m_modem_uart(*this, "modem_uart"),
	m_watchdog(*this, "watchdog"),
    m_sys_config(*owner, "sys_config"),
    m_emu_config(*owner, "emu_config"),
    m_power_led(*this, "power_led"),
    m_connect_led(*this, "connect_led"),
    m_message_led(*this, "message_led"),
    m_ata(*this, finder_base::DUMMY_TAG)
{
}

static DEVICE_INPUT_DEFAULTS_START( wtv_modem )
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END

DECLARE_INPUT_CHANGED_MEMBER(pbuff_index_changed);

void solo_asic_device::bus_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_0000_r));                                      // BUS_CHIPID
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_0004_r), FUNC(solo_asic_device::reg_0004_w)); // BUS_CHIPCNTL
	map(0x008, 0x00b).r(FUNC(solo_asic_device::reg_0008_r));                                      // BUS_INTSTAT
	map(0x108, 0x10b).w(FUNC(solo_asic_device::reg_0108_w));                                      // BUS_INTEN_S
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_000c_r), FUNC(solo_asic_device::reg_000c_w)); // BUS_ERRSTAT
	map(0x10c, 0x10f).rw(FUNC(solo_asic_device::reg_010c_r), FUNC(solo_asic_device::reg_010c_w)); // BUS_INTEN_C
	map(0x110, 0x113).rw(FUNC(solo_asic_device::reg_0110_r), FUNC(solo_asic_device::reg_0110_w)); // BUS_ERRSTAT, BUS_ERRSTAT_C
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_0014_r), FUNC(solo_asic_device::reg_0014_w)); // BUS_ERREN_S
	map(0x114, 0x117).rw(FUNC(solo_asic_device::reg_0114_r), FUNC(solo_asic_device::reg_0114_w)); // BUS_ERREN_C
	map(0x018, 0x01b).r(FUNC(solo_asic_device::reg_0018_r));                                      // BUS_ERRADDR
	map(0x118, 0x11b).w(FUNC(solo_asic_device::reg_0118_w));                                      // BUS_WDREG_C
	map(0x01c, 0x01f).rw(FUNC(solo_asic_device::reg_001c_r), FUNC(solo_asic_device::reg_001c_w)); // BUS_FENADDR1
	map(0x020, 0x023).rw(FUNC(solo_asic_device::reg_0020_r), FUNC(solo_asic_device::reg_0020_w)); // BUS_FENMASK1
	map(0x024, 0x027).rw(FUNC(solo_asic_device::reg_0024_r), FUNC(solo_asic_device::reg_0024_w)); // BUS_FENADDR2
	map(0x028, 0x02b).rw(FUNC(solo_asic_device::reg_0028_r), FUNC(solo_asic_device::reg_0028_w)); // BUS_FENMASK2
	map(0x048, 0x04b).rw(FUNC(solo_asic_device::reg_0048_r), FUNC(solo_asic_device::reg_0048_w)); // BUS_TCOUNT
	map(0x04c, 0x04f).rw(FUNC(solo_asic_device::reg_004c_r), FUNC(solo_asic_device::reg_004c_w)); // BUS_TCOMPARE
	map(0x05c, 0x05f).rw(FUNC(solo_asic_device::reg_005c_r), FUNC(solo_asic_device::reg_005c_w)); // BUS_GPINTEN_S
	map(0x15c, 0x15f).rw(FUNC(solo_asic_device::reg_015c_r), FUNC(solo_asic_device::reg_015c_w)); // BUS_GPINTEN_C
	map(0x058, 0x05b).r(FUNC(solo_asic_device::reg_0058_r));                                      // BUS_GPINTSTAT
	map(0x060, 0x063).rw(FUNC(solo_asic_device::reg_0060_r), FUNC(solo_asic_device::reg_0060_w)); // BUS_GPINTSTAT_S
	map(0x158, 0x15b).w(FUNC(solo_asic_device::reg_0158_w));                                      // BUS_GPINTSTAT_C
	map(0x070, 0x073).rw(FUNC(solo_asic_device::reg_0070_r), FUNC(solo_asic_device::reg_0070_w)); // BUS_AUDINTEN_S
	map(0x170, 0x173).rw(FUNC(solo_asic_device::reg_0170_r), FUNC(solo_asic_device::reg_0170_w)); // BUS_AUDINTEN_C
	map(0x068, 0x06b).r(FUNC(solo_asic_device::reg_0068_r));                                      // BUS_AUDINTSTAT
	map(0x06c, 0x06f).rw(FUNC(solo_asic_device::reg_006c_r), FUNC(solo_asic_device::reg_006c_w)); // BUS_AUDINTSTAT_S
	map(0x168, 0x16b).w(FUNC(solo_asic_device::reg_0168_w));                                      // BUS_AUDINTSTAT_C
	map(0x07c, 0x07f).rw(FUNC(solo_asic_device::reg_007c_r), FUNC(solo_asic_device::reg_007c_w)); // BUS_DEVINTEN_S
	map(0x17c, 0x17f).rw(FUNC(solo_asic_device::reg_017c_r), FUNC(solo_asic_device::reg_017c_w)); // BUS_DEVINTEN_C
	map(0x074, 0x077).r(FUNC(solo_asic_device::reg_0074_r));                                      // BUS_DEVINTSTAT
	map(0x078, 0x07b).rw(FUNC(solo_asic_device::reg_0078_r), FUNC(solo_asic_device::reg_0078_w)); // BUS_DEVINTSTAT_S
	map(0x174, 0x177).w(FUNC(solo_asic_device::reg_0174_w));                                      // BUS_DEVINTSTAT_C
	map(0x088, 0x08b).rw(FUNC(solo_asic_device::reg_0088_r), FUNC(solo_asic_device::reg_0088_w)); // BUS_VIDINTEN_S
	map(0x188, 0x18b).rw(FUNC(solo_asic_device::reg_0188_r), FUNC(solo_asic_device::reg_0188_w)); // BUS_VIDINTEN_C
	map(0x080, 0x083).r(FUNC(solo_asic_device::reg_0080_r));                                      // BUS_VIDINTSTAT
	map(0x084, 0x087).rw(FUNC(solo_asic_device::reg_0084_r), FUNC(solo_asic_device::reg_0084_w)); // BUS_VIDINTSTAT_S
	map(0x180, 0x183).w(FUNC(solo_asic_device::reg_0180_w));                                      // BUS_VIDINTSTAT_C
	map(0x098, 0x09b).rw(FUNC(solo_asic_device::reg_0098_r), FUNC(solo_asic_device::reg_0098_w)); // BUS_RIOINTEN_S
	map(0x198, 0x19b).rw(FUNC(solo_asic_device::reg_0198_r), FUNC(solo_asic_device::reg_0198_w)); // BUS_RIOINTEN_C
	map(0x08c, 0x08f).r(FUNC(solo_asic_device::reg_008c_r));                                      // BUS_RIOINTSTAT
	map(0x090, 0x093).rw(FUNC(solo_asic_device::reg_0090_r), FUNC(solo_asic_device::reg_0090_w)); // BUS_RIOINTSTAT_S
	map(0x18c, 0x18f).w(FUNC(solo_asic_device::reg_018c_w));                                      // BUS_RIOINTSTAT_C
	map(0x09c, 0x09f).r(FUNC(solo_asic_device::reg_009c_r));                                      // BUS_TIMINTSTAT
	map(0x090, 0x093).rw(FUNC(solo_asic_device::reg_00a0_r), FUNC(solo_asic_device::reg_00a0_w)); // BUS_TIMINTSTAT_S
	map(0x18c, 0x18f).w(FUNC(solo_asic_device::reg_019c_w));                                      // BUS_TIMINTSTAT_C
	map(0x0a8, 0x0ab).rw(FUNC(solo_asic_device::reg_00a8_r), FUNC(solo_asic_device::reg_00a8_w)); // BUS_RESETCAUSE
	map(0x0ac, 0x0af).w(FUNC(solo_asic_device::reg_00ac_w));                                      // BUS_RESETCAUSE_C
}

void solo_asic_device::rom_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_1000_r));                                      // ROM_SYSCONFIG
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_1004_r), FUNC(solo_asic_device::reg_1004_w)); // ROM_CNTL0
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_1008_r), FUNC(solo_asic_device::reg_1008_w)); // ROM_CNTL1
}

void solo_asic_device::aud_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_2000_r));                                      // AUD_CSTART
	map(0x004, 0x007).r(FUNC(solo_asic_device::reg_2004_r));                                      // AUD_CSIZE
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_2008_r), FUNC(solo_asic_device::reg_2008_w)); // AUD_CCONFIG
	map(0x00c, 0x00f).r(FUNC(solo_asic_device::reg_200c_r));                                      // AUD_CCNT
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_2010_r), FUNC(solo_asic_device::reg_2010_w)); // AUD_NSTART
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_2014_r), FUNC(solo_asic_device::reg_2014_w)); // AUD_NSIZE
	map(0x018, 0x01b).rw(FUNC(solo_asic_device::reg_2018_r), FUNC(solo_asic_device::reg_2018_w)); // AUD_NCONFIG
	map(0x01c, 0x01f).rw(FUNC(solo_asic_device::reg_201c_r), FUNC(solo_asic_device::reg_201c_w)); // AUD_DMACNTL
}

void solo_asic_device::vid_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_3000_r));                                      // VID_CSTART
	map(0x004, 0x007).r(FUNC(solo_asic_device::reg_3004_r));                                      // VID_CSIZE
	map(0x008, 0x00b).r(FUNC(solo_asic_device::reg_3008_r));                                      // VID_CCNT
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_300c_r), FUNC(solo_asic_device::reg_300c_w)); // VID_NSTART
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_3010_r), FUNC(solo_asic_device::reg_3010_w)); // VID_NSIZE
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_3014_r), FUNC(solo_asic_device::reg_3014_w)); // VID_DMACNTL
	map(0x038, 0x03b).r(FUNC(solo_asic_device::reg_3038_r));                                      // VID_INTSTAT
	map(0x138, 0x13b).w(FUNC(solo_asic_device::reg_3138_w));                                      // VID_INTSTAT_C
	map(0x03c, 0x03f).rw(FUNC(solo_asic_device::reg_303c_r), FUNC(solo_asic_device::reg_303c_w)); // VID_INTEN
	map(0x13c, 0x13f).w(FUNC(solo_asic_device::reg_313c_w));                                      // VID_INTEN_C
	map(0x040, 0x043).rw(FUNC(solo_asic_device::reg_3040_r), FUNC(solo_asic_device::reg_3040_w)); // VID_VDATA
}

void solo_asic_device::dev_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_4000_r));                                      // DEV_IROLD
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_4004_r), FUNC(solo_asic_device::reg_4004_w)); // DEV_LED
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_4008_r), FUNC(solo_asic_device::reg_4008_w)); // DEV_IDCNTL
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_400c_r), FUNC(solo_asic_device::reg_400c_w)); // DEV_NVCNTL
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_4010_r), FUNC(solo_asic_device::reg_4010_w)); // DEV_SCCNTL
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_4014_r), FUNC(solo_asic_device::reg_4014_w)); // DEV_EXTTIME
	map(0x018, 0x01b).rw(FUNC(solo_asic_device::reg_4018_r), FUNC(solo_asic_device::reg_4018_w)); // DEV_
	map(0x020, 0x023).rw(FUNC(solo_asic_device::reg_4020_r), FUNC(solo_asic_device::reg_4020_w)); // DEV_IRIN_SAMPLE
	map(0x024, 0x027).rw(FUNC(solo_asic_device::reg_4024_r), FUNC(solo_asic_device::reg_4024_w)); // DEV_IRIN_REJECT_INT
	map(0x028, 0x02b).r(FUNC(solo_asic_device::reg_4028_r));                                      // DEV_IRIN_TRANS_DATA
	map(0x02c, 0x02f).rw(FUNC(solo_asic_device::reg_402c_r), FUNC(solo_asic_device::reg_402c_w)); // DEV_IRIN_STATCNTL
}

void solo_asic_device::mem_unit_map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(solo_asic_device::reg_5000_r), FUNC(solo_asic_device::reg_5000_w)); // MEM_CNTL
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_5004_r), FUNC(solo_asic_device::reg_5004_w)); // MEM_REFCNT
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_5008_r), FUNC(solo_asic_device::reg_5008_w)); // MEM_DATA
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_500c_r), FUNC(solo_asic_device::reg_500c_w)); // MEM_CMD
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_5010_r), FUNC(solo_asic_device::reg_5010_w)); // MEM_TIMING
}

void solo_asic_device::gfx_unit_map(address_map &map)
{
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_6004_r), FUNC(solo_asic_device::reg_6004_w)); // GFX_CONTROL
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_6010_r), FUNC(solo_asic_device::reg_6010_w)); // GFX_OOTYCOUNT
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_6014_r), FUNC(solo_asic_device::reg_6014_w)); // GFX_CELSBASE
	map(0x018, 0x01b).rw(FUNC(solo_asic_device::reg_6018_r), FUNC(solo_asic_device::reg_6018_w)); // GFX_YMAPBASE
	map(0x01c, 0x01f).rw(FUNC(solo_asic_device::reg_601c_r), FUNC(solo_asic_device::reg_601c_w)); // GFX_CELSBASEMASTER
	map(0x020, 0x023).rw(FUNC(solo_asic_device::reg_6020_r), FUNC(solo_asic_device::reg_6020_w)); // GFX_YMAPBASEMASTER
	map(0x024, 0x027).rw(FUNC(solo_asic_device::reg_6024_r), FUNC(solo_asic_device::reg_6024_w)); // GFX_INITCOLOR
	map(0x028, 0x02b).rw(FUNC(solo_asic_device::reg_6028_r), FUNC(solo_asic_device::reg_6028_w)); // GFX_YCOUNTERINlT
	map(0x02c, 0x02f).rw(FUNC(solo_asic_device::reg_602c_r), FUNC(solo_asic_device::reg_602c_w)); // GFX_PAUSECYCLES
	map(0x030, 0x033).rw(FUNC(solo_asic_device::reg_6030_r), FUNC(solo_asic_device::reg_6030_w)); // GFX_OOTCELSBASE
	map(0x034, 0x037).rw(FUNC(solo_asic_device::reg_6034_r), FUNC(solo_asic_device::reg_6034_w)); // GFX_OOTYMAPBASE
	map(0x038, 0x03b).rw(FUNC(solo_asic_device::reg_6038_r), FUNC(solo_asic_device::reg_6038_w)); // GFX_OOTCELSOFFSET
	map(0x03c, 0x03f).rw(FUNC(solo_asic_device::reg_603c_r), FUNC(solo_asic_device::reg_603c_w)); // GFX_OOTYMAPCOUNT
	map(0x040, 0x043).rw(FUNC(solo_asic_device::reg_6040_r), FUNC(solo_asic_device::reg_6040_w)); // GFX_TERMCYCLECOUNT
	map(0x044, 0x047).rw(FUNC(solo_asic_device::reg_6044_r), FUNC(solo_asic_device::reg_6044_w)); // GFX_HCOUNTERINIT
	map(0x048, 0x04b).rw(FUNC(solo_asic_device::reg_6048_r), FUNC(solo_asic_device::reg_6048_w)); // GFX_BLANKLINES
	map(0x04c, 0x04f).rw(FUNC(solo_asic_device::reg_604c_r), FUNC(solo_asic_device::reg_604c_w)); // GFX_ACTIVELINES
	map(0x060, 0x063).rw(FUNC(solo_asic_device::reg_6060_r), FUNC(solo_asic_device::reg_6060_w)); // GFX_INTEN
	map(0x064, 0x067).w(FUNC(solo_asic_device::reg_6064_w));                                      // GFX_INTEN_C
	map(0x068, 0x06b).rw(FUNC(solo_asic_device::reg_6068_r), FUNC(solo_asic_device::reg_6068_w)); // GFX_INTSTAT
	map(0x06c, 0x06f).w(FUNC(solo_asic_device::reg_606c_w));                                      // GFX_INTSTAT_C
	map(0x080, 0x083).rw(FUNC(solo_asic_device::reg_6080_r), FUNC(solo_asic_device::reg_6080_w)); // GFX_WBDSTART
	map(0x084, 0x087).rw(FUNC(solo_asic_device::reg_6084_r), FUNC(solo_asic_device::reg_6084_w)); // GFX_WBDLSIZE
	map(0x08c, 0x08f).rw(FUNC(solo_asic_device::reg_608c_r), FUNC(solo_asic_device::reg_608c_w)); // GFX_WBSTRIDE
	map(0x090, 0x093).rw(FUNC(solo_asic_device::reg_6090_r), FUNC(solo_asic_device::reg_6090_w)); // GFX_WBDCONFIG
	map(0x094, 0x097).rw(FUNC(solo_asic_device::reg_6094_r), FUNC(solo_asic_device::reg_6094_w)); // GFX_WBDSTART
}

void solo_asic_device::dve_unit_map(address_map &map)
{
	//
}

void solo_asic_device::div_unit_map(address_map &map)
{
}

void solo_asic_device::pot_unit_map(address_map &map)
{
	map(0x080, 0x083).rw(FUNC(solo_asic_device::reg_9080_r), FUNC(solo_asic_device::reg_9080_w)); // POT_VSTART
	map(0x084, 0x087).rw(FUNC(solo_asic_device::reg_9084_r), FUNC(solo_asic_device::reg_9084_w)); // POT_VSIZE
	map(0x088, 0x08b).rw(FUNC(solo_asic_device::reg_9088_r), FUNC(solo_asic_device::reg_9088_w)); // POT_BLNKCOL
	map(0x08c, 0x08f).rw(FUNC(solo_asic_device::reg_908c_r), FUNC(solo_asic_device::reg_908c_w)); // POT_HSTART
	map(0x090, 0x093).rw(FUNC(solo_asic_device::reg_9090_r), FUNC(solo_asic_device::reg_9090_w)); // POT_HSIZE
	map(0x094, 0x097).rw(FUNC(solo_asic_device::reg_9094_r), FUNC(solo_asic_device::reg_9094_w)); // POT_CNTL
	map(0x098, 0x09b).rw(FUNC(solo_asic_device::reg_9098_r), FUNC(solo_asic_device::reg_9098_w)); // POT_HINTLINE
	map(0x09c, 0x09f).rw(FUNC(solo_asic_device::reg_909c_r), FUNC(solo_asic_device::reg_909c_w)); // POT_INTEN
	map(0x0a4, 0x0a7).w(FUNC(solo_asic_device::reg_90a4_w));                                      // POT_INTEN_C
	map(0x0a0, 0x0a3).r(FUNC(solo_asic_device::reg_90a0_r));                                      // POT_INTSTAT
	map(0x0a8, 0x0ab).w(FUNC(solo_asic_device::reg_90a8_w));                                      // POT_INTSTAT_C
	map(0x0ac, 0x0af).r(FUNC(solo_asic_device::reg_90ac_r));                                      // POT_CLINE
}

void solo_asic_device::suc_unit_map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(solo_asic_device::reg_a000_r), FUNC(solo_asic_device::reg_a000_w)); // SUCGPU_TFFHR
	map(0x00c, 0x00f).r(FUNC(solo_asic_device::reg_a00c_r));                                      // SUCGPU_TFFCNT
	map(0x010, 0x013).r(FUNC(solo_asic_device::reg_a010_r));                                      // SUCGPU_TFFMAX
	map(0xab8, 0xabb).r(FUNC(solo_asic_device::reg_aab8_r));                                      // SUCSC0_GPIOVAL
}

void solo_asic_device::mod_unit_map(address_map &map)
{
}

void solo_asic_device::hardware_modem_map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(solo_asic_device::reg_modem_0000_r), FUNC(solo_asic_device::reg_modem_0000_w)); // Modem I/O port base   (RBR/THR/DLL)
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_modem_0004_r), FUNC(solo_asic_device::reg_modem_0004_w)); // Modem I/O port base+1 (IER/DLM)
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_modem_0008_r), FUNC(solo_asic_device::reg_modem_0008_w)); // Modem I/O port base+2 (IIR/FCR)
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_modem_000c_r), FUNC(solo_asic_device::reg_modem_000c_w)); // Modem I/O port base+3 (LCR)
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_modem_0010_r), FUNC(solo_asic_device::reg_modem_0010_w)); // Modem I/O port base+4 (MCR)
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_modem_0014_r), FUNC(solo_asic_device::reg_modem_0014_w)); // Modem I/O port base+5 (LSR)
	map(0x018, 0x01b).rw(FUNC(solo_asic_device::reg_modem_0018_r), FUNC(solo_asic_device::reg_modem_0018_w)); // Modem I/O port base+6 (MSR)
	map(0x01c, 0x01f).rw(FUNC(solo_asic_device::reg_modem_001c_r), FUNC(solo_asic_device::reg_modem_001c_w)); // Modem I/O port base+7 (SCR)
}

void solo_asic_device::ide_map(address_map &map)
{
	map(0x000000, 0x000003).rw(FUNC(solo_asic_device::reg_ide_000000_r), FUNC(solo_asic_device::reg_ide_000000_w)); // IDE I/O port cs0[0] (data)
	map(0x000004, 0x000007).rw(FUNC(solo_asic_device::reg_ide_000004_r), FUNC(solo_asic_device::reg_ide_000004_w)); // IDE I/O port cs0[1] (error or feature)
	map(0x000008, 0x00000b).rw(FUNC(solo_asic_device::reg_ide_000008_r), FUNC(solo_asic_device::reg_ide_000008_w)); // IDE I/O port cs0[2] (sector count)
	map(0x00000c, 0x00000f).rw(FUNC(solo_asic_device::reg_ide_00000c_r), FUNC(solo_asic_device::reg_ide_00000c_w)); // IDE I/O port cs0[3] (sector number)
	map(0x000010, 0x000013).rw(FUNC(solo_asic_device::reg_ide_000010_r), FUNC(solo_asic_device::reg_ide_000010_w)); // IDE I/O port cs0[4] (cylinder low)
	map(0x000014, 0x000017).rw(FUNC(solo_asic_device::reg_ide_000014_r), FUNC(solo_asic_device::reg_ide_000014_w)); // IDE I/O port cs0[5] (cylinder high)
	map(0x000018, 0x00001b).rw(FUNC(solo_asic_device::reg_ide_000018_r), FUNC(solo_asic_device::reg_ide_000018_w)); // IDE I/O port cs0[6] (drive/head)
	map(0x00001c, 0x00001f).rw(FUNC(solo_asic_device::reg_ide_00001c_r), FUNC(solo_asic_device::reg_ide_00001c_w)); // IDE I/O port cs0[7] (status or command)
	map(0x400018, 0x40001b).rw(FUNC(solo_asic_device::reg_ide_400018_r), FUNC(solo_asic_device::reg_ide_400018_w)); // IDE I/O port cs1[6] (altstatus or device control)
	map(0x40001c, 0x40001f).rw(FUNC(solo_asic_device::reg_ide_40001c_r), FUNC(solo_asic_device::reg_ide_40001c_w)); // IDE I/O port cs1[7] (device address)
}

void solo_asic_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_screen_update(FUNC(solo_asic_device::screen_update));
	m_screen->screen_vblank().set(FUNC(solo_asic_device::vblank_irq));
	m_screen->set_raw(POT_DEFAULT_XTAL, POT_DEFAULT_HTOTAL, 0, POT_DEFAULT_HBSTART, POT_DEFAULT_VTOTAL, 0, POT_DEFAULT_VBSTART);

	SPEAKER(config, m_lspeaker).front_left();
	SPEAKER(config, m_rspeaker).front_right();
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[0], 0).add_route(0, m_lspeaker, 0.0);
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[1], 0).add_route(0, m_rspeaker, 0.0);

	NS16550(config, m_modem_uart, 1.8432_MHz_XTAL);
	m_modem_uart->out_tx_callback().set("modem", FUNC(rs232_port_device::write_txd));
	m_modem_uart->out_dtr_callback().set("modem", FUNC(rs232_port_device::write_dtr));
	m_modem_uart->out_rts_callback().set("modem", FUNC(rs232_port_device::write_rts));
	m_modem_uart->out_int_callback().set(FUNC(solo_asic_device::irq_modem_w));

	rs232_port_device &rs232(RS232_PORT(config, "modem", default_rs232_devices, "null_modem"));
	rs232.set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(wtv_modem));
	rs232.rxd_handler().set(m_modem_uart, FUNC(ns16450_device::rx_w));
	rs232.dcd_handler().set(m_modem_uart, FUNC(ns16450_device::dcd_w));
	rs232.dsr_handler().set(m_modem_uart, FUNC(ns16450_device::dsr_w));
	rs232.ri_handler().set(m_modem_uart, FUNC(ns16450_device::ri_w));
	rs232.cts_handler().set(m_modem_uart, FUNC(ns16450_device::cts_w));

	SEIJIN_KBD(config, m_irkbdc);
	m_irkbdc->sample_fifo_trigger_callback().set(FUNC(solo_asic_device::irq_keyboard_w));

	WATCHDOG_TIMER(config, m_watchdog);
	solo_asic_device::watchdog_enable(0);
}

void solo_asic_device::device_resolve_objects()
{
	// This grabs the configuration before it's usually done so we can read the PAL or NTSC bit to configure the screen.
	machine().manager().before_load_settings(machine());
	machine().configuration().load_settings();

	if (m_sys_config->read() & SYSCONFIG_PAL)
		m_screen->set_raw(PAL_SCREEN_XTAL, PAL_SCREEN_HTOTAL, 0, PAL_SCREEN_HBSTART, PAL_SCREEN_VTOTAL, 0, PAL_SCREEN_VBSTART);
	else
		m_screen->set_raw(NTSC_SCREEN_XTAL, NTSC_SCREEN_HTOTAL, 0, NTSC_SCREEN_HBSTART, NTSC_SCREEN_VTOTAL, 0, NTSC_SCREEN_VBSTART);
}

void solo_asic_device::device_start()
{
	m_power_led.resolve();
	m_connect_led.resolve();
	m_message_led.resolve();

	dac_update_timer = timer_alloc(FUNC(solo_asic_device::dac_update), this);
	modem_buffer_timer = timer_alloc(FUNC(solo_asic_device::flush_modem_buffer), this);
	compare_timer = timer_alloc(FUNC(solo_asic_device::timer_irq), this);

	solo_asic_device::device_reset();

	save_item(NAME(m_bus_intenable));
	save_item(NAME(m_bus_intstat));
	save_item(NAME(m_tcompare));
	save_item(NAME(m_busgpio_intenable));
	save_item(NAME(m_busgpio_intstat));
	save_item(NAME(m_busaud_intenable));
	save_item(NAME(m_busaud_intstat));
	save_item(NAME(m_busdev_intenable));
	save_item(NAME(m_busdev_intstat));
	save_item(NAME(m_busvid_intenable));
	save_item(NAME(m_busvid_intstat));
	save_item(NAME(m_busrio_intenable));
	save_item(NAME(m_busrio_intstat));
	save_item(NAME(m_bustim_intstat));
	save_item(NAME(m_errenable));
	save_item(NAME(m_chpcntl));
	save_item(NAME(m_wdenable));
	save_item(NAME(m_errstat));
	save_item(NAME(m_resetcause));

	save_item(NAME(m_vid_nstart));
	save_item(NAME(m_vid_nsize));
	save_item(NAME(m_vid_dmacntl));
	save_item(NAME(m_vid_cstart));
	save_item(NAME(m_vid_csize));
	save_item(NAME(m_vid_ccnt));
	save_item(NAME(m_vid_cline));
	save_item(NAME(m_vid_vdata));
	save_item(NAME(m_vid_intenable));
	save_item(NAME(m_vid_intstat));

	save_item(NAME(m_gfx_cntl));
	save_item(NAME(m_gfx_activelines));
	save_item(NAME(m_gfx_wbdstart));
	save_item(NAME(m_gfx_wbdlsize));
	save_item(NAME(m_gfx_intenable));
	save_item(NAME(m_gfx_intstat));

	save_item(NAME(m_div_intenable));
	save_item(NAME(m_div_intstat));

	save_item(NAME(m_pot_vstart));
	save_item(NAME(m_pot_vsize));
	save_item(NAME(m_pot_blank_color));
	save_item(NAME(m_pot_hstart));
	save_item(NAME(m_pot_hsize));
	save_item(NAME(m_pot_cntl));
	save_item(NAME(m_pot_hintline));
	save_item(NAME(m_pot_intenable));
	save_item(NAME(m_pot_intstat));

	save_item(NAME(m_vid_draw_nstart));
	save_item(NAME(m_pot_draw_hstart));
	save_item(NAME(m_pot_draw_hsize));
	save_item(NAME(m_pot_draw_vstart));
	save_item(NAME(m_pot_draw_vsize));
	save_item(NAME(m_pot_draw_blank_color));
	save_item(NAME(m_pot_draw_hintline));

	save_item(NAME(m_aud_cstart));
	save_item(NAME(m_aud_csize));
	save_item(NAME(m_aud_cconfig));
	save_item(NAME(m_aud_ccnt));
	save_item(NAME(m_aud_nstart));
	save_item(NAME(m_aud_nsize));
	save_item(NAME(m_aud_nconfig));
	save_item(NAME(m_aud_dmacntl));
	save_item(NAME(m_smrtcrd_serial_bitmask));
	save_item(NAME(m_smrtcrd_serial_rxdata));
	save_item(NAME(m_rom_cntl0));
	save_item(NAME(m_rom_cntl1));
	save_item(NAME(m_ledstate));
	save_item(NAME(dev_idcntl));
	save_item(NAME(dev_id_state));
	save_item(NAME(dev_id_bit));
	save_item(NAME(dev_id_bitidx));

	m_resetcause = 0x0;
}

void solo_asic_device::device_reset()
{
	dac_update_timer->adjust(attotime::from_hz(AUD_DEFAULT_CLK), 0, attotime::from_hz(AUD_DEFAULT_CLK));

	m_memcntl = 0b11;
	m_memrefcnt = 0x0400;
	m_memdata = 0x0;
	m_memcmd = 0x0;
	m_memtiming = 0xadbadffa;
	m_errenable = 0x0;
	m_chpcntl = 0x0;
	m_wdenable = 0x0;
	m_errstat = 0x0;
	m_nvcntl = 0x0;
	m_fence1_addr = 0x0;
	m_fence1_mask = 0x0;
	m_fence2_addr = 0x0;
	m_fence2_mask = 0x0;
	m_tcompare = 0x0;
	m_bus_intenable = 0x0;
	m_bus_intstat = 0x0;
	m_busgpio_intenable = 0x0;
	m_busgpio_intstat = 0x0;
	m_busaud_intenable = 0x0;
	m_busaud_intstat = 0x0;
	m_busdev_intenable = 0x0;
	m_busdev_intstat = 0x0;
	m_busvid_intenable = 0x0;
	m_busvid_intstat = 0x0;
	m_busrio_intenable = 0x0;
	m_busrio_intstat = 0x0;
	m_bustim_intstat = 0x0;

	m_vid_nstart = 0x0;
	m_vid_nsize = 0x0;
	m_vid_dmacntl = 0x0;
	m_vid_cstart = 0x0;
	m_vid_csize = 0x0;
	m_vid_ccnt = 0x0;
	m_vid_cline = 0x0;
	m_vid_vdata = 0x0;
	m_vid_intenable = 0x0;
	m_vid_intstat = 0x0;

	m_gfx_cntl = 0x0;
	m_gfx_activelines = 0x0;
	m_gfx_wbdstart = 0x0;
	m_gfx_wbdlsize = 0x0;
	m_gfx_intenable = 0x0;
	m_gfx_intstat = 0x0;

	m_div_intenable = 0x0;
	m_div_intstat = 0x0;

	m_pot_vstart = POT_DEFAULT_VSTART;
	m_pot_vsize = POT_DEFAULT_VSIZE;
	m_pot_blank_color = POT_DEFAULT_COLOR;
	m_pot_hstart = POT_VIDUNIT_HSTART_OFFSET + POT_DEFAULT_HSTART;
	m_pot_hsize = POT_DEFAULT_HSIZE;
	m_pot_cntl = 0x0;
	m_pot_hintline = 0x0;
	m_pot_intenable = 0x0;
	m_pot_intstat = 0x0;

	m_vid_draw_nstart = 0x0;
	m_pot_draw_hstart = POT_VIDUNIT_HSTART_OFFSET;
	m_pot_draw_hsize = m_pot_hsize;
	m_pot_draw_vstart = m_pot_vstart;
	m_pot_draw_vsize = m_pot_vsize;
	m_pot_draw_blank_color = m_pot_blank_color;
	m_pot_draw_hintline = 0x0;

	m_aud_cstart = 0x0;
	m_aud_csize = 0x0;
	m_aud_cend = 0x0;
	m_aud_cconfig = 0x0;
	m_aud_ccnt = 0x0;
	m_aud_nstart = 0x0;
	m_aud_nsize = 0x0;
	m_aud_nconfig = 0x0;
	m_aud_dmacntl = 0x0;
	m_aud_dma_ongoing = false;

	m_rom_cntl0 = 0x0;
	m_rom_cntl1 = 0x0;

	m_ledstate = 0xFFFFFFFF;
	m_power_led = 0;
	m_connect_led = 0;
	m_message_led = 0;

	dev_idcntl = 0x00;
	dev_id_state = SSID_STATE_IDLE;
	dev_id_bit = 0x0;
	dev_id_bitidx = 0x0;

	m_smrtcrd_serial_bitmask = 0x0;
	m_smrtcrd_serial_rxdata = 0x0;

	modem_txbuff_size = 0x0;
	modem_txbuff_index = 0x0;
	modfw_mode = true;
	modfw_message_index = 0x0;
	modfw_will_flush = false;
	modfw_will_ack = false;

	solo_asic_device::validate_active_area();
	solo_asic_device::watchdog_enable(m_wdenable);
	m_irkbdc->enable(1);

	// Assume it's a watchdog reset if the reset wasn't commanded using the BUS_RESETCAUSE register
	if (m_resetcause != RESETCAUSE_SOFTWARE)
	{
		m_resetcause = RESETCAUSE_WATCHDOG;
	}
}

void solo_asic_device::device_stop()
{
}

void solo_asic_device::validate_active_area()
{
	// The active h size can't be larger than the screen width or smaller than 2 pixels.
	m_pot_draw_hsize = std::clamp(m_pot_hsize, (uint32_t)0x2, (uint32_t)m_screen->width());
	// The active v size can't be larger than the screen height or smaller than 2 pixels.
	m_pot_draw_vsize = std::clamp(m_pot_vsize, (uint32_t)0x2, (uint32_t)m_screen->height());


	uint32_t hstart_offset = POT_VIDUNIT_HSTART_OFFSET;
	if (m_pot_cntl & POT_FCNTL_USEGFX)
	{
		hstart_offset = POT_GFXUNIT_HSTART_OFFSET;
	}
	else
	{
		hstart_offset = POT_VIDUNIT_HSTART_OFFSET;
	}

	// The active h start can't be smaller than 2
	m_pot_draw_hstart = std::max(m_pot_hstart - hstart_offset, (uint32_t)0x2);
	// The active v start can't be smaller than 2
	m_pot_draw_vstart = std::max(m_pot_vstart, (uint32_t)0x2);

	// The active h start can't push the active area off the screen.
	if ((m_pot_draw_hstart + m_pot_draw_hsize) > m_screen->width())
		m_pot_draw_hstart = (m_screen->width() - m_pot_draw_hsize); // to screen edge
	else if (m_pot_draw_hstart < 0)
		m_pot_draw_hstart = 0;

	// The active v start can't push the active area off the screen.
	if ((m_pot_draw_vstart + m_pot_draw_vsize) > m_screen->height())
		m_pot_draw_vstart = (m_screen->height() - m_pot_draw_vsize); // to screen edge
	else if (m_pot_draw_vstart < 0)
		m_pot_draw_vstart = 0;

	solo_asic_device::pixel_buffer_index_update();
}

void solo_asic_device::pixel_buffer_index_update()
{
	uint32_t screen_lines = m_pot_draw_vsize;
	uint32_t screen_buffer_size = m_vid_nsize;

	/*if (!(m_pot_cntl & POT_FCNTL_PROGRESSIVE))
	{
		// Interlace mode splits the buffer into two halfs. We can capture both halfs if we double the line count.
		screen_buffer_size = (screen_buffer_size * 2);
		screen_lines = (screen_lines * 2);
	}*/

	m_vid_draw_nstart = m_vid_nstart;

	if (m_emu_config->read() & EMUCONFIG_PBUFF1)
	{
		m_vid_draw_nstart += screen_buffer_size;
		m_vid_draw_nstart -= (m_pot_draw_hsize * VID_BYTES_PER_PIXEL);
		m_pot_draw_vsize = screen_lines;
	}
	else
	{
		m_vid_draw_nstart += 2 * (m_pot_draw_hsize * VID_BYTES_PER_PIXEL);
		m_pot_draw_vsize = screen_lines - 3;
	}
}

void solo_asic_device::watchdog_enable(int state)
{
	m_wdenable = state;

	if (m_wdenable)
		m_watchdog->set_time(attotime::from_usec(WATCHDOG_TIMER_USEC));
	else
		m_watchdog->set_time(attotime::zero);

	m_watchdog->watchdog_enable(m_wdenable);
}

uint32_t solo_asic_device::reg_0000_r()
{
	return 0x03120000;
}

uint32_t solo_asic_device::reg_0004_r()
{
	return m_chpcntl;
}

void solo_asic_device::reg_0004_w(uint32_t data)
{
	if ((m_chpcntl ^ data) & CHPCNTL_WDENAB_MASK)
	{
		uint32_t wd_cntl = (data & CHPCNTL_WDENAB_MASK);

		int32_t wd_diff = wd_cntl - (m_chpcntl & CHPCNTL_WDENAB_MASK);

		// Count down to disable (3, 2, 1, 0), count up to enable (0, 1, 2, 3)
		// This doesn't track the count history but gets the expected result for the ROM.
		if (
			(!m_wdenable && wd_diff > 0 && wd_cntl == CHPCNTL_WDENAB_SEQ3)
			|| (m_wdenable && wd_diff < 0 && wd_cntl == CHPCNTL_WDENAB_SEQ0)
		)
		{
			solo_asic_device::watchdog_enable(wd_cntl == CHPCNTL_WDENAB_SEQ3);
		}
	}

	if ((m_chpcntl ^ data) & CHPCNTL_AUDCLKDIV_MASK)
	{
		uint32_t audclk_cntl = (data & CHPCNTL_AUDCLKDIV_MASK);

		uint32_t sys_clk = solo_asic_device::clock();
		uint32_t aud_clk = AUD_DEFAULT_CLK;

		switch(audclk_cntl)
		{
			case CHPCNTL_AUDCLKDIV_EXTC:
			default:
				aud_clk = AUD_DEFAULT_CLK;
				break;

			case CHPCNTL_AUDCLKDIV_DIV1:
				aud_clk = sys_clk / (1 * 0x100);
				break;

			case CHPCNTL_AUDCLKDIV_DIV2:
				aud_clk = sys_clk / (2 * 0x100);
				break;

			case CHPCNTL_AUDCLKDIV_DIV3:
				aud_clk = sys_clk / (3 * 0x100);
				break;

			case CHPCNTL_AUDCLKDIV_DIV4:
				aud_clk = sys_clk / (4 * 0x100);
				break;

			case CHPCNTL_AUDCLKDIV_DIV5:
				aud_clk = sys_clk / (5 * 0x100);
				break;

			case CHPCNTL_AUDCLKDIV_DIV6:
				aud_clk = sys_clk / (6 * 0x100);
				break;

		}

		dac_update_timer->adjust(attotime::from_hz(aud_clk), 0, attotime::from_hz(aud_clk));
	}

	m_chpcntl = data;
}

uint32_t solo_asic_device::reg_0008_r()
{
	if (do7e_hack)
	{
		do7e_hack = false;

		return 0x01;
	}
	else
	{
		return m_bus_intstat;
	}
}

void solo_asic_device::reg_0108_w(uint32_t data)
{
	solo_asic_device::set_bus_irq(data, 0);
}

uint32_t solo_asic_device::reg_000c_r()
{
	return m_bus_intenable;
}

void solo_asic_device::reg_000c_w(uint32_t data)
{
	m_bus_intenable |= data & 0xff;
}

uint32_t solo_asic_device::reg_010c_r()
{
	return m_bus_intenable;
}

void solo_asic_device::reg_010c_w(uint32_t data)
{
	m_bus_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0010_r()
{
	return m_errstat;
}

uint32_t solo_asic_device::reg_0110_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_0110_w(uint32_t data)
{
	m_errstat &= (~data) & 0xFF;
}

uint32_t solo_asic_device::reg_0014_r()
{
	return m_errenable;
}

void solo_asic_device::reg_0014_w(uint32_t data)
{
	m_errenable |= data & 0xFF;
}

uint32_t solo_asic_device::reg_0114_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_0114_w(uint32_t data)
{
	// When ERR_LOWWRITE is disabled then we're in a shutdown phase.
	// This makes sure our hacks are in the appropriate state to work after shutdown.
	if(data == ERR_LOWWRITE)
	{
		// Turn back on the modem downloader hack.
		modfw_mode = true;
		modfw_message_index = 0x0;
		modfw_will_flush = false;
		modfw_will_ack = false;

		// Disable the timer interrupt. This is related to a reg_019c_w issue.
		m_bus_intstat &= (~BUS_INT_TIMER) & 0xff;
	}

	m_errenable &= (~data) & 0xFF;
}

uint32_t solo_asic_device::reg_0018_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_0118_w(uint32_t data)
{
	if (m_wdenable)
		m_watchdog->reset_w(data);
}

uint32_t solo_asic_device::reg_001c_r()
{
	return m_fence1_addr;
}

void solo_asic_device::reg_001c_w(uint32_t data)
{
	m_fence1_addr = data;
}

uint32_t solo_asic_device::reg_0020_r()
{
	return m_fence1_mask;
}

void solo_asic_device::reg_0020_w(uint32_t data)
{
	m_fence1_mask = data;
}

uint32_t solo_asic_device::reg_0024_r()
{
	return m_fence2_addr;
}

void solo_asic_device::reg_0024_w(uint32_t data)
{
	m_fence2_addr = data;
}

uint32_t solo_asic_device::reg_0028_r()
{
	return m_fence2_mask;
}

void solo_asic_device::reg_0028_w(uint32_t data)
{
	m_fence2_mask = data;
}

uint32_t solo_asic_device::reg_0048_r()
{
	return m_hostcpu->total_cycles() >> 1;
}

void solo_asic_device::reg_0048_w(uint32_t data)
{
}

uint32_t solo_asic_device::reg_004c_r()
{
	return m_tcompare;
}

void solo_asic_device::reg_004c_w(uint32_t data)
{
	// Make sure the interrupt is cleared. Fix for a reg_019c_w issue.
	solo_asic_device::set_timer_irq(BUS_INT_TIM_SYSTIMER, 0);

	m_tcompare = data;

	m_bus_intenable |= BUS_INT_TIMER;

	// There seems to be an issue using the timer compare value. Hardcoding this to around 0.01s which seems to be what the firmware is using.
	compare_timer->adjust(attotime::from_usec(TCOMPARE_TIMER_USEC));
	//compare_timer->adjust(m_hostcpu->cycles_to_attotime(((m_tcompare - m_hostcpu->total_cycles()) * 2)));
}

uint32_t solo_asic_device::reg_005c_r()
{
	return m_busgpio_intenable;
}

void solo_asic_device::reg_005c_w(uint32_t data)
{
	m_busgpio_intenable |= data & 0xff;
}

uint32_t solo_asic_device::reg_015c_r()
{
	return m_busgpio_intenable;
}

void solo_asic_device::reg_015c_w(uint32_t data)
{
	m_busgpio_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0058_r()
{
	return m_busgpio_intstat;
}

uint32_t solo_asic_device::reg_0060_r()
{
	return m_busgpio_intstat;
}

void solo_asic_device::reg_0060_w(uint32_t data)
{
	m_busgpio_intstat |= data & 0xff;
}

void solo_asic_device::reg_0158_w(uint32_t data)
{
	m_busgpio_intstat &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0070_r()
{
	return m_busaud_intenable;
}

void solo_asic_device::reg_0070_w(uint32_t data)
{
	m_busaud_intenable |= data & 0xff;
	if (m_busaud_intenable != 0x0)
	{
		m_bus_intenable |= BUS_INT_AUDIO;
	}
}

uint32_t solo_asic_device::reg_0170_r()
{
	return m_busaud_intenable;
}

void solo_asic_device::reg_0170_w(uint32_t data)
{
	m_busaud_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0068_r()
{
	return m_busaud_intstat;
}

uint32_t solo_asic_device::reg_006c_r()
{
	return m_busaud_intstat;
}

void solo_asic_device::reg_006c_w(uint32_t data)
{
	m_busaud_intstat |= data & 0xff;
}

void solo_asic_device::reg_0168_w(uint32_t data)
{
	solo_asic_device::set_audio_irq(data, 0);
}

uint32_t solo_asic_device::reg_007c_r()
{
	return m_busdev_intenable;
}

void solo_asic_device::reg_007c_w(uint32_t data)
{
	m_busdev_intenable |= data & 0xff;
	if (m_busdev_intenable != 0x0)
	{
		m_bus_intenable |= BUS_INT_DEV;
	}
}

uint32_t solo_asic_device::reg_017c_r()
{
	return m_busdev_intenable;
}

void solo_asic_device::reg_017c_w(uint32_t data)
{
	m_busdev_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0074_r()
{
	return m_busdev_intstat;
}

uint32_t solo_asic_device::reg_0078_r()
{
	return m_busdev_intstat;
}

void solo_asic_device::reg_0078_w(uint32_t data)
{
	m_busdev_intstat |= data & 0xff;
}

void solo_asic_device::reg_0174_w(uint32_t data)
{
	solo_asic_device::set_dev_irq(data, 0);
}

uint32_t solo_asic_device::reg_0088_r()
{
	return m_busvid_intenable;
}

void solo_asic_device::reg_0088_w(uint32_t data)
{
	m_busvid_intenable |= data & 0xff;
	if (m_busvid_intenable != 0x0)
	{
		m_bus_intenable |= BUS_INT_VIDEO;
	}
}

uint32_t solo_asic_device::reg_0188_r()
{
	return m_busvid_intenable;
}

void solo_asic_device::reg_0188_w(uint32_t data)
{
	m_busvid_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_0080_r()
{
	return m_busvid_intstat;
}

uint32_t solo_asic_device::reg_0084_r()
{
	return m_busvid_intstat;
}

void solo_asic_device::reg_0084_w(uint32_t data)
{
	m_busvid_intstat |= data & 0xff;
}

void solo_asic_device::reg_0180_w(uint32_t data)
{
	uint32_t check_intstat = 0x0;

	switch (data)
	{
		case BUS_INT_VID_DIVUNIT:
			check_intstat = m_div_intstat;
			break;

		case BUS_INT_VID_GFXUNIT:
			check_intstat = m_gfx_intstat;
			break;

		case BUS_INT_VID_POTUNIT:
			check_intstat = m_pot_intstat;
			break;

		case BUS_INT_VID_VIDUNIT:
			check_intstat = m_vid_intstat;
			break;
	}

	if (check_intstat == 0x0)
	{
		m_busvid_intstat &= (~data) & 0xff;

		if(m_busvid_intstat == 0x0)
		{
			solo_asic_device::set_bus_irq(BUS_INT_VIDEO, 0);
		}
	}
}

uint32_t solo_asic_device::reg_0098_r()
{
	return m_busrio_intenable;
}

void solo_asic_device::reg_0098_w(uint32_t data)
{
	m_busrio_intenable |= data & 0xff;
	if (m_busrio_intenable != 0x0)
	{
		m_bus_intenable |= BUS_INT_RIO;
	}
}

uint32_t solo_asic_device::reg_0198_r()
{
	return m_busrio_intenable;
}

void solo_asic_device::reg_0198_w(uint32_t data)
{
	if (data != BUS_INT_RIO_DEVICE0) // The modem timinng is incorrect, so ignore the ROM trying to disable the modem interrupt.
	{
		m_busrio_intenable &= (~data) & 0xff;
	}
}

uint32_t solo_asic_device::reg_008c_r()
{
	return m_busrio_intstat;
}

uint32_t solo_asic_device::reg_0090_r()
{
	return m_busrio_intstat;
}

void solo_asic_device::reg_0090_w(uint32_t data)
{
	m_busrio_intstat |= data & 0xff;
}

void solo_asic_device::reg_018c_w(uint32_t data)
{
	solo_asic_device::set_rio_irq(data, 0);
}

uint32_t solo_asic_device::reg_009c_r()
{
	return m_bustim_intstat;
}

uint32_t solo_asic_device::reg_00a0_r()
{
	return m_bustim_intstat;
}

void solo_asic_device::reg_00a0_w(uint32_t data)
{
	m_bustim_intstat |= data & 0xff;
}

void solo_asic_device::reg_019c_w(uint32_t data)
{
	// Removing due to problems. The interrupt will be cleared when a new compare value is written.
	//solo_asic_device::set_timer_irq(data, 0);
}

uint32_t solo_asic_device::reg_00a8_r()
{
	return m_resetcause;
}

void solo_asic_device::reg_00a8_w(uint32_t data)
{
	m_resetcause |= data;

	if (m_resetcause & RESETCAUSE_SOFTWARE)
	{
		m_resetcause = RESETCAUSE_SOFTWARE;

		popmessage("Software reset fired");

		machine().schedule_soft_reset();
	}
}

void solo_asic_device::reg_00ac_w(uint32_t data)
{
	m_resetcause &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_1000_r()
{
	return m_sys_config->read();
}

uint32_t solo_asic_device::reg_1004_r()
{
	return m_rom_cntl0;
}
void solo_asic_device::reg_1004_w(uint32_t data)
{
	m_rom_cntl0 = data;
}

uint32_t solo_asic_device::reg_1008_r()
{
	return m_rom_cntl1;
}

void solo_asic_device::reg_1008_w(uint32_t data)
{
	m_rom_cntl1 = data;
}

uint32_t solo_asic_device::reg_2000_r()
{
	return m_aud_cstart;
}

uint32_t solo_asic_device::reg_2004_r()
{
	return m_aud_csize;
}

uint32_t solo_asic_device::reg_2008_r()
{
	return m_aud_cconfig;
}

void solo_asic_device::reg_2008_w(uint32_t data)
{
	m_aud_cconfig = data;
}

uint32_t solo_asic_device::reg_200c_r()
{
	return m_aud_ccnt;
}

uint32_t solo_asic_device::reg_2010_r()
{
	return m_aud_nstart;
}

void solo_asic_device::reg_2010_w(uint32_t data)
{
	m_aud_nstart = data;
}

uint32_t solo_asic_device::reg_2014_r()
{
	return m_aud_nsize;
}

void solo_asic_device::reg_2014_w(uint32_t data)
{
	m_aud_nsize = data;
}

uint32_t solo_asic_device::reg_2018_r()
{
	return m_aud_nconfig;
}

void solo_asic_device::reg_2018_w(uint32_t data)
{
	m_aud_nconfig = data;
}

uint32_t solo_asic_device::reg_201c_r()
{
	solo_asic_device::set_audio_irq(BUS_INT_AUD_AUDDMAOUT, 0);

	return m_aud_dmacntl;
}

void solo_asic_device::reg_201c_w(uint32_t data)
{
	if ((m_aud_dmacntl ^ data) & AUD_DMACNTL_DMAEN)
	{
		if (data & AUD_DMACNTL_DMAEN)
		{
			m_lspeaker->set_input_gain(0, AUD_OUTPUT_GAIN);
			m_rspeaker->set_input_gain(0, AUD_OUTPUT_GAIN);
		}
		else
		{
			m_lspeaker->set_input_gain(0, 0.0);
			m_rspeaker->set_input_gain(0, 0.0);
		}
	}

	m_aud_dmacntl = data;
}

uint32_t solo_asic_device::reg_3000_r()
{
	return m_vid_cstart;
}

uint32_t solo_asic_device::reg_3004_r()
{
	return m_vid_csize;
}

uint32_t solo_asic_device::reg_3008_r()
{
	return m_vid_ccnt;
}

uint32_t solo_asic_device::reg_300c_r()
{
	return m_vid_nstart;
}

void solo_asic_device::reg_300c_w(uint32_t data)
{
	bool has_changed = (m_vid_nstart != data);

	m_vid_nstart = data;

	if (has_changed)
		solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_3010_r()
{
	return m_vid_nsize;
}

void solo_asic_device::reg_3010_w(uint32_t data)
{
	bool has_changed = (m_vid_nsize != data);

	m_vid_nsize = data;

	if (has_changed)
		solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_3014_r()
{
	return m_vid_dmacntl;
}

void solo_asic_device::reg_3014_w(uint32_t data)
{
	if ((m_vid_dmacntl ^ data) & VID_DMACNTL_NV && data & VID_DMACNTL_NV)
		solo_asic_device::pixel_buffer_index_update();

	m_vid_dmacntl = data;
}

uint32_t solo_asic_device::reg_3038_r()
{
	return m_vid_intstat;
}

void solo_asic_device::reg_3138_w(uint32_t data)
{
	solo_asic_device::set_video_irq(BUS_INT_VID_VIDUNIT, data, 0);
}

uint32_t solo_asic_device::reg_303c_r()
{
	return m_vid_intenable;
}

void solo_asic_device::reg_303c_w(uint32_t data)
{
	m_vid_intenable |= (data & 0xff);
	if (m_vid_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_VIDUNIT;
		m_bus_intenable |= BUS_INT_VIDEO;
	}
}

void solo_asic_device::reg_313c_w(uint32_t data)
{
	m_vid_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_3040_r()
{
	return m_vid_vdata;
}

void solo_asic_device::reg_3040_w(uint32_t data)
{
	m_vid_vdata = data;
}

// Read IR receiver chip
uint32_t solo_asic_device::reg_4000_r()
{
	return m_irkbdc->data_r(wtvir_seijin_device::DEV_IROLD);
}

// Read LED states
uint32_t solo_asic_device::reg_4004_r()
{
    m_power_led = !BIT(m_ledstate, 2);
    m_connect_led = !BIT(m_ledstate, 1);
    m_message_led = !BIT(m_ledstate, 0);
    return m_ledstate;
}

// Update LED states
void solo_asic_device::reg_4004_w(uint32_t data)
{
	m_ledstate = data;
	m_power_led = !BIT(m_ledstate, 2);
	m_connect_led = !BIT(m_ledstate, 1);
	m_message_led = !BIT(m_ledstate, 0);
}

// Not using logic inside DS2401.cpp because the delay logic in the ROM doesn't work properly.

uint32_t solo_asic_device::reg_4008_r()
{
	dev_id_bit = 0x0;

	if (dev_id_state == SSID_STATE_PRESENCE)
	{
		dev_id_bit = 0x0; // We're present.
		dev_id_state = SSID_STATE_COMMAND; // This normally would stay in presence mode for 480us then command, but we immediatly go into command mode.
		dev_id_bitidx = 0x0;
	}
	else if (dev_id_state == SSID_STATE_READROM_PULSEEND)
	{
		dev_id_state = SSID_STATE_READROM_BIT;
	}
	else if (dev_id_state == SSID_STATE_READROM_BIT)
	{
		dev_id_state = SSID_STATE_READROM; // Go back into the read ROM pulse state

		dev_id_bit = m_serial_id->direct_read(dev_id_bitidx / 8) >> (dev_id_bitidx & 0x7);

		dev_id_bitidx++;
		if (dev_id_bitidx == 64)
		{
			// We've read the entire SSID. Go back into idle.
			dev_id_state = SSID_STATE_IDLE;
			dev_id_bitidx = 0x0;
		}
	}

	return dev_idcntl | (dev_id_bit & 1);
}

void solo_asic_device::reg_4008_w(uint32_t data)
{
	dev_idcntl = (data & 0x2);

	if (dev_idcntl & 0x2)
	{
		switch(dev_id_state) // States for high
		{
			case SSID_STATE_RESET: // End reset low pulse to go into prescense mode. Chip should read low to indicate presence.
				dev_id_state = SSID_STATE_PRESENCE; // This pulse normally lasts 480us before going into command mode.
				break;

			case SSID_STATE_COMMAND: // Ended a command bit pulse. Increment bit index. We always assume a read from ROM command after we get 8 bits.
				dev_id_bitidx++;

				if (dev_id_bitidx == 8)
				{
					dev_id_state = SSID_STATE_READROM; // Now we can read back the SSID. ROM reads it as two 32-bit integers.
					dev_id_bitidx = 0;
				}
				break;

			case SSID_STATE_READROM_PULSESTART:
				dev_id_state = SSID_STATE_READROM_PULSEEND;
		}
	}
	else
	{
		switch(dev_id_state) // States for low
		{
			case SSID_STATE_IDLE: // When idle, we can drive the chip low for reset
				dev_id_state = SSID_STATE_RESET; // We'd normally leave this for 480us to go into presence mode.
				break;

			case SSID_STATE_READROM:
				dev_id_state = SSID_STATE_READROM_PULSESTART;
				break;
		}
	}
}

// 400c commands the I2C bus (referenced as the IIC bus in WebTV's code)
//
// The SOLO programming doc calls this as an NVCNTL register but this us used as an I2C register.
//
// There's two known devices that sit on this bus:
//
//	Address		Device
//	0x8C		Philips SAA7187 encoder
//				Used for the S-Video and composite out
//	0xa0		Atmel AT24C01A EEPROM NVRAM
//				Used for the encryption shared secret (0x14) and crash log counter (0x23)
//
// We emulate the AT24C01A here.
//
uint32_t solo_asic_device::reg_400c_r()
{
	int sda_bit = (m_nvram->read_sda()) & 0x1;

	return (m_nvcntl & 0xE) | sda_bit;
}

void solo_asic_device::reg_400c_w(uint32_t data)
{
	if (data & NVCNTL_WRITE_EN) {
		m_nvram->write_sda(((data & NVCNTL_SDA_W) == NVCNTL_SDA_W) & 0x1);
	} else {
		m_nvram->write_sda(0x1);
	}

	m_nvram->write_scl(((data & NVCNTL_SCL) == NVCNTL_SCL) & 1);

	m_nvcntl = data & 0xE;
}

uint32_t solo_asic_device::reg_4010_r()
{
	return 0;
}

void solo_asic_device::reg_4010_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_4014_r()
{
	return 0;
}

void solo_asic_device::reg_4014_w(uint32_t data)
{
}

uint32_t solo_asic_device::reg_4018_r()
{
	return 0x00000000; //
}

void solo_asic_device::reg_4018_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_4020_r()
{
	return m_irkbdc->data_r(wtvir_seijin_device::DEV_IRIN_SAMPLE);
}

void solo_asic_device::reg_4020_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_seijin_device::DEV_IRIN_SAMPLE, data);
}

uint32_t solo_asic_device::reg_4024_r()
{
	return m_irkbdc->data_r(wtvir_seijin_device::DEV_IRIN_REJECT_INT);
}

void solo_asic_device::reg_4024_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_seijin_device::DEV_IRIN_REJECT_INT, data);
}

uint32_t solo_asic_device::reg_4028_r()
{
	return m_irkbdc->data_r(wtvir_seijin_device::DEV_IRIN_TRANS_DATA);
}

uint32_t solo_asic_device::reg_402c_r()
{
	return m_irkbdc->data_r(wtvir_seijin_device::DEV_IRIN_STATCNTL);
}

void solo_asic_device::reg_402c_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_seijin_device::DEV_IRIN_STATCNTL, data);
}

// memUnit registers

uint32_t solo_asic_device::reg_5000_r()
{
	return m_memcntl;
}

void solo_asic_device::reg_5000_w(uint32_t data)
{
	m_memcntl = data;
}

uint32_t solo_asic_device::reg_5004_r()
{
	return m_memrefcnt;
}

void solo_asic_device::reg_5004_w(uint32_t data)
{
	m_memrefcnt = data;
}

uint32_t solo_asic_device::reg_5008_r()
{
	return m_memdata;
}

void solo_asic_device::reg_5008_w(uint32_t data)
{
	m_memdata = data;
}

uint32_t solo_asic_device::reg_500c_r()
{
	// FIXME: This is defined as a write-only register, yet the WebTV software reads from it? Still need to see what the software expects from this.
	return m_memcmd;
}

void solo_asic_device::reg_500c_w(uint32_t data)
{
	m_memcmd = data;
}

uint32_t solo_asic_device::reg_5010_r()
{
	return m_memtiming;
}

void solo_asic_device::reg_5010_w(uint32_t data)
{
	m_memtiming = data;
}

// gfxUnit registers

uint32_t solo_asic_device::reg_6004_r()
{
	return m_gfx_cntl;
}

void solo_asic_device::reg_6004_w(uint32_t data)
{
	m_gfx_cntl = data;
}

uint32_t solo_asic_device::reg_6010_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6010_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6014_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6014_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6018_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6018_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_601c_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_601c_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6020_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6020_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6024_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6024_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6028_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6028_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_602c_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_602c_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6030_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6030_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6034_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6034_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6038_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6038_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_603c_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_603c_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6040_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6040_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6044_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6044_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_6048_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6048_w(uint32_t data)
{
	//
}

uint32_t solo_asic_device::reg_604c_r()
{
	return m_gfx_activelines;
}

void solo_asic_device::reg_604c_w(uint32_t data)
{
	m_gfx_activelines = data;
}

uint32_t solo_asic_device::reg_6060_r()
{
	return m_gfx_intenable;
}

void solo_asic_device::reg_6060_w(uint32_t data)
{
	m_gfx_intenable |= (data & 0xff);
	if (m_gfx_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_GFXUNIT;
		m_bus_intenable |= BUS_INT_VIDEO;
	}
}

void solo_asic_device::reg_6064_w(uint32_t data)
{
	solo_asic_device::set_video_irq(BUS_INT_VID_GFXUNIT, data, 0);
}

uint32_t solo_asic_device::reg_6068_r()
{
	return m_gfx_intstat;
}

void solo_asic_device::reg_6068_w(uint32_t data)
{
	m_gfx_intstat |= (data & 0xff);
}

void solo_asic_device::reg_606c_w(uint32_t data)
{
	m_gfx_intstat &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_6080_r()
{
	return m_gfx_wbdstart;
}

void solo_asic_device::reg_6080_w(uint32_t data)
{
	m_gfx_wbdstart = data;
}
uint32_t solo_asic_device::reg_6084_r()
{
	return m_gfx_wbdlsize;
}

void solo_asic_device::reg_6084_w(uint32_t data)
{
	m_gfx_wbdlsize = data;
}
uint32_t solo_asic_device::reg_608c_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_608c_w(uint32_t data)
{
	//
}
uint32_t solo_asic_device::reg_6090_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6090_w(uint32_t data)
{
	//
}
uint32_t solo_asic_device::reg_6094_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_6094_w(uint32_t data)
{
	//
}

// potUnit registers

uint32_t solo_asic_device::reg_9080_r()
{
	return m_pot_vstart;
}

void solo_asic_device::reg_9080_w(uint32_t data)
{
	bool has_changed = (m_pot_vstart != data);

	m_pot_vstart = data;

	if (has_changed)
			solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_9084_r()
{
	return m_pot_vsize;
}

void solo_asic_device::reg_9084_w(uint32_t data)
{
	bool has_changed = (m_pot_vstart != data);

	m_pot_vsize = data;

	if (has_changed)
			solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_9088_r()
{
	return m_pot_blank_color;
}

void solo_asic_device::reg_9088_w(uint32_t data)
{
	m_pot_blank_color = data;

	m_pot_draw_blank_color = (((data >> 0x10) & 0xff) << 0x18) | (((data >> 0x08) & 0xff) << 0x10) | (((data >> 0x10) & 0xff) << 0x08) | (data & 0xff);     
}

uint32_t solo_asic_device::reg_908c_r()
{
	return m_pot_hstart;
}

void solo_asic_device::reg_908c_w(uint32_t data)
{
	bool has_changed = (m_pot_hstart != data);

	m_pot_hstart = data;

	if (has_changed)
			solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_9090_r()
{
	return m_pot_hsize;
}

void solo_asic_device::reg_9090_w(uint32_t data)
{
	bool has_changed = (m_pot_hsize != data);

	m_pot_hsize = data;

	if (has_changed)
			solo_asic_device::validate_active_area();
}

uint32_t solo_asic_device::reg_9094_r()
{
	return m_pot_cntl;
}

void solo_asic_device::reg_9094_w(uint32_t data)
{
	m_pot_cntl = data;
}

uint32_t solo_asic_device::reg_9098_r()
{
	return m_pot_hintline;
}

void solo_asic_device::reg_9098_w(uint32_t data)
{
	m_pot_hintline = data;

	m_pot_draw_hintline = std::min(m_pot_hintline, (uint32_t)(m_screen->height() - 1));
}

// _vid_ int variables are being used because everything fires as Vvid interrupts right now (code was copied from SPOT)

uint32_t solo_asic_device::reg_909c_r()
{
	return m_pot_intenable;
}

void solo_asic_device::reg_909c_w(uint32_t data)
{
	m_pot_intenable |= (data & 0xff);
	if (m_pot_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_POTUNIT;
		m_bus_intenable |= BUS_INT_VIDEO;
	}
}

void solo_asic_device::reg_90a4_w(uint32_t data)
{
	m_pot_intenable &= (~data) & 0xff;
}

uint32_t solo_asic_device::reg_90a0_r()
{
	return m_pot_intstat;
}

void solo_asic_device::reg_90a8_w(uint32_t data)
{
	solo_asic_device::set_video_irq(BUS_INT_VID_POTUNIT, data, 0);
}

uint32_t solo_asic_device::reg_90ac_r()
{
	return m_screen->vpos();
}

uint32_t solo_asic_device::reg_a000_r()
{
	return 0x00000000;
}

void solo_asic_device::reg_a000_w(uint32_t data)
{
	osd_printf_verbose("%c", (data & 0xff));
}

uint32_t solo_asic_device::reg_a00c_r()
{
	return 0x00000000;
}

uint32_t solo_asic_device::reg_a010_r()
{
	return 0x00000001;
}

uint32_t solo_asic_device::reg_aab8_r()
{
	return (0x1 << 4);
}

uint32_t solo_asic_device::reg_modem_0000_r()
{
	if(modfw_mode)
	{
		if(modfw_will_ack)
		{
			modfw_will_ack = false;

			return MODFW_RBR_ACK;
		}
		else
		{
			if(modfw_message_index < sizeof(modfw_message))
			{
				uint32_t message_chr = modfw_message[modfw_message_index++] & 0xff;

				if(modfw_message_index == MODFW_MSG_IDX_FLUSH0 || modfw_message_index == MODFW_MSG_IDX_FLUSH1)
				{
					modfw_will_flush = true;
				}
				else if((modfw_message_index + 1) >= sizeof(modfw_message))
				{
					modfw_mode = false;
				}

				return message_chr;
			}
			else
			{
				return MODFW_NULL_RESULT;
			}
		}
	}
	else
	{
		uint32_t data = m_modem_uart->ins8250_r(0x0);

		// There's a bug in the WebTV OS/firmware that sets the modem base address to 0xa4000000
		// This happens after it does a PPP signal call when it recieves a 0x7e byte. This causes everything to be messed up and crash.
		// So then prepare the reg_0008_r register ("Modem IIR") to return a no interrupt pending ID after we pre-emptidly detect the 0x7e byte.
		// No idea why this doesn't happen on real hardware but this is a workaround for MAME.
		if (data == 0x7e)
		{
			do7e_hack = true;
		}

		return data;
	}
}

 void solo_asic_device::reg_modem_0000_w(uint32_t data)
 {
	if(modfw_mode)
	{
		modfw_will_ack = true;
	}
	else
	{
		if (modem_txbuff_size == 0 && (m_modem_uart->ins8250_r(0x5) & INS8250_LSR_TSRE))
		{
			m_modem_uart->ins8250_w(0x0, data & 0xff);
		}
		else
		{
			modem_txbuff[modem_txbuff_size++ & (MBUFF_MAX_SIZE - 1)] = data & 0xff;

			modem_buffer_timer->adjust(attotime::from_usec(MBUFF_FLUSH_TIME));
		}
	}
 }

uint32_t solo_asic_device::reg_modem_0004_r()
{
	return m_modem_uart->ins8250_r(0x1);
}

void solo_asic_device::reg_modem_0004_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x1, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0008_r()
{
	return m_modem_uart->ins8250_r(0x2);
}

void solo_asic_device::reg_modem_0008_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x2, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_000c_r()
{
	return m_modem_uart->ins8250_r(0x3);
}

void solo_asic_device::reg_modem_000c_w(uint32_t data)
{
	// If this register is written to then the downloader phase has finished. Make sure to turn the modemfw hack off.
	modfw_mode = false;

	m_modem_uart->ins8250_w(0x3, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0010_r()
{
	return m_modem_uart->ins8250_r(0x4);
}

void solo_asic_device::reg_modem_0010_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x4, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0014_r()
{
	if(modfw_mode)
	{
		if(modfw_will_flush)
		{
			modfw_will_flush = false;

			return MODFW_NULL_RESULT;
		}
		else
		{
			return MODFW_LSR_READY;
		}
	}
	else
	{
		return m_modem_uart->ins8250_r(0x5);
	}
}

void solo_asic_device::reg_modem_0014_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x5, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0018_r()
{
	return m_modem_uart->ins8250_r(0x6);
}

void solo_asic_device::reg_modem_0018_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x6, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_001c_r()
{
	return m_modem_uart->ins8250_r(0x7);
}

void solo_asic_device::reg_modem_001c_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x7, data & 0xff);
}

uint32_t solo_asic_device::reg_ide_000000_r()
{
	return m_ata->cs0_r(0);
}

void solo_asic_device::reg_ide_000000_w(uint32_t data)
{
	m_ata->cs0_w(0, data);
}

uint32_t solo_asic_device::reg_ide_000004_r()
{
	return m_ata->cs0_r(1);
}

void solo_asic_device::reg_ide_000004_w(uint32_t data)
{
	m_ata->cs0_w(1, data);
}

uint32_t solo_asic_device::reg_ide_000008_r()
{
	return m_ata->cs0_r(2);
}

void solo_asic_device::reg_ide_000008_w(uint32_t data)
{
	m_ata->cs0_w(2, data);
}

uint32_t solo_asic_device::reg_ide_00000c_r()
{
	return m_ata->cs0_r(3);
}

void solo_asic_device::reg_ide_00000c_w(uint32_t data)
{
	m_ata->cs0_w(3, data);
}

uint32_t solo_asic_device::reg_ide_000010_r()
{
	return m_ata->cs0_r(4);
}

void solo_asic_device::reg_ide_000010_w(uint32_t data)
{
	m_ata->cs0_w(4, data);
}

uint32_t solo_asic_device::reg_ide_000014_r()
{
	return m_ata->cs0_r(5);
}

void solo_asic_device::reg_ide_000014_w(uint32_t data)
{
	m_ata->cs0_w(5, data);
}

uint32_t solo_asic_device::reg_ide_000018_r()
{
	return m_ata->cs0_r(6);
}

void solo_asic_device::reg_ide_000018_w(uint32_t data)
{
	m_ata->cs0_w(6, data);
}

uint32_t solo_asic_device::reg_ide_00001c_r()
{
	return m_ata->cs0_r(7);
}

void solo_asic_device::reg_ide_00001c_w(uint32_t data)
{
	m_ata->cs0_w(7, data);
}

uint32_t solo_asic_device::reg_ide_400018_r()
{
	return m_ata->cs1_r(6);
}

void solo_asic_device::reg_ide_400018_w(uint32_t data)
{
	m_ata->cs1_w(6, data);
}

uint32_t solo_asic_device::reg_ide_40001c_r()
{
	return m_ata->cs1_r(7);
}

void solo_asic_device::reg_ide_40001c_w(uint32_t data)
{
	m_ata->cs1_w(7, data);
}

TIMER_CALLBACK_MEMBER(solo_asic_device::dac_update)
{
	if (m_aud_dmacntl & AUD_DMACNTL_DMAEN)
	{
		if (m_aud_dma_ongoing)
		{
			address_space &space = m_hostcpu->space(AS_PROGRAM);

			int16_t samplel = space.read_word(m_aud_ccnt);
			m_aud_ccnt += 2;
			int16_t sampler = space.read_word(m_aud_ccnt);
			m_aud_ccnt += 2;

			// For 8-bit we're assuming left-aligned samples
			switch(m_aud_cconfig)
			{
				case AUD_CONFIG_16BIT_STEREO:
				default:
					m_dac[0]->write(samplel);
					m_dac[1]->write(sampler);
					break;

				case AUD_CONFIG_16BIT_MONO:
					m_dac[0]->write(samplel);
					m_dac[1]->write(samplel);
					break;

				case AUD_CONFIG_8BIT_STEREO:
					m_dac[0]->write((samplel >> 0x8) & 0xFF);
					m_dac[1]->write((sampler >> 0x8) & 0xFF);
					break;

				case AUD_CONFIG_8BIT_MONO:
					m_dac[0]->write((samplel >> 0x8) & 0xFF);
					m_dac[1]->write((samplel >> 0x8) & 0xFF);
					break;
			}
			if (m_aud_ccnt >= m_aud_cend)
			{
				solo_asic_device::set_audio_irq(BUS_INT_AUD_AUDDMAOUT, 1);
				m_aud_dma_ongoing = false; // nothing more to DMA
			}
		}
		if (!m_aud_dma_ongoing)
		{
			// wait for next DMA values to be marked as valid
			m_aud_dma_ongoing = m_aud_dmacntl & (AUD_DMACNTL_NV | AUD_DMACNTL_NVF);
			if (!m_aud_dma_ongoing) return; // values aren't marked as valid; don't prepare for next DMA
			m_aud_cstart = m_aud_nstart;
			m_aud_csize = m_aud_nsize;
			m_aud_cend = (m_aud_cstart + m_aud_csize);
			m_aud_cconfig = m_aud_nconfig;
			m_aud_ccnt = m_aud_cstart;
		}
	}
}

TIMER_CALLBACK_MEMBER(solo_asic_device::flush_modem_buffer)
{
	if (modem_txbuff_size > 0 && (m_modem_uart->ins8250_r(0x5) & INS8250_LSR_TSRE))
	{
		m_modem_uart->ins8250_w(0x0, modem_txbuff[modem_txbuff_index++ & (MBUFF_MAX_SIZE - 1)]);

		if (modem_txbuff_index == modem_txbuff_size)
		{
			modem_txbuff_index = 0x0;
			modem_txbuff_size = 0x0;
		}
	}

	if (modem_txbuff_size > 0)
		modem_buffer_timer->adjust(attotime::from_usec(MBUFF_FLUSH_TIME));
}

TIMER_CALLBACK_MEMBER(solo_asic_device::timer_irq)
{
	solo_asic_device::set_timer_irq(BUS_INT_TIM_SYSTIMER, 1);
}

// The interrupt handler gets copied into memory @ 0x80000200 to match up with the MIPS3 interrupt vector
void solo_asic_device::vblank_irq(int state) 
{
	solo_asic_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_VSYNCO, 1);
}

void solo_asic_device::irq_modem_w(int state) 
{
	solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE0, state);
}

void solo_asic_device::irq_ide_w(int state)
{
	solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE1, state);
}

void solo_asic_device::irq_keyboard_w(int state)
{
	solo_asic_device::set_dev_irq(BUS_INT_DEV_IRIN, state);
}

void solo_asic_device::set_audio_irq(uint8_t mask, int state)
{
	if (m_busaud_intenable & mask)
	{
		if (state)
		{
			m_busaud_intstat |= mask;

			solo_asic_device::set_bus_irq(BUS_INT_AUDIO, state);
		}
		else
		{
			m_busaud_intstat &= (~mask) & 0xff;

			if(m_busaud_intstat == 0x00)
			{
				solo_asic_device::set_bus_irq(BUS_INT_AUDIO, state);
			}
		}
	}
}

void solo_asic_device::set_dev_irq(uint8_t mask, int state)
{
	if (m_busdev_intenable & mask)
	{
		if (state)
		{
			m_busdev_intstat |= mask;

			solo_asic_device::set_bus_irq(BUS_INT_DEV, state);
		}
		else
		{
			m_busdev_intstat &= (~mask) & 0xff;

			if(m_busdev_intstat == 0x00)
			{
				solo_asic_device::set_bus_irq(BUS_INT_DEV, state);
			}
		}
	}
}

void solo_asic_device::set_rio_irq(uint8_t mask, int state)
{
	if (m_busrio_intenable & mask)
	{
		if (state)
		{
			m_busrio_intstat |= mask;

			solo_asic_device::set_bus_irq(BUS_INT_RIO, state);
		}
		else
		{
			m_busrio_intstat &= (~mask) & 0xff;

			if(m_busrio_intstat == 0x00)
			{
				solo_asic_device::set_bus_irq(BUS_INT_RIO, state);
			}
		}
	}
}

void solo_asic_device::set_video_irq(uint8_t mask, uint8_t sub_mask, int state)
{
	if (m_busvid_intenable & mask)
	{
		uint32_t sub_intstat = 0x00;

		switch (mask)
		{
			case BUS_INT_VID_DIVUNIT:
				if (m_div_intenable & sub_mask)
				{
					if (state)
						m_div_intstat |= sub_mask;
					else
						m_div_intstat &= (~sub_mask) & 0xff;
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_div_intstat;
				break;

			case BUS_INT_VID_GFXUNIT:
				if (m_gfx_intenable & sub_mask)
				{
					if (state)
						m_gfx_intstat |= sub_mask;
					else
						m_gfx_intstat &= (~sub_mask) & 0xff;
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_gfx_intstat;
				break;

			case BUS_INT_VID_POTUNIT:
				if (m_pot_intenable & sub_mask)
				{
					if (state)
						m_pot_intstat |= sub_mask;
					else
						m_pot_intstat &= (~sub_mask) & 0xff;
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_pot_intstat;
				break;

			case BUS_INT_VID_VIDUNIT:
				if (m_vid_intenable & sub_mask)
				{
					if (state)
						m_vid_intstat |= sub_mask;
					else
						m_vid_intstat &= (~sub_mask) & 0xff;
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_vid_intstat;
				break;
		}

		if (state)
		{
			m_busvid_intstat |= mask;

			solo_asic_device::set_bus_irq(BUS_INT_VIDEO, state);
		}
		else if(sub_intstat == 0x0)
		{
			m_busvid_intstat &= (~mask) & 0xff;

			if(m_busvid_intstat == 0x0)
			{
				solo_asic_device::set_bus_irq(BUS_INT_VIDEO, state);
			}
		}
	}
}

void solo_asic_device::set_timer_irq(uint8_t mask, int state)
{
	if (m_bus_intenable & BUS_INT_TIMER)
	{
		if (state)
		{
			m_bus_intstat |= BUS_INT_TIMER;
			m_bustim_intstat |= mask;

			m_hostcpu->set_input_line(MIPS3_IRQ5, ASSERT_LINE);
		}
		else
		{
			m_bustim_intstat &= (~mask) & 0xff;

			if(m_bustim_intstat == 0x00)
			{
				m_bus_intstat &= (~BUS_INT_TIMER) & 0xff;
				m_hostcpu->set_input_line(MIPS3_IRQ5, CLEAR_LINE);
			}
		}
	}
}

void solo_asic_device::set_bus_irq(uint8_t mask, int state)
{
	if (m_bus_intenable & mask)
	{
		if (state)
			m_bus_intstat |= mask;
		else
			m_bus_intstat &= (~mask) & 0xff;
		
		m_hostcpu->set_input_line(MIPS3_IRQ0, state ? ASSERT_LINE : CLEAR_LINE);
	}
 }

uint32_t solo_asic_device::gfxunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint16_t screen_width = bitmap.width();
	uint16_t screen_height = bitmap.height();
	uint8_t vid_step = (2 * VID_BYTES_PER_PIXEL);
	bool screen_enabled = (m_pot_cntl & POT_FCNTL_EN) && (m_gfx_cntl & GFX_FCNTL_EN);

	m_vid_cstart = m_vid_nstart;
	m_vid_csize = m_vid_nsize;
	m_vid_ccnt = m_gfx_wbdstart;

	// These values are quick hacks to get gfxUnit to draw properly.
	// Changes will come once we understand gfxUnit better
	uint32_t _m_pot_draw_hstart = 40;
	uint32_t _m_pot_draw_hsize = m_gfx_wbdlsize;
	uint32_t _m_pot_draw_vstart = m_pot_vstart;
	uint32_t _m_pot_draw_vsize = m_pot_vsize - 59;

	address_space &space = m_hostcpu->space(AS_PROGRAM);

	for (int y = 0; y < screen_height; y++)
	{
		uint32_t *line = &bitmap.pix(y);

		m_vid_cline = y;

		if (m_vid_cline == m_pot_draw_hintline)
			solo_asic_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_HSYNC, 1);

		for (int x = 0; x < screen_width; x += 2)
		{
			uint32_t pixel = POT_DEFAULT_COLOR;

			bool is_active_area = (
				y >= _m_pot_draw_vstart
				&& y < (_m_pot_draw_vstart + _m_pot_draw_vsize)

				&& x >= _m_pot_draw_hstart
				&& x < (_m_pot_draw_hstart + _m_pot_draw_hsize)
			);

			if (screen_enabled && is_active_area)
			{
				pixel = space.read_dword(m_vid_ccnt);

				m_vid_ccnt += vid_step;
			}
			else
			{
				pixel = m_pot_draw_blank_color;
			}

			int32_t y1 = ((pixel >> 0x18) & 0xff) - VID_Y_BLACK;
			int32_t Cb = ((pixel >> 0x10) & 0xff) - VID_UV_OFFSET;
			int32_t y2 = ((pixel >> 0x08) & 0xff) - VID_Y_BLACK;
			int32_t Cr = ((pixel) & 0xff) - VID_UV_OFFSET;

			y1 = (((y1 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);
			y2 = (((y2 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);

			int32_t r = ((0x166 * Cr) + VID_UV_OFFSET) >> 8;
			int32_t b = ((0x1C7 * Cb) + VID_UV_OFFSET) >> 8;
			int32_t g = ((0x32 * b) + (0x83 * r) + VID_UV_OFFSET) >> 8;

			*line++ = (
				std::clamp(y1 + r, 0x00, 0xff) << 0x10
				| std::clamp(y1 - g, 0x00, 0xff) << 0x08
				| std::clamp(y1 + b, 0x00, 0xff)
			);

			*line++ = (
				std::clamp(y2 + r, 0x00, 0xff) << 0x10
				| std::clamp(y2 - g, 0x00, 0xff) << 0x08
				| std::clamp(y2 + b, 0x00, 0xff)
			);
		}
	}

	solo_asic_device::set_video_irq(BUS_INT_VID_GFXUNIT, GFX_INT_RANGEINT_WBEOF, 1);

	return 0;
}

uint32_t solo_asic_device::vidunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint16_t screen_width = bitmap.width();
	uint16_t screen_height = bitmap.height();
	uint8_t vid_step = (2 * VID_BYTES_PER_PIXEL);
	bool screen_enabled = (m_pot_cntl & POT_FCNTL_EN) && (m_vid_dmacntl & VID_DMACNTL_DMAEN);

	m_vid_cstart = m_vid_nstart;
	m_vid_csize = m_vid_nsize;
	m_vid_ccnt = m_vid_draw_nstart;

	address_space &space = m_hostcpu->space(AS_PROGRAM);

	for (int y = 0; y < screen_height; y++)
	{
		uint32_t *line = &bitmap.pix(y);

		m_vid_cline = y;

		if (m_vid_cline == m_pot_draw_hintline)
			solo_asic_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_HSYNC, 1);

		for (int x = 0; x < screen_width; x += 2)
		{
			uint32_t pixel = POT_DEFAULT_COLOR;

			bool is_active_area = (
				y >= m_pot_draw_vstart
				&& y < (m_pot_draw_vstart + m_pot_draw_vsize)

				&& x >= m_pot_draw_hstart
				&& x < (m_pot_draw_hstart + m_pot_draw_hsize)
			);

			if (screen_enabled && is_active_area)
			{
				pixel = space.read_dword(m_vid_ccnt);

				m_vid_ccnt += vid_step;
			}
			else
			{
				pixel = m_pot_draw_blank_color;
			}

			int32_t y1 = ((pixel >> 0x18) & 0xff) - VID_Y_BLACK;
			int32_t Cb = ((pixel >> 0x10) & 0xff) - VID_UV_OFFSET;
			int32_t y2 = ((pixel >> 0x08) & 0xff) - VID_Y_BLACK;
			int32_t Cr = ((pixel) & 0xff) - VID_UV_OFFSET;

			y1 = (((y1 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);
			y2 = (((y2 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);

			int32_t r = ((0x166 * Cr) + VID_UV_OFFSET) >> 8;
			int32_t b = ((0x1C7 * Cb) + VID_UV_OFFSET) >> 8;
			int32_t g = ((0x32 * b) + (0x83 * r) + VID_UV_OFFSET) >> 8;

			*line++ = (
				std::clamp(y1 + r, 0x00, 0xff) << 0x10
				| std::clamp(y1 - g, 0x00, 0xff) << 0x08
				| std::clamp(y1 + b, 0x00, 0xff)
			);

			*line++ = (
				std::clamp(y2 + r, 0x00, 0xff) << 0x10
				| std::clamp(y2 - g, 0x00, 0xff) << 0x08
				| std::clamp(y2 + b, 0x00, 0xff)
			);
		}
	}

	solo_asic_device::set_video_irq(BUS_INT_VID_VIDUNIT, VID_INT_DMA, 1);

	return 0;
}

uint32_t solo_asic_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (m_pot_cntl & POT_FCNTL_USEGFX)
	{
		return gfxunit_screen_update(screen, bitmap, cliprect);
	}
	else
	{
		return vidunit_screen_update(screen, bitmap, cliprect);
	}
}
