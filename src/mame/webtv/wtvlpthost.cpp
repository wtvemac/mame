// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// A real parallel port on the host, wired pin for pin to the box's printer port (Linux only).

#include "emu.h"

#include "wtvlpthost.h"

// Linux only
#if defined(__linux__)

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <unistd.h>

DEFINE_DEVICE_TYPE(WTV_LPT_HOST, wtv_lpt_host_device, "wtv_lpt_host", "Host parallel port (pin passthrough)")

wtv_lpt_host_device::wtv_lpt_host_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTV_LPT_HOST, tag, owner, clock),
	device_centronics_peripheral_interface(mconfig, *this),
	device_image_interface(mconfig, *this),
	m_fd(-1),
	// ecp_lpt starts with every line low and only signals changes
	m_box_data(0x00),
	m_box_control(0x00),
	m_box_drives_data(true),
	m_port_data(0x00),
	m_port_data_valid(false),
	m_port_reverse(false),
	m_port_can_reverse(false),
	m_shown_data(DATA_RELEASED),
	m_shown_status(STATUS_PULLED_UP),
	m_poll_timer(nullptr),
	m_flush_timer(nullptr)
{
}

void wtv_lpt_host_device::device_start()
{
	m_poll_timer = timer_alloc(FUNC(wtv_lpt_host_device::poll_tick), this);
	m_flush_timer = timer_alloc(FUNC(wtv_lpt_host_device::flush_tick), this);

	// The far end is real hardware, so nothing here goes in a save state.
}

void wtv_lpt_host_device::device_stop()
{
	close_port();
}

std::pair<std::error_condition, std::string> wtv_lpt_host_device::call_load()
{
	close_port();

	const int fd = ::open(filename(), O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		const int err = errno;
		std::string message = util::string_format("can't open %s: %s", filename(), std::strerror(err));

		if (err == EACCES)
			message += " (is this user in the device's group, usually \"lp\"?)";
		else if (err == ENOENT)
			message += " (is the ppdev module loaded? USB printer cables, /dev/usb/lpN, can't be used)";

		return std::make_pair(std::error_condition(err, std::generic_category()), message);
	}

	// Nothing else may touch the port while it's ours (the lp driver, CUPS).
	if (::ioctl(fd, PPEXCL) < 0)
		osd_printf_warning("hostlpt: %s: can't get exclusive use: %s\n", filename(), std::strerror(errno));

	if (::ioctl(fd, PPCLAIM) < 0)
	{
		const int err = errno;
		::close(fd);
		return std::make_pair(std::error_condition(err, std::generic_category()),
				util::string_format("%s isn't a ppdev parallel port, or it's in use: %s", filename(), std::strerror(err)));
	}

	m_fd = fd;

	unsigned int modes = 0;
	m_port_can_reverse = ::ioctl(m_fd, PPGETMODES, &modes) == 0 && (modes & PARPORT_MODE_TRISTATE);
	if (!m_port_can_reverse)
	{
		osd_printf_warning("hostlpt: %s can't turn its data lines around (set the port to PS/2, EPP or ECP in the BIOS). The box can still send, and read the status lines (nibble mode), but not read D0-D7.\n", filename());
	}

	m_port_reverse = false;
	m_port_data_valid = false;
	apply_data_dir();
	flush_data();
	write_control();

	if (!port_open())
		return std::make_pair(std::error_condition(EIO, std::generic_category()), std::string("can't drive the port's lines"));

	// Show the box the real lines, whatever it thinks it saw before.
	sample_lines(true);

	if (!port_open())
		return std::make_pair(std::error_condition(EIO, std::generic_category()), std::string("can't read the port's lines"));

	osd_printf_info("hostlpt: %s attached\n", filename());

	mark_active();
	return std::make_pair(std::error_condition(), std::string());
}

void wtv_lpt_host_device::call_unload()
{
	close_port();
}

void wtv_lpt_host_device::close_port()
{
	if (m_poll_timer)
		m_poll_timer->adjust(attotime::never);
	if (m_flush_timer)
		m_flush_timer->adjust(attotime::never);

	if (port_open())
	{
		// Leave the port as a printer driver expects to find it: data in, control lines idle.
		if (m_port_can_reverse)
		{
			int reverse = 1;
			::ioctl(m_fd, PPDATADIR, &reverse);
		}

		uint8_t control = PARPORT_CONTROL_INIT;
		::ioctl(m_fd, PPWCONTROL, &control);

		::ioctl(m_fd, PPRELEASE);
		::close(m_fd);
	}
	m_fd = -1;

	// Back to an empty slot.
	if (started())
		present(DATA_RELEASED, STATUS_PULLED_UP, false);
}

void wtv_lpt_host_device::port_failed(const char *what)
{
	osd_printf_error("hostlpt: %s failed: %s; detaching the port\n", what, std::strerror(errno));
	close_port();
}

void wtv_lpt_host_device::mark_active()
{
	if (!port_open())
		return;

	const attotime now = machine().time();
	const bool was_idle = now >= m_active_until;
	m_active_until = now + ACTIVE_HOLD;

	if (was_idle || m_poll_timer->remaining() > ACTIVE_POLL)
		m_poll_timer->adjust(ACTIVE_POLL);
}

void wtv_lpt_host_device::change_data_bit(int bit, int state)
{
	const uint8_t old_data = m_box_data;
	if (state)
		m_box_data |= 1 << bit;
	else
		m_box_data &= ~(1 << bit);

	if (m_box_data == old_data || !port_open())
		return;

	// ecp_lpt sends a byte a bit at a time: put it out once, before the next strobe or read.
	if (!m_flush_timer->enabled())
		m_flush_timer->adjust(attotime::zero);

	mark_active();
}

void wtv_lpt_host_device::change_control(uint8_t pin, int state)
{
	const uint8_t old_control = m_box_control;
	if (state)
		m_box_control |= pin;
	else
		m_box_control &= ~pin;

	if (m_box_control == old_control || !port_open())
		return;

	// The data must be on the lines before the strobe that clocks it.
	flush_data();
	write_control();
	mark_active();
}

void wtv_lpt_host_device::host_data_dir(bool host_drives)
{
	m_box_drives_data = host_drives;
	if (!port_open())
		return;

	apply_data_dir();
	mark_active();
}

void wtv_lpt_host_device::host_sample_lines()
{
	if (!port_open())
		return;

	sample_lines();
}

TIMER_CALLBACK_MEMBER(wtv_lpt_host_device::flush_tick)
{
	flush_data();
}

TIMER_CALLBACK_MEMBER(wtv_lpt_host_device::poll_tick)
{
	if (!port_open())
		return;

	sample_lines();
	if (!port_open())
		return;

	m_poll_timer->adjust(machine().time() < m_active_until ? ACTIVE_POLL : IDLE_POLL);
}

void wtv_lpt_host_device::apply_data_dir()
{
	// A port that can't turn around keeps driving; the box then reads back its own released 0xff.
	const bool reverse = !m_box_drives_data && m_port_can_reverse;
	if (reverse == m_port_reverse)
		return;

	int dir = reverse ? 1 : 0;
	if (::ioctl(m_fd, PPDATADIR, &dir) < 0)
	{
		port_failed("PPDATADIR");
		return;
	}
	m_port_reverse = reverse;

	// what the box drives goes on the lines as they turn
	if (!reverse)
		flush_data();
}

void wtv_lpt_host_device::flush_data()
{
	if (!port_open())
		return;

	m_flush_timer->adjust(attotime::never);
	if (m_port_data_valid && m_port_data == m_box_data)
		return;

	uint8_t data = m_box_data;
	if (::ioctl(m_fd, PPWDATA, &data) < 0)
	{
		port_failed("PPWDATA");
		return;
	}
	m_port_data = m_box_data;
	m_port_data_valid = true;
}

void wtv_lpt_host_device::write_control()
{
	if (!port_open())
		return;

	// ppdev takes the raw control register: the port inverts nStrobe, nAutoFd and nSelectIn to the
	// pins, but not nInit.
	uint8_t control = 0;
	if (!(m_box_control & PIN_STROBE_N))
		control |= PARPORT_CONTROL_STROBE;
	if (!(m_box_control & PIN_AUTOFD_N))
		control |= PARPORT_CONTROL_AUTOFD;
	if (m_box_control & PIN_INIT_N)
		control |= PARPORT_CONTROL_INIT;
	if (!(m_box_control & PIN_SELECTIN_N))
		control |= PARPORT_CONTROL_SELECT;

	if (::ioctl(m_fd, PPWCONTROL, &control) < 0)
		port_failed("PPWCONTROL");
}

void wtv_lpt_host_device::sample_lines(bool force)
{
	flush_data();
	if (!port_open())
		return;

	// Data first: in ECP reverse, the box latches D0-D7 on nAck's falling edge.
	uint8_t data = DATA_RELEASED;
	if (m_port_reverse)
	{
		uint8_t port_data;
		if (::ioctl(m_fd, PPRDATA, &port_data) < 0)
		{
			port_failed("PPRDATA");
			return;
		}
		data = port_data;
	}

	// The raw status register: Busy is inverted from its pin, the rest aren't.
	uint8_t port_status;
	if (::ioctl(m_fd, PPRSTATUS, &port_status) < 0)
	{
		port_failed("PPRSTATUS");
	}
	else
	{
		uint8_t status = 0;
		if (!(port_status & PARPORT_STATUS_BUSY))
			status |= STATUS_BUSY;
		if (port_status & PARPORT_STATUS_ACK)
			status |= STATUS_ACK_N;
		if (port_status & PARPORT_STATUS_PAPEROUT)
			status |= STATUS_PERROR;
		if (port_status & PARPORT_STATUS_SELECT)
			status |= STATUS_SELECT;
		if (port_status & PARPORT_STATUS_ERROR)
			status |= STATUS_FAULT_N;

		if (force || data != m_shown_data || status != m_shown_status)
		{
			present(data, status, force);
			mark_active();
		}
	}
}

void wtv_lpt_host_device::present(uint8_t data, uint8_t status, bool force)
{
	const uint8_t data_changed = force ? 0xff : (data ^ m_shown_data);
	const uint8_t status_changed = force ? 0xff : (status ^ m_shown_status);

	m_shown_data = data;
	m_shown_status = status;

	if (BIT(data_changed, 0))
		output_data0(BIT(data, 0));
	if (BIT(data_changed, 1))
		output_data1(BIT(data, 1));
	if (BIT(data_changed, 2))
		output_data2(BIT(data, 2));
	if (BIT(data_changed, 3))
		output_data3(BIT(data, 3));
	if (BIT(data_changed, 4))
		output_data4(BIT(data, 4));
	if (BIT(data_changed, 5))
		output_data5(BIT(data, 5));
	if (BIT(data_changed, 6))
		output_data6(BIT(data, 6));
	if (BIT(data_changed, 7))
		output_data7(BIT(data, 7));

	if (status_changed & STATUS_FAULT_N)
		output_fault((status & STATUS_FAULT_N) ? 1 : 0);
	if (status_changed & STATUS_SELECT)
		output_select((status & STATUS_SELECT) ? 1 : 0);
	if (status_changed & STATUS_PERROR)
		output_perror((status & STATUS_PERROR) ? 1 : 0);
	if (status_changed & STATUS_BUSY)
		output_busy((status & STATUS_BUSY) ? 1 : 0);
	// nAck last: it's the strobe of the peripheral-to-box handshakes.
	if (status_changed & STATUS_ACK_N)
		output_ack((status & STATUS_ACK_N) ? 1 : 0);
}

#endif // defined(__linux__)
