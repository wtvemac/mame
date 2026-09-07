// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// Invariant: comonly ran interrupt code.
// Protected same-priv IRQ delivery, protected software ints, protected IRET
// ----------------------------------------------------------------------------

void i386_device::static_generate_irq_deliver_protected_same_priv()
{
	alloc_handle(*m_drc_uml, m_irq_deliver_protected_same_priv, "irq_deliver_protected_same_priv");

	drcuml_block &b(m_drc_uml->begin_invariant_block(32768));

	UML_HANDLE(b, *m_irq_deliver_protected_same_priv);

	const uml::parameter &result = I0;

	int label_ctr = 1;

	compiler_state ctx = drc_create_compiler_state(b, label_ctr, 0, 0, 0);

	const uml::code_label bail_result = NEW_LBL(ctx);
	const uml::code_label done_result = NEW_LBL(ctx);

	drc_gen_deliver_protected_same_priv(ctx, false, bail_result, done_result, DRC_EIP, 6);

	UML_LABEL(b, bail_result);
	UML_MOV(b, result, 0);
	UML_RET(b);

	UML_LABEL(b, done_result);
	UML_MOV(b, result, 1);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_soft_int_deliver_protected()
{
	alloc_handle(*m_drc_uml, m_soft_int_deliver_protected, "soft_int_deliver_protected");

	drcuml_block &b(m_drc_uml->begin_invariant_block(32768));

	UML_HANDLE(b, *m_soft_int_deliver_protected);

	const uml::parameter &result = I0;

	int label_ctr = 1;

	compiler_state ctx = drc_create_compiler_state(b, label_ctr, 0, 0, 0);

	const uml::code_label bail_result = NEW_LBL(ctx);

	drc_gen_deliver_protected_same_priv(ctx, true, bail_result, bail_result, uml::mem(&m_core->soft_int_ret_eip), -1);

	UML_LABEL(b, bail_result);
	UML_MOV(b, result, 0);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_iret_protected()
{
	alloc_handle(*m_drc_uml, m_iret_protected, "iret_protected");

	drcuml_block &b(m_drc_uml->begin_invariant_block(32768));

	UML_HANDLE(b, *m_iret_protected);

	const uml::parameter &result = I0;

	int label_ctr = 1;

	compiler_state ctx = drc_create_compiler_state(b, label_ctr, 0, 0, 0);

	const uml::code_label bail_result = NEW_LBL(ctx);

	drc_gen_iret_protected(ctx, bail_result);

	UML_LABEL(b, bail_result);
	UML_MOV(b, result, 0);
	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// C++ Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_check_irq_native_cb()
{
	if (m_core->delayed_interrupt_enable)
	{
		m_core->IF                       = 1;
		m_core->delayed_interrupt_enable = 0;
	}

	if (!m_core->irq_state)
		return;

	drc_enter_interpreter();

	uint32_t eip_before = m_core->eip;

	drc_catch_fault_inplace(
			[&]
			{
				i386_check_irq_line();
			}
	);

	drc_leave_interpreter();

	if (m_core->eip != eip_before)
		m_core->cycles = 0;
}

void i386_device::drc_deliver_irq_vector_cb()
{
	drc_enter_interpreter();

	uint32_t eip_before = m_core->eip;
	drc_catch_fault_inplace(
			[&]
			{
				m_core->cycles -= 2;
				i386_trap((int)m_core->data32, 1);
			}
	);

	drc_leave_interpreter();

	if (m_core->eip != eip_before)
		m_core->cycles = 0;
}

void i386_device::drc_get_irq_vector_cb()
{
	int vector = standard_irq_callback(0, m_core->pc);

	m_core->data32 = (uint32_t)vector;
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_gen_irq_poll(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &deliver_result = I0;

	const uml::code_label skip_sti  = NEW_LBL(ctx);
	const uml::code_label no_irq    = NEW_LBL(ctx);
	const uml::code_label take_slow = NEW_LBL(ctx);
	const uml::code_label done      = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->delayed_interrupt_enable), 0);
	UML_JMPc(b, COND_Z, skip_sti);

	UML_MOV(b, uml::mem(&m_core->IF), 1);
	UML_MOV(b, uml::mem(&m_core->delayed_interrupt_enable), 0);
	UML_LABEL(b, skip_sti);

	UML_CMP(b, uml::mem(&m_core->irq_state), 0);
	UML_JMPc(b, COND_Z, no_irq);

	UML_CMP(b, uml::mem(&m_core->IF), 0);
	UML_JMPc(b, COND_Z, no_irq);

	bool const is_386 = (m_cpu_version & 0xf00) == 0x300;

	if (debugger_enabled())
	{
		UML_JMP(b, take_slow);
	}
	else if (!PROTECTED_MODE)
	{
		drc_gen_irq_deliver_realmode(ctx);
	}
	else if (!is_386 && (m_drc_options & I386DRC_SKIP_STACKCHECKS)) // Protected mode, 486 and up with stack checks skipped
	{
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_irq_deliver_protected_same_priv);

		UML_CMP(b, deliver_result, 0);
		UML_JMPc(b, COND_E, take_slow);

		drc_gen_fault_inplace_check(ctx);
		UML_JMP(b, done);
	}
	else // 386 protected mode, or stack checks enabled
	{
		UML_JMP(b, take_slow);
	}

	UML_LABEL(b, no_irq);

	UML_CMP(b, uml::mem(&m_core->smi), 0);
	UML_JMPc(b, COND_Z, done);

	UML_CMP(b, uml::mem(&m_core->smm), 0);
	UML_JMPc(b, COND_Z, take_slow);

	UML_JMP(b, done);

	UML_LABEL(b, take_slow);
	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_check_irq_native_cb), this);
	drc_gen_fault_inplace_check(ctx);

	UML_LABEL(b, done);
}

void i386_device::drc_gen_irq_deliver_realmode(compiler_state &ctx)
{
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &working_value = I0;
	const uml::parameter &irq_vector    = I4;
	const uml::parameter &ivt_entry     = I7;
	const uml::parameter &new_pc        = I1;

	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_get_irq_vector_cb), this);

	UML_MOV(b, irq_vector, DRC_SCR32);

	UML_SHL(b, working_value, irq_vector, 2);
	UML_ADD(b, working_value, working_value, DRC_IDT(base));
	UML_MOV(b, mem_addr, working_value);
	UML_CALLH(b, *m_mem_read32);

	UML_MOV(b, ivt_entry, mem_data);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, 6);

	drc_gen_flags_all(ctx);

	drc_flush_cycles(ctx);
	drc_gen_pack_eflags(b, working_value);

	UML_AND(b, working_value, working_value, 0xffff);

	drc_gen_push16(ctx, working_value);
	drc_gen_push16(ctx, DRC_SEG(CS, selector));
	drc_gen_push16(ctx, DRC_EIP);

	UML_AND(b, working_value, ivt_entry, 0xffff);
	UML_MOV(b, DRC_EIP, working_value);
	UML_SHR(b, working_value, ivt_entry, 16);
	UML_AND(b, DRC_SEG(CS, selector), working_value, 0xffff);

	UML_MOV(b, uml::mem(&m_core->IF), 0);
	UML_MOV(b, uml::mem(&m_core->TF), 0);

	UML_SHL(b, working_value, DRC_SEG(CS, selector), 4);

	const uml::code_label skip_reset_quirk = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->performed_intersegment_jump), 0);
	UML_JMPc(b, COND_NZ, skip_reset_quirk);

	UML_OR(b, working_value, working_value, 0xfff00000);
	UML_LABEL(b, skip_reset_quirk);

	if (m_cpu_version < 0x500)
		UML_MOV(b, DRC_SEG(CS, flags), 0x93);
	UML_MOV(b, DRC_SEG(CS, base), working_value);
	UML_MOV(b, DRC_SEG(CS, d), 0);
	UML_MOV(b, DRC_SEG(CS, valid), 1);

	UML_ADD(b, new_pc, DRC_EIP, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_pc);

	drc_flush_cycles(ctx);
	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}

void i386_device::drc_gen_deliver_protected_same_priv(compiler_state &ctx, bool is_software, const uml::code_label bail, const uml::code_label done, const uml::parameter ret_eip, int cycle_count)
{
	const uml::parameter &gate_offset        = I5;
	const uml::parameter &gate_addr          = I5;
	const uml::parameter &phys_addr          = I7;
	const uml::parameter &gate_lo            = I6;
	const uml::parameter &gate_hi            = I7;
	const uml::parameter &gate_type          = I0;
	const uml::parameter &gate_dpl           = I0;
	const uml::parameter &new_eip            = I1;
	const uml::parameter &target_cs_selector = I4;
	const uml::parameter &descriptor_lo      = I6;
	const uml::parameter &descriptor_hi      = I7;
	const uml::parameter &target_desc_flags  = I0;
	const uml::parameter &dpl                = I5;
	const uml::parameter &target_limit       = I5;

	drcuml_block &b = ctx.block;

	if (!is_software)
	{
		UML_CMP(b, uml::mem(&m_core->ext), 1);
		UML_JMPc(b, COND_NE, bail);
	}

	const uml::code_label fail = is_software ? bail : NEW_LBL(ctx);

	if (is_software)
	{
		UML_SHL(b, gate_offset, uml::mem(&m_core->soft_int_vector), 3);
	}
	else
	{
		drc_flush_cycles(ctx);
		UML_CALLC(b, I386_CB(drc_get_irq_vector_cb), this);
		UML_MOV(b, uml::mem(&m_core->irq_vector_pending), DRC_SCR32);
		UML_SHL(b, gate_offset, uml::mem(&m_core->irq_vector_pending), 3);
	}

	UML_CMP(b, gate_offset, DRC_IDT(limit));
	UML_JMPc(b, COND_AE, fail);

	UML_ADD(b, gate_addr, gate_offset, DRC_IDT(base));

	drc_gen_privileged_walk(b, ctx.label_ctr, gate_addr, phys_addr, fail);
	UML_DREAD_NOFP(b, gate_lo, phys_addr, SIZE_QWORD, SPACE_PROGRAM);
	UML_DSHR(b, gate_hi, gate_lo, 32);

	UML_SHR(b, gate_type, gate_hi, 8);
	UML_AND(b, gate_type, gate_type, 0x1f);

	UML_AND(b, uml::mem(&m_core->irq_gate_is_trap), gate_type, 1);

	UML_AND(b, gate_type, gate_type, 0x1e);
	UML_CMP(b, gate_type, 0x0e);
	UML_JMPc(b, COND_NE, fail);

	if (is_software)
	{
		UML_SHR(b, gate_dpl, gate_hi, 13);
		UML_AND(b, gate_dpl, gate_dpl, 3);
		UML_CMP(b, gate_dpl, uml::mem(&m_core->CPL));
		UML_JMPc(b, COND_B, fail);
	}

	UML_TEST(b, gate_hi, 0x8000);
	UML_JMPc(b, COND_Z, fail);

	UML_AND(b, new_eip, gate_hi, 0xffff0000);
	UML_ROLINS(b, new_eip, gate_lo, 0, 0xffff);

	UML_SHR(b, target_cs_selector, gate_lo, 16);

	UML_TEST(b, target_cs_selector, 0xfffc);
	UML_JMPc(b, COND_Z, fail);

	UML_MOV(b, DRC_SCR32, target_cs_selector);
	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, fail);

	UML_ROLAND(b, target_desc_flags, descriptor_hi, 32 - 8, 0xf0ff);

	UML_AND(b, dpl, target_desc_flags, 0x18);

	UML_CMP(b, dpl, 0x18);
	UML_JMPc(b, COND_NE, fail);

	UML_TEST(b, target_desc_flags, 0x80);
	UML_JMPc(b, COND_Z, fail);

	const uml::code_label conforming   = NEW_LBL(ctx);
	const uml::code_label do_elevation = NEW_LBL(ctx);

	UML_TEST(b, target_desc_flags, 0x04);
	UML_JMPc(b, COND_NZ, conforming);

	UML_SHR(b, dpl, target_desc_flags, 5);
	UML_AND(b, dpl, dpl, 3);

	UML_CMP(b, dpl, uml::mem(&m_core->CPL));
	UML_JMPc(b, COND_B, do_elevation);

	UML_JMPc(b, COND_NE, fail);
	UML_JMP(b, conforming);

	UML_LABEL(b, do_elevation);

	drc_gen_deliver_protected_inner_priv(ctx, new_eip, target_cs_selector, descriptor_lo, descriptor_hi, dpl, ret_eip, cycle_count, fail);

	UML_LABEL(b, conforming);

	drc_gen_unpack_descriptor_limit_inline(b, ctx.label_ctr, target_limit, descriptor_lo, descriptor_hi);

	UML_CMP(b, new_eip, target_limit);
	UML_JMPc(b, COND_A, fail);

	UML_CMP(b, DRC_REG32(ESP), 10);
	UML_JMPc(b, COND_B, fail);

	UML_AND(b, uml::mem(&m_core->irq_old_cs_selector), DRC_SEG(CS, selector), 0xffff);

	UML_AND(b, target_cs_selector, target_cs_selector, 0xfffffffc);
	UML_OR(b, target_cs_selector, target_cs_selector, uml::mem(&m_core->CPL));
	UML_MOV(b, DRC_SCR32, target_cs_selector);

	drc_flush_cycles(ctx);

	drc_gen_commit_cs_same_priv(b, ctx.label_ctr, new_eip, fail);

	drc_gen_int_finish(ctx, new_eip, ret_eip, cycle_count);

	if (!is_software)
	{
		UML_LABEL(b, fail);
		UML_MOV(b, DRC_SCR32, uml::mem(&m_core->irq_vector_pending));

		drc_flush_cycles(ctx);

		UML_CALLC(b, I386_CB(drc_deliver_irq_vector_cb), this);

		UML_JMP(b, done);
	}
}

void i386_device::drc_gen_deliver_protected_inner_priv(compiler_state &ctx, const uml::parameter new_eip, const uml::parameter target_cs_selector, const uml::parameter target_cs_v1, const uml::parameter target_cs_v2, const uml::parameter dpl, const uml::parameter ret_eip, int cycle_count, const uml::code_label bail)
{
	const uml::parameter &reg_v1    = I6;
	const uml::parameter &reg_v2    = I7;
	const uml::parameter &tss_addr  = I0;
	const uml::parameter &tss_dword = I4;

	drcuml_block &b = ctx.block;

	UML_CMP(b, uml::mem(&m_core->VM), 0);
	UML_JMPc(b, COND_NZ, bail);

	UML_MOV(b, uml::mem(&m_core->ctl_target_dpl), dpl);

	UML_AND(b, uml::mem(&m_core->irq_old_cs_selector), DRC_SEG(CS, selector), 0xffff);
	UML_AND(b, target_cs_selector, target_cs_selector, 0xfffffffc);
	UML_OR(b, target_cs_selector, target_cs_selector, uml::mem(&m_core->ctl_target_dpl));
	UML_MOV(b, DRC_SCR32, target_cs_selector);
	UML_MOV(b, reg_v1, target_cs_v1);
	UML_MOV(b, reg_v2, target_cs_v2);

	drc_flush_cycles(ctx);

	drc_gen_commit_cs_same_priv(b, ctx.label_ctr, new_eip, bail);

	UML_TEST(b, DRC_TSK(flags), 8);
	UML_JMPc(b, COND_Z, bail);

	UML_SHL(b, tss_addr, uml::mem(&m_core->ctl_target_dpl), 3);
	UML_ADD(b, tss_addr, tss_addr, DRC_TSK(base));
	UML_ADD(b, tss_addr, tss_addr, 8);
	drc_gen_privileged_read32walk(b, ctx.label_ctr, tss_dword, tss_addr, bail);
	UML_AND(b, uml::mem(&m_core->ctl_new_ss), tss_dword, 0xffff);

	UML_TEST(b, uml::mem(&m_core->ctl_new_ss), 0xfffc);
	UML_JMPc(b, COND_Z, bail);

	UML_AND(b, uml::mem(&m_core->irq_old_ss_selector), DRC_SEG(SS, selector), 0xffff);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_ss));

	drc_flush_cycles(ctx);

	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, reg_v1, reg_v2, bail);

	drc_gen_commit_ss_for_privilege_change(b, ctx.label_ctr, uml::mem(&m_core->ctl_target_dpl), bail);

	UML_SHL(b, tss_addr, uml::mem(&m_core->ctl_target_dpl), 3);
	UML_ADD(b, tss_addr, tss_addr, DRC_TSK(base));
	UML_ADD(b, tss_addr, tss_addr, 4);
	drc_gen_privileged_read32walk(b, ctx.label_ctr, tss_dword, tss_addr, bail);
	UML_MOV(b, uml::mem(&m_core->ctl_new_esp), tss_dword);

	UML_TEST(b, DRC_SEG(SS, flags), 0x04);
	UML_JMPc(b, COND_NZ, bail);

	UML_CMP(b, uml::mem(&m_core->ctl_new_esp), 20);
	UML_JMPc(b, COND_B, bail);

	UML_MOV(b, uml::mem(&m_core->irq_old_esp), DRC_REG32(ESP));

	UML_MOV(b, uml::mem(&m_core->CPL), uml::mem(&m_core->ctl_target_dpl));
	UML_MOV(b, DRC_REG32(ESP), uml::mem(&m_core->ctl_new_esp));

	drc_gen_push32(ctx, uml::mem(&m_core->irq_old_ss_selector));
	drc_gen_push32(ctx, uml::mem(&m_core->irq_old_esp));

	drc_gen_int_finish(ctx, new_eip, ret_eip, cycle_count);
}

void i386_device::drc_gen_int_finish(compiler_state &ctx, const uml::parameter new_eip, const uml::parameter ret_eip, int cycle_count)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &packed_eflags = I0;
	const uml::parameter &new_eip_stage = I7;
	const uml::parameter &new_pc        = I1;

	UML_MOV(b, new_eip_stage, new_eip);
	drc_gen_flags_all(ctx);

	drc_flush_cycles(ctx);

	drc_gen_pack_eflags(b, packed_eflags);
	UML_AND(b, packed_eflags, packed_eflags, 0x00ffffff);
	UML_OR(b, packed_eflags, packed_eflags, EFLAG_RF);

	drc_gen_push32(ctx, packed_eflags);
	drc_gen_push32(ctx, uml::mem(&m_core->irq_old_cs_selector));
	drc_gen_push32(ctx, ret_eip);

	UML_MOV(b, DRC_EIP, new_eip_stage);

	const uml::code_label skip_if_clear = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->irq_gate_is_trap), 0);
	UML_JMPc(b, COND_NZ, skip_if_clear);

	UML_MOV(b, uml::mem(&m_core->IF), 0);
	UML_LABEL(b, skip_if_clear);

	UML_MOV(b, uml::mem(&m_core->NT), 0);
	UML_MOV(b, uml::mem(&m_core->TF), 0);

	drc_flush_cycles(ctx);

	if (cycle_count > 0)
		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, cycle_count);
	else if (cycle_count < 0)
		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, uml::mem(&m_core->pending_cycles));

	UML_ADD(b, new_pc, DRC_EIP, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_pc);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}

void i386_device::drc_gen_soft_int_realmode(compiler_state &ctx, uint32_t vector, uint32_t ret_eip, int trap_cycles)
{
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &working_value = I0;
	const uml::parameter &ivt_entry     = I7;
	const uml::parameter &new_pc        = I1;

	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);

	UML_ADD(b, mem_addr, DRC_IDT(base), vector * 4);
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, ivt_entry, mem_data);

	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, trap_cycles + 4);

	drc_gen_pack_eflags(b, working_value);
	UML_AND(b, working_value, working_value, 0xffff);

	drc_gen_push16(ctx, working_value);
	drc_gen_push16(ctx, DRC_SEG(CS, selector));
	drc_gen_push16(ctx, ret_eip);

	UML_AND(b, working_value, ivt_entry, 0xffff);
	UML_MOV(b, DRC_EIP, working_value);
	UML_SHR(b, working_value, ivt_entry, 16);
	UML_AND(b, DRC_SEG(CS, selector), working_value, 0xffff);

	UML_MOV(b, uml::mem(&m_core->IF), 0);
	UML_MOV(b, uml::mem(&m_core->TF), 0);

	UML_SHL(b, working_value, DRC_SEG(CS, selector), 4);

	const uml::code_label skip_reset_quirk = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->performed_intersegment_jump), 0);
	UML_JMPc(b, COND_NZ, skip_reset_quirk);

	UML_OR(b, working_value, working_value, 0xfff00000);
	UML_LABEL(b, skip_reset_quirk);

	if (m_cpu_version < 0x500)
		UML_MOV(b, DRC_SEG(CS, flags), 0x93);
	UML_MOV(b, DRC_SEG(CS, base), working_value);
	UML_MOV(b, DRC_SEG(CS, d), 0);
	UML_MOV(b, DRC_SEG(CS, valid), 1);

	UML_ADD(b, new_pc, DRC_EIP, DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_pc);

	drc_flush_cycles(ctx);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}

void i386_device::drc_gen_iret_protected(compiler_state &ctx, const uml::code_label bail)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &stack_addr      = I6;
	const uml::parameter &flags_addr      = I7;
	const uml::parameter &mem_addr        = I0;
	const uml::parameter &mem_data        = I2;
	const uml::parameter &cs_eip_pair     = I6;
	const uml::parameter &new_eip         = I1;
	const uml::parameter &new_cs_selector = I0;
	const uml::parameter &rpl_check       = I0;
	const uml::parameter &working_eflags  = I0;
	const uml::parameter &packed_eflags   = I4;
	const uml::parameter &descriptor_lo   = I6;
	const uml::parameter &descriptor_hi   = I7;
	const uml::parameter &access_flags    = I0;
	const uml::parameter &scratch         = I2;
	const uml::parameter &seg_limit       = I5;

	const uml::code_label do_outer_priv = NEW_LBL(ctx);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->VM), 0);
	UML_JMPc(b, COND_NZ, bail);

	UML_CMP(b, uml::mem(&m_core->NT), 0);
	UML_JMPc(b, COND_NZ, bail);

	UML_CMP(b, DRC_SEG(SS, d), 0);
	UML_JMPc(b, COND_Z, bail);

	drc_gen_stack_peek_range(b, ctx.label_ctr, bail, 11);

	UML_MOV(b, stack_addr, uml::mem(&m_core->mem_laddr));
	UML_ADD(b, flags_addr, stack_addr, 8);

	UML_MOV(b, mem_addr, stack_addr);
	UML_CALLH(b, *m_mem_read64);
	UML_DMOV(b, cs_eip_pair, mem_data);

	UML_MOV(b, mem_addr, flags_addr);
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, uml::mem(&m_core->ctl_newflags), mem_data);

	UML_TEST(b, uml::mem(&m_core->ctl_newflags), EFLAG_VM);
	UML_JMPc(b, COND_NZ, bail);

	UML_MOV(b, new_eip, cs_eip_pair);
	UML_MOV(b, uml::mem(&m_core->ctl_new_eip), new_eip);
	UML_DSHR(b, new_cs_selector, cs_eip_pair, 32);
	UML_AND(b, new_cs_selector, new_cs_selector, 0xffff);
	UML_MOV(b, uml::mem(&m_core->ctl_new_cs), new_cs_selector);
	UML_MOV(b, DRC_SCR32, new_cs_selector);

	UML_TEST(b, uml::mem(&m_core->ctl_new_cs), 0xfffc);
	UML_JMPc(b, COND_Z, bail);

	UML_AND(b, rpl_check, uml::mem(&m_core->ctl_new_cs), 3);
	UML_MOV(b, uml::mem(&m_core->ctl_target_dpl), rpl_check);

	UML_CMP(b, rpl_check, uml::mem(&m_core->CPL));
	UML_JMPc(b, COND_B, bail);
	UML_JMPc(b, COND_A, do_outer_priv);

	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);

	drc_flush_cycles(ctx);
	drc_gen_commit_cs_same_priv(b, ctx.label_ctr, new_eip, bail);

	drc_gen_iret_restore_eflags(ctx, working_eflags, packed_eflags);

	UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 12);
	UML_MOV(b, DRC_EIP, uml::mem(&m_core->ctl_new_eip));
	UML_ADD(b, new_eip, uml::mem(&m_core->ctl_new_eip), DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_eip);

	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, uml::mem(&m_core->pending_cycles));

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, do_outer_priv);

	drc_gen_stack_peek_range(b, ctx.label_ctr, bail, 19);

	UML_ADD(b, mem_addr, uml::mem(&m_core->mem_laddr), 12);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read64);
	UML_MOV(b, uml::mem(&m_core->ctl_new_esp), mem_data);
	UML_DSHR(b, scratch, mem_data, 32);
	UML_AND(b, uml::mem(&m_core->ctl_new_ss), scratch, 0xffff);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_cs));

	drc_flush_cycles(ctx);
	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);

	UML_ROLAND(b, access_flags, descriptor_hi, 32 - 8, 0xf0ff);

	UML_AND(b, scratch, access_flags, 0x18);

	UML_CMP(b, scratch, 0x18);
	UML_JMPc(b, COND_NE, bail);

	UML_SHR(b, scratch, access_flags, 5);
	UML_AND(b, scratch, scratch, 3);

	const uml::code_label cs_conforming = NEW_LBL(ctx);
	const uml::code_label cs_priv_ok    = NEW_LBL(ctx);

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

	// i386_check_sreg_validity in UML. Removed for performance. I may add this back but toggled by an option.
	/*
	for (uint32_t reg_idx : { DS, ES, FS, GS })
	{
	    const uml::code_label seg_ok    = NEW_LBL(ctx);
	    const uml::code_label do_null    = NEW_LBL(ctx);
	    const uml::code_label type_ok    = NEW_LBL(ctx);
	    const uml::code_label dpl_check  = NEW_LBL(ctx);

	    UML_TEST(b, DRC_SEG(reg_idx, selector), 0xfffc);
	    UML_JMPc(b, COND_Z, seg_ok);

	    UML_AND(b, seg_type, DRC_SEG(reg_idx, flags), 0x18);

	    UML_CMP(b, seg_type, 0x10);
	    UML_JMPc(b, COND_E, type_ok);

	    UML_CMP(b, seg_type, 0x18);
	    UML_JMPc(b, COND_NE, do_null);

	    UML_TEST(b, DRC_SEG(reg_idx, flags), 0x02);
	    UML_JMPc(b, COND_Z, do_null);

	    UML_LABEL(b, type_ok);

	    UML_CMP(b, seg_type, 0x18);
	    UML_JMPc(b, COND_NE, dpl_check);

	    UML_TEST(b, DRC_SEG(reg_idx, flags), 0x04);
	    UML_JMPc(b, COND_NZ, seg_ok);

	    UML_LABEL(b, dpl_check);
	    UML_SHR(b, seg_dpl, DRC_SEG(reg_idx, flags), 5);
	    UML_AND(b, seg_dpl, seg_dpl, 3);

	    UML_CMP(b, seg_dpl, uml::mem(&m_core->ctl_target_dpl));
	    UML_JMPc(b, COND_B, do_null);

	    UML_AND(b, seg_rpl, DRC_SEG(reg_idx, selector), 3);
	    UML_CMP(b, seg_dpl, seg_rpl);

	    UML_JMPc(b, COND_B, do_null);
	    UML_JMP(b, seg_ok);

	    UML_LABEL(b, do_null);
	    UML_MOV(b, DRC_SEG(reg_idx, selector), 0);
	    UML_MOV(b, DRC_SEG(reg_idx, base), 0);
	    UML_MOV(b, DRC_SEG(reg_idx, limit), 0);
	    UML_MOV(b, DRC_SEG(reg_idx, d), 0);
	    UML_MOV(b, DRC_SEG(reg_idx, valid), 0);

	    UML_LABEL(b, seg_ok);
	}
	*/

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_cs));
	UML_ROLAND(b, packed_eflags, descriptor_hi, 32 - 8, 0xf0ff);

	drc_gen_unpack_descriptor_limit_base_inline(b, ctx.label_ctr, DRC_SEG(CS, limit), DRC_SEG(CS, base), descriptor_lo, descriptor_hi);

	drc_gen_commit_seg_descriptor(b, CS, packed_eflags, descriptor_lo);

	drc_gen_write_descriptor_accessed_inline(b, ctx.label_ctr, descriptor_lo, bail);

	UML_MOV(b, DRC_SCR32, uml::mem(&m_core->ctl_new_ss));

	drc_flush_cycles(ctx);

	drc_gen_privileged_read_descriptor(b, ctx.label_ctr, descriptor_lo, descriptor_hi, bail);

	drc_gen_commit_ss_for_privilege_change(b, ctx.label_ctr, uml::mem(&m_core->ctl_target_dpl), bail);

	drc_gen_iret_restore_eflags(ctx, working_eflags, packed_eflags);

	UML_AND(b, uml::mem(&m_core->CPL), uml::mem(&m_core->ctl_new_cs), 3);
	UML_MOV(b, DRC_REG32(ESP), uml::mem(&m_core->ctl_new_esp));

	UML_MOV(b, DRC_EIP, uml::mem(&m_core->ctl_new_eip));
	UML_ADD(b, new_eip, uml::mem(&m_core->ctl_new_eip), DRC_SEG(CS, base));
	UML_MOV(b, DRC_PC, new_eip);

	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, uml::mem(&m_core->pending_cycles));

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}

void i386_device::drc_gen_iret_restore_eflags(compiler_state &ctx, const uml::parameter working_eflags, const uml::parameter packed_eflags)
{
	drcuml_block &b = ctx.block;

	const uml::code_label priv0   = NEW_LBL(ctx);
	const uml::code_label skip_if = NEW_LBL(ctx);

	UML_MOV(b, working_eflags, uml::mem(&m_core->ctl_newflags));

	UML_CMP(b, uml::mem(&m_core->CPL), 0);
	UML_JMPc(b, COND_E, priv0);

	drc_flush_cycles(ctx);
	drc_gen_pack_eflags(b, packed_eflags);

	UML_ROLINS(b, working_eflags, packed_eflags, 0, EFLAG_IOPL);

	UML_CMP(b, uml::mem(&m_core->CPL), uml::mem(&m_core->IOPL));
	UML_JMPc(b, COND_BE, skip_if);

	UML_ROLINS(b, working_eflags, packed_eflags, 0, EFLAG_IF);
	UML_LABEL(b, skip_if);

	UML_LABEL(b, priv0);

	drc_gen_unpack_eflags(b, working_eflags, packed_eflags);

	drc_gen_clear_flags(ctx);
}

// ----------------------------------------------------------------------------
// INT/INTO/IRET/HLT
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_int(compiler_state &ctx)
{
	offs_t initial_ip = ctx.cursor;

	if (PROTECTED_MODE && ctx.mode == 0)
		return false;

	drcuml_block &b = ctx.block;

	const uint32_t vector  = drc_get_imm8(ctx);
	const uint32_t ret_eip = ctx.eip + ctx.desc->length;

	drc_flush_cycles(ctx);

	drc_gen_flags_all(ctx);

	if (PROTECTED_MODE)
	{
		if (m_drc_options & I386DRC_SKIP_STACKCHECKS)
		{
			UML_MOV(b, DRC_PC, ctx.pc);
			UML_MOV(b, uml::mem(&m_core->soft_int_ret_eip), ret_eip);
			UML_MOV(b, uml::mem(&m_core->soft_int_vector), vector);
			UML_MOV(b, uml::mem(&m_core->pending_cycles), m_cycle_table_rm[CYCLES_INT] + 4);

			UML_CALLH(b, *m_soft_int_deliver_protected);
		}
		else
		{
			drc_gen_control_transfer_cb(ctx, initial_ip, I386_INTERP(i386_int), this);
		}
	}
	else
	{
		drc_gen_soft_int_realmode(ctx, vector, ret_eip, m_cycle_table_rm[CYCLES_INT]);
	}

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_int3(compiler_state &ctx)
{
	offs_t initial_ip = ctx.cursor;

	if (PROTECTED_MODE && ctx.mode == 0)
		return false;

	drcuml_block &b = ctx.block;

	const uint32_t ret_eip = ctx.eip + ctx.desc->length;

	drc_flush_cycles(ctx);

	drc_gen_flags_all(ctx);

	if (PROTECTED_MODE)
	{
		if (m_drc_options & I386DRC_SKIP_STACKCHECKS)
		{
			UML_MOV(b, DRC_PC, ctx.pc);
			UML_MOV(b, uml::mem(&m_core->soft_int_ret_eip), ret_eip);
			UML_MOV(b, uml::mem(&m_core->soft_int_vector), 3);
			UML_MOV(b, uml::mem(&m_core->pending_cycles), m_cycle_table_rm[CYCLES_INT3] + 4);

			UML_CALLH(b, *m_soft_int_deliver_protected);
		}
		else
		{
			drc_gen_control_transfer_cb(ctx, initial_ip, I386_INTERP(i386_int3), this);
		}
	}
	else
	{
		drc_gen_soft_int_realmode(ctx, 3, ret_eip, m_cycle_table_rm[CYCLES_INT3]);
	}

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_into(compiler_state &ctx)
{
	offs_t initial_ip = ctx.cursor;

	if (PROTECTED_MODE && ctx.mode == 0)
		return false;

	drcuml_block &b = ctx.block;

	const uint32_t ret_eip = ctx.eip + ctx.desc->length;
	const offs_t   next_pc = ctx.pc + ctx.desc->length;

	UML_MOV(b, DRC_PC, ctx.pc);
	UML_MOV(b, DRC_EIP, ctx.eip);

	drc_flush_cycles(ctx);

	drc_gen_flags_all(ctx);

	const uml::code_label of_clear = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->OF), 0);
	UML_JMPc(b, COND_E, of_clear);

	if (PROTECTED_MODE)
	{
		if (m_drc_options & I386DRC_SKIP_STACKCHECKS)
		{
			UML_MOV(b, uml::mem(&m_core->soft_int_ret_eip), ret_eip);
			UML_MOV(b, uml::mem(&m_core->soft_int_vector), 4);
			UML_MOV(b, uml::mem(&m_core->pending_cycles), m_cycle_table_rm[CYCLES_INTO_OF1] + 4);

			UML_CALLH(b, *m_soft_int_deliver_protected);
		}
		else
		{
			drc_gen_control_transfer_cb(ctx, initial_ip, I386_INTERP(i386_into), this);
		}
	}
	else
	{
		drc_gen_soft_int_realmode(ctx, 4, ret_eip, m_cycle_table_rm[CYCLES_INTO_OF1]);
	}

	UML_LABEL(b, of_clear);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, m_cycle_table_rm[CYCLES_INTO_OF0]);
	UML_MOV(b, DRC_EIP, ret_eip);
	UML_MOV(b, DRC_PC, next_pc);

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, next_pc);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	ctx.block_ended = true;

	return false;
}

bool i386_device::drc_pri_iret16(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_iret16), this);
}

bool i386_device::drc_pri_iret32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	offs_t initial_ip = ctx.cursor;

	if (PROTECTED_MODE && (m_drc_options & I386DRC_SKIP_STACKCHECKS))
	{
		UML_MOV(b, DRC_PC, ctx.pc);
		UML_MOV(b, DRC_EIP, ctx.eip);

		UML_MOV(b, uml::mem(&m_core->pending_cycles), m_cycle_table_rm[CYCLES_IRET]);

		drc_flush_cycles(ctx);

		UML_CALLH(b, *m_iret_protected);

		// If m_iret_protected fails with bail_result then we try the interpreter
		drc_gen_control_transfer_cb(ctx, initial_ip, I386_INTERP(i386_iret32), this);

		ctx.block_ended = true;

		return false;
	}
	else
	{
		return drc_gen_control_transfer_cb(ctx, initial_ip, I386_INTERP(i386_iret32), this);
	}
}

bool i386_device::drc_pri_hlt(compiler_state &ctx)
{
	return drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_INTERP(i386_hlt), this);
}
