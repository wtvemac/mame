// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_LAPM_H
#define MAME_WEBTV_WTVSOFTMODEM_LAPM_H

#pragma once

#include "wtvsoftmodem_bitblock.h"

#include <array>
#include <cstdint>

// ITU-T V.42 7.2.1 Detection phase (ODP/ADP)

class wtvsoftmodem_lapm
{
public:
	enum class LAPMState
	{
		WAIT_ODP,
		SEND_ADP,
		DONE
	};

	static constexpr std::array<uint8_t, 5> ODP_PATTERN = { 0x22, 0xfe, 0x2f, 0xf2, 0xff };
	static constexpr std::array<uint8_t, 5> ADP_PATTERN = { 0x8a, 0xfe, 0x6f, 0xe8, 0xff };

	static constexpr int32_t ADP_REPEAT_COUNT = 10;

	static constexpr int32_t ODP_CONFIRM_WINDOWS = 3;

	LAPMState m_state = LAPMState::WAIT_ODP;
	uint64_t m_rx_window = 0;
	uint64_t m_rx_bits = 0;
	uint32_t m_rx_ones = 0;
	uint32_t m_rx_since_ff = NO_RECENT_FF;
	std::array<int32_t, 8> m_confirm_count{};

	int32_t m_tx_pattern_pos = 0;
	int32_t m_tx_bitpos = 0;
	int32_t m_tx_repeats_left = ADP_REPEAT_COUNT;

	wtvsoftmodem_lapm();

	void reset();

	LAPMState state() const
	{
		return m_state;
	}
	bool waiting_for_odp() const
	{
		return m_state == LAPMState::WAIT_ODP;
	}
	bool sending_adp() const
	{
		return m_state == LAPMState::SEND_ADP;
	}
	bool done() const
	{
		return m_state == LAPMState::DONE;
	}

	bool rx_bit(bool bit);

	bool tx_get_bit();

	bool rx_bits_without_odp(const wtvsoftmodem_bitblock &bits);

private:

	static constexpr uint32_t NO_RECENT_FF = 64;

	void track_ff(bool bit);

	static bool anchor_in_reach(uint64_t window, const wtvsoftmodem_bitblock &bits);
	static int32_t last_run_of_ones(uint32_t ones, const wtvsoftmodem_bitblock &bits);

	bool window_matches_any_rotation() const
	{
		return window_matches_any_rotation(m_rx_window);
	}

	static bool window_matches_any_rotation(uint64_t window);
};

#endif // MAME_WEBTV_WTVSOFTMODEM_LAPM_H
