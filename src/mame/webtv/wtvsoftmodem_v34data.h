// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_V34DATA_H
#define MAME_WEBTV_WTVSOFTMODEM_V34DATA_H

#pragma once

#include "wtvsoftmodem_bitblock.h"
#include "wtvsoftmodem_scrambler.h"
#include "wtvsoftmodem_v34trellis16.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// ITU-T V.34 8 Framing, 9 Encoder

class wtvsoftmodem_v34data
{

public:
	struct point_t
	{
		double i = 0.0;
		double q = 0.0;
	};

	struct config_t
	{
		int32_t baud_rate_code = 5;
		int32_t bit_rate_code = 20;
		bool answer_mode = true;
		bool expanded_shaping = false;
		double point_scale = 1.0;
		bool match_trn_power = true;

		bool invert_output = true;

		bool use_precoder = false;
		int16_t precoder_coeffs[3][2] = {
			{ 0, 0 },
            { 0, 0 },
            { 0, 0 }
		};

		double nonlin_k = 0.0;

		int32_t nonlin_prescale  = 512;
		int32_t nonlin_postscale = 512;
		bool    native_gains     = false;

		bool apply_tx_gains();
	};

	wtvsoftmodem_v34data();

	void set_bit_source(std::function<int32_t()> src)
	{
		m_get_bit = std::move(src);
	}

	void set_bit_sink(std::function<void(int32_t)> sink)
	{
		m_put_bit = std::move(sink);
	}

	void collect_bits(bool enable)
	{
		m_collect_bits = enable;
		m_collected    = {};
	}

	wtvsoftmodem_bitblock take_bits()
	{
		return m_collected.count ? std::exchange(m_collected, {}) : wtvsoftmodem_bitblock{};
	}

	bool frame_complete() const
	{
		return m_rx_sym_pos == 0;
	}
	
	bool trellis_active() const
	{
		return bool(m_rx_trellis);
	}

	void set_symbol_reference_sink(std::function<void(point_t, uint64_t)> sink)
	{
		m_reference_sink = std::move(sink);
	}

	const config_t &config() const
	{
		return m_cfg;
	}

	void rx_set_frame_phase(int32_t phase)
	{
		m_s_bit_cnt = (phase * m_r) % m_p;
	}

	int32_t b() const
	{
		return m_b;
	}

	int32_t k() const
	{
		return m_k;
	}

	int32_t q() const
	{
		return m_q;
	}

	int32_t j() const
	{
		return m_j;
	}

	int32_t p() const
	{
		return m_p;
	}

	int32_t r() const
	{
		return m_r;
	}

	int32_t w() const
	{
		return m_w;
	}

	int32_t mapping_M() const
	{
		return m_m;
	}

	int32_t bit_rate() const
	{
		return m_bit_rate;
	}

	void set_nonlin_scale(int32_t prescale, int32_t postscale)
	{
		m_cfg.nonlin_prescale  = prescale;
		m_cfg.nonlin_postscale = postscale;

		update_inverse_scales();
	}

	void set_exact_demap(bool enable)
	{
		m_exact_demap = enable;
	}

	const wtvsoftmodem_scrambler &scrambler() const
	{
		return m_scram;
	}

	void set_scrambler_state(uint64_t state)
	{
		m_scram.reset(state);
	}

	void init(const config_t &cfg);
	void reset();

	void set_precoder(const int16_t coeffs[3][2]);

	point_t next_symbol();

	bool put_symbol(point_t rx);
	bool put_symbol_shared(const wtvsoftmodem_v34data &geometry);

	void finish_shared_geometry();
	void observe_front_end(point_t rx);

	bool enable_trellis(uint64_t received_symbols_since_e);

	point_t snap(point_t rx) const;

	static uint32_t inverse_shell_map(int32_t m_ring_count, const int32_t mjk[8], bool *valid = nullptr);

	static void shell_map_fwd(int32_t m_ring_count, uint32_t r0, int32_t mjk[8]);

private:

	static constexpr int32_t  LATTICE_SCALE   = 128;
	static constexpr double   DEMAP_UNIT      = 128.0;
	static constexpr double   TRN_4PT_REF     = 5793.0;
	static constexpr uint32_t V0_REG_SEED     = 0xbfdc;
	static constexpr int32_t  COSET_WIDE_BITS = 56;
	static constexpr double   DEMAP_MAX_D2    = 1e18;

	static constexpr int32_t AXIS_LIMIT  = 45;
	static constexpr int32_t AXIS_LEVELS = AXIS_LIMIT + 1;

	static constexpr int32_t XOFF = 3;

	using axis_encoding = std::array<double, AXIS_LEVELS>;

	struct demapped_t
	{
		int32_t mi = -1;
		int32_t quadrant = 0;
		int32_t yre = 0;
		int32_t yim = 0;
		int32_t xre = 0;
		int32_t xim = 0;
	};

	struct rx_trellis_state
	{
		wtvsoftmodem::v34::trellis16::decoder         decoder;
		wtvsoftmodem::v34::trellis16::decoder::point4 pair {};

		uint32_t fill = 0;
		uint32_t sfs = 0;
		uint32_t reg = 0;
		uint32_t pending_symbols = 0;
		uint64_t output_epoch = 0;

		inline uint32_t next_inversion(uint32_t p)
		{
			uint32_t bit = 0;

			if (!sfs)
			{
				reg >>= 1;
				bit = reg & 1;

				if (!reg)
					reg = V0_REG_SEED;
			}

			sfs++;

			if (sfs == 2 * p)
				sfs = 0;

			return bit;
		}
	};

	struct decision_table
	{
		std::vector<point_t> points;
		std::vector<point_t> base;
		std::vector<std::array<int32_t, 2>> lattice;
		std::vector<std::array<uint8_t, 2>> axis;

		struct column
		{
			uint8_t first = AXIS_LEVELS;
			uint8_t last = 0;
		};

		bool contiguous_columns = true;
		std::array<column, AXIS_LEVELS> columns {};
		std::array<std::array<uint16_t, AXIS_LEVELS>, AXIS_LEVELS> column_index {};
		axis_encoding linear_axis {};
	};

	wtvsoftmodem_scrambler m_scram;
	wtvsoftmodem_bitblock m_collected;
	const wtvsoftmodem_v34data *m_shared_geometry = nullptr;

	std::array<std::array<int32_t, 2>, 3> m_front_xhist {};

	int32_t m_rx_sym_pos   = 0;
	bool m_rx_sync_ok   = true;
	bool m_collect_bits = false;
	int32_t m_b = 0;
	int32_t m_k = 0;
	int32_t m_q = 0;
	int32_t m_m = 0;
	int32_t m_j = 0;
	int32_t m_p = 0;
	int32_t m_r = 0;
	int32_t m_w = 0;
	int32_t m_s_bit_cnt   = 0;
	int32_t m_aux_bit_cnt = 0;

	std::array<point_t, 8> m_rx_sym {};
	std::array<point_t, 8> m_rx_linear {};

	std::shared_ptr<rx_trellis_state> m_rx_trellis;
	std::function<void(point_t, uint64_t)> m_reference_sink;


	bool m_exact_demap   = false;
	int32_t m_rx_top2[4]    = { 0, 0, 0, 0 };
	int32_t m_rx_subsets[2] = { 0, 0 };
	int32_t m_rx_u0         = 0;

	std::function<void(int32_t)> m_put_bit;

	config_t m_cfg;

	int32_t m_bit_rate = 0;
	int32_t m_sfs_count = 0;
	uint32_t m_v0_reg = V0_REG_SEED;

	uint32_t m_r0 = 0;
	int32_t m_ibits[4] = { 0, 0, 0, 0 };
	int32_t m_qbits[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int32_t m_mjk[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	int32_t m_z = 0;
	int32_t m_y0 = 0;
	int32_t m_state = 0;

	int32_t m_xhist[11][2] = {};
	int32_t m_c_re = 0;
	int32_t m_c_im = 0;
	int32_t m_p_re = 0;
	int32_t m_p_im = 0;
	double m_q7_scale = 1.0;
	double m_pow_norm = 1.0;

	std::array<point_t, 8> m_sym {};
	int32_t m_sym_pos = 8;

	double m_nrm = 1.0;

	std::function<int32_t()> m_get_bit;

	std::array<uint8_t, 96> m_framebits {};	

	std::shared_ptr<const decision_table> m_decisions;
	double m_decision_scale = 1.0;

	int32_t m_inverse_prescale = 1 << 14;
	int32_t m_inverse_postscale = 1 << 14;

	inline static constexpr uint8_t axis_index(int32_t coordinate)
	{
		return uint8_t((coordinate + AXIS_LIMIT) / 2);
	}

	inline bool separable_encoding() const
	{
		return m_cfg.nonlin_k == 0.0;
	}

	inline void update_inverse_scales()
	{
		if (m_cfg.nonlin_postscale)
			m_inverse_postscale = (512 << 14) / m_cfg.nonlin_postscale;

		if (m_cfg.nonlin_prescale)
			m_inverse_prescale = (512 << 14) / m_cfg.nonlin_prescale;
	}

	void map_frame();
	void parse_bitstream();
	void shell_map();

	void demap_frame_finish();
	demapped_t demap_trellis(const point_t &sym, bool &ok) const;
	demapped_t demap_exact(const point_t &sym) const;
	demapped_t demap_nearest(const point_t &linear, bool &ok) const;
	void emit_reference(int32_t xre, int32_t xim);
	void track_quadrant(int32_t step, int32_t quadrant);
	void advance_rx_precoder(int32_t step, int32_t xre, int32_t xim);
	void advance_rx_trellis(int32_t step, int32_t c_prev_re, int32_t c_prev_im);
	void unparse_bitstream(uint32_t r0, const int32_t ibits[4], const int32_t qbits[8], bool deliver = true);
	bool reverse_lookup(int32_t yre, int32_t yim, int32_t &mi, int32_t &quadrant) const;

	point_t receive_linear(point_t rx) const;
	void observe_linear(point_t linear);
	void store_received(point_t rx, point_t linear);
	point_t front_prediction() const;
	void encode_q7(int32_t &re, int32_t &im) const;
	point_t encode_candidate(int32_t re, int32_t im) const;

	void encode_axis(int32_t offset, axis_encoding &out) const;
	int32_t nearest_separable(point_t rx, const axis_encoding &re, const axis_encoding &im, double limit) const;
};

#endif // MAME_WEBTV_WTVSOFTMODEM_V34DATA_H
