/*************************************************************************************
 *
 * WebTV LC2 (1997)
 * 
 * Shorthand for "Low Cost v2", this is the second generation of the WebTV hardware.
 * It added graphics acceleration, an on-board printer port, and the ability to use a
 * hard drive, a TV tuner, and satellite receiver circuitry. It uses a custom ASIC
 * designed by WebTV Networks Inc. known as the SOLO chip.
 *
 * The original LC2 boards used a MIPS IDT R4640 clocked at 167MHz, although later
 * board revisions switched to a MIPS RM5230.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.
 *
 * TODO:
 * - Map everything to SOLO1
 * - Much, much more
 *
 * New:
 * 2023/7/7 - Preliminary driver
 * 
 *************************************************************************************/

#include "emu.h"

#include "cpu/mips/mips3.h"
#include "machine/solo1_asic.h"

#include "main.h"
#include "screen.h"

namespace {

class webtv2_state : public driver_device
{
public:
	// constructor
	webtv2_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_soloasic(*this, "soloasic")
	{ }

	void webtv2_base(machine_config& config);
	void webtv2_sony(machine_config& config);
	void webtv2_philips(machine_config& config);


protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	required_device<mips3_device> m_maincpu;
	required_device<solo1_asic_device> m_soloasic;

	void webtv2_map(address_map& map);
};

void webtv2_state::webtv2_map(address_map& map)
{
	map.global_mask(0x1fffffff);

	// TODO: bank0, diag, and bank1 should be mapped to SOLO

	// RAM
	map(0x00000000, 0x03ffffff).ram().share("mainram");

	// SOLO registers
	map(0x04000000, 0x047fffff).m(m_soloasic, FUNC(solo1_asic_device::regs_map)).share("solo_regs");

	// expansion device areas
	//map(0x04800000, 0x04ffffff).ram().share("exp1");
	//map(0x05000000, 0x057fffff).ram().share("exp2");
	//map(0x05800000, 0x05ffffff).ram().share("exp3");
	//map(0x06000000, 0x067fffff).ram().share("exp4");
	//map(0x06800000, 0x06ffffff).ram().share("exp5");
	//map(0x07000000, 0x077fffff).ram().share("exp6");
	//map(0x07800000, 0x07ffffff).ram().share("exp7");

	map(0x1f000000, 0x1f3fffff).rom().region("bank0", 0);
	map(0x1f400000, 0x1f7fffff).ram().share("diag");
	map(0x1f800000, 0x1fffffff).rom().region("bank1", 0);
}

void webtv2_state::webtv2_base(machine_config& config)
{
	R4640BE(config, m_maincpu, 167000000);
	m_maincpu->set_icache_size(8192);
	m_maincpu->set_dcache_size(8192);
	m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::webtv2_map);

	SOLO1_ASIC(config, m_soloasic, 0);
	m_soloasic->set_hostcpu(m_maincpu);
}

void webtv2_state::webtv2_sony(machine_config& config)
{
	webtv2_base(config);
	// TODO: differentiate manufacturers
}

void webtv2_state::webtv2_philips(machine_config& config)
{
	webtv2_base(config);
	// TODO: differentiate manufacturers
}

void webtv2_state::machine_start()
{

}

void webtv2_state::machine_reset()
{

}

ROM_START( wtv2sony )
	ROM_REGION32_BE(0x400000, "bank0", 0)
	ROM_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5))
	ROM_RELOAD(0x200000, 0x200000)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
ROM_END

ROM_START( wtv2phil )
	ROM_REGION32_BE(0x400000, "bank0", 0)
	ROM_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5))
	ROM_RELOAD(0x200000, 0x200000)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
ROM_END

}

//    YEAR  NAME      PARENT  COMPAT  MACHINE        INPUT  CLASS         INIT        COMPANY               FULLNAME                        FLAGS
CONS( 1997, wtv2sony,      0,      0, webtv2_sony,       0, webtv2_state, empty_init, "Sony",               "INT-W200 WebTV Plus Receiver", MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
CONS( 1997, wtv2phil,      0,      0, webtv2_philips,    0, webtv2_state, empty_init, "Philips-Magnavox",   "MAT972 WebTV Plus Receiver",   MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
