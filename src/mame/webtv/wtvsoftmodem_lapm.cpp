// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_lapm.h"

#include <algorithm>
#include <cstddef>

// ITU-T V.42 7.2.1 Detection phase (ODP/ADP)

wtvsoftmodem_lapm::wtvsoftmodem_lapm()
{
	//
}

void wtvsoftmodem_lapm::reset()
{
	m_state = LAPMState::WAIT_ODP;
	m_rx_window = 0;
	m_rx_bits = 0;
	m_rx_ones = 0;
	m_rx_since_ff = NO_RECENT_FF;
	m_confirm_count.fill(0);
	m_tx_pattern_pos = 0;
	m_tx_bitpos = 0;
	m_tx_repeats_left = ADP_REPEAT_COUNT;
}

bool wtvsoftmodem_lapm::rx_bit(bool bit)
{
	if (m_state != LAPMState::WAIT_ODP)
		return false;

	m_rx_window = (m_rx_window >> 1) | (uint64_t(bit) << 39);
	m_rx_bits++;

	track_ff(bit);

	if (m_rx_bits < 40)
		return false;

	int32_t &confirm = m_confirm_count[m_rx_bits & 7];
	if (window_matches_any_rotation())
	{
		confirm++;
		if (confirm >= ODP_CONFIRM_WINDOWS)
		{
			m_state = LAPMState::SEND_ADP;
			m_tx_pattern_pos = 0;
			m_tx_bitpos = 0;
			m_tx_repeats_left = ADP_REPEAT_COUNT;

			return true;
		}
	}
	else
	{
		confirm = 0;
	}

	return false;
}

bool wtvsoftmodem_lapm::tx_get_bit()
{
	const uint8_t byte = ADP_PATTERN[m_tx_pattern_pos];
	const bool bit = (byte >> m_tx_bitpos) & 1;

	m_tx_bitpos++;
	if (m_tx_bitpos >= 8)
	{
		m_tx_bitpos = 0;

		m_tx_pattern_pos++;
		if (m_tx_pattern_pos >= int32_t(ADP_PATTERN.size()))
		{
			m_tx_pattern_pos = 0;
			m_tx_repeats_left--;

			if (m_tx_repeats_left <= 0)
				m_state = LAPMState::DONE;
		}
	}

	return bit;
}

bool wtvsoftmodem_lapm::rx_bits_without_odp(const wtvsoftmodem_bitblock &bits)
{
	if (m_state != LAPMState::WAIT_ODP)
		return true;

	const uint32_t count = bits.count;
	uint32_t ones = m_rx_ones;
	uint32_t since = m_rx_since_ff;
	uint64_t window = m_rx_window;

	if ((since <= 32 || bits.contains(wtvsoftmodem_bitblock::low_mask(ones) << (7 - ones), 8, 0xff)) && anchor_in_reach(window, bits))
	{
		for (uint32_t i = 0; i < count; i++)
		{
			const uint32_t bit = bits.bit(i);

			window = (window >> 1) | (uint64_t(bit) << 39);

			if (m_rx_bits + i + 1 >= 40 && window_matches_any_rotation(window))
				return false;

			ones = bit ? std::min(ones + 1, 8U) : 0;

			since = ones >= 8 ? 0 : std::min(since + 1, NO_RECENT_FF);
		}
	}
	else
	{
		window = count >= 40 ? bits.get(count - 40, 40) : (window >> count) | (bits.get(0, count) << (40 - count));

		const int32_t last_run = last_run_of_ones(ones, bits);
		const uint32_t run = bits.trailing_ones();

		ones = std::min((run == count) ? ones + run : run, 8U);

		since = last_run < 0 ? std::min(since + count, NO_RECENT_FF) : std::min(count - 1 - uint32_t(last_run), NO_RECENT_FF);
	}

	const uint64_t first = std::max<uint64_t>(m_rx_bits + 1, 40);
	const uint64_t last = m_rx_bits + count;

	for (uint64_t position = first; position <= last && position < first + 8; position++)
		m_confirm_count[position & 7] = 0;

	m_rx_window = window;
	m_rx_bits = last;
	m_rx_ones = ones;
	m_rx_since_ff = since;

	return true;
}

bool wtvsoftmodem_lapm::anchor_in_reach(uint64_t window, const wtvsoftmodem_bitblock &bits)
{
	static constexpr uint64_t ANCHOR = ODP_PATTERN[0];

	const uint64_t history = window >> 1;
	uint64_t match = wtvsoftmodem_bitblock::low_mask(32);

	for (uint32_t k = 0; k < 8; k++)
		match &= ((ANCHOR >> k) & 1) ? history >> k : ~history >> k;

	return match || bits.contains(window >> 33, 8, ANCHOR);
}

int32_t wtvsoftmodem_lapm::last_run_of_ones(uint32_t ones, const wtvsoftmodem_bitblock &bits)
{
	uint64_t previous = ones ? wtvsoftmodem_bitblock::low_mask(ones) << (64 - ones) : 0;

	int32_t last = -1;
	for (uint32_t w = 0; w * 64 < bits.count; w++)
	{
		const uint64_t current = bits.word[w];
		uint64_t ends = current & wtvsoftmodem_bitblock::low_mask(bits.count - w * 64);

		for (uint32_t k = 1; k < 8; k++)
			ends &= (current << k) | (previous >> (64 - k));

		if (ends)
			last = int32_t(w * 64 + 63 - uint32_t(std::countl_zero(ends)));

		previous = current;
	}

	return last;
}

void wtvsoftmodem_lapm::track_ff(bool bit)
{
	m_rx_ones = bit ? std::min<uint32_t>(m_rx_ones + 1, 8) : 0;
	m_rx_since_ff = (m_rx_ones >= 8) ? 0 : std::min<uint32_t>(m_rx_since_ff + 1, NO_RECENT_FF);
}

bool wtvsoftmodem_lapm::window_matches_any_rotation(uint64_t window)
{
	struct lookup
	{
		std::array<uint64_t, 256> table {};
		bool distinct = true;
	};

	static constexpr lookup BY_LOW_BYTE = []
	{
		lookup result;

		result.table.fill(~uint64_t(0));

		for (std::size_t rot = 0; rot < ODP_PATTERN.size(); rot++)
		{
			uint64_t pattern = 0;
			for (std::size_t i = 0; i < ODP_PATTERN.size(); i++)
				pattern |= uint64_t(ODP_PATTERN[(i + rot) % ODP_PATTERN.size()]) << (8 * i);

			result.distinct = result.distinct && result.table[pattern & 0xff] == ~uint64_t(0);
			result.table[pattern & 0xff] = pattern;
		}

		return result;
	}();

	static_assert(BY_LOW_BYTE.distinct, "ODP rotations must have distinct low bytes");

	return window == BY_LOW_BYTE.table[window & 0xff];
}
