// license:BSD-3-Clause
// copyright-holders:FairPlay137

/***************************************************************************************
 *
 * WebTV LC2 (1997)
 * 
 * The WebTV line of products was an early attempt to bring the Internet to the
 * television. Later on in its life, it was rebranded as MSN TV.
 * 
 * LC2, shorthand for "Low Cost v2", was the second generation of the WebTV hardware.
 * It added graphics acceleration, an on-board printer port, and the ability to use a
 * hard drive, a TV tuner, and satellite receiver circuitry. It uses a custom ASIC
 * designed by WebTV Networks Inc. known as the SOLO chip.
 *
 * The original LC2 boards used a MIPS IDT R4640 clocked at 167MHz, although later
 * board revisions switched to a MIPS RM5230, although some LC2 dev boards and trial
 * run boards had RM5230s back when the retail models still used the R4640.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.
 * 
 * The technical specifications that this implementation is based on can be found here:
 * http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf
 * 
 * Stuff that still needs to be done:
 * - Different configurations for disk units and flash units (since there is one known
 * surviving trial unit in a flash-only configuration)
 * - Support for PAL emulation
 * - Video output
 * - Audio output
 * - Dummy emulation of tuner (required for booting Plus ROMs all the way)
 * 
 ***************************************************************************************/

#include "emu.h"

#include "bus/pc_kbd/keyboards.h"
#include "cpu/mips/mips3.h"
#include "machine/ds2401.h"
#include "machine/intelfsh.h"
#include "bus/ata/ataintf.h"
#include "solo_asic.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

// The system clock is used to drive the SOLO ASIC, drive the CPU and is used to calculate the audio clock.
constexpr XTAL     SYSCLOCK         = (XTAL(41'539'000) * 2);
constexpr uint16_t RAM_FLASHER_SIZE = 0x100;
constexpr uint32_t MAX_RAM_SIZE     = 0x4000000;

class webtv2_state : public driver_device
{
public:
	// constructor
	webtv2_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_soloasic(*this, "solo"),
		m_serial_id(*this, "serial_id"),
		m_nvram(*this, "nvram"),
		m_flash0(*this, "bank0_flash0"), // labeled U0501, contains upper bits
		m_flash1(*this, "bank0_flash1"), // labeled U0502, contains lower bits
		m_ata(*this, "ata")
	{ }

	void webtv2_sony(machine_config& config);
	void webtv2_philips(machine_config& config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	required_device<mips3_device> m_maincpu;
	required_device<solo_asic_device> m_soloasic;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;

	required_device<intelfsh16_device> m_flash0;
	required_device<intelfsh16_device> m_flash1;

	optional_device<ata_interface_device> m_ata;

	uint8_t ram_flasher[RAM_FLASHER_SIZE];

	void webtv2_base_map(address_map &map);
	void webtv2_base(machine_config& config);
	void webtv2_ram_map(address_map &map, uint32_t ram_size);
	void webtv2_retail_map(address_map &map);
	
	void bank0_flash_w(offs_t offset, uint32_t data);
	uint32_t bank0_flash_r(offs_t offset);
	uint8_t ram_flasher_r(offs_t offset);
	void ram_flasher_w(offs_t offset, uint8_t data);
	uint32_t status_r(offs_t offset);
	void status_w(offs_t offset, uint32_t data);
};

//
// WebTV stores the approm across 2 flash chips. The approm firmware is upgradable over a network.
//
// There's two 16-bit flash chips striped across a 32-bit bus. The chip with the upper 16-bits is labeled U0501, and lower is U0502.
//
// WebTV supports flash configurations to allow 1MB (2 x 4Mbit chips), 2MB (2 x 8Mbit chips) and 4MB (2 x 16Mbit chips) approm images.
// The 1MB flash configuration seems possible but it's unknown if it was used, 2MB flash configuration was released to the public
// and it seemed a 4MB flash configuration was used for debug builds during approm development as well as for a prototype Japan box.
//
// 1MB:          AM29F400AT + AM29F400AT (citation needed)
// Production:   AM29F800BT + AM29F800BT
// 4MB debug/JP: MX29F1610  + MX29F1610  (citation needed)
//
// WebTV supported SO-44 flash chips:
//
// Fujitsu:
//    MBM29F400B:  bottom bs, 5v 4Mbit  device id=0x22ab
//    MBM29F400T:  top bs,    5v 4Mbit  device id=0x2223 (have MAME support)
//    MBM29F800B:  bottom bs, 5v 8Mbit  device id=0x2258
//    MBM29F800T:  top bs,    5v 8Mbit  device id=0x22d6
//
// AMD:
//    AM29F400AB:  bottom bs, 5v 4Mbit  device id=0x22ab
//    AM29F400AT:  top bs,    5v 4Mbit  device id=0x2223
//    AM29F800BB:  bottom bs, 5v 8Mbit  device id=0x2258 (have MAME support)
//    AM29F800BT:  top bs,    5v 8Mbit  device id=0x22d6
//
// Macronix:
//    MX29F1610:   top bs,    5v 16Mbit device id=0x00f1 (have MAME support)
//
// bank0_flash0 = U0501
// bank0_flash1 = U0502
//
// NOTE: if you dump these chips you may need to byte swap the output. The bus is big-endian.
//
void webtv2_state::bank0_flash_w(offs_t offset, uint32_t data)
{
	//uint32_t actual_offset = offset & 0xfffff;
	uint16_t upper_value = (data >> 16) & 0xffff;
	//upper_value = (upper_value << 8) | ((upper_value >> 8) & 0xff);
	m_flash0->write(offset, upper_value);

	uint16_t lower_value = data & 0xffff;
	//lower_value = (lower_value << 8) | ((lower_value >> 8) & 0xff);
	m_flash1->write(offset, lower_value);
}

uint32_t webtv2_state::bank0_flash_r(offs_t offset)
{
	//uint32_t actual_offset = offset & 0xfffff;
	uint16_t upper_value = m_flash0->read(offset);
	//upper_value = (upper_value << 8) | ((upper_value >> 8) & 0xff);
	uint16_t lower_value = m_flash1->read(offset);
	//lower_value = (lower_value << 8) | ((lower_value >> 8) & 0xff);
	return (upper_value << 16) | (lower_value);
}

// WebTV's firmware writes the flashing code to the lower 256 bytes of RAM
// The flash ID instructions are written first, then the flash erase instructions then the flash program instructions.
// Since everything is written to the same place, the drc cache becomes out of sync and just re-executes the ID instructions.
// This allows us to capture when new code is written and then clear the drc cache.
uint8_t webtv2_state::ram_flasher_r(offs_t offset)
{
	return ram_flasher[offset & (RAM_FLASHER_SIZE - 1)];
}
void webtv2_state::ram_flasher_w(offs_t offset, uint8_t data)
{
	if (offset == 0)
		// New code is being written, clear drc cache.
		m_maincpu->code_flush_cache();

	ram_flasher[offset & (RAM_FLASHER_SIZE - 1)] = data;
}

// The flash programing code for the MX chips incorrectly bleeds over into the next memory region to read the status.
// This will return the correct status so it can continue.
uint32_t webtv2_state::status_r(offs_t offset)
{
	return 0x00800080;
}
void webtv2_state::status_w(offs_t offset, uint32_t data)
{
}

void webtv2_state::webtv2_base_map(address_map &map)
{
	map.global_mask(0x1fffffff);

	// Expansion device #1 to #7 8MB each (0x00800000-0x03ffffff)

	// WebTV Control Space (0x04000000-0x047fffff)
	map(0x04000000, 0x04000fff).m(m_soloasic, FUNC(solo_asic_device::bus_unit_map));
	map(0x04001000, 0x04001fff).m(m_soloasic, FUNC(solo_asic_device::rom_unit_map));
	map(0x04002000, 0x04002fff).m(m_soloasic, FUNC(solo_asic_device::aud_unit_map));
	map(0x04003000, 0x04003fff).m(m_soloasic, FUNC(solo_asic_device::vid_unit_map));
	map(0x04004000, 0x04004fff).m(m_soloasic, FUNC(solo_asic_device::dev_unit_map));
	map(0x04005000, 0x04005fff).m(m_soloasic, FUNC(solo_asic_device::mem_unit_map));
	map(0x04006000, 0x04006fff).m(m_soloasic, FUNC(solo_asic_device::gfx_unit_map));
	map(0x04007000, 0x04007fff).m(m_soloasic, FUNC(solo_asic_device::dve_unit_map));
	map(0x04008000, 0x04008fff).m(m_soloasic, FUNC(solo_asic_device::div_unit_map));
	map(0x04009000, 0x04009fff).m(m_soloasic, FUNC(solo_asic_device::pot_unit_map));
	map(0x0400a000, 0x0400afff).m(m_soloasic, FUNC(solo_asic_device::suc_unit_map));
	map(0x0400b000, 0x0400bfff).m(m_soloasic, FUNC(solo_asic_device::mod_unit_map));

	// Reserved (0x04800000-0x1f7fffff)
	map(0x1e000000, 0x1e00001f).m(m_soloasic, FUNC(solo_asic_device::hardware_modem_map));
	map(0x1e400000, 0x1e80001f).m(m_soloasic, FUNC(solo_asic_device::ide_map));
	map(0x1d400000, 0x1d80001f).m(m_soloasic, FUNC(solo_asic_device::ide_map));
}

void webtv2_state::webtv2_base(machine_config &config)
{
	config.set_default_layout(layout_webtv);

	R4640BE(config, m_maincpu, SYSCLOCK*2);
	m_maincpu->set_icache_size(0x2000);
	m_maincpu->set_dcache_size(0x2000);

	DS2401(config, m_serial_id, 0);

	I2C_24C01(config, m_nvram, 0);

	ATA_INTERFACE(config, m_ata).options(ata_devices, "hdd", nullptr, true);
	m_ata->irq_handler().set(m_soloasic, FUNC(solo_asic_device::irq_ide_w));

	SOLO_ASIC(config, m_soloasic, SYSCLOCK);
	m_soloasic->set_hostcpu(m_maincpu);
	m_soloasic->set_serial_id(m_serial_id);
	m_soloasic->set_ata(m_ata);
	m_soloasic->set_nvram(m_nvram);
}

void webtv2_state::webtv2_ram_map(address_map &map, uint32_t ram_size)
{
	ram_size = std::min(ram_size, MAX_RAM_SIZE);

	if ((MAX_RAM_SIZE - ram_size) > ram_size)
		map(0x00000000, (ram_size - 1)).ram().mirror(MAX_RAM_SIZE - ram_size).share("ram");
	else
		map(0x00000000, (ram_size - 1)).ram().share("ram");

	// The RAM flash code gets mirrored across the entire RAM region.
	for (uint32_t ram_flasher_base = 0x00000000; ram_flasher_base < MAX_RAM_SIZE; ram_flasher_base += ram_size)
		map(ram_flasher_base, ram_flasher_base + (RAM_FLASHER_SIZE - 1)).rw(FUNC(webtv2_state::ram_flasher_r), FUNC(webtv2_state::ram_flasher_w));
}

void webtv2_state::webtv2_retail_map(address_map &map)
{
	webtv2_state::webtv2_base_map(map);

	// 8MB RAM
	webtv2_state::webtv2_ram_map(map, 0x800000);

	// ROML Bank 0 (0x1f000000-0x1f3fffff)
	map(0x1f000000, 0x1f3fffff).rw(FUNC(webtv2_state::bank0_flash_r), FUNC(webtv2_state::bank0_flash_w)).share("bank0");

	// Diagnostic Space (0xf400000-0x1f7fffff)

	// ROMU Bank 1 (0x1f800000-0x1fffffff)
	map(0x1f800000, 0x1fffffff).rom().region("bank1", 0);

	// Reserved (0x20000000-0xffffffff)
}

void webtv2_state::webtv2_sony(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv2_state::webtv2_base(config);

	// 4MB approm
	MACRONIX_29F1610_16BIT(config, m_flash0, 0);
	MACRONIX_29F1610_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::webtv2_retail_map);
}

void webtv2_state::webtv2_philips(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv2_state::webtv2_base(config);

	// 4MB approm
	MACRONIX_29F1610_16BIT(config, m_flash0, 0);
	MACRONIX_29F1610_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::webtv2_retail_map);
}

void webtv2_state::machine_start()
{
	popmessage("WebTV starts with the display off. Press F1 to power on.\n");
}

void webtv2_state::machine_reset()
{

}

// This is emulator-specific config options that go beyond sysconfig offers.
static INPUT_PORTS_START( emu_config )
	PORT_START("emu_config")

	PORT_CONFNAME(0x03, 0x00, "Pixel buffer index")
	PORT_CONFSETTING(0x00, "Use pixel buffer 0")
	PORT_CONFSETTING(0x01, "Use pixel buffer 1")

INPUT_PORTS_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////


static INPUT_PORTS_START( retail_sys_config )
	PORT_START("sys_config")

	PORT_DIPNAME(0x03, 0x00, "Unknown")
	PORT_DIPSETTING(0x00, "Unknown")
	PORT_DIPSETTING(0x01, "Unknown")
	PORT_DIPSETTING(0x02, "Unknown")
	PORT_DIPSETTING(0x03, "Unknown")

	PORT_DIPNAME(0x04, 0x00, "Disk present")
	PORT_DIPSETTING(0x00, "Present")
	PORT_DIPSETTING(0x04, "Not present")

	PORT_DIPNAME(0x08, 0x00, "TV Standard")
	PORT_DIPSETTING(0x00, "NTSC")
	PORT_DIPSETTING(0x08, "PAL")

	PORT_DIPNAME(0xf0, 0x000, "Unknown")
	PORT_DIPSETTING(0x00, "Unknown")
	PORT_DIPSETTING(0x10, "Unknown")
	PORT_DIPSETTING(0x20, "Unknown")
	PORT_DIPSETTING(0x30, "Unknown")
	PORT_DIPSETTING(0x40, "Unknown")
	PORT_DIPSETTING(0x50, "Unknown")
	PORT_DIPSETTING(0x60, "Unknown")
	PORT_DIPSETTING(0x70, "Unknown")
	PORT_DIPSETTING(0x80, "Unknown")
	PORT_DIPSETTING(0x90, "Unknown")
	PORT_DIPSETTING(0xa0, "Unknown")
	PORT_DIPSETTING(0xb0, "Unknown")
	PORT_DIPSETTING(0xc0, "Unknown")
	PORT_DIPSETTING(0xd0, "Unknown")
	PORT_DIPSETTING(0xe0, "Unknown")
	PORT_DIPSETTING(0xf0, "Unknown")

	PORT_DIPNAME(0xf00, 0x700, "Board revision")
	PORT_DIPSETTING(0x000, "Unknown")
	PORT_DIPSETTING(0x100, "Unknown")
	PORT_DIPSETTING(0x200, "Unknown")
	PORT_DIPSETTING(0x300, "Unknown")
	PORT_DIPSETTING(0x400, "Unknown")
	PORT_DIPSETTING(0x500, "Unknown")
	PORT_DIPSETTING(0x600, "Unknown")
	PORT_DIPSETTING(0x700, "Unknown")
	PORT_DIPSETTING(0x800, "Unknown")
	PORT_DIPSETTING(0x900, "Unknown")
	PORT_DIPSETTING(0xa00, "Unknown")
	PORT_DIPSETTING(0xb00, "Unknown")
	PORT_DIPSETTING(0xc00, "Unknown")
	PORT_DIPSETTING(0xd00, "Unknown")
	PORT_DIPSETTING(0xe00, "Unknown")
	PORT_DIPSETTING(0xf00, "Unknown")

	PORT_DIPNAME(0x7000, 0x0000, "Board type")
	PORT_DIPSETTING(0x0000, "Unknown")
	PORT_DIPSETTING(0x1000, "Unknown")
	PORT_DIPSETTING(0x2000, "Unknown")
	PORT_DIPSETTING(0x3000, "Unknown")
	PORT_DIPSETTING(0x4000, "Unknown")
	PORT_DIPSETTING(0x5000, "Unknown")
	PORT_DIPSETTING(0x6000, "Unknown")
	PORT_DIPSETTING(0x7000, "Unknown")

	PORT_DIPNAME(0x18000, 0xc000, "CPU drive");
	PORT_DIPSETTING(0x0000, "Bus speed = 83%")
	PORT_DIPSETTING(0x4000, "Bus speed = 100%")
	PORT_DIPSETTING(0x8000, "Bus speed = 50%")
	PORT_DIPSETTING(0xc000, "Bus speed = 67%")

	PORT_DIPNAME(0x20000, 0x20000, "CPU clock multiplier")
	PORT_DIPSETTING(0x00000, "CPU clock = 3x bus clock")
	PORT_DIPSETTING(0x20000, "CPU clock = 2x bus clock")

	PORT_DIPNAME(0x40000, 0x40000, "Unknown")
	PORT_DIPSETTING(0x00000, "Unknown")
	PORT_DIPSETTING(0x40000, "Unknown")

	PORT_DIPNAME(0x80000, 0x80000, "CPU endian")
	PORT_DIPSETTING(0x00000, "Little endian")
	PORT_DIPSETTING(0x80000, "Big endian")

	PORT_DIPNAME(0x100000, 0x100000, "CPU class")
	PORT_DIPSETTING(0x000000, "IDT 5230")
	PORT_DIPSETTING(0x100000, "IDT 4640")

	PORT_DIPNAME(0x200000, 0x200000, "SMARTCARD0 present")
	PORT_DIPSETTING(0x000000, "Not present")
	PORT_DIPSETTING(0x200000, "Present")

	PORT_DIPNAME(0x400000, 0x000000, "SMARTCARD1 present")
	PORT_DIPSETTING(0x000000, "Not present")
	PORT_DIPSETTING(0x400000, "Present")

INPUT_PORTS_END

static INPUT_PORTS_START( retail_input )
	PORT_INCLUDE(retail_sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv2sony )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "LC2 Retail BootROM (2.0, build 2046)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
	ROM_RELOAD(0x600000, 0x200000)

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvhdd", 0x0000, NO_DUMP )
ROM_END

ROM_START( wtv2phil )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "LC2 Retail BootROM (2.0, build 2046)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
	ROM_RELOAD(0x600000, 0x200000)

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvhdd", 0x0000, NO_DUMP )
ROM_END

//    YEAR  NAME      PARENT  COMPAT  MACHINE         INPUT         CLASS         INIT        COMPANY               FULLNAME                            FLAGS
CONS( 1996, wtv2sony,      0,      0, webtv2_sony,    retail_input, webtv2_state, empty_init, "Sony",               "INT-W200 WebTV Plus Receiver", MACHINE_UNOFFICIAL )
CONS( 1996, wtv2phil,      0,      0, webtv2_philips, retail_input, webtv2_state, empty_init, "Philips-Magnavox",   "MAT972 WebTV Plus Receiver",   MACHINE_UNOFFICIAL )
