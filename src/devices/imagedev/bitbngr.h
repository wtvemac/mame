// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Miodrag Milanovic
/*********************************************************************

    bitbngr.h

*********************************************************************/

#ifndef MAME_IMAGEDEV_BITBNGR_H
#define MAME_IMAGEDEV_BITBNGR_H

#pragma once

class bitbanger_device : public device_t,
	public device_image_interface
{
public:
	// modem lines for set_lines()/get_lines() on host serial ports
	static constexpr u32 LINE_DTR = SERIAL_LINE_DTR; // output
	static constexpr u32 LINE_RTS = SERIAL_LINE_RTS; // output
	static constexpr u32 LINE_CTS = SERIAL_LINE_CTS; // input
	static constexpr u32 LINE_DSR = SERIAL_LINE_DSR; // input
	static constexpr u32 LINE_DCD = SERIAL_LINE_DCD; // input
	static constexpr u32 LINE_RI  = SERIAL_LINE_RI;  // input

	void set_stream_mode(bool enabled) { m_stream_mode = enabled; }
	virtual bool core_opens_image_file() const noexcept override { return !m_stream_mode; }

	void set_interface(const char *interface) { m_interface = interface; }
	void set_readonly(bool is_readonly) { m_is_readonly = is_readonly; }

	// called with 1 after an image is mounted and 0 after it's unmounted
	auto mounted_callback() { return m_mounted_cb.bind(); }

	// construction/destruction
	bitbanger_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	// device_image_interface implementation
	virtual std::pair<std::error_condition, std::string> call_load() override;
	virtual std::pair<std::error_condition, std::string> call_create(int format_type, util::option_resolution *format_options) override;
	virtual void call_unload() override;

	virtual bool is_readable()  const noexcept override { return true; }
	virtual bool is_writeable() const noexcept override { return !m_is_readonly; }
	virtual bool is_creatable() const noexcept override { return !m_is_readonly && !m_stream_mode; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual bool support_command_line_image_creation() const noexcept override { return !m_is_readonly && !m_stream_mode; }
	virtual const char *image_interface() const noexcept override { return m_interface; }
	virtual const char *file_extensions() const noexcept override { return ""; }
	virtual const char *image_type_name() const noexcept override { return "bitbanger"; }
	virtual const char *image_brief_type_name() const noexcept override { return "bitb"; }

	std::error_condition stream_read(void *buffer, uint32_t length, uint32_t &actual);
	std::error_condition stream_write(void const *buffer, uint32_t length, uint32_t &actual);
	void output(uint8_t data);
	uint32_t input(void *buffer, uint32_t length);

	// Host serial port control.
	std::error_condition set_lines(u32 mask, u32 value);       // LINE_DTR/LINE_RTS
	std::error_condition get_lines(u32 &lines);                // all LINE_* that are asserted
	std::error_condition set_format(std::string_view options); // e.g. "b19200,parenb,cstopb"
	std::error_condition purge();                              // drop unsent and unread data

protected:
	// device_t implementation
	virtual void device_start() override ATTR_COLD;

	// device_image_interface implementation
	virtual software_list_loader const &get_software_list_loader() const override;

private:
	util::core_file *serial_file();

	devcb_write_line m_mounted_cb;
	char const *m_interface;
	bool m_is_readonly;
	bool m_stream_mode = false;
	osd_file::ptr m_stream;
};


// device type definition
DECLARE_DEVICE_TYPE(BITBANGER, bitbanger_device)

#endif // MAME_IMAGEDEV_BITBNGR_H
