// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "posixfile.h"
#include "serialopts.h"

#include <cassert>
#include <cerrno>
#include <memory>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#if defined(__linux__)
// termios2 allows any baud rate (BOTHER); <termios.h> can't be included alongside it
#include <asm/termbits.h>
#else
#include <termios.h>
#if defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif
#endif


namespace {

class posix_osd_serial : public osd_file
{
public:
	posix_osd_serial(posix_osd_serial const &) = delete;
	posix_osd_serial(posix_osd_serial &&) = delete;
	posix_osd_serial& operator=(posix_osd_serial const &) = delete;
	posix_osd_serial& operator=(posix_osd_serial &&) = delete;

	posix_osd_serial(int fd, osd_serial_options &&options) noexcept : m_fd(fd), m_options(std::move(options))
	{
		assert(m_fd >= 0);
	}

	virtual ~posix_osd_serial()
	{
		// exclusive mode outlives close() if something else holds the tty
		::ioctl(m_fd, TIOCNXCL);
		::close(m_fd);
	}

	// apply the options the port was opened with
	std::error_condition configure() noexcept
	{
		return configure_port(m_options);
	}

	virtual std::error_condition read(void *buffer, std::uint64_t offset, std::uint32_t count, std::uint32_t &actual) noexcept override
	{
		ssize_t const result = ::read(m_fd, buffer, count);
		if (result < 0)
		{
			actual = 0;
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				return std::error_condition();
			return std::error_condition(errno, std::generic_category());
		}

		actual = std::uint32_t(size_t(result));
		return std::error_condition();
	}

	virtual std::error_condition write(void const *buffer, std::uint64_t offset, std::uint32_t count, std::uint32_t &actual) noexcept override
	{
		ssize_t const result = ::write(m_fd, buffer, count);
		if (result < 0)
			return std::error_condition(errno, std::generic_category());

		actual = std::uint32_t(size_t(result));
		return std::error_condition();
	}

	virtual std::error_condition truncate(std::uint64_t offset) noexcept override
	{
		// doesn't make sense on a serial port
		return std::errc::bad_file_descriptor;
	}

	virtual std::error_condition flush() noexcept override
	{
		// no userspace buffers on read/write
		return std::error_condition();
	}

	virtual int fileno() noexcept override
	{
		return m_fd;
	}

	virtual std::error_condition set_serial_lines(std::uint32_t mask, std::uint32_t value) noexcept override
	{
		if (mask & ~(SERIAL_LINE_DTR | SERIAL_LINE_RTS))
			return std::errc::invalid_argument;

		// the driver controls RTS with hardware flow control
		if ((mask & SERIAL_LINE_RTS) && m_options.rtscts)
			return std::errc::operation_not_permitted;

		return set_lines(mask, value);
	}

	virtual std::error_condition get_serial_lines(std::uint32_t &lines) noexcept override
	{
		int status;
		if (::ioctl(m_fd, TIOCMGET, &status) < 0)
			return modem_error(errno);

		lines = 0;
		if (status & TIOCM_DTR)
			lines |= SERIAL_LINE_DTR;
		if (status & TIOCM_RTS)
			lines |= SERIAL_LINE_RTS;
		if (status & TIOCM_CTS)
			lines |= SERIAL_LINE_CTS;
		if (status & TIOCM_DSR)
			lines |= SERIAL_LINE_DSR;
		if (status & TIOCM_CD)
			lines |= SERIAL_LINE_DCD;
		if (status & TIOCM_RI)
			lines |= SERIAL_LINE_RI;

		return std::error_condition();
	}

	virtual std::error_condition set_serial_format(std::string_view format) noexcept override
	{
		try
		{
			osd_serial_options options = m_options;
			options.dtr.reset();
			options.rts.reset();
			if (!options.parse_format(format))
				return std::errc::invalid_argument;

			if (std::error_condition const err = configure_port(options))
			{
				// put the old format back, the port may be half configured
				osd_serial_options previous = m_options;
				previous.dtr.reset();
				previous.rts.reset();
				configure_port(previous);

				return err;
			}

			m_options = std::move(options);

			return std::error_condition();
		}
		catch (...)
		{
			return std::errc::not_enough_memory;
		}
	}

	virtual std::error_condition purge_serial() noexcept override
	{
#if defined(__linux__)
		if (::ioctl(m_fd, TCFLSH, TCIOFLUSH) < 0)
#else
		if (::tcflush(m_fd, TCIOFLUSH) < 0)
#endif
			return std::error_condition(errno, std::generic_category());

		return std::error_condition();
	}

private:
	// ttys without modem lines (e.g. ptys) reject the TIOCM ioctls
	static std::error_condition modem_error(int error) noexcept
	{
		if (error == ENOTTY || error == EINVAL)
			return std::errc::not_supported;

		return std::error_condition(error, std::generic_category());
	}

	std::error_condition configure_port(osd_serial_options const &options) noexcept
	{
#if defined(__linux__)
		struct termios2 tios;
		if (::ioctl(m_fd, TCGETS2, &tios) < 0)
			return std::error_condition(errno, std::generic_category());
#else
		struct termios tios;
		if (::tcgetattr(m_fd, &tios) < 0)
			return std::error_condition(errno, std::generic_category());
#endif

		// raw mode, the same as cfmakeraw()
		tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY | INPCK);
		tios.c_oflag &= ~OPOST;
		tios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		tios.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB | CRTSCTS);
		tios.c_cflag |= CREAD | CLOCAL;

		switch (options.data_bits)
		{
			case 5: tios.c_cflag |= CS5; break;
			case 6: tios.c_cflag |= CS6; break;
			case 7: tios.c_cflag |= CS7; break;
			default: tios.c_cflag |= CS8; break;
		}

		if (options.parity)
		{
			tios.c_cflag |= PARENB;
			if (options.parity_odd)
				tios.c_cflag |= PARODD;
		}

		if (options.two_stop_bits)
			tios.c_cflag |= CSTOPB;

		if (options.rtscts)
			tios.c_cflag |= CRTSCTS;

		if (options.xon_output)
			tios.c_iflag |= IXON;

		if (options.xon_input)
			tios.c_iflag |= IXOFF;

		// never block in read(); the port is also O_NONBLOCK
		tios.c_cc[VMIN] = 0;
		tios.c_cc[VTIME] = 0;

#if defined(__linux__)
		tios.c_cflag &= ~CBAUD;
		tios.c_cflag |= BOTHER;
		tios.c_ispeed = options.baud;
		tios.c_ospeed = options.baud;

		if (::ioctl(m_fd, TCSETS2, &tios) < 0)
			return std::error_condition(errno, std::generic_category());
#else
		// BSD speed_t values are the rate itself; non-standard rates are set below on macOS
		if (::cfsetspeed(&tios, speed_t(options.baud)) < 0)
		{
#if defined(__APPLE__)
			::cfsetspeed(&tios, B9600);
#else
			return std::error_condition(errno, std::generic_category());
#endif
		}

		if (::tcsetattr(m_fd, TCSANOW, &tios) < 0)
			return std::error_condition(errno, std::generic_category());

#if defined(__APPLE__)
		speed_t speed = speed_t(options.baud);
		if (::ioctl(m_fd, IOSSIOSPEED, &speed) < 0)
			return std::error_condition(errno, std::generic_category());
#endif
#endif

		std::uint32_t mask = 0, value = 0;
		if (options.dtr)
		{
			mask |= SERIAL_LINE_DTR;
			value |= *options.dtr ? SERIAL_LINE_DTR : 0;
		}
		if (options.rts)
		{
			mask |= SERIAL_LINE_RTS;
			value |= *options.rts ? SERIAL_LINE_RTS : 0;
		}

		return set_lines(mask, value);
	}

	std::error_condition set_lines(std::uint32_t mask, std::uint32_t value) noexcept
	{
		int set = 0, clear = 0;
		if (mask & SERIAL_LINE_DTR)
			((value & SERIAL_LINE_DTR) ? set : clear) |= TIOCM_DTR;
		if (mask & SERIAL_LINE_RTS)
			((value & SERIAL_LINE_RTS) ? set : clear) |= TIOCM_RTS;

		if (set && ::ioctl(m_fd, TIOCMBIS, &set) < 0)
			return modem_error(errno);
		else if (clear && ::ioctl(m_fd, TIOCMBIC, &clear) < 0)
			return modem_error(errno);
		else
			return std::error_condition();
	}

	int m_fd;
	osd_serial_options m_options;
};

} // anonymous namespace


bool posix_check_serial_path(std::string const &path) noexcept
{
	return osd_serial_options::is_serial_path(path);
}


std::error_condition posix_open_serial(std::string const &path, std::uint32_t openflags, osd_file::ptr &file, std::uint64_t &filesize) noexcept
{
	osd_serial_options options;
	try
	{
		if (!options.parse(path))
			return std::errc::invalid_argument;
	}
	catch (...)
	{
		return std::errc::not_enough_memory;
	}

	int access = O_NOCTTY | O_NONBLOCK;
	if ((openflags & OPEN_FLAG_READ) && (openflags & OPEN_FLAG_WRITE))
		access |= O_RDWR;
	else if (openflags & OPEN_FLAG_WRITE)
		access |= O_WRONLY;
	else
		access |= O_RDONLY;

	int const fd = ::open(options.device.c_str(), access);
	if (fd < 0)
		return std::error_condition(errno, std::generic_category());

	// keep other programs off the port while it's in use
	::ioctl(fd, TIOCEXCL);

	std::unique_ptr<posix_osd_serial> result(new (std::nothrow) posix_osd_serial(fd, std::move(options)));
	if (!result)
	{
		::ioctl(fd, TIOCNXCL);
		::close(fd);
		return std::errc::not_enough_memory;
	}

	// on failure the destructor releases and closes the port
	std::error_condition err = result->configure();
	if (!err)
		err = result->purge_serial();
	if (err)
		return err;

	file = std::move(result);
	filesize = 0;

	return std::error_condition();
}
