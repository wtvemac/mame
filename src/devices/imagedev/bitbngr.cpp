// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Miodrag Milanovic
/*********************************************************************

    bitbngr.c

*********************************************************************/

#include "emu.h"
#include "bitbngr.h"

#include "softlist_dev.h"

#include <cstring>



/***************************************************************************
    IMPLEMENTATION
***************************************************************************/

DEFINE_DEVICE_TYPE(BITBANGER, bitbanger_device, "bitbanger", "Bitbanger")

/*-------------------------------------------------
    ctor
-------------------------------------------------*/

bitbanger_device::bitbanger_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, BITBANGER, tag, owner, clock),
	device_image_interface(mconfig, *this),
	m_mounted_cb(*this),
	m_interface(nullptr),
	m_is_readonly(false)
{
}



/*-------------------------------------------------
    native_output - outputs data to a file
-------------------------------------------------*/

void bitbanger_device::output(uint8_t data)
{
	if (!loaded_through_softlist() && exists())
		fwrite(&data, 1);
}


/*-------------------------------------------------
    native_input - inputs data from a file
-------------------------------------------------*/

uint32_t bitbanger_device::input(void *buffer, uint32_t length)
{
	if (exists())
		return fread(buffer, length);
	else
		return 0;
}



/*-------------------------------------------------
    host serial port control
-------------------------------------------------*/

util::core_file *bitbanger_device::serial_file()
{
	if (!is_open() || loaded_through_softlist())
		return nullptr;

	return &image_core_file();
}

std::error_condition bitbanger_device::set_lines(u32 mask, u32 value)
{
	util::core_file *const file = serial_file();
	return file ? file->set_serial_lines(mask, value) : std::errc::bad_file_descriptor;
}

std::error_condition bitbanger_device::get_lines(u32 &lines)
{
	util::core_file *const file = serial_file();
	return file ? file->get_serial_lines(lines) : std::errc::bad_file_descriptor;
}

std::error_condition bitbanger_device::set_format(std::string_view options)
{
	util::core_file *const file = serial_file();
	return file ? file->set_serial_format(options) : std::errc::bad_file_descriptor;
}

std::error_condition bitbanger_device::purge()
{
	util::core_file *const file = serial_file();
	return file ? file->purge_serial() : std::errc::bad_file_descriptor;
}



/*-------------------------------------------------
    device_start
-------------------------------------------------*/

void bitbanger_device::device_start()
{
}



/*-------------------------------------------------
    get_software_list_loader
-------------------------------------------------*/

const software_list_loader &bitbanger_device::get_software_list_loader() const
{
	return image_software_list_loader::instance();
}



/*-------------------------------------------------
    call_load
-------------------------------------------------*/

std::pair<std::error_condition, std::string> bitbanger_device::call_load()
{
	if (m_stream_mode)
	{
		if (std::strncmp(filename(), "socket.", 7) && std::strncmp(filename(), "domain.", 7))
			return { image_error::UNSUPPORTED, "Stream mode requires a socket. or domain. connection" };

		uint64_t size;
		auto err = osd_file::open(filename(), OPEN_FLAG_READ | OPEN_FLAG_WRITE | OPEN_FLAG_NONBLOCK, m_stream, size);
		if (err)
			return { err, err.message() };
	}

	m_mounted_cb(1);
	return std::make_pair(std::error_condition(), std::string());
}

std::pair<std::error_condition, std::string> bitbanger_device::call_create(int format_type, util::option_resolution *format_options)
{
	if (m_stream_mode)
	{
		return { image_error::UNSUPPORTED, "Stream mode connects to an existing endpoint" };
	}
	else
	{
		m_mounted_cb(1);
		return std::make_pair(std::error_condition(), std::string());
	}
}

/*-------------------------------------------------
    call_unload
-------------------------------------------------*/

void bitbanger_device::call_unload()
{
	m_mounted_cb(0);
	m_stream.reset();
}

std::error_condition bitbanger_device::stream_wait(bool writing, std::chrono::milliseconds timeout)
{
	if (m_stream)
		return m_stream->wait(writing, timeout);
	else
		return std::errc::not_connected;
}

std::error_condition bitbanger_device::stream_read(void *buffer, uint32_t length, uint32_t &actual)
{
	actual = 0;

	if (m_stream)
		return m_stream->read(buffer, 0, length, actual);
	else
		return std::make_error_condition(std::errc::not_connected);
}

std::error_condition bitbanger_device::stream_write(void const *buffer, uint32_t length, uint32_t &actual)
{
	actual = 0;

	if (m_stream)
		return m_stream->write(buffer, 0, length, actual);
	else
		return std::make_error_condition(std::errc::not_connected);
}
