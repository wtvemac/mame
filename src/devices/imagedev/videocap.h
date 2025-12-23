// license:BSD-3-Clause
// copyright-holders:wtvemac
/*********************************************************************

    ccapvideo.h

    Image device for video capture.

*********************************************************************/

#ifndef MAME_IMAGEDEV_VIDEOCAP_H
#define MAME_IMAGEDEV_VIDEOCAP_H

#pragma once

class video_capture_device : public device_t, public device_image_interface
{

public:

	video_capture_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	virtual ~video_capture_device();

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

};

DECLARE_DEVICE_TYPE(IMAGE_VIDCAP, avivideo_image_device)

#endif // MAME_IMAGEDEV_VIDEOCAP_H
