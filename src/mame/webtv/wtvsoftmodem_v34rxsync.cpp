// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_v34rxsync.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <numeric>
#include <thread>

// ITU-T V.34 8 Framing (mapping frame alignment from B1, 10.1.3.1)

wtvsoftmodem_v34rxsync::wtvsoftmodem_v34rxsync()
{
	//
}

void wtvsoftmodem_v34rxsync::init(const codec::config_t &cfg, bool acquire_b1, bool acquire_hdlc, double gain_center, int32_t gain_steps, int32_t phase_steps)
{
	m_acquire_hdlc = acquire_hdlc;
	m_candidates.clear();
	m_locked = -1;
	m_drop = 0;
	m_symbol = 0;
	m_tolerant_marks = acquire_b1;

	codec parameters;
	parameters.init(cfg);
	m_b = parameters.b();
	m_p = parameters.p();
	m_r = parameters.r();

	const int32_t phases = acquire_b1 ? m_p / std::gcd(m_r, m_p) : m_p;
	m_phases = phases;

	m_cycle_bits = (m_b - 1) * phases + phases * m_r / m_p;

	m_candidates.reserve((2 * phase_steps + 1) * (acquire_b1 ? 2 * gain_steps + 1 : 1) * FRAME_SYMBOLS * phases);


	// In order to lock onto the RX we do a brute force parameter search. This builds a table we can use to search.

	// Phase rotation angle
	for (int32_t angle = -phase_steps; angle <= phase_steps; angle++)
	{
		// Constellation scale (gain)
		for (int32_t gain = acquire_b1 ? -gain_steps : 0; gain <= (acquire_b1 ? gain_steps : 0); gain++)
		{
			// ITU-T V.34 Section 8 Framing (specifically the mapping frame)

			// Where in the V.34 8-symbol mapping frame we're at.
			for (int32_t offset = 0; offset < FRAME_SYMBOLS; offset++)
			{
				// Which mapping frame we're in
				for (int32_t phase = 0; phase < phases; phase++)
				{
					m_candidates.emplace_back();
					auto &c = m_candidates.back();

					c.decoder = std::make_unique<codec>(parameters);
					c.decoder->rx_set_frame_phase(phase);
					c.offset = uint32_t(offset);
					c.phase = phase;
					c.gain = gain_center + gain * GAIN_STEP;
					c.angle = angle * PHASE_STEP;
					c.cosine = std::cos(c.angle);
					c.sine = std::sin(c.angle);
					c.decoder->collect_bits(true);
				}
			}
		}
	}
}

std::unique_ptr<wtvsoftmodem_v34rxsync::codec> wtvsoftmodem_v34rxsync::take_decoder()
{
	if (!locked())
		return {};

	auto decoder = std::move(m_candidates[m_locked].decoder);

	decoder->finish_shared_geometry();
	decoder->collect_bits(false);
	decoder->set_bit_sink(
		[sink = m_sink, skip = m_drop](int32_t bit) mutable
		{
			if (skip > 0)
			{
				skip--;
			}
			else if (sink)
			{
				sink(bit);
			}
		}
	);

	m_candidates.clear();
	m_locked = -1;

	return decoder;
}

void wtvsoftmodem_v34rxsync::put_symbol(codec::point_t point)
{
	if (m_locked >= 0)
	{
		process(m_locked, point);
		m_symbol++;
		return;
	}

	const uint32_t groups = uint32_t(m_candidates.size()) / uint32_t(m_phases);
	std::atomic<int32_t> first_lock{ std::numeric_limits<int32_t>::max() };

	struct ranked
	{
		double score = std::numeric_limits<double>::infinity();
		int32_t index = -1;
	};

	std::array<ranked, MAX_TASKS> best{};
	auto run_groups = [&](uint32_t begin, uint32_t end, ranked &task_best)
	{
		for (uint32_t group = begin; group < end; group++)
		{
			const uint32_t first = group * uint32_t(m_phases);

			for (uint32_t index = first; index < first + uint32_t(m_phases); index++)
			{
				if (index > first && !m_acquire_hdlc && !m_candidates[first].decoder->frame_complete())
					break;

				process(int32_t(index), point);

				const auto &c = m_candidates[index];
				if (c.lock_kind != LockKind::NONE)
				{
					int32_t current = first_lock.load(std::memory_order_relaxed);
					while (int32_t(index) < current && !first_lock.compare_exchange_weak(current, int32_t(index), std::memory_order_relaxed))
					{
					}

					return;
				}

				if (m_acquire_hdlc && c.ready && c.distance_count)
				{
					const double score = c.distance / c.distance_count;
					if (score < task_best.score)
						task_best = { score, int32_t(index) };
				}
			}
		}
	};

	if (m_candidates.size() >= PARALLEL_MIN_CANDIDATES && !m_workers && sync_workers::default_threads() > 1)
		m_workers = std::make_unique<sync_workers>(sync_workers::default_threads());

	uint32_t tasks = 1;
	if (m_workers && m_candidates.size() >= PARALLEL_MIN_CANDIDATES)
	{
		tasks = std::min({ groups, m_workers->threads() * TASKS_PER_THREAD, uint32_t(MAX_TASKS) });
		auto task = [&](uint32_t t)
		{
			run_groups(t * groups / tasks, (t + 1) * groups / tasks, best[t]);
		};
		m_workers->run(tasks, task);
	}
	else
	{
		run_groups(0, groups, best[0]);
	}

	if (const int32_t index = first_lock.load(std::memory_order_relaxed); index != std::numeric_limits<int32_t>::max())
		commit_lock(index);

	m_symbol++;

	if (m_acquire_hdlc && m_locked < 0)
	{
		double score = std::numeric_limits<double>::infinity();
		for (uint32_t t = 0; t < tasks; t++)
		{
			if (best[t].index >= 0 && best[t].score < score)
			{
				score = best[t].score;
				m_locked = best[t].index;
			}
		}

		if (m_locked >= 0)
		{
			auto &c = m_candidates[m_locked];
			m_drop = 0;
			if (m_sink)
			{
				for (int32_t value : c.confirmed)
					m_sink(value);
			}

			c.confirmed.clear();
		}
	}
}

// We use thread workers to do the sync framing search faster doing in all in a single thread
// (and within MAME's execution window for wtvmame)

uint32_t wtvsoftmodem_v34rxsync::sync_workers::default_threads()
{
	static const uint32_t threads = []
	{
		const uint32_t hardware = std::thread::hardware_concurrency();

		return hardware <= 2 ? hardware ? hardware : 1 : std::min(hardware - 1, 4U);
	}();

	return threads;
}

wtvsoftmodem_v34rxsync::sync_workers::sync_workers(uint32_t threads)
{
	for (uint32_t i = 1; i < threads; i++)
	{
		m_threads.emplace_back(
			[this]
			{
				work();
			}
		);
	}
}

wtvsoftmodem_v34rxsync::sync_workers::~sync_workers()
{
	m_stop.store(true, std::memory_order_relaxed);
	m_generation.fetch_add(1, std::memory_order_release);
	m_generation.notify_all();

	for (auto &thread : m_threads)
		thread.join();
}

void wtvsoftmodem_v34rxsync::sync_workers::execute()
{
	for (uint32_t i; (i = m_next.fetch_add(1, std::memory_order_relaxed)) < m_count;)
		m_task(m_context, i);
}

void wtvsoftmodem_v34rxsync::sync_workers::work()
{
	uint32_t seen = 0;

	while (true)
	{
		uint32_t now = seen;
		for (uint32_t spin = 0; spin < SPIN_ROUNDS && (now = m_generation.load(std::memory_order_acquire)) == seen; spin++)
			pause();

		if (now == seen)
		{
			m_generation.wait(seen, std::memory_order_acquire);
			now = m_generation.load(std::memory_order_acquire);
		}

		seen = now;

		if (m_stop.load(std::memory_order_relaxed))
			return;

		execute();

		m_acks.fetch_add(1, std::memory_order_release);
	}
}

void wtvsoftmodem_v34rxsync::process(int32_t index, codec::point_t point)
{
	auto &c = m_candidates[index];

	if (m_symbol < c.offset)
		return;

	c.frame_bits = 0;

	const codec::point_t scaled {
		(point.i * c.cosine - point.q * c.sine) * c.gain,
		(point.i * c.sine + point.q * c.cosine) * c.gain
	};

	if (m_acquire_hdlc && (c.valid_frames || c.ready))
	{
		const auto decision = c.decoder->snap(scaled);
		c.distance += (decision.i - scaled.i) * (decision.i - scaled.i) + (decision.q - scaled.q) * (decision.q - scaled.q);
		c.distance_count++;
	}

	const bool shared = m_locked < 0 && c.phase > 0;
	const auto &geometry = *m_candidates[index - (shared ? c.phase : 0)].decoder;

	if (shared && !geometry.frame_complete() && !(m_acquire_hdlc && (c.valid_frames || c.ready)))
		return;
	const bool valid = shared ? c.decoder->put_symbol_shared(geometry) : c.decoder->put_symbol(scaled);

	receive_frame(index, c.decoder->take_bits());

	if (!valid)
		c.marks = 0;
}

void wtvsoftmodem_v34rxsync::commit_lock(int32_t index)
{
	auto &c = m_candidates[index];

	m_locked = index;
	m_drop = c.lock_drop;

	if (c.lock_kind == LockKind::ODP && m_sink)
	{
		for (uint32_t k = ODP_REPLAY_BITS; k; k--)
			m_sink(int32_t(c.window_bit((c.window_pos + WINDOW_BITS - k) % WINDOW_BITS)));
	}

	for (uint32_t i = c.lock_from; i < c.lock_rest.count; i++)
	{
		if (m_drop > 0)
		{
			m_drop--;
			continue;
		}

		if (m_sink)
			m_sink(int32_t(c.lock_rest.bit(i)));
	}
}

uint64_t wtvsoftmodem_v34rxsync::candidate::window_bits(uint32_t pos, uint32_t len) const
{
	uint64_t value = 0;
	for (uint32_t done = 0; done < len;)
	{
		const uint32_t offset = pos % 64;
		const uint32_t take = std::min(len - done, 64 - offset);

		value |= ((window[pos / 64] >> offset) & wtvsoftmodem_bitblock::low_mask(take)) << done;
		done += take;
		pos = (pos + take) % WINDOW_BITS;
	}

	return value;
}

void wtvsoftmodem_v34rxsync::candidate::set_window_bits(uint32_t pos, uint32_t len, uint64_t value)
{
	for (uint32_t done = 0; done < len;)
	{
		const uint32_t offset = pos % 64;
		const uint32_t take = std::min(len - done, 64 - offset);
		const uint64_t mask = wtvsoftmodem_bitblock::low_mask(take) << offset;

		window[pos / 64] = (window[pos / 64] & ~mask) | (((value >> done) << offset) & mask);
		done += take;
		pos = (pos + take) % WINDOW_BITS;
	}
}

void wtvsoftmodem_v34rxsync::receive_frame(int32_t index, const wtvsoftmodem_bitblock &block)
{
	if (!block.count)
		return;
	auto &c = m_candidates[index];
	uint32_t first = 0;

	if (m_locked < 0 && m_acquire_hdlc)
		first = receive_hdlc_bits(c, block);
	else if (m_locked < 0 && receive_mark_frame(c, block))
		return;

	for (; first < block.count; first++)
	{
		receive_bit(index, int32_t(block.bit(first)));

		if (c.lock_kind != LockKind::NONE)
		{
			c.lock_rest = block;
			c.lock_from = first + 1;
			return;
		}
	}
}

uint32_t wtvsoftmodem_v34rxsync::receive_hdlc_bits(candidate &c, const wtvsoftmodem_bitblock &block)
{
	const uint32_t count = block.count;
	const int32_t index = int32_t(&c - m_candidates.data());

	for (uint32_t first = 0; first < count;)
	{
		if (m_locked >= 0)
			return first;

		if (c.ready)
		{
			c.frame_bits += count - first;
			block.copy_to(c.confirmed, first, count - first);
			return count;
		}

		const int32_t flag = block.find(first, c.flag >> 1, 8, HDLC_FLAG);
		const uint32_t run = (flag < 0 ? count : uint32_t(flag)) - first;

		if (c.frame.size() + run > MAX_FRAME_BITS || c.confirmed.size() + (c.valid_frames ? run : 0) > MAX_CONFIRMED_BITS)
			return first;

		if (run)
		{
			c.frame_bits += run;

			if (c.valid_frames)
				block.copy_to(c.confirmed, first, run);

			block.copy_to(c.frame, first, run);

			const uint32_t end = first + run;

			c.flag = end >= 8 ? uint8_t(block.get(end - 8, 8)) : uint8_t((c.flag >> run) | (block.get(first, run) << (8 - run)));
			first = end;
		}

		if (first < count)
			receive_bit(index, int32_t(block.bit(first++)));
	}
	return count;
}

bool wtvsoftmodem_v34rxsync::receive_mark_frame(candidate &c, const wtvsoftmodem_bitblock &block)
{
	if (!m_tolerant_marks || c.odp_bits)
		return false;

	const uint32_t count = block.count;

	wtvsoftmodem_bitblock outgoing;
	outgoing.count = count;

	for (uint32_t first = 0; first < count; first += 64)
		outgoing.word[first / 64] = c.window_bits((c.window_pos + first) % WINDOW_BITS, std::min(count - first, 64U));

	uint32_t ones = c.window_ones;
	uint32_t filled = c.window_count;
	const uint32_t final_ones = ones + block.ones() - outgoing.ones();

	if (filled + count >= WINDOW_BITS)
	{
		const uint32_t bound = ones + block.ones();

		if (bound >= MARK_THRESHOLD)
		{
			for (uint32_t i = 0; i < count; i++)
			{
				ones += uint32_t(block.word[i / 64] >> (i % 64)) & 1;
				ones -= uint32_t(outgoing.word[i / 64] >> (i % 64)) & 1;

				if (filled < WINDOW_BITS)
					filled++;

				if (filled == WINDOW_BITS && ones >= MARK_THRESHOLD)
					return false;
			}
		}
	}

	if (!c.detection.rx_bits_without_odp(block))
		return false;

	c.frame_bits += count;

	const uint32_t run = block.trailing_ones();
	c.marks = run == (count) ? c.marks + int32_t(count) : int32_t(run);

	for (uint32_t first = 0; first < count; first += 64)
		c.set_window_bits((c.window_pos + first) % WINDOW_BITS, std::min(count - first, 64U), block.word[first / 64]);

	c.window_pos = (c.window_pos + count) % WINDOW_BITS;
	c.window_ones = final_ones;
	c.window_count = std::min(c.window_count + count, WINDOW_BITS);

	return true;
}

bool wtvsoftmodem_v34rxsync::confirm_hdlc(candidate &c, int32_t bit)
{
	if (c.valid_frames)
		c.confirmed.push_back(bit);

	c.flag = uint8_t((c.flag >> 1) | (bit << 7));
	c.frame.push_back(bit);

	if (c.frame.size() > MAX_FRAME_BITS || c.confirmed.size() > MAX_CONFIRMED_BITS)
	{
		c.frame.clear();
		c.confirmed.clear();
		c.valid_frames = 0;
		c.validated_bits = 0;
		c.seen_flag = false;
		c.distance = 0;
		c.distance_count = 0;
		return false;
	}

	if (c.flag != HDLC_FLAG)
		return false;

	if (c.seen_flag && c.frame.size() > 16)
	{
		uint32_t ones = 0;
		uint32_t position = 0;
		uint32_t bytes = 0;
		uint8_t byte = 0;
		uint8_t address = 0;
		uint16_t crc = 0xffff;
		bool valid = true;

		for (size_t i = 8; i + 8 < c.frame.size(); i++)
		{
			const int32_t value = c.frame[i];
			if (ones == 5)
			{
				if (value)
				{
					valid = false;
					break;
				}

				ones = 0;

				continue;
			}

			ones = value ? ones + 1 : 0;
			byte |= uint8_t(value << position);

			position++;
			if (position == 8)
			{
				if (!bytes++)
					address = byte;

				crc ^= byte;
				for (int32_t k = 0; k < 8; k++)
					crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);

				byte = 0;
				position = 0;
			}
		}

		valid = valid && !position && bytes >= 4 && crc == 0xf0b8 && (address == 1 || address == 3);
		if (valid)
		{
			c.validated_bits += uint32_t(c.frame.size() - 8);
			c.valid_frames++;

			if (c.valid_frames == 1)
				c.confirmed = c.frame;
			else if (c.confirmed.size() >= uint32_t(m_cycle_bits))
				return true;
		}
		else
		{
			c.valid_frames = 0;
			c.validated_bits = 0;
			c.confirmed.clear();
			c.distance = 0;
			c.distance_count = 0;
		}
	}

	if (c.valid_frames && c.frame.size() == 16 && c.confirmed.size() >= uint32_t(m_cycle_bits) + 32)
		return true;

	if (c.frame.size() > 8)
		c.frame.erase(c.frame.begin(), c.frame.end() - 8);

	c.seen_flag = true;

	return false;
}

void wtvsoftmodem_v34rxsync::receive_bit(int32_t index, int32_t bit)
{
	auto &c = m_candidates[index];

	c.frame_bits++;

	if (m_locked >= 0)
	{
		if (m_drop > 0)
		{
			m_drop--;
		}
		else if (m_sink)
		{
			m_sink(bit);
		}

		return;
	}

	if (m_acquire_hdlc)
	{
		if (c.ready)
		{
			c.confirmed.push_back(bit);
		}
		else if (confirm_hdlc(c, bit))
		{
			c.ready = true;
		}

		return;
	}

	c.marks = bit ? c.marks + 1 : 0;

	if (m_tolerant_marks)
	{
		uint64_t &word = c.window[c.window_pos / 64];

		const uint64_t mask = uint64_t(1) << (c.window_pos % 64);

		c.window_ones -= (word & mask) ? 1 : 0;
		word = bit ? word | mask : word & ~mask;
		c.window_ones += bit;
		c.window_pos = (c.window_pos + 1) % WINDOW_BITS;

		if (c.window_count < WINDOW_BITS)
			c.window_count++;

		if (c.odp_bits)
		{
			if (uint32_t(bit) == c.window_bit((c.window_pos + WINDOW_BITS - (ODP_PERIOD_BITS + 1)) % WINDOW_BITS))
			{
				c.odp_bits++;
			}
			else
			{
				c.odp_bits = 0;
				c.detection.reset();
			}
		}
		else if (c.detection.rx_bit(bit))
		{
			c.odp_bits = ODP_DETECT_BITS;
		}

		if (c.odp_bits >= uint32_t(m_cycle_bits) + ODP_LOCK_MARGIN_BITS)
		{
			c.lock_kind = LockKind::ODP;
			c.lock_drop = 0;

			return;
		}
		if (c.window_count < WINDOW_BITS || c.window_ones < MARK_THRESHOLD)
		{
			return;
		}
	}
	else if (c.marks < MARK_LOCK_RUN)
	{
		return;
	}

	const uint64_t symbols = m_symbol - c.offset + 1;
	const uint64_t frame = c.phase + symbols / FRAME_SYMBOLS - 1;
	const uint64_t bit_position = frame * (m_b - 1) + frame * m_r / m_p + c.frame_bits;

	c.lock_kind = LockKind::MARKS;
	c.lock_drop = int32_t((8 - bit_position % 8) % 8);
}
