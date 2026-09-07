// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// ADD/OR/ADC/SBB/AND/SUB/XOR/CMP (alu8/16/32 + imm)
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_alu_op(compiler_state &ctx, uint8_t alu_opcode, const uml::parameter lhs, const uml::parameter rhs, const uml::parameter result, int width_bits)
{
	drcuml_block &b = ctx.block;

	switch (alu_opcode)
	{
		case 0:
			UML_ADD(b, result, lhs, rhs);
			break;
		case 1:
			UML_OR(b, result, lhs, rhs);
			break;
		case 2:
			UML_CARRY(b, uml::mem(&m_core->CF), 0);
			UML_ADDC(b, result, lhs, rhs);
			break;
		case 3:
			UML_CARRY(b, uml::mem(&m_core->CF), 0);
			UML_SUBB(b, result, lhs, rhs);
			break;
		case 4:
			UML_AND(b, result, lhs, rhs);
			break;
		case 5:
			UML_SUB(b, result, lhs, rhs);
			break;
		case 6:
			UML_XOR(b, result, lhs, rhs);
			break;
		case 7:
		default:
			UML_SUB(b, result, lhs, rhs);
			break;
	}

	drc_gen_defer_flags_alu(ctx, alu_opcode, width_bits);
}

bool i386_device::drc_pri_alu8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t alu_opcode = OP_GET_SUBOP(ctx.desc->opcode0);
	bool    to_rm      = !(ctx.desc->opcode0 & 2);

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_b = MRM_REG8(modrm);
	bool    is_m;

	if (to_rm)
	{
		is_m = ctx.desc->is_mem;
		if (!is_m)
		{
			UML_AND(b, alu_lhs, DRC_GET_MRM_RM8(modrm), 0xff);
			UML_AND(b, alu_rhs, DRC_REG8(reg_b), 0xff);
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_MOV(b, mem_addr_save, mem_addr);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, alu_lhs, mem_data, 0xff);
			UML_AND(b, alu_rhs, DRC_REG8(reg_b), 0xff);
		}
	}
	else
	{
		drc_gen_rm8(ctx, modrm);
		UML_MOV(b, alu_rhs, alu_lhs);
		UML_AND(b, alu_lhs, DRC_REG8(reg_b), 0xff);
		is_m = false;
	}

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 8);

	if (alu_opcode != 7)
	{

		if (to_rm)
		{
			if (!is_m)
			{
				UML_BREG_WRITE(b, MRM_RM8(modrm), alu_result);
			}
			else
			{
				UML_MOV(b, mem_addr, mem_addr_save);
				drc_flush_cycles(ctx);
				UML_CALLH(b, *m_mem_write8);
			}
		}
		else
		{
			UML_BREG_WRITE(b, reg_b, alu_result);
		}
	}

	bool is_reg = MRM_MOD(modrm) == 3;
	drc_record_cycles(ctx, is_reg ? CYCLES_ALU_REG_REG : CYCLES_ALU_REG_MEM);

	return true;
}

inline bool i386_device::drc_gen_alu8_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm      = drc_get_modrm(ctx);
	uint8_t alu_opcode = MRM_OPCODE(modrm);

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		UML_AND(b, alu_lhs, DRC_GET_MRM_RM8(modrm), 0xff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, alu_lhs, mem_data, 0xff);
	}

	UML_MOV(b, alu_rhs, (uint32_t)(drc_get_imm8(ctx) & 0xff));

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 8);

	if (alu_opcode != 7)
	{
		if (!is_m)
		{
			UML_BREG_WRITE(b, MRM_RM8(modrm), alu_result);
		}
		else
		{
			UML_MOV(b, mem_addr, mem_addr_save);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write8);
		}
	}

	bool is_reg = MRM_MOD(modrm) == 3;
	drc_record_cycles(ctx, is_reg ? CYCLES_ALU_IMM_REG : CYCLES_ALU_IMM_MEM);

	return true;
}

bool i386_device::drc_pri_alu_acc_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &alu_lhs    = I0;
	const uml::parameter &alu_rhs    = I1;
	const uml::parameter &alu_result = I2;

	uint8_t alu_opcode = OP_GET_SUBOP(ctx.desc->opcode0);

	if (!(ctx.desc->opcode0 & 1)) // 8-bit operand
	{
		UML_AND(b, alu_lhs, DRC_REG8(AL), 0xff);
		UML_MOV(b, alu_rhs, (uint32_t)drc_get_imm8(ctx));

		drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 8);

		if (alu_opcode != 7)
			UML_BREG_WRITE(b, AL, alu_result);
	}
	else
	{
		UML_MOV(b, alu_lhs, DRC_REG32(EAX));
		UML_MOV(b, alu_rhs, drc_get_imm32(ctx));

		drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 32);

		if (alu_opcode != 7)
			UML_MOV(b, DRC_REG32(EAX), alu_result);
	}

	drc_record_cycles(ctx, CYCLES_ALU_IMM_ACC);

	return true;
}

bool i386_device::drc_pri_alu16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t alu_opcode = OP_GET_SUBOP(ctx.desc->opcode0);
	bool    to_rm      = !(ctx.desc->opcode0 & 2);

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);

	if (to_rm)
	{
		if (!ctx.desc->is_mem)
		{
			UML_AND(b, alu_lhs, DRC_REG32(MRM_RM32(modrm)), 0xffff);
			UML_AND(b, alu_rhs, DRC_REG32(reg_d), 0xffff);
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_MOV(b, mem_addr_save, mem_addr);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, alu_lhs, mem_data, 0xffff);
			UML_AND(b, alu_rhs, DRC_REG32(reg_d), 0xffff);
		}
	}
	else
	{
		drc_gen_rm16(ctx, modrm);
		UML_MOV(b, alu_rhs, alu_lhs);
		UML_AND(b, alu_lhs, DRC_REG32(reg_d), 0xffff);
	}

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 16);

	if (alu_opcode != 7)
	{
		if (to_rm)
		{
			if (MRM_MOD(modrm) != 3)
			{
				UML_AND(b, alu_result, alu_result, 0xffff);
				UML_MOV(b, mem_addr, mem_addr_save);
				drc_flush_cycles(ctx);
				UML_CALLH(b, *m_mem_write16);
			}
			else
			{
				UML_ROLINS(b, DRC_GET_MRM_RM32(modrm), alu_result, 0, 0xffff);
			}
		}
		else
		{
			UML_ROLINS(b, DRC_REG32(reg_d), alu_result, 0, 0xffff);
		}
	}

	drc_record_cycles(ctx, MRM_MOD(modrm) == 3 ? CYCLES_ALU_REG_REG : (to_rm ? CYCLES_ALU_REG_MEM : CYCLES_ALU_MEM_REG));

	return true;
}

inline bool i386_device::drc_gen_alu16_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm      = drc_get_modrm(ctx);
	uint8_t alu_opcode = MRM_OPCODE(modrm);

	bool is_m = ctx.desc->is_mem;

	if (!is_m)
	{
		UML_AND(b, alu_lhs, DRC_REG32(MRM_RM32(modrm)), 0xffff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read16);
		UML_AND(b, alu_lhs, mem_data, 0xffff);
	}

	uint32_t imm;
	// 0x83 GRP1 imm8 (sign extended); 0x81 uses imm16/32
	if (ctx.desc->opcode0 == 0x83)
		imm = (uint32_t)(int32_t)(int8_t)drc_get_imm8(ctx) & 0xffff;
	else
		imm = (uint32_t)drc_get_imm16(ctx) & 0xffff;
	UML_MOV(b, alu_rhs, imm);

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 16);

	if (alu_opcode != 7)
	{
		if (!is_m)
		{
			UML_ROLINS(b, DRC_REG32(MRM_RM32(modrm)), alu_result, 0, 0xffff);
		}
		else
		{
			UML_AND(b, alu_result, alu_result, 0xffff);
			UML_MOV(b, mem_addr, mem_addr_save);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write16);
		}
	}

	drc_record_cycles(ctx, is_m ? CYCLES_ALU_IMM_MEM : CYCLES_ALU_IMM_REG);

	return true;
}

bool i386_device::drc_pri_alu16_acc_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &alu_lhs    = I0;
	const uml::parameter &alu_rhs    = I1;
	const uml::parameter &alu_result = I2;

	uint8_t  alu_opcode = OP_GET_SUBOP(ctx.desc->opcode0);
	uint32_t imm        = (uint32_t)drc_get_imm16(ctx) & 0xffff;

	UML_AND(b, alu_lhs, DRC_REG32(EAX), 0xffff);
	UML_MOV(b, alu_rhs, imm);

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 16);

	if (alu_opcode != 7)
		UML_ROLINS(b, DRC_REG32(EAX), alu_result, 0, 0xffff);

	drc_record_cycles(ctx, CYCLES_ALU_IMM_ACC);

	return true;
}

bool i386_device::drc_pri_alu32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t alu_opcode = OP_GET_SUBOP(ctx.desc->opcode0);
	bool    to_rm      = !(ctx.desc->opcode0 & 2);
	if (!(ctx.desc->opcode0 & 1))
		return false;

	uint8_t modrm  = drc_get_modrm(ctx);
	int     reg_d  = MRM_REG32(modrm);
	bool    is_reg = MRM_MOD(modrm) == 3;

	if (to_rm)
	{
		if (!ctx.desc->is_mem)
		{
			UML_MOV(b, alu_lhs, DRC_REG32(MRM_RM32(modrm)));
			UML_MOV(b, alu_rhs, DRC_REG32(reg_d));
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_MOV(b, mem_addr_save, mem_addr);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, alu_lhs, mem_data);
			UML_MOV(b, alu_rhs, DRC_REG32(reg_d));
		}
	}
	else
	{

		drc_gen_rm32(ctx, modrm);
		UML_MOV(b, alu_rhs, alu_lhs);
		UML_MOV(b, alu_lhs, DRC_REG32(reg_d));
	}

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 32);

	if (alu_opcode != 7)
	{
		if (to_rm)
		{
			bool is_m = MRM_MOD(modrm) != 3;
			if (!is_m)
			{
				UML_MOV(b, DRC_GET_MRM_RM32(modrm), alu_result);
			}
			else
			{
				UML_MOV(b, mem_addr, mem_addr_save);
				drc_flush_cycles(ctx);
				UML_CALLH(b, *m_mem_write32);
			}
		}
		else
		{
			UML_MOV(b, DRC_REG32(reg_d), alu_result);
		}
	}

	drc_record_cycles(ctx, is_reg ? CYCLES_ALU_REG_REG : (to_rm ? CYCLES_ALU_REG_MEM : CYCLES_ALU_MEM_REG));

	return true;
}

inline bool i386_device::drc_gen_alu32_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &alu_lhs       = I0;
	const uml::parameter &alu_rhs       = I1;
	const uml::parameter &alu_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm      = drc_get_modrm(ctx);
	uint8_t alu_opcode = MRM_OPCODE(modrm);

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		UML_MOV(b, alu_lhs, DRC_REG32(MRM_RM32(modrm)));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, alu_lhs, mem_data);
	}

	// 0x83 GRP1 imm8 (sign extended); 0x81 uses imm16/32
	uint32_t imm = (ctx.desc->opcode0 == 0x83) ? (uint32_t)(int32_t)(int8_t)drc_get_imm8(ctx) : drc_get_imm32(ctx);
	UML_MOV(b, alu_rhs, imm);

	drc_gen_alu_op(ctx, alu_opcode, alu_lhs, alu_rhs, alu_result, 32);

	if (alu_opcode != 7)
	{
		if (!is_m)
		{
			UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), alu_result);
		}
		else
		{
			UML_MOV(b, mem_addr, mem_addr_save);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write32);
		}
	}

	drc_record_cycles(ctx, is_m ? CYCLES_ALU_IMM_MEM : CYCLES_ALU_IMM_REG);

	return true;
}

// ----------------------------------------------------------------------------
// TEST
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_test8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &test_lhs = I0;
	const uml::parameter &test_and = I2;

	// 0xa8 TEST AL, imm8; 0x84 TEST r/m8, r8
	if (ctx.desc->opcode0 == 0xa8)
	{

		UML_AND(b, test_and, DRC_REG8(AL), (uint32_t)drc_get_imm8(ctx));
		drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL8);

		drc_record_cycles(ctx, CYCLES_TEST_IMM_ACC);
	}
	else
	{
		uint8_t modrm = drc_get_modrm(ctx);
		drc_gen_rm8(ctx, modrm);

		UML_AND(b, test_and, test_lhs, DRC_GET_MRM_REG8(modrm));
		drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL8);
		bool is_reg = MRM_MOD(modrm) == 3;

		drc_record_cycles(ctx, is_reg ? CYCLES_TEST_REG_REG : CYCLES_TEST_REG_MEM);
	}

	return true;
}

bool i386_device::drc_pri_test16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &test_lhs = I0;
	const uml::parameter &test_rhs = I1;
	const uml::parameter &test_and = I2;

	uint8_t modrm = drc_get_modrm(ctx);

	drc_gen_rm16(ctx, modrm);
	UML_AND(b, test_rhs, DRC_GET_MRM_REG32(modrm), 0xffff);
	UML_AND(b, test_and, test_lhs, test_rhs);

	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL16);

	drc_record_cycles(ctx, MRM_MOD(modrm) == 3 ? CYCLES_MOV_REG_REG : CYCLES_MOV_REG_MEM);

	return true;
}

bool i386_device::drc_pri_test32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &test_lhs = I0;
	const uml::parameter &test_rhs = I1;
	const uml::parameter &test_and = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	drc_gen_rm32(ctx, modrm);
	// 0x85 TEST r/m32, r32; 0xf7 /0 TEST r/m32, imm32 (group F7)
	if (ctx.desc->opcode0 == 0x85)
	{
		UML_MOV(b, test_rhs, DRC_GET_MRM_REG32(modrm));
		UML_AND(b, test_and, test_lhs, test_rhs);
	}
	else
	{
		UML_AND(b, test_and, test_lhs, drc_get_imm32(ctx));
	}
	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL32);

	drc_record_cycles(ctx, CYCLES_MOV_REG_REG);

	return true;
}

bool i386_device::drc_pri_test_acc_imm16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &eax_masked  = I0;
	const uml::parameter &test_result = I2;

	uint32_t imm16 = (uint32_t)drc_get_imm16(ctx) & 0xffff;

	UML_AND(b, eax_masked, DRC_REG32(EAX), 0xffff);
	UML_AND(b, test_result, eax_masked, imm16);
	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL16);

	drc_record_cycles(ctx, CYCLES_TEST_IMM_ACC);

	return true;
}

bool i386_device::drc_pri_test_acc_imm32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &test_result = I2;

	UML_MOV(b, test_result, DRC_REG32(EAX));
	UML_AND(b, test_result, test_result, drc_get_imm32(ctx));
	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL32);

	drc_record_cycles(ctx, CYCLES_TEST_IMM_ACC);

	return true;
}

// ----------------------------------------------------------------------------
// Group 80/81/82/83 (alu rm, imm)
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_group81_32(compiler_state &ctx)
{
	if (ctx.desc->modrm_regop != 7)
		ctx.invalidate_rscratch();

	return drc_gen_alu32_imm(ctx);
}

bool i386_device::drc_pri_group81_16(compiler_state &ctx)
{
	if (ctx.desc->modrm_regop != 7)
		ctx.invalidate_rscratch();
	return drc_gen_alu16_imm(ctx);
}

bool i386_device::drc_pri_group83_32(compiler_state &ctx)
{
	if (ctx.desc->modrm_regop != 7)
		ctx.invalidate_rscratch();

	return drc_gen_alu32_imm(ctx);
}

bool i386_device::drc_pri_group83_16(compiler_state &ctx)
{
	if (ctx.desc->modrm_regop != 7)
		ctx.invalidate_rscratch();
	return drc_gen_alu16_imm(ctx);
}

bool i386_device::drc_pri_group80_8(compiler_state &ctx)
{
	if (ctx.desc->modrm_regop != 7)
		ctx.invalidate_rscratch();
	return drc_gen_alu8_imm(ctx);
}

// ----------------------------------------------------------------------------
// INC/DEC
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_incdec_r32(compiler_state &ctx)
{
	return drc_gen_incdec32(ctx, true);
}

bool i386_device::drc_pri_incdec_r16(compiler_state &ctx)
{
	return drc_gen_incdec16(ctx, true);
}

inline bool i386_device::drc_gen_incdec_group(compiler_state &ctx, int width_bits, bool short_form)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &orig_value    = I2;
	const uml::parameter &incdec_result = I0;
	const uml::parameter &mem_addr_save = I6;

	bool is_mem_write = false;
	int  wb_reg       = -1;
	bool is_dec;

	if (short_form)
	{
		wb_reg = OP_RM32(ctx.desc->opcode0);
		is_dec = (ctx.desc->opcode0 >= 0x48);

		if (width_bits == 16)
			UML_AND(b, orig_value, DRC_REG32(wb_reg), 0xffff);
		else
			UML_MOV(b, orig_value, DRC_REG32(wb_reg));
	}
	else
	{
		uint8_t modrm         = drc_get_modrm(ctx);
		uint8_t incdec_opcode = MRM_OPCODE(modrm);
		is_dec                = (incdec_opcode == 1);

		if (width_bits != 32 && incdec_opcode != 0 && incdec_opcode != 1)
			return false;

		uint8_t rm_reg = MRM_RM32(modrm);
		bool    is_m   = ctx.desc->is_mem;
		if (!is_m)
		{
			switch (width_bits)
			{
				case 8:
					wb_reg = MRM_RM8(modrm);
					UML_AND(b, orig_value, DRC_GET_MRM_RM8(modrm), 0xff);
					break;
				case 16:
					wb_reg = rm_reg;
					UML_AND(b, orig_value, DRC_REG32(rm_reg), 0xffff);
					break;
				case 32:
				default:
					wb_reg = rm_reg;
					UML_MOV(b, orig_value, DRC_REG32(rm_reg));
					break;
			}
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			is_mem_write = true;
			UML_MOV(b, mem_addr_save, mem_addr);
			drc_flush_cycles(ctx);

			switch (width_bits)
			{
				case 8:
					UML_CALLH(b, *m_mem_read8);
					UML_AND(b, mem_data, mem_data, 0xff);
					break;
				case 16:
					UML_CALLH(b, *m_mem_read16);
					UML_AND(b, mem_data, mem_data, 0xffff);
					break;
				case 32:
				default:
					UML_CALLH(b, *m_mem_read32);
					break;
			}
		}
	}

	const uml::parameter &incdec_value = is_mem_write ? mem_data : orig_value;

	uint32_t tag;
	if (is_dec)
	{
		UML_SUB(b, incdec_result, incdec_value, 1);
		switch (width_bits)
		{
			case 8:
				tag = FLAGS_OPTYPE_DEC8;
				break;
			case 16:
				tag = FLAGS_OPTYPE_DEC16;
				break;
			case 32:
			default:
				tag = FLAGS_OPTYPE_DEC32;
				break;
		}
	}
	else
	{
		UML_ADD(b, incdec_result, incdec_value, 1);
		switch (width_bits)
		{
			case 8:
				tag = FLAGS_OPTYPE_INC8;
				break;
			case 16:
				tag = FLAGS_OPTYPE_INC16;
				break;
			case 32:
			default:
				tag = FLAGS_OPTYPE_INC32;
				break;
		}
	}
	drc_gen_defer_flags_incdec(ctx, tag);

	if (is_mem_write)
	{
		drc_flush_cycles(ctx);
		switch (width_bits)
		{
			case 8:
				UML_MOV(b, mem_data, incdec_result);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write8);
				break;
			case 16:
				UML_AND(b, mem_data, incdec_result, 0xffff);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write16);
				break;
			case 32:
			default:
				UML_MOV(b, mem_data, incdec_result);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write32);
				break;
		}
	}
	else if (wb_reg >= 0)
	{
		switch (width_bits)
		{
			case 8:
				UML_BREG_WRITE(b, wb_reg, incdec_result);
				break;
			case 16:
				UML_ROLINS(b, DRC_REG32(wb_reg), incdec_result, 0, 0xffff);
				break;
			case 32:
			default:
				UML_MOV(b, DRC_REG32(wb_reg), incdec_result);
				break;
		}
	}

	drc_record_cycles(ctx, CYCLES_MOV_REG_REG);

	return true;
}

inline bool i386_device::drc_gen_incdec32(compiler_state &ctx, bool short_form)
{
	return drc_gen_incdec_group(ctx, 32, short_form);
}

inline bool i386_device::drc_gen_incdec16(compiler_state &ctx, bool short_form)
{
	return drc_gen_incdec_group(ctx, 16, short_form);
}

bool i386_device::drc_pri_incdec8_rm(compiler_state &ctx)
{
	return drc_gen_incdec_group(ctx, 8, false);
}

// ----------------------------------------------------------------------------
// CBW/CWDE/CWD/CDQ (sign extension)
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_cbw_cwde(compiler_state &ctx)
{
	return drc_gen_cwde(ctx);
}

bool i386_device::drc_pri_cwd_cdq(compiler_state &ctx)
{
	return drc_gen_cdq(ctx);
}

inline bool i386_device::drc_gen_cwde(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_SEXT(b, DRC_REG32(EAX), DRC_REG16(AX), SIZE_WORD);

	drc_record_cycles(ctx, CYCLES_CBW);

	return true;
}

inline bool i386_device::drc_gen_cdq(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_SAR(b, DRC_REG32(EDX), DRC_REG32(EAX), 31);

	drc_record_cycles(ctx, CYCLES_MOV_REG_REG);

	return true;
}

// ----------------------------------------------------------------------------
// Group F6/F7 (TEST/NOT/NEG/MUL/IMUL/DIV/IDIV)
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_not_neg32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &orig_value    = I1;
	const uml::parameter &neg_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm           = drc_get_modrm(ctx);
	uint8_t group_f7_opcode = MRM_OPCODE(modrm);
	if (group_f7_opcode != 2 && group_f7_opcode != 3)
		return false;

	bool is_m = ctx.desc->is_mem;

	if (!is_m)
	{
		UML_MOV(b, rm_value, DRC_REG32(MRM_RM32(modrm)));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, rm_value, mem_data);
	}

	if (group_f7_opcode == 2)
	{
		UML_XOR(b, rm_value, rm_value, 0xffffffff);
	}
	else
	{

		UML_MOV(b, orig_value, rm_value);
		UML_MOV(b, rm_value, 0);
		UML_SUB(b, neg_result, rm_value, orig_value);
		drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP32, 32);
		UML_MOV(b, rm_value, neg_result);
	}

	if (!is_m)
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), rm_value);
	}
	else
	{
		UML_MOV(b, mem_data, rm_value);
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}

	drc_record_cycles(ctx, CYCLES_MOV_REG_REG);

	return true;
}


inline void i386_device::drc_gen_f6_test(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data    = I2;
	const uml::parameter &rm_value    = I0;
	const uml::parameter &test_result = I2;

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		UML_AND(b, rm_value, DRC_GET_MRM_RM8(modrm), 0xff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, rm_value, mem_data, 0xff);
	}
	uint32_t imm = (uint32_t)drc_get_imm8(ctx);
	UML_AND(b, test_result, rm_value, imm);

	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL8);

	drc_record_cycles(ctx, is_m ? CYCLES_ALU_IMM_MEM : CYCLES_ALU_IMM_REG);
}


inline void i386_device::drc_gen_f6_not(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		UML_AND(b, rm_value, DRC_GET_MRM_RM8(modrm), 0xff);
		UML_XOR(b, rm_value, rm_value, 0xff);
		UML_BREG_WRITE(b, MRM_RM8(modrm), rm_value);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, rm_value, mem_data, 0xff);
		UML_XOR(b, rm_value, rm_value, 0xff);
		UML_MOV(b, mem_data, rm_value);
		UML_MOV(b, mem_addr, mem_addr_save);
		UML_CALLH(b, *m_mem_write8);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_NOT_MEM : CYCLES_NOT_REG);
}


inline void i386_device::drc_gen_f6_neg(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I1;
	const uml::parameter &zero_const    = I0;
	const uml::parameter &neg_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		UML_AND(b, rm_value, DRC_GET_MRM_RM8(modrm), 0xff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, rm_value, mem_data, 0xff);
	}
	UML_MOV(b, zero_const, 0);
	UML_SUB(b, neg_result, zero_const, rm_value);

	drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP8, 8);

	if (!is_m)
	{
		UML_BREG_WRITE(b, MRM_RM8(modrm), neg_result);
	}
	else
	{
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write8);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_NEG_MEM : CYCLES_NEG_REG);
}


inline void i386_device::drc_gen_f6_mul(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &al_value     = I1;
	const uml::parameter &product_lo   = I0;
	const uml::parameter &product_hi   = I2;
	const uml::parameter &ah_result    = I1;

	bool is_m = MRM_MOD(modrm) != 3;

	drc_gen_rm8(ctx, modrm);
	UML_AND(b, al_value, DRC_REG8(AL), 0xff);
	UML_MULU(b, product_lo, product_hi, loaded_value, al_value);
	UML_ROLINS(b, DRC_REG32(EAX), product_lo, 0, 0x0000ffff);
	UML_BFXU(b, ah_result, product_lo, 8, 8);

	UML_TEST(b, ah_result, ah_result);
	UML_MOVc(b, COND_NZ, uml::mem(&m_core->CF), 1);
	UML_MOVc(b, COND_Z, uml::mem(&m_core->CF), 0);

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	drc_record_cycles(ctx, is_m ? CYCLES_MUL8_ACC_MEM : CYCLES_MUL8_ACC_REG);
}


inline void i386_device::drc_gen_f6_imul(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &al_value     = I1;
	const uml::parameter &product_lo   = I0;
	const uml::parameter &product_hi   = I2;
	const uml::parameter &lo_sign_ext  = I2;
	const uml::parameter &hi_byte      = I1;

	bool is_m = MRM_MOD(modrm) != 3;

	drc_gen_rm8(ctx, modrm);
	UML_SHL(b, loaded_value, loaded_value, 24);
	UML_SAR(b, loaded_value, loaded_value, 24);
	UML_AND(b, al_value, DRC_REG8(AL), 0xff);
	UML_SHL(b, al_value, al_value, 24);
	UML_SAR(b, al_value, al_value, 24);
	UML_MULS(b, product_lo, product_hi, loaded_value, al_value);
	UML_ROLINS(b, DRC_REG32(EAX), product_lo, 0, 0x0000ffff);

	UML_SHL(b, lo_sign_ext, product_lo, 24);
	UML_SAR(b, lo_sign_ext, lo_sign_ext, 24);
	UML_SAR(b, hi_byte, product_lo, 8);

	UML_CMP(b, hi_byte, lo_sign_ext);
	UML_MOVc(b, COND_NE, uml::mem(&m_core->CF), 1);
	UML_MOVc(b, COND_E, uml::mem(&m_core->CF), 0);

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	drc_record_cycles(ctx, is_m ? CYCLES_IMUL8_ACC_MEM : CYCLES_IMUL8_ACC_REG);
}


inline void i386_device::drc_gen_f6_div(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &divisor      = I1;
	const uml::parameter &dividend     = I0;
	const uml::parameter &quotient     = I2;
	const uml::parameter &remainder    = I3;

	const uml::code_label div_zero = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	bool is_m = MRM_MOD(modrm) != 3;

	drc_gen_rm8(ctx, modrm);
	UML_MOV(b, divisor, loaded_value);
	UML_AND(b, dividend, DRC_REG16(AX), 0xffff);

	UML_CMP(b, divisor, 0);
	UML_JMPc(b, COND_E, div_zero);

	UML_DIVU(b, quotient, remainder, dividend, divisor);

	UML_CMP(b, quotient, 0xff);
	UML_JMPc(b, COND_A, done);

	UML_BREG_WRITE(b, AL, quotient);
	UML_BREG_WRITE(b, AH, remainder);

	if (m_cpuid_id0 != 0x69727943)
		UML_MOV(b, uml::mem(&m_core->CF), 1);
	UML_JMP(b, done);

	UML_LABEL(b, div_zero);
	drc_gen_interpreter_fallback(ctx);

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_DIV8_ACC_MEM : CYCLES_DIV8_ACC_REG);
}


inline void i386_device::drc_gen_f6_idiv(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &divisor      = I1;
	const uml::parameter &dividend     = I0;
	const uml::parameter &quotient     = I2;
	const uml::parameter &remainder    = I3;

	const uml::code_label div_zero = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	bool is_m = MRM_MOD(modrm) != 3;

	drc_gen_rm8(ctx, modrm);
	UML_SHL(b, divisor, loaded_value, 24);
	UML_SAR(b, divisor, divisor, 24);
	UML_AND(b, dividend, DRC_REG16(AX), 0xffff);
	UML_SHL(b, dividend, dividend, 16);
	UML_SAR(b, dividend, dividend, 16);

	UML_CMP(b, divisor, 0);
	UML_JMPc(b, COND_E, div_zero);

	UML_DIVS(b, quotient, remainder, dividend, divisor);

	UML_CMP(b, quotient, 0x7f);
	UML_JMPc(b, COND_G, done);

	UML_CMP(b, quotient, (uint32_t)-0x80);
	UML_JMPc(b, COND_L, done);

	UML_BREG_WRITE(b, AL, quotient);
	UML_BREG_WRITE(b, AH, remainder);
	if (m_cpuid_id0 != 0x69727943)
		UML_MOV(b, uml::mem(&m_core->CF), 1);
	UML_JMP(b, done);

	UML_LABEL(b, div_zero);
	drc_gen_interpreter_fallback(ctx);

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_IDIV8_ACC_MEM : CYCLES_IDIV8_ACC_REG);
}

inline bool i386_device::drc_gen_f6_group(compiler_state &ctx)
{
	uint8_t modrm           = drc_get_modrm(ctx);
	uint8_t group_f6_opcode = ctx.desc->modrm_regop;

	switch (group_f6_opcode)
	{
		case 0:
		case 1:
			drc_gen_f6_test(ctx, modrm);
			break;
		case 2:
			drc_gen_f6_not(ctx, modrm);
			break;
		case 3:
			drc_gen_f6_neg(ctx, modrm);
			break;
		case 4:
			drc_gen_f6_mul(ctx, modrm);
			break;
		case 5:
			drc_gen_f6_imul(ctx, modrm);
			break;
		case 6:
			drc_gen_f6_div(ctx, modrm);
			break;
		case 7:
			drc_gen_f6_idiv(ctx, modrm);
			break;
	}

	return true;
}

bool i386_device::drc_pri_groupF6_8(compiler_state &ctx)
{
	uint8_t group_f6_opcode = ctx.desc->modrm_regop;

	if (group_f6_opcode > 1)
		ctx.invalidate_rscratch();

	if (group_f6_opcode != 2 && group_f6_opcode != 4 && group_f6_opcode != 5)
		drc_gen_clear_flags(ctx);

	return drc_gen_f6_group(ctx);
}

inline bool i386_device::drc_gen_f7_16_test(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &test_lhs = I0;
	const uml::parameter &test_and = I2;

	drc_gen_rm16(ctx, modrm);
	uint32_t imm = (uint32_t)drc_get_imm16(ctx) & 0xffff;
	UML_AND(b, test_and, test_lhs, imm);
	drc_gen_defer_flags_logical(ctx, FLAGS_OPTYPE_LOGICAL16);

	drc_record_cycles(ctx, MRM_MOD(modrm) != 3 ? CYCLES_TEST_REG_MEM : CYCLES_TEST_REG_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_not(compiler_state &ctx, uint8_t rm16, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;

	UML_XOR(b, rm_value, rm_value, 0xffff);
	if (!is_m)
	{
		UML_ROLINS(b, DRC_REG32(rm16), rm_value, 0, 0xffff);
	}
	else
	{
		UML_AND(b, mem_data, rm_value, 0xffff);
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_NOT_MEM : CYCLES_NOT_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_neg(compiler_state &ctx, uint8_t rm16, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;
	const uml::parameter &orig_value    = I1;
	const uml::parameter &zero_const    = I0;
	const uml::parameter &neg_result    = I2;

	UML_MOV(b, orig_value, rm_value);
	UML_MOV(b, zero_const, 0);
	UML_SUB(b, neg_result, zero_const, orig_value);

	drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP16, 16);

	if (!is_m)
	{
		UML_ROLINS(b, DRC_REG32(rm16), neg_result, 0, 0xffff);
	}
	else
	{
		UML_AND(b, neg_result, neg_result, 0xffff);
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_NOT_MEM : CYCLES_NOT_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_mul(compiler_state &ctx, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value   = I0;
	const uml::parameter &ax_value   = I3;
	const uml::parameter &product_lo = I1;
	const uml::parameter &product_hi = I2;
	const uml::parameter &hi_word    = I3;

	UML_AND(b, ax_value, DRC_REG32(EAX), 0xffff);
	UML_MULU(b, product_lo, product_hi, ax_value, rm_value);
	UML_ROLINS(b, DRC_REG32(EAX), product_lo, 0, 0xffff);
	UML_SHR(b, hi_word, product_lo, 16);
	UML_ROLINS(b, DRC_REG32(EDX), hi_word, 0, 0xffff);

	UML_TEST(b, hi_word, 0xffff);
	UML_SETc(b, COND_NZ, uml::mem(&m_core->CF));

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	drc_record_cycles(ctx, is_m ? CYCLES_MUL32_ACC_MEM : CYCLES_MUL32_ACC_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_imul(compiler_state &ctx, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value   = I0;
	const uml::parameter &ax_value   = I3;
	const uml::parameter &product_lo = I1;
	const uml::parameter &product_hi = I2;
	const uml::parameter &hi_check   = I3;

	UML_AND(b, ax_value, DRC_REG32(EAX), 0xffff);
	UML_SHL(b, ax_value, ax_value, 16);
	UML_SAR(b, ax_value, ax_value, 16);
	UML_SHL(b, rm_value, rm_value, 16);
	UML_SAR(b, rm_value, rm_value, 16);
	UML_MULS(b, product_lo, product_hi, ax_value, rm_value);
	UML_ROLINS(b, DRC_REG32(EAX), product_lo, 0, 0xffff);
	UML_SAR(b, product_hi, product_lo, 16);
	UML_ROLINS(b, DRC_REG32(EDX), product_hi, 0, 0xffff);

	UML_SAR(b, hi_check, product_lo, 15);
	UML_SHR(b, hi_check, hi_check, 16);
	UML_AND(b, product_hi, product_hi, 0xffff);

	UML_CMP(b, product_hi, hi_check);
	UML_SETc(b, COND_NE, uml::mem(&m_core->CF));

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	drc_record_cycles(ctx, is_m ? CYCLES_IMUL32_ACC_MEM : CYCLES_IMUL32_ACC_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_div(compiler_state &ctx, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value  = I0;
	const uml::parameter &divisor   = I1;
	const uml::parameter &dividend  = I0;
	const uml::parameter &quotient  = I2;
	const uml::parameter &remainder = I3;

	const uml::code_label div_zero = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	UML_MOV(b, divisor, rm_value);
	UML_AND(b, quotient, DRC_REG16(DX), 0xffff);
	UML_SHL(b, quotient, quotient, 16);
	UML_AND(b, dividend, DRC_REG16(AX), 0xffff);
	UML_OR(b, dividend, dividend, quotient);

	UML_CMP(b, divisor, 0);
	UML_JMPc(b, COND_E, div_zero);

	UML_DIVU(b, quotient, remainder, dividend, divisor);

	UML_CMP(b, quotient, 0xffff);
	UML_JMPc(b, COND_A, done);

	UML_ROLINS(b, DRC_REG32(EAX), quotient, 0, 0xffff);
	UML_ROLINS(b, DRC_REG32(EDX), remainder, 0, 0xffff);

	if (m_cpuid_id0 != 0x69727943) // != Cyrix CPU
		UML_MOV(b, uml::mem(&m_core->CF), 1);

	UML_JMP(b, done);

	UML_LABEL(b, div_zero);
	drc_gen_interpreter_fallback(ctx);

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_DIV16_ACC_MEM : CYCLES_DIV16_ACC_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_16_idiv(compiler_state &ctx, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value  = I0;
	const uml::parameter &divisor   = I1;
	const uml::parameter &dividend  = I0;
	const uml::parameter &quotient  = I2;
	const uml::parameter &remainder = I3;

	const uml::code_label div_zero = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	UML_SHL(b, divisor, rm_value, 16);
	UML_SAR(b, divisor, divisor, 16);
	UML_AND(b, quotient, DRC_REG16(DX), 0xffff);
	UML_SHL(b, quotient, quotient, 16);
	UML_AND(b, dividend, DRC_REG16(AX), 0xffff);
	UML_OR(b, dividend, dividend, quotient);

	UML_CMP(b, divisor, 0);
	UML_JMPc(b, COND_E, div_zero);

	UML_DIVS(b, quotient, remainder, dividend, divisor);

	UML_CMP(b, quotient, 0x7fff);
	UML_JMPc(b, COND_G, done);

	UML_CMP(b, quotient, (uint32_t)-0x8000);
	UML_JMPc(b, COND_L, done);

	UML_ROLINS(b, DRC_REG32(EAX), quotient, 0, 0xffff);
	UML_ROLINS(b, DRC_REG32(EDX), remainder, 0, 0xffff);

	if (m_cpuid_id0 != 0x69727943) // != Cyrix CPU
		UML_MOV(b, uml::mem(&m_core->CF), 1);

	UML_JMP(b, done);

	UML_LABEL(b, div_zero);
	drc_gen_interpreter_fallback(ctx);

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_IDIV16_ACC_MEM : CYCLES_IDIV16_ACC_REG);

	return true;
}

inline bool i386_device::drc_gen_f7_group16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm           = drc_get_modrm(ctx);
	uint8_t group_f7_opcode = MRM_OPCODE(modrm);

	if (group_f7_opcode == 0)
		return drc_gen_f7_16_test(ctx, modrm);


	uint8_t rm16 = MRM_RM32(modrm);
	bool    is_m = ctx.desc->is_mem;
	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read16);
		UML_AND(b, rm_value, mem_data, 0xffff);
	}
	else
	{
		UML_AND(b, rm_value, DRC_REG32(rm16), 0xffff);
	}

	switch (group_f7_opcode)
	{
		case 2:
			return drc_gen_f7_16_not(ctx, rm16, is_m);
		case 3:
			return drc_gen_f7_16_neg(ctx, rm16, is_m);
		case 4:
			return drc_gen_f7_16_mul(ctx, is_m);
		case 5:
			return drc_gen_f7_16_imul(ctx, is_m);
		case 6:
			return drc_gen_f7_16_div(ctx, is_m);
		case 7:
			return drc_gen_f7_16_idiv(ctx, is_m);
	}

	return false;
}

bool i386_device::drc_pri_groupF7_16(compiler_state &ctx)
{
	uint8_t group_f7_opcode = ctx.desc->modrm_regop;

	if (group_f7_opcode > 1)
		ctx.invalidate_rscratch();

	if (group_f7_opcode != 2 && group_f7_opcode != 4 && group_f7_opcode != 5)
		drc_gen_clear_flags(ctx);

	return drc_gen_f7_group16(ctx);
}

inline bool i386_device::drc_gen_mul32_acc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &multiplicand = I0;
	const uml::parameter &product_lo   = I1;
	const uml::parameter &product_hi   = I2;
	const uml::parameter &lo_sign_ext  = I3;

	uint8_t modrm           = drc_get_modrm(ctx);
	uint8_t group_f7_opcode = MRM_OPCODE(modrm);
	bool    is_m            = MRM_MOD(modrm) != 3;

	drc_gen_rm32(ctx, modrm);

	if (group_f7_opcode == 4)
	{
		UML_MULU(b, product_lo, product_hi, DRC_REG32(EAX), multiplicand);
		UML_MOV(b, DRC_REG32(EAX), product_lo);
		UML_MOV(b, DRC_REG32(EDX), product_hi);

		UML_CMP(b, product_hi, 0);
		UML_SETc(b, COND_NZ, uml::mem(&m_core->CF));

		UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

		if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
			UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

		drc_record_cycles(ctx, is_m ? CYCLES_MUL32_ACC_MEM : CYCLES_MUL32_ACC_REG);
	}
	else
	{
		UML_MULS(b, product_lo, product_hi, DRC_REG32(EAX), multiplicand);
		UML_MOV(b, DRC_REG32(EAX), product_lo);
		UML_MOV(b, DRC_REG32(EDX), product_hi);

		UML_SAR(b, lo_sign_ext, product_lo, 31);

		UML_CMP(b, lo_sign_ext, product_hi);
		UML_SETc(b, COND_NZ, uml::mem(&m_core->CF));

		UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

		if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
			UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

		drc_record_cycles(ctx, is_m ? CYCLES_IMUL32_ACC_MEM : CYCLES_IMUL32_ACC_REG);
	}
	return true;
}

inline bool i386_device::drc_gen_div32_acc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &divisor   = I1;
	const uml::parameter &dividend  = I0;
	const uml::parameter &edx_high  = I2;
	const uml::parameter &quotient  = I2;
	const uml::parameter &remainder = I3;

	const uml::code_label div_zero = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	uint8_t modrm           = drc_get_modrm(ctx);
	uint8_t group_f7_opcode = MRM_OPCODE(modrm);
	bool    is_m            = MRM_MOD(modrm) != 3;

	drc_gen_rm32(ctx, modrm);
	UML_MOV(b, divisor, dividend);
	UML_MOV(b, dividend, DRC_REG32(EAX));
	UML_MOV(b, edx_high, DRC_REG32(EDX));
	UML_DSHL(b, edx_high, edx_high, 32);
	UML_DOR(b, dividend, dividend, edx_high);

	if (group_f7_opcode == 6)
	{
		UML_CMP(b, divisor, 0);
		UML_JMPc(b, COND_E, div_zero);

		UML_DDIVU(b, quotient, remainder, dividend, divisor);

		UML_DCMP(b, quotient, 0xffffffffULL);
		UML_JMPc(b, COND_A, done);

		UML_MOV(b, DRC_REG32(EAX), quotient);
		UML_MOV(b, DRC_REG32(EDX), remainder);
		UML_JMP(b, done);

		UML_LABEL(b, div_zero);
		drc_gen_interpreter_fallback(ctx);

		UML_LABEL(b, done);

		drc_record_cycles(ctx, is_m ? CYCLES_DIV32_ACC_MEM : CYCLES_DIV32_ACC_REG);
	}
	else
	{
		UML_CMP(b, divisor, 0);
		UML_JMPc(b, COND_E, div_zero);

		UML_DSHL(b, divisor, divisor, 32);
		UML_DSAR(b, divisor, divisor, 32);

		UML_DDIVS(b, quotient, remainder, dividend, divisor);

		UML_DCMP(b, quotient, 0x7fffffffULL);
		UML_JMPc(b, COND_G, done);

		UML_DCMP(b, quotient, 0xffffffff80000000ULL);
		UML_JMPc(b, COND_L, done);

		UML_MOV(b, DRC_REG32(EAX), quotient);
		UML_MOV(b, DRC_REG32(EDX), remainder);
		UML_JMP(b, done);

		UML_LABEL(b, div_zero);
		drc_gen_interpreter_fallback(ctx);

		UML_LABEL(b, done);

		drc_record_cycles(ctx, is_m ? CYCLES_IDIV32_ACC_MEM : CYCLES_IDIV32_ACC_REG);
	}

	return true;
}

bool i386_device::drc_pri_groupF7_32(compiler_state &ctx)
{
	uint8_t group_f7_opcode = ctx.desc->modrm_regop;

	if (group_f7_opcode > 1)
		ctx.invalidate_rscratch();

	switch (group_f7_opcode)
	{
		case 0:
			drc_gen_clear_flags(ctx);
			return drc_pri_test32(ctx);
		case 2:
			return drc_gen_not_neg32(ctx);
		case 3:
			drc_gen_clear_flags(ctx);
			return drc_gen_not_neg32(ctx);
		case 4:
		case 5:
			return drc_gen_mul32_acc(ctx);
		case 6:
		case 7:
			drc_gen_clear_flags(ctx);
			return drc_gen_div32_acc(ctx);
		default:
			return drc_gen_interpreter_fallback(ctx);
	}
}

// ----------------------------------------------------------------------------
// IMUL
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_imul_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &product_hi   = I1;
	const uml::parameter &lo_sign_ext  = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);

	drc_gen_rm32(ctx, modrm);

	// 0x69 IMUL r32, r/m32, imm32
	if (ctx.desc->opcode0 == 0x69)
	{
		uint32_t imm = drc_get_imm32(ctx);
		UML_MULS(b, loaded_value, product_hi, loaded_value, imm);
	}
	// 0x6b IMUL r32, r/m32, imm8 (sign extended)
	else if (ctx.desc->opcode0 == 0x6b)
	{
		uint32_t imm = (uint32_t)(int32_t)(int8_t)drc_get_imm8(ctx);
		UML_MULS(b, loaded_value, product_hi, loaded_value, imm);
	}
	else
	{
		UML_MULS(b, loaded_value, product_hi, loaded_value, DRC_REG32(reg_d));
	}

	UML_SAR(b, lo_sign_ext, loaded_value, 31);

	UML_CMP(b, lo_sign_ext, product_hi);
	UML_MOVc(b, COND_NZ, uml::mem(&m_core->CF), 1);
	UML_MOVc(b, COND_Z, uml::mem(&m_core->CF), 0);

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	UML_MOV(b, DRC_REG32(reg_d), loaded_value);

	drc_record_cycles(ctx, CYCLES_IMUL32_REG_REG);

	return true;
}

bool i386_device::drc_x0f_imul(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &product_hi   = I1;
	const uml::parameter &lo_sign_ext  = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);

	drc_gen_rm32(ctx, modrm);

	UML_MULS(b, loaded_value, product_hi, loaded_value, DRC_REG32(reg_d));

	UML_SAR(b, lo_sign_ext, loaded_value, 31);

	UML_CMP(b, lo_sign_ext, product_hi);
	UML_MOVc(b, COND_NZ, uml::mem(&m_core->CF), 1);
	UML_MOVc(b, COND_Z, uml::mem(&m_core->CF), 0);

	UML_MOV(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF));

	if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

	UML_MOV(b, DRC_REG32(reg_d), loaded_value);

	drc_record_cycles(ctx, CYCLES_IMUL32_REG_REG);

	return true;
}

// ----------------------------------------------------------------------------
// Shift/rotate group (SHL/SHR/SAR/ROL/ROR/RCL/RCR)
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_shift8(compiler_state &ctx)
{
	return drc_gen_shift_group(ctx, 8);
}

bool i386_device::drc_pri_shift16(compiler_state &ctx)
{
	return drc_gen_shift_group(ctx, 16);
}

bool i386_device::drc_pri_shift32(compiler_state &ctx)
{
	return drc_gen_shift_group(ctx, 32);
}

inline bool i386_device::drc_gen_shift_group_load(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t &rm_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;

	rm_reg = MRM_RM32(modrm);

	bool is_m = ctx.desc->is_mem;
	if (!is_m)
	{
		switch (width_bits)
		{
			case 8:
				UML_AND(b, rm_value, DRC_GET_MRM_RM8(modrm), 0xff);
				break;
			case 16:
				UML_AND(b, rm_value, DRC_REG32(rm_reg), 0xffff);
				break;
			case 32:
			default:
				UML_MOV(b, rm_value, DRC_REG32(rm_reg));
				break;
		}
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		drc_flush_cycles(ctx);
		UML_MOV(b, mem_addr_save, mem_addr);
		switch (width_bits)
		{
			case 8:
				UML_CALLH(b, *m_mem_read8);
				UML_AND(b, rm_value, mem_data, 0xff);
				break;
			case 16:
				UML_CALLH(b, *m_mem_read16);
				UML_AND(b, rm_value, mem_data, 0xffff);
				break;
			case 32:
			default:
				UML_CALLH(b, *m_mem_read32);
				UML_MOV(b, rm_value, mem_data);
				break;
		}
	}

	return is_m;
}


inline void i386_device::drc_gen_shift_group_store(compiler_state &ctx, uint8_t modrm, int width_bits, bool is_m, uint8_t rm_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I0;
	const uml::parameter &mem_addr_save = I6;

	if (!is_m)
	{
		switch (width_bits)
		{
			case 8:
				UML_BREG_WRITE(b, MRM_RM8(modrm), rm_value);
				break;
			case 16:
				UML_ROLINS(b, DRC_REG32(rm_reg), rm_value, 0, 0xffff);
				break;
			case 32:
			default:
				UML_MOV(b, DRC_REG32(rm_reg), rm_value);
				break;
		}
	}
	else
	{
		drc_flush_cycles(ctx);
		switch (width_bits)
		{
			case 8:
				UML_MOV(b, mem_data, rm_value);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write8);
				break;
			case 16:
				UML_AND(b, mem_data, rm_value, 0xffff);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write16);
				break;
			case 32:
			default:
				UML_MOV(b, mem_data, rm_value);
				UML_MOV(b, mem_addr, mem_addr_save);
				UML_CALLH(b, *m_mem_write32);
				break;
		}
	}
}

inline bool i386_device::drc_gen_shift_group_rotate_carry(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t shift_opcode)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value         = I0;
	const uml::parameter &rotated_value    = I2;
	const uml::parameter &result_msb       = I3;
	const uml::parameter &result_bit_hi2   = I4;
	const uml::parameter &carry_in_shifted = I3;

	// 0xd0/0xd1 shift-by-1 form (count is a compile-time constant); the 0xc0/0xc1 imm8
	// and 0xd2/0xd3 CL forms share the count-based path below
	if (ctx.desc->opcode0 == 0xd0 || ctx.desc->opcode0 == 0xd1)
	{
		uint8_t rm_reg;
		bool    is_m = drc_gen_shift_group_load(ctx, modrm, width_bits, rm_reg);

		if (shift_opcode == 2)
		{
			UML_SHL(b, rotated_value, rm_value, 1);
			UML_OR(b, rotated_value, rotated_value, uml::mem(&m_core->CF));
			UML_BFXU(b, uml::mem(&m_core->CF), rm_value, width_bits - 1, 1);
			UML_BFXU(b, result_msb, rotated_value, width_bits - 1, 1);
			UML_XOR(b, uml::mem(&m_core->OF), uml::mem(&m_core->CF), result_msb);
			UML_MOV(b, rm_value, rotated_value);
		}
		else
		{
			UML_SHL(b, carry_in_shifted, uml::mem(&m_core->CF), width_bits - 1);
			UML_SHR(b, rotated_value, rm_value, 1);
			UML_OR(b, rotated_value, rotated_value, carry_in_shifted);
			UML_BFXU(b, result_msb, rotated_value, width_bits - 1, 1);
			UML_BFXU(b, result_bit_hi2, rotated_value, width_bits - 2, 1);
			UML_AND(b, uml::mem(&m_core->CF), rm_value, 1);
			UML_XOR(b, uml::mem(&m_core->OF), result_msb, result_bit_hi2);
			UML_MOV(b, rm_value, rotated_value);
		}

		if (m_drc_options & I386DRC_LAZY_FLAGS && !debugger_enabled())
			UML_MOV(b, uml::mem(&m_core->flags_of_direct), 1);

		drc_gen_shift_group_store(ctx, modrm, width_bits, is_m, rm_reg);

		drc_record_cycles(ctx, is_m ? CYCLES_ROTATE_CARRY_MEM : CYCLES_ROTATE_CARRY_REG);

		return true;
	}
	else
	{
		return drc_gen_interpreter_fallback(ctx);
	}
}

inline bool i386_device::drc_gen_shift_group_count(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t shift_opcode)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value    = I0;
	const uml::parameter &shift_count = I1;
	const uml::parameter &shift_temp  = I2;
	const uml::parameter &shift_compl = I3;

	const uml::code_label done = NEW_LBL(ctx);

	const uint32_t width_mask = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);

	uint8_t rm_reg;
	bool    is_m = drc_gen_shift_group_load(ctx, modrm, width_bits, rm_reg);

	switch (ctx.desc->opcode0)
	{
		// 0xd0/0xd1 shift/rotate by 1
		case 0xd0:
		case 0xd1:
			UML_MOV(b, shift_count, 1);
			break;
		// 0xd2/0xd3 shift/rotate by CL
		case 0xd2:
		case 0xd3:
			UML_AND(b, shift_count, DRC_REG8(CL), 0x1f);
			break;
		// 0xc0/0xc1 shift/rotate by imm8
		default:
			UML_MOV(b, shift_count, (uint32_t)(drc_get_imm8(ctx) & 0x1f));
			break;
	}

	if (width_bits < 32 && (shift_opcode == 0 || shift_opcode == 1))
		UML_AND(b, shift_count, shift_count, width_bits - 1);

	UML_TEST(b, shift_count, shift_count);
	UML_JMPc(b, COND_Z, done);

	if (width_bits == 32)
	{
		switch (shift_opcode)
		{
			case 0:
				UML_ROL(b, rm_value, rm_value, shift_count);
				break;
			case 1:
				UML_ROR(b, rm_value, rm_value, shift_count);
				break;
			case 4:
			case 6:
				UML_SHL(b, rm_value, rm_value, shift_count);
				break;
			case 5:
				UML_SHR(b, rm_value, rm_value, shift_count);
				break;
			case 7:
				UML_SAR(b, rm_value, rm_value, shift_count);
				break;
			default:
				return drc_gen_interpreter_fallback(ctx);
		}

		if (shift_opcode == 0 || shift_opcode == 1)
			drc_gen_flags_rotate(b);
		else
			drc_gen_defer_flags_shift(ctx, FLAGS_OPTYPE_SHIFT32, 32);
	}
	else
	{
		switch (shift_opcode)
		{
			case 0:
				UML_SHL(b, shift_temp, rm_value, shift_count);
				UML_SUB(b, shift_compl, width_bits, shift_count);
				UML_SHR(b, rm_value, rm_value, shift_compl);
				UML_OR(b, rm_value, shift_temp, rm_value);
				UML_AND(b, rm_value, rm_value, width_mask);
				UML_AND(b, uml::mem(&m_core->CF), rm_value, 1);
				break;
			case 1:
				UML_SHR(b, shift_temp, rm_value, shift_count);
				UML_SUB(b, shift_compl, width_bits, shift_count);
				UML_SHL(b, rm_value, rm_value, shift_compl);
				UML_OR(b, rm_value, shift_temp, rm_value);
				UML_AND(b, rm_value, rm_value, width_mask);
				UML_BFXU(b, uml::mem(&m_core->CF), rm_value, width_bits - 1, 1);
				break;
			case 4:
			case 6:
				UML_SHL(b, rm_value, rm_value, shift_count);
				UML_BFXU(b, uml::mem(&m_core->CF), rm_value, width_bits, 1);
				UML_AND(b, rm_value, rm_value, width_mask);
				break;
			case 5:
				UML_SUB(b, shift_temp, shift_count, 1);
				UML_SHR(b, shift_temp, rm_value, shift_temp);
				UML_AND(b, uml::mem(&m_core->CF), shift_temp, 1);
				UML_SHR(b, rm_value, rm_value, shift_count);
				UML_AND(b, rm_value, rm_value, width_mask);
				break;
			case 7:
				UML_SHL(b, rm_value, rm_value, 32 - width_bits);
				UML_SAR(b, rm_value, rm_value, 32 - width_bits);
				UML_SUB(b, shift_temp, shift_count, 1);
				UML_SAR(b, shift_temp, rm_value, shift_temp);
				UML_AND(b, uml::mem(&m_core->CF), shift_temp, 1);
				UML_SAR(b, rm_value, rm_value, shift_count);
				UML_AND(b, rm_value, rm_value, width_mask);
				break;
		}

		if (shift_opcode != 0 && shift_opcode != 1)
			drc_gen_defer_flags_shift(ctx, (width_bits == 8) ? FLAGS_OPTYPE_SHIFT8 : FLAGS_OPTYPE_SHIFT16, width_bits);
	}

	UML_LABEL(b, done);

	drc_gen_shift_group_store(ctx, modrm, width_bits, is_m, rm_reg);

	if (width_bits == 32)
		drc_record_cycles(ctx, CYCLES_MOV_REG_REG);
	else
		drc_record_cycles(ctx, is_m ? CYCLES_ROTATE_MEM : CYCLES_ROTATE_REG);

	return true;
}

inline bool i386_device::drc_gen_shift_group(compiler_state &ctx, int width_bits)
{
	uint8_t modrm        = drc_get_modrm(ctx);
	uint8_t shift_opcode = MRM_OPCODE(modrm);

	switch (shift_opcode)
	{
		case 2:
		case 3:
			return drc_gen_shift_group_rotate_carry(ctx, modrm, width_bits, shift_opcode);

		default:
			return drc_gen_shift_group_count(ctx, modrm, width_bits, shift_opcode);
	}
}

// ----------------------------------------------------------------------------
// Bit test group (BT/BTS/BTR/BTC), SHLD/SHRD, BSF/BSR, XADD/CMPXCHG
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_bt_apply_modify(compiler_state &ctx, uint8_t bt_opcode, const uml::parameter &new_value, const uml::parameter &bit_mask, const uml::parameter &bit_pos)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, bit_mask, 1);
	UML_SHL(b, bit_mask, bit_mask, bit_pos);
	if (bt_opcode == 5)
	{
		UML_OR(b, new_value, new_value, bit_mask);
	}
	else if (bt_opcode == 6)
	{
		UML_XOR(b, bit_mask, bit_mask, 0xffffffff);
		UML_AND(b, new_value, new_value, bit_mask);
	}
	else
	{
		UML_XOR(b, new_value, new_value, bit_mask);
	}
}


inline int i386_device::drc_gen_bt_cycles(uint8_t bt_opcode, bool is_m, bool is_imm) const
{
	if (is_m)
	{
		switch (bt_opcode)
		{
			case 4: // BT
				return CYCLES_BT_IMM_MEM;
			case 5: // BTS
				return CYCLES_BTS_IMM_MEM;
			case 6: // BTR
				return CYCLES_BTR_IMM_MEM;
			case 7: // BTC
			default:
				return CYCLES_BTC_IMM_MEM;
		}
	}
	else
	{
		switch (bt_opcode)
		{
			case 4: // BT
				return CYCLES_BT_REG_REG;
			case 5: // BTS
				return (is_imm ? CYCLES_BTS_IMM_REG : CYCLES_BTS_REG_REG);
			case 6: // BTR
				return (is_imm ? CYCLES_BTR_IMM_REG : CYCLES_BTR_REG_REG);
			case 7: // BTC
			default:
				return (is_imm ? CYCLES_BTC_IMM_REG : CYCLES_BTC_REG_REG);
		}
	}
}

inline bool i386_device::drc_gen_bt_group_mem_dynamic(compiler_state &ctx, uint8_t modrm, uint8_t bt_opcode)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &bit_index_src = I6;
	const uml::parameter &word_offset   = I1;
	const uml::parameter &bit_pos       = I1;
	const uml::parameter &ea_result     = I0;
	const uml::parameter &adjusted_addr = I5;
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &loaded_value  = I0;
	const uml::parameter &shifted_bit   = I2;
	const uml::parameter &bit_mask      = I2;
	const uml::parameter &new_value     = I0;

	UML_MOV(b, bit_index_src, DRC_GET_MRM_REG32(modrm));
	(this->*ctx.gen_ea)(ctx);
	UML_SAR(b, word_offset, bit_index_src, 5);
	UML_SHL(b, word_offset, word_offset, 2);
	UML_ADD(b, adjusted_addr, ea_result, word_offset);
	UML_MOV(b, mem_addr, adjusted_addr);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, loaded_value, mem_data);
	UML_AND(b, bit_pos, bit_index_src, 0x1f);
	UML_SHR(b, shifted_bit, loaded_value, bit_pos);
	UML_AND(b, uml::mem(&m_core->CF), shifted_bit, 1);

	if (bt_opcode != 4)
	{
		drc_gen_bt_apply_modify(ctx, bt_opcode, new_value, bit_mask, bit_pos);
		UML_MOV(b, mem_data, new_value);
		UML_MOV(b, mem_addr, adjusted_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}

	drc_record_cycles(ctx, drc_gen_bt_cycles(bt_opcode, true, false));

	return true;
}

inline bool i386_device::drc_gen_bt_group_general(compiler_state &ctx, uint8_t modrm, uint8_t bt_opcode, bool is_imm, bool is_m)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &rm_value      = I0;
	const uml::parameter &bit_pos       = I1;
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &loaded_value  = I0;
	const uml::parameter &mem_addr_save = I6;
	const uml::parameter &shifted_bit   = I2;
	const uml::parameter &bit_mask      = I2;
	const uml::parameter &new_value     = I0;

	uint8_t rm32 = 0;
	if (is_imm)
	{
		bool mem = ctx.desc->is_mem;
		if (!mem)
		{
			rm32 = MRM_RM32(modrm);
			UML_MOV(b, rm_value, DRC_REG32(rm32));
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_MOV(b, mem_addr_save, mem_addr);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, loaded_value, mem_data);
		}
		UML_MOV(b, bit_pos, (uint32_t)(drc_get_imm8(ctx) & 0x1f));
		is_m = mem;
	}
	else
	{

		rm32 = MRM_RM32(modrm);
		UML_MOV(b, rm_value, DRC_REG32(rm32));
		UML_AND(b, bit_pos, DRC_GET_MRM_REG32(modrm), 0x1f);
	}

	UML_SHR(b, shifted_bit, rm_value, bit_pos);
	UML_AND(b, uml::mem(&m_core->CF), shifted_bit, 1);

	if (bt_opcode != 4)
	{
		drc_gen_bt_apply_modify(ctx, bt_opcode, new_value, bit_mask, bit_pos);

		if (is_m)
		{
			UML_MOV(b, mem_data, new_value);
			UML_MOV(b, mem_addr, mem_addr_save);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write32);
		}
		else
		{
			UML_MOV(b, DRC_REG32(rm32), new_value);
		}
	}

	drc_record_cycles(ctx, drc_gen_bt_cycles(bt_opcode, is_m, is_imm));

	return true;
}

bool i386_device::drc_x0f_bt_group(compiler_state &ctx)
{
	uint8_t modrm = drc_get_modrm(ctx);
	// 0x0f 0xba BT/BTS/BTR/BTC r/m, imm8
	bool is_imm   = (ctx.desc->opcode1 == 0xba);

	uint8_t bt_opcode;
	if (is_imm)
	{
		bt_opcode = MRM_OPCODE(modrm);
		if (bt_opcode < 4)
			return false;
	}
	else
	{
		// 0x0f 0xa3 BT r/m, r · 0xab BTS r/m, r · 0xb3 BTR r/m, r · 0xbb BTC r/m, r
		bt_opcode = (ctx.desc->opcode1 == 0xa3) ? 4 : (ctx.desc->opcode1 == 0xab) ? 5 : (ctx.desc->opcode1 == 0xb3) ? 6 : 7;
	}

	bool is_m = MRM_MOD(modrm) != 3;

	if (!is_imm && is_m)
		return drc_gen_bt_group_mem_dynamic(ctx, modrm, bt_opcode);

	return drc_gen_bt_group_general(ctx, modrm, bt_opcode, is_imm, is_m);
}

bool i386_device::drc_x0f_shld(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &dst_value     = I0;
	const uml::parameter &src_value     = I2;
	const uml::parameter &shift_result  = I2;
	const uml::parameter &mem_addr_save = I6;
	const uml::parameter &shift_count   = I1;
	const uml::parameter &shift_scratch = I3;

	const uml::code_label done = NEW_LBL(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	// 0x0f 0xa5 SHLD r/m, r, CL; 0xa4 SHLD r/m, r, imm8
	bool is_cl    = (ctx.desc->opcode1 == 0xa5);

	bool is_m = ctx.desc->is_mem;

	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
	}

	if (is_cl)
		UML_AND(b, shift_count, DRC_REG8(CL), 0x1f);
	else
		UML_MOV(b, shift_count, (uint32_t)(drc_get_imm8(ctx) & 0x1f));

	if (!is_m)
	{
		UML_MOV(b, dst_value, DRC_REG32(MRM_RM32(modrm)));
	}
	else
	{

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, dst_value, mem_data);
	}

	UML_MOV(b, src_value, DRC_GET_MRM_REG32(modrm));

	UML_CMP(b, shift_count, 0);
	UML_JMPc(b, COND_Z, done);

	UML_SUB(b, shift_scratch, 32, shift_count);
	UML_SHR(b, shift_scratch, dst_value, shift_scratch);
	UML_AND(b, uml::mem(&m_core->CF), shift_scratch, 1);

	UML_SUB(b, shift_scratch, 32, shift_count);
	UML_SHR(b, shift_scratch, src_value, shift_scratch);
	UML_SHL(b, shift_result, dst_value, shift_count);
	UML_OR(b, shift_result, shift_result, shift_scratch);

	UML_SAR(b, shift_scratch, shift_result, 31);
	UML_AND(b, uml::mem(&m_core->SF), shift_scratch, 1);

	UML_CMP(b, shift_result, 0);
	UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

	drc_gen_set_pf(b, shift_result);

	UML_XOR(b, shift_scratch, dst_value, shift_result);
	UML_BFXU(b, uml::mem(&m_core->OF), shift_scratch, 31, 1);

	if (!is_m)
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), shift_result);
	}
	else
	{

		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_SHLD_MEM : CYCLES_SHLD_REG);

	return true;
}

bool i386_device::drc_x0f_shrd(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &dst_value     = I0;
	const uml::parameter &src_value     = I2;
	const uml::parameter &shift_result  = I2;
	const uml::parameter &mem_addr_save = I6;
	const uml::parameter &shift_count   = I1;
	const uml::parameter &shift_scratch = I3;

	uint8_t modrm = drc_get_modrm(ctx);
	// 0x0f 0xad SHRD r/m, r, CL; 0xac SHRD r/m, r, imm8
	bool is_cl    = (ctx.desc->opcode1 == 0xad);

	bool is_m = ctx.desc->is_mem;

	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
	}

	if (is_cl)
		UML_AND(b, shift_count, DRC_REG8(CL), 0x1f);
	else
		UML_MOV(b, shift_count, (uint32_t)(drc_get_imm8(ctx) & 0x1f));

	if (!is_m)
	{
		UML_MOV(b, dst_value, DRC_REG32(MRM_RM32(modrm)));
	}
	else
	{

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, dst_value, mem_data);
	}

	UML_MOV(b, src_value, DRC_GET_MRM_REG32(modrm));

	const uml::code_label done = NEW_LBL(ctx);

	UML_CMP(b, shift_count, 0);
	UML_JMPc(b, COND_Z, done);

	UML_SUB(b, shift_scratch, shift_count, 1);
	UML_SHR(b, shift_scratch, dst_value, shift_scratch);
	UML_AND(b, uml::mem(&m_core->CF), shift_scratch, 1);

	UML_SUB(b, shift_scratch, 32, shift_count);
	UML_SHL(b, shift_scratch, src_value, shift_scratch);
	UML_SHR(b, shift_result, dst_value, shift_count);
	UML_OR(b, shift_result, shift_result, shift_scratch);

	UML_SAR(b, shift_scratch, shift_result, 31);
	UML_AND(b, uml::mem(&m_core->SF), shift_scratch, 1);

	UML_CMP(b, shift_result, 0);
	UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

	drc_gen_set_pf(b, shift_result);

	UML_XOR(b, shift_scratch, dst_value, shift_result);
	UML_BFXU(b, uml::mem(&m_core->OF), shift_scratch, 31, 1);

	if (!is_m)
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), shift_result);
	}
	else
	{
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}

	UML_LABEL(b, done);

	drc_record_cycles(ctx, is_m ? CYCLES_SHRD_MEM : CYCLES_SHRD_REG);

	return true;
}

bool i386_device::drc_x0f_bsf_bsr(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;

	const uml::code_label skip = NEW_LBL(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);
	// 0x0f 0xbc BSF r, r/m; 0xbd BSR r, r/m
	bool is_bsf   = (ctx.desc->opcode1 == 0xbc);

	drc_gen_rm32(ctx, modrm);

	UML_CMP(b, loaded_value, 0);
	UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

	UML_JMPc(b, COND_Z, skip);

	if (is_bsf)
	{
		UML_TZCNT(b, DRC_REG32(reg_d), loaded_value);
	}
	else
	{
		UML_LZCNT(b, DRC_REG32(reg_d), loaded_value);
		UML_XOR(b, DRC_REG32(reg_d), DRC_REG32(reg_d), 31);
	}
	UML_LABEL(b, skip);

	drc_record_cycles(ctx, is_bsf ? CYCLES_BSF_BASE : CYCLES_BSR_BASE);

	return true;
}

bool i386_device::drc_x0f_xadd(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &loaded_value  = I0;
	const uml::parameter &reg_value     = I1;
	const uml::parameter &sum_result    = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);

	bool is_m = ctx.desc->is_mem;
	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, loaded_value, mem_data);
	}
	else
	{
		UML_MOV(b, loaded_value, DRC_REG32(MRM_RM32(modrm)));
	}

	UML_MOV(b, reg_value, DRC_REG32(reg_d));
	UML_MOV(b, DRC_REG32(reg_d), loaded_value);
	UML_ADD(b, sum_result, loaded_value, reg_value);

	drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_ADD32, 32);

	if (is_m)
	{
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}
	else
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), sum_result);
	}

	drc_record_cycles(ctx, CYCLES_XADD);

	return true;
}

bool i386_device::drc_x0f_cmpxchg(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &rm_value      = I1;
	const uml::parameter &eax_value     = I0;
	const uml::parameter &cmp_result    = I2;
	const uml::parameter &cond_result   = I0;
	const uml::parameter &mem_addr_save = I6;

	const uml::code_label cx_neq     = NEW_LBL(ctx);
	const uml::code_label cx_done    = NEW_LBL(ctx);
	const uml::code_label cx_have_zf = NEW_LBL(ctx);

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);

	bool is_m = ctx.desc->is_mem;
	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, rm_value, mem_data);
	}
	else
	{
		UML_MOV(b, rm_value, DRC_REG32(MRM_RM32(modrm)));
	}

	UML_MOV(b, eax_value, DRC_REG32(EAX));
	UML_SUB(b, cmp_result, eax_value, rm_value);
	drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP32, 32);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
	UML_JMPc(b, COND_E, cx_have_zf);

	drc_gen_flags_cc_dispatch(ctx, FLAGS_CC_Z);
	UML_MOV(b, uml::mem(&m_core->ZF), cond_result);
	drc_gen_clear_flags(ctx);
	UML_LABEL(b, cx_have_zf);

	UML_CMP(b, uml::mem(&m_core->ZF), 0);
	UML_JMPc(b, COND_Z, cx_neq);

	if (is_m)
	{
		UML_MOV(b, mem_addr, mem_addr_save);
		UML_MOV(b, mem_data, DRC_REG32(reg_d));
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);
	}
	else
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), DRC_REG32(reg_d));
	}

	UML_JMP(b, cx_done);

	UML_LABEL(b, cx_neq);
	UML_MOV(b, DRC_REG32(EAX), rm_value);

	UML_LABEL(b, cx_done);

	drc_record_cycles(ctx, is_m ? CYCLES_CMPXCHG_REG_MEM_T : CYCLES_CMPXCHG_REG_REG_T);

	return true;
}
