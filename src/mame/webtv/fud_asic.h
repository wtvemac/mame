// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_FUD_ASIC_H
#define MAME_WEBTV_FUD_ASIC_H

#pragma once

#include "cpu/mips/mips3.h"

constexpr uint32_t GPIO_TUNER0_LOCK = 0x00000002;
constexpr uint32_t GPIO_TUNER1_LOCK = 0x00000004;

constexpr uint32_t TUNER_LOCK_HZ       = 2;

class fud_asic_device : public device_t
{

public:

	// Transport PCI BAR
	static constexpr uint32_t TP_BASE        = 0x04870000;
	static constexpr uint32_t TP_SIZE        = 0x10000;

	fud_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t gpio_base_address, uint32_t clock = 0);

	void cntl_map(address_map &map);
	void gpio_map(address_map &map);
	void transport_map(address_map &map);
	auto ca_card_present_callback() { return m_ca_card_present.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	static constexpr uint32_t GPIO_INT_TP0     = 1 << 8;
	static constexpr uint32_t GPIO_INT_TP1     = 1 << 10;

	static constexpr uint32_t TP_PROC_BASE     = 0x1000;
	static constexpr uint32_t TP_PROC_STRIDE   = 0x2000;
	static constexpr uint32_t TP_PROC_COUNT    = 2;
	static constexpr uint32_t TP_CTLSTAT       = 0x1120; // bits 0-4 DMA channel summary, bit 31 run
	static constexpr uint32_t TP_FILTERCTL     = 0x1128; // bits 0-11 packet control slot enables
	static constexpr uint32_t TP_PKTCTL        = 0x1200; // bits 0-11 SCID, 19-21 DMA channel
	static constexpr uint32_t TP_SLOT_COUNT    = 12;
	static constexpr uint32_t TP_DMA           = 0x2400;
	static constexpr uint32_t TP_DMA_STRIDE    = 0x100;
	static constexpr uint32_t TP_DMA_COUNT     = 5;
	static constexpr uint32_t TP_DMA_NEXTADDR  = 0x00;
	static constexpr uint32_t TP_DMA_NEXTSIZE  = 0x04; // in 32-byte units
	static constexpr uint32_t TP_DMA_CONTROL   = 0x0c;
	static constexpr uint32_t TP_DMA_CURRADDR  = 0x10;
	static constexpr uint32_t TP_DMA_INDEX     = 0x1c;

	static constexpr uint32_t TP_CTLSTAT_W1C   = 0x0005ffff;

	static constexpr uint32_t TP_DMA_NEXTVALID = 1 << 1;
	static constexpr uint32_t TP_DMA_RUN       = 1 << 2;
	static constexpr uint32_t TP_DMA_DONE      = 1 << 5;
	static constexpr uint32_t TP_DMA_STOPPED   = 1 << 7;
	static constexpr uint32_t TP_DMA_PROGIEN   = 1 << 8;
	static constexpr uint32_t TP_DMA_PROGRESS  = 1 << 9;
	static constexpr uint32_t TP_DMA_W1C       = TP_DMA_DONE | TP_DMA_STOPPED | TP_DMA_PROGRESS;

	static constexpr uint32_t TP_RECORD_SIZE   = 160;
	static constexpr uint32_t TP_RECORD_PREFIX = 2;
	static constexpr uint32_t DSS_PACKET_SIZE  = 130;

	static constexpr uint16_t APG_BOOT_SCID          = 0x810;
	static constexpr uint32_t GUIDE_HZ               = 8;
	static constexpr uint32_t GUIDE_PACKETS_PER_TICK = 4;
	static constexpr uint16_t APG_NETWORK_CURRENT    = 0xffff;
	static constexpr uint16_t APG_NETWORK_OTHER      = 0xa003;
	static constexpr uint16_t APG_NETWORK_SEARCHED   = 0x0000;
	static constexpr uint8_t  GUIDE_OBJECT_VERSION   = 1;
	static constexpr uint32_t GUIDE_KEY_ULO_CURRENT  = 0x001;
	static constexpr uint32_t GUIDE_KEY_ULO_OTHER    = 0x002;
	static constexpr uint32_t GUIDE_KEY_ULO_SEARCHED = 0x003;
	static constexpr uint32_t GUIDE_KEY_CHANNEL      = 0x100;
	static constexpr uint32_t GUIDE_KEY_SCHEDULE     = 0x200;
	static constexpr uint32_t GUIDE_KEY_TIME         = 0x300;
	static constexpr uint32_t GUIDE_KEY_SUBSCRIPTION = 0x400;
	static constexpr uint32_t GUIDE_TIME_INTERVAL    = 30;
	static constexpr int64_t  GPS_EPOCH_UNIX         = 315964800; // 1980-01-06 00:00:00 UTC
	static constexpr uint8_t  GPS_LEAP_SECONDS       = 18;
	static constexpr uint16_t GUIDE_SOURCE_ID        = 1;
	static constexpr uint16_t GUIDE_CHANNEL_NUMBER   = 100;
	static constexpr uint32_t GUIDE_SCHEDULE_START   = 1;
	static constexpr uint32_t GUIDE_EVENT_COUNT      = 241;
	static constexpr uint32_t GUIDE_EVENT_SECONDS    = 0xffffff;

	using guide_cycle = std::vector<std::array<uint8_t, DSS_PACKET_SIZE>>;

	struct tp_dma_state
	{
		bool active;
		uint32_t start;
		uint32_t end;
		uint32_t ptr;
		uint32_t since_progress;
	};

	required_device<mips3_device> m_hostcpu;
	devcb_read_line m_ca_card_present;

	std::unique_ptr<uint32_t[]> m_tp_regs;
	tp_dma_state m_tp_dma[TP_PROC_COUNT * TP_DMA_COUNT];
	uint8_t m_guide_cc[TP_PROC_COUNT];
	uint32_t m_guide_pos[TP_PROC_COUNT];
	guide_cycle m_guide_cycle[TP_PROC_COUNT];
	attotime m_guide_time_next;

	emu_timer *tuner_lock_timer = nullptr;
	TIMER_CALLBACK_MEMBER(set_tuner_lock);

	emu_timer *m_guide_timer = nullptr;
	TIMER_CALLBACK_MEMBER(guide_tick);

	uint32_t m_fud_cntl;

	uint32_t cntl_0008_r();          //
	void cntl_0008_w(uint32_t data); //
	uint32_t cntl_000c_r();          //
	void cntl_000c_w(uint32_t data); //

	uint32_t m_gpio_base;
	uint32_t m_gpio_intenable;
	uint32_t m_gpio_intstat;
	uint32_t m_gpio_level;

	void set_gpio_irq(uint32_t mask, int state);
	void update_gpio_irq();

	uint32_t gpio_002c_r();          // GPIO_INTEN (read)
	void gpio_002c_w(uint32_t data); // GPIO_INTEN_S (write)
	uint32_t gpio_0030_r();          // GPIO_INTSTAT (read)
	void gpio_0030_w(uint32_t data); // GPIO_INTSTAT_C (write)
	uint32_t gpio_0034_r();          // GPIO pending & enabled (read)
	void gpio_0034_w(uint32_t data); // GPIO_INTEN_S (write)
	uint32_t gpio_003c_r();          // GPIO_INTEN (read)
	void gpio_003c_w(uint32_t data); // GPIO_INTSTAT_C (write)

	tp_dma_state &tp_dma(uint32_t proc, uint32_t channel)
	{ 
		return m_tp_dma[proc * TP_DMA_COUNT + channel];
	}
	uint32_t &tp_reg(uint32_t proc, uint32_t byte_offset)
	{
		return m_tp_regs[(proc * TP_PROC_STRIDE + byte_offset) >> 2];
	}
	uint32_t &tp_dma_reg(uint32_t proc, uint32_t channel, uint32_t reg)
	{
		return tp_reg(proc, TP_DMA + channel * TP_DMA_STRIDE + reg);
	}

	bool tp_decode(uint32_t byte_offset, uint32_t &proc, uint32_t &reg);
	uint32_t tp_r(offs_t offset);
	void tp_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void tp_dma_control_w(uint32_t proc, uint32_t channel, uint32_t old_data, uint32_t data);
	bool tp_dma_load_next(uint32_t proc, uint32_t channel);
	void tp_dma_raise(uint32_t proc, uint32_t channel, uint32_t status);
	bool tp_dma_write_record(uint32_t proc, uint32_t channel, const uint8_t *packet);
	void tp_update_irq();

	int tp_find_channel(uint32_t proc, uint16_t scid);
	void append_guide_object(guide_cycle &cycle, uint16_t net_id, uint8_t object_type, uint32_t object_key, const std::vector<uint8_t> &data);
	void build_guide_cycle(guide_cycle &cycle, bool with_time);

};

DECLARE_DEVICE_TYPE(FUD_ASIC, fud_asic_device)

#endif // MAME_WEBTV_FUD_ASIC_H