// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "winfile.h"
#include "serialopts.h"

#include "strconv.h"
#include "winutil.h"

#include <cassert>
#include <memory>
#include <utility>

#include <windows.h>


namespace {

class win_osd_serial : public osd_file
{
public:
	win_osd_serial(win_osd_serial const &) = delete;
	win_osd_serial(win_osd_serial &&) = delete;
	win_osd_serial& operator=(win_osd_serial const &) = delete;
	win_osd_serial& operator=(win_osd_serial &&) = delete;

	win_osd_serial(HANDLE handle, osd_serial_options &&options) noexcept : m_handle(handle), m_options(std::move(options))
	{
		assert(m_handle);
		assert(INVALID_HANDLE_VALUE != m_handle);
	}

	~win_osd_serial()
	{
		CloseHandle(m_handle);
	}

	// apply the options the port was opened with
	std::error_condition configure() noexcept
	{
		return configure_port(m_options);
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

	virtual std::error_condition set_serial_lines(std::uint32_t mask, std::uint32_t value) noexcept override
	{
		if (mask & ~(SERIAL_LINE_DTR | SERIAL_LINE_RTS))
			return std::errc::invalid_argument;

		// the driver controls RTS with hardware flow control
		if ((mask & SERIAL_LINE_RTS) && m_options.rtscts)
			return std::errc::operation_not_permitted;

		if (mask & SERIAL_LINE_DTR)
		{
			if (!EscapeCommFunction(m_handle, (value & SERIAL_LINE_DTR) ? SETDTR : CLRDTR))
				return win_error_to_error_condition(GetLastError());

			m_outputs = (m_outputs & ~SERIAL_LINE_DTR) | (value & SERIAL_LINE_DTR);
		}

		if (mask & SERIAL_LINE_RTS)
		{
			if (!EscapeCommFunction(m_handle, (value & SERIAL_LINE_RTS) ? SETRTS : CLRRTS))
				return win_error_to_error_condition(GetLastError());

			m_outputs = (m_outputs & ~SERIAL_LINE_RTS) | (value & SERIAL_LINE_RTS);
		}

		return std::error_condition();
	}

	virtual std::error_condition get_serial_lines(std::uint32_t &lines) noexcept override
	{
		DWORD status;
		if (!GetCommModemStatus(m_handle, &status))
			return win_error_to_error_condition(GetLastError());

		// Windows can't read back DTR/RTS so set what was last known (status)
		lines = m_outputs;
		if (status & MS_CTS_ON)
			lines |= SERIAL_LINE_CTS;
		if (status & MS_DSR_ON)
			lines |= SERIAL_LINE_DSR;
		if (status & MS_RLSD_ON)
			lines |= SERIAL_LINE_DCD;
		if (status & MS_RING_ON)
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

			if (std::error_condition const err = configure_port(keep_outputs(options)))
			{
				// put the old format back, the port may be half configured
				configure_port(keep_outputs(m_options));

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
		if (!PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT))
			return win_error_to_error_condition(GetLastError());

		return std::error_condition();
	}

private:
	static std::uint32_t initial_outputs(osd_serial_options const &options) noexcept
	{
		// configure_port asserts both lines unless told otherwise
		std::uint32_t outputs = 0;
		if (options.dtr.value_or(true))
			outputs |= SERIAL_LINE_DTR;
		if (options.rts.value_or(true))
			outputs |= SERIAL_LINE_RTS;

		return outputs;
	}

	// SetCommState also sets DTR/RTS, so format changes carry the current state
	osd_serial_options keep_outputs(osd_serial_options const &options) const
	{
		osd_serial_options result = options;
		result.dtr = bool(m_outputs & SERIAL_LINE_DTR);
		if (result.rtscts)
			result.rts.reset();
		else
			result.rts = bool(m_outputs & SERIAL_LINE_RTS);

		return result;
	}

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
		dcb.fDtrControl = options.dtr.value_or(true) ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
		dcb.fRtsControl = options.rtscts ? RTS_CONTROL_HANDSHAKE : options.rts.value_or(true) ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
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

		return std::error_condition();
	}

	HANDLE m_handle;
	osd_serial_options m_options;
	std::uint32_t m_outputs = initial_outputs(m_options);
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

	std::unique_ptr<win_osd_serial> result(new (std::nothrow) win_osd_serial(handle, std::move(options)));
	if (!result)
	{
		CloseHandle(handle);
		return std::errc::not_enough_memory;
	}

	// on failure the destructor closes the port
	std::error_condition err = result->configure();
	if (!err)
		err = result->purge_serial();
	if (err)
		return err;

	file = std::move(result);
	filesize = 0;

	return std::error_condition();
}
