// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_TONE_H
#define MAME_WEBTV_WTVSOFTMODEM_TONE_H

#pragma once

#include "wtvsoftmodem_dsp.h"
#include <functional>

class wtvsoftmodem_tone
{

public:

	typedef uint8_t tone_t;

	typedef uint8_t tone_state_t;

	typedef struct detected_tone
	{
		tone_state_t state;
		tone_t tone;
		int32_t hit_count;
	} detected_tone_t;

	typedef struct amplitude_modulation
	{
		wtvsoftmodem_dsp::frequency_t f;
		double amin;
		double amax;
	} amplitude_modulation_t;

	typedef struct tone_detail
	{
		tone_t tone;
		tone_t next;
		wtvsoftmodem_dsp::frequency_t f[2];
		amplitude_modulation_t am;
		int32_t preverse_ms; // phase reversal every ms
		int32_t detect_duration_ms;
		int32_t play_duration_ms;
	} tone_detail_t;

	static constexpr tone_state_t TONE_NONE       = 0x00; // No tone detected
	static constexpr tone_state_t TONE_INPROGRESS = 0xff; // Tone detection in progress.
	static constexpr tone_state_t TONE_NEW        = 0xfd; // New tone fully detected.
	static constexpr tone_state_t TONE_CONTINUE   = 0xfe; // Previous tone still being detected.
	static constexpr int32_t      TONE_INFINITE   =   -1; // Tone goes on forever.

	static constexpr tone_t       TONE_NULL       = 0x00; // Silence
	static constexpr tone_t       TONE_STOP       = 0x00; // Silence, but used to indicate to stop the tone progression.

	static constexpr tone_t       TONE_DTMF_1     = '1'; // 0x31
	static constexpr tone_t       TONE_DTMF_2     = '2'; // 0x32
	static constexpr tone_t       TONE_DTMF_3     = '3'; // 0x33
	static constexpr tone_t       TONE_DTMF_A     = 'A'; // 0x41
	static constexpr tone_t       TONE_DTMF_4     = '4'; // 0x34
	static constexpr tone_t       TONE_DTMF_5     = '5'; // 0x35
	static constexpr tone_t       TONE_DTMF_6     = '6'; // 0x36
	static constexpr tone_t       TONE_DTMF_B     = 'B'; // 0x42
	static constexpr tone_t       TONE_DTMF_7     = '7'; // 0x37
	static constexpr tone_t       TONE_DTMF_8     = '8'; // 0x38
	static constexpr tone_t       TONE_DTMF_9     = '9'; // 0x39
	static constexpr tone_t       TONE_DTMF_C     = 'C'; // 0x43
	static constexpr tone_t       TONE_DTMF_STAR  = '*'; // 0x2a
	static constexpr tone_t       TONE_DTMF_0     = '0'; // 0x30
	static constexpr tone_t       TONE_DTMF_POUND = '#'; // 0x23
	static constexpr tone_t       TONE_DTMF_D     = 'D'; // 0x44

	static constexpr tone_t       TONE_DIAL_NA    = 0x80;
	static constexpr tone_t       TONE_DIAL_UK    = 0x81;
	static constexpr tone_t       TONE_DIAL_EU    = 0x82;
	static constexpr tone_t       TONE_DIAL_FR    = 0x83;
	static constexpr tone_t       TONE_DIAL_JP    = 0x84;

	wtvsoftmodem_tone();

	void set_freq_table(const wtvsoftmodem_dsp::frequency_t* set_frequency_table, uint8_t set_frequency_count, wtvsoftmodem_dsp::dft_state_t* set_dft_state);
	void set_tone_table(const tone_detail_t* set_tone_table, uint8_t set_tone_count);
	void set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate);

	void tx_init(tone_t tone = wtvsoftmodem_tone::TONE_NULL, int32_t total_samples_override = 0, float aglobal = 1.0, bool play_next = true);
	void tx_init(const tone_detail_t* tone_detail, int32_t total_samples, uint32_t preverse_samples = 0, double aglobal = 1.0, bool play_next = true);
	inline wtvsoftmodem_dsp::sample_t tx_sample()
	{
		if (mod_state.samples_left > 0)
		{
			mod_state.pcurrent[0] += mod_state.padvance[0];
			mod_state.pcurrent[1] += mod_state.padvance[1];

			double amplitude = mod_state.aglobal;
			if (mod_state.aadvance > 0)
			{
				mod_state.acurrent += mod_state.aadvance;

				amplitude *= mod_state.amin + (mod_state.aspread * (dsp.sin(mod_state.acurrent).raw));
			}
			
			wtvsoftmodem_dsp::sample_t cur_sample = (wtvsoftmodem_dsp::sample_t)((dsp.sin(mod_state.pcurrent[0]).sample + dsp.sin(mod_state.pcurrent[1]).sample) * amplitude);

			if (mod_state.preverse_samples > 0)
			{
				mod_state.preverse_samples_left--;

				if (mod_state.preverse_samples_left == 0)
				{
					mod_state.preverse_samples_left = mod_state.preverse_samples;
					mod_state.pcurrent[0] += mod_state.pradvance;
					mod_state.pcurrent[1] += mod_state.pradvance;
				}
			}

			if (!mod_state.infinite)
			{
				mod_state.samples_left--;

				if (mod_state.samples_left == 0)
				{
					if (mod_state.next_tone != TONE_STOP)
						tx_init(mod_state.next_tone, sample_rate, 0, mod_state.aglobal);
					else
						tone_completed();
				}
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

	inline bool is_new_tone(detected_tone_t detected_tone)
	{
		return (detected_tone.state == TONE_NEW);
	}
	inline char get_tone_digit(detected_tone_t detected_tone)
	{
		if (detected_tone.state == TONE_NEW && detected_tone.tone >= 0x20 && detected_tone.tone <= 0x5a)
			return detected_tone.tone;
		else
			return ' ';

	}
	void rx_init();
	detected_tone_t detect_dual_tone_freq(const int32_t* sample, uint32_t sample_count);
	detected_tone_t detect_single_tone_freq(const int32_t* sample, uint32_t sample_count);
	detected_tone_t detect_dtmf(const int32_t* sample, uint32_t sample_count);

private:

	typedef struct tone_modulate_state
	{
		double aglobal;
		wtvsoftmodem_dsp::phase_t pcurrent[2];
		wtvsoftmodem_dsp::phase_t padvance[2];
		wtvsoftmodem_dsp::phase_t acurrent;
		wtvsoftmodem_dsp::phase_t aadvance;
		double amin;
		double aspread;
		wtvsoftmodem_dsp::phase_t pradvance;
		uint32_t preverse_samples;
		uint32_t preverse_samples_left;
		uint32_t samples_left;
		bool infinite;
		tone_t next_tone;
	} tone_modulate_state_t;

	typedef struct tone_demodulate_state
	{
		int32_t tone_index;
		int32_t hit_count;
	} tone_demodulate_state_t;

	typedef struct dual_frequency
	{
		uint32_t index[2];
	} dual_frequency_t;

	static constexpr uint32_t SAMPLES_PER_HIT            = 384;

	static constexpr uint8_t  FREQUENCY_NOT_DETECTED     = -1;
	static constexpr uint8_t  FREQUENCY_DETECTING        = -2;
	static constexpr uint8_t  DTMF_TABLE_ROW_START_INDEX = 0;
	static constexpr uint8_t  DTMF_TABLE_COL_START_INDEX = 4;
	static constexpr uint8_t  DTMF_FREQUENCY_COUNT       = 4;

	static constexpr wtvsoftmodem_dsp::frequency_t DEFAULT_FREQ_TABLE[] = {
		// DTMF column frequencies
		697,
		770,
		852,
		941,
		// DTMF row frequencies
		1209,
		1336,
		1477,
		1633,
		// Dial tone frequencies
		350,
		440,
		450,
		425,
		400
	};

	static constexpr tone_detail_t DEFAULT_TONE_TABLE[] = {
		/*
		*
		* DTMF tones
		*
		*     1209  1336  1477  1633
		* 697    1     2     3     A
		* 770    4     5     6     B
		* 852    7     8     9     C
		* 941    *     0     #     D
		*
		*/
		{
			.tone = TONE_DTMF_1,
			.next = TONE_STOP,
			.f = { 697, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_2,
			.next = TONE_STOP,
			.f = { 697, 1336 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_3,
			.next = TONE_STOP,
			.f = { 697, 1477 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_A,
			.next = TONE_STOP,
			.f = { 697, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_4,
			.next = TONE_STOP,
			.f = { 770, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_5,
			.next = TONE_STOP,
			.f = { 770, 1336 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_6,
			.next = TONE_STOP,
			.f = { 770, 1477 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_B,
			.next = TONE_STOP,
			.f = { 770, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_7,
			.next = TONE_STOP,
			.f = { 852, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_8,
			.next = TONE_STOP,
			.f = { 852, 1336 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_9,
			.next = TONE_STOP,
			.f = { 852, 1477 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_C,
			.next = TONE_STOP,
			.f = { 852, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_STAR,
			.next = TONE_STOP,
			.f = { 941, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_0,
			.next = TONE_STOP,
			.f = { 941, 1336 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_POUND,
			.next = TONE_STOP,
			.f = { 941, 1477 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DTMF_D,
			.next = TONE_STOP,
			.f = { 941, 1209 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		// Dial tones
		{
			.tone = TONE_DIAL_NA,
			.next = TONE_STOP,
			.f = { 350, 440 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DIAL_UK,
			.next = TONE_STOP,
			.f = { 350, 450 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DIAL_EU,
			.next = TONE_STOP,
			.f = { 425, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DIAL_FR,
			.next = TONE_STOP,
			.f = { 440, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		},
		{
			.tone = TONE_DIAL_JP,
			.next = TONE_STOP,
			.f = { 400, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = TONE_INFINITE
		}
	};

	wtvsoftmodem_dsp dsp;

	tone_modulate_state_t mod_state;
	tone_demodulate_state_t demod_state;

	wtvsoftmodem_dsp::dft_state_t default_dft_state[(sizeof(DEFAULT_FREQ_TABLE) / sizeof(DEFAULT_FREQ_TABLE[0]))];

	uint8_t frequency_count;
	const wtvsoftmodem_dsp::frequency_t* frequency_table;
	wtvsoftmodem_dsp::dft_state_t* dft_state;

	uint8_t tone_count;
	const tone_detail_t* tone_table;

	wtvsoftmodem_dsp::frequency_t sample_rate;

	virtual void tone_completed() { }

	inline dual_frequency_t detect_dual_freq(uint32_t index_start, uint32_t length, const int32_t* sample, uint32_t sample_count)
	{
		double row_max_result = 0.0;
		dual_frequency_t detected_frequencies = {
			.index = { FREQUENCY_NOT_DETECTED, FREQUENCY_NOT_DETECTED }
		};
		for (uint8_t fidx = index_start; fidx < std::min(frequency_count, (uint8_t)(index_start + length)); fidx++)
		{
			if (dsp.dft(&dft_state[fidx], sample, sample_count))
			{
				double result = dsp.dft_result(&dft_state[fidx]);

				if (result > row_max_result)
				{
					row_max_result = result;
					detected_frequencies.index[1] = detected_frequencies.index[0];
					detected_frequencies.index[0] = fidx;
				}
			}
			else if(detected_frequencies.index[0] == FREQUENCY_NOT_DETECTED)
			{
				detected_frequencies.index[0] = FREQUENCY_DETECTING;
				detected_frequencies.index[1] = FREQUENCY_DETECTING;
			}
		}

		return detected_frequencies;
	}
	inline int32_t detect_single_freq(uint32_t index_start, uint32_t length, const int32_t* sample, uint32_t sample_count)
	{
		double row_max_result = 0.0;
		int32_t detected_frequency = FREQUENCY_NOT_DETECTED;
		for (uint8_t fidx = index_start; fidx < std::min(frequency_count, (uint8_t)(index_start + length)); fidx++)
		{
			if (dsp.dft(&dft_state[fidx], sample, sample_count))
			{
				double result = dsp.dft_result(&dft_state[fidx]);

				if (result > row_max_result)
				{
					row_max_result = result;
					detected_frequency = fidx;
				}
			}
			else if(detected_frequency == FREQUENCY_NOT_DETECTED)
			{
				detected_frequency = FREQUENCY_DETECTING;
			}
		}

		return detected_frequency;
	}
	detected_tone_t log_rx_hit(uint32_t tone_index);
	void rx_reset();

};

#endif // MAME_WEBTV_WTVSOFTMODEM_TONE_H