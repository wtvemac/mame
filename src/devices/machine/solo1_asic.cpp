/***********************************************************************************************

    solo1_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    Bus Registers (from official specification):
    0000 - BUS_CHIPID            - Chip version
    0004 - BUS_CHPCNTL           - Chip control

    0008 - BUS_INTSTAT           - Interrupt status
    0108 - BUS_INTSTAT Clear
    000c - BUS_INTEN             - Interrupt enable (R/set)
    010c - BUS_INTEN Clear

    0010 - BUS_ERRSTAT Set
    0110 - BUS_ERRSTAT Clear
    0014 - BUS_ERREN             - Error enable (R/set)
    0114 - BUS_ERREN Clear
    0018 - BUS_ERRADDR           - Error address

    0030 - BUS_WDVALUE           - Watchdog reset value

    0034 - BUS_LOWRDADDR         - Low memory read protection address
    0038 - BUS_LOWRDMASK         - Low memory read protection mask
    003c - BUS_LOWWRADDR         - Low memory write protection address
    0040 - BUS_LOWWRMASK         - Low memory write protection mask

    0048 - BUS_TCOUNT            - Timer counter
    004c - BUS_TCOMPARE          - Timer compare
    0050 - BUS_INTSTAT Set
    0054 - BUS_ERRSTAT           - Error status (R/set)

    0058 - BUS_GPINTSTAT         - GPIO interrupt status (write)
    0158 - BUS_GPINTSTAT Clear
    005c - BUS_GPINTEN           - GPIO interrupt enable (R/set)
    015c - BUS_GPINTEN Clear
    0060 - BUS_GPINTSTAT         - GPIO interrupt status (R/set)
    0064 - BUS_GPINTPOL          - GPIO interrupt polling

    0068 - BUS_AUDINTSTAT        - Audio interrupt status (write)
    0168 - BUS_AUDINTSTAT Clear
    006c - BUS_AUDINTSTAT        - Audio interrupt status (R/set)
    0070 - BUS_AUDINTEN          - Audio interrupt enable (R/set)
    0170 - BUS_AUDINTEN Clear

    0074 - BUS_DEVINTSTAT        - Device interrupt status (write)
    0174 - BUS_DEVINTSTAT Clear
    0078 - BUS_DEVINTSTAT        - Device interrupt status (R/set)
    007c - BUS_DEVINTEN          - Device interrupt enable (R/set)
    017c - BUS_DEVINTEN Clear

    0080 - BUS_VIDINTSTAT        - Video interrupt status (write)
    0180 - BUS_VIDINTSTAT Clear
    0084 - BUS_VIDINTSTAT        - Video interrupt status (R/set)
    0088 - BUS_VIDINTEN          - Video interrupt enable (R/set)
    0188 - BUS_VIDINTEN Clear

    008c - BUS_RIOINTSTAT        - RIO bus interrupt status (write)
    018c - BUS_RIOINTSTAT Clear
    0090 - BUS_RIOINTSTAT        - RIO bus interrupt status (R/set)
    0094 - BUS_RIOINTPOL         - RIO bus interrupt polling
    0098 - BUS_RIOINTEN          - RIO bus interrupt enable (R/set)
    0198 - BUS_RIOINTEN Clear

    009c - BUS_TIMINTSTAT        - Timing interrupt status (write)
    019c - BUS_TIMINTSTAT Clear
    00a0 - BUS_TIMINTSTAT        - Timing interrupt status (R/set)
    00a4 - BUS_TIMINTEN          - Timing interrupt enable (R/set)
    01a4 - BUS_TIMINTEN Clear

    00a8 - RESETCAUSE            - Reset cause (R/set)
    01a8 - RESETCAUSE Clear

    00b0 - BUS_J1FENLADDR        - Java1 R/W protection fence lower bound
    00b4 - BUS_J1FENHADDR        - Java1 R/W protection fence upper bound
    00b8 - BUS_J2FENLADDR        - Java2 R/W protection fence lower bound
    00bc - BUS_J2FENHADDR        - Java2 R/W protection fence upper bound
    00c0 - BUS_TOPOFRAM          - Total physical memory size
    00c4 - BUS_FENCECNTL         - Cancel writes to the J1 and J2 valid address ranges

    00c8 - BUS_BOOTMODE          - CPU reset string
    00cc - BUS_USEBOOTMODE       - Enable BOOTMODE

    The SOLO ASIC is split into multiple "units", of which this only emulates the busUnit.

************************************************************************************************/
#include "emu.h"
#include "solo1_asic.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_READS       (1U << 2)
#define LOG_WRITES      (1U << 3)
#define LOG_ERRORS      (1U << 4)
#define LOG_I2C_IGNORES (1U << 5)
#define LOG_DEFAULT     (LOG_READS | LOG_WRITES | LOG_ERRORS | LOG_I2C_IGNORES | LOG_UNKNOWN)

#define VERBOSE         (LOG_DEFAULT)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(SOLO1_ASIC, solo1_asic_device, "solo1_asic", "WebTV SOLO1 ASIC")

solo1_asic_device::solo1_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO1_ASIC, tag, owner, clock)
{
}

void solo1_asic_device::regs_map(address_map &map)
{
    map(0x0000, 0x0fff).rw(FUNC(solo1_asic_device::dma_bus_r), FUNC(solo1_asic_device::dma_bus_w)); // busUnit
    map(0x1000, 0x1fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // rioUnit
    map(0x2000, 0x2fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // audUnit
    map(0x3000, 0x3fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // vidUnit
    map(0x4000, 0x4fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // devUnit
    map(0x5000, 0x5fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // memUnit
    map(0x6000, 0x6fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // gfxUnit
    map(0x7000, 0x7fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // dveUnit
    map(0x8000, 0x8fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // divUnit
    map(0x9000, 0x9fff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // potUnit
    map(0xa000, 0xafff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // sucUnit
    map(0xb000, 0xbfff).rw(FUNC(solo1_asic_device::dma_unk_r), FUNC(solo1_asic_device::dma_unk_w)); // modUnit
}

void solo1_asic_device::device_start()
{
    m_chip_id = 0x03120000; // SOLO1 chip ID
}

void solo1_asic_device::device_reset()
{
    m_chip_cntl = 0;

    m_gpio_int_status = 0;
    m_gpio_int_enable = 0;

    m_aud_int_status = 0;
    m_aud_int_enable = 0;

    m_dev_int_status = 0;
    m_dev_int_enable = 0;

    m_rio_int_status = 0;
    m_rio_int_enable = 0;

    m_tim_int_status = 0;
    m_tim_int_enable = 0;

    m_memsize = 0x03ffffff; // the software determines the true RAM size during boot
}

uint32_t solo1_asic_device::dma_bus_r(offs_t offset)
{
    switch(offset & 0x1000)
    {
    case 0x000: // BUS_CHIPID (R/W)
        return m_chip_id;
    case 0x004: // BUS_CHIPCNTL (R/W)
        return m_chip_cntl;
    case 0x008: // BUS_INTSTAT (R/W)
        return m_int_status;
    case 0x00c: // BUS_INTEN (R/Set)
        return m_int_enable;
    case 0x010: // BUS_ERRSTAT (W)
        break;
    case 0x014: // BUS_ERREN (R/Set)
        return m_err_enable;
    case 0x018: // BUS_ERRADDR (R/W)
        return m_err_address;
    case 0x030: // BUS_WDVALUE (R/W)
        return m_wd_reset_val;
    case 0x034: // BUS_LOWRDADDR (R/W)
        return m_lomem_rdprot_addr;
    case 0x038: // BUS_LOWRDMASK (R/W)
        return m_lomem_rdprot_mask;
    case 0x03c: // BUS_LOWWRADDR (R/W)
        return m_lomem_wrprot_addr;
    case 0x040: // BUS_LOWWRMASK (R/W)
        return m_lomem_wrprot_mask;
    case 0x048: // BUS_TCOUNT (R/W)
        return m_tmr_count;
    case 0x04c: // BUS_TCOMPARE (R/W)
        return m_tmr_compare;
    default:
        printf("Attempted read from unimplemented or reserved register %04x\n", offset);
        break;
    }
    return 0;
}

void solo1_asic_device::dma_bus_w(offs_t offset, uint32_t data)
{
    switch(offset & 0x1000)
    {
    case 0x000: // BUS_CHIPID (R/W)
        // TODO: should this be the expected behavior?
        printf("Discarding write to BUS_CHIPID\n");
        break;
    case 0x004: // BUS_CHIPCNTL (R/W)
        m_chip_cntl = data;
        break;
    case 0x008: // BUS_INTSTAT (R/W)
        m_int_status = data;
        break;
    case 0x00c: // BUS_INTEN (R/Set)
        m_int_enable = data;
        break;
    case 0x010: // BUS_ERRSTAT (W)
        m_err_status = data;
        break;
    case 0x014: // BUS_ERREN (R/Set)
        m_err_enable = data;
        break;
    case 0x018: // BUS_ERRADDR (R/W)
        m_err_enable = data;
        break;
    case 0x030: // BUS_WDVALUE (R/W)
        m_wd_reset_val = data;
        break;
    case 0x034: // BUS_LOWRDADDR (R/W)
        m_lomem_rdprot_addr = data;
        break;
    case 0x038: // BUS_LOWRDMASK (R/W)
        m_lomem_rdprot_mask = data;
        break;
    case 0x03c: // BUS_LOWWRADDR (R/W)
        m_lomem_wrprot_addr = data;
        break;
    case 0x040: // BUS_LOWWRMASK (R/W)
        m_lomem_wrprot_mask = data;
        break;
    default:
        printf("Attempted write %08x to unimplemented or reserved register %04x\n", data, offset);
        break;
    }
}

uint32_t solo1_asic_device::dma_unk_r(offs_t offset)
{
    switch(offset/0x1000)
    {
    case SOLO_REGION_BUS:
        printf("Unimplemented busUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_RIO:
        printf("Unimplemented rioUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_AUD:
        printf("Unimplemented audUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_VID:
        printf("Unimplemented vidUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_DEV:
        printf("Unimplemented devUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_MEM:
        printf("Unimplemented memUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_GFX:
        printf("Unimplemented gfxUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_DVE:
        printf("Unimplemented dveUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_DIV:
        printf("Unimplemented divUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_POT:
        printf("Unimplemented potUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_SUC:
        printf("Unimplemented sucUnit read from %04x\n", offset);
        break;
    case SOLO_REGION_MOD:
        printf("Unimplemented modUnit read from %04x\n", offset);
        break;
    default:
        printf("Unknown SOLO I/O read from %04x\n", offset);
        break;
    }
    return 0;
}

void solo1_asic_device::dma_unk_w(offs_t offset, uint32_t data)
{
    switch(offset/0x1000)
    {
    case SOLO_REGION_BUS:
        printf("Unimplemented busUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_RIO:
        printf("Unimplemented rioUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_AUD:
        printf("Unimplemented audUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_VID:
        printf("Unimplemented vidUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_DEV:
        printf("Unimplemented devUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_MEM:
        printf("Unimplemented memUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_GFX:
        printf("Unimplemented gfxUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_DVE:
        printf("Unimplemented dveUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_DIV:
        printf("Unimplemented divUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_POT:
        printf("Unimplemented potUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_SUC:
        printf("Unimplemented sucUnit write %04x = %08x\n", offset, data);
        break;
    case SOLO_REGION_MOD:
        printf("Unimplemented modUnit write %04x = %08x\n", offset, data);
        break;
    default:
        printf("Unknown SOLO I/O write %04x = %08x\n", offset, data);
        break;
    }
}