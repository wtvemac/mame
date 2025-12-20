// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v8.h"
#include <cstdio>

wtvsoftmodem_v8::wtvsoftmodem_v8() :
	wtvsoftmodem_tone(),
	wtvsoftmodem_fsk()
{
	wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::IDLE);

	tx_buffer_size = 0;

	uint8_t set_frequency_count = (sizeof(wtvsoftmodem_v8::V8_FREQ_TABLE) / sizeof(wtvsoftmodem_v8::V8_FREQ_TABLE[0]));
	wtvsoftmodem_v8::set_freq_table(wtvsoftmodem_v8::V8_FREQ_TABLE, set_frequency_count, v8_dft_state);

	uint8_t set_tone_count = (sizeof(wtvsoftmodem_v8::V8_TONE_TABLE) / sizeof(wtvsoftmodem_v8::V8_TONE_TABLE[0]));
	wtvsoftmodem_v8::set_tone_table(wtvsoftmodem_v8::V8_TONE_TABLE, set_tone_count);

	wtvsoftmodem_v8::set_sample_rate(wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE);

	v8_tx_state = wtvsoftmodem_v8::V8OutState::DISABLED;

	wtvsoftmodem_fsk::init(wtvsoftmodem_fsk::FSK_V21);
}

void wtvsoftmodem_v8::set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate)
{
	wtvsoftmodem_tone::set_sample_rate(set_sample_rate);
	wtvsoftmodem_fsk::set_sample_rate(set_sample_rate);
}

bool wtvsoftmodem_v8::add_tx_capability(menu_item_t item)
{
	if (!wtvsoftmodem_v8::is_tx_capable(item))
	{
		tx_capabilities.push_back(item);

		return true;
	}
	else
	{
		return false;
	}
}
bool wtvsoftmodem_v8::is_tx_capable(menu_item_t item)
{
	return (std::find(tx_capabilities.begin(), tx_capabilities.end(), item) != tx_capabilities.end());
}
vector<wtvsoftmodem_v8::menu_item_t>& wtvsoftmodem_v8::get_tx_capabilities()
{
	return tx_capabilities;
}

bool wtvsoftmodem_v8::add_rx_capability(menu_item_t item)
{
	if (!wtvsoftmodem_v8::is_rx_capable(item))
	{
		rx_capabilities.push_back(item);

		return true;
	}
	else
	{
		return false;
	}
	
}
bool wtvsoftmodem_v8::is_rx_capable(menu_item_t item)
{
	return (std::find(rx_capabilities.begin(), rx_capabilities.end(), item) != rx_capabilities.end());
}
vector<wtvsoftmodem_v8::menu_item_t>& wtvsoftmodem_v8::get_rx_capabilities()
{
	return rx_capabilities;
}

void wtvsoftmodem_v8::tone_completed()
{
	if (v8_state == wtvsoftmodem_v8::V8State::SND_ANSAM)
	{
		wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::SNT_ANSAM);
	}
}

void wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State new_state)
{
	wtvsoftmodem_v8::V8State old_state = v8_state;

	v8_state = new_state;

	if (old_state != new_state && state_change_cb != nullptr)
		state_change_cb(old_state, new_state);
}

wtvsoftmodem_v8::V8State wtvsoftmodem_v8::get_current_state()
{
	return v8_state;
}

void wtvsoftmodem_v8::set_state_change_callback(wtvsoftmodem_v8::state_change_callback_func cb)
{
	state_change_cb = cb;
}

void wtvsoftmodem_v8::start(bool disable_echo_suppressors)
{
	wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::SND_ANSAM);

	rx_bit_sync = 0;
	rx_buffer_size = 0;
	rx_blank_byte_count = 0;
	rx_synced = wtvsoftmodem_v8::BIT_SYNC_NONE;
	frame8n1::parse_start(&rx_frame_state);

	tx_sync_header = 0;
	tx_sync_bit_index = 0;
	tx_buffer_size = 0;
	tx_buffer_index = 0;
	frame8n1::build_frame_for_byte(&tx_frame_state, 0x00);
	tx_repeated_count = 0;

	if (disable_echo_suppressors)
		wtvsoftmodem_tone::tx_init(wtvsoftmodem_v8::TONE_V8_XANSAM);
	else
		wtvsoftmodem_tone::tx_init(wtvsoftmodem_v8::TONE_V8_ANSAM);
}

void wtvsoftmodem_v8::stop()
{
	wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::IDLE);
}

void wtvsoftmodem_v8::push_rx_bit(bool bit)
{
	rx_bit_sync = ((rx_bit_sync << 1) | bit) & wtvsoftmodem_v8::BIT_SYNC_MASK;

	if (rx_bit_sync == wtvsoftmodem_v8::BIT_SYNC_CMJM)
	{
		if (rx_synced == wtvsoftmodem_v8::BIT_SYNC_CMJM && rx_buffer_size > 0)
		{
			wtvsoftmodem_v8::parse_menu();
		}

		rx_synced = wtvsoftmodem_v8::BIT_SYNC_CMJM;
		rx_buffer_size = 0;
		rx_blank_byte_count = 0;

		frame8n1::parse_start(&rx_frame_state);
	}
	else if (rx_synced && frame8n1::parse_be_frame_bit(&rx_frame_state, bit))
	{
		if (rx_frame_state.byte == 0x00)
		{
			rx_blank_byte_count++;

			if (v8_state == wtvsoftmodem_v8::V8State::SND_JM && rx_blank_byte_count >= 3)
				wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::NEGOTIATED);
		}
		else if (rx_buffer_size < wtvsoftmodem_v8::MAX_RX_BUFFER_SIZE)
		{
			rx_buffer[rx_buffer_size] = rx_frame_state.byte;
			rx_buffer_size++;
		}

	}
}

void wtvsoftmodem_v8::start_tx()
{
	wtvsoftmodem_v8::reset_tx_state();

	tx_repeated_count = 0;

	wtvsoftmodem_fsk::set_tx_enabled(true);
}

void wtvsoftmodem_v8::repeat_tx()
{
	wtvsoftmodem_v8::reset_tx_state();

	tx_repeated_count++;
}

void wtvsoftmodem_v8::reset_tx_state()
{
	v8_tx_state = wtvsoftmodem_v8::V8OutState::SEND_SYNC;

	tx_sync_bit_index = 0;

	tx_buffer_index = 0;
	frame8n1::build_frame_for_byte(&tx_frame_state, tx_buffer[tx_buffer_index]);
}

bool wtvsoftmodem_v8::get_tx_sync_bit()
{
	bool tx_bit = 0;

	if (tx_sync_bit_index >= 0 && tx_sync_bit_index < wtvsoftmodem_v8::BIT_SYNC_BITS)
		tx_bit = (tx_sync_header >> ((wtvsoftmodem_v8::BIT_SYNC_BITS - 1) - tx_sync_bit_index)) & 0x1;

	tx_sync_bit_index++;

	if (tx_sync_bit_index >= wtvsoftmodem_v8::BIT_SYNC_BITS)
		v8_tx_state = wtvsoftmodem_v8::V8OutState::SEND_BYTES;

	return tx_bit;
}

bool wtvsoftmodem_v8::get_tx_byte_bit()
{
	if (tx_buffer_size > 0)
	{
		bool bit = frame8n1::build_be_frame_bit(&tx_frame_state);

		if (tx_frame_state.hit_end)
		{
			tx_buffer_index++;

			if (tx_buffer_index >= tx_buffer_size)
				wtvsoftmodem_v8::repeat_tx();
			else
				frame8n1::build_frame_for_byte(&tx_frame_state, tx_buffer[tx_buffer_index]);
		}

		return bit;
	}
	else
	{
		wtvsoftmodem_v8::repeat_tx();

		return 0;
	}
}

bool wtvsoftmodem_v8::pop_tx_bit()
{
	switch (v8_tx_state)
	{
		case SEND_SYNC:
			return wtvsoftmodem_v8::get_tx_sync_bit();

		case SEND_BYTES:
			return wtvsoftmodem_v8::get_tx_byte_bit();

		case DISABLED:
		default:
			return 0;
	}
}

bool wtvsoftmodem_v8::add_tx_byte(uint8_t byte)
{
	if (tx_buffer_size < MAX_TX_BUFFER_SIZE)
	{
		tx_buffer[tx_buffer_size] = byte;
		tx_buffer_size++;

		return true;
	}
	else
	{
		return false;
	}
}

void wtvsoftmodem_v8::prepare_menu_tx_buffer()
{
	tx_sync_header = wtvsoftmodem_v8::BIT_SYNC_CMJM;

	uint8_t modn0_byte = wtvsoftmodem_v8::V8_MENU_CAT_MMODE0 & 0xff;
	uint8_t modn1_byte = wtvsoftmodem_v8::V8_MENU_CAT_MMODE1 & 0xff;
	uint8_t modn2_byte = wtvsoftmodem_v8::V8_MENU_CAT_MMODE2 & 0xff;

	for (const auto &item : tx_capabilities)
	{
		if (((item & wtvsoftmodem_v8::V8_MENU_CAT_MMODE0) == wtvsoftmodem_v8::V8_MENU_CAT_MMODE0))
		{
			modn0_byte |= item;
		}
		else if (((item & wtvsoftmodem_v8::V8_MENU_CAT_MMODE1) == wtvsoftmodem_v8::V8_MENU_CAT_MMODE1))
		{
			modn1_byte |= item;
		}
		else if (((item & wtvsoftmodem_v8::V8_MENU_CAT_MMODE2) == wtvsoftmodem_v8::V8_MENU_CAT_MMODE2))
		{
			modn2_byte |= item;
		}
		else
		{
			wtvsoftmodem_v8::add_tx_byte(item & 0xff);
		}
	}

	wtvsoftmodem_v8::add_tx_byte(modn0_byte & 0xff);
	wtvsoftmodem_v8::add_tx_byte(modn1_byte & 0xff);
	wtvsoftmodem_v8::add_tx_byte(modn2_byte & 0xff);
}

void wtvsoftmodem_v8::send_cm()
{
	wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::SND_CM);

	wtvsoftmodem_v8::send_menu();
}

void wtvsoftmodem_v8::send_jm()
{
	if (v8_state == wtvsoftmodem_v8::V8State::RCV_CM)
	{
		wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::SND_JM);

		wtvsoftmodem_v8::send_menu();
	}
}

void wtvsoftmodem_v8::send_menu()
{
	wtvsoftmodem_v8::prepare_menu_tx_buffer();

	wtvsoftmodem_v8::start_tx();
}

uint32_t wtvsoftmodem_v8::tx(int32_t* sample, uint32_t sample_count)
{
	switch (v8_state)
	{
		case wtvsoftmodem_v8::V8State::SND_ANSAM:
			return wtvsoftmodem_tone::tx(sample, sample_count);

		case wtvsoftmodem_v8::V8State::IDLE:
		case wtvsoftmodem_v8::V8State::NEGOTIATED:
			memset(sample, 0, sizeof(int32_t) * sample_count);
			return 0;

		default:
			return wtvsoftmodem_fsk::tx(sample, sample_count);
	}
}

bool wtvsoftmodem_v8::parse_protocol_menu_item(uint8_t octet)
{
	switch (octet & wtvsoftmodem_v8::V8_MENU_ITM_MASK)
	{
		case (wtvsoftmodem_v8::V8_MENU_PROTO_V42CALLS & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PROTO_V42CALLS);

		case (wtvsoftmodem_v8::V8_MENU_PROTO_EXTCALLS & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PROTO_EXTCALLS);

		default:
			return false;
	}
}

bool wtvsoftmodem_v8::parse_fax_menu_item(uint8_t octet)
{
	return false;
}

bool wtvsoftmodem_v8::parse_call_function_menu_item(uint8_t octet)
{
	switch (octet & wtvsoftmodem_v8::V8_MENU_ITM_MASK)
	{
		case (wtvsoftmodem_v8::V8_MENU_CFUNC_PSTNMULT & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_PSTNMULT);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_TXTPHONE & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_TXTPHONE);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_VIDPHONE & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_VIDPHONE);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_T30TXFAX & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_T30TXFAX);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_T30RXFAX & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_T30RXFAX);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_DATMODEM & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_DATMODEM);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_CALLFEXT & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_CALLFEXT);

		default:
			return false;
	}
}

bool wtvsoftmodem_v8::parse_pcm_modem_menu_item(uint8_t octet)
{
	switch (octet & wtvsoftmodem_v8::V8_MENU_ITM_MASK)
	{
		case (wtvsoftmodem_v8::V8_MENU_CFUNC_V90ANLOG & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_V90ANLOG);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_V90DIGIT & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_V90DIGIT);

		case (wtvsoftmodem_v8::V8_MENU_CFUNC_V91AVAIL & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_V91AVAIL);

		default:
			return false;
	}
}

bool wtvsoftmodem_v8::parse_mmode_menu_item(uint8_t octet, uint8_t mmode_index)
{
	bool found_new_match = false;

	if (mmode_index == 0 && ((octet & wtvsoftmodem_v8::V8_MENU_CAT_MASK) == (wtvsoftmodem_v8::V8_MENU_CAT_MMODE0 & wtvsoftmodem_v8::V8_MENU_CAT_MASK)))
	{
		uint8_t menu_items = octet & wtvsoftmodem_v8::V8_MENU_ITM_MASK;

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM & wtvsoftmodem_v8::V8_MENU_ITM_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD & wtvsoftmodem_v8::V8_MENU_ITM_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V34HALFD & wtvsoftmodem_v8::V8_MENU_ITM_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34HALFD);
		}
	}
	else if(mmode_index == 1 && ((octet & wtvsoftmodem_v8::V8_MENU_MMODE_EXM_MASK) == (wtvsoftmodem_v8::V8_MENU_CAT_MMODE1 & wtvsoftmodem_v8::V8_MENU_MMODE_EXM_MASK)))
	{
		uint8_t menu_items = octet & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK;

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V32AVAIL & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V32AVAIL);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V22AVAIL & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V22AVAIL);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V17AVAIL & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V17AVAIL);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V29HALFD & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V29HALFD);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V27TERAV & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V27TERAV);
		}
	}
	else if(mmode_index == 2 && ((octet & wtvsoftmodem_v8::V8_MENU_MMODE_EXM_MASK) == (wtvsoftmodem_v8::V8_MENU_CAT_MMODE2 & wtvsoftmodem_v8::V8_MENU_MMODE_EXM_MASK)))
	{
		uint8_t menu_items = octet & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK;

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V26TERAV & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V26TERAV);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V26BISAV & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V26BISAV);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V23FULLD & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V23FULLD);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V23HALFD & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V23HALFD);
		}

		if (menu_items & (wtvsoftmodem_v8::V8_MENU_MMODE_V21AVAIL & wtvsoftmodem_v8::V8_MENU_MMODE_EXI_MASK))
		{
			found_new_match |= wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V21AVAIL);
		}
	}

	return found_new_match;
}

bool wtvsoftmodem_v8::parse_pstn_access_menu_item(uint8_t octet)
{
	switch (octet & wtvsoftmodem_v8::V8_MENU_ITM_MASK)
	{
		case (wtvsoftmodem_v8::V8_MENU_PSTNA_DCECCELL & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCECCELL);

		case (wtvsoftmodem_v8::V8_MENU_PSTNA_DCEACELL & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCEACELL);

		case (wtvsoftmodem_v8::V8_MENU_PSTNA_DCEDIGIT & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCEDIGIT);

		case (wtvsoftmodem_v8::V8_MENU_PSTNA_DCEANLOG & wtvsoftmodem_v8::V8_MENU_ITM_MASK):
			return wtvsoftmodem_v8::add_rx_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCEANLOG);

		default:
			return false;
	}
}

bool wtvsoftmodem_v8::parse_nonstd_menu_item(uint8_t octet)
{
	return false;
}

void wtvsoftmodem_v8::parse_menu()
{
	if (rx_buffer_size > 0)
	{
		uint32_t current_menu_checksum = 0;
		uint8_t mmode_index = 0;
		for (uint8_t bidx = 0; bidx < rx_buffer_size; bidx++)
		{
			current_menu_checksum += rx_buffer[bidx];

			if (mmode_index >= 1)
			{
				wtvsoftmodem_v8::parse_mmode_menu_item(rx_buffer[bidx], mmode_index);
				mmode_index++;

				if (mmode_index == 3)
					mmode_index = 0;
			}
			else
			{
				switch (rx_buffer[bidx] & wtvsoftmodem_v8::V8_MENU_CAT_MASK)
				{
					case (wtvsoftmodem_v8::V8_MENU_CAT_PROTO  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_protocol_menu_item(rx_buffer[bidx]);
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_FAXFN  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_fax_menu_item(rx_buffer[bidx]);
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_CFUNC  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_call_function_menu_item(rx_buffer[bidx]);
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_PCMMD  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_pcm_modem_menu_item(rx_buffer[bidx]);
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_MMODE0 & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_mmode_menu_item(rx_buffer[bidx], mmode_index);
						mmode_index++;
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_PSTNA  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_pstn_access_menu_item(rx_buffer[bidx]);
						break;

					case (wtvsoftmodem_v8::V8_MENU_CAT_NSTND  & wtvsoftmodem_v8::V8_MENU_CAT_MASK):
						wtvsoftmodem_v8::parse_nonstd_menu_item(rx_buffer[bidx]);
						break;

					default:
						break;
				}
			}
		}

		// Move on to the next step if we find a repeated menu frame.
		if (current_menu_checksum == previous_menu_checksum)
		{
			if (v8_state == wtvsoftmodem_v8::V8State::SND_CM)
			{
				wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::RCV_JM);
			}
			else if(v8_state < wtvsoftmodem_v8::V8State::RCV_CM)
			{
				wtvsoftmodem_v8::set_current_state(wtvsoftmodem_v8::V8State::RCV_CM);

				wtvsoftmodem_v8::send_jm();
			}

		}
		else
		{
			previous_menu_checksum = current_menu_checksum;
		}
	}
}
