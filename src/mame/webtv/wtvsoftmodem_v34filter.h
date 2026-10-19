// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34FILTER_H
#define MAME_WEBTV_WTVSOFTMODEM_V34FILTER_H

#pragma once

#include "wtvsoftmodem_dsp.h"

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
	static constexpr int8_t COEFFS_3429_C16[6][16] = {
		{ 5, 0,  4, 0,  6, 0,  8, 1,  11, 3,  15, 7,  22, 18, 45, 113 }, // variant 0 (preemph 0)
		{ 6, 0,  5, 0,  6, 0,  8, 0,  11, 2,  14, 5,  20, 15, 38, 116 }, // variant 1
		{ 6, -1, 5, -1, 6, -1, 8, -1, 11, 0,  13, 3,  18, 12, 31, 119 }, // variant 2
		{ 7, -1, 5, -1, 7, -2, 8, -2, 10, -1, 13, 2,  17, 10, 23, 121 }, // variant 3
		{ 7, -1, 5, -2, 7, -2, 8, -3, 10, -2, 12, 0,  15, 9,  16, 123 }, // variant 4
		{ 8, -2, 5, -2, 7, -3, 8, -4, 10, -3, 12, -1, 13, 8,  8,  124 }, // variant 5
	};

	inline void expand(int32_t v, int16_t h[32])
	{
		if (v < 0)
			v = 0;
		else if (v > 5)
			v = 5;

		for (int32_t k = 0; k < 16; k++)
		{
			h[k] = COEFFS_3429_C16[v][k];
			h[31 - k] = -COEFFS_3429_C16[v][k];
		}
	}

	inline std::vector<double> at_rate(int32_t variant, uint32_t factor, double carrier_hz)
	{
		if (!factor)
			return {};

		int16_t native[32];

		expand(variant, native);

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

	inline std::vector<double> resample(int32_t variant, double fs_out, double carrier_hz, int32_t ntaps)
	{
		if (variant < 0)
			return {};

		int16_t h8[32];

		expand(variant, h8);

		const double fs8 = 8000.0;
		const double target_gain = wtvsoftmodem_dsp::db_to_amplitude(3.0);

		const int32_t factor = wtvsoftmodem_dsp::round_nearest<int32_t>(fs_out / fs8);
		if (factor >= 1 && std::fabs(fs_out - fs8 * factor) < 1.0)
		{
			return at_rate(variant, uint32_t(factor), carrier_hz);
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
