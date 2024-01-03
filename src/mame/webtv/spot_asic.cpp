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

#include "spot_asic.h"

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
    m_hostcpu(*this, finder_base::DUMMY_TAG),
    m_serial_id(*this, finder_base::DUMMY_TAG),
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
    //map(0x00c, 0x00f).rw(FUNC(spot_asic_device::reg_300c_r), FUNC(spot_asic_device::reg_300c_w));
    //map(0x010, 0x013).rw(FUNC(spot_asic_device::reg_3010_r), FUNC(spot_asic_device::reg_3010_w));
    //map(0x014, 0x017).rw(FUNC(spot_asic_device::reg_3014_r), FUNC(spot_asic_device::reg_3014_w));
    //map(0x018, 0x01b).rw(FUNC(spot_asic_device::reg_3018_r), FUNC(spot_asic_device::reg_3018_w));
    //map(0x01c, 0x01f).rw(FUNC(spot_asic_device::reg_301c_r), FUNC(spot_asic_device::reg_301c_w));
    //map(0x020, 0x023).rw(FUNC(spot_asic_device::reg_3020_r), FUNC(spot_asic_device::reg_3020_w));
    //map(0x024, 0x027).rw(FUNC(spot_asic_device::reg_3024_r), FUNC(spot_asic_device::reg_3024_w));
    //map(0x028, 0x02b).rw(FUNC(spot_asic_device::reg_3028_r), FUNC(spot_asic_device::reg_3028_w));
    //map(0x02c, 0x02f).rw(FUNC(spot_asic_device::reg_302c_r), FUNC(spot_asic_device::reg_302c_w));
    //map(0x030, 0x033).rw(FUNC(spot_asic_device::reg_3030_r), FUNC(spot_asic_device::reg_3030_w));
    //map(0x034, 0x037).r(FUNC(spot_asic_device::reg_3034_r));
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
    //map(0x020, 0x023).rw(FUNC(spot_asic_device::reg_4020_r), FUNC(spot_asic_device::reg_4020_w));
    //map(0x024, 0x027).rw(FUNC(spot_asic_device::reg_4024_r), FUNC(spot_asic_device::reg_4024_w));
    //map(0x028, 0x02b).rw(FUNC(spot_asic_device::reg_4028_r), FUNC(spot_asic_device::reg_4028_w));
    //map(0x02c, 0x02f).rw(FUNC(spot_asic_device::reg_402c_r), FUNC(spot_asic_device::reg_402c_w));
    //map(0x030, 0x033).rw(FUNC(spot_asic_device::reg_4030_r), FUNC(spot_asic_device::reg_4030_w));
    //map(0x034, 0x037).rw(FUNC(spot_asic_device::reg_4034_r), FUNC(spot_asic_device::reg_4034_w));
    //map(0x038, 0x03b).rw(FUNC(spot_asic_device::reg_4038_r), FUNC(spot_asic_device::reg_4038_w));
    //map(0x03c, 0x03f).rw(FUNC(spot_asic_device::reg_403c_r), FUNC(spot_asic_device::reg_403c_w));
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
}

uint32_t spot_asic_device::reg_0000_r()
{
    // This appears to be read a lot.
	//logerror("%s: reg_0000_r (BUS_CHIPID)\n", machine().describe_context());
    return 0x10100000;
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
    return 0x00000000;
}

void spot_asic_device::reg_0108_w(uint32_t data)
{
	logerror("%s: reg_0108_w %08x (BUS_INTSTAT clear)\n", machine().describe_context(), data);
}

uint32_t spot_asic_device::reg_000c_r()
{
	logerror("%s: reg_000c_r (BUS_INTEN)\n", machine().describe_context());
    return 0x00000000;
}

void spot_asic_device::reg_000c_w(uint32_t data)
{
	logerror("%s: reg_000c_w %08x (BUS_INTEN)\n", machine().describe_context(), data);
}

void spot_asic_device::reg_010c_w(uint32_t data)
{
	logerror("%s: reg_000c_w %08x (BUS_INTEN clear)\n", machine().describe_context(), data);
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

uint32_t spot_asic_device::reg_4000_r()
{
    logerror("%s: reg_4000_r (DEV_IRDATA)\n", machine().describe_context());
    // TODO: figure out what kind of IR controller this should probe
    return 0;
}

uint32_t spot_asic_device::reg_4004_r()
{
    logerror("%s: reg_4004_r (DEV_LED)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_4004_w(uint32_t data)
{
    logerror("%s: reg_4004_w %08x (DEV_LED)\n", machine().describe_context(), data);
    // TODO: write to the LED devices!
}

uint32_t spot_asic_device::reg_4008_r()
{
    logerror("%s: reg_4008_r (DEV_IDCNTL)\n", machine().describe_context());
    return (m_serial_id->read() + (m_serial_id_tx << 1));
}

void spot_asic_device::reg_4008_w(uint32_t data)
{
    m_serial_id_tx = BIT(data, 1);
    logerror("%s: reg_4008_w %08x - write %d (DEV_IDCNTL)\n", machine().describe_context(), data, m_serial_id_tx);
    m_serial_id->write(m_serial_id_tx);
}

uint32_t spot_asic_device::reg_400c_r()
{
    logerror("%s: reg_400c_r (DEV_NVCNTL)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_400c_w(uint32_t data)
{
    logerror("%s: reg_400c_w %08x (DEV_NVCNTL)\n", machine().describe_context(), data);
    // TODO: Check what kind of EEPROM is required
}

uint32_t spot_asic_device::reg_4010_r()
{
    logerror("%s: reg_4010_r (DEV_SCCNTL)\n", machine().describe_context());
    return 0;
}

void spot_asic_device::reg_4010_w(uint32_t data)
{
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
    // This is defined as a write-only register, yet the WebTV software reads from it? Still need to see how this behaves on real hardware.
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
