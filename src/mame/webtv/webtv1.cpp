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

	void webtv2_map(address_map& map);
}