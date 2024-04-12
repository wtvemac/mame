// license:BSD-3-Clause
// copyright-holders:FairPlay137

/***********************************************************************************************

    spot_asic.cpp

    WebTV Networks Inc. SPOT ASIC

    This ASIC controls most of the I/O on the 1st generation WebTV hardware. It is also referred
    to as FIDO on the actual chip that implements the SPOT logic.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The SPOT ASIC is split into multiple "units", just like its successor.

    The romUnit (0xA4001xxx) provides a shared interface to the ROM.

    The audUnit (0xA4002xxx) handles audio DMA.

    The vidUnit (0xA4003xxx) handles video DMA.

    The devUnit (0xA4004xxx) handles GPIO, IR input, front panel LEDs, modem interfacing, and the
    PS/2 keyboard port.

    The memUnit (0xA4005xxx) handles memory timing and other memory-related operations. These
    registers are only emulated for completeness; they do not have an effect on the emulation.

****************************************************************************************************/
#include "emu.h"

#include "render.h"
#include "spot_asic.h"
#include "screen.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_READS       (1U << 2)
#define LOG_WRITES      (1U << 3)
#define LOG_ERRORS      (1U << 4)
#define LOG_I2C_IGNORES (1U << 5)
#define LOG_DEFAULT     (LOG_READS | LOG_WRITES | LOG_ERRORS | LOG_I2C_IGNORES | LOG_UNKNOWN)

#define VERBOSE         (LOG_DEFAULT)
#include "logmacro.h"

#define SPOT_NTSC_WIDTH 640
#define SPOT_NTSC_HEIGHT 480

DEFINE_DEVICE_TYPE(SPOT_ASIC, spot_asic_device, "spot_asic", "WebTV SPOT ASIC")

spot_asic_device::spot_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SPOT_ASIC, tag, owner, clock),
	device_serial_interface(mconfig, *this),
	device_video_interface(mconfig, *this),
    m_hostcpu(*this, finder_base::DUMMY_TAG),
    m_serial_id(*this, finder_base::DUMMY_TAG),
    m_nvram(*this, finder_base::DUMMY_TAG),
    m_kbdc(*this, "kbdc"),
	m_screen(*this, "screen"),
    m_power_led(*this, "power_led"),
    m_connect_led(*this, "connect_led"),
    m_message_led(*this, "message_led"),
    m_videobitmap(SPOT_NTSC_WIDTH, SPOT_NTSC_HEIGHT),
    m_hsync_cb(*this),
	m_vsync_cb(*this),
    m_sys_timer(nullptr) // when it goes off, timer interrupt fires
{
}

void spot_asic_device::fillbitmap_yuy16(bitmap_yuy16 &bitmap, uint8_t yval, uint8_t cr, uint8_t cb)
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

void spot_asic_device::bus_unit_map(address_map &map)
{
    map(0x000, 0x003).r(FUNC(spot_asic_device::reg_0000_r));
    map(0x004, 0x007).rw(FUNC(spot_asic_device::reg_0004_r), FUNC(spot_asic_device::reg_0004_w));
    map(0x008, 0x00b).r(FUNC(spot_asic_device::reg_0008_r));
    map(0x108, 0x10b).w(FUNC(spot_asic_device::reg_0108_w));
    map(0x00c, 0x00f).rw(FUNC(spot_asic_device::reg_000c_r), FUNC(spot_asic_device::reg_000c_w));
    map(0x10c, 0x10f).w(FUNC(spot_asic_device::reg_010c_w));
    //map(0x010, 0x013).r(FUNC(spot_asic_device::reg_0010_r));
    //map(0x110, 0x113).w(FUNC(spot_asic_device::reg_0110_w));
    //map(0x014, 0x017).rw(FUNC(spot_asic_device::reg_0014_r), FUNC(spot_asic_device::reg_0014_w));
    //map(0x114, 0x117).w(FUNC(spot_asic_device::reg_0114_w));
    //map(0x018, 0x01b).r(FUNC(spot_asic_device::reg_0018_r));
    //map(0x118, 0x11b).w(FUNC(spot_asic_device::reg_0118_w));
    //map(0x01c, 0x01f).rw(FUNC(spot_asic_device::reg_001c_r), FUNC(spot_asic_device::reg_001c_w));
    //map(0x020, 0x023).rw(FUNC(spot_asic_device::reg_0020_r), FUNC(spot_asic_device::reg_0020_w));
    //map(0x024, 0x027).rw(FUNC(spot_asic_device::reg_0024_r), FUNC(spot_asic_device::reg_0024_w));
    //map(0x028, 0x02b).rw(FUNC(spot_asic_device::reg_0028_r), FUNC(spot_asic_device::reg_0028_w));
}

void spot_asic_device::rom_unit_map(address_map &map)
{
    map(0x000, 0x003).r(FUNC(spot_asic_device::reg_1000_r));
    map(0x004, 0x007).rw(FUNC(spot_asic_device::reg_1004_r), FUNC(spot_asic_device::reg_1004_w));
    map(0x008, 0x00b).rw(FUNC(spot_asic_device::reg_1008_r), FUNC(spot_asic_device::reg_1008_w));
}

void spot_asic_device::aud_unit_map(address_map &map)
{
    //map(0x000, 0x003).r(FUNC(spot_asic_device::reg_2000_r));
    //map(0x004, 0x007).r(FUNC(spot_asic_device::reg_2004_r));
    //map(0x008, 0x00b).rw(FUNC(spot_asic_device::reg_2008_r), FUNC(spot_asic_device::reg_2008_w));
    //map(0x00c, 0x00f).r(FUNC(spot_asic_device::reg_200c_r));
    //map(0x010, 0x013).rw(FUNC(spot_asic_device::reg_2010_r), FUNC(spot_asic_device::reg_2010_w));
    //map(0x014, 0x017).rw(FUNC(spot_asic_device::reg_2014_r), FUNC(spot_asic_device::reg_2014_w));
    //map(0x018, 0x01b).rw(FUNC(spot_asic_device::reg_2018_r), FUNC(spot_asic_device::reg_2018_w));
    //map(0x01c, 0x01f).rw(FUNC(spot_asic_device::reg_201c_r), FUNC(spot_asic_device::reg_201c_w));
}

void spot_asic_device::vid_unit_map(address_map &map)
{
    map(0x000, 0x003).r(FUNC(spot_asic_device::reg_3000_r));
    map(0x004, 0x007).r(FUNC(spot_asic_device::reg_3004_r));
    map(0x008, 0x00b).r(FUNC(spot_asic_device::reg_3008_r));
    map(0x00c, 0x00f).rw(FUNC(spot_asic_device::reg_300c_r), FUNC(spot_asic_device::reg_300c_w));
    map(0x010, 0x013).rw(FUNC(spot_asic_device::reg_3010_r), FUNC(spot_asic_device::reg_3010_w));
    map(0x014, 0x017).rw(FUNC(spot_asic_device::reg_3014_r), FUNC(spot_asic_device::reg_3014_w));
    //map(0x018, 0x01b).rw(FUNC(spot_asic_device::reg_3018_r), FUNC(spot_asic_device::reg_3018_w));
    //map(0x01c, 0x01f).rw(FUNC(spot_asic_device::reg_301c_r), FUNC(spot_asic_device::reg_301c_w));
    map(0x020, 0x023).rw(FUNC(spot_asic_device::reg_3020_r), FUNC(spot_asic_device::reg_3020_w));
    map(0x024, 0x027).rw(FUNC(spot_asic_device::reg_3024_r), FUNC(spot_asic_device::reg_3024_w));
    map(0x028, 0x02b).rw(FUNC(spot_asic_device::reg_3028_r), FUNC(spot_asic_device::reg_3028_w));
    map(0x02c, 0x02f).rw(FUNC(spot_asic_device::reg_302c_r), FUNC(spot_asic_device::reg_302c_w));
    map(0x030, 0x033).rw(FUNC(spot_asic_device::reg_3030_r), FUNC(spot_asic_device::reg_3030_w));
    map(0x034, 0x037).r(FUNC(spot_asic_device::reg_3034_r));
    //map(0x038, 0x03b).r(FUNC(spot_asic_device::reg_3038_r));
    //map(0x138, 0x13b).w(FUNC(spot_asic_device::reg_3138_w));
    //map(0x03c, 0x03f).rw(FUNC(spot_asic_device::reg_303c_r), FUNC(spot_asic_device::reg_303c_w));
    //map(0x13c, 0x13f).w(FUNC(spot_asic_device::reg_313c_w));
}

void spot_asic_device::dev_unit_map(address_map &map)
{
    map(0x000, 0x003).r(FUNC(spot_asic_device::reg_4000_r));
    map(0x004, 0x007).rw(FUNC(spot_asic_device::reg_4004_r), FUNC(spot_asic_device::reg_4004_w));
    map(0x008, 0x00b).rw(FUNC(spot_asic_device::reg_4008_r), FUNC(spot_asic_device::reg_4008_w));
    map(0x00c, 0x00f).rw(FUNC(spot_asic_device::reg_400c_r), FUNC(spot_asic_device::reg_400c_w));
    map(0x010, 0x013).rw(FUNC(spot_asic_device::reg_4010_r), FUNC(spot_asic_device::reg_4010_w));
    map(0x014, 0x017).rw(FUNC(spot_asic_device::reg_4014_r), FUNC(spot_asic_device::reg_4014_w));
    map(0x020, 0x023).rw(FUNC(spot_asic_device::reg_4020_r), FUNC(spot_asic_device::reg_4020_w));
    map(0x024, 0x027).rw(FUNC(spot_asic_device::reg_4024_r), FUNC(spot_asic_device::reg_4024_w));
    map(0x028, 0x02b).rw(FUNC(spot_asic_device::reg_4028_r), FUNC(spot_asic_device::reg_4028_w));
    map(0x02c, 0x02f).rw(FUNC(spot_asic_device::reg_402c_r), FUNC(spot_asic_device::reg_402c_w));
    map(0x030, 0x033).rw(FUNC(spot_asic_device::reg_4030_r), FUNC(spot_asic_device::reg_4030_w));
    map(0x034, 0x037).rw(FUNC(spot_asic_device::reg_4034_r), FUNC(spot_asic_device::reg_4034_w));
    map(0x038, 0x03b).rw(FUNC(spot_asic_device::reg_4038_r), FUNC(spot_asic_device::reg_4038_w));
    map(0x03c, 0x03f).rw(FUNC(spot_asic_device::reg_403c_r), FUNC(spot_asic_device::reg_403c_w));
    //map(0x040, 0x043).rw(FUNC(spot_asic_device::reg_4040_r), FUNC(spot_asic_device::reg_4040_w));
    //map(0x044, 0x047).rw(FUNC(spot_asic_device::reg_4044_r), FUNC(spot_asic_device::reg_4044_w));
    //map(0x048, 0x04b).rw(FUNC(spot_asic_device::reg_4048_r), FUNC(spot_asic_device::reg_4048_w));
    //map(0x04c, 0x04f).rw(FUNC(spot_asic_device::reg_404c_r), FUNC(spot_asic_device::reg_404c_w));
    //map(0x050, 0x053).rw(FUNC(spot_asic_device::reg_4050_r), FUNC(spot_asic_device::reg_4050_w));
    //map(0x054, 0x057).rw(FUNC(spot_asic_device::reg_4054_r), FUNC(spot_asic_device::reg_4054_w));
    //map(0x058, 0x05b).rw(FUNC(spot_asic_device::reg_4058_r), FUNC(spot_asic_device::reg_4058_w));
    //map(0x05c, 0x05f).rw(FUNC(spot_asic_device::reg_405c_r), FUNC(spot_asic_device::reg_405c_w));
}

void spot_asic_device::mem_unit_map(address_map &map)
{
    map(0x000, 0x003).rw(FUNC(spot_asic_device::reg_5000_r), FUNC(spot_asic_device::reg_5000_w));
    map(0x004, 0x007).rw(FUNC(spot_asic_device::reg_5004_r), FUNC(spot_asic_device::reg_5004_w));
    map(0x008, 0x00b).rw(FUNC(spot_asic_device::reg_5008_r), FUNC(spot_asic_device::reg_5008_w));
    map(0x00c, 0x00f).rw(FUNC(spot_asic_device::reg_500c_r), FUNC(spot_asic_device::reg_500c_w)); // write only register, but read handler is hooked up to debug behavior
    map(0x010, 0x013).rw(FUNC(spot_asic_device::reg_5010_r), FUNC(spot_asic_device::reg_5010_w));
}

void spot_asic_device::device_add_mconfig(machine_config &config)
{
    SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
    m_screen->set_size(SPOT_NTSC_WIDTH, SPOT_NTSC_HEIGHT);
	m_screen->set_visarea(0, SPOT_NTSC_WIDTH - 1, 0, SPOT_NTSC_HEIGHT - 1);
	m_screen->set_refresh_hz(59.94);
    
    m_screen->set_screen_update(FUNC(spot_asic_device::screen_update));
    set_clock(m_screen->clock() * 2); // internal clock is always set to double the pixel clock

	KBDC8042(config, m_kbdc);
	m_kbdc->set_keyboard_type(kbdc8042_device::KBDC8042_PS2);
	m_kbdc->input_buffer_full_callback().set(FUNC(spot_asic_device::irq_keyboard_w));
	m_kbdc->system_reset_callback().set_inputline(":maincpu", INPUT_LINE_RESET);
	m_kbdc->set_keyboard_tag("at_keyboard");

	at_keyboard_device &at_keyb(AT_KEYB(config, "at_keyboard", pc_keyboard_device::KEYBOARD_TYPE::AT, 1));
	at_keyb.keypress().set(m_kbdc, FUNC(kbdc8042_device::keyboard_w));
}

void spot_asic_device::device_start()
{
    m_memcntl = 0b11;
    m_memrefcnt = 0x0400;
    m_memdata = 0x0;
    m_memtiming = 0xadbadffa;
    m_intenable = 0x0;
    m_intstat = 0x0;
    m_errenable = 0x0;
    m_errstat = 0x0;
    m_timeout_compare = 0xffff;
    m_nvcntl = 0x0;
    m_vid_nstart = 0x0;
    m_vid_nsize = 0x0;
    m_vid_dmacntl = 0x0;
    m_vid_hstart = 0x0;
    m_vid_hsize = 0x0;
    m_vid_vstart = 0x0;
    m_vid_vsize = 0x0;
    m_vid_hintline = 0x0;
    m_vid_cline = 0x0;
    m_ledstate = 0xFFFFFFFF;

    emc_bitcount = 0x0;
    emc_byte = 0x0;
    emc_vbltimer = 0x0;
}

void spot_asic_device::device_reset()
{
    m_memcntl = 0b11;
    m_memrefcnt = 0x0400;
    m_memdata = 0x0;
    m_memtiming = 0xadbadffa;
    m_intenable = 0x0;
    m_intstat = 0x0;
    m_errenable = 0x0;
    m_errstat = 0x0;
    m_timeout_compare = 0xffff;
    m_nvcntl = 0x0;
    m_vid_nstart = 0x0;
    m_vid_nsize = 0x0;
    m_vid_dmacntl = 0x0;
    m_vid_hstart = 0x0;
    m_vid_hsize = 0x0;
    m_vid_vstart = 0x0;
    m_vid_vsize = 0x0;
    m_vid_hintline = 0x0;
    m_vid_cline = 0x0;

    emc_bitcount = 0x0;
    emc_byte = 0x0;
    emc_vbltimer = 0x0;
}

uint32_t spot_asic_device::reg_0000_r()
{
	//logerror("%s: reg_0000_r (BUS_CHIPID)\n", machine().describe_context());
    return 0x01010000;
}

uint32_t spot_asic_device::reg_0004_r()
{
	logerror("%s: reg_0004_r (BUS_CHPCNTL)\n", machine().describe_context());
    return 0x00000000;
}

void spot_asic_device::reg_0004_w(uint32_t data)
{
	logerror("%s: reg_0004_w %08x (BUS_CHPCNTL)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_0008_r()
{
	logerror("%s: reg_0008_r (BUS_INTSTAT)\n", machine().describe_context());
    return m_intstat;
}

void spot_asic_device::reg_0108_w(uint32_t data)
{
	logerror("%s: reg_0108_w %08x (BUS_INTSTAT clear)\n", machine().describe_context(), data);
    m_intstat &= (~data) & 0xFF;
}

uint32_t spot_asic_device::reg_000c_r()
{
	logerror("%s: reg_000c_r (BUS_INTEN)\n", machine().describe_context());
    return m_intenable;
}

void spot_asic_device::reg_000c_w(uint32_t data)
{
	logerror("%s: reg_000c_w %08x (BUS_INTEN)\n", machine().describe_context(), data);
    m_intenable |= data & 0xFF;
}

void spot_asic_device::reg_010c_w(uint32_t data)
{
	logerror("%s: reg_010c_w %08x (BUS_INTEN clear)\n", machine().describe_context(), data);
    m_intenable &= (~data) & 0xFF;
}

uint32_t spot_asic_device::reg_0010_r()
{
	logerror("%s: reg_0010_r (BUS_ERRSTAT)\n", machine().describe_context());
    return m_errstat;
}

uint32_t spot_asic_device::reg_1000_r()
{
	logerror("%s: reg_1000_r (ROM_SYSCONF)\n", machine().describe_context());
    // The values here correspond to a retail FCS board, with flash ROM in bank 0 and mask ROM in bank 1
    return 0x2becbf8f;
}

uint32_t spot_asic_device::reg_1004_r()
{
    logerror("%s: reg_1004_r (ROM_CNTL0)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_1004_w(uint32_t data)
{
    logerror("%s: reg_1004_w %08x (ROM_CNTL0)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_1008_r()
{
    logerror("%s: reg_1008_r (ROM_CNTL1)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_1008_w(uint32_t data)
{
    logerror("%s: reg_1008_w %08x (ROM_CNTL1)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3000_r()
{
    logerror("%s: reg_3000_r (VID_CSTART)\n", machine().describe_context());
    return 0;
}

uint32_t spot_asic_device::reg_3004_r()
{
    logerror("%s: reg_3004_r (VID_CSIZE)\n", machine().describe_context());
    return 0;
}

uint32_t spot_asic_device::reg_3008_r()
{
    logerror("%s: reg_3008_r (VID_CCNT)\n", machine().describe_context());
    return 0;
}

uint32_t spot_asic_device::reg_300c_r()
{
    logerror("%s: reg_300c_r (VID_NSTART)\n", machine().describe_context());
    return m_vid_nstart;
}

void spot_asic_device::reg_300c_w(uint32_t data)
{
    m_vid_nstart = data;
    printf("m_vid_nstart=%08x\n", m_vid_nstart);
    logerror("%s: reg_300c_w %08x (VID_NSTART)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3010_r()
{
    logerror("%s: reg_3010_r (VID_NSIZE)\n", machine().describe_context());
    return m_vid_nsize;
}

void spot_asic_device::reg_3010_w(uint32_t data)
{
    m_vid_nsize = data;
    printf("m_vid_nsize=%08x\n", m_vid_nsize);
    logerror("%s: reg_3010_w %08x (VID_NSIZE)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3014_r()
{
    logerror("%s: reg_3014_r (VID_DMACNTL)\n", machine().describe_context());
    return m_vid_dmacntl;
}

void spot_asic_device::reg_3014_w(uint32_t data)
{
    m_vid_dmacntl = data;
    printf("m_vid_dmacntl=%08x\n", m_vid_dmacntl);
    logerror("%s: reg_3014_w %08x (VID_DMACNTL)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3020_r()
{
    logerror("%s: reg_3020_r (VID_HSTART)\n", machine().describe_context());
    return m_vid_hstart;
}

void spot_asic_device::reg_3020_w(uint32_t data)
{
    m_vid_hstart = data;
    printf("m_vid_hstart=%08x\n", m_vid_hstart);
    logerror("%s: reg_3020_w %08x (VID_HSTART)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3024_r()
{
    logerror("%s: reg_3024_r (VID_HSIZE)\n", machine().describe_context());
    return m_vid_hsize;
}

void spot_asic_device::reg_3024_w(uint32_t data)
{
    m_vid_hsize = data;
    printf("m_vid_hsize=%08x\n", m_vid_hsize);
    logerror("%s: reg_3024_w %08x (VID_HSIZE)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3028_r()
{
    logerror("%s: reg_3028_r (VID_VSTART)\n", machine().describe_context());
    return m_vid_vstart;
}

void spot_asic_device::reg_3028_w(uint32_t data)
{
    m_vid_vstart = data;
    printf("m_vid_vstart=%08x\n", m_vid_vstart);
    logerror("%s: reg_3028_w %08x (VID_VSTART)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_302c_r()
{
    logerror("%s: reg_302c_r (VID_VSIZE)\n", machine().describe_context());
    return m_vid_vsize;
}

void spot_asic_device::reg_302c_w(uint32_t data)
{
    m_vid_vsize = data;
    printf("m_vid_vsize=%08x\n", m_vid_vsize);
    logerror("%s: reg_302c_w %08x (VID_VSIZE)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3030_r()
{
    logerror("%s: reg_3030_r (VID_HINTLINE)\n", machine().describe_context());
    return m_vid_hintline;
}

void spot_asic_device::reg_3030_w(uint32_t data)
{
    m_vid_hintline = data;
    logerror("%s: reg_3030_w %08x (VID_HINTLINE)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3034_r()
{
    logerror("%s: reg_3034_r (VID_CLINE)\n", machine().describe_context());

    // Older spot builds uses the VBL to calculate the CPU speed.
    // The calculated CPU speed gets used everywhere in the build so this is a hack to temp fix bootup issues.
    return (m_vid_cline++) & 0x1ffff; // 0x1ffff is arbitrary. I just set it to this since it gets a MHz I'm happy with
}


// Read IR receiver chip
uint32_t spot_asic_device::reg_4000_r()
{
    logerror("%s: reg_4000_r (DEV_IRDATA)\n", machine().describe_context());
    // TODO: This seems to have been handled by a PIC16CR54AT. We do not have the ROM for this chip, so its behavior will need to be emulated at a high level.
    return 0;
}

// Read LED states
uint32_t spot_asic_device::reg_4004_r()
{
    logerror("%s: reg_4004_r (DEV_LED)\n", machine().describe_context());
    m_power_led = !BIT(m_ledstate, 2);
    m_connect_led = !BIT(m_ledstate, 1);
    m_message_led = !BIT(m_ledstate, 0);
    return m_ledstate;
}

// Update LED states
void spot_asic_device::reg_4004_w(uint32_t data)
{
    logerror("%s: reg_4004_w %08x (DEV_LED)\n", machine().describe_context(), data);
    m_ledstate = data;
    m_power_led = !BIT(m_ledstate, 2);
    m_connect_led = !BIT(m_ledstate, 1);
    m_message_led = !BIT(m_ledstate, 0);
}

// Read from DS2401
uint32_t spot_asic_device::reg_4008_r()
{
    logerror("%s: reg_4008_r (DEV_IDCNTL)\n", machine().describe_context());
    return (m_serial_id->read() + (m_serial_id_tx << 1));
}

// Write to DS2401
void spot_asic_device::reg_4008_w(uint32_t data)
{
    m_serial_id_tx = BIT(data, 1);
    logerror("%s: reg_4008_w %08x - write %d (DEV_IDCNTL)\n", machine().describe_context(), data, m_serial_id_tx);
    m_serial_id->write(m_serial_id_tx ? ASSERT_LINE : CLEAR_LINE);
}

// Read from I2C EEPROM device (24C01A?)
uint32_t spot_asic_device::reg_400c_r()
{
    logerror("%s: reg_400c_r (DEV_NVCNTL)\n", machine().describe_context());
    return (m_nvcntl & ((NVCNTL_SCL) + (NVCNTL_WRITE_EN))) + (m_nvram->read_sda() * NVCNTL_SDA_W) + (m_nvram->read_sda() * NVCNTL_SDA_R);
}

// Write to I2C EEPROM device
void spot_asic_device::reg_400c_w(uint32_t data)
{
    logerror("%s: reg_400c_w %08x (DEV_NVCNTL)\n", machine().describe_context(), data);
    m_nvram->write_scl((data & NVCNTL_SCL) ? ASSERT_LINE : CLEAR_LINE);
    if (data & NVCNTL_WRITE_EN) {
        logerror("%s: Writing %01x to NVRAM...\n", machine().describe_context(), (data & NVCNTL_SDA_W) ? ASSERT_LINE : CLEAR_LINE);
        m_nvram->write_sda((data & NVCNTL_SDA_W) ? ASSERT_LINE : CLEAR_LINE);
    }
    m_nvcntl = data & ((NVCNTL_SCL) + (NVCNTL_WRITE_EN) + (NVCNTL_SDA_W) + (NVCNTL_SDA_R));
    
}

uint32_t spot_asic_device::reg_4010_r()
{
    logerror("%s: reg_4010_r (DEV_SCCNTL)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_4010_w(uint32_t data)
{
    //m_smartcard->ins8250_w(1, (data >> 0x05) & 0x1); // SCCLK or UART_TXD
    //m_smartcard->ins8250_w(1, (data >> 0x02) & 0x1); // SCDATA0EN or UART_DTR
    //m_smartcard->ins8250_w(1, (data >> 0x01) & 0x1); // SCRESET or UART_RTS

    if(emc_bitcount == 0) {
        if(data != 0) {
            emc_bitcount -= 1;
        }
    } else if(emc_bitcount == 1) {
        if(data == 0) {
            emc_bitcount -= 2;
        }
    } else if(emc_bitcount <= 9) {
        if(data == 0) {
            emc_byte |= 1 << (emc_bitcount - 2);
        } // 0x20
    } else {
        if(data == 0) {
            printf("%c", emc_byte);
        }

        emc_byte = 0x00;
        emc_bitcount = -1;
    }
    
    emc_bitcount++;

    logerror("%s: reg_4010_w %08x (DEV_SCCNTL)\n", machine().describe_context(), data);
}    

uint32_t spot_asic_device::reg_4014_r()
{
    logerror("%s: reg_4014_r (DEV_EXTTIME)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_4014_w(uint32_t data)
{
    logerror("%s: reg_4014_w %08x (DEV_EXTTIME)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_4020_r()
{
    logerror("%s: reg_4020_r (DEV_KBD0)\n", machine().describe_context());
    return m_kbdc->data_r(0x0);
}

void spot_asic_device::reg_4020_w(uint32_t data)
{
    logerror("%s: reg_4020_w %08x (DEV_KBD0)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x0, data & 0xFF);
}

uint32_t spot_asic_device::reg_4024_r()
{
    logerror("%s: reg_4024_r (DEV_KBD1)\n", machine().describe_context());
    return m_kbdc->data_r(0x1);
}

void spot_asic_device::reg_4024_w(uint32_t data)
{
    logerror("%s: reg_4024_w %08x (DEV_KBD1)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x1, data & 0xFF);
}

uint32_t spot_asic_device::reg_4028_r()
{
    logerror("%s: reg_4028_r (DEV_KBD2)\n", machine().describe_context());
    return m_kbdc->data_r(0x2);
}

void spot_asic_device::reg_4028_w(uint32_t data)
{
    logerror("%s: reg_4028_w %08x (DEV_KBD2)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x2, data & 0xFF);
}

uint32_t spot_asic_device::reg_402c_r()
{
    logerror("%s: reg_402c_r (DEV_KBD3)\n", machine().describe_context());
    return m_kbdc->data_r(0x3);
}

void spot_asic_device::reg_402c_w(uint32_t data)
{
    logerror("%s: reg_402c_w %08x (DEV_KBD3)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x3, data & 0xFF);
}

uint32_t spot_asic_device::reg_4030_r()
{
    logerror("%s: reg_4030_r (DEV_KBD4)\n", machine().describe_context());
    return m_kbdc->data_r(0x4);
}

void spot_asic_device::reg_4030_w(uint32_t data)
{
    logerror("%s: reg_4030_w %08x (DEV_KBD4)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x4, data & 0xFF);
}

uint32_t spot_asic_device::reg_4034_r()
{
    logerror("%s: reg_4034_r (DEV_KBD5)\n", machine().describe_context());
    return m_kbdc->data_r(0x5);
}

void spot_asic_device::reg_4034_w(uint32_t data)
{
    logerror("%s: reg_4034_w %08x (DEV_KBD5)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x5, data & 0xFF);
}

uint32_t spot_asic_device::reg_4038_r()
{
    logerror("%s: reg_4038_r (DEV_KBD6)\n", machine().describe_context());
    return m_kbdc->data_r(0x6);
}

void spot_asic_device::reg_4038_w(uint32_t data)
{
    logerror("%s: reg_4038_w %08x (DEV_KBD6)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x6, data & 0xFF);
}

uint32_t spot_asic_device::reg_403c_r()
{
    logerror("%s: reg_403c_r (DEV_KBD7)\n", machine().describe_context());
    return m_kbdc->data_r(0x7);
}

void spot_asic_device::reg_403c_w(uint32_t data)
{
    logerror("%s: reg_403c_w %08x (DEV_KBD7)\n", machine().describe_context(), data);
    m_kbdc->data_w(0x7, data & 0xFF);
}

uint32_t spot_asic_device::reg_5000_r()
{
    logerror("%s: reg_5000_r (MEM_CNTL)\n", machine().describe_context());
    return m_memcntl;
}

void spot_asic_device::reg_5000_w(uint32_t data)
{
    logerror("%s: reg_5000_w %08x (MEM_CNTL)\n", machine().describe_context(), data);
    m_memcntl = data;
}

uint32_t spot_asic_device::reg_5004_r()
{
    logerror("%s: reg_5004_r (MEM_REFCNT)\n", machine().describe_context());
    return m_memrefcnt;
}

void spot_asic_device::reg_5004_w(uint32_t data)
{
    logerror("%s: reg_5004_w %08x (MEM_REFCNT)\n", machine().describe_context(), data);
    m_memrefcnt = data;
}

uint32_t spot_asic_device::reg_5008_r()
{
    logerror("%s: reg_5008_r (MEM_DATA)\n", machine().describe_context());
    return m_memdata;
}

void spot_asic_device::reg_5008_w(uint32_t data)
{
    logerror("%s: reg_5008_w %08x (MEM_DATA)\n", machine().describe_context(), data);
    m_memdata = data;
}

uint32_t spot_asic_device::reg_500c_r()
{
    logerror("%s: reg_500c_r (MEM_CMD - not a readable register!)\n", machine().describe_context());
    // FIXME: This is defined as a write-only register, yet the WebTV software reads from it? Still need to see what the software expects from this.
    return 0;
}

void spot_asic_device::reg_500c_w(uint32_t data)
{
    logerror("%s: reg_500c_w %08x (MEM_CMD)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_5010_r()
{
    logerror("%s: reg_5010_r (MEM_TIMING)\n", machine().describe_context());
    return m_memtiming;
}

void spot_asic_device::reg_5010_w(uint32_t data)
{
    logerror("%s: reg_500c_w %08x (MEM_TIMING)\n", machine().describe_context(), data);
    m_memtiming = data;
}

void spot_asic_device::irq_keyboard_w(int state)
{
    set_bus_irq(BUS_INT_DEVKBD, state);
}

void spot_asic_device::set_bus_irq(uint8_t mask, int state)
{
    if (m_intenable & mask)
    {
        if (state)
            m_intstat |= mask;
        else
            m_intstat &= ~(mask);
        
        m_hostcpu->set_input_line(MIPS3_IRQ0, state ? ASSERT_LINE : CLEAR_LINE);
    }
}

uint32_t spot_asic_device::ycbycr_to_rgb32_value(uint32_t ycbycr_val, bool secondPixel)
{
    uint8_t y = (ycbycr_val >> 0x18) & 0xff;
    if(secondPixel)
    {
        y = (ycbycr_val >> 0x08) & 0xff;
    }
    uint8_t cb = (ycbycr_val >> 0x10) & 0xff;
    uint8_t cr = (ycbycr_val >> 0x00) & 0xff;

    int8_t scb = cb - 128;
    int8_t scr = cr - 128;

    int32_t ir = static_cast<int32_t>(y + 1.402 * scr);
    int32_t ig = static_cast<int32_t>(y - 0.34414 * scb - 0.71414 * scr);
    int32_t ib = static_cast<int32_t>(y + 1.772 * scb);

    uint8_t r = std::max(0, std::min(255, ir));
    uint8_t g = std::max(0, std::min(255, ig));
    uint8_t b = std::max(0, std::min(255, ib));
    
    return (r<<0x10) | (g<<0x08) | (b);
}

uint32_t spot_asic_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    uint32_t adder = 0x72D80;//0x73f00;//0x72D80;
    uint32_t ystart = 0;
    uint32_t ymax = 420;
    uint32_t xstart = 0;
    uint32_t xmax = 560;
    //uint32_t adder = 0x0;

    if(m_vid_dmacntl & VID_DMACNTL_DMAEN) {
        //uint16_t *m_displayram = (uint16_t *)memshare(":ram")->ptr();
        
        if(0) {
            /*for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
            {
                address_space &space = m_hostcpu->space(AS_PROGRAM);

                uint32_t *colorptr = &m_frameColor[frame_addr_from_xy(0, y, false)];
                std::copy(colorptr + (cliprect.min_x * 2), colorptr + ((cliprect.max_x + 1) * 2), &bitmap.pix(y, cliprect.min_x));
            }*/
        } else {
            address_space &space = m_hostcpu->space(AS_PROGRAM);

            uint32_t bpp = 4;
            int actual_y = 0;
            for (int y = ystart; y < ymax; y++) {
                uint32_t *line = &bitmap.pix(y);

                if((y%2) == 0) {
                    for (int x = xstart; x < (xmax / 2); x++) {
                        uint32_t pixel1 = space.read_dword(m_vid_nstart + adder + (actual_y * (xmax) * bpp) + ((xmax / 2) * bpp) + (x * bpp));

                        /*uint8_t y = (pixel1 >> 0x18) & 0xff;
                        uint8_t u = (pixel1 >> 0x10) & 0xff;
                        uint8_t v = (pixel1 >> 0x00) & 0xff;
                        uint8_t uvg = u;
                        y -= 0x10;//kBlackY;
                        y = (((y << 8) + (1<<7)) / (0xEB - 0x10));//kRangeY);
                        uint8_t r = (y + v)   & 0xff;
                        uint8_t g = (y - uvg) & 0xff;
                        uint8_t b = (y + u)   & 0xff;
                        uint32_t pixel2 = (r<<0x18) | (g<<0x10) | (b);*/

                        *line++ = ycbycr_to_rgb32_value(pixel1, false);
                        *line++ = ycbycr_to_rgb32_value(pixel1, true);
                    }

                    actual_y++;
                } else {
                    for (int x = 0; x < (xmax / 2); x++) {
                        uint32_t pixel1 = space.read_dword(m_vid_nstart + adder + (actual_y * (xmax) * bpp) + (x * bpp));

                        *line++ = ycbycr_to_rgb32_value(pixel1, false);
                        *line++ = ycbycr_to_rgb32_value(pixel1, true);
                    }

                    /*if((y%2) == 0) {
                        actual_y++;
                    }*/
                }
            }
        }


        //printf("SCREEN UPDATE %08x -> %08x: %p:%08x\n", m_vid_nstart, m_vid_nsize, m_displayram, *m_displayram);
    }

    /*
    m_videotex->set_bitmap(m_videobitmap, m_videobitmap.cliprect(), TEXFORMAT_YUY16);

    // reset the screen contents
    screen.container().empty();

    // add the video texture
    rgb_t videocolor = 0xffffffff; // Fully visible, white
    if ((m_pot_cntl&POT_CNTL_ENABLE_OUTPUTS) == 0)
        videocolor = 0xff000000; // Blank the texture's RGB of the texture
    m_screen->container().add_quad(0.0f, 0.0f, 1.0f, 1.0f, videocolor, m_videotex, PRIMFLAG_BLENDMODE(BLENDMODE_NONE) | PRIMFLAG_SCREENTEX(1));
    */
    return 0;
}
