// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_DSP_H
#define MAME_WEBTV_WTVSOFTMODEM_DSP_H

#pragma once

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
	#define M_SQRT2 1.41421356237309504880
#endif

class wtvsoftmodem_dsp
{

public:

	// Resolution of the I and Q table
	static constexpr uint32_t    IQ_TABLE_BITS     = 7;
	static constexpr uint32_t    IQ_TABLE_SIZE     = (1 << IQ_TABLE_BITS);

	typedef int16_t sample_t;

	typedef uint16_t phase_t;

	typedef double frequency_t;

	typedef struct trig_entry
	{
		double raw;
		sample_t sample;
	} trig_entry_t;

	typedef struct iqtable
	{
		uint32_t table_size;
		sample_t itable[IQ_TABLE_SIZE];
		sample_t qtable[IQ_TABLE_SIZE];
	} iqtable_t;

	typedef struct goertzel_state
	{
		uint32_t samples_processed;
		uint32_t samples_per_block;
		double omega;
		double coeff;
		double q[2];
	} goertzel_state_t;

	typedef struct bandpass_state
	{
		double b0;     // b1 = 0, b2 = -b0
		double a1;
		double a2;
		double x[2];   // x[n-1], x[n-2]
		double y[2];   // y[n-1], y[n-2]
	} bandpass_state_t;

	struct oscillator
	{
		static constexpr uint32_t RENORMALIZE_INTERVAL = 1024;

		double re = 1.0;
		double im = 0.0;
		double step_re = 1.0;
		double step_im = 0.0;
		uint32_t count = 0;

		void set_step(double radians)
		{
			step_re = std::cos(radians); 
			step_im = std::sin(radians);
		}
		void set_phase(double radians)
		{
			re = std::cos(radians);
			im = std::sin(radians);
			count = 0;
		}

		void advance()
		{
			const double next_re = re * step_re - im * step_im;
			im = re * step_im + im * step_re;
			re = next_re;
			count++;
			if (count == RENORMALIZE_INTERVAL)
			{
				count = 0;
				const double scale = 1.0 / std::sqrt(re * re + im * im);
				re *= scale;
				im *= scale;
			}
		}
	};

	static constexpr frequency_t DEFAULT_SAMPLE_RATE   = 16000;

	static constexpr double      M_TAU                 = 2 * M_PI;

	wtvsoftmodem_dsp();

	static inline double wrap_radians(double phase)
	{
		while (phase >= wtvsoftmodem_dsp::M_TAU)
			phase -= wtvsoftmodem_dsp::M_TAU;

		while (phase < 0.0)
			phase += wtvsoftmodem_dsp::M_TAU;

		return phase;
	}

	static inline double radians_per_sample(frequency_t frequency, frequency_t sample_rate)
	{
		return wtvsoftmodem_dsp::M_TAU * frequency / sample_rate;
	}

	template <typename T = double>
	static inline T ms_to_samples(double ms, frequency_t sample_rate)
	{
		return T(ms * sample_rate / 1000.0);
	}

	static inline void clear_samples(int16_t *sample, uint32_t sample_count, uint32_t first_idx = 0)
	{
		if (first_idx < sample_count)
			std::fill(sample + first_idx, sample + sample_count, int16_t(0));
	}

	static inline double db_to_amplitude(double db)
	{
		return std::pow(10.0, db / 20.0);
	}

	static inline double amplitude_to_db(double amplitude)
	{
		return 20 * std::log10(amplitude);
	}

	static inline double power_to_db(double power)
	{
		return 10 * std::log10(power);
	}

	static inline sample_t saturate(int32_t value)
	{
		auto clamped = std::clamp(
			value,
			static_cast<int32_t>(std::numeric_limits<sample_t>::min()),
			static_cast<int32_t>(std::numeric_limits<sample_t>::max())
		);

		return static_cast<sample_t>(clamped);
	}

	static inline uint32_t popcount(uint64_t v)
	{
		v = v - ((v >> 1) & 0x5555555555555555ULL);
		v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
		v = (v + (v >> 4)) & 0x0f0f0f0f0f0f0f0fULL;

		return uint32_t((v * 0x0101010101010101ULL) >> 56);
	}

	template <typename T = long>
	static inline T round_nearest(double value)
	{
		const long whole = long(value);
		const double fraction = value - double(whole);

		return T(whole + (fraction >= 0.5) - (fraction <= -0.5));
	}

	static inline sample_t saturate_round(double value)
	{
		long long rounded = std::llround(value);

		auto clamped = std::clamp(
			rounded,
			static_cast<long long>(std::numeric_limits<sample_t>::min()),
			static_cast<long long>(std::numeric_limits<sample_t>::max())
		);

		return static_cast<sample_t>(clamped);
	}

	inline trig_entry_t sin(phase_t phase_angle) const
	{
		phase_angle >>= (TRIG_DOMAIN_BITS - TRIG_TABLE_BITS);

		return sin_table[phase_angle & (TRIG_TABLE_SIZE - 1)];
	}

	// Value to advance of the domain (phase/time) per sample to play a frequency at the given sample rate.
	// This value is passed to sin() to produce a sample value.
	phase_t get_padvance(frequency_t frequency, frequency_t sample_rate);

	// Value to advance the domain (phase/time) such that the value of sin() is inverted compared to the previous sample.
	// For example: if the previous sample value is 10 then this will advance the played frequency so it produces -10 for the next sample.
	// this is used to do a phase reversal for the ANSam tone.
	phase_t get_pradvance();

	// Value of the domain (phase/time) to modulate the amplitude at a given frequency and at the given sample rate. Used for AM needed to produce some tones.
	// This value is passed to sin() to to produce an amplitude value that's used to adjust the sample value.
	phase_t get_aadvance(frequency_t frequency, frequency_t sample_rate);

	// Discrete Fourier transform using the Goertzel algorithm 
	// Info: https://eng.libretexts.org/Bookshelves/Electrical_Engineering/Signal_Processing_and_Modeling/Fast_Fourier_Transforms_(Burrus)/04%3A_The_DFT_as_Convolution_or_Filtering/4.04%3A_Goertzel's_Algorithm_or_A_Better_DFT_Algorithm
	static inline goertzel_state_t goertzel_init(frequency_t frequency, frequency_t sample_rate, uint32_t samples_per_block, bool round_to_bin = true)
	{
		// round_to_bin moves the frequency to the nearest bin centre of the block (k = rint(f * N / fs)).
		double omega;
		if (round_to_bin)
		{
			double k = std::rint(frequency * ((double)samples_per_block / sample_rate));
			omega = k * (M_TAU / (double)samples_per_block);
		}
		else
		{
			omega = M_TAU * (frequency / sample_rate);
		}

		return {
			.samples_processed = 0,
			.samples_per_block = samples_per_block,
			.omega             = omega,
			.coeff             = 2.0 * std::cos(omega),
			.q                 = { 0, 0 }
		};
	}
	static inline void goertzel_put(goertzel_state_t* state, double sample)
	{
		double q    = state->coeff * state->q[0] - state->q[1] + sample;
		state->q[1] = state->q[0];
		state->q[0] = q;

		state->samples_processed++;
	}
	template <typename T>
	static inline uint32_t goertzel_update(goertzel_state_t* state, const T* sample, uint32_t sample_count)
	{
		uint32_t remaining = (state->samples_processed < state->samples_per_block) ? (state->samples_per_block - state->samples_processed) : 0;
		uint32_t take = std::min(sample_count, remaining);

		for (uint32_t sidx = 0; sidx < take; sidx++)
			goertzel_put(state, (double)sample[sidx]);

		return take;
	}
	static inline bool goertzel_ready(const goertzel_state_t* state)
	{
		return (state->samples_processed >= state->samples_per_block);
	}
	static inline double goertzel_power(const goertzel_state_t* state)
	{
		// |X(k)|^2
		return (state->q[0] * state->q[0]) + (state->q[1] * state->q[1]) - (state->coeff * state->q[0] * state->q[1]);
	}
	static inline std::complex<double> goertzel_value(const goertzel_state_t* state)
	{
		// X(k) = sum(x[n] * e^(-j*omega*n))
		std::complex<double> y(state->q[0] - (std::cos(state->omega) * state->q[1]), std::sin(state->omega) * state->q[1]);

		if (state->samples_processed == 0)
			return 0;
		else
			return y * std::polar(1.0, -state->omega * (double)(state->samples_processed - 1));
	}
	static inline void goertzel_reset(goertzel_state_t* state)
	{
		state->samples_processed = 0;
		state->q[0] = 0;
		state->q[1] = 0;
	}

	static inline bandpass_state_t bandpass_init(frequency_t center_frequency, frequency_t sample_rate, double q)
	{
		const double w     = wtvsoftmodem_dsp::radians_per_sample(center_frequency, sample_rate);
		const double alpha = std::sin(w) / (2 * q);

		return {
			.b0 = alpha / (1 + alpha),
			.a1 = -2 * std::cos(w) / (1 + alpha),
			.a2 = (1 - alpha) / (1 + alpha),
			.x  = { 0, 0 },
			.y  = { 0, 0 }
		};
	}
	static inline double bandpass_put(bandpass_state_t* state, double sample)
	{
		const double filtered = state->b0 * (sample - state->x[1]) - state->a1 * state->y[0] - state->a2 * state->y[1];

		state->x[1] = state->x[0];
		state->x[0] = sample;
		state->y[1] = state->y[0];
		state->y[0] = filtered;

		return filtered;
	}

	static inline double cubic(double p0, double p1, double p2, double p3, double t)
	{
		double a0 = p1;
		double a1 = 0.5 * (p2 - p0);
		double a2 = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3;
		double a3 = -0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3;

		return ((a3 * t + a2) * t + a1) * t + a0;
	}

	static inline void loop_gains(double bn, double &kp, double &ki)
	{
		if (bn <= 0.0)
		{
			kp = 0.0;
			ki = 0.0;
		}
		else
		{
			const double z = 0.70710678;
			double d = 1.0 + 2.0 * z * bn + bn * bn;

			kp = (4.0 * z * bn) / d;
			ki = (4.0 * bn * bn) / d;
		}
	}

	static inline double hamming_window(size_t index, size_t length)
	{
		return 0.54 - 0.46 * std::cos(wtvsoftmodem_dsp::M_TAU * (double)index / (double)(length - 1));
	}
	static inline double blackman_window(size_t index, size_t length)
	{
		return 0.42 - 0.5 * std::cos(wtvsoftmodem_dsp::M_TAU * (double)index / (double)(length - 1)) + 0.08 * std::cos(2.0 * wtvsoftmodem_dsp::M_TAU * (double)index / (double)(length - 1));
	}

	// Root-raised-cosine pulse
	static inline double rrc_pulse(double t, double beta)
	{
		if (beta <= 0.0)
		{
			if (std::fabs(t) < 1e-9)
			{
				return 1.0;
			}
			else
			{
				double x = M_PI * t;

				return std::sin(x) / x;
			}
		}
		else if (std::fabs(t) < 1e-9)
		{
			return 1.0 - beta + (4.0 * beta / M_PI);
		}
		else
		{
			double sing = 1.0 / (4.0 * beta);
			if (std::fabs(std::fabs(t) - sing) < 1e-7)
			{
				double a = M_PI / (4.0 * beta);

				return (beta / std::sqrt(2.0)) * ((1.0 + 2.0 / M_PI) * std::sin(a) + (1.0 - 2.0 / M_PI) * std::cos(a));
			}
			else
			{
				double pit = M_PI * t;
				double num = std::sin(pit * (1.0 - beta)) + 4.0 * beta * t * std::cos(pit * (1.0 + beta));
				double den = pit * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));

				return num / den;
			}
		}
	}
	static inline std::vector<double> design_hilbert(size_t length)
	{
		std::vector<double> taps(length, 0.0);
		const int32_t centre = int32_t(length / 2);

		for (size_t k = 0; k < length; k++)
		{
			int32_t m = int32_t(k) - centre;

			if (m & 1)
				taps[k] = (2.0 / (M_PI * m)) * wtvsoftmodem_dsp::hamming_window(k, length);
		}

		return taps;
	}
	static inline void design_lowpass(double* taps, size_t length, double cutoff)
	{
		const double centre = double((length - 1) / 2);

		for (size_t k = 0; k < length; k++)
		{
			const double t = double(k) - centre;
			const double sinc = t == 0.0 ? 2.0 * cutoff : std::sin(wtvsoftmodem_dsp::M_TAU * cutoff * t) / (M_PI * t);

			taps[k] = sinc * wtvsoftmodem_dsp::blackman_window(k, length);
		}
	}

	template <typename T>
	static inline std::complex<double> fir_response(const T* taps, size_t count, frequency_t frequency, frequency_t sample_rate, double delay = 0.0)
	{
		double re = 0.0;
		double im = 0.0;

		for (size_t n = 0; n < count; n++)
		{
			const double phase = -wtvsoftmodem_dsp::M_TAU * frequency * ((double)n - delay) / sample_rate;

			re += taps[n] * std::cos(phase);
			im += taps[n] * std::sin(phase);
		}

		return { re, im };
	}

	template <typename T>
	static inline std::vector<double> resample_fir(const T* source, size_t source_count, frequency_t source_rate, double source_delay, frequency_t sample_rate, int32_t length, int32_t grid_points = 512)
	{
		if (!(length & 1))
			length++;

		const int32_t half = length / 2;
		std::vector<double> taps(length, 0.0);

		std::vector<std::complex<double>> response(grid_points + 1);
		for (int32_t g = 0; g <= grid_points; g++)
		{
			const double f = (sample_rate * 0.5) * double(g) / grid_points;
			if (f <= source_rate * 0.5)
				response[g] = wtvsoftmodem_dsp::fir_response(source, source_count, f, source_rate, source_delay);
		}

		for (int32_t n = 0; n < length; n++)
		{
			double acc = 0.0;
			for (int32_t g = 0; g <= grid_points; g++)
			{
				double f = (sample_rate * 0.5) * double(g) / grid_points;

				const double re = response[g].real();
				const double im = response[g].imag();

				double ph = wtvsoftmodem_dsp::M_TAU * f * (n - half) / sample_rate;
				double w  = (g == 0 || g == grid_points) ? 0.5 : 1.0;

				acc += w * (re * std::cos(ph) - im * std::sin(ph));
			}

			acc *= 2.0 / grid_points;
			taps[n] = acc * wtvsoftmodem_dsp::hamming_window(n, length);
		}

		return taps;
	}

	template <typename T>
	class delay_line
	{
	public:

		delay_line() = default;
		explicit delay_line(size_t length)
		{
			assign(length);
		}
		void assign(size_t length, const T& value = T{})
		{
			samples.assign(2 * length, value);
			line_length = length;
			sample_idx = 0;
		}
		void clear()
		{
			samples.clear();
			line_length = 0;
			sample_idx = 0;
		}
		bool empty() const
		{
			return line_length == 0;
		}
		size_t size() const
		{
			return line_length;
		}
		void push(const T& sample)
		{
			samples[sample_idx] = sample;
			samples[sample_idx + line_length] = sample;
			sample_idx++;
			if (sample_idx == line_length)
				sample_idx = 0;
		}

		const T& operator[](size_t age) const
		{
			return newest()[-std::ptrdiff_t(age)];
		}

		const T& slot(size_t index) const
		{
			return samples[index];
		}

		template <typename F>
		void visit(size_t count, F&& visitor, size_t first_age = 0) const
		{
			if (!count)
				return;

			if (first_age + count <= line_length)
			{
				const T *line = newest() - first_age;

				for (size_t k = 0; k < count; k++)
					visitor(k, line[-std::ptrdiff_t(k)]);
			}
			else
			{
				size_t idx = wrapped_index(first_age);

				for (size_t k = 0; k < count; k++)
				{
					visitor(k, samples[idx]);
					idx = (idx == 0 ? line_length : idx) - 1;
				}
			}
		}

		template <typename A, typename F>
		A accumulate(A acc, size_t count, F&& mac, size_t first_age = 0) const
		{
			if (!count)
				return acc;

			if (first_age + count <= line_length)
			{
				const T *line = newest() - first_age;

				for (size_t k = 0; k < count; k++)
					acc = mac(acc, k, line[-std::ptrdiff_t(k)]);
			}
			else
			{
				size_t idx = wrapped_index(first_age);

				for (size_t k = 0; k < count; k++)
				{
					acc = mac(acc, k, samples[idx]);
					idx = (idx == 0 ? line_length : idx) - 1;
				}
			}
			return acc;
		}

	private:

		std::vector<T> samples;
		size_t line_length = 0;
		size_t sample_idx = 0;

		const T* newest() const
		{
			return samples.data() + sample_idx + line_length - 1;
		}

		size_t wrapped_index(size_t age) const
		{
			return (sample_idx + line_length - 1 - age % line_length) % line_length;
		}
	};

	void populate_i_table(wtvsoftmodem_dsp::sample_t* itable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate);
	void populate_q_table(wtvsoftmodem_dsp::sample_t* qtable, uint32_t size, wtvsoftmodem_dsp::frequency_t frequency, wtvsoftmodem_dsp::frequency_t sample_rate);
	void populate_iq_table(iqtable_t* table, uint32_t size, frequency_t frequency, frequency_t sample_rate);
	inline int32_t circular_dot_product(sample_t* table0, sample_t* circular_table1, uint32_t table_size, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{
		int32_t dot_product = 0;

		for (uint32_t fdix = 0, sidx = sidx_start; fdix < table_size; fdix++, sidx++)
			dot_product += (table0[fdix] * circular_table1[sidx & sidx_mask]);

		dot_product >>= TRIG_RANGE_BITS;

		return dot_product;
	}
	inline int32_t get_iq_similarity(wtvsoftmodem_dsp::sample_t* itable, wtvsoftmodem_dsp::sample_t* qtable, uint32_t table_size, sample_t* samples, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{
		// I and Q circular dot product
		int32_t idot_product = 0;
		int32_t qdot_product = 0;
		for (uint32_t fdix = 0, sidx = sidx_start; fdix < table_size; fdix++, sidx++)
		{
			idot_product += (itable[fdix] * samples[sidx & sidx_mask]);
			qdot_product += (qtable[fdix] * samples[sidx & sidx_mask]);
		}
		idot_product >>= TRIG_RANGE_BITS;
		qdot_product >>= TRIG_RANGE_BITS;

		int32_t power = (idot_product * idot_product) + (qdot_product * qdot_product);

		return power;
	}
	inline int32_t get_iq_similarity(iqtable_t* iqtable, sample_t* samples, uint32_t sidx_start = 0, uint32_t sidx_mask = 0xffffffff)
	{
		return get_iq_similarity(iqtable->itable, iqtable->qtable, iqtable->table_size, samples, sidx_start, sidx_mask);
	}

private:

	// This defines the resolution of the input data into sin() or cos()
	// Must be at or higher than TRIG_TABLE_BITS and at or lower than phase_t's bit count.
	// Will be reduced to TRIG_TABLE_BITS to do a trig table lookup
	static constexpr uint32_t    TRIG_DOMAIN_BITS  = 16;
	static constexpr double      TRIG_DOMAIN_LIMIT = (1 << TRIG_DOMAIN_BITS);

	// This defines the range of the output data from sin() or cos()
	// TRIG_RANGE_LIMIT must me less than max(trig_entry_t)/2.
	// The more bits used here will increase the amplitude range (and thus be louder)
	static constexpr uint32_t    TRIG_RANGE_BITS   = 13;
	static constexpr double      TRIG_RANGE_LIMIT  = (1 << TRIG_RANGE_BITS);

	// This defines the resolution of the main table
	// The higher this is the more memory the table will take up.
	static constexpr uint32_t    TRIG_TABLE_BITS   = 14;
	static constexpr uint32_t    TRIG_TABLE_SIZE   = (1 << TRIG_TABLE_BITS);


	static const trig_entry_t* shared_sin_table();
	static void populate_sin_table(trig_entry_t* table, uint32_t size);
	static void populate_cos_table(trig_entry_t* table, uint32_t size);

	const trig_entry_t* sin_table;

};

#endif // MAME_WEBTV_WTVSOFTMODEM_DSP_H