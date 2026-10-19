// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_LINK_H
#define MAME_WEBTV_WTVSOFTMODEM_LINK_H

#pragma once

#include "wtvsoftmodem_lapm.h"
#include "wtvsoftmodem_v42bis.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

// ITU-T V.42 8 LAPM procedures

class wtvsoftmodem_link
{
public:

	static constexpr uint32_t WINDOW_SEED = 4;

	using sink_t = std::function<void(uint8_t)>;

	wtvsoftmodem_link();

	void set_bit_rate(uint32_t rate)
	{
		m_rate = std::max(rate, 2400U);

		m_retry_bits = 2 * m_rate;
	}

	void set_rx_byte_callback(sink_t cb)
	{
		m_sink = std::move(cb);
	}

	void reset();

	void retrain();

	bool waiting_for_odp() const
	{
		return m_detection.waiting_for_odp();
	}

	bool sending_adp() const
	{
		return m_detection.sending_adp();
	}

	bool done() const
	{
		return m_detection.done();
	}

	bool connected() const
	{
		return m_open;
	}

	uint32_t crc_fail_count() const
	{
		return m_crc_fail;
	}

	uint32_t crc_ok_count() const
	{
		return m_crc_ok;
	}

	uint32_t retry_count() const
	{
		return m_retry;
	}

	void tx_put_byte(uint8_t b)
	{
		m_host.push_back(b);
	}

	bool rx_bit(bool bit);

	bool tx_get_bit();

private:

	static constexpr uint16_t FCS_GOOD = 0xf0b8;

	static constexpr uint8_t ADDR_OUR_COMMAND   = 0x01;
	static constexpr uint8_t ADDR_OUR_RESPONSE  = 0x03;
	static constexpr uint8_t ADDR_PEER_COMMAND  = 0x03;
	static constexpr uint8_t ADDR_PEER_RESPONSE = 0x01;

	static constexpr uint8_t PF_BIT  = 0x10;

	static constexpr uint8_t U_XID   = 0xaf;
	static constexpr uint8_t U_SABME = 0x6f;
	static constexpr uint8_t U_DISC  = 0x43;
	static constexpr uint8_t U_UA    = 0x63;
	static constexpr uint8_t U_TEST  = 0xe3;

	static constexpr uint8_t S_RR    = 0x01;
	static constexpr uint8_t S_RNR   = 0x05;
	static constexpr uint8_t S_REJ   = 0x09;
	static constexpr uint8_t S_SREJ  = 0x0d;

	struct outstanding
	{
		uint8_t seq = 0;
		bool resend_due = false;
		uint32_t resends = 0;
		uint32_t frame_bits = 0;
		uint64_t sent_bits = 0;
		std::vector<uint8_t> bytes;
	};

	wtvsoftmodem_lapm m_detection;
	wtvsoftmodem_v42bis m_encoder;
	wtvsoftmodem_v42bis m_decoder;
	sink_t m_sink;
	std::deque<bool> m_tx;
	std::deque<std::vector<bool>> m_tx_sup;
	std::vector<bool> m_raw;
	std::deque<uint8_t> m_host;
	std::deque<uint8_t> m_wire;
	std::deque<outstanding> m_pending;
	uint8_t m_flag = 0;
	uint8_t m_vs = 0;
	uint8_t m_vr = 0;
	uint32_t m_rate = 56000;
	uint32_t m_retry_bits = 112000;
	uint32_t m_txflag = 0;
	uint32_t m_clock = 0;
	uint32_t m_retry = 0;
	uint32_t m_compression = 0;
	uint32_t m_length = 6;
	uint32_t m_max_info = 128;
	uint32_t m_window = WINDOW_SEED;
	uint32_t m_window_max = 15;
	uint64_t m_bits_sent = 0;
	uint32_t m_rtt_bits = 0;
	uint32_t m_inflight_bits = 0;
	uint32_t m_grow = 0;
	uint32_t m_crc_fail = 0;
	uint32_t m_crc_ok = 0;
	bool m_open = false;
	bool m_seen_flag = false;
	bool m_remote_busy = false;
	bool m_rej_sent = false;

	static uint16_t crc(uint16_t f, uint8_t c);

	void deliver(uint8_t byte)
	{
		if (m_sink)
			m_sink(byte);
	}

	static void flag(std::vector<bool> &bits)
	{
		for (uint32_t i = 0; i < 8; i++)
			bits.push_back((0x7e >> i) & 1);
	}

	static std::vector<bool> encode(std::vector<uint8_t> bytes, uint16_t f = 0xffff);

	void frame(std::vector<uint8_t> bytes)
	{
		m_tx_sup.push_back(encode(std::move(bytes)));
	}

	void load_next_frame();
	void emit(const std::vector<bool> &bits);

	void resend_pending()
	{
		for (auto &p : m_pending)
			p.resend_due = true;
	}

	void on_busy()
	{
		m_grow = 0;
		m_window = std::max(2U, m_window / 2);
	}

	void note_ack(const outstanding &p);
	bool may_send() const;

	uint32_t nominal_frame_bits() const
	{
		return (m_max_info + 5) * 8 + 24;
	}

	bool unstuff_frame(std::vector<uint8_t> &bytes);
	void receive();
	void receive_unnumbered(const std::vector<uint8_t> &bytes);
	void receive_numbered(const std::vector<uint8_t> &bytes);
	void acknowledge(uint8_t nr);
	void receive_information(const std::vector<uint8_t> &bytes);
	void receive_supervisory(const std::vector<uint8_t> &bytes);

	void open_link();
	void init_compression(uint32_t length);
	void xid(const std::vector<uint8_t> &request);

	void send_data(outstanding &p);
	void pump();
};

#endif // MAME_WEBTV_WTVSOFTMODEM_LINK_H
