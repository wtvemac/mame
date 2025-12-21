// license: BSD-3-Clause
// copyright-holders: FairPlay137, wtvemac

// Description here

#ifndef MAME_WEBTV_SOLO_ASIC_AUDIO_H
#define MAME_WEBTV_SOLO_ASIC_AUDIO_H

#include "cpu/mips/mips3.h"
#include "sound.h"
#include "speaker.h"
#include "wtvsoftmodem.h"

#pragma once

#define MOD_AUDIO_DEBUG false
#define MOD_PLAY_IN     true
#define MOD_PLAY_OUT    true

constexpr uint32_t BUS_INT_AUD_SPDIFOUT  = 1 << 7; // UltimateTV's S/PDIF out
constexpr uint32_t BUS_INT_AUD_SMODEMIN  = 1 << 6; // Soft modem DMA in
constexpr uint32_t BUS_INT_AUD_SMODEMOUT = 1 << 5; // Soft modem DMA out
constexpr uint32_t BUS_INT_AUD_DIVUNIT   = 1 << 4; // divUnit audio
constexpr uint32_t BUS_INT_AUD_AUDDMAIN  = 1 << 3; // Audio in
constexpr uint32_t BUS_INT_AUD_AUDDMAOUT = 1 << 2; // Audio out

constexpr uint32_t AUD_CONFIG_16BIT_STEREO = 0;
constexpr uint32_t AUD_CONFIG_16BIT_MONO   = 1;
constexpr uint32_t AUD_CONFIG_8BIT_STEREO  = 2;
constexpr uint32_t AUD_CONFIG_8BIT_MONO    = 3;

constexpr uint32_t AUD_DEFAULT_CLK = 44100;
constexpr float    AUD_OUTPUT_GAIN = 1.0;
constexpr float    AUD_INPUT_GAIN  = 1.0;

constexpr uint32_t MOD_DMACNTL_UTV    = 1 << 3; // Unknown why this exists but is used in UTV's Solo chip
constexpr uint32_t MOD_DMACNTL_DMAEN  = 1 << 2;
constexpr uint32_t MOD_DMACNTL_NV     = 1 << 1;
constexpr uint32_t MOD_DMACNTL_NVF    = 1 << 0;

constexpr uint32_t AUD_DMACNTL_DMAEN  = 1 << 2; // audUnit DMA channel enabled
constexpr uint32_t AUD_DMACNTL_NV     = 1 << 1; // audUnit DMA next registers are valid
constexpr uint32_t AUD_DMACNTL_NVF    = 1 << 0; // audUnit DMA next registers are always valid

class solo_asic_audio_device : public device_t, public device_sound_interface
{

public:

	solo_asic_audio_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, uint32_t aud_clock = 44100, bool softmodem_enabled = false);

	void map(address_map &map);
	void aud_unit_map(address_map &map);
	void div_unit_map(address_map &map);
	void mod_unit_map(address_map &map);
	void spdif_unit_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_hostram(T &&tag) { m_hostram.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_softmodem(T &&tag) { m_softmodem.set_tag(std::forward<T>(tag)); }

	void set_aout_clock(uint32_t clock);

	auto int_enable_callback() { return m_int_enable_cb.bind(); }
	auto int_irq_callback() { return m_int_irq_cb.bind(); }

	uint32_t busaud_intenable_get();
	void busaud_intenable_set(uint32_t data);
	void busaud_intenable_clear(uint32_t data);

	uint32_t busaud_intstat_get();
	void busaud_intstat_set(uint32_t data);
	void busaud_intstat_clear(uint32_t data);


protected:

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void sound_stream_update(sound_stream &stream) override;

	uint32_t m_aud_clock;

	uint32_t m_busaud_intenable;
	uint32_t m_busaud_intstat;

	uint32_t m_aud_ocstart;
	uint32_t m_aud_ocsize;
	uint32_t m_aud_ocend;
	uint32_t m_aud_occonfig;
	uint32_t m_aud_occnt;
	bool m_aud_ocvalid;
	uint32_t m_aud_onstart;
	uint32_t m_aud_onsize;
	uint32_t m_aud_onconfig;
	uint32_t m_aud_odmacntl;
	
	uint32_t m_aud_icstart;
	uint32_t m_aud_icsize;
	uint32_t m_aud_icend;
	uint32_t m_aud_iccnt;
	bool m_aud_icvalid;
	uint32_t m_aud_instart;
	uint32_t m_aud_insize;
	uint32_t m_aud_idmacntl;

	bool m_mod_enabled;
	uint32_t m_mod_ocstart;
	uint32_t m_mod_ocsize;
	uint32_t m_mod_ocend;
	uint32_t m_mod_occonfig;
	uint32_t m_mod_occnt;
	bool m_mod_ocvalid;
	uint32_t m_mod_onstart;
	uint32_t m_mod_onsize;
	uint32_t m_mod_onconfig;
	uint32_t m_mod_odmacntl;
	uint32_t m_mod_icstart;
	uint32_t m_mod_icsize;
	uint32_t m_mod_icend;
	uint32_t m_mod_iccnt;
	bool m_mod_icvalid;
	uint32_t m_mod_instart;
	uint32_t m_mod_insize;
	uint32_t m_mod_inconfig;
	uint32_t m_mod_idmacntl;

	uint32_t m_div_audcntl;
	uint32_t m_div_cstart;
	uint32_t m_div_csize;
	uint32_t m_div_nstart;
	uint32_t m_div_nsize;

	uint32_t m_spdif_unknown000;
	uint32_t m_spdif_unknown00c;
	uint32_t m_spdif_unknown010;
	uint32_t m_spdif_unknown014;
	uint32_t m_spdif_unknown018;
	uint32_t m_spdif_unknown01c;
	uint32_t m_spdif_unknown020;
	uint32_t m_spdif_unknown040;
	uint32_t m_spdif_unknown044;

private:

	required_device<mips3_device> m_hostcpu;
	required_shared_ptr<uint32_t> m_hostram;

	sound_stream *m_aud_stream;
	required_device<speaker_device> m_lspeaker;
	required_device<speaker_device> m_rspeaker;

	required_device<microphone_device> m_audio_in;

	optional_device<wtvsoftmodem_device> m_softmodem;

	devcb_write_line m_int_enable_cb;
	devcb_write_line m_int_irq_cb;

	emu_timer *play_modout_timer = nullptr;
	TIMER_CALLBACK_MEMBER(play_modout_samples);
	emu_timer *play_modin_timer = nullptr;
	TIMER_CALLBACK_MEMBER(play_modin_samples);

	void adjust_audio_update_rate();

	void audio_output_update(sound_stream &stream);
	void audio_input_update(sound_stream &stream);

	void set_audio_irq(uint32_t mask, int state);

	/* audUnit registers */

	uint32_t reg_2000_r();          // AUD_OCSTART (read-only)
	uint32_t reg_2004_r();          // AUD_OCSIZE (read-only)
	uint32_t reg_2008_r();          // AUD_OCCONFIG (read)
	uint32_t reg_200c_r();          // AUD_OCCNT (read-only)
	uint32_t reg_2010_r();          // AUD_ONSTART (read)
	void reg_2010_w(uint32_t data); // AUD_ONSTART (write)
	uint32_t reg_2014_r();          // AUD_ONSIZE (read)
	void reg_2014_w(uint32_t data); // AUD_ONSIZE (write)
	uint32_t reg_2018_r();          // AUD_ONCONFIG (read)
	void reg_2018_w(uint32_t data); // AUD_ONCONFIG (write)
	uint32_t reg_201c_r();          // AUD_ODMACNTL (read)
	void reg_201c_w(uint32_t data); // AUD_ODMACNTL (write)

	uint32_t reg_2020_r();          // AUD_ICSTART (read-only)
	uint32_t reg_2024_r();          // AUD_ICSIZE (read-only)
	uint32_t reg_202c_r();          // AUD_ICCNT (read-only)
	uint32_t reg_2030_r();          // AUD_INSTART (read)
	void reg_2030_w(uint32_t data); // AUD_INSTART (write)
	uint32_t reg_2034_r();          // AUD_INSIZE (read)
	void reg_2034_w(uint32_t data); // AUD_INSIZE (write)
	uint32_t reg_203c_r();          // AUD_IDMACNTL (read)
	void reg_203c_w(uint32_t data); // AUD_IDMACNTL (write)

	/* divUnit registers */

	uint32_t reg_8040_r();          // DIV_AUDCNTL (read)
	void reg_8040_w(uint32_t data); // DIV_AUDCNTL (write)
	uint32_t reg_8044_r();          // DIV_NEXTAUDADDR (read)
	void reg_8044_w(uint32_t data); // DIV_NEXTAUDADDR (write)
	uint32_t reg_8048_r();          // DIV_NEXTAUDLEN (read)
	void reg_8048_w(uint32_t data); // DIV_NEXTAUDLEN (write)
	uint32_t reg_804c_r();          // DIV_CURAUDADDR (read)
	void reg_804c_w(uint32_t data); // DIV_CURAUDADDR (write)
	uint32_t reg_8050_r();          // DIV_CURAUDLEN (read)
	void reg_8050_w(uint32_t data); // DIV_CURAUDLEN (write)

	/* modUnit */

	uint32_t reg_b000_r();
	void reg_b000_w(uint32_t data);
	uint32_t reg_b004_r();
	void reg_b004_w(uint32_t data);
	uint32_t reg_b008_r();
	void reg_b008_w(uint32_t data);
	uint32_t reg_b00c_r();
	void reg_b00c_w(uint32_t data);
	uint32_t reg_b010_r();
	void reg_b010_w(uint32_t data);
	uint32_t reg_b014_r();
	void reg_b014_w(uint32_t data);
	uint32_t reg_b018_r();
	void reg_b018_w(uint32_t data);
	uint32_t reg_b01c_r();
	void reg_b01c_w(uint32_t data);
	uint32_t reg_b020_r();
	void reg_b020_w(uint32_t data);
	uint32_t reg_b024_r();
	void reg_b024_w(uint32_t data);
	uint32_t reg_b02c_r();
	void reg_b02c_w(uint32_t data);
	uint32_t reg_b030_r();
	void reg_b030_w(uint32_t data);
	uint32_t reg_b034_r();
	void reg_b034_w(uint32_t data);
	uint32_t reg_b03c_r();
	void reg_b03c_w(uint32_t data);
	uint32_t reg_b040_r();
	void reg_b040_w(uint32_t data);
	uint32_t reg_b044_r();
	void reg_b044_w(uint32_t data);
	uint32_t reg_b05c_r();
	void reg_b05c_w(uint32_t data);

	/* spdifUnit registers */

	uint32_t reg_e000_r();          // SPDIF_? (read)
	void reg_e000_w(uint32_t data); // SPDIF_? (write)
	uint32_t reg_e00c_r();          // SPDIF_? (read)
	void reg_e00c_w(uint32_t data); // SPDIF_? (write)
	uint32_t reg_e010_r();          // SPDIF_? appears to be an ONSTART (read)
	void reg_e010_w(uint32_t data); // SPDIF_? appears to be an ONSTART (write)
	uint32_t reg_e014_r();          // SPDIF_? appears to be an ONSIZE (read)
	void reg_e014_w(uint32_t data); // SPDIF_? appears to be an ONSIZE (write)
	uint32_t reg_e018_r();          // SPDIF_? ONCONFIG? (read)
	void reg_e018_w(uint32_t data); // SPDIF_? ONCONFIG? (write)
	uint32_t reg_e01c_r();          // SPDIF_? appears to be an ODMACNTL (read)
	void reg_e01c_w(uint32_t data); // SPDIF_? appears to be an ODMACNTL (write)
	uint32_t reg_e020_r();          // SPDIF_? (read)
	void reg_e020_w(uint32_t data); // SPDIF_? (write)
	uint32_t reg_e040_r();          // SPDIF_? (read)
	void reg_e040_w(uint32_t data); // SPDIF_? (write)
	uint32_t reg_e044_r();          // SPDIF_? (read)
	void reg_e044_w(uint32_t data); // SPDIF_? (write)

};

DECLARE_DEVICE_TYPE(SOLO_ASIC_AUDIO, solo_asic_audio_device)

#endif // MAME_WEBTV_SOLO_ASIC_AUDIO_H