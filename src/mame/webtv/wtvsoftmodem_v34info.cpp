// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34info.h"
#include "wtvsoftmodem_dsp.h"

#include <algorithm>
#include <cmath>

// ITU-T V.34 10.1.2.3 INFO sequences, ITU-T V.90 8.2.3 INFO sequences

void wtvsoftmodem_v34dpsk::reset()
{
	wtvsoftmodem_dpsk::reset();
	m_rx_history.clear();
}

void wtvsoftmodem_v34dpsk::rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	if (m_rx_history.size() + sample_count <= HISTORY_CAP)
		m_rx_history.insert(m_rx_history.end(), sample, sample + sample_count);

	wtvsoftmodem_dpsk::rx(sample, sample_count);
}

std::vector<uint8_t> wtvsoftmodem_v34dpsk::rx_decode_buffered(double rate_trim, double phase0) const
{
	wtvsoftmodem_dsp::oscillator rx_carrier;

	rx_carrier.set_step(rx_step());

	double rx_sym_phase = phase0 - std::floor(phase0);
	double acc_i = 0.0;
	double acc_q = 0.0;
	double prev_i = 0.0;
	double prev_q = 0.0;
	bool have_prev = false;

	std::vector<uint8_t> bits;
	bits.reserve(m_rx_history.size() / std::max(1.0, rx_sps()));

	for (int32_t raw : m_rx_history)
	{
		const double c = rx_carrier.re;
		const double sn = rx_carrier.im;
		const double v = double(raw);

		acc_i += v * c;
		acc_q += -v * sn;
		rx_carrier.advance();

		rx_sym_phase += (1.0 / rx_sps()) * (1.0 + rate_trim);
		if (rx_sym_phase >= 1.0)
		{
			const double zi = acc_i;
			const double zq = acc_q;

			rx_sym_phase -= 1.0;

			acc_i = 0.0;
			acc_q = 0.0;

			if (have_prev)
			{
				const double dot_re = zi * prev_i + zq * prev_q;

				bits.push_back(dot_re < 0.0 ? 1 : 0);
			}

			prev_i = zi;
			prev_q = zq;
			have_prev = true;
		}
	}

	return bits;
}

namespace wtvsoftmodem::v34::info
{
	namespace
	{

		void put_msb(std::vector<uint8_t> &v, uint32_t value, int32_t nbits)
		{
			for (int32_t b = nbits - 1; b >= 0; b--)
				v.push_back(uint8_t((value >> b) & 1));
		}

		void put_lsb(std::vector<uint8_t> &v, uint32_t value, int32_t nbits)
		{
			for (int32_t b = 0; b < nbits; b++)
				v.push_back(uint8_t((value >> b) & 1));
		}

		uint32_t get_lsb(const uint8_t *bits, uint32_t &pos, int32_t nbits)
		{
			uint32_t v = 0;

			for (int32_t b = 0; b < nbits; b++)
				v |= uint32_t(bits[pos++] & 1) << b;

			return v;
		}

		int16_t sign_extend10(uint32_t value)
		{
			return int16_t(value & 0x200 ? int32_t(value) - 0x400 : int32_t(value));
		}

		std::vector<uint8_t> info_frame(const std::vector<uint8_t> &payload)
		{
			const uint16_t crc = crc16_bits(payload.data(), uint32_t(payload.size()), 0xffff);

			std::vector<uint8_t> f;

			f.insert(f.end(), 4, 1);

			put_msb(f, SYNC10 & 0xff, 8);

			f.insert(f.end(), payload.begin(), payload.end());

			put_lsb(f, crc, 16);

			f.insert(f.end(), 4, 1);
			f.insert(f.end(), 8, 0);

			return f;
		}

		bool find_info_frame(const uint8_t *bits, uint32_t nbits, uint32_t frame_bits, uint32_t &start)
		{
			uint16_t win = 0;

			for (uint32_t i = 0; i < nbits; i++)
			{
				win = uint16_t(((win << 1) | (bits[i] & 1)) & 0x3ff);

				if (i < 9 || win != SYNC10)
					continue;

				start = i + 1;

				if (start + frame_bits > nbits)
					return false;
				else if (crc16_bits(&bits[start], frame_bits, 0xffff) == 0)
					return true;
			}

			return false;
		}
	}

	uint16_t crc16_bits(const uint8_t *bits, uint32_t nbits, uint16_t crc)
	{
		for (uint32_t i = 0; i < nbits; i++)
		{
			if (((bits[i] ^ crc) & 1))
				crc = uint16_t((crc >> 1) ^ 0x8408);
			else
				crc = uint16_t(crc >> 1);
		}
		return crc;
	}

	static void info0_payload(const info0_t &in, std::vector<uint8_t> &p)
	{
		p.clear();

		put_lsb(p, in.sr_2743 ? 1 : 0, 1);
		put_lsb(p, in.sr_2800 ? 1 : 0, 1);
		put_lsb(p, in.sr_3429 ? 1 : 0, 1);
		put_lsb(p, in.sr_3000_low ? 1 : 0, 1);
		put_lsb(p, in.sr_3000_high ? 1 : 0, 1);
		put_lsb(p, in.sr_3200_low ? 1 : 0, 1);
		put_lsb(p, in.sr_3200_high ? 1 : 0, 1);
		put_lsb(p, in.rate_3429_allowed ? 1 : 0, 1);
		put_lsb(p, in.power_reduction ? 1 : 0, 1);
		put_lsb(p, in.max_baud_diff & 0x7, 3);
		put_lsb(p, in.from_cme_modem ? 1 : 0, 1);
		put_lsb(p, in.support_1664 ? 1 : 0, 1);
		put_lsb(p, in.tx_clock_source & 0x3, 2);
		put_lsb(p, in.ack ? 1 : 0, 1);
	}

	std::vector<uint8_t> build_info0(const info0_t &in)
	{
		std::vector<uint8_t> payload;

		info0_payload(in, payload);

		if (in.v90_digital)
			put_lsb(payload, in.v90_extension & INFO0D_EXTENSION_MASK, INFO0D_EXTENSION_BITS);

		return info_frame(payload);
	}

	bool parse_info0(const uint8_t *bits, uint32_t nbits, info0_t &out, uint32_t *consumed, bool digital_answer)
	{
		const uint32_t target = INFO0_PAYLOAD_BITS + (digital_answer ? INFO0D_EXTENSION_BITS : 0) + 16;

		uint32_t start = 0;
		if (!find_info_frame(bits, nbits, target, start))
			return false;

		uint32_t q = start;

		out.sr_2743 = get_lsb(bits, q, 1) != 0;
		out.sr_2800 = get_lsb(bits, q, 1) != 0;
		out.sr_3429 = get_lsb(bits, q, 1) != 0;
		out.sr_3000_low = get_lsb(bits, q, 1) != 0;
		out.sr_3000_high = get_lsb(bits, q, 1) != 0;
		out.sr_3200_low = get_lsb(bits, q, 1) != 0;
		out.sr_3200_high = get_lsb(bits, q, 1) != 0;
		out.rate_3429_allowed = get_lsb(bits, q, 1) != 0;
		out.power_reduction = get_lsb(bits, q, 1) != 0;
		out.max_baud_diff = uint8_t(get_lsb(bits, q, 3));
		out.from_cme_modem = get_lsb(bits, q, 1) != 0;
		out.support_1664 = get_lsb(bits, q, 1) != 0;
		out.tx_clock_source = uint8_t(get_lsb(bits, q, 2));
		out.ack = get_lsb(bits, q, 1) != 0;
		out.v90_digital = digital_answer;
		out.v90_extension = digital_answer ? uint16_t(get_lsb(bits, q, INFO0D_EXTENSION_BITS)) : 0;

		if (consumed)
			*consumed = start + target;

		return true;
	}

	static void info1a_payload(const info1a_t &in, std::vector<uint8_t> &p)
	{
		p.clear();

		put_lsb(p, in.power_reduction & 0x7, 3);
		put_lsb(p, in.add_power_reduction & 0x7, 3);
		put_lsb(p, in.md & 0x7f, 7);

		if (in.pcm_downstream)
		{
			put_lsb(p, in.training_ucode & 0x7f, 9);
		}
		else
		{
			put_lsb(p, in.use_high_carrier ? 1 : 0, 1);
			put_lsb(p, in.preemphasis_filter & 0xf, 4);
			put_lsb(p, in.max_data_rate & 0xf, 4);
		}

		put_lsb(p, in.baud_rate_a_to_c & 0x7, 3);
		put_lsb(p, in.pcm_downstream ? 6 : (in.baud_rate_c_to_a & 0x7), 3);
		put_lsb(p, uint32_t(in.freq_offset) & 0x3ff, 10);
	}

	std::vector<uint8_t> build_info1a(const info1a_t &in)
	{
		std::vector<uint8_t> payload;

		info1a_payload(in, payload);

		return info_frame(payload);
	}

	bool parse_info1a(const uint8_t *bits, uint32_t nbits, info1a_t &out, uint32_t *consumed)
	{
		const uint32_t target = INFO1A_PAYLOAD_BITS + 16;

		uint32_t start = 0;
		if (!find_info_frame(bits, nbits, target, start))
			return false;

		uint32_t q = start;

		info1a_t v;

		v.power_reduction = uint8_t(get_lsb(bits, q, 3));
		v.add_power_reduction = uint8_t(get_lsb(bits, q, 3));
		v.md = uint8_t(get_lsb(bits, q, 7));
		v.use_high_carrier = get_lsb(bits, q, 1) != 0;
		v.preemphasis_filter = uint8_t(get_lsb(bits, q, 4));
		v.max_data_rate = uint8_t(get_lsb(bits, q, 4));
		v.baud_rate_a_to_c = uint8_t(get_lsb(bits, q, 3));
		v.baud_rate_c_to_a = uint8_t(get_lsb(bits, q, 3));
		v.pcm_downstream = v.baud_rate_c_to_a == 6;

		if (v.pcm_downstream)
		{
			uint32_t training_pos = start + 13;
			v.training_ucode = uint8_t(get_lsb(bits, training_pos, 7));
		}

		v.freq_offset = sign_extend10(get_lsb(bits, q, 10));

		out = v;

		if (consumed)
			*consumed = start + target;

		return true;
	}

	static void info1c_payload(const info1c_t &in, std::vector<uint8_t> &p)
	{
		p.clear();

		put_lsb(p, in.power_reduction & 0x7, 3);
		put_lsb(p, in.add_power_reduction & 0x7, 3);
		put_lsb(p, in.md & 0x7f, 7);
		for (int32_t r = 0; r < 6; r++)
		{
			put_lsb(p, in.rate[r].use_high_carrier ? 1 : 0, 1);
			put_lsb(p, in.rate[r].pre_emphasis & 0xf, 4);
			put_lsb(p, in.rate[r].max_bit_rate & 0xf, 4);
		}
		put_lsb(p, uint32_t(in.freq_offset) & 0x3ff, 10);
	}

	std::vector<uint8_t> build_info1c(const info1c_t &in)
	{
		std::vector<uint8_t> payload;

		info1c_payload(in, payload);

		return info_frame(payload);
	}

	static info1c_t read_info1c(const uint8_t *bits, uint32_t q)
	{
		info1c_t v;

		v.power_reduction = uint8_t(get_lsb(bits, q, 3));
		v.add_power_reduction = uint8_t(get_lsb(bits, q, 3));
		v.md = uint8_t(get_lsb(bits, q, 7));

		for (int32_t r = 0; r < 6; r++)
		{
			v.rate[r].use_high_carrier = get_lsb(bits, q, 1) != 0;
			v.rate[r].pre_emphasis = uint8_t(get_lsb(bits, q, 4));
			v.rate[r].max_bit_rate = uint8_t(get_lsb(bits, q, 4));
		}

		v.freq_offset = sign_extend10(get_lsb(bits, q, 10));

		return v;
	}

	bool parse_info1c(const uint8_t *bits, uint32_t nbits, info1c_t &out, uint32_t *consumed)
	{
		const uint32_t target = INFO1C_PAYLOAD_BITS + 16;

		uint32_t start = 0;
		if (!find_info_frame(bits, nbits, target, start))
			return false;

		out = read_info1c(bits, start);

		if (consumed)
			*consumed = start + target;

		return true;
	}

	bool parse_info1c_erasure(const uint8_t *bits, const uint8_t *reliable, uint32_t nbits, info1c_t &out, uint32_t *consumed, uint32_t max_flip_bits)
	{
		const uint32_t frame_bits = 10 + INFO1C_PAYLOAD_BITS + 16;
		if (frame_bits > nbits)
			return false;

		uint8_t local[10 + INFO1C_PAYLOAD_BITS + 16];
		uint32_t unreliable_off[64];

		for (uint32_t start = 0; start + frame_bits <= nbits; start++)
		{
			uint32_t m = 0;
			for (uint32_t k = 0; k < frame_bits; k++)
			{
				local[k] = bits[start + k];

				if (!reliable[start + k])
				{
					if (m >= max_flip_bits)
					{
						m = UINT32_MAX;
						break;
					}

					unreliable_off[m++] = k;
				}
			}

			if (m == UINT32_MAX || m > max_flip_bits)
				continue;

			const uint32_t combos = 1U << m;
			for (uint32_t mask = 0; mask < combos; mask++)
			{
				for (uint32_t j = 0; j < m; j++)
					local[unreliable_off[j]] = bits[start + unreliable_off[j]] ^ ((mask >> j) & 1);

				uint16_t win = 0;
				for (int32_t k = 0; k < 10; k++)
					win = uint16_t(((win << 1) | local[k]) & 0x3ff);

				if (win != SYNC10)
					continue;
				else if (crc16_bits(local + 10, INFO1C_PAYLOAD_BITS + 16, 0xffff) != 0)
					continue;

				out = read_info1c(local, 10);

				if (consumed)
					*consumed = start + frame_bits;

				return true;
			}
		}

		return false;
	}

	precoder_t mp_precoder(const mp_t &mp)
	{
		precoder_t taps {};

		if (mp.type != 1 || !mp.precoding_enabled)
			return taps;

		auto word = [&](int32_t group)
		{
			uint16_t value = 0;
			for (int32_t bit = 0; bit < 16; bit++)
				value |= uint16_t(mp.long_ext[group * 16 + bit] & 1) << bit;

			return int16_t(value);
		};

		taps[0] = { word(3), word(4) };
		taps[1] = { word(1), word(2) };
		taps[2] = { mp.precoder_c3_real, word(0) };

		return taps;
	}

	const std::array<uint8_t, 96> &mp_long_ext_template()
	{

		static const std::array<uint8_t, 96> t = []
		{
			static const char *bits = "110100111111111111111100000000001110111111111111110111011111111111000000000000000000000000000000";

			std::array<uint8_t, 96> a {};

			for (int32_t i = 0; i < 96; i++)
				a[i] = uint8_t(bits[i] - '0');

			return a;
		}();

		return t;
	}

	const std::array<uint8_t, 96> &mp_long_ext_identity()
	{

		static const std::array<uint8_t, 96> t {};

		return t;
	}

	static uint16_t mp_crc(const uint8_t *frame, int32_t n_groups)
	{
		uint16_t crc = 0xffff;
		int32_t off = MP_SYNC_BITS;
		for (int32_t g = 0; g < n_groups; g++)
		{
			crc = crc16_bits(frame + off, MP_GROUP_DATA_BITS, crc);
			off += MP_GROUP_BITS;
		}

		return crc;
	}

	std::vector<uint8_t> build_mp(const mp_t &in)
	{
		const bool long_frame = (in.type & 1) != 0;
		const int32_t n_groups = long_frame ? MP1_GROUPS : MP0_GROUPS;

		std::vector<uint8_t> f;
		f.reserve(long_frame ? MP1_LEN : MP0_LEN);

		put_lsb(f, (1U << (MP_SYNC_BITS - 1)) - 1, MP_SYNC_BITS);
		put_lsb(f, in.precoding_enabled ? 1 : 0, 1);
		put_lsb(f, 0, 1);
		put_lsb(f, in.bit_rate_c_to_a & 0xf, 4);
		put_lsb(f, in.bit_rate_a_to_c & 0xf, 4);
		put_lsb(f, in.aux_channel ? 1 : 0, 1);
		put_lsb(f, in.trellis_sel & 3, 2);
		put_lsb(f, in.nonlinear_encoder ? 1 : 0, 1);
		put_lsb(f, in.expanded_shaping ? 1 : 0, 1);
		put_lsb(f, in.mp_acknowledged ? 1 : 0, 1);
		put_lsb(f, 0, 1);
		put_lsb(f, in.signalling_rate_mask & 0x7fff, 15);
		put_lsb(f, in.asymmetric_rates ? 1 : 0, 1);
		put_lsb(f, 0, 1);
		put_lsb(f, long_frame ? uint16_t(in.precoder_c3_real) : 0, 16);

		if (long_frame)
		{
			for (int32_t g = 0; g < 6; g++)
			{
				put_lsb(f, 0, 1);
				for (int32_t b = 0; b < MP_GROUP_DATA_BITS; b++)
					f.push_back(in.long_ext[g * MP_GROUP_DATA_BITS + b]);
			}
		}

		put_lsb(f, 0, 1);

		uint16_t crc = mp_crc(f.data(), n_groups);
		put_lsb(f, crc, MP_CRC_BITS);
		put_lsb(f, 0, 3);

		return f;
	}

	bool parse_mp(const uint8_t *bits, uint32_t nbits, mp_t &out, uint32_t *consumed, uint32_t *resume)
	{
		uint32_t settled = resume ? *resume : 0;
		bool pending = false;

		for (uint32_t i = settled; i + MP0_LEN <= nbits; i++)
		{
			bool sync = true;
			for (int32_t k = 0; k < MP_SYNC_BITS - 1 && sync; k++)
				sync = bits[i + k] & 1;

			if (!sync || (bits[i + MP_SYNC_BITS - 1] & 1))
			{
				if (!pending)
					settled = i + 1;

				continue;
			}

			auto try_len = [&](int32_t n_groups) -> bool
			{
				const int32_t crc_off = MP_SYNC_BITS + n_groups * MP_GROUP_BITS;
				const int32_t core_length = crc_off + MP_CRC_BITS;
				if (i + uint32_t(core_length) > nbits)
				{
					pending = true;
					return false;
				}

				const uint8_t *f = bits + i;

				uint32_t pos = uint32_t(crc_off);
				if (mp_crc(f, n_groups) != get_lsb(f, pos, MP_CRC_BITS))
					return false;

				mp_t m;

				pos = MP_SYNC_BITS;

				m.type = uint8_t(n_groups == MP1_GROUPS ? 1 : 0);
				m.precoding_enabled = get_lsb(f, pos, 1) != 0;
				pos += 1;
				m.bit_rate_c_to_a = uint8_t(get_lsb(f, pos, 4));
				m.bit_rate_a_to_c = uint8_t(get_lsb(f, pos, 4));
				m.aux_channel = get_lsb(f, pos, 1) != 0;
				m.trellis_sel = uint8_t(get_lsb(f, pos, 2));
				m.nonlinear_encoder = get_lsb(f, pos, 1) != 0;
				m.expanded_shaping = get_lsb(f, pos, 1) != 0;
				m.mp_acknowledged = get_lsb(f, pos, 1) != 0;
				pos += 1;
				m.signalling_rate_mask = uint16_t(get_lsb(f, pos, 15));
				m.asymmetric_rates = get_lsb(f, pos, 1) != 0;
				pos += 1;

				if (n_groups == MP1_GROUPS)
				{
					m.precoder_c3_real = int16_t(get_lsb(f, pos, 16));

					for (int32_t g = 0; g < 6; g++)
					{
						pos += 1;
						for (int32_t b = 0; b < MP_GROUP_DATA_BITS; b++)
							m.long_ext[g * MP_GROUP_DATA_BITS + b] = f[pos++] & 1;
					}
				}

				out = m;

				if (consumed)
					*consumed = i + uint32_t(core_length);

				return true;
			};

			if (try_len(MP0_GROUPS))
				return true;
			if (try_len(MP1_GROUPS))
				return true;

			if (!pending)
				settled = i + 1;
		}

		if (resume)
			*resume = settled;

		return false;
	}

	void info1c_settle::reset()
	{
		m_clean_now = false;
		m_clean_since_sample = 0;
	}

	bool info1c_settle::update(const std::vector<uint8_t> &reliable, uint64_t now, bool probe_ended, uint64_t probe_end_sample, wtvsoftmodem_dsp::frequency_t sample_rate)
	{
		size_t rel_window = std::min<size_t>(RELIABLE_WINDOW, reliable.size());
		double rel_frac = 1.0;

		if (rel_window > 0)
		{
			uint32_t rel_sum = 0;
			for (size_t i = reliable.size() - rel_window; i < reliable.size(); i++)
				rel_sum += reliable[i];

			rel_frac = double(rel_sum) / double(rel_window);
		}

		const bool signal_clean_now = rel_window >= RELIABLE_WINDOW && rel_frac >= RELIABLE_FRAC;

		if (signal_clean_now && !m_clean_now)
		{
			m_clean_now = true;
			m_clean_since_sample = now;
		}
		else if (!signal_clean_now && m_clean_now)
		{
			m_clean_now = false;
		}

		const bool sustained_clean = m_clean_now && (now - m_clean_since_sample) >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(CLEAN_SUSTAIN_MS, sample_rate);
		const bool max_elapsed = (now - probe_end_sample) >= wtvsoftmodem_dsp::ms_to_samples<uint64_t>(SETTLE_MAX_MS, sample_rate);

		return probe_ended && (sustained_clean || max_elapsed);
	}

}
