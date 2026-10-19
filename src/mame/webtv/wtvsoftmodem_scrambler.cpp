// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_scrambler.h"

// ITU-T V.34 7 Scrambler (also the V.22bis scrambler)

wtvsoftmodem_scrambler::wtvsoftmodem_scrambler()
{
	uint8_t set_scrambler_spec_count = (sizeof(wtvsoftmodem_scrambler::DEFAULT_SCRAMBLER_SPEC_TABLE) / sizeof(wtvsoftmodem_scrambler::DEFAULT_SCRAMBLER_SPEC_TABLE[0]));

	set_scrambler_spec_table(wtvsoftmodem_scrambler::DEFAULT_SCRAMBLER_SPEC_TABLE, set_scrambler_spec_count);

	init(SCRAMBLER_V34);
}

void wtvsoftmodem_scrambler::set_scrambler_spec_table(const scrambler_detail_t *set_scrambler_spec_table, uint8_t set_scrambler_spec_count)
{
	m_spec_table = set_scrambler_spec_table;
	m_spec_count = set_scrambler_spec_count;
}

void wtvsoftmodem_scrambler::init(scrambler_spec_t scrambler_spec)
{
	for (uint8_t sidx = 0; sidx < m_spec_count; sidx++)
	{
		if (m_spec_table[sidx].spec == scrambler_spec)
		{
			wtvsoftmodem_scrambler::init(&m_spec_table[sidx]);
			break;
		}
	}
}

void wtvsoftmodem_scrambler::init(const scrambler_detail_t *scrambler_detail)
{
	m_detail = scrambler_detail;

	wtvsoftmodem_scrambler::set_tx_channel(scrambler_detail->default_tx_channel);
	wtvsoftmodem_scrambler::set_rx_channel(scrambler_detail->default_rx_channel);

	reset();
}

void wtvsoftmodem_scrambler::set_tx_channel(uint8_t channel_index)
{
	if (m_detail == nullptr || channel_index >= m_detail->channel_count)
		return;

	const scrambler_channel_t &channel = m_detail->channel[channel_index];

	m_tx_mask0 = uint64_t(1) << (channel.tap0 - 1);
	m_tx_mask1 = uint64_t(1) << (channel.tap1 - 1);
}

void wtvsoftmodem_scrambler::set_rx_channel(uint8_t channel_index)
{
	if (m_detail == nullptr || channel_index >= m_detail->channel_count)
		return;

	const scrambler_channel_t &channel = m_detail->channel[channel_index];

	m_rx_mask0 = uint64_t(1) << (channel.tap0 - 1);
	m_rx_mask1 = uint64_t(1) << (channel.tap1 - 1);
}
