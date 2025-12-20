// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34.h"
#include <cstdio>

wtvsoftmodem_v34::wtvsoftmodem_v34() :
	wtvsoftmodem_fsk()
{
	wtvsoftmodem_fsk::set_sample_rate(wtvsoftmodem_dsp::DEFAULT_SAMPLE_RATE);

	wtvsoftmodem_fsk::init(wtvsoftmodem_fsk::FSK_V23_MODE2);

	wtvsoftmodem_fsk::set_tx_enabled(false);
}

void wtvsoftmodem_v34::push_rx_bit(bool bit)
{
	//
}

bool wtvsoftmodem_v34::pop_tx_bit()
{
	return 0;
}
