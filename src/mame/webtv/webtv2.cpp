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
 * board revisions switched to a MIPS RM5230.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.

 * The technical specifications that this implementation is based on can be found here:
 * http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf
 * 
 ***************************************************************************************/

#include "emu.h"

#include "cpu/mips/mips3.h"
#include "solo1_asic.h"
#include "solo1_asic_vid.h"
#include "bus/ata/ataintf.h"
#include "bus/ata/hdd.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

#define CPUCLOCK 167000000
#define SYSCLOCK 83300000

#define POT_INT_SHIFT     1 << 2 // SOLO1 never sets this bit
#define POT_INT_VIDHSYNC  1 << 3 // Interrupt fires on hsync
#define POT_INT_VIDVSYNCO 1 << 4 // Interrupt fires on odd field vsync
#define POT_INT_VIDVSYNCE 1 << 5 // Interrupt fires on even field vsync

#define BUS_VID_INTSTAT_POT 1 << 3 // Interrupt trigger in potUnit
#define BUS_VID_INTSTAT_VID 1 << 2 // Interrupt trigger in vidUnit

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
		m_power_led(*this, "power_led"),
		m_connect_led(*this, "connect_led"),
		m_message_led(*this, "message_led")
	{ }

	void webtv2_base(machine_config& config);
	void webtv2_sony(machine_config& config);
	void webtv2_philips(machine_config& config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

	void solo_hsync_callback(uint16_t data);
	void solo_vsync_callback(uint16_t data);

private:
	required_device<mips3_device> m_maincpu;
	required_device<solo1_asic_device> m_soloasic;
	required_device<solo1_asic_vid_device> m_solovid;
//	required_device<solo1_asic_aud_device> m_soloaud;
//	required_device<ata_interface_device> m_ata0;

	output_finder<1> m_power_led;
	output_finder<1> m_connect_led;
	output_finder<1> m_message_led;

	enum
	{
		LED_POWER = 0x4,
		LED_CONNECTED = 0x2,
		LED_MESSAGE = 0x1
	};

	void led_w(uint32_t data);
	uint32_t led_r();

	void webtv2_map(address_map& map);
};

void webtv2_state::led_w(uint32_t data)
{
}

uint32_t webtv2_state::led_r()
{
    return 0;
}

void webtv2_state::webtv2_map(address_map &map)
{
	map.global_mask(0x1fffffff);

	// RAM
	map(0x00000000, 0x03ffffff).ram().share("mainram"); // TODO: allocating all 64MB is inaccurate to retail hardware!

	// SOLO
	// busUnit: 0x04000000 - 0x04000fff
	map(0x04000000, 0x04000003).rw(m_soloasic, FUNC(solo1_asic_device::reg_bus_r), FUNC(solo1_asic_device::reg_bus_w)); // busUnit
	map(0x04000004, 0x04000007).rw(FUNC(led_r), FUNC(led_w));
	map(0x04000008, 0x04000fff).rw(m_soloasic, FUNC(solo1_asic_device::reg_bus_r), FUNC(solo1_asic_device::reg_bus_w)); // busUnit

	// rioUnit: 0x04001000 - 0x04001fff
    //map(0x04001000, 0x04001fff).rw(m_soloasic, FUNC(solo1_asic_device::reg_rio_r), FUNC(solo1_asic_device::reg_rio_w)); // rioUnit

	// audUnit: 0x04002000 - 0x04002fff
    //map(0x04002000, 0x04002fff).rw(m_soloaud, FUNC(solo1_asic_aud_device::reg_aud_r), FUNC(solo1_asic_aud_device::reg_aud_w)); // audUnit

	// vidUnit: 0x04003000 - 0x04003fff
    map(0x04003000, 0x04003fff).rw(m_solovid, FUNC(solo1_asic_vid_device::reg_vid_r), FUNC(solo1_asic_vid_device::reg_vid_w)); // vidUnit

	// devUnit: 0x04004000 - 0x04004fff
    map(0x04004000, 0x04004fff).rw(m_soloasic, FUNC(solo1_asic_device::reg_dev_r), FUNC(solo1_asic_device::reg_dev_w)); // devUnit

	// memUnit: 0x04005000 - 0x04005fff
    map(0x04005000, 0x04005fff).rw(m_soloasic, FUNC(solo1_asic_device::reg_mem_r), FUNC(solo1_asic_device::reg_mem_w)); // memUnit

	// gfxUnit: 0x04006000 - 0x04006fff
    //map(0x04006000, 0x04006fff).rw(m_solovid, FUNC(solo1_asic_vid_device::reg_gfx_r), FUNC(solo1_asic_vid_device::reg_gfx_w)); // gfxUnit

	// dveUnit: 0x04007000 - 0x04007fff
    map(0x04007000, 0x04007fff).rw(m_solovid, FUNC(solo1_asic_vid_device::reg_dve_r), FUNC(solo1_asic_vid_device::reg_dve_w)); // dveUnit

	// divUnit: 0x04008000 - 0x04008fff
    //map(0x04008000, 0x04008fff).rw(m_soloasic, FUNC(solo1_asic_device::reg_div_r), FUNC(solo1_asic_device::reg_div_w)); // divUnit

	// potUnit: 0x04009000 - 0x04009fff
    map(0x04009000, 0x04009fff).rw(m_solovid, FUNC(solo1_asic_vid_device::reg_pot_r), FUNC(solo1_asic_vid_device::reg_pot_w)); // potUnit

	// sucUnit: 0x0400a000 - 0x0400afff
    //map(0x0400a000, 0x0400afff).rw(m_soloasic, FUNC(solo1_asic_device::reg_suc_r), FUNC(solo1_asic_device::reg_suc_w)); // sucUnit

	// modUnit: 0x0400b000 - 0x0400bfff
    //map(0x0400b000, 0x0400bfff).rw(m_soloaud, FUNC(solo1_asic_aud_device::reg_mod_r), FUNC(solo1_asic_aud_device::reg_mod_w)); // modUnit

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
	config.set_default_layout(layout_webtv);

	R4640BE(config, m_maincpu, CPUCLOCK);
	m_maincpu->set_icache_size(8192);
	m_maincpu->set_dcache_size(8192);
	m_maincpu->set_addrmap(AS_PROGRAM, &webtv2_state::webtv2_map);

	SOLO1_ASIC(config, m_soloasic, SYSCLOCK);
	m_soloasic->set_hostcpu(m_maincpu);
	
    SOLO1_ASIC_VID(config, m_solovid, 0); // clock freq will be set internally during screen initialization
	m_solovid->set_hostcpu(m_maincpu);
	m_solovid->hsync_callback().set(FUNC(webtv2_state::solo_hsync_callback));
	m_solovid->vsync_callback().set(FUNC(webtv2_state::solo_vsync_callback));
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


void webtv2_state::solo_hsync_callback(uint16_t data)
{
	if(data==0) return;
    if(m_solovid->m_pot_int_enable&POT_INT_VIDHSYNC)
    {
        m_solovid->m_pot_int_status |= POT_INT_VIDHSYNC;
        m_soloasic->set_vid_int_flag(BUS_VID_INTSTAT_POT);
        m_maincpu->set_input_line(MIPS3_IRQ0, ASSERT_LINE);
    }
}

void webtv2_state::solo_vsync_callback(uint16_t data)
{
	if(data==0) return;
    if(m_solovid->is_even_field())
    {
        // even
        if(m_solovid->m_pot_int_enable&POT_INT_VIDVSYNCE)
        {
            m_solovid->m_pot_int_status |= POT_INT_VIDVSYNCE;
            m_soloasic->set_vid_int_flag(BUS_VID_INTSTAT_POT);
            m_maincpu->set_input_line(MIPS3_IRQ0, ASSERT_LINE);
        }
    }
    else
    {
        // odd
        if(m_solovid->m_pot_int_enable&POT_INT_VIDVSYNCO)
        {
            m_solovid->m_pot_int_status |= POT_INT_VIDVSYNCO;
            m_soloasic->set_vid_int_flag(BUS_VID_INTSTAT_POT);
            m_maincpu->set_input_line(MIPS3_IRQ0, ASSERT_LINE);
        }
    }
}

ROM_START( wtv2sony )
	ROM_REGION32_BE(0x800000, "bank0", ROMREGION_ERASEFF)
	// this area is reserved for flash ROM

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_SYSTEM_BIOS(0, "2046ndbg", "Standard Boot ROM (2.0, build 2046)")
	ROMX_LOAD("2046ndbg.o", 0x000000, 0x200000, CRC(89938464) SHA1(3C614AA2E1457A9D30C7696ADEBB7260D07963E5), ROM_BIOS(0))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
	ROM_SYSTEM_BIOS(1, "joedebug", "Joe Britt's Debug Boot ROM (build 32767)")
	ROMX_LOAD("joedebug.o", 0x000000, 0x200000, CRC(FF8A6E04) SHA1(4ECA661E94785676795A19899B22724F660AA505), ROM_BIOS(1))
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
CONS( 1997, wtv2sony,      0,      0, webtv2_sony,       0, webtv2_state, empty_init, "Sony",               "INT-W200 WebTV Plus Receiver", MACHINE_NOT_WORKING | MACHINE_NO_SOUND | MACHINE_IMPERFECT_GRAPHICS | MACHINE_NODEVICE_MICROPHONE | MACHINE_NODEVICE_PRINTER )
CONS( 1997, wtv2phil,      0,      0, webtv2_philips,    0, webtv2_state, empty_init, "Philips-Magnavox",   "MAT972 WebTV Plus Receiver",   MACHINE_NOT_WORKING | MACHINE_NO_SOUND | MACHINE_IMPERFECT_GRAPHICS | MACHINE_NODEVICE_MICROPHONE | MACHINE_NODEVICE_PRINTER )
