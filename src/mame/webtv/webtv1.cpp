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
 * A typical retail configuration uses an R4640 clocked at 112MHz, with 2MB of on-board
 * RAM, 2MB of flash memory for the software, and 2MB of ROM.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.
 * 
 ***************************************************************************************/

#include "emu.h"

#include "cpu/mips/mips3.h"
#include "spot_asic.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

#define CPUCLOCK 112000000

class webtv1_state : public driver_device
{
public:
	// constructor
	webtv1_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		//m_spotasic(*this, "spot"),
		m_power_led(*this, "power_led"),
		m_connect_led(*this, "connect_led"),
		m_message_led(*this, "message_led")
	{ }

	void webtv1_base(machine_config& config);
	void webtv1_sony(machine_config& config);
	void webtv1_philips(machine_config& config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;
    
private:
	required_device<mips3_device> m_maincpu;
	//required_device<spot_asic_device> m_spotasic;

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

	void webtv1_map(address_map& map);
};

void webtv1_state::webtv1_map(address_map &map)
{
	map.global_mask(0x1fffffff);

	// RAM
	map(0x00000000, 0x007fffff).ram().share("mainram"); // TODO: allocating all 8MB is inaccurate to retail hardware!
	
	map(0x1f800000, 0x1fffffff).rom().region("bank1", 0); // Mask ROM
}

void webtv1_state::webtv1_base(machine_config &config)
{
	config.set_default_layout(layout_webtv);

	R4640BE(config, m_maincpu, CPUCLOCK);
	m_maincpu->set_icache_size(8192);
	m_maincpu->set_dcache_size(8192);
	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_map);

	//SPOT_ASIC(config, m_spotasic, SYSCLOCK);
	//m_spotasic->set_hostcpu(m_maincpu);
}

void webtv1_state::webtv1_sony(machine_config& config)
{
	webtv1_base(config);
	// TODO: differentiate manufacturers via emulated serial id
}

void webtv1_state::webtv1_philips(machine_config& config)
{
	webtv1_base(config);
	// TODO: differentiate manufacturers via emulated serial id
}

void webtv1_state::machine_start()
{

}

void webtv1_state::machine_reset()
{

}

ROM_START( wtv1sony )
	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, CRC(71C321DB) SHA1(6A39E064FB2312D70728B8105DE990762226BD07))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
ROM_END

ROM_START( wtv1phil )
	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, CRC(71C321DB) SHA1(6A39E064FB2312D70728B8105DE990762226BD07))
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
	ROM_RELOAD(0x600000, 0x200000)
ROM_END

//    YEAR  NAME      PARENT  COMPAT  MACHINE        INPUT  CLASS         INIT        COMPANY               FULLNAME                            FLAGS
CONS( 1996, wtv1sony,      0,      0, webtv1_sony,       0, webtv1_state, empty_init, "Sony",               "INT-W100 WebTV Internet Terminal", MACHINE_IS_SKELETON )
CONS( 1996, wtv1phil,      0,      0, webtv1_philips,    0, webtv1_state, empty_init, "Philips-Magnavox",   "MAT960 WebTV Internet Terminal",   MACHINE_IS_SKELETON )