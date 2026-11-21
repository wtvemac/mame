// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// Minimal USB 1.1 for a WebTV host controller

#ifndef MAME_WEBTV_WTVUSB_H
#define MAME_WEBTV_WTVUSB_H

#pragma once

#include <vector>

class wtvusb_device_interface : public device_interface
{
public:

	static constexpr int32_t USB_NAK   = -1;
	static constexpr int32_t USB_STALL = -2;

	static constexpr int32_t SETUP_LENGTH = 8;

	// Standard request types and requests (USB 1.1 chapter 9)
	static constexpr uint8_t REQTYPE_DIR_IN      = 0x80;
	static constexpr uint8_t REQTYPE_TYPE_MASK   = 0x60;
	static constexpr uint8_t REQTYPE_STANDARD    = 0x00;
	static constexpr uint8_t REQTYPE_CLASS       = 0x20;
	static constexpr uint8_t REQTYPE_VENDOR      = 0x40;
	static constexpr uint8_t REQTYPE_RECIP_MASK  = 0x1f;
	static constexpr uint8_t REQTYPE_RECIP_DEV   = 0x00;
	static constexpr uint8_t REQTYPE_RECIP_IFACE = 0x01;
	static constexpr uint8_t REQTYPE_RECIP_EP    = 0x02;

	static constexpr uint8_t REQ_GET_STATUS        = 0x00;
	static constexpr uint8_t REQ_CLEAR_FEATURE     = 0x01;
	static constexpr uint8_t REQ_SET_FEATURE       = 0x03;
	static constexpr uint8_t REQ_SET_ADDRESS       = 0x05;
	static constexpr uint8_t REQ_GET_DESCRIPTOR    = 0x06;
	static constexpr uint8_t REQ_SET_DESCRIPTOR    = 0x07;
	static constexpr uint8_t REQ_GET_CONFIGURATION = 0x08;
	static constexpr uint8_t REQ_SET_CONFIGURATION = 0x09;
	static constexpr uint8_t REQ_GET_INTERFACE     = 0x0a;
	static constexpr uint8_t REQ_SET_INTERFACE     = 0x0b;

	static constexpr uint8_t DESC_DEVICE        = 0x01;
	static constexpr uint8_t DESC_CONFIGURATION = 0x02;
	static constexpr uint8_t DESC_STRING        = 0x03;

	enum usb_pid_t : uint8_t
	{
		PID_SETUP = 0,
		PID_OUT   = 1,
		PID_IN    = 2
	};

	struct setup_packet_t
	{
		uint8_t  request_type;
		uint8_t  request;
		uint16_t value;
		uint16_t index;
		uint16_t length;
	};

	wtvusb_device_interface(const machine_config &mconfig, device_t &device);

	virtual ~wtvusb_device_interface();
	
	int32_t usb_transaction(usb_pid_t pid, int32_t endpoint, uint8_t *buffer, uint32_t length);
	void usb_bus_reset();
	uint8_t usb_address() const { return m_address; }
	virtual bool usb_low_speed() const { return false; }

protected:

	virtual void interface_pre_start() override ATTR_COLD;

	void set_device_descriptor(const std::vector<uint8_t> &descriptor) { m_device_descriptor = descriptor; }
	void set_configuration_descriptor(const std::vector<uint8_t> &descriptor) { m_configuration_descriptor = descriptor; }
	void add_string(const uint8_t *string);

	uint8_t usb_configuration() const { return m_configuration; }
	uint8_t ep0_max_packet() const { return m_device_descriptor.size() > 7 ? m_device_descriptor[7] : 8; }

	virtual void usb_reset() { }
	virtual void usb_configured(uint8_t configuration) { }
	virtual void usb_endpoint_halt(int32_t endpoint, bool halted) { }

	virtual int32_t usb_control_in(const setup_packet_t &setup, uint8_t *data) { return USB_STALL; }
	virtual int32_t usb_control_out(const setup_packet_t &setup, const uint8_t *data) { return USB_STALL; }

	virtual int32_t usb_endpoint_in(int32_t endpoint, uint8_t *buffer, uint32_t max_length) { return USB_STALL; }
	virtual int32_t usb_endpoint_out(int32_t endpoint, const uint8_t *buffer, uint32_t length) { return USB_STALL; }

private:

	enum control_state_t : uint8_t
	{
		CONTROL_IDLE,
		CONTROL_DATA_IN,
		CONTROL_DATA_OUT,
		CONTROL_STATUS_IN,  // host reads a zero length status packet
		CONTROL_STATUS_OUT, // host writes a zero length status packet
		CONTROL_STALLED
	};

	int32_t control_transaction(usb_pid_t pid, uint8_t *buffer, uint32_t length);
	int32_t get_setup_packet(uint8_t *buffer, uint32_t length);
	int32_t standard_request_in(const setup_packet_t &setup, uint8_t *data);
	bool standard_request_out(const setup_packet_t &setup);
	int32_t get_descriptor(uint8_t type, uint8_t index, uint8_t *data, uint32_t max_length);

	std::vector<uint8_t> m_device_descriptor;
	std::vector<uint8_t> m_configuration_descriptor;
	std::vector<std::vector<uint8_t>> m_strings;

	uint8_t m_address;
	uint8_t m_pending_address;
	bool m_address_pending;
	uint8_t m_configuration;
	uint32_t m_halted;

	control_state_t m_control_state;
	setup_packet_t m_setup;
	std::vector<uint8_t> m_control_data;
	uint32_t m_control_pos;
	bool m_control_short;

};

class wtvusb_port_device : public device_t, public device_single_card_slot_interface<wtvusb_device_interface>
{

public:

	template <typename T>
	wtvusb_port_device(const machine_config &mconfig, const char *tag, device_t *owner, T &&opts, const char *dflt)
		: wtvusb_port_device(mconfig, tag, owner, 0)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_fixed(false);
	}

	wtvusb_port_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	wtvusb_device_interface *function() const { return m_function; }

protected:

	virtual void device_start() override ATTR_COLD;

private:

	wtvusb_device_interface *m_function;

};

DECLARE_DEVICE_TYPE(WTVUSB_PORT, wtvusb_port_device)

void wtvusb_devices(device_slot_interface &device);

#endif // MAME_WEBTV_WTVUSB_H
