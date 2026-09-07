// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_CPU_I386_DRC_I386FE_H
#define MAME_CPU_I386_DRC_I386FE_H

#pragma once

#include "cpu/drcfe.h"

#include "drc_i386.h"
#include "i386.h"
#include "i386priv.h"

#include <ostream>
#include <streambuf>


class i386_device::opcode_desc : public opcode_desc_base<opcode_desc, 1>
{
public:
	uint8_t opcode0 = 0;
	uint8_t opcode1 = 0;

	drc_dispatch_type dispatch_type = DISPATCH_PRI_TABLE;

	uint8_t pfx_len    = 0;
	uint8_t opcode_len = 1;

	int8_t seg_override = -1;
	bool   osz_override = false; // 0x66 (operand-size override) prefix present
	bool   asz_override = false; // 0x67 (address-size override) prefix present
	bool   op32         = false;
	bool   addr32       = false;

	bool    has_modrm   = false;
	uint8_t modrm       = 0;
	uint8_t modrm_mode  = 0;
	uint8_t modrm_regop = 0;
	uint8_t modrm_rm    = 0;
	bool    is_mem      = false; // modrm_mode != 3

	bool    has_sib    = false;
	uint8_t sib        = 0;
	uint8_t disp_bytes = 0; // 0, 1, 2, or 4
	int32_t disp       = 0; // sign extended; meaningless when disp_bytes == 0

	uint8_t  imm_bytes = 0; // 0, 1, 2, or 4
	uint32_t imm       = 0; // raw imm bits

	void reset(offs_t curpc, bool in_delay_slot)
	{
		opcode_desc_base::reset(curpc, in_delay_slot);

		opcode0    = 0;
		opcode1    = 0;
		pfx_len    = 0;
		opcode_len = 1;

		dispatch_type = DISPATCH_PRI_TABLE;

		seg_override = -1;
		osz_override = false;
		asz_override = false;
		op32         = false;
		addr32       = false;

		has_modrm   = false;
		modrm       = 0;
		modrm_mode  = 0;
		modrm_regop = 0;
		modrm_rm    = 0;
		is_mem      = false;
		has_sib     = false;
		sib         = 0;
		disp_bytes  = 0;
		disp        = 0;
		imm_bytes   = 0;
		imm         = 0;
	}
};

class i386_device::frontend : public drc_frontend_base<opcode_desc>
{
public:
	frontend(i386_device *i386, uint32_t window_start, uint32_t window_end, uint32_t max_sequence) :
		drc_frontend_base(12 /* x86 paging is always 4K pages */, window_start, window_end, max_sequence),
		m_i386(i386)
	{
	}

	const opcode_desc *describe_code(offs_t startpc, uint8_t mode)
	{
		m_mode = mode;
		return do_describe_code(
				[this](opcode_desc &desc, opcode_desc const *prev)
				{
					return describe(desc, prev);
				},
				startpc
		);
	}

private:
	void classify_opcode(opcode_desc &desc)
	{
		// NOTE: gotos in here are for improved performance.

		offs_t cursor = desc.pc;
		while (true)
		{
			uint8_t byte = m_i386->drc_peek8(cursor++);

			switch (byte)
			{
				// ES segment override
				case 0x26:
					desc.seg_override = ES;
					continue;
				// CS segment override
				case 0x2e:
					desc.seg_override = CS;
					continue;
				// SS segment override
				case 0x36:
					desc.seg_override = SS;
					continue;
				// DS segment override
				case 0x3e:
					desc.seg_override = DS;
					continue;
				// FS segment override
				case 0x64:
					desc.seg_override = FS;
					continue;
				// GS segment override
				case 0x65:
					desc.seg_override = GS;
					continue;
				// Operand size override (change data size 16-bit vs 32-bit)
				case 0x66:
					desc.osz_override = true;
					continue;
				// Address size override (change address size 16-bit vs 32-bit)
				case 0x67:
					desc.asz_override = true;
					continue;
				// Exclusive use of all shared memory (lock)
				case 0xf0:
					continue;
				// 0x0f type-byte op
				case 0x0f:
					desc.opcode_len    = 2;
					desc.dispatch_type = DISPATCH_X0F_TABLE;
					desc.opcode0       = byte;
					goto done_rewind_cursor;
				// REP/REPNE type-byte string op
				case 0xf2:
				case 0xf3:
					desc.opcode_len = 2;
					desc.opcode0    = byte;
					goto done_rewind_cursor;
				// Eveyrthing else isn't a prefix, so we need to rewind the cursor
				default:
					desc.opcode0 = byte;
					goto done_rewind_cursor;
			}
		}

	done_rewind_cursor:
		cursor--;
		desc.pfx_len = (uint8_t)(cursor - desc.pc);
		desc.opcode1 = (desc.opcode_len == 2) ? m_i386->drc_peek8(cursor + 1) : 0;
	}

	offs_t branch_targetpc(opcode_desc const &desc, int32_t rel) const
	{
		if (desc.op32 || (desc.opcode0 >= 0xe0 && desc.opcode0 <= 0xe3))
		{
			return desc.pc + desc.length + rel;
		}
		else
		{
			const offs_t cs_base = m_i386->m_core->sreg[CS].base;

			return cs_base + (((desc.pc - cs_base) + desc.length + rel) & 0xffff);
		}
	}

	bool describe(opcode_desc &desc, opcode_desc const *prev)
	{
		classify_opcode(desc);

		const offs_t opcode_pc = desc.pc + desc.pfx_len;

		const uint8_t opcode0 = desc.opcode0;

		desc.op32   = (m_mode == 1) != desc.osz_override;
		desc.addr32 = (m_mode == 1) != desc.asz_override;

		offs_t cursor = opcode_pc + desc.opcode_len;

		uint32_t flags;
		switch (desc.dispatch_type)
		{
			case DISPATCH_X0F_TABLE:
				flags = m_i386->m_drc_sel_x0f_table[desc.opcode1].drc_flags;
				break;
			case DISPATCH_PRI_TABLE:
			default:
				flags = m_i386->m_drc_sel_pri_table[desc.opcode0].drc_flags;
				break;
		}

		if (flags & DRC_HAS_MODRM)
		{
			desc.has_modrm = true;
			desc.modrm     = m_i386->drc_peek8(cursor);
			cursor++;
			desc.modrm_mode  = MRM_MOD(desc.modrm);
			desc.modrm_regop = MRM_REG(desc.modrm);
			desc.modrm_rm    = MRM_RM(desc.modrm);
			desc.is_mem      = desc.modrm_mode != 3;

			if (desc.is_mem)
			{
				if (desc.addr32)
				{
					if (desc.modrm_mode == 0 && desc.modrm_rm == 5)
					{
						desc.disp_bytes = 4;
					}
					else if (MRM_HAS_SIB(desc.modrm))
					{
						desc.has_sib = true;
						desc.sib     = m_i386->drc_peek8(cursor);
						cursor++;
						const uint8_t sib_base = SIB_BASE(desc.sib);
						switch (desc.modrm_mode)
						{
							case 0:
								if (sib_base == 5)
									desc.disp_bytes = 4;
								break;
							case 1:
								desc.disp_bytes = 1;
								break;
							case 2:
								desc.disp_bytes = 4;
								break;
						}
					}
					else if (desc.modrm_mode == 1)
					{
						desc.disp_bytes = 1;
					}
					else if (desc.modrm_mode == 2)
					{
						desc.disp_bytes = 4;
					}
				}
				else // 16-bit addressing - never any SIB
				{
					if (desc.modrm_rm == 6 && desc.modrm_mode == 0)
						desc.disp_bytes = 2;
					else if (desc.modrm_mode == 1)
						desc.disp_bytes = 1;
					else if (desc.modrm_mode == 2)
						desc.disp_bytes = 2;
				}

				switch (desc.disp_bytes)
				{
					case 1:
						desc.disp = (int32_t)(int8_t)m_i386->drc_peek8(cursor);
						break;
					case 2:
						desc.disp = (int32_t)m_i386->drc_peek16(cursor);
						break;
					case 4:
						desc.disp = (int32_t)m_i386->drc_peek32(cursor);
						break;
					default:
						break;
				}
				cursor += desc.disp_bytes;
			}
		}

		switch (DRC_IMM_CLASS(flags))
		{
			case DRC_IMM_NONE:
				break;
			case DRC_IMM_B:
				desc.imm_bytes = 1;
				desc.imm       = m_i386->drc_peek8(cursor);
				break;
			case DRC_IMM_BS:
				desc.imm_bytes = 1;
				desc.imm       = m_i386->drc_peek8(cursor);
				break;
			case DRC_IMM_Z:
				desc.imm_bytes = desc.op32 ? 4 : 2;
				desc.imm       = desc.op32 ? m_i386->drc_peek32(cursor) : m_i386->drc_peek16(cursor);
				break;
			case DRC_IMM_W:
				desc.imm_bytes = 2;
				desc.imm       = m_i386->drc_peek16(cursor);
				break;
			case DRC_IMM_MOFFS:
				desc.imm_bytes = desc.addr32 ? 4 : 2;
				desc.imm       = desc.addr32 ? m_i386->drc_peek32(cursor) : m_i386->drc_peek16(cursor);
				break;
			case DRC_IMM_FARPTR:
				desc.imm_bytes = (desc.op32 ? 4 : 2) + 2;
				break;
			case DRC_IMM_GRP3:
				if (desc.modrm_regop < 2)
				{
					if (opcode0 == 0xf6)
					{
						desc.imm_bytes = 1;
						desc.imm       = m_i386->drc_peek8(cursor);
					}
					else
					{
						desc.imm_bytes = desc.op32 ? 4 : 2;
						desc.imm       = desc.op32 ? m_i386->drc_peek32(cursor) : m_i386->drc_peek16(cursor);
					}
				}
				break;
			default:
				break;
		}
		cursor += desc.imm_bytes;

		desc.length = (uint8_t)(cursor - desc.pc);

		if (opcode0 == 0x0f)
		{
			if (flags & DRC_BR_RELW)
			{
				const int32_t rel = (desc.length == desc.pfx_len + 4) ? (int32_t)(int16_t)m_i386->drc_peek16(opcode_pc + 2) : (int32_t)m_i386->drc_peek32(opcode_pc + 2);
				desc.targetpc     = branch_targetpc(desc, rel);

				desc.set_is_conditional_branch();
			}
			else if (flags & DRC_BR_END)
			{
				desc.set_end_sequence();
			}
		}
		else if (opcode0 == 0xff)
		{
			const uint8_t group_ff_opcode = desc.modrm_regop;
			if (group_ff_opcode >= 2 && group_ff_opcode <= 5)
				desc.set_end_sequence();
		}
		else if (flags & DRC_BR_REL8)
		{
			desc.targetpc = branch_targetpc(desc, (int32_t)(int8_t)m_i386->drc_peek8(opcode_pc + 1));

			if (flags & DRC_BR_COND)
			{
				desc.set_is_conditional_branch();
			}
			else
			{
				desc.set_is_unconditional_branch();
				desc.set_end_sequence();
			}
		}
		else if (flags & DRC_BR_RELW)
		{
			const int32_t rel = (desc.length == desc.pfx_len + 3) ? (int32_t)(int16_t)m_i386->drc_peek16(opcode_pc + 1) : (int32_t)m_i386->drc_peek32(opcode_pc + 1);
			desc.targetpc     = branch_targetpc(desc, rel);

			desc.set_is_unconditional_branch();

			desc.set_end_sequence();
		}
		else if (flags & DRC_BR_END)
		{
			desc.set_end_sequence();
		}

		return true;
	}

	uint8_t      m_mode = 0;
	i386_device *m_i386;
};

#endif // MAME_CPU_I386_DRC_I386FE_H
