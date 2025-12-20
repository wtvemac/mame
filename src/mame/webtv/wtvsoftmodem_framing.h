// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_WTVSOFTMODEM_FRAMING_H
#define MAME_MACHINE_WTVSOFTMODEM_FRAMING_H

#pragma once

namespace frame8n1 {

	static constexpr uint32_t BYTE_SYNC_MASK        = 0x201;
	static constexpr uint32_t BYTE_SYNC_BE_VALUE    = 0x001;
	static constexpr uint32_t BYTE_SYNC_LE_VALUE    = 0x200;

	static constexpr int8_t   FRAME_BIT_COUNT       = 10;
	static constexpr int8_t   FRAME_BIT_LAST_INDEX  = (FRAME_BIT_COUNT - 1);

	static constexpr int8_t   START_BIT_BUILD_INDEX = -1;
	static constexpr bool     START_BIT_VALUE       =  0;

	static constexpr int8_t   STOP_BIT_BUILD_INDEX  =  8;
	static constexpr bool     STOP_BIT_VALUE        =  1;

	typedef struct parser_state {
		uint16_t byte_bitmask;
		uint16_t partial_byte;
		int8_t byte_bit_index;
		uint8_t byte;
	} parser_state_t;

	inline void parse_start(parser_state_t* state)
	{
		state->byte_bitmask   = 0x000;
		state->partial_byte   = 0x000;
		state->byte_bit_index = 0;
	}

	inline bool parse_frame(parser_state_t* state, uint32_t frame_sync_value)
	{
		bool hit_end = false;

		if ((state->partial_byte & BYTE_SYNC_MASK) == frame_sync_value)
		{
			state->byte = (state->partial_byte >> 1);

			frame8n1::parse_start(state);

			hit_end = true;
		}

		return hit_end;
	}

	inline bool parse_be_frame_bit(parser_state_t* state, bool bit)
	{
		bool hit_end = false;

		if (bit)
			state->partial_byte |=  (1 << (FRAME_BIT_LAST_INDEX - state->byte_bit_index));
		else
			state->partial_byte &= ~(1 << (FRAME_BIT_LAST_INDEX - state->byte_bit_index));
		state->byte_bit_index++;

		if (state->byte_bit_index == FRAME_BIT_COUNT)
		{
			hit_end = parse_frame(state, BYTE_SYNC_BE_VALUE);

			if (!hit_end)
			{
				state->partial_byte <<= 1;
				state->byte_bit_index = FRAME_BIT_LAST_INDEX;
			}
		}

		return hit_end;
	}

	inline bool parse_le_frame_bit(parser_state_t* state, bool bit)
	{
		bool hit_end = false;

		if (bit)
			state->partial_byte |=  (1 << state->byte_bit_index);
		else
			state->partial_byte &= ~(1 << state->byte_bit_index);
		state->byte_bit_index++;

		if (state->byte_bit_index == FRAME_BIT_COUNT)
		{
			hit_end = parse_frame(state, BYTE_SYNC_LE_VALUE);

			if (!hit_end)
			{
				state->partial_byte >>= 1;
				state->byte_bit_index = FRAME_BIT_LAST_INDEX;
			}
		}

		return hit_end;
	}

	typedef struct builder_state {
		int8_t build_index;
		uint16_t byte;
		bool hit_end;
	} builder_state_t;

	inline void build_frame_for_byte(builder_state_t* state, uint8_t byte)
	{
		state->byte = byte;
		state->build_index = START_BIT_BUILD_INDEX;
		state->hit_end = false;
	}

	inline bool builer_on_sync_bit(builder_state_t* state, bool* bit_value)
	{
		bool is_on_sync_bit = false;

		if(state->build_index == START_BIT_BUILD_INDEX)
		{
			is_on_sync_bit = true;
			
			*bit_value = START_BIT_VALUE;

			state->build_index++;
		}
		else if(state->build_index >= STOP_BIT_BUILD_INDEX)
		{
			is_on_sync_bit = true;

			*bit_value = STOP_BIT_VALUE;

			state->hit_end = true;

			state->build_index = START_BIT_BUILD_INDEX;
		}

		return is_on_sync_bit;
	}

	inline bool build_be_frame_bit(builder_state_t* state)
	{
		bool bit;

		if (!builer_on_sync_bit(state, &bit))
		{
			bit = (state->byte >> (7 - state->build_index)) & 0x1;

			state->build_index++;
		}

		return bit;
	}

	inline bool build_le_frame_bit(builder_state_t* state)
	{
		bool bit;

		if (!builer_on_sync_bit(state, &bit))
		{
			bit = (state->byte >> state->build_index) & 0x1;

			state->build_index++;
		}

		return bit;
	}
}

#endif // MAME_MACHINE_WTVSOFTMODEM_FRAMING_H
