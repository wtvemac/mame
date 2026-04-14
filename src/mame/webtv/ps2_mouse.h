// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_PS2_MOUSE_H
#define MAME_WEBTV_PS2_MOUSE_H

#pragma once

class ps2_mouse_device : public device_t
{

public:

	static constexpr uint8_t DEFAULT_POLL_RATE_HZ = 100;

	enum in_mode_t : uint8_t
	{
		COMMAND    = 0x01,
		POLL_RATE  = 0x02,
		RESOLUTION = 0x03
	};

	ps2_mouse_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto out_w_callback() { return m_out_w_cb.bind(); }
	void in_w(uint8_t data);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

private:

	optional_ioport m_mousex_port;
	optional_ioport m_mousey_port;
	optional_ioport m_mousebtn_port;

	devcb_write8 m_out_w_cb;

	in_mode_t m_in_mode;

	uint8_t m_poll_rate;
	uint8_t m_resolution;

	uint16_t m_mouse_x;
	uint16_t m_mouse_y;
	uint8_t m_mouse_btn;

	void reset();
	void set_defaults();
	void enable_scanning(bool enable);
	void in_command_w(uint8_t data);
	void in_poll_rate_w(uint8_t data);
	void in_resolution_w(uint8_t data);

	emu_timer *m_input_timer;
	TIMER_CALLBACK_MEMBER(poll_mouse);

};

DECLARE_DEVICE_TYPE(PS2_MOUSE, ps2_mouse_device)

#endif // MAME_WEBTV_PS2_MOUSE_H
