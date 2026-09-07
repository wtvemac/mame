// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// C++ Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_flush_tlb_cb()
{
	m_core->cr[3] = m_core->data32;
	vtlb_flush_dynamic();
	m_core->cycles = 0;
}

void i386_device::drc_invlpg_cb()
{
	vtlb_flush_address(m_core->mem_laddr);
	m_core->cycles = 0;
}

void i386_device::drc_io_fault_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				m_core->ext = 1;
				i386_trap_with_error(FAULT_GP, 0, 0, 0);
			}
	);
	drc_leave_interpreter();
}

// ----------------------------------------------------------------------------
// IN/OUT
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_get_io_port(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	// 0xe4-0xe7 (IN/OUT imm8) port=imm8; 0xec-0xef (IN/OUT DX) port=DX reg
	const bool port_is_imm8 = ctx.desc->opcode0 <= 0xe7;

	if (port_is_imm8)
	{
		uint8_t port = drc_get_imm8(ctx);
		UML_MOV(b, uml::mem(&m_core->mem_laddr), port);
	}
	else
	{
		UML_AND(b, uml::mem(&m_core->mem_laddr), DRC_REG32(EDX), 0xffff);
	}
}

inline void i386_device::drc_gen_io_permission_check(compiler_state &ctx)
{
	if ((m_drc_options & I386DRC_SKIP_IOCHECKS) || !PROTECTED_MODE)
		return;

	drcuml_block &b = ctx.block;

	const uml::code_label allowed = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->CPL), uml::mem(&m_core->IOPL));
	UML_JMPc(b, COND_BE, allowed);

	offs_t fault_pc = CTRANSFER_USE_PC;
	drc_gen_control_transfer_cb(ctx, fault_pc, I386_CB(drc_io_fault_cb), this);

	UML_LABEL(b, allowed);
}

inline void i386_device::drc_gen_io_remap_check(compiler_state &ctx)
{
	if ((m_drc_options & I386DRC_SKIP_IOCHECKS))
		return;

	drcuml_block &b = ctx.block;

	const offs_t next_pc = ctx.desc->pc + ctx.desc->length;

	drc_flush_cycles(ctx);

	UML_MOV(b, DRC_PC, next_pc);
	UML_SUB(b, DRC_EIP, next_pc, DRC_SEG(CS, base));

	UML_CMP(b, uml::mem(&m_core->drc_cache_dirty), 0);
	UML_EXHc(b, COND_NZ, *m_out_of_cycles, next_pc);
}

bool i386_device::drc_pri_io_read8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	drc_flush_cycles(ctx);

	UML_READ(b, DRC_SCR32, uml::mem(&m_core->mem_laddr), SIZE_BYTE, SPACE_IO);
	UML_BREG_WRITE(b, AL, DRC_SCR32);

	drc_record_cycles(ctx, CYCLES_IN_VAR);

	return true;
}

bool i386_device::drc_pri_io_write8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	UML_AND(b, DRC_SCR32, DRC_REG32(EAX), 0xff);

	drc_flush_cycles(ctx);

	UML_WRITE(b, uml::mem(&m_core->mem_laddr), DRC_SCR32, SIZE_BYTE, SPACE_IO);

	drc_record_cycles(ctx, CYCLES_OUT_VAR);

	drc_gen_io_remap_check(ctx);

	return true;
}

bool i386_device::drc_pri_io_read16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	drc_flush_cycles(ctx);

	UML_READ(b, DRC_SCR16, uml::mem(&m_core->mem_laddr), SIZE_WORD, SPACE_IO);
	UML_ROLINS(b, DRC_REG32(EAX), DRC_SCR16, 0, 0xffff);

	drc_record_cycles(ctx, CYCLES_IN_VAR);

	return true;
}

bool i386_device::drc_pri_io_write16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	UML_AND(b, DRC_SCR16, DRC_REG32(EAX), 0xffff);

	drc_flush_cycles(ctx);

	UML_WRITE(b, uml::mem(&m_core->mem_laddr), DRC_SCR16, SIZE_WORD, SPACE_IO);

	drc_record_cycles(ctx, CYCLES_OUT_VAR);

	drc_gen_io_remap_check(ctx);

	return true;
}

bool i386_device::drc_pri_io_read32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	drc_flush_cycles(ctx);

	UML_READ(b, DRC_SCR32, uml::mem(&m_core->mem_laddr), SIZE_DWORD, SPACE_IO);
	UML_MOV(b, DRC_REG32(EAX), DRC_SCR32);

	drc_record_cycles(ctx, CYCLES_IN_VAR);

	return true;
}

bool i386_device::drc_pri_io_write32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_gen_get_io_port(ctx);

	drc_gen_io_permission_check(ctx);

	UML_MOV(b, DRC_SCR32, DRC_REG32(EAX));

	drc_flush_cycles(ctx);

	UML_WRITE(b, uml::mem(&m_core->mem_laddr), DRC_SCR32, SIZE_DWORD, SPACE_IO);

	drc_record_cycles(ctx, CYCLES_OUT_VAR);

	drc_gen_io_remap_check(ctx);

	return true;
}

// ----------------------------------------------------------------------------
// Group 0F 00 (SLDT/STR/LLDT/LTR/VERR/VERW)
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_0f00_sldt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	if (is_reg)
	{
		UML_MOV(b, DRC_GET_MRM_RM32(modrm), DRC_TSK(segment));

		drc_record_cycles(ctx, CYCLES_SLDT_REG);
	}
	else
	{
		const uml::parameter &mem_data = I2;

		(this->*ctx.gen_ea)(ctx);

		UML_AND(b, mem_data, DRC_TSK(segment), 0xffff);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);

		drc_record_cycles(ctx, CYCLES_SLDT_MEM);
	}

	return true;
}

inline bool i386_device::drc_gen_0f00_str(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	if (is_reg)
	{
		UML_MOV(b, DRC_GET_MRM_RM32(modrm), DRC_TSK(segment));

		drc_record_cycles(ctx, CYCLES_STR_REG);
	}
	else
	{
		const uml::parameter &mem_data = I2;

		(this->*ctx.gen_ea)(ctx);

		UML_AND(b, mem_data, DRC_TSK(segment), 0xffff);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);

		drc_record_cycles(ctx, CYCLES_STR_MEM);
	}

	return true;
}

inline bool i386_device::drc_gen_0f00_lldt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data      = I2;
	const uml::parameter &descriptor_lo = I3;
	const uml::parameter &descriptor_hi = I1;
	const uml::parameter &read_desc_lo  = I6;
	const uml::parameter &read_desc_hi  = I7;

	if (is_reg)
	{
		UML_MOV(b, DRC_SCR32, DRC_GET_MRM_RM32(modrm));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, DRC_SCR32, mem_data);
	}

	if (PROTECTED_MODE)
	{
		const uml::code_label take_slow = NEW_LBL(ctx);
		const uml::code_label done      = NEW_LBL(ctx);

		drc_flush_cycles(ctx);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, take_slow);

		UML_CMP(b, uml::mem(&m_core->CPL), 0);
		UML_JMPc(b, COND_NE, take_slow);

		UML_MOV(b, DRC_LDT(segment), DRC_SCR32);

		drc_gen_read_descriptor(b, ctx.label_ctr, take_slow);
		UML_MOV(b, descriptor_lo, read_desc_lo);
		UML_MOV(b, descriptor_hi, read_desc_hi);

		drc_gen_unpack_descriptor_limit_base(b, ctx.label_ctr, DRC_LDT(limit), DRC_LDT(base), descriptor_lo, descriptor_hi);

		UML_SHR(b, DRC_LDT(flags), descriptor_hi, 8);
		UML_AND(b, DRC_LDT(flags), DRC_LDT(flags), 0xf0ff);

		UML_JMP(b, done);

		UML_LABEL(b, take_slow);
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_lldt_cb), this, false);

		UML_LABEL(b, done);
	}
	else
	{
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_lldt_cb), this, false);
	}

	drc_record_cycles(ctx, is_reg ? CYCLES_LLDT_REG : CYCLES_LLDT_MEM);

	return true;
}

inline bool i386_device::drc_gen_0f00_ltr(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data      = I2;
	const uml::parameter &descriptor_lo = I3;
	const uml::parameter &descriptor_hi = I1;
	const uml::parameter &access_byte   = I6;
	const uml::parameter &read_desc_lo  = I6;
	const uml::parameter &read_desc_hi  = I7;

	if (is_reg)
	{
		UML_MOV(b, DRC_SCR32, DRC_GET_MRM_RM32(modrm));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, DRC_SCR32, mem_data);
	}

	if (PROTECTED_MODE)
	{
		const uml::code_label take_slow = NEW_LBL(ctx);
		const uml::code_label done      = NEW_LBL(ctx);

		drc_flush_cycles(ctx);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, take_slow);

		UML_CMP(b, uml::mem(&m_core->CPL), 0);
		UML_JMPc(b, COND_NE, take_slow);

		UML_MOV(b, DRC_TSK(segment), DRC_SCR32);

		drc_gen_read_descriptor(b, ctx.label_ctr, take_slow);
		UML_MOV(b, descriptor_lo, read_desc_lo);
		UML_MOV(b, descriptor_hi, read_desc_hi);

		drc_gen_unpack_descriptor_limit_base(b, ctx.label_ctr, DRC_TSK(limit), DRC_TSK(base), descriptor_lo, descriptor_hi);

		UML_SHR(b, DRC_TSK(flags), descriptor_hi, 8);
		UML_AND(b, DRC_TSK(flags), DRC_TSK(flags), 0xf0ff);
		UML_OR(b, DRC_TSK(flags), DRC_TSK(flags), 2);

		UML_SHR(b, access_byte, descriptor_hi, 8);
		UML_AND(b, access_byte, access_byte, 0xff);
		UML_OR(b, access_byte, access_byte, 2);
		drc_gen_write_descriptor_accessed(b, ctx.label_ctr, access_byte, take_slow);

		UML_JMP(b, done);

		UML_LABEL(b, take_slow);
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_ltr_cb), this, false);

		UML_LABEL(b, done);
	}
	else
	{
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_ltr_cb), this, false);
	}

	drc_record_cycles(ctx, is_reg ? CYCLES_LTR_REG : CYCLES_LTR_MEM);

	return true;
}

inline bool i386_device::drc_gen_0f00_verr(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data      = I2;
	const uml::parameter &descriptor_hi = I7;
	const uml::parameter &access_flags  = I6;
	const uml::parameter &dpl           = I7;
	const uml::parameter &rpl           = I6;

	if (is_reg)
	{
		UML_MOV(b, DRC_SCR32, DRC_GET_MRM_RM32(modrm));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, DRC_SCR32, mem_data);
	}

	if (PROTECTED_MODE)
	{
		const uml::code_label take_slow     = NEW_LBL(ctx);
		const uml::code_label clear         = NEW_LBL(ctx);
		const uml::code_label set           = NEW_LBL(ctx);
		const uml::code_label done          = NEW_LBL(ctx);
		const uml::code_label nonconforming = NEW_LBL(ctx);

		drc_flush_cycles(ctx);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, take_slow);

		drc_gen_read_descriptor(b, ctx.label_ctr, take_slow);

		UML_SHR(b, access_flags, descriptor_hi, 8);
		UML_AND(b, access_flags, access_flags, 0xf0ff);

		UML_TEST(b, access_flags, 0x10);
		UML_JMPc(b, COND_Z, clear);

		UML_TEST(b, access_flags, 0x08);
		UML_JMPc(b, COND_Z, nonconforming);

		UML_TEST(b, access_flags, 0x02);
		UML_JMPc(b, COND_Z, clear);

		UML_TEST(b, access_flags, 0x04);
		UML_JMPc(b, COND_NZ, set);

		UML_LABEL(b, nonconforming);
		UML_SHR(b, dpl, access_flags, 5);
		UML_AND(b, dpl, dpl, 3);
		UML_AND(b, rpl, DRC_SCR32, 3);

		UML_CMP(b, uml::mem(&m_core->CPL), rpl);
		UML_MOVc(b, COND_A, rpl, uml::mem(&m_core->CPL));

		UML_CMP(b, dpl, rpl);
		UML_JMPc(b, COND_B, clear);

		UML_JMP(b, set);

		UML_LABEL(b, clear);
		UML_MOV(b, uml::mem(&m_core->ZF), 0);
		UML_JMP(b, done);

		UML_LABEL(b, set);
		UML_MOV(b, uml::mem(&m_core->ZF), 1);
		UML_JMP(b, done);

		UML_LABEL(b, take_slow);
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_verr_cb), this, false);

		UML_LABEL(b, done);
	}
	else
	{
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_verr_cb), this, false);
	}

	drc_record_cycles(ctx, is_reg ? CYCLES_VERR_REG : CYCLES_VERR_MEM);

	return true;
}

inline bool i386_device::drc_gen_0f00_verw(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value  = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &descriptor_hi = I7;
	const uml::parameter &access_flags  = I6;
	const uml::parameter &dpl           = I7;
	const uml::parameter &rpl           = I6;

	if (is_reg)
	{
		UML_AND(b, loaded_value, DRC_GET_MRM_RM16(modrm), 0xffff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read16);
		UML_AND(b, loaded_value, mem_data, 0xffff);
	}

	UML_MOV(b, DRC_SCR32, loaded_value);

	if (PROTECTED_MODE)
	{
		const uml::code_label take_slow = NEW_LBL(ctx);
		const uml::code_label clear     = NEW_LBL(ctx);
		const uml::code_label set       = NEW_LBL(ctx);
		const uml::code_label done      = NEW_LBL(ctx);
		const uml::code_label dpl_check = NEW_LBL(ctx);

		drc_flush_cycles(ctx);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, take_slow);

		drc_gen_read_descriptor(b, ctx.label_ctr, take_slow);

		UML_SHR(b, access_flags, descriptor_hi, 8);
		UML_AND(b, access_flags, access_flags, 0xf0ff);

		UML_TEST(b, access_flags, 0x10);
		UML_JMPc(b, COND_Z, dpl_check);

		UML_TEST(b, access_flags, 0x08);
		UML_JMPc(b, COND_NZ, clear);

		UML_TEST(b, access_flags, 0x02);
		UML_JMPc(b, COND_Z, clear);

		UML_LABEL(b, dpl_check);
		UML_SHR(b, dpl, access_flags, 5);
		UML_AND(b, dpl, dpl, 3);
		UML_AND(b, rpl, DRC_SCR32, 3);

		UML_CMP(b, uml::mem(&m_core->CPL), rpl);
		UML_MOVc(b, COND_A, rpl, uml::mem(&m_core->CPL));

		UML_CMP(b, dpl, rpl);
		UML_JMPc(b, COND_B, clear);

		UML_JMP(b, set);

		UML_LABEL(b, clear);
		UML_MOV(b, uml::mem(&m_core->ZF), 0);
		UML_JMP(b, done);

		UML_LABEL(b, set);
		UML_MOV(b, uml::mem(&m_core->ZF), 1);
		UML_JMP(b, done);

		UML_LABEL(b, take_slow);
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_verw_cb), this, false);

		UML_LABEL(b, done);
	}
	else
	{
		drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_verw_cb), this, false);
	}

	drc_record_cycles(ctx, is_reg ? CYCLES_VERW_REG : CYCLES_VERW_MEM);

	return true;
}

bool i386_device::drc_x0f_group0f00(compiler_state &ctx)
{
	uint8_t modrm             = drc_get_modrm(ctx);
	uint8_t group_0f00_opcode = MRM_OPCODE(modrm);
	bool    is_reg            = MRM_MOD(modrm) == 3;

	ctx.invalidate_rscratch();

	switch (group_0f00_opcode)
	{
		case 0:
			return drc_gen_0f00_sldt(ctx, modrm, is_reg);
		case 1:
			return drc_gen_0f00_str(ctx, modrm, is_reg);
		case 2:
			return drc_gen_0f00_lldt(ctx, modrm, is_reg);
		case 3:
			return drc_gen_0f00_ltr(ctx, modrm, is_reg);
		case 4:
			drc_gen_clear_flags(ctx);
			return drc_gen_0f00_verr(ctx, modrm, is_reg);
		case 5:
			drc_gen_clear_flags(ctx);
			return drc_gen_0f00_verw(ctx, modrm, is_reg);
	}

	return false;
}

// ----------------------------------------------------------------------------
// Group 0F 01 (SGDT/SIDT/LGDT/LIDT/SMSW/LMSW/INVLPG)
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_0f01_sgdt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;
	const uml::parameter &addr_save = I6;
	const uml::parameter &mem_data  = I2;

	if (is_reg)
		return false;

	(this->*ctx.gen_ea)(ctx);

	UML_MOV(b, addr_save, ea_result);
	drc_flush_cycles(ctx);
	UML_AND(b, mem_data, DRC_GDT(limit), 0xffff);
	UML_CALLH(b, *m_mem_write16);
	UML_ADD(b, ea_result, addr_save, 2);
	UML_MOV(b, mem_data, DRC_GDT(base));
	UML_CALLH(b, *m_mem_write32);

	drc_record_cycles(ctx, CYCLES_SGDT);

	return true;
}

inline bool i386_device::drc_gen_0f01_sidt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;
	const uml::parameter &addr_save = I6;
	const uml::parameter &mem_data  = I2;

	if (is_reg)
		return false;

	(this->*ctx.gen_ea)(ctx);

	UML_MOV(b, addr_save, ea_result);
	drc_flush_cycles(ctx);
	UML_AND(b, mem_data, DRC_IDT(limit), 0xffff);
	UML_CALLH(b, *m_mem_write16);
	UML_ADD(b, ea_result, addr_save, 2);
	UML_MOV(b, mem_data, DRC_IDT(base));
	UML_CALLH(b, *m_mem_write32);

	drc_record_cycles(ctx, CYCLES_SIDT);

	return true;
}

inline bool i386_device::drc_gen_0f01_lgdt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;
	const uml::parameter &addr_save = I6;
	const uml::parameter &mem_data  = I2;

	if (is_reg)
		return false;

	(this->*ctx.gen_ea)(ctx);

	UML_MOV(b, addr_save, ea_result);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read16);
	UML_AND(b, DRC_GDT(limit), mem_data, 0xffff);
	UML_ADD(b, ea_result, addr_save, 2);
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, DRC_GDT(base), mem_data);

	drc_record_cycles(ctx, CYCLES_LGDT);

	return true;
}

inline bool i386_device::drc_gen_0f01_lidt(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;
	const uml::parameter &addr_save = I6;
	const uml::parameter &mem_data  = I2;

	if (is_reg)
		return false;

	(this->*ctx.gen_ea)(ctx);

	UML_MOV(b, addr_save, ea_result);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read16);
	UML_AND(b, DRC_IDT(limit), mem_data, 0xffff);
	UML_ADD(b, ea_result, addr_save, 2);
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, DRC_IDT(base), mem_data);

	drc_record_cycles(ctx, CYCLES_LIDT);

	return true;
}

inline bool i386_device::drc_gen_0f01_smsw(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	drc_flush_cycles(ctx);
	if (is_reg)
	{
		UML_AND(b, DRC_GET_MRM_RM32(modrm), DRC_CR(0), 0xffff);

		drc_record_cycles(ctx, CYCLES_SMSW_REG);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		UML_AND(b, mem_data, DRC_CR(0), 0xffff);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);

		drc_record_cycles(ctx, CYCLES_SMSW_MEM);
	}

	return true;
}

inline bool i386_device::drc_gen_0f01_lmsw(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	if (is_reg)
	{
		UML_AND(b, DRC_SCR32, DRC_GET_MRM_RM32(modrm), 0xffff);
		drc_flush_cycles(ctx);
		UML_CALLC(b, I386_CB(drc_lmsw_cb), this);

		drc_record_cycles(ctx, CYCLES_LMSW_REG);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read16);
		UML_AND(b, DRC_SCR32, mem_data, 0xffff);
		UML_CALLC(b, I386_CB(drc_lmsw_cb), this);

		drc_record_cycles(ctx, CYCLES_LMSW_MEM);
	}

	return true;
}

inline bool i386_device::drc_gen_0f01_invlpg(compiler_state &ctx, uint8_t modrm, bool is_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;

	if (is_reg)
		return false;

	(this->*ctx.gen_ea)(ctx);

	UML_MOV(b, uml::mem(&m_core->mem_laddr), ea_result);
	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_invlpg_cb), this);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, 25);

	return true;
}

bool i386_device::drc_x0f_group0f01(compiler_state &ctx)
{
	uint8_t modrm             = drc_get_modrm(ctx);
	uint8_t group_0f01_opcode = MRM_OPCODE(modrm);
	bool    is_reg            = MRM_MOD(modrm) == 3;

	switch (group_0f01_opcode)
	{
		case 0:
			return drc_gen_0f01_sgdt(ctx, modrm, is_reg);
		case 1:
			return drc_gen_0f01_sidt(ctx, modrm, is_reg);
		case 2:
			return drc_gen_0f01_lgdt(ctx, modrm, is_reg);
		case 3:
			return drc_gen_0f01_lidt(ctx, modrm, is_reg);
		case 4:
			return drc_gen_0f01_smsw(ctx, modrm, is_reg);
		case 6:
			return drc_gen_0f01_lmsw(ctx, modrm, is_reg);
		case 7:
			return drc_gen_0f01_invlpg(ctx, modrm, is_reg);
		default:
			return false;
	}
}

// ----------------------------------------------------------------------------
// CLTS / MOV (to/from) CR / control-register + LMSW
// ----------------------------------------------------------------------------

bool i386_device::drc_x0f_clts(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	// Remove task switched flag
	UML_AND(b, DRC_CR(0), DRC_CR(0), ~CR0_TS);

	drc_record_cycles(ctx, CYCLES_CLTS);

	return true;
}

bool i386_device::drc_x0f_mov_r32_cr(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t modrm  = drc_get_modrm(ctx);
	uint8_t cr_idx = MRM_REG(modrm);
	uint8_t dst    = MRM_RM32(modrm);

	switch (cr_idx)
	{
		case 0:
		case 2:
		case 3:
		case 4:
			drc_flush_cycles(ctx);
			UML_MOV(b, DRC_REG32(dst), DRC_CR(cr_idx));
			break;

		default:
			return false;
	}

	drc_record_cycles(ctx, CYCLES_MOV_CR_REG);

	return true;
}

bool i386_device::drc_x0f_mov_cr_r32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t modrm = drc_get_modrm(ctx);
	uint8_t cr    = MRM_REG(modrm);
	uint8_t src   = MRM_RM32(modrm);

	switch (cr)
	{
		case 0:
			drc_flush_cycles(ctx);
			UML_MOV(b, DRC_SCR32, DRC_REG32(src));
			UML_CALLC(b, I386_CB(drc_write_cr0_cb), this);
			drc_record_cycles(ctx, CYCLES_MOV_REG_CR0);
			return true;
		case 2:
			drc_flush_cycles(ctx);
			UML_MOV(b, DRC_CR(2), DRC_REG32(src));
			drc_record_cycles(ctx, CYCLES_MOV_REG_CR2);
			return true;
		case 3:
			drc_flush_cycles(ctx);
			UML_MOV(b, DRC_SCR32, DRC_REG32(src));
			UML_CALLC(b, I386_CB(drc_flush_tlb_cb), this);
			drc_record_cycles(ctx, CYCLES_MOV_REG_CR3);
			return true;
		case 4:
			drc_flush_cycles(ctx);
			UML_MOV(b, DRC_SCR32, DRC_REG32(src));
			UML_CALLC(b, I386_CB(drc_write_cr4_cb), this);
			drc_record_cycles(ctx, CYCLES_MOV_REG_CR0);
			return true;
		default:
			return false;
	}
}


void i386_device::drc_write_cr0_cb()
{
	uint32_t old  = m_core->cr[0];
	m_core->cr[0] = m_core->data32;
	if ((old ^ m_core->cr[0]) & CR0_PG) // Page mode was toggled on or off
	{
		vtlb_flush_dynamic();
		m_core->drc_cache_dirty = true;
		m_core->cycles          = 0;
	}

	if ((old ^ m_core->cr[0]) & CR0_PE) // Protected mode was toggled on or off
	{
		m_core->drc_cache_dirty = true;
		m_core->cycles          = 0;
	}
}


void i386_device::drc_write_cr4_cb()
{
	uint32_t old  = m_core->cr[4];
	m_core->cr[4] = m_core->data32;
	if ((old ^ m_core->cr[4]) & 0x90)
	{
		vtlb_flush_dynamic();
		m_core->drc_cache_dirty = true;
		m_core->cycles          = 0;
	}
}


void i386_device::drc_lmsw_cb()
{
	uint32_t old = m_core->cr[0];
	uint16_t val = (uint16_t)(m_core->data32 & 0xffff);

	if (PROTECTED_MODE)
		val |= CR0_PE;

	m_core->cr[0] = (m_core->cr[0] & ~(CR0_PE | CR0_MP | CR0_EM | CR0_TS)) | (val & (CR0_PE | CR0_MP | CR0_EM | CR0_TS));

	if ((old ^ m_core->cr[0]) & CR0_PE) // Protected mode was toggled on or off
	{
		m_core->drc_cache_dirty = true;
		m_core->cycles          = 0;
	}
}

// ----------------------------------------------------------------------------
// CPUID/RDMSR/WRMSR/RDTSC
// ----------------------------------------------------------------------------

bool i386_device::drc_x0f_cpuid(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_cpuid_cb), this);

	return true;
}


void i386_device::drc_cpuid_cb()
{
	if (m_cpuid_id0 != 0)
	{
		uint32_t leaf = m_core->reg.d[EAX];
		switch (leaf)
		{
			case 0:
				m_core->reg.d[EAX] = m_cpuid_max_input_value_eax;
				m_core->reg.d[EBX] = m_cpuid_id0;
				m_core->reg.d[ECX] = m_cpuid_id2;
				m_core->reg.d[EDX] = m_cpuid_id1;
				m_core->cycles -= CYCLES_CPUID;
				break;
			case 1:
				m_core->reg.d[EAX] = m_cpu_version;
				m_core->reg.d[EBX] = m_brand_id;
				m_core->reg.d[ECX] = 0;
				m_core->reg.d[EDX] = m_feature_flags;
				m_core->cycles -= CYCLES_CPUID_EAX1;
				break;
			default:
				drc_enter_interpreter();
				opcode_cpuid();
				drc_leave_interpreter();
				break;
		}
	}
}

bool i386_device::drc_x0f_rdmsr(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_rdmsr_cb), this);
	return true;
}


void i386_device::drc_rdmsr_cb()
{
	drc_enter_interpreter();
	pentium_rdmsr();
	drc_leave_interpreter();
}

bool i386_device::drc_x0f_wrmsr(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_wrmsr_cb), this);

	return true;
}


void i386_device::drc_wrmsr_cb()
{
	drc_enter_interpreter();
	pentium_wrmsr();
	drc_leave_interpreter();
}

bool i386_device::drc_x0f_rdtsc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &elapsed_cycles = I0;
	const uml::parameter &tsc_value      = I1;

	drc_flush_cycles(ctx);
	UML_SUB(b, elapsed_cycles, uml::mem(&m_core->base_cycles), DRC_CYCLES);

	UML_DSEXT(b, tsc_value, elapsed_cycles, SIZE_DWORD);
	UML_DADD(b, tsc_value, tsc_value, uml::mem(&m_core->tsc));
	UML_MOV(b, DRC_REG32(EAX), tsc_value);
	UML_DSHR(b, tsc_value, tsc_value, 32);
	UML_MOV(b, DRC_REG32(EDX), tsc_value);

	drc_record_cycles(ctx, CYCLES_RDTSC);

	return true;
}
