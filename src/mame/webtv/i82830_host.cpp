// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82830_host.h"

DEFINE_DEVICE_TYPE(I82830_HOST, i82830_host_device, "i82830_host", "Intel 830M Chipset 82830 Northbridge")

i82830_host_device::i82830_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_host_device(mconfig, I82830_HOST, tag, owner, clock),
	m_hostcpumem(*this, finder_base::DUMMY_TAG),
	m_hostcpu(*this, finder_base::DUMMY_TAG)
{
}

void i82830_host_device::device_start()
{
	pci_host_device::device_start();

	set_spaces(&m_hostcpumem->space(AS_PROGRAM), &m_hostcpumem->space(AS_IO));

	memory_window_start = 0;
	memory_window_end = 0xffffffff;
	memory_offset = 0;

	io_window_start = 0;
	io_window_end = 0xffff;
	io_offset = 0;

	status = 0x0010;

	i82830_host_device::set_default_values();
}

void i82830_host_device::device_reset()
{
	pci_host_device::device_reset();

	i82830_host_device::set_default_values();

}

void i82830_host_device::set_default_values()
{
	status = 0x0010;

	m_agp_enabled = false;

	m_rrbar = 0x00000000;

	m_gcc0 = (1 << 15) | (1 << 1); // 0x8002, value needed to reach default value (and not documented)
	m_gcc0 |= i82830_host_device::GMCH_CNTL0_LPGP_008 << i82830_host_device::GMCH_CNTL0_LPGP_SHIFT;
	m_gcc0 |= i82830_host_device::GMCH_CNTL0_IOQ_GC_INF << i82830_host_device::GMCH_CNTL0_IOQ_GC_SHIFT;

	m_gcc1 = 0x0000;

	m_fdhc = 0x00;
	m_dra = 0x00ff;
	m_drt = 0x00000010;
	m_drc = 0x00000000;
	m_dtc = 0x00000000;
	m_errcmd = 0x0000;

	std::fill(std::begin(m_pam), std::end(m_pam), 0);
	std::fill(std::begin(m_drb), std::end(m_drb), 0);

	m_acapid = 0x00200002;
	m_agpcmd = 0x00000000;
	m_agpstat = 0x1f000217;
	m_agpctrl = 0x00000000;
	m_apbase = i82830_host_device::GFX_APFLAG_PREFETCHABLE;
	m_apsize = i82830_host_device::GFX_APSIZE_256MB;
	m_attbase = 0x00000000;
	m_amtt = 0x10;
	m_lptt = 0x10;
}

void i82830_host_device::reset_all_mappings()
{
	pci_host_device::reset_all_mappings();

	m_smram_cntl = i82830_host_device::SMRAM_CNTL_C_BASE_SPACE2;
	m_esmram_cntl = i82830_host_device::ESMRAM_CNTL_CACHE | i82830_host_device::ESMRAM_CNTL_L1_EN | i82830_host_device::ESMRAM_CNTL_L2_EN;
}

void i82830_host_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	io_space->install_device(0x0000, 0xffff, *static_cast<pci_host_device *>(this), &pci_host_device::io_configuration_access_map);

	uint32_t ram_size = i82830_host_device::get_ram_size();
	uint32_t ram_dword_size = ram_size >> 2;
	if(m_ram.size() < ram_dword_size)
		m_ram.resize(ram_dword_size);

	//
	// DOS Application Area
	//

	memory_space->install_ram(0x00000000, 0x0009ffff, &m_ram[0x00000000 >> 2]);

	//
	// System Management Area (may also be the VGA video buffer, that's allocated via i82830_graphics_device)
	//

	if(m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_OPEN)
	{
		memory_space->install_ram(0x000a0000, 0x000bffff, &m_ram[0x000a0000 >> 2]);
	}

	// m_pam[0]=30, m_pam[1]=33, m_pam[2]=33, m_pam[3]=33, m_pam[4]=33, m_pam[5]=33, m_pam[6]=33

	//
	// System BIOS Area
	//

	i82830_host_device::install_pam(memory_space, m_pam[0] >> PAM_74_SHIFT, 0x000f0000, 0x000fffff);

	//
	// Expansion Area
	//

	i82830_host_device::install_pam(memory_space, m_pam[1] >> PAM_30_SHIFT, 0x000c0000, 0x000c3fff);
	i82830_host_device::install_pam(memory_space, m_pam[1] >> PAM_74_SHIFT, 0x000c4000, 0x000c7fff);

	i82830_host_device::install_pam(memory_space, m_pam[2] >> PAM_30_SHIFT, 0x000c8000, 0x000cbfff);
	i82830_host_device::install_pam(memory_space, m_pam[2] >> PAM_74_SHIFT, 0x000cc000, 0x000cffff);

	i82830_host_device::install_pam(memory_space, m_pam[3] >> PAM_30_SHIFT, 0x000d0000, 0x000d3fff);
	i82830_host_device::install_pam(memory_space, m_pam[3] >> PAM_74_SHIFT, 0x000d4000, 0x000d7fff);

	i82830_host_device::install_pam(memory_space, m_pam[4] >> PAM_30_SHIFT, 0x000d8000, 0x000dbfff);
	i82830_host_device::install_pam(memory_space, m_pam[4] >> PAM_74_SHIFT, 0x000dc000, 0x000dffff);

	//
	// Extended System BIOS Area
	//

	i82830_host_device::install_pam(memory_space, m_pam[5] >> PAM_30_SHIFT, 0x000e0000, 0x000e3fff);
	i82830_host_device::install_pam(memory_space, m_pam[5] >> PAM_74_SHIFT, 0x000e4000, 0x000e7fff);

	i82830_host_device::install_pam(memory_space, m_pam[6] >> PAM_30_SHIFT, 0x000e8000, 0x000ebfff);
	i82830_host_device::install_pam(memory_space, m_pam[6] >> PAM_74_SHIFT, 0x000ec000, 0x000effff);

	if(ram_size > 0x00100000)
	{
		memory_space->install_ram(0x00100000, (ram_size - 1), &m_ram[0x00100000 >> 2]);

	}

	if(m_smram_cntl & i82830_host_device::SMRAM_CNTL_G_EN)
	{
		//
		// High System Management Area
		//

		if(m_esmram_cntl & i82830_host_device::ESMRAM_CNTL_H_EN)
		{
			memory_space->install_ram(0xfeda0000, 0xfedbffff, &m_ram[0x000a0000 >> 2]);
		}
	}

	if(m_gcc0 & i82830_host_device::GMCH_CNTL0_RRBAR_EN)
	{
		uint16_t rmbar = (m_rrbar & i82830_host_device::RRBAR_BASE_MASK);
		if(rmbar != 0x00000000)
			io_space->install_device(rmbar, rmbar + (i82830_host_device::RRBAR_SIZE - 1), *this, &i82830_host_device::rrbar_map);
	}
}

void i82830_host_device::install_pam(address_space *memory_space, uint8_t attr, uint64_t memory_window_start, uint64_t memory_window_end)
{
	if((attr & PAM_RW_EN) == PAM_RW_EN)
		memory_space->install_ram      (memory_window_start, memory_window_end, &m_ram[memory_window_start >> 2]);
	else if(attr & PAM_W_EN)
		memory_space->install_writeonly(memory_window_start, memory_window_end, &m_ram[memory_window_start >> 2]);
	else if(attr & PAM_R_EN)
		memory_space->install_rom      (memory_window_start, memory_window_end, &m_ram[memory_window_start >> 2]);
}

uint32_t i82830_host_device::get_tseg_size()
{
	if(m_esmram_cntl & i82830_host_device::ESMRAM_CNTL_TSEG_EN)
		return (m_esmram_cntl & i82830_host_device::ESMRAM_CNTL_TSEG_1M) ? (1 * 1024 * 1024) : (512 * 1024);
	else
		return 0;
}

uint32_t i82830_host_device::get_tseg_base()
{
	uint32_t ram_size = i82830_host_device::get_ram_size();
	uint32_t tseg_size = i82830_host_device::get_tseg_size();
	uint32_t graphics_size = i82830_host_device::get_graphics_size();

	if(tseg_size > 0 && tseg_size <= (ram_size - i82830_host_device::MIN_RAM_SIZE - graphics_size))
		return (ram_size - graphics_size - tseg_size);
	else
		return 0;
}

uint32_t i82830_host_device::get_graphics_size()
{
	// Check dynamic allocation
	/*
	uint32_t ap_real_size = 0;
	switch(m_apsize & i82830_host_device::GFX_APSIZE_MASK)
	{
		case GFX_APSIZE_32MB:
			ap_real_size = 32 * 1024 * 1024;
			break;
		case GFX_APSIZE_64MB:
			ap_real_size = 64 * 1024 * 1024;
			break;
		case GFX_APSIZE_128MB:
			ap_real_size = 128 * 1024 * 1024;
			break;
		case GFX_APSIZE_256MB:
		default:
			ap_real_size = 256 * 1024 * 1024;
	}
	*/

	switch(m_gcc1 >> i82830_host_device::GMCH_CNTL1_GMS_SHIFT)
	{
		case i82830_host_device::GMCH_CNTL1_GMS_8M:
			return 8 * 1024 * 1024;

		case i82830_host_device::GMCH_CNTL1_GMS_1M:
			return 1 * 1024 * 1024;

		case i82830_host_device::GMCH_CNTL1_GMS_512KB:
			return 512 * 1024;

		default:
			return 0;
	}
}

uint32_t i82830_host_device::get_graphics_base()
{
	uint32_t ram_size = i82830_host_device::get_ram_size();
	uint32_t graphics_size = i82830_host_device::get_graphics_size();


	if(graphics_size > 0 && graphics_size <= (ram_size - i82830_host_device::MIN_RAM_SIZE - graphics_size))
		return (ram_size - graphics_size);
	else
		return 0;
}

uint8_t i82830_host_device::sspace_r(offs_t offset)
{
	return m_sspace[offset & (i82830_host_device::SSPACE_BLOCK_SIZE - 1)];
}

void i82830_host_device::sspace_w(offs_t offset, uint8_t data)
{
	m_sspace[offset & (i82830_host_device::SSPACE_BLOCK_SIZE - 1)] = data;
}

void i82830_host_device::rrbar_map(address_map &map)
{
	// Note: suppose to be split between read-only (0x00-0x3f) and read-write (0x40-0xff) registers.
	map(0x00000, 0x000ff).m(FUNC(i82830_host_device::config_map));

	uint32_t sspace_start = i82830_host_device::RRBAR_SIZE - i82830_host_device::SSPACE_BLOCK_SIZE;

	if((m_gcc0 & i82830_host_device::GMCH_CNTL0_SSPACE_EN) && (m_rrbar & i82830_host_device::RRBAR_SSPACE_SIZE_MASK) == i82830_host_device::RRBAR_SSPACE_SIZE_256B)
		map(sspace_start, sspace_start + (i82830_host_device::RRBAR_SIZE - 1)).rw(FUNC(i82830_host_device::sspace_r), FUNC(i82830_host_device::sspace_w));
}

void i82830_host_device::config_map(address_map &map)
{
	pci_host_device::config_map(map);

	map(0x10, 0x13).rw(FUNC(i82830_host_device::apbase_r), FUNC(i82830_host_device::apbase_w));
	map(0x48, 0x4b).rw(FUNC(i82830_host_device::rrbar_r), FUNC(i82830_host_device::rrbar_w));
	map(0x50, 0x51).rw(FUNC(i82830_host_device::gcc0_r), FUNC(i82830_host_device::gcc0_w));
	map(0x52, 0x53).rw(FUNC(i82830_host_device::gcc1_r), FUNC(i82830_host_device::gcc1_w));
	map(0x58, 0x58).rw(FUNC(i82830_host_device::fdhc_r), FUNC(i82830_host_device::fdhc_w));
	map(0x59, 0x59 + (i82830_host_device::PAM_REG_COUNT - 1)).rw(FUNC(i82830_host_device::pam_r), FUNC(i82830_host_device::pam_w));
	map(0x60, 0x60 + (i82830_host_device::DRB_REG_COUNT - 1)).rw(FUNC(i82830_host_device::drb_r), FUNC(i82830_host_device::drb_w));
	map(0x70, 0x71).rw(FUNC(i82830_host_device::dra_r), FUNC(i82830_host_device::dra_w));
	map(0x78, 0x7b).rw(FUNC(i82830_host_device::drt_r), FUNC(i82830_host_device::drt_w));
	map(0x7c, 0x7f).rw(FUNC(i82830_host_device::drc_r), FUNC(i82830_host_device::drc_w));
	map(0x8c, 0x8f).rw(FUNC(i82830_host_device::dtc_r), FUNC(i82830_host_device::dtc_w));
	map(0x90, 0x90).rw(FUNC(i82830_host_device::smram_r), FUNC(i82830_host_device::smram_w));
	map(0x91, 0x91).rw(FUNC(i82830_host_device::esmramc_r), FUNC(i82830_host_device::esmramc_w));
	map(0x92, 0x93).r(FUNC(i82830_host_device::errsts_r));
	map(0x94, 0x95).rw(FUNC(i82830_host_device::errcmd_r), FUNC(i82830_host_device::errcmd_w));
	map(0xa0, 0xa3).r(FUNC(i82830_host_device::acapid_r));
	map(0xa4, 0xa7).r(FUNC(i82830_host_device::agpstat_r));
	map(0xa8, 0xab).rw(FUNC(i82830_host_device::agpcmd_r), FUNC(i82830_host_device::agpcmd_w));
	map(0xb0, 0xb3).rw(FUNC(i82830_host_device::agpctrl_r), FUNC(i82830_host_device::agpctrl_w));
	map(0xb4, 0xb4).rw(FUNC(i82830_host_device::apsize_r), FUNC(i82830_host_device::apsize_w));
	map(0xb8, 0xbb).rw(FUNC(i82830_host_device::attbase_r), FUNC(i82830_host_device::attbase_w));
	map(0xbc, 0xbc).rw(FUNC(i82830_host_device::amtt_r), FUNC(i82830_host_device::amtt_w));
	map(0xbd, 0xbd).rw(FUNC(i82830_host_device::lptt_r), FUNC(i82830_host_device::lptt_w));
	map(0xc2, 0xc3).rw(FUNC(i82830_host_device::creg_c2_r), FUNC(i82830_host_device::creg_c2_w));
}

uint8_t i82830_host_device::capptr_r()
{
	return 0x40;
}

uint32_t i82830_host_device::rrbar_r()
{
	return m_rrbar;
}

void i82830_host_device::rrbar_w(uint32_t data)
{
	m_rrbar = data;
}

uint16_t i82830_host_device::gcc0_r()
{
	return m_gcc0;
}

void i82830_host_device::gcc0_w(uint16_t data)
{
	uint8_t wmask = (m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_LCK) ? i82830_host_device::GMCH_CNTL0_LCK_WMASK : i82830_host_device::GMCH_CNTL0_WMASK;

	m_gcc0 = (m_gcc0 & (~wmask)) | (data & wmask);

	i82830_host_device::remap_cb();
}

uint16_t i82830_host_device::gcc1_r()
{
	return m_gcc1;
}

void i82830_host_device::gcc1_w(uint16_t data)
{
	m_gcc1 = data;
}

uint8_t i82830_host_device::fdhc_r()
{
	return m_fdhc;
}

void i82830_host_device::fdhc_w(uint8_t data)
{
	m_fdhc = data;
}

/*
XXX>p 1 0 0 0 59
Read 0x00000030
XXX>p 1 0 0 0 5a
Read 0x00000033
XXX>p 1 0 0 0 5b
Read 0x00000033
XXX>p 1 0 0 0 5c
Read 0x00000033
XXX>p 1 0 0 0 5d
Read 0x00000033
XXX>p 1 0 0 0 5e
Read 0x00000033
XXX>p 1 0 0 0 5f
Read 0x00000033
*/
uint8_t i82830_host_device::pam_r(offs_t offset)
{
	return m_pam[offset];
}

void i82830_host_device::pam_w(offs_t offset, uint8_t data)
{
	m_pam[offset] = data;
	i82830_host_device::remap_cb();
}


/*
XXX>p 1 0 0 0 60
Read 0x00000000
XXX>p 1 0 0 0 61
Read 0x00000000
XXX>p 1 0 0 0 62
Read 0x00000004
XXX>p 1 0 0 0 63
Read 0x00000004
XXX>p 1 0 0 0 64
Read 0x00000004
XXX>p 1 0 0 0 65
Read 0x00000004
*/
uint8_t i82830_host_device::drb_r(offs_t offset)
{

	return m_drb[offset];
}

void i82830_host_device::drb_w(offs_t offset, uint8_t data)
{
	if(!(m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_LCK))
	{
		m_drb[offset] = data;
		i82830_host_device::remap_cb();
	}

	//machine().debug_break();
}

uint16_t i82830_host_device::dra_r()
{
	return m_dra;
}

void i82830_host_device::dra_w(uint16_t data)
{
	if(!(m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_LCK))
		m_dra = data;
}

uint32_t i82830_host_device::drt_r()
{
	return m_drt;
}

void i82830_host_device::drt_w(uint32_t data)
{
	m_drt = data;
}

uint32_t i82830_host_device::drc_r()
{
	return m_drc;
}

void i82830_host_device::drc_w(uint32_t data)
{
	m_drc = data;
}

uint32_t i82830_host_device::dtc_r()
{
	return m_dtc;
}

void i82830_host_device::dtc_w(uint32_t data)
{
	m_dtc = data;
}

uint8_t i82830_host_device::smram_r()
{
	return m_smram_cntl;
}

void i82830_host_device::smram_w(uint8_t data)
{

	if(!(m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_LCK))
	{
		if(data & i82830_host_device::SMRAM_CNTL_D_LCK)
			data &= (~i82830_host_device::SMRAM_CNTL_D_OPEN);

		m_smram_cntl = (data & (~i82830_host_device::SMRAM_CNTL_WMASK)) | (data & i82830_host_device::SMRAM_CNTL_WMASK);
		i82830_host_device::remap_cb();
	}
}

uint8_t i82830_host_device::esmramc_r()
{
	return m_esmram_cntl;
}

void i82830_host_device::esmramc_w(uint8_t data)
{
	uint8_t wmask = (m_smram_cntl & i82830_host_device::SMRAM_CNTL_D_LCK) ? i82830_host_device::ESMRAM_CNTL_LCK_WMASK : i82830_host_device::ESMRAM_CNTL_WMASK;

	// ESMRAM_CNTL_H_ERR will be cleared here with the WMASK.

	m_esmram_cntl = (data & (~wmask)) | (data & wmask);

	i82830_host_device::remap_cb();
}

uint16_t i82830_host_device::errsts_r()
{
	return 0x0000;
}

uint16_t i82830_host_device::errcmd_r()
{
	return m_errcmd;
}

void i82830_host_device::errcmd_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_errcmd);
}


uint32_t i82830_host_device::acapid_r()
{
	return m_acapid;
}

uint32_t i82830_host_device::agpstat_r()
{
	return m_agpstat;
}

uint32_t i82830_host_device::agpcmd_r()
{
	return m_agpcmd;
}

void i82830_host_device::agpcmd_w(uint32_t data)
{
	if(m_agp_enabled)
		m_agpcmd = data;
}

uint32_t i82830_host_device::agpctrl_r()
{
	return m_agpctrl;
}

void i82830_host_device::agpctrl_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	COMBINE_DATA(&m_agpctrl);
}

uint32_t i82830_host_device::apbase_r()
{
	return m_apbase;
}

void i82830_host_device::apbase_w(uint32_t data)
{
	if(m_agp_enabled)
	{
		uint32_t wmask = 0;

		switch(m_apsize & i82830_host_device::GFX_APSIZE_MASK)
		{
			case GFX_APSIZE_32MB:
				wmask = GFX_APBASE_32MB_WMASK;
				break;
			case GFX_APSIZE_64MB:
				wmask = GFX_APBASE_64MB_WMASK;
				break;
			case GFX_APSIZE_128MB:
				wmask = GFX_APBASE_128MB_WMASK;
				break;
			case GFX_APSIZE_256MB:
			default:
				wmask = GFX_APBASE_256_WMASK;
		}


		m_apbase = (m_apbase & (~wmask)) | (data & wmask);
	}
}


uint8_t i82830_host_device::apsize_r()
{
	return m_apsize;
}

void i82830_host_device::apsize_w(uint8_t data)
{
	if(m_agp_enabled)
	{
		m_apsize = data;

		i82830_host_device::apbase_w(m_apbase);
	}
}

uint32_t i82830_host_device::attbase_r()
{
	return m_attbase;
}

void i82830_host_device::attbase_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	if(m_agp_enabled)
		COMBINE_DATA(&m_attbase);
}

uint8_t i82830_host_device::amtt_r()
{
	return m_amtt;
}

void i82830_host_device::amtt_w(uint8_t data)
{
	if(m_agp_enabled)
		m_amtt = data;
}

uint8_t i82830_host_device::lptt_r()
{
	return m_lptt;
}

void i82830_host_device::lptt_w(uint8_t data)
{
	if(m_agp_enabled)
		m_lptt = data;
}

uint16_t i82830_host_device::creg_c2_r()
{
	return m_creg_c2;
}

void i82830_host_device::creg_c2_w(uint16_t data)
{
	m_creg_c2 = data;
}


