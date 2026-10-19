// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_SCRAMBLER_H
#define MAME_WEBTV_WTVSOFTMODEM_SCRAMBLER_H

#pragma once

#include "wtvsoftmodem_bitblock.h"

#include <bit>
#include <cstdint>

// ITU-T V.34 7 Scrambler (also the V.22bis scrambler)

class wtvsoftmodem_scrambler
{

public:

	static constexpr uint8_t MAX_CHANNELS = 8;

	typedef uint8_t scrambler_spec_t;

	// Tap positions expressed as the exponents p, q in GPC(x) = 1 + x^-p + x^-q.
	typedef struct scrambler_channel
	{
		uint8_t tap0; // p
		uint8_t tap1; // q
	} scrambler_channel_t;

	typedef struct scrambler_detail
	{
		scrambler_spec_t spec;
		uint8_t channel_count;
		scrambler_channel_t channel[MAX_CHANNELS];
		uint8_t default_tx_channel;
		uint8_t default_rx_channel;
	} scrambler_detail_t;

	static constexpr scrambler_spec_t SCRAMBLER_V34    = 0x01;
	static constexpr scrambler_spec_t SCRAMBLER_V22BIS = 0x02;

	static constexpr uint8_t CALL_CHANNEL   = 0;
	static constexpr uint8_t ANSWER_CHANNEL = 1;

	wtvsoftmodem_scrambler();

	void set_scrambler_spec_table(const scrambler_detail_t *set_scrambler_detail_table, uint8_t set_scrambler_detail_count);

	void init(scrambler_spec_t scrambler_spec);
	void init(const scrambler_detail_t *scrambler_detail);

	void set_tx_channel(uint8_t channel_index);
	void set_rx_channel(uint8_t channel_index);

	void reset(uint64_t seed = 0)
	{
		m_sr = seed;
	}

	uint64_t state() const
	{
		return m_sr;
	}

	inline void descramble(wtvsoftmodem_bitblock &bits)
	{
		const uint32_t count = bits.count;
		const uint32_t a = uint32_t(std::countr_zero(m_rx_mask0)) + 1;
		const uint32_t b = uint32_t(std::countr_zero(m_rx_mask1)) + 1;
		const uint32_t history = a > b ? a : b;

		if (!count)
			return;

		if (history >= 64 || history + count > wtvsoftmodem_bitblock::CAPACITY)
		{
			wtvsoftmodem_bitblock out;

			for (uint32_t i = 0; i < count; i++)
				out.append(descramble(bool((bits.word[i / 64] >> (i % 64)) & 1)), 1);

			bits = out;
		}
		else
		{
			wtvsoftmodem_bitblock stream;
			stream.count   = history + count;
			stream.word[0] = (wtvsoftmodem_bitblock::reverse(m_sr) >> (64 - history)) | (bits.word[0] << history);
			stream.word[1] = (bits.word[0] >> (64 - history)) | (bits.word[1] << history);

			wtvsoftmodem_bitblock out;

			for (uint32_t first = 0; first < count; first += 64)
			{
				const uint32_t len = count - first < 64 ? count - first : 64;

				out.append(stream.get(history + first, len) ^ stream.get(history + first - a, len) ^ stream.get(history + first - b, len), len);
			}

			m_sr = count >= 64 ? wtvsoftmodem_bitblock::reverse(bits.get(count - 64, 64)) : (m_sr << count) | (wtvsoftmodem_bitblock::reverse(bits.get(0, count)) >> (64 - count));
			bits = out;
		}
	}

	inline bool scramble(bool data_bit)
	{
		bool fb  = feedback(m_tx_mask0, m_tx_mask1);
		bool out = data_bit ^ fb;

		m_sr = (m_sr << 1) | uint64_t(out & 1);

		return out;
	}

	inline bool descramble(bool line_bit)
	{
		bool fb  = feedback(m_rx_mask0, m_rx_mask1);
		bool out = line_bit ^ fb;

		m_sr = (m_sr << 1) | uint64_t(line_bit & 1);

		return out;
	}

private:

	static constexpr scrambler_detail_t DEFAULT_SCRAMBLER_SPEC_TABLE[] = {
		{ 
			.spec = SCRAMBLER_V34,
			.channel_count = 2,
			.channel = {
				{
					.tap0 = 18,
					.tap1 = 23
				},
				{
					.tap0 = 5,
					.tap1 = 23
				}
			}, 
			.default_tx_channel = ANSWER_CHANNEL,
			.default_rx_channel = CALL_CHANNEL
		},
		{
			.spec = SCRAMBLER_V22BIS,
			.channel_count = 2,
			.channel = {
				{
					.tap0 = 14,
					.tap1 = 17
				},
				{
					.tap0 = 14,
					.tap1 = 17
				}
			},
			.default_tx_channel = ANSWER_CHANNEL,
			.default_rx_channel = CALL_CHANNEL
		}
	};

	inline bool feedback(uint64_t mask0, uint64_t mask1) const
	{
		return ((m_sr & mask0) != 0) ^ ((m_sr & mask1) != 0);
	}

	uint8_t m_spec_count = 0;

	const scrambler_detail_t *m_spec_table = nullptr;
	const scrambler_detail_t *m_detail = nullptr;

	uint64_t m_sr = 0;
	uint64_t m_tx_mask0 = uint64_t(1) << (23 - 1);
	uint64_t m_tx_mask1 = uint64_t(1) << (23 - 1);
	uint64_t m_rx_mask0 = uint64_t(1) << (23 - 1);
	uint64_t m_rx_mask1 = uint64_t(1) << (23 - 1);
};

#endif // MAME_WEBTV_WTVSOFTMODEM_SCRAMBLER_H
