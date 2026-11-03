// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

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
	save_item(NAME(m_han_pending));
	save_item(NAME(m_han_pending_count));
	save_item(NAME(m_han_channel));
	save_item(NAME(m_han_dma_regs));
	save_item(NAME(m_han_dma_received));
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
	m_han_pending_count = 0;
	m_han_channel = 0;
	m_han_dma_received = 0;
	std::fill_n(m_han_dma_regs, HAN_DMA_REGS_COUNT, 0);
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

	// Data DMA channels
	// 0x2f0, 0x3f0
	map(0x200, 0x3ff).rw(FUNC(han_asic_device::reg_han_dma_r), FUNC(han_asic_device::reg_han_dma_w));
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

uint32_t han_asic_device::reg_han_dma_r(offs_t offset)
{
	return han_asic_device::arrange_han_data(m_han_dma_regs[offset]);
}

void han_asic_device::reg_han_dma_w(offs_t offset, uint32_t data)
{
	m_han_dma_regs[offset] = han_asic_device::arrange_han_data(data);
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

uint16_t *han_asic_device::queue_han_message(uint16_t msg_type, uint16_t msg_subtype)
{
	if (m_han_pending_count >= HAN_PENDING_MAX)
		return nullptr;

	uint16_t *msg = m_han_pending[m_han_pending_count++];

	std::fill_n(msg, HAN_MSGBUFF_SIZE >> 1, 0);
	msg[HAN_MSGSIZE_INDEX] = HAN_MSGBUFF_SIZE;
	msg[HAN_MSGTYPE_INDEX] = msg_type;
	msg[HAN_MSGSUBTYPE_INDEX] = msg_subtype;

	return msg;
}

uint16_t *han_asic_device::queue_han_mailbox(uint16_t msg_subtype)
{
	return queue_han_message(han_msgtype_t::IPC_CLASS_MAILBOX, msg_subtype);
}

uint32_t han_asic_device::han_dma_reg(uint8_t channel, uint32_t reg)
{
	const uint32_t base = (channel < 10) ? (0x000 + channel * 0x10) : (0x100 + (channel - 10) * 0x10);

	return m_han_dma_regs[(base + reg) >> 2];
}

bool han_asic_device::send_data_dma(uint32_t type, const std::vector<uint8_t> &payload)
{
	// Writes one packet into the channel's current buffer and tells the OS side it's ready.

	const uint32_t len_reg = han_dma_reg(HAN_DMA_DATA_CHANNEL, HAN_DMA_LEN);
	const uint32_t address = han_dma_reg(HAN_DMA_DATA_CHANNEL, HAN_DMA_ADDR) & HAN_DMA_ADDR_MASK;
	const uint32_t buffer_size = len_reg & HAN_DMA_LEN_MASK;
	const uint32_t packet_size = HAN_DATA_HEADER_SIZE + payload.size();

	if (!(len_reg & HAN_DMA_LEN_ENABLE) || packet_size > buffer_size || m_han_pending_count >= HAN_PENDING_MAX)
		return false;

	address_space &space = m_hostcpu->space(AS_PROGRAM);
	const uint32_t length = payload.size() + 0x10;

	auto write_le32 = [&space] (offs_t at, uint32_t data)
	{
		for (int i = 0; i < 4; i++)
			space.write_byte(at + i, (data >> (i * 8)) & 0xff);
	};

	space.write_dword(address + 0x00, HAN_DATA_MAGIC);
	write_le32(address + 0x04, length);
	write_le32(address + 0x08, type);
	for (uint32_t i = 0x0c; i < HAN_DATA_HEADER_SIZE; i++)
		space.write_byte(address + i, 0);
	for (uint32_t i = 0; i < payload.size(); i++)
		space.write_byte(address + HAN_DATA_HEADER_SIZE + i, payload[i]);

	// The WebTV OS counts each packet as its length + 0x27 (rounded down to 32 bytes)
	m_han_dma_received += (length + 0x27) & ~0x1f;

	if (uint16_t *msg = queue_han_message(han_msgtype_t::IPC_CLASS_DMA, 0x0000))
	{
		for (int i = 0; i < 4; i++)
			set_han_msg_uint8(msg, HAN_DMA_STATUS_RECEIVED + i, (m_han_dma_received >> (i * 8)) & 0xff);
	}

	return true;
}

bool han_asic_device::send_service_list()
{
	static const han_service_t services[] =
	{
		{
			.channel = 691,
			.name = "Hack TV 691",
			.callsign = "HckTV"
		},
		{
			.channel = 692,
			.name = "Fart TV 692",
			.callsign = "FrtTV"
		},
		{
			.channel = 693,
			.name = "eMac 693",
			.callsign = "AC1BO"
		},
		{
			.channel = 694,
			.name = "Zefie 694",
			.callsign = "Zefie"
		},
		{
			.channel = 695,
			.name = "MattMan 695",
			.callsign = "MattM"
		},
	};

	std::vector<uint8_t> payload(HAN_SERVICES_START + std::size(services) * HAN_SERVICE_SIZE + 4, 0);

	payload[HAN_SERVICES_COUNT + 0] = std::size(services) & 0xff;
	payload[HAN_SERVICES_COUNT + 1] = std::size(services) >> 8;

	for (size_t i = 0; i < std::size(services); i++)
	{
		uint8_t *service = &payload[HAN_SERVICES_START + i * HAN_SERVICE_SIZE];

		service[HAN_SERVICE_CHANNEL + 0] = services[i].channel & 0xff;
		service[HAN_SERVICE_CHANNEL + 1] = services[i].channel >> 8;
		service[HAN_SERVICE_TYPE] = HAN_SERVICE_TYPE_TV;
		service[HAN_SERVICE_NETWORK + 0] = HAN_EMU_NETWORK_ID & 0xff;
		service[HAN_SERVICE_NETWORK + 1] = HAN_EMU_NETWORK_ID >> 8;
		strncpy((char *)&service[HAN_SERVICE_NAME], services[i].name, 16);
		memcpy(&service[HAN_SERVICE_CALLSIGN], services[i].callsign, 5);
	}

	return send_data_dma(HAN_DATA_SERVICE_LIST, payload);
}

void han_asic_device::set_han_msg_uint8(uint16_t *msg, uint32_t offset, uint8_t data)
{
	const uint32_t shift = (offset & 1) * 8;

	msg[offset >> 1] = (msg[offset >> 1] & ~(0xff << shift)) | (data << shift);
}

void han_asic_device::set_han_msg_uint16(uint16_t *msg, uint32_t offset, uint16_t data)
{
	han_asic_device::set_han_msg_uint8(msg, offset + 0, data & 0xff);
	han_asic_device::set_han_msg_uint8(msg, offset + 1, data >> 8);
}

void han_asic_device::set_han_msg_uint32(uint16_t *msg, uint32_t offset, uint32_t data)
{
	msg[(offset >> 1) + 0] = data & 0xffff;
	msg[(offset >> 1) + 1] = data >> 16;
}

void han_asic_device::set_han_msg_string(uint16_t *msg, uint32_t offset, const char *str, uint32_t size)
{
	for (uint32_t i = 0; i < (size - 1) && str[i]; i++)
		han_asic_device::set_han_msg_uint8(msg, offset + i, str[i]);
}

bool han_asic_device::send_pending_han_message()
{
	// Wait until the last message was read out and the WebTV OS isn't writing one.
	if (m_han_pending_count == 0 || m_han_startup_step != HAN_STARTUP_DONE || (m_han_msgbuff_status & (HAN_MSGCNTL_DATA_WAITING | HAN_MSGCNTL_BUFF_OWN)))
		return false;

	if (!send_han_message(han_msgtype_t::IPC_CLASS_MAILBOX, m_han_pending[0][HAN_MSGSUBTYPE_INDEX], NULL, 0x0000))
		return false;

	std::copy_n(m_han_pending[0], HAN_MSGBUFF_SIZE >> 1, m_han_msgbuff);

	m_han_pending_count--;
	for (uint8_t i = 0; i < m_han_pending_count; i++)
		std::copy_n(m_han_pending[i + 1], HAN_MSGBUFF_SIZE >> 1, m_han_pending[i]);

	return true;
}

void han_asic_device::queue_tune_complete()
{
	// Announce the channel is authorized before announcing a successful channel change
	if (uint16_t *msg = queue_han_mailbox(han_mailbox_msgsubtype_t::EPC2SRA_CHANNEL_CHANGE_STATUS))
		set_han_msg_uint32(msg, 0x08, HAN_TUNE_STATUS_AUTHORIZED);

	queue_han_mailbox(han_mailbox_msgsubtype_t::EPC2SRA_CHANNEL_CHANGED);
}

void han_asic_device::queue_mailbox_long(uint16_t msg_subtype, uint32_t data)
{
	han_asic_device::queue_mailbox_long(msg_subtype, 0x08, data);
}

// offset is 0x08 or 0x0c, the longs HanMessageIntf::ReadMessageBuffer reads as two shorts.
void han_asic_device::queue_mailbox_long(uint16_t msg_subtype, uint32_t offset, uint32_t data)
{
	if (uint16_t *msg = queue_han_mailbox(msg_subtype))
		set_han_msg_uint32(msg, offset, data);
}

void han_asic_device::queue_date_time(uint16_t msg_subtype, bool local)
{
	system_time systime;
	machine().current_datetime(systime);

	const system_time::full_time &time = local ? systime.local_time : systime.utc_time;

	if (uint16_t *msg = queue_han_mailbox(msg_subtype))
	{
		set_han_msg_uint8(msg, HAN_TIME_MONTH, time.month + 1);
		set_han_msg_uint8(msg, HAN_TIME_DAY, time.mday);
		set_han_msg_uint16(msg, HAN_TIME_YEAR, time.year);
		set_han_msg_uint8(msg, HAN_TIME_HOUR, time.hour);
		set_han_msg_uint8(msg, HAN_TIME_MINUTE, time.minute);
		set_han_msg_uint8(msg, HAN_TIME_SECOND, time.second);
	}
}

void han_asic_device::queue_network_hidden_status()
{
	if (uint16_t *msg = queue_han_mailbox(han_mailbox_msgsubtype_t::SRA2EPC_GET_NETWORK_HIDDEN_STATUS))
	{
		set_han_msg_uint16(msg, HAN_HIDDEN_COUNT, 1);
		set_han_msg_uint16(msg, HAN_HIDDEN_ENTRIES, HAN_EMU_NETWORK_ID);
		set_han_msg_uint8(msg, HAN_HIDDEN_ENTRIES + 2, 0); // not hidden
	}
}

void han_asic_device::queue_system_info()
{
	static const han_sysinfo_field_t fields[] =
	{
		{
			.offset = HAN_SYSINFO_RECEIVER_ID,
			.size = 0x19,
			.value = "MAME_WTVESR"
		},
		{
			.offset = HAN_SYSINFO_CARD_ID,
			.size = 0x19,
			.value = "MAME_CARD69"
		},
		{
			.offset = HAN_SYSINFO_CARD_VERSION,
			.size = 0x10,
			.value = "1.0"
		},
		{
			.offset = HAN_SYSINFO_ROM_VERSION1,
			.size = 0x0c,
			.value = "1.00"
		},
		{
			.offset = HAN_SYSINFO_ROM_VERSION2,
			.size = 0x0c,
			.value = "1.00"
		},
		{
			.offset = HAN_SYSINFO_CUST_SVC,
			.size = 0x16,
			.value = "800-867-5309"
		},
		{
			.offset = HAN_SYSINFO_INTERNET_ADDR,
			.size = 0x1e,
			.value = "0.0.0.0"
		},
		{
			.offset =  HAN_SYSINFO_LOCATION_ID,
			.size = 0x20,
			.value = "00000"
		},
	};

	if (uint16_t *msg = queue_han_mailbox(han_mailbox_msgsubtype_t::SRA2EPC_GET_SYSTEM_INFO))
	{
		for (const auto &field : fields)
			set_han_msg_string(msg, field.offset, field.value, field.size);
	}
}

void han_asic_device::queue_signal_strength()
{
	// Both 119 and 110 satellites are locked and loaded
	if (uint16_t *msg = queue_han_mailbox(han_mailbox_msgsubtype_t::SRA2EPC_GET_SIGNAL_STRENGTH))
	{
		set_han_msg_uint32(msg, HAN_SIGNAL_MAX_SCALE, HAN_EMU_SIGNAL_MAX_SCALE);
		set_han_msg_uint32(msg, HAN_SIGNAL_TONE, 0);

		set_han_msg_uint8(msg, HAN_SIGNAL_LOCKED, 1);
		set_han_msg_uint16(msg, HAN_SIGNAL_STRENGTH, HAN_EMU_SIGNAL_STRENGTH);
		set_han_msg_uint8(msg, HAN_SIGNAL_NEGATIVE, 0);
		set_han_msg_string(msg, HAN_SIGNAL_NAME, "119", 0x20);

		set_han_msg_uint8(msg, HAN_SIGNAL2_LOCKED, 1);
		set_han_msg_uint16(msg, HAN_SIGNAL2_STRENGTH, HAN_EMU_SIGNAL_STRENGTH);
		set_han_msg_uint8(msg, HAN_SIGNAL2_NEGATIVE, 0);
		set_han_msg_string(msg, HAN_SIGNAL2_NAME, "110", 0x20);
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
						queue_han_mailbox(han_mailbox_msgsubtype_t::SRC2EPC_RETURN_FROM_STANDBY);
						queue_tune_complete();
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
						queue_system_info();
						break;

					case han_mailbox_msgsubtype_t::WEB2SRC_GET_CAPABILITIES:
						// Prevents the WebTV OS from failing to client:wrongROM thinking we got a bad update.
						queue_mailbox_long(han_mailbox_msgsubtype_t::WEB2SRC_GET_CAPABILITIES, HAN_CAPABILITY_DVR);
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
						if (!send_service_list())
							queue_han_mailbox(han_mailbox_msgsubtype_t::SRC2EPC_SERVICE_LIST_NOT_AVAILABLE);
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
						queue_signal_strength();
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
						queue_network_hidden_status();
						break;

					case SRA2EPC_CHANGE_CHANNEL:
					case SRA2EPC_RETUNE_CHANNEL:
						m_han_channel = m_han_msgbuff[4] | (m_han_msgbuff[5] << 16);
						queue_tune_complete();
						break;

					case EPC2SRC_TUNE_TO_EPG:
					case WEB2SRC_TUNE_TO_WEB_CHAN:
						queue_tune_complete();
						break;
					case SRA2EPC_GET_STATUS:
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRA2EPC_GET_STATUS, 0);
						break;

					case SRA2EPC_GET_CURRENT_CHANNEL:
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRA2EPC_GET_CURRENT_CHANNEL, m_han_channel);
						break;

					case SRA2EPC_GET_ASPECT_RATIO:
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRA2EPC_GET_ASPECT_RATIO, 0);
						break;

					case EPC2SRC_GET_VIRTUAL_SUBSCRIPTION_RIGHTS:
						if (uint16_t *msg = queue_han_mailbox(han_mailbox_msgsubtype_t::EPC2SRC_GET_VIRTUAL_SUBSCRIPTION_RIGHTS))
						{
							set_han_msg_uint32(msg, 0x08, m_han_msgbuff[4] | (m_han_msgbuff[5] << 16)); // the index asked for
							set_han_msg_uint32(msg, 0x0c, 1); // subscribed
						}
						break;

					case SRA2EPC_GET_TIME:
						queue_date_time(han_mailbox_msgsubtype_t::SRA2EPC_GET_TIME, false);
						break;

					case SRA2EPC_GET_LOCAL_TIME:
						queue_date_time(han_mailbox_msgsubtype_t::SRA2EPC_GET_LOCAL_TIME, true);
						break;

					case EPC2SRC_SEND_EPG:
						queue_han_mailbox(han_mailbox_msgsubtype_t::SRC2EPC_EPG_COMPLETE);
						break;

					case SRA2EPC_GET_DIAGNOSTICS:
						// all zero counters on the diagnostics page
						queue_han_mailbox(han_mailbox_msgsubtype_t::SRA2EPC_GET_DIAGNOSTICS);
						break;

					case SRA2EPC_GET_PREFERRED_AUDIO:
						// keeps the language code
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRA2EPC_GET_PREFERRED_AUDIO, 0x0c, 0);
						break;

					case EPC2SRC_PURCHASE_PPV: 
						// EStarProgram::Purchase takes 0 and 1 as bought.
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRC2EPC_PURCHASE_STATUS, 0x08, 0);
						break;

					case EPC2SRC_GET_SUBSCRIPTION_ACCESS:
						// 0 is not subscribed
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRC2EPC_SUBSCRIPTION_ACCESS_RIGHTS, 0x0c, 1);
						break;

					case EPC2SRC_GET_EVENT_ACCESS:
						// bit 2 would make it pay per view
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRC2EPC_EVENT_ACCESS_RIGHTS, 0x0c, 0);
						break;

					case EPC2SRC_GET_DATA_TAGS:
						// a count of 0 is "No data tags are available"
						queue_mailbox_long(han_mailbox_msgsubtype_t::SRC2EPC_DATA_TAGS, 0x0c, 0);
						break;

					case WEB2SRC_NEED_UPDATE:
						queue_mailbox_long(han_mailbox_msgsubtype_t::WEB2SRC_NEED_UPDATE, 0);
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
	else if (han_asic_device::send_pending_han_message())
	{
		// The interrupt needs to goes out on the next tick.
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
	m_han_pending_count = 0;

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
