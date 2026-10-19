// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_qam.h"

#include "wtvsoftmodem_dsp.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iterator>

wtvsoftmodem_qam::wtvsoftmodem_qam()
{
	set_qam_spec_table(DEFAULT_QAM_SPEC_TABLE, uint8_t(std::size(DEFAULT_QAM_SPEC_TABLE)));

	init(config_t {});
}

wtvsoftmodem_qam::wtvsoftmodem_qam(const config_t &cfg)
{
	set_qam_spec_table(DEFAULT_QAM_SPEC_TABLE, uint8_t(std::size(DEFAULT_QAM_SPEC_TABLE)));

	init(cfg);
}

void wtvsoftmodem_qam::build_rrc_tables()
{
	const int32_t res = RRC_TABLE_RES;
	m_rrc_half = m_cfg.filter_span_syms;
	m_rrc_step_inv = double(res);

	const int32_t n = 2 * m_rrc_half * res + 1;
	m_rrc_count = n;
	m_rrc_rows = 2 * m_rrc_half + 1;
	m_rrc_table.assign(size_t(res) * m_rrc_rows, 0.0);
	for (int32_t k = 0; k < n; k++)
	{
		const double t = double(k) / res - m_rrc_half;
		m_rrc_table[(k % res) * m_rrc_rows + k / res] = m_cfg.tx_pulse_fn ? m_cfg.tx_pulse_fn(t) : wtvsoftmodem_dsp::rrc_pulse(t, m_cfg.rolloff);
	}
}

void wtvsoftmodem_qam::set_qam_spec_table(const qam_detail_t *set_qam_spec_table, uint8_t set_qam_spec_count)
{
	m_spec_table = set_qam_spec_table;
	m_spec_count = set_qam_spec_count;
}

bool wtvsoftmodem_qam::init(qam_spec_t qam_spec, config_t cfg)
{
	for (uint8_t qidx = 0; qidx < m_spec_count; qidx++)
	{
		if (m_spec_table[qidx].spec == qam_spec)
		{
			cfg.symbol_rate = m_spec_table[qidx].symbol_rate;
			cfg.carrier_freq = m_spec_table[qidx].carrier_freq;
			cfg.rolloff = m_spec_table[qidx].rolloff;
			cfg.filter_span_syms = m_spec_table[qidx].filter_span_syms;

			init(cfg);

			return true;
		}
	}

	return false;
}

void wtvsoftmodem_qam::init(const config_t &cfg)
{
	m_cfg = cfg;

	m_sps = m_cfg.sample_rate / m_cfg.symbol_rate;
	m_tx_step = 1.0 / m_sps;
	m_carrier_step = wtvsoftmodem_dsp::radians_per_sample(m_cfg.carrier_freq, m_cfg.sample_rate);
	m_tx_carrier.set_step(m_carrier_step);
	m_rx_nco.set_step(m_carrier_step);
	m_rs_step = m_cfg.sample_rate / (2.0 * m_cfg.symbol_rate);

	build_rrc_tables();

	int32_t half = 2 * m_cfg.filter_span_syms;
	int32_t L = 2 * half + 1;
	std::vector<double> raw(L);
	double norm = 0.0;
	for (int32_t j = 0; j < L; j++)
	{
		raw[j] = rrc_lookup((j - half) * 0.5);
		norm += raw[j] * raw[j];
	}
	if (norm < 1e-12)
		norm = 1.0;

	m_mf_taps.resize(L);
	for (int32_t j = 0; j < L; j++)
		m_mf_taps[j] = raw[j] / norm;

	const int32_t HL = 2 * HILBERT_HALF_TAPS + 1;
	m_rx_hilb    = wtvsoftmodem_dsp::design_hilbert(HL);
	m_rx_hilb_odd.clear();
	m_rx_hilb_delay = HL / 2;
	for (int32_t k = 0; k < HL; k++)
	{
		if ((k - m_rx_hilb_delay) & 1)
			m_rx_hilb_odd.push_back({ size_t(k), m_rx_hilb[k] });
	}

	m_pipeline_delay_syms = wtvsoftmodem_dsp::round_nearest<int32_t>(double(m_rx_hilb_delay) / m_sps + 1.0 + m_cfg.filter_span_syms);
	m_pipeline_delay_syms += m_cfg.filter_span_syms;

	wtvsoftmodem_dsp::loop_gains(m_cfg.timing_loop_bw, m_tl_kp, m_tl_ki);
	wtvsoftmodem_dsp::loop_gains(m_cfg.carrier_loop_bw, m_cl_kp, m_cl_ki);

	m_slicer = nullptr;

	reset();
}

void wtvsoftmodem_qam::reset()
{
	rx_use_feedback_equalizer(false);

	m_tx_syms.clear();
	m_tx_sym0_index = 0;
	m_tx_sym_time = 0.0;
	m_tx_carrier.set_phase(0.0);

	if (m_cfg.tx_post_fir.size() > 1)
		m_tx_post_line.assign(m_cfg.tx_post_fir.size());
	else
		m_tx_post_line.clear();

	m_rx_real_line.assign(m_rx_hilb.size());
	m_rx_nco.set_phase(0.0);

	for (auto &h : m_rs_hist)
		h = csymbol_t {};

	m_rx_n = 0;
	m_rs_next = m_rs_step;
	m_tl_freq = 0.0;
	m_tl_ctrl = 0.0;

	m_mf_line.assign(m_mf_taps.size());
	m_t2_on_time = false;
	m_mf_pow_est = 0.0;

	m_g_prev_on = csymbol_t {};
	m_g_half = csymbol_t {};
	m_g_have_prev = false;

	if (m_cfg.eq_taps > 0)
	{
		int32_t n = m_cfg.eq_taps | 1;
		m_eq_taps.assign(n, csymbol_t {});

		int32_t c = n / 2;
		if ((n - 1 - c) & 1)
			c--;

		m_eq_taps[c] = csymbol_t { 1.0, 0.0 };

		m_eq_line.assign(n);
	}
	else
	{
		m_eq_taps.clear();
		m_eq_line.clear();
	}

	m_rx_gain = 1.0;
	m_agc_primed = false;
	m_rx_theta = 0.0;
	m_cl_freq = 0.0;
	m_sym_count = 0;

	m_rx_ref.clear();
	m_rx_ref_pos = 0;
	m_align_buf.clear();
	m_align_ref_power_n = 0;
	m_ref_lag = 0;
	m_ref_aligned = false;
	m_ref_acquire = false;
	m_ref_error_acc = 0.0;
	m_ref_trained = false;
	m_ref_samples = 0;
	m_dd_latched = false;
	m_rx_frozen = false;
	m_rx_timing_hold = false;
	m_hold_clock_frequency = false;
	m_tl_batch = 0;
	m_tl_batch_count = 0;
	m_tl_batch_sum = 0.0;
	m_clk_est_on = false;

	m_evm_acc = 0.0;
}


void wtvsoftmodem_qam::rx_set_training_reference(const csymbol_t *ref, uint32_t n)
{
	m_rx_ref.assign(ref, ref + n);
	m_align_ref_power_n = 0;
	m_rx_ref_pos        = 0;
	m_align_buf.clear();
	m_ref_lag = 0;
	m_ref_aligned = false;
	m_ref_acquire = false;
	m_ref_error_acc = 0.0;
	m_ref_trained = false;
	m_ref_samples = 0;
}

void wtvsoftmodem_qam::rx_acquire_training_reference(const csymbol_t *ref, uint32_t n)
{
	rx_set_training_reference(ref, n);

	m_ref_acquire = true;
}

void wtvsoftmodem_qam::rx_set_tracking_rates(double timing_bandwidth, double carrier_bandwidth, double agc_rate)
{
	m_tl_batch = 0;
	m_cfg.timing_loop_bw  = std::max(0.0, timing_bandwidth);
	m_cfg.carrier_loop_bw = std::max(0.0, carrier_bandwidth);
	m_cfg.agc_rate = std::max(0.0, agc_rate);

	wtvsoftmodem_dsp::loop_gains(m_cfg.timing_loop_bw, m_tl_kp, m_tl_ki);
	wtvsoftmodem_dsp::loop_gains(m_cfg.carrier_loop_bw, m_cl_kp, m_cl_ki);
}

void wtvsoftmodem_qam::rx_set_paced_timing(double integral_divisor, uint32_t batch_symbols)
{
	m_tl_ki /= integral_divisor;
	m_tl_batch = batch_symbols;
	m_tl_batch_sum = 0.0;
	m_tl_batch_count = 0;
}

void wtvsoftmodem_qam::rx_clock_estimate_start(uint32_t settle_symbols)
{
	m_clk_est_on = true;
	m_clk_est_settle = settle_symbols;
	m_clk_est_skip = 0;
	m_clk_est_n = 0;
	m_clk_est_phase = 0.0;
	m_clk_est_sn = 0.0;
	m_clk_est_snn = 0.0;
	m_clk_est_sp = 0.0;
	m_clk_est_snp = 0.0;
}

bool wtvsoftmodem_qam::rx_clock_estimate(uint32_t minimum_symbols, double &frequency)
{
	m_clk_est_on = false;
	if (m_clk_est_n < minimum_symbols)
		return false;

	const double n = double(m_clk_est_n);

	const double denominator = n * m_clk_est_snn - m_clk_est_sn * m_clk_est_sn;
	if (denominator <= 0.0)
		return false;

	frequency = (n * m_clk_est_snp - m_clk_est_sn * m_clk_est_sp) / denominator;

	m_clk_est_n = 0;

	return std::isfinite(frequency);
}

void wtvsoftmodem_qam::rx_use_feedback_equalizer(bool enabled)
{
	m_use_feedback_eq  = enabled;
	m_feedback_updates = 0;
	m_feedback_error = 0;
	m_feedback_power = 0;

	for (auto &sample : m_eq_feedback)
	{
		sample.index = -1;

		if (enabled)
			sample.inputs.resize(m_eq_taps.size());
	}
}

bool wtvsoftmodem_qam::rx_feedback_equalizer(csymbol_t reference, uint32_t age)
{
	if (!m_use_feedback_eq || m_rx_frozen || m_cfg.eq_mu <= 0 || age >= m_eq_feedback.size())
		return false;

	const int64_t index = m_sym_count - 1 - age;
	if (index < 0)
		return false;

	const auto &sample = m_eq_feedback[size_t(index) % m_eq_feedback.size()];
	if (sample.index != index || sample.gain <= 1e-12)
		return false;

	const double c = std::cos(sample.theta);
	const double s = std::sin(sample.theta);
	const double ei = reference.i - sample.output.i;
	const double eq = reference.q - sample.output.q;

	m_feedback_error = .998 * m_feedback_error + .002 * (ei * ei + eq * eq);
	m_feedback_power = .998 * m_feedback_power + .002 * (reference.i * reference.i + reference.q * reference.q);

	const csymbol_t error { (ei * c - eq * s) / sample.gain, (ei * s + eq * c) / sample.gain };
	const double    mu = m_cfg.eq_mu / sample.energy;
	for (size_t k = 0; k < m_eq_taps.size(); k++)
	{
		const auto &input = sample.inputs[k];
		m_eq_taps[k].i += mu * (error.i * input.i + error.q * input.q);
		m_eq_taps[k].q += mu * (error.q * input.i - error.i * input.q);
	}

	m_feedback_updates++;

	return true;
}

double wtvsoftmodem_qam::rx_evm() const
{
	if (m_use_feedback_eq && m_feedback_updates >= 256 && m_feedback_power > 1e-12)
		return std::sqrt(m_feedback_error / m_feedback_power);
	else
		return std::sqrt(m_evm_acc);
}

uint32_t wtvsoftmodem_qam::tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	const double span = m_cfg.filter_span_syms;

	uint32_t sample_idx = 0;
	for (; sample_idx < sample_count; sample_idx++)
	{
		int64_t kmax = int64_t(std::floor(m_tx_sym_time + span));
		int64_t kmin = int64_t(std::ceil(m_tx_sym_time - span));
		if (kmin < 0)
			kmin = 0;
		if (kmax - m_tx_sym0_index >= int64_t(m_tx_syms.size()))
			break;

		double bi = 0.0;
		double bq = 0.0;
		if (m_tx_sym_time < 2.0 * span)
		{
			for (int64_t k = kmin; k <= kmax; k++)
			{
				const csymbol_t &s = m_tx_syms[size_t(k - m_tx_sym0_index)];

				const double w = rrc_lookup(m_tx_sym_time - double(k));

				bi += s.i * w;
				bq += s.q * w;
			}
		}
		else
		{
			if (m_tx_sym_time - double(kmin) >= span)
				kmin++;
			if (m_tx_sym_time - double(kmax) <= -span)
				kmax--;

			if (kmin <= kmax)
			{
				const double x = (m_tx_sym_time - double(kmin) + m_rrc_half) * m_rrc_step_inv;
				const int32_t index = int32_t(x);
				const double f = x - index;
				const double g = 1.0 - f;

				const int32_t phase = index % RRC_TABLE_RES;
				const double *now = &m_rrc_table[phase * m_rrc_rows];
				const double *next = phase + 1 < RRC_TABLE_RES ? now + m_rrc_rows : &m_rrc_table[1];
				int32_t row = index / RRC_TABLE_RES;

				for (int64_t k = kmin; k <= kmax; k++, row--)
				{
					const csymbol_t &s = m_tx_syms[size_t(k - m_tx_sym0_index)];
					const double w = now[row] * g + next[row] * f;

					bi += s.i * w;
					bq += s.q * w;
				}
			}
		}

		double v = (bi * m_tx_carrier.re - bq * m_tx_carrier.im) * m_cfg.output_scale;

		if (!m_tx_post_line.empty())
		{
			const std::vector<double> &h = m_cfg.tx_post_fir;

			m_tx_post_line.push(v);

			v = m_tx_post_line.accumulate(
				h[0] * v,
				h.size() - 1,
				[&h](double acc, size_t k, double x)
				{
					return acc + h[k + 1] * x;
				},
				1
			);
		}

		sample[sample_idx] = wtvsoftmodem_dsp::saturate_round(v);

		m_tx_carrier.advance();
		m_tx_sym_time += m_tx_step;

		int64_t drop_below = int64_t(std::floor(m_tx_sym_time - span)) - 1;
		while (m_tx_sym0_index < drop_below && !m_tx_syms.empty())
		{
			m_tx_syms.pop_front();
			m_tx_sym0_index++;
		}
	}

	// Clear remaining buffer
	wtvsoftmodem_dsp::clear_samples(sample, sample_count, sample_idx);

	return sample_idx;
}

wtvsoftmodem_qam::csymbol_t wtvsoftmodem_qam::default_slice(csymbol_t y) const
{
	int32_t M = m_cfg.slicer_qam_order < 4 ? 4 : m_cfg.slicer_qam_order;
	int32_t L = wtvsoftmodem_dsp::round_nearest<int32_t>(std::sqrt(double(M)));
	double  norm = std::sqrt(2.0 * (M - 1) / 3.0);

	auto q = [&](double v)
	{
		double t = v * norm;
		double s = 2.0 * std::round((t - 1.0) * 0.5) + 1.0;
		double lim = double(L - 1);

		if (s > lim)
			s = lim;
		if (s < -lim)
			s = -lim;

		return s / norm;
	};

	return csymbol_t { q(y.i), q(y.q) };
}

void wtvsoftmodem_qam::rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		m_rx_real_line.push(double(sample[sample_idx]));

		double aI = m_rx_real_line[size_t(m_rx_hilb_delay)];
		double aQ = 0.0;

		for (const auto &[j, h] : m_rx_hilb_odd)
			aQ += h * m_rx_real_line[j];

		const double c = m_rx_nco.re;
		const double sn = m_rx_nco.im;
		csymbol_t    bb{ aI * c + aQ * sn, aQ * c - aI * sn };
		m_rx_nco.advance();

		m_rs_hist[0] = m_rs_hist[1];
		m_rs_hist[1] = m_rs_hist[2];
		m_rs_hist[2] = m_rs_hist[3];
		m_rs_hist[3] = bb;
		m_rx_n++;

		while (m_rs_next <= double(m_rx_n - 1))
		{
			double mu = m_rs_next - double(m_rx_n - 2);

			csymbol_t x {
				wtvsoftmodem_dsp::cubic(m_rs_hist[0].i, m_rs_hist[1].i, m_rs_hist[2].i, m_rs_hist[3].i, mu),
				wtvsoftmodem_dsp::cubic(m_rs_hist[0].q, m_rs_hist[1].q, m_rs_hist[2].q, m_rs_hist[3].q, mu)
			};
			rx_process_t2(x);

			m_rs_next += m_rs_step * (1.0 - m_tl_ctrl);
		}
	}
}

void wtvsoftmodem_qam::rx_process_t2(csymbol_t x)
{
	m_mf_line.push(x);

	const double *taps = m_mf_taps.data();

	const csymbol_t yraw = m_mf_line.accumulate(
		csymbol_t {},
		m_mf_taps.size(),
		[taps](csymbol_t acc, size_t j, const csymbol_t &h)
		{
			acc.i += taps[j] * h.i;
			acc.q += taps[j] * h.q;

			return acc;
		}
	);

	csymbol_t y = yraw;

	if (!m_eq_taps.empty())
		m_eq_line.push(y);

	m_t2_on_time = !m_t2_on_time;

	if (!m_t2_on_time)
	{
		m_g_half = y;

		return;
	}

	double ypow = y.i * y.i + y.q * y.q;

	if (m_mf_pow_est <= 0.0)
		m_mf_pow_est = ypow + 1e-9;
	else
		m_mf_pow_est = 0.99 * m_mf_pow_est + 0.01 * ypow;

	if (m_g_have_prev && !m_rx_frozen && !m_rx_timing_hold && m_cfg.timing_loop_bw > 0.0)
	{
		double e = (y.i - m_g_prev_on.i) * m_g_half.i + (y.q - m_g_prev_on.q) * m_g_half.q;
		e /= (m_mf_pow_est + 1e-9);
		if (e > 1.0)
			e = 1.0;
		if (e < -1.0)
			e = -1.0;

		if (!m_hold_clock_frequency)
		{
			if (m_tl_batch)
			{
				m_tl_batch_sum += e;
				m_tl_batch_count++;

				if (m_tl_batch_count >= m_tl_batch)
				{
					m_tl_freq += m_tl_ki * m_tl_batch_sum;
					m_tl_batch_sum   = 0.0;
					m_tl_batch_count = 0;
				}
			}
			else
			{
				m_tl_freq += m_tl_ki * e;
			}
		}

		m_tl_ctrl = m_tl_freq + m_tl_kp * e;
		if (m_tl_ctrl > 0.25)
			m_tl_ctrl = 0.25;
		if (m_tl_ctrl < -0.25)
			m_tl_ctrl = -0.25;

		m_clk_est_skip++;
		if (m_clk_est_on && m_ref_aligned && m_clk_est_skip > m_clk_est_settle)
		{
			m_clk_est_phase += m_tl_ctrl;
			const double n = double(m_clk_est_n++);

			m_clk_est_sn += n;
			m_clk_est_snn += n * n;
			m_clk_est_sp += m_clk_est_phase;
			m_clk_est_snp += n * m_clk_est_phase;
		}
	}

	m_g_prev_on = y;
	m_g_have_prev = true;

	rx_emit_symbol(y);
}

void wtvsoftmodem_qam::rx_emit_symbol(csymbol_t on_time)
{
	csymbol_t eq = rx_equalize(on_time);

	double epow = eq.i * eq.i + eq.q * eq.q;
	if (!m_agc_primed && epow > 1e-12)
	{
		m_rx_gain    = std::sqrt(m_cfg.agc_target_power / epow);
		m_agc_primed = true;
	}

	csymbol_t g { eq.i * m_rx_gain, eq.q * m_rx_gain };
	csymbol_t d = rotate(g, std::cos(-m_rx_theta), std::sin(-m_rx_theta));

	csymbol_t rdec;
	bool have_ref = rx_reference_symbol(d, rdec);
	csymbol_t sl = m_slicer ? m_slicer(d) : default_slice(d);
	csymbol_t dec = have_ref ? rdec : sl;

	double dpow = dec.i * dec.i + dec.q * dec.q + 1e-9;
	csymbol_t err { dec.i - d.i, dec.q - d.q };
	if (m_ref_acquire && have_ref && rx_reference_ended(err, dpow))
	{
		m_rx_ref.clear();
		have_ref = false;
		dec      = sl;
		dpow     = dec.i * dec.i + dec.q * dec.q + 1e-9;
		err      = { dec.i - d.i, dec.q - d.q };
	}

	double slpow = sl.i * sl.i + sl.q * sl.q + 1e-9;
	m_evm_acc    = 0.99 * m_evm_acc + 0.01 * ((sl.i - d.i) * (sl.i - d.i) + (sl.q - d.q) * (sl.q - d.q)) / slpow;

	if (!m_dd_latched && (have_ref || !m_cfg.carrier_blind || m_evm_acc < 0.04 || m_sym_count >= m_cfg.blind_symbols))
		m_dd_latched = true;

	if (!m_rx_frozen)
	{
		if (m_use_feedback_eq && !m_eq_taps.empty())
			rx_capture_feedback(d);

		rx_track_carrier(d, dec, dpow);
		rx_track_agc(d, dec);

		if (!m_use_feedback_eq && !m_eq_taps.empty() && m_cfg.eq_mu > 0.0 && m_rx_gain > 1e-12 && (have_ref || (m_ref_aligned && m_evm_acc < 0.05)))
			rx_adapt_equalizer(err);
	}
	else
	{
		m_rx_theta = wtvsoftmodem_dsp::wrap_radians(m_rx_theta + m_cl_freq);
	}

	m_sym_count++;
	if (m_cfg.freeze_after > 0 && m_sym_count >= m_cfg.freeze_after && !m_rx_frozen)
		rx_freeze_loops(true);

	if (m_rx_cb)
		m_rx_cb(d);
}

inline wtvsoftmodem_qam::csymbol_t wtvsoftmodem_qam::rx_equalize(csymbol_t on_time) const
{
	if (m_eq_taps.empty())
		return on_time;

	return m_eq_line.accumulate(
		csymbol_t{},
		m_eq_taps.size(),
		[this](csymbol_t acc, size_t k, const csymbol_t &u)
		{
			acc.i += m_eq_taps[k].i * u.i - m_eq_taps[k].q * u.q;
			acc.q += m_eq_taps[k].i * u.q + m_eq_taps[k].q * u.i;

			return acc;
		}
	);
}

inline bool wtvsoftmodem_qam::rx_reference_symbol(csymbol_t &d, csymbol_t &ref)
{
	if (m_rx_ref.empty())
		return false;

	if (!m_ref_aligned)
		rx_align_reference(d);

	if (!m_ref_aligned)
		return false;

	int64_t ri = m_sym_count + m_ref_lag;
	if (ri < 0 || ri >= int64_t(m_rx_ref.size()))
		return false;

	ref = m_rx_ref[size_t(ri)];

	return true;
}

inline void wtvsoftmodem_qam::rx_align_reference(csymbol_t &d)
{
	m_align_buf.push_back(d);

	const int32_t window = m_ref_acquire ? ACQUIRE_WIN : ALIGN_WIN;
	if (m_ref_acquire && int32_t(m_align_buf.size()) > window)
		m_align_buf.pop_front();

	if (int32_t(m_align_buf.size()) < window)
		return;

	const reference_match best = rx_search_reference(window);
	if (m_ref_acquire && best.metric < 0.75)
		return;

	m_ref_lag     = m_ref_acquire ? best.lag - (m_sym_count - window + 1) : best.lag;
	m_ref_aligned = true;

	if (!m_ref_acquire)
		return;

	const double amplitude = std::abs(best.gain);
	if (amplitude > 1e-12)
	{
		m_rx_gain /= amplitude;
		d.i /= amplitude;
		d.q /= amplitude;
	}

	const double correction = std::arg(best.gain);
	m_rx_theta = wtvsoftmodem_dsp::wrap_radians(m_rx_theta + correction);
	d = rotate(d, std::cos(-correction), std::sin(-correction));
	m_ref_error_acc = 0.0;
}

inline wtvsoftmodem_qam::reference_match wtvsoftmodem_qam::rx_search_reference(int32_t window)
{
	const int32_t max_lag = 4 * m_cfg.filter_span_syms + 48;
	const int32_t n  = int32_t(m_align_buf.size());
	const int32_t ref_n = int32_t(m_rx_ref.size());
	const int32_t lags = max_lag + 1;

	if (m_align_ref_power_n != n)
	{
		m_align_ref_power.assign(lags, 0.0);
		for (int32_t k = 0; k < n; k++)
		{
			const int32_t lag_end = std::min(lags, ref_n - k);
			for (int32_t lag = 0; lag < lag_end; lag++)
				m_align_ref_power[lag] += m_rx_ref[k + lag].i * m_rx_ref[k + lag].i + m_rx_ref[k + lag].q * m_rx_ref[k + lag].q;
		}
		m_align_ref_power_n = n;
	}

	m_align_rx_power.resize(n + 1);
	m_align_rx_power[0] = 0.0;
	m_align_acc_i.assign(lags, 0.0);
	m_align_acc_q.assign(lags, 0.0);

	double *const acc_i = m_align_acc_i.data();
	double *const acc_q = m_align_acc_q.data();
	for (int32_t k = 0; k < n; k++)
	{
		const csymbol_t  b = m_align_buf[k];
		const csymbol_t *ref = m_rx_ref.data() + k;
		const int32_t    lag_end = std::min(lags, ref_n - k);

		m_align_rx_power[k + 1] = m_align_rx_power[k] + (b.i * b.i + b.q * b.q);

		for (int32_t lag = 0; lag < lag_end; lag++)
		{
			acc_i[lag] += b.i * ref[lag].i + b.q * ref[lag].q;
			acc_q[lag] += b.q * ref[lag].i - b.i * ref[lag].q;
		}
	}

	reference_match best;
	for (int32_t lag = 0; lag <= max_lag; lag++)
	{
		const std::complex<double> acc(acc_i[lag], acc_q[lag]);
		const double               ref_power = m_align_ref_power[lag];
		const double               rx_power  = m_align_rx_power[std::clamp(ref_n - lag, 0, n)];
		const double               metric    = m_ref_acquire ? std::norm(acc) / (ref_power * rx_power + 1e-20) : std::abs(acc);
		if (metric > best.metric)
		{
			best.metric = metric;
			best.lag    = lag;
			best.gain   = acc / (ref_power + 1e-20);
		}
	}
	return best;
}

inline bool wtvsoftmodem_qam::rx_reference_ended(csymbol_t err, double dpow)
{
	m_ref_error_acc = 0.95 * m_ref_error_acc + 0.05 * (err.i * err.i + err.q * err.q) / dpow;
	m_ref_samples++;

	if (m_ref_samples > 200 && m_ref_error_acc < 0.02)
		m_ref_trained = true;

	return m_ref_trained && m_ref_error_acc > 0.10;
}

inline void wtvsoftmodem_qam::rx_capture_feedback(csymbol_t d)
{
	auto &sample  = m_eq_feedback[size_t(m_sym_count) % m_eq_feedback.size()];

	sample.index = m_sym_count;
	sample.output = d;
	sample.gain = m_rx_gain;
	sample.theta = m_rx_theta;
	sample.energy = 1e-6;

	m_eq_line.visit(
		m_eq_taps.size(),
		[&sample](size_t k, const csymbol_t &input)
		{
			sample.inputs[k] = input;
			sample.energy += input.i * input.i + input.q * input.q;
		}
	);
}

inline void wtvsoftmodem_qam::rx_track_carrier(csymbol_t d, csymbol_t dec, double dpow)
{
	double ephi;
	if (m_dd_latched)
	{
		ephi = (d.q * dec.i - d.i * dec.q) / dpow;
	}
	else
	{
		double r2  = d.i * d.i + d.q * d.q;
		double re2 = d.i * d.i - d.q * d.q;
		double im2 = 2.0 * d.i * d.q;
		double im4 = 2.0 * re2 * im2;

		ephi = -im4 / (r2 * r2 + 1e-9);
	}

	ephi = std::clamp(ephi, -1.0, 1.0);

	if (m_cfg.carrier_loop_bw > 0.0)
	{
		if (!m_hold_clock_frequency)
			m_cl_freq += m_cl_ki * ephi;

		m_rx_theta += m_cl_freq + m_cl_kp * ephi;
		m_rx_theta = wtvsoftmodem_dsp::wrap_radians(m_rx_theta);
	}
}

inline void wtvsoftmodem_qam::rx_track_agc(csymbol_t d, csymbol_t dec)
{
	if (m_cfg.agc_rate <= 0.0)
		return;

	double p = d.i * d.i + d.q * d.q;
	double step = m_cfg.agc_rate * (m_cfg.agc_target_power - p) / m_cfg.agc_target_power;

	if (m_cfg.agc_decision_directed)
		step = m_cfg.agc_rate * ((dec.i * d.i + dec.q * d.q) / (p + 1e-12) - 1.0);

	m_rx_gain *= 1.0 + std::clamp(step, -0.10, 0.10);
	m_rx_gain = std::clamp(m_rx_gain, 1e-8, 1e4);
}

inline void wtvsoftmodem_qam::rx_adapt_equalizer(csymbol_t err)
{
	csymbol_t e_bt = rotate(err, std::cos(m_rx_theta), std::sin(m_rx_theta));

	e_bt.i /= m_rx_gain;
	e_bt.q /= m_rx_gain;

	const size_t N = m_eq_taps.size();
	double energy = 1e-6;

	for (size_t k = 0; k < N; k++)
		energy += m_eq_line.slot(k).i * m_eq_line.slot(k).i + m_eq_line.slot(k).q * m_eq_line.slot(k).q;

	double mu = m_cfg.eq_mu / energy;

	m_eq_line.visit(
		N,
		[this, mu, &e_bt](size_t k, const csymbol_t &u)
		{
			m_eq_taps[k].i += mu * (e_bt.i * u.i + e_bt.q * u.q);
			m_eq_taps[k].q += mu * (e_bt.q * u.i - e_bt.i * u.q);
		}
	);
}
