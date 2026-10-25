// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34TRAIN_H
#define MAME_WEBTV_WTVSOFTMODEM_V34TRAIN_H

#pragma once

#include "wtvsoftmodem_dsp.h"
#include "wtvsoftmodem_scrambler.h"

#include <complex>
#include <cstdint>

// ITU-T V.34 10.1.3 Phases 3 and 4 signals (S, PP, TRN, J, MP, E)

class wtvsoftmodem_v34train
{
public:

	struct point_t
	{
		double i = 0.0;
		double q = 0.0;
	};

	enum segment_t
	{
		SEG_NONE,
		SEG_S,
		SEG_SBAR,
		SEG_PP,
		SEG_TRN
	};

	struct config_t
	{
		double s_amplitude = 1.0;
		double trn_scale = 0.5;
		bool answer_mode = true;
	};

	struct detect_state_t
	{
		bool s_present = false;
		int32_t polarity = 0;
		bool reversal = false;
		double confidence = 0.0;
	};

	wtvsoftmodem_v34train();
	explicit wtvsoftmodem_v34train(const config_t &cfg);

	void init(const config_t &cfg);
	void reset();

	void set_trn_scale(double s)
	{
		m_cfg.trn_scale = s;
	}

	point_t gen_s(bool native_pattern = false);
	point_t gen_sbar(bool native_pattern = false);
	point_t gen_pp();
	point_t gen_trn();
	point_t gen_trn(bool d0, bool d1);
	point_t gen_trn_pattern(uint32_t bits);

	// V.34 Table 18 J patterns
	static constexpr uint16_t J_PATTERN_4_POINT  = 0x8990;
	static constexpr uint16_t J_PATTERN_16_POINT = 0x89b0;

	point_t gen_j();
	void set_j_pattern(uint16_t pattern) { m_j_pattern = pattern; }
	void j_reset()
	{
		m_j_bit = 0;
		m_j_diff = 0;
	}

	point_t gen_e();
	void e_reset()
	{
		m_e_bit = 0;
	}

	point_t gen_diff4(bool d0, bool d1);
	point_t gen_diff16(const uint8_t *bits);
	void set_tx_training16(bool enabled) { m_tx_training16 = enabled; }
	uint32_t tx_training_bits() const { return m_tx_training16 ? 4 : 2; }

	void mp_rx_reset();

	int32_t detect_diff4(point_t sym, uint8_t *d);
	int32_t detect_diff16(point_t sym, uint8_t *d);

	static point_t snap16(point_t sym);

	detect_state_t detect(point_t sym);

	void detect_trn_reset();
	int32_t detect_trn(point_t sym, uint8_t *bits_out);

	void trn_reset()
	{
		m_trn_scram.reset();
		m_trn_descram.reset();
	}

private:

	config_t m_cfg;
	uint16_t m_j_pattern = J_PATTERN_4_POINT;
	bool m_tx_training16 = false;

	int64_t m_s_n = 0;
	int64_t m_pp_n = 0;
	int32_t m_j_bit = 0;
	int32_t m_e_bit = 0;
	uint8_t m_j_diff = 0;
	uint8_t m_mp_rx_diff = 0;
	uint8_t m_mp_rx_diff16 = 0;
	wtvsoftmodem_scrambler m_mp_descram16;
	wtvsoftmodem_scrambler m_trn_scram;
	wtvsoftmodem_scrambler m_mp_descram;

	std::complex<double> m_alt_acc { 0.0, 0.0 };
	double m_pow_acc = 1e-9;
	int64_t m_det_n = 0;
	int32_t m_last_polarity = 0;

	wtvsoftmodem_scrambler m_trn_descram;

	point_t trn_point(uint32_t bits) const;
	point_t trn_point16(uint32_t bits) const;

public:

	uint64_t trn_scrambler_state() const
	{
		return m_trn_scram.state();
	}

	class retrain_s_detector
	{
	public:

		enum class RetrainSEvent
		{
			NONE,
			S,
			SBAR
		};

		void reset()
		{
			*this = {};
		}

		double confidence() const
		{
			return m_confidence;
		}

		RetrainSEvent put(double i, double q);

	private:

		bool m_confirmed = false;
		bool m_reversed = false;

		uint32_t m_count = 0;
		uint32_t m_run = 0;
		uint32_t m_reverse_run = 0;

		std::complex<double> m_sum {};
		std::complex<double> m_dc {};
		std::complex<double> m_phasor {};
		std::complex<double> m_mean {};
		std::complex<double> m_candidate_sum {};
		std::complex<double> m_candidate_dc {};

		double m_candidate_power = 0;
		double m_power = 0;
		double m_confidence = 0;
	};

	class retrain_tone_detector
	{
	public:

		void reset(double rate, double frequency = 1200);
		bool put(double sample);

	private:

		static constexpr uint32_t CONFIRM_BLOCKS = 11;
		static constexpr double TONE_PURITY = 0.85;
		static constexpr double RAW_PURITY = 0.30;

		wtvsoftmodem_dsp::goertzel_state_t m_bin = wtvsoftmodem_dsp::goertzel_init(1200, 16000, 80, false);
		wtvsoftmodem_dsp::bandpass_state_t m_filter = wtvsoftmodem_dsp::bandpass_init(1200, 16000, 8);
		wtvsoftmodem_dsp::goertzel_state_t m_raw_bin = m_bin;

		double m_power = 0;
		uint32_t m_run = 0;
		double m_raw_power = 0;
		double m_raw_periodic_power = 0;

		double m_periodic_power = 0;
		std::complex<double> m_periodic_sum {};
		std::complex<double> m_raw_periodic_sum {};
	};

	static bool retrain_bad_window(uint32_t good, uint32_t bad)
	{
		return (bad >= 3 && 20ULL * bad >= uint64_t(good) + bad) || (!good && bad >= 2);
	}

	static uint32_t retrain_sustainable_rate(double snr_db, uint32_t baud, bool expanded_shaping, bool nonlinear, bool precoding = false, bool aux = false);

private:

	// Table pulled from AltoCom
	//
	// This finds the minimum SNR given a number of parameters. Mostly arbitrary (no hard science binding them to these values).
	// The final decision to retrain is based on if the current SNR drops below the selected SNR in this table and on V.42 frame CRC error count.
	//
	// RETRAIN_RATE_THRESHOLD
	//   [baud rate index]
	//       0 = 2400 baud
	//       1 = 2743 baud
	//       2 = 2800 baud
	//       3 = 3000 baud
	//       4 = 3200 baud
	//       5 = 3429 baud
	//       6 = reserved
	//   [bit rate index]
	//       0 =  2400 bits/s
	//       1 =  4800 bits/s
	//       2 =  7200 bits/s
	//       3 =  9600 bits/s
	//       4 = 12000 bits/s
	//       5 = 14400 bits/s
	//       6 = 16800 bits/s
	//       7 = 19200 bits/s
	//       8 = 21600 bits/s
	//       9 = 24000 bits/s
	//      10 = 26400 bits/s
	//      11 = 28800 bits/s
	//      12 = 31200 bits/s
	//      13 = 33600 bits/s
	//   [modulation option group index]
	//          group = (auxiliary_channel ? 4 : 0)
	//                + (expanded_shaping  ? 2 : 0)
	//                + (precoding         ? 1 : 0);
	//       0 = auxillary channel disaled, expanded shaping disabled, precoding disabled
	//       1 = auxillary channel disaled, expanded shaping disabled, precoding enabled
	//       2 = auxillary channel disaled, expanded shaping enabled, precoding disabled
	//       3 = auxillary channel disaled, expanded shaping enabled, precoding enabled
	//       4 = auxillary channel enabled, expanded shaping disabled, precoding disabled
	//       5 = auxillary channel enabled, expanded shaping disabled, precoding enabled
	//       6 = auxillary channel enabled, expanded shaping enabled, precoding disabled
	//       7 = auxillary channel enabled, expanded shaping enabled, precoding enabled
	//   [nonlinear state]
	//       0 = disabled
	//       1 = enabled
	static constexpr uint8_t RETRAIN_RATE_THRESHOLD[7][14][8][2] = {
		// 2400 baud
		{
			// 2400 bits/s
			{ {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 } },
			// 4800 bits/s
			{ {  46,  49 }, {  49,  52 }, {  46,  49 }, {  49,  52 }, {  47,  50 }, {  50,  53 }, {  47,  50 }, {  50,  53 } },
			// 7200 bits/s
			{ {  58,  60 }, {  59,  62 }, {  57,  60 }, {  59,  61 }, {  58,  61 }, {  60,  62 }, {  58,  61 }, {  60,  62 } },
			// 9600 bits/s
			{ {  70,  73 }, {  71,  73 }, {  69,  72 }, {  70,  73 }, {  70,  73 }, {  71,  74 }, {  70,  72 }, {  71,  73 } },
			// 12000 bits/s
			{ {  82,  85 }, {  82,  85 }, {  81,  84 }, {  81,  84 }, {  83,  85 }, {  83,  86 }, {  82,  85 }, {  82,  85 } },
			// 14400 bits/s
			{ {  94,  97 }, {  94,  97 }, {  93,  96 }, {  93,  96 }, {  95,  98 }, {  95,  98 }, {  94,  97 }, {  94,  97 } },
			// 16800 bits/s
			{ { 106, 109 }, { 107, 109 }, { 105, 108 }, { 105, 108 }, { 107, 109 }, { 107, 110 }, { 106, 109 }, { 106, 109 } },
			// 19200 bits/s
			{ { 118, 121 }, { 118, 121 }, { 117, 120 }, { 117, 120 }, { 119, 122 }, { 119, 122 }, { 118, 121 }, { 118, 121 } },
			// 21600 bits/s
			{ { 130, 133 }, { 130, 133 }, { 129, 132 }, { 129, 132 }, { 131, 134 }, { 131, 134 }, { 130, 133 }, { 130, 133 } },
			// 24000 bits/s
			{ { 142, 145 }, { 142, 145 }, {   0,   0 }, {   0,   0 }, { 143, 146 }, { 143, 146 }, {   0,   0 }, {   0,   0 } },
			// 26400 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 28800 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 31200 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 33600 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } }
		},
		// 2743 baud
		{
			// 2400 bits/s
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 } },
			// 4800 bits/s
			{ {  42,  46 }, {  46,  49 }, {  42,  46 }, {  46,  49 }, {  43,  46 }, {  47,  49 }, {  43,  46 }, {  47,  49 } },
			// 7200 bits/s
			{ {  53,  56 }, {  55,  58 }, {  53,  56 }, {  55,  58 }, {  54,  56 }, {  56,  58 }, {  54,  56 }, {  56,  58 } },
			// 9600 bits/s
			{ {  65,  67 }, {  66,  68 }, {  63,  66 }, {  64,  67 }, {  64,  67 }, {  66,  68 }, {  64,  67 }, {  66,  68 } },
			// 12000 bits/s
			{ {  74,  77 }, {  75,  77 }, {  74,  76 }, {  74,  77 }, {  76,  78 }, {  76,  79 }, {  74,  77 }, {  75,  78 } },
			// 14400 bits/s
			{ {  85,  88 }, {  86,  88 }, {  84,  87 }, {  84,  87 }, {  86,  89 }, {  86,  89 }, {  85,  88 }, {  85,  88 } },
			// 16800 bits/s
			{ {  96,  98 }, {  96,  98 }, {  94,  97 }, {  95,  97 }, {  96,  99 }, {  97,  99 }, {  95,  98 }, {  95,  98 } },
			// 19200 bits/s
			{ { 106, 109 }, { 107, 109 }, { 105, 108 }, { 105, 108 }, { 107, 109 }, { 107, 110 }, { 106, 109 }, { 106, 109 } },
			// 21600 bits/s
			{ { 117, 119 }, { 117, 119 }, { 115, 118 }, { 116, 118 }, { 117, 120 }, { 117, 120 }, { 116, 119 }, { 117, 119 } },
			// 24000 bits/s
			{ { 127, 130 }, { 127, 130 }, { 126, 129 }, { 126, 129 }, { 128, 130 }, { 128, 130 }, { 127, 130 }, { 127, 130 } },
			// 26400 bits/s
			{ { 138, 141 }, { 138, 141 }, { 137, 139 }, { 137, 139 }, { 138, 141 }, { 138, 141 }, { 137, 140 }, { 137, 140 } },
			// 28800 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 31200 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 33600 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } }
		},
		// 2800 baud
		{
			// 2400 bits/s
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 } },
			// 4800 bits/s
			{ {  42,  45 }, {  46,  49 }, {  42,  45 }, {  46,  49 }, {  43,  46 }, {  46,  49 }, {  43,  46 }, {  46,  49 } },
			// 7200 bits/s
			{ {  52,  55 }, {  54,  57 }, {  52,  55 }, {  54,  57 }, {  53,  56 }, {  55,  58 }, {  53,  56 }, {  55,  58 } },
			// 9600 bits/s
			{ {  63,  66 }, {  65,  67 }, {  62,  65 }, {  64,  66 }, {  65,  67 }, {  66,  68 }, {  63,  66 }, {  64,  67 } },
			// 12000 bits/s
			{ {  73,  75 }, {  73,  76 }, {  72,  75 }, {  73,  76 }, {  74,  76 }, {  74,  77 }, {  73,  76 }, {  74,  77 } },
			// 14400 bits/s
			{ {  83,  86 }, {  84,  86 }, {  82,  85 }, {  83,  86 }, {  85,  87 }, {  85,  88 }, {  83,  86 }, {  84,  87 } },
			// 16800 bits/s
			{ {  94,  97 }, {  94,  97 }, {  93,  96 }, {  93,  96 }, {  95,  97 }, {  95,  98 }, {  94,  97 }, {  94,  97 } },
			// 19200 bits/s
			{ { 104, 107 }, { 104, 107 }, { 103, 106 }, { 103, 106 }, { 105, 107 }, { 105, 108 }, { 104, 107 }, { 104, 107 } },
			// 21600 bits/s
			{ { 115, 117 }, { 115, 117 }, { 113, 116 }, { 114, 117 }, { 115, 118 }, { 115, 118 }, { 114, 117 }, { 114, 117 } },
			// 24000 bits/s
			{ { 125, 128 }, { 125, 128 }, { 124, 127 }, { 124, 127 }, { 125, 128 }, { 125, 128 }, { 124, 127 }, { 125, 128 } },
			// 26400 bits/s
			{ { 136, 138 }, { 136, 138 }, { 134, 137 }, { 134, 137 }, { 137, 140 }, { 138, 140 }, { 135, 138 }, { 135, 138 } },
			// 28800 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 31200 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 33600 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } }
		},
		// 3000 baud
		{
			// 2400 bits/s
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 4800 bits/s
			{ {  40,  43 }, {  44,  47 }, {  40,  43 }, {  44,  47 }, {  41,  44 }, {  45,  48 }, {  41,  44 }, {  45,  48 } },
			// 7200 bits/s
			{ {  53,  56 }, {  55,  58 }, {  53,  56 }, {  55,  58 }, {  55,  58 }, {  57,  60 }, {  55,  58 }, {  57,  60 } },
			// 9600 bits/s
			{ {  59,  62 }, {  61,  64 }, {  59,  62 }, {  61,  64 }, {  60,  63 }, {  62,  64 }, {  60,  63 }, {  62,  64 } },
			// 12000 bits/s
			{ {  70,  73 }, {  71,  73 }, {  69,  72 }, {  70,  73 }, {  70,  73 }, {  71,  74 }, {  69,  72 }, {  70,  73 } },
			// 14400 bits/s
			{ {  79,  82 }, {  80,  82 }, {  78,  81 }, {  79,  82 }, {  80,  83 }, {  81,  83 }, {  79,  82 }, {  80,  82 } },
			// 16800 bits/s
			{ {  90,  92 }, {  90,  92 }, {  88,  91 }, {  88,  91 }, {  90,  92 }, {  90,  92 }, {  89,  92 }, {  89,  92 } },
			// 19200 bits/s
			{ {  99, 102 }, {  99, 102 }, {  98, 100 }, {  98, 101 }, { 101, 103 }, { 101, 103 }, {  98, 101 }, {  99, 101 } },
			// 21600 bits/s
			{ { 108, 111 }, { 109, 111 }, { 107, 110 }, { 108, 110 }, { 109, 112 }, { 109, 112 }, { 108, 111 }, { 108, 111 } },
			// 24000 bits/s
			{ { 118, 121 }, { 118, 121 }, { 117, 120 }, { 117, 120 }, { 119, 121 }, { 119, 121 }, { 118, 121 }, { 118, 121 } },
			// 26400 bits/s
			{ { 127, 130 }, { 127, 130 }, { 126, 129 }, { 127, 129 }, { 129, 131 }, { 129, 131 }, { 127, 130 }, { 127, 130 } },
			// 28800 bits/s
			{ { 138, 140 }, { 138, 140 }, { 136, 139 }, { 136, 139 }, { 138, 140 }, { 138, 140 }, { 137, 140 }, { 137, 140 } },
			// 31200 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 33600 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } }
		},
		// 3200 baud
		{
			// 2400 bits/s
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 4800 bits/s
			{ {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 }, {  39,  41 }, {  43,  46 }, {  39,  41 }, {  43,  46 } },
			// 7200 bits/s
			{ {  50,  53 }, {  53,  55 }, {  50,  53 }, {  53,  55 }, {  51,  54 }, {  54,  56 }, {  51,  54 }, {  54,  56 } },
			// 9600 bits/s
			{ {  58,  60 }, {  59,  62 }, {  57,  60 }, {  59,  61 }, {  58,  60 }, {  59,  62 }, {  58,  60 }, {  59,  62 } },
			// 12000 bits/s
			{ {  68,  70 }, {  69,  71 }, {  66,  69 }, {  67,  70 }, {  67,  70 }, {  68,  71 }, {  67,  70 }, {  68,  71 } },
			// 14400 bits/s
			{ {  77,  79 }, {  77,  80 }, {  75,  78 }, {  76,  78 }, {  77,  80 }, {  77,  80 }, {  76,  78 }, {  76,  79 } },
			// 16800 bits/s
			{ {  85,  88 }, {  86,  88 }, {  84,  87 }, {  84,  87 }, {  86,  88 }, {  86,  89 }, {  85,  87 }, {  85,  88 } },
			// 19200 bits/s
			{ {  94,  97 }, {  94,  97 }, {  93,  96 }, {  93,  96 }, {  95,  97 }, {  95,  97 }, {  94,  96 }, {  94,  97 } },
			// 21600 bits/s
			{ { 103, 106 }, { 103, 106 }, { 102, 105 }, { 102, 105 }, { 103, 106 }, { 104, 106 }, { 103, 105 }, { 103, 105 } },
			// 24000 bits/s
			{ { 113, 116 }, { 114, 116 }, { 111, 114 }, { 111, 114 }, { 113, 115 }, { 113, 116 }, { 112, 114 }, { 112, 115 } },
			// 26400 bits/s
			{ { 121, 124 }, { 122, 124 }, { 120, 123 }, { 120, 123 }, { 122, 124 }, { 122, 125 }, { 121, 123 }, { 121, 124 } },
			// 28800 bits/s
			{ { 130, 133 }, { 130, 133 }, { 129, 132 }, { 129, 132 }, { 131, 133 }, { 131, 133 }, { 130, 133 }, { 130, 133 } },
			// 31200 bits/s
			{ { 139, 142 }, { 139, 142 }, { 138, 141 }, { 138, 141 }, { 140, 142 }, { 140, 142 }, { 139, 142 }, { 139, 142 } },
			// 33600 bits/s (not possible)
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } }
		},
		// 3429 baud
		{
			// 2400 bits/s
			{ {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 }, {   0,   0 } },
			// 4800 bits/s
			{ {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 }, {  37,  39 }, {  42,  44 } },
			// 7200 bits/s
			{ {  47,  51 }, {  50,  53 }, {  47,  51 }, {  50,  53 }, {  48,  51 }, {  51,  54 }, {  48,  51 }, {  51,  54 } },
			// 9600 bits/s
			{ {  55,  58 }, {  57,  59 }, {  55,  58 }, {  57,  59 }, {  56,  58 }, {  58,  60 }, {  56,  58 }, {  58,  60 } },
			// 12000 bits/s
			{ {  65,  67 }, {  66,  68 }, {  63,  66 }, {  64,  67 }, {  64,  67 }, {  65,  68 }, {  64,  67 }, {  65,  68 } },
			// 14400 bits/s
			{ {  72,  75 }, {  73,  76 }, {  71,  74 }, {  72,  75 }, {  72,  75 }, {  73,  75 }, {  72,  75 }, {  73,  75 } },
			// 16800 bits/s
			{ {  80,  83 }, {  81,  83 }, {  80,  82 }, {  80,  83 }, {  81,  84 }, {  82,  84 }, {  80,  83 }, {  81,  84 } },
			// 19200 bits/s
			{ {  90,  92 }, {  90,  92 }, {  88,  91 }, {  88,  91 }, {  89,  92 }, {  90,  92 }, {  89,  92 }, {  89,  92 } },
			// 21600 bits/s
			{ {  98, 100 }, {  98, 100 }, {  96,  99 }, {  97,  99 }, {  99, 101 }, {  99, 101 }, {  97, 100 }, {  97, 100 } },
			// 24000 bits/s
			{ { 106, 109 }, { 107, 109 }, { 105, 108 }, { 105, 108 }, { 106, 109 }, { 107, 109 }, { 106, 108 }, { 106, 109 } },
			// 26400 bits/s
			{ { 114, 117 }, { 115, 117 }, { 113, 116 }, { 114, 116 }, { 115, 117 }, { 115, 117 }, { 114, 117 }, { 114, 117 } },
			// 28800 bits/s
			{ { 123, 126 }, { 123, 126 }, { 122, 124 }, { 122, 125 }, { 124, 127 }, { 125, 127 }, { 122, 125 }, { 123, 125 } },
			// 31200 bits/s
			{ { 131, 134 }, { 131, 134 }, { 130, 133 }, { 130, 133 }, { 132, 134 }, { 132, 134 }, { 131, 134 }, { 131, 134 } },
			// 33600 bits/s
			{ { 139, 142 }, { 139, 142 }, { 138, 141 }, { 139, 141 }, { 140, 143 }, { 140, 143 }, { 139, 142 }, { 139, 142 } }
		}
	};
};

#endif // MAME_WEBTV_WTVSOFTMODEM_V34TRAIN_H
