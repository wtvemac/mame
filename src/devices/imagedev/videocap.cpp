// license:BSD-3-Clause
// copyright-holders:wtvemac
/*********************************************************************

    ccapvideo.cpp

    Image device for video capture.

*********************************************************************/

#include "emu.h"
#include "ccapvideo.h"

DEFINE_DEVICE_TYPE(IMAGE_VIDCAP, video_capture_device, "video_capture", "Video Capture")

video_capture_device::video_capture_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, IMAGE_VIDCAP, tag, owner, clock),
	device_image_interface(mconfig, *this)
{
}

video_capture_device::~video_capture_device()
{
	//
}


void video_capture_device::device_start()
{
	//
}

void video_capture_device::device_reset()
{
	//
}
