// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_WTVLPTPRINTER_H
#define MAME_WEBTV_WTVLPTPRINTER_H

#pragma once

#include "bus/centronics/ctronics.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class render_font;

class wtv_pcl3_renderer
{
public:
	struct page
	{
		uint32_t              width  = 0;
		uint32_t              height = 0;
		uint32_t              xres   = 0; // dots per inch
		uint32_t              yres   = 0;
		std::vector<uint32_t> argb; // width * height, 0xffRRGGBB
	};

	// A character drawn at a font size of em dots (the font height, in page dots).
	struct glyph
	{
		uint32_t             width   = 0; // bitmap size
		uint32_t             height  = 0;
		int32_t              left    = 0; // bitmap left edge from the pen position
		int32_t              top     = 0; // bitmap top edge from the baseline (negative is above it)
		double               advance = 0; // pen movement, in dots
		std::vector<uint8_t> alpha;       // width * height coverage, 255 = ink
	};

	using page_func  = std::function<void(const page &)>;
	using glyph_func = std::function<bool(char32_t ch, double em, glyph &out)>;

	wtv_pcl3_renderer();

	void set_page_callback(page_func cb)
	{
		m_page_cb = std::move(cb);
	}
	void set_glyph_callback(glyph_func cb)
	{
		m_glyph_cb = std::move(cb);
	}

	void reset();
	void write(uint8_t data);
	void flush(); // end of job: emit a page that has marks on it

	uint32_t pages_out() const
	{
		return m_pages_out;
	}

private:

	static constexpr uint8_t CHAR_LF    = 0x0a;
	static constexpr uint8_t CHAR_FF    = 0x0c;
	static constexpr uint8_t CHAR_CR    = 0x0d;
	static constexpr uint8_t CHAR_ESC   = 0x1b;
	static constexpr uint8_t CHAR_SPACE = 0x20;
	static constexpr uint8_t CHAR_DEL   = 0x7f;
	static constexpr uint8_t CHAR_NBSP  = 0xa0;

	static constexpr uint8_t CLASS_FIRST          = 0x21; // '!' to '/' start a parameterised command
	static constexpr uint8_t CLASS_LAST           = 0x2f;
	static constexpr uint8_t TWO_CHAR_FIRST       = 0x30; // '0' to '~' make a two-character command
	static constexpr uint8_t TWO_CHAR_LAST        = 0x7e;
	static constexpr uint8_t GROUP_FIRST          = 0x60; // '`' to '~' name a group
	static constexpr uint8_t GROUP_LAST           = 0x7e;
	static constexpr uint8_t PARAMETER_FIRST      = 0x40; // '@' to '~' end a value
	static constexpr uint8_t PARAMETER_LAST       = 0x7e;
	static constexpr uint8_t FINAL_PARAMETER_LAST = 0x5e; // upper case ends the sequence
	static constexpr uint8_t LOWER_TO_UPPER       = 0x20;
	static constexpr size_t  MAX_VALUE_CHARS      = 32;
	static constexpr int     UEL_VALUE            = -12345; // ESC %-12345X

	static constexpr uint32_t MAX_PLANES     = 16;
	static constexpr uint32_t MAX_DATA       = 0x10000;
	static constexpr double   MAX_DATA_COUNT = MAX_DATA * 16;
	static constexpr uint32_t MAX_PAGE_DOTS  = 1200 * 17; // bounds a hostile width/height
	static constexpr uint32_t MAX_RESOLUTION = 1200;
	static constexpr int      MAX_UNIT       = 7200;
	static constexpr double   MAX_LPI        = 48;
	static constexpr double   MAX_POINTS     = 999;
	static constexpr uint32_t MAX_LEVELS     = 256;
	static constexpr uint32_t MAX_INK        = 255;

	static constexpr uint32_t DEFAULT_RESOLUTION = 75;
	static constexpr uint32_t DEFAULT_UNIT       = 300;
	static constexpr int      RASTER_AT_CURSOR   = 1; // ESC *r1A

	static constexpr int    PAGE_EXECUTIVE   = 1;
	static constexpr int    PAGE_LETTER      = 2;
	static constexpr int    PAGE_LEGAL       = 3;
	static constexpr int    PAGE_A4          = 26;
	static constexpr double EXECUTIVE_WIDTH  = 7.25;
	static constexpr double EXECUTIVE_HEIGHT = 10.5;
	static constexpr double LETTER_WIDTH     = 8.5;
	static constexpr double LETTER_HEIGHT    = 11.0;
	static constexpr double LEGAL_WIDTH      = 8.5;
	static constexpr double LEGAL_HEIGHT     = 14.0;
	static constexpr double A4_WIDTH         = 8.27;
	static constexpr double A4_HEIGHT        = 11.69;

	static constexpr uint8_t CRD_FORMAT          = 2;
	static constexpr size_t  CRD_HEADER_BYTES    = 2; // format, component count
	static constexpr size_t  CRD_COMPONENT_BYTES = 6; // hres, vres, levels (16 bits each)

	static constexpr uint32_t COMPRESS_NONE              = 0;
	static constexpr uint32_t COMPRESS_RUN_LENGTH        = 1;
	static constexpr uint32_t COMPRESS_PACKBITS          = 2;
	static constexpr uint32_t COMPRESS_DELTA_ROW         = 3;
	static constexpr uint32_t COMPRESS_REPLACEMENT_DELTA = 9;

	static constexpr uint8_t  EXTEND_MORE                  = 255; // an extension byte of 255 continues
	static constexpr uint32_t DELTA_COUNT_SHIFT            = 5;
	static constexpr uint32_t DELTA_OFFSET_MASK            = 0x1f;
	static constexpr uint32_t DELTA_OFFSET_EXTEND          = 31;
	static constexpr uint8_t  REPLACE_RUN_FLAG             = 0x80;
	static constexpr uint32_t REPLACE_RUN_OFFSET_SHIFT     = 5;
	static constexpr uint32_t REPLACE_RUN_OFFSET_MASK      = 0x03;
	static constexpr uint32_t REPLACE_RUN_COUNT_MASK       = 0x1f;
	static constexpr uint32_t REPLACE_RUN_MIN              = 2;
	static constexpr uint32_t REPLACE_LITERAL_OFFSET_SHIFT = 3;
	static constexpr uint32_t REPLACE_LITERAL_OFFSET_MASK  = 0x0f;
	static constexpr uint32_t REPLACE_LITERAL_COUNT_MASK   = 0x07;
	static constexpr uint32_t REPLACE_LITERAL_MIN          = 1;

	static constexpr double   POINTS_PER_INCH        = 72.0;
	static constexpr int      SYMBOL_SET_PC8         = 10;   // ESC (10U
	static constexpr double   DEFAULT_PITCH          = 10.0; // Courier 10 pitch, 12 point, 6 lpi
	static constexpr double   DEFAULT_POINTS         = 12.0;
	static constexpr double   DEFAULT_LPI            = 6.0;
	static constexpr uint32_t TEXT_CYAN              = 1; // ESC *v#S, the CMY palette
	static constexpr uint32_t TEXT_MAGENTA           = 2;
	static constexpr uint32_t TEXT_YELLOW            = 4;
	static constexpr uint32_t TEXT_BLACK             = 7;
	static constexpr char32_t PC8_HOUSE              = 0x2302; // PC-8 0x7f
	static constexpr char32_t BOX_HORIZONTAL         = 0x2500; // PC-8 0xc4
	static constexpr char32_t BOX_DOUBLE_HORIZONTAL  = 0x2550; // PC-8 0xcd
	static constexpr double   MISSING_GLYPH_ADVANCE  = 0.5;    // in ems
	static constexpr double   RULE_ADVANCE           = 0.6;    // box-drawing rule width, in ems
	static constexpr double   RULE_RAISE             = 0.3;    // rule height above the baseline, in ems
	static constexpr double   RULE_THICKNESS_DIVISOR = 16.0;   // rule thickness = em / this
	static constexpr double   DOUBLE_RULE_GAP        = 0.08;   // in ems
	static constexpr double   BOLD_SMEAR_DIVISOR     = 28.0;   // extra stroke width = em / this
	static constexpr double   ITALIC_SLANT           = 0.2;    // x shift per dot above the baseline

	enum parse_state : uint8_t
	{
		PS_TEXT,
		PS_ESC,
		PS_PARAM_GROUP, // after ESC and a class char, waiting for the group char
		PS_PARAM_VALUE, // inside a value
		PS_DATA,        // swallowing the data bytes of a command
		PS_PJL          // inside a PJL line
	};

	enum colorant : uint8_t
	{
		INK_BLACK = 0,
		INK_CYAN,
		INK_MAGENTA,
		INK_YELLOW
	};

	enum symbol_set : uint8_t
	{
		SYM_LATIN1 = 0,
		SYM_PC8
	};

	struct colorant_info
	{
		colorant which = INK_BLACK;
		uint32_t xres = 300;
		uint32_t yres = 300;
		uint32_t levels = 2;
		uint32_t bits = 1; // planes per row of this colorant
		uint32_t rows = 1; // its rows per raster row (yres / lowest yres)
	};

	// one parameter of an escape sequence, e.g. "1p" of ESC (s1p8V
	struct parameter
	{
		uint8_t cmd_class = 0; // ESC <class>: '*', '&', '(', '%', ...
		uint8_t group = 0; // lower-case group letter, 0 for none
		uint8_t letter = 0; // parameter letter, upper case
		bool last = false; // it ended the sequence
		bool has_value = false;
		bool relative = false; // the value had a sign
		double value = 0;
		int ivalue = 0;
	};

	page_func m_page_cb;
	glyph_func m_glyph_cb;

	// parser state
	parse_state m_parse_state;
	uint8_t m_cmd_class; // of the escape sequence being parsed
	uint8_t m_cmd_group;
	std::string m_cmd_value;
	bool m_uel_pjl; // just saw a UEL: '@' lines that follow are PJL
	uint32_t m_data_left;
	uint8_t m_data_class; // the command whose data bytes are coming in
	uint8_t m_data_group;
	uint8_t m_data_letter;
	bool m_data_continue; // data came from a combined sequence that carries on
	bool m_raster_resume; // a digit right after an ESC *b sequence continues it
	std::vector<uint8_t> m_data;

	// job/page state
	uint32_t m_res; // ESC *t#R
	uint32_t m_unit; // ESC &u#D
	double m_page_width; // inches
	double m_page_height;
	double m_cur_x; // cursor, at m_unit
	double m_cur_y;
	uint32_t m_compression;
	bool m_raster_on;
	bool m_raster_at_cursor;

	std::vector<colorant_info> m_colorants;
	uint32_t m_total_planes;
	uint32_t m_plane; // next plane within the row
	std::vector<std::vector<uint8_t>> m_seed; // per plane
	std::vector<std::vector<uint8_t>> m_row; // per plane, the row being built
	uint32_t m_row_y; // raster row, at max_yres(), from the page top
	uint32_t m_row_x; // raster left edge, at max_xres()

	// text state
	symbol_set m_symbol_set;
	bool m_font_fixed; // ESC (s0P
	double m_font_pitch; // ESC (s#H, characters per inch
	double m_font_points; // ESC (s#V
	bool m_font_italic; // ESC (s#S
	bool m_font_bold; // ESC (s#B
	double m_lpi; // ESC &l#D
	uint32_t m_text_color; // ESC *v#S

	page m_page;
	bool m_page_marked;
	bool m_page_alloc;
	uint32_t m_pages_out;

	void text_state(uint8_t c);
	void escape_state(uint8_t c);
	void group_state(uint8_t c);
	void value_state(uint8_t c);
	void data_state(uint8_t c);
	void finish_parameter(uint8_t c);
	void two_char_command(uint8_t c);
	void parameter_command(const parameter &p);
	void star_command(const parameter &p);
	void ampersand_command(const parameter &p);
	void font_command(const parameter &p);
	void page_command(const parameter &p);
	void raster_command(const parameter &p);
	void raster_row_command(const parameter &p);
	void cursor_command(const parameter &p);
	bool is_data_command(const parameter &p) const;
	void start_data(const parameter &p);
	void data_done();

	void configure_simple(int planes);
	void configure_crd(const std::vector<uint8_t> &crd);
	void start_raster(int mode);
	void end_raster();
	void transfer_plane(const std::vector<uint8_t> &data, bool last);
	void decode_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed);
	void decode_delta_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed);
	void decode_replacement_delta_row(const std::vector<uint8_t> &in, std::vector<uint8_t> &seed);
	bool row_has_ink() const;
	uint32_t pixel_level(uint32_t first_plane, uint32_t bits, size_t px) const;
	void draw_colorant_row(const colorant_info &col, uint32_t first_plane, uint32_t row_y);
	void put_row();
	void ensure_page();
	void eject_page();
	void ink_dot(uint32_t x, uint32_t y, colorant which, uint32_t ink);
	uint32_t to_dots(double units, uint32_t res) const;
	uint32_t max_xres() const;
	uint32_t max_yres() const;
	uint32_t row_height() const;

	void reset_text();
	void text_byte(uint8_t c, bool transparent);
	char32_t map_char(uint8_t c) const;
	void draw_glyph(const glyph &g, double pen_x, double base_y, double em);
	void ink_text_dot(uint32_t x, uint32_t y, uint32_t alpha);
	void draw_rule(double pen_x, double base_y, double em, double advance);

};

class wtv_lpt_deskjet_device : public device_t, public device_centronics_peripheral_interface
{

public:
	wtv_lpt_deskjet_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	virtual ~wtv_lpt_deskjet_device();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

	virtual void input_strobe(int state) override;
	virtual void input_data0(int state) override
	{
		change_data_bit(0, state);
	}
	virtual void input_data1(int state) override
	{
		change_data_bit(1, state);
	}
	virtual void input_data2(int state) override
	{
		change_data_bit(2, state);
	}
	virtual void input_data3(int state) override
	{
		change_data_bit(3, state);
	}
	virtual void input_data4(int state) override
	{
		change_data_bit(4, state);
	}
	virtual void input_data5(int state) override
	{
		change_data_bit(5, state);
	}
	virtual void input_data6(int state) override
	{
		change_data_bit(6, state);
	}
	virtual void input_data7(int state) override
	{
		change_data_bit(7, state);
	}
	virtual void input_autofd(int state) override;
	virtual void input_init(int state) override;
	virtual void input_select_in(int state) override;

private:

	// true: a DeskJet 600 with its one black cartridge
	// false: a DeskJet 660C with black and color cartridges
	static constexpr bool PRINTER_IS_BW_PRINTER = false;

	static constexpr char DEVICE_ID_MODEL_BW[]  = "DESKJET 600";
	static constexpr char DEVICE_ID_MODEL_TWO[] = "DESKJET 660C";
	static constexpr char DEVICE_ID_PENS_BW[]   = "$CB0";
	static constexpr char DEVICE_ID_PENS_TWO[]  = "$CB0$RB0";

	// MFG:...MDL:<model>;CMD:PCL;CLS:PRINTER;DES:<model>;VSTATUS:<pens><tail>
	static constexpr char DEVICE_ID_HEAD[]    = "MFG:HEWLETT-PACKARD;MDL:";
	static constexpr char DEVICE_ID_MID[]     = ";CMD:PCL;CLS:PRINTER;DES:";
	static constexpr char DEVICE_ID_VSTATUS[] = ";VSTATUS:";
	static constexpr char DEVICE_ID_TAIL[]    = ",ff,DN,IDLE,CUT,K0,C0,DP,NR,KP000,CP027;";

	// The device ID starts with its own length, big endian, counting these two bytes.
	static constexpr uint32_t DEVICE_ID_LENGTH_BYTES = 2;

	// Compatibility handshake. The box's PPutChar gives nAck 40 us to fall and 10 us to rise.
	static constexpr attotime ACK_DELAY = attotime::from_usec(2);
	static constexpr attotime ACK_WIDTH = attotime::from_usec(3);

	// A job ends when the box has sent nothing for this long.
	static constexpr attotime JOB_IDLE      = attotime::from_seconds(5);
	static constexpr size_t   MAX_JOB_BYTES = 0x4000000;

	// IEEE 1284 extensibility request bits
	static constexpr uint8_t EXT_DEVICE_ID = 0x04;
	static constexpr uint8_t EXT_MODE_MASK = 0x7b; // everything but the device ID bit; 0 = nibble

	// nibble mode: which status line carries each nibble bit
	static constexpr int     NIBBLE_BIT_FAULT  = 0;
	static constexpr int     NIBBLE_BIT_SELECT = 1;
	static constexpr int     NIBBLE_BIT_PERROR = 2;
	static constexpr int     NIBBLE_BIT_BUSY   = 3;
	static constexpr uint8_t NIBBLE_MASK       = 0x0f;
	static constexpr uint8_t NIBBLE_NO_DATA    = 0x0f;

	static constexpr uint8_t DATA_LINES_IDLE = 0xff;

	// glyphs from the UI font
	static constexpr double  GLYPH_MIN_EM              = 1.0;
	static constexpr double  GLYPH_MAX_EM              = 2000.0;
	static constexpr size_t  GLYPH_CACHE_MAX           = 4096;
	static constexpr float   BASELINE_REFERENCE_HEIGHT = 200.0F;
	static constexpr float   BASELINE_FALLBACK         = 0.8F; // of the cell height
	static constexpr uint8_t BASELINE_INK_THRESHOLD    = 128;
	static constexpr int     GLYPH_SCRATCH_WIDTH       = 3; // scratch bitmap, in cell heights
	static constexpr int     GLYPH_SCRATCH_HEIGHT      = 2;
	static constexpr int     GLYPH_SCRATCH_MARGIN      = 8;

	enum lpt_mode : uint8_t
	{
		MODE_COMPAT = 0,  // forward, compatibility mode
		MODE_NEG_STROBE,  // negotiation: event 2 done, waiting for nStrobe low (event 3)
		MODE_NEG_RELEASE, // waiting for nStrobe and nAutoFd high (event 4)
		MODE_REV_IDLE,    // nibble mode, between nibbles
		MODE_REV_NIBBLE,  // nibble on the lines, nAck low, waiting for nAutoFd high (event 10)
		MODE_REJECTED,    // mode refused, waiting for termination
		MODE_TERM_ACK     // termination: nAck low, waiting for nAutoFd low (event 25)
	};

	uint8_t m_mode;
	uint8_t m_data; // levels the box drives on D0-D7
	bool m_strobe_n;
	bool m_autofd_n;
	bool m_init_n;
	bool m_select_in_n;
	bool m_strobe_seen; // negotiation: nStrobe went low

	uint8_t m_ext_request; // extensibility byte of the current negotiation
	std::vector<uint8_t> m_reverse_data; // bytes for the box in nibble mode
	uint32_t m_reverse_pos;
	bool m_send_high_nibble; // the next nibble is the high half of the byte

	// what is on the status lines
	bool m_busy;
	bool m_ack_n;
	bool m_perror;
	bool m_select;
	bool m_fault_n;

	wtv_pcl3_renderer m_pcl;
	std::unique_ptr<render_font> m_font; // text in resident fonts is drawn with the UI font
	float m_font_baseline; // baseline from the cell top, per unit of height
	std::map<std::pair<char32_t, int>, wtv_pcl3_renderer::glyph> m_glyphs;

	std::vector<uint8_t> m_job;
	uint32_t m_job_number;
	uint32_t m_job_pages;
	std::string m_job_stamp;

	const char *model_name() const;
	const char *pen_ids() const;
	std::string device_id() const;

	bool font_glyph(char32_t ch, double em, wtv_pcl3_renderer::glyph &out);
	void render_font_cell(char32_t ch, float height, bitmap_argb32 &bitmap, rectangle &bounds);
	void measure_font_baseline();

	void change_data_bit(int bit, int state);
	void set_status(bool busy, bool ack_n, bool perror, bool select, bool fault_n);
	void compat_status();
	void check_negotiation();
	void negotiation_done();
	void put_nibble();
	void terminate();

	void take_byte(uint8_t data);
	void end_job();
	void save_page(const wtv_pcl3_renderer::page &page);

	emu_timer *m_ack_start_timer;
	TIMER_CALLBACK_MEMBER(ack_start);
	emu_timer *m_ack_end_timer;
	TIMER_CALLBACK_MEMBER(ack_end);
	emu_timer *m_job_timer;
	TIMER_CALLBACK_MEMBER(job_idle);

};

DECLARE_DEVICE_TYPE(WTV_LPT_DESKJET, wtv_lpt_deskjet_device)

#endif // MAME_WEBTV_WTVLPTPRINTER_H
