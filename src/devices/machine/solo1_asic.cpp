/***********************************************************************************************

    solo1_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The technical specifications that this implementation is based on can be found here:
    http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf

    The SOLO ASIC is split into multiple "units", of which this implementation currently only
    emulates the busUnit and the memUnit. Depending on how these are used within a SOLO-based
    system, some units may be in their own devices.

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
	: device_t(mconfig, SOLO1_ASIC, tag, owner, clock),
    m_hostcpu(*this, finder_base::DUMMY_TAG)
{
}

void solo1_asic_device::regs_map(address_map &map)
{
    map(0x0000, 0x0fff).rw(FUNC(solo1_asic_device::dma_bus_r), FUNC(solo1_asic_device::dma_bus_w)); // busUnit
    //map(0x1000, 0x1fff).rw(FUNC(solo1_asic_device::dma_rio_r), FUNC(solo1_asic_device::dma_rio_w)); // rioUnit
    //map(0x2000, 0x2fff).rw(FUNC(solo1_asic_device::dma_aud_r), FUNC(solo1_asic_device::dma_aud_w)); // audUnit
    //map(0x3000, 0x3fff).rw(FUNC(solo1_asic_device::dma_vid_r), FUNC(solo1_asic_device::dma_vid_w)); // vidUnit
    //map(0x4000, 0x4fff).rw(FUNC(solo1_asic_device::dma_dev_r), FUNC(solo1_asic_device::dma_dev_w)); // devUnit
    map(0x5000, 0x5fff).rw(FUNC(solo1_asic_device::dma_mem_r), FUNC(solo1_asic_device::dma_mem_w)); // memUnit
    //map(0x6000, 0x6fff).rw(FUNC(solo1_asic_device::dma_gfx_r), FUNC(solo1_asic_device::dma_gfx_w)); // gfxUnit
    //map(0x7000, 0x7fff).rw(FUNC(solo1_asic_device::dma_dve_r), FUNC(solo1_asic_device::dma_dve_w)); // dveUnit
    //map(0x8000, 0x8fff).rw(FUNC(solo1_asic_device::dma_div_r), FUNC(solo1_asic_device::dma_div_w)); // divUnit
    //map(0x9000, 0x9fff).rw(FUNC(solo1_asic_device::dma_pot_r), FUNC(solo1_asic_device::dma_pot_w)); // potUnit
    //map(0xa000, 0xafff).rw(FUNC(solo1_asic_device::dma_suc_r), FUNC(solo1_asic_device::dma_suc_w)); // sucUnit
    //map(0xb000, 0xbfff).rw(FUNC(solo1_asic_device::dma_mod_r), FUNC(solo1_asic_device::dma_mod_w)); // modUnit
}

void solo1_asic_device::device_add_mconfig(machine_config &config)
{

}

void solo1_asic_device::device_start()
{
    m_bus_chip_id = 0x03120000; // SOLO1 chip ID
}

void solo1_asic_device::device_reset()
{
    m_bus_chip_cntl = 0;

    m_bus_gpio_int_status = 0;
    m_bus_gpio_int_enable = 0;

    m_bus_aud_int_status = 0;
    m_bus_aud_int_enable = 0;

    m_bus_dev_int_status = 0;
    m_bus_dev_int_enable = 0;

    m_bus_rio_int_status = 0;
    m_bus_rio_int_enable = 0;

    m_bus_tim_int_status = 0;
    m_bus_tim_int_enable = 0;

    m_bus_memsize = 0x03ffffff; // the software determines the true RAM size during boot
}

uint32_t solo1_asic_device::dma_bus_r(offs_t offset)
{
    printf("SOLO read: busUnit %04x\n", offset*4);
    switch(offset*4)
    {
    case 0x000: // BUS_CHIPID (R/W)
        return m_bus_chip_id;
    case 0x004: // BUS_CHIPCNTL (R/W)
        return m_bus_chip_cntl;
    case 0x008: // BUS_INTSTAT (R/W)
        return m_bus_int_status;
    case 0x00c: // BUS_INTEN (R/Set)
        return m_bus_int_enable;
    case 0x010: // BUS_ERRSTAT (W)
        break;
    case 0x110: // BUS_ERRSTAT (Clear)
        break;
    case 0x014: // BUS_ERREN (R/Set)
        return m_bus_err_enable;
    case 0x114: // BUS_ERREN (Clear)
        break;
    case 0x018: // BUS_ERRADDR (R/W)
        return m_bus_err_address;
    case 0x030: // BUS_WDVALUE (R/W)
        return m_bus_wd_reset_val;
    case 0x034: // BUS_LOWRDADDR (R/W)
        return m_bus_lomem_rdprot_addr;
    case 0x038: // BUS_LOWRDMASK (R/W)
        return m_bus_lomem_rdprot_mask;
    case 0x03c: // BUS_LOWWRADDR (R/W)
        return m_bus_lomem_wrprot_addr;
    case 0x040: // BUS_LOWWRMASK (R/W)
        return m_bus_lomem_wrprot_mask;
    case 0x048: // BUS_TCOUNT (R/W)
        return m_bus_tmr_count;
    case 0x04c: // BUS_TCOMPARE (R/W)
        return m_bus_tmr_compare;
    case 0x050: // BUS_INTSTAT (Set)
        break;
    case 0x054: // BUS_ERRSTAT (R/Set)
        return m_bus_err_status;
    case 0x058: // BUS_GPINTSTAT (W)
        break;
    case 0x05c: // BUS_GPINTEN (R/Set)
        return m_bus_gpio_int_enable;
    case 0x15c: // BUS_GPINTEN (Clear)
        break;
    case 0x060: // BUS_GPINTSTAT (R/Set)
        return m_bus_gpio_int_status;
    case 0x064: // BUS_GPINTPOL (R/W)
        return m_bus_gpio_int_polling;
    case 0x068: // BUS_AUDINTSTAT (W)
        break;
    case 0x168: // BUS_AUDINTSTAT (Clear)
        break;
    case 0x06c: // BUS_AUDINTSTAT (R/Set)
        return m_bus_aud_int_status;
    case 0x070: // BUS_AUDINTEN (R/Set)
        return m_bus_aud_int_enable;
    case 0x170: // BUS_AUDINTEN (Clear)
        break;
    case 0x074: // BUS_DEVINTSTAT (W)
        break;
    case 0x174: // BUS_DEVINTSTAT (Clear)
        break;
    case 0x078: // BUS_DEVINTSTAT (R/Set)
        return m_bus_dev_int_status;
    case 0x07c: // BUS_DEVINTEN (R/Set)
        return m_bus_dev_int_enable;
    case 0x17c: // BUS_DEVINTEN (Clear)
        break;
    case 0x080: // BUS_VIDINTSTAT (W)
        break;
    case 0x180: // BUS_VIDINTSTAT (Clear)
        break;
    case 0x084: // BUS_VIDINTSTAT (R/Set)
        return m_bus_vid_int_status;
    case 0x088: // BUS_VIDINTEN (R/Set)
        return m_bus_vid_int_enable;
    case 0x188: // BUS_VIDINTEN (Clear)
        break;
    case 0x08c: // BUS_RIOINTSTAT (W)
        break;
    case 0x18c: // BUS_RIOINTSTAT (Clear)
        break;
    case 0x094: // BUS_RIOINTPOL (R/W)
        return m_bus_rio_int_polling;
    case 0x090: // BUS_RIOINTSTAT (R/Set)
        return m_bus_rio_int_status;
    case 0x098: // BUS_DEVINTEN (R/Set)
        return m_bus_rio_int_enable;
    case 0x198: // BUS_DEVINTEN (Clear)
        break;
    case 0x09c: // BUS_TIMINTSTAT (W)
        break;
    case 0x19c: // BUS_TIMINTSTAT (Clear)
        break;
    case 0x0a0: // BUS_TIMINTSTAT (R/Set)
        return m_bus_tim_int_status;
    case 0x0a4: // BUS_TIMINTEN (R/Set)
        return m_bus_tim_int_enable;
    case 0x1a4: // BUS_TIMINTEN (Clear)
        break;
    case 0x0a8: // RESETCAUSE (R/Set)
        return m_bus_reset_cause;
    case 0x0ac: // RESETCAUSE (Clear)
        break;
    case 0x0b0: // BUS_J1FENLADDR (R/W)
        return m_bus_java1_fence_addr_l;
    case 0x0b4: // BUS_J1FENHADDR (R/W)
        return m_bus_java1_fence_addr_h;
    case 0x0b8: // BUS_J2FENLADDR (R/W)
        return m_bus_java2_fence_addr_l;
    case 0x0bc: // BUS_J2FENHADDR (R/W)
        return m_bus_java2_fence_addr_h;
    case 0x0c0: // BUS_TOPOFRAM (R/W)
        return m_bus_memsize;
    case 0x0c4: // BUS_FENCECNTL (R/W)
        return m_bus_fence_cntl;
    case 0x0c8: // BUS_BOOTMODE (R/W)
        return m_bus_bootmode;
    case 0x0cc: // BUS_USEBOOTMODE (R/W)
        return m_bus_use_bootmode;
    default:
        logerror("Attempted read from reserved register 0%03x!\n", offset*4);
        break;
    }
    return 0;
}

void solo1_asic_device::dma_bus_w(offs_t offset, uint32_t data)
{
    printf("SOLO write: busUnit %08x to %04x\n", data, offset*4);
    switch(offset*4)
    {
    case 0x000: // BUS_CHIPID (R/W)
        logerror("Discarding write (%08x) to BUS_CHIPID\n", data); // Presumed behavior, there is no real need to write to this register
        break;
    case 0x004: // BUS_CHIPCNTL (R/W)
        m_bus_chip_cntl = data;
        break;
    case 0x008: // BUS_INTSTAT (R/W)
        m_bus_int_status = data;
        break;
    case 0x108: // BUS_INTSTAT (Clear)
        m_bus_int_status &= ~data;
        break;
    case 0x00c: // BUS_INTEN (R/Set)
        m_bus_int_enable |= data;
        break;
    case 0x010: // BUS_ERRSTAT (W)
        m_bus_err_status = data;
        break;
    case 0x110: // BUS_ERRSTAT (Clear)
        m_bus_err_status &= ~data;
        break;
    case 0x014: // BUS_ERREN (R/Set)
        m_bus_err_enable |= data;
        break;
    case 0x114: // BUS_ERREN (Clear)
        m_bus_err_enable &= ~data;
        break;
    case 0x018: // BUS_ERRADDR (R/W)
        m_bus_err_enable = data;
        break;
    case 0x030: // BUS_WDVALUE (R/W)
        m_bus_wd_reset_val = data;
        break;
    case 0x034: // BUS_LOWRDADDR (R/W)
        m_bus_lomem_rdprot_addr = data;
        break;
    case 0x038: // BUS_LOWRDMASK (R/W)
        m_bus_lomem_rdprot_mask = data;
        break;
    case 0x03c: // BUS_LOWWRADDR (R/W)
        m_bus_lomem_wrprot_addr = data;
        break;
    case 0x040: // BUS_LOWWRMASK (R/W)
        m_bus_lomem_wrprot_mask = data;
        break;
    case 0x048: // BUS_TCOUNT (R/W)
        m_bus_tmr_count = data;
        break;
    case 0x04c: // BUS_TCOMPARE (R/W)
        m_bus_tmr_compare = data;
        break;
    case 0x050: // BUS_INTSTAT (Set)
        m_bus_int_status |= data;
        break;
    case 0x054: // BUS_ERRSTAT (R/Set)
        m_bus_err_status |= data;
        break;
    case 0x058: // BUS_GPINTSTAT (W)
        m_bus_gpio_int_status = data;
        break;
    case 0x158: // BUS_GPINTSTAT (Clear)
        m_bus_gpio_int_status &= ~data;
        break;
    case 0x05c: // BUS_GPINTEN (R/Set)
        m_bus_gpio_int_enable |= data;
        break;
    case 0x15c: // BUS_GPINTEN (Clear)
        m_bus_gpio_int_enable &= ~data;
        break;
    case 0x060: // BUS_GPINTSTAT (W)
        m_bus_gpio_int_status = data;
        break;
    case 0x064: // BUS_GPINTPOL (R/W)
        m_bus_gpio_int_polling = data;
        break;
    case 0x068: // BUS_AUDINTSTAT (W)
        m_bus_aud_int_status = data;
        break;
    case 0x168: // BUS_AUDINTSTAT (Clear)
        m_bus_aud_int_status &= ~data;
        break;
    case 0x06c: // BUS_AUDINTSTAT (R/Set)
        m_bus_aud_int_status |= data;
        break;
    case 0x070: // BUS_AUDINTEN (R/Set)
        m_bus_aud_int_enable |= data;
        break;
    case 0x170: // BUS_AUDINTEN (Clear)
        m_bus_aud_int_enable &= ~data;
        break;
    case 0x074: // BUS_DEVINTSTAT (W)
        m_bus_dev_int_status = data;
        break;
    case 0x174: // BUS_DEVINTSTAT (Clear)
        m_bus_dev_int_status &= ~data;
        break;
    case 0x078: // BUS_DEVINTSTAT (R/Set)
        m_bus_dev_int_status |= data;
        break;
    case 0x07c: // BUS_DEVINTEN (R/Set)
        m_bus_dev_int_enable |= data;
        break;
    case 0x17c: // BUS_DEVINTEN (Clear)
        m_bus_dev_int_enable &= ~data;
        break;
    case 0x080: // BUS_VIDINTSTAT (W)
        m_bus_vid_int_status = data;
        break;
    case 0x180: // BUS_VIDINTSTAT (Clear)
        m_bus_vid_int_status &= ~data;
        break;
    case 0x084: // BUS_VIDINTSTAT (R/Set)
        m_bus_vid_int_status |= data;
        break;
    case 0x088: // BUS_VIDINTEN (R/Set)
        m_bus_vid_int_enable |= data;
        break;
    case 0x188: // BUS_VIDINTEN (Clear)
        m_bus_vid_int_enable &= ~data;
        break;
    case 0x08c: // BUS_RIOINTSTAT (W)
        m_bus_rio_int_status = data;
        break;
    case 0x18c: // BUS_RIOINTSTAT (Clear)
        m_bus_rio_int_status &= ~data;
        break;
    case 0x090: // BUS_RIOINTSTAT (R/Set)
        m_bus_rio_int_status |= data;
        break;
    case 0x094: // BUS_RIOINTPOL (R/W)
        m_bus_rio_int_polling = data;
        break;
    case 0x098: // BUS_RIOINTEN (R/Set)
        m_bus_rio_int_enable |= data;
        break;
    case 0x198: // BUS_RIOINTEN (Clear)
        m_bus_rio_int_enable &= ~data;
        break;
    case 0x09c: // BUS_DEVINTSTAT (W)
        m_bus_tim_int_status = data;
        break;
    case 0x19c: // BUS_DEVINTSTAT (Clear)
        m_bus_tim_int_status &= ~data;
        break;
    case 0x0a0: // BUS_DEVINTSTAT (R/Set)
        m_bus_tim_int_status |= data;
        break;
    case 0x0a4: // BUS_DEVINTEN (R/Set)
        m_bus_tim_int_enable |= data;
        break;
    case 0x1a4: // BUS_DEVINTEN (Clear)
        m_bus_tim_int_enable &= ~data;
        break;
    case 0x0a8: // RESETCAUSE (R/Set)
        m_bus_reset_cause |= data;
        break;
    case 0x0ac: // RESETCAUSE (Clear)
        m_bus_reset_cause &= ~data;
        break;
    case 0x0b0: // BUS_J1FENLADDR (R/W)
        m_bus_java1_fence_addr_l = data;
        break;
    case 0x0b4: // BUS_J1FENHADDR (R/W)
        m_bus_java1_fence_addr_h = data;
        break;
    case 0x0b8: // BUS_J2FENLADDR (R/W)
        m_bus_java2_fence_addr_l = data;
        break;
    case 0x0bc: // BUS_J2FENHADDR (R/W)
        m_bus_java2_fence_addr_h = data;
        break;
    case 0x0c0: // BUS_TOPOFRAM (R/W)
        m_bus_memsize = data;
        break;
    case 0x0c4: // BUS_FENCECNTL (R/W)
        m_bus_fence_cntl = data;
        break;
    case 0x0c8: // BUS_BOOTMODE (R/W)
        m_bus_bootmode = data;
        break;
    case 0x0cc: // BUS_USEBOOTMODE (R/W)
        m_bus_use_bootmode = data;
        break;
    default:
        logerror("Attempted write (%08x) to reserved register 0%03x!\n", data, offset*4);
        break;
    }
}

uint32_t solo1_asic_device::dma_mem_r(offs_t offset)
{
    printf("SOLO read: memUnit %04x\n", offset*4);
    switch(offset*4)
    {
    case 0x000: // MEM_TIMING (R/W)
        return m_mem_timing;
    case 0x004: // MEM_CNTL (R/W)
        return m_mem_cntl;
    case 0x008: // MEM_BURP (R/W)
        return m_mem_burp;
    case 0x00c: // MEM_REFCNTL (R/W)
        return m_mem_refresh_cntl;
    case 0x010: // MEM_CMD (R/W)
        return m_mem_cmd;
    default:
        logerror("Attempted read from reserved register 5%03x!\n", offset*4);
        break;
    }
    return 0;
}

void solo1_asic_device::dma_mem_w(offs_t offset, uint32_t data)
{
    printf("SOLO write: memUnit %08x to %04x\n", data, offset*4);
    switch(offset*4)
    {
    case 0x000: // MEM_TIMING (R/W)
        m_mem_timing = data;
        break;
    case 0x004: // MEM_CNTL (R/W)
        m_mem_cntl = data;
        break;
    case 0x008: // MEM_BURP (R/W)
        m_mem_burp = data;
        break;
    case 0x00c: // MEM_REFCNTL (R/W)
        m_mem_refresh_cntl = data;
        break;
    case 0x010: // MEM_CMD (R/W)
        m_mem_cmd = data;
        break;
    default:
        logerror("Attempted write (%08x) to reserved register 5%03x!\n", data, offset*4);
        break;
    }
}