// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v22.h"
#include <cstdio>

wtvsoftmodem_v22::wtvsoftmodem_v22(bool use_mode1) :
	wtvsoftmodem_fsk()
{
	wtvsoftmodem_fsk::set_sample_rate(wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE);

	if (use_mode1)
		wtvsoftmodem_fsk::init(wtvsoftmodem_fsk::FSK_V23_MODE1);
	else
		wtvsoftmodem_fsk::init(wtvsoftmodem_fsk::FSK_V23_MODE2);

	wtvsoftmodem_fsk::set_tx_enabled(true);

	rx_byte_cb = nullptr;

	wtvsoftmodem_v22::reset_rx();
	wtvsoftmodem_v22::reset_tx();
}

void wtvsoftmodem_v22::reset_rx()
{
	frame8n1::parse_start(&rx_frame_state);
	frame8n1::parse_start(&in_frame_parser);
}

void wtvsoftmodem_v22::push_rx_bit(bool bit)
{
	if (frame8n1::parse_le_frame_bit(&rx_frame_state, bit) && rx_byte_cb != nullptr)
		rx_byte_cb(rx_frame_state.byte);
}

void wtvsoftmodem_v22::reset_tx()
{
	frame8n1::build_frame_for_byte(&tx_frame_state, 0x00);

	tx_buffer_size = 0;
	tx_buffer_index = 0;
}

bool wtvsoftmodem_v22::pop_tx_bit()
{
	if (tx_buffer_size > 0)
	{
		bool bit = frame8n1::build_le_frame_bit(&tx_frame_state);

		if (tx_frame_state.hit_end)
		{
			tx_buffer_index++;

			if (tx_buffer_index >= tx_buffer_size)
				wtvsoftmodem_v22::reset_tx();
			else
				frame8n1::build_frame_for_byte(&tx_frame_state, tx_buffer[tx_buffer_index]);
		}

		return bit;

	}
	else
	{
		return 1;
	}
}
