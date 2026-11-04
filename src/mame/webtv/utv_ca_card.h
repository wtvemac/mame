// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_UTV_CA_CARD_H
#define MAME_WEBTV_UTV_CA_CARD_H

#pragma once

#include <array>
#include <cstdint>
#include <bit>

class utv_ca_card
{
public:

	static constexpr uint32_t SUBSCRIPTION_ID         = 0x4d414d45; // "MAME"
	static constexpr uint8_t  SUBSCRIPTION_SELECTOR   = 1;
	static constexpr uint32_t CARD_SERIAL             = 0x02000101;
	static constexpr uint8_t  ATR[]                   = { 0x3f, 0x30, 0x11, 0x05 };
	static constexpr uint32_t REPLY_SIZE              = 512;
	static constexpr uint32_t COMMAND_HEADER_SIZE     = 5;
	static constexpr uint32_t MAX_PPS_SIZE            = 6;
	static constexpr uint32_t PPS_BASE_SIZE           = 3;
	static constexpr uint32_t PPS_FORMAT_OFFSET       = 1;
	static constexpr uint32_t PPS_TIMING_OFFSET       = 2;
	static constexpr uint8_t  GUIDE_TYPE_SUBSCRIPTION = 4;
	static constexpr uint8_t  GUIDE_TAG_SUBSCRIPTION  = 0x83;
	static constexpr uint32_t ID_BYTES                = 4;
	static constexpr uint32_t ID_TABLE_BYTES          = 16;
	static constexpr uint32_t DESCRIPTOR_SIZE         = 127;
	static constexpr uint32_t DESCRIPTOR_BLOCK_OFFSET = 18;
	// Class/word length, type 4, zero, payload length, slot-zero selector.
	static constexpr uint8_t SELECTOR_BLOCK[]         = { 0xc2, 4, 0, 1, SUBSCRIPTION_SELECTOR };
	static constexpr uint32_t SERIAL_OFFSET           = 1;

	enum guide_offset : uint32_t
	{
		GUIDE_TYPE_LOW   = 1,
		GUIDE_LENGTH     = 4,
		GUIDE_TAG        = 5,
		GUIDE_ID         = 6,
		GUIDE_DESCRIPTOR = GUIDE_ID + ID_BYTES
	};

	enum header_offset : uint32_t
	{
		HEADER_CLASS  = 0,
		HEADER_INS    = 1,
		HEADER_P1     = 2,
		HEADER_P2     = 3,
		HEADER_LENGTH = 4
	};

	enum instruction : uint8_t
	{
		INS_SUBSCRIPTION_STATUS = 0x30,
		INS_SELECT_SUBSCRIPTION = 0x46,
		INS_WRITE_RECEIVER      = 0x4c,
		INS_IDENTITY            = 0x58,
		INS_EVENT_QUEUE         = 0x5c
	};

	enum protocol_value : uint8_t
	{
		COMMAND_CLASS              = 0x48,
		QUERY_ALTERNATE_MODE       = 0x20,
		QUERY_SUBSCRIPTION         = 1,
		VS_AUTHORIZED              = 0x10,
		LAST_EVENT_QUEUE           = 5,
		IDENTITY_SIZE              = 23,
		RECEIVER_DATA_SIZE         = 9,
		SELECTOR_SIZE              = 1,
		SUBSCRIPTION_STATUS_SIZE   = 12,
		EVENT_SIZE                 = 4,
		PPS_START                  = 0xff,
		PPS_PARAMETER_FLAGS        = 0x70,
		PPS_HAS_PPS1               = 0x10,
		PPS_HAS_PPS2               = 0x20,
		PPS_HAS_PPS3               = 0x40,
		PPS_UNSUPPORTED_PARAMETERS = 0xef,
		PPS_DEFAULT_FI_DI          = 0x11
	};

	enum status_word : uint16_t
	{
		STATUS_OK                 = 0x9000,
		STATUS_UNKNOWN_SELECTOR   = 0x6a88,
		STATUS_UNSUPPORTED_INS    = 0x6d00,
		STATUS_INVALID_PARAMETERS = 0x6a86,
		STATUS_INVALID_LENGTH     = 0x6700,
		STATUS_NOT_SELECTED       = 0x6985
	};

	uint8_t m_reply[REPLY_SIZE] {};
	uint8_t m_header[MAX_PPS_SIZE] {};
	uint32_t m_reply_head = 0;
	uint32_t m_reply_count = 0;
	uint32_t m_header_count = 0;
	uint32_t m_payload_count = 0;
	uint32_t m_payload_expected = 0;
	uint8_t m_last_ins = 0;
	bool m_pps = false;
	bool m_commands_started = false;
	bool m_subscription_selected = false;

	static constexpr std::array<uint8_t, DESCRIPTOR_SIZE> descriptor()
	{
		std::array<uint8_t, DESCRIPTOR_SIZE> data{};

		for (uint32_t i = 0; i < ID_BYTES; i++)
			data[i] = SUBSCRIPTION_ID >> ((ID_BYTES - 1 - i) * 8);

		// unused IDs
		for (uint32_t i = ID_BYTES; i < ID_TABLE_BYTES; i++)
			data[i] = 0xff;

		for (uint32_t i = 0; i < sizeof(SELECTOR_BLOCK); i++)
			data[DESCRIPTOR_BLOCK_OFFSET + i] = SELECTOR_BLOCK[i];

		return data;
	}

	static constexpr std::array<uint8_t, GUIDE_DESCRIPTOR + DESCRIPTOR_SIZE> guide_object()
	{
		std::array<uint8_t, GUIDE_DESCRIPTOR + DESCRIPTOR_SIZE> data{};

		data[GUIDE_TYPE_LOW] = GUIDE_TYPE_SUBSCRIPTION;
		data[GUIDE_LENGTH] = 1 + ID_BYTES + DESCRIPTOR_SIZE;
		data[GUIDE_TAG] = GUIDE_TAG_SUBSCRIPTION;

		for (uint32_t i = 0; i < ID_BYTES; i++)
			data[GUIDE_ID + i] = SUBSCRIPTION_ID >> ((ID_BYTES - 1 - i) * 8);

		const auto definition = descriptor();
		for (uint32_t i = 0; i < definition.size(); i++)
			data[GUIDE_DESCRIPTOR + i] = definition[i];

		return data;
	}


	void clear()
	{
		*this = utv_ca_card{};
	}

	void start()
	{
		clear();

		for (auto byte : ATR)
			queue(byte);
	}

	bool queue(uint8_t byte)
	{
		if (m_reply_count == REPLY_SIZE)
			return false;

		m_reply[(m_reply_head + m_reply_count++) % REPLY_SIZE] = byte;

		return true;
	}

	bool take(uint8_t &byte)
	{
		if (!m_reply_count)
			return false;

		byte = m_reply[m_reply_head];
		m_reply_head = (m_reply_head + 1) % REPLY_SIZE;
		m_reply_count--;

		return true;
	}

	void status(status_word word)
	{
		queue(word >> 8);
		queue(word & 0xff);
	}

	void write(uint8_t byte)
	{
		if (m_payload_expected)
		{
			write_payload(byte);
		}
		else
		{
			if (!m_header_count && !m_commands_started && byte == PPS_START)
				m_pps = true;

			m_header[m_header_count++] = byte;

			if (m_pps)
			{
				process_pps();
			}
			else if (m_header_count == COMMAND_HEADER_SIZE)
			{
				m_header_count = 0;
				m_commands_started = true;
				execute_command();
			}
		}
	}

private:

	void write_payload(uint8_t byte)
	{
		m_payload_count++;
		if (m_payload_count != m_payload_expected)
			return;

		if (m_last_ins == INS_SELECT_SUBSCRIPTION)
		{
			m_subscription_selected = byte == SUBSCRIPTION_SELECTOR;
			status(m_subscription_selected ? STATUS_OK : STATUS_UNKNOWN_SELECTOR);
		}
		else
		{
			status(STATUS_OK);
		}

		m_payload_count = m_payload_expected = 0;
	}

	void process_pps()
	{
		if (m_header_count <= PPS_FORMAT_OFFSET)
			return;

		const uint8_t flags = m_header[PPS_FORMAT_OFFSET] & PPS_PARAMETER_FLAGS;
		const uint32_t length = PPS_BASE_SIZE + std::popcount(uint8_t(flags & (PPS_HAS_PPS1 | PPS_HAS_PPS2 | PPS_HAS_PPS3)));
		if (m_header_count < length)
			return;

		uint8_t checksum = 0;
		for (uint32_t i = 0; i < length; i++)
			checksum ^= m_header[i];

		if (!checksum && !(m_header[PPS_FORMAT_OFFSET] & PPS_UNSUPPORTED_PARAMETERS) && (!(flags & PPS_HAS_PPS1) || m_header[PPS_TIMING_OFFSET] == PPS_DEFAULT_FI_DI))
		{
			for (uint32_t i = 0; i < length; i++)
				queue(m_header[i]);
		}

		m_header_count = 0;
		m_pps = false;
	}

	void execute_command()
	{
		const uint8_t mask = m_header[HEADER_CLASS] ^ COMMAND_CLASS;
		m_last_ins = m_header[HEADER_INS] ^ mask;

		uint32_t length;
		switch (m_last_ins)
		{
			case INS_IDENTITY:
				length = IDENTITY_SIZE;
				break;

			case INS_WRITE_RECEIVER:
				length = RECEIVER_DATA_SIZE;
				break;

			case INS_SELECT_SUBSCRIPTION:
				length = SELECTOR_SIZE;
				break;

			case INS_SUBSCRIPTION_STATUS:
				length = SUBSCRIPTION_STATUS_SIZE;
				break;

			case INS_EVENT_QUEUE:
				length = EVENT_SIZE;
				break;

			default:
				status(STATUS_UNSUPPORTED_INS);
				return;
		}

		const bool subscription = m_last_ins == INS_SELECT_SUBSCRIPTION || m_last_ins == INS_SUBSCRIPTION_STATUS;
		const auto p1 = m_header[HEADER_P1] ^ mask;
		bool valid_p1 = (p1 == 0);
		if (subscription)
			valid_p1 = (p1 == 0 || p1 == QUERY_ALTERNATE_MODE);
		else if (m_last_ins == INS_EVENT_QUEUE)
			valid_p1 = (p1 <= LAST_EVENT_QUEUE);

		if (!valid_p1 || (m_header[HEADER_P2] ^ mask) != (subscription ? QUERY_SUBSCRIPTION : 0))
		{
			status(STATUS_INVALID_PARAMETERS);
		}
		else if (m_header[HEADER_LENGTH] != length)
		{
			status(STATUS_INVALID_LENGTH);
		}
		else if (m_last_ins == INS_SUBSCRIPTION_STATUS && !m_subscription_selected)
		{
			status(STATUS_NOT_SELECTED);
		}
		else
		{
			queue(m_header[HEADER_INS]);

			switch (m_last_ins)
			{
				case INS_IDENTITY:
				{
					for (uint32_t i = 0; i < IDENTITY_SIZE; i++)
						queue(i >= SERIAL_OFFSET && i < SERIAL_OFFSET + ID_BYTES ? CARD_SERIAL >> ((SERIAL_OFFSET + ID_BYTES - 1 - i) * 8) : 0);

					status(STATUS_OK);
					break;
				}

				case INS_EVENT_QUEUE:
				{
					// Empty event queue
					for (uint32_t i = 0; i < EVENT_SIZE; i++)
						queue(0);

					status(STATUS_OK);
					break;
				}

				case INS_SUBSCRIPTION_STATUS:
				{
					queue(VS_AUTHORIZED);

					for (uint32_t i = 1; i < SUBSCRIPTION_STATUS_SIZE; i++)
						queue(0);

					status(STATUS_OK);
					break;
				}

				case INS_SELECT_SUBSCRIPTION:
				case INS_WRITE_RECEIVER:
					m_payload_expected = length;
					break;
			}
		}
	}
};

#endif // MAME_WEBTV_UTV_CA_CARD_H
