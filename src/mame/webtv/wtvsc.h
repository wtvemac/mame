// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSC_H
#define MAME_WEBTV_WTVSC_H

#pragma once

#include "utv_ca_card.h"

#include "imagedev/bitbngr.h"

class wtvsc_device : public device_t
{
public:

	wtvsc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// Use the internal UTV CA card while nothing is mounted
	void set_internal_card(bool internal_card) { m_internal_card = internal_card; }

	// Called with 1 when something is mounted on the bitbanger and 0 when it's unmounted
	auto mounted_callback() { return m_mounted_cb.bind(); }

	bool mounted() const { return m_mounted; }

	// Polls the card detect line
	bool present();

	// The card was deactivated (no power or held in reset)
	void clear();

	// The card was powered and reset released, the ATR follows
	void start();

	// data is sucUnit's FIFO value.
	// inverse_convention enables ISO 7816-3 inverse convention
	void write(uint8_t data, bool inverse_convention);
	bool peek(uint8_t &data, bool inverse_convention);
	void pop();

	// Converts between direct and inverse convention (bit order reversed, levels inverted)
	static constexpr uint8_t inverse(uint8_t value)
	{
		value = ((value & 0x55) << 1) | ((value >> 1) & 0x55);
		value = ((value & 0x33) << 2) | ((value >> 2) & 0x33);

		return ~((value << 4) | (value >> 4));
	}

protected:

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	enum : uint32_t
	{
		CONFIG_RESET_MASK       = 0x03,
		CONFIG_RESET_NONE       = 0x00,
		CONFIG_RESET_RTS        = 0x01,
		CONFIG_RESET_DTR        = 0x02,
		CONFIG_RESET_INVERT     = 0x04, // line deasserted holds the card in reset
		CONFIG_DETECT_MASK      = 0x18,
		CONFIG_DETECT_NONE      = 0x00,
		CONFIG_DETECT_CTS       = 0x08,
		CONFIG_DETECT_DCD       = 0x10,
		CONFIG_DETECT_DSR       = 0x18,
		CONFIG_DETECT_INVERT    = 0x20, // line deasserted when a card is inserted
		CONFIG_ECHO             = 0x40  // the reader echoes what's sent (single I/O line)
	};

	required_device<bitbanger_device> m_bitb;
	required_ioport m_config;
	devcb_write_line m_mounted_cb;

	utv_ca_card m_ca_card;
	bool m_internal_card;
	bool m_active;
	bool m_mounted;
	bool m_have_rx;
	uint8_t m_rx;
	uint32_t m_echo;

	void bitb_mounted(int state);
	void set_reset(bool reset);
	void drop_input();
};

DECLARE_DEVICE_TYPE(WTVSC, wtvsc_device)

#endif // MAME_WEBTV_WTVSC_H
