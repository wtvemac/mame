// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v42bis.h"

#include <algorithm>
#include <array>

// ITU-T V.42bis

wtvsoftmodem_v42bis::wtvsoftmodem_v42bis()
{
	//
}

void wtvsoftmodem_v42bis::init(uint32_t entries, uint32_t length, sink_t sink)
{
	m_size = std::clamp(entries, 512U, 65535U);
	m_max_length = std::clamp(length, 6U, 250U);
	m_sink = std::move(sink);

	reset();
}

void wtvsoftmodem_v42bis::reset()
{
	m_nodes.assign(m_size, {});

	for (uint32_t i = 3; i < 259; i++)
	{
		m_nodes[i].used = true;
		m_nodes[i].byte = i - 3;
		m_nodes[i].length = 1;
	}

	m_next = 259;
	m_last = 0;
	m_current = 0;
	m_previous = 0;
	m_escape = 0;
	m_escaped = false;
	m_compressed = false;
	m_width = 9;
	m_bits = 0;
	m_count = 0;
	m_sample_chars = 0;
	m_sample_bits = 0;
	m_failed = false;
}

void wtvsoftmodem_v42bis::encode(uint8_t c)
{
	if (m_previous)
	{
		add(m_previous, c);
		m_previous = 0;
	}

	uint32_t found = m_current ? child(m_current, c) : 0;
	if (m_current && found && found != m_last && m_nodes[found].length <= m_max_length)
	{
		m_current = found;
	}
	else
	{
		if (m_current)
		{
			account(m_current);

			if (m_compressed)
				code(m_current);

			add(m_current, c);

			if (m_sample_chars >= 128)
			{
				bool want;
				if (m_compressed)
					want = (m_sample_bits < m_sample_chars * 8);
				else
					want = m_sample_bits * 10 < m_sample_chars * 8 * 9;

				if (want != m_compressed)
				{
					if (want)
					{
						emit(m_escape);
						emit(0);
					}
					else
					{
						code(0);
						align();
					}
					m_compressed = want;
				}

				m_sample_chars = 0;
				m_sample_bits = 0;
			}
		}

		m_current = uint32_t(c) + 3;
	}

	if (!m_compressed)
	{
		emit(c);

		if (c == m_escape)
			emit(1);
	}

	if (c == m_escape)
		m_escape = uint8_t(m_escape + 51);
}

void wtvsoftmodem_v42bis::flush()
{
	if (m_compressed && m_current)
	{
		account(m_current);
		code(m_current);

		m_previous = m_current;
		m_current = 0;

		if (m_count)
		{
			code(1);
			align();
		}
	}
}

void wtvsoftmodem_v42bis::decode(uint8_t c)
{
	if (m_failed)
		return;

	if (!m_compressed)
	{
		if (m_escaped)
		{
			m_escaped = false;

			if (c == 0)
			{
				m_previous = m_current;
				m_current = 0;
				m_compressed = true;
			}
			else if (c == 1)
			{
				transparent(m_escape);
				m_escape = uint8_t(m_escape + 51);
			}
			else if (c == 2)
			{
				reset();
			}
			else
			{
				m_failed = true;
			}
		}
		else if (c == m_escape)
		{
			m_escaped = true;
		}
		else
		{
			transparent(c);
		}
	}
	else
	{
		m_bits |= uint32_t(c) << m_count;
		m_count += 8;

		while (m_compressed && m_count >= m_width && !m_failed)
		{
			uint32_t value = m_bits & ((1U << m_width) - 1);
			m_bits >>= m_width;
			m_count -= m_width;

			if (value == 0)
			{
				m_compressed = false;
				m_current = 0;
				m_bits = 0;
				m_count = 0;
			}
			else if (value == 1)
			{
				m_bits = 0;
				m_count = 0;
			}
			else if (value == 2)
			{
				if ((1U << m_width) >= m_size)
					m_failed = true;
				else
					m_width++;
			}
			else
			{
				if (value >= m_size || !m_nodes[value].used)
				{
					m_failed = true;
					break;
				}

				std::array<uint8_t, 250> str;
				uint32_t begin = str.size();
				for (uint32_t p = value; p; p = m_nodes[p].parent)
				{
					begin--;
					str[begin] = m_nodes[p].byte;
				}

				if (m_previous)
					add(m_previous, str[begin]);

				m_previous = value;
				for (uint32_t i = begin; i < str.size(); i++)
				{
					const uint8_t b = str[i];
					emit(b);
					if (b == m_escape)
						m_escape = uint8_t(m_escape + 51);
				}
			}
		}
	}
}

uint32_t wtvsoftmodem_v42bis::child(uint32_t parent, uint8_t c) const
{
	for (uint32_t i = m_nodes[parent].first_child; i; i = m_nodes[i].next_sibling)
	{
		if (m_nodes[i].byte == c)
			return i;
	}

	return 0;
}

void wtvsoftmodem_v42bis::add(uint32_t parent, uint8_t c)
{
	if (!parent || m_nodes[parent].length >= m_max_length || child(parent, c))
		return;

	m_last = m_next;
	auto &n = m_nodes[m_next];
	n = { parent, 0, m_nodes[parent].length + 1, c, true, 0, m_nodes[parent].first_child };
	m_nodes[parent].first_child = m_next;

	m_nodes[parent].children++;
	do
	{
		m_next++;
		if (m_next == m_size)
			m_next = 259;
	} while (m_nodes[m_next].used && m_nodes[m_next].children);

	auto &free = m_nodes[m_next];

	if (free.used)
	{
		uint32_t *link = &m_nodes[free.parent].first_child;
		while (*link != m_next)
			link = &m_nodes[*link].next_sibling;

		*link = free.next_sibling;

		m_nodes[free.parent].children--;

		free = {};
	}
}

void wtvsoftmodem_v42bis::transparent(uint8_t c)
{
	if (m_previous)
	{
		add(m_previous, c);
		m_previous = 0;
	}

	uint32_t found = m_current ? child(m_current, c) : 0;
	if (m_current && found && found != m_last)
	{
		m_current = found;
	}
	else
	{
		if (m_current)
			add(m_current, c);

		m_current = uint32_t(c) + 3;
	}

	emit(c);
}

void wtvsoftmodem_v42bis::account(uint32_t p)
{
	m_sample_chars += m_nodes[p].length;
	m_sample_bits += m_width;
}

void wtvsoftmodem_v42bis::code(uint32_t value)
{
	while (value >= (1U << m_width))
	{
		pack(2);
		++m_width;
	}

	pack(value);
}

void wtvsoftmodem_v42bis::pack(uint32_t value)
{
	m_bits |= uint32_t(value) << m_count;

	m_count += m_width;
	while (m_count >= 8)
	{
		emit(uint8_t(m_bits));

		m_bits >>= 8;
		m_count -= 8;
	}
}

void wtvsoftmodem_v42bis::align()
{
	if (m_count)
		emit(uint8_t(m_bits));

	m_count = 0;
	m_bits = 0;
}
