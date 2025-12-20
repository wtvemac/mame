// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_dsp.h"
#include <cstdio>
#include <algorithm>

wtvsoftmodem_dsp::wtvsoftmodem_dsp(bool populate_sin_table)
{
	if (populate_sin_table)
		wtvsoftmodem_dsp::populate_sin_table(sin_table, wtvsoftmodem_dsp::TRIG_TABLE_SIZE);
}

void wtvsoftmodem_dsp::populate_sin_table(wtvsoftmodem_dsp::trig_entry_t* table, uint32_t size)
{
	size = std::min(size, wtvsoftmodem_dsp::TRIG_TABLE_SIZE);

	for(uint32_t i = 0; i < size; i++)
	{
		double phase_angle = (double)i / (double)size;

		double raw = std::sin(phase_angle *wtvsoftmodem_dsp:: M_TAU);
		double sample_dbl = (raw * (double)(wtvsoftmodem_dsp::TRIG_RANGE_LIMIT - 1));

		table[i] = {
			.raw = raw,
			.sample = (sample_t)sample_dbl,
			.sample_dbl = sample_dbl
		};
	}
}

void wtvsoftmodem_dsp::populate_cos_table(wtvsoftmodem_dsp::trig_entry_t* table, uint32_t size)
{
	size = std::min(size, wtvsoftmodem_dsp::TRIG_TABLE_SIZE);

	for(uint32_t i = 0; i < size; i++)
	{
		double phase_angle = (double)i / (double)size;

		double raw = std::cos(phase_angle * wtvsoftmodem_dsp::M_TAU);
		double sample_dbl = (raw * (double)(wtvsoftmodem_dsp::TRIG_RANGE_LIMIT - 1));

		table[i] = {
			.raw = raw,
			.sample = (sample_t)sample_dbl,
			.sample_dbl = sample_dbl
		};
	}
}

wtvsoftmodem_dsp::phase_t wtvsoftmodem_dsp::get_padvance(wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	return (phase_t)((wtvsoftmodem_dsp::TRIG_DOMAIN_LIMIT - 1) * (frequency / sample_rate));
}

wtvsoftmodem_dsp::phase_t wtvsoftmodem_dsp::get_pradvance()
{
	return (phase_t)((wtvsoftmodem_dsp::TRIG_DOMAIN_LIMIT - 1) / 2);
}

wtvsoftmodem_dsp::phase_t wtvsoftmodem_dsp::get_aadvance(wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	return (phase_t)((wtvsoftmodem_dsp::TRIG_DOMAIN_LIMIT - 1) * (frequency / sample_rate));
}

// Discrete Fourier transform using the Goertzel algorithm 
// Info: https://eng.libretexts.org/Bookshelves/Electrical_Engineering/Signal_Processing_and_Modeling/Fast_Fourier_Transforms_(Burrus)/04%3A_The_DFT_as_Convolution_or_Filtering/4.04%3A_Goertzel's_Algorithm_or_A_Better_DFT_Algorithm

wtvsoftmodem_dsp::dft_state_t wtvsoftmodem_dsp::dft_init(wtvsoftmodem_dsp::frequency_t sniff_frequency, wtvsoftmodem_dsp::frequency_t sample_rate, uint32_t samples_per_block)
{
	double k = std::rint(sniff_frequency * ((double)samples_per_block / sample_rate));

	return {
		.samples_processed = 0,
		.samples_per_block = samples_per_block,
		.coeff             = 2.0 * std::cos(k * (M_TAU / (double)samples_per_block)),
		.q                 = { 0, 0 }
	};
}

void wtvsoftmodem_dsp::populate_i_table(wtvsoftmodem_dsp::sample_t* itable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	for (uint32_t fdix = 0; fdix < size; fdix++)
	{
		double phase_angle = frequency * ((double)fdix / sample_rate);

		double iraw = std::cos(phase_angle * wtvsoftmodem_dsp::M_TAU);
		itable[fdix] = (wtvsoftmodem_dsp::sample_t)(iraw * (double)(wtvsoftmodem_dsp::TRIG_RANGE_LIMIT - 1));
	}
}

void wtvsoftmodem_dsp::populate_q_table(wtvsoftmodem_dsp::sample_t* qtable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	for (uint32_t fdix = 0; fdix < size; fdix++)
	{
		double phase_angle = frequency * ((double)fdix / sample_rate);

		double qraw = std::sin(phase_angle * wtvsoftmodem_dsp::M_TAU);
		qtable[fdix] = (wtvsoftmodem_dsp::sample_t)(qraw * (double)(wtvsoftmodem_dsp::TRIG_RANGE_LIMIT - 1));
	}
}

void wtvsoftmodem_dsp::populate_iq_table(wtvsoftmodem_dsp::iqtable_t* table, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	size = std::min(wtvsoftmodem_dsp::IQ_TABLE_SIZE, size);

	wtvsoftmodem_dsp::populate_i_table(table->itable, size, frequency, sample_rate);
	wtvsoftmodem_dsp::populate_q_table(table->qtable, size, frequency, sample_rate);

	table->table_size = size;
}
