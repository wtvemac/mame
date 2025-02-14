
#include "emu.h"
#include "wtvir.h"
#include "natkeyboard.h"

wtvir_device_base::wtvir_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, type, tag, owner, clock),
	m_sample_fifo_trigger_cb(*this)
{
}

TIMER_CALLBACK_MEMBER(wtvir_device_base::poll_buttons)
{
	polling();
}

void wtvir_device_base::device_start()
{
	save_item(NAME(m_queued_button_head));
	save_item(NAME(m_queued_button_tail));

	m_queued_button_head = 0x0;
	m_queued_button_tail = 0x0;
	
	m_fifo_data_bit_count = 7;
	m_waiting_for_fifo_read = false;

	m_irin_sample_interval = 0x0;
	m_irin_reject_interval = 0x0;
	m_irin_statcntl = 0x0;
	m_irin_bit_sample_clock_cnt = DEFAULT_BIT_SAMPLE_CLOCKS;

	m_input_timer = timer_alloc(FUNC(wtvir_device_base::poll_buttons), this);
}

void wtvir_device_base::device_reset()
{
	m_input_timer->adjust(attotime::from_msec(5), 0, attotime::from_hz(60));

	m_irin_statcntl ^= 0x2; // set resit bit back to 0
}

void wtvir_device_base::enable(int state)
{
	if (state)
	{
		m_input_timer->adjust(attotime::from_msec(5), 0, attotime::from_hz(60));
	}
	else
	{
		m_input_timer->adjust(attotime::never);
	}
}

uint32_t wtvir_device_base::data_r(offs_t offset)
{
	uint32_t result = 0x00000000;

	switch (offset)
	{
		case DEV_IROLD:
			// not implemented
			result |= ((IR_MICROCODE_VERSION & 0x00ff) << 16);
			break;

		case DEV_IRIN_SAMPLE:
			result = m_irin_sample_interval & 0xffff;
			break;

		case DEV_IRIN_REJECT_INT:
			result = m_irin_reject_interval & 0xff;
			break;

		case DEV_IRIN_TRANS_DATA:
			{
				ir_button_state_t* active_key = current_button();

				if (active_key != NULL)
				{
					bool bit_val = ((active_key->ir_data & (1 << active_key->trans_bit_index)) != 0x0);
					uint8_t bit_cnt = 1;
					
					for (uint8_t sample_fifo_idx = 0; sample_fifo_idx < wtvir_device_base::MAX_SAMPLE_FIFO_ENTRIES; sample_fifo_idx++)
					{
						active_key->trans_bit_index++;

						if (active_key->trans_bit_index < m_fifo_data_bit_count)
						{
							bool this_bit_val = ((active_key->ir_data & (1 << active_key->trans_bit_index)) != 0x0);

							if (this_bit_val != bit_val)
							{
								break;
							}
							else
							{
								bit_cnt++;
							}
						}
						else
						{
							break;
						}
					}

					uint16_t sample_clock_cnt = bit_cnt * m_irin_bit_sample_clock_cnt;

					uint8_t fifo_samples_left = 0x0;
					// It needs to be less 2 because less 1 (meaning we're full: current + max-1 left) makes the WebTV OS/firmware reset thinking there's an overflow
					uint8_t fifo_samples_left_max = (MAX_SAMPLE_FIFO_ENTRIES - 2);

					if (queued_button_count() > 1)
					{
						// If we have more than one button event queued then set to max minus 2.
						fifo_samples_left = fifo_samples_left_max;
					}
					else
					{
						fifo_samples_left = (m_fifo_data_bit_count - active_key->trans_bit_index);
						if (fifo_samples_left > fifo_samples_left_max)
						{
							fifo_samples_left = fifo_samples_left_max;
						}
					}

					//
					//  IR transition register data bits:
					//
					//  SSSS | V | TTTTTTTTTTT
					//
					//    SSSS        = the number of transition entries in the FIFO buffer
					//    V           = value of the current bit
					//    TTTTTTTTTTT = the time of the current bit transition measured in sample clocks. 
					//                  1 sample clock is defined in the register DEV_IR_IN_SAMPLE_TICKS 
					//                  which is number of system clock cycles per sample clock.
					//
					result |= ((fifo_samples_left & 0x00f) << 12);
					result |= ((bit_val           & 0x001) << 11);
					result |= ((sample_clock_cnt  & 0x7ff) <<  0);

					if (active_key->trans_bit_index >= m_fifo_data_bit_count)
					{
						dequeue_button();
					}
				}

				m_waiting_for_fifo_read = false;
			}
			break;

		case DEV_IRIN_STATCNTL:
			result = m_irin_statcntl & 0xff;
			break;
	}

	return result;
}

void wtvir_device_base::data_w(offs_t offset, uint32_t data)
{
	switch (offset)
	{
		case DEV_IRIN_SAMPLE:
			m_irin_sample_interval = data & 0xffff;
			break;

		case DEV_IRIN_REJECT_INT:
			m_irin_reject_interval = data & 0xff;
			break;

		case DEV_IRIN_STATCNTL:
			{
				m_irin_statcntl = data & 0xff;

				if (m_irin_statcntl & 0x2)
				{
					device_reset();
				}
			}
			break;
	}
}

bool wtvir_device_base::enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data)
{
	if (queued_button_count() < wtvir_device_base::MAX_QUEUED_BUTTONS && ((m_queued_button_head + 1) != m_queued_button_tail))
	{
		m_queued_buttons[m_queued_button_head] = {
			.scancode = scancode,
			.is_make = is_make,
			.trans_bit_index = 0x0,
			.ir_data = ir_data
		};

		m_queued_button_head++;
		m_queued_button_head &= (wtvir_device_base::MAX_QUEUED_BUTTONS - 1);

		return true;
	}
	else
	{
		return false;
	}
}

uint8_t wtvir_device_base::queued_button_count()
{
	return (m_queued_button_head - m_queued_button_tail) & (wtvir_device_base::MAX_QUEUED_BUTTONS - 1);
}

wtvir_device_base::ir_button_state_t* wtvir_device_base::current_button()
{
	if (m_queued_button_head != m_queued_button_tail)
	{
		return &m_queued_buttons[m_queued_button_tail];
	}
	else
	{
		return NULL;
	}
}

wtvir_device_base::ir_button_state_t* wtvir_device_base::dequeue_button()
{
	if (m_queued_button_head != m_queued_button_tail)
	{
		m_queued_button_tail++;
		m_queued_button_tail &= (wtvir_device_base::MAX_QUEUED_BUTTONS - 1);

		return &m_queued_buttons[m_queued_button_tail];
	}
	else
	{
		return NULL;
	}
}

void wtvir_device_base::polling()
{
	if (queued_button_count() >= 1 && !m_waiting_for_fifo_read)
	{
		m_waiting_for_fifo_read = true;
		m_sample_fifo_trigger_cb(1);
	}
}

DEFINE_DEVICE_TYPE(SEIJIN_KBD, wtvir_seijin_device, "seijinkbd", "Seijin IR Keyboard")

wtvir_seijin_device::wtvir_seijin_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	wtvir_device_base(mconfig, SEIJIN_KBD, tag, owner, clock),
	m_ioport(*this, "wtvir_kbd%u", 0)
{
}

void wtvir_seijin_device::device_start()
{
	wtvir_device_base::device_start();

	save_item(NAME(m_device_id));
	save_item(NAME(m_port_state));

	m_device_id = 0x01;

	m_fifo_data_bit_count = wtvir_seijin_device::SEIJIN_DATA_BIT_COUNT;

	std::fill(std::begin(m_port_state), std::end(m_port_state), 0);
}

void wtvir_seijin_device::device_reset()
{
	wtvir_device_base::device_reset();
}

uint8_t wtvir_seijin_device::calculate_odd_parity(uint32_t data, uint8_t bit_start, uint8_t bit_count)
{
	uint8_t parity_bit = 0x0;

	uint8_t bit_end = (bit_start + bit_count);

	if (bit_end < 0x20)
	{
		uint8_t bit_count = 0;

		for (uint8_t bit_idx = bit_start; bit_idx < bit_end; bit_idx++)
		{
			if ((data & (1 << bit_idx)) != 0x0)
			{
				bit_count++;
			}
		}

		if ((bit_count & 0x1) != 0x0)
		{
			parity_bit = 0x1;
		}
		else
		{
			parity_bit = 0x0;
		}
	}

	return parity_bit;
}

bool wtvir_seijin_device::enqueue_button(uint8_t scancode, bool is_make, uint32_t ir_data)
{
	//
	//  Seijin IR keyboard data bits:
	//
	//  [1 P 0 SSSSSSS 0] [1 P 1 M DD 0100 0]
	//
	//    DD      = ID bit, usually 10 for WebTV's keyboard (10 = 103/86 keyboard)
	//    M       = 0=make (key press), 1=break (key release)
	//    P       = odd parity bit
	//    SSSSSSS = scancode for the key pressed
	//
	ir_data |= ((m_device_id & 0x03) <<  5);
	ir_data |= ((scancode    & 0x7f) << 12);
	ir_data |= ((is_make     & 0x01) <<  7);

	ir_data |= ((calculate_odd_parity(ir_data,  0, 11) & 0x1) <<  9);
	ir_data |= ((calculate_odd_parity(ir_data, 11, 11) & 0x1) << 20);

	return wtvir_device_base::enqueue_button(scancode, is_make, ir_data);
}

void wtvir_seijin_device::polling()
{
	if (queued_button_count() < wtvir_device_base::MAX_QUEUED_BUTTONS)
	{
		for (uint8_t port_idx = 0x0; port_idx < (wtvir_seijin_device::SEIJIN_SCANCODE_COUNT >> 4); port_idx++)
		{
			uint32_t prev_state = m_port_state[port_idx];
			uint32_t curr_state = readport(port_idx);

			uint32_t state_diff = prev_state ^ curr_state;

			if (state_diff != 0x0)
			{
				for (uint8_t key_idx = 0x0; key_idx < 0x10; key_idx++)
				{
					uint32_t key_bitmask = (0x1 << key_idx);

					if (state_diff & key_bitmask)
					{
						uint8_t scancode = ((port_idx << 4) | key_idx);
						bool is_make = ((prev_state & key_bitmask) != 0x0);

						enqueue_button(scancode, is_make, wtvir_seijin_device::SEIJIN_DEFAULT_IR_DATA);
					}
				}
			}

			m_port_state[port_idx] = curr_state;
		}
	}

	// Only transmitting once can cause issues with modifier keys like shift. Will need to look into other methods.

	if (queued_button_count() >= 1 && !m_waiting_for_fifo_read)
	{
		m_waiting_for_fifo_read = true;
		m_sample_fifo_trigger_cb(1);
	}
}

uint32_t wtvir_seijin_device::readport(int port)
{
	if ((port < m_ioport.size()) && m_ioport[port].found())
	{
		return m_ioport[port]->read();
	}
	else
	{
		return 0x0;
	}
}

INPUT_PORTS_START(wtvir_kbd)

	PORT_START("wtvir_kbd0")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x00 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x01 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x02 UNUSED */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x03 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("` ~")             PORT_CODE(KEYCODE_TILDE)      PORT_CHAR('`')                     PORT_CHAR('~')      /* 0x04 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: View")       PORT_CODE(KEYCODE_F12)        PORT_CHAR(UCHAR_MAMEKEY(F12))                          /* 0x05 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x06 UNUSED */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x07 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x08 UNUSED */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Caps Lock")       PORT_CODE(KEYCODE_CAPSLOCK)                                                          /* 0x09 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("X")               PORT_CODE(KEYCODE_X)          PORT_CHAR('X')                                         /* 0x0a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0b UNUSED */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0c UNUSED */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("2 @")             PORT_CODE(KEYCODE_2)          PORT_CHAR('2')                     PORT_CHAR('@')      /* 0x0d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("S")               PORT_CODE(KEYCODE_S)          PORT_CHAR('S')                                         /* 0x0e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("W")               PORT_CODE(KEYCODE_W)          PORT_CHAR('W')                                         /* 0x0f */

	PORT_START("wtvir_kbd1")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x10 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x11 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("C")               PORT_CODE(KEYCODE_C)          PORT_CHAR('C')                                         /* 0x12 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x13 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x14 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("3 #")             PORT_CODE(KEYCODE_3)          PORT_CHAR('3')                     PORT_CHAR('#')      /* 0x15 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("D")               PORT_CODE(KEYCODE_D)          PORT_CHAR('D')                                         /* 0x16 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("E")               PORT_CODE(KEYCODE_E)          PORT_CHAR('E')                                         /* 0x17 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Alt")       PORT_CODE(KEYCODE_RALT)                                                              /* 0x18 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Tab")             PORT_CODE(KEYCODE_TAB)        PORT_CHAR(0x09)                                        /* 0x19 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Z")               PORT_CODE(KEYCODE_Z)          PORT_CHAR('Z')                                         /* 0x1a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Power")      PORT_CODE(KEYCODE_F1)         PORT_CHAR(UCHAR_MAMEKEY(F1))                           /* 0x1b (not used but F1 is 0x0c) */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Command")    PORT_CODE(KEYCODE_LCONTROL)   PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))                     /* 0x1c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("1 !")             PORT_CODE(KEYCODE_1)          PORT_CHAR('1')                      PORT_CHAR('!')     /* 0x1d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("A")               PORT_CODE(KEYCODE_A)          PORT_CHAR('A')                                         /* 0x1e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Q")               PORT_CODE(KEYCODE_Q)          PORT_CHAR('Q')   

	PORT_START("wtvir_kbd2")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("B")               PORT_CODE(KEYCODE_B)          PORT_CHAR('B')                                         /* 0x20 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("T")               PORT_CODE(KEYCODE_T)          PORT_CHAR('T')                                         /* 0x21 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("V")               PORT_CODE(KEYCODE_V)          PORT_CHAR('V')                                         /* 0x22 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("G")               PORT_CODE(KEYCODE_G)          PORT_CHAR('G')                                         /* 0x23 */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("5 %")             PORT_CODE(KEYCODE_5)          PORT_CHAR('5')                      PORT_CHAR('%')     /* 0x24 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("4 $")             PORT_CODE(KEYCODE_4)          PORT_CHAR('4')                      PORT_CHAR('$')     /* 0x25 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("F")               PORT_CODE(KEYCODE_F)          PORT_CHAR('F')                                         /* 0x26 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("R")               PORT_CODE(KEYCODE_R)          PORT_CHAR('R')                                         /* 0x27 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Alt")        PORT_CODE(KEYCODE_LALT)                                                              /* 0x28 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Esc")             PORT_CODE(KEYCODE_ESC)                                                               /* 0x29 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x2a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Control")         PORT_CODE(KEYCODE_RCONTROL)   PORT_CHAR(UCHAR_MAMEKEY(RCONTROL))                     /* 0x2b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0c UNUSED */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Goto")       PORT_CODE(KEYCODE_F7)         PORT_CHAR(UCHAR_MAMEKEY(F7))                           /* 0x2d (not used but F7 is 0x61) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x0e UNUSED */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Save")       PORT_CODE(KEYCODE_F8)         PORT_CHAR(UCHAR_MAMEKEY(F8))                           /* 0x2f */

	PORT_START("wtvir_kbd3")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x30 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Shift")      PORT_CODE(KEYCODE_LSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(LSHIFT))                       /* 0x31 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Shift")     PORT_CODE(KEYCODE_RSHIFT)     PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))                       /* 0x32 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x33 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x34 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x35 UNUSED */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Channel Down")    PORT_CODE(KEYCODE_PRTSCR)                                                            /* 0x36 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Channel Up")      PORT_CODE(KEYCODE_PAUSE)                                                             /* 0x37 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Space")           PORT_CODE(KEYCODE_SPACE)      PORT_CHAR(' ')                                         /* 0x38 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Backspace")       PORT_CODE(KEYCODE_BACKSPACE)  PORT_CHAR(0x08)                                        /* 0x39 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Return")          PORT_CODE(KEYCODE_ENTER)      PORT_CHAR(0x0d)                                        /* 0x3a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x3b UNUSED */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Home")       PORT_CODE(KEYCODE_7_PAD)      PORT_CODE(KEYCODE_HOME)                                /* 0x3c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Search")     PORT_CODE(KEYCODE_F3)         PORT_CHAR(UCHAR_MAMEKEY(F3))                           /* 0x3d (not used but F3 is 0x11) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("\\ |")            PORT_CODE(KEYCODE_BACKSLASH)  PORT_CHAR('\\')                     PORT_CHAR('|')     /* 0x3e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x3f UNUSED */

	PORT_START("wtvir_kbd4")
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x41 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x42 UNUSED */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x43 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x44 UNUSED */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x45 UNUSED */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x46 (WebTV's FN key) */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x47 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left")           PORT_CODE(KEYCODE_LEFT)       PORT_CODE(KEYCODE_4_PAD)                                /* 0x48 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x49 UNUSED */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Save")      PORT_CODE(KEYCODE_F9)         PORT_CHAR(UCHAR_MAMEKEY(F9))                            /* 0x4a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Up")             PORT_CODE(KEYCODE_UP)         PORT_CODE(KEYCODE_8_PAD)                                /* 0x4b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Edit")      PORT_CODE(KEYCODE_INSERT)     PORT_CODE(KEYCODE_0_PAD)                                /* 0x4c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x4d UNUSED */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Back")      PORT_CODE(KEYCODE_1_PAD)      PORT_CODE(KEYCODE_END)                                  /* 0x4e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x4f UNUSED */


	PORT_START("wtvir_kbd5")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Down")           PORT_CODE(KEYCODE_DOWN)       PORT_CODE(KEYCODE_2_PAD)                                /* 0x50 */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x51 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Info")      PORT_CODE(KEYCODE_F6)         PORT_CHAR(UCHAR_MAMEKEY(F6))                            /* 0x52 (not used but F6 is 0x3b) */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x53 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Options")   PORT_CODE(KEYCODE_F11)        PORT_CHAR(UCHAR_MAMEKEY(F11))                           /* 0x54 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Mail")      PORT_CODE(KEYCODE_F4)         PORT_CHAR(UCHAR_MAMEKEY(F4))                            /* 0x55 (not used but F4 is 0x13) */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Scroll Up")      PORT_CODE(KEYCODE_PGUP)       PORT_CODE(KEYCODE_9_PAD)                                /* 0x56 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x57 UNUSED */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right")          PORT_CODE(KEYCODE_RIGHT)      PORT_CODE(KEYCODE_6_PAD)                                /* 0x58 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x59 UNUSED */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Scroll Down")    PORT_CODE(KEYCODE_PGDN)       PORT_CODE(KEYCODE_3_PAD)                                /* 0x5b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Recents")   PORT_CODE(KEYCODE_F10)        PORT_CHAR(UCHAR_MAMEKEY(F10))                           /* 0x5c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Find")      PORT_CODE(KEYCODE_F5)         PORT_CHAR(UCHAR_MAMEKEY(F5))                            /* 0x5d (not used but F5 is 0x0b) */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5e UNUSED */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x5f UNUSED */

	PORT_START("wtvir_kbd6")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x60 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x61 UNUSED */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(". >")             PORT_CODE(KEYCODE_STOP)       PORT_CHAR('.')                      PORT_CHAR('>')     /* 0x62 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x63 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("WTV: Favorites")  PORT_CODE(KEYCODE_F2)         PORT_CHAR(UCHAR_MAMEKEY(F2))                           /* 0x64 (not used but F2 is 0x14) */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("9 (")             PORT_CODE(KEYCODE_9)          PORT_CHAR('9')                      PORT_CHAR('(')     /* 0x65 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("L")               PORT_CODE(KEYCODE_L)          PORT_CHAR('L')                                         /* 0x66 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("O")               PORT_CODE(KEYCODE_O)          PORT_CHAR('O')                                         /* 0x67 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("/ ?")             PORT_CODE(KEYCODE_SLASH)      PORT_CHAR('/')                      PORT_CHAR('?')     /* 0x68 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("[ {")             PORT_CODE(KEYCODE_OPENBRACE)  PORT_CHAR('[')                      PORT_CHAR('{')     /* 0x69 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x6a UNUSED */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("' \"")            PORT_CODE(KEYCODE_QUOTE)      PORT_CHAR('\'')                     PORT_CHAR('\"')    /* 0x6b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("- _")             PORT_CODE(KEYCODE_MINUS)      PORT_CHAR('-')                      PORT_CHAR('_')     /* 0x6c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("0 )")             PORT_CODE(KEYCODE_0)          PORT_CHAR('0')                      PORT_CHAR(')')     /* 0x6d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("; :")             PORT_CODE(KEYCODE_COLON)      PORT_CHAR(';')                      PORT_CHAR(':')     /* 0x6e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("P")               PORT_CODE(KEYCODE_P)          PORT_CHAR('P')                                         /* 0x6f */

	PORT_START("wtvir_kbd7")
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x70 UNUSED */
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("] }")             PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']')                      PORT_CHAR('}')     /* 0x71 */
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(", <")             PORT_CODE(KEYCODE_COMMA)      PORT_CHAR(',')                      PORT_CHAR('<')     /* 0x72 */
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_UNUSED)                                                                                                                     /* 0x73 UNUSED */
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("= +")             PORT_CODE(KEYCODE_EQUALS)     PORT_CHAR('=')                      PORT_CHAR('+')     /* 0x74 */
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("8 *")             PORT_CODE(KEYCODE_8)          PORT_CHAR('8')                      PORT_CHAR('*')     /* 0x75 */
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("K")               PORT_CODE(KEYCODE_K)          PORT_CHAR('K')                                         /* 0x76 */
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("I")               PORT_CODE(KEYCODE_I)          PORT_CHAR('I')                                         /* 0x77 */
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("N")               PORT_CODE(KEYCODE_N)          PORT_CHAR('N')                                         /* 0x78 */
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Y")               PORT_CODE(KEYCODE_Y)          PORT_CHAR('Y')                                         /* 0x79 */
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("M")               PORT_CODE(KEYCODE_M)          PORT_CHAR('M')                                         /* 0x7a */
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")               PORT_CODE(KEYCODE_H)          PORT_CHAR('H')                                         /* 0x7b */
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("6 ^")             PORT_CODE(KEYCODE_6)          PORT_CHAR('6')                      PORT_CHAR('^')     /* 0x7c */
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("7 &")             PORT_CODE(KEYCODE_7)          PORT_CHAR('7')                      PORT_CHAR('&')     /* 0x7d */
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("J")               PORT_CODE(KEYCODE_J)          PORT_CHAR('J')                                         /* 0x7e */
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("H")               PORT_CODE(KEYCODE_U)          PORT_CHAR('U')                                         /* 0x7f */

INPUT_PORTS_END

ioport_constructor wtvir_seijin_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(wtvir_kbd);
}