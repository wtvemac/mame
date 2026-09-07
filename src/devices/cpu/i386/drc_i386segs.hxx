// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// Invariant: the shared descriptor read/commit CS/write accessed/unpack handles
// ----------------------------------------------------------------------------

void i386_device::static_generate_read_descriptor()
{
	alloc_handle(*m_drc_uml, m_read_descriptor, "read_descriptor");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_read_descriptor);

	const uml::parameter &selector          = I6;
	const uml::parameter &table_index       = I7;
	const uml::parameter &table_limit_check = I6;
	const uml::parameter &phys_addr         = I7;
	const uml::parameter &page_index        = I6;
	const uml::parameter &tlb_entry         = I6;
	const uml::parameter &tlb_flags_check   = I7;
	const uml::parameter &descriptor_lo     = I6;
	const uml::parameter &descriptor_hi     = I7;
	const uml::parameter &result            = I0;

	int label_ctr = 1;

	const uml::code_label use_ldt        = NEW_SLBL();
	const uml::code_label base_done      = NEW_SLBL();
	const uml::code_label do_nonpaged    = NEW_SLBL();
	const uml::code_label translate_done = NEW_SLBL();
	const uml::code_label fail           = NEW_SLBL();
	const uml::code_label done           = NEW_SLBL();

	UML_AND(b, selector, DRC_SCR32, 0xffff);

	UML_TEST(b, selector, 4);
	UML_JMPc(b, COND_NZ, use_ldt);

	UML_AND(b, table_index, selector, 0xfffffff8);
	UML_ADD(b, table_limit_check, table_index, 7);

	UML_CMP(b, table_limit_check, DRC_GDT(limit));
	UML_JMPc(b, COND_A, fail);

	UML_ADD(b, uml::mem(&m_core->mem_laddr), table_index, DRC_GDT(base));
	UML_JMP(b, base_done);
	UML_LABEL(b, use_ldt);
	UML_AND(b, table_index, selector, 0xfffffff8);
	UML_ADD(b, table_limit_check, table_index, 7);

	UML_CMP(b, table_limit_check, DRC_LDT(limit));
	UML_JMPc(b, COND_A, fail);

	UML_ADD(b, uml::mem(&m_core->mem_laddr), table_index, DRC_LDT(base));
	UML_LABEL(b, base_done);

	UML_TEST(b, uml::mem(&m_core->mem_laddr), 3);
	UML_JMPc(b, COND_NZ, fail);

	// Check if page mode is enabled
	UML_TEST(b, DRC_CR(0), CR0_PG);
	UML_JMPc(b, COND_Z, do_nonpaged);

	UML_SHR(b, page_index, uml::mem(&m_core->mem_laddr), 12);
	UML_LOAD(b, tlb_entry, vtlb_table(), page_index, SIZE_DWORD, SCALE_x4);
	UML_AND(b, tlb_flags_check, tlb_entry, FLAG_VALID | READ_ALLOWED);

	UML_CMP(b, tlb_flags_check, FLAG_VALID | READ_ALLOWED);
	UML_JMPc(b, COND_NE, fail);

	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	UML_ROLINS(b, phys_addr, tlb_entry, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));
	UML_JMP(b, translate_done);

	UML_LABEL(b, do_nonpaged);
	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, translate_done);

	if (m_drc_options & I386DRC_INLINE_FASTRAM)
	{
		const uml::code_label fastram_lo_done = NEW_SLBL();

		drc_gen_inline_fastram32(b, label_ctr, phys_addr, descriptor_lo, fastram_lo_done, false);
		UML_READ_NOFP(b, descriptor_lo, phys_addr, SIZE_DWORD, SPACE_PROGRAM);
		UML_LABEL(b, fastram_lo_done);
	}
	else
	{
		UML_READ_NOFP(b, descriptor_lo, phys_addr, SIZE_DWORD, SPACE_PROGRAM);
	}
	UML_ADD(b, phys_addr, phys_addr, 4);
	if (m_drc_options & I386DRC_INLINE_FASTRAM)
	{
		const uml::code_label fastram_hi_done = NEW_SLBL();

		drc_gen_inline_fastram32(b, label_ctr, phys_addr, descriptor_hi, fastram_hi_done, false);
		UML_READ_NOFP(b, descriptor_hi, phys_addr, SIZE_DWORD, SPACE_PROGRAM);
		UML_LABEL(b, fastram_hi_done);
	}
	else
	{
		UML_READ_NOFP(b, descriptor_hi, phys_addr, SIZE_DWORD, SPACE_PROGRAM);
	}
	UML_MOV(b, result, 1);
	UML_JMP(b, done);

	UML_LABEL(b, fail);
	UML_MOV(b, result, 0);

	UML_LABEL(b, done);
	UML_RET(b);

	b.end();
}


void i386_device::static_generate_commit_cs_same_priv()
{
	alloc_handle(*m_drc_uml, m_commit_cs_same_priv, "commit_cs_same_priv");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_commit_cs_same_priv);

	const uml::parameter &descriptor_lo   = I6;
	const uml::parameter &descriptor_hi   = I7;
	const uml::parameter &access_flags    = I4;
	const uml::parameter &type_field      = I5;
	const uml::parameter &dpl             = I5;
	const uml::parameter &rpl             = I0;
	const uml::parameter &new_access_byte = I6;
	const uml::parameter &phys_addr       = I7;
	const uml::parameter &page_index      = I6;
	const uml::parameter &tlb_entry       = I6;
	const uml::parameter &tlb_flags_check = I7;
	const uml::parameter &new_eip         = I1;
	const uml::parameter &result          = I0;

	int label_ctr = 1;

	const uml::code_label conforming  = NEW_SLBL();
	const uml::code_label priv_ok     = NEW_SLBL();
	const uml::code_label no_g        = NEW_SLBL();
	const uml::code_label do_nonpaged = NEW_SLBL();
	const uml::code_label write_byte  = NEW_SLBL();
	const uml::code_label fail        = NEW_SLBL();
	const uml::code_label done        = NEW_SLBL();

	UML_ROLAND(b, access_flags, descriptor_hi, 32 - 8, 0xf0ff);

	UML_AND(b, type_field, access_flags, 0x18);

	UML_CMP(b, type_field, 0x18);
	UML_JMPc(b, COND_NE, fail);

	UML_TEST(b, access_flags, 0x04);
	UML_JMPc(b, COND_NZ, conforming);

	UML_SHR(b, dpl, access_flags, 5);
	UML_AND(b, dpl, dpl, 3);
	UML_AND(b, rpl, DRC_SCR32, 3);

	UML_CMP(b, dpl, rpl);
	UML_JMPc(b, COND_NE, fail);

	UML_JMP(b, priv_ok);

	UML_LABEL(b, conforming);

	UML_SHR(b, dpl, access_flags, 5);
	UML_AND(b, dpl, dpl, 3);
	UML_AND(b, rpl, DRC_SCR32, 3);

	UML_CMP(b, dpl, rpl);
	UML_JMPc(b, COND_A, fail);

	UML_LABEL(b, priv_ok);

	UML_TEST(b, access_flags, 0x80);
	UML_JMPc(b, COND_Z, fail);

	UML_AND(b, DRC_SEG(CS, limit), descriptor_hi, 0xf0000);
	UML_ROLINS(b, DRC_SEG(CS, limit), descriptor_lo, 0, 0xffff);

	UML_TEST(b, descriptor_hi, 0x00800000);
	UML_JMPc(b, COND_Z, no_g);

	UML_SHL(b, DRC_SEG(CS, limit), DRC_SEG(CS, limit), 12);
	UML_OR(b, DRC_SEG(CS, limit), DRC_SEG(CS, limit), 0xfff);
	UML_LABEL(b, no_g);

	UML_CMP(b, new_eip, DRC_SEG(CS, limit));
	UML_JMPc(b, COND_A, fail);

	UML_AND(b, DRC_SEG(CS, base), descriptor_hi, 0xff000000);
	UML_ROLINS(b, DRC_SEG(CS, base), descriptor_hi, 16, 0x00ff0000);
	UML_ROLINS(b, DRC_SEG(CS, base), descriptor_lo, 16, 0xffff);

	drc_gen_commit_seg_descriptor(b, CS, access_flags, new_access_byte);

	UML_ADD(b, uml::mem(&m_core->mem_laddr), uml::mem(&m_core->mem_laddr), 5);

	// Check if page mode is enabled
	UML_TEST(b, DRC_CR(0), CR0_PG);
	UML_JMPc(b, COND_Z, do_nonpaged);

	UML_MOV(b, DRC_SCR32, new_access_byte);

	UML_SHR(b, page_index, uml::mem(&m_core->mem_laddr), 12);
	UML_LOAD(b, tlb_entry, vtlb_table(), page_index, SIZE_DWORD, SCALE_x4);
	UML_AND(b, tlb_flags_check, tlb_entry, FLAG_VALID | READ_ALLOWED);

	UML_CMP(b, tlb_flags_check, FLAG_VALID | READ_ALLOWED);
	UML_JMPc(b, COND_NE, fail);

	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	UML_ROLINS(b, phys_addr, tlb_entry, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));
	UML_MOV(b, new_access_byte, DRC_SCR32);
	UML_JMP(b, write_byte);

	UML_LABEL(b, do_nonpaged);
	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, write_byte);
	UML_WRITE_NOFP(b, phys_addr, new_access_byte, SIZE_BYTE, SPACE_PROGRAM);
	UML_MOV(b, result, 1);
	UML_JMP(b, done);

	UML_LABEL(b, fail);
	UML_MOV(b, result, 0);

	UML_LABEL(b, done);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_write_descriptor_accessed()
{
	alloc_handle(*m_drc_uml, m_write_descriptor_accessed, "write_descriptor_accessed");

	drcuml_block &b(m_drc_uml->begin_invariant_block(2048));

	UML_HANDLE(b, *m_write_descriptor_accessed);

	const uml::parameter &new_access_byte = I6;
	const uml::parameter &result          = I0;

	int label_ctr = 1;

	const uml::code_label fail = NEW_SLBL();
	const uml::code_label done = NEW_SLBL();

	drc_gen_write_descriptor_accessed_inline(b, label_ctr, new_access_byte, fail);
	UML_MOV(b, result, 1);
	UML_JMP(b, done);

	UML_LABEL(b, fail);
	UML_MOV(b, result, 0);

	UML_LABEL(b, done);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_unpack_descriptor_limit_base()
{
	alloc_handle(*m_drc_uml, m_unpack_descriptor_limit_base, "unpack_descriptor_limit_base");

	drcuml_block &b(m_drc_uml->begin_invariant_block(2048));

	UML_HANDLE(b, *m_unpack_descriptor_limit_base);

	const uml::parameter &descriptor_lo = I6;
	const uml::parameter &descriptor_hi = I7;
	const uml::parameter &limit_out     = I5;
	const uml::parameter &base_out      = I0;

	int label_ctr = 1;

	drc_gen_unpack_descriptor_limit_base_inline(b, label_ctr, limit_out, base_out, descriptor_lo, descriptor_hi);

	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// C++ helpers
// ----------------------------------------------------------------------------

void i386_device::drc_mov_to_sreg_cb()
{
	uint16_t selector = (uint16_t)m_core->data32;
	uint8_t  seg      = (uint8_t)(m_core->data16 & 7);
	drc_enter_interpreter();
	bool fault = false;
	drc_catch_fault_inplace(
			[&]
			{
				i386_sreg_load(selector, seg, &fault);
			}
	);
	if (seg == SS && !fault && m_core->IF != 0)
	{
		m_core->IF                       = 0;
		m_core->delayed_interrupt_enable = 1;
	}
	drc_leave_interpreter();
}

// ----------------------------------------------------------------------------
// UML helpers
// ----------------------------------------------------------------------------

void i386_device::drc_gen_write_descriptor_accessed_inline(drcuml_block &b, int &label_ctr, const uml::parameter access_byte, const uml::code_label take_slow)
{
	const uml::parameter &new_access_byte = I6;
	const uml::parameter &phys_addr       = I7;
	const uml::parameter &page_index      = I6;
	const uml::parameter &tlb_entry       = I6;
	const uml::parameter &tlb_flags_check = I7;

	const uml::code_label do_nonpaged = NEW_SLBL();
	const uml::code_label write_byte  = NEW_SLBL();

	UML_MOV(b, new_access_byte, access_byte);
	UML_ADD(b, uml::mem(&m_core->mem_laddr), uml::mem(&m_core->mem_laddr), 5);

	// Check if page mode is enabled
	UML_TEST(b, DRC_CR(0), CR0_PG);
	UML_JMPc(b, COND_Z, do_nonpaged);

	UML_MOV(b, DRC_SCR32, new_access_byte);

	UML_SHR(b, page_index, uml::mem(&m_core->mem_laddr), 12);
	UML_LOAD(b, tlb_entry, vtlb_table(), page_index, SIZE_DWORD, SCALE_x4);
	UML_AND(b, tlb_flags_check, tlb_entry, FLAG_VALID | READ_ALLOWED);

	UML_CMP(b, tlb_flags_check, FLAG_VALID | READ_ALLOWED);
	UML_JMPc(b, COND_NE, take_slow);

	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	UML_ROLINS(b, phys_addr, tlb_entry, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));
	UML_MOV(b, new_access_byte, DRC_SCR32);
	UML_JMP(b, write_byte);

	UML_LABEL(b, do_nonpaged);
	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_laddr));
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, write_byte);
	UML_WRITE_NOFP(b, phys_addr, new_access_byte, SIZE_BYTE, SPACE_PROGRAM);
}

void i386_device::drc_gen_unpack_descriptor_limit_inline(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi)
{
	const uml::code_label no_g = NEW_SLBL();

	UML_AND(b, limit_dest, descriptor_hi, 0xf0000);
	UML_ROLINS(b, limit_dest, descriptor_lo, 0, 0xffff);

	UML_TEST(b, descriptor_hi, 0x00800000);
	UML_JMPc(b, COND_Z, no_g);

	UML_SHL(b, limit_dest, limit_dest, 12);
	UML_OR(b, limit_dest, limit_dest, 0xfff);

	UML_LABEL(b, no_g);
}

void i386_device::drc_gen_unpack_descriptor_limit_base_inline(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter base_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi)
{
	drc_gen_unpack_descriptor_limit_inline(b, label_ctr, limit_dest, descriptor_lo, descriptor_hi);

	UML_AND(b, base_dest, descriptor_hi, 0xff000000);
	UML_ROLINS(b, base_dest, descriptor_hi, 16, 0x00ff0000);
	UML_ROLINS(b, base_dest, descriptor_lo, 16, 0xffff);
}

inline void i386_device::drc_gen_commit_seg_descriptor(drcuml_block &b, int seg, const uml::parameter access_flags, const uml::parameter new_access_byte)
{
	const uml::parameter &flags_val = I5; // scratch (free at all callers: only used pre-call)

	UML_AND(b, DRC_SEG(seg, selector), DRC_SCR32, 0xffff);

	UML_OR(b, flags_val, access_flags, 1);
	UML_ROLINS(b, DRC_SEG(seg, flags), flags_val, 0, 0xffff);

	UML_MOV(b, DRC_SEG(seg, valid), 1);
	UML_SHR(b, DRC_SEG(seg, d), access_flags, 14);
	UML_AND(b, DRC_SEG(seg, d), DRC_SEG(seg, d), 1);

	UML_AND(b, new_access_byte, access_flags, 0xff);
	UML_OR(b, new_access_byte, new_access_byte, 1);
}

inline void i386_device::drc_gen_read_descriptor(drcuml_block &b, int &label_ctr, const uml::code_label take_slow)
{
	const uml::parameter &result = I0;

	if (PROTECTED_MODE)
	{
		UML_CALLH(b, *m_read_descriptor);

		UML_CMP(b, result, 0);
		UML_JMPc(b, COND_E, take_slow);
	}
	else
	{
		UML_JMP(b, take_slow);
	}
}

void i386_device::drc_gen_privileged_read_descriptor(drcuml_block &b, int &label_ctr, const uml::parameter out_lo, const uml::parameter out_hi, const uml::code_label take_slow)
{
	const uml::parameter &limit_chk = I0;
	const uml::parameter &idx       = I2;
	const uml::parameter &address   = I2;
	const uml::parameter &phys      = I0;

	const uml::code_label use_ldt   = NEW_SLBL();
	const uml::code_label tbl_ready = NEW_SLBL();

	UML_AND(b, idx, DRC_SCR32, 0xfff8);

	UML_TEST(b, DRC_SCR32, 4);
	UML_JMPc(b, COND_NZ, use_ldt);

	UML_ADD(b, limit_chk, idx, 7);

	UML_CMP(b, limit_chk, DRC_GDT(limit));
	UML_JMPc(b, COND_A, take_slow);

	UML_ADD(b, uml::mem(&m_core->mem_laddr), idx, DRC_GDT(base));

	UML_JMP(b, tbl_ready);

	UML_LABEL(b, use_ldt);
	UML_ADD(b, limit_chk, idx, 7);

	UML_CMP(b, limit_chk, DRC_LDT(limit));
	UML_JMPc(b, COND_A, take_slow);

	UML_ADD(b, uml::mem(&m_core->mem_laddr), idx, DRC_LDT(base));

	UML_LABEL(b, tbl_ready);
	UML_MOV(b, address, uml::mem(&m_core->mem_laddr));

	UML_TEST(b, address, 3);
	UML_JMPc(b, COND_NZ, take_slow);

	drc_gen_privileged_walk(b, label_ctr, address, phys, take_slow);

	UML_READ_NOFP(b, out_lo, phys, SIZE_DWORD, SPACE_PROGRAM);
	UML_ADD(b, phys, phys, 4);
	UML_READ_NOFP(b, out_hi, phys, SIZE_DWORD, SPACE_PROGRAM);
}

inline void i386_device::drc_gen_commit_cs_same_priv(drcuml_block &b, int &label_ctr, const uml::parameter new_eip, const uml::code_label take_slow)
{
	const uml::parameter &staged_new_eip = I1;
	const uml::parameter &result         = I0;

	UML_MOV(b, staged_new_eip, new_eip);
	UML_CALLH(b, *m_commit_cs_same_priv);

	UML_CMP(b, result, 0);
	UML_JMPc(b, COND_E, take_slow);
}

inline void i386_device::drc_gen_write_descriptor_accessed(drcuml_block &b, int &label_ctr, const uml::parameter access_byte, const uml::code_label take_slow)
{
	const uml::parameter &new_access_byte = I6;
	const uml::parameter &result          = I0;

	UML_MOV(b, new_access_byte, access_byte);
	UML_CALLH(b, *m_write_descriptor_accessed);

	UML_CMP(b, result, 0);
	UML_JMPc(b, COND_E, take_slow);
}

inline void i386_device::drc_gen_unpack_descriptor_limit_base(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter base_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi)
{
	const uml::parameter &in_lo    = I6;
	const uml::parameter &in_hi    = I7;
	const uml::parameter &out_lmt  = I5;
	const uml::parameter &out_base = I0;

	UML_MOV(b, in_lo, descriptor_lo);
	UML_MOV(b, in_hi, descriptor_hi);
	UML_CALLH(b, *m_unpack_descriptor_limit_base);
	UML_MOV(b, limit_dest, out_lmt);
	UML_MOV(b, base_dest, out_base);
}

void i386_device::drc_gen_commit_ss_for_privilege_change(drcuml_block &b, int &label_ctr, const uml::parameter dpl, const uml::code_label take_slow)
{
	const uml::parameter &descriptor_lo   = I6;
	const uml::parameter &descriptor_hi   = I7;
	const uml::parameter &access_flags    = I4;
	const uml::parameter &rpl_check       = I0;
	const uml::parameter &new_access_byte = I6;

	const uml::code_label writable_data_ok = NEW_SLBL();

	UML_AND(b, rpl_check, DRC_SCR32, 3);

	UML_CMP(b, rpl_check, dpl);
	UML_JMPc(b, COND_NE, take_slow);

	UML_ROLAND(b, access_flags, descriptor_hi, 32 - 8, 0xf0ff);

	UML_SHR(b, rpl_check, access_flags, 5);
	UML_AND(b, rpl_check, rpl_check, 3);

	UML_CMP(b, rpl_check, dpl);
	UML_JMPc(b, COND_NE, take_slow);

	UML_AND(b, rpl_check, access_flags, 0x18);

	UML_CMP(b, rpl_check, 0x10);
	UML_JMPc(b, COND_E, writable_data_ok);

	UML_TEST(b, access_flags, 0x02);
	UML_JMPc(b, COND_NZ, take_slow);

	UML_LABEL(b, writable_data_ok);

	UML_TEST(b, access_flags, 0x80);
	UML_JMPc(b, COND_Z, take_slow);

	drc_gen_unpack_descriptor_limit_base_inline(b, label_ctr, DRC_SEG(SS, limit), DRC_SEG(SS, base), descriptor_lo, descriptor_hi);

	drc_gen_commit_seg_descriptor(b, SS, access_flags, new_access_byte);

	drc_gen_write_descriptor_accessed_inline(b, label_ctr, new_access_byte, take_slow);
}

inline bool i386_device::drc_gen_push_seg32(compiler_state &ctx, int seg_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &seg_sel = I2;

	seg_reg &= 7;

	drc_flush_cycles(ctx);
	UML_MOV(b, seg_sel, DRC_SEG(seg_reg, selector));
	drc_gen_push32(ctx, seg_sel);
	drc_record_cycles(ctx, CYCLES_PUSH_SREG);

	return true;
}

inline bool i386_device::drc_gen_push_seg16(compiler_state &ctx, int seg_reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &seg_sel = I2;

	seg_reg &= 7;

	drc_flush_cycles(ctx);
	UML_MOV(b, seg_sel, DRC_SEG(seg_reg, selector));
	drc_gen_push16(ctx, seg_sel);
	drc_record_cycles(ctx, CYCLES_PUSH_SREG);

	return true;
}

inline bool i386_device::drc_gen_pop_sreg(compiler_state &ctx, int seg, void (*slow_cfunc)(void *))
{
	drcuml_block &b = ctx.block;

	const bool is_ss = (seg == SS);

	if (PROTECTED_MODE)
	{
		const uml::parameter &descriptor_lo   = I3;
		const uml::parameter &descriptor_hi   = I1;
		const uml::parameter &access_flags    = I4;
		const uml::parameter &type_field      = I5;
		const uml::parameter &dpl             = I5;
		const uml::parameter &rpl             = I0;
		const uml::parameter &new_access_byte = I6;
		const uml::parameter &read_desc_lo    = I6;
		const uml::parameter &read_desc_hi    = I7;

		const uml::code_label take_slow = NEW_LBL(ctx);
		const uml::code_label done      = NEW_LBL(ctx);
		const uml::code_label null_sel  = NEW_LBL(ctx);

		UML_MOV(b, DRC_PC, ctx.cursor);
		UML_SUB(b, DRC_EIP, ctx.cursor, DRC_SEG(CS, base));

		drc_flush_cycles(ctx);
		drc_gen_pop_seg_peek(b, ctx.label_ctr, take_slow);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NZ, take_slow);

		UML_TEST(b, DRC_SCR32, 0xfffc);
		UML_JMPc(b, COND_Z, is_ss ? take_slow : null_sel);

		drc_flush_cycles(ctx);
		drc_gen_read_descriptor(b, ctx.label_ctr, take_slow);
		UML_MOV(b, descriptor_lo, read_desc_lo);
		UML_MOV(b, descriptor_hi, read_desc_hi);

		UML_SHR(b, access_flags, descriptor_hi, 8);
		UML_AND(b, access_flags, access_flags, 0xf0ff);

		if (is_ss)
		{
			UML_AND(b, type_field, access_flags, 0x18);

			UML_CMP(b, type_field, 0x10);
			UML_JMPc(b, COND_NE, take_slow);

			UML_TEST(b, access_flags, 0x02);
			UML_JMPc(b, COND_Z, take_slow);

			UML_SHR(b, dpl, access_flags, 5);
			UML_AND(b, dpl, dpl, 3);
			UML_AND(b, rpl, DRC_SCR32, 3);

			UML_CMP(b, rpl, uml::mem(&m_core->CPL));
			UML_JMPc(b, COND_NE, take_slow);

			UML_CMP(b, dpl, uml::mem(&m_core->CPL));
			UML_JMPc(b, COND_NE, take_slow);
		}
		else
		{
			const uml::code_label check2     = NEW_LBL(ctx);
			const uml::code_label check3     = NEW_LBL(ctx);
			const uml::code_label priv_check = NEW_LBL(ctx);

			UML_AND(b, type_field, access_flags, 0x18);

			UML_CMP(b, type_field, 0x10);
			UML_JMPc(b, COND_E, check2);

			UML_TEST(b, access_flags, 0x10);
			UML_JMPc(b, COND_Z, take_slow);

			UML_TEST(b, access_flags, 0x02);
			UML_JMPc(b, COND_Z, check2);

			UML_CMP(b, type_field, 0x18);
			UML_JMPc(b, COND_E, check2);

			UML_JMP(b, take_slow);

			UML_LABEL(b, check2);

			UML_AND(b, type_field, access_flags, 0x18);

			UML_CMP(b, type_field, 0x10);
			UML_JMPc(b, COND_E, priv_check);

			UML_CMP(b, type_field, 0x18);
			UML_JMPc(b, COND_NE, check3);

			UML_TEST(b, access_flags, 0x04);
			UML_JMPc(b, COND_NZ, check3);

			UML_LABEL(b, priv_check);
			UML_SHR(b, dpl, access_flags, 5);
			UML_AND(b, dpl, dpl, 3);
			UML_AND(b, rpl, DRC_SCR32, 3);

			UML_CMP(b, rpl, dpl);
			UML_JMPc(b, COND_A, take_slow);

			UML_CMP(b, uml::mem(&m_core->CPL), dpl);
			UML_JMPc(b, COND_A, take_slow);

			UML_LABEL(b, check3);
		}

		UML_TEST(b, access_flags, 0x80);
		UML_JMPc(b, COND_Z, take_slow);

		drc_gen_unpack_descriptor_limit_base(b, ctx.label_ctr, DRC_SEG(seg, limit), DRC_SEG(seg, base), descriptor_lo, descriptor_hi);

		drc_gen_commit_seg_descriptor(b, seg, access_flags, new_access_byte);
		drc_gen_write_descriptor_accessed(b, ctx.label_ctr, new_access_byte, take_slow);

		UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 4);

		if (is_ss)
		{
			const uml::code_label skip_shadow = NEW_LBL(ctx);

			UML_CMP(b, uml::mem(&m_core->IF), 0);
			UML_JMPc(b, COND_Z, skip_shadow);

			UML_MOV(b, uml::mem(&m_core->IF), 0);
			UML_MOV(b, uml::mem(&m_core->delayed_interrupt_enable), 1);
			UML_LABEL(b, skip_shadow);
		}

		UML_JMP(b, done);

		if (!is_ss)
		{
			UML_LABEL(b, null_sel);

			UML_ADD(b, DRC_REG32(ESP), DRC_REG32(ESP), 4);
			UML_MOV(b, DRC_SEG(seg, selector), 0);
			UML_MOV(b, DRC_SEG(seg, limit), 0);
			UML_MOV(b, DRC_SEG(seg, flags), 0);
			UML_MOV(b, DRC_SEG(seg, base), 0);
			UML_MOV(b, DRC_SEG(seg, valid), 0);
			UML_MOV(b, DRC_SEG(seg, d), 0);
			UML_JMP(b, done);
		}

		UML_LABEL(b, take_slow);
		drc_gen_control_transfer_cb(ctx, ctx.cursor, slow_cfunc, this, false);

		UML_LABEL(b, done);
	}
	else
	{
		drc_gen_control_transfer_cb(ctx, ctx.cursor, slow_cfunc, this, false);
	}

	drc_record_cycles(ctx, CYCLES_POP_SREG);

	return true;
}

inline void i386_device::drc_gen_pop_seg_peek(drcuml_block &b, int &label_ctr, const uml::code_label take_slow)
{
	const uml::parameter &limit_scratch = I7;
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;

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

		UML_ADD(b, limit_scratch, DRC_REG32(ESP), 3);

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

	UML_ADD(b, mem_addr, DRC_REG32(ESP), DRC_SEG(SS, base));
	UML_CALLH(b, *m_mem_read32);
	UML_MOV(b, DRC_SCR32, mem_data);
}

// ----------------------------------------------------------------------------
// LLDT/LTR/VERR/VERW
// ----------------------------------------------------------------------------

void i386_device::drc_lldt_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				if (PROTECTED_MODE && !V8086_MODE)
				{
					if (m_core->CPL)
					{
						m_core->ext = 1;
						i386_trap_with_error(FAULT_GP, 0, 0, 0);
					}
					else
					{
						m_core->ldtr.segment = m_core->data32;
						I386_SREG seg;
						memset(&seg, 0, sizeof(seg));
						seg.selector = m_core->ldtr.segment;
						i386_load_protected_mode_segment(&seg, nullptr);
						m_core->ldtr.limit = seg.limit;
						m_core->ldtr.base  = seg.base;
						m_core->ldtr.flags = seg.flags;
					}
				}
				else
				{
					i386_trap(6, 0);
				}
			}
	);
	drc_leave_interpreter();
}

void i386_device::drc_ltr_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				if (PROTECTED_MODE && !V8086_MODE)
				{
					if (m_core->CPL)
					{
						m_core->ext = 1;
						i386_trap_with_error(FAULT_GP, 0, 0, 0);
					}
					else
					{
						m_core->task.segment = m_core->data32;
						I386_SREG seg;
						memset(&seg, 0, sizeof(seg));
						seg.selector = m_core->task.segment;
						i386_load_protected_mode_segment(&seg, nullptr);

						offs_t        addr       = ((seg.selector & 4) ? m_core->ldtr.base : m_core->gdtr.base) + (seg.selector & ~7) + 5;
						const uint8_t busy_flags = (seg.flags & 0xff) | 2;
						if (void *p = drc_try_fastram(addr, 1, true))
						{
							*(uint8_t *)p = busy_flags;
						}
						else
						{
							i386_translate_address(TR_READ, false, &addr, nullptr);
							m_program->write_byte(addr, busy_flags);
						}

						m_core->task.limit = seg.limit;
						m_core->task.base  = seg.base;
						m_core->task.flags = seg.flags | 2;
					}
				}
				else
				{
					i386_trap(6, 0);
				}
			}
	);
	drc_leave_interpreter();
}

void i386_device::drc_verr_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				if (PROTECTED_MODE && !V8086_MODE)
				{
					uint32_t  address = m_core->data32;
					uint8_t   result;
					I386_SREG seg;
					memset(&seg, 0, sizeof(seg));
					seg.selector = address;
					result       = i386_load_protected_mode_segment(&seg, nullptr);
					if (!(seg.flags & 0x10))
						result = 0;
					if (seg.flags & 0x10)
					{
						if (seg.flags & 0x08)
						{
							if (!(seg.flags & 0x02))
							{
								result = 0;
							}
							else
							{
								if (!(seg.flags & 0x04))
								{

									if (((seg.flags >> 5) & 0x03) < std::max((uint8_t)m_core->CPL, (uint8_t)(address & 0x03)))
										result = 0;
								}
							}
						}
						else
						{
							if (((seg.flags >> 5) & 0x03) < std::max((uint8_t)m_core->CPL, (uint8_t)(address & 0x03)))
								result = 0;
						}
					}
					SetZF(result);
				}
				else
				{
					i386_trap(6, 0);
				}
			}
	);
	drc_leave_interpreter();
}


void i386_device::drc_verw_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				if (PROTECTED_MODE && !V8086_MODE)
				{
					uint32_t  address = m_core->data32;
					uint8_t   result;
					I386_SREG seg;
					memset(&seg, 0, sizeof(seg));
					seg.selector = address;
					result       = i386_load_protected_mode_segment(&seg, nullptr);
					if (!(seg.flags & 0x10))
						result = 0;
					if (seg.flags & 0x10)
					{
						if (seg.flags & 0x08)
						{
							result = 0;
						}
						else
						{
							if (!(seg.flags & 0x02))
								result = 0;
						}
					}
					if (((seg.flags >> 5) & 0x03) < std::max((uint8_t)m_core->CPL, (uint8_t)(address & 0x03)))
						result = 0;
					SetZF(result);
				}
				else
				{
					i386_trap(6, 0);
				}
			}
	);
	drc_leave_interpreter();
}

// ----------------------------------------------------------------------------
// PUSH/POP segment register
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_push_es16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, ES);
}

bool i386_device::drc_pri_push_es32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, ES);
}

bool i386_device::drc_pri_push_cs16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, CS);
}

bool i386_device::drc_pri_push_cs32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, CS);
}

bool i386_device::drc_pri_push_ss16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, SS);
}

bool i386_device::drc_pri_push_ss32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, SS);
}

bool i386_device::drc_pri_push_ds16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, DS);
}

bool i386_device::drc_pri_push_ds32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, DS);
}

bool i386_device::drc_x0f_push_fs16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, FS);
}

bool i386_device::drc_x0f_push_fs32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, FS);
}

bool i386_device::drc_x0f_push_gs16(compiler_state &ctx)
{
	return drc_gen_push_seg16(ctx, GS);
}

bool i386_device::drc_x0f_push_gs32(compiler_state &ctx)
{
	return drc_gen_push_seg32(ctx, GS);
}

bool i386_device::drc_pri_pop_es(compiler_state &ctx)
{
	return drc_gen_pop_sreg(ctx, ES, I386_INTERP(i386_pop_es32));
}

bool i386_device::drc_pri_pop_ss(compiler_state &ctx)
{
	return drc_gen_pop_sreg(ctx, SS, I386_INTERP(i386_pop_ss32));
}

bool i386_device::drc_pri_pop_ds(compiler_state &ctx)
{
	return drc_gen_pop_sreg(ctx, DS, I386_INTERP(i386_pop_ds32));
}

bool i386_device::drc_x0f_pop_fs(compiler_state &ctx)
{
	return drc_gen_pop_sreg(ctx, FS, I386_INTERP(i386_pop_fs32));
}

bool i386_device::drc_x0f_pop_gs(compiler_state &ctx)
{
	return drc_gen_pop_sreg(ctx, GS, I386_INTERP(i386_pop_gs32));
}

// ----------------------------------------------------------------------------
// MOV to segment register
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_mov_to_sreg(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;
	const uml::parameter &rm_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	uint8_t seg   = MRM_REG(modrm);
	if (seg >= 6)
		return false;

	if (!ctx.desc->is_mem)
	{
		UML_AND(b, rm_value, DRC_REG32(MRM_RM32(modrm)), 0xffff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read16);
		UML_AND(b, rm_value, mem_data, 0xffff);
	}

	UML_MOV(b, DRC_SCR32, rm_value);
	UML_MOV(b, DRC_SCR16, seg);

	bool is_reg = MRM_MOD(modrm) == 3;

	drc_record_cycles(ctx, is_reg ? CYCLES_MOV_SREG_REG : CYCLES_MOV_SREG_MEM);

	drc_gen_control_transfer_cb(ctx, ctx.cursor, I386_CB(drc_mov_to_sreg_cb), this, false);

	ctx.block_ended = true;

	return false;
}