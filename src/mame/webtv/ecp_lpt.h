// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_ECP_LPT_H
#define MAME_WEBTV_ECP_LPT_H

#pragma once

#include "machine/buffer.h"

class ecp_lpt_device : public device_t
{

public:

	enum ltp_spp_status_t : uint8_t
	{
		TIMEOUT   = 1 << 0,
		ERROR     = 1 << 3,
		SELECT    = 1 << 4,
		PERROR    = 1 << 5,
		ACK       = 1 << 6,
		BUSY      = 1 << 7
	};

	enum ltp_spp_cntl_t : uint8_t
	{
		BIDIRECTIONAL = 1 << 5,
		IRQ_ENABLE    = 1 << 4,
		SELECT_IN     = 1 << 3,
		INITIALISE    = 1 << 2,
		AUTOFEED      = 1 << 1,
		STROBE        = 1 << 0
	};

	static constexpr uint8_t SPP_PDATA_OFFSET  = 0;
	static constexpr uint8_t SPP_STATUS_OFFSET = 1;
	static constexpr uint8_t SPP_CNTL_OFFSET   = 2;

	static constexpr uint8_t SPP_DEFAULT_VALUE = 0xff;
	static constexpr uint8_t SPP_CNTL_WMASK    = 0x3f;

	ecp_lpt_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto irq_handler() { return m_irq_handler.bind(); }

	uint8_t read(offs_t offset);
	void write(offs_t offset, uint8_t data);

	uint8_t data_r();
	void data_w(uint8_t data);
	uint8_t status_r();
	uint8_t control_r();
	void control_w(uint8_t data);

	template <unsigned BITIDX> auto hst2dev_pdata_bit_handler()
	{
		return m_hst2dev_pdata_w[BITIDX].bind();
	}
	template <unsigned BITIDX> auto hst2dev_cntl_bit_handler()
	{ 
		return m_hst2dev_cntl_w[BITIDX].bind();
	}
	template <unsigned BITIDX> void dev2hst_status_wbit(int state)
	{
		ecp_lpt_device::change_bit(state, BITIDX, &m_dev2hst_status);
	}
	template <unsigned BITIDX> void dev2hst_cntl_wbit(int state)
	{
		ecp_lpt_device::change_bit(state, BITIDX, &m_dev2hst_cntl);
	}

	void hst2dev_cntl_irq_enable_w(int state);

	template <typename T> static void change_bit(int state, uint8_t bit, T* data)
	{
		if(state)
			*data = *data | (1 << bit);
		else
			*data = *data & (~(1 << bit));
	}

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

private:

	template <typename T> void signal_changed_bits(T &&write_line, uint8_t old_data, uint8_t new_data)
	{
		uint8_t changed = (old_data ^ new_data);
		
		for (uint8_t i = 0; i < 8; i++)
		{
			if (BIT(changed, i))
				write_line[i](BIT(new_data, i));
		}
	}

	devcb_write_line m_irq_handler;

	required_device<input_buffer_device> m_dev2hst_pdata;
	devcb_write_line::array<8> m_hst2dev_pdata_w;
	uint8_t m_hst2dev_pdata;

	uint8_t m_dev2hst_status;

	devcb_write_line::array<8> m_hst2dev_cntl_w;
	uint8_t m_hst2dev_cntl;
	uint8_t m_dev2hst_cntl;

	bool m_irq_state;

	void set_parallel_irq();

};

DECLARE_DEVICE_TYPE(ECP_LPT, ecp_lpt_device)

#endif // MAME_WEBTV_ECP_LPT_H
