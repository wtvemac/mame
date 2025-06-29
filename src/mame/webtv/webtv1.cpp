// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#include "emu.h"

#include "bus/pc_kbd/keyboards.h"
#include "cpu/mips/mips3.h"
#include "machine/ds2401.h"
#include "machine/intelfsh.h"
#include "spot_asic.h"
#include "wtvdbg.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

class webtv1_state : public driver_device
{

public:

	enum cpu_type_t : uint8_t
	{
		MIPS_R4640_BE,
		MIPS_R4640_LE,
		MIPS_RM5230_BE,
		MIPS_RM5230_LE,
		MIPS_RM5231_BE,
		MIPS_RM5231_LE
	};

	static constexpr XTAL CPU_112MHZ = XTAL(56'448'000) * 2;
	static constexpr XTAL CPU_167MHZ = XTAL(41'539'000) * 4;
	static constexpr XTAL CPU_150MHZ = XTAL(75'000'000) * 2;
	static constexpr XTAL CPU_250MHZ = XTAL(75'000'000) * 4; // 300MHz for testing

	enum mem_size_t : uint32_t
	{
		MEM_0MB  = 0,
		MEM_1MB  = 1024 * 1024 *  1,
		MEM_2MB  = 1024 * 1024 *  2,
		MEM_4MB  = 1024 * 1024 *  4,
		MEM_8MB  = 1024 * 1024 *  8,
		MEM_16MB = 1024 * 1024 * 16,
		MEM_32MB = 1024 * 1024 * 32,
		MEM_64MB = 1024 * 1024 * 64
	};

	static constexpr uint32_t MAX_RAM_SIZE = mem_size_t::MEM_64MB;
	static constexpr uint32_t MAX_ROM_SIZE = mem_size_t::MEM_8MB;

	static constexpr uint16_t RAM_FLASHER_SIZE = 0x100;

	enum device_t : uint32_t
	{
		NONE              = 0x00000000,
		FUJITSU_1MB_FLASH = 0x00000001,
		AMD_2MB_FLASH     = 0x00000002,
		AMD_4MB_FLASH     = 0x00000004,
		MX_4MB_FLASH      = 0x00000008,
		FLASH             = 0x00000010,
		ROM               = 0x00000020,
		WEBTV_PORT        = 0x00000800,
		PEKOE             = 0x00200000,
		BANK0_IS_DIAG     = 0x10000000,
		BANK0_IS_BFE      = 0x20000000,
		CUSTOM_ADDRMAP    = 0x80000000
	};

	webtv1_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_mainram(*this, "mainram"), // shared pointer to ram()
		m_spotasic(*this, "spot"),
		m_serial_id(*this, "serial_id"),
		m_nvram(*this, "nvram"),
		m_bank0_flash0(*this, "bank0_flash0"), // labeled U0501, contains upper bits
		m_bank0_flash1(*this, "bank0_flash1"), // labeled U0502, contains lower bits
		m_bank1_flash0(*this, "bank1_flash0"), // labeled U0503, contains upper bits
		m_bank1_flash1(*this, "bank1_flash1"), // labeled U0504, contains lower bits
		m_pekoe(*this, "pekoe")
	{ }

	void base_init();

	void webtv1_bfe(machine_config& config);
	void webtv1_bf0(machine_config& config);
	void webtv1_pal(machine_config& config);
	void webtv1_dev(machine_config& config);
	void webtv1_dev2(machine_config& config);

protected:

	virtual void machine_start() override;
	virtual void machine_reset() override;

private:

	required_device<mips3_device> m_maincpu;
	required_shared_ptr<uint32_t> m_mainram;

	required_device<spot_asic_device> m_spotasic;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;

	required_device<intelfsh16_device> m_bank0_flash0;
	required_device<intelfsh16_device> m_bank0_flash1;
	optional_device<intelfsh16_device> m_bank1_flash0;
	optional_device<intelfsh16_device> m_bank1_flash1;

	optional_device<wtvdbg_pekoe_device> m_pekoe;

	webtv1_state::mem_size_t m_ram_size = webtv1_state::MEM_8MB;
	webtv1_state::mem_size_t m_rom_size = webtv1_state::MEM_2MB;
	webtv1_state::mem_size_t m_flash_size = webtv1_state::MEM_0MB;
	uint32_t m_device_config = webtv1_state::WEBTV_PORT;

	uint32_t m_reset_count;
	
	uint8_t ram_flasher[RAM_FLASHER_SIZE];

	void build_webtv_device(machine_config &config, webtv1_state::cpu_type_t cpu, XTAL cpu_clock, webtv1_state::mem_size_t ram_size, webtv1_state::mem_size_t rom_size, uint32_t chip_id, uint32_t sys_config, uint32_t device_config);

	void base_addrmap(address_map &map);

	uint8_t iic_sda_r();
	void iic_sda_w(uint8_t sda);

	void bank0_flash_w(offs_t offset, uint32_t data);
	uint32_t bank0_flash_r(offs_t offset);
	void bank1_flash_w(offs_t offset, uint32_t data);
	uint32_t bank1_flash_r(offs_t offset);
	uint8_t ram_flasher_r(offs_t offset);
	void ram_flasher_w(offs_t offset, uint8_t data);

};

void webtv1_state::build_webtv_device(machine_config &config, webtv1_state::cpu_type_t cpu, XTAL cpu_clock, webtv1_state::mem_size_t ram_size, webtv1_state::mem_size_t rom_size, uint32_t chip_id, uint32_t sys_config, uint32_t device_config)
{
	config.set_default_layout(layout_webtv);

	if (cpu == webtv1_state::MIPS_R4640_BE)
		R4640BE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv1_state::MIPS_R4640_LE)
		R4640LE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv1_state::MIPS_RM5230_BE || cpu == webtv1_state::MIPS_RM5231_BE)
		RM5230BE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv1_state::MIPS_RM5230_LE || cpu == webtv1_state::MIPS_RM5231_LE)
		RM5230LE(config, m_maincpu, cpu_clock);

	m_maincpu->set_icache_size(0x2000);
	m_maincpu->set_dcache_size(0x2000);

	m_ram_size = ram_size;
	m_rom_size = rom_size;
	m_flash_size = webtv1_state::MEM_0MB;
	m_device_config = device_config;

	//if (cpu == webtv1_state::MIPS_R4640_BE || cpu == webtv1_state::MIPS_R4640_LE)
	//	m_maincpu->add_fastram(0x00000000, (m_ram_size - 1), false, m_mainram);

	if (!(m_device_config & webtv1_state::CUSTOM_ADDRMAP))
		m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::base_addrmap);

	// Flash in bank1 only works if there's flash in bank0
	// This is just a requirment for this MAME driver, not nessisarly for hardware.

	if (m_device_config & webtv1_state::FUJITSU_1MB_FLASH)
	{
		FUJITSU_29F400T_16BIT(config, m_bank0_flash0, 0);
		FUJITSU_29F400T_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv1_state::MEM_1MB;

		if (!(m_device_config & webtv1_state::ROM))
		{
			FUJITSU_29F400T_16BIT(config, m_bank1_flash0, 0);
			FUJITSU_29F400T_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv1_state::AMD_2MB_FLASH)
	{
		AMD_29F800B_16BIT(config, m_bank0_flash0, 0);
		AMD_29F800B_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv1_state::MEM_2MB;

		if (!(m_device_config & webtv1_state::ROM))
		{
			AMD_29F800B_16BIT(config, m_bank1_flash0, 0);
			AMD_29F800B_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv1_state::AMD_4MB_FLASH)
	{
		AMD_29LV160B_16BIT(config, m_bank0_flash0, 0);
		AMD_29LV160B_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv1_state::MEM_4MB;

		if (!(m_device_config & webtv1_state::ROM))
		{
			AMD_29LV160B_16BIT(config, m_bank1_flash0, 0);
			AMD_29LV160B_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv1_state::MX_4MB_FLASH)
	{
		MACRONIX_29F1610_16BIT(config, m_bank0_flash0, 0);
		MACRONIX_29F1610_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv1_state::MEM_4MB;

		if (!(m_device_config & webtv1_state::ROM))
		{
			MACRONIX_29F1610_16BIT(config, m_bank1_flash0, 0);
			MACRONIX_29F1610_16BIT(config, m_bank1_flash1, 0);
		}
	}

	DS2401(config, m_serial_id, 0);

	I2C_24C01(config, m_nvram, 0);

	XTAL bus_clock = cpu_clock / 2;

	m_maincpu->set_system_clock(bus_clock.value());

	SPOT_ASIC(config, m_spotasic, bus_clock, chip_id, sys_config);
	m_spotasic->set_serial_id(m_serial_id);
	m_spotasic->sda_r_callback().set(FUNC(webtv1_state::iic_sda_r));
	m_spotasic->sda_w_callback().set(FUNC(webtv1_state::iic_sda_w));

	if (m_device_config & webtv1_state::PEKOE)
		WTV_PEKOEDBG(config, m_pekoe);
}

void webtv1_state::base_addrmap(address_map &map)
{
	map.global_mask(0x1fffffff);

	uint32_t usable_ram_size = std::min((uint32_t)m_ram_size, MAX_RAM_SIZE);
	if (usable_ram_size == MAX_RAM_SIZE)
		map(0x00000000, (usable_ram_size - 1)).ram().share("mainram");
	else
		map(0x00000000, (usable_ram_size - 1)).ram().mirror(MAX_RAM_SIZE - usable_ram_size).share("mainram");

	map(0x04000000, 0x0400ffff).m(m_spotasic, FUNC(spot_asic_device::map));

	if (m_device_config & webtv1_state::PEKOE)
	{
		map(0x00f00000, 0x00f0001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04800000, 0x0480001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04804000, 0x0480401f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04f00000, 0x04f0001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
	}

	if (m_flash_size > 0)
	{
		if (m_device_config & webtv1_state::BANK0_IS_DIAG)
			map(0x1f400000, 0x1fffffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).mask(m_flash_size - 1).share("bank0");
		if (m_device_config & webtv1_state::BANK0_IS_BFE)
			map(0x1fe00000, 0x1fffffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).mask(m_flash_size - 1).share("bank0");
		else
			map(0x1f000000, 0x1f4fffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).share("bank0");
	}

	uint32_t usable_rom_size = std::min((uint32_t)m_rom_size, MAX_ROM_SIZE);
	uint32_t rom_adr_start = 0x1f800000;
	if (m_device_config & webtv1_state::BANK0_IS_BFE)
		rom_adr_start = 0x1fc00000;
	uint32_t rom_adr_end = rom_adr_start + (usable_rom_size - 1);
	if (m_device_config & webtv1_state::ROM && (usable_rom_size == MAX_ROM_SIZE || m_device_config & webtv1_state::BANK0_IS_DIAG || m_device_config & webtv1_state::BANK0_IS_BFE))
		map(rom_adr_start, rom_adr_end).region("bank1", 0x000000).rom();
	else if (m_device_config & webtv1_state::ROM)
		map(rom_adr_start, rom_adr_end).region("bank1", 0x000000).rom().mirror(MAX_ROM_SIZE - usable_rom_size);
	else if (m_device_config & webtv1_state::BANK0_IS_DIAG || m_device_config & webtv1_state::BANK0_IS_BFE)
		map(rom_adr_start, rom_adr_end).rw(FUNC(webtv1_state::bank1_flash_r), FUNC(webtv1_state::bank1_flash_w)).share("bank1");
	else
		map(rom_adr_start, 0x1fffffff).rw(FUNC(webtv1_state::bank1_flash_r), FUNC(webtv1_state::bank1_flash_w)).mask(usable_rom_size - 1).share("bank1");

	// The RAM flash code gets mirrored across the entire RAM region.
	for (uint32_t ram_flasher_base = 0x00000000; ram_flasher_base < MAX_RAM_SIZE; ram_flasher_base += m_ram_size)
		map(ram_flasher_base, ram_flasher_base + (RAM_FLASHER_SIZE - 1)).rw(FUNC(webtv1_state::ram_flasher_r), FUNC(webtv1_state::ram_flasher_w));
}

void webtv1_state::base_init()
{
	m_maincpu->mips3drc_set_options(MIPS3DRC_FASTEST_OPTIONS);
}

void webtv1_state::machine_start()
{
	m_reset_count = 0x00000000;
}

void webtv1_state::machine_reset()
{
	m_reset_count++;

	if (m_reset_count == 1)
	{
		game_driver driver = machine().system();

		if (strcmp(driver.name, "wtv1bfe") == 0)
			popmessage("NOTE: WebTV starts with the display off. Press F3 to power on for pre-alpha builds or F1 to power on for other builds.");
		else
			popmessage("NOTE: WebTV starts with the display off. Press F1 to power on.");
	}
}

//
// WebTV can store the approm across flash chips. The approm firmware is upgradable over a network.
//
// There's two 16-bit flash chips striped across a 32-bit bus. The chip with the upper 16-bits is labeled U0501, and lower is U0502.
//
// WebTV supports flash configurations to allow 1MB (2 x 4Mbit chips), 2MB (2 x 8Mbit chips) and 4MB (2 x 16Mbit chips) approm images.
// The 1MB flash configuration seems possible but it's unknown if it was used, 2MB flash configuration was released to the public
// and it seemed a 4MB flash configuration was used for debug builds during approm development, for a prototype Japan box and for the WLD100 box.
//
// The flash chips are usually addressed as "bank0" which gives a max of 4MB of flash. That can be doubled up to 8MB if you use "bank1" which is 
// usually reserved for the BootROM on mask ROM chips.
//
// 1MB:          AM29F400AT  + AM29F400AT (citation needed)
// Production:   AM29F800BT  + AM29F800BT
// 4MB debug/JP: MX29F1610   + MX29F1610  (citation needed)
// 8MB WLD100:   AM29LV160BB + AM29LV160BB (x2 across bank1 for 8MB)
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
//    AM29LV160BT: top bs,    3v 16Mbit device id=0x22c4
//    AM29LV160BB: bottom bs, 3v 16Mbit device id=0x22c4 (have MAME support)
//
// Macronix:
//    MX29F1610:   top bs,    5v 16Mbit device id=0x00f1 (have MAME support)
//
// bank0_flash0 = U0501
// bank0_flash1 = U0502
// bank1_flash0 = U0503 (if flash, usually ROM)
// bank1_flash1 = U0504 (if flash, usually ROM)
//
// NOTE: if you dump these chips you may need to byte swap the output.
//       The bus is big-endian when the CPU is in big-endian mode.
//

uint32_t webtv1_state::bank0_flash_r(offs_t offset)
{
	//printf("READ: offset=%08x\n", offset);
	uint16_t upper_value = m_bank0_flash0->read(offset);
	uint16_t lower_value = m_bank0_flash1->read(offset);

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
		return (upper_value << 0x10) | (lower_value << 0x00);
	else
		return (lower_value << 0x10) | (upper_value << 0x00);
}

void webtv1_state::bank0_flash_w(offs_t offset, uint32_t data)
{
	//printf("WRITE: offset=%08x\n", offset);
	uint16_t upper_value = data >> 0x10;
	uint16_t lower_value = data >> 0x00;

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
	{
		m_bank0_flash0->write(offset, upper_value & 0xffff);
		m_bank0_flash1->write(offset, lower_value & 0xffff);
	}
	else
	{
		m_bank0_flash0->write(offset, lower_value & 0xffff);
		m_bank0_flash1->write(offset, upper_value & 0xffff);
	}
}

uint32_t webtv1_state::bank1_flash_r(offs_t offset)
{
	uint16_t upper_value = m_bank1_flash0->read(offset);
	uint16_t lower_value = m_bank1_flash1->read(offset);

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
		return (upper_value << 0x10) | (lower_value << 0x00);
	else
		return (lower_value << 0x10) | (upper_value << 0x00);
}

void webtv1_state::bank1_flash_w(offs_t offset, uint32_t data)
{
	uint16_t upper_value = data >> 0x10;
	uint16_t lower_value = data >> 0x00;

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
	{
		m_bank1_flash0->write(offset, upper_value & 0xffff);
		m_bank1_flash1->write(offset, lower_value & 0xffff);
	}
	else
	{
		m_bank1_flash0->write(offset, lower_value & 0xffff);
		m_bank1_flash1->write(offset, upper_value & 0xffff);
	}
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

// There's few known devices that can sit on this bus:
//
//	0x8c/0x8d	Philips SAA7187 video encoder
//				Used for the S-Video and composite out
//	0xa0/0xa1	Atmel AT24C01A EEPROM NVRAM
//				Used for the encryption shared secret (0x14), crash log counter (0x23) and other settings
//

uint8_t webtv1_state::iic_sda_r()
{
	int sda_bit = 1;

	sda_bit &= (m_nvram->read_sda()) & 0x1;

	return sda_bit;
}

void webtv1_state::iic_sda_w(uint8_t sda)
{
	uint8_t scl = m_spotasic->scl_r();

	m_nvram->write_sda(sda);
	m_nvram->write_scl(scl);
}

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv1_state::webtv1_bfe(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_112MHZ,
		MEM_2MB,
		MEM_2MB,
		0x01010000, // citation needed
		0x2becbf8f, // citation needed
		webtv1_state::ROM | webtv1_state::AMD_2MB_FLASH | webtv1_state::BANK0_IS_BFE | webtv1_state::WEBTV_PORT
	);
}

ROM_START(wtv1bfe)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bfe-boot", "Prototype bfe BootROM (1.0, build 105)")
	ROMX_LOAD("bfe-boot.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "prealpha-boot", "Pre-alpha bfe BootROM")
	ROMX_LOAD("prealpha-boot.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv1_state::webtv1_bf0(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_112MHZ,
		MEM_2MB,
		MEM_2MB,
		0x01010000,
		0x2becbf8f,
		webtv1_state::ROM | webtv1_state::AMD_2MB_FLASH | webtv1_state::WEBTV_PORT
	);
}

ROM_START(wtv1bf0)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP)

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

// This isn't a know box. It's a pretend box using experimental parameters.

void webtv1_state::webtv1_pal(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_112MHZ,
		MEM_4MB,
		MEM_2MB,
		0x01010000, // citation needed
		0x2becb78f, // citation needed
		webtv1_state::ROM | webtv1_state::MX_4MB_FLASH | webtv1_state::WEBTV_PORT
	);
}

ROM_START(wtv1pal)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP)

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

// This isn't a know box. It's a pretend box using experimental parameters.

void webtv1_state::webtv1_dev(machine_config& config)
{
	// chip=0x03120000, sysconfig=0x3f8700: build 2.0 2046
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_112MHZ,
		MEM_4MB,
		MEM_2MB,
		0x01010000, // citation needed
		0x2becbf8f, // citation needed
		webtv1_state::ROM | webtv1_state::MX_4MB_FLASH | webtv1_state::WEBTV_PORT | webtv1_state::PEKOE
	);
}

ROM_START(wtv1dev)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP)

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

// This isn't a know box. It's a pretend box using experimental parameters.

void webtv1_state::webtv1_dev2(machine_config& config)
{
	// chip=0x03120000, sysconfig=0x3f8700: build 2.0 2046
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_112MHZ,
		MEM_2MB,
		MEM_2MB,
		0x01010000, // citation needed
		0x2becbf8f, // citation needed
		webtv1_state::ROM | webtv1_state::AMD_2MB_FLASH | webtv1_state::WEBTV_PORT | webtv1_state::PEKOE
	);
}

ROM_START(wtv1dv2)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Standard bf0 Retail BootROM (1.0, build 105)")
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP)

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

//   YEAR  NAME      PARENT  COMPAT  MACHINE         INPUT CLASS         INIT       COMPANY  FULLNAME                                  FLAGS
CONS(1996, wtv1bfe,       0,      0, webtv1_bfe,     0,    webtv1_state, base_init, "WebTV", "WebTV 1: bfeapp Protoype Box",           MACHINE_UNOFFICIAL )
CONS(1996, wtv1bf0,       0,      0, webtv1_bf0,     0,    webtv1_state, base_init, "WebTV", "WebTV 1: Classic | MAT960 / INT-W100",   MACHINE_UNOFFICIAL )
// Can add child machines here if you want to split out wtv1bf0
CONS(1996, wtv1pal,       0,      0, webtv1_pal,     0,    webtv1_state, base_init, "WebTV", "WebTV 1: PAL Box",                       MACHINE_UNOFFICIAL )
CONS(1996, wtv1dev,       0,      0, webtv1_dev,     0,    webtv1_state, base_init, "WebTV", "WebTV 1: Classic | 4MB Development Box", MACHINE_UNOFFICIAL )
CONS(1996, wtv1dv2,       0,      0, webtv1_dev2,    0,    webtv1_state, base_init, "WebTV", "WebTV 1: Classic | 2MB Development Box", MACHINE_UNOFFICIAL )
// Can add other machines here like the JP box (can use wtv1dv2 for that?)
