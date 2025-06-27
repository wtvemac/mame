// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_WTVIR_H
#define MAME_MACHINE_WTVIR_H

#pragma once

#include "machine/pckeybrd.h"

class wtvir_device_base : public device_t
{
public:

	static constexpr uint32_t MAX_QUEUED_BUTTONS        =  0x08;
	static constexpr uint32_t MAX_SAMPLE_FIFO_ENTRIES   =  0x10;
	static constexpr uint32_t DEFAULT_BIT_SAMPLE_CLOCKS =  0x30;
	static constexpr uint32_t IR_MICROCODE_VERSION      =  0x69;

	enum wtvir_register_t
	{
		DEV_IROLD                = 0x00,
		DEV_IRDATA               = 0x00,
		DEV_IRIN_SAMPLE          = 0x08,
		DEV_IRIN_REJECT_INT      = 0x09,
		DEV_IRIN_TRANS_DATA      = 0x0a,
		DEV_IRIN_STATCNTL        = 0x0b,
		DEV_IROUT_FIFO           = 0x10,
		DEV_IROUT_STATUS         = 0x11,
		DEV_IROUT_PERIOD         = 0x12,
		DEV_IROUT_ON             = 0x13,
		DEV_IROUT_CURRENT_PERIOD = 0x14,
		DEV_IROUT_CURRENT_ON     = 0x15,
		DEV_IROUT_CURRENT_COUNT  = 0x16
	};

	typedef struct
	{
		uint8_t scancode;
		bool is_make;
		uint8_t trans_bit_index;
		uint32_t ir_data;
	} ir_button_state_t;

	wtvir_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual void enable(int state);
	uint32_t data_r(offs_t offset);
	void data_w(offs_t offset, uint32_t data);

	virtual bool enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data);
	virtual uint8_t queued_button_count();
	virtual ir_button_state_t* current_button();
	virtual ir_button_state_t* dequeue_button();

	auto sample_fifo_trigger_callback() { return m_sample_fifo_trigger_cb.bind(); }

	uint8_t m_fifo_data_bit_count;
	bool m_waiting_for_fifo_read;

	devcb_write_line m_sample_fifo_trigger_cb;

	emu_timer *m_input_timer;

	uint8_t m_queued_button_head;
	uint8_t m_queued_button_tail;
	ir_button_state_t m_queued_buttons[MAX_QUEUED_BUTTONS];

	uint16_t m_irin_sample_interval;
	uint8_t m_irin_reject_interval;
	uint8_t m_irin_statcntl;
	uint8_t m_irin_bit_sample_clock_cnt;

private:

	virtual void polling();

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	TIMER_CALLBACK_MEMBER(poll_buttons);
};

class wtvir_sejin_device : public wtvir_device_base
{
public:

	static constexpr uint32_t SEJIN_SCANCODE_COUNT    =  128;
	static constexpr uint32_t SEJIN_DEFAULT_IR_DATA   =  0x200508;
	static constexpr uint32_t SEJIN_DATA_BIT_COUNT    =  22;

	wtvir_sejin_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	bool enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data) override;

private:

	uint8_t calculate_odd_parity(uint32_t data, uint8_t bit_start, uint8_t bit_end);

	void polling() override;
	uint32_t readport(int port);

	uint8_t m_device_id;

	optional_ioport_array<8> m_ioport;
	uint32_t m_port_state[SEJIN_SCANCODE_COUNT >> 4];

protected:

	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

};

DECLARE_DEVICE_TYPE(SEJIN_KBD, wtvir_sejin_device)

#endif // MAME_MACHINE_WTVIR_H