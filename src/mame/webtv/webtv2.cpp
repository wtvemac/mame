/*************************************************************************************
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
 *************************************************************************************/

#include "emu.h"

#include "cpu/mips/mips3.h"
#include "machine/solo1_asic.h"
#include "machine/solo1_asic_vid.h"
#include "bus/ata/ataintf.h"
#include "bus/ata/hdd.h"

#include "main.h"
#include "screen.h"

#define CPUCLOCK 167000000
#define SYSCLOCK 83300000

#define SOLO1_NTSC_WIDTH 640
#define SOLO1_NTSC_HEIGHT 480
#define SOLO1_NTSC_CLOCK 3.579575_MHz_XTAL

namespace {

class webtv2_state : public driver_device
{
public:
	// constructor
	webtv2_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_soloasic(*this, "solo"),
    	m_solovid(*this, "solo_vid"),
//    	m_soloaud(*this, "solo_aud"),
//		m_ata0(*this, "ata"),
		m_screen(*this, "screen")
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
	required_device<solo1_asic_vid_device> m_solovid;
//	required_device<solo1_asic_aud_device> m_soloaud;
	required_device<screen_device> m_screen;
//	required_device<ata_interface_device> m_ata0;

	void webtv2_map(address_map& map);
};

void webtv2_state::webtv2_map(address_map& map)
{
	map.global_mask(0x1fffffff);

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
	
	// 0x1d000000 - 0x1d3fffff: secondary device 4 (unassigned)
	// 0x1d400000 - 0x1d7fffff: secondary device 5 (IDE CD-ROM)
	// 0x1d800000 - 0x1dbfffff: secondary device 6 (IDE CD-ROM)
	// 0x1dc00000 - 0x1dffffff: secondary device 7 (unassigned)

	// 0x1e000000 - 0x1e3fffff: primary device 0 (modem/ethernet)
	// 0x1e400000 - 0x1e7fffff: primary device 1 (IDE hard disk)
	// 0x1e800000 - 0x1ebfffff: primary device 2 (IDE hard disk)
	// 0x1ec00000 - 0x1effffff: primary device 3 (unassigned)

	map(0x1f000000, 0x1f7fffff).rom().region("bank0", 0); // Flash ROM
	map(0x1f800000, 0x1fffffff).rom().region("bank1", 0); // Mask ROM
}

void webtv2_state::webtv2_base(machine_config& config)
{
	R4640BE(config, m_maincpu, CPUCLOCK);
	m_maincpu->set_icache_size(8192);
	m_maincpu->set_dcache_size(8192);
	m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::webtv2_map);
	
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_raw(SOLO1_NTSC_CLOCK, SOLO1_NTSC_WIDTH, 0, SOLO1_NTSC_WIDTH, SOLO1_NTSC_HEIGHT, 0, SOLO1_NTSC_HEIGHT);
	
    SOLO1_ASIC_VID(config, m_solovid, SOLO1_NTSC_CLOCK*2); // NTSC is assumed
	m_solovid->set_hostcpu(m_maincpu);

	SOLO1_ASIC(config, m_soloasic, SYSCLOCK);
	m_soloasic->set_hostcpu(m_maincpu);

	m_soloasic->set_solovid(m_solovid);

	m_screen->set_screen_update("solo_vid", FUNC(solo1_asic_vid_device::screen_update));
}

void webtv2_state::webtv2_sony(machine_config& config)
{
	webtv2_base(config);
	// TODO: differentiate manufacturers via emulated serial id
}

void webtv2_state::webtv2_philips(machine_config& config)
{
	webtv2_base(config);
	// TODO: differentiate manufacturers via emulated serial id
}

void webtv2_state::machine_start()
{

}

void webtv2_state::machine_reset()
{

}

ROM_START( wtv2sony )
	ROM_REGION32_BE(0x800000, "bank0", ROMREGION_ERASEFF)
	// this area is reserved for flash ROM

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
ROM_END

ROM_START( wtv2phil )
	ROM_REGION32_BE(0x800000, "bank0", ROMREGION_ERASEFF)
	// this area is reserved for flash ROM

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
