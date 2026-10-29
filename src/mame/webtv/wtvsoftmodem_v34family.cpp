// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34family.h"
#include <format>
#include "wtvsoftmodem_v34filter.h"

#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

// ITU-T V.34 11 Duplex operating procedures, ITU-T V.90 9 Operating procedures

using namespace wtvsoftmodem::v34;

static_assert(wtvsoftmodem_v34family::pcm_to_state(wtvsoftmodem_pcm::PCMState::RDBAR) == wtvsoftmodem_v34family::PH_PCM_RDBAR, "PH_PCM_* must mirror wtvsoftmodem_pcm::PCMState");
static_assert(wtvsoftmodem_v34family::phase_of(wtvsoftmodem_v34family::PH_PCM_RDBAR) == wtvsoftmodem_v34family::PHASE_PCM, "PCM stages must fit in PHASE_PCM");

wtvsoftmodem_v34family::wtvsoftmodem_v34family()
{
	set_sample_rate(wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE);

	m_pcm.set_state_change_callback(
		[this](wtvsoftmodem_pcm::PCMState, wtvsoftmodem_pcm::PCMState new_state)
		{
			pcm_state_changed(new_state);
		}
	);
}

wtvsoftmodem_qam::csymbol_t wtvsoftmodem_v34family::four_point_decision(wtvsoftmodem_qam::csymbol_t point)
{
	return { point.i >= 0 ? SUBC_AMP : -SUBC_AMP, point.q >= 0 ? SUBC_AMP : -SUBC_AMP };
}

int32_t wtvsoftmodem_v34family::shaper_variant() const
{
	if (m_tx_preemphasis >= 0)
		return m_tx_preemphasis;

	return m_got_info1c ? m_peer_info1c.rate[m_v34_baud].pre_emphasis : 0;
}

void wtvsoftmodem_v34family::set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate)
{
	m_sr  = set_sample_rate;
	m_qsr = set_sample_rate;

	const bool info_answer = m_answer && !m_v90_mode;
	m_dpsk.set_sample_rate(set_sample_rate);
	m_dpsk.set_output_scale(m_v90_mode ? 10000.0 : V34_DPSK_OUT_SCALE);
	m_dpsk.init(wtvsoftmodem_dpsk::DPSK_V34_INFO);
	m_dpsk.set_tx_channel(info_answer ? wtvsoftmodem_dpsk::V34_INFO_ANSWER_CHANNEL : wtvsoftmodem_dpsk::V34_INFO_CALL_CHANNEL);
	m_dpsk.set_rx_channel(info_answer ? wtvsoftmodem_dpsk::V34_INFO_CALL_CHANNEL : wtvsoftmodem_dpsk::V34_INFO_ANSWER_CHANNEL);

	init_v34_qam();

	wtvsoftmodem_v34train::config_t tcfg;
	tcfg.s_amplitude  = SUBC_AMP;
	tcfg.trn_scale    = SUBC_AMP;
	tcfg.answer_mode  = m_answer;
	m_train.init(tcfg);
}

void wtvsoftmodem_v34family::init_v34_qam()
{
	wtvsoftmodem_qam::config_t qc;
	qc.sample_rate      = m_qsr;
	qc.output_scale     = V34_OUT_SCALE;
	qc.agc_target_power = 1.0;

	qc.tx_post_fir = filter::shaping::resample(shaper_variant(), m_qsr, m_v34_carrier, V34_SHAPER_TAPS, m_v34_baud, m_v34_high_carrier);
	qc.eq_taps     = V34_EQ_TAPS;
	qc.eq_mu       = V34_TRN_EQ_RATE;

	if (m_v34_baud == V34_BAUD_3429)
	{
		m_qam.init(V34_QAM_SPEC, qc);
	}
	else
	{
		const auto *spec = wtvsoftmodem_qam::default_spec(UPSTREAM_SPEC[m_v34_baud]);
		qc.symbol_rate      = spec->symbol_rate;
		qc.carrier_freq     = m_v34_carrier;
		qc.rolloff          = spec->rolloff;
		qc.filter_span_syms = spec->filter_span_syms;
		m_qam.init(qc);
	}
}

bool wtvsoftmodem_v34family::v34_baud_allowed(int32_t baud, bool high_carrier) const
{
	if (!m_got_info0c)
	{
		return true;
	}
	else
	{
		switch (baud)
		{
			case 0:
				return true;
			case 1:
				return m_peer_info0.sr_2743;
			case 2:
				return m_peer_info0.sr_2800;
			case 3:
				return high_carrier ? m_peer_info0.sr_3000_high : m_peer_info0.sr_3000_low;
			case 4:
				return high_carrier ? m_peer_info0.sr_3200_high : m_peer_info0.sr_3200_low;
			case 5:
				return m_peer_info0.sr_3429 && m_peer_info0.rate_3429_allowed;
			default:
				return false;
		}
	}
}

int32_t wtvsoftmodem_v34family::select_v34_baud() const
{
	if (!m_got_info1c)
	{
		return V34_BAUD_3429;
	}
	else
	{
		int32_t best = -1;
		for (int32_t baud = V34_BAUD_3429; baud >= 0; baud--)
		{
			const auto &rate = m_peer_info1c.rate[baud];

			if (!rate.max_bit_rate || !v34_baud_allowed(baud, rate.use_high_carrier))
				continue;

			if (best < 0 || rate.max_bit_rate > m_peer_info1c.rate[best].max_bit_rate)
				best = baud;
		}

		return best < 0 ? V34_BAUD_3429 : best;
	}
}

void wtvsoftmodem_v34family::set_v34_symbol_rate(int32_t baud, bool high_carrier)
{
	if (baud == m_v34_baud && high_carrier == m_v34_high_carrier)
		return;

	m_v34_baud = baud;
	m_v34_high_carrier = high_carrier;

	if (baud == V34_BAUD_3429)
	{
		m_v34_symbol_rate = V34_SYMBOL_RATE;
		m_v34_carrier = V34_CARRIER;
	}
	else
	{
		const auto *spec = wtvsoftmodem_qam::default_spec(UPSTREAM_SPEC[baud]);

		m_v34_symbol_rate = spec->symbol_rate;
		m_v34_carrier = high_carrier ? spec->symbol_rate * BAUD_HIGH_CARRIER_RATIO[baud] : spec->carrier_freq;
	}

	init_v34_qam();
}

void wtvsoftmodem_v34family::configure_datamode(int32_t bit_rate_code, int32_t baud_rate_code, bool expanded_shaping)
{
	wtvsoftmodem_v34data::config_t dc;
	dc.bit_rate_code    = bit_rate_code;
	dc.baud_rate_code   = baud_rate_code;
	dc.answer_mode      = m_answer;
	dc.expanded_shaping = expanded_shaping;
	dc.point_scale = SUBC_AMP * std::sqrt(2.0);
	dc.invert_output = true;
	dc.use_precoder      = m_had_mp && m_peer_mp.type == 1 && m_peer_mp.precoding_enabled;
	const auto peer_taps = info::mp_precoder(m_peer_mp);
	for (int32_t tap = 0; tap < 3; tap++)
	{
		for (int32_t axis = 0; axis < 2; axis++)
			dc.precoder_coeffs[tap][axis] = peer_taps[2 - tap][axis];
	}
	dc.nonlin_k = (m_had_mp && m_peer_mp.nonlinear_encoder) ? 1.0 : 0.0;
	dc.apply_tx_gains();

	m_datamode.init(dc);
	m_lapm.set_bit_rate(uint32_t(m_datamode.bit_rate()));

	m_dm_e_bits = 0;

	m_datamode.set_bit_source(
		[this]() -> int32_t
		{
			if (m_dm_e_bits > 0)
			{
				m_dm_e_bits--;
				return 1;
			}

			return m_lapm.tx_get_bit() ? 1 : 0;
		}
	);
}

void wtvsoftmodem_v34family::setup_datamode_channel()
{
	if (!m_retraining || m_peer_e_received)
		install_data_slicer();

	m_lapm.set_rx_byte_callback(
		[this](uint8_t b)
		{
			if (m_rx_byte_cb)
				m_rx_byte_cb(b);
		}
	);

	const uint32_t peer_hw_cap = (m_got_info1c && m_peer_info1c.rate[m_v34_baud].max_bit_rate > 0) ? (uint32_t)m_peer_info1c.rate[m_v34_baud].max_bit_rate : 15U;
	uint32_t nr = (m_had_mp && m_peer_mp.bit_rate_a_to_c > 0) ? m_peer_mp.bit_rate_a_to_c : std::min<uint32_t>(V34_MAX_RATE, peer_hw_cap);

	if (m_local_mp.bit_rate_a_to_c > 0)
		nr = std::min<uint32_t>(nr, m_local_mp.bit_rate_a_to_c);

	if (m_had_mp)
		nr = std::min<uint32_t>(nr, info::mp_selected_rate(m_local_mp, m_peer_mp, false));

	if (nr > V34_MAX_RATE)
		nr = V34_MAX_RATE;

	if (!m_had_mp && nr > peer_hw_cap)
		nr = peer_hw_cap;

	int32_t rc;
	if (nr >= 2 && nr <= 15)
		rc = int32_t((nr - 1) * 2);
	else if (m_had_mp && m_peer_mp.bit_rate_a_to_c > 0)
		rc = (m_peer_mp.bit_rate_a_to_c - 1) * 2;
	else
		rc = 20;

	configure_datamode(rc, m_v34_baud, true);
}

void wtvsoftmodem_v34family::install_data_slicer()
{
	m_qam.set_slicer(
		[this](wtvsoftmodem_qam::csymbol_t point)
		{
			auto decision = m_dm_rx.snap({ point.i * m_rx_data_gain, point.q * m_rx_data_gain });

			return wtvsoftmodem_qam::csymbol_t{ decision.i / m_rx_data_gain, decision.q / m_rx_data_gain };
		}
	);
}

void wtvsoftmodem_v34family::setup_datamode_rx_channel()
{
	const uint32_t peer_hw_cap = (m_got_info1c && m_peer_info1c.rate[m_v34_baud].max_bit_rate > 0) ? (uint32_t)m_peer_info1c.rate[m_v34_baud].max_bit_rate : 15U;

	uint32_t nr = (m_had_mp && m_peer_mp.bit_rate_c_to_a > 0) ? m_peer_mp.bit_rate_c_to_a : std::min<uint32_t>(V34_MAX_RATE, peer_hw_cap);

	if (m_local_mp.bit_rate_c_to_a > 0)
		nr = std::min<uint32_t>(nr, m_local_mp.bit_rate_c_to_a);

	if (!m_had_mp)
		nr = std::min<uint32_t>(nr, peer_hw_cap);

	if (m_had_mp)
		nr = std::min<uint32_t>(nr, info::mp_selected_rate(m_local_mp, m_peer_mp, true));

	if (nr > 15)
		nr = 15;

	if (m_dm_rx_ready && nr == m_dm_rx_cfg_rate)
		return;

	if (m_dm_rx_ready && nr != m_dm_rx_cfg_rate)
		m_rx_frame_locked = false;


	wtvsoftmodem_v34data::config_t cfg;
	cfg.baud_rate_code = m_v34_baud;
	cfg.bit_rate_code  = int32_t(nr - 1) * 2;
	cfg.answer_mode = !m_answer;
	cfg.expanded_shaping = true;
	cfg.point_scale      = SUBC_AMP * std::sqrt(2.0);
	cfg.match_trn_power  = true;
	cfg.invert_output = true;
	const auto precoder = info::mp_precoder(m_local_mp);
	cfg.use_precoder = m_local_mp.type == 1 && m_local_mp.precoding_enabled;
	for (int32_t tap = 0; tap < 3; tap++)
	{
		for (int32_t axis = 0; axis < 2; axis++)
			cfg.precoder_coeffs[tap][axis] = precoder[2 - tap][axis];
	}
	cfg.nonlin_k = m_local_mp.nonlinear_encoder ? 1.0 : 0.0;
	cfg.apply_tx_gains();

	m_dm_rx.init(cfg);

	m_dm_rx_bit_sink = [this](int32_t b)
	{
		if (m_lapm.rx_bit(b != 0))
		{
			m_dm_e_bits = 0;
		}
	};

	m_dm_rx.set_bit_sink(m_dm_rx_bit_sink);
	m_dm_rx_cfg_rate = nr;
	m_dm_rx_ready    = true;
}

void wtvsoftmodem_v34family::ph2_queue_info0a()
{
	info::info0_t in = m_local_info0;
	in.is_answer = m_answer;

	if (!in.v90_raw)
	{
		in.v90_digital = m_v90_mode;

		if (m_pcm.alaw())
			in.v90_extension |= info::INFO0D_ALAW;
		else
			in.v90_extension &= ~info::INFO0D_ALAW;
	}

	in.ack = m_got_info0c;

	m_info0a_bits = info::build_info0(in);
	m_info0a_pos = 0;
}

void wtvsoftmodem_v34family::queue_mp_bits(bool finish_frame)
{
	info::mp_t mp;

	mp.is_answer = m_answer;
	const uint32_t mp_rate = std::min({
		uint32_t(m_mp_rate_cap),
		uint32_t(m_configured_rate_cap),
		BAUD_MAXIMUM[m_v34_baud] 
	});
	mp.type = (mp_rate >= 3) ? 1 : 0;
	mp.bit_rate_c_to_a = uint8_t(mp_rate);
	mp.bit_rate_a_to_c = uint8_t(mp_rate);
	mp.aux_channel = false;
	mp.trellis_sel = 0;
	mp.nonlinear_encoder = m_rx_nonlinear_requested;
	mp.expanded_shaping = true;
	mp.mp_acknowledged  = m_mp_prime || m_got_mp;
	mp.signalling_rate_mask = uint16_t((1U << m_configured_rate_cap) - 1);
	mp.asymmetric_rates = false;

	if (mp.type == 1)
	{
		mp.long_ext = m_rx_precoder_identity ? info::mp_long_ext_identity() : info::mp_long_ext_template();
		mp.precoding_enabled = true;
	}

	m_local_mp   = mp;
	auto bits = info::build_mp(mp);
	if (finish_frame && m_mp_tx_pos != 0)
	{
		m_mp_pending_bits = std::move(bits);
	}
	else
	{
		m_mp_pending_bits.clear();
		m_mp_tx_bits = std::move(bits);
		m_mp_tx_pos = 0;
		if (finish_frame) m_mp_ack_guard_symbols = 0;
	}
}

void wtvsoftmodem_v34family::start()
{
	if (m_v90_mode)
	{
		set_sample_rate(m_sr);
		m_v90_p2_clock = 0;
		m_v90_info1_recovery_clock = 0;
		m_v90_peer_reversal = false;
		m_v90_p2_bits.clear();
		m_v90_zero_run = 0;
		m_v90_snr_cap = 14;
	}
	else
	{
		set_v34_symbol_rate(V34_BAUD_3429, true);
	}

	m_qam.tx_flush_pending();
	m_total_samples = 0;
	m_dm_rx_ready = false;
	m_rx_tracking_pending = false;
	m_rx_training_pending = false;
	m_rx_data_phase = 0;
	m_got_info0c = false;
	m_info0_recovery = false;
	m_info0_finish = false;
	m_peer_info0 = {};
	m_got_info1c = false;
	m_got_info1a = false;
	m_info1c_last_try_sample = 0;
	m_info1c_full_recovery_tried = false;
	m_info1c_recovery_phase = 0;
	m_info1c_valid_sample = 0;
	m_got_mp = false;
	m_had_mp = false;
	m_peer_s_seen = false;
	m_retraining = false;
	m_full_retrain = false;
	m_rt_peer_sbar = false;
	m_rt_pending_peer = false;
	m_v90_retrain16 = false;
	m_rt_periodic_peer = false;
	m_rt_started = 0;
	m_rt_data_since = 0;
	m_rt_downshifts = 0;
	m_rt_upshift_from = 0;
	m_rt_rate_ceiling = 15;
	m_rt_full_requested = false;
	m_rt_pending_full = false;
	m_rt_saved_clocks = false;
	m_rx_j_hold = false;
	m_rt_equalizer_prior.clear();
	m_rt_mp_soft.clear();
	m_rt_mp_phase_valid = false;
	m_rt_phase_corrected = false;
	m_rt_s_detector.reset();
	m_rt_tone_detector.reset(m_sr, m_v90_mode ? 2400 : 1200);
	m_dm_s_persist = 0;
	m_rt_requesting = false;
	m_mp_rate_cap = m_configured_rate_cap;
	m_rt_last_check = 0;
	m_rt_prev_crc_ok = 0;
	m_rt_prev_crc_fail = 0;
	m_rt_good_run = 0;
	m_rt_bad_run = 0;
	m_probe_watch = 0;
	m_ph3_round = 0;
	m_ph3_sym = 0;
	m_jwait_peer_up = 0;
	m_a2_silent = false;
	m_info1c_settle.reset();
	m_l1l2.rx_reset();

	m_mp_prime     = false;
	m_info0c_count = 0;
	m_dpsk_rx_bits.clear();
	m_dpsk_rx_reliable.clear();
	m_mp_rx_bits.clear();

	reset_dpsk_rx();

	enter(m_v90_mode ? PH2_A_INITIAL : PH2_A_INFO);
}

void wtvsoftmodem_v34family::stop()
{
	enter(PH_IDLE);
}

void wtvsoftmodem_v34family::hold_receiver_after_j()
{
	m_qam.rx_finish_training();

	if (m_qam.rx_reference_snr_count() >= V34_TRN_SNR_MIN_SYMBOLS)
		m_phase3_snr = m_qam.rx_reference_snr_db();
	else
		m_phase3_snr = -wtvsoftmodem_dsp::amplitude_to_db(std::max(m_qam.rx_evm(), TRN_EVM_FLOOR));

	double clock;
	if (m_qam.rx_clock_estimate(v34_clock_est_minimum(), clock))
		m_qam.rx_set_clock_frequencies(clock, m_qam.rx_carrier_frequency());

	m_qam.rx_freeze_loops(true);
	m_rx_j_hold = true;
}

void wtvsoftmodem_v34family::narrow_training_rx()
{
	double clock;
	m_trn_narrowed = true;
	if (m_qam.rx_clock_estimate(v34_trn_narrow_symbols(), clock))
		m_qam.rx_set_clock_frequencies(clock, m_qam.rx_carrier_frequency());

	m_qam.rx_set_tracking_rates(V34_DATA_TIMING_BW, V34_TRN_CARRIER_BW, V34_TRN_AGC_RATE);
	m_qam.rx_set_equalizer_rate(V34_DATA_EQ_RATE);
	m_qam.rx_clock_estimate_start(0);
	m_qam.rx_reference_snr_reset();
}

void wtvsoftmodem_v34family::set_data_timing(bool seed)
{
	double clock;

	if (seed && m_qam.rx_clock_estimate(v34_clock_est_minimum(), clock))
	{
		m_qam.rx_set_clock_frequencies(clock, m_qam.rx_carrier_frequency());

		if (m_rt_saved_clocks)
			m_rt_saved_timing = clock;
	}

	m_qam.rx_set_paced_timing(V34_DATA_TIMING_DIVISOR, v34_data_timing_batch());
}

void wtvsoftmodem_v34family::setup_training_rx()
{
	m_qam.rx_use_decision_agc(false);
	m_qam.rx_use_feedback_equalizer(false);
	m_qam.rx_set_equalizer_rate(m_retraining && !m_full_retrain && !m_v90_mode ? V34_DATA_EQ_RATE : V34_TRN_EQ_RATE);
	m_qam.rx_set_tracking_rates(V34_TRN_TIMING_BW, V34_TRN_CARRIER_BW, V34_TRN_AGC_RATE);
	m_trn_narrowed = false;
	m_trn_snr_settled = false;

	if (!m_v90_mode)
		m_qam.rx_clock_estimate_start(v34_clock_est_settle());

	wtvsoftmodem_v34train::config_t peer_config;
	peer_config.answer_mode = m_v90_mode || !m_answer;
	peer_config.trn_scale   = SUBC_AMP;

	wtvsoftmodem_v34train peer_training(peer_config);
	std::vector<wtvsoftmodem_qam::csymbol_t> reference;

	reference.reserve(V34_TRN_REFERENCE_SYMBOLS);
	for (int32_t k = 0; k < int32_t(V34_TRN_REFERENCE_SYMBOLS); k++)
	{
		auto point = (m_retraining && !m_full_retrain && k >= int32_t(m_v34_symbol_rate * 0.5)) ? peer_training.gen_trn((k & 1) != 0, (k & 1) != 0) : peer_training.gen_trn();

		reference.push_back({ point.i, point.q });
	}

	m_qam.rx_acquire_training_reference(reference.data(), uint32_t(reference.size()));
	m_qam.set_slicer(four_point_decision);
}

void wtvsoftmodem_v34family::begin_retrain(std::string_view reason)
{
	m_rt_saved_clocks  = m_rx_frame_locked;
	m_rt_saved_timing  = m_qam.rx_timing_frequency();
	m_rt_saved_carrier = m_qam.rx_carrier_frequency();
	m_rt_mp_soft.clear();
	m_rt_mp_phase_valid = false;
	m_rt_phase_corrected = false;
	m_dm_s_persist = 0;
	m_lapm.retrain();
	m_retraining   = true;
	m_full_retrain = false;
	m_rt_peer_sbar = false;
	m_got_mp = false;
	m_had_mp = false;
	m_mp_prime = false;
	m_peer_mp_ack = false;
	m_peer_e_received = false;
	m_rx_epoch_valid = false;
	m_rx_epoch_symbols = 0;
	m_mp_rx_bits.clear();
	m_peer_mark_run = 0;
	m_rt_started    = m_total_samples;
	m_train.reset();

	if (m_rt_requesting)
		m_rt_s_detector.reset();

	m_ph3_round           = 1;
	m_rx_sync_searching   = false;
	m_rx_frame_locked     = false;
	m_rx_data_tracking    = false;
	m_rx_tracking_pending = false;
	m_rx_training_pending = false;
	m_qam.rx_freeze_loops(true);
	m_dm_rx_ready = false;

	m_qam.tx_flush_pending();

	enter(PH3_S);
}

void wtvsoftmodem_v34family::request_retrain(bool full, std::string_view reason)
{
	if (!is_data_mode() || m_retraining)
		return;

	const std::string cause = "local retrain request: " + std::string(reason);
	m_rt_requesting = true;

	if (full && m_v90_mode && m_pcm.state() == wtvsoftmodem_pcm::PCMState::DATA)
	{
		m_rt_full_requested = true;
		begin_v90_retrain(true, cause);
	}
	else if (full)
	{
		begin_long_retrain(cause);
	}
	else if (m_v90_mode)
	{
		begin_v90_retrain(true, cause);
	}
	else
	{
		begin_retrain(cause);
	}
}

void wtvsoftmodem_v34family::begin_long_retrain(std::string_view reason)
{
	const bool     saved_clocks    = m_rx_frame_locked;
	const auto     equalizer_prior = m_qam.rx_equalizer();
	const double   timing = m_qam.rx_timing_frequency();
	const double   carrier = m_qam.rx_carrier_frequency();
	const uint64_t clock      = m_total_samples;
	const bool     connected  = m_v90_connected;
	const int32_t  rate_cap   = m_mp_rate_cap;
	const bool     requesting = m_rt_requesting;
	const uint32_t upshift_from = m_rt_upshift_from;
	const uint32_t rate_ceiling = m_rt_rate_ceiling;

	m_lapm.retrain();
	start();

	m_total_samples    = clock;
	m_rt_upshift_from  = upshift_from;
	m_rt_rate_ceiling  = rate_ceiling;
	m_rt_saved_clocks  = saved_clocks;
	m_rt_saved_timing  = timing;
	m_rt_saved_carrier = carrier;

	if (saved_clocks)
		m_rt_equalizer_prior = equalizer_prior;

	m_v90_connected = connected;
	m_mp_rate_cap   = rate_cap;
	m_rt_requesting = requesting;
	m_retraining = true;
	m_full_retrain = true;
	m_rt_started = clock;
	m_rx_sync_searching = false;
	m_rx_frame_locked = false;
	m_dm_rx_ready = false;

	m_pcm.stop();

	enter(PH_RETRAIN_SILENCE);
}

void wtvsoftmodem_v34family::begin_v90_retrain(bool requesting, std::string_view reason)
{
	if (m_shared_sample_clock)
		m_qam.rx_hold_timing(false);

	m_lapm.retrain();
	m_pcm.set_upstream_rate_cap(uint32_t(m_mp_rate_cap));

	if (requesting)
	{
		m_rt_s_detector.reset();
		m_pcm.request_retrain();
	}
	else
	{
		m_pcm.observe_upstream_s(false);
	}

	m_retraining = true;
	m_full_retrain = false;
	m_rt_requesting = requesting;
	m_rt_started = m_total_samples;
	m_v90_data_rx = false;
	m_v90_downstream_ready = false;
	m_v90_retrain16 = false;
	m_rx_sync_searching = false;
	m_rx_frame_locked = false;
	m_rx_data_tracking = false;
	m_rx_tracking_pending  = false;
	m_rx_training_pending = true;

	m_v90_upstream_bits.clear();
	m_v90_upstream_bits16.clear();
	m_train.mp_rx_reset();

	if (m_v90_wait_peer_probe)
	{
		m_rx_epoch_valid = false;
		m_peer_mark_run = 0;
		m_v90_upstream_capped = false;
	}
}

void wtvsoftmodem_v34family::check_retrain_quality()
{
	if (m_retraining)
		return;

	if (!m_lapm.connected())
	{
		m_rt_prev_crc_ok   = m_lapm.crc_ok_count();
		m_rt_prev_crc_fail = m_lapm.crc_fail_count();
		m_rt_bad_run = 0;
		m_rt_good_run = 0;

		return;
	}

	const uint32_t ok = m_lapm.crc_ok_count();
	const uint32_t fail = m_lapm.crc_fail_count();
	const uint32_t dok = ok - m_rt_prev_crc_ok;
	const uint32_t dfail = fail - m_rt_prev_crc_fail;

	m_rt_prev_crc_ok     = ok;
	m_rt_prev_crc_fail   = fail;

	if (!m_rx_frame_locked || !m_rx_data_tracking)
		return;

	const double snr = -wtvsoftmodem_dsp::amplitude_to_db(std::max(m_qam.rx_evm(), 1e-6));
	const uint32_t baud = m_v90_mode ? m_v90_upstream_baud : uint32_t(m_v34_baud);
	const bool expanded = !m_v90_mode;
	const bool nonlinear = !m_v90_mode && m_local_mp.nonlinear_encoder;
	const uint32_t sustainable = wtvsoftmodem_v34train::retrain_sustainable_rate(snr, baud, expanded, nonlinear, expanded);
	const uint32_t current = m_v90_mode ? m_v90_upstream_rate : std::max<uint32_t>(2, m_dm_rx_cfg_rate);
	const uint32_t down_rate = m_v90_mode ? sustainable : wtvsoftmodem_v34train::retrain_sustainable_rate(snr + RATE_DOWN_MARGIN_DB, baud, expanded, nonlinear, expanded);

	if (m_rt_upshift_from)
	{
		if (current <= m_rt_upshift_from)
		{
			m_rt_rate_ceiling = current;
		}

		m_rt_upshift_from = 0;
	}

	const uint32_t up_rate = std::min<uint32_t>(
		rx_rate_ceiling(), 
		wtvsoftmodem_v34train::retrain_sustainable_rate(snr - RATE_UP_MARGIN_DB, baud, expanded, nonlinear, expanded)
	);

	const bool impaired = m_v90_mode ? dfail != 0 : wtvsoftmodem_v34train::retrain_bad_window(dok, dfail);

	if (down_rate < current && impaired)
	{
		m_rt_bad_run++;
		m_rt_good_run = 0;
	}
	else if (up_rate > current && !dfail && dok)
	{
		m_rt_good_run++;
		m_rt_bad_run = 0;
	}
	else
	{
		m_rt_bad_run = 0;
		m_rt_good_run = 0;
	}

	if (m_rt_bad_run >= 2)
	{
		m_rt_bad_run = 0;
		m_rt_upshift_from = 0;

		// Escalate to a full retrain
		std::string escalation;
		if (m_rt_downshifts >= MAX_SHORT_DOWNSHIFTS)
			escalation = "short downshift limit reached";
		else if (snr <= FULL_RETRAIN_SNR_DB)
			escalation = std::format("SNR <= {:.2f} dB", FULL_RETRAIN_SNR_DB);
		else if (sustainable < 2)
			escalation = "no sustainable data rate";

		if (!escalation.empty())
		{
			request_retrain(
				true,
				std::format("Bad RX quality: 2 bad windows; SNR {:.2f} dB, failed CRC {}/{}, {}", snr, dfail, dok + dfail, escalation)
			);
		}
		else
		{
			m_rt_downshifts++;
			m_mp_rate_cap = int32_t(std::min(sustainable, current - 1));

			request_retrain(
				false,
				std::format("Bad RX quality: 2 bad windows; SNR {:.2f} dB, failed CRC {}/{}, rate {} -> {} bps", snr, dfail, dok + dfail, current * 2400, m_mp_rate_cap * 2400));
		}
	}
	else if (m_rt_good_run >= 3 && m_total_samples - m_rt_data_since >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(RATE_UPSHIFT_HOLD_MS, m_sr))
	{
		m_rt_good_run = 0;
		m_rt_upshift_from = current;
		m_mp_rate_cap = int32_t(up_rate);

		request_retrain(
			false,
			std::format("Good RX quality: SNR {:.2f} dB, failed CRC {}/{}, rate {} -> {} bps", snr, dfail, dok + dfail, current * 2400, m_mp_rate_cap * 2400)
		);
	}
}

uint32_t wtvsoftmodem_v34family::rx_rate_ceiling() const
{
	uint32_t ceiling = std::min<uint32_t>(uint32_t(m_configured_rate_cap), m_rt_rate_ceiling);

	if (m_v90_mode)
	{
		ceiling = std::min(ceiling, m_v90_upstream_ceiling);
		if (const uint32_t peer = m_pcm.peer_upstream_rate_max())
			ceiling = std::min(ceiling, peer);
	}
	else
	{
		ceiling = std::min(ceiling, BAUD_MAXIMUM[m_v34_baud]);

		if (m_got_info1c && m_peer_info1c.rate[m_v34_baud].max_bit_rate > 0)
			ceiling = std::min<uint32_t>(ceiling, uint32_t(m_peer_info1c.rate[m_v34_baud].max_bit_rate));

		if (m_had_mp && m_peer_mp.bit_rate_c_to_a > 0)
			ceiling = std::min<uint32_t>(ceiling, uint32_t(m_peer_mp.bit_rate_c_to_a));
	}

	return ceiling;
}

uint32_t wtvsoftmodem_v34family::v90_upstream_data_rate() const
{
	uint32_t rate = std::min(m_v90_upstream_ceiling, m_pcm.upstream_rate_cap());
	if (const uint32_t peer = m_pcm.peer_upstream_rate_max())
		rate = std::min(rate, peer);

	return rate;
}

void wtvsoftmodem_v34family::enter(V34FamState new_state)
{
	const V34FamState prev_state = m_state;
	m_state = new_state;
	m_state_samples = 0;

	if (prev_state != new_state && m_state_change_cb)
		m_state_change_cb(prev_state, new_state);

	switch (phase_of(new_state))
	{
		case PHASE_2:
			enter_phase2(new_state);
			break;

		case PHASE_3:
			enter_phase3(new_state);
			break;

		case PHASE_4:
			enter_phase4(new_state, prev_state);
			break;

		case PHASE_FAILED:
		{
			if (m_failed_cb)
				m_failed_cb();
			break;
		}

		default:
			break;
	}
}

void wtvsoftmodem_v34family::enter_phase2(V34FamState new_state)
{
	m_peer_rev_events = 0;
	m_rev_run = 0;
	m_clean_run = 0;
	m_pending_clean_run = 0;

	switch (new_state)
	{
		case PH2_RETRAIN_TONE:
		case PH2_A_INITIAL:
			reset_dpsk_rx();
			break;

		case PH2_A_INFO:
		{
			m_info0_last_try_sample = 0;
			m_info0_recovery_phase = 0;
			ph2_queue_info0a();

			if (m_v90_mode)
				m_dpsk.tx_put_bits(m_info0a_bits.data(), uint32_t(m_info0a_bits.size()));
			break;
		}

		case PH2_NOT_A:
			m_initial_a_reversed = false;
			break;

		case PH2_A_SECOND:
		case PH2_V90_PRE_L1:
		{
			m_dpsk.tx_reversal();
			break;
		}

		case PH2_L1:
		case PH2_L2:
		{
			m_l1l2.start(
				new_state == PH2_L1 ? wtvsoftmodem_v34probe::L1 : wtvsoftmodem_v34probe::L2,
				m_sr,
				false,
				0.5
			);

			if (m_v90_mode && new_state == PH2_L1)
				m_pcm.begin_pcm_probe(m_sr);
			break;
		}

		case PH2_V90_POST_L2:
		{
			m_dpsk.reset();
			break;
		}

		case PH2_POST_L2_A:
		{
			m_probe_watch = 0;
			m_l1l2.detect_reset();
			break;
		}

		case PH2_POST_L2_NOT_A:
		{
			tx_reversal_pause_rx();
			break;
		}

		case PH2_POST_L2_A2:
		{
			tx_reversal_pause_rx();
			m_a2_silent = false;
			m_info1c_settle.reset();
			m_l1l2.detect_reset();
			break;
		}

		case PH2_INFO1:
		{
			m_dpsk.tx_reversal();

			if (!m_v90_mode)
			{
				const int32_t baud = select_v34_baud();
				set_v34_symbol_rate(baud, m_got_info1c ? m_peer_info1c.rate[baud].use_high_carrier : true);
			}

			info::info1a_t ia;
			ia.preemphasis_filter = uint8_t(m_rx_preemphasis >= 0 ? m_rx_preemphasis : m_got_info1c ? m_peer_info1c.rate[m_v34_baud].pre_emphasis : 0);
			ia.max_data_rate      = m_configured_rate_cap;
			ia.baud_rate_a_to_c   = m_v34_baud;
			ia.baud_rate_c_to_a   = m_v34_baud;
			ia.freq_offset        = INFO1_FREQ_OFFSET;
			ia.use_high_carrier   = m_v34_high_carrier;

			m_info1a_bits = info::build_info1a(ia);
			m_info1a_pos  = 0;
			break;
		}

		default:
			break;
	}
}

void wtvsoftmodem_v34family::enter_phase3(V34FamState new_state)
{
	switch (new_state)
	{
		case PH3_S:
		{
			m_ph3_sym = 0;
			if (m_ph3_round == 1)
			{
				m_train.mp_rx_reset();
				m_peer_round2_mark_run = 0;
				m_peer_round2_settle_symbols = 0;
				m_peer_round2_trn_ready = false;
			}
			else if (m_ph3_round == 0)
			{
				start_qam_training();
			}
			break;
		}

		case PH3_SBAR:
		case PH3_PP:
			m_ph3_sym = 0;
			break;

		case PH3_TRN:
		{
			m_ph3_sym = 0;

			bool j16 = (m_last_peer_j_pattern == wtvsoftmodem_v34train::J_PATTERN_16_POINT);

			// NOTE: V.34 11.6: rate renegotiation always uses 4-point constellation (even if @ startup the J pattern/sequence wants 16-point constellation)
			const bool rate_renegotiation = m_retraining && !m_full_retrain;

			m_train.set_tx_training16(!m_v90_mode && m_ph3_round && j16 && !rate_renegotiation);

			m_train.trn_reset();
			break;
		}

		case PH3_J:
		{
			m_train.j_reset();
			m_ph3_sym = 0;
			break;
		}

		case PH3_JWAIT:
		{
			m_peer_s_seen = false;
			m_peer_j_hold_symbols = 0;
			m_peer_j_window = 0;
			m_peer_j_bits = 0;
			m_peer_j_detected = false;
			m_last_peer_j_pattern = 0;
			m_train.mp_rx_reset();
			m_jwait_peer_up = 0;
			m_jwait_fast_pwr = 0.0;
			m_jwait_fast_run = 0;
			m_jwait_fast_on  = false;
			break;
		}

		case PH3_MP:
			enter_ph3_mp();
			break;

		case PH3_E:
			m_train.e_reset();
			m_e_tx_symbols = 0;
			start_datamode_rx_acquisition();
			break;

		default:
			break;
	}
}

void wtvsoftmodem_v34family::start_qam_training()
{
	m_qam.reset();

	if (m_full_retrain && !m_rt_equalizer_prior.empty())
		m_qam.rx_prime_equalizer(m_rt_equalizer_prior);

	m_rx_sync_searching = false;
	m_rx_frame_locked = false;
	m_rx_data_tracking = false;
	m_rx_tracking_pending = false;
	m_rx_training_pending = false;
	m_rx_power_variance = 0.0;
	setup_training_rx();

	m_qam.set_rx_symbol_callback(
		[this](wtvsoftmodem_qam::csymbol_t sym)
		{
			on_qam_symbol(sym);
		}
	);

	m_train.reset();
	m_qam_started = true;

	m_qam.set_tx_post_fir(filter::shaping::resample(shaper_variant(), m_qsr, m_v34_carrier, V34_SHAPER_TAPS, m_v34_baud, m_v34_high_carrier));
}

void wtvsoftmodem_v34family::enter_ph3_mp()
{
	m_qam.rx_freeze_loops(false);

	if (m_full_retrain && m_rt_saved_clocks)
	{
		m_qam.rx_set_clock_frequencies(m_rt_saved_timing, m_rt_saved_carrier);
		m_qam.rx_hold_clock_frequency(true);
	}

	if (m_train.tx_training_bits() == 4 && !m_retraining)
		cap_mp_rate_for_snr(m_phase3_snr, "phase 3 TRN");
	else if (m_qam.rx_reference_snr_count() >= V34_TRN_SNR_MIN_SYMBOLS)
		cap_mp_rate_for_snr(std::min(m_qam.rx_reference_snr_db(), -wtvsoftmodem_dsp::amplitude_to_db(TRN_EVM_FLOOR)), "TRN mean");
	else if (m_qam.rx_reference_trained())
		cap_mp_rate_for_snr(-wtvsoftmodem_dsp::amplitude_to_db(std::max(m_qam.rx_evm(), TRN_EVM_FLOOR)), "TRN EVM");

	queue_mp_bits();

	m_peer_mp_ack = false;
	m_peer_e_received = false;
	m_rx_epoch_valid = false;
	m_rx_epoch_symbols = 0;
	m_peer_mark_run = 0;
	m_mp_ack_guard_symbols = 0;
	m_mp_rx_bits.clear();
	m_got_mp = false;
	m_mp_prime = false;
	m_train.j_reset();
	m_train.mp_rx_reset();
}

void wtvsoftmodem_v34family::enter_phase4(V34FamState new_state, V34FamState prev_state)
{
	switch (new_state)
	{
		case PH4_MPWAIT:
		{
			m_mp_prime = false;
			m_got_mp   = false;
			queue_mp_bits();
			m_mp_rx_bits.clear();
			m_mp2_acked = false;

			m_train.trn_reset();
			m_train.j_reset();
			m_train.mp_rx_reset();

			m_mp2_leadin_left = wtvsoftmodem_dsp::round_nearest<int32_t>(m_v34_symbol_rate * (PH4_MP2_LEADIN_MS / 1000.0));
			break;
		}

		case PH4_DATA:
		{
			enter_data_mode(prev_state);
			break;
		}

		default:
			break;
	}
}

void wtvsoftmodem_v34family::enter_data_mode(V34FamState prev_state)
{
	setup_datamode_rx_loops();

	const bool was_retraining = m_retraining;
	m_rt_data_since = m_total_samples;

	reset_datamode_link(was_retraining, prev_state);
	setup_datamode_rx_channel();
	start_datamode_bit_source(was_retraining);

	if (!was_retraining && m_connected_cb)
		m_connected_cb();
}

void wtvsoftmodem_v34family::setup_datamode_rx_loops()
{
	m_qam.rx_freeze_loops(false);
	if (m_retraining && m_rt_saved_clocks && !m_rx_frame_locked)
		m_qam.rx_hold_clock_frequency(true);

	if (!m_v90_mode)
		m_qam.rx_set_equalizer_rate(V34_DATA_EQ_RATE);

	m_qam.rx_set_tracking_rates(wtvsoftmodem_v34family::data_timing_bw(), V34_DATA_CARRIER_BW, wtvsoftmodem_v34family::data_agc_rate());
	m_qam.rx_hold_timing(false);

	if (!m_v90_mode)
		set_data_timing(true);

	const bool trained = m_qam.rx_reference_aligned();
	m_qam.rx_finish_training();
	if (!trained)
		m_qam.rx_rearm_blind_carrier();
}

void wtvsoftmodem_v34family::reset_datamode_link(bool was_retraining, V34FamState prev_state)
{
	setup_datamode_channel();

	m_datamode.reset();

	if (!was_retraining)
		m_lapm.reset();

	m_rt_prev_crc_ok   = m_lapm.crc_ok_count();
	m_rt_prev_crc_fail = m_lapm.crc_fail_count();

	if (!(prev_state == PH3_E && m_e_tx_symbols == SYM_PH3_E * 2 / m_train.tx_training_bits()))
		m_qam.tx_flush_pending();
}

void wtvsoftmodem_v34family::start_datamode_bit_source(bool was_retraining)
{
	m_dm_e_bits = was_retraining ? m_datamode.b() : int32_t(INITIAL_MARK_FILL_MS / 1000.0 * m_v34_symbol_rate * 8.0);

	m_datamode.set_bit_source(
		[this, adp_started = false]() mutable -> int32_t
		{
			if (m_dm_e_bits > 0)
			{
				m_dm_e_bits--;
				return 1;
			}

			const bool sending_adp = m_lapm.sending_adp();
			if (sending_adp && !adp_started)
				adp_started = true;

			const bool bit = m_lapm.tx_get_bit();

			return bit ? 1 : 0;
		}
	);
}

void wtvsoftmodem_v34family::reset_dpsk_rx()
{
	m_dpsk.reset();
	m_dpsk.set_rx_bit_callback(
		[this](bool b, bool r)
		{
			on_dpsk_bit(b, r);
		}
	);
}

void wtvsoftmodem_v34family::tx_reversal_pause_rx()
{
	m_dpsk.tx_reversal();
	m_dpsk.rx_pause_adaptation(PH2_REVERSAL_RX_PAUSE);
}

void wtvsoftmodem_v34family::cap_mp_rate_for_snr(double snr, std::string_view source)
{
	const uint32_t rate = wtvsoftmodem_v34train::retrain_sustainable_rate(snr, m_v34_baud, true, m_rx_nonlinear_requested, true);
	m_mp_rate_cap = std::min(m_mp_rate_cap, int32_t(std::max(MP_RETRAIN_MIN_RATE, rate)));
}

uint32_t wtvsoftmodem_v34family::tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	uint32_t out = 0;

	switch (phase_of(m_state))
	{
		case PHASE_IDLE:
		case PHASE_RETRAIN:
		case PHASE_FAILED:
			wtvsoftmodem_dsp::clear_samples(sample, sample_count);
			out = sample_count;
			break;

		case PHASE_PCM:
			out = m_pcm.tx(sample, sample_count);
			break;

		case PHASE_2:
			out = tx_phase2(sample, sample_count);
			break;

		default:
		{
			out = tx_qam_symbols(sample, sample_count);

			wtvsoftmodem_dsp::clear_samples(sample, sample_count, out);

			break;
		}
	}

	m_state_samples += sample_count;
	m_total_samples += sample_count;

	tick_timeout();

	return out;
}

uint32_t wtvsoftmodem_v34family::tx_phase2(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	if (m_v90_mode)
	{
		return tx_v90_phase2(sample, sample_count);
	}
	else if (m_state == PH2_L1 || m_state == PH2_L2)
	{
		return m_l1l2.tx(sample, sample_count);
	}
	else
	{
		switch (m_state)
		{
			case PH2_A_INFO:
			{
				while (m_dpsk.tx_pending_bits() < 8)
				{
					if (m_info0a_pos == m_info0a_bits.size())
					{
						if (!m_info0_recovery || m_info0_finish)
							break;

						ph2_queue_info0a();
					}

					m_dpsk.tx_put_bit(m_info0a_bits[m_info0a_pos++] != 0);
				}
				break;
			}

			case PH2_INFO1:
			{
				while (m_info1a_pos < m_info1a_bits.size() && m_dpsk.tx_pending_bits() < 64)
					m_dpsk.tx_put_bit(m_info1a_bits[m_info1a_pos++] != 0);

				if (m_info1a_pos >= m_info1a_bits.size() && m_dpsk.tx_pending_bits() == 0)
				{

					if (!m_got_info1c)
					{
						info::info1c_t ic;
						if (info::parse_info1c(m_dpsk_rx_bits.data(), uint32_t(m_dpsk_rx_bits.size()), ic, nullptr))
						{
							m_got_info1c  = true;
							m_peer_info1c = ic;
						}
					}

					wtvsoftmodem_dsp::clear_samples(sample, sample_count);

					return sample_count;
				}
				break;
			}

			case PH2_POST_L2_A2:
			case PH2_POST_L2_A:
			{
				if (m_a2_silent)
				{
					wtvsoftmodem_dsp::clear_samples(sample, sample_count);

					return sample_count;
				}
				break;
			}

			default:
				break;
		}

		return m_dpsk.tx(sample, sample_count);
	}
}

uint32_t wtvsoftmodem_v34family::tx_qam_symbols(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	const double sps  = m_qsr / m_qam.config().symbol_rate;
	const size_t need = size_t(sample_count / sps) + 2 * size_t(m_qam.config().filter_span_syms) + 6;

	while (m_qam.tx_pending_symbols() < need)
	{
		wtvsoftmodem_qam::csymbol_t sym { 0, 0 };

		switch (m_state)
		{
			case PH3_S:
			{
				const uint64_t lim = m_ph3_round ? SYM_PH3_S2 : SYM_PH3_S;
				if (m_ph3_sym >= lim)
				{
					enter(PH3_SBAR);
					continue;
				}
				else
				{
					auto p = m_train.gen_s(!m_v90_mode || (m_retraining && (!m_full_retrain || m_ph3_round == 1)));

					sym = { p.i, p.q };
					m_ph3_sym++;
				}
				break;
			}

			case PH3_SBAR:
			{
				const uint64_t lim = m_ph3_round ? SYM_PH3_SBAR2 : SYM_PH3_SBAR;
				if (m_ph3_sym >= lim)
				{
					enter(m_ph3_round ? PH3_TRN : PH3_PP);
					continue;
				}
				else
				{
					auto p = m_train.gen_sbar(!m_v90_mode || (m_retraining && (!m_full_retrain || m_ph3_round == 1)));

					sym = { p.i, p.q };
					m_ph3_sym++;
				}
				break;
			}

			case PH3_PP:
			{
				if (m_ph3_sym >= SYM_PH3_PP)
				{
					enter(PH3_TRN);
					continue;
				}
				else
				{
					auto p = m_train.gen_pp();

					sym = { p.i, p.q };
					m_ph3_sym++;
				}
				break;
			}

			case PH3_TRN:
			{
				bool training_done;
				if (m_retraining && m_ph3_round == 1)
				{
					bool peer_ready = (m_full_retrain) ? m_peer_round2_trn_ready : m_rt_peer_sbar;
					training_done = (m_ph3_sym >= uint64_t(m_v34_symbol_rate * 1.5) && peer_ready);
				}
				else if (m_ph3_round)
				{
					const uint64_t minimum = m_train.tx_training_bits() == 4 ? uint64_t(m_v34_symbol_rate * 1.6) : 256;
					training_done = (m_ph3_sym >= minimum && m_peer_round2_trn_ready && m_peer_round2_settle_symbols >= uint64_t(m_v34_symbol_rate * PH3_STAGE_SETTLE_MS / 1000.0));
				}
				else
				{
					training_done = (m_ph3_sym >= SYM_PH3_TRN1);
				}

				if (training_done)
				{
					enter(m_ph3_round ? PH3_MP : PH3_J);

					continue;
				}
				else if (m_retraining && !m_full_retrain && m_ph3_sym >= sym_ph3_trn2_cap())
				{
					begin_long_retrain("local retrain escalation: short retrain TRN timed out");

					continue;
				}
				else
				{
					// V.34 10.1.3.8: TRN scrambles binary ones.
					const auto p = m_train.gen_trn();

					sym = { p.i, p.q };
					m_ph3_sym++;
				}
				break;
			}

			case PH3_J:
			{
				if (m_ph3_sym >= SYM_PH3_J)
				{
					enter(PH3_JWAIT);
					continue;
				}
				else
				{
					auto p = m_train.gen_j();

					sym = { p.i, p.q };
					m_ph3_sym++;
				}
				break;
			}

			case PH3_JWAIT:
			{
				if (!m_v90_mode && !m_jwait_peer_up && m_peer_s_seen && (m_ph3_sym % 8) == 0)
					m_jwait_peer_up = m_total_samples;

				if (!m_v90_mode && !m_jwait_peer_up)
				{
					const auto p = m_train.gen_j();
					sym = {p.i, p.q};
					m_ph3_sym++;
				}
				else
				{
					sym = {0.0, 0.0};
				}
				break;
			}

			case PH4_MPWAIT:
				if (m_mp2_leadin_left > 0)
				{
					m_mp2_leadin_left--;
					auto p = m_train.gen_trn();

					sym = { p.i, p.q };
					break;
				}
				[[fallthrough]];
			case PH3_MP:
			{
				if (m_mp_tx_pos == 0 && !m_mp_pending_bits.empty())
				{
					m_mp_tx_bits = std::move(m_mp_pending_bits);
					m_mp_pending_bits.clear();
					m_mp_ack_guard_symbols = 0;
				}
				if (m_state == PH3_MP && m_mp_prime && (m_peer_mp_ack || m_peer_e_received) && m_mp_ack_guard_symbols >= SYM_MP_ACK_GUARD && m_mp_tx_pos == 0 && (!m_retraining || m_total_samples - m_rt_peer_sbar_sample >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(RETRAIN_SBAR_TO_E_MS, m_sr)))
				{
					enter(PH3_E);
					continue;
				}
				else
				{
					if (m_state == PH3_MP && (m_peer_mp_ack || m_peer_e_received))
						m_mp_ack_guard_symbols++;

					uint8_t bits[4];
					for (uint32_t b = 0; b < m_train.tx_training_bits(); b++)
					{
						bits[b] = m_mp_tx_bits[m_mp_tx_pos++] & 1;

						if (m_mp_tx_pos >= m_mp_tx_bits.size())
							m_mp_tx_pos = 0;
					}

					auto p = m_train.tx_training_bits() == 4 ? m_train.gen_diff16(bits) : m_train.gen_diff4(bits[0], bits[1]);

					sym = { p.i, p.q };
				}
				break;
			}

			case PH3_E:
			{
				if (m_e_tx_symbols == SYM_PH3_E * 2 / m_train.tx_training_bits())
				{
					enter(PH4_DATA);
					continue;
				}
				else
				{
					auto p = m_train.gen_e();

					sym = { p.i, p.q };
					m_e_tx_symbols++;
				}
				break;
			}

			case PH4_DATA:
			{
				auto p = m_datamode.next_symbol();

				sym = { p.i, p.q };
				break;
			}

			default:
				break;
		}

		m_qam.tx_put_symbol(sym);
	}

	return m_qam.tx(sample, sample_count);
}

void wtvsoftmodem_v34family::rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	if (m_v90_mode && phase_of(m_state) == PHASE_2)
	{
		if (m_state == PH2_RETRAIN_TONE)
		{
			for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
				m_rt_periodic_peer |= m_rt_tone_detector.put(sample[sample_idx]);
		}

		m_dpsk.rx(sample, sample_count);
		return;
	}
	else
	{
		if (!m_retraining && m_rx_frame_locked && (m_state == PH4_DATA || phase_of(m_state) == PHASE_PCM))
		{
			for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
			{
				if (m_rt_tone_detector.put(sample[sample_idx]))
				{
					m_rt_requesting = false;
					begin_long_retrain("peer retrain request: retrain tone detected");

					return;
				}
			}
		}

		if (phase_of(m_state) == PHASE_PCM)
		{
			m_qam.rx(sample, sample_count);
			finish_rx_transitions();

			return;
		}

		switch (phase_of(m_state))
		{
			case PHASE_2:
			{
				rx_ph2(sample, sample_count);
				break;
			}

			case PHASE_3:
			case PHASE_4:
			{
				if (m_qam_started)
					m_qam.rx(sample, sample_count);

				for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
				{
					double v = double(sample[sample_idx]) / 32768.0;
					m_rx_pwr = 0.999 * m_rx_pwr + 0.001 * (v * v);
				}

				m_rx_on = m_rx_pwr > RX_ON_POWER;

				if (m_state == PH3_JWAIT && sample_count > 0)
				{
					double sum_sq = 0.0;
					for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
					{
						const double v = double(sample[sample_idx]) / 32768.0;
						sum_sq += v * v;
					}

					m_jwait_fast_pwr = sum_sq / sample_count;

					if (m_jwait_fast_pwr > RX_ON_POWER)
					{
						if (m_jwait_fast_run < JWAIT_FAST_PERSIST)
							m_jwait_fast_run++;
					}
					else
					{
						m_jwait_fast_run = 0;
					}

					m_jwait_fast_on = m_jwait_fast_run >= JWAIT_FAST_PERSIST;

					if (!m_v90_mode && m_jwait_fast_on)
						m_peer_s_seen = true;
				}
				break;
			}

			default:
				break;
		}

		finish_rx_transitions();
	}
}

void wtvsoftmodem_v34family::finish_rx_transitions()
{
	if (m_rt_pending_full)
	{
		m_rt_pending_full = false;
		begin_long_retrain("peer full-retrain response detected");
		return;
	}

	if (m_rt_pending_peer)
	{
		m_rt_pending_peer = false;
		m_rt_requesting   = false;
		begin_retrain("peer retrain request: S sequence detected");
	}

	if (m_rx_training_pending)
	{
		m_rx_training_pending = false;
		if (m_retraining && !m_full_retrain && m_v90_mode)
		{
			m_qam.rx_freeze_loops(false);
			m_qam.rx_finish_training();
			m_qam.rx_set_tracking_rates(data_timing_bw(), V34_DATA_CARRIER_BW, 0);

			if (m_v90_wait_peer_probe)
			{
				m_qam.rx_set_clock_frequencies(0, m_qam.rx_carrier_frequency());
				m_qam.rx_hold_clock_frequency(true);
			}

			m_qam.set_slicer(
				[this](wtvsoftmodem_qam::csymbol_t point)
				{
					if (m_v90_mode && m_v90_retrain16)
					{
						const auto p = wtvsoftmodem_v34train::snap16({ point.i, point.q });
						return wtvsoftmodem_qam::csymbol_t{ p.i, p.q };
					}
					return four_point_decision(point);
				}
			);
		}
		else
		{
			m_qam.rx_freeze_loops(false);
			setup_training_rx();
		}
	}

	if (m_rx_tracking_pending)
	{
		m_rx_tracking_pending = false;
		if (m_retraining && !m_rx_frame_locked && (m_full_retrain || phase_of(m_state) != PHASE_PCM))
		{
			if (m_rt_saved_clocks)
				m_qam.rx_set_clock_frequencies(m_rt_saved_timing, m_rt_saved_carrier);

			if (m_rt_mp_phase_valid && !m_rt_phase_corrected)
			{
				m_qam.rx_correct_carrier_phase(m_rt_mp_phase);
				m_rt_phase_corrected = true;
			}
		}

		m_qam.rx_freeze_loops(false);
		if (m_v90_mode && m_v90_wait_peer_probe)
		{
			m_qam.rx_set_clock_frequencies(0, m_qam.rx_carrier_frequency());
			m_qam.rx_hold_clock_frequency(true);
		}
		else
		{
			m_qam.rx_hold_clock_frequency(m_retraining && m_rt_saved_clocks && !m_rx_frame_locked && (m_full_retrain || phase_of(m_state) != PHASE_PCM));
		}

		if (m_rx_frame_locked && m_rx_data_phase != 0)
		{
			m_qam.rx_correct_carrier_phase(-m_rx_data_phase);
			m_rx_data_phase = 0;
		}

		m_qam.rx_finish_training();

		if (!m_v90_mode)
		{
			m_qam.rx_set_equalizer_rate(V34_DATA_EQ_RATE);
			m_qam.rx_use_decision_agc(m_rx_frame_locked && m_rx_hdlc_acquisition);
			m_qam.rx_use_feedback_equalizer(m_dm_rx.trellis_active());
		}

		m_qam.rx_set_tracking_rates(data_timing_bw(), V34_DATA_CARRIER_BW, phase_of(m_state) == PHASE_PCM ? 0 : data_agc_rate());

		if (!m_v90_mode)
			set_data_timing(false);

		install_data_slicer();
	}
}

void wtvsoftmodem_v34family::rx_ph2(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	if (m_state == PH2_RETRAIN_TONE)
	{
		for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
			m_rt_periodic_peer |= m_rt_tone_detector.put(sample[sample_idx]);
	}

	m_dpsk.rx(sample, sample_count);
	m_l1l2.rx(sample, sample_count, m_sr);

	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double v = double(sample[sample_idx]) / 32768.0;
		m_rx_pwr = 0.999 * m_rx_pwr + 0.001 * (v * v);
	}

	const bool on = m_rx_pwr > RX_ON_POWER;
	if (on != m_rx_on)
	{
		m_rx_on = on;
		m_rx_edge_sample = m_total_samples;
	}
}

void wtvsoftmodem_v34family::on_dpsk_bit(bool bit, bool reliable)
{
	if (m_v90_mode && phase_of(m_state) == PHASE_2)
	{
		rx_v90_bit(bit, reliable);
		return;
	}

	bool tone_stage = (m_state == PH2_NOT_A || m_state == PH2_NOT_A_REV || m_state == PH2_A_INITIAL || m_state == PH2_A_SECOND || m_state == PH2_RETRAIN_TONE || m_state == PH2_POST_L2_A || m_state == PH2_POST_L2_NOT_A || m_state == PH2_POST_L2_A2);
	if (bit)
	{
		if (m_rev_run == 0)
			m_pending_clean_run = m_clean_run;
		m_rev_run++;
		m_clean_run = 0;
	}
	else
	{
		if (tone_stage && m_rev_run >= 1 && m_rev_run <= 6 && m_dpsk.rx_symbol_confident() && m_pending_clean_run >= MIN_CLEAN_RUN_BITS)
		{
			m_peer_rev_events++;
			m_last_rev_sample = m_total_samples;
		}

		m_rev_run = 0;
		m_clean_run++;
	}

	m_dpsk_rx_bits.push_back(bit ? 1 : 0);
	m_dpsk_rx_reliable.push_back(reliable ? 1 : 0);
	if (m_dpsk_rx_bits.size() > 8192)
	{
		m_dpsk_rx_bits.erase(m_dpsk_rx_bits.begin(), m_dpsk_rx_bits.begin() + 4096);
		m_dpsk_rx_reliable.erase(m_dpsk_rx_reliable.begin(), m_dpsk_rx_reliable.begin() + 4096);
	}

	if (m_state == PH2_POST_L2_A2 && !m_got_info1c && m_dpsk_rx_bits.size() >= 109)
	{
		info::info1c_t ic;
		if (info::parse_info1c(m_dpsk_rx_bits.data(), uint32_t(m_dpsk_rx_bits.size()), ic))
		{
			m_got_info1c  = true;
			m_peer_info1c = ic;
		}
	}

	if (m_dpsk_rx_bits.size() >= 40)
	{
		info::info0_t got;
		uint32_t consumed = 0;
		if (info::parse_info0(m_dpsk_rx_bits.data(), uint32_t(m_dpsk_rx_bits.size()), got, &consumed))
		{
			m_dpsk_rx_bits.erase(m_dpsk_rx_bits.begin(), m_dpsk_rx_bits.begin() + consumed);
			m_dpsk_rx_reliable.erase(m_dpsk_rx_reliable.begin(), m_dpsk_rx_reliable.begin() + consumed);
			accept_peer_info0(got);
		}
	}
}

void wtvsoftmodem_v34family::accept_peer_info0(info::info0_t got)
{
	got.is_answer = !m_answer;
	m_peer_info0 = got;
	m_info0c_count++;
	m_got_info0c = true;
	if (m_state == PH2_A_INFO && m_info0c_count >= 2 && !m_info0_recovery)
		m_info0_recovery = true;
}

void wtvsoftmodem_v34family::recover_initial_info0()
{
	if (m_got_info0c || !state_elapsed(150.0) ||
		!state_elapsed_since(m_info0_last_try_sample, 25.0))
		return;

	m_info0_last_try_sample = m_state_samples;

	for (unsigned candidate = 0; candidate < 4; candidate++)
	{
		const unsigned phase = m_info0_recovery_phase;
		m_info0_recovery_phase = (phase + 1) % 64;
		const auto bits = m_dpsk.rx_decode_buffered(0.0, double(phase) / 64.0);

		info::info0_t got;
		if (!info::parse_info0(bits.data(), uint32_t(bits.size()), got))
			continue;

		accept_peer_info0(got);
		return;
	}
}

void wtvsoftmodem_v34family::recover_v90_info0()
{
	if (m_got_info0c || !state_elapsed_since(m_info0_last_try_sample, 25.0))
		return;

	m_info0_last_try_sample = m_state_samples;
	for (unsigned candidate = 0; candidate < 4; candidate++)
	{
		const unsigned phase = m_info0_recovery_phase;
		m_info0_recovery_phase = (phase + 1) % 64;

		const auto bits = m_dpsk.rx_decode_buffered(0.0, double(phase) / 64.0);
		info::info0_t peer;
		if (!info::parse_info0(bits.data(), uint32_t(bits.size()), peer))
			continue;

		peer.is_answer = !m_answer;
		m_peer_info0 = peer;
		m_got_info0c = true;
		m_v90_p2_bits.clear();
		return;
	}
}

void wtvsoftmodem_v34family::start_datamode_rx_acquisition()
{
	if (m_retraining && !m_peer_e_received)
		return;

	if (m_rx_sync_searching || m_rx_frame_locked)
		return;

	setup_datamode_rx_channel();

	m_rx_hdlc_acquisition = m_retraining && m_lapm.connected();

	if (m_full_retrain)
		m_rx_data_gain = 1.0;

	const double gain = m_rx_hdlc_acquisition ? m_rx_data_gain : 1.0;
	const int32_t phase_steps = (m_full_retrain || (m_rx_hdlc_acquisition && !m_v90_mode)) ? 1 : (!m_retraining && !m_v90_mode ? 2 : 0);

	auto search_cfg = m_dm_rx.config();

	if (m_v90_mode && !m_rx_hdlc_acquisition)
		search_cfg.nonlin_k = 0.0;

	m_rx_sync.init(search_cfg, true, m_rx_hdlc_acquisition, gain, 12, phase_steps);

	m_rx_sync.set_bit_sink(m_dm_rx_bit_sink);

	m_rx_sync_searching   = true;
	m_rx_frame_locked     = false;
	m_rx_tracking_pending = !m_retraining || m_peer_e_received;
}

void wtvsoftmodem_v34family::receive_datamode_symbol(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_rx_epoch_valid)
		m_rx_epoch_symbols++;

	if (m_rx_sync_searching)
	{
		const double c = std::cos(m_rx_data_phase);
		const double s = std::sin(m_rx_data_phase);
		m_dm_rx.observe_front_end({ (sym.i * c - sym.q * s) * m_rx_data_gain, (sym.i * s + sym.q * c) * m_rx_data_gain });

		m_rx_sync.put_symbol({ sym.i, sym.q });
		if (m_rx_sync.locked())
		{
			m_rx_data_gain  = m_rx_sync.gain();
			m_rx_data_phase = m_rx_sync.carrier_phase();
			if (phase_of(m_state) == PHASE_PCM)
			{
				m_rx_data_gain = m_rx_sync.gain();
				m_qam.rx_set_tracking_rates(data_timing_bw(), V34_DATA_CARRIER_BW, 0);
			}

			auto decoder = m_rx_sync.take_decoder();
			m_dm_rx = std::move(*decoder);
			m_dm_rx.set_exact_demap(true);
			if ((!m_v90_mode || m_v90_wait_peer_probe) && m_rx_hdlc_acquisition && m_rx_epoch_valid)
			{
				if (m_dm_rx.enable_trellis(m_rx_epoch_symbols))
				{
					m_dm_rx.set_symbol_reference_sink(
						[this](wtvsoftmodem_v34data::point_t point, uint64_t epoch)
						{
							if (!m_rx_epoch_valid || epoch >= m_rx_epoch_symbols || m_rx_data_gain <= 0)
								return;

							const uint64_t age = m_rx_epoch_symbols - 1 - epoch;
							if (age >= 128)
								return;

							const double c = std::cos(m_rx_data_phase);
							const double s = std::sin(m_rx_data_phase);

							const wtvsoftmodem_qam::csymbol_t reference { (point.i * c + point.q * s) / m_rx_data_gain, (point.q * c - point.i * s) / m_rx_data_gain };

							m_qam.rx_feedback_equalizer(reference, uint32_t(age));
						}
					);
				}
			}

			m_rx_sync_searching = false;
			m_rx_frame_locked = true;
			m_rx_tracking_pending = true;
		}
	}
	else if (m_rx_frame_locked)
	{
		const double gain = m_rx_data_gain;
		const double c = std::cos(m_rx_data_phase);
		const double s = std::sin(m_rx_data_phase);

		m_dm_rx.put_symbol({ (sym.i * c - sym.q * s) * gain, (sym.i * s + sym.q * c) * gain });

		const bool epoch_known = (!m_v90_mode || (m_v90_wait_peer_probe && m_rx_epoch_valid));
		const bool link_establishing = !m_lapm.waiting_for_odp() && !m_lapm.connected();

		if (epoch_known && link_establishing && !m_rx_hdlc_acquisition)
		{
			m_rx_sync.init(m_dm_rx.config(), true, true, 1.0, 12, 1);
			m_rx_sync.set_bit_sink(m_dm_rx_bit_sink);
			m_rx_sync_searching = true;
			m_rx_frame_locked = false;
			m_rx_hdlc_acquisition = true;
		}
	}
}

void wtvsoftmodem_v34family::on_qam_symbol(wtvsoftmodem_qam::csymbol_t sym)
{
	rx_watch_short_retrain_sbar(sym);
	rx_watch_round2_trn(sym);
	rx_watch_data_variance(sym);

	switch (m_state)
	{
		case PH3_S:
		case PH3_SBAR:
		case PH3_PP:
		{
			m_det = m_train.detect({ sym.i, sym.q });
			if (m_det.s_present)
				m_peer_s_seen = true;
			break;
		}

		case PH3_JWAIT:
		{
			rx_phase3_jwait(sym);
			break;
		}

		case PH3_TRN:
		{
			if (m_retraining && !m_full_retrain && m_rt_peer_sbar)
				rx_mp_symbol(sym);
			break;
		}

		case PH3_MP:
		case PH4_MPWAIT:
		{
			rx_mp_symbol(sym);
			break;
		}

		case PH3_E:
		{
			if (!detect_peer_e(sym))
				receive_datamode_symbol(sym);
			break;
		}

		case PH4_DATA:
		{
			if (detect_peer_e(sym))
				break;

			if (!m_retraining && m_rt_s_detector.put(sym.i, sym.q) == wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::S && !m_rt_pending_peer)
			{
				m_rt_pending_peer = true;
				break;
			}

			if (m_dm_rx_ready)
				receive_datamode_symbol(sym);
			break;
		}

		default:
			break;
	}
}

void wtvsoftmodem_v34family::rx_watch_short_retrain_sbar(wtvsoftmodem_qam::csymbol_t sym)
{
	if (!m_retraining || m_full_retrain || m_rt_peer_sbar)
		return;

	if (m_rt_s_detector.put(sym.i, sym.q) == wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::SBAR)
	{
		note_peer_sbar();
		m_train.mp_rx_reset();
		m_rx_training_pending = true;
	}
}

void wtvsoftmodem_v34family::rx_watch_round2_trn(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_ph3_round != 1 || (m_state != PH3_S && m_state != PH3_SBAR && m_state != PH3_TRN))
		return;

	if (m_peer_round2_trn_ready)
	{
		m_peer_round2_settle_symbols++;
		return;
	}

	uint8_t bits[2];
	detect_diff4_marks(sym, bits, m_peer_round2_mark_run);

	if (m_peer_round2_mark_run >= ROUND2_TRN_MARK_RUN && m_qam.rx_evm() < wtvsoftmodem_dsp::db_to_amplitude(ROUND2_TRN_EVM_DB))
	{
		m_peer_round2_trn_ready = true;

		if (m_rx_j_hold)
		{
			m_rx_j_hold = false;
			m_qam.rx_freeze_loops(false);
			m_qam.rx_hold_timing(true);
		}

		if (m_full_retrain)
			note_peer_sbar();
	}
}

void wtvsoftmodem_v34family::rx_watch_data_variance(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_retraining || !(m_rx_sync_searching || m_rx_frame_locked) || m_rx_data_tracking)
		return;

	if (update_rx_power_variance(sym))
	{
		m_rx_data_tracking = true;
		m_peer_e_received = true;
		m_rx_tracking_pending = true;
	}
}

bool wtvsoftmodem_v34family::update_rx_power_variance(wtvsoftmodem_qam::csymbol_t sym)
{
	const double power  = sym.i * sym.i + sym.q * sym.q;
	m_rx_power_variance = RX_POWER_VARIANCE_DECAY * m_rx_power_variance + RX_POWER_VARIANCE_GAIN * (power - 1.0) * (power - 1.0);

	return m_rx_power_variance > RX_DATA_VARIANCE;
}

void wtvsoftmodem_v34family::rx_phase3_jwait(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_peer_j_detected)
	{
		m_peer_j_hold_symbols++;
		return;
	}

	if (!m_qam.rx_reference_trained())
		return;

	uint8_t bits[2];
	m_train.detect_diff4({ sym.i, sym.q }, bits);

	for (uint8_t bit : bits)
	{
		if (const uint16_t pattern = detect_peer_j(bit))
		{
			m_peer_j_detected = true;
			break;
		}
	}
}

void wtvsoftmodem_v34family::rx_mp_symbol(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_rx_sync_searching || m_rx_frame_locked)
		receive_datamode_symbol(sym);

	if (m_peer_e_received)
		return;

	uint8_t bits[2];
	detect_diff4_marks(sym, bits, m_peer_mark_run);

	if (m_got_mp && m_peer_mark_run >= E_MARK_RUN)
	{
		accept_peer_e();
		return;
	}

	if (keep_retrain_mp_soft())
	{
		m_rt_mp_soft.push_back(sym);
		if (m_rt_mp_soft.size() > MP_RETRAIN_SOFT_POINTS)
			m_rt_mp_soft.pop_front();
	}

	m_mp_rx_bits.append(bits, 2);
	if (m_mp_rx_bits.bits.size() > MP_RX_MAX_BITS)
		m_mp_rx_bits.drop_front(MP_RX_TRIM_BITS);

	if (m_mp_rx_bits.bits.size() < MP_RX_MIN_BITS)
		return;

	info::mp_t mp;
	uint32_t   consumed = 0;
	if (!info::parse_mp(m_mp_rx_bits.bits.data(), uint32_t(m_mp_rx_bits.bits.size()), mp, &consumed, &m_mp_rx_bits.resume))
		return;

	m_mp_rx_bits.drop_front(consumed);

	if (!info::mp_selected_rate(m_local_mp, mp, true) || !info::mp_selected_rate(m_local_mp, mp, false))
		return;

	accept_peer_mp(mp);
}

void wtvsoftmodem_v34family::accept_peer_mp(const info::mp_t &mp)
{
	if (mp.mp_acknowledged)
		m_peer_mp_ack = true;

	const bool first_mp = !m_got_mp;
	m_got_mp = true;
	m_had_mp = true;
	m_peer_mp = mp;

	if (keep_retrain_mp_soft() && m_rt_mp_soft.size() == MP_RETRAIN_SOFT_POINTS)
		estimate_retrain_mp_channel(first_mp);

	setup_datamode_rx_channel();
	start_datamode_rx_acquisition();
}

void wtvsoftmodem_v34family::estimate_retrain_mp_channel(bool first_mp)
{
	std::complex<double> correlation {};
	for (const auto &point : m_rt_mp_soft)
	{
		const auto decision = four_point_decision(point);
		correlation += std::complex<double>(point.i, point.q) * std::conj(std::complex<double>(decision.i, decision.q));
	}

	m_rt_mp_phase = std::arg(correlation);
	m_rt_mp_phase_valid = true;

	if (!first_mp || !m_full_retrain)
		return;

	const auto channel = correlation / double(m_rt_mp_soft.size());
	double error = 0;
	double signal = 0;
	for (const auto &point : m_rt_mp_soft)
	{
		const auto nearest = four_point_decision(point);
		const std::complex<double> decision(nearest.i, nearest.q);

		error += std::norm(std::complex<double>(point.i, point.q) - channel * decision);
		signal += std::norm(channel * decision);
	}

	cap_mp_rate_for_snr(wtvsoftmodem_dsp::power_to_db(signal / std::max(error, 1e-12)), "retrain MP");
}

void wtvsoftmodem_v34family::detect_diff4_marks(wtvsoftmodem_qam::csymbol_t sym, uint8_t (&bits)[2], uint32_t &run)
{
	m_train.detect_diff4({ sym.i, sym.q }, bits);
	for (uint8_t bit : bits)
		run = bit ? run + 1 : 0;
}

void wtvsoftmodem_v34family::note_peer_sbar()
{
	m_rt_peer_sbar = true;
	m_rt_peer_sbar_sample = m_total_samples;
}

void wtvsoftmodem_v34family::accept_peer_e()
{
	m_peer_e_received = true;
	m_rx_epoch_valid  = true;
	m_rx_epoch_symbols = 0;
	m_rx_data_tracking = true;
	m_rx_tracking_pending = true;

	start_datamode_rx_acquisition();
}

uint16_t wtvsoftmodem_v34family::detect_peer_j(bool bit)
{
	m_peer_j_window = (m_peer_j_window >> 1) | (uint32_t(bit) << 31);

	if (m_peer_j_bits < 32)
		m_peer_j_bits++;

	if (m_peer_j_bits < 32)
		return 0;

	for (const auto &[window, pattern] : PEER_J_WINDOWS)
	{
		if (m_peer_j_window == window)
		{
			m_last_peer_j_pattern = pattern;

			return pattern;
		}
	}

	return 0;
}

bool wtvsoftmodem_v34family::detect_peer_e(wtvsoftmodem_qam::csymbol_t sym)
{
	if (m_peer_e_received || !m_got_mp)
		return false;

	uint8_t bits[2];
	m_train.detect_diff4({ sym.i, sym.q }, bits);

	for (int32_t bit : bits)
		m_peer_mark_run = bit ? m_peer_mark_run + 1 : 0;

	if (m_peer_mark_run < E_MARK_RUN)
		return false;

	accept_peer_e();

	return true;
}

void wtvsoftmodem_v34family::finish_retrain()
{
	m_rt_data_since = m_total_samples;
	m_rt_good_run = 0;
	m_rt_bad_run = 0;
	m_retraining = false;
	m_full_retrain = false;
	m_rt_requesting = false;
	m_rt_full_requested = false;
	m_rt_pending_full = false;
	m_rt_s_detector.reset();
	m_rt_tone_detector.reset(m_sr, m_v90_mode ? 2400 : 1200);
	m_rt_last_check = m_state_samples;
	m_rt_prev_crc_ok = m_lapm.crc_ok_count();
	m_rt_prev_crc_fail = m_lapm.crc_fail_count();
}

void wtvsoftmodem_v34family::transmit_byte(uint8_t b)
{
	m_lapm.tx_put_byte(b);
}

inline void wtvsoftmodem_v34family::tick_timeout()
{
	if (tick_timeout_retrain())
		return;

	switch (phase_of(m_state))
	{
		case PHASE_PCM:
			tick_timeout_pcm();
			break;

		case PHASE_2:
			if (!m_v90_mode)
				tick_timeout_phase2();
			break;

		case PHASE_3:
			tick_timeout_phase3();
			break;

		case PHASE_4:
			tick_timeout_phase4();
			break;

		default:
			break;
	}
}

inline bool wtvsoftmodem_v34family::tick_timeout_retrain()
{
	if (m_state == PH_RETRAIN_SILENCE && state_elapsed(RETRAIN_SILENCE_MS))
	{
		enter(PH2_RETRAIN_TONE);

		if (m_v90_mode)
			m_v90_p2_clock = 0;
	}

	const double short_timeout = (m_v90_mode && m_v90_wait_peer_probe) ? V90_RETRAIN_SHORT_TIMEOUT_MS : RETRAIN_SHORT_TIMEOUT_MS;
	if (m_retraining && m_total_samples - m_rt_started > wtvsoftmodem_dsp::ms_to_samples<uint64_t>(m_full_retrain ? RETRAIN_FULL_TIMEOUT_MS : short_timeout, m_sr))
	{
		if (m_full_retrain)
			enter(PH_FAILED);
		else
			begin_long_retrain("local escalation: short retrain timed out");

		return true;
	}

	if (m_state == PH4_DATA && m_retraining && m_rx_frame_locked)
		finish_retrain();

	return false;
}

inline void wtvsoftmodem_v34family::tick_timeout_pcm()
{
	if (m_v90_downstream_ready && m_rx_frame_locked && m_retraining)
		finish_retrain();

	if (m_pcm.state() == wtvsoftmodem_pcm::PCMState::DATA)
		tick_timeout_retrain_quality();
}

inline void wtvsoftmodem_v34family::tick_timeout_retrain_quality()
{
	if (!m_retraining && state_elapsed_since(m_rt_last_check, RETRAIN_CHECK_INTERVAL_MS))
	{
		m_rt_last_check = m_state_samples;
		check_retrain_quality();
	}
}

inline void wtvsoftmodem_v34family::tick_timeout_phase2()
{
	switch (m_state)
	{
		case PH2_RETRAIN_TONE:
		{
			if (m_rt_periodic_peer && state_elapsed(RETRAIN_TONE_MIN_MS))
				enter(PH2_NOT_A);
			else if (state_elapsed(RETRAIN_TONE_MAX_MS))
				enter(PH_FAILED);
			break;
		}

		case PH2_A_INITIAL:
		{
			if (state_elapsed(PH2_A_INITIAL_MS))
				enter(PH2_A_INFO);
			break;
		}

		case PH2_A_INFO:
		{
			recover_initial_info0();

			if (m_got_info0c && (m_peer_info0.ack || m_clean_run >= 30))
				m_info0_finish = true;

			if (m_info0_finish && m_info0a_pos == m_info0a_bits.size() &&
				m_dpsk.tx_pending_bits() == 0 && state_elapsed(PH2_A_INITIAL_MS))
			{
				enter(PH2_NOT_A);
			}
			else if (state_elapsed(5000.0))
			{
				enter(PH_FAILED);
			}
			break;
		}

		case PH2_NOT_A:
		{
			if (!m_initial_a_reversed && state_elapsed(50.0))
			{
				m_dpsk.tx_reversal();
				m_initial_a_reversed = true;
				m_peer_rev_events = 0;
			}

			if (m_initial_a_reversed && (m_peer_rev_events >= 1 || state_elapsed(PH2_NOT_A_MAX_MS)))
				enter(PH2_NOT_A_REV);
			break;
		}

		case PH2_NOT_A_REV:
		{
			if (state_elapsed(PH2_NOT_A_REV_MS))
				enter(PH2_A_SECOND);
			break;
		}

		case PH2_A_SECOND:
		{
			if (state_elapsed(PH2_A_SECOND_MS))
				enter(PH2_L1);
			break;
		}

		case PH2_L1:
		{
			if (state_elapsed(wtvsoftmodem_v34probe::L1_MS))
				enter(PH2_L2);
			break;
		}

		case PH2_L2:
		{
			if (state_elapsed(wtvsoftmodem_v34probe::L2_MS))
				enter(PH2_POST_L2_A);
			break;
		}

		case PH2_POST_L2_A:
		{
			m_a2_silent = m_l1l2.detect(m_rx_on, m_rx_pwr, m_state_samples, m_sr, true);

			if (m_rx_on && !m_a2_silent && m_probe_watch == 0 && state_elapsed(PH2_POST_A_MIN_MS))
			{
				m_probe_watch = 1;
				m_a2_last_rev_sample = m_state_samples;
			}

			if ((m_probe_watch == 1 && state_elapsed_since(m_a2_last_rev_sample, PH2_POST_A_WATCH_MS)) || state_elapsed(PH2_POST_A_MAX_MS))
				enter(PH2_POST_L2_NOT_A);
			break;
		}

		case PH2_POST_L2_NOT_A:
		{
			if (state_elapsed(PH2_POST_NOT_A_MS))
				enter(PH2_POST_L2_A2);
			break;
		}

		case PH2_POST_L2_A2:
		{
			const bool peer_probing_now = m_l1l2.detect(m_rx_on, m_rx_pwr, m_state_samples, m_sr, false);
			const bool settled = m_info1c_settle.update(m_dpsk_rx_reliable, m_state_samples, m_l1l2.peer_probe_ended(), m_l1l2.peer_probe_end_sample(), m_sr);
			const bool want_info1a = settled && !m_got_info1c;
			const bool time_for_periodic_try = state_elapsed_since(m_info1c_last_try_sample, INFO1C_RETRY_MS);
			const bool near_cap = state_elapsed(PH2_A2_CAP_MS - PH2_A2_RECOVERY_LEAD_MS);
			const bool run_full_recovery = want_info1a || (near_cap && (!m_info1c_full_recovery_tried || m_info1c_recovery_phase != 0));

			if (!m_got_info1c && m_dpsk_rx_bits.size() >= INFO1C_BITS && time_for_periodic_try)
			{
				m_info1c_last_try_sample = m_state_samples;

				if (run_full_recovery)
					m_info1c_full_recovery_tried = true;

				if (const auto ic = decode_info1c(run_full_recovery))
				{
					m_got_info1c  = true;
					m_peer_info1c = *ic;
				}
			}

			if (m_got_info1c && m_info1c_valid_sample == 0)
				m_info1c_valid_sample = m_state_samples;

			m_a2_silent = peer_probing_now;

			if (m_got_info1c ? state_elapsed_since(m_info1c_valid_sample, INFO1C_TO_INFO1A_MS) : state_elapsed(PH2_A2_CAP_MS))
				enter(PH2_INFO1);
			break;
		}

		case PH2_INFO1:
		{
			if (state_elapsed(1000.0 * m_info1a_bits.size() / m_dpsk.tx_baud_rate() + PH2_INFO1_FLUSH_MS))
			{
				m_ph3_round = 0;
				enter(PH3_S);
			}
			break;
		}

		default:
			break;
	}
}

inline std::optional<info::info1c_t> wtvsoftmodem_v34family::decode_info1c(bool full_recovery)
{
	info::info1c_t ic;

	if (info::parse_info1c(m_dpsk_rx_bits.data(), uint32_t(m_dpsk_rx_bits.size()), ic, nullptr))
		return ic;

	if (!full_recovery)
		return std::nullopt;

	std::vector<uint8_t> compact;
	compact.reserve(m_dpsk_rx_bits.size());

	for (size_t i = 0; i < m_dpsk_rx_bits.size(); i++)
	{
		if (m_dpsk_rx_reliable[i])
			compact.push_back(m_dpsk_rx_bits[i]);
	}

	if (info::parse_info1c(compact.data(), uint32_t(compact.size()), ic, nullptr))
		return ic;

	if (info::parse_info1c_erasure(m_dpsk_rx_bits.data(), m_dpsk_rx_reliable.data(), uint32_t(m_dpsk_rx_bits.size()), ic, nullptr))
		return ic;

	for (uint32_t candidate = 0; candidate < INFO1C_RECOVERY_CANDIDATES; candidate++)
	{
		const double phase      = double(m_info1c_recovery_phase) / INFO1C_RECOVERY_PHASES;
		m_info1c_recovery_phase = (m_info1c_recovery_phase + 1) % INFO1C_RECOVERY_PHASES;

		const std::vector<uint8_t> bits = m_dpsk.rx_decode_buffered(0.0, phase);

		if (info::parse_info1c(bits.data(), uint32_t(bits.size()), ic, nullptr))
			return ic;
	}

	return std::nullopt;
}

inline void wtvsoftmodem_v34family::tick_timeout_phase3()
{
	switch (m_state)
	{
		case PH3_TRN:
		{
			if (m_ph3_round == 1 && state_elapsed(m_phase3_timeout_ms))
				handle_phase3_timeout();
			break;
		}

		case PH3_JWAIT:
		{
			if (!m_trn_narrowed && (!m_retraining || m_full_retrain) && !m_v90_mode && m_qam.rx_reference_trained() && m_qam.rx_clock_estimate_count() >= v34_trn_narrow_symbols())
			{
				narrow_training_rx();
			}
			else if (m_trn_narrowed && !m_trn_snr_settled && m_qam.rx_clock_estimate_count() >= V34_TRN_SNR_SETTLE)
			{
				m_trn_snr_settled = true;
				m_qam.rx_reference_snr_reset();
			}

			if (m_peer_j_detected && m_peer_j_hold_symbols >= sym_ph3_jwait_hold())
			{
				m_ph3_round = 1;
				if ((!m_retraining || m_full_retrain) && !m_v90_mode)
					hold_receiver_after_j();

				enter(PH3_S);
			}
			else if (state_elapsed(m_phase3_timeout_ms))
			{
				handle_phase3_timeout();
			}
			break;
		}

		case PH3_MP:
		{
			if (!m_mp_prime && m_got_mp)
			{
				m_mp_prime = true;
				queue_mp_bits(true);
			}
			break;
		}

		default:
			break;
	}
}

inline void wtvsoftmodem_v34family::handle_phase3_timeout()
{
	if (m_retraining)
		enter(PH_FAILED);
	else
		start();
}

inline void wtvsoftmodem_v34family::tick_timeout_phase4()
{
	switch (m_state)
	{
		case PH4_MPWAIT:
			if (!m_mp2_acked && m_got_mp)
			{
				m_mp2_acked      = true;
				m_mp2_ack_sample = m_state_samples;
				queue_mp_bits(true);
			}
			if ((m_mp2_acked && state_elapsed_since(m_mp2_ack_sample, PH4_MP_ACK_HOLD_MS)) || state_elapsed(PH4_MPWAIT_MAX_MS))
				enter(PH4_DATA);
			break;

		case PH4_DATA:
			tick_timeout_retrain_quality();
			break;

		default:
			break;
	}
}

void wtvsoftmodem_v34family::rx_v90_bit(bool bit, bool reliable)
{

	if (m_state == PH2_RETRAIN_TONE && bit)
	{
		if (m_rt_periodic_peer)
			m_v90_peer_reversal = true;
	}

	m_v90_p2_bits.push_back(bit ? 1 : 0);
	const uint32_t zero_run = m_v90_zero_run;
	m_v90_zero_run = bit ? 0 : m_v90_zero_run + 1;
	if (m_v90_p2_bits.size() > 4096)
		m_v90_p2_bits.erase(m_v90_p2_bits.begin(), m_v90_p2_bits.begin() + 2048);

	if (m_state == PH2_A_INFO)
	{
		info::info0_t peer;
		if (!m_got_info0c && info::parse_info0(m_v90_p2_bits.data(), uint32_t(m_v90_p2_bits.size()), peer))
		{
			peer.is_answer = !m_answer;
			m_peer_info0 = peer;
			m_got_info0c = true;
			m_v90_p2_bits.clear();
		}

		const bool tone_reversal = bit && zero_run >= V90_REVERSAL_ZERO_RUN;
		if (m_got_info0c && (reliable || tone_reversal) && bit && m_v90_p2_clock > uint64_t(m_sr * 0.2))
			m_v90_peer_reversal = true;
	}

	if (m_state == PH2_A_SECOND)
	{
		if (m_v90_wait_peer_probe)
		{
			m_v90_recent_bits = (m_v90_recent_bits << 1) | (bit ? 1 : 0);
			if (std::popcount(m_v90_recent_bits & V90_PROBE_WINDOW_MASK) >= V90_PROBE_MIN_ONES)
				m_v90_peer_probe_seen = true;

			if (m_v90_peer_probe_seen && bit && zero_run >= V90_REVERSAL_ZERO_RUN)
				m_v90_peer_reversal = true;
		}
		else if (bit)
		{
			m_v90_peer_reversal = v90_a_second_clock() > v90_a_second_samples(V90_A_SECOND_REV_RETRAIN_MS, V90_A_SECOND_REV_MS);
		}
	}

	if (m_state == PH2_V90_INFO1A_WAIT)
	{
		info::info1a_t peer;
		if (info::parse_info1a(m_v90_p2_bits.data(), uint32_t(m_v90_p2_bits.size()), peer))
			accept_v90_info1(peer);
	}
}

void wtvsoftmodem_v34family::pcm_state_changed(wtvsoftmodem_pcm::PCMState new_state)
{
	if (phase_of(m_state) != PHASE_PCM || new_state == wtvsoftmodem_pcm::PCMState::IDLE)
		return;

	if (new_state == wtvsoftmodem_pcm::PCMState::FAILED)
	{
		enter(PH_FAILED);
	}
	else
	{
		const V34FamState prev_state = m_state;
		m_state = pcm_to_state(new_state);

		if (prev_state != m_state && m_state_change_cb)
			m_state_change_cb(prev_state, m_state);
	}
}

void wtvsoftmodem_v34family::accept_v90_info1(const info::info1a_t &peer)
{
	m_peer_info1a = peer;
	m_got_info1a = true;

	if (peer.pcm_downstream)
	{
		m_pcm.start(m_sr, peer.training_ucode);
		setup_v90_upstream(peer);
		enter(pcm_to_state(m_pcm.state()));
	}
	else
	{
		enter(PH_FAILED);
	}
}

uint32_t wtvsoftmodem_v34family::tx_v90_phase2(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	const double elapsed = double(m_state_samples) * 1000.0 / m_sr;

	switch (m_state)
	{
		case PH2_RETRAIN_TONE:
		{
			if (elapsed >= 50.0 && m_v90_peer_reversal)
				enter(PH2_NOT_A);
			break;
		}

		case PH2_A_INITIAL:
		{
			if (elapsed >= 55.0)
				enter(PH2_A_INFO);
			break;
		}

		case PH2_A_INFO:
		{
			recover_v90_info0();

			if (m_v90_peer_reversal && elapsed > 1000.0 * m_info0a_bits.size() / 600.0)
				enter(PH2_NOT_A);
			break;
		}

		case PH2_NOT_A:
		{
			if (elapsed >= 40.0)
			{
				m_v90_peer_reversal = false;
				m_v90_peer_probe_seen = false;
				m_v90_recent_bits = 0;

				enter(PH2_A_SECOND);
			}
			break;
		}

		case PH2_A_SECOND:
		{
			if (m_v90_peer_reversal && elapsed >= 600.0)
				enter(PH2_V90_REVERSAL_DELAY);
			break;
		}

		case PH2_V90_REVERSAL_DELAY:
		{
			if (elapsed >= 50.0)
				enter(PH2_V90_PRE_L1);
			break;
		}

		case PH2_V90_PRE_L1:
		{
			if (elapsed >= 10.0)
				enter(PH2_L1);
			break;
		}

		case PH2_L1:
		{
			if (elapsed >= 160.0)
				enter(PH2_L2);

			m_l1l2.tx(sample, sample_count);
			m_pcm.quantize_probe(sample, sample_count);
			break;
		}

		case PH2_L2:
		{
			if (elapsed >= 480.0)
				enter(PH2_V90_POST_L2);

			m_l1l2.tx(sample, sample_count);
			m_pcm.quantize_probe(sample, sample_count);
			break;
		}

		case PH2_V90_POST_L2:
		{
			if (elapsed >= 120.0)
			{
				m_v90_p2_bits.clear();
				enter(PH2_V90_INFO1C);
			}
			break;
		}

		case PH2_V90_INFO1C:
		{
			info::info1c_t info;
			for (auto &row : info.rate)
			{
				row.max_bit_rate     = 14;
				row.pre_emphasis     = 0;
				row.use_high_carrier = false;
			}

			info.freq_offset = INFO1_FREQ_OFFSET;
			auto bits        = info::build_info1c(info);
			m_dpsk.tx_put_bits(bits.data(), uint32_t(bits.size()));

			enter(PH2_V90_INFO1A_WAIT);
			break;
		}

		case PH2_V90_INFO1A_WAIT:
		{
			if (m_v90_p2_clock - m_v90_info1_recovery_clock >= uint64_t(m_sr * 0.12))
			{
				m_v90_info1_recovery_clock = m_v90_p2_clock;

				for (uint32_t phase = 0; phase < 16; phase++)
				{
					auto           bits = m_dpsk.rx_decode_buffered(0.0, double(phase) / 16.0);
					info::info1a_t peer;
					if (info::parse_info1a(bits.data(), uint32_t(bits.size()), peer))
					{
						accept_v90_info1(peer);
						break;
					}
				}
			}

			if (phase_of(m_state) == PHASE_2 && elapsed >= (m_v90_wait_peer_probe ? V90_INFO1C_QUIET_RETRY_MS : 220.0))
				enter(PH2_V90_INFO1C);
			break;
		}

		default:
			if (phase_of(m_state) == PHASE_PCM)
				return m_pcm.tx(sample, sample_count);
			break;
	}

	if (m_state != PH2_L1 && m_state != PH2_L2)
	{
		m_dpsk.tx(sample, sample_count);

		if (m_state == PH2_RETRAIN_TONE && !m_rt_periodic_peer && std::fmod(elapsed, 600.0) >= 500.0)
			wtvsoftmodem_dsp::clear_samples(sample, sample_count);

		if (m_state == PH2_A_INITIAL && m_v90_silent_lead_in)
			wtvsoftmodem_dsp::clear_samples(sample, sample_count);

		if (m_state == PH2_V90_INFO1A_WAIT && m_v90_wait_peer_probe && m_dpsk.tx_pending_bits() == 0)
			wtvsoftmodem_dsp::clear_samples(sample, sample_count);

		if (m_state == PH2_A_SECOND && wtvsoftmodem_v34family::v90_a_second_silent())
			wtvsoftmodem_dsp::clear_samples(sample, sample_count);
	}

	m_v90_p2_clock += sample_count;

	if (m_v90_p2_clock >= uint64_t(m_sr * 15.0))
		enter(PH_FAILED);

	return sample_count;
}

void wtvsoftmodem_v34family::setup_v90_upstream(const info::info1a_t &peer)
{
	const uint32_t rate = std::min<uint32_t>(peer.baud_rate_a_to_c, 5);

	wtvsoftmodem_qam::config_t cfg;
	cfg.sample_rate = m_sr;
	cfg.eq_taps = 97;
	cfg.eq_mu = 0.20;

	m_v90_upstream_baud = rate;
	m_v90_upstream_rate = BAUD_MAXIMUM[rate];
	m_v90_upstream_ceiling = BAUD_MAXIMUM[rate];
	m_pcm.set_upstream_rate_cap(std::min(m_v90_upstream_ceiling, uint32_t(m_mp_rate_cap)));
	m_v90_data_rx = false;
	m_v90_downstream_ready = false;

	if (!m_retraining)
		m_v90_connected = false;

	m_pcm.set_bit_source(
		[this]() -> int32_t
		{
			return int32_t(m_lapm.tx_get_bit());
		}
	);
	m_pcm.set_data_callback(
		[this]()
		{
			m_v90_downstream_ready = true;
			notify_v90_connected();
		}
	);

	m_qam.init(UPSTREAM_SPEC[rate], cfg);
	m_qam.rx_set_equalizer_rate(0.20);
	m_qam.rx_set_tracking_rates(0.006, 0.004, 0.003);

	wtvsoftmodem_v34train::config_t analog;
	analog.answer_mode = true;
	analog.trn_scale   = SUBC_AMP;
	wtvsoftmodem_v34train training(analog);
	std::vector<wtvsoftmodem_qam::csymbol_t> reference;
	for (uint32_t i = 0; i < 8192; i++)
	{
		auto point = training.gen_trn();
		reference.push_back({ point.i, point.q });
	}
	m_qam.rx_acquire_training_reference(reference.data(), uint32_t(reference.size()));
	m_qam.set_slicer(four_point_decision);

	wtvsoftmodem_v34train::config_t digital;
	digital.answer_mode = false;
	digital.trn_scale   = SUBC_AMP;
	m_train.init(digital);
	m_v90_upstream_bits.clear();
	m_v90_upstream_bits16.clear();
	m_v90_upstream_symbols = 0;
	m_v90_upstream_trained = false;
	m_v90_upstream_capped = false;
	m_v90_upstream_s_run   = 0;
	m_qam.set_rx_symbol_callback(
		[this](wtvsoftmodem_qam::csymbol_t point)
		{
			rx_v90_upstream(point);
		}
	);
}

void wtvsoftmodem_v34family::rx_v90_upstream(wtvsoftmodem_qam::csymbol_t symbol)
{
	m_v90_upstream_symbols++;

	if (!m_v90_upstream_trained && m_qam.rx_reference_trained() && m_qam.rx_evm() < 0.15)
	{
		m_v90_upstream_trained = true;
		m_v90_upstream_settle = 0;
		m_v90_trn_blocks.clear();
		m_train.mp_rx_reset();
		m_qam.rx_reference_snr_reset();

		if (m_v90_wait_peer_probe)
		{
			m_qam.rx_set_clock_frequencies(0.0, m_qam.rx_carrier_frequency());
			m_qam.rx_hold_clock_frequency(true);
			m_qam.rx_set_tracking_rates(V34_DATA_TIMING_BW, V34_TRN_CARRIER_BW, V34_TRN_AGC_RATE);
		}
	}

	if (m_v90_upstream_trained && m_v90_upstream_settle < V34_TRN_SNR_SETTLE)
	{
		m_v90_upstream_settle++;

		if (m_v90_upstream_settle == V34_TRN_SNR_SETTLE)
		{
			m_qam.rx_reference_snr_reset();
			m_v90_trn_blocks.clear();
			m_v90_trn_block_start = 0;
			m_v90_trn_block_error = 0;
			m_v90_trn_block_power = 0;
		}
	}

	if (m_v90_upstream_trained && m_v90_upstream_settle == V34_TRN_SNR_SETTLE && m_qam.rx_reference_snr_count() >= (m_v90_trn_block_start + V90_TRN_SNR_BLOCK))
	{
		const double error = m_qam.rx_reference_error_sum() - m_v90_trn_block_error;
		const double power = m_qam.rx_reference_power_sum() - m_v90_trn_block_power;

		m_v90_trn_blocks.push_back(wtvsoftmodem_dsp::power_to_db(power / std::max(error, 1e-12)));
		m_v90_trn_block_start = m_qam.rx_reference_snr_count();
		m_v90_trn_block_error = m_qam.rx_reference_error_sum();
		m_v90_trn_block_power = m_qam.rx_reference_power_sum();
	}


	if ((!m_retraining && m_pcm.state() == wtvsoftmodem_pcm::PCMState::DATA) || (m_retraining && !m_full_retrain))
	{
		const auto event = m_rt_s_detector.put(symbol.i, symbol.q);

		if (m_rt_full_requested && event == wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::S)
			m_rt_pending_full = true;

		if (m_pcm.state() == wtvsoftmodem_pcm::PCMState::DATA && event == wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::S)
			begin_v90_retrain(false, "peer retrain request: V.90 upstream S sequence detected");

		if (event == wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::SBAR)
		{
			if (m_pcm.state() == wtvsoftmodem_pcm::PCMState::RENEG_WAIT)
				m_pcm.observe_upstream_s(true);

			m_v90_retrain16       = true;
			m_rx_training_pending = true;
			m_rt_s_detector.reset();
		}
	}

	if (m_pcm.upstream_s_expected())
	{
		if (m_v90_wait_peer_probe)
		{
			const auto event = m_rt_s_detector.put(symbol.i, symbol.q);
			switch (event)
			{
				case wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::S:
					m_pcm.observe_upstream_s(false);
					break;

				case wtvsoftmodem_v34train::retrain_s_detector::RetrainSEvent::SBAR:
					m_pcm.observe_upstream_s(true);
					m_rt_s_detector.reset();
					break;

				default:
					break;
			}
		}
		else
		{
			const auto detected = m_train.detect({ symbol.i, symbol.q });
			m_v90_upstream_s_run = detected.confidence > 0.40 ? m_v90_upstream_s_run + 1 : 0;
			if (m_v90_upstream_s_run >= 8 || detected.reversal)
				m_pcm.observe_upstream_s(detected.reversal);
		}
	}

	if (m_v90_data_rx)
	{
		if (!m_rx_data_tracking && update_rx_power_variance(symbol))
		{
			m_rx_data_tracking = true;
			install_data_slicer();
		}

		receive_datamode_symbol(symbol);
		notify_v90_connected();

		if (m_v90_wait_peer_probe && !m_rx_epoch_valid)
		{
			if (m_v90_upstream_cp16)
			{
				uint8_t bits16[4];
				m_train.detect_diff16({ symbol.i, symbol.q }, bits16);
				for (uint8_t bit : bits16)
					m_peer_mark_run = bit ? m_peer_mark_run + 1 : 0;
			}
			else
			{
				uint8_t bits[2];
				detect_diff4_marks(symbol, bits, m_peer_mark_run);
			}

			if (m_peer_mark_run >= E_MARK_RUN)
			{
				m_rx_epoch_valid   = true;
				m_rx_epoch_symbols = 0;
			}
		}

		return;
	}

	uint8_t bits16[4];
	m_train.detect_diff16({ symbol.i, symbol.q }, bits16);
	m_v90_upstream_bits16.append(bits16, 4);

	uint8_t bits[2];
	m_train.detect_diff4({ symbol.i, symbol.q }, bits);
	m_v90_upstream_bits.append(bits, 2);

	std::vector<uint8_t> ja;
	if (m_pcm.state() == wtvsoftmodem_pcm::PCMState::WAIT_JA && wtvsoftmodem_pcm::parse_ja(m_v90_upstream_bits.bits, ja, &m_v90_upstream_bits.ja_resume))
		m_pcm.accept_ja(ja);

	const bool late_cp = m_v90_wait_peer_probe && m_pcm.past_mp();
	if ((m_v90_upstream_symbols % 16) == 0 && (m_pcm.cp_expected() || late_cp))
	{
		std::vector<uint8_t> cp;

		size_t consumed = 0;

		const bool four_point = wtvsoftmodem_pcm::parse_cp(m_v90_upstream_bits.bits, cp, &consumed, &m_v90_upstream_bits.resume);
		const bool sixteen_point = !four_point && wtvsoftmodem_pcm::parse_cp(m_v90_upstream_bits16.bits, cp, &consumed, &m_v90_upstream_bits16.resume);
		if (four_point || sixteen_point)
		{
			if (m_v90_wait_peer_probe && !m_v90_upstream_capped && cp.size() > 1 && (cp[0] & 2))
				cap_v90_upstream_rate();

			m_pcm.accept_cp(cp);
			m_v90_upstream_cp16 = sixteen_point;
			m_qam.rx_freeze_loops(false);

			auto &stream = four_point ? m_v90_upstream_bits : m_v90_upstream_bits16;
			stream.drop_front(consumed);

			if ((cp[0] & 2) && ((cp[1] & 128) || m_v90_wait_peer_probe))
				setup_v90_data_rx();
		}
	}

	if (m_v90_upstream_bits16.bits.size() > 8192)
		m_v90_upstream_bits16.drop_front(4096);

	if (m_v90_upstream_bits.bits.size() > 8192)
		m_v90_upstream_bits.drop_front(4096);
}

void wtvsoftmodem_v34family::cap_v90_upstream_rate()
{
	m_v90_upstream_capped = true;
	if (m_v90_trn_blocks.size() * V90_TRN_SNR_BLOCK < V34_TRN_SNR_MIN_SYMBOLS)
	{
		const uint32_t cap = std::clamp<uint32_t>(
			std::min({ m_v90_snr_cap, m_v90_upstream_ceiling, uint32_t(m_mp_rate_cap) }),
			MP_RETRAIN_MIN_RATE, 
			14
		);
		m_pcm.set_upstream_rate_cap(cap);
	}
	else
	{
		auto blocks = m_v90_trn_blocks;
		auto pick = blocks.begin() + std::ptrdiff_t(blocks.size() * V90_TRN_SNR_PERCENTILE / 100);
		std::nth_element(blocks.begin(), pick, blocks.end());

		const double snr = *pick;
		const uint32_t rate = wtvsoftmodem_v34train::retrain_sustainable_rate(snr, m_v90_upstream_baud, false, false, false);
		m_v90_snr_cap = rate;

		const uint32_t cap = std::clamp<uint32_t>(
			std::min({ rate, m_v90_upstream_ceiling, uint32_t(m_mp_rate_cap) }),
			MP_RETRAIN_MIN_RATE,
			14
		);
		m_pcm.set_upstream_rate_cap(cap);
	}
}

void wtvsoftmodem_v34family::setup_v90_data_rx()
{
	if (m_v90_data_rx)
		return;

	m_v90_upstream_rate  = v90_upstream_data_rate();

	wtvsoftmodem_v34data::config_t cfg;
	cfg.baud_rate_code   = int32_t(m_v90_upstream_baud);
	cfg.bit_rate_code    = int32_t((m_v90_upstream_rate - 1) * 2);
	cfg.answer_mode      = true;
	cfg.expanded_shaping = false;
	cfg.point_scale      = SUBC_AMP * std::sqrt(2.0);
	cfg.match_trn_power  = true;
	cfg.invert_output    = true;
	cfg.nonlin_k         = 0;
	cfg.use_precoder     = false;

	m_dm_rx.init(cfg);

	m_rx_sync.init(cfg, true, m_retraining && m_lapm.connected(), m_retraining ? m_rx_data_gain : 1.0, 12);

	if (!m_retraining)
		m_rx_data_gain = 1.0;

	if (!m_v90_connected)
		m_lapm.reset();

	m_lapm.set_bit_rate(m_pcm.data_rate());
	m_lapm.set_rx_byte_callback(
		[this](uint8_t byte)
		{
			if (m_rx_byte_cb)
				m_rx_byte_cb(byte);
		}
	);

	m_dm_rx_bit_sink = [this](int32_t bit)
	{
		m_lapm.rx_bit(bit != 0);
	};

	m_rx_sync.set_bit_sink(m_dm_rx_bit_sink);
	m_rx_sync_searching = true;

	m_rx_frame_locked   = false;
	m_rx_data_tracking  = false;
	m_rx_power_variance = 0;

	m_v90_data_rx       = true;

	m_qam.rx_finish_training();
	m_qam.rx_set_equalizer_rate(V34_DATA_EQ_RATE);
	install_data_slicer();

	m_peer_mark_run = 0;

	m_qam.rx_set_tracking_rates(data_timing_bw(), V34_DATA_CARRIER_BW, m_retraining ? 0 : V34_DATA_AGC_RATE);

	if (m_shared_sample_clock)
	{
		m_qam.rx_set_clock_frequencies(0.0, m_qam.rx_carrier_frequency());
		m_qam.rx_hold_timing(true);
	}
	else if (m_v90_wait_peer_probe)
	{
		m_qam.rx_set_clock_frequencies(0.0, m_qam.rx_carrier_frequency());
		m_qam.rx_hold_clock_frequency(true);
	}
}

void wtvsoftmodem_v34family::notify_v90_connected()
{
	if (m_v90_connected || !m_v90_downstream_ready || !m_rx_frame_locked)
		return;

	m_v90_connected = true;
	m_rt_data_since = m_total_samples;

	if (m_connected_cb)
		m_connected_cb();
}
