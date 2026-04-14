// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "ecp_lpt.h"

DEFINE_DEVICE_TYPE(ECP_LPT, ecp_lpt_device, "ecp_lpt", "IEEE 1284 SPP/EPP/ECP LPT Controller")

ecp_lpt_device::ecp_lpt_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, ECP_LPT, tag, owner, clock),
	m_irq_handler(*this),
	m_dev2hst_pdata(*this, "dev2hst_pdata"),
	m_hst2dev_pdata_w(*this),
	m_hst2dev_cntl_w(*this)
{
}

void ecp_lpt_device::device_start()
{
	//
}

void ecp_lpt_device::device_reset()
{
	m_irq_state = false;

	m_hst2dev_pdata = 0x00;
	m_dev2hst_status = 0x00;
	m_hst2dev_cntl = 0x00;
	m_dev2hst_cntl = 0x00;

	ecp_lpt_device::data_w(ecp_lpt_device::SPP_DEFAULT_VALUE);
	ecp_lpt_device::control_w(~ecp_lpt_device::ltp_spp_cntl_t::INITIALISE);
}

void ecp_lpt_device::device_add_mconfig(machine_config &config)
{
	INPUT_BUFFER(config, m_dev2hst_pdata);
}

uint8_t ecp_lpt_device::read(offs_t offset)
{
	uint8_t data = 0x00;

	switch (offset)
	{
		case ecp_lpt_device::SPP_PDATA_OFFSET:
			data = ecp_lpt_device::data_r();
			break;

		case ecp_lpt_device::SPP_STATUS_OFFSET:
			data = ecp_lpt_device::status_r();
			break;

		case ecp_lpt_device::SPP_CNTL_OFFSET:
			data = ecp_lpt_device::control_r();
			break;

		default:
			data = ecp_lpt_device::SPP_DEFAULT_VALUE;
			break;
	}
	//printf("ecp_lpt_device::read: offset=%02x data=%02x\n", offset, data);

	return data;
}

void ecp_lpt_device::write(offs_t offset, uint8_t data)
{
	//printf("ecp_lpt_device::write: offset=%02x, data=%02x\n", offset, data);
	switch (offset)
	{
		case ecp_lpt_device::SPP_PDATA_OFFSET:
			ecp_lpt_device::data_w(data);
			break;

		case ecp_lpt_device::SPP_STATUS_OFFSET:
			break;

		case ecp_lpt_device::SPP_CNTL_OFFSET:
			ecp_lpt_device::control_w(data);
			break;

		default:
			break;
	}
}

uint8_t ecp_lpt_device::data_r()
{
	//printf("ecp_lpt_device::data_r: data=%08x & %08x -> %08x\n", m_hst2dev_pdata, m_dev2hst_pdata->read(), (m_hst2dev_pdata & m_dev2hst_pdata->read()));

	return m_hst2dev_pdata & m_dev2hst_pdata->read();
}

void ecp_lpt_device::data_w(uint8_t data)
{
	ecp_lpt_device::signal_changed_bits(m_hst2dev_pdata_w, m_hst2dev_pdata, data);

	//printf("ecp_lpt_device::data_w: data=%08x -> %08x\n", m_hst2dev_pdata, data);

	m_hst2dev_pdata = data;
}

uint8_t ecp_lpt_device::status_r()
{
	//printf("ecp_lpt_device::status_r: data=%08x\n", m_dev2hst_status);

	return m_dev2hst_status;
}

uint8_t ecp_lpt_device::control_r()
{
	uint32_t data = (m_hst2dev_cntl & (m_dev2hst_cntl & ecp_lpt_device::SPP_CNTL_WMASK));

	//printf("ecp_lpt_device::control_r: data=%08x\n", (~data));

	return (~data);
}

void ecp_lpt_device::control_w(uint8_t data)
{
	data = ~(data ^ ecp_lpt_device::ltp_spp_cntl_t::INITIALISE);

	ecp_lpt_device::signal_changed_bits(m_hst2dev_cntl_w, m_hst2dev_cntl, data);

	//printf("ecp_lpt_device::control_w: data=%08x -> %08x\n", m_hst2dev_cntl, data);

	m_hst2dev_cntl = data;
}

void ecp_lpt_device::hst2dev_cntl_irq_enable_w(int state)
{
	ecp_lpt_device::set_parallel_irq();
}

void ecp_lpt_device::set_parallel_irq()
{
	bool ack = m_dev2hst_status & ecp_lpt_device::ltp_spp_status_t::ACK;
	bool irq_enable = m_hst2dev_cntl & ecp_lpt_device::ltp_spp_cntl_t::IRQ_ENABLE;

	bool new_irq_state = irq_enable || ack;

	if(m_irq_state != new_irq_state)
	{
		if(new_irq_state)
			m_irq_handler(CLEAR_LINE);
		else
			m_irq_handler(ASSERT_LINE);

		m_irq_state = new_irq_state;
	}
}
