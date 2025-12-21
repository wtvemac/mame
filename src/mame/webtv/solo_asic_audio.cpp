// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#include "emu.h"
#include "solo_asic_audio.h"

DEFINE_DEVICE_TYPE(SOLO_ASIC_AUDIO, solo_asic_audio_device, "solo_asic_audio_device", "WebTV SOLO AUDIO (aud, div)")

solo_asic_audio_device::solo_asic_audio_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, bool softmodem_enabled)
	: device_t(mconfig, SOLO_ASIC_AUDIO, tag, owner, clock),
	device_sound_interface(mconfig, *this),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_hostram(*this, finder_base::DUMMY_TAG),
	m_lspeaker(*this, "lspeaker"),
	m_rspeaker(*this, "rspeaker"),
	m_audio_in(*this, "audio_input"),
	m_softmodem(*this, finder_base::DUMMY_TAG),
	m_int_enable_cb(*this),
	m_int_irq_cb(*this)
{
	m_mod_enabled = softmodem_enabled;
}

void solo_asic_audio_device::device_start()
{
	m_aud_stream = stream_alloc(1, 2, AUD_DEFAULT_CLK);

	if (m_mod_enabled)
	{
		play_modout_timer = timer_alloc(FUNC(solo_asic_audio_device::play_modout_samples), this);
		play_modin_timer = timer_alloc(FUNC(solo_asic_audio_device::play_modin_samples), this);
	}

	solo_asic_audio_device::device_reset();

	save_item(NAME(m_busaud_intenable));
	save_item(NAME(m_busaud_intstat));

	save_item(NAME(m_aud_ocstart));
	save_item(NAME(m_aud_ocsize));
	save_item(NAME(m_aud_occonfig));
	save_item(NAME(m_aud_occnt));
	save_item(NAME(m_aud_ocvalid));
	save_item(NAME(m_aud_onstart));
	save_item(NAME(m_aud_onsize));
	save_item(NAME(m_aud_onconfig));
	save_item(NAME(m_aud_odmacntl));

	save_item(NAME(m_aud_icstart));
	save_item(NAME(m_aud_icsize));
	save_item(NAME(m_aud_iccnt));
	save_item(NAME(m_aud_icvalid));
	save_item(NAME(m_aud_instart));
	save_item(NAME(m_aud_insize));
	save_item(NAME(m_aud_idmacntl));

	save_item(NAME(m_mod_ocstart));
	save_item(NAME(m_mod_ocsize));
	save_item(NAME(m_mod_ocend));
	save_item(NAME(m_mod_occonfig));
	save_item(NAME(m_mod_occnt));
	save_item(NAME(m_mod_ocvalid));
	save_item(NAME(m_mod_onstart));
	save_item(NAME(m_mod_onsize));
	save_item(NAME(m_mod_onconfig));
	save_item(NAME(m_mod_odmacntl));
	save_item(NAME(m_mod_icstart));
	save_item(NAME(m_mod_icsize));
	save_item(NAME(m_mod_icend));
	save_item(NAME(m_mod_iccnt));
	save_item(NAME(m_mod_icvalid));
	save_item(NAME(m_mod_instart));
	save_item(NAME(m_mod_insize));
	save_item(NAME(m_mod_inconfig));
	save_item(NAME(m_mod_idmacntl));

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
	if (m_mod_enabled)
	{
		play_modout_timer->adjust(attotime::from_hz(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE), 0, attotime::from_hz(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE));
		play_modin_timer->adjust(attotime::from_hz(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE), 0, attotime::from_hz(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE));

		m_softmodem->reset();
	}

	m_busaud_intenable = 0x0;
	m_busaud_intstat = 0x0;

	m_aud_ocstart = 0x0;
	m_aud_ocsize = 0x0;
	m_aud_ocend = 0x0;
	m_aud_occonfig = 0x0;
	m_aud_occnt = 0x0;
	m_aud_ocvalid = false;
	m_aud_onstart = 0x80000000;
	m_aud_onsize = 0x0;
	m_aud_onconfig = 0x0;
	m_aud_odmacntl = 0x0;

	m_aud_icstart = 0x0;
	m_aud_icsize = 0x0;
	m_aud_icend = 0x0;
	m_aud_iccnt = 0x0;
	m_aud_icvalid = false;
	m_aud_instart = 0x80000000;
	m_aud_insize = 0x0;
	m_aud_idmacntl = 0x0;

	m_mod_ocstart = 0x0;
	m_mod_ocsize = 0x0;
	m_mod_ocend = 0x0;
	m_mod_occonfig = 0x0;
	m_mod_occnt = 0x0;
	m_mod_ocvalid = false;
	m_mod_onstart = 0x80000000;
	m_mod_onsize = 0x0;
	m_mod_onconfig = 0x0;
	m_mod_odmacntl = 0x0;
	m_mod_icstart = 0x0;
	m_mod_icsize = 0x0;
	m_mod_icend = 0x0;
	m_mod_iccnt = 0x0;
	m_mod_icvalid = false;
	m_mod_instart = 0x80000000;
	m_mod_insize = 0x0;
	m_mod_inconfig = 0x0;
	m_mod_idmacntl = 0x0;

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
	SPEAKER(config, m_lspeaker, 1).front_left();
	add_route(0, m_lspeaker, AUD_OUTPUT_GAIN);

	SPEAKER(config, m_rspeaker, 1).front_right();
	add_route(1, m_rspeaker, AUD_OUTPUT_GAIN);

	MICROPHONE(config, m_audio_in, 1).front_center();
	m_audio_in->add_route(0, *this, 1.0, 0);
}

void solo_asic_audio_device::map(address_map &map)
{
	map(0x2000, 0x2fff).m(FUNC(solo_asic_audio_device::aud_unit_map));
	map(0x8000, 0x8fff).m(FUNC(solo_asic_audio_device::div_unit_map));
	map(0xe000, 0xefff).m(FUNC(solo_asic_audio_device::spdif_unit_map));

	if (m_mod_enabled)
		map(0xb000, 0xbfff).m(FUNC(solo_asic_audio_device::mod_unit_map));
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

	map(0x020, 0x023).r(FUNC(solo_asic_audio_device::reg_2020_r));                                            // AUD_ICSTART
	map(0x024, 0x027).r(FUNC(solo_asic_audio_device::reg_2024_r));                                            // AUD_ICSIZE
	map(0x02c, 0x02f).r(FUNC(solo_asic_audio_device::reg_202c_r));                                            // AUD_ICCNT
	map(0x030, 0x033).rw(FUNC(solo_asic_audio_device::reg_2030_r), FUNC(solo_asic_audio_device::reg_2030_w)); // AUD_INSTART
	map(0x034, 0x037).rw(FUNC(solo_asic_audio_device::reg_2034_r), FUNC(solo_asic_audio_device::reg_2034_w)); // AYD_INSIZE
	map(0x03c, 0x03f).rw(FUNC(solo_asic_audio_device::reg_203c_r), FUNC(solo_asic_audio_device::reg_203c_w)); // AUD_IDMACNTL
}

void solo_asic_audio_device::div_unit_map(address_map &map)
{
	map(0x040, 0x043).rw(FUNC(solo_asic_audio_device::reg_8040_r), FUNC(solo_asic_audio_device::reg_8040_w)); // DIV_AUDCNTL
	map(0x044, 0x047).rw(FUNC(solo_asic_audio_device::reg_8044_r), FUNC(solo_asic_audio_device::reg_8044_w)); // DIV_NEXTAUDADDR
	map(0x048, 0x04b).rw(FUNC(solo_asic_audio_device::reg_8048_r), FUNC(solo_asic_audio_device::reg_8048_w)); // DIV_NEXTAUDLEN
	map(0x04c, 0x04f).rw(FUNC(solo_asic_audio_device::reg_804c_r), FUNC(solo_asic_audio_device::reg_804c_w)); // DIV_CURAUDADDR
	map(0x050, 0x053).rw(FUNC(solo_asic_audio_device::reg_8050_r), FUNC(solo_asic_audio_device::reg_8050_w)); // DIV_CURAUDLEN
}

void solo_asic_audio_device::mod_unit_map(address_map &map)
{
	map(0x000, 0x003).rw(FUNC(solo_asic_audio_device::reg_b000_r), FUNC(solo_asic_audio_device::reg_b000_w)); // MOD_OCSTART
	map(0x004, 0x007).rw(FUNC(solo_asic_audio_device::reg_b004_r), FUNC(solo_asic_audio_device::reg_b004_w)); // MOD_OCSIZE
	map(0x008, 0x00b).rw(FUNC(solo_asic_audio_device::reg_b008_r), FUNC(solo_asic_audio_device::reg_b008_w)); // MOD_OCCONFIG
	map(0x00c, 0x00f).rw(FUNC(solo_asic_audio_device::reg_b00c_r), FUNC(solo_asic_audio_device::reg_b00c_w)); // MOD_OCCNT
	map(0x010, 0x013).rw(FUNC(solo_asic_audio_device::reg_b010_r), FUNC(solo_asic_audio_device::reg_b010_w)); // MOD_ONSTART
	map(0x014, 0x017).rw(FUNC(solo_asic_audio_device::reg_b014_r), FUNC(solo_asic_audio_device::reg_b014_w)); // MOD_ONSIZE
	map(0x018, 0x01b).rw(FUNC(solo_asic_audio_device::reg_b018_r), FUNC(solo_asic_audio_device::reg_b018_w)); // MOD_ONCONFIG
	map(0x01c, 0x01f).rw(FUNC(solo_asic_audio_device::reg_b01c_r), FUNC(solo_asic_audio_device::reg_b01c_w)); // MOD_ODMACNTL
	map(0x020, 0x023).rw(FUNC(solo_asic_audio_device::reg_b020_r), FUNC(solo_asic_audio_device::reg_b020_w)); // MOD_ICSTART
	map(0x024, 0x027).rw(FUNC(solo_asic_audio_device::reg_b024_r), FUNC(solo_asic_audio_device::reg_b024_w)); // MOD_ICSIZE
	map(0x02c, 0x02f).rw(FUNC(solo_asic_audio_device::reg_b02c_r), FUNC(solo_asic_audio_device::reg_b02c_w)); // MOD_ICCNT
	map(0x030, 0x033).rw(FUNC(solo_asic_audio_device::reg_b030_r), FUNC(solo_asic_audio_device::reg_b030_w)); // MOD_INSTART
	map(0x034, 0x037).rw(FUNC(solo_asic_audio_device::reg_b034_r), FUNC(solo_asic_audio_device::reg_b034_w)); // MOD_INSIZE
	map(0x03c, 0x03f).rw(FUNC(solo_asic_audio_device::reg_b03c_r), FUNC(solo_asic_audio_device::reg_b03c_w)); // MOD_INSIZE
	map(0x040, 0x043).rw(FUNC(solo_asic_audio_device::reg_b040_r), FUNC(solo_asic_audio_device::reg_b040_w)); // MOD_IDMACNTL
	map(0x044, 0x047).rw(FUNC(solo_asic_audio_device::reg_b044_r), FUNC(solo_asic_audio_device::reg_b044_w)); // MOD_IFCNTL
	map(0x05c, 0x05f).rw(FUNC(solo_asic_audio_device::reg_b05c_r), FUNC(solo_asic_audio_device::reg_b05c_w)); // MOD_GPOCNTL
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
	m_aud_stream->set_sample_rate(clock);
	solo_asic_audio_device::adjust_audio_update_rate();
}

void solo_asic_audio_device::adjust_audio_update_rate()
{
	double sample_rate = (double)m_aud_stream->sample_rate();
	// The SOLO allows the input and output audio buffers to be different sizes.
	// But since MAME expects them to be the same, we use the largest buffer to cover both.
	// The smaller buffer would be corrupted in this case. Luckly the WebTV OS keeps the buffers
	// the same size so it works out.
	double samples_per_block = (double)(std::max(m_aud_onsize, m_aud_insize) / 4);

	if (samples_per_block > 0)
		machine().sound().set_update_interval(attotime::from_hz(sample_rate / samples_per_block));
}

// audUnit

uint32_t solo_asic_audio_device::reg_2000_r()
{
	return m_aud_ocstart;
}

uint32_t solo_asic_audio_device::reg_2004_r()
{
	return m_aud_ocsize;
}

uint32_t solo_asic_audio_device::reg_2008_r()
{
	return m_aud_occonfig;
}

uint32_t solo_asic_audio_device::reg_200c_r()
{
	return m_aud_occnt;
}

uint32_t solo_asic_audio_device::reg_2010_r()
{
	return m_aud_onstart;
}

void solo_asic_audio_device::reg_2010_w(uint32_t data)
{
	m_aud_onstart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_2014_r()
{
	return m_aud_onsize;
}

void solo_asic_audio_device::reg_2014_w(uint32_t data)
{
	m_aud_onsize = data;

	solo_asic_audio_device::adjust_audio_update_rate();
}

uint32_t solo_asic_audio_device::reg_2018_r()
{
	return m_aud_onconfig;
}

void solo_asic_audio_device::reg_2018_w(uint32_t data)
{
	m_aud_onconfig = data;
}

uint32_t solo_asic_audio_device::reg_201c_r()
{
	return m_aud_odmacntl;
}

void solo_asic_audio_device::reg_201c_w(uint32_t data)
{
	if ((m_aud_odmacntl ^ data) & AUD_DMACNTL_DMAEN)
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

	m_aud_odmacntl = data;
}

uint32_t solo_asic_audio_device::reg_2020_r()
{
	return m_aud_icstart;
}

uint32_t solo_asic_audio_device::reg_2024_r()
{
	return m_aud_icsize;
}

uint32_t solo_asic_audio_device::reg_202c_r()
{
	return m_aud_iccnt;
}

uint32_t solo_asic_audio_device::reg_2030_r()
{
	return m_aud_instart;
}

void solo_asic_audio_device::reg_2030_w(uint32_t data)
{
	m_aud_instart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_2034_r()
{
	return m_aud_insize;
}

void solo_asic_audio_device::reg_2034_w(uint32_t data)
{
	m_aud_insize = data;

	solo_asic_audio_device::adjust_audio_update_rate();
}

uint32_t solo_asic_audio_device::reg_203c_r()
{
	return m_aud_idmacntl;
}

void solo_asic_audio_device::reg_203c_w(uint32_t data)
{
	if ((m_aud_idmacntl ^ data) & AUD_DMACNTL_DMAEN)
	{
		if (data & AUD_DMACNTL_DMAEN)
		{
		}
		else
		{
		}
	}

	m_aud_idmacntl = data;
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

// modUnit

uint32_t solo_asic_audio_device::reg_b000_r()
{
	return m_mod_ocstart | 0xa0000000;
}

void solo_asic_audio_device::reg_b000_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b004_r()
{
	return m_mod_ocsize;
}

void solo_asic_audio_device::reg_b004_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b008_r()
{
	return m_mod_occonfig;
}

void solo_asic_audio_device::reg_b008_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b00c_r()
{
	return m_aud_occnt;
}

void solo_asic_audio_device::reg_b00c_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b010_r()
{
	return m_mod_onstart;
}

void solo_asic_audio_device::reg_b010_w(uint32_t data)
{
	m_mod_onstart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_b014_r()
{
	return m_mod_onsize;
}

void solo_asic_audio_device::reg_b014_w(uint32_t data)
{
	m_mod_onsize = data;
}

uint32_t solo_asic_audio_device::reg_b018_r()
{
	return m_mod_onconfig;
}

void solo_asic_audio_device::reg_b018_w(uint32_t data)
{
	m_mod_onconfig = data;
}

uint32_t solo_asic_audio_device::reg_b01c_r()
{
	return m_mod_odmacntl;
}

void solo_asic_audio_device::reg_b01c_w(uint32_t data)
{
	m_mod_odmacntl = data;
}

uint32_t solo_asic_audio_device::reg_b020_r()
{
	return m_mod_icstart | 0xa0000000;
}

void solo_asic_audio_device::reg_b020_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b024_r()
{
	return m_mod_icsize;
}

void solo_asic_audio_device::reg_b024_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b02c_r()
{
	return m_mod_iccnt;
}

void solo_asic_audio_device::reg_b02c_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b030_r()
{
	return m_mod_instart;
}

void solo_asic_audio_device::reg_b030_w(uint32_t data)
{
	m_mod_instart = data & (~0xfc000003);
}

uint32_t solo_asic_audio_device::reg_b034_r()
{
	return m_mod_insize;
}

void solo_asic_audio_device::reg_b034_w(uint32_t data)
{
	m_mod_insize = data;
}

uint32_t solo_asic_audio_device::reg_b03c_r()
{
	return m_mod_idmacntl;
}

void solo_asic_audio_device::reg_b03c_w(uint32_t data)
{
	m_mod_idmacntl = data;
}

uint32_t solo_asic_audio_device::reg_b040_r()
{
	return 0xffffffff; // look into why sending 0 causes this to be re-read multiple times.
	//return 0x00000000;
}

void solo_asic_audio_device::reg_b040_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b044_r()
{
	return 0xffffffff; // look into why sending 0 causes this to be re-read multiple times.
	//return 0x00000000;
}

void solo_asic_audio_device::reg_b044_w(uint32_t data)
{
	//
}

uint32_t solo_asic_audio_device::reg_b05c_r()
{
	return (m_mod_idmacntl << 0x04) | (m_mod_odmacntl << 0x00);
}

void solo_asic_audio_device::reg_b05c_w(uint32_t data)
{
	m_mod_idmacntl = (data >> 0x04) & 0x7;
	m_mod_odmacntl = (data >> 0x00) & 0x7;
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

TIMER_CALLBACK_MEMBER(solo_asic_audio_device::play_modout_samples)
{
	if (m_mod_odmacntl & MOD_DMACNTL_DMAEN)
	{
		if (!m_mod_ocvalid && (m_mod_odmacntl & MOD_DMACNTL_NV) && m_mod_onstart != 0x80000000)
		{
			m_mod_ocstart = m_mod_onstart;
			m_mod_ocsize = m_mod_onsize;
			m_mod_occonfig = m_mod_onconfig;

			m_mod_occnt = m_mod_ocstart;
			m_mod_ocend = (m_mod_ocstart + m_mod_ocsize);

			m_mod_ocvalid = true;

			if ((m_mod_odmacntl & MOD_DMACNTL_NVF) == 0x0)
			{
				m_mod_odmacntl &= (~MOD_DMACNTL_NV);
			}

			m_softmodem->push((int32_t*)&m_hostram[m_mod_occnt >> 0x02], (m_mod_ocsize >> 2));

			solo_asic_audio_device::set_audio_irq(BUS_INT_AUD_SMODEMOUT, ASSERT_LINE);
		}

		if (m_mod_ocvalid)
		{
#if MOD_AUDIO_DEBUG && MOD_PLAY_OUT
			// Temporarily (maybe) disabled, new channels need to be created for this.
			//m_dac[0]->write((m_hostram[m_mod_occnt >> 0x02] >> 0x00) & 0xffff);
#endif

			m_mod_occnt += 4;

			if (m_mod_occnt >= m_mod_ocend)
			{
				m_mod_ocvalid = false;
			}
		}
	}
}

TIMER_CALLBACK_MEMBER(solo_asic_audio_device::play_modin_samples)
{
	if (m_mod_idmacntl & MOD_DMACNTL_DMAEN)
	{
		if (!m_mod_icvalid && (m_mod_idmacntl & MOD_DMACNTL_NV) && m_mod_instart != 0x80000000)
		{
			m_mod_icstart = m_mod_instart;
			m_mod_icsize = m_mod_insize;

			m_mod_iccnt = m_mod_icstart;
			m_mod_icend = (m_mod_icstart + m_mod_icsize);

			m_mod_icvalid = true;

		}

		if (m_mod_icvalid)
		{
#if MOD_AUDIO_DEBUG && MOD_PLAY_IN
			// Temporarily (maybe) disabled, new channels need to be created for this.
			//m_dac[1]->write((m_hostram[m_mod_iccnt >> 0x02] >> 0x00) & 0xffff);
#endif

			m_mod_iccnt += 4;

			if (m_mod_iccnt >= m_mod_icend)
			{
				m_softmodem->pull((int32_t*)&m_hostram[m_mod_icstart >> 0x02], (m_mod_icsize >> 2));

				if ((m_mod_idmacntl & MOD_DMACNTL_NVF) == 0x0)
				{
					m_mod_idmacntl &= (~MOD_DMACNTL_NV);
				}

				solo_asic_audio_device::set_audio_irq(BUS_INT_AUD_SMODEMIN, ASSERT_LINE);

				m_mod_icvalid = false;
			}
		}
	}
}

void solo_asic_audio_device::audio_output_update(sound_stream &stream)
{
	if (m_aud_odmacntl & AUD_DMACNTL_DMAEN)
	{
		// No current buffer ready to play. Check if there's anything lined up for us.
		if (!m_aud_ocvalid && (m_aud_odmacntl & AUD_DMACNTL_NV) && m_aud_onstart != 0x80000000)
		{
			m_aud_ocstart = m_aud_onstart;
			m_aud_ocsize = m_aud_onsize;
			m_aud_occonfig = m_aud_onconfig;

			m_aud_occnt = m_aud_ocstart;
			m_aud_ocend = (m_aud_ocstart + m_aud_ocsize);

			// Next buffer loaded, so we will now play the it
			m_aud_ocvalid = true;

			// If next buffer isn't flagged as continous then invalidate the next values.
			// The OS will reload it with valid values.
			if ((m_aud_odmacntl & AUD_DMACNTL_NVF) == 0x0)
			{
				m_aud_odmacntl &= (~AUD_DMACNTL_NV);
			}

			// Ask OS to load new next values. We will play it after the current buffer finished playing.
			solo_asic_audio_device::set_audio_irq(BUS_INT_AUD_AUDDMAOUT, ASSERT_LINE);
		}

		// If the current buffer is valid (ready), then play it.
		if (m_aud_ocvalid)
		{
			for(int i = 0; i < stream.samples(); i++)
			{
				int16_t lchannel_sample;
				int16_t rchannel_sample;
				uint32_t max_sample_value;

				switch(m_aud_occonfig)
				{
					case AUD_CONFIG_16BIT_STEREO:
					default:
						lchannel_sample = m_hostram[m_aud_occnt >> 0x02] >> 0x10;
						rchannel_sample = m_hostram[m_aud_occnt >> 0x02] >> 0x00;
						max_sample_value = 0x8000;
						break;

					case AUD_CONFIG_16BIT_MONO:
						lchannel_sample = m_hostram[m_aud_occnt >> 0x02] >> 0x10;
						rchannel_sample = lchannel_sample;
						max_sample_value = 0x8000;
						break;

					// For 8-bit we're assuming left-aligned samples

					case AUD_CONFIG_8BIT_STEREO:
						lchannel_sample = (int8_t)(m_hostram[m_aud_occnt >> 0x02] >> 0x18);
						rchannel_sample = (int8_t)(m_hostram[m_aud_occnt >> 0x02] >> 0x08);
						max_sample_value = 0x80;
						break;

					case AUD_CONFIG_8BIT_MONO:
						lchannel_sample = (int8_t)(m_hostram[m_aud_occnt >> 0x02] >> 0x18);
						rchannel_sample = lchannel_sample;
						max_sample_value = 0x80;
						break;
				}

				stream.put_int(0, i, lchannel_sample, max_sample_value);
				stream.put_int(1, i, rchannel_sample, max_sample_value);

				m_aud_occnt += 4;

				if (m_aud_occnt >= m_aud_ocend)
				{
					// Invalidate current buffer and load next (valid) buffer.
					m_aud_ocvalid = false;
					break;
				}

			}
		}
	}
}

void solo_asic_audio_device::audio_input_update(sound_stream &stream)
{
	if (m_aud_idmacntl & AUD_DMACNTL_DMAEN)
	{
		// No current buffer ready for input samples. Check if there OS has anything lined up for us.
		if (!m_aud_icvalid && (m_aud_idmacntl & AUD_DMACNTL_NV) && m_aud_instart != 0x80000000)
		{
			m_aud_icstart = m_aud_instart;
			m_aud_icsize = m_aud_insize;

			m_aud_iccnt = m_aud_icstart;
			m_aud_icend = (m_aud_icstart + m_aud_icsize);

			// Next buffer ready for input samples, so we will now push it
			m_aud_icvalid = true;
		}

		// If there's a buffer ready for input samples, then copy our input samples.
		if (m_aud_icvalid)
		{
			for(int i = 0; i < stream.samples(); i++)
			{
				int16_t sample = (int16_t)(stream.get(0, i) * (float)0x8000);

				// The buffer allows for stereo input but we're only have 1 input channel at the moment
				// So we're just repeating the same value for the left and right channels.
				m_hostram[m_aud_iccnt >> 0x02] = (sample << 0x10) | (sample << 0x00);

				m_aud_iccnt += 4;

				if (m_aud_iccnt >= m_aud_icend)
				{
					// We finished copying samples, so invalidate the buffer.
					m_aud_icvalid = false;

					// If next buffer isn't flagged as continous then tell the OS we've invalidated the buffer.
					// The OS will tell us when the next buffer is ready.
					if ((m_aud_idmacntl & AUD_DMACNTL_NVF) == 0x0)
					{
						m_aud_idmacntl &= (~AUD_DMACNTL_NV);
					}

					// Ask the OS to process our input samples and to prepare a new buffer for us (if needed).
					solo_asic_audio_device::set_audio_irq(BUS_INT_AUD_AUDDMAIN, ASSERT_LINE);

					break;
				}

			}
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

void solo_asic_audio_device::sound_stream_update(sound_stream &stream)
{
	solo_asic_audio_device::audio_output_update(stream);
	solo_asic_audio_device::audio_input_update(stream);
}