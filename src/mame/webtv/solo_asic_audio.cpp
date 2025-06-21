#include "emu.h"
#include "solo_asic_audio.h"

DEFINE_DEVICE_TYPE(SOLO_ASIC_AUDIO, solo_asic_audio_device, "solo_asic_audio_device", "WebTV SOLO AUDIO (aud, div)")

solo_asic_audio_device::solo_asic_audio_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SOLO_ASIC_AUDIO, tag, owner, clock),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_hostram(*this, finder_base::DUMMY_TAG),
	m_dac(*this, "dac%u", 0),
	m_lspeaker(*this, "lspeaker"),
	m_rspeaker(*this, "rspeaker"),
	m_int_enable_cb(*this),
	m_int_irq_cb(*this)
{
}

void solo_asic_audio_device::device_start()
{
	dac_update_timer = timer_alloc(FUNC(solo_asic_audio_device::dac_update), this);

	solo_asic_audio_device::device_reset();

	save_item(NAME(m_busaud_intenable));
	save_item(NAME(m_busaud_intstat));

	save_item(NAME(m_aud_cstart));
	save_item(NAME(m_aud_csize));
	save_item(NAME(m_aud_cconfig));
	save_item(NAME(m_aud_ccnt));
	save_item(NAME(m_aud_nstart));
	save_item(NAME(m_aud_nsize));
	save_item(NAME(m_aud_nconfig));
	save_item(NAME(m_aud_dmacntl));

	save_item(NAME(m_div_audcntl));
	save_item(NAME(m_div_cstart));
	save_item(NAME(m_div_csize));
	save_item(NAME(m_div_nstart));
	save_item(NAME(m_div_nsize));

	save_item(NAME(m_spdif_unknown000));
	save_item(NAME(m_spdif_unknown00c));
	save_item(NAME(m_spdif_unknown010));
	save_item(NAME(m_spdif_unknown014));
	save_item(NAME(m_spdif_unknown018));
	save_item(NAME(m_spdif_unknown01c));
	save_item(NAME(m_spdif_unknown020));
	save_item(NAME(m_spdif_unknown040));
	save_item(NAME(m_spdif_unknown044));
}

void solo_asic_audio_device::device_reset()
{
	dac_update_timer->adjust(attotime::from_hz(AUD_DEFAULT_CLK), 0, attotime::from_hz(AUD_DEFAULT_CLK));

	m_busaud_intenable = 0x0;
	m_busaud_intstat = 0x0;

	m_aud_cstart = 0x0;
	m_aud_csize = 0x0;
	m_aud_cend = 0x0;
	m_aud_cconfig = 0x0;
	m_aud_ccnt = 0x0;
	m_aud_nstart = 0x80000000;
	m_aud_nsize = 0x0;
	m_aud_nconfig = 0x0;
	m_aud_dmacntl = 0x0;
	m_aud_dma_ongoing = false;

	m_div_audcntl = 0x0;
	m_div_cstart = 0x0;
	m_div_csize = 0x0;
	m_div_nstart = 0x0;
	m_div_nsize = 0x0;

	m_spdif_unknown000 = 0x0;
	m_spdif_unknown00c = 0x0;
	m_spdif_unknown010 = 0x0;
	m_spdif_unknown014 = 0x0;
	m_spdif_unknown018 = 0x0;
	m_spdif_unknown01c = 0x0;
	m_spdif_unknown020 = 0x0;
	m_spdif_unknown040 = 0x0;
	m_spdif_unknown044 = 0x0;
}

void solo_asic_audio_device::device_stop()
{
	//
}

void solo_asic_audio_device::device_add_mconfig(machine_config &config)
{
	SPEAKER(config, m_lspeaker).front_left();
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[0], 0).add_route(0, m_lspeaker, 0.0);

	SPEAKER(config, m_rspeaker).front_right();
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[1], 0).add_route(0, m_rspeaker, 0.0);
}

void solo_asic_audio_device::map(address_map &map)
{
	map(0x2000, 0x2fff).m(FUNC(solo_asic_audio_device::aud_unit_map));
	map(0x8000, 0x8fff).m(FUNC(solo_asic_audio_device::div_unit_map));
	map(0xe000, 0xefff).m(FUNC(solo_asic_audio_device::spdif_unit_map));
}

void solo_asic_audio_device::aud_unit_map(address_map &map)
{
	map(0x000, 0x003).r(FUNC(solo_asic_audio_device::reg_2000_r));                                            // AUD_OCSTART
	map(0x004, 0x007).r(FUNC(solo_asic_audio_device::reg_2004_r));                                            // AUD_OCSIZE
	map(0x008, 0x00b).r(FUNC(solo_asic_audio_device::reg_2008_r));                                            // AUD_OCCONFIG
	map(0x00c, 0x00f).r(FUNC(solo_asic_audio_device::reg_200c_r));                                            // AUD_OCCNT
	map(0x010, 0x013).rw(FUNC(solo_asic_audio_device::reg_2010_r), FUNC(solo_asic_audio_device::reg_2010_w)); // AUD_ONSTART
	map(0x014, 0x017).rw(FUNC(solo_asic_audio_device::reg_2014_r), FUNC(solo_asic_audio_device::reg_2014_w)); // AUD_ONSIZE
	map(0x018, 0x01b).rw(FUNC(solo_asic_audio_device::reg_2018_r), FUNC(solo_asic_audio_device::reg_2018_w)); // AUD_ONCONFIG
	map(0x01c, 0x01f).rw(FUNC(solo_asic_audio_device::reg_201c_r), FUNC(solo_asic_audio_device::reg_201c_w)); // AUD_ODMACNTL
}

void solo_asic_audio_device::div_unit_map(address_map &map)
{
	map(0x040, 0x043).rw(FUNC(solo_asic_audio_device::reg_8040_r), FUNC(solo_asic_audio_device::reg_8040_w)); // DIV_AUDCNTL
	map(0x044, 0x047).rw(FUNC(solo_asic_audio_device::reg_8044_r), FUNC(solo_asic_audio_device::reg_8044_w)); // DIV_NEXTAUDADDR
	map(0x048, 0x04b).rw(FUNC(solo_asic_audio_device::reg_8048_r), FUNC(solo_asic_audio_device::reg_8048_w)); // DIV_NEXTAUDLEN
	map(0x04c, 0x04f).rw(FUNC(solo_asic_audio_device::reg_804c_r), FUNC(solo_asic_audio_device::reg_804c_w)); // DIV_CURAUDADDR
	map(0x050, 0x053).rw(FUNC(solo_asic_audio_device::reg_8050_r), FUNC(solo_asic_audio_device::reg_8050_w)); // DIV_CURAUDLEN
}

void solo_asic_audio_device::spdif_unit_map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(solo_asic_audio_device::reg_e000_r), FUNC(solo_asic_audio_device::reg_e000_w)); // SPDIF_?
	map(0x00c, 0x00f).rw(FUNC(solo_asic_audio_device::reg_e00c_r), FUNC(solo_asic_audio_device::reg_e00c_w)); // SPDIF_?
	map(0x010, 0x013).rw(FUNC(solo_asic_audio_device::reg_e010_r), FUNC(solo_asic_audio_device::reg_e010_w)); // SPDIF_? appears to be an ONSTART
	map(0x014, 0x017).rw(FUNC(solo_asic_audio_device::reg_e014_r), FUNC(solo_asic_audio_device::reg_e014_w)); // SPDIF_? appears to be an ONSIZE
	map(0x018, 0x01b).rw(FUNC(solo_asic_audio_device::reg_e018_r), FUNC(solo_asic_audio_device::reg_e018_w)); // SPDIF_? ONCONFIG?
	map(0x01c, 0x01f).rw(FUNC(solo_asic_audio_device::reg_e01c_r), FUNC(solo_asic_audio_device::reg_e01c_w)); // SPDIF_? appears to be an ODMACNTL
	map(0x020, 0x023).rw(FUNC(solo_asic_audio_device::reg_e020_r), FUNC(solo_asic_audio_device::reg_e020_w)); // SPDIF_?
	map(0x040, 0x043).rw(FUNC(solo_asic_audio_device::reg_e040_r), FUNC(solo_asic_audio_device::reg_e040_w)); // SPDIF_?
	map(0x044, 0x047).rw(FUNC(solo_asic_audio_device::reg_e044_r), FUNC(solo_asic_audio_device::reg_e044_w)); // SPDIF_?
}

uint32_t solo_asic_audio_device::busaud_intenable_get()
{
	return m_busaud_intenable;
}

void solo_asic_audio_device::busaud_intenable_set(uint32_t data)
{
	m_busaud_intenable |= data;
	if (m_busaud_intenable != 0x0)
	{
		m_int_enable_cb(1);
	}
}

void solo_asic_audio_device::busaud_intenable_clear(uint32_t data)
{
	m_busaud_intenable &= (~data);
}

uint32_t solo_asic_audio_device::busaud_intstat_get()
{
	return m_busaud_intstat;
}

void solo_asic_audio_device::busaud_intstat_set(uint32_t data)
{
	m_busaud_intstat |= data;
}

void solo_asic_audio_device::busaud_intstat_clear(uint32_t data)
{
	solo_asic_audio_device::set_audio_irq(data, CLEAR_LINE);
}

void solo_asic_audio_device::set_aout_clock(uint32_t clock)
{
	dac_update_timer->adjust(attotime::from_hz(clock), 0, attotime::from_hz(clock));
}

// audUnit

uint32_t solo_asic_audio_device::reg_2000_r()
{
	return m_aud_cstart;
}

uint32_t solo_asic_audio_device::reg_2004_r()
{
	return m_aud_csize;
}

uint32_t solo_asic_audio_device::reg_2008_r()
{
	return m_aud_cconfig;
}

uint32_t solo_asic_audio_device::reg_200c_r()
{
	return m_aud_ccnt;
}

uint32_t solo_asic_audio_device::reg_2010_r()
{
	return m_aud_nstart;
}

void solo_asic_audio_device::reg_2010_w(uint32_t data)
{
	m_aud_nstart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_2014_r()
{
	return m_aud_nsize;
}

void solo_asic_audio_device::reg_2014_w(uint32_t data)
{
	m_aud_nsize = data;
}

uint32_t solo_asic_audio_device::reg_2018_r()
{
	return m_aud_nconfig;
}

void solo_asic_audio_device::reg_2018_w(uint32_t data)
{
	m_aud_nconfig = data;
}

uint32_t solo_asic_audio_device::reg_201c_r()
{
	return m_aud_dmacntl;
}

void solo_asic_audio_device::reg_201c_w(uint32_t data)
{
	if ((m_aud_dmacntl ^ data) & AUD_DMACNTL_DMAEN)
	{
		if (data & AUD_DMACNTL_DMAEN)
		{
			m_lspeaker->set_input_gain(0, AUD_OUTPUT_GAIN);
			m_rspeaker->set_input_gain(0, AUD_OUTPUT_GAIN);
		}
		else
		{
			m_lspeaker->set_input_gain(0, 0.0);
			m_rspeaker->set_input_gain(0, 0.0);
		}
	}

	m_aud_dmacntl = data;
}

// divUnit

uint32_t solo_asic_audio_device::reg_8040_r()
{
	return m_div_audcntl;
}

void solo_asic_audio_device::reg_8040_w(uint32_t data)
{
	m_div_audcntl = data;
}

uint32_t solo_asic_audio_device::reg_8044_r()
{
	return m_div_cstart;
}

void solo_asic_audio_device::reg_8044_w(uint32_t data)
{
	m_div_cstart = data;
}

uint32_t solo_asic_audio_device::reg_8048_r()
{
	return m_div_csize;
}

void solo_asic_audio_device::reg_8048_w(uint32_t data)
{
	m_div_csize = data;
}

uint32_t solo_asic_audio_device::reg_804c_r()
{
	return m_div_nstart;
}

void solo_asic_audio_device::reg_804c_w(uint32_t data)
{
	m_div_nstart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_8050_r()
{
	return m_div_nsize;
}

void solo_asic_audio_device::reg_8050_w(uint32_t data)
{
	m_div_nsize = data;
}

// spdifUnit

uint32_t solo_asic_audio_device::reg_e000_r()
{
	return m_spdif_unknown000;
}

void solo_asic_audio_device::reg_e000_w(uint32_t data)
{
	m_spdif_unknown000 = data;
}

uint32_t solo_asic_audio_device::reg_e00c_r()
{
	return m_spdif_unknown00c;
}

void solo_asic_audio_device::reg_e00c_w(uint32_t data)
{
	m_spdif_unknown00c = data;
}

uint32_t solo_asic_audio_device::reg_e010_r()
{
	return m_spdif_unknown010;
}

void solo_asic_audio_device::reg_e010_w(uint32_t data)
{
	m_spdif_unknown010 = data;
}

uint32_t solo_asic_audio_device::reg_e014_r()
{
	return m_spdif_unknown014;
}

void solo_asic_audio_device::reg_e014_w(uint32_t data)
{
	m_spdif_unknown014 = data;
}

uint32_t solo_asic_audio_device::reg_e018_r()
{
	return m_spdif_unknown018;
}

void solo_asic_audio_device::reg_e018_w(uint32_t data)
{
	m_spdif_unknown018 = data;
}

uint32_t solo_asic_audio_device::reg_e01c_r()
{
	return m_spdif_unknown01c;
}

void solo_asic_audio_device::reg_e01c_w(uint32_t data)
{
	m_spdif_unknown01c = data;
}

uint32_t solo_asic_audio_device::reg_e020_r()
{
	return m_spdif_unknown020;
}

void solo_asic_audio_device::reg_e020_w(uint32_t data)
{
	m_spdif_unknown020 = data;
}

uint32_t solo_asic_audio_device::reg_e040_r()
{
	return m_spdif_unknown040;
}

void solo_asic_audio_device::reg_e040_w(uint32_t data)
{
	m_spdif_unknown040 = data;
}

uint32_t solo_asic_audio_device::reg_e044_r()
{
	return m_spdif_unknown044;
}

void solo_asic_audio_device::reg_e044_w(uint32_t data)
{
	m_spdif_unknown044 = data;
}

TIMER_CALLBACK_MEMBER(solo_asic_audio_device::dac_update)
{
	if (m_aud_dmacntl & AUD_DMACNTL_DMAEN)
	{
		if (m_aud_dma_ongoing && m_aud_ccnt != 0x80000000)
		{
			// For 8-bit we're assuming left-aligned samples
			switch(m_aud_cconfig)
			{
				case AUD_CONFIG_16BIT_STEREO:
				default:
					m_dac[0]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x10) & 0xffff);
					m_dac[1]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x00) & 0xffff);
					break;

				case AUD_CONFIG_16BIT_MONO:
					m_dac[0]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x10) & 0xffff);
					m_dac[1]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x10) & 0xffff);
					break;

				case AUD_CONFIG_8BIT_STEREO:
					m_dac[0]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x18) & 0x00ff);
					m_dac[1]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x08) & 0x00ff);
					break;

				case AUD_CONFIG_8BIT_MONO:
					m_dac[0]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x18) & 0x00ff);
					m_dac[1]->write((m_hostram[m_aud_ccnt >> 0x02] >> 0x18) & 0x00ff);
					break;
			}

			m_aud_ccnt += 4;

			if (m_aud_ccnt >= m_aud_cend)
			{
				solo_asic_audio_device::set_audio_irq(BUS_INT_AUD_AUDDMAOUT, ASSERT_LINE);
				m_aud_dma_ongoing = false; // nothing more to DMA
			}
		}
		else
		{
			m_aud_dma_ongoing = false;
		}
		if (!m_aud_dma_ongoing)
		{
			// wait for next DMA values to be marked as valid
			m_aud_dma_ongoing = m_aud_dmacntl & (AUD_DMACNTL_NV | AUD_DMACNTL_NVF);
			if (!m_aud_dma_ongoing) return; // values aren't marked as valid; don't prepare for next DMA
			m_aud_cstart = m_aud_nstart;
			m_aud_csize = m_aud_nsize;
			m_aud_cend = (m_aud_cstart + m_aud_csize);
			m_aud_cconfig = m_aud_nconfig;
			m_aud_ccnt = m_aud_cstart;
		}
	}
}

void solo_asic_audio_device::set_audio_irq(uint32_t mask, int state)
{
	if (m_busaud_intenable & mask)
	{
		if (state)
		{
			m_busaud_intstat |= mask;

			m_int_irq_cb(state);
		}
		else
		{
			m_busaud_intstat &= (~mask);

			if(m_busaud_intstat == 0x00)
				m_int_irq_cb(state);
		}
	}
}
