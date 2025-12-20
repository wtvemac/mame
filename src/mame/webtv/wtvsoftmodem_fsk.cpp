// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here



#include "wtvsoftmodem_fsk.h"
#include <cstdio>

wtvsoftmodem_fsk::wtvsoftmodem_fsk()
{
	uint8_t set_fsk_spec_count = (sizeof(wtvsoftmodem_fsk::DEFAULT_FSK_SPEC_TABLE) / sizeof(wtvsoftmodem_fsk::DEFAULT_FSK_SPEC_TABLE[0]));
	set_fsk_spec_table(wtvsoftmodem_fsk::DEFAULT_FSK_SPEC_TABLE, set_fsk_spec_count);
}

void wtvsoftmodem_fsk::set_fsk_spec_table(const fsk_detail_t* set_fsk_spec_table, uint8_t set_fsk_spec_count)
{
	fsk_spec_table = set_fsk_spec_table;
	fsk_spec_count = set_fsk_spec_count;
}

void wtvsoftmodem_fsk::set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate)
{
	sample_rate = set_sample_rate;
}

void wtvsoftmodem_fsk::init(fsk_spec_t fsk_spec)
{
	for (uint8_t fdix = 0; fdix < fsk_spec_count; fdix++)
	{
		if (fsk_spec_table[fdix].spec == fsk_spec)
		{
			wtvsoftmodem_fsk::init(&fsk_spec_table[fdix]);
			break;
		}
	}
}
void wtvsoftmodem_fsk::init(const fsk_detail_t* fsk_detail)
{
	wtvsoftmodem_fsk::tx_init(fsk_detail);
	wtvsoftmodem_fsk::rx_init(fsk_detail);
}

void wtvsoftmodem_fsk::tx_init(const fsk_detail_t* fsk_detail)
{
	for (uint32_t fidx = 0; fidx < fsk_detail->channel_count; fidx++)
	{
		mod_state.channel[fidx].pcurrent = 0;
		wtvsoftmodem_dsp::frequency_t bit0_f = fsk_detail->channel[fidx].center_f + fsk_detail->channel[fidx].bit0_fd;
		mod_state.channel[fidx].bit0_padvance = dsp.get_padvance(bit0_f, sample_rate);

		wtvsoftmodem_dsp::frequency_t bit1_f = fsk_detail->channel[fidx].center_f + fsk_detail->channel[fidx].bit1_fd;
		mod_state.channel[fidx].bit1_padvance = dsp.get_padvance(bit1_f, sample_rate);

		mod_state.channel[fidx].bcurrent = 0;
		mod_state.channel[fidx].badvance = (fsk_detail->channel[fidx].baud_rate / sample_rate) * BAUD_PADJUST_BASE;
	}

	wtvsoftmodem_fsk::set_tx_channel(fsk_detail->default_tx_channel);

	mod_state.current_bit = 0;
	mod_state.enabled = false;
}
void wtvsoftmodem_fsk::set_tx_channel(uint8_t channel_index)
{
	if (channel_index < wtvsoftmodem_fsk::MAX_CHANNELS)
		mod_state.channel_index = channel_index;
}
void wtvsoftmodem_fsk::set_tx_enabled(bool enabled)
{
	mod_state.enabled = enabled;
}

void wtvsoftmodem_fsk::rx_init(const fsk_detail_t* fsk_detail)
{
	for (uint32_t fidx = 0; fidx < fsk_detail->channel_count; fidx++)
	{
		demod_state.channel[fidx].samples_per_bit = (uint32_t)(sample_rate / fsk_detail->channel[fidx].baud_rate);

		wtvsoftmodem_dsp::frequency_t bit0_f = fsk_detail->channel[fidx].center_f + fsk_detail->channel[fidx].bit0_fd;
		dsp.populate_iq_table(&demod_state.channel[fidx].bit0, demod_state.channel[fidx].samples_per_bit, bit0_f, sample_rate);

		wtvsoftmodem_dsp::frequency_t bit1_f = fsk_detail->channel[fidx].center_f + fsk_detail->channel[fidx].bit1_fd;
		dsp.populate_iq_table(&demod_state.channel[fidx].bit1, demod_state.channel[fidx].samples_per_bit, bit1_f, sample_rate);

		memset(demod_state.channel[fidx].sample_buffer, 0, (sizeof(wtvsoftmodem_dsp::sample_t) * SAMPLE_BUFFER_SIZE));
		demod_state.channel[fidx].sample_buffer_index = 0;

		demod_state.channel[fidx].bcurrent = 0;
		demod_state.channel[fidx].badvance = (fsk_detail->channel[fidx].baud_rate / sample_rate) * BAUD_PADJUST_BASE;
		demod_state.channel[fidx].balign = demod_state.channel[fidx].badvance >> 2;

		int32_t spb_n = demod_state.channel[fidx].bit0.table_size >> 2;
		demod_state.channel[fidx].scaling_shift = 0;
		while (spb_n != 0)
		{
			demod_state.channel[fidx].scaling_shift++;
			spb_n >>= 1;
		}
	}

	wtvsoftmodem_fsk::set_rx_channel(fsk_detail->default_rx_channel);
}
void wtvsoftmodem_fsk::set_rx_channel(uint8_t channel_index)
{
	if (channel_index < wtvsoftmodem_fsk::MAX_CHANNELS)
		demod_state.channel_index = channel_index;
}
