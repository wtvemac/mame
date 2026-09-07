// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

template <typename EmitFlush> inline void i386_device::drc_gen_chunked_tlb_translate(compiler_state &ctx, const uml::parameter &address, const uml::parameter &entry_reg, const uml::parameter &phys_addr, bool is_write, EmitFlush &&flush)
{
	drcuml_block &b = ctx.block;

	const uint32_t tlb_valid_bits = is_write ? (FLAG_VALID | FLAG_DIRTY) : FLAG_VALID;

	const uml::code_label tlb_ok   = NEW_LBL(ctx);
	const uml::code_label got_phys = NEW_LBL(ctx);

	UML_SHR(b, entry_reg, address, 12);
	UML_LOAD(b, entry_reg, vtlb_table(), entry_reg, SIZE_DWORD, SCALE_x4);

	UML_MOV(b, phys_addr, entry_reg);
	UML_AND(b, phys_addr, phys_addr, tlb_valid_bits);

	UML_CMP(b, phys_addr, tlb_valid_bits);
	UML_JMPc(b, COND_E, tlb_ok);

	UML_MOV(b, uml::mem(&m_core->mem_laddr), address);
	UML_MOV(b, uml::mem(&m_core->mem_iswrite), is_write ? 1 : 0);
	drc_flush_cycles(ctx);
	UML_MOV(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_CALLH(b, *m_resolve_tlb);

	const uml::code_label no_fault = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_JMPc(b, COND_E, no_fault);

	flush();
	UML_CALLC(b, I386_CB(drc_tlb_fault_cb), this);
	UML_EXH(b, *m_fault, DRC_PC);
	UML_LABEL(b, no_fault);

	UML_MOV(b, phys_addr, uml::mem(&m_core->mem_paddr));
	UML_JMP(b, got_phys);

	UML_LABEL(b, tlb_ok);

	UML_MOV(b, phys_addr, address);
	UML_ROLINS(b, phys_addr, entry_reg, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_addr, phys_addr, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, got_phys);
}

template <typename OnChunk> inline void i386_device::drc_gen_stos_vector_fill_tiers(compiler_state &ctx, void *fastbase, const uml::parameter &fill_addr, const uml::parameter &fill_vector, const uml::parameter &remaining_bytes, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes, OnChunk &&on_chunk)
{
	drcuml_block &b = ctx.block;

	if (wide_fill_tier_active && (m_drc_options & I386DRC_REP_VECTOR))
	{
		const uml::code_label vec_loop_wide      = NEW_LBL(ctx);
		const uml::code_label vec_loop_wide_exit = NEW_LBL(ctx);

		UML_LABEL(b, vec_loop_wide);

		UML_CMP(b, remaining_bytes, wide_fill_chunk_bytes);
		UML_JMPc(b, COND_B, vec_loop_wide_exit);

		UML_VSTOREW(b, fastbase, fill_addr, fill_vector, wide_fill_chunk_bytes);
		if (m_drc_options & I386DRC_REP_PREFETCH)
			UML_PREFETCH(b, fastbase, fill_addr, 1, uml::HINT_WRITE);

		UML_ADD(b, fill_addr, fill_addr, wide_fill_chunk_bytes);
		UML_SUB(b, remaining_bytes, remaining_bytes, wide_fill_chunk_bytes);
		on_chunk(wide_fill_chunk_bytes);
		UML_JMP(b, vec_loop_wide);

		UML_LABEL(b, vec_loop_wide_exit);
		UML_VZEROU(b);
	}

	const uml::code_label vec_loop       = NEW_LBL(ctx);
	const uml::code_label vec_tail_check = NEW_LBL(ctx);

	UML_LABEL(b, vec_loop);
	if (m_drc_options & I386DRC_REP_VECTOR)
	{
		UML_CMP(b, remaining_bytes, 16);
		UML_JMPc(b, COND_B, vec_tail_check);

		UML_VSTORE(b, fastbase, fill_addr, fill_vector);
		if (m_drc_options & I386DRC_REP_PREFETCH)
			UML_PREFETCH(b, fastbase, fill_addr, 1, uml::HINT_WRITE);
		UML_ADD(b, fill_addr, fill_addr, 16);
		UML_SUB(b, remaining_bytes, remaining_bytes, 16);
		on_chunk(16);

		UML_JMP(b, vec_loop);
	}
	else
	{
		UML_JMP(b, vec_tail_check);
	}

	UML_LABEL(b, vec_tail_check);
}

template <typename OnChunk> inline void i386_device::drc_gen_movs_vector_copy_tiers(compiler_state &ctx, void *src_fastbase, void *dst_fastbase, const uml::parameter &src_addr, const uml::parameter &dst_addr, const uml::parameter &copy_vector, const uml::parameter &remaining_bytes, bool wide_tier_active, uint32_t wide_chunk_bytes, OnChunk &&on_chunk)
{
	drcuml_block &b = ctx.block;

	if (wide_tier_active && (m_drc_options & I386DRC_REP_VECTOR))
	{
		const uml::code_label vector_loop_wide      = NEW_LBL(ctx);
		const uml::code_label vector_loop_wide_exit = NEW_LBL(ctx);

		UML_LABEL(b, vector_loop_wide);

		UML_CMP(b, remaining_bytes, wide_chunk_bytes);
		UML_JMPc(b, COND_B, vector_loop_wide_exit);

		UML_VLOADW(b, copy_vector, src_fastbase, src_addr, wide_chunk_bytes);
		UML_VSTOREW(b, dst_fastbase, dst_addr, copy_vector, wide_chunk_bytes);
		if (m_drc_options & I386DRC_REP_PREFETCH)
		{
			UML_PREFETCH(b, src_fastbase, src_addr, 1, uml::HINT_READ);
			UML_PREFETCH(b, dst_fastbase, dst_addr, 1, uml::HINT_WRITE);
		}

		UML_ADD(b, src_addr, src_addr, wide_chunk_bytes);
		UML_ADD(b, dst_addr, dst_addr, wide_chunk_bytes);
		UML_SUB(b, remaining_bytes, remaining_bytes, wide_chunk_bytes);
		on_chunk(wide_chunk_bytes);

		UML_JMP(b, vector_loop_wide);

		UML_LABEL(b, vector_loop_wide_exit);
		UML_VZEROU(b);
	}

	const uml::code_label vector_loop       = NEW_LBL(ctx);
	const uml::code_label vector_tail_check = NEW_LBL(ctx);

	UML_LABEL(b, vector_loop);
	if (m_drc_options & I386DRC_REP_VECTOR)
	{
		UML_CMP(b, remaining_bytes, 16);
		UML_JMPc(b, COND_B, vector_tail_check);

		UML_VLOAD(b, copy_vector, src_fastbase, src_addr);
		UML_VSTORE(b, dst_fastbase, dst_addr, copy_vector);

		if (m_drc_options & I386DRC_REP_PREFETCH)
		{
			UML_PREFETCH(b, src_fastbase, src_addr, 1, uml::HINT_READ);
			UML_PREFETCH(b, dst_fastbase, dst_addr, 1, uml::HINT_WRITE);
		}

		UML_ADD(b, src_addr, src_addr, 16);
		UML_ADD(b, dst_addr, dst_addr, 16);
		UML_SUB(b, remaining_bytes, remaining_bytes, 16);
		on_chunk(16);

		UML_JMP(b, vector_loop);
	}
	else
	{
		UML_JMP(b, vector_tail_check);
	}

	UML_LABEL(b, vector_tail_check);
}

inline void i386_device::drc_gen_mem_ptr_add(compiler_state &ctx, int reg, const uml::parameter &seg_base)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;

	if (ctx.desc->addr32)
	{
		UML_ADD(b, mem_addr, DRC_REG32(reg), seg_base);
	}
	else
	{
		UML_AND(b, mem_addr, DRC_REG32(reg), 0xffff);
		UML_ADD(b, mem_addr, mem_addr, seg_base);
	}
}

inline void i386_device::drc_gen_mem_ptr_advance(compiler_state &ctx, int reg)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &addr_index     = I0;
	const uml::parameter &src_value_save = I6;

	if (ctx.desc->addr32)
	{
		UML_ADD(b, DRC_REG32(reg), DRC_REG32(reg), addr_index);
	}
	else
	{
		UML_ADD(b, src_value_save, DRC_REG32(reg), addr_index);
		UML_ROLINS(b, DRC_REG32(reg), src_value_save, 0, 0xffff);
	}
}

inline void i386_device::drc_gen_rep_zf_early_exit(compiler_state &ctx, const uml::code_label done)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &cond_result = I0;

	const uml::code_label rep_have_zf = NEW_LBL(ctx);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
	UML_JMPc(b, COND_E, rep_have_zf);

	drc_gen_flags_cc_dispatch(ctx, FLAGS_CC_Z);
	UML_MOV(b, uml::mem(&m_core->ZF), cond_result);
	drc_gen_clear_flags(ctx);
	UML_LABEL(b, rep_have_zf);

	UML_TEST(b, uml::mem(&m_core->ZF), uml::mem(&m_core->ZF));
	// 0xf2 REPNE/REPNZ prefix
	if (ctx.desc->opcode0 == 0xf2)
		UML_JMPc(b, COND_NZ, done);
	// 0xf3 REP/REPE/REPZ prefix
	else
		UML_JMPc(b, COND_Z, done);
}

inline void i386_device::drc_gen_rep_movs_chunked_flush(compiler_state &ctx, const uml::parameter &esi_reg, const uml::parameter &edi_reg, const uml::parameter &ecx_reg, uint32_t shift, int cycles_per_element, bool wide_tier_active, uint32_t wide_chunk_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &flush_src_addr   = I6;
	const uml::parameter &flush_dst_addr   = I7;
	const uml::parameter &flush_remaining  = I9;
	const uml::parameter &copy_vector      = F0;
	const uml::parameter &tail_byte        = I8;
	const uml::parameter &flush_elem_count = I6;

	const uml::code_label no_pending = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_dst_region), 0xffffffff);
	UML_JMPc(b, COND_E, no_pending);

	const uml::code_label flush_done = NEW_LBL(ctx);

	for (int dst_ram = 0; dst_ram < m_fastram_select; dst_ram++)
	{
		fastram_entry &dfi = m_fastram[dst_ram];
		if (dfi.readonly)
			continue;

		for (int src_ram = 0; src_ram < m_fastram_select; src_ram++)
		{
			fastram_entry &sfi = m_fastram[src_ram];

			void *dst_fastbase = (uint8_t *)dfi.base - dfi.start;
			void *src_fastbase = (uint8_t *)sfi.base - sfi.start;

			const uml::code_label not_this_pair = NEW_LBL(ctx);
			const uml::code_label tail_loop     = NEW_LBL(ctx);

			UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_dst_region), dst_ram);
			UML_JMPc(b, COND_NE, not_this_pair);

			UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_src_region), src_ram);
			UML_JMPc(b, COND_NE, not_this_pair);

			UML_MOV(b, flush_src_addr, uml::mem(&m_core->rep_chunked_src_physstart));
			UML_MOV(b, flush_dst_addr, uml::mem(&m_core->rep_chunked_dst_physstart));
			UML_MOV(b, flush_remaining, uml::mem(&m_core->rep_chunked_movs_bytes));

			drc_gen_movs_vector_copy_tiers(
					ctx,
					src_fastbase,
					dst_fastbase,
					flush_src_addr,
					flush_dst_addr,
					copy_vector,
					flush_remaining,
					wide_tier_active,
					wide_chunk_bytes,
					[](u32)
					{
					}
			);

			UML_LABEL(b, tail_loop);

			UML_CMP(b, flush_remaining, 0);
			UML_JMPc(b, COND_Z, flush_done);

			UML_LOAD(b, tail_byte, src_fastbase, flush_src_addr, SIZE_BYTE, SCALE_x1);
			UML_STORE(b, dst_fastbase, flush_dst_addr, tail_byte, SIZE_BYTE, SCALE_x1);
			UML_ADD(b, flush_src_addr, flush_src_addr, 1);
			UML_ADD(b, flush_dst_addr, flush_dst_addr, 1);
			UML_SUB(b, flush_remaining, flush_remaining, 1);
			UML_JMP(b, tail_loop);

			UML_LABEL(b, not_this_pair);
		}
	}

	UML_LABEL(b, flush_done);

	UML_ADD(b, esi_reg, esi_reg, uml::mem(&m_core->rep_chunked_movs_bytes));
	UML_ADD(b, edi_reg, edi_reg, uml::mem(&m_core->rep_chunked_movs_bytes));
	UML_SHR(b, flush_elem_count, uml::mem(&m_core->rep_chunked_movs_bytes), shift);
	UML_SUB(b, ecx_reg, ecx_reg, flush_elem_count);
	UML_MULULW(b, flush_elem_count, flush_elem_count, cycles_per_element);
	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, flush_elem_count);

	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_src_region), 0xffffffff);
	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_dst_region), 0xffffffff);

	UML_LABEL(b, no_pending);
}

inline void i386_device::drc_gen_stos_chunked_fill_region(compiler_state &ctx, void *fastbase, const uml::parameter &flush_addr, const uml::parameter &flush_remaining, const uml::parameter &fill_byte, const uml::parameter &fill_vector, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes, const uml::code_label flush_done)
{
	drcuml_block &b = ctx.block;

	const uml::code_label tail_loop = NEW_LBL(ctx);

	drc_gen_stos_vector_fill_tiers(
			ctx,
			fastbase,
			flush_addr,
			fill_vector,
			flush_remaining,
			wide_fill_tier_active,
			wide_fill_chunk_bytes,
			[](u32)
			{
			}
	);

	UML_LABEL(b, tail_loop);

	UML_CMP(b, flush_remaining, 0);
	UML_JMPc(b, COND_Z, flush_done);

	UML_STORE(b, fastbase, flush_addr, fill_byte, SIZE_BYTE, SCALE_x1);
	UML_ADD(b, flush_addr, flush_addr, 1);
	UML_SUB(b, flush_remaining, flush_remaining, 1);

	UML_JMP(b, tail_loop);
}

inline void i386_device::drc_gen_rep_stos_chunked_flush(compiler_state &ctx, const uml::parameter &edi_reg, const uml::parameter &ecx_reg, uint32_t shift, int cycles_per_element, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &fill_byte        = I5;
	const uml::parameter &fill_vector      = F0;
	const uml::parameter &flush_addr       = I6;
	const uml::parameter &flush_remaining  = I7;
	const uml::parameter &flush_elem_count = I2;

	const uml::code_label no_pending = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_region), 0xffffffff);
	UML_JMPc(b, COND_E, no_pending);

	const uml::code_label flush_done = NEW_LBL(ctx);

	UML_MOV(b, fill_byte, uml::mem(&m_core->rep_chunked_fillvalue));

	if (wide_fill_tier_active)
		UML_VBCASTBW(b, fill_vector, fill_byte, wide_fill_chunk_bytes);
	else
		UML_VBCASTB(b, fill_vector, fill_byte);

	for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
	{
		fastram_entry &fi = m_fastram[ramnum];
		if (fi.readonly)
			continue;

		void *fastbase = (uint8_t *)fi.base - fi.start;

		const uml::code_label not_this_region = NEW_LBL(ctx);

		UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_region), ramnum);
		UML_JMPc(b, COND_NE, not_this_region);

		UML_MOV(b, flush_addr, uml::mem(&m_core->rep_chunked_physstart));
		UML_MOV(b, flush_remaining, uml::mem(&m_core->rep_chunked_bytes));

		drc_gen_stos_chunked_fill_region(ctx, fastbase, flush_addr, flush_remaining, fill_byte, fill_vector, wide_fill_tier_active, wide_fill_chunk_bytes, flush_done);

		UML_LABEL(b, not_this_region);
	}

	UML_LABEL(b, flush_done);

	UML_ADD(b, edi_reg, edi_reg, uml::mem(&m_core->rep_chunked_bytes));
	UML_SHR(b, flush_elem_count, uml::mem(&m_core->rep_chunked_bytes), shift);
	UML_SUB(b, ecx_reg, ecx_reg, flush_elem_count);
	UML_MULULW(b, flush_elem_count, flush_elem_count, cycles_per_element);
	drc_flush_cycles(ctx);
	UML_SUB(b, DRC_CYCLES, DRC_CYCLES, flush_elem_count);

	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_region), 0xffffffff);

	UML_LABEL(b, no_pending);
}

// ----------------------------------------------------------------------------
// MOVS/CMPS/STOS/LODS/SCAS string instructions
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_mem_movs(compiler_state &ctx, uint32_t operand_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data       = I2;
	const uml::parameter &addr_index     = I0;
	const uml::parameter &src_value_save = I6;

	int src_seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	drc_gen_mem_ptr_add(ctx, ESI, DRC_SEG(src_seg, base));
	drc_flush_cycles(ctx);
	switch (operand_bytes)
	{
		case 1:
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, src_value_save, mem_data, 0xff);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_MOV(b, mem_data, src_value_save);
			UML_CALLH(b, *m_mem_write8);
			break;
		case 2:
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, src_value_save, mem_data, 0xffff);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_MOV(b, mem_data, src_value_save);
			UML_CALLH(b, *m_mem_write16);
			break;
		case 4:
		default:
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, src_value_save, mem_data);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_MOV(b, mem_data, src_value_save);
			UML_CALLH(b, *m_mem_write32);
			break;
	}

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_MOVc(b, COND_NZ, addr_index, (~operand_bytes + 1));
	UML_MOVc(b, COND_Z, addr_index, operand_bytes);

	drc_gen_mem_ptr_advance(ctx, ESI);
	drc_gen_mem_ptr_advance(ctx, EDI);

	drc_record_cycles(ctx, CYCLES_MOVS);

	return true;
}

inline bool i386_device::drc_gen_mem_cmps(compiler_state &ctx, uint32_t operand_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr       = I0;
	const uml::parameter &mem_data       = I2;
	const uml::parameter &addr_index     = I0;
	const uml::parameter &src_value_save = I6;
	const uml::parameter &dest_value     = I1;
	const uml::parameter &cmp_result     = I2;

	int src_seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	drc_gen_mem_ptr_add(ctx, ESI, DRC_SEG(src_seg, base));
	drc_flush_cycles(ctx);

	switch (operand_bytes)
	{
		case 1:
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, src_value_save, mem_data, 0xff);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, dest_value, mem_data, 0xff);

			UML_MOV(b, mem_addr, src_value_save);
			UML_SUB(b, cmp_result, mem_addr, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP8, 8);
			break;
		case 2:
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, src_value_save, mem_data, 0xffff);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, dest_value, mem_data, 0xffff);

			UML_MOV(b, mem_addr, src_value_save);
			UML_SUB(b, cmp_result, mem_addr, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP16, 16);
			break;
		case 4:
		default:
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, src_value_save, mem_data);

			drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));

			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, dest_value, mem_data);

			UML_MOV(b, mem_addr, src_value_save);
			UML_SUB(b, cmp_result, mem_addr, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP32, 32);
			break;
	}
	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_MOVc(b, COND_NZ, addr_index, (~operand_bytes + 1));
	UML_MOVc(b, COND_Z, addr_index, operand_bytes);
	drc_gen_mem_ptr_advance(ctx, ESI);
	drc_gen_mem_ptr_advance(ctx, EDI);

	drc_record_cycles(ctx, CYCLES_CMPS);

	return true;
}

inline bool i386_device::drc_gen_mem_stos(compiler_state &ctx, uint32_t operand_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data   = I2;
	const uml::parameter &addr_index = I0;

	drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));
	drc_flush_cycles(ctx);
	switch (operand_bytes)
	{
		case 1:
			UML_AND(b, mem_data, DRC_REG32(EAX), 0xff);
			UML_CALLH(b, *m_mem_write8);
			break;
		case 2:
			UML_AND(b, mem_data, DRC_REG32(EAX), 0xffff);
			UML_CALLH(b, *m_mem_write16);
			break;
		case 4:
		default:
			UML_MOV(b, mem_data, DRC_REG32(EAX));
			UML_CALLH(b, *m_mem_write32);
			break;
	}

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_MOVc(b, COND_NZ, addr_index, (~operand_bytes + 1));
	UML_MOVc(b, COND_Z, addr_index, operand_bytes);

	drc_gen_mem_ptr_advance(ctx, EDI);

	drc_record_cycles(ctx, CYCLES_STOS);

	return true;
}

inline bool i386_device::drc_gen_mem_lods(compiler_state &ctx, uint32_t operand_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data   = I2;
	const uml::parameter &addr_index = I0;

	int src_seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	drc_gen_mem_ptr_add(ctx, ESI, DRC_SEG(src_seg, base));
	drc_flush_cycles(ctx);
	switch (operand_bytes)
	{
		case 1:
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, addr_index, mem_data, 0xff);
			UML_BREG_WRITE(b, AL, addr_index);
			break;
		case 2:
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, addr_index, mem_data, 0xffff);
			UML_ROLINS(b, DRC_REG32(EAX), addr_index, 0, 0xffff);
			break;
		case 4:
		default:
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, DRC_REG32(EAX), mem_data);
			break;
	}

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_MOVc(b, COND_NZ, addr_index, (~operand_bytes + 1));
	UML_MOVc(b, COND_Z, addr_index, operand_bytes);

	drc_gen_mem_ptr_advance(ctx, ESI);

	drc_record_cycles(ctx, CYCLES_LODS);

	return true;
}

inline bool i386_device::drc_gen_mem_scas(compiler_state &ctx, uint32_t operand_bytes)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data   = I2;
	const uml::parameter &addr_index = I0;
	const uml::parameter &dest_value = I1;
	const uml::parameter &cmp_result = I2;

	drc_gen_mem_ptr_add(ctx, EDI, DRC_SEG(ES, base));
	drc_flush_cycles(ctx);
	switch (operand_bytes)
	{
		case 1:
			UML_CALLH(b, *m_mem_read8);
			UML_AND(b, dest_value, mem_data, 0xff);
			UML_AND(b, addr_index, DRC_REG8(AL), 0xff);

			UML_SUB(b, cmp_result, addr_index, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP8, 8);
			break;
		case 2:
			UML_CALLH(b, *m_mem_read16);
			UML_AND(b, dest_value, mem_data, 0xffff);
			UML_AND(b, addr_index, DRC_REG32(EAX), 0xffff);

			UML_SUB(b, cmp_result, addr_index, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP16, 16);
			break;
		case 4:
		default:
			UML_CALLH(b, *m_mem_read32);
			UML_MOV(b, dest_value, mem_data);
			UML_MOV(b, addr_index, DRC_REG32(EAX));

			UML_SUB(b, cmp_result, addr_index, dest_value);

			drc_gen_defer_flags_arith(ctx, FLAGS_OPTYPE_CMP32, 32);
			break;
	}

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_MOVc(b, COND_NZ, addr_index, (~operand_bytes + 1));
	UML_MOVc(b, COND_Z, addr_index, operand_bytes);

	drc_gen_mem_ptr_advance(ctx, EDI);

	drc_record_cycles(ctx, CYCLES_SCAS);

	return true;
}

inline bool i386_device::drc_gen_mem(compiler_state &ctx, uint8_t opcode)
{
	uint32_t operand_bytes;
	if (!(opcode & 1))
		operand_bytes = 1;
	else if (!ctx.desc->op32)
		operand_bytes = 2;
	else
		operand_bytes = 4;

	switch (opcode)
	{
		case 0xa4:
		case 0xa5:
			return drc_gen_mem_movs(ctx, operand_bytes);
		case 0xa6:
		case 0xa7:
			return drc_gen_mem_cmps(ctx, operand_bytes);
		case 0xaa:
		case 0xab:
			return drc_gen_mem_stos(ctx, operand_bytes);
		case 0xac:
		case 0xad:
			return drc_gen_mem_lods(ctx, operand_bytes);
		case 0xae:
		case 0xaf:
			return drc_gen_mem_scas(ctx, operand_bytes);
	}
	return true;
}

bool i386_device::drc_pri_mem32(compiler_state &ctx)
{
	return drc_gen_mem(ctx, ctx.desc->opcode0);
}

bool i386_device::drc_pri_mem16(compiler_state &ctx)
{
	return drc_gen_mem(ctx, ctx.desc->opcode0);
}

// ----------------------------------------------------------------------------
// REP/REPNE prefixed dispatch (to repeat string instructions above)
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_rep_stos_paged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar)
{
	drcuml_block &b = ctx.block;

	if (m_fastram_select == 0 || debugger_enabled())
	{
		UML_JMP(b, bail_to_scalar);
		return;
	}

	const uml::parameter &dest_addr       = I0;
	const uml::parameter &remaining_bytes = I1;
	const uml::parameter  edi_reg         = ctx.desc->addr32 ? DRC_REG32(EDI) : DRC_REG16(DI);
	const uml::parameter  ecx_reg         = ctx.desc->addr32 ? DRC_REG32(ECX) : DRC_REG16(CX);

	const uml::code_label page_loop       = NEW_LBL(ctx);
	const uml::code_label cycles_ok       = NEW_LBL(ctx);
	const uml::code_label scan_done       = NEW_LBL(ctx);
	const uml::code_label handle_straddle = NEW_LBL(ctx);

	const uint32_t shift                 = operand_bytes / 2;
	const uint8_t  str_opcode            = (operand_bytes == 1) ? 0xaa : 0xab;
	const int      cycles_per_element    = m_cycle_table_rm[CYCLES_STOS];
	const uint32_t wide_fill_chunk_bytes = (VECTOR_FILL_CHUNK_BYTES == 0) ? m_drc_uml->max_supported_vector_bytes(true) : VECTOR_FILL_CHUNK_BYTES;
	const bool     wide_fill_tier_active = (wide_fill_chunk_bytes > 16);

	drc_flush_cycles(ctx);

	auto flush = [&]()
	{
		drc_gen_rep_stos_chunked_flush(ctx, edi_reg, ecx_reg, shift, cycles_per_element, wide_fill_tier_active, wide_fill_chunk_bytes);
	};

	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_region), 0xffffffff);

	switch (operand_bytes)
	{
		case 1:
			UML_AND(b, uml::mem(&m_core->rep_chunked_fillvalue), DRC_REG32(EAX), 0xff);
			break;
		case 2:
			UML_AND(b, uml::mem(&m_core->rep_chunked_fillvalue), DRC_REG32(EAX), 0xffff);
			break;
		case 4:
		default:
			UML_MOV(b, uml::mem(&m_core->rep_chunked_fillvalue), DRC_REG32(EAX));
			break;
	}

	if (operand_bytes > 1)
	{
		const uml::parameter &fill_check_lo = I2;
		const uml::parameter &fill_check_hi = I3;

		const uml::code_label uniform = NEW_LBL(ctx);

		if (!ctx.desc->op32)
		{

			UML_AND(b, fill_check_lo, uml::mem(&m_core->rep_chunked_fillvalue), 0xff);
			UML_SHR(b, fill_check_hi, uml::mem(&m_core->rep_chunked_fillvalue), 8);
			UML_AND(b, fill_check_hi, fill_check_hi, 0xff);
			UML_CMP(b, fill_check_lo, fill_check_hi);
		}
		else
		{

			UML_ROL(b, fill_check_lo, uml::mem(&m_core->rep_chunked_fillvalue), 8);
			UML_CMP(b, fill_check_lo, uml::mem(&m_core->rep_chunked_fillvalue));
		}
		UML_JMPc(b, COND_E, uniform);

		UML_JMP(b, bail_to_scalar);

		UML_LABEL(b, uniform);
	}

	if (ctx.desc->addr32)
	{
		UML_ADD(b, dest_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
		UML_SHL(b, remaining_bytes, DRC_REG32(ECX), shift);
	}
	else
	{
		UML_AND(b, dest_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dest_addr, dest_addr, DRC_SEG(ES, base));
		UML_AND(b, remaining_bytes, DRC_REG32(ECX), 0xffff);
		UML_SHL(b, remaining_bytes, remaining_bytes, shift);
	}

	UML_LABEL(b, page_loop);

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_JMPc(b, COND_G, cycles_ok);

	flush();
	drc_flush_cycles(ctx);
	UML_EXH(b, *m_out_of_cycles, rep_pc);

	UML_LABEL(b, cycles_ok);

	UML_CMP(b, remaining_bytes, 0);
	UML_JMPc(b, COND_Z, scan_done);

	const uml::parameter &page_offset = I2;
	const uml::parameter &page_room   = I3;
	const uml::parameter &chunk_bytes = I4;

	UML_AND(b, page_offset, dest_addr, 0xfff);
	UML_SUB(b, page_room, 0x1000, page_offset);

	UML_MOV(b, chunk_bytes, remaining_bytes);

	UML_CMP(b, remaining_bytes, page_room);
	UML_MOVc(b, COND_A, chunk_bytes, page_room);

	UML_CMP(b, chunk_bytes, operand_bytes);
	UML_JMPc(b, COND_B, handle_straddle);

	UML_AND(b, chunk_bytes, chunk_bytes, ~(operand_bytes - 1));

	const uml::parameter &tlb_entry = I2;
	const uml::parameter &phys_addr = I3;

	drc_gen_chunked_tlb_translate(ctx, dest_addr, tlb_entry, phys_addr, true, flush);

	if (!debugger_enabled())
	{
		for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
		{
			fastram_entry &fi = m_fastram[ramnum];
			if (fi.readonly)
				continue;

			const uint32_t region_bytes = fi.end - fi.start + 1;
			if (region_bytes == 0)
				continue;

			const uml::parameter &region_offset    = I2;
			const uml::parameter &region_room      = I6;
			const uml::parameter &pending_end_addr = I6;

			const uml::code_label skip_region = NEW_LBL(ctx);


			UML_CMP(b, chunk_bytes, region_bytes);
			UML_JMPc(b, COND_A, skip_region);

			UML_SUB(b, region_offset, phys_addr, fi.start);
			UML_SUB(b, region_room, region_bytes, chunk_bytes);

			UML_CMP(b, region_offset, region_room);
			UML_JMPc(b, COND_A, skip_region);

			const uml::code_label extend          = NEW_LBL(ctx);
			const uml::code_label start_new       = NEW_LBL(ctx);
			const uml::code_label chunk_accounted = NEW_LBL(ctx);

			UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_region), ramnum);
			UML_JMPc(b, COND_NE, start_new);

			UML_ADD(b, pending_end_addr, uml::mem(&m_core->rep_chunked_physstart), uml::mem(&m_core->rep_chunked_bytes));

			UML_CMP(b, pending_end_addr, phys_addr);
			UML_JMPc(b, COND_E, extend);

			UML_LABEL(b, start_new);
			flush();
			UML_MOV(b, uml::mem(&m_core->rep_chunked_physstart), phys_addr);
			UML_MOV(b, uml::mem(&m_core->rep_chunked_bytes), chunk_bytes);
			UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_region), ramnum);
			UML_JMP(b, chunk_accounted);

			UML_LABEL(b, extend);
			UML_ADD(b, uml::mem(&m_core->rep_chunked_bytes), uml::mem(&m_core->rep_chunked_bytes), chunk_bytes);

			UML_LABEL(b, chunk_accounted);

			UML_ADD(b, dest_addr, dest_addr, chunk_bytes);
			UML_SUB(b, remaining_bytes, remaining_bytes, chunk_bytes);

			UML_JMP(b, page_loop);

			UML_LABEL(b, skip_region);
		}
	}

	flush();
	UML_JMP(b, bail_to_scalar);

	UML_LABEL(b, handle_straddle);

	const uml::parameter &remaining_before_scalar = I5;

	flush();
	UML_MOV(b, remaining_before_scalar, remaining_bytes);
	drc_gen_mem(ctx, str_opcode);
	UML_SUB(b, ecx_reg, ecx_reg, 1);
	if (ctx.desc->addr32)
	{
		UML_ADD(b, dest_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
	}
	else
	{
		UML_AND(b, dest_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dest_addr, dest_addr, DRC_SEG(ES, base));
	}
	UML_SUB(b, remaining_bytes, remaining_before_scalar, operand_bytes);
	UML_JMP(b, page_loop);

	UML_LABEL(b, scan_done);
	flush();
	UML_JMP(b, done);
}

inline void i386_device::drc_gen_rep_stos_nonpaged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &dest_addr       = I0;
	const uml::parameter &remaining_bytes = I1;
	const uml::parameter &region_offset   = I2;
	const uml::parameter &region_room     = I3;
	const uml::parameter &fill_check_lo   = I2;
	const uml::parameter &fill_check_hi   = I3;
	const uml::parameter &fill_value      = I4;
	const uml::parameter &fill_vector     = F0;
	const uml::parameter  edi_reg         = ctx.desc->addr32 ? DRC_REG32(EDI) : DRC_REG16(DI);
	const uml::parameter  ecx_reg         = ctx.desc->addr32 ? DRC_REG32(ECX) : DRC_REG16(CX);

	const uint32_t shift = operand_bytes / 2;

	drc_flush_cycles(ctx);

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_JMPc(b, COND_NZ, bail_to_scalar);

	if (ctx.desc->addr32)
	{
		UML_ADD(b, dest_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
		UML_SHL(b, remaining_bytes, DRC_REG32(ECX), shift);
	}
	else
	{
		UML_AND(b, dest_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dest_addr, dest_addr, DRC_SEG(ES, base));
		UML_AND(b, remaining_bytes, DRC_REG32(ECX), 0xffff);
		UML_SHL(b, remaining_bytes, remaining_bytes, shift);
	}

	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, dest_addr, dest_addr, uml::mem(&m_core->a20_mask));

	const int      cycles_per_element    = m_cycle_table_rm[CYCLES_STOS];
	const uint32_t wide_fill_chunk_bytes = m_drc_uml->max_supported_vector_bytes(true);
	const bool     wide_fill_tier_active = (wide_fill_chunk_bytes > 16);

	if (!debugger_enabled())
	{
		for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
		{
			fastram_entry &fi = m_fastram[ramnum];
			if (fi.readonly)
				continue;

			const uint32_t region_bytes = fi.end - fi.start + 1;
			if (region_bytes == 0)
				continue;

			const uml::code_label next_region = NEW_LBL(ctx);

			UML_CMP(b, remaining_bytes, region_bytes);
			UML_JMPc(b, COND_A, next_region);

			UML_SUB(b, region_offset, dest_addr, fi.start);
			UML_SUB(b, region_room, region_bytes, remaining_bytes);

			UML_CMP(b, region_offset, region_room);
			UML_JMPc(b, COND_A, next_region);

			void *fastbase = (uint8_t *)fi.base - fi.start;

			const uml::code_label fill_loop         = NEW_LBL(ctx);
			const uml::code_label skip_budget_check = NEW_LBL(ctx);
			const uml::code_label vector_uniform    = NEW_LBL(ctx);

			switch (operand_bytes)
			{
				case 1:
					UML_AND(b, fill_value, DRC_REG32(EAX), 0xff);

					UML_JMP(b, vector_uniform);
					break;
				case 2:
					UML_AND(b, fill_value, DRC_REG32(EAX), 0xffff);
					UML_AND(b, fill_check_lo, fill_value, 0xff);
					UML_SHR(b, fill_check_hi, fill_value, 8);
					UML_AND(b, fill_check_hi, fill_check_hi, 0xff);

					UML_CMP(b, fill_check_lo, fill_check_hi);
					UML_JMPc(b, COND_NE, fill_loop);
					break;
				case 4:
				default:
					UML_MOV(b, fill_value, DRC_REG32(EAX));
					UML_ROL(b, fill_check_lo, fill_value, 8);

					UML_CMP(b, fill_check_lo, fill_value);
					UML_JMPc(b, COND_NE, fill_loop);
					break;
			}

			UML_LABEL(b, vector_uniform);

			if (wide_fill_tier_active)
				UML_VBCASTBW(b, fill_vector, fill_value, wide_fill_chunk_bytes);
			else
				UML_VBCASTB(b, fill_vector, fill_value);

			drc_gen_stos_vector_fill_tiers(
					ctx,
					fastbase,
					dest_addr,
					fill_vector,
					remaining_bytes,
					wide_fill_tier_active,
					wide_fill_chunk_bytes,
					[&](uint32_t chunk_bytes)
					{
						UML_ADD(b, edi_reg, edi_reg, chunk_bytes);
						UML_SUB(b, ecx_reg, ecx_reg, chunk_bytes >> shift);
						drc_flush_cycles(ctx);
						UML_SUB(b, DRC_CYCLES, DRC_CYCLES, cycles_per_element * int(chunk_bytes >> shift));
						UML_EXHc(b, COND_LE, *m_out_of_cycles, rep_pc);
					}
			);

			UML_CMP(b, remaining_bytes, 0);
			UML_JMPc(b, COND_NZ, fill_loop);

			UML_JMP(b, done);

			UML_LABEL(b, fill_loop);

			switch (operand_bytes)
			{
				case 1:
					UML_STORE(b, fastbase, dest_addr, fill_value, SIZE_BYTE, SCALE_x1);
					break;
				case 2:
					UML_STORE(b, fastbase, dest_addr, fill_value, SIZE_WORD, SCALE_x1);
					break;
				case 4:
				default:
					UML_STORE(b, fastbase, dest_addr, fill_value, SIZE_DWORD, SCALE_x1);
					break;
			}

			UML_ADD(b, dest_addr, dest_addr, operand_bytes);
			UML_ADD(b, edi_reg, edi_reg, operand_bytes);
			UML_SUB(b, ecx_reg, ecx_reg, 1);
			drc_record_cycles(ctx, CYCLES_STOS);

			UML_TEST(b, ecx_reg, 0xff);
			UML_JMPc(b, COND_NZ, skip_budget_check);

			drc_flush_cycles(ctx);

			UML_CMP(b, DRC_CYCLES, 0);
			UML_EXHc(b, COND_LE, *m_out_of_cycles, rep_pc);

			UML_LABEL(b, skip_budget_check);

			UML_CMP(b, ecx_reg, 0);
			UML_JMPc(b, COND_NZ, fill_loop);

			UML_JMP(b, done);

			UML_LABEL(b, next_region);
		}
	}

	UML_JMP(b, bail_to_scalar);
}

inline void i386_device::drc_gen_rep_movs_paged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar)
{
	drcuml_block &b = ctx.block;

	if (m_fastram_select == 0 || debugger_enabled())
	{
		UML_JMP(b, bail_to_scalar);
		return;
	}

	const uml::parameter &src_addr        = I0;
	const uml::parameter &dst_addr        = I5;
	const uml::parameter &remaining_bytes = I1;
	const uml::parameter  esi_reg         = ctx.desc->addr32 ? DRC_REG32(ESI) : DRC_REG16(SI);
	const uml::parameter  edi_reg         = ctx.desc->addr32 ? DRC_REG32(EDI) : DRC_REG16(DI);
	const uml::parameter  ecx_reg         = ctx.desc->addr32 ? DRC_REG32(ECX) : DRC_REG16(CX);

	const uml::code_label page_loop       = NEW_LBL(ctx);
	const uml::code_label cycles_ok       = NEW_LBL(ctx);
	const uml::code_label scan_done       = NEW_LBL(ctx);
	const uml::code_label handle_straddle = NEW_LBL(ctx);

	const uint32_t shift              = operand_bytes / 2;
	const uint8_t  str_opcode         = (operand_bytes == 1) ? 0xa4 : 0xa5;
	const int      src_seg            = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;
	const int      cycles_per_element = m_cycle_table_rm[CYCLES_MOVS];
	const uint32_t wide_chunk_bytes   = m_drc_uml->max_supported_vector_bytes();
	const bool     wide_tier_active   = (wide_chunk_bytes > 16);

	drc_flush_cycles(ctx);

	auto flush = [&]()
	{
		drc_gen_rep_movs_chunked_flush(ctx, esi_reg, edi_reg, ecx_reg, shift, cycles_per_element, wide_tier_active, wide_chunk_bytes);
	};

	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_src_region), 0xffffffff);
	UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_dst_region), 0xffffffff);

	if (ctx.desc->addr32)
	{
		UML_ADD(b, src_addr, DRC_REG32(ESI), DRC_SEG(src_seg, base));
		UML_ADD(b, dst_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
		UML_SHL(b, remaining_bytes, DRC_REG32(ECX), shift);
	}
	else
	{
		UML_AND(b, src_addr, DRC_REG32(ESI), 0xffff);
		UML_ADD(b, src_addr, src_addr, DRC_SEG(src_seg, base));
		UML_AND(b, dst_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dst_addr, dst_addr, DRC_SEG(ES, base));
		UML_AND(b, remaining_bytes, DRC_REG32(ECX), 0xffff);
		UML_SHL(b, remaining_bytes, remaining_bytes, shift);
	}

	const uml::parameter &src_end = I6;
	const uml::parameter &dst_end = I7;

	const uml::code_label ranges_ok = NEW_LBL(ctx);

	UML_ADD(b, src_end, src_addr, remaining_bytes);
	UML_ADD(b, dst_end, dst_addr, remaining_bytes);

	UML_CMP(b, src_end, dst_addr);
	UML_JMPc(b, COND_BE, ranges_ok);

	UML_CMP(b, dst_end, src_addr);
	UML_JMPc(b, COND_BE, ranges_ok);

	UML_JMP(b, bail_to_scalar);
	UML_LABEL(b, ranges_ok);

	UML_LABEL(b, page_loop);

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_JMPc(b, COND_G, cycles_ok);

	flush();
	drc_flush_cycles(ctx);
	UML_EXH(b, *m_out_of_cycles, rep_pc);

	UML_LABEL(b, cycles_ok);

	UML_CMP(b, remaining_bytes, 0);
	UML_JMPc(b, COND_Z, scan_done);

	const uml::parameter &region_scratch = I6;
	const uml::parameter &src_page_room  = I2;
	const uml::parameter &dst_page_room  = I3;
	const uml::parameter &chunk_bytes    = I4;
	const uml::parameter &src_phys_addr  = I2;
	const uml::parameter &dst_phys_addr  = I3;
	const uml::parameter &region_room    = I7;

	UML_AND(b, region_scratch, src_addr, 0xfff);
	UML_SUB(b, src_page_room, 0x1000, region_scratch);
	UML_AND(b, region_scratch, dst_addr, 0xfff);
	UML_SUB(b, dst_page_room, 0x1000, region_scratch);

	UML_MOV(b, chunk_bytes, remaining_bytes);

	UML_CMP(b, chunk_bytes, src_page_room);
	UML_MOVc(b, COND_A, chunk_bytes, src_page_room);

	UML_CMP(b, chunk_bytes, dst_page_room);
	UML_MOVc(b, COND_A, chunk_bytes, dst_page_room);

	UML_CMP(b, chunk_bytes, operand_bytes);
	UML_JMPc(b, COND_B, handle_straddle);

	UML_AND(b, chunk_bytes, chunk_bytes, ~(operand_bytes - 1));

	drc_gen_chunked_tlb_translate(ctx, src_addr, region_scratch, src_phys_addr, false, flush);
	drc_gen_chunked_tlb_translate(ctx, dst_addr, region_scratch, dst_phys_addr, true, flush);

	if (!debugger_enabled())
	{
		for (int src_ram = 0; src_ram < m_fastram_select; src_ram++)
		{
			fastram_entry &sfi = m_fastram[src_ram];

			const uint32_t src_region_bytes = sfi.end - sfi.start + 1;
			if (src_region_bytes == 0)
				continue;

			const uml::code_label next_src_region = NEW_LBL(ctx);

			UML_CMP(b, chunk_bytes, src_region_bytes);
			UML_JMPc(b, COND_A, next_src_region);

			UML_SUB(b, region_scratch, src_phys_addr, sfi.start);
			UML_SUB(b, region_room, src_region_bytes, chunk_bytes);

			UML_CMP(b, region_scratch, region_room);
			UML_JMPc(b, COND_A, next_src_region);

			for (int dst_ram = 0; dst_ram < m_fastram_select; dst_ram++)
			{
				fastram_entry &dfi = m_fastram[dst_ram];
				if (dfi.readonly)
					continue;

				const uint32_t dst_region_bytes = dfi.end - dfi.start + 1;
				if (dst_region_bytes == 0)
					continue;

				const uml::code_label next_dst_region = NEW_LBL(ctx);

				UML_CMP(b, chunk_bytes, dst_region_bytes);
				UML_JMPc(b, COND_A, next_dst_region);

				UML_SUB(b, region_scratch, dst_phys_addr, dfi.start);
				UML_SUB(b, region_room, dst_region_bytes, chunk_bytes);

				UML_CMP(b, region_scratch, region_room);
				UML_JMPc(b, COND_A, next_dst_region);

				const uml::code_label extend          = NEW_LBL(ctx);
				const uml::code_label start_new       = NEW_LBL(ctx);
				const uml::code_label chunk_accounted = NEW_LBL(ctx);

				UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_src_region), src_ram);
				UML_JMPc(b, COND_NE, start_new);

				UML_CMP(b, uml::mem(&m_core->rep_chunked_pending_dst_region), dst_ram);
				UML_JMPc(b, COND_NE, start_new);

				UML_ADD(b, region_scratch, uml::mem(&m_core->rep_chunked_src_physstart), uml::mem(&m_core->rep_chunked_movs_bytes));

				UML_CMP(b, region_scratch, src_phys_addr);
				UML_JMPc(b, COND_NE, start_new);

				UML_ADD(b, region_scratch, uml::mem(&m_core->rep_chunked_dst_physstart), uml::mem(&m_core->rep_chunked_movs_bytes));

				UML_CMP(b, region_scratch, dst_phys_addr);
				UML_JMPc(b, COND_E, extend);

				UML_LABEL(b, start_new);
				flush();
				UML_MOV(b, uml::mem(&m_core->rep_chunked_src_physstart), src_phys_addr);
				UML_MOV(b, uml::mem(&m_core->rep_chunked_dst_physstart), dst_phys_addr);
				UML_MOV(b, uml::mem(&m_core->rep_chunked_movs_bytes), chunk_bytes);
				UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_src_region), src_ram);
				UML_MOV(b, uml::mem(&m_core->rep_chunked_pending_dst_region), dst_ram);
				UML_JMP(b, chunk_accounted);

				UML_LABEL(b, extend);
				UML_ADD(b, uml::mem(&m_core->rep_chunked_movs_bytes), uml::mem(&m_core->rep_chunked_movs_bytes), chunk_bytes);

				UML_LABEL(b, chunk_accounted);

				UML_ADD(b, src_addr, src_addr, chunk_bytes);
				UML_ADD(b, dst_addr, dst_addr, chunk_bytes);
				UML_SUB(b, remaining_bytes, remaining_bytes, chunk_bytes);

				UML_JMP(b, page_loop);

				UML_LABEL(b, next_dst_region);
			}

			UML_LABEL(b, next_src_region);
		}
	}

	flush();
	UML_JMP(b, bail_to_scalar);

	UML_LABEL(b, handle_straddle);

	const uml::parameter &remaining_before_scalar = I7;

	flush();
	UML_MOV(b, remaining_before_scalar, remaining_bytes);
	drc_gen_mem(ctx, str_opcode);
	UML_SUB(b, ecx_reg, ecx_reg, 1);
	if (ctx.desc->addr32)
	{
		UML_ADD(b, src_addr, DRC_REG32(ESI), DRC_SEG(src_seg, base));
		UML_ADD(b, dst_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
	}
	else
	{
		UML_AND(b, src_addr, DRC_REG32(ESI), 0xffff);
		UML_ADD(b, src_addr, src_addr, DRC_SEG(src_seg, base));
		UML_AND(b, dst_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dst_addr, dst_addr, DRC_SEG(ES, base));
	}
	UML_SUB(b, remaining_bytes, remaining_before_scalar, operand_bytes);
	UML_JMP(b, page_loop);

	UML_LABEL(b, scan_done);
	flush();
	UML_JMP(b, done);
}

inline void i386_device::drc_gen_rep_movs_nonpaged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &src_addr        = I0;
	const uml::parameter &dst_addr        = I5;
	const uml::parameter &remaining_bytes = I1;
	const uml::parameter &src_end         = I2;
	const uml::parameter &dst_end         = I3;
	const uml::parameter &region_offset   = I6;
	const uml::parameter &region_room     = I3;
	const uml::parameter &elem_value      = I4;
	const uml::parameter &copy_vector     = F0;
	const uml::parameter  esi_reg         = ctx.desc->addr32 ? DRC_REG32(ESI) : DRC_REG16(SI);
	const uml::parameter  edi_reg         = ctx.desc->addr32 ? DRC_REG32(EDI) : DRC_REG16(DI);
	const uml::parameter  ecx_reg         = ctx.desc->addr32 ? DRC_REG32(ECX) : DRC_REG16(CX);

	const uint32_t shift              = operand_bytes / 2;
	const int      src_seg            = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;
	const int      cycles_per_element = m_cycle_table_rm[CYCLES_MOVS];
	const uint32_t wide_chunk_bytes   = m_drc_uml->max_supported_vector_bytes();
	const bool     wide_tier_active   = (wide_chunk_bytes > 16);

	drc_flush_cycles(ctx);

	UML_TEST(b, uml::mem(&m_core->DF), 1);
	UML_JMPc(b, COND_NZ, bail_to_scalar);

	if (ctx.desc->addr32)
	{
		UML_ADD(b, src_addr, DRC_REG32(ESI), DRC_SEG(src_seg, base));
		UML_ADD(b, dst_addr, DRC_REG32(EDI), DRC_SEG(ES, base));
		UML_SHL(b, remaining_bytes, DRC_REG32(ECX), shift);
	}
	else
	{
		UML_AND(b, src_addr, DRC_REG32(ESI), 0xffff);
		UML_ADD(b, src_addr, src_addr, DRC_SEG(src_seg, base));
		UML_AND(b, dst_addr, DRC_REG32(EDI), 0xffff);
		UML_ADD(b, dst_addr, dst_addr, DRC_SEG(ES, base));
		UML_AND(b, remaining_bytes, DRC_REG32(ECX), 0xffff);
		UML_SHL(b, remaining_bytes, remaining_bytes, shift);
	}

	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
	{
		UML_AND(b, src_addr, src_addr, uml::mem(&m_core->a20_mask));
		UML_AND(b, dst_addr, dst_addr, uml::mem(&m_core->a20_mask));
	}

	const uml::code_label ranges_ok = NEW_LBL(ctx);

	UML_ADD(b, src_end, src_addr, remaining_bytes);
	UML_ADD(b, dst_end, dst_addr, remaining_bytes);

	UML_CMP(b, src_end, dst_addr);
	UML_JMPc(b, COND_BE, ranges_ok);

	UML_CMP(b, dst_end, src_addr);
	UML_JMPc(b, COND_BE, ranges_ok);

	UML_JMP(b, bail_to_scalar);

	UML_LABEL(b, ranges_ok);

	if (!debugger_enabled())
	{
		for (int dst_ram = 0; dst_ram < m_fastram_select; dst_ram++)
		{
			fastram_entry &dfi = m_fastram[dst_ram];
			if (dfi.readonly)
				continue;

			const uint32_t dst_region_bytes = dfi.end - dfi.start + 1;
			if (dst_region_bytes == 0)
				continue;

			const uml::code_label next_dst_region = NEW_LBL(ctx);

			UML_CMP(b, remaining_bytes, dst_region_bytes);
			UML_JMPc(b, COND_A, next_dst_region);

			UML_SUB(b, region_offset, dst_addr, dfi.start);
			UML_SUB(b, region_room, dst_region_bytes, remaining_bytes);

			UML_CMP(b, region_offset, region_room);
			UML_JMPc(b, COND_A, next_dst_region);

			for (int src_ram = 0; src_ram < m_fastram_select; src_ram++)
			{
				fastram_entry &sfi = m_fastram[src_ram];

				const uint32_t src_region_bytes = sfi.end - sfi.start + 1;
				if (src_region_bytes == 0)
					continue;

				const uml::code_label next_src_region = NEW_LBL(ctx);

				UML_CMP(b, remaining_bytes, src_region_bytes);
				UML_JMPc(b, COND_A, next_src_region);

				UML_SUB(b, region_offset, src_addr, sfi.start);
				UML_SUB(b, region_room, src_region_bytes, remaining_bytes);

				UML_CMP(b, region_offset, region_room);
				UML_JMPc(b, COND_A, next_src_region);

				void *dst_fastbase = (uint8_t *)dfi.base - dfi.start;
				void *src_fastbase = (uint8_t *)sfi.base - sfi.start;

				const uml::code_label copy_loop         = NEW_LBL(ctx);
				const uml::code_label skip_budget_check = NEW_LBL(ctx);

				drc_gen_movs_vector_copy_tiers(
						ctx,
						src_fastbase,
						dst_fastbase,
						src_addr,
						dst_addr,
						copy_vector,
						remaining_bytes,
						wide_tier_active,
						wide_chunk_bytes,
						[&](uint32_t chunk_bytes)
						{
							UML_ADD(b, esi_reg, esi_reg, chunk_bytes);
							UML_ADD(b, edi_reg, edi_reg, chunk_bytes);
							UML_SUB(b, ecx_reg, ecx_reg, chunk_bytes >> shift);
							drc_flush_cycles(ctx);
							UML_SUB(b, DRC_CYCLES, DRC_CYCLES, cycles_per_element * int(chunk_bytes >> shift));
							UML_EXHc(b, COND_LE, *m_out_of_cycles, rep_pc);
						}
				);

				UML_CMP(b, remaining_bytes, 0);
				UML_JMPc(b, COND_NZ, copy_loop);

				UML_JMP(b, done);

				UML_LABEL(b, copy_loop);

				switch (operand_bytes)
				{
					case 1:
						UML_LOAD(b, elem_value, src_fastbase, src_addr, SIZE_BYTE, SCALE_x1);
						UML_STORE(b, dst_fastbase, dst_addr, elem_value, SIZE_BYTE, SCALE_x1);
						break;
					case 2:
						UML_LOAD(b, elem_value, src_fastbase, src_addr, SIZE_WORD, SCALE_x1);
						UML_STORE(b, dst_fastbase, dst_addr, elem_value, SIZE_WORD, SCALE_x1);
						break;
					case 4:
					default:
						UML_LOAD(b, elem_value, src_fastbase, src_addr, SIZE_DWORD, SCALE_x1);
						UML_STORE(b, dst_fastbase, dst_addr, elem_value, SIZE_DWORD, SCALE_x1);
						break;
				}

				UML_ADD(b, src_addr, src_addr, operand_bytes);
				UML_ADD(b, dst_addr, dst_addr, operand_bytes);
				UML_ADD(b, esi_reg, esi_reg, operand_bytes);
				UML_ADD(b, edi_reg, edi_reg, operand_bytes);
				UML_SUB(b, ecx_reg, ecx_reg, 1);

				drc_record_cycles(ctx, CYCLES_MOVS);

				UML_TEST(b, ecx_reg, 0xff);
				UML_JMPc(b, COND_NZ, skip_budget_check);

				drc_flush_cycles(ctx);

				UML_CMP(b, DRC_CYCLES, 0);
				UML_EXHc(b, COND_LE, *m_out_of_cycles, rep_pc);

				UML_LABEL(b, skip_budget_check);

				UML_CMP(b, ecx_reg, 0);
				UML_JMPc(b, COND_NZ, copy_loop);

				UML_JMP(b, done);
				UML_LABEL(b, next_src_region);
			}

			UML_LABEL(b, next_dst_region);
		}
	}

	UML_JMP(b, bail_to_scalar);
}

inline bool i386_device::drc_gen_rep(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter ecx_reg = ctx.desc->addr32 ? DRC_REG32(ECX) : DRC_REG16(CX);

	ctx.cursor++;

	offs_t rep_pc = ctx.pc;

	drc_flush_cycles(ctx);

	const uml::code_label done = NEW_LBL(ctx);

	UML_CMP(b, ecx_reg, 0);
	UML_JMPc(b, COND_Z, done);

	const uml::code_label scalar_loop = NEW_LBL(ctx);

	bool     has_zf = false;
	uint32_t operand_bytes;
	if (!(ctx.desc->opcode1 & 1))
		operand_bytes = 1;
	else if (!ctx.desc->op32)
		operand_bytes = 2;
	else
		operand_bytes = 4;
	switch (ctx.desc->opcode1)
	{
		// 0xa6/0xa7 CMPS
		case 0xa6:
		case 0xa7:
		// 0xae/0xaf SCAS
		case 0xae:
		case 0xaf:
			// Both are ZF-terminated and fall to scalar loop
			has_zf = true;
			drc_gen_clear_flags(ctx);
			break;

		// 0xa4/0xa5 MOVS (w/ vector chunking optimization)
		case 0xa4:
		case 0xa5:
			if (PAGE_MODE_ENABLED)
			{
				UML_TEST(b, uml::mem(&m_core->DF), 1);
				UML_JMPc(b, COND_NZ, scalar_loop);

				drc_gen_rep_movs_paged_chunked(ctx, rep_pc, operand_bytes, done, scalar_loop);
			}
			else
			{
				drc_gen_rep_movs_nonpaged_chunked(ctx, rep_pc, operand_bytes, done, scalar_loop);
			}
			break;

		// 0xaa/0xab STOS (w/ vector chunking optimization)
		case 0xaa:
		case 0xab:
			if (PAGE_MODE_ENABLED)
			{
				UML_TEST(b, uml::mem(&m_core->DF), 1);
				UML_JMPc(b, COND_NZ, scalar_loop);

				drc_gen_rep_stos_paged_chunked(ctx, rep_pc, operand_bytes, done, scalar_loop);
			}
			else
			{
				drc_gen_rep_stos_nonpaged_chunked(ctx, rep_pc, operand_bytes, done, scalar_loop);
			}
			break;

		// 0xac/0xad LODS (falls to interpreter)
		case 0xac:
		case 0xad:
			break;
		default:
			UML_LABEL(b, done);
			return drc_gen_interpreter_fallback(ctx);
	}

	// Simple scalar loop, this is a fallback if the vector method is unavailable or fails

	UML_LABEL(b, scalar_loop);

	drc_gen_mem(ctx, ctx.desc->opcode1);

	UML_SUB(b, ecx_reg, ecx_reg, 1);

	if (has_zf)
		drc_gen_rep_zf_early_exit(ctx, done);

	UML_CMP(b, ecx_reg, 0);
	UML_JMPc(b, COND_Z, done);

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, rep_pc);

	UML_JMP(b, scalar_loop);

	UML_LABEL(b, done);

	return true;
}

bool i386_device::drc_pri_repne16(compiler_state &ctx)
{
	return drc_gen_rep(ctx);
}

bool i386_device::drc_pri_repne32(compiler_state &ctx)
{
	return drc_gen_rep(ctx);
}

bool i386_device::drc_pri_rep16(compiler_state &ctx)
{
	return drc_gen_rep(ctx);
}

bool i386_device::drc_pri_rep32(compiler_state &ctx)
{
	return drc_gen_rep(ctx);
}
