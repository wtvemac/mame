// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "emuopts.h"
#include "wtvsoftmodem.h"
#include <algorithm>
#include <cmath>

DEFINE_DEVICE_TYPE(WTVSOFTMODEM, wtvsoftmodem_device, "wtvsoftmodem_device", "WebTV Softmodem")

wtvsoftmodem_device::wtvsoftmodem_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVSOFTMODEM, tag, owner, clock),
	device_buffered_serial_interface(mconfig, *this),
	m_out_tx_cb(*this),
	m_out_dtr_cb(*this),
	m_out_rts_cb(*this),
	m_out_int_cb(*this),
	m_moip(*this, "moip"),
	m_wav_recorder_enabled(false)
{
}

void wtvsoftmodem_device::device_add_mconfig(machine_config &config)
{
	MOIPBRIDGE(config, m_moip, 0, wtvsoftmodem_device::DEFAULT_SAMPLE_RATE);
}

void wtvsoftmodem_device::device_start()
{
	dial_completed_timer = timer_alloc(FUNC(wtvsoftmodem_device::dial_completed), this);
	data_start_timer = timer_alloc(FUNC(wtvsoftmodem_device::start_data), this);

	m_wav_recorder_enabled = device().machine().options().sm_record();

	save_item(NAME(m_sample_rate));
	//save_item(NAME(m_state));
}

void wtvsoftmodem_device::device_reset()
{
	// EMAC NOTE: Clean this up. This function needs to be broken up.

	wtvsoftmodem_device::reset();

	device_buffered_serial_interface::set_data_frame(1, 8, PARITY_NONE, STOP_BITS_1);
	device_buffered_serial_interface::set_rate(wtvsoftmodem_device::DEFAULT_DTE_BUAD_RATE);

	wtvsoftmodem_device::set_clock(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE);

	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_DATMODEM);

	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V23FULLD);

	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD);

	configure_call({ });

	m_out_dtr_cb(1);

	m_v23.set_rx_byte_callback([this] (uint8_t byte)
	{
		device_buffered_serial_interface::transmit_byte(byte);
	});

	m_v34fam.set_answer_mode(true);
	m_v34fam.set_rx_byte_callback([this] (uint8_t byte)
	{
		m_dte_pending.push_back(byte);
		drain_dte_pending();
	});
	m_v34fam.set_connected_callback([this] ()
	{
		if (!m_dte_in_atd)
			wtvsoftmodem_device::enter_data_mode();
	});

	m_v8.set_state_change_callback([this] (wtvsoftmodem_v8::V8State old_state, wtvsoftmodem_v8::V8State new_state)
	{
		if (m_wav_recorder_enabled)
			m_wav_recorder.annotate(std::format("V.8 state to '{}'", wtvsoftmodem_v8::v8_state_to_string(new_state)));

		if (new_state == wtvsoftmodem_v8::V8State::RCV_CM)
		{
			if (m_wav_recorder_enabled)
			{
				std::string peer_caps = "";

				auto in_caps = m_v8.get_peer_capabilities();
				for (const auto &in_cap : in_caps)
					peer_caps += std::format(" 0x{:04x}", in_cap);

				m_wav_recorder.annotate("V.8 peer capabilities:" +  peer_caps);

				std::string joint_caps = "";

				for (const auto &joint_cap : m_v8.get_joint_capabilities())
					joint_caps += std::format(" 0x{:04x}", joint_cap);

				m_wav_recorder.annotate("V.8 joint capabilities:" +  joint_caps);
			}
		}
		else if (new_state == wtvsoftmodem_v8::V8State::NEGOTIATED)
		{
			if (new_state == wtvsoftmodem_v8::V8State::NEGOTIATED)
			{
				if (m_v8.is_peer_capable(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD))
				{
					m_dte_byte_tx = true;

					wtvsoftmodem_device::reset_echo(false);

					ModemState next_state;
					const bool can_pcm = m_v8.is_joint_capable(wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM, wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM);
					const bool can_v90 = m_v8.is_joint_capable(wtvsoftmodem_v8::V8_MENU_PCMMD_V90DIGIT, wtvsoftmodem_v8::V8_MENU_PCMMD_V90ANLOG);
					if (can_pcm && can_v90)
					{
						next_state = ModemState::V90;

						m_v34fam.set_v90_mode(true);

						if (m_wav_recorder_enabled)
							m_wav_recorder.annotate("V.8 negotiated, going to V.90");
					}
					else
					{
						next_state = ModemState::V34;

						// A previous V.90 call leaves V.90 mode set.
						if (m_v34fam.is_v90_mode())
							m_v34fam.set_v90_mode(false);

						if (m_wav_recorder_enabled)
							m_wav_recorder.annotate("V.8 negotiated, going to V.34");
					}

					wtvsoftmodem_device::set_is_waiting(wtvsoftmodem_device::WAIT_BEFORE_TRAINING_MS, next_state);

					m_v34fam.start();
				}
				else
				{
					if (m_wav_recorder_enabled)
						m_wav_recorder.annotate("V.8 negotiated, going to V.23");

					m_dte_byte_tx = false;
					m_state = ModemState::V23;
					wtvsoftmodem_device::enter_data_mode();
				}
			}
		}
	});

	m_v34fam.set_state_change_callback([this] (wtvsoftmodem_v34family::V34FamState old_state, wtvsoftmodem_v34family::V34FamState new_state)
	{
		if (m_wav_recorder_enabled)
		{
			m_wav_recorder.annotate(std::format("Modem state to '{}'", wtvsoftmodem_v34family::state_to_string(new_state)));

			static constexpr const char *BAUD_NAMES[] = {
				"2400",
				"2743",
				"2800",
				"3000",
				"3200",
				"3429",
				"pcm",
				"unknown"
			};
			static constexpr const char *TX_CLOCK_NAMES[] = {
				"internal",
				"rx-timing",
				"external",
				"unknown"
			};

			auto freq_offset_str = [] (int16_t freq_offset)
			{
				return (freq_offset == -512) ? std::string("n/a") : std::format("{:.2f}Hz", freq_offset * 0.02);
			};

			auto mp_str = [] (const wtvsoftmodem::v34::info::mp_t &mp)
			{
				static constexpr const char *TRELLIS_NAMES[] = {
					"16",
					"32",
					"64",
					"unknown"
				};

				std::string precoder = "off";
				if (mp.type == 1 && mp.precoding_enabled)
				{
					const auto taps = wtvsoftmodem::v34::info::mp_precoder(mp);
					precoder = std::format("[{},{} {},{} {},{}]", taps[0][0], taps[0][1], taps[1][0], taps[1][1], taps[2][0], taps[2][1]);
				}

				return std::format("MP{}{}: C->A {} / A->C {}, rate mask=0x{:04x}, asym={}, trellis={}, nonlinear={}, shaping={}, aux={}, precoder={}",
					mp.type,
					mp.mp_acknowledged ? "'" : "",
					mp.bit_rate_c_to_a * 2400,
					mp.bit_rate_a_to_c * 2400,
					mp.signalling_rate_mask,
					mp.asymmetric_rates ? "y" : "n",
					TRELLIS_NAMES[mp.trellis_sel & 3],
					mp.nonlinear_encoder ? "y" : "n",
					mp.expanded_shaping ? "y" : "n",
					mp.aux_channel ? "y" : "n",
					precoder
				);
			};

			if (old_state == wtvsoftmodem_v34family::PH2_A_INFO && m_v34fam.has_peer_info0())
			{
				const auto &info0 = m_v34fam.get_peer_info0();

				std::string bauds = "2400";
				if (info0.sr_2743)
					bauds += " 2743";
				if (info0.sr_2800)
					bauds += " 2800";
				if (info0.sr_3000_low || info0.sr_3000_high)
					bauds += std::format(" 3000({}{})", info0.sr_3000_low ? "L" : "", info0.sr_3000_high ? "H" : "");
				if (info0.sr_3200_low || info0.sr_3200_high)
					bauds += std::format(" 3200({}{})", info0.sr_3200_low ? "L" : "", info0.sr_3200_high ? "H" : "");
				if (info0.sr_3429)
					bauds += " 3429";

				m_wav_recorder.annotate(std::format("Peer INFO0: baud [{}], 3429 allowed={}, power reduction={}, max baud diff={}, CME={}, 1664pt={}, tx clock={}, ack={}",
					bauds,
					info0.rate_3429_allowed ? "y" : "n",
					info0.power_reduction ? "y" : "n",
					info0.max_baud_diff,
					info0.from_cme_modem ? "y" : "n",
					info0.support_1664 ? "y" : "n",
					TX_CLOCK_NAMES[info0.tx_clock_source & 3],
					info0.ack ? "y" : "n")
				);
			}
			else if (old_state == wtvsoftmodem_v34family::PH2_INFO1 && m_v34fam.has_peer_info1c())
			{
				const auto &info1c = m_v34fam.get_peer_info1c();

				std::string rates = "";
				for (int32_t r = 0; r < 6; r++)
				{
					const auto &rate = info1c.rate[r];
					rates += std::format(" {}:{}/pe{}/{}", BAUD_NAMES[r], rate.use_high_carrier ? "H" : "L", rate.pre_emphasis, rate.max_bit_rate * 2400);
				}

				m_wav_recorder.annotate(std::format("Peer INFO1c: power reduction={}dB+{}dB, MD={}ms, freq offset={}, baud carrier/pre-emphasis/max bps:{}",
					info1c.power_reduction,
					info1c.add_power_reduction,
					info1c.md * 35,
					freq_offset_str(info1c.freq_offset),
					rates)
				);
			}
			else if (old_state == wtvsoftmodem_v34family::PH2_V90_INFO1A_WAIT && m_v34fam.has_peer_info1a())
			{
				const auto &info1a = m_v34fam.get_peer_info1a();

				std::string mode;
				if (info1a.pcm_downstream)
					mode = std::format("PCM downstream, training ucode=0x{:02x}", info1a.training_ucode);
				else
					mode = std::format("V.34 downstream {} baud, carrier={}, pre-emphasis={}, max bps={}",
							BAUD_NAMES[info1a.baud_rate_c_to_a & 7],
							info1a.use_high_carrier ? "H" : "L",
							info1a.preemphasis_filter,
							info1a.max_data_rate * 2400
						);

				m_wav_recorder.annotate(std::format("Peer INFO1a: {}, upstream {} baud, power reduction={}dB+{}dB, MD={}ms, freq offset={}",
					mode,
					BAUD_NAMES[info1a.baud_rate_a_to_c & 7],
					info1a.power_reduction,
					info1a.add_power_reduction,
					info1a.md * 35,
					freq_offset_str(info1a.freq_offset))
				);
			}
			else if (old_state == wtvsoftmodem_v34family::PH3_MP || old_state == wtvsoftmodem_v34family::PH4_MPWAIT)
			{
				const auto &local_mp = m_v34fam.get_local_mp();

				m_wav_recorder.annotate("Local " + mp_str(local_mp));

				if (m_v34fam.has_peer_mp())
				{
					const auto &peer_mp = m_v34fam.get_peer_mp();

					m_wav_recorder.annotate(std::format("Peer {}, selected C->A {} / A->C {}",
						mp_str(peer_mp),
						wtvsoftmodem::v34::info::mp_selected_rate(local_mp, peer_mp, true) * 2400,
						wtvsoftmodem::v34::info::mp_selected_rate(local_mp, peer_mp, false) * 2400)
					);
				}
			}
		}
	});
}

void wtvsoftmodem_device::device_stop()
{
	if (m_wav_recorder_enabled)
		m_wav_recorder.close();
}

void wtvsoftmodem_device::reset()
{
	if (m_moip->is_enabled())
	{
		m_moip->reset_audio();

		if (m_has_dialed)
			m_moip->hangup();
	}

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

	if (m_wav_recorder_enabled)
		m_wav_recorder.close();

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
	m_v34fam.set_sample_rate(m_sample_rate);

	m_wav_recorder.set_sample_rate(m_sample_rate);
}

void wtvsoftmodem_device::rx_w(int32_t state)
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
				m_v34fam.transmit_byte(byte);
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
		wtvsoftmodem_dsp::clear_samples(sample, sample_count);
	}

	uint32_t tx_sample_count = 0;

	if (m_moip->is_enabled() && m_has_dialed)
	{
		tx_sample_count = m_moip->pull(sample, sample_count);
	}
	else
	{
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

			case ModemState::V34:
			case ModemState::V90:
				tx_sample_count = m_v34fam.tx(sample, sample_count);
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
	}

	if (m_wav_recorder_enabled)
		m_wav_recorder.push_right(sample, tx_sample_count);

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

		if (m_wav_recorder_enabled)
		{
			m_wav_recorder.open(WAV_RECORDER_FILETAG);
			m_wav_recorder.annotate("Modem off hook");
		}

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
	for (uint32_t offset = 0; offset < sample_count; offset += wtvsoftmodem_tone::SAMPLES_PER_HIT)
	{
		wtvsoftmodem_tone::detected_tone_t dt = m_tone.detect_dtmf(&sample[offset], std::min(sample_count - offset, wtvsoftmodem_tone::SAMPLES_PER_HIT));
		if (m_tone.is_new_tone(dt))
		{
			const char detected_digit = m_tone.get_tone_digit(dt);

			dial_completed_timer->adjust(attotime::from_msec(WAIT_AFTER_DIAL_SILENCE_MS));

			m_dial_digits += detected_digit;

			if (m_wav_recorder_enabled)
				m_wav_recorder.annotate(std::format("Detected DTMF digit '{}'", detected_digit));

			if (m_state == ModemState::IDLE_OFF_HOOK)
				wtvsoftmodem_device::set_is_dialing();
		}
	}
}

bool wtvsoftmodem_device::configure_call(std::string_view number)
{
	const bool pcm = std::find(std::begin(V34_NUMBERS), std::end(V34_NUMBERS), number) == std::end(V34_NUMBERS);

	m_v8.add_local_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD);
	for (const auto capability : { wtvsoftmodem_v8::V8_MENU_PCMMD_V90DIGIT, wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM, wtvsoftmodem_v8::V8_MENU_PSTNA_DCEDIGIT })
	{
		if (pcm)
			m_v8.add_local_capability(capability);
		else
			std::erase(m_v8.get_local_capabilities(), capability);
	}

	return pcm;
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
	
	if (m_wav_recorder_enabled)
		m_wav_recorder.push_left(sample, sample_count);


	if (m_moip->is_enabled() && m_has_dialed)
	{
		m_moip->push(sample, sample_count);
		return;
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

		case ModemState::V34:
		case ModemState::V90:
			m_v34fam.rx(sample, sample_count);
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

	if (m_moip->is_enabled())
	{
		if (m_wav_recorder_enabled)
			m_wav_recorder.annotate("Dialed '" +  m_dial_digits + "', calling via MoIP");

		m_state = ModemState::MOIP;

		m_moip->dial(m_dial_digits);
	}
	else
	{
		if (m_wav_recorder_enabled)
			m_wav_recorder.annotate("Dialed '" +  m_dial_digits + "', going to V.8");

		configure_call(m_dial_digits);

		m_state = ModemState::V8;
		m_v8.start();
	}
}

TIMER_CALLBACK_MEMBER(wtvsoftmodem_device::start_data)
{
	m_dte_in_atd = true;

	std::string_view at_command(reinterpret_cast<const char*>(touchppp_data_command), sizeof(touchppp_data_command) - 1);
	for (char cmd_char : at_command)
		device_buffered_serial_interface::transmit_byte(static_cast<uint8_t>(cmd_char));
}
