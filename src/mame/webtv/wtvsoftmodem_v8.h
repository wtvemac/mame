// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// https://www.itu.int/rec/T-REC-V.8-199905-S

#ifndef MAME_WEBTV_WTVSOFTMODEM_V8_H
#define MAME_WEBTV_WTVSOFTMODEM_V8_H

#pragma once

#include "wtvsoftmodem_tone.h"
#include "wtvsoftmodem_fsk.h"
#include "wtvsoftmodem_framing.h"
#include <vector>
#include <algorithm>

using std::vector;

class wtvsoftmodem_v8 : public wtvsoftmodem_tone, public wtvsoftmodem_fsk
{

public:

	enum V8State {
		IDLE,      // Nothing has happend yet
		RCV_CI,    // Received Call Indication (CI)
		SND_CI,    // Sending Call Indication (CI)
		SNT_CI,    // Sent Call Indication (CI)
		RCV_ANSAM, // Received modified answer tone ANSam
		SND_ANSAM, // Sending modified answer tone ANSam
		SNT_ANSAM, // Sent modified answer tone ANSam
		RCV_CM,    // Received Call Menu (CM)
		SND_CM,    // Sending Call Menu (CM)
		SNT_CM,    // Sent Call Menu (CM)
		RCV_JM,    // Received Joint Menu (JM)
		SND_JM,    // Sending Joint Menu (JM)
		SNT_JM,    // Sent Joint Menu (JM)
		NEGOTIATED // Both sides fully negotiated parameters
	};

	typedef std::function<void(V8State, V8State)> state_change_callback_func;

	typedef uint16_t menu_item_t;

	static constexpr tone_t TONE_V8_CRE0   = 0x90;
	static constexpr tone_t TONE_V8_CRE1   = 0x91;
	static constexpr tone_t TONE_V8_CRDI0  = 0x92;
	static constexpr tone_t TONE_V8_CRDI1  = 0x93;
	static constexpr tone_t TONE_V8_CRDR0  = 0x94;
	static constexpr tone_t TONE_V8_CRDR1  = 0x95;
	static constexpr tone_t TONE_V8_CT     = 0x96;
	static constexpr tone_t TONE_V8_CNG    = 0x97;
	static constexpr tone_t TONE_V8_ESI0   = 0x98;
	static constexpr tone_t TONE_V8_ESI1   = 0x99;
	static constexpr tone_t TONE_V8_ESR0   = 0x9a;
	static constexpr tone_t TONE_V8_ESR1   = 0x9b;
	static constexpr tone_t TONE_V8_MRE0   = 0x9c;
	static constexpr tone_t TONE_V8_MRE1   = 0x9d;
	static constexpr tone_t TONE_V8_MRDI0  = 0x9e;
	static constexpr tone_t TONE_V8_MRDI1  = 0x9f;
	static constexpr tone_t TONE_V8_MRDR0  = 0xa0;
	static constexpr tone_t TONE_V8_MRDR1  = 0xa1;
	static constexpr tone_t TONE_V8_ANS    = 0xa2;
	static constexpr tone_t TONE_V8_XANS   = 0xa3;
	static constexpr tone_t TONE_V8_ANSAM  = 0xa4;
	static constexpr tone_t TONE_V8_XANSAM = 0xa5;

	static constexpr menu_item_t V8_MENU_CAT_MASK       = 0x0f8;
	static constexpr menu_item_t V8_MENU_ITM_MASK       = 0x007;

	static constexpr menu_item_t V8_MENU_CAT_PROTO      = 0x550;                     // Protocols
	static constexpr menu_item_t V8_MENU_PROTO_V42CALLS = V8_MENU_CAT_PROTO  | 0x02; // Calls via V.42 LAPM protocol
	static constexpr menu_item_t V8_MENU_PROTO_EXTCALLS = V8_MENU_CAT_PROTO  | 0x01; // Calls via extension octet

	static constexpr menu_item_t V8_MENU_CAT_FAXFN      = 0x770;                     // Fax function defined in T.66

	static constexpr menu_item_t V8_MENU_CAT_CFUNC      = 0x880;                     // Call function
	static constexpr menu_item_t V8_MENU_CFUNC_PSTNMULT = V8_MENU_CAT_CFUNC  | 0x04; // PSTN Multimedia Terminal H.324
	static constexpr menu_item_t V8_MENU_CFUNC_TXTPHONE = V8_MENU_CAT_CFUNC  | 0x02; // Textphone V.18
	static constexpr menu_item_t V8_MENU_CFUNC_VIDPHONE = V8_MENU_CAT_CFUNC  | 0x06; // Videotext T.101
	static constexpr menu_item_t V8_MENU_CFUNC_T30TXFAX = V8_MENU_CAT_CFUNC  | 0x01; // Transmit facsimile from call terminal T.30
	static constexpr menu_item_t V8_MENU_CFUNC_T30RXFAX = V8_MENU_CAT_CFUNC  | 0x05; // Receive facsimile at call terminal T.30
	static constexpr menu_item_t V8_MENU_CFUNC_DATMODEM = V8_MENU_CAT_CFUNC  | 0x03; // Data modem (unspecified application)
	static constexpr menu_item_t V8_MENU_CFUNC_CALLFEXT = V8_MENU_CAT_CFUNC  | 0x07; // Call function as indicated in an extension octet

	static constexpr menu_item_t V8_MENU_MMODE_EXM_MASK = 0x01c;
	static constexpr menu_item_t V8_MENU_MMODE_EXI_MASK = 0x0e3;
	static constexpr menu_item_t V8_MENU_CAT_MMODE0     = 0xaa0;                     // Modulation modes octet 0 (modn0)
	static constexpr menu_item_t V8_MENU_MMODE_PCMMODEM = V8_MENU_CAT_MMODE0 | 0x04; // PCM Modem availability
	static constexpr menu_item_t V8_MENU_MMODE_V34FULLD = V8_MENU_CAT_MMODE0 | 0x02; // V.34 duplex availability
	static constexpr menu_item_t V8_MENU_MMODE_V34HALFD = V8_MENU_CAT_MMODE0 | 0x01; // V.34 half-duplex availability
	static constexpr menu_item_t V8_MENU_CAT_MMODE1     = 0xb08;                     // Modulation modes octet 1 (modn1)
	static constexpr menu_item_t V8_MENU_MMODE_V32AVAIL = V8_MENU_CAT_MMODE1 | 0x80; // V.32 bis/V.32 availability
	static constexpr menu_item_t V8_MENU_MMODE_V22AVAIL = V8_MENU_CAT_MMODE1 | 0x40; // V.22 bis/V.22 availability
	static constexpr menu_item_t V8_MENU_MMODE_V17AVAIL = V8_MENU_CAT_MMODE1 | 0x20; // V.17 availability
	static constexpr menu_item_t V8_MENU_MMODE_V29HALFD = V8_MENU_CAT_MMODE1 | 0x02; // V.29 half-duplex availability
	static constexpr menu_item_t V8_MENU_MMODE_V27TERAV = V8_MENU_CAT_MMODE1 | 0x01; // V.27 ter availability
	static constexpr menu_item_t V8_MENU_CAT_MMODE2     = 0xc08;                     // Modulation modes octet 2 (modn2)
	static constexpr menu_item_t V8_MENU_MMODE_V26TERAV = V8_MENU_CAT_MMODE2 | 0x80; // V.26 ter availability
	static constexpr menu_item_t V8_MENU_MMODE_V26BISAV = V8_MENU_CAT_MMODE2 | 0x40; // V.26 bis availability 
	static constexpr menu_item_t V8_MENU_MMODE_V23FULLD = V8_MENU_CAT_MMODE2 | 0x20; // V.23 duplex availability
	static constexpr menu_item_t V8_MENU_MMODE_V23HALFD = V8_MENU_CAT_MMODE2 | 0x02; // V.23 half-duplex availability 
	static constexpr menu_item_t V8_MENU_MMODE_V21AVAIL = V8_MENU_CAT_MMODE2 | 0x01; // V.21 availability

	static constexpr menu_item_t V8_MENU_CAT_PSTNA      = 0xbb0;                     // PSTN access
	static constexpr menu_item_t V8_MENU_PSTNA_DCECCELL = V8_MENU_CAT_PSTNA  | 0x04; // Call DCE is via cellular
	static constexpr menu_item_t V8_MENU_PSTNA_DCEACELL = V8_MENU_CAT_PSTNA  | 0x02; // Answer DCE is via cellular
	static constexpr menu_item_t V8_MENU_PSTNA_DCEDIGIT = V8_MENU_CAT_PSTNA  | 0x01; // DCE is digital
	static constexpr menu_item_t V8_MENU_PSTNA_DCEANLOG = V8_MENU_CAT_PSTNA  | 0x00; // DCE is analog

	static constexpr menu_item_t V8_MENU_CAT_PCMMD      = 0xee0;                     // PCM modem availability
	static constexpr menu_item_t V8_MENU_CFUNC_V90ANLOG = V8_MENU_CAT_PCMMD  | 0x04; // V.90 analogue modem availability
	static constexpr menu_item_t V8_MENU_CFUNC_V90DIGIT = V8_MENU_CAT_PCMMD  | 0x02; // V.90 digital modem availability
	static constexpr menu_item_t V8_MENU_CFUNC_V91AVAIL = V8_MENU_CAT_PCMMD  | 0x01; // V.91 availability

	static constexpr menu_item_t V8_MENU_CAT_NSTND      = 0xff0;                     // Non-standard facilities

	wtvsoftmodem_v8();

	void set_sample_rate(wtvsoftmodem_dsp::frequency_t set_sample_rate);

	bool add_tx_capability(menu_item_t item);
	bool is_tx_capable(menu_item_t item);
	vector<menu_item_t>& get_tx_capabilities();

	bool is_rx_capable(menu_item_t item);
	vector<menu_item_t>& get_rx_capabilities();

	V8State get_current_state();

	void set_state_change_callback(state_change_callback_func cb);

	void start(bool disable_echo_suppressors = true);
	void stop();

	uint32_t tx(int32_t* sample, uint32_t sample_count);

private:

	static constexpr uint32_t BIT_SYNC_NONE  = 0x000;
	static constexpr uint32_t BIT_SYNC_BITS  = 20;
	static constexpr uint32_t BIT_SYNC_MASK  = ((1 << BIT_SYNC_BITS) - 1);
	static constexpr uint32_t BIT_SYNC_START = 0x3ff;
	static constexpr uint32_t BIT_SYNC_CI    = (0x3ff << 10) | 0x001;
	static constexpr uint32_t BIT_SYNC_CMJM  = (0x3ff << 10) | 0x00f;
	static constexpr uint32_t BIT_SYNC_CJ    = 0x3ff;

	static constexpr uint8_t  MAX_RX_BUFFER_SIZE   = 64;
	static constexpr uint8_t  MAX_TX_BUFFER_SIZE  = 64;

	static constexpr wtvsoftmodem_dsp::frequency_t V8_FREQ_TABLE[] = {
		1375,
		2002,
		400,
		1900,
		1529,
		2225,
		2100,
		1300,
		1100,
		980,
		1650,
		650,
		1150
	};

	// https://www.rfc-editor.org/rfc/rfc4734.html
	static constexpr tone_detail_t V8_TONE_TABLE[] = {
		{
			.tone = TONE_V8_CRE0, // Capabilities Request (CRe); first tone
			.next = TONE_V8_CRE1,
			.f = { 1375, 2002 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_CRE1, // Capabilities Request (CRe); second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 400, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_CRDI0, // Capabilities Request (CRdi), initiating side; first tone
			.next = TONE_V8_CRDI1,
			.f = { 1375, 2002 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_CRDI1, // Capabilities Request (CRdi), initiating side; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1900, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_CRDR0, // Capabilities Request (CRdr), responding side after CRdi; first tone
			.next = TONE_V8_CRDR1,
			.f = { 1529, 2225 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_CRDR1, // Capabilities Request (CRdr), responding side after CRdi; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1900, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_CT, // Calling tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1300, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 600
		},
		{
			.tone = TONE_V8_CNG, // Calling tone for calling Group III fax machine
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1100, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 600
		},
		{
			.tone = TONE_V8_ESI0, // Escape Signal (ESi) initiating side; first tone
			.next = TONE_V8_ESI1,
			.f = { 1375, 2002 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_ESI1, // Escape Signal (ESi) initiating side; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 980, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_ESR0, // Escape Signal (ESr) responding side after ESi; first tone
			.next = TONE_V8_ESR1,
			.f = { 1529, 2225 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_ESR1, // Escape Signal (ESr) responding side after ESi; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1650, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_MRE0, // Mode Request (MRe); first tone
			.next = TONE_V8_MRE1,
			.f = { 1375, 2002 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_MRE1, // Mode Request (MRe); second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 650, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_MRDI0, // Mode Request (MRdi), initiating side; first tone
			.next = TONE_V8_MRDR1,
			.f = { 1375, 2002 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_MRDI1, // Mode Request (MRdi), initiating side; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1150, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_MRDR0, // Mode Request (MRdr), responding side after MRdi; first tone
			.next = TONE_V8_MRDR1,
			.f = { 1529, 2225 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 400
		},
		{
			.tone = TONE_V8_MRDR1, // Mode Request (MRdr), responding side after MRdi; second tone
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 1150, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 100
		},
		{
			.tone = TONE_V8_ANS, // ANS; disable echo supression
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 2100, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 2000
		},
		{
			.tone = TONE_V8_XANS, // /ANS; disable echo supression + disable echo cancellers
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 2100, 0 },
			.am = { .f = 0, .amin = 0, .amax = 0 },
			.preverse_ms = 450,
			.detect_duration_ms = 48,
			.play_duration_ms = 2000
		},
		{
 			.tone = TONE_V8_ANSAM, // ANSam; no need to disable echo cancellers
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 2100, 0 },
			.am = { .f = 15, .amin = 0.8, .amax = 1.2 },
			.preverse_ms = 0,
			.detect_duration_ms = 48,
			.play_duration_ms = 2000
		},
		{
			.tone = TONE_V8_XANSAM, // /ANSam; no need to disable echo cancellers but disable echo supression
			.next = wtvsoftmodem_tone::TONE_STOP,
			.f = { 2100, 0 },
			.am = { .f = 15, .amin = 0.8, .amax = 1.2 },
			.preverse_ms = 450,
			.detect_duration_ms = 48,
			.play_duration_ms = 2000
		}
	};

	uint32_t rx_synced;
	uint32_t rx_bit_sync;
	frame8n1::parser_state_t rx_frame_state;
	uint8_t rx_blank_byte_count;
	uint8_t rx_buffer_size;
	uint8_t rx_buffer[MAX_RX_BUFFER_SIZE];
	uint32_t previous_menu_checksum;

	enum V8OutState {
		DISABLED,
		SEND_SYNC,
		SEND_BYTES
	};

	V8OutState v8_tx_state;

	uint32_t tx_sync_header;
	int8_t tx_sync_bit_index;
	uint8_t tx_buffer[MAX_TX_BUFFER_SIZE];
	uint8_t tx_buffer_size;
	uint8_t tx_buffer_index;
	frame8n1::builder_state_t tx_frame_state;
	uint32_t tx_repeated_count;

	vector<menu_item_t> rx_capabilities;
	vector<menu_item_t> tx_capabilities;

	wtvsoftmodem_dsp::dft_state_t v8_dft_state[(sizeof(V8_FREQ_TABLE) / sizeof(V8_FREQ_TABLE[0]))];

	V8State v8_state;

	state_change_callback_func state_change_cb;
	
	//wtvsoftmodem_dsp::frequency_t sample_rate;

	virtual void tone_completed() override;

	void set_current_state(V8State new_state);

	virtual void push_rx_bit(bool bit) override;

	void start_tx();
	void repeat_tx();
	void reset_tx_state();
	bool get_tx_sync_bit();
	bool get_tx_byte_bit();
	virtual bool pop_tx_bit() override;

	bool add_tx_byte(uint8_t byte);
	void prepare_menu_tx_buffer();
	void send_cm();
	void send_jm();
	void send_menu();

	bool add_rx_capability(menu_item_t item);

	bool parse_protocol_menu_item(uint8_t octet);
	bool parse_fax_menu_item(uint8_t octet);
	bool parse_call_function_menu_item(uint8_t octet);
	bool parse_pcm_modem_menu_item(uint8_t octet);
	bool parse_mmode_menu_item(uint8_t octet, uint8_t mmode_index);
	bool parse_pstn_access_menu_item(uint8_t octet);
	bool parse_nonstd_menu_item(uint8_t octet);
	void parse_menu();

};

#endif // MAME_WEBTV_WTVSOFTMODEM_V8_H