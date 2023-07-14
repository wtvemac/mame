/***********************************************************************************************

    solo1_asic_vid.cpp

    WebTV Networks Inc. SOLO1 ASIC (Video)

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This sub-device contains the potUnit and the vidUnit.

    The potUnit (Pixel Output Unit) controls the actual video signal going to the television.
    This implementation currently assumes a NTSC signal, but a PAL signal should be able to be
    emulated as well.

    The vidUnit controls the framebuffer being passed into the potUnit for display output.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The technical specifications that this implementation is based on can be found here:
    http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf

************************************************************************************************/
#include "emu.h"
#include "solo1_asic_vid.h"
#include "screen.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_READS       (1U << 2)
#define LOG_WRITES      (1U << 3)
#define LOG_ERRORS      (1U << 4)
#define LOG_I2C_IGNORES (1U << 5)
#define LOG_DEFAULT     (LOG_READS | LOG_WRITES | LOG_ERRORS | LOG_I2C_IGNORES | LOG_UNKNOWN)

#define VERBOSE         (LOG_DEFAULT)
#include "logmacro.h"

#define SOLO1_NTSC_WIDTH 640
#define SOLO1_NTSC_HEIGHT 480

#define POT_INT_SHIFT     1 << 2
#define POT_INT_VIDHSYNC  1 << 3 // Interrupt fires on hsync
#define POT_INT_VIDVSYNCO 1 << 4 // Interrupt fires on odd line vsync
#define POT_INT_VIDVSYNCE 1 << 5 // Interrupt fires on even line vsync

#define VID_INT_DMA       1 << 2

DEFINE_DEVICE_TYPE(SOLO1_ASIC_VID, solo1_asic_vid_device, "solo1_asic_vid", "WebTV SOLO1 ASIC (Video)")

solo1_asic_vid_device::solo1_asic_vid_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO1_ASIC_VID, tag, owner, clock),
	device_video_interface(mconfig, *this),
    m_hostcpu(*this, finder_base::DUMMY_TAG)
{
}

void solo1_asic_vid_device::fillbitmap_yuy16(bitmap_yuy16 &bitmap, uint8_t yval, uint8_t cr, uint8_t cb)
{
	uint16_t color0 = (yval << 8) | cb;
	uint16_t color1 = (yval << 8) | cr;

	// write 32 bits of color (2 pixels at a time)
	for (int y = 0; y < bitmap.height(); y++)
	{
		uint16_t *dest = &bitmap.pix(y);
		for (int x = 0; x < bitmap.width() / 2; x++)
		{
			*dest++ = color0;
			*dest++ = color1;
		}
	}
}

void solo1_asic_vid_device::device_start()
{
    screen().set_clock(3.579575_MHz_XTAL);
	screen().set_size(SOLO1_NTSC_WIDTH, SOLO1_NTSC_HEIGHT);
    screen().set_refresh_hz(59.94);
}

void solo1_asic_vid_device::device_reset()
{
    m_vid_dmacntl = 0;
}

uint32_t solo1_asic_vid_device::reg_pot_r(offs_t offset)
{
    //printf("SOLO read: potUnit %04x\n", offset*4);
    switch(offset*4)
    {
    case 0x080: // POT_VSTART (R/W)
        return m_pot_vstart;
    case 0x084: // POT_VSIZE (R/W)
        return m_pot_vsize;
    case 0x088: // POT_BLNKCOL (R/W)
        return m_pot_blnkcol;
    case 0x08c: // POT_HSTART (R/W)
        return m_pot_hstart;
    case 0x090: // POT_HSIZE (R/W)
        return m_pot_hsize;
    case 0x094: // POT_CNTL (R/W)
        return m_pot_cntl;
    case 0x098: // POT_HINTLINE (R/W)
        return m_pot_hintline;
    case 0x09c: // POT_INTEN (R/Set)
        return m_pot_int_enable;
    case 0x0a0: // POT_INTSTAT (R/Set)
        return m_pot_int_status & m_pot_int_enable;
    case 0x0a4: // POT_INTEN (Clear)
        break;
    case 0x0a8: // POT_INTSTAT (Clear)
        break;
    case 0x0ac: // POT_CLINE (R/W)
        return m_pot_cline;
    default:
        logerror("Attempted read from reserved register 9%03x!\n", offset*4);
        break;
    }
    return 0;
}

void solo1_asic_vid_device::reg_pot_w(offs_t offset, uint32_t data)
{
    //printf("SOLO write: potUnit %08x to %04x\n", data, offset*4);
    switch(offset*4)
    {
    case 0x080: // POT_VSTART (R/W)
        m_pot_vstart = data;
        break;
    case 0x084: // POT_VSIZE (R/W)
        m_pot_vsize = data;
        break;
    case 0x088: // POT_BLNKCOL (R/W)
        m_pot_blnkcol = data;
        break;
    case 0x08c: // POT_HSTART (R/W)
        m_pot_hstart = data;
        break;
    case 0x090: // POT_HSIZE (R/W)
        m_pot_hsize = data;
        break;
    case 0x094: // POT_CNTL (R/W)
        m_pot_cntl = data;
        break;
    case 0x098: // POT_HINTLINE (R/W)
        m_pot_hintline = data;
        break;
    case 0x09c: // POT_INTEN (R/Set)
        m_pot_int_enable |= data; // TODO: is this correct behavior?
        break;
    case 0x0a0: // POT_INTSTAT (R/Set)
        m_pot_int_status |= data; // TODO: is this correct behavior?
        break;
    case 0x0a4: // POT_INTEN (Clear)
        m_pot_int_enable &= ~data; // TODO: is this correct behavior?
        break;
    case 0x0a8: // POT_INTSTAT (Clear)
        m_pot_int_status &= ~data; // TODO: is this correct behavior?
        break;
    case 0x0ac: // POT_CLINE (R/W)
        m_pot_cline = data;
        break;
    default:
        logerror("Attempted write (%08x) to reserved register 9%03x!\n", data, offset*4);
        break;
    }
}

uint32_t solo1_asic_vid_device::reg_vid_r(offs_t offset)
{
    //printf("SOLO read: vidUnit %04x\n", offset*4);
    switch(offset*4)
    {
    case 0x000: // VID_CSTART (R/W)
        return m_vid_cstart;
    case 0x004: // VID_CSIZE (R/W)
        return m_vid_csize;
    case 0x008: // VID_CCNT (R/W)
        return m_vid_ccount;
    case 0x00c: // VID_NSTART (R/W)
        return m_vid_nstart;
    case 0x010: // VID_NSIZE (R/W)
        return m_vid_nsize;
    case 0x014: // VID_DMACNTL (R/W)
        return m_vid_dmacntl;
    case 0x038: // VID_INTSTAT (R/Set)
        return m_vid_int_status & m_vid_int_enable;
    case 0x03c: // VID_INTEN (R/Set)
        return m_vid_int_enable;
    case 0x040: // VID_VDATA (R/W)
        return m_vid_vdata;
    case 0x138: // VID_INTSTAT (Clear)
        break;
    case 0x13c: // VID_INTEN (Clear)
        break;
    default:
        logerror("Attempted read from reserved register 3%03x!\n", offset*4);
        break;
    }
    return 0;
}

void solo1_asic_vid_device::reg_vid_w(offs_t offset, uint32_t data)
{
    //printf("SOLO write: vidUnit %08x to %04x\n", data, offset*4);
    switch(offset*4)
    {
    case 0x000: // VID_CSTART (R/W)
        logerror("Attempted write to read-only register 3000 (VID_CSTART)\n");
        break;
    case 0x004: // VID_CSIZE (R/W)
        logerror("Attempted write to read-only register 3004 (VID_CSIZE)\n");
        break;
    case 0x008: // VID_CCNT (R/W)
        logerror("Attempted write to read-only register 3008 (VID_CCNT)\n");
        break;
    case 0x00c: // VID_NSTART (R/W)
        m_vid_nstart = data;
        break;
    case 0x010: // VID_NSIZE (R/W)
        m_vid_nsize = data;
        break;
    case 0x014: // VID_DMACNTL (R/W)
        m_vid_dmacntl = data;
        break;
    case 0x038: // VID_INTSTAT (R/Set)
        m_vid_int_status |= data; // TODO: is this correct behavior?
        break;
    case 0x03c: // VID_INTEN (R/Set)
        m_vid_int_enable |= data; // TODO: is this correct behavior?
        break;
    case 0x040: // VID_VDATA (R/W)
        m_vid_vdata = data;
        break;
    case 0x138: // VID_INTSTAT (Clear)
        m_vid_int_status &= ~data; // TODO: is this correct behavior?
        break;
    case 0x13c: // VID_INTEN (Clear)
        m_vid_int_enable &= ~data; // TODO: is this correct behavior?
        break;
    default:
        logerror("Attempted write (%08x) to reserved register 3%03x!\n", data, offset*4);
        break;
    }
}

uint32_t solo1_asic_vid_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    // TODO: everything
    return 0;
}
