// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVIR_H
#define MAME_WEBTV_WTVIR_H

#pragma once

#include "imagedev/lirc.h"
#include "machine/pckeybrd.h"

class wtvir_device_base : public device_t
{
public:

	static constexpr uint32_t POLL_RATE_HZ = 60;

	static constexpr uint32_t MAX_QUEUED_BUTTONS        =  0x08;
	static constexpr uint8_t MAX_SAMPLE_FIFO_ENTRIES    =  0x10;
	static constexpr uint32_t DEFAULT_BIT_SAMPLE_CLOCKS =  48;
	static constexpr uint32_t IR_MICROCODE_VERSION      =  0x69;
	static constexpr double DEFAULT_SAMPLE_CLOCK_US     =  789.0 / 51.3;
	static constexpr uint32_t LIRC_MAX_US               =  10000; // 10ms
	static constexpr uint8_t LIRC_SAMPLE_FIFO_ENTRIES   =  0x80;

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

	typedef struct ir_old_value
	{
		uint8_t data[4];

		uint32_t get_raw() const
		{
			return (
				  (static_cast<uint32_t>(data[0]) << 24)
				| (static_cast<uint32_t>(data[1]) << 16)
				| (static_cast<uint32_t>(data[2]) <<  8)
				| (static_cast<uint32_t>(data[3]) <<  0)
			);
		}

		bool get_is_sejin_keyboard() const
		{
			return ((data[0] != 0x00) && get_keyboard_id() == 0x10);
		}
		bool get_key_is_make() const
		{
			return ((data[0] & 0x40) == 0x00);
		}
		uint8_t get_keyboard_id() const
		{
			return (data[0] & 0x30);
		}
		uint8_t get_key_flags() const
		{
			return data[0];
		}
		void set_key_flags(uint8_t value)
		{
			data[0] = value;
		}

		uint8_t get_primary_data() const
		{
			return data[1];
		}
		void set_primary_data(uint8_t value)
		{
			data[1] = value;
		}

		uint8_t get_secondary_data() const
		{
			return data[2];
		}
		void set_secondary_data(uint8_t value)
		{
			data[2] = value;
		}

		uint8_t get_remote_button() const
		{
			return data[3];
		}
		void set_remote_button(uint8_t value)
		{
			data[3] = value;
		}

		void reset()
		{
			data[0] = 0x00;
			data[1] = 0x00;
			data[2] = 0x00;
			data[3] = 0x00;
		}
	} ir_old_value_t;

	wtvir_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0, bool lirc_in_enabled = true);

	virtual void enable(int state);
	bool lirc_in_enabled();
	uint32_t data_r(offs_t offset);
	void data_w(offs_t offset, uint32_t data);

	virtual bool enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data);
	virtual uint8_t queued_button_count();
	virtual ir_button_state_t* current_button();
	virtual ir_button_state_t* dequeue_button();

	virtual bool enqueue_lirc_in(uint32_t state);
	virtual uint8_t queued_lirc_in_count();
	virtual uint32_t current_lirc_in();
	virtual uint32_t dequeue_lirc_in();

	auto sample_fifo_trigger_callback() { return m_sample_fifo_trigger_cb.bind(); }

	uint8_t m_fifo_data_bit_count;
	bool m_waiting_for_fifo_read;

	devcb_write_line m_sample_fifo_trigger_cb;
	required_device<lirc_device> m_lirc;

	emu_timer *m_input_timer;

	uint8_t m_queued_button_head;
	uint8_t m_queued_button_tail;
	ir_button_state_t m_queued_buttons[MAX_QUEUED_BUTTONS];

	ir_old_value_t m_irin_old_value;
	uint16_t m_irin_sample_interval;
	uint8_t m_irin_reject_interval;
	uint8_t m_irin_statcntl;
	uint8_t m_irin_bit_sample_clock_cnt;

	uint8_t m_lirc_in_head;
	uint8_t m_lirc_in_tail;

private:

	bool m_lirc_in_enabled;

	uint32_t lirc_in_fifo[wtvir_device_base::LIRC_SAMPLE_FIFO_ENTRIES];

	virtual void poll_buttons();
	virtual void poll_lirc_in();
	virtual void poll();

	uint32_t get_ir_in_data();
	uint32_t build_solo_ir_in_data(uint8_t fifo_cnt, bool bit_val, uint16_t sample_clocks);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	TIMER_CALLBACK_MEMBER(poll_timer);
};

// The MSNTV2 and original classic use a PIC chip to decode IR input
class wtvir_decoder_device : public wtvir_device_base
{
public:

	// The amount of time (in microseconds) a pulse or space state needs to hold before we register a bit.
	// 789us is the exact specced time from Sejin
	// Adding an adjustment factor in case you want to account for some error
	static constexpr double WTVKB_TIME_PER_BIT = 1.00 * 789;

	// If you're looking at the Sejin doc you need to do an endian flip since bit 1 starts on the rightmost side here.

	// Sejin transmits 22 bits per key packet
	static constexpr uint8_t WTVKB_MIN_BITCNT = 1;
	static constexpr uint8_t WTVKB_MAX_BITCNT = 22;
	static constexpr uint8_t WTVKB_BIT_SHIFT  = (WTVKB_MAX_BITCNT - 1);
	static constexpr uint32_t WTVKB_BIT_MASK  = (1 << WTVKB_MAX_BITCNT) - 1;

	// If we find this in an idle state then we found the starting bits to a key packet
	static constexpr uint8_t WTVKB_START_BITCNT = 5;
	static constexpr uint32_t WTVKB_START_MASK  = ((1 << WTVKB_START_BITCNT) - 1);
	// Sejin key packet start bit is 0b0, then a bit sequence of 0b0,0b0,0b1,0b0 (making it 0b01000 or 0x08)
	static constexpr uint32_t WTVKB_START_MARK  = 0x08;

	static constexpr uint32_t WTVKB_IS_BREAK     = 0x80;
	static constexpr uint8_t WTVKB_KEYCODE_SHIFT = 12;
	static constexpr uint32_t WTVKB_KEYCODE_MASK = 0x7f;

	// This is a cheap check to make sure the bits match 0b1xxxxxxxxxx1x1xxxx1xxx
	// Proper parity checks will be more accurate
	static constexpr uint32_t WTVKB_DATA_GOOD_MASK = 0x200508;

	wtvir_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto ir_code_callback() { return m_ir_code_cb.bind(); }

	wtvir_device_base::ir_old_value_t get_irin_old_value();

private:

	typedef struct
	{
		uint32_t min;
		uint32_t max;
	} threshold_t;

	enum phillips_state_t
	{
		PHILLIPS_RC6_IN_SFT,
		PHILLIPS_RC6_IN_6T,
		PHILLIPS_RC6_IN_2T,
		PHILLIPS_RC6_IN_SYMBOLS
	};

	typedef struct
	{
		uint8_t t_value;
		threshold_t threshold;
	} phillips_t_value_t;

	// Transition interval duration thresholds (how long a pulse or space state holds before flipping states)
	// These values are arbitrary. IR clock values were pulled from a WebTV build and adjusted to work with LIRC us durations.
	// all thresholds are (MIN, MAX)
	//
	// This is the min and max of the pulse interval time
	static constexpr uint32_t PHILLIPS_RC6_SFT_MIN   = 6500; // Signal Free Time
	static constexpr threshold_t PHILLIPS_RC6_LEADER_6T = { .min = 5447, .max = 8400 };
	static constexpr threshold_t PHILLIPS_RC6_LEADER_2T = { .min = 1677, .max = 3100 };
	// We're adding both the pulse and the following space interval time for all of these thresholds.
	// The symbol to bit value depends on the last bit in phillips_bits 
	static constexpr phillips_t_value_t PHILLIPS_RC6_SYM_DEF[] = {
		{ .t_value = 2, .threshold = { .min = 1870, .max = 2495 } },
		{ .t_value = 3, .threshold = { .min = 3005, .max = 3572 } },
		{ .t_value = 4, .threshold = { .min = 4137, .max = 4705 } },
		//{ .t_value = 5, .threshold = { .min = 5272, .max = 5840 } },
		//{ .t_value = 6, .threshold = { .min = 6405, .max = 6972 } },
	};
	// Seems there can only be a total of 20 bits with Phillips WebTV remotes
	static constexpr uint8_t PHILLIPS_RC6_MAX_BITS = 20;

	enum sony_state_t
	{
		SONY_SIRC_IN_GUIDE,
		SONY_SIRC_IN_TLEAD,
		SONY_SIRC_IN_BITS,
	};

	// Sony's SIRC protocol works off of multiples of 600us
	static constexpr double SONY_SIRC_T          = 600;
	// Some error factor for the min and max threshold
	static constexpr double SONY_SIRC_MIN_ADJ    = 0.83;
	static constexpr double SONY_SIRC_MAX_ADJ    = 1.20;
	// The start (or guide in the WebTV code) is T*4 for 2400us
	static constexpr double SONY_SIRC_GUIDE_US = SONY_SIRC_T * 4;
	static constexpr threshold_t SONY_SIRC_GUIDE = { .min = static_cast<uint32_t>(SONY_SIRC_GUIDE_US * SONY_SIRC_MIN_ADJ), .max = static_cast<uint32_t>(SONY_SIRC_GUIDE_US * SONY_SIRC_MAX_ADJ) };
	// After the start space, there's a normal 600us pulse (TLEAD)
	static constexpr threshold_t SONY_SIRC_TLEAD = { .min = static_cast<uint32_t>(SONY_SIRC_T * SONY_SIRC_MIN_ADJ), .max = static_cast<uint32_t>(SONY_SIRC_T * SONY_SIRC_MAX_ADJ) };
	// A "0" bit is T/600us pulse and then a T/600us space
	static constexpr double SONY_SIRC_0BIT_US    = 2.0 * SONY_SIRC_T;
	static constexpr threshold_t SONY_SIRC_0BIT  = { .min = static_cast<uint32_t>(SONY_SIRC_0BIT_US * SONY_SIRC_MIN_ADJ), .max = static_cast<uint32_t>(SONY_SIRC_0BIT_US * SONY_SIRC_MAX_ADJ) };
	// A "1" bit is 2*T/1200Us pulse then a T/600us space
	static constexpr double SONY_SIRC_1BIT_US    = 3.0 * SONY_SIRC_T;
	static constexpr threshold_t SONY_SIRC_1BIT  = { .min = static_cast<uint32_t>(SONY_SIRC_1BIT_US * SONY_SIRC_MIN_ADJ), .max = static_cast<uint32_t>(SONY_SIRC_1BIT_US * SONY_SIRC_MAX_ADJ) };
	// Seems there can only be a max of 20 bits with Sony WebTV remotes
	static constexpr uint8_t SONY_SIRC_BITCNT    = 20;
	// Mask to tell if this is a WebTV button rather than a TV mode button
	static constexpr uint32_t SONY_SIRC_WTV_BTN  = 0xf0000000;
	// I'm also capturing when the Sony remote is in TV more, which is 12 bits
	static constexpr uint8_t SONY_SIRC_SBITCNT   = 12;
	static constexpr uint32_t SONY_SIRC_TV_BTN   = 0x10000000;
	// This is for the 15-bit Sony UltimateTV remote
	static constexpr uint8_t SONY_SIRC_UBITCNT   = 15;
	static constexpr uint32_t SONY_SIRC_UTV_BTN  = 0x60000000;

	void send_decoded_state(uint8_t key_flags, uint8_t primary, uint8_t secondary = 0x00, uint8_t remote_button = 0x00);

	void prime_wtvkb_state();
	void reset_wtvkb_state();
	bool validate_wtvkb_parity();
	void push_wtvkb_ir_event(bool ir_state, uint32_t interval);

	bool remote_within_threshold(threshold_t threshold, uint32_t period);

	void reset_phillips_state();
	void phillips_push_button();
	uint8_t phillips_get_symbol_t_value();
	void phillips_push_bits(uint8_t t);
	void phillips_push_ir_event(bool ir_state, uint32_t interval);

	void reset_sony_state();
	void sony_push_button();
	void sony_push_ir_event(bool ir_state, uint32_t interval);

	void reset_remote_state();
	void push_remote_ir_event(bool ir_state, uint32_t interval);

	void poll_lirc_in() override;

	devcb_write_line m_ir_code_cb;

	uint32_t m_wtvkb_bits;
	uint8_t m_wtvkb_bit_idx;
	bool m_wtvkb_packet_started;

	phillips_state_t m_phillips_state;
	uint32_t m_phillips_period;
	uint32_t m_phillips_bits;
	uint8_t m_phillips_bit_idx;

	sony_state_t m_sony_state;
	uint32_t m_sony_period;
	uint32_t m_sony_bits;
	uint8_t m_sony_bit_idx;

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

};

DECLARE_DEVICE_TYPE(WTVIR, wtvir_decoder_device)

class wtvir_sejin_device : public wtvir_device_base
{
public:

	static constexpr uint32_t SEJIN_SCANCODE_COUNT    =  128;
	static constexpr uint32_t SEJIN_DEFAULT_IR_DATA   =  0x200508;
	static constexpr uint32_t SEJIN_DATA_BIT_COUNT    =  22;

	wtvir_sejin_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, bool lirc_in_enabled = true);

	bool enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data) override;

private:

	uint8_t calculate_odd_parity(uint32_t data, uint8_t bit_start, uint8_t bit_end);

	void poll_buttons() override;

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

#endif // MAME_WEBTV_WTVIR_H
