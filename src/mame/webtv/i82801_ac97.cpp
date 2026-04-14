// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82801_ac97.h"

DEFINE_DEVICE_TYPE(I82801_AC97, i82801_ac97_device, "i82801_ac97", "i82801 ICH4 AC97 Audio Controller")
DEFINE_DEVICE_TYPE(I82801_MC97, i82801_mc97_device, "i82801_mc97", "i82801 ICH4 MC97 Modem Controller")

class i82801_ac97_base;

aclink_interface::aclink_interface(const machine_config &mconfig, device_t &device)
	: device_interface(device, "ac97_aclink"),
	m_ac97(dynamic_cast<i82801_ac97_base *>(device.owner()))
{
}

ac97_codec_device::ac97_codec_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock),
	aclink_interface(mconfig, *this)
{
	m_aud_clk = i82801_ac97_base::AUD_DEFAULT_CLK;
}

void ac97_codec_device::device_start()
{
	//
}

void ac97_codec_device::device_reset()
{
	nam_reset();
}

void ac97_codec_device::nam_reset()
{
	std::fill(std::begin(m_nam_regs), std::end(m_nam_regs), 0);

	// Basic capability register
	m_nam_regs[ac97_codec_device::NAM_REG_MASTER_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_AUX_OUT_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_MONO_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_MASTER_TONE] = 0x0f0f;
	m_nam_regs[ac97_codec_device::NAM_REG_PCBEEP_VOL] = 0x0000;
	m_nam_regs[ac97_codec_device::NAM_REG_PHONE_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_MIC_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_LINE_IN_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_CD_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_VID_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_AUX_IN_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_PCM_OUT_VOL] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_RECORD_SEL] = 0x0000;
	m_nam_regs[ac97_codec_device::NAM_REG_RECORD_GAIN] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_RECORD_GAIN_MIC] = ac97_codec_device::SOUND_MUTED;
	m_nam_regs[ac97_codec_device::NAM_REG_GEN_PURPOSE] = 0x0000;
	m_nam_regs[ac97_codec_device::NAM_REG_3D_CNTL] = 0x0000;
	// Reserved register/modem sample rate/audio interrupt and paging mechanism register
	m_nam_regs[ac97_codec_device::NAM_REG_PWRDOWN_CNTLSTAT] = 0x0000;
	// Extended audio registers
	// Vendor and reserved registers
}

uint16_t ac97_codec_device::nam_reg_r(offs_t offset)
{
	offset &= (ac97_codec_device::NAM_REG_CNT - 1);

	return m_nam_regs[offset];
}

void ac97_codec_device::nam_reg_w(offs_t offset, uint16_t data)
{
	offset &= (ac97_codec_device::NAM_REG_CNT - 1);


	if(offset == ac97_codec_device::NAM_REG_RESET)
		nam_reset();
	else
		m_nam_regs[offset] = data;
}

aclink_connection_interface::aclink_connection_interface(const machine_config &mconfig, device_t &device, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func)
	: aclink_interface(mconfig, device),
	m_codec(device, "codec%d", 0)
{
	m_setup_codecs_func = setup_codecs_func;
}

i82801_ac97_base::i82801_ac97_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func, uint32_t main_id, uint32_t revision)
	: pci_device(mconfig, type, tag, owner, clock),
	aclink_connection_interface(mconfig, *this, setup_codecs_func),
	m_pirq_cb(*this)
{
	// 0x04: Multimedia Controller; 0x01xx=Multimedia Audio Controller
	uint32_t device_class = 0x040100;

	set_ids(main_id, revision, device_class, 0x00000000);

	m_codec_enabled = 0;

	m_pcm1_in_offset = 0;
	m_pcm_out_offset = 0;
	m_mic1_in_offset = 0;
	m_mic2_in_offset = 0;
	m_pcm2_in_offset = 0;
	m_spidf_in_offset = 0;
	m_modem_in_offset = 0;
	m_modem_out_offset = 0;
}

void i82801_ac97_base::device_start()
{
	pci_device::device_start();
}

void i82801_ac97_base::device_reset()
{
	pci_device::device_reset();

	status = 0x0290;

	m_nammbar = i82801_ac97_base::AC97_DEFAULT_IO_BASE;
	m_nabmbar = i82801_ac97_base::AC97_DEFAULT_IO_BASE;
	m_mmbar = i82801_ac97_base::AC97_DEFAULT_MEM_BASE;
	m_mbbar = i82801_ac97_base::AC97_DEFAULT_MEM_BASE;
	m_svid = 0x0000;
	m_sid = 0x0000;
	m_pcid = 0x09;
	m_cfg = 0x00;

	m_cas = 0x00;

	i82801_ac97_base::nabm_reset();
}

void i82801_ac97_base::device_add_mconfig(machine_config &config)
{
	if (m_setup_codecs_func != nullptr)
		m_setup_codecs_func(*this, config);
}

void i82801_ac97_base::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space* memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space* io_space)
{

	uint16_t nammbar = (m_nammbar & i82801_ac97_base::AC97_IO_MXR_BASE_MASK);
	if(nammbar > 0 && (nammbar + i82801_ac97_base::AC97_NABM_REGS_SIZE) < 0x10000)
		io_space->install_device(nammbar, nammbar + (ac97_codec_device::NAM_REGS_SIZE - 1), *this, &i82801_ac97_base::nam_io_map);

	uint32_t mmbar = (m_mmbar & i82801_ac97_base::AC97_MEM_MXR_BASE_MASK);
	if(mmbar > 0)
		memory_space->install_device(mmbar, mmbar + (ac97_codec_device::NAM_REGS_SIZE - 1), *this, &i82801_ac97_base::nam_mem_map);

	uint16_t nabmbar = (m_nabmbar & i82801_ac97_base::AC97_IO_NABM_BASE_MASK);
	if(nabmbar > 0 && (nabmbar + i82801_ac97_base::AC97_NABM_REGS_SIZE) < 0x10000)
		io_space->install_device(nabmbar, nabmbar + (i82801_ac97_base::AC97_NABM_REGS_SIZE - 1), *this, &i82801_ac97_base::nabm_map);

	uint32_t mbbar = (m_mbbar & i82801_ac97_base::AC97_MEM_NABM_BASE_MASK);
	if(mbbar > 0)
		memory_space->install_device(mbbar, mbbar + (i82801_ac97_base::AC97_NABM_REGS_SIZE - 1), *this, &i82801_ac97_base::nabm_map);
}

void i82801_ac97_base::config_map(address_map &map)
{
	pci_device::config_map(map);

	// 0x10-0x13: Native Audio Mixer Base Address (IO)
	map(0x10, 0x13).rw(FUNC(i82801_ac97_base::nammbar_r), FUNC(i82801_ac97_base::nammbar_w));
	// 0x14-0x17: Native Audio Bus Mastering Base Address (IO)
	map(0x14, 0x17).rw(FUNC(i82801_ac97_base::nambbar_r), FUNC(i82801_ac97_base::nambbar_w));
	// 0x18-0x1b: Mixer Base Address (Mem)
	map(0x18, 0x1b).rw(FUNC(i82801_ac97_base::mmbar_r), FUNC(i82801_ac97_base::mmbar_w));
	// 0x1c-0x1f: Bus Master Base Address (Mem)
	map(0x1c, 0x1f).rw(FUNC(i82801_ac97_base::mbbar_r), FUNC(i82801_ac97_base::mbbar_w));
	// 0x2c-0x2d: Subsystem Vendor ID
	map(0x2c, 0x2d).w(FUNC(i82801_ac97_base::subvendor_w));
	// 0x2e-0x2f: Subsystem ID
	map(0x2e, 0x2f).w(FUNC(i82801_ac97_base::subsystem_w));
	// 0x40: Programmable Codec ID
	map(0x40, 0x40).rw(FUNC(i82801_ac97_base::pcid_r), FUNC(i82801_ac97_base::pcid_w));
	// 0x41: Configuration
	map(0x41, 0x41).rw(FUNC(i82801_ac97_base::cfg_r), FUNC(i82801_ac97_base::cfg_w));
}

// The I/O map can address codec 0 and 1
void i82801_ac97_base::nam_io_map(address_map &map)
{
	uint16_t regs_size = ac97_codec_device::NAM_REGS_SIZE * 2;

	map(0x0000, (regs_size - 1)).rw(FUNC(i82801_ac97_base::nam_reg_r), FUNC(i82801_ac97_base::nam_reg_w));
}

// The mem map can address all three codecs
void i82801_ac97_base::nam_mem_map(address_map &map)
{
	uint32_t regs_size = ac97_codec_device::NAM_REGS_SIZE * 3;

	map(0x00000000, (regs_size - 1)).rw(FUNC(i82801_ac97_base::nam_reg_r), FUNC(i82801_ac97_base::nam_reg_w));
}

uint16_t i82801_ac97_base::nam_reg_r(offs_t offset)
{
	uint16_t codec_id = offset & CODEC_ID_MASK;


	offset &= (ac97_codec_device::NAM_REG_CNT - 1);

	uint16_t data = 0xffff;

	if((m_codec_enabled & i82801_ac97_base::CODEC_EN0) && codec_id == i82801_ac97_base::CODEC_ID0)
		data = m_codec[0]->nam_reg_r(offset);
	else if((m_codec_enabled & i82801_ac97_base::CODEC_EN1) && codec_id == i82801_ac97_base::CODEC_ID1)
		data = m_codec[1]->nam_reg_r(offset);
	else if((m_codec_enabled & i82801_ac97_base::CODEC_EN2) && codec_id == i82801_ac97_base::CODEC_ID2)
		data = m_codec[2]->nam_reg_r(offset);
	else
		data = 0xffff;

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

	return data;
}

void i82801_ac97_base::nam_reg_w(offs_t offset, uint16_t data)
{
	uint16_t codec_id = offset & CODEC_ID_MASK;


	offset &= (ac97_codec_device::NAM_REG_CNT - 1);

	if((m_codec_enabled & i82801_ac97_base::CODEC_EN0) && codec_id == CODEC_ID0)
		m_codec[0]->nam_reg_w(offset, data);
	else if((m_codec_enabled & i82801_ac97_base::CODEC_EN1) && codec_id == CODEC_ID1)
		m_codec[1]->nam_reg_w(offset, data);
	else if((m_codec_enabled & i82801_ac97_base::CODEC_EN2) && codec_id == CODEC_ID2)
		m_codec[2]->nam_reg_w(offset, data);

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
}

void i82801_ac97_base::nabm_map(address_map &map)
{
	for(offs_t chan_idx = 0; chan_idx < m_chan_enabled_count; chan_idx++)
	{
		i82801_ac97_base::map_nabm_chan(chan_idx, map);
	}

	map(0x002c, 0x002f).rw(FUNC(i82801_ac97_base::glob_cntl_r), FUNC(i82801_ac97_base::glob_cntl_w));
	map(0x0030, 0x0033).rw(FUNC(i82801_ac97_base::glob_status_r), FUNC(i82801_ac97_base::glob_status_w));
	map(0x0034, 0x0034).rw(FUNC(i82801_ac97_base::cas_r), FUNC(i82801_ac97_base::cas_w));
	map(0x0080, 0x0080).rw(FUNC(i82801_ac97_base::sdata_in_r), FUNC(i82801_ac97_base::sdata_in_w));
}

void i82801_ac97_base::map_nabm_chan(offs_t offset, address_map &map)
{
	uint32_t base = m_chan_offsets[offset];

	if(base != i82801_ac97_base::NABM_CHAN_BLANK_OFFSET)
	{
		map(base + 0x0000, base + 0x0003).lrw32(
			NAME((
				[this, offset] () -> uint32_t
				{
					uint32_t data = i82801_ac97_base::nabm_chan_bdbar_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this, offset] (uint32_t data)
				{
					i82801_ac97_base::nabm_chan_bdbar_w(offset, data);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x0004, base + 0x0004).lrw8(
			NAME((
				[this, offset] () -> uint8_t
				{
					uint8_t data = i82801_ac97_base::nabm_chan_civ_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this] (uint8_t data)
				{
					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x0005, base + 0x0005).lrw8(
			NAME((
				[this, offset] () -> uint8_t
				{
					uint8_t data = i82801_ac97_base::nabm_chan_lvi_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this, offset] (uint8_t data)
				{
					i82801_ac97_base::nabm_chan_lvi_w(offset, data);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x0006, base + 0x0007).lrw16(
			NAME((
				[this, offset] () -> uint16_t
				{
					uint16_t data = i82801_ac97_base::nabm_chan_sr_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this, offset] (uint16_t data)
				{
					i82801_ac97_base::nabm_chan_sr_w(offset, data);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x0008, base + 0x0009).lrw16(
			NAME((
				[this, offset] () -> uint16_t
				{
					uint16_t data = i82801_ac97_base::nabm_chan_picb_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this] (uint16_t data)
				{
					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x000a, base + 0x000a).lrw8(
			NAME((
				[this, offset] () -> uint8_t
				{
					uint8_t data = i82801_ac97_base::nabm_chan_piv_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this] (uint8_t data)
				{
					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);

		map(base + 0x000b, base + 0x000b).lrw8(
			NAME((
				[this, offset] () -> uint8_t
				{
					uint8_t data = i82801_ac97_base::nabm_chan_cr_r(offset);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

					return data;
				}
			)),
			NAME((
				[this, offset] (uint8_t data)
				{
					i82801_ac97_base::nabm_chan_cr_w(offset, data);

					m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
				}
			))
		);
	}
}

void i82801_ac97_base::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	pci_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

uint32_t i82801_ac97_base::nammbar_r()
{
	return m_nammbar;
}

void i82801_ac97_base::nammbar_w(uint32_t data)
{
	uint32_t nammbar = (data & i82801_ac97_base::AC97_IO_MXR_BASE_MASK);
	if((m_cfg & i82801_ac97_base::AC97_CNFG_IOSE) && nammbar != 0x00000000)
	{
		m_nammbar = (m_nammbar & (~i82801_ac97_base::AC97_IO_MXR_BASE_MASK)) | nammbar;
		i82801_ac97_base::remap_cb();
	}
}

uint32_t i82801_ac97_base::nambbar_r()
{
	return m_nabmbar;
}

void i82801_ac97_base::nambbar_w(uint32_t data)
{
	uint32_t nabmbar = (data & i82801_ac97_base::AC97_IO_NABM_BASE_MASK);
	if((m_cfg & i82801_ac97_base::AC97_CNFG_IOSE) && nabmbar != 0x00000000)
	{
		m_nabmbar = (m_nabmbar & (~i82801_ac97_base::AC97_IO_NABM_BASE_MASK)) | nabmbar;
		i82801_ac97_base::remap_cb();
	}
}

uint32_t i82801_ac97_base::mmbar_r()
{
	return m_mmbar;
}

void i82801_ac97_base::mmbar_w(uint32_t data)
{
	uint32_t mmbar = (data & i82801_ac97_base::AC97_MEM_MXR_BASE_MASK);
	if(mmbar != 0x00000000)
	{
		m_mmbar = (m_mmbar & (~i82801_ac97_base::AC97_MEM_MXR_BASE_MASK)) | mmbar;
		i82801_ac97_base::remap_cb();
	}
}

uint32_t i82801_ac97_base::mbbar_r()
{
	return m_mbbar;
}

void i82801_ac97_base::mbbar_w(uint32_t data)
{
	uint32_t mbbar = (data & i82801_ac97_base::AC97_MEM_NABM_BASE_MASK);
	if(mbbar != 0x00000000)
	{
		m_mbbar = (m_mbbar & (~i82801_ac97_base::AC97_MEM_NABM_BASE_MASK)) | mbbar;
		i82801_ac97_base::remap_cb();
	}
}

void i82801_ac97_base::subvendor_w(uint16_t data)
{
	if(!(subsystem_id & 0xffff0000))
		subsystem_id = (subsystem_id & 0x0000ffff) | data << 0x10;
}

void i82801_ac97_base::subsystem_w(uint16_t data)
{
	if(!(subsystem_id & 0x0000ffff))
		subsystem_id = (subsystem_id & 0xffff0000) | data;
}

uint8_t i82801_ac97_base::capptr_r()
{
	return 0x50;
}

uint8_t i82801_ac97_base::pcid_r()
{
	return m_pcid;
}

void i82801_ac97_base::pcid_w(uint8_t data)
{
	m_pcid = data & 0x0f;
}

uint8_t i82801_ac97_base::cfg_r()
{
	return m_cfg;
}

void i82801_ac97_base::cfg_w(uint8_t data)
{
	m_cfg = data & 0x01;
}


//////////////
//////////////
//////////////

void i82801_ac97_base::nabm_reset()
{
	std::fill(std::begin(m_chan), std::end(m_chan), i82801_ac97_base::NABM_CHAN_DEFAULT_STATE);

	m_glob_cntl = i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_MODE_16BIT;
	m_glob_cntl |= i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT;

	m_glob_status = i82801_ac97_base::NABM_GLOB_STATUS_CAP_20BIT_SAMP;
	m_glob_status |= i82801_ac97_base::NABM_GLOB_STATUS_CAP_6CHAN;
	m_glob_status |= i82801_ac97_base::NABM_GLOB_STATUS_CAP_4CHAN;

	m_sdata_in = 0x00;
}

void i82801_ac97_base::nabm_chan_reset(offs_t offset)
{
	m_chan[offset] = i82801_ac97_base::NABM_CHAN_DEFAULT_STATE;
}

uint32_t i82801_ac97_base::glob_cntl_r()
{

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

	return m_glob_cntl;
}

void i82801_ac97_base::glob_cntl_w(uint32_t data)
{

	if(data & i82801_ac97_base::NABM_GLOB_CNTL_WARM_RESET)
		data &= (~i82801_ac97_base::NABM_GLOB_CNTL_WARM_RESET);

	if(data & i82801_ac97_base::NABM_GLOB_CNTL_COLD_RESET)
	{
		i82801_ac97_base::nabm_reset();

		data &= (~i82801_ac97_base::NABM_GLOB_CNTL_COLD_RESET);
	}

	m_glob_cntl = data;

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
}

uint32_t i82801_ac97_base::glob_status_r()
{
	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

	return m_glob_status;
}

void i82801_ac97_base::glob_status_w(uint32_t data)
{

	uint32_t wcmask = data & i82801_ac97_base::NABM_GLOB_STATUS_WCMASK;

	if(wcmask != 0x00000000)
		i82801_ac97_base::set_nabm_glob_irq(wcmask, CLEAR_LINE);

	m_glob_status = (m_glob_status & (~i82801_ac97_base::NABM_GLOB_STATUS_WMASK)) | (data & i82801_ac97_base::NABM_GLOB_STATUS_WMASK);

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
}

uint8_t i82801_ac97_base::cas_r()
{
	uint32_t cas = m_cas;

	// Reading this register sets the CAS bit
	m_cas |= i82801_ac97_base::NABM_CAS_SET;

	return cas;
}

void i82801_ac97_base::cas_w(uint8_t data)
{
	m_cas = data;
}

uint8_t i82801_ac97_base::sdata_in_r()
{
	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);

	return m_sdata_in;
}

void i82801_ac97_base::sdata_in_w(uint8_t data)
{
	m_sdata_in = (m_sdata_in & (~i82801_ac97_base::NABM_SDIN_WMASK)) | (data & i82801_ac97_base::NABM_SDIN_WMASK);

	m_cas &= (~i82801_ac97_base::NABM_CAS_SET);
}

//////////////
//////////////
//////////////

uint32_t i82801_ac97_base::nabm_chan_bdbar_r(offs_t offset)
{
	return m_chan[offset].bdesc_start;
}

void i82801_ac97_base::nabm_chan_bdbar_w(offs_t offset, uint32_t data)
{
	m_chan[offset].bdesc_start = (data & i82801_ac97_base::NABM_CHAN_BBDBAR_WMASK);
}

uint8_t i82801_ac97_base::nabm_chan_civ_r(offs_t offset)
{
	return m_chan[offset].bdesc_cidx;
}

uint8_t i82801_ac97_base::nabm_chan_lvi_r(offs_t offset)
{
	return m_chan[offset].bdesc_lidx;
}

void i82801_ac97_base::nabm_chan_lvi_w(offs_t offset, uint8_t data)
{
	m_chan[offset].bdesc_lidx = (data & i82801_ac97_base::NABM_CHAN_IDX_MASK);
}

uint16_t i82801_ac97_base::nabm_chan_sr_r(offs_t offset)
{
	return m_chan[offset].tx_status;
}

void i82801_ac97_base::nabm_chan_sr_w(offs_t offset, uint16_t data)
{

	uint16_t wcmask = data & i82801_ac97_base::NABM_CHAN_STATUS_WCMASK;
	if(wcmask != 0x0000)
		i82801_ac97_base::set_nabm_chan_irq(offset, wcmask, CLEAR_LINE);
}

uint16_t i82801_ac97_base::nabm_chan_picb_r(offs_t offset)
{
	return m_chan[offset].tx_cleft;
}

uint8_t i82801_ac97_base::nabm_chan_piv_r(offs_t offset)
{
	return m_chan[offset].bdesc_nidx;
}

uint8_t i82801_ac97_base::nabm_chan_cr_r(offs_t offset)
{
	return m_chan[offset].tx_cntl;
}

void i82801_ac97_base::nabm_chan_cr_w(offs_t offset, uint8_t data)
{
	bool tx_mode_switch = ((m_chan[offset].tx_cntl ^ data) & i82801_ac97_base::NABM_CHAN_CNTL_TRANSFER_EN);


	if(data & i82801_ac97_base::NABM_CHAN_CNTL_RESET)
	{
		uint8_t tx_cntl_ints = m_chan[offset].tx_cntl & i82801_ac97_base::NABM_CHAN_CNTL_INT_MASK;

		i82801_ac97_base::nabm_chan_reset(offset);

		m_chan[offset].tx_cntl = tx_cntl_ints;
	}
	else
	{
		if(tx_mode_switch)
		{
			if(data & i82801_ac97_base::NABM_CHAN_CNTL_TRANSFER_EN)
			{
				if(!m_chan[offset].bdesc_valid)
					i82801_ac97_base::chan_transfer_fetch(offset);

				m_chan[offset].tx_status &= (~i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED);
			}
			else
			{
				m_chan[offset].tx_status |= i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED;
			}
		}

		m_chan[offset].tx_cntl = data;
	}
}

i82801_ac97_base::chan_dir_t i82801_ac97_base::chan_transfer_direction(offs_t offset)
{
	return m_chan_direction[offset];
}

bool i82801_ac97_base::chan_transfer_enabled(offs_t offset)
{
	return m_chan[offset].bdesc_valid && (m_chan[offset].tx_cntl & i82801_ac97_base::NABM_CHAN_CNTL_TRANSFER_EN);
}

void i82801_ac97_base::chan_transfer_fetch(offs_t offset)
{
	address_space* dma_space = get_pci_busmaster_space();

	m_chan[offset].tx_status &= (~i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED);

	m_chan[offset].bdesc_cidx = m_chan[offset].bdesc_nidx & i82801_ac97_base::NABM_CHAN_IDX_MASK;

	m_chan[offset].bdesc_nidx = (m_chan[offset].bdesc_nidx + 1) & i82801_ac97_base::NABM_CHAN_IDX_MASK;

	uint32_t cur_bdbase = m_chan[offset].bdesc_start + (m_chan[offset].bdesc_cidx * i82801_ac97_base::BDESC_SIZE);

	uint32_t tx_start_addr = dma_space->read_dword(cur_bdbase + i82801_ac97_base::BDESC_ADDR_OFFSET);
	uint16_t bdesc_cmd = dma_space->read_word(cur_bdbase + i82801_ac97_base::BDESC_CMD_OFFSET);
	uint16_t tx_csize = dma_space->read_word(cur_bdbase + i82801_ac97_base::BDESC_SIZE_OFFSET);

	m_chan[offset].tx_caddr = tx_start_addr;
	m_chan[offset].tx_cleft = tx_csize;
	m_chan[offset].bdesc_cmd = bdesc_cmd;

	m_chan[offset].bdesc_valid = true;

}

void i82801_ac97_base::chan_transfer_end(offs_t offset)
{
	uint16_t int_mask = 0x0000;


	if(m_chan[offset].bdesc_cidx == m_chan[offset].bdesc_lidx)
	{
		int_mask |= i82801_ac97_base::NABM_CHAN_STATUS_LAST_BUFF_INT;

		m_chan[offset].tx_status |= i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED | i82801_ac97_base::NABM_CHAN_STATUS_EOL;

		if(m_chan[offset].bdesc_cmd & i82801_ac97_base::BDESC_CMD_USE_BUP)
			m_chan[offset].tx_sample = 0x00000000;
	}
	else
	{
		i82801_ac97_base::chan_transfer_fetch(offset);
	}

	if(m_chan[offset].bdesc_cmd & i82801_ac97_base::BDESC_CMD_SET_IOC)
		int_mask |= i82801_ac97_base::NABM_CHAN_STATUS_IOC_INT;

	if(m_chan[offset].tx_status & i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED)
		m_chan[offset].tx_cntl &= (~i82801_ac97_base::NABM_CHAN_CNTL_TRANSFER_EN);

	if(int_mask != 0x0000)
		i82801_ac97_base::set_nabm_chan_irq(offset, int_mask, ASSERT_LINE);
}

uint32_t i82801_ac97_base::chan_sample_read32(offs_t offset)
{
	if(i82801_ac97_base::chan_transfer_enabled(offset))
	{
		address_space* dma_space = get_pci_busmaster_space();

		uint32_t samples_in_read = 2;

		m_chan[offset].tx_sample = dma_space->read_dword(m_chan[offset].tx_caddr);

		m_chan[offset].tx_caddr += (i82801_ac97_base::SAMPLE_16BIT_SIZE * samples_in_read);
		m_chan[offset].tx_cleft -= samples_in_read;

		if(m_chan[offset].tx_cleft <= 0)
			i82801_ac97_base::chan_transfer_end(offset);

	}

	return m_chan[offset].tx_sample;
}

void i82801_ac97_base::chan_sample_write32(offs_t offset, uint32_t sample)
{
	m_chan[offset].tx_sample = sample;

	if(i82801_ac97_base::chan_transfer_enabled(offset))
	{
		address_space* dma_space = get_pci_busmaster_space();

		uint32_t samples_in_read = 2;


		dma_space->write_dword(m_chan[offset].tx_caddr, m_chan[offset].tx_sample);

		m_chan[offset].tx_caddr += (i82801_ac97_base::SAMPLE_16BIT_SIZE * samples_in_read);
		m_chan[offset].tx_cleft -= samples_in_read;

		if(m_chan[offset].tx_cleft <= 0)
			i82801_ac97_base::chan_transfer_end(offset);

	}
}

void i82801_ac97_base::set_nabm_chan_irq(offs_t offset, uint16_t mask, int state)
{
	bool irq_enabled = false;

	if(mask & i82801_ac97_base::NABM_CHAN_STATUS_FIFO_ERR_INT)
		irq_enabled |= (m_chan[offset].tx_cntl & NABM_CHAN_CNTL_FIFO_ERR_INT_EN);
	if(mask & i82801_ac97_base::NABM_CHAN_STATUS_IOC_INT)
		irq_enabled |= (m_chan[offset].tx_cntl & NABM_CHAN_CNTL_IOC_INT_EN);
	if(mask & i82801_ac97_base::NABM_CHAN_STATUS_LAST_BUFF_INT)
		irq_enabled |= (m_chan[offset].tx_cntl & NABM_CHAN_CNTL_LAST_BUFF_INT_EN);

	if (state)
	{
		m_chan[offset].tx_status |= mask;
	}
	else
	{
		m_chan[offset].tx_status &= (~mask);

		uint16_t current_int_stat = (m_chan[offset].tx_status & i82801_ac97_base::NABM_CHAN_STATUS_WCMASK);

		// Don't clear global int if there's still uncleared channel ints

		if(current_int_stat & i82801_ac97_base::NABM_CHAN_STATUS_FIFO_ERR_INT)
			irq_enabled &= !(m_chan[offset].tx_cntl & NABM_CHAN_CNTL_FIFO_ERR_INT_EN);
		if(current_int_stat & i82801_ac97_base::NABM_CHAN_STATUS_IOC_INT)
			irq_enabled &= !(m_chan[offset].tx_cntl & NABM_CHAN_CNTL_IOC_INT_EN);
		if(current_int_stat & i82801_ac97_base::NABM_CHAN_STATUS_LAST_BUFF_INT)
			irq_enabled &= !(m_chan[offset].tx_cntl & NABM_CHAN_CNTL_LAST_BUFF_INT_EN);
	}


	if(irq_enabled)
		i82801_ac97_base::set_nabm_glob_irq(m_chan_glob_int_mask[offset], state);
}

void i82801_ac97_base::set_nabm_glob_irq(uint32_t mask, int state)
{
	if (state)
		m_glob_status |= mask;
	else
		m_glob_status &= (~mask);

	bool irq_enabled = false;

	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_SDIN2_INT)
		irq_enabled |= (m_glob_cntl & NABM_GLOB_CNTL_CODEC2_RSM_INT);
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_SPDIF_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_PCM_IN2_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_MIC_IN2_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_SDIN1_INT)
		irq_enabled |= (m_glob_cntl & NABM_GLOB_CNTL_CODEC1_RSM_INT);
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_SDIN0_INT)
		irq_enabled |= (m_glob_cntl & NABM_GLOB_CNTL_CODEC0_RSM_INT);
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_MIC_IN1_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_PCM_OUT_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_PCM_IN1_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_MDM_OUT_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_MDM_IN_INT)
		irq_enabled |= true;
	if(mask & i82801_ac97_base::NABM_GLOB_STATUS_GSCI_INT)
		irq_enabled |= true;

	if (irq_enabled)
		m_pirq_cb(m_pirq_pin, state);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

i82801_ac97_device::i82801_ac97_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func, uint32_t main_id, uint32_t revision)
	: i82801_ac97_base(mconfig, I82801_AC97, tag, owner, clock, setup_codecs_func, main_id, revision)
{
	intr_line = 0x00;
	intr_pin = i82801_lpc_device::INT_PIN_B;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_B;

	m_chan_enabled_count = i82801_ac97_base::NABM_CHAN_COUNT;

	m_pcm1_in_offset = 0;
	m_pcm_out_offset = 1;
	m_mic1_in_offset = 2;
	m_mic2_in_offset = 3;
	m_pcm2_in_offset = 4;
	m_spidf_in_offset = 5;


	m_chan_offsets = {
		0x00000000, // PCM in 1
		0x00000010, // PCM out
		0x00000020, // Mic in 1
		// Global codec regs 0x2c-0x3f
		0x00000040, // Mic in 2
		0x00000050, // PCM in 2
		0x00000060  // SPIDF out
	};

	m_chan_direction = {
		chan_dir_t::NABM_CHAN_DIRECTION_IN,
		chan_dir_t::NABM_CHAN_DIRECTION_OUT,
		chan_dir_t::NABM_CHAN_DIRECTION_IN,
		chan_dir_t::NABM_CHAN_DIRECTION_IN,
		chan_dir_t::NABM_CHAN_DIRECTION_IN,
		chan_dir_t::NABM_CHAN_DIRECTION_OUT
	};

	m_chan_glob_int_mask = {
		i82801_ac97_base::NABM_GLOB_STATUS_PCM_IN1_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_PCM_OUT_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_MIC_IN1_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_MIC_IN2_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_PCM_IN2_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_SPDIF_INT
	};
}

i82801_mc97_device::i82801_mc97_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func, uint32_t main_id, uint32_t revision)
	: i82801_ac97_base(mconfig, I82801_MC97, tag, owner, clock, setup_codecs_func, main_id, revision)
{
	intr_line = 0x00;
	intr_pin = i82801_lpc_device::INT_PIN_B;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_B;

	m_chan_enabled_count = 2;
	
	m_modem_in_offset = 0;
	m_modem_out_offset = 1;

	m_chan_offsets = {
		0x0000, // Modem in
		0x0010, // modem out
		// Global codec regs 0x2c-0x3f
		i82801_ac97_base::NABM_CHAN_BLANK_OFFSET,
		i82801_ac97_base::NABM_CHAN_BLANK_OFFSET,
		i82801_ac97_base::NABM_CHAN_BLANK_OFFSET
	};

	m_chan_direction = {
		chan_dir_t::NABM_CHAN_DIRECTION_IN,
		chan_dir_t::NABM_CHAN_DIRECTION_OUT,
		chan_dir_t::NABM_CHAN_DIRECTION_NONE,
		chan_dir_t::NABM_CHAN_DIRECTION_NONE,
		chan_dir_t::NABM_CHAN_DIRECTION_NONE,
		chan_dir_t::NABM_CHAN_DIRECTION_NONE
	};

	m_chan_glob_int_mask = {
		i82801_ac97_base::NABM_GLOB_STATUS_MDM_IN_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_MDM_OUT_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_BLANK_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_BLANK_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_BLANK_INT,
		i82801_ac97_base::NABM_GLOB_STATUS_BLANK_INT
	};
}
