/***************************************************************************************************

    solo1_asic.cpp

    WebTV Networks Inc. SOLO1 ASIC

    This ASIC controls most of the I/O on the 2nd generation WebTV hardware.

    This implementation is based off of both the archived technical specifications, as well as
    the various reverse-engineering efforts of the WebTV community.

    The technical specifications that this implementation is based on can be found here:
    http://wiki.webtv.zone/misc/SOLO1/SOLO1_ASIC_Spec.pdf

    The SOLO ASIC is split into multiple "units", of which this implementation currently only
    emulates the busUnit, the memUnit, and the devUnit.

    The rioUnit (0xA4001xxx) provides a shared interface to the ROM, asynchronous devices (including
    the modem, the IDE hard drive, and the IDE CD-ROM), and synchronous devices which plug into the
    WebTV Port connector (which did not see much use other than for the FIDO/FCS printer interface).

    The audUnit (0xA4002xxx) handles audio DMA.

    The vidUnit (0xA4003xxx) handles video DMA.

    The devUnit (0xA4004xxx) handles GPIO, IR input, IR blaster output, front panel LEDs, and the
    parallel port.

    The memUnit (0xA4005xxx) handles memory timing and other memory-related operations. These
    registers are only emulated for completeness; they do not currently have an effect on the
    emulation.

    The gfxUnit (0xA4006xxx) is responsible for accelerated graphics.

    The dveUnit (0xA4007xxx) is responsible for digital video encoding.

    The divUnit (0xA4008xxx) is responsible for video input decoding.

    The potUnit (0xA4009xxx) handles low-level video output.

    The sucUnit (0xA400Axxx) handles serial I/O for both the RS232 port and the SmartCard reader.

    The modUnit (0xA400Bxxx) handles softmodem I/O. It's basically a stripped down audUnit.

****************************************************************************************************/
#include "emu.h"

#include "solo1_asic.h"
#include "solo1_asic_vid.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_READS       (1U << 2)
#define LOG_WRITES      (1U << 3)
#define LOG_ERRORS      (1U << 4)
#define LOG_I2C_IGNORES (1U << 5)
#define LOG_DEFAULT     (LOG_READS | LOG_WRITES | LOG_ERRORS | LOG_I2C_IGNORES | LOG_UNKNOWN)

#define VERBOSE         (LOG_DEFAULT)
#include "logmacro.h"

#define BUS_INTSTAT_VIDEO 1 << 7 // Video interrupt
#define BUS_INTSTAT_AUDIO 1 << 6 // Audio interrupt
#define BUS_INTSTAT_RIO   1 << 5 // RIO device interrupt
#define BUS_INTSTAT_SOLO1 1 << 4 // SOLO1 device interrupt
#define BUS_INTSTAT_TIMER 1 << 3 // Timer interrupt
#define BUS_INTSTAT_FENCE 1 << 2 // Fence (error) interrupt

#define BUS_GPINTSTAT_15 1 << 17
#define BUS_GPINTSTAT_14 1 << 16
#define BUS_GPINTSTAT_13 1 << 15
#define BUS_GPINTSTAT_12 1 << 14
#define BUS_GPINTSTAT_11 1 << 13
#define BUS_GPINTSTAT_10 1 << 12
#define BUS_GPINTSTAT_9  1 << 11
#define BUS_GPINTSTAT_8  1 << 10
#define BUS_GPINTSTAT_7  1 << 9
#define BUS_GPINTSTAT_6  1 << 8
#define BUS_GPINTSTAT_5  1 << 7
#define BUS_GPINTSTAT_4  1 << 6
#define BUS_GPINTSTAT_3  1 << 5
#define BUS_GPINTSTAT_2  1 << 4
#define BUS_GPINTSTAT_1  1 << 3
#define BUS_GPINTSTAT_0  1 << 2

#define BUS_AUD_INTSTAT_SOFTMODEM_DMA_IN  1 << 6 // modUnit DMA input interrupt
#define BUS_AUD_INTSTAT_SOFTMODEM_DMA_OUT 1 << 5 // modUnit DMA output interrupt
#define BUS_AUD_INTSTAT_DIV_AUDIO         1 << 4 // divUnit audio interrupt
#define BUS_AUD_INTSTAT_DMA_IN            1 << 3 // audUnit DMA input interrupt
#define BUS_AUD_INTSTAT_DMA_OUT           1 << 2 // audUnit DMA output interrupt

#define BUS_VID_INTSTAT_DIV 1 << 5 // Interrupt trigger in divUnit
#define BUS_VID_INTSTAT_GFX 1 << 4 // Interrupt trigger in gfxUnit
#define BUS_VID_INTSTAT_POT 1 << 3 // Interrupt trigger in potUnit
#define BUS_VID_INTSTAT_VID 1 << 2 // Interrupt trigger in vidUnit

#define BUS_DEV_INTSTAT_GPIO      1 << 7 // GPIO interrupt
#define BUS_DEV_INTSTAT_UART      1 << 6 // sucUnit UART interrupt (RS232)
#define BUS_DEV_INTSTAT_SMARTCARD 1 << 5 // sucUnit SmartCard interrupt
#define BUS_DEV_INTSTAT_PARALLEL  1 << 4 // devUnit Parallel interrupt
#define BUS_DEV_INTSTAT_IR_OUT    1 << 3 // devUnit IR output interrupt
#define BUS_DEV_INTSTAT_IR_IN     1 << 2 // devUnit IR input interrupt

#define BUS_RIO_INTSTAT_DEVICE3 1 << 5 // Device 3 interrupt
#define BUS_RIO_INTSTAT_DEVICE2 1 << 4 // Device 2 interrupt
#define BUS_RIO_INTSTAT_DEVICE1 1 << 3 // Device 1 interrupt (typically IDE controller)
#define BUS_RIO_INTSTAT_DEVICE0 1 << 2 // Device 0 interrupt (typically modem/ethernet)

#define BUS_TIM_INTSTAT_SYSTIMER    1 << 3 // System timer interrupt
#define BUS_TIM_INTSTAT_BUS_TIMEOUT 1 << 2 // Bus timeout interrupt

#define BUS_RESETCAUSE_SOFTWARE 1 << 2 // Software reset
#define BUS_RESETCAUSE_WATCHDOG 1 << 1 // Watchdog reset
#define BUS_RESETCAUSE_SWITCH   1 << 0 // Reset button pressed

#define SOLO1_NTSC_CLOCK 3.579575_MHz_XTAL

DEFINE_DEVICE_TYPE(SOLO1_ASIC, solo1_asic_device, "solo1_asic", "WebTV SOLO1 ASIC")

solo1_asic_device::solo1_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO1_ASIC, tag, owner, clock),
	device_serial_interface(mconfig, *this),
    m_hostcpu(*this, finder_base::DUMMY_TAG),
//    m_solovid(*this, finder_base::DUMMY_TAG),
    m_sys_timer(nullptr) // when it goes off, timer interrupt fires
//    m_watchdog_timer(nullptr)
{
}

void solo1_asic_device::regs_map(address_map &map)
{
    map(0x0000, 0x0fff).rw(FUNC(solo1_asic_device::reg_bus_r), FUNC(solo1_asic_device::reg_bus_w)); // busUnit
    //map(0x1000, 0x1fff).rw(FUNC(solo1_asic_device::reg_rio_r), FUNC(solo1_asic_device::reg_rio_w)); // rioUnit
    //map(0x2000, 0x2fff).rw(FUNC(solo1_asic_device::reg_aud_r), FUNC(solo1_asic_device::reg_aud_w)); // audUnit
//    map(0x3000, 0x3fff).rw(FUNC(solo1_asic_device::reg_vid_r), FUNC(solo1_asic_device::reg_vid_w)); // vidUnit
    map(0x4000, 0x4fff).rw(FUNC(solo1_asic_device::reg_dev_r), FUNC(solo1_asic_device::reg_dev_w)); // devUnit
    map(0x5000, 0x5fff).rw(FUNC(solo1_asic_device::reg_mem_r), FUNC(solo1_asic_device::reg_mem_w)); // memUnit
    //map(0x6000, 0x6fff).rw(FUNC(solo1_asic_device::reg_gfx_r), FUNC(solo1_asic_device::reg_gfx_w)); // gfxUnit
//    map(0x7000, 0x7fff).rw(FUNC(solo1_asic_device::reg_dve_r), FUNC(solo1_asic_device::reg_dve_w)); // dveUnit
    //map(0x8000, 0x8fff).rw(FUNC(solo1_asic_device::reg_div_r), FUNC(solo1_asic_device::reg_div_w)); // divUnit
//    map(0x9000, 0x9fff).rw(FUNC(solo1_asic_device::reg_pot_r), FUNC(solo1_asic_device::reg_pot_w)); // potUnit
    //map(0xa000, 0xafff).rw(FUNC(solo1_asic_device::reg_suc_r), FUNC(solo1_asic_device::reg_suc_w)); // sucUnit
    //map(0xb000, 0xbfff).rw(FUNC(solo1_asic_device::reg_mod_r), FUNC(solo1_asic_device::reg_mod_w)); // modUnit
}

void solo1_asic_device::set_aud_int_flag(uint32_t value)
{
    m_bus_int_status |= BUS_INTSTAT_AUDIO;
    m_bus_aud_int_status |= value;
}

void solo1_asic_device::set_vid_int_flag(uint32_t value)
{
    m_bus_int_status |= BUS_INTSTAT_VIDEO;
    m_bus_vid_int_status |= value;
}

void solo1_asic_device::set_rio_int_flag(uint32_t value)
{
    m_bus_int_status |= BUS_INTSTAT_RIO;
    m_bus_rio_int_status |= value;
}

void solo1_asic_device::device_start()
{
    m_sys_timer = timer_alloc(FUNC(solo1_asic_device::sys_timer_callback), this);
    m_bus_chip_id = 0x03120000; // SOLO1 chip ID - this should not change

    save_item(NAME(m_bus_chip_cntl));
    save_item(NAME(m_bus_int_status));
    save_item(NAME(m_bus_int_enable));
    save_item(NAME(m_bus_err_status));
    save_item(NAME(m_bus_err_enable));
    save_item(NAME(m_bus_err_address));
    save_item(NAME(m_bus_wd_reset_val));
    save_item(NAME(m_bus_lomem_rdprot_addr));
    save_item(NAME(m_bus_lomem_rdprot_mask));
    save_item(NAME(m_bus_lomem_wrprot_addr));
    save_item(NAME(m_bus_lomem_wrprot_mask));
    save_item(NAME(m_bus_tmr_compare));
    //save_item(NAME(m_compare_armed));
    save_item(NAME(m_bus_gpio_int_status));
    save_item(NAME(m_bus_gpio_int_enable));
    save_item(NAME(m_bus_gpio_int_polling));
    save_item(NAME(m_bus_aud_int_status));
    save_item(NAME(m_bus_aud_int_enable));
    save_item(NAME(m_bus_dev_int_status));
    save_item(NAME(m_bus_dev_int_enable));
    save_item(NAME(m_bus_vid_int_status));
    save_item(NAME(m_bus_vid_int_enable));
    save_item(NAME(m_bus_rio_int_status));
    save_item(NAME(m_bus_rio_int_enable));
    save_item(NAME(m_bus_rio_int_polling));
    save_item(NAME(m_bus_tim_int_status));
    save_item(NAME(m_bus_tim_int_enable));
    save_item(NAME(m_bus_reset_cause));
    save_item(NAME(m_bus_java1_fence_addr_l));
    save_item(NAME(m_bus_java1_fence_addr_h));
    save_item(NAME(m_bus_java2_fence_addr_l));
    save_item(NAME(m_bus_java2_fence_addr_h));
    save_item(NAME(m_bus_memsize));
    save_item(NAME(m_bus_fence_cntl));
    save_item(NAME(m_bus_bootmode));
    save_item(NAME(m_bus_use_bootmode));
}

void solo1_asic_device::device_reset()
{
    m_sys_timer->adjust(attotime::never); // disable

    m_bus_chip_cntl = 0;

    m_bus_int_status = 0;
    m_bus_int_enable = 0;

    m_bus_aud_int_enable = 0;
    m_bus_aud_int_status = 0;
    
    m_bus_vid_int_enable = 0;
    m_bus_vid_int_status = 0;
    
    m_bus_rio_int_enable = 0;
    m_bus_rio_int_status = 0;

    m_bus_tim_int_enable = 0;
    m_bus_tim_int_status = 0;

    m_bus_java1_fence_addr_l = 0;
    m_bus_java1_fence_addr_h = 0;
    m_bus_java2_fence_addr_l = 0;
    m_bus_java2_fence_addr_h = 0;

    m_bus_tmr_count = 0;

    m_bus_memsize = 0x04000000;

    m_bus_reset_cause = BUS_RESETCAUSE_SWITCH; // reset button hit
}

void solo1_asic_device::solo1_update_cycle_counting()
{
    m_bus_tmr_count = m_clock;
    if(m_compare_armed) {
        uint32_t delta = m_bus_tmr_compare - m_bus_tmr_count;
        m_sys_timer->adjust(clocks_to_attotime(delta));
    }
}

TIMER_CALLBACK_MEMBER(solo1_asic_device::sys_timer_callback)
{
    m_sys_timer->adjust(attotime::never);
    m_compare_armed = 0;
    m_bus_tmr_count = m_bus_tmr_compare;
    if((m_bus_int_enable & BUS_INTSTAT_TIMER)&&(m_bus_tim_int_enable & BUS_TIM_INTSTAT_SYSTIMER))
    {
        m_bus_int_status |= BUS_INTSTAT_TIMER;
        m_bus_tim_int_status |= BUS_TIM_INTSTAT_SYSTIMER;
        m_hostcpu->set_input_line(MIPS3_IRQ0, ASSERT_LINE);
    }
}

uint32_t solo1_asic_device::reg_bus_r(offs_t offset)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_READS, "busUnit: read %04x\n", offset * 4);
    switch(offset * 4)
    {
    case 0x000: // BUS_CHIPID (R/W)
        return m_bus_chip_id;
    case 0x004: // BUS_CHIPCNTL (R/W)
        return m_bus_chip_cntl;
    case 0x008: // BUS_INTSTAT (R/W)
        return m_bus_int_status & m_bus_int_enable; // TODO: is this correct behavior?
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
        m_bus_tmr_count = m_clock;
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
        return m_bus_gpio_int_status & m_bus_gpio_int_enable; // TODO: is this correct behavior?
    case 0x064: // BUS_GPINTPOL (R/W)
        return m_bus_gpio_int_polling;
    case 0x068: // BUS_AUDINTSTAT (W)
        break;
    case 0x168: // BUS_AUDINTSTAT (Clear)
        break;
    case 0x06c: // BUS_AUDINTSTAT (R/Set)
        return m_bus_aud_int_status & m_bus_aud_int_enable; // TODO: is this correct behavior?
    case 0x070: // BUS_AUDINTEN (R/Set)
        return m_bus_aud_int_enable;
    case 0x170: // BUS_AUDINTEN (Clear)
        break;
    case 0x074: // BUS_DEVINTSTAT (W)
        break;
    case 0x174: // BUS_DEVINTSTAT (Clear)
        break;
    case 0x078: // BUS_DEVINTSTAT (R/Set)
        return m_bus_dev_int_status & m_bus_dev_int_enable; // TODO: is this correct behavior?
    case 0x07c: // BUS_DEVINTEN (R/Set)
        return m_bus_dev_int_enable;
    case 0x17c: // BUS_DEVINTEN (Clear)
        break;
    case 0x080: // BUS_VIDINTSTAT (W)
        break;
    case 0x180: // BUS_VIDINTSTAT (Clear)
        break;
    case 0x084: // BUS_VIDINTSTAT (R/Set)
        return m_bus_vid_int_status & m_bus_vid_int_enable; // TODO: is this correct behavior?
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
        return m_bus_rio_int_status & m_bus_rio_int_enable; // TODO: is this correct behavior?
    case 0x098: // BUS_DEVINTEN (R/Set)
        return m_bus_rio_int_enable;
    case 0x198: // BUS_DEVINTEN (Clear)
        break;
    case 0x09c: // BUS_TIMINTSTAT (W)
        break;
    case 0x19c: // BUS_TIMINTSTAT (Clear)
        break;
    case 0x0a0: // BUS_TIMINTSTAT (R/Set)
        return m_bus_tim_int_status & m_bus_tim_int_enable; // TODO: is this correct behavior?
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
        logerror("Attempted read from reserved register 0%03x!\n", offset * 4);
        break;
    }
    return 0;
}

void solo1_asic_device::reg_bus_w(offs_t offset, uint32_t data)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_WRITES, "busUnit: write %08x to %04x\n", data, offset * 4);
    switch(offset * 4)
    {
    case 0x000: // BUS_CHIPID (R/W)
        logerror("Attempted write (%08x) to read-only register 0000 (BUS_CHIPID)\n", data); // Presumed behavior, there is no real need to write to this register
        break;
    case 0x004: // BUS_CHIPCNTL (R/W)
        m_bus_chip_cntl = data;
        break;
    case 0x008: // BUS_INTSTAT (R/W)
        m_bus_int_status = data;
        break;
    case 0x108: // BUS_INTSTAT (Clear)
        m_bus_int_status &= ~data; // TODO: is this correct behavior?
        break;
    case 0x00c: // BUS_INTEN (R/Set)
        m_bus_int_enable |= data; // TODO: is this correct behavior?
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
        solo1_update_cycle_counting();
        break;
    case 0x04c: // BUS_TCOMPARE (R/W)
        m_bus_tmr_compare = data;
        m_compare_armed = 1;
        solo1_update_cycle_counting();
        break;
    case 0x050: // BUS_INTSTAT (Set)
        m_bus_int_status |= data; // TODO: is this correct behavior?
        break;
    case 0x054: // BUS_ERRSTAT (R/Set)
        m_bus_err_status |= data; // TODO: is this correct behavior?
        break;
    case 0x058: // BUS_GPINTSTAT (W)
        m_bus_gpio_int_status = data;
        break;
    case 0x158: // BUS_GPINTSTAT (Clear)
        m_bus_gpio_int_status &= ~data; // TODO: is this correct behavior?
        break;
    case 0x05c: // BUS_GPINTEN (R/Set)
        m_bus_gpio_int_enable |= data; // TODO: is this correct behavior?
        break;
    case 0x15c: // BUS_GPINTEN (Clear)
        m_bus_gpio_int_enable &= ~data; // TODO: is this correct behavior?
        break;
    case 0x060: // BUS_GPINTSTAT (W)
        m_bus_gpio_int_status = data;
        break;
    case 0x064: // BUS_GPINTPOL (R/W)
        m_bus_gpio_int_polling = data;
        break;
    case 0x068: // BUS_AUDINTSTAT (W)
        m_bus_aud_int_status = data; // TODO: is this correct behavior? also mirror this over to audUnit (solo1_asic_aud)
        break;
    case 0x168: // BUS_AUDINTSTAT (Clear)
        m_bus_aud_int_status &= ~data; // TODO: is this correct behavior? also mirror this over to audUnit (solo1_asic_aud)
        break;
    case 0x06c: // BUS_AUDINTSTAT (R/Set)
        m_bus_aud_int_status |= data; // TODO: is this correct behavior? also mirror this over to audUnit (solo1_asic_aud)
        break;
    case 0x070: // BUS_AUDINTEN (R/Set)
        m_bus_aud_int_enable |= data; // TODO: is this correct behavior? also mirror this over to audUnit (solo1_asic_aud)
        break;
    case 0x170: // BUS_AUDINTEN (Clear)
        m_bus_aud_int_enable &= ~data; // TODO: is this correct behavior? also mirror this over to audUnit (solo1_asic_aud)
        break;
    case 0x074: // BUS_DEVINTSTAT (W)
        m_bus_dev_int_status = data; // TODO: is this correct behavior? also mirror this over to devUnit
        break;
    case 0x174: // BUS_DEVINTSTAT (Clear)
        m_bus_dev_int_status &= ~data; // TODO: is this correct behavior? also mirror this over to devUnit
        break;
    case 0x078: // BUS_DEVINTSTAT (R/Set)
        m_bus_dev_int_status |= data; // TODO: is this correct behavior? also mirror this over to devUnit
        break;
    case 0x07c: // BUS_DEVINTEN (R/Set)
        m_bus_dev_int_enable |= data; // TODO: is this correct behavior? also mirror this over to devUnit
        break;
    case 0x17c: // BUS_DEVINTEN (Clear)
        m_bus_dev_int_enable &= ~data; // TODO: is this correct behavior? also mirror this over to devUnit
        break;
    case 0x080: // BUS_VIDINTSTAT (W)
        m_bus_vid_int_status = data; // TODO: is this correct behavior? also mirror this over to vidUnit (solo1_asic_vid)
        break;
    case 0x180: // BUS_VIDINTSTAT (Clear)
        m_bus_vid_int_status &= ~data; // TODO: is this correct behavior? also mirror this over to vidUnit (solo1_asic_vid)
        break;
    case 0x084: // BUS_VIDINTSTAT (R/Set)
        m_bus_vid_int_status |= data; // TODO: is this correct behavior? also mirror this over to vidUnit (solo1_asic_vid)
        break;
    case 0x088: // BUS_VIDINTEN (R/Set)
        m_bus_vid_int_enable |= data; // TODO: is this correct behavior? also mirror this over to vidUnit (solo1_asic_vid)
        break;
    case 0x188: // BUS_VIDINTEN (Clear)
        m_bus_vid_int_enable &= ~data; // TODO: is this correct behavior? also mirror this over to vidUnit (solo1_asic_vid)
        break;
    case 0x08c: // BUS_RIOINTSTAT (W)
        m_bus_rio_int_status = data; // TODO: is this correct behavior? also mirror this over to rioUnit
        break;
    case 0x18c: // BUS_RIOINTSTAT (Clear)
        m_bus_rio_int_status &= ~data; // TODO: is this correct behavior? also mirror this over to rioUnit
        break;
    case 0x090: // BUS_RIOINTSTAT (R/Set)
        m_bus_rio_int_status |= data; // TODO: is this correct behavior? also mirror this over to rioUnit
        break;
    case 0x094: // BUS_RIOINTPOL (R/W)
        m_bus_rio_int_polling = data; // TODO: mirror this over to rioUnit
        break;
    case 0x098: // BUS_RIOINTEN (R/Set)
        m_bus_rio_int_enable |= data; // TODO: is this correct behavior? also mirror this over to rioUnit
        break;
    case 0x198: // BUS_RIOINTEN (Clear)
        m_bus_rio_int_enable &= ~data; // TODO: is this correct behavior? also mirror this over to rioUnit
        break;
    case 0x09c: // BUS_TIMINTSTAT (W)
        m_bus_tim_int_status = data;
        break;
    case 0x19c: // BUS_TIMINTSTAT (Clear)
        m_bus_tim_int_status &= ~data; // TODO: is this correct behavior?
        break;
    case 0x0a0: // BUS_TIMINTSTAT (R/Set)
        m_bus_tim_int_status |= data; // TODO: is this correct behavior?
        break;
    case 0x0a4: // BUS_TIMINTEN (R/Set)
        m_bus_tim_int_enable |= data; // TODO: is this correct behavior?
        break;
    case 0x1a4: // BUS_TIMINTEN (Clear)
        m_bus_tim_int_enable &= ~data; // TODO: is this correct behavior?
        break;
    case 0x0a8: // RESETCAUSE (R/Set)
        m_bus_reset_cause |= data; // TODO: is this correct behavior?
        break;
    case 0x0ac: // RESETCAUSE (Clear)
        m_bus_reset_cause &= ~data; // TODO: is this correct behavior?
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

uint32_t solo1_asic_device::reg_dev_r(offs_t offset)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_READS, "devUnit: read 4%03x\n", (offset+2)*4);
    switch((offset+2) * 4)
    {
    case 0x000: // DEV_IROLD (R/W)
        // TODO: Remove this case! This isn't used anymore!
        return m_dev_irold;
    case 0x004: // DEV_LED (R/W)
        // TODO: Remove this case! This isn't used anymore!
        return m_dev_led;
    case 0x008: // DEV_IDCNTL (R/W)
        return m_dev_id_chip_cntl;
    case 0x00c: // DEV_IICCNTL (R/W)
        return m_dev_iic_cntl;
    case 0x010: // DEV_GPIOIN (R/W)
        return m_dev_gpio_in;
    case 0x014: // DEV_GPIOOUT (R/Set)
        return m_dev_gpio_out;
    case 0x114: // DEV_GPIOOUT (Clear)
        break;
    case 0x018: // DEV_GPIOEN (R/Set)
        return m_dev_gpio_en;
    case 0x118: // DEV_GPIOEN (Clear)
        break;
    case 0x020: // DEV_IRIN_SAMPLE_INT (R/W)
        return m_dev_ir_in_sample_int;
    case 0x024: // DEV_IRIN_REJECT_INT (R/W)
        return m_dev_ir_in_reject_int;
    case 0x028: // DEV_IRIN_TRANS_DATA (R/W)
        return m_dev_ir_in_transition_data;
    case 0x02c: // DEV_IRIN_STATCNTL (R/W)
        return m_dev_ir_in_status_cntl;
    case 0x040: // DEV_IROUT_FIFO (R/W)
        return m_dev_ir_out_fifo;
    case 0x044: // DEV_IROUT_STATUS (R/W)
        return m_dev_ir_out_status;
    case 0x048: // DEV_IROUT_PERIOD (R/W)
        return m_dev_ir_out_period;
    case 0x04c: // DEV_IROUT_ON (R/W)
        return m_dev_ir_out_on;
    case 0x050: // DEV_IROUT_CURRENT_PERIOD (R/W)
        return m_dev_ir_out_current_period;
    case 0x054: // DEV_IROUT_CURRENT_ON (R/W)
        return m_dev_ir_out_current_on;
    case 0x058: // DEV_IROUT_CURRENT_COUNT (R/W)
        return m_dev_ir_out_current_count;
    case 0x200: // DEV_PPORT_DATA (R/W)
        return m_dev_parallel_data;
    case 0x204: // DEV_PPORT_CTRL (R/W)
        return m_dev_parallel_ctrl;
    case 0x208: // DEV_PPORT_STAT (R/W)
        return m_dev_parallel_status;
    case 0x20c: // DEV_PPORT_CNFG (R/W)
        return m_dev_parallel_cnfg;
    case 0x210: // DEV_PPORT_FIFOCTRL (R/W)
        return m_dev_parallel_fifo_ctrl;
    case 0x214: // DEV_PPORT_FIFOSTAT (R/W)
        return m_dev_parallel_fifo_status;
    case 0x218: // DEV_PPORT_TIMEOUT (R/W)
        return m_dev_parallel_timeout;
    case 0x21c: // DEV_PPORT_STAT2 (R/W)
        return m_dev_parallel_stat2;
    case 0x220: // DEV_PPORT_IEN (R/W)
        return m_dev_parallel_int_enable;
    case 0x224: // DEV_PPORT_IEN (R/W)
        return m_dev_parallel_int_status;
    case 0x228: // DEV_PPORT_CLRINT (R/W)
        return m_dev_parallel_clr_int;
    case 0x22c: // DEV_PPORT_ENABLE (R/W)
        return m_dev_parallel_enable;
    case 0x804: // DEV_DIAG (R/W)
        return m_dev_diag;
    case 0x808: // DEV_DEVDIAG (R/W)
        return m_dev_devdiag;
    default:
        logerror("Attempted read from reserved register 4%03x!\n", offset*4);
        break;
    }
    return 0;
}

uint32_t solo1_asic_device::reg_dev_irold_r(offs_t offset)
{
    return m_dev_irold;
}

void solo1_asic_device::reg_dev_irold_w(offs_t offset, uint32_t data)
{
    m_dev_irold = data;
}

void solo1_asic_device::reg_dev_w(offs_t offset, uint32_t data)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_WRITES, "devUnit: write %08x to 4%03x\n", data, (offset+2)*4);
    switch((offset+2)*4)
    {
    case 0x000: // DEV_IROLD (R/W)
        // TODO: Remove this case! This isn't used anymore!
        m_dev_irold = data;
        break;
    case 0x004: // DEV_LED (R/W)
        // TODO: Remove this case! This isn't used anymore!
        m_dev_led = data;
        popmessage("[DEBUG] LEDs: %d %d %d", (~data)&0x4/0x4, (~data)&0x2/0x2, (~data)&0x1);
        break;
    case 0x008: // DEV_IDCNTL (R/W)
        m_dev_id_chip_cntl = data;
        break;
    case 0x00c: // DEV_IICCNTL (R/W)
        // TODO: Implement the I2C bus
        m_dev_iic_cntl = data;
        break;
    case 0x010: // DEV_GPIOIN (R/W)
        m_dev_gpio_in = data;
        break;
    case 0x014: // DEV_GPIOOUT (R/Set)
        m_dev_gpio_out |= data;
        break;
    case 0x114: // DEV_GPIOOUT (Clear)
        m_dev_gpio_out &= ~data;
        break;
    case 0x018: // DEV_GPIOEN (R/Set)
        m_dev_gpio_en |= data;
        break;
    case 0x118: // DEV_GPIOEN (Clear)
        m_dev_gpio_en &= ~data;
        break;
    case 0x020: // DEV_IRIN_SAMPLE_INT (R/W)
        m_dev_ir_in_sample_int = data;
        break;
    case 0x024: // DEV_IRIN_REJECT_INT (R/W)
        m_dev_ir_in_reject_int = data;
        break;
    case 0x028: // DEV_IRIN_TRANS_DATA (R/W)
        m_dev_ir_in_transition_data = data;
        break;
    case 0x02c: // DEV_IRIN_STATCNTL (R/W)
        m_dev_ir_in_status_cntl = data;
        break;
    case 0x040: // DEV_IROUT_FIFO (R/W)
        m_dev_ir_out_fifo = data;
        break;
    case 0x044: // DEV_IROUT_STATUS (R/W)
        m_dev_ir_out_status = data;
        break;
    case 0x048: // DEV_IROUT_PERIOD (R/W)
        m_dev_ir_out_period = data;
        break;
    case 0x04c: // DEV_IROUT_ON (R/W)
        m_dev_ir_out_on = data;
        break;
    case 0x050: // DEV_IROUT_CURRENT_PERIOD (R/W)
        m_dev_ir_out_current_period = data;
        break;
    case 0x054: // DEV_IROUT_CURRENT_ON (R/W)
        m_dev_ir_out_current_on = data;
        break;
    case 0x058: // DEV_IROUT_CURRENT_COUNT (R/W)
        m_dev_ir_out_current_count = data;
        break;
    case 0x200: // DEV_PPORT_DATA (R/W)
        m_dev_parallel_data = data;
        break;
    case 0x204: // DEV_PPORT_CTRL (R/W)
        m_dev_parallel_ctrl = data;
        break;
    case 0x208: // DEV_PPORT_STAT (R/W)
        m_dev_parallel_status = data;
        break;
    case 0x20c: // DEV_PPORT_CNFG (R/W)
        m_dev_parallel_cnfg = data;
        break;
    case 0x210: // DEV_PPORT_FIFOCTRL (R/W)
        m_dev_parallel_fifo_ctrl = data;
        break;
    case 0x214: // DEV_PPORT_FIFOSTAT (R/W)
        m_dev_parallel_fifo_status = data;
        break;
    case 0x218: // DEV_PPORT_TIMEOUT (R/W)
        m_dev_parallel_timeout = data;
        break;
    case 0x21c: // DEV_PPORT_STAT2 (R/W)
        m_dev_parallel_stat2 = data;
        break;
    case 0x220: // DEV_PPORT_IEN (R/W)
        m_dev_parallel_int_enable = data;
        break;
    case 0x224: // DEV_PPORT_IST (R/W)
        m_dev_parallel_int_status = data;
        break;
    case 0x228: // DEV_PPORT_CLRINT (R/W)
        m_dev_parallel_clr_int = data;
        break;
    case 0x22c: // DEV_PPORT_ENABLE (R/W)
        m_dev_parallel_enable = data;
        break;
    case 0x804: // DEV_DIAG (R/W)
        m_dev_diag = data;
        break;
    case 0x808: // DEV_DEVDIAG (R/W)
        m_dev_devdiag = data;
        break;
    default:
        logerror("Attempted write (%08x) to reserved register 4%03x!\n", data, offset*4);
        break;
    }
}

uint32_t solo1_asic_device::reg_mem_r(offs_t offset)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_READS, "memUnit: read 5%03x\n", offset*4);
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

void solo1_asic_device::reg_mem_w(offs_t offset, uint32_t data)
{
    // TODO: split this out into multiple handlers! using a giant switch statement for this is just ugly
    LOGMASKED(LOG_WRITES, "memUnit: write %08x to 5%03x", data, offset*4);
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

/*uint32_t solo1_asic_device::reg_dve_r(offs_t offset)
{
    return m_solovid->reg_dve_r(offset);
}

void solo1_asic_device::reg_dve_w(offs_t offset, uint32_t data)
{
    m_solovid->reg_dve_w(offset, data);
}

uint32_t solo1_asic_device::reg_pot_r(offs_t offset)
{
    return m_solovid->reg_pot_r(offset);
}

void solo1_asic_device::reg_pot_w(offs_t offset, uint32_t data)
{
    m_solovid->reg_pot_w(offset, data);
}

uint32_t solo1_asic_device::reg_vid_r(offs_t offset)
{
    return m_solovid->reg_vid_r(offset);
}

void solo1_asic_device::reg_vid_w(offs_t offset, uint32_t data)
{
    m_solovid->reg_vid_w(offset, data);
}*/