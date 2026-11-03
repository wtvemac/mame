// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "videocap.h"

#ifdef MAME_HAVE_CCAP
#include "ccap.h"
#else
// Builds without ccap still need complete types so m_provider can be destroyed. It's never created.
namespace ccap { class Provider { }; struct VideoFrame { }; }
#endif

#include <algorithm>

DEFINE_DEVICE_TYPE(VIDEO_CAPTURE, video_capture_device, "video_capture", "Video Capture")

video_capture_device::video_capture_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, VIDEO_CAPTURE, tag, owner, clock),
	device_image_interface(mconfig, *this)
{
}

video_capture_device::~video_capture_device()
{
}

void video_capture_device::device_start()
{
	//
}

void video_capture_device::device_stop()
{
	close_device();
}

std::pair<std::error_condition, std::string> video_capture_device::call_load()
{
#ifdef MAME_HAVE_CCAP
	m_name = filename();

	return std::make_pair(std::error_condition(), std::string());
#else
	return std::make_pair(image_error::UNSUPPORTED, "MAME was built without ccap, -vidcap can't capture video");
#endif
}

void video_capture_device::call_unload()
{
	close_device();
}

#ifdef MAME_HAVE_CCAP

bool video_capture_device::frame_to_yuyv(const ccap::VideoFrame &frame, std::vector<uint8_t> &out)
{
	const uint32_t width = frame.width & ~1u;
	const uint32_t height = frame.height;

	if (!width || !height || !frame.data[0])
		return false;

	out.resize(size_t(width) * height * 2);

	const bool flip = (frame.orientation == ccap::FrameOrientation::BottomToTop);

	for (uint32_t y = 0; y < height; y++)
	{
		const uint32_t row = flip ? (height - 1 - y) : y;
		const uint8_t *src = frame.data[0] + size_t(row) * frame.stride[0];
		uint8_t *dst = &out[size_t(y) * width * 2];

		switch (frame.pixelFormat)
		{
			case ccap::PixelFormat::YUYV:
			case ccap::PixelFormat::YUYVf:
				std::copy_n(src, size_t(width) * 2, dst);
				break;

			case ccap::PixelFormat::UYVY:
			case ccap::PixelFormat::UYVYf:
				for (uint32_t x = 0; x < width * 2; x += 2)
				{
					dst[x + 0] = src[x + 1];
					dst[x + 1] = src[x + 0];
				}
				break;

			case ccap::PixelFormat::NV12:
			case ccap::PixelFormat::NV12f:
			{
				const uint8_t *uv = frame.data[1] + size_t(row / 2) * frame.stride[1];
				for (uint32_t x = 0; x < width; x += 2)
				{
					dst[x * 2 + 0] = src[x + 0];
					dst[x * 2 + 1] = uv[x + 0];
					dst[x * 2 + 2] = src[x + 1];
					dst[x * 2 + 3] = uv[x + 1];
				}
				break;
			}

			case ccap::PixelFormat::I420:
			case ccap::PixelFormat::I420f:
			{
				const uint8_t *u = frame.data[1] + size_t(row / 2) * frame.stride[1];
				const uint8_t *v = frame.data[2] + size_t(row / 2) * frame.stride[2];
				for (uint32_t x = 0; x < width; x += 2)
				{
					dst[x * 2 + 0] = src[x + 0];
					dst[x * 2 + 1] = u[x / 2];
					dst[x * 2 + 2] = src[x + 1];
					dst[x * 2 + 3] = v[x / 2];
				}
				break;
			}

			case ccap::PixelFormat::RGB24:
			case ccap::PixelFormat::BGR24:
			case ccap::PixelFormat::RGBA32:
			case ccap::PixelFormat::BGRA32:
			{
				const bool bgr = (frame.pixelFormat == ccap::PixelFormat::BGR24) || (frame.pixelFormat == ccap::PixelFormat::BGRA32);
				const bool four = (frame.pixelFormat == ccap::PixelFormat::RGBA32) || (frame.pixelFormat == ccap::PixelFormat::BGRA32);
				const uint32_t bytes = four ? 4 : 3;
				for (uint32_t x = 0; x < width; x += 2)
				{
					const uint8_t *p0 = src + size_t(x) * bytes;
					const uint8_t *p1 = p0 + bytes;
					const int r0 = bgr ? p0[2] : p0[0], g0 = p0[1], b0 = bgr ? p0[0] : p0[2];
					const int r1 = bgr ? p1[2] : p1[0], g1 = p1[1], b1 = bgr ? p1[0] : p1[2];
					dst[x * 2 + 0] = rgb_to_y(r0, g0, b0);
					dst[x * 2 + 1] = rgb_to_cb((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2);
					dst[x * 2 + 2] = rgb_to_y(r1, g1, b1);
					dst[x * 2 + 3] = rgb_to_cr((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2);
				}
				break;
			}

			default:
				return false;
		}
	}

	return true;
}

bool video_capture_device::on_frame(const std::shared_ptr<ccap::VideoFrame> &frame)
{
	std::lock_guard<std::mutex> guard(m_lock);
	if (frame_to_yuyv(*frame, m_incoming))
	{
		m_incoming_width = frame->width & ~1u;
		m_incoming_height = frame->height;
		m_incoming_fresh = true;
	}
	return true;
}

void video_capture_device::open_device()
{
	m_provider = std::make_unique<ccap::Provider>();

	for (const std::string &device : m_provider->findDeviceNames())
		m_found_devices += " \"" + device + "\"";

	m_provider->set(ccap::PropertyName::Width, WANTED_WIDTH);
	m_provider->set(ccap::PropertyName::Height, WANTED_HEIGHT);
	m_provider->set(ccap::PropertyName::PixelFormatInternal, double(ccap::PixelFormat::YUYV));
	m_provider->set(ccap::PropertyName::PixelFormatOutput, double(ccap::PixelFormat::Unknown)); // the capture format as is, frame_to_yuyv() handles the rest
	m_provider->setNewFrameCallback([this] (const std::shared_ptr<ccap::VideoFrame> &frame) { return on_frame(frame); });

	m_state = m_provider->open((m_name == "auto") ? std::string_view() : std::string_view(m_name), true) ? OPEN_OK : OPEN_FAILED;
}

#endif // MAME_HAVE_CCAP

void video_capture_device::close_device()
{
	if (m_worker.joinable())
		m_worker.join();

	m_provider.reset();

	m_state = OPEN_IDLE;
	m_found_devices.clear();
	m_incoming_fresh = false;
	m_latched_width = 0;
	m_latched_height = 0;
}

void video_capture_device::latch_frame()
{
	if (!exists())
		return;

#ifdef MAME_HAVE_CCAP
	const int state = m_state;

	if (state == OPEN_IDLE)
	{
		m_state = OPEN_RUNNING;
		m_worker = std::thread([this] { open_device(); });
	}
	else if (state == OPEN_FAILED || state == OPEN_OK)
	{
		m_worker.join();
		m_state = OPEN_REPORTED;
		if (state == OPEN_FAILED)
			osd_printf_error("%s: couldn't open capture device \"%s\". Found:%s\n", tag(), m_name, m_found_devices.empty() ? " none" : m_found_devices);
		else
			osd_printf_verbose("%s: opened capture device \"%s\"\n", tag(), m_name);
	}
#endif

	std::lock_guard<std::mutex> guard(m_lock);
	if (m_incoming_fresh)
	{
		m_latched.swap(m_incoming);
		m_latched_width = m_incoming_width;
		m_latched_height = m_incoming_height;
		m_incoming_fresh = false;
	}
}

void video_capture_device::copy_field(uint32_t field, uint32_t width, uint32_t lines, uint32_t stride, uint32_t address, uint32_t *ram, size_t ram_words)
{
	const uint32_t words = width / 2;
	const uint32_t src_width = m_latched_width;
	const uint32_t src_height = m_latched_height;

	if (!words || !lines)
		return;

	field &= 1;

	for (uint32_t line = 0; line < lines; line++)
	{
		const size_t start = (size_t(address) + size_t(line) * stride) >> 2;

		if ((start + words) > ram_words)
			break;

		uint32_t *out = &ram[start];

		if (!src_width)
		{
			std::fill_n(out, words, BLACK_WORD);
			continue;
		}

		// Each field takes every other line of the host frame.
		const uint32_t src_y = std::min(uint32_t((uint64_t(2 * line + field) * src_height) / (2 * lines)), src_height - 1);
		const uint8_t *src = &m_latched[size_t(src_y) * src_width * 2];

		for (uint32_t x = 0; x < words; x++)
		{
			const uint32_t x0 = uint32_t((uint64_t(2 * x + 0) * src_width) / width);
			const uint32_t x1 = uint32_t((uint64_t(2 * x + 1) * src_width) / width);
			const uint8_t *chroma = &src[(x0 & ~1u) * 2];

			out[x] = (uint32_t(src[x0 * 2]) << 24) | (uint32_t(chroma[1]) << 16) | (uint32_t(src[x1 * 2]) << 8) | uint32_t(chroma[3]);
		}
	}
}
