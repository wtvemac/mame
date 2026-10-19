// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34RXSYNC_H
#define MAME_WEBTV_WTVSOFTMODEM_V34RXSYNC_H

#pragma once

#include "wtvsoftmodem_bitblock.h"
#include "wtvsoftmodem_lapm.h"
#include "wtvsoftmodem_v34data.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#endif

// ITU-T V.34 8 Framing (mapping frame alignment from B1, 10.1.3.1)

class wtvsoftmodem_v34rxsync
{
public:
	using codec = wtvsoftmodem_v34data;

	wtvsoftmodem_v34rxsync();

	void init(const codec::config_t &cfg, bool acquire_b1 = false, bool acquire_hdlc = false, double gain_center = 1.0, int32_t gain_steps = 12, int32_t phase_steps = 0);

	void set_bit_sink(std::function<void(int32_t)> sink)
	{
		m_sink = std::move(sink);
	}

	bool locked() const
	{
		return m_locked >= 0;
	}

	double gain() const
	{
		return locked() ? m_candidates[m_locked].gain : 1.0;
	}

	double carrier_phase() const
	{
		return locked() ? m_candidates[m_locked].angle : 0.0;
	}

	std::unique_ptr<codec> take_decoder();
	void put_symbol(codec::point_t point);

private:

	static constexpr uint32_t WINDOW_BITS = 512;
	static constexpr uint32_t MARK_ERRORS = 26;
	static constexpr uint32_t MARK_THRESHOLD = WINDOW_BITS - MARK_ERRORS;
	static constexpr int32_t MARK_LOCK_RUN = 512;
	static constexpr uint32_t ODP_PERIOD_BITS = 40;
	static constexpr uint8_t HDLC_FLAG = 0x7e;
	static constexpr size_t MAX_FRAME_BITS = 4096;
	static constexpr size_t MAX_CONFIRMED_BITS = 131072;
	static constexpr size_t PARALLEL_MIN_CANDIDATES = 512;
	static constexpr size_t MAX_TASKS = 64;
	static constexpr uint32_t TASKS_PER_THREAD = 8;

	static constexpr int32_t FRAME_SYMBOLS = 8;
	static constexpr double GAIN_STEP = 0.005;
	static constexpr double PHASE_STEP = 0.025;

	static constexpr uint32_t ODP_DETECT_BITS = ODP_PERIOD_BITS + 8 * (wtvsoftmodem_lapm::ODP_CONFIRM_WINDOWS - 1);
	static constexpr uint32_t ODP_LOCK_MARGIN_BITS = 2 * ODP_PERIOD_BITS;
	static constexpr uint32_t ODP_REPLAY_BITS = 64;

	enum class LockKind : uint8_t
	{
		NONE,
		ODP,
		MARKS
	};

	class sync_workers
	{
	public:
		static uint32_t default_threads();

		explicit sync_workers(uint32_t threads);
		~sync_workers();
		sync_workers(const sync_workers &) = delete;
		sync_workers &operator=(const sync_workers &) = delete;

		uint32_t threads() const
		{
			return uint32_t(m_threads.size()) + 1;
		}

		template <typename F> void run(uint32_t count, F &task)
		{
			if (m_threads.empty() || count <= 1)
			{
				for (uint32_t i = 0; i < count; i++)
					task(i);

				return;
			}

			m_task = [](void *context, uint32_t i)
			{
				(*static_cast<F *>(context))(i);
			};

			m_context = &task;
			m_count = count;
			m_next.store(0, std::memory_order_relaxed);
			m_acks.store(0, std::memory_order_relaxed);
			m_generation.fetch_add(1, std::memory_order_release);
			m_generation.notify_all();

			execute();

			const uint32_t workers = uint32_t(m_threads.size());

			while (m_acks.load(std::memory_order_acquire) != workers)
				pause();
		}

	private:
		static constexpr uint32_t SPIN_ROUNDS = 20000;

		std::vector<std::thread> m_threads;
		void (*m_task)(void *, uint32_t) = nullptr;
		void *m_context = nullptr;
		uint32_t m_count = 0;
		alignas(64) std::atomic<uint32_t> m_generation{ 0 };
		alignas(64) std::atomic<uint32_t> m_next{ 0 };
		alignas(64) std::atomic<uint32_t> m_acks{ 0 };
		std::atomic<bool> m_stop{ false };

		static void pause()
		{
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
			_mm_pause();
#else
			std::this_thread::yield();
#endif
		}

		void execute();
		void work();
	};

	struct candidate
	{
		std::unique_ptr<codec> decoder;
		double gain = 1.0;
		double angle = 0;
		double cosine = 1;
		double sine = 0;
		uint32_t offset = 0;
		int32_t phase = 0;
		int32_t marks = 0;
		int32_t frame_bits = 0;
		uint32_t valid_frames = 0;
		bool ready = false;
		uint8_t flag = 0;
		std::array<uint64_t, WINDOW_BITS / 64> window{};
		uint32_t window_pos = 0;
		uint32_t window_count = 0;
		uint32_t window_ones = 0;
		uint32_t odp_bits = 0;
		wtvsoftmodem_lapm detection;

		bool seen_flag = false;
		uint32_t validated_bits = 0;
		double distance = 0;
		uint32_t distance_count = 0;
		std::vector<uint8_t> frame;
		std::vector<uint8_t> confirmed;
		LockKind lock_kind = LockKind::NONE;
		int32_t lock_drop = 0;
		uint32_t lock_from = 0;
		wtvsoftmodem_bitblock lock_rest;

		uint32_t window_bit(uint32_t pos) const
		{
			return uint32_t(window[pos / 64] >> (pos % 64)) & 1;
		}

		uint64_t window_bits(uint32_t pos, uint32_t len) const;
		void set_window_bits(uint32_t pos, uint32_t len, uint64_t value);
	};

	std::vector<candidate> m_candidates;
	std::function<void(int32_t)> m_sink;
	bool m_tolerant_marks = false;
	bool m_acquire_hdlc = false;
	int32_t m_cycle_bits = 0;
	int32_t m_locked = -1;
	int32_t m_drop = 0;
	int32_t m_b = 0;
	int32_t m_p = 1;
	int32_t m_r = 0;
	int32_t m_phases = 1;
	uint64_t m_symbol = 0;
	std::unique_ptr<sync_workers> m_workers;

	void process(int32_t index, codec::point_t point);
	void commit_lock(int32_t index);
	void receive_frame(int32_t index, const wtvsoftmodem_bitblock &block);
	uint32_t receive_hdlc_bits(candidate &c, const wtvsoftmodem_bitblock &block);
	bool receive_mark_frame(candidate &c, const wtvsoftmodem_bitblock &block);
	bool confirm_hdlc(candidate &c, int32_t bit);
	void receive_bit(int32_t index, int32_t bit);
};

#endif // MAME_WEBTV_WTVSOFTMODEM_V34RXSYNC_H
