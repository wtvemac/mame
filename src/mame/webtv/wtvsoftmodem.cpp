// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvsoftmodem.h"
#include <cmath>

DEFINE_DEVICE_TYPE(WTVSOFTMODEM, wtvsoftmodem_device, "wtvsoftmodem_device", "WebTV Softmodem")

wtvsoftmodem_device::wtvsoftmodem_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVSOFTMODEM, tag, owner, clock),
	device_buffered_serial_interface(mconfig, *this),
	m_out_tx_cb(*this),
	m_out_dtr_cb(*this),
	m_out_rts_cb(*this),
	m_out_int_cb(*this)
{
}

void wtvsoftmodem_device::device_start()
{
	dial_completed_timer = timer_alloc(FUNC(wtvsoftmodem_device::dial_completed), this);
	data_start_timer = timer_alloc(FUNC(wtvsoftmodem_device::start_data), this);

	save_item(NAME(m_sample_rate));
	//save_item(NAME(m_state));
}

/*
in_cap=00000883 Data modem (unspecified application)
in_cap=00000aa4 PCM Modem availability
in_cap=00000aa2 V.34 duplex availability
in_cap=00000c09 V.21 availability
in_cap=00000ee4 V.90 analogue modem availability
in_cap=00000bb0 DCE is analog
*/

void wtvsoftmodem_device::device_reset()
{
	wtvsoftmodem_device::reset();

	device_buffered_serial_interface::set_data_frame(1, 8, PARITY_NONE, STOP_BITS_1);
	device_buffered_serial_interface::set_rate(wtvsoftmodem_device::DEFAULT_DTE_BUAD_RATE);

	wtvsoftmodem_device::set_clock(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE);

	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_DATMODEM);
	//m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V21AVAIL);
	//m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V32AVAIL);
	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V23FULLD);
	//m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V22AVAIL);

	/*
	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD);
	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_V90ANLOG);
	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM);
	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCEANLOG);
	*/
	
	m_out_dtr_cb(1);

	m_v23.set_rx_byte_callback([this] (uint8_t byte)
	{
		device_buffered_serial_interface::transmit_byte(byte);
	});

	m_v8.set_state_change_callback([this] (wtvsoftmodem_v8::V8State old_state, wtvsoftmodem_v8::V8State new_state)
	{
#if SOFTMODEM_DEBUG
		printf("MAME MESSAGE-> V8: old=%02x, new=%02x\n", old_state, new_state);
#endif

		if (new_state == wtvsoftmodem_v8::V8State::RCV_CM)
		{
#if SOFTMODEM_DEBUG
			auto in_caps = m_v8.get_peer_capabilities();
			for (const auto &in_cap : in_caps)
			{
				printf("\tMAME MESSAGE-> in_cap=%08x\n", in_cap);
			}
#endif
		}
		else if (new_state == wtvsoftmodem_v8::V8State::NEGOTIATED)
		{
#if SOFTMODEM_DEBUG
			printf("MAME MESSAGE-> GO V23\n");
#endif
			m_state = ModemState::V23;
			wtvsoftmodem_device::enter_data_mode();
		}
	});
}

void wtvsoftmodem_device::reset()
{
	m_state = ModemState::IDLE_ON_HOOK;

	wtvsoftmodem_device::reset_echo(false);

	m_has_dialed = false;
	m_dial_digits.clear();

	m_dte_byte_tx = false;

	m_dte_in_atd = false;
	m_dte_data_started = false;
	m_dte_skip_lf = false;
	m_dte_reply_line.clear();
	m_dte_pending.clear();
	clear_fifo();
	transmit_register_reset();

	data_start_timer->adjust(attotime::never);
	dial_completed_timer->adjust(attotime::never);
}

void wtvsoftmodem_device::tra_callback()
{
	int m_txd = device_buffered_serial_interface::transmit_register_get_data_bit();

	m_out_tx_cb(m_txd);
}

void wtvsoftmodem_device::tra_complete()
{
	device_buffered_serial_interface::tra_complete();
	drain_dte_pending();
}

void wtvsoftmodem_device::drain_dte_pending()
{
	while (m_dte_in_atd && m_dte_data_started && !m_dte_pending.empty() && !fifo_full())
	{
		device_buffered_serial_interface::transmit_byte(m_dte_pending.front());
		m_dte_pending.pop_front();
	}
}

void wtvsoftmodem_device::set_clock(uint32_t clock)
{
	m_sample_rate = clock;

	m_tone.set_sample_rate(m_sample_rate);
	m_v8.set_sample_rate(m_sample_rate);
	m_v22.set_sample_rate(m_sample_rate);
	m_v23.set_sample_rate(m_sample_rate);
}

void wtvsoftmodem_device::rx_w(int state)
{
	if (m_dte_byte_tx)
	{
		// Will collect bits without framing until we have a byte ready then process the entire byte in received_byte.
		device_serial_interface::rx_w(state);
	}
	else
	{
		switch (m_state)
		{
			case ModemState::V23:
				m_v23.transmit_le_8n1_bit(state & 0x1);
				break;

			default:
				break;
		}
	}
}

void wtvsoftmodem_device::received_byte(uint8_t byte)
{
	if (m_dte_in_atd)
	{
		// Remove modem status messages from byte stream before we commit to data mode
		if (!m_dte_data_started)
		{
			bool can_transmit = false;

			if (byte == '\r' || byte == '\n')
			{
				if (m_dte_reply_line == "CONNECT" || m_dte_reply_line.starts_with("CONNECT "))
					m_dte_data_started = true;

				m_dte_reply_line.clear();
			}
			// If we get a PPP flag byte then enter data mode. 
			else if(byte == 0x7e)
			{
				m_dte_data_started = true;
				can_transmit = true;
			}
			// Log printable ASCII characters
			else if (byte >= 0x20 && byte < 0x7f && m_dte_reply_line.size() < 128)
			{
				m_dte_reply_line.push_back(static_cast<char>(byte));
			}

			if (m_dte_data_started)
			{
				m_dte_skip_lf = (byte == '\r');
				drain_dte_pending();
			}

			if (!can_transmit)
				return;
		}
		else if (m_dte_skip_lf)
		{
			m_dte_skip_lf = false;

			if (byte == '\n')
				return;
		}

		// Send actual PPP data
		switch (m_state)
		{
			case ModemState::V34:
			case ModemState::V90:
				break;

			default:
				break;
		}

	}
}

void wtvsoftmodem_device::dcd_w(int state)
{
	//
}

void wtvsoftmodem_device::dsr_w(int state)
{
	//
}

void wtvsoftmodem_device::ri_w(int state)
{
	//
}

void wtvsoftmodem_device::cts_w(int state)
{
	//
}

template <wtvsoftmodem_sample_buffer T>
uint32_t wtvsoftmodem_device::pull(T* buffer, uint32_t sample_count)
{
	int16_t* sample;
	if constexpr (std::is_same_v<T, uint32_t>)
	{
		m_sample_scratch.assign(sample_count, 0);
		sample = m_sample_scratch.data();
	}
	else
	{
		sample = buffer;
		std::fill_n(sample, sample_count, 0);
	}

	uint32_t tx_sample_count = 0;

	switch (m_state)
	{
		case ModemState::IDLE_ON_HOOK:
		case ModemState::CALLER_DIALING:
			tx_sample_count = sample_count;
			break;

		case ModemState::IDLE_OFF_HOOK:
			tx_sample_count = m_tone.tx(sample, sample_count);
			break;

		case ModemState::WAITING:
		{
			tx_sample_count = sample_count;
			m_wait_samples -= sample_count;
			if(m_wait_samples <= 0)
				m_state = m_wait_end_state;
			break;
		}

		case ModemState::V8:
			tx_sample_count = m_v8.tx(sample, sample_count);
			break;

		case ModemState::V22:
			tx_sample_count = m_v22.tx(sample, sample_count);
			break;

		case ModemState::V23:
			tx_sample_count = m_v23.tx(sample, sample_count);
			break;

		default:
			break;
	}

	if (m_can_echo)
	{
		const uint32_t base = (m_echo_idx - tx_sample_count - wtvsoftmodem_device::ECHO_DELAY_SAMPLE_CNT);
		if (base > 0)
		{
			for (uint32_t sample_idx = 0; sample_idx < tx_sample_count; sample_idx++)
			{
				int32_t echo_sample = (int32_t)m_echo[(base + sample_idx) & (wtvsoftmodem_device::ECHO_SAMPLE_CNT - 1)];

				sample[sample_idx] = (wtvsoftmodem_dsp::sample_t)std::clamp(
					(int32_t)sample[sample_idx] + (echo_sample / wtvsoftmodem_device::ECHO_SCALEDOWN),
					static_cast<int32_t>(std::numeric_limits<wtvsoftmodem_dsp::sample_t>::min()),
					static_cast<int32_t>(std::numeric_limits<wtvsoftmodem_dsp::sample_t>::max())
				);
			}
		}
	}

	if constexpr (std::is_same_v<T, uint32_t>)
	{
		for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
			buffer[sample_idx] = uint16_t(sample[sample_idx]);
	}

	return tx_sample_count;
}
template uint32_t wtvsoftmodem_device::pull<uint32_t>(uint32_t* buffer, uint32_t sample_count);
template uint32_t wtvsoftmodem_device::pull<wtvsoftmodem_dsp::sample_t>(wtvsoftmodem_dsp::sample_t* buffer, uint32_t sample_count);

void wtvsoftmodem_device::restart(wtvsoftmodem_tone::tone_t dial_tone)
{
	if (m_state != ModemState::IDLE_ON_HOOK)
	{
		m_state = ModemState::IDLE_OFF_HOOK;

		m_tone.tx_init(dial_tone);
	}
}

bool wtvsoftmodem_device::is_on_hook()
{
	return (m_state == ModemState::IDLE_ON_HOOK);
}

void wtvsoftmodem_device::set_on_hook()
{
	wtvsoftmodem_device::reset();
}

void wtvsoftmodem_device::set_off_hook(wtvsoftmodem_tone::tone_t dial_tone)
{
	if (m_state == ModemState::IDLE_ON_HOOK)
	{
		m_state = ModemState::IDLE_OFF_HOOK;

		m_dial_digits.clear();

		wtvsoftmodem_device::reset_echo(true);

		wtvsoftmodem_device::restart(dial_tone);
	}
}

void wtvsoftmodem_device::set_is_waiting(uint32_t wait_ms, ModemState wait_end_state)
{
	m_state = ModemState::WAITING;
	m_wait_samples = (wait_ms * m_sample_rate)/1000;
	m_wait_end_state = wait_end_state;
}

void wtvsoftmodem_device::reset_echo(bool can_echo)
{
	m_can_echo = can_echo;

	m_echo.fill(0);
	m_echo_idx = 0;
}

void wtvsoftmodem_device::set_is_dialing()
{
	m_state = ModemState::CALLER_DIALING;

	m_tone.tx_init(wtvsoftmodem_tone::TONE_NULL);
}

void wtvsoftmodem_device::listen_for_dial_string(const wtvsoftmodem_dsp::sample_t* sample, uint32_t sample_count)
{
	wtvsoftmodem_tone::detected_tone_t dt = m_tone.detect_dtmf(sample, sample_count);
	if (m_tone.is_new_tone(dt))
	{
#if SOFTMODEM_DEBUG
		printf("MAME MESSAGE-> Detected DTMF digit: %c\n", m_tone.get_tone_digit(dt));
#endif
		dial_completed_timer->adjust(attotime::from_msec(WAIT_AFTER_DIAL_SILENCE_MS));
		m_dial_digits += m_tone.get_tone_digit(dt);
		if (m_state == ModemState::IDLE_OFF_HOOK)
			wtvsoftmodem_device::set_is_dialing();
	}
}

void wtvsoftmodem_device::enter_data_mode()
{
	data_start_timer->adjust(attotime::from_msec(WAIT_BEFORE_DATA_MS));
}

template <wtvsoftmodem_sample_buffer T>
void wtvsoftmodem_device::push(const T* buffer, uint32_t sample_count)
{
	const int16_t* sample;
	if constexpr (std::is_same_v<T, uint32_t>)
	{
		m_sample_scratch.resize(sample_count);
		for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
			m_sample_scratch[sample_idx] = wtvsoftmodem_dsp::sample_t(uint16_t(buffer[sample_idx]));
		sample = m_sample_scratch.data();
	}
	else
	{
		sample = buffer;
	}

	// Add an echo so audible dialing doesn't sound too weird on a zero-echo channel (things like hearing the dialed digit)
	// Echo gets turned off after V.8 to not disturb training.
	// These samples are mixed with pull()'d samples to generage the echo.
	if (m_can_echo)
	{
		for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
			m_echo[m_echo_idx++ & (wtvsoftmodem_device::ECHO_SAMPLE_CNT - 1)] = sample[sample_idx];
	}

	switch (m_state)
	{
		case ModemState::IDLE_ON_HOOK:
			break;

		case ModemState::IDLE_OFF_HOOK:
		case ModemState::CALLER_DIALING:
			wtvsoftmodem_device::listen_for_dial_string(sample, sample_count);
			break;

		case ModemState::WAITING:
			break;

		case ModemState::V8:
			m_v8.rx(sample, sample_count);
			break;

		case ModemState::V22:
			m_v22.rx(sample, sample_count);
			break;

		case ModemState::V23:
			m_v23.rx(sample, sample_count);
			break;

		default:
			break;
	}
}
template void wtvsoftmodem_device::push<uint32_t>(const uint32_t* buffer, uint32_t sample_count);
template void wtvsoftmodem_device::push<wtvsoftmodem_dsp::sample_t>(const wtvsoftmodem_dsp::sample_t* buffer, uint32_t sample_count);

TIMER_CALLBACK_MEMBER(wtvsoftmodem_device::dial_completed)
{
	m_has_dialed = true;

	m_state = ModemState::V8;
	m_v8.start();
}

TIMER_CALLBACK_MEMBER(wtvsoftmodem_device::start_data)
{
	m_dte_in_atd = true;

	std::string_view at_command(reinterpret_cast<const char*>(touchppp_data_command), sizeof(touchppp_data_command) - 1);
	for (char cmd_char : at_command)
		device_buffered_serial_interface::transmit_byte(static_cast<uint8_t>(cmd_char));
}
