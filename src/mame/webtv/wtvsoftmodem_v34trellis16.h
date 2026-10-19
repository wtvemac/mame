// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34TRELLIS16_H
#define MAME_WEBTV_WTVSOFTMODEM_V34TRELLIS16_H

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace wtvsoftmodem::v34::trellis16
{
	// ITU-T V.34 Table 13 [Y4(m), Y3(m), Y2(m), Y1(m)]
	static constexpr int8_t ENCODER_INPUT[8][8] = {
		{  0,  0,  1,  1,  8,  8,  9,  9 },
		{  3,  2,  2,  3, 11, 10, 10, 11 },
		{  5,  5,  4,  4, 13, 13, 12, 12 },
		{  6,  7,  7,  6, 14, 15, 15, 14 },
		{  8,  8,  9,  9,  0,  0,  1,  1 },
		{ 11, 10, 10, 11,  3,  2,  2,  3 },
		{ 13, 13, 12, 12,  5,  5,  4,  4 },
		{ 14, 15, 15, 14,  6,  7,  7,  6 },
	};

	// ITU-T V.34 Figure 10 16-state convolutional encoder
	static constexpr uint8_t ENCODER_NEXT_STATE[16][16] = {
		{ 0x00, 0x01, 0x06, 0x07, 0x00, 0x01, 0x06, 0x07, 0x00, 0x01, 0x06, 0x07, 0x00, 0x01, 0x06, 0x07 },
		{ 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b },
		{ 0x01, 0x00, 0x07, 0x06, 0x01, 0x00, 0x07, 0x06, 0x01, 0x00, 0x07, 0x06, 0x01, 0x00, 0x07, 0x06 },
		{ 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a },
		{ 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04, 0x05 },
		{ 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09 },
		{ 0x03, 0x02, 0x05, 0x04, 0x03, 0x02, 0x05, 0x04, 0x03, 0x02, 0x05, 0x04, 0x03, 0x02, 0x05, 0x04 },
		{ 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08 },
		{ 0x04, 0x05, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03 },
		{ 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f, 0x08, 0x09, 0x0e, 0x0f },
		{ 0x05, 0x04, 0x03, 0x02, 0x05, 0x04, 0x03, 0x02, 0x05, 0x04, 0x03, 0x02, 0x05, 0x04, 0x03, 0x02 },
		{ 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e, 0x09, 0x08, 0x0f, 0x0e },
		{ 0x06, 0x07, 0x00, 0x01, 0x06, 0x07, 0x00, 0x01, 0x06, 0x07, 0x00, 0x01, 0x06, 0x07, 0x00, 0x01 },
		{ 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d, 0x0a, 0x0b, 0x0c, 0x0d },
		{ 0x07, 0x06, 0x01, 0x00, 0x07, 0x06, 0x01, 0x00, 0x07, 0x06, 0x01, 0x00, 0x07, 0x06, 0x01, 0x00 },
		{ 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c, 0x0b, 0x0a, 0x0d, 0x0c },
	};

	// ITU-T V.34 Figure 10 16-state convolutional encoder
	static constexpr uint8_t TRANSITION[16][4] = {
		{  0,  1,  6,  7 },
		{ 12, 13, 10, 11 },
		{  1,  0,  7,  6 },
		{ 13, 12, 11, 10 },
		{  2,  3,  4,  5 },
		{ 14, 15,  8,  9 },
		{  3,  2,  5,  4 },
		{ 15, 14,  9,  8 },
		{  4,  5,  2,  3 },
		{  8,  9, 14, 15 },
		{  5,  4,  3,  2 },
		{  9,  8, 15, 14 },
		{  6,  7,  0,  1 },
		{ 10, 11, 12, 13 },
		{  7,  6,  1,  0 },
		{ 11, 10, 13, 12 },
	};

	// ITU-T V.34 Derived from Figure 9 (point labelling) and Table 13
	static constexpr uint8_t BRANCH_INPUT[16] = {
		0, 2, 3, 1, 1, 2, 3, 0, 0, 3, 2, 1, 1, 3, 2, 0,
	};

	// Pulled from AltoCom
	static constexpr uint8_t TIE_PRIORITY[16] = {
		0, 5, 6, 3, 2, 5, 6, 1, 0, 7, 4, 3, 2, 7, 4, 1,
	};

	static constexpr auto NEXT_STATE_TABLE = []
	{
		std::array<std::array<std::array<int8_t, 16>, 16>, 2> table {};

		for (uint32_t inversion = 0; inversion < 2; inversion++)
		{
			for (uint32_t state = 0; state < 16; state++)
			{
				for (uint32_t pattern = 0; pattern < 16; pattern++)
				{
					if ((std::popcount(pattern) & 1U) != ((state & 1U) ^ inversion))
						table[inversion][state][pattern] = int8_t(-1);
					else
						table[inversion][state][pattern] = int8_t(TRANSITION[state][BRANCH_INPUT[pattern]]);
				}
			}
		}

		return table;
	}();

	inline int32_t subset_label(int32_t re, int32_t im)
	{
		int32_t xored = re ^ im;
		int32_t x = xored & 2;

		return ((xored & 4) ^ (x << 1)) | (re & 2) | (x >> 1);
	}

	inline int32_t next_state(uint32_t inversion, uint32_t state, uint32_t pattern)
	{
		return NEXT_STATE_TABLE[inversion][state][pattern];
	}

	class decoder
	{
	public:

		using point4 = std::array<double, 4>;

		struct choice
		{
			uint8_t previous = 0;
			uint8_t pattern = 0;
			point4 point{};
		};
	
		std::array<double, 16> metric {};

		std::array<std::array<choice, 16>, 32> history {};

		uint64_t count = 0;

		bool put(point4 input, uint32_t inversion, point4 &output)
		{
			std::array<std::array<double, 2>, 4> nearest {};
			std::array<std::array<double, 2>, 4> error2 {};

			for (int32_t k = 0; k < 4; k++)
			{
				for (int32_t bit = 0; bit < 2; bit++)
				{
					const double base = bit ? 128. : -128.;
					nearest[k][bit] = base + 512. * std::floor((input[k] - base + 256.) / 512.);

					const double error = input[k] - nearest[k][bit];
					error2[k][bit] = error * error;
				}
			}

			std::array<point4, 16> candidates {};
			std::array<double, 16> distance {};
			for (int32_t p = 0; p < 16; p++)
			{
				for (int32_t k = 0; k < 4; k++)
				{
					const int32_t bit = (p >> k) & 1;
					candidates[p][k] = nearest[k][bit];
					distance[p] += error2[k][bit];
				}
			}

			std::array<double, 16> next;
			next.fill(std::numeric_limits<double>::infinity());

			auto &row = history[count % 32];

			for (int32_t prev = 0; prev < 16; prev++)
			{
				for (int32_t p = 0; p < 16; p++)
				{
					int32_t dst = next_state(inversion, prev, p);

					if (dst < 0)
						continue;

					double score = metric[prev] + distance[p];
					if (score < next[dst] || (score == next[dst] && TIE_PRIORITY[p] > TIE_PRIORITY[row[dst].pattern]))
					{
						next[dst] = score;
						row[dst] = { uint8_t(prev), uint8_t(p), candidates[p] };
					}
				}
			}

			const auto best_state = std::min_element(next.begin(), next.end());
			const double best = *best_state;
			uint32_t state = uint32_t(best_state - next.begin());

			for (int32_t k = 0; k < 16; k++)
				metric[k] = next[k] - best;

			count++;

			if (count < 32)
				return false;

			for (uint32_t k = 0; k < 31; k++)
				state = history[(count - 1 - k) % 32][state].previous;

			output = history[(count - 32) % 32][state].point;

			return true;
		}
	};

}

#endif // MAME_WEBTV_WTVSOFTMODEM_V34TRELLIS16_H