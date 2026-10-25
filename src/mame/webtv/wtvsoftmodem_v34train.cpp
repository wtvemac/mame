// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34train.h"
#include "wtvsoftmodem_dsp.h"
#include "wtvsoftmodem_v34constellation.h"

#include <algorithm>
#include <array>
#include <cmath>

// ITU-T V.34 10.1.3 Phases 3 and 4 signals (S, PP, TRN, J, MP, E)

wtvsoftmodem_v34train::wtvsoftmodem_v34train()
{
	init(config_t {});
}

wtvsoftmodem_v34train::wtvsoftmodem_v34train(const config_t &cfg)
{
	init(cfg);
}

void wtvsoftmodem_v34train::init(const config_t &cfg)
{
	m_cfg = cfg;

	m_trn_scram.set_tx_channel(cfg.answer_mode ? wtvsoftmodem_scrambler::ANSWER_CHANNEL : wtvsoftmodem_scrambler::CALL_CHANNEL);
	m_trn_descram.set_rx_channel(cfg.answer_mode ? wtvsoftmodem_scrambler::ANSWER_CHANNEL : wtvsoftmodem_scrambler::CALL_CHANNEL);

	wtvsoftmodem_v34train::reset();
}

void wtvsoftmodem_v34train::reset()
{
	m_tx_training16 = false;
	m_trn_scram.reset();
	m_trn_descram.reset();

	m_s_n    = 0;
	m_pp_n   = 0;
	m_j_bit  = 0;
	m_j_diff = 0;
	m_alt_acc       = { 0.0, 0.0 };
	m_pow_acc       = 1e-9;
	m_det_n         = 0;
	m_last_polarity = 0;
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_s(bool native_pattern)
{
	double s = (m_s_n++ & 1) ? -m_cfg.s_amplitude : m_cfg.s_amplitude;
	return { s, native_pattern ? m_cfg.s_amplitude : s };
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_sbar(bool native_pattern)
{
	double s = (m_s_n++ & 1) ? m_cfg.s_amplitude : -m_cfg.s_amplitude;
	return { s, native_pattern ? -m_cfg.s_amplitude : s };
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_pp()
{
	static const std::array<point_t, 48> tab =
	[] {
		std::array<point_t, 48> t {};

		for (int32_t i = 0; i < 48; i++)
		{
			int32_t k     = i / 4;
			int32_t ii    = i % 4;
			double  kx    = (k % 3 == 1) ? 4.0 : 0.0;
			double  theta = M_PI * (double(k * ii) + kx) / 6.0;

			t[i] = { std::cos(theta), std::sin(theta) };
		}

		return t;
	}();

	const double amp = m_cfg.s_amplitude * M_SQRT2;
	int32_t idx = int32_t(m_pp_n++ % 48);

	return { tab[idx].i * amp, tab[idx].q * amp };
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::trn_point(uint32_t bits) const
{
	// V.34 10.1.3.8 4-point
	static const int8_t mi[4] = { +1, +1, -1, -1 };
	static const int8_t mq[4] = { +1, -1, -1, +1 };

	uint32_t s = bits & 3;

	return { mi[s] * m_cfg.trn_scale, mq[s] * m_cfg.trn_scale };
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_trn()
{
	return gen_trn_pattern(15);
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_trn(bool d0, bool d1)
{
	return gen_trn_pattern(uint32_t(d0) | (uint32_t(d1) << 1));
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_trn_pattern(uint32_t input)
{
	uint32_t bits = 0;
	for (uint32_t b = 0; b < tx_training_bits(); ++b)
		bits |= uint32_t(m_trn_scram.scramble((input >> b) & 1)) << b;

	return m_tx_training16 ? trn_point16(bits) : trn_point(bits);
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_j()
{
	uint32_t v = 0;
	for (int32_t b = 0; b < 2; b++)
	{
		bool pat = (m_j_pattern >> m_j_bit) & 1;

		m_j_bit  = (m_j_bit + 1) & 15;

		v |= (m_trn_scram.scramble(pat) ? 1U : 0U) << b;
	}

	m_j_diff = uint8_t((m_j_diff + v) & 3);

	return trn_point(m_j_diff);
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_e()
{
	uint32_t v = 0;

	for (uint32_t b = 0; b < tx_training_bits(); b++)
	{
		bool pat = true;

		m_e_bit  = (m_e_bit + 1) % 20;

		v |= (m_trn_scram.scramble(pat) ? 1U : 0U) << b;
	}

	m_j_diff = uint8_t((m_j_diff + v) & 3);

	return m_tx_training16 ? trn_point16((v & 12) | m_j_diff) : trn_point(m_j_diff);
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::trn_point16(uint32_t bits) const
{
	const auto &p = wtvsoftmodem::v34::constellation::TRN_MAP16[bits & 15];

	const double scale = m_cfg.trn_scale / std::sqrt(5.0);

	return { p[0] * scale, p[1] * scale };
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_diff16(const uint8_t *bits)
{
	uint32_t v = 0;
	for (uint32_t b = 0; b < 4; ++b)
		v |= uint32_t(m_trn_scram.scramble(bits[b] != 0)) << b;

	m_j_diff = uint8_t((m_j_diff + v) & 3);

	return trn_point16((v & 12) | m_j_diff);
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::gen_diff4(bool d0, bool d1)
{
	uint32_t v = (m_trn_scram.scramble(d0) ? 1U : 0U) | (m_trn_scram.scramble(d1) ? 2U : 0U);

	m_j_diff   = uint8_t((m_j_diff + v) & 3);

	return trn_point(m_j_diff);
}

void wtvsoftmodem_v34train::mp_rx_reset()
{
	m_mp_descram.set_rx_channel(m_cfg.answer_mode ? wtvsoftmodem_scrambler::CALL_CHANNEL : wtvsoftmodem_scrambler::ANSWER_CHANNEL);
	m_mp_descram.reset();

	m_mp_descram16.set_rx_channel(m_cfg.answer_mode ? wtvsoftmodem_scrambler::CALL_CHANNEL : wtvsoftmodem_scrambler::ANSWER_CHANNEL);
	m_mp_descram16.reset();

	m_mp_rx_diff = 0;
	m_mp_rx_diff16 = 0;
}

int32_t wtvsoftmodem_v34train::detect_diff4(point_t sym, uint8_t *d)
{
	bool si = sym.i > 0.0;
	bool sq = sym.q > 0.0;

	uint8_t  q = si ? (sq ? 0 : 1) : (sq ? 3 : 2);
	uint32_t v = uint32_t((q - m_mp_rx_diff) & 3);

	m_mp_rx_diff = q;

	d[0] = m_mp_descram.descramble((v  & 1)    ) ? 1 : 0;
	d[1] = m_mp_descram.descramble((v >> 1) & 1) ? 1 : 0;

	return 2;
}

wtvsoftmodem_v34train::detect_state_t wtvsoftmodem_v34train::detect(point_t sym)
{
	const double decay = 0.94;
	double       sign  = (m_det_n++ & 1) ? -1.0 : 1.0;

	std::complex<double> z(sym.i, sym.q);

	m_alt_acc = decay * m_alt_acc + sign * z;
	m_pow_acc = decay * m_pow_acc + std::norm(z);

	detect_state_t detect_state;

	detect_state.confidence = std::norm(m_alt_acc) * (1.0 - decay) / (m_pow_acc + 1e-12);
	detect_state.s_present  = detect_state.confidence > 0.55;
	if (detect_state.s_present)
	{
		double re = m_alt_acc.real();
		double im = m_alt_acc.imag();
		double dom  = (std::fabs(re) >= std::fabs(im)) ? re : im;

		detect_state.polarity = (dom >= 0.0) ? 1 : -1;

		if (m_last_polarity != 0 && detect_state.polarity != m_last_polarity)
			detect_state.reversal = true;

		m_last_polarity = detect_state.polarity;
	}

	return detect_state;
}

static const std::array<wtvsoftmodem_v34train::point_t, 16> &training16_points()
{
	using wtvsoftmodem::v34::constellation::TRN_MAP16;

	static const std::array<wtvsoftmodem_v34train::point_t, 16> scaled =
	[] {
		std::array<wtvsoftmodem_v34train::point_t, 16> t {};

		for (uint32_t i = 0; i < 16; i++)
			t[i] = { TRN_MAP16[i][0] / std::sqrt(10.0), TRN_MAP16[i][1] / std::sqrt(10.0) };

		return t;
	}();

	return scaled;
}

static uint32_t nearest_training16(wtvsoftmodem_v34train::point_t sym)
{
	const auto &points = training16_points();
	uint32_t    q      = 0;
	double      best   = 1e100;

	for (uint32_t i = 0; i < 16; i++)
	{
		const double x = sym.i - points[i].i;
		const double y = sym.q - points[i].q;
		const double error = x * x + y * y;

		if (error < best)
		{
			best = error;
			q    = i;
		}
	}

	return q;
}

int32_t wtvsoftmodem_v34train::detect_diff16(point_t sym, uint8_t *d)
{
	const uint32_t q = nearest_training16(sym);
	const uint32_t v = (q & 12) | ((q - m_mp_rx_diff16) & 3);

	m_mp_rx_diff16   = q;

	for (uint32_t i = 0; i < 4; i++)
		d[i] = m_mp_descram16.descramble((v >> i) & 1) ? 1 : 0;

	return 4;
}

wtvsoftmodem_v34train::point_t wtvsoftmodem_v34train::snap16(point_t sym)
{
	point_t result {};
	double  best = 1e100;

	for (const auto &candidate : training16_points())
	{
		const double x = sym.i - candidate.i;
		const double y = sym.q - candidate.q;

		if (x * x + y * y < best)
		{
			best   = x * x + y * y;
			result = candidate;
		}
	}

	return result;
}

void wtvsoftmodem_v34train::detect_trn_reset()
{
	m_trn_descram.reset();
}

int32_t wtvsoftmodem_v34train::detect_trn(point_t sym, uint8_t *bits_out)
{
	bool si = sym.i > 0.0;
	bool sq = sym.q > 0.0;

	uint32_t bits = si ? (sq ? 0U : 1U) : (sq ? 3U : 2U);

	for (int32_t b = 0; b < 2; b++)
		bits_out[b] = uint8_t((bits >> b) & 1);

	return 2;
}

wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent wtvsoftmodem_v34train::retrain_s_detector::put(double i, double q)
{
	const double sign = (m_count++ & 1) ? -1.0 : 1.0;
	const std::complex<double> raw(i, q);
	const auto point = raw * sign;

	if (m_confirmed && !m_reversed)
	{
		const auto expected = m_mean + sign * m_phasor;
		const double dot = std::real(raw * std::conj(expected));

		if (dot < -0.7 * std::abs(raw) * std::abs(expected))
			m_reverse_run++;
		else
			m_reverse_run = 0;

		if (m_reverse_run == 4)
		{
			m_reversed = true;
			return RetrainSEvent::SBAR;
		}
	}

	m_sum += point;
	m_dc += raw;
	m_power += std::norm(point);

	if (m_count % 16)
		return RetrainSEvent::NONE;

	m_confidence = m_power > 1e-6 ? (std::norm(m_sum) + std::norm(m_dc)) / (16 * m_power) : 0;

	const bool periodic = m_power > 1e-6 && std::norm(m_sum) > 0.20 * 16 * m_power && std::norm(m_dc) > 0.20 * 16 * m_power && std::norm(m_sum) + std::norm(m_dc) > 0.85 * 16 * m_power;
	if (periodic)
	{
		m_run++;
		m_candidate_sum += m_sum;
		m_candidate_dc += m_dc;
		m_candidate_power += m_power;

		if (std::norm(m_candidate_sum) + std::norm(m_candidate_dc) < 0.85 * m_run * 16 * m_candidate_power)
		{
			m_run = 1;
			m_candidate_sum = m_sum;
			m_candidate_dc = m_dc;
			m_candidate_power = m_power;
		}
	}
	else
	{
		m_run = 0;
		m_candidate_sum = {};
		m_candidate_dc = {};
		m_candidate_power = 0;
	}

	if (!m_confirmed && m_run >= 4)
	{
		m_confirmed = true;
		m_phasor = m_sum / 16.0;
		m_mean = m_dc / 16.0;
		m_sum = {};
		m_dc = {};
		m_power = 0;

		return RetrainSEvent::S;
	}
	else
	{
		m_sum = {};
		m_dc = {};
		m_power = 0;

		return RetrainSEvent::NONE;
	}
}

void wtvsoftmodem_v34train::retrain_tone_detector::reset(double rate, double frequency)
{
	m_bin = wtvsoftmodem_dsp::goertzel_init(frequency, rate, uint32_t(rate / 200), false);
	m_filter = wtvsoftmodem_dsp::bandpass_init(frequency, rate, 8);

	m_power = 0;
	m_run = 0;
	m_raw_power = 0;
	m_raw_bin = m_bin;

	m_periodic_sum = {};
	m_periodic_power = 0;
	m_raw_periodic_power = 0;
	m_raw_periodic_sum = {};
}

bool wtvsoftmodem_v34train::retrain_tone_detector::put(double sample)
{
	const double raw = sample;

	sample = wtvsoftmodem_dsp::bandpass_put(&m_filter, sample);

	wtvsoftmodem_dsp::goertzel_put(&m_bin, sample);

	m_power += sample * sample;

	wtvsoftmodem_dsp::goertzel_put(&m_raw_bin, raw);

	m_raw_power += raw * raw;

	if (!wtvsoftmodem_dsp::goertzel_ready(&m_bin))
		return false;

	const uint32_t count = m_bin.samples_processed;

	const std::complex<double> bin = wtvsoftmodem_dsp::goertzel_value(&m_bin);

	const bool tone = m_power > count * 100.0 && 2 * std::norm(bin) > TONE_PURITY * count * m_power;
	if (tone)
	{
		m_run++;
		m_periodic_sum += bin;
		m_periodic_power += m_power;
		m_raw_periodic_sum += wtvsoftmodem_dsp::goertzel_value(&m_raw_bin);
		m_raw_periodic_power += m_raw_power;
	}
	else
	{
		m_run = 0;
		m_periodic_sum = {};
		m_periodic_power = 0;
		m_raw_periodic_sum = {};
		m_raw_periodic_power = 0;
	}

	// The tone has to stay phase-coherent (this is an after filter check)
	const bool filtered_coherent = 2 * std::norm(m_periodic_sum) > TONE_PURITY * m_run * count * m_periodic_power;
	// The filtered signal can give false-positives, so also check for the tone on unfiltered data (it will be weaker but it should be there)
	const bool raw_coherent = 2 * std::norm(m_raw_periodic_sum) > RAW_PURITY * m_run * count * m_raw_periodic_power;

	const bool confirmed = m_run >= CONFIRM_BLOCKS && filtered_coherent && raw_coherent;

	wtvsoftmodem_dsp::goertzel_reset(&m_bin);
	wtvsoftmodem_dsp::goertzel_reset(&m_raw_bin);

	m_power = 0;
	m_raw_power = 0;

	return confirmed;
}

uint32_t wtvsoftmodem_v34train::retrain_sustainable_rate(double snr_db, uint32_t baud, bool expanded_shaping, bool nonlinear, bool precoding, bool aux)
{
	if (baud > 5 || !std::isfinite(snr_db))
		return 0;

	const int32_t snr = wtvsoftmodem_dsp::round_nearest<int32_t>(snr_db * 4);

	uint32_t rate = baud ? 1 : 0;

	const uint32_t group = uint32_t(aux) * 4 + uint32_t(expanded_shaping) * 2 + uint32_t(precoding);

	while (rate < 14)
	{
		const uint32_t minimum = RETRAIN_RATE_THRESHOLD[baud][rate][group][nonlinear ? 1 : 0];
		if (!minimum || snr < int32_t(minimum))
			break;
		rate++;
	}

	return baud == 0 && rate == 1 ? 0 : rate;
}
