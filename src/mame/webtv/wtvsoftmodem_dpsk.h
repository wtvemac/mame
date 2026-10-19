// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_DPSK_H
#define MAME_WEBTV_WTVSOFTMODEM_DPSK_H

#pragma once

#include "wtvsoftmodem_dsp.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>

class wtvsoftmodem_dpsk
{

public:
	static constexpr uint8_t MAX_CHANNELS = 8;

	typedef uint8_t dpsk_spec_t;

	typedef struct dpsk_channel
	{
		wtvsoftmodem_dsp::frequency_t carrier_f;
		wtvsoftmodem_dsp::frequency_t baud_rate;
	} dpsk_channel_t;

	typedef struct dpsk_detail
	{
		dpsk_spec_t spec;
		uint8_t channel_count;
		dpsk_channel_t channel[MAX_CHANNELS];
		uint8_t default_tx_channel;
		uint8_t default_rx_channel;
		uint8_t rx_settle_symbols;
	} dpsk_detail_t;

	static constexpr dpsk_spec_t DPSK_V34_INFO = 0x01;

	static constexpr uint8_t V34_INFO_CALL_CHANNEL = 0;
	static constexpr uint8_t V34_INFO_ANSWER_CHANNEL = 1;

	typedef std::function<void(bool bit, bool reliable)> rx_bit_cb;

	wtvsoftmodem_dpsk();
	virtual ~wtvsoftmodem_dpsk() = default;

	void set_dpsk_spec_table(const dpsk_detail_t *set_dpsk_detail_table, uint8_t set_dpsk_detail_count);
	void set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate);
	void set_output_scale(double set_output_scale);

	void init(dpsk_spec_t dpsk_spec);
	void init(const dpsk_detail_t *dpsk_detail);
	virtual void reset();

	void tx_init(const dpsk_detail_t *dpsk_detail);
	void set_tx_channel(uint8_t channel_index);

	uint32_t tx(wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	wtvsoftmodem_dsp::frequency_t tx_baud_rate() const
	{
		return m_tx_baud;
	}

	void tx_put_bit(bool b)
	{
		m_tx_bits.push_back(b ? 1 : 0);
	}

	void tx_put_bits(const uint8_t *b, uint32_t n)
	{
		for (uint32_t i = 0; i < n; i++)
			m_tx_bits.push_back(b[i] & 1);
	}

	size_t tx_pending_bits() const
	{
		return m_tx_bits.size();
	}

	void tx_reversal()
	{
		m_tx_reversal_pending = true;
	}


	void rx_init(const dpsk_detail_t *dpsk_detail);
	void set_rx_channel(uint8_t channel_index);

	virtual void rx(const wtvsoftmodem_dsp::sample_t *sample, uint32_t sample_count);

	void set_rx_bit_callback(rx_bit_cb cb)
	{
		m_rx_cb = std::move(cb);
	}

	double rx_level() const
	{
		return m_rx_level;
	}

	bool rx_symbol_confident() const
	{
		return m_rx_symbol_confident;
	}

	void rx_pause_adaptation(int32_t syms)
	{
		m_rx_pause = std::max(m_rx_pause, syms);
	}

protected:

	double rx_step() const
	{
		return m_rx_step;
	}
	double rx_sps() const
	{
		return m_rx_sps;
	}
	int32_t rx_settle_symbols() const
	{
		return m_rx_settle_syms;
	}

private:

	void tx_reset();
	void rx_reset();

	static constexpr double CONFIDENCE_RATIO = 0.30;
	static constexpr int32_t DROPOUT_COOLDOWN_SYMS = 40;
	static constexpr int32_t PHASE_RAMP_SAMPLES = 8;

	static constexpr dpsk_detail_t DEFAULT_DPSK_SPEC_TABLE[] = {
		{
			.spec = DPSK_V34_INFO,
			.channel_count = 2,
			.channel = {
				{
					.carrier_f = 1200,
					.baud_rate = 600
				},
				{
					.carrier_f = 2400,
					.baud_rate = 600
				}
			},
			.default_tx_channel = V34_INFO_ANSWER_CHANNEL,
			.default_rx_channel = V34_INFO_CALL_CHANNEL,
			.rx_settle_symbols = 4
		}
	};

	uint8_t m_spec_count = 0;

	const dpsk_detail_t *m_spec_table = nullptr;
	const dpsk_detail_t *m_tx_detail = nullptr;
	const dpsk_detail_t *m_rx_detail = nullptr;

	wtvsoftmodem_dsp::frequency_t m_sample_rate = wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE;

	double m_output_scale = 6000.0;

	double m_tx_sps = 1.0;
	double m_tx_step = 0.0;
	double m_rx_sps = 1.0;
	double m_rx_step = 0.0;
	int32_t m_rx_settle_syms = 0;
	wtvsoftmodem_dsp::oscillator m_rx_carrier;
	double m_rx_level = 0.0;
	bool m_rx_symbol_confident = false;
	rx_bit_cb m_rx_cb;
	int32_t m_rx_pause = 0;

	wtvsoftmodem_dsp::frequency_t m_tx_baud = 0.0;
	std::deque<uint8_t> m_tx_bits;
	double m_tx_phase = 0.0;
	double m_tx_phase_cos = 1.0;
	double m_tx_phase_sin = 0.0;
	wtvsoftmodem_dsp::oscillator m_tx_carrier;
	double m_tx_sym_phase = 0.0;
	int32_t m_tx_cur_bit = 1;
	bool m_tx_reversal_pending = false;
	double m_tx_phase_base = 0.0;
	int32_t m_tx_ramp_pos = 0;
	int32_t m_tx_ramp_len = 0;

	double m_rx_sym_phase = 0.0;
	double m_acc_i = 0.0;
	double m_acc_q = 0.0;
	double m_prev_i = 0.0;
	double m_prev_q = 0.0;
	bool m_have_prev = false;
	int32_t m_settle = 0;

};

#endif // MAME_WEBTV_WTVSOFTMODEM_DPSK_H
