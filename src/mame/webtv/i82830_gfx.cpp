// license: BSD-3-Clause
// copyright-holders: wtvemac

// Can also be in src/devices/machine/

// Description here

#include "emu.h"
#include "i82830_gfx.h"
#include "screen.h"

DEFINE_DEVICE_TYPE(I82830_CGC, i82830_graphics_device, "i82830_graphics", "Intel 830M Chipset 82830 Integrated Graphics Controller")

i82830_graphics_device::i82830_graphics_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: agp_device(mconfig, I82830_CGC, tag, owner, clock),
	m_mcu(*this, finder_base::DUMMY_TAG),
	m_svga(*this, "svga"),
	m_pirq_w_cb(*this),
	m_gpio_r_cb(*this, 0),
	m_gpio_w_cb(*this)
{
	intr_pin = i82801_lpc_device::INT_PIN_A;
	m_pirq_pin = i82801_lpc_device::PIRQ_SELECT_A;
}

void i82830_graphics_device::device_start()
{
	agp_device::device_start();

	m_pins_execute_timer = timer_alloc(FUNC(i82830_graphics_device::pins_execute), this);
}

void i82830_graphics_device::device_reset()
{
	agp_device::device_reset();

	status = 0x0090;

	m_gmadr = i82830_graphics_device::GFX_MEM_TYPE_ANY | i82830_graphics_device::GFX_MEM_PREFETCHABLE;
	m_mmadr = i82830_graphics_device::GFX_MEM_TYPE_ANY;

	m_miscc = 0x00000000;
	m_scram = 0x00000000;
	m_coreclk = 0x00000000;

	std::fill(std::begin(m_mm_block), std::end(m_mm_block), 0);

	i82830_graphics_device::pins_execute_stop();
}

void i82830_graphics_device::device_add_mconfig(machine_config &config)
{
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_raw(XTAL(25'174'800), 640, 0, 640, 480, 0, 480);
	screen.screen_vblank().set(FUNC(i82830_graphics_device::vblank_irq));
	screen.set_screen_update(FUNC(i82830_graphics_device::screen_update));

	I82830_VGA(config, m_svga, 0);
	m_svga->set_screen("screen");
	m_svga->set_vram_size(i82830_svga_device::VGA_MEM_SIZE);
}

void i82830_graphics_device::map_extra(uint64_t memory_window_start, uint64_t memory_window_end, uint64_t memory_offset, address_space *memory_space,
									uint64_t io_window_start, uint64_t io_window_end, uint64_t io_offset, address_space *io_space)
{
	m_max_physical_address = m_mcu->get_ram_size();

	//uint16_t gcc0 = m_mcu->get_gcc0();
	uint16_t gcc1 = m_mcu->get_gcc1();

	if(!(gcc1 & i82830_host_device::GMCH_CNTL1_IGD_OFF))
	{
		uint32_t mmadr = (m_mmadr & i82830_graphics_device::GFX_MMADR_MEM_BASE_MASK);
		if(mmadr > 0)
			memory_space->install_device(mmadr, mmadr + (i82830_graphics_device::MM_SIZE - 1), *this, &i82830_graphics_device::mm_map);

		uint32_t gmadr_mask = 
			(gcc1 & i82830_host_device::GMCH_CNTL1_IGD_GM_64MB)
			? i82830_graphics_device::GFX_GMADR_64MEM_BASE_MASK
			: i82830_graphics_device::GFX_GMADR_128MEM_BASE_MASK;

		uint32_t gmadr = (m_gmadr & gmadr_mask);
		if(gmadr > 0)
			memory_space->install_device(gmadr, gmadr + ((70 * 1024 * 1024) - 1), *this, &i82830_graphics_device::gm_map);

		if(!(gcc1 & i82830_host_device::GMCH_CNTL1_IGD_VGA_OFF))
		{
			io_space->install_device(i82830_svga_device::VGA_IO_BASE, i82830_svga_device::VGA_IO_BASE + (i82830_svga_device::VGA_IO_SIZE - 1), *this, &i82830_graphics_device::vga_io_map);
			memory_space->install_device(i82830_svga_device::VGA_MEM_BASE, i82830_svga_device::VGA_MEM_BASE + (i82830_svga_device::VGA_MEM_SIZE - 1), *this, &i82830_graphics_device::vga_mem_map);

			pclass = (pclass & (~0xff00)) | (0x00 << 0x08); // VGA Compatible Controller
		}
		else
		{
			pclass = (pclass & (~0xff00)) | (0x80 << 0x08); // Other Display Controller
		}

	}
}

void i82830_graphics_device::config_map(address_map &map)
{
	agp_device::config_map(map);

	map(0x10, 0x13).rw(FUNC(i82830_graphics_device::gmadr_r), FUNC(i82830_graphics_device::gmadr_w));
	map(0x14, 0x17).rw(FUNC(i82830_graphics_device::mmadr_r), FUNC(i82830_graphics_device::mmadr_w));

	map(0x2c, 0x2d).w(FUNC(i82830_graphics_device::subvendor_w));
	map(0x2e, 0x2f).w(FUNC(i82830_graphics_device::subsystem_w));

	map(0x50, 0x53).rw(FUNC(i82830_graphics_device::xxx32_r), FUNC(i82830_graphics_device::xxx32_w));
	map(0x52, 0x53).r(FUNC(i82830_graphics_device::coreclk_r));

	// sdram
	map(0x70, 0x71).rw(FUNC(i82830_graphics_device::xxx16_r), FUNC(i82830_graphics_device::xxx16_w));
	// miscc
	map(0x72, 0x73).rw(FUNC(i82830_graphics_device::xxx16_r), FUNC(i82830_graphics_device::xxx16_w));

	map(0xb4, 0xb7).rw(FUNC(i82830_graphics_device::xxx32_r), FUNC(i82830_graphics_device::xxx32_w));
	map(0xc2, 0xc3).rw(FUNC(i82830_graphics_device::xxx16_r), FUNC(i82830_graphics_device::xxx16_w));
}

void i82830_graphics_device::mm_map(address_map &map)
{
	map(0x00000000, (i82830_graphics_device::MM_SIZE - 1)).rw(FUNC(i82830_graphics_device::mm_block_r), FUNC(i82830_graphics_device::mm_block_w));
}

void i82830_graphics_device::vga_io_map(address_map &map)
{
	map(0x0000, (i82830_svga_device::VGA_IO_SIZE - 1)).m(m_svga, FUNC(i82830_svga_device::io_map));
}

void i82830_graphics_device::vga_mem_map(address_map &map)
{
	map(0x00000000, (i82830_svga_device::VGA_MEM_SIZE - 1)).m(m_svga, FUNC(i82830_svga_device::mem_map));
}

void i82830_graphics_device::gm_map(address_map &map)
{
	map(0x00000000, ((70 * 1024 * 1024) - 1)).rw(FUNC(i82830_graphics_device::gm_r), FUNC(i82830_graphics_device::gm_w));
}

uint32_t i82830_graphics_device::gtt_lookup(uint32_t graphics_offset)
{
	uint32_t page_table_offset = graphics_offset >> i82830_graphics_device::MM_GTT_PAGE_ADDR32_SHIFT;
	uint32_t page_table_index = i82830_graphics_device::MM_GTT_PAGE_TABLE + page_table_offset;

	uint32_t page_table_entry = m_mm_block[page_table_index & (i82830_graphics_device::MM_GTT_PAGE_TABLE_SIZE - 1)];

	if((page_table_entry & i82830_graphics_device::MM_GTT_PAGE_TABLE_VALID) & 0x01 && page_table_entry < m_max_physical_address)
		return (page_table_entry >> 2) + (graphics_offset & i82830_graphics_device::MM_GTT_PAGE_ADDR32_OMASK);
	else
		return i82830_graphics_device::MM_GTT_PAGE_INVALID;
}

uint32_t i82830_graphics_device::graphics_memory_read(uint32_t graphics_offset)
{
	uint32_t* ram = m_mcu->get_ram_pointer();

	uint32_t physical_address = i82830_graphics_device::gtt_lookup(graphics_offset);

	if(physical_address != i82830_graphics_device::MM_GTT_PAGE_INVALID)
		return ram[physical_address];
	else
		return 0x00000000;
}

void i82830_graphics_device::graphics_memory_write(uint32_t graphics_offset, uint32_t data)
{
	uint32_t* ram = m_mcu->get_ram_pointer();

	uint32_t physical_address = i82830_graphics_device::gtt_lookup(graphics_offset);

	if(physical_address != i82830_graphics_device::MM_GTT_PAGE_INVALID)
		ram[physical_address] = data;
}

uint32_t i82830_graphics_device::gm_r(offs_t offset)
{
	return i82830_graphics_device::graphics_memory_read(offset);
}

void i82830_graphics_device::gm_w(offs_t offset, uint32_t data)
{
	i82830_graphics_device::graphics_memory_write(offset, data);
}

TIMER_CALLBACK_MEMBER(i82830_graphics_device::pins_execute)
{
	if(i82830_graphics_device::execute_instruction_buffer(i82830_graphics_device::MM_CNTL_PRINGBUF))
		i82830_graphics_device::pins_execute_stop();
	else
		i82830_graphics_device::pins_execute_next();
}

void i82830_graphics_device::pins_execute_next()
{
	if(m_mm_block[i82830_graphics_device::MM_CNTL_PRINGBUF_CNTL] & i82830_graphics_device::MM_CNTL_RINGBUF_CNTL_EN)
	{
		m_pins_exc_state = ins_execute_state_t::EXC_ACTIVE;

		m_pins_execute_timer->enable(true);
		m_pins_execute_timer->adjust(i82830_graphics_device::PINS_EXECUTE_RATE);
	}
	else
	{
		i82830_graphics_device::pins_execute_stop();
	}
}

void i82830_graphics_device::pins_execute_stop()
{
	m_pins_exc_state = ins_execute_state_t::EXC_IDLE;

	m_pins_execute_timer->enable(false);
	m_pins_execute_timer->reset();
}

bool i82830_graphics_device::execute_instruction_buffer(uint32_t mm_ringbuf_index)
{
	ins_parser_state_t parser_state = {
		.start_addr = m_mm_block[mm_ringbuf_index + i82830_graphics_device::MM_CNTL_RINGBUF_START_OFFSET],
		.cur_addr  = 0x00000000,
		.size       = i82830_graphics_device::MM_CNTL_RINGBUF_MAX_SIZE,
		.head       = m_mm_block[mm_ringbuf_index + i82830_graphics_device::MM_CNTL_RINGBUF_HEAD_OFFSET],
		.tail       = m_mm_block[mm_ringbuf_index + i82830_graphics_device::MM_CNTL_RINGBUF_TAIL_OFFSET]
	};

	bool finished = true;

	if(parser_state.tail != parser_state.head)
	{
		parser_state.cur_addr = parser_state.start_addr + parser_state.head;

		uint32_t instruction = i82830_graphics_device::instruction_buffer_shift(&parser_state);

		switch(instruction & i82830_graphics_device::MI_TYPE_MASK)
		{
			case MI_TYPE_PS:
				i82830_graphics_device::execute_ps_instruction(instruction, &parser_state);
				break;

			case MI_TYPE_2D:
				i82830_graphics_device::execute_2d_instruction(instruction, &parser_state);
				break;

			case MI_TYPE_3D:
				i82830_graphics_device::execute_3d_instruction(instruction, &parser_state);
				break;

			default:
				logerror("Unknown graphics instruction: %08x\n", instruction);
				machine().debug_break();
				break;
		}

		m_mm_block[mm_ringbuf_index + i82830_graphics_device::MM_CNTL_RINGBUF_HEAD_OFFSET] = parser_state.head;

		finished = (parser_state.tail == parser_state.head);
	}

	return finished;
}

uint32_t i82830_graphics_device::instruction_buffer_shift(ins_parser_state_t* parser_state)
{
	uint32_t data = i82830_graphics_device::graphics_memory_read(parser_state->cur_addr >> 2);

	parser_state->head += i82830_graphics_device::MM_CNTL_RINGBUF_INS_SIZE;
	parser_state->head &= (parser_state->size - 1);

	parser_state->cur_addr = parser_state->start_addr + parser_state->head;

	return data;
}

void i82830_graphics_device::instruction_buffer_skip(ins_parser_state_t* parser_state, uint32_t skip_amount)
{
	parser_state->head += (i82830_graphics_device::MM_CNTL_RINGBUF_INS_SIZE * skip_amount);
	parser_state->head &= (parser_state->size - 1);

	parser_state->cur_addr = parser_state->start_addr + parser_state->head;
}

bool i82830_graphics_device::execute_ps_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{

	switch(instruction & i82830_graphics_device::MI_PS_SUBTYPE_MASK)
	{
		case MI_PS_CMD_NOP_IDENTIFICATION:
			// EMAC(NOTE): not implemented
			return true;

		case MI_PS_CMD_WAIT_FOR_EVENT:
			// EMAC(NOTE): not implemented
			return true;

		case MI_PS_CMD_FLUSH:
			// EMAC(NOTE): not implemented
			return true;

		case MI_PS_CMD_LOAD_SCAN_LINES_INCL:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 1);
			return true;

		case MI_PS_CMD_SET_CONTEXT:
			i82830_graphics_device::instruction_buffer_skip(parser_state, 1);
			return true;

		case MI_PS_CMD_STORE_DWORD_IMM:
		{
			uint32_t address = i82830_graphics_device::instruction_buffer_shift(parser_state);
			uint32_t data = i82830_graphics_device::instruction_buffer_shift(parser_state);
			i82830_graphics_device::graphics_memory_write((0x04000000 + address) >> 2, data);
			return true;
		}

		case MI_PS_CMD_LOAD_REGISTER_IMM:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 2);
			return true;

		default:
			logerror("Unknown parser graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

bool i82830_graphics_device::execute_2d_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{
	switch(instruction & i82830_graphics_device::MI_2D_SUBTYPE_MASK)
	{
		case MI_2D_CMD_XY_COLOR_BLT:
			i82830_graphics_device::xy_color_blit(parser_state);
			return true;

		case MI_2D_CMD_XY_SRC_COPY_BLT:
			i82830_graphics_device::xy_copy_blit(parser_state);
			return true;

		default:
			logerror("Unknown 2d graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

void i82830_graphics_device::xy_color_blit(ins_parser_state_t* parser_state)
{
	uint32_t br13_props    = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br22_dst_trxy = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br23_dst_brxy = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br09_dst_base = i82830_graphics_device::instruction_buffer_shift(parser_state) >> 2;
	uint32_t br16_color    = i82830_graphics_device::instruction_buffer_shift(parser_state);

	uint32_t dst_x_start = br22_dst_trxy & 0xffff;
	uint32_t dst_y_start = (br22_dst_trxy >> 0x10) & 0xffff;
	uint32_t dst_y_end = ((br23_dst_brxy >> 0x10) & 0xffff);
	uint32_t dst_stride = (br13_props & 0xffff) >> 2;

	uint32_t x_size = (br23_dst_brxy & 0xffff) - dst_x_start;

	uint32_t dst_stride_overflow = dst_stride - x_size;

	uint32_t dst_virtual_addr = br09_dst_base + (dst_y_start * dst_stride) + dst_x_start;
	uint32_t dst_physical_addr = 0x00000000;

	uint32_t* ram = m_mcu->get_ram_pointer();

	for(uint32_t dst_y = dst_y_start; dst_y < dst_y_end; dst_y++)
	{
		bool physical_addr_dirty = true;

		uint32_t pixels_copied = 0;
		while(true)
		{
			if(physical_addr_dirty)
			{
				dst_physical_addr = i82830_graphics_device::gtt_lookup(
					dst_virtual_addr
				);

				if(dst_physical_addr == i82830_graphics_device::MM_GTT_PAGE_INVALID)
					break;

				physical_addr_dirty = false;
			}

			// Try to copy all pixels as long as we don't reach the page boundry (in this case we'd need to copy up to the page boundry then to another page lookup for the next page)
			uint32_t dst_page_offset = dst_virtual_addr & i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE_MASK;
			uint32_t pixels_to_copy = std::min({
				(i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE - dst_page_offset), 
				(x_size - pixels_copied)
			});

			std::fill(&ram[dst_physical_addr], &ram[dst_physical_addr + pixels_to_copy], br16_color);

			dst_virtual_addr  += pixels_to_copy;
			dst_physical_addr += pixels_to_copy;

			pixels_copied     += pixels_to_copy;

			if(pixels_copied < x_size)
				physical_addr_dirty = true;
			else
				break;
		}

		dst_virtual_addr += dst_stride_overflow;
	}
}

void i82830_graphics_device::xy_copy_blit(ins_parser_state_t* parser_state)
{
	// Assuming this will always be a 32-bit color copy and clipping disabled

	uint32_t br13_props    = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br22_dst_trxy = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br23_dst_brxy = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br09_dst_base = i82830_graphics_device::instruction_buffer_shift(parser_state) >> 2;
	uint32_t br26_src_trxy = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br11_src_strd = i82830_graphics_device::instruction_buffer_shift(parser_state);
	uint32_t br12_src_base = i82830_graphics_device::instruction_buffer_shift(parser_state) >> 2;

	uint32_t src_x_start = br26_src_trxy & 0xffff;
	uint32_t src_y_start = (br26_src_trxy >> 0x10) & 0xffff;
	uint32_t src_stride = (br11_src_strd & 0xffff) >> 2;

	uint32_t dst_x_start = br22_dst_trxy & 0xffff;
	uint32_t dst_y_start = (br22_dst_trxy >> 0x10) & 0xffff;
	uint32_t dst_y_end = ((br23_dst_brxy >> 0x10) & 0xffff);
	uint32_t dst_stride = (br13_props & 0xffff) >> 2;

	uint32_t x_size = (br23_dst_brxy & 0xffff) - dst_x_start;

	uint32_t src_stride_overflow = src_stride - x_size;
	uint32_t dst_stride_overflow = dst_stride - x_size;

	uint32_t src_virtual_addr = br12_src_base + (src_y_start * src_stride) + src_x_start;
	uint32_t src_physical_addr = 0x00000000;
	uint32_t dst_virtual_addr = br09_dst_base + (dst_y_start * dst_stride) + dst_x_start;
	uint32_t dst_physical_addr = 0x00000000;

	uint32_t* ram = m_mcu->get_ram_pointer();

	for(uint32_t dst_y = dst_y_start, src_y = src_y_start; dst_y < dst_y_end; dst_y++, src_y++)
	{
		bool physical_addr_dirty = true;

		uint32_t pixels_copied = 0;
		while(true)
		{
			if(physical_addr_dirty)
			{
				src_physical_addr = i82830_graphics_device::gtt_lookup(
					src_virtual_addr
				);
				dst_physical_addr = i82830_graphics_device::gtt_lookup(
					dst_virtual_addr
				);

				if(src_physical_addr == i82830_graphics_device::MM_GTT_PAGE_INVALID || dst_physical_addr == i82830_graphics_device::MM_GTT_PAGE_INVALID)
					break;

				physical_addr_dirty = false;
			}

			// Try to copy all pixels as long as we don't reach the page boundry (in this case we'd need to copy up to the page boundry then to another page lookup for the next page)
			uint32_t src_page_offset = src_virtual_addr & i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE_MASK;
			uint32_t dst_page_offset = dst_virtual_addr & i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE_MASK;
			uint32_t pixels_to_copy = std::min({
				(i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE - src_page_offset), 
				(i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE - dst_page_offset), 
				(x_size - pixels_copied)
			});

			memcpy(&ram[dst_physical_addr], &ram[src_physical_addr], pixels_to_copy << 2);

			src_virtual_addr  += pixels_to_copy;
			src_physical_addr += pixels_to_copy;
			dst_virtual_addr  += pixels_to_copy;
			dst_physical_addr += pixels_to_copy;

			pixels_copied     += pixels_to_copy;

			if(pixels_copied < x_size)
				physical_addr_dirty = true;
			else
				break;
		}

		src_virtual_addr += src_stride_overflow;
		dst_virtual_addr += dst_stride_overflow;
	}
}

bool i82830_graphics_device::execute_3d_state16_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{
	switch(instruction & i82830_graphics_device::MI_3D_STATE16_SUBTYPE_MASK)
	{
		case MI_3D_STATE16_CMD_SCISSOR_ENABLE:
			return true;

		default:
			logerror("Unknown 3d state16 graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

bool i82830_graphics_device::execute_3d_statemw_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{
	switch(instruction & i82830_graphics_device::MI_3D_STATEMW_SUBTYPE_MASK)
	{
		case MI_3D_STATEMW_CMD_UNK1:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 7);
			return true;

		case MI_3D_STATEMW_CMD_SCISSOR_RECTANGLE_INFO:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 2);
			return true;

		case MI_3D_STATEMW_CMD_STIPPLE_PATTERN:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 1);
			return true;

		case MI_3D_STATEMW_CMD_DEST_BUFFER_VARIABLES:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 7);
			return true;

		case MI_3D_STATEMW_CMD_UNK2:
			// EMAC(NOTE): not implemented
			i82830_graphics_device::instruction_buffer_skip(parser_state, 2);
			return true;

		case MI_3D_STATEMW_CMD_UNK3:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_STATEMW_CMD_UNK4:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_STATEMW_CMD_UNK5:
			// EMAC(NOTE): not implemented
			return true;

		default:
			logerror("Unknown 3d statemw graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

bool i82830_graphics_device::execute_3d_block_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{
	switch(instruction & i82830_graphics_device::MI_3D_BLOCK_SUBTYPE_MASK)
	{
		default:
			logerror("Unknown 3d block graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

bool i82830_graphics_device::execute_3d_prim_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{
	switch(instruction & i82830_graphics_device::MI_3D_PRIM_SUBTYPE_MASK)
	{
		default:
			logerror("Unknown 3d prim graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

bool i82830_graphics_device::execute_3d_instruction(uint32_t instruction, ins_parser_state_t* parser_state)
{

	switch(instruction & i82830_graphics_device::MI_3D_SUBTYPE_MASK)
	{
		// STATE24   = 0x00 .. 0x0f

		case MI_3D_CMD_BOOLEAN_ENA_1:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_CMD_BOOLEAN_ENA_2:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_CMD_UNK1:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_CMD_UNK2:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_CMD_UNK3:
			// EMAC(NOTE): not implemented
			return true;

		// STATE24NP = 0x10 .. 0x18

		case MI_3D_CMD_FOG_COLOR:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_CMD_UNK4:
			// EMAC(NOTE): not implemented
			return true;

		case MI_3D_STATE16:
			return i82830_graphics_device::execute_3d_state16_instruction(instruction, parser_state);

		case MI_3D_STATEMW:
			return i82830_graphics_device::execute_3d_statemw_instruction(instruction, parser_state);

		case MI_3D_BLOCK:
			return i82830_graphics_device::execute_3d_block_instruction(instruction, parser_state);

		case MI_3D_PRIM:
			return i82830_graphics_device::execute_3d_prim_instruction(instruction, parser_state);
		
		default:
			logerror("Unknown 3d graphics instruction: %08x\n", instruction);
			machine().debug_break();
			return false;
	}
}

uint32_t i82830_graphics_device::mm_block_r(offs_t offset)
{
	offset &= ((i82830_graphics_device::MM_SIZE / 4) - 1);
	
	uint32_t data = 0xffffffff;

	switch(offset)
	{
		case i82830_graphics_device::MM_IOCNTL_GPIOA:
		case i82830_graphics_device::MM_IOCNTL_GPIOB:
		case i82830_graphics_device::MM_IOCNTL_GPIOC:
			data = m_gpio_r_cb(offset);
			break;

		case i82830_graphics_device::MM_TV_HTOTAL:
			data = 0x000004ff; // 0x280
			break;

		case i82830_graphics_device::MM_TV_VTOTAL:
			data = 0x000003bf; // 0x1e0
			break;

		case i82830_graphics_device::MM_DISPLAY_UNKNOWN1:
			data = 0x18100000;
			break;

		default:
		{
			data = m_mm_block[offset];
			break;
		}
	}

	return data;
}

void i82830_graphics_device::mm_block_w(offs_t offset, uint32_t data)
{
	offset &= ((i82830_graphics_device::MM_SIZE / 4) - 1);

	switch(offset)
	{
		case i82830_graphics_device::MM_CNTL_PRINGBUF_CNTL:
		case i82830_graphics_device::MM_CNTL_PRINGBUF_HEAD:
		case i82830_graphics_device::MM_CNTL_PRINGBUF_TAIL:
			m_mm_block[offset] = data;

			i82830_graphics_device::pins_execute_next();
			break;

		case i82830_graphics_device::MM_DPLLA_CTRL:
			//if(false&&data == 0xd0842000)
			//	machine().debug_break();

			data &= 0xfffbffff;

			m_mm_block[offset] = data;
		break;

		case i82830_graphics_device::MM_IOCNTL_GPIOA:
		case i82830_graphics_device::MM_IOCNTL_GPIOB:
		case i82830_graphics_device::MM_IOCNTL_GPIOC:
			m_gpio_w_cb(offset, data);
			break;

		case i82830_graphics_device::MM_CNTL_IIR:
			if(m_mm_block[offset] & data)
			{
				m_mm_block[offset] &= (~data);
				m_pirq_w_cb(m_pirq_pin, CLEAR_LINE);
			}
			break;

		default:
			m_mm_block[offset] = data;
			break;
	}
}


uint32_t i82830_graphics_device::gmadr_r()
{
	return m_gmadr;
}

void i82830_graphics_device::gmadr_w(uint32_t data)
{
	uint16_t gcc1 = m_mcu->get_gcc1();

	uint32_t gmadr_mask = 
		(gcc1 & i82830_host_device::GMCH_CNTL1_IGD_GM_64MB)
		? i82830_graphics_device::GFX_GMADR_64MEM_BASE_MASK
		: i82830_graphics_device::GFX_GMADR_128MEM_BASE_MASK;

	uint32_t gmadr = (data & gmadr_mask);
	if(gmadr != 0x00000000)
	{
		m_gmadr = (m_gmadr & (~gmadr_mask)) | gmadr;
		i82830_graphics_device::remap_cb();
	}
}

uint32_t i82830_graphics_device::mmadr_r()
{
	return m_mmadr;
}

void i82830_graphics_device::mmadr_w(uint32_t data)
{
	uint32_t mmadr = (data & i82830_graphics_device::GFX_MMADR_MEM_BASE_MASK);
	if(mmadr != 0x00000000)
	{
		m_mmadr = (m_mmadr & (~i82830_graphics_device::GFX_MMADR_MEM_BASE_MASK)) | mmadr;
		i82830_graphics_device::remap_cb();
	}
}

void i82830_graphics_device::subvendor_w(uint16_t data)
{
	if(!(subsystem_id & 0xffff0000))
		subsystem_id = (subsystem_id & 0x0000ffff) | data << 0x10;
}

void i82830_graphics_device::subsystem_w(uint16_t data)
{
	if(!(subsystem_id & 0x0000ffff))
		subsystem_id = (subsystem_id & 0xffff0000) | data;
}

uint8_t i82830_graphics_device::capptr_r()
{
	return 0xd0;
}

uint16_t i82830_graphics_device::coreclk_r()
{
	return 0x0000;
}

uint16_t i82830_graphics_device::xxx16_r()
{
	return 0x0000;
}

void i82830_graphics_device::xxx16_w(uint16_t data)
{
	//
}

uint32_t i82830_graphics_device::xxx32_r()
{
	return 0x00000001;
}

void i82830_graphics_device::xxx32_w(uint32_t data)
{
	//
}

uint32_t i82830_graphics_device::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint32_t base = 0x00000000;
	uint32_t screen_height = 480;
	uint32_t screen_width = 640;
	uint32_t stride = 0x400;

	uint32_t virtual_addr = base;
	uint32_t physical_addr = 0x00000000;
	uint32_t stride_overflow = (stride - screen_width);

	uint32_t* ram = m_mcu->get_ram_pointer();

	for(uint32_t y = 0; y < screen_height; y++)
	{
		uint32_t* line = &bitmap.pix(y);

		bool addr_dirty = true;

		uint32_t pixels_copied = 0;
		while(true)
		{
			if(addr_dirty)
			{
				physical_addr = i82830_graphics_device::gtt_lookup(
					virtual_addr
				);

				if(physical_addr == i82830_graphics_device::MM_GTT_PAGE_INVALID)
					break;

				addr_dirty = false;
			}

			// Try to copy all pixels as long as we don't reach the page boundry (in this case we'd need to copy up to the page boundry then to another page lookup for the next page)
			uint32_t page_offset = virtual_addr & i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE_MASK;
			uint32_t pixels_to_copy = std::min({
				(i82830_graphics_device::MM_GTT_PAGE_ADDR32_SIZE - page_offset), 
				(screen_width - pixels_copied)
			});

			memcpy(line, &ram[physical_addr], pixels_to_copy << 2);

			virtual_addr  += pixels_to_copy;
			physical_addr += pixels_to_copy;

			line          += pixels_to_copy;

			pixels_copied += pixels_to_copy;

			if(pixels_copied < screen_width)
				addr_dirty = true;
			else
				break;
		}

		virtual_addr += stride_overflow;
	}

	//return m_svga->screen_update(screen, bitmap, cliprect);

	return 0;
}

void i82830_graphics_device::set_connected_pirq(uint8_t legacy_interrupt_pin, uint8_t pirq_pin)
{
	agp_device::interrupt_pin_w(0, legacy_interrupt_pin);

	m_pirq_pin = pirq_pin;
}

void i82830_graphics_device::vblank_irq(int state)
{
/*
mm_block_w: offset=00070024, data=00000002 [m_test_val_tx=00, m_test_val_tx_bit=08]
mm_block_r: offset=00070024 [m_test_val_tx=00, m_test_val_tx_bit=08] = 00000002
mm_block_w: offset=00070024, data=00020002 [m_test_val_tx=00, m_test_val_tx_bit=08]

IIR—Interrupt Identity Register: mm_block_w: offset=000020a4, data=00000080 [m_test_val_tx=00, m_test_val_tx_bit=08]
IMR—Interrupt Mask Register: mm_block_r: offset=000020a8 [m_test_val_tx=00, m_test_val_tx_bit=08] = 00000000
IMR—Interrupt Mask Register: mm_block_w: offset=000020a8, data=00000000 [m_test_val_tx=00, m_test_val_tx_bit=08]
IER—Interrupt Enable Register: mm_block_w: offset=000020a0, data=00000080 [m_test_val_tx=00, m_test_val_tx_bit=08]
ISR—Interrupt Status Register: 000020ac

	static constexpr uint32_t MM_CNTL_IER                = 0x020a0;
	static constexpr uint32_t MM_CNTL_IIR                = 0x020a4;
	static constexpr uint32_t MM_CNTL_IMR                = 0x020a8;
	static constexpr uint32_t MM_CNTL_ISR                = 0x020ac;

*/
	if(m_mm_block[i82830_graphics_device::MM_DISPLAY_DPLYSTAS] & 0x00020002)
	{
		m_mm_block[i82830_graphics_device::MM_CNTL_IIR] |= 0x80 & (~m_mm_block[i82830_graphics_device::MM_CNTL_IMR]);

		if(m_mm_block[i82830_graphics_device::MM_CNTL_IER] & 0x80)
		{
			m_mm_block[i82830_graphics_device::MM_CNTL_ISR] |= 0x80;


			if(intr_line > 0 && m_mm_block[i82830_graphics_device::MM_CNTL_IIR] & 0x80)
				m_pirq_w_cb(m_pirq_pin, ASSERT_LINE);
		}
	}
}