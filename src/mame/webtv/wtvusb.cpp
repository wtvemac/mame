// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvusb.h"
#include "wtvusb_adm8511.h"

DEFINE_DEVICE_TYPE(WTVUSB_PORT, wtvusb_port_device, "wtvusb_port", "WebTV USB port")

wtvusb_device_interface::wtvusb_device_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "wtvusb"),
	m_address(0),
	m_pending_address(0),
	m_address_pending(false),
	m_configuration(0),
	m_halted(0),
	m_control_state(wtvusb_device_interface::control_state_t::CONTROL_IDLE),
	m_setup {
		.request_type = 0,
		.request = 0,
		.value = 0,
		.index = 0,
		.length = 0
	},
	m_control_pos(0),
	m_control_short(false)
{
	//
}

wtvusb_device_interface::~wtvusb_device_interface()
{
	//
}

void wtvusb_device_interface::interface_pre_start()
{
	device().save_item(NAME(m_address));
	device().save_item(NAME(m_pending_address));
	device().save_item(NAME(m_address_pending));
	device().save_item(NAME(m_configuration));
	device().save_item(NAME(m_halted));
	device().save_item(NAME(m_control_pos));
	device().save_item(NAME(m_control_short));
}

void wtvusb_device_interface::add_string(const uint8_t *string)
{
	if (m_strings.empty())
		// String index 0 is the language ID list (US English here)
		m_strings.push_back(
			{
				4,
				wtvusb_device_interface::DESC_STRING,
				0x09,
				0x04 
			}
		);

	std::vector<uint8_t> descriptor = {
		2,
		DESC_STRING
	};

	for (const uint8_t *c = string; *c; c++)
	{
		descriptor.push_back(uint8_t(*c));
		descriptor.push_back(0x00);
	}

	descriptor[0] = uint8_t(descriptor.size());

	m_strings.push_back(descriptor);
}

void wtvusb_device_interface::usb_bus_reset()
{
	m_address = 0;
	m_pending_address = 0;
	m_address_pending = false;
	m_configuration = 0;
	m_halted = 0;
	m_control_state = wtvusb_device_interface::control_state_t::CONTROL_IDLE;
	m_control_data.clear();
	m_control_pos = 0;
	m_control_short = false;

	usb_reset();
}

int32_t wtvusb_device_interface::usb_transaction(wtvusb_device_interface::usb_pid_t pid, int32_t endpoint, uint8_t *buffer, uint32_t length)
{
	// Transactions are one packet each
	// returns the number of bytes moved (IN) or accepted (SETUP/OUT) or NAK / STALL

	endpoint &= 0x0f;

	if (endpoint == 0)
		return wtvusb_device_interface::control_transaction(pid, buffer, length);
	else if (pid == wtvusb_device_interface::usb_pid_t::PID_SETUP || !m_configuration || (m_halted & (1 << endpoint)))
		return wtvusb_device_interface::USB_STALL;
	else if (pid == wtvusb_device_interface::usb_pid_t::PID_IN)
		return usb_endpoint_in(endpoint, buffer, length);
	else
		return usb_endpoint_out(endpoint, buffer, length);
}

int32_t wtvusb_device_interface::control_transaction(wtvusb_device_interface::usb_pid_t pid, uint8_t *buffer, uint32_t length)
{
	if (pid == wtvusb_device_interface::usb_pid_t::PID_SETUP)
	{
		return wtvusb_device_interface::get_setup_packet(buffer, length);
	}
	else
	{
		switch (m_control_state)
		{
			case wtvusb_device_interface::CONTROL_DATA_IN:
			{
				if (pid == wtvusb_device_interface::usb_pid_t::PID_IN)
				{
					int32_t count = std::min<int32_t>(length, m_control_data.size() - m_control_pos);

					std::copy_n(&m_control_data[m_control_pos], count, buffer);
					m_control_pos += count;

					return count;
				}

				// OUT ends the data stage early (status stage)
				m_control_state = wtvusb_device_interface::control_state_t::CONTROL_IDLE;

				return 0;
			}

			case wtvusb_device_interface::CONTROL_DATA_OUT:
			{
				if (pid == wtvusb_device_interface::usb_pid_t::PID_OUT)
				{
					int32_t count = std::min<int32_t>(length, m_setup.length - m_control_data.size());
					m_control_data.insert(m_control_data.end(), buffer, buffer + count);

					if (m_control_data.size() >= m_setup.length)
					{
						const bool standard = (m_setup.request_type & wtvusb_device_interface::REQTYPE_TYPE_MASK) == wtvusb_device_interface::REQTYPE_STANDARD;
						
						bool ok;
						if (standard)
							ok = false;
						else
							ok = (usb_control_out(m_setup, m_control_data.data()) >= 0);

						m_control_state = ok ? wtvusb_device_interface::control_state_t::CONTROL_STATUS_IN : wtvusb_device_interface::control_state_t::CONTROL_STALLED;
					}

					return length;
				}
				else
				{
					return wtvusb_device_interface::USB_STALL;
				}
			}

			case wtvusb_device_interface::CONTROL_STATUS_IN:
			{
				if (pid == wtvusb_device_interface::usb_pid_t::PID_IN)
				{
					if (m_address_pending)
					{
						m_address = m_pending_address;
						m_address_pending = false;
					}

					m_control_state = wtvusb_device_interface::control_state_t::CONTROL_IDLE;

					return 0;
				}
				else
				{
					return wtvusb_device_interface::USB_STALL;
				}
			}

			case wtvusb_device_interface::CONTROL_STATUS_OUT:
			{
				if (pid == wtvusb_device_interface::usb_pid_t::PID_OUT)
				{
					m_control_state = wtvusb_device_interface::control_state_t::CONTROL_IDLE;

					return 0;
				}
				else
				{
					return wtvusb_device_interface::USB_STALL;
				}
			}

			case wtvusb_device_interface::CONTROL_STALLED:
				return wtvusb_device_interface::USB_STALL;

			case wtvusb_device_interface::CONTROL_IDLE:
			default:
				// A stray zero length status packet after a completed transfer
				return (pid == wtvusb_device_interface::usb_pid_t::PID_OUT) ? length : wtvusb_device_interface::USB_STALL;
		}
	}
}

int32_t wtvusb_device_interface::get_setup_packet(uint8_t *buffer, uint32_t length)
{
	if (length != wtvusb_device_interface::SETUP_LENGTH)
	{
		return USB_STALL;
	}
	else
	{
		m_setup.request_type = buffer[0];
		m_setup.request      = buffer[1];
		m_setup.value        = buffer[2] | (buffer[3] << 8);
		m_setup.index        = buffer[4] | (buffer[5] << 8);
		m_setup.length       = buffer[6] | (buffer[7] << 8);

		m_control_data.clear();
		m_control_pos = 0;
		m_control_short = false;
		m_address_pending = false;

		const bool standard = ((m_setup.request_type & wtvusb_device_interface::REQTYPE_TYPE_MASK) == wtvusb_device_interface::REQTYPE_STANDARD);

		if (m_setup.request_type & REQTYPE_DIR_IN)
		{
			m_control_data.resize(std::max<uint16_t>(m_setup.length, 2));

			int32_t result;
			if (standard)
				result = wtvusb_device_interface::standard_request_in(m_setup, m_control_data.data());
			else
				result = usb_control_in(m_setup, m_control_data.data());

			if (result < 0)
			{
				m_control_state = wtvusb_device_interface::control_state_t::CONTROL_STALLED;
			}
			else
			{
				m_control_data.resize(std::min<int>(result, m_setup.length));
				m_control_state = m_setup.length ? wtvusb_device_interface::CONTROL_DATA_IN : wtvusb_device_interface::CONTROL_STATUS_OUT;
			}
		}
		else if (m_setup.length == 0)
		{
			bool ok;
			if (standard)
				ok = wtvusb_device_interface::standard_request_out(m_setup);
			else
				ok = (usb_control_out(m_setup, nullptr) >= 0);

			m_control_state = ok ? wtvusb_device_interface::control_state_t::CONTROL_STATUS_IN : wtvusb_device_interface::CONTROL_STALLED;
		}
		else
		{
			m_control_state = wtvusb_device_interface::control_state_t::CONTROL_DATA_OUT;
		}

		return wtvusb_device_interface::SETUP_LENGTH;
	}
}

int32_t wtvusb_device_interface::get_descriptor(uint8_t type, uint8_t index, uint8_t *data, uint32_t max_length)
{
	const std::vector<uint8_t> *descriptor = nullptr;

	if (type == wtvusb_device_interface::DESC_DEVICE)
		descriptor = &m_device_descriptor;
	else if (type == wtvusb_device_interface::DESC_CONFIGURATION && index == 0)
		descriptor = &m_configuration_descriptor;
	else if (type == wtvusb_device_interface::DESC_STRING && index < m_strings.size())
		descriptor = &m_strings[index];

	if (!descriptor || descriptor->empty())
	{
		return USB_STALL;
	}
	else
	{
		int32_t count = std::min<int32_t>(max_length, descriptor->size());
		std::copy_n(descriptor->begin(), count, data);

		return count;
	}
}

int32_t wtvusb_device_interface::standard_request_in(const setup_packet_t &setup, uint8_t *data)
{
	const uint8_t recipient = setup.request_type & wtvusb_device_interface::REQTYPE_RECIP_MASK;

	switch (setup.request)
	{
		case wtvusb_device_interface::REQ_GET_DESCRIPTOR:
			return wtvusb_device_interface::get_descriptor(setup.value >> 8, setup.value & 0xff, data, setup.length);

		case wtvusb_device_interface::REQ_GET_CONFIGURATION:
			data[0] = m_configuration;

			return 1;

		case wtvusb_device_interface::REQ_GET_INTERFACE:
			data[0] = 0;

			return 1;

		case wtvusb_device_interface::REQ_GET_STATUS:
		{
			data[0] = 0;
			data[1] = 0;

			if (recipient == wtvusb_device_interface::REQTYPE_RECIP_EP)
				data[0] = BIT(m_halted, setup.index & 0x0f);

			return 2;
		}

		default:
			return wtvusb_device_interface::USB_STALL;
	}
}

bool wtvusb_device_interface::standard_request_out(const setup_packet_t &setup)
{
	const uint8_t recipient = setup.request_type & REQTYPE_RECIP_MASK;

	switch (setup.request)
	{
		case wtvusb_device_interface::REQ_SET_ADDRESS:
			// Takes effect after the status stage
			m_pending_address = setup.value & 0x7f;
			m_address_pending = true;
			return true;

		case wtvusb_device_interface::REQ_SET_CONFIGURATION:
		{
			const uint8_t value = setup.value & 0xff;
			const uint8_t valid = (m_configuration_descriptor.size() > 5) ? m_configuration_descriptor[5] : 1;

			if (value != 0 && value != valid)
				return false;

			m_configuration = value;
			m_halted = 0;
			usb_configured(value);
			return true;
		}

		case wtvusb_device_interface::REQ_SET_INTERFACE:
			return (setup.value == 0);

		case wtvusb_device_interface::REQ_CLEAR_FEATURE:
		case wtvusb_device_interface::REQ_SET_FEATURE:
		{
			if (recipient == wtvusb_device_interface::REQTYPE_RECIP_EP && setup.value == 0) // ENDPOINT_HALT
			{
				const int32_t endpoint = setup.index & 0x0f;
				const bool halt = (setup.request == wtvusb_device_interface::REQ_SET_FEATURE);

				if (endpoint != 0)
				{
					if (halt)
						m_halted |= (1 << endpoint);
					else
						m_halted &= ~(1 << endpoint);

					usb_endpoint_halt(endpoint, halt);
				}
			}

			return true;
		}

		default:
			return false;
	}
}

wtvusb_port_device::wtvusb_port_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, WTVUSB_PORT, tag, owner, clock),
	device_single_card_slot_interface<wtvusb_device_interface>(mconfig, *this),
	m_function(nullptr)
{
	//
}

void wtvusb_port_device::device_start()
{
	m_function = get_card_device();
}

void wtvusb_devices(device_slot_interface &device)
{
	device.option_add("adm8511", WTVUSB_ADM8511);
}
