// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82801_ETH_H
#define MAME_WEBTV_I82801_ETH_H

#pragma once

#include "bus/pci/pci_slot.h"
#include "dinetwork.h"
#include "i82801_lpc.h"

// 82562EZ/82562ET: 64 registers
// 82562EX/82562EP/82562EM: 256 registers

class i82801_eth_eeprom_device : public device_t, public device_nvram_interface
{

public:

	i82801_eth_eeprom_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, bool extended_reg_count = false);

	int cs_r();
	void cs_w(int state);
	int sk_r();
	void sk_w(int state);
	int do_r();
	void di_w(int state);

	void set_write_enable(bool write_en);
	uint16_t read_reg(offs_t offset);
	void write_reg(offs_t offset, uint16_t data);
	void erase_reg(offs_t offset);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void nvram_default() override;
	virtual bool nvram_read(util::read_stream &file) override;
	virtual bool nvram_write(util::write_stream &file) override;

private:

	static constexpr uint16_t EEPROM_REG_COUNT      = 0x040;
	static constexpr uint16_t EEPROM_EXREG_COUNT    = 0x100;

	static constexpr uint8_t EEPROM_CMD_MASK        = 0x03;
	static constexpr uint8_t EEPROM_CMD_BIT_COUNT   = 2;
	static constexpr uint8_t EEPROM_CMD_OTHER       = 0x00;
	static constexpr uint8_t EEPROM_CMD_WRITE       = 0x01;
	static constexpr uint8_t EEPROM_CMD_READ        = 0x02;
	static constexpr uint8_t EEPROM_CMD_ERASE       = 0x03;
	static constexpr uint8_t EEPROM_CMD_OTHER_WD    = 0x00;
	static constexpr uint8_t EEPROM_CMD_OTHER_EALL  = 0x10;
	static constexpr uint8_t EEPROM_CMD_OTHER_WALL  = 0x20;
	static constexpr uint8_t EEPROM_CMD_OTHER_WE    = 0x30;
	static constexpr uint8_t EEPROM_ADDR6_MASK      = 0x3f;
	static constexpr uint8_t EEPROM_ADDR6_BIT_COUNT = 6;
	static constexpr uint8_t EEPROM_ADDR8_MASK      = 0xff;
	static constexpr uint8_t EEPROM_ADDR8_BIT_COUNT = 8;

	static constexpr uint16_t EEPROM_DATA_MASK     = 0xffff;
	static constexpr uint8_t EEPROM_DATA_BIT_COUNT = 16;

	enum eeprom_state_t : uint8_t
	{
		START_OFF = 0,
		START_IDLE,
		STATE_COMMAND,
		STATE_ADDRESS,
		STATE_DUMMY_BIT_WAIT,
		STATE_ADDRESS_READY,
		STATE_DATA_IN,
		STATE_DATA_OUT,
		STATE_DONE
	};

	std::unique_ptr<uint16_t[]> m_eeprom_data;

	uint16_t m_register_count;
	uint32_t m_eeprom_size;

	bool m_write_en;

	bool m_cs;
	bool m_sk;
	bool m_do;
	bool m_di;

	uint8_t m_state;

	int8_t m_bit_position;
	uint16_t m_bit_mask;
	uint8_t m_command;
	uint8_t m_address;
	uint16_t m_data;

	void set_idle();
	void start_command();
	void end_command();
	void process_other_command(uint16_t command);

};

class i82801_eth_device : public pci_card_device, public device_network_interface
{

public:

	static constexpr uint32_t CSR_SIZE             = 0x20;

	static constexpr uint32_t CSR_BASE_IS_IO       = 1 << 0;
	static constexpr uint32_t CSR_BASE_IS_MEM      = 0 << 0;
	static constexpr uint32_t CSR_IO_BASE_MASK     = 0x0000ffc0;
	static constexpr uint32_t CSR_MEM_BASE_MASK    = 0xfffff000;
	static constexpr uint32_t CSR_MEM_TYPE_MASK    = 0x00000006;
	static constexpr uint32_t CSR_MEM_TYPE_ANY     = 0 << 2;
	static constexpr uint32_t CSR_MEM_PREFETCHABLE = 1 << 3;

	static constexpr uint32_t EEPROM_DO_BITPOS = 3;
	static constexpr uint32_t EEPROM_DO = 1 << EEPROM_DO_BITPOS;
	static constexpr uint32_t EEPROM_DI_BITPOS = 2;
	static constexpr uint32_t EEPROM_DI = 1 << EEPROM_DI_BITPOS;
	static constexpr uint32_t EEPROM_CS_BITPOS = 1;
	static constexpr uint32_t EEPROM_CS = 1 << EEPROM_CS_BITPOS;
	static constexpr uint32_t EEPROM_SK_BITPOS = 0;
	static constexpr uint32_t EEPROM_SK = 1 << EEPROM_SK_BITPOS;

	static constexpr uint32_t DEFAULT_CSR_MEM_BASE = i82801_eth_device::CSR_MEM_PREFETCHABLE | i82801_eth_device::CSR_MEM_TYPE_ANY | i82801_eth_device::CSR_BASE_IS_MEM;
	static constexpr uint32_t DEFAULT_CSR_IO_BASE  = i82801_eth_device::CSR_BASE_IS_IO;

	template <typename T>
	i82801_eth_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&bm_tag, uint32_t bm_space = AS_PROGRAM, uint32_t main_id = 0x80862449, uint8_t revision = 0x82)
		: i82801_eth_device(mconfig, tag, owner, clock)
	{
		// 0x02: Network Controller; 0x0000: Ethernet Controller
		uint32_t device_class = 0x020000;

		set_ids(main_id, revision, device_class, 0x00000000);
		set_busmaster_tag(std::forward<T>(bm_tag), bm_space);
	}

	i82801_eth_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_busmaster_tag(T &&tag, uint32_t space) { m_dma_space.set_tag(std::forward<T>(tag), space); }

	auto pirq_callback() { return m_pirq_cb.bind(); }

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

	void set_default_link_state(bool active) { 	m_default_link_state = active; }
	void set_link_state(bool active);
	void set_phy_link_state(offs_t phy_offset, bool active);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

	virtual uint8_t capptr_r() override ATTR_COLD;

	virtual int recv_start_cb(uint8_t *frame, int frame_len) override;
	virtual void send_complete_cb(int result) override;

private:

	required_address_space m_dma_space;
	required_device<i82801_eth_eeprom_device> m_eeprom;

	devcb_write8 m_pirq_cb;

	uint8_t m_pirq_pin;
	
	uint32_t m_csr_membase;
	uint32_t m_csr_iobase;

	// LAN Connect (PLC) is similar to Intel's AC97 AC-Link where we're talking to ICs that interface with the physical world.
	
	// Registers for the Intel 8256ET PHY

	static constexpr uint8_t PLC_REGISTER_CNTL         = 0;
	static constexpr uint8_t PLC_REGISTER_STATUS       = 1;
	static constexpr uint8_t PLC_REGISTER_ID1          = 2;
	static constexpr uint8_t PLC_REGISTER_ID2          = 3;
	static constexpr uint8_t PLC_REGISTER_AUTONEG_AD   = 4;
	static constexpr uint8_t PLC_REGISTER_AUTONEG_LP   = 5;
	static constexpr uint8_t PLC_REGISTER_AUTONEG_EXP  = 6;
	static constexpr uint8_t PLC_REGISTER_PHY_STATCNTL = 16;
	static constexpr uint8_t PLC_REGISTER_PHY_SUCNTL1  = 17;
	static constexpr uint8_t PLC_REGISTER_PHY_ADDR     = 18;
	static constexpr uint8_t PLC_REGISTER_RFC_CNT      = 19;
	static constexpr uint8_t PLC_REGISTER_RDC_CNT      = 20;
	static constexpr uint8_t PLC_REGISTER_RERR_CNT     = 21;
	static constexpr uint8_t PLC_REGISTER_RSERR_CNT    = 22;
	static constexpr uint8_t PLC_REGISTER_RPEOF_CNT    = 23;
	static constexpr uint8_t PLC_REGISTER_REOF_CNT     = 24;
	static constexpr uint8_t PLC_REGISTER_TJAB_CNT     = 25;
	static constexpr uint8_t PLC_REGISTER_PHY_SUCNTL2  = 27;

	static constexpr uint16_t PLC_CNTL_RESET           = 1 << 15;
	static constexpr uint16_t PLC_CNTL_LOOPBACK        = 1 << 14;
	static constexpr uint16_t PLC_CNTL_100MBPS_EN      = 1 << 13;
	static constexpr uint16_t PLC_CNTL_AUTONEG_EN      = 1 << 12;
	static constexpr uint16_t PLC_CNTL_POWER_DOWN      = 1 << 11;
	static constexpr uint16_t PLC_CNTL_ISOLATE         = 1 << 10;
	static constexpr uint16_t PLC_CNTL_RESTART_AUTONEG = 1 <<  9;
	static constexpr uint16_t PLC_CNTL_FULL_DUPLEX_EN  = 1 <<  8;
	static constexpr uint16_t PLC_CNTL_COLLISION_TEST  = 1 <<  7;
	static constexpr uint16_t PLC_CNTL_WMASK           = 0xfd80;

	static constexpr uint16_t PLC_STATUS_100BTXFD_EN  = 1 << 14;
	static constexpr uint16_t PLC_STATUS_100BTXHD_EN  = 1 << 13;
	static constexpr uint16_t PLC_STATUS_10BTXFD_EN   = 1 << 12;
	static constexpr uint16_t PLC_STATUS_10BTXHD_EN   = 1 << 11;
	static constexpr uint16_t PLC_STATUS_MFPRE_SUP    = 1 <<  6;
	static constexpr uint16_t PLC_STATUS_AUTONEG_DONE = 1 <<  5;
	static constexpr uint16_t PLC_STATUS_REMOTE_ERR   = 1 <<  4;
	static constexpr uint16_t PLC_STATUS_AUTONEG_EN   = 1 <<  3;
	static constexpr uint16_t PLC_STATUS_LINK_EN      = 1 <<  2;
	static constexpr uint16_t PLC_STATUS_JABBER       = 1 <<  1;
	static constexpr uint16_t PLC_STATUS_EXCAP_EN     = 1 <<  0;

	static constexpr uint16_t PLC_ID1 = 0x02A8; 
	static constexpr uint16_t PLC_ID2 = 0x0330;

	static constexpr uint16_t PLC_AUTONEG_AD_NEXTP         = 1 <<  15;
	static constexpr uint16_t PLC_AUTONEG_AD_REMOTE_ERR    = 1 <<  13;
	static constexpr uint16_t PLC_AUTONEG_AD_TECHCAP_SHIFT = 5;
	static constexpr uint16_t PLC_AUTONEG_AD_TECHCAP_MASK  = 0xff << i82801_eth_device::PLC_AUTONEG_AD_TECHCAP_SHIFT;
	static constexpr uint16_t PLC_AUTONEG_AD_SEL_SHIFT     = 0;
	static constexpr uint16_t PLC_AUTONEG_AD_SEL_MASK      = 0x0f << i82801_eth_device::PLC_AUTONEG_AD_SEL_SHIFT;
	static constexpr uint16_t PLC_AUTONEG_AD_WMASK         = 0x3fe0;

	static constexpr uint16_t PLC_AUTONEG_LP_NEXTP         = 1 <<  15;
	static constexpr uint16_t PLC_AUTONEG_LP_ACK           = 1 <<  14;
	static constexpr uint16_t PLC_AUTONEG_LP_REMOTE_ERR    = 1 <<  13;
	static constexpr uint16_t PLC_AUTONEG_LP_TECHCAP_SHIFT = 5;
	static constexpr uint16_t PLC_AUTONEG_LP_TECHCAP_MASK  = 0xff << i82801_eth_device::PLC_AUTONEG_AD_TECHCAP_SHIFT;
	static constexpr uint16_t PLC_AUTONEG_LP_SEL_SHIFT     = 0;
	static constexpr uint16_t PLC_AUTONEG_LP_SEL_MASK      = 0x0f << i82801_eth_device::PLC_AUTONEG_AD_SEL_SHIFT;

	static constexpr uint16_t PLC_AUTONEG_EXP_PARDET_ERR     = 1 << 4;
	static constexpr uint16_t PLC_AUTONEG_EXP_LP_NEXTP_CAP   = 1 << 3;
	static constexpr uint16_t PLC_AUTONEG_EXP_NEXTP_CAP      = 1 << 2;
	static constexpr uint16_t PLC_AUTONEG_EXP_PAGE_RECV      = 1 << 1;
	static constexpr uint16_t PLC_AUTONEG_EXP_LP_AUTONEG_CAP = 1 << 0;

	static constexpr uint16_t PLC_PHY_STATCNTL_NO_AUTOPD     = 1 << 13;
	static constexpr uint16_t PLC_PHY_STATCNTL_RECV_DSSYNC   = 1 << 11;
	static constexpr uint16_t PLC_PHY_STATCNTL_100PD         = 1 << 10;
	static constexpr uint16_t PLC_PHY_STATCNTL_10PD          = 1 <<  9;
	static constexpr uint16_t PLC_PHY_STATCNTL_POL           = 1 <<  8;
	static constexpr uint16_t PLC_PHY_STATCNTL_PHYADDR_SHIFT = 2;
	static constexpr uint16_t PLC_PHY_STATCNTL_PHYADDR_MASK  = 0x1f << i82801_eth_device::PLC_PHY_STATCNTL_PHYADDR_SHIFT;
	static constexpr uint16_t PLC_PHY_STATCNTL_100MBPS_EN    = 1 <<  1;
	static constexpr uint16_t PLC_PHY_STATCNTL_FULLD_EN      = 1 <<  0;
	static constexpr uint16_t PLC_PHY_STATCNTL_WMASK         = 0xf000;

	static constexpr uint16_t PLC_PHY_SUCNTL1_SCMBL_BYPASS     = 1 << 15;
	static constexpr uint16_t PLC_PHY_SUCNTL1_45BIT_BYPASS     = 1 << 14;
	static constexpr uint16_t PLC_PHY_SUCNTL1_FORCE_HPATTX     = 1 << 13;
	static constexpr uint16_t PLC_PHY_SUCNTL1_FORCE_34PATTX    = 1 << 12;
	static constexpr uint16_t PLC_PHY_SUCNTL1_100BTX_VALIDL    = 1 << 11;
	static constexpr uint16_t PLC_PHY_SUCNTL1_SYM_ERR_EN       = 1 << 10;
	static constexpr uint16_t PLC_PHY_SUCNTL1_CS_DISABLE       = 1 <<  9;
	static constexpr uint16_t PLC_PHY_SUCNTL1_DYNPD_DISABLE    = 1 <<  8;
	static constexpr uint16_t PLC_PHY_SUCNTL1_AUTONEG_LOOPBACK = 1 <<  7;
	static constexpr uint16_t PLC_PHY_SUCNTL1_MDI_TRISTATE     = 1 <<  6;
	static constexpr uint16_t PLC_PHY_SUCNTL1_FORCE_REV_POL    = 1 <<  5;
	static constexpr uint16_t PLC_PHY_SUCNTL1_AUTOPOL_DISABLE  = 1 <<  4;
	static constexpr uint16_t PLC_PHY_SUCNTL1_SQUELCH_DISABLE  = 1 <<  3;
	static constexpr uint16_t PLC_PHY_SUCNTL1_EXSQUELCH_EN     = 1 <<  2;
	static constexpr uint16_t PLC_PHY_SUCNTL1_LINTEG_DISABLE   = 1 <<  1;
	static constexpr uint16_t PLC_PHY_SUCNTL1_JABBER_DISABLE   = 1 <<  1;
	static constexpr uint16_t PLC_PHY_SUCNTL1_WMASK            = 0xffff;

	static constexpr uint16_t PLC_PHY_SUCNTL2_SWITCH_LED_MAP = 1 << 5;
	static constexpr uint16_t PLC_PHY_SUCNTL2_RJAB_DISABLE   = 1 << 3;
	static constexpr uint16_t PLC_PHY_SUCNTL2_LED_CNTL_SHIFT = 0;
	static constexpr uint16_t PLC_PHY_SUCNTL2_LED_CNTL_MASK  = 0x07 << i82801_eth_device::PLC_PHY_SUCNTL2_LED_CNTL_SHIFT;
	static constexpr uint16_t PLC_PHY_SUCNTL2_WMASK          = 0xffff;

	static constexpr uint8_t PHY_COUNT = 1; // The number of addressable PHYs via LAN Connect. Essentially the number of ethernet connectors

	static constexpr uint8_t PLC_REGISTER_COUNT = 32;
	typedef struct {
		uint16_t reg[i82801_eth_device::PLC_REGISTER_COUNT];
	} plc_state_t;
	plc_state_t m_phy[i82801_eth_device::PHY_COUNT];

	bool m_default_link_state;

	static constexpr uint32_t MAX_FRAME_SIZE = 0x10000;
	static constexpr uint32_t MIN_FRAME_SIZE = 6 + 6 + 2 + 4;

	static constexpr uint32_t NULL_POINTER = 0xffffffff;

	static constexpr uint8_t CU_CONFIG_MIN_SIZE = 0x04;
	static constexpr uint8_t CU_CONFIG_MAX_SIZE = 0x16;
	typedef struct eth_configuration
	{
		uint8_t data[i82801_eth_device::CU_CONFIG_MAX_SIZE];

		uint8_t size() const { return std::clamp((uint8_t)(data[0] & 0x3f), i82801_eth_device::CU_CONFIG_MIN_SIZE, i82801_eth_device::CU_CONFIG_MAX_SIZE); }
		bool    nasi() const { return (data[10] & 0x08); }

	} eth_configuration_t;
	eth_configuration_t m_configuration;

	static constexpr uint16_t STAT_COUNTERS_DUMP_EOD = 0xa005;
	static constexpr uint16_t STAT_COUNTERS_DRST_EOD = 0xa007;
	typedef struct eth_stat_counters
	{
		// Transmit maximum collisions (MAXCOL) errors
		uint32_t tx_cnt;
		// Transmit late collisions (LATECOL) errors
		uint32_t tx_maxcol_errcnt;
		// Transmit underrun errors
		uint32_t tx_latecol_errcnt;
		// Transmit lost carrier sense (CRS)
		uint32_t tx_crs_cnt;
		// Transmit deferred
		uint32_t tx_deffered_cnt;
		// Transmit single collision
		uint32_t tx_scol_errcnt;
		// Transmit multiple collisions
		uint32_t tx_mcol_errcnt;
		// Transmit total collisions
		uint32_t tx_col_errcnt;
		// Receive good frames
		uint32_t rx_cnt;
		// Receive CRC errors
		uint32_t rx_crc_errcnt;
		// Receive alignment errors
		uint32_t rx_align_errcnt;
		// Receive resource errors
		uint32_t rx_resource_errcnt;
		// Receive overrun errors
		uint32_t rx_overrun_errcnt;
		// Receive collision detect (CDT) errors
		uint32_t rx_cdt_errcnt;
		// Receive short frame errors
		uint32_t rx_shrt_frame_errcnt;
		// Flow control transmit pause
		uint32_t fc_tx_pause_cnt;
		// Flow control receive pause
		uint32_t fc_rx_pause_cnt;
		// Flow control receive unsupported
		uint32_t fc_rx_unsupported_cnt;
		// Transmit TCO frames
		uint16_t tx_tco_frame_cnt;
		// Receive TCO frames
		uint16_t rx_tco_frame_cnt;

		void reset()
		{
			*this = {};
		}

		void dma_copy(address_space* dma_space, uint32_t addr, bool reset = false)
		{
			dma_space->write_dword(addr, tx_cnt);
			addr += 4;
			dma_space->write_dword(addr, tx_maxcol_errcnt);
			addr += 4;
			dma_space->write_dword(addr, tx_latecol_errcnt);
			addr += 4;
			dma_space->write_dword(addr, tx_crs_cnt);
			addr += 4;
			dma_space->write_dword(addr, tx_deffered_cnt);
			addr += 4;
			dma_space->write_dword(addr, tx_scol_errcnt);
			addr += 4;
			dma_space->write_dword(addr, tx_mcol_errcnt);
			addr += 4;
			dma_space->write_dword(addr, tx_col_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_cnt);
			addr += 4;
			dma_space->write_dword(addr, rx_crc_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_align_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_resource_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_overrun_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_cdt_errcnt);
			addr += 4;
			dma_space->write_dword(addr, rx_shrt_frame_errcnt);
			addr += 4;
			dma_space->write_dword(addr, fc_tx_pause_cnt);
			addr += 4;
			dma_space->write_dword(addr, fc_rx_pause_cnt);
			addr += 4;
			dma_space->write_dword(addr, fc_rx_unsupported_cnt);
			addr += 4;
			dma_space->write_word(addr, tx_tco_frame_cnt);
			addr += 2;
			dma_space->write_word(addr, rx_tco_frame_cnt);
			addr += 2;
			dma_space->write_dword(addr, 0);
			addr += 4;

			if(reset)
			{
				dma_space->write_word(addr, i82801_eth_device::STAT_COUNTERS_DRST_EOD);
				this->reset();
			}
			else
			{
				dma_space->write_word(addr, i82801_eth_device::STAT_COUNTERS_DRST_EOD);
			}
		}
	} eth_stat_counters_t;
	eth_stat_counters_t m_counters;

	uint32_t m_csr_scb_genptr;
	uint32_t m_csr_scp_dmpptr;
	uint32_t m_csr_port;
	uint32_t m_csr_dma_bytecnt;
	uint8_t m_csr_early_recvint;
	uint16_t m_csr_flow_cntl;
	uint8_t m_csr_pmdr;
	uint8_t m_csr_cntl;
	uint8_t m_csr_sts;
	uint16_t m_pm_csr;

	static constexpr uint16_t SCB_STATUS_CU_DONE_INT     = 1 << 15; // CX
	static constexpr uint16_t SCB_STATUS_RU_DONE_INT     = 1 << 14; // FR
	static constexpr uint16_t SCB_STATUS_CU_NA_INT       = 1 << 13; // CNA
	static constexpr uint16_t SCB_STATUS_RU_NR_INT       = 1 << 12; // RNR
	static constexpr uint16_t SCB_STATUS_MDI_INT         = 1 << 11; // MDI
	static constexpr uint16_t SCB_STATUS_SW_INT          = 1 << 10; // SWI/SI
	static constexpr uint16_t SCB_STATUS_EARLY_RECV_INT  = 1 <<  9; // ER
	static constexpr uint16_t SCB_STATUS_FCNTL_PAUSE_INT = 1 <<  8; // FCP
	static constexpr uint16_t SCB_STATUS_CU_SHIFT        = 6;
	static constexpr uint16_t SCB_STATUS_CU_MASK         = 0x03 << i82801_eth_device::SCB_STATUS_CU_SHIFT;
	static constexpr uint16_t SCB_STATUS_RU_SHIFT        = 2;
	static constexpr uint16_t SCB_STATUS_RU_MASK         = 0x0f << i82801_eth_device::SCB_STATUS_RU_SHIFT;

	uint16_t m_csr_scb_sts;

	static constexpr uint16_t SCB_CNTL_CU_DONE_INT_MASK     = 1 << 15; // CX
	static constexpr uint16_t SCB_CNTL_RU_DONE_INT_MASK     = 1 << 14; // FR
	static constexpr uint16_t SCB_CNTL_CU_NA_INT_MASK       = 1 << 13; // CNA
	static constexpr uint16_t SCB_CNTL_RU_NR_INT_MASK       = 1 << 12; // RNR
	static constexpr uint16_t SCB_CNTL_EARLY_RECV_INT_MASK  = 1 << 11; // ER
	static constexpr uint16_t SCB_CNTL_FCNTL_PAUSE_INT_MASK = 1 << 10; // FCP
	static constexpr uint16_t SCB_CNTL_SW_INT_ASSERT        = 1 <<  9; // SWI/SI
	static constexpr uint16_t SCB_CNTL_INTA_DISABLE         = 1 <<  8;
	static constexpr uint16_t SCB_CNTL_CU_CMD_SHIFT         = 4;
	static constexpr uint16_t SCB_CNTL_CU_CMD_MASK          = 0x0f << i82801_eth_device::SCB_CNTL_CU_CMD_SHIFT;
	static constexpr uint16_t SCB_CNTL_RU_CMD_SHIFT         = 0;
	static constexpr uint16_t SCB_CNTL_RU_CMD_MASK          = 0x07 << i82801_eth_device::SCB_CNTL_RU_CMD_SHIFT;

	uint16_t m_csr_scb_cmd;

	static constexpr uint32_t PORT_POINTER_ADDR_MASK = 0xfffffff0;
	static constexpr uint32_t PORT_FUNC_MASK         = 0x0000000f;
	static constexpr uint8_t PORT_FUNC_SW_RESET      = 0x00;
	static constexpr uint8_t PORT_FUNC_SELF_TEST     = 0x01;
	static constexpr uint8_t PORT_FUNC_SEL_RESET     = 0x02;

	static constexpr uint32_t PORT_SELF_TEST_GNRL_FAILED = 1 << 12;
	static constexpr uint32_t PORT_SELF_TEST_DIAG_FAILED = 1 <<  5;
	static constexpr uint32_t PORT_SELF_TEST_REGR_FAILED = 1 <<  3;
	static constexpr uint32_t PORT_SELF_TEST_ROMC_FAILED = 1 <<  2;
	static constexpr uint32_t PORT_SELF_TEST_GOOD        = 0x00000000;

	static constexpr uint32_t PMDR_LINK_STATUS_CHANGE       = 1 <<  7;
	static constexpr uint32_t PMDR_FOUND_MAGIC_PACKET       = 1 <<  6;
	static constexpr uint32_t PMDR_FOUND_INTERESTING_PACKET = 1 <<  5;
	static constexpr uint32_t PMDR_PME_STATUS_BIT           = 1 <<  0;

	static constexpr uint16_t STATUS_WMASK = 0xbfff;

	const attotime CU_ACTION_RATE = attotime::from_usec(1000);
	emu_timer* m_cu_action_timer;
	TIMER_CALLBACK_MEMBER(cu_cbl_execute);

	static constexpr uint16_t CU_CBL_COMMAND_LAST      = 1 << 15;
	static constexpr uint16_t CU_CBL_COMMAND_SUSPEND   = 1 << 14;
	static constexpr uint16_t CU_CBL_COMMAND_INT       = 1 << 13;
	static constexpr uint16_t CU_CBL_COMMAND_CID_SHIFT = 8;
	static constexpr uint16_t CU_CBL_COMMAND_CID_MASK  = 0x1f << i82801_eth_device::CU_CBL_COMMAND_CID_SHIFT;
	static constexpr uint16_t CU_CBL_COMMAND_RAW       = 1 << 5;
	static constexpr uint16_t CU_CBL_COMMAND_FLEX_MODE = 1 << 4;
	static constexpr uint16_t CU_CBL_COMMAND_OP_SHIFT  = 0;
	static constexpr uint16_t CU_CBL_COMMAND_OP_MASK   = 0x07 << i82801_eth_device::CU_CBL_COMMAND_OP_SHIFT;

	static constexpr uint16_t CU_CBL_STATUS_COMPLETE = 1 << 15;
	static constexpr uint16_t CU_CBL_STATUS_OK       = 1 << 13;
	static constexpr uint16_t CU_CBL_STATUS_UNDERRUN = 1 << 12;

	static constexpr uint32_t CU_TBD_COUNT_SHIFT      = 24;
	static constexpr uint32_t CU_TBD_COUNT_MASK       = 0xff << i82801_eth_device::CU_TBD_COUNT_SHIFT;
	static constexpr uint32_t CU_TCB_THRESH_SHIFT     = 16;
	static constexpr uint32_t CU_TCB_THRESH_MASK      = 0xff << i82801_eth_device::CU_TCB_THRESH_SHIFT;
	static constexpr uint32_t CU_TCB_EOF              = 1 << 15;
	static constexpr uint32_t CU_TCB_BYTE_COUNT_SHIFT = 0;
	static constexpr uint32_t CU_TCB_BYTE_COUNT_MASK  = 0x3fff << i82801_eth_device::CU_TCB_BYTE_COUNT_SHIFT;

	static constexpr uint32_t CU_TBD_EOL              = 1 << 16;
	static constexpr uint32_t CU_TBD_BYTE_COUNT_SHIFT = 0;
	static constexpr uint32_t CU_TBD_BYTE_COUNT_MASK  = 0x3fff << i82801_eth_device::CU_TCB_BYTE_COUNT_SHIFT;

	enum scb_cu_cmd_t : uint8_t
	{
		SCP_CUC_NOP                          = 0x00,
		SCP_CUC_START                        = 0x01,
		SCP_CUC_RESUME                       = 0x02,
		SCP_CUC_HPQ_START                    = 0x03,
		SCP_CUC_LOAD_DUMP_COUNTER_ADDR       = 0x04,
		SCP_CUC_DUMP_STAT_COUNTERS           = 0x05,
		SCP_CUC_LOAD_BASE                    = 0x06,
		SCP_CUC_DUMP_AND_RESET_STAT_COUNTERS = 0x07,
		SCP_CUC_STATIC_RESUME                = 0x0a,
		SCP_CUC_HPQ_RESUME                   = 0x0b
	};
	enum cu_cmd_t : uint16_t
	{
		CUC_NOP                          = 0x0000,
		CUC_INDIVIDUAL_ADDRESS_SETUP     = 0x0001,
		CUC_CONFIGURE                    = 0x0002,
		CUC_MULTICAST_SETUP              = 0x0003,
		CUC_TRANSMIT                     = 0x0004,
		CUC_LOAD_MICROCODE               = 0x0005,
		CUC_DUMP                         = 0x0006,
		CUC_DIAGNOSE                     = 0x0007
	};

	enum cu_state_t : uint8_t
	{
		CU_IDLE       = 0x00,
		CU_SUSPENDED  = 0x01,
		CU_LPQ_ACTIVE = 0x02,
		CU_HPQ_ACTIVE = 0x03
	};

	cu_state_t m_cu_state;
	uint32_t m_cbl_base_addr;     // This plus any blk offset forms the m_cbl_hpq_nblk_addr or m_cbl_lpq_nblk_addr value.
	uint32_t m_cbl_hpq_nblk_addr; // Next high-priority block waiting to be executed.
	uint32_t m_cbl_lpq_nblk_addr; // Next low-priority block waiting to be executed.
	uint32_t m_cbl_cblk_addr;     // The current block being executed.
	uint32_t m_cbl_cexc_addr;     // The current address/cursor being read during execution.
	uint8_t m_cu_frame[i82801_eth_device::MAX_FRAME_SIZE];
	uint16_t m_cu_frame_len;

	const attotime RU_ACTION_RATE = attotime::zero;
	emu_timer* m_ru_action_timer;
	TIMER_CALLBACK_MEMBER(ru_rfd_execute);

	static constexpr uint16_t RU_RFD_COMMAND_LAST      = 1 << 15;
	static constexpr uint16_t RU_RFD_COMMAND_SUSPEND   = 1 << 14;
	static constexpr uint16_t RU_RFD_COMMAND_HEADER    = 1 << 5;

	static constexpr uint16_t RU_RFD_STATUS_COMPLETE     = 1 << 15;
	static constexpr uint16_t RU_RFD_STATUS_OK           = 1 << 13;
	static constexpr uint16_t RU_RFD_STATUS_RESULT_SHIFT = 0;
	static constexpr uint16_t RU_RFD_STATUS_RESULT_MASK  = 0x1fff << i82801_eth_device::RU_RFD_STATUS_RESULT_SHIFT;

	static constexpr uint16_t RU_RFD_RESULT_CRC_ERR_ALIGNED  = 1 << 11;
	static constexpr uint16_t RU_RFD_RESULT_CRC_ERR_MALIGNED = 1 << 10;
	static constexpr uint16_t RU_RFD_RESULT_OVERFLOW         = 1 <<  9;
	static constexpr uint16_t RU_RFD_RESULT_OVERRUN          = 1 <<  8;
	static constexpr uint16_t RU_RFD_RESULT_TOO_SHORT        = 1 <<  7;
	static constexpr uint16_t RU_RFD_RESULT_TYPE_FRAME       = 1 <<  5;
	static constexpr uint16_t RU_RFD_RESULT_RX_ERR           = 1 <<  4;
	static constexpr uint16_t RU_RFD_RESULT_ADDR_MISMATCH    = 1 <<  2;
	static constexpr uint16_t RU_RFD_RESULT_MAC_MISMATCH     = 1 <<  1;
	static constexpr uint16_t RU_RFD_RESULT_COLLISION        = 1 <<  2;

	static constexpr uint32_t RU_RFD_BPROPS_AVAIL_SIZE_SHIFT = 16;
	static constexpr uint32_t RU_RFD_BPROPS_AVAIL_SIZE_MASK  = 0x3fff << i82801_eth_device::RU_RFD_BPROPS_AVAIL_SIZE_SHIFT;
	static constexpr uint32_t RU_RFD_BPROPS_EOF              = 1 << 15;
	static constexpr uint32_t RU_RFD_BPROPS_SIZE_WRITTEN     = 1 << 14;
	static constexpr uint32_t RU_RFD_BPROPS_USED_SIZE_SHIFT  = 0;
	static constexpr uint32_t RU_RFD_BPROPS_USED_SIZE_MASK   = 0x3fff << i82801_eth_device::RU_RFD_BPROPS_USED_SIZE_SHIFT;
	static constexpr uint32_t RU_RFD_BPROPS_WMASK            = 0x0000ffff;

	enum scb_ru_cmd_t : uint8_t
	{
		SCP_RUC_NOP                   = 0x00,
		SCP_RUC_START                 = 0x01,
		SCP_RUC_RESUME                = 0x02,
		SCP_RUC_RCV_DMA_REDIRECT      = 0x03,
		SCP_RUC_ABORT                 = 0x04,
		SCP_RUC_LOAD_HEADER_DATA_SIZE = 0x05,
		SCP_RUC_LOAD_BASE             = 0x06,
		SCP_RUC_RBD_RESUME            = 0x07
	};

	enum ru_state_t : uint8_t
	{
		RU_IDLE             = 0x00,
		RU_SUSPENDED        = 0x01,
		RU_SUSPENDED_NORDBS = 0x09,
		RU_OUTOFMEM         = 0x02,
		RU_OUTOFMEM_NORDBS  = 0x0a,
		RU_READY            = 0x04,
		RU_READY_NORDBS     = 0x0c,
		RU_ACTIVE           = 0x14
	};

	ru_state_t m_ru_state;
	uint32_t m_rfd_base_addr; // This plus any rfd offset forms the m_rfd_nfrm_addr value.
	uint32_t m_rfd_nfrm_addr; // The next frame to be processed.
	uint32_t m_rfd_cfrm_addr; // The current frame being processed.
	uint32_t m_rfd_cexc_addr; // The current address/cursor being read during processing.
	uint8_t m_ru_frame[i82801_eth_device::MAX_FRAME_SIZE];
	uint32_t m_ru_frame_len;
	uint32_t m_ru_frame_idx;

	static constexpr uint32_t MDI_INT_EN            = 1 << 29;
	static constexpr uint32_t MDI_READY             = 1 << 28;
	static constexpr uint32_t MDI_OPCODE_SHIFT      = 26;
	static constexpr uint32_t MDI_OPCODE_MASK       = 0x03 << i82801_eth_device::MDI_OPCODE_SHIFT;
	static constexpr uint32_t MDI_PLC_ADDR_SHIFT    = 21;
	static constexpr uint32_t MDI_PLC_ADDR_MASK     = 0x1f << i82801_eth_device::MDI_PLC_ADDR_SHIFT;
	static constexpr uint32_t MDI_PLCREG_ADDR_SHIFT = 16;
	static constexpr uint32_t MDI_PLCREG_ADDR_MASK  = 0x1f << i82801_eth_device::MDI_PLCREG_ADDR_SHIFT;
	static constexpr uint32_t MDI_DATA_SHIFT        = 0;
	static constexpr uint32_t MDI_DATA_MASK         = 0xffff << i82801_eth_device::MDI_DATA_SHIFT;
	enum mdi_cmd_t : uint8_t
	{
		MDI_WRITE = 0x01,
		MDI_READ  = 0x02
	};

	uint32_t m_csr_mdi_cntl;

	static constexpr uint8_t GENERAL_CNTL_PLC_RESET  = 1 << 3;
	static constexpr uint8_t GENERAL_CNTL_POWER_DOWN = 1 << 1;

	static constexpr uint8_t GENERAL_STATUS_FULL_DUPLEX_EN = 1 << 2;
	static constexpr uint8_t GENERAL_STATUS_100MBPS_EN     = 1 << 1;
	static constexpr uint8_t GENERAL_STATUS_LINK_EN        = 1 << 0;

	void set_plc_defaults();
	void set_default_phy_state(offs_t offset);
	uint16_t phy_state_read(offs_t phy_offset, offs_t reg_offset);
	void phy_state_write(offs_t phy_offset, offs_t reg_offset, uint16_t data);

	void full_controller_reset();
	void partial_controller_reset();

	uint32_t csr_membase_r();
	void csr_membase_w(uint32_t data);
	uint32_t csr_iobase_r();
	void csr_iobase_w(uint32_t data);
	uint8_t capid_r();
	uint8_t nextptr_r();
	uint16_t pm_cap_r();
	uint16_t pm_csr_r();
	void pm_csr_w(uint16_t data);
	uint8_t pm_data_r();

	void csr_map(address_map &map);

	uint16_t csr_scb_sts_r();
	void csr_scb_sts_w(uint16_t data);
	uint16_t csr_scb_cmd_r();
	void csr_scb_cmd_w(uint16_t data);
	uint32_t csr_scb_genptr_r();
	void csr_scb_genptr_w(uint32_t data);
	uint32_t csr_port_r();
	void csr_port_w(uint32_t data);
	uint8_t csr_eeprom_cntl_r();
	void csr_eeprom_cntl_w(uint8_t data);
	uint32_t csr_mdi_cntl_r();
	void csr_mdi_cntl_w(uint32_t data);
	uint32_t csr_dma_bytecnt_r();
	void csr_dma_bytecnt_w(uint32_t data);
	uint8_t csr_early_recvint_r();
	void csr_early_recvint_w(uint8_t data);
	uint8_t csr_flow_cntl_high_r();
	void csr_flow_cntl_high_w(uint8_t data);
	uint8_t csr_flow_cntl_low_r();
	void csr_flow_cntl_low_w(uint8_t data);
	uint8_t csr_pmdr_r();
	void csr_pmdr_w(uint8_t data);
	uint8_t csr_cntl_r();
	void csr_cntl_w(uint8_t data);
	uint8_t csr_sts_r();
	void csr_sts_w(uint8_t data);

	uint16_t r16_advance(uint32_t* exc_addr);
	void w16_advance(uint32_t* exc_addr, uint16_t data);
	uint32_t r32_advance(uint32_t* exc_addr);
	void w32_advance(uint32_t* exc_addr, uint32_t data);
	void set_status(uint32_t* saddr, uint16_t status);
	void clear_status(uint32_t* saddr, uint16_t status);
	uint16_t copy_from_memory(uint8_t* buffer, uint32_t mem_addr, uint16_t length);
	uint16_t copy_to_memory(uint8_t* buffer, uint32_t mem_addr, uint16_t length);

	cu_state_t get_cu_state();
	void set_cu_state(cu_state_t state);
	void cu_set_hpq_next_addr(uint32_t offset_addr);
	void cu_set_lpq_next_addr(uint32_t offset_addr);
	void cu_execute_next(cu_state_t state = cu_state_t::CU_HPQ_ACTIVE);
	void cu_execute_pause();
	void cu_execute_stop();
	void cu_scb_execute();
	void cu_iasetup(uint32_t commnd_word);
	void cu_mcsetup(uint32_t commnd_word);
	void cu_configure(uint32_t commnd_word);
	void cu_transmit(uint32_t commnd_word);
	void cu_load_microcode(uint32_t commnd_word);
	void cu_dump(uint32_t commnd_word);
	void cu_diagnose(uint32_t commnd_word);
	void cu_nop(uint32_t commnd_word);

	ru_state_t get_ru_state();
	void set_ru_state(ru_state_t state);
	void set_rfd_buffer_props(uint32_t* saddr, uint16_t actual_size, bool eof);
	void ru_set_next_addr(uint32_t offset_addr);
	void ru_execute_wait();
	void ru_execute_next();
	void ru_execute_pause();
	void ru_execute_stop();
	void ru_complete();
	void ru_scb_execute();

	void set_irq(uint32_t mask, int state);

};

DECLARE_DEVICE_TYPE(I82801_ETH, i82801_eth_device)
DECLARE_DEVICE_TYPE(I82801_ETH_EEPROM, i82801_eth_eeprom_device)

#endif // MAME_WEBTV_I82801_ETH_H
