// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82801_IDE_H
#define MAME_WEBTV_I82801_IDE_H

#include "machine/pci.h"
#include "machine/idectrl.h"
#include "i82801_lpc.h"

class i82801_ide_device : public pci_device
{

public:

	static constexpr uint16_t PCI_SPACE_MEM_EN      = 0x02;
	static constexpr uint16_t PCI_SPACE_IO_EN       = 0x01;

	static constexpr uint16_t IDE_LEGACY_PCCMD_BASE = 0x01f0;
	static constexpr uint16_t IDE_LEGACY_PCCNL_BASE = 0x03f4;
	static constexpr uint16_t IDE_LEGACY_SCCMD_BASE = 0x0170;
	static constexpr uint16_t IDE_LEGACY_SCCNL_BASE = 0x0374;
	static constexpr uint32_t IDE_LEGACY_CMD_SIZE   = 0x0008;
	static constexpr uint32_t IDE_LEGACY_CNL_SIZE   = 0x0001;

	static constexpr uint8_t IDE_CMD_SIZE        = 0x0008;
	static constexpr uint8_t IDE_CNL_SIZE        = 0x0004;
	static constexpr uint8_t IDE_PBMR_OFFSET     = 0x0000;
	static constexpr uint8_t IDE_SBMR_OFFSET     = 0x0008;
	static constexpr uint8_t IDE_BMR_SIZE        = 0x0008;

	static constexpr uint32_t IDE_BASE_IS_IO        = 0x00000001;
	static constexpr uint32_t IDE_IO_BASE_MASK      = 0x0000fff8;

	static constexpr uint32_t IDE_EXBAR_MASK        = 0xfffffffc;
	static constexpr uint32_t IDE_EXBAR_SIZE        = 0x400;

	static constexpr uint32_t DEFAULT_IDE_IO_BASE  = i82801_ide_device::IDE_BASE_IS_IO;

	static constexpr uint8_t IDE_PI_SUPPORTS_BMR        = 1 << 7;
	static constexpr uint8_t IDE_PI_SOP_MODE_CAP_NATIVE = 1 << 3;
	static constexpr uint8_t IDE_PI_SOP_MODE_SEL_NATIVE = 1 << 2;
	static constexpr uint8_t IDE_PI_POP_MODE_CAP_NATIVE = 1 << 1;
	static constexpr uint8_t IDE_PI_POP_MODE_SEL_NATIVE = 1 << 0;
	static constexpr uint8_t IDE_PI_WMASK               = 0x05;

	static constexpr uint32_t IDE_TIM_DECODE_ENABLE  = 1 << 15;
	static constexpr uint32_t IDE_TIM_D1_USE_SLVTIM  = 1 << 14;
	static constexpr uint32_t IDE_TIM_IORDY_MASK     = 0x03;
	static constexpr uint32_t IDE_TIM_IORDY_SHIFT    = 12;
	static constexpr uint32_t IDE_TIM_IORDY_5CLK     = 0 << i82801_ide_device::IDE_TIM_IORDY_MASK;
	static constexpr uint32_t IDE_TIM_IORDY_4CLK     = 1 << i82801_ide_device::IDE_TIM_IORDY_MASK;
	static constexpr uint32_t IDE_TIM_IORDY_3CLK     = 2 << i82801_ide_device::IDE_TIM_IORDY_MASK;
	static constexpr uint32_t IDE_TIM_RECOVERY_MASK  = 0x03;
	static constexpr uint32_t IDE_TIM_RECOVERY_SHIFT = 8;
	static constexpr uint32_t IDE_TIM_RECOVERY_4CLK  = 0 << i82801_ide_device::IDE_TIM_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_TIM_RECOVERY_3CLK  = 1 << i82801_ide_device::IDE_TIM_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_TIM_RECOVERY_2CLK  = 2 << i82801_ide_device::IDE_TIM_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_TIM_RECOVERY_1CLK  = 3 << i82801_ide_device::IDE_TIM_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_TIM_D1_DMA_EN      = 1 << 7;
	static constexpr uint32_t IDE_TIM_D1_PP_EN       = 1 << 6; // Prefetch and posting enable
	static constexpr uint32_t IDE_TIM_D1_IORDY_EN    = 1 << 5;
	static constexpr uint32_t IDE_TIM_D1_FASTBANK    = 1 << 4;
	static constexpr uint32_t IDE_TIM_D0_DMA_EN      = 1 << 3;
	static constexpr uint32_t IDE_TIM_D0_PP_EN       = 1 << 2; // Prefetch and posting enable
	static constexpr uint32_t IDE_TIM_D0_IORDY_EN    = 1 << 1;
	static constexpr uint32_t IDE_TIM_D0_FASTBANK    = 1 << 0;

	static constexpr uint32_t IDE_SLVTIM_IORDY_5CLK          = 0;
	static constexpr uint32_t IDE_SLVTIM_IORDY_4CLK          = 1;
	static constexpr uint32_t IDE_SLVTIM_IORDY_3CLK          = 2;
	static constexpr uint32_t IDE_SLVTIM_IORDY_MASK          = 0x03;
	static constexpr uint32_t IDE_SLVTIM_RECOVERY_4CLK       = 0;
	static constexpr uint32_t IDE_SLVTIM_RECOVERY_3CLK       = 1;
	static constexpr uint32_t IDE_SLVTIM_RECOVERY_2CLK       = 2;
	static constexpr uint32_t IDE_SLVTIM_RECOVERY_1CLK       = 3;
	static constexpr uint32_t IDE_SLVTIM_RECOVERY_MASK       = 0x03;
	static constexpr uint32_t IDE_SLVTIM_SCD1_IORDY_SHIFT    = 6;
	static constexpr uint32_t IDE_SLVTIM_SCD1_IORDY_5CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_5CLK << i82801_ide_device::IDE_SLVTIM_SCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_IORDY_4CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_4CLK << i82801_ide_device::IDE_SLVTIM_SCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_IORDY_3CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_3CLK << i82801_ide_device::IDE_SLVTIM_SCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_RECOVERY_SHIFT = 4;
	static constexpr uint32_t IDE_SLVTIM_SCD1_RECOVERY_4CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_4CLK << i82801_ide_device::IDE_SLVTIM_SCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_RECOVERY_3CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_3CLK << i82801_ide_device::IDE_SLVTIM_SCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_RECOVERY_2CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_2CLK << i82801_ide_device::IDE_SLVTIM_SCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_SCD1_RECOVERY_1CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_1CLK << i82801_ide_device::IDE_SLVTIM_SCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_IORDY_SHIFT    = 2;
	static constexpr uint32_t IDE_SLVTIM_PCD1_IORDY_5CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_5CLK << i82801_ide_device::IDE_SLVTIM_PCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_IORDY_4CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_4CLK << i82801_ide_device::IDE_SLVTIM_PCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_IORDY_3CLK     = i82801_ide_device::IDE_SLVTIM_IORDY_3CLK << i82801_ide_device::IDE_SLVTIM_PCD1_IORDY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_RECOVERY_SHIFT = 0;
	static constexpr uint32_t IDE_SLVTIM_PCD1_RECOVERY_4CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_4CLK << i82801_ide_device::IDE_SLVTIM_PCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_RECOVERY_3CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_3CLK << i82801_ide_device::IDE_SLVTIM_PCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_RECOVERY_2CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_2CLK << i82801_ide_device::IDE_SLVTIM_PCD1_RECOVERY_SHIFT;
	static constexpr uint32_t IDE_SLVTIM_PCD1_RECOVERY_1CLK  = i82801_ide_device::IDE_SLVTIM_RECOVERY_1CLK << i82801_ide_device::IDE_SLVTIM_PCD1_RECOVERY_SHIFT;
	
	static constexpr uint32_t IDE_SDMA_CNTL_SCD1_DMA_EN = 1 << 3;
	static constexpr uint32_t IDE_SDMA_CNTL_SCD0_DMA_EN = 1 << 2;
	static constexpr uint32_t IDE_SDMA_CNTL_PCD1_DMA_EN = 1 << 1;
	static constexpr uint32_t IDE_SDMA_CNTL_PCD0_DMA_EN = 1 << 0;

	static constexpr uint32_t IDE_SDMA_TIM_CT2        = 2;
	static constexpr uint32_t IDE_SDMA_TIM_CT3        = 1;
	static constexpr uint32_t IDE_SDMA_TIM_CT4        = 0;
	static constexpr uint32_t IDE_SDMA_TIM_SCD1_SHIFT = 12;
	static constexpr uint32_t IDE_SDMA_TIM_SCD1_CT2   = i82801_ide_device::IDE_SDMA_TIM_CT2 << i82801_ide_device::IDE_SDMA_TIM_SCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_SCD1_CT3   = i82801_ide_device::IDE_SDMA_TIM_CT3 << i82801_ide_device::IDE_SDMA_TIM_SCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_SCD1_CT4   = i82801_ide_device::IDE_SDMA_TIM_CT4 << i82801_ide_device::IDE_SDMA_TIM_SCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_SCD0_SHIFT = 8;
	static constexpr uint32_t IDE_SDMA_TIM_SCD0_CT2   = i82801_ide_device::IDE_SDMA_TIM_CT2 << i82801_ide_device::IDE_SDMA_TIM_SCD0_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_SCD0_CT3   = i82801_ide_device::IDE_SDMA_TIM_CT3 << i82801_ide_device::IDE_SDMA_TIM_SCD0_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_SCD0_CT4   = i82801_ide_device::IDE_SDMA_TIM_CT4 << i82801_ide_device::IDE_SDMA_TIM_SCD0_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD1_SHIFT = 4;
	static constexpr uint32_t IDE_SDMA_TIM_PCD1_CT2   = i82801_ide_device::IDE_SDMA_TIM_CT2 << i82801_ide_device::IDE_SDMA_TIM_PCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD1_CT3   = i82801_ide_device::IDE_SDMA_TIM_CT3 << i82801_ide_device::IDE_SDMA_TIM_PCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD1_CT4   = i82801_ide_device::IDE_SDMA_TIM_CT4 << i82801_ide_device::IDE_SDMA_TIM_PCD1_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD0_SHIFT = 0;
	static constexpr uint32_t IDE_SDMA_TIM_PCD0_CT2   = i82801_ide_device::IDE_SDMA_TIM_CT2 << i82801_ide_device::IDE_SDMA_TIM_PCD0_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD0_CT3   = i82801_ide_device::IDE_SDMA_TIM_CT3 << i82801_ide_device::IDE_SDMA_TIM_PCD0_SHIFT;
	static constexpr uint32_t IDE_SDMA_TIM_PCD0_CT4   = i82801_ide_device::IDE_SDMA_TIM_CT4 << i82801_ide_device::IDE_SDMA_TIM_PCD0_SHIFT;

	static constexpr uint32_t IDE_CONFIG_SCSIG_NORMAL   = 1 << 18;
	static constexpr uint32_t IDE_CONFIG_SCSIG_TRISTATE = 1 << 18;
	static constexpr uint32_t IDE_CONFIG_SCSIG_DRIVELOW = 1 << 18;
	static constexpr uint32_t IDE_CONFIG_PCSIG_NORMAL   = 1 << 16;
	static constexpr uint32_t IDE_CONFIG_PCSIG_TRISTATE = 1 << 16;
	static constexpr uint32_t IDE_CONFIG_PCSIG_DRIVELOW = 1 << 16;
	static constexpr uint32_t IDE_CONFIG_SCD1_ATA100    = 1 << 15; // Overrides IDE_CONFIG_SCD1_ATA66
	static constexpr uint32_t IDE_CONFIG_SCD0_ATA100    = 1 << 14; // Overrides IDE_CONFIG_SCD0_ATA66
	static constexpr uint32_t IDE_CONFIG_PCD1_ATA100    = 1 << 13; // Overrides IDE_CONFIG_PCD1_ATA66
	static constexpr uint32_t IDE_CONFIG_PCD0_ATA100    = 1 << 12; // Overrides IDE_CONFIG_PCD0_ATA66
	static constexpr uint32_t IDE_CONFIG_WR_PINGPONG_EN = 1 << 10;
	static constexpr uint32_t IDE_CONFIG_SCS_80WIRE     = 1 <<  7;
	static constexpr uint32_t IDE_CONFIG_SCM_80WIRE     = 1 <<  6;
	static constexpr uint32_t IDE_CONFIG_PCS_80WIRE     = 1 <<  5;
	static constexpr uint32_t IDE_CONFIG_PCM_80WIRE     = 1 <<  4;
	static constexpr uint32_t IDE_CONFIG_SCD1_ATA66     = 1 <<  3; // Otherwise using ATA33
	static constexpr uint32_t IDE_CONFIG_SCD0_ATA66     = 1 <<  2; // Otherwise using ATA33
	static constexpr uint32_t IDE_CONFIG_PCD1_ATA66     = 1 <<  1; // Otherwise using ATA33
	static constexpr uint32_t IDE_CONFIG_PCD0_ATA66     = 1 <<  0; // Otherwise using ATA33

	template <typename T>
	i82801_ide_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, const char *ide0_master_dflt, const char *ide0_slave_dflt, bool ide0_fixed, const char *ide1_master_dflt, const char *ide1_slave_dflt, bool ide1_fixed, T &&bm_tag, uint32_t bm_space = AS_PROGRAM, uint32_t main_id = 0x808624cb, uint8_t revision = 0x02)
		: i82801_ide_device(mconfig, tag, owner, clock)
	{
		// 0x01: Mass Storage Controller; 0x018a: 0x01xx=IDE Controller, 0xxx8a=ISA Compatibility mode controller, supports both channels switched to PCI native mode, supports bus mastering 
		uint32_t device_class = 0x010100;
		// | 0x8a
		device_class |= (i82801_ide_device::IDE_PI_SUPPORTS_BMR | i82801_ide_device::IDE_PI_SOP_MODE_CAP_NATIVE | i82801_ide_device::IDE_PI_POP_MODE_CAP_NATIVE);

		set_ids(main_id, revision, device_class, 0x00000000);
		set_busmaster_tag(std::forward<T>(bm_tag), bm_space);

		i0_mdflt = ide0_master_dflt;
		i0_sdflt = ide0_slave_dflt;
		i0_fixed = ide0_fixed;

		i1_mdflt = ide1_master_dflt;
		i1_sdflt = ide1_slave_dflt;
		i1_fixed = ide1_fixed;
	}

	i82801_ide_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_busmaster_tag(T &&tag, uint32_t space) { m_bus_master_space.set_tag(std::forward<T>(tag), space); }

	auto pirq_callback() { return m_pirq_cb.bind(); }
	auto ide0_legacy_irq_callback() { return m_ide0_lirq_cb.bind(); }
	auto ide1_legacy_irq_callback() { return m_ide1_lirq_cb.bind(); }

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

	auto subsystem_id_callback() { return m_ssid_w_cb.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

private:

	required_device_array<bus_master_ide_controller_device, 2> m_ide;
	required_address_space m_bus_master_space;

	devcb_write_line m_ide0_lirq_cb;
	devcb_write_line m_ide1_lirq_cb;
	devcb_write8 m_pirq_cb;

	devcb_write32 m_ssid_w_cb;

	uint8_t m_pirq_pin;

	const char *i0_mdflt = nullptr;
	const char *i0_sdflt = nullptr;
	bool i0_fixed;
	const char *i1_mdflt = nullptr;
	const char *i1_sdflt = nullptr;
	bool i1_fixed;

	uint32_t m_pcmd_bar;
	uint32_t m_pcnl_bar;
	uint32_t m_scmd_bar;
	uint32_t m_scnl_bar;
	uint32_t m_bmr_bar;
	uint32_t m_exbar;
	uint16_t m_ide_timp;
	uint16_t m_ide_tims;
	uint8_t m_sidetim;
	uint8_t m_sdmac;
	uint16_t m_sdmatim;
	uint32_t m_ide_config;
	
	uint32_t exp[i82801_ide_device::IDE_EXBAR_SIZE];

	void set_default_values();
	void io_map(address_map &map);

	void map_ide_bus(address_map &map, uint8_t ide_bus_index, bool is_legacy, uint16_t cmd_io_addr, uint16_t cnl_io_addr, uint16_t bmr_io_addr);
	void map_empty_bus(address_map &map, bool is_legacy, uint16_t cmd_io_addr, uint16_t cnl_io_addr, uint16_t bmr_io_addr);

	uint8_t empty_read(offs_t offset);
	void empty_write(offs_t offset, uint8_t data);

	void irq_w(uint8_t irqno, int state);
	void set_ide0_irq(int state);
	void set_ide1_irq(int state);

	void pi_w(uint8_t data);
	void subvendor_w(uint16_t data);
	void subsystem_w(uint16_t data);
	uint32_t pcmd_bar_r();
	void pcmd_bar_w(uint32_t data);
	uint32_t pcnl_bar_r();
	void pcnl_bar_w(uint32_t data);
	uint32_t scmd_bar_r();
	void scmd_bar_w(uint32_t data);
	uint32_t scnl_bar_r();
	void scnl_bar_w(uint32_t data);
	uint32_t bmr_bar_r();
	void bmr_bar_w(uint32_t data);
	uint32_t exbar_r();
	void exbar_w(uint32_t data);
	uint16_t ide_timp_r();
	void ide_timp_w(uint16_t data);
	uint16_t ide_tims_r();
	void ide_tims_w(uint16_t data);
	uint8_t sidetim_r();
	void sidetim_w(uint8_t data);
	uint8_t sdmac_r();
	void sdmac_w(uint8_t data);
	uint16_t sdmatim_r();
	void sdmatim_w(uint16_t data);
	uint32_t ide_config_r();
	void ide_config_w(uint32_t data);

};

DECLARE_DEVICE_TYPE(I82801_IDE, i82801_ide_device)

#endif // MAME_WEBTV_I82801_IDE_H
