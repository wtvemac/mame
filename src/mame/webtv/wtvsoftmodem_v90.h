// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V90_H
#define MAME_WEBTV_WTVSOFTMODEM_V90_H

#pragma once

#include "wtvsoftmodem_fsk.h"

class wtvsoftmodem_v90 : public wtvsoftmodem_fsk
{

public:

	wtvsoftmodem_v90();

private:

	virtual void push_rx_bit(bool bit) override;

	virtual bool pop_tx_bit() override;

};

#endif // MAME_WEBTV_WTVSOFTMODEM_V90_H