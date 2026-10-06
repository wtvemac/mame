// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "emuopts.h"
#include "softlist_dev.h"

#include "lirc.h"

DEFINE_DEVICE_TYPE(LIRC, lirc_device, "lirc", "LIRC IR Device")

/*-------------------------------------------------
    ctor
-------------------------------------------------*/

lirc_device::lirc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, LIRC, tag, owner, clock),
	device_image_interface(mconfig, *this),
	m_interface(nullptr)
{
}

void lirc_device::device_start()
{
	//
}

const software_list_loader &lirc_device::get_software_list_loader() const
{
	return image_software_list_loader::instance();
}

std::pair<std::error_condition, std::string> lirc_device::call_load()
{
#if defined(__linux__)
	util::core_file &file= image_core_file();
	int fd = file.fileno();
	if (fd > 0)
	{
		// Set LIRC data receive mode to MODE2 (raw pulse and space readings)
		uint32_t mode = LIRC_MODE_MODE2;
		ioctl(fd, LIRC_SET_REC_MODE, &mode);

		// Set to non-blocking so we don't freeze MAME when there's no data to read
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags != -1)
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
#endif

	return std::make_pair(std::error_condition(), std::string());
}

std::pair<std::error_condition, std::string> lirc_device::call_create(int format_type, util::option_resolution *format_options)
{
	return std::make_pair(std::error_condition(), std::string());
}

void lirc_device::call_unload()
{
	//
}

uint32_t lirc_device::read()
{
	uint32_t data = lirc_device::LIRC_INVALID_DATA;

	if(exists())
	{
		size_t request_size = sizeof(data);
		bool valid_read = false;

#if defined(__linux__)
		util::core_file &file= image_core_file();
		int fd = file.fileno();
		if (fd > 0)
			valid_read = (::read(fd, &data, request_size) == request_size);
		else
			valid_read = (fread(&data, request_size) == request_size);
#else
		valid_read = (fread(&data, request_size) == request_size);
#endif

		if (!valid_read)
			data = lirc_device::LIRC_INVALID_DATA;
		else if (device().machine().options().lirc_inverted())
			data ^= lirc_device::LIRC_IS_PULSE_BIT;
	}

	return data;
}
