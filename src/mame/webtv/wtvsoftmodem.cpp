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
	m_state = ModemState::IDLE_ON_HOOK;

	device_buffered_serial_interface::set_data_frame(1, 8, PARITY_NONE, STOP_BITS_1);
	device_buffered_serial_interface::set_rate(wtvsoftmodem_device::DEFAULT_DTE_BUAD_RATE);

	wtvsoftmodem_device::set_clock(wtvsoftmodem_device::DEFAULT_SAMPLE_RATE);

	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_DATMODEM);
	//m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V21AVAIL);
	//m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V32AVAIL);
	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V23FULLD);
	//m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V22AVAIL);

	/*
	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_V34FULLD);
	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_CFUNC_V90ANLOG);
	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_MMODE_PCMMODEM);
	m_v8.add_tx_capability(wtvsoftmodem_v8::V8_MENU_PSTNA_DCEANLOG);
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
			auto in_caps = m_v8.get_rx_capabilities();
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

			data_start_timer->adjust(attotime::from_msec(BEFORE_DATA_MS));
		}
	});
}

void wtvsoftmodem_device::tra_callback()
{
	int m_txd = device_buffered_serial_interface::transmit_register_get_data_bit();

	m_out_tx_cb(m_txd);
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
	m_v23.transmit_le_8n1_bit(state & 0x1);
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

uint32_t wtvsoftmodem_device::pull(int32_t* sample, uint32_t sample_count)
{
	uint32_t tx_sample_count = 0;

	switch (m_state)
	{
		case ModemState::IDLE_ON_HOOK:
		case ModemState::CALLER_DIALING:
			memset(sample, 0, sample_count << 2);

			tx_sample_count = sample_count;
			break;

		case ModemState::IDLE_OFF_HOOK:
			tx_sample_count = m_tone.tx(sample, sample_count);
			break;

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

	return tx_sample_count;
}

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
	m_state = ModemState::IDLE_ON_HOOK;
}

void wtvsoftmodem_device::set_off_hook(wtvsoftmodem_tone::tone_t dial_tone)
{
	if (m_state == ModemState::IDLE_ON_HOOK)
	{
		m_state = ModemState::IDLE_OFF_HOOK;

		wtvsoftmodem_device::restart(dial_tone);
	}
}

void wtvsoftmodem_device::set_is_dialing()
{
	m_state = ModemState::CALLER_DIALING;

	m_tone.tx_init(wtvsoftmodem_tone::TONE_NULL);
}

void wtvsoftmodem_device::listen_for_dial_string(const int32_t* sample, uint32_t sample_count)
{
	wtvsoftmodem_tone::detected_tone_t dt = m_tone.detect_dtmf(sample, sample_count);
	if (m_tone.is_new_tone(dt))
	{
		dial_completed_timer->adjust(attotime::from_msec(AFTER_DIAL_SILENCE_MS));
#if SOFTMODEM_DEBUG
		printf("MAME MESSAGE-> Detected DTMF digit: %c\n", m_tone.get_tone_digit(dt));
#endif
		if (m_state == ModemState::IDLE_OFF_HOOK)
		{
			wtvsoftmodem_device::set_is_dialing();
		}
	}
}

void wtvsoftmodem_device::push(const int32_t* sample, uint32_t sample_count)
{
	switch (m_state)
	{
		case ModemState::IDLE_ON_HOOK:
			break;

		case ModemState::IDLE_OFF_HOOK:
		case ModemState::CALLER_DIALING:
			wtvsoftmodem_device::listen_for_dial_string(sample, sample_count);
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

void wtvsoftmodem_device::received_byte(u8 byte)
{
#if SOFTMODEM_DEBUG
	printf("received_byte: byte=%08x\n", byte);
#endif
}

TIMER_CALLBACK_MEMBER(wtvsoftmodem_device::dial_completed)
{
	m_state = ModemState::V8;
	m_v8.start();
}


TIMER_CALLBACK_MEMBER(wtvsoftmodem_device::start_data)
{
	device_buffered_serial_interface::transmit_byte(0x41);
	device_buffered_serial_interface::transmit_byte(0x54);
	device_buffered_serial_interface::transmit_byte(0x44);
	device_buffered_serial_interface::transmit_byte(0x0d);
}

