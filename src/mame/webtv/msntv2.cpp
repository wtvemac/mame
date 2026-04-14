// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "cpu/i386/i386.h"
#include "machine/pci.h"
#include "i82830_host.h"
#include "i82830_gfx.h"
#include "i82801_usb.h"
#include "i82801_lpc.h"
#include "lpc47m192.h"
#include "i82801_ide.h"
#include "machine/pci-smbus.h"
#include "i82801_ac97.h"
#include "i82801_eth.h"
#include "wtvvidstream.h"
#include "msntv2_fpanel.h"
#include "stac9767.h"
#include "bus/rs232/null_modem.h"

#include "webtv.lh"
#include "main.h"

static DEVICE_INPUT_DEFAULTS_START(coma)
	DEVICE_INPUT_DEFAULTS("RS232_RXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_TXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_DATABITS", 0xff, RS232_DATABITS_8)
	DEVICE_INPUT_DEFAULTS("RS232_PARITY", 0xff, RS232_PARITY_NONE)
	DEVICE_INPUT_DEFAULTS("RS232_STOPBITS", 0xff, RS232_STOPBITS_1)
DEVICE_INPUT_DEFAULTS_END

static DEVICE_INPUT_DEFAULTS_START(comb)
	DEVICE_INPUT_DEFAULTS("RS232_RXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_TXBAUD", 0xff, RS232_BAUD_115200)
	DEVICE_INPUT_DEFAULTS("RS232_DATABITS", 0xff, RS232_DATABITS_8)
	DEVICE_INPUT_DEFAULTS("RS232_PARITY", 0xff, RS232_PARITY_NONE)
	DEVICE_INPUT_DEFAULTS("RS232_STOPBITS", 0xff, RS232_STOPBITS_1)
DEVICE_INPUT_DEFAULTS_END

class msntv2_state : public driver_device
{

public:

	static constexpr uint32_t SMBUS_CONSUME_INTERVAL = 10; // time is in microseconds
	static constexpr uint32_t SMBUS_69_BUFFER_LEN = 32;

	msntv2_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		// Northbridge devices
		m_mcu(*this, "pci:00.0"),
		m_gfx(*this, "pci:02.0"),
		// Southbridge devices
		m_usb1(*this, "pci:1d.%d", 0),
		m_usb2(*this, "pci:1d.7"),
		m_south_pci_bridge(*this, "pci:1e.0"),
		m_south_lpc_bridge(*this, "pci:1f.0"),
		m_ide(*this, "pci:1f.1"),
		m_smbus(*this, "pci:1f.3"),
		m_audio(*this, "pci:1f.5"),
		m_softmodem(*this, "pci:1f.6"),
		// Other board devices
		m_cx25873(*this, "encoder")
	{ }

	void msntv2(machine_config &config);

protected:

	virtual void machine_start() override;
	virtual void machine_reset() override;

private:

	required_device<i386_device> m_maincpu;
	required_device<i82830_host_device> m_mcu;
	required_device<i82830_graphics_device> m_gfx;
	required_device_array<i82801_usb_uhci_device, 3> m_usb1;
	required_device<i82801_usb_ehci_device> m_usb2;
	required_device<pci_bridge_device> m_south_pci_bridge;
	required_device<i82801_lpc_device> m_south_lpc_bridge;
	required_device<i82801_ide_device> m_ide;
	required_device<smbus_device> m_smbus;
	required_device<i82801_ac97_device> m_audio;
	required_device<i82801_mc97_device> m_softmodem;
	required_device<cx25873_encoder_device> m_cx25873;

	uint8_t m_pport[8];
	uint32_t m_gfx_enc_iic;

	uint8_t smbus_69_head_index;
	uint8_t smbus_69_tail_index;
	uint8_t smbus_69_block_size;
	uint8_t smbus_69_block[SMBUS_69_BUFFER_LEN];
	bool smbus_69_block_completed;
	bool ceddk_stall_hack_en = true;

	void ceddk_stall_hack();
	void ide0_legacy_irq(int state);
	void ide1_legacy_irq(int state);
	void ide_pirq(offs_t offset, uint8_t state);
	void ide_set_subsystem_id(uint32_t data);

	static void configure_superio(device_t *device);
	static void configure_fpanel(device_t *device);
	static void configure_ethernet(device_t *device);
	uint32_t gfx_gpio_r(offs_t offset);
	void gfx_gpio_w(offs_t offset, uint32_t data);
	uint8_t pport_r(offs_t offset);
	void pport_w(offs_t offset, uint8_t data);
	void smbus_control_2d(int status);
	void smbus_control_69(int status);
	void smbus_control(int status);
	uint8_t smbus_block_byte_read();
	void smbus_block_byte_write(uint8_t data);
	void smbus_status(int status);

};

void msntv2_state::machine_start()
{
	smbus_69_head_index = 0;
	smbus_69_tail_index = 0;
	smbus_69_block_completed = false;
}

void msntv2_state::machine_reset()
{
	//
}

void msntv2_state::configure_superio(device_t *device)
{
	/*
	uint8_t enabled_suporio_slots = lpc47m192_device::ENABLE_SLOT_SERIAL0 | lpc47m192_device::ENABLE_SLOT_SERIAL1 | lpc47m192_device::ENABLE_SLOT_LPT | lpc47m192_device::ENABLE_SLOT_KBD;
	.set_enabled_slots(enabled_suporio_slots);
	*/

	auto &superio = *downcast<lpc47m192_device *>(device);

	superio.irq1_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq1_w));
	superio.irq2_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq2_w));
	superio.irq3_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq3_w));
	superio.irq4_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq4_w));
	superio.irq5_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq5_w));
	superio.irq6_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq6_w));
	superio.irq7_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq7_w));
	superio.irq8_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq8_w));
	superio.irq9_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq9_w));
	superio.irq10_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq10_w));
	superio.irq11_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq11_w));
	superio.irq12_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq12_w));
	superio.irq13_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq13_w));
	superio.irq14_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq14_w));
	superio.irq15_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::irq15_w));

	superio.system_reset_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::line_reset_w));
	superio.gate_a20_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::line_a20_w));

	machine_config &config = (machine_config&)superio.mconfig();

	auto pport_device_options = [] (device_slot_interface &slot) {
		slot.option_add("msntv2_fpanel", MSNTV2_FPANEL)
			.machine_config(configure_fpanel);
	};
	superio.connect_pport_centronics(config, "pport", pport_device_options, "msntv2_fpanel");

	auto coma_device_options = [] (device_slot_interface &slot) {
		default_rs232_devices(slot);

		slot.set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(coma));
	};
	superio.connect_coma_rs232(config, "coma", coma_device_options, "null_modem");

	auto comb_device_options = [] (device_slot_interface &slot) {
		default_rs232_devices(slot);

		slot.set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(comb));
	};
	superio.connect_comb_rs232(config, "comb", comb_device_options, "null_modem");

	superio.connect_kbdc_mouse(config, "mouse");
}

void msntv2_state::configure_fpanel(device_t *device)
{
	auto &fpanel = *downcast<msntv2_fpanel_device *>(device);

	auto &superio = *downcast<lpc47m192_device *>(fpanel.owner()->owner()->subdevice("lpc47m192"));
	auto &irkbd = *downcast<pic12f629_irkbd_device *>(fpanel.subdevice("irkbd"));

	irkbd.out_w_callback().set(superio, FUNC(lpc47m192_device::keyboard_out_w));
	irkbd.keypress().set(irkbd, FUNC(pic12f629_irkbd_device::send_keypresses)); ///
	superio.keyboard_in_w_callback().set(irkbd, FUNC(pic12f629_irkbd_device::in_w));
}

void msntv2_state::configure_ethernet(device_t *device)
{
	// 0x0006: Network Controller; 0x0000: Ethernet Controller
	uint32_t device_class = 0x00020000;

	auto &eth = *downcast<i82801_eth_device *>(device);
	eth.set_ids(0x8086103a, 0x82, device_class, 0x00000000);
	eth.set_busmaster_tag(":maincpu", AS_PROGRAM);
	eth.set_connected_pirq(i82801_lpc_device::INT_PIN_A, i82801_lpc_device::PIRQ_SELECT_A);
	eth.pirq_callback().set(":pci:1f.0", FUNC(i82801_lpc_device::pirq_w));
	eth.set_default_link_state(true);
}

// MSNTV uses a Conexant CX25873 encoder for video out and in. It's controlled using the i2c protocol at the 0x88 address.
// The i2c pins are controlled in software using the GPIO registers provided by Intel's 82830 graphics chip.

uint32_t msntv2_state::gfx_gpio_r(offs_t offset)
{
	if(offset == i82830_graphics_device::MM_IOCNTL_GPIOB)
	{
		// There's no two-step procedure when reading. Just write the bit at position 0x0c and bit 0x04 always needs to be 1.

		uint32_t sda_bit = (m_cx25873->sda_read()) & 0x1;

		return (sda_bit << 0x0c) | (1 << 0x04);
	}
	else
	{
		return 0xff;
	}
}

void msntv2_state::gfx_gpio_w(offs_t offset, uint32_t data)
{
	if(offset == i82830_graphics_device::MM_IOCNTL_GPIOB)
	{
		// Based on what I see in the MSNTV v1.387 BIOS:

		// Write a 1 to the i2c data line if we write 0x500 then 0x400 to the GPIOB register.
		if(m_gfx_enc_iic == 0x500 && data == 0x400)
			m_cx25873->sda_write(1);
		// Write a 0 to the i2c data line if we write 0x700 then 0x600 to the GPIOB register.
		else if(m_gfx_enc_iic == 0x700 && data == 0x600)
			m_cx25873->sda_write(0);
		// Write a 1 to the i2c clock line if we write 0x005 then 0x004 to the GPIOB register.
		else if(m_gfx_enc_iic == 0x005 && data == 0x004)
			m_cx25873->scl_write(1);
		// Write a 0 to the i2c clock line if we write 0x007 then 0x006 to the GPIOB register.
		else if(m_gfx_enc_iic == 0x007 && data == 0x006)
			m_cx25873->scl_write(0);

		m_gfx_enc_iic = data;
	}
}

void msntv2_state::smbus_control_2d(int status)
{
	/*
		I 1 a040 ff
		i 1 a040
		I 1 a044 5b
		I 1 a043 26
		I 1 a040 00
		I 1 a042 48
		I 1 a040 00
		i 1 a040
		i 1 a045
	*/
	if(m_smbus->hst_cmd_r() == 0x26) // Used in IOCTL_MSNTV_GET_TEMPERATURE
	{
		if((m_smbus->hst_cnt_r() & smbus_device::HST_CNT_SMB_CMD) == smbus_device::SMB_CMD_WRBYTE)
		{
			if(m_smbus->is_slv_read())
				m_smbus->set_hst_d0(0x55);
			//else
				//
		}

		// Always set success status to satisfy software
		m_smbus->set_hst_sts(smbus_device::HST_STS_SUCCESS);
	}
	/*
		I 1 a040 ff
		i 1 a040
		I 1 a044 5b
		I 1 a043 22
		I 1 a040 00
		I 1 a042 48
		I 1 a040 00
		i 1 a040
		i 1 a045
	*/
	else if(m_smbus->hst_cmd_r() == 0x22) // Used in IOCTL_MSNTV_GET_VOLTAGE
	{
		if((m_smbus->hst_cnt_r() & smbus_device::HST_CNT_SMB_CMD) == smbus_device::SMB_CMD_WRBYTE)
		{
			if(m_smbus->is_slv_read())
				m_smbus->set_hst_d0(0x55);
			//else
				//
		}

		// Always set success status to satisfy software
		m_smbus->set_hst_sts(smbus_device::HST_STS_SUCCESS);
	}
	else
	{
		if(m_smbus->is_slv_read())
			m_smbus->set_hst_d0(0x00);

		if(smbus_69_block_completed && m_smbus->hst_d0_r() == 0x09)
		{
			smbus_69_block_completed = false;
			// After boot, software doesn't want a success status after it block sends to 0x69
			m_smbus->set_hst_sts(smbus_device::HST_STS_DEV_ERR);
		}
		else
		{
			// Set success status to satisfy software
			m_smbus->set_hst_sts(smbus_device::HST_STS_SUCCESS);
		}
	}
}

void msntv2_state::smbus_control_69(int status)
{
	if((m_smbus->hst_cnt_r() & smbus_device::HST_CNT_SMB_CMD) == smbus_device::SMB_CMD_WRBLOCK && !(m_smbus->hst_cnt_r() & smbus_device::HST_STS_SMBALERT))
	{
		smbus_69_tail_index = 0;

		if(m_smbus->is_slv_read())
		{
			m_smbus->set_hst_d0(smbus_69_block_size);
			smbus_69_head_index = smbus_69_block_size;
			smbus_69_block_completed = false;
			m_smbus->set_hst_sts(smbus_device::HST_STS_GOT_BBYTE | smbus_device::HST_STS_SUCCESS);
		}
		else
		{
			smbus_69_head_index = m_smbus->hst_d0_r();
			if(smbus_69_head_index >= SMBUS_69_BUFFER_LEN)
			{
				smbus_69_head_index = 0;
				smbus_69_block_completed = true;
				m_smbus->set_hst_sts(smbus_device::HST_STS_FAILED);
			}
			else
			{
				smbus_69_block_completed = false;
				smbus_69_block[smbus_69_tail_index++ & (SMBUS_69_BUFFER_LEN - 1)] = m_smbus->get_host_blkbyte();
				m_smbus->set_hst_sts(smbus_device::HST_STS_GOT_BBYTE | smbus_device::HST_STS_SUCCESS);
			}
		}
	}
}

void msntv2_state::smbus_control(int status)
{
	uint8_t device_address = m_smbus->get_slv_addr();

	switch(device_address)
	{
		// The MSNTV2 searches for a SPD response at 0x50 and 0x52. If both error then it auto-populates the Northbridge's DRB for 128MB. If found, read SPD to populate 128MB+Calculated amount from SPD
		case 0x52:
		case 0x50:
			m_smbus->set_hst_sts(smbus_device::HST_STS_DEV_ERR);
			break;

		case 0x2d: // Seems to be related to auidio or system monitoring
			msntv2_state::smbus_control_2d(status);
			break;

		case 0x69: // Might be related to audio.
			msntv2_state::smbus_control_69(status);
			break;

		default:
			m_smbus->set_hst_sts(smbus_device::HST_STS_DEV_ERR);
			break;
	}
}

uint8_t msntv2_state::smbus_block_byte_read()
{
	uint8_t device_address = m_smbus->get_slv_addr();

	switch(device_address)
	{
		case 0x69:
		{
			if(m_smbus->is_slv_read() && smbus_69_head_index > 0 && smbus_69_tail_index <= smbus_69_head_index)
			{
				m_smbus->set_hst_sts(smbus_device::HST_STS_GOT_BBYTE | smbus_device::HST_STS_SUCCESS);
				return smbus_69_block[smbus_69_tail_index++ & (SMBUS_69_BUFFER_LEN - 1)];
			}
			else
			{
				return 0x00;
			}
			break;
		}

		default:
			return 0x00;
	}
}

void msntv2_state::smbus_block_byte_write(uint8_t data)
{
	uint8_t device_address = m_smbus->get_slv_addr();

	switch(device_address)
	{
		case 0x69:
		{
			if(!m_smbus->is_slv_read() && smbus_69_head_index > 0 && smbus_69_tail_index <= smbus_69_head_index)
			{
				smbus_69_block[smbus_69_tail_index++ & (SMBUS_69_BUFFER_LEN - 1)] = m_smbus->get_host_blkbyte();
			}
			break;
		}

		default:
			break;
	}
}

void msntv2_state::smbus_status(int status)
{
	uint8_t device_address = m_smbus->get_slv_addr();

	switch(device_address)
	{
		case 0x69:
		{
			if(!smbus_69_block_completed && smbus_69_tail_index > smbus_69_head_index)
			{
				smbus_69_block_size = smbus_69_head_index;
				smbus_69_block_completed = true;
				smbus_69_head_index = 0;
				smbus_69_tail_index = 0;
			}
			else
			{
				if(smbus_69_tail_index == smbus_69_head_index)
					smbus_69_tail_index++;
				
				m_smbus->set_hst_sts(smbus_device::HST_STS_GOT_BBYTE | smbus_device::HST_STS_SUCCESS);
			}
			break;
		}

		default:
			break;
	}
}

void msntv2_state::ide_set_subsystem_id(uint32_t subdevice_id)
{
	m_usb1[0]->set_subsystem_id(subdevice_id);
	m_usb1[1]->set_subsystem_id(subdevice_id);
	m_usb1[2]->set_subsystem_id(subdevice_id);

	// 0x000c: Serial Bus Controller; 0x0500: 0x05xx=SMBus Controller
	uint32_t smbus_device_class = 0x000c0500;
	m_smbus->set_ids(0x808624c3, 0x00, smbus_device_class, subdevice_id);
}

// This is a hack to force ceddk.dll to not stall waiting for the device to respond
void msntv2_state::ceddk_stall_hack()
{
	// During boot ichide.dll calls ceddk.dll which waits in a loop checking this value.
	// Not sure if it's waiting for an interrupt or this is configurable at compile-time but this
	// speeds up the boot process and doesn't seem to cause any issues if this is set to 1.
	// This is set to 1 every time we re-map the PCI memory space.

	uint64_t ceddk_stall_virtaddr[1] = {
		0x01fe00b8 // value in ceddk.dll that needs to be 1 to stop the stall loop. Seems to be the same across multiple NK.bin versions.
	};

	uint64_t ceddk_stall_physaddr = m_maincpu->debug_virttophys(0, ceddk_stall_virtaddr);

	if(ceddk_stall_physaddr > 0)
	{
		address_space& program_space = m_maincpu->space(AS_PROGRAM);

		uint32_t ceddk_stall_break = program_space.read_dword(ceddk_stall_physaddr);

		if(ceddk_stall_break == 0x00000000)
		{
			program_space.write_dword(ceddk_stall_physaddr, 0x00000001);
		}
	}
}

void msntv2_state::ide0_legacy_irq(int state)
{
	if(ceddk_stall_hack_en)
		msntv2_state::ceddk_stall_hack();

	m_south_lpc_bridge->irq14_w(state);
}

void msntv2_state::ide1_legacy_irq(int state)
{
	if(ceddk_stall_hack_en)
		msntv2_state::ceddk_stall_hack();

	m_south_lpc_bridge->irq15_w(state);
}

void msntv2_state::ide_pirq(offs_t offset, uint8_t state)
{
	if(ceddk_stall_hack_en)
		msntv2_state::ceddk_stall_hack();

	m_south_lpc_bridge->pirq_w(offset, state);
}

void msntv2_state::msntv2(machine_config &config)
{
	config.set_default_layout(layout_webtv);

	P3CELERON(config, m_maincpu, 733'333'333); // "Socket 479" mobile Celeron on RM4100, "Socket 479" mobile Pentium 3 on IP1000
	m_maincpu->set_irq_acknowledge_callback(m_south_lpc_bridge, FUNC(i82801_lpc_device::irq_acknowledge));

	// The MSNTV2 uses CX25873 chip that is controlled by an I2C signal from the northbridge's graphics GPIO pins.
	CX25873(config, m_cx25873, 0x88);

	PCI_ROOT(config, "pci", 0);

		// The MSNTV2 v1.387 BIOS will assume 128MB is on the board and then query the SMBus for SPD data to add additional memory.
		// The MSNTV2 board has a DIMM slot footprint on the board to add additional memory modules.
		I82830_HOST(config, m_mcu, 0, "maincpu", 128 * 1024 * 1024); // 128MB max system RAM, exact amount configured from the BIOS
			m_mcu->interrupt_pin_w(0, i82801_lpc_device::INT_PIN_NONE);
			m_mcu->enable_agp(false); // The AGP bridge @ pci:01.0 is not available

		I82830_CGC(config, m_gfx, 0, "pci:00.0"); // Graphics integrated into the northbridge. Graphics memory paging functions similarly to AGP
			m_gfx->set_connected_pirq(i82801_lpc_device::INT_PIN_A, i82801_lpc_device::PIRQ_SELECT_A);
			m_gfx->gpio_read_callback().set(FUNC(msntv2_state::gfx_gpio_r));
			m_gfx->gpio_write_callback().set(FUNC(msntv2_state::gfx_gpio_w));
			m_gfx->pirq_callback().set(m_south_lpc_bridge, FUNC(i82801_lpc_device::pirq_w));

		// The MSNTV2 v1.387 BIOS only initilizes the USB 1.0 controller so the ports are 1.0 in the BIOS
		// The ports switch to 2.0 mode once it boots into Windows CE and the EHCI device is initlized (if I'm reading the ICH4 docs correctly)
		I82801_USB_UHCI(config, m_usb1[0], 0, 0x808624c2, 0x00); // Southbridge's USB 1.0 #0
			m_usb1[0]->set_connected_pirq(i82801_lpc_device::INT_PIN_A, i82801_lpc_device::PIRQ_SELECT_A);
		I82801_USB_UHCI(config, m_usb1[1], 0, 0x808624c4, 0x00); // Southbridge's USB 1.0 #1; checked for accurate VID, DID, SVID SDID, CLSID, and USB release num in the MSNTV2 v1.387 BIOS
			m_usb1[1]->set_connected_pirq(i82801_lpc_device::INT_PIN_B, i82801_lpc_device::PIRQ_SELECT_D);
		I82801_USB_UHCI(config, m_usb1[2], 0, 0x808624c7, 0x00); // Southbridge's USB 1.0 #2; checked for accurate VID, DID, SVID SDID, CLSID, and USB release num in the MSNTV2 v1.387 BIOS
			m_usb1[2]->set_connected_pirq(i82801_lpc_device::INT_PIN_C, i82801_lpc_device::PIRQ_SELECT_C);
		I82801_USB_EHCI(config, m_usb2, 0, 0x808624cd, 0x00); // Southbridge's USB 2.0
			m_usb2->set_connected_pirq(i82801_lpc_device::INT_PIN_D, i82801_lpc_device::PIRQ_SELECT_H);

		PCI_BRIDGE(config, m_south_pci_bridge, 0, 0x8086244e, 0x00); // Southbridge's PCI bus, Intel's integrated ethernet connected (device 8)
			m_south_pci_bridge->interrupt_pin_w(0, i82801_lpc_device::INT_PIN_NONE);
			auto pci_device_options = [] (device_slot_interface &slot) {
				// The v1.387 BIOS supports these ethernet controllers:
				//   8086 103a: Intel PRO/100 VM
				//   8086 1039: Intel 82801DB
				//   8086 1051: Intel 82801EB <-- the default IDs used in I82801_ETH
				//   8086 2449: Intel 82801BA/BAM/CA/CAM
				slot.option_add("i82801_eth", I82801_ETH)
					.machine_config(configure_ethernet);
			};
			//
			// There seems to be an unpopulated PCI slot footprint on the board from pictures I've seen. We'd define that slot here.
			//
			// "ICH4 integrated LAN controller appears to reside at PCI Device 8, Function 0 on the secondary side of the ICH4's virtual PCI-to-PCI Bridge"
			PCI_SLOT(config, "pci:1e.0:slot8", pci_device_options, 8, 0, 1, 2, 3, "i82801_eth");

		I82801_LPC(config, m_south_lpc_bridge, 0, "maincpu"); // Southbridge's LPC/ISA bus, Intel's SuperIO connected (device 0)
			m_south_lpc_bridge->interrupt_pin_w(0, i82801_lpc_device::INT_PIN_NONE);
			m_south_lpc_bridge->set_dummydelay_ioaddr(0x00eb);
			auto lpc_device_options = [] (device_slot_interface &slot) {
				slot.option_add("lpc47m192", LPC47M192)
					.machine_config(configure_superio);
			};
			// This is really an isabus rather than lpcbus in MAME. Software doesn't care.
			ISA16_SLOT(config, "pci:1f.0:lpcbus:slot0", 0, "pci:1f.0:lpcbus", lpc_device_options, "lpc47m192", true);

		// IDE Config
		//    ide[0].master=compact flash card
		//    ide[0].slave=missing
		//    ide[1].master=missing
		//    ide[1].slave=missing
		// "hdd", "cdrom", false, "maincpu");
		I82801_IDE(config, m_ide, 0, "cf", nullptr, true, nullptr, nullptr, false, "maincpu"); // Southbridge's IDE controller
			m_ide->set_connected_pirq(i82801_lpc_device::INT_PIN_A, i82801_lpc_device::PIRQ_SELECT_C);
			m_ide->ide0_legacy_irq_callback().set(FUNC(msntv2_state::ide0_legacy_irq));
			m_ide->ide1_legacy_irq_callback().set(FUNC(msntv2_state::ide1_legacy_irq));
			m_ide->pirq_callback().set(FUNC(msntv2_state::ide_pirq));
			m_ide->subsystem_id_callback().set(FUNC(msntv2_state::ide_set_subsystem_id));

		SMBUS(config, m_smbus, 0, 0x808624c3, 0x00, 0x00000000); // Southbridge's SMBus controller
			m_smbus->interrupt_pin_w(0, i82801_lpc_device::INT_PIN_B);
			m_smbus->control_callback().set(FUNC(msntv2_state::smbus_control));
			m_smbus->status_callback().set(FUNC(msntv2_state::smbus_status));
			m_smbus->block_byte_read_callback().set(FUNC(msntv2_state::smbus_block_byte_read));
			m_smbus->block_byte_write_callback().set(FUNC(msntv2_state::smbus_block_byte_write));

		auto codec_setup = [] (aclink_connection_interface &aclink, machine_config &mconfig) {
			aclink.codec_insert(mconfig, 0, STAC9767);
		};
		I82801_AC97(config, m_audio, 0, codec_setup); // Southbridge's AC'97 2.7 audio
			m_audio->set_connected_pirq(i82801_lpc_device::INT_PIN_B, i82801_lpc_device::PIRQ_SELECT_B);
			m_audio->pirq_callback().set(m_south_lpc_bridge, FUNC(i82801_lpc_device::pirq_w));

		// Unused. The MSNTV2 uses a CX81801-94 hardmodem that's connected to the COM2 port of the SuperIO.
		// This is available because the ICH4 supports it but no softmodem codec is hooked up.
		I82801_MC97(config, m_softmodem, 0); // Southbridge's AC'97 2.7 softmodem.
			m_softmodem->set_connected_pirq(i82801_lpc_device::INT_PIN_B, i82801_lpc_device::PIRQ_SELECT_B);
}


ROM_START(msntv2)
	ROM_REGION32_LE(0x100000, "pci:1f.0", 0)
	//
	// For non-PC BIOSes, you're given a short time to type these commands after the "#" character is printed out COM1 during boot:
	//     - '$' will drop into the service menu.
	//     - '+' not sure, might be verbose boot logging to a log file in the boot partition.
	//     - '@URL' load a custom application via URL. Looks to be BOOT.SIG verified.
	//
	// bios-emac.bin is a RM4100 Retail BIOS (v1.387) BIOS with these notes:
	//     - Removed BIOS checksum checking so the BIOS can be modified (easily). [POST error code 135]
	//     - Change upgrade/disaster recovery host from headwaiter.msntv.msn.com to msntv2.ooguy.com. NOTE: this can also be done through DNS spoofing.
	//     - Removed memory checking to speed up boot.
	//     - Removed RSA SHA1 signature checking. MD5 integrety checking still exists.
	//
	ROM_SYSTEM_BIOS(0, "emac", "RM4100 emac-mod BIOS (v1.387)")
	ROMX_LOAD("bios-emac.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(0))
	//
	// bios-retail.bin is the stock MSNTV2 BIOS. This will boot fine, just not with the modified HackTV CF image because of the RSA SHA1 gate.
	//
	ROM_SYSTEM_BIOS(1, "retail", "RM4100 Retail BIOS (v1.387)")
	ROMX_LOAD("bios-retail.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(1))
	//
	// This isn't included since the Thomson IP1000 doesn't seem to be a MSNTV2 box. Might be worth adding another machine for this since it's the same tech as the MSNTV2. Uncomment to add this yourself for now.
	//
	// bios-euro.bin I believe is the BIOS for the Thomson IP1000
	//     - It initilizes only 64MB instead of RM4100's 128MB of RAM.
	//     - Blue background for the disaster recovery screens (rather than green) and uses syncserver.iptv.microsoft.com as the service URL.
	//     - The signing keys are different from the RM4100, so none of the WinCE images will work.
	//     - COM2 for the modem isn't used at all. Ethernet must be used.
	//
	// The i82830_host.cpp will identify the RAM difference without a machine config update. It's based on BIOS writes to the drb registers.
	//
	//ROM_SYSTEM_BIOS(2, "euro", "IP1000 'Euro' BIOS (v1.10159)")
	//ROMX_LOAD("bios-euro.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(2))
	//
	// bios-linux.bin is a RM4100 Retail BIOS (v1.387) BIOS with these notes:
	//     - It loads a vmlinux.bin file using the config from a cmdline file. Both these files need to be in the MSNTV2 boot partition.
	//     - This mod is from toc2rta. Details: https://wiki.webtv.zone/mediawiki/index.php/Installing_Linux_on_the_MSN_TV_2_(RM4100)
	//     - Video out isn't working in toc2rta's version of Linux. Linux does support most of MSNTV2's hardware out-of-the-box so it's possible to get it working if you put in the work.
	//
	ROM_SYSTEM_BIOS(2, "toc2rta", "RM4100 toc2rta-mod BIOS (v1.387)")
	ROMX_LOAD("bios-toc2rta.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(2))
	//
	// Note: bios-pc.bin and bios-coreboot.bin might be split into a separate machine config since they work best with modified hardware.
	//
	// bios-pc.bin:
	//     - Regular PC BIOS. Turns the MSNTV2 into a PC.
	//
	ROM_SYSTEM_BIOS(3, "pc", "PC BIOS")
	ROMX_LOAD("bios-pc.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(3))
	//
	// bios-coreboot.bin:
	//     - Older coreboot image from the settoplinux folks. Newer versions of Coreboot stripped the RM4100 out. 
	//     - It requres Linux to be placed in its own partition. Loaded using FILO.
	//     - Similar to bios-pc.bin, the MSNTV2 is treated like a PC.
	//
	ROM_SYSTEM_BIOS(4, "coreboot", "Coreboot BIOS")
	ROMX_LOAD("bios-coreboot.bin", 0x000000, 0x100000, NO_DUMP, ROM_BIOS(4))
ROM_END

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

//   YEAR  NAME    PARENT COMPAT  MACHINE  INPUT CLASS        INIT        COMPANY      FULLNAME              FLAGS
CONS(2004, msntv2, 0,     0,      msntv2, 0,    msntv2_state, empty_init, "Microsoft", "MSNTV2: RCA RM4100", MACHINE_UNOFFICIAL)
