// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#include "emu.h"

#include "machine/input_merger.h"
#include "render.h"
#include "solo_asic.h"
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

solo_asic_device::solo_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t chip_id, uint32_t sys_config, uint32_t aud_clock, bool softmodem_enabled)
	: device_t(mconfig, SOLO_ASIC, tag, owner, clock),
	device_serial_interface(mconfig, *this),
	m_hostcpu(*owner, "maincpu"),
	m_hostram(*owner, "mainram"),
	m_video(*this, "videox"),
	m_audio(*this, "audiox"),
	m_serial_id(*this, finder_base::DUMMY_TAG),
	m_irkbdc(*this, "irkbdc"),
    m_modem_uart(*this, "modem_uart"),
	m_softmodem_uart(*this, "smodem_uart"),
	m_debug_uart(*this, "debug"),
	m_watchdog(*this, "watchdog"),
    m_power_led(*this, "power_led"),
    m_connect_led(*this, "connect_led"),
    m_message_led(*this, "message_led"),
    m_ata(*this, finder_base::DUMMY_TAG),
	m_reset_hack_cb(*this),
	m_iic_sda_in_cb(*this, 0),
	m_iic_sda_out_cb(*this)
{
	m_chip_id = chip_id;
	m_sys_config = sys_config;
	m_aud_clock = aud_clock;
	m_softmodem_enabled = softmodem_enabled;
	m_hardmodem_enabled = !softmodem_enabled;
}

static DEVICE_INPUT_DEFAULTS_START(wtv_modem)
	DEVICE_INPUT_DEFAULTS("RS232_RXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_TXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_DATABITS", 0xff, RS232_DATABITS_8)
	DEVICE_INPUT_DEFAULTS("RS232_PARITY", 0xff, RS232_PARITY_NONE)
	DEVICE_INPUT_DEFAULTS("RS232_STOPBITS", 0xff, RS232_STOPBITS_1)
DEVICE_INPUT_DEFAULTS_END

void solo_asic_device::map(address_map &map)
{
	map(0x0000, 0x0fff).m(FUNC(solo_asic_device::bus_unit_map));
	map(0x1000, 0x1fff).m(FUNC(solo_asic_device::rom_unit_map));
	map(0x4000, 0x4fff).m(FUNC(solo_asic_device::dev_unit_map));
	map(0x5000, 0x5fff).m(FUNC(solo_asic_device::mem_unit_map));
	map(0xa000, 0xafff).m(FUNC(solo_asic_device::suc_unit_map));
	map(0xb000, 0xbfff).m(FUNC(solo_asic_device::mod_unit_map));
	map(0xc000, 0xcfff).m(FUNC(solo_asic_device::dma_unit_map));

	map(0x0000, 0xffff).m(m_audio, FUNC(solo_asic_audio_device::map));

	map(0x0000, 0xffff).m(m_video, FUNC(solo_asic_video_device::map));
}

void solo_asic_device::bus_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_0000_r));                                      // BUS_CHIPID
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_0004_r), FUNC(solo_asic_device::reg_0004_w)); // BUS_CHIPCNTL
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_0008_r), FUNC(solo_asic_device::reg_0008_w)); // BUS_INTSTAT, BUS_INTSTAT_S
	map(0x108, 0x10b).rw(FUNC(solo_asic_device::reg_0108_r), FUNC(solo_asic_device::reg_0108_w)); // BUS_INTSTAT, BUS_INTSTAT_C
	map(0x050, 0x053).r(FUNC(solo_asic_device::reg_0050_r));                                      // BUS_INTSTATRAW
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_000c_r), FUNC(solo_asic_device::reg_000c_w)); // BUS_INTEN
	map(0x10c, 0x10f).rw(FUNC(solo_asic_device::reg_010c_r), FUNC(solo_asic_device::reg_010c_w)); // BUS_INTEN, BUS_INTEN_C
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_0010_r), FUNC(solo_asic_device::reg_0010_w)); // BUS_ERRSTAT, BUS_ERRSTAT_S
	map(0x110, 0x113).rw(FUNC(solo_asic_device::reg_0110_r), FUNC(solo_asic_device::reg_0110_w)); // BUS_ERRSTAT, BUS_ERRSTAT_C
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_0014_r), FUNC(solo_asic_device::reg_0014_w)); // BUS_ERREN_S
	map(0x114, 0x117).rw(FUNC(solo_asic_device::reg_0114_r), FUNC(solo_asic_device::reg_0114_w)); // BUS_ERREN_C
	map(0x018, 0x01b).r(FUNC(solo_asic_device::reg_0018_r));                                      // BUS_ERRADDR
	map(0x030, 0x033).rw(FUNC(solo_asic_device::reg_0030_r), FUNC(solo_asic_device::reg_0030_w)); // BUS_WDVALUE
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
	map(0x158, 0x15b).rw(FUNC(solo_asic_device::reg_0158_r), FUNC(solo_asic_device::reg_0158_w)); // BUS_GPINTSTAT_C
	map(0x064, 0x067).rw(FUNC(solo_asic_device::reg_0064_r), FUNC(solo_asic_device::reg_0064_w)); // BUS_GPINTPOL
	map(0x070, 0x073).rw(FUNC(solo_asic_device::reg_0070_r), FUNC(solo_asic_device::reg_0070_w)); // BUS_AUDINTEN_S
	map(0x170, 0x173).rw(FUNC(solo_asic_device::reg_0170_r), FUNC(solo_asic_device::reg_0170_w)); // BUS_AUDINTEN_C
	map(0x068, 0x06b).r(FUNC(solo_asic_device::reg_0068_r));                                      // BUS_AUDINTSTAT
	map(0x06c, 0x06f).rw(FUNC(solo_asic_device::reg_006c_r), FUNC(solo_asic_device::reg_006c_w)); // BUS_AUDINTSTAT_S
	map(0x168, 0x16b).rw(FUNC(solo_asic_device::reg_0168_r), FUNC(solo_asic_device::reg_0168_w)); // BUS_AUDINTSTAT_C
	map(0x07c, 0x07f).rw(FUNC(solo_asic_device::reg_007c_r), FUNC(solo_asic_device::reg_007c_w)); // BUS_DEVINTEN_S
	map(0x17c, 0x17f).rw(FUNC(solo_asic_device::reg_017c_r), FUNC(solo_asic_device::reg_017c_w)); // BUS_DEVINTEN_C
	map(0x074, 0x077).r(FUNC(solo_asic_device::reg_0074_r));                                      // BUS_DEVINTSTAT
	map(0x078, 0x07b).rw(FUNC(solo_asic_device::reg_0078_r), FUNC(solo_asic_device::reg_0078_w)); // BUS_DEVINTSTAT_S
	map(0x174, 0x177).rw(FUNC(solo_asic_device::reg_0174_r), FUNC(solo_asic_device::reg_0174_w)); // BUS_DEVINTSTAT_C
	map(0x088, 0x08b).rw(FUNC(solo_asic_device::reg_0088_r), FUNC(solo_asic_device::reg_0088_w)); // BUS_VIDINTEN_S
	map(0x188, 0x18b).rw(FUNC(solo_asic_device::reg_0188_r), FUNC(solo_asic_device::reg_0188_w)); // BUS_VIDINTEN_C
	map(0x080, 0x083).r(FUNC(solo_asic_device::reg_0080_r));                                      // BUS_VIDINTSTAT
	map(0x084, 0x087).rw(FUNC(solo_asic_device::reg_0084_r), FUNC(solo_asic_device::reg_0084_w)); // BUS_VIDINTSTAT_S
	map(0x180, 0x183).rw(FUNC(solo_asic_device::reg_0180_r), FUNC(solo_asic_device::reg_0180_w)); // BUS_VIDINTSTAT_C
	map(0x098, 0x09b).rw(FUNC(solo_asic_device::reg_0098_r), FUNC(solo_asic_device::reg_0098_w)); // BUS_RIOINTEN_S
	map(0x198, 0x19b).rw(FUNC(solo_asic_device::reg_0198_r), FUNC(solo_asic_device::reg_0198_w)); // BUS_RIOINTEN_C
	map(0x08c, 0x08f).r(FUNC(solo_asic_device::reg_008c_r));                                      // BUS_RIOINTSTAT
	map(0x090, 0x093).rw(FUNC(solo_asic_device::reg_0090_r), FUNC(solo_asic_device::reg_0090_w)); // BUS_RIOINTSTAT_S
	map(0x18c, 0x18f).rw(FUNC(solo_asic_device::reg_018c_r), FUNC(solo_asic_device::reg_018c_w)); // BUS_RIOINTSTAT_C
	map(0x0a4, 0x0a7).rw(FUNC(solo_asic_device::reg_00a4_r), FUNC(solo_asic_device::reg_00a4_w)); // BUS_TIMINTEN_S
	map(0x1a4, 0x1a7).rw(FUNC(solo_asic_device::reg_01a4_r), FUNC(solo_asic_device::reg_01a4_w)); // BUS_TIMINTEN_C
	map(0x09c, 0x09f).r(FUNC(solo_asic_device::reg_009c_r));                                      // BUS_TIMINTSTAT
	map(0x0a0, 0x0a3).rw(FUNC(solo_asic_device::reg_00a0_r), FUNC(solo_asic_device::reg_00a0_w)); // BUS_TIMINTSTAT_S
	map(0x19c, 0x19f).rw(FUNC(solo_asic_device::reg_019c_r), FUNC(solo_asic_device::reg_019c_w)); // BUS_TIMINTSTAT_C
	map(0x0a8, 0x0ab).rw(FUNC(solo_asic_device::reg_00a8_r), FUNC(solo_asic_device::reg_00a8_w)); // BUS_RESETCAUSE
	map(0x0ac, 0x0af).w(FUNC(solo_asic_device::reg_00ac_w));                                      // BUS_RESETCAUSE_C
	map(0x0b0, 0x0b3).rw(FUNC(solo_asic_device::reg_00b0_r), FUNC(solo_asic_device::reg_00b0_w)); // BUS_J1FENLADDR
	map(0x0b4, 0x0b7).rw(FUNC(solo_asic_device::reg_00b4_r), FUNC(solo_asic_device::reg_00b4_w)); // BUS_J1FENHADDR
	map(0x0b8, 0x0bb).rw(FUNC(solo_asic_device::reg_00b8_r), FUNC(solo_asic_device::reg_00b8_w)); // BUS_J2FENLADDR
	map(0x0bc, 0x0bf).rw(FUNC(solo_asic_device::reg_00bc_r), FUNC(solo_asic_device::reg_00bc_w)); // BUS_J2FENHADDR
	map(0x0c8, 0x0cb).rw(FUNC(solo_asic_device::reg_00c8_r), FUNC(solo_asic_device::reg_00c8_w)); // BUS_BOOTMODE
	map(0x0cc, 0x0cf).rw(FUNC(solo_asic_device::reg_00cc_r), FUNC(solo_asic_device::reg_00cc_w)); // BUS_USEBOOTMODE
}

void solo_asic_device::rom_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_1000_r));                                      // ROM_SYSCONFIG
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_1004_r), FUNC(solo_asic_device::reg_1004_w)); // ROM_CNTL0
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_1008_r), FUNC(solo_asic_device::reg_1008_w)); // ROM_CNTL1
}

void solo_asic_device::dev_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_device::reg_4000_r));                                      // DEV_IROLD
	map(0x004, 0x007).rw(FUNC(solo_asic_device::reg_4004_r), FUNC(solo_asic_device::reg_4004_w)); // DEV_LED
	map(0x008, 0x00b).rw(FUNC(solo_asic_device::reg_4008_r), FUNC(solo_asic_device::reg_4008_w)); // DEV_IDCNTL
	map(0x00c, 0x00f).rw(FUNC(solo_asic_device::reg_400c_r), FUNC(solo_asic_device::reg_400c_w)); // DEV_NVCNTL
	map(0x010, 0x013).rw(FUNC(solo_asic_device::reg_4010_r), FUNC(solo_asic_device::reg_4010_w)); // DEV_GPIOIN
	map(0x014, 0x017).rw(FUNC(solo_asic_device::reg_4014_r), FUNC(solo_asic_device::reg_4014_w)); // DEV_GPIOOUT
	map(0x114, 0x117).rw(FUNC(solo_asic_device::reg_4114_r), FUNC(solo_asic_device::reg_4114_w)); // DEV_GPIOOUT_C
	map(0x018, 0x01b).rw(FUNC(solo_asic_device::reg_4018_r), FUNC(solo_asic_device::reg_4018_w)); // DEV_GPIOEN
	map(0x118, 0x11b).rw(FUNC(solo_asic_device::reg_4118_r), FUNC(solo_asic_device::reg_4118_w)); // DEV_GPIOEN_C
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

void solo_asic_device::suc_unit_map(address_map &map)
{
	map(0x000, 0x003).w(FUNC(solo_asic_device::reg_a000_w)); // SUCGPU_TFFHR
	map(0x00c, 0x00f).r(FUNC(solo_asic_device::reg_a00c_r)); // SUCGPU_TFFCNT
	map(0x010, 0x013).r(FUNC(solo_asic_device::reg_a010_r)); // SUCGPU_TFFMAX
	map(0x040, 0x043).r(FUNC(solo_asic_device::reg_a040_r)); // SUCGPU_RFFHR
	map(0x04c, 0x04f).r(FUNC(solo_asic_device::reg_a04c_r)); // SUCGPU_RFFCNT
	map(0x050, 0x053).r(FUNC(solo_asic_device::reg_a050_r)); // SUCGPU_RFFMAX
	map(0xab8, 0xabb).r(FUNC(solo_asic_device::reg_aab8_r)); // SUCSC0_GPIOVAL
}

void solo_asic_device::mod_unit_map(address_map &map)
{
}

void solo_asic_device::dma_unit_map(address_map &map)
{
	map(0x020, 0x023).rw(FUNC(solo_asic_device::reg_c020_r), FUNC(solo_asic_device::reg_c020_w));
	map(0x024, 0x027).rw(FUNC(solo_asic_device::reg_c024_r), FUNC(solo_asic_device::reg_c024_w));
	map(0x028, 0x02b).rw(FUNC(solo_asic_device::reg_c028_r), FUNC(solo_asic_device::reg_c028_w));
	map(0x02c, 0x02f).rw(FUNC(solo_asic_device::reg_c02c_r), FUNC(solo_asic_device::reg_c02c_w));
	map(0x040, 0x043).rw(FUNC(solo_asic_device::reg_c040_r), FUNC(solo_asic_device::reg_c040_w));
	map(0x100, 0x103).rw(FUNC(solo_asic_device::reg_c100_r), FUNC(solo_asic_device::reg_c100_w));
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
	SOLO_ASIC_VIDEO(config, m_video);
	m_video->int_enable_callback().set(FUNC(solo_asic_device::int_enable_vid_w));
	m_video->int_irq_callback().set(FUNC(solo_asic_device::irq_vid_w));
	m_video->set_hostcpu(m_hostcpu);
	m_video->set_hostram(m_hostram);
	if (m_sys_config & SYSCONFIG_PAL)
		m_video->enable_pal();
	else
		m_video->enable_ntsc();

	if (m_softmodem_enabled)
	{
		WTVSOFTMODEM(config, m_softmodem_uart);
		m_softmodem_uart->out_tx_callback().set("modem", FUNC(rs232_port_device::write_txd));
		m_softmodem_uart->out_dtr_callback().set("modem", FUNC(rs232_port_device::write_dtr));
		m_softmodem_uart->out_rts_callback().set("modem", FUNC(rs232_port_device::write_rts));

		rs232_port_device &rs232(RS232_PORT(config, "modem", default_rs232_devices, "null_modem"));
		rs232.set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(wtv_modem));
		rs232.rxd_handler().set(m_softmodem_uart, FUNC(wtvsoftmodem_device::rx_w));
		rs232.dcd_handler().set(m_softmodem_uart, FUNC(wtvsoftmodem_device::dcd_w));
		rs232.dsr_handler().set(m_softmodem_uart, FUNC(wtvsoftmodem_device::dsr_w));
		rs232.ri_handler().set(m_softmodem_uart, FUNC(wtvsoftmodem_device::ri_w));
		rs232.cts_handler().set(m_softmodem_uart, FUNC(wtvsoftmodem_device::cts_w));
	}
	else
	{
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
	}

	SOLO_ASIC_AUDIO(config, m_audio, 0, m_softmodem_enabled);
	m_audio->int_enable_callback().set(FUNC(solo_asic_device::int_enable_aud_w));
	m_audio->int_irq_callback().set(FUNC(solo_asic_device::irq_aud_w));
	m_audio->set_hostcpu(m_hostcpu);
	m_audio->set_hostram(m_hostram);
	if (m_softmodem_enabled)
		m_audio->set_softmodem(m_softmodem_uart);

	SEJIN_KBD(config, m_irkbdc);
	m_irkbdc->sample_fifo_trigger_callback().set(FUNC(solo_asic_device::irq_keyboard_w));

	WTV_RS232DBG(config, m_debug_uart);
	m_debug_uart->serial_rx_handler().set(FUNC(solo_asic_device::irq_uart_w));

	WATCHDOG_TIMER(config, m_watchdog);
	solo_asic_device::watchdog_enable(0);
}

void solo_asic_device::device_start()
{
	m_power_led.resolve();
	m_connect_led.resolve();
	m_message_led.resolve();

	m_resetcause = 0x0;
	m_bootmode = 0x0;
	m_use_bootmode = 0x0;

	modem_buffer_timer = timer_alloc(FUNC(solo_asic_device::flush_modem_buffer), this);
	compare_timer = timer_alloc(FUNC(solo_asic_device::timer_irq), this);

	solo_asic_device::device_reset();

	save_item(NAME(m_bus_intenable));
	save_item(NAME(m_bus_intstat));
	save_item(NAME(m_tcompare));
	save_item(NAME(m_busgpio_intenable));
	save_item(NAME(m_busgpio_intstat));
	save_item(NAME(m_busgpio_intpol));
	save_item(NAME(m_busdev_intenable));
	save_item(NAME(m_busdev_intstat));
	save_item(NAME(m_busrio_intenable));
	save_item(NAME(m_busrio_intstat));
	save_item(NAME(m_bustim_intenable));
	save_item(NAME(m_bustim_intstat));
	save_item(NAME(m_errenable));
	save_item(NAME(m_chpcntl));
	save_item(NAME(m_wdenable));
	save_item(NAME(m_iiccntl));
	save_item(NAME(m_iic_sda));
	save_item(NAME(m_iic_scl));
	save_item(NAME(m_errstat));
	save_item(NAME(m_j1fenladdr));
	save_item(NAME(m_j1fenhaddr));
	save_item(NAME(m_j2fenladdr));
	save_item(NAME(m_j2fenhaddr));
	save_item(NAME(m_resetcause));
	save_item(NAME(m_bootmode));
	save_item(NAME(m_use_bootmode));

	save_item(NAME(m_smrtcrd_serial_bitmask));
	save_item(NAME(m_smrtcrd_serial_rxdata));
	save_item(NAME(m_utvdma_src));
	save_item(NAME(m_utvdma_dst));
	save_item(NAME(m_utvdma_size));
	save_item(NAME(m_utvdma_mode));
	save_item(NAME(m_utvdma_cntl));
	save_item(NAME(m_utvdma_locked));
	save_item(NAME(m_utvdma_started));
	save_item(NAME(m_utvdma_ccnt));
	save_item(NAME(m_ide1_dmarq_state));
	save_item(NAME(m_rom_cntl0));
	save_item(NAME(m_rom_cntl1));
	save_item(NAME(m_ledstate));
	save_item(NAME(m_dev_idcntl));
	save_item(NAME(m_dev_id_state));
	save_item(NAME(m_dev_id_bit));
	save_item(NAME(m_dev_id_bitidx));
	save_item(NAME(m_dev_gpio_in));
	save_item(NAME(m_dev_gpio_out));
	save_item(NAME(m_dev_gpio_in_mask));
	save_item(NAME(m_dev_gpio_out_mask));
}

void solo_asic_device::device_reset()
{
	if (m_use_bootmode == 0x1)
	{
		if ((m_bootmode & BOOTMODE_BIG_ENDIAN) != 0x0)
		{
			m_hostcpu->set_endianness(ENDIANNESS_BIG);
		}
		else
		{
			m_hostcpu->set_endianness(ENDIANNESS_LITTLE);
		}
	}

	m_memcntl = 0b11;
	m_memrefcnt = 0x0400;
	m_memdata = 0x0;
	m_memcmd = 0x0;
	m_memtiming = 0xadbadffa;
	m_errenable = 0x0;
	m_chpcntl = 0x0;
	m_wdenable = 0x0;
	m_errstat = 0x0;
	m_iiccntl = 0x0;
	m_iic_sda = 0x0;
	m_iic_scl = 0x0;
	m_fence1_addr = 0x0;
	m_fence1_mask = 0x0;
	m_fence2_addr = 0x0;
	m_fence2_mask = 0x0;
	m_j1fenladdr = 0x0;
	m_j1fenhaddr = 0x0;
	m_j2fenladdr = 0x0;
	m_j2fenhaddr = 0x0;
	m_tcompare = 0x0;
	m_bus_intenable = 0x0;
	m_bus_intstat = 0x0;
	m_busgpio_intenable = 0x0;
	m_busgpio_intstat = 0x0;
	m_busgpio_intpol = 0x0;
	m_busdev_intenable = 0x0;
	m_busdev_intstat = 0x0;
	m_busrio_intenable = 0x0;
	m_busrio_intstat = 0x0;
	m_bustim_intenable = 0x0;
	m_bustim_intstat = 0x0;
	
	m_rom_cntl0 = 0x0;
	m_rom_cntl1 = 0x0;

	m_ledstate = 0xFFFFFFFF;
	m_power_led = 0;
	m_connect_led = 0;
	m_message_led = 0;

	m_dev_idcntl = 0x00;
	m_dev_id_state = SSID_STATE_IDLE;
	m_dev_id_bit = 0x0;
	m_dev_id_bitidx = 0x0;
	m_dev_gpio_in = 0x0;
	m_dev_gpio_out = 0x0;
	m_dev_gpio_in_mask = 0x0;
	m_dev_gpio_out_mask = (~m_dev_gpio_in_mask);

	m_smrtcrd_serial_bitmask = 0x0;
	m_smrtcrd_serial_rxdata = 0x0;

	m_utvdma_src = 0x0;
	m_utvdma_dst = 0x0;
	m_utvdma_size = 0x0;
	m_utvdma_mode = 0x0;
	m_utvdma_cntl = 0x0;
	m_utvdma_locked = 0x0;
	m_utvdma_started = 0x0;
	m_utvdma_ccnt = 0x0;
	m_ide1_dmarq_state = 0x0;

	modem_txbuff_size = 0x0;
	modem_txbuff_index = 0x0;
	modem_should_threint = false;
	solo_asic_device::modfw_hack_begin();

	if (device().started())
	{
		solo_asic_device::watchdog_enable(m_wdenable);
		m_irkbdc->enable(1);
	}

	// Assume it's a watchdog reset if the reset wasn't commanded using the BUS_RESETCAUSE register
	if (m_resetcause != RESETCAUSE_SOFTWARE)
	{
		m_resetcause = RESETCAUSE_WATCHDOG;
	}
}

void solo_asic_device::device_stop()
{
	//
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

bool solo_asic_device::is_webtvos()
{
	// The WebTV OS always run in big endian mode while Windows CE runs in little endian.
	// This allows us to easily get around quirks specific to the OS.
	return (m_hostcpu->get_endianness() == ENDIANNESS_BIG);
}

void solo_asic_device::modfw_hack_begin()
{
	modfw_mode = true;

	modfw_message_index = 0x0;
	modfw_will_flush = false;
	modfw_will_ack = false;
	modfw_enable_index = 0x0;
	modfw_reset_index = 0x0;
}

void solo_asic_device::modfw_hack_end()
{
	modfw_mode = false;

	if (m_hardmodem_enabled && !solo_asic_device::is_webtvos())
	{
		// DLAB enabled, 8 data bits
		m_modem_uart->ins8250_w(0x3, 0x83);
		// Set 115200 baud (through DLAB DLL)
		m_modem_uart->ins8250_w(0x0, 0x01);
		// DLAB disabled, 8 data bits
		m_modem_uart->ins8250_w(0x3, 0x03);
	}
}

void solo_asic_device::mod_reset()
{
	modfw_reset_index = 0x0;
}

uint32_t solo_asic_device::reg_0000_r()
{
	return m_chip_id;
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
		// On hardware this sets the AUD_XTAL audio clock divider. AUD_XTAL is typically based on the system bus clock.

		//uint32_t requested_audclk_div = ((data & CHPCNTL_AUDCLKDIV_MASK) >> CHPCNTL_AUDCLKDIV_SHIFT) * 0x100;
		//m_audio->set_aout_clock(solo_asic_device::clock() / requested_audclk_div);

		// The OS/fimrware tries to find the closest divider to create a 44100Hz or 48000Hz audio clock based
		// on the calculated system bus clock.
		// 
		// This implementation always sets 44100Hz exactly to cover most cases. This doesn't behave exactly like 
		// hardware but is better optimized for audio quality. There's some tradeoffs with this but I fell this 
		// is better for our use case.

		m_audio->set_aout_clock(m_aud_clock);
	}

	m_chpcntl = data;
}

uint32_t solo_asic_device::reg_0008_r()
{
	if (do7e_hack && m_hardmodem_enabled)
	{
		do7e_hack = false;

		uint8_t iir_data = m_modem_uart->ins8250_r(0x2);

		if ((iir_data & 0x1) != 0x0)
		{
			// Re-try with an another interrupt if we're not done with interrupt handling.
			// We can't continue with broken data (do7e_hack)
			solo_asic_device::irq_modem_w(1);
		}

		// This register is the IIR modem register in this state.
		return 0x1; // after we broke out of the loop from reg_0014_r, this stops the modem int train
	}
	else
	{
		return m_bus_intstat;
	}
}

void solo_asic_device::reg_0008_w(uint32_t data)
{
	m_bus_intstat |= data;
}

uint32_t solo_asic_device::reg_0108_r()
{
	return m_bus_intstat;
}

void solo_asic_device::reg_0108_w(uint32_t data)
{
	solo_asic_device::set_bus_irq(data, CLEAR_LINE);
}

uint32_t solo_asic_device::reg_0050_r()
{
	// This will only be set if interrupts are enabled rather than be set reguardless of enable bits.
	// Possible future implementation is to set this when we want to interrupt then mask based on enable bits for reg_0008_r.
	return m_bus_intstat;
}

uint32_t solo_asic_device::reg_000c_r()
{
	return m_bus_intenable;
}

void solo_asic_device::reg_000c_w(uint32_t data)
{
	m_bus_intenable |= data;
}

uint32_t solo_asic_device::reg_010c_r()
{
	return m_bus_intenable;
}

void solo_asic_device::reg_010c_w(uint32_t data)
{
	solo_asic_device::reg_0108_w(data);
	m_bus_intenable &= (~data);
}

uint32_t solo_asic_device::reg_0010_r()
{
	return m_errstat;
}

void solo_asic_device::reg_0010_w(uint32_t data)
{
	m_errstat |= data & 0xff;
}

uint32_t solo_asic_device::reg_0110_r()
{
	return m_errstat;
}

void solo_asic_device::reg_0110_w(uint32_t data)
{
	m_errstat &= (~data) & 0xFF;
}

uint32_t solo_asic_device::reg_0014_r()
{
	if (do7e_hack)
		// This register is the LSR modem register in this state.
		return 0x0; // This causes the firmware to break out of the handler loop
	else
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
	if(data == ERR_LOWWRITE || data == 0xffff)
	{
		// Turn back on the modem downloader hack.
		solo_asic_device::modfw_hack_begin();

		solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE0, CLEAR_LINE);

		if (m_j2fenladdr == 0x0)
			m_reset_hack_cb(1);
	}

	m_errenable &= (~data) & 0xFF;
}

uint32_t solo_asic_device::reg_0018_r()
{
	return 0x00000000;
}

uint32_t solo_asic_device::reg_0030_r()
{
	return m_wdvalue;
}

void solo_asic_device::reg_0030_w(uint32_t data)
{
	m_wdvalue = data; // value is ignored
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
	m_tcompare = data;

	if (m_tcompare != 0x0)
	{
		m_bustim_intenable |= BUS_INT_TIM_SYSTIMER;
		m_bus_intenable |= BUS_INT_TIMER;

		// There seems to be an issue using the timer compare value. Hardcoding this to around 0.05s.
		compare_timer->adjust(attotime::from_usec(TCOMPARE_TIMER_USEC));
		//compare_timer->adjust(m_hostcpu->cycles_to_attotime(((m_tcompare - m_hostcpu->total_cycles()) * 2)));
	}
	else
	{
		solo_asic_device::set_timer_irq(BUS_INT_TIM_SYSTIMER, CLEAR_LINE);
	}
}

uint32_t solo_asic_device::reg_005c_r()
{
	return m_busgpio_intenable;
}

void solo_asic_device::reg_005c_w(uint32_t data)
{
	m_busgpio_intenable |= data;

	if (m_busgpio_intenable != 0x0)
		solo_asic_device::reg_007c_w(BUS_INT_DEV_GPIO);
}

uint32_t solo_asic_device::reg_015c_r()
{
	return m_busgpio_intenable;
}

void solo_asic_device::reg_015c_w(uint32_t data)
{
	solo_asic_device::reg_0158_w(data);
	m_busgpio_intenable &= (~data);

	if (m_busgpio_intenable == 0x0)
		solo_asic_device::reg_017c_w(BUS_INT_DEV_GPIO);
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
	m_busgpio_intstat |= data;
}

uint32_t solo_asic_device::reg_0158_r()
{
	return m_busgpio_intstat;
}

void solo_asic_device::reg_0158_w(uint32_t data)
{
	solo_asic_device::set_gpio_irq(data, CLEAR_LINE);
}

uint32_t solo_asic_device::reg_0064_r()
{
	return m_busgpio_intpol;
}

void solo_asic_device::reg_0064_w(uint32_t data)
{
	m_busgpio_intpol = data;
}

uint32_t solo_asic_device::reg_0070_r()
{
	return m_audio->busaud_intenable_get();
}

void solo_asic_device::reg_0070_w(uint32_t data)
{
	m_audio->busaud_intenable_set(data);
}

uint32_t solo_asic_device::reg_0170_r()
{
	return m_audio->busaud_intenable_get();
}

void solo_asic_device::reg_0170_w(uint32_t data)
{
	solo_asic_device::reg_0168_w(data);
	m_audio->busaud_intenable_clear(data);
}

uint32_t solo_asic_device::reg_0068_r()
{
	return m_audio->busaud_intstat_get();
}

uint32_t solo_asic_device::reg_006c_r()
{
	return m_audio->busaud_intstat_get();
}

void solo_asic_device::reg_006c_w(uint32_t data)
{
	m_audio->busaud_intstat_set(data);
}

uint32_t solo_asic_device::reg_0168_r()
{
	return m_audio->busaud_intstat_get();
}

void solo_asic_device::reg_0168_w(uint32_t data)
{
	return m_audio->busaud_intstat_clear(data);
}

uint32_t solo_asic_device::reg_007c_r()
{
	return m_busdev_intenable;
}

void solo_asic_device::reg_007c_w(uint32_t data)
{
	m_busdev_intenable |= data;
	if (m_busdev_intenable != 0x0)
		m_bus_intenable |= BUS_INT_DEV;
}

uint32_t solo_asic_device::reg_017c_r()
{
	return m_busdev_intenable;
}

void solo_asic_device::reg_017c_w(uint32_t data)
{
	solo_asic_device::reg_0174_w(data);
	m_busdev_intenable &= (~data);
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
	m_busdev_intstat |= data;
}

uint32_t solo_asic_device::reg_0174_r()
{
	return m_busdev_intstat;
}

void solo_asic_device::reg_0174_w(uint32_t data)
{
	solo_asic_device::set_dev_irq(data, CLEAR_LINE);
}

uint32_t solo_asic_device::reg_0088_r()
{
	return m_video->busvid_intenable_get();
}

void solo_asic_device::reg_0088_w(uint32_t data)
{
	m_video->busvid_intenable_set(data);
}

uint32_t solo_asic_device::reg_0188_r()
{
	return m_video->busvid_intenable_get();
}

void solo_asic_device::reg_0188_w(uint32_t data)
{
	solo_asic_device::reg_0180_w(data);
	m_video->busvid_intenable_clear(data);
}

uint32_t solo_asic_device::reg_0080_r()
{
	return m_video->busvid_intstat_get();
}

uint32_t solo_asic_device::reg_0084_r()
{
	return m_video->busvid_intstat_get();
}

void solo_asic_device::reg_0084_w(uint32_t data)
{
	m_video->busvid_intstat_set(data);
}

uint32_t solo_asic_device::reg_0180_r()
{
	return m_video->busvid_intstat_get();
}

void solo_asic_device::reg_0180_w(uint32_t data)
{
	m_video->busvid_intstat_clear(data);
}

uint32_t solo_asic_device::reg_0098_r()
{
	return m_busrio_intenable;
}

void solo_asic_device::reg_0098_w(uint32_t data)
{
	m_busrio_intenable |= data;
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
	// Windows CE builds
	if (!solo_asic_device::is_webtvos())
	{
		solo_asic_device::set_rio_irq(data, CLEAR_LINE);
		m_busrio_intenable &= (~data);
	}
	// WebTV OS builds the modem timinng is incorrect, so ignore the ROM trying to disable the modem interrupt.
	else if ((data & BUS_INT_RIO_DEVICE0) == 0x00)
	{
		solo_asic_device::reg_018c_w(data);
		m_busrio_intenable &= (~data);
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
	m_busrio_intstat |= data;
}

uint32_t solo_asic_device::reg_018c_r()
{
	return m_busrio_intstat;
}

void solo_asic_device::reg_018c_w(uint32_t data)
{
	solo_asic_device::set_rio_irq(data, CLEAR_LINE);

	// Re-assert RIO interrupt if the modem UART device still hasn't cleared the interrupt pending state.
	if (m_hardmodem_enabled && !modfw_mode && ((solo_asic_device::is_webtvos() && m_modem_uart->intrpt_r()) || solo_asic_device::get_wince_intrpt_r()))
	{
		solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE0, ASSERT_LINE);
	}
}

uint32_t solo_asic_device::reg_00a4_r()
{
	return m_bustim_intenable;
}

void solo_asic_device::reg_00a4_w(uint32_t data)
{
	m_bustim_intenable |= data;
	if (m_bustim_intenable != 0x0)
	{
		m_bus_intenable |= BUS_INT_TIMER;
	}
}

uint32_t solo_asic_device::reg_01a4_r()
{
	return m_bustim_intenable;
}

void solo_asic_device::reg_01a4_w(uint32_t data)
{
	solo_asic_device::reg_019c_w(data);
	m_bustim_intenable &= (~data);
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
	m_bustim_intstat |= data;
}

uint32_t solo_asic_device::reg_019c_r()
{
	return m_bustim_intstat;
}

void solo_asic_device::reg_019c_w(uint32_t data)
{
	solo_asic_device::set_timer_irq(data, CLEAR_LINE);
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

uint32_t solo_asic_device::reg_00b0_r()
{
	return m_j1fenladdr;
}

void solo_asic_device::reg_00b0_w(uint32_t data)
{
	m_j1fenladdr = data;
}

uint32_t solo_asic_device::reg_00b4_r()
{
	return m_j1fenhaddr;
}

void solo_asic_device::reg_00b4_w(uint32_t data)
{
	m_j1fenhaddr = data;
}

uint32_t solo_asic_device::reg_00b8_r()
{
	return m_j2fenladdr;
}

void solo_asic_device::reg_00b8_w(uint32_t data)
{
	m_j2fenladdr = data;
}

uint32_t solo_asic_device::reg_00bc_r()
{
	return m_j2fenhaddr;
}

void solo_asic_device::reg_00bc_w(uint32_t data)
{
	m_j2fenhaddr = data;
}

uint32_t solo_asic_device::reg_00c8_r()
{
	return m_bootmode;
}

void solo_asic_device::reg_00c8_w(uint32_t data)
{
	m_bootmode = data;
}

uint32_t solo_asic_device::reg_00cc_r()
{
	return m_use_bootmode;
}

void solo_asic_device::reg_00cc_w(uint32_t data)
{
	m_use_bootmode = data;
}

uint32_t solo_asic_device::reg_1000_r()
{
	return m_sys_config;
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


// Read IR receiver chip
uint32_t solo_asic_device::reg_4000_r()
{
	return m_irkbdc->data_r(wtvir_sejin_device::DEV_IROLD);
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
	m_dev_id_bit = 0x0;

	if (m_dev_id_state == SSID_STATE_PRESENCE)
	{
		m_dev_id_bit = 0x0; // We're present.
		m_dev_id_state = SSID_STATE_COMMAND; // This normally would stay in presence mode for 480us then command, but we immediatly go into command mode.
		m_dev_id_bitidx = 0x0;
	}
	else if (m_dev_id_state == SSID_STATE_READROM_PULSEEND)
	{
		m_dev_id_state = SSID_STATE_READROM_BIT;
	}
	else if (m_dev_id_state == SSID_STATE_READROM_BIT)
	{
		m_dev_id_state = SSID_STATE_READROM; // Go back into the read ROM pulse state

		m_dev_id_bit = m_serial_id->direct_read(m_dev_id_bitidx / 8) >> (m_dev_id_bitidx & 0x7);

		m_dev_id_bitidx++;
		if (m_dev_id_bitidx == 64)
		{
			// We've read the entire SSID. Go back into idle.
			m_dev_id_state = SSID_STATE_IDLE;
			m_dev_id_bitidx = 0x0;
		}
	}

	return m_dev_idcntl | (m_dev_id_bit & 1);
}

void solo_asic_device::reg_4008_w(uint32_t data)
{
	m_dev_idcntl = (data & 0x2);

	if (m_dev_idcntl & 0x2)
	{
		switch(m_dev_id_state) // States for high
		{
			case SSID_STATE_RESET: // End reset low pulse to go into prescense mode. Chip should read low to indicate presence.
				m_dev_id_state = SSID_STATE_PRESENCE; // This pulse normally lasts 480us before going into command mode.
				break;

			case SSID_STATE_COMMAND: // Ended a command bit pulse. Increment bit index. We always assume a read from ROM command after we get 8 bits.
				m_dev_id_bitidx++;

				if (m_dev_id_bitidx == 8)
				{
					m_dev_id_state = SSID_STATE_READROM; // Now we can read back the SSID. ROM reads it as two 32-bit integers.
					m_dev_id_bitidx = 0;
				}
				break;

			case SSID_STATE_READROM_PULSESTART:
				m_dev_id_state = SSID_STATE_READROM_PULSEEND;
		}
	}
	else
	{
		switch(m_dev_id_state) // States for low
		{
			case SSID_STATE_IDLE: // When idle, we can drive the chip low for reset
				m_dev_id_state = SSID_STATE_RESET; // We'd normally leave this for 480us to go into presence mode.
				break;

			case SSID_STATE_READROM:
				m_dev_id_state = SSID_STATE_READROM_PULSESTART;
				break;
		}
	}
}

uint32_t solo_asic_device::reg_400c_r()
{
	m_iic_sda = m_iic_sda_in_cb();

	return (m_iiccntl & 0xE) | m_iic_sda;
}

void solo_asic_device::reg_400c_w(uint32_t data)
{
	m_iic_scl = ((data & NVCNTL_SCL) == NVCNTL_SCL) & 1;

	if (data & NVCNTL_WRITE_EN)
		m_iic_sda = ((data & NVCNTL_SDA_W) == NVCNTL_SDA_W) & 0x1;
	else
		m_iic_sda = 0x1;

	m_iiccntl = data & 0xE;

	m_iic_sda_out_cb(m_iic_sda);
}

uint32_t solo_asic_device::reg_4010_r()
{
	uint32_t gpio_data = (m_dev_gpio_in & m_dev_gpio_in_mask);
	gpio_data |= (m_dev_gpio_out & m_dev_gpio_out_mask);

	return gpio_data;
}

void solo_asic_device::reg_4010_w(uint32_t data)
{
	// Can't set GPIOIN pins, use registers 0x4014 and 0x4114 instead.
}

uint32_t solo_asic_device::reg_4014_r()
{
	return (m_dev_gpio_out & m_dev_gpio_out_mask);
}

void solo_asic_device::reg_4014_w(uint32_t data)
{
	data &= m_dev_gpio_out_mask;

	if (m_softmodem_enabled)
	{
		if (data & GPIO_SOFTMODEM_HOOK_STATE)
			m_softmodem_uart->set_off_hook();
		else if (data & GPIO_SOFTMODEM_RESET)
			m_softmodem_uart->restart();
		else if (data & GPIO_SOFTMODEM_LINE_CHECK)
			solo_asic_device::set_gpio_irq(GPIO_SOFTMODEM_HAS_LINE_VOLTAGE, ASSERT_LINE);
	}

	m_dev_gpio_out |= data;
}

uint32_t solo_asic_device::reg_4114_r()
{
	return (m_dev_gpio_out & m_dev_gpio_out_mask);
}

void solo_asic_device::reg_4114_w(uint32_t data)
{
	data &= m_dev_gpio_out_mask;

	if (m_softmodem_enabled)
	{
		if (data & GPIO_SOFTMODEM_HOOK_STATE)
			m_softmodem_uart->set_on_hook();
		// NOTE: currently there is a bug where the UTV doesn't reset properly after the first dial. Need to look into this.
		else if (data & GPIO_SOFTMODEM_LINE_CHECK)
			solo_asic_device::set_gpio_irq(GPIO_SOFTMODEM_HAS_LINE_VOLTAGE, ASSERT_LINE);
	}

	m_dev_gpio_out &= (~data);
}

uint32_t solo_asic_device::reg_4018_r()
{
	return m_dev_gpio_in_mask;
}

void solo_asic_device::reg_4018_w(uint32_t data)
{
	m_dev_gpio_out_mask |= data;
	m_dev_gpio_in_mask = (~m_dev_gpio_out_mask);
}

uint32_t solo_asic_device::reg_4118_r()
{
	return m_dev_gpio_in_mask;
}

void solo_asic_device::reg_4118_w(uint32_t data)
{
	m_dev_gpio_out_mask &= (~data);
	m_dev_gpio_in_mask = (~m_dev_gpio_out_mask);
}

uint32_t solo_asic_device::reg_4020_r()
{
	return m_irkbdc->data_r(wtvir_sejin_device::DEV_IRIN_SAMPLE);
}

void solo_asic_device::reg_4020_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_sejin_device::DEV_IRIN_SAMPLE, data);
}

uint32_t solo_asic_device::reg_4024_r()
{
	return m_irkbdc->data_r(wtvir_sejin_device::DEV_IRIN_REJECT_INT);
}

void solo_asic_device::reg_4024_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_sejin_device::DEV_IRIN_REJECT_INT, data);
}

uint32_t solo_asic_device::reg_4028_r()
{
	return m_irkbdc->data_r(wtvir_sejin_device::DEV_IRIN_TRANS_DATA);
}

uint32_t solo_asic_device::reg_402c_r()
{
	return m_irkbdc->data_r(wtvir_sejin_device::DEV_IRIN_STATCNTL);
}

void solo_asic_device::reg_402c_w(uint32_t data)
{
	m_irkbdc->data_w(wtvir_sejin_device::DEV_IRIN_STATCNTL, data);
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

// sucUnit registers

void solo_asic_device::reg_a000_w(uint32_t data)
{
	m_debug_uart->serial_tx_byte_w(data & 0xff);
}

uint32_t solo_asic_device::reg_a00c_r()
{
	return m_debug_uart->serial_rx_buffcnt_r();
}

uint32_t solo_asic_device::reg_a010_r()
{
	return m_debug_uart->serial_tx_buffmax_r();
}

uint32_t solo_asic_device::reg_a040_r()
{
	return m_debug_uart->serial_rx_byte_r();
}

uint32_t solo_asic_device::reg_a04c_r()
{
	return m_debug_uart->serial_rx_buffcnt_r();
}

uint32_t solo_asic_device::reg_a050_r()
{
	return m_debug_uart->serial_rx_buffmax_r();
}

uint32_t solo_asic_device::reg_aab8_r()
{
	return (0x1 << 4);
}

// UTV dmaUnit

uint32_t solo_asic_device::reg_c020_r()
{
	return m_utvdma_src;
}

void solo_asic_device::reg_c020_w(uint32_t data)
{
	if (m_utvdma_locked)
		m_utvdma_src = data;
}

uint32_t solo_asic_device::reg_c024_r()
{
	return m_utvdma_dst;
}

void solo_asic_device::reg_c024_w(uint32_t data)
{
	if (m_utvdma_locked)
		m_utvdma_dst = data;
}

uint32_t solo_asic_device::reg_c028_r()
{
	return m_utvdma_size;
}

void solo_asic_device::reg_c028_w(uint32_t data)
{
	if (m_utvdma_locked)
		m_utvdma_size = data;
}

uint32_t solo_asic_device::reg_c02c_r()
{
	return m_utvdma_mode;
}

void solo_asic_device::reg_c02c_w(uint32_t data)
{
	if (m_utvdma_locked)
		m_utvdma_mode = data;
}

uint32_t solo_asic_device::reg_c040_r()
{
	return m_utvdma_cntl;
}

void solo_asic_device::reg_c040_w(uint32_t data)
{
	if (m_utvdma_locked)
	{
		bool switched_to_ready = ((data ^ m_utvdma_cntl) & UTV_DMACNTL_READY);

		m_utvdma_cntl = data;

		if (switched_to_ready && !m_utvdma_started)
			solo_asic_device::utvdma_start();
	}
}

uint32_t solo_asic_device::reg_c100_r()
{
	return m_utvdma_locked;
}

void solo_asic_device::reg_c100_w(uint32_t data)
{
	m_utvdma_locked = data;
}

void solo_asic_device::utvdma_start()
{
	if (m_utvdma_locked && m_utvdma_cntl & UTV_DMACNTL_READY)
	{
		m_utvdma_csrc = m_utvdma_src;
		m_utvdma_cdst = m_utvdma_dst;
		m_utvdma_csize = m_utvdma_size;
		m_utvdma_cmode = m_utvdma_mode;

		if (m_utvdma_cmode & UTV_DMAMODE_READ)
			m_utvdma_ccnt = m_utvdma_cdst;
		else
			m_utvdma_ccnt = m_utvdma_csrc;
		
		m_utvdma_started = 0x1;
	}
}

void solo_asic_device::utvdma_next()
{
	if (m_utvdma_src != m_utvdma_csrc || m_utvdma_dst != m_utvdma_cdst || m_utvdma_csize != m_utvdma_size || m_utvdma_cmode != m_utvdma_mode)
		solo_asic_device::utvdma_start();
	else
		m_utvdma_csize = 0x0;
}

void solo_asic_device::utvdma_stop()
{
	m_utvdma_locked = 0x0;
	m_utvdma_started = 0x0;
	m_utvdma_csrc = 0x0;
	m_utvdma_cdst = 0x0;
	m_utvdma_csize = 0x0;
	m_utvdma_cmode = 0x0;
	m_utvdma_cntl = 0x0;

}

// Hardware modem registers
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
					solo_asic_device::modfw_hack_end();
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
		if (solo_asic_device::is_webtvos() && data == 0x7e)
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
		modem_should_threint = false;

		// Send directly to the UART device if in DLAB mode. For a WebTV OS, we also send direct if the UART receive buffer and modem buffer is emtpy.
		if ((m_modem_uart->ins8250_r(0x3) & 0x80) || (solo_asic_device::is_webtvos() && modem_txbuff_size == 0 && (m_modem_uart->ins8250_r(0x5) & INS8250_LSR_TSRE)))
		{
			m_modem_uart->ins8250_w(0x0, data & 0xff);
		}
		else
		{
			modem_txbuff[modem_txbuff_size++ & (MBUFF_MAX_SIZE - 1)] = data & 0xff;
			modem_buffer_timer->adjust(attotime::from_usec(MBUFF_FLUSH_TIME));
		}
	}

	if (data == modfw_enable_string[modfw_enable_index])
	{
		modfw_enable_index++;

		if ((modfw_enable_index + 1) >= sizeof(modfw_enable_string))
		{
			solo_asic_device::modfw_hack_begin();
			modfw_will_ack = true;
		}
	}
	else if (data == modfw_enable_string[0x0])
	{
		modfw_enable_index = 0x1;
	}
	else
	{
		modfw_enable_index = 0x0;
	}

	if (data == modfw_reset_string[modfw_reset_index])
	{
		modfw_reset_index++;

		if ((modfw_reset_index + 1) >= sizeof(modfw_reset_string))
		{
			solo_asic_device::mod_reset();
		}
	}
	else if (data == modfw_reset_string[0x0])
	{
		modfw_reset_index = 0x1;
	}
	else
	{
		modfw_reset_index = 0x0;
	}	
}

uint32_t solo_asic_device::reg_modem_0004_r()
{
	return m_modem_uart->ins8250_r(0x1);
}

void solo_asic_device::reg_modem_0004_w(uint32_t data)
{
	// If IER/DLM is written to then the downloader phase has finished, break out of the modem downloader hack.
	solo_asic_device::modfw_hack_end();

	m_modem_uart->ins8250_w(0x1, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0008_r()
{
	if (solo_asic_device::is_webtvos())
	{
		return m_modem_uart->ins8250_r(0x2);
	}
	else
	{
		return solo_asic_device::get_wince_modem_iir(true);
	}
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
	// If LCR is written to then the downloader phase has finished, break out of the modem downloader hack.
	solo_asic_device::modfw_hack_end();

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
		uint32_t data = m_modem_uart->ins8250_r(0x5);

		// The transmitter holding register empty or THRE is handled by our modem buffer on Windows CE.
		if (!solo_asic_device::is_webtvos())
		{
			if (modem_txbuff_size == 0x0)
			{
				data |= (0x20); // set THR is empty
			}
			else
			{
				data &= (~0x20); // remove THR is empty
			}
		}

		return data;
	}
}

void solo_asic_device::reg_modem_0014_w(uint32_t data)
{
	m_modem_uart->ins8250_w(0x5, data & 0xff);
}

uint32_t solo_asic_device::reg_modem_0018_r()
{
	uint32_t data = m_modem_uart->ins8250_r(0x6);

	if (solo_asic_device::is_webtvos())
	{
		// The &(~0x80) flips the carrier detect bit.
		// This is checked after hangup and causes a long wait. So we force it 0.
		// The wait will eventially timeout but this reduces the time we need to wait after hangup.
		// Always setting this to 0 doesn't effect anything else.
		data &= (~0x80);
	}

	return data;
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

uint8_t solo_asic_device::get_modem_iir()
{
	// Disables the UART device from clearing the THRE int
	auto sed = machine().disable_side_effects();

	return m_modem_uart->ins8250_r(0x2);
}

uint8_t solo_asic_device::get_wince_modem_iir(bool clear_threint)
{
	// The 0x2 (transmitter holding register empty or THRE) interrupt is handled by our modem buffer.
	// So erase the UART THRE interrupt bit and insert our own if needed.

	// Windows CE is more sensitive to this interrupt so we're being more specific when the buffer is empty.

	uint8_t data = solo_asic_device::get_modem_iir();

	if ((data & 0xe) == 0x2)
	{
		data &= (~0x2);
		data |= 0x1;
	}

	if (modem_should_threint)
	{
		// Transmit interrupts will take priority over receieve interrupts
		// So remove any receieve interrupt.
		data &= (~0x4);

		data |= 0x2;
		data &= (~0x1);

		if (clear_threint)
		{
			// The UART device spec says to clear the THRE interrupt when the IIR is read.
			modem_should_threint = false;
		}
	}

	return data;
}

int solo_asic_device::get_wince_intrpt_r()
{
	return  !BIT(solo_asic_device::get_wince_modem_iir(false), 0);
}

// IDE registers

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

// IIC operations used for devices like the tuner, NVRAM etc...

uint8_t solo_asic_device::sda_r()
{
	return m_iic_sda & 0x1;
}

void solo_asic_device::sda_w(uint8_t state)
{
	m_iic_sda = state & 0x1;
}

uint8_t solo_asic_device::scl_r()
{
	return m_iic_scl & 0x1;
}

void solo_asic_device::scl_w(uint8_t state)
{
	m_iic_scl = state & 0x1;
}

TIMER_CALLBACK_MEMBER(solo_asic_device::flush_modem_buffer)
{
	if (modem_should_threint && m_modem_uart->ins8250_r(0x5) & INS8250_LSR_TSRE)
	{
		// Assert to tell Windows CE we're ready for more data
		solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE0, ASSERT_LINE);
	}
	else if (modem_txbuff_size > 0 && (m_modem_uart->ins8250_r(0x5) & INS8250_LSR_TSRE))
	{
		m_modem_uart->ins8250_w(0x0, modem_txbuff[modem_txbuff_index++ & (MBUFF_MAX_SIZE - 1)]);

		if (modem_txbuff_index == modem_txbuff_size)
		{
			modem_txbuff_index = 0x0;
			modem_txbuff_size = 0x0;
			if (!solo_asic_device::is_webtvos())
			{
				// Allows us to tell Windows CE that the modem buffer is empty.
				modem_should_threint = true;
			}
		}
	}

	if (modem_txbuff_size > 0 || modem_should_threint)
		modem_buffer_timer->adjust(attotime::from_usec(MBUFF_FLUSH_TIME));
}

TIMER_CALLBACK_MEMBER(solo_asic_device::timer_irq)
{
	if (m_tcompare > 0x0)
	{
		solo_asic_device::set_timer_irq(BUS_INT_TIM_SYSTIMER, ASSERT_LINE);
	}
}

void solo_asic_device::int_enable_vid_w(int state)
{
	if (state)
		m_bus_intenable |= BUS_INT_VIDEO;
	else
		m_bus_intenable &= (~BUS_INT_VIDEO);
}

void solo_asic_device::irq_vid_w(int state)
{
	solo_asic_device::set_bus_irq(BUS_INT_VIDEO, state);
}

void solo_asic_device::int_enable_aud_w(int state)
{
	if (state)
		m_bus_intenable |= BUS_INT_AUDIO;
	else
		m_bus_intenable &= (~BUS_INT_AUDIO);
}

void solo_asic_device::irq_aud_w(int state)
{
	solo_asic_device::set_bus_irq(BUS_INT_AUDIO, state);
}

void solo_asic_device::irq_modem_w(int state)
{
	// Assert if WebTV OS or Windows CE with a non-THRE interrupt
	if (solo_asic_device::is_webtvos() || solo_asic_device::get_wince_intrpt_r())
	{
		solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE0, state);
	}
}

void solo_asic_device::irq_uart_w(int state)
{
	solo_asic_device::set_dev_irq(BUS_INT_DEV_UART, state);
}

void solo_asic_device::irq_ide1_w(int state)
{
	solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE1, state);
}

void solo_asic_device::irq_ide2_w(int state)
{
	solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE2, state);
}

void solo_asic_device::dmarq_ide1_w(int state)
{
	if (m_ide1_dmarq_state != state && m_utvdma_locked && m_utvdma_cntl & UTV_DMACNTL_READY)
	{
		m_ide1_dmarq_state = state;

		if (m_ide1_dmarq_state)
		{
			solo_asic_device::set_rio_irq(BUS_INT_RIO_DEVICE1, ASSERT_LINE);

			m_ata->write_dmack(ASSERT_LINE);

			while (m_ide1_dmarq_state)
			{
				if (m_utvdma_cmode & UTV_DMAMODE_READ)
					solo_asic_device::dmarq_dmaread(&m_ide1_dmarq_state, m_utvdma_csrc, m_utvdma_cdst, (m_utvdma_csize * sizeof(uint16_t)));
				else
					solo_asic_device::dmarq_dmawrite(&m_ide1_dmarq_state, m_utvdma_cdst, m_utvdma_csrc, (m_utvdma_csize * sizeof(uint16_t)));
			}

			solo_asic_device::set_dev_irq(BUS_INT_DEV_DMA, ASSERT_LINE);

			if (m_utvdma_csize == 0x0)
				solo_asic_device::utvdma_stop();

			m_ata->write_dmack(CLEAR_LINE);
		}
	}
}
void solo_asic_device::dmarq_dmaread(uint32_t* dmarq_state, uint32_t ide_device_base, uint32_t buf_start, uint32_t buf_size)
{
	if (buf_size > 0)
	{
		address_space &space = m_hostcpu->space(AS_PROGRAM);

		while (*dmarq_state)
		{
			uint32_t cur_size = (m_utvdma_ccnt - buf_start);

			if (cur_size < buf_size)
			{
				space.write_word(m_utvdma_ccnt, m_ata->read_dma());

				m_utvdma_ccnt += 0x02;
			}
			else
			{
				break;
			}
		}

		if ((m_utvdma_ccnt - buf_start) >= buf_size)
			solo_asic_device::utvdma_next();
	}
	else
	{
		while (*dmarq_state)
			m_ata->read_dma();
	}
}

void solo_asic_device::dmarq_dmawrite(uint32_t* dmarq_state, uint32_t ide_device_base, uint32_t buf_start, uint32_t buf_size)
{
	if (buf_size > 0)
	{
		address_space &space = m_hostcpu->space(AS_PROGRAM);

		while (*dmarq_state)
		{
			uint32_t cur_size = (m_utvdma_ccnt - buf_start);

			if (cur_size < buf_size)
			{
				m_ata->write_dma(space.read_word(m_utvdma_ccnt));

				m_utvdma_ccnt += 0x02;
			}
			else
			{
				break;
			}
		}

		if ((m_utvdma_ccnt - buf_start) >= buf_size)
			solo_asic_device::utvdma_next();
	}
	else
	{
		while (*dmarq_state)
			m_ata->write_dma(0x0000);
	}
}

void solo_asic_device::irq_keyboard_w(int state)
{
	solo_asic_device::set_dev_irq(BUS_INT_DEV_IRIN, state);
}

void solo_asic_device::set_gpio_irq(uint32_t mask, int state)
{
	if (mask & m_dev_gpio_in_mask)
	{
		m_busgpio_intenable |= m_dev_gpio_in_mask;
	}

	mask &= m_busgpio_intenable;

	if (m_busgpio_intenable & mask)
	{
		if (state)
		{
			m_busgpio_intstat |= mask;

			solo_asic_device::set_dev_irq(BUS_INT_DEV_GPIO, state);
		}
		else
		{
			m_busgpio_intstat &= (~mask);

			if(m_busgpio_intstat == 0x00)
				solo_asic_device::set_dev_irq(BUS_INT_DEV_GPIO, state);
		}
	}
}

void solo_asic_device::set_dev_irq(uint32_t mask, int state)
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
			m_busdev_intstat &= (~mask);

			if(m_busdev_intstat == 0x00)
				solo_asic_device::set_bus_irq(BUS_INT_DEV, state);
		}
	}
}

void solo_asic_device::set_rio_irq(uint32_t mask, int state)
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
			m_busrio_intstat &= (~mask);

			if(m_busrio_intstat == 0x00)
				solo_asic_device::set_bus_irq(BUS_INT_RIO, state);
		}
	}
}

void solo_asic_device::set_timer_irq(uint32_t mask, int state)
{
	if (m_bustim_intenable & mask && m_bus_intenable & BUS_INT_TIMER)
	{
		if (state)
		{
			m_bustim_intstat |= mask;

			solo_asic_device::set_bus_irq(BUS_INT_TIMER, state);
		}
		else
		{
			m_bustim_intstat &= (~mask);

			if(m_bustim_intstat == 0x00)
				solo_asic_device::set_bus_irq(BUS_INT_TIMER, state);
		}
	}
}

void solo_asic_device::set_bus_irq(uint32_t mask, int state)
{
	if (m_bus_intenable & mask)
	{
		if (state)
		{
			m_bus_intstat |= mask;

			if (m_bus_intstat != 0x0)
				m_hostcpu->set_input_line(MIPS3_IRQ0, ASSERT_LINE);
		}
		else
		{
			m_bus_intstat &= (~mask);

			// Once there's no more interrupts in progress then clear the IRQ0 bit.
			if (m_bus_intstat == 0x0)
				m_hostcpu->set_input_line(MIPS3_IRQ0, CLEAR_LINE);
		}
	}
 }