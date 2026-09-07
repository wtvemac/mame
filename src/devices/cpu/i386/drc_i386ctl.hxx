// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// Invariant: RETF protected mode
// ----------------------------------------------------------------------------

void i386_device::static_generate_retf_protected()
{
	alloc_handle(*m_drc_uml, m_retf_protected, "retf_protected");

	drcuml_block &b(m_drc_uml->begin_invariant_block(32768));

	UML_HANDLE(b, *m_retf_protected);

	const uml::parameter &result = I0;

	int label_ctr = 1;

	compiler_state ctx = drc_create_compiler_state(b, label_ctr, 0, 0, 0);

	const uml::code_label bail_result = NEW_LBL(ctx);

	drc_gen_retf_protected(ctx, bail_result);

	UML_LABEL(b, bail_result);
	UML_MOV(b, result, 0);
	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_gen_retf_protected(compiler_state &ctx, const uml::code_label bail)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &new_eip       = I1;
	const uml::parameter &rpl_check     = I0;
	const uml::parameter &access_flags  = I0;
	const uml::parameter &scratch       = I2;
	const uml::parameter &seg_limit     = I5;
	const uml::parameter &cs_access     = I4;
	const uml::parameter &descriptor_lo = I6;
	const uml::parameter &descriptor_hi = I7;

	const uml::code_label do_outer_priv = NEW_LBL(ctx);
	const uml::code_label cs_conforming = NEW_LBL(ctx);
	const uml::code_label cs_priv_ok    = NEW_LBL(ctx);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->VM), 0);
	UML_JMPc(b, COND_NZ, bail);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, bail);

	drc_gen_stack_peek_range(b, ctx.label_ctr, bail, 7);

	UML_MOV(b, mem_addr, uml::mem(&m_core->mem_laddr));
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read64);
	UML_MOV(b, uml::mem(&m_core->ctl_new_eip), mem_data);
	UML_DSHR(b, mem_data, mem_data, 32);
	UML_AND(b, mem_data, mem_data, 0xffff);
	UML_MOV(b, uml::mem(&m_core->ctl_new_cs), mem_data);

	UML_TEST(b, uml::mem(&m_core->ctl_new_cs), 0xfffc);
	UML_JMPc(b, COND_Z, bail);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_cs));

	UML_AND(b, rpl_check, uml::mem(&m_core->ctl_new_cs), 3);
	UML_MOV(b, uml::mem(&m_core->ctl_target_dpl), rpl_check);

	UML_CMP(b, rpl_check, uml::mem(&m_core->CPL));
	UML_JMPc(b, COND_B, bail);
	UML_JMPc(b, COND_A, do_outer_priv);

	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);

	drc_flush_cycles(ctx);
	drc_gen_commit_cs_same_priv(b, ctx.label_ctr, uml::mem(&m_core->ctl_new_eip), bail);

	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 8);
	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), uml::mem(&m_core->ctl_pop_count));

	UML_MOV(b, DRC_EIP, uml::mem(&m_core->ctl_new_eip));
	UML_ADD(b, new_eip, uml::mem(&m_core->ctl_new_eip), DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_eip);

	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, uml::mem(&m_core->pending_cycles));

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, do_outer_priv);

	UML_ADD(b, mem_addr, DRC_REG32(ESP), DRC_SEG(SS, base));
	UML_ADD(b, mem_addr, mem_addr, uml::mem(&m_core->ctl_pop_count));
	UML_ADD(b, mem_addr, mem_addr, 8);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read64);
	UML_MOV(b, uml::mem(&m_core->ctl_new_esp), mem_data);
	UML_DSHR(b, mem_data, mem_data, 32);
	UML_AND(b, mem_data, mem_data, 0xffff);
	UML_MOV(b, uml::mem(&m_core->ctl_new_ss), mem_data);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_cs));

	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);

	UML_ROLAND(b, access_flags, descriptor_hi, 32 - 8, 0xf0ff);

	UML_AND(b, scratch, access_flags, 0x18);
	UML_CMP(b, scratch, 0x18);
	UML_JMPc(b, COND_NE, bail);

	UML_SHR(b, scratch, access_flags, 5);
	UML_AND(b, scratch, scratch, 3);

	UML_TEST(b, access_flags, 0x04);
	UML_JMPc(b, COND_NZ, cs_conforming);

	UML_CMP(b, scratch, uml::mem(&m_core->ctl_target_dpl));
	UML_JMPc(b, COND_NE, bail);
	UML_JMP(b, cs_priv_ok);

	UML_LABEL(b, cs_conforming);
	UML_CMP(b, scratch, uml::mem(&m_core->ctl_target_dpl));
	UML_JMPc(b, COND_A, bail);

	UML_LABEL(b, cs_priv_ok);
	UML_TEST(b, access_flags, 0x80);
	UML_JMPc(b, COND_Z, bail);

	drc_gen_unpack_descriptor_limit_inline(b, ctx.label_ctr, seg_limit, descriptor_lo, descriptor_hi);

	UML_CMP(b, uml::mem(&m_core->ctl_new_eip), seg_limit);
	UML_JMPc(b, COND_A, bail);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_cs));
	UML_ROLAND(b, cs_access, descriptor_hi, 32 - 8, 0xf0ff);

	drc_gen_unpack_descriptor_limit_base_inline(b, ctx.label_ctr, DRC_SEG(CS, limit), DRC_SEG(CS, base), descriptor_lo, descriptor_hi);

	drc_gen_commit_seg_descriptor(b, CS, cs_access, descriptor_lo);

	drc_gen_write_descriptor_accessed_inline(b, ctx.label_ctr, descriptor_lo, bail);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_ss));
	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);
	drc_gen_commit_ss_for_privilege_change(b, ctx.label_ctr, uml::mem(&m_core->ctl_target_dpl), bail);

	// NOTE: i386_check_sreg_validity here

	UML_AND(b, uml::mem(&m_core->CPL), uml::mem(&m_core->ctl_new_cs), 3);
	UML_MOV(b, DRC_REG32(ESP), uml::mem(&m_core->ctl_new_esp));
	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), uml::mem(&m_core->ctl_pop_count));

	UML_MOV(b, DRC_EIP, uml::mem(&m_core->ctl_new_eip));
	UML_ADD(b, new_eip, uml::mem(&m_core->ctl_new_eip), DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_eip);

	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, uml::mem(&m_core->pending_cycles));

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}

inline void i386_device::drc_gen_intrablock_jump(compiler_state &ctx, offs_t target_pc)
{
	drcuml_block &b = ctx.block;

	if (!(m_drc_options & I386DRC_DISABLE_INTRABLOCK) && ctx.desc->intrablock_branch())
	{
		UML_JMP(b, target_pc);
	}
	else
	{
		drc_flush_cycles(ctx);
		UML_HASHJMP(b, ctx.mode, target_pc, *m_nocode);
	}
}

// ----------------------------------------------------------------------------
// Conditional jumps
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_jcc32(compiler_state &ctx, uint8_t cc, bool is_rel32)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &cond_result = I0;

	const uml::code_label nojmp = NEW_LBL(ctx);

	int32_t rel       = is_rel32 ? (int32_t)drc_get_imm32(ctx) : (int32_t)(int8_t)drc_get_imm8(ctx);
	offs_t  after_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
	offs_t  taken_eip = after_eip + rel;
	offs_t  taken_pc  = taken_eip + m_core->sreg[CS].base;

	drc_gen_flags_cc(ctx, cc);

	UML_CMP(b, cond_result, 0);
	UML_JMPc(b, COND_Z, nojmp);

	UML_MOV(b, DRC_EIP, taken_eip);
	UML_MOV(b, DRC_PC, taken_pc);

	drc_record_cycles(ctx, is_rel32 ? CYCLES_JCC_FULL_DISP : CYCLES_JCC_DISP8);

	drc_gen_intrablock_jump(ctx, taken_pc);

	UML_LABEL(b, nojmp);

	drc_record_cycles(ctx, is_rel32 ? CYCLES_JCC_FULL_DISP_NOBRANCH : CYCLES_JCC_DISP8_NOBRANCH);

	return true;
}

inline bool i386_device::drc_gen_jcc_short16(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &cond_result = I0;

	const uml::code_label nojmp = NEW_LBL(ctx);

	int32_t rel       = (int32_t)(int8_t)drc_get_imm8(ctx);
	offs_t  after_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
	offs_t  taken_eip = (after_eip + rel) & 0xffff;
	offs_t  taken_pc  = taken_eip + m_core->sreg[CS].base;

	drc_gen_flags_cc(ctx, cc);

	UML_CMP(b, cond_result, 0);
	UML_JMPc(b, COND_Z, nojmp);

	UML_MOV(b, DRC_EIP, taken_eip);
	UML_MOV(b, DRC_PC, taken_pc);

	drc_record_cycles(ctx, CYCLES_JCC_DISP8);

	drc_gen_intrablock_jump(ctx, taken_pc);

	UML_LABEL(b, nojmp);

	drc_record_cycles(ctx, CYCLES_JCC_DISP8_NOBRANCH);

	return true;
}

inline bool i386_device::drc_gen_jcc16(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &cond_result = I0;

	const uml::code_label nojmp = NEW_LBL(ctx);

	int32_t rel       = (int32_t)(int16_t)drc_get_imm16(ctx);
	offs_t  after_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);

	offs_t taken_eip = (after_eip + rel) & 0xffff;
	offs_t taken_pc  = taken_eip + m_core->sreg[CS].base;

	drc_gen_flags_cc(ctx, cc);

	UML_CMP(b, cond_result, 0);
	UML_JMPc(b, COND_Z, nojmp);

	UML_MOV(b, DRC_EIP, taken_eip);
	UML_MOV(b, DRC_PC, taken_pc);

	drc_record_cycles(ctx, CYCLES_JCC_FULL_DISP);

	drc_gen_intrablock_jump(ctx, taken_pc);

	UML_LABEL(b, nojmp);

	drc_record_cycles(ctx, CYCLES_JCC_FULL_DISP_NOBRANCH);

	return true;
}

bool i386_device::drc_pri_jcc_rel8_32(compiler_state &ctx)
{
	return drc_gen_jcc32(ctx, ctx.desc->opcode0 & FLAGS_CC_MASK, false);
}

bool i386_device::drc_pri_jcc_rel8_16(compiler_state &ctx)
{
	return drc_gen_jcc_short16(ctx, ctx.desc->opcode0 & FLAGS_CC_MASK);
}

bool i386_device::drc_x0f_jcc_rel32(compiler_state &ctx)
{
	return drc_gen_jcc32(ctx, ctx.desc->opcode1 & FLAGS_CC_MASK, true);
}

bool i386_device::drc_x0f_jcc_rel16(compiler_state &ctx)
{
	return drc_gen_jcc16(ctx, ctx.desc->opcode1 & FLAGS_CC_MASK);
}

bool i386_device::drc_pri_jcxz(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::code_label fail = NEW_LBL(ctx);

	int8_t rel8     = (int8_t)drc_get_imm8(ctx);
	offs_t next_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
	offs_t tgt_eip  = next_eip + (offs_t)(int32_t)rel8;

	drc_flush_cycles(ctx);

	if (ctx.desc->addr32)
	{
		UML_TEST(b, DRC_REG32(ECX), DRC_REG32(ECX));
	}
	else
	{
		// 16-bit address override

		const uml::parameter &cx16 = I1;

		UML_AND(b, cx16, DRC_REG32(ECX), 0xffff);
		UML_TEST(b, cx16, cx16);
	}

	UML_JMPc(b, COND_NZ, fail);

	offs_t tgt_pc = tgt_eip + m_core->sreg[CS].base;
	UML_MOV(b, DRC_EIP, tgt_eip);
	UML_MOV(b, DRC_PC, tgt_pc);

	drc_record_cycles(ctx, CYCLES_JCXZ);

	drc_gen_intrablock_jump(ctx, tgt_pc);

	UML_LABEL(b, fail);

	drc_record_cycles(ctx, CYCLES_JCXZ_NOBRANCH);

	return true;
}

// ----------------------------------------------------------------------------
// Unconditional jumps
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_jmp_rel8_32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	int8_t rel  = (int8_t)drc_get_imm8(ctx);
	offs_t neip = (ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) + (int32_t)rel;
	offs_t npc  = neip + m_core->sreg[CS].base;

	UML_MOV(b, DRC_EIP, neip);
	UML_MOV(b, DRC_PC, npc);

	drc_record_cycles(ctx, CYCLES_JMP_SHORT);

	drc_gen_intrablock_jump(ctx, npc);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_rel16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	int32_t rel  = (int32_t)(int16_t)drc_get_imm16(ctx);
	offs_t  neip = ((ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) + rel) & 0xffff;
	offs_t  npc  = neip + m_core->sreg[CS].base;
	UML_MOV(b, DRC_EIP, neip);
	UML_MOV(b, DRC_PC, npc);

	drc_record_cycles(ctx, CYCLES_JMP);

	drc_gen_intrablock_jump(ctx, npc);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_rel8_16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	int8_t rel  = (int8_t)drc_get_imm8(ctx);
	offs_t neip = ((ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) + (int32_t)rel) & 0xffff;
	offs_t npc  = neip + m_core->sreg[CS].base;
	UML_MOV(b, DRC_EIP, neip);
	UML_MOV(b, DRC_PC, npc);

	drc_record_cycles(ctx, CYCLES_JMP_SHORT);

	drc_gen_intrablock_jump(ctx, npc);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_rel32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	int32_t rel  = (int32_t)drc_get_imm32(ctx);
	offs_t  neip = (ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) + rel;
	offs_t  npc  = neip + m_core->sreg[CS].base;

	UML_MOV(b, DRC_EIP, neip);
	UML_MOV(b, DRC_PC, npc);

	drc_record_cycles(ctx, CYCLES_JMP);

	drc_gen_intrablock_jump(ctx, npc);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_rm32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &jump_target = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_OPCODE(modrm) != 4)
		return false;


	drc_gen_rm32(ctx, modrm);

	UML_MOV(b, DRC_EIP, jump_target);
	UML_ADD(b, jump_target, jump_target, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, jump_target);

	drc_record_cycles(ctx, CYCLES_JMP_REG);

	drc_flush_cycles(ctx);
	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_rm16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &jump_target = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_OPCODE(modrm) != 4)
		return false;

	drc_gen_rm16(ctx, modrm);

	UML_MOV(b, DRC_EIP, jump_target);
	UML_ADD(b, jump_target, jump_target, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, jump_target);

	drc_record_cycles(ctx, CYCLES_JMP_REG);

	drc_flush_cycles(ctx);
	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_jmp_abs(compiler_state &ctx)
{
	return drc_gen_interpreter_fallback(ctx);
}

// ----------------------------------------------------------------------------
// LOOP/LOOPcc
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_loop(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &cond_result = I0;

	const uml::code_label fail = NEW_LBL(ctx);

	int8_t rel8     = (int8_t)drc_get_imm8(ctx);
	offs_t next_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
	offs_t tgt_eip  = next_eip + (offs_t)(int32_t)rel8;

	drc_flush_cycles(ctx);

	if (ctx.desc->addr32)
	{
		UML_SUB(b, DRC_REG32(ECX), DRC_REG32(ECX), 1);
	}
	else
	{
		// 16-bit address size (0x67)

		const uml::parameter &cx16 = I1;

		UML_AND(b, cx16, DRC_REG32(ECX), 0xffff);
		UML_SUB(b, cx16, cx16, 1);
		UML_AND(b, cx16, cx16, 0xffff);
		UML_ROLINS(b, DRC_REG32(ECX), cx16, 0, 0xffff);

		UML_TEST(b, cx16, cx16);
	}

	UML_JMPc(b, COND_Z, fail);

	X86_CYCLES loop_cycles = CYCLES_LOOP;
	// 0xe2 LOOP (no ZF test); 0xe0 LOOPNE/LOOPNZ and 0xe1 LOOPE/LOOPZ (with ZF test)
	if (ctx.desc->opcode0 != 0xe2)
	{
		const uml::code_label have_zf = NEW_LBL(ctx);

		UML_CMP(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
		UML_JMPc(b, COND_E, have_zf);

		drc_gen_flags_cc_dispatch(ctx, FLAGS_CC_Z);
		UML_MOV(b, uml::mem(&m_core->ZF), cond_result);
		drc_gen_clear_flags(ctx);
		UML_LABEL(b, have_zf);

		switch (ctx.desc->opcode0)
		{
			// 0xe0 LOOPNE/LOOPNZ
			case 0xe0:
				UML_TEST(b, uml::mem(&m_core->ZF), uml::mem(&m_core->ZF));
				UML_JMPc(b, COND_NZ, fail);

				loop_cycles = CYCLES_LOOPNZ;
				break;
			// 0xe1 LOOPE/LOOPZ
			case 0xe1:
				UML_TEST(b, uml::mem(&m_core->ZF), uml::mem(&m_core->ZF));
				UML_JMPc(b, COND_Z, fail);

				loop_cycles = CYCLES_LOOPZ;
				break;
		}
	}

	offs_t tgt_pc = tgt_eip + m_core->sreg[CS].base;
	UML_MOV(b, DRC_EIP, tgt_eip);
	UML_MOV(b, DRC_PC, tgt_pc);

	drc_record_cycles(ctx, loop_cycles);

	drc_gen_intrablock_jump(ctx, tgt_pc);

	UML_LABEL(b, fail);

	drc_record_cycles(ctx, loop_cycles);

	return true;
}

// ----------------------------------------------------------------------------
// CALL/RET/RETF
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_call32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &call_target  = I1;

	bool   dynamic = false;
	offs_t ret_eip, target_eip;

	// 0xe8 CALL rel32; else reached via 0xff /2 CALL r/m32 (group FF)
	if (ctx.desc->opcode0 == 0xe8)
	{
		int32_t rel = (int32_t)drc_get_imm32(ctx);
		ret_eip     = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
		target_eip  = ret_eip + rel;
	}
	else
	{
		uint8_t modrm = drc_get_modrm(ctx);
		if (MRM_OPCODE(modrm) != 2)
			return false;


		drc_gen_rm32(ctx, modrm);

		UML_MOV(b, call_target, loaded_value);
		ret_eip = ctx.eip + (offs_t)(ctx.cursor - ctx.pc);
		dynamic = true;
	}

	drc_gen_push32(ctx, (uint32_t)ret_eip);

	if (!dynamic)
	{
		offs_t tpc = target_eip + m_core->sreg[CS].base;
		UML_MOV(b, DRC_EIP, target_eip);
		UML_MOV(b, DRC_PC, tpc);

		drc_record_cycles(ctx, CYCLES_CALL);

		drc_gen_intrablock_jump(ctx, tpc);
	}
	else
	{
		UML_MOV(b, DRC_EIP, call_target);
		UML_ADD(b, call_target, call_target, DRC_SEG(CS, base));
		UML_MOV(b, DRC_PC, call_target);

		drc_record_cycles(ctx, CYCLES_CALL_REG);

		drc_flush_cycles(ctx);
		UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
	}

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_call16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &call_target  = I1;

	bool   dynamic = false;
	offs_t ret_eip, target_eip;

	// 0xe8 CALL rel16; else reached via 0xff /2 CALL r/m16 (group FF)
	if (ctx.desc->opcode0 == 0xe8)
	{
		int32_t rel = (int32_t)(int16_t)drc_get_imm16(ctx);
		ret_eip     = (ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) & 0xffff;
		target_eip  = (ret_eip + rel) & 0xffff;
	}
	else
	{
		uint8_t modrm = drc_get_modrm(ctx);
		if (MRM_OPCODE(modrm) != 2)
			return false;

		drc_gen_rm16(ctx, modrm);

		UML_MOV(b, call_target, loaded_value);
		ret_eip = (ctx.eip + (offs_t)(ctx.cursor - ctx.pc)) & 0xffff;
		dynamic = true;
	}

	drc_gen_push16(ctx, (uint32_t)ret_eip);

	if (!dynamic)
	{
		offs_t tpc = target_eip + m_core->sreg[CS].base;
		UML_MOV(b, DRC_EIP, target_eip);
		UML_MOV(b, DRC_PC, tpc);

		drc_record_cycles(ctx, CYCLES_CALL);

		drc_gen_intrablock_jump(ctx, tpc);
	}
	else
	{
		UML_MOV(b, DRC_EIP, call_target);
		UML_ADD(b, call_target, call_target, DRC_SEG(CS, base));
		UML_MOV(b, DRC_PC, call_target);

		drc_record_cycles(ctx, CYCLES_CALL_REG);

		drc_flush_cycles(ctx);
		UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
	}

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_ret32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &popped_value = I0;


	// 0xc2 RET imm16; 0xc3 RET
	uint16_t pop_count = (ctx.desc->opcode0 == 0xc2) ? drc_get_imm16(ctx) : 0;

	drc_gen_pop32(ctx);

	if (pop_count)
		UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), pop_count);
	UML_MOV(b, DRC_EIP, popped_value);
	UML_ADD(b, popped_value, popped_value, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, popped_value);

	drc_record_cycles(ctx, CYCLES_RET);

	drc_flush_cycles(ctx);
	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_retf_i16(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_retf_i16), this);
}

bool i386_device::drc_pri_retf16(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_retf16), this);
}

bool i386_device::drc_pri_retf32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	offs_t initial_ip          = ctx.cursor;
	// 0xca RETF imm16; 0xcb RETF
	const bool has_imm         = (ctx.desc->opcode0 == 0xca);
	void (*slow_cfunc)(void *) = has_imm ? I386_INTERP(i386_retf_i32) : I386_INTERP(i386_retf32);

	if (PROTECTED_MODE)
	{
		if (m_drc_options & I386DRC_SKIP_STACKCHECKS)
		{
			uint16_t count = has_imm ? drc_get_imm16(ctx) : 0;

			UML_MOV(b, DRC_PC, ctx.pc);
			UML_MOV(b, DRC_EIP, ctx.eip);

			UML_MOV(b, uml::mem(&m_core->ctl_pop_count), count);
			UML_MOV(b, uml::mem(&m_core->pending_cycles), m_cycle_table_rm[has_imm ? CYCLES_RET_IMM_INTERSEG : CYCLES_RET_INTERSEG]);

			drc_flush_cycles(ctx);

			UML_CALLH(b, *m_retf_protected);

			// If m_retf_protected fails with bail_result then we try the interpreter
			drc_gen_control_transfer_cb(ctx, initial_ip, slow_cfunc, this);

			ctx.block_ended = true;

			return false;
		}
		else
		{
			return drc_gen_control_transfer_cb(ctx, initial_ip, slow_cfunc, this);
		}
	}
	else
	{
		return drc_gen_control_transfer_cb(ctx, initial_ip, slow_cfunc, this);
	}
}

// ----------------------------------------------------------------------------
// Group FF (INC/DEC/CALL/JMP/PUSH via ModR/M)
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_groupFF_32(compiler_state &ctx)
{
	uint8_t group_ff_opcode = ctx.desc->modrm_regop;

	ctx.invalidate_rscratch();

	if (group_ff_opcode <= 1)
		drc_gen_clear_flags(ctx);

	switch (group_ff_opcode)
	{
		case 0:
		case 1:
			return drc_gen_incdec32(ctx, false);
		case 2:
			return drc_pri_call32(ctx);
		case 4:
			return drc_pri_jmp_rm32(ctx);
		case 6:
			return drc_pri_push_rm32(ctx);
		default:
			return drc_gen_interpreter_fallback(ctx);
	}
}

bool i386_device::drc_pri_groupFF_16(compiler_state &ctx)
{
	uint8_t group_ff_opcode = ctx.desc->modrm_regop;

	ctx.invalidate_rscratch();

	if (group_ff_opcode <= 1)
		drc_gen_clear_flags(ctx);

	switch (group_ff_opcode)
	{
		case 0:
		case 1:
			return drc_gen_incdec16(ctx, false);
		case 2:
			return drc_pri_call16(ctx);
		case 4:
			return drc_pri_jmp_rm16(ctx);
		case 6:
			return drc_pri_push_rm16(ctx);
		default:
			return drc_gen_interpreter_fallback(ctx);
	}
}

// ----------------------------------------------------------------------------
// NOP
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_nop(compiler_state &ctx)
{
	// Technically XCHG EAX,EAX on i386
	UML_NOP(ctx.block);

	drc_record_cycles(ctx, CYCLES_MOV_REG_REG);

	return true;
}
