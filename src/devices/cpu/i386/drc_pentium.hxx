// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "x87priv.h"

// ----------------------------------------------------------------------------
// C++ Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_mmx_nm_trap_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				i386_trap(FAULT_NM, 0);
			}
	);
	drc_leave_interpreter();
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

template <typename ApplyFn> inline bool i386_device::drc_gen_mmx_binop(compiler_state &ctx, bool mem_is_32bit, ApplyFn &&apply)
{
	drcuml_block &b = ctx.block;

	drc_gen_mmx_prolog(ctx);

	uint8_t modrm   = drc_get_modrm(ctx);
	int     dst_n   = MRM_REG(modrm);
	void   *dst_ptr = &m_core->x87_reg[dst_n].signif;
	void   *src_ptr;

	if (MRM_MOD(modrm) == 3)
	{
		int src_n = MRM_RM(modrm);
		src_ptr   = &m_core->x87_reg[src_n].signif;
	}
	else
	{
		const uml::parameter &mem_data = I2;

		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		if (mem_is_32bit)
		{
			UML_CALLH(b, *m_mem_read32);
			UML_DAND(b, DRC_SCR64, mem_data, 0xffffffffULL);
		}
		else
		{
			UML_CALLH(b, *m_mem_read64);
			UML_DMOV(b, DRC_SCR64, mem_data);
		}
		src_ptr = &m_core->data64;
	}

	apply(b, dst_ptr, src_ptr);

	drc_record_cycles(ctx, CYCLES_MOV_REG_MEM);
	return true;
}

void i386_device::drc_gen_mmx_prolog(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::code_label fault = NEW_LBL(ctx);
	const uml::code_label done  = NEW_LBL(ctx);

	// Check if task switched flag is set (MMX context could be borked)
	UML_TEST(b, DRC_CR(0), CR0_TS);
	UML_JMPc(b, COND_NZ, fault);

	uint32_t mask = ~(X87_SW_TOP_MASK << X87_SW_TOP_SHIFT);

	UML_AND(b, uml::mem(&m_core->x87_sw), uml::mem(&m_core->x87_sw), mask);
	UML_MOV(b, uml::mem(&m_core->x87_tw), 0);
	UML_JMP(b, done);

	UML_LABEL(b, fault);
	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_mmx_nm_trap_cb), this);
	drc_gen_fault_inplace_check(ctx);

	UML_LABEL(b, done);
}

// ----------------------------------------------------------------------------
// Pentium MMX instructions
// ----------------------------------------------------------------------------

bool i386_device::drc_x0f_mmx_group_0f71(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mmx_value = I1;
	const uml::parameter &result    = I2;
	const uml::parameter &lane_tmp  = I3;

	uint8_t modrm      = drc_get_modrm(ctx);
	uint8_t mmx_opcode = MRM_OPCODE(modrm);
	bool    is_reg     = MRM_MOD(modrm) == 3;

	if (is_reg)
	{
		switch (mmx_opcode)
		{
			case 2:
			case 4:
			case 6:
				break;

			default:
				return false;
		}
	}

	uint8_t imm8 = drc_get_imm8(ctx);

	drc_gen_mmx_prolog(ctx);

	if (!is_reg)
		return true;

	int n = MRM_RM(modrm);
	UML_DMOV(b, mmx_value, DRC_X87REG(n, signif));

	if (mmx_opcode == 2)
	{
		if (imm8 > 15)
		{
			UML_DMOV(b, result, 0);
		}
		else
		{
			uint16_t lane      = (uint16_t)(0xffff >> imm8);
			uint64_t keep_mask = lane | ((uint64_t)lane << 16) | ((uint64_t)lane << 32) | ((uint64_t)lane << 48);
			UML_DSHR(b, result, mmx_value, imm8);
			UML_DAND(b, result, result, keep_mask);
		}
	}
	else if (mmx_opcode == 6)
	{
		if (imm8 > 15)
		{
			UML_DMOV(b, result, 0);
		}
		else
		{
			uint16_t lane      = (uint16_t)((0xffff << imm8) & 0xffff);
			uint64_t keep_mask = lane | ((uint64_t)lane << 16) | ((uint64_t)lane << 32) | ((uint64_t)lane << 48);
			UML_DSHL(b, result, mmx_value, imm8);
			UML_DAND(b, result, result, keep_mask);
		}
	}
	else
	{
		int shift_amt = (imm8 > 15) ? 15 : imm8;
		UML_DMOV(b, result, 0);
		for (int lane = 0; lane < 4; lane++)
		{
			UML_DBFXS(b, lane_tmp, mmx_value, lane * 16, 16);
			UML_DSAR(b, lane_tmp, lane_tmp, shift_amt);
			UML_DROLINS(b, result, lane_tmp, lane * 16, 0xffffULL << (lane * 16));
		}
	}

	UML_DMOV(b, DRC_X87REG(n, signif), result);

	return true;
}

bool i386_device::drc_x0f_mmx_paddw(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIADD(b, dst_ptr, dst_ptr, src_ptr, SIZE_WORD);
			}
	);
}

bool i386_device::drc_x0f_mmx_movq_store(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	drc_gen_mmx_prolog(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	int     src_n = MRM_REG(modrm);

	if (MRM_MOD(modrm) == 3)
	{
		int dst_n = MRM_RM(modrm);
		UML_DMOV(b, DRC_X87REG(dst_n, signif), DRC_X87REG(src_n, signif));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		UML_DMOV(b, mem_data, DRC_X87REG(src_n, signif));
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write64);
	}

	drc_record_cycles(ctx, CYCLES_MOV_REG_MEM);

	return true;
}

bool i386_device::drc_x0f_mmx_movq_load(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	drc_gen_mmx_prolog(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	int     dst_n = MRM_REG(modrm);

	if (MRM_MOD(modrm) == 3)
	{
		int src_n = MRM_RM(modrm);
		UML_DMOV(b, DRC_X87REG(dst_n, signif), DRC_X87REG(src_n, signif));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read64);
		UML_DMOV(b, DRC_X87REG(dst_n, signif), mem_data);
	}

	drc_record_cycles(ctx, CYCLES_MOV_REG_MEM);

	return true;
}

bool i386_device::drc_x0f_mmx_movd_load(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	drc_gen_mmx_prolog(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	int     dst_n = MRM_REG(modrm);

	if (MRM_MOD(modrm) == 3)
	{
		UML_MOV(b, mem_data, DRC_GET_MRM_RM32(modrm));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
	}

	UML_DAND(b, DRC_X87REG(dst_n, signif), mem_data, 0xffffffffULL);

	drc_record_cycles(ctx, CYCLES_MOV_REG_MEM);

	return true;
}

bool i386_device::drc_x0f_mmx_pmullw(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIMUL(b, dst_ptr, dst_ptr, src_ptr, SIZE_WORD);
			}
	);
}

bool i386_device::drc_x0f_mmx_punpcklbw(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			true,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIUNPCKL(b, dst_ptr, dst_ptr, src_ptr, SIZE_BYTE);
			}
	);
}

bool i386_device::drc_x0f_mmx_punpckhbw(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIUNPCKH(b, dst_ptr, dst_ptr, src_ptr, SIZE_BYTE);
			}
	);
}

bool i386_device::drc_x0f_mmx_packuswb(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIPACKUS(b, dst_ptr, dst_ptr, src_ptr, SIZE_WORD);
			}
	);
}

bool i386_device::drc_x0f_mmx_paddusb(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIADDUS(b, dst_ptr, dst_ptr, src_ptr, SIZE_BYTE);
			}
	);
}

bool i386_device::drc_x0f_mmx_bitwise(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				switch (opcode1)
				{
					case 0xdb: // PAND
						UML_VIAND(b, dst_ptr, dst_ptr, src_ptr);
						break;
					case 0xeb: // POR
						UML_VIOR(b, dst_ptr, dst_ptr, src_ptr);
						break;
					case 0xef: // PXOR
						UML_VIXOR(b, dst_ptr, dst_ptr, src_ptr);
						break;
					case 0xdf: // PANDN - (~dst) & src
						UML_VIANDN(b, dst_ptr, dst_ptr, src_ptr);
						break;
				}
			}
	);
}

bool i386_device::drc_x0f_mmx_add_wrap(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xfc) ? SIZE_BYTE : (opcode1 == 0xfe) ? SIZE_DWORD : SIZE_WORD;
				UML_VIADD(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_sub_wrap(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xf8) ? SIZE_BYTE : (opcode1 == 0xfa) ? SIZE_DWORD : SIZE_WORD;
				UML_VISUB(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_paddq(compiler_state &ctx)
{
	return drc_gen_mmx_binop(
			ctx,
			false,
			[](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				UML_VIADD(b, dst_ptr, dst_ptr, src_ptr, SIZE_QWORD);
			}
	);
}

bool i386_device::drc_x0f_mmx_add_usat(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xdc) ? SIZE_BYTE : SIZE_WORD;
				UML_VIADDUS(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_sub_usat(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xd8) ? SIZE_BYTE : SIZE_WORD;
				UML_VISUBUS(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_add_ssat(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xec) ? SIZE_BYTE : SIZE_WORD;
				UML_VIADDS(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_sub_ssat(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0xe8) ? SIZE_BYTE : SIZE_WORD;
				UML_VISUBS(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_punpckl(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			true,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0x61) ? SIZE_WORD : SIZE_DWORD;
				UML_VIUNPCKL(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_punpckh(compiler_state &ctx)
{
	uint8_t opcode1 = ctx.desc->opcode1;
	return drc_gen_mmx_binop(
			ctx,
			false,
			[opcode1](drcuml_block &b, void *dst_ptr, void *src_ptr)
			{
				operand_size elemsize = (opcode1 == 0x69) ? SIZE_WORD : SIZE_DWORD;
				UML_VIUNPCKH(b, dst_ptr, dst_ptr, src_ptr, elemsize);
			}
	);
}

bool i386_device::drc_x0f_mmx_emms(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_INTERP(mmx_emms), this);
	drc_gen_fault_inplace_check(ctx);

	return true;
}
