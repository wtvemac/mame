// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "winfile.h"
#include "serialopts.h"

#include "strconv.h"
#include "winutil.h"

#include <cassert>

#include <windows.h>


namespace {

class win_osd_serial : public osd_file
{
public:
	win_osd_serial(win_osd_serial const &) = delete;
	win_osd_serial(win_osd_serial &&) = delete;
	win_osd_serial& operator=(win_osd_serial const &) = delete;
	win_osd_serial& operator=(win_osd_serial &&) = delete;

	win_osd_serial(HANDLE handle) noexcept : m_handle(handle)
	{
		assert(m_handle);
		assert(INVALID_HANDLE_VALUE != m_handle);
	}

	~win_osd_serial()
	{
		CloseHandle(m_handle);
	}

	virtual std::error_condition read(void *buffer, std::uint64_t offset, std::uint32_t count, std::uint32_t &actual) noexcept override
	{
		DWORD bytes_read;
		if (!ReadFile(m_handle, buffer, count, &bytes_read, nullptr))
		{
			actual = 0;
			return win_error_to_error_condition(GetLastError());
		}

		actual = bytes_read;
		return std::error_condition();
	}

	virtual std::error_condition write(void const *buffer, std::uint64_t offset, std::uint32_t count, std::uint32_t &actual) noexcept override
	{
		DWORD bytes_written;
		if (!WriteFile(m_handle, buffer, count, &bytes_written, nullptr))
			return win_error_to_error_condition(GetLastError());

		actual = bytes_written;
		return std::error_condition();
	}

	virtual std::error_condition truncate(std::uint64_t offset) noexcept override
	{
		// doesn't make sense on a serial port
		return std::errc::bad_file_descriptor;
	}

	virtual std::error_condition flush() noexcept override
	{
		// FlushFileBuffers would wait for the UART to drain
		return std::error_condition();
	}

private:
	std::error_condition configure_port(osd_serial_options const &options) noexcept
	{
		DCB dcb;
		ZeroMemory(&dcb, sizeof(dcb));
		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState(m_handle, &dcb))
			return win_error_to_error_condition(GetLastError());

		dcb.BaudRate = options.baud;
		dcb.ByteSize = options.data_bits;
		dcb.fBinary = TRUE;
		dcb.fParity = options.parity ? TRUE : FALSE;
		dcb.Parity = !options.parity ? NOPARITY : options.parity_odd ? ODDPARITY : EVENPARITY;
		dcb.StopBits = options.two_stop_bits ? TWOSTOPBITS : ONESTOPBIT;
		dcb.fOutxCtsFlow = options.rtscts ? TRUE : FALSE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fRtsControl = options.rtscts ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
		dcb.fDsrSensitivity = FALSE;
		dcb.fOutX = options.xon_output ? TRUE : FALSE;
		dcb.fInX = options.xon_input ? TRUE : FALSE;
		dcb.fErrorChar = FALSE;
		dcb.fNull = FALSE;
		dcb.fAbortOnError = FALSE;

		if (!SetCommState(m_handle, &dcb))
			return win_error_to_error_condition(GetLastError());

		// reads return immediately with whatever has arrived
		COMMTIMEOUTS timeouts;
		ZeroMemory(&timeouts, sizeof(timeouts));
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 100;
		if (!SetCommTimeouts(m_handle, &timeouts))
			return win_error_to_error_condition(GetLastError());

		PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

		return std::error_condition();
	}

	HANDLE m_handle;
};

} // anonymous namespace


bool win_check_serial_path(std::string const &path) noexcept
{
	return osd_serial_options::is_serial_path(path);
}


std::error_condition win_open_serial(std::string const &path, std::uint32_t openflags, osd_file::ptr &file, std::uint64_t &filesize) noexcept
{
	osd_serial_options options;
	osd::text::tstring t_name;
	try
	{
		if (!options.parse(path))
			return std::errc::invalid_argument;

		// COM10 and up only open through the device namespace
		std::string device = options.device;
		if (device.rfind("\\\\.\\", 0) != 0)
			device.insert(0, "\\\\.\\");
		t_name = osd::text::to_tstring(device);
	}
	catch (...)
	{
		return std::errc::not_enough_memory;
	}

	DWORD access = 0;
	if (openflags & OPEN_FLAG_READ)
		access |= GENERIC_READ;
	if (openflags & OPEN_FLAG_WRITE)
		access |= GENERIC_WRITE;

	HANDLE const handle = CreateFile(t_name.c_str(), access, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (INVALID_HANDLE_VALUE == handle)
		return win_error_to_error_condition(GetLastError());

	if (std::error_condition const err = configure_port(handle, options))
	{
		CloseHandle(handle);
		return err;
	}

	osd_file::ptr result(new (std::nothrow) win_osd_serial(handle));
	if (!result)
	{
		CloseHandle(handle);
		return std::errc::not_enough_memory;
	}
	else
	{
		file = std::move(result);
		filesize = 0;

		return std::error_condition();
	}
}
