// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_UTV_SC_H
#define MAME_WEBTV_UTV_SC_H

#pragma once

#include "utv_ca_card.h"

#include <cstdint>

class utv_sc
{
public:

	static constexpr uint32_t FIFO_SIZE       = 16;
	static constexpr uint32_t IRQ_RX          = 0x04;
	static constexpr uint32_t IRQ_TX          = 0x20;
	static constexpr uint32_t IRQ_CARD_DETECT = 0x80;
	static constexpr uint32_t REGISTER_BYTES  = 0x300;
	static constexpr uint32_t REGISTER_SHIFT  = 2;

	enum register_offset : uint32_t
	{
		REG_TX_DATA           = 0x000,
		REG_TX_COUNT          = 0x00c,
		REG_TX_CAPACITY       = 0x010,
		REG_RX_DATA           = 0x040,
		REG_RX_COUNT          = 0x04c,
		REG_RX_CAPACITY       = 0x050,
		REG_RX_CONTROL        = 0x054,
		REG_RX_ENABLE         = 0x0c0,
		REG_CLOCK_DIVIDER     = 0x100,
		REG_BAUD_DIVIDER_LOW  = 0x104,
		REG_BAUD_DIVIDER_HIGH = 0x108,
		REG_CONVENTION        = 0x184,
		REG_CLOCK_FREQUENCY   = 0x188,
		REG_IRQ_MASKED        = 0x200,
		REG_IRQ_RAW           = 0x204,
		REG_IRQ_ENABLE        = 0x208,
		REG_IRQ_DISABLE       = 0x20c,
		REG_IRQ_ACK           = 0x210,
		REG_GPIO_SET          = 0x2b0,
		REG_GPIO_CLEAR        = 0x2b4,
		REG_GPIO_VALUE        = 0x2b8
	};

	enum register_mask : uint32_t
	{
		RX_ENABLE          = 0x01,
		RX_FIFO_RESET      = 0x02,
		CONVENTION_INVERSE = 0x03,
		GPIO_SERIAL_IDLE   = 0x01,
		GPIO_POWER_DISABLE = 0x04,
		GPIO_RESET_RELEASE = 0x08,
		GPIO_CARD_ABSENT   = 0x10,
		GPIO_OUTPUT_MASK   = 0x0e,
		TX_DATA_MASK       = 0xff,
		BAUD_LOW_MASK      = 0xff,
		BAUD_HIGH_MASK     = 0x1f,
		CLOCK_DIVIDER_MASK = 0xffff
	};

	static constexpr uint32_t CLOCK_PULSE_BAUD_FACTOR = 16;
	static constexpr uint32_t CLOCK_DIVIDER_SCALE     = 128 * 2;

	uint32_t m_regs[REGISTER_BYTES >> REGISTER_SHIFT] {};
	uint8_t m_rx[FIFO_SIZE] {};
	uint8_t m_tx[FIFO_SIZE] {};
	bool m_tx_pulse[FIFO_SIZE] {};
	uint32_t m_rx_head = 0;
	uint32_t m_rx_count = 0;
	uint32_t m_tx_head = 0;
	uint32_t m_tx_count = 0;
	uint32_t m_irq_pending = 0;
	uint32_t m_irq_enabled = 0;
	uint32_t m_gpio_outputs = 0;
	bool m_present = false;

	void insert_card(utv_ca_card &card)
	{
		remove_card();

		m_card = &card;
		m_card->clear();

		set_present(true);
	}

	void remove_card()
	{
		set_present(false);

		if (m_card)
			m_card->clear();

		m_card = nullptr;
	}

	void reset()
	{
		auto *card = m_card;
		*this = utv_sc{};
		m_card = card;

		if (m_card)
		{
			m_card->clear();
			set_present(true);
		}
	}

	void tick()
	{
		uint8_t data;
		bool clock_pulse;

		if (transmit(data, &clock_pulse) && card_active() && m_card && !clock_pulse)
			m_card->write(data);

		if (card_active() && m_card && m_card->m_reply_count && receive_card(m_card->m_reply[m_card->m_reply_head]))
			m_card->take(data);
	}


	uint32_t irq_status() const
	{
		return m_irq_pending | (m_rx_count ? IRQ_RX : 0);
	}

	bool have_irq_pending() const
	{
		return (irq_status() & m_irq_enabled) != 0;
	}

	bool card_active() const
	{
		return m_present && !(m_gpio_outputs & GPIO_POWER_DISABLE) && (m_gpio_outputs & GPIO_RESET_RELEASE);
	}

	void set_present(bool value)
	{
		const bool was_active = card_active();

		if (m_present != value)
			m_irq_pending |= IRQ_CARD_DETECT;

		m_present = value;

		update_card(was_active);
	}

	static constexpr uint8_t inverse(uint8_t value)
	{
		value = ((value & 0x55) << 1) | ((value >> 1) & 0x55);
		value = ((value & 0x33) << 2) | ((value >> 2) & 0x33);

		return ~((value << 4) | (value >> 4));
	}

	bool receive_card(uint8_t data)
	{
		if (!(m_regs[REG_RX_ENABLE >> REGISTER_SHIFT] & RX_ENABLE) || !(m_regs[REG_RX_CONTROL >> REGISTER_SHIFT] & RX_ENABLE))
			return false;
		else
			return receive((m_regs[REG_CONVENTION >> REGISTER_SHIFT] & CONVENTION_INVERSE) == CONVENTION_INVERSE ? data : inverse(data));
	}

	bool receive(uint8_t data)
	{
		if (m_rx_count == FIFO_SIZE)
			return false;

		m_rx[(m_rx_head + m_rx_count++) % FIFO_SIZE] = data;

		return true;
	}

	bool transmit(uint8_t &data, bool *clock_pulse = nullptr)
	{
		if (!m_tx_count)
			return false;

		data = m_tx[m_tx_head];

		if (clock_pulse)
			*clock_pulse = m_tx_pulse[m_tx_head];

		m_tx_head = (m_tx_head + 1) % FIFO_SIZE;
		m_tx_count--;
		m_irq_pending |= IRQ_TX;

		return true;
	}

	uint32_t read(uint32_t reg, bool side_effects = true)
	{
		switch (reg)
		{
			case REG_TX_COUNT:
				return m_tx_count;

			case REG_TX_CAPACITY:
				return FIFO_SIZE;

			case REG_RX_DATA:
			{
				const auto data = m_rx_count ? m_rx[m_rx_head] : 0;
				if (m_rx_count && side_effects)
				{
					m_rx_head = (m_rx_head + 1) % FIFO_SIZE;
					m_rx_count--;
				}
				return data;
			}

			case REG_RX_COUNT:
				return m_rx_count;

			case REG_RX_CAPACITY:
				return FIFO_SIZE;

			case REG_IRQ_MASKED:
				return irq_status() & m_irq_enabled;

			case REG_IRQ_RAW:
				return irq_status();

			case REG_IRQ_ENABLE:
			case REG_IRQ_DISABLE:
				return m_irq_enabled;

			case REG_GPIO_VALUE:
				return m_gpio_outputs | (m_present ? GPIO_SERIAL_IDLE : GPIO_CARD_ABSENT); // detect bit 4 is active low

			default:
				return reg < sizeof(m_regs) ? m_regs[reg >> REGISTER_SHIFT] : 0xffffffff;
		}
	}

	bool write(uint32_t reg, uint32_t data, uint32_t mask = 0xffffffff)
	{
		const bool was_active = card_active();
		const auto bits = data & mask;

		switch (reg)
		{
			case REG_TX_DATA:
			{
				if (!(mask & TX_DATA_MASK))
					return true;
				if (m_tx_count == FIFO_SIZE)
					return false;

				const auto position = (m_tx_head + m_tx_count++) % FIFO_SIZE;
				m_tx[position] = data & TX_DATA_MASK;

				const auto baud_low = m_regs[REG_BAUD_DIVIDER_LOW >> REGISTER_SHIFT] & BAUD_LOW_MASK;
				const auto baud_high = m_regs[REG_BAUD_DIVIDER_HIGH >> REGISTER_SHIFT] & BAUD_HIGH_MASK;
				const auto baud_divider = baud_low | (baud_high << 8);
				const auto clock_divider = m_regs[REG_CLOCK_DIVIDER >> REGISTER_SHIFT] & CLOCK_DIVIDER_MASK;
				const bool clock_enabled = m_regs[REG_CLOCK_FREQUENCY >> REGISTER_SHIFT] != 0;

				const auto symbol_period = CLOCK_PULSE_BAUD_FACTOR * (baud_divider + 1);
				const auto clock_period = CLOCK_DIVIDER_SCALE * (clock_divider + 1);

				m_tx_pulse[position] = clock_enabled && symbol_period < clock_period;

				m_irq_pending &= ~IRQ_TX;
				return true;
			}

			case REG_IRQ_MASKED:
				m_irq_pending &= ~bits;
				break;

			case REG_IRQ_ENABLE:
				m_irq_enabled |= bits;
				break;

			case REG_IRQ_DISABLE:
				m_irq_enabled &= ~bits;
				break;

			case REG_IRQ_ACK:
				m_irq_pending &= ~bits;
				break;

			case REG_GPIO_SET:
				m_gpio_outputs |= bits & GPIO_OUTPUT_MASK;
				break;

			case REG_GPIO_CLEAR:
				m_gpio_outputs &= ~(bits & GPIO_OUTPUT_MASK);
				break;

			case REG_GPIO_VALUE:
				m_gpio_outputs = (m_gpio_outputs & ~(mask & GPIO_OUTPUT_MASK)) | (bits & GPIO_OUTPUT_MASK);
				break;

			case REG_RX_CONTROL:
			{
				m_regs[reg >> REGISTER_SHIFT] = (m_regs[reg >> REGISTER_SHIFT] & ~mask) | bits;

				if (bits & RX_FIFO_RESET)
					m_rx_head = m_rx_count = 0;
				break;
			}

			default:
			{
				if (reg < sizeof(m_regs))
					m_regs[reg >> REGISTER_SHIFT] = (m_regs[reg >> REGISTER_SHIFT] & ~mask) | bits;
				break;
			}
		}

		update_card(was_active);

		return true;
	}

private:

	utv_ca_card *m_card = nullptr;

	void update_card(bool was_active)
	{
		if (was_active == card_active())
			return;

		m_rx_head = m_rx_count = 0;
		m_tx_head = m_tx_count = 0;
		m_irq_pending &= ~IRQ_TX;

		if (m_card)
		{
			if (card_active())
				m_card->start();
			else
				m_card->clear();
		}
	}
};

#endif // MAME_WEBTV_UTV_SC_H
