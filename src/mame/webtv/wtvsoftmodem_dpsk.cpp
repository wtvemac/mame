// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_dpsk.h"

#include <cmath>

wtvsoftmodem_dpsk::wtvsoftmodem_dpsk()
{
	uint8_t set_dpsk_spec_count = (sizeof(wtvsoftmodem_dpsk::DEFAULT_DPSK_SPEC_TABLE) / sizeof(wtvsoftmodem_dpsk::DEFAULT_DPSK_SPEC_TABLE[0]));

	set_dpsk_spec_table(wtvsoftmodem_dpsk::DEFAULT_DPSK_SPEC_TABLE, set_dpsk_spec_count);
}

void wtvsoftmodem_dpsk::set_dpsk_spec_table(const dpsk_detail_t *set_dpsk_spec_table, uint8_t set_dpsk_spec_count)
{
	m_spec_table = set_dpsk_spec_table;
	m_spec_count = set_dpsk_spec_count;
}

void wtvsoftmodem_dpsk::set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate)
{
	m_sample_rate = set_sample_rate;
}

void wtvsoftmodem_dpsk::set_output_scale(double set_output_scale)
{
	m_output_scale = set_output_scale;
}

void wtvsoftmodem_dpsk::init(dpsk_spec_t dpsk_spec)
{
	for (uint8_t didx = 0; didx < m_spec_count; didx++)
	{
		if (m_spec_table[didx].spec == dpsk_spec)
		{
			wtvsoftmodem_dpsk::init(&m_spec_table[didx]);
			break;
		}
	}
}
void wtvsoftmodem_dpsk::init(const dpsk_detail_t *dpsk_detail)
{
	m_tx_detail = dpsk_detail;
	m_rx_detail = dpsk_detail;
	m_rx_settle_syms = dpsk_detail->rx_settle_symbols;

	wtvsoftmodem_dpsk::set_tx_channel(dpsk_detail->default_tx_channel);
	wtvsoftmodem_dpsk::set_rx_channel(dpsk_detail->default_rx_channel);

	reset();
}

void wtvsoftmodem_dpsk::reset()
{
	wtvsoftmodem_dpsk::tx_reset();
	wtvsoftmodem_dpsk::rx_reset();
}

void wtvsoftmodem_dpsk::tx_init(const dpsk_detail_t *dpsk_detail)
{
	m_tx_detail = dpsk_detail;

	wtvsoftmodem_dpsk::set_tx_channel(dpsk_detail->default_tx_channel);

	wtvsoftmodem_dpsk::tx_reset();
}

void wtvsoftmodem_dpsk::set_tx_channel(uint8_t channel_index)
{
	if (m_tx_detail == nullptr || channel_index >= m_tx_detail->channel_count)
		return;

	const dpsk_channel_t &channel = m_tx_detail->channel[channel_index];

	m_tx_baud = channel.baud_rate;
	m_tx_sps = m_sample_rate / channel.baud_rate;
	m_tx_step = wtvsoftmodem_dsp::radians_per_sample(channel.carrier_f, m_sample_rate);
	m_tx_carrier.set_step(m_tx_step);
}

void wtvsoftmodem_dpsk::tx_reset()
{
	m_tx_bits.clear();
	m_tx_phase = 0.0;
	m_tx_phase_cos = 1.0;
	m_tx_phase_sin = 0.0;
	m_tx_carrier.set_phase(0.0);
	m_tx_sym_phase = 0.0;
	m_tx_cur_bit = 1;
	m_tx_reversal_pending = false;
	m_tx_phase_base = 0.0;
	m_tx_ramp_pos = 0;
	m_tx_ramp_len = 0;
}

uint32_t wtvsoftmodem_dpsk::tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	uint32_t sample_idx = 0;
	for (; sample_idx < sample_count; sample_idx++)
	{
		if (m_tx_sym_phase >= 1.0)
		{
			m_tx_sym_phase -= 1.0;

			int32_t bit = 0;

			if (!m_tx_bits.empty())
			{
				bit = m_tx_bits.front();
				m_tx_bits.pop_front();
			}

			if (m_tx_reversal_pending)
			{
				bit ^= 1;
				m_tx_reversal_pending = false;
			}

			m_tx_cur_bit = bit;

			if (bit)
			{

				m_tx_phase_base = m_tx_phase;
				m_tx_ramp_pos = 0;
				m_tx_ramp_len = PHASE_RAMP_SAMPLES;
			}
		}

		if (m_tx_ramp_len > 0)
		{
			const double frac = double(m_tx_ramp_pos) / double(m_tx_ramp_len);
			const double offset = (M_PI * 0.5) * (1.0 - std::cos(M_PI * frac));

			m_tx_phase = wtvsoftmodem_dsp::wrap_radians(m_tx_phase_base + offset);

			m_tx_ramp_pos++;
			if (m_tx_ramp_pos > m_tx_ramp_len)
			{
				m_tx_phase = wtvsoftmodem_dsp::wrap_radians(m_tx_phase_base + M_PI);
				m_tx_ramp_len = 0;
			}

			m_tx_phase_cos = std::cos(m_tx_phase);
			m_tx_phase_sin = std::sin(m_tx_phase);
		}

		double v = (m_tx_carrier.im * m_tx_phase_cos + m_tx_carrier.re * m_tx_phase_sin) * m_output_scale;
		sample[sample_idx] = wtvsoftmodem_dsp::saturate_round(v);

		m_tx_carrier.advance();
		m_tx_sym_phase += 1.0 / m_tx_sps;
	}

	// Clear remaining buffer
	wtvsoftmodem_dsp::clear_samples(sample, sample_count, sample_idx);

	return sample_idx;
}

void wtvsoftmodem_dpsk::rx_init(const dpsk_detail_t *dpsk_detail)
{
	m_rx_detail = dpsk_detail;
	m_rx_settle_syms = dpsk_detail->rx_settle_symbols;

	wtvsoftmodem_dpsk::set_rx_channel(dpsk_detail->default_rx_channel);

	wtvsoftmodem_dpsk::rx_reset();
}
void wtvsoftmodem_dpsk::set_rx_channel(uint8_t channel_index)
{
	if (m_rx_detail == nullptr || channel_index >= m_rx_detail->channel_count)
		return;

	const dpsk_channel_t &channel = m_rx_detail->channel[channel_index];

	m_rx_sps = m_sample_rate / channel.baud_rate;
	m_rx_step = wtvsoftmodem_dsp::radians_per_sample(channel.carrier_f, m_sample_rate);
	m_rx_carrier.set_step(m_rx_step);
}
void wtvsoftmodem_dpsk::rx_reset()
{
	m_rx_carrier.set_phase(0.0);
	m_rx_sym_phase = 0.0;
	m_acc_i = 0.0;
	m_acc_q = 0.0;
	m_prev_i = 0.0;
	m_prev_q = 0.0;
	m_have_prev = false;
	m_settle = 0;
	m_rx_level = 0.0;
	m_rx_symbol_confident = false;
	m_rx_pause = 0;
}

void wtvsoftmodem_dpsk::rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count)
{
	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
	{
		double c = m_rx_carrier.re;
		double sn = m_rx_carrier.im;
		double v = double(sample[sample_idx]);
		m_acc_i += v * c;
		m_acc_q += -v * sn;
		m_rx_carrier.advance();

		m_rx_sym_phase += 1.0 / m_rx_sps;

		if (m_rx_sym_phase >= 1.0)
		{
			m_rx_sym_phase -= 1.0;

			double zi = m_acc_i;
			double zq = m_acc_q;
			m_acc_i = 0.0;
			m_acc_q = 0.0;
			const double mag = std::sqrt(zi * zi + zq * zq);

			const double level_before = m_rx_level;
			m_rx_level = 0.9 * m_rx_level + 0.1 * mag;

			if (m_have_prev)
			{
				const double dot_re = zi * m_prev_i + zq * m_prev_q;
				bool bit = dot_re < 0.0;

				const bool confident = mag > (CONFIDENCE_RATIO * level_before);
				m_rx_symbol_confident = confident;
				if (!confident)
					m_rx_pause = std::max(m_rx_pause, DROPOUT_COOLDOWN_SYMS);

				const bool bit_reliable = (m_rx_pause == 0);

				if (m_rx_pause > 0)
					m_rx_pause--;

				if (m_settle >= m_rx_settle_syms)
				{
					if (m_rx_cb)
						m_rx_cb(bit, bit_reliable);
				}
				else
				{
					m_settle++;
				}
			}

			m_prev_i = zi;
			m_prev_q = zq;
			m_have_prev = true;
		}
	}
}
