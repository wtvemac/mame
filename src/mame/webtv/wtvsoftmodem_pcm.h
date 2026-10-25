// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_PCM_H
#define MAME_WEBTV_WTVSOFTMODEM_PCM_H
#pragma once

#include "wtvsoftmodem_dsp.h"

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

// ITU-T V.90 5 Digital modem, 8.4 and 8.6 Digital modem start-up signals

class wtvsoftmodem_pcm
{
public:

	class mapper
	{
	public:

		struct sign_state
		{
			uint32_t word = 0;
			uint32_t changed = 0;
			uint32_t difference = 0;
			uint32_t trellis = 0;
		};
	
		void set_alaw(bool alaw)
		{
			m_alaw = alaw;
		}
	
		bool configure(const std::vector<uint8_t> &cp, const std::vector<uint8_t> *shaping = nullptr);
	
		void reset();
	
		uint32_t magnitude_bits() const
		{
			return m_k;
		}
	
		uint32_t sign_bits() const
		{
			return m_s;
		}
	
		uint32_t frame_bits() const
		{
			return m_k + m_s;
		}
	
		const std::array<std::vector<uint8_t>, 6> &constellations() const
		{
			return m_codes;
		}

		static uint32_t recover_signs(uint32_t word, uint32_t s, sign_state &st);
		std::array<int16_t, 6> encode(uint64_t data);

	private:

		std::array<std::vector<uint8_t>, 6> m_codes;

		uint32_t m_k = 0;
		uint32_t m_s = 6;
		bool m_alaw = false;

		sign_state m_sign {};

		std::array<double, 4> m_filter {};

		double m_x1 = 0;
		double m_x2 = 0;
		double m_y1 = 0;
		double m_y2 = 0;
	};

	enum class PCMState
	{
		IDLE,
		WAIT_JA,
		S,
		SBAR,
		TRN1D,
		JD,
		JDPRIME,
		DIL,
		RI,
		RIBAR,
		TRN2D,
		MP,
		ED,
		B1,
		DATA,
		RENEG_WAIT,
		RD,
		RDBAR,
		FAILED
	};

	wtvsoftmodem_pcm();

	void start(wtvsoftmodem_dsp::frequency_t sample_rate, uint8_t training_ucode);
	void stop()
	{
		set_state(PCMState::IDLE);
	}

	uint32_t tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	typedef std::function<void(PCMState, PCMState)> state_change_cb;

	static std::string pcm_state_to_string(PCMState state)
	{
		switch (state)
		{
			case PCMState::IDLE:
				return "IDLE";
			case PCMState::WAIT_JA:
				return "WAIT_JA";
			case PCMState::S:
				return "S";
			case PCMState::SBAR:
				return "SBAR";
			case PCMState::TRN1D:
				return "TRN1D";
			case PCMState::JD:
				return "JD";
			case PCMState::JDPRIME:
				return "JDPRIME";
			case PCMState::DIL:
				return "DIL";
			case PCMState::RI:
				return "RI";
			case PCMState::RIBAR:
				return "RIBAR";
			case PCMState::TRN2D:
				return "TRN2D";
			case PCMState::MP:
				return "MP";
			case PCMState::ED:
				return "ED";
			case PCMState::B1:
				return "B1";
			case PCMState::DATA:
				return "DATA";
			case PCMState::RENEG_WAIT:
				return "RENEG_WAIT";
			case PCMState::RD:
				return "RD";
			case PCMState::RDBAR:
				return "RDBAR";
			case PCMState::FAILED:
				return "FAILED";
			default:
				return "UNKNOWN";
		}
	}

	void set_state_change_callback(state_change_cb cb)
	{
		m_state_change_cb = std::move(cb);
	}

	PCMState state() const
	{
		return m_state;
	}

	void set_alaw(bool alaw)
	{
		m_alaw = alaw;
		m_mapper.set_alaw(alaw);
	}
	bool alaw() const
	{
		return m_alaw;
	}

	uint64_t pcm_symbols() const
	{
		return m_symbol;
	}

	void set_upstream_rate_cap(uint32_t rate)
	{
		m_upstream_rate_cap = rate < 2 ? 2 : rate > 14 ? 14 : rate;
	}
	uint32_t upstream_rate_cap() const
	{
		return m_upstream_rate_cap;
	}

	uint32_t peer_upstream_rate_max() const
	{
		if (m_cp.size() < 4)
			return 0;

		return uint32_t(std::bit_width(uint32_t(m_cp[2] & 0xfe) | uint32_t(m_cp[3] & 0x3f) << 8));
	}

	void set_bit_source(std::function<int32_t()> source)
	{
		m_bit_source = std::move(source);
	}

	void set_downstream_rate_cap(uint32_t bps)
	{
		uint32_t rates = 0;
		if (bps >= 28000)
			rates = std::min<uint32_t>(22, ((bps - 28000) * 6 + 4000) / 8000 + 1);

		m_downstream_rates = (1u << rates) - 1;
	}

	void set_wait_peer_cp(bool enabled)
	{
		m_wait_peer_cp = enabled;
	}

	void set_symbol_observer(std::function<void(int16_t)> observer)
	{
		m_symbol_observer = std::move(observer);
	}

	void set_data_callback(std::function<void()> callback)
	{
		m_data_callback = std::move(callback);
	}

	uint32_t data_rate() const
	{
		return m_mapper.frame_bits() * 8000 / 6;
	}

	bool upstream_s_expected() const
	{
		switch (m_state)
		{
			case PCMState::JD:
			case PCMState::JDPRIME:
			case PCMState::DIL:
				return true;

			default:
				return false;
		}
	}

	bool cp_expected() const
	{
		switch (m_state)
		{
			case PCMState::DIL:
			case PCMState::RI:
			case PCMState::RIBAR:
			case PCMState::TRN2D:
			case PCMState::MP:
			case PCMState::RENEG_WAIT:
			case PCMState::RD:
			case PCMState::RDBAR:
				return true;

			default:
				return false;
		}
	}

	static int16_t mulaw_decode(uint8_t octet);
	static uint8_t mulaw_encode(int16_t sample);
	static int16_t alaw_decode(uint8_t octet);
	static uint8_t alaw_encode(int16_t sample);

	void begin_pcm_probe(wtvsoftmodem_dsp::frequency_t sample_rate);
	void quantize_probe(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	static std::vector<uint8_t> build_parameter_frame(const std::vector<uint8_t> &payload);

	static int16_t ucode_level(uint8_t ucode, bool alaw = false);
	static uint8_t scramble_byte(uint8_t input, uint32_t &state);

	static bool parse_ja(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, uint32_t *resume = nullptr);
	void accept_ja(const std::vector<uint8_t> &payload);

	void observe_upstream_s(bool reversal);

	bool request_retrain();


	static bool parse_cp(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, size_t *consumed = nullptr, uint32_t *resume = nullptr);
	void accept_cp(const std::vector<uint8_t> &payload);


private:

	static constexpr uint64_t SYM_S = 64 * 6;
	static constexpr uint64_t SYM_SBAR = 8 * 6;
	static constexpr uint64_t SYM_TRN1D = 31992;
	static constexpr uint64_t SYM_JDPRIME = 12;
	static constexpr uint64_t SYM_RD = 64 * 6;
	static constexpr uint64_t SYM_RDBAR = 4 * 6;
	static constexpr uint64_t SYM_RI_MIN = 192;
	static constexpr uint64_t SYM_RIBAR = 4 * 6;
	static constexpr uint64_t SYM_TRN2D = 12000;
	static constexpr uint64_t SYM_TRN2D_HOLD_MAX = 8 * 8000;
	static constexpr uint64_t SYM_ED = 12;
	static constexpr uint64_t SYM_MP_ACK_MIN = 1600;
	static constexpr uint32_t MP_ACK_MIN_FRAMES = 2; 

	// Jd payload bits (V.90 Table 13)
	static constexpr uint32_t JD_RATE_MASK = 0x3fffff;
	static constexpr uint32_t JD_TRAIN_16_POINT = 1u << 28;
	static constexpr uint32_t JD_RENEG_16_POINT = 1u << 29;
	static constexpr uint32_t JD_LOOKAHEAD_SHIFT = 30;
	static constexpr uint64_t SYM_B1 = 288;

	int16_t pcm_level(uint8_t ucode) const
	{
		return ucode_level(ucode, m_alaw);
	}

	template <typename Length>
	static bool parse_parameter_frame(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, size_t *consumed, uint32_t *resume, Length length_of);

	std::vector<uint8_t> m_ja;
	std::vector<uint8_t> m_parameter;
	std::vector<uint8_t> m_cp;
	std::vector<uint8_t> m_data_cp;
	std::vector<uint8_t> m_previous_data_cp;
	std::function<int32_t()> m_bit_source;
	std::function<void()> m_data_callback;
	std::function<void(int16_t)> m_symbol_observer;
	bool m_wait_peer_cp = false;
	uint32_t m_downstream_rates = 0x3fffff;
	size_t m_parameter_index = 0;
	bool m_mp_ack = false;
	bool m_sent_mp_ack = false;
	uint32_t m_sent_mp_ack_frames = 0;
	bool m_peer_cp_ack = false;
	mapper m_mapper;
	std::array<int16_t, 6> m_frame{};
	uint32_t m_frame_index = 6;
	std::array<int16_t, 6> m_reneg_levels{};
	bool m_dil_complete = false;
	bool m_mapper_ready = false;
	bool m_upstream_s = false;
	uint32_t m_dil_segment = 0;
	uint32_t m_upstream_reversals = 0;
	PCMState m_state = PCMState::IDLE;
	state_change_cb m_state_change_cb;
	wtvsoftmodem_dsp::frequency_t m_sample_rate = 16000.0;
	double m_clock = 0.0;
	uint64_t m_symbol = 0;
	uint64_t m_stage_symbol = 0;
	uint8_t m_training_ucode = 0;
	bool m_alaw = false;
	int16_t m_level = 0;
	uint32_t m_scrambler = 0;
	uint8_t m_bits = 0;
	uint32_t m_bit_index = 8;
	uint32_t m_data_sign = 0;
	uint32_t m_upstream_rate_cap = 14;
	std::array<double, 65> m_filter {};
	wtvsoftmodem_dsp::delay_line<double> m_history { m_filter.size() };

	void set_state(PCMState next);
	void enter_state(PCMState next);
	void enter_trn2d();
	bool advance_state();
	int16_t next_symbol();
	int16_t next_dil_symbol();
	int16_t next_mapped_symbol();

	void prepare_mp(bool acknowledge);

	void init_reconstruction(wtvsoftmodem_dsp::frequency_t sample_rate);

	int32_t reconstruct(double value);
};
#endif // MAME_WEBTV_WTVSOFTMODEM_PCM_H
