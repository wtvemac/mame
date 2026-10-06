// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_IMAGEDEV_LIRC_H
#define MAME_IMAGEDEV_LIRC_H

#pragma once

#if defined(__linux__)
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

class lirc_device : public device_t, public device_image_interface
{
public:

	static constexpr uint32_t LIRC_IS_PULSE_BIT  = 0x01000000;
	static constexpr uint32_t LIRC_INTERVAL_MASK = 0x00ffffff;
	static constexpr uint32_t LIRC_INVALID_DATA  = 0x00000000;

	void set_interface(const char *interface) { m_interface = interface; }

	lirc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual std::pair<std::error_condition, std::string> call_load() override;
	virtual std::pair<std::error_condition, std::string> call_create(int format_type, util::option_resolution *format_options) override;
	virtual void call_unload() override;

	virtual bool is_readable()  const noexcept override { return true; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual bool is_writeable() const noexcept override { return false; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool support_command_line_image_creation() const noexcept override { return false; }

	virtual const char *image_interface() const noexcept override { return m_interface; }
	virtual const char *file_extensions() const noexcept override { return ""; }
	virtual const char *image_type_name() const noexcept override { return "lirc"; }
	virtual const char *image_brief_type_name() const noexcept override { return "lirc"; }

	bool is_enabled() const { return exists(); }

	uint32_t read();

protected:

	virtual void device_start() override ATTR_COLD;

	virtual software_list_loader const &get_software_list_loader() const override;

private:

#if defined(__linux__)
	// This is properly defined in linux/lirc.h
	// I'm bypassing that so it's not required to download the lirc headers just for this.
	static constexpr uint32_t LIRC_SET_REC_MODE  = _IOW('i', 0x00000012, uint32_t);
	static constexpr uint32_t LIRC_MODE_MODE2    = 0x00000004;
#endif

	char const *m_interface;
};


DECLARE_DEVICE_TYPE(LIRC, lirc_device)

#endif // MAME_IMAGEDEV_LIRC_H
