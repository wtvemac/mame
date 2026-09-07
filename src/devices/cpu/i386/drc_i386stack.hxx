// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here


// ----------------------------------------------------------------------------
// Invariant: the shared PUSH32/POP32 handles
// ----------------------------------------------------------------------------

void i386_device::static_generate_push32()
{
	alloc_handle(*m_drc_uml, m_push32, "push32");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_push32);


	const uml::parameter &mem_addr = I0;
	const uml::parameter &esp16    = I5;
	const uml::parameter &new_esp  = I6;

	int label_ctr = 1;

	const uml::code_label stk16 = NEW_SLBL();
	const uml::code_label done  = NEW_SLBL();

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, stk16);

	UML_SUB(b, new_esp, DRC_REG32(ESP), 4);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, label_ctr, new_esp, 4, true);
	UML_ADD(b, mem_addr, new_esp, DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_write32);
	UML_MOV(b, DRC_REG32(ESP), new_esp);
	UML_JMP(b, done);

	UML_LABEL(b, stk16);
	UML_AND(b, esp16, DRC_REG32(ESP), 0xffff);
	UML_SUB(b, new_esp, esp16, 4);
	UML_AND(b, new_esp, new_esp, 0xffff);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, label_ctr, new_esp, 4, true);
	UML_ADD(b, mem_addr, new_esp, DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_write32);
	UML_ROLINS(b, DRC_REG32(ESP), new_esp, 0, 0xffff);

	UML_LABEL(b, done);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_pop32()
{
	alloc_handle(*m_drc_uml, m_pop32, "pop32");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_pop32);


	const uml::parameter &mem_addr     = I0;
	const uml::parameter &mem_data     = I2;
	const uml::parameter &popped_value = I0;
	const uml::parameter &esp16        = I6;

	int label_ctr = 1;

	const uml::code_label stk16 = NEW_SLBL();
	const uml::code_label done  = NEW_SLBL();

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, stk16);

	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, label_ctr, DRC_REG32(ESP), 4, false);
	UML_ADD(b, mem_addr, DRC_REG32(ESP), DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, popped_value, mem_data);
	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 4);
	UML_JMP(b, done);

	UML_LABEL(b, stk16);
	UML_AND(b, esp16, DRC_REG32(ESP), 0xffff);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, label_ctr, esp16, 4, false);
	UML_ADD(b, mem_addr, esp16, DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, popped_value, mem_data);
	UML_ADD(b, esp16, esp16, 4);
	UML_AND(b, esp16, esp16, 0xffff);
	UML_ROLINS(b, DRC_REG32(ESP), esp16, 0, 0xffff);

	UML_LABEL(b, done);
	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// C++ Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_stack_fault_ss_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				m_core->ext = 1;
				i386_trap_with_error(FAULT_SS, 0, 0, 0);
			}
	);
	drc_leave_interpreter();
}

void i386_device::drc_stack_fault_gp_cb()
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
// UML helpers
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_push32(compiler_state &ctx, const uml::parameter val)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	if (mem_data != val)
		UML_MOV(b, mem_data, val);


	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		UML_MOV(b, DRC_PC, ctx.pc);
		UML_MOV(b, DRC_EIP, ctx.eip);
	}

	drc_flush_cycles(ctx);

	UML_CALLH(b, *m_push32);
}

inline void i386_device::drc_gen_pop32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;


	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		UML_MOV(b, DRC_PC, ctx.pc);
		UML_MOV(b, DRC_EIP, ctx.eip);
	}

	drc_flush_cycles(ctx);

	UML_CALLH(b, *m_pop32);
}

inline void i386_device::drc_gen_push16(compiler_state &ctx, const uml::parameter val)
{
	drcuml_block &b = ctx.block;


	const uml::parameter &mem_addr   = I0;
	const uml::parameter &mem_data   = I2;
	const uml::parameter &push_value = I3;
	const uml::parameter &esp16      = I5;
	const uml::parameter &new_esp    = I6;

	UML_MOV(b, push_value, val);
	UML_AND(b, push_value, push_value, 0xffff);


	const uml::code_label stk16 = NEW_LBL(ctx);
	const uml::code_label done  = NEW_LBL(ctx);

	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		UML_MOV(b, DRC_PC, ctx.pc);
		UML_MOV(b, DRC_EIP, ctx.eip);
	}

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, stk16);

	UML_SUB(b, new_esp, DRC_REG32(ESP), 2);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, ctx.label_ctr, new_esp, 2, true);
	UML_ADD(b, mem_addr, new_esp, DRC_SEG(SS, base));
	UML_MOV(b, mem_data, push_value);
	UML_CALLH(b, *m_mem_write16);
	UML_MOV(b, DRC_REG32(ESP), new_esp);
	UML_JMP(b, done);

	UML_LABEL(b, stk16);
	UML_AND(b, esp16, DRC_REG32(ESP), 0xffff);
	UML_SUB(b, new_esp, esp16, 2);
	UML_AND(b, new_esp, new_esp, 0xffff);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, ctx.label_ctr, new_esp, 2, true);
	UML_ADD(b, mem_addr, new_esp, DRC_SEG(SS, base));
	UML_MOV(b, mem_data, push_value);
	UML_CALLH(b, *m_mem_write16);
	UML_ROLINS(b, DRC_REG32(ESP), new_esp, 0, 0xffff);

	UML_LABEL(b, done);
}

inline void i386_device::drc_gen_pop16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;


	const uml::parameter &popped_value = I0;


	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;
	const uml::parameter &esp16    = I6;

	const uml::code_label stk16 = NEW_LBL(ctx);
	const uml::code_label done  = NEW_LBL(ctx);

	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		UML_MOV(b, DRC_PC, ctx.pc);
		UML_MOV(b, DRC_EIP, ctx.eip);
	}

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, stk16);

	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, ctx.label_ctr, DRC_REG32(ESP), 2, false);
	UML_ADD(b, mem_addr, DRC_REG32(ESP), DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_read16);
	UML_AND(b, popped_value, mem_data, 0xffff);
	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 2);
	UML_JMP(b, done);

	UML_LABEL(b, stk16);
	UML_AND(b, esp16, DRC_REG32(ESP), 0xffff);
	if (!(m_drc_options & I386DRC_SKIP_STACKCHECKS))
		drc_gen_stack_fault_check(b, ctx.label_ctr, esp16, 2, false);
	UML_ADD(b, mem_addr, esp16, DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_read16);
	UML_AND(b, popped_value, mem_data, 0xffff);
	UML_ADD(b, esp16, esp16, 2);
	UML_AND(b, esp16, esp16, 0xffff);
	UML_ROLINS(b, DRC_REG32(ESP), esp16, 0, 0xffff);

	UML_LABEL(b, done);
}

inline bool i386_device::drc_gen_pusha(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &stack_addr   = I0;
	const uml::parameter &region_delta = I1;
	const uml::parameter &elem_addr    = I2;
	const uml::parameter &saved_esp    = I4;

	const uml::code_label skip_fast = NEW_LBL(ctx);
	const uml::code_label done      = NEW_LBL(ctx);

	drc_record_cycles(ctx, CYCLES_PUSHA);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, skip_fast);

	UML_ADD(b, stack_addr, DRC_REG32(ESP), DRC_SEG(SS, base));
	UML_SUB(b, stack_addr, stack_addr, 32);

	if (!debugger_enabled())
	{
		for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
		{
			fastram_entry &fi = m_fastram[ramnum];
			if (fi.readonly)
				continue;

			const uint32_t region_bytes = fi.end - fi.start + 1;
			if (region_bytes < 32)
				continue;

			const uml::code_label next_region = NEW_LBL(ctx);

			UML_SUB(b, region_delta, stack_addr, fi.start);

			UML_CMP(b, region_delta, region_bytes - 32);
			UML_JMPc(b, COND_A, next_region);

			void *fastbase = (uint8_t *)fi.base - fi.start;

			UML_STORE(b, fastbase, stack_addr, DRC_REG32(EDI), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 4);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(ESI), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 8);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(EBP), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 12);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(ESP), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 16);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(EBX), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 20);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(EDX), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 24);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(ECX), SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 28);
			UML_STORE(b, fastbase, elem_addr, DRC_REG32(EAX), SIZE_DWORD, SCALE_x1);

			UML_SUB(b, DRC_REG32(ESP), DRC_REG32(ESP), 32);

			UML_JMP(b, done);
			UML_LABEL(b, next_region);
		}
	}

	UML_LABEL(b, skip_fast);

	UML_MOV(b, saved_esp, DRC_REG32(ESP));
	drc_gen_push32(ctx, DRC_REG32(EAX));
	drc_gen_push32(ctx, DRC_REG32(ECX));
	drc_gen_push32(ctx, DRC_REG32(EDX));
	drc_gen_push32(ctx, DRC_REG32(EBX));
	drc_gen_push32(ctx, saved_esp);
	drc_gen_push32(ctx, DRC_REG32(EBP));
	drc_gen_push32(ctx, DRC_REG32(ESI));
	drc_gen_push32(ctx, DRC_REG32(EDI));

	UML_LABEL(b, done);

	return true;
}

inline bool i386_device::drc_gen_popa(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &stack_addr   = I0;
	const uml::parameter &region_delta = I1;
	const uml::parameter &elem_addr    = I2;
	const uml::parameter &popped_value = I0;

	const uml::code_label skip_fast = NEW_LBL(ctx);
	const uml::code_label done      = NEW_LBL(ctx);

	drc_record_cycles(ctx, CYCLES_POPA);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, skip_fast);

	UML_ADD(b, stack_addr, DRC_REG32(ESP), DRC_SEG(SS, base));

	if (!debugger_enabled())
	{
		for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
		{
			fastram_entry &fi = m_fastram[ramnum];

			const uint32_t region_bytes = fi.end - fi.start + 1;
			if (region_bytes < 32)
				continue;

			const uml::code_label next_region = NEW_LBL(ctx);

			UML_SUB(b, region_delta, stack_addr, fi.start);

			UML_CMP(b, region_delta, region_bytes - 32);
			UML_JMPc(b, COND_A, next_region);

			void *fastbase = (uint8_t *)fi.base - fi.start;

			UML_LOAD(b, DRC_REG32(EDI), fastbase, stack_addr, SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 4);
			UML_LOAD(b, DRC_REG32(ESI), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 8);
			UML_LOAD(b, DRC_REG32(EBP), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);

			UML_ADD(b, elem_addr, stack_addr, 16);
			UML_LOAD(b, DRC_REG32(EBX), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 20);
			UML_LOAD(b, DRC_REG32(EDX), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 24);
			UML_LOAD(b, DRC_REG32(ECX), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);
			UML_ADD(b, elem_addr, stack_addr, 28);
			UML_LOAD(b, DRC_REG32(EAX), fastbase, elem_addr, SIZE_DWORD, SCALE_x1);

			UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 32);

			UML_JMP(b, done);
			UML_LABEL(b, next_region);
		}
	}

	UML_LABEL(b, skip_fast);

	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EDI), popped_value);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(ESI), popped_value);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EBP), popped_value);
	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 4);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EBX), popped_value);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EDX), popped_value);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(ECX), popped_value);
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EAX), popped_value);

	UML_LABEL(b, done);

	return true;
}

void i386_device::drc_gen_stack_fault_check(drcuml_block &b, int &label_ctr, const uml::parameter &offset, uint32_t byte_count, bool is_write)
{
	const uml::parameter &check_scratch = I7;

	const uml::code_label normal_limit = NEW_SLBL();
	const uml::code_label do_access    = NEW_SLBL();
	const uml::code_label fault_ss     = NEW_SLBL();
	const uml::code_label fault_gp     = NEW_SLBL();
	const uml::code_label done         = NEW_SLBL();

	UML_TEST(b, DRC_CR(0), CR0_PE);
	UML_JMPc(b, COND_Z, done);

	UML_CMP(b, uml::mem(&m_core->VM), 0);
	UML_JMPc(b, COND_NZ, done);

	UML_AND(b, check_scratch, DRC_SEG(SS, flags), 0x18);

	UML_CMP(b, check_scratch, 0x10);
	UML_JMPc(b, COND_NE, normal_limit);

	UML_TEST(b, DRC_SEG(SS, flags), 0x04);
	UML_JMPc(b, COND_Z, normal_limit);

	UML_CMP(b, offset, DRC_SEG(SS, limit));

	UML_JMPc(b, COND_BE, fault_ss);
	UML_CMP(b, DRC_SEG(SS, d), 0);

	UML_JMPc(b, COND_NZ, do_access);
	UML_CMP(b, offset, 0xffff);

	UML_JMPc(b, COND_A, fault_ss);
	UML_JMP(b, do_access);

	UML_LABEL(b, normal_limit);
	UML_ADD(b, check_scratch, offset, byte_count - 1);
	UML_CMP(b, check_scratch, DRC_SEG(SS, limit));
	UML_JMPc(b, COND_A, fault_ss);

	UML_LABEL(b, do_access);
	UML_TEST(b, DRC_SEG(SS, flags), 0x08);
	if (is_write)
	{
		UML_JMPc(b, COND_NZ, fault_gp);

		UML_TEST(b, DRC_SEG(SS, flags), 0x02);
		UML_JMPc(b, COND_Z, fault_gp);
	}
	else
	{
		UML_JMPc(b, COND_Z, done);

		UML_TEST(b, DRC_SEG(SS, flags), 0x02);
		UML_JMPc(b, COND_Z, fault_gp);
	}
	UML_JMP(b, done);

	UML_LABEL(b, fault_ss);
	UML_CALLC(b, I386_CB(drc_stack_fault_ss_cb), this);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, fault_gp);
	UML_CALLC(b, I386_CB(drc_stack_fault_gp_cb), this);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, done);
}

inline void i386_device::drc_gen_stack_peek_range(drcuml_block &b, int &label_ctr, const uml::code_label take_slow, uint32_t byte_count)
{
	const uml::parameter &limit_scratch = I7;

	if (PROTECTED_MODE && !(m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		const uml::code_label limit_check = NEW_SLBL();
		const uml::code_label expand_down = NEW_SLBL();
		const uml::code_label do_read     = NEW_SLBL();

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, do_read);

		UML_AND(b, limit_scratch, DRC_SEG(SS, flags), 0x18);

		UML_CMP(b, limit_scratch, 0x10);
		UML_JMPc(b, COND_NE, limit_check);

		UML_TEST(b, DRC_SEG(SS, flags), 0x04);
		UML_JMPc(b, COND_NZ, expand_down);

		UML_LABEL(b, limit_check);

		UML_ADD(b, limit_scratch, DRC_REG32(ESP), byte_count);

		UML_CMP(b, limit_scratch, DRC_SEG(SS, limit));
		UML_JMPc(b, COND_A, take_slow);

		UML_JMP(b, do_read);

		UML_LABEL(b, expand_down);

		UML_CMP(b, DRC_REG32(ESP), DRC_SEG(SS, limit));
		UML_JMPc(b, COND_BE, take_slow);

		UML_CMP(b, DRC_SEG(SS, d), 0);
		UML_JMPc(b, COND_NZ, do_read);

		UML_CMP(b, DRC_REG32(ESP), 0xffff);
		UML_JMPc(b, COND_A, take_slow);

		UML_LABEL(b, do_read);
	}

	UML_ADD(b, uml::mem(&m_core->mem_laddr), DRC_REG32(ESP), DRC_SEG(SS, base));
}

inline bool i386_device::drc_gen_leave(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &popped_value = I0;

	UML_MOV(b, DRC_REG32(ESP), DRC_REG32(EBP));
	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_REG32(EBP), popped_value);

	drc_record_cycles(ctx, CYCLES_LEAVE);

	return true;
}

inline bool i386_device::drc_gen_leave16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &popped_value = I0;

	UML_ROLINS(b, DRC_REG32(ESP), DRC_REG32(EBP), 0, 0xffff);
	drc_gen_pop16(ctx);
	UML_ROLINS(b, DRC_REG32(EBP), popped_value, 0, 0xffff);

	drc_record_cycles(ctx, CYCLES_LEAVE);

	return true;
}

// ----------------------------------------------------------------------------
// PUSHA/POPA/LEAVE/PUSH r/POP r
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_pusha(compiler_state &ctx)
{
	return drc_gen_pusha(ctx);
}

bool i386_device::drc_pri_popa(compiler_state &ctx)
{
	return drc_gen_popa(ctx);
}

bool i386_device::drc_pri_leave32(compiler_state &ctx)
{
	return drc_gen_leave(ctx);
}

bool i386_device::drc_pri_leave16(compiler_state &ctx)
{
	return drc_gen_leave16(ctx);
}

bool i386_device::drc_pri_push_eax32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(EAX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_eax16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(EAX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ecx32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(ECX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ecx16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(ECX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_edx32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(EDX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_edx16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(EDX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ebx32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(EBX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ebx16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(EBX));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_esp32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(ESP));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_esp16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(ESP));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ebp32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(EBP));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_ebp16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(EBP));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_esi32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(ESI));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_esi16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(ESI));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_edi32(compiler_state &ctx)
{
	drc_gen_push32(ctx, DRC_REG32(EDI));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_push_edi16(compiler_state &ctx)
{
	drc_gen_push16(ctx, DRC_REG32(EDI));

	drc_record_cycles(ctx, CYCLES_PUSH_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_pop_r32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &popped_value = I0;

	drc_gen_pop32(ctx);
	UML_MOV(b, DRC_GET_OP_RM32(ctx.desc->opcode0), popped_value);

	drc_record_cycles(ctx, CYCLES_POP_REG_SHORT);

	return true;
}

bool i386_device::drc_pri_pop_r16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &popped_value = I0;

	drc_gen_pop16(ctx);
	UML_ROLINS(b, DRC_GET_OP_RM32(ctx.desc->opcode0), popped_value, 0, 0x0000ffff);

	drc_record_cycles(ctx, CYCLES_POP_REG_SHORT);

	return true;
}

// ----------------------------------------------------------------------------
// PUSH imm / PUSH r/m
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_push_i32(compiler_state &ctx)
{
	drc_gen_push32(ctx, drc_get_imm32(ctx));

	drc_record_cycles(ctx, CYCLES_PUSH_IMM);

	return true;
}

bool i386_device::drc_pri_push_i16(compiler_state &ctx)
{
	uint16_t imm = (uint16_t)drc_get_imm16(ctx);
	drc_gen_push16(ctx, (uint32_t)imm);

	drc_record_cycles(ctx, CYCLES_PUSH_IMM);

	return true;
}

bool i386_device::drc_pri_push_i8_32(compiler_state &ctx)
{
	drc_gen_push32(ctx, (uint32_t)(int32_t)(int8_t)drc_get_imm8(ctx));

	drc_record_cycles(ctx, CYCLES_PUSH_IMM);

	return true;
}

bool i386_device::drc_pri_push_i8_16(compiler_state &ctx)
{
	uint16_t imm = (uint16_t)(int16_t)(int8_t)drc_get_imm8(ctx);
	drc_gen_push16(ctx, (uint32_t)imm);

	drc_record_cycles(ctx, CYCLES_PUSH_IMM);

	return true;
}

bool i386_device::drc_pri_push_rm32(compiler_state &ctx)
{
	const uml::parameter &loaded_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	drc_gen_rm32(ctx, modrm);
	drc_gen_push32(ctx, loaded_value);

	drc_record_cycles(ctx, CYCLES_PUSH_RM);

	return true;
}

bool i386_device::drc_pri_push_rm16(compiler_state &ctx)
{
	const uml::parameter &loaded_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	drc_gen_rm16(ctx, modrm);
	drc_gen_push16(ctx, loaded_value);

	drc_record_cycles(ctx, CYCLES_PUSH_RM);

	return true;
}

// ----------------------------------------------------------------------------
// PUSHF/POPF
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_pushfd(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &eflags_value = I4;

	const uml::code_label take_slow = NEW_LBL(ctx);
	const uml::code_label do_push   = NEW_LBL(ctx);
	const uml::code_label done      = NEW_LBL(ctx);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->VM), 0);
	UML_JMPc(b, COND_Z, do_push);

	UML_CMP(b, uml::mem(&m_core->IOPL), 0);
	UML_JMPc(b, COND_NZ, do_push);

	UML_JMP(b, take_slow);

	UML_LABEL(b, do_push);
	drc_gen_flags_all(ctx);
	drc_gen_pack_eflags(b, eflags_value);
	UML_AND(b, eflags_value, eflags_value, 0x00fcffff);
	drc_gen_push32(ctx, eflags_value);

	drc_record_cycles(ctx, CYCLES_PUSHF);

	UML_JMP(b, done);

	UML_LABEL(b, take_slow);
	drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_pushfd), this, false);

	UML_LABEL(b, done);

	return true;
}

bool i386_device::drc_pri_pushf16(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_pushf), this, false);
}

bool i386_device::drc_pri_popfd(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_popfd), this, false);
}

bool i386_device::drc_pri_popf16(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_popf), this, false);
}
