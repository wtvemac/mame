// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_WTVSOFTMODEM_V34_H
#define MAME_MACHINE_WTVSOFTMODEM_V34_H

#pragma once

#include "wtvsoftmodem_fsk.h"

class wtvsoftmodem_v34 : public wtvsoftmodem_fsk
{

public:

	wtvsoftmodem_v34();

private:

	virtual void push_rx_bit(bool bit) override;

	virtual bool pop_tx_bit() override;

};

#endif // MAME_MACHINE_WTVSOFTMODEM_V34_H