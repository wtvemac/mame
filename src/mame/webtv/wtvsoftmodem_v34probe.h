// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34PROBE_H
#define MAME_WEBTV_WTVSOFTMODEM_V34PROBE_H

#pragma once

#include "wtvsoftmodem_dsp.h"

#include <cstdint>

// ITU-T V.34 10.1.2.4 Line probing signals

class wtvsoftmodem_v34probe
{

public:

	enum signal_t
	{
		L1,
		L2
	};

	static constexpr double L1_MS = 192.0;
	static constexpr double L2_MS = 480.0;

	void start(signal_t signal, wtvsoftmodem_dsp::frequency_t sample_rate, bool predistort, double scale = 1.0);

	uint32_t tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	void rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count, wtvsoftmodem_dsp::frequency_t sample_rate);
	void rx_reset();

	void detect_reset();

	bool detect(bool rx_on, double rx_pwr, uint64_t now, wtvsoftmodem_dsp::frequency_t sample_rate, bool confirm_gap);

	bool peer_probe_ended() const
	{
		return m_peer_ended;
	}

	uint64_t peer_probe_end_sample() const
	{
		return m_peer_end_sample;
	}

private:

	static constexpr int32_t TONE_COUNT = 21;

	// ITU-T V.34 10.1.2.4 Table 17 Probing tones

	static constexpr double TONE_FREQUENCY[TONE_COUNT] = { 
		150, 300, 450, 600, 750, 1050, 1350, 1500, 1650, 1950, 2100, 2250, 2550, 2700, 2850, 3000, 3150, 3300, 3450, 3600, 3750
	};

	static constexpr double TONE_PHASE[TONE_COUNT] = { 
		0, M_PI, 0, 0, 0, 0, 0, 0, M_PI, 0, 0, M_PI, 0, M_PI, 0, M_PI, M_PI, M_PI, M_PI, 0, 0
	};

	static constexpr int32_t BOOSTED_TONE      = 14;
	static constexpr double  BOOSTED_TONE_GAIN = 12.0;

	static constexpr double PEER_TONE_HZ        = 1200.0;
	static constexpr double PEER_MAX_SILENCE_MS = 700.0;
	static constexpr double PEER_GAP_CONFIRM_MS = 250.0;

	static constexpr double NARROWBAND_FRAC_LIMIT = 0.20;

	wtvsoftmodem_dsp::oscillator m_tone[TONE_COUNT];
	double m_tone_gain[TONE_COUNT] = {};

	double m_amp = 1000.0;

	double m_narrow_i = 0.0;
	double m_narrow_q = 0.0;
	wtvsoftmodem_dsp::oscillator m_narrow_osc;

	bool m_peer_seen = false;
	bool m_peer_ended = false;
	uint64_t m_peer_start_sample = 0;
	uint64_t m_peer_end_sample = 0;
	uint64_t m_peer_gap_sample = 0;
};

#endif
