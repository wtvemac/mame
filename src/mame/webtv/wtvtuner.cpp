#include "emu.h"
#include "wtvtuner.h"

wtvtuner_device_base::wtvtuner_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, u16 iic_address) :
	device_t(mconfig, type, tag, owner, clock),
	i2c_hle_interface(mconfig, *this, iic_address >> 1)
{
	write_sda.bind().set(tag, FUNC(wtvtuner_device_base::sdar_w));
}

void wtvtuner_device_base::device_start()
{
	//
}

void wtvtuner_device_base::device_reset()
{
	m_sdar = 0x1;
}

void wtvtuner_device_base::sdar_w(int state)
{
	m_sdar = state;
}

int wtvtuner_device_base::sda_read()
{
	return m_sdar & 0x1;
}

DEFINE_DEVICE_TYPE(L64734, l64734_tuner_device, "l64734_tuner_device", "L64734 Satellite Tuner")

l64734_tuner_device::l64734_tuner_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock) :
	wtvtuner_device_base(mconfig, L64734, tag, owner, clock, iic_address)
{
}

void l64734_tuner_device::device_start()
{
	//
}

void l64734_tuner_device::device_reset()
{
	m_chip_id = 0x00;
}

u8 l64734_tuner_device::read_data(u16 offset)
{
	switch (offset)
	{
		case 0x0003: // Appears to be an ID
			return m_chip_id;

		case 0x0004: // Appears to be the gain register (how much signal we receieve)
			return 0x32; // reports 100%

		default:
			return 0xff;
	}
}

void l64734_tuner_device::write_data(u16 offset, u8 data)
{
	//
}
