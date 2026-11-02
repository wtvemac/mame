// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "util/hashing.h"

#include "osdcore.h"

#include "modules/lib/osdobj_common.h"
#include "modules/osdmodule.h"
#include "netdev_common.h"
#include "netdev_module.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace osd
{

	namespace
	{

#if defined(_WIN32)
		typedef SOCKET socket_t;
		static constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
		static void close_socket(socket_t s)
		{
			closesocket(s);
		}
		static bool would_block()
		{
			int const err = WSAGetLastError();
			return (err == WSAEWOULDBLOCK) || (err == WSAEINPROGRESS);
		}
		static bool set_nonblocking(socket_t s)
		{
			u_long mode = 1;
			return ioctlsocket(s, FIONBIO, &mode) == 0;
		}
#else
		typedef int socket_t;
		static constexpr socket_t INVALID_SOCKET_VALUE = -1;
		static void close_socket(socket_t s)
		{
			close(s);
		}
		static bool would_block()
		{
			return (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS) || (errno == EINTR);
		}
		static bool set_nonblocking(socket_t s)
		{
			int const flags = fcntl(s, F_GETFL, 0);
			return (flags != -1) && (fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1);
		}
#endif

		class socket_module : public osd_module, public netdev_module
		{

		public:
			static constexpr int MIN_FRAME_BYTECNT = 64; // Minimum Ethernet frame size (from 802.3)
			static constexpr uint32_t MAX_RX_FRAME_BYTECNT = 2048 - 4; // RX buffer minus 4-byte FCS
			static constexpr int HEADER_BYTECNT = 4;
			static constexpr size_t MAX_TX_QUEUE = 256 * 1024;
			static constexpr int RECONNECT_MS = 1000;

			socket_module() :
				osd_module(OSD_NETDEV_PROVIDER, "socket"),
				netdev_module()
			{
			}
			virtual ~socket_module()
			{
			}

			virtual int init(osd_interface &osd, const osd_options &options) override;
			virtual void exit() override;

			virtual bool probe() override
			{
				return true;
			}

			virtual std::unique_ptr<network_device> open_device(int id, network_handler &handler) override;
			virtual std::vector<network_device_info> list_devices() override;

		private:
			std::string m_endpoint;
			std::string m_description;
			bool m_winsock_started = false;
		};


		class netdev_socket : public network_device_base
		{
		public:
			netdev_socket(std::string const &endpoint, network_handler &handler);
			~netdev_socket();

			int send(void const *buf, int len) override;

		protected:
			int recv_dev(uint8_t **buf) override;

		private:
			enum class state
			{
				IDLE,
				CONNECTING,
				CONNECTED
			};

			void resolve();
			void start_connect();
			void check_connect();
			void disconnect();
			void flush_tx();
			bool fill_rx();

			std::string m_host;
			std::string m_port;
			std::vector<sockaddr_storage> m_addrs;
			std::vector<socklen_t> m_addr_lens;
			size_t m_addr_index = 0;

			socket_t m_socket = INVALID_SOCKET_VALUE;
			state m_state = state::IDLE;
			osd_ticks_t m_next_attempt = 0;

			std::vector<uint8_t> m_tx;
			std::vector<uint8_t> m_rx;
			size_t m_rx_len = 0;
			uint8_t m_frame[2048];
		};


		static uint32_t finalise_frame(uint8_t buf[], uint32_t length)
		{
			// From taptun:
			/*
			 * The taptun driver receives frames which are shorter than the Ethernet
			 * minimum. Partly this is because it can't see the frame check sequence
			 * bytes, but mainly it's because the OS expects the lower level device
			 * to add the required padding.
			 *
			 * We do the equivalent padding here (i.e. pad with zeroes to the
			 * minimum Ethernet length minus FCS), so that devices which check
			 * for this will not reject these packets.
			 */

			if (length < socket_module::MIN_FRAME_BYTECNT - 4)
			{
				std::fill_n(&buf[length], socket_module::MIN_FRAME_BYTECNT - length - 4, 0);

				length = socket_module::MIN_FRAME_BYTECNT - 4;
			}

			const uint32_t fcs = util::crc32_creator::simple(buf, length);

			buf[length++] = (fcs >> 0) & 0xff;
			buf[length++] = (fcs >> 8) & 0xff;
			buf[length++] = (fcs >> 16) & 0xff;
			buf[length++] = (fcs >> 24) & 0xff;

			return length;
		}

		netdev_socket::netdev_socket(std::string const &endpoint, network_handler &handler) :
			network_device_base(handler),
			m_rx(socket_module::HEADER_BYTECNT + socket_module::MAX_RX_FRAME_BYTECNT)
		{
			std::string::size_type const colon = endpoint.rfind(':');

			if (colon == std::string::npos)
			{
				osd_printf_error("netdev_socket: invalid endpoint '%s', expected host:port\n", endpoint);
			}
			else
			{
				m_host = endpoint.substr(0, colon);
				m_port = endpoint.substr(colon + 1);

				if ((m_host.size() >= 2) && (m_host.front() == '[') && (m_host.back() == ']'))
					m_host = m_host.substr(1, m_host.size() - 2);

				if (m_host.empty())
					m_host = "127.0.0.1";

				resolve();
				start_connect();
			}
		}

		netdev_socket::~netdev_socket()
		{
			disconnect();
		}

		void netdev_socket::resolve()
		{
			m_addrs.clear();
			m_addr_lens.clear();
			m_addr_index = 0;

			addrinfo hints;
			std::memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			addrinfo *result = nullptr;
			if (getaddrinfo(m_host.c_str(), m_port.c_str(), &hints, &result) != 0)
			{
				osd_printf_error("netdev_socket: unable to resolve %s:%s\n", m_host, m_port);
			}
			else
			{
				for (addrinfo *ai = result; ai; ai = ai->ai_next)
				{
					sockaddr_storage addr;
					std::memset(&addr, 0, sizeof(addr));
					std::memcpy(&addr, ai->ai_addr, ai->ai_addrlen);
					m_addrs.push_back(addr);
					m_addr_lens.push_back(socklen_t(ai->ai_addrlen));
				}

				freeaddrinfo(result);
			}
		}

		void netdev_socket::start_connect()
		{
			m_next_attempt = osd_ticks() + (osd_ticks_per_second() * socket_module::RECONNECT_MS / 1000);

			if (m_addrs.empty())
				return;

			sockaddr_storage const &addr = m_addrs[m_addr_index];
			socklen_t const addr_len = m_addr_lens[m_addr_index];
			m_addr_index = (m_addr_index + 1) % m_addrs.size();

			m_socket = ::socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
			if (m_socket == INVALID_SOCKET_VALUE)
				return;

			if (!set_nonblocking(m_socket))
			{
				disconnect();
				return;
			}

			int const nodelay = 1;
			setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char const *>(&nodelay), sizeof(nodelay));

			if (::connect(m_socket, reinterpret_cast<sockaddr const *>(&addr), addr_len) == 0)
			{
				osd_printf_verbose("netdev_socket: connected to %s:%s\n", m_host, m_port);
				m_state = state::CONNECTED;
			}
			else if (would_block())
			{
				m_state = state::CONNECTING;
			}
			else
			{
				disconnect();
			}
		}

		void netdev_socket::check_connect()
		{
			fd_set wfds, efds;
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			FD_SET(m_socket, &wfds);
			FD_SET(m_socket, &efds);

			timeval tv = { 0, 0 };
			int const ready = select(int(m_socket + 1), nullptr, &wfds, &efds, &tv);
			if (ready == 0)
			{
				if (osd_ticks() >= m_next_attempt)
					disconnect();
			}
			else
			{
				int err = 0;
				socklen_t err_len = sizeof(err);
				if ((ready < 0) || getsockopt(m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &err_len) != 0 || err != 0)
				{
					disconnect();
					return;
				}

				osd_printf_verbose("netdev_socket: connected to %s:%s\n", m_host, m_port);
				m_state = state::CONNECTED;
			}
		}

		void netdev_socket::disconnect()
		{
			if (m_state == state::CONNECTED)
				osd_printf_verbose("netdev_socket: disconnected from %s:%s\n", m_host, m_port);

			if (m_socket != INVALID_SOCKET_VALUE)
				close_socket(m_socket);

			m_socket = INVALID_SOCKET_VALUE;
			m_state = state::IDLE;
			m_tx.clear();
			m_rx_len = 0;
		}

		void netdev_socket::flush_tx()
		{
			while (!m_tx.empty())
			{
				int const sent = ::send(m_socket, reinterpret_cast<char const *>(m_tx.data()), int(m_tx.size()), 0);
				if (sent > 0)
				{
					m_tx.erase(m_tx.begin(), m_tx.begin() + sent);
				}
				else
				{
					if ((sent < 0) && would_block())
						return;

					disconnect();
				}
			}
		}

		bool netdev_socket::fill_rx()
		{
			for (;;)
			{
				size_t want = socket_module::HEADER_BYTECNT;
				if (m_rx_len >= socket_module::HEADER_BYTECNT)
				{
					const uint32_t frame_len = (uint32_t(m_rx[0]) << 24) | (uint32_t(m_rx[1]) << 16) | (uint32_t(m_rx[2]) << 8) | uint32_t(m_rx[3]);
					if (frame_len > socket_module::MAX_RX_FRAME_BYTECNT)
					{
						osd_printf_error("netdev_socket: peer sent oversized frame (%u bytes), dropping connection\n", frame_len);
						disconnect();

						return false;
					}
					want += frame_len;
				}

				if (m_rx_len >= want)
				{
					if (want > socket_module::HEADER_BYTECNT)
						return true;

					m_rx_len = 0;
					continue;
				}

				int const got = ::recv(m_socket, reinterpret_cast<char *>(&m_rx[m_rx_len]), int(want - m_rx_len), 0);
				if (got > 0)
				{
					m_rx_len += got;
				}
				else
				{
					if ((got < 0) && would_block())
						return false;

					disconnect();

					return false;
				}
			}
		}

		int netdev_socket::send(void const *buf, int len)
		{
			if ((m_state != state::CONNECTED) || (len <= 0))
				return 0;

			if ((m_tx.size() + socket_module::HEADER_BYTECNT + len) > socket_module::MAX_TX_QUEUE)
			{
				osd_printf_verbose("netdev_socket: transmit queue full, dropping frame\n");
				return 0;
			}

			const uint32_t frame_len = uint32_t(len);
			const uint8_t header[socket_module::HEADER_BYTECNT] = { uint8_t(frame_len >> 24), uint8_t(frame_len >> 16), uint8_t(frame_len >> 8), uint8_t(frame_len) };
			m_tx.insert(m_tx.end(), std::begin(header), std::end(header));
			m_tx.insert(m_tx.end(), reinterpret_cast<const uint8_t *>(buf), reinterpret_cast<const uint8_t *>(buf) + len);

			flush_tx();

			return len;
		}

		int netdev_socket::recv_dev(uint8_t **buf)
		{
			switch (m_state)
			{
				case state::IDLE:
					if (osd_ticks() >= m_next_attempt)
						start_connect();
					return 0;

				case state::CONNECTING:
					check_connect();
					return 0;

				case state::CONNECTED:
					break;
			}

			flush_tx();

			if ((m_state != state::CONNECTED) || !fill_rx())
				return 0;

			const uint32_t frame_len = uint32_t(m_rx_len - socket_module::HEADER_BYTECNT);
			std::copy_n(&m_rx[socket_module::HEADER_BYTECNT], frame_len, m_frame);
			m_rx_len = 0;

			*buf = m_frame;

			return finalise_frame(m_frame, frame_len);
		}


		int socket_module::init(osd_interface &osd, const osd_options &options)
		{
#if defined(_WIN32)
			WSADATA wsa_data;
			if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
			{
				osd_printf_error("netdev_socket: WSAStartup failed\n");
				return -1;
			}
			m_winsock_started = true;
#endif

			m_endpoint = options.network_socket();
			m_description = "Socket (" + m_endpoint + ")";

			return 0;
		}

		void socket_module::exit()
		{
#if defined(_WIN32)
			if (m_winsock_started)
				WSACleanup();
#endif

			m_winsock_started = false;
		}

		std::unique_ptr<network_device> socket_module::open_device(int id, network_handler &handler)
		{
			if (id != 0)
				return nullptr;

			return std::make_unique<netdev_socket>(m_endpoint, handler);
		}

		std::vector<network_device_info> socket_module::list_devices()
		{
			std::vector<network_device_info> result;
			result.emplace_back(network_device_info{ 0, m_description });
			return result;
		}

	} // anonymous namespace

} // namespace osd


MODULE_DEFINITION(NETDEV_SOCKET, osd::socket_module)
