// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#ifndef MAME_WEBTV_SOLO_ASIC_H
#define MAME_WEBTV_SOLO_ASIC_H

#pragma once

#include "diserial.h"
#include "bus/rs232/rs232.h"
#include "cpu/mips/mips3.h"
#include "solo_asic_video.h"
#include "solo_asic_audio.h"
#include "wtvir.h"
#include "wtvdbg.h"
#include "wtvsoftmodem.h"
#include "machine/ds2401.h"
#include "machine/i2cmem.h"
#include "machine/ins8250.h"
#include "bus/ata/ataintf.h"
#include "machine/pc_lpt.h"
#include "machine/watchdog.h"

constexpr uint32_t SYSCONFIG_PAL = 1 << 3;  // use PAL mode

constexpr uint32_t CHPCNTL_WDENAB_MASK     = 0x03 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ0     = 0x00 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ1     = 0x01 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ2     = 0x02 << 30;
constexpr uint32_t CHPCNTL_WDENAB_SEQ3     = 0x03 << 30;
constexpr uint32_t CHPCNTL_AUDCLKDIV_SHIFT = 26;
constexpr uint32_t CHPCNTL_AUDCLKDIV_MASK  = 0x0f << CHPCNTL_AUDCLKDIV_SHIFT;

constexpr uint32_t ERR_LOWWRITE = 1 << 7; // low memory write fence error
constexpr uint32_t ERR_F1READ   = 1 << 6; // BUS_FENADDR1 read fence check error
constexpr uint32_t ERR_F1WRITE  = 1 << 5; // BUS_FENADDR1 write fence check error
constexpr uint32_t ERR_F2READ   = 1 << 4; // BUS_FENADDR2 read fence check error
constexpr uint32_t ERR_F2WRITE  = 1 << 3; // BUS_FENADDR2 write fence check error
constexpr uint32_t ERR_TIMEOUT  = 1 << 2; // io timeout error
constexpr uint32_t ERR_OW       = 1 << 0; // double-fault

constexpr uint32_t RESETCAUSE_SOFTWARE = 1 << 2;
constexpr uint32_t RESETCAUSE_WATCHDOG = 1 << 1;
constexpr uint32_t RESETCAUSE_SWITCH   = 1 << 0;

constexpr uint32_t BOOTMODE_BIG_ENDIAN = 1 << 8;

constexpr uint32_t WATCHDOG_TIMER_USEC = 1000000;
constexpr uint16_t TCOMPARE_TIMER_USEC = 50000;

constexpr uint32_t BUS_INT_VIDEO = 1 << 7; // vidUnit, gfxUnit, divUnit, or potUnit interrupt
constexpr uint32_t BUS_INT_AUDIO = 1 << 6; // Soft mode, divUnit and audio in/out interrupt
constexpr uint32_t BUS_INT_RIO   = 1 << 5; // modem IRQ
constexpr uint32_t BUS_INT_DEV   = 1 << 4; // IR data ready to read
constexpr uint32_t BUS_INT_TIMER = 1 << 3; // Timer interrupt (TCOUNT == TCOMPARE)
constexpr uint32_t BUS_INT_FENCE = 1 << 2; // Fence error

constexpr uint32_t BUS_INT_DEV_UNKNOWN   = 1 << 9; // Unknown but UltimateTV builds check this.
constexpr uint32_t BUS_INT_DEV_DMA       = 1 << 8; // Seems to be used for disk IO on the UltimateTV
constexpr uint32_t BUS_INT_DEV_GPIO      = 1 << 7;
constexpr uint32_t BUS_INT_DEV_UART      = 1 << 6;
constexpr uint32_t BUS_INT_DEV_SMARTCARD = 1 << 5;
constexpr uint32_t BUS_INT_DEV_PARPORT   = 1 << 4;
constexpr uint32_t BUS_INT_DEV_IROUT     = 1 << 3;
constexpr uint32_t BUS_INT_DEV_IRIN      = 1 << 2;

constexpr uint32_t BUS_INT_RIO_DEVICE3 = 1 << 5;
constexpr uint32_t BUS_INT_RIO_DEVICE2 = 1 << 4;
constexpr uint32_t BUS_INT_RIO_DEVICE1 = 1 << 3;
constexpr uint32_t BUS_INT_RIO_DEVICE0 = 1 << 2;

constexpr uint32_t BUS_INT_TIM_SYSTIMER = 1 << 3;
constexpr uint32_t BUS_INT_TIM_BUSTOUT  = 1 << 2;

constexpr uint32_t PPORT_INT_CFIFO_ERROR = 1 << 4;
constexpr uint32_t PPORT_INT_ACK_DONE    = 1 << 3;
constexpr uint32_t PPORT_INT_ECP_REVERSE = 1 << 2;
constexpr uint32_t PPORT_INT_THRESH      = 1 << 0;

constexpr uint32_t UTV_DMAMODE_READ    = 1 << 0;
constexpr uint32_t UTV_DMACNTL_READY   = 1 << 2;

constexpr uint32_t NVCNTL_SCL      = 1 << 3;
constexpr uint32_t NVCNTL_WRITE_EN = 1 << 2;
constexpr uint32_t NVCNTL_SDA_W    = 1 << 1;
constexpr uint32_t NVCNTL_SDA_R    = 1 << 0;

// Used to set or clear the hook state on boxes before the UTV
constexpr uint32_t GPIO_SOFTMODEM_HOOK_STATE       = 1 << 10;
// Used in the UTV, not sure if names are correct. These names are based on reverse-engineered behaviour.
// UTV seems to start with setting the modem off hook, then checking if their a line voltage to know if the line is ready.
constexpr uint32_t GPIO_SOFTMODEM_RESET            = 1 <<  5; // Called before the modem is opened and after the modem is closed.
constexpr uint32_t GPIO_SOFTMODEM_HAS_LINE_VOLTAGE = 1 <<  6; // GPIO IRQ indicating line voltage is available.
constexpr uint32_t GPIO_SOFTMODEM_LINE_CHECK       = 1 << 14; // Called every so often, looks like it requests the modem to IRQ if there's line voltage.

constexpr uint8_t  INS8250_LSR_TSRE = 0x40;
constexpr uint8_t  INS8250_LSR_THRE = 0x20;
constexpr uint16_t MBUFF_MAX_SIZE   = 0x800;
constexpr uint16_t MBUFF_FLUSH_TIME = 100;  // time is in microseconds

constexpr uint8_t SSID_STATE_IDLE               = 0x0;
constexpr uint8_t SSID_STATE_RESET              = 0x1;
constexpr uint8_t SSID_STATE_PRESENCE           = 0x2;
constexpr uint8_t SSID_STATE_COMMAND            = 0x3;
constexpr uint8_t SSID_STATE_READROM            = 0x4;
constexpr uint8_t SSID_STATE_READROM_PULSESTART = 0x5;
constexpr uint8_t SSID_STATE_READROM_PULSEEND   = 0x6;
constexpr uint8_t SSID_STATE_READROM_BIT        = 0x7;

constexpr uint8_t MODFW_NULL_RESULT             = 0x00000000;
constexpr uint8_t MODFW_RBR_ACK                 = 0x2e;
constexpr uint8_t MODFW_LSR_READY               = 0x21;
constexpr uint8_t MODFW_MSG_IDX_FLUSH0          = 0x2;
constexpr uint8_t MODFW_MSG_IDX_FLUSH1          = 0x1d;

constexpr uint8_t modfw_message[] = "\x0a\x0a.""Download Modem Firmware ................""\x0d\x0a""Modem Firmware Successfully Loaded""\x0d\x0a";
constexpr uint8_t modfw_enable_string[] = "AT**\x0d";
constexpr uint8_t modfw_reset_string[] = "ATZ\x0d";

class solo_asic_device : public device_t, public device_serial_interface
{

public:

	solo_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, uint32_t chip_id = 0, uint32_t sys_config = 0, uint32_t aud_clock = 44100, bool softmodem_enabled = false);

	void map(address_map &map);
	void bus_unit_map(address_map &map);
	void rom_unit_map(address_map &map);
	void dev_unit_map(address_map &map);
	void mem_unit_map(address_map &map);
	void suc_unit_map(address_map &map);
	void mod_unit_map(address_map &map);
	void dma_unit_map(address_map &map);

	void hardware_modem_map(address_map &map);
	void ide_map(address_map &map);

	template <typename T> void set_serial_id(T &&tag) { m_serial_id.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_ata(T &&tag) { m_ata.set_tag(std::forward<T>(tag)); }

	auto reset_hack_callback() { return m_reset_hack_cb.bind(); }
	auto sda_r_callback() { return m_iic_sda_in_cb.bind(); }
	auto sda_w_callback() { return m_iic_sda_out_cb.bind(); }

	void irq_ide1_w(int state);
	void irq_ide2_w(int state);
	void dmarq_ide1_w(int state);

	uint8_t sda_r();
	void sda_w(uint8_t state);
	uint8_t scl_r();
	void scl_w(uint8_t state);

protected:

	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

	uint32_t m_chip_id = 0x03120000;
	uint32_t m_sys_config = 0x00000000;
	
	uint32_t m_chpcntl;

	uint8_t m_wdenable;
	uint8_t m_wdvalue;

	uint32_t m_fence1_addr;
	uint32_t m_fence1_mask;
	uint32_t m_fence2_addr;
	uint32_t m_fence2_mask;
	uint32_t m_tcompare;
	uint32_t m_resetcause;
	uint32_t m_j1fenladdr;
	uint32_t m_j1fenhaddr;
	uint32_t m_j2fenladdr;
	uint32_t m_j2fenhaddr;
	uint32_t m_bootmode;
	uint32_t m_use_bootmode;

	uint32_t m_bus_intenable;
	uint32_t m_bus_intstat;
	uint32_t m_busgpio_intenable;
	uint32_t m_busgpio_intstat;
	uint32_t m_busgpio_intpol;
	uint32_t m_busaud_intenable;
	uint32_t m_busaud_intstat;
	uint32_t m_busdev_intenable;
	uint32_t m_busdev_intstat;
	uint32_t m_busrio_intenable;
	uint32_t m_busrio_intstat;
	uint32_t m_bustim_intenable;
	uint32_t m_bustim_intstat;

	uint8_t m_errenable;
	uint8_t m_errstat;

	uint32_t m_memcntl;
	uint32_t m_memrefcnt;
	uint32_t m_memdata;
	uint32_t m_memcmd;
	uint32_t m_memtiming;

	uint8_t m_iiccntl;
	uint8_t m_iic_sda;
	uint8_t m_iic_scl;

	uint32_t m_ledstate;

	uint8_t m_fcntl;

	uint32_t m_rom_cntl0;
	uint32_t m_rom_cntl1;

	uint32_t m_aud_clock;

	uint32_t m_dev_idcntl;
	uint8_t m_dev_id_state;
	uint8_t m_dev_id_bit;
	uint8_t m_dev_id_bitidx;
	uint32_t m_dev_gpio_in;
	uint32_t m_dev_gpio_out;
	uint32_t m_dev_gpio_in_mask;
	uint32_t m_dev_gpio_out_mask;
	bool m_dev_pport_enabled;
	uint32_t m_dev_pport_intenable;
	uint32_t m_dev_pport_intstat;

	uint16_t m_smrtcrd_serial_bitmask = 0x0;
	uint16_t m_smrtcrd_serial_rxdata = 0x0;

	// The names for these are guesses from reading the dissasembly.
	uint32_t m_utvdma_src;       // Solo register 0x0400c020
	uint32_t m_utvdma_dst;       // Solo register 0x0400c024
	uint32_t m_utvdma_size;      // Solo register 0x0400c028
	uint32_t m_utvdma_mode;      // Solo register 0x0400c02c
	uint32_t m_utvdma_cntl;      // Solo register 0x0400c040
	uint32_t m_utvdma_locked;    // Solo register 0x0400c100
	uint32_t m_utvdma_csrc;
	uint32_t m_utvdma_cdst;
	uint32_t m_utvdma_csize;
	uint32_t m_utvdma_cmode;
	uint32_t m_utvdma_started;
	uint32_t m_utvdma_ccnt;
	uint32_t m_ide1_dmarq_state;

	bool m_softmodem_enabled;
	bool m_hardmodem_enabled;
	uint8_t modem_txbuff[MBUFF_MAX_SIZE];
	uint32_t modem_txbuff_size;
	uint32_t modem_txbuff_index;
	bool modem_should_threint;
	bool modfw_mode;
	uint32_t modfw_message_index;
	bool modfw_will_flush;
	bool modfw_will_ack;
	uint32_t modfw_enable_index;
	uint32_t modfw_reset_index;
	bool do7e_hack;

private:

	required_device<mips3_device> m_hostcpu;
	required_shared_ptr<uint32_t> m_hostram;
	required_device<solo_asic_video_device> m_video;
	required_device<solo_asic_audio_device> m_audio;
	required_device<ds2401_device> m_serial_id;
	required_device<wtvir_sejin_device> m_irkbdc;
	optional_device<ns16550_device> m_modem_uart;
	optional_device<wtvsoftmodem_device> m_softmodem_uart;
	required_device<wtvdbg_rs232_device> m_debug_uart;
	required_device<pc_lpt_device> m_pport;
	required_device<watchdog_timer_device> m_watchdog;

	output_finder<> m_power_led;
	output_finder<> m_connect_led;
	output_finder<> m_message_led;

	optional_device<ata_interface_device> m_ata;

	devcb_write_line m_reset_hack_cb;
	devcb_read8 m_iic_sda_in_cb;
	devcb_write8 m_iic_sda_out_cb;


	emu_timer *modem_buffer_timer = nullptr;
	TIMER_CALLBACK_MEMBER(flush_modem_buffer);

	emu_timer *compare_timer = nullptr;
	TIMER_CALLBACK_MEMBER(timer_irq);

	void int_enable_vid_w(int state);
	void irq_vid_w(int state);
	void int_enable_aud_w(int state);
	void irq_aud_w(int state);
	void irq_modem_w(int state);
	void irq_uart_w(int state);
	void irq_pport_w(int state);
	void dmarq_dmaread(uint32_t* dmarq_state, uint32_t ide_device_base, uint32_t buf_start, uint32_t buf_size);
	void dmarq_dmawrite(uint32_t* dmarq_state, uint32_t ide_device_base, uint32_t buf_start, uint32_t buf_size);
	void irq_keyboard_w(int state);
	void set_gpio_irq(uint32_t mask, int state);
	void set_pport_irq(uint32_t mask, int state);
	void set_dev_irq(uint32_t mask, int state);
	void set_rio_irq(uint32_t mask, int state);
	void set_timer_irq(uint32_t mask, int state);
	void set_bus_irq(uint32_t mask, int state);

	void validate_active_area();
	void watchdog_enable(int state);
	bool is_webtvos();
	void modfw_hack_begin();
	void modfw_hack_end();
	void mod_reset();

	/* busUnit registers */

	uint32_t reg_0000_r();          // BUS_CHIPID (read-only)
	uint32_t reg_0004_r();          // BUS_CHPCNTL (read)
	void reg_0004_w(uint32_t data); // BUS_CHPCNTL (write)
	uint32_t reg_0008_r();          // BUS_INTSTAT (read)
	void reg_0008_w(uint32_t data); // BUS_INTSTAT_S (write)
	uint32_t reg_0108_r();          // BUS_INTSTAT (read)
	void reg_0108_w(uint32_t data); // BUS_INTSTAT_C (write)
	uint32_t reg_0050_r();          // BUS_INTSTATRAW (read)
	uint32_t reg_000c_r();          // BUS_INTEN (read)
	void reg_000c_w(uint32_t data); // BUS_INTEN_S (write)
	uint32_t reg_010c_r();          // BUS_INTEN (read)
	void reg_010c_w(uint32_t data); // BUS_INTEN_C (write)
	uint32_t reg_0010_r();          // BUS_ERRSTAT (read)
	void reg_0010_w(uint32_t data); // BUS_ERRSTAT_S (write)
	uint32_t reg_0110_r();          // BUS_ERRSTAT (read)
	void reg_0110_w(uint32_t data); // BUS_ERRSTAT_C (write)
	uint32_t reg_0014_r();          // BUS_ERREN_S (read)
	void reg_0014_w(uint32_t data); // BUS_ERREN_S (write)
	uint32_t reg_0114_r();          // BUS_ERREN_C (read)
	void reg_0114_w(uint32_t data); // BUS_ERREN_C (clear)
	uint32_t reg_0018_r();          // BUS_ERRADDR (read-only)
	uint32_t reg_0030_r();          // BUS_WDVALUE (read)
	void reg_0030_w(uint32_t data); // BUS_WDVALUE (write)
	void reg_0118_w(uint32_t data); // BUS_WDREG_C (clear)
	uint32_t reg_001c_r();          // BUS_FENADDR1 (read)
	void reg_001c_w(uint32_t data); // BUS_FENADDR1 (write)
	uint32_t reg_0020_r();          // BUS_FENMASK1 (read)
	void reg_0020_w(uint32_t data); // BUS_FENMASK1 (write)
	uint32_t reg_0024_r();          // BUS_FENADDR1 (read)
	void reg_0024_w(uint32_t data); // BUS_FENADDR1 (write)
	uint32_t reg_0028_r();          // BUS_FENMASK2 (read)
	void reg_0028_w(uint32_t data); // BUS_FENMASK2 (write)
	uint32_t reg_0048_r();          // BUS_TCOUNT (read)
	void reg_0048_w(uint32_t data); // BUS_TCOUNT (write)
	uint32_t reg_004c_r();          // BUS_TCOMPARE (read)
	void reg_004c_w(uint32_t data); // BUS_TCOMPARE (write)
	uint32_t reg_005c_r();          // BUS_GPINTEN_S (read)
	void reg_005c_w(uint32_t data); // BUS_GPINTEN_S (write)
	uint32_t reg_015c_r();          // BUS_GPINTEN_C (read)
	void reg_015c_w(uint32_t data); // BUS_GPINTEN_C (write)
	uint32_t reg_0058_r();          // BUS_GPINTSTAT (read)
	uint32_t reg_0060_r();          // BUS_GPINTSTAT_S (read)
	void reg_0060_w(uint32_t data); // BUS_GPINTSTAT_S (write)
	uint32_t reg_0158_r();          // BUS_GPINTSTAT_C (read)
	void reg_0158_w(uint32_t data); // BUS_GPINTSTAT_C (write)
	uint32_t reg_0064_r();          // BUS_GPINTPOL (read)
	void reg_0064_w(uint32_t data); // BUS_GPINTPOL (write)
	uint32_t reg_0070_r();          // BUS_AUDINTEN_S (read)
	void reg_0070_w(uint32_t data); // BUS_AUDINTEN_S (write)
	uint32_t reg_0170_r();          // BUS_AUDINTEN_C (read)
	void reg_0170_w(uint32_t data); // BUS_AUDINTEN_C (write)
	uint32_t reg_0068_r();          // BUS_AUDINTSTAT (read)
	uint32_t reg_006c_r();          // BUS_AUDINTSTAT_S (read)
	void reg_006c_w(uint32_t data); // BUS_AUDINTSTAT_S (write)
	uint32_t reg_0168_r();          // BUS_AUDINTSTAT_C (read)
	void reg_0168_w(uint32_t data); // BUS_AUDINTSTAT_C (write)
	uint32_t reg_007c_r();          // BUS_DEVINTEN_S (read)
	void reg_007c_w(uint32_t data); // BUS_DEVINTEN_S (write)
	uint32_t reg_017c_r();          // BUS_DEVINTEN_C (read)
	void reg_017c_w(uint32_t data); // BUS_DEVINTEN_C (write)
	uint32_t reg_0074_r();          // BUS_DEVINTSTAT (read)
	uint32_t reg_0078_r();          // BUS_DEVINTSTAT_S (read)
	void reg_0078_w(uint32_t data); // BUS_DEVINTSTAT_S (write)
	uint32_t reg_0174_r();          // BUS_DEVINTSTAT_C (read)
	void reg_0174_w(uint32_t data); // BUS_DEVINTSTAT_C (write)
	uint32_t reg_0088_r();          // BUS_VIDINTEN_S (read)
	void reg_0088_w(uint32_t data); // BUS_VIDINTEN_S (write)
	uint32_t reg_0188_r();          // BUS_VIDINTEN_C (read)
	void reg_0188_w(uint32_t data); // BUS_VIDINTEN_C (write)
	uint32_t reg_0080_r();          // BUS_VIDINTSTAT (read)
	uint32_t reg_0084_r();          // BUS_VIDINTSTAT_S (read)
	void reg_0084_w(uint32_t data); // BUS_VIDINTSTAT_S (write)
	uint32_t reg_0180_r();          // BUS_VIDINTSTAT_C (read)
	void reg_0180_w(uint32_t data); // BUS_VIDINTSTAT_C (write)
	uint32_t reg_0098_r();          // BUS_RIOINTEN_S (read)
	void reg_0098_w(uint32_t data); // BUS_RIOINTEN_S (write)
	uint32_t reg_0198_r();          // BUS_RIOINTEN_C (read)
	void reg_0198_w(uint32_t data); // BUS_RIOINTEN_C (write)
	uint32_t reg_008c_r();          // BUS_RIOINTSTAT (read)
	uint32_t reg_0090_r();          // BUS_RIOINTSTAT_S (read)
	void reg_0090_w(uint32_t data); // BUS_RIOINTSTAT_S (write)
	uint32_t reg_018c_r();          // BUS_RIOINTSTAT_C (read)
	void reg_018c_w(uint32_t data); // BUS_RIOINTSTAT_C (write)
	uint32_t reg_00a4_r();          // BUS_TIMINTEN_S (read)
	void reg_00a4_w(uint32_t data); // BUS_TIMINTEN_S (write)
	uint32_t reg_01a4_r();          // BUS_TIMINTEN_C (read)
	void reg_01a4_w(uint32_t data); // BUS_TIMINTEN_C (write)
	uint32_t reg_009c_r();          // BUS_TIMINTSTAT (read)
	uint32_t reg_00a0_r();          // BUS_TIMINTSTAT_S (read)
	void reg_00a0_w(uint32_t data); // BUS_TIMINTSTAT_S (write)
	uint32_t reg_019c_r();          // BUS_TIMINTSTAT_C (read)
	void reg_019c_w(uint32_t data); // BUS_TIMINTSTAT_C (write)
	uint32_t reg_00a8_r();          // BUS_RESETCAUSE (read)
	void reg_00a8_w(uint32_t data); // BUS_RESETCAUSE (write)
	void reg_00ac_w(uint32_t data); // BUS_RESETCAUSE_C (write)
	uint32_t reg_00b0_r();          // BUS_J1FENLADDR (read)
	void reg_00b0_w(uint32_t data); // BUS_J1FENLADDR (write)
	uint32_t reg_00b4_r();          // BUS_J1FENHADDR (read)
	void reg_00b4_w(uint32_t data); // BUS_J1FENHADDR (write)
	uint32_t reg_00b8_r();          // BUS_J2FENLADDR (read)
	void reg_00b8_w(uint32_t data); // BUS_J2FENLADDR (write)
	uint32_t reg_00bc_r();          // BUS_J2FENHADDR (read)
	void reg_00bc_w(uint32_t data); // BUS_J2FENHADDR (write)
	uint32_t reg_00c8_r();          // BUS_BOOTMODE (read)
	void reg_00c8_w(uint32_t data); // BUS_BOOTMODE (write)
	uint32_t reg_00cc_r();          // BUS_USEBOOTMODE (read)
	void reg_00cc_w(uint32_t data); // BUS_USEBOOTMODE (write)

	/* romUnit registers */

	uint32_t reg_1000_r();          // ROM_SYSCONF (read-only)
	uint32_t reg_1004_r();          // ROM_CNTL0 (read)
	void reg_1004_w(uint32_t data); // ROM_CNTL0 (write)
	uint32_t reg_1008_r();          // ROM_CNTL1 (read)
	void reg_1008_w(uint32_t data); // ROM_CNTL1 (write)

	/* devUnit registers */

	uint32_t reg_4000_r();          // DEV_IROLD (read-only)
	uint32_t reg_4004_r();          // DEV_LED (read)
	void reg_4004_w(uint32_t data); // DEV_LED (write)
	uint32_t reg_4008_r();          // DEV_IDCNTL (read)
	void reg_4008_w(uint32_t data); // DEV_IDCNTL (write)
	uint32_t reg_400c_r();          // DEV_NVCNTL (read)
	void reg_400c_w(uint32_t data); // DEV_NVCNTL (write)

	uint32_t reg_4010_r();          // DEV_GPIOIN (read)
	void reg_4010_w(uint32_t data); // DEV_GPIOIN (write)
	uint32_t reg_4014_r();          // DEV_GPIOOUT_S (read)
	void reg_4014_w(uint32_t data); // DEV_GPIOOUT_S (write)
	uint32_t reg_4114_r();          // DEV_GPIOOUT_C (read)
	void reg_4114_w(uint32_t data); // DEV_GPIOOUT_C (write)
	uint32_t reg_4018_r();          // DEV_GPIOEN_S (read)
	void reg_4018_w(uint32_t data); // DEV_GPIOEN_S (write)
	uint32_t reg_4118_r();          // DEV_GPIOEN_C (read)
	void reg_4118_w(uint32_t data); // DEV_GPIOEN_C (write)

	uint32_t reg_4020_r();          // DEV_IRIN_SAMPLE (read)
	void reg_4020_w(uint32_t data); // DEV_IRIN_SAMPLE (write)
	uint32_t reg_4024_r();          // DEV_IRIN_REJECT_INT (read)
	void reg_4024_w(uint32_t data); // DEV_IRIN_REJECT_INT (write)
	uint32_t reg_4028_r();          // DEV_IRIN_TRANS_DATA (read)
	uint32_t reg_402c_r();          // DEV_IRIN_STATCNTL (read)
	void reg_402c_w(uint32_t data); // DEV_IRIN_STATCNTL (write)
	// The boot ROM seems to write to register 4018, which is not mentioned anywhere in the documentation.
	uint32_t reg_4200_r();          // DEV_PPORT_DATA (read)
	void reg_4200_w(uint32_t data); // DEV_PPORT_DATA (write)
	uint32_t reg_4204_r();          // DEV_PPORT_CTRL (read)
	void reg_4204_w(uint32_t data); // DEV_PPORT_CTRL (write)
	uint32_t reg_4208_r();          // DEV_PPORT_STAT (read)
	uint32_t reg_420c_r();          // DEV_PPORT_CNFG (read)
	void reg_420c_w(uint32_t data); // DEV_PPORT_CNFG (write)
	uint32_t reg_4210_r();          // DEV_PPORT_FIFOCTRL (read)
	void reg_4210_w(uint32_t data); // DEV_PPORT_FIFOCTRL (write)
	uint32_t reg_4214_r();          // DEV_PPORT_FIFOSTAT (read)
	void reg_4214_w(uint32_t data); // DEV_PPORT_FIFOSTAT (write)
	uint32_t reg_4218_r();          // DEV_PPORT_TIMEOUT (read)
	void reg_4218_w(uint32_t data); // DEV_PPORT_TIMEOUT (write)
	uint32_t reg_421c_r();          // DEV_PPORT_STAT2 (read)
	void reg_421c_w(uint32_t data); // DEV_PPORT_STAT2 (write)
	uint32_t reg_4220_r();          // DEV_PPORT_IEN (read)
	void reg_4220_w(uint32_t data); // DEV_PPORT_IEN (write)
	uint32_t reg_4224_r();          // DEV_PPORT_IST (read)
	void reg_4224_w(uint32_t data); // DEV_PPORT_IST (write)
	uint32_t reg_4228_r();          // DEV_PPORT_CLRINT (read)
	void reg_4228_w(uint32_t data); // DEV_PPORT_CLRINT (write)
	uint32_t reg_422c_r();          // DEV_PPORT_ENABLE (read)
	void reg_422c_w(uint32_t data); // DEV_PPORT_ENABLE (write)

	/* memUnit registers */

	uint32_t reg_5000_r();          // MEM_CNTL (read)
	void reg_5000_w(uint32_t data); // MEM_CNTL (write)
	uint32_t reg_5004_r();          // MEM_REFCNT (read)
	void reg_5004_w(uint32_t data); // MEM_REFCNT (write)
	uint32_t reg_5008_r();          // MEM_DATA (read)
	void reg_5008_w(uint32_t data); // MEM_DATA (write)
	uint32_t reg_500c_r();          // MEM_CMD (read)
	void reg_500c_w(uint32_t data); // MEM_CMD (write-only)
	uint32_t reg_5010_r();          // MEM_TIMING (read)
	void reg_5010_w(uint32_t data); // MEM_TIMING (write)

	/* sucUnit registers */

	void reg_a000_w(uint32_t data); // SUCGPU_TFFHR (write)
	uint32_t reg_a00c_r();          // SUCGPU_TFFCNT (read)
	uint32_t reg_a010_r();          // SUCGPU_TFFMAX (read)
	uint32_t reg_a040_r();          // SUCGPU_RFFHR (read)
	uint32_t reg_a04c_r();          // SUCGPU_RFFCNT (read)
	uint32_t reg_a050_r();          // SUCGPU_RFFMAX (read)
	uint32_t reg_aab8_r();          // SUCSC0_GPIOVAL (read)

	/* modUnit registers */

	/* UTV dmaUnit registers */

	uint32_t reg_c020_r();
	void reg_c020_w(uint32_t data);
	uint32_t reg_c024_r();
	void reg_c024_w(uint32_t data);
	uint32_t reg_c028_r();
	void reg_c028_w(uint32_t data);
	uint32_t reg_c02c_r();
	void reg_c02c_w(uint32_t data);
	uint32_t reg_c040_r();
	void reg_c040_w(uint32_t data);
	uint32_t reg_c100_r();
	void reg_c100_w(uint32_t data);
	void utvdma_start();
	void utvdma_next();
	void utvdma_stop();

	/* Hardware modem registers */

	uint32_t reg_modem_0000_r();          // Modem I/O port base   (RBR/DLL read)
	void reg_modem_0000_w(uint32_t data); // Modem I/O port base   (THR/DLL write)
	uint32_t reg_modem_0004_r();          // Modem I/O port base+1 (IER/DLM read)
	void reg_modem_0004_w(uint32_t data); // Modem I/O port base+1 (IER/DLM write)
	uint32_t reg_modem_0008_r();          // Modem I/O port base+2 (IIR/FCR read)
	void reg_modem_0008_w(uint32_t data); // Modem I/O port base+2 (IIR/FCR write)
	uint32_t reg_modem_000c_r();          // Modem I/O port base+3 (LCR read)
	void reg_modem_000c_w(uint32_t data); // Modem I/O port base+3 (LCR write)
	uint32_t reg_modem_0010_r();          // Modem I/O port base+4 (MCR read)
	void reg_modem_0010_w(uint32_t data); // Modem I/O port base+4 (MCR write)
	uint32_t reg_modem_0014_r();          // Modem I/O port base+5 (LSR read)
	void reg_modem_0014_w(uint32_t data); // Modem I/O port base+5 (LSR write)
	uint32_t reg_modem_0018_r();          // Modem I/O port base+6 (MSR read)
	void reg_modem_0018_w(uint32_t data); // Modem I/O port base+6 (MSR write)
	uint32_t reg_modem_001c_r();          // Modem I/O port base+7 (SCR read)
	void reg_modem_001c_w(uint32_t data); // Modem I/O port base+7 (SCR write)
	uint8_t get_modem_iir();
	uint8_t get_wince_modem_iir(bool clear_threint);
	int get_wince_intrpt_r();

	/* IDE registers */

	uint32_t reg_ide_000000_r();          // IDE I/O port cs0[0] (data read)
	void reg_ide_000000_w(uint32_t data); // IDE I/O port cs0[0] (data write)
	uint32_t reg_ide_000004_r();          // IDE I/O port cs0[1] (error read)
	void reg_ide_000004_w(uint32_t data); // IDE I/O port cs0[1] (feature write)
	uint32_t reg_ide_000008_r();          // IDE I/O port cs0[2] (sector count read)
	void reg_ide_000008_w(uint32_t data); // IDE I/O port cs0[2] (sector count write)
	uint32_t reg_ide_00000c_r();          // IDE I/O port cs0[3] (sector number read)
	void reg_ide_00000c_w(uint32_t data); // IDE I/O port cs0[3] (sector number write)
	uint32_t reg_ide_000010_r();          // IDE I/O port cs0[4] (cylinder low read)
	void reg_ide_000010_w(uint32_t data); // IDE I/O port cs0[4] (cylinder low write)
	uint32_t reg_ide_000014_r();          // IDE I/O port cs0[5] (cylinder high read)
	void reg_ide_000014_w(uint32_t data); // IDE I/O port cs0[5] (cylinder high write)
	uint32_t reg_ide_000018_r();          // IDE I/O port cs0[6] (drive/head read)
	void reg_ide_000018_w(uint32_t data); // IDE I/O port cs0[6] (drive/head write)
	uint32_t reg_ide_00001c_r();          // IDE I/O port cs0[7] (drive/head read)
	void reg_ide_00001c_w(uint32_t data); // IDE I/O port cs0[7] (drive/head write)
	uint32_t reg_ide_400018_r();          // IDE I/O port cs1[6] (altstatus read)
	void reg_ide_400018_w(uint32_t data); // IDE I/O port cs1[6] (device control write)
	uint32_t reg_ide_40001c_r();          // IDE I/O port cs1[7] (device address read)
	void reg_ide_40001c_w(uint32_t data); // IDE I/O port cs1[7] (device address write)

};

DECLARE_DEVICE_TYPE(SOLO_ASIC, solo_asic_device)

#endif // MAME_WEBTV_SOLO_ASIC_H