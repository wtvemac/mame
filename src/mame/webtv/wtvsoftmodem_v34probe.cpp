// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34probe.h"

#include <algorithm>
#include <cmath>

// ITU-T V.34 10.1.2.4 Line probing signals

void wtvsoftmodem_v34probe::start(signal_t signal, wtvsoftmodem_dsp::frequency_t sample_rate, bool predistort, double scale)
{
	for (int32_t t = 0; t < TONE_COUNT; t++)
	{
		m_tone_gain[t] = (predistort && t == BOOSTED_TONE) ? BOOSTED_TONE_GAIN : 1.0;
		m_tone[t].set_step(wtvsoftmodem_dsp::radians_per_sample(TONE_FREQUENCY[t], sample_rate));

		if (signal == L1)
			m_tone[t].set_phase(TONE_PHASE[t]);
	}

	double peak = 0.0;
	int32_t period = wtvsoftmodem_dsp::round_nearest<int32_t>(sample_rate / 50.0);
	for (int32_t k = 0; k < period; k++)
	{
		double v = 0.0;
		for (int32_t t = 0; t < TONE_COUNT; t++)
			v += m_tone_gain[t] * std::cos(TONE_PHASE[t] + 2.0 * M_PI * TONE_FREQUENCY[t] * k / sample_rate);

		peak = std::max(peak, std::fabs(v));
	}

	m_amp = (0.85 * 30000.0 / (peak > 1e-9 ? peak : 1.0));
	if (signal == L2)
		m_amp *= 0.5;

	m_amp *= scale;
}

uint32_t wtvsoftmodem_v34probe::tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double v = 0.0;
		for (int32_t t = 0; t < TONE_COUNT; t++)
		{
			v += m_tone_gain[t] * m_tone[t].re;
			m_tone[t].advance();
		}

		sample[sample_idx] = wtvsoftmodem_dsp::saturate_round(v * m_amp);
	}

	return sample_count;
}

void wtvsoftmodem_v34probe::rx_reset()
{
	m_narrow_i = 0.0;
	m_narrow_q = 0.0;
	m_narrow_osc.set_phase(0.0);

	detect_reset();
}

void wtvsoftmodem_v34probe::rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count, wtvsoftmodem_dsp::frequency_t sample_rate)
{
	m_narrow_osc.set_step(wtvsoftmodem_dsp::radians_per_sample(PEER_TONE_HZ, sample_rate > 0 ? sample_rate : 16000.0));

	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double v = double(sample[sample_idx]) / 32768.0;

		m_narrow_i = 0.999 * m_narrow_i + 0.001 * (v * m_narrow_osc.re);
		m_narrow_q = 0.999 * m_narrow_q + 0.001 * (v * m_narrow_osc.im);
		m_narrow_osc.advance();
	}
}

void wtvsoftmodem_v34probe::detect_reset()
{
	m_peer_seen = false;
	m_peer_ended = false;
	m_peer_start_sample = 0;
	m_peer_end_sample = 0;
	m_peer_gap_sample = 0;
}

bool wtvsoftmodem_v34probe::detect(bool rx_on, double rx_pwr, uint64_t now, wtvsoftmodem_dsp::frequency_t sample_rate, bool confirm_gap)
{
	const double narrow_pwr = m_narrow_i * m_narrow_i + m_narrow_q * m_narrow_q;
	const double narrow_frac = narrow_pwr / std::max(rx_pwr, 1e-9);
	const bool probing = rx_on && (narrow_frac < NARROWBAND_FRAC_LIMIT);
	
	if (probing && !m_peer_seen)
		m_peer_start_sample = now;

	const bool capped = m_peer_seen && !m_peer_ended && (now - m_peer_start_sample) >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(PEER_MAX_SILENCE_MS, sample_rate);
	const bool silent = probing && !capped && !m_peer_ended;

	bool ended = false;

	if (probing)
	{
		m_peer_gap_sample = 0;
		ended = !confirm_gap && capped;
		m_peer_seen = true;
	}
	else if (m_peer_seen && !m_peer_ended)
	{
		if (m_peer_gap_sample == 0)
			m_peer_gap_sample = now;

		ended = !confirm_gap || capped || (now - m_peer_gap_sample) >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(PEER_GAP_CONFIRM_MS, sample_rate);
	}
	
	if (ended)
	{
		m_peer_ended = true;
		m_peer_end_sample = now;
	}

	return silent;
}
