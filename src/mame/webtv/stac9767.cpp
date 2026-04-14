// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

// Only 16-bit samples over PCM out is supported at the moment.

#include "emu.h"
#include "stac9767.h"

DEFINE_DEVICE_TYPE(STAC9767, stac9767_codec, "stac9767_codec", "SigmaTel STAC9767 AC97 Codec")

stac9767_codec::stac9767_codec(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	ac97_codec_device(mconfig, STAC9767, tag, owner, clock),
	device_sound_interface(mconfig, *this),
	m_lspeaker(*this, "lspeaker"),
	m_rspeaker(*this, "rspeaker")
{
	m_samples_per_block = stac9767_codec::AUD_SAMPLES_PER_BLOCK;
}

void stac9767_codec::device_start()
{
	ac97_codec_device::device_start();

	m_aud_stream = stream_alloc(stac9767_codec::AUD_IN_CHAN_COUNT, stac9767_codec::AUD_OUT_CHAN_COUNT, m_aud_clk);

	machine().sound().set_update_interval(attotime::from_hz(m_aud_clk / m_samples_per_block));
}

void stac9767_codec::device_reset()
{
	ac97_codec_device::device_reset();
}

void stac9767_codec::device_add_mconfig(machine_config &config)
{
	SPEAKER(config, m_lspeaker, 1).front_left();
	add_route(0, m_lspeaker, stac9767_codec::AUD_OUTPUT_GAIN);

	SPEAKER(config, m_rspeaker, 1).front_right();
	add_route(1, m_rspeaker, stac9767_codec::AUD_OUTPUT_GAIN);
}

void stac9767_codec::nam_reg_w(offs_t offset, uint16_t data)
{
	ac97_codec_device::nam_reg_w(offset, data);

	if(offset == ac97_codec_device::NAM_REG_MASTER_VOL)
	{
		stac9767_codec::sync_volume(offset);
	}
}

void stac9767_codec::sync_volume(offs_t offset)
{
	if(offset == ac97_codec_device::NAM_REG_MASTER_VOL)
	{
		bool master_muted = m_nam_regs[ac97_codec_device::NAM_REG_MASTER_VOL] & ac97_codec_device::SOUND_MUTED;

		if(master_muted)
		{
			m_rspeaker->set_input_gain(0, 0.0);
			m_lspeaker->set_input_gain(0, 0.0);
		}
		else
		{
			float lmaster_vol = (float)((m_nam_regs[ac97_codec_device::NAM_REG_MASTER_VOL] & ac97_codec_device::CH1_GENVOL_MASK) >> ac97_codec_device::CH1_GENVOL_SHIFT);
			float rmaster_vol = (float)((m_nam_regs[ac97_codec_device::NAM_REG_MASTER_VOL] & ac97_codec_device::CH2_GENVOL_MASK) >> ac97_codec_device::CH2_GENVOL_SHIFT);

			float lmaster_gain = std::pow(2.0, -1 * (lmaster_vol * ac97_codec_device::GENVOL_DB_INCREMENT) / 10.0);
			float rmaster_gain = std::pow(2.0, -1 * (rmaster_vol * ac97_codec_device::GENVOL_DB_INCREMENT) / 10.0);

			m_lspeaker->set_input_gain(0, lmaster_gain);
			m_rspeaker->set_input_gain(0, rmaster_gain);
		}
	}
}

void stac9767_codec::nam_reset()
{
	ac97_codec_device::nam_reset();

	m_nam_regs[ac97_codec_device::NAM_REG_VENDOR_ID1]       = 0x8384;
	m_nam_regs[ac97_codec_device::NAM_REG_VENDOR_ID2]       = 0x7666;

	m_nam_regs[ac97_codec_device::NAM_REG_CAPABILITIES]     = ac97_codec_device::CODEC_CAP_HEADPHONE_OUT;
	m_nam_regs[ac97_codec_device::NAM_REG_CAPABILITIES]    |= ac97_codec_device::CODEC_CAP_20BIT_DAC;
	m_nam_regs[ac97_codec_device::NAM_REG_CAPABILITIES]    |= ac97_codec_device::CODEC_CAP_20BIT_ADC;
	m_nam_regs[ac97_codec_device::NAM_REG_CAPABILITIES]    |= 0x001a << ac97_codec_device::CODEC_CAP_3DSTEREO_SHIFT;
	
	m_nam_regs[ac97_codec_device::NAM_REG_EX_CAPABILITIES]  = 0x0000;

	stac9767_codec::sync_volume(ac97_codec_device::NAM_REG_MASTER_VOL);
}

void stac9767_codec::sound_stream_update(sound_stream &stream)
{
	for(int i = 0; i < stream.samples(); i++)
	{
		int16_t lchannel_sample;
		int16_t rchannel_sample;
		uint32_t max_sample_value;

		max_sample_value = 0x8000;

		uint32_t sample = m_ac97->chan_sample_read32(m_ac97->m_pcm_out_offset);

		lchannel_sample = sample >> 0x00; // Slot 3
		rchannel_sample = sample >> 0x10; // Slot 4

		stream.put_int(0, i, lchannel_sample, max_sample_value);
		stream.put_int(1, i, rchannel_sample, max_sample_value);
	}
}