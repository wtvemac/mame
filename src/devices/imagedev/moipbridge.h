// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_IMAGEDEV_MOIPBRIDGE_H
#define MAME_IMAGEDEV_MOIPBRIDGE_H

#pragma once

#if defined(MAME_HAVE_PJSUA2)
#include <pjsua2.hpp>
#define MOIPBRIDGE_HAVE_PJSUA2 1
#else
#define MOIPBRIDGE_HAVE_PJSUA2 0
#endif

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>
#include <cstring>
#include <memory>
#include <vector>
#include <atomic>
#include <new>
#include <bit>
#include <concepts>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <mutex>
#include <optional>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

constexpr double MOIP_SAMPLE_RATE  = 8000.0;
constexpr uint32_t MOIP_CHANNEL_COUNT  = 1;

class sip_account;
class sip_mediaport;
class sip_call;

class moip_sample_ring_buffer
{
public:
	// There's no hard science behind all these numbers
	// I just played around and found these work best given the current FIR interpolator/decimator code
	static constexpr double SRB_DEFAULT_GAIN = 1.08;
	static constexpr double SRB_DEFAULT_PPM_OFFSET = 0.0;
	static constexpr uint32_t SRB_DEFAULT_DELAY_SAMPLES = 720;
	static constexpr uint32_t SRB_POLYNOMIAL_ORDER = 7;
	static constexpr uint8_t SRB_DEFAULT_TAP_COUNT = 64;
	static constexpr double SRB_DEFAULT_CUTOFF_FREQLO = 200.0;
	static constexpr double SRB_DEFAULT_CUTOFF_FREQHI = 3564.0;
	static constexpr double SRB_DEFAULT_SLATTDB = 90.0;

	enum MoipSampleOwner : uint8_t {
		SAMPLE_OWNER_SIP = 0,
		SAMPLE_OWNER_MACHINE = 1
	};

	typedef int16_t sample_t;

	typedef std::vector<std::vector<double>> coeff_table_t;

	moip_sample_ring_buffer(uint32_t capacity, double machine_sample_rate = MOIP_SAMPLE_RATE, double gain_adjust = moip_sample_ring_buffer::SRB_DEFAULT_GAIN, double ppm_offset = SRB_DEFAULT_PPM_OFFSET, uint32_t delay_samples = moip_sample_ring_buffer::SRB_DEFAULT_DELAY_SAMPLES, uint8_t = moip_sample_ring_buffer::SRB_DEFAULT_TAP_COUNT, double cutoff_freqlo = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQLO, double cutoff_freqhi = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQHI, double slattdb = moip_sample_ring_buffer::SRB_DEFAULT_SLATTDB);

	static const double calculate_kaiser_shape(double slattdb);

	template <std::integral T>
	static constexpr T clamp(double v);

	template <std::integral T>
	uint32_t pull(T *sample, uint32_t sample_count, moip_sample_ring_buffer::MoipSampleOwner samples_for, bool apply_jitter_delay = false);

	template <std::integral T>
	bool push(const T *sample, uint32_t sample_count, moip_sample_ring_buffer::MoipSampleOwner samples_from);

	[[nodiscard]] const uint32_t avail();

	void set_gain_adjust(double gain);

	void set_delay_samples(uint32_t delay_samples);

	[[nodiscard]] const bool within_pull_delay();

	void clear();

private:

	const double besselI0(double x);

	void build_coeffs(coeff_table_t &target_table, uint32_t input_rate, bool advancing);

	inline void history_push(std::vector<double> &hist, size_t &head, double sample);

	inline const sample_t fir(const coeff_table_t &target_table, std::vector<double> &hist, size_t &head, double sub_frac, double gain);

	template <std::integral T>
	inline const T get_machine_sample(sample_t sample);

	template <std::integral T>
	inline const sample_t get_moip_sample(T stored_sample);

	uint32_t pull(sample_t* sample, uint32_t sample_count);

	std::unique_ptr<sample_t[]> m_buf;
	std::atomic<uint32_t> m_head{0};
	std::atomic<uint32_t> m_tail{0};
	uint32_t m_mask;

	double m_machine_sample_rate;

	uint8_t m_tap_count;
	double m_cutoff_freqlo;
	double m_cutoff_freqhi;
	double m_beta;
	coeff_table_t m_moip_coeffs;
	coeff_table_t m_machine_coeffs;
	sample_t m_last_sample;
	std::vector<double> m_pull_hist;
	size_t m_pull_head = 0;
	double m_pull_frac_phase;
	std::vector<double> m_push_hist;
	size_t m_push_head = 0;
	double m_push_frac_phase;

	double m_gain_adjust;
	double m_ppm_offset;

	uint32_t m_delay_samples;
	bool m_within_delay;
};

class sip_client
{
	public:

	enum SipLoggingLevel : uint8_t {
		SIP_LOG_NONE    = 0,
		SIP_LOG_VERBOSE = 1,
		SIP_LOG_DEBUG   = 2
	};	

	static constexpr std::string_view SIP_USER_AGENT = "wtvmame-moipbridge";

	static constexpr uint16_t SIP_DEFAULT_PORT  = 5060;

	static constexpr uint32_t SIP_RING_BUFFER_MS  = 2048;
	static constexpr uint32_t SIP_RING_BUFFER_SZ  = sip_client::SIP_RING_BUFFER_MS / (1000.0 / MOIP_SAMPLE_RATE);

	struct SIPURIConfig
	{
		SIPURIConfig() = default;

		std::string host = "";
		uint16_t port = sip_client::SIP_DEFAULT_PORT;
		std::string transport = "";

		std::string user = "";
		std::string password = "";

		double in_gain = moip_sample_ring_buffer::SRB_DEFAULT_GAIN;
		double in_ppm_offset = moip_sample_ring_buffer::SRB_DEFAULT_PPM_OFFSET;
		uint32_t in_delay_samples = moip_sample_ring_buffer::SRB_DEFAULT_DELAY_SAMPLES;
		uint8_t in_tap_count = moip_sample_ring_buffer::SRB_DEFAULT_TAP_COUNT;
		double in_cutoff_freqlo = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQLO;
		double in_cutoff_freqhi = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQHI;
		double in_slattdb = moip_sample_ring_buffer::SRB_DEFAULT_SLATTDB;

		double out_gain = moip_sample_ring_buffer::SRB_DEFAULT_GAIN;
		double out_ppm_offset = moip_sample_ring_buffer::SRB_DEFAULT_PPM_OFFSET;
		uint8_t out_tap_count = moip_sample_ring_buffer::SRB_DEFAULT_TAP_COUNT;
		double out_cutoff_freqlo = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQLO;
		double out_cutoff_freqhi = moip_sample_ring_buffer::SRB_DEFAULT_CUTOFF_FREQHI;
		double out_slattdb = moip_sample_ring_buffer::SRB_DEFAULT_SLATTDB;

		double disable_ulaw = false;
		double disable_l16 = false;
	};

	sip_client(const char *sip_uri, double machine_sample_rate = MOIP_SAMPLE_RATE, sip_client::SipLoggingLevel logging_level = sip_client::SipLoggingLevel::SIP_LOG_NONE);

	static const bool validate_uri(const char *sip_uri);

	template <std::integral T>
	uint32_t pull(T* sample, uint32_t sample_count);

	template <std::integral T>
	void push(const T* sample, uint32_t sample_count);

	void reset_audio();

	#if MOIPBRIDGE_HAVE_PJSUA2
	bool start_sip(std::string &error);
	void dial(const std::string &number);
	void hangup();
	const bool call_active();
	const bool call_dropped();
	void set_call_dropped(bool enabled = true);
	void shutdown();

	std::unique_ptr<sip_mediaport> m_mprt;

	#else
	bool start_sip(std::string &error)
	{
		error = "MAME was built without pjsua2 -moip cannot place calls";
		return false;
	}
	void dial(const std::string &) { }
	void hangup() { }
	const bool call_dropped() { return false; }
	const bool call_active() { return false; }
	void set_call_dropped(bool enabled = true) { }
	void shutdown(){ }
	#endif // MOIPBRIDGE_HAVE_PJSUA2

	std::unique_ptr<moip_sample_ring_buffer> m_out_buf;
	std::unique_ptr<moip_sample_ring_buffer> m_in_buf;
private:

	static std::optional<sip_client::SIPURIConfig> parse_uri(const char *sip_uri);

	template <std::integral T>
	uint32_t play_fast_busy(T* sample, uint32_t sample_count);

	#if MOIPBRIDGE_HAVE_PJSUA2
	std::unique_ptr<pj::Endpoint> m_endp;
	std::unique_ptr<sip_account> m_acct;
	std::unique_ptr<sip_call> m_call;
	#endif // MOIPBRIDGE_HAVE_PJSUA2

	double m_machine_sample_rate;
	sip_client::SipLoggingLevel m_logging_level;

	SIPURIConfig m_config;
	bool m_valid_uri;

	bool m_call_droped;

	double m_tone_position;
};

#if MOIPBRIDGE_HAVE_PJSUA2
class sip_account : public pj::Account
{
public:
	sip_account(sip_client &sip);

	void onRegState(pj::OnRegStateParam &prm) override;
	void onIncomingCall(pj::OnIncomingCallParam &) override;
private:
	sip_client &m_sip;
};

class sip_mediaport : public pj::AudioMediaPort
{
public:
	sip_mediaport(sip_client &sip);

	void onFrameRequested(pj::MediaFrame &frame) override;
	void onFrameReceived(pj::MediaFrame &frame) override;

private:
	sip_client &m_sip;
};

class sip_call : public pj::Call
{
public:
	sip_call(pj::Account &acc, sip_client &sip);

	void onCallState(pj::OnCallStateParam &prm) override;
	void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

	const bool active() { return m_active; }

private:
	void bridge();

	sip_client &m_sip;

	int  m_bridged_slot = -1;

	bool m_active = true;
};
#endif // MOIPBRIDGE_HAVE_PJSUA2

class moipbridge_device : public device_t, public device_image_interface
{
public:
	moipbridge_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0, double sample_rate = MOIP_SAMPLE_RATE);
	virtual ~moipbridge_device();

	virtual std::pair<std::error_condition, std::string> call_load() override;
	virtual void call_unload() override;

	virtual bool core_opens_image_file() const noexcept override { return false; }

	virtual bool is_readable()  const noexcept override { return false; }
	virtual bool is_writeable() const noexcept override { return false; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual const char *file_extensions() const noexcept override { return ""; }
	virtual const char *image_type_name() const noexcept override { return "moipbridge"; }
	virtual const char *image_brief_type_name() const noexcept override { return "moip"; }

	bool is_enabled() const { return exists(); }

	void dial(const std::string &number);
	void hangup();

	const bool call_active();

	// Audio at the bridge boundary is mono signed PCM16.

	uint32_t pull(int16_t* sample, uint32_t sample_count);
	void push(const int16_t* sample, uint32_t sample_count);

	void reset_audio();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

private:
	
	std::unique_ptr<sip_client> m_sip;

	double m_machine_sample_rate;
};

DECLARE_DEVICE_TYPE(MOIPBRIDGE, moipbridge_device)

#endif // MAME_IMAGEDEV_MOIPBRIDGE_H
