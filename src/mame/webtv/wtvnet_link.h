// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVNET_LINK_H
#define MAME_WEBTV_WTVNET_LINK_H

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <vector>

class ethernet_link
{

public:

	using packet = std::vector<uint8_t>;
	using mac_address = std::array<uint8_t, 6>;

	static constexpr uint32_t MTU = 1500;

	static constexpr size_t   ETHERNET_HEADER_SIZE    = 14;
	static constexpr size_t   ETHERNET_MIN_FRAME_SIZE = 60; // without FCS
	static constexpr size_t   ETHERNET_MAX_FRAME_SIZE = 1522; // with an 802.1Q tag and FCS
	static constexpr uint16_t ETHERTYPE_IPV4          = 0x0800;
	static constexpr uint16_t ETHERTYPE_ARP           = 0x0806;

	static constexpr size_t IPV4_HEADER_SIZE   = 20;
	static constexpr uint8_t IPV4_PROTOCOL_UDP = 17;
	static constexpr uint32_t IPV4_BROADCAST   = 0xffffffff;
	static constexpr size_t UDP_HEADER_SIZE    = 8;

	struct configuration
	{
		uint32_t ip = 0;
		uint32_t mask = 0;
		uint32_t gateway = 0;
		uint32_t dns1 = 0;
		uint32_t dns2 = 0;
	};
	configuration config;

	std::function<void (const packet &)> transmit; // Ethernet frame to the provider
	std::function<void (const uint8_t *, size_t)> receive_ip; // IPv4 datagram for the guest

	mac_address mac = { 0x02, 0x57, 0x54, 0x56, 0x00, 0x01 };
	bool ready = false; // a DHCP lease is bound
	uint32_t epoch = 1; // changes whenever the configuration or neighbours are discarded

	static uint16_t be16(const uint8_t *packet)
	{
		return (uint16_t(packet[0]) << 8) | packet[1];
	}

	static uint32_t be32(const uint8_t *packet)
	{
		return (uint32_t(be16(packet)) << 16) | be16(packet + 2);
	}

	static void put16(uint8_t *packet, uint16_t num_val)
	{
		packet[0] = num_val >> 8;
		packet[1] = num_val;
	}

	static void put32(uint8_t *packet, uint32_t num_val)
	{
		put16(packet, num_val >> 16);
		put16(packet + 2, num_val);
	}

	// Internet checksum of byte_count bytes, continuing from sum (e.g. a UDP pseudo-header). 0 means valid.
	static uint16_t checksum(const uint8_t *packet, size_t byte_count, uint32_t sum = 0)
	{
		for (; byte_count >= 2; byte_count -= 2, packet += 2)
			sum += be16(packet);

		if (byte_count)
			sum += uint16_t(*packet) << 8;

		while (sum >> 16)
			sum = (sum & 0xffff) + (sum >> 16);

		return uint16_t(~sum);
	}

	static size_t ipv4_header_size(const uint8_t *packet)
	{
		return (packet[0] & 15) * 4;
	}

	// A well-formed IPv4 header with a valid checksum whose total length fits in byte_count bytes.
	static bool ipv4_valid(const uint8_t *packet, size_t byte_count)
	{
		if (byte_count < IPV4_HEADER_SIZE || (packet[0] >> 4) != 4)
			return false;

		const size_t header = ipv4_header_size(packet);
		const uint16_t total = be16(packet + 2);
		const bool header_fits = header >= IPV4_HEADER_SIZE && header <= byte_count;
		const bool total_fits = total >= header && total <= byte_count;

		if (!header_fits || !total_fits)
			return false;

		return checksum(packet, header) == 0;
	}

	static bool ipv4_multicast(uint32_t address)
	{
		return (address & 0xf0000000) == 0xe0000000;
	}	

	void reset()
	{
		ready = false;
		config = {};
		epoch++;
		m_xid++;
		m_phase = DISCOVER;
		m_deadline = 0;
		m_expiry = 0;
		m_renew = 0;
		m_attempts = 0;
		m_offer = 0;
		m_server = 0;
		m_neighbours.clear();
		m_pending.clear();
	}

	void tick(uint64_t now)
	{
		m_now = now;

		if (ready && now >= m_expiry)
			reset();

		if (ready && now >= m_renew && m_phase == BOUND)
		{
			m_phase = RENEW;
			m_xid++;
			m_deadline = 0;
		}

		if (m_phase != BOUND && now >= m_deadline)
		{
			if (m_phase == REQUEST && m_attempts >= DHCP_REQUEST_ATTEMPTS)
				reset();

			dhcp(m_phase == DISCOVER ? DHCP_DISCOVER : DHCP_REQUEST);

			m_attempts++;
			m_deadline = now + DHCP_RETRY_TIME;
		}

		for (auto it = m_pending.begin(); it != m_pending.end(); )
		{
			const auto found = m_neighbours.find(it->next);

			if (found != m_neighbours.end() && now < found->second.expires)
			{
				emit(ETHERTYPE_IPV4, found->second.mac, it->data);
				it = m_pending.erase(it);
			}
			else if (now >= it->expires)
			{
				it = m_pending.erase(it);
			}
			else
			{
				if (now >= it->retry)
				{
					arp(ARP_REQUEST, broadcast(), it->next);
					it->retry = now + ARP_RETRY_TIME;
				}

				it++;
			}
		}
	}

	bool send_ip(const uint8_t *packet, size_t byte_count)
	{
		if (!ready || byte_count > MTU || !ipv4_valid(packet, byte_count))
			return false;

		byte_count = be16(packet + 2);
		const uint32_t dst = be32(packet + 16);

		mac_address dest;
		if (dst == IPV4_BROADCAST || dst == subnet_broadcast())
		{
			dest = broadcast();
		}
		else if (ipv4_multicast(dst))
		{
			dest = { 0x01, 0x00, 0x5e, uint8_t((dst >> 16) & 0x7f), uint8_t(dst >> 8), uint8_t(dst) };
		}
		else
		{
			const uint32_t next = ((dst & config.mask) == (config.ip & config.mask)) ? dst : config.gateway;

			if (!next)
				return false;

			const auto found = m_neighbours.find(next);
			if (found == m_neighbours.end() || m_now >= found->second.expires)
			{
				if (m_pending.size() >= MAX_PENDING)
					return false;

				m_pending.push_back({ next, m_now + PENDING_LIFETIME, m_now + ARP_RETRY_TIME, ethernet_link::packet(packet, packet + byte_count) });
				arp(ARP_REQUEST, broadcast(), next);

				return true;
			}

			dest = found->second.mac;
		}

		emit(ETHERTYPE_IPV4, dest, ethernet_link::packet(packet, packet + byte_count));

		return true;
	}

	void receive(const uint8_t *packet, size_t byte_count)
	{
		if (byte_count < ETHERNET_HEADER_SIZE)
			return;

		mac_address dst;
		mac_address src;

		std::copy_n(packet, 6, dst.begin());
		std::copy_n(packet + 6, 6, src.begin());

		if (dst != mac && dst != broadcast() && !(dst[0] & 1))
			return;

		const uint16_t type = be16(packet + 12);

		packet += ETHERNET_HEADER_SIZE;
		byte_count -= ETHERNET_HEADER_SIZE;

		if (type == ETHERTYPE_ARP)
		{
			if (!ready || byte_count < ARP_SIZE)
				return;

			const bool ethernet_ipv4 = be16(packet) == ARP_HTYPE_ETHERNET && be16(packet + 2) == ETHERTYPE_IPV4;
			const bool address_sizes_valid = packet[4] == 6 && packet[5] == 4;

			if (!ethernet_ipv4 || !address_sizes_valid)
				return;

			const uint16_t op = be16(packet + 6);
			const uint32_t sender = be32(packet + 14);
			const bool operation_valid = op == ARP_REQUEST || op == ARP_REPLY;
			const bool sender_matches = std::equal(src.begin(), src.end(), packet + 8);

			if (!operation_valid || !sender || !sender_matches)
				return;

			if (m_neighbours.size() >= MAX_NEIGHBOURS)
				m_neighbours.erase(m_neighbours.begin());

			m_neighbours[sender] = { src, m_now + NEIGHBOUR_LIFETIME };

			if (op == ARP_REQUEST && be32(packet + 24) == config.ip)
				arp(ARP_REPLY, src, sender);
		}
		else if (type == ETHERTYPE_IPV4 && ipv4_valid(packet, byte_count))
		{
			byte_count = be16(packet + 2);

			const size_t h = ipv4_header_size(packet);
			const bool unfragmented = !(be16(packet + 6) & 0x3fff);

			bool dhcp_packet = false;

			if (packet[9] == IPV4_PROTOCOL_UDP && unfragmented && byte_count >= h + UDP_HEADER_SIZE)
			{
				const bool from_server = be16(packet + h) == DHCP_SERVER_PORT;
				const bool to_client = be16(packet + h + 2) == DHCP_CLIENT_PORT;

				dhcp_packet = from_server && to_client;
			}

			if (dhcp_packet)
			{
				const size_t size = be16(packet + h + 4);
				if (size >= UDP_HEADER_SIZE && size <= byte_count - h)
				{
					const uint32_t source_sum = uint32_t(be16(packet + 12)) + be16(packet + 14);
					const uint32_t destination_sum = uint32_t(be16(packet + 16)) + be16(packet + 18);
					const uint32_t pseudo_header = source_sum + destination_sum + IPV4_PROTOCOL_UDP + size;
					const bool checksum_omitted = be16(packet + h + 6) == 0;

					if (checksum_omitted || checksum(packet + h, size, pseudo_header) == 0)
						dhcp_reply(packet + h + UDP_HEADER_SIZE, size - UDP_HEADER_SIZE);
				}
			}
			else if (ready && receive_ip)
			{
				const uint32_t dst = be32(packet + 16);
				const bool for_guest = dst == config.ip;
				const bool broadcast_packet = dst == IPV4_BROADCAST || dst == subnet_broadcast();
				const bool multicast_packet = ipv4_multicast(dst);

				if (for_guest || broadcast_packet || multicast_packet)
					receive_ip(packet, byte_count);
			}
		}
	}

private:

	static constexpr uint16_t ARP_HTYPE_ETHERNET = 1;
	static constexpr uint16_t ARP_REQUEST        = 1;
	static constexpr uint16_t ARP_REPLY          = 2;
	static constexpr size_t   ARP_SIZE           = 28;

	static constexpr uint16_t DHCP_SERVER_PORT         = 67;
	static constexpr uint16_t DHCP_CLIENT_PORT         = 68;
	static constexpr size_t   DHCP_FIXED_SIZE          = 240; // BOOTP fields plus the magic cookie
	static constexpr uint32_t DHCP_MAGIC_COOKIE        = 0x63825363;
	static constexpr uint8_t  DHCP_DISCOVER            = 1;
	static constexpr uint8_t  DHCP_OFFER               = 2;
	static constexpr uint8_t  DHCP_REQUEST             = 3;
	static constexpr uint8_t  DHCP_ACK                 = 5;
	static constexpr uint8_t  DHCP_NAK                 = 6;
	static constexpr uint8_t  DHCP_OPTION_PAD          = 0;
	static constexpr uint8_t  DHCP_OPTION_SUBNET_MASK  = 1;
	static constexpr uint8_t  DHCP_OPTION_ROUTER       = 3;
	static constexpr uint8_t  DHCP_OPTION_DNS          = 6;
	static constexpr uint8_t  DHCP_OPTION_REQUESTED_IP = 50;
	static constexpr uint8_t  DHCP_OPTION_LEASE_TIME   = 51;
	static constexpr uint8_t  DHCP_OPTION_MESSAGE_TYPE = 53;
	static constexpr uint8_t  DHCP_OPTION_SERVER_ID    = 54;
	static constexpr uint8_t  DHCP_OPTION_PARAMETERS   = 55;
	static constexpr uint8_t  DHCP_OPTION_MAX_SIZE     = 57;
	static constexpr uint8_t  DHCP_OPTION_CLIENT_ID    = 61;
	static constexpr uint8_t  DHCP_OPTION_END          = 255;
	static constexpr uint32_t DHCP_DEFAULT_LEASE       = 3600; // seconds, when the server doesn't say

	static constexpr uint64_t DHCP_RETRY_TIME       = 2000;
	static constexpr unsigned DHCP_REQUEST_ATTEMPTS = 5; // then start over with DISCOVER
	static constexpr uint64_t ARP_RETRY_TIME        = 1000;
	static constexpr uint64_t PENDING_LIFETIME      = 5000;
	static constexpr uint64_t NEIGHBOUR_LIFETIME    = 60000;
	static constexpr size_t MAX_PENDING             = 8;
	static constexpr size_t MAX_NEIGHBOURS          = 32;

	enum dhcp_phase_t
	{
		DISCOVER,
		REQUEST,
		BOUND,
		RENEW
	};

	struct neighbour
	{
		mac_address mac;
		uint64_t expires;
	};

	// A datagram waiting for its next hop's MAC.
	struct pending
	{
		uint32_t next;
		uint64_t expires;
		uint64_t retry;
		packet data;
	};

	dhcp_phase_t m_phase = DISCOVER;
	std::map<uint32_t, neighbour> m_neighbours;
	std::deque<pending> m_pending;
	unsigned m_attempts = 0;
	uint32_t m_xid = 0x57545601;
	uint32_t m_offer = 0;
	uint32_t m_server = 0;
	uint64_t m_now = 0;
	uint64_t m_deadline = 0;
	uint64_t m_expiry = 0;
	uint64_t m_renew = 0;

	static mac_address broadcast()
	{
		return { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	}

	uint32_t subnet_broadcast() const
	{
		return config.ip | ~config.mask;
	}

	void emit(uint16_t type, const mac_address &dest, const packet &payload)
	{
		packet frame(std::max(ETHERNET_MIN_FRAME_SIZE, ETHERNET_HEADER_SIZE + payload.size()), 0);

		std::copy(dest.begin(), dest.end(), frame.begin());

		std::copy(mac.begin(), mac.end(), frame.begin() + 6);

		put16(&frame[12], type);

		std::copy(payload.begin(), payload.end(), frame.begin() + ETHERNET_HEADER_SIZE);

		if (transmit)
			transmit(frame); // the provider adds the FCS, just as for MSNTV2
	}

	void arp(uint16_t op, const mac_address &dest, uint32_t target)
	{
		packet body(ARP_SIZE, 0);

		put16(&body[0], ARP_HTYPE_ETHERNET);
		put16(&body[2], ETHERTYPE_IPV4);

		body[4] = 6; // hardware address size
		body[5] = 4; // protocol address size

		put16(&body[6], op);

		std::copy(mac.begin(), mac.end(), body.begin() + 8);

		put32(&body[14], config.ip);

		if (op == ARP_REPLY)
			std::copy(dest.begin(), dest.end(), body.begin() + 18);

		put32(&body[24], target);

		emit(ETHERTYPE_ARP, dest, body);
	}

	void dhcp(uint8_t type)
	{
		packet body(DHCP_FIXED_SIZE, 0);

		body[0] = 1; // BOOTREQUEST
		body[1] = 1; // Ethernet
		body[2] = 6; // hardware address size

		put32(&body[4], m_xid);
		put16(&body[10], 0x8000); // ask for broadcast replies

		if (m_phase == RENEW)
			put32(&body[12], config.ip);

		std::copy(mac.begin(), mac.end(), body.begin() + 28);

		put32(&body[236], DHCP_MAGIC_COOKIE);

		body.insert(body.end(), { DHCP_OPTION_MESSAGE_TYPE, 1, type, DHCP_OPTION_CLIENT_ID, 7, 1 });
		body.insert(body.end(), mac.begin(), mac.end());

		if (m_phase == REQUEST)
		{
			body.insert(body.end(), { DHCP_OPTION_REQUESTED_IP, 4, 0, 0, 0, 0, DHCP_OPTION_SERVER_ID, 4, 0, 0, 0, 0 });
			put32(&body[body.size() - 10], m_offer);
			put32(&body[body.size() - 4], m_server);
		}

		body.insert(
			body.end(),
			{
				DHCP_OPTION_PARAMETERS,
				4,
				DHCP_OPTION_SUBNET_MASK,
				DHCP_OPTION_ROUTER,
				DHCP_OPTION_DNS,
				DHCP_OPTION_LEASE_TIME,
				DHCP_OPTION_MAX_SIZE,
				2,
				MTU >> 8,
				MTU & 0xff,
				DHCP_OPTION_END
			}
		);

		packet ip(IPV4_HEADER_SIZE + UDP_HEADER_SIZE + body.size(), 0);
		ip[0] = 0x45; // version 4, 20-byte header
		put16(&ip[2], ip.size());
		ip[8] = 64; // TTL
		ip[9] = IPV4_PROTOCOL_UDP;
		put32(&ip[12], m_phase == RENEW ? config.ip : 0);
		put32(&ip[16], IPV4_BROADCAST);
		put16(&ip[10], checksum(ip.data(), IPV4_HEADER_SIZE));
		put16(&ip[20], DHCP_CLIENT_PORT);
		put16(&ip[22], DHCP_SERVER_PORT);
		put16(&ip[24], UDP_HEADER_SIZE + body.size());

		std::copy(body.begin(), body.end(), ip.begin() + IPV4_HEADER_SIZE + UDP_HEADER_SIZE);

		emit(ETHERTYPE_IPV4, broadcast(), ip);
	}

	void dhcp_reply(const uint8_t *packet, size_t byte_count)
	{
		if (byte_count < DHCP_FIXED_SIZE)
			return;

		const bool ethernet_reply = packet[0] == 2 && packet[1] == 1 && packet[2] == 6;
		const bool transaction_matches = be32(packet + 4) == m_xid;
		const bool client_matches = std::equal(mac.begin(), mac.end(), packet + 28);
		const bool cookie_valid = be32(packet + 236) == DHCP_MAGIC_COOKIE;

		if (!ethernet_reply || !transaction_matches)
			return;

		if (!client_matches || !cookie_valid)
			return;

		uint8_t type = 0;
		uint32_t server = 0;
		uint32_t lease = DHCP_DEFAULT_LEASE;

		configuration proposed = config;

		proposed.ip = be32(packet + 16);
		if (!proposed.ip && m_phase == RENEW)
			proposed.ip = config.ip;

		for (size_t i = DHCP_FIXED_SIZE; i < byte_count; )
		{
			const uint8_t code = packet[i++];
			if (code == DHCP_OPTION_END)
				break;

			if (code == DHCP_OPTION_PAD)
				continue;

			if (i >= byte_count)
				return;

			const size_t size = packet[i++];
			if (size > byte_count - i)
				return;

			const bool addresses = size >= 4 && !(size % 4);

			if (code == DHCP_OPTION_MESSAGE_TYPE && size == 1)
				type = packet[i];

			if (code == DHCP_OPTION_SERVER_ID && size == 4)
				server = be32(packet + i);

			if (code == DHCP_OPTION_LEASE_TIME && size == 4)
				lease = be32(packet + i);

			if (code == DHCP_OPTION_SUBNET_MASK && size == 4)
				proposed.mask = be32(packet + i);

			if (code == DHCP_OPTION_ROUTER && addresses)
				proposed.gateway = be32(packet + i);

			if (code == DHCP_OPTION_DNS && addresses)
			{
				proposed.dns1 = be32(packet + i);
				proposed.dns2 = (size >= 8) ? be32(packet + i + 4) : 0;
			}
			i += size;
		}

		const bool offer_valid = proposed.ip != 0 && server != 0;
		if (type == DHCP_OFFER && m_phase == DISCOVER && offer_valid)
		{
			m_offer = proposed.ip;
			m_server = server;
			config = proposed;
			m_phase = REQUEST;
			m_attempts = 0;
			m_deadline = 0;
		}
		else if (type == DHCP_NAK && server == m_server && m_phase != BOUND)
		{
			reset();
		}
		else if (type == DHCP_ACK)
		{
			const bool renewing = m_phase == RENEW;
			const bool ack_expected = m_phase == REQUEST || renewing;
			const bool server_matches = server == m_server || (renewing && server != 0);
			const bool address_matches = proposed.ip == m_offer || (renewing && proposed.ip == config.ip);
			const bool configuration_valid = proposed.mask != 0 && proposed.dns1 != 0 && lease >= 2;
			if (!ack_expected || !server_matches || !address_matches || !configuration_valid)
				return;

			const uint32_t inverse = ~proposed.mask;
			if (inverse & (inverse + 1))
				return; // not a contiguous mask

			const bool address_changed = config.ip != proposed.ip || config.mask != proposed.mask;
			const bool gateway_changed = config.gateway != proposed.gateway;
			const bool dns_changed = config.dns1 != proposed.dns1 || config.dns2 != proposed.dns2;
			const bool configuration_changed = address_changed || gateway_changed || dns_changed;
			const bool newly_bound = !ready;

			config = proposed;
			m_server = server;
			ready = true;
			m_phase = BOUND;
			m_expiry = m_now + uint64_t(lease) * 1000;
			m_renew = m_now + uint64_t(lease) * 500; // T1 at half the lease

			if (newly_bound || configuration_changed)
			{
				epoch++;
				m_neighbours.clear();
				m_pending.clear();
			}
		}
	}
};

#endif // MAME_WEBTV_WTVNET_LINK_H
