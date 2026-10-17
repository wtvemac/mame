// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVSOFTMODEM_RECORDER_H
#define MAME_WEBTV_WTVSOFTMODEM_RECORDER_H

#pragma once

#include <string>
#include <cstdint>
#include <concepts>
#include <cstddef>
#include <fstream>
#include <vector>
#include <queue>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <limits>
#include <chrono>

#include "wtvsoftmodem_dsp.h"

static constexpr uint32_t chk_nameint(const char(&name)[5])
{
	return    (static_cast<uint32_t>(name[3]) << 24)
			| (static_cast<uint32_t>(name[2]) << 16)
			| (static_cast<uint32_t>(name[1]) << 8)
			| (static_cast<uint32_t>(name[0]) << 0);
}

class wtvsoftmodem_recorder
{

public:

	wtvsoftmodem_recorder();

	~wtvsoftmodem_recorder();

	void set_sample_rate(wtvsoftmodem_dsp::frequency_t new_sample_rate);

	void open(const std::string &filetag);

	template <std::integral T>
	void push_left(const T* samples, uint32_t sample_count);
	template <std::integral T>
	void push_right(const T* samples, uint32_t sample_count);

	const uint64_t get_current_sample_index();
	const uint64_t get_pending_sample_count();
	const uint64_t get_written_sample_count();

	void annotate(const std::string& message);

	size_t flush_samples();
	size_t purge_samples();

	size_t write_annotations();

	void close();

private:


	static constexpr uint32_t WAV_RIFF_CHKHDR_BYTECNT = 8;
	static constexpr uint32_t WAV_RIFF_CHKDAT_BYTECNT = 4;
	static constexpr uint32_t WAV_RIFF_CHUNK_ID = chk_nameint("RIFF");
	static constexpr uint32_t WAV_RIFF_START_DATA = chk_nameint("WAVE");
	static constexpr uint32_t WAV_RIFF_BYTECNT = wtvsoftmodem_recorder::WAV_RIFF_CHKHDR_BYTECNT + wtvsoftmodem_recorder::WAV_RIFF_CHKDAT_BYTECNT;

	static constexpr uint32_t WAV_FMT_CHKHDR_BYTECNT = 8;
	static constexpr uint32_t WAV_FMT_CHKDAT_BYTECNT = 16;
	static constexpr uint32_t WAV_FMT_CHUNK_ID = chk_nameint("fmt ");
	static constexpr uint16_t WAV_AUDIO_FORMAT = 1; // PCM integer
	static constexpr uint16_t WAV_AUDIO_CHANNEL_CNT = 2; // Stereo
	static constexpr uint16_t WAV_AUDIO_SAMPLE_BITS = sizeof(int16_t) * 8;
	static constexpr uint32_t WAV_FMT_BYTECNT = wtvsoftmodem_recorder::WAV_FMT_CHKHDR_BYTECNT + wtvsoftmodem_recorder::WAV_FMT_CHKDAT_BYTECNT;

	static constexpr uint32_t WAV_DATA_CHKHDR_BYTECNT = 8;
	static constexpr uint32_t WAV_DATA_CHUNK_ID = chk_nameint("data");

	static constexpr size_t WAV_START_BYTECNT = wtvsoftmodem_recorder::WAV_RIFF_BYTECNT + wtvsoftmodem_recorder::WAV_FMT_BYTECNT + wtvsoftmodem_recorder::WAV_DATA_CHKHDR_BYTECNT;

	static constexpr uint32_t WAV_CUE_CHKHDR_BYTECNT = 8;
	static constexpr uint32_t WAV_CUE_CHKDAT_COUNT_BYTECNT = 4;
	static constexpr uint32_t WAV_CUE_ENTRY_BYTECNT = 24;
	static constexpr uint32_t WAV_CUE_CHUNK_ID = chk_nameint("cue ");
	static constexpr uint32_t WAV_CUE_DATA_ID = chk_nameint("data");

	static constexpr uint32_t WAV_ADTL_CHKHDR_BYTECNT = 8;
	static constexpr uint32_t WAV_ADTL_CHKDAT_TYPE_BYTECNT = 4;
	static constexpr uint32_t WAV_ADTL_CHUNK_ID = chk_nameint("LIST");
	static constexpr uint32_t WAV_ADTL_LIST_TYPE = chk_nameint("adtl");

	static constexpr uint32_t WAV_ADTL_ENTRY_TYPE = chk_nameint("labl");
	static constexpr uint32_t WAV_ADTL_ENTRY_ID_BYTECNT = 4;
	static constexpr uint32_t WAV_ADTL_ENTRY_SIZE_BYTECNT = 4;
	static constexpr uint32_t WAV_ADTL_ENTRY_HEADER_BYTECNT = wtvsoftmodem_recorder::WAV_ADTL_ENTRY_ID_BYTECNT + wtvsoftmodem_recorder::WAV_ADTL_ENTRY_SIZE_BYTECNT;
	static constexpr uint32_t WAV_ADTL_ENTRY_CUE_ID_BYTECNT = 4;

	struct WAVAnnotation {
		uint32_t id;
		uint32_t sample_offset;
		std::string text;
	};

	std::mutex m_mutex;

	std::ofstream m_wav_file;

	bool m_wav_active;

	wtvsoftmodem_dsp::frequency_t m_sample_rate;
	size_t m_total_written_bytecnt;
	size_t m_aud_bytecnt;
	uint64_t m_aud_samples_written;

	std::queue<int16_t> m_pending_left;
	std::queue<int16_t> m_pending_right;
	std::vector<WAVAnnotation> m_annotations;

	std::vector<int16_t> m_stereo_scratch;

	template <typename T>
	size_t write(const T* data, size_t size)
	{
		m_wav_file.write(reinterpret_cast<const char*>(data), size);

		m_total_written_bytecnt += size;

		return size;
	}	

	template <std::integral T>
	constexpr T clamp(double v);

	void flush();

};

#endif // MAME_WEBTV_WTVSOFTMODEM_RECORDER_H
