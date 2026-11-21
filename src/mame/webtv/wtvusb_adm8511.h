// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ADMtek ADM8511 (Pegasus II ethernet board) USB 1.1 to 10/100 Ethernet adapter
//
// Some reverse engineering from adm8511.dll, some from the Linux driver

#ifndef MAME_WEBTV_WTVUSB_ADM8511_H
#define MAME_WEBTV_WTVUSB_ADM8511_H

#pragma once

#include "wtvusb.h"
#include "dinetwork.h"

#include <deque>

class wtvusb_adm8511_device : public device_t, public wtvusb_device_interface, public device_network_interface
{

public:

	wtvusb_adm8511_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	// wtvusb_device_interface
	virtual void usb_reset() override;
	virtual int32_t usb_control_in(const setup_packet_t &setup, uint8_t *data) override;
	virtual int32_t usb_control_out(const setup_packet_t &setup, const uint8_t *data) override;
	virtual int32_t usb_endpoint_in(int32_t endpoint, uint8_t *buffer, uint32_t max_length) override;
	virtual int32_t usb_endpoint_out(int32_t endpoint, const uint8_t *buffer, uint32_t length) override;

	// device_network_interface
	virtual int recv_start_cb(u8 *buf, int len) override;

private:

	// From default.reg
	static constexpr uint16_t VENDOR_ID  = 0x050d;
	static constexpr uint16_t PRODUCT_ID = 0x0122;

	static constexpr uint8_t REQ_GET_REGS = 0xf0;
	static constexpr uint8_t REQ_SET_REGS = 0xf1;

	static constexpr int32_t EP_BULK_IN  = 1;
	static constexpr int32_t EP_BULK_OUT = 2;
	static constexpr int32_t EP_INTR_IN  = 3;
	static constexpr int32_t BULK_MAX_PACKET = 64;

	// Registers
	static constexpr uint8_t REG_ETH_CTRL0  = 0x00;
	static constexpr uint8_t REG_ETH_CTRL1  = 0x01;
	static constexpr uint8_t REG_ETH_CTRL2  = 0x02;
	static constexpr uint8_t REG_MCAST      = 0x08;
	static constexpr uint8_t REG_ETH_ID     = 0x10;
	static constexpr uint8_t REG_EE_OFFSET  = 0x20;
	static constexpr uint8_t REG_EE_DATA    = 0x21;
	static constexpr uint8_t REG_EE_CTRL    = 0x23;
	static constexpr uint8_t REG_PHY_ADDR   = 0x25;
	static constexpr uint8_t REG_PHY_DATA   = 0x26;
	static constexpr uint8_t REG_PHY_CTRL   = 0x28;
	static constexpr uint8_t REG_GPIO0      = 0x7e;

	static constexpr uint8_t CTRL0_RX_EN      = 0x08;
	static constexpr uint8_t CTRL0_RX_MCAST   = 0x02;
	static constexpr uint8_t CTRL2_RX_PROMISC = 0x04;

	static constexpr uint8_t EE_WRITE = 0x01;
	static constexpr uint8_t EE_READ  = 0x02;
	static constexpr uint8_t EE_DONE  = 0x04;

	static constexpr uint8_t PHY_WRITE = 0x20;
	static constexpr uint8_t PHY_READ  = 0x40;
	static constexpr uint8_t PHY_DONE  = 0x80;

	static constexpr size_t RX_QUEUE_MAX = 32;

	static constexpr uint32_t MAX_FRAME = 1536;

	void reset_chip();
	void registers_written(uint8_t first, uint32_t count);
	uint16_t phy_read(uint8_t reg_idx);
	void phy_write(uint8_t reg_idx, uint16_t data);
	bool accept_frame(const uint8_t *frame, uint32_t length) const;

	uint8_t m_regs[256];
	uint16_t m_eeprom[64];
	uint16_t m_phy_bmcr;
	uint16_t m_phy_anar;
	uint8_t m_mac[6];

	std::vector<uint8_t> m_tx;
	std::deque<std::vector<uint8_t>> m_rx_queue;
	std::vector<uint8_t> m_rx_current;
	uint32_t m_rx_pos;
	bool m_rx_zlp;

};

DECLARE_DEVICE_TYPE(WTVUSB_ADM8511, wtvusb_adm8511_device)

#endif // MAME_WEBTV_WTVUSB_ADM8511_H
