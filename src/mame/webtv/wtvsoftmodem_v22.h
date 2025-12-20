// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_WTVSOFTMODEM_V22_H
#define MAME_MACHINE_WTVSOFTMODEM_V22_H

#pragma once

#include "wtvsoftmodem_fsk.h"
#include "wtvsoftmodem_framing.h"

class wtvsoftmodem_v22 : public wtvsoftmodem_fsk
{

public:

	typedef std::function<void(uint8_t)> rx_callback_func;

	wtvsoftmodem_v22(bool use_mode1 = false);

	void set_rx_byte_callback(rx_callback_func cb)
	{
		rx_byte_cb = cb;
	}

	inline bool transmit_buffer_full()
	{
		return (tx_buffer_size >= MAX_TX_BUFFER_SIZE);
	}

	inline bool transmit_buffer_empty()
	{
		return (tx_buffer_size == 0);
	}

	inline void transmit_byte(uint8_t byte)
	{
		if (!transmit_buffer_full())
		{
			if (transmit_buffer_empty())
				frame8n1::build_frame_for_byte(&tx_frame_state, byte);

			tx_buffer[tx_buffer_size] = byte;
			tx_buffer_size++;
		}
	}

	inline void transmit_le_8n1_bit(bool bit)
	{
		if (frame8n1::parse_le_frame_bit(&in_frame_parser, bit))
			transmit_byte(in_frame_parser.byte);
	}

	inline void transmit_be_8n1_bit(bool bit)
	{
		if (frame8n1::parse_be_frame_bit(&in_frame_parser, bit))
			transmit_byte(in_frame_parser.byte);
	}

private:

	rx_callback_func rx_byte_cb;

	frame8n1::parser_state_t rx_frame_state;

	frame8n1::parser_state_t in_frame_parser;
	frame8n1::builder_state_t tx_frame_state;

	static constexpr uint16_t MAX_TX_BUFFER_SIZE = 256;
	uint8_t tx_buffer[MAX_TX_BUFFER_SIZE];
	uint8_t tx_buffer_size;
	uint8_t tx_buffer_index;

	void reset_rx();
	virtual void push_rx_bit(bool bit) override;

	void reset_tx();
	virtual bool pop_tx_bit() override;

};

#endif // MAME_MACHINE_WTVSOFTMODEM_V22_H