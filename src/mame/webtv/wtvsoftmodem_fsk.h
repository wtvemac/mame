// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_WTVSOFTMODEM_FSK_H
#define MAME_MACHINE_WTVSOFTMODEM_FSK_H

#pragma once

#include "wtvsoftmodem_dsp.h"
#include <cstring>
#include <functional>
#include <cstdio>

class wtvsoftmodem_fsk
{

public:

	static constexpr uint8_t    MAX_CHANNELS            = 8;

	typedef uint8_t fsk_spec_t;

	typedef struct fsk_channel
	{
		wtvsoftmodem_dsp::frequency_t center_f;
		wtvsoftmodem_dsp::frequency_t bit0_fd;
		wtvsoftmodem_dsp::frequency_t bit1_fd;
		wtvsoftmodem_dsp::frequency_t baud_rate;
	} fsk_channel_t;

	typedef struct fsk_detail
	{
		fsk_spec_t spec;
		uint8_t channel_count;
		fsk_channel_t channel[MAX_CHANNELS];
		uint8_t default_tx_channel;
		uint8_t default_rx_channel;
	} fsk_detail_t;

	static constexpr fsk_spec_t FSK_V21                 = 0x01;
	static constexpr fsk_spec_t FSK_V21_110             = 0x02;
	static constexpr fsk_spec_t FSK_V23_MODE1           = 0x03;
	static constexpr fsk_spec_t FSK_V23_MODE2           = 0x04;
	static constexpr fsk_spec_t FSK_BELL103             = 0x05;
	static constexpr fsk_spec_t FSK_BELL202             = 0x06;
	static constexpr fsk_spec_t FSK_WEITBRECHT_45_45    = 0x07;
	static constexpr fsk_spec_t FSK_WEITBRECHT_47_60    = 0x08;
	static constexpr fsk_spec_t FSK_WEITBRECHT_50_00    = 0x09;

	wtvsoftmodem_fsk();

	void set_fsk_spec_table(const fsk_detail_t* set_fsk_detail_table, uint8_t set_fsk_detail_count);
	void set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate);

	void init(fsk_spec_t fsk_spec);
	void init(const fsk_detail_t* fsk_detail);
	
	void tx_init(const fsk_detail_t* fsk_detail);
	void set_tx_channel(uint8_t channel_index);
	inline wtvsoftmodem_dsp::sample_t tx_sample()
	{
		if (mod_state.enabled)
		{
			mod_state.channel[mod_state.channel_index].bcurrent += mod_state.channel[mod_state.channel_index].badvance;

			if (mod_state.current_bit)
				mod_state.channel[mod_state.channel_index].pcurrent += mod_state.channel[mod_state.channel_index].bit1_padvance;
			else
				mod_state.channel[mod_state.channel_index].pcurrent += mod_state.channel[mod_state.channel_index].bit0_padvance;

			wtvsoftmodem_dsp::sample_t cur_sample = dsp.sin(mod_state.channel[mod_state.channel_index].pcurrent).sample;

			if (mod_state.channel[mod_state.channel_index].bcurrent >= BAUD_PADJUST_BASE)
			{
				mod_state.channel[mod_state.channel_index].bcurrent -= BAUD_PADJUST_BASE;

				mod_state.current_bit = pop_tx_bit();
			}

			return cur_sample;
		}
		else
		{
			return 0;
		}
	}
	inline uint32_t tx(int32_t* sample, uint32_t sample_count)
	{
		for(uint32_t i = 0; i < sample_count; i++)
		{
			sample[i] = tx_sample();
		}

		return sample_count;
	}
	void set_tx_enabled(bool enabled);

	void rx_init(const fsk_detail_t* fsk_detail);
	void set_rx_channel(uint8_t channel_index);
	inline void rx(const int32_t* sample, uint32_t sample_count)
	{
		for(uint32_t sidx = 0; sidx < sample_count; sidx++)
		{
			demod_state.channel[demod_state.channel_index].sample_buffer[demod_state.channel[demod_state.channel_index].sample_buffer_index & SAMPLE_BUFFER_MASK] = (wtvsoftmodem_dsp::sample_t)sample[sidx] >> demod_state.channel[demod_state.channel_index].scaling_shift;
			demod_state.channel[demod_state.channel_index].sample_buffer_index++;

			int32_t bit1_result = get_bit_confidence(&demod_state.channel[demod_state.channel_index], 1);
			int32_t bit0_result = get_bit_confidence(&demod_state.channel[demod_state.channel_index], 0);

			if (bit1_result > 0 && bit0_result > 0)
			{
				bool current_bit = (bit1_result > bit0_result);

				if (current_bit != demod_state.channel[demod_state.channel_index].last_bit)
				{
					demod_state.channel[demod_state.channel_index].last_bit = current_bit;

					if (demod_state.channel[demod_state.channel_index].bcurrent >= (BAUD_PADJUST_BASE / 2))
						demod_state.channel[demod_state.channel_index].bcurrent -= demod_state.channel[demod_state.channel_index].balign;
					else
						demod_state.channel[demod_state.channel_index].bcurrent += demod_state.channel[demod_state.channel_index].balign;
				}

				demod_state.channel[demod_state.channel_index].bcurrent += demod_state.channel[demod_state.channel_index].badvance;
				
				if (demod_state.channel[demod_state.channel_index].bcurrent >= BAUD_PADJUST_BASE)
				{
					demod_state.channel[demod_state.channel_index].bcurrent -= BAUD_PADJUST_BASE;

					push_rx_bit(current_bit);
				}
			}
		}
	}

private:

	static constexpr uint32_t   SAMPLE_BUFFER_SIZE      = 256;
	static constexpr uint32_t   SAMPLE_BUFFER_MASK      = (SAMPLE_BUFFER_SIZE - 1);
	static constexpr int32_t    BAUD_PADJUST_BASE       = 0x10000;

	typedef struct channel_tx_state
	{
		wtvsoftmodem_dsp::phase_t pcurrent;
		wtvsoftmodem_dsp::phase_t bit0_padvance;
		wtvsoftmodem_dsp::phase_t bit1_padvance;
		int32_t bcurrent;
		int32_t badvance;
	} channel_tx_state_t;

	typedef struct modulate_state
	{
		channel_tx_state_t channel[MAX_CHANNELS];
		uint8_t channel_index;
		bool enabled;
		bool current_bit;
	} modulate_state_t;

	typedef struct channel_in_state
	{
		wtvsoftmodem_dsp::iqtable_t bit0;
		wtvsoftmodem_dsp::iqtable_t bit1;
		int32_t bcurrent;
		int32_t badvance;
		int32_t balign;
		int32_t scaling_shift;
		uint32_t samples_per_bit;
		bool last_bit;
		uint32_t sample_buffer_index;
		wtvsoftmodem_dsp::sample_t sample_buffer[SAMPLE_BUFFER_SIZE];
	} channel_in_state_t;

	typedef struct demodulate_state
	{
		channel_in_state_t channel[MAX_CHANNELS];
		uint8_t channel_index;
	} demodulate_state_t;

	static constexpr fsk_detail_t DEFAULT_FSK_SPEC_TABLE[] = {
		{
			.spec = FSK_V21,
			.channel_count = 2,
			.channel = {
				{
					.center_f = 1080,
					.bit0_fd = 100,
					.bit1_fd = -100,
					.baud_rate = 300
				},
				{
					.center_f = 1750,
					.bit0_fd = 100,
					.bit1_fd = -100,
					.baud_rate = 300
				}
			},
			.default_tx_channel = 1,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_V21_110,
			.channel_count = 2,
			.channel = {
				{
					.center_f = 1080,
					.bit0_fd = 100,
					.bit1_fd = -100,
					.baud_rate = 110
				},
				{
					.center_f = 420,
					.bit0_fd = 30,
					.bit1_fd = -30,
					.baud_rate = 300
				},
			},
			.default_tx_channel = 1,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_V23_MODE1,
			.channel_count = 2,
			.channel = {
				{
					.center_f = 1500,
					.bit0_fd = 200,
					.bit1_fd = -200,
					.baud_rate = 600
				},
				{
					.center_f = 420,
					.bit0_fd = 30,
					.bit1_fd = -30,
					.baud_rate = 75
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 1
		},
		{
			.spec = FSK_V23_MODE2,
			.channel_count = 2,
			.channel = {
				{
					.center_f = 1700,
					.bit0_fd = 400,
					.bit1_fd = -400,
					.baud_rate = 1200
				},
				{
					.center_f = 420,
					.bit0_fd = 30,
					.bit1_fd = -30,
					.baud_rate = 75
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 1
		},
		{
			.spec = FSK_BELL103,
			.channel_count = 2,
			.channel = {
				{
					.center_f = 2125,
					.bit0_fd = -100,
					.bit1_fd = 100,
					.baud_rate = 300
				},
				{
					.center_f = 1170,
					.bit0_fd = -100,
					.bit1_fd = 100,
					.baud_rate = 300
				}
			},
			.default_tx_channel = 1,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_BELL202,
			.channel_count = 1,
			.channel = {
				{
					.center_f = 1700,
					.bit0_fd = 500,
					.bit1_fd = -500,
					.baud_rate = 1200
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_WEITBRECHT_45_45,
			.channel_count = 1,
			.channel = {
				{
					.center_f = 1600,
					.bit0_fd = 200,
					.bit1_fd = -200,
					.baud_rate = 45.45
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_WEITBRECHT_47_60,
			.channel_count = 1,
			.channel = {
				{
					.center_f = 1600,
					.bit0_fd = 200,
					.bit1_fd = -200,
					.baud_rate = 47.60
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 0
		},
		{
			.spec = FSK_WEITBRECHT_50_00,
			.channel_count = 1,
			.channel = {
				{
					.center_f = 1600,
					.bit0_fd = 200,
					.bit1_fd = -200,
					.baud_rate = 50
				}
			},
			.default_tx_channel = 0,
			.default_rx_channel = 0
		}
	};

	wtvsoftmodem_dsp dsp;

	modulate_state_t mod_state;
	demodulate_state_t demod_state;

	uint8_t fsk_spec_count;
	const fsk_detail_t* fsk_spec_table;

	wtvsoftmodem_dsp::frequency_t sample_rate;

	virtual bool pop_tx_bit() { return 0; }

	virtual void push_rx_bit(bool bit) { }

	inline int32_t get_bit_confidence(channel_in_state_t* channel_in_state, bool bit_value)
	{
		wtvsoftmodem_dsp::iqtable_t* iqtable = (bit_value) ? &channel_in_state->bit1 : &channel_in_state->bit0;
		uint32_t sidx_start = (channel_in_state->sample_buffer_index - iqtable->table_size);

		int32_t result = dsp.get_iq_similarity(iqtable, channel_in_state->sample_buffer, sidx_start, SAMPLE_BUFFER_MASK);

		return result;
	}

};

#endif // MAME_MACHINE_WTVSOFTMODEM_FSK_H