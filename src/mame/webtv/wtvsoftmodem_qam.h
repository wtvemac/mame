// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_QAM_H
#define MAME_WEBTV_WTVSOFTMODEM_QAM_H

#pragma once

#include "wtvsoftmodem_dsp.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

class wtvsoftmodem_qam
{
public:

	struct csymbol_t
	{
		double i = 0.0;
		double q = 0.0;
	};

	struct config_t
	{
		wtvsoftmodem_dsp::frequency_t sample_rate = 16000.0;
		double symbol_rate = 24000.0 / 7.0;
		double carrier_freq = 96000.0 / 49.0;
		double rolloff = 0.20;
		int32_t filter_span_syms = 8;
		double output_scale = 6000.0;

		std::function<double(double)> tx_pulse_fn;

		std::vector<double> tx_post_fir;

		double timing_loop_bw = 0.006;
		double carrier_loop_bw = 0.004;
		bool carrier_blind = true;
		double agc_rate = 0.003;
		double agc_target_power = 1.0;
		bool agc_decision_directed = false;
		int32_t blind_symbols = 300;
		int32_t freeze_after = 0;
		int32_t eq_taps = 0;
		double eq_mu = 0.20;
		int32_t slicer_qam_order = 4;
	};

	typedef std::function<void(csymbol_t)> rx_symbol_cb;
	typedef std::function<csymbol_t(csymbol_t)> slicer_fn;

	typedef uint8_t qam_spec_t;

	typedef struct qam_detail
	{
		qam_spec_t spec;
		double symbol_rate;
		double carrier_freq;
		double rolloff;
		int32_t filter_span_syms;
	} qam_detail_t;

	static constexpr qam_spec_t QAM_V34_2400_LOW = 0x01;
	static constexpr qam_spec_t QAM_V34_2743_LOW = 0x02;
	static constexpr qam_spec_t QAM_V34_2800_LOW = 0x03;
	static constexpr qam_spec_t QAM_V34_3000_LOW = 0x04;
	static constexpr qam_spec_t QAM_V34_3200_LOW = 0x05;
	static constexpr qam_spec_t QAM_V34_3429_LOW = 0x06;
	static constexpr qam_spec_t QAM_V34_PRIMARY  = 0x07;

	wtvsoftmodem_qam();
	explicit wtvsoftmodem_qam(const config_t &cfg);

	void set_qam_spec_table(const qam_detail_t *set_qam_detail_table, uint8_t set_qam_detail_count);

	bool init(qam_spec_t qam_spec, config_t cfg);
	void init(const config_t &cfg);
	void reset();

	static constexpr const qam_detail_t *default_spec(qam_spec_t qam_spec)
	{
		for (const qam_detail_t &detail : DEFAULT_QAM_SPEC_TABLE)
		{
			if (detail.spec == qam_spec)
				return &detail;
		}
		return nullptr;
	}

	const config_t &config() const
	{
		return m_cfg;
	}

	int32_t pipeline_delay_symbols() const
	{
		return m_pipeline_delay_syms;
	}

	void set_tx_post_fir(std::vector<double> taps)
	{
		m_cfg.tx_post_fir = std::move(taps);

		if (m_cfg.tx_post_fir.size() > 1)
			m_tx_post_line.assign(m_cfg.tx_post_fir.size());
		else
			m_tx_post_line.clear();
	}

	void tx_flush_pending()
	{
		m_tx_syms.clear();
		m_tx_sym0_index = int64_t(std::floor(m_tx_sym_time - m_cfg.filter_span_syms));
	}

	void tx_put_symbol(csymbol_t s)
	{
		m_tx_syms.push_back(s);
	}

	void tx_put_symbols(const csymbol_t *s, uint32_t n)
	{
		for (uint32_t k = 0; k < n; k++)
			m_tx_syms.push_back(s[k]);
	}

	size_t tx_pending_symbols() const
	{
		return m_tx_syms.size();
	}

	void set_rx_symbol_callback(rx_symbol_cb cb)
	{
		m_rx_cb = std::move(cb);
	}
	void set_slicer(slicer_fn fn)
	{
		m_slicer = std::move(fn);
	}

	void rx_set_equalizer_rate(double rate)
	{
		m_cfg.eq_mu = rate;
	}
	void rx_use_decision_agc(bool enabled)
	{
		m_cfg.agc_decision_directed = enabled;
	}

	void rx_clear_training_reference()
	{
		m_rx_ref.clear();
		m_align_buf.clear();
		m_ref_aligned = false;
	}

	void rx_finish_training()
	{
		m_rx_ref.clear();
		m_align_buf.clear();
		m_ref_acquire = false;
	}

	bool rx_reference_aligned() const
	{
		return m_ref_aligned;
	}

	bool rx_reference_trained() const
	{
		return m_ref_trained;
	}

	bool rx_training_reference_active() const
	{
		return !m_rx_ref.empty();
	}

	bool rx_decision_directed() const
	{
		return m_dd_latched;
	}

	void rx_rearm_blind_carrier()
	{
		m_dd_latched = false;
		m_sym_count = 0;
		m_evm_acc = 1.0;
	}

	void rx_correct_carrier_phase(double phase)
	{
		m_rx_theta += phase;
	}

	void rx_hold_clock_frequency(bool hold)
	{
		m_hold_clock_frequency = hold;
	}

	void rx_hold_timing(bool hold)
	{
		m_rx_timing_hold = hold;

		if (hold)
			m_tl_ctrl = m_tl_freq;
	}
	void rx_freeze_loops(bool freeze)
	{
		m_hold_clock_frequency = false;
		m_rx_frozen = freeze;

		if (freeze)
			m_tl_ctrl = m_tl_freq;
	}

	const std::vector<csymbol_t> &rx_equalizer() const
	{
		return m_eq_taps;
	}

	void rx_prime_equalizer(const std::vector<csymbol_t> &taps)
	{
		if (taps.size() == m_eq_taps.size())
			m_eq_taps = taps;
	}

	double rx_timing_frequency() const
	{
		return m_tl_freq;
	}

	double rx_carrier_frequency() const
	{
		return m_cl_freq;
	}

	void rx_set_clock_frequencies(double timing, double carrier)
	{
		m_tl_freq = timing;
		m_tl_ctrl = timing;
		m_cl_freq = carrier;
	}

	uint32_t tx(wtvsoftmodem_dsp::sample_t* sample, uint32_t sample_count);

	void rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);
	void rx_set_training_reference(const csymbol_t *ref, uint32_t n);
	void rx_acquire_training_reference(const csymbol_t *ref, uint32_t n);
	void rx_set_tracking_rates(double timing_bandwidth, double carrier_bandwidth, double agc_rate);
	void rx_set_paced_timing(double integral_divisor, uint32_t batch_symbols);
	void rx_clock_estimate_start(uint32_t settle_symbols);
	bool rx_clock_estimate(uint32_t minimum_symbols, double &frequency);
	uint32_t rx_clock_estimate_count() const { return m_clk_est_n; }

	void rx_reference_snr_reset()
	{
		m_ref_err_sum = 0.0;
		m_ref_pow_sum = 0.0;
		m_ref_snr_n = 0;
	}
	uint32_t rx_reference_snr_count() const { return m_ref_snr_n; }
	double rx_reference_error_sum() const { return m_ref_err_sum; }
	double rx_reference_power_sum() const { return m_ref_pow_sum; }
	double rx_reference_snr_db() const { return wtvsoftmodem_dsp::power_to_db(m_ref_pow_sum / std::max(m_ref_err_sum, 1e-12)); }
	void rx_use_feedback_equalizer(bool enabled);
	bool rx_feedback_equalizer(csymbol_t reference, uint32_t age);
	double rx_evm() const;

private:

	static constexpr qam_detail_t DEFAULT_QAM_SPEC_TABLE[] = {
		{
			.spec = QAM_V34_2400_LOW,
			.symbol_rate = 2400.0, .carrier_freq = 1600.0,
			.rolloff = 0.12,
			.filter_span_syms = 8
		},
		{
			.spec = QAM_V34_2743_LOW,
			.symbol_rate = 2400.0 * 8.0 / 7.0,
			.carrier_freq = 2400.0 * 8.0 / 7.0 * 3.0 / 5.0,
			.rolloff = 0.12,
			.filter_span_syms = 8
		},
		{
			.spec = QAM_V34_2800_LOW,
			.symbol_rate = 2800.0,
			.carrier_freq = 1680.0,
			.rolloff = 0.12,
			.filter_span_syms = 8
		},
		{
			.spec = QAM_V34_3000_LOW,
			.symbol_rate = 3000.0,
			.carrier_freq = 1800.0,
			.rolloff = 0.12,
			.filter_span_syms = 8
		},
		{
			.spec = QAM_V34_3200_LOW,
			.symbol_rate = 3200.0,
			.carrier_freq = 3200.0 * 4.0 / 7.0,
			.rolloff = 0.12,
			.filter_span_syms = 8
		},
		{
			.spec = QAM_V34_3429_LOW,
			.symbol_rate = 2400.0 * 10.0 / 7.0,
			.carrier_freq = 2400.0 * 10.0 / 7.0 * 4.0 / 7.0,
			.rolloff = 0.14,
			.filter_span_syms = 18
		},
		{
			.spec = QAM_V34_PRIMARY,
			.symbol_rate = 24000.0 / 7.0,
			.carrier_freq = 96000.0 / 49.0,
			.rolloff = 0.14,
			.filter_span_syms = 18
		}
	};

	uint8_t m_spec_count = 0;
	const qam_detail_t *m_spec_table = nullptr;

	static constexpr int32_t RRC_TABLE_RES = 2048;

	static constexpr int32_t ALIGN_WIN   = 48;
	static constexpr int32_t ACQUIRE_WIN = 96;

	static constexpr int32_t HILBERT_HALF_TAPS = 24;

	config_t m_cfg;

	struct reference_match
	{
		int32_t lag = 0;
		double metric = -1.0;
		std::complex<double> gain { 1, 0 };
	};

	struct feedback_sample
	{
		int64_t index = -1;
		csymbol_t output {};
		double gain = 1;
		double theta = 0;
		double energy = 0;
		std::vector<csymbol_t> inputs;
	};

	double m_sps = 1.0;
	double m_tx_step = 1.0;
	double m_carrier_step = 0.0;
	int32_t m_pipeline_delay_syms = 0;

	double m_tl_kp = 0.0;
	double m_tl_ki = 0.0;
	uint32_t m_tl_batch = 0;
	uint32_t m_tl_batch_count = 0;
	double m_tl_batch_sum = 0.0;
	bool m_clk_est_on = false;
	uint32_t m_clk_est_settle = 0;
	uint32_t m_clk_est_skip = 0;
	uint32_t m_clk_est_n = 0;
	double m_clk_est_phase = 0.0;
	double m_clk_est_sn = 0.0;
	double m_clk_est_snn = 0.0;
	double m_clk_est_sp = 0.0;
	double m_clk_est_snp = 0.0;
	double m_cl_kp = 0.0;
	double m_cl_ki = 0.0;

	std::deque<csymbol_t> m_tx_syms;
	int64_t m_tx_sym0_index = 0;
	double m_tx_sym_time = 0.0;
	wtvsoftmodem_dsp::oscillator m_tx_carrier;

	wtvsoftmodem_dsp::delay_line<double> m_tx_post_line;

	rx_symbol_cb m_rx_cb;
	slicer_fn m_slicer;

	std::vector<double> m_rx_hilb;
	std::vector<std::pair<size_t, double>> m_rx_hilb_odd;
	wtvsoftmodem_dsp::delay_line<double> m_rx_real_line;
	int32_t m_rx_hilb_delay = 0;
	wtvsoftmodem_dsp::oscillator m_rx_nco;

	csymbol_t m_rs_hist[4];
	int64_t m_rx_n = 0;
	double m_rs_next = 0.0;
	double m_rs_step = 0.0;
	double m_tl_freq = 0.0;
	double m_tl_ctrl = 0.0;

	std::vector<double> m_mf_taps;
	wtvsoftmodem_dsp::delay_line<csymbol_t> m_mf_line;
	bool m_t2_on_time = false;
	double m_mf_pow_est = 0.0;

	csymbol_t m_g_prev_on;
	csymbol_t m_g_half;
	bool m_g_have_prev = false;

	std::vector<csymbol_t> m_eq_taps;
	std::array<feedback_sample, 128> m_eq_feedback;
	bool m_use_feedback_eq = false;
	uint64_t m_feedback_updates = 0;
	double m_feedback_error = 0;
	double m_feedback_power = 0;
	wtvsoftmodem_dsp::delay_line<csymbol_t> m_eq_line;

	double m_rx_gain = 1.0;
	bool m_agc_primed = false;
	double m_rx_theta = 0.0;
	double m_cl_freq = 0.0;
	int64_t m_sym_count = 0;

	std::vector<csymbol_t> m_rx_ref;
	size_t m_rx_ref_pos = 0;
	std::deque<csymbol_t> m_align_buf;
	std::vector<double> m_align_acc_i;
	std::vector<double> m_align_acc_q;
	std::vector<double> m_align_rx_power;
	std::vector<double> m_align_ref_power;

	int32_t m_align_ref_power_n = 0;
	int64_t m_ref_lag = 0;
	bool m_ref_aligned = false;
	bool m_ref_acquire = false;
	double m_ref_error_acc = 0.0;
	bool m_ref_trained = false;
	uint32_t m_ref_samples = 0;
	bool m_dd_latched = false;
	bool m_hold_clock_frequency = false;
	double m_evm_acc = 0.0;
	double m_ref_err_sum = 0.0;
	double m_ref_pow_sum = 0.0;
	uint32_t m_ref_snr_n = 0;
	bool m_rx_frozen = false;
	bool m_rx_timing_hold = false;

	std::vector<double> m_rrc_table;
	int32_t m_rrc_count = 0;
	int32_t m_rrc_rows = 0;
	double m_rrc_step_inv = 0.0;
	int32_t m_rrc_half = 0;

	static csymbol_t rotate(csymbol_t x, double c, double s)
	{
		return { x.i * c - x.q * s, x.i * s + x.q * c };
	}

	double rrc_lookup(double t) const
	{
		if (t <= -m_rrc_half || t >= m_rrc_half)
			return 0.0;

		double x = (t + m_rrc_half) * m_rrc_step_inv;
		int32_t i0 = int32_t(x);
		double f = x - i0;

		if (i0 + 1 >= m_rrc_count)
			return rrc_entry(i0);
		else
			return rrc_entry(i0) * (1.0 - f) + rrc_entry(i0 + 1) * f;
	}

	double rrc_entry(int32_t i) const
	{
		return m_rrc_table[(i % RRC_TABLE_RES) * m_rrc_rows + i / RRC_TABLE_RES];
	}

	void build_rrc_tables();

	csymbol_t default_slice(csymbol_t y) const;

	void rx_process_t2(csymbol_t x);
	void rx_emit_symbol(csymbol_t on_time);
	csymbol_t rx_equalize(csymbol_t on_time) const;
	bool rx_reference_symbol(csymbol_t &d, csymbol_t &ref);
	void rx_align_reference(csymbol_t &d);
	reference_match rx_search_reference(int32_t window);
	bool rx_reference_ended(csymbol_t err, double dpow);
	void rx_capture_feedback(csymbol_t d);
	void rx_track_carrier(csymbol_t d, csymbol_t dec, double dpow);
	void rx_track_agc(csymbol_t d, csymbol_t dec);
	void rx_adapt_equalizer(csymbol_t err);

};

#endif // MAME_WEBTV_WTVSOFTMODEM_QAM_H
