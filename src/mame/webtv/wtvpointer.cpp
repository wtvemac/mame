// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "emu.h"
#include "wtvpointer.h"
#include "wtvfart.h"

#include "render.h"
#include "rendlay.h"
#include "uiinput.h"

DEFINE_DEVICE_TYPE(WTVPOINTER, wtvpointer_device, "wtvpointer", "WebTV pointer support")

wtvpointer_device::wtvpointer_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTVPOINTER, tag, owner, clock),
	m_hostcpu(*this, finder_base::DUMMY_TAG),
	m_screen(*this, finder_base::DUMMY_TAG),
	m_screen_to_frame(*this)
{
}

void wtvpointer_device::device_start()
{
	m_screen_to_frame.resolve();
	machine().ui_input().set_mouse_wheel_handler([this](const ui_event &event)
	{
		if (machine().paused() || !event.target || m_host_down || m_host_touch >= 0)
			return;

		float x;
		float y;
		if (!event.target->map_point_container(event.mouse_x, event.mouse_y, m_screen->container(), x, y))
			return;

		for (layout_view_item &item : event.target->current_view().visible_screen_items())
		{
			if (item.screen() == m_screen.target())
			{
				wtvpointer_device::unorient_point(item, x, y);
				break;
			}
		}

		wtvpointer_device::host_pointer_position(x, y, 0);
		wtvpointer_device::host_pointer_wheel(event.zdelta);
	});

	machine().add_notifier(MACHINE_NOTIFY_FRAME, machine_notify_delegate(&wtvpointer_device::poll, this));

	wtvpointer_device::bind_host_pointer();

	save_item(NAME(m_table));
	save_item(NAME(m_table_heartbeat));
	save_item(NAME(m_table_caps));
	save_item(NAME(m_table_idle_polls));
	save_item(NAME(m_table_live));
	save_item(NAME(m_table_pending));
	save_item(NAME(m_table_seq));
	save_item(NAME(m_click_queued));
	save_item(NAME(m_click_x));
	save_item(NAME(m_click_y));
	save_item(NAME(m_sent_x));
	save_item(NAME(m_sent_y));
}

void wtvpointer_device::device_reset()
{
	wtvpointer_device::clear_table();
	wtvpointer_device::host_pointer_lost();

	m_host_drag = false;
	m_host_consumed = false;
	m_host_edge = false;
	m_host_is_touch = false;

	m_sent_x = -1;
	m_sent_y = -1;
}

void wtvpointer_device::unorient_point(const layout_view_item &item, float &x, float &y)
{
	if (item.orientation() & ORIENTATION_FLIP_X)
		x = 1.0f - x;

	if (item.orientation() & ORIENTATION_FLIP_Y)
		y = 1.0f - y;

	if (item.orientation() & ORIENTATION_SWAP_XY)
		std::swap(x, y);
}

void wtvpointer_device::poll()
{
	// Don't inject input while we're paused.
	if (machine().paused())
		return;

	wtvpointer_device::bind_host_pointer();

	bool clicked = std::exchange(m_host_click, false);

	const bool table_live = update_table();
	if (!m_host_inside || !table_live)
	{
		m_host_down = false;
		m_click_queued = false;
		m_navigation_pending = 0;

		clear_scroll();

		return;
	}

	if (!(m_table_caps & CAP_NAVIGATION))
		m_navigation_pending = 0;

	wtvpointer_device::check_long_press();

	if (m_navigation_pending)
	{
		if (wtvpointer_device::table_idle())
			wtvpointer_device::send_table_event(std::exchange(m_navigation_pending, 0), 0, 0);

		return;
	}

	if (!(m_table_caps & CAP_SCROLL))
		wtvpointer_device::clear_scroll();

	rectangle guest_viewport(0, -1, 0, -1);

	if (m_table_caps & CAP_DISPLAY)
	{
		const uint32_t width = guest_read(m_table + T_WIDTH);
		const uint32_t height = guest_read(m_table + T_HEIGHT);
		const uint32_t offsets = guest_read(m_table + T_OFFSETS);

		// display isn't initialised yet
		if (!width || width > MAX_DISPLAY_SIZE || !height || height > MAX_DISPLAY_SIZE)
			return;

		const int32_t left = int16_t(offsets >> 16);
		const int32_t top = int16_t(offsets & 0xffff);

		guest_viewport = rectangle(left, left + int32_t(width) - 1, top, top + int32_t(height) - 1);
	}

	int32_t frame_x;
	int32_t frame_y;
	if (!m_screen_to_frame(m_host_x, m_host_y, frame_x, frame_y, guest_viewport))
	{
		m_host_scroll = 0;
		m_host_wheel = 0;

		if (!m_host_edge)
			m_host_down = false;

		return;
	}

	if (clicked && !m_screen_to_frame(m_host_click_x, m_host_click_y, frame_x, frame_y, guest_viewport))
		clicked = false;

	if (clicked && (m_table_caps & CAP_CLICK))
	{
		m_click_queued = true;
		m_click_x = frame_x;
		m_click_y = frame_y;
	}

	if (!wtvpointer_device::table_idle())
		return;

	if (m_host_wheel && (m_table_caps & CAP_SCROLL))
	{
		const bool down = (m_host_wheel > 0);

		send_table_event(down ? CMD_SCROLL_DOWN : CMD_SCROLL_UP, frame_x, frame_y);

		m_host_wheel += down ? -1 : 1;
	}
	else if (m_host_scroll && (m_table_caps & CAP_SCROLL_PIXELS))
	{
		int32_t anchor_x;
		int32_t anchor_y;
		if (!m_screen_to_frame(m_host_click_x, m_host_click_y, anchor_x, anchor_y, guest_viewport))
		{
			m_host_scroll = 0;
			return;
		}

		const uint32_t anchor = (uint32_t(uint16_t(anchor_x)) << 16) | uint16_t(anchor_y);

		wtvpointer_device::send_table_event(CMD_SCROLL_PIXELS, int32_t(anchor), std::exchange(m_host_scroll, 0));

		m_sent_x = frame_x;
		m_sent_y = frame_y;
	}
	else if (std::abs(m_host_scroll) >= SCROLL_KEY_PIXELS && (m_table_caps & CAP_SCROLL))
	{
		const bool down = m_host_scroll > 0;

		wtvpointer_device::send_table_event(down ? CMD_SCROLL_DOWN : CMD_SCROLL_UP, frame_x, frame_y);

		m_host_scroll += down ? -SCROLL_KEY_PIXELS : SCROLL_KEY_PIXELS;
	}
	else if (m_click_queued && (m_table_caps & CAP_CLICK))
	{
		wtvpointer_device::send_table_event(CMD_CLICK, m_click_x, m_click_y);

		m_click_queued = false;
	}
	else if (!m_host_down && (m_table_caps & CAP_HOVER) && (frame_x != m_sent_x || frame_y != m_sent_y))
	{
		wtvpointer_device::send_table_event(CMD_HOVER, frame_x, frame_y);
	}
}

void wtvpointer_device::bind_host_pointer()
{
	for (render_target &target : machine().render().targets())
	{
		layout_view *view = &target.current_view();
		if (std::find(m_host_views.begin(), m_host_views.end(), view) != m_host_views.end())
			continue;

		m_host_views.push_back(view);

		view->set_pointer_updated_callback(layout_view::pointer_updated_delegate(
			[this, view](osd::ui_event_handler::pointer type, uint16_t id, uint16_t, float x, float y, uint32_t, uint32_t pressed, uint32_t released, int16_t)
			{
				const bool touch = (type == osd::ui_event_handler::pointer::TOUCH);
				if (touch)
				{
					if (m_host_touch < 0 && (pressed & 1))
						m_host_touch = id;

					if (m_host_touch != id)
						return;
				}
				else if (type != osd::ui_event_handler::pointer::MOUSE || id != 0 || m_host_touch >= 0)
				{
					return;
				}

				m_host_inside = false;
				for (layout_view_item &item : view->visible_screen_items())
				{
					if (item.screen() != m_screen.target())
						continue;

					const render_bounds bounds = item.bounds();
					if (!bounds.includes(x, y) || bounds.width() <= 0 || bounds.height() <= 0)
						continue;

					float sx = (x - bounds.x0) / bounds.width();
					float sy = (y - bounds.y0) / bounds.height();
					wtvpointer_device::unorient_point(item, sx, sy);
					wtvpointer_device::host_pointer_position(sx, sy, pressed, released, touch);

					if (!touch)
					{
						if (pressed & 8) // fourth button
							wtvpointer_device::queue_navigation(CMD_BACK);
						else if (pressed & 2) // secondary button
							wtvpointer_device::queue_navigation(CMD_OPTIONS);
					}

					break;
				}

				if (!m_host_inside)
				{
					m_host_down = false;
					m_host_click = false;

					wtvpointer_device::clear_scroll();
				}

				if (touch && (released & 1))
					m_host_touch = -1;
			}
		));

		auto leave = [this](osd::ui_event_handler::pointer type, uint16_t id, uint16_t, float, float, uint32_t, int16_t)
		{
			if ((type == osd::ui_event_handler::pointer::MOUSE && id == 0 && m_host_touch < 0) || (type == osd::ui_event_handler::pointer::TOUCH && m_host_touch == id))
				wtvpointer_device::host_pointer_lost();
		};

		view->set_pointer_left_callback(layout_view::pointer_left_delegate(leave));
		view->set_pointer_aborted_callback(layout_view::pointer_left_delegate(leave));
		view->set_forget_pointers_callback(layout_view::forget_pointers_delegate([this]() { host_pointer_lost(); }));
	}
}

void wtvpointer_device::host_pointer_lost()
{
	m_host_touch = -1;
	m_navigation_pending = 0;
	m_host_inside = false;
	m_host_click = false;
	m_host_down = false;

	wtvpointer_device::clear_scroll();
}

void wtvpointer_device::clear_scroll()
{
	m_host_scroll = 0;
	m_host_wheel = 0;
	m_wheel_remainder = 0;
}

void wtvpointer_device::queue_navigation(uint32_t command)
{
	if (!m_host_inside || machine().paused() || !m_table_live || !(m_table_caps & CAP_NAVIGATION) || (command != CMD_BACK && command != CMD_OPTIONS))
		return;

	m_navigation_pending = command;
	m_host_consumed = true;
	m_host_click = false;
	m_click_queued = false;
	m_host_scroll = 0;
	m_host_wheel = 0;
}

void wtvpointer_device::check_long_press()
{
	if (m_host_down && m_host_is_touch && !m_host_drag && !m_host_consumed && std::chrono::steady_clock::now() - m_host_pressed >= LONG_PRESS_TIME)
		wtvpointer_device::queue_navigation(CMD_OPTIONS);
}

void wtvpointer_device::host_pointer_wheel(int32_t delta)
{
	if (!m_host_inside || m_host_down
			|| !m_table_live || !(m_table_caps & CAP_SCROLL))
		return;

	m_wheel_remainder += delta;

	const int32_t steps = m_wheel_remainder / WHEEL_DETENT;

	m_wheel_remainder %= WHEEL_DETENT;

	m_host_wheel = std::clamp(m_host_wheel - steps, -MAX_WHEEL_STEPS, MAX_WHEEL_STEPS);
}

void wtvpointer_device::host_pointer_position(float x, float y, uint32_t pressed, uint32_t released, bool touch)
{
	const rectangle &area = m_screen->visible_area();
	m_host_inside = x >= 0 && x < 1 && y >= 0 && y < 1;
	if (!m_host_inside)
		return;

	m_host_x = area.left() + int32_t(x * area.width());
	m_host_y = area.top() + int32_t(y * area.height());

	if (pressed & 1)
	{
		m_host_down = true;
		m_host_drag = false;
		m_host_is_touch = touch;
		m_host_edge = touch && x <= EDGE_WIDTH;
		m_host_consumed = false;
		m_host_pressed = std::chrono::steady_clock::now();
		m_host_scroll_y = m_host_y;
		m_host_click_x = m_host_x;
		m_host_click_y = m_host_y;
	}

	if (!m_host_down)
		return;

	const int32_t dx = m_host_x - m_host_click_x;
	const int32_t dy = m_host_y - m_host_click_y;
	if (std::abs(dx) > DRAG_DISTANCE || std::abs(dy) > DRAG_DISTANCE)
		m_host_drag = true;

	if (m_host_edge && std::abs(dy) > EDGE_SLOP)
		m_host_edge = false;

	if (m_host_edge && !m_host_consumed && dx >= EDGE_SWIPE_DISTANCE)
		wtvpointer_device::queue_navigation(CMD_BACK);

	if (m_host_drag && !m_host_edge && !m_host_consumed && (m_table_caps & (CAP_SCROLL | CAP_SCROLL_PIXELS)))
	{
		m_host_scroll = std::clamp(m_host_scroll + m_host_scroll_y - m_host_y, -MAX_SCROLL_PIXELS, MAX_SCROLL_PIXELS);
		m_host_scroll_y = m_host_y;
	}

	if (released & 1)
	{
		wtvpointer_device::check_long_press();
		m_host_click = !m_host_drag && !m_host_consumed;
		m_host_down = false;
	}
}

bool wtvpointer_device::table_address_valid(uint32_t address)
{
	// must be in RAM. Not in ROM, in Solo/spot regs etc...
	return !wtvfart::overlaps_discovery(address, TABLE_SIZE) && wtvfart::ram_range_valid(m_hostcpu->space(AS_PROGRAM), address, TABLE_SIZE);
}

bool wtvpointer_device::table_idle()
{
	// The last command was ACKd so we're idle.
	return wtvpointer_device::guest_read(m_table + T_SEQ) == guest_read(m_table + T_ACK);
}

void wtvpointer_device::clear_table()
{
	if (m_table)
	{
		m_sent_x = -1;
		m_sent_y = -1;
	}

	m_table = 0;
	m_table_heartbeat = 0;
	m_table_caps = 0;
	m_table_idle_polls = 0;
	m_table_live = false;
	m_table_pending = false;
	m_table_seq = 0;
	m_click_queued = false;
	m_click_x = 0;
	m_click_y = 0;
}

bool wtvpointer_device::update_table()
{
	const uint32_t table = wtvfart::find_feature(m_hostcpu->space(AS_PROGRAM), wtvfart::FEATURE_POINTER);
	if (!table || !wtvpointer_device::table_address_valid(table) || guest_read(table + T_MAGIC) != wtvfart::MAGIC || wtvpointer_device::guest_read(table + T_VERSION) != TABLE_VERSION)
	{
		wtvpointer_device::clear_table();

		return false;
	}

	const uint32_t heartbeat = guest_read(table + T_HEARTBEAT);
	if (table != m_table)
	{
		wtvpointer_device::clear_table();

		m_table = table;
		m_table_heartbeat = heartbeat;

		return false; // a stale RAM signature can't enable input
	}
	if (heartbeat != m_table_heartbeat)
	{
		m_table_heartbeat = heartbeat;
		m_table_idle_polls = 0;
		if (!m_table_live)
		{
			m_table_live = true;
			m_sent_x = -1;
			m_sent_y = -1;

			if (!m_table_logged)
			{
				popmessage("Pointer support enabled (patched approm %u)", guest_read(table + T_BUILD));
				m_table_logged = true;
			}
		}
	}
	else if (m_table_idle_polls < HEARTBEAT_TIMEOUT && ++m_table_idle_polls == HEARTBEAT_TIMEOUT)
	{
		m_table_live = false;
		m_table_pending = false;
		m_click_queued = false;
	}
	if (!m_table_live)
		return false;

	m_table_caps = wtvpointer_device::guest_read(table + T_CAPS);
	if (m_table_pending && wtvpointer_device::guest_read(table + T_ACK) == m_table_seq)
		m_table_pending = false;

	return true;
}

void wtvpointer_device::send_table_event(uint32_t cmd, int32_t param1, int32_t param2)
{
	const uint32_t seq = wtvpointer_device::guest_read(m_table + T_SEQ) + 1;

	wtvpointer_device::guest_write(m_table + T_CMD, cmd);
	wtvpointer_device::guest_write(m_table + T_PARAM1, uint32_t(param1));
	wtvpointer_device::guest_write(m_table + T_PARAM2, uint32_t(param2));
	wtvpointer_device::guest_write(m_table + T_SEQ, seq);

	m_table_seq = seq;
	m_table_pending = true;
	m_sent_x = param1;
	m_sent_y = param2;
}

uint32_t wtvpointer_device::guest_read(uint32_t vaddr)
{
	return wtvfart::read_word(m_hostcpu->space(AS_PROGRAM), vaddr);
}

void wtvpointer_device::guest_write(uint32_t vaddr, uint32_t data)
{
	m_hostcpu->space(AS_PROGRAM).write_dword(vaddr & 0x1fffffff, data);
}
