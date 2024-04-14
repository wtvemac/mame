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
    m_sys_timer(nullptr) // when it goes off, timer interrupt fires
{
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
    map(0x01c, 0x01f).rw(FUNC(spot_asic_device::reg_301c_r), FUNC(spot_asic_device::reg_301c_w));
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
    m_screen->set_size(VID_DEFAULT_WIDTH, VID_DEFAULT_HEIGHT);
    m_screen->set_visarea(0, VID_DEFAULT_WIDTH, 0, VID_DEFAULT_HEIGHT);
    m_screen->set_refresh_hz(VID_DEFAULT_HZ);

    m_screen->set_screen_update(FUNC(spot_asic_device::screen_update));

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
    m_vid_hstart = VID_DEFAULT_HSTART;
    m_vid_hsize = VID_DEFAULT_HSIZE;
    m_vid_vstart = VID_DEFAULT_VSTART;
    m_vid_vsize = VID_DEFAULT_VSIZE;
    m_vid_blank_color = VID_DEFAULT_COLOR;
    m_vid_hintline = 0x0;
    m_vid_cstart = 0x0;
    m_vid_csize = 0x0;
    m_vid_ccnt = 0x0;
    m_vid_cline = 0x0;
    m_vid_cline_cyccnt = 0x0;
    m_vid_cline_direct = false;
    m_ledstate = 0xFFFFFFFF;

    emc_bitcount = 0x0;
    emc_byte = 0x0;
    emc_vbltimer = 0x0;
}

void spot_asic_device::device_reset()
{
    spot_asic_device::device_start();
}

void spot_asic_device::validate_active_area()
{
    // hsize and vsize changes will break the screen but it would break on hardware.

    // The active h size can't be larger than the screen width.
    if(m_vid_hsize > m_screen->width())
    {
        m_vid_hsize = m_screen->width();
    }

    // The active v size can't be larger than the screen height.
    if(m_vid_vsize > m_screen->height())
    {
        m_vid_vsize = m_screen->height();
    }

    // hsize and vsize need to be a multiple of 2
    m_vid_hsize = (m_vid_hsize / 2) * 2;
    m_vid_vsize = (m_vid_vsize / 2) * 2;

    // The active h offset (hstart) can't push the active area off the screen.
    if((m_vid_hstart + m_vid_hsize) > m_screen->width())
    {
        m_vid_hstart = (m_screen->width() - m_vid_hsize) / 2; // to screen center
    }
    

    // The active v offset (vstart) can't push the active area off the screen.
    if((m_vid_vstart + m_vid_vsize) > m_screen->height())
    {
        m_vid_vstart = (m_screen->height() - m_vid_vsize) / 2; // to screen center
    }

    // hstart and vstart need to be a multiple of 8
    m_vid_hstart = (m_vid_hstart / 8) * 8;
    m_vid_vstart = (m_vid_vstart / 8) * 8;
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
    return m_vid_cstart;
}

uint32_t spot_asic_device::reg_3004_r()
{
    logerror("%s: reg_3004_r (VID_CSIZE)\n", machine().describe_context());
    return m_vid_csize;
}

uint32_t spot_asic_device::reg_3008_r()
{
    logerror("%s: reg_3008_r (VID_CCNT)\n", machine().describe_context());
    return m_vid_ccnt;
}

uint32_t spot_asic_device::reg_300c_r()
{
    logerror("%s: reg_300c_r (VID_NSTART)\n", machine().describe_context());
    return m_vid_nstart;
}

void spot_asic_device::reg_300c_w(uint32_t data)
{
    m_vid_nstart = data;

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

    logerror("%s: reg_3014_w %08x (VID_DMACNTL)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_301c_r()
{
    logerror("%s: reg_301c_r (VID_BLNKCOL)\n", machine().describe_context());
    return m_vid_blank_color;
}

void spot_asic_device::reg_301c_w(uint32_t data)
{
    m_vid_blank_color = data;

    logerror("%s: reg_301c_r %08x (VID_BLNKCOL)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_3020_r()
{
    logerror("%s: reg_3020_r (VID_HSTART)\n", machine().describe_context());
    return m_vid_hstart;
}

void spot_asic_device::reg_3020_w(uint32_t data)
{
    m_vid_hstart = data;

    spot_asic_device::validate_active_area();

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

    spot_asic_device::validate_active_area();

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

    spot_asic_device::validate_active_area();
    
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

    spot_asic_device::validate_active_area();
    
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
    if(!m_vid_cline_direct)
    {
        /*
            Builds uses this to calculate the CPU speed. 
            
            Emulation has this decoupled so it doesn't work. I'll try to accomidate the CPU speed calculation but use the correct value set in screen_update on an interrupt.

            The build uses the time it takes to draw one frames as a time base (33.3333ms for two fields @ 60hz) then counts the number of CPU cycles it takes to do that

            speed = cycles * (2 clock ticks / cycle) * (2 fields or 1 frame) / (33.3333 ms) * (1000 ms/s)

            Steps:

            1. The build grabs the current line position.
            2. The build waits until the next line then grabs the current cycle count.
            3. The build waits until it returns to the line position in step 1 then grabs the current cycle count again.
            4. The cycle count different between steps 3 and 1 is made. We should be able to compute the CPU clock speed using the frame rate.

            Then the build takes that and multiplies it by 12, 10 or 120 for whatever reason. I assume to adjust for any counter error.
        */

        uint32_t clocks_per_frame = (m_hostcpu->clock() / 2) / ATTOSECONDS_TO_HZ(m_screen->refresh_attoseconds());
        uint32_t current_cyccnt = m_hostcpu->total_cycles();

        if(m_vid_cline_cyccnt == 0 || (current_cyccnt - m_vid_cline_cyccnt) >= clocks_per_frame)
        {
            m_vid_cline = 0;
            m_vid_cline_cyccnt = current_cyccnt;
        }
        else
        {
            m_vid_cline = 1;
        }
    }

    logerror("%s: reg_3034_r (VID_CLINE)\n", machine().describe_context());

    return m_vid_cline;
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

uint32_t spot_asic_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    uint16_t screen_width = bitmap.width();
    uint16_t screen_height =  bitmap.height();

    m_vid_cstart = m_vid_nstart;
    m_vid_csize = m_vid_nsize;
    m_vid_ccnt = m_vid_cstart;// + m_vid_csize;

    address_space &space = m_hostcpu->space(AS_PROGRAM);

    for (int y = 0; y < screen_height; y++)
    {
        uint32_t *line = &bitmap.pix(y);

        m_vid_cline = y;

        for (int x = 0; x < screen_width; x += 2)
        {
            uint32_t pixel = 0x00000000;

            bool is_active_area = (
                y >= m_vid_vstart
                && y < (m_vid_vstart + m_vid_vsize)

                && x >= m_vid_hstart
                && x < (m_vid_hstart + m_vid_hsize)
            );

            if(m_vid_dmacntl & VID_DMACNTL_DMAEN && is_active_area)
            {
                pixel = space.read_dword(m_vid_ccnt);

                m_vid_ccnt += 2 * VID_BYTES_PER_PIXEL;
            }
            else
            {
                int32_t blnkY = (m_vid_blank_color >> 0x10) & 0xff;
                int32_t blnkCr = (m_vid_blank_color >> 0x08) & 0xff;
                int32_t blnkCb = m_vid_blank_color & 0xff;

                pixel =  blnkY << 0x18 | blnkCb << 0x10 | blnkY << 0x08 | blnkCr;
            }

            int32_t y1 = ((pixel >> 0x18) & 0xff) - VID_Y_BLACK;
            int32_t Cb  = ((pixel >> 0x10) & 0xff) - VID_UV_OFFSET;
            int32_t y2 = ((pixel >> 0x08) & 0xff) - VID_Y_BLACK;
            int32_t Cr  = ((pixel) & 0xff) - VID_UV_OFFSET;

            y1 = (((y1 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);
            y2 = (((y2 << 8) + VID_UV_OFFSET) / VID_Y_RANGE);

            int32_t r = ((0x166 * Cr) + VID_UV_OFFSET) >> 8;
            int32_t b = ((0x1C7 * Cb) + VID_UV_OFFSET) >> 8;
            int32_t g = ((0x32 * b) + (0x83 * r) + VID_UV_OFFSET) >> 8;

            *line++ = (
                std::clamp(y1 + r, 0x00, 0xff) << 0x10
                | std::clamp(y1 - g, 0x00, 0xff) << 0x08
                | std::clamp(y1 + b, 0x00, 0xff)
            );

            *line++ = (
                std::clamp(y2 + r, 0x00, 0xff) << 0x10
                | std::clamp(y2 - g, 0x00, 0xff) << 0x08
                | std::clamp(y2 + b, 0x00, 0xff)
            );
        }
    }

    return 0;
}
