// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/bus/pci/

// Description here

#include "emu.h"
#include "i82801_eth.h"
#include "multibyte.h"

DEFINE_DEVICE_TYPE(I82801_ETH,        i82801_eth_device,          "i82801_eth",        "Intel 82801 10/100 Ethernet Controller w/ 8256ET PHY")
DEFINE_DEVICE_TYPE(I82801_ETH_EEPROM, i82801_eth_eeprom_device,   "i82801_eth_eeprom", "Intel 82801 10/100 Serial EEPROM")

i82801_eth_eeprom_device::i82801_eth_eeprom_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, bool extended_reg_count)
	: device_t(mconfig, I82801_ETH_EEPROM, tag, owner, clock),
	device_nvram_interface(mconfig, *this)
{
	if(extended_reg_count)
		m_register_count = i82801_eth_eeprom_device::EEPROM_EXREG_COUNT;
	else
		m_register_count = i82801_eth_eeprom_device::EEPROM_REG_COUNT;

	m_eeprom_size = m_register_count << 1;
}

void i82801_eth_eeprom_device::device_start()
{
	m_eeprom_data = make_unique_clear<uint16_t[]>(m_register_count);


	save_pointer(NAME(m_eeprom_data), m_eeprom_size);
}

void i82801_eth_eeprom_device::device_reset()
{
	i82801_eth_eeprom_device::set_idle();
}

void i82801_eth_eeprom_device::nvram_default()
{
	memset((uint8_t *)m_eeprom_data.get(), 0x00, m_eeprom_size);
}

bool i82801_eth_eeprom_device::nvram_read(util::read_stream &file)
{
	std::error_condition err;
	size_t actual;

	std::tie(err, actual) = read(file, (uint8_t *)m_eeprom_data.get(), m_eeprom_size);

	return (!err && actual == m_eeprom_size);
}

bool i82801_eth_eeprom_device::nvram_write(util::write_stream &file)
{
	std::error_condition err;
	size_t actual;

	std::tie(err, actual) = write(file, (uint8_t *)m_eeprom_data.get(), m_eeprom_size);

	return (!err && actual == m_eeprom_size);
}

int i82801_eth_eeprom_device::cs_r()
{
	return m_cs;
}

void i82801_eth_eeprom_device::cs_w(int state)
{
	if(!state)
	{
		if(m_cs != state)
		{
			if(m_command == i82801_eth_eeprom_device::EEPROM_CMD_OTHER && m_state == i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS_READY)
				i82801_eth_eeprom_device::process_other_command(m_address);

			if(m_state == i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE)
				i82801_eth_eeprom_device::end_command();
		}

		i82801_eth_eeprom_device::set_idle();
	}

	m_cs = state;
}

int i82801_eth_eeprom_device::sk_r()
{
	if(m_cs)
		return m_sk;
	else
		return 0;
}

void i82801_eth_eeprom_device::sk_w(int state)
{
	bool ready = (m_cs && (state || m_state == i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS_READY));

	if (m_sk != state && ready)
	{
		switch (m_state)
		{
			case i82801_eth_eeprom_device::eeprom_state_t::START_IDLE: // Start bit wait
				if(m_di) // if first bit is 1 then it is the start bit
				{
					m_command = 0x00;
					m_bit_position = (i82801_eth_eeprom_device::EEPROM_CMD_BIT_COUNT - 1);
					m_bit_mask = i82801_eth_eeprom_device::EEPROM_CMD_MASK;
					m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_COMMAND;
				}
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_COMMAND:
				m_command |= (m_di << m_bit_position) & m_bit_mask;
				m_bit_position--;

				if (m_bit_position < 0)
				{
					m_address = 0x00;
					if(m_register_count == i82801_eth_eeprom_device::EEPROM_EXREG_COUNT)
					{
						m_bit_position = (i82801_eth_eeprom_device::EEPROM_ADDR8_BIT_COUNT - 1);
						m_bit_mask = i82801_eth_eeprom_device::EEPROM_ADDR8_MASK;
					}
					else
					{
						m_bit_position = (i82801_eth_eeprom_device::EEPROM_ADDR6_BIT_COUNT - 1);
						m_bit_mask = i82801_eth_eeprom_device::EEPROM_ADDR6_MASK;
					}

					m_do = 1;

					m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS;
				}
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS:
				m_address |= (m_di << m_bit_position) & m_bit_mask;
				m_bit_position--;

				if (m_bit_position < 0)
				{
					m_do = 0;

					if(m_command == i82801_eth_eeprom_device::EEPROM_CMD_READ)
						m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DUMMY_BIT_WAIT;
					else
						m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS_READY;
				}
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_ADDRESS_READY:
					i82801_eth_eeprom_device::start_command();
					break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_DUMMY_BIT_WAIT:
				if(!m_di) // this meed to see a 0 for the dummy bit
					i82801_eth_eeprom_device::start_command();
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_DATA_IN:
				m_data |= (m_di << m_bit_position) & i82801_eth_eeprom_device::EEPROM_DATA_MASK;
				m_bit_position--;

				if (m_bit_position < 0)
				{
					m_bit_position = 0;
					m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE;
				}
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_DATA_OUT:
				m_bit_position--;

				if (m_bit_position < 0)
				{
					m_bit_position = 0;
					m_bit_mask = 0x0000;
					m_do = 1;
					m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE;
				}
				else
				{
					m_do = (m_data >> m_bit_position) & 1;
				}
				break;

			case i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE:
			default:
				break;
		}
	}

	m_sk = state;
}

int i82801_eth_eeprom_device::do_r()
{
	if(m_cs)
		return m_do;
	else
		return 0;
}

void i82801_eth_eeprom_device::di_w(int state)
{
	if(m_cs)
		m_di = state;
}

void i82801_eth_eeprom_device::set_idle()
{
	m_write_en = false;

	m_cs = 0;
	m_sk = 0;
	m_do = 1;
	m_di = 1;

	m_bit_position = 0;
	m_command = 0x00;
	m_address = 0x00;
	m_data = 0x0000;

	m_state = i82801_eth_eeprom_device::eeprom_state_t::START_IDLE;
}

void i82801_eth_eeprom_device::start_command()
{
	switch(m_command)
	{
		case i82801_eth_eeprom_device::EEPROM_CMD_OTHER:
			i82801_eth_eeprom_device::process_other_command(m_address);
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_READ:
			m_data = i82801_eth_eeprom_device::read_reg(m_address);
			m_bit_position = (i82801_eth_eeprom_device::EEPROM_DATA_BIT_COUNT - 1);
			m_bit_mask = i82801_eth_eeprom_device::EEPROM_DATA_MASK;
			m_do = (m_data >> m_bit_position) & 1;
			m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DATA_OUT;
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_WRITE:
			m_data = 0x0000;
			m_bit_position = (i82801_eth_eeprom_device::EEPROM_DATA_BIT_COUNT - 1);
			m_bit_mask = i82801_eth_eeprom_device::EEPROM_DATA_MASK;
			m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DATA_IN;
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_ERASE:
			m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE;
			break;

		default:
			break;
	}
}

void i82801_eth_eeprom_device::end_command()
{
	switch(m_command)
	{
		case i82801_eth_eeprom_device::EEPROM_CMD_WRITE:
			i82801_eth_eeprom_device::write_reg(m_address, m_data);
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_ERASE:
			i82801_eth_eeprom_device::erase_reg(m_address);
			break;

		default:
			break;
	}
}

void i82801_eth_eeprom_device::process_other_command(uint16_t command)
{
	switch(command)
	{
		case i82801_eth_eeprom_device::EEPROM_CMD_OTHER_WD: // Write disable
			i82801_eth_eeprom_device::set_write_enable(false);
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_OTHER_EALL: // Erase all
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_OTHER_WALL: // Write all
			break;

		case i82801_eth_eeprom_device::EEPROM_CMD_OTHER_WE: // Write enable
			i82801_eth_eeprom_device::set_write_enable(true);
			break;

		default:
			break;
	}

	m_state = i82801_eth_eeprom_device::eeprom_state_t::STATE_DONE;
}

void i82801_eth_eeprom_device::set_write_enable(bool write_en)
{
	m_write_en = write_en;
}

uint16_t i82801_eth_eeprom_device::read_reg(offs_t offset)
{
	return m_eeprom_data[offset];
}

void i82801_eth_eeprom_device::write_reg(offs_t offset, uint16_t data)
{
	if (m_write_en)
		m_eeprom_data[offset] = data;
}

void i82801_eth_eeprom_device::erase_reg(offs_t offset)
{
	if (m_write_en)
		m_eeprom_data[offset] = 0x0000;
}

i82801_eth_device::i82801_eth_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_card_device(mconfig, I82801_ETH, tag, owner, clock),
	device_network_interface(mconfig, *this, 100),
	m_dma_space(*this, ":maincpu", AS_PROGRAM),
	m_eeprom(*this, "eeprom"),
	m_pirq_cb(*this)
{
	intr_line = 0x00;
	intr_pin = i82801_lpc_device::INT_PIN_A;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_A;
	m_default_link_state = true;
}

void i82801_eth_device::device_start()
{
	pci_card_device::device_start();

	m_cu_action_timer = timer_alloc(FUNC(i82801_eth_device::cu_cbl_execute), this);
	m_ru_action_timer = timer_alloc(FUNC(i82801_eth_device::ru_rfd_execute), this);
}

void i82801_eth_device::device_reset()
{
	pci_card_device::device_reset();

	i82801_eth_device::full_controller_reset();

	i82801_eth_device::set_plc_defaults();
}

void i82801_eth_device::device_add_mconfig(machine_config &config)
{
	I82801_ETH_EEPROM(config, m_eeprom);
}

void i82801_eth_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	uint32_t membase = (m_csr_membase & i82801_eth_device::CSR_MEM_BASE_MASK);
	if(membase != 0x00000000)
	{
		memory_space->install_device(membase, membase + (i82801_eth_device::CSR_SIZE - 1), *this, &i82801_eth_device::csr_map);
	}

	uint16_t iobase = (m_csr_iobase & i82801_eth_device::CSR_IO_BASE_MASK);
	if(iobase != 0x0000)
	{
		io_space->install_device(iobase, iobase + (i82801_eth_device::CSR_SIZE - 1), *this, &i82801_eth_device::csr_map);
	}
}

void i82801_eth_device::config_map(address_map &map)
{
	pci_card_device::config_map(map);

	map(0x10, 0x13).rw(FUNC(i82801_eth_device::csr_membase_r), FUNC(i82801_eth_device::csr_membase_w));
	map(0x14, 0x17).rw(FUNC(i82801_eth_device::csr_iobase_r), FUNC(i82801_eth_device::csr_iobase_w));
	map(0xdc, 0xdc).r(FUNC(i82801_eth_device::capid_r));
	map(0xdd, 0xdd).r(FUNC(i82801_eth_device::nextptr_r));
	map(0xde, 0xdf).r(FUNC(i82801_eth_device::pm_cap_r));
	map(0xe0, 0xe1).rw(FUNC(i82801_eth_device::pm_csr_r), FUNC(i82801_eth_device::pm_csr_w));
	map(0xe3, 0xe3).r(FUNC(i82801_eth_device::pm_data_r));
}

void i82801_eth_device::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	pci_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

void i82801_eth_device::set_plc_defaults()
{
	for(uint8_t phy_offset = 0; phy_offset < i82801_eth_device::PHY_COUNT; phy_offset++)
		i82801_eth_device::set_default_phy_state(phy_offset);
}

void i82801_eth_device::set_default_phy_state(offs_t phy_offset)
{
	phy_offset &= (i82801_eth_device::PHY_COUNT - 1);

	for(uint8_t reg_offset = 0; reg_offset < i82801_eth_device::PLC_REGISTER_COUNT; reg_offset++)
		m_phy[phy_offset].reg[reg_offset] = 0x0000;

	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_ID1] = i82801_eth_device::PLC_ID1;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_ID2] = i82801_eth_device::PLC_ID2;

	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_CNTL] |= i82801_eth_device::PLC_CNTL_100MBPS_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_CNTL] |= i82801_eth_device::PLC_CNTL_AUTONEG_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_CNTL] |= i82801_eth_device::PLC_CNTL_FULL_DUPLEX_EN;

	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_100BTXFD_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_100BTXHD_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_10BTXFD_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_10BTXHD_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_AUTONEG_EN;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_EXCAP_EN;
	
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_AUTONEG_LP] |= i82801_eth_device::PLC_AUTONEG_LP_ACK;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_AUTONEG_LP] |= i82801_eth_device::PLC_AUTONEG_LP_TECHCAP_MASK; // 0xff
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_AUTONEG_LP] |= i82801_eth_device::PLC_AUTONEG_LP_SEL_MASK; // 0x0f

	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= i82801_eth_device::PLC_PHY_STATCNTL_NO_AUTOPD;
	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= (phy_offset + 1) << i82801_eth_device::PLC_PHY_STATCNTL_PHYADDR_SHIFT;

	m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_ADDR] = (phy_offset + 1);

	i82801_eth_device::set_phy_link_state(phy_offset, m_default_link_state);
}

void i82801_eth_device::set_link_state(bool active)
{
	for(uint8_t phy_offset = 0; phy_offset < i82801_eth_device::PHY_COUNT; phy_offset++)
		i82801_eth_device::set_phy_link_state(phy_offset, active);
}

void i82801_eth_device::set_phy_link_state(offs_t phy_offset, bool active)
{
	if(active)
	{
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_AUTONEG_DONE;
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] |= i82801_eth_device::PLC_STATUS_LINK_EN;

		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= i82801_eth_device::PLC_PHY_STATCNTL_RECV_DSSYNC;
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= i82801_eth_device::PLC_PHY_STATCNTL_10PD;
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= i82801_eth_device::PLC_PHY_STATCNTL_100MBPS_EN;
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] |= i82801_eth_device::PLC_PHY_STATCNTL_FULLD_EN;

		m_csr_sts |= i82801_eth_device::GENERAL_STATUS_FULL_DUPLEX_EN;
		m_csr_sts |= i82801_eth_device::GENERAL_STATUS_100MBPS_EN;
		m_csr_sts |= i82801_eth_device::GENERAL_STATUS_LINK_EN;
	}
	else
	{
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] &= (~i82801_eth_device::PLC_STATUS_AUTONEG_DONE);
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_STATUS] &= (~i82801_eth_device::PLC_STATUS_LINK_EN);

		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] &= (~i82801_eth_device::PLC_PHY_STATCNTL_RECV_DSSYNC);
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] &= (~i82801_eth_device::PLC_PHY_STATCNTL_10PD);
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] &= (~i82801_eth_device::PLC_PHY_STATCNTL_100MBPS_EN);
		m_phy[phy_offset].reg[i82801_eth_device::PLC_REGISTER_PHY_STATCNTL] &= (~i82801_eth_device::PLC_PHY_STATCNTL_FULLD_EN);

		m_csr_sts &= (~i82801_eth_device::GENERAL_STATUS_FULL_DUPLEX_EN);
		m_csr_sts &= (~i82801_eth_device::GENERAL_STATUS_100MBPS_EN);
		m_csr_sts &= (~i82801_eth_device::GENERAL_STATUS_LINK_EN);
	}

	m_csr_pmdr |= i82801_eth_device::PMDR_LINK_STATUS_CHANGE;
}

uint16_t i82801_eth_device::phy_state_read(offs_t phy_offset, offs_t reg_offset)
{
	if(phy_offset < 1 || phy_offset > i82801_eth_device::PHY_COUNT)
		return 0x0000;

	if(reg_offset >= i82801_eth_device::PLC_REGISTER_COUNT)
		return 0x0000;

	phy_offset--; // Internally we start at 0 rather than 1

	phy_offset &= (i82801_eth_device::PHY_COUNT - 1);
	reg_offset &= (i82801_eth_device::PLC_REGISTER_COUNT - 1);

	uint16_t cur_data = m_phy[phy_offset].reg[reg_offset];

	// Clear bits from self-clearing-on-read registers
	switch(reg_offset)
	{
		case i82801_eth_device::PLC_REGISTER_AUTONEG_EXP:
		{
			uint16_t new_data = cur_data;

			new_data &= (~i82801_eth_device::PLC_AUTONEG_EXP_PARDET_ERR);
			new_data &= (~i82801_eth_device::PLC_AUTONEG_EXP_PAGE_RECV);

			m_phy[phy_offset].reg[reg_offset & (i82801_eth_device::PLC_REGISTER_COUNT - 1)] = new_data;
			break;
		}

		case i82801_eth_device::PLC_REGISTER_RFC_CNT:
		case i82801_eth_device::PLC_REGISTER_RDC_CNT:
		case i82801_eth_device::PLC_REGISTER_RERR_CNT:
		case i82801_eth_device::PLC_REGISTER_RSERR_CNT:
		case i82801_eth_device::PLC_REGISTER_RPEOF_CNT:
		case i82801_eth_device::PLC_REGISTER_REOF_CNT:
		case i82801_eth_device::PLC_REGISTER_TJAB_CNT:
			m_phy[phy_offset].reg[reg_offset & (i82801_eth_device::PLC_REGISTER_COUNT - 1)] = 0;
			break;

		default:
			break;
	}

	return cur_data;
}

void i82801_eth_device::phy_state_write(offs_t phy_offset, offs_t reg_offset, uint16_t data)
{
	if(phy_offset < 1 || phy_offset > i82801_eth_device::PHY_COUNT)
		return;

	if(reg_offset >= i82801_eth_device::PLC_REGISTER_COUNT)
		return;

	phy_offset--; // Internally we start at 0 rather than 1

	phy_offset &= (i82801_eth_device::PHY_COUNT - 1);
	reg_offset &= (i82801_eth_device::PLC_REGISTER_COUNT - 1);

	uint16_t cur_data = m_phy[phy_offset].reg[reg_offset];

	switch(reg_offset)
	{
		case i82801_eth_device::PLC_REGISTER_CNTL:
		{
			if(data & i82801_eth_device::PLC_CNTL_RESET)
			{
				i82801_eth_device::set_default_phy_state(phy_offset);
				data &= (~i82801_eth_device::PLC_CNTL_RESET);
			}

			if(data & i82801_eth_device::PLC_CNTL_RESTART_AUTONEG)
				data &= (~i82801_eth_device::PLC_CNTL_RESTART_AUTONEG);

			if(data & i82801_eth_device::PLC_CNTL_AUTONEG_EN)
				data |= i82801_eth_device::PLC_CNTL_FULL_DUPLEX_EN;

			data = (cur_data & (~i82801_eth_device::PLC_CNTL_WMASK)) | (data & i82801_eth_device::PLC_CNTL_WMASK);
			break;
		}

		case i82801_eth_device::PLC_REGISTER_AUTONEG_AD:
			data = (cur_data & (~i82801_eth_device::PLC_AUTONEG_AD_WMASK)) | (data & i82801_eth_device::PLC_AUTONEG_AD_WMASK);
			break;

		case i82801_eth_device::PLC_REGISTER_PHY_STATCNTL:
			data = (cur_data & (~i82801_eth_device::PLC_PHY_STATCNTL_WMASK)) | (data & i82801_eth_device::PLC_PHY_STATCNTL_WMASK);
			break;

		case i82801_eth_device::PLC_REGISTER_PHY_SUCNTL1:
			data = (cur_data & (~i82801_eth_device::PLC_PHY_SUCNTL1_WMASK)) | (data & i82801_eth_device::PLC_PHY_SUCNTL1_WMASK);
			break;

		case i82801_eth_device::PLC_REGISTER_PHY_SUCNTL2:
			data = (cur_data & (~i82801_eth_device::PLC_PHY_SUCNTL2_WMASK)) | (data & i82801_eth_device::PLC_PHY_SUCNTL2_WMASK);
			break;

		default:
			data = cur_data; // Read only
			break;
	}

	m_phy[phy_offset].reg[reg_offset & (i82801_eth_device::PLC_REGISTER_COUNT - 1)] = data;
}

void i82801_eth_device::full_controller_reset()
{
	bool link_established = (m_csr_sts & i82801_eth_device::GENERAL_STATUS_LINK_EN);

	status = 0x0290;
	m_csr_membase = i82801_eth_device::DEFAULT_CSR_MEM_BASE;
	m_csr_iobase = i82801_eth_device::DEFAULT_CSR_IO_BASE;

	// subsystem_id set from eeprom

	m_csr_scb_sts = 0x0000;
	m_csr_scb_cmd = 0x0000;
	m_csr_scb_genptr = 0x00000000;
	m_csr_port = 0x00000000;
	m_csr_mdi_cntl = i82801_eth_device::MDI_READY;
	m_csr_dma_bytecnt = 0x00000000;
	m_csr_early_recvint = 0x00;
	m_csr_flow_cntl = 0x0000;
	m_csr_pmdr = 0x00;
	m_csr_cntl = 0x00;
	m_csr_sts = 0x00;
	m_pm_csr = 0x0000;

	std::fill(std::begin(m_configuration.data), std::end(m_configuration.data), 0x00);

	m_counters.reset();

	i82801_eth_device::cu_execute_stop();
	i82801_eth_device::ru_execute_stop();

	if(link_established)
		i82801_eth_device::set_link_state(true);
}

void i82801_eth_device::partial_controller_reset()
{
	i82801_eth_device::cu_execute_pause();
	i82801_eth_device::set_cu_state(cu_state_t::CU_IDLE);
	i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_CU_NA_INT, ASSERT_LINE);

	i82801_eth_device::ru_execute_pause();
	i82801_eth_device::set_ru_state(ru_state_t::RU_IDLE);
}

uint32_t i82801_eth_device::csr_membase_r()
{
	return m_csr_membase;
}

void i82801_eth_device::csr_membase_w(uint32_t data)
{
	uint32_t membase = (data & i82801_eth_device::CSR_MEM_BASE_MASK);
	if(membase != 0x00000000)
	{
		m_csr_membase = (m_csr_membase & (~i82801_eth_device::CSR_MEM_BASE_MASK)) | membase;
		i82801_eth_device::remap_cb();
	}
}

uint32_t i82801_eth_device::csr_iobase_r()
{
	return m_csr_iobase;
}

void i82801_eth_device::csr_iobase_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_eth_device::CSR_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_csr_iobase = (m_csr_iobase & (~i82801_eth_device::CSR_IO_BASE_MASK)) | iobase;
		i82801_eth_device::remap_cb();
	}
}

uint8_t i82801_eth_device::capptr_r()
{
	return 0xdc;
}

uint8_t i82801_eth_device::capid_r()
{
	return 0x01;
}

uint8_t i82801_eth_device::nextptr_r()
{
	return 0x00;
}

uint16_t i82801_eth_device::pm_cap_r()
{
	return 0xfe21;
}

uint16_t i82801_eth_device::pm_csr_r()
{
	return m_pm_csr;
}

void i82801_eth_device::pm_csr_w(uint16_t data)
{
	m_pm_csr = data;
}

uint8_t i82801_eth_device::pm_data_r()
{
	return 0x00;
}

void i82801_eth_device::csr_map(address_map &map)
{
	map(0x00, 0x01).rw(FUNC(i82801_eth_device::csr_scb_sts_r), FUNC(i82801_eth_device::csr_scb_sts_w));
	map(0x02, 0x03).rw(FUNC(i82801_eth_device::csr_scb_cmd_r), FUNC(i82801_eth_device::csr_scb_cmd_w));
	map(0x04, 0x07).rw(FUNC(i82801_eth_device::csr_scb_genptr_r), FUNC(i82801_eth_device::csr_scb_genptr_w));
	map(0x08, 0x0b).rw(FUNC(i82801_eth_device::csr_port_r), FUNC(i82801_eth_device::csr_port_w));
	map(0x0e, 0x0e).rw(FUNC(i82801_eth_device::csr_eeprom_cntl_r), FUNC(i82801_eth_device::csr_eeprom_cntl_w));
	map(0x10, 0x13).rw(FUNC(i82801_eth_device::csr_mdi_cntl_r), FUNC(i82801_eth_device::csr_mdi_cntl_w));
	map(0x14, 0x17).rw(FUNC(i82801_eth_device::csr_dma_bytecnt_r), FUNC(i82801_eth_device::csr_dma_bytecnt_w));
	map(0x18, 0x18).rw(FUNC(i82801_eth_device::csr_early_recvint_r), FUNC(i82801_eth_device::csr_early_recvint_w));
	map(0x19, 0x19).rw(FUNC(i82801_eth_device::csr_flow_cntl_high_r), FUNC(i82801_eth_device::csr_flow_cntl_high_w));
	map(0x1a, 0x1a).rw(FUNC(i82801_eth_device::csr_flow_cntl_low_r), FUNC(i82801_eth_device::csr_flow_cntl_low_w));
	map(0x1b, 0x1b).rw(FUNC(i82801_eth_device::csr_pmdr_r), FUNC(i82801_eth_device::csr_pmdr_w));
	map(0x1c, 0x1c).rw(FUNC(i82801_eth_device::csr_cntl_r), FUNC(i82801_eth_device::csr_cntl_w));
	map(0x1d, 0x1d).rw(FUNC(i82801_eth_device::csr_sts_r), FUNC(i82801_eth_device::csr_sts_w));
}

uint32_t cool0 = 0;

uint16_t i82801_eth_device::csr_scb_sts_r()
{
	return m_csr_scb_sts;
}

void i82801_eth_device::csr_scb_sts_w(uint16_t data)
{
	i82801_eth_device::set_irq(data, CLEAR_LINE);
}

uint16_t i82801_eth_device::csr_scb_cmd_r()
{
	return m_csr_scb_cmd;
}

void i82801_eth_device::csr_scb_cmd_w(uint16_t data)
{

	if(data & i82801_eth_device::SCB_CNTL_SW_INT_ASSERT)
	{
		data &= (~i82801_eth_device::SCB_CNTL_SW_INT_ASSERT);

		i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_SW_INT, ASSERT_LINE);
	}

	m_csr_scb_cmd = data;

	i82801_eth_device::cu_scb_execute();
	i82801_eth_device::ru_scb_execute();
}

uint32_t i82801_eth_device::csr_scb_genptr_r()
{
	return m_csr_scb_genptr;
}

void i82801_eth_device::csr_scb_genptr_w(uint32_t data)
{
	m_csr_scb_genptr = data;
}

uint32_t i82801_eth_device::csr_port_r()
{
	return m_csr_port;
}

void i82801_eth_device::csr_port_w(uint32_t data)
{
	m_csr_port = data;

	uint32_t port_pointer = m_csr_port & i82801_eth_device::PORT_POINTER_ADDR_MASK;
	uint8_t port_function = m_csr_port & i82801_eth_device::PORT_FUNC_MASK;

	// EMAC(NOTE): Full reset disabled here, seems to error in WinCE. Some values must not reset.

	switch(port_function)
	{
		case i82801_eth_device::PORT_FUNC_SW_RESET:
			//i82801_eth_device::full_controller_reset();
			m_csr_port = 0;
			break;

		case i82801_eth_device::PORT_FUNC_SELF_TEST:
			m_dma_space->write_dword(port_pointer, i82801_eth_device::PORT_SELF_TEST_GOOD);
			//i82801_eth_device::full_controller_reset();
			m_csr_port = 0;
			break;

		case i82801_eth_device::PORT_FUNC_SEL_RESET:
			i82801_eth_device::partial_controller_reset();
			m_csr_port = 0;
			break;

		default:
			break;
	}
}

uint8_t i82801_eth_device::csr_eeprom_cntl_r()
{
	uint8_t data = 0x00;

	data |= m_eeprom->cs_r() << EEPROM_CS_BITPOS;
	data |= m_eeprom->do_r() << EEPROM_DO_BITPOS;
	data |= m_eeprom->sk_r() << EEPROM_SK_BITPOS;

	return data;
}

void i82801_eth_device::csr_eeprom_cntl_w(uint8_t data)
{

	m_eeprom->cs_w((data & EEPROM_CS) == EEPROM_CS);
	m_eeprom->di_w((data & EEPROM_DI) == EEPROM_DI);
	m_eeprom->sk_w((data & EEPROM_SK) == EEPROM_SK);
}

uint32_t i82801_eth_device::csr_mdi_cntl_r()
{
	return m_csr_mdi_cntl;
}

void i82801_eth_device::csr_mdi_cntl_w(uint32_t data)
{
	uint8_t phy_offset = (data & i82801_eth_device::MDI_PLC_ADDR_MASK) >> i82801_eth_device::MDI_PLC_ADDR_SHIFT;
	uint8_t reg_offset = (data & i82801_eth_device::MDI_PLCREG_ADDR_MASK) >> i82801_eth_device::MDI_PLCREG_ADDR_SHIFT;

	uint8_t opcode = (data & i82801_eth_device::MDI_OPCODE_MASK) >> i82801_eth_device::MDI_OPCODE_SHIFT;

	switch(opcode)
	{
		case mdi_cmd_t::MDI_WRITE:
		{
			data &= (~i82801_eth_device::MDI_READY);

			uint16_t write_data = (data & i82801_eth_device::MDI_DATA_MASK) >> i82801_eth_device::MDI_DATA_SHIFT;

			i82801_eth_device::phy_state_write(phy_offset, reg_offset, write_data);

			data |= i82801_eth_device::MDI_READY;
			i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_MDI_INT, ASSERT_LINE);
			break;
		}

		case mdi_cmd_t::MDI_READ:
		{
			data &= (~i82801_eth_device::MDI_READY);

			uint16_t read_data = i82801_eth_device::phy_state_read(phy_offset, reg_offset);

			data &= (~i82801_eth_device::MDI_DATA_MASK);
			data |= ((read_data << i82801_eth_device::MDI_DATA_SHIFT) & i82801_eth_device::MDI_DATA_MASK);

			data |= i82801_eth_device::MDI_READY;
			i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_MDI_INT, ASSERT_LINE);
			break;
		}

		default:
			break;
	}

	m_csr_mdi_cntl = data;
}

uint32_t i82801_eth_device::csr_dma_bytecnt_r()
{
	return m_csr_dma_bytecnt;
}

void i82801_eth_device::csr_dma_bytecnt_w(uint32_t data)
{
	m_csr_dma_bytecnt = data;
}

uint8_t i82801_eth_device::csr_early_recvint_r()
{
	return m_csr_early_recvint;
}

void i82801_eth_device::csr_early_recvint_w(uint8_t data)
{
	m_csr_early_recvint = data;
}

uint8_t i82801_eth_device::csr_flow_cntl_high_r()
{
	return (m_csr_flow_cntl >> 0x08) & 0xff;
}

void i82801_eth_device::csr_flow_cntl_high_w(uint8_t data)
{
	m_csr_flow_cntl = (m_csr_flow_cntl & 0x00ff) | (data << 0x08);
}

uint8_t i82801_eth_device::csr_flow_cntl_low_r()
{
	return m_csr_flow_cntl & 0xff;
}

void i82801_eth_device::csr_flow_cntl_low_w(uint8_t data)
{
	m_csr_flow_cntl = (m_csr_flow_cntl & 0xff00) | (data << 0x00);
}

uint8_t i82801_eth_device::csr_pmdr_r()
{
	return m_csr_pmdr;
}

void i82801_eth_device::csr_pmdr_w(uint8_t data)
{
	m_csr_pmdr &= (~data);
}

uint8_t i82801_eth_device::csr_cntl_r()
{
	return m_csr_cntl;
}

void i82801_eth_device::csr_cntl_w(uint8_t data)
{

	if(data & i82801_eth_device::GENERAL_CNTL_PLC_RESET)
	{
		i82801_eth_device::set_plc_defaults();
		data &= (~i82801_eth_device::GENERAL_CNTL_PLC_RESET);
	}

	m_csr_cntl = data;
}

uint8_t i82801_eth_device::csr_sts_r()
{
	return m_csr_sts;
}

void i82801_eth_device::csr_sts_w(uint8_t data)
{
	m_csr_sts = data;
}

uint16_t i82801_eth_device::r16_advance(uint32_t* exc_addr)
{
	uint16_t data = m_dma_space->read_word(*exc_addr);

	*exc_addr += 2;

	return data;
}

void i82801_eth_device::w16_advance(uint32_t* exc_addr, uint16_t data)
{
	m_dma_space->write_word(*exc_addr, data);

	*exc_addr += 2;
}

uint32_t i82801_eth_device::r32_advance(uint32_t* exc_addr)
{
	uint32_t data = m_dma_space->read_dword(*exc_addr);

	*exc_addr += 4;

	return data;
}

void i82801_eth_device::w32_advance(uint32_t* exc_addr, uint32_t data)
{
	m_dma_space->write_dword(*exc_addr, data);

	*exc_addr += 4;
}

void i82801_eth_device::set_status(uint32_t* saddr, uint16_t status)
{
	uint16_t cur_status = m_dma_space->read_word(*saddr);

	uint16_t new_status = (cur_status & (~i82801_eth_device::STATUS_WMASK)) | (status & i82801_eth_device::STATUS_WMASK);

	m_dma_space->write_word(*saddr, new_status);
}

void i82801_eth_device::clear_status(uint32_t* saddr, uint16_t status)
{
	uint16_t cur_status = m_dma_space->read_word(*saddr);

	uint16_t new_status = cur_status & (~(status & i82801_eth_device::STATUS_WMASK));

	m_dma_space->write_word(*saddr, new_status);
}

uint16_t i82801_eth_device::copy_from_memory(uint8_t* buffer, uint32_t mem_addr, uint16_t length)
{
	uint16_t copied_length;

	for(copied_length = 0; copied_length < length; copied_length++)
	{
		*buffer = m_dma_space->read_byte(mem_addr++);
		buffer++;
	}

	return copied_length;
}

uint16_t i82801_eth_device::copy_to_memory(uint8_t* buffer, uint32_t mem_addr, uint16_t length)
{
	uint16_t copied_length;

	for(copied_length = 0; copied_length < length; copied_length++)
	{
		m_dma_space->write_byte(mem_addr++, *buffer);
		buffer++;
	}

	return copied_length;
}

i82801_eth_device::cu_state_t i82801_eth_device::get_cu_state()
{
	return m_cu_state;
}

void i82801_eth_device::set_cu_state(cu_state_t state)
{
	m_cu_state = state;

	uint32_t scb_sts = (m_cu_state << i82801_eth_device::SCB_STATUS_CU_SHIFT) & i82801_eth_device::SCB_STATUS_CU_MASK;

	m_csr_scb_sts = scb_sts | (m_csr_scb_sts & (~i82801_eth_device::SCB_STATUS_CU_MASK));
}

void i82801_eth_device::cu_set_hpq_next_addr(uint32_t offset_addr)
{
	if(offset_addr != i82801_eth_device::NULL_POINTER)
		m_cbl_hpq_nblk_addr = m_cbl_base_addr + offset_addr;
	else
		m_cbl_hpq_nblk_addr = i82801_eth_device::NULL_POINTER;
}

void i82801_eth_device::cu_set_lpq_next_addr(uint32_t offset_addr)
{
	if(offset_addr != i82801_eth_device::NULL_POINTER)
		m_cbl_lpq_nblk_addr = m_cbl_base_addr + offset_addr;
	else
		m_cbl_lpq_nblk_addr = i82801_eth_device::NULL_POINTER;
}

void i82801_eth_device::cu_execute_next(cu_state_t state)
{
	if(m_cbl_hpq_nblk_addr != i82801_eth_device::NULL_POINTER || m_cbl_lpq_nblk_addr != i82801_eth_device::NULL_POINTER)
	{
		i82801_eth_device::set_cu_state(state);

		m_cu_action_timer->enable(true);
		m_cu_action_timer->adjust(i82801_eth_device::CU_ACTION_RATE);
	}
	else
	{
		i82801_eth_device::cu_execute_stop();
	}
}

void i82801_eth_device::cu_execute_pause()
{
	i82801_eth_device::set_cu_state(cu_state_t::CU_SUSPENDED);

	m_cu_action_timer->enable(false);
	m_cu_action_timer->reset();
}

void i82801_eth_device::cu_execute_stop()
{
	i82801_eth_device::set_cu_state(cu_state_t::CU_IDLE);

	if(i82801_eth_device::get_cu_state() == cu_state_t::CU_HPQ_ACTIVE)
		m_cbl_hpq_nblk_addr = i82801_eth_device::NULL_POINTER;
	else
		m_cbl_lpq_nblk_addr = i82801_eth_device::NULL_POINTER;

	m_cbl_cblk_addr = i82801_eth_device::NULL_POINTER;
	m_cbl_cexc_addr = i82801_eth_device::NULL_POINTER;

	m_cu_frame_len = 0;
	
	m_cu_action_timer->enable(false);
	m_cu_action_timer->reset();
}

void i82801_eth_device::cu_scb_execute()
{
	uint8_t command = (m_csr_scb_cmd & i82801_eth_device::SCB_CNTL_CU_CMD_MASK) >> i82801_eth_device::SCB_CNTL_CU_CMD_SHIFT;

	switch (command)
	{
		case scb_cu_cmd_t::SCP_CUC_START:
			i82801_eth_device::cu_set_lpq_next_addr(m_csr_scb_genptr);
			i82801_eth_device::cu_execute_next(cu_state_t::CU_LPQ_ACTIVE);
			break;

		case scb_cu_cmd_t::SCP_CUC_RESUME:
			if(m_cu_state != cu_state_t::CU_IDLE)
				i82801_eth_device::cu_execute_next(cu_state_t::CU_LPQ_ACTIVE);
			break;

		case scb_cu_cmd_t::SCP_CUC_HPQ_START:
			i82801_eth_device::cu_set_hpq_next_addr(m_csr_scb_genptr);
			i82801_eth_device::cu_execute_next(cu_state_t::CU_HPQ_ACTIVE);
			break;

		case scb_cu_cmd_t::SCP_CUC_HPQ_RESUME:
			if(m_cu_state != cu_state_t::CU_IDLE)
				i82801_eth_device::cu_execute_next(cu_state_t::CU_HPQ_ACTIVE);
			break;

		case scb_cu_cmd_t::SCP_CUC_LOAD_DUMP_COUNTER_ADDR:
			m_csr_scp_dmpptr = m_csr_scb_genptr;
			break;

		case scb_cu_cmd_t::SCP_CUC_DUMP_STAT_COUNTERS:
			m_counters.dma_copy(m_dma_space, m_csr_scp_dmpptr, false);
			break;

		case scb_cu_cmd_t::SCP_CUC_LOAD_BASE:
			m_cbl_base_addr = m_csr_scb_genptr;
			break;

		case scb_cu_cmd_t::SCP_CUC_DUMP_AND_RESET_STAT_COUNTERS:
			m_counters.dma_copy(m_dma_space, m_csr_scp_dmpptr, true);
			break;

		case scb_cu_cmd_t::SCP_CUC_STATIC_RESUME:
			if(m_cu_state != cu_state_t::CU_IDLE)
				i82801_eth_device::cu_execute_next(cu_state_t::CU_LPQ_ACTIVE);
			break;

		case scb_cu_cmd_t::SCP_CUC_NOP:
		default:
			break;
	}

	m_csr_scb_cmd &= (~i82801_eth_device::SCB_CNTL_CU_CMD_MASK);
}
 
TIMER_CALLBACK_MEMBER(i82801_eth_device::cu_cbl_execute)
{
	bool hpq_active = (i82801_eth_device::get_cu_state() == cu_state_t::CU_HPQ_ACTIVE);

	if(hpq_active)
		m_cbl_cblk_addr = m_cbl_hpq_nblk_addr;
	else
		m_cbl_cblk_addr = m_cbl_lpq_nblk_addr;
	m_cbl_cexc_addr = m_cbl_cblk_addr;

	m_cbl_cexc_addr += 2; // Skip over status word
	uint16_t commnd_word = i82801_eth_device::r16_advance(&m_cbl_cexc_addr);
	uint32_t next_offset = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);

	switch (commnd_word & i82801_eth_device::CU_CBL_COMMAND_OP_MASK)
	{
		case cu_cmd_t::CUC_INDIVIDUAL_ADDRESS_SETUP:
			i82801_eth_device::cu_iasetup(commnd_word);
			break;

		case cu_cmd_t::CUC_MULTICAST_SETUP:
			i82801_eth_device::cu_mcsetup(commnd_word);
			break;

		case cu_cmd_t::CUC_CONFIGURE:
			i82801_eth_device::cu_configure(commnd_word);
			break;

		case cu_cmd_t::CUC_TRANSMIT:
			i82801_eth_device::cu_transmit(commnd_word);
			break;

		case cu_cmd_t::CUC_LOAD_MICROCODE:
			i82801_eth_device::cu_load_microcode(commnd_word);
			break;

		case cu_cmd_t::CUC_DUMP:
			i82801_eth_device::cu_dump(commnd_word);
			break;

		case cu_cmd_t::CUC_DIAGNOSE:
			i82801_eth_device::cu_diagnose(commnd_word);
			break;

		case cu_cmd_t::CUC_NOP:
			i82801_eth_device::cu_nop(commnd_word);
			break;

		default:
			break;
	}

	if(commnd_word & i82801_eth_device::CU_CBL_COMMAND_INT)
		i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_CU_DONE_INT, ASSERT_LINE);

	bool went_inactive = false;

	if(hpq_active)
		i82801_eth_device::cu_set_hpq_next_addr(next_offset);
	else
		i82801_eth_device::cu_set_lpq_next_addr(next_offset);

	if(commnd_word & i82801_eth_device::CU_CBL_COMMAND_LAST)
	{
		i82801_eth_device::cu_execute_stop();

		went_inactive = true;
	}
	else if(commnd_word & i82801_eth_device::CU_CBL_COMMAND_SUSPEND)
	{

		i82801_eth_device::cu_execute_pause();

		went_inactive = true;
	}

	if(went_inactive)
	{
		if(hpq_active && m_cbl_lpq_nblk_addr != i82801_eth_device::NULL_POINTER)
		{
			i82801_eth_device::cu_execute_next(cu_state_t::CU_LPQ_ACTIVE);
		}
		else
		{
			i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_CU_NA_INT, ASSERT_LINE);
		}
	}
	else
	{
		if(hpq_active)
			i82801_eth_device::cu_execute_next(cu_state_t::CU_HPQ_ACTIVE);
		else
			i82801_eth_device::cu_execute_next(cu_state_t::CU_LPQ_ACTIVE);
	}
}

void i82801_eth_device::cu_iasetup(uint32_t commnd_word)
{
	uint8_t mac[6];

	uint32_t ia0 = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);
	put_u32le(&mac[0], ia0);

	uint32_t ia1 = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);
	put_u16le(&mac[4], ia1 & 0xffff);

	logerror("set MAC address to %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	set_mac(mac);

	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);
}

void i82801_eth_device::cu_mcsetup(uint32_t commnd_word)
{
	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);
}

void i82801_eth_device::cu_configure(uint32_t commnd_word)
{
	uint32_t config0 = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);
	put_u32le(&m_configuration.data[0], config0);

	uint8_t size = m_configuration.size() >> 2;
	for(uint8_t bidx = 1, cidx = 4; bidx < size; bidx++, cidx += 4)
	{
		uint32_t config = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);
		put_u32le(&m_configuration.data[cidx], config);
	}

	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);
}

void i82801_eth_device::cu_transmit(uint32_t commnd_word)
{
	uint32_t tbd_addr = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);
	uint32_t props = i82801_eth_device::r32_advance(&m_cbl_cexc_addr);

	uint16_t tcb_buff_size = (props & i82801_eth_device::CU_TCB_BYTE_COUNT_MASK) >> i82801_eth_device::CU_TCB_BYTE_COUNT_SHIFT;

	m_cu_frame_len = 0;

	if(tbd_addr != i82801_eth_device::NULL_POINTER)
	{
		tbd_addr += m_cbl_base_addr;

		uint8_t tbd_count = (props & i82801_eth_device::CU_TBD_COUNT_MASK) >> i82801_eth_device::CU_TBD_COUNT_SHIFT;

		bool using_cbl_cexc = false;
		uint8_t tbd_idx = 0;
		while(tbd_idx < tbd_count)
		{
			uint32_t txbuf_props = m_dma_space->read_dword(tbd_addr + 4);

			uint16_t txbuf_size = (txbuf_props & i82801_eth_device::CU_TBD_BYTE_COUNT_MASK) >> i82801_eth_device::CU_TBD_BYTE_COUNT_SHIFT;

			// Need to figure out why I need to do this (not documented) but it looks like the TDB array can be after the command block.
			// So we assume it's after the command block if we find a zero sized tx buffer.
			if(txbuf_size == 0 && !using_cbl_cexc)
			{
				tbd_addr = m_cbl_cexc_addr;
				using_cbl_cexc = true;

				// Try again with our new tdb address.
				continue;
			}

			uint32_t txbuf_addr = m_dma_space->read_dword(tbd_addr + 0);

			bool tbd_eol = (txbuf_props & i82801_eth_device::CU_TBD_EOL);

			uint16_t copied_length = i82801_eth_device::copy_from_memory(&m_cu_frame[m_cu_frame_len & (i82801_eth_device::MAX_FRAME_SIZE - 1)], txbuf_addr, txbuf_size);

			m_cu_frame_len += copied_length;

			tbd_addr += 8;
			tbd_idx++;

			if(tbd_eol)
				break;
		}

		if(using_cbl_cexc)
			m_cbl_cexc_addr = tbd_addr;

	}
	else if(tcb_buff_size > 0)
	{
		uint16_t copied_length = i82801_eth_device::copy_from_memory(&m_cu_frame[m_cu_frame_len & (i82801_eth_device::MAX_FRAME_SIZE - 1)], m_cbl_cexc_addr, tcb_buff_size);

		m_cu_frame_len += copied_length;
		m_cbl_cexc_addr += copied_length;
	}

	if(m_cu_frame_len >= i82801_eth_device::MIN_FRAME_SIZE)
	{
		bool insert_mac = !(m_configuration.nasi() || (commnd_word & CU_CBL_COMMAND_RAW));
		bool insert_crc = !(commnd_word & CU_CBL_COMMAND_RAW);

		if(insert_mac)
		{
			const std::array<u8, 6> &mac = get_mac();

			std::copy_n(std::begin(mac), std::size(mac), std::begin(m_cu_frame) + 6);
		}

		if(insert_crc)
		{
			uint32_t crc = util::crc32_creator::simple(m_cu_frame, m_cu_frame_len);

			put_u32le(&m_cu_frame[(m_cu_frame_len & (i82801_eth_device::MAX_FRAME_SIZE - 1)) - 4], crc);
		}

		m_counters.tx_cnt++;

		send(&m_cu_frame[0], m_cu_frame_len);

		uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
		i82801_eth_device::set_status(&m_cbl_cblk_addr, status);
	}
}

void i82801_eth_device::cu_load_microcode(uint32_t commnd_word)
{
	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);

	// No need to implement this.
}

void i82801_eth_device::cu_dump(uint32_t commnd_word)
{
	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);

	// EMAC(NOTE): data dump not implemented
}

void i82801_eth_device::cu_diagnose(uint32_t commnd_word)
{
	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);

	// EMAC(NOTE): diagnose not implemented
}

void i82801_eth_device::cu_nop(uint32_t commnd_word)
{
	uint16_t status = i82801_eth_device::CU_CBL_STATUS_COMPLETE | i82801_eth_device::CU_CBL_STATUS_OK;
	i82801_eth_device::set_status(&m_cbl_cblk_addr, status);
}

i82801_eth_device::ru_state_t i82801_eth_device::get_ru_state()
{
	return m_ru_state;
}

void i82801_eth_device::set_ru_state(ru_state_t state)
{
	m_ru_state = state;

	uint32_t scb_sts = (m_ru_state << i82801_eth_device::SCB_STATUS_RU_SHIFT) & i82801_eth_device::SCB_STATUS_RU_MASK;

	m_csr_scb_sts = scb_sts | (m_csr_scb_sts & (~i82801_eth_device::SCB_STATUS_RU_MASK));
}

void i82801_eth_device::set_rfd_buffer_props(uint32_t* saddr, uint16_t written_size, bool eof)
{
	uint32_t cur_bprops = m_dma_space->read_dword(*saddr + 12);

	uint32_t new_bprops = 0x0000;

	new_bprops |= (written_size << i82801_eth_device::RU_RFD_BPROPS_USED_SIZE_SHIFT) & i82801_eth_device::RU_RFD_BPROPS_USED_SIZE_MASK;
	new_bprops |= i82801_eth_device::RU_RFD_BPROPS_SIZE_WRITTEN;
	if(eof)
		new_bprops |= i82801_eth_device::RU_RFD_BPROPS_EOF;
	new_bprops &= i82801_eth_device::RU_RFD_BPROPS_WMASK;

	new_bprops |= cur_bprops & (~i82801_eth_device::RU_RFD_BPROPS_WMASK);

	m_dma_space->write_dword(*saddr + 12, new_bprops);
}

void i82801_eth_device::ru_set_next_addr(uint32_t offset_addr)
{
	if(offset_addr != i82801_eth_device::NULL_POINTER)
		m_rfd_nfrm_addr = m_rfd_base_addr + offset_addr;
	else
		m_rfd_nfrm_addr = i82801_eth_device::NULL_POINTER;
}

void i82801_eth_device::ru_execute_wait()
{
	ru_state_t state;

	if(m_rfd_nfrm_addr == i82801_eth_device::NULL_POINTER)
		state = ru_state_t::RU_READY_NORDBS;
	else
		state = ru_state_t::RU_READY;

	i82801_eth_device::set_ru_state(state);

	m_ru_frame_len = 0;
	m_ru_frame_idx = 0;

	m_ru_action_timer->enable(false);
	m_ru_action_timer->reset();
}

void i82801_eth_device::ru_execute_next()
{
	if(m_rfd_nfrm_addr != i82801_eth_device::NULL_POINTER && m_ru_frame_len > 0)
	{
		i82801_eth_device::set_ru_state(ru_state_t::RU_ACTIVE);

		m_ru_action_timer->enable(true);
		m_ru_action_timer->adjust(i82801_eth_device::RU_ACTION_RATE);
	}
	else
	{
		i82801_eth_device::ru_execute_stop();
	}
}

void i82801_eth_device::ru_execute_pause()
{
	ru_state_t state;

	if(m_rfd_nfrm_addr == i82801_eth_device::NULL_POINTER)
		state = ru_state_t::RU_SUSPENDED_NORDBS;
	else
		state = ru_state_t::RU_SUSPENDED;

	i82801_eth_device::set_ru_state(state);

	m_ru_action_timer->enable(false);
	m_ru_action_timer->reset();

	i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_RU_NR_INT, ASSERT_LINE);
}

void i82801_eth_device::ru_execute_stop()
{
	ru_state_t state;

	if(m_ru_frame_idx < m_ru_frame_len)
	{
		if(m_rfd_nfrm_addr == i82801_eth_device::NULL_POINTER)
			state = ru_state_t::RU_OUTOFMEM_NORDBS;
		else
			state = ru_state_t::RU_OUTOFMEM;
	}
	else
	{
		state = ru_state_t::RU_IDLE;
	}

	i82801_eth_device::set_ru_state(state);

	m_rfd_nfrm_addr = i82801_eth_device::NULL_POINTER;
	m_rfd_cfrm_addr = i82801_eth_device::NULL_POINTER;
	m_rfd_cexc_addr = i82801_eth_device::NULL_POINTER;

	m_ru_frame_len = 0;
	m_ru_frame_idx  = 0;

	m_ru_action_timer->enable(false);
	m_ru_action_timer->reset();

	i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_RU_NR_INT, ASSERT_LINE);
}

void i82801_eth_device::ru_complete()
{
	i82801_eth_device::ru_execute_wait();

	i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_RU_DONE_INT, ASSERT_LINE);
}

void i82801_eth_device::ru_scb_execute()
{
	uint8_t command = (m_csr_scb_cmd & i82801_eth_device::SCB_CNTL_RU_CMD_MASK) >> i82801_eth_device::SCB_CNTL_RU_CMD_SHIFT;

	switch (command)
	{
		case scb_ru_cmd_t::SCP_RUC_START:
			i82801_eth_device::ru_set_next_addr(m_csr_scb_genptr);
			i82801_eth_device::ru_execute_wait();
			break;

		case scb_ru_cmd_t::SCP_RUC_RESUME:
			if(m_ru_state == ru_state_t::RU_SUSPENDED || m_ru_state == ru_state_t::RU_IDLE)
				i82801_eth_device::ru_execute_next();
			break;

		case scb_ru_cmd_t::SCP_RUC_RCV_DMA_REDIRECT:
			break;

		case scb_ru_cmd_t::SCP_RUC_ABORT:
			i82801_eth_device::ru_execute_stop();
			break;

		case scb_ru_cmd_t::SCP_RUC_LOAD_HEADER_DATA_SIZE:
			break;

		case scb_ru_cmd_t::SCP_RUC_LOAD_BASE:
			m_rfd_base_addr = m_csr_scb_genptr;
			break;

		case scb_ru_cmd_t::SCP_RUC_RBD_RESUME:
			break;

		case scb_ru_cmd_t::SCP_RUC_NOP:
		default:
			break;
	}

	m_csr_scb_cmd &= (~i82801_eth_device::SCB_CNTL_RU_CMD_MASK);
}

TIMER_CALLBACK_MEMBER(i82801_eth_device::ru_rfd_execute)
{

	m_rfd_cfrm_addr = m_rfd_nfrm_addr;
	m_rfd_cexc_addr = m_rfd_cfrm_addr;

	m_rfd_cexc_addr += 2; // Skip over status word
	uint16_t commnd_word = i82801_eth_device::r16_advance(&m_rfd_cexc_addr);
	uint32_t next_offset = i82801_eth_device::r32_advance(&m_rfd_cexc_addr);
	m_rfd_cexc_addr += 4; // Skip over reserved section.
	uint32_t buffer_props = i82801_eth_device::r32_advance(&m_rfd_cexc_addr);
	uint32_t buffer_size = (buffer_props & i82801_eth_device::RU_RFD_BPROPS_AVAIL_SIZE_MASK) >> i82801_eth_device::RU_RFD_BPROPS_AVAIL_SIZE_SHIFT;

	uint16_t result = 0x0000;
	uint16_t written_size = 0x0000;

	uint32_t write_size = std::min(m_ru_frame_len, buffer_size);

	written_size = i82801_eth_device::copy_to_memory(&m_ru_frame[m_ru_frame_idx & (i82801_eth_device::MAX_FRAME_SIZE - 1)], m_rfd_cexc_addr, write_size);

	m_ru_frame_idx += written_size;

	bool eof = m_ru_frame_idx >= m_ru_frame_len;

	if(!eof && (next_offset == i82801_eth_device::NULL_POINTER || buffer_size == 0))
		result |= i82801_eth_device::RU_RFD_RESULT_OVERFLOW;

	uint16_t status = i82801_eth_device::RU_RFD_STATUS_COMPLETE | i82801_eth_device::RU_RFD_STATUS_OK;
	status |= ((result << i82801_eth_device::RU_RFD_STATUS_RESULT_SHIFT) & i82801_eth_device::RU_RFD_STATUS_RESULT_MASK);
	i82801_eth_device::set_status(&m_rfd_cfrm_addr, status);

	i82801_eth_device::set_rfd_buffer_props(&m_rfd_cfrm_addr, written_size, eof);

	i82801_eth_device::ru_set_next_addr(next_offset);

	if(commnd_word & i82801_eth_device::RU_RFD_COMMAND_LAST || (result & i82801_eth_device::RU_RFD_RESULT_OVERFLOW))
		i82801_eth_device::ru_execute_stop();
	else if(commnd_word & i82801_eth_device::RU_RFD_COMMAND_SUSPEND)
		i82801_eth_device::ru_execute_pause();
	else if(!eof)
		i82801_eth_device::ru_execute_next();
	else
		i82801_eth_device::ru_complete();
}

void i82801_eth_device::set_irq(uint32_t mask, int state)
{
	if (state)
		m_csr_scb_sts |= mask;
	else
		m_csr_scb_sts &= (~mask);

	bool irq_enabled = false;

	if(mask & i82801_eth_device::SCB_STATUS_CU_DONE_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_CU_DONE_INT_MASK);
	if(mask & i82801_eth_device::SCB_STATUS_RU_DONE_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_RU_DONE_INT_MASK);
	if(mask & i82801_eth_device::SCB_STATUS_CU_NA_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_CU_NA_INT_MASK);
	if(mask & i82801_eth_device::SCB_STATUS_RU_NR_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_RU_NR_INT_MASK);
	if(mask & i82801_eth_device::SCB_STATUS_MDI_INT)
		irq_enabled |= (m_csr_mdi_cntl & MDI_INT_EN);
	if(mask & i82801_eth_device::SCB_STATUS_SW_INT)
		irq_enabled |= true;
	if(mask & i82801_eth_device::SCB_STATUS_EARLY_RECV_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_EARLY_RECV_INT_MASK);
	if(mask & i82801_eth_device::SCB_STATUS_FCNTL_PAUSE_INT)
		irq_enabled |= !(m_csr_scb_cmd & SCB_CNTL_FCNTL_PAUSE_INT_MASK);

	irq_enabled &= !(m_csr_scb_cmd & SCB_CNTL_INTA_DISABLE);

	if (irq_enabled)
		m_pirq_cb(m_pirq_pin, state);
}

int i82801_eth_device::recv_start_cb(uint8_t *frame, int frame_len)
{

	if(m_ru_state == ru_state_t::RU_READY)
	{
		if(frame_len > 0)
		{
			if(frame_len < i82801_eth_device::MAX_FRAME_SIZE)
			{
				i82801_eth_device::set_irq(i82801_eth_device::SCB_STATUS_EARLY_RECV_INT, ASSERT_LINE);

				m_ru_frame_len = frame_len;

				std::copy_n(frame, m_ru_frame_len, std::begin(m_ru_frame));

				i82801_eth_device::ru_execute_next();

				m_counters.rx_cnt++;

				return m_ru_frame_len;
			}
		}
	}

	return 0;
}

void i82801_eth_device::send_complete_cb(int result)
{
	//
}
