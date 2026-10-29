// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34FAMILY_H
#define MAME_WEBTV_WTVSOFTMODEM_V34FAMILY_H

#pragma once

#include "wtvsoftmodem_dsp.h"
#include "wtvsoftmodem_link.h"
#include "wtvsoftmodem_pcm.h"
#include "wtvsoftmodem_qam.h"
#include "wtvsoftmodem_v34data.h"
#include "wtvsoftmodem_v34info.h"
#include "wtvsoftmodem_v34probe.h"
#include "wtvsoftmodem_v34rxsync.h"
#include "wtvsoftmodem_v34train.h"

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ITU-T V.34 11 Duplex operating procedures, ITU-T V.90 9 Operating procedures

class wtvsoftmodem_v34family
{

public:
	typedef std::function<void(uint8_t)> rx_byte_cb;
	typedef std::function<void()> event_cb;

	static constexpr uint32_t STATE_PHASE_SHIFT = 5;
	static constexpr uint8_t  STATE_STAGE_MASK  = (1 << STATE_PHASE_SHIFT) - 1;

	enum V34FamPhase : uint8_t
	{
		PHASE_IDLE,
		PHASE_RETRAIN,
		PHASE_2,
		PHASE_3,
		PHASE_4,
		PHASE_PCM,
		PHASE_FAILED = 7
	};

	enum V34FamState : uint8_t
	{
		PH_IDLE = PHASE_IDLE << STATE_PHASE_SHIFT,

		PH_RETRAIN_SILENCE = PHASE_RETRAIN << STATE_PHASE_SHIFT,

		PH2_A_INITIAL = PHASE_2 << STATE_PHASE_SHIFT,
		PH2_A_INFO,
		PH2_NOT_A,
		PH2_NOT_A_REV,
		PH2_A_SECOND,
		PH2_L1,
		PH2_L2,
		PH2_POST_L2_A,

		PH2_POST_L2_NOT_A,
		PH2_POST_L2_A2,

		PH2_INFO1,
		PH2_RETRAIN_TONE,

		PH2_V90_REVERSAL_DELAY,
		PH2_V90_PRE_L1,
		PH2_V90_POST_L2,
		PH2_V90_INFO1C,
		PH2_V90_INFO1A_WAIT,

		PH3_S = PHASE_3 << STATE_PHASE_SHIFT,
		PH3_SBAR,
		PH3_PP,
		PH3_TRN,
		PH3_J,
		PH3_JWAIT,
		PH3_MP,
		PH3_E,

		PH4_MPWAIT = PHASE_4 << STATE_PHASE_SHIFT,
		PH4_DATA,

		// These states are handled in wtvsoftmodem_pcm, they are just here so these states bubble down to state_change_cb.
		PH_PCM_WAIT_JA = PHASE_PCM << STATE_PHASE_SHIFT,
		PH_PCM_S,
		PH_PCM_SBAR,
		PH_PCM_TRN1D,
		PH_PCM_JD,
		PH_PCM_JDPRIME,
		PH_PCM_DIL,
		PH_PCM_RI,
		PH_PCM_RIBAR,
		PH_PCM_TRN2D,
		PH_PCM_MP,
		PH_PCM_ED,
		PH_PCM_B1,
		PH_PCM_DATA,
		PH_PCM_RENEG_WAIT,
		PH_PCM_RD,
		PH_PCM_RDBAR,

		PH_FAILED = PHASE_FAILED << STATE_PHASE_SHIFT
	};

	static constexpr V34FamPhase phase_of(V34FamState s)
	{
		return V34FamPhase(s >> STATE_PHASE_SHIFT);
	}
	static constexpr uint32_t stage_of(V34FamState s)
	{
		return s & STATE_STAGE_MASK;
	}

	static constexpr V34FamState pcm_to_state(wtvsoftmodem_pcm::PCMState state)
	{
		return V34FamState(PH_PCM_WAIT_JA + (uint8_t(state) - uint8_t(wtvsoftmodem_pcm::PCMState::WAIT_JA)));
	}

	static std::string state_to_string(V34FamState state)
	{
		switch (state)
		{
			case PH_IDLE:
				return "PH_IDLE";
			case PH_RETRAIN_SILENCE:
				return "PH_RETRAIN_SILENCE";
			case PH2_A_INITIAL:
				return "PH2_A_INITIAL";
			case PH2_A_INFO:
				return "PH2_A_INFO";
			case PH2_NOT_A:
				return "PH2_NOT_A";
			case PH2_NOT_A_REV:
				return "PH2_NOT_A_REV";
			case PH2_A_SECOND:
				return "PH2_A_SECOND";
			case PH2_L1:
				return "PH2_L1";
			case PH2_L2:
				return "PH2_L2";
			case PH2_POST_L2_A:
				return "PH2_POST_L2_A";
			case PH2_POST_L2_NOT_A:
				return "PH2_POST_L2_NOT_A";
			case PH2_POST_L2_A2:
				return "PH2_POST_L2_A2";
			case PH2_INFO1:
				return "PH2_INFO1";
			case PH2_RETRAIN_TONE:
				return "PH2_RETRAIN_TONE";
			case PH2_V90_REVERSAL_DELAY:
				return "PH2_V90_REVERSAL_DELAY";
			case PH2_V90_PRE_L1:
				return "PH2_V90_PRE_L1";
			case PH2_V90_POST_L2:
				return "PH2_V90_POST_L2";
			case PH2_V90_INFO1C:
				return "PH2_V90_INFO1C";
			case PH2_V90_INFO1A_WAIT:
				return "PH2_V90_INFO1A_WAIT";
			case PH3_S:
				return "PH3_S";
			case PH3_SBAR:
				return "PH3_SBAR";
			case PH3_PP:
				return "PH3_PP";
			case PH3_TRN:
				return "PH3_TRN";
			case PH3_J:
				return "PH3_J";
			case PH3_JWAIT:
				return "PH3_JWAIT";
			case PH3_MP:
				return "PH3_MP";
			case PH3_E:
				return "PH3_E";
			case PH4_MPWAIT:
				return "PH4_MPWAIT";
			case PH4_DATA:
				return "PH4_DATA";
			case PH_PCM_WAIT_JA:
				return "PH_PCM_WAIT_JA";
			case PH_PCM_S:
				return "PH_PCM_S";
			case PH_PCM_SBAR:
				return "PH_PCM_SBAR";
			case PH_PCM_TRN1D:
				return "PH_PCM_TRN1D";
			case PH_PCM_JD:
				return "PH_PCM_JD";
			case PH_PCM_JDPRIME:
				return "PH_PCM_JDPRIME";
			case PH_PCM_DIL:
				return "PH_PCM_DIL";
			case PH_PCM_RI:
				return "PH_PCM_RI";
			case PH_PCM_RIBAR:
				return "PH_PCM_RIBAR";
			case PH_PCM_TRN2D:
				return "PH_PCM_TRN2D";
			case PH_PCM_MP:
				return "PH_PCM_MP";
			case PH_PCM_ED:
				return "PH_PCM_ED";
			case PH_PCM_B1:
				return "PH_PCM_B1";
			case PH_PCM_DATA:
				return "PH_PCM_DATA";
			case PH_PCM_RENEG_WAIT:
				return "PH_PCM_RENEG_WAIT";
			case PH_PCM_RD:
				return "PH_PCM_RD";
			case PH_PCM_RDBAR:
				return "PH_PCM_RDBAR";
			case PH_FAILED:
				return "PH_FAILED";
		}

		return "UNKNOWN";
	}

	typedef std::function<void(V34FamState, V34FamState)> state_change_cb;

	static constexpr wtvsoftmodem::v34::info::info0_t DEFAULT_LOCAL_INFO0 = {
		.sr_2743 = true,
		.sr_2800 = true,
		.sr_3429 = true,
		.sr_3000_low = true,
		.sr_3000_high = true,
		.sr_3200_low = true,
		.sr_3200_high = true,
		.rate_3429_allowed = true,
		.power_reduction = true,
		.max_baud_diff = 0,
		.from_cme_modem = false,
		.support_1664 = true,
		.tx_clock_source = wtvsoftmodem::v34::info::INFO0_TX_CLOCK_RX_TIMING
	};

	wtvsoftmodem_v34family();

	void set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate);

	void set_answer_mode(bool answer)
	{
		m_answer = answer;
		set_sample_rate(m_sr);
	}
	void set_rx_byte_callback(rx_byte_cb cb)
	{
		m_rx_byte_cb = std::move(cb);
	}
	void set_connected_callback(event_cb cb)
	{
		m_connected_cb = std::move(cb);
	}
	void set_failed_callback(event_cb cb)
	{
		m_failed_cb = std::move(cb);
	}
	void set_state_change_callback(state_change_cb cb)
	{
		m_state_change_cb = std::move(cb);
	}
	void set_local_info0(const wtvsoftmodem::v34::info::info0_t &info0)
	{
		m_local_info0 = info0;
	}
	wtvsoftmodem::v34::info::info0_t &get_local_info0()
	{
		return m_local_info0;
	}

	bool has_peer_info0() const
	{
		return m_got_info0c;
	}
	const wtvsoftmodem::v34::info::info0_t &get_peer_info0() const
	{
		return m_peer_info0;
	}

	bool has_peer_info1c() const
	{
		return m_got_info1c;
	}
	const wtvsoftmodem::v34::info::info1c_t &get_peer_info1c() const
	{
		return m_peer_info1c;
	}

	bool has_peer_info1a() const
	{
		return m_got_info1a;
	}
	const wtvsoftmodem::v34::info::info1a_t &get_peer_info1a() const
	{
		return m_peer_info1a;
	}

	const wtvsoftmodem::v34::info::mp_t &get_local_mp() const
	{
		return m_local_mp;
	}
	bool has_peer_mp() const
	{
		return m_got_mp;
	}
	const wtvsoftmodem::v34::info::mp_t &get_peer_mp() const
	{
		return m_peer_mp;
	}

	void set_v90_mode(bool enabled)
	{
		m_v90_mode = enabled;
		set_sample_rate(m_sr);
	}

	void start();
	void stop();
	void request_retrain(bool full = false, std::string_view reason = "retrain request");

	V34FamState state() const
	{
		return m_state;
	}

	bool is_data_mode() const
	{
		return m_state == PH4_DATA || (m_v90_mode && m_pcm.state() == wtvsoftmodem_pcm::PCMState::DATA);
	}

	bool is_retraining() const
	{
		return m_retraining;
	}

	bool is_v90_mode() const
	{
		return m_v90_mode;
	}

	void enable_pcm_alaw(bool enable)
	{
		m_pcm.set_alaw(enable);
	}

	bool pcm_alaw_enabled() const
	{
		return m_pcm.alaw();
	}

	uint32_t rx_good_frames() const
	{
		return m_lapm.crc_ok_count();
	}

	uint32_t rx_bad_frames() const
	{
		return m_lapm.crc_fail_count();
	}

	bool rx_link_detection_complete() const
	{
		return m_rx_frame_locked && !m_lapm.waiting_for_odp();
	}

	uint32_t tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);
	void rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	void transmit_byte(uint8_t b);

private:

	static constexpr wtvsoftmodem_qam::qam_spec_t V34_QAM_SPEC    = wtvsoftmodem_qam::QAM_V34_PRIMARY;
	static constexpr double                       V34_SYMBOL_RATE = wtvsoftmodem_qam::default_spec(V34_QAM_SPEC)->symbol_rate;
	static constexpr double                       V34_CARRIER     = wtvsoftmodem_qam::default_spec(V34_QAM_SPEC)->carrier_freq;

	static constexpr double V34_OUT_SCALE = 6443.85;

	static constexpr int32_t V34_BAUD_3429 = 5;
	static constexpr int32_t V34_MAX_RATE  = 14;

	static constexpr int32_t  V34_SHAPER_TAPS           = 63;
	static constexpr int32_t  V34_EQ_TAPS               = 97;
	static constexpr double   V34_TRN_EQ_RATE           = 0.20;
	static constexpr double   V34_DATA_EQ_RATE          = 0.02;
	static constexpr uint32_t V34_TRN_REFERENCE_SYMBOLS = 8192;

	static constexpr double V34_TRN_CARRIER_BW  = 0.004;
	static constexpr double V34_TRN_AGC_RATE    = 0.003;
	static constexpr double V34_DATA_TIMING_BW  = 0.0002;
	static constexpr double V90_DATA_TIMING_BW  = 0.0006;
	static constexpr double V34_DATA_CARRIER_BW = 0.0004;
	static constexpr double V34_DATA_AGC_RATE   = 0.0003;

	static constexpr double RX_ON_POWER = 2.0e-4;


	static constexpr uint32_t V34_DATA_TIMING_DIVISOR  = 64;
	static constexpr double   V34_TRN_TIMING_BW        = 0.006;
	static constexpr uint32_t V34_TRN_SNR_MIN_SYMBOLS  = 1024;
	static constexpr uint32_t V34_TRN_SNR_SETTLE       = 512;

	static constexpr double V34_DPSK_OUT_SCALE = 2000.0;

	static constexpr double SUBC_AMP = 0.70710678;

	static constexpr double  PH2_A_INITIAL_MS        = 55.0;
	static constexpr double  PH2_A_INFO_MAX_MS       = 600.0;
	static constexpr double  PH2_REVERSAL_SETTLE_MS  = 25.0;
	static constexpr double  PH2_NOT_A_MAX_MS        = 2000.0;
	static constexpr double  PH2_NOT_A_REV_MS        = 150.0;
	static constexpr double  PH2_A_SECOND_MS         = 20.0;
	static constexpr double  PH2_POST_A_MIN_MS       = 300.0;
	static constexpr double  PH2_POST_A_MAX_MS       = 4000.0;
	static constexpr double  PH2_POST_A_WATCH_MS     = 60.0;
	static constexpr double  PH2_POST_NOT_A_MS       = 15.0;
	static constexpr double  PH2_A2_CAP_MS           = 12000.0;
	static constexpr double  PH2_A2_RECOVERY_LEAD_MS = 2000.0;
	static constexpr double  PH2_INFO1_FLUSH_MS      = 24.0;
	static constexpr int32_t PH2_REVERSAL_RX_PAUSE   = 20;

	static constexpr double   INFO1C_TO_INFO1A_MS        = 70.0;
	static constexpr double   INFO1C_RETRY_MS            = 25.0;
	static constexpr size_t   INFO1C_BITS                = 109;
	static constexpr uint32_t INFO1C_RECOVERY_PHASES     = 200;
	static constexpr uint32_t INFO1C_RECOVERY_CANDIDATES = 4;
	static constexpr int32_t  INFO1_FREQ_OFFSET          = -512;

	static constexpr uint32_t V90_REVERSAL_ZERO_RUN               = 24;
	static constexpr uint32_t V90_PROBE_WINDOW_MASK               = (1u << 30) - 1;
	static constexpr int      V90_PROBE_MIN_ONES                  = 8;
	static constexpr double   V90_INFO1C_QUIET_RETRY_MS           = 3000.0;
	static constexpr double V90_A_SECOND_SILENCE_START_RETRAIN_MS = 150.0;
	static constexpr double V90_A_SECOND_SILENCE_END_RETRAIN_MS   = 570.0;
	static constexpr double V90_A_SECOND_SILENCE_START_MS         = 480.0;
	static constexpr double V90_A_SECOND_SILENCE_END_MS           = 900.0;
	static constexpr double V90_A_SECOND_SILENCE_MS               = V90_A_SECOND_SILENCE_END_MS - V90_A_SECOND_SILENCE_START_MS;
	static constexpr double V90_A_SECOND_REV_RETRAIN_MS           = 690.0;
	static constexpr double V90_A_SECOND_REV_MS                   = 1020.0;

	static constexpr int32_t MIN_CLEAN_RUN_BITS = 15;


	static constexpr double PH3_JWAIT_CAP_MS    = 4500.0;
	static constexpr double PH3_STAGE_SETTLE_MS = 70.0;
	static constexpr double PH3_MP_MAX_MS       = 6000.0;
	static constexpr double PH3_MPPRIME_MS      = 100.0;
	static constexpr double PH3_E_MS            = 40.0;
	static constexpr double PH3_E_MIN_FLOOR_MS  = 150.0;
	static constexpr double PH3_E_MAX_MS        = 3000.0;

	static constexpr uint64_t SYM_PH3_S          = 128;
	static constexpr uint64_t SYM_PH3_SBAR       = 16;
	static constexpr uint64_t SYM_PH3_PP         = 288;
	static constexpr uint64_t SYM_PH3_TRN1       = 1152;
	static constexpr uint64_t SYM_PH3_J          = 48;
	static constexpr uint64_t SYM_PH3_S2         = 128;
	static constexpr uint64_t SYM_PH3_SBAR2      = 16;
	static constexpr uint32_t SYM_PH3_E          = 10;

	static constexpr int32_t JWAIT_FAST_PERSIST = 2;

	static constexpr uint32_t E_MARK_RUN = 20;

	static constexpr double   TRN_EVM_FLOOR       = 1e-6;
	static constexpr uint32_t ROUND2_TRN_MARK_RUN = 128;
	static constexpr double   ROUND2_TRN_EVM_DB   = -7.0;

	static constexpr double PH4_MPWAIT_MAX_MS  = 3000.0;
	static constexpr double PH4_MP2_LEADIN_MS  = 100.0;
	static constexpr double PH4_MP_ACK_HOLD_MS = 72.0;

	static constexpr size_t   MP_RX_MIN_BITS         = 64;
	static constexpr size_t   MP_RX_MAX_BITS         = 4096;
	static constexpr size_t   MP_RX_TRIM_BITS        = 2048;
	static constexpr size_t   MP_RETRAIN_SOFT_POINTS = 64;
	static constexpr uint32_t MP_RETRAIN_MIN_RATE    = 2;
	static constexpr double   MP_ALTOCOM_E_BUDGET_MS = 2500.0 + 2.0 * 132.0;
	static constexpr uint32_t SYM_MP_ACK_GUARD       = 64;

	static constexpr double RX_POWER_VARIANCE_DECAY = 0.9;
	static constexpr double RX_POWER_VARIANCE_GAIN  = 0.1;
	static constexpr double RX_DATA_VARIANCE        = 0.04;

	static constexpr double INITIAL_MARK_FILL_MS = 500.0;

	static constexpr double RETRAIN_CHECK_INTERVAL_MS = 2000.0;
	static constexpr double RETRAIN_SILENCE_MS        = 70.0;
	static constexpr double RETRAIN_SHORT_TIMEOUT_MS  = 6000.0;
	static constexpr double RETRAIN_FULL_TIMEOUT_MS   = 30000.0;
	static constexpr double V90_RETRAIN_SHORT_TIMEOUT_MS = 12000.0;
	static constexpr uint32_t V90_TRN_SNR_BLOCK      = 64;
	static constexpr uint32_t V90_TRN_SNR_PERCENTILE = 30;
	static constexpr double RETRAIN_TONE_MIN_MS       = 50.0;
	static constexpr double RETRAIN_TONE_MAX_MS       = 10000.0;
	static constexpr double RETRAIN_SBAR_TO_E_MS      = 1810.0;

	static constexpr double   RATE_DOWN_MARGIN_DB  = 1.5;
	static constexpr double   RATE_UP_MARGIN_DB    = 2.0;
	static constexpr double   FULL_RETRAIN_SNR_DB  = 10.0;
	static constexpr uint32_t MAX_SHORT_DOWNSHIFTS = 3;
	static constexpr double   RATE_UPSHIFT_HOLD_MS  = 30000.0;

	static constexpr wtvsoftmodem_qam::qam_spec_t UPSTREAM_SPEC[] = { wtvsoftmodem_qam::QAM_V34_2400_LOW, wtvsoftmodem_qam::QAM_V34_2743_LOW, wtvsoftmodem_qam::QAM_V34_2800_LOW, wtvsoftmodem_qam::QAM_V34_3000_LOW, wtvsoftmodem_qam::QAM_V34_3200_LOW, wtvsoftmodem_qam::QAM_V34_3429_LOW };

	static constexpr uint32_t BAUD_MAXIMUM[6] = { 9, 11, 11, 12, 13, 14 };

	// V.34 Table 2: arrier frequencies versus symbol rate
	static constexpr double BAUD_HIGH_CARRIER_RATIO[6] = {
		// 1800Hz
		3.0 / 4.0,
		// 1829Hz
		2.0 / 3.0,
		// 1867Hz
		2.0 / 3.0,
		// 2000Hz
		2.0 / 3.0,
		// 1920Hz
		3.0 / 5.0,
		// 1959Hz
		4.0 / 7.0
	};

	static constexpr std::array<std::pair<uint32_t, uint16_t>, 32> PEER_J_WINDOWS = []
	{
		std::array<std::pair<uint32_t, uint16_t>, 32> t{};

		uint32_t n = 0;

		for (uint16_t pattern : { wtvsoftmodem_v34train::J_PATTERN_4_POINT, wtvsoftmodem_v34train::J_PATTERN_16_POINT })
		{
			for (uint32_t phase = 0; phase < 16; phase++)
			{
				const uint16_t rotated = uint16_t((uint32_t(pattern) >> phase) | (uint32_t(pattern) << (16 - phase)));

				t[n++] = { uint32_t(rotated) | (uint32_t(rotated) << 16), pattern };
			}
		}

		return t;
	}();

	bool m_answer = true;
	double m_phase3_timeout_ms = PH3_JWAIT_CAP_MS;
	uint16_t m_last_peer_j_pattern = 0;

	V34FamState m_state = PH_IDLE;

	wtvsoftmodem_v34dpsk m_dpsk;
	wtvsoftmodem_v34probe m_l1l2;
	wtvsoftmodem_v34train m_train;

	wtvsoftmodem_qam m_qam;
	wtvsoftmodem_v34data m_datamode;
	wtvsoftmodem_v34data m_dm_rx;
	wtvsoftmodem_v34rxsync m_rx_sync;
	wtvsoftmodem_link m_lapm;

	wtvsoftmodem_dsp::frequency_t m_sr = wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE;
	wtvsoftmodem_dsp::frequency_t m_qsr = wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE;

	int32_t m_v34_baud = V34_BAUD_3429;
	bool m_v34_high_carrier = true;
	double m_v34_symbol_rate = V34_SYMBOL_RATE;
	double m_v34_carrier = V34_CARRIER;

	wtvsoftmodem_pcm m_pcm;
	uint32_t m_v90_upstream_baud = 4;
	uint32_t m_v90_upstream_rate = 13;
	uint32_t m_v90_upstream_ceiling = 13;
	bool m_v90_data_rx = false;
	bool m_v90_downstream_ready = false;
	bool m_v90_connected = false;

	struct frame_backlog
	{
		std::vector<uint8_t> bits;
		uint32_t resume = 0;
		uint32_t ja_resume = 0;

		void clear()
		{
			bits.clear();
			resume = 0;
			ja_resume = 0;
		}

		void append(const uint8_t *data, size_t count)
		{
			bits.insert(bits.end(), data, data + count);
		}

		void drop_front(size_t count)
		{
			bits.erase(bits.begin(), bits.begin() + count);

			resume = (resume > count) ? uint32_t(resume - count) : 0;
			ja_resume = (ja_resume > count) ? uint32_t(ja_resume - count) : 0;
		}
	};

	frame_backlog m_v90_upstream_bits;
	frame_backlog m_v90_upstream_bits16;
	uint64_t m_v90_upstream_symbols = 0;
	bool m_v90_upstream_trained = false;
	uint32_t m_v90_upstream_s_run = 0;
	uint64_t m_v90_info1_recovery_clock = 0;
	uint64_t m_v90_p2_clock = 0;
	bool m_v90_peer_reversal = false;
	std::vector<uint8_t> m_v90_p2_bits;
	bool m_v90_mode = false;

	double m_rx_data_gain = 1.0;
	double m_rx_data_phase = 0;
	uint64_t m_state_samples = 0;
	uint64_t m_total_samples = 0;
	uint64_t m_ph3_sym = 0;
	uint32_t m_e_tx_symbols = 0;
	bool m_peer_mp_ack = false;
	uint32_t m_mp_ack_guard_symbols = 0;
	bool m_peer_e_received = false;
	uint32_t m_peer_mark_run = 0;
	int32_t m_ph3_round = 0;
	uint64_t m_jwait_peer_up = 0;
	uint64_t m_peer_j_hold_symbols = 0;
	uint32_t m_peer_j_window = 0;
	uint32_t m_peer_j_bits = 0;
	bool m_peer_j_detected = false;
	uint32_t m_peer_round2_mark_run = 0;
	uint64_t m_peer_round2_settle_symbols = 0;
	bool m_peer_round2_trn_ready = false;
	bool m_mp_prime = false;
	bool m_mp2_acked = false;
	uint64_t m_mp2_ack_sample = 0;
	int32_t m_mp2_leadin_left = 0;

	wtvsoftmodem::v34::info::info0_t m_local_info0 = DEFAULT_LOCAL_INFO0;
	wtvsoftmodem::v34::info::info0_t m_peer_info0;
	wtvsoftmodem::v34::info::info1c_t m_peer_info1c;
	wtvsoftmodem::v34::info::info1a_t m_peer_info1a;
	bool m_got_info1a = false;
	std::vector<uint8_t> m_info0a_bits;
	size_t m_info0a_pos = 0;
	bool m_info0_recovery = false;
	bool m_info0_finish = false;
	uint64_t m_info0_last_try_sample = 0;
	uint32_t m_info0_recovery_phase = 0;
	bool m_initial_a_reversed = false;
	std::vector<uint8_t> m_info1a_bits;
	size_t m_info1a_pos = 0;
	std::vector<uint8_t> m_dpsk_rx_bits;
	std::vector<uint8_t> m_dpsk_rx_reliable;
	bool m_got_info0c = false;
	int32_t m_info0c_count = 0;
	uint64_t m_info1c_last_try_sample = 0;
	bool m_info1c_full_recovery_tried = false;
	uint32_t m_info1c_recovery_phase = 0;
	uint64_t m_info1c_valid_sample = 0;
	bool m_got_info1c = false;
	wtvsoftmodem::v34::info::info1c_settle m_info1c_settle;

	int32_t m_rev_run = 0;
	int32_t m_peer_rev_events = 0;
	uint64_t m_last_rev_sample = 0;
	int32_t m_clean_run = 0;
	int32_t m_pending_clean_run = 0;
	double m_rx_pwr = 0.0;
	bool m_rx_on = false;
	uint64_t m_rx_edge_sample = 0;
	double m_jwait_fast_pwr = 0.0;
	int32_t m_jwait_fast_run = 0;
	bool m_jwait_fast_on = false;
	int32_t m_probe_watch = 0;
	uint64_t m_probe_start_sample = 0;
	uint64_t m_a2_last_rev_sample = 0;
	bool m_a2_silent = false;
	bool m_qam_started = false;


	wtvsoftmodem_v34train::detect_state_t m_det;
	bool m_peer_s_seen = false;
	bool m_retraining = false;
	bool m_full_retrain = false;
	bool m_rt_peer_sbar = false;
	bool m_rt_periodic_peer = false;
	bool m_rt_pending_peer = false;
	bool m_v90_retrain16 = false;
	bool m_rt_full_requested = false;
	bool m_rt_pending_full = false;
	bool m_rt_saved_clocks = false;
	std::vector<wtvsoftmodem_qam::csymbol_t> m_rt_equalizer_prior;
	double m_rt_saved_timing = 0;
	double m_rt_saved_carrier = 0;
	std::deque<wtvsoftmodem_qam::csymbol_t> m_rt_mp_soft;
	double m_rt_mp_phase = 0;
	bool m_rt_mp_phase_valid = false;
	bool m_rt_phase_corrected = false;
	uint64_t m_rt_started = 0;
	uint64_t m_rt_peer_sbar_sample = 0;
	uint64_t m_rt_data_since = 0;
	uint32_t m_rt_downshifts = 0;
	wtvsoftmodem_v34train::retrain_s_detector m_rt_s_detector;
	wtvsoftmodem_v34train::retrain_tone_detector m_rt_tone_detector;
	int32_t m_dm_s_persist = 0;
	bool m_rt_requesting = false;
	int32_t m_configured_rate_cap = 14;
	bool m_rx_nonlinear_requested = false;
	bool m_rx_precoder_identity = true;
	int32_t m_rx_preemphasis = -1;
	int32_t m_tx_preemphasis = -1;
	bool m_trn_narrowed = false;
	double m_phase3_snr = 0.0;
	bool m_v90_silent_lead_in = false;
	uint32_t m_v90_zero_run = 0;
	bool m_v90_upstream_cp16 = false;
	bool m_v90_upstream_capped = false;
	uint32_t m_v90_upstream_settle = 0;
	uint32_t m_v90_snr_cap = 14;
	std::vector<double> m_v90_trn_blocks;
	uint32_t m_v90_trn_block_start = 0;
	double m_v90_trn_block_error = 0;
	double m_v90_trn_block_power = 0;
	bool m_v90_wait_peer_probe = false;
	bool m_v90_peer_probe_seen = false;
	uint32_t m_v90_recent_bits = 0;
	bool m_trn_snr_settled = false;
	int32_t m_mp_rate_cap = 14;
	uint64_t m_rt_last_check = 0;
	uint32_t m_rt_prev_crc_ok = 0;
	uint32_t m_rt_prev_crc_fail = 0;
	uint32_t m_rt_good_run = 0;
	uint32_t m_rt_bad_run = 0;
	uint32_t m_rt_upshift_from = 0;
	uint32_t m_rt_rate_ceiling = 15;
	std::vector<uint8_t> m_mp_tx_bits;
	std::vector<uint8_t> m_mp_pending_bits;
	size_t m_mp_tx_pos = 0;
	frame_backlog m_mp_rx_bits;
	bool m_got_mp = false;
	wtvsoftmodem::v34::info::mp_t m_peer_mp;
	wtvsoftmodem::v34::info::mp_t m_local_mp;
	bool m_had_mp = false;
	bool m_datamode_ready = false;
	int32_t m_dm_e_bits = 0;


	std::function<void(int32_t)> m_dm_rx_bit_sink;
	bool m_rx_sync_searching = false;
	bool m_rx_frame_locked = false;
	bool m_rx_data_tracking = false;
	bool m_rx_tracking_pending = false;
	bool m_rx_training_pending = false;
	bool m_rx_hdlc_acquisition = false;
	bool m_rx_epoch_valid = false;
	uint64_t m_rx_epoch_symbols = 0;
	double m_rx_power_variance = 0.0;
	bool m_dm_rx_ready = false;
	uint32_t m_dm_rx_cfg_rate = 0;
	bool m_rx_j_hold = false;

	double v34_timing_block_symbols() const { return 48.0 / 8000.0 * m_v34_symbol_rate; }
	uint32_t v34_clock_est_settle() const { return uint32_t(m_v34_symbol_rate); }
	uint32_t v34_clock_est_minimum() const { return uint32_t(256 * v34_timing_block_symbols()); }
	uint32_t v34_data_timing_batch() const { return uint32_t(128 * v34_timing_block_symbols()); }
	uint32_t v34_trn_narrow_symbols() const { return v34_clock_est_minimum() / 2; }
	uint64_t sym_ph3_jwait_hold() const { return uint64_t(m_v34_symbol_rate * 0.4) - 32 + 5 * 16 + uint64_t(m_v34_symbol_rate * PH3_STAGE_SETTLE_MS / 1000.0); }
	uint64_t sym_ph3_trn2_cap() const { return uint64_t(2000.0 * m_v34_symbol_rate / 1000.0); }

	static wtvsoftmodem_qam::csymbol_t four_point_decision(wtvsoftmodem_qam::csymbol_t point);

	void enter(V34FamState new_state);
	void enter_phase2(V34FamState new_state);
	void enter_phase3(V34FamState new_state);
	void enter_phase4(V34FamState new_state, V34FamState prev_state);
	void enter_ph3_mp();
	void enter_data_mode(V34FamState prev_state);
	void setup_datamode_rx_loops();
	void reset_datamode_link(bool was_retraining, V34FamState prev_state);
	void start_datamode_bit_source(bool was_retraining);
	void start_qam_training();

	void reset_dpsk_rx();

	void tx_reversal_pause_rx();

	void cap_mp_rate_for_snr(double snr, std::string_view source);

	void tick_timeout();
	bool tick_timeout_retrain();
	void tick_timeout_pcm();
	void tick_timeout_phase2();
	void tick_timeout_phase3();
	void tick_timeout_phase4();
	void tick_timeout_retrain_quality();
	void handle_phase3_timeout();

	std::optional<wtvsoftmodem::v34::info::info1c_t> decode_info1c(bool full_recovery);

	inline bool state_elapsed(double ms) const
	{
		return m_state_samples >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(ms, m_sr);
	}
	inline bool state_elapsed_since(uint64_t mark, double ms) const
	{
		return m_state_samples - mark >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(ms, m_sr);
	}

	void configure_datamode(int32_t bit_rate_code, int32_t baud_rate_code, bool expanded_shaping);
	void setup_datamode_channel();

	uint32_t tx_phase2(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);
	uint32_t tx_qam_symbols(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);
	void     rx_ph2(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	void on_dpsk_bit(bool bit, bool reliable);
	void accept_peer_info0(wtvsoftmodem::v34::info::info0_t got);
	void recover_initial_info0();
	void on_qam_symbol(wtvsoftmodem_qam::csymbol_t sym);
	void rx_watch_short_retrain_sbar(wtvsoftmodem_qam::csymbol_t sym);
	void rx_watch_round2_trn(wtvsoftmodem_qam::csymbol_t sym);
	void rx_watch_data_variance(wtvsoftmodem_qam::csymbol_t sym);
	bool update_rx_power_variance(wtvsoftmodem_qam::csymbol_t sym);
	void rx_phase3_jwait(wtvsoftmodem_qam::csymbol_t sym);
	void rx_mp_symbol(wtvsoftmodem_qam::csymbol_t sym);
	void accept_peer_mp(const wtvsoftmodem::v34::info::mp_t &mp);
	void estimate_retrain_mp_channel(bool first_mp);
	void detect_diff4_marks(wtvsoftmodem_qam::csymbol_t sym, uint8_t (&bits)[2], uint32_t &run);
	void note_peer_sbar();
	bool keep_retrain_mp_soft() const
	{
		return m_full_retrain || (m_retraining && !m_v90_mode);
	}

	void ph2_queue_info0a();

	void queue_mp_bits(bool finish_frame = false);

	void begin_retrain(std::string_view reason);
	void begin_long_retrain(std::string_view reason);
	void begin_v90_retrain(bool requesting, std::string_view reason);

	void finish_rx_transitions();

	void check_retrain_quality();
	uint32_t rx_rate_ceiling() const;
	uint32_t v90_upstream_data_rate() const;
	void finish_retrain();

	void accept_peer_e();
	bool detect_peer_e(wtvsoftmodem_qam::csymbol_t sym);

	uint16_t detect_peer_j(bool bit);

	void install_data_slicer();

	int32_t shaper_variant() const;
	bool v34_baud_allowed(int32_t baud, bool high_carrier) const;
	int32_t select_v34_baud() const;
	void set_v34_symbol_rate(int32_t baud, bool high_carrier);
	void init_v34_qam();
	void narrow_training_rx();
	void recover_v90_info0();
	void cap_v90_upstream_rate();

	uint64_t v90_a_second_clock() const
	{
		return m_full_retrain ? m_state_samples : m_v90_p2_clock;
	}
	uint64_t v90_a_second_samples(double retrain_ms, double ms) const
	{
		return wtvsoftmodem_dsp::ms_to_samples<uint64_t>(m_full_retrain ? retrain_ms : ms, m_sr);
	}
	bool v90_a_second_silent() const
	{
		if (m_v90_wait_peer_probe && !m_full_retrain)
		{
			return !wtvsoftmodem_v34family::state_elapsed(V90_A_SECOND_SILENCE_MS);
		}
		else
		{
			const uint64_t clock = wtvsoftmodem_v34family::v90_a_second_clock();

			return clock >= wtvsoftmodem_v34family::v90_a_second_samples(V90_A_SECOND_SILENCE_START_RETRAIN_MS, V90_A_SECOND_SILENCE_START_MS)
				&& clock < wtvsoftmodem_v34family::v90_a_second_samples(V90_A_SECOND_SILENCE_END_RETRAIN_MS, V90_A_SECOND_SILENCE_END_MS);
		}
	}

	double data_timing_bw() const
	{
		return m_v90_mode && !m_v90_wait_peer_probe ? V90_DATA_TIMING_BW : V34_DATA_TIMING_BW;
	}
	double data_agc_rate() const
	{
		return m_retraining && !m_rx_frame_locked ? V34_TRN_AGC_RATE : V34_DATA_AGC_RATE;
	}

	void setup_v90_upstream(const wtvsoftmodem::v34::info::info1a_t &peer);
	void rx_v90_upstream(wtvsoftmodem_qam::csymbol_t symbol);
	void setup_v90_data_rx();
	void notify_v90_connected();
	uint32_t tx_v90_phase2(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);
	void rx_v90_bit(bool bit, bool reliable);
	void accept_v90_info1(const wtvsoftmodem::v34::info::info1a_t &peer);
	void pcm_state_changed(wtvsoftmodem_pcm::PCMState new_state);


	void setup_datamode_rx_channel();
	void setup_training_rx();
	void set_data_timing(bool seed);
	void hold_receiver_after_j();
	void start_datamode_rx_acquisition();
	void receive_datamode_symbol(wtvsoftmodem_qam::csymbol_t sym);

private:

	rx_byte_cb      m_rx_byte_cb;
	event_cb        m_connected_cb;
	event_cb        m_failed_cb;
	state_change_cb m_state_change_cb;};

#endif // MAME_WEBTV_WTVSOFTMODEM_V34FAMILY_H
