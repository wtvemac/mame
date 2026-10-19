// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34data.h"
#include "wtvsoftmodem_dsp.h"
#include "wtvsoftmodem_v34constellation.h"
#include "wtvsoftmodem_v34filter.h"
#include "wtvsoftmodem_v34map.h"

#include <bit>
#include <algorithm>
#include <cmath>

// ITU-T V.34 8 Framing, 9 Encoder

using namespace wtvsoftmodem::v34;

wtvsoftmodem_v34data::wtvsoftmodem_v34data()
{
	init(config_t {});
}

bool wtvsoftmodem_v34data::config_t::apply_tx_gains()
{
	native_gains = false;

	const int32_t rate = (bit_rate_code >> 1) + 1;

	if (baud_rate_code != 5 || !expanded_shaping || (bit_rate_code & 1) || rate < 2 || rate > 14)
	{
		return false;
	}
	else
	{
		bool precoded = false;
		if (use_precoder)
		{
			for (const auto &tap : precoder_coeffs)
				precoded |= (tap[0] != 0 || tap[1] != 0);
		}

		static constexpr int32_t PRE[2][15] = {
			{ 0, 0, 23296, 17152, 13824, 10880, 8576, 6784, 5312, 4160, 3264, 2560, 2032, 1584, 1248 },
			{ 0, 0, 19968, 15872, 13056, 10496, 8448, 6720, 5248, 4160, 3232, 2560, 2016, 1568, 1248 }
		};

		static constexpr int32_t POST[2][15] = {
			{ 512, 512, 486, 466, 474, 472, 473, 471, 472, 472, 473, 472, 471, 473, 472 },
			{ 512, 512, 486, 472, 476, 474, 474, 472, 473, 472, 473, 472, 472, 473, 472 }
		};

		nonlin_prescale  = PRE[precoded][rate];
		nonlin_postscale = POST[precoded][rate];

		native_gains = true;

		return true;
	}
}

void wtvsoftmodem_v34data::init(const config_t &cfg)
{
	m_cfg = cfg;
	update_inverse_scales();

	const auto mapping = map::parameters::derive(cfg.baud_rate_code, cfg.bit_rate_code, cfg.expanded_shaping);

	m_bit_rate = mapping.bit_rate;
	m_b = mapping.b;
	m_k = mapping.k;
	m_q = mapping.q;
	m_m = mapping.m;
	m_j = mapping.j;
	m_p = mapping.p;
	m_w = mapping.w;
	m_r = mapping.r;

	m_scram.set_tx_channel(cfg.answer_mode ? wtvsoftmodem_scrambler::ANSWER_CHANNEL : wtvsoftmodem_scrambler::CALL_CHANNEL);
	m_scram.set_rx_channel(cfg.answer_mode ? wtvsoftmodem_scrambler::ANSWER_CHANNEL : wtvsoftmodem_scrambler::CALL_CHANNEL);

	m_nrm = DEMAP_UNIT / TRN_4PT_REF;

	m_q7_scale = m_cfg.native_gains ? (1.0 / 8192.0) : (m_nrm / 128.0);
	m_pow_norm = 1.0;

	reset();

	if (!m_cfg.native_gains)
	{
		auto save = m_get_bit;
		m_get_bit = nullptr;

		double acc = 0.0;
		int32_t n = 0;
		const int32_t frames = 8 * m_p + 4;
		for (int32_t f = 0; f < frames; f++)
		{
			map_frame();

			for (const auto &p : m_sym)
			{
				acc += p.i * p.i + p.q * p.q;
				n++;
			}
		}

		const double mean_pow = n ? acc / n : 1.0;
		const double target = m_cfg.point_scale * m_cfg.point_scale;

		m_pow_norm = (m_cfg.match_trn_power && mean_pow > 1e-12) ? std::sqrt(target / mean_pow) : 1.0;
		m_get_bit = save;
	}
	m_q7_scale = m_cfg.native_gains ? 1.0 / 8192.0 : (m_nrm / 128.0) * m_pow_norm;

	auto decisions = std::make_shared<decision_table>();
	m_decisions = decisions;

	const int32_t point_count = std::min(416, m_m << m_q);

	decisions->base.reserve(point_count * 4);
	decisions->lattice.reserve(point_count * 4);
	decisions->axis.reserve(point_count * 4);
	decisions->points.reserve(point_count * 4);

	m_decision_scale = (m_cfg.invert_output ? -1.0 : 1.0) * m_q7_scale * m_cfg.point_scale;

	for (int32_t mi = 0; mi < point_count; mi++)
	{
		for (int32_t quadrant = 0; quadrant < 4; quadrant++)
		{
			int32_t re = constellation::SUPERCONSTELLATION[mi][0];
			int32_t im = constellation::SUPERCONSTELLATION[mi][1];
			constellation::rot90(re, im, quadrant);

			decisions->base.push_back({ double(re * 128), double(im * 128) });
			decisions->lattice.push_back({ re * 128, im * 128 });
			decisions->axis.push_back({ axis_index(re), axis_index(im) });

			int32_t xre = re * 128;
			int32_t xim = im * 128;
			encode_q7(xre, xim);

			decisions->points.push_back({ xre * m_decision_scale, xim * m_decision_scale });
		}
	}

	std::array<uint64_t, AXIS_LEVELS> present {};
	for (size_t idx = 0; idx < decisions->axis.size(); idx++)
	{
		const auto [re, im] = decisions->axis[idx];
		const uint64_t bit = uint64_t(1) << im;

		if (!(present[re] & bit))
		{
			present[re] |= bit;
			decisions->column_index[re][im] = uint16_t(idx);
		}
	}

	for (int32_t re = 0; re < AXIS_LEVELS; re++)
	{
		if (!present[re])
			continue;

		const int32_t first = std::countr_zero(present[re]);
		const int32_t last = 63 - std::countl_zero(present[re]);

		decisions->columns[re] = { uint8_t(first), uint8_t(last) };
		decisions->contiguous_columns &= std::popcount(present[re]) == last - first + 1;
	}

	encode_axis(0, decisions->linear_axis);

	reset();
}

void wtvsoftmodem_v34data::encode_q7(int32_t &re, int32_t &im) const
{
	if (!m_cfg.native_gains && m_cfg.nonlin_k == 0.0)
		return;

	auto scale = [&](int32_t value, int32_t gain)
	{
		if (m_cfg.native_gains)
			return int32_t(filter::nonlinear::narrow(filter::nonlinear::multiply(value, gain) >> 9));
		else
			return int32_t((int64_t(value) * gain) >> 9);
	};

	re = scale(re, m_cfg.nonlin_prescale);
	im = scale(im, m_cfg.nonlin_prescale);

	if (m_cfg.nonlin_k != 0.0)
	{
		filter::nonlinear::encode(re, im);

		if (m_cfg.native_gains)
		{
			re = filter::nonlinear::narrow(re);
			im = filter::nonlinear::narrow(im);
		}

		re = scale(re, m_cfg.nonlin_postscale);
		im = scale(im, m_cfg.nonlin_postscale);
	}
}

wtvsoftmodem_v34data::point_t wtvsoftmodem_v34data::receive_linear(point_t rx) const
{
	point_t result { rx.i / m_decision_scale, rx.q / m_decision_scale };

	if (m_cfg.native_gains || m_cfg.nonlin_k != 0.0)
	{
		auto scale = [](double value, int32_t gain)
		{
			const int16_t input = filter::nonlinear::narrow(wtvsoftmodem_dsp::round_nearest<int32_t>(value));

			return double(filter::nonlinear::narrow(filter::nonlinear::multiply(input, gain) >> 14));
		};

		if (m_cfg.nonlin_k != 0.0)
		{
			const auto output = filter::nonlinear::decode({ int16_t(scale(result.i, m_inverse_postscale)), int16_t(scale(result.q, m_inverse_postscale)) });

			result.i = output.i;
			result.q = output.q;
		}

		result.i = scale(result.i, m_inverse_prescale);
		result.q = scale(result.q, m_inverse_prescale);
	}

	return result;
}

wtvsoftmodem_v34data::point_t wtvsoftmodem_v34data::front_prediction() const
{
	if (!m_cfg.use_precoder)
		return { 0.0, 0.0 };

	const auto p = filter::precoder::predict(
		m_cfg.precoder_coeffs,
		[&](int32_t i) -> decltype(auto)
		{
			return m_front_xhist[i];
		}
	);

	return { double(p[0]), double(p[1]) };
}

void wtvsoftmodem_v34data::encode_axis(int32_t offset, axis_encoding &out) const
{
	static_assert(
		[]
		{
			for (const auto &point : constellation::SUPERCONSTELLATION)
			{
				for (int32_t coordinate : point)
				{
					if (!(coordinate & 1) || coordinate < -AXIS_LIMIT || coordinate > AXIS_LIMIT)
						return false;
				}
			}

			return true;
		}(),
		"superconstellation coordinates must be odd and within AXIS_LIMIT"
	);

	for (int32_t level = 0; level < AXIS_LEVELS; level++)
	{
		int32_t re = (2 * level - AXIS_LIMIT) * 128 + offset;
		int32_t im = 0;
		encode_q7(re, im);
		out[level] = re * m_decision_scale;
	}
}

int32_t wtvsoftmodem_v34data::nearest_separable(point_t rx, const axis_encoding &re, const axis_encoding &im, double limit) const
{
	const auto &table = *m_decisions;

	bool rising = true;
	bool falling = true;
	for (int32_t level = 1; level < AXIS_LEVELS; level++)
	{
		rising &= im[level - 1] <= im[level];
		falling &= im[level - 1] >= im[level];
	}

	int32_t best_idx = -1;

	if (!table.contiguous_columns || !(rising || falling))
	{
		for (size_t idx = 0; idx < table.axis.size(); idx++)
		{
			const double di = rx.i - re[table.axis[idx][0]];
			const double dq = rx.q - im[table.axis[idx][1]];
			const double distance = di * di + dq * dq;

			if (distance < limit)
			{
				limit = distance;
				best_idx = int32_t(idx);
			}
		}

		return best_idx;
	}

	std::array<double, AXIS_LEVELS> di2;
	std::array<double, AXIS_LEVELS> dq2;
	int32_t nearest_q = 0;
	for (int32_t level = 0; level < AXIS_LEVELS; level++)
	{
		const double di = rx.i - re[level];
		const double dq = rx.q - im[level];

		di2[level] = di * di;
		dq2[level] = dq * dq;

		if (dq2[level] < dq2[nearest_q])
			nearest_q = level;
	}

	double best = limit;
	for (int32_t level = 0; level < AXIS_LEVELS; level++)
	{
		const auto &column = table.columns[level];
		if (column.first > column.last)
			continue;

		const double distance = di2[level] + dq2[std::clamp<int32_t>(nearest_q, column.first, column.last)];
		if (distance < best)
			best = distance;
	}

	if (!(best < limit))
		return -1;

	for (int32_t level = 0; level < AXIS_LEVELS; level++)
	{
		const auto &column = table.columns[level];
		if (column.first > column.last)
			continue;

		const int32_t center = std::clamp<int32_t>(nearest_q, column.first, column.last);
		if (di2[level] + dq2[center] != best)
			continue;

		int32_t low = center;
		while (low > column.first && di2[level] + dq2[low - 1] == best)
			low--;

		int32_t high = center;
		while (high < column.last && di2[level] + dq2[high + 1] == best)
			high++;

		for (int32_t q = low; q <= high; q++)
		{
			const int32_t idx = table.column_index[level][q];
			if (best_idx < 0 || idx < best_idx)
				best_idx = idx;
		}
	}

	return best_idx;
}

wtvsoftmodem_v34data::point_t wtvsoftmodem_v34data::snap(point_t rx) const
{
	point_t best {};

	if (!m_decisions)
		return best;

	double best_distance = INFINITY;

	if (m_cfg.use_precoder)
	{
		const auto prediction = front_prediction();
		const int32_t pre = int32_t(prediction.i);
		const int32_t pim = int32_t(prediction.q);
		const int32_t cre = filter::precoder::quantize_coset(pre, m_b >= COSET_WIDE_BITS);
		const int32_t cim = filter::precoder::quantize_coset(pim, m_b >= COSET_WIDE_BITS);

		if (separable_encoding())
		{
			axis_encoding encoded_re;
			axis_encoding encoded_im;
			encode_axis(cre * 128 - pre, encoded_re);
			encode_axis(cim * 128 - pim, encoded_im);

			const int32_t idx = nearest_separable(rx, encoded_re, encoded_im, INFINITY);
			if (idx >= 0)
				best = { encoded_re[m_decisions->axis[idx][0]], encoded_im[m_decisions->axis[idx][1]] };

			return best;
		}
		else
		{
			for (const auto &base : m_decisions->lattice)
			{
				int32_t xre = base[0] + cre * 128 - pre;
				int32_t xim = base[1] + cim * 128 - pim;
				encode_q7(xre, xim);

				const point_t candidate{ xre * m_decision_scale, xim * m_decision_scale };
				const double  di = rx.i - candidate.i;
				const double  dq = rx.q - candidate.q;
				const double  distance = di * di + dq * dq;

				if (distance < best_distance)
				{
					best_distance = distance;
					best          = candidate;
				}
			}

			return best;
		}
	}
	else if (separable_encoding())
	{
		const auto &axis = m_decisions->linear_axis;

		const int32_t idx = nearest_separable(rx, axis, axis, INFINITY);
		if (idx >= 0)
			best = m_decisions->points[idx];

		return best;
	}
	else
	{
		for (const point_t &candidate : m_decisions->points)
		{
			const double di = rx.i - candidate.i;
			const double dq = rx.q - candidate.q;
			const double distance = di * di + dq * dq;

			if (distance < best_distance)
			{
				best_distance = distance;
				best = candidate;
			}
		}

		return best;
	}
}

void wtvsoftmodem_v34data::set_precoder(const int16_t coeffs[3][2])
{
	bool any = false;

	for (int32_t i = 0; i < 3; i++)
	{
		for (int32_t j = 0; j < 2; j++)
		{
			m_cfg.precoder_coeffs[i][j] = coeffs ? coeffs[i][j] : 0;
			if (m_cfg.precoder_coeffs[i][j])
				any = true;
		}
	}

	m_cfg.use_precoder = any;
}

void wtvsoftmodem_v34data::reset()
{
	m_scram.reset();
	m_s_bit_cnt = 0;
	m_aux_bit_cnt = 0;

	m_sfs_count = 0;

	m_v0_reg = V0_REG_SEED >> ((2 * m_j + 30) & 31);
	m_z = 0;
	m_y0 = 0;
	m_state = 0;
	m_sym_pos = 8;

	for (auto &x : m_xhist)
	{
		x[0] = 0;
		x[1] = 0;
	}

	m_c_re = 0;
	m_c_im = 0;
	m_p_re = 0;
	m_p_im = 0;

	m_rx_sym_pos = 0;
	m_front_xhist = {};
	m_rx_trellis.reset();
	m_shared_geometry = nullptr;
	m_rx_top2[0] = 0;
	m_rx_top2[1] = 0;
	m_rx_top2[2] = 0;
	m_rx_top2[3] = 0;
	m_rx_subsets[0] = 0;
	m_rx_subsets[1] = 0;
	m_rx_u0 = 0;
	m_rx_sync_ok = true;
	m_z = 0;
}

wtvsoftmodem_v34data::point_t wtvsoftmodem_v34data::next_symbol()
{
	if (m_sym_pos >= 8)
	{
		map_frame();
		m_sym_pos = 0;
	}

	return m_sym[m_sym_pos++];
}

void wtvsoftmodem_v34data::parse_bitstream()
{
	int32_t bb = m_b;
	int32_t kk = m_k;

	m_s_bit_cnt += m_r;

	if (m_s_bit_cnt >= m_p)
	{
		m_s_bit_cnt -= m_p;
	}
	else if (bb > 12)
	{
		bb--;
		kk--;
	}

	auto data_bit = [&]() -> int32_t
	{
		int32_t d = m_get_bit ? m_get_bit() : 1;
		if (d < 0)
			d = 1;

		return m_scram.scramble(d & 1) ? 1 : 0;
	};

	int32_t aux_here = 0;
	m_aux_bit_cnt += m_w;
	if (m_w && m_aux_bit_cnt >= m_p)
	{
		m_aux_bit_cnt -= m_p;
		aux_here = 1;
	}

	int32_t n = 0;
	if (aux_here)
	{
		for (; n < kk; n++)
			m_framebits[n] = uint8_t(data_bit());
		m_framebits[n++] = 0;
	}
	for (; n < bb; n++)
		m_framebits[n] = uint8_t(data_bit());

	auto get = [&](int32_t &pos, int32_t w)
	{
		uint32_t v = 0;
		for (int32_t i = 0; i < w; i++)
			v |= uint32_t(m_framebits[pos++] & 1) << i;

		return v;
	};

	int32_t pos = 0;
	if (m_k)
	{
		m_r0 = get(pos, kk);
		for (int32_t i = 0; i < 4; i++)
		{
			m_ibits[i] = int32_t(get(pos, 3));
			if (m_q)
			{
				m_qbits[2 * i]     = int32_t(get(pos, m_q));
				m_qbits[2 * i + 1] = int32_t(get(pos, m_q));
			}
			else
			{
				m_qbits[2 * i] = 0;
				m_qbits[2 * i + 1] = 0;
			}
		}
	}
	else
	{

		m_r0      = 0;
		int32_t three = bb - 8;
		int32_t i = 0;
		for (; i < three; i++)
			m_ibits[i] = int32_t(get(pos, 3));
		for (; i < 4; i++)
			m_ibits[i] = int32_t(get(pos, 2));
		for (int32_t q = 0; q < 8; q++)
			m_qbits[q] = 0;
	}
}

void wtvsoftmodem_v34data::shell_map()
{
	shell_map_fwd(m_m, m_r0, m_mjk);
}

void wtvsoftmodem_v34data::shell_map_fwd(int32_t m_ring_count, uint32_t r0_in, int32_t mjk[8])
{
	const int32_t m_m = m_ring_count;
	if (m_m == 0)
	{
		for (int32_t i = 0; i < 8; i++)
			mjk[i] = 0;

		return;
	}

	const uint32_t *g2 = map::shell::G2S[m_m];
	const uint32_t *g4 = map::shell::G4S[m_m];
	const uint32_t *z8 = map::shell::Z8S[m_m];
	const int64_t r0 = int64_t(r0_in);

	int32_t a;
	for (a = 1; int64_t(z8[a]) <= r0; a++)
	{
	}
	a--;

	int64_t t1 = 0;
	int64_t t2 = r0 - int64_t(z8[a]);
	int32_t b = -1;
	while (t2 >= 0)
	{
		b++;
		t1 = int64_t(g4[b]) * int64_t(g4[a - b]);
		t2 -= t1;
	}

	int32_t r1 = int32_t(t2 + t1);
	int32_t r2 = r1 % int32_t(g4[b]);
	int32_t r3 = (r1 - r2) / int32_t(g4[b]);

	t2    = r2;
	t1    = 0;

	int32_t c = -1;
	while (t2 >= 0)
	{
		c++;
		t1 = int64_t(g2[c]) * int64_t(g2[b - c]);
		t2 -= t1;
	}

	int32_t r4 = int32_t(t2 + t1);

	t2 = r3;
	t1 = 0;
	int32_t d = -1;
	while (t2 >= 0)
	{
		d++;
		t1 = int64_t(g2[d]) * int64_t(g2[a - b - d]);
		t2 -= t1;
	}

	int32_t r5 = int32_t(t2 + t1);
	int32_t e = r4 % int32_t(g2[c]);
	int32_t f = (r4 - e) / int32_t(g2[c]);
	int32_t g = r5 % int32_t(g2[d]);
	int32_t h = (r5 - g) / int32_t(g2[d]);

	if (c < m_m)
	{
		mjk[0] = e;
		mjk[1] = c - mjk[0];
	}
	else
	{
		mjk[1] = m_m - 1 - e;
		mjk[0] = c - mjk[1];
	}

	if (b - c < m_m)
	{
		mjk[2] = f;
		mjk[3] = b - c - mjk[2];
	}
	else
	{
		mjk[3] = m_m - 1 - f;
		mjk[2] = b - c - mjk[3];
	}

	if (d < m_m)
	{
		mjk[4] = g;
		mjk[5] = d - mjk[4];
	}
	else
	{
		mjk[5] = m_m - 1 - g;
		mjk[4] = d - mjk[5];
	}

	if (a - b - d < m_m)
	{
		mjk[6] = h;
		mjk[7] = a - b - d - mjk[6];
	}
	else
	{
		mjk[7] = m_m - 1 - h;
		mjk[6] = a - b - d - mjk[7];
	}
}

uint32_t wtvsoftmodem_v34data::inverse_shell_map(int32_t m_ring_count, const int32_t mjk[8], bool *valid)
{
	if (valid)
		*valid = false;

	if (m_ring_count <= 0 || m_ring_count >= int32_t(std::size(map::shell::LIMITS)) || !map::shell::Z8S[m_ring_count])
		return 0;
	
	const int32_t M = m_ring_count;

	for (uint32_t i = 0; i < 8; i++)
	{
		if (mjk[i] < 0 || mjk[i] >= M)
			return 0;
	}

		const uint32_t *g2 = map::shell::G2S[M];
	const uint32_t *g4 = map::shell::G4S[M];
	const uint32_t *z8 = map::shell::Z8S[M];

	auto recover_pair = [&](int32_t m0, int32_t m1, int32_t &sum)
	{
		sum = m0 + m1;
		return (sum < M) ? m0 : (M - 1 - m1);
	};

	int32_t c;
	int32_t bc;
	int32_t d;
	int32_t abd;

	const int32_t e = recover_pair(mjk[0], mjk[1], c);
	const int32_t f = recover_pair(mjk[2], mjk[3], bc);
	const int32_t g = recover_pair(mjk[4], mjk[5], d);
	const int32_t h = recover_pair(mjk[6], mjk[7], abd);

	const int32_t b   = c + bc;
	const int32_t a   = b + d + abd;
	const auto limits = map::shell::LIMITS[M];
	if (size_t(a) >= limits.z8)
		return 0;

	auto count2 = [&](int32_t index)
	{
		return size_t(index) < limits.g2 ? g2[index] : 0U;
	};

	auto count4 = [&](int32_t index)
	{
		return size_t(index) < limits.g4 ? g4[index] : 0U;
	};

	int64_t r4 = int64_t(f) * int64_t(count2(c)) + int64_t(e);
	int64_t r5 = int64_t(h) * int64_t(count2(d)) + int64_t(g);
	int64_t r2 = r4;
	for (int32_t i = 0; i < c; i++)
		r2 += int64_t(count2(i)) * int64_t(count2(b - i));

	int64_t r3 = r5;
	for (int32_t i = 0; i < d; i++)
		r3 += int64_t(count2(i)) * int64_t(count2(a - b - i));

	const int64_t r1 = r3 * int64_t(count4(b)) + r2;
	int64_t sum_ab = r1;
	for (int32_t i = 0; i < b; i++)
		sum_ab += int64_t(count4(i)) * int64_t(count4(a - i));

	if (valid)
		*valid = true;

	return uint32_t(int64_t(z8[a]) + sum_ab);
}

void wtvsoftmodem_v34data::map_frame()
{
	parse_bitstream();
	shell_map();

	int32_t subsets[2] = { 0, 0 };
	int32_t u0     = 0;

	for (int32_t step = 0; step < 8; step++)
	{
		int32_t mi = (m_mjk[step] << m_q) + m_qbits[step];
		if (mi < 0)
			mi = 0;

		if (mi > 415)
			mi = 415;

		int32_t vre = constellation::SUPERCONSTELLATION[mi][0];
		int32_t vim = constellation::SUPERCONSTELLATION[mi][1];

		int32_t rot;
		if ((step & 1) == 0)
		{
			m_z = (m_z + (m_ibits[step >> 1] >> 1)) & 3;

			rot = m_z;
		}
		else
		{
			rot = (m_z + ((m_ibits[step >> 1] & 1) << 1) + u0) & 3;
		}

		int32_t ure = vre;
		int32_t uim = vim;
		constellation::rot90(ure, uim, rot);

		const int32_t yre = ure + m_c_re;
		const int32_t yim = uim + m_c_im;
		const int32_t slot = XOFF + step;
		const int32_t xre = (yre << 7) - m_p_re;
		const int32_t xim = (yim << 7) - m_p_im;

		m_xhist[slot][0] = xre;
		m_xhist[slot][1] = xim;

		const double osign = m_cfg.invert_output ? -1.0 : 1.0;

		int32_t wre = xre;
		int32_t wim = xim;
		encode_q7(wre, wim);

		m_sym[step] = { osign * wre * m_q7_scale * m_cfg.point_scale, osign * wim * m_q7_scale * m_cfg.point_scale };

		subsets[step & 1] = trellis16::subset_label(yre, yim);

		int32_t c_prev_re = m_c_re;
		int32_t c_prev_im = m_c_im;
		if (m_cfg.use_precoder)
		{
			const auto p = filter::precoder::predict(m_cfg.precoder_coeffs, [&](int32_t i) -> decltype(auto) { return m_xhist[slot - i]; });
			m_p_re = p[0];
			m_p_im = p[1];
			const bool wide_coset = (m_b >= COSET_WIDE_BITS);
			m_c_re = filter::precoder::quantize_coset(m_p_re, wide_coset);
			m_c_im = filter::precoder::quantize_coset(m_p_im, wide_coset);
		}

		if ((step & 1) == 0)
		{
			int32_t c0 = 0;
			if (m_cfg.use_precoder)
				c0 = (((c_prev_re + c_prev_im) >> 1) ^ ((m_c_re + m_c_im) >> 1)) & 1;

			int32_t v0 = 0;
			if (m_sfs_count == 0)
			{
				m_v0_reg >>= 1;
				v0 = m_v0_reg & 1;
				if (m_v0_reg == 0)
					m_v0_reg = V0_REG_SEED;
			}

			m_sfs_count++;
			if (m_sfs_count >= 2 * m_p)
				m_sfs_count = 0;

			u0 = (m_state ^ c0 ^ v0) & 1;
		}
		else
		{
			int32_t y4321 = trellis16::ENCODER_INPUT[subsets[0] & 7][subsets[1] & 7];
			m_y0 = m_state & 1;
			m_state = trellis16::ENCODER_NEXT_STATE[m_state & 15][y4321 & 15];
		}
	}

	m_xhist[XOFF - 3][0] = m_xhist[XOFF + 5][0];
	m_xhist[XOFF - 3][1] = m_xhist[XOFF + 5][1];
	m_xhist[XOFF - 2][0] = m_xhist[XOFF + 6][0];
	m_xhist[XOFF - 2][1] = m_xhist[XOFF + 6][1];
	m_xhist[XOFF - 1][0] = m_xhist[XOFF + 7][0];
	m_xhist[XOFF - 1][1] = m_xhist[XOFF + 7][1];
}

bool wtvsoftmodem_v34data::reverse_lookup(int32_t yre, int32_t yim, int32_t &mi, int32_t &quadrant) const
{
	static constexpr int32_t LIMIT = []
	{
		int32_t bound = 0;

		for (const auto &point : constellation::SUPERCONSTELLATION)
		{
			for (int32_t coordinate : point)
				bound = std::max(bound, coordinate < 0 ? -coordinate : coordinate);
		}

		return bound;
	}();

	static constexpr int32_t SIDE  = 2 * LIMIT + 1;

	static constexpr auto TABLE = []
	{
		std::array<uint16_t, SIDE * SIDE> t {};

		for (int32_t m = 0; m < 416; m++)
		{
			for (int32_t q = 0; q < 4; q++)
			{
				int32_t re = constellation::SUPERCONSTELLATION[m][0];
				int32_t im = constellation::SUPERCONSTELLATION[m][1];
				constellation::rot90(re, im, q);

				auto &entry = t[(re + LIMIT) * SIDE + im + LIMIT];
				if (!entry)
					entry = uint16_t(1 + 4 * m + q);
			}
		}

		return t;
	}();

	if (yre < -LIMIT || yre > LIMIT || yim < -LIMIT || yim > LIMIT)
		return false;

	const uint16_t entry = TABLE[(yre + LIMIT) * SIDE + yim + LIMIT];
	if (!entry)
		return false;

	mi = (entry - 1) / 4;
	quadrant = (entry - 1) % 4;

	return true;
}

wtvsoftmodem_v34data::point_t wtvsoftmodem_v34data::encode_candidate(int32_t re, int32_t im) const
{
	encode_q7(re, im);

	return { re * m_decision_scale, im * m_decision_scale };
}

inline void wtvsoftmodem_v34data::demap_frame_finish()
{
	int32_t mjk[8] = {};
	bool ok = true;

	for (int32_t step = 0; step < 8; step++)
	{
		const point_t &sym = m_rx_sym[step];
		const bool trellis_decided = m_rx_trellis && !m_rx_trellis->pending_symbols;

		demapped_t d;
		if (trellis_decided)
			d = demap_trellis(sym, ok);
		else if (m_exact_demap)
			d = demap_exact(sym);
		else
			d = demap_nearest(m_rx_linear[step], ok);

		if (trellis_decided)
			emit_reference(d.xre, d.xim);

		m_rx_subsets[step & 1] = trellis16::subset_label(d.yre, d.yim);
		mjk[step] = (d.mi >= 0) ? (d.mi >> m_q) : 0;
		m_qbits[step] = (d.mi >= 0 && m_q > 0) ? (d.mi & ((1 << m_q) - 1)) : 0;

		track_quadrant(step, d.quadrant);

		const int32_t c_prev_re = m_c_re;
		const int32_t c_prev_im = m_c_im;
		if (m_cfg.use_precoder)
			advance_rx_precoder(step, d.xre, d.xim);

		advance_rx_trellis(step, c_prev_re, c_prev_im);
	}

	if (m_cfg.use_precoder)
	{
		for (int32_t k = 0; k < 3; k++)
		{
			m_xhist[XOFF - 3 + k][0] = m_xhist[XOFF + 5 + k][0];
			m_xhist[XOFF - 3 + k][1] = m_xhist[XOFF + 5 + k][1];
		}
	}

	m_r0 = ok ? inverse_shell_map(m_m, mjk, &ok) : 0;
	m_rx_sync_ok = ok;

	unparse_bitstream(m_r0, m_ibits, m_qbits, ok);
}

inline wtvsoftmodem_v34data::demapped_t wtvsoftmodem_v34data::demap_trellis(const point_t &sym, bool &ok) const
{
	demapped_t d;

	d.yre = wtvsoftmodem_dsp::round_nearest<int32_t>(sym.i / double(LATTICE_SCALE));
	d.yim = wtvsoftmodem_dsp::round_nearest<int32_t>(sym.q / double(LATTICE_SCALE));

	if (!reverse_lookup(d.yre - m_c_re, d.yim - m_c_im, d.mi, d.quadrant))
	{
		ok = false;
		d.mi = 0;
		d.quadrant = 0;
	}

	d.xre = d.yre * LATTICE_SCALE - m_p_re;
	d.xim = d.yim * LATTICE_SCALE - m_p_im;

	return d;
}

inline wtvsoftmodem_v34data::demapped_t wtvsoftmodem_v34data::demap_exact(const point_t &sym) const
{
	demapped_t d;
	double best_d2 = DEMAP_MAX_D2;

	const int32_t offset_re = m_cfg.use_precoder ? m_c_re * LATTICE_SCALE - m_p_re : 0;
	const int32_t offset_im = m_cfg.use_precoder ? m_c_im * LATTICE_SCALE - m_p_im : 0;

	int32_t best_idx = -1;

	if (separable_encoding())
	{
		axis_encoding encoded_re;
		axis_encoding encoded_im;
		encode_axis(offset_re, encoded_re);
		encode_axis(offset_im, encoded_im);

		best_idx = nearest_separable(sym, encoded_re, encoded_im, best_d2);
	}
	else
	{
		for (size_t idx = 0; idx < m_decisions->base.size(); idx++)
		{
			const auto &lattice = m_decisions->lattice[idx];
			const point_t enc = encode_candidate(lattice[0] + offset_re, lattice[1] + offset_im);

			const double di = sym.i - enc.i;
			const double dq = sym.q - enc.q;
			const double d2 = di * di + dq * dq;

			if (d2 < best_d2)
			{
				best_d2 = d2;
				best_idx = int32_t(idx);
			}
		}
	}

	if (best_idx >= 0)
	{
		const auto &lattice = m_decisions->lattice[best_idx];
		const point_t &base = m_decisions->base[best_idx];

		d.mi = best_idx / 4;
		d.quadrant = best_idx % 4;

		d.xre = lattice[0] + offset_re;
		d.xim = lattice[1] + offset_im;

		d.yre = wtvsoftmodem_dsp::round_nearest<int32_t>(base.i / double(LATTICE_SCALE)) + (m_cfg.use_precoder ? m_c_re : 0);
		d.yim = wtvsoftmodem_dsp::round_nearest<int32_t>(base.q / double(LATTICE_SCALE)) + (m_cfg.use_precoder ? m_c_im : 0);
	}

	return d;
}

inline wtvsoftmodem_v34data::demapped_t wtvsoftmodem_v34data::demap_nearest(const point_t &linear, bool &ok) const
{
	demapped_t d;

	const double xre_f = linear.i;
	const double xim_f = linear.q;

	double y0re_f = 0.0;
	double y0im_f = 0.0;
	double yre_f;
	double yim_f;
	if (m_cfg.use_precoder)
	{
		y0re_f = (xre_f + double(m_p_re)) / double(LATTICE_SCALE);
		y0im_f = (xim_f + double(m_p_im)) / double(LATTICE_SCALE);

		yre_f = y0re_f - double(m_c_re);
		yim_f = y0im_f - double(m_c_im);
	}
	else
	{
		yre_f = xre_f / double(LATTICE_SCALE);
		yim_f = xim_f / double(LATTICE_SCALE);
	}

	const int32_t yre0 = wtvsoftmodem_dsp::round_nearest<int32_t>(yre_f);
	const int32_t yim0 = wtvsoftmodem_dsp::round_nearest<int32_t>(yim_f);

	d.yre = yre0;
	d.yim = yim0;
	double best_d2 = DEMAP_MAX_D2;

	for (int32_t window : { 2, 8 })
	{
		if (window > 2)
			ok = false;

		for (int32_t cyre = (yre0 - window) | 1; cyre <= yre0 + window; cyre += 2)
		{
			for (int32_t cyim = (yim0 - window) | 1; cyim <= yim0 + window; cyim += 2)
			{
				int32_t cmi = -1;
				int32_t cq = 0;

				if (!reverse_lookup(cyre, cyim, cmi, cq))
					continue;

				const double d2 = (cyre - yre_f) * (cyre - yre_f) + (cyim - yim_f) * (cyim - yim_f);
				if (d2 < best_d2)
				{
					best_d2 = d2;
					d.mi = cmi;
					d.quadrant = cq;
					d.yre = cyre;
					d.yim = cyim;
				}
			}
		}

		if (d.mi >= 0)
			break;
	}

	if (d.mi < 0)
		d.quadrant = 0;

	d.xre = wtvsoftmodem_dsp::round_nearest<int32_t>(xre_f);
	d.xim = wtvsoftmodem_dsp::round_nearest<int32_t>(xim_f);
	if (m_cfg.use_precoder)
	{
		d.yre = wtvsoftmodem_dsp::round_nearest<int32_t>(y0re_f);
		d.yim = wtvsoftmodem_dsp::round_nearest<int32_t>(y0im_f);
	}

	return d;
}

inline void wtvsoftmodem_v34data::emit_reference(int32_t xre, int32_t xim)
{
	const uint64_t epoch = m_rx_trellis->output_epoch++;
	if (!m_reference_sink)
		return;

	encode_q7(xre, xim);
	m_reference_sink({ xre * m_decision_scale, xim * m_decision_scale }, epoch);
}

inline void wtvsoftmodem_v34data::track_quadrant(int32_t step, int32_t quadrant)
{
	const int32_t pair = step >> 1;

	if ((step & 1) == 0)
	{
		m_rx_top2[pair] = (quadrant - m_z) & 3;
		m_z = quadrant;
	}
	else
	{
		m_ibits[pair] = (m_rx_top2[pair] << 1) | (((quadrant - m_z) & 3) >> 1);
	}
}

inline void wtvsoftmodem_v34data::advance_rx_precoder(int32_t step, int32_t xre, int32_t xim)
{
	const int32_t slot = XOFF + step;

	m_xhist[slot][0] = xre;
	m_xhist[slot][1] = xim;

	const auto p = filter::precoder::predict(
		m_cfg.precoder_coeffs,
		[&](int32_t i) -> decltype(auto)
		{
			return m_xhist[slot - i];
		}
	);
	m_p_re = p[0];
	m_p_im = p[1];

	const bool wide_coset = (m_b >= COSET_WIDE_BITS);
	m_c_re = filter::precoder::quantize_coset(m_p_re, wide_coset);
	m_c_im = filter::precoder::quantize_coset(m_p_im, wide_coset);
}

inline void wtvsoftmodem_v34data::advance_rx_trellis(int32_t step, int32_t c_prev_re, int32_t c_prev_im)
{
	if ((step & 1) == 0)
	{
		int32_t c0 = 0;
		if (m_cfg.use_precoder)
			c0 = (((c_prev_re + c_prev_im) >> 1) ^ ((m_c_re + m_c_im) >> 1)) & 1;

		int32_t v0 = 0;
		if (m_sfs_count == 0)
		{
			m_v0_reg >>= 1;
			v0 = m_v0_reg & 1;

			if (m_v0_reg == 0)
				m_v0_reg = V0_REG_SEED;
		}

		m_sfs_count++;
		if (m_sfs_count >= (2 * m_p))
			m_sfs_count = 0;

		m_rx_u0 = (m_y0 ^ c0 ^ v0) & 1;
	}
	else
	{
		const int32_t y4321 = trellis16::ENCODER_INPUT[m_rx_subsets[0] & 7][m_rx_subsets[1] & 7];
		m_y0 = m_state & 1;
		m_state = trellis16::ENCODER_NEXT_STATE[m_state & 15][y4321 & 15];
	}
}

inline void wtvsoftmodem_v34data::unparse_bitstream(uint32_t r0, const int32_t ibits[4], const int32_t qbits[8], bool deliver)
{
	int32_t bb = m_b;
	int32_t kk = m_k;

	m_s_bit_cnt += m_r;

	if (m_s_bit_cnt >= m_p)
	{
		m_s_bit_cnt -= m_p;
	}
	else if (bb > 12)
	{
		bb--;
		kk--;
	}

	int32_t aux_here = 0;
	m_aux_bit_cnt += m_w;
	if (m_w && m_aux_bit_cnt >= m_p)
	{
		m_aux_bit_cnt -= m_p;
		aux_here = 1;
	}

	if (m_collect_bits)
	{
		wtvsoftmodem_bitblock line;

		if (m_k)
		{
			line.append(r0, kk);
			for (int32_t i = 0; i < 4; i++)
			{
				line.append(uint32_t(ibits[i]), 3);
				if (m_q)
				{
					line.append(uint32_t(qbits[2 * i]), m_q);
					line.append(uint32_t(qbits[2 * i + 1]), m_q);
				}
			}
		}
		else
		{
			const int32_t three = bb - 8;
			for (int32_t i = 0; i < 4; i++)
				line.append(uint32_t(ibits[i]), i < three ? 3 : 2);
		}

		line.truncate(bb);

		if (aux_here)
			line.erase(kk);

		m_scram.descramble(line);
		m_collected = deliver ? line : wtvsoftmodem_bitblock{};
	}
	else
	{
		auto put = [&](int32_t &pos, int32_t w, uint32_t v)
		{
			for (int32_t i = 0; i < w; i++)
				m_framebits[pos++] = uint8_t((v >> i) & 1);
		};

		int32_t pos = 0;
		if (m_k)
		{
			put(pos, kk, r0);
			for (int32_t i = 0; i < 4; i++)
			{
				put(pos, 3, uint32_t(ibits[i]));
				if (m_q)
				{
					put(pos, m_q, uint32_t(qbits[2 * i]));
					put(pos, m_q, uint32_t(qbits[2 * i + 1]));
				}
			}
		}
		else
		{

			const int32_t three = bb - 8;
			int32_t i= 0;

			for (; i < three; i++)
				put(pos, 3, uint32_t(ibits[i]));

			for (; i < 4; i++)
				put(pos, 2, uint32_t(ibits[i]));
		}

		auto consume = [&](uint8_t line_bit)
		{
			const int32_t bit = m_scram.descramble(line_bit != 0) ? 1 : 0;
			if (deliver && m_put_bit)
				m_put_bit(bit);
		};

		int32_t bit_idx = 0;
		if (aux_here)
		{
			for (; bit_idx < kk; bit_idx++)
				consume(m_framebits[bit_idx]);

			bit_idx++;
		}
		for (; bit_idx < bb; bit_idx++)
			consume(m_framebits[bit_idx]);
	}
}

void wtvsoftmodem_v34data::observe_front_end(point_t rx)
{
	observe_linear(receive_linear(rx));
}

inline void wtvsoftmodem_v34data::observe_linear(point_t linear)
{
	m_front_xhist[2] = m_front_xhist[1];
	m_front_xhist[1] = m_front_xhist[0];
	m_front_xhist[0] = { wtvsoftmodem_dsp::round_nearest<int32_t>(linear.i), wtvsoftmodem_dsp::round_nearest<int32_t>(linear.q) };
}

bool wtvsoftmodem_v34data::enable_trellis(uint64_t epoch)
{
	if (m_cfg.bit_rate_code & 1)
		return false;

	m_rx_trellis = std::make_shared<rx_trellis_state>();

	m_rx_trellis->pending_symbols = (8 - m_rx_sym_pos) % 8;
	m_rx_trellis->output_epoch = epoch + m_rx_trellis->pending_symbols;

	const uint64_t coding_epoch = (m_rx_trellis->output_epoch + 4) & ~uint64_t(7);

	m_rx_trellis->reg = V0_REG_SEED >> ((2 * m_j + 30) & 31);

	for (uint64_t i = 0; i < coding_epoch / 2; i++)
		m_rx_trellis->next_inversion(m_p);

	return true;
}

void wtvsoftmodem_v34data::finish_shared_geometry()
{
	if (!m_shared_geometry)
		return;

	const auto &geometry = *m_shared_geometry;

	m_rx_sym = geometry.m_rx_sym;
	m_rx_linear = geometry.m_rx_linear;
	m_rx_sym_pos = geometry.m_rx_sym_pos;
	m_front_xhist = geometry.m_front_xhist;
	m_rx_sync_ok = geometry.m_rx_sync_ok;
	m_r0  = geometry.m_r0;

	std::copy(std::begin(geometry.m_ibits), std::end(geometry.m_ibits), std::begin(m_ibits));
	std::copy(std::begin(geometry.m_qbits), std::end(geometry.m_qbits), std::begin(m_qbits));
	std::copy(std::begin(geometry.m_rx_top2), std::end(geometry.m_rx_top2), std::begin(m_rx_top2));
	std::copy(std::begin(geometry.m_rx_subsets), std::end(geometry.m_rx_subsets), std::begin(m_rx_subsets));

	for (uint32_t i = 0; i < 11; i++)
	{
		for (uint32_t axis = 0; axis < 2; axis++)
			m_xhist[i][axis] = geometry.m_xhist[i][axis];
	}

	m_p_re = geometry.m_p_re;
	m_p_im = geometry.m_p_im;
	m_c_re = geometry.m_c_re;
	m_c_im = geometry.m_c_im;
	m_z = geometry.m_z;
	m_y0 = geometry.m_y0;
	m_state = geometry.m_state;
	m_rx_u0 = geometry.m_rx_u0;
	m_sfs_count = geometry.m_sfs_count;
	m_v0_reg = geometry.m_v0_reg;
	m_shared_geometry = nullptr;
}

bool wtvsoftmodem_v34data::put_symbol_shared(const wtvsoftmodem_v34data &geometry)
{
	m_shared_geometry = &geometry;
	m_front_xhist     = geometry.m_front_xhist;
	m_rx_sync_ok      = geometry.m_rx_sync_ok;
	m_rx_sym_pos      = geometry.m_rx_sym_pos;

	if (m_rx_sym_pos == 0)
	{
		unparse_bitstream(geometry.m_r0, geometry.m_ibits, geometry.m_qbits, m_rx_sync_ok);

		return m_rx_sync_ok;
	}
	else
	{
		return true;
	}
}

inline void wtvsoftmodem_v34data::store_received(point_t rx, point_t linear)
{
	observe_linear(linear);

	m_rx_linear[m_rx_sym_pos] = linear;
	m_rx_sym[m_rx_sym_pos++] = rx;
}

bool wtvsoftmodem_v34data::put_symbol(point_t rx)
{
	finish_shared_geometry();

	if (m_rx_trellis && !m_rx_trellis.unique())
		m_rx_trellis = std::make_shared<rx_trellis_state>(*m_rx_trellis);

	if (m_rx_trellis && m_rx_trellis->pending_symbols)
	{
		store_received(rx, receive_linear(rx));

		if (m_rx_sym_pos == 8)
		{
			m_rx_sym_pos = 0;
			demap_frame_finish();
		}

		m_rx_trellis->pending_symbols--;

		return m_rx_sync_ok;
	}
	else if (m_rx_trellis)
	{
		auto &state = *m_rx_trellis;
		const auto x = receive_linear(rx);
		const auto prediction = front_prediction();

		observe_linear(x);

		state.pair[state.fill++] = x.i + prediction.i;
		state.pair[state.fill++] = x.q + prediction.q;

		if (state.fill != 4)
			return true;

		state.fill = 0;

		trellis16::decoder::point4 output;
		if (!state.decoder.put(state.pair, state.next_inversion(m_p), output))
			return true;

		m_rx_sym[m_rx_sym_pos++] = { output[0], output[1] };
		m_rx_sym[m_rx_sym_pos++] = { output[2], output[3] };

		if (m_rx_sym_pos != 8)
			return true;

		m_rx_sym_pos = 0;

		demap_frame_finish();

		return m_rx_sync_ok;
	}
	else
	{
		store_received(rx, receive_linear(rx));

		if (m_rx_sym_pos >= 8)
		{
			m_rx_sym_pos = 0;
			demap_frame_finish();

			return m_rx_sync_ok;
		}
		else
		{
			return true;
		}
	}
}
