// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_IMAGEDEV_VIDEOCAP_H
#define MAME_IMAGEDEV_VIDEOCAP_H

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Only videocap.cpp includes ccap.h, drivers that include this header don't have ccap's include path.
namespace ccap { class Provider; struct VideoFrame; }

class video_capture_device : public device_t, public device_image_interface
{
public:
	video_capture_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	virtual ~video_capture_device();

	virtual std::pair<std::error_condition, std::string> call_load() override;
	virtual void call_unload() override;

	virtual bool core_opens_image_file() const noexcept override { return false; }

	virtual bool is_readable()  const noexcept override { return false; }
	virtual bool is_writeable() const noexcept override { return false; }
	virtual bool is_creatable() const noexcept override { return false; }
	virtual bool is_reset_on_load() const noexcept override { return false; }
	virtual const char *file_extensions() const noexcept override { return ""; }
	virtual const char *image_type_name() const noexcept override { return "videocapture"; }
	virtual const char *image_brief_type_name() const noexcept override { return "vidcap"; }

	void latch_frame();

	void copy_field(uint32_t field, uint32_t width, uint32_t lines, uint32_t stride, uint32_t address, uint32_t *ram, size_t ram_words);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

private:
	static constexpr uint32_t BLACK_WORD = 0x10801080;

	static constexpr uint32_t WANTED_WIDTH  = 640;
	static constexpr uint32_t WANTED_HEIGHT = 480;

	enum open_state : int
	{
		OPEN_IDLE,
		OPEN_RUNNING,
		OPEN_OK,
		OPEN_FAILED,
		OPEN_REPORTED
	};

	static uint8_t rgb_to_y(int r, int g, int b) { return uint8_t(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16); }
	static uint8_t rgb_to_cb(int r, int g, int b) { return uint8_t(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128); }
	static uint8_t rgb_to_cr(int r, int g, int b) { return uint8_t(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128); }

	static bool frame_to_yuyv(const ccap::VideoFrame &frame, std::vector<uint8_t> &out);

	void open_device();
	void close_device();
	bool on_frame(const std::shared_ptr<ccap::VideoFrame> &frame);

	std::mutex m_lock;
	std::vector<uint8_t> m_incoming;
	uint32_t m_incoming_width = 0;
	uint32_t m_incoming_height = 0;
	bool m_incoming_fresh = false;

	std::vector<uint8_t> m_latched;
	uint32_t m_latched_width = 0;
	uint32_t m_latched_height = 0;

	std::unique_ptr<ccap::Provider> m_provider;
	std::thread m_worker;
	std::atomic<int> m_state{ OPEN_IDLE };
	std::string m_name;
	std::string m_found_devices;
};

DECLARE_DEVICE_TYPE(VIDEO_CAPTURE, video_capture_device)

#endif // MAME_IMAGEDEV_VIDEOCAP_H
