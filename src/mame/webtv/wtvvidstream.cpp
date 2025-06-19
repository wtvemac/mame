#include "emu.h"
#include "wtvvidstream.h"

wtvvidstream_device_base::wtvvidstream_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, u16 iic_address) :
	device_t(mconfig, type, tag, owner, clock),
	i2c_hle_interface(mconfig, *this, iic_address >> 1)
{
	write_sda.bind().set(tag, FUNC(wtvvidstream_device_base::iic_sda_w));
}

void wtvvidstream_device_base::device_start()
{
	//
}

void wtvvidstream_device_base::device_reset()
{
	m_iic_sda = 0x1;
}

void wtvvidstream_device_base::iic_sda_w(int state)
{
	m_iic_sda = state & 0x1;
}

int wtvvidstream_device_base::sda_read()
{
	return m_iic_sda & 0x1;
}

DEFINE_DEVICE_TYPE(BT835, bt835_decoder_device, "bt835_decoder_device", "BT835 Decoder")

bt835_decoder_device::bt835_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock) :
	wtvvidstream_device_base(mconfig, BT835, tag, owner, clock, iic_address)
{
}

void bt835_decoder_device::device_start()
{
	//
}

void bt835_decoder_device::device_reset()
{
	//
}

u8 bt835_decoder_device::read_data(u16 offset)
{
	switch (offset)
	{
		default:
			return 0xff;
	}
}

void bt835_decoder_device::write_data(u16 offset, u8 data)
{
	//
}

DEFINE_DEVICE_TYPE(BT827, bt827_decoder_device, "bt827_decoder_device", "BT827 Decoder")

bt827_decoder_device::bt827_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock) :
	wtvvidstream_device_base(mconfig, BT827, tag, owner, clock, iic_address)
{
}

void bt827_decoder_device::device_start()
{
	//
}

void bt827_decoder_device::device_reset()
{
	//
}

u8 bt827_decoder_device::read_data(u16 offset)
{
	switch (offset)
	{
		default:
			return 0xff;
	}
}

void bt827_decoder_device::write_data(u16 offset, u8 data)
{
	//
}

DEFINE_DEVICE_TYPE(SAA71786, saa71786_encoder_device, "saa71786_encoder_device", "SAA71786 Encoder")

saa71786_encoder_device::saa71786_encoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock) :
	wtvvidstream_device_base(mconfig, SAA71786, tag, owner, clock, iic_address)
{
}

void saa71786_encoder_device::device_start()
{
	//
}

void saa71786_encoder_device::device_reset()
{
	//
}

u8 saa71786_encoder_device::read_data(u16 offset)
{
	switch (offset)
	{
		default:
			return 0xff;
	}
}

void saa71786_encoder_device::write_data(u16 offset, u8 data)
{
	//
}
