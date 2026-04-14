// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82801_lpc.h"

DEFINE_DEVICE_TYPE(I82801_LPC, i82801_lpc_device, "i82801_lpc", "i82801 ICH4 ISA/LPC southbridge")

i82801_lpc_device::i82801_lpc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_device(mconfig, I82801_LPC, tag, owner, clock),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_pic_master(*this, "master_pic"),
	m_pic_slave(*this, "slave_pic"),
	m_dmac_master(*this, "dma1"),
	m_dmac_slave(*this, "dma2"),
	m_pit(*this, "pit"),
	m_bios_region(*this, DEVICE_SELF),
	m_rtc(*this, "rtc"),
	m_lpcbus(*this, "lpcbus"),
	m_acpi(*this, "acpi"),
	m_pc_sound(*this, "pc_sound")
{
	m_dummydelay_ioaddr = 0x0000;
}

void i82801_lpc_device::device_start()
{
	pci_device::device_start();

	i82801_lpc_device::set_default_values();
}

void i82801_lpc_device::device_reset()
{
	pci_device::device_reset();

	i82801_lpc_device::set_default_values();
}

void i82801_lpc_device::device_add_mconfig(machine_config &config)
{
	PIC8259(config, m_pic_master, 0);
	m_pic_master->out_int_callback().set_inputline(m_hostcpu, INPUT_LINE_IRQ0);
	m_pic_master->in_sp_callback().set_constant(1);
	m_pic_master->read_slave_ack_callback().set(FUNC(i82801_lpc_device::get_slave_ack));

	PIC8259(config, m_pic_slave, 0);
	m_pic_slave->out_int_callback().set(m_pic_master, FUNC(pic8259_device::ir2_w));
	m_pic_slave->in_sp_callback().set_constant(0);

	AM9517A(config, m_dmac_master, XTAL(14'318'181)/3);
	m_dmac_master->out_hreq_callback().set(m_dmac_slave, FUNC(am9517a_device::dreq0_w));
	m_dmac_master->out_eop_callback().set(FUNC(i82801_lpc_device::dmac_eop_w));
	m_dmac_master->in_memr_callback().set(FUNC(i82801_lpc_device::dmac_master_byte_r));
	m_dmac_master->out_memw_callback().set(FUNC(i82801_lpc_device::dmac_master_byte_w));
	m_dmac_master->in_ior_callback<0>().set(FUNC(i82801_lpc_device::dmac_io0_r));
	m_dmac_master->out_iow_callback<0>().set(FUNC(i82801_lpc_device::dmac_io0_w));
	m_dmac_master->in_ior_callback<1>().set(FUNC(i82801_lpc_device::dmac_io1_r));
	m_dmac_master->out_iow_callback<1>().set(FUNC(i82801_lpc_device::dmac_io1_w));
	m_dmac_master->in_ior_callback<2>().set(FUNC(i82801_lpc_device::dmac_io2_r));
	m_dmac_master->out_iow_callback<2>().set(FUNC(i82801_lpc_device::dmac_io2_w));
	m_dmac_master->in_ior_callback<3>().set(FUNC(i82801_lpc_device::dmac_io3_r));
	m_dmac_master->out_iow_callback<3>().set(FUNC(i82801_lpc_device::dmac_io3_w));
	m_dmac_master->out_dack_callback<0>().set(FUNC(i82801_lpc_device::dmac_dack0_w));
	m_dmac_master->out_dack_callback<1>().set(FUNC(i82801_lpc_device::dmac_dack1_w));
	m_dmac_master->out_dack_callback<2>().set(FUNC(i82801_lpc_device::dmac_dack2_w));
	m_dmac_master->out_dack_callback<3>().set(FUNC(i82801_lpc_device::dmac_dack3_w));

	AM9517A(config, m_dmac_slave, XTAL(14'318'181)/3);
	m_dmac_slave->out_hreq_callback().set(FUNC(i82801_lpc_device::dmac_hreq_w));
	m_dmac_slave->in_memr_callback().set(FUNC(i82801_lpc_device::dmac_slave_byte_r));
	m_dmac_slave->out_memw_callback().set(FUNC(i82801_lpc_device::dmac_slave_byte_w));
	m_dmac_slave->in_ior_callback<1>().set(FUNC(i82801_lpc_device::dmac_io5_r));
	m_dmac_slave->out_iow_callback<1>().set(FUNC(i82801_lpc_device::dmac_io5_w));
	m_dmac_slave->in_ior_callback<2>().set(FUNC(i82801_lpc_device::dmac_io6_r));
	m_dmac_slave->out_iow_callback<2>().set(FUNC(i82801_lpc_device::dmac_io6_w));
	m_dmac_slave->in_ior_callback<3>().set(FUNC(i82801_lpc_device::dmac_io7_r));
	m_dmac_slave->out_iow_callback<3>().set(FUNC(i82801_lpc_device::dmac_io7_w));
	m_dmac_slave->out_dack_callback<0>().set(FUNC(i82801_lpc_device::dmac_dack4_w));
	m_dmac_slave->out_dack_callback<1>().set(FUNC(i82801_lpc_device::dmac_dack5_w));
	m_dmac_slave->out_dack_callback<2>().set(FUNC(i82801_lpc_device::dmac_dack6_w));
	m_dmac_slave->out_dack_callback<3>().set(FUNC(i82801_lpc_device::dmac_dack7_w));

	PIT8254(config, m_pit, 0);
	// Counter 0, System Timer
	m_pit->set_clk<0>(i82801_lpc_device::LPC_8254_CLOCK / 12);
	m_pit->out_handler<0>().set(m_pic_master, FUNC(pic8259_device::ir0_w));
	// Counter 1, DRAM Refresh Request Signal
	m_pit->set_clk<1>(i82801_lpc_device::LPC_8254_CLOCK / 12);
	m_pit->out_handler<1>().set(FUNC(i82801_lpc_device::pit_counter1));
	// Counter 2, Speaker Tone
	m_pit->set_clk<2>(i82801_lpc_device::LPC_8254_CLOCK / 12);
	m_pit->out_handler<2>().set(FUNC(i82801_lpc_device::pit_counter2));

	// ICH4 contains a Motorola MC146818A-compatible real-time clock with 256 bytes of battery-backed RAM
	DS12885EXT(config, m_rtc, i82801_lpc_device::RTC_CLOCK);
	m_rtc->irq().set(m_pic_slave, FUNC(pic8259_device::ir0_w));

	ISA16(config, m_lpcbus, 0);
	m_lpcbus->irq2_callback().set(m_pic_master, FUNC(pic8259_device::ir2_w));
	m_lpcbus->irq3_callback().set(m_pic_master, FUNC(pic8259_device::ir3_w));
	m_lpcbus->irq4_callback().set(m_pic_master, FUNC(pic8259_device::ir4_w));
	m_lpcbus->irq5_callback().set(m_pic_master, FUNC(pic8259_device::ir5_w));
	m_lpcbus->irq6_callback().set(m_pic_master, FUNC(pic8259_device::ir6_w));
	m_lpcbus->irq7_callback().set(m_pic_master, FUNC(pic8259_device::ir7_w));
	m_lpcbus->irq10_callback().set(m_pic_slave, FUNC(pic8259_device::ir2_w));
	m_lpcbus->irq11_callback().set(m_pic_slave, FUNC(pic8259_device::ir3_w));
	m_lpcbus->irq12_callback().set(m_pic_slave, FUNC(pic8259_device::ir4_w));
	m_lpcbus->irq14_callback().set(m_pic_slave, FUNC(pic8259_device::ir6_w));
	m_lpcbus->irq15_callback().set(m_pic_slave, FUNC(pic8259_device::ir7_w));
	m_lpcbus->drq0_callback().set(m_dmac_master, FUNC(am9517a_device::dreq0_w));
	m_lpcbus->drq1_callback().set(m_dmac_master, FUNC(am9517a_device::dreq1_w));
	m_lpcbus->drq2_callback().set(m_dmac_master, FUNC(am9517a_device::dreq2_w));
	m_lpcbus->drq3_callback().set(m_dmac_master, FUNC(am9517a_device::dreq3_w));
	m_lpcbus->drq5_callback().set(m_dmac_slave, FUNC(am9517a_device::dreq1_w));
	m_lpcbus->drq6_callback().set(m_dmac_slave, FUNC(am9517a_device::dreq2_w));
	m_lpcbus->drq7_callback().set(m_dmac_slave, FUNC(am9517a_device::dreq3_w));
	m_lpcbus->iochck_callback().set(FUNC(i82801_lpc_device::isa_iochck_w));

	LPC_ACPI(config, m_acpi, 0);

	SPEAKER(config, "pc_speaker").front_center();
	SPEAKER_SOUND(config, m_pc_sound).add_route(ALL_OUTPUTS, "pc_speaker", 0.75);
}

void i82801_lpc_device::device_config_complete()
{
	pci_device::device_config_complete();

	auto lpcbus = m_lpcbus.finder_target();
	lpcbus.first.subdevice<isa16_device>(lpcbus.second)->set_memspace(m_hostcpu, AS_PROGRAM);
	lpcbus.first.subdevice<isa16_device>(lpcbus.second)->set_iospace(m_hostcpu, AS_IO);
}

void i82801_lpc_device::set_default_values()
{
	m_tco_cntl = 0x00;
	m_serirq_cntl = 0x10;
	std::fill(std::begin(m_pirq_rout), std::end(m_pirq_rout), PIRQ_NOT_ROUTED);
	m_d31_err_cfg = 0x00;
	m_d31_err_sts = 0x00;
	m_pci_dma_cfg = 0x0000;
	m_func_dis = 0x0080;
	m_siu_config_port = 0;
	m_siu_config_state = 0;
	m_gen_pmcon_1 = 0;
	m_gen_pmcon_2 = 0;
	m_gen_pmcon_3 = 0;
	m_apm_cnt = 0;
	m_apm_sts = 0;
	m_gpi_rout = 0;
	m_mon_fwd_en = 0;
	std::fill(std::begin(m_mon_trp_rng), std::end(m_mon_trp_rng), 0);
	m_mon_trp_msk = 0;
	m_nmi_sc = 0;
	m_gen_sta = 0x00;
	m_eisa_irq_mode = 0x0000;
	m_gpio_use_sel1 = 0x1a003180;
	m_gpio_io_sel1 = 0x0000ffff;
	m_gpio_lvl1 = 0x1f1f0000;
	m_gpio_ttl = 0x06630000;
	m_gpio_blink = 0x00000000;
	m_gpio_inv = 0x00000000;
	m_gpio_use_sel2 = 0x00000000;
	m_gpio_io_sel2 = 0x00000000;
	m_gpio_lvl2 = 0x00000fff;
	std::fill(std::begin(m_apic_reg), std::end(m_apic_reg), 0);
	m_apic_io_en = false;
	m_apic_xio_en = false;
	m_apic_local_en = false;
	m_apic_svec = 0x00;
}

void i82801_lpc_device::reset_all_mappings()
{
	pci_device::reset_all_mappings();

	m_dmac_cur_chan = -1;
	m_dmac_cur_eop = 0;
	m_dmac_high_byte = 0x0000;
	std::fill(std::begin(m_dmac_page), std::end(m_dmac_page), 0x00);
	std::fill(std::begin(m_dmac_master_paddr), std::end(m_dmac_master_paddr), 0x00);
	std::fill(std::begin(m_dmac_slave_paddr), std::end(m_dmac_slave_paddr), 0x00);
	m_rtc_index = 0x00;
	m_rtc_conf = 0x00;
	m_pmbase = 0;
	m_acpi_cntl = 0;
	m_gpio_base = GPIO_BASE_IS_IO;
	m_gpio_cntl = 0x00;
	m_back_cntl = 0x0f;
	m_lpc_if_com_range = 0x00;
	m_lpc_if_fdd_lpt_range = 0x00;
	m_lpc_if_sound_range = 0x00;
	m_fwh_dec_en1 = 0xff;
	m_fwh_dec_en2 = 0x0f;
	m_gen1_dec = 0x0000;
	m_lpc_en = 0x0000;
	m_fwh_sel1 = 0x00112233;
	m_gen_cntl = 0x00000080;
}

void i82801_lpc_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_F8_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xfff80000, 0xffffffff, 7);
		i82801_lpc_device::map_bios(memory_space, 0xffb80000, 0xffbfffff, 7);
		i82801_lpc_device::map_bios(memory_space, 0x000e0000, 0x000fffff, 7);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_F0_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xfff00000, 0xfff7ffff, 6);
		i82801_lpc_device::map_bios(memory_space, 0xffb00000, 0xffb7ffff, 6);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_E8_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffe80000, 0xffefffff, 5);
		i82801_lpc_device::map_bios(memory_space, 0xffa80000, 0xffafffff, 5);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_E0_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffe00000, 0xffe7ffff, 4);
		i82801_lpc_device::map_bios(memory_space, 0xffa00000, 0xffa7ffff, 4);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_D8_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffd80000, 0xffdfffff, 3);
		i82801_lpc_device::map_bios(memory_space, 0xff980000, 0xff9fffff, 3);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_D0_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffd00000, 0xffd7ffff, 2);
		i82801_lpc_device::map_bios(memory_space, 0xff900000, 0xff97ffff, 2);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_C8_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffc80000, 0xffcfffff, 1);
		i82801_lpc_device::map_bios(memory_space, 0xff880000, 0xff8fffff, 1);
	}
	if(m_fwh_dec_en1 & i82801_lpc_device::FWH_C0_ENABLE)
	{
		i82801_lpc_device::map_bios(memory_space, 0xffc00000, 0xffc7ffff, 0);
		i82801_lpc_device::map_bios(memory_space, 0xff800000, 0xff87ffff, 0);
	}

	io_space->install_device(0x0000, 0xffff, *this, &i82801_lpc_device::misc_map);

	if(m_acpi_cntl & i82801_lpc_device::ACPI_ENABLE)
		m_acpi->map_device(memory_window_start, memory_window_end, 0, memory_space, io_window_start, io_window_end, m_pmbase, io_space);

	uint16_t gpio_base = (m_gpio_base & GPIO_IO_BASE_MASK);
	if(m_gpio_cntl & i82801_lpc_device::GPIO_ENABLE && gpio_base != 0x0000)
		memory_space->install_device(gpio_base, gpio_base + (i82801_lpc_device::GPIO_REG_MAPS_SIZE - 1), *this, &i82801_lpc_device::gpio_map);

	if(m_apic_io_en)
		memory_space->install_device(i82801_lpc_device::APIC_DEFAULT_BASE, i82801_lpc_device::APIC_DEFAULT_BASE + (i82801_lpc_device::APIC_REG_MAP_SIZE - 1), *this, &i82801_lpc_device::apic_map);

	m_lpcbus->remap(AS_IO, 0x0000, 0xffff);

	// m_lpc_en is not checked here. SuperIO is enabled by the driver and that will handle logical device activation.
}

void i82801_lpc_device::misc_map(address_map &map)
{
	map(0x0000, 0x001f).rw(m_dmac_master, FUNC(am9517a_device::read), FUNC(am9517a_device::write));

	map(0x0020, 0x0021).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0024, 0x0025).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0028, 0x0029).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x002c, 0x002d).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0030, 0x0031).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0034, 0x0035).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0038, 0x0039).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x003c, 0x003d).rw(m_pic_master, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x0040, 0x0047).rw(m_pit, FUNC(pit8254_device::read), FUNC(pit8254_device::write));
	map(0x0061, 0x0061).rw(FUNC(i82801_lpc_device::nmi_sc_r), FUNC(i82801_lpc_device::nmi_sc_w));
	map(0x0070, 0x0073).rw(FUNC(i82801_lpc_device::rtc_nmi_r), FUNC(i82801_lpc_device::rtc_nmi_w));
	map(0x0074, 0x0077).rw(FUNC(i82801_lpc_device::rtc_nmi_r), FUNC(i82801_lpc_device::rtc_nmi_w));
	map(0x0080, 0x009f).rw(FUNC(i82801_lpc_device::dmac_page_r), FUNC(i82801_lpc_device::dmac_page_w));
	map(0x00a0, 0x00a1).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00a4, 0x00a5).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00a8, 0x00a9).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00ac, 0x00ad).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00b0, 0x00b1).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00b4, 0x00b5).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00b8, 0x00b9).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));
	map(0x00bc, 0x00bd).rw(m_pic_slave, FUNC(pic8259_device::read), FUNC(pic8259_device::write));

	map(0x00c0, 0x00df).rw(FUNC(i82801_lpc_device::dmac2_r), FUNC(i82801_lpc_device::dmac2_w));

	// Used in the MSNTV2's POST.
	// The timer counter at *0x9900c is read, I/O 0xeb is written to in a loop 2096 times, then *0x9900c is read again. Timer needs to advance but not more than 10 ticks otherwise a POST error 0x8c is thrown.
	// These appear to be non-existant dummy I/O port addresses used to eat CPU cycles, so we're artificially inducing a delay here.
	if(m_dummydelay_ioaddr != 0x0000)
		map(m_dummydelay_ioaddr, m_dummydelay_ioaddr).rw(FUNC(i82801_lpc_device::dummydelay_r), FUNC(i82801_lpc_device::dummydelay_w));
}

void i82801_lpc_device::gpio_map(address_map &map)
{
	map(0x0000, 0x0003).rw(FUNC(i82801_lpc_device::gpio_use_sel1_r), FUNC(i82801_lpc_device::gpio_use_sel1_w));
	map(0x0004, 0x0007).rw(FUNC(i82801_lpc_device::gpio_io_sel1_r), FUNC(i82801_lpc_device::gpio_io_sel1_w));
	map(0x000c, 0x000f).rw(FUNC(i82801_lpc_device::gpio_lvl1_r), FUNC(i82801_lpc_device::gpio_lvl1_w));
	map(0x0014, 0x0017).rw(FUNC(i82801_lpc_device::gpio_ttl_r), FUNC(i82801_lpc_device::gpio_ttl_w));
	map(0x0018, 0x001b).rw(FUNC(i82801_lpc_device::gpio_blink_r), FUNC(i82801_lpc_device::gpio_blink_w));
	map(0x002c, 0x002f).rw(FUNC(i82801_lpc_device::gpio_inv_r), FUNC(i82801_lpc_device::gpio_inv_w));
	map(0x0030, 0x0033).rw(FUNC(i82801_lpc_device::gpio_use_sel2_r), FUNC(i82801_lpc_device::gpio_use_sel2_w));
	map(0x0034, 0x0037).rw(FUNC(i82801_lpc_device::gpio_io_sel2_r), FUNC(i82801_lpc_device::gpio_io_sel2_w));
	map(0x0038, 0x003b).rw(FUNC(i82801_lpc_device::gpio_lvl2_r), FUNC(i82801_lpc_device::gpio_lvl2_w));
}

void i82801_lpc_device::apic_map(address_map &map)
{
	map(0x0000, 0x03ff).rw(FUNC(i82801_lpc_device::apic_reg_data_r), FUNC(i82801_lpc_device::apic_reg_data_w));
}

void i82801_lpc_device::config_map(address_map &map)
{
	pci_device::config_map(map);

	map(0x40, 0x43).rw(FUNC(i82801_lpc_device::pmbase_r), FUNC(i82801_lpc_device::pmbase_w));
	map(0x44, 0x44).rw(FUNC(i82801_lpc_device::acpi_cntl_r), FUNC(i82801_lpc_device::acpi_cntl_w));
	map(0x4e, 0x4f).rw(FUNC(i82801_lpc_device::bios_cntl_r), FUNC(i82801_lpc_device::bios_cntl_w));
	map(0x54, 0x54).rw(FUNC(i82801_lpc_device::tco_cntl_r), FUNC(i82801_lpc_device::tco_cntl_w));
	map(0x58, 0x5b).rw(FUNC(i82801_lpc_device::gpio_base_r), FUNC(i82801_lpc_device::gpio_base_w));
	map(0x5c, 0x5c).rw(FUNC(i82801_lpc_device::gpio_cntl_r), FUNC(i82801_lpc_device::gpio_cntl_w));
	map(0x60, 0x63).rw(FUNC(i82801_lpc_device::pirq_rout_r), FUNC(i82801_lpc_device::pirq_rout_w));
	map(0x64, 0x64).rw(FUNC(i82801_lpc_device::serirq_cntl_r), FUNC(i82801_lpc_device::serirq_cntl_w));
	map(0x68, 0x6b).rw(FUNC(i82801_lpc_device::pirq2_rout_r), FUNC(i82801_lpc_device::pirq2_rout_w));
	map(0x88, 0x88).rw(FUNC(i82801_lpc_device::d31_err_cfg_r), FUNC(i82801_lpc_device::d31_err_cfg_w));
	map(0x8a, 0x8a).rw(FUNC(i82801_lpc_device::d31_err_sts_r), FUNC(i82801_lpc_device::d31_err_sts_w));
	map(0x90, 0x91).rw(FUNC(i82801_lpc_device::pci_dma_cfg_r), FUNC(i82801_lpc_device::pci_dma_cfg_w));
	map(0xa0, 0xa1).rw(FUNC(i82801_lpc_device::gen_pmcon_1_r), FUNC(i82801_lpc_device::gen_pmcon_1_w));
	map(0xa2, 0xa2).rw(FUNC(i82801_lpc_device::gen_pmcon_2_r), FUNC(i82801_lpc_device::gen_pmcon_2_w));
	map(0xa4, 0xa4).rw(FUNC(i82801_lpc_device::gen_pmcon_3_r), FUNC(i82801_lpc_device::gen_pmcon_3_w));
	map(0xb2, 0xb2).rw(FUNC(i82801_lpc_device::apm_cnt_r), FUNC(i82801_lpc_device::apm_cnt_w));
	map(0xb3, 0xb3).rw(FUNC(i82801_lpc_device::apm_sts_r), FUNC(i82801_lpc_device::apm_sts_w));
	map(0xb8, 0xbb).rw(FUNC(i82801_lpc_device::gpi_rout_r), FUNC(i82801_lpc_device::gpi_rout_w));
	map(0xc0, 0xc0).rw(FUNC(i82801_lpc_device::mon_fwd_en_r), FUNC(i82801_lpc_device::mon_fwd_en_w));
	map(0xc4, 0xcb).rw(FUNC(i82801_lpc_device::mon_trp_rng_r), FUNC(i82801_lpc_device::mon_trp_rng_w));
	map(0xcc, 0xcd).rw(FUNC(i82801_lpc_device::mon_trp_msk_r), FUNC(i82801_lpc_device::mon_trp_msk_w));
	map(0xd0, 0xd3).rw(FUNC(i82801_lpc_device::gen_cntl_r), FUNC(i82801_lpc_device::gen_cntl_w));
	map(0xd4, 0xd4).rw(FUNC(i82801_lpc_device::gen_sta_r), FUNC(i82801_lpc_device::gen_sta_w));
	map(0xd5, 0xd5).rw(FUNC(i82801_lpc_device::back_cntl_r), FUNC(i82801_lpc_device::back_cntl_w));
	map(0xd8, 0xd8).rw(FUNC(i82801_lpc_device::rtc_conf_r), FUNC(i82801_lpc_device::rtc_conf_w));
	map(0xe0, 0xe0).rw(FUNC(i82801_lpc_device::lpc_if_com_range_r), FUNC(i82801_lpc_device::lpc_if_com_range_w));
	map(0xe1, 0xe1).rw(FUNC(i82801_lpc_device::lpc_if_fdd_lpt_range_r), FUNC(i82801_lpc_device::lpc_if_fdd_lpt_range_w));
	map(0xe2, 0xe2).rw(FUNC(i82801_lpc_device::lpc_if_sound_range_r), FUNC(i82801_lpc_device::lpc_if_sound_range_w));
	map(0xe3, 0xe3).rw(FUNC(i82801_lpc_device::fwh_dec_en1_r), FUNC(i82801_lpc_device::fwh_dec_en1_w));
	map(0xe4, 0xe5).rw(FUNC(i82801_lpc_device::gen1_dec_r), FUNC(i82801_lpc_device::gen1_dec_w));
	map(0xe6, 0xe7).rw(FUNC(i82801_lpc_device::lpc_en_r), FUNC(i82801_lpc_device::lpc_en_w));
	map(0xe8, 0xeb).rw(FUNC(i82801_lpc_device::fwh_sel1_r), FUNC(i82801_lpc_device::fwh_sel1_w));
	map(0xec, 0xed).rw(FUNC(i82801_lpc_device::gen2_dec_r), FUNC(i82801_lpc_device::gen2_dec_w));
	map(0xee, 0xef).rw(FUNC(i82801_lpc_device::fwh_sel2_r), FUNC(i82801_lpc_device::fwh_sel2_w));
	map(0xf0, 0xf0).rw(FUNC(i82801_lpc_device::fwh_dec_en2_r), FUNC(i82801_lpc_device::fwh_dec_en2_w));
	map(0xf2, 0xf3).rw(FUNC(i82801_lpc_device::func_dis_r), FUNC(i82801_lpc_device::func_dis_w));
}

uint8_t i82801_lpc_device::get_slave_ack(offs_t offset)
{
	switch (offset)
	{
		case 0x02:
			return m_pic_slave->acknowledge();
		
		default:
			return 0x00;
	}
}

uint8_t i82801_lpc_device::dmac_page_r(offs_t offset)
{
	offset &= 0x0f;

	return m_dmac_page[offset];
}

void i82801_lpc_device::dmac_page_w(offs_t offset, uint8_t data)
{
	offset &= 0x0f;

	m_dmac_page[offset] = data;

	switch(offset & 7)
	{
		case 0:
			i82801_lpc_device::post_code_w(data);
			break;

		case 1:
			m_dmac_master_paddr[2] = data; // DMA Channel 2 Address (on master DMA Controller)
			break;

		case 2:
			m_dmac_master_paddr[3] = data; // DMA Channel 3 Address (on master DMA Controller)
			break;

		case 3:
			m_dmac_master_paddr[1] = data; // DMA Channel 1 Address (on master DMA Controller)
			break;

		case 7:
			m_dmac_master_paddr[0] = data; // DMA Channel 0 Address (on master DMA Controller)
			break;

		case 9:
			m_dmac_slave_paddr[2] = data; // DMA Channel 6 Address (on slave DMA Controller)
			break;

		case 10:
			m_dmac_slave_paddr[3] = data; // DMA Channel 7 Address (on slave DMA Controller)
			break;

		case 11:
			m_dmac_slave_paddr[1] = data; // DMA Channel 5 Address (on slave DMA Controller)
			break;

		case 15:
			m_dmac_slave_paddr[0] = data; // DMA Channel 4 Address (on slave DMA Controller)
	}
}

void i82801_lpc_device::dmac_chan_select(int8_t channel, int state)
{
	if(state)
		m_dmac_cur_chan = -1;
	else if(m_dmac_cur_chan == channel)
		m_dmac_cur_chan = channel;
	else
		return;

	if(m_dmac_cur_eop)
		m_lpcbus->eop_w(channel, state ? CLEAR_LINE : ASSERT_LINE);
}

void i82801_lpc_device::dmac_eop_w(int state)
{
	m_dmac_cur_eop = state;

	if(m_dmac_cur_chan != -1)
		m_lpcbus->eop_w(m_dmac_cur_chan, (m_dmac_cur_eop == ASSERT_LINE) ? ASSERT_LINE : CLEAR_LINE);
}

void i82801_lpc_device::dmac_hreq_w(int state)
{
	m_hostcpu->set_input_line(INPUT_LINE_HALT, state ? ASSERT_LINE : CLEAR_LINE);

	m_dmac_slave->hack_w(state);
}

uint8_t i82801_lpc_device::dmac_master_byte_r(offs_t offset)
{
	if(m_dmac_cur_chan != -1)
	{
		address_space& program_space = m_hostcpu->space(AS_PROGRAM);

		offs_t page_offset = ((offs_t)m_dmac_master_paddr[m_dmac_cur_chan & 3]) << 0x10;

		return program_space.read_byte(page_offset + offset);
	}
	else
	{
		return 0xff;
	}
}

void i82801_lpc_device::dmac_master_byte_w(offs_t offset, uint8_t data)
{
	if(m_dmac_cur_chan != -1)
	{
		address_space& program_space = m_hostcpu->space(AS_PROGRAM);

		offs_t page_offset = ((offs_t)m_dmac_master_paddr[m_dmac_cur_chan & 3]) << 0x10;

		program_space.write_byte(page_offset + offset, data);
	}
}

uint8_t i82801_lpc_device::dmac_slave_byte_r(offs_t offset)
{
	if(m_dmac_cur_chan != -1)
	{
		address_space& program_space = m_hostcpu->space(AS_PROGRAM);

		offs_t page_offset = ((offs_t)m_dmac_slave_paddr[m_dmac_cur_chan & 3]) << 0x10;

		uint16_t result = program_space.read_word((page_offset & 0xfe0000) | (offset << 1));

		m_dmac_high_byte = result & 0xff00;

		return result & 0xff;
	}
	else
	{
		return 0xff;
	}
}

void i82801_lpc_device::dmac_slave_byte_w(offs_t offset, uint8_t data)
{
	if(m_dmac_cur_chan != -1)
	{
		address_space& program_space = m_hostcpu->space(AS_PROGRAM);

		offs_t page_offset = ((offs_t)m_dmac_slave_paddr[m_dmac_cur_chan & 3]) << 0x10;

		program_space.write_word((page_offset & 0xfe0000) | (offset << 1), m_dmac_high_byte | data);
	}
}

uint8_t i82801_lpc_device::dmac_io0_r()
{
	return m_lpcbus->dack_r(0);
}

void i82801_lpc_device::dmac_io0_w(uint8_t data)
{
	m_lpcbus->dack_w(0, data);
}

uint8_t i82801_lpc_device::dmac_io1_r()
{
	return m_lpcbus->dack_r(1);
}

void i82801_lpc_device::dmac_io1_w(uint8_t data)
{
	m_lpcbus->dack_w(1, data);
}

uint8_t i82801_lpc_device::dmac_io2_r()
{
	return m_lpcbus->dack_r(2);
}

void i82801_lpc_device::dmac_io2_w(uint8_t data)
{
	m_lpcbus->dack_w(2, data);
}

uint8_t i82801_lpc_device::dmac_io3_r()
{
	return m_lpcbus->dack_r(3);
}

void i82801_lpc_device::dmac_io3_w(uint8_t data)
{
	m_lpcbus->dack_w(3, data);
}

uint8_t i82801_lpc_device::dmac_io5_r()
{
	return m_lpcbus->dack_r(5);
}

void i82801_lpc_device::dmac_io5_w(uint8_t data)
{
	m_lpcbus->dack_w(5, data);
}

uint8_t i82801_lpc_device::dmac_io6_r()
{
	return m_lpcbus->dack_r(6);
}

void i82801_lpc_device::dmac_io6_w(uint8_t data)
{
	m_lpcbus->dack_w(6, data);
}

uint8_t i82801_lpc_device::dmac_io7_r()
{
	return m_lpcbus->dack_r(7);
}

void i82801_lpc_device::dmac_io7_w(uint8_t data)
{
	m_lpcbus->dack_w(7, data);
}

void i82801_lpc_device::dmac_dack0_w(int state)
{
	i82801_lpc_device::dmac_chan_select(0, state);
}

void i82801_lpc_device::dmac_dack1_w(int state)
{
	i82801_lpc_device::dmac_chan_select(1, state);
}

void i82801_lpc_device::dmac_dack2_w(int state)
{
	i82801_lpc_device::dmac_chan_select(2, state);
}

void i82801_lpc_device::dmac_dack3_w(int state)
{
	i82801_lpc_device::dmac_chan_select(3, state);
}

void i82801_lpc_device::dmac_dack4_w(int state)
{
	m_dmac_master->hack_w(state);
}

void i82801_lpc_device::dmac_dack5_w(int state)
{
	i82801_lpc_device::dmac_chan_select(5, state);
}

void i82801_lpc_device::dmac_dack6_w(int state)
{
	i82801_lpc_device::dmac_chan_select(6, state);
}

void i82801_lpc_device::dmac_dack7_w(int state)
{
	i82801_lpc_device::dmac_chan_select(7, state);
}

uint8_t i82801_lpc_device::dmac2_r(offs_t offset)
{
	return m_dmac_slave->read(offset >> 1);
}

void i82801_lpc_device::dmac2_w(offs_t offset, uint8_t data)
{
	m_dmac_slave->write(offset >> 1, data);
}

void i82801_lpc_device::pit_counter1(int state)
{
	//
}

void i82801_lpc_device::pit_counter2(int state)
{
	if(state)
		m_nmi_sc |= NMI_SC_TMR2_OUT_STS;
	else
		m_nmi_sc &= (~NMI_SC_TMR2_OUT_STS);

	i82801_lpc_device::update_sound();
}

void i82801_lpc_device::isa_iochck_w(int state)
{
	if (m_nmi_enable && !state && !(m_nmi_sc & NMI_SC_IOCHK_NMI_EN))
		m_hostcpu->set_input_line(INPUT_LINE_NMI, state);
}

uint8_t i82801_lpc_device::eisa_irq_r(offs_t offset)
{
	if (offset == 0)
		return m_eisa_irq_mode & 0xff;
	else
		return m_eisa_irq_mode >> 8;
}

void i82801_lpc_device::eisa_irq_w(offs_t offset, uint8_t data)
{
	if (offset == 0)
		m_eisa_irq_mode = (m_eisa_irq_mode & 0xff00) | data;
	else
		m_eisa_irq_mode = (m_eisa_irq_mode & 0x00ff) | (data << 8);
}

void i82801_lpc_device::pirq_w(offs_t offset, uint8_t state)
{
	switch(offset)
	{
		case PIRQ_SELECT_A:
			i82801_lpc_device::route_irq(m_pirq_rout[0], state);
			break;

		case PIRQ_SELECT_B:
			i82801_lpc_device::route_irq(m_pirq_rout[1], state);
			break;

		case PIRQ_SELECT_C:
			i82801_lpc_device::route_irq(m_pirq_rout[2], state);
			break;

		case PIRQ_SELECT_D:
			i82801_lpc_device::route_irq(m_pirq_rout[3], state);
			break;

		case PIRQ_SELECT_E:
			i82801_lpc_device::route_irq(m_pirq_rout[4], state);
			break;

		case PIRQ_SELECT_F:
			i82801_lpc_device::route_irq(m_pirq_rout[5], state);
			break;

		case PIRQ_SELECT_G:
			i82801_lpc_device::route_irq(m_pirq_rout[6], state);
			break;

		case PIRQ_SELECT_H:
			i82801_lpc_device::route_irq(m_pirq_rout[7], state);
			break;

		default:
			break;
	}
}

void i82801_lpc_device::route_irq(uint8_t pirq_rout, int state)
{
	switch(pirq_rout)
	{
		case PIRQ_ROUTE_IRQ3:
			i82801_lpc_device::irq3_w(state);
			break;

		case PIRQ_ROUTE_IRQ4:
			i82801_lpc_device::irq4_w(state);
			break;

		case PIRQ_ROUTE_IRQ5:
			i82801_lpc_device::irq5_w(state);
			break;

		case PIRQ_ROUTE_IRQ6:
			i82801_lpc_device::irq6_w(state);
			break;

		case PIRQ_ROUTE_IRQ7:
			i82801_lpc_device::irq7_w(state);
			break;

		case PIRQ_ROUTE_IRQ9:
			i82801_lpc_device::irq9_w(state);
			break;

		case PIRQ_ROUTE_IRQ10:
			i82801_lpc_device::irq10_w(state);
			break;

		case PIRQ_ROUTE_IRQ11:
			i82801_lpc_device::irq11_w(state);
			break;

		case PIRQ_ROUTE_IRQ12:
			i82801_lpc_device::irq12_w(state);
			break;

		case PIRQ_ROUTE_IRQ14:
			i82801_lpc_device::irq14_w(state);
			break;

		case PIRQ_ROUTE_IRQ15:
			i82801_lpc_device::irq15_w(state);
			break;

		case PIRQ_NOT_ROUTED:
		default:
			break;
	}
}

void i82801_lpc_device::irq1_w(int state)
{
	m_pic_master->ir1_w(state);
}

void i82801_lpc_device::irq2_w(int state)
{
	m_pic_master->ir2_w(state);
}

void i82801_lpc_device::irq3_w(int state)
{
	m_pic_master->ir3_w(state);
}

void i82801_lpc_device::irq4_w(int state)
{
	m_pic_master->ir4_w(state);
}

void i82801_lpc_device::irq5_w(int state)
{
	m_pic_master->ir5_w(state);
}

void i82801_lpc_device::irq6_w(int state)
{
	m_pic_master->ir6_w(state);
}

void i82801_lpc_device::irq7_w(int state)
{
	m_pic_master->ir7_w(state);
}

void i82801_lpc_device::irq8_w(int state)
{
	m_pic_slave->ir0_w(state);
}

void i82801_lpc_device::irq9_w(int state)
{
	m_pic_slave->ir1_w(state);
}

void i82801_lpc_device::irq10_w(int state)
{
	m_pic_slave->ir2_w(state);
}

void i82801_lpc_device::irq11_w(int state)
{
	m_pic_slave->ir3_w(state);
}

void i82801_lpc_device::irq12_w(int state)
{
	m_pic_slave->ir4_w(state);
}

void i82801_lpc_device::irq13_w(int state)
{
	m_pic_slave->ir5_w(state);
}

void i82801_lpc_device::irq14_w(int state)
{
	m_pic_slave->ir6_w(state);
}

void i82801_lpc_device::irq15_w(int state)
{
	m_pic_slave->ir7_w(state);
}

void i82801_lpc_device::post_code_w(uint8_t data)
{
	if(m_log_post_codes)
		logerror("%s: POST CODE '%02x'\n", tag(), data);
}

void i82801_lpc_device::update_sound()
{
	bool pc_sound_state = (m_nmi_sc & NMI_SC_PCI_SERR_EN);
	bool pc_sound_timer = (m_nmi_sc & NMI_SC_TMR2_OUT_STS);

	m_pc_sound->level_w(pc_sound_state & pc_sound_timer);
}

void i82801_lpc_device::map_bios(address_space *memory_space, uint32_t start, uint32_t end, int idsel)
{
	// Ignore idsel, a16 inversion for now
	uint32_t mask = m_bios_region->bytes() - 1;
	// Using RAM because coreboot wants to write to a blank porition of flash to load up FILO
	memory_space->install_ram(start, end, m_bios_region->base() + (start & mask));
	//memory_space->install_rom(start, end, m_bios_region->base() + (start & mask));
}

uint32_t i82801_lpc_device::pmbase_r()
{
	return m_pmbase | 1;
}

void i82801_lpc_device::pmbase_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_pmbase);
	m_pmbase &= 0x0000ff80;
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::acpi_cntl_r()
{
	return m_acpi_cntl;
}

void i82801_lpc_device::acpi_cntl_w(uint8_t data)
{
	m_acpi_cntl = data;
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::bios_cntl_r()
{
	return m_pmbase | 1;
}

void i82801_lpc_device::bios_cntl_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_bios_cntl);
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::tco_cntl_r()
{
	return m_tco_cntl;
}

void i82801_lpc_device::tco_cntl_w(uint8_t data)
{
	m_tco_cntl = data;
}

uint32_t i82801_lpc_device::gpio_base_r()
{
	return m_gpio_base | GPIO_BASE_IS_IO;
}

void i82801_lpc_device::gpio_base_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_gpio_base);
	m_gpio_base &= GPIO_IO_BASE_MASK;
	m_gpio_base |= GPIO_BASE_IS_IO;

	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::gpio_cntl_r()
{
	return m_gpio_cntl;
}

void i82801_lpc_device::gpio_cntl_w(uint8_t data)
{
	m_gpio_cntl = data;
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::pirq_rout_r(offs_t offset)
{
	uint8_t data = m_pirq_rout[offset & (PIRQ_COUNT - 1)];

	logerror("PIRQ%02x read = %02x\n", (offset + 1), data);

	return data;
}

void i82801_lpc_device::pirq_rout_w(offs_t offset, uint8_t data)
{
	logerror("PIRQ%02x write %02x\n", (offset + 1), data);

	m_pirq_rout[offset & (PIRQ_COUNT - 1)] = data;
}

uint8_t i82801_lpc_device::serirq_cntl_r()
{
	return m_serirq_cntl;
}

void i82801_lpc_device::serirq_cntl_w(uint8_t data)
{
	m_serirq_cntl = data;
}

uint8_t i82801_lpc_device::pirq2_rout_r(offs_t offset)
{
	offset += 4;

	return i82801_lpc_device::pirq_rout_r(offset);
}

void i82801_lpc_device::pirq2_rout_w(offs_t offset, uint8_t data)
{
	offset += 4;

	i82801_lpc_device::pirq_rout_w(offset, data);
}

uint8_t i82801_lpc_device::d31_err_cfg_r()
{
	return m_d31_err_cfg;
}

void i82801_lpc_device::d31_err_cfg_w(uint8_t data)
{
	m_d31_err_cfg = data;
}

uint8_t i82801_lpc_device::d31_err_sts_r()
{
	return m_d31_err_sts;
}

void i82801_lpc_device::d31_err_sts_w(uint8_t data)
{
	m_d31_err_sts &= ~data;
}

uint16_t i82801_lpc_device::pci_dma_cfg_r()
{
	return m_pci_dma_cfg;
}

void i82801_lpc_device::pci_dma_cfg_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_pci_dma_cfg);
}

uint16_t i82801_lpc_device::gen_pmcon_1_r()
{
	return m_gen_pmcon_1;
}

void i82801_lpc_device::gen_pmcon_1_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_gen_pmcon_1);
}

uint8_t i82801_lpc_device::gen_pmcon_2_r()
{
	return m_gen_pmcon_2;
}

void i82801_lpc_device::gen_pmcon_2_w(uint8_t data)
{
	m_gen_pmcon_2 = data;
}

uint8_t i82801_lpc_device::gen_pmcon_3_r()
{
	return m_gen_pmcon_3;
}

void i82801_lpc_device::gen_pmcon_3_w(uint8_t data)
{
	m_gen_pmcon_3 = data;
}

uint8_t i82801_lpc_device::apm_cnt_r()
{
	return m_apm_cnt;
}

void i82801_lpc_device::apm_cnt_w(uint8_t data)
{
	m_apm_cnt = data;
}

uint8_t i82801_lpc_device::apm_sts_r()
{
	return m_apm_sts;
}

void i82801_lpc_device::apm_sts_w(uint8_t data)
{
	m_apm_sts = data;
}

uint32_t i82801_lpc_device::gpi_rout_r()
{
	return m_gpi_rout;
}

void i82801_lpc_device::gpi_rout_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_gpi_rout);
}

uint8_t i82801_lpc_device::mon_fwd_en_r()
{
	return m_mon_fwd_en;
}

void i82801_lpc_device::mon_fwd_en_w(uint8_t data)
{
	m_mon_fwd_en = data;
}

uint16_t i82801_lpc_device::mon_trp_rng_r(offs_t offset)
{
	return m_mon_trp_rng[offset];
}

void i82801_lpc_device::mon_trp_rng_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_mon_trp_rng[offset]);
}

uint16_t i82801_lpc_device::mon_trp_msk_r()
{
	return m_mon_trp_msk;
}

void i82801_lpc_device::mon_trp_msk_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_mon_trp_msk);
}

uint32_t i82801_lpc_device::gen_cntl_r()
{
	return m_gen_cntl;
}

void i82801_lpc_device::gen_cntl_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{

	COMBINE_DATA(&m_gen_cntl);

	m_apic_io_en = (m_gen_cntl & i82801_lpc_device::GCNTL_APIC_EN);
	m_apic_xio_en = (m_gen_cntl & i82801_lpc_device::GCNTL_XAPIC_EN);

	i82801_lpc_device::remap_cb();

}

uint8_t i82801_lpc_device::gen_sta_r()
{
	return m_gen_sta;
}

void i82801_lpc_device::gen_sta_w(uint8_t data)
{
	m_gen_sta = data;
}

uint8_t i82801_lpc_device::back_cntl_r()
{
	return m_back_cntl;
}

void i82801_lpc_device::back_cntl_w(uint8_t data)
{
	m_back_cntl = data;
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::rtc_conf_r()
{
	return m_rtc_conf;
}

void i82801_lpc_device::rtc_conf_w(uint8_t data)
{
	m_rtc_conf = data;
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::lpc_if_com_range_r()
{
	return m_lpc_if_com_range;
}

void i82801_lpc_device::lpc_if_com_range_w(uint8_t data)
{
	m_lpc_if_com_range = data;
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::lpc_if_fdd_lpt_range_r()
{
	return m_lpc_if_fdd_lpt_range;
}

void i82801_lpc_device::lpc_if_fdd_lpt_range_w(offs_t offset, uint8_t data, uint8_t mem_mask)
{
	COMBINE_DATA(&m_lpc_if_fdd_lpt_range);
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::lpc_if_sound_range_r()
{
	return m_lpc_if_sound_range;
}

void i82801_lpc_device::lpc_if_sound_range_w(offs_t offset, uint8_t data, uint8_t mem_mask)
{
	COMBINE_DATA(&m_lpc_if_sound_range);
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::fwh_dec_en1_r()
{
	return m_fwh_dec_en1;
}

void i82801_lpc_device::fwh_dec_en1_w(uint8_t data)
{
	m_fwh_dec_en1 = data;
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::gen1_dec_r()
{
	return m_gen1_dec;
}

void i82801_lpc_device::gen1_dec_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_gen1_dec);
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::lpc_en_r()
{
	return m_lpc_en;
}

void i82801_lpc_device::lpc_en_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_lpc_en);
	i82801_lpc_device::remap_cb();
}

uint32_t i82801_lpc_device::fwh_sel1_r()
{
	return m_fwh_sel1;
}

void i82801_lpc_device::fwh_sel1_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_fwh_sel1);
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::gen2_dec_r()
{
	return m_gen2_dec;
}

void i82801_lpc_device::gen2_dec_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_gen2_dec);
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::fwh_sel2_r()
{
	return m_fwh_sel2;
}

void i82801_lpc_device::fwh_sel2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_fwh_sel2);
	i82801_lpc_device::remap_cb();
}

uint8_t i82801_lpc_device::fwh_dec_en2_r()
{
	return m_fwh_dec_en2;
}

void i82801_lpc_device::fwh_dec_en2_w(uint8_t data)
{
	m_fwh_dec_en2 = data;
	i82801_lpc_device::remap_cb();
}

uint16_t i82801_lpc_device::func_dis_r()
{
	return m_func_dis;
}

void i82801_lpc_device::func_dis_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_func_dis);

}

uint8_t i82801_lpc_device::nmi_sc_r()
{
	m_nmi_sc ^= NMI_SC_REF_TOGGLE;

	return m_nmi_sc;
}

void i82801_lpc_device::nmi_sc_w(uint8_t data)
{
	m_pit->write_gate2(data & NMI_SC_TIM_CNT2_EN);

	if(((m_nmi_sc ^ data) & NMI_SC_IOCHK_NMI_EN) && data & NMI_SC_IOCHK_NMI_EN)
		m_hostcpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);

	i82801_lpc_device::update_sound();

	m_nmi_sc = data & NMI_IN_MASK;
}

uint8_t i82801_lpc_device::rtc_nmi_r(offs_t offset)
{
	switch (offset)
	{
		case 0:
			return (m_rtc->get_address() & 0x7f) | ((m_nmi_enable) ? 0x80 : 0x00);

		case 1:
			return m_rtc->data_r();

		case 2:
			if(m_rtc_conf & i82801_lpc_device::RTC_UPPER128_ENABLE)
				return 0x80 | (m_rtc->get_address() & 0x7f);
			else
				return 0xff;

		case 3:
			if(m_rtc_conf & i82801_lpc_device::RTC_UPPER128_ENABLE)
				return m_rtc->data_r();
			else
				return 0xff;

		default:
			return 0xff;
	}
}

void i82801_lpc_device::rtc_nmi_w(offs_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0:
			m_nmi_enable = data & 0x80;
			m_rtc->address_w(data & 0x7f);
			break;

		case 1:
			m_rtc->data_w(data);
			break;

		case 2:
			if(m_rtc_conf & i82801_lpc_device::RTC_UPPER128_ENABLE)
				m_rtc->address_w(0x80 | (data & 0x7f));
			break;

		case 3:
			if(m_rtc_conf & i82801_lpc_device::RTC_UPPER128_ENABLE)
				m_rtc->data_w(data);
			break;

		default:
			break;
	}
}

uint32_t i82801_lpc_device::gpio_use_sel1_r()
{
	return m_gpio_use_sel1;
}

void i82801_lpc_device::gpio_use_sel1_w(uint32_t data)
{
	m_gpio_use_sel1 = data;
}

uint32_t i82801_lpc_device::gpio_io_sel1_r()
{
	return m_gpio_io_sel1;
}

void i82801_lpc_device::gpio_io_sel1_w(uint32_t data)
{
	m_gpio_io_sel1 = data;
}

uint32_t i82801_lpc_device::gpio_lvl1_r()
{
	return m_gpio_lvl1;
}

void i82801_lpc_device::gpio_lvl1_w(uint32_t data)
{
	m_gpio_lvl1 = data;
}

uint32_t i82801_lpc_device::gpio_ttl_r()
{
	return m_gpio_ttl;
}

void i82801_lpc_device::gpio_ttl_w(uint32_t data)
{
	m_gpio_ttl = data;
}

uint32_t i82801_lpc_device::gpio_blink_r()
{
	return m_gpio_blink;
}

void i82801_lpc_device::gpio_blink_w(uint32_t data)
{
	m_gpio_blink = data;
}

uint32_t i82801_lpc_device::gpio_inv_r()
{
	return m_gpio_inv;
}

void i82801_lpc_device::gpio_inv_w(uint32_t data)
{
	m_gpio_inv = data;
}

uint32_t i82801_lpc_device::gpio_use_sel2_r()
{
	return m_gpio_use_sel2;
}

void i82801_lpc_device::gpio_use_sel2_w(uint32_t data)
{
	m_gpio_use_sel2 = data;
}

uint32_t i82801_lpc_device::gpio_io_sel2_r()
{
	return m_gpio_io_sel2;
}

void i82801_lpc_device::gpio_io_sel2_w(uint32_t data)
{
	m_gpio_io_sel2 = data;
}

uint32_t i82801_lpc_device::gpio_lvl2_r()
{
	return m_gpio_lvl2;
}

void i82801_lpc_device::gpio_lvl2_w(uint32_t data)
{
	m_gpio_lvl2 = data;
}

uint8_t i82801_lpc_device::dummydelay_r(offs_t offset)
{
	m_hostcpu->eat_cycles(m_hostcpu->clock() / 1000000 + 1);
	
	return 0x00000000;
}

void i82801_lpc_device::dummydelay_w(offs_t offset, uint8_t data)
{
	m_hostcpu->eat_cycles(m_hostcpu->clock() / 1000000 + 1);
}

IRQ_CALLBACK_MEMBER(i82801_lpc_device::irq_acknowledge)
{
	if(m_apic_io_en)
		return i82801_lpc_device::apic_acknowledge();
	else
		return m_pic_master->acknowledge();
}

uint8_t i82801_lpc_device::apic_acknowledge()
{
	return 0x00;
}

uint32_t i82801_lpc_device::apic_reg_data_r(offs_t offset)
{
	offset >>= 2;
	offset &= (i82801_lpc_device::APIC_REG_COUNT - 1);


	switch(offset)
	{
		case i82801_lpc_device::APIC_REG_DATA:
		{
			uint32_t reg_offset = m_apic_reg[i82801_lpc_device::APIC_REG_ADDR] & (i82801_lpc_device::APIC_REG_COUNT - 1);
			reg_offset += i82801_lpc_device::APIC_REG_ADDR_OFFSET;

			return m_apic_reg[reg_offset];
		}

		case i82801_lpc_device::APIC_REG_EOI:
			// Write only
			return 0x00000000;

		default:
			return m_apic_reg[offset];
	}
}

void i82801_lpc_device::apic_reg_data_w(offs_t offset, uint32_t data)
{
	offset >>= 2;
	offset &= (i82801_lpc_device::APIC_REG_COUNT - 1);


	switch(offset)
	{
		case i82801_lpc_device::APIC_REG_DATA:
		{
			uint32_t reg_offset = m_apic_reg[i82801_lpc_device::APIC_REG_ADDR] & (i82801_lpc_device::APIC_REG_COUNT - 1);
			reg_offset += i82801_lpc_device::APIC_REG_ADDR_OFFSET;

			m_apic_reg[reg_offset] = data;
			break;
		}

		case i82801_lpc_device::APIC_REG_SIVR:
			m_apic_local_en = (data & i82801_lpc_device::APIC_SIVR_LOCAL_EN);
			m_apic_svec = data & i82801_lpc_device::APIC_SIVR_SVEC_MASK;

			m_apic_reg[offset] = data;
			break;

		case i82801_lpc_device::APIC_REG_LAPIC_VER:
		case i82801_lpc_device::APIC_REG_APR:
		case i82801_lpc_device::APIC_REG_PPR:
		case i82801_lpc_device::APIC_REG_RDD:
		case i82801_lpc_device::APIC_REG_ISR:
		case i82801_lpc_device::APIC_REG_TMR:
		case i82801_lpc_device::APIC_REG_IRR:
		case i82801_lpc_device::APIC_REG_ESR:
		case i82801_lpc_device::APIC_REG_CCNT:
			// Read only
			break;

		default:
			m_apic_reg[offset] = data;
			break;
	}
}

/*

	uint32_t reg_eb_val;
	uint32_t reg_1008_val; // a counter of some sort
	reg_eb_val = 0x00000000;
	reg_1008_val = 0x00000000;



	uint8_t nop_r(offs_t offset);
	void nop_w(offs_t offset, uint8_t data);
	void eb_w(uint8_t data);
	uint32_t reg_1008_r();



void msntv2_state::eb_w(uint8_t data)
{
	reg_eb_val++;

	if((reg_eb_val % 0x100) == 0x00)
		m_maincpu->set_input_line(INPUT_LINE_IRQ0, ASSERT_LINE); // IRQ0 every 0x100 to advance the timer count
}

uint32_t msntv2_state::reg_1008_r()
{
	reg_1008_val += 0x2b;

	return reg_1008_val;
}
	*/