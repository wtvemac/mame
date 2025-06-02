#include "emu.h"
#include "wtvdbg.h"
#include "bus/rs232/null_modem.h"

static DEVICE_INPUT_DEFAULTS_START(wtv_debug)
	DEVICE_INPUT_DEFAULTS("RS232_RXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_TXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_DATABITS", 0xff, RS232_DATABITS_8)
	DEVICE_INPUT_DEFAULTS("RS232_PARITY", 0xff, RS232_PARITY_NONE)
	DEVICE_INPUT_DEFAULTS("RS232_STOPBITS", 0xff, RS232_STOPBITS_1)
DEVICE_INPUT_DEFAULTS_END

wtvdbg_device_base::wtvdbg_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	rs232_port_device(mconfig, type, tag, owner, clock),
	m_serial_rx_handler(*this)
{
	wtvdbg_device_base::set_fixed(false);

	m_rxd_handler.bind().set(tag, FUNC(wtvdbg_device_base::serial_rx));

	wtvdbg_device_base::option_reset();
	wtvdbg_device_base::option_add("null_modem", NULL_MODEM);
	wtvdbg_device_base::set_default_option("null_modem");
	wtvdbg_device_base::set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(wtv_debug));
}


void wtvdbg_device_base::device_start()
{
	m_serial_rx_bit_idx = 0x0;
	m_serial_rx_buff_size = 0x0;
	m_serial_rx_buff_index = 0x0;

	save_item(NAME(m_serial_rx_bit_idx));
	save_item(NAME(m_serial_rx_buff_size));
	save_item(NAME(m_serial_rx_buff_index));
	
	m_serial_tx_bit_idx = 0x0;
	m_serial_tx_cur_byte = 0x0;
	m_serial_tx_buff_size = 0x0;
	m_serial_tx_buff_index = 0x0;

	save_item(NAME(m_serial_tx_bit_idx));
	save_item(NAME(m_serial_tx_cur_byte));
	save_item(NAME(m_serial_tx_buff_size));
	save_item(NAME(m_serial_tx_buff_index));
	
	m_serial_tx_timer = timer_alloc(FUNC(wtvdbg_device_base::serial_tx_tick), this);
}

void wtvdbg_device_base::device_reset()
{
	//
}

uint16_t wtvdbg_device_base::serial_tx_buffcnt_r()
{
	return (m_serial_tx_buff_size - m_serial_tx_buff_index);
}

uint16_t wtvdbg_device_base::serial_tx_buffmax_r()
{
	return wtvdbg_device_base::BUFF_MAX_SIZE;
}

void wtvdbg_device_base::serial_tx_byte_w(uint8_t data)
{
	m_serial_tx_buff[m_serial_tx_buff_size & (wtvdbg_device_base::BUFF_MAX_SIZE - 1)] = data;

	if (m_serial_tx_buff_size == 0x0)
	{
		m_serial_tx_timer->adjust(attotime::from_hz(wtvdbg_device_base::UART_BUAD_RATE));
	}

	m_serial_tx_buff_size++;
}

void wtvdbg_device_base::serial_tx_bitbang_w(uint32_t data)
{

	m_tx_bitbang_bitmask = (m_tx_bitbang_bitmask << 1) | 1;
	m_tx_bitbang_tx_data = (m_tx_bitbang_tx_data << 1) | (data == 0);

	// Just checking if the all bits are present. Not checking if they're valid.
	if ((m_tx_bitbang_bitmask & 0x7ff) == 0x7ff)
	{
		uint8_t tx_byte = 0x00;

		if ((m_tx_bitbang_tx_data & 0x700) != 0x600)
			// V1: there's 2 bits at the start (1 high and 1 low), 8 data bits and 1 bit at the end.
			tx_byte = (m_tx_bitbang_tx_data >> 1);
		else
			// V2: there's 3 bits at the start (all high), 8 data bits and no bits at the end.
			tx_byte = m_tx_bitbang_tx_data;

		// This reverses the bit order
		tx_byte = (tx_byte & 0xf0) >> 4 | (tx_byte & 0x0f) << 4; // Divide byte into 2 nibbles and swap them
		tx_byte = (tx_byte & 0xcc) >> 2 | (tx_byte & 0x33) << 2; // Divide nibble into 2 bits and swap them
		tx_byte = (tx_byte & 0xaa) >> 1 | (tx_byte & 0x55) << 1; // Divide again and swap the remaining bits

		wtvdbg_device_base::serial_tx_byte_w(tx_byte);

		m_tx_bitbang_bitmask = 0x0;
		m_tx_bitbang_tx_data = 0x0;
	}
}

TIMER_CALLBACK_MEMBER(wtvdbg_device_base::serial_tx_tick)
{
	attotime next_tick = attotime::never;

	if (m_serial_tx_bit_idx == 0x0)
	{
		wtvdbg_device_base::write_txd(0);
	}
	else if (m_serial_tx_bit_idx == 0x9)
	{
		wtvdbg_device_base::write_txd(1);
	}
	else
	{
		if (m_serial_tx_bit_idx == 0x1)
		{
			osd_printf_debug("%c", m_serial_tx_buff[m_serial_tx_buff_index & (BUFF_MAX_SIZE - 1)]);
		}

		uint8_t tx_bit = m_serial_tx_buff[m_serial_tx_buff_index & (wtvdbg_device_base::BUFF_MAX_SIZE - 1)] & 0x1;
		m_serial_tx_buff[m_serial_tx_buff_index & (wtvdbg_device_base::BUFF_MAX_SIZE - 1)] >>= 1;

		wtvdbg_device_base::write_txd(tx_bit);
	}

	if (m_serial_tx_bit_idx == 0x9)
	{
		m_serial_tx_bit_idx = 0x0;

		m_serial_tx_buff_index++;

		if (m_serial_tx_buff_index >= m_serial_tx_buff_size)
		{
			m_serial_tx_buff_index = 0x0;
			m_serial_tx_buff_size = 0x0;
		}
		else
		{
			next_tick = attotime::from_hz(wtvdbg_device_base::UART_BUAD_RATE);
		}
	}
	else
	{
		m_serial_tx_bit_idx++;
		next_tick = attotime::from_hz(wtvdbg_device_base::UART_BUAD_RATE);
	}

	m_serial_tx_timer->adjust(next_tick);
}

uint16_t wtvdbg_device_base::serial_rx_buffcnt_r()
{
	return (m_serial_rx_buff_size - m_serial_rx_buff_index);
}

uint16_t wtvdbg_device_base::serial_rx_buffmax_r()
{
	return wtvdbg_device_base::BUFF_MAX_SIZE;
}

uint8_t wtvdbg_device_base::serial_rx_byte_r()
{
	uint8_t data = 0x00000000;

	if (m_serial_rx_buff_size > 0x0)
	{
		data = m_serial_rx_buff[m_serial_rx_buff_index++ & (wtvdbg_device_base::BUFF_MAX_SIZE - 1)];

		if (m_serial_rx_buff_index >= m_serial_rx_buff_size)
		{
			m_serial_rx_buff_index = 0x0;
			m_serial_rx_buff_size = 0x0;
		}
	}

	return data;
}

void wtvdbg_device_base::serial_rx(int state)
{
	if (m_serial_rx_bit_idx == 0x0)
	{
		if (state == 0x0)
		{
			m_serial_tx_cur_byte = 0x0;
			m_serial_rx_bit_idx++;
		}
	}
	else if(m_serial_rx_bit_idx == 0x9)
	{
		if (state == 0x1)
		{
			m_serial_rx_bit_idx = 0x0;
			m_serial_rx_buff[m_serial_rx_buff_size++ & (wtvdbg_device_base::BUFF_MAX_SIZE - 1)] = m_serial_tx_cur_byte;

			m_serial_rx_handler(state);
		}
	}
	else if (m_serial_rx_bit_idx > 0)
	{
		m_serial_tx_cur_byte |= ((state & 0x1) << (m_serial_rx_bit_idx - 1));

		m_serial_rx_bit_idx++;
	}
}

DEFINE_DEVICE_TYPE(WTV_RS232DBG, wtvdbg_rs232_device, "wtvdbg_rs232", "WebTV RS232 Debug Device")

wtvdbg_rs232_device::wtvdbg_rs232_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	wtvdbg_device_base(mconfig, WTV_RS232DBG, tag, owner, clock)
{
}

DEFINE_DEVICE_TYPE(WTV_PEKOEDBG, wtvdbg_pekoe_device, "wtvdbg_pekoe", "WebTV Pekoe Debug Device")

wtvdbg_pekoe_device::wtvdbg_pekoe_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
wtvdbg_device_base(mconfig, WTV_PEKOEDBG, tag, owner, clock)
{
}

void wtvdbg_pekoe_device::device_start()
{
	wtvdbg_device_base::device_start();

	m_mode = MODE_DEVICE_ENABLED;

	save_item(NAME(m_mode));
}

void wtvdbg_pekoe_device::device_reset()
{
	wtvdbg_device_base::device_reset();
}

void wtvdbg_pekoe_device::map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(wtvdbg_pekoe_device::reg_0000_r), FUNC(wtvdbg_pekoe_device::reg_0000_w));
	map(0x004, 0x007).rw(FUNC(wtvdbg_pekoe_device::reg_0004_r), FUNC(wtvdbg_pekoe_device::reg_0004_w));
	map(0x008, 0x00b).rw(FUNC(wtvdbg_pekoe_device::reg_0008_r), FUNC(wtvdbg_pekoe_device::reg_0008_w));
	map(0x00c, 0x00f).rw(FUNC(wtvdbg_pekoe_device::reg_000c_r), FUNC(wtvdbg_pekoe_device::reg_000c_w));
	map(0x010, 0x013).rw(FUNC(wtvdbg_pekoe_device::reg_0010_r), FUNC(wtvdbg_pekoe_device::reg_0010_w));
	map(0x014, 0x017).rw(FUNC(wtvdbg_pekoe_device::reg_0014_r), FUNC(wtvdbg_pekoe_device::reg_0000_w));
}

uint32_t wtvdbg_pekoe_device::reg_0000_r()
{
	if ((m_mode & MODE_DEVICE_ENABLED) == MODE_DEVICE_ENABLED)
	{
		return wtvdbg_pekoe_device::serial_rx_byte_r();
	}
	else
	{
		return 0x00000000;
	}
}

void wtvdbg_pekoe_device::reg_0000_w(uint32_t data)
{
	if ((m_mode & MODE_DEVICE_ENABLED) == MODE_DEVICE_ENABLED)
	{
		if (m_mode & MODE_BAUD_CONFIGURE)
		{
			// Configure baud rate  (upper half)
		}
		else
		{
			wtvdbg_pekoe_device::serial_tx_byte_w(data & 0xff);
		}
	}
}

uint32_t wtvdbg_pekoe_device::reg_0004_r()
{
	return 0x00000000;
}

void wtvdbg_pekoe_device::reg_0004_w(uint32_t data)
{
	if (m_mode & MODE_BAUD_CONFIGURE)
	{
		// Configure baud rate (lower half)
	}
	else
	{
		//
	}
}

uint32_t wtvdbg_pekoe_device::reg_0008_r()
{
	return 0x00000000;
}

void wtvdbg_pekoe_device::reg_0008_w(uint32_t data)
{
	// Used when this device is configured. Set to 0x6
}

uint32_t wtvdbg_pekoe_device::reg_000c_r()
{
	return m_mode;
}

void wtvdbg_pekoe_device::reg_000c_w(uint32_t data)
{
	m_mode = data;
}

uint32_t wtvdbg_pekoe_device::reg_0010_r()
{
	return 0x00000000;
}

void wtvdbg_pekoe_device::reg_0010_w(uint32_t data)
{
	// Used when this device is configured. Set 0xb
}

uint32_t wtvdbg_pekoe_device::reg_0014_r()
{
	uint32_t status = 0x00000000;

	if (wtvdbg_pekoe_device::serial_tx_buffcnt_r() < wtvdbg_pekoe_device::serial_tx_buffmax_r())
		status |= wtvdbg_pekoe_device::STATUS_TX_CAN_SEND_BYTE;

	if (wtvdbg_pekoe_device::serial_rx_buffcnt_r() > 0)
		status |= wtvdbg_pekoe_device::STATUS_RX_BYTE_AVAILABLE;

	return status;
}

void wtvdbg_pekoe_device::reg_0014_w(uint32_t data)
{
	//
}