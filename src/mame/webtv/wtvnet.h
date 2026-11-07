// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVNET_H
#define MAME_WEBTV_WTVNET_H

#pragma once

#include "cpu/mips/mips3.h"
#include "dinetwork.h"

#include "wtvnet_link.h"

class wtvnet_device : public device_t, public device_network_interface
{

public:

	wtvnet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override;

	// device_network_interface
	virtual void recv_cb(u8 *buf, int len) override;

private:

	static constexpr uint32_t MAGIC       = 0x574e4554; // 'WNET'
	static constexpr uint32_t VERSION     = 0x00010060; // (1 << 16) | header size
	static constexpr uint32_t SLOTS       = 8;
	static constexpr uint32_t STRIDE      = 0x600;
	static constexpr uint32_t SLOT_DATA   = 4; // after the u32 length
	static constexpr uint32_t TX_OFFSET   = 0x100;
	static constexpr uint32_t RX_OFFSET   = 0x3100;
	static constexpr uint32_t RESERVATION = 0x6100; // header and both rings

	// Block offsets.
	static constexpr uint32_t T_MAGIC     = 0x00;
	static constexpr uint32_t T_VERSION   = 0x04;
	static constexpr uint32_t G_HEARTBEAT = 0x08;
	static constexpr uint32_t H_HEARTBEAT = 0x0c;
	static constexpr uint32_t G_FLAGS     = 0x10;
	static constexpr uint32_t H_FLAGS     = 0x14;
	static constexpr uint32_t G_MTU       = 0x18;
	static constexpr uint32_t G_SLOTS     = 0x1c;
	static constexpr uint32_t TX_BASE     = 0x20;
	static constexpr uint32_t TX_PROD     = 0x24;
	static constexpr uint32_t TX_CONS     = 0x28;
	static constexpr uint32_t RX_BASE     = 0x2c;
	static constexpr uint32_t RX_PROD     = 0x30;
	static constexpr uint32_t RX_CONS     = 0x34;
	static constexpr uint32_t H_IP        = 0x38;
	static constexpr uint32_t H_MASK      = 0x3c;
	static constexpr uint32_t H_GATEWAY   = 0x40;
	static constexpr uint32_t H_DNS1      = 0x44;
	static constexpr uint32_t H_DNS2      = 0x48;
	static constexpr uint32_t G_TX_DROPS  = 0x4c;
	static constexpr uint32_t H_RX_DROPS  = 0x50;
	static constexpr uint32_t H_EPOCH     = 0x54;
	static constexpr uint32_t G_EPOCH     = 0x58;
	static constexpr uint32_t H_ERROR     = 0x5c;

	// Flags.
	static constexpr uint32_t HOST_ENABLED  = 1;
	static constexpr uint32_t HOST_READY    = 2;
	static constexpr uint32_t GUEST_ACTIVE  = 1;
	static constexpr uint32_t GUEST_LATCHED = 2;

	static constexpr uint32_t POLL_PERIOD = 20;     // ms
	static constexpr uint64_t GUEST_TIMEOUT = 5000; // ms without a guest heartbeat

	required_device<mips3_device> m_hostcpu;

	emu_timer *m_timer = nullptr;
	ethernet_link m_link;
	uint32_t m_table = 0;
	uint32_t m_guest_heartbeat = 0;
	uint32_t m_host_heartbeat = 0;
	uint64_t m_guest_seen = 0;
	bool m_live = false;

	TIMER_CALLBACK_MEMBER(poll);
	void restart_link();
	void receive_ip(const uint8_t *data, size_t length);

	bool table_valid(uint32_t table);
	uint32_t read(uint32_t offset);
	void write(uint32_t offset, uint32_t data);
};

DECLARE_DEVICE_TYPE(WTVNET, wtvnet_device)

#endif // MAME_WEBTV_WTVNET_H
