// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V42BIS_H
#define MAME_WEBTV_WTVSOFTMODEM_V42BIS_H

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

// ITU-T V.42bis

class wtvsoftmodem_v42bis
{
public:

	using sink_t = std::function<void(uint8_t)>;

	wtvsoftmodem_v42bis();

	void init(uint32_t entries, uint32_t length, sink_t sink);
	void reset();

	bool failed() const
	{
		return m_failed;
	}

	bool compressed() const
	{
		return m_compressed;
	}

	void encode(uint8_t c);
	void flush();
	void decode(uint8_t c);

private:

	struct node
	{
		uint32_t parent = 0;
		uint32_t children = 0;
		uint32_t length = 0;
		uint8_t byte = 0;
		bool used = false;
		uint32_t first_child = 0;
		uint32_t next_sibling = 0;
	};

	std::vector<node> m_nodes;
	sink_t m_sink;
	uint32_t m_size = 512;
	uint32_t m_max_length = 6;
	uint32_t m_next = 259;
	uint32_t m_last = 0;
	uint32_t m_current = 0;
	uint32_t m_previous = 0;
	uint32_t m_width = 9;
	uint32_t m_count = 0;
	uint32_t m_sample_chars = 0;
	uint32_t m_sample_bits = 0;
	uint32_t m_bits = 0;
	uint8_t m_escape = 0;
	bool m_escaped = false;
	bool m_compressed = false;
	bool m_failed = false;

	uint32_t child(uint32_t parent, uint8_t c) const;
	void add(uint32_t parent, uint8_t c);
	void transparent(uint8_t c);

	void emit(uint8_t c)
	{
		if (m_sink)
			m_sink(c);
	}

	void account(uint32_t p);
	void code(uint32_t value);
	void pack(uint32_t value);
	void align();

};

#endif // MAME_WEBTV_WTVSOFTMODEM_V42BIS_H
