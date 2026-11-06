// license:BSD-3-Clause
// copyright-holders:wtvemac

// Description here

#ifndef MAME_BUS_CENTRONICS_LPTLINK_H
#define MAME_BUS_CENTRONICS_LPTLINK_H
#pragma once

// Optional controller/peripheral extension for bidirectional parallel links.
// Standard Centronics callbacks do not convey bus ownership, sampling barriers,
// or atomic output-update boundaries. Existing peripherals need not implement it.
class device_ecp_lpt_link_interface
{
public:
	virtual ~device_ecp_lpt_link_interface() = default;

	// true: the host drives D0-D7; false: it has released them for the peripheral
	virtual void host_data_dir(bool host_drives) = 0;

	// the host is about to look at the status or data lines
	virtual void host_sample_lines() = 0;

	// End of an atomic data/control output update (pin callbacks are bitwise).
	virtual void host_output_changed() { }
};

#endif
