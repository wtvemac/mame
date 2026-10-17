// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// This device interactcs with the WebTV OS by generating or parsing audio samples, allowing WebTV softmodem MAME machines to communicate.

// WebTV OS In  <- This device TX: uint32_t pull(int32_t* sample, uint32_t sample_count)
// WebTV OS Out -> This device RX: void push(const int32_t* sample, uint32_t sample_count);

#ifndef MAME_WEBTV_WTVSOFTMODEM_H
#define MAME_WEBTV_WTVSOFTMODEM_H

#pragma once

#include "diserial.h"

#include <vector>
#include <string>
#include <deque>

#include "wtvsoftmodem_tone.h"
#include "wtvsoftmodem_v8.h"
#include "wtvsoftmodem_v22.h"
#include "wtvsoftmodem_v23.h"
#include "wtvsoftmodem_v34.h"
#include "wtvsoftmodem_v90.h"

#include "wtvsoftmodem_recorder.h"

template <typename T>
concept wtvsoftmodem_sample_buffer = std::is_same_v<T, uint32_t> || std::is_same_v<T, wtvsoftmodem_dsp::sample_t>;

class wtvsoftmodem_device : public device_t, public device_buffered_serial_interface<64>
{

public:

	enum ModemState {
		IDLE_ON_HOOK,
		IDLE_OFF_HOOK,
		CALLER_DIALING,
		WAITING,
		V8,
		V22,
		V23,
		V34,
		V90
	};

	static constexpr uint32_t DEFAULT_SAMPLE_RATE = wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE;
	static constexpr uint32_t DEFAULT_DTE_BUAD_RATE = 115200;

	wtvsoftmodem_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	void reset();

	void set_clock(uint32_t clock);

	auto out_tx_callback() { return m_out_tx_cb.bind(); }
	auto out_dtr_callback() { return m_out_dtr_cb.bind(); }
	auto out_rts_callback() { return m_out_rts_cb.bind(); }
	auto out_int_callback() { return m_out_int_cb.bind(); }

	void rx_w(int state);
	void dcd_w(int state);
	void dsr_w(int state);
	void ri_w(int state);
	void cts_w(int state);

	template <wtvsoftmodem_sample_buffer T>
	uint32_t pull(T* buffer, uint32_t sample_count);

	void restart(wtvsoftmodem_tone::tone_t dial_tone = wtvsoftmodem_tone::TONE_DIAL_NA);
	bool is_on_hook();
	void set_on_hook();
	void set_off_hook(wtvsoftmodem_tone::tone_t dial_tone = wtvsoftmodem_tone::TONE_DIAL_NA);

	template <wtvsoftmodem_sample_buffer T>
	void push(const T* buffer, uint32_t sample_count);

private:

	static constexpr uint32_t WAIT_AFTER_DIAL_SILENCE_MS = 1000; // time is in milliseconds
	static constexpr uint32_t WAIT_BEFORE_DATA_MS = 3200; // time is in milliseconds
	static constexpr uint32_t ECHO_SAMPLE_CNT = 4096;
	static constexpr uint32_t ECHO_DELAY_SAMPLE_CNT = 45;
	static constexpr int32_t ECHO_SCALEDOWN = 4;

	static constexpr std::string WAV_RECORDER_FILETAG = "wtvsm";

	static constexpr uint8_t touchppp_data_command[] = "ATD\x0d";

	devcb_write_line m_out_tx_cb;
	devcb_write_line m_out_dtr_cb;
	devcb_write_line m_out_rts_cb;
	devcb_write_line m_out_int_cb;

	emu_timer *dial_completed_timer = nullptr;
	TIMER_CALLBACK_MEMBER(dial_completed);

	emu_timer *data_start_timer = nullptr;
	TIMER_CALLBACK_MEMBER(start_data);

	uint32_t m_sample_rate;

	wtvsoftmodem_tone m_tone;
	wtvsoftmodem_v8 m_v8;
	wtvsoftmodem_v22 m_v22;
	wtvsoftmodem_v23 m_v23;
	wtvsoftmodem_v34 m_v34;
	wtvsoftmodem_v90 m_v90;

	ModemState m_state;

	bool m_dte_byte_tx = true;
	bool m_dte_in_atd = false;
	bool m_dte_data_started = false;
	bool m_dte_skip_lf = false;
	std::string m_dte_reply_line;
	std::deque<uint8_t> m_dte_pending;

	bool m_wav_recorder_enabled;
	wtvsoftmodem_recorder m_wav_recorder;

	bool m_has_dialed;
	std::string m_dial_digits;

	int32_t m_wait_samples;
	ModemState m_wait_end_state;

	bool m_can_echo;
	std::array<wtvsoftmodem_dsp::sample_t, wtvsoftmodem_device::ECHO_SAMPLE_CNT> m_echo;
	uint32_t m_echo_idx;

	std::vector<wtvsoftmodem_dsp::sample_t> m_sample_scratch;

	void drain_dte_pending();

	void tone_out_completed();

	void reset_echo(bool can_echo = true);

	void set_is_waiting(uint32_t wait_ms, ModemState wait_end_state);
	void set_is_dialing();
	void listen_for_dial_string(const wtvsoftmodem_dsp::sample_t* sample, uint32_t sample_count);

	void enter_data_mode();

	virtual void received_byte(uint8_t byte) override;

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

	virtual void tra_callback() override;
	virtual void tra_complete() override;

};

DECLARE_DEVICE_TYPE(WTVSOFTMODEM, wtvsoftmodem_device)

#endif // MAME_WEBTV_WTVSOFTMODEM_H