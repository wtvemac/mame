// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "fud_asic.h"
#include "utv_ca_card.h"

DEFINE_DEVICE_TYPE(FUD_ASIC, fud_asic_device, "fud_asic_device", "WebTV FUD ASIC")

fud_asic_device::fud_asic_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t gpio_base_address, uint32_t clock) :
	device_t(mconfig, FUD_ASIC, tag, owner, clock),
	m_hostcpu(*owner, "maincpu"),
	m_ca_card_present(*this, 0)
{
	m_gpio_base = gpio_base_address;
}

void fud_asic_device::device_start()
{
	tuner_lock_timer = timer_alloc(FUNC(fud_asic_device::set_tuner_lock), this);
	m_guide_timer = timer_alloc(FUNC(fud_asic_device::guide_tick), this);

	m_tp_regs = std::make_unique<uint32_t[]>(TP_SIZE / 4);

	save_item(NAME(m_fud_cntl));
	save_item(NAME(m_gpio_base));
	save_item(NAME(m_gpio_intenable));
	save_item(NAME(m_gpio_intstat));
	save_item(NAME(m_gpio_level));
	save_pointer(NAME(m_tp_regs), TP_SIZE / 4);
	save_item(STRUCT_MEMBER(m_tp_dma, active));
	save_item(STRUCT_MEMBER(m_tp_dma, start));
	save_item(STRUCT_MEMBER(m_tp_dma, end));
	save_item(STRUCT_MEMBER(m_tp_dma, ptr));
	save_item(STRUCT_MEMBER(m_tp_dma, since_progress));
	save_item(NAME(m_guide_cc));
}

void fud_asic_device::device_reset()
{
	m_fud_cntl = 0x0;
	m_gpio_intenable = 0x0;
	m_gpio_intstat = 0x0;
	m_gpio_level = 0x0;

	std::fill_n(m_tp_regs.get(), TP_SIZE / 4, 0);

	for (auto &dma : m_tp_dma)
		dma = tp_dma_state{ false, 0, 0, 0, 0 };

	for (uint32_t proc = 0; proc < TP_PROC_COUNT; proc++)
	{
		m_guide_cc[proc] = 0;
		m_guide_pos[proc] = 0;
		m_guide_cycle[proc].clear();
	}
	m_guide_time_next = attotime::zero;

	m_guide_timer->adjust(attotime::from_hz(GUIDE_HZ), 0, attotime::from_hz(GUIDE_HZ));
}

void fud_asic_device::cntl_map(address_map &map)
{
	map(0x008, 0x00b).rw(FUNC(fud_asic_device::cntl_0008_r), FUNC(fud_asic_device::cntl_0008_w));
	map(0x00c, 0x00f).rw(FUNC(fud_asic_device::cntl_000c_r), FUNC(fud_asic_device::cntl_000c_w));
}

void fud_asic_device::gpio_map(address_map &map)
{
	map(0x02c, 0x02f).rw(FUNC(fud_asic_device::gpio_002c_r), FUNC(fud_asic_device::gpio_002c_w));
	map(0x030, 0x033).rw(FUNC(fud_asic_device::gpio_0030_r), FUNC(fud_asic_device::gpio_0030_w));
	map(0x034, 0x037).rw(FUNC(fud_asic_device::gpio_0034_r), FUNC(fud_asic_device::gpio_0034_w));
	map(0x03c, 0x03f).rw(FUNC(fud_asic_device::gpio_003c_r), FUNC(fud_asic_device::gpio_003c_w));
}

void fud_asic_device::transport_map(address_map &map)
{
	map(0x0000, TP_SIZE - 1).rw(FUNC(fud_asic_device::tp_r), FUNC(fud_asic_device::tp_w));
}

TIMER_CALLBACK_MEMBER(fud_asic_device::set_tuner_lock)
{
	uint32_t gpio_tuner_lock = GPIO_TUNER0_LOCK | GPIO_TUNER1_LOCK;

	m_gpio_intenable |= gpio_tuner_lock;

	if (!(m_gpio_intstat & gpio_tuner_lock))
		fud_asic_device::set_gpio_irq(gpio_tuner_lock, ASSERT_LINE);
}

void fud_asic_device::set_gpio_irq(uint32_t mask, int state)
{
	if (state)
		m_gpio_intstat |= mask;
	else
		m_gpio_intstat &= (~mask);

	fud_asic_device::update_gpio_irq();
}

void fud_asic_device::update_gpio_irq()
{
	if ((m_gpio_intstat | m_gpio_level) & m_gpio_intenable)
		m_hostcpu->set_input_line(MIPS3_IRQ2, ASSERT_LINE);
	else
		m_hostcpu->set_input_line(MIPS3_IRQ2, CLEAR_LINE);
}

uint32_t fud_asic_device::cntl_0008_r()
{
	return 0x00000000;
}

void fud_asic_device::cntl_0008_w(uint32_t data)
{
	m_fud_cntl = data;
}

uint32_t fud_asic_device::cntl_000c_r()
{
	// Hacked PCI regs, may add a proper PCI interface (needed for USB) later but right now this is "good enough"

	if (m_fud_cntl == 0x80009818)
	{
		// DVR Transport BAR
		return TP_BASE;
	}
	else if (m_fud_cntl == 0x80000018)
	{
		tuner_lock_timer->adjust(attotime::from_hz(TUNER_LOCK_HZ), 0, attotime::from_hz(TUNER_LOCK_HZ));

		return m_gpio_base;
	}
	else
	{
		return 0x80201414;
	}
}

void fud_asic_device::cntl_000c_w(uint32_t data)
{
	//
}

uint32_t fud_asic_device::gpio_002c_r()
{
	return m_gpio_intenable;
}

void fud_asic_device::gpio_002c_w(uint32_t data)
{
	m_gpio_intenable |= data;

	fud_asic_device::update_gpio_irq();
}

uint32_t fud_asic_device::gpio_0030_r()
{
	return m_gpio_intstat | m_gpio_level;
}

void fud_asic_device::gpio_0030_w(uint32_t data)
{
	m_gpio_intenable &= (~data);

	fud_asic_device::update_gpio_irq();
}

uint32_t fud_asic_device::gpio_0034_r()
{
	return (m_gpio_intstat | m_gpio_level) & m_gpio_intenable;
}

void fud_asic_device::gpio_0034_w(uint32_t data)
{
	m_gpio_intenable |= data;

	fud_asic_device::update_gpio_irq();
}

uint32_t fud_asic_device::gpio_003c_r()
{
	return m_gpio_intenable;
}

void fud_asic_device::gpio_003c_w(uint32_t data)
{
	fud_asic_device::set_gpio_irq(data, CLEAR_LINE);
}

// DVR Transport

bool fud_asic_device::tp_decode(uint32_t byte_offset, uint32_t &proc, uint32_t &reg)
{
	if (byte_offset < TP_PROC_BASE)
		return false;

	proc = (byte_offset - TP_PROC_BASE) / TP_PROC_STRIDE;
	reg = byte_offset - proc * TP_PROC_STRIDE;

	return proc < TP_PROC_COUNT;
}

uint32_t fud_asic_device::tp_r(offs_t offset)
{
	uint32_t proc;
	uint32_t reg;

	if (fud_asic_device::tp_decode(offset << 2, proc, reg) && reg >= TP_DMA && reg < TP_DMA + TP_DMA_COUNT * TP_DMA_STRIDE)
	{
		const uint32_t channel = (reg - TP_DMA) / TP_DMA_STRIDE;
		const uint32_t dma_reg = (reg - TP_DMA) % TP_DMA_STRIDE;
		const tp_dma_state &dma = tp_dma(proc, channel);

		if (dma_reg == TP_DMA_CURRADDR)
			return dma.ptr;
		else if (dma_reg == TP_DMA_INDEX)
			return 0;
	}

	return m_tp_regs[offset];
}

void fud_asic_device::tp_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	uint32_t proc;
	uint32_t reg;
	const bool in_proc = fud_asic_device::tp_decode(offset << 2, proc, reg);
	const uint32_t old_data = m_tp_regs[offset];

	data = (old_data & ~mem_mask) | (data & mem_mask);

	if (!in_proc)
	{
		m_tp_regs[offset] = data;
	}
	else if (reg == TP_CTLSTAT)
	{
		m_tp_regs[offset] = (data & ~TP_CTLSTAT_W1C) | (old_data & TP_CTLSTAT_W1C & ~data);

		fud_asic_device::tp_update_irq();
	}
	else if (reg >= TP_DMA && reg < TP_DMA + TP_DMA_COUNT * TP_DMA_STRIDE && ((reg - TP_DMA) % TP_DMA_STRIDE) == TP_DMA_CONTROL)
	{
		fud_asic_device::tp_dma_control_w(proc, (reg - TP_DMA) / TP_DMA_STRIDE, old_data, data);
	}
	else
	{
		m_tp_regs[offset] = data;
	}
}

void fud_asic_device::tp_dma_control_w(uint32_t proc, uint32_t channel, uint32_t old_data, uint32_t data)
{
	tp_dma_state &dma = tp_dma(proc, channel);
	uint32_t &control = tp_dma_reg(proc, channel, TP_DMA_CONTROL);

	control = (data & ~TP_DMA_W1C) | (old_data & TP_DMA_W1C & ~data);

	if (!(control & TP_DMA_RUN))
	{
		dma.active = false;
	}
	else if (!dma.active && (control & TP_DMA_NEXTVALID))
	{
		fud_asic_device::tp_dma_load_next(proc, channel);
	}
}

bool fud_asic_device::tp_dma_load_next(uint32_t proc, uint32_t channel)
{
	tp_dma_state &dma = tp_dma(proc, channel);
	uint32_t &control = tp_dma_reg(proc, channel, TP_DMA_CONTROL);

	if (!(control & TP_DMA_RUN) || !(control & TP_DMA_NEXTVALID))
	{
		dma.active = false;
		return false;
	}

	dma.start = tp_dma_reg(proc, channel, TP_DMA_NEXTADDR) & 0x1fffffff;
	dma.end = dma.start + (tp_dma_reg(proc, channel, TP_DMA_NEXTSIZE) & 0x000fffff) * 32;
	dma.ptr = dma.start;
	dma.since_progress = 0;
	dma.active = (dma.end > dma.start);

	control &= ~TP_DMA_NEXTVALID;

	return dma.active;
}

void fud_asic_device::tp_dma_raise(uint32_t proc, uint32_t channel, uint32_t status)
{
	tp_dma_reg(proc, channel, TP_DMA_CONTROL) |= status;
	tp_reg(proc, TP_CTLSTAT) |= (1 << channel);

	fud_asic_device::tp_update_irq();
}

bool fud_asic_device::tp_dma_write_record(uint32_t proc, uint32_t channel, const uint8_t *packet)
{
	tp_dma_state &dma = tp_dma(proc, channel);

	if (!dma.active)
		return false;

	if (dma.ptr + TP_RECORD_SIZE > dma.end)
	{
		fud_asic_device::tp_dma_raise(proc, channel, TP_DMA_DONE);

		if (!fud_asic_device::tp_dma_load_next(proc, channel))
			return false;
	}

	address_space &space = m_hostcpu->space(AS_PROGRAM);

	uint8_t record[TP_RECORD_SIZE] = { 0 };
	std::copy_n(packet, DSS_PACKET_SIZE, &record[TP_RECORD_PREFIX]);

	const bool little_endian = (m_hostcpu->get_endianness() == ENDIANNESS_LITTLE);

	for (uint32_t i = 0; i < TP_RECORD_SIZE; i += 4)
	{
		uint32_t word;

		if (little_endian)
			word = record[i] | (record[i + 1] << 8) | (record[i + 2] << 16) | (uint32_t(record[i + 3]) << 24);
		else
			word = (uint32_t(record[i]) << 24) | (record[i + 1] << 16) | (record[i + 2] << 8) | record[i + 3];

		space.write_dword(dma.ptr + i, word);
	}

	dma.ptr += TP_RECORD_SIZE;

	const uint32_t control = tp_dma_reg(proc, channel, TP_DMA_CONTROL);

	if (control & TP_DMA_PROGIEN)
	{
		const uint32_t threshold = std::max<uint32_t>((control >> 10) & 0xff, 1);

		dma.since_progress++;
		if (dma.since_progress >= threshold)
		{
			dma.since_progress = 0;
			fud_asic_device::tp_dma_raise(proc, channel, TP_DMA_PROGRESS);
		}
	}

	return true;
}

void fud_asic_device::tp_update_irq()
{
	uint32_t level = 0;

	if (tp_reg(0, TP_CTLSTAT) & 0x1f)
		level |= GPIO_INT_TP0;

	if (tp_reg(1, TP_CTLSTAT) & 0x1f)
		level |= GPIO_INT_TP1;

	m_gpio_level = level;

	fud_asic_device::update_gpio_irq();
}

int fud_asic_device::tp_find_channel(uint32_t proc, uint16_t scid)
{
	const uint32_t enables = tp_reg(proc, TP_FILTERCTL) & 0xfff;

	for (uint32_t slot = 0; slot < TP_SLOT_COUNT; slot++)
	{
		if (!(enables & (1 << slot)))
			continue;

		const uint32_t control = tp_reg(proc, TP_PKTCTL + slot * 4);

		if ((control & 0xfff) != scid)
			continue;

		// DMA channel for the slot
		const uint32_t channel = (control >> 19) & 7;

		if (channel < TP_DMA_COUNT)
			return channel;
	}

	return -1;
}

void fud_asic_device::append_guide_object(guide_cycle &cycle, uint16_t net_id, uint8_t object_type, uint32_t object_key, const std::vector<uint8_t> &data)
{
	static constexpr uint32_t PAYLOAD_SIZE = DSS_PACKET_SIZE - 4;

	// object header + data, 12 bits
	const uint32_t length = 12 + data.size();

	// Segment header (8 bytes) + object, then the checksum
	const uint32_t count = (8 + length + 4 + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;

	// APG reassembles at most 0xfc0 bytes (32 packets) per segment.
	if (length > 0xfff || count > 32)
		return;

	std::vector<uint8_t> buffer(count * PAYLOAD_SIZE, 0);

	// Segment header
	buffer[0] = net_id >> 8;
	buffer[1] = net_id & 0xff;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = (length >> 8) & 0x0f;
	buffer[7] = length & 0xff;

	// Object header
	buffer[8] = object_type;
	buffer[9] = GUIDE_OBJECT_VERSION;
	buffer[16] = (object_key >> 24) & 0xff;
	buffer[17] = (object_key >> 16) & 0xff;
	buffer[18] = (object_key >> 8) & 0xff;
	buffer[19] = object_key & 0xff;

	std::copy(data.begin(), data.end(), buffer.begin() + 20);

	const uint32_t sum_offset = buffer.size() - 4;
	uint32_t sum = 0;
	for (uint32_t i = 0; i < sum_offset; i++)
		sum += buffer[i];

	buffer[sum_offset + 0] = (sum >> 24) & 0xff;
	buffer[sum_offset + 1] = (sum >> 16) & 0xff;
	buffer[sum_offset + 2] = (sum >> 8) & 0xff;
	buffer[sum_offset + 3] = sum & 0xff;

	for (uint32_t i = 0; i < count; i++)
	{
		std::array<uint8_t, DSS_PACKET_SIZE> packet;

		// DSS header
		packet[0] = 0x20 | ((APG_BOOT_SCID >> 8) & 0x0f);
		packet[1] = APG_BOOT_SCID & 0xff;
		packet[2] = 0x04;
		packet[3] = 0x3c | ((i == 0) ? 0x02 : 0) | ((i == count - 1) ? 0x01 : 0);

		std::copy_n(&buffer[i * PAYLOAD_SIZE], PAYLOAD_SIZE, &packet[4]);

		cycle.push_back(packet);
	}
}

void fud_asic_device::build_guide_cycle(guide_cycle &cycle, bool with_time)
{
	cycle.clear();
	const bool test_subscription = m_ca_card_present();

	if (with_time)
	{
		system_time systime;
		machine().current_datetime(systime);

		const uint32_t gps = uint32_t(systime.time - GPS_EPOCH_UNIX + GPS_LEAP_SECONDS);

		const std::vector<uint8_t> time_object = {
				uint8_t(gps >> 24), // GPS seconds since 1980-01-06 00:00:00 UTC (bits 31-24)
				uint8_t(gps >> 16), // ^ (bits 23-16)
				uint8_t(gps >> 8),  // ^ (bits 15-8)
				uint8_t(gps),       // ^ (bits 7-0)
				GPS_LEAP_SECONDS,   // GPS-UTC offset in seconds, subtracted to get UTC
				0x00, 0x00          // DST descriptor length
		};

		append_guide_object(cycle, APG_NETWORK_CURRENT, 0x91, GUIDE_KEY_TIME, time_object);
	}

	// Channel object
	static constexpr char CHANNEL_NAME[] = "MAME";
	std::vector<uint8_t> channel;
	channel.push_back(GUIDE_SOURCE_ID >> 8);
	channel.push_back(GUIDE_SOURCE_ID & 0xff);
	channel.push_back(GUIDE_CHANNEL_NUMBER >> 8);
	channel.push_back(GUIDE_CHANNEL_NUMBER & 0xff);
	channel.push_back(sizeof(CHANNEL_NAME) - 1);
	channel.insert(channel.end(), CHANNEL_NAME, CHANNEL_NAME + sizeof(CHANNEL_NAME) - 1);
	channel.push_back(0x03);
	channel.push_back(0x00);
	channel.push_back(0x00);
	channel.push_back(0x00);
	channel.push_back(0x00);
	append_guide_object(cycle, APG_NETWORK_OTHER, 0x84, GUIDE_KEY_CHANNEL, channel);

	// Schedule (BEV) object
	std::vector<uint8_t> schedule;
	schedule.push_back(GUIDE_SOURCE_ID >> 8);
	schedule.push_back(GUIDE_SOURCE_ID & 0xff);
	schedule.push_back((GUIDE_SCHEDULE_START >> 24) & 0xff);
	schedule.push_back((GUIDE_SCHEDULE_START >> 16) & 0xff);
	schedule.push_back((GUIDE_SCHEDULE_START >> 8) & 0xff);
	schedule.push_back(GUIDE_SCHEDULE_START & 0xff);
	schedule.push_back(GUIDE_EVENT_COUNT);
	for (uint32_t i = 0; i < GUIDE_EVENT_COUNT; i++)
	{
		schedule.push_back(0xc0);
		schedule.push_back((GUIDE_EVENT_SECONDS >> 16) & 0xff);
		schedule.push_back((GUIDE_EVENT_SECONDS >> 8) & 0xff);
		schedule.push_back(GUIDE_EVENT_SECONDS & 0xff);
		schedule.push_back(0x00);
		schedule.push_back(0x00);
		schedule.push_back(0x00);
	}
	schedule.push_back(0x00);
	schedule.push_back(0x00);
	append_guide_object(cycle, APG_NETWORK_OTHER, 0x82, GUIDE_KEY_SCHEDULE, schedule);
	if (test_subscription)
	{
		constexpr auto definition = utv_ca_card::guide_object();
		append_guide_object(cycle, APG_NETWORK_OTHER, 0x94, GUIDE_KEY_SUBSCRIPTION,
			std::vector<uint8_t>(definition.begin(), definition.end()));
	}

	// Update List Objects
	auto ulo_item = [] (std::vector<uint8_t> &ulo, uint32_t key, uint8_t type)
	{
		ulo.push_back((key >> 24) & 0xff);
		ulo.push_back((key >> 16) & 0xff);
		ulo.push_back((key >> 8) & 0xff);
		ulo.push_back(key & 0xff);
		ulo.push_back(type);
		ulo.push_back(GUIDE_OBJECT_VERSION);
		ulo.push_back(0x00);
	};

	std::vector<uint8_t> current_ulo = { 0x00, 0x00, uint8_t(test_subscription ? 3 : 2) };
	ulo_item(current_ulo, GUIDE_KEY_CHANNEL, 0x84);
	ulo_item(current_ulo, GUIDE_KEY_SCHEDULE, 0x82);
	if (test_subscription)
		ulo_item(current_ulo, GUIDE_KEY_SUBSCRIPTION, 0x94);
	current_ulo.push_back(0x00);
	current_ulo.push_back(0x00);
	append_guide_object(cycle, APG_NETWORK_CURRENT, 0x92, GUIDE_KEY_ULO_CURRENT, current_ulo);

	// Network 0
	append_guide_object(cycle, APG_NETWORK_SEARCHED, 0x92, GUIDE_KEY_ULO_SEARCHED, current_ulo);

	const std::vector<uint8_t> empty_ulo(5, 0);
	append_guide_object(cycle, APG_NETWORK_OTHER, 0x92, GUIDE_KEY_ULO_OTHER, empty_ulo);
}

TIMER_CALLBACK_MEMBER(fud_asic_device::guide_tick)
{
	for (uint32_t proc = 0; proc < TP_PROC_COUNT; proc++)
	{
		const int channel = fud_asic_device::tp_find_channel(proc, APG_BOOT_SCID);
		if (channel < 0)
			continue;

		guide_cycle &cycle = m_guide_cycle[proc];

		for (uint32_t i = 0; i < GUIDE_PACKETS_PER_TICK; i++)
		{
			if (m_guide_pos[proc] >= cycle.size())
			{
				const bool with_time = machine().time() >= m_guide_time_next;
				if (with_time)
					m_guide_time_next = machine().time() + attotime::from_seconds(GUIDE_TIME_INTERVAL);

				fud_asic_device::build_guide_cycle(cycle, with_time);
				m_guide_pos[proc] = 0;

				if (cycle.empty())
					break;

			}

			std::array<uint8_t, DSS_PACKET_SIZE> packet = cycle[m_guide_pos[proc]];
			packet[2] = ((m_guide_cc[proc] & 0x0f) << 4) | 0x04;

			if (!fud_asic_device::tp_dma_write_record(proc, channel, packet.data()))
				break;

			m_guide_cc[proc]++;
			m_guide_pos[proc]++;
		}
	}
}
