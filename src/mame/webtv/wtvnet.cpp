// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvnet.h"
#include "wtvfart.h"

DEFINE_DEVICE_TYPE(WTVNET, wtvnet_device, "wtvnet", "WebTV IP network bypass")

wtvnet_device::wtvnet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVNET, tag, owner, clock),
	device_network_interface(mconfig, *this, 10),
	m_hostcpu(*this, finder_base::DUMMY_TAG)
{
}

void wtvnet_device::device_start()
{
	m_timer = timer_alloc(FUNC(wtvnet_device::poll), this);

	// Take a quick hash of the system name to generate the NIC ID part of the MAC addr
	uint32_t hash = 2166136261U;
	for (const char *p = machine().system().name; *p; ++p)
		hash = (hash ^ uint8_t(*p)) * 16777619U;

	m_link.mac = { 0x02, 0x57, 0x54, uint8_t(hash >> 16), uint8_t(hash >> 8), uint8_t(hash) };

	set_mac(m_link.mac.data());

	m_link.transmit = [this](const ethernet_link::packet &frame)
	{
		send(const_cast<u8 *>(frame.data()), frame.size());
	};

	m_link.receive_ip = [this](const uint8_t *data, size_t length)
	{
		wtvnet_device::receive_ip(data, length);
	};
}

void wtvnet_device::device_reset()
{
	m_table = 0;
	m_live = false;
	m_link.mac = get_mac();
	m_link.reset();
	m_host_heartbeat = 0;

	if (get_interface() < 0)
		set_interface(0);

	m_timer->adjust(attotime::zero, 0, attotime::from_msec(POLL_PERIOD));
}

void wtvnet_device::device_post_load()
{
	// Provider sockets and DHCP/ARP leases can't be restored from a save state.
	m_table = 0;
	m_live = false;
	m_link.reset();
}

TIMER_CALLBACK_MEMBER(wtvnet_device::poll)
{
	const uint64_t now = machine().time().as_ticks(1000);
	if (m_link.mac != get_mac())
	{
		m_link.mac = get_mac();
		m_link.reset();
	}

	const uint32_t table = wtvfart::find_feature(m_hostcpu->space(AS_PROGRAM), wtvfart::FEATURE_NET);
	if (!table || !table_valid(table))
	{
		if (m_table)
			m_link.reset();

		m_table = 0;
		m_live = false;

		return;
	}

	if (table != m_table)
	{
		m_table = table;
		m_guest_heartbeat = read(G_HEARTBEAT);
		m_guest_seen = now;
		m_live = false;

		wtvnet_device::restart_link();
	}

	const uint32_t heartbeat = read(G_HEARTBEAT);
	if (heartbeat != m_guest_heartbeat)
	{
		if (uint32_t(heartbeat - m_guest_heartbeat) > 0x80000000U)
		{
			// The heartbeat went backwards
			wtvnet_device::restart_link();
		}

		m_guest_heartbeat = heartbeat;
		m_guest_seen = now;
		m_live = true;
	}

	if (now - m_guest_seen >= GUEST_TIMEOUT || !has_net_device())
	{
		if (m_live)
			m_link.reset();

		m_live = false;
	}

	wtvnet_device::write(H_HEARTBEAT, ++m_host_heartbeat);
	if (!m_live)
	{
		wtvnet_device::write(H_FLAGS, 0);

		return;
	}

	const uint32_t tx_queued = read(TX_PROD) - read(TX_CONS);
	const uint32_t rx_queued = read(RX_PROD) - read(RX_CONS);
	if (tx_queued > SLOTS || rx_queued > SLOTS)
	{
		write(H_ERROR, 1);
		write(H_FLAGS, 0);

		return; // never repair the guest's counters
	}

	m_link.tick(now);

	wtvnet_device::write(H_IP, m_link.config.ip);
	wtvnet_device::write(H_MASK, m_link.config.mask);
	wtvnet_device::write(H_GATEWAY, m_link.config.gateway);
	wtvnet_device::write(H_DNS1, m_link.config.dns1);
	wtvnet_device::write(H_DNS2, m_link.config.dns2);
	wtvnet_device::write(H_EPOCH, m_link.epoch);
	wtvnet_device::write(H_ERROR, 0);
	wtvnet_device::write(H_FLAGS, HOST_ENABLED | (m_link.ready ? HOST_READY : 0));

	if (!m_link.ready || wtvnet_device::read(G_EPOCH) != m_link.epoch || !(wtvnet_device::read(G_FLAGS) & GUEST_ACTIVE))
		return;

	address_space &space = m_hostcpu->space(AS_PROGRAM);
	for (uint32_t count = 0; count < SLOTS && wtvnet_device::read(TX_CONS) != wtvnet_device::read(TX_PROD); ++count)
	{
		const uint32_t consumer = wtvnet_device::read(TX_CONS);
		const uint32_t slot = wtvnet_device::read(TX_BASE) + (consumer % SLOTS) * STRIDE;
		const uint32_t length = wtvfart::read_word(space, slot);
		if (length >= ethernet_link::IPV4_HEADER_SIZE && length <= ethernet_link::MTU)
		{
			ethernet_link::packet data(length);

			for (uint32_t i = 0; i < length; ++i)
				data[i] = space.read_byte((slot + SLOT_DATA + i) & 0x1fffffff);

			if (ethernet_link::ipv4_valid(data.data(), data.size()) && !m_link.send_ip(data.data(), data.size()))
				break; // retry this slot on the next poll
		}

		write(TX_CONS, consumer + 1);
	}
}

void wtvnet_device::restart_link()
{
	m_link.reset();

	m_link.epoch = wtvnet_device::read(H_EPOCH) + 1;

	if (!m_link.epoch)
		m_link.epoch++;
}

void wtvnet_device::receive_ip(const uint8_t *data, size_t length)
{
	if (length > ethernet_link::MTU || !m_live || !wtvnet_device::table_valid(m_table) || wtvnet_device::read(G_EPOCH) != m_link.epoch || !(wtvnet_device::read(G_FLAGS) & GUEST_ACTIVE))
		return;

	const uint32_t producer = wtvnet_device::read(RX_PROD);
	if (producer - wtvnet_device::read(RX_CONS) >= SLOTS)
	{
		wtvnet_device::write(H_RX_DROPS, wtvnet_device::read(H_RX_DROPS) + 1);
		return;
	}

	const uint32_t slot = wtvnet_device::read(RX_BASE) + (producer % SLOTS) * STRIDE;
	address_space &space = m_hostcpu->space(AS_PROGRAM);

	for (uint32_t i = 0; i < length; ++i)
		space.write_byte((slot + SLOT_DATA + i) & 0x1fffffff, data[i]);

	space.write_dword(slot & 0x1fffffff, length);
	wtvnet_device::write(RX_PROD, producer + 1);
}

void wtvnet_device::recv_cb(u8 *buf, int len)
{
	if (len >= int(ethernet_link::ETHERNET_HEADER_SIZE) && len <= int(ethernet_link::ETHERNET_MAX_FRAME_SIZE))
		m_link.receive(buf, len);
}

bool wtvnet_device::table_valid(uint32_t table)
{
	address_space &space = m_hostcpu->space(AS_PROGRAM);
	if (wtvfart::overlaps_discovery(table, RESERVATION) || !wtvfart::ram_range_valid(space, table, RESERVATION))
		return false;

	const auto word = [&space, table](uint32_t offset)
	{
		return wtvfart::read_word(space, table + offset);
	};

	return word(T_MAGIC) == MAGIC && word(T_VERSION) == VERSION && word(G_MTU) == ethernet_link::MTU && word(G_SLOTS) == SLOTS && word(TX_BASE) == table + TX_OFFSET && word(RX_BASE) == table + RX_OFFSET;
}

uint32_t wtvnet_device::read(uint32_t offset)
{
	return wtvfart::read_word(m_hostcpu->space(AS_PROGRAM), m_table + offset);
}

void wtvnet_device::write(uint32_t offset, uint32_t data)
{
	m_hostcpu->space(AS_PROGRAM).write_dword((m_table + offset) & 0x1fffffff, data);
}
