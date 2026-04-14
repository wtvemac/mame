// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "i82830_vga.h"

DEFINE_DEVICE_TYPE(I82830_VGA, i82830_svga_device, "i82830_vga", "Intel 830M VGA Controller")

i82830_svga_device::i82830_svga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: svga_device(mconfig, I82830_VGA, tag, owner, clock)
{
}

void i82830_svga_device::device_start()
{
	svga_device::device_start();

	i82830_svga_device::set_default_values();
}

void i82830_svga_device::device_reset()
{
	svga_device::device_reset();

	i82830_svga_device::set_default_values();
}

void i82830_svga_device::set_default_values()
{
	vga.crtc.sync_en = 1;
	vga.gc.alpha_dis = 0;
	vga.crtc.maximum_scan_line = 1;
	vga.svga_intf.vram_size = 512*1024;
	//
}

void i82830_svga_device::io_map(address_map &map)
{
	map(0x04, 0x04).rw(FUNC(i82830_svga_device::crtc_address_r), FUNC(i82830_svga_device::crtc_address_w));
	map(0x05, 0x05).rw(FUNC(i82830_svga_device::crtc_data_r), FUNC(i82830_svga_device::crtc_data_w));
	map(0x0a, 0x0a).rw(FUNC(i82830_svga_device::input_status_1_r), FUNC(i82830_svga_device::feature_control_w));

	map(0x10, 0x10).rw(FUNC(i82830_svga_device::atc_address_r), FUNC(i82830_svga_device::atc_address_data_w));
	map(0x11, 0x11).r(FUNC(i82830_svga_device::atc_data_r));
	map(0x12, 0x12).rw(FUNC(i82830_svga_device::input_status_0_r), FUNC(i82830_svga_device::miscellaneous_output_w));
	map(0x14, 0x14).rw(FUNC(i82830_svga_device::sequencer_address_r), FUNC(i82830_svga_device::sequencer_address_w));
	map(0x15, 0x15).rw(FUNC(i82830_svga_device::sequencer_data_r), FUNC(i82830_svga_device::sequencer_data_w));
	map(0x16, 0x16).rw(FUNC(i82830_svga_device::ramdac_mask_r), FUNC(i82830_svga_device::ramdac_mask_w));
	map(0x17, 0x17).rw(FUNC(i82830_svga_device::ramdac_state_r), FUNC(i82830_svga_device::ramdac_read_index_w));
	map(0x18, 0x18).rw(FUNC(i82830_svga_device::ramdac_write_index_r), FUNC(i82830_svga_device::ramdac_write_index_w));
	map(0x19, 0x19).rw(FUNC(i82830_svga_device::ramdac_data_r), FUNC(i82830_svga_device::ramdac_data_w));
	map(0x1a, 0x1a).r(FUNC(i82830_svga_device::feature_control_r));
	map(0x1c, 0x1c).r(FUNC(i82830_svga_device::miscellaneous_output_r));
	map(0x1e, 0x1e).rw(FUNC(i82830_svga_device::gc_address_r), FUNC(i82830_svga_device::gc_address_w));
	map(0x1f, 0x1f).rw(FUNC(i82830_svga_device::gc_data_r), FUNC(i82830_svga_device::gc_data_w));

	map(0x24, 0x24).rw(FUNC(i82830_svga_device::crtc_address_r), FUNC(i82830_svga_device::crtc_address_w));
	map(0x25, 0x25).rw(FUNC(i82830_svga_device::crtc_data_r), FUNC(i82830_svga_device::crtc_data_w));
	map(0x2a, 0x2a).rw(FUNC(i82830_svga_device::input_status_1_r), FUNC(i82830_svga_device::feature_control_w));
}

void i82830_svga_device::mem_map(address_map &map)
{
	map(0x00000000, (VGA_MEM_SIZE - 1)).rw(FUNC(i82830_svga_device::mem_r), FUNC(i82830_svga_device::mem_w));
}
