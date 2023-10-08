/***********************************************************************************************

    spot_asic.cpp

    WebTV Networks Inc. SPOT ASIC

    This ASIC controls most of the I/O on the 1st generation WebTV hardware.It is also referred
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
    m_sys_timer(nullptr) // when it goes off, timer interrupt fires
{
}

void spot_asic_device::bus_unit_map(address_map &map)
{
    // TODO: map out busUnit!
}

void spot_asic_device::rom_unit_map(address_map &map)
{
    // TODO: map out romUnit!
}

void spot_asic_device::aud_unit_map(address_map &map)
{
    // TODO: map out audUnit!
}

void spot_asic_device::vid_unit_map(address_map &map)
{
    // TODO: map out vidUnit!
}

void spot_asic_device::dev_unit_map(address_map &map)
{
    // TODO: map out devUnit!
}

void spot_asic_device::mem_unit_map(address_map &map)
{
    // TODO: map out memUnit! these appear to be unchanged in SOLO1, 
}

void spot_asic_device::device_start()
{
}

void spot_asic_device::device_reset()
{
}
