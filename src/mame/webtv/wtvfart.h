// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVFART_H
#define MAME_WEBTV_WTVFART_H

#pragma once

namespace wtvfart {

static constexpr uint32_t MAGIC             = 0x46415254; // 'FART'
static constexpr uint32_t DISCOVERY_MAGIC   = 0x800003f8;
static constexpr uint32_t DISCOVERY_POINTER = 0x800003fc;

static constexpr uint32_t DIRECTORY_VERSION     = 2;
static constexpr uint32_t DIRECTORY_HEADER_SIZE = 0x10;
static constexpr uint32_t DIRECTORY_ENTRY_SIZE  = 0x08;
static constexpr uint32_t MAX_FEATURES          = 8;

static constexpr uint32_t FEATURE_POINTER = 0x504e5452; // 'PNTR'
static constexpr uint32_t FEATURE_NET     = 0x574e4554; // 'WNET'

inline uint32_t read_word(address_space &space, uint32_t vaddr)
{
	return space.read_dword(vaddr & 0x1fffffff);
}

inline bool overlaps_discovery(uint32_t vaddr, uint32_t length)
{
	return vaddr < DISCOVERY_POINTER + 4 && vaddr + length > DISCOVERY_MAGIC;
}

inline bool ram_range_valid(address_space &space, uint32_t vaddr, uint32_t length)
{
	if (!length || (vaddr & 0xfc000003) != 0x80000000 || length > 0x04000000 || vaddr > 0x84000000 - length)
		return false;

	const offs_t physical = vaddr & 0x1fffffff;
	auto *const base = static_cast<uint8_t *>(space.get_write_ptr(physical));
	if (!base || space.get_read_ptr(physical) != base)
		return false;

	const offs_t native_mask = (space.data_width() / 8) - 1;
	const uint32_t step = (length <= 0x100) ? 4 : 0x1000;
	const uint32_t last = (length - 1) & ~3U;
	for (uint32_t offset = step; ; offset += step)
	{
		if (offset > last)
			offset = last;

		const offs_t displacement = ((physical + offset) & ~native_mask) - (physical & ~native_mask);
		if (space.get_write_ptr(physical + offset) != base + displacement || space.get_read_ptr(physical + offset) != base + displacement)
			return false;

		if (offset == last)
			break;
	}

	return true;
}

inline bool directory_range_valid(address_space &space, uint32_t vaddr, uint32_t length)
{
	if ((vaddr & 0xe0000003) != 0x80000000 || length > 0x20000000 || vaddr > 0xa0000000 - length)
		return false;

	for (uint32_t offset = 0; offset < length; offset += 4)
	{
		if (!space.get_read_ptr((vaddr + offset) & 0x1fffffff))
			return false;
	}

	return true;
}

inline uint32_t find_directory(address_space &space)
{
	if (read_word(space, DISCOVERY_MAGIC) != MAGIC)
		return 0;

	const uint32_t directory = read_word(space, DISCOVERY_POINTER);
	if (!directory_range_valid(space, directory, DIRECTORY_HEADER_SIZE) || read_word(space, directory) != MAGIC || read_word(space, directory + 4) != ((DIRECTORY_VERSION << 16) | DIRECTORY_HEADER_SIZE))
		return 0;

	return directory;
}

inline uint32_t find_feature(address_space &space, uint32_t feature)
{
	const uint32_t directory = find_directory(space);
	if (!directory)
		return 0;

	const uint32_t count = read_word(space, directory + 0x0c);
	if (!count || count > MAX_FEATURES || !directory_range_valid(space, directory, DIRECTORY_HEADER_SIZE + count * DIRECTORY_ENTRY_SIZE))
		return 0;

	for (uint32_t index = 0; index < count; index++)
	{
		const uint32_t entry = directory + DIRECTORY_HEADER_SIZE + index * DIRECTORY_ENTRY_SIZE;
		if (read_word(space, entry) == feature)
			return read_word(space, entry + 4);
	}

	return 0;
}

inline uint32_t directory_build(address_space &space)
{
	const uint32_t directory = find_directory(space);

	return directory ? read_word(space, directory + 8) : 0;
}

}

#endif // MAME_WEBTV_WTVFART_H
