// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "wtvsoftmodem_recorder.h"

wtvsoftmodem_recorder::wtvsoftmodem_recorder() :
	m_wav_active(false),
	m_total_written_bytecnt(0),
	m_aud_bytecnt(0),
	m_aud_samples_written(0)
{
	//
}

wtvsoftmodem_recorder::~wtvsoftmodem_recorder()
{
	wtvsoftmodem_recorder::close();
}

void wtvsoftmodem_recorder::set_sample_rate(wtvsoftmodem_dsp::frequency_t new_sample_rate)
{
	m_sample_rate = new_sample_rate;
}

void wtvsoftmodem_recorder::open(const std::string &filetag)
{
	if (m_wav_file.is_open())
		wtvsoftmodem_recorder::close();

	std::string timestamp = std::format("{:%Y-%m-%d_%H%M%S}", std::chrono::system_clock::now());

	std::string filename = filetag + "_" + timestamp + ".wav";
	m_wav_file.open(filename, std::ios::binary);

	m_total_written_bytecnt = 0;
	m_aud_bytecnt = 0;
	m_aud_samples_written = 0;
	m_pending_left = {};
	m_pending_right = {};
	m_annotations.clear();

	// Write a blank starting data (RIFF header, full format chunk and WAV data chunk header). It will be filled in once the WAV file closes.
	std::vector<char> header(wtvsoftmodem_recorder::WAV_START_BYTECNT, 0);
	wtvsoftmodem_recorder::write(header.data(), header.size());

	m_wav_active = true;
}

template <std::integral T>
void wtvsoftmodem_recorder::push_left(const T* samples, uint32_t sample_count)
{
	if (!m_wav_active)
		return;

	std::lock_guard<std::mutex> lock(m_mutex);

	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
		m_pending_left.push(int16_t(uint16_t(samples[sample_idx])));

	wtvsoftmodem_recorder::flush_samples();
}
template void wtvsoftmodem_recorder::push_left<uint32_t>(const uint32_t* samples, uint32_t sample_count);
template void wtvsoftmodem_recorder::push_left<wtvsoftmodem_dsp::sample_t>(const wtvsoftmodem_dsp::sample_t* samples, uint32_t sample_count);

template <std::integral T>
void wtvsoftmodem_recorder::push_right(const T* samples, uint32_t sample_count)
{
	if (!m_wav_active)
		return;

	std::lock_guard<std::mutex> lock(m_mutex);

	for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++)
		m_pending_right.push(int16_t(uint16_t(samples[sample_idx])));

	wtvsoftmodem_recorder::flush_samples();
}
template void wtvsoftmodem_recorder::push_right<uint32_t>(const uint32_t* samples, uint32_t sample_count);
template void wtvsoftmodem_recorder::push_right<wtvsoftmodem_dsp::sample_t>(const wtvsoftmodem_dsp::sample_t* samples, uint32_t sample_count);

const uint64_t wtvsoftmodem_recorder::get_current_sample_index()
{
	return (wtvsoftmodem_recorder::get_written_sample_count() + wtvsoftmodem_recorder::get_pending_sample_count());
}

const uint64_t wtvsoftmodem_recorder::get_pending_sample_count()
{
	return std::min(m_pending_left.size(), m_pending_right.size());
}

const uint64_t wtvsoftmodem_recorder::get_written_sample_count()
{
	return m_aud_samples_written;
}

void wtvsoftmodem_recorder::annotate(const std::string& message)
{
	if (!m_wav_active)
		return;

	std::lock_guard<std::mutex> lock(m_mutex);

	uint32_t current_sample = static_cast<uint32_t>(m_aud_samples_written + wtvsoftmodem_recorder::get_pending_sample_count());

	m_annotations.push_back(
		{
			.id = static_cast<uint32_t>(m_annotations.size() + 1),
			.sample_offset = current_sample,
			.text = message,
		}
	);
}

size_t wtvsoftmodem_recorder::flush_samples()
{
	uint64_t pending_sample_count = wtvsoftmodem_recorder::get_pending_sample_count();

	if (pending_sample_count == 0)
		return 0;

	size_t data_written_bytecnt = 0;

	size_t sample_stide = sizeof(int16_t);

	m_stereo_scratch.clear();
	m_stereo_scratch.reserve(pending_sample_count * sample_stide);

	for (uint64_t sample_idx = 0; sample_idx < pending_sample_count; sample_idx++)
	{
		m_stereo_scratch.push_back(m_pending_left.front());
		m_pending_left.pop();

		m_stereo_scratch.push_back(m_pending_right.front());
		m_pending_right.pop();
	}

	wtvsoftmodem_recorder::write(m_stereo_scratch.data(), m_stereo_scratch.size() * sample_stide);

	data_written_bytecnt = static_cast<uint32_t>(m_stereo_scratch.size() * sample_stide);
	m_aud_bytecnt += data_written_bytecnt;

	m_aud_samples_written += pending_sample_count;

	return data_written_bytecnt;
}

size_t wtvsoftmodem_recorder::purge_samples()
{
	size_t data_written_bytecnt = 0;

	while (!m_pending_left.empty() || !m_pending_right.empty())
	{
		int16_t left_val = 0x0000;
		int16_t right_val = 0x0000;

		if (!m_pending_left.empty())
		{
			left_val = m_pending_left.front();
			m_pending_left.pop();
		}

		if (!m_pending_right.empty())
		{
			right_val = m_pending_right.front();
			m_pending_right.pop();
		}

		wtvsoftmodem_recorder::write(&left_val, sizeof(left_val));
		wtvsoftmodem_recorder::write(&right_val, sizeof(right_val));

		data_written_bytecnt += sizeof(left_val) + sizeof(right_val);

		m_aud_samples_written++;
	}

	m_aud_bytecnt += data_written_bytecnt;

	return data_written_bytecnt;
}

size_t wtvsoftmodem_recorder::write_annotations()
{
	if (m_annotations.empty())
		return 0;

	size_t data_written_bytecnt = 0;

	uint32_t num_annotations = static_cast<uint32_t>(m_annotations.size());

	// Annotation cue chunk (positional information)

	// Calculate cur chunk data size
	uint32_t cue_chunk_bytecnt = wtvsoftmodem_recorder::WAV_CUE_CHKDAT_COUNT_BYTECNT + (num_annotations * wtvsoftmodem_recorder::WAV_CUE_ENTRY_BYTECNT);

	// Write cue chunk block
	wtvsoftmodem_recorder::write(&WAV_CUE_CHUNK_ID, 4);
	wtvsoftmodem_recorder::write(&cue_chunk_bytecnt, 4);
	wtvsoftmodem_recorder::write(&num_annotations, 4);
	for (const auto& annotation : m_annotations)
	{
		uint32_t cue_position = 0;
		uint32_t cue_chunk_start = 0;
		uint32_t cue_block_start = 0;

		wtvsoftmodem_recorder::write(&annotation.id, 4);
		wtvsoftmodem_recorder::write(&cue_position, 4);
		wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_CUE_DATA_ID, 4);
		wtvsoftmodem_recorder::write(&cue_chunk_start, 4);
		wtvsoftmodem_recorder::write(&cue_block_start, 4);
		wtvsoftmodem_recorder::write(&annotation.sample_offset, 4);
	}
	data_written_bytecnt += wtvsoftmodem_recorder::WAV_CUE_CHKHDR_BYTECNT + cue_chunk_bytecnt;

	// Annotation associated data list chunk (actual text entries)

	// Calculate list chunk data size 
	uint32_t list_chunk_bytecnt = wtvsoftmodem_recorder::WAV_ADTL_CHKDAT_TYPE_BYTECNT;
	for (const auto& annotation : m_annotations)
	{
		uint32_t text_bytecnt = static_cast<uint32_t>(annotation.text.length() + 1); // Text + null byte
		uint32_t entry_bytecnt = wtvsoftmodem_recorder::WAV_ADTL_ENTRY_CUE_ID_BYTECNT + text_bytecnt;
		list_chunk_bytecnt += wtvsoftmodem_recorder::WAV_ADTL_ENTRY_HEADER_BYTECNT + entry_bytecnt;
		// RIFF needs to be word aligned, text being variable length can cause misalignment so it must be padded so it's aligned
		list_chunk_bytecnt += (entry_bytecnt % 2);
	}

	// Write list chunk block
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_ADTL_CHUNK_ID, 4);
	wtvsoftmodem_recorder::write(&list_chunk_bytecnt, 4);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_ADTL_LIST_TYPE, 4);
	for (const auto& annotation : m_annotations)
	{
		uint32_t text_bytecnt = static_cast<uint32_t>(annotation.text.length() + 1); // Text + null byte
		uint32_t entry_bytecnt = wtvsoftmodem_recorder::WAV_ADTL_ENTRY_CUE_ID_BYTECNT + text_bytecnt;

		wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_ADTL_ENTRY_TYPE, 4);
		wtvsoftmodem_recorder::write(&entry_bytecnt, 4);
		wtvsoftmodem_recorder::write(&annotation.id, 4);
		wtvsoftmodem_recorder::write(annotation.text.c_str(), text_bytecnt);

		// Add padding
		if ((entry_bytecnt % 2) != 0)
		{
			char pad = 0x00;
			wtvsoftmodem_recorder::write(&pad, 1);
		}
	}
	data_written_bytecnt += wtvsoftmodem_recorder::WAV_ADTL_CHKHDR_BYTECNT + list_chunk_bytecnt;

	return data_written_bytecnt;
}

void wtvsoftmodem_recorder::close()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_wav_file.is_open())
		return;

	m_wav_active = false;

	wtvsoftmodem_recorder::purge_samples();

	size_t data_end_offset = m_total_written_bytecnt;

	// Skip over RIFF in the header since that will be written last.
	m_wav_file.seekp(wtvsoftmodem_recorder::WAV_RIFF_BYTECNT, std::ios::beg);

	uint32_t sample_rate = static_cast<uint32_t>(m_sample_rate);

	// WAV format chunk
	uint16_t sample_block_stride = wtvsoftmodem_recorder::WAV_AUDIO_CHANNEL_CNT * sizeof(int16_t);
	uint32_t audio_byte_rate = sample_rate * sample_block_stride;
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_FMT_CHUNK_ID, 4);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_FMT_CHKDAT_BYTECNT, 4);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_AUDIO_FORMAT, 2);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_AUDIO_CHANNEL_CNT, 2);
	wtvsoftmodem_recorder::write(&sample_rate, 4);
	wtvsoftmodem_recorder::write(&audio_byte_rate, 4);
	wtvsoftmodem_recorder::write(&sample_block_stride, 2);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_AUDIO_SAMPLE_BITS, 2);

	// WAV data chunk
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_DATA_CHUNK_ID, 4);
	wtvsoftmodem_recorder::write(&m_aud_bytecnt, 4);
	// -- WAV data (already written) --

	// Seek to end to write annotations (cue and associated data list chunks)
	m_wav_file.seekp(data_end_offset, std::ios::beg);
	size_t file_bytecnt = data_end_offset + wtvsoftmodem_recorder::write_annotations();

	// Now that we calculate the total WAV file size, write the WAV RIFF header.
	m_wav_file.seekp(0, std::ios::beg);
	uint32_t riff_file_bytecnt = static_cast<uint32_t>(file_bytecnt - wtvsoftmodem_recorder::WAV_RIFF_CHKHDR_BYTECNT);
	// Main WAV RIFF chunk (all other chunks are sub chunks)
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_RIFF_CHUNK_ID, 4);
	wtvsoftmodem_recorder::write(&riff_file_bytecnt, 4);
	wtvsoftmodem_recorder::write(&wtvsoftmodem_recorder::WAV_RIFF_START_DATA, 4);

	m_wav_file.close();
}

template <std::integral T>
constexpr T wtvsoftmodem_recorder::clamp(double v)
{
	long long rounded = std::llround(v);
	auto clamped = std::clamp(
		rounded,
		static_cast<long long>(std::numeric_limits<T>::min()),
		static_cast<long long>(std::numeric_limits<T>::max())
	);
	return static_cast<T>(clamped);
}
