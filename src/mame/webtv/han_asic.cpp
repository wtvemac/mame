#include "emu.h"
#include "han_asic.h"

DEFINE_DEVICE_TYPE(HAN_ASIC, han_asic_device, "han_asic_device", "WebTV HAN ASIC")

han_asic_device::han_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, HAN_ASIC, tag, owner, clock),
	m_hostcpu(*owner, "maincpu"),
    m_ata(*this, finder_base::DUMMY_TAG)
{
}

void han_asic_device::device_start()
{
	han_message_timer = timer_alloc(FUNC(han_asic_device::check_han_message_state), this);
	han_reboot_timer = timer_alloc(FUNC(han_asic_device::han_reboot), this);

	save_item(NAME(m_han_intenable));
	save_item(NAME(m_han_intstat));
	save_item(NAME(m_han_need_in_int));
	save_item(NAME(m_han_msgbuff_status));
	save_item(NAME(m_han_msgbuff_index));
	save_item(NAME(m_han_startup_step));
	save_item(NAME(m_han_msgbuff));
}

void han_asic_device::device_reset()
{
	m_hostcpu->set_input_line(MIPS3_IRQ3, CLEAR_LINE);
	han_message_timer->enable(false);
	han_reboot_timer->adjust(attotime::from_msec(HAN_REBOOT_WAIT_MS));

	m_han_intenable = 0x0;
	m_han_intstat = 0x0;
	m_han_need_in_int = false;
	m_han_msgbuff_status = 0x0;
	m_han_msgbuff_index = 0x0;
	m_han_startup_step = HAN_STARTUP_BEGIN;
}

void han_asic_device::map(address_map &map)
{
	// Interrupts
	// 0x000, 0x004
	map(0x000, 0x003).rw(FUNC(han_asic_device::reg_han_0000_r), FUNC(han_asic_device::reg_han_0000_w)); // INTSTAT_C
	map(0x004, 0x007).rw(FUNC(han_asic_device::reg_han_0004_r), FUNC(han_asic_device::reg_han_0004_w)); // INTEN_S

	// Unknown
	// 0x014,  0x154,  0x158
	// 0x100 (DMA?), 0x200 (DMA?), 0x2f0 (DMA? DERR, int bit 0xe), 0x300 (DMA?), 0x3f0 (DMA? SERR, int bit 0xf)

	// IDE
	map(0x100, 0x103).rw(FUNC(han_asic_device::reg_ide_000000_r), FUNC(han_asic_device::reg_ide_000000_w)); // IDE I/O port cs0[0] (data)
	map(0x104, 0x107).rw(FUNC(han_asic_device::reg_ide_000004_r), FUNC(han_asic_device::reg_ide_000004_w)); // IDE I/O port cs0[1] (error or feature)
	map(0x108, 0x10b).rw(FUNC(han_asic_device::reg_ide_000008_r), FUNC(han_asic_device::reg_ide_000008_w)); // IDE I/O port cs0[2] (sector count)
	map(0x10c, 0x10f).rw(FUNC(han_asic_device::reg_ide_00000c_r), FUNC(han_asic_device::reg_ide_00000c_w)); // IDE I/O port cs0[3] (sector number)
	map(0x110, 0x113).rw(FUNC(han_asic_device::reg_ide_000010_r), FUNC(han_asic_device::reg_ide_000010_w)); // IDE I/O port cs0[4] (cylinder low)
	map(0x114, 0x117).rw(FUNC(han_asic_device::reg_ide_000014_r), FUNC(han_asic_device::reg_ide_000014_w)); // IDE I/O port cs0[5] (cylinder high)
	map(0x118, 0x11b).rw(FUNC(han_asic_device::reg_ide_000018_r), FUNC(han_asic_device::reg_ide_000018_w)); // IDE I/O port cs0[6] (drive/head)
	map(0x11c, 0x11f).rw(FUNC(han_asic_device::reg_ide_00001c_r), FUNC(han_asic_device::reg_ide_00001c_w)); // IDE I/O port cs0[7] (status or command)
	map(0x138, 0x13b).rw(FUNC(han_asic_device::reg_ide_400018_r), FUNC(han_asic_device::reg_ide_400018_w)); // IDE I/O port cs1[6] (altstatus or device control)
	map(0x13c, 0x13f).rw(FUNC(han_asic_device::reg_ide_40001c_r), FUNC(han_asic_device::reg_ide_40001c_w)); // IDE I/O port cs1[7] (device address)

	// IDE Timing
	// 0x140, 0x144, 0x148

	// Message pipeline (used in Doly AC-3, state messages etc...)
	// 0x10, 0x080, 0x084, 0x180
	map(0x010, 0x013).rw(FUNC(han_asic_device::reg_han_0010_r), FUNC(han_asic_device::reg_han_0010_w)); // _
	map(0x080, 0x083).rw(FUNC(han_asic_device::reg_han_0080_r), FUNC(han_asic_device::reg_han_0080_w)); // MSGCNTL?
	map(0x084, 0x087).rw(FUNC(han_asic_device::reg_han_0084_r), FUNC(han_asic_device::reg_han_0084_w)); // MSGFIFO?
}

void han_asic_device::prepare_for_reset()
{
	if (m_han_startup_step == HAN_STARTUP_DONE)
		han_reboot_timer->adjust(attotime::from_msec(HAN_REBOOT_WAIT_MS));
}

void han_asic_device::irq_ide1_w(int state)
{
	han_asic_device::set_han_irq(HAN_INT_IDE, state);
}

bool han_asic_device::have_queued_han_message()
{
	return (((m_han_msgbuff_status & HAN_MSGCNTL_DATA_WAITING) != 0x0) && m_han_need_in_int);
}

bool han_asic_device::can_send_han_message()
{
	return ((m_han_intenable & HAN_INT_MSG_OUT) != 0x0);
}

bool han_asic_device::send_han_message(uint16_t msg_type, uint16_t msg_subtype, uint8_t* msg, uint16_t msg_size)
{
	if (han_asic_device::can_send_han_message())
	{
		// Bytes 0x000-0x002[uint16]: Message size
		// Bytes 0x002-0x003[uint8]:  Message sequence
		// Bytes 0x003-0x004[uint8]:  Message type
		// Bytes 0x004-0x008[uint16]: Message sub type or id
		// Bytes 0x008-0x100[blob]:  Message-specific data (params etc...)

		memset(m_han_msgbuff, 0x00, HAN_MSGBUFF_SIZE);

		m_han_msgbuff[HAN_MSGSIZE_INDEX] = HAN_MSGBUFF_SIZE;
		m_han_msgbuff[HAN_MSGTYPE_INDEX] = msg_type;
		m_han_msgbuff[HAN_MSGSUBTYPE_INDEX] = msg_subtype;

		if (msg != NULL && msg_size > 0)
		{
			memcpy(&m_han_msgbuff[HAN_MSG_START_INDEX], msg, std::min(msg_size, (uint16_t)((HAN_MSGBUFF_SIZE >> 1) - (sizeof(m_han_msgbuff[0]) * 2))));
		}

		m_han_msgbuff_status = HAN_MSGCNTL_DATA_WAITING;
		m_han_msgbuff_status &= (~(HAN_MSGCNTL_BUFF_OWN | HAN_MSGCNTL_DATA_READ));

		m_han_msgbuff_index = 0x0;

		m_han_need_in_int = true;

		return true;
	}
	else
	{
		return false;
	}
}

void han_asic_device::set_han_irq(uint32_t mask, int state)
{
	if (m_han_intenable & mask)
	{
		if (state)
		{
			m_han_intstat |= mask;

			m_hostcpu->set_input_line(MIPS3_IRQ3, ASSERT_LINE);
		}
		else
		{
			m_han_intstat &= (~mask);

			m_hostcpu->set_input_line(MIPS3_IRQ3, CLEAR_LINE);
		}
	}
}

uint32_t han_asic_device::arrange_han_data(uint32_t data)
{
	// aka std::byteswap, __builtin_bswap32, _byteswap_ulong
	uint32_t han_data = 0x00000000;

	han_data |= ((data >> 0x00) & 0xff) << 0x18;
	han_data |= ((data >> 0x08) & 0xff) << 0x10;
	han_data |= ((data >> 0x10) & 0xff) << 0x08;
	han_data |= ((data >> 0x18) & 0xff) << 0x00;

	return han_data;
}

uint32_t han_asic_device::reg_han_0000_r()
{
	return han_asic_device::arrange_han_data(m_han_intstat);
}

void han_asic_device::reg_han_0000_w(uint32_t data)
{
	data = han_asic_device::arrange_han_data(data);

	han_asic_device::set_han_irq(data, CLEAR_LINE);
}

uint32_t han_asic_device::reg_han_0004_r()
{
	return han_asic_device::arrange_han_data(m_han_intenable);
}

void han_asic_device::reg_han_0004_w(uint32_t data)
{
	data = han_asic_device::arrange_han_data(data);

	m_han_intenable = data;
}

uint32_t han_asic_device::reg_han_0010_r()
{
	return han_asic_device::arrange_han_data(0x00000000);
}

void han_asic_device::reg_han_0010_w(uint32_t data)
{
	//
}

uint32_t han_asic_device::reg_han_0080_r()
{
	return han_asic_device::arrange_han_data(m_han_msgbuff_status);
}

void han_asic_device::reg_han_0080_w(uint32_t data)
{
	uint32_t cntl = han_asic_device::arrange_han_data(data);

	m_han_msgbuff_index = 0x0;
	
	switch (cntl)
	{
		case HAN_MSGCNTL_BUFF_RESET:
			m_han_msgbuff_index = 0x0;
			break;

		case HAN_MSGCNTL_DATA_READ:
		{
			if (m_han_startup_step == HAN_STARTUP_SEND_RESTART)
			{
				m_han_startup_step = HAN_STARTUP_RESTART_OK;
			}
			else if(m_han_startup_step == HAN_STARTUP_SEND_RESET)
			{
				m_han_startup_step = HAN_STARTUP_DONE;
			}
			break;
		}

		case HAN_MSGCNTL_BUFF_RELEASE:
			m_han_msgbuff_status &= (~HAN_MSGCNTL_BUFF_OWN);
			break;

		case HAN_MSGCNTL_BUFF_OWN:
			m_han_msgbuff_status |= HAN_MSGCNTL_BUFF_OWN;
			break;
	}
}

uint32_t han_asic_device::reg_han_0084_r()
{
	if (m_han_msgbuff_status & HAN_MSGCNTL_DATA_WAITING && m_han_msgbuff_index < (HAN_MSGBUFF_SIZE >> 1))
	{
		uint32_t data = han_asic_device::arrange_han_data(m_han_msgbuff[m_han_msgbuff_index++]);

		if (m_han_msgbuff_index >= (HAN_MSGBUFF_SIZE >> 1))
		{
			m_han_msgbuff_status |= HAN_MSGCNTL_DATA_VALID;
			m_han_msgbuff_status &= (~HAN_MSGCNTL_DATA_WAITING);
		}

		return data;
	}
	else
	{
		return 0x00000000;
	}
}

void han_asic_device::reg_han_0084_w(uint32_t data)
{
	if (m_han_msgbuff_status & HAN_MSGCNTL_BUFF_OWN && m_han_msgbuff_index < (HAN_MSGBUFF_SIZE >> 1))
	{
		m_han_msgbuff[m_han_msgbuff_index++] = han_asic_device::arrange_han_data(data);

		if (m_han_msgbuff_index >= (HAN_MSGBUFF_SIZE >> 1))
		{
			uint8_t han_msgtype = m_han_msgbuff[HAN_MSGTYPE_INDEX] & 0xff;

			if(han_msgtype == han_msgtype_t::IPC_CLASS_MAILBOX)
			{
				switch (m_han_msgbuff[HAN_MSGSUBTYPE_INDEX])
				{
					case han_mailbox_msgsubtype_t::SRA2EPC_RETURN_FROM_STANDBY:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_RETURN_FROM_STANDBY,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::SRA2EPC_GO_TO_STANDBY:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_IN_STANDBY,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::SRA2EPC_GET_SYSTEM_INFO:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_SYSTEM_INFO,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::WEB2SRC_GET_CAPABILITIES:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::WEB2SRC_GET_CAPABILITIES,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::SRA2EPC_GET_ALARM_STATUS:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_ALARM_STATUS,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::SRC2EPC_GET_AC3_SETUP:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_GET_AC3_SETUP,
							NULL,
							0x0000
						);
						break;


					case han_mailbox_msgsubtype_t::EPC2SRC_GET_SUID_LIST:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_SERVICE_LIST_NOT_AVAILABLE,
							NULL,
							0x0000
						);
						break;

					case han_mailbox_msgsubtype_t::EPC2SRC_GET_SERVICE_LIST:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_SERVICE_LIST_NOT_AVAILABLE,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_CURRENT_TRANSPONDER:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_CURRENT_TRANSPONDER,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_CURRENT_NETWORK:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_CURRENT_NETWORK,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_CHECK_XPONDER_NUM:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_CHECK_XPONDER_NUM,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_DISH_500_INSTALL:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_DISH_500_INSTALL,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_SIGNAL_STRENGTH:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_SIGNAL_STRENGTH,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_POINTING_INFO:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_POINTING_INFO,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_CHECK_SWITCH:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRC2EPC_SWITCH_VERIFIED,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_SWITCH_INFO:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_SWITCH_INFO,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_SWITCH_TEST_STEP:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_SWITCH_TEST_STEP,
							NULL,
							0x0000
						);
						break;

					case SRA2EPC_GET_NETWORK_HIDDEN_STATUS:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::SRA2EPC_GET_NETWORK_HIDDEN_STATUS,
							NULL,
							0x0000
						);
						break;

					case WEB2EPC_HARD_RESET:
						send_han_message(
							han_msgtype_t::IPC_CLASS_MAILBOX,
							han_mailbox_msgsubtype_t::WEB2EPC_HARD_RESET,
							NULL,
							0x0000
						);
						break;
				}
			}
			else if(han_msgtype == han_msgtype_t::IPC_CLASS_DIAG)
			{
				send_han_message(
					han_msgtype_t::IPC_CLASS_DIAG,
					han_diag_msgsubtype_t::DIAG_ECHO_MB_MSG,
					NULL,
					0x0000
				);
			}

			m_han_msgbuff_status &= (~HAN_MSGCNTL_BUFF_OWN);
			m_han_msgbuff_status |= HAN_MSGCNTL_DATA_READ;
		}
	}
}

TIMER_CALLBACK_MEMBER(han_asic_device::check_han_message_state)
{
	if (m_han_startup_step == HAN_STARTUP_BEGIN)
	{
		if (send_han_message(
				han_msgtype_t::IPC_CLASS_RESTART,
				0x0000,
				NULL,
				0x0000
			))
		{
			m_han_startup_step = HAN_STARTUP_SEND_RESTART;
		}
	}
	else if (m_han_startup_step == HAN_STARTUP_RESTART_OK)
	{
		if (send_han_message(
				han_msgtype_t::IPC_CLASS_MAILBOX,
				han_mailbox_msgsubtype_t::EPC2SRA_RESET,
				NULL,
				0x0000
			))
		{
			m_han_startup_step = HAN_STARTUP_SEND_RESET;
		}
	}
	else if(han_asic_device::have_queued_han_message())
	{
		m_han_msgbuff_index = 0x0;
		han_asic_device::set_han_irq(HAN_INT_MSG_IN, ASSERT_LINE);
		m_han_need_in_int = false;
	}
	else
	{
		han_asic_device::set_han_irq(HAN_INT_MSG_OUT, ASSERT_LINE);
	}
}

TIMER_CALLBACK_MEMBER(han_asic_device::han_reboot)
{
	m_han_need_in_int = false;
	m_han_msgbuff_status = 0x0;
	m_han_msgbuff_index = 0x0;
	m_han_startup_step = HAN_STARTUP_BEGIN;

	han_message_timer->adjust(attotime::from_hz(HAN_MSGIRQ_HACK_HZ), 0, attotime::from_hz(HAN_MSGIRQ_HACK_HZ));
	han_message_timer->enable(true);
}

uint32_t han_asic_device::reg_ide_000000_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(0));
}

void han_asic_device::reg_ide_000000_w(uint32_t data)
{
	m_ata->cs0_w(0, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_000004_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(1));
}

void han_asic_device::reg_ide_000004_w(uint32_t data)
{
	m_ata->cs0_w(1, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_000008_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(2));
}

void han_asic_device::reg_ide_000008_w(uint32_t data)
{
	m_ata->cs0_w(2, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_00000c_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(3));
}

void han_asic_device::reg_ide_00000c_w(uint32_t data)
{
	m_ata->cs0_w(3, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_000010_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(4));
}

void han_asic_device::reg_ide_000010_w(uint32_t data)
{
	m_ata->cs0_w(4, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_000014_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(5));
}

void han_asic_device::reg_ide_000014_w(uint32_t data)
{
	m_ata->cs0_w(5, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_000018_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(6));
}

void han_asic_device::reg_ide_000018_w(uint32_t data)
{
	m_ata->cs0_w(6, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_00001c_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs0_r(7));
}

void han_asic_device::reg_ide_00001c_w(uint32_t data)
{
	m_ata->cs0_w(7, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_400018_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs1_r(6));
}

void han_asic_device::reg_ide_400018_w(uint32_t data)
{
	m_ata->cs1_w(6, han_asic_device::arrange_han_data(data));
}

uint32_t han_asic_device::reg_ide_40001c_r()
{
	return han_asic_device::arrange_han_data(m_ata->cs1_r(7));
}

void han_asic_device::reg_ide_40001c_w(uint32_t data)
{
	m_ata->cs1_w(7, han_asic_device::arrange_han_data(data));
}
