// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_STAC9767_H
#define MAME_WEBTV_STAC9767_H

#pragma once

#include "i82801_ac97.h"
#include "speaker.h"

class stac9767_codec : public ac97_codec_device, public device_sound_interface
{

public:

	static constexpr uint32_t AUD_IN_CHAN_COUNT     = 0;
	static constexpr uint32_t AUD_OUT_CHAN_COUNT    = 2;
	static constexpr float    AUD_OUTPUT_GAIN       = 1.0;
	static constexpr uint32_t AUD_SAMPLES_PER_BLOCK = 0x100;

	stac9767_codec(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual void nam_reg_w(offs_t offset, uint16_t data) override;

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void device_add_mconfig(machine_config &config) override;

	virtual void nam_reset() override ATTR_COLD;

	virtual void sound_stream_update(sound_stream &stream) override;

private:

	sound_stream *m_aud_stream;

	required_device<speaker_device> m_lspeaker;
	required_device<speaker_device> m_rspeaker;

	uint32_t m_samples_per_block;

	void sync_volume(offs_t offset);

};

DECLARE_DEVICE_TYPE(STAC9767, stac9767_codec)

#endif // MAME_WEBTV_STAC9767_H