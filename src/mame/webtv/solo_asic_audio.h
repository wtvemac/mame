
#ifndef MAME_MACHINE_SOLO_ASIC_AUDIO
#define MAME_MACHINE_SOLO_ASIC_AUDIO

#include "cpu/mips/mips3.h"
#include "sound/dac.h"
#include "speaker.h"

#pragma once

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

constexpr uint32_t AUD_DMACNTL_DMAEN  = 1 << 2; // audUnit DMA channel enabled
constexpr uint32_t AUD_DMACNTL_NV     = 1 << 1; // audUnit DMA next registers are valid
constexpr uint32_t AUD_DMACNTL_NVF    = 1 << 0; // audUnit DMA next registers are always valid

class solo_asic_audio_device : public device_t
{

public:

	solo_asic_audio_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	void map(address_map &map);
	void aud_unit_map(address_map &map);
	void div_unit_map(address_map &map);
	void spdif_unit_map(address_map &map);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_hostram(T &&tag) { m_hostram.set_tag(std::forward<T>(tag)); }

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

	uint32_t m_busaud_intenable;
	uint32_t m_busaud_intstat;

	uint32_t m_aud_cstart;
	uint32_t m_aud_csize;
	uint32_t m_aud_cend;
	uint32_t m_aud_cconfig;
	uint32_t m_aud_ccnt;
	bool m_aud_cvalid;
	uint32_t m_aud_nstart;
	uint32_t m_aud_nsize;
	uint32_t m_aud_nconfig;
	uint32_t m_aud_dmacntl;

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

	required_device_array<dac_word_interface, 2> m_dac;
	required_device<speaker_device> m_lspeaker;
	required_device<speaker_device> m_rspeaker;

	devcb_write_line m_int_enable_cb;
	devcb_write_line m_int_irq_cb;

	emu_timer *dac_update_timer = nullptr;
	TIMER_CALLBACK_MEMBER(dac_update);

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

#endif // MAME_MACHINE_SOLO_ASIC_AUDIO