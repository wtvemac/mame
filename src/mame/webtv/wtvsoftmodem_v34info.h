// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34INFO_H
#define MAME_WEBTV_WTVSOFTMODEM_V34INFO_H

#pragma once

#include "wtvsoftmodem_dpsk.h"

#include <array>
#include <cstdint>
#include <vector>

// ITU-T V.34 10.1.2.3 INFO sequences, ITU-T V.90 8.2.3 INFO sequences

class wtvsoftmodem_v34dpsk : public wtvsoftmodem_dpsk
{

public:

	wtvsoftmodem_v34dpsk()
	{
		init(DPSK_V34_INFO);
	}

	virtual void reset() override;
	virtual void rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count) override;

	std::vector<uint8_t> rx_decode_buffered(double rate_trim, double phase0 = 0.0) const;

private:

	std::vector<wtvsoftmodem_dsp::sample_t> m_rx_history;
	static constexpr size_t HISTORY_CAP = 2000000;
};

namespace wtvsoftmodem::v34::info
{
	uint16_t crc16_bits(const uint8_t *bits, uint32_t nbits, uint16_t crc = 0xffff);

	// V.90 Table 7: INFO0d bits 29:41 (v90_extension)
	static constexpr int32_t  INFO0D_EXTENSION_BITS      = 13;
	static constexpr uint16_t INFO0D_EXTENSION_MASK      = (1 << INFO0D_EXTENSION_BITS) - 1;
	static constexpr uint16_t INFO0D_NOMINAL_POWER_SHIFT = 0;                                    // Bits 29:32: Phase 2 power, -1 dBm0 steps from -6 dBm0
	static constexpr uint16_t INFO0D_NOMINAL_POWER_MASK  = 0x00f << INFO0D_NOMINAL_POWER_SHIFT;
	static constexpr uint16_t INFO0D_MAX_POWER_SHIFT     = 4;                                    // Bits 33:37: max power, -0.5 dBm0 steps from -0.5 dBm0
	static constexpr uint16_t INFO0D_MAX_POWER_MASK      = 0x01f << INFO0D_MAX_POWER_SHIFT;
	static constexpr uint16_t INFO0D_POWER_AT_CODEC      = 1 << 9;                               // Bit 38: power measured at the codec output
	static constexpr uint16_t INFO0D_ALAW                = 1 << 10;                              // Bit 39: 0 = u-law, 1 = a-law
	static constexpr uint16_t INFO0D_UPSTREAM_3429       = 1 << 11;                              // Bit 40: V.90 upstream at 3429 baud
	static constexpr uint16_t INFO0D_DEFAULT_EXTENSION   = (4 << INFO0D_NOMINAL_POWER_SHIFT)     // -10 dBm0
	                                                         | (17 << INFO0D_MAX_POWER_SHIFT)    // -9 dBm0
	                                                         | INFO0D_UPSTREAM_3429;

	// V.34 Table 12: INFO0 bits 26:27 (tx_clock_source)
	static constexpr uint8_t INFO0_TX_CLOCK_INTERNAL  = 0;
	static constexpr uint8_t INFO0_TX_CLOCK_RX_TIMING = 1; // Synchronized to receive timing
	static constexpr uint8_t INFO0_TX_CLOCK_EXTERNAL  = 2;

	struct info0_t
	{
		bool v90_raw = false;
		bool v90_digital = false;
		uint16_t v90_extension = INFO0D_DEFAULT_EXTENSION;
		bool is_answer = false;

		bool sr_2743 = true;
		bool sr_2800 = true;
		bool sr_3429 = true;

		bool sr_3000_low = true;
		bool sr_3000_high = true;
		bool sr_3200_low = true;
		bool sr_3200_high = true;
		bool rate_3429_allowed = true;
		bool power_reduction = false;
		uint8_t max_baud_diff = 0;
		bool from_cme_modem = false;
		bool support_1664 = false;
		uint8_t tx_clock_source = INFO0_TX_CLOCK_INTERNAL;
		bool ack = false;
	};

	std::vector<uint8_t> build_info0(const info0_t &in);

	bool parse_info0(const uint8_t *bits, uint32_t nbits, info0_t &out, uint32_t *consumed = nullptr, bool digital_answer = false);

	static constexpr uint16_t SYNC10 = 0x372;

	static constexpr int32_t INFO0_PAYLOAD_BITS  = 17;
	static constexpr int32_t INFO1A_PAYLOAD_BITS = 38;
	static constexpr int32_t INFO1C_PAYLOAD_BITS = 77;

	static constexpr int32_t MP0_LEN    = 88;
	static constexpr int32_t MP0_GROUPS = 3;

	static constexpr int32_t MP1_LEN    = 190;
	static constexpr int32_t MP1_GROUPS = 9;
	static constexpr int32_t MP_SYNC_BITS = 18;
	static constexpr int32_t MP_GROUP_BITS = 17;
	static constexpr int32_t MP_GROUP_DATA_BITS = 16;
	static constexpr int32_t MP_CRC_BITS = 16;

	struct info1a_t
	{
		bool pcm_downstream = false;
		uint8_t training_ucode = 0x34;
		uint8_t power_reduction = 0;
		uint8_t add_power_reduction = 0;
		uint8_t md = 0;
		bool use_high_carrier = false;
		uint8_t preemphasis_filter = 6;
		uint8_t max_data_rate = 14;
		uint8_t baud_rate_a_to_c = 5;
		uint8_t baud_rate_c_to_a = 5;
		int16_t freq_offset = -512;
	};

	std::vector<uint8_t> build_info1a(const info1a_t &in);
	bool parse_info1a(const uint8_t *bits, uint32_t nbits, info1a_t &out, uint32_t *consumed = nullptr);

	struct info1c_rate_t
	{
		bool use_high_carrier = false;
		uint8_t pre_emphasis = 0;
		uint8_t max_bit_rate = 0;
	};

	struct info1c_t
	{
		uint8_t power_reduction = 0;
		uint8_t add_power_reduction = 0;
		uint8_t md = 0;
		info1c_rate_t rate[6];
		int16_t freq_offset = -512;
	};

	struct mp_t
	{
		bool v90_digital = false;
		uint16_t v90_extension = INFO0D_DEFAULT_EXTENSION;
		bool is_answer = false;

		uint8_t type = 1;

		bool precoding_enabled = false;
		uint8_t bit_rate_c_to_a = 14;
		uint8_t bit_rate_a_to_c = 14;
		bool aux_channel = false;
		uint8_t trellis_sel = 0;
		bool nonlinear_encoder = false;
		bool expanded_shaping = false;
		bool mp_acknowledged = false;
		uint16_t signalling_rate_mask = 0x3fff;
		bool asymmetric_rates = false;

		int16_t precoder_c3_real = 0;

		std::array<uint8_t, 96> long_ext{};
	};

	class info1c_settle
	{
	public:
	
		void reset();

		bool update(const std::vector<uint8_t> &reliable, uint64_t now, bool probe_ended, uint64_t probe_end_sample, wtvsoftmodem_dsp::frequency_t sample_rate);

	private:

		static constexpr size_t RELIABLE_WINDOW = 60;
		static constexpr double RELIABLE_FRAC = 0.70;
		static constexpr double CLEAN_SUSTAIN_MS = 500.0;
		static constexpr double SETTLE_MAX_MS = 2000.0;

		bool m_clean_now = false;
		uint64_t m_clean_since_sample = 0;
	};

	using precoder_t = std::array<std::array<int16_t, 2>, 3>;

	std::vector<uint8_t> build_info1c(const info1c_t &in);

	bool parse_info1c(const uint8_t *bits, uint32_t nbits, info1c_t &out, uint32_t *consumed = nullptr);

	bool parse_info1c_erasure(const uint8_t *bits, const uint8_t *reliable, uint32_t nbits, info1c_t &out, uint32_t *consumed = nullptr, uint32_t max_flip_bits = 6);

	inline uint8_t mp_selected_rate(const mp_t &local, const mp_t &peer, bool c_to_a)
	{
		const uint8_t ca = std::min(local.bit_rate_c_to_a, peer.bit_rate_c_to_a);
		const uint8_t ac = std::min(local.bit_rate_a_to_c, peer.bit_rate_a_to_c);

		uint8_t rate;
		if (local.asymmetric_rates && peer.asymmetric_rates)
			rate = std::min<uint8_t>(
				14,
				c_to_a ? ca : ac
			);
		else
			rate = std::min<uint8_t>(
				14,
				std::min(ca, ac)
			);

		// V.34 11.4.1: choose the highest rate enabled by both modems
		const uint16_t enabled = local.signalling_rate_mask & peer.signalling_rate_mask;
		while (rate && !(enabled & (1 << (rate - 1))))
			--rate;

		return rate;
	}

	precoder_t mp_precoder(const mp_t &mp);

	bool parse_mp(const uint8_t *bits, uint32_t nbits, mp_t &out, uint32_t *consumed = nullptr, uint32_t *resume = nullptr);

	const std::array<uint8_t, 96> &mp_long_ext_template();
	const std::array<uint8_t, 96> &mp_long_ext_identity();
	std::vector<uint8_t> build_mp(const mp_t &in);

}

#endif // MAME_WEBTV_WTVSOFTMODEM_V34INFO_H
