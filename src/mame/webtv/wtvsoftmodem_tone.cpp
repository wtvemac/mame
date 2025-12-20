// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_tone.h"
#include <cstdio>

wtvsoftmodem_tone::wtvsoftmodem_tone()
{
	uint8_t set_frequency_count = (sizeof(wtvsoftmodem_tone::DEFAULT_FREQ_TABLE) / sizeof(wtvsoftmodem_tone::DEFAULT_FREQ_TABLE[0]));
	wtvsoftmodem_tone::set_freq_table(wtvsoftmodem_tone::DEFAULT_FREQ_TABLE, set_frequency_count, default_dft_state);

	uint8_t set_tone_count = (sizeof(wtvsoftmodem_tone::DEFAULT_TONE_TABLE) / sizeof(wtvsoftmodem_tone::DEFAULT_TONE_TABLE[0]));
	wtvsoftmodem_tone::set_tone_table(wtvsoftmodem_tone::DEFAULT_TONE_TABLE, set_tone_count);

	wtvsoftmodem_tone::set_sample_rate(wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE);
}

void wtvsoftmodem_tone::set_freq_table(const wtvsoftmodem_dsp::frequency_t* set_frequency_table, uint8_t set_frequency_count, wtvsoftmodem_dsp::dft_state_t* set_dft_state)
{
	frequency_table = set_frequency_table;
	frequency_count = set_frequency_count;
	dft_state = set_dft_state;
}

void wtvsoftmodem_tone::set_tone_table(const tone_detail_t* set_tone_table, uint8_t set_tone_count)
{
	tone_table = set_tone_table;
	tone_count = set_tone_count;
}

void wtvsoftmodem_tone::set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate)
{
	sample_rate = set_sample_rate;

	wtvsoftmodem_tone::rx_init();
	wtvsoftmodem_tone::tx_init();
}

void wtvsoftmodem_tone::rx_reset()
{
	demod_state.tone_index = -1;
	demod_state.hit_count = 0;
}

wtvsoftmodem_tone::detected_tone_t wtvsoftmodem_tone::log_rx_hit(uint32_t tone_index)
{
	if (tone_index < tone_count)
	{
		if (demod_state.tone_index != tone_index)
			wtvsoftmodem_tone::rx_reset();

		demod_state.tone_index = tone_index;
		demod_state.hit_count++;

		const tone_detail_t* tone_detail = &tone_table[demod_state.tone_index];

		int32_t min_hits = std::rint((tone_detail->detect_duration_ms * sample_rate) / (1000 * wtvsoftmodem_tone::SAMPLES_PER_HIT));
		int32_t max_hits = std::rint((tone_detail->play_duration_ms * sample_rate) / (1000 * wtvsoftmodem_tone::SAMPLES_PER_HIT));

		if (demod_state.hit_count >= min_hits)
		{
			if(demod_state.hit_count == min_hits)
			{
				return {
					.state = wtvsoftmodem_tone::TONE_NEW,
					.tone = tone_detail->tone,
					.hit_count = demod_state.hit_count
				};
			}
			else if (max_hits > min_hits && demod_state.hit_count >= max_hits)
			{
				demod_state.tone_index = -1;
				demod_state.hit_count = 0;

				return {
					.state = wtvsoftmodem_tone::TONE_NONE,
					.tone = wtvsoftmodem_tone::TONE_NULL,
					.hit_count = 0
				};
			}
			else
			{
				return {
					.state = wtvsoftmodem_tone::TONE_CONTINUE,
					.tone = tone_detail->tone,
					.hit_count = demod_state.hit_count
				};
			}
		}
		else
		{
			return {
				.state = wtvsoftmodem_tone::TONE_INPROGRESS,
				.tone = tone_detail->tone,
				.hit_count = demod_state.hit_count
			};
		}
	}
	else
	{
		return {
			.state = wtvsoftmodem_tone::TONE_NONE,
			.tone = wtvsoftmodem_tone::TONE_NULL,
			.hit_count = 0
		};
	}
}

void wtvsoftmodem_tone::rx_init()
{
	for (uint32_t fidx = 0; fidx < frequency_count; fidx++)
	{
		dft_state[fidx] = dsp.dft_init(frequency_table[fidx], sample_rate, wtvsoftmodem_tone::SAMPLES_PER_HIT);
	}

	demod_state.tone_index = -1;
	demod_state.hit_count = 0;
}

wtvsoftmodem_tone::detected_tone_t wtvsoftmodem_tone::detect_dual_tone_freq(const int32_t* sample, uint32_t sample_count)
{
	dual_frequency_t detected_frequencies = wtvsoftmodem_tone::detect_dual_freq(0, frequency_count, sample, sample_count);

	if(detected_frequencies.index[0] == wtvsoftmodem_tone::FREQUENCY_NOT_DETECTED || detected_frequencies.index[1] == wtvsoftmodem_tone::FREQUENCY_NOT_DETECTED)
	{
		wtvsoftmodem_tone::rx_reset();

		return {
			.state = wtvsoftmodem_tone::TONE_NONE,
			.tone = wtvsoftmodem_tone::TONE_NULL,
			.hit_count = 0
		};
	}
	else if (detected_frequencies.index[0] == wtvsoftmodem_tone::FREQUENCY_DETECTING || detected_frequencies.index[1] == wtvsoftmodem_tone::FREQUENCY_DETECTING)
	{
		return {
			.state = wtvsoftmodem_tone::TONE_INPROGRESS,
			.tone = wtvsoftmodem_tone::TONE_NULL, // partial tone in progress
			.hit_count = 0
		};
	}
	else
	{
		int32_t found_tidx = -1;
		for (uint8_t tidx = 0; tidx < tone_count; tidx++)
		{
			if (tone_table[tidx].f[1] != 0)
			{
				if ((tone_table[tidx].f[0] == frequency_table[detected_frequencies.index[0]] && tone_table[tidx].f[1] == frequency_table[detected_frequencies.index[1]])
				||  (tone_table[tidx].f[0] == frequency_table[detected_frequencies.index[1]] && tone_table[tidx].f[1] == frequency_table[detected_frequencies.index[0]]))
				{
					found_tidx = tidx;
					break;
				}
			}
		}

		if (found_tidx != -1)
			return wtvsoftmodem_tone::log_rx_hit(found_tidx);
		else
			return {
				.state = wtvsoftmodem_tone::TONE_NONE,
				.tone = wtvsoftmodem_tone::TONE_NULL,
				.hit_count = 0
			};
	}
}

wtvsoftmodem_tone::detected_tone_t wtvsoftmodem_tone::detect_single_tone_freq(const int32_t* sample, uint32_t sample_count)
{
	int32_t detected_index = wtvsoftmodem_tone::detect_single_freq(0, frequency_count, sample, sample_count);

	if(detected_index == wtvsoftmodem_tone::FREQUENCY_NOT_DETECTED)
	{
		wtvsoftmodem_tone::rx_reset();

		return {
			.state = wtvsoftmodem_tone::TONE_NONE,
			.tone = wtvsoftmodem_tone::TONE_NULL,
			.hit_count = 0
		};
	}
	else if (detected_index == wtvsoftmodem_tone::FREQUENCY_DETECTING)
	{
		return {
			.state = wtvsoftmodem_tone::TONE_INPROGRESS,
			.tone = wtvsoftmodem_tone::TONE_NULL, // partial tone in progress
			.hit_count = 0
		};
	}
	else
	{
		int32_t found_tidx = -1;
		for (uint8_t tidx = 0; tidx < tone_count; tidx++)
		{
			if (tone_table[tidx].f[1] == 0 && tone_table[tidx].f[0] == frequency_table[detected_index])
			{
				found_tidx = tidx;
				break;
			}
		}

		if (found_tidx != -1)
			return wtvsoftmodem_tone::log_rx_hit(found_tidx);
		else
			return {
				.state = wtvsoftmodem_tone::TONE_NONE,
				.tone = wtvsoftmodem_tone::TONE_NULL,
				.hit_count = 0
			};
	}
}

wtvsoftmodem_tone::detected_tone_t wtvsoftmodem_tone::detect_dtmf(const int32_t* sample, uint32_t sample_count)
{
	int32_t detected_row_index = wtvsoftmodem_tone::detect_single_freq(wtvsoftmodem_tone::DTMF_TABLE_ROW_START_INDEX, wtvsoftmodem_tone::DTMF_FREQUENCY_COUNT, sample, sample_count);
	int32_t detected_col_index = wtvsoftmodem_tone::detect_single_freq(wtvsoftmodem_tone::DTMF_TABLE_COL_START_INDEX, wtvsoftmodem_tone::DTMF_FREQUENCY_COUNT, sample, sample_count);

	if(detected_col_index == wtvsoftmodem_tone::FREQUENCY_NOT_DETECTED || detected_row_index == wtvsoftmodem_tone::FREQUENCY_NOT_DETECTED)
	{
		wtvsoftmodem_tone::rx_reset();

		return {
			.state = wtvsoftmodem_tone::TONE_NONE,
			.tone = wtvsoftmodem_tone::TONE_NULL,
			.hit_count = 0
		};
	}
	else if (detected_col_index == wtvsoftmodem_tone::FREQUENCY_DETECTING || detected_row_index == wtvsoftmodem_tone::FREQUENCY_DETECTING)
	{
		return {
			.state = wtvsoftmodem_tone::TONE_INPROGRESS,
			.tone =wtvsoftmodem_tone::TONE_NULL, // partial tone in progress
			.hit_count = 0
		};
	}
	else
	{
		detected_row_index -= wtvsoftmodem_tone::DTMF_TABLE_ROW_START_INDEX;
		detected_col_index -= wtvsoftmodem_tone::DTMF_TABLE_COL_START_INDEX;

		return wtvsoftmodem_tone::log_rx_hit((detected_row_index * DTMF_FREQUENCY_COUNT) + detected_col_index);
	}
}

void wtvsoftmodem_tone::tx_init(wtvsoftmodem_tone::tone_t tone, int32_t total_samples_override, float amplitude, bool play_next)
{
	if (tone == wtvsoftmodem_tone::TONE_NULL)
	{
		int32_t total_samples;
		if (total_samples_override == 0)
			total_samples = wtvsoftmodem_tone::TONE_INFINITE;
		else
			total_samples = total_samples_override;

		tone_detail_t tone_detail = {
			.tone = 0x00,
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 0, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 0,
			.play_duration_ms = wtvsoftmodem_tone::TONE_INFINITE
		};

		wtvsoftmodem_tone::tx_init(&tone_detail, total_samples, 0, amplitude, play_next);
	}
	else
	{
		for (uint32_t didx = 0; didx < tone_count; didx++)
		{
			if (tone_table[didx].tone == tone)
			{
				int32_t total_samples;
				if (total_samples_override == 0)
				{
					if (tone_table[didx].play_duration_ms == wtvsoftmodem_tone::TONE_INFINITE)
						total_samples = wtvsoftmodem_tone::TONE_INFINITE;
					else
						total_samples = std::rint((tone_table[didx].play_duration_ms * sample_rate) / 1000);
				}
				else
				{
					total_samples = total_samples_override;
				}

				int32_t preverse_samples;
				if (tone_table[didx].preverse_ms > 0)
					preverse_samples = std::rint((tone_table[didx].preverse_ms * sample_rate) / 1000);
				else
					preverse_samples = 0;

				wtvsoftmodem_tone::tx_init(&tone_table[didx], total_samples, preverse_samples, amplitude, play_next);

				break;
			}
		}
	}
}

void wtvsoftmodem_tone::tx_init(const wtvsoftmodem_tone::tone_detail_t* tone_detail, int32_t total_samples, uint32_t preverse_samples, double aglobal, bool play_next)
{
	mod_state.pcurrent[0] = 0;
	mod_state.padvance[0] = dsp.get_padvance(tone_detail->f[0], sample_rate);
	mod_state.pcurrent[1] = 0;
	mod_state.padvance[1] = dsp.get_padvance(tone_detail->f[1], sample_rate);

	mod_state.aglobal = aglobal;

	mod_state.acurrent = 1.0;
	if (tone_detail->am.f > 0)
	{
		mod_state.aadvance = dsp.get_aadvance(tone_detail->am.f, sample_rate);
		mod_state.amin = tone_detail->am.amin;
		mod_state.aspread = (tone_detail->am.amax - tone_detail->am.amin);
	}
	else
	{
		mod_state.aadvance = 0;
		mod_state.amin = 0;
		mod_state.aspread = 0;
	}

	if (preverse_samples > 0)
	{
		mod_state.preverse_samples = preverse_samples;
		mod_state.preverse_samples_left = preverse_samples;
		mod_state.pradvance = dsp.get_pradvance();
	}
	else
	{
		mod_state.preverse_samples = 0;
		mod_state.preverse_samples_left = 0;
		mod_state.pradvance = 0;
	}

	if (total_samples == wtvsoftmodem_tone::TONE_INFINITE)
	{
		mod_state.infinite = true;
		mod_state.samples_left = 1;
		mod_state.next_tone = wtvsoftmodem_tone::TONE_STOP;
	}
	else
	{
		mod_state.infinite = false;
		mod_state.samples_left = total_samples;

		if (play_next && tone_detail->next != wtvsoftmodem_tone::TONE_STOP)
			mod_state.next_tone = tone_detail->next;
		else
			mod_state.next_tone = wtvsoftmodem_tone::TONE_STOP;
	}

}
