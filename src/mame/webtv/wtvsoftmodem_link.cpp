// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_link.h"

#include <algorithm>
#include <array>

// ITU-T V.42 8 LAPM procedures

wtvsoftmodem_link::wtvsoftmodem_link()
{
	reset();
}

void wtvsoftmodem_link::reset()
{
	m_detection.reset();
	m_tx.clear();
	m_raw.clear();
	m_host.clear();
	m_wire.clear();
	m_pending.clear();
	m_open = false;
	m_seen_flag = false;
	m_flag = 0;
	m_vs = 0;
	m_vr = 0;
	m_remote_busy = false;
	m_txflag = 0;
	m_clock = 0;
	m_retry = 0;
	m_crc_fail = 0;
	m_crc_ok = 0;
	m_compression = 0;
	m_length = 6;
	m_max_info = 128;
	m_window_max = 15;
	m_window = WINDOW_SEED;
	m_inflight_bits = 0;
	m_tx_sup.clear();
	m_bits_sent = 0;
	m_rtt_bits = 0;
	m_grow = 0;
	m_rej_sent = false;

	init_compression(6);
}

void wtvsoftmodem_link::retrain()
{
	m_tx.clear();
	m_txflag = 0;
	m_raw.clear();
	m_flag = 0;
	m_seen_flag = false;
	m_tx_sup.clear();
	m_rej_sent = false;
	m_clock = 0;

	if (m_open)
	{
		resend_pending();
		frame({
			ADDR_OUR_COMMAND,
			S_RR,
			uint8_t(m_vr << 1)
		});
	}
}

bool wtvsoftmodem_link::rx_bit(bool bit)
{
	if (waiting_for_odp())
	{
		const bool detected = m_detection.rx_bit(bit);
		return detected;
	}
	else
	{
		m_flag = uint8_t((m_flag >> 1) | (uint8_t(bit) << 7));
		m_raw.push_back(bit);

		if (m_flag == 0x7e)
		{
			if (m_seen_flag && m_raw.size() > 8)
				receive();

			m_raw.clear();
			m_seen_flag = true;
		}
		else if (m_raw.size() > 16384)
		{
			m_raw.clear();
			m_seen_flag = false;
		}

		return false;
	}
}

bool wtvsoftmodem_link::tx_get_bit()
{
	if (waiting_for_odp())
		return true;

	if (sending_adp())
		return m_detection.tx_get_bit();

	m_bits_sent++;
	m_clock++;
	if (m_clock >= m_retry_bits)
	{
		m_clock = 0;
		if (m_open && !m_pending.empty())
		{
			m_retry++;
			if (uint64_t(m_retry) * m_retry_bits < 20ULL * m_rate)
			{
				on_busy();

				if (!m_remote_busy)
					resend_pending();

				frame({
					ADDR_OUR_COMMAND,
					S_RR,
					uint8_t((m_vr << 1) | 1)
				});
			}
			else
			{
				m_open = false;
			}
		}
	}

	if (m_tx.empty())
		load_next_frame();

	if (m_tx.empty())
	{
		bool b = (0x7e >> m_txflag) & 1;

		m_txflag = (m_txflag + 1) & 7;

		return b;
	}
	else
	{
		bool b = m_tx.front();

		m_tx.pop_front();

		return b;
	}
}

uint16_t wtvsoftmodem_link::crc(uint16_t f, uint8_t c)
{
	static constexpr auto TABLE = []
	{
		std::array<uint16_t, 256> t {};
		for (uint32_t i = 0; i < t.size(); i++)
		{
			uint16_t value = uint16_t(i);

			for (int32_t bit = 0; bit < 8; bit++)
				value = (value >> 1) ^ ((value & 1) ? 0x8408 : 0);

			t[i] = value;
		}

		return t;
	}();

	return uint16_t((f >> 8) ^ TABLE[(f ^ c) & 0xff]);
}

std::vector<bool> wtvsoftmodem_link::encode(std::vector<uint8_t> bytes, uint16_t f)
{
	for (uint8_t b : bytes)
		f = crc(f, b);
	f = ~f;

	bytes.push_back(uint8_t(f));
	bytes.push_back(uint8_t(f >> 8));

	std::vector<bool> bits;
	bits.reserve(bytes.size() * 9 + 16);
	flag(bits);

	uint32_t ones = 0;
	for (uint8_t b : bytes)
	{
		for (uint32_t i = 0; i < 8; i++)
		{
			bool bit = (b >> i) & 1;

			bits.push_back(bit);

			ones = bit ? ones + 1 : 0;
			if (ones == 5)
			{
				bits.push_back(false);
				ones = 0;
			}
		}
	}
	flag(bits);

	return bits;
}

void wtvsoftmodem_link::load_next_frame()
{
	if (!m_tx_sup.empty())
	{
		emit(m_tx_sup.front());

		m_tx_sup.pop_front();

		return;
	}

	for (auto &p : m_pending)
	{
		if (p.resend_due)
		{
			p.resend_due = false;
			p.resends++;

			send_data(p);

			return;
		}
	}

	pump();
}

void wtvsoftmodem_link::emit(const std::vector<bool> &bits)
{
	while (m_txflag)
	{
		m_tx.push_back((0x7e >> m_txflag) & 1);
		m_txflag = (m_txflag + 1) & 7;
	}
	m_tx.insert(m_tx.end(), bits.begin(), bits.end());
}

void wtvsoftmodem_link::note_ack(const outstanding &p)
{
	m_inflight_bits -= std::min(m_inflight_bits, p.frame_bits);
	m_grow++;

	if (m_window < m_window_max && m_grow >= m_window)
	{
		m_window++;
		m_grow = 0;
	}

	if (!p.resends && m_bits_sent > p.sent_bits)
	{
		const uint32_t rtt = uint32_t(std::min<uint64_t>(m_bits_sent - p.sent_bits, 2 * m_rate));

		m_rtt_bits = m_rtt_bits ? (m_rtt_bits * 3 + rtt) / 4 : rtt;
		m_retry_bits = std::min(2 * m_rate, std::max(m_rate / 2, 4 * m_rtt_bits));
	}
}

bool wtvsoftmodem_link::may_send() const
{
	if (!m_open || m_remote_busy || m_pending.size() >= m_window)
		return false;
	else if (!m_rtt_bits)
		return m_pending.size() < WINDOW_SEED;
	else
		return m_inflight_bits < m_rtt_bits + 2 * nominal_frame_bits();
}

bool wtvsoftmodem_link::unstuff_frame(std::vector<uint8_t> &bytes)
{
	uint32_t ones = 0;
	uint32_t pos = 0;
	uint8_t byte = 0;

	for (size_t i = 0; i + 8 < m_raw.size(); i++)
	{
		const bool bit = m_raw[i];

		if (ones == 5)
		{
			if (bit)
				return false;

			ones = 0;

			continue;
		}

		ones = bit ? ones + 1 : 0;
		byte |= uint8_t(bit) << pos;

		pos++;

		if (pos == 8)
		{
			bytes.push_back(byte);
			byte = 0;
			pos = 0;
		}
	}

	if (pos || bytes.size() < 4)
		return false;

	uint16_t f = 0xffff;
	for (uint8_t b : bytes)
		f = crc(f, b);

	if (f != FCS_GOOD)
	{
		m_crc_fail++;

		return false;
	}

	m_crc_ok++;
	bytes.resize(bytes.size() - 2);

	return true;
}

void wtvsoftmodem_link::receive()
{
	std::vector<uint8_t> bytes;

	if (!unstuff_frame(bytes))
		return;

	if (bytes[0] != ADDR_PEER_COMMAND && bytes[0] != ADDR_PEER_RESPONSE)
		return;

	if ((bytes[1] & 3) == 3)
		receive_unnumbered(bytes);
	else if (m_open && bytes.size() >= 3)
		receive_numbered(bytes);
}

void wtvsoftmodem_link::receive_unnumbered(const std::vector<uint8_t> &bytes)
{
	const bool command = bytes[0] == ADDR_PEER_COMMAND;
	const uint8_t control = bytes[1];

	switch (control & ~PF_BIT)
	{
		case U_XID:
		{
			if (command)
				xid(bytes);
			break;
		}

		case U_SABME:
		{
			if (command)
			{
				open_link();
				frame({
					ADDR_OUR_RESPONSE,
					uint8_t(U_UA | (control & PF_BIT))
				});
			}
			break;
		}

		case U_DISC:
		{
			frame({
				ADDR_OUR_RESPONSE,
				uint8_t(U_UA | (control & PF_BIT))
			});
			m_open = false;
			break;
		}

		case U_TEST:
			if (command)
				frame(bytes);
			break;

		default:
			break;
	}
}

void wtvsoftmodem_link::receive_numbered(const std::vector<uint8_t> &bytes)
{
	acknowledge(bytes[2] >> 1);

	if (!(bytes[1] & 1))
		receive_information(bytes);
	else
		receive_supervisory(bytes);
}

void wtvsoftmodem_link::acknowledge(uint8_t nr)
{
	uint32_t delta = (nr - (m_pending.empty() ? m_vs : m_pending.front().seq)) & 127;

	if (!delta || delta > m_pending.size())
		return;

	while (delta--)
	{
		note_ack(m_pending.front());
		m_pending.pop_front();
	}

	m_retry = 0;
	m_clock = 0;
}

void wtvsoftmodem_link::receive_information(const std::vector<uint8_t> &bytes)
{
	const uint8_t ns = bytes[1] >> 1;
	const bool poll = bytes[2] & 1;

	if (ns == m_vr)
	{
		m_vr = (m_vr + 1) & 127;
		m_rej_sent = false;

		for (size_t i = 3; i < bytes.size(); i++)
		{
			if (m_compression & 1)
				m_decoder.decode(bytes[i]);
			else
				deliver(bytes[i]);
		}

		if (m_decoder.failed())
		{
			m_open = false;

			return;
		}
	}

	const bool gap = ns != m_vr && ((ns - m_vr) & 127) < 64;
	const bool rej = gap && (!m_rej_sent || poll);

	if (gap)
		m_rej_sent = true;

	frame({
		ADDR_OUR_RESPONSE,
		uint8_t(rej ? S_REJ : S_RR),
		uint8_t((m_vr << 1) | uint8_t(poll))
	});
}

void wtvsoftmodem_link::receive_supervisory(const std::vector<uint8_t> &bytes)
{
	const uint8_t control = bytes[1];
	const uint8_t nr = bytes[2] >> 1;
	const bool poll = bytes[2] & 1;
	const bool was_busy = m_remote_busy;

	m_remote_busy = control == S_RNR;
	if (m_remote_busy && !was_busy)
		on_busy();

	if (control == S_REJ)
	{
		resend_pending();
	}
	else if (control == S_SREJ)
	{
		for (auto &p : m_pending)
		{
			if (p.seq == nr)
				p.resend_due = true;
		}
	}

	if (bytes[0] == ADDR_PEER_COMMAND && poll)
		frame({
			ADDR_OUR_RESPONSE,
			S_RR,
			uint8_t((m_vr << 1) | 1)
		});
}

void wtvsoftmodem_link::open_link()
{
	m_vs = 0;
	m_vr = 0;
	m_pending.clear();
	m_wire.clear();
	m_open = true;
	m_retry = 0;
	m_rej_sent = false;
	m_inflight_bits = 0;

	init_compression(m_length);
}

void wtvsoftmodem_link::init_compression(uint32_t length)
{
	m_encoder.init(
		512,
		length,
		[this](uint8_t b)
		{
			m_wire.push_back(b);
		}
	);

	m_decoder.init(
		512,
		length,
		[this](uint8_t b)
		{
			deliver(b);
		}
	);
}

void wtvsoftmodem_link::xid(const std::vector<uint8_t> &request)
{
	if (m_open)
		return;

	if (request.size() < 3 || request[2] != 0x82)
		return;

	uint32_t peer_tx = 128;
	uint32_t peer_rx = 128;
	uint32_t peer_window = 15;
	uint32_t compression = 0;
	uint32_t length = 6;
	uint32_t entries = 512;
	bool v42 = false;

	for (size_t i = 3; i + 3 <= request.size();)
	{
		uint32_t group = request[i];
		uint32_t size = (uint32_t(request[i + 1]) << 8) | request[i + 2];

		i += 3;

		if (size > request.size() - i)
			return;

		size_t end = i + size;
		while (i + 2 <= end)
		{
			uint32_t id = request[i];
			uint32_t len = request[i + 1];
			i += 2;
			if (len > end - i)
				return;

			uint32_t value = 0;
			for (uint32_t j = 0; j < len && j < 4; j++)
				value = (value << 8) | request[i + j];

			if (group == 0x80)
			{
				if (id == 5)
					peer_tx = value / 8;
				if (id == 6)
					peer_rx = value / 8;
				if (id == 8)
					peer_window = value;
			}

			if (group == 0xf0)
			{
				if (id == 0)
					v42 = len == 3 && value == 0x563432;
				else if (v42 && id == 1)
					compression = value & 3;
				else if (v42 && id == 2)
					entries = value;
				else if (v42 && id == 3)
					length = value;
			}

			i += len;
		}

		if (i != end)
			return;
	}
	if (entries < 512 || length < 6 || length > 250 || !peer_tx || !peer_rx || !peer_window)
		return;
	m_max_info = std::min(peer_rx, 128U);

	m_window_max = std::min(peer_window, 15U);
	m_window = m_window_max;
	m_rtt_bits = 0;
	m_inflight_bits = 0;
	m_grow = 0;
	m_compression = compression;
	m_length = length;

	// HDLC XID data for LAPM
	// Reference ITU-T V.42 (03/2002), 12.2 "Information fields in XID frames"
	frame({
		ADDR_OUR_RESPONSE,                     // Address: DLCI 0, C/R = 1 (answerer response), EA = 1
		uint8_t(request[1]),                   // Control: XID, P/F echoed from the command
		0x82,                                  // Format identifier: general purpose (ISO 8885)
		0x80,                                  // Group identifier: parameter negotiation
		0x00,                                  // Group length
		0x13,
		0x03,                                  // PI 3: HDLC optional functions
		0x03,                                  // PL: 3 bytes
		0x8a,                                  // Bits 1-8: REJ (2), UI (4), extended addressing (8)
		0x89,                                  // Bits 9-16: delete response I frames (9), modulo 128 (12), 16-bit FCS (16)
		0x00,                                  // Bits 17-24: none (no 32-bit FCS)
		0x05,                                  // PI 5: maximum information field length, transmit bits (N401)
		0x02,                                  // PL: 2 bytes
		uint8_t(m_max_info >> 5),              // Our transmit N401: m_max_info * 8
		uint8_t(m_max_info << 3),
		0x06,                                  // PI 6: maximum information field length, receive (N401, in bits)
		0x02,                                  // PL: 2 bytes
		uint8_t(std::min(peer_tx, 128U) >> 5), // Our receive N401: peer transmit size, capped at 128 octets, * 8
		uint8_t(std::min(peer_tx, 128U) << 3),
		0x07,                                  // PI 7: window size, transmit (k)
		0x01,                                  // PL: 1 byte
		uint8_t(m_window_max),                 // Our transmit window: peer receive window, capped at 15
		0x08,                                  // PI 8: window size, receive (k)
		0x01,                                  // PL: 1 byte
		0x0f,                                  // Our receive window: 15 frames
		0xf0,                                  // Group identifier: private parameter negotiation (V.42bis)
		0x00,                                  // Group length (15 bytes)
		0x0f,
		0x00,                                  // PI 0: parameter set identification
		0x03,                                  // PL: 3 bytes
		0x56,                                  // 'V'
		0x34,                                  // '4'
		0x32,                                  // '2'
		0x01,                                  // PI 1: V.42bis data compression request (P0)
		0x01,                                  // PL: 1 byte
		uint8_t(compression),                  // P0: 0 none, 1 initiator to responder, 2 responder to initiator, 3 both
		0x02,                                  // PI 2: V.42bis number of codewords (P1)
		0x02,                                  // PL: 2 bytes
		0x02,                                  // P1: 0x0200 = 512 codewords
		0x00,
		0x03,                                  // PI 3: V.42bis maximum string length (P2)
		0x01,                                  // PL: 1 byte
		uint8_t(length),                       // P2: maximum string length
	});
}

void wtvsoftmodem_link::send_data(outstanding &p)
{
	std::vector<uint8_t> b = {
		ADDR_OUR_COMMAND,
		uint8_t(p.seq << 1),
		uint8_t(m_vr << 1)
	};
	b.insert(b.end(), p.bytes.begin(), p.bytes.end());

	const std::vector<bool> bits = encode(std::move(b));

	p.sent_bits = m_bits_sent + m_tx.size() + bits.size();

	emit(bits);
}

void wtvsoftmodem_link::pump()
{
	if (!may_send())
		return;

	uint32_t count = 0;
	while (!m_host.empty() && count++ < 128)
	{
		uint8_t b = m_host.front();

		m_host.pop_front();

		if (m_compression & 2)
			m_encoder.encode(b);
		else
			m_wire.push_back(b);
	}

	if (m_compression & 2)
		m_encoder.flush();

	if (m_wire.empty())
		return;

	outstanding p;
	p.seq = m_vs;

	m_vs = (m_vs + 1) & 127;

	while (!m_wire.empty() && p.bytes.size() < m_max_info)
	{
		p.bytes.push_back(m_wire.front());
		m_wire.pop_front();
	}

	if (m_pending.empty())
		m_clock = 0;

	m_pending.push_back(std::move(p));

	outstanding &q = m_pending.back();
	q.frame_bits = uint32_t((q.bytes.size() + 5) * 8 + 24);
	m_inflight_bits += q.frame_bits;

	send_data(q);
}
