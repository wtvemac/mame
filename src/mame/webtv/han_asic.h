// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_HAN_ASIC_H
#define MAME_WEBTV_HAN_ASIC_H

#pragma once

#include "cpu/mips/mips3.h"
#include "bus/ata/ataintf.h"

constexpr uint32_t HAN_INT_IDE     = 1 << 2;
constexpr uint32_t HAN_INT_MSG_OUT = 1 << 1;
constexpr uint32_t HAN_INT_MSG_IN  = 1 << 0;

constexpr uint32_t HAN_MSGCNTL_BUFF_RESET   = 1 << 5;
constexpr uint32_t HAN_MSGCNTL_BUFF_RELEASE = 1 << 3;
constexpr uint32_t HAN_MSGCNTL_DATA_READ    = 1 << 2;
constexpr uint32_t HAN_MSGCNTL_DATA_VALID   = 1 << 1;
constexpr uint32_t HAN_MSGCNTL_DATA_WAITING = 1 << 1;
constexpr uint32_t HAN_MSGCNTL_BUFF_OWN     = 1 << 0;

constexpr uint8_t HAN_STARTUP_BEGIN           = 0;
constexpr uint8_t HAN_STARTUP_SEND_RESTART    = 1;
constexpr uint8_t HAN_STARTUP_RESTART_OK      = 2;
constexpr uint8_t HAN_STARTUP_SEND_RESET      = 3;
constexpr uint8_t HAN_STARTUP_DONE            = 4;

enum han_msgtype_t : uint16_t
{
	IPC_CLASS_TUNER   = 0x0000,
	IPC_CLASS_DMA     = 0x0001,
	IPC_CLASS_CA      = 0x0002,
	IPC_CLASS_DIAG    = 0x0003,
	IPC_CLASS_MAILBOX = 0x0004,
	IPC_CLASS_RESTART = 0x00f0,
};

enum han_diag_msgsubtype_t : uint16_t
{
	DIAG_ECHO_MB_MSG          = 0x0000,
	DIAG_GET_RX_ERROR_COUNT   = 0x0001,
	DIAG_RESET_RX_ERROR_COUNT = 0x0002,
	DIAG_GET_TX_ERROR_COUNT   = 0x0003,
	DIAG_RESET_TX_ERROR_COUNT = 0x0004,
};

enum han_mailbox_msgsubtype_t : uint16_t
{
	EPC2SRA_EPG_CHANGED                     = 0x0401,
	EPC2SRA_CHANNEL_CHANGED                 = 0x0402,
	EPC2SRA_NOTIFY_FORCED_STANDBY           = 0x0403,
	EPC2SRA_NOTIFY_ALARM                    = 0x0404,
	EPC2SRA_CLEAR_ALARM                     = 0x0405,
	EPC2SRA_BURNING_FLASH                   = 0x0406,
	EPC2SRA_RESET                           = 0x0407,
	EPC2SRA_FREE_PERIOD_OVER                = 0x0408,
	EPC2SRA_GOT_AGC_NO_LOCK                 = 0x0409,
	EPC2SRA_DOWNLOAD_AVAILABLE              = 0x040a,
	EPC2SRA_NO_PHONE                        = 0x040b,
	EPC2SRA_PPV_AUTHORIZATION               = 0x040c,
	EPC2SRA_SW_UPDATE_AVAILABLE             = 0x040d,
	EPC2SRA_PHONE_STATUS                    = 0x040e,
	EPC2SRA_SMART_CARD_NOT_AUTHORIZED       = 0x040f,
	EPC2SRA_MUSIC_TITLE_CHANGED             = 0x0410,
	EPC2SRA_CLOSED_CAPTION_DATA             = 0x0411,
	EPC2SRA_CHANNEL_CHANGE_STATUS           = 0x0412,
	EPC2SRA_VIDEO_TEST_RESULTS              = 0x0413,
	EPC2SRA_MAIN_UNIT_TEST_RESULTS          = 0x0414,
	EPC2SRA_ECM_NOT_AVAILABLE               = 0x0415,
	EPC2SRA_SWITCH_VERIFIED                 = 0x0416,
	EPC2SRA_PAT_PMT_ERROR                   = 0x0417,
	EPC2SRA_IPPV_JEOPARDY                   = 0x0418,
	EPC2SRA_UPDATE_PHONE_STATUS             = 0x0419,
	EPC2SRA_NVM_TEST_RESULT                 = 0x041a,
	EPC2SRA_SEND_APPLICATION_VERSION_ID     = 0x041b,
	EPC2SRA_NEW_CHANNEL_MAP_AVAILABLE       = 0x041c,
	EPC2SRA_GENERIC_POPUP                   = 0x041d,
	EPC2SRA_GENERIC_SCREEN                  = 0x041e,
	EPC2SRA_UNUSED_8                        = 0x041f,
	EPC2SRA_UNUSED_9                        = 0x0420,
	EPC2SRA_UNUSED_10                       = 0x0421,
	EPC2SRA_UNUSED_11                       = 0x0422,
	EPC2SRA_UNUSED_12                       = 0x0423,
	EPC2SRA_UNUSED_13                       = 0x0424,
	EPC2SRA_UNUSED_14                       = 0x0425,
	EPC2SRA_UNUSED_15                       = 0x0426,
	EPC2SRA_MAIL                            = 0x0427,
	SRA2EPC_SET_VERSION_ID                  = 0x0428,
	SRA2EPC_SEND_CLOSED_CAPTION             = 0x0429,
	SRA2EPC_STOP_CLOSED_CAPTION             = 0x042a,
	SRA2EPC_GET_ALARM_STATUS                = 0x042b,
	SRA2EPC_GET_STATUS                      = 0x042c,
	SRA2EPC_GET_FRONT_END_LOCK_STATUS       = 0x042d,
	SRA2EPC_TUNE_TO_TRANSPONDER             = 0x042e,
	SRA2EPC_GET_CURRENT_TRANSPONDER         = 0x042f,
	SRA2EPC_GET_CURRENT_NETWORK             = 0x0430,
	SRA2EPC_STOP_CHANNEL                    = 0x0431,
	SRA2EPC_CHANGE_CHANNEL                  = 0x0432,
	SRA2EPC_GET_CURRENT_CHANNEL             = 0x0433,
	SRA2EPC_GET_SYSTEM_INFO                 = 0x0434,
	SRA2EPC_GET_NVRAM                       = 0x0435,
	SRA2EPC_GET_MEMORY                      = 0x0436,
	SRA2EPC_GET_DIAGNOSTICS                 = 0x0437,
	SRA2EPC_GET_ZIP_CODE                    = 0x0438,
	SRA2EPC_PUT_ZIP_CODE                    = 0x0439,
	SRA2EPC_GO_TO_STANDBY                   = 0x043a,
	SRA2EPC_RETURN_FROM_STANDBY             = 0x043b,
	SRA2EPC_GET_ALT_AUDIO                   = 0x043c,
	SRA2EPC_CHANGE_TO_ALT_AUDIO             = 0x043d,
	SRA2EPC_SET_PREFERRED_AUDIO             = 0x043e,
	SRA2EPC_GET_PREFERRED_AUDIO             = 0x043f,
	SRA2EPC_TEST_PHONE_LINE                 = 0x0440,
	SRA2EPC_GET_SIGNAL_STRENGTH             = 0x0441,
	SRA2EPC_GET_POINTING_INFO               = 0x0442,
	SRA2EPC_PURCHASE_PPV                    = 0x0443,
	SRA2EPC_START_SIGNAL_STRENGTH_SESSION   = 0x0444,
	SRA2EPC_END_SIGNAL_STRENGTH_SESSION     = 0x0445,
	SRA2EPC_ANALOG_AUDIO_SOURCE_SELECT      = 0x0446,
	SRA2EPC_ANALOG_VIDEO_SOURCE_SELECT      = 0x0447,
	SRA2EPC_DIGITAL_AUDIO_SOURCE_SELECT     = 0x0448,
	SRA2EPC_DIGITAL_VIDEO_SOURCE_SELECT     = 0x0449,
	SRA2EPC_GET_TIME                        = 0x044a,
	SRA2EPC_GET_LOCAL_TIME                  = 0x044b,
	SRA2EPC_RESET_MAILBOX                   = 0x044c,
	SRA2EPC_RESET_EPC                       = 0x044d,
	SRA2EPC_PERFORM_VIDEO_TEST              = 0x044e,
	SRA2EPC_PERFORM_MAIN_UNIT_TEST          = 0x044f,
	SRA2EPC_GET_MUSIC_TITLE                 = 0x0450,
	SRA2EPC_GET_SW_UPDATE                   = 0x0451,
	SRA2EPC_CLEAR_NVM                       = 0x0452,
	SRA2EPC_TOGGLE_UI                       = 0x0453,
	SRA2EPC_CHECK_SWITCH                    = 0x0454,
	SRA2EPC_GET_SWITCH_INFO                 = 0x0455,
	SRA2EPC_GET_SWITCH_MATRIX               = 0x0456,
	SRA2EPC_GET_SWITCH_TEST_STEP            = 0x0457,
	SRA2EPC_GET_EPG_START_TIME              = 0x0458,
	SRA2EPC_CHECK_XPONDER_NUM               = 0x0459,
	SRA2EPC_SET_CURRENT_SATELLITE           = 0x045a,
	SRA2EPC_GET_EPC_STATE                   = 0x045b,
	SRA2EPC_SET_ASPECT_RATIO                = 0x045c,
	SRA2EPC_GET_ASPECT_RATIO                = 0x045d,
	SRA2EPC_RETUNE_CHANNEL                  = 0x045e,
	SRA2EPC_GET_NETWORK_HIDDEN_STATUS       = 0x045f,
	SRA2EPC_UPDATE_PHONE_STATUS             = 0x0460,
	SRA2EPC_TEST_NVM                        = 0x0461,
	SRA2EPC_APPLICATION_VERSION_ID          = 0x0462,
	SRA2EPC_GET_CHANNEL_MAP                 = 0x0463,
	SRA2EPC_RESET_PURCASE_HISTORY           = 0x0464,
	SRA2EPC_UPDATE_EPG                      = 0x0465,
	SRA2EPC_DISH_500_INSTALL                = 0x0466,
	SRA2EPC_UNUSED_13                       = 0x0467,
	SRA2EPC_UNUSED_14                       = 0x0468,
	SRA2EPC_UNUSED_15                       = 0x0469,
	SRA2EPC_GET_LOCAL_OFFSET                = 0x046a,
	PC2EPC_REGISTER_DATA_APPLICATION        = 0x046b,
	PC2EPC_DATA_APP_TUNE_REQUEST            = 0x046c,
	PC2EPC_START_SIGNAL_STRENGTH_SESSION    = 0x046d,
	PC2EPC_END_SIGNAL_STRENGTH_SESSION      = 0x046e,
	PC2EPC_GET_SIGNAL_STRENGTH              = 0x046f,
	PC2EPC_CLEAR_NVM                        = 0x0470,
	PC2EPC_GET_AUTH_DATA_SERVICES           = 0x0471,
	PC2EPC_SET_MEMORY                       = 0x0472,
	PC2EPC_GET_TRANSPONDER_INFO             = 0x0473,
	EPC2PC_DATA_REC_STATUS                  = 0x0474,
	PC2EPC_GET_DATA_TUNE_INFO               = 0x0475,
	PC2EPC_GET_DATA                         = 0x0476,
	EPC2PC_GENERIC_DATA                     = 0x0477,
	PC2EPC_CANCEL_DATA_APP_TUNE_REQUEST     = 0x0478,
	PC2EPC_GET_ACTIVE_DATA_DOWNLOADS        = 0x0479,
	PC2EPC_GET_CURRENT_DATA_TAGS            = 0x047a,
	EPC2PC_DMA_CHANNEL_ERROR                = 0x047b,
	PC2EPC_UNUSED_11                        = 0x047c,
	PC2EPC_UNUSED_12                        = 0x047d,
	PC2EPC_UNUSED_13                        = 0x047e,
	PC2EPC_UNUSED_14                        = 0x047f,
	PC2EPC_UNUSED_15                        = 0x0480,
	PC2EPC_UNUSED_16                        = 0x0481,
	PC2EPC_UNREGISTER_DATA_APPLICATION      = 0x0482,
	EPC2SRC_SEND_PRESENT_FOLLOWING          = 0x0483,
	EPC2SRC_PURCHASE_PPV                    = 0x0484,
	EPC2SRC_GET_SUBSCRIPTION_ACCESS         = 0x0485,
	EPC2SRC_GET_EVENT_ACCESS                = 0x0486,
	EPC2SRC_GET_PURCHASE_HISTORY            = 0x0487,
	EPC2SRC_SEND_TIME_SLICE                 = 0x0488,
	EPC2SRC_STOP_EPG                        = 0x0489,
	EPC2SRC_MODEM_RESPONSE                  = 0x048a,
	EPC2SRC_MODEM_DATA                      = 0x048b,
	EPC2SRC_GET_SERVICE_LIST                = 0x048c,
	EPC2SRC_EPC_SHUTTING_DOWN               = 0x048d,
	EPC2SRC_EPC_OPERATIONAL                 = 0x048e,
	EPC2SRC_TUNE_REQUEST                    = 0x048f,
	EPC2SRC_GET_NVM_VARIABLE                = 0x0490,
	EPC2SRC_SET_NVM_VARIABLE                = 0x0491,
	EPC2SRC_TUNE_TO_EPG                     = 0x0492,
	EPC2SRC_PHONE_STATUS                    = 0x0493,
	EPC2SRC_START_DMA_CHANNEL               = 0x0494,
	EPC2SRC_STOP_DMA_CHANNEL                = 0x0495,
	EPC2SRC_SET_MEMORY                      = 0x0496,
	EPC2SRC_GET_PIDS                        = 0x0497,
	EPC2SRC_GET_SERVICE_ATTRIBS             = 0x0498,
	EPC2SRC_SEND_ASSOC_TAG_DESCRIPTORS      = 0x0499,
	EPC2SRC_AC3_SETUP                       = 0x049a,
	EPC2SRC_GET_DATA_TAGS                   = 0x049b,
	EPC2SRC_SEND_DATA_TAGS                  = 0x049c,
	EPC2SRC_UPDATE_CHANNEL_LIST             = 0x049d,
	EPC2SRC_GET_VIRTUAL_SUBSCRIPTION_RIGHTS = 0x049e,
	EPC2SRC_SEND_EPG                        = 0x049f,
	EPC2SRC_UNUSED_11                       = 0x04a0,
	EPC2SRC_UNUSED_12                       = 0x04a1,
	EPC2SRC_UNUSED_13                       = 0x04a2,
	EPC2SRC_EEIT_PIDS                       = 0x04a3,
	EPC2SRC_GET_SUID_LIST                   = 0x04a4,
	EPC2SRC_UNUSED_16                       = 0x04a5,
	EPC2SRC_GET_EPG_STATUS                  = 0x04a6,
	SRC2EPC_EPG_STATUS                      = 0x04a7,
	SRC2EPC_TUNE_REQUEST_RESULT             = 0x04a8,
	SRC2EPC_NEW_PRESENT_FOLLOWING           = 0x04a9,
	SRC2EPC_PURCHASE_STATUS                 = 0x04aa,
	SRC2EPC_SUBSCRIPTION_ACCESS_RIGHTS      = 0x04ab,
	SRC2EPC_EVENT_ACCESS_RIGHTS             = 0x04ac,
	SRC2EPC_PURCHASE_HISTORY                = 0x04ad,
	SRC2EPC_EPG_TIME_SLICE_NOT_AVAIL        = 0x04ae,
	SRC2EPC_EPG_TIME_SLICE_COMPLETE         = 0x04af,
	SRC2EPC_EPG_SECTION                     = 0x04b0,
	SRC2EPC_MODEM_COMMAND                   = 0x04b1,
	SRC2EPC_MODEM_DATA                      = 0x04b2,
	SRC2EPC_SERVICE_LIST                    = 0x04b3,
	SRC2EPC_MODEM_CNTL                      = 0x04b4,
	SRC2EPC_SIGNAL_STRENGTH                 = 0x04b5,
	SRC2EPC_MAIL                            = 0x04b6,
	SRC2EPC_NO_DOWNLOAD_AVAILABLE           = 0x04b7,
	SRC2EPC_EPG_NOT_AVAILABLE               = 0x04b8,
	SRC2EPC_SERVICE_LIST_NOT_AVAILABLE      = 0x04b9,
	SRC2EPC_NEW_SERVICE_LIST_AVAIL          = 0x04ba,
	SRC2EPC_IN_STANDBY                      = 0x04bb,
	SRC2EPC_RETURN_FROM_STANDBY             = 0x04bc,
	SRC2EPC_PF_SECTION                      = 0x04bd,
	SRC2EPC_TEST_PHONE                      = 0x04be,
	SRC2EPC_DATA_FILE_SECTION               = 0x04bf,
	SRC2EPC_VIDEO_TEST_RESULTS              = 0x04c0,
	SRC2EPC_MAIN_UNIT_TEST_RESULTS          = 0x04c1,
	SRC2EPC_SWITCH_VERIFIED                 = 0x04c2,
	SRC2EPC_NVM_WRITE_FAILURE               = 0x04c3,
	SRC2EPC_GET_PIDS                        = 0x04c4,
	SRC2EPC_RESET_APPLICATION               = 0x04c5,
	SRC2EPC_ASSOC_TAG_DESCRIPTORS_AVAILABLE = 0x04c6,
	SRC2EPC_ASSOC_TAG_DESCRIPTORS_REQUEST   = 0x04c7,
	SRC2EPC_GET_AC3_SETUP                   = 0x04c8,
	SRC2EPC_DATA_TAGS                       = 0x04c9,
	SRC2EPC_IRB_WATCHDOG_FIRED              = 0x04ca,
	SRC2EPC_EPG_COMPLETE                    = 0x04cb,
	SRC2EPC_UNUSED_9                        = 0x04cc,
	SRC2EPC_UNUSED_10                       = 0x04cd,
	SRC2EPC_UNUSED_11                       = 0x04ce,
	SRC2EPC_UNUSED_12                       = 0x04cf,
	SRC2EPC_UNUSED_13                       = 0x04d0,
	SRC2EPC_UNUSED_14                       = 0x04d1,
	SRC2EPC_UNUSED_15                       = 0x04d2,
	SRC2EPC_UNUSED_16                       = 0x04d3,
	SRC2EPC_UNUSED_17                       = 0x04d4,
	WEB2EPC_PURCHASE_PAY_PER_VIEW           = 0x04d5,
	WEB2EPC_GET_EVENT_ACCESS_RIGHTS         = 0x04d6,
	EPC2WEB_EVENT_ACCESS_RIGHTS             = 0x04d7,
	WEB2EPC_GET_SUBSCRIPTION_ACCESS_RIGHTS  = 0x04d8,
	EPC2WEB_SUBSCRIPTION_ACCESS_RIGHTS      = 0x04d9,
	EPC2WEB_FRONT_PANEL_KEY                 = 0x04da,
	WEB2EPC_GET_AUDIO_SAMPLE_RATE           = 0x04db,
	WEB2EPC_IRB_SOURCE_SELECT               = 0x04dc,
	WEB2EPC_HARD_RESET                      = 0x04dd,
	WEB2SRC_DELAY_MODE_SETUP                = 0x04de,
	WEB2SRC_DELAY_MODE_PLAYBACK             = 0x04df,
	WEB2SRC_AC3_INIT_DATA                   = 0x04e0,
	SRC2WEB_RESET_AC3_PTRS                  = 0x04e1,
	WEB2SRC_AC3_PTRS_RESET                  = 0x04e2,
	WEB2SRC_GET_CAPABILITIES                = 0x04e3,
	SRC2WEB_GET_VERSIONS                    = 0x04e4,
	SRC2WEB_REQUEST_RESET                   = 0x04e5,
	WEB2SRC_FREEZE_FRAME                    = 0x04e6,
	WEB2SRC_CLEANUP_PREDATOR_EFFECT         = 0x04e7,
	SRC2WEB_CHECK_FOR_PREDATOR_EFFECT       = 0x04e8,
	WEB2SRC_TUNE_TO_WEB_CHAN                = 0x04e9,
	WEB2SRC_NEED_UPDATE                     = 0x04ea,
	WEB2SRC_RESET_DECODER                   = 0x04eb,
	EPC2WEB_ERROR_DIAG                      = 0x04ec,
	SRC2WEB_FROZEN_PTS                      = 0x04ed,
};

constexpr uint32_t HAN_MSGIRQ_HACK_HZ = 60;
constexpr uint32_t HAN_REBOOT_WAIT_MS = 2000;
constexpr uint16_t HAN_MSGBUFF_SIZE = 0x100;
constexpr uint32_t HAN_MSGSIZE_INDEX = 0x0000;
constexpr uint32_t HAN_MSGTYPE_INDEX = 0x0001;
constexpr uint32_t HAN_MSGSUBTYPE_INDEX = 0x0002;
constexpr uint32_t HAN_MSG_START_INDEX = 0x0003;

class han_asic_device : public device_t
{

public:

	han_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	void map(address_map &map);

	void prepare_for_reset();

	void irq_ide1_w(int state);

	template <typename T> void set_ata(T &&tag) { m_ata.set_tag(std::forward<T>(tag)); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	required_device<mips3_device> m_hostcpu;
	optional_device<ata_interface_device> m_ata;

	emu_timer *han_message_timer = nullptr;
	TIMER_CALLBACK_MEMBER(check_han_message_state);

	emu_timer *han_reboot_timer = nullptr;
	TIMER_CALLBACK_MEMBER(han_reboot);

	uint32_t m_han_intenable;
	uint32_t m_han_intstat;
	
	uint32_t m_han_msgbuff_status;
	bool m_han_need_in_int;
	uint16_t m_han_msgbuff[HAN_MSGBUFF_SIZE >> 1];
	uint32_t m_han_msgbuff_index;
	uint8_t m_han_startup_step;

	bool have_queued_han_message();
	bool can_send_han_message();
	bool send_han_message(uint16_t msg_type, uint16_t msg_subtype, uint8_t* msg, uint16_t msg_size);
	uint32_t arrange_han_data(uint32_t data);
	void set_han_irq(uint32_t mask, int state);

	uint32_t reg_han_0000_r();          // INTSTAT_C (read)
	void reg_han_0000_w(uint32_t data); // INTSTAT_C (write)
	uint32_t reg_han_0004_r();          // INTEN_S (read)
	void reg_han_0004_w(uint32_t data); // INTEN_S (write)

	uint32_t reg_han_0010_r();          // _ (read)
	void reg_han_0010_w(uint32_t data); // _ (write)
	uint32_t reg_han_0080_r();          // MSGCNTL? (read)
	void reg_han_0080_w(uint32_t data); // MSGCNTL? (write)
	uint32_t reg_han_0084_r();          // MSGFIFO? (read)
	void reg_han_0084_w(uint32_t data); // MSGFIFO? (write)

	uint32_t reg_ide_000000_r();          // IDE I/O port cs0[0] (data read)
	void reg_ide_000000_w(uint32_t data); // IDE I/O port cs0[0] (data write)
	uint32_t reg_ide_000004_r();          // IDE I/O port cs0[1] (error read)
	void reg_ide_000004_w(uint32_t data); // IDE I/O port cs0[1] (feature write)
	uint32_t reg_ide_000008_r();          // IDE I/O port cs0[2] (sector count read)
	void reg_ide_000008_w(uint32_t data); // IDE I/O port cs0[2] (sector count write)
	uint32_t reg_ide_00000c_r();          // IDE I/O port cs0[3] (sector number read)
	void reg_ide_00000c_w(uint32_t data); // IDE I/O port cs0[3] (sector number write)
	uint32_t reg_ide_000010_r();          // IDE I/O port cs0[4] (cylinder low read)
	void reg_ide_000010_w(uint32_t data); // IDE I/O port cs0[4] (cylinder low write)
	uint32_t reg_ide_000014_r();          // IDE I/O port cs0[5] (cylinder high read)
	void reg_ide_000014_w(uint32_t data); // IDE I/O port cs0[5] (cylinder high write)
	uint32_t reg_ide_000018_r();          // IDE I/O port cs0[6] (drive/head read)
	void reg_ide_000018_w(uint32_t data); // IDE I/O port cs0[6] (drive/head write)
	uint32_t reg_ide_00001c_r();          // IDE I/O port cs0[7] (drive/head read)
	void reg_ide_00001c_w(uint32_t data); // IDE I/O port cs0[7] (drive/head write)
	uint32_t reg_ide_400018_r();          // IDE I/O port cs1[6] (altstatus read)
	void reg_ide_400018_w(uint32_t data); // IDE I/O port cs1[6] (device control write)
	uint32_t reg_ide_40001c_r();          // IDE I/O port cs1[7] (device address read)
	void reg_ide_40001c_w(uint32_t data); // IDE I/O port cs1[7] (device address write)

};

DECLARE_DEVICE_TYPE(HAN_ASIC, han_asic_device)

#endif // MAME_WEBTV_HAN_ASIC_H