// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82801_AC97_H
#define MAME_WEBTV_I82801_AC97_H

#pragma once

#include "machine/pci.h"
#include "sound.h"
#include "i82801_lpc.h"

class aclink_interface : public device_interface
{

protected:

	aclink_interface(const machine_config &mconfig, device_t &device);

	class i82801_ac97_base* const m_ac97;

};

class ac97_codec_device : public device_t, public aclink_interface
{

public:

	static constexpr uint8_t NAM_REG_CNT                = 64;
	static constexpr uint16_t NAM_REGS_SIZE             = ac97_codec_device::NAM_REG_CNT * sizeof(uint16_t);
	static constexpr uint8_t NAM_REG_RESET              = 0x00 >> 1;
	static constexpr uint8_t NAM_REG_CAPABILITIES       = 0x00 >> 1;
	static constexpr uint8_t NAM_REG_MASTER_VOL         = 0x02 >> 1;
	static constexpr uint8_t NAM_REG_AUX_OUT_VOL        = 0x04 >> 1;
	static constexpr uint8_t NAM_REG_MONO_VOL           = 0x06 >> 1;
	static constexpr uint8_t NAM_REG_MASTER_TONE        = 0x08 >> 1;
	static constexpr uint8_t NAM_REG_PCBEEP_VOL         = 0x0a >> 1;
	static constexpr uint8_t NAM_REG_PHONE_VOL          = 0x0c >> 1;
	static constexpr uint8_t NAM_REG_MIC_VOL            = 0x0e >> 1;
	static constexpr uint8_t NAM_REG_LINE_IN_VOL        = 0x10 >> 1;
	static constexpr uint8_t NAM_REG_CD_VOL             = 0x12 >> 1;
	static constexpr uint8_t NAM_REG_VID_VOL            = 0x14 >> 1;
	static constexpr uint8_t NAM_REG_AUX_IN_VOL         = 0x16 >> 1;
	static constexpr uint8_t NAM_REG_PCM_OUT_VOL        = 0x18 >> 1;
	static constexpr uint8_t NAM_REG_RECORD_SEL         = 0x1a >> 1;
	static constexpr uint8_t NAM_REG_RECORD_GAIN        = 0x1c >> 1;
	static constexpr uint8_t NAM_REG_RECORD_GAIN_MIC    = 0x1e >> 1;
	static constexpr uint8_t NAM_REG_GEN_PURPOSE        = 0x20 >> 1;
	static constexpr uint8_t NAM_REG_3D_CNTL            = 0x22 >> 1;
	// AC97 Reserved: 0x24 (modem sample rate/audio interrupt and paging mechanism register)
	static constexpr uint8_t NAM_REG_PWRDOWN_CNTLSTAT   = 0x26 >> 1;
	static constexpr uint8_t NAM_REG_EX_CAPABILITIES    = 0x28 >> 1;
	static constexpr uint8_t NAM_REG_EX_CNTLSTAT        = 0x2a >> 1;
	static constexpr uint8_t NAM_REG_PCM_FRONT_DAC_RATE = 0x2c >> 1;
	static constexpr uint8_t NAM_REG_PCM_SURND_DAC_RATE = 0x2e >> 1;
	static constexpr uint8_t NAM_REG_PCM_LFE_DAC_RATE   = 0x30 >> 1;
	static constexpr uint8_t NAM_REG_PCM_LR_ADC_RATE    = 0x32 >> 1;
	static constexpr uint8_t NAM_REG_MIC_ADC_RATE       = 0x34 >> 1;
	static constexpr uint8_t NAM_REG_6CH_VOL_CLFE       = 0x36 >> 1;
	static constexpr uint8_t NAM_REG_6CH_VOL_LRSUR      = 0x38 >> 1;
	static constexpr uint8_t NAM_REG_SPDIF_CNTL         = 0x3a >> 1;
	// Intel Reserved: 0x3c–0x56 / Modem registers
	// AC92 Reserved: 0x58
	// Vencor Reserved: 0x5a
	static constexpr uint8_t NAM_REG_VENDOR_ID1         = 0x7c >> 1;
	static constexpr uint8_t NAM_REG_VENDOR_ID2         = 0x7e >> 1;

	static constexpr uint16_t SOUND_MUTED      = 0x8000;
	// General volume values, used to help interpret most of the volume registers
	static constexpr float GENVOL_DB_INCREMENT = 1.5;
	static constexpr uint16_t GENVOL_MAX       = 0x1f; // 46.5dB max (incremented by 1.5dB)
	// Commonly the left channel volume
	static constexpr uint16_t CH1_GENVOL_SHIFT = 8;
	static constexpr uint16_t CH1_GENVOL_MASK  = 0x1f << ac97_codec_device::CH1_GENVOL_SHIFT;
	// Commonly the right channel volume (or mono volume)
	static constexpr uint16_t CH2_GENVOL_SHIFT = 0;
	static constexpr uint16_t CH2_GENVOL_MASK  = 0x1f << ac97_codec_device::CH2_GENVOL_SHIFT;
	static constexpr uint16_t MIC_BOOST        = 0x40;

	static constexpr uint16_t CODEC_CAP_DEDICATED_MIC_PCM = 1 << 0;
	static constexpr uint16_t CODEC_CAP_MODEM_LINE        = 1 << 1;
	static constexpr uint16_t CODEC_CAP_BASS_TREBLE_CNTL  = 1 << 2;
	static constexpr uint16_t CODEC_CAP_SIM_STEREO        = 1 << 3;
	static constexpr uint16_t CODEC_CAP_HEADPHONE_OUT     = 1 << 4;
	static constexpr uint16_t CODEC_CAP_BASS_BOOST        = 1 << 5;
	static constexpr uint16_t CODEC_CAP_18BIT_DAC         = 1 << 6;
	static constexpr uint16_t CODEC_CAP_20BIT_DAC         = 1 << 7;
	static constexpr uint16_t CODEC_CAP_18BIT_ADC         = 1 << 8;
	static constexpr uint16_t CODEC_CAP_20BIT_ADC         = 1 << 9;
	static constexpr uint16_t CODEC_CAP_3DSTEREO_SHIFT    = 10;
	static constexpr uint16_t CODEC_CAP_3DSTEREO_MASK     = 0x001f << ac97_codec_device::CODEC_CAP_3DSTEREO_SHIFT;
	static constexpr uint16_t CODEC_CAP_3DSTEREO_NONE     = 0x0000 << ac97_codec_device::CODEC_CAP_3DSTEREO_SHIFT;

	// The 3d stereo id is vendor-specific

	ac97_codec_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);
	
	virtual uint16_t nam_reg_r(offs_t offset);
	virtual void nam_reg_w(offs_t offset, uint16_t data);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void nam_reset() ATTR_COLD;

	uint32_t m_aud_clk;

	uint16_t m_nam_regs[NAM_REG_CNT];

private:

};

class aclink_connection_interface : public aclink_interface
{

public:

	static constexpr uint8_t AC97_CODEC_CNT = 3;

	template <class CODEC_IMPL>
	void codec_insert(machine_config &mconfig, uint8_t index, CODEC_IMPL &CODEC)
	{
		index &= aclink_connection_interface::AC97_CODEC_CNT;

		CODEC(mconfig, m_codec[index], 0);

		m_codec_enabled |= (1 << index);
	}

protected:

	aclink_connection_interface(const machine_config &mconfig, device_t &device, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func = nullptr);

	std::function<void (aclink_connection_interface &, machine_config &)> m_setup_codecs_func = nullptr;

	optional_device_array<ac97_codec_device, aclink_connection_interface::AC97_CODEC_CNT> m_codec;
	uint8_t m_codec_enabled;

};

class i82801_ac97_base : public pci_device, public aclink_connection_interface
{

public:

	enum chan_dir_t : uint8_t
	{
		NABM_CHAN_DIRECTION_IN,
		NABM_CHAN_DIRECTION_OUT,
		NABM_CHAN_DIRECTION_NONE
	};

	typedef struct
	{
		bool bdesc_valid;     // INTERNAL: if the buffer descriptor has been loaded
		uint32_t bdesc_start; // BDBAR[0x00]: Physical address of buffer descriptor list
		uint8_t bdesc_lidx;   // LVI  [0x05]: Last Valid Index; the index value of the last valid entry in the buffer descriptor list
		uint8_t bdesc_cidx;   // CIV  [0x04]: Current Index Value; count of buffer descriptors processed
		uint8_t bdesc_nidx;   // PIV  [0x0a]: Prefetched Index Value; index of the next buffer descriptor that will be processed
		uint32_t bdesc_cmd;   // INTERNAL:    The buffer descriptor entry control bits
		uint16_t tx_status;   // SR   [0x06]: tatus of data transfer
		int16_t tx_cleft;     // PICB [0x08]: Position in Current Buffer; count of samples to be processed in current buffer descriptor
		uint32_t tx_caddr;    // INTERNAL:    The address to the current position in the sample buffer.
		uint8_t tx_cntl;      // CR   [0x0b]: Transfer control
		uint32_t tx_sample;    // INTERNAL: last transferred sample
	} nabm_channel_state;

	static constexpr uint32_t AUD_DEFAULT_CLK = 48000;

	static constexpr uint32_t AC97_IOSPACE_SIZE       = 0x007f;
	static constexpr uint32_t AC97_IOSPACE_PRI_OFFSET = 0x0000;
	static constexpr uint32_t AC97_IOSPACE_SEC_OFFSET = 0x8000;
	static constexpr uint32_t AC97_IOSPACE_TER_OFFSET = 0x0100;
	static constexpr uint32_t AC97_IOSPACE_RES_OFFSET = 0x0180;

	static constexpr uint16_t AC97_NABM_REGS_SIZE = 0x84;

	static constexpr uint32_t AC97_IO_MXR_BASE_MASK  = 0x0000ff00;
	static constexpr uint32_t AC97_MEM_MXR_BASE_MASK = 0xfffffe00;
	static constexpr uint32_t AC97_IO_NABM_BASE_MASK  = 0x0000ffc0;
	static constexpr uint32_t AC97_MEM_NABM_BASE_MASK = 0xffffff00;
	static constexpr uint32_t AC97_BASE_RTE         = 0x00000001;
	static constexpr uint32_t AC97_BASE_IS_IO       = 1 << 0;
	static constexpr uint32_t AC97_BASE_IS_MEM      = 0 << 0;
	static constexpr uint32_t AC97_MEM_TYPE_MASK    = 0x00000006;
	static constexpr uint32_t AC97_MEM_TYPE_ANY     = 0 << 1;
	static constexpr uint32_t AC97_MEM_PREFETCHABLE = 1 << 3;

	static constexpr uint32_t AC97_CNFG_IOSE = 0x00000001;

	static constexpr uint32_t AC97_DEFAULT_MEM_BASE = AC97_MEM_TYPE_ANY | AC97_BASE_IS_MEM;
	static constexpr uint32_t AC97_DEFAULT_IO_BASE  = AC97_BASE_IS_IO;

	// Referenced by 2-byte increments rather than 1-byte increments so everything is >> 1
	static constexpr uint16_t CODEC_ID_MASK = 0x180 >> 1;
	static constexpr uint16_t CODEC_ID0     = 0 << (7 - 1);
	static constexpr uint16_t CODEC_ID1     = 1 << (7 - 1);
	static constexpr uint16_t CODEC_ID2     = 2 << (7 - 1);
	static constexpr uint16_t CODEC_EN0     = 1 << 0;
	static constexpr uint16_t CODEC_EN1     = 1 << 1;
	static constexpr uint16_t CODEC_EN2     = 1 << 2;

	static constexpr uint32_t NABM_GLOB_CNTL_SPDIF_SMAP_SHIFT    = 30;
	static constexpr uint32_t NABM_GLOB_CNTL_SPDIF_SMAP_MASK     = 0x03 << i82801_ac97_base::NABM_GLOB_CNTL_SPDIF_SMAP_SHIFT;;
	static constexpr uint32_t NABM_GLOB_CNTL_SPDIF_SMAP_SLOT0708 = 1 << i82801_ac97_base::NABM_GLOB_CNTL_SPDIF_SMAP_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_SPDIF_SMAP_SLOT0609 = 2 << i82801_ac97_base::NABM_GLOB_CNTL_SPDIF_SMAP_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_SPDIF_SMAP_SLOT1011 = 3 << i82801_ac97_base::NABM_GLOB_CNTL_SPDIF_SMAP_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_MODE_SHIFT  = 22;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_MODE_MASK   = 0x03 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_MODE_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_MODE_16BIT  = 0 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_MODE_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_MODE_20BIT  = 1 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_MODE_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT  = 20;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_CHAN_MASK   = 0x03 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_2CHAN       = 0 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_4CHAN       = 1 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_PCM_OUT_6CHAN       = 2 << i82801_ac97_base::NABM_GLOB_CNTL_PCM_OUT_CHAN_SHIFT;
	static constexpr uint32_t NABM_GLOB_CNTL_CODEC2_RSM_INT      = 1 << 6;
	static constexpr uint32_t NABM_GLOB_CNTL_CODEC1_RSM_INT      = 1 << 5;
	static constexpr uint32_t NABM_GLOB_CNTL_CODEC0_RSM_INT      = 1 << 4;
	static constexpr uint32_t NABM_GLOB_CNTL_ACLINK_OFF          = 1 << 3;
	static constexpr uint32_t NABM_GLOB_CNTL_WARM_RESET          = 1 << 2;
	static constexpr uint32_t NABM_GLOB_CNTL_COLD_RESET          = 1 << 1;
	static constexpr uint32_t NABM_GLOB_CNTL_GPI_EN              = 1 << 0;

	static constexpr uint32_t NABM_GLOB_STATUS_SDIN2_INT        = 1 << 29;
	static constexpr uint32_t NABM_GLOB_STATUS_SDIN2_RDY        = 1 << 29;
	static constexpr uint32_t NABM_GLOB_STATUS_BCLK_STOP        = 1 << 27;
	static constexpr uint32_t NABM_GLOB_STATUS_SPDIF_INT        = 1 << 26;
	static constexpr uint32_t NABM_GLOB_STATUS_PCM_IN2_INT      = 1 << 25;
	static constexpr uint32_t NABM_GLOB_STATUS_MIC_IN2_INT      = 1 << 24;
	static constexpr uint32_t NABM_GLOB_STATUS_CAP_20BIT_SAMP   = 1 << 22;
	static constexpr uint32_t NABM_GLOB_STATUS_CAP_6CHAN        = 1 << 21;
	static constexpr uint32_t NABM_GLOB_STATUS_CAP_4CHAN        = 1 << 20;
	static constexpr uint32_t NABM_GLOB_STATUS_MD3              = 1 << 17;
	static constexpr uint32_t NABM_GLOB_STATUS_AD3              = 1 << 16;
	static constexpr uint32_t NABM_GLOB_STATUS_RCS              = 1 << 15;
	static constexpr uint32_t NABM_GLOB_STATUS_BIT3_SLOT12      = 1 << 14;
	static constexpr uint32_t NABM_GLOB_STATUS_BIT2_SLOT12      = 1 << 13;
	static constexpr uint32_t NABM_GLOB_STATUS_BIT1_SLOT12      = 1 << 12;
	static constexpr uint32_t NABM_GLOB_STATUS_SDIN1_INT        = 1 << 11;
	static constexpr uint32_t NABM_GLOB_STATUS_SDIN0_INT        = 1 << 10;
	static constexpr uint32_t NABM_GLOB_STATUS_SDIN1_RDY        = 1 << 9;
	static constexpr uint32_t NABM_GLOB_STATUS_SDIN0_RDY        = 1 << 8;
	static constexpr uint32_t NABM_GLOB_STATUS_MIC_IN1_INT      = 1 << 7;
	static constexpr uint32_t NABM_GLOB_STATUS_PCM_OUT_INT      = 1 << 6;
	static constexpr uint32_t NABM_GLOB_STATUS_PCM_IN1_INT      = 1 << 5;
	static constexpr uint32_t NABM_GLOB_STATUS_MDM_OUT_INT      = 1 << 2;
	static constexpr uint32_t NABM_GLOB_STATUS_MDM_IN_INT       = 1 << 1;
	static constexpr uint32_t NABM_GLOB_STATUS_GSCI_INT         = 1 << 0;
	static constexpr uint32_t NABM_GLOB_STATUS_BLANK_INT        = 0x00000000;
	static constexpr uint32_t NABM_GLOB_STATUS_WMASK            = 0x00030000;
	static constexpr uint32_t NABM_GLOB_STATUS_WCMASK           = 0x00008c01;

	static constexpr uint16_t NABM_CAS_SET = 1 << 0;

	static constexpr uint8_t NABM_SDIN_MIC2_SHIFT   = 6;
	static constexpr uint8_t NABM_SDIN_MIC2_MASK    = 0x03 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC2_CODEC0  = 0 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC2_CODEC1  = 1 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC2_CODEC2  = 2 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC1_SHIFT   = 4;
	static constexpr uint8_t NABM_SDIN_MIC1_MASK    = 0x03 << i82801_ac97_base::NABM_SDIN_MIC1_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC1_CODEC0  = 0 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC1_CODEC1  = 1 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_MIC1_CODEC2  = 2 << i82801_ac97_base::NABM_SDIN_MIC2_SHIFT;
	static constexpr uint8_t NABM_SDIN_STEER_EN     = 1 << 3;
	static constexpr uint8_t NABM_SDIN_LDATA_SHIFT  = 0;
	static constexpr uint8_t NABM_SDIN_LDATA_MASK   = 0x03 << i82801_ac97_base::NABM_SDIN_LDATA_SHIFT;
	static constexpr uint8_t NABM_SDIN_LDATA_CODEC0 = 0 << i82801_ac97_base::NABM_SDIN_LDATA_SHIFT;
	static constexpr uint8_t NABM_SDIN_LDATA_CODEC1 = 1 << i82801_ac97_base::NABM_SDIN_LDATA_SHIFT;
	static constexpr uint8_t NABM_SDIN_LDATA_CODEC2 = 2 << i82801_ac97_base::NABM_SDIN_LDATA_SHIFT;
	static constexpr uint8_t NABM_SDIN_WMASK        = 0xf8;

	static constexpr uint8_t BDESC_SIZE        = 8;
	static constexpr uint8_t BDESC_ADDR_OFFSET = 0;
	static constexpr uint8_t BDESC_SIZE_OFFSET = 4;
	static constexpr uint8_t BDESC_CMD_OFFSET  = 6;
	static constexpr uint8_t SAMPLE_16BIT_SIZE = 2;
	static constexpr uint8_t SAMPLE_20BIT_SIZE = 4;

	static constexpr uint16_t BDESC_CMD_SET_IOC = 1 << 15;
	static constexpr uint16_t BDESC_CMD_USE_BUP = 1 << 14;

	static constexpr uint8_t NABM_CHAN_COUNT                  = 6;

	static constexpr uint32_t NABM_CHAN_BLANK_OFFSET          = 0xffffffff;

	static constexpr uint32_t NABM_CHAN_BBDBAR_WMASK          = 0xfffffff8;
	static constexpr uint8_t NABM_CHAN_IDX_MASK               = 0x1f;

	static constexpr uint16_t NABM_CHAN_STATUS_FIFO_ERR_INT   = 1 << 4;
	static constexpr uint16_t NABM_CHAN_STATUS_IOC_INT        = 1 << 3;
	static constexpr uint16_t NABM_CHAN_STATUS_LAST_BUFF_INT  = 1 << 2;
	static constexpr uint16_t NABM_CHAN_STATUS_EOL            = 1 << 1;
	static constexpr uint16_t NABM_CHAN_STATUS_DMA_HALTED     = 1 << 0;
	static constexpr uint16_t NABM_CHAN_STATUS_WCMASK         = 0x1c;

	static constexpr uint16_t NABM_CHAN_PICB_MASK             = 0xffff;
	static constexpr uint8_t NABM_CHAN_PIV_MASK               = 0x1f;

	static constexpr uint8_t NABM_CHAN_CNTL_IOC_INT_EN        = 1 << 4;
	static constexpr uint8_t NABM_CHAN_CNTL_FIFO_ERR_INT_EN   = 1 << 3;
	static constexpr uint8_t NABM_CHAN_CNTL_LAST_BUFF_INT_EN  = 1 << 2;
	static constexpr uint8_t NABM_CHAN_CNTL_RESET             = 1 << 1;
	static constexpr uint8_t NABM_CHAN_CNTL_TRANSFER_EN       = 1 << 0;
	static constexpr uint8_t NABM_CHAN_CNTL_INT_MASK          = 0x1c;

	static constexpr nabm_channel_state NABM_CHAN_DEFAULT_STATE = {
		.bdesc_valid = false,
		.bdesc_start = 0x00000000,
		.bdesc_lidx = 0,
		.bdesc_cidx = 0,
		.bdesc_nidx = 0,
		.bdesc_cmd = 0,
		.tx_status = i82801_ac97_base::NABM_CHAN_STATUS_DMA_HALTED,
		.tx_cleft = 0,
		.tx_caddr = 0x00000000,
		.tx_cntl = 0,
		.tx_sample = 0x00000000
	};

	uint8_t m_pcm1_in_offset;
	uint8_t m_pcm_out_offset;
	uint8_t m_mic1_in_offset;
	uint8_t m_mic2_in_offset;
	uint8_t m_pcm2_in_offset;
	uint8_t m_spidf_in_offset;
	uint8_t m_modem_in_offset;
	uint8_t m_modem_out_offset;

	i82801_ac97_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func = nullptr, uint32_t main_id = 0x808624c5, uint32_t revision = 0x00);

	auto pirq_callback() { return m_pirq_cb.bind(); }

	void set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin);

	chan_dir_t chan_transfer_direction(offs_t offset);
	bool chan_transfer_enabled(offs_t offset);
	void chan_transfer_fetch(offs_t offset);
	void chan_transfer_end(offs_t offset);
	uint32_t chan_sample_read32(offs_t offset);
	void chan_sample_write32(offs_t offset, uint32_t sample);

protected:


	uint8_t m_chan_enabled_count;
	std::array<uint32_t, i82801_ac97_base::NABM_CHAN_COUNT> m_chan_offsets;
	std::array<chan_dir_t, i82801_ac97_base::NABM_CHAN_COUNT> m_chan_direction;
	std::array<uint32_t, i82801_ac97_base::NABM_CHAN_COUNT> m_chan_glob_int_mask;

	uint8_t m_pirq_pin;

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	void device_add_mconfig(machine_config &config) override ATTR_COLD;

	virtual void map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space* memory_space,
							uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space* io_space) override ATTR_COLD;

	virtual void config_map(address_map &map) override ATTR_COLD;

	virtual uint8_t capptr_r() override;

private:

	devcb_write8 m_pirq_cb;

	uint32_t m_nammbar;
	uint32_t m_nabmbar;
	uint32_t m_mmbar;
	uint32_t m_mbbar;
	uint16_t m_svid;
	uint16_t m_sid;
	uint8_t m_pcid;
	uint8_t m_cfg;

	uint32_t m_glob_cntl;
	uint32_t m_glob_status;
	uint8_t m_cas;
	uint8_t m_sdata_in;

	///

	nabm_channel_state m_chan[i82801_ac97_base::NABM_CHAN_COUNT];

	void nam_io_map(address_map &map);
	void nam_mem_map(address_map &map);
	void nabm_map(address_map &map);
	void map_nabm_chan(offs_t offset, address_map &map);

	uint16_t nam_reg_r(offs_t offset);
	void nam_reg_w(offs_t offset, uint16_t data);

	uint32_t nammbar_r();
	void nammbar_w(uint32_t data);
	uint32_t nambbar_r();
	void nambbar_w(uint32_t data);
	uint32_t mmbar_r();
	void mmbar_w(uint32_t data);
	uint32_t mbbar_r();
	void mbbar_w(uint32_t data);
	void subvendor_w(uint16_t data);
	void subsystem_w(uint16_t data);
	uint8_t pcid_r();
	void pcid_w(uint8_t data);
	uint8_t cfg_r();
	void cfg_w(uint8_t data);

	void nabm_reset();
	void nabm_chan_reset(offs_t offset);
	uint32_t glob_cntl_r();
	void glob_cntl_w(uint32_t data);
	uint32_t glob_status_r();
	void glob_status_w(uint32_t data);
	uint8_t cas_r();
	void cas_w(uint8_t data);
	uint8_t sdata_in_r();
	void sdata_in_w(uint8_t data);


	uint32_t nabm_chan_bdbar_r(offs_t offset);
	void nabm_chan_bdbar_w(offs_t offset, uint32_t data);
	uint8_t nabm_chan_civ_r(offs_t offset);
	uint8_t nabm_chan_lvi_r(offs_t offset);
	void nabm_chan_lvi_w(offs_t offset, uint8_t data);
	uint16_t nabm_chan_sr_r(offs_t offset);
	void nabm_chan_sr_w(offs_t offset, uint16_t data);
	uint16_t nabm_chan_picb_r(offs_t offset);
	uint8_t nabm_chan_piv_r(offs_t offset);
	uint8_t nabm_chan_cr_r(offs_t offset);
	void nabm_chan_cr_w(offs_t offset, uint8_t data);

	void set_nabm_chan_irq(offs_t offset, uint16_t mask, int state);
	void set_nabm_glob_irq(uint32_t mask, int state);

};

class i82801_ac97_device : public i82801_ac97_base
{

public:

	i82801_ac97_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func = nullptr, uint32_t main_id = 0x808624c5, uint32_t revision = 0x02);

};

class i82801_mc97_device : public i82801_ac97_base
{

public:

	i82801_mc97_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, std::function<void (aclink_connection_interface &, machine_config &)> setup_codecs_func = nullptr, uint32_t main_id = 0x80862c64, uint32_t revision = 0x02);

};


DECLARE_DEVICE_TYPE(I82801_AC97, i82801_ac97_device)
DECLARE_DEVICE_TYPE(I82801_MC97, i82801_mc97_device)

#endif // MAME_WEBTV_I82801_AC97_H
