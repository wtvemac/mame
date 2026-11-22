// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvusb_adm8511.h"

DEFINE_DEVICE_TYPE(WTVUSB_ADM8511, wtvusb_adm8511_device, "wtvusb_adm8511", "ADMtek USB Ethernet (ADM8511)")

wtvusb_adm8511_device::wtvusb_adm8511_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, WTVUSB_ADM8511, tag, owner, clock),
	wtvusb_device_interface(mconfig, *this),
	device_network_interface(mconfig, *this, 10),
	m_phy_bmcr(0),
	m_phy_anar(0),
	m_mac { 0x02, 0x00, 0x4d, 0x00, 0x00, 0x00 },
	m_rx_pos(0),
	m_rx_zlp(false)
{
	// Fields are little-endian.
	set_device_descriptor({
		0x12,                                // bLength
		DESC_DEVICE,                         // bDescriptorType
		0x10, 0x01,                          // bcdUSB: USB 1.1
		0x00,                                // bDeviceClass: given per interface
		0x00,                                // bDeviceSubClass
		0x00,                                // bDeviceProtocol
		0x08,                                // bMaxPacketSize0
		VENDOR_ID  & 0xff, VENDOR_ID  >> 8,  // idVendor
		PRODUCT_ID & 0xff, PRODUCT_ID >> 8,  // idProduct
		0x01, 0x01,                          // bcdDevice: 1.01
		0x00,                                // iManufacturer: none
		0x00,                                // iProduct: none
		0x00,                                // iSerialNumber: none
		0x01                                 // bNumConfigurations
	});

	set_configuration_descriptor({
		// Configuration
		0x09,                                // bLength
		DESC_CONFIGURATION,                  // bDescriptorType
		0x27, 0x00,                          // wTotalLength: 9 + 9 + 3 * 7 = 39
		0x01,                                // bNumInterfaces
		0x01,                                // bConfigurationValue
		0x00,                                // iConfiguration: none
		0x80,                                // bmAttributes: bus powered
		0x64,                                // bMaxPower: 200 mA

		// Interface 0
		0x09,                                // bLength
		DESC_INTERFACE,                      // bDescriptorType
		0x00,                                // bInterfaceNumber
		0x00,                                // bAlternateSetting
		0x03,                                // bNumEndpoints
		0xff,                                // bInterfaceClass: vendor specific
		0x00,                                // bInterfaceSubClass
		0x00,                                // bInterfaceProtocol
		0x00,                                // iInterface: none

		// Bulk IN: received frames
		0x07,                                // bLength
		DESC_ENDPOINT,                       // bDescriptorType
		EP_DIR_IN | EP_BULK_IN,              // bEndpointAddress
		EP_ATTR_BULK,                        // bmAttributes
		BULK_MAX_PACKET, 0x00,               // wMaxPacketSize
		0x00,                                // bInterval: unused for bulk

		// Bulk OUT: frames to send
		0x07,                                // bLength
		DESC_ENDPOINT,                       // bDescriptorType
		EP_BULK_OUT,                         // bEndpointAddress
		EP_ATTR_BULK,                        // bmAttributes
		BULK_MAX_PACKET, 0x00,               // wMaxPacketSize
		0x00,                                // bInterval: unused for bulk

		// Interrupt IN: link status
		0x07,                                // bLength
		DESC_ENDPOINT,                       // bDescriptorType
		EP_DIR_IN | EP_INTR_IN,              // bEndpointAddress
		EP_ATTR_INTR,                        // bmAttributes
		0x08, 0x00,                          // wMaxPacketSize
		0x01                                 // bInterval: 1 ms
	});
}

void wtvusb_adm8511_device::device_start()
{
	// Take a quick hash of the system name to generate the NIC ID part of the MAC addr
	// This is from the -wtvnet implementation (FNV-1a hash)
	uint32_t hash = 2166136261U;
	for (const char *p = machine().system().name; *p; ++p)
		hash = (hash ^ uint8_t(*p)) * 16777619U;

	m_mac[3] = uint8_t(hash >> 16);
	m_mac[4] = uint8_t(hash >> 8);
	m_mac[5] = uint8_t(hash);

	std::fill(std::begin(m_eeprom), std::end(m_eeprom), 0);
	for (uint32_t i = 0; i < 3; i++)
		m_eeprom[i] = m_mac[i * 2] | (m_mac[i * 2 + 1] << 8);

	set_mac(m_mac);

	save_item(NAME(m_regs));
	save_item(NAME(m_eeprom));
	save_item(NAME(m_phy_bmcr));
	save_item(NAME(m_phy_anar));
	save_item(NAME(m_mac));
	save_item(NAME(m_rx_pos));
	save_item(NAME(m_rx_zlp));
}

void wtvusb_adm8511_device::device_reset()
{
	reset_chip();
}

void wtvusb_adm8511_device::reset_chip()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);

	for (uint32_t i = 0; i < 3; i++)
	{
		m_mac[i * 2] = m_eeprom[i] & 0xff;
		m_mac[i * 2 + 1] = m_eeprom[i] >> 8;
	}

	std::copy_n(m_mac, 6, &m_regs[REG_ETH_ID]);
	set_mac(m_mac);

	m_phy_bmcr = 0x3100; // autonegotiation, 100Mb, full duplex
	m_phy_anar = 0x01e1;

	m_tx.clear();
	m_rx_queue.clear();
	m_rx_current.clear();
	m_rx_pos = 0;
	m_rx_zlp = false;
}

void wtvusb_adm8511_device::usb_reset()
{
	m_tx.clear();
	m_rx_current.clear();
	m_rx_pos = 0;
	m_rx_zlp = false;
}

//
// Registers
//

int32_t wtvusb_adm8511_device::usb_control_in(const setup_packet_t &setup, uint8_t *data)
{
	if ((setup.request_type & REQTYPE_TYPE_MASK) != REQTYPE_VENDOR || setup.request != REQ_GET_REGS)
		return USB_STALL;

	for (uint32_t i = 0; i < setup.length; i++)
		data[i] = m_regs[(setup.index + i) & 0xff];

	return setup.length;
}

int32_t wtvusb_adm8511_device::usb_control_out(const setup_packet_t &setup, const uint8_t *data)
{
	if ((setup.request_type & REQTYPE_TYPE_MASK) != REQTYPE_VENDOR || setup.request != REQ_SET_REGS)
		return USB_STALL;

	const uint8_t first = setup.index & 0xff;

	if (setup.length == 0)
	{
		m_regs[first] = setup.value & 0xff;
		wtvusb_adm8511_device::registers_written(first, 1);
	}
	else
	{
		for (uint32_t i = 0; i < setup.length; i++)
			m_regs[(first + i) & 0xff] = data[i];

		wtvusb_adm8511_device::registers_written(first, setup.length);
	}

	return 0;
}

void wtvusb_adm8511_device::registers_written(uint8_t first, uint32_t count)
{
	auto written = [first, count] (uint8_t reg) {
		return uint8_t(reg - first) < count;
	};

	if (written(REG_ETH_CTRL1) && (m_regs[REG_ETH_CTRL1] & 0x08))
	{
		// MAC reset completes immediately
		m_regs[REG_ETH_CTRL1] &= ~0x08;
		m_rx_queue.clear();
	}

	for (int32_t i = 0; i < 6; i++)
	{
		if (written(REG_ETH_ID + i))
		{
			std::copy_n(&m_regs[REG_ETH_ID], 6, m_mac);
			set_mac(m_mac);
			break;
		}
	}

	if (written(REG_EE_CTRL))
	{
		const uint8_t control = m_regs[REG_EE_CTRL];
		const uint8_t offset = m_regs[REG_EE_OFFSET] & 0x3f;

		if (control & EE_READ)
		{
			m_regs[REG_EE_DATA] = m_eeprom[offset] & 0xff;
			m_regs[REG_EE_DATA + 1] = m_eeprom[offset] >> 8;
		}
		else if (control & EE_WRITE)
		{
			m_eeprom[offset] = m_regs[REG_EE_DATA] | (m_regs[REG_EE_DATA + 1] << 8);
		}

		m_regs[REG_EE_CTRL] = (control & ~(EE_READ | EE_WRITE)) | EE_DONE;
	}

	if (written(REG_PHY_CTRL))
	{
		const uint8_t control = m_regs[REG_PHY_CTRL];
		const uint8_t reg = control & 0x1f;

		// The internal PHY answers at any address (the driver probes from 0, and writes with address 0)
		if (control & PHY_READ)
		{
			const uint16_t data = phy_read(reg);
			m_regs[REG_PHY_DATA] = data & 0xff;
			m_regs[REG_PHY_DATA + 1] = data >> 8;
		}
		else if (control & PHY_WRITE)
		{
			phy_write(reg, m_regs[REG_PHY_DATA] | (m_regs[REG_PHY_DATA + 1] << 8));
		}

		m_regs[REG_PHY_CTRL] = reg | PHY_DONE;
	}
}

uint16_t wtvusb_adm8511_device::phy_read(uint8_t reg_idx)
{
	switch (reg_idx)
	{
		case 0:
			return m_phy_bmcr;

		case 1:
			return 0x782d; // 100/10 full/half capable, autonegotiation complete, link up, extended registers

		case 2:
			return 0x0022; // PHY ID

		case 3:
			return 0x5610;

		case 4:
			return m_phy_anar;

		case 5:
			return 0x41e1; // link partner 100/10 full/half, acknowledge

		case 6:
			return 0x0001; // link partner can autonegotiate

		default:
			return 0x0000;
	}
}

void wtvusb_adm8511_device::phy_write(uint8_t reg_idx, uint16_t data)
{
	switch (reg_idx)
	{
		case 0:
			// reset and restart autonegotiation then complete immediately
			m_phy_bmcr = (data & 0x8000) ? 0x3100 : (data & ~0x0200);
			break;

		case 4:
			m_phy_anar = data;
			break;

		default:
			break;
	}
}

//
// Data endpoints
//

int32_t wtvusb_adm8511_device::usb_endpoint_in(int32_t endpoint, uint8_t *buffer, uint32_t max_length)
{
	if (endpoint == EP_INTR_IN)
	{
		return USB_NAK; // ADM8511.dll doesn't use the interrupt endpoint
	}
	else if (endpoint != EP_BULK_IN)
	{
		return USB_STALL;
	}
	else if (m_rx_zlp)
	{
		// the frame ended on a full packet
		m_rx_zlp = false;

		return 0;
	}
	else
	{
		if (m_rx_current.empty())
		{
			if (m_rx_queue.empty())
				return USB_NAK;

			m_rx_current = std::move(m_rx_queue.front());
			m_rx_queue.pop_front();
			m_rx_pos = 0;

			// received length including the 4 byte FCS, status
			const uint32_t length = m_rx_current.size() + 4;
			m_rx_current.push_back(length & 0xff);
			m_rx_current.push_back((length >> 8) & 0x0f);
			m_rx_current.push_back(0x00);
			m_rx_current.push_back(0x00);
		}

		const int32_t count = std::min<int32_t>(max_length, m_rx_current.size() - m_rx_pos);
		std::copy_n(&m_rx_current[m_rx_pos], count, buffer);
		m_rx_pos += count;

		if (m_rx_pos >= m_rx_current.size())
		{
			m_rx_current.clear();
			m_rx_pos = 0;
			// Sends a zero length packet when the frame ended with a full wMaxPacketSize packet
			m_rx_zlp = (count == BULK_MAX_PACKET);
		}

		return count;
	}
}

int32_t wtvusb_adm8511_device::usb_endpoint_out(int32_t endpoint, const uint8_t *buffer, uint32_t length)
{
	if (endpoint != EP_BULK_OUT)
	{
		return USB_STALL;
	}
	else if (length == 0 && m_tx.empty())
	{
		return 0; // the zero length transfer after a frame that filled its last packet

	}
	else
	{
		if (m_tx.size() + length <= MAX_FRAME + 2 + BULK_MAX_PACKET)
			m_tx.insert(m_tx.end(), buffer, buffer + length);

		if (m_tx.size() >= 2)
		{
			const uint32_t frame_length = (m_tx[0] | (m_tx[1] << 8)) & 0x07ff;

			if (m_tx.size() >= frame_length + 2)
			{
				if (frame_length > 0 && frame_length <= MAX_FRAME)
				{
					std::vector<uint8_t> frame(m_tx.begin() + 2, m_tx.begin() + 2 + frame_length);
					send(frame.data(), frame.size());
				}

				m_tx.clear();

				return length;
			}
		}

		// A short packet ends the transfer anything still incomplete is dropped.
		if (length < BULK_MAX_PACKET)
			m_tx.clear();

		return length;
	}
}

bool wtvusb_adm8511_device::accept_frame(const uint8_t *frame, uint32_t length) const
{
	if (length < 14 || !(m_regs[REG_ETH_CTRL0] & CTRL0_RX_EN))
		return false;
	else if (m_regs[REG_ETH_CTRL2] & CTRL2_RX_PROMISC)
		return true;
	else if (frame[0] & 0x01)
		return true; // broadcast and multicast (the hash filter isn't emulated)
	else
		return std::equal(m_mac, m_mac + 6, frame);
}

int wtvusb_adm8511_device::recv_start_cb(u8 *buf, int len)
{
	if (!usb_configuration() || len > MAX_FRAME || !accept_frame(buf, len))
	{
		return 0;
	}
	else if (m_rx_queue.size() >= RX_QUEUE_MAX)
	{
		return 0;
	}
	else
	{
		m_rx_queue.emplace_back(buf, buf + len);

		return len;
	}
}
