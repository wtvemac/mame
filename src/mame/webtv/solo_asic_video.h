// license:BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#ifndef MAME_MACHINE_SOLO_ASIC_VIDEO
#define MAME_MACHINE_SOLO_ASIC_VIDEO

#include "cpu/mips/mips3.h"

#pragma once

// Convert a 16-bit signed number with 8 bits of signed integer and 8 bits of fraction to a float.
// XSIGNED88 is used in the case where the integer and fraction are separated.
#define XSIGNED88(intval, fracval)   ((float)intval + ((float)fracval / (float)(0x1 << 0x8)))
#define SIGNED88(val)                XSIGNED88(((int8_t)(val >> 0x8)), ((uint8_t)(val & 0x00ff)))
// Convert a 10-bit signed integer to a 16-bit signed integer.
#define SIGNED10(val)                (int16_t)((val & 0x01ff) | (0 - (val & 0x0200)))
// Convert a 20-bit signed number with 10 bits of signed integer and 10 bits of fraction to a double.
// XSIGNED1010 is used in the case where the integer and fraction are separated.
#define XSIGNED1010(intval, fracval) ((double)SIGNED10(intval) + ((double)fracval / (double)(0x1 << 0xa)))
#define SIGNED1010(val)              XSIGNED1010(((int16_t)(val >> 0xa)), ((uint16_t)(val & 0x03ff)))
// Convert a double or float to an integer by moving forward to the nearest whole number.
#define INTF_TRUNC(val)                  (int32_t)((val > 0) ? std::ceil(val) : std::floor(val))
#define MASKED_INTF_TRUNC(val, mask)     (int32_t)((int8_t)(INTF_TRUNC(val) & mask))
// Convert a double or float to an integer by moving back to the nearest whole number.
#define INTR_TRUNC(val)                  (int32_t)((val > 0) ? std::floor(val) : std::ceil(val))
#define MASKED_INTR_TRUNC(val, mask)     (int32_t)((int8_t)(INTR_TRUNC(val) & mask))

constexpr uint16_t Y_BLACK         = 0x10;
constexpr uint16_t Y_WHITE         = 0xeb;
constexpr uint16_t Y_RANGE         = (Y_WHITE - Y_BLACK);
constexpr uint16_t UV_OFFSET       = 0x80;
constexpr uint8_t  BYTES_PER_PIXEL = 2;

constexpr uint32_t VID_DMACNTL_ITRLEN = 1 << 3; // interlaced video in DMA channel
constexpr uint32_t VID_DMACNTL_DMAEN  = 1 << 2; // DMA channel enabled
constexpr uint32_t VID_DMACNTL_NV     = 1 << 1; // DMA next registers are valid
constexpr uint32_t VID_DMACNTL_NVF    = 1 << 0; // DMA next registers are always valid

constexpr uint32_t VID_INT_DMA = 1 << 2; // vidUnit DMA completion

constexpr uint32_t GFX_FCNTL_EN          = 1 << 7; // gfxUnit processing enable
constexpr uint32_t GFX_FCNTL_DELTATIME   = 1 << 6; // dx calculation correction
constexpr uint32_t GFX_FCNTL_WAITDISABLE = 1 << 5; // "should always be set to 0"
constexpr uint32_t GFX_FCNTL_WRITEBACKEN = 1 << 4; // 1=Use write-back operation. 0=use ping-pong operation
constexpr uint32_t GFX_FCNTL_FTB         = 1 << 3; // "must always be programmed as 1 for proper write-back operation"
constexpr uint32_t GFX_FCNTL_SOFTRESET   = 1 << 0; // Soft reset gfxUnit

// These are guessed pixel clocks. They were chosen because they cause expected behaviour in emulation.

constexpr uint32_t NTSC_SCREEN_XTAL    = 18393540; // Pixel clock. 480 lines and 640 "pixes" per line @ 60Hz
constexpr uint32_t NTSC_SCREEN_HTOTAL  = 640;      // Total pixels per line (total screen width)
constexpr uint32_t NTSC_SCREEN_HSTART  = 40;       // How many pixel before the active screen starts
constexpr uint32_t NTSC_SCREEN_HSIZE   = 560;      // How many pixels to draw (active screen width)
constexpr uint32_t NTSC_SCREEN_HBSTART = 640;      // How many pixels before the blanking interval starts
constexpr uint32_t NTSC_SCREEN_VTOTAL  = 480;      // Total lines (total screen height)
constexpr uint32_t NTSC_SCREEN_VSTART  = 30;       // How many lines before the active screen starts
constexpr uint32_t NTSC_SCREEN_VSIZE   = 420;      // How many lines to draw (active screen height)
constexpr uint32_t NTSC_SCREEN_VBSTART = 480;      // How many lines before the blanking interval starts

constexpr uint32_t PAL_SCREEN_XTAL    = 21465500; // Pixel clock. 560 lines and 768 "pixes" per line @ 50Hz
constexpr uint32_t PAL_SCREEN_HTOTAL  = 768;      // Total pixels per line (total screen width)
constexpr uint32_t PAL_SCREEN_HSTART  = 72;       // How many pixel before the active screen starts
constexpr uint32_t PAL_SCREEN_HSIZE   = 624;      // How many pixels to draw (active screen width)
constexpr uint32_t PAL_SCREEN_HBSTART = 768;      // How many pixels before the blanking interval starts
constexpr uint32_t PAL_SCREEN_VTOTAL  = 560;      // Total lines (total screen height)
constexpr uint32_t PAL_SCREEN_VSTART  = 40;       // How many lines before the active screen starts
constexpr uint32_t PAL_SCREEN_VSIZE   = 480;      // How many lines to draw (active screen height)
constexpr uint32_t PAL_SCREEN_VBSTART = 560;      // How many lines before the blanking interval starts

constexpr uint32_t POT_DEFAULT_XTAL    = NTSC_SCREEN_XTAL;
constexpr uint32_t POT_DEFAULT_HTOTAL  = NTSC_SCREEN_HTOTAL;
constexpr uint32_t POT_DEFAULT_HSTART  = NTSC_SCREEN_HSTART;
constexpr uint32_t POT_DEFAULT_HBSTART = NTSC_SCREEN_HBSTART;
constexpr uint32_t POT_DEFAULT_HSIZE   = NTSC_SCREEN_HSIZE;
constexpr uint32_t POT_DEFAULT_VTOTAL  = NTSC_SCREEN_VTOTAL;
constexpr uint32_t POT_DEFAULT_VSTART  = NTSC_SCREEN_VSTART;
constexpr uint32_t POT_DEFAULT_VBSTART = NTSC_SCREEN_VBSTART;
constexpr uint32_t POT_DEFAULT_VSIZE   = NTSC_SCREEN_VSIZE;
// This is always 0x77 on for some reason (even on hardware)
// This is needed to correct the HSTART value.
constexpr uint32_t POT_HSTART_OFFSET  = 0x77;
constexpr uint32_t POT_VSTART_OFFSET  = 0x23;

constexpr uint32_t POT_DEFAULT_COLOR   = (UV_OFFSET << 0x10) | (Y_BLACK << 0x08) | UV_OFFSET;

constexpr uint32_t POT_FCNTL_USEGFX444    = 1 << 11; // Use 4:4:4 data from gfxUnit when source from dveUnit
constexpr uint32_t POT_FCNTL_DVECCS       = 1 << 10; // Select wich edge of CrCbSel used to latch GFX->DVE interp
constexpr uint32_t POT_FCNTL_DVEHALFSHIFT = 1 << 9;  // Shift pipeline to dveUnit 1/2 pixel (debug bit)
constexpr uint32_t POT_FCNTL_HINT2XFLINE  = 1 << 8;  // hint is in 2x field lines (off = 1x frame lines)
constexpr uint32_t POT_FCNTL_SOUTEN       = 1 << 7;  // Enable video sync outpuit pins  (DVE_TEN is set needs to be set)
constexpr uint32_t POT_FCNTL_DOUTEN       = 1 << 6;  // Enable video output pins (DVE_TEN is set needs to be set)
constexpr uint32_t POT_FCNTL_HALFSHIFT    = 1 << 5;  // Shifts the external encoder pixel pipeline 1/2 pixel (debug bit)
constexpr uint32_t POT_FCNTL_CRCBINVERT   = 1 << 4;  // invert MSB Cb and Cb
constexpr uint32_t POT_FCNTL_USEGFX       = 1 << 3;  // Use gfxUnit as the video source, rather than vidUnit
constexpr uint32_t POT_FCNTL_SOFTRESET    = 1 << 2;  // Soft reset potUnit
constexpr uint32_t POT_FCNTL_PROGRESSIVE  = 1 << 1;  // progressive video enabled
constexpr uint32_t POT_FCNTL_EN           = 1 << 0;  // potUnit output enable

constexpr uint32_t GFX_INT_RANGEINT_WBEOFL = 1 << 4; // Writeback has finished field
constexpr uint32_t GFX_INT_RANGEINT_OOT    = 1 << 3; // qfxUnit ran out of time compositing line
constexpr uint32_t GFX_INT_RANGEINT_WBEOF  = 1 << 2; // Writeback has finished frame

constexpr uint16_t GFX_TERMINATION_LINE = 0x1ff;
constexpr uint8_t  GFX_MAX_YMAPS        = 255;
constexpr uint8_t  GFX_MAX_CELS         = 255;

constexpr uint32_t POT_INT_VSYNCE = 1 << 5; // even field VSYNC
constexpr uint32_t POT_INT_VSYNCO = 1 << 4; // odd field VSYNC
constexpr uint32_t POT_INT_HSYNC  = 1 << 3; // HSYNC on line specified by VID_HINTLINE
constexpr uint32_t POT_INT_SHIFT  = 1 << 2; // when shiftage occures (no valid pixels from vidUnit when read)

constexpr uint32_t BUS_INT_VID_DIVUNIT = 1 << 5;
constexpr uint32_t BUS_INT_VID_GFXUNIT = 1 << 4;
constexpr uint32_t BUS_INT_VID_POTUNIT = 1 << 3;
constexpr uint32_t BUS_INT_VID_VIDUNIT = 1 << 2;

constexpr int32_t Y_TRANSPARENT     = 0xff;
constexpr int32_t ALPHA_MASK        = 0xff;
constexpr uint8_t ALPHA_TRANSPARENT = 0x00;
constexpr uint8_t ALPHA_OPAQUE      = 0xff;

enum gfx_celrecord_size_t : uint8_t
{
	CELRECORD_SIZE_MICRO_CEL = 0x08, // 64 bits /  8 bytes
	CELRECORD_SIZE_MINI_CEL  = 0x10, // 128 bits / 16 bytes
	CELRECORD_SIZE_FULL_CEL  = 0x30  // 384 bits / 48 bytes
};

constexpr gfx_celrecord_size_t gfx_celrecord_size[] = {
	CELRECORD_SIZE_MICRO_CEL,
	CELRECORD_SIZE_MINI_CEL,
	CELRECORD_SIZE_FULL_CEL
};

enum gfx_texdata_src_t : uint8_t
{
	CEL_TEXMEM_RAM         = 0x00,
	CEL_TEXMEM_ROM         = 0x01,
	CEL_TEXTMEM_WEBTV_PORT = 0x02 // Not used. Would be set when (mode() & 0x1)=0x1
};
enum gfx_loaddata_type_t : uint8_t
{
	LOADDATA_TYPE_CODEBOOK         = 0x00,
	LOADDATA_TYPE_YMAP_BASE        = 0x01,
	LOADDATA_TYPE_CELS_BASE        = 0x02,
	LOADDATA_TYPE_INIT_COLOR       = 0x04,
	LOADDATA_TYPE_YMAP_BASE_MASTER = 0x05,
	LOADDATA_TYPE_CELS_BASE_MASTER = 0x06
};
enum gfx_alpha_type_t : uint8_t
{
	ALPHA_TYPE_BLEND          = 0x00,
	ALPHA_TYPE_BG_OPAQUE      = 0x01,
	ALPHA_TYPE_BG_TRANSPARENT = 0x02,
	ALPHA_TYPE_BRIGHTEN       = 0x03
};
enum gfx_texdata_type_t : uint8_t
{
	TEXDATA_TYPE_VQ8_422  = 0x00,
	TEXDATA_TYPE_DIR_422  = 0x01,
	TEXDATA_TYPE_VQ8_444  = 0x02,
	TEXDATA_TYPE_DIR_444  = 0x03,
	TEXDATA_TYPE_DIR_422O = 0x04,
	TEXDATA_TYPE_DIR_422A = 0x05,
	TEXDATA_TYPE_LOADDATA = 0x06,
	TEXDATA_TYPE_VQ4_444  = 0x07
};

typedef struct gfx_ymap // 32 bits / 4 bytes
{
	uint32_t data[1];

	uint8_t index;

	uint16_t             line_cnt()   const {                       return ((data[0x0] >> 0x17) & 0x0001ff); } //  9 bits
	int16_t              line_top()   const {                       return SIGNED10(data[0x0] >> 0x0d);      } // 10 bits
	bool                 disabled()   const {                       return ((data[0x0] >> 0x0c) & 0x000001); } //  1 bit
	gfx_celrecord_size_t cel_size()   const {     return gfx_celrecord_size[(data[0x0] >> 0x0a) & 0x000003]; } //  2 bits
	uint16_t             celblk_ptr() const {                       return ((data[0x0] >> 0x00) & 0x0003ff); } // 10 bits
} gfx_ymap_t;

typedef struct gfx_cel // 384 bits / 48 bytes
{
	uint32_t data[12];

	uint8_t index;

	uint8_t              mode()                  const {          return ((data[0x0] >> 0x18) & 0x0000ff); } //  8 bits
	bool                 islast()            const { return                (bool)((mode() >> 0x7) & 0x1); } //  1 bit
	gfx_texdata_type_t   texdata_type()      const { return  (gfx_texdata_type_t)((mode() >> 0x1) & 0x7); } //  3 bits
	gfx_texdata_src_t    texdata_src()       const { return   (gfx_texdata_src_t)((mode() >> 0x6) & 0x1); } //  1 bit
	gfx_alpha_type_t     alpha_type()        const { return    (gfx_alpha_type_t)((mode() >> 0x4) & 0x3); } //  2 bits
	gfx_loaddata_type_t  loaddata_type()     const { return (gfx_loaddata_type_t)((mode() >> 0x4) & 0x7); } //  3 bits

	uint8_t              top_offset()            const {          return ((data[0x0] >> 0x10) & 0x0000ff); } //  8 bits
	// 
	// reserved                                                                                                  2 bits
	uint32_t             rowbytes()              const {          return ((rowlongs() + 1) << 0x2);        }
	uint16_t             rowlongs()              const {  return ((rowlongs_hi() << 0x4) | rowlongs_lo()); } // 12 bits
	uint8_t              rowlongs_lo()           const {          return ((data[0x0] >> 0x0a) & 0x00000f); } //  4 bits
	uint16_t             xleftstart_int()        const {          return ((data[0x0] >> 0x00) & 0x0003ff); } // 10 bits
	int16_t              xleftstart_sint()       const {          return SIGNED10(xleftstart_int());       } // 10 bits
	double               xleftstart()  const {  return XSIGNED1010(xleftstart_int(),  xleftstart_frac());  } // 10 bits
	uint8_t              bottom_offset()         const {          return ((data[0x1] >> 0x18) & 0x0000ff); } //  8 bits
	uint32_t             texdata_base()          const {          return ((data[0x1] >> 0x00) & 0xffffff); } // 24 bits
	/// micro cell end ///
	uint8_t              rowlongs_hi()           const {          return ((data[0x2] >> 0x18) & 0x0000ff); } //  8 bits
	uint32_t             dux_raw()               const { return ((((duxMSN() << 0xc) | (dux_center() << 0x4) | (duxLSN() << 0x0))) & 0xffff); } //  16 bits
	float                dux()                   const {          return SIGNED88(dux_raw());              } //  16 bits
	uint8_t              dux_center()            const {          return ((data[0x2] >> 0x10) & 0x0000ff); } //  8 bits
	// reserved                                                                                                  6 bits
	uint16_t             xrightstart_int()       const {          return ((data[0x2] >> 0x00) & 0x0003ff); } // 10 bits
	int16_t              xrightstart_sint()      const {          return SIGNED10(xrightstart_int());      } // 10 bits
	double               xrightstart() const  { return XSIGNED1010(xrightstart_int(), xrightstart_frac()); } // 10 bits
	uint8_t              global_alpha()          const {          return ((data[0x3] >> 0x18) & 0x0000ff); } //  8 bits
	uint32_t             codebook_base()         const {          return ((data[0x3] >> 0x00) & 0xffffff); } // 24 bits
	/// mini cell end ///
	uint32_t             ustart_raw()            const {          return ((data[0x4] >> 0x10) & 0x00ffff); } // 16 bits
	float                ustart()                const {          return SIGNED88(ustart_raw());           } // 16 bits
	uint32_t             durow_adjust_raw()      const {          return ((data[0x4] >> 0x00) & 0x00ffff); } // 16 bits
	float                durow_adjust()          const {          return SIGNED88(durow_adjust_raw());     } // 16 bits
	// reserved                                                                                                 16 bits
	// reserved                                                                                                 16 bits
	uint32_t             vstart_raw()            const {          return ((data[0x6] >> 0x10) & 0x00ffff); } // 16 bits
	float                vstart()                const {          return SIGNED88(vstart_raw());           } // 16 bits
	uint32_t             dvrow_adjust_raw()      const {          return ((data[0x6] >> 0x00) & 0x00ffff); } // 16 bits
	float                dvrow_adjust()          const {          return SIGNED88(dvrow_adjust_raw());     } // 16 bits
	// reserved                                                                                                  2 bits
	uint16_t             xleftstart_frac()       const {          return ((data[0x7] >> 0x14) & 0x0003ff); } // 10 bits
	uint32_t             dx_left_raw()           const {          return ((data[0x7] >> 0x00) & 0x0fffff); } // 20 bits
	double               dx_left()               const {          return SIGNED1010(dx_left_raw());        } // 20 bits
	// reserved                                                                                                  2 bits
	uint16_t             xrightstart_frac()      const {          return ((data[0x8] >> 0x14) & 0x0003ff); } // 10 bits
	uint32_t             dx_right_raw()          const {          return ((data[0x8] >> 0x00) & 0x0fffff); } // 20 bits
	double               dx_right()              const {          return SIGNED1010(dx_right_raw());       } // 20 bits
	// reserved                                                                                                 16 bits
	uint32_t             dvx_raw()               const {          return ((data[0x9] >> 0x00) & 0x00ffff); } // 16 bits
	float                dvx()                   const {          return SIGNED88(dvx_raw());              } // 16 bits
	uint8_t              umask()                 const {          return ((data[0xa] >> 0x18) & 0x0000ff); } //  8 bits
	uint8_t              vmask()                 const {          return ((data[0xa] >> 0x10) & 0x0000ff); } //  8 bits
	int8_t               duxMSN()                const {          return ((data[0xa] >> 0x0c) & 0x00000f); } //  4 bits
	int8_t               duxLSN()                const {          return ((data[0xa] >> 0x08) & 0x00000f); } //  4 bits
	// reserved                                                                                                  8 bits
	// reserved                                                                                                  8 bits
	uint8_t              y_blend_color()         const {          return ((data[0xb] >> 0x10) & 0x0000ff); } //  8 bits
	uint8_t              cb_blend_color()        const {          return ((data[0xb] >> 0x08) & 0x0000ff); } //  8 bits
	uint8_t              cr_blend_color()        const {          return ((data[0xb] >> 0x00) & 0x0000ff); } //  8 bits

	float y_offset;
	float u;
	float v;

	void dux_to_dvrow_adjust()
	{
		data[6] = (data[6] & 0xffff0000) | (((data[2] >> 0x10) & 0xff) << 0x4);
	}

	uint32_t texdata_index()
	{
		int32_t base = -1;

		switch (texdata_type())
		{
			case TEXDATA_TYPE_VQ8_422:
				base = ((int32_t)(texdata_base() << 0x02) + (((MASKED_INTF_TRUNC(v, vmask()) / 0x02) * (int32_t)rowbytes()) + (MASKED_INTF_TRUNC(u, umask()) / 0x02)));
				break;

			case TEXDATA_TYPE_DIR_422O:
			case TEXDATA_TYPE_DIR_422A:
			case TEXDATA_TYPE_DIR_422:
				base = ((int32_t)(texdata_base() << 0x02) + (((MASKED_INTF_TRUNC(v, vmask()) * 0x01) * (int32_t)rowbytes()) + (MASKED_INTF_TRUNC(u, umask()) * 0x02)));
				break;

			case TEXDATA_TYPE_VQ8_444:
				base = ((int32_t)(texdata_base() << 0x02) + (((MASKED_INTF_TRUNC(v, vmask()) * 0x01) * (int32_t)rowbytes()) + (MASKED_INTF_TRUNC(u, umask()) * 0x01)));
				break;

			case TEXDATA_TYPE_VQ4_444:
				base = ((int32_t)(texdata_base() << 0x02) + (((MASKED_INTF_TRUNC(v, vmask()) * 0x01) * (int32_t)rowbytes()) + (MASKED_INTF_TRUNC(u, umask()) / 0x02)));
				break;

			case TEXDATA_TYPE_DIR_444:
				base = ((int32_t)(texdata_base() << 0x02) + (((MASKED_INTF_TRUNC(v, vmask()) * 0x01) * (int32_t)rowbytes()) + (MASKED_INTF_TRUNC(u, umask()) * 0x04)));
				break;

			default:
				//
				break;
		}

		if (base < 0)
			return (texdata_base() << 0x02);
		else
			return (uint32_t)base;
	}
	void texdata_start()
	{
		y_offset = 0.00;

		u = ustart();
		v = vstart();
	}
	void advance_x()
	{
		u += dux();
		v += dvx();
	}
	void advance_x_by(uint8_t amount)
	{
		u += dux() * amount;
		v += dvx() * amount;
	}
	void advance_y()
	{
		y_offset++;

		u = ustart() + (y_offset * durow_adjust());
		v = vstart() + (y_offset * dvrow_adjust());
	}
	void advance_y_by(uint8_t amount)
	{
		y_offset += amount;

		u = ustart() + (y_offset * durow_adjust() * amount);
		v = vstart() + (y_offset * dvrow_adjust() * amount);
	}
} gfx_cel_t;

class solo_asic_video_device : public device_t, public device_video_interface
{
public:
	// construction/destruction
	solo_asic_video_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	void map(address_map &map);
	void vid_unit_map(address_map &map);
	void gfx_unit_map(address_map &map);
	void dve_unit_map(address_map &map);
	void div_unit_map(address_map &map);
	void pot_unit_map(address_map &map);
	void mpeg_unit_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_hostram(T &&tag) { m_hostram.set_tag(std::forward<T>(tag)); }

	auto int_enable_callback() { return m_int_enable_cb.bind(); }
	auto int_irq_callback() { return m_int_irq_cb.bind(); }

	void enable_ntsc();
	void enable_pal();

	uint32_t busvid_intenable_get();
	void busvid_intenable_set(uint32_t data);
	void busvid_intenable_clear(uint32_t data);

	uint32_t busvid_intstat_get();
	void busvid_intstat_set(uint32_t data);
	void busvid_intstat_clear(uint32_t data);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;
	virtual void device_add_mconfig(machine_config &config) override;

	bool m_use_pal = false;

	uint32_t m_busvid_intenable;
	uint32_t m_busvid_intstat;

	uint32_t m_gfx_cntl;
	uint32_t m_gfx_oot_ycount;
	uint32_t m_gfx_ymap_base;
	uint32_t m_gfx_ymap_base_master;
	uint32_t m_gfx_cels_base;
	uint32_t m_gfx_cels_base_master;
	uint32_t m_gfx_initcolor;
	uint32_t m_gfx_ycounter_init;
	uint32_t m_gfx_pausecycles;
	uint32_t m_gfx_oot_cels_base;
	uint32_t m_gfx_oot_ymap_base;
	uint32_t m_gfx_oot_cels_offset;
	uint32_t m_gfx_oot_ymap_count;
	uint32_t m_gfx_termcycle_count;
	uint32_t m_gfx_hcounter_init;
	uint32_t m_gfx_blanklines;
	uint32_t m_gfx_activelines;
	uint32_t m_gfx_wbdstart;
	uint32_t m_gfx_wbdlsize;
	uint32_t m_gfx_wbstride;
	uint32_t m_gfx_wbdconfig;
	uint32_t m_gfx_intenable;
	uint32_t m_gfx_intstat;

	uint32_t m_dve_unknown1;
	uint32_t m_dve_unknown2;
	
	uint32_t m_vid_nstart;
	uint32_t m_vid_nsize;
	uint32_t m_vid_dmacntl;
	uint32_t m_vid_cstart;
	uint32_t m_vid_csize;
	uint32_t m_vid_ccnt;
	uint32_t m_vid_cline;
	uint32_t m_vid_vdata;
	uint32_t m_vid_intenable;
	uint32_t m_vid_intstat;
	
	uint32_t m_div_intenable;
	uint32_t m_div_intstat;
	uint32_t m_div_dmacntl;
	uint32_t m_div_nextcfg;
	uint32_t m_div_currcfg;
	
	uint8_t m_pot_cntl;
	uint32_t m_pot_hintline;
	uint32_t m_pot_vstart;
	uint32_t m_pot_vsize;
	uint32_t m_pot_blank_color;
	uint32_t m_pot_hstart;
	uint32_t m_pot_hsize;
	uint32_t m_pot_intenable;
	uint32_t m_pot_intstat;
	
	// Values set from software are corrected then stored here to draw the actual screen.
	uint32_t m_vid_draw_nstart;
	uint32_t m_pot_draw_hstart;
	uint32_t m_pot_draw_hsize;
	uint32_t m_pot_draw_vstart;
	uint32_t m_pot_draw_vsize;
	uint32_t m_pot_draw_blank_color;
	uint32_t m_pot_draw_hintline;

private:
	required_device<mips3_device> m_hostcpu;
	required_shared_ptr<uint32_t> m_hostram;
	required_device<screen_device> m_screen;

	devcb_write_line m_int_enable_cb;
	devcb_write_line m_int_irq_cb;

	void vblank_irq(int state);
	void set_video_irq(uint32_t mask, uint32_t sub_mask, int state);

	inline void draw_pixel(gfx_cel_t *cel, uint8_t a, int32_t r, int32_t g, int32_t b, uint32_t **out);
	inline void draw444(gfx_cel_t *cel, int8_t offset, uint32_t in0, uint32_t in1, uint32_t **out);
	inline void draw422(gfx_cel_t *cel, int8_t offset, uint32_t in, uint32_t **out);

	inline void gfxunit_draw_cel(gfx_ymap_t *ymap, gfx_cel_t *cel, screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	inline void gfxunit_exec_cel_loaddata(gfx_cel_t *cel);
	inline void gfxunit_draw_cels(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	uint32_t gfxunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	uint32_t vidunit_screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	void validate_active_area();

	/* gfxUnit registers */

	uint32_t reg_6004_r();          // GFX_CONTROL (read)
	void reg_6004_w(uint32_t data); // GFX_CONTROL (write)
	uint32_t reg_6010_r();          // GFX_OOTYCOUNT (read)
	void reg_6010_w(uint32_t data); // GFX_OOTYCOUNT (write)
	uint32_t reg_6014_r();          // GFX_CELSBASE (read)
	void reg_6014_w(uint32_t data); // GFX_CELSBASE (write)
	uint32_t reg_6018_r();          // GFX_YMAPBASE (read)
	void reg_6018_w(uint32_t data); // GFX_YMAPBASE (write)
	uint32_t reg_601c_r();          // GFX_CELSBASEMASTER (read)
	void reg_601c_w(uint32_t data); // GFX_CELSBASEMASTER (write)
	uint32_t reg_6020_r();          // GFX_YMAPBASEMASTER (read)
	void reg_6020_w(uint32_t data); // GFX_YMAPBASEMASTER (write)
	uint32_t reg_6024_r();          // GFX_INITCOLOR (read)
	void reg_6024_w(uint32_t data); // GFX_INITCOLOR (write)
	uint32_t reg_6028_r();          // GFX_YCOUNTERINlT (read)
	void reg_6028_w(uint32_t data); // GFX_YCOUNTERINlT (write)
	uint32_t reg_602c_r();          // GFX_PAUSECYCLES (read)
	void reg_602c_w(uint32_t data); // GFX_PAUSECYCLES (write)
	uint32_t reg_6030_r();          // GFX_OOTCELSBASE (read)
	void reg_6030_w(uint32_t data); // GFX_OOTCELSBASE (write)
	uint32_t reg_6034_r();          // GFX_OOTYMAPBASE (read)
	void reg_6034_w(uint32_t data); // GFX_OOTYMAPBASE (write)
	uint32_t reg_6038_r();          // GFX_OOTCELSOFFSET (read)
	void reg_6038_w(uint32_t data); // GFX_OOTCELSOFFSET (write)
	uint32_t reg_603c_r();          // GFX_OOTYMAPCOUNT (read)
	void reg_603c_w(uint32_t data); // GFX_OOTYMAPCOUNT (write)
	uint32_t reg_6040_r();          // GFX_TERMCYCLECOUNT (read)
	void reg_6040_w(uint32_t data); // GFX_TERMCYCLECOUNT (write)
	uint32_t reg_6044_r();          // GFX_HCOUNTERINIT (read)
	void reg_6044_w(uint32_t data); // GFX_HCOUNTERINIT (write)
	uint32_t reg_6048_r();          // GFX_BLANKLINES (read)
	void reg_6048_w(uint32_t data); // GFX_BLANKLINES (write)
	uint32_t reg_604c_r();          // GFX_ACTIVELINES (read)
	void reg_604c_w(uint32_t data); // GFX_ACTIVELINES (write)
	uint32_t reg_6060_r();          // GFX_INTEN (read)
	void reg_6060_w(uint32_t data); // GFX_INTEN (write)
	void reg_6064_w(uint32_t data); // GFX_INTEN_C (write-only)
	uint32_t reg_6068_r();          // GFX_INTSTAT (read)
	void reg_6068_w(uint32_t data); // GFX_INTSTAT (write)
	void reg_606c_w(uint32_t data); // GFX_INTSTAT_C (write-only)
	uint32_t reg_6080_r();          // GFX_WBDSTART (read)
	void reg_6080_w(uint32_t data); // GFX_WBDSTART (write)
	uint32_t reg_6084_r();          // GFX_WBDLSIZE (read)
	void reg_6084_w(uint32_t data); // GFX_WBDLSIZE (write)
	uint32_t reg_608c_r();          // GFX_WBSTRIDE (read)
	void reg_608c_w(uint32_t data); // GFX_WBSTRIDE (write)
	uint32_t reg_6090_r();          // GFX_WBDCONFIG (read)
	void reg_6090_w(uint32_t data); // GFX_WBDCONFIG (write)
	uint32_t reg_6094_r();          // GFX_WBDSTART (read)
	void reg_6094_w(uint32_t data); // GFX_WBDSTART (write)

	/* vidUnit registers */

	uint32_t reg_3000_r();          // VID_CSTART (read-only)
	uint32_t reg_3004_r();          // VID_CSIZE (read-only)
	uint32_t reg_3008_r();          // VID_CCNT (read-only)
	uint32_t reg_300c_r();          // VID_NSTART (read)
	void reg_300c_w(uint32_t data); // VID_NSTART (write)
	uint32_t reg_3010_r();          // VID_NSIZE (read)
	void reg_3010_w(uint32_t data); // VID_NSIZE (write)
	uint32_t reg_3014_r();          // VID_DMACNTL (read)
	void reg_3014_w(uint32_t data); // VID_DMACNTL (write)
	uint32_t reg_3038_r();          // VID_INTSTAT (read)
	void reg_3138_w(uint32_t data); // VID_INTSTAT (clear)
	uint32_t reg_303c_r();          // VID_INTEN_S (read)
	void reg_303c_w(uint32_t data); // VID_INTEN_S (write)
	void reg_313c_w(uint32_t data); // VID_INTEN_C (clear)
	uint32_t reg_3040_r();          // VID_VDATA (read)
	void reg_3040_w(uint32_t data); // VID_VDATA (write)

	/* dveUnit registers */

	uint32_t reg_7024_r();
	void reg_7024_w(uint32_t data);
	uint32_t reg_7028_r();
	void reg_7028_w(uint32_t data);

	/* divUnit registers */

	uint32_t reg_8004_r();
	void reg_8004_w(uint32_t data);
	uint32_t reg_801c_r();
	void reg_801c_w(uint32_t data);
	uint32_t reg_8038_r();
	void reg_8038_w(uint32_t data);
	uint32_t reg_8060_r();
	void reg_8060_w(uint32_t data);

	/* potUnit registers */

	uint32_t reg_9080_r();          // POT_VSTART (read)
	void reg_9080_w(uint32_t data); // POT_VSTART (write)
	uint32_t reg_9084_r();          // POT_VSIZE (read)
	void reg_9084_w(uint32_t data); // POT_VSIZE (write)
	uint32_t reg_9088_r();          // POT_BLNKCOL (read)
	void reg_9088_w(uint32_t data); // POT_BLNKCOL (write)
	uint32_t reg_908c_r();          // POT_HSTART (read)
	void reg_908c_w(uint32_t data); // POT_HSTART (write)
	uint32_t reg_9090_r();          // POT_HSIZE (read)
	void reg_9090_w(uint32_t data); // POT_HSIZE (write)
	uint32_t reg_9094_r();          // POT_CNTL (read)
	void reg_9094_w(uint32_t data); // POT_CNTL (write)
	uint32_t reg_9098_r();          // POT_HINTLINE (read)
	void reg_9098_w(uint32_t data); // POT_HINTLINE (write)
	uint32_t reg_909c_r();          // POT_INTEN   (read)
	void reg_909c_w(uint32_t data); // POT_INTEN_S (write)
	void reg_90a4_w(uint32_t data); // POT_INTEN_C (write-only)
	uint32_t reg_90a0_r();          // POT_INTSTAT (read)
	void reg_90a8_w(uint32_t data); // POT_INTSTAT_C (write)
	uint32_t reg_90a8_r();          // POT_INTSTAT_C (read)
	uint32_t reg_90ac_r();          // POT_CLINE (read)

	/* mpegUnit registers */

	uint32_t reg_d000_r();          // Unknown Solo2 register. MPEG revision? (read)

};

DECLARE_DEVICE_TYPE(SOLO_ASIC_VIDEO, solo_asic_video_device)

#endif // MAME_MACHINE_SOLO_ASIC_VIDEO