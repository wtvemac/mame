// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

//============================================================
//
//  host serial port path parsing similar to socat:
//
//    serial./dev/ttyUSB0,b115200,cs8,parenb,cstopb
//    serial.COM3,b115200,crtscts
//
//  Options (comma separated and any order):
//    b<rate>               baud rate, any value the driver accepts
//    cs5 | cs6 | cs7 | cs8 data bits
//    parenb[=0|1]          enable parity (even unless parodd)
//    parodd[=0|1]          odd parity (only with parenb)
//    cstopb[=0|1]          two stop bits
//    crtscts[=0|1]         RTS/CTS hardware flow control
//    ixon[=0|1]            XON/XOFF flow control on output
//    ixoff[=0|1]           XON/XOFF flow control on input
//
//  Defaults to 9600 baud, 8 data bits, no parity, 1 stop bit and no flow control.
//
//============================================================

#ifndef MAME_OSD_MODULES_FILE_SERIALOPTS_H
#define MAME_OSD_MODULES_FILE_SERIALOPTS_H

#pragma once

#include <cstdint>
#include <string>
#include <string_view>


struct osd_serial_options
{
	static constexpr std::string_view IDENTIFIER = "serial.";

	std::string device;
	std::uint32_t baud = 9600;
	std::uint8_t data_bits = 8;
	bool parity = false;
	bool parity_odd = false;
	bool two_stop_bits = false;
	bool rtscts = false;
	bool xon_output = false;
	bool xon_input = false;

	static bool is_serial_path(std::string_view path) noexcept
	{
		return path.substr(0, IDENTIFIER.size()) == IDENTIFIER;
	}

	// returns false on an empty device name or an unknown/malformed option
	bool parse(std::string_view path)
	{
		if (!is_serial_path(path))
			return false;

		std::string_view rest = path.substr(IDENTIFIER.size());
		auto const comma = rest.find(',');
		device.assign(rest.substr(0, comma));
		if (device.empty())
			return false;

		rest = (comma == std::string_view::npos) ? std::string_view() : rest.substr(comma + 1);
		while (!rest.empty())
		{
			auto const next = rest.find(',');
			std::string_view const option = rest.substr(0, next);
			rest = (next == std::string_view::npos) ? std::string_view() : rest.substr(next + 1);

			if (!option.empty() && !parse_option(option))
				return false;
		}

		return !parity_odd || parity;
	}

private:
	static bool parse_number(std::string_view text, std::uint32_t &value) noexcept
	{
		if (text.empty() || text.size() > 9)
			return false;

		value = 0;
		for (char const c : text)
		{
			if (c < '0' || c > '9')
				return false;

			value = value * 10 + std::uint32_t(c - '0');
		}

		return true;
	}

	// "name" or "name=0" / "name=1"; returns true if the option was this flag
	static bool parse_flag(std::string_view option, std::string_view name, bool &value) noexcept
	{
		if (option.substr(0, name.size()) != name)
			return false;

		std::string_view const tail = option.substr(name.size());
		if (tail.empty() || tail == "=1")
		{
			value = true;

			return true;
		}
		else if (tail == "=0")
		{
			value = false;

			return true;
		}
		else
		{
			return false;
		}
	}

	bool parse_option(std::string_view option)
	{
		// These are ignored (the port is always raw and without echo).
		// Here just so there's no error if someone copies and pastes socat options
		if (option == "raw" || option == "rawer" || option == "echo=0")
			return true;

		if (option.size() == 3 && option.substr(0, 2) == "cs" && option[2] >= '5' && option[2] <= '8')
		{
			data_bits = std::uint8_t(option[2] - '0');
			return true;
		}

		if (option[0] == 'b')
		{
			std::uint32_t rate;
			if (!parse_number(option.substr(1), rate) || !rate)
				return false;

			baud = rate;

			return true;
		}
		else
		{
			return parse_flag(option, "parenb", parity)
					|| parse_flag(option, "parodd", parity_odd)
					|| parse_flag(option, "cstopb", two_stop_bits)
					|| parse_flag(option, "crtscts", rtscts)
					|| parse_flag(option, "ixon", xon_output)
					|| parse_flag(option, "ixoff", xon_input);
		}
	}
};

#endif // MAME_OSD_MODULES_FILE_SERIALOPTS_H
