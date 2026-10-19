// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_BITBLOCK_H
#define MAME_WEBTV_WTVSOFTMODEM_BITBLOCK_H
#pragma once

#include "wtvsoftmodem_dsp.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

struct wtvsoftmodem_bitblock
{
	static constexpr uint32_t CAPACITY = 128;

	std::array<uint64_t, 2> word {};
	uint32_t count = 0;

	static constexpr uint64_t low_mask(uint32_t bits)
	{
		return bits >= 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
	}

	static wtvsoftmodem_bitblock pack(const uint8_t *bits, uint32_t count)
	{
		wtvsoftmodem_bitblock block;

		block.count = count;

		for (uint32_t i = 0; i < count; i++)
			block.word[i / 64] |= uint64_t(bits[i]) << (i % 64);

		return block;
	}

	uint64_t get(uint32_t first, uint32_t len) const
	{
		if (!len)
			return 0;

		uint64_t value = word[first / 64] >> (first % 64);

		if (first % 64 && first / 64 == 0)
			value |= word[1] << (64 - first % 64);

		return value & low_mask(len);
	}

	static constexpr uint64_t reverse(uint64_t v)
	{
		v = ((v >> 1) & 0x5555555555555555ULL) | ((v & 0x5555555555555555ULL) << 1);
		v = ((v >> 2) & 0x3333333333333333ULL) | ((v & 0x3333333333333333ULL) << 2);
		v = ((v >> 4) & 0x0f0f0f0f0f0f0f0fULL) | ((v & 0x0f0f0f0f0f0f0f0fULL) << 4);
		v = ((v >> 8) & 0x00ff00ff00ff00ffULL) | ((v & 0x00ff00ff00ff00ffULL) << 8);
		v = ((v >> 16) & 0x0000ffff0000ffffULL) | ((v & 0x0000ffff0000ffffULL) << 16);

		return (v >> 32) | (v << 32);
	}

	void append(uint64_t value, uint32_t width)
	{
		if (!width)
			return;

		value &= low_mask(width);

		const uint32_t offset = count % 64;

		word[count / 64] |= value << offset;

		if (offset && offset + width > 64)
			word[1] |= value >> (64 - offset);

		count += width;
	}

	void erase(uint32_t position)
	{
		const uint64_t carry = word[1] & 1;

		if (position < 64)
		{
			const uint64_t keep = low_mask(position);
			word[0] = (word[0] & keep) | ((word[0] >> 1) & ~keep) | (carry << 63);
			word[1] >>= 1;
		}
		else
		{
			const uint64_t keep = low_mask(position - 64);
			word[1] = (word[1] & keep) | ((word[1] >> 1) & ~keep);
		}

		count--;
	}

	void truncate(uint32_t bits)
	{
		if (bits >= count)
			return;

		count = bits;

		if (bits < 64)
		{
			word[0] &= low_mask(bits);
			word[1] = 0;
		}
		else
		{
			word[1] &= low_mask(bits - 64);
		}
	}

	uint32_t ones() const
	{
		return wtvsoftmodem_dsp::popcount(word[0]) + wtvsoftmodem_dsp::popcount(word[1]);
	}

	uint32_t trailing_ones() const
	{
		uint32_t run = 0;

		for (uint32_t end = count; end;)
		{
			const uint32_t len = end < 64 ? end : 64;
			const uint32_t chunk = uint32_t(std::countl_one(get(end - len, len) << (64 - len)));

			run += chunk < len ? chunk : len;

			if (chunk < len)
				break;

			end -= len;
		}

		return run;
	}

	int32_t find(uint32_t from, uint64_t history, uint32_t width, uint64_t pattern) const
	{
		const uint32_t lead = width - 1;
		const uint32_t step = 64 - lead;

		for (uint32_t first = from; first < count; first += step)
		{
			const uint32_t len = count - first < step ? count - first : step;

			uint64_t stream;
			if (first > from)
				stream = get(first - lead, lead);
			else
				stream = history & low_mask(lead);
			
			stream |= (get(first, len) << lead);

			uint64_t match = low_mask(len);

			for (uint32_t k = 0; k < width; k++)
				match &= ((pattern >> k) & 1) ? stream >> k : ~stream >> k;

			if (match)
				return int32_t(first + uint32_t(std::countr_zero(match)));
		}
		return -1;
	}

	bool contains(uint64_t history, uint32_t width, uint64_t pattern) const
	{
		return find(0, history, width, pattern) >= 0;
	}

	uint32_t bit(uint32_t position) const
	{
		return uint32_t(word[position / 64] >> (position % 64)) & 1;
	}

	void copy_to(std::vector<uint8_t> &out, uint32_t first, uint32_t len) const
	{
		static constexpr auto SPREAD = []
		{
			std::array<std::array<uint8_t, 8>, 256> table {};

			for (uint32_t value = 0; value < 256; value++)
			{
				for (uint32_t k = 0; k < 8; k++)
					table[value][k] = uint8_t((value >> k) & 1);
			}

			return table;
		}();

		const size_t base = out.size();
		out.resize(base + len);

		uint8_t *dest = out.data() + base;

		uint32_t i = 0;
		for (; i + 8 <= len; i += 8)
			std::memcpy(dest + i, SPREAD[get(first + i, 8)].data(), 8);

		for (; i < len; i++)
			dest[i] = uint8_t(bit(first + i));
	}
};

#endif
