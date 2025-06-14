#include "emu.h"

#include "screen.h"
#include "solo_asic_video.h"

DEFINE_DEVICE_TYPE(SOLO_ASIC_VIDEO, solo_asic_video_device, "solo_asic_video_device", "WebTV SOLO VIDEO (vid, gfx, dve, div, pot)")

solo_asic_video_device::solo_asic_video_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO_ASIC_VIDEO, tag, owner, clock),
	device_video_interface(mconfig, *this),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_hostram(*this, finder_base::DUMMY_TAG),
	m_screen(*this, "screen"),
	m_int_enable_cb(*this),
	m_int_irq_cb(*this)
{
}

void solo_asic_video_device::device_start()
{
	solo_asic_video_device::device_reset();

	save_item(NAME(m_busvid_intenable));
	save_item(NAME(m_busvid_intstat));

	save_item(NAME(m_vid_nstart));
	save_item(NAME(m_vid_nsize));
	save_item(NAME(m_vid_dmacntl));
	save_item(NAME(m_vid_cstart));
	save_item(NAME(m_vid_csize));
	save_item(NAME(m_vid_ccnt));
	save_item(NAME(m_vid_cline));
	save_item(NAME(m_vid_vdata));
	save_item(NAME(m_vid_intenable));
	save_item(NAME(m_vid_intstat));
	save_item(NAME(m_gfx_cntl));
	save_item(NAME(m_gfx_oot_ycount));
	save_item(NAME(m_gfx_ymap_base));
	save_item(NAME(m_gfx_ymap_base_master));
	save_item(NAME(m_gfx_cels_base));
	save_item(NAME(m_gfx_cels_base_master));
	save_item(NAME(m_gfx_initcolor));
	save_item(NAME(m_gfx_ycounter_init));
	save_item(NAME(m_gfx_pausecycles));
	save_item(NAME(m_gfx_oot_cels_base));
	save_item(NAME(m_gfx_oot_ymap_base));
	save_item(NAME(m_gfx_oot_cels_offset));
	save_item(NAME(m_gfx_oot_ymap_count));
	save_item(NAME(m_gfx_termcycle_count));
	save_item(NAME(m_gfx_hcounter_init));
	save_item(NAME(m_gfx_blanklines));
	save_item(NAME(m_gfx_activelines));
	save_item(NAME(m_gfx_wbdstart));
	save_item(NAME(m_gfx_wbdlsize));
	save_item(NAME(m_gfx_wbstride));
	save_item(NAME(m_gfx_wbdconfig));
	save_item(NAME(m_gfx_intenable));
	save_item(NAME(m_gfx_intstat));

	save_item(NAME(m_div_intenable));
	save_item(NAME(m_div_intstat));

	save_item(NAME(m_pot_vstart));
	save_item(NAME(m_pot_vsize));
	save_item(NAME(m_pot_blank_color));
	save_item(NAME(m_pot_hstart));
	save_item(NAME(m_pot_hsize));
	save_item(NAME(m_pot_cntl));
	save_item(NAME(m_pot_hintline));
	save_item(NAME(m_pot_intenable));
	save_item(NAME(m_pot_intstat));

	save_item(NAME(m_vid_draw_nstart));
	save_item(NAME(m_pot_draw_hstart));
	save_item(NAME(m_pot_draw_hsize));
	save_item(NAME(m_pot_draw_vstart));
	save_item(NAME(m_pot_draw_vsize));
	save_item(NAME(m_pot_draw_blank_color));
	save_item(NAME(m_pot_draw_hintline));
}

void solo_asic_video_device::device_reset()
{
	m_busvid_intenable = 0x0;
	m_busvid_intstat = 0x0;

	m_vid_nstart = 0x80000000;
	m_vid_nsize = 0x0;
	m_vid_dmacntl = 0x0;
	m_vid_cstart = 0x0;
	m_vid_csize = 0x0;
	m_vid_ccnt = 0x0;
	m_vid_cline = 0x0;
	m_vid_vdata = 0x0;
	m_vid_intenable = 0x0;
	m_vid_intstat = 0x0;
	m_gfx_cntl = 0x0;
	m_gfx_oot_ycount = 0x0;
	m_gfx_ymap_base = 0x80000000;
	m_gfx_ymap_base_master = 0x80000000;
	m_gfx_cels_base = 0x80000000;
	m_gfx_cels_base_master = 0x80000000;
	m_gfx_initcolor = 0x0;
	m_gfx_ycounter_init = 0x0;
	m_gfx_pausecycles = 0x0;
	m_gfx_oot_cels_base = 0x80000000;
	m_gfx_oot_ymap_base = 0x80000000;
	m_gfx_oot_cels_offset = 0x0;
	m_gfx_oot_ymap_count = 0x0;
	m_gfx_termcycle_count = 0x0;
	m_gfx_hcounter_init = 0x0;
	m_gfx_blanklines = 0x0;
	m_gfx_activelines = 0x0;
	m_gfx_wbdstart = 0x80000000;
	m_gfx_wbdlsize = 0x0;
	m_gfx_wbstride = 0x0;
	m_gfx_wbdconfig = 0x0;
	m_gfx_intenable = 0x0;
	m_gfx_intstat = 0x0;

	m_div_intenable = 0x0;
	m_div_intstat = 0x0;

	m_pot_vstart = POT_DEFAULT_VSTART;
	m_pot_vsize = POT_DEFAULT_VSIZE;
	m_pot_blank_color = POT_DEFAULT_COLOR;
	m_pot_hstart = POT_HSTART_OFFSET + POT_DEFAULT_HSTART;
	m_pot_hsize = POT_DEFAULT_HSIZE;
	m_pot_cntl = 0x0;
	m_pot_hintline = 0x0;
	m_pot_intenable = 0x0;
	m_pot_intstat = 0x0;

	m_vid_draw_nstart = 0x0;
	m_pot_draw_hstart = POT_HSTART_OFFSET;
	m_pot_draw_hsize = m_pot_hsize;
	m_pot_draw_vstart = m_pot_vstart;
	m_pot_draw_vsize = m_pot_vsize;
	m_pot_draw_blank_color = m_pot_blank_color;
	m_pot_draw_hintline = 0x0;

	solo_asic_video_device::validate_active_area();
}

void solo_asic_video_device::device_stop()
{
	//
}

void solo_asic_video_device::device_add_mconfig(machine_config &config)
{
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_screen_update(FUNC(solo_asic_video_device::screen_update));
	m_screen->screen_vblank().set(FUNC(solo_asic_video_device::vblank_irq));
	if (m_use_pal)
		m_screen->set_raw(PAL_SCREEN_XTAL, PAL_SCREEN_HTOTAL, 0, PAL_SCREEN_HBSTART, PAL_SCREEN_VTOTAL, 0, PAL_SCREEN_VBSTART);
	else
		m_screen->set_raw(NTSC_SCREEN_XTAL, NTSC_SCREEN_HTOTAL, 0, NTSC_SCREEN_HBSTART, NTSC_SCREEN_VTOTAL, 0, NTSC_SCREEN_VBSTART);
}

void solo_asic_video_device::vid_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_video_device::reg_3000_r));                                      // VID_CSTART
	map(0x004, 0x007).r(FUNC(solo_asic_video_device::reg_3004_r));                                      // VID_CSIZE
	map(0x008, 0x00b).r(FUNC(solo_asic_video_device::reg_3008_r));                                      // VID_CCNT
	map(0x00c, 0x00f).rw(FUNC(solo_asic_video_device::reg_300c_r), FUNC(solo_asic_video_device::reg_300c_w)); // VID_NSTART
	map(0x010, 0x013).rw(FUNC(solo_asic_video_device::reg_3010_r), FUNC(solo_asic_video_device::reg_3010_w)); // VID_NSIZE
	map(0x014, 0x017).rw(FUNC(solo_asic_video_device::reg_3014_r), FUNC(solo_asic_video_device::reg_3014_w)); // VID_DMACNTL
	map(0x038, 0x03b).r(FUNC(solo_asic_video_device::reg_3038_r));                                      // VID_INTSTAT
	map(0x138, 0x13b).w(FUNC(solo_asic_video_device::reg_3138_w));                                      // VID_INTSTAT_C
	map(0x03c, 0x03f).rw(FUNC(solo_asic_video_device::reg_303c_r), FUNC(solo_asic_video_device::reg_303c_w)); // VID_INTEN
	map(0x13c, 0x13f).w(FUNC(solo_asic_video_device::reg_313c_w));                                      // VID_INTEN_C
	map(0x040, 0x043).rw(FUNC(solo_asic_video_device::reg_3040_r), FUNC(solo_asic_video_device::reg_3040_w)); // VID_VDATA
}

void solo_asic_video_device::gfx_unit_map(address_map &map)
{
	map(0x004, 0x007).rw(FUNC(solo_asic_video_device::reg_6004_r), FUNC(solo_asic_video_device::reg_6004_w)); // GFX_CONTROL
	map(0x010, 0x013).rw(FUNC(solo_asic_video_device::reg_6010_r), FUNC(solo_asic_video_device::reg_6010_w)); // GFX_OOTYCOUNT
	map(0x014, 0x017).rw(FUNC(solo_asic_video_device::reg_6014_r), FUNC(solo_asic_video_device::reg_6014_w)); // GFX_CELSBASE
	map(0x018, 0x01b).rw(FUNC(solo_asic_video_device::reg_6018_r), FUNC(solo_asic_video_device::reg_6018_w)); // GFX_YMAPBASE
	map(0x01c, 0x01f).rw(FUNC(solo_asic_video_device::reg_601c_r), FUNC(solo_asic_video_device::reg_601c_w)); // GFX_CELSBASEMASTER
	map(0x020, 0x023).rw(FUNC(solo_asic_video_device::reg_6020_r), FUNC(solo_asic_video_device::reg_6020_w)); // GFX_YMAPBASEMASTER
	map(0x024, 0x027).rw(FUNC(solo_asic_video_device::reg_6024_r), FUNC(solo_asic_video_device::reg_6024_w)); // GFX_INITCOLOR
	map(0x028, 0x02b).rw(FUNC(solo_asic_video_device::reg_6028_r), FUNC(solo_asic_video_device::reg_6028_w)); // GFX_YCOUNTERINlT
	map(0x02c, 0x02f).rw(FUNC(solo_asic_video_device::reg_602c_r), FUNC(solo_asic_video_device::reg_602c_w)); // GFX_PAUSECYCLES
	map(0x030, 0x033).rw(FUNC(solo_asic_video_device::reg_6030_r), FUNC(solo_asic_video_device::reg_6030_w)); // GFX_OOTCELSBASE
	map(0x034, 0x037).rw(FUNC(solo_asic_video_device::reg_6034_r), FUNC(solo_asic_video_device::reg_6034_w)); // GFX_OOTYMAPBASE
	map(0x038, 0x03b).rw(FUNC(solo_asic_video_device::reg_6038_r), FUNC(solo_asic_video_device::reg_6038_w)); // GFX_OOTCELSOFFSET
	map(0x03c, 0x03f).rw(FUNC(solo_asic_video_device::reg_603c_r), FUNC(solo_asic_video_device::reg_603c_w)); // GFX_OOTYMAPCOUNT
	map(0x040, 0x043).rw(FUNC(solo_asic_video_device::reg_6040_r), FUNC(solo_asic_video_device::reg_6040_w)); // GFX_TERMCYCLECOUNT
	map(0x044, 0x047).rw(FUNC(solo_asic_video_device::reg_6044_r), FUNC(solo_asic_video_device::reg_6044_w)); // GFX_HCOUNTERINIT
	map(0x048, 0x04b).rw(FUNC(solo_asic_video_device::reg_6048_r), FUNC(solo_asic_video_device::reg_6048_w)); // GFX_BLANKLINES
	map(0x04c, 0x04f).rw(FUNC(solo_asic_video_device::reg_604c_r), FUNC(solo_asic_video_device::reg_604c_w)); // GFX_ACTIVELINES
	map(0x060, 0x063).rw(FUNC(solo_asic_video_device::reg_6060_r), FUNC(solo_asic_video_device::reg_6060_w)); // GFX_INTEN
	map(0x064, 0x067).w(FUNC(solo_asic_video_device::reg_6064_w));                                      // GFX_INTEN_C
	map(0x068, 0x06b).rw(FUNC(solo_asic_video_device::reg_6068_r), FUNC(solo_asic_video_device::reg_6068_w)); // GFX_INTSTAT
	map(0x06c, 0x06f).w(FUNC(solo_asic_video_device::reg_606c_w));                                      // GFX_INTSTAT_C
	map(0x080, 0x083).rw(FUNC(solo_asic_video_device::reg_6080_r), FUNC(solo_asic_video_device::reg_6080_w)); // GFX_WBDSTART
	map(0x084, 0x087).rw(FUNC(solo_asic_video_device::reg_6084_r), FUNC(solo_asic_video_device::reg_6084_w)); // GFX_WBDLSIZE
	map(0x08c, 0x08f).rw(FUNC(solo_asic_video_device::reg_608c_r), FUNC(solo_asic_video_device::reg_608c_w)); // GFX_WBSTRIDE
	map(0x090, 0x093).rw(FUNC(solo_asic_video_device::reg_6090_r), FUNC(solo_asic_video_device::reg_6090_w)); // GFX_WBDCONFIG
	map(0x094, 0x097).rw(FUNC(solo_asic_video_device::reg_6094_r), FUNC(solo_asic_video_device::reg_6094_w)); // GFX_WBDSTART
}

void solo_asic_video_device::dve_unit_map(address_map &map)
{
	//
}

void solo_asic_video_device::div_unit_map(address_map &map)
{
	map(0x004, 0x007).rw(FUNC(solo_asic_video_device::reg_8004_r), FUNC(solo_asic_video_device::reg_8004_w)); // DIV_CMACTL
	map(0x01c, 0x01f).rw(FUNC(solo_asic_video_device::reg_801c_r), FUNC(solo_asic_video_device::reg_801c_w)); // DIV_NEXTCFG
	map(0x038, 0x03b).rw(FUNC(solo_asic_video_device::reg_8038_r), FUNC(solo_asic_video_device::reg_8038_w)); // DIV_CURRCFG
}

void solo_asic_video_device::pot_unit_map(address_map &map)
{
	map(0x080, 0x083).rw(FUNC(solo_asic_video_device::reg_9080_r), FUNC(solo_asic_video_device::reg_9080_w)); // POT_VSTART
	map(0x084, 0x087).rw(FUNC(solo_asic_video_device::reg_9084_r), FUNC(solo_asic_video_device::reg_9084_w)); // POT_VSIZE
	map(0x088, 0x08b).rw(FUNC(solo_asic_video_device::reg_9088_r), FUNC(solo_asic_video_device::reg_9088_w)); // POT_BLNKCOL
	map(0x08c, 0x08f).rw(FUNC(solo_asic_video_device::reg_908c_r), FUNC(solo_asic_video_device::reg_908c_w)); // POT_HSTART
	map(0x090, 0x093).rw(FUNC(solo_asic_video_device::reg_9090_r), FUNC(solo_asic_video_device::reg_9090_w)); // POT_HSIZE
	map(0x094, 0x097).rw(FUNC(solo_asic_video_device::reg_9094_r), FUNC(solo_asic_video_device::reg_9094_w)); // POT_CNTL
	map(0x098, 0x09b).rw(FUNC(solo_asic_video_device::reg_9098_r), FUNC(solo_asic_video_device::reg_9098_w)); // POT_HINTLINE
	map(0x09c, 0x09f).rw(FUNC(solo_asic_video_device::reg_909c_r), FUNC(solo_asic_video_device::reg_909c_w)); // POT_INTEN
	map(0x0a4, 0x0a7).w(FUNC(solo_asic_video_device::reg_90a4_w));                                            // POT_INTEN_C
	map(0x0a0, 0x0a3).r(FUNC(solo_asic_video_device::reg_90a0_r));                                            // POT_INTSTAT
	map(0x0a8, 0x0ab).rw(FUNC(solo_asic_video_device::reg_90a8_r), FUNC(solo_asic_video_device::reg_90a8_w)); // POT_INTSTAT_C
	map(0x0ac, 0x0af).r(FUNC(solo_asic_video_device::reg_90ac_r));                                            // POT_CLINE
}

void solo_asic_video_device::enable_ntsc()
{
	m_use_pal = false;
}

void solo_asic_video_device::enable_pal()
{
	m_use_pal = true;
}

uint32_t solo_asic_video_device::busvid_intenable_get()
{
	return m_busvid_intenable;
}

void solo_asic_video_device::busvid_intenable_set(uint32_t data)
{
	m_busvid_intenable |= data;
	if (m_busvid_intenable != 0x0)
	{
		m_int_enable_cb(1);
	}
}

void solo_asic_video_device::busvid_intenable_clear(uint32_t data)
{
	m_busvid_intenable &= (~data);
}

uint32_t solo_asic_video_device::busvid_intstat_get()
{
	return m_busvid_intstat;
}

void solo_asic_video_device::busvid_intstat_set(uint32_t data)
{
	m_busvid_intstat |= data;
}

void solo_asic_video_device::busvid_intstat_clear(uint32_t data)
{
	uint32_t check_intstat = 0x0;

	switch (data)
	{
		case BUS_INT_VID_DIVUNIT:
			check_intstat = m_div_intstat;
			break;

		case BUS_INT_VID_GFXUNIT:
			check_intstat = m_gfx_intstat;
			break;

		case BUS_INT_VID_POTUNIT:
			check_intstat = m_pot_intstat;
			break;

		case BUS_INT_VID_VIDUNIT:
			check_intstat = m_vid_intstat;
			break;
	}

	// Windows CE builds
	if (m_hostcpu->get_endianness() == ENDIANNESS_LITTLE)
	{
		solo_asic_video_device::set_video_irq(data, check_intstat, 0);
	}
	// WebTV OS builds
	else if (check_intstat == 0x0)
	{
		m_busvid_intstat &= (~data);

		if(m_busvid_intstat == 0x0)
		{
			m_int_irq_cb(0);
		}
	}
}

void solo_asic_video_device::validate_active_area()
{
	// The active h size can't be larger than the screen width or smaller than 2 pixels.
	m_pot_draw_hsize = std::clamp(m_pot_hsize, (uint32_t)0x2, (uint32_t)m_screen->width());
	// The active v size can't be larger than the screen height or smaller than 2 pixels.
	m_pot_draw_vsize = std::clamp(m_pot_vsize, (uint32_t)0x2, (uint32_t)m_screen->height());

	m_vid_draw_nstart = m_vid_nstart;

	uint32_t hstart_offset = POT_HSTART_OFFSET;
	uint32_t vstart_offset = POT_VSTART_OFFSET;
	if (m_pot_cntl & POT_FCNTL_USEGFX)
	{
		m_pot_draw_hsize = std::min(m_pot_draw_hsize, 2 * m_gfx_hcounter_init);
		m_pot_draw_vsize = std::min(m_pot_draw_vsize, (2 * m_gfx_activelines) - m_gfx_blanklines - 2);
	}
	else
	{
		m_vid_draw_nstart += 2 * (m_pot_draw_hsize * BYTES_PER_PIXEL);
		m_pot_draw_vsize = m_pot_draw_vsize - 2;
	}

	// The active h start can't be smaller than 2
	m_pot_draw_hstart = std::max((int32_t)m_pot_hstart - (int32_t)hstart_offset, (int32_t)0x2);
	// The active v start can't be smaller than 2
	m_pot_draw_vstart = std::max((int32_t)m_pot_vstart - (int32_t)vstart_offset, (int32_t)0x2);

	// The active h start can't push the active area off the screen.
	if ((m_pot_draw_hstart + m_pot_draw_hsize) > m_screen->width())
		m_pot_draw_hstart = (m_screen->width() - m_pot_draw_hsize); // to screen edge
	else if ((int32_t)m_pot_draw_hstart < 0)
		m_pot_draw_hstart = 0;

	// The active v start can't push the active area off the screen.
	if ((m_pot_draw_vstart + m_pot_draw_vsize) > m_screen->height())
		m_pot_draw_vstart = (m_screen->height() - m_pot_draw_vsize); // to screen edge
	else if ((int32_t)m_pot_draw_vstart < 0)
		m_pot_draw_vstart = 0;
}

void solo_asic_video_device::vblank_irq(int state)
{
	solo_asic_video_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_VSYNCO, 1);
}

uint32_t solo_asic_video_device::reg_3000_r()
{
	return m_vid_cstart;
}

uint32_t solo_asic_video_device::reg_3004_r()
{
	return m_vid_csize;
}

uint32_t solo_asic_video_device::reg_3008_r()
{
	return m_vid_ccnt;
}

uint32_t solo_asic_video_device::reg_300c_r()
{
	return m_vid_nstart;
}

void solo_asic_video_device::reg_300c_w(uint32_t data)
{
	data &= (~0xfc000003);

	bool has_changed = (m_vid_nstart != data);

	m_vid_nstart = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_3010_r()
{
	return m_vid_nsize;
}

void solo_asic_video_device::reg_3010_w(uint32_t data)
{
	bool has_changed = (m_vid_nsize != data);

	m_vid_nsize = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_3014_r()
{
	return m_vid_dmacntl;
}

void solo_asic_video_device::reg_3014_w(uint32_t data)
{
	if ((m_vid_dmacntl ^ data) & VID_DMACNTL_NV && data & VID_DMACNTL_NV)
		solo_asic_video_device::validate_active_area();

	m_vid_dmacntl = data;
}

uint32_t solo_asic_video_device::reg_3038_r()
{
	return m_vid_intstat;
}

void solo_asic_video_device::reg_3138_w(uint32_t data)
{
	solo_asic_video_device::set_video_irq(BUS_INT_VID_VIDUNIT, data, 0);
}

uint32_t solo_asic_video_device::reg_303c_r()
{
	return m_vid_intenable;
}

void solo_asic_video_device::reg_303c_w(uint32_t data)
{
	m_vid_intenable |= data;
	if (m_vid_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_VIDUNIT;
		m_int_enable_cb(1);
	}
}

void solo_asic_video_device::reg_313c_w(uint32_t data)
{
	solo_asic_video_device::reg_3138_w(data);
	m_vid_intenable &= (~data);
}

uint32_t solo_asic_video_device::reg_3040_r()
{
	return m_vid_vdata;
}

void solo_asic_video_device::reg_3040_w(uint32_t data)
{
	m_vid_vdata = data;
}

uint32_t solo_asic_video_device::reg_6004_r()
{
	return m_gfx_cntl;
}

void solo_asic_video_device::reg_6004_w(uint32_t data)
{
	m_gfx_cntl = data;
}

uint32_t solo_asic_video_device::reg_6010_r()
{
	return m_gfx_oot_ycount;
}

void solo_asic_video_device::reg_6010_w(uint32_t data)
{
	m_gfx_oot_ycount = data;
}

uint32_t solo_asic_video_device::reg_6014_r()
{
	return m_gfx_cels_base;
}

void solo_asic_video_device::reg_6014_w(uint32_t data)
{
	m_gfx_cels_base = data & (~0xfc000003);
}

uint32_t solo_asic_video_device::reg_6018_r()
{
	return m_gfx_ymap_base;
}

void solo_asic_video_device::reg_6018_w(uint32_t data)
{
	m_gfx_ymap_base = data & (~0xfc000003);
}

uint32_t solo_asic_video_device::reg_601c_r()
{
	return m_gfx_cels_base_master;
}

void solo_asic_video_device::reg_601c_w(uint32_t data)
{
	m_gfx_cels_base_master = data & (~0xfc000003);
}

uint32_t solo_asic_video_device::reg_6020_r()
{
	return m_gfx_ymap_base_master;
}

void solo_asic_video_device::reg_6020_w(uint32_t data)
{
	m_gfx_ymap_base_master = data & (~0xfc000003);
}

uint32_t solo_asic_video_device::reg_6024_r()
{
	return m_gfx_initcolor;
}

void solo_asic_video_device::reg_6024_w(uint32_t data)
{
	m_gfx_initcolor = data;
}

uint32_t solo_asic_video_device::reg_6028_r()
{
	return m_gfx_ycounter_init;
}

void solo_asic_video_device::reg_6028_w(uint32_t data)
{
	m_gfx_ycounter_init = data;
}

uint32_t solo_asic_video_device::reg_602c_r()
{
	return m_gfx_pausecycles;
}

void solo_asic_video_device::reg_602c_w(uint32_t data)
{
	m_gfx_pausecycles = data;
}

uint32_t solo_asic_video_device::reg_6030_r()
{
	return m_gfx_oot_cels_base;
}

void solo_asic_video_device::reg_6030_w(uint32_t data)
{
	m_gfx_oot_cels_base = data;
}

uint32_t solo_asic_video_device::reg_6034_r()
{
	return m_gfx_oot_ymap_base;
}

void solo_asic_video_device::reg_6034_w(uint32_t data)
{
	m_gfx_oot_ymap_base = data;
}

uint32_t solo_asic_video_device::reg_6038_r()
{
	return m_gfx_oot_cels_offset;
}

void solo_asic_video_device::reg_6038_w(uint32_t data)
{
	m_gfx_oot_cels_offset = data;
}

uint32_t solo_asic_video_device::reg_603c_r()
{
	return m_gfx_oot_ymap_count;
}

void solo_asic_video_device::reg_603c_w(uint32_t data)
{
	m_gfx_oot_ymap_count = data;
}

uint32_t solo_asic_video_device::reg_6040_r()
{
	return m_gfx_termcycle_count;
}

void solo_asic_video_device::reg_6040_w(uint32_t data)
{
	m_gfx_termcycle_count = data;
}

uint32_t solo_asic_video_device::reg_6044_r()
{
	return m_gfx_hcounter_init;
}

void solo_asic_video_device::reg_6044_w(uint32_t data)
{
	m_gfx_hcounter_init = data;
}

uint32_t solo_asic_video_device::reg_6048_r()
{
	return m_gfx_blanklines;
}

void solo_asic_video_device::reg_6048_w(uint32_t data)
{
	bool has_changed = (m_gfx_blanklines != data);

	m_gfx_blanklines = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_604c_r()
{
	return m_gfx_activelines;
}

void solo_asic_video_device::reg_604c_w(uint32_t data)
{
	bool has_changed = (m_gfx_activelines != data);

	m_gfx_activelines = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_6060_r()
{
	return m_gfx_intenable;
}

void solo_asic_video_device::reg_6060_w(uint32_t data)
{
	m_gfx_intenable |= data;
	if (m_gfx_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_GFXUNIT;
		m_int_enable_cb(1);
	}
}

void solo_asic_video_device::reg_6064_w(uint32_t data)
{
	solo_asic_video_device::reg_606c_w(data);
	m_gfx_intenable &= (~data);
}

uint32_t solo_asic_video_device::reg_6068_r()
{
	return m_gfx_intstat;
}

void solo_asic_video_device::reg_6068_w(uint32_t data)
{
	m_gfx_intstat |= data;
}

void solo_asic_video_device::reg_606c_w(uint32_t data)
{
	solo_asic_video_device::set_video_irq(BUS_INT_VID_GFXUNIT, data, 0);
}

uint32_t solo_asic_video_device::reg_6080_r()
{
	return m_gfx_wbdstart;
}

void solo_asic_video_device::reg_6080_w(uint32_t data)
{
	m_gfx_wbdstart = data & (~0xfc000003);
}
uint32_t solo_asic_video_device::reg_6084_r()
{
	return m_gfx_wbdlsize;
}

void solo_asic_video_device::reg_6084_w(uint32_t data)
{
	m_gfx_wbdlsize = data;
}

uint32_t solo_asic_video_device::reg_608c_r()
{
	return m_gfx_wbstride;
}

void solo_asic_video_device::reg_608c_w(uint32_t data)
{
	m_gfx_wbstride = data;
}

uint32_t solo_asic_video_device::reg_6090_r()
{
	return m_gfx_wbdconfig;
}

void solo_asic_video_device::reg_6090_w(uint32_t data)
{
	m_gfx_wbdconfig = data;
}

uint32_t solo_asic_video_device::reg_6094_r()
{
	return m_gfx_wbdstart;
}

void solo_asic_video_device::reg_6094_w(uint32_t data)
{
	m_gfx_wbdstart = data;
}

// divUnit registers

uint32_t solo_asic_video_device::reg_8004_r()
{
	return m_div_dmacntl;
}

void solo_asic_video_device::reg_8004_w(uint32_t data)
{
	m_div_dmacntl = data;
}

uint32_t solo_asic_video_device::reg_801c_r()
{
	return m_div_nextcfg;
}

void solo_asic_video_device::reg_801c_w(uint32_t data)
{
	m_div_nextcfg = data;
	m_div_currcfg = 0;
}

uint32_t solo_asic_video_device::reg_8038_r()
{
	return m_div_currcfg;
}

void solo_asic_video_device::reg_8038_w(uint32_t data)
{
	m_div_currcfg = data;
}

// potUnit registers

uint32_t solo_asic_video_device::reg_9080_r()
{
	return m_pot_vstart;
}

void solo_asic_video_device::reg_9080_w(uint32_t data)
{
	bool has_changed = (m_pot_vstart != data);

	m_pot_vstart = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_9084_r()
{
	return m_pot_vsize;
}

void solo_asic_video_device::reg_9084_w(uint32_t data)
{
	bool has_changed = (m_pot_vstart != data);

	m_pot_vsize = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_9088_r()
{
	return m_pot_blank_color;
}

void solo_asic_video_device::reg_9088_w(uint32_t data)
{
	m_pot_blank_color = data;

	m_pot_draw_blank_color = (((data >> 0x10) & 0xff) << 0x18) | (((data >> 0x08) & 0xff) << 0x10) | (((data >> 0x10) & 0xff) << 0x08) | (data & 0xff);     
}

uint32_t solo_asic_video_device::reg_908c_r()
{
	return m_pot_hstart;
}

void solo_asic_video_device::reg_908c_w(uint32_t data)
{
	bool has_changed = (m_pot_hstart != data);

	m_pot_hstart = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_9090_r()
{
	return m_pot_hsize;
}

void solo_asic_video_device::reg_9090_w(uint32_t data)
{
	bool has_changed = (m_pot_hsize != data);

	m_pot_hsize = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_9094_r()
{
	return m_pot_cntl;
}

void solo_asic_video_device::reg_9094_w(uint32_t data)
{
	bool has_changed = ((m_pot_cntl ^ data) & POT_FCNTL_USEGFX && data & POT_FCNTL_USEGFX);

	m_pot_cntl = data;

	if (has_changed)
		solo_asic_video_device::validate_active_area();
}

uint32_t solo_asic_video_device::reg_9098_r()
{
	return m_pot_hintline;
}

void solo_asic_video_device::reg_9098_w(uint32_t data)
{
	m_pot_hintline = data;

	m_pot_draw_hintline = std::min(m_pot_hintline, (uint32_t)(m_screen->height() - 1));
}

// _vid_ int variables are being used because everything fires as Vvid interrupts right now (code was copied from SPOT)

uint32_t solo_asic_video_device::reg_909c_r()
{
	return m_pot_intenable;
}

void solo_asic_video_device::reg_909c_w(uint32_t data)
{
	m_pot_intenable |= data;
	if (m_pot_intenable != 0x0)
	{
		m_busvid_intenable |= BUS_INT_VID_POTUNIT;
		m_int_enable_cb(1);
	}
}

void solo_asic_video_device::reg_90a4_w(uint32_t data)
{
	solo_asic_video_device::reg_90a8_w(data);
	m_pot_intenable &= (~data);
}

uint32_t solo_asic_video_device::reg_90a0_r()
{
	return m_pot_intstat;
}

uint32_t solo_asic_video_device::reg_90a8_r()
{
	return m_pot_intstat;
}

void solo_asic_video_device::reg_90a8_w(uint32_t data)
{
	solo_asic_video_device::set_video_irq(BUS_INT_VID_POTUNIT, data, 0);
}

uint32_t solo_asic_video_device::reg_90ac_r()
{
	return m_screen->vpos();
}

void solo_asic_video_device::set_video_irq(uint32_t mask, uint32_t sub_mask, int state)
{
	if (m_busvid_intenable & mask)
	{
		uint32_t sub_intstat = 0x00;

		switch (mask)
		{
			case BUS_INT_VID_DIVUNIT:
				if (m_div_intenable & sub_mask)
				{
					if (state)
						m_div_intstat |= sub_mask;
					else
						m_div_intstat &= (~sub_mask);
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_div_intstat;
				break;

			case BUS_INT_VID_GFXUNIT:
				if (m_gfx_intenable & sub_mask)
				{
					if (state)
						m_gfx_intstat |= sub_mask;
					else
						m_gfx_intstat &= (~sub_mask);
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_gfx_intstat;
				break;

			case BUS_INT_VID_POTUNIT:
				if (m_pot_intenable & sub_mask)
				{
					if (state)
						m_pot_intstat |= sub_mask;
					else
						m_pot_intstat &= (~sub_mask);
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_pot_intstat;
				break;

			case BUS_INT_VID_VIDUNIT:
				if (m_vid_intenable & sub_mask)
				{
					if (state)
						m_vid_intstat |= sub_mask;
					else
						m_vid_intstat &= (~sub_mask);
				}
				else
				{
					state = 0;
				}
				sub_intstat = m_vid_intstat;
				break;
		}

		if (state)
		{
			m_busvid_intstat |= mask;

			m_int_irq_cb(state);
		}
		else if(sub_intstat == 0x0)
		{
			m_busvid_intstat &= (~mask);

			if(m_busvid_intstat == 0x0)
			{
				m_int_irq_cb(state);
			}
		}
	}
}

inline void solo_asic_video_device::draw_pixel(gfx_cel_t *cel, uint8_t a, int32_t r, int32_t g, int32_t b, uint32_t **out)
{
	gfx_alpha_type_t alpha_type;

	if (cel != NULL)
	{
		alpha_type = cel->alpha_type();

		if (a == ALPHA_OPAQUE)
		{
			a = cel->global_alpha();
		}
		else if (cel->global_alpha() != ALPHA_OPAQUE)
		{
			a = std::clamp(a * cel->global_alpha(), 0x00, 0xff);
		}
	}
	else
	{
		alpha_type = gfx_alpha_type_t::ALPHA_TYPE_BG_TRANSPARENT;
	}

	if (a != ALPHA_TRANSPARENT)
	{
		if (a != ALPHA_OPAQUE)
		{
			int32_t fg_alpha = a;
			int32_t bg_alpha = (~a) & ALPHA_MASK;

			switch (alpha_type)
			{
				case ALPHA_TYPE_BLEND:
				{
					int32_t r_bg = ((*(*out) >> 0x10) & 0xff);
					int32_t g_bg = ((*(*out) >> 0x08) & 0xff);
					int32_t b_bg = ((*(*out) >> 0x00) & 0xff);

					r = ((fg_alpha * r) + (bg_alpha * r_bg)) / ALPHA_MASK;
					g = ((fg_alpha * g) + (bg_alpha * g_bg)) / ALPHA_MASK;
					b = ((fg_alpha * b) + (bg_alpha * b_bg)) / ALPHA_MASK;
					break;
				}

				case ALPHA_TYPE_BG_OPAQUE:
				{
					int32_t r_bg = ((*(*out) >> 0x10) & 0xff);
					int32_t g_bg = ((*(*out) >> 0x08) & 0xff);
					int32_t b_bg = ((*(*out) >> 0x00) & 0xff);

					r = ((fg_alpha * r) + (fg_alpha * r_bg)) / ALPHA_MASK;
					g = ((fg_alpha * g) + (fg_alpha * g_bg)) / ALPHA_MASK;
					b = ((fg_alpha * b) + (fg_alpha * b_bg)) / ALPHA_MASK;
					break;
				}

				case ALPHA_TYPE_BG_TRANSPARENT:
				{
					r = (fg_alpha * r) / ALPHA_MASK;
					g = (fg_alpha * g) / ALPHA_MASK;
					b = (fg_alpha * b) / ALPHA_MASK;
					break;
				}

				case ALPHA_TYPE_BRIGHTEN:
				{
					if (cel != NULL)
					{
						int32_t y  = cel->y_blend_color();
						int32_t Cr = ((cel->cr_blend_color() + UV_OFFSET) & 0xff) - UV_OFFSET;
						int32_t Cb = ((cel->cb_blend_color() + UV_OFFSET) & 0xff) - UV_OFFSET;
					
						y = ((((y + Y_BLACK) << 0x08) + UV_OFFSET) / Y_RANGE);
						int32_t r_bg = y + (((0x166 * Cr) + UV_OFFSET) >> 0x08);
						int32_t b_bg = y - (((0x1C7 * Cb) + UV_OFFSET) >> 0x08);
						int32_t g_bg = y + (((0x32 * b_bg) + (0x83 * r_bg) + UV_OFFSET) >> 0x08);
					
						r = ((fg_alpha * r) + (bg_alpha * r_bg)) / ALPHA_MASK;
						g = ((fg_alpha * g) + (bg_alpha * g_bg)) / ALPHA_MASK;
						b = ((fg_alpha * b) + (bg_alpha * b_bg)) / ALPHA_MASK;
					}
					break;
				}
				
				default:
				{
					break;
				}
			}
		}

		*(*out) = (
			std::clamp(r, 0x00, 0xff) << 0x10
			| std::clamp(g, 0x00, 0xff) << 0x08
			| std::clamp(b, 0x00, 0xff) << 0x00
		);
	}
}

inline void solo_asic_video_device::draw444(gfx_cel_t *cel, int8_t offset, uint32_t in0, uint32_t in1, uint32_t **out)
{
	int32_t a0  = (  in0 >> 0x08) & 0xff;
	int32_t y0  = (  in0 >> 0x18) & 0xff;
	if (a0 != ALPHA_TRANSPARENT && y0 != Y_TRANSPARENT && offset != 1)
	{
		int32_t Cr0 = (((in0 >> 0x00) + UV_OFFSET) & 0xff) - UV_OFFSET;
		int32_t Cb0 = (((in0 >> 0x10) + UV_OFFSET) & 0xff) - UV_OFFSET;
		y0 = ((((y0 + Y_BLACK) << 0x08) + UV_OFFSET) / Y_RANGE);
		int32_t r0 = (((0x166 * Cr0) + UV_OFFSET) >> 0x08);
		int32_t b0 = (((0x1C7 * Cb0) + UV_OFFSET) >> 0x08);
		int32_t g0 = (((0x32 * b0) + (0x83 * r0) + UV_OFFSET) >> 0x08);
		solo_asic_video_device::draw_pixel(cel, a0, (y0 + r0), (y0 - g0), (y0 + b0), out);
	}
	(*out)++;

	int32_t a1  = (  in1 >> 0x08) & 0xff;
	int32_t y1  = (  in1 >> 0x18) & 0xff;
	if (a1 != ALPHA_TRANSPARENT && y1 != Y_TRANSPARENT && offset != -1)
	{
		int32_t Cr1 = (((in1 >> 0x00) + UV_OFFSET) & 0xff) - UV_OFFSET;
		int32_t Cb1 = (((in1 >> 0x10) + UV_OFFSET) & 0xff) - UV_OFFSET;
		y1 = ((((y1 + Y_BLACK) << 0x08) + UV_OFFSET) / Y_RANGE);
		int32_t r1 = (((0x166 * Cr1) + UV_OFFSET) >> 0x08);
		int32_t b1 = (((0x1C7 * Cb1) + UV_OFFSET) >> 0x08);
		int32_t g1 = (((0x32 * b1) + (0x83 * r1) + UV_OFFSET) >> 0x08);
		solo_asic_video_device::draw_pixel(cel, a1, (y1 + r1), (y1 - g1), (y1 + b1), out);
	}
	(*out)++;
}

inline void solo_asic_video_device::draw422(gfx_cel_t *cel, int8_t offset, uint32_t in, uint32_t **out)
{
	int32_t Cb = ((in >> 0x10) & 0xff) - UV_OFFSET;
	int32_t Cr = ((in >> 0x00) & 0xff) - UV_OFFSET;

	int32_t r = ((0x166 * Cr) + UV_OFFSET) >> 0x08;
	int32_t b = ((0x1C7 * Cb) + UV_OFFSET) >> 0x08;
	int32_t g = ((0x32 * b) + (0x83 * r) + UV_OFFSET) >> 0x08;

	int32_t y0 = (in >> 0x18) & 0xff;
	if (y0 != Y_TRANSPARENT && offset != 1)
	{
		uint8_t a0 = 0xff;
		y0 = ((((y0 - Y_BLACK) << 0x08) + UV_OFFSET) / Y_RANGE);
		solo_asic_video_device::draw_pixel(cel, a0, (y0 + r), (y0 - g), (y0 + b), out);
	}
	(*out)++;

	int32_t y1 = (in >> 0x08) & 0xff;
	if (y1 != Y_TRANSPARENT && offset != -1)
	{
		uint8_t a1 = 0xff;
		y1 = ((((y1 - Y_BLACK) << 0x08) + UV_OFFSET) / Y_RANGE);
		solo_asic_video_device::draw_pixel(cel, a1, (y1 + r), (y1 - g), (y1 + b), out);
	}
	(*out)++;
}

inline void solo_asic_video_device::gfxunit_draw_cel(gfx_ymap_t *ymap, gfx_cel_t *cel, screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint32_t codebook_base = cel->codebook_base();

	cel->texdata_start();

	int32_t y_top    = (int32_t)m_pot_draw_vstart + (((int32_t)m_pot_draw_vsize / 2) + ymap->line_top()) + cel->top_offset();
	int32_t y_bottom = std::min((int32_t)(m_pot_draw_vstart + m_pot_draw_vsize), y_top + (ymap->line_cnt() - cel->bottom_offset()));

	if (y_top < (int32_t)m_pot_draw_vstart)
	{
		cel->advance_y_by((int32_t)m_pot_draw_vstart - y_top);
		y_top = (int32_t)m_pot_draw_vstart;
	}

	// Hack to force YDKJ to not draw the last line (the line is broken)
	if(ymap->line_cnt() == 210 && ymap->line_top() >= 2 && ymap->line_top() <= 4)
	{
		y_bottom -= 1;
	}

	for (int32_t y = y_top; y < y_bottom; y++)
	{
		double x_left   = cel->xleftstart()  + (cel->y_offset * cel->dx_left());
		double x_right  = cel->xrightstart() + (cel->y_offset * cel->dx_right());

		int32_t x_start = (int32_t)m_pot_draw_hstart + (                                    ((int32_t)m_pot_draw_hsize / 2) + INTR_TRUNC(x_left ) );
		int32_t x_end   = (int32_t)m_pot_draw_hstart + (std::min((int32_t)m_pot_draw_hsize, ((int32_t)m_pot_draw_hsize / 2) + INTR_TRUNC(x_right)));

		if (x_start < (int32_t)m_pot_draw_hstart)
		{
			cel->advance_x_by((int32_t)m_pot_draw_hstart - x_start);
			x_start = (int32_t)m_pot_draw_hstart;
		}

		uint32_t *line = (&bitmap.pix(y)) + x_start;

		for (int32_t x = x_start; x < x_end; x += 2)
		{
			// There's an odd number or pizels per line. This handles the case where the last iteration only draws one pixel.
			bool one_pixel_only = ((x + 1) == x_end);

			switch (cel->texdata_type())
			{
				case TEXDATA_TYPE_VQ8_422:
				{
					//
					break;
				}

				case TEXDATA_TYPE_DIR_422O:
				case TEXDATA_TYPE_DIR_422A:
				case TEXDATA_TYPE_DIR_422:
				{
					uint32_t colors = m_hostram[cel->texdata_index() >> 0x2];
					cel->advance_x();
					cel->advance_x();

					solo_asic_video_device::draw422(
						cel,
						(one_pixel_only) ? -1 : 0,
						colors,
						&line
					);
					break;
				}

				case TEXDATA_TYPE_VQ8_444:
				{
					uint8_t color_idx0 = m_hostram[cel->texdata_index() >> 0x2] >> (((~cel->texdata_index()) & 0x3) << 0x3);
					cel->advance_x();
					uint8_t color_idx1 = m_hostram[cel->texdata_index() >> 0x2] >> (((~cel->texdata_index()) & 0x3) << 0x3);
					cel->advance_x();

					
					uint32_t color0 = m_hostram[codebook_base + color_idx0];
					uint32_t color1 = m_hostram[codebook_base + color_idx1];

					solo_asic_video_device::draw444(
						cel,
						(one_pixel_only) ? -1 : 0,
						color0,
						color1,
						&line
					);
					break;
				}
		
				case TEXDATA_TYPE_VQ4_444:
				{
					uint8_t colors_idx = m_hostram[cel->texdata_index() >> 0x2] >> (((~cel->texdata_index()) & 0x3) << 0x3);
					cel->advance_x();
					
					uint32_t color0 = m_hostram[codebook_base + ((colors_idx >> 0x4) & 0xf)];
					uint32_t color1 = m_hostram[codebook_base + ((colors_idx >> 0x0) & 0xf)];

					solo_asic_video_device::draw444(
						cel,
						(one_pixel_only) ? -1 : 0,
						color0,
						color1,
						&line
					);
					break;
				}
		
				case TEXDATA_TYPE_DIR_444:
				{
					uint32_t color0 = m_hostram[cel->texdata_index() >> 0x2];
					cel->advance_x();
					uint32_t color1 = m_hostram[cel->texdata_index() >> 0x2];
					cel->advance_x();

					solo_asic_video_device::draw444(
						cel,
						(one_pixel_only) ? -1 : 0,
						color0,
						color1,
						&line
					);
					break;
				}
		
				default:
				{
					//
					break;
				}
			}
		}

		cel->advance_y();
	}
}

inline void solo_asic_video_device::gfxunit_exec_cel_loaddata(gfx_cel_t *cel)
{
	switch (cel->loaddata_type())
	{
		case LOADDATA_TYPE_CODEBOOK:
			// Internal codebook not implemented.
			break;

		case LOADDATA_TYPE_INIT_COLOR:
			// Init color not used.
			m_gfx_initcolor = cel->texdata_base();
			break;

		case gfx_loaddata_type_t::LOADDATA_TYPE_YMAP_BASE:
			m_gfx_ymap_base = (cel->texdata_base() << 2) & (~0xfc000003);
			break;

		case gfx_loaddata_type_t::LOADDATA_TYPE_YMAP_BASE_MASTER:
			m_gfx_ymap_base = m_gfx_ymap_base_master;
			break;

		case gfx_loaddata_type_t::LOADDATA_TYPE_CELS_BASE:
			m_gfx_cels_base = (cel->texdata_base() << 2) & (~0xfc000003);
			break;

		case gfx_loaddata_type_t::LOADDATA_TYPE_CELS_BASE_MASTER:
			m_gfx_cels_base = m_gfx_cels_base_master;
			break;

		default:
			break;
	}
}

inline void solo_asic_video_device::gfxunit_draw_cels(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	m_gfx_ymap_base = m_gfx_ymap_base_master;
	m_gfx_cels_base = m_gfx_cels_base_master;

	uint32_t ymccnt = 0;
	for (uint8_t ymidx = 0; ymidx < GFX_MAX_YMAPS; ymidx++)
	{
		gfx_ymap_t ymap;
		memcpy(ymap.data, &m_hostram[(m_gfx_ymap_base + ymccnt) >> 0x2], sizeof(ymap.data));
		ymccnt += sizeof(ymap.data);

		ymap.index = ymidx;

		if (ymap.disabled())
		{
			continue;
		}
		else if (ymap.line_top() == GFX_TERMINATION_LINE)
		{
			break;
		}
		else
		{
			gfx_cel_t cel = {
				.data = {
					0x00000000,
					0x00000000,
					0x00100000, // duxCenter   = 0x0010
					0xff000000, // globalAlpha = 0x00ff
					0x00000000,
					0x00000000,
					0x00000100, // dvRowAdjust = 0x0100
					0x00000000,
					0x00000000,
					0x00000000,
					0xffff0000, // uMask = 0xff, vMask = 0xff
					0x00dbdbdb  // YBlendColor = 0xdb, CbBlendColor = 0xdb, CrBlendColor = 0xdb
				}
			};

			uint32_t cel_ptr = (m_gfx_cels_base >> 0x2) + (ymap.celblk_ptr() << 0x1);
			int32_t cel_size = ymap.cel_size();

			for (uint8_t clidx = 0; clidx < GFX_MAX_CELS; clidx++)
			{
				cel.index = clidx;

				memcpy(cel.data, &m_hostram[cel_ptr], cel_size);
				
				// Mini cel records share the dux and dvrow_adjust values.
				if(ymap.cel_size() == CELRECORD_SIZE_MINI_CEL)
				{
					cel.dux_to_dvrow_adjust();
				}

				if (cel.texdata_type() == gfx_texdata_type_t::TEXDATA_TYPE_LOADDATA)
					solo_asic_video_device::gfxunit_exec_cel_loaddata(&cel);
				else
					solo_asic_video_device::gfxunit_draw_cel(&ymap, &cel, screen, bitmap, cliprect);

				if (cel.islast())
					break;
				else
					cel_ptr += (cel_size >> 0x2);
			}
		}
	}
}

uint32_t solo_asic_video_device::gfxunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint16_t screen_width = bitmap.width();
	uint16_t screen_height = bitmap.height();

	bool screen_enabled = (m_pot_cntl & POT_FCNTL_EN) && (m_gfx_cntl & GFX_FCNTL_EN);

	m_vid_cstart = m_vid_nstart;
	m_vid_csize = m_vid_nsize;
	m_div_currcfg = m_div_nextcfg;

	for (int y = 0; y < screen_height; y++)
	{
		uint32_t *line = &bitmap.pix(y);

		m_vid_cline = y;

		if (m_vid_cline == m_pot_draw_hintline)
			solo_asic_video_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_HSYNC, 1);

		for (int x = 0; x < screen_width; x += 2)
		{
			uint32_t pixel = POT_DEFAULT_COLOR;

			bool is_active_area = (
				y >= m_pot_draw_vstart
				&& y < (m_pot_draw_vstart + m_pot_draw_vsize)

				&& x >= m_pot_draw_hstart
				&& x < (m_pot_draw_hstart + m_pot_draw_hsize)
			);

			if (screen_enabled && is_active_area)
			{
				*line++ = 0x00000000;
				*line++ = 0x00000000;
			}
			else
			{
				pixel = m_pot_draw_blank_color;
				solo_asic_video_device::draw422(NULL, 0, pixel, &line);
			}

		}
	}

	if (m_gfx_ymap_base_master != 0x80000000)
	{
		solo_asic_video_device::gfxunit_draw_cels(screen, bitmap, cliprect);
	}

	// Write back would happen here (would need to push to a buffer rather than direct to the screen)
	//solo_asic_video_device::set_video_irq(BUS_INT_VID_GFXUNIT, GFX_INT_RANGEINT_WBEOF, 1);

	return 0;
}

uint32_t solo_asic_video_device::vidunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint16_t screen_width = bitmap.width();
	uint16_t screen_height = bitmap.height();
	uint8_t vid_step = (2 * BYTES_PER_PIXEL);
	bool screen_enabled = (m_pot_cntl & POT_FCNTL_EN) && (m_vid_dmacntl & VID_DMACNTL_DMAEN);

	m_vid_cstart = m_vid_nstart;
	m_vid_csize = m_vid_nsize;
	m_vid_ccnt = m_vid_draw_nstart;
	m_div_currcfg = m_div_nextcfg;

	for (int y = 0; y < screen_height; y++)
	{
		uint32_t *line = &bitmap.pix(y);

		m_vid_cline = y;

		if (m_vid_cline == m_pot_draw_hintline)
			solo_asic_video_device::set_video_irq(BUS_INT_VID_POTUNIT, POT_INT_HSYNC, 1);

		for (int x = 0; x < screen_width; x += 2)
		{
			uint32_t pixel = POT_DEFAULT_COLOR;

			bool is_active_area = (
				y >= m_pot_draw_vstart
				&& y < (m_pot_draw_vstart + m_pot_draw_vsize)

				&& x >= m_pot_draw_hstart
				&& x < (m_pot_draw_hstart + m_pot_draw_hsize)
			);

			if (screen_enabled && is_active_area && m_vid_ccnt != 0x80000000)
			{
				pixel = m_hostram[m_vid_ccnt >> 0x2];

				m_vid_ccnt += vid_step;
			}
			else
			{
				pixel = m_pot_draw_blank_color;
			}

			solo_asic_video_device::draw422(NULL, 0, pixel, &line);
		}
	}

	solo_asic_video_device::set_video_irq(BUS_INT_VID_VIDUNIT, VID_INT_DMA, 1);

	return 0;
}

uint32_t solo_asic_video_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	if (m_pot_cntl & POT_FCNTL_USEGFX)
	{
		return gfxunit_screen_update(screen, bitmap, cliprect);
	}
	else
	{
		return vidunit_screen_update(screen, bitmap, cliprect);
	}
}
