// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "posixfile.h"
#include "serialopts.h"

#include <cassert>
#include <cerrno>

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

	posix_osd_serial(int fd) noexcept : m_fd(fd)
	{
		assert(m_fd >= 0);
	}

	virtual ~posix_osd_serial()
	{
		::close(m_fd);
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

private:
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

		::ioctl(m_fd, TCFLSH, TCIOFLUSH);
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

		::tcflush(m_fd, TCIOFLUSH);
#endif

		return std::error_condition();
	}

	int m_fd;
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

	if (std::error_condition const err = configure_port(fd, options))
	{
		::close(fd);
		return err;
	}

	osd_file::ptr result(new (std::nothrow) posix_osd_serial(fd));
	if (!result)
	{
		::close(fd);
		return std::errc::not_enough_memory;
	}

	file = std::move(result);
	filesize = 0;

	return std::error_condition();
}
