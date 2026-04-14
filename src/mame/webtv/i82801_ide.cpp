// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82801_ide.h"

DEFINE_DEVICE_TYPE(I82801_IDE, i82801_ide_device, "i82801_ide", "i82801 ICH4 IDE")

i82801_ide_device::i82801_ide_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_device(mconfig, I82801_IDE, tag, owner, clock),
	m_ide(*this, "ide%d", 0),
	m_bus_master_space(*this, ":maincpu", AS_PROGRAM),
	m_ide0_lirq_cb(*this),
	m_ide1_lirq_cb(*this),
	m_pirq_cb(*this),
	m_ssid_w_cb(*this)
{
	intr_line = 0x00;
	intr_pin = i82801_lpc_device::INT_PIN_A;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_C;
}

void i82801_ide_device::device_start()
{
	pci_device::device_start();

	i82801_ide_device::set_default_values();
}

void i82801_ide_device::device_reset()
{
	pci_device::device_reset();

	i82801_ide_device::set_default_values();
}

void i82801_ide_device::set_default_values()
{
	status = 0x0280;
	
	m_pcmd_bar = i82801_ide_device::DEFAULT_IDE_IO_BASE;
	m_pcnl_bar = i82801_ide_device::DEFAULT_IDE_IO_BASE;
	m_scmd_bar = i82801_ide_device::DEFAULT_IDE_IO_BASE;
	m_scnl_bar = i82801_ide_device::DEFAULT_IDE_IO_BASE;
	m_bmr_bar = i82801_ide_device::DEFAULT_IDE_IO_BASE;
	m_exbar = 0x00000000;
	m_ide_timp = 0x0000;
	m_ide_tims = 0x0000;
	m_sidetim = 0x00;
	m_sdmac = 0x00;
	m_sdmatim = 0x0000;
	m_ide_config = 0x00000000;
	std::fill(std::begin(exp), std::end(exp), 0);
}

void i82801_ide_device::device_add_mconfig(machine_config &config)
{
	BUS_MASTER_IDE_CONTROLLER(config, m_ide[0]);
	m_ide[0]->options(ata_devices, i0_mdflt, i0_sdflt, i0_fixed);
	m_ide[0]->irq_handler().set(FUNC(i82801_ide_device::set_ide0_irq));
	m_ide[0]->dmarq_handler().set(FUNC(i82801_ide_device::set_ide0_irq));
	m_ide[0]->set_bus_master_space(m_bus_master_space);

	BUS_MASTER_IDE_CONTROLLER(config, m_ide[1]);
	m_ide[1]->options(ata_devices, i1_mdflt, i1_sdflt, i1_fixed);
	m_ide[1]->irq_handler().set(FUNC(i82801_ide_device::set_ide1_irq));
	m_ide[1]->dmarq_handler().set(FUNC(i82801_ide_device::set_ide1_irq));
	m_ide[1]->set_bus_master_space(m_bus_master_space);
}

void i82801_ide_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	io_space->install_device(0x0000, 0xffff, *this, &i82801_ide_device::io_map);

	if(command & PCI_SPACE_MEM_EN)
	{
		if(m_exbar != 0x00000000 && (0xffffffff - m_exbar) > i82801_ide_device::IDE_EXBAR_SIZE)
			memory_space->install_ram(m_exbar, m_exbar + (i82801_ide_device::IDE_EXBAR_SIZE - 1), &exp[0x000/4]);
	}

}

void i82801_ide_device::config_map(address_map &map)
{
	pci_device::config_map(map);

	map(0x09, 0x09).w(FUNC(i82801_ide_device::pi_w));
	map(0x10, 0x13).rw(FUNC(i82801_ide_device::pcmd_bar_r), FUNC(i82801_ide_device::pcmd_bar_w));
	map(0x14, 0x17).rw(FUNC(i82801_ide_device::pcnl_bar_r), FUNC(i82801_ide_device::pcnl_bar_w));
	map(0x18, 0x1b).rw(FUNC(i82801_ide_device::scmd_bar_r), FUNC(i82801_ide_device::scmd_bar_w));
	map(0x1c, 0x1f).rw(FUNC(i82801_ide_device::scnl_bar_r), FUNC(i82801_ide_device::scnl_bar_w));
	map(0x20, 0x23).rw(FUNC(i82801_ide_device::bmr_bar_r), FUNC(i82801_ide_device::bmr_bar_w));
	map(0x24, 0x27).rw(FUNC(i82801_ide_device::exbar_r), FUNC(i82801_ide_device::exbar_w));
	map(0x2c, 0x2d).w(FUNC(i82801_ide_device::subvendor_w));
	map(0x2e, 0x2f).w(FUNC(i82801_ide_device::subsystem_w));
	map(0x40, 0x41).rw(FUNC(i82801_ide_device::ide_timp_r), FUNC(i82801_ide_device::ide_timp_w));
	map(0x42, 0x43).rw(FUNC(i82801_ide_device::ide_tims_r), FUNC(i82801_ide_device::ide_tims_w));
	map(0x44, 0x44).rw(FUNC(i82801_ide_device::sidetim_r), FUNC(i82801_ide_device::sidetim_w));
	map(0x48, 0x48).rw(FUNC(i82801_ide_device::sdmac_r), FUNC(i82801_ide_device::sdmac_w));
	map(0x4a, 0x4b).rw(FUNC(i82801_ide_device::sdmatim_r), FUNC(i82801_ide_device::sdmatim_w));
	map(0x54, 0x57).rw(FUNC(i82801_ide_device::ide_config_r), FUNC(i82801_ide_device::ide_config_w));
}

void i82801_ide_device::io_map(address_map &map)
{
	if(command & i82801_ide_device::PCI_SPACE_IO_EN)
	{
		uint16_t bmr_bar = (m_bmr_bar & i82801_ide_device::IDE_IO_BASE_MASK);

		if(m_ide_timp & i82801_ide_device::IDE_TIM_DECODE_ENABLE)
		{
			uint16_t pbmr_bar = (bmr_bar != 0x0000) ? bmr_bar + i82801_ide_device::IDE_PBMR_OFFSET : 0x0000;

			if(pclass & IDE_PI_POP_MODE_SEL_NATIVE)
			{
				uint16_t pcmd_bar = (m_pcmd_bar & i82801_ide_device::IDE_IO_BASE_MASK);
				uint16_t pcnl_bar = (m_pcnl_bar & i82801_ide_device::IDE_IO_BASE_MASK);

				if(i0_mdflt == nullptr && i0_sdflt == nullptr)
					i82801_ide_device::map_empty_bus(map, false, pcmd_bar, pcnl_bar, pbmr_bar);
				else
					i82801_ide_device::map_ide_bus(map, 0, false, pcmd_bar, pcnl_bar, pbmr_bar);
			}
			else
			{
				if(i0_mdflt == nullptr && i0_sdflt == nullptr)
					i82801_ide_device::map_empty_bus(map, true, i82801_ide_device::IDE_LEGACY_PCCMD_BASE, i82801_ide_device::IDE_LEGACY_PCCNL_BASE, pbmr_bar);
				else
					i82801_ide_device::map_ide_bus(map, 0, true, i82801_ide_device::IDE_LEGACY_PCCMD_BASE, i82801_ide_device::IDE_LEGACY_PCCNL_BASE, pbmr_bar);
			}
		}

		if(m_ide_tims & i82801_ide_device::IDE_TIM_DECODE_ENABLE)
		{
			uint16_t sbmr_bar = (bmr_bar != 0x0000) ? bmr_bar + i82801_ide_device::IDE_SBMR_OFFSET : 0x0000;

			if(pclass & IDE_PI_SOP_MODE_SEL_NATIVE)
			{
				uint16_t scmd_bar = (m_scmd_bar & i82801_ide_device::IDE_IO_BASE_MASK);
				uint16_t scnl_bar = (m_scnl_bar & i82801_ide_device::IDE_IO_BASE_MASK);

				if(i1_mdflt == nullptr && i1_sdflt == nullptr)
					i82801_ide_device::map_empty_bus(map, false, scmd_bar, scnl_bar, sbmr_bar);
				else
					i82801_ide_device::map_ide_bus(map, 1, false, scmd_bar, scnl_bar, sbmr_bar);
			}
			else
			{
				if(i1_mdflt == nullptr && i1_sdflt == nullptr)
					i82801_ide_device::map_empty_bus(map, true, i82801_ide_device::IDE_LEGACY_SCCMD_BASE, i82801_ide_device::IDE_LEGACY_SCCNL_BASE, sbmr_bar);
				else
					i82801_ide_device::map_ide_bus(map, 1, true, i82801_ide_device::IDE_LEGACY_SCCMD_BASE, i82801_ide_device::IDE_LEGACY_SCCNL_BASE, sbmr_bar);
			}
		}
	}

}

void i82801_ide_device::map_ide_bus(address_map &map, uint8_t ide_bus_index, bool is_legacy, uint16_t cmd_io_addr, uint16_t cnl_io_addr, uint16_t bmr_io_addr)
{
	auto ide_bus = m_ide[ide_bus_index];


	if(cmd_io_addr != 0x0000)
		map(cmd_io_addr, cmd_io_addr + (i82801_ide_device::IDE_CMD_SIZE - 1)).rw(ide_bus, FUNC(bus_master_ide_controller_device::read_cs0), FUNC(bus_master_ide_controller_device::write_cs0));

	if(cnl_io_addr != 0x0000)
	{
		uint32_t cnl_io_reg6_addr = cnl_io_addr + 2;
		map(cnl_io_reg6_addr, cnl_io_reg6_addr).lrw8(
			NAME((
				[this, ide_bus_index] (offs_t offset) -> uint8_t {
					return m_ide[ide_bus_index]->read_cs1(1, 0xff << 0x10) >> 0x10;
				}
			)),
			NAME((
				[this, ide_bus_index] (offs_t offset, uint8_t data) {
					m_ide[ide_bus_index]->write_cs1(1, data << 0x10, 0xff << 0x10);
				}
			))
		);

		if(!is_legacy)
		{
			uint32_t cnl_io_reg7_addr = cnl_io_addr + 3;
			map(cnl_io_reg7_addr, cnl_io_reg7_addr).lrw8(
				NAME((
					[this, ide_bus_index] (offs_t offset) -> uint8_t {
						return m_ide[ide_bus_index]->read_cs1(1, 0xff << 0x18) >> 0x18;
					}
				)),
				NAME((
					[this, ide_bus_index] (offs_t offset, uint8_t data) {
						m_ide[ide_bus_index]->write_cs1(1, data << 0x18, 0xff << 0x18);
					}
				))
			);
		}
	}

	if(bmr_io_addr != 0x0000)
		map(bmr_io_addr, bmr_io_addr + (i82801_ide_device::IDE_BMR_SIZE - 1)).rw(ide_bus, FUNC(bus_master_ide_controller_device::bmdma_r), FUNC(bus_master_ide_controller_device::bmdma_w));
}

void i82801_ide_device::map_empty_bus(address_map &map, bool is_legacy, uint16_t cmd_io_addr, uint16_t cnl_io_addr, uint16_t bmr_io_addr)
{

	if(cmd_io_addr != 0x0000)
		map(cmd_io_addr, cmd_io_addr + (i82801_ide_device::IDE_CMD_SIZE - 1)).rw(FUNC(i82801_ide_device::empty_read), FUNC(i82801_ide_device::empty_write));

	if(cnl_io_addr != 0x0000)
	{
		uint32_t cnl_io_reg6_addr = cnl_io_addr + 2;
		map(cnl_io_reg6_addr, cnl_io_reg6_addr).rw(FUNC(i82801_ide_device::empty_read), FUNC(i82801_ide_device::empty_write));

		if(!is_legacy)
		{
			uint32_t cnl_io_reg7_addr = cnl_io_addr + 3;
			map(cnl_io_reg7_addr, cnl_io_reg7_addr).rw(FUNC(i82801_ide_device::empty_read), FUNC(i82801_ide_device::empty_write));
		}
	}

	if(bmr_io_addr != 0x0000)
		map(bmr_io_addr, bmr_io_addr + (i82801_ide_device::IDE_BMR_SIZE - 1)).rw(FUNC(i82801_ide_device::empty_read), FUNC(i82801_ide_device::empty_write));
}

uint8_t i82801_ide_device::empty_read(offs_t offset)
{
	return 0x00;
}

void i82801_ide_device::empty_write(offs_t offset, uint8_t data)
{
	//
}

void i82801_ide_device::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	pci_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

void i82801_ide_device::set_ide0_irq(int state)
{
	if(pclass & IDE_PI_POP_MODE_SEL_NATIVE)
		m_pirq_cb(m_pirq_pin, state);
	else
		m_ide0_lirq_cb(state);
}

void i82801_ide_device::set_ide1_irq(int state)
{
	if(pclass & IDE_PI_POP_MODE_SEL_NATIVE)
		m_pirq_cb(m_pirq_pin, state);
	else
		m_ide1_lirq_cb(state);
}

void i82801_ide_device::pi_w(uint8_t data)
{
	bool new_pi_mode = (data ^ pclass) & i82801_ide_device::IDE_PI_WMASK;

	pclass = (pclass & (~i82801_ide_device::IDE_PI_WMASK)) | (data & i82801_ide_device::IDE_PI_WMASK);

	if(new_pi_mode)
		i82801_ide_device::remap_cb();
}

void i82801_ide_device::subvendor_w(uint16_t data)
{
	if(!(subsystem_id & 0xffff0000))
	{
		subsystem_id = (subsystem_id & 0x0000ffff) | data << 0x10;
		m_ssid_w_cb(subsystem_id);
	}
}

void i82801_ide_device::subsystem_w(uint16_t data)
{
	if(!(subsystem_id & 0x0000ffff))
	{
		subsystem_id = (subsystem_id & 0xffff0000) | data;
		m_ssid_w_cb(subsystem_id);
	}
}

uint32_t i82801_ide_device::pcmd_bar_r()
{
	return m_pcmd_bar;
}

void i82801_ide_device::pcmd_bar_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_ide_device::IDE_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_pcmd_bar = (m_pcmd_bar & (~i82801_ide_device::IDE_IO_BASE_MASK)) | iobase;
		i82801_ide_device::remap_cb();
	}
}

uint32_t i82801_ide_device::pcnl_bar_r()
{
	return m_pcnl_bar;
}

void i82801_ide_device::pcnl_bar_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_ide_device::IDE_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_pcnl_bar = (m_pcnl_bar & (~i82801_ide_device::IDE_IO_BASE_MASK)) | iobase;
		i82801_ide_device::remap_cb();
	}
}

uint32_t i82801_ide_device::scmd_bar_r()
{
	return m_scmd_bar;
}

void i82801_ide_device::scmd_bar_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_ide_device::IDE_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_scmd_bar = (m_scmd_bar & (~i82801_ide_device::IDE_IO_BASE_MASK)) | iobase;
		i82801_ide_device::remap_cb();
	}
}

uint32_t i82801_ide_device::scnl_bar_r()
{
	return m_scnl_bar;
}

void i82801_ide_device::scnl_bar_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_ide_device::IDE_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_scnl_bar = (m_scnl_bar & (~i82801_ide_device::IDE_IO_BASE_MASK)) | iobase;
		i82801_ide_device::remap_cb();
	}
}

uint32_t i82801_ide_device::bmr_bar_r()
{
	return m_bmr_bar;
}

void i82801_ide_device::bmr_bar_w(uint32_t data)
{
	uint32_t iobase = (data & i82801_ide_device::IDE_IO_BASE_MASK);
	if(iobase != 0x00000000)
	{
		m_bmr_bar = (m_bmr_bar & (~i82801_ide_device::IDE_IO_BASE_MASK)) | iobase;
		i82801_ide_device::remap_cb();
	}
}

uint32_t i82801_ide_device::exbar_r()
{
	return m_exbar;
}

void i82801_ide_device::exbar_w(uint32_t data)
{
	uint32_t exbar = (data & i82801_ide_device::IDE_EXBAR_MASK);
	if(exbar != 0x00000000)
	{
		m_exbar = exbar;
		i82801_ide_device::remap_cb();
	}
}

uint16_t i82801_ide_device::ide_timp_r()
{
	return m_ide_timp;
}

void i82801_ide_device::ide_timp_w(uint16_t data)
{
	m_ide_timp = data;
}

uint16_t i82801_ide_device::ide_tims_r()
{
	return m_ide_tims;
}

void i82801_ide_device::ide_tims_w(uint16_t data)
{
	m_ide_tims = data;
}

uint8_t i82801_ide_device::sidetim_r()
{
	return m_sidetim;
}

void i82801_ide_device::sidetim_w(uint8_t data)
{
	m_sidetim = data;
}

uint8_t i82801_ide_device::sdmac_r()
{
	return m_sdmac;
}

void i82801_ide_device::sdmac_w(uint8_t data)
{
	m_sdmac = data;
}

uint16_t i82801_ide_device::sdmatim_r()
{
	return m_sdmatim;
}

void i82801_ide_device::sdmatim_w(uint16_t data)
{
	m_sdmatim = data;
}

uint32_t i82801_ide_device::ide_config_r()
{
	return m_ide_config;
}

void i82801_ide_device::ide_config_w(uint32_t data)
{
	m_ide_config = data;
}
