
// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvir.h"
#include "natkeyboard.h"
#include <iterator>

wtvir_device_base::wtvir_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, bool lirc_in_enabled) :
	device_t(mconfig, type, tag, owner, clock),
	m_sample_fifo_trigger_cb(*this),
	m_lirc(*this, "lirc"),
	m_lirc_in_enabled(lirc_in_enabled)
{
}

TIMER_CALLBACK_MEMBER(wtvir_device_base::poll_timer)
{
	poll();
}

void wtvir_device_base::device_start()
{
	save_item(NAME(m_lirc_in_enabled));

	save_item(NAME(m_queued_button_head));
	save_item(NAME(m_queued_button_tail));

	m_queued_button_head = 0x0;
	m_queued_button_tail = 0x0;
	
	m_fifo_data_bit_count = 7;
	m_waiting_for_fifo_read = false;

	m_irin_old_value.reset();
	m_irin_old_value.set_primary_data(IR_MICROCODE_VERSION);

	m_irin_sample_interval = 0x0;
	m_irin_reject_interval = 0x0;
	m_irin_statcntl = 0x0;
	m_irin_bit_sample_clock_cnt = DEFAULT_BIT_SAMPLE_CLOCKS;

	save_item(NAME(m_lirc_in_head));
	save_item(NAME(m_lirc_in_tail));

	m_lirc_in_head = 0;
	m_lirc_in_tail = 0;

	m_input_timer = timer_alloc(FUNC(wtvir_device_base::poll_timer), this);
}

void wtvir_device_base::device_reset()
{
	m_input_timer->adjust(attotime::from_msec(5), 0, attotime::from_hz(POLL_RATE_HZ));

	m_irin_statcntl &= (~0x2); // set reset bit back to 0
}

void wtvir_device_base::device_add_mconfig(machine_config &config)
{
	LIRC(config, m_lirc);
}

void wtvir_device_base::enable(int state)
{
	if (state)
		m_input_timer->adjust(attotime::from_msec(5), 0, attotime::from_hz(POLL_RATE_HZ));
	else
		m_input_timer->adjust(attotime::never);
}

bool wtvir_device_base::lirc_in_enabled()
{
	return (m_lirc_in_enabled && m_lirc->is_enabled());
}

uint32_t wtvir_device_base::data_r(offs_t offset)
{
	uint32_t result = 0x00000000;
	
	// Convert to map()?
	switch (offset)
	{
		case DEV_IROLD:
			// not implemented
			result |= m_irin_old_value.get_raw();
			break;

		case DEV_IRIN_SAMPLE:
			result = m_irin_sample_interval & 0xffff;
			break;

		case DEV_IRIN_REJECT_INT:
			result = m_irin_reject_interval & 0xff;
			break;

		case DEV_IRIN_TRANS_DATA:
			result = wtvir_device_base::get_ir_in_data();
			break;

		case DEV_IRIN_STATCNTL:
			result = m_irin_statcntl & 0xff;
			break;
	}

	return result;
}

void wtvir_device_base::data_w(offs_t offset, uint32_t data)
{
	switch (offset)
	{
		case DEV_IRIN_SAMPLE:
			m_irin_sample_interval = data & 0xffff;
			break;

		case DEV_IRIN_REJECT_INT:
			m_irin_reject_interval = data & 0xff;
			break;

		case DEV_IRIN_STATCNTL:
			{
				m_irin_statcntl = data & 0xff;

				if (m_irin_statcntl & 0x2)
				{
					device_reset();
				}
			}
			break;
	}
}

uint32_t wtvir_device_base::get_ir_in_data()
{
	// It needs to be less 2 because less 1 (meaning we're full: current + max-1 left) makes the WebTV OS/firmware reset thinking there's an overflow
	uint8_t fifo_samples_left_max = (wtvir_device_base::MAX_SAMPLE_FIFO_ENTRIES - 2);

	uint8_t fifo_samples_left = 0x0;
	bool bit_val = 0;
	uint16_t sample_clock_cnt = 0x0;

	ir_button_state_t* active_key = current_button();

	if (active_key != nullptr)
	{
		bit_val = ((active_key->ir_data & (1 << active_key->trans_bit_index)) != 0x0);

		uint8_t bit_cnt = 1;
		
		for (uint8_t sample_fifo_idx = 0; sample_fifo_idx < wtvir_device_base::MAX_SAMPLE_FIFO_ENTRIES; sample_fifo_idx++)
		{
			active_key->trans_bit_index++;

			if (active_key->trans_bit_index < m_fifo_data_bit_count)
			{
				bool this_bit_val = ((active_key->ir_data & (1 << active_key->trans_bit_index)) != 0x0);

				if (this_bit_val != bit_val)
					break;
				else
					bit_cnt++;
			}
			else
			{
				break;
			}
		}

		sample_clock_cnt = bit_cnt * m_irin_bit_sample_clock_cnt;

		if (queued_button_count() > 1)
			// If we have more than one button event queued then set to max minus 2.
			fifo_samples_left = fifo_samples_left_max;
		else
			fifo_samples_left = (m_fifo_data_bit_count - active_key->trans_bit_index);

		if (active_key->trans_bit_index >= m_fifo_data_bit_count)
			dequeue_button();
	}
	else if(lirc_in_enabled())
	{
		uint32_t lirc_state = dequeue_lirc_in();

		if (lirc_state != lirc_device::LIRC_INVALID_DATA)
		{
			fifo_samples_left = queued_lirc_in_count() + 1;
			bit_val = (lirc_state & lirc_device::LIRC_IS_PULSE_BIT);
			double state_us = static_cast<double>(lirc_state & lirc_device::LIRC_INTERVAL_MASK);
			// Divide transision microseconds (us) by DEFAULT_SAMPLE_CLOCK_US and handle rounding
			// To produce the mock clock count
			sample_clock_cnt = static_cast<uint16_t>(std::round(state_us / wtvir_device_base::DEFAULT_SAMPLE_CLOCK_US));
		}
	}

	m_waiting_for_fifo_read = false;

	fifo_samples_left = std::min(fifo_samples_left, fifo_samples_left_max);

	return wtvir_device_base::build_solo_ir_in_data(fifo_samples_left, bit_val, sample_clock_cnt);
}

uint32_t wtvir_device_base::build_solo_ir_in_data(uint8_t fifo_cnt, bool bit_val, uint16_t sample_clocks)
{
	uint32_t data = 0x00000000;

	if (fifo_cnt > 0)
	{
		//
		//  IR transition register data bits:
		//
		//  SSSS | V | TTTTTTTTTTT
		//
		//    SSSS        = the number of transition entries in the FIFO buffer
		//    V           = value of the current bit
		//    TTTTTTTTTTT = the time of the current bit transition measured in sample clocks. 
		//                  1 sample clock is defined in the register DEV_IR_IN_SAMPLE_TICKS 
		//                  which is number of system clock cycles per sample clock.
		//
		data |= ((fifo_cnt      & 0x00f) << 12);
		data |= ((bit_val       & 0x001) << 11);
		data |= ((sample_clocks & 0x7ff) <<  0);
	}

	return data;
}

bool wtvir_device_base::enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data)
{
	if (queued_button_count() < MAX_QUEUED_BUTTONS && ((m_queued_button_head + 1) != m_queued_button_tail))
	{
		m_queued_buttons[m_queued_button_head] = {
			.scancode = scancode,
			.is_make = is_make,
			.trans_bit_index = 0x0,
			.ir_data = ir_data
		};

		m_queued_button_head++;
		m_queued_button_head &= (MAX_QUEUED_BUTTONS - 1);

		return true;
	}
	else
	{
		return false;
	}
}

uint8_t wtvir_device_base::queued_button_count()
{
	return (m_queued_button_head - m_queued_button_tail) & (MAX_QUEUED_BUTTONS - 1);
}

wtvir_device_base::ir_button_state_t* wtvir_device_base::current_button()
{
	if (m_queued_button_head != m_queued_button_tail)
		return &m_queued_buttons[m_queued_button_tail];
	else
		return nullptr;
}

wtvir_device_base::ir_button_state_t* wtvir_device_base::dequeue_button()
{
	if (m_queued_button_head != m_queued_button_tail)
	{
		ir_button_state_t* state = &m_queued_buttons[m_queued_button_tail];

		m_queued_button_tail++;
		m_queued_button_tail &= (MAX_QUEUED_BUTTONS - 1);

		return state;
	}
	else
	{
		return nullptr;
	}
}

bool wtvir_device_base::enqueue_lirc_in(uint32_t state)
{
	if (queued_lirc_in_count() < LIRC_SAMPLE_FIFO_ENTRIES && ((m_lirc_in_head + 1) != m_lirc_in_tail))
	{
		uint32_t state_us = (state & lirc_device::LIRC_INTERVAL_MASK);

		if (state_us > LIRC_MAX_US)
			state = lirc_device::LIRC_INVALID_DATA;

		lirc_in_fifo[m_lirc_in_head] = state;

		m_lirc_in_head++;
		m_lirc_in_head &= (LIRC_SAMPLE_FIFO_ENTRIES - 1);

		return true;
	}
	else
	{
		return false;
	}
}

uint8_t wtvir_device_base::queued_lirc_in_count()
{
	return (m_lirc_in_head - m_lirc_in_tail) & (LIRC_SAMPLE_FIFO_ENTRIES - 1);
}

uint32_t wtvir_device_base::current_lirc_in()
{
	if (m_lirc_in_head != m_lirc_in_tail)
		return lirc_in_fifo[m_lirc_in_tail];
	else
		return lirc_device::LIRC_INVALID_DATA;
}

uint32_t wtvir_device_base::dequeue_lirc_in()
{
	if (m_lirc_in_head != m_lirc_in_tail)
	{
		uint32_t state = lirc_in_fifo[m_lirc_in_tail];

		m_lirc_in_tail++;
		m_lirc_in_tail &= (LIRC_SAMPLE_FIFO_ENTRIES - 1);

		return state;
	}
	else
	{
		return lirc_device::LIRC_INVALID_DATA;
	}
}

void wtvir_device_base::poll_buttons()
{
	//
}

void wtvir_device_base::poll_lirc_in()
{
	while(true)
	{
		uint32_t lirc_state = m_lirc->read();

		if (lirc_state == lirc_device::LIRC_INVALID_DATA || !enqueue_lirc_in(lirc_state))
			break;
	}
}

void wtvir_device_base::poll()
{
	poll_buttons();

	if(lirc_in_enabled())
		poll_lirc_in();

	bool have_waiting_buttons = (queued_button_count() >= 1 && !m_waiting_for_fifo_read);
	bool lirc_in_ready = (queued_lirc_in_count() > wtvir_device_base::MAX_SAMPLE_FIFO_ENTRIES);

	bool should_trigger_fifo_int = (have_waiting_buttons || lirc_in_ready);

	if (have_waiting_buttons)
		m_waiting_for_fifo_read = true;

	if (should_trigger_fifo_int)
		m_sample_fifo_trigger_cb(1);
}

DEFINE_DEVICE_TYPE(WTVIR, wtvir_decoder_device, "wtvir", "WebTV IR Decoder")

wtvir_decoder_device::wtvir_decoder_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	wtvir_device_base(mconfig, WTVIR, tag, owner, clock, true),
	m_ir_code_cb(*this)
{
}

void wtvir_decoder_device::device_start()
{
	save_item(NAME(m_wtvkb_bits));
	save_item(NAME(m_wtvkb_bit_idx));
	save_item(NAME(m_wtvkb_packet_started));

	wtvir_decoder_device::reset_wtvkb_state();
	wtvir_decoder_device::reset_remote_state();

	wtvir_device_base::device_start();
}

void wtvir_decoder_device::device_reset()
{
	wtvir_device_base::device_reset();

	if (!lirc_in_enabled())
		m_input_timer->adjust(attotime::never);
}

wtvir_device_base::ir_old_value_t wtvir_decoder_device::get_irin_old_value()
{
	return m_irin_old_value;
}

void wtvir_decoder_device::send_decoded_state(uint8_t key_flags, uint8_t primary, uint8_t secondary, uint8_t remote_button)
{
	m_irin_old_value.reset();

	m_irin_old_value.set_key_flags(key_flags);
	m_irin_old_value.set_primary_data(primary);
	m_irin_old_value.set_secondary_data(secondary);
	m_irin_old_value.set_remote_button(remote_button);

	m_ir_code_cb(1);
}

void wtvir_decoder_device::prime_wtvkb_state()
{
	m_wtvkb_bit_idx = wtvir_decoder_device::WTVKB_START_BITCNT;
	m_wtvkb_packet_started = true;
}

void wtvir_decoder_device::reset_wtvkb_state()
{
	m_wtvkb_bits = 0x00000000;
	m_wtvkb_bit_idx = 0;
	m_wtvkb_packet_started = false;
}

bool wtvir_decoder_device::validate_wtvkb_parity()
{
	uint32_t id_byte = (m_wtvkb_bits >> 1) & 0xFF;

	int id_byte_cparity = (std::popcount(id_byte) % 2 == 0) ? 1 : 0;
	uint32_t id_byte_fparity = (m_wtvkb_bits >> 9) & 1;

	uint32_t data_byte = (m_wtvkb_bits >> 11) & 0x1FF;

	int data_byte_cparity = (std::popcount(data_byte) % 2 == 0) ? 1 : 0;
	uint32_t data_byte_fparity = (m_wtvkb_bits >> 20) & 1;

	return (id_byte_cparity == id_byte_fparity) && (data_byte_cparity == data_byte_fparity);
}

void wtvir_decoder_device::push_wtvkb_ir_event(bool ir_state, uint32_t interval)
{
	uint8_t bit_count = static_cast<uint8_t>(std::round(static_cast<double>(interval) / wtvir_decoder_device::WTVKB_TIME_PER_BIT));
	bit_count = std::min(std::max(wtvir_decoder_device::WTVKB_MIN_BITCNT, bit_count), wtvir_decoder_device::WTVKB_MAX_BITCNT);

	if (ir_state)
		m_wtvkb_bits |= ((1 << bit_count) - 1) << m_wtvkb_bit_idx;

	m_wtvkb_bit_idx += bit_count;

	if (m_wtvkb_bit_idx >= wtvir_decoder_device::WTVKB_MAX_BITCNT)
	{
		if (m_wtvkb_packet_started && (m_wtvkb_bits & wtvir_decoder_device::WTVKB_DATA_GOOD_MASK) == wtvir_decoder_device::WTVKB_DATA_GOOD_MASK)
		{
			if (wtvir_decoder_device::validate_wtvkb_parity())
			{
				// Set Sejin ID byte as the key flag
				// Strip off start bit (>>1) and parity + stop bit + data byte (&0xff)
				uint8_t id_byte = (m_wtvkb_bits >> 1) & 0xff;

				// Set Sejin data byte as primary data (minus start bit and parity + stop bit)
				uint8_t data_byte = (m_wtvkb_bits >> wtvir_decoder_device::WTVKB_KEYCODE_SHIFT) & wtvir_decoder_device::WTVKB_KEYCODE_MASK;

				wtvir_decoder_device::send_decoded_state(id_byte, data_byte);
			}
		}

		wtvir_decoder_device::reset_wtvkb_state();
	}
	else if(m_wtvkb_bit_idx >= wtvir_decoder_device::WTVKB_START_BITCNT and !m_wtvkb_packet_started)
	{
		uint32_t start_check = m_wtvkb_bits >> (m_wtvkb_bit_idx - wtvir_decoder_device::WTVKB_START_BITCNT);

		if (start_check == wtvir_decoder_device::WTVKB_START_MARK)
			wtvir_decoder_device::prime_wtvkb_state();
	}
}

bool wtvir_decoder_device::remote_within_threshold(threshold_t threshold, uint32_t period)
{
	return threshold.min <= period and period <= threshold.max;
}

void wtvir_decoder_device::reset_phillips_state()
{
	m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_SFT;
	m_phillips_period = 0;
	m_phillips_bits = 0x00000000;
	m_phillips_bit_idx = 0;
}

void wtvir_decoder_device::phillips_push_button()
{
	// Convert (our) Phillips button to Sony remote button
	// This is because our Phillips decoder dosn't decode the bits properly but it's somewhat identifiable to convert to a Sony button press.

	uint8_t sony_button = 0xff;

	switch (m_phillips_bits)
	{
		case 0x1bb6f: // Power
			sony_button = 0x15;
			break;
		case 0x3b6f: // Up
			sony_button = 0x74;
			break;
		case 0x6d8: // Down
			sony_button = 0x75;
			break;
		case 0x1b6f: // Left
			sony_button = 0x34;
			break;
		case 0x36d8: // Right
			sony_button = 0x33;
			break;

		//case 0xdb6f: // Channel up
		case 0x7ed8: // Channel down
			sony_button = 0x91;
			break;
		//case 0x3b68: // Listing

		case 0x6ed8: // 0
			sony_button = 0x09;
			break;
		case 0x19b6f: // 1
			sony_button = 0x00;
			break;
		case 0xced8: // 2
			sony_button = 0x01;
			break;
		case 0xc6d8: // 3
			sony_button = 0x02;
			break;
		case 0x1db6f: // 4
			sony_button = 0x03;
			break;
		case 0xeed8: // 5
			sony_button = 0x04;
			break;
		case 0xe6d8: // 6
			sony_button = 0x05;
			break;
		case 0xf6d8: // 7
			sony_button = 0x06;
			break;
		case 0xf36f: // 8
			sony_button = 0x07;
			break;
		case 0xfb6f: // 9
			sony_button = 0x08;
			break;

		case 0xdb6f: // Go
			sony_button = 0x65;
			break;
		case 0x1ed8: // Return
			sony_button = 0x0b;
			break;
		case 0x336f: // Page up
			sony_button = 0x48;
			break;
		case 0xed8: // Page down
			sony_button = 0x59;
			break;

		case 0x76d8: // Home
			sony_button = 0x62;
			break;
		case 0x1b6d8: // Back
			sony_button = 0x4c;
			break;
		case 0xe36f: // View
			sony_button = 0x5c;
			break;
		case 0x7b6f: // Options
			sony_button = 0x61;
			break;
		case 0x66d8: // Recent
			sony_button = 0x4b;
			break;
		//case 0x3b6f: // Info
		//case 0x36f: // Phillips smart connect

		default:
			break;
	}

	if (sony_button != 0xff)
		wtvir_decoder_device::send_decoded_state(0x00, 0x0f, 0x3a, sony_button);

	wtvir_decoder_device::reset_phillips_state();
}

uint8_t wtvir_decoder_device::phillips_get_symbol_t_value()
{
	for (const auto& sym_def : wtvir_decoder_device::PHILLIPS_RC6_SYM_DEF)
	{
		if (wtvir_decoder_device::remote_within_threshold(sym_def.threshold, m_phillips_period))
			return sym_def.t_value;
	}

	return 0;
}

void wtvir_decoder_device::phillips_push_bits(uint8_t t)
{
	uint8_t bit_cnt = 0;
	uint32_t bit_value = 0;

	if (m_phillips_bits & 1) // last bit 1
	{
		switch (t)
		{
			case 2:
				bit_cnt = 1;
				bit_value = 1;
				break;
			case 3:
				bit_cnt = 1;
				bit_value = 0;
				break;
			default:
				wtvir_decoder_device::reset_phillips_state();
				break;
		}
	}
	else // last bit 0
	{
		switch (t)
		{
			case 2:
				bit_cnt = 1;
				bit_value = 0;
				break;
			case 3:
				bit_cnt = 2;
				bit_value = 3;
				break;
			case 4:
				bit_cnt = 2;
				bit_value = 2;
				break;
			default:
				// Add last bit
				if (m_phillips_bit_idx == (wtvir_decoder_device::PHILLIPS_RC6_MAX_BITS - 1))
				{
					bit_cnt = 1;
					bit_value = 1;
				}
				else
				{
					wtvir_decoder_device::reset_phillips_state();
				}
				break;
		}
	}

	if (bit_cnt > 0)
	{
		m_phillips_bits = (m_phillips_bits << bit_cnt) | bit_value;
		m_phillips_bit_idx += bit_cnt;
	}
}

void wtvir_decoder_device::phillips_push_ir_event(bool ir_state, uint32_t interval)
{
	switch(m_phillips_state)
	{
		case wtvir_decoder_device::PHILLIPS_RC6_IN_SFT:
			if (ir_state && interval >= wtvir_decoder_device::PHILLIPS_RC6_SFT_MIN)
				m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_6T;
			break;

		case wtvir_decoder_device::PHILLIPS_RC6_IN_6T:
			// On the box this would be "not ir_state" but it seems this is always on the pulse transition with LIRC
			if (ir_state)
			{
				if (wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::PHILLIPS_RC6_LEADER_6T, interval))
				{
					wtvir_decoder_device::reset_phillips_state();
					m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_2T;
				}
			}
			break;

		case wtvir_decoder_device::PHILLIPS_RC6_IN_2T:
			if (ir_state)
			{
				if (wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::PHILLIPS_RC6_LEADER_2T, interval))
				{
					m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_SYMBOLS;
					m_phillips_period = 0;
				}
			}
			break;

		case wtvir_decoder_device::PHILLIPS_RC6_IN_SYMBOLS:
			if (ir_state)
			{
				if (wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::PHILLIPS_RC6_LEADER_6T, interval))
				{
					wtvir_decoder_device::phillips_push_button();
					m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_2T;
				}
				else if (interval >= wtvir_decoder_device::PHILLIPS_RC6_SFT_MIN)
				{
					wtvir_decoder_device::phillips_push_button();
					m_phillips_state = wtvir_decoder_device::PHILLIPS_RC6_IN_6T;
				}
				else
				{
					m_phillips_period = interval;
				}
			}
			else if (m_phillips_period > 0)
			{
				m_phillips_period += interval;

				uint8_t t = wtvir_decoder_device::phillips_get_symbol_t_value();

				if (t != 0)
				{
					// The code on the box catches when the bit position is 3, checks if the value is 0x8
					// then go into another mode where it translates the bits differently
					// I'm keeping this since each button still has a unique signature this way even if the bits don't match
					wtvir_decoder_device::phillips_push_bits(t);
				}
				else
				{
					wtvir_decoder_device::reset_phillips_state();
				}
			}
			break;
		
		default:
			break;
	}
}

void wtvir_decoder_device::reset_sony_state()
{
	m_sony_state = wtvir_decoder_device::SONY_SIRC_IN_GUIDE;
	m_sony_period = 0;
	m_sony_bits = 0x00000000;
	m_sony_bit_idx = 0;
}

void wtvir_decoder_device::sony_push_button()
{
    if (m_sony_bit_idx == wtvir_decoder_device::SONY_SIRC_BITCNT)
		wtvir_decoder_device::send_decoded_state(0x00, (m_sony_bits >> 28), (m_sony_bits >> 20), (m_sony_bits >> 13));

	wtvir_decoder_device::reset_sony_state();
}

void wtvir_decoder_device::sony_push_ir_event(bool ir_state, uint32_t interval)
{
	if (interval > wtvir_decoder_device::SONY_SIRC_GUIDE.max)
		wtvir_decoder_device::reset_sony_state();

	switch (m_sony_state)
	{
		case wtvir_decoder_device::SONY_SIRC_IN_GUIDE:
			if (!ir_state && wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::SONY_SIRC_GUIDE, interval))
				m_sony_state = wtvir_decoder_device::SONY_SIRC_IN_TLEAD;
			break;
		case wtvir_decoder_device::SONY_SIRC_IN_TLEAD:
			if (ir_state && wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::SONY_SIRC_TLEAD, interval))
				m_sony_state = wtvir_decoder_device::SONY_SIRC_IN_BITS;
			else
				wtvir_decoder_device::reset_sony_state();
			break;
		case wtvir_decoder_device::SONY_SIRC_IN_BITS:
			if (ir_state)
			{
				m_sony_period += interval;

				if (wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::SONY_SIRC_1BIT, m_sony_period))
				{
					m_sony_bits >>= 1;
					m_sony_bits |= 0x80000000;
					m_sony_bit_idx += 1;
				}
				else if (wtvir_decoder_device::remote_within_threshold(wtvir_decoder_device::SONY_SIRC_0BIT, m_sony_period))
				{
					m_sony_bits >>= 1;
					m_sony_bit_idx += 1;
				}
				else
				{
					wtvir_decoder_device::reset_sony_state();
				}

				// standard 20-bit WebTV remote codes
				if (m_sony_bit_idx == (wtvir_decoder_device::SONY_SIRC_BITCNT - 1))
				{
					m_sony_bit_idx = wtvir_decoder_device::SONY_SIRC_BITCNT;
					wtvir_decoder_device::sony_push_button();
				}
			}
			else
			{
				m_sony_period = interval;
			}
			break;

		default:
			break;
	}
}

void wtvir_decoder_device::reset_remote_state()
{
	wtvir_decoder_device::reset_phillips_state();
	wtvir_decoder_device::reset_sony_state();
}

void wtvir_decoder_device::push_remote_ir_event(bool ir_state, uint32_t interval)
{
	wtvir_decoder_device::phillips_push_ir_event(ir_state, interval);
	wtvir_decoder_device::sony_push_ir_event(ir_state, interval);
}

void wtvir_decoder_device::poll_lirc_in()
{
	while(true)
	{
		uint32_t lirc_state = m_lirc->read();

		if (lirc_state == lirc_device::LIRC_INVALID_DATA)
			break;

		bool ir_state = (lirc_state & lirc_device::LIRC_IS_PULSE_BIT);
		uint32_t interval = lirc_state & lirc_device::LIRC_INTERVAL_MASK;

		wtvir_decoder_device::push_wtvkb_ir_event(ir_state, interval);
		wtvir_decoder_device::push_remote_ir_event(ir_state, interval);
	}
}


DEFINE_DEVICE_TYPE(SEJIN_KBD, wtvir_sejin_device, "sejinkbd", "Sejin IR Keyboard")

wtvir_sejin_device::wtvir_sejin_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, bool lirc_in_enabled) :
	wtvir_device_base(mconfig, SEJIN_KBD, tag, owner, clock, lirc_in_enabled),
	m_ioport(*this, "wtvir_kbd%u", 0)
{
}

void wtvir_sejin_device::device_start()
{
	wtvir_device_base::device_start();

	save_item(NAME(m_device_id));
	save_item(NAME(m_port_state));

	m_device_id = 0x01;

	m_fifo_data_bit_count = wtvir_sejin_device::SEJIN_DATA_BIT_COUNT;

	std::fill(std::begin(m_port_state), std::end(m_port_state), 0);
}

void wtvir_sejin_device::device_reset()
{
	wtvir_device_base::device_reset();
}

uint8_t wtvir_sejin_device::calculate_odd_parity(uint32_t data, uint8_t bit_start, uint8_t bit_count)
{
	uint8_t parity_bit = 0x0;

	uint8_t bit_end = (bit_start + bit_count);

	if (bit_end < 0x20)
	{
		uint8_t bit_count = 0;

		for (uint8_t bit_idx = bit_start; bit_idx < bit_end; bit_idx++)
		{
			if ((data & (1 << bit_idx)) != 0x0)
			{
				bit_count++;
			}
		}

		if ((bit_count & 0x1) != 0x0)
		{
			parity_bit = 0x1;
		}
		else
		{
			parity_bit = 0x0;
		}
	}

	return parity_bit;
}

bool wtvir_sejin_device::enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data)
{
	//
	//  Sejin IR keyboard data bits:
	//
	//  [1 P 0 SSSSSSS 0] [1 P 1 M DD 0100 0]
	//
	//    DD      = ID bit, usually 10 for WebTV's keyboard (10 = 103/86 keyboard)
	//    M       = 0=make (key press), 1=break (key release)
	//    P       = odd parity bit
	//    SSSSSSS = scancode for the key pressed
	//
	ir_data |= ((m_device_id & 0x03) <<  5);
	ir_data |= ((scancode    & 0x7f) << 12);
	ir_data |= ((is_make     & 0x01) <<  7);

	ir_data |= ((calculate_odd_parity(ir_data,  0, 11) & 0x1) <<  9);
	ir_data |= ((calculate_odd_parity(ir_data, 11, 11) & 0x1) << 20);

	return wtvir_device_base::enqueue_button(scancode, is_make, ir_data);
}

void wtvir_sejin_device::poll_buttons()
{
	if (queued_button_count() < wtvir_device_base::MAX_QUEUED_BUTTONS)
	{
		for (uint8_t port_idx = 0x0; port_idx < (wtvir_sejin_device::SEJIN_SCANCODE_COUNT >> 4); port_idx++)
		{
			uint32_t prev_state = m_port_state[port_idx];
			uint32_t curr_state = wtvir_sejin_device::readport(port_idx);

			uint32_t state_diff = prev_state ^ curr_state;

			if (state_diff != 0x0)
			{
				for (uint8_t key_idx = 0x0; key_idx < 0x10; key_idx++)
				{
					uint32_t key_bitmask = (0x1 << key_idx);

					if (state_diff & key_bitmask)
					{
						uint8_t scancode = ((port_idx << 4) | key_idx);
						bool is_make = ((prev_state & key_bitmask) != 0x0);

						enqueue_button(scancode, is_make, wtvir_sejin_device::SEJIN_DEFAULT_IR_DATA);
					}
				}
			}

			m_port_state[port_idx] = curr_state;
		}
	}
}

uint32_t wtvir_sejin_device::readport(int port)
{
	if ((port < m_ioport.size()) && m_ioport[port].found())
	{
		return m_ioport[port]->read();
	}
	else
	{
		return 0x0;
	}
}

INPUT_PORTS_START(wtvir_kbd)

	PORT_START("wtvir_kbd0")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x00 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x01 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x02 UNUSED */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x03 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("` ~")             PORT_CODE(KEYCODE_TILDE)      PORT_CHAR('`')                     PORT_CHAR('~')      /* 0x04 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: View")       PORT_CODE(KEYCODE_F12)        PORT_CHAR(UCHAR_MAMEKEY(F12))                          /* 0x05 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x06 UNUSED */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x07 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x08 UNUSED */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Caps Lock")       PORT_CODE(KEYCODE_CAPSLOCK)                                                          /* 0x09 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X")               PORT_CODE(KEYCODE_X)          PORT_CHAR('X')                                         /* 0x0a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0b UNUSED */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0c UNUSED */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2 @")             PORT_CODE(KEYCODE_2)          PORT_CHAR('2')                     PORT_CHAR('@')      /* 0x0d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S")               PORT_CODE(KEYCODE_S)          PORT_CHAR('S')                                         /* 0x0e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W")               PORT_CODE(KEYCODE_W)          PORT_CHAR('W')                                         /* 0x0f */

	PORT_START("wtvir_kbd1")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x10 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x11 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C")               PORT_CODE(KEYCODE_C)          PORT_CHAR('C')                                         /* 0x12 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x13 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x14 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3 #")             PORT_CODE(KEYCODE_3)          PORT_CHAR('3')                     PORT_CHAR('#')      /* 0x15 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D")               PORT_CODE(KEYCODE_D)          PORT_CHAR('D')                                         /* 0x16 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E")               PORT_CODE(KEYCODE_E)          PORT_CHAR('E')                                         /* 0x17 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Alt")       PORT_CODE(KEYCODE_RALT)                                                              /* 0x18 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Tab")             PORT_CODE(KEYCODE_TAB)        PORT_CHAR(0x09)                                        /* 0x19 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z")               PORT_CODE(KEYCODE_Z)          PORT_CHAR('Z')                                         /* 0x1a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Power")      PORT_CODE(KEYCODE_F1)         PORT_CHAR(UCHAR_MAMEKEY(F1))                           /* 0x1b (not used but F1 is 0x0c) */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Command")    PORT_CODE(KEYCODE_LCONTROL)   PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))                     /* 0x1c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1 !")             PORT_CODE(KEYCODE_1)          PORT_CHAR('1')                      PORT_CHAR('!')     /* 0x1d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A")               PORT_CODE(KEYCODE_A)          PORT_CHAR('A')                                         /* 0x1e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q")               PORT_CODE(KEYCODE_Q)          PORT_CHAR('Q')   

	PORT_START("wtvir_kbd2")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B")               PORT_CODE(KEYCODE_B)          PORT_CHAR('B')                                         /* 0x20 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T")               PORT_CODE(KEYCODE_T)          PORT_CHAR('T')                                         /* 0x21 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V")               PORT_CODE(KEYCODE_V)          PORT_CHAR('V')                                         /* 0x22 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G")               PORT_CODE(KEYCODE_G)          PORT_CHAR('G')                                         /* 0x23 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5 %")             PORT_CODE(KEYCODE_5)          PORT_CHAR('5')                      PORT_CHAR('%')     /* 0x24 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4 $")             PORT_CODE(KEYCODE_4)          PORT_CHAR('4')                      PORT_CHAR('$')     /* 0x25 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F")               PORT_CODE(KEYCODE_F)          PORT_CHAR('F')                                         /* 0x26 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R")               PORT_CODE(KEYCODE_R)          PORT_CHAR('R')                                         /* 0x27 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Alt")        PORT_CODE(KEYCODE_LALT)                                                              /* 0x28 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Esc")             PORT_CODE(KEYCODE_ESC)                                                               /* 0x29 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x2a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Control")         PORT_CODE(KEYCODE_RCONTROL)   PORT_CHAR(UCHAR_MAMEKEY(RCONTROL))                     /* 0x2b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0c UNUSED */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Goto")       PORT_CODE(KEYCODE_F7)         PORT_CHAR(UCHAR_MAMEKEY(F7))                           /* 0x2d (not used but F7 is 0x61) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0e UNUSED */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Send")       PORT_CODE(KEYCODE_F8)         PORT_CHAR(UCHAR_MAMEKEY(F8))                           /* 0x2f */

	PORT_START("wtvir_kbd3")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x30 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Shift")      PORT_CODE(KEYCODE_LSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(LSHIFT))                       /* 0x31 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Shift")     PORT_CODE(KEYCODE_RSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))                       /* 0x32 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x33 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x34 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x35 UNUSED */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Channel Down")    PORT_CODE(KEYCODE_PRTSCR)                                                            /* 0x36 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Channel Up")      PORT_CODE(KEYCODE_PAUSE)                                                             /* 0x37 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Space")           PORT_CODE(KEYCODE_SPACE)      PORT_CHAR(' ')                                         /* 0x38 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Backspace")       PORT_CODE(KEYCODE_BACKSPACE)  PORT_CHAR(0x08)                                        /* 0x39 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Return")          PORT_CODE(KEYCODE_ENTER)      PORT_CHAR(0x0d)                                        /* 0x3a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x3b UNUSED */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Home")       PORT_CODE(KEYCODE_7_PAD)      PORT_CODE(KEYCODE_HOME)                                /* 0x3c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Search")     PORT_CODE(KEYCODE_F3)         PORT_CHAR(UCHAR_MAMEKEY(F3))                           /* 0x3d (not used but F3 is 0x11) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("\\ |")            PORT_CODE(KEYCODE_BACKSLASH)  PORT_CHAR('\\')                     PORT_CHAR('|')     /* 0x3e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x3f UNUSED */

	PORT_START("wtvir_kbd4")
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x41 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x42 UNUSED */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x43 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x44 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x45 UNUSED */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x46 (WebTV's FN key) */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x47 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left")           PORT_CODE(KEYCODE_LEFT)       PORT_CODE(KEYCODE_4_PAD)                                /* 0x48 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x49 UNUSED */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Save")      PORT_CODE(KEYCODE_F9)         PORT_CHAR(UCHAR_MAMEKEY(F9))                            /* 0x4a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Up")             PORT_CODE(KEYCODE_UP)         PORT_CODE(KEYCODE_8_PAD)                                /* 0x4b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Edit")      PORT_CODE(KEYCODE_INSERT)     PORT_CODE(KEYCODE_0_PAD)                                /* 0x4c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x4d UNUSED */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Back")      PORT_CODE(KEYCODE_1_PAD)      PORT_CODE(KEYCODE_END)                                  /* 0x4e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x4f UNUSED */


	PORT_START("wtvir_kbd5")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Down")           PORT_CODE(KEYCODE_DOWN)       PORT_CODE(KEYCODE_2_PAD)                                /* 0x50 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x51 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Info")      PORT_CODE(KEYCODE_F6)         PORT_CHAR(UCHAR_MAMEKEY(F6))                            /* 0x52 (not used but F6 is 0x3b) */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x53 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Options")   PORT_CODE(KEYCODE_F11)        PORT_CHAR(UCHAR_MAMEKEY(F11))                           /* 0x54 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Mail")      PORT_CODE(KEYCODE_F4)         PORT_CHAR(UCHAR_MAMEKEY(F4))                            /* 0x55 (not used but F4 is 0x13) */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Scroll Up")      PORT_CODE(KEYCODE_PGUP)       PORT_CODE(KEYCODE_9_PAD)                                /* 0x56 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x57 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right")          PORT_CODE(KEYCODE_RIGHT)      PORT_CODE(KEYCODE_6_PAD)                                /* 0x58 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x59 UNUSED */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Scroll Down")    PORT_CODE(KEYCODE_PGDN)       PORT_CODE(KEYCODE_3_PAD)                                /* 0x5b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Recents")   PORT_CODE(KEYCODE_F10)        PORT_CHAR(UCHAR_MAMEKEY(F10))                           /* 0x5c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Find")      PORT_CODE(KEYCODE_F5)         PORT_CHAR(UCHAR_MAMEKEY(F5))                            /* 0x5d (not used but F5 is 0x0b) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5e UNUSED */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5f UNUSED */

	PORT_START("wtvir_kbd6")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x60 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x61 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(". >")             PORT_CODE(KEYCODE_STOP)       PORT_CHAR('.')                      PORT_CHAR('>')     /* 0x62 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x63 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Favorites")  PORT_CODE(KEYCODE_F2)         PORT_CHAR(UCHAR_MAMEKEY(F2))                           /* 0x64 (not used but F2 is 0x14) */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9 (")             PORT_CODE(KEYCODE_9)          PORT_CHAR('9')                      PORT_CHAR('(')     /* 0x65 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L")               PORT_CODE(KEYCODE_L)          PORT_CHAR('L')                                         /* 0x66 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O")               PORT_CODE(KEYCODE_O)          PORT_CHAR('O')                                         /* 0x67 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/ ?")             PORT_CODE(KEYCODE_SLASH)      PORT_CHAR('/')                      PORT_CHAR('?')     /* 0x68 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("[ {")             PORT_CODE(KEYCODE_OPENBRACE)  PORT_CHAR('[')                      PORT_CHAR('{')     /* 0x69 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x6a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("' \"")            PORT_CODE(KEYCODE_QUOTE)      PORT_CHAR('\'')                     PORT_CHAR('\"')    /* 0x6b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("- _")             PORT_CODE(KEYCODE_MINUS)      PORT_CHAR('-')                      PORT_CHAR('_')     /* 0x6c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0 )")             PORT_CODE(KEYCODE_0)          PORT_CHAR('0')                      PORT_CHAR(')')     /* 0x6d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("; :")             PORT_CODE(KEYCODE_COLON)      PORT_CHAR(';')                      PORT_CHAR(':')     /* 0x6e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P")               PORT_CODE(KEYCODE_P)          PORT_CHAR('P')                                         /* 0x6f */

	PORT_START("wtvir_kbd7")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x70 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("] }")             PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']')                      PORT_CHAR('}')     /* 0x71 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(", <")             PORT_CODE(KEYCODE_COMMA)      PORT_CHAR(',')                      PORT_CHAR('<')     /* 0x72 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x73 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("= +")             PORT_CODE(KEYCODE_EQUALS)     PORT_CHAR('=')                      PORT_CHAR('+')     /* 0x74 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8 *")             PORT_CODE(KEYCODE_8)          PORT_CHAR('8')                      PORT_CHAR('*')     /* 0x75 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K")               PORT_CODE(KEYCODE_K)          PORT_CHAR('K')                                         /* 0x76 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I")               PORT_CODE(KEYCODE_I)          PORT_CHAR('I')                                         /* 0x77 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N")               PORT_CODE(KEYCODE_N)          PORT_CHAR('N')                                         /* 0x78 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y")               PORT_CODE(KEYCODE_Y)          PORT_CHAR('Y')                                         /* 0x79 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M")               PORT_CODE(KEYCODE_M)          PORT_CHAR('M')                                         /* 0x7a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")               PORT_CODE(KEYCODE_H)          PORT_CHAR('H')                                         /* 0x7b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6 ^")             PORT_CODE(KEYCODE_6)          PORT_CHAR('6')                      PORT_CHAR('^')     /* 0x7c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7 &")             PORT_CODE(KEYCODE_7)          PORT_CHAR('7')                      PORT_CHAR('&')     /* 0x7d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J")               PORT_CODE(KEYCODE_J)          PORT_CHAR('J')                                         /* 0x7e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")               PORT_CODE(KEYCODE_U)          PORT_CHAR('U')                                         /* 0x7f */

INPUT_PORTS_END

ioport_constructor wtvir_sejin_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(wtvir_kbd);
}