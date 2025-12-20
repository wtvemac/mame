// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVTUNER_H
#define MAME_WEBTV_WTVTUNER_H

#pragma once

#include "machine/i2chle.h"

class wtvtuner_device_base : public device_t, public i2c_hle_interface
{

public:

	wtvtuner_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, u16 iic_address);

	int sda_read();

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	int m_iic_sda;

	void iic_sda_w(int state);

};

class generic_tuner_device : public wtvtuner_device_base
{

public:

	generic_tuner_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 read_data(u16 offset) override;
	virtual void write_data(u16 offset, u8 data) override;

private:

};

DECLARE_DEVICE_TYPE(TUNER, generic_tuner_device)

class l64734_tuner_device : public wtvtuner_device_base
{

public:

	l64734_tuner_device(const machine_config &mconfig, const char *tag, device_t *owner, u16 iic_address, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual u8 read_data(u16 offset) override;
	virtual void write_data(u16 offset, u8 data) override;

private:

	uint8_t m_chip_id;

};

DECLARE_DEVICE_TYPE(L64734, l64734_tuner_device)

#endif // MAME_WEBTV_WTVTUNER_H