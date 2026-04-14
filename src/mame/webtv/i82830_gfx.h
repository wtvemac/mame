// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82830_GFX_H
#define MAME_WEBTV_I82830_GFX_H

#pragma once

#include "machine/pci.h"
#include "i82830_host.h"
#include "i82830_vga.h"
#include "i82801_lpc.h"

class i82830_graphics_device : public agp_device
{

public:

	static constexpr uint32_t GFX_GMADR_128MEM_BASE_MASK = 0xf8000000;
	static constexpr uint32_t GFX_GMADR_64MEM_BASE_MASK  = 0xfc000000;
	static constexpr uint32_t GFX_MMADR_MEM_BASE_MASK    = 0xfff80000;
	static constexpr uint32_t GFX_BASE_RTE               = 0x00000001;
	static constexpr uint32_t GFX_BASE_IS_IO             = 1 << 0;
	static constexpr uint32_t GFX_BASE_IS_MEM            = 0 << 0;
	static constexpr uint32_t GFX_MEM_TYPE_MASK          = 0x00000006;
	static constexpr uint32_t GFX_MEM_TYPE_ANY           = 0 << 1;
	static constexpr uint32_t GFX_MEM_PREFETCHABLE       = 1 << 3;

	static constexpr uint32_t MM_SIZE                     = 0x80000; // 512kB

	// Instruction and Interrupt Control Registers (0x01000h-0x02FFF)
	static constexpr uint32_t MM_CNTL_FENCE                = 0x02000 >> 2;
	static constexpr uint32_t MM_CNTL_PGTBL                = 0x02020 >> 2;
	static constexpr uint32_t MM_CNTL_PGTBL_ER             = 0x02024 >> 2;
	static constexpr uint32_t MM_CNTL_RINGBUF_MAX_SIZE     = 0x00100000;
	static constexpr uint32_t MM_CNTL_RINGBUF_INS_SIZE     = 0x00000004;
	static constexpr uint32_t MM_CNTL_RINGBUF_TAIL_OFFSET  = 0x00000000;
	static constexpr uint32_t MM_CNTL_RINGBUF_HEAD_OFFSET  = 0x00000001;
	static constexpr uint32_t MM_CNTL_RINGBUF_START_OFFSET = 0x00000002;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_OFFSET  = 0x00000003;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_LENM    = 0x001ff000;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_RMASK   = 0x00000006;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_R16     = 1 << 1;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_R32     = 3 << 1;
	static constexpr uint32_t MM_CNTL_RINGBUF_CNTL_EN      = 1 << 0;
	static constexpr uint32_t MM_CNTL_PRINGBUF             = 0x02030 >> 2;
	static constexpr uint32_t MM_CNTL_PRINGBUF_TAIL        = (i82830_graphics_device::MM_CNTL_PRINGBUF + i82830_graphics_device::MM_CNTL_RINGBUF_TAIL_OFFSET);
	static constexpr uint32_t MM_CNTL_PRINGBUF_HEAD        = (i82830_graphics_device::MM_CNTL_PRINGBUF + i82830_graphics_device::MM_CNTL_RINGBUF_HEAD_OFFSET);
	static constexpr uint32_t MM_CNTL_PRINGBUF_START       = (i82830_graphics_device::MM_CNTL_PRINGBUF + i82830_graphics_device::MM_CNTL_RINGBUF_START_OFFSET);
	static constexpr uint32_t MM_CNTL_PRINGBUF_CNTL        = (i82830_graphics_device::MM_CNTL_PRINGBUF + i82830_graphics_device::MM_CNTL_RINGBUF_CNTL_OFFSET);
	static constexpr uint32_t MM_CNTL_HWS_PGA              = 0x02080 >> 2;
	static constexpr uint32_t MM_CNTL_IPEIR                = 0x02088 >> 2;
	static constexpr uint32_t MM_CNTL_IPEHR                = 0x0208c >> 2;
	static constexpr uint32_t MM_CNTL_INSTDONE             = 0x02090 >> 2;
	static constexpr uint32_t MM_CNTL_NOPID                = 0x02094 >> 2;
	static constexpr uint32_t MM_CNTL_HWSTAM               = 0x02098 >> 2;
	static constexpr uint32_t MM_CNTL_IER                  = 0x020a0 >> 2;
	static constexpr uint32_t MM_CNTL_IIR                  = 0x020a4 >> 2;
	static constexpr uint32_t MM_CNTL_IMR                  = 0x020a8 >> 2;
	static constexpr uint32_t MM_CNTL_ISR                  = 0x020ac >> 2;
	static constexpr uint32_t MM_CNTL_EIR                  = 0x020b0 >> 2;
	static constexpr uint32_t MM_CNTL_EMR                  = 0x020b4 >> 2;
	static constexpr uint32_t MM_CNTL_ESR                  = 0x020b8 >> 2;
	static constexpr uint32_t MM_CNTL_INSTPM               = 0x020c0 >> 2;
	static constexpr uint32_t MM_CNTL_INSTPS               = 0x020c4 >> 2;
	static constexpr uint32_t MM_CNTL_BBP_PTR              = 0x020c8 >> 2;
	static constexpr uint32_t MM_CNTL_ABB_SRT              = 0x020cc >> 2;
	static constexpr uint32_t MM_CNTL_ABB_END              = 0x020d0 >> 2;
	static constexpr uint32_t MM_CNTL_DMA_FADD             = 0x020d4 >> 2;
	static constexpr uint32_t MM_CNTL_FW_BLC               = 0x020d8 >> 2;
	static constexpr uint32_t MM_CNTL_MEM_MODE             = 0x020dc >> 2;
	// Memory Control Registers (0x03000h-0x03FFF)
	static constexpr uint32_t MM_MEMCNTL_DRT               = 0x03000 >> 2;
	static constexpr uint32_t MM_MEMCNTL_DRAMCL            = 0x03001 >> 2;
	static constexpr uint32_t MM_MEMCNTL_DRAMCH            = 0x03002 >> 2;
	// Span Cursor Registers (0x04000h-0x04FFF)
	static constexpr uint32_t MM_SPANC_UI_SC_CTL           = 0x04008 >> 2;
	// I/O Control Registers (0x05000h-0x05FFF)
	static constexpr uint32_t MM_IOCNTL_HVSYNC             = 0x05000 >> 2;
	static constexpr uint32_t MM_IOCNTL_GPIOA              = 0x05010 >> 2;
	static constexpr uint32_t MM_IOCNTL_GPIOB              = 0x05014 >> 2;
	static constexpr uint32_t MM_IOCNTL_GPIOC              = 0x0501c >> 2;
	// Clock Control and Power Management Registers (0x06000h-0x06FFF)
	static constexpr uint32_t MM_CLKPWR_DCLK_0D            = 0x06000 >> 2;
	static constexpr uint32_t MM_CLKPWR_DCLK_1D            = 0x06004 >> 2;
	static constexpr uint32_t MM_CLKPWR_DCLK_2D            = 0x06008 >> 2;
	static constexpr uint32_t MM_CLKPWR_LCD_CLKD           = 0x0600c >> 2;
	static constexpr uint32_t MM_CLKPWR_DCLK_0DS           = 0x06010 >> 2;
	static constexpr uint32_t MM_DPLLA_CTRL                = 0x06014 >> 2;
	static constexpr uint32_t MM_DPLLB_CTRL                = 0x06018 >> 2;
	// Graphics Translation Table Range Definition (0x10000h-0x2FFFF)
	static constexpr uint32_t MM_GTT_PAGE_TABLE            = 0x10000 >> 2; // Maps to memory @ MM_CNTL_PGTBL
	static constexpr uint32_t MM_GTT_PAGE_TABLE_SIZE       = 0x20000;
	static constexpr uint32_t MM_GTT_PAGE_TABLE_VALID      = 0x00001; // In-table marker for a valid page table entry.
	static constexpr uint32_t MM_GTT_PAGE_INVALID          = 0xffffffff; // Used internally as a return result.
	static constexpr uint32_t MM_GTT_PAGE_SIZE             = 4 * 1024;
	static constexpr uint32_t MM_GTT_PAGE_SIZE_MASK        = (i82830_graphics_device::MM_GTT_PAGE_SIZE - 1);
	// We index mostly using 32-bit (4-byte) chunks, so these are values that are pre-divided to fit within 4-byte boundries rather than 1-byte bounries
	static constexpr uint32_t MM_GTT_PAGE_ADDR32_SIZE      = i82830_graphics_device::MM_GTT_PAGE_SIZE >> 2;
	static constexpr uint32_t MM_GTT_PAGE_ADDR32_SIZE_MASK = (i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE - 1);
	static constexpr uint32_t MM_GTT_PAGE_ADDR32_SHIFT     = (12 - 2);
	static constexpr uint32_t MM_GTT_PAGE_ADDR32_OMASK     = 0x00000fff >> 2;
	// Overlay Registers (0x30000h−0x3FFFF)
	static constexpr uint32_t MM_OVERLAY_OVOADDR           = 0x30000 >> 2;
	static constexpr uint32_t MM_OVERLAY_DOVOSTA           = 0x30008 >> 2;
	static constexpr uint32_t MM_OVERLAY_GAMMA             = 0x30010 >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_0Y           = 0x30100 >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_1Y           = 0x30104 >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_0U           = 0x30108 >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_0V           = 0x3010c >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_1U           = 0x30110 >> 2;
	static constexpr uint32_t MM_OVERLAY_OBUF_1V           = 0x30114 >> 2;
	static constexpr uint32_t MM_OVERLAY_OVOSTRIDE         = 0x30118 >> 2;
	static constexpr uint32_t MM_OVERLAY_YRGB_VPH          = 0x3011c >> 2;
	static constexpr uint32_t MM_OVERLAY_UV_VPH            = 0x30120 >> 2;
	static constexpr uint32_t MM_OVERLAY_HORZ_PH           = 0x30124 >> 2;
	static constexpr uint32_t MM_OVERLAY_INIT_PH           = 0x30128 >> 2;
	static constexpr uint32_t MM_OVERLAY_DWINPOS           = 0x3012c >> 2;
	static constexpr uint32_t MM_OVERLAY_DWINSZ            = 0x30130 >> 2;
	static constexpr uint32_t MM_OVERLAY_SWID              = 0x30134 >> 2;
	static constexpr uint32_t MM_OVERLAY_SWIDQW            = 0x30138 >> 2;
	static constexpr uint32_t MM_OVERLAY_SHEIGHT           = 0x3013f >> 2;
	static constexpr uint32_t MM_OVERLAY_YRGBSCALE         = 0x30140 >> 2;
	static constexpr uint32_t MM_OVERLAY_UVSCALE           = 0x30144 >> 2;
	static constexpr uint32_t MM_OVERLAY_OVOCLRCO          = 0x30148 >> 2;
	static constexpr uint32_t MM_OVERLAY_OVOCLRC1          = 0x3014c >> 2;
	static constexpr uint32_t MM_OVERLAY_DCLRKV            = 0x30150 >> 2;
	static constexpr uint32_t MM_OVERLAY_DLCRKM            = 0x30154 >> 2;
	static constexpr uint32_t MM_OVERLAY_SCLRKVH           = 0x30158 >> 2;
	static constexpr uint32_t MM_OVERLAY_SCLRKVL           = 0x3015c >> 2;
	static constexpr uint32_t MM_OVERLAY_SCLRKM            = 0x30160 >> 2;
	static constexpr uint32_t MM_OVERLAY_OVOCONF           = 0x30164 >> 2;
	static constexpr uint32_t MM_OVERLAY_OVOCMD            = 0x30168 >> 2;
	static constexpr uint32_t MM_OVERLAY_AWINPOS           = 0x30170 >> 2;
	static constexpr uint32_t MM_OVERLAY_AWINZ             = 0x30174 >> 2;
	// BLT Engine Status (Software Debug) registers (0x40000h−0x4FFFF)
	static constexpr uint32_t MM_BLT_BR00                  = 0x40000 >> 2;
	static constexpr uint32_t MM_BLT_BRO1                  = 0x40004 >> 2;
	static constexpr uint32_t MM_BLT_BR02                  = 0x40008 >> 2;
	static constexpr uint32_t MM_BLT_BR03                  = 0x4000c >> 2;
	static constexpr uint32_t MM_BLT_BR04                  = 0x40010 >> 2;
	static constexpr uint32_t MM_BLT_BR05                  = 0x40014 >> 2;
	static constexpr uint32_t MM_BLT_BR06                  = 0x40018 >> 2;
	static constexpr uint32_t MM_BLT_BR07                  = 0x4001c >> 2;
	static constexpr uint32_t MM_BLT_BR08                  = 0x40020 >> 2;
	static constexpr uint32_t MM_BLT_BR09                  = 0x40024 >> 2;
	static constexpr uint32_t MM_BLT_BR10                  = 0x40028 >> 2;
	static constexpr uint32_t MM_BLT_BR11                  = 0x4002c >> 2;
	static constexpr uint32_t MM_BLT_BR12                  = 0x40030 >> 2;
	static constexpr uint32_t MM_BLT_BR13                  = 0x40034 >> 2;
	static constexpr uint32_t MM_BLT_BR14                  = 0x40038 >> 2;
	static constexpr uint32_t MM_BLT_BR15                  = 0x4003c >> 2;
	static constexpr uint32_t MM_BLT_BR16                  = 0x40040 >> 2;
	static constexpr uint32_t MM_BLT_BR17                  = 0x40044 >> 2;
	static constexpr uint32_t MM_BLT_BR18                  = 0x40048 >> 2;
	static constexpr uint32_t MM_BLT_BR19                  = 0x4004c >> 2;
	static constexpr uint32_t MM_BLT_SSLADD                = 0x40074 >> 2;
	static constexpr uint32_t MM_BLT_DSLH                  = 0x40078 >> 2;
	static constexpr uint32_t MM_BLT_DSLRADD               = 0x4007c >> 2;
	// LCD/TV-Out and HW DVD Registers (0x60000h-0x6FFFF)
	static constexpr uint32_t MM_TV_HTOTAL                 = 0x60000 >> 2;
	static constexpr uint32_t MM_TV_HBLANK                 = 0x60004 >> 2;
	static constexpr uint32_t MM_TV_HSYNC                  = 0x60008 >> 2;
	static constexpr uint32_t MM_TV_VTOTAL                 = 0x6000c >> 2;
	static constexpr uint32_t MM_TV_VBLANK                 = 0x60010 >> 2;
	static constexpr uint32_t MM_TV_VSYNC                  = 0x60014 >> 2;
	static constexpr uint32_t MM_TV_LCDTV_C                = 0x60018 >> 2;
	static constexpr uint32_t MM_TV_OVRACT                 = 0x6001c >> 2;
	static constexpr uint32_t MM_TV_BCLRPAT                = 0x60020 >> 2;
	static constexpr uint32_t MM_TV_UNKNOWN1               = 0x61100 >> 2;
	static constexpr uint32_t MM_TV_UNKNOWN2               = 0x61120 >> 2;
	static constexpr uint32_t MM_TV_UNKNOWN3               = 0x61140 >> 2;
	static constexpr uint32_t MM_TV_UNKNOWN4               = 0x61160 >> 2; // Used on the MSNTV2; VTOTAL parsed differently if "& 0x300000 == 0x80100000"
	// Display and Cursor Control Registers (0x70000-0x7FFFF)
	static constexpr uint32_t MM_DISPLAY_DISP_SL           = 0x70000 >> 2;
	static constexpr uint32_t MM_DISPLAY_DISP_SLC          = 0x70004 >> 2;
	static constexpr uint32_t MM_DISPLAY_PIXCONF           = 0x70008 >> 2;
	static constexpr uint32_t MM_DISPLAY_PIXCONF1          = 0x70009 >> 2;
	static constexpr uint32_t MM_DISPLAY_BLTCNTL           = 0x7000c >> 2;
	static constexpr uint32_t MM_DISPLAY_SWF               = 0x70014 >> 2;
	static constexpr uint32_t MM_DISPLAY_DPLYBASE          = 0x70020 >> 2;
	static constexpr uint32_t MM_DISPLAY_DPLYSTAS          = 0x70024 >> 2;
	static constexpr uint32_t MM_DISPLAY_CURCNTR           = 0x70080 >> 2;
	static constexpr uint32_t MM_DISPLAY_CURBASE           = 0x70084 >> 2;
	static constexpr uint32_t MM_DISPLAY_CURPOS            = 0x70088 >> 2;
	static constexpr uint32_t MM_DISPLAY_UNKNOWN1          = 0x70180 >> 2; // Used on the MSNTV2; needs "& 0x300000 == 0x100000" if MM_TV_UNKNOWN1 "& 0x80100000 != 0x80100000"
	static constexpr uint32_t MM_DISPLAY_UNKNOWN3          = 0x70184 >> 2; // Used on the MSNTV2
	static constexpr uint32_t MM_DISPLAY_UNKNOWN4          = 0x70188 >> 2; // Used on the MSNTV2; effected by if MM_TV_UNKNOWN1 "& 0x80100000 == 0x80100000"
	static constexpr uint32_t MM_DISPLAY_UNKNOWN5          = 0x7019c >> 2; // Used on the MSNTV2; needs "!= *MM_DISPLAY_UNKNOWN3"
	static constexpr uint32_t MM_DISPLAY_UNKNOWN6          = 0x71410 >> 2; // Used on the MSNTV2
	static constexpr uint32_t MM_DISPLAY_UNKNOWN7          = 0x71418 >> 2; // Used on the MSNTV2
	static constexpr uint32_t MM_DISPLAY_UNKNOWN8          = 0x71428 >> 2; // Used on the MSNTV2

	static constexpr uint32_t MI_TYPE_SHIFT                            = 29;
	static constexpr uint32_t MI_TYPE_MASK                             = 0x07 << 29;

	static constexpr uint32_t MI_TYPE_PS                               = 0x00 << 29;
	static constexpr uint32_t MI_PS_SUBTYPE_SHIFT                      = 23;
	static constexpr uint32_t MI_PS_SUBTYPE_MASK                       = 0x3f << i82830_graphics_device::MI_PS_SUBTYPE_SHIFT;
	static constexpr uint32_t MI_PS_CMD_NOP_IDENTIFICATION             = 0x00000000;
	static constexpr uint32_t MI_PS_CMD_WAIT_FOR_EVENT                 = 0x01800000;
	static constexpr uint32_t MI_PS_CMD_FLUSH                          = 0x02000000;
	static constexpr uint32_t MI_PS_CMD_LOAD_SCAN_LINES_INCL           = 0x09000000;
	static constexpr uint32_t MI_PS_CMD_SET_CONTEXT                    = 0x0c000000;
	static constexpr uint32_t MI_PS_CMD_STORE_DWORD_IMM                = 0x10800000;
	static constexpr uint32_t MI_PS_CMD_LOAD_REGISTER_IMM              = 0x11000000;

	static constexpr uint32_t MI_TYPE_2D                               = 0x02 << 29;
	static constexpr uint32_t MI_2D_SUBTYPE_SHIFT                      = 22;
	static constexpr uint32_t MI_2D_SUBTYPE_MASK                       = 0x7f << i82830_graphics_device::MI_2D_SUBTYPE_SHIFT;
	static constexpr uint32_t MI_2D_CMD_XY_COLOR_BLT                   = 0x14000000;
	static constexpr uint32_t MI_2D_CMD_XY_SRC_COPY_BLT                = 0x14c00000;

	static constexpr uint32_t MI_TYPE_3D                               = 0x03 << 29;
	static constexpr uint32_t MI_3D_SUBTYPE_SHIFT                      = 24;
	static constexpr uint32_t MI_3D_SUBTYPE_MASK                       = 0x1f << i82830_graphics_device::MI_3D_SUBTYPE_SHIFT;
	static constexpr uint32_t MI_3D_CMD_BOOLEAN_ENA_1                  = 0x03000000;
	static constexpr uint32_t MI_3D_CMD_BOOLEAN_ENA_2                  = 0x04000000;
	static constexpr uint32_t MI_3D_CMD_UNK1                           = 0x07000000; // 0x07
	static constexpr uint32_t MI_3D_CMD_UNK2                           = 0x0b000000; // 0x0b
	static constexpr uint32_t MI_3D_CMD_UNK3                           = 0x0c000000; // 0x0c
	static constexpr uint32_t MI_3D_CMD_FOG_COLOR                      = 0x15000000;
	static constexpr uint32_t MI_3D_CMD_UNK4                           = 0x16000000; // 0x16

	static constexpr uint32_t MI_3D_STATE16                            = 0x1c000000;
	static constexpr uint32_t MI_3D_STATE16_SUBTYPE_SHIFT              = 19;
	static constexpr uint32_t MI_3D_STATE16_SUBTYPE_MASK               = 0x1f << i82830_graphics_device::MI_3D_STATE16_SUBTYPE_SHIFT;
	static constexpr uint32_t MI_3D_STATE16_CMD_SCISSOR_ENABLE         = 0x00800000;

	static constexpr uint32_t MI_3D_STATEMW                            = 0x1d000000;
	static constexpr uint32_t MI_3D_STATEMW_SUBTYPE_SHIFT              = 16;
	static constexpr uint32_t MI_3D_STATEMW_SUBTYPE_MASK               = 0xff << i82830_graphics_device::MI_3D_STATEMW_SUBTYPE_SHIFT;
	static constexpr uint32_t MI_3D_STATEMW_CMD_UNK1                   = 0x00040000; // 0x04
	static constexpr uint32_t MI_3D_STATEMW_CMD_SCISSOR_RECTANGLE_INFO = 0x00810000;
	static constexpr uint32_t MI_3D_STATEMW_CMD_STIPPLE_PATTERN        = 0x00830000;
	static constexpr uint32_t MI_3D_STATEMW_CMD_DEST_BUFFER_VARIABLES  = 0x00850000; // 0x85
	static constexpr uint32_t MI_3D_STATEMW_CMD_UNK2                   = 0x00890000; // 0x89
	static constexpr uint32_t MI_3D_STATEMW_CMD_UNK3                   = 0x00980000; // 0x98
	static constexpr uint32_t MI_3D_STATEMW_CMD_UNK4                   = 0x00990000; // 0x99
	static constexpr uint32_t MI_3D_STATEMW_CMD_UNK5                   = 0x009a0000; // 0x9a

	static constexpr uint32_t MI_3D_BLOCK                              = 0x1e000000;
	static constexpr uint32_t MI_3D_BLOCK_SUBTYPE_SHIFT                = 16;
	static constexpr uint32_t MI_3D_BLOCK_SUBTYPE_MASK                 = 0xff << i82830_graphics_device::MI_3D_BLOCK_SUBTYPE_SHIFT;

	static constexpr uint32_t MI_3D_PRIM                               = 0x1f000000;
	static constexpr uint32_t MI_3D_PRIM_SUBTYPE_SHIFT                 = 16;
	static constexpr uint32_t MI_3D_PRIM_SUBTYPE_MASK                  = 0xff << i82830_graphics_device::MI_3D_PRIM_SUBTYPE_SHIFT;

	template <typename T>
	i82830_graphics_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&mcu_tag, uint32_t main_id = 0x80863577, uint8_t revision = 0x00, uint32_t subdevice_id = 0x00000000)
		: i82830_graphics_device(mconfig, tag, owner, clock)
	{
		// 0x03: Display Controller; 0x0000: VGA Compatible Controller
		uint32_t device_class = 0x030000;

		set_ids(main_id, revision, device_class, subdevice_id);
		set_mcu_tag(std::forward<T>(mcu_tag));
	}

	i82830_graphics_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <typename T> void set_mcu_tag(T &&tag) { m_mcu.set_tag(std::forward<T>(tag)); }

	auto pirq_callback() { return m_pirq_w_cb.bind(); }

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

	auto gpio_read_callback() { return m_gpio_r_cb.bind(); }
	auto gpio_write_callback() { return m_gpio_w_cb.bind(); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

	virtual uint8_t capptr_r() override ATTR_COLD;

private:

	required_device<i82830_host_device> m_mcu;
	required_device<i82830_svga_device> m_svga;

	devcb_write8 m_pirq_w_cb;

	devcb_read32 m_gpio_r_cb;
	devcb_write32 m_gpio_w_cb;

	uint32_t m_max_physical_address;

	uint8_t m_pirq_pin;

	uint32_t m_gmadr;
	uint32_t m_mmadr;
	uint16_t m_miscc;
	uint16_t m_scram;
	uint32_t m_coreclk;

	uint32_t m_mm_block[MM_SIZE / 4];

	void mm_map(address_map &map);
	void vga_io_map(address_map &map);
	void vga_mem_map(address_map &map);
	void gm_map(address_map &map);
	uint32_t gm_r(offs_t offset);
	void gm_w(offs_t offset, uint32_t data);

	uint32_t gtt_lookup(uint32_t graphics_offset);
	uint32_t graphics_memory_read(uint32_t graphics_offset);
	void graphics_memory_write(uint32_t graphics_offset, uint32_t data);

	typedef struct {
		uint32_t start_addr;
		uint32_t cur_addr;
		uint32_t size;
		uint32_t head;
		uint32_t tail;
	} ins_parser_state_t;

	enum ins_execute_state_t : uint8_t
	{
		EXC_IDLE,
		EXC_SUSPENDED,
		EXC_ACTIVE
	};

	ins_execute_state_t m_pins_exc_state;

	const attotime PINS_EXECUTE_RATE = attotime::zero;
	emu_timer* m_pins_execute_timer;
	TIMER_CALLBACK_MEMBER(pins_execute);

	void pins_execute_next();
	void pins_execute_stop();
	bool execute_instruction_buffer(uint32_t mm_ringbuf_index);
	uint32_t instruction_buffer_shift(ins_parser_state_t* parser_state);
	void instruction_buffer_skip(ins_parser_state_t* parser_state, uint32_t skip_amount);
	bool execute_ps_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	bool execute_2d_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	void xy_color_blit(ins_parser_state_t* parser_state);
	void xy_copy_blit(ins_parser_state_t* parser_state);
	bool execute_3d_state16_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	bool execute_3d_statemw_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	bool execute_3d_block_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	bool execute_3d_prim_instruction(uint32_t instruction, ins_parser_state_t* parser_state);
	bool execute_3d_instruction(uint32_t instruction, ins_parser_state_t* parser_state);

	uint32_t mm_block_r(offs_t offset);
	void mm_block_w(offs_t offset, uint32_t data);

	uint32_t gmadr_r();
	void gmadr_w(uint32_t data);
	uint32_t mmadr_r();
	void mmadr_w(uint32_t data);
	void subvendor_w(uint16_t data);
	void subsystem_w(uint16_t data);
	uint16_t coreclk_r();
	uint16_t xxx16_r();
	void xxx16_w(uint16_t data);
	uint32_t xxx32_r();
	void xxx32_w(uint32_t data);

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	void vblank_irq(int state);

};

DECLARE_DEVICE_TYPE(I82830_CGC, i82830_graphics_device)

#endif // MAME_WEBTV_I82830_GFX_H
