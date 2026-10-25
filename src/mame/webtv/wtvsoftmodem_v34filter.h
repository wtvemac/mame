// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34FILTER_H
#define MAME_WEBTV_WTVSOFTMODEM_V34FILTER_H

#pragma once

#include "wtvsoftmodem_dsp.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

// ITU-T V.34 9.6.2 Precoder, 9.7 Non-linear encoder, 5.4 Pre-emphasis

namespace wtvsoftmodem::v34::filter::precoder
{
	inline int32_t round_q14(int64_t sum)
	{
		int64_t m = ((sum < 0 ? -sum : sum) + 0x1fff) >> 14;

		return int32_t(sum < 0 ? -m : m);
	}

	inline int32_t quantize_coset(int32_t p_q7, bool b_ge_56)
	{
		const int32_t mag = (p_q7 < 0 ? -p_q7 : p_q7);
		const int32_t t = b_ge_56 ? (((mag + 0xff) >> 9) << 2) : (((mag + 0x7f) >> 8) << 1);

		return p_q7 < 0 ? -t : t;
	}

	template <typename History>
	inline std::array<int32_t, 2> predict(const int16_t (&h)[3][2], History &&x)
	{
		int64_t sre = 0;
		int64_t sim = 0;

		for (int32_t i = 0; i < 3; i++)
		{
			const int32_t hr = h[i][0];
			const int32_t hi = h[i][1];
			const auto &p = x(i);
			const int32_t xr = p[0];
			const int32_t xi = p[1];

			sre += int64_t(xr) * hr - int64_t(xi) * hi;
			sim += int64_t(xr) * hi + int64_t(xi) * hr;
		}

		return { round_q14(sre), round_q14(sim) };
	}
}

namespace wtvsoftmodem::v34::filter::shaping
{
	// Pulled from AltoCom
	// The rate index into the COEFFS_C16 table.
	// BANK
	//   [symbol rate]
	//   [carrier type]
	//      low=0
	//      high=1
	static constexpr uint8_t BANK[6][2] = {
		// 2400 baud
		{ 0, 1 },
		// 2734 baud
		{ 2, 3 },
		// 2800 baud (same as 2734)
		{ 2, 3 },
		// 3000 baud
		{ 4, 5 },
		// 3200 baud
		{ 6, 7 },
		// 3429 baud
		{ 8, 8 },
	};

	// COEFFS_C16
	//   [rate index, from above]
	//   [pre-emphasis index]
	//      0 -  5 = V.34 Table 3, Figure 1
	//      6 - 10 = V.34 Table 4, Figure 2
	//   [filter tap index] (first half of the 32-tap FIR)
	static constexpr int8_t COEFFS_C16[9][11][16] = {
		{
			{   -2,   -1,   -3,   -2,   -3,   -6,    0,   -5,   -4,    7,   -8,   12,   19,   -2,   88, 87 },
			{   -1,   -1,   -3,   -1,   -2,   -6,    1,   -6,   -4,    8,  -10,   12,   18,  -12,   85, 90 },
			{   -1,    0,   -3,    0,   -2,   -6,    3,   -6,   -4,    9,  -13,   12,   17,  -21,   80, 92 },
			{    0,    0,   -3,    0,   -1,   -6,    4,   -6,   -4,   10,  -15,   11,   18,  -30,   75, 93 },
			{    0,    1,   -3,    1,   -1,   -6,    5,   -7,   -4,   12,  -17,   11,   19,  -38,   69, 94 },
			{    0,    1,   -3,    1,   -1,   -6,    6,   -7,   -4,   13,  -19,   10,   20,  -45,   63, 94 },
			{   -1,   -1,   -3,   -2,   -3,   -6,    1,   -6,   -4,    8,   -9,   13,   20,   -8,   86, 88 },
			{   -1,   -1,   -3,   -1,   -2,   -6,    2,   -6,   -4,    8,  -11,   14,   22,  -13,   83, 90 },
			{   -1,   -1,   -3,   -1,   -2,   -6,    3,   -6,   -5,    9,  -12,   14,   23,  -18,   79, 91 },
			{   -1,    0,   -4,   -1,   -2,   -7,    3,   -7,   -5,   10,  -14,   15,   25,  -24,   75, 92 },
			{    0,    0,   -4,    0,   -2,   -7,    4,   -7,   -5,   11,  -16,   15,   27,  -29,   71, 93 },
		},
		{
			{    2,    1,    2,    0,    1,   -3,   -4,   -2,  -13,   -1,  -11,   -6,   18,  -12,   78, 95 },
			{    1,    1,    2,   -1,    2,   -3,   -3,   -1,  -13,    2,  -11,   -7,   19,  -22,   72, 98 },
			{    0,    1,    2,   -1,    3,   -3,   -2,    1,  -13,    5,  -11,   -8,   22,  -30,   65, 100 },
			{    0,    1,    2,   -2,    4,   -3,   -2,    2,  -14,    7,  -11,   -9,   25,  -38,   57, 101 },
			{    0,    1,    2,   -2,    4,   -4,   -1,    4,  -14,    9,  -11,  -10,   28,  -44,   50, 101 },
			{   -1,    1,    2,   -3,    5,   -4,   -1,    5,  -15,   11,  -10,  -12,   31,  -49,   42, 101 },
			{    1,    1,    2,   -1,    2,   -3,   -4,   -1,  -14,    0,  -11,   -6,   20,  -17,   75, 97 },
			{    1,    2,    2,   -1,    2,   -3,   -3,   -1,  -14,    2,  -11,   -7,   23,  -22,   70, 99 },
			{    1,    2,    2,   -1,    3,   -3,   -3,    0,  -15,    3,  -11,   -7,   25,  -26,   66, 100 },
			{    1,    2,    2,   -2,    3,   -4,   -3,    1,  -15,    5,  -11,   -8,   28,  -31,   61, 101 },
			{    0,    2,    2,   -2,    4,   -4,   -2,    2,  -16,    6,  -11,   -8,   31,  -35,   56, 101 },
		},
		{
			{    6,   -3,    8,   -2,    7,    2,    5,    8,    5,    9,   14,    4,   34,    2,   71, 97 },
			{    5,   -2,    6,   -1,    4,    4,    1,    9,    3,    8,   14,   -1,   35,   -6,   68, 98 },
			{    4,    0,    3,    2,    0,    8,   -4,   13,   -2,    8,   13,   -5,   38,  -16,   67, 96 },
			{    4,    0,    2,    4,   -3,   11,   -8,   15,   -6,   10,    9,   -5,   35,  -18,   58, 102 },
			{    4,    0,    2,    4,   -3,   11,   -9,   16,   -7,   10,    9,   -8,   36,  -23,   52, 103 },
			{    5,   -1,    2,    4,   -4,   12,  -11,   17,   -9,    9,    9,  -11,   37,  -28,   47, 104 },
			{    6,   -3,    7,   -2,    7,    2,    4,    8,    5,    8,   14,    2,   36,   -1,   67, 99 },
			{    6,   -2,    6,   -1,    4,    4,    2,   10,    3,    8,   15,   -1,   39,   -6,   66, 98 },
			{    5,   -1,    5,    1,    2,    7,   -1,   12,    1,    8,   16,   -4,   43,  -11,   65, 97 },
			{    5,   -1,    3,    3,   -1,    9,   -4,   14,   -1,    8,   15,   -7,   46,  -16,   62, 95 },
			{    4,    0,    2,    4,   -3,   12,   -8,   18,   -5,   11,   12,   -5,   44,  -14,   54, 101 },
		},
		{
			{   -5,    3,   -7,    6,   -8,    7,   -5,    5,    3,    1,   15,   -3,   33,    1,   66, 100 },
			{   -5,    3,   -6,    6,   -8,    7,   -5,    4,    3,   -1,   16,   -7,   33,   -5,   59, 104 },
			{   -4,    3,   -6,    5,   -7,    6,   -4,    3,    4,   -3,   17,  -10,   32,   -9,   51, 109 },
			{   -4,    3,   -6,    5,   -7,    5,   -4,    1,    5,   -7,   18,  -16,   34,  -16,   45, 109 },
			{   -5,    3,   -7,    5,   -8,    5,   -5,    0,    5,   -9,   19,  -19,   35,  -20,   37, 109 },
			{   -5,    3,   -8,    6,   -9,    5,   -6,    0,    4,  -11,   20,  -23,   35,  -24,   30, 110 },
			{   -5,    3,   -7,    6,   -8,    7,   -5,    5,    3,    0,   16,   -5,   35,   -1,   62, 102 },
			{   -5,    3,   -7,    6,   -8,    7,   -5,    5,    3,   -2,   17,   -7,   36,   -4,   57, 104 },
			{   -5,    4,   -7,    6,   -8,    7,   -5,    5,    4,   -3,   18,   -9,   38,   -6,   52, 106 },
			{   -5,    3,   -6,    6,   -8,    6,   -4,    3,    5,   -4,   19,  -10,   38,   -6,   45, 109 },
			{   -5,    3,   -6,    6,   -8,    5,   -4,    2,    6,   -7,   21,  -15,   41,  -10,   42, 107 },
		},
		{
			{    1,    4,    0,    4,    1,    5,    4,    3,   11,    0,   21,   -3,   35,    3,   65, 100 },
			{   -1,    4,   -1,    5,    0,    5,    3,    2,   11,   -2,   21,   -6,   35,   -3,   59, 103 },
			{   -2,    4,   -2,    5,   -1,    5,    2,    2,   10,   -4,   22,  -10,   36,   -8,   54, 105 },
			{   -3,    4,   -2,    5,   -2,    5,    2,    1,   10,   -6,   22,  -14,   36,  -13,   47, 107 },
			{   -3,    4,   -3,    6,   -3,    5,    1,    0,   10,   -8,   23,  -17,   36,  -17,   41, 108 },
			{   -4,    4,   -4,    6,   -4,    5,    1,   -1,   10,  -10,   24,  -20,   37,  -20,   35, 109 },
			{    0,    4,   -1,    5,    0,    5,    4,    3,   11,   -1,   22,   -5,   37,    1,   61, 101 },
			{   -1,    4,   -1,    5,    0,    5,    3,    3,   12,   -2,   23,   -7,   39,   -1,   57, 103 },
			{   -1,    5,   -1,    6,   -1,    5,    3,    2,   12,   -4,   23,   -9,   40,   -3,   53, 104 },
			{   -2,    5,   -2,    6,   -1,    5,    3,    2,   12,   -5,   24,  -11,   42,   -5,   48, 105 },
			{   -2,    5,   -2,    6,   -2,    5,    2,    1,   13,   -6,   25,  -14,   43,   -7,   44, 105 },
		},
		{
			{   -3,   -3,   -1,   -4,    1,   -5,    3,   -6,    7,   -6,   14,   -2,   24,   10,   51, 111 },
			{   -2,   -3,    0,   -4,    2,   -6,    5,   -8,    9,   -8,   15,   -6,   24,    5,   44, 114 },
			{   -1,   -4,    1,   -5,    2,   -8,    6,  -10,   10,  -11,   16,   -9,   23,    0,   37, 116 },
			{   -2,   -4,    1,   -6,    3,   -9,    6,  -12,   10,  -13,   16,  -12,   22,   -3,   28, 117 },
			{   -2,   -5,    1,   -7,    3,  -10,    6,  -13,   10,  -16,   16,  -15,   21,   -6,   19, 118 },
			{   -2,   -5,    1,   -7,    3,  -11,    6,  -15,   11,  -18,   16,  -17,   20,   -8,   11, 118 },
			{   -2,   -3,    0,   -4,    1,   -6,    4,   -7,    9,   -7,   15,   -4,   25,    8,   47, 113 },
			{   -2,   -3,    1,   -5,    2,   -7,    5,   -8,   10,   -9,   16,   -6,   26,    7,   42, 114 },
			{   -1,   -4,    1,   -5,    3,   -8,    6,   -9,   11,  -10,   17,   -7,   27,    6,   37, 115 },
			{   -1,   -4,    1,   -6,    3,   -9,    6,  -11,   12,  -12,   17,   -9,   27,    5,   31, 116 },
			{   -2,   -5,    1,   -6,    3,  -10,    6,  -12,   12,  -13,   17,  -11,   27,    5,   25, 116 },
		},
		{
			{    6,    0,    5,    0,    7,   -1,   11,   -2,   16,   -2,   22,    0,   30,    9,   55, 105 },
			{    5,    1,    3,    1,    6,   -1,   10,   -3,   15,   -4,   22,   -4,   30,    4,   50, 108 },
			{    3,    3,    2,    1,    5,   -1,    9,   -4,   15,   -6,   22,   -7,   30,    0,   45, 111 },
			{    0,    4,    2,    1,    5,   -1,    9,   -5,   15,   -8,   22,  -10,   30,   -4,   39, 112 },
			{   -1,    3,    2,    1,    4,   -2,    9,   -6,   15,  -10,   23,  -13,   30,   -7,   32, 114 },
			{   -3,    3,    2,    1,    4,   -3,    9,   -7,   16,  -12,   23,  -15,   30,  -10,   25, 114 },
			{    6,    1,    4,    0,    7,   -1,   11,   -2,   16,   -3,   22,   -2,   32,    8,   51, 107 },
			{    5,    1,    3,    1,    7,   -1,   11,   -3,   17,   -4,   23,   -4,   33,    7,   47, 108 },
			{    4,    2,    2,    1,    6,   -1,   11,   -4,   17,   -6,   24,   -6,   34,    6,   43, 109 },
			{    3,    3,    2,    1,    6,   -1,   11,   -4,   18,   -7,   24,   -8,   35,    5,   39, 110 },
			{    2,    4,    2,    1,    6,   -2,   11,   -5,   18,   -8,   25,  -10,   36,    4,   34, 111 },
		},
		{
			{    1,    0,    4,   -1,    6,   -1,    9,   -2,   13,   -1,   19,    1,   28,   11,   52, 108 },
			{    0,   -1,    4,   -1,    6,   -3,    9,   -3,   14,   -4,   19,   -2,   26,    7,   46, 112 },
			{    0,   -2,    4,   -2,    6,   -4,   10,   -5,   14,   -6,   19,   -4,   25,    4,   39, 115 },
			{    0,   -2,    4,   -3,    6,   -5,   10,   -7,   14,   -8,   19,   -7,   25,    1,   31, 117 },
			{    0,   -3,    4,   -3,    6,   -6,   10,   -8,   15,  -10,   19,   -9,   24,   -2,   24, 118 },
			{    0,   -3,    4,   -4,    7,   -7,   11,   -9,   15,  -12,   19,  -12,   23,   -4,   16, 119 },
			{    0,    0,    4,   -1,    6,   -2,    9,   -3,   14,   -2,   19,    0,   28,   11,   48, 110 },
			{    0,   -1,    4,   -1,    6,   -3,   10,   -4,   15,   -4,   20,   -2,   29,   10,   43, 112 },
			{    0,   -1,    4,   -2,    7,   -3,   10,   -5,   15,   -5,   20,   -3,   29,   10,   38, 113 },
			{    0,   -2,    4,   -2,    7,   -4,   11,   -6,   16,   -6,   21,   -5,   29,    9,   33, 114 },
			{    0,   -2,    4,   -3,    7,   -5,   11,   -7,   17,   -7,   21,   -6,   29,    9,   28, 115 },
		},
		{
			{    5,    0,    4,    0,    6,    0,    8,    1,   11,    3,   15,    7,   22,   18,   45, 113 },
			{    6,    0,    5,    0,    6,    0,    8,    0,   11,    2,   14,    5,   20,   15,   38, 116 },
			{    6,   -1,    5,   -1,    6,   -1,    8,   -1,   11,    0,   13,    3,   18,   12,   31, 119 },
			{    7,   -1,    5,   -1,    7,   -2,    8,   -2,   10,   -1,   13,    2,   17,   10,   23, 121 },
			{    7,   -1,    5,   -2,    7,   -2,    8,   -3,   10,   -2,   12,    0,   15,    9,   16, 123 },
			{    8,   -2,    5,   -2,    7,   -3,    8,   -4,   10,   -3,   12,   -1,   13,    8,    8, 124 },
			{    6,    0,    5,    0,    6,    0,    9,    1,   11,    3,   15,    6,   22,   18,   40, 114 },
			{    6,    0,    5,    0,    7,    0,    9,    0,   12,    2,   15,    5,   21,   18,   36, 116 },
			{    7,   -1,    5,   -1,    7,   -1,    9,    0,   12,    2,   15,    4,   21,   19,   30, 117 },
			{    7,   -1,    5,   -1,    7,   -1,    9,   -1,   12,    1,   15,    4,   21,   19,   25, 118 },
			{    8,   -1,    6,   -1,    8,   -1,   10,   -2,   12,    0,   15,    3,   20,   20,   20, 119 },
		},
	};

	inline void expand(int32_t v, int16_t h[32], int32_t baud = 5, bool high_carrier = true)
	{
		if (v < 0)
			v = 0;
		else if (v > 10)
			v = 10;

		baud = std::clamp(baud, 0, 5);
		const auto &coeffs = COEFFS_C16[BANK[baud][high_carrier ? 1 : 0]][v];

		for (int32_t k = 0; k < 16; k++)
		{
			h[k] = coeffs[k];
			h[31 - k] = -coeffs[k];
		}
	}

	inline std::vector<double> at_rate(int32_t variant, uint32_t factor, double carrier_hz, int32_t baud = 5, bool high_carrier = true)
	{
		if (!factor)
			return {};

		int16_t native[32];

		expand(variant, native, baud, high_carrier);

		std::vector<double> taps(31 * factor + 1, 0.0);
		for (uint32_t n = 0; n < 32; n++)
			taps[n * factor] = native[n];

		const std::complex<double> response = wtvsoftmodem_dsp::fir_response(native, 32, carrier_hz, 8000.0);

		const double gain = std::hypot(response.real(), response.imag());
		if (gain > 1e-9)
		{
			for (auto &tap : taps)
				tap *= wtvsoftmodem_dsp::db_to_amplitude(3.0) / gain;
		}

		return taps;
	}

	inline std::vector<double> resample(int32_t variant, double fs_out, double carrier_hz, int32_t ntaps, int32_t baud = 5, bool high_carrier = true)
	{
		if (variant < 0)
			return {};

		int16_t h8[32];

		expand(variant, h8, baud, high_carrier);

		const double fs8 = 8000.0;
		const double target_gain = wtvsoftmodem_dsp::db_to_amplitude(3.0);

		const int32_t factor = wtvsoftmodem_dsp::round_nearest<int32_t>(fs_out / fs8);
		if (factor >= 1 && std::fabs(fs_out - fs8 * factor) < 1.0)
		{
			return at_rate(variant, uint32_t(factor), carrier_hz, baud, high_carrier);
		}
		else
		{
			const double grpdelay8 = 15.5;

			std::vector<double> taps = wtvsoftmodem_dsp::resample_fir(h8, 32, fs8, grpdelay8, fs_out, ntaps);

			const std::complex<double> response_at_carrier = wtvsoftmodem_dsp::fir_response(taps.data(), taps.size(), carrier_hz, fs_out);
			double g = std::hypot(response_at_carrier.real(), response_at_carrier.imag());

			if (g > 1e-9)
			{
				for (auto &t : taps)
					t *= (target_gain / g);
			}

			return taps;
		}
	}
}

namespace wtvsoftmodem::v34::filter::nonlinear
{
	struct point
	{
		int16_t i;
		int16_t q;
	};

	inline int32_t multiply(int32_t a, int32_t b)
	{
		return std::bit_cast<int32_t>(uint32_t(int64_t(a) * b));
	}

	inline int32_t add(int32_t a, int32_t b)
	{
		return std::bit_cast<int32_t>(uint32_t(a) + uint32_t(b));
	}

	inline int16_t narrow(int32_t value)
	{
		return std::bit_cast<int16_t>(uint16_t(value));
	}

	inline void encode(int32_t &re, int32_t &im)
	{
		constexpr int32_t K_CONST = 0x6aaa;
		const int32_t x = re;
		const int32_t y = im;
		const int32_t a2 = multiply(x, x) + multiply(y, y);
		const int32_t t2 = a2 >> 15;
		const int32_t t4 = multiply(t2, K_CONST) >> 15;
		const int32_t t6 = multiply(t2, t2) >> 15;
		const int32_t t8 = multiply(t6, K_CONST) >> 17;
		const int32_t t0 = t4 + t8;

		re = (multiply(x, t0) >> 15) + x;
		im = (multiply(y, t0) >> 15) + y;
	}

	inline point decode(point input)
	{
		const int32_t x = input.i;
		const int32_t y = input.q;
		const int32_t r2 = add(multiply(x, x), multiply(y, y)) >> 15;
		const int32_t r4 = multiply(r2, r2) >> 15;
		const int32_t r6 = multiply(r4, r2) >> 15;
		int32_t gain = add(multiply(r2, -0x5ea1) >> 15, 0x7fa3);

		gain = add(gain, multiply(r4, 0x756d) >> 15);
		gain = add(gain, multiply(r6, -0x4ab3) >> 15);

		return { narrow(multiply(x, gain) >> 15), narrow(multiply(y, gain) >> 15) };
	}
}

#endif // MAME_WEBTV_WTVSOFTMODEM_V34FILTER_H
