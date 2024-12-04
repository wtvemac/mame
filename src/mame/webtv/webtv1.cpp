// license:BSD-3-Clause
// copyright-holders:FairPlay137

/***************************************************************************************
 *
 * WebTV FCS (1996)
 * 
 * The WebTV line of products was an early attempt to bring the Internet to the
 * television. Later on in its life, it was rebranded as MSN TV.
 * 
 * FCS, shorthand for First Customer Ship, was the first generation of WebTV hardware.
 * Its ASIC, known as SPOT or FIDO, is much simpler than SOLO.
 * 
 * A typical retail configuration uses a MIPS IDT R4640 clocked at 112MHz, with 2MB of
 * on-board RAM, 2MB of flash memory (using two 16-bit 1MB AMD 29F800B chips) for the
 * updatable software, and 2MB of mask ROM.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.
 * 
 * Known issues:
 * - The CPU gets thrown into the exception handler loop at bfc0c030, a little before
 *   the memory check. This has to be manually bypassed to continue the boot process.
 *   (Worked around in mips3 and mips3drc)
 * - AppROMs appear to crash shortly after power on, with debug AppROMs throwing up
 *   "### CACHE ERROR" when serial communication is wired up to the SmartCard slot.
 * 
 ***************************************************************************************/

#include "emu.h"

#include "bus/pc_kbd/keyboards.h"
#include "cpu/mips/mips3.h"
#include "machine/ds2401.h"
#include "machine/intelfsh.h"
#include "spot_asic.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

// The system clock is used to drive the SPOT ASIC, drive the CPU and is used to calculate the audio clock.
// 56.448MHz was probably chosen because it's a multiple of the audio clock? (44100Hz)
constexpr XTAL     SYSCLOCK         = XTAL(56'448'000);
constexpr uint16_t RAM_FLASHER_SIZE = 0x100;
constexpr uint32_t MAX_RAM_SIZE     = 0x800000;

class webtv1_state : public driver_device
{
public:
	// constructor
	webtv1_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_spotasic(*this, "spot"),
		m_serial_id(*this, "serial_id"),
		m_nvram(*this, "nvram"),
		m_flash0(*this, "bank0_flash0"), // labeled U0501, contains upper bits
		m_flash1(*this, "bank0_flash1")  // labeled U0502, contains lower bits
	{ }

	void webtv1_dev(machine_config& config);
	void webtv1_bfe(machine_config& config);
	void webtv1_pal(machine_config& config);
	void webtv1_sony(machine_config& config);
	void webtv1_philips(machine_config& config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	required_device<mips3_device> m_maincpu;
	required_device<spot_asic_device> m_spotasic;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;

	required_device<intelfsh16_device> m_flash0;
	required_device<intelfsh16_device> m_flash1;

	uint8_t ram_flasher[RAM_FLASHER_SIZE];

	void webtv1_base_map(address_map &map);
	void webtv1_base(machine_config& config);
	void webtv1_ram_map(address_map &map, uint32_t ram_size);
	void webtv1_dev_map(address_map &map);
	void webtv1_bfe_map(address_map &map);
	void webtv1_retail_map(address_map &map);
	
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
void webtv1_state::bank0_flash_w(offs_t offset, uint32_t data)
{
	//uint32_t actual_offset = offset & 0xfffff;
	uint16_t upper_value = (data >> 16) & 0xffff;
	//upper_value = (upper_value << 8) | ((upper_value >> 8) & 0xff);
	m_flash0->write(offset, upper_value);

	uint16_t lower_value = data & 0xffff;
	//lower_value = (lower_value << 8) | ((lower_value >> 8) & 0xff);
	m_flash1->write(offset, lower_value);
}

uint32_t webtv1_state::bank0_flash_r(offs_t offset)
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
uint8_t webtv1_state::ram_flasher_r(offs_t offset)
{
	return ram_flasher[offset & (RAM_FLASHER_SIZE - 1)];
}
void webtv1_state::ram_flasher_w(offs_t offset, uint8_t data)
{
	if (offset == 0)
		// New code is being written, clear drc cache.
		m_maincpu->code_flush_cache();

	ram_flasher[offset & (RAM_FLASHER_SIZE - 1)] = data;
}

// The flash programing code for the MX chips incorrectly bleeds over into the next memory region to read the status.
// This will return the correct status so it can continue.
uint32_t webtv1_state::status_r(offs_t offset)
{
	return 0x00800080;
}
void webtv1_state::status_w(offs_t offset, uint32_t data)
{
}


void webtv1_state::webtv1_base_map(address_map &map)
{
	map.global_mask(0x1fffffff);

	// Expansion device #1 to #7 8MB each (0x00800000-0x03ffffff)

	// WebTV Control Space (0x04000000-0x047fffff)
	// SPOT
	map(0x04000000, 0x04000fff).m(m_spotasic, FUNC(spot_asic_device::bus_unit_map));
	map(0x04001000, 0x04001fff).m(m_spotasic, FUNC(spot_asic_device::rom_unit_map));
	map(0x04002000, 0x04002fff).m(m_spotasic, FUNC(spot_asic_device::aud_unit_map));
	map(0x04003000, 0x04003fff).m(m_spotasic, FUNC(spot_asic_device::vid_unit_map));
	map(0x04004000, 0x04004fff).m(m_spotasic, FUNC(spot_asic_device::dev_unit_map));
	map(0x04005000, 0x04005fff).m(m_spotasic, FUNC(spot_asic_device::mem_unit_map));

	// Reserved (0x04800000-0x1f7fffff)
}

void webtv1_state::webtv1_base(machine_config &config)
{
	config.set_default_layout(layout_webtv);

	R4640BE(config, m_maincpu, SYSCLOCK*2);
	m_maincpu->set_icache_size(0x2000);
	m_maincpu->set_dcache_size(0x2000);

	DS2401(config, m_serial_id, 0);

	I2C_24C01(config, m_nvram, 0);

	SPOT_ASIC(config, m_spotasic, SYSCLOCK);
	m_spotasic->set_hostcpu(m_maincpu);
	m_spotasic->set_serial_id(m_serial_id);
	m_spotasic->set_nvram(m_nvram);
}

void webtv1_state::webtv1_ram_map(address_map &map, uint32_t ram_size)
{
	ram_size = std::min(ram_size, MAX_RAM_SIZE);

	if ((MAX_RAM_SIZE - ram_size) > ram_size)
		map(0x00000000, (ram_size - 1)).ram().mirror(MAX_RAM_SIZE - ram_size).share("ram");
	else
		map(0x00000000, (ram_size - 1)).ram().share("ram");

	// The RAM flash code gets mirrored across the entire RAM region.
	for (uint32_t ram_flasher_base = 0x00000000; ram_flasher_base < MAX_RAM_SIZE; ram_flasher_base += ram_size)
		map(ram_flasher_base, ram_flasher_base + (RAM_FLASHER_SIZE - 1)).rw(FUNC(webtv1_state::ram_flasher_r), FUNC(webtv1_state::ram_flasher_w));
}

void webtv1_state::webtv1_dev_map(address_map &map)
{
	webtv1_state::webtv1_base_map(map);

	// Use 4MB for debug boxes
	webtv1_state::webtv1_ram_map(map, 0x400000);

	// ROML Bank 0 (0x1f000000-0x1f3fffff)
	map(0x1f000000, 0x1f3fffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).share("bank0");
	map(0x1f400000, 0x1f400003).rw(FUNC(webtv1_state::status_r), FUNC(webtv1_state::status_w));

	// Diagnostic Space (0xf400000-0x1f7fffff)

	// ROMU Bank 1 (0x1f800000-0x1fffffff)
	//map(0x1fc00000, 0x1fdfffff).rw(FUNC(webtv1_state::bootrom_flash_r), FUNC(webtv1_state::bootrom_flash_w)).share("bootrom_flash");
	map(0x1f800000, 0x1fdfffff).rom().region("bank1", 0);

	// Reserved (0x20000000-0xffffffff)
}

void webtv1_state::webtv1_dev(machine_config& config)
{
	webtv1_state::webtv1_base(config);

	// 4MB bf0app Debug Approm
	MACRONIX_29F1610_16BIT(config, m_flash0, 0);
	MACRONIX_29F1610_16BIT(config, m_flash1, 0);

	// 2MB Flashable Bootrom
	//AMD_29F800B_16BIT(config, m_bootrom_flash0, 0);
	//AMD_29F800B_16BIT(config, m_bootrom_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_dev_map);
}

void webtv1_state::webtv1_bfe_map(address_map &map)
{
	webtv1_state::webtv1_base_map(map);

	// 2MB RAM
	webtv1_state::webtv1_ram_map(map, 0x200000);

	// ROML Bank 0 (0x1f000000-0x1f3fffff)

	// Diagnostic Space (0xf400000-0x1f7fffff)

	// ROMU Bank 1 (0x1f800000-0x1fffffff)
	map(0x1f800000, 0x1fdfffff).rom().region("bank1", 0);
	// not renaming to maintain compatibility with previous files but this is on bank1, not bank0.
	// I've tested by renaming this to approm_flash since it's descriptive and no need to cludge up MAME with multiple variables.
	map(0x1fe00000, 0x1fffffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).share("bank0");

	// Reserved (0x20000000-0xffffffff)
}

void webtv1_state::webtv1_bfe(machine_config& config)
{
	webtv1_state::webtv1_base(config);

	// 2MB bfeapp Approm
	AMD_29F800B_16BIT(config, m_flash0, 0);
	AMD_29F800B_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_bfe_map);
}

void webtv1_state::webtv1_retail_map(address_map &map)
{
	webtv1_state::webtv1_base_map(map);

	// 2MB RAM
	webtv1_state::webtv1_ram_map(map, 0x200000);

	// ROML Bank 0 (0x1f000000-0x1f3fffff)
	map(0x1f000000, 0x1f3fffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).share("bank0");

	// Diagnostic Space (0xf400000-0x1f7fffff)

	// ROMU Bank 1 (0x1f800000-0x1fffffff)
	map(0x1f800000, 0x1fdfffff).rom().region("bank1", 0);

	// Reserved (0x20000000-0xffffffff)
}

void webtv1_state::webtv1_pal(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv1_state::webtv1_base(config);

	// 2MB bf0app Approm
	AMD_29F800B_16BIT(config, m_flash0, 0);
	AMD_29F800B_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_retail_map);
}

void webtv1_state::webtv1_sony(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv1_state::webtv1_base(config);

	// 2MB bf0app Approm
	AMD_29F800B_16BIT(config, m_flash0, 0);
	AMD_29F800B_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_retail_map);
}

void webtv1_state::webtv1_philips(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv1_state::webtv1_base(config);

	// 2MB bf0app Approm
	AMD_29F800B_16BIT(config, m_flash0, 0);
	AMD_29F800B_16BIT(config, m_flash1, 0);

	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_retail_map);
}

void webtv1_state::machine_start()
{
	popmessage("WebTV starts with the display off. Press F1 to power on.\n");
}

void webtv1_state::machine_reset()
{

}

// This is emulator-specific config options that go beyond sysconfig offers.
static INPUT_PORTS_START( emu_config )
	PORT_START("emu_config")

	PORT_CONFNAME(0x03, 0x00, "Pixel buffer index")
	PORT_CONFSETTING(0x00, "Use pixel buffer 0")
	PORT_CONFSETTING(0x01, "Use pixel buffer 1")

	PORT_CONFNAME(0x0c, 0x0c, "Smartcard bangserial")
	PORT_CONFSETTING(0x00, "Off")
	PORT_CONFSETTING(0x04, "V1 bangserial data")
	PORT_CONFSETTING(0x08, "V2 bangserial data")
	PORT_CONFSETTING(0x0c, "Autodetect")
INPUT_PORTS_END

// Sysconfig options are usually configured via resistors on the board.

static INPUT_PORTS_START( bfe_sys_config )
	PORT_START("sys_config")

	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")

	PORT_DIPNAME(0x0c, 0x0c, "Board type")
	PORT_DIPSETTING(0x00, "Reserved")
	PORT_DIPSETTING(0x04, "Reserved")
	PORT_DIPSETTING(0x08, "Trial-type board")
	PORT_DIPSETTING(0x0c, "FCS board (retail)")

	PORT_DIPNAME(0xf0, 0x80, "Board revision");

	PORT_DIPUNUSED_DIPLOC(0x100, 0x100, "SW1:8")
	PORT_DIPUNUSED_DIPLOC(0x200, 0x200, "SW1:9")
	PORT_DIPUNUSED_DIPLOC(0x400, 0x400, "SW1:10")

	// A 24.54MHz or 29.5MHz crystal drives the SAA7187 encoder chip. These pixel clocks are divisors of that input.
	PORT_DIPNAME(0x800, 0x800, "NTSC/PAL");
	PORT_DIPSETTING(0x000, "PAL mode w/ 14.75MHz pixel clock")
	PORT_DIPSETTING(0x800, "NTSC mode w/ 12.26MHz pixel clock")

	PORT_DIPUNUSED_DIPLOC(0x1000, 0x1000, "SW1:12")

	PORT_DIPNAME(0x2000, 0x2000, "CPU output buffers")
	PORT_DIPSETTING(0x0000, "83% CPU output buffers on reset")
	PORT_DIPSETTING(0x2000, "50% CPU output buffers on reset (emulated behavior)")

	PORT_DIPNAME(0xc000, 0x8000, "CPU clock multiplier");
	PORT_DIPSETTING(0x0000, "CPU clock = 5X bus clock")
	PORT_DIPSETTING(0x4000, "CPU clock = 4X bus clock")
	PORT_DIPSETTING(0x8000, "CPU clock = 2X bus clock (emulated behavior)")
	PORT_DIPSETTING(0xc000, "CPU clock = 3X bus clock")

	PORT_DIPNAME(0x10000, 0x00000, "vidUnit clock source")
	PORT_DIPSETTING(0x00000, "Use external video clock")
	PORT_DIPSETTING(0x10000, "SPOT controlled video clock")

	PORT_DIPNAME(0x20000, 0x00000, "audUnit clock source")
	PORT_DIPSETTING(0x00000, "SPOT controlled DAC clock")
	PORT_DIPSETTING(0x20000, "Use external DAC clock")

	PORT_DIPNAME(0xc0000, 0xc0000, "audUnit DAC type");
	PORT_DIPSETTING(0x00000, "Reserved")
	PORT_DIPSETTING(0x40000, "Reserved")
	PORT_DIPSETTING(0x80000, "Reserved")
	PORT_DIPSETTING(0xc0000, "AKM 4310/4309")

	PORT_DIPNAME(0x300000, 0x200000, "Memory vendor")
	PORT_DIPSETTING(0x000000, "Other")
	PORT_DIPSETTING(0x100000, "Samsung")
	PORT_DIPSETTING(0x200000, "Fujitsu")
	PORT_DIPSETTING(0x300000, "NEC")

	PORT_DIPNAME(0xc00000, 0xC00000, "Memory speed")
	PORT_DIPSETTING(0x000000, "100MHz parts")
	PORT_DIPSETTING(0x400000, "66MHz parts")
	PORT_DIPSETTING(0x800000, "77MHz parts")
	PORT_DIPSETTING(0xc00000, "83MHz parts")

	PORT_DIPNAME(0x3000000, 0x3000000, "Bank 1 ROM speed")
	PORT_DIPSETTING(0x0000000, "200ns/100ns")
	PORT_DIPSETTING(0x1000000, "100ns/50ns")
	PORT_DIPSETTING(0x2000000, "90ns/45ns")
	PORT_DIPSETTING(0x3000000, "120ns/60ns")

	PORT_DIPNAME(0x4000000, 0x0000000, "Bank 1 ROM mode")
	PORT_DIPSETTING(0x0000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x4000000, "Supports page mode")

	PORT_DIPNAME(0x8000000, 0x8000000, "Bank 1 ROM type")
	PORT_DIPSETTING(0x0000000, "Flash ROM")
	PORT_DIPSETTING(0x8000000, "Mask ROM (emulated behavior)")

	PORT_DIPNAME(0x30000000, 0x20000000, "Bank 0 ROM speed")
	PORT_DIPSETTING(0x00000000, "200ns/100ns")
	PORT_DIPSETTING(0x10000000, "100ns/50ns")
	PORT_DIPSETTING(0x20000000, "90ns/45ns")
	PORT_DIPSETTING(0x30000000, "120ns/60ns")

	PORT_DIPNAME(0x40000000, 0x00000000, "Bank 0 ROM mode")
	PORT_DIPSETTING(0x00000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x40000000, "Supports page mode")

	PORT_DIPNAME(0x80000000, 0x00000000, "Bank 0 ROM type")
	PORT_DIPSETTING(0x00000000, "Flash ROM (emulated behavior)")
	PORT_DIPSETTING(0x80000000, "Mask ROM")
INPUT_PORTS_END

static INPUT_PORTS_START( bfe_input )
	PORT_INCLUDE(bfe_sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv1bfe )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x600000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bfe-boot", "Prototype bfe BootROM (1.0, build 105)")
	ROMX_LOAD("bfe-boot.o", 0x400000, 0x200000, NO_DUMP, ROM_BIOS(0))         // 0x9fc00000/0xbfc00000
	ROM_SYSTEM_BIOS(1, "prealpha-boot", "Pre-alpha bfe BootROM")
	ROMX_LOAD("prealpha-boot.o", 0x400000, 0x200000, NO_DUMP, ROM_BIOS(1))    // 0x9fc00000/0xbfc00000
ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

static INPUT_PORTS_START( dev_sys_config )
	PORT_START("sys_config")

	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")

	PORT_DIPNAME(0x0c, 0x0c, "Board type")
	PORT_DIPSETTING(0x00, "Reserved")
	PORT_DIPSETTING(0x04, "Reserved")
	PORT_DIPSETTING(0x08, "Trial-type board")
	PORT_DIPSETTING(0x0c, "FCS board (retail)")

	PORT_DIPNAME(0xf0, 0x80, "Board revision");

	PORT_DIPUNUSED_DIPLOC(0x100, 0x100, "SW1:8")
	PORT_DIPUNUSED_DIPLOC(0x200, 0x200, "SW1:9")
	PORT_DIPUNUSED_DIPLOC(0x400, 0x400, "SW1:10")

	// A 24.54MHz or 29.5MHz crystal drives the SAA7187 encoder chip. These pixel clocks are divisors of that input.
	PORT_DIPNAME(0x800, 0x800, "NTSC/PAL");
	PORT_DIPSETTING(0x000, "PAL mode w/ 14.75MHz pixel clock")
	PORT_DIPSETTING(0x800, "NTSC mode w/ 12.26MHz pixel clock")

	PORT_DIPUNUSED_DIPLOC(0x1000, 0x1000, "SW1:12")

	PORT_DIPNAME(0x2000, 0x2000, "CPU output buffers")
	PORT_DIPSETTING(0x0000, "83% CPU output buffers on reset")
	PORT_DIPSETTING(0x2000, "50% CPU output buffers on reset (emulated behavior)")

	PORT_DIPNAME(0xc000, 0x8000, "CPU clock multiplier");
	PORT_DIPSETTING(0x0000, "CPU clock = 5X bus clock")
	PORT_DIPSETTING(0x4000, "CPU clock = 4X bus clock")
	PORT_DIPSETTING(0x8000, "CPU clock = 2X bus clock (emulated behavior)")
	PORT_DIPSETTING(0xc000, "CPU clock = 3X bus clock")

	PORT_DIPNAME(0x10000, 0x00000, "vidUnit clock source")
	PORT_DIPSETTING(0x00000, "Use external video clock")
	PORT_DIPSETTING(0x10000, "SPOT controlled video clock")

	PORT_DIPNAME(0x20000, 0x00000, "audUnit clock source")
	PORT_DIPSETTING(0x00000, "SPOT controlled DAC clock")
	PORT_DIPSETTING(0x20000, "Use external DAC clock")

	PORT_DIPNAME(0xc0000, 0xc0000, "audUnit DAC type");
	PORT_DIPSETTING(0x00000, "Reserved")
	PORT_DIPSETTING(0x40000, "Reserved")
	PORT_DIPSETTING(0x80000, "Reserved")
	PORT_DIPSETTING(0xc0000, "AKM 4310/4309")

	PORT_DIPNAME(0x300000, 0x200000, "Memory vendor")
	PORT_DIPSETTING(0x000000, "Other")
	PORT_DIPSETTING(0x100000, "Samsung")
	PORT_DIPSETTING(0x200000, "Fujitsu")
	PORT_DIPSETTING(0x300000, "NEC")

	PORT_DIPNAME(0xc00000, 0xC00000, "Memory speed")
	PORT_DIPSETTING(0x000000, "100MHz parts")
	PORT_DIPSETTING(0x400000, "66MHz parts")
	PORT_DIPSETTING(0x800000, "77MHz parts")
	PORT_DIPSETTING(0xc00000, "83MHz parts")

	PORT_DIPNAME(0x3000000, 0x3000000, "Bank 1 ROM speed")
	PORT_DIPSETTING(0x0000000, "200ns/100ns")
	PORT_DIPSETTING(0x1000000, "100ns/50ns")
	PORT_DIPSETTING(0x2000000, "90ns/45ns")
	PORT_DIPSETTING(0x3000000, "120ns/60ns")

	PORT_DIPNAME(0x4000000, 0x0000000, "Bank 1 ROM mode")
	PORT_DIPSETTING(0x0000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x4000000, "Supports page mode")

	PORT_DIPNAME(0x8000000, 0x8000000, "Bank 1 ROM type")
	PORT_DIPSETTING(0x0000000, "Flash ROM")
	PORT_DIPSETTING(0x8000000, "Mask ROM (emulated behavior)")

	PORT_DIPNAME(0x30000000, 0x20000000, "Bank 0 ROM speed")
	PORT_DIPSETTING(0x00000000, "200ns/100ns")
	PORT_DIPSETTING(0x10000000, "100ns/50ns")
	PORT_DIPSETTING(0x20000000, "90ns/45ns")
	PORT_DIPSETTING(0x30000000, "120ns/60ns")

	PORT_DIPNAME(0x40000000, 0x00000000, "Bank 0 ROM mode")
	PORT_DIPSETTING(0x00000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x40000000, "Supports page mode")

	PORT_DIPNAME(0x80000000, 0x00000000, "Bank 0 ROM type")
	PORT_DIPSETTING(0x00000000, "Flash ROM (emulated behavior)")
	PORT_DIPSETTING(0x80000000, "Mask ROM")
INPUT_PORTS_END

static INPUT_PORTS_START( dev_input )
	PORT_INCLUDE(dev_sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv1dev )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x600000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

static INPUT_PORTS_START( pal_sys_config )
	PORT_START("sys_config")

	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")

	PORT_DIPNAME(0x0c, 0x0c, "Board type")
	PORT_DIPSETTING(0x00, "Reserved")
	PORT_DIPSETTING(0x04, "Reserved")
	PORT_DIPSETTING(0x08, "Trial-type board")
	PORT_DIPSETTING(0x0c, "FCS board (retail)")

	PORT_DIPNAME(0xf0, 0x80, "Board revision");

	PORT_DIPUNUSED_DIPLOC(0x100, 0x100, "SW1:8")
	PORT_DIPUNUSED_DIPLOC(0x200, 0x200, "SW1:9")
	PORT_DIPUNUSED_DIPLOC(0x400, 0x400, "SW1:10")

	// A 24.54MHz or 29.5MHz crystal drives the SAA7187 encoder chip. These pixel clocks are divisors of that input.
	PORT_DIPNAME(0x800, 0x000, "NTSC/PAL");
	PORT_DIPSETTING(0x000, "PAL mode w/ 14.75MHz pixel clock")
	PORT_DIPSETTING(0x800, "NTSC mode w/ 12.26MHz pixel clock")

	PORT_DIPUNUSED_DIPLOC(0x1000, 0x1000, "SW1:12")

	PORT_DIPNAME(0x2000, 0x2000, "CPU output buffers")
	PORT_DIPSETTING(0x0000, "83% CPU output buffers on reset")
	PORT_DIPSETTING(0x2000, "50% CPU output buffers on reset (emulated behavior)")

	PORT_DIPNAME(0xc000, 0x8000, "CPU clock multiplier");
	PORT_DIPSETTING(0x0000, "CPU clock = 5X bus clock")
	PORT_DIPSETTING(0x4000, "CPU clock = 4X bus clock")
	PORT_DIPSETTING(0x8000, "CPU clock = 2X bus clock (emulated behavior)")
	PORT_DIPSETTING(0xc000, "CPU clock = 3X bus clock")

	PORT_DIPNAME(0x10000, 0x00000, "vidUnit clock source")
	PORT_DIPSETTING(0x00000, "Use external video clock")
	PORT_DIPSETTING(0x10000, "SPOT controlled video clock")

	PORT_DIPNAME(0x20000, 0x00000, "audUnit clock source")
	PORT_DIPSETTING(0x00000, "SPOT controlled DAC clock")
	PORT_DIPSETTING(0x20000, "Use external DAC clock")

	PORT_DIPNAME(0xc0000, 0xc0000, "audUnit DAC type");
	PORT_DIPSETTING(0x00000, "Reserved")
	PORT_DIPSETTING(0x40000, "Reserved")
	PORT_DIPSETTING(0x80000, "Reserved")
	PORT_DIPSETTING(0xc0000, "AKM 4310/4309")

	PORT_DIPNAME(0x300000, 0x200000, "Memory vendor")
	PORT_DIPSETTING(0x000000, "Other")
	PORT_DIPSETTING(0x100000, "Samsung")
	PORT_DIPSETTING(0x200000, "Fujitsu")
	PORT_DIPSETTING(0x300000, "NEC")

	PORT_DIPNAME(0xc00000, 0xC00000, "Memory speed")
	PORT_DIPSETTING(0x000000, "100MHz parts")
	PORT_DIPSETTING(0x400000, "66MHz parts")
	PORT_DIPSETTING(0x800000, "77MHz parts")
	PORT_DIPSETTING(0xc00000, "83MHz parts")

	PORT_DIPNAME(0x3000000, 0x3000000, "Bank 1 ROM speed")
	PORT_DIPSETTING(0x0000000, "200ns/100ns")
	PORT_DIPSETTING(0x1000000, "100ns/50ns")
	PORT_DIPSETTING(0x2000000, "90ns/45ns")
	PORT_DIPSETTING(0x3000000, "120ns/60ns")

	PORT_DIPNAME(0x4000000, 0x0000000, "Bank 1 ROM mode")
	PORT_DIPSETTING(0x0000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x4000000, "Supports page mode")

	PORT_DIPNAME(0x8000000, 0x8000000, "Bank 1 ROM type")
	PORT_DIPSETTING(0x0000000, "Flash ROM")
	PORT_DIPSETTING(0x8000000, "Mask ROM (emulated behavior)")

	PORT_DIPNAME(0x30000000, 0x20000000, "Bank 0 ROM speed")
	PORT_DIPSETTING(0x00000000, "200ns/100ns")
	PORT_DIPSETTING(0x10000000, "100ns/50ns")
	PORT_DIPSETTING(0x20000000, "90ns/45ns")
	PORT_DIPSETTING(0x30000000, "120ns/60ns")

	PORT_DIPNAME(0x40000000, 0x00000000, "Bank 0 ROM mode")
	PORT_DIPSETTING(0x00000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x40000000, "Supports page mode")

	PORT_DIPNAME(0x80000000, 0x00000000, "Bank 0 ROM type")
	PORT_DIPSETTING(0x00000000, "Flash ROM (emulated behavior)")
	PORT_DIPSETTING(0x80000000, "Mask ROM")
INPUT_PORTS_END

static INPUT_PORTS_START( pal_input )
	PORT_INCLUDE(pal_sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv1pal )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x600000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

static INPUT_PORTS_START( retail_sys_config )
	PORT_START("sys_config")

	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")

	PORT_DIPNAME(0x0c, 0x0c, "Board type")
	PORT_DIPSETTING(0x00, "Reserved")
	PORT_DIPSETTING(0x04, "Reserved")
	PORT_DIPSETTING(0x08, "Trial-type board")
	PORT_DIPSETTING(0x0c, "FCS board (retail)")

	PORT_DIPNAME(0xf0, 0x80, "Board revision");

	PORT_DIPUNUSED_DIPLOC(0x100, 0x100, "SW1:8")
	PORT_DIPUNUSED_DIPLOC(0x200, 0x200, "SW1:9")
	PORT_DIPUNUSED_DIPLOC(0x400, 0x400, "SW1:10")

	// A 24.54MHz or 29.5MHz crystal drives the SAA7187 encoder chip. These pixel clocks are divisors of that input.
	PORT_DIPNAME(0x800, 0x800, "NTSC/PAL");
	PORT_DIPSETTING(0x000, "PAL mode w/ 14.75MHz pixel clock")
	PORT_DIPSETTING(0x800, "NTSC mode w/ 12.26MHz pixel clock")

	PORT_DIPUNUSED_DIPLOC(0x1000, 0x1000, "SW1:12")

	PORT_DIPNAME(0x2000, 0x2000, "CPU output buffers")
	PORT_DIPSETTING(0x0000, "83% CPU output buffers on reset")
	PORT_DIPSETTING(0x2000, "50% CPU output buffers on reset (emulated behavior)")

	PORT_DIPNAME(0xc000, 0x8000, "CPU clock multiplier");
	PORT_DIPSETTING(0x0000, "CPU clock = 5X bus clock")
	PORT_DIPSETTING(0x4000, "CPU clock = 4X bus clock")
	PORT_DIPSETTING(0x8000, "CPU clock = 2X bus clock (emulated behavior)")
	PORT_DIPSETTING(0xc000, "CPU clock = 3X bus clock")

	PORT_DIPNAME(0x10000, 0x00000, "vidUnit clock source")
	PORT_DIPSETTING(0x00000, "Use external video clock")
	PORT_DIPSETTING(0x10000, "SPOT controlled video clock")

	PORT_DIPNAME(0x20000, 0x00000, "audUnit clock source")
	PORT_DIPSETTING(0x00000, "SPOT controlled DAC clock")
	PORT_DIPSETTING(0x20000, "Use external DAC clock")

	PORT_DIPNAME(0xc0000, 0xc0000, "audUnit DAC type");
	PORT_DIPSETTING(0x00000, "Reserved")
	PORT_DIPSETTING(0x40000, "Reserved")
	PORT_DIPSETTING(0x80000, "Reserved")
	PORT_DIPSETTING(0xc0000, "AKM 4310/4309")

	PORT_DIPNAME(0x300000, 0x200000, "Memory vendor")
	PORT_DIPSETTING(0x000000, "Other")
	PORT_DIPSETTING(0x100000, "Samsung")
	PORT_DIPSETTING(0x200000, "Fujitsu")
	PORT_DIPSETTING(0x300000, "NEC")

	PORT_DIPNAME(0xc00000, 0xC00000, "Memory speed")
	PORT_DIPSETTING(0x000000, "100MHz parts")
	PORT_DIPSETTING(0x400000, "66MHz parts")
	PORT_DIPSETTING(0x800000, "77MHz parts")
	PORT_DIPSETTING(0xc00000, "83MHz parts")

	PORT_DIPNAME(0x3000000, 0x3000000, "Bank 1 ROM speed")
	PORT_DIPSETTING(0x0000000, "200ns/100ns")
	PORT_DIPSETTING(0x1000000, "100ns/50ns")
	PORT_DIPSETTING(0x2000000, "90ns/45ns")
	PORT_DIPSETTING(0x3000000, "120ns/60ns")

	PORT_DIPNAME(0x4000000, 0x0000000, "Bank 1 ROM mode")
	PORT_DIPSETTING(0x0000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x4000000, "Supports page mode")

	PORT_DIPNAME(0x8000000, 0x8000000, "Bank 1 ROM type")
	PORT_DIPSETTING(0x0000000, "Flash ROM")
	PORT_DIPSETTING(0x8000000, "Mask ROM (emulated behavior)")

	PORT_DIPNAME(0x30000000, 0x20000000, "Bank 0 ROM speed")
	PORT_DIPSETTING(0x00000000, "200ns/100ns")
	PORT_DIPSETTING(0x10000000, "100ns/50ns")
	PORT_DIPSETTING(0x20000000, "90ns/45ns")
	PORT_DIPSETTING(0x30000000, "120ns/60ns")

	PORT_DIPNAME(0x40000000, 0x00000000, "Bank 0 ROM mode")
	PORT_DIPSETTING(0x00000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x40000000, "Supports page mode")

	PORT_DIPNAME(0x80000000, 0x00000000, "Bank 0 ROM type")
	PORT_DIPSETTING(0x00000000, "Flash ROM (emulated behavior)")
	PORT_DIPSETTING(0x80000000, "Mask ROM")
INPUT_PORTS_END

static INPUT_PORTS_START( retail_input )
	PORT_INCLUDE(retail_sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv1sony )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x600000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
ROM_END

ROM_START( wtv1phil )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x600000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x400000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
ROM_END

//    YEAR  NAME      PARENT  COMPAT  MACHINE         INPUT         CLASS         INIT        COMPANY               FULLNAME                            FLAGS
CONS( 1996, wtv1bfe,       0,      0, webtv1_bfe,     bfe_input,    webtv1_state, empty_init, "WebTV",               "WebTV bfeapp Protoype Box",       MACHINE_UNOFFICIAL )
CONS( 1996, wtv1dev,       0,      0, webtv1_dev,     dev_input,    webtv1_state, empty_init, "WebTV",               "WebTV 4MB Development Box",       MACHINE_UNOFFICIAL )
CONS( 1996, wtv1pal,       0,      0, webtv1_pal,     pal_input,    webtv1_state, empty_init, "WebTV",               "WebTV PAL Box",                   MACHINE_UNOFFICIAL )
CONS( 1996, wtv1sony,      0,      0, webtv1_sony,    retail_input, webtv1_state, empty_init, "Sony",               "INT-W100 WebTV Internet Terminal", MACHINE_UNOFFICIAL )
CONS( 1996, wtv1phil,      0,      0, webtv1_philips, retail_input, webtv1_state, empty_init, "Philips-Magnavox",   "MAT960 WebTV Internet Terminal",   MACHINE_UNOFFICIAL )
