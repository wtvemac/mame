
#ifndef MAME_MACHINE_WTVSTREAM_H
#define MAME_MACHINE_WTVSTREAM_H

#pragma once

#include "machine/i2chle.h"

class wtvvidstream_device_base : public device_t, public i2c_hle_interface
{

public:

	wtvvidstream_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, u16 iic_address);

	int sda_read();

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	int m_iic_sda;

	void iic_sda_w(int state);

};

class bt835_decoder_device : public wtvvidstream_device_base
{

public:

	bt835_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 read_data(u16 offset) override;
	virtual void write_data(u16 offset, u8 data) override;

};

DECLARE_DEVICE_TYPE(BT835, bt835_decoder_device)

class bt827_decoder_device : public wtvvidstream_device_base
{

public:

	bt827_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 read_data(u16 offset) override;
	virtual void write_data(u16 offset, u8 data) override;

};

DECLARE_DEVICE_TYPE(BT827, bt827_decoder_device)


class saa71786_encoder_device : public wtvvidstream_device_base
{

public:

	saa71786_encoder_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 read_data(u16 offset) override;
	virtual void write_data(u16 offset, u8 data) override;

};

DECLARE_DEVICE_TYPE(SAA71786, saa71786_encoder_device)

#endif // MAME_MACHINE_WTVSTREAM_H