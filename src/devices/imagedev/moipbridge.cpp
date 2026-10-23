// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "emuopts.h"
#include "moipbridge.h"
#include <stdexcept>

DEFINE_DEVICE_TYPE(VOIPBRIDGE, moipbridge_device, "moipbridge", "MoIP (SIP) Bridge")

moipbridge_device::moipbridge_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, double sample_rate) :
	device_t(mconfig, VOIPBRIDGE, tag, owner, clock),
	device_image_interface(mconfig, *this),
	m_machine_sample_rate(sample_rate)
{
}

moipbridge_device::~moipbridge_device()
{
	if (m_sip)
		m_sip->shutdown();
}

void moipbridge_device::device_start()
{
	//
}

void moipbridge_device::device_stop()
{
	if (m_sip)
		m_sip->shutdown();
}

std::pair<std::error_condition, std::string> moipbridge_device::call_load()
{

	if (!sip_client::validate_uri(filename()))
		return std::make_pair(image_error::INVALIDIMAGE, "expected sip:<user>[:<password>]@<host>[:<port>]");

	sip_client::SipLoggingLevel logging_level;
	if (machine().options().debug())
		logging_level = sip_client::SipLoggingLevel::SIP_LOG_DEBUG;
	else if (machine().options().verbose())
		logging_level = sip_client::SipLoggingLevel::SIP_LOG_VERBOSE;
	else
		logging_level = sip_client::SipLoggingLevel::SIP_LOG_NONE;

	m_sip = std::make_unique<sip_client>(filename(), m_machine_sample_rate, logging_level);

	std::string error;
	if (!m_sip->start_sip(error))
		return std::make_pair(image_error::UNSPECIFIED, error);

	return std::make_pair(std::error_condition(), std::string());
}

void moipbridge_device::call_unload()
{
	if (m_sip)
		m_sip->shutdown();
}

void moipbridge_device::dial(const std::string &number)
{
	if (m_sip)
		m_sip->dial(number);
}

void moipbridge_device::hangup()
{
	if (m_sip)
		m_sip->hangup();
}

const bool moipbridge_device::call_active()
{
	return m_sip && m_sip->call_active();
}

uint32_t moipbridge_device::pull(int16_t* sample, uint32_t sample_count)
{
	if (m_sip)
		return m_sip->pull(sample, sample_count);
	else
		return 0;
}

void moipbridge_device::push(const int16_t* sample, uint32_t sample_count)
{
	if (m_sip && m_sip->call_active())
		m_sip->push(sample, sample_count);
}

void moipbridge_device::reset_audio()
{
	if (m_sip)
		m_sip->reset_audio();
}

// ----------------------------------------------------------------------------
// SIP sample buffer: up/down sampling (interpolation/decimation), filtering 
// and call quality adjustments for modems.
// ----------------------------------------------------------------------------

moip_sample_ring_buffer::moip_sample_ring_buffer(uint32_t capacity, double machine_sample_rate, bool bypass_resampler, double gain_adjust, double ppm_offset, uint32_t delay_samples, uint8_t tap_count, double cutoff_freqlo, double cutoff_freqhi, double slattdb)
	: m_machine_sample_rate(machine_sample_rate),
	m_bypass_resampler(bypass_resampler),
	m_tap_count(tap_count),
	m_cutoff_freqlo(cutoff_freqlo),
	m_cutoff_freqhi(cutoff_freqhi),
	m_beta(moip_sample_ring_buffer::calculate_kaiser_shape(slattdb)),
	m_moip_coeffs(
		std::size(MOIP_SAMPLE_RATES),
		coeff_table_t
		{
			.sample_rate = 0.0,
			.coeffs = std::vector<std::vector<double>>(tap_count, std::vector<double>(moip_sample_ring_buffer::SRB_POLYNOMIAL_ORDER + 1, 0.0)),
		}
	),
	m_machine_coeffs(
		{
			.sample_rate = machine_sample_rate,
			.coeffs = std::vector<std::vector<double>>(tap_count, std::vector<double>(moip_sample_ring_buffer::SRB_POLYNOMIAL_ORDER + 1, 0.0)),
		}
	),
	m_gain_adjust(gain_adjust),
	m_ppm_offset(ppm_offset * 1e-6),
	m_delay_samples(delay_samples),
	m_within_delay(delay_samples > 0)
{
	assert(m_machine_sample_rate >= MOIP_MAX_SAMPLE_RATE);

	uint32_t alloced_capacity = std::max(std::bit_ceil(capacity), (uint32_t)2);

	m_buf = std::make_unique<sample_t[]>(alloced_capacity);
	m_mask = alloced_capacity - 1;
	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);

	m_push_hist.assign(m_tap_count, 0.0);
	m_pull_hist.assign(m_tap_count, 0.0);

	uint32_t rate_idx = 0;
	for (double sample_rate : MOIP_SAMPLE_RATES)
	{
		moip_sample_ring_buffer::build_coeffs(m_moip_coeffs[rate_idx], sample_rate, true);
		rate_idx++;
	}
	moip_sample_ring_buffer::build_coeffs(m_machine_coeffs, machine_sample_rate, false);

	clear();
}

// https://www.mathworks.com/help/signal/ref/kaiser.html
// https://metricgate.com/docs/kaiser-window/
const double moip_sample_ring_buffer::calculate_kaiser_shape(double slattdb)
{
	if (slattdb > 50)
		return 0.1102 * (slattdb - 8.7000);
	else if(slattdb >= 21 && slattdb <= 50)
		return (0.5842 * std::pow((slattdb - 21.0), 0.4) + 0.07886 * (slattdb - 21.0));
	else
		return 0.0;
}

const double moip_sample_ring_buffer::besselI0(double x)
{
	double sum = 1.0;
	double term = 1.0;
	double k = 0.0;

	while (true)
	{
		k += 1.0;
		double temp = x / (2.0 * k);
		term *= temp * temp;
		sum += term;

		if (term < 1e-12 * sum || k > 50)
			break;
	}
	return sum;
}

void moip_sample_ring_buffer::build_coeffs(coeff_table_t &target_table, uint32_t input_rate, bool advancing)
{
	target_table.sample_rate = input_rate;

	const double freq_lo = m_cutoff_freqlo / static_cast<double>(input_rate);
	const double freq_hi = m_cutoff_freqhi / static_cast<double>(input_rate);

	const double center = static_cast<double>(m_tap_count - 1) / 2.0;

	const int order = moip_sample_ring_buffer::SRB_POLYNOMIAL_ORDER;
	const int num_coeffs = order + 1;

	const int fit_points = 32;
	std::vector<double> mu_points(fit_points);
	for (int p = 0; p < fit_points; ++p)
		mu_points[p] = static_cast<double>(p) / static_cast<double>(fit_points - 1);

	for (int t = 0; t < m_tap_count; ++t)
	{
		std::vector<std::vector<double>> ATA(num_coeffs, std::vector<double>(num_coeffs, 0.0));
		std::vector<double> ATb(num_coeffs, 0.0);

		for (int p = 0; p < fit_points; ++p)
		{
			double mu = mu_points[p];
			double n_idx = (static_cast<double>(t) - center) + (advancing ? mu : -mu);

			double sinc;
			if (std::abs(n_idx) < 1e-9)
			{
				sinc = 2.0 * (freq_hi - freq_lo);
			}
			else
			{
				sinc = (std::sin(2.0 * M_PI * freq_hi * n_idx) - std::sin(2.0 * M_PI * freq_lo * n_idx)) 
					/ (M_PI * n_idx);
			}

			double arg = 1.0 - std::pow((static_cast<double>(t) - center) / center, 2.0);
			arg = std::max(0.0, arg);
			double window = moip_sample_ring_buffer::besselI0(m_beta * std::sqrt(arg)) / moip_sample_ring_buffer::besselI0(m_beta);
			double h_ideal = sinc * window;

			std::vector<double> phi(num_coeffs);
			phi[0] = 1.0;
			for (int r = 1; r < num_coeffs; ++r)
			{
				phi[r] = phi[r - 1] * mu;
			}

			for (int r = 0; r <= order; ++r)
			{
				ATb[r] += phi[r] * h_ideal;
				for (int c = 0; c <= order; ++c)
				{
					ATA[r][c] += phi[r] * phi[c];
				}
			}
		}

		std::vector<std::vector<double>> aug(num_coeffs, std::vector<double>(num_coeffs + 1, 0.0));
		for (int r = 0; r <= order; ++r)
		{
			for (int c = 0; c <= order; ++c)
				aug[r][c] = ATA[r][c];

			aug[r][num_coeffs] = ATb[r];
		}

		for (int i = 0; i <= order; ++i)
		{
			double pivot = aug[i][i];
			int pivot_row = i;

			for (int r = i + 1; r <= order; ++r)
			{
				if (std::abs(aug[r][i]) > std::abs(pivot))
				{
					pivot = aug[r][i];
					pivot_row = r;
				}
			}

			if (pivot_row != i)
			{
				for (int c = 0; c <= num_coeffs; ++c)
					std::swap(aug[i][c], aug[pivot_row][c]);
			}

			if (std::abs(aug[i][i]) > 1e-12)
			{
				for (int c = i + 1; c <= num_coeffs; ++c)
					aug[i][c] /= aug[i][i];
				aug[i][i] = 1.0;

				for (int r = 0; r <= order; ++r)
				{
					if (r != i)
					{
						double factor = aug[r][i];
						for (int c = i; c <= num_coeffs; ++c)
							aug[r][c] -= factor * aug[i][c];
					}
				}
			}
		}

		for (int r = 0; r <= order; ++r)
			target_table.coeffs[t][r] = aug[r][num_coeffs];
	}
}

void moip_sample_ring_buffer::history_push(std::vector<double> &hist, size_t &head, double sample)
{
	head = (head - 1 + hist.size()) % hist.size();
	hist[head] = sample;
}

const moip_sample_ring_buffer::sample_t moip_sample_ring_buffer::fir(const coeff_table_t &target_table, std::vector<double> &hist, size_t &head, double sub_frac, double gain)
{
	double sum = 0.0;

	double mu = sub_frac;

	for (int t = 0; t < m_tap_count; ++t)
	{
		int hist_idx = (head + t) % hist.size();

		double sample = hist[hist_idx];

		const auto &c = target_table.coeffs[t];

		double tap_coeff = c.back();
		for (int r = int(c.size()) - 2; r >= 0; --r)
			tap_coeff = tap_coeff * mu + c[r];

		sum += sample * tap_coeff;
	}

	return clamp<sample_t>(sum * gain);
}

template <std::integral T>
constexpr T moip_sample_ring_buffer::clamp(double v)
{
	long long rounded = std::llround(v);

	auto clamped = std::clamp(
		rounded,
		static_cast<long long>(std::numeric_limits<T>::min()),
		static_cast<long long>(std::numeric_limits<T>::max())
	);

	return static_cast<T>(clamped);
}

template <std::integral T>
const T moip_sample_ring_buffer::get_machine_sample(sample_t sample)
{
	// The WebTV driver passes a full uint32_t* buffer with left and right channels.
	// only the right channel sample_t is used so if we see T=uint32_t then store the right channel sample_t only
	if constexpr (std::is_same_v<T, uint32_t>)
		// Conversion to uint16_t is used so the sample_t sign bit doesn't get mapped to the 32-bit sign bit.
		return static_cast<T>(static_cast<uint16_t>(sample));
	else
		return static_cast<T>(sample);
}

template <std::integral T>
const moip_sample_ring_buffer::sample_t moip_sample_ring_buffer::get_moip_sample(T sample)
{
	// The WebTV driver passes a full uint32_t* buffer with left and right channels.
	// only the right channel sample_t is used so if we see T=uint32_t then isolate the right channel sample_t
	if constexpr (std::is_same_v<T, uint32_t>)
		// Conversion to uint16_t is used so the 32-bit sign bit doesn't get mapped to the sample_t sign bit.
		return static_cast<sample_t>(static_cast<uint16_t>(sample));
	else
		return static_cast<sample_t>(sample);
}

template <std::integral T>
bool moip_sample_ring_buffer::push(const T *sample, uint32_t sample_count, moip_sample_ring_buffer::MoipSampleOwner samples_from)
{
	if (sample_count == 0)
		return true;

	const bool machine_samples_in = !(m_bypass_resampler && m_sip_sample_rate == m_machine_sample_rate) && (samples_from == moip_sample_ring_buffer::MoipSampleOwner::SAMPLE_OWNER_MACHINE);

	uint32_t head = m_head.load(std::memory_order_acquire);
	uint32_t tail = m_tail.load(std::memory_order_relaxed);
	uint32_t capacity = m_mask + 1;

	const uint32_t free_samples = capacity - (tail - head);
	const double phase_increment = (m_sip_sample_rate / m_machine_sample_rate) * (1.0 + m_ppm_offset);
	if (machine_samples_in)
	{
		double phase = m_push_frac_phase;
		uint32_t output_count = 0;
		for (uint32_t i = 0; i < sample_count; i++)
		{
			phase += phase_increment;
			while (phase >= 1.0)
			{
				if (output_count == free_samples)
					return false;

				output_count++;
				phase -= 1.0;
			}
		}
	}
	else if (sample_count > free_samples)
		return false;

	uint32_t samples_pushed = 0;

	if (machine_samples_in)
	{
		const double gain = m_gain_adjust;

		for (uint32_t i = 0; i < sample_count; ++i)
		{
			moip_sample_ring_buffer::history_push(m_push_hist, m_push_head, static_cast<double>(moip_sample_ring_buffer::get_moip_sample(sample[i])));

			m_push_frac_phase += phase_increment;

			while (m_push_frac_phase >= 1.0)
			{
				double sub_frac = (m_push_frac_phase - 1.0) / phase_increment;
				if (sub_frac > 1.0)
					sub_frac = 1.0;

				sample_t fir_sample = moip_sample_ring_buffer::fir(m_machine_coeffs, m_push_hist, m_push_head, sub_frac, gain);

				// Store the decimated (downsampled) samples to be consumed by pull() from the 8kHz MoIP transport side
				m_buf.get()[(tail + samples_pushed++) & m_mask] = fir_sample;

				m_push_frac_phase -= 1.0;
			}
		}
	}
	else
	{
		// Store MoIP transport PCM samples to be consumed by pull() from the machine
		// If the pushed sample buffer type is the same as the ring buffer then we can use std::memcpy
		if constexpr (std::is_same_v<T, sample_t>)
		{
			uint32_t capacity = m_mask + 1;
			uint32_t write_idx = tail & m_mask;
			uint32_t first_chunk = std::min(sample_count, capacity - write_idx);

			std::memcpy(m_buf.get() + write_idx, sample, first_chunk * sizeof(sample_t));

			// Ring buffer wrap around
			if (first_chunk < sample_count)
				std::memcpy(m_buf.get(), sample + first_chunk, (sample_count - first_chunk) * sizeof(sample_t));

			samples_pushed = sample_count;
		}
		else
		{
			for (uint32_t i = 0; i < sample_count; ++i)
				m_buf.get()[(tail + samples_pushed++) & m_mask] = moip_sample_ring_buffer::get_moip_sample<T>(sample[i]);
		}
	}

	m_tail.store(tail + samples_pushed, std::memory_order_release);

	return true;
}

template <std::integral T>
uint32_t moip_sample_ring_buffer::pull(T *sample, uint32_t sample_count, moip_sample_ring_buffer::MoipSampleOwner samples_for, bool apply_jitter_delay)
{
	if (sample_count == 0)
		return 0;

	const bool machine_samples_out = (samples_for == moip_sample_ring_buffer::MoipSampleOwner::SAMPLE_OWNER_MACHINE);

	uint32_t samples_pulled = 0;

	if (apply_jitter_delay && m_within_delay)
	{
		const uint32_t target = machine_samples_out ? m_delay_samples : std::max(m_delay_samples, sample_count);
		if (avail() >= target)
		{
			m_within_delay = false;
		}
		else
		{
			std::fill(sample, sample + sample_count, get_machine_sample<T>(0));

			return 0;
		}
	}

	if(machine_samples_out)
	{
		if (m_bypass_resampler && m_sip_sample_rate == m_machine_sample_rate)
		{
			// Apply jitter delay only then exit when we're bypassing the resampler code.
			const uint32_t samples_pulled = moip_sample_ring_buffer::pull(sample, sample_count, SAMPLE_OWNER_SIP, false);

			if (apply_jitter_delay && samples_pulled < sample_count)
				m_within_delay = (m_delay_samples > 0);

			return samples_pulled;
		}
		else
		{
			double phase_increment = (m_sip_sample_rate / m_machine_sample_rate) * (1.0 + m_ppm_offset);
			double gain = m_gain_adjust;

			if (apply_jitter_delay && m_within_delay)
			{
				if (avail() >= m_delay_samples)
					m_within_delay = false;
				else
					return 0;
			}

			for (uint32_t i = 0; i < sample_count; ++i)
			{
				while (m_pull_frac_phase >= 1.0)
				{
					// Pull the PCM sample from the MoIP transport to be interpolated (upsampled) for the machine
					sample_t moip_sample;
					if (moip_sample_ring_buffer::pull(&moip_sample, 1) == 1)
					{
						moip_sample_ring_buffer::history_push(m_pull_hist, m_pull_head, static_cast<double>(moip_sample));
					}
					else
					{
						if (apply_jitter_delay)
							m_within_delay = true;

						// Repeating the last send sample rather than using 0
						// This allows the amplitude, frequency and phase change calculation to appear flat.

						T machine_sample = moip_sample_ring_buffer::get_machine_sample<T>(m_last_sample);

						std::fill(sample + i, sample + sample_count, machine_sample);

						return samples_pulled;
					}
					m_pull_frac_phase -= 1.0;
				}

				double sub_frac = m_pull_frac_phase;
				if (sub_frac > 1.0)
					sub_frac = 1.0;

				sample_t fir_sample = moip_sample_ring_buffer::fir(m_moip_coeffs[m_sip_rate_idx], m_pull_hist, m_pull_head, sub_frac, gain);

				sample[i] = moip_sample_ring_buffer::get_machine_sample<T>(fir_sample);

				m_last_sample = fir_sample;

				m_pull_frac_phase += phase_increment;

				samples_pulled++;
			}
		}
	}
	else
	{
		// Transfer decimated samples from the machine to be forwarded over the MoIP transport
		// If the pull sample buffer type is the same as the ring buffer then we can use std::memcpy
		if constexpr (std::is_same_v<T, sample_t>)
		{
			samples_pulled = moip_sample_ring_buffer::pull(sample, sample_count);
		}
		else
		{
			for (uint32_t i = 0; i < sample_count; ++i)
			{
				sample_t moip_sample;
				if (moip_sample_ring_buffer::pull(&moip_sample, 1) == 1)
					sample[i] = moip_sample_ring_buffer::get_machine_sample<T>(moip_sample);
				else
					break;

				samples_pulled++;
			}
		}

		if (samples_pulled < sample_count)
		{
			uint32_t last_sample_idx;
			if(samples_pulled > 0)
			{
				last_sample_idx = (samples_pulled - 1);
				m_last_sample = sample[last_sample_idx];
			}
			else
			{
				last_sample_idx = 0;
				m_last_sample = 0;
			}

			T machine_sample = moip_sample_ring_buffer::get_machine_sample<T>(m_last_sample);

			std::fill(sample + last_sample_idx, sample + sample_count, machine_sample);
		}
	}

	return samples_pulled;
}

const uint32_t moip_sample_ring_buffer::avail()
{
	uint32_t head = m_head.load(std::memory_order_relaxed);
	uint32_t tail = m_tail.load(std::memory_order_acquire);
	return (tail - head);
}

void moip_sample_ring_buffer::set_sip_sample_rate(double rate)
{
	auto rate_ptr = std::find(std::begin(MOIP_SAMPLE_RATES), std::end(MOIP_SAMPLE_RATES), rate);

	assert(rate_ptr != std::end(MOIP_SAMPLE_RATES));

	moip_sample_ring_buffer::set_sip_sample_rate(static_cast<uint32_t>(std::distance(std::begin(MOIP_SAMPLE_RATES), rate_ptr)));
}

void moip_sample_ring_buffer::set_sip_sample_rate(uint32_t rate_idx)
{
	assert(rate_idx < std::size(MOIP_SAMPLE_RATES));

	m_sip_rate_idx = rate_idx;
	m_sip_sample_rate = MOIP_SAMPLE_RATES[rate_idx];
}

void moip_sample_ring_buffer::set_gain_adjust(double gain)
{
	m_gain_adjust = gain;
}

void moip_sample_ring_buffer::set_delay_samples(uint32_t delay_samples)
{
	m_delay_samples = delay_samples;
	m_within_delay = (m_delay_samples > 0);
}

const bool moip_sample_ring_buffer::within_pull_delay()
{
	return m_within_delay;
}

void moip_sample_ring_buffer::clear()
{
	m_head.store(0, std::memory_order_relaxed);
	m_tail.store(0, std::memory_order_relaxed);

	m_within_delay = (m_delay_samples > 0);

	m_last_sample = 0;
	std::fill(m_push_hist.begin(), m_push_hist.end(), 0);
	m_push_head = 0;
	m_push_frac_phase = 0.0;
	std::fill(m_pull_hist.begin(), m_pull_hist.end(), 0);
	m_pull_head = 0;
	m_pull_frac_phase = 0.0;
}

uint32_t moip_sample_ring_buffer::pull(sample_t *sample, uint32_t sample_count)
{
	uint32_t head = m_head.load(std::memory_order_relaxed);
	uint32_t tail = m_tail.load(std::memory_order_acquire);
	uint32_t avail_samples = tail - head;
	uint32_t samples_pulled = std::min(sample_count, avail_samples);

	if (samples_pulled > 0)
	{
		uint32_t capacity = m_mask + 1;
		uint32_t idx = head & m_mask;
		uint32_t first_chunk = std::min(capacity - idx, samples_pulled);

		std::memcpy(sample, m_buf.get() + idx, first_chunk * sizeof(sample_t));
		if (first_chunk < samples_pulled)
			std::memcpy(sample + first_chunk, m_buf.get(), (samples_pulled - first_chunk) * sizeof(sample_t));

		m_head.store(head + samples_pulled, std::memory_order_release);
	}

	return samples_pulled;
}

// ----------------------------------------------------------------------------
// SIP client state
// ----------------------------------------------------------------------------

sip_client::sip_client(const char *sip_uri, double machine_sample_rate, sip_client::SipLoggingLevel logging_level) :
	m_machine_sample_rate(machine_sample_rate),
	m_logging_level(logging_level)
{
	if (auto config = sip_client::parse_uri(sip_uri))
	{
		m_config = *config;
		m_valid_uri = true;
	}
	else
	{
		m_valid_uri = false;
	}
}

const bool sip_client::validate_uri(const char *sip_uri)
{
	return sip_client::parse_uri(sip_uri).has_value();
}

std::optional<sip_client::SIPURIConfig> sip_client::parse_uri(const char *sip_uri)
{
	if (!sip_uri)
		return std::nullopt;

	constexpr std::string_view PREFIX = "sip:";

	std::string_view uri(sip_uri);

	if (!uri.starts_with(PREFIX))
		return std::nullopt;

	uri.remove_prefix(PREFIX.size());

	const size_t at = uri.rfind('@');
	if (at == std::string_view::npos)
		return std::nullopt;

	const std::string_view userinfo = uri.substr(0, at);
	std::string_view hostport_params = uri.substr(at + 1);

	SIPURIConfig config;

	const size_t colon = userinfo.find(':');
	if (colon == std::string_view::npos)
	{
		config.user = userinfo;
	}
	else
	{
		config.user = userinfo.substr(0, colon);
		config.password = userinfo.substr(colon + 1);
	}

	if (config.user.empty())
		return std::nullopt;

	std::string_view params_str;
	const size_t semi = hostport_params.find(';');
	if (semi != std::string_view::npos)
	{
		params_str = hostport_params.substr(semi + 1);
		hostport_params = hostport_params.substr(0, semi);
	}

	if (!params_str.empty())
	{
		// Convert string to boolean.
		auto stob = [](const std::string& str) -> bool {
			std::string lower = str;
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

			// Default is true unless one of these values is found.
			return !(lower == "false" || lower == "0" || lower == "no");
		};

		size_t pos = 0;
		while (pos < params_str.length())
		{
			size_t next_semi = params_str.find(';', pos);
			std::string_view kv = (next_semi == std::string_view::npos)
				? params_str.substr(pos)
				: params_str.substr(pos, next_semi - pos);

			size_t eq = kv.find('=');
			if (eq != std::string_view::npos)
			{
				std::string_view key = kv.substr(0, eq);
				std::string_view val = kv.substr(eq + 1);
				std::string val_str(val); // For std::stod / std::stoul compatibility

				if (key == "transport")
				{
					config.transport = val_str;
				}
				else if (key == "jitter_ms")
				{
					config.jitter_ms = static_cast<uint32_t>(std::stoul(val_str));
				}
				else if (key == "resampler_bypass" || key == "can_bypass")
				{
					config.bypass_resampler = stob(val_str);
				}
				else if (key == "gain")
				{
					config.in_gain = std::stod(val_str);
					config.out_gain = std::stod(val_str);
				}
				else if (key == "in_gain")
				{
					config.in_gain = std::stod(val_str);
				}
				else if (key == "out_gain")
				{
					config.out_gain = std::stod(val_str);
				}
				else if (key == "ppm_offset")
				{
					config.in_ppm_offset = std::stod(val_str);
					config.out_ppm_offset = std::stod(val_str);
				}
				else if (key == "in_ppm_offset")
				{
					config.in_ppm_offset = std::stod(val_str);
				}
				else if (key == "out_ppm_offset")
				{
					config.out_ppm_offset = std::stod(val_str);
				}
				else if (key == "delay_samples")
				{
					uint32_t d = static_cast<uint32_t>(std::stoul(val_str));
					config.in_delay_samples = d;
					config.out_delay_samples = d;
				}
				else if (key == "in_delay_samples")
				{
					config.in_delay_samples = static_cast<uint32_t>(std::stoul(val_str));
				}
				else if (key == "out_delay_samples")
				{
					config.out_delay_samples = static_cast<uint32_t>(std::stoul(val_str));
				}
				else if (key == "tap_count")
				{
					uint8_t t = static_cast<uint8_t>(std::stoul(val_str));
					config.in_tap_count = t;
					config.out_tap_count = t;
				}
				else if (key == "in_tap_count")
				{
					config.in_tap_count = static_cast<uint8_t>(std::stoul(val_str));
				}
				else if (key == "out_tap_count")
				{
					config.out_tap_count = static_cast<uint8_t>(std::stoul(val_str));
				}
				else if (key == "cutoff_freqlo")
				{
					config.in_cutoff_freqlo = std::stod(val_str);
					config.out_cutoff_freqlo = std::stod(val_str);
				}
				else if (key == "in_cutoff_freqlo")
				{
					config.in_cutoff_freqlo = std::stod(val_str);
				}
				else if (key == "out_cutoff_freqlo")
				{
					config.out_cutoff_freqlo = std::stod(val_str);
				}
				else if (key == "cutoff_freqhi")
				{
					config.in_cutoff_freqhi = std::stod(val_str);
					config.out_cutoff_freqhi = std::stod(val_str);
				}
				else if (key == "in_cutoff_freqhi")
				{
					config.in_cutoff_freqhi = std::stod(val_str);
				}
				else if (key == "out_cutoff_freqhi")
				{
					config.out_cutoff_freqhi = std::stod(val_str);
				}
				else if (key == "slattdb")
				{
					config.in_slattdb = std::stod(val_str);
					config.out_slattdb = std::stod(val_str);
				}
				else if (key == "in_slattdb")
				{
					config.in_slattdb = std::stod(val_str);
				}
				else if (key == "out_slattdb")
				{
					config.out_slattdb = std::stod(val_str);
				}
				else if (key == "disable_8k")
				{
					config.disable_8k = stob(val_str);
				}
				else if (key == "disable_16k")
				{
					config.disable_16k = stob(val_str);
				}
				else if (key == "disable_ulaw")
				{
					config.disable_ulaw = stob(val_str);
				}
				else if (key == "disable_l16")
				{
					config.disable_l16 = stob(val_str);
				}
			}

			if (next_semi == std::string_view::npos)
				break;

			pos = next_semi + 1;
		}
	}

	const size_t pcolon = hostport_params.rfind(':');
	if (pcolon == std::string_view::npos)
	{
		config.host = hostport_params;
	}
	else
	{
		config.host = hostport_params.substr(0, pcolon);

		std::string port_str(hostport_params.substr(pcolon + 1));
		const unsigned long p = std::strtoul(port_str.c_str(), nullptr, 10);
		config.port = (p != 0 && p <= 65535) ? uint16_t(p) : sip_client::SIP_DEFAULT_PORT;
	}

	if (config.host.empty())
		return std::nullopt;

	return config;
}

#if MOIPBRIDGE_HAVE_PJSUA2
bool sip_client::start_sip(std::string &error)
{
	if (!m_valid_uri)
	{
		error = "SIP URI not parsed.";
		return false;
	}

	uint32_t sample_rate = static_cast<uint32_t>(MOIP_DEFAULT_SAMPLE_RATE);
	uint32_t channel_count = MOIP_DEFAULT_CHANNEL_COUNT;

	try
	{
		//
		// SIP endpoint setup
		//

		m_endp = std::make_unique<pj::Endpoint>();
		m_endp->libCreate();

		// ms of audio in one packet.
		uint32_t ptime_ms = sip_client::SIP_PTIME_MS;

		pj::EpConfig endpoint_config;
		endpoint_config.uaConfig.userAgent = sip_client::SIP_USER_AGENT;
		endpoint_config.medConfig.clockRate = sample_rate;
		endpoint_config.medConfig.sndClockRate = sample_rate;
		endpoint_config.medConfig.channelCount = channel_count;
		// Amount of time a packet of samples contains
		// lower=lower latency better packet loss protection under low-loss conditions but more data to send
		// Setting this to 10ms because it works with pjmedia's system, bandwidth is negligble for most users
		// and it provides good packet loss protection (less sample time lost per datagram/packet)
		endpoint_config.medConfig.audioFramePtime = ptime_ms;
		// Voice activity detection is disabled.
		// There's no voice on this line and this will mess with data transmission
		endpoint_config.medConfig.noVad = true;
		// Echo canceller tail length in ms
		// All echo cancelation is disabled. There shouldn't be any sample alteration, leave that to the modem.
		endpoint_config.medConfig.ecTailLen = 0;
		// There appears to be no way to disable pjmedia's jitter buffer. =0 sets it to adaptive which is bad for modems that assume a constant delay.
		// So this allows it to be configured with jitter_ms= in incrments of ptime_ms and have it be fixed, not adaptive.
		const unsigned prefetch_ms = m_config.jitter_ms ? ((m_config.jitter_ms + (ptime_ms - 1)) / ptime_ms) * ptime_ms : 0;
		// Jitter buffer prefetch delay in ms
		endpoint_config.medConfig.jbInit = prefetch_ms;
		// Jitter buffer minimum prefetch delay in ms
		endpoint_config.medConfig.jbMinPre = prefetch_ms;
		// Jitter buffer maximum prefetch delay in ms
		endpoint_config.medConfig.jbMaxPre = prefetch_ms;
		// Jitter buffer maximum delay in ms
		endpoint_config.medConfig.jbMax = 0;
		// Jitter buffer discard algorithm. PJMEDIA_JB_DISCARD_NONE sets it to not discard unless the jitter buffer is full.
		endpoint_config.medConfig.jbDiscardAlgo = PJMEDIA_JB_DISCARD_NONE;
		// Thread count
		endpoint_config.medConfig.threadCnt = 1;
		// Logging (on for -verbose or -debug)
		switch (m_logging_level)
		{
			case SipLoggingLevel::SIP_LOG_VERBOSE:
				endpoint_config.logConfig.level = 3;
				endpoint_config.logConfig.consoleLevel = 3;
				break;

			case SipLoggingLevel::SIP_LOG_DEBUG:
				endpoint_config.logConfig.level = 4;
				endpoint_config.logConfig.consoleLevel = 4;
				break;

			case SipLoggingLevel::SIP_LOG_NONE:
			default:
				endpoint_config.logConfig.level = 0;
				endpoint_config.logConfig.consoleLevel = 0;
				break;
		}
		m_endp->libInit(endpoint_config);

		pj::TransportConfig transport_config;
		transport_config.port = 0;
		m_endp->transportCreate(PJSIP_TRANSPORT_UDP, transport_config);

		m_endp->libStart();

		m_endp->audDevManager().setNoDev();

		// Setup codecs. The codec choices are optimized for MoIP
		const pj::CodecInfoVector2 codecs = m_endp->codecEnum2();
		for (auto &ci : codecs)
		{
			uint32_t priority = 0;

			for (double search_sample_rate : MOIP_SAMPLE_RATES)
			{
				if (search_sample_rate == 8000.0 && m_config.disable_8k)
					continue;
				else if (search_sample_rate == 16000.0 && m_config.disable_16k)
					continue;

				// L16 raw uncompressed mono PCM at the selected sample rate
				if (ci.codecId.rfind(std::format("L16/{}/{}", static_cast<uint32_t>(search_sample_rate), channel_count)) == 0)
				{
					if (!m_config.disable_l16)
						priority = (search_sample_rate == 16000.0) ? 251 : 250;

					// Found codec so exiting sample rate search.
					break;
				}
				// G.711 u-law (normally @ 8kHz). Predictable logrithmic compression (13 bits to 8 bits)
				// Some quantization errors but they aren't as bad as true compression codecs
				// 64kbps
				else if (ci.codecId.rfind(std::format("PCMU/{}/{}", static_cast<uint32_t>(search_sample_rate), channel_count)) == 0)
				{
					// 16kHz isn't supported but specifying the priority here in case I try to add it.
					if (!m_config.disable_ulaw)
						priority = (search_sample_rate == 16000.0) ? 241 : 240;

					// Found codec so exiting sample rate search.
					break;
				}

			}

			m_endp->codecSetPriority(ci.codecId, priority);
		}

		//
		// Media handler setup
		//

		// Setup sample ring buffer

		// Buffer from the MAME machine to the remote modem over SIP for either side (MAME or SIP)
		m_out_buf = std::make_unique<moip_sample_ring_buffer>(
			// Ring buffer size, must be enough to store one frame full of samples
			sip_client::SIP_RING_BUFFER_SZ,
			// The MAME machine sample rate (16kHz for WebTV softmodems)
			m_machine_sample_rate,
			// Bypass the resampler entierly. This can only work if both the SIP and machine sample rate are the same.
			m_config.bypass_resampler,
			// The gain adjustment
			m_config.out_gain,
			// Frequency offset adjustment in ppm
			m_config.out_ppm_offset,
			// Amount of samples to prime the jitter buffer with. This is a constant jitter correction, nothing is adaptive.
			m_config.out_delay_samples,
			// Polyphase FIR filter taps per phase table values (higher is better filtering, lower is faster evaluation and less memory)
			m_config.out_tap_count,
			// The FIR polyphase FIR filter low cutoff freqency (everything below this is filtered)
			m_config.out_cutoff_freqlo,
			// The FIR polyphase FIR filter high cutoff freqency (everything above this is filtered)
			m_config.out_cutoff_freqhi,
			// Sidelobe attenuation in dB for the Kaiser window, higher is better frequency response
			m_config.out_slattdb
		);
		// Buffer from the remote modem over SIP to the MAME machine
		m_in_buf = std::make_unique<moip_sample_ring_buffer>(
			// Ring buffer size, must be enough to store one frame full of samples for either side (MAME or SIP)
			sip_client::SIP_RING_BUFFER_SZ,
			// The MAME machine sample rate (16kHz for WebTV softmodems)
			m_machine_sample_rate,
			// Bypass the resampler entierly. This can only work if both the SIP and machine sample rate are the same.
			m_config.bypass_resampler,
			// The gain adjustment
			m_config.in_gain,
			// Frequency offset adjustment in ppm
			m_config.in_ppm_offset,
			// Amoutn of samples to prime the jitter buffer with. This is a constant jitter correction, nothing is adaptive.
			m_config.in_delay_samples,
			// Polyphase FIR filter taps per phase table values (higher is better filtering, lower is faster evaluation and less memory)
			m_config.in_tap_count,
			// The FIR polyphase FIR filter low cutoff freqency (everything below this is filtered)
			m_config.in_cutoff_freqlo,
			// The FIR polyphase FIR filter high cutoff freqency (everything above this is filtered)
			m_config.in_cutoff_freqhi,
			// Sidelobe attenuation in dB for the Kaiser window, higher is better frequency response
			m_config.in_slattdb
		);

		//
		// Account setup
		//

		m_acct = std::make_unique<sip_account>(*this);

		pj::AccountConfig account_config;

		std::string transport_param = m_config.transport.empty() ? "" : ";transport=" + m_config.transport;

		// URL used to identify this bridge
		account_config.idUri = std::format("sip:{}@{}{}", m_config.user, m_config.host, transport_param);
		// URL used to register the identity
		account_config.regConfig.registrarUri = std::format("sip:{}:{}{}", m_config.host, m_config.port, transport_param);

		account_config.sipConfig.authCreds.push_back(pj::AuthCredInfo("digest", "*", m_config.user, 0, m_config.password));

		m_acct->create(account_config);

		return true;
	}
	catch (pj::Error &e)
	{
		error = e.info();

		sip_client::shutdown();

		return false;
	}
}

void sip_client::dial(const std::string &number)
{
	if (!m_valid_uri || !m_acct)
		return;

	sip_client::set_call_dropped(false);

	try
	{
		std::string transport_param = m_config.transport.empty() ? "" : ";transport=" + m_config.transport;

		std::string dest_uri = std::format("sip:{}@{}:{}{}", number, m_config.host, m_config.port, transport_param);

		m_call = std::make_unique<sip_call>(*m_acct, *this);

		pj::CallOpParam prm(true);
		m_call->makeCall(dest_uri, prm);

		osd_printf_verbose("MoIP bridge: placing call %s\n", dest_uri.c_str());
	}
	catch (pj::Error &e)
	{
		osd_printf_verbose("MoIP bridge: makeCall err: %s\n", e.info().c_str());

		sip_client::set_call_dropped();

		if (m_call)
			m_call.reset();
	}
}

void sip_client::hangup()
{
	if (!m_valid_uri || !m_call || !sip_client::call_active())
		return;
	
	try
	{
		m_call->kill_audio();

		pj::CallOpParam p;
		m_call->hangup(p);
	}
	catch (...)
	{

	}

	m_call.reset();
}

const bool sip_client::call_active()
{
	return m_call && m_call->active();
}

const bool sip_client::call_dropped()
{
	return m_call && m_call_droped;
}

void sip_client::set_call_dropped(bool enabled)
{
	m_call_droped = enabled;
}

void sip_client::shutdown()
{
	if(!m_valid_uri)
		return;

	if (!m_endp)
	{
		m_call.reset();
		return;
	}

	sip_client::hangup();

	try
	{
		m_endp->libDestroy();
	}
	catch (...)
	{

	}

	m_acct.reset();
	m_endp.reset();
}
#endif // MOIPBRIDGE_HAVE_PJSUA2

template <std::integral T>
uint32_t sip_client::play_fast_busy(T* sample, uint32_t sample_count)
{
	constexpr double f[2] = { 480.0, 620.0 };

	constexpr double tao = 2.0 * M_PI;

	constexpr double max_val = static_cast<double>(std::numeric_limits<int16_t>::max()) * 0.5;

	double on_samples = (m_machine_sample_rate * 0.25);
	double off_samples = (m_machine_sample_rate * 0.25);

	double full_cycle_samples = on_samples + off_samples;

	for (uint32_t i = 0; i < sample_count; ++i)
	{
		double cycle_position = std::fmod(m_tone_position, full_cycle_samples);

		T sample_value = 0;

		if (cycle_position < on_samples) // within on_samples?
		{
			double t = m_tone_position / m_machine_sample_rate;

			double sample_val = (std::sin(tao * f[0] * t) + std::sin(tao * f[1] * t)) * 0.5;

			sample_value = static_cast<T>(sample_val * max_val);
		}

		sample[i] = sample_value;

		m_tone_position++;
	}

	return sample_count;
}

template <std::integral T>
void sip_client::push(const T* sample, uint32_t sample_count)
{
	std::shared_lock guard(m_mutex);

	if(!m_valid_uri)
		return;

	m_out_buf->push(sample, sample_count, moip_sample_ring_buffer::MoipSampleOwner::SAMPLE_OWNER_MACHINE);
}

template <std::integral T>
uint32_t sip_client::pull(T* sample, uint32_t sample_count)
{
	std::shared_lock guard(m_mutex);

	if(!m_valid_uri)
		return 0;

	uint32_t samples_pulled;
	if(!sip_client::call_dropped())
		samples_pulled = m_in_buf->pull(sample, sample_count, moip_sample_ring_buffer::MoipSampleOwner::SAMPLE_OWNER_MACHINE, (m_config.in_delay_samples > 0));
	else
		samples_pulled = sip_client::play_fast_busy(sample, sample_count);

	return samples_pulled;
}

void sip_client::reset_audio()
{
	std::unique_lock guard(m_mutex);

	if(!m_valid_uri)
		return;

	sip_client::set_call_dropped(false);

	m_tone_position = 0;

	m_out_buf->clear();
	m_in_buf->clear();
}

void sip_client::sip_audio_init(double rate)
{
	std::unique_lock guard(m_mutex);

	m_out_buf->set_sip_sample_rate(rate);
	m_out_buf->clear();

	m_in_buf->set_sip_sample_rate(rate);
	m_in_buf->clear();
}

void sip_client::sip_audio_pull(int16_t *sample, uint32_t sample_count)
{
	std::shared_lock guard(m_mutex);

	m_out_buf->pull(sample, sample_count, moip_sample_ring_buffer::SAMPLE_OWNER_SIP, true);
}

void sip_client::sip_audio_push(const int16_t *sample, uint32_t sample_count)
{
	std::shared_lock guard(m_mutex);

	m_in_buf->push(sample, sample_count, moip_sample_ring_buffer::SAMPLE_OWNER_SIP);
}

#if MOIPBRIDGE_HAVE_PJSUA2

// ----------------------------------------------------------------------------
// SIP account state
// ----------------------------------------------------------------------------

sip_account::sip_account(sip_client &sip)
	: m_sip(sip)
{

}

void sip_account::onRegState(pj::OnRegStateParam &prm)
{
	osd_printf_verbose("MoIP bridge: register: code=%d %s\n", prm.code, prm.reason.c_str());
}

void sip_account::onIncomingCall(pj::OnIncomingCallParam &)
{

}

// ----------------------------------------------------------------------------
// SIP media bridge
// ----------------------------------------------------------------------------

sip_audio_bridge::~sip_audio_bridge()
{
	sip_audio_bridge::leave();
}

void sip_audio_bridge::join(pjmedia_port *port, rx_cb_fn rx_cb, tx_cb_fn tx_cb)
{
	std::lock_guard guard(m_mutex);

	sip_audio_bridge::leave();

	if (!port)
		throw std::runtime_error("PJMEDIA port is empty!");

	m_port = port;

	const uint32_t sample_rate = PJMEDIA_PIA_SRATE(&m_port->info);
	const uint32_t channel_count = PJMEDIA_PIA_CCNT(&m_port->info);
	const uint32_t samples_per_frame = PJMEDIA_PIA_SPF(&m_port->info);
	const uint32_t bits_per_sample = PJMEDIA_PIA_BITS(&m_port->info);

	if (bits_per_sample != (sizeof(int16_t) * 8))
		throw std::runtime_error("PJMEDIA requests a bad bits per sample value!");

	if (channel_count != MOIP_DEFAULT_CHANNEL_COUNT)
		throw std::runtime_error("PJMEDIA requests a bad channel count value!");

	if (samples_per_frame == 0)
		throw std::runtime_error("PJMEDIA requests a an empty samples per frame value!");

	if (samples_per_frame == 0)
		throw std::runtime_error("PJMEDIA requests a an empty samples per frame value!");

	auto rate_ptr = std::find(std::begin(MOIP_SAMPLE_RATES), std::end(MOIP_SAMPLE_RATES), static_cast<double>(sample_rate));
	if (rate_ptr == std::end(MOIP_SAMPLE_RATES))
		throw std::runtime_error("PJMEDIA requests a bad sample rate value!");

	m_pool = pjsua_pool_create("sip_audio_bridge", sip_audio_bridge::SIP_AUDIO_POOL_SIZE, sip_audio_bridge::SIP_AUDIO_POOL_INCR);
	if (!m_pool)
	{
		m_port = nullptr;
		throw std::bad_alloc();
	}

	m_rx_sample.assign(samples_per_frame, 0);
	m_rx_cb = std::move(rx_cb);

	m_tx_sample.assign(samples_per_frame, 0);
	m_tx_cb = std::move(tx_cb);

	m_error.store(PJ_SUCCESS);

	const auto status = pjmedia_clock_create(
		m_pool,
		sample_rate,
		channel_count,
		samples_per_frame,
		PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
		&tick,
		this, // user_data
		&m_clock
	);

	if (status != PJ_SUCCESS)
	{
		sip_audio_bridge::leave();

		throw std::runtime_error("Failed to create PJMEDIA clock!");
	}
}

void sip_audio_bridge::leave()
{
	std::lock_guard guard(m_mutex);

	sip_audio_bridge::stop();

	m_port = nullptr;

	if (m_clock)
	{
		pjmedia_clock_destroy(m_clock);
		m_clock = nullptr;
	}

	if (m_pool)
	{
		pj_pool_release(m_pool);
		m_pool = nullptr;
	}
}

void sip_audio_bridge::start()
{
	std::lock_guard guard(m_mutex);

	if (!m_clock || m_started)
		return;

	if (pjmedia_clock_start(m_clock) != PJ_SUCCESS)
		throw std::runtime_error("Cannot start MoIP media clock!");

	m_started = true;
}

void sip_audio_bridge::stop()
{
	std::lock_guard guard(m_mutex);

	if (m_clock && m_started)
		pjmedia_clock_stop(m_clock);

	m_started = false;
}

void sip_audio_bridge::process_audio(pj_timestamp timestamp) noexcept
{
	// RX

	pjmedia_frame rx_frame {};
	std::fill(m_rx_sample.begin(), m_rx_sample.end(), 0);
	rx_frame.buf = m_rx_sample.data();
	rx_frame.size = m_rx_sample.size() * sizeof(int16_t);
	rx_frame.timestamp = timestamp;

	const auto rx_status = pjmedia_port_get_frame(m_port, &rx_frame);
	if (rx_status != PJ_SUCCESS)
		m_error.store(rx_status);

	if (rx_status != PJ_SUCCESS || rx_frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		std::fill(m_rx_sample.begin(), m_rx_sample.end(), 0);
	else if (rx_frame.size < (m_rx_sample.size() * sizeof(int16_t)))
		std::fill(m_rx_sample.begin() + rx_frame.size / sizeof(int16_t), m_rx_sample.end(), 0);

	try
	{
		m_rx_cb(m_rx_sample.data(), (uint32_t)m_rx_sample.size());
	}
	catch (...)
	{
		m_error.store(PJ_EUNKNOWN);
	}

	// TX

	std::fill(m_tx_sample.begin(), m_tx_sample.end(), 0);
	try
	{
		m_tx_cb(m_tx_sample.data(), (uint32_t)m_tx_sample.size());
	}
	catch (...)
	{
		m_error.store(PJ_EUNKNOWN);
		std::fill(m_tx_sample.begin(), m_tx_sample.end(), 0);
	}

	pjmedia_frame tx_frame {};
	tx_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	tx_frame.buf = m_tx_sample.data();
	tx_frame.size = m_tx_sample.size() * sizeof(int16_t);
	tx_frame.timestamp = timestamp;

	const auto tx_status = pjmedia_port_put_frame(m_port, &tx_frame);
	if (tx_status != PJ_SUCCESS)
		m_error.store(tx_status);
}

// ----------------------------------------------------------------------------
// SIP media handling
// ----------------------------------------------------------------------------

void sip_call::kill_audio()
{
	m_audio.leave();
}

void sip_call::onStreamCreated(pj::OnStreamCreatedParam &prm)
{
	try
	{
		kill_audio();

		auto *port = static_cast<pjmedia_port *>(prm.pPort);
		m_sip.sip_audio_init(PJMEDIA_PIA_SRATE(&port->info));

		m_audio.join(port,
			[this](const int16_t *sample, unsigned sample_count)
			{
				m_sip.sip_audio_push(sample, sample_count);
			},
			[this](int16_t *sample, unsigned sample_count)
			{
				m_sip.sip_audio_pull(sample, sample_count);
			}
		);

		m_stream = prm.stream;
	}
	catch (const std::exception &e)
	{
		m_sip.set_call_dropped();
		osd_printf_verbose("MoIP bridge: error with stream setup: %s\n", e.what());
	}
}

void sip_call::onStreamDestroyed(pj::OnStreamDestroyedParam &prm)
{
	if (prm.stream == m_stream)
	{
		kill_audio();

		m_stream = nullptr;
	}
}


// ----------------------------------------------------------------------------
// SIP call state
// ----------------------------------------------------------------------------

sip_call::sip_call(pj::Account &acct, sip_client &sip)
	: pj::Call(acct),
	m_sip(sip)
{

}

sip_call::~sip_call()
{
	sip_call::kill_audio();
}

void sip_call::onCallState(pj::OnCallStateParam &)
{
	pj::CallInfo ci = getInfo();

	osd_printf_verbose("MoIP bridge: call state: #%d %s (%s)\n", ci.state, ci.stateText.c_str(), ci.lastReason.c_str());

	if (ci.state == PJSIP_INV_STATE_CONFIRMED)
	{
		bridge();
	}
	else if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		kill_audio();

		m_active = false;

		m_sip.set_call_dropped();
	}
}

void sip_call::onCallMediaState(pj::OnCallMediaStateParam &)
{
	bridge();
}

void sip_call::bridge()
{
	try
	{
		const auto ci = getInfo();
		for (const auto &media : ci.media)
		{
			if (media.type == PJMEDIA_TYPE_AUDIO && media.status == PJSUA_CALL_MEDIA_ACTIVE)
			{
				m_audio.start();

				osd_printf_verbose("MoIP bridge: media bridged\n");

				return;
			}
		}
		m_audio.stop();
	}
	catch (const std::exception &e)
	{
		m_sip.set_call_dropped();
		osd_printf_verbose("MoIP bridge: bridge err: %s\n", e.what());
	}
	catch (const pj::Error &e)
	{
		m_sip.set_call_dropped();
		osd_printf_verbose("MoIP bridge: bridge err: %s\n", e.info().c_str());
	}
}

#endif // MOIPBRIDGE_HAVE_PJSUA2
