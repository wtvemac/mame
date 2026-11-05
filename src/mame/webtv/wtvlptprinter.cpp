// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// DeskJet 600 (PRINTER_IS_BW_PRINTER=true) and DeskJet 660C (default) support

#include "emu.h"

#include "wtvlptprinter.h"

#include "emuopts.h"
#include "fileio.h"
#include "render.h"
#include "rendfont.h"

#include "png.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

DEFINE_DEVICE_TYPE(WTV_LPT_DESKJET, wtv_lpt_deskjet_device, "wtv_deskjet600", "HP DeskJet 600 (WebTV printer)")

wtv_lpt_deskjet_device::wtv_lpt_deskjet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, WTV_LPT_DESKJET, tag, owner, clock),
	device_centronics_peripheral_interface(mconfig, *this),
	m_font_baseline(-1.0F)
{
	//
}

wtv_lpt_deskjet_device::~wtv_lpt_deskjet_device() = default;

void wtv_lpt_deskjet_device::device_start()
{
	m_ack_start_timer = timer_alloc(FUNC(wtv_lpt_deskjet_device::ack_start), this);
	m_ack_end_timer = timer_alloc(FUNC(wtv_lpt_deskjet_device::ack_end), this);
	m_job_timer = timer_alloc(FUNC(wtv_lpt_deskjet_device::job_idle), this);

	m_mode = MODE_COMPAT;
	m_data = DATA_LINES_IDLE;
	m_strobe_n = true;
	m_autofd_n = true;
	m_init_n = true;
	m_select_in_n = true;
	m_strobe_seen = false;
	m_ext_request = 0;
	m_reverse_pos = 0;
	m_send_high_nibble = false;
	m_busy = false;
	m_ack_n = true;
	m_perror = false;
	m_select = true;
	m_fault_n = true;
	m_job_number = 0;
	m_job_pages = 0;

	m_pcl.set_page_callback(
			[this](const wtv_pcl3_renderer::page &page)
			{
				save_page(page);
			}
	);
	m_pcl.set_glyph_callback(
			[this](char32_t ch, double em, wtv_pcl3_renderer::glyph &g)
			{
				return font_glyph(ch, em, g);
			}
	);

	save_item(NAME(m_data));
	save_item(NAME(m_strobe_n));
	save_item(NAME(m_autofd_n));
	save_item(NAME(m_init_n));
	save_item(NAME(m_select_in_n));
}

void wtv_lpt_deskjet_device::device_reset()
{
	m_ack_start_timer->adjust(attotime::never);
	m_ack_end_timer->adjust(attotime::never);

	m_mode = MODE_COMPAT;
	m_strobe_seen = false;
	m_reverse_data.clear();
	m_reverse_pos = 0;

	compat_status();
}

void wtv_lpt_deskjet_device::device_stop()
{
	end_job();
}

// device ID

const char *wtv_lpt_deskjet_device::model_name() const
{
	if (PRINTER_IS_BW_PRINTER)
		return DEVICE_ID_MODEL_BW;

	return DEVICE_ID_MODEL_TWO;
}

const char *wtv_lpt_deskjet_device::pen_ids() const
{
	if (PRINTER_IS_BW_PRINTER)
		return DEVICE_ID_PENS_BW;

	return DEVICE_ID_PENS_TWO;
}

std::string wtv_lpt_deskjet_device::device_id() const
{
	std::string id = DEVICE_ID_HEAD;
	id += model_name();
	id += DEVICE_ID_MID;
	id += model_name();
	id += DEVICE_ID_VSTATUS;
	id += pen_ids();
	id += DEVICE_ID_TAIL;

	return id;
}

// glyphs for the renderer

void wtv_lpt_deskjet_device::render_font_cell(char32_t ch, float height, bitmap_argb32 &bitmap, rectangle &bounds)
{
	const int width = int(height * GLYPH_SCRATCH_WIDTH) + GLYPH_SCRATCH_MARGIN;
	const int rows = int(height * GLYPH_SCRATCH_HEIGHT) + GLYPH_SCRATCH_MARGIN;
	bitmap.allocate(width, rows);
	bitmap.fill(0);
	m_font->get_scaled_bitmap_and_bounds(bitmap, height, 1.0F, ch, bounds);
}

static uint8_t glyph_coverage(rgb_t pixel)
{
	const uint8_t brightest = std::max({ pixel.r(), pixel.g(), pixel.b() });
	return uint8_t(pixel.a() * brightest / 255);
}

void wtv_lpt_deskjet_device::measure_font_baseline()
{
	bitmap_argb32 bitmap;
	rectangle bounds;
	render_font_cell('H', BASELINE_REFERENCE_HEIGHT, bitmap, bounds);

	const int rows = std::min(bounds.height(), bitmap.height());
	const int columns = std::min(bounds.width(), bitmap.width());
	int bottom = -1;
	for (int y = 0; y < rows; y++)
	{
		for (int x = 0; x < columns; x++)
		{
			if (glyph_coverage(bitmap.pix(y, x)) > BASELINE_INK_THRESHOLD)
				bottom = y;
		}
	}

	if (bottom >= 0)
		m_font_baseline = float(bottom + 1) / BASELINE_REFERENCE_HEIGHT;
	else
		m_font_baseline = BASELINE_FALLBACK;
}

bool wtv_lpt_deskjet_device::font_glyph(char32_t ch, double em, wtv_pcl3_renderer::glyph &out)
{
	if (!m_font)
	{
		m_font = machine().render().font_alloc(machine().options().ui_font());
		m_font_baseline = -1.0F;
		m_glyphs.clear();
	}
	if (!m_font)
		return false;
	if (em < GLYPH_MIN_EM || em > GLYPH_MAX_EM)
		return false;

	const int size = int(std::lround(em));
	const auto key = std::make_pair(ch, size);
	const auto found = m_glyphs.find(key);
	if (found != m_glyphs.end())
	{
		out = found->second;
		return out.advance > 0;
	}

	if (m_font_baseline < 0)
		measure_font_baseline();

	const float height = float(size);
	bitmap_argb32 bitmap;
	rectangle bounds;
	render_font_cell(ch, height, bitmap, bounds);

	wtv_pcl3_renderer::glyph g;
	g.advance = m_font->char_width(height, 1.0F, ch);

	const bool has_size = bounds.width() > 0 && bounds.height() > 0;
	const bool fits = bounds.width() <= bitmap.width() && bounds.height() <= bitmap.height();
	if (has_size && fits)
	{
		g.width = bounds.width();
		g.height = bounds.height();
		g.left = bounds.left();
		g.top = -int32_t(std::lround(m_font_baseline * height));
		g.alpha.resize(size_t(g.width) * g.height);
		for (uint32_t y = 0; y < g.height; y++)
		{
			for (uint32_t x = 0; x < g.width; x++)
				g.alpha[size_t(y) * g.width + x] = glyph_coverage(bitmap.pix(y, x));
		}
	}

	if (m_glyphs.size() > GLYPH_CACHE_MAX)
		m_glyphs.clear();
	m_glyphs.emplace(key, g);
	out = std::move(g);

	return out.advance > 0;
}

// status lines

void wtv_lpt_deskjet_device::set_status(bool busy, bool ack_n, bool perror, bool select, bool fault_n)
{
	if (busy != m_busy)
	{
		m_busy = busy;
		output_busy(int(busy));
	}
	if (perror != m_perror)
	{
		m_perror = perror;
		output_perror(int(perror));
	}
	if (select != m_select)
	{
		m_select = select;
		output_select(int(select));
	}
	if (fault_n != m_fault_n)
	{
		m_fault_n = fault_n;
		output_fault(int(fault_n));
	}
	if (ack_n != m_ack_n)
	{
		m_ack_n = ack_n;
		output_ack(int(ack_n));
	}
}

void wtv_lpt_deskjet_device::compat_status()
{
	// Drive every line, the bus doesn't know our levels after a reset.
	m_busy = false;
	m_ack_n = true;
	m_perror = false;
	m_select = true;
	m_fault_n = true;

	output_busy(0);
	output_perror(0);
	output_select(1);
	output_fault(1);
	output_ack(1);
}

// cable side

void wtv_lpt_deskjet_device::change_data_bit(int bit, int state)
{
	if (state)
		m_data |= (1 << bit);
	else
		m_data &= ~(1 << bit);
}

void wtv_lpt_deskjet_device::input_init(int state)
{
	const bool init_n = state != 0;
	if (init_n == m_init_n)
		return;

	m_init_n = init_n;

	if (!init_n)
	{
		// nInit low resets the interface back to compatibility mode.
		m_ack_start_timer->adjust(attotime::never);
		m_ack_end_timer->adjust(attotime::never);
		m_mode = MODE_COMPAT;
		compat_status();
	}
}

void wtv_lpt_deskjet_device::input_strobe(int state)
{
	const bool strobe_n = state != 0;
	if (strobe_n == m_strobe_n)
		return;

	m_strobe_n = strobe_n;

	switch (m_mode)
	{
		case MODE_COMPAT:
		{
			// The byte is taken on the falling edge, like a real printer.
			const bool ready = !m_busy && m_init_n;
			if (!strobe_n && ready)
			{
				set_status(true, m_ack_n, m_perror, m_select, m_fault_n);
				take_byte(m_data);
				m_ack_start_timer->adjust(ACK_DELAY);
			}
			break;
		}

		case MODE_NEG_STROBE:
		{
			if (!strobe_n)
			{
				// Event 3: the extensibility byte is latched now.
				m_ext_request = m_data;
				m_strobe_seen = true;
				m_mode = MODE_NEG_RELEASE;
			}
			break;
		}

		case MODE_NEG_RELEASE:
			check_negotiation();
			break;

		default:
			break;
	}
}

void wtv_lpt_deskjet_device::input_autofd(int state)
{
	const bool autofd_n = state != 0;
	if (autofd_n == m_autofd_n)
		return;

	m_autofd_n = autofd_n;

	switch (m_mode)
	{
		case MODE_COMPAT:
		case MODE_NEG_RELEASE:
			check_negotiation();
			break;

		case MODE_REV_IDLE:
		{
			// Event 7: HostBusy low, the box wants a nibble
			if (!autofd_n)
				put_nibble();
			break;
		}

		case MODE_REV_NIBBLE:
		{
			// Event 10: the box has the nibble. Event 11: nAck high
			if (autofd_n)
			{
				m_mode = MODE_REV_IDLE;
				if (m_send_high_nibble)
				{
					set_status(m_busy, true, m_perror, m_select, m_fault_n);
				}
				else
				{
					// Byte done. nFault low while there is more to read (nDataAvail)
					const bool more = m_reverse_pos < m_reverse_data.size();
					set_status(false, true, false, true, !more);
				}
			}
			break;
		}

		case MODE_TERM_ACK:
		{
			// Event 25: back to compatibility status (event 26) and nAck high (event 27)
			if (!autofd_n)
			{
				m_mode = MODE_COMPAT;
				set_status(false, true, false, true, true);
			}
			break;
		}

		default:
			break;
	}
}

void wtv_lpt_deskjet_device::input_select_in(int state)
{
	const bool select_in_n = state != 0;
	if (select_in_n == m_select_in_n)
		return;

	m_select_in_n = select_in_n;

	switch (m_mode)
	{
		case MODE_COMPAT:
			check_negotiation();
			break;

		case MODE_NEG_STROBE:
		case MODE_NEG_RELEASE:
		case MODE_REV_IDLE:
		case MODE_REV_NIBBLE:
		case MODE_REJECTED:
		{
			// Event 22: nSelectIn low ends the mode.
			if (!select_in_n)
				terminate();
			break;
		}

		default:
			break;
	}
}

void wtv_lpt_deskjet_device::check_negotiation()
{
	if (m_mode == MODE_COMPAT)
	{
		// Event 1: nSelectIn high and nAutoFd low with nStrobe high asks for a 1284 mode.
		const bool host_request = m_select_in_n && !m_autofd_n && m_strobe_n;
		const bool idle = m_init_n && !m_busy;
		if (host_request && idle)
		{
			m_ext_request = m_data;
			m_strobe_seen = false;
			m_mode = MODE_NEG_STROBE;
			// Event 2: nAck low, PError high, nFault high, Select high.
			set_status(false, false, true, true, true);
		}
	}
	else if (m_mode == MODE_NEG_RELEASE)
	{
		// Event 4: nStrobe and nAutoFd both back high
		const bool released = m_strobe_n && m_autofd_n;
		if (m_strobe_seen && released)
			negotiation_done();
	}
}

void wtv_lpt_deskjet_device::negotiation_done()
{
	const bool nibble = (m_ext_request & EXT_MODE_MASK) == 0;
	const bool want_id = (m_ext_request & EXT_DEVICE_ID) != 0;

	if (!nibble)
	{
		// Only nibble mode (with or without the device ID) is offered
		m_mode = MODE_REJECTED;
		set_status(false, false, false, false, true);
		set_status(false, true, false, false, true);
	}
	else
	{
		m_reverse_data.clear();
		m_reverse_pos = 0;
		m_send_high_nibble = false;

		if (want_id)
		{
			const std::string id = device_id();
			const uint32_t length = id.size() + DEVICE_ID_LENGTH_BYTES;
			m_reverse_data.push_back(uint8_t(length >> 8));
			m_reverse_data.push_back(uint8_t(length));
			m_reverse_data.insert(m_reverse_data.end(), id.begin(), id.end());
		}

		m_mode = MODE_REV_IDLE;

		// Event 5: PError low, nFault low when there's data, Select = XFlag (low for plain nibble mode, high for a device ID request)
		// Event 6: nAck high
		const bool no_data = m_reverse_data.empty();
		set_status(false, false, false, want_id, no_data);
		set_status(false, true, false, want_id, no_data);
	}
}

void wtv_lpt_deskjet_device::put_nibble()
{
	uint8_t nibble = NIBBLE_NO_DATA;
	if (m_reverse_pos < m_reverse_data.size())
	{
		const uint8_t byte = m_reverse_data[m_reverse_pos];
		if (m_send_high_nibble)
		{
			nibble = byte >> 4;
			m_reverse_pos++;
		}
		else
		{
			nibble = byte & NIBBLE_MASK;
		}
	}
	m_send_high_nibble = !m_send_high_nibble;

	// Event 8: the nibble on nFault, Select, PError and Busy. Event 9: nAck low.
	const bool busy = BIT(nibble, NIBBLE_BIT_BUSY);
	const bool perror = BIT(nibble, NIBBLE_BIT_PERROR);
	const bool select = BIT(nibble, NIBBLE_BIT_SELECT);
	const bool fault_n = BIT(nibble, NIBBLE_BIT_FAULT);
	m_mode = MODE_REV_NIBBLE;
	set_status(busy, m_ack_n, perror, select, fault_n);
	set_status(busy, false, perror, select, fault_n);
}

void wtv_lpt_deskjet_device::terminate()
{
	// Events 23/24: Busy low, nFault high, then nAck low. Event 25 (nAutoFd low) finishes it.
	m_mode = MODE_TERM_ACK;
	m_reverse_data.clear();
	m_reverse_pos = 0;
	set_status(false, m_ack_n, m_perror, m_select, true);
	set_status(false, false, m_perror, m_select, true);
}

TIMER_CALLBACK_MEMBER(wtv_lpt_deskjet_device::ack_start)
{
	if (m_mode != MODE_COMPAT)
		return;

	set_status(m_busy, false, m_perror, m_select, m_fault_n);
	m_ack_end_timer->adjust(ACK_WIDTH);
}

TIMER_CALLBACK_MEMBER(wtv_lpt_deskjet_device::ack_end)
{
	if (m_mode != MODE_COMPAT)
		return;

	set_status(false, true, m_perror, m_select, m_fault_n);
}

// print side

void wtv_lpt_deskjet_device::take_byte(uint8_t data)
{
	if (m_job.empty())
	{
		m_job_number++;
		m_job_pages = 0;

		const std::time_t now = std::time(nullptr);
		char stamp[32];
		std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
		m_job_stamp = util::string_format("%s-%u", stamp, m_job_number);

		m_pcl.reset();
	}

	if (m_job.size() < MAX_JOB_BYTES)
		m_job.push_back(data);
	m_pcl.write(data);

	m_job_timer->adjust(JOB_IDLE);
}

TIMER_CALLBACK_MEMBER(wtv_lpt_deskjet_device::job_idle)
{
	end_job();
}

void wtv_lpt_deskjet_device::end_job()
{
	if (m_job.empty())
		return;

	m_job_timer->adjust(attotime::never);
	m_pcl.flush();

	emu_file file(machine().options().snapshot_directory(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);
	const std::error_condition err = file.open(util::string_format("wtvprinter" PATH_SEPARATOR "%s.pcl", m_job_stamp));
	if (!err)
		file.write(m_job.data(), m_job.size());

	m_job.clear();
}

void wtv_lpt_deskjet_device::save_page(const wtv_pcl3_renderer::page &page)
{
	m_job_pages++;

	bitmap_rgb32 bitmap(page.width, page.height);
	for (uint32_t y = 0; y < page.height; y++)
		std::copy_n(&page.argb[size_t(y) * page.width], page.width, &bitmap.pix(y));

	emu_file file(machine().options().snapshot_directory(), OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS);
	const std::error_condition err = file.open(util::string_format("wtvprinter" PATH_SEPARATOR "%s-%u.png", m_job_stamp, m_job_pages));
	if (err)
		return;

	const game_driver &system = machine().system();
	util::png_info pnginfo;
	pnginfo.add_text("System", util::string_format("%s %s", system.manufacturer, system.type.fullname()));
	pnginfo.add_text("Comment", util::string_format("HP %s, %ux%u dpi", model_name(), page.xres, page.yres));
	util::png_write_bitmap(file, &pnginfo, bitmap, 0, nullptr);
}

//////////////////
//////////////////
//////////////////

wtv_pcl3_renderer::wtv_pcl3_renderer()
{
	reset();
	m_pages_out = 0;
}

void wtv_pcl3_renderer::reset()
{
	m_parse_state = PS_TEXT;
	m_cmd_class = 0;
	m_cmd_group = 0;
	m_cmd_value.clear();
	m_uel_pjl = false;
	m_data_left = 0;
	m_data_class = 0;
	m_data_group = 0;
	m_data_letter = 0;
	m_data_continue = false;
	m_raster_resume = false;
	m_data.clear();

	m_res = DEFAULT_RESOLUTION;
	m_unit = DEFAULT_UNIT;
	m_page_width = LETTER_WIDTH;
	m_page_height = LETTER_HEIGHT;
	m_cur_x = 0;
	m_cur_y = 0;
	m_compression = COMPRESS_NONE;
	m_raster_on = false;
	m_raster_at_cursor = false;

	m_colorants.clear();
	configure_simple(1);
	m_row_y = 0;
	m_row_x = 0;

	reset_text();

	m_page = page();
	m_page_marked = false;
	m_page_alloc = false;
}

void wtv_pcl3_renderer::reset_text()
{
	// The DeskJet power-on font: Courier, 10 pitch, 12 point, Roman-8 (taken as Latin-1).
	m_symbol_set = SYM_LATIN1;
	m_font_fixed = true;
	m_font_pitch = DEFAULT_PITCH;
	m_font_points = DEFAULT_POINTS;
	m_font_italic = false;
	m_font_bold = false;
	m_lpi = DEFAULT_LPI;
	m_text_color = TEXT_BLACK;
}

void wtv_pcl3_renderer::flush()
{
	if (m_page_marked)
		eject_page();
}

uint32_t wtv_pcl3_renderer::max_xres() const
{
	uint32_t res = 0;
	for (const auto &col : m_colorants)
		res = std::max(res, col.xres);

	if (res == 0)
		return m_res;
	return res;
}

uint32_t wtv_pcl3_renderer::max_yres() const
{
	uint32_t res = 0;
	for (const auto &col : m_colorants)
		res = std::max(res, col.yres);

	if (res == 0)
		return m_res;
	return res;
}

uint32_t wtv_pcl3_renderer::to_dots(double units, uint32_t res) const
{
	return uint32_t(std::max(0.0, units * double(res) / m_unit));
}

// parser

static bool is_digit_or_sign(uint8_t c)
{
	const bool digit = c >= '0' && c <= '9';
	return digit || c == '+' || c == '-';
}

void wtv_pcl3_renderer::write(uint8_t c)
{
	switch (m_parse_state)
	{
		case PS_TEXT:
			text_state(c);
			break;

		case PS_PJL:
			if (c == CHAR_LF)
				m_parse_state = PS_TEXT; // more '@' lines may follow
			break;

		case PS_ESC:
			escape_state(c);
			break;

		case PS_PARAM_GROUP:
			group_state(c);
			break;

		case PS_PARAM_VALUE:
			value_state(c);
			break;

		case PS_DATA:
			data_state(c);
			break;
	}
}

void wtv_pcl3_renderer::text_state(uint8_t c)
{
	const bool resume = m_raster_resume && is_digit_or_sign(c);
	m_raster_resume = false;

	if (resume)
	{
		m_cmd_class = '*';
		m_cmd_group = 'b';
		m_cmd_value.clear();
		m_parse_state = PS_PARAM_VALUE;
		value_state(c);
	}
	else if (c == CHAR_ESC)
	{
		m_parse_state = PS_ESC;
	}
	else if (m_uel_pjl && c == '@')
	{
		m_parse_state = PS_PJL;
	}
	else if (c == CHAR_FF)
	{
		m_uel_pjl = false;
		eject_page();
	}
	else if (c >= CHAR_SPACE)
	{
		m_uel_pjl = false;
		text_byte(c, false);
	}
	else if (c == CHAR_CR || c == CHAR_LF)
	{
		text_byte(c, false);
	}
}

void wtv_pcl3_renderer::escape_state(uint8_t c)
{
	if (c >= CLASS_FIRST && c <= CLASS_LAST)
	{
		m_cmd_class = c;
		m_cmd_group = 0;
		m_cmd_value.clear();
		m_parse_state = PS_PARAM_GROUP;
	}
	else if (c >= TWO_CHAR_FIRST && c <= TWO_CHAR_LAST)
	{
		m_parse_state = PS_TEXT;
		two_char_command(c);
	}
	else
	{
		m_parse_state = PS_TEXT;
	}
}

void wtv_pcl3_renderer::group_state(uint8_t c)
{
	m_parse_state = PS_PARAM_VALUE;

	if (c >= GROUP_FIRST && c <= GROUP_LAST)
		m_cmd_group = c;
	else
		value_state(c); // no group: this is already the value
}

void wtv_pcl3_renderer::value_state(uint8_t c)
{
	if (is_digit_or_sign(c) || c == '.')
	{
		if (m_cmd_value.size() < MAX_VALUE_CHARS)
			m_cmd_value.push_back(char(c));
	}
	else if (c >= PARAMETER_FIRST && c <= PARAMETER_LAST)
	{
		finish_parameter(c);
	}
	else if (c == CHAR_ESC)
	{
		m_parse_state = PS_ESC; // broken sequence
	}
	else
	{
		m_parse_state = PS_TEXT;
	}
}

void wtv_pcl3_renderer::finish_parameter(uint8_t c)
{
	parameter p;
	p.cmd_class = m_cmd_class;
	p.group = m_cmd_group;
	p.last = c <= FINAL_PARAMETER_LAST;
	p.letter = c;
	if (!p.last)
		p.letter = uint8_t(c - LOWER_TO_UPPER);

	const bool sign_only = m_cmd_value == "+" || m_cmd_value == "-";
	p.has_value = !m_cmd_value.empty() && !sign_only;
	if (p.has_value)
	{
		p.value = std::strtod(m_cmd_value.c_str(), nullptr);
		p.relative = m_cmd_value[0] == '+' || m_cmd_value[0] == '-';
	}
	p.ivalue = int(p.value);

	m_parse_state = PS_PARAM_VALUE;
	if (p.last)
		m_parse_state = PS_TEXT;
	m_cmd_value.clear();

	parameter_command(p);

	const bool raster_sequence = m_cmd_class == '*' && m_cmd_group == 'b';
	if (p.last && m_parse_state == PS_TEXT && raster_sequence)
		m_raster_resume = true;
}

void wtv_pcl3_renderer::data_state(uint8_t c)
{
	if (m_data.size() < MAX_DATA)
		m_data.push_back(c);

	m_data_left--;
	if (m_data_left == 0)
		data_done();
}

void wtv_pcl3_renderer::two_char_command(uint8_t c)
{
	if (c != 'E')
		return;

	// ESC E: printer reset
	m_uel_pjl = false;
	end_raster();
	if (m_page_marked)
		eject_page();

	m_compression = COMPRESS_NONE;
	m_res = DEFAULT_RESOLUTION;
	m_unit = DEFAULT_UNIT;
	m_cur_x = 0;
	m_cur_y = 0;
	m_colorants.clear();

	configure_simple(1);

	reset_text();
}

bool wtv_pcl3_renderer::is_data_command(const parameter &p) const
{
	if (p.letter == 'W')
	{
		// ESC &k#W (DeskJet text scale) is the one W command without data.
		const bool text_scale = p.cmd_class == '&' && p.group == 'k';
		return !text_scale;
	}

	const bool raster_plane = p.cmd_class == '*' && p.group == 'b' && p.letter == 'V';
	const bool transparent_print = p.cmd_class == '&' && p.group == 'p' && p.letter == 'X';
	return raster_plane || transparent_print;
}

void wtv_pcl3_renderer::parameter_command(const parameter &p)
{
	if (p.cmd_class == '%')
	{
		if (p.letter == 'X' && p.ivalue == UEL_VALUE)
		{
			end_raster();
			m_uel_pjl = true;
		}
	}
	else if (is_data_command(p))
	{
		start_data(p);
	}
	else
	{
		switch (p.cmd_class)
		{
			case '*':
				star_command(p);
				break;

			case '&':
				ampersand_command(p);
				break;

			case '(':
				font_command(p);
				break;

			default:
				break;
		}
	}
}

void wtv_pcl3_renderer::star_command(const parameter &p)
{
	switch (p.group)
	{
		case 't':
			// ESC *t#R: resolution. A plain ESC *r#U setup follows it.
			if (p.letter == 'R' && p.ivalue > 0 && uint32_t(p.ivalue) <= MAX_RESOLUTION)
			{
				m_res = uint32_t(p.ivalue);
				for (auto &col : m_colorants)
				{
					col.xres = m_res;
					col.yres = m_res;
				}
			}
			break;

		case 'r':
			raster_command(p);
			break;

		case 'b':
			raster_row_command(p);
			break;

		case 'p':
			cursor_command(p);
			break;

		case 'v':
			if (p.letter == 'S')
				m_text_color = uint32_t(std::max(p.ivalue, 0));
			break;

		default:
			break;
	}
}

void wtv_pcl3_renderer::raster_command(const parameter &p)
{
	switch (p.letter)
	{
		case 'U':
			if (p.has_value)
				configure_simple(p.ivalue);
			else
				configure_simple(1);
			break;

		case 'A':
			start_raster(p.ivalue);
			break;

		case 'B':
		case 'C':
			end_raster();
			break;

		default:
			break; // S width and T height: rows are taken as they come
	}
}

void wtv_pcl3_renderer::raster_row_command(const parameter &p)
{
	switch (p.letter)
	{
		case 'M':
			m_compression = uint32_t(std::max(p.ivalue, 0));
			break;

		case 'Y':
			if (!m_raster_on)
				start_raster(RASTER_AT_CURSOR);
			if (p.has_value && p.value > 0)
				m_row_y += uint32_t(p.value) * row_height();
			for (auto &seed : m_seed)
				std::fill(seed.begin(), seed.end(), 0);
			break;

		default:
			break;
	}
}

void wtv_pcl3_renderer::cursor_command(const parameter &p)
{
	if (p.letter == 'X')
	{
		double x = p.value;
		if (p.relative)
			x += m_cur_x;
		m_cur_x = std::max(0.0, x);
	}
	else if (p.letter == 'Y')
	{
		double y = p.value;
		if (p.relative)
			y += m_cur_y;
		m_cur_y = std::max(0.0, y);

		// HP's APDK drivers stay in raster mode and place each row with ESC *p#Y, skipping blank rows, so the next row goes where the cursor now is.
		if (m_raster_on)
		{
			m_row_y = to_dots(m_cur_y, max_yres());
			m_plane = 0;
		}
	}
}

void wtv_pcl3_renderer::ampersand_command(const parameter &p)
{
	if (p.group == 'u')
	{
		// ESC &u#D: unit of measure
		if (p.letter == 'D' && p.ivalue > 0 && p.ivalue <= MAX_UNIT)
			m_unit = uint32_t(p.ivalue);
	}
	else if (p.group == 'l')
	{
		page_command(p);
	}
}

void wtv_pcl3_renderer::page_command(const parameter &p)
{
	switch (p.letter)
	{
		case 'A':
			switch (p.ivalue)
			{
				case PAGE_EXECUTIVE:
					m_page_width = EXECUTIVE_WIDTH;
					m_page_height = EXECUTIVE_HEIGHT;
					break;

				case PAGE_LETTER:
					m_page_width = LETTER_WIDTH;
					m_page_height = LETTER_HEIGHT;
					break;

				case PAGE_LEGAL:
					m_page_width = LEGAL_WIDTH;
					m_page_height = LEGAL_HEIGHT;
					break;

				case PAGE_A4:
					m_page_width = A4_WIDTH;
					m_page_height = A4_HEIGHT;
					break;

				default:
					break;
			}
			break;

		case 'H':
			// ESC &l0H ejects the page
			if (p.has_value && p.ivalue == 0)
				eject_page();
			break;

		case 'D':
			if (p.value > 0 && p.value <= MAX_LPI)
				m_lpi = p.value;
			break;

		default:
			break;
	}
}

void wtv_pcl3_renderer::font_command(const parameter &p)
{
	if (p.group == 0)
	{
		// ESC (#X symbol set: the value then a letter (0N Latin-1, 10U PC-8)
		if (p.letter == 'U' && p.ivalue == SYMBOL_SET_PC8)
			m_symbol_set = SYM_PC8;
		else
			m_symbol_set = SYM_LATIN1;
	}
	else if (p.group != 's')
	{
		return;
	}
	else
	{
		switch (p.letter)
		{
			case 'P':
				m_font_fixed = p.ivalue == 0;
				break;

			case 'H':
			{
				if (p.value > 0)
					m_font_pitch = p.value;
				break;
			}

			case 'V':
			{
				if (p.value > 0 && p.value <= MAX_POINTS)
					m_font_points = p.value;
				break;
			}

			case 'S':
				m_font_italic = (p.ivalue & 1) != 0;
				break;

			case 'B':
				m_font_bold = p.ivalue > 0;
				break;

			default:
				break; // T typeface, Q quality
		}
	}
}

void wtv_pcl3_renderer::start_data(const parameter &p)
{
	m_data.clear();
	m_data_class = m_cmd_class;
	m_data_group = m_cmd_group;
	m_data_letter = p.letter;
	m_data_continue = !p.last;

	m_data_left = 0;
	if (p.has_value && p.value > 0)
		m_data_left = uint32_t(std::min(p.value, MAX_DATA_COUNT));

	if (m_data_left == 0)
		data_done();
	else
		m_parse_state = PS_DATA;
}

void wtv_pcl3_renderer::data_done()
{
	m_parse_state = PS_TEXT;
	if (m_data_continue)
		m_parse_state = PS_PARAM_VALUE;
	m_cmd_class = m_data_class;
	m_cmd_group = m_data_group;

	const bool raster_data = m_data_class == '*' && m_data_group == 'b';
	const bool crd_data = m_data_class == '*' && m_data_group == 'g' && m_data_letter == 'W';
	const bool transparent_print = m_data_class == '&' && m_data_group == 'p' && m_data_letter == 'X';

	if (raster_data && (m_data_letter == 'W' || m_data_letter == 'V'))
	{
		transfer_plane(m_data, m_data_letter == 'W');
		m_raster_resume = !m_data_continue;
	}
	else if (crd_data)
	{
		configure_crd(m_data);
	}
	else if (transparent_print)
	{
		// Every byte is a character, control codes included.
		for (uint8_t c : m_data)
			text_byte(c, true);
	}

	m_data.clear();
}

// raster setup

void wtv_pcl3_renderer::configure_simple(int planes)
{
	std::vector<colorant> order;
	switch (planes)
	{
		case -4:
			order = { INK_BLACK, INK_CYAN, INK_MAGENTA, INK_YELLOW };
			break;

		case -3:
		case 3:
			order = { INK_CYAN, INK_MAGENTA, INK_YELLOW }; // RGB (3) is taken as CMY; HP drivers send -3/-4
			break;

		default:
			order = { INK_BLACK };
			break;
	}

	m_colorants.clear();
	for (colorant which : order)
	{
		colorant_info col;
		col.which = which;
		col.xres = m_res;
		col.yres = m_res;
		col.levels = 2;
		col.bits = 1;
		m_colorants.push_back(col);
	}

	m_total_planes = uint32_t(m_colorants.size());
	m_seed.assign(m_total_planes, {});
	m_row.assign(m_total_planes, {});
	m_plane = 0;
}

static uint32_t read_be16(const uint8_t *p)
{
	return (uint32_t(p[0]) << 8) | p[1];
}

void wtv_pcl3_renderer::configure_crd(const std::vector<uint8_t> &crd)
{
	if (crd.size() < CRD_HEADER_BYTES || crd[0] != CRD_FORMAT)
		return;

	const uint32_t count = crd[1];
	std::vector<colorant> order;
	switch (count)
	{
		case 1:
			order = { INK_BLACK };
			break;

		case 3:
			order = { INK_CYAN, INK_MAGENTA, INK_YELLOW };
			break;

		case 4:
			order = { INK_BLACK, INK_CYAN, INK_MAGENTA, INK_YELLOW };
			break;

		default:
			return;
	}

	if (crd.size() < CRD_HEADER_BYTES + count * CRD_COMPONENT_BYTES)
		return;

	std::vector<colorant_info> cols;
	for (uint32_t i = 0; i < count; i++)
	{
		const uint8_t *component = &crd[CRD_HEADER_BYTES + i * CRD_COMPONENT_BYTES];
		colorant_info col;
		col.which = order[i];
		col.xres = read_be16(&component[0]);
		col.yres = read_be16(&component[2]);
		col.levels = read_be16(&component[4]);

		const bool xres_ok = col.xres != 0 && col.xres <= MAX_RESOLUTION;
		const bool yres_ok = col.yres != 0 && col.yres <= MAX_RESOLUTION;
		const bool levels_ok = col.levels >= 2 && col.levels <= MAX_LEVELS;
		if (!xres_ok || !yres_ok || !levels_ok)
			return;

		col.bits = 0;
		while ((1U << col.bits) < col.levels)
			col.bits++;
		cols.push_back(col);
	}

	uint32_t min_yres = cols[0].yres;
	for (const auto &col : cols)
		min_yres = std::min(min_yres, col.yres);

	uint32_t planes = 0;
	for (auto &col : cols)
	{
		if (col.yres % min_yres)
			return;
		col.rows = col.yres / min_yres;
		planes += col.bits * col.rows;
	}

	if (planes > MAX_PLANES)
		return;

	m_colorants = std::move(cols);
	m_total_planes = planes;
	m_seed.assign(m_total_planes, {});
	m_row.assign(m_total_planes, {});
	m_plane = 0;
}

void wtv_pcl3_renderer::start_raster(int mode)
{
	ensure_page();
	m_raster_on = true;
	m_raster_at_cursor = mode == RASTER_AT_CURSOR;
	m_plane = 0;
	for (auto &seed : m_seed)
		seed.clear();

	m_row_y = to_dots(m_cur_y, max_yres());
	m_row_x = 0;
	if (m_raster_at_cursor)
		m_row_x = to_dots(m_cur_x, max_xres());
}

void wtv_pcl3_renderer::end_raster()
{
	if (!m_raster_on)
		return;

	// The cursor ends up under the last raster row.
	m_raster_on = false;
	m_cur_y = double(m_row_y) * m_unit / double(max_yres());
}

// rows

static uint32_t read_extended(const std::vector<uint8_t> &in, size_t &pos, uint32_t value, uint8_t more)
{
	// A value continued by extension bytes: each byte adds to it, 255 means another follows.
	while (pos < in.size())
	{
		const uint8_t extra = in[pos++];
		value += extra;
		if (extra != more)
			break;
	}

	return value;
}

void wtv_pcl3_renderer::decode_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed)
{
	const size_t size = in.size();
	std::vector<uint8_t> out;
	size_t pos = 0;

	switch (m_compression)
	{
		case COMPRESS_NONE:
			seed = in;
			break;

		case COMPRESS_RUN_LENGTH:
		{
			// pairs of (repeat count - 1, byte)
			while (pos + 1 < size)
			{
				const uint32_t count = uint32_t(in[pos]) + 1;
				out.insert(out.end(), count, in[pos + 1]);
				pos += 2;
			}
			seed = out;
			break;
		}

		case COMPRESS_PACKBITS:
		{
			while (pos < size)
			{
				const int8_t control = int8_t(in[pos++]);
				if (control >= 0)
				{
					// control + 1 literal bytes
					const size_t count = std::min<size_t>(size_t(control) + 1, size - pos);
					out.insert(out.end(), in.begin() + pos, in.begin() + pos + count);
					pos += count;
				}
				else if (control != -128 && pos < size)
				{
					// the next byte 1 - control times; -128 is a no-op
					out.insert(out.end(), size_t(1 - control), in[pos]);
					pos++;
				}
			}
			seed = out;
			break;
		}

		case COMPRESS_DELTA_ROW:
			decode_delta_row(in, seed);
			break;

		case COMPRESS_REPLACEMENT_DELTA:
			decode_replacement_delta_row(in, seed);
			break;

		default:
			seed.clear();
			break;
	}
}

void wtv_pcl3_renderer::decode_delta_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed)
{
	// Command byte: replacement count - 1 in the top 3 bits, offset in the low 5.
	size_t pos = 0;
	size_t column = 0;
	while (pos < in.size())
	{
		const uint8_t command = in[pos++];
		const uint32_t count = (command >> DELTA_COUNT_SHIFT) + 1;
		uint32_t offset = command & DELTA_OFFSET_MASK;
		if (offset == DELTA_OFFSET_EXTEND)
			offset = read_extended(in, pos, offset, EXTEND_MORE);

		column += offset;
		for (uint32_t k = 0; k < count && pos < in.size(); k++)
		{
			if (column >= seed.size())
				seed.resize(column + 1, 0);
			seed[column] = in[pos++];
			column++;
		}
	}
}

void wtv_pcl3_renderer::decode_replacement_delta_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed)
{
	// Command byte, top bit set: a run (offset in bits 5-6, count - 2 in bits 0-4, then the byte)
	// Clear: literal bytes (offset in bits 3-6, count - 1 in bits 0-2). The largest offset or count is continued by extension bytes.
	size_t pos = 0;
	size_t column = 0;
	while (pos < in.size())
	{
		const uint8_t command = in[pos++];
		const bool run = (command & REPLACE_RUN_FLAG) != 0;

		uint32_t offset;
		uint32_t count;
		uint32_t offset_max;
		uint32_t count_max;
		if (run)
		{
			offset = (command >> REPLACE_RUN_OFFSET_SHIFT) & REPLACE_RUN_OFFSET_MASK;
			count = command & REPLACE_RUN_COUNT_MASK;
			offset_max = REPLACE_RUN_OFFSET_MASK;
			count_max = REPLACE_RUN_COUNT_MASK;
		}
		else
		{
			offset = (command >> REPLACE_LITERAL_OFFSET_SHIFT) & REPLACE_LITERAL_OFFSET_MASK;
			count = command & REPLACE_LITERAL_COUNT_MASK;
			offset_max = REPLACE_LITERAL_OFFSET_MASK;
			count_max = REPLACE_LITERAL_COUNT_MASK;
		}

		if (offset == offset_max)
			offset = read_extended(in, pos, offset, EXTEND_MORE);
		if (count == count_max)
			count = read_extended(in, pos, count, EXTEND_MORE);

		column += offset;
		if (run)
		{
			if (pos >= in.size())
				break;

			const uint8_t value = in[pos++];
			count += REPLACE_RUN_MIN;
			if (column + count > seed.size())
				seed.resize(column + count, 0);
			std::fill(seed.begin() + column, seed.begin() + column + count, value);
			column += count;
		}
		else
		{
			count += REPLACE_LITERAL_MIN;
			for (uint32_t k = 0; k < count && pos < in.size(); k++)
			{
				if (column >= seed.size())
					seed.resize(column + 1, 0);
				seed[column] = in[pos++];
				column++;
			}
		}
	}
}

void wtv_pcl3_renderer::transfer_plane(const std::vector<uint8_t> &data, bool last)
{
	if (!m_raster_on)
		start_raster(0);

	if (m_plane < m_total_planes)
	{
		decode_row(data, m_seed[m_plane]);
		m_row[m_plane] = m_seed[m_plane];
		m_plane++;
	}

	if (!last)
		return;

	// Planes not sent this row are zero, and their seed rows are cleared.
	for (uint32_t plane = m_plane; plane < m_total_planes; plane++)
	{
		m_seed[plane].clear();
		m_row[plane].clear();
	}
	put_row();
	m_plane = 0;
}

void wtv_pcl3_renderer::ensure_page()
{
	if (m_page_alloc)
		return;

	const uint32_t xres = max_xres();
	const uint32_t yres = max_yres();
	const uint32_t width = uint32_t(std::lround(m_page_width * xres));
	const uint32_t height = uint32_t(std::lround(m_page_height * yres));

	m_page.xres = xres;
	m_page.yres = yres;
	m_page.width = std::min<uint32_t>(width, MAX_PAGE_DOTS);
	m_page.height = std::min<uint32_t>(height, MAX_PAGE_DOTS);
	m_page.argb.assign(size_t(m_page.width) * m_page.height, 0xffffffff);
	m_page_alloc = true;
}

bool wtv_pcl3_renderer::row_has_ink() const
{
	for (const auto &plane : m_row)
	{
		const bool any = std::any_of(
				plane.begin(),
				plane.end(),
				[](uint8_t b)
				{
					return b != 0;
				}
		);

		if (any)
			return true;
	}

	return false;
}

uint32_t wtv_pcl3_renderer::pixel_level(uint32_t first_plane, uint32_t bits, size_t px) const
{
	const size_t byte = px >> 3;
	const int shift = 7 - int(px & 7);
	uint32_t level = 0;
	for (uint32_t b = 0; b < bits; b++)
	{
		const auto &plane = m_row[first_plane + b];
		if (byte < plane.size() && ((plane[byte] >> shift) & 1))
			level |= 1U << b;
	}
	return level;
}

void wtv_pcl3_renderer::draw_colorant_row(const colorant_info &col, uint32_t first_plane, uint32_t row_y)
{
	// Each pixel covers page.xres/xres dots across and page.yres/yres rows.
	const uint32_t x_scale = std::max<uint32_t>(1, m_page.xres / col.xres);
	const uint32_t y_scale = std::max<uint32_t>(1, m_page.yres / col.yres);
	const uint32_t max_level = col.levels - 1;

	size_t row_bytes = 0;
	for (uint32_t b = 0; b < col.bits; b++)
		row_bytes = std::max(row_bytes, m_row[first_plane + b].size());

	for (size_t px = 0; px < row_bytes * 8; px++)
	{
		const uint32_t level = std::min(pixel_level(first_plane, col.bits, px), max_level);
		if (level == 0)
			continue;

		const uint32_t ink = (level * MAX_INK + max_level / 2) / max_level;
		const uint32_t x0 = m_row_x + uint32_t(px) * x_scale;
		for (uint32_t dy = 0; dy < y_scale; dy++)
		{
			for (uint32_t dx = 0; dx < x_scale; dx++)
				ink_dot(x0 + dx, row_y + dy, col.which, ink);
		}
	}
}

void wtv_pcl3_renderer::put_row()
{
	ensure_page();

	if (row_has_ink() && m_row_y < m_page.height)
	{
		uint32_t plane = 0;
		for (const auto &col : m_colorants)
		{
			const uint32_t y_scale = std::max<uint32_t>(1, m_page.yres / col.yres);
			for (uint32_t sub = 0; sub < col.rows; sub++)
			{
				draw_colorant_row(col, plane, m_row_y + sub * y_scale);
				plane += col.bits;
			}
		}
		m_page_marked = true;
	}

	m_row_y += row_height();
}

uint32_t wtv_pcl3_renderer::row_height() const
{
	uint32_t min_yres = 0;
	for (const auto &col : m_colorants)
	{
		if (min_yres == 0 || col.yres < min_yres)
			min_yres = col.yres;
	}

	if (min_yres == 0)
		return 1;
	return std::max<uint32_t>(1, max_yres() / min_yres);
}

void wtv_pcl3_renderer::eject_page()
{
	end_raster();

	if (m_page_marked)
	{
		if (m_page_cb)
			m_page_cb(m_page);
		m_pages_out++;
	}

	m_page = page();
	m_page_alloc = false;
	m_page_marked = false;
	m_cur_x = 0;
	m_cur_y = 0;
	m_row_y = 0;
}

void wtv_pcl3_renderer::ink_dot(uint32_t x, uint32_t y, colorant which, uint32_t ink)
{
	if (x >= m_page.width || y >= m_page.height || ink == 0)
		return;

	// Inks are subtractive: each takes away its share of red, green or blue.
	uint32_t &dot = m_page.argb[size_t(y) * m_page.width + x];
	uint32_t red = (dot >> 16) & 0xff;
	uint32_t green = (dot >> 8) & 0xff;
	uint32_t blue = dot & 0xff;
	const uint32_t keep = MAX_INK - std::min(ink, MAX_INK);
	switch (which)
	{
		case INK_BLACK:
			red = red * keep / MAX_INK;
			green = green * keep / MAX_INK;
			blue = blue * keep / MAX_INK;
			break;

		case INK_CYAN:
			red = red * keep / MAX_INK;
			break;

		case INK_MAGENTA:
			green = green * keep / MAX_INK;
			break;

		case INK_YELLOW:
			blue = blue * keep / MAX_INK;
			break;
	}
	dot = 0xff000000 | (red << 16) | (green << 8) | blue;
}

// text

char32_t wtv_pcl3_renderer::map_char(uint8_t c) const
{
	// Code Page 437 (PC-8) Control Characters (0x00 - 0x1f)
	static constexpr char16_t PC8_CONTROL[0x20] = {
		// 0x00 - 0x0f (faces, card suits, bullets and circles, gender signs, notes, sun)
		0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
		0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
		// 0x10 - 0x1f (pointers, arrows, double exclamation, paragraph, section)
		0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
		0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc
	};
	// Code Page 437 (PC-8) High Characters (0x80 - 0xff)
	static constexpr char16_t PC8_HIGH[0x80] = {
		// 0x80 - 0x8f (accented letters)
		0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
		0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
		// 0x90 - 0x9f (accented letters, currency: cent, pound, yen, peseta, florin)
		0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
		0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
		// 0xa0 - 0xaf (accented letters, ordinals, inverted punctuation, fractions, guillemets)
		0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
		0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
		// 0xb0 - 0xbf (shading and early box-drawing elements)
		0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
		0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
		// 0xc0 - 0xcf (box-drawing elements)
		0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
		0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
		// 0xd0 - 0xdf (box-drawing elements and block graphics)
		0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
		0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
		// 0xe0 - 0xef (greek letters and math symbols)
		0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
		0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
		// 0xf0 - 0xff (math symbols, degree, superscripts, square, non-breaking space)
		0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
		0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
	};
	static constexpr uint8_t PC8_HIGH_FIRST = 0x80;

	if (m_symbol_set != SYM_PC8)
		return c;

	if (c < CHAR_SPACE)
		return PC8_CONTROL[c];
	if (c == CHAR_DEL)
		return PC8_HOUSE;
	if (c >= PC8_HIGH_FIRST)
		return PC8_HIGH[c - PC8_HIGH_FIRST];
	return c;
}

void wtv_pcl3_renderer::text_byte(uint8_t c, bool transparent)
{
	if (!transparent && c == CHAR_CR)
	{
		m_cur_x = 0;
	}
	else if (!transparent && c == CHAR_LF)
	{
		m_cur_x = 0;
		m_cur_y += double(m_unit) / m_lpi;
	}
	else
	{
		if (!m_glyph_cb)
			return;

		// Control codes only have glyphs in PC-8.
		const char32_t ch = map_char(c);
		if (ch == 0)
			return;
		if (ch < CHAR_SPACE && m_symbol_set != SYM_PC8)
			return;

		ensure_page();
		const double xres = double(m_page.xres);
		const double yres = double(m_page.yres);
		const double em = m_font_points * yres / POINTS_PER_INCH;
		const double pen_x = m_cur_x * xres / m_unit;
		const double base_y = m_cur_y * yres / m_unit;

		double advance; // in dots
		if (ch == BOX_HORIZONTAL || ch == BOX_DOUBLE_HORIZONTAL)
		{
			// Box-drawing rules, drawn edge to edge so a run of them makes a line.
			advance = em * RULE_ADVANCE;
			if (m_font_fixed)
				advance = xres / m_font_pitch;

			draw_rule(pen_x, base_y, em, advance);
			if (ch == BOX_DOUBLE_HORIZONTAL)
				draw_rule(pen_x, base_y - em * DOUBLE_RULE_GAP, em, advance);
		}
		else
		{
			glyph g;
			const bool have = m_glyph_cb(ch, em, g);
			const bool has_bitmap = g.width && g.height && g.alpha.size() >= size_t(g.width) * g.height;
			const bool blank = ch == ' ' || ch == CHAR_NBSP;
			if (have && has_bitmap && !blank)
				draw_glyph(g, pen_x, base_y, em);

			advance = em * MISSING_GLYPH_ADVANCE;
			if (have)
				advance = g.advance;
			if (m_font_fixed)
				advance = xres / m_font_pitch;
		}

		m_cur_x += advance * m_unit / xres;
	}
}

void wtv_pcl3_renderer::ink_text_dot(uint32_t x, uint32_t y, uint32_t alpha)
{
	// Text color: the simple color (CMY) palette, 7 black. Anything else is black.
	const bool black = m_text_color == 0 || m_text_color >= TEXT_BLACK;
	if (black)
	{
		ink_dot(x, y, INK_BLACK, alpha);
	}
	else
	{
		if (m_text_color & TEXT_CYAN)
			ink_dot(x, y, INK_CYAN, alpha);
		if (m_text_color & TEXT_MAGENTA)
			ink_dot(x, y, INK_MAGENTA, alpha);
		if (m_text_color & TEXT_YELLOW)
			ink_dot(x, y, INK_YELLOW, alpha);
	}
}

void wtv_pcl3_renderer::draw_glyph(const glyph &g, double pen_x, double base_y, double em)
{
	// Bold is smeared to the right, italic is sheared.
	int bold_dots = 0;
	if (m_font_bold)
		bold_dots = std::max(1, int(std::lround(em / BOLD_SMEAR_DIVISOR)));

	double slant = 0.0;
	if (m_font_italic)
		slant = ITALIC_SLANT;

	bool any = false;
	for (uint32_t gy = 0; gy < g.height; gy++)
	{
		const double y = base_y + g.top + double(gy);
		if (y < 0)
			continue;

		const int shear = int(std::lround(slant * (base_y - y)));
		for (uint32_t gx = 0; gx < g.width; gx++)
		{
			const uint32_t alpha = g.alpha[size_t(gy) * g.width + gx];
			if (alpha == 0)
				continue;

			for (int smear = 0; smear <= bold_dots; smear++)
			{
				const double x = pen_x + g.left + double(gx) + shear + smear;
				if (x >= 0)
					ink_text_dot(uint32_t(x), uint32_t(y), alpha);
			}
			any = true;
		}
	}

	if (any)
		m_page_marked = true;
}

void wtv_pcl3_renderer::draw_rule(double pen_x, double base_y, double em, double advance)
{
	const double thickness = std::max(1.0, std::round(em / RULE_THICKNESS_DIVISOR));
	const double top = std::round(base_y - em * RULE_RAISE);
	const double left = std::round(pen_x);
	const double right = std::round(pen_x + advance);
	for (double y = top; y < top + thickness; y++)
	{
		for (double x = left; x < right; x++)
		{
			if (x >= 0 && y >= 0)
				ink_dot(uint32_t(x), uint32_t(y), INK_BLACK, MAX_INK);
		}
	}
	m_page_marked = true;
}
