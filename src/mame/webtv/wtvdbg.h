// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVDBG_H
#define MAME_WEBTV_WTVDBG_H

#pragma once

#include "bus/rs232/rs232.h"

class wtvdbg_device_base : public rs232_port_device
{

public:

	wtvdbg_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	auto serial_rx_handler() { return m_serial_rx_handler.bind(); }

	uint16_t serial_tx_buffcnt_r();
	uint16_t serial_tx_buffmax_r();
	void serial_tx_byte_w(uint8_t data);
	void serial_tx_bitbang_w(uint32_t data);

	uint16_t serial_rx_buffcnt_r();
	uint16_t serial_rx_buffmax_r();
	uint8_t serial_rx_byte_r();

private:

	static constexpr uint16_t BUFF_MAX_SIZE = 0x800;
	static constexpr uint32_t UART_BUAD_RATE = 115200;

	uint32_t m_serial_rx_bit_idx;
	uint8_t m_serial_rx_buff[BUFF_MAX_SIZE];
	uint32_t m_serial_rx_buff_size;
	uint32_t m_serial_rx_buff_index;

	void serial_rx(int state);

	uint8_t m_serial_tx_bit_idx;
	uint8_t m_serial_tx_cur_byte;
	uint8_t m_serial_tx_buff[BUFF_MAX_SIZE];
	uint32_t m_serial_tx_buff_size;
	uint32_t m_serial_tx_buff_index;
	uint16_t m_tx_bitbang_bitmask;
	uint16_t m_tx_bitbang_tx_data;

	emu_timer *m_serial_tx_timer = nullptr;
	TIMER_CALLBACK_MEMBER(serial_tx_tick);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	devcb_write_line m_serial_rx_handler;

};


class wtvdbg_rs232_device : public wtvdbg_device_base
{

public:

	wtvdbg_rs232_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

};

DECLARE_DEVICE_TYPE(WTV_RS232DBG, wtvdbg_rs232_device)


class wtvdbg_pekoe_device : public wtvdbg_device_base
{

public:

	wtvdbg_pekoe_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	void map(address_map &map);

private:

	// These are best-guesses

	/*
	  This seems to always ben set 0x03 whenever the device is used. It's probably two bits that have some meaning.
	*/
	static constexpr uint32_t MODE_DEVICE_ENABLED = 0x00000003;
	/*
	  0x1c200 is devided by the desired baud rate then the result's lower half (x & 0xff) is sent to 0000_w and the upper half ((x >> 0x08) && 0xff) is sent to 0004_w
	*/
	static constexpr uint32_t MODE_BAUD_CONFIGURE = 0x00000080;

	static constexpr uint32_t STATUS_RX_BYTE_AVAILABLE = 0x00000001;
	static constexpr uint32_t STATUS_TX_CAN_SEND_BYTE  = 0x00000060;

	uint32_t m_mode;

	/* Pekoe registers */

	uint32_t reg_0000_r();          // Pekoe data (read)
	void reg_0000_w(uint32_t data); // Pekoe data (write)
	uint32_t reg_0004_r();          // Pekoe ??? (read)
	void reg_0004_w(uint32_t data); // Pekoe ??? (write)
	uint32_t reg_0008_r();          // Pekoe ??? (read)
	void reg_0008_w(uint32_t data); // Pekoe ??? (write)
	uint32_t reg_000c_r();          // Pekoe mode? (read)
	void reg_000c_w(uint32_t data); // Pekoe mode? (write)
	uint32_t reg_0010_r();          // Pekoe ??? (read)
	void reg_0010_w(uint32_t data); // Pekoe ??? (write)
	uint32_t reg_0014_r();          // Pekoe status? (read)
	void reg_0014_w(uint32_t data); // Pekoe status? (write)

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

};

DECLARE_DEVICE_TYPE(WTV_PEKOEDBG, wtvdbg_pekoe_device)

#endif // MAME_WEBTV_WTVDBG_H