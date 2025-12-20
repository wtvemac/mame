// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#include "emu.h"
#include "solo_asic_audio.h"

DEFINE_DEVICE_TYPE(SOLO_ASIC_AUDIO, solo_asic_audio_device, "solo_asic_audio_device", "WebTV SOLO AUDIO (aud, div)")

solo_asic_audio_device::solo_asic_audio_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, bool softmodem_enabled)
	: device_t(mconfig, SOLO_ASIC_AUDIO, tag, owner, clock),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_hostram(*this, finder_base::DUMMY_TAG),
	m_dac(*this, "dac%u", 0),
	m_lspeaker(*this, "lspeaker"),
	m_rspeaker(*this, "rspeaker"),
	m_softmodem(*this, finder_base::DUMMY_TAG),
	m_int_enable_cb(*this),
	m_int_irq_cb(*this)
{
	m_mod_enabled = softmodem_enabled;
}

void solo_asic_audio_device::device_start()
{
	play_aout_timer = timer_alloc(FUNC(solo_asic_audio_device::play_aout_samples), this);

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
	play_aout_timer->adjust(attotime::from_hz(AUD_DEFAULT_CLK), 0, attotime::from_hz(AUD_DEFAULT_CLK));

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
	SPEAKER(config, m_lspeaker).front_left();
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[0], 0).add_route(0, m_lspeaker, 0.9);

	SPEAKER(config, m_rspeaker).front_right();
	DAC_16BIT_R2R_TWOS_COMPLEMENT(config, m_dac[1], 0).add_route(0, m_rspeaker, 0.9);
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
	play_aout_timer->adjust(attotime::from_hz(clock), 0, attotime::from_hz(clock));
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
			m_dac[0]->write((m_hostram[m_mod_occnt >> 0x02] >> 0x00) & 0xffff);
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
			m_dac[1]->write((m_hostram[m_mod_iccnt >> 0x02] >> 0x00) & 0xffff);
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

TIMER_CALLBACK_MEMBER(solo_asic_audio_device::play_aout_samples)
{
#if !MOD_AUDIO_DEBUG
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
			switch(m_aud_occonfig)
			{
				case AUD_CONFIG_16BIT_STEREO:
				default:
					m_dac[0]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x10) & 0xffff);
					m_dac[1]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x00) & 0xffff);
					break;

				case AUD_CONFIG_16BIT_MONO:
					m_dac[0]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x10) & 0xffff);
					m_dac[1]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x10) & 0xffff);
					break;

				// For 8-bit we're assuming left-aligned samples

				case AUD_CONFIG_8BIT_STEREO:
					m_dac[0]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x18) & 0x00ff);
					m_dac[1]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x08) & 0x00ff);
					break;

				case AUD_CONFIG_8BIT_MONO:
					m_dac[0]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x18) & 0x00ff);
					m_dac[1]->write((m_hostram[m_aud_occnt >> 0x02] >> 0x18) & 0x00ff);
					break;
			}

			m_aud_occnt += 4;

			if (m_aud_occnt >= m_aud_ocend)
			{
				// Invalidate current buffer and load next (valid) buffer.
				m_aud_ocvalid = false;
			}
		}
	}
#endif
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
