// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVPOINTER_H
#define MAME_WEBTV_WTVPOINTER_H

#pragma once

#include "cpu/mips/mips3.h"
#include "screen.h"

#include <chrono>
#include <vector>

class layout_view_item;

class wtvpointer_device : public device_t
{

public:

	wtvpointer_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	template <typename T> void set_hostcpu(T &&tag) { m_hostcpu.set_tag(std::forward<T>(tag)); }
	template <typename T> void set_screen(T &&tag) { m_screen.set_tag(std::forward<T>(tag)); }
	template <typename... T> void set_screen_to_frame(T &&...args) { m_screen_to_frame.set(std::forward<T>(args)...); }

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	static constexpr uint32_t TABLE_SIZE = 0x3c;
	static constexpr uint32_t TABLE_VERSION = 0x0001003c;
	static constexpr uint32_t CAP_CLICK = 1;
	static constexpr uint32_t CAP_HOVER = 2;
	static constexpr uint32_t CAP_DISPLAY = 4;
	static constexpr uint32_t CAP_SCROLL = 8;
	static constexpr uint32_t CAP_SCROLL_PIXELS = 16;
	static constexpr uint32_t CAP_NAVIGATION = 32;
	static constexpr unsigned HEARTBEAT_TIMEOUT = 120; // about two seconds of input frames
	static constexpr uint32_t MAX_DISPLAY_SIZE = 4096;

	static constexpr std::chrono::milliseconds LONG_PRESS_TIME{600};
	static constexpr int32_t DRAG_DISTANCE = 12;
	static constexpr float EDGE_WIDTH = 0.05f; // fraction of the screen width
	static constexpr int32_t EDGE_SLOP = 24;
	static constexpr int32_t EDGE_SWIPE_DISTANCE = 64;
	static constexpr int32_t SCROLL_KEY_PIXELS = 32;
	static constexpr int32_t MAX_SCROLL_PIXELS = 4096;
	static constexpr int32_t WHEEL_DETENT = 120;
	static constexpr int32_t MAX_WHEEL_STEPS = 8;

	enum table_offset_t : uint32_t
	{
		T_MAGIC     = 0x00,
		T_VERSION   = 0x04,
		T_HEARTBEAT = 0x08,
		T_CAPS      = 0x0c,
		T_BUILD     = 0x10,
		T_WIDTH     = 0x14,
		T_HEIGHT    = 0x18,
		T_OFFSETS   = 0x1c,
		T_SEQ       = 0x20,
		T_CMD       = 0x24,
		T_PARAM1    = 0x28,
		T_PARAM2    = 0x2c,
		T_ACK       = 0x30,
		T_RESULT    = 0x34,
		T_VIEW      = 0x38
	};

	enum pointer_cmd_t : uint32_t
	{
		CMD_HOVER         = 1,
		CMD_CLICK         = 2,
		CMD_SCROLL_UP     = 3,
		CMD_SCROLL_DOWN   = 4,
		CMD_SCROLL_PIXELS = 5,
		CMD_BACK          = 6,
		CMD_OPTIONS       = 7
	};

	required_device<mips3_device> m_hostcpu;
	required_device<screen_device> m_screen;
	device_delegate<bool (int32_t, int32_t, int32_t &, int32_t &, const rectangle &)> m_screen_to_frame;

	// Host pointer state.
	std::vector<layout_view *> m_host_views;
	bool m_table_logged = false;
	bool m_host_inside = false;
	bool m_host_click = false;
	int32_t m_host_touch = -1;
	bool m_host_down = false;
	bool m_host_drag = false;
	bool m_host_is_touch = false;
	bool m_host_edge = false;
	bool m_host_consumed = false;
	uint32_t m_navigation_pending = 0;
	std::chrono::steady_clock::time_point m_host_pressed;
	int32_t m_host_scroll_y = 0;
	int32_t m_host_scroll = 0; // pending pixels, converted to keys for legacy stubs
	int32_t m_host_wheel = 0;
	int32_t m_wheel_remainder = 0;
	int32_t m_host_x = 0;
	int32_t m_host_y = 0;
	int32_t m_host_click_x = 0;
	int32_t m_host_click_y = 0;
	int32_t m_sent_x = -1;
	int32_t m_sent_y = -1;

	// Guest table state.
	uint32_t m_table = 0;
	uint32_t m_table_heartbeat = 0;
	uint32_t m_table_caps = 0;
	unsigned m_table_idle_polls = 0;
	bool m_table_live = false;
	bool m_table_pending = false;
	uint32_t m_table_seq = 0;
	bool m_click_queued = false;
	int32_t m_click_x = 0;
	int32_t m_click_y = 0;

	void poll();

	static void unorient_point(const layout_view_item &item, float &x, float &y);

	void bind_host_pointer();
	void host_pointer_lost();
	void clear_scroll();
	void queue_navigation(uint32_t command);
	void check_long_press();
	void host_pointer_wheel(int32_t delta);
	void host_pointer_position(float x, float y, uint32_t pressed, uint32_t released = 0, bool touch = false);

	bool table_address_valid(uint32_t address);
	bool table_idle();
	bool update_table();
	void clear_table();
	void send_table_event(uint32_t cmd, int32_t param1, int32_t param2);

	uint32_t guest_read(uint32_t vaddr);
	void guest_write(uint32_t vaddr, uint32_t data);
};

DECLARE_DEVICE_TYPE(WTVPOINTER, wtvpointer_device)

#endif // MAME_WEBTV_WTVPOINTER_H
