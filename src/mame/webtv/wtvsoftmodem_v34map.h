// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34MAP_H
#define MAME_WEBTV_WTVSOFTMODEM_V34MAP_H
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace wtvsoftmodem::v34::map::parameters
{
	struct mapping_t
	{
		// ITU-T V.34 Table 8 Switching Pattern (SWP)
		uint8_t b;

		// ITU-T V.34 Table 10 Mapping parameters K, M and L at different data rates and symbol rates
		uint8_t m_min;
		uint8_t m_exp;
	};

	static constexpr mapping_t MAPPINGS[6][28] = {
		{
			{  8,  1,  1 }, {  9,  1,  1 }, { 16,  2,  2 }, { 17,  2,  2},
			{ 24,  3,  4 }, { 25,  4,  4 }, { 32,  6,  7 }, { 33,  7,  8},
			{ 40, 12, 14 }, { 41, 13, 15 }, { 48, 12, 14 }, { 49, 13, 15},
			{ 56, 12, 14 }, { 57, 13, 15 }, { 64, 12, 14 }, { 65, 13, 15},
			{ 72, 12, 14 }, { 73, 13, 15 }, {  0,  0,  0 }, {  0,  0,  0},
			{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0},
			{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0},
		},
		{
			{  0,  0,  0 }, {  0,  0,  0 }, { 14,  2,  2 }, { 15,  2,  2},
			{ 21,  3,  3 }, { 22,  3,  3 }, { 28,  4,  5 }, { 29,  5,  5},
			{ 35,  8,  9 }, { 36,  8, 10 }, { 42, 14, 17 }, { 43, 15, 18},
			{ 49, 13, 15 }, { 50, 14, 17 }, { 56, 12, 14 }, { 57, 13, 15},
			{ 63, 11, 13 }, { 64, 12, 14 }, { 70, 10, 12 }, { 71, 11, 13},
			{ 77,  9, 11 }, { 78, 10, 12 }, {  0,  0,  0 }, {  0,  0,  0},
			{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0},
		},
		{
			{  0,  0,  0 }, {  0,  0,  0 }, { 14,  2,  2 }, { 15,  2,  2},
			{ 21,  3,  3 }, { 22,  3,  3 }, { 28,  4,  5 }, { 28,  4,  5},
			{ 35,  8,  9 }, { 35,  8,  9 }, { 42, 14, 17 }, { 42, 14, 17},
			{ 48, 12, 14 }, { 49, 13, 15 }, { 55, 11, 13 }, { 56, 12, 14},
			{ 62, 10, 12 }, { 63, 11, 13 }, { 69,  9, 11 }, { 70, 10, 12},
			{ 76,  8, 10 }, { 76,  8, 10 }, {  0,  0,  0 }, {  0,  0,  0},
			{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0},
		},
		{
			{  0,  0,  0 }, {  0,  0,  0 }, { 13,  2,  2 }, { 14,  2,  2},
			{ 20,  2,  3 }, { 20,  2,  3 }, { 26,  4,  4 }, { 27,  4,  5},
			{ 32,  6,  7 }, { 33,  7,  8 }, { 39, 11, 13 }, { 39, 11, 13},
			{ 45,  9, 11 }, { 46, 10, 12 }, { 52,  8, 10 }, { 52,  8, 10},
			{ 58, 14, 17 }, { 59, 15, 18 }, { 64, 12, 14 }, { 65, 13, 15},
			{ 71, 11, 13 }, { 71, 11, 13 }, { 77,  9, 11 }, { 78, 10, 12},
			{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0},
		},
		{
			{  0,  0,  0 }, {  0,  0,  0 }, { 12,  1,  1 }, { 13,  2,  2},
			{ 18,  2,  2 }, { 19,  2,  2 }, { 24,  3,  4 }, { 25,  4,  4},
			{ 30,  5,  6 }, { 31,  6,  6 }, { 36,  8, 10 }, { 37,  9, 11},
			{ 42, 14, 17 }, { 43, 15, 18 }, { 48, 12, 14 }, { 49, 13, 15},
			{ 54, 10, 12 }, { 55, 11, 13 }, { 60,  8, 10 }, { 61,  9, 11},
			{ 66, 14, 17 }, { 67, 15, 18 }, { 72, 12, 14 }, { 73, 13, 15},
			{ 78, 10, 12 }, { 79, 11, 13 }, {  0,  0,  0 }, {  0,  0,  0},
		},
		{
			{  0,  0,  0 }, {  0,  0,  0 }, { 12,  1,  1 }, { 12,  1,  1},
			{ 17,  2,  2 }, { 18,  2,  2 }, { 23,  3,  3 }, { 23,  3,  3},
			{ 28,  4,  5 }, { 29,  5,  5 }, { 34,  7,  8 }, { 35,  8,  9},
			{ 40, 12, 14 }, { 40, 12, 14 }, { 45,  9, 11 }, { 46, 10, 12},
			{ 51, 15, 18 }, { 51, 15, 18 }, { 56, 12, 14 }, { 57, 13, 15},
			{ 62, 10, 12 }, { 63, 11, 13 }, { 68,  8, 10 }, { 68,  8, 10},
			{ 73, 13, 15 }, { 74, 14, 17 }, { 79, 11, 13 }, { 79, 11, 13},
		},
	};

	// ITU-T V.34 Table 7 Framing parameters
	static constexpr int32_t J[6] = {
		7, // @ symbol rate 2400 baud
		8, // @ symbol rate 2743 baud
		7, // @ symbol rate 2800 baud
		7, // @ symbol rate 3000 baud
		7, // @ symbol rate 3200 baud
		8, // @ symbol rate 3429 baud
	};
	static constexpr int32_t P[6] = {
		12, // @ symbol rate 2400 baud
		12, // @ symbol rate 2743 baud
		14, // @ symbol rate 2800 baud
		15, // @ symbol rate 3000 baud
		16, // @ symbol rate 3200 baud
		15, // @ symbol rate 3429 baud
	};

	struct derived_t
	{
		int32_t bit_rate;
		int32_t b;
		int32_t k;
		int32_t q;
		int32_t m;
		int32_t j;
		int32_t p;
		int32_t w;
		int32_t r;
	};

	inline derived_t derive(int32_t baud_rate_code, int32_t bit_rate_code, bool expanded_shaping)
	{
		const int32_t br = baud_rate_code & 7;
		const int32_t rc = bit_rate_code;

		derived_t out{};
		out.bit_rate = ((rc >> 1) + 1) * 2400 + (rc & 1) * 200;

		const mapping_t &mp = MAPPINGS[br < 6 ? br : 5][rc < 28 ? rc : 27];
		out.b = mp.b;

		if (out.b <= 12)
		{
			out.k = 0;
			out.q = 0;
		}
		else
		{
			out.k = out.b - 12;
			out.q = 0;
			while (out.k >= 32)
			{
				out.k -= 8;
				out.q++;
			}
		}

		out.m = expanded_shaping ? mp.m_exp : mp.m_min;
		out.j = J[br < 6 ? br : 5];
		out.p = P[br < 6 ? br : 5];
		out.w = (rc & 1) ? (15 - out.j) : 0;
		out.r = (out.bit_rate * 28) / (out.j * 100) - (out.b - 1) * out.p;

		return out;
	}

}

namespace wtvsoftmodem::v34::map::shell
{
	// These were taken from SpanDSP. They're described in the standard (ITU-T V.34 section 9.3).

	// 1 rings deals with up to 0 bits
	static constexpr uint32_t G2_1_RINGS[] = {
		1, 0,
	};
	static constexpr uint32_t G4_1_RINGS[] = {
		1, 0,
	};
	static constexpr uint32_t Z8_1_RINGS[] = {
		0x00000000, 0x00000001,
	};

	// 2 rings deals with up to 8 bits
	static constexpr uint32_t G2_2_RINGS[] = {
		1, 2, 1, 0, 0,
	};
	static constexpr uint32_t G4_2_RINGS[] = {
		1, 4, 6, 4, 1, 0, 0, 0, 0,
	};
	static constexpr uint32_t Z8_2_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x00000025, 0x0000005d, 0x000000a3, 0x000000db, 0x000000f7,
		0x000000ff,
	};

	// 3 rings deals with up to 12 bits
	static constexpr uint32_t G2_3_RINGS[] = {
		1, 2, 3, 2, 1, 0, 0, 0, 0,
	};
	static constexpr uint32_t G4_3_RINGS[] = {
		 1,  4, 10, 16, 19, 16, 10,  4,  1,  0,
		 0,
	};
	static constexpr uint32_t Z8_3_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x0000009d, 0x000001a7, 0x0000039f, 0x000006af,
		0x00000aa7, 0x00000efa, 0x000012f2,
	};

	// 4 rings deals with up to 16 bits
	static constexpr uint32_t G2_4_RINGS[] = {
		1, 2, 3, 4, 3, 2, 1, 0, 0, 0,
		0, 0, 0,
	};
	static constexpr uint32_t G4_4_RINGS[] = {
		 1,  4, 10, 20, 31, 40, 44, 40, 31, 20,
		10,  4,  1,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,
	};
	static constexpr uint32_t Z8_4_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001e7, 0x000004bf, 0x00000a53,
		0x000013fb, 0x000022ea, 0x000037ba, 0x00005202, 0x00007032, 0x00008fce, 0x0000adfe, 0x0000c846,
		0x0000dd16, 0x0000ec05, 0x0000f5ad, 0x0000fb41, 0x0000fe19, 0x0000ff5b, 0x0000ffd3, 0x0000fff7,
		0x0000ffff,
	};

	// 5 rings deals with up to 18 bits
	static constexpr uint32_t G2_5_RINGS[] = {
		1, 2, 3, 4, 5, 4, 3, 2, 1, 0,
		0, 0, 0, 0, 0, 0, 0,
	};
	static constexpr uint32_t G4_5_RINGS[] = {
		 1,  4, 10, 20, 35, 52, 68, 80, 85, 80,
		68, 52, 35, 20, 10,  4,  1,  0,  0,  0,
	};
	static constexpr uint32_t Z8_5_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x000004ff, 0x00000b73,
		0x000017bb, 0x00002d1e, 0x00004f7e, 0x000082d2, 0x0000ca62, 0x000127e6, 0x00019abe, 0x00021f8e,
		0x0002b066, 0x0003457b, 0x0003d653, 0x00045b23,
	};

	// 6 rings deals with up to 20 bits
	static constexpr uint32_t G2_6_RINGS[] = {
		1, 2, 3, 4, 5, 6, 5, 4, 3, 2,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0,
	};
	static constexpr uint32_t G4_6_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  80, 104, 125, 140,
		146, 140, 125, 104,  80,  56,  35,  20,  10,   4,
		  1,   0,   0,   0,
	};
	static constexpr uint32_t Z8_6_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bb3,
		0x000018db, 0x000030de, 0x000059ce, 0x00009b76, 0x0000ff06, 0x00018e56, 0x000252c6, 0x000353d6,
		0x000495a6, 0x000617a3, 0x0007d3bb, 0x0009be4f, 0x000bc6f7, 0x000dda09, 0x000fe2b1, 0x0011cd45,
	};

	// 7 rings deals with up to 22 bits
	static constexpr uint32_t G2_7_RINGS[] = {
		1, 2, 3, 4, 5, 6, 7, 6, 5, 4,
		3, 2, 1, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	};
	static constexpr uint32_t G4_7_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  84, 116, 149, 180,
		206, 224, 231, 224, 206, 180, 149, 116,  84,  56,
		 35,  20,  10,   4,   1,   0,   0,   0,   0,
	};
	static constexpr uint32_t Z8_7_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x0000191b, 0x000031fe, 0x00005d8e, 0x0000a5c6, 0x000117c6, 0x0001c3da, 0x0002bd0a, 0x0004181e,
		0x0005ea16, 0x0008462b, 0x000b3b83, 0x000ed2d7, 0x00130c5f, 0x0017de6d, 0x001d34fd, 0x0022f25d,
		0x0028f0ed, 0x002f05d4, 0x00350464, 0x003ac1c4, 0x00401854,
	};

	// 8 rings deals with up to 24 bits
	static constexpr uint32_t G2_8_RINGS[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 7, 6,
		5, 4, 3, 2, 1, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	static constexpr uint32_t G4_8_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  84, 120, 161, 204,
		246, 284, 315, 336, 344, 336, 315, 284, 246, 204,
		161, 120,  84,  56,  35,  20,  10,   4,   1,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,
	};
	static constexpr uint32_t Z8_8_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x0000323e, 0x00005eae, 0x0000a986, 0x00012216, 0x0001dc9a, 0x0002f2aa, 0x00048342,
		0x0006b232, 0x0009a6db, 0x000d8a33, 0x0012841f, 0x0018b847, 0x0020429d, 0x002933e5, 0x00338ea1,
		0x003f44d9, 0x004c3714, 0x005a34b4, 0x0068fdc4, 0x00784624, 0x0087b9dc, 0x0097023c, 0x00a5cb4c,
		0x00b3c8ec, 0x00c0bb27, 0x00cc715f, 0x00d6cc1b, 0x00dfbd63, 0x00e747b9, 0x00ed7be1, 0x00f275cd,
		0x00f65925, 0x00f94dce, 0x00fb7cbe, 0x00fd0d56, 0x00fe2366, 0x00feddea, 0x00ff567a, 0x00ffa152,
		0x00ffcdc2, 0x00ffe6dd, 0x00fff445, 0x00fffaf9, 0x00fffe11, 0x00ffff5b, 0x00ffffd3, 0x00fffff7,
		0x00ffffff,
	};

	// 9 rings deals with up to 25 bits
	static constexpr uint32_t G2_9_RINGS[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 8,
		7, 6, 5, 4, 3, 2, 1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0,
	};
	static constexpr uint32_t G4_9_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  84, 120, 165, 216,
		270, 324, 375, 420, 456, 480, 489, 480, 456, 420,
		375, 324, 270, 216, 165, 120,  84,  56,  35,  20,
		 10,   4,   1,   0,   0,   0,   0,   0,   0,   0,
	};
	static constexpr uint32_t Z8_9_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005eee, 0x0000aaa6, 0x000125d6, 0x0001e6ea, 0x00030b6a, 0x0004b8e2,
		0x00071d72, 0x000a6fd7, 0x000eeeb7, 0x0014df0f, 0x001c89b7, 0x00263805, 0x00322fad, 0x0040ae11,
		0x0051e349, 0x0065ed40, 0x007cd358, 0x0096831c, 0x00b2ce64, 0x00d16b2c, 0x00f1f53c, 0x0113f19c,
		0x0136d3ac, 0x015a0395, 0x017ce5a5, 0x019ee205, 0x01bf6c15, 0x01de08dd, 0x01fa5425, 0x021403e9,
	};

	// 10 rings deals with up to 26 bits
	static constexpr uint32_t G2_10_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_10_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  84, 120, 165, 220,
		282, 348, 415, 480, 540, 592, 633, 660, 670, 660,
		633, 592, 540, 480, 415, 348, 282, 220, 165, 120,
		 84,  56,  35,  20,  10,   4,   1,   0,   0,   0,
		  0,   0,
	};
	static constexpr uint32_t Z8_10_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aae6, 0x000126f6, 0x0001eaaa, 0x000315ba, 0x0004d1a2,
		0x00075312, 0x000adb17, 0x000fb7cf, 0x00164473, 0x001ee87b, 0x002a15b5, 0x0038453d, 0x0049f359,
		0x005f9a51, 0x0079ac70, 0x00988d70, 0x00bc8ba8, 0x00e5d968, 0x01148704, 0x01487e24, 0x01817ecc,
		0x01bf1e6c, 0x0200c925, 0x0245c54d, 0x028d3919, 0x02d63231, 0x031faecf, 0x0368a7e7, 0x03b01bb3,
		0x03f517db, 0x0436c294,
	};

	// 11 rings deals with up to 27 bits
	static constexpr uint32_t G2_11_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 10,  9,  8,  7,  6,  5,  4,  3,  2,
		 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,
	};
	static constexpr uint32_t G4_11_RINGS[] = {
		  1,   4,  10,  20,  35,  56,  84, 120, 165, 220,
		286, 360, 439, 520, 600, 676, 745, 804, 850, 880,
		891, 880, 850, 804, 745, 676, 600, 520, 439, 360,
		286, 220, 165, 120,  84,  56,  35,  20,  10,   4,
		  1,   0,   0,   0,   0,
	};
	static constexpr uint32_t Z8_11_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x00012736, 0x0001ebca, 0x0003197a, 0x0004dbf2,
		0x00076bd2, 0x000b10b7, 0x0010230f, 0x00170d8b, 0x00204dfb, 0x002c7559, 0x003c26c1, 0x00501529,
		0x0068ffc1, 0x0087acf8, 0x00ace438, 0x00d96680, 0x010de618, 0x014afdb0, 0x01912750, 0x01e0b394,
		0x0239c1c4, 0x029c396d, 0x0307c5fd, 0x037bd4c1, 0x03f79581, 0x0479fdd3, 0x0501cf1b, 0x058d9f0b,
		0x061be253, 0x06aaf90e, 0x07393c56, 0x07c50c46, 0x084cdd8e,
	};

	// 12 rings deals with up to 28 bits
	static constexpr uint32_t G2_12_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 11, 10,  9,  8,  7,  6,  5,  4,
		 3,  2,  1,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_12_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  451,  544,  640,  736,  829,  916,  994, 1060,
		1111, 1144, 1156, 1144, 1111, 1060,  994,  916,  829,  736,
		 640,  544,  451,  364,  286,  220,  165,  120,   84,   56,
		  35,   20,   10,    4,    1,    0,    0,    0,    0,
	};
	static constexpr uint32_t Z8_12_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec0a, 0x00031a9a, 0x0004dfb2,
		0x00077622, 0x000b2977, 0x001058af, 0x001778cb, 0x00211713, 0x002ddad9, 0x003e8681, 0x0053f78d,
		0x006f2565, 0x00911ea8, 0x00bb04e8, 0x00ee06c8, 0x012b5888, 0x01742b20, 0x01c9a220, 0x022cc8a0,
		0x029e85a0, 0x031f904d, 0x03b064b5, 0x04513989, 0x0501f7a1, 0x05c233d3, 0x06912b8b, 0x076dc46f,
		0x08568f37, 0x0949cdbe, 0x0a457c2e, 0x0b475d06, 0x0c4d0796, 0x0d53f86a, 0x0e59a2fa, 0x0f5b83d2,
		0x10573242,
	};

	// 13 rings deals with up to 29 bits
	static constexpr uint32_t G2_13_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 12, 11, 10,  9,  8,  7,  6,
		 5,  4,  3,  2,  1,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_13_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  455,  556,  664,  776,  889, 1000, 1106, 1204,
		1291, 1364, 1420, 1456, 1469, 1456, 1420, 1364, 1291, 1204,
		1106, 1000,  889,  776,  664,  556,  455,  364,  286,  220,
		 165,  120,   84,   56,   35,   20,   10,    4,    1,    0,
		   0,    0,    0,    0,
	};
	static constexpr uint32_t Z8_13_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec12, 0x00031ada, 0x0004e0d2,
		0x000779e2, 0x000b33c7, 0x0010716f, 0x0017ae6b, 0x00218253, 0x002ea3f1, 0x003fec01, 0x0056574d,
		0x007307e5, 0x0097452c, 0x00c47a6c, 0x00fc33b8, 0x014018f8, 0x0191e710, 0x01f36728, 0x02666430,
		0x02ec9ed0, 0x0387c009, 0x04394ae1, 0x05028d89, 0x05e49281, 0x06e01253, 0x07f56693, 0x09247ee7,
		0x0a6cd8b7, 0x0bcd7a02, 0x0d44efb2, 0x0ed14fa6, 0x10703e86, 0x121ef952, 0x13da627a, 0x159f122a,
		0x17696952, 0x1935a6cf, 0x1afffdf7, 0x1cc4ada7, 0x1e8016cf, 0x202ed19b,
	};

	// 14 rings deals with up to 30 bits
	static constexpr uint32_t G2_14_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 14, 13, 12, 11, 10,  9,  8,
		 7,  6,  5,  4,  3,  2,  1,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,
	};
	static constexpr uint32_t G4_14_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  455,  560,  676,  800,  929, 1060, 1190, 1316,
		1435, 1544, 1640, 1720, 1781, 1820, 1834, 1820, 1781, 1720,
		1640, 1544, 1435, 1316, 1190, 1060,  929,  800,  676,  560,
		 455,  364,  286,  220,  165,  120,   84,   56,   35,   20,
		  10,    4,    1,    0,    0,    0,    0,    0,    0,    0,
		   0,
	};
	static constexpr uint32_t Z8_14_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec12, 0x00031ae2, 0x0004e112,
		0x00077b02, 0x000b3787, 0x00107bbf, 0x0017c72b, 0x0021b7f3, 0x002f0f31, 0x0040b519, 0x0057bccd,
		0x007567a5, 0x009b27ac, 0x00caa10c, 0x0105aa1c, 0x014e49bc, 0x01a6b3c0, 0x02114340, 0x029072b8,
		0x0326d1f8, 0x03d6f9f9, 0x04a37ec1, 0x058edf95, 0x069b75cd, 0x07cb62b3, 0x09207ceb, 0x0a9c3df7,
		0x0c3fb07f, 0x0e0b601a, 0x0fff4b6a, 0x121ad93a, 0x145cd12a, 0x16c35852, 0x194bf222, 0x1bf385a2,
		0x1eb66712, 0x219065c7, 0x247cddff, 0x2776ce43, 0x2a78efcb, 0x2d7dd135, 0x307ff2bd, 0x3379e301,
		0x36665b39, 0x394059ee, 0x3c033b5e, 0x3eaacede, 0x413368ae,
	};

	// 15 rings deals with up to 31 bits
	static constexpr uint32_t G2_15_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 14, 15, 14, 13, 12, 11, 10,
		 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_15_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  455,  560,  680,  812,  953, 1100, 1250, 1400,
		1547, 1688, 1820, 1940, 2045, 2132, 2198, 2240, 2255, 2240,
		2198, 2132, 2045, 1940, 1820, 1688, 1547, 1400, 1250, 1100,
		 953,  812,  680,  560,  455,  364,  286,  220,  165,  120,
		  84,   56,   35,   20,   10,    4,    1,    0,    0,    0,
		   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
	};
	static constexpr uint32_t Z8_15_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec12, 0x00031ae2, 0x0004e11a,
		0x00077b42, 0x000b38a7, 0x00107f7f, 0x0017d17b, 0x0021d0b3, 0x002f44d1, 0x00412059, 0x005885e5,
		0x0076cd25, 0x009d876c, 0x00ce838c, 0x010bd0bc, 0x0157c03c, 0x01b4e564, 0x022613c4, 0x02ae5b10,
		0x035100a8, 0x041176a1, 0x04f35049, 0x05fa3435, 0x0729cbfd, 0x0885b1df, 0x0a115c97, 0x0bd009cf,
		0x0dc4a79f, 0x0ff1bdaa, 0x1259567a, 0x14fce9d2, 0x17dd48c2, 0x1afa8c5e, 0x1e5407d6, 0x21e83e8a,
		0x25b4de92, 0x29b6c00f, 0x2de9e977, 0x324998eb, 0x36d05283, 0x3b77f359, 0x4039c8f9, 0x450eacb9,
		0x49ef2259, 0x4ed37928, 0x53b3eec8, 0x5888d288, 0x5d4aa828, 0x61f248fe, 0x66790296, 0x6ad8b20a,
		0x6f0bdb72, 0x730dbcef, 0x76da5cf7, 0x7a6e93ab, 0x7dc80f23, 0x80e552bf,
	};

	// 17 rings deals with up to 30 bits
	static constexpr uint32_t G2_17_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 14, 15, 16, 17, 16, 15, 14,
		13, 12, 11, 10,  9,  8,  7,  6,  5,  4,
		 3,  2,  1,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_17_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  455,  560,  680,  816,  969, 1136, 1314, 1500,
		1691, 1884, 2076, 2264, 2445, 2616, 2774, 2916, 3039, 3140,
		3216, 3264, 3281, 3264, 3216, 3140, 3039, 2916, 2774, 2616,
		2445, 2264, 2076, 1884, 1691, 1500, 1314, 1136,  969,  816,
		 680,  560,
	};
	static constexpr uint32_t Z8_17_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec12, 0x00031ae2, 0x0004e11a,
		0x00077b4a, 0x000b38ef, 0x001080df, 0x0017d65b, 0x0021dec3, 0x002f67e1, 0x00416eb9, 0x005926c5,
		0x0078017d, 0x009fb604, 0x00d248cc, 0x011212fc, 0x0161c95c, 0x01c48284, 0x023dbc04, 0x02d15e34,
		0x0383be54, 0x04599e9d, 0x05582bdd, 0x0684f84d, 0x07e5f365, 0x09815e87, 0x0b5dbe6f, 0x0d81c96b,
		0x0ff45273, 0x12bc314e, 0x15e02806, 0x1966c602, 0x1d564932, 0x21b47dce, 0x26869d3e, 0x2bd12cd6,
		0x3197dd26, 0x37dd6ab3, 0x3ea38103, 0x45eaa0fb,
	};

	// 18 rings deals with up to 31 bits
	static constexpr uint32_t G2_18_RINGS[] = {
		 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 17, 16,
		15, 14, 13, 12, 11, 10,  9,  8,  7,  6,
		 5,  4,  3,  2,  1,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		 0,  0,  0,  0,  0,  0,  0,  0,  0,
	};
	static constexpr uint32_t G4_18_RINGS[] = {
		   1,    4,   10,   20,   35,   56,   84,  120,  165,  220,
		 286,  364,  455,  560,  680,  816,  969, 1140, 1326, 1524,
		1731, 1944, 2160, 2376, 2589, 2796, 2994, 3180, 3351, 3504,
		3636, 3744, 3825, 3876, 3894, 3876, 3825, 3744, 3636, 3504,
		3351, 3180, 2994, 2796, 2589, 2376, 2160, 1944, 1731, 1524,
		1326, 1140,  969,  816,  680,  560,  455,
	};
	static constexpr uint32_t Z8_18_RINGS[] = {
		0x00000000, 0x00000001, 0x00000009, 0x0000002d, 0x000000a5, 0x000001ef, 0x00000507, 0x00000bbb,
		0x00001923, 0x00003246, 0x00005ef6, 0x0000aaee, 0x0001273e, 0x0001ec12, 0x00031ae2, 0x0004e11a,
		0x00077b4a, 0x000b38ef, 0x001080e7, 0x0017d69b, 0x0021dfe3, 0x002f6ba1, 0x00417909, 0x00593f85,
		0x0078371d, 0x00a02144, 0x00d311e4, 0x0113787c, 0x0164291c, 0x01c86504, 0x0243e2a4, 0x02dad4b4,
		0x0391f014, 0x046e701d, 0x05761905, 0x06af37f1, 0x0820a049, 0x09d1a5f7, 0x0bca144f, 0x0e122173,
		0x10b25e1b, 0x13b3a1be, 0x171ef32e, 0x1afd6dce, 0x1f58239e, 0x2437fc6e, 0x29a5929e, 0x2fa90de6,
		0x3649fcb6, 0x3d8f2cd3, 0x457e83eb, 0x4e1cd8ef, 0x576dcf17, 0x6173b389, 0x6c2f5eb1, 0x77a01a35,
		0x83c38c4d,
	};

	static constexpr const uint32_t *G2S[] = {
		nullptr,      G2_1_RINGS,   G2_2_RINGS,   G2_3_RINGS,   G2_4_RINGS,
		G2_5_RINGS,   G2_6_RINGS,   G2_7_RINGS,   G2_8_RINGS,   G2_9_RINGS,
		G2_10_RINGS,  G2_11_RINGS,  G2_12_RINGS,  G2_13_RINGS,  G2_14_RINGS,
		G2_15_RINGS,  nullptr,      G2_17_RINGS,  G2_18_RINGS,
	};
	static constexpr const uint32_t *G4S[] = {
		nullptr,      G4_1_RINGS,   G4_2_RINGS,   G4_3_RINGS,   G4_4_RINGS,
		G4_5_RINGS,   G4_6_RINGS,   G4_7_RINGS,   G4_8_RINGS,   G4_9_RINGS,
		G4_10_RINGS,  G4_11_RINGS,  G4_12_RINGS,  G4_13_RINGS,  G4_14_RINGS,
		G4_15_RINGS,  nullptr,      G4_17_RINGS,  G4_18_RINGS,
	};
	static constexpr const uint32_t *Z8S[] = {
		nullptr,      Z8_1_RINGS,   Z8_2_RINGS,   Z8_3_RINGS,   Z8_4_RINGS,
		Z8_5_RINGS,   Z8_6_RINGS,   Z8_7_RINGS,   Z8_8_RINGS,   Z8_9_RINGS,
		Z8_10_RINGS,  Z8_11_RINGS,  Z8_12_RINGS,  Z8_13_RINGS,  Z8_14_RINGS,
		Z8_15_RINGS,  nullptr,      Z8_17_RINGS,  Z8_18_RINGS,
	};

	struct table_limits
	{
		size_t g2;
		size_t g4;
		size_t z8;
	};

	static constexpr table_limits LIMITS[] = {
		{ 0,                      0,                      0                      },
		{ std::size(G2_1_RINGS),  std::size(G4_1_RINGS),  std::size(Z8_1_RINGS)  },
		{ std::size(G2_2_RINGS),  std::size(G4_2_RINGS),  std::size(Z8_2_RINGS)  },
		{ std::size(G2_3_RINGS),  std::size(G4_3_RINGS),  std::size(Z8_3_RINGS)  },
		{ std::size(G2_4_RINGS),  std::size(G4_4_RINGS),  std::size(Z8_4_RINGS)  },
		{ std::size(G2_5_RINGS),  std::size(G4_5_RINGS),  std::size(Z8_5_RINGS)  },
		{ std::size(G2_6_RINGS),  std::size(G4_6_RINGS),  std::size(Z8_6_RINGS)  },
		{ std::size(G2_7_RINGS),  std::size(G4_7_RINGS),  std::size(Z8_7_RINGS)  },
		{ std::size(G2_8_RINGS),  std::size(G4_8_RINGS),  std::size(Z8_8_RINGS)  },
		{ std::size(G2_9_RINGS),  std::size(G4_9_RINGS),  std::size(Z8_9_RINGS)  },
		{ std::size(G2_10_RINGS), std::size(G4_10_RINGS), std::size(Z8_10_RINGS) },
		{ std::size(G2_11_RINGS), std::size(G4_11_RINGS), std::size(Z8_11_RINGS) },
		{ std::size(G2_12_RINGS), std::size(G4_12_RINGS), std::size(Z8_12_RINGS) },
		{ std::size(G2_13_RINGS), std::size(G4_13_RINGS), std::size(Z8_13_RINGS) },
		{ std::size(G2_14_RINGS), std::size(G4_14_RINGS), std::size(Z8_14_RINGS) },
		{ std::size(G2_15_RINGS), std::size(G4_15_RINGS), std::size(Z8_15_RINGS) },
		{ 0,                      0,                      0                      },
		{ std::size(G2_17_RINGS), std::size(G4_17_RINGS), std::size(Z8_17_RINGS) },
		{ std::size(G2_18_RINGS), std::size(G4_18_RINGS), std::size(Z8_18_RINGS) },
	};
	static_assert(
		   std::size(LIMITS) == std::size(G2S)
		&& std::size(LIMITS) == std::size(G4S)
		&& std::size(LIMITS) == std::size(Z8S)
	);

}

#endif // MAME_WEBTV_WTVSOFTMODEM_V34MAP_H
