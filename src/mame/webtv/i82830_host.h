// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82830_HOST_H
#define MAME_WEBTV_I82830_HOST_H

#pragma once

#include "machine/pci.h"
#include "cpu/i386/i386.h"
class i82830_host_device : public pci_host_device
{

public:

	static constexpr uint32_t MIN_RAM_SIZE = 1 * 1024 * 1024;

	static constexpr uint8_t PAM_REG_COUNT = 7;
	static constexpr uint8_t PAM_30_SHIFT  = 0;
	static constexpr uint8_t PAM_74_SHIFT  = 4;
	static constexpr uint8_t PAM_W_EN      = 1 << 1;
	static constexpr uint8_t PAM_R_EN      = 1 << 0;
	static constexpr uint8_t PAM_RW_EN     = i82830_host_device::PAM_W_EN | i82830_host_device::PAM_R_EN;

	static constexpr uint8_t SMRAM_CNTL_D_OPEN        = 1 << 6;
	static constexpr uint8_t SMRAM_CNTL_D_CLS         = 1 << 5;
	static constexpr uint8_t SMRAM_CNTL_D_LCK         = 1 << 4;
	static constexpr uint8_t SMRAM_CNTL_G_EN          = 1 << 3;
	static constexpr uint8_t SMRAM_CNTL_C_BASE_MASK   = 0x7;
	static constexpr uint8_t SMRAM_CNTL_C_BASE_SPACE2 = 0x7; // 0xa0000-0xbffff
	static constexpr uint8_t SMRAM_CNTL_WMASK         = 0x78;

	static constexpr uint8_t ESMRAM_CNTL_H_EN      = 1 << 7;
	static constexpr uint8_t ESMRAM_CNTL_H_ERR     = 1 << 6; // Write 1 here to clear the error
	static constexpr uint8_t ESMRAM_CNTL_CACHE     = 1 << 5; // Hardwired to 1
	static constexpr uint8_t ESMRAM_CNTL_L1_EN     = 1 << 4; // Hardwired to 1
	static constexpr uint8_t ESMRAM_CNTL_L2_EN     = 1 << 3; // Hardwired to 1
	static constexpr uint8_t ESMRAM_CNTL_TSEG_1M   = 1 << 2; // Otherwise 512K
	static constexpr uint8_t ESMRAM_CNTL_TSEG_EN   = 1 << 0;
	static constexpr uint8_t ESMRAM_CNTL_WMASK     = 0xbb;
	static constexpr uint8_t ESMRAM_CNTL_LCK_WMASK = 0x38;

	static constexpr uint8_t DRB_REG_COUNT   = 8;
	static constexpr uint32_t DRB_GRANULARITY = 32 * 1024 * 1024;

	static constexpr uint32_t SSPACE_BLOCK_SIZE =  256;

	static constexpr uint32_t RRBAR_WMASK            =  0xfffc0000;
	static constexpr uint32_t RRBAR_BASE_MASK        =  0xfffc0000;
	static constexpr uint32_t RRBAR_SIZE             =  256 * 1024;
	static constexpr uint32_t RRBAR_SSPACE_SIZE_MASK =  0x000000ff;
	static constexpr uint32_t RRBAR_SSPACE_SIZE_256B =  0 << 0;

	static constexpr uint32_t GFX_APFLAG_PREFETCHABLE =  1 << 3;
	static constexpr uint32_t GFX_APFLAG_TYPE_MASK    = 0x00000006;
	static constexpr uint32_t GFX_APFLAG_TYPE_UPPER32 =  0 << 1;
	static constexpr uint32_t GFX_APFLAG_MSPACE       =  0 << 0;
	static constexpr uint32_t GFX_APFLAG_MASK         = 0xf;

	static constexpr uint32_t GFX_APBASE_WMASK       = 0xff000000;
	static constexpr uint32_t GFX_APBASE_32MB_WMASK  = 0xfe000000;
	static constexpr uint32_t GFX_APBASE_64MB_WMASK  = 0xfe000000;
	static constexpr uint32_t GFX_APBASE_128MB_WMASK = 0xf8000000;
	static constexpr uint32_t GFX_APBASE_256_WMASK   = 0xf0000000;

	static constexpr uint32_t GFX_APSIZE_32MB  = 7 <<  3;
	static constexpr uint32_t GFX_APSIZE_64MB  = 6 <<  3;
	static constexpr uint32_t GFX_APSIZE_128MB = 4 <<  3;
	static constexpr uint32_t GFX_APSIZE_256MB = 0 <<  3;
	static constexpr uint32_t GFX_APSIZE_MASK  = 0x38;

	static constexpr uint16_t GMCH_CNTL0_LPGP_SHIFT   = 12;
	static constexpr uint16_t GMCH_CNTL0_LPGP_MASK    = 7 << i82830_host_device::GMCH_CNTL0_LPGP_SHIFT;
	static constexpr uint16_t GMCH_CNTL0_LPGP_004     = 0x01;
	static constexpr uint16_t GMCH_CNTL0_LPGP_008     = 0x02;
	static constexpr uint16_t GMCH_CNTL0_LPGP_016     = 0x03;
	static constexpr uint16_t GMCH_CNTL0_LPGP_024     = 0x04;
	static constexpr uint16_t GMCH_CNTL0_LPGP_032     = 0x05;
	static constexpr uint16_t GMCH_CNTL0_SSPACE_EN    = 1 << 11;
	static constexpr uint16_t GMCH_CNTL0_AAG_EN       = 1 << 9;
	static constexpr uint16_t GMCH_CNTL0_RRBAR_EN     = 1 << 8;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_SHIFT = 4;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_MASK  = 7 << i82830_host_device::GMCH_CNTL0_IOQ_GC_SHIFT;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_004   = 0x00;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_008   = 0x01;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_016   = 0x02;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_024   = 0x03;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_032   = 0x04;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_048   = 0x05;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_064   = 0x06;
	static constexpr uint16_t GMCH_CNTL0_IOQ_GC_INF   = 0x07;
	static constexpr uint16_t GMCH_CNTL0_MDA_PRESENT  = 1 << 0;
	static constexpr uint16_t GMCH_CNTL0_WMASK        = 0x7b71;
	static constexpr uint16_t GMCH_CNTL0_LCK_WMASK    = 0x7371;

	static constexpr uint16_t GMCH_CNTL1_GMS_SHIFT   = 4;
	static constexpr uint16_t GMCH_CNTL1_GMS_MASK    = 7 << i82830_host_device::GMCH_CNTL1_GMS_SHIFT;
	static constexpr uint16_t GMCH_CNTL1_GMS_MEM_EN  = 0x01;
	static constexpr uint16_t GMCH_CNTL1_GMS_512KB   = 0x02;
	static constexpr uint16_t GMCH_CNTL1_GMS_1M      = 0x03;
	static constexpr uint16_t GMCH_CNTL1_GMS_8M      = 0x04;
	static constexpr uint16_t GMCH_CNTL1_IGD_OFF     = 1 << 3;
	static constexpr uint16_t GMCH_CNTL1_IGD_F1_EN   = 1 << 2;
	static constexpr uint16_t GMCH_CNTL1_IGD_VGA_OFF = 1 << 1;
	static constexpr uint16_t GMCH_CNTL1_IGD_GM_64MB = 1 << 0; // Otherwise 128MB

	template <typename T>
	i82830_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&cpu_tag, uint32_t max_ram_size, bool allocate_max_ram = false, uint32_t main_id = 0x80863575, uint8_t revision = 0x04, uint32_t subdevice_id = 0x00000000)
		: i82830_host_device(mconfig, tag, owner, clock)
	{
		set_ids_host(main_id, revision, subdevice_id);
		set_cpu_tag(std::forward<T>(cpu_tag));
		set_max_ram_size(max_ram_size, allocate_max_ram);
	}

	i82830_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_cpu_tag(T &&tag)
	{
		m_hostcpumem.set_tag(std::forward<T>(tag));
		m_hostcpu.set_tag(std::forward<T>(tag)); 
	}

	void enable_agp(bool enabled) { m_agp_enabled = enabled; }

	uint16_t get_gcc0() { return m_gcc0; }
	uint16_t get_gcc1() { return m_gcc1; }

	void set_max_ram_size(uint32_t max_ram_size, bool allocate_max_ram = false)
	{
		m_max_ram_size = max_ram_size;
		m_allocate_max_ram = allocate_max_ram;
	}
	inline uint32_t get_ram_size()
	{
		if(m_allocate_max_ram)
		{
			return m_max_ram_size;
		}
		else
		{
			uint32_t ram_size = 0;

			uint32_t drb_result = 0;
			for(int idx = 0; idx < i82830_host_device::DRB_REG_COUNT; idx++)
			{
				if(m_drb[idx] > drb_result)
					drb_result = m_drb[idx];
			}

			ram_size = drb_result * DRB_GRANULARITY;

			if(ram_size > m_max_ram_size)
				ram_size = m_max_ram_size;

			if(ram_size < i82830_host_device::MIN_RAM_SIZE)
				ram_size = i82830_host_device::MIN_RAM_SIZE;

			return std::min(m_max_ram_size, ram_size);
		}
	}
	template <typename T> inline T* get_ram_pointer()
	{
		static_assert(std::is_integral_v<T>);

		uint32_t base = 0x00000000 >> 2;
		
		return reinterpret_cast<T *>(&m_ram[base]);
	}
	inline uint32_t* get_ram_pointer() { return i82830_host_device::get_ram_pointer<uint32_t>(); }

	uint32_t get_tseg_size();
	uint32_t get_tseg_base();
	template <typename T> T* get_tseg_pointer()
	{
		static_assert(std::is_integral_v<T>);

		uint32_t base = i82830_host_device::get_tseg_base() >> 2;

		if(base > 0)
			return reinterpret_cast<T *>(&m_ram[base]);
		else
			return nullptr;
	}
	uint32_t* get_tseg_pointer() { return i82830_host_device::get_tseg_pointer<uint32_t>(); }

	uint32_t get_graphics_size();
	uint32_t get_graphics_base();
	template <typename T> T* get_graphics_pointer()
	{
		static_assert(std::is_integral_v<T>);

		uint32_t base = i82830_host_device::get_graphics_base() >> 2;

		if(base > 0)
			return reinterpret_cast<T *>(&m_ram[base]);
		else
			return nullptr;
	}
	uint32_t* get_graphics_pointer() { return i82830_host_device::get_graphics_pointer<uint32_t>(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void reset_all_mappings() override;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

	virtual uint8_t capptr_r() override ATTR_COLD;

private:

	required_device<device_memory_interface> m_hostcpumem;
	required_device<i386_device> m_hostcpu;

	uint32_t m_max_ram_size;
	bool m_allocate_max_ram;
	std::vector<uint32_t> m_ram;

	uint32_t m_rrbar;
	uint16_t m_gcc0;
	uint16_t m_gcc1;
	uint8_t m_fdhc;
	uint8_t m_pam[PAM_REG_COUNT];
	uint8_t m_drb[DRB_REG_COUNT];
	uint16_t m_dra;
	uint32_t m_drt;
	uint32_t m_drc;
	uint32_t m_dtc;
	uint8_t m_smram_cntl;
	uint8_t m_esmram_cntl;
	uint16_t m_errcmd;
	uint32_t m_acapid;
	uint32_t m_agpstat;
	uint32_t m_agpcmd;
	uint32_t m_agpctrl;
	uint32_t m_apbase = 0;
	uint8_t m_apsize = 0;
	uint32_t m_attbase;
	uint8_t m_amtt;
	uint8_t m_lptt;
	uint16_t m_creg_c2;
	bool m_agp_enabled;

	uint8_t m_sspace[SSPACE_BLOCK_SIZE];
	
	void set_default_values();

	void install_pam(address_space *memory_space, uint8_t attr, uint64_t memory_window_start, uint64_t memory_window_end);

	uint8_t sspace_r(offs_t offset);
	void sspace_w(offs_t offset, uint8_t data);

	void rrbar_map(address_map &map);
	
	uint32_t apbase_r();                                                  // 10-13h
	void apbase_w(uint32_t data);
	uint32_t rrbar_r();                                                   // 48-4bh
	void rrbar_w(uint32_t data);
	uint16_t gcc0_r();                                                    // 50-51h
	void gcc0_w(uint16_t data);
	uint16_t gcc1_r();                                                    // 52-53h
	void gcc1_w(uint16_t data);
	uint8_t fdhc_r();                                                     // 58h
	void fdhc_w(uint8_t data);
	uint8_t pam_r(offs_t offset);                                         // 59-6fh
	void pam_w(offs_t offset, uint8_t data);
	uint8_t drb_r(offs_t offset);                                         // 60-67h
	void drb_w(offs_t offset, uint8_t data);
	uint16_t dra_r();                                                     // 70-71h
	void dra_w(uint16_t data);
	uint32_t drt_r();                                                     // 78-7bh
	void drt_w(uint32_t data);
	uint32_t drc_r();                                                     // 7c-7fh
	void drc_w(uint32_t data);
	uint32_t dtc_r();                                                     // 8c-8fh
	void dtc_w(uint32_t data);
	uint8_t smram_r();                                                    // 90h
	void smram_w(uint8_t data);
	uint8_t esmramc_r();                                                  // 91h
	void esmramc_w(uint8_t data);
	uint16_t errsts_r();                                                  // 82-93h
	uint32_t acapid_r();                                                  // 94-95h
	uint32_t agpstat_r();                                                 // a0-a3h
	uint32_t agpcmd_r();                                                  // a4-a7h
	void agpcmd_w(uint32_t data);
	uint16_t errcmd_r();                                                  // a8-abh
	void errcmd_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t agpctrl_r();                                                 // b0-b3h
	void agpctrl_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t apsize_r();                                                   // b4h
	void apsize_w(uint8_t data);
	uint32_t attbase_r();                                                 // b8-bbh
	void attbase_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t amtt_r();                                                     // bch
	void amtt_w(uint8_t data);
	uint8_t lptt_r();                                                     // bdh
	void lptt_w(uint8_t data);
	uint16_t creg_c2_r();                                                 // c2h
	void creg_c2_w(uint16_t data);

};

DECLARE_DEVICE_TYPE(I82830_HOST, i82830_host_device)

#endif // MAME_WEBTV_I82830_HOST_H
