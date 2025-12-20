// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_DSP_H
#define MAME_WEBTV_WTVSOFTMODEM_DSP_H

#pragma once

#include <cstdint>
#include <cmath>

class wtvsoftmodem_dsp
{

public:

	// Resolution of the I and Q table
	static constexpr uint32_t    IQ_TABLE_BITS     = 7;
	static constexpr uint32_t    IQ_TABLE_SIZE     = (1 << IQ_TABLE_BITS);

	typedef int16_t sample_t;

	typedef uint16_t phase_t;

	typedef double frequency_t;

	typedef struct trig_entry
	{
		double raw;
		sample_t sample;
		double sample_dbl;
	} trig_entry_t;

	typedef struct iqtable
	{
		uint32_t table_size;
		sample_t itable[IQ_TABLE_SIZE];
		sample_t qtable[IQ_TABLE_SIZE];
	} iqtable_t;

	typedef struct dft_state
	{
		uint32_t samples_processed;
		uint32_t samples_per_block;
		double coeff;
		double q[2];
	} dft_state_t;

	static constexpr frequency_t DEFAULT_SAMPLE_RATE   = 16000;

	wtvsoftmodem_dsp(bool populate_sin_table = true);

	void populate_sin_table(trig_entry_t* table, uint32_t size);
	void populate_cos_table(trig_entry_t* table, uint32_t size);

	inline trig_entry_t sin(phase_t phase_angle)
	{
		phase_angle >>= (TRIG_DOMAIN_BITS - TRIG_TABLE_BITS);

		return sin_table[phase_angle & (TRIG_TABLE_SIZE - 1)];
	}

	// Value to advance of the domain (phase/time) per sample to play a frequency at the given sample rate.
	// This value is passed to sin() to produce a sample value.
	phase_t get_padvance(frequency_t frequency, frequency_t sample_rate);

	// Value to advance the domain (phase/time) such that the value of sin() is inverted compared to the previous sample.
	// For example: if the previous sample value is 10 then this will advance the played frequency so it produces -10 for the next sample.
	// this is used to do a phase reversal for the ANSam tone.
	phase_t get_pradvance();

	// Value of the domain (phase/time) to modulate the amplitude at a given frequency and at the given sample rate. Used for AM needed to produce some tones.
	// This value is passed to sin() to to produce an amplitude value that's used to adjust the sample value.
	phase_t get_aadvance(frequency_t frequency, frequency_t sample_rate);

	dft_state_t dft_init(frequency_t sniff_frequency, frequency_t sample_rate, uint32_t samples_per_block);
	bool dft(dft_state_t* state, const int32_t* sample, uint32_t sample_count)
	{
		for (uint32_t sidx = 0; sidx < sample_count; sidx++)
		{
			double q    = state->coeff * state->q[0] - state->q[1] + (double)sample[sidx];
			state->q[1] = state->q[0];
			state->q[0] = q;
		}

		state->samples_processed += sample_count;

		return (state->samples_processed >= state->samples_per_block);
	}
	inline double dft_result(dft_state_t* state)
	{
		double power = (state->q[0] * state->q[0]) + (state->q[1] * state->q[1]) - (state->coeff * state->q[0] * state->q[1]);

		if (state->samples_processed >= state->samples_per_block)
		{
			state->samples_processed = 0;
			state->q[0] = 0;
			state->q[1] = 0;
		}

		return power;
	}

	void populate_i_table(wtvsoftmodem_dsp::sample_t* itable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate);
	void populate_q_table(wtvsoftmodem_dsp::sample_t* qtable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate);
	void populate_iq_table(iqtable_t* table, uint32_t size, frequency_t frequency, frequency_t sample_rate);
	inline int32_t circular_dot_product(sample_t* table0, sample_t* circular_table1, uint32_t table_size, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{
		int32_t dot_product = 0;

		for (uint32_t fdix = 0, sidx = sidx_start; fdix < table_size; fdix++, sidx++)
		{
			dot_product += (table0[fdix] * circular_table1[sidx & sidx_mask]);
		}

		dot_product >>= TRIG_RANGE_BITS;

		return dot_product;
	}
	inline int32_t get_iq_similarity(wtvsoftmodem_dsp::sample_t* itable, wtvsoftmodem_dsp::sample_t* qtable, uint32_t table_size, sample_t* samples, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{
		int32_t idot_product = circular_dot_product(itable, samples, table_size, sidx_start, sidx_mask);
		int32_t qdot_product = circular_dot_product(qtable, samples, table_size, sidx_start, sidx_mask);

		int32_t power = (idot_product * idot_product) + (qdot_product * qdot_product);

		return power;
	}
	inline int32_t get_iq_similarity(iqtable_t* iqtable, sample_t* samples, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{

		return get_iq_similarity(iqtable->itable, iqtable->qtable, iqtable->table_size, samples, sidx_start, sidx_mask);
	}

private:

	static constexpr double      M_TAU                 = 2 * M_PI;

	// This defines the resolution of the input data into sin() or cos()
	// Must be at or higher than TRIG_TABLE_BITS and at or lower than phase_t's bit count.
	// Will be reduced to TRIG_TABLE_BITS to do a trig table lookup
	static constexpr uint32_t    TRIG_DOMAIN_BITS  = 16;
	static constexpr double      TRIG_DOMAIN_LIMIT = (1 << TRIG_DOMAIN_BITS);

	// This defines the range of the output data from sin() or cos()
	// TRIG_RANGE_LIMIT must me less than max(trig_entry_t)/2.
	// The more bits used here will increase the amplitude range (and thus be louder)
	static constexpr uint32_t    TRIG_RANGE_BITS   = 13;
	static constexpr double      TRIG_RANGE_LIMIT  = (1 << TRIG_RANGE_BITS);

	// This defines the resolution of the main table
	// The higher this is the more memory the table will take up.
	static constexpr uint32_t    TRIG_TABLE_BITS   = 14;
	static constexpr uint32_t    TRIG_TABLE_SIZE   = (1 << TRIG_TABLE_BITS);

	trig_entry_t sin_table[TRIG_TABLE_SIZE];

};

#endif // MAME_WEBTV_WTVSOFTMODEM_DSP_H