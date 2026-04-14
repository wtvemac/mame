// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82801_LPC_H
#define MAME_WEBTV_I82801_LPC_H

#include "cpu/i386/i386.h"
#include "machine/pci.h"
#include "machine/pic8259.h"
#include "machine/am9517a.h"
#include "machine/pit8253.h"
#include "machine/ds128x.h"
#include "machine/ds17x85.h"
#include "bus/isa/isa.h"
#include "machine/lpc-acpi.h"
#include "sound/spkrdev.h"
#include "speaker.h"

class i82801_lpc_device : public pci_device
{

public:

	static constexpr uint8_t PIRQ_COUNT        = 0x08;
	static constexpr uint8_t PIRQ_ROUTE_MASK   = 0x0f;
	static constexpr uint8_t PIRQ_NOT_ROUTED   = 1  << 7;
	static constexpr uint8_t PIRQ_ROUTE_IRQ3   = 3  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ4   = 4  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ5   = 5  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ6   = 6  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ7   = 7  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ9   = 9  << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ10  = 10 << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ11  = 11 << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ12  = 12 << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ14  = 14 << 0;
	static constexpr uint8_t PIRQ_ROUTE_IRQ15  = 15 << 0;
	static constexpr uint8_t INT_PIN_NONE      = 0;
	static constexpr uint8_t INT_PIN_A         = 1;
	static constexpr uint8_t INT_PIN_B         = 2;
	static constexpr uint8_t INT_PIN_C         = 3;
	static constexpr uint8_t INT_PIN_D         = 4;
	static constexpr uint8_t INT_PIN_E         = 5;
	static constexpr uint8_t INT_PIN_F         = 6;
	static constexpr uint8_t INT_PIN_G         = 7;
	static constexpr uint8_t INT_PIN_H         = 8;
	static constexpr uint8_t PIRQ_SELECT_NONE  = 0;
	static constexpr uint8_t PIRQ_SELECT_A     = 1;
	static constexpr uint8_t PIRQ_SELECT_B     = 2;
	static constexpr uint8_t PIRQ_SELECT_C     = 3;
	static constexpr uint8_t PIRQ_SELECT_D     = 4;
	static constexpr uint8_t PIRQ_SELECT_E     = 5;
	static constexpr uint8_t PIRQ_SELECT_F     = 6;
	static constexpr uint8_t PIRQ_SELECT_G     = 7;
	static constexpr uint8_t PIRQ_SELECT_H     = 8;

	// PCI bus memory range for the Firmware Controller Hub
	// ST M50FW080 8-Mbit on the MSNTV2
	static constexpr uint32_t FWH_F8_ENABLE = 1 <<  7;
	static constexpr uint32_t FWH_F0_ENABLE = 1 <<  6;
	static constexpr uint32_t FWH_E8_ENABLE = 1 <<  5;
	static constexpr uint32_t FWH_E0_ENABLE = 1 <<  4;
	static constexpr uint32_t FWH_D8_ENABLE = 1 <<  3;
	static constexpr uint32_t FWH_D0_ENABLE = 1 <<  2;
	static constexpr uint32_t FWH_C8_ENABLE = 1 <<  1;
	static constexpr uint32_t FWH_C0_ENABLE = 1 <<  0;

	static constexpr uint32_t GCNTL_PCPCIB_SEL  = 1 << 25;
	static constexpr uint32_t GCNTL_HIDE_ISA    = 1 << 24;
	static constexpr uint32_t GCNTL_FERR_MUX_EN = 1 << 21;
	static constexpr uint32_t GCNTL_COPR_ERR_EN = 1 << 13;
	static constexpr uint32_t GCNTL_IRQ1L_EN    = 1 << 12;
	static constexpr uint32_t GCNTL_IRQ12L_EN   = 1 << 11;
	static constexpr uint32_t GCNTL_APIC_EN     = 1 <<  8;
	static constexpr uint32_t GCNTL_XAPIC_EN    = 1 <<  7;
	static constexpr uint32_t GCNTL_ALTACC_EN   = 1 <<  6;
	static constexpr uint32_t GCNTL_DCB_EN      = 1 <<  2;
	static constexpr uint32_t GCNTL_DTE_EN      = 1 <<  1;
	static constexpr uint32_t GCNTL_POS_DEC_EN  = 1 <<  0;

	static constexpr uint32_t GPIO_ENABLE = 1 <<  4;

	static constexpr uint32_t ACPI_ENABLE       = 1 <<  4; // Enable I/O ports 4eh and 4fh
	static constexpr uint32_t ACPI_SCI_IRQ_MASK = 0x7;     // Mask on the ACPI_CNTL register to reveal the IRQ value
	static constexpr uint32_t ACPI_SCI_IRQ9     = 0x0;
	static constexpr uint32_t ACPI_SCI_IRQ10    = 0x1;
	static constexpr uint32_t ACPI_SCI_IRQ11    = 0x2;
	static constexpr uint32_t ACPI_SCI_IRQ20    = 0x4;     // IRQ20 only available if APIC is enabled
	static constexpr uint32_t ACPI_SCI_IRQ21    = 0x5;     // IRQ21 only available if APIC is enabled
	static constexpr uint32_t ACPI_SCI_IRQ22    = 0x6;     // IRQ22 only available if APIC is enabled
	static constexpr uint32_t ACPI_SCI_IRQ23    = 0x7;     // IRQ23 only available if APIC is enabled

	static constexpr uint32_t APIC_REG_COUNT         = 64;
	static constexpr uint32_t APIC_REG_SIZE          = 0x10;
	static constexpr uint32_t APIC_REG_MAP_SIZE      = i82801_lpc_device::APIC_REG_COUNT * i82801_lpc_device::APIC_REG_SIZE;
	static constexpr uint32_t APIC_DEFAULT_BASE      = 0xfec00000;
	static constexpr uint32_t APIC_REG_ADDR_OFFSET   = 0x02;
	static constexpr uint32_t APIC_REG_ADDR          = 0x00;
	static constexpr uint32_t APIC_REG_DATA          = 0x01;
	static constexpr uint32_t APIC_REG_LAPIC_ID      = 0x02; // Local APIC ID Register
	static constexpr uint32_t APIC_REG_LAPIC_VER     = 0x03; // Local APIC Version Register
	static constexpr uint32_t APIC_REG_TPR           = 0x08; // Task Priority Register (TPR)
	static constexpr uint32_t APIC_REG_APR           = 0x09; // Arbitration Priority Register (APR)
	static constexpr uint32_t APIC_REG_PPR           = 0x0a; // Processor Priority Register (PPR)
	static constexpr uint32_t APIC_REG_EOI           = 0x0b; // EOI register
	static constexpr uint32_t APIC_REG_RDD           = 0x0c; // Remote Read Register (RRD)
	static constexpr uint32_t APIC_REG_LDR           = 0x0d; // Logical Destination Register
	static constexpr uint32_t APIC_REG_DFR           = 0x0e; // Destination Format Register
	static constexpr uint32_t APIC_REG_SIVR          = 0x0f; // Spurious Interrupt Vector Register
	static constexpr uint32_t APIC_REG_ISR           = 0x10; // In-Service Register (ISR)
	static constexpr uint32_t APIC_REG_TMR           = 0x18; // Trigger Mode Register (TMR)
	static constexpr uint32_t APIC_REG_IRR           = 0x20; // Interrupt Request Register (IRR)
	static constexpr uint32_t APIC_REG_ESR           = 0x28; // Error Status Register
	static constexpr uint32_t APIC_REG_LVT_CMCI      = 0x2f; // LVT Corrected Machine Check Interrupt (CMCI) Register
	static constexpr uint32_t APIC_REG_ICR           = 0x30; // Interrupt Command Register (ICR)
	static constexpr uint32_t APIC_REG_LVT_TMR       = 0x32; // LVT Timer Register
	static constexpr uint32_t APIC_REG_LVT_TSR       = 0x33; // LVT Thermal Sensor Register
	static constexpr uint32_t APIC_REG_LVT_PMCR      = 0x34; // LVT Thermal Sensor Register
	static constexpr uint32_t APIC_REG_LVT_LINT0     = 0x35; // LVT LINT0 Register
	static constexpr uint32_t APIC_REG_LVT_LINT1     = 0x36; // LVT LINT1 Register
	static constexpr uint32_t APIC_REG_LVT_ERR       = 0x37; // LVT Error Register
	static constexpr uint32_t APIC_REG_ICNT          = 0x38; // Initial Count Register (for Timer)
	static constexpr uint32_t APIC_REG_CCNT          = 0x39; // Current Count Register (for Timer)
	static constexpr uint32_t APIC_REG_DCR           = 0x3e; // Divide Configuration Register (for Timer)
	static constexpr uint32_t APIC_SIVR_LOCAL_EN     = 1 << 8;
	static constexpr uint32_t APIC_SIVR_FASTPROC     = 1 << 9;
	static constexpr uint32_t APIC_SIVR_SVEC_MASK    = 0xff;

	static constexpr uint32_t GPIO_REG_MAPS_SIZE = 0x40;
	static constexpr uint32_t GPIO_BASE_IS_IO    = 0x00000001;
	static constexpr uint32_t GPIO_IO_BASE_MASK  = 0x0000ffc0;

	static constexpr uint32_t LPC_CNF2_ENABLE   = 1 << 13; // Enable I/O ports 4eh and 4fh
	static constexpr uint32_t LPC_CNF1_ENABLE   = 1 << 12; // Enable I/O ports 2eh and 2fh Super I/O (SMSC LPC47M192 on the MSNTV2)
	static constexpr uint32_t LPC_MC_ENABLE     = 1 << 11; // Enable I/O ports 62h and 66h
	static constexpr uint32_t LPC_KBC_ENABLE    = 1 << 10; // Enable I/O ports 60h and 64h Keyboard
	static constexpr uint32_t LPC_GAMEH_ENABLE  = 1 <<  9; // Enable I/O ports 208h-20fh
	static constexpr uint32_t LPC_GAMEL_ENABLE  = 1 <<  8; // Enable I/O ports 200h-207h
	static constexpr uint32_t LPC_ADLIB_ENABLE  = 1 <<  7; // Enable I/O ports 388h-38bh
	static constexpr uint32_t LPC_MSS_ENABLE    = 1 <<  6; // Enable MSS I/O ports
	static constexpr uint32_t LPC_MIDI_ENABLE   = 1 <<  5; // Enable MIDI I/O ports
	static constexpr uint32_t LPC_SB16_ENABLE   = 1 <<  4; // Enable SB16 I/O ports
	static constexpr uint32_t LPC_FDD_ENABLE    = 1 <<  3; // Enable FDD I/O ports
	static constexpr uint32_t LPC_LPT_ENABLE    = 1 <<  2; // Enable LPT I/O ports
	static constexpr uint32_t LPC_COMB_ENABLE   = 1 <<  1; // Enable COM B (COM2/ttyS1) I/O ports
	static constexpr uint32_t LPC_COMA_ENABLE   = 1 <<  0; // Enable COM A (COM1/ttyS0) I/O ports

	static constexpr uint32_t RTC_UPPER128_LOCK   = 1 <<  4; // Needs hardware reset to unset. Not  implemented.
	static constexpr uint32_t RTC_LOWER128_LOCK   = 1 <<  3; // Needs hardware reset to unset. Not  implemented.
	static constexpr uint32_t RTC_UPPER128_ENABLE = 1 <<  2;

	static constexpr uint32_t NMI_IN_MASK          = 0x0f;
	static constexpr uint32_t NMI_SC_SERR_NMI_STS  = 1 << 7;
	static constexpr uint32_t NMI_SC_IOCHK_SMI_STS = 1 << 6;
	static constexpr uint32_t NMI_SC_TMR2_OUT_STS  = 1 << 5;
	static constexpr uint32_t NMI_SC_REF_TOGGLE    = 1 << 4;
	static constexpr uint32_t NMI_SC_IOCHK_NMI_EN  = 1 << 3;
	static constexpr uint32_t NMI_SC_PCI_SERR_EN   = 1 << 2;
	static constexpr uint32_t NMI_SC_SPKR_DAT_EN   = 1 << 1;
	static constexpr uint32_t NMI_SC_TIM_CNT2_EN   = 1 << 0;

	static constexpr XTAL LPC_8254_CLOCK = XTAL(14'318'181);
	static constexpr XTAL RTC_CLOCK      = XTAL(32'768);

	template <typename T>
	i82801_lpc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&cpu_tag, uint32_t main_id = 0x808624c0, uint8_t revision = 0x00, uint32_t subdevice_id = 0x00000000)
		: i82801_lpc_device(mconfig, tag, owner, clock)
	{
		// 0x06: Bridge; 0x0001: ISA Bridge
		uint32_t device_class = 0x060001;

		set_ids(main_id, revision, device_class, subdevice_id);
		set_cpu_tag(std::forward<T>(cpu_tag));
	}

	i82801_lpc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_cpu_tag(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }

	void line_reset_w(int state) { m_hostcpu->set_input_line(INPUT_LINE_RESET, state); }
	void line_a20_w(int state) { m_hostcpu->set_input_line(INPUT_LINE_A20, state); }

	void pirq_w(offs_t offset, uint8_t state);

	void irq1_w(int state);
	void irq2_w(int state);
	void irq3_w(int state);
	void irq4_w(int state);
	void irq5_w(int state);
	void irq6_w(int state);
	void irq7_w(int state);
	void irq8_w(int state);
	void irq9_w(int state);
	void irq10_w(int state);
	void irq11_w(int state);
	void irq12_w(int state);
	void irq13_w(int state);
	void irq14_w(int state);
	void irq15_w(int state);

	void log_post_codes(bool enabled) { m_log_post_codes = enabled; }

	void set_dummydelay_ioaddr(uint16_t ioaddr) { m_dummydelay_ioaddr = ioaddr; }

	IRQ_CALLBACK_MEMBER(irq_acknowledge);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual void device_config_complete() override;

	virtual void reset_all_mappings() override;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

private:
	
	required_device<cpu_device> m_hostcpu;
	required_device<pic8259_device> m_pic_master;
	required_device<pic8259_device> m_pic_slave;
	required_device<am9517a_device> m_dmac_master;
	required_device<am9517a_device> m_dmac_slave;
	required_device<pit8254_device> m_pit;
	required_memory_region m_bios_region;
	required_device<ds12885ext_device> m_rtc;
	required_device<isa16_device> m_lpcbus;
	required_device<lpc_acpi_device> m_acpi;
	required_device<speaker_sound_device> m_pc_sound;

	bool m_log_post_codes = false;

	int8_t m_dmac_cur_chan;
	int m_dmac_cur_eop;
	uint16_t m_dmac_high_byte;
	uint8_t m_dmac_page[16];
	uint8_t m_dmac_master_paddr[4];
	uint8_t m_dmac_slave_paddr[4];
	uint8_t m_rtc_index;
	uint8_t m_rtc_conf;
	bool m_nmi_enable;
	uint8_t m_acpi_cntl;
	uint32_t m_pmbase;
	uint32_t m_gpio_base;
	uint32_t m_fwh_sel1;
	uint32_t m_gen_cntl;
	uint32_t m_gpi_rout;
	uint16_t m_bios_cntl;
	uint16_t m_pci_dma_cfg;
	uint16_t m_gen1_dec;
	uint16_t m_lpc_en;
	uint16_t m_gen2_dec;
	uint16_t m_fwh_sel2;
	uint16_t m_func_dis;
	uint16_t m_gen_pmcon_1;
	uint16_t m_mon_trp_rng[4];
	uint16_t m_mon_trp_msk;
	uint8_t m_pirq_rout[PIRQ_COUNT];
	uint8_t m_tco_cntl;
	uint8_t m_gpio_cntl;
	uint8_t m_serirq_cntl;
	uint8_t m_d31_err_cfg;
	uint8_t m_d31_err_sts;
	uint8_t m_gen_sta;
	uint8_t m_back_cntl;
	uint8_t m_lpc_if_com_range;
	uint8_t m_lpc_if_fdd_lpt_range;
	uint8_t m_lpc_if_sound_range;
	uint8_t m_fwh_dec_en1;
	uint8_t m_fwh_dec_en2;
	uint8_t m_siu_config_port;
	uint8_t m_gen_pmcon_2;
	uint8_t m_gen_pmcon_3;
	uint8_t m_apm_cnt;
	uint8_t m_apm_sts;
	uint8_t m_mon_fwd_en;
	uint8_t m_nmi_sc;
	int m_siu_config_state;
	uint16_t m_eisa_irq_mode;
	uint32_t m_gpio_use_sel1;
	uint32_t m_gpio_io_sel1;
	uint32_t m_gpio_lvl1;
	uint32_t m_gpio_ttl;
	uint32_t m_gpio_blink;
	uint32_t m_gpio_inv;
	uint32_t m_gpio_use_sel2;
	uint32_t m_gpio_io_sel2;
	uint32_t m_gpio_lvl2;
	uint16_t m_dummydelay_ioaddr;
	uint32_t m_apic_reg[APIC_REG_COUNT];
	bool m_apic_io_en;
	bool m_apic_xio_en;
	bool m_apic_local_en;
	uint8_t m_apic_svec;

	void set_default_values();

	void misc_map(address_map &map);
	void gpio_map(address_map &map);
	void apic_map(address_map &map);

	void route_irq(uint8_t pirq_rout, int state);

	uint8_t get_slave_ack(offs_t offset);

	uint8_t dmac_page_r(offs_t offset);
	void dmac_page_w(offs_t offset, uint8_t data);
	void dmac_chan_select(int8_t channel, int state);
	void dmac_eop_w(int state);
	void dmac_hreq_w(int state);
	uint8_t dmac_master_byte_r(offs_t offset);
	void dmac_master_byte_w(offs_t offset, uint8_t data);
	uint8_t dmac_slave_byte_r(offs_t offset);
	void dmac_slave_byte_w(offs_t offset, uint8_t data);
	uint8_t dmac_io0_r();
	void dmac_io0_w(uint8_t data);
	uint8_t dmac_io1_r();
	void dmac_io1_w(uint8_t data);
	uint8_t dmac_io2_r();
	void dmac_io2_w(uint8_t data);
	uint8_t dmac_io3_r();
	void dmac_io3_w(uint8_t data);
	uint8_t dmac_io5_r();
	void dmac_io5_w(uint8_t data);
	uint8_t dmac_io6_r();
	void dmac_io6_w(uint8_t data);
	uint8_t dmac_io7_r();
	void dmac_io7_w(uint8_t data);
	void dmac_dack0_w(int state);
	void dmac_dack1_w(int state);
	void dmac_dack2_w(int state);
	void dmac_dack3_w(int state);
	void dmac_dack4_w(int state);
	void dmac_dack5_w(int state);
	void dmac_dack6_w(int state);
	void dmac_dack7_w(int state);
	uint8_t dmac2_r(offs_t offset);
	void dmac2_w(offs_t offset, uint8_t data);
	void pit_counter1(int state);
	void pit_counter2(int state);
	void isa_iochck_w(int state);
	void pc_speaker_state_w(uint8_t data);
	uint8_t eisa_irq_r(offs_t offset);
	void eisa_irq_w(offs_t offset, uint8_t data);
	void post_code_w(uint8_t data);
	void update_sound();
	void map_bios(address_space *memory_space, uint32_t start, uint32_t end, int idsel);

	//
	// configuration space registers
	//

	uint32_t pmbase_r();                                                             // 40h
	void pmbase_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t acpi_cntl_r();                                                           // 44h
	void acpi_cntl_w(uint8_t data);
	uint16_t bios_cntl_r();                                                          // 4eh
	void bios_cntl_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t tco_cntl_r();                                                            // 54h
	void tco_cntl_w(uint8_t data);
	uint32_t gpio_base_r();                                                          // 58h
	void gpio_base_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t gpio_cntl_r();                                                           // 5ch
	void gpio_cntl_w(uint8_t data);
	uint8_t pirq_rout_r(offs_t offset);                                              // 60-63h
	void pirq_rout_w(offs_t offset, uint8_t data);
	uint8_t serirq_cntl_r();                                                         // 64h
	void serirq_cntl_w(uint8_t data);
	uint8_t pirq2_rout_r(offs_t offset);                                             // 68-6bh
	void pirq2_rout_w(offs_t offset, uint8_t data);
	uint8_t d31_err_cfg_r();                                                         // 88h
	void d31_err_cfg_w(uint8_t data);
	uint8_t d31_err_sts_r();                                                         // 8ah
	void d31_err_sts_w(uint8_t data);
	uint16_t pci_dma_cfg_r();                                                        // 90h
	void pci_dma_cfg_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t gen_pmcon_1_r();                                                        // a0h
	void gen_pmcon_1_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t gen_pmcon_2_r();                                                         // a2h
	void gen_pmcon_2_w(uint8_t data);
	uint8_t gen_pmcon_3_r();                                                         // a4h
	void gen_pmcon_3_w(uint8_t data);
	uint8_t apm_cnt_r();                                                             // b2h
	void apm_cnt_w(uint8_t data);
	uint8_t apm_sts_r();                                                             // b3h
	void apm_sts_w(uint8_t data);
	uint32_t gpi_rout_r();                                                           // b8h
	void gpi_rout_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t mon_fwd_en_r();                                                          // c0h
	void mon_fwd_en_w(uint8_t data);
	uint16_t mon_trp_rng_r(offs_t offset);                                           // c4-cah
	void mon_trp_rng_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t mon_trp_msk_r();                                                        // cch
	void mon_trp_msk_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t gen_cntl_r();                                                           // d0h
	void gen_cntl_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint8_t gen_sta_r();                                                             // d4h
	void gen_sta_w(uint8_t data);
	uint8_t back_cntl_r();                                                           // d5h
	void back_cntl_w(uint8_t data);
	uint8_t rtc_conf_r();                                                            // d8h
	void rtc_conf_w(uint8_t data);
	uint8_t lpc_if_com_range_r();                                                    // e0h
	void lpc_if_com_range_w(uint8_t data);
	uint8_t lpc_if_fdd_lpt_range_r();                                                // e1h
	void lpc_if_fdd_lpt_range_w(offs_t offset, uint8_t data, uint8_t mem_mask = ~0);
	uint8_t lpc_if_sound_range_r();                                                  // e2h
	void lpc_if_sound_range_w(offs_t offset, uint8_t data, uint8_t mem_mask = ~0);
	uint8_t fwh_dec_en1_r();                                                         // e3h
	void fwh_dec_en1_w(uint8_t data);
	uint16_t gen1_dec_r();                                                           // e4h
	void gen1_dec_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t lpc_en_r();                                                             // e6h
	void lpc_en_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint32_t fwh_sel1_r();                                                           // e8h
	void fwh_sel1_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint16_t gen2_dec_r();                                                           // ech
	void gen2_dec_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t fwh_sel2_r();                                                           // eeh
	void fwh_sel2_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t fwh_dec_en2_r();                                                         // f0h
	void fwh_dec_en2_w(uint8_t data);
	uint16_t func_dis_r();                                                           // f2h
	void func_dis_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	
	//
	// I/O space registers
	//

	uint8_t nmi_sc_r();                                                              // 61h
	void nmi_sc_w(uint8_t data);
	uint8_t rtc_nmi_r(offs_t offset);                                                // 70-73h
	void rtc_nmi_w(offs_t offset, uint8_t data);
	void rtc_data_w(u8 data);

	uint32_t gpio_use_sel1_r();
	void gpio_use_sel1_w(uint32_t data);
	uint32_t gpio_io_sel1_r();
	void gpio_io_sel1_w(uint32_t data);
	uint32_t gpio_lvl1_r();
	void gpio_lvl1_w(uint32_t data);
	uint32_t gpio_ttl_r();
	void gpio_ttl_w(uint32_t data);
	uint32_t gpio_blink_r();
	void gpio_blink_w(uint32_t data);
	uint32_t gpio_inv_r();
	void gpio_inv_w(uint32_t data);
	uint32_t gpio_use_sel2_r();
	void gpio_use_sel2_w(uint32_t data);
	uint32_t gpio_io_sel2_r();
	void gpio_io_sel2_w(uint32_t data);
	uint32_t gpio_lvl2_r();
	void gpio_lvl2_w(uint32_t data);
	uint8_t dummydelay_r(offs_t offset);
	void dummydelay_w(offs_t offset, uint8_t data);

	uint8_t apic_acknowledge();
	uint32_t apic_reg_data_r(offs_t offset);
	void apic_reg_data_w(offs_t offset, uint32_t data);


};

DECLARE_DEVICE_TYPE(I82801_LPC, i82801_lpc_device)

#endif // MAME_WEBTV_I82801_LPC_H
