// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_pcm.h"
#include "wtvsoftmodem_dsp.h"
#include "wtvsoftmodem_v34info.h"

#include <algorithm>
#include <limits>

// ITU-T V.90 5 Digital modem, 8.4 and 8.6 Digital modem start-up signals

wtvsoftmodem_pcm::wtvsoftmodem_pcm()
{
	//
}

void wtvsoftmodem_pcm::start(wtvsoftmodem_dsp::frequency_t sample_rate, uint8_t training_ucode)
{
	m_sample_rate = sample_rate;
	m_symbol = 0;
	m_stage_symbol = 0;
	m_upstream_rate_cap = 14;
	m_scrambler = 0;
	m_ja.clear();
	m_parameter.clear();
	m_cp.clear();
	m_data_cp.clear();
	m_parameter_index = 0;
	m_mp_ack = false;
	m_sent_mp_ack = false;
	m_peer_cp_ack = false;
	m_data_sign = 0;
	m_upstream_s = false;
	m_dil_segment = 0;
	m_upstream_reversals = 0;
	m_dil_complete = false;
	m_mapper_ready = false;
	m_frame_index = 6;
	m_bits = 0;
	m_bit_index = 8;
	m_clock  = sample_rate;
	m_history.assign(m_filter.size());
	m_training_ucode = training_ucode;
	m_level = pcm_level(training_ucode);
	set_state(PCMState::WAIT_JA);

	init_reconstruction(sample_rate);
}

void wtvsoftmodem_pcm::init_reconstruction(wtvsoftmodem_dsp::frequency_t sample_rate)
{
	m_sample_rate = sample_rate;
	m_history.assign(m_filter.size());

	wtvsoftmodem_dsp::design_lowpass(m_filter.data(), m_filter.size(), 4000.0 / sample_rate);

	for (auto &tap : m_filter)
		tap = tap * sample_rate / 8000.0;
}

inline int16_t wtvsoftmodem_pcm::ucode_level(uint8_t ucode, bool alaw)
{
	const uint32_t u = ucode & 127;

	if (alaw)
		return int16_t((u >> 4) ? ((u & 15) * 16 + 264) << ((u >> 4) - 1) : (u & 15) * 16 + 8);
	else
		return int16_t((((u & 15) * 2 + 33) << (u >> 4)) * 4 - 132);
}

bool wtvsoftmodem_pcm::mapper::configure(const std::vector<uint8_t> &cp)
{
	if (cp.size() < 30 || (cp[2] & 1))
		return false;

	uint64_t product = 1;
	for (uint32_t i = 0; i < 6; i++)
	{
		const uint32_t index = (cp[10 + i / 2] >> (4 * (i & 1))) & 15;

		if (index > 5 || cp.size() < 14 + 16 * (index + 1))
			return false;

		m_codes[i].clear();

		for (uint32_t u = 0; u < 128; u++)
		{
			if ((cp[14 + 16 * index + u / 8] >> (u % 8)) & 1)
				m_codes[i].push_back(uint8_t(u));
		}

		if (m_codes[i].empty())
			return false;

		product *= m_codes[i].size();
	}

	m_s = 6 - ((cp[1] >> 5) & 3);

	const uint32_t drn = (cp[0] >> 2) & 31;
	if (!drn || drn > 22)
		return false;

	const uint32_t frame_bits = drn + ((cp[0] & 2) ? 20 : 8);

	m_k = frame_bits - m_s;
	if ((uint64_t(1) << m_k) > product)
		return false;

	for (uint32_t i = 0; i < 4; i++)
		m_filter[i] = double(int8_t(cp[6 + i])) / 64;

	reset();

	return true;
}

void wtvsoftmodem_pcm::mapper::reset()
{
	m_sign = {};
	m_x1 = 0;
	m_x2 = 0;
	m_y1 = 0;
	m_y2 = 0;
}

uint32_t wtvsoftmodem_pcm::mapper::recover_signs(uint32_t word, uint32_t s, sign_state &st)
{
	constexpr uint32_t MASKS[4] = { 0, 63, 42, 21 };
	constexpr uint32_t TRANSITIONS[4] = { 0, 2, 2, 0 };

	uint32_t result = 0;

	if (s == 6)
	{
		result  = word ^ ((st.word >> 5) | ((word & 31) << 1));

		st.word = word;
	}
	else if (s == 5)
	{
		const uint32_t index   = (word & 1) | st.trellis;
		const uint32_t changed = word ^ MASKS[index];
		const uint32_t d = changed ^ st.changed;

		result = (d ^ ((d << 2) & 32) ^ ((d << 2) & 8) ^ ((st.difference >> 4) & 2)) >> 1;

		st.trellis = TRANSITIONS[index];
		st.changed = changed;
		st.difference = d;
		st.word = word;
	}
	else
	{
		const uint32_t width = s == 4 ? 3 : 2;
		const uint32_t groups = 6 / width;

		for (uint32_t i = 0; i < groups; i++)
		{
			const uint32_t chunk = (word >> (width * i)) & ((1U << width) - 1);
			const uint32_t index = (chunk & 1) | st.trellis;
			const uint32_t changed = (chunk ^ MASKS[index]) & ((1U << width) - 1);
			const uint32_t d = changed ^ st.changed;
			const uint32_t recovered = s == 4 ? (d ^ (st.difference & 2)) : ((d ^ st.difference) & 2);

			result |= (recovered >> 1) << ((width - 1) * i);

			st.trellis = TRANSITIONS[index];
			st.changed = changed;
			st.difference = d;
			st.word = chunk;
		}
	}

	return result & ((1U << s) - 1);
}

std::array<int16_t, 6> wtvsoftmodem_pcm::mapper::encode(uint64_t data)
{
	const uint32_t signs = uint32_t(data) & ((1U << m_s) - 1);
	uint64_t rank  = data >> m_s;

	std::array<int16_t, 6> magnitude {};
	for (uint32_t i = 0; i < 6; i++)
	{
		const uint32_t radix = m_codes[i].size();
		const uint32_t label = radix - 1 - uint32_t(rank % radix);

		rank /= radix;
		magnitude[i] = ucode_level(m_codes[i][label], m_alaw);
	}

	if (m_s == 6)
	{
		uint32_t previous = (m_sign.word >> 5) & 1;
		uint32_t word = 0;
		std::array<int16_t, 6> output {};

		for (uint32_t i = 0; i < 6; i++)
		{
			previous ^= (signs >> i) & 1;
			word |= previous << i;
			output[i] = previous ? -magnitude[i] : magnitude[i];

			const double x = output[i];
			const double y = x + m_filter[2] * m_x1 + m_filter[3] * m_x2 - m_filter[0] * m_y1 - m_filter[1] * m_y2;

			m_x2 = m_x1;
			m_x1 = x;
			m_y2 = m_y1;
			m_y1 = y;
		}

		m_sign.word = word;

		return output;
	}
	else
	{
		double best = std::numeric_limits<double>::infinity();

		std::array<int16_t, 6> output {};

		sign_state chosen {};
		double chosen_x1 = 0;
		double chosen_x2 = 0;
		double chosen_y1 = 0;
		double chosen_y2 = 0;

		for (uint32_t word = 0; word < 64; word++)
		{
			sign_state trial = m_sign;

			if (recover_signs(word, m_s, trial) != signs)
				continue;

			double cost = 0;
			double x1 = m_x1;
			double x2 = m_x2;
			double y1 = m_y1;
			double y2 = m_y2;

			std::array<int16_t, 6> candidate {};

			for (uint32_t i = 0; i < 6; i++)
			{
				candidate[i] = (word & (1U << i)) ? -magnitude[i] : magnitude[i];

				const double x = candidate[i];
				const double y = x + m_filter[2] * x1 + m_filter[3] * x2 - m_filter[0] * y1 - m_filter[1] * y2;

				cost += y * y;
				x2 = x1;
				x1 = x;
				y2 = y1;
				y1 = y;
			}

			if (cost < best)
			{
				best = cost;
				output = candidate;
				chosen = trial;
				chosen_x1 = x1;
				chosen_x2 = x2;
				chosen_y1 = y1;
				chosen_y2 = y2;
			}
		}

		m_sign = chosen;
		m_x1 = chosen_x1;
		m_x2 = chosen_x2;
		m_y1  = chosen_y1;
		m_y2 = chosen_y2;

		return output;
	}
}

inline int16_t wtvsoftmodem_pcm::mulaw_decode(uint8_t octet)
{
	const uint8_t u = uint8_t(~octet);
	const int16_t magnitude = ucode_level(u);

	return (u & 128) ? -magnitude : magnitude;
}

inline uint8_t wtvsoftmodem_pcm::mulaw_encode(int16_t sample)
{
	int32_t magnitude = sample < 0 ? -int32_t(sample) : int32_t(sample);
	const uint32_t sign = sample < 0 ? 128 : 0;

	magnitude = std::min(magnitude, 32635) + 132;

	uint32_t exponent = 7;
	for (uint32_t mask = 0x4000; exponent && !(magnitude & mask); mask >>= 1)
		exponent--;

	const uint32_t mantissa = (magnitude >> (exponent + 3)) & 15;

	return uint8_t(~(sign | (exponent << 4) | mantissa));
}

inline int16_t wtvsoftmodem_pcm::alaw_decode(uint8_t octet)
{
	const uint8_t a = uint8_t(octet ^ 0x55);
	const int16_t magnitude = ucode_level(a, true);

	return (a & 128) ? magnitude : -magnitude;
}

inline uint8_t wtvsoftmodem_pcm::alaw_encode(int16_t sample)
{
	const uint32_t sign = sample >= 0 ? 128 : 0;
	const uint32_t magnitude = uint32_t(sample >= 0 ? sample : ~sample) >> 3;

	uint32_t exponent  = 0;
	while (exponent < 8 && magnitude >= (0x20U << exponent))
		exponent++;

	if (exponent == 8)
	{
		return uint8_t((sign | 127) ^ 0x55);
	}
	else
	{
		const uint32_t mantissa = (magnitude >> (exponent ? exponent : 1)) & 15;

		return uint8_t((sign | (exponent << 4) | mantissa) ^ 0x55);
	}
}

inline uint8_t wtvsoftmodem_pcm::scramble_byte(uint8_t input, uint32_t &state)
{
	const uint32_t bits = (state >> 14) ^ input ^ (state >> 9);

	state = (state >> 8) | (bits << 24);

	return uint8_t(bits);
}

template <typename Length>
bool wtvsoftmodem_pcm::parse_parameter_frame(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, size_t *consumed, uint32_t *resume, Length length_of)
{
	size_t settled = resume ? *resume : 0;
	bool pending = false;

	uint32_t ones = 0;
	for (size_t sync = settled; sync < bits.size(); sync++)
	{
		if (bits[sync])
		{
			ones++;
			continue;
		}

		const bool header = ones >= 17;
		ones = 0;

		bool rejected = !header;

		if (header)
		{
			std::vector<uint8_t> bytes;
			size_t length = 0;
			uint16_t crc = 0xffff;
			for (size_t p = sync + 1; p + 17 <= bits.size() && !rejected; p += 17)
			{
				if (bits[p + 16])
				{
					rejected = true;
					break;
				}

				uint16_t word = 0;
				for (uint32_t bit = 0; bit < 16; bit++)
					word |= uint16_t(bits[p + bit]) << bit;

				crc = wtvsoftmodem::v34::info::crc16_bits(bits.data() + p, 16, crc);

				bytes.push_back(uint8_t(word));
				bytes.push_back(uint8_t(word >> 8));

				if (!length)
				{
					length = length_of(bytes);
					if (length == SIZE_MAX)
					{
						rejected = true;
						break;
					}
				}

				if (length && bytes.size() == length + 2)
				{
					if (crc == 0)
					{
						bytes.resize(length);

						payload = std::move(bytes);

						if (consumed)
							*consumed = p + 17;

						return true;
					}

					rejected = true;
				}
			}
		}

		// A frame that ran out of bits may still complete.
		pending |= !rejected;
		if (!pending)
			settled = sync + 1;
	}

	if (resume)
		*resume = uint32_t(settled);

	return false;
}



int32_t wtvsoftmodem_pcm::reconstruct(double value)
{
	m_history.push(value);

	const double filtered = m_history.accumulate(
		0.0,
		m_filter.size(),
		[this](double acc, size_t k, double x)
		{
			return acc + m_filter[k] * x;
		}
	);

	return wtvsoftmodem_dsp::round_nearest<int32_t>(filtered);
}

void wtvsoftmodem_pcm::begin_pcm_probe(wtvsoftmodem_dsp::frequency_t sample_rate)
{
	init_reconstruction(sample_rate);
	m_clock = sample_rate;
}

void wtvsoftmodem_pcm::quantize_probe(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double value = 0;
		if (m_clock >= m_sample_rate)
		{
			m_clock -= m_sample_rate;
			value = m_alaw ? alaw_decode(alaw_encode(sample[sample_idx])) : mulaw_decode(mulaw_encode(sample[sample_idx]));
		}
		m_clock += 8000.0;

		sample[sample_idx] = wtvsoftmodem_dsp::saturate(reconstruct(value));
	}
}

void wtvsoftmodem_pcm::set_state(PCMState next)
{
	const PCMState prev = m_state;
	m_state = next;

	if (prev != next && m_state_change_cb)
		m_state_change_cb(prev, next);
}

void wtvsoftmodem_pcm::enter_state(PCMState next)
{
	set_state(next);
	m_stage_symbol = 0;
}

inline void wtvsoftmodem_pcm::enter_trn2d()
{
	enter_state(PCMState::TRN2D);

	m_frame_index = 6;
	m_scrambler = 0;
	m_mapper.reset();
}

inline bool wtvsoftmodem_pcm::advance_state()
{
	switch (m_state)
	{
		case PCMState::S:
		{
			if (m_stage_symbol == SYM_S)
				enter_state(PCMState::SBAR);
			break;
		}

		case PCMState::SBAR:
		{
			if (m_stage_symbol == SYM_SBAR)
			{
				enter_state(PCMState::TRN1D);
				m_scrambler = 0;
				m_bit_index = 8;
			}
			break;
		}

		case PCMState::TRN1D:
		{
			if (m_stage_symbol == SYM_TRN1D)
			{
				m_parameter = build_parameter_frame({ 0xff, 0xff, 0x3f, 0x60 });
				m_parameter.insert(m_parameter.end(), 3, 0);

				m_data_sign = (m_bits >> 7) & 1;

				enter_state(PCMState::JD);
			}
			break;
		}

		case PCMState::JD:
		{
			if (m_stage_symbol == m_parameter.size())
				enter_state(m_upstream_s ? PCMState::JDPRIME : PCMState::JD);
			break;
		}

		case PCMState::JDPRIME:
		{
			if (m_stage_symbol == SYM_JDPRIME)
			{
				enter_state(PCMState::DIL);
				m_dil_segment = 0;
			}
			break;
		}

		case PCMState::RD:
		{
			if (m_stage_symbol == SYM_RD)
				enter_state(PCMState::RDBAR);
			break;
		}

		case PCMState::RDBAR:
		{
			if (m_stage_symbol == SYM_RDBAR)
			{
				if (!m_mapper.configure(m_cp))
					return false;

				enter_trn2d();
			}
			break;
		}

		case PCMState::RI:
		{
			if (m_mapper_ready && m_stage_symbol >= SYM_RI_MIN && (m_stage_symbol % 6) == 0)
				enter_state(PCMState::RIBAR);
			break;
		}

		case PCMState::RIBAR:
		{
			if (m_stage_symbol == SYM_RIBAR)
				enter_trn2d();
			break;
		}

		case PCMState::TRN2D:
		{
			if (m_stage_symbol == SYM_TRN2D)
			{
				enter_state(PCMState::MP);
				prepare_mp(!m_data_cp.empty());
			}
			break;
		}

		case PCMState::ED:
		{
			if (m_stage_symbol == SYM_ED)
			{
				if (!m_mapper.configure(m_data_cp))
					return false;

				enter_state(PCMState::B1);

				m_frame_index = 6;
				m_scrambler   = 0;
			}
			break;
		}

		case PCMState::B1:
		{
			if (m_stage_symbol == SYM_B1)
			{
				enter_state(PCMState::DATA);

				if (m_data_callback)
					m_data_callback();
			}
			break;
		}

		default:
			break;
	}

	return true;
}

int16_t wtvsoftmodem_pcm::next_symbol()
{
	if (!advance_state())
	{
		set_state(PCMState::FAILED);

		return 0;
	}
	else
	{
		int16_t value = 0;
		switch (m_state)
		{
			case PCMState::S:
			case PCMState::SBAR:
			{
				static constexpr int32_t PATTERN[6] = { 1, 0, 1, -1, 0, -1 };
				const int16_t level = pcm_level(uint8_t(std::min(127, int32_t(m_training_ucode) + 16)));

				value = int16_t(PATTERN[m_stage_symbol % 6] * level * (m_state == PCMState::SBAR ? -1 : 1));
				break;
			}

			case PCMState::TRN1D:
				if (m_bit_index == 8)
				{
					m_bits      = scramble_byte(255, m_scrambler);
					m_bit_index = 0;
				}

				value = ((m_bits >> m_bit_index++) & 1) ? m_level : -m_level;
				break;

			case PCMState::JD:
			case PCMState::JDPRIME:
			{
				const uint32_t input = m_state == PCMState::JD ? m_parameter[size_t(m_stage_symbol)] : 0;
				const uint32_t bit   = ((m_scrambler >> 14) ^ (m_scrambler >> 9) ^ input) & 1;

				m_scrambler          = (m_scrambler >> 1) | (bit << 31);
				m_data_sign ^= bit;

				value = m_data_sign ? m_level : -m_level;
				break;
			}

			case PCMState::DIL:
			{
				value = next_dil_symbol();
				break;
			}

			case PCMState::RD:
			case PCMState::RDBAR:
			{
				const uint32_t index = uint32_t(m_stage_symbol % 6);
				const int32_t  sign  = index < 3 ? 1 : -1;

				value = int16_t(sign * m_reneg_levels[index] * (m_state == PCMState::RDBAR ? -1 : 1));
				break;
			}

			case PCMState::RI:
			case PCMState::RIBAR:
			{
				const int32_t sign = (m_stage_symbol % 6) < 3 ? 1 : -1;

				value = int16_t(sign * m_level * (m_state == PCMState::RIBAR ? -1 : 1));
				break;
			}

			case PCMState::TRN2D:
			case PCMState::MP:
			case PCMState::ED:
			case PCMState::B1:
			case PCMState::DATA:
			case PCMState::RENEG_WAIT:
			{
				value = next_mapped_symbol();
				break;
			}

			default:
				break;
		}

		m_symbol++;
		m_stage_symbol++;

		return value;
	}
}

uint32_t wtvsoftmodem_pcm::tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	if (m_state == PCMState::IDLE || m_state == PCMState::FAILED)
	{
		wtvsoftmodem_dsp::clear_samples(sample, sample_count);
		return sample_count;
	}

	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double value = 0;

		if (m_clock >= m_sample_rate)
		{
			m_clock -= m_sample_rate;
			value = next_symbol();
		}

		m_clock += 8000.0;

		sample[sample_idx] = wtvsoftmodem_dsp::saturate(reconstruct(value));
	}

	return sample_count;
}

std::vector<uint8_t> wtvsoftmodem_pcm::build_parameter_frame(const std::vector<uint8_t> &payload)
{
	if (payload.empty() || (payload.size() & 1))
		return {};

	std::vector<uint8_t> bytes = payload;

	uint16_t crc = 0xffff;
	for (uint8_t byte : payload)
	{
		for (uint32_t bit = 0; bit < 8; bit++)
		{
			const uint8_t input = (byte >> bit) & 1;

			crc = wtvsoftmodem::v34::info::crc16_bits(&input, 1, crc);
		}
	}

	bytes.push_back(uint8_t(crc));
	bytes.push_back(uint8_t(crc >> 8));

	std::vector<uint8_t> frame;

	frame.reserve(18 + 17 * (bytes.size() / 2));
	frame.assign(17, 1);
	frame.push_back(0);

	for (size_t i = 0; i < bytes.size(); i += 2)
	{
		const uint16_t word = bytes[i] | (uint16_t(bytes[i + 1]) << 8);

		for (uint32_t bit = 0; bit < 16; bit++)
			frame.push_back((word >> bit) & 1);

		frame.push_back(0);
	}

	return frame;
}

bool wtvsoftmodem_pcm::parse_ja(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, uint32_t *resume)
{
	return parse_parameter_frame(
		bits,
		payload,
		nullptr,
		resume,
		[](const std::vector<uint8_t> &bytes) -> size_t
		{
			if (bytes.size() != 4)
				return 0;

			if (bytes[2] > 127 || bytes[3] > 127)
				return SIZE_MAX;

			return 4 + 2 * ((uint32_t(bytes[2]) + 16) / 16) + 2 * ((uint32_t(bytes[3]) + 16) / 16) + 16 + ((uint32_t(bytes[0]) + 1) & ~1U);
		}
);
}

void wtvsoftmodem_pcm::accept_ja(const std::vector<uint8_t> &payload)
{
	m_ja = payload;

	if (m_state == PCMState::WAIT_JA)
	{
		enter_state(PCMState::S);
	}
}

bool wtvsoftmodem_pcm::request_retrain()
{
	if (m_state != PCMState::DATA)
	{
		return false;
	}
	else
	{
		observe_upstream_s(false);

		m_upstream_s = true;

		return true;
	}
}

void wtvsoftmodem_pcm::observe_upstream_s(bool reversal)
{
	if (m_state == PCMState::DATA && !reversal)
	{
		for (uint32_t i = 0; i < 6; i++)
			m_reneg_levels[i] = pcm_level(m_mapper.constellations()[i].back());

		set_state(PCMState::RENEG_WAIT);
		m_upstream_s = false;
		m_data_cp.clear();
		m_mp_ack = false;
		m_sent_mp_ack = false;
		m_peer_cp_ack = false;
	}

	if (m_state == PCMState::RENEG_WAIT && reversal)
		m_upstream_s = true;

	if (m_state == PCMState::JD && !m_upstream_s)
		m_upstream_s = true;

	if (reversal && (m_state == PCMState::JDPRIME || m_state == PCMState::DIL))
	{
		m_upstream_reversals++;
		if (m_upstream_reversals >= 2)
			m_dil_complete = true;
	}
}

int16_t wtvsoftmodem_pcm::next_dil_symbol()
{
	if (m_ja.size() < 4 || m_ja[0] == 0)
		return 0;

	const uint32_t sign_length = uint32_t(m_ja[2]) + 1;
	const uint32_t train_length = uint32_t(m_ja[3]) + 1;
	const uint32_t sign_bytes = 2 * ((sign_length + 15) / 16);
	const uint32_t train_bytes = 2 * ((train_length + 15) / 16);
	const uint32_t h = 4 + sign_bytes + train_bytes;

	const uint32_t codes = h + 16;
	if (codes + m_ja[0] > m_ja.size())
		return 0;

	const uint8_t code = m_ja[codes + m_dil_segment] & 127;
	const uint32_t chord = code / 16;

	const uint64_t segment_length = (uint64_t(m_ja[h + chord] & 127) + 1) * 6;
	if (m_stage_symbol == segment_length)
	{
		m_stage_symbol = 0;

		if (m_dil_complete)
		{
			set_state(PCMState::RI);

			return m_level;
		}
		else
		{
			m_dil_segment = (m_dil_segment + 1) % m_ja[0];

			return next_dil_symbol();
		}
	}

	const uint32_t sp = uint32_t(m_stage_symbol % sign_length);
	const uint32_t tp = uint32_t(m_stage_symbol % train_length);
	const bool positive = (m_ja[4 + sp / 8] >> (sp % 8)) & 1;
	const bool training = (m_ja[4 + sign_bytes + tp / 8] >> (tp % 8)) & 1;

	const int16_t level = pcm_level(training ? code : (m_ja[h + 8 + chord] & 127));

	return positive ? level : -level;
}

bool wtvsoftmodem_pcm::parse_cp(const std::vector<uint8_t> &bits, std::vector<uint8_t> &payload, size_t *consumed, uint32_t *resume)
{
	return parse_parameter_frame(
		bits,
		payload,
		consumed,
		resume,
		[](const std::vector<uint8_t> &bytes) -> size_t
		{
			if (bytes.size() != 14)
				return 0;

			uint32_t maximum = 0;
			for (uint32_t i = 10; i < 13; i++)
				maximum = std::max(maximum, std::max(uint32_t(bytes[i] & 15), uint32_t(bytes[i] >> 4)));

			if (maximum > 5)
				return SIZE_MAX;

			return 14 + 16 * (maximum + 1) * ((bytes[13] & 1) ? 2 : 1);
		}
	);
}

void wtvsoftmodem_pcm::accept_cp(const std::vector<uint8_t> &payload)
{
	if (payload.size() < 14)
		return;
	if (payload[0] & 2)
	{
		m_peer_cp_ack = (payload[1] & 128) != 0;
		if (payload == m_data_cp)
			return;
		m_data_cp = payload;
	}
	else
	{
		if (payload == m_cp)
			return;

		m_cp = payload;

		m_mapper_ready = m_mapper.configure(payload);
		if (m_mapper_ready && m_state == PCMState::DIL)
			m_dil_complete = true;
	}
}

void wtvsoftmodem_pcm::prepare_mp(bool acknowledge)
{
	const uint32_t mask = (1U << m_upstream_rate_cap) - 2;

	m_parameter = build_parameter_frame({
		uint8_t((m_upstream_rate_cap & 3) << 6),
		uint8_t((m_upstream_rate_cap >> 2) | (acknowledge ? 128 : 0)),
		uint8_t(mask),
		uint8_t(mask >> 8),
		0,
		0
	});

	while (m_parameter.size() % m_mapper.frame_bits())
		m_parameter.push_back(0);

	m_parameter_index = 0;
	m_mp_ack = acknowledge;
}

int16_t wtvsoftmodem_pcm::next_mapped_symbol()
{
	if (m_frame_index == 6)
	{
		if (m_state == PCMState::RENEG_WAIT && m_upstream_s)
		{
			m_upstream_s = false;
			enter_state(PCMState::RD);

			return m_reneg_levels[0];
		}
		else
		{
			if (m_state == PCMState::MP && m_parameter_index == m_parameter.size())
			{
				if (m_mp_ack)
					m_sent_mp_ack = true;

				if (m_sent_mp_ack && m_peer_cp_ack)
				{
					enter_state(PCMState::ED);
				}
				else
				{
					prepare_mp(!m_data_cp.empty());
				}
			}

			uint64_t data = 0;
			for (uint32_t i = 0; i < m_mapper.frame_bits(); i++)
			{
				uint32_t input = 1;

				if (m_state == PCMState::MP)
					input = m_parameter[m_parameter_index++];
				else if (m_state == PCMState::ED)
					input = 0;
				else if (m_state == PCMState::DATA && m_bit_source)
					input = m_bit_source() ? 1 : 0;

				const uint32_t bit = ((m_scrambler >> 14) ^ (m_scrambler >> 9) ^ input) & 1;
				m_scrambler = (m_scrambler >> 1) | (bit << 31);

				data |= uint64_t(bit) << i;
			}

			m_frame = m_mapper.encode(data);
			m_frame_index = 0;
		}
	}

	return m_frame[m_frame_index++];
}
