// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#include "emu.h"

#include "bus/pc_kbd/keyboards.h"
#include "cpu/mips/mips3.h"
#include "machine/ds2401.h"
#include "machine/intelfsh.h"
#include "bus/ata/ataintf.h"
#include "mdoc.h"
#include "solo_asic.h"
#include "han_asic.h"
#include "fud_asic.h"
#include "wtvdbg.h"
#include "wtvtuner.h"
#include "wtvvidstream.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

class webtv2_state : public driver_device
{

public:

	static constexpr uint32_t FUD_GPIO_BASE    = 0x04860000;

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
		DISK              = 0x00000040,
		FLASHDISK         = 0x00000080,
		HAN               = 0x00000100,
		HWMODEM           = 0x00000200,
		SWMODEM           = 0x00000400,
		WEBTV_PORT        = 0x00000800,
		ESTAR_SAT_TUNER   = 0x00001000,
		DTV01_SAT_TUNER   = 0x00002000, // Can use the L64734 -OR- CX24110 tuner. L64734 was used in retail boxes but had dendrite corrosion that fubared the chip over time.
		NTSCM_CBL_TUNER   = 0x00004000,
		NTSCJ_CBL_TUNER   = 0x00008000,
		BT827_MEDIA_IN    = 0x00010000,
		BT835_MEDIA_IN    = 0x00020000,
		FUD               = 0x00080000,
		PEKOE             = 0x00200000,
		BANK0_IS_DIAG     = 0x10000000,
		BANK0_IS_BFE      = 0x20000000,
		CUSTOM_ADDRMAP    = 0x80000000
	};

	webtv2_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_mainram(*this, "mainram"), // shared pointer to ram()
		m_soloasic(*this, "solo"),
		m_serial_id(*this, "serial_id"),
		m_nvram(*this, "nvram"),
		m_bank0_flash0(*this, "bank0_flash0"), // labeled U0501, contains upper bits
		m_bank0_flash1(*this, "bank0_flash1"), // labeled U0502, contains lower bits
		m_bank1_flash0(*this, "bank1_flash0"), // labeled U0503, contains upper bits
		m_bank1_flash1(*this, "bank1_flash1"), // labeled U0504, contains lower bits
		m_mdoc(*this, "mdoc"),
		m_ata(*this, "ata"),
		m_hanasic(*this, "han"),
		m_fudasic(*this, "fud"),
		m_tuner(*this, "tuner"),
		m_l64734_tun0(*this, "l64734_tun0"),
		m_l64734_tun1(*this, "l64734_tun1"),
		m_bt827_in(*this, "bt827_in"),
		m_bt835_in(*this, "bt835_in"),
		m_pekoe(*this, "pekoe")
	{
	}

	void base_init();

	void webtv2_lc2(machine_config& config);
	void webtv2_dev(machine_config& config);
	void webtv2_jpp(machine_config& config);
	void webtv2_jpc(machine_config& config);
	void webtv2_derby(machine_config& config);
	void webtv2_wld(machine_config& config);
	void webtv2_newplus(machine_config& config);
	void webtv2_newclsc(machine_config& config);
	void webtv2_estar(machine_config& config);
	void webtv2_utv(machine_config& config);

protected:

	virtual void machine_start() override;
	virtual void machine_reset() override;

private:

	required_device<mips3_device> m_maincpu;
	required_shared_ptr<uint32_t> m_mainram;

	required_device<solo_asic_device> m_soloasic;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;

	optional_device<intelfsh16_device> m_bank0_flash0;
	optional_device<intelfsh16_device> m_bank0_flash1;
	optional_device<intelfsh16_device> m_bank1_flash0;
	optional_device<intelfsh16_device> m_bank1_flash1;

	optional_device<mdoc_collection> m_mdoc;

	optional_device<ata_interface_device> m_ata;

	optional_device<han_asic_device> m_hanasic;
	optional_device<fud_asic_device> m_fudasic;

	optional_device<generic_tuner_device> m_tuner;
	optional_device<l64734_tuner_device> m_l64734_tun0;
	optional_device<l64734_tuner_device> m_l64734_tun1;

	optional_device<bt827_decoder_device> m_bt827_in;
	optional_device<bt835_decoder_device> m_bt835_in;

	optional_device<wtvdbg_pekoe_device> m_pekoe;

	webtv2_state::mem_size_t m_ram_size = webtv2_state::MEM_8MB;
	webtv2_state::mem_size_t m_rom_size = webtv2_state::MEM_2MB;
	webtv2_state::mem_size_t m_flash_size = webtv2_state::MEM_0MB;
	uint32_t m_device_config = webtv2_state::DISK | webtv2_state::HWMODEM | webtv2_state::NTSCM_CBL_TUNER | webtv2_state::BT827_MEDIA_IN | webtv2_state::WEBTV_PORT;

 	uint32_t m_reset_count;

	uint8_t ram_flasher[RAM_FLASHER_SIZE];

	void build_webtv_device(machine_config &config, webtv2_state::cpu_type_t cpu, XTAL cpu_clock, webtv2_state::mem_size_t ram_size, webtv2_state::mem_size_t rom_size, uint32_t chip_id, uint32_t sys_config, uint32_t device_config);

	void base_addrmap(address_map &map);
	
	void reset_hack(int state);
	
	uint8_t iic_sda_r();
	void iic_sda_w(uint8_t sda);

	void wld_addrmap(address_map &map);

	void bank0_flash_w(offs_t offset, uint32_t data);
	uint32_t bank0_flash_r(offs_t offset);
	void bank1_flash_w(offs_t offset, uint32_t data);
	uint32_t bank1_flash_r(offs_t offset);
	uint8_t ram_flasher_r(offs_t offset);
	void ram_flasher_w(offs_t offset, uint8_t data);

};

void webtv2_state::build_webtv_device(machine_config &config, webtv2_state::cpu_type_t cpu, XTAL cpu_clock, webtv2_state::mem_size_t ram_size, webtv2_state::mem_size_t rom_size, uint32_t chip_id, uint32_t sys_config, uint32_t device_config)
{
	config.set_default_layout(layout_webtv);

	if (cpu == webtv2_state::MIPS_R4640_BE)
		R4640BE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv2_state::MIPS_R4640_LE)
		R4640LE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv2_state::MIPS_RM5230_BE || cpu == webtv2_state::MIPS_RM5231_BE)
		RM5230BE(config, m_maincpu, cpu_clock);
	else if (cpu == webtv2_state::MIPS_RM5230_LE || cpu == webtv2_state::MIPS_RM5231_LE)
		RM5230LE(config, m_maincpu, cpu_clock);

	m_maincpu->set_icache_size(0x2000);
	m_maincpu->set_dcache_size(0x2000);

	m_ram_size = ram_size;
	m_rom_size = rom_size;
	m_flash_size = webtv2_state::MEM_0MB;
	m_device_config = device_config;

	//if (cpu == webtv2_state::MIPS_R4640_BE || cpu == webtv2_state::MIPS_R4640_LE)
	//	m_maincpu->add_fastram(0x00000000, (m_ram_size - 1), false, m_mainram);

	if (!(m_device_config & webtv2_state::CUSTOM_ADDRMAP))
		m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::base_addrmap);

	if (m_device_config & webtv2_state::DISK)
	{
		ATA_INTERFACE(config, m_ata).options(ata_devices, "hdd", nullptr, true);
		if (m_device_config & webtv2_state::HAN)
		{
			m_ata->irq_handler().set(m_hanasic, FUNC(han_asic_device::irq_ide1_w));
		}
		else
		{
			m_ata->irq_handler().set(m_soloasic, FUNC(solo_asic_device::irq_ide1_w));
			m_ata->dmarq_handler().set(m_soloasic, FUNC(solo_asic_device::dmarq_ide1_w));
		}
	}

	// Flash in bank1 only works if there's flash in bank0
	// This is just a requirment for this MAME driver, not nessisarly for hardware.

	if (m_device_config & webtv2_state::FUJITSU_1MB_FLASH)
	{
		FUJITSU_29F400T_16BIT(config, m_bank0_flash0, 0);
		FUJITSU_29F400T_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv2_state::MEM_1MB;

		if (!(m_device_config & webtv2_state::ROM))
		{
			FUJITSU_29F400T_16BIT(config, m_bank1_flash0, 0);
			FUJITSU_29F400T_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv2_state::AMD_2MB_FLASH)
	{
		AMD_29F800B_16BIT(config, m_bank0_flash0, 0);
		AMD_29F800B_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv2_state::MEM_2MB;

		if (!(m_device_config & webtv2_state::ROM))
		{
			AMD_29F800B_16BIT(config, m_bank1_flash0, 0);
			AMD_29F800B_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv2_state::AMD_4MB_FLASH)
	{
		AMD_29LV160B_16BIT(config, m_bank0_flash0, 0);
		AMD_29LV160B_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv2_state::MEM_4MB;

		if (!(m_device_config & webtv2_state::ROM))
		{
			AMD_29LV160B_16BIT(config, m_bank1_flash0, 0);
			AMD_29LV160B_16BIT(config, m_bank1_flash1, 0);
		}
	}
	else if (m_device_config & webtv2_state::MX_4MB_FLASH)
	{
		MACRONIX_29F1610_16BIT(config, m_bank0_flash0, 0);
		MACRONIX_29F1610_16BIT(config, m_bank0_flash1, 0);
		m_flash_size = webtv2_state::MEM_4MB;

		if (!(m_device_config & webtv2_state::ROM))
		{
			MACRONIX_29F1610_16BIT(config, m_bank1_flash0, 0);
			MACRONIX_29F1610_16BIT(config, m_bank1_flash1, 0);
		}
	}

	if (m_device_config & webtv2_state::FLASHDISK)
	{
		MDOC_COLLECTION(config, m_mdoc, 0, [](mdoc_collection &mdoc, machine_config &dconfig) {
			mdoc.add_chip(dconfig, 0, 0, MD2810_D08);
		});
	}

	if (m_device_config & webtv2_state::DTV01_SAT_TUNER)
	{
		L64734(config, m_l64734_tun0, 0x18);
		L64734(config, m_l64734_tun1, 0x1a);
	}

	if (m_device_config & webtv2_state::NTSCM_CBL_TUNER || m_device_config & webtv2_state::NTSCJ_CBL_TUNER)
		TUNER(config, m_tuner, 0x88); // Works for now but need to correct.

	if (m_device_config & webtv2_state::BT827_MEDIA_IN)
		BT827(config, m_bt827_in, 0x88);

	if (m_device_config & webtv2_state::BT835_MEDIA_IN)
		BT835(config, m_bt835_in, 0x88);

	DS2401(config, m_serial_id, 0);

	I2C_24C01(config, m_nvram, 0);

	XTAL bus_clock = cpu_clock / 2;

	m_maincpu->set_system_clock(bus_clock.value());

	SOLO_ASIC(config, m_soloasic, bus_clock, chip_id, sys_config);
	m_soloasic->set_serial_id(m_serial_id);
	m_soloasic->set_ata(m_ata);
	m_soloasic->reset_hack_callback().set(FUNC(webtv2_state::reset_hack));
	m_soloasic->sda_r_callback().set(FUNC(webtv2_state::iic_sda_r));
	m_soloasic->sda_w_callback().set(FUNC(webtv2_state::iic_sda_w));

	if (m_device_config & webtv2_state::HAN)
	{
		HAN_ASIC(config, m_hanasic);
		m_hanasic->set_ata(m_ata);
	}

	if (m_device_config & webtv2_state::FUD)
		FUD_ASIC(config, m_fudasic, FUD_GPIO_BASE);

	if (m_device_config & webtv2_state::PEKOE)
		WTV_PEKOEDBG(config, m_pekoe);
}

void webtv2_state::base_addrmap(address_map &map)
{
	map.global_mask(0x1fffffff);

	uint32_t usable_ram_size = std::min((uint32_t)m_ram_size, MAX_RAM_SIZE);
	if (usable_ram_size == MAX_RAM_SIZE)
		map(0x00000000, (usable_ram_size - 1)).ram().share("mainram");
	else
		map(0x00000000, (usable_ram_size - 1)).ram().mirror(MAX_RAM_SIZE - usable_ram_size).share("mainram");

	map(0x04000000, 0x0400ffff).m(m_soloasic, FUNC(solo_asic_device::map));

	if (m_device_config & webtv2_state::DISK)
	{
		if (m_device_config & webtv2_state::HAN)
			map(0x10000000, 0x10000303).m(m_hanasic, FUNC(han_asic_device::map));
		else
			map(0x1e400000, 0x1e80001f).m(m_soloasic, FUNC(solo_asic_device::ide_map));

		// There's another internal SOLO IDE controller @ 0x1d400000 with RIO device 2 interrupts.
		// If you want to define another IDE drive you can use a custom addrmap for the device.
	}

	if (m_device_config & webtv2_state::FLASHDISK)
		map(0x1ec00000, 0x1ec07fff).rw(m_mdoc, FUNC(mdoc_collection::read32), FUNC(mdoc_collection::write32));

	if (m_device_config & webtv2_state::HWMODEM)
		map(0x1e000000, 0x1e00001f).m(m_soloasic, FUNC(solo_asic_device::hardware_modem_map));

	if (m_device_config & webtv2_state::PEKOE)
	{
		map(0x00f00000, 0x00f0001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04800000, 0x0480001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04804000, 0x0480401f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
		map(0x04f00000, 0x04f0001f).m(m_pekoe, FUNC(wtvdbg_pekoe_device::map));
	}

	if (m_device_config & webtv2_state::FUD)
	{
		map(0x00800cf0, 0x00800cff).m(m_fudasic, FUNC(fud_asic_device::cntl_map));
		map(0x10000cf0, 0x10000cff).m(m_fudasic, FUNC(fud_asic_device::cntl_map));
		map(FUD_GPIO_BASE, FUD_GPIO_BASE + 0x3f).m(m_fudasic, FUNC(fud_asic_device::gpio_map));
	}

	if (m_flash_size > 0)
	{
		if (m_device_config & webtv2_state::BANK0_IS_DIAG)
			map(0x1f400000, 0x1fffffff).rw(FUNC(webtv2_state::bank0_flash_r), FUNC(webtv2_state::bank0_flash_w)).mask(m_flash_size - 1).share("bank0");
		else
			map(0x1f000000, 0x1f3fffff).rw(FUNC(webtv2_state::bank0_flash_r), FUNC(webtv2_state::bank0_flash_w)).mask(m_flash_size - 1).share("bank0");
	}

	uint32_t usable_rom_size = std::min((uint32_t)m_rom_size, MAX_ROM_SIZE);
	uint32_t rom_adr_start = 0x1f800000;
	if (m_device_config & webtv2_state::BANK0_IS_BFE)
		rom_adr_start = 0x1fc00000;
	uint32_t rom_adr_end = rom_adr_start + (usable_rom_size - 1);
	if (m_device_config & webtv2_state::ROM && (usable_rom_size == MAX_ROM_SIZE || m_device_config & webtv2_state::BANK0_IS_DIAG || m_device_config & webtv2_state::BANK0_IS_BFE))
		map(rom_adr_start, rom_adr_end).region("bank1", 0x000000).rom();
	else if (m_device_config & webtv2_state::ROM)
		map(rom_adr_start, rom_adr_end).region("bank1", 0x000000).rom().mirror(MAX_ROM_SIZE - usable_rom_size);
	else if (m_device_config & webtv2_state::BANK0_IS_DIAG || m_device_config & webtv2_state::BANK0_IS_BFE)
		map(rom_adr_start, rom_adr_end).rw(FUNC(webtv2_state::bank1_flash_r), FUNC(webtv2_state::bank1_flash_w)).share("bank1");
	else
		map(rom_adr_start, 0x1fffffff).rw(FUNC(webtv2_state::bank1_flash_r), FUNC(webtv2_state::bank1_flash_w)).mask(usable_rom_size - 1).share("bank1");

	// The RAM flash code gets mirrored across the entire RAM region.
	for (uint32_t ram_flasher_base = 0x00000000; ram_flasher_base < MAX_RAM_SIZE; ram_flasher_base += m_ram_size)
		map(ram_flasher_base, ram_flasher_base + (RAM_FLASHER_SIZE - 1)).rw(FUNC(webtv2_state::ram_flasher_r), FUNC(webtv2_state::ram_flasher_w));
}

void webtv2_state::base_init()
{
	m_maincpu->mips3drc_set_options(MIPS3DRC_FASTEST_OPTIONS);
}

void webtv2_state::machine_start()
{
	m_reset_count = 0x00000000;
}

void webtv2_state::machine_reset()
{
	m_reset_count++;

	game_driver driver = machine().system();

	if (m_reset_count <= 3 && (strcmp(driver.name, "wtv2utv") == 0 || strcmp(driver.name, "wtv2utvdev") == 0))
	{
		popmessage("NOTE: Startup may take several minutes.\nPress F1 to power on once the power LED stops blinking.\n");
	}
	else if (m_reset_count == 1)
	{
		if ((strcmp(driver.name, "wtv2jpc") == 0 || strcmp(driver.name, "wtv2jpp") == 0) && (m_serial_id->direct_read(4) & 0xf0) == 0x10)
			popmessage("NOTE: Press F1 to power on once the power LED stops blinking.\nAlso, your Philips Magnavox SSID may cause connection issues. Try enabling audible dialing or connecting from the minibrowser if you have issues.");
		else
			popmessage("NOTE: Press F1 to power on once the power LED stops blinking.");
	}
}

void webtv2_state::reset_hack(int state)
{
	if (state && m_device_config & webtv2_state::HAN)
		m_hanasic->prepare_for_reset();
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

uint32_t webtv2_state::bank0_flash_r(offs_t offset)
{
	uint16_t upper_value = m_bank0_flash0->read(offset);
	uint16_t lower_value = m_bank0_flash1->read(offset);

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
		return (upper_value << 0x10) | (lower_value << 0x00);
	else
		return (lower_value << 0x10) | (upper_value << 0x00);
}

void webtv2_state::bank0_flash_w(offs_t offset, uint32_t data)
{
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

uint32_t webtv2_state::bank1_flash_r(offs_t offset)
{
	uint16_t upper_value = m_bank1_flash0->read(offset);
	uint16_t lower_value = m_bank1_flash1->read(offset);

	if (m_maincpu->get_endianness() == ENDIANNESS_BIG)
		return (upper_value << 0x10) | (lower_value << 0x00);
	else
		return (lower_value << 0x10) | (upper_value << 0x00);
}

void webtv2_state::bank1_flash_w(offs_t offset, uint32_t data)
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

// There's a few known devices that can sit on this bus:
//
//	Address		Device
//	0x18/0x19	UltimateTV's LSI L64734 -OR- Conexant CX24110 tuner 0
//	0x1a/0x1b	UltimateTV's LSI L64734 -OR- Conexant CX24110 tuner 1
//	0x88/0x89	Rockwell/BrookTree Bt827 or Rockwell/BrookTree Bt835 media in/out
//	0x8c/0x8d	Philips SAA7187 video encoder
//				Used for the S-Video and composite out
//	0xa0/0xa1	Atmel AT24C01A EEPROM NVRAM
//				Used for the encryption shared secret (0x14), crash log counter (0x23) and other settings
//	0xb6/0xb7	9850 audio encoder
//	0xc2		ALPS tuner
//

uint8_t webtv2_state::iic_sda_r()
{
	int sda_bit = 1;

	sda_bit &= (m_nvram->read_sda()) & 0x1;

	if (m_device_config & webtv2_state::NTSCM_CBL_TUNER || m_device_config & webtv2_state::NTSCJ_CBL_TUNER)
		sda_bit &= (m_tuner->sda_read()) & 0x1;

	if (m_device_config & webtv2_state::DTV01_SAT_TUNER)
	{
		sda_bit &= (m_l64734_tun0->sda_read()) & 0x1;
		sda_bit &= (m_l64734_tun1->sda_read()) & 0x1;
	}

	if (m_device_config & webtv2_state::BT827_MEDIA_IN)
	{
		sda_bit &= (m_bt827_in->sda_read()) & 0x1;
	}

	if (m_device_config & webtv2_state::BT835_MEDIA_IN)
	{
		sda_bit &= (m_bt835_in->sda_read()) & 0x1;
	}

	return sda_bit;
}

void webtv2_state::iic_sda_w(uint8_t sda)
{
	uint8_t scl = m_soloasic->scl_r();

	m_nvram->write_sda(sda);
	m_nvram->write_scl(scl);

	if (m_device_config & webtv2_state::NTSCM_CBL_TUNER || m_device_config & webtv2_state::NTSCJ_CBL_TUNER)
	{
		m_tuner->sda_write(sda);
		m_tuner->scl_write(scl);
	}

	if (m_device_config & webtv2_state::DTV01_SAT_TUNER)
	{
		m_l64734_tun0->sda_write(sda);
		m_l64734_tun0->scl_write(scl);

		m_l64734_tun1->sda_write(sda);
		m_l64734_tun1->scl_write(scl);
	}

	if (m_device_config & webtv2_state::BT827_MEDIA_IN)
	{
		m_bt827_in->sda_write(sda);
		m_bt827_in->scl_write(scl);
	}

	if (m_device_config & webtv2_state::BT835_MEDIA_IN)
	{
		m_bt835_in->sda_write(sda);
		m_bt835_in->scl_write(scl);
	}
}

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_lc2(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_167MHZ,
		MEM_8MB,
		MEM_2MB,
		0x03120000,
		0x003f8700,
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::HWMODEM | webtv2_state::NTSCM_CBL_TUNER | webtv2_state::BT827_MEDIA_IN | webtv2_state::WEBTV_PORT
	);
}

ROM_START(wtv2lc2)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "LC2 BootROM (2.0, build 2046)")
	ROMX_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "joebrom", "Joe Britt's Dev BootROM (12/19/1997)")
	ROMX_LOAD("joebrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvhdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_jpp(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_RM5231_BE,
		CPU_167MHZ,
		MEM_16MB,
		MEM_4MB,
		0x03120000, // citation needed
		0x003f8700, // citation needed
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::HWMODEM | webtv2_state::NTSCJ_CBL_TUNER | webtv2_state::BT827_MEDIA_IN
	);
}

ROM_START(wtv2jpp)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Japan Plus BootROM (2.0.2, build 3612)")
	ROM_LOAD("bootrom.o", 0x000000, 0x400000, NO_DUMP)

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvjphdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_jpc(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_R4640_BE,
		CPU_167MHZ,
		MEM_8MB,
		MEM_4MB,
		0x03120000, // citation needed
		0x003f8700, // citation needed
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::HWMODEM | webtv2_state::WEBTV_PORT
	);
}

ROM_START(wtv2jpc)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Japan Classic BootROM (2.0J, build 2135)")
	ROM_LOAD("bootrom.o", 0x000000, 0x400000, NO_DUMP)

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvjchdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_derby(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_RM5231_BE,
		CPU_167MHZ,
		MEM_8MB,
		MEM_2MB,
		0x03120000, // citation needed
		0x003f8700, // citation needed
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::SWMODEM | webtv2_state::NTSCM_CBL_TUNER | webtv2_state::BT827_MEDIA_IN | webtv2_state::WEBTV_PORT
	);
}

ROM_START(wtv2drb)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Derby BootROM (2.0.1, build 2243)")
	ROMX_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "dbugrom", "Debug Derby BootROM (2.0.1, build 2243)")
	ROMX_LOAD("dbugrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvdhdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

//
// From original flash dump July 4th 2024:
//
// Working Memory Map:
//
//     0xbf400000-0xbf410000: Bootloader/ROM Manager (4kB; U0501+U0502)
//         First few instructions are mapped to the MIPS reset vector @ 0xbfc00000
//         After the MIPS reset vector is called, this section is loaded into memory @ 0x80fe0250 and then jumps to 0x80fe0770 where it tries to load an OS
//
//         0xbfc00000-0xbfc00520: MIPS start code
//             This does initial RAM setup and loads this section into RAM @ 0x80fe0250
//         0xbf400520-0xbf408000: ROM Manager code
//             This is ran from RAM (0x80fe0770-0x80fe8250)
//             This does the initial hardware setup and then boots an OS from flash (either the upgrade or browser OS)
//             There seems to be boot flags @ 0xbf406ea0 (or 0x80fe70f0 in RAM) compiled in. Boot flags usage tbd
//         0xbf408000-0xbf40c000: EraseBlock 0
//             No idea what this is for. Different between MattMan's and Ryders box
//             It's called "EraseBlock 0" in the ROM Manager code and has a 0xa55afff8 header
//         0xbf40c000-0xbf410000: EraseBlock 1
//             No idea what this is for. Different between MattMan's and Ryders box
//             It's called "EraseBlock 1" in the ROM Manager code and has a 0xa55afffc header
//
//     0xbf410000-0xbf420000: Upgrade storage area? (4kB; U0501+U0502)
//         0xbf410000-0xbf414000: Upgrade Sector
//             It's called "Upgrade Sector" in the ROM Manager code and has a 0xae1cffff header. This might be used to store upgrade data during a browser upgrade.
//             The ROM manager uses this logic to boot the browser OS, otherwise the upgrade OS is booted:
//                 ((*0xbf410002) != 0xae1c || (*0xbf410000 & 1) != 0 || (*0xbf410000 & 4) != 0 || (*0xbf410000 & 8) == 0) &&
//                 ((*0xbf410002) != 0xae1c || (*0xbf410000 & 1) != 0 || (*0xbf410000 & 4) != 0 || ((*0xbf410000 & 8) != 0 && (*0xbf410000 & 0x10) == 0))
//
//                 Also, when the box first boots you can press U from the console to star the boot menu. From there you can select the upgrade OS.
//
//     0xbf420000-0xbfa40000: Browser OS Section (6.125MB; U0501+U0502++U0503+U0504)
//         0xbf420000-0xbf9c49b8: nk.bin
//             ROMHdr is @ 0xbf9c388c/0x9f9c388c
//         0xbf9c49b8-0xbfa40000: Padding
//
//     0xbfa40000-0xbfbc0000: Upgrade OS Section (1.5MB; U0503+U0504)
//         0xbfa40000-0xbfb96c74: nk.bin
// `            ROMHdr is @ 0xbfb96884/0x9fb96884
//         0xbfb96c74-0xbfbc0000: Padding
//
//     0xbfbc0000-0xbfbe0000: Modem Firmware Section (128kB; U0503+U0504)
//         0xbfbc0000-0xbfbdf09e: Modem Firmware
//         0xbfbdf09e-0xbfbe0000: Padding
//
//     0xbfbe0000-0xbfc00000: Diagnostic? Section (128kB; U0503+U0504)
//         This is called during the MIPS reset vector stage. This section needs to have 0xb0bfb0bf @ 0xbfbe0000 before it's called.
//
//         0xbfbe0000-0xbfbe0400: Diagnostic? Code
//         0xbfbe0400-0xbfc00000: Padding
//
// This data appears to be mirrored:
//     0x9f400000-0x9fc00000 == 0xbf400000-0xbfc00000
//     0x9fc00000-0xa0000000 == 0xbf400000-0xbf800000
//     0xbfc00000-0xc0000000 == 0xbf400000-0xbf800000
//

void webtv2_state::webtv2_wld(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_RM5230_LE,
		CPU_167MHZ,
		MEM_16MB,
		MEM_4MB,
		0x03120000, // citation needed
		0x002ef200, // citation needed
		webtv2_state::AMD_4MB_FLASH | webtv2_state::BANK0_IS_DIAG | webtv2_state::HWMODEM | webtv2_state::PEKOE
	);
}

ROM_START(wtv2wld)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_newplus(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_RM5230_BE,
		CPU_150MHZ,
		MEM_16MB,
		MEM_2MB,
		0x03320000,
		0x002f8c24,
		webtv2_state::ROM | webtv2_state::FLASHDISK | webtv2_state::SWMODEM | webtv2_state::NTSCM_CBL_TUNER | webtv2_state::BT827_MEDIA_IN
	);
}

ROM_START(wtv2npl)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "New Plus BootROM (2.0.5, build 2524)")
	ROMX_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "dbugrom", "Debug New Plus BootROM (2.0.5, build 2524)")
	ROMX_LOAD("dbugrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_newclsc(machine_config& config)
{
	build_webtv_device(
		config,
		MIPS_RM5230_BE,
		CPU_150MHZ,
		MEM_8MB,
		MEM_2MB,
		0x05020000,
		0x002fd106,
		webtv2_state::ROM | webtv2_state::FLASHDISK | webtv2_state::SWMODEM
	);
}

ROM_START(wtv2ncl)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "New Classic BootROM (2.0.5, build 2524)")
	ROMX_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "dbugrom", "Debug New Classic BootROM (2.0.5, build 2524)")
	ROMX_LOAD("dbugrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_estar(machine_config& config)
{
	build_webtv_device(
		config,
		//MIPS_RM5230_BE,
		MIPS_R4640_BE, // Using R4640 to get around TLB issues. Seems to work the same anyway.
		CPU_167MHZ,
		MEM_16MB,
		MEM_2MB,
		0x03320000,
		0x002fc310,
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::HAN | webtv2_state::SWMODEM | webtv2_state::ESTAR_SAT_TUNER | webtv2_state::BT827_MEDIA_IN
	);
}

ROM_START(wtv2esr)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "Echostar BootROM (2.0.3, build 2324)")
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP)

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvehdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void webtv2_state::webtv2_utv(machine_config& config)
{
	// Configuring with HWMODEM just in case someone wants to run a modified UTV build that can interact with the hardware modem.
	// serial.dll may be used from the wtv2wld box but significant modifications are required so the correct kernel calls are made.

	// The UTV has a PCI and USB bus that can be implemented in fud_asic. From there you can implement a USB to ethernet driver.
	// You'll need to add an entry in the NVRAM to enable the USB ethernet driver (usually activated by connecting to the servie via the modem)

	// Or you can create your own method to get connected with a custom Windows CE driver.

	build_webtv_device(
		config,
		MIPS_RM5231_BE,
		CPU_250MHZ,
		MEM_32MB,
		MEM_2MB,
		0x04120000,
		0x034dea33,
		webtv2_state::ROM | webtv2_state::DISK | webtv2_state::SWMODEM | webtv2_state::HWMODEM | webtv2_state::DTV01_SAT_TUNER | webtv2_state::BT835_MEDIA_IN | webtv2_state::FUD
	);
}

ROM_START(wtv2utv)

	ROM_REGION(0x8, "serial_id", 0)
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "bootrom", "UltimateTV BootROM (2.0.6, build 2545)")
	ROMX_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(0))
	ROM_SYSTEM_BIOS(1, "dbugrom", "Debug UltimateTV BootROM (2.0.6, build 2545)")
	ROMX_LOAD("dbugrom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(1))
	ROM_SYSTEM_BIOS(2, "dgbprom", "Debug UltimateTV BootROM w/ HD bypass (2.0.6, build 2545)")
	ROMX_LOAD("dgbprom.o", 0x000000, 0x200000, NO_DUMP, ROM_BIOS(2))

	DISK_REGION("ata:0:hdd")
	DISK_IMAGE("wtvuhdd", 0x0000, NO_DUMP )

ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

//   YEAR  NAME      PARENT  COMPAT  MACHINE         INPUT CLASS         INIT       COMPANY    FULLNAME                                                                                 FLAGS
CONS(1997, wtv2lc2,       0,      0, webtv2_lc2,     0,    webtv2_state, base_init, "WebTV",   "WebTV 2: Plus | Sony INT-W200 / Philips MAT972 / Mitsubishi WB-2000 / Samsung SIS-100", MACHINE_UNOFFICIAL)
// Can add child machines here if you want to split out wtv2lc2
CONS(1997, wtv2jpp,       0,      0, webtv2_jpp,     0,    webtv2_state, base_init, "WebTV",   "WebTV 2: Japan Plus | Sony INT-WJ300 / Panasonic TU-WE100",                             MACHINE_UNOFFICIAL)
// Can add child machines here if you want to split out wtv2jpp
CONS(1997, wtv2jpc,       0,      0, webtv2_jpc,     0,    webtv2_state, base_init, "WebTV",   "WebTV 2: Japan Classic | Sony INT-WJ200 / Fujitsu F993000",                             MACHINE_UNOFFICIAL)
// Can add child machines here if you want to split out wtv2jpc
CONS(1998, wtv2drb,       0,      0, webtv2_derby,   0,    webtv2_state, base_init, "WebTV",   "WebTV 2: Derby | Sony INT-W200 'CND'",                                                  MACHINE_UNOFFICIAL | MACHINE_NODEVICE_LAN)
CONS(1998, wtv2wld,       0,      0, webtv2_wld,     0,    webtv2_state, base_init, "Philips", "WebTV 2: Philips WLD100 Italian Prototype",                                             MACHINE_UNOFFICIAL)
CONS(1999, wtv2npl,       0,      0, webtv2_newplus, 0,    webtv2_state, base_init, "WebTV",   "WebTV 2: New Plus | Sony INT-W250 / Philips MAT976 / RCA RW2110",                       MACHINE_UNOFFICIAL | MACHINE_NODEVICE_LAN)
// Can add child machines here if you want to split out wtv2npl
CONS(1999, wtv2ncl,       0,      0, webtv2_newclsc, 0,    webtv2_state, base_init, "WebTV",   "WebTV 2: BPS | Sony INT-W150 / Philips MAT965 / RCA RW2100",                            MACHINE_UNOFFICIAL | MACHINE_NODEVICE_LAN)
// Can add child machines here if you want to split out wtv2ncl
CONS(1999, wtv2esr,       0,      0, webtv2_estar,   0,    webtv2_state, base_init, "WebTV",   "WebTV 2: Echostar | DishPlayer 7100 / DishPlayer 7200",                                 MACHINE_UNOFFICIAL | MACHINE_NODEVICE_LAN)
// Can add child machines here if you want to split out wtv2esr
CONS(2000, wtv2utv,       0,      0, webtv2_utv,     0,    webtv2_state, base_init, "WebTV",   "WebTV 2: UltimateTV | Sony SAT-W60 / RCA DWD490RE / RCA DWD495RG",                      MACHINE_UNOFFICIAL | MACHINE_NODEVICE_LAN)
// Can add child machines here if you want to split out wtv2utv
