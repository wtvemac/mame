// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// Invariant: the shared EFLAGS evaluation and pack handles
// ----------------------------------------------------------------------------

void i386_device::static_generate_flags_eval_cc()
{
	alloc_handle(*m_drc_uml, m_flags_eval_cc, "flags_eval_cc");

	drcuml_block &b(m_drc_uml->begin_invariant_block(24576));

	UML_HANDLE(b, *m_flags_eval_cc);

	const uml::parameter &optype_shifted = I2;
	const uml::parameter &combined_index = I3;
	const uml::parameter &result         = I0;

	int label_ctr = 1;

	// We're slotting in a "x86 condition code" (==, >=, > etc..) for each operation type.
	UML_MOV(b, optype_shifted, uml::mem(&m_core->flags_optype));
	UML_SHL(b, optype_shifted, optype_shifted, FLAGS_CC_SHIFT);
	UML_MOV(b, combined_index, uml::mem(&m_core->flags_cc));
	UML_OR(b, combined_index, combined_index, optype_shifted);

	constexpr uint32_t table_size = FLAGS_OPTYPE_MAX << FLAGS_CC_SHIFT;

	std::array<uml::code_label, table_size> jmpt_table;

	const uml::code_label invalid = NEW_SLBL();

	jmpt_table.fill(invalid);

	// Loop through each operation type, skipping index=0 because FLAGS_OPTYPE_UNKNOWN has nothing to reconstruct
	for (uint32_t optype_idx = 1; optype_idx < FLAGS_OPTYPE_MAX; optype_idx++)
	{
		// Loop through each x86 condition code.
		for (uint32_t cc = 0; cc < FLAGS_CC_MAX; cc++)
		{
			uint32_t jmpt_key = (optype_idx << FLAGS_CC_SHIFT) + cc;

			jmpt_table[jmpt_key] = NEW_SLBL();
		}
	}

	UML_JMPT(b, combined_index, jmpt_table.data(), table_size, I386DRC_JMPT_REC_CHAIN_COUNT);

	for (uint32_t optype_idx = 1; optype_idx < FLAGS_OPTYPE_MAX; optype_idx++)
	{
		for (uint32_t cc = 0; cc < FLAGS_CC_MAX; cc++)
		{
			uint32_t jmpt_key = (optype_idx << FLAGS_CC_SHIFT) + cc;

			UML_LABEL(b, jmpt_table[jmpt_key]);

			drc_gen_eval_flags_cc(b, label_ctr, cc, optype_idx);

			UML_RET(b);
		}
	}

	UML_LABEL(b, invalid);
	UML_MOV(b, result, 0);
	UML_RET(b);

	b.end();
}

void i386_device::static_generate_flags_eval_all()
{
	alloc_handle(*m_drc_uml, m_flags_eval_all, "flags_eval_all");

	drcuml_block &b(m_drc_uml->begin_invariant_block(16384));

	UML_HANDLE(b, *m_flags_eval_all);

	int label_ctr = 1;

	const uml::code_label done = NEW_SLBL();

	std::array<uml::code_label, FLAGS_OPTYPE_MAX> jmpt_table;

	// FLAGS_OPTYPE_UNKNOWN has nothing to compute so it's skipped
	jmpt_table[0] = done;

	for (int optype_idx = 1; optype_idx < FLAGS_OPTYPE_MAX; optype_idx++)
	{
		uint32_t jmpt_key = optype_idx;

		jmpt_table[jmpt_key] = NEW_SLBL();
	}

	UML_JMPT(b, uml::mem(&m_core->flags_optype), jmpt_table.data(), FLAGS_OPTYPE_MAX, I386DRC_JMPT_MAT_CHAIN_COUNT);

	for (uint32_t optype_idx = 1; optype_idx < FLAGS_OPTYPE_MAX; optype_idx++)
	{
		uint32_t jmpt_key = optype_idx;

		UML_LABEL(b, jmpt_table[jmpt_key]);

		drc_gen_eval_flags_all(b, label_ctr, jmpt_key);

		UML_JMP(b, done);
	}

	// Return to a state where there's no pending flags to reconstruct.
	UML_LABEL(b, done);
	UML_MOV(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
	UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);
	UML_RET(b);

	b.end();
}

// Takes each flag member of m_core and creates the eflags register value.
void i386_device::static_generate_pack_eflags()
{
	alloc_handle(*m_drc_uml, m_pack_eflags, "pack_eflags");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_pack_eflags);

	const uml::parameter &dest    = I0;
	const uml::parameter &scratch = I5;

	UML_MOV(b, dest, EFLAG_RESERVED1);
	UML_ROLINS(b, dest, uml::mem(&m_core->CF), EFLAG_CF_SHIFT, EFLAG_CF);
	UML_ROLINS(b, dest, uml::mem(&m_core->PF), EFLAG_PF_SHIFT, EFLAG_PF);
	UML_ROLINS(b, dest, uml::mem(&m_core->AF), EFLAG_AF_SHIFT, EFLAG_AF);
	UML_ROLINS(b, dest, uml::mem(&m_core->ZF), EFLAG_ZF_SHIFT, EFLAG_ZF);
	UML_ROLINS(b, dest, uml::mem(&m_core->SF), EFLAG_SF_SHIFT, EFLAG_SF);
	UML_ROLINS(b, dest, uml::mem(&m_core->TF), EFLAG_TF_SHIFT, EFLAG_TF);
	UML_ROLINS(b, dest, uml::mem(&m_core->IF), EFLAG_IF_SHIFT, EFLAG_IF);
	UML_ROLINS(b, dest, uml::mem(&m_core->DF), EFLAG_DF_SHIFT, EFLAG_DF);
	UML_ROLINS(b, dest, uml::mem(&m_core->OF), EFLAG_OF_SHIFT, EFLAG_OF);
	UML_ROLINS(b, dest, uml::mem(&m_core->IOPL), EFLAG_IOPL_SHIFT, EFLAG_IOPL);
	UML_ROLINS(b, dest, uml::mem(&m_core->NT), EFLAG_NT_SHIFT, EFLAG_NT);
	UML_ROLINS(b, dest, uml::mem(&m_core->RF), EFLAG_RF_SHIFT, EFLAG_RF);
	UML_ROLINS(b, dest, uml::mem(&m_core->VM), EFLAG_VM_SHIFT, EFLAG_VM);
	UML_ROLINS(b, dest, uml::mem(&m_core->AC), EFLAG_AC_SHIFT, EFLAG_AC);
	UML_ROLINS(b, dest, uml::mem(&m_core->VIF), EFLAG_VIF_SHIFT, EFLAG_VIF);
	UML_ROLINS(b, dest, uml::mem(&m_core->VIP), EFLAG_VIP_SHIFT, EFLAG_VIP);
	UML_ROLINS(b, dest, uml::mem(&m_core->ID), EFLAG_ID_SHIFT, EFLAG_ID);

	UML_XOR(b, scratch, uml::mem(&m_core->eflags), dest);
	UML_AND(b, scratch, scratch, uml::mem(&m_core->eflags_mask));
	UML_XOR(b, dest, uml::mem(&m_core->eflags), scratch);

	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// C++ Helpers
// ----------------------------------------------------------------------------

// This does the same thing as drc_gen_flags_all but using C++ rather than DRC UML
// The interpreter doesn't know anything about deferred flags, so all flags must be made available before entering the interpreter
inline void i386_device::drc_flags_all()
{
	if (m_core->flags_optype >= std::size(s_flags_optype_info))
		return;

	const optype_info &info = s_flags_optype_info[m_core->flags_optype];

	// FLAGS_OPTYPE_UNKNOWN is invalid
	if (info.width_bits == 0)
		return;

	const uint32_t op1 = m_core->flags_data_a;
	uint32_t       op2 = 0;
	switch (info.op2)
	{
		case op2_source::direct:
			op2 = m_core->flags_data_b;
			break;
		case op2_source::one:
			op2 = 1;
			break;
		case op2_source::carry:
			op2 = m_core->flags_data_b + m_core->flags_carry;
			break;
		case op2_source::none:
			break;
	}

	const uint32_t result = info.is_logical ? op1 : (info.is_sub ? (op1 - op2) : (op1 + op2));
	const uint32_t mask   = (info.width_bits >= 32) ? 0xffffffff : ((1 << info.width_bits) - 1);

	m_core->ZF = ((result & mask) == 0) ? 1 : 0;
	m_core->SF = (result >> (info.width_bits - 1)) & 1;
	m_core->PF = (uint32_t)i386_parity_table[result & 0xff];

	if (!m_core->flags_of_direct)
	{
		if (info.is_logical)
		{
			m_core->OF = 0;
		}
		else
		{
			const uint32_t sign_bit = info.width_bits - 1;
			const uint32_t of_cond  = info.is_sub ? ((op1 ^ op2) & (op1 ^ result)) : (~(op1 ^ op2) & (op1 ^ result));

			m_core->OF = (of_cond >> sign_bit) & 1;
		}
	}
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_set_pf(drcuml_block &b, const uml::parameter src)
{
	const uml::parameter &low_byte = I4;

	UML_AND(b, low_byte, src, 0xff);
	UML_LOAD(b, low_byte, (void *)i386_parity_table, low_byte, SIZE_DWORD, SCALE_x4);
	UML_MOV(b, uml::mem(&m_core->PF), low_byte);
}


inline void i386_device::drc_gen_flags_rotate(drcuml_block &b)
{
	const uml::parameter &host_flags = I3;

	UML_GETFLGS(b, host_flags, FLAG_C);
	UML_AND(b, uml::mem(&m_core->CF), host_flags, UMLF_C);
}

inline void i386_device::drc_gen_combine_flags(drcuml_block &b, uint8_t cc)
{
	const uml::parameter &cond_result = I0;
	const uml::parameter &sf_xor_of   = I1;

	switch (cc & FLAGS_CC_MASK)
	{

		case FLAGS_CC_O:
			UML_MOV(b, cond_result, uml::mem(&m_core->OF));
			break;
		case FLAGS_CC_NO:
			UML_MOV(b, cond_result, uml::mem(&m_core->OF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_B:
			UML_MOV(b, cond_result, uml::mem(&m_core->CF));
			break;
		case FLAGS_CC_AE:
			UML_MOV(b, cond_result, uml::mem(&m_core->CF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_Z:
			UML_MOV(b, cond_result, uml::mem(&m_core->ZF));
			break;
		case FLAGS_CC_NZ:
			UML_MOV(b, cond_result, uml::mem(&m_core->ZF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_BE:
			UML_MOV(b, cond_result, uml::mem(&m_core->CF));
			UML_OR(b, cond_result, cond_result, uml::mem(&m_core->ZF));
			break;
		case FLAGS_CC_A:
			UML_MOV(b, cond_result, uml::mem(&m_core->CF));
			UML_OR(b, cond_result, cond_result, uml::mem(&m_core->ZF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_S:
			UML_MOV(b, cond_result, uml::mem(&m_core->SF));
			break;
		case FLAGS_CC_NS:
			UML_MOV(b, cond_result, uml::mem(&m_core->SF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_P:
			UML_MOV(b, cond_result, uml::mem(&m_core->PF));
			break;
		case FLAGS_CC_NP:
			UML_MOV(b, cond_result, uml::mem(&m_core->PF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_L:
			UML_MOV(b, cond_result, uml::mem(&m_core->SF));
			UML_XOR(b, cond_result, cond_result, uml::mem(&m_core->OF));
			break;
		case FLAGS_CC_GE:
			UML_MOV(b, cond_result, uml::mem(&m_core->SF));
			UML_XOR(b, cond_result, cond_result, uml::mem(&m_core->OF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
		case FLAGS_CC_LE:
			UML_MOV(b, sf_xor_of, uml::mem(&m_core->SF));
			UML_XOR(b, sf_xor_of, sf_xor_of, uml::mem(&m_core->OF));
			UML_OR(b, cond_result, sf_xor_of, uml::mem(&m_core->ZF));
			break;
		case FLAGS_CC_G:
			UML_MOV(b, sf_xor_of, uml::mem(&m_core->SF));
			UML_XOR(b, sf_xor_of, sf_xor_of, uml::mem(&m_core->OF));
			UML_OR(b, cond_result, sf_xor_of, uml::mem(&m_core->ZF));
			UML_XOR(b, cond_result, cond_result, 1);
			break;
	}
}

// Takes a eflags register value and sets each flag member of m_core.
inline void i386_device::drc_gen_unpack_eflags(drcuml_block &b, const uml::parameter src, const uml::parameter scratch)
{
	UML_AND(b, scratch, src, uml::mem(&m_core->eflags_mask));

	UML_BFXU(b, uml::mem(&m_core->CF), scratch, EFLAG_CF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->PF), scratch, EFLAG_PF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->AF), scratch, EFLAG_AF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->ZF), scratch, EFLAG_ZF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->SF), scratch, EFLAG_SF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->TF), scratch, EFLAG_TF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->IF), scratch, EFLAG_IF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->DF), scratch, EFLAG_DF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->OF), scratch, EFLAG_OF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->IOPL), scratch, EFLAG_IOPL_SHIFT, 2);
	UML_BFXU(b, uml::mem(&m_core->NT), scratch, EFLAG_NT_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->RF), scratch, EFLAG_RF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->VM), scratch, EFLAG_VM_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->AC), scratch, EFLAG_AC_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->VIF), scratch, EFLAG_VIF_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->VIP), scratch, EFLAG_VIP_SHIFT, 1);
	UML_BFXU(b, uml::mem(&m_core->ID), scratch, EFLAG_ID_SHIFT, 1);

	UML_MOV(b, uml::mem(&m_core->eflags), scratch);
}

inline void i386_device::drc_gen_clear_flags(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);

	ctx.compile_time_flags_ready = false;
}

// ----------------------------------------------------------------------------
// Deffering flags helpers, when I386DRC_LAZY_FLAGS is off then calculate all flags
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_defer_flags_arith(compiler_state &ctx, uint32_t optype, int width_bits)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &flags_data_a = I0;
	const uml::parameter &flags_data_b = I1;
	const uml::parameter &flags_result = I2;
	const uml::parameter &carry_bits   = I3;
	const uml::parameter &af_scratch   = I3;

	// Using (slightly) faster UML for these cases since we can use faster registers and skip deferred flag checks
	// Rather than calling drc_gen_eval_flags_all_arith
	if (!(m_drc_options & I386DRC_LAZY_FLAGS) || debugger_enabled())
	{
		if (width_bits == 32)
		{
			const uml::parameter &host_flags = I4;

			UML_GETFLGS(b, host_flags, FLAG_C | FLAG_V | FLAG_Z | FLAG_S);
			UML_AND(b, uml::mem(&m_core->CF), host_flags, UMLF_C);
			UML_BFXU(b, uml::mem(&m_core->OF), host_flags, 1, 1);
			UML_BFXU(b, uml::mem(&m_core->ZF), host_flags, 2, 1);
			UML_BFXU(b, uml::mem(&m_core->SF), host_flags, 3, 1);
		}
		else
		{
			const uml::parameter &of_scratch = I4;
			const uml::parameter &sign_xor   = I5;

			bool is_sub = false;
			switch (optype)
			{
				case FLAGS_OPTYPE_CMP32:
				case FLAGS_OPTYPE_CMP8:
				case FLAGS_OPTYPE_CMP16:
					is_sub = true;
					break;
				default:
					break;
			}

			uint32_t mask     = (1 << width_bits) - 1;
			int      sign_bit = width_bits - 1;

			UML_BFXU(b, uml::mem(&m_core->SF), flags_result, sign_bit, 1);

			UML_TEST(b, flags_result, mask);
			UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

			UML_BFXU(b, uml::mem(&m_core->CF), flags_result, width_bits, 1);

			UML_XOR(b, of_scratch, flags_data_a, flags_data_b);
			if (!is_sub)
			{
				UML_XOR(b, of_scratch, of_scratch, mask);
			}
			UML_XOR(b, sign_xor, flags_data_a, flags_result);
			UML_AND(b, of_scratch, of_scratch, sign_xor);
			UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);
		}

		drc_gen_set_pf(b, flags_result);

		UML_XOR(b, af_scratch, flags_data_a, flags_data_b);
		UML_XOR(b, af_scratch, af_scratch, flags_result);
		UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);
		return;
	}

	UML_MOV(b, uml::mem(&m_core->flags_data_a), flags_data_a);
	UML_MOV(b, uml::mem(&m_core->flags_data_b), flags_data_b);
	UML_MOV(b, uml::mem(&m_core->flags_optype), optype);

	UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);
	if (width_bits == 32)
	{
		UML_GETFLGS(b, carry_bits, FLAG_C);
		UML_AND(b, uml::mem(&m_core->CF), carry_bits, UMLF_C);
	}
	else
	{
		UML_BFXU(b, uml::mem(&m_core->CF), flags_result, width_bits, 1);
	}

	UML_XOR(b, af_scratch, flags_data_a, flags_data_b);
	UML_XOR(b, af_scratch, af_scratch, flags_result);
	UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);

	ctx.compile_time_flags_optype = optype;
	ctx.compile_time_flags_ready  = true;
}

inline void i386_device::drc_gen_defer_flags_logical(compiler_state &ctx, uint32_t optype)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &flags_result = I2;

	// Using (slightly) faster UML for these cases since we can use faster registers and skip deferred flag checks
	// Rather than calling drc_gen_eval_flags_all_arith
	if (!(m_drc_options & I386DRC_LAZY_FLAGS) || debugger_enabled())
	{
		int width_bits = 32;
		switch (optype)
		{
			case FLAGS_OPTYPE_LOGICAL8:
				width_bits = 8;
				break;
			case FLAGS_OPTYPE_LOGICAL16:
				width_bits = 16;
				break;
			case FLAGS_OPTYPE_LOGICAL32:
			default:
				width_bits = 32;
				break;
		}

		uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
		int      sign_bit = width_bits - 1;

		UML_BFXU(b, uml::mem(&m_core->SF), flags_result, sign_bit, 1);

		UML_TEST(b, flags_result, mask);
		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

		UML_MOV(b, uml::mem(&m_core->CF), 0);
		UML_MOV(b, uml::mem(&m_core->OF), 0);

		drc_gen_set_pf(b, flags_result);
		return;
	}

	UML_MOV(b, uml::mem(&m_core->flags_data_a), flags_result);
	UML_MOV(b, uml::mem(&m_core->flags_optype), optype);

	UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);
	UML_MOV(b, uml::mem(&m_core->CF), 0);

	ctx.compile_time_flags_optype = optype;
	ctx.compile_time_flags_ready  = true;
}

inline void i386_device::drc_gen_defer_flags_incdec(compiler_state &ctx, uint32_t optype)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &incdec_result = I2;
	const uml::parameter &orig_value    = I0;
	const uml::parameter &af_scratch    = I3;

	// Using (slightly) faster UML for these cases since we can use faster registers and skip deferred flag checks
	// Rather than calling drc_gen_eval_flags_all_arith
	if (!(m_drc_options & I386DRC_LAZY_FLAGS) || debugger_enabled())
	{
		const uml::parameter &of_scratch = I4;
		const uml::parameter &sign_xor   = I5;

		bool is_dec     = false;
		int  width_bits = 16;

		switch (optype)
		{
			case FLAGS_OPTYPE_DEC32:
				is_dec     = true;
				width_bits = 32;
				break;
			case FLAGS_OPTYPE_DEC8:
				is_dec     = true;
				width_bits = 8;
				break;
			case FLAGS_OPTYPE_DEC16:
				is_dec     = true;
				width_bits = 16;
				break;
			case FLAGS_OPTYPE_INC32:
				width_bits = 32;
				break;
			case FLAGS_OPTYPE_INC8:
				width_bits = 8;
				break;
			case FLAGS_OPTYPE_INC16:
				width_bits = 16;
				break;
			default:
				break;
		}

		uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
		int      sign_bit = width_bits - 1;

		UML_BFXU(b, uml::mem(&m_core->SF), orig_value, sign_bit, 1);

		UML_TEST(b, orig_value, mask);
		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

		UML_XOR(b, of_scratch, incdec_result, 1);
		if (!is_dec)
		{
			UML_XOR(b, of_scratch, of_scratch, mask);
		}
		UML_XOR(b, sign_xor, incdec_result, orig_value);
		UML_AND(b, of_scratch, of_scratch, sign_xor);
		UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);

		drc_gen_set_pf(b, orig_value);

		UML_XOR(b, af_scratch, incdec_result, orig_value);
		UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);
	}
	else
	{
		UML_MOV(b, uml::mem(&m_core->flags_data_a), incdec_result);
		UML_MOV(b, uml::mem(&m_core->flags_optype), optype);
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);

		UML_XOR(b, af_scratch, incdec_result, orig_value);
		UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);

		ctx.compile_time_flags_optype = optype;
		ctx.compile_time_flags_ready  = true;
	}
}

inline void i386_device::drc_gen_defer_flags_adcsbb(compiler_state &ctx, uint32_t optype, int width_bits)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &flags_data_a = I0;
	const uml::parameter &flags_data_b = I1;
	const uml::parameter &flags_result = I2;
	const uml::parameter &carry_bits   = I3;
	const uml::parameter &af_scratch   = I3;

	// Using (slightly) faster UML for these cases since we can use faster registers and skip deferred flag checks
	// Rather than calling drc_gen_eval_flags_all_adcsbb
	if (!(m_drc_options & I386DRC_LAZY_FLAGS) || debugger_enabled())
	{
		if (width_bits == 32)
		{
			const uml::parameter &host_flags = I4;

			UML_GETFLGS(b, host_flags, FLAG_C | FLAG_V | FLAG_Z | FLAG_S);
			UML_AND(b, uml::mem(&m_core->CF), host_flags, UMLF_C);
			UML_BFXU(b, uml::mem(&m_core->OF), host_flags, 1, 1);
			UML_BFXU(b, uml::mem(&m_core->ZF), host_flags, 2, 1);
			UML_BFXU(b, uml::mem(&m_core->SF), host_flags, 3, 1);
		}
		else
		{
			const uml::parameter &of_scratch = I4;
			const uml::parameter &sign_xor   = I5;

			bool is_sub = false;
			switch (optype)
			{
				case FLAGS_OPTYPE_SBB32:
				case FLAGS_OPTYPE_SBB8:
				case FLAGS_OPTYPE_SBB16:
					is_sub = true;
					break;
				default:
					break;
			}

			uint32_t mask     = (1 << width_bits) - 1;
			int      sign_bit = width_bits - 1;

			UML_BFXU(b, uml::mem(&m_core->CF), flags_result, width_bits, 1);
			UML_BFXU(b, uml::mem(&m_core->SF), flags_result, sign_bit, 1);

			UML_TEST(b, flags_result, mask);
			UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

			UML_XOR(b, of_scratch, flags_data_a, flags_data_b);
			if (!is_sub)
			{
				UML_XOR(b, of_scratch, of_scratch, mask);
			}
			UML_XOR(b, sign_xor, flags_data_a, flags_result);
			UML_AND(b, of_scratch, of_scratch, sign_xor);
			UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);
		}

		drc_gen_set_pf(b, flags_result);

		UML_XOR(b, af_scratch, flags_data_a, flags_data_b);
		UML_XOR(b, af_scratch, af_scratch, flags_result);
		UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);
	}
	else
	{
		UML_MOV(b, uml::mem(&m_core->flags_carry), uml::mem(&m_core->CF));
		UML_MOV(b, uml::mem(&m_core->flags_data_a), flags_data_a);
		UML_MOV(b, uml::mem(&m_core->flags_data_b), flags_data_b);
		UML_MOV(b, uml::mem(&m_core->flags_optype), optype);

		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);
		if (width_bits == 32)
		{
			UML_GETFLGS(b, carry_bits, FLAG_C);
			UML_AND(b, uml::mem(&m_core->CF), carry_bits, UMLF_C);
		}
		else
		{
			UML_BFXU(b, uml::mem(&m_core->CF), flags_result, width_bits, 1);
		}

		UML_XOR(b, af_scratch, flags_data_a, flags_data_b);
		UML_XOR(b, af_scratch, af_scratch, flags_result);
		UML_BFXU(b, uml::mem(&m_core->AF), af_scratch, 4, 1);

		ctx.compile_time_flags_optype = optype;
		ctx.compile_time_flags_ready  = true;
	}
}


inline void i386_device::drc_gen_defer_flags_shift(compiler_state &ctx, uint32_t optype, int width_bits)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &host_flags   = I3;
	const uml::parameter &flags_result = I0;

	// Using (slightly) faster UML for these cases since we can use faster registers and skip deferred flag checks
	// Rather than calling drc_gen_eval_flags_all_shift
	if (!(m_drc_options & I386DRC_LAZY_FLAGS) || debugger_enabled())
	{
		if (width_bits >= 32)
		{
			UML_GETFLGS(b, host_flags, (FLAG_C | FLAG_Z | FLAG_S));
			UML_AND(b, uml::mem(&m_core->CF), host_flags, UMLF_C);

			UML_BFXU(b, uml::mem(&m_core->ZF), host_flags, 2, 1);
			UML_BFXU(b, uml::mem(&m_core->SF), host_flags, 3, 1);
		}
		else
		{
			int      sign_bit = width_bits - 1;
			uint32_t mask     = (1 << width_bits) - 1;

			UML_MOV(b, uml::mem(&m_core->OF), 0);

			UML_BFXU(b, uml::mem(&m_core->SF), flags_result, sign_bit, 1);

			UML_TEST(b, flags_result, mask);
			UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
		}

		drc_gen_set_pf(b, flags_result);
	}
	else
	{
		if (width_bits >= 32)
		{
			UML_GETFLGS(b, host_flags, FLAG_C);
			UML_AND(b, uml::mem(&m_core->CF), host_flags, UMLF_C);
		}
		else
		{
			UML_MOV(b, uml::mem(&m_core->OF), 0);
		}

		UML_MOV(b, uml::mem(&m_core->flags_data_a), flags_result);
		UML_MOV(b, uml::mem(&m_core->flags_optype), optype);

		ctx.compile_time_flags_optype = optype;
		ctx.compile_time_flags_ready  = true;
	}
}

inline void i386_device::drc_gen_defer_flags_alu(compiler_state &ctx, uint8_t alu_opcode, int width_bits)
{
	auto get_optype = [width_bits](uint32_t optype8, uint32_t optype16, uint32_t optype32)
	{
		switch (width_bits)
		{
			case 8:
				return optype8;
			case 16:
				return optype16;
			case 32:
			default:
				return optype32;
		}
	};

	switch (alu_opcode)
	{
		case 0:
		{
			uint32_t optype = get_optype(FLAGS_OPTYPE_ADD8, FLAGS_OPTYPE_ADD16, FLAGS_OPTYPE_ADD32);
			drc_gen_defer_flags_arith(ctx, optype, width_bits);
			break;
		}
		case 1:
		case 4:
		case 6:
		{
			uint32_t optype = get_optype(FLAGS_OPTYPE_LOGICAL8, FLAGS_OPTYPE_LOGICAL16, FLAGS_OPTYPE_LOGICAL32);
			drc_gen_defer_flags_logical(ctx, optype);
			break;
		}
		case 5:
		case 7:
		{
			uint32_t optype = get_optype(FLAGS_OPTYPE_CMP8, FLAGS_OPTYPE_CMP16, FLAGS_OPTYPE_CMP32);
			drc_gen_defer_flags_arith(ctx, optype, width_bits);
			break;
		}
		case 2:
		{
			uint32_t optype = get_optype(FLAGS_OPTYPE_ADC8, FLAGS_OPTYPE_ADC16, FLAGS_OPTYPE_ADC32);
			drc_gen_defer_flags_adcsbb(ctx, optype, width_bits);
			break;
		}
		case 3:
		default:
		{
			uint32_t optype = get_optype(FLAGS_OPTYPE_SBB8, FLAGS_OPTYPE_SBB16, FLAGS_OPTYPE_SBB32);
			drc_gen_defer_flags_adcsbb(ctx, optype, width_bits);
			break;
		}
	}
}

// ----------------------------------------------------------------------------
// static_generate_flags_eval_cc helpers
// ----------------------------------------------------------------------------

// Check if flags are pending (otype != unknown) and dispatch to flag eval code depending on set options.
// If flags already set then call drc_gen_combine_flags to populate cond_result with current flag value
inline void i386_device::drc_gen_flags_cc(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::code_label have_pending_flags = NEW_LBL(ctx);
	const uml::code_label done               = NEW_LBL(ctx);

	drc_flush_cycles(ctx);

	UML_CMP(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
	UML_JMPc(b, COND_NE, have_pending_flags);

	drc_gen_combine_flags(b, cc);
	UML_JMP(b, done);

	UML_LABEL(b, have_pending_flags);
	drc_gen_flags_cc_dispatch(ctx, cc);

	UML_LABEL(b, done);
}

inline void i386_device::drc_gen_flags_cc_dispatch(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);

	if ((m_drc_options & I386DRC_INLINE_LAZY_FLAGS) && ctx.compile_time_flags_ready)
	{
		drc_gen_eval_flags_cc(b, ctx.label_ctr, cc, ctx.compile_time_flags_optype);
	}
	else
	{
		UML_MOV(b, uml::mem(&m_core->flags_cc), cc);
		UML_CALLH(b, *m_flags_eval_cc);
	}
}

void i386_device::drc_gen_eval_flags_cc_arith(drcuml_block &b, int &label_ctr, uint8_t cc, bool is_sub, const uml::parameter op2, int width_bits, bool fast_path)
{
	const uml::parameter &recomputed_result = I5;
	const uml::parameter &cond_result       = I0;
	// fast-path add needs a dest but the sum itself is discarded (only the host flags matter)
	const uml::parameter &add_flags_sink    = I4;

	// Condition codes that can exit early
	switch (cc)
	{
		case FLAGS_CC_P:
		case FLAGS_CC_NP:
		{
			if (is_sub)
				UML_SUB(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);
			else
				UML_ADD(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);

			drc_gen_set_pf(b, recomputed_result);

			// Fast drc_gen_combine_flags
			UML_MOV(b, cond_result, uml::mem(&m_core->PF));
			if (cc & FLAGS_CC_INVERT_BIT)
				UML_XOR(b, cond_result, cond_result, 1);

			return;
		}

		case FLAGS_CC_B:
		case FLAGS_CC_AE:
			drc_gen_combine_flags(b, cc);
			return;

		default:
			break;
	}

	// Fast-path optimization for Z, NZ, S, and NS
	if (fast_path)
	{
		switch (cc)
		{
			case FLAGS_CC_Z:
			case FLAGS_CC_NZ:
			case FLAGS_CC_S:
			case FLAGS_CC_NS:
			{
				condition_t cond;
				switch (cc)
				{
					case FLAGS_CC_Z:
						cond = COND_Z;
						break;
					case FLAGS_CC_NZ:
						cond = COND_NZ;
						break;
					case FLAGS_CC_S:
						cond = COND_S;
						break;
					default:
						cond = COND_NS;
						break;
				}

				if (is_sub)
					UML_CMP(b, uml::mem(&m_core->flags_data_a), op2);
				else
					UML_ADD(b, add_flags_sink, uml::mem(&m_core->flags_data_a), op2);

				UML_SETc(b, cond, cond_result);

				// drc_gen_combine_flags not needed, skipping to reduce cycles

				return;
			}
			default:
				break;
		}
	}

	// Set required flags for each condition code
	bool needs_of = false;
	bool needs_zf = false;
	bool needs_sf = false;

	switch (cc)
	{
		case FLAGS_CC_O:
		case FLAGS_CC_NO:
			needs_of = true;
			break;
		case FLAGS_CC_Z:
		case FLAGS_CC_NZ:
		case FLAGS_CC_BE:
		case FLAGS_CC_A:
			needs_zf = true;
			break;
		case FLAGS_CC_S:
		case FLAGS_CC_NS:
			needs_sf = true;
			break;
		case FLAGS_CC_L:
		case FLAGS_CC_GE:
			needs_of = true;
			needs_sf = true;
			break;
		case FLAGS_CC_LE:
		case FLAGS_CC_G:
			needs_of = true;
			needs_zf = true;
			needs_sf = true;
			break;
		default:
			break;
	}

	// Arithmetic operation
	if (fast_path)
	{
		if (is_sub)
			UML_CMP(b, uml::mem(&m_core->flags_data_a), op2);
		else
			UML_ADD(b, add_flags_sink, uml::mem(&m_core->flags_data_a), op2);
	}
	else
	{
		if (is_sub)
			UML_SUB(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);
		else
			UML_ADD(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);
	}

	// Evaluate flags
	if (fast_path)
	{
		const uml::parameter &of_scratch = I3;

		if (needs_of)
			UML_SETc(b, COND_V, of_scratch);
		if (needs_zf)
			UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
		if (needs_sf)
			UML_SETc(b, COND_S, uml::mem(&m_core->SF));

		if (needs_of)
		{
			const uml::code_label of_done = NEW_SLBL();

			UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
			UML_JMPc(b, COND_NZ, of_done);

			UML_MOV(b, uml::mem(&m_core->OF), of_scratch);

			UML_LABEL(b, of_done);
		}
	}
	else
	{
		const uml::parameter &masked_result = I1;
		const uml::parameter &of_scratch    = I2;
		const uml::parameter &sign_xor      = I3;

		uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
		int      sign_bit = width_bits - 1;

		if (needs_sf)
			UML_BFXU(b, uml::mem(&m_core->SF), recomputed_result, sign_bit, 1);

		if (needs_zf)
		{
			UML_AND(b, masked_result, recomputed_result, mask);

			UML_TEST(b, masked_result, mask);
			UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
		}

		if (needs_of)
		{
			UML_XOR(b, of_scratch, uml::mem(&m_core->flags_data_a), op2);
			if (!is_sub)
				UML_XOR(b, of_scratch, of_scratch, mask);
			UML_XOR(b, sign_xor, uml::mem(&m_core->flags_data_a), recomputed_result);
			UML_AND(b, of_scratch, of_scratch, sign_xor);

			const uml::code_label of_done = NEW_SLBL();

			UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
			UML_JMPc(b, COND_NZ, of_done);

			UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);

			UML_LABEL(b, of_done);
		}
	}

	drc_gen_combine_flags(b, cc);
}

void i386_device::drc_gen_eval_flags_cc_adcsbb(drcuml_block &b, int &label_ctr, uint8_t cc, bool is_sub, int width_bits)
{
	const uml::parameter &recomputed_result = I5;
	const uml::parameter &cond_result       = I0;
	const uml::parameter &masked_result     = I1;
	const uml::parameter &of_scratch        = I2;
	const uml::parameter &sign_xor          = I3;

	// Condition codes that can exit early
	switch (cc)
	{
		case FLAGS_CC_B:
		case FLAGS_CC_AE:
			drc_gen_combine_flags(b, cc);
			return;

		default:
			break;
	}

	// Arithmetic operation (with carry)
	if (is_sub)
	{
		UML_SUB(b, recomputed_result, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
		UML_SUB(b, recomputed_result, recomputed_result, uml::mem(&m_core->flags_carry));
	}
	else
	{
		UML_ADD(b, recomputed_result, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
		UML_ADD(b, recomputed_result, recomputed_result, uml::mem(&m_core->flags_carry));
	}

	// Parity condition code can now exit early
	switch (cc)
	{
		case FLAGS_CC_P:
		case FLAGS_CC_NP:
		{
			drc_gen_set_pf(b, recomputed_result);

			// Fast drc_gen_combine_flags
			UML_MOV(b, cond_result, uml::mem(&m_core->PF));
			if (cc & FLAGS_CC_INVERT_BIT)
				UML_XOR(b, cond_result, cond_result, 1);
			return;
		}

		default:
			break;
	}

	// Set required flags for each condition code
	bool needs_of = false;
	bool needs_zf = false;
	bool needs_sf = false;
	switch (cc)
	{
		case FLAGS_CC_O:
		case FLAGS_CC_NO:
			needs_of = true;
			break;
		case FLAGS_CC_Z:
		case FLAGS_CC_NZ:
		case FLAGS_CC_BE:
		case FLAGS_CC_A:
			needs_zf = true;
			break;
		case FLAGS_CC_S:
		case FLAGS_CC_NS:
			needs_sf = true;
			break;
		case FLAGS_CC_L:
		case FLAGS_CC_GE:
			needs_of = true;
			needs_sf = true;
			break;
		case FLAGS_CC_LE:
		case FLAGS_CC_G:
			needs_of = true;
			needs_zf = true;
			needs_sf = true;
			break;
		default:
			break;
	}

	// Evaluate flags

	uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
	int      sign_bit = width_bits - 1;

	if (needs_sf)
		UML_BFXU(b, uml::mem(&m_core->SF), recomputed_result, sign_bit, 1);

	if (needs_zf)
	{
		UML_AND(b, masked_result, recomputed_result, mask);

		UML_TEST(b, masked_result, mask);
		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
	}

	if (needs_of)
	{
		UML_XOR(b, of_scratch, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
		if (!is_sub)
			UML_XOR(b, of_scratch, of_scratch, mask);
		UML_XOR(b, sign_xor, uml::mem(&m_core->flags_data_a), recomputed_result);
		UML_AND(b, of_scratch, of_scratch, sign_xor);

		const uml::code_label of_done = NEW_SLBL();

		UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
		UML_JMPc(b, COND_NZ, of_done);

		UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);

		UML_LABEL(b, of_done);
	}

	drc_gen_combine_flags(b, cc);
}


void i386_device::drc_gen_eval_flags_cc_shift(drcuml_block &b, uint8_t cc, int width_bits)
{
	const uml::parameter &cond_result   = I0;
	const uml::parameter &masked_result = I1;

	// Condition codes that can exit early
	switch (cc)
	{
		case FLAGS_CC_P:
		case FLAGS_CC_NP:
		{
			drc_gen_set_pf(b, uml::mem(&m_core->flags_data_a));

			// Fast drc_gen_combine_flags
			UML_MOV(b, cond_result, uml::mem(&m_core->PF));
			if (cc & FLAGS_CC_INVERT_BIT)
				UML_XOR(b, cond_result, cond_result, 1);

			return;
		}

		default:
			break;
	}

	// Set required flags for each condition code
	bool needs_zf = false;
	bool needs_sf = false;
	switch (cc)
	{
		case FLAGS_CC_Z:
		case FLAGS_CC_NZ:
		case FLAGS_CC_BE:
		case FLAGS_CC_A:
			needs_zf = true;
			break;
		case FLAGS_CC_S:
		case FLAGS_CC_NS:
		case FLAGS_CC_L:
		case FLAGS_CC_GE:
			needs_sf = true;
			break;
		case FLAGS_CC_LE:
		case FLAGS_CC_G:
			needs_zf = true;
			needs_sf = true;
			break;
		default:
			break;
	}

	// Evaluate flags

	uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
	int      sign_bit = width_bits - 1;

	if (needs_sf)
		UML_BFXU(b, uml::mem(&m_core->SF), uml::mem(&m_core->flags_data_a), sign_bit, 1);

	if (needs_zf)
	{
		UML_AND(b, masked_result, uml::mem(&m_core->flags_data_a), mask);

		UML_TEST(b, masked_result, mask);
		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
	}

	drc_gen_combine_flags(b, cc);
}

void i386_device::drc_gen_eval_flags_cc(drcuml_block &b, int &label_ctr, uint8_t cc, uint32_t optype)
{
	switch (optype)
	{
		case FLAGS_OPTYPE_CMP32:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, uml::mem(&m_core->flags_data_b), 32, true);
			break;
		case FLAGS_OPTYPE_ADD32:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, uml::mem(&m_core->flags_data_b), 32, true);
			break;
		case FLAGS_OPTYPE_LOGICAL32:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 0, 32, true);
			break;
		case FLAGS_OPTYPE_CMP8:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, uml::mem(&m_core->flags_data_b), 8, false);
			break;
		case FLAGS_OPTYPE_ADD8:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, uml::mem(&m_core->flags_data_b), 8, false);
			break;
		case FLAGS_OPTYPE_LOGICAL8:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 0, 8, false);
			break;
		case FLAGS_OPTYPE_CMP16:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, uml::mem(&m_core->flags_data_b), 16, false);
			break;
		case FLAGS_OPTYPE_ADD16:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, uml::mem(&m_core->flags_data_b), 16, false);
			break;
		case FLAGS_OPTYPE_LOGICAL16:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 0, 16, false);
			break;
		case FLAGS_OPTYPE_INC32:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, 1, 32, false);
			break;
		case FLAGS_OPTYPE_DEC32:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 1, 32, false);
			break;
		case FLAGS_OPTYPE_INC8:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, 1, 8, false);
			break;
		case FLAGS_OPTYPE_DEC8:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 1, 8, false);
			break;
		case FLAGS_OPTYPE_INC16:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, false, 1, 16, false);
			break;
		case FLAGS_OPTYPE_DEC16:
			drc_gen_eval_flags_cc_arith(b, label_ctr, cc, true, 1, 16, false);
			break;
		case FLAGS_OPTYPE_ADC32:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, false, 32);
			break;
		case FLAGS_OPTYPE_SBB32:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, true, 32);
			break;
		case FLAGS_OPTYPE_ADC8:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, false, 8);
			break;
		case FLAGS_OPTYPE_SBB8:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, true, 8);
			break;
		case FLAGS_OPTYPE_ADC16:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, false, 16);
			break;
		case FLAGS_OPTYPE_SBB16:
			drc_gen_eval_flags_cc_adcsbb(b, label_ctr, cc, true, 16);
			break;
		case FLAGS_OPTYPE_SHIFT32:
			drc_gen_eval_flags_cc_shift(b, cc, 32);
			break;
		case FLAGS_OPTYPE_SHIFT8:
			drc_gen_eval_flags_cc_shift(b, cc, 8);
			break;
		case FLAGS_OPTYPE_SHIFT16:
			drc_gen_eval_flags_cc_shift(b, cc, 16);
			break;
	}
}

// ----------------------------------------------------------------------------
// static_generate_flags_eval_all helpers
// ----------------------------------------------------------------------------

// Check if flags are pending (otype != unknown) and dispatch to flag eval code depending on set options.
inline void i386_device::drc_gen_flags_all(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::code_label no_pending_flags = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
	UML_JMPc(b, COND_E, no_pending_flags);

	drc_gen_flags_all_dispatch(ctx);

	UML_LABEL(b, no_pending_flags);
}

inline void i386_device::drc_gen_flags_all_dispatch(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);

	if ((m_drc_options & I386DRC_INLINE_LAZY_FLAGS) && ctx.compile_time_flags_ready)
	{
		drc_gen_eval_flags_all(b, ctx.label_ctr, ctx.compile_time_flags_optype);
		UML_MOV(b, uml::mem(&m_core->flags_of_direct), 0);
		drc_gen_clear_flags(ctx);
	}
	else
	{
		UML_CALLH(b, *m_flags_eval_all);
	}
}

void i386_device::drc_gen_eval_flags_all_arith(drcuml_block &b, int &label_ctr, bool is_sub, const uml::parameter op2, int width_bits)
{
	const uml::parameter &recomputed_result = I5;
	const uml::parameter &of_scratch        = I4;

	if (is_sub)
		UML_SUB(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);
	else
		UML_ADD(b, recomputed_result, uml::mem(&m_core->flags_data_a), op2);

	if (width_bits == 32)
	{
		const uml::parameter &of_host = I3;

		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
		UML_SETc(b, COND_S, uml::mem(&m_core->SF));
		UML_SETc(b, COND_V, of_host);

		drc_gen_set_pf(b, recomputed_result);

		const uml::code_label of_direct = NEW_SLBL();

		UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
		UML_JMPc(b, COND_NZ, of_direct);

		UML_MOV(b, uml::mem(&m_core->OF), of_host);
		UML_LABEL(b, of_direct);
	}
	else
	{
		const uml::parameter &sign_xor = I3;

		uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
		int      sign_bit = width_bits - 1;

		UML_BFXU(b, uml::mem(&m_core->SF), recomputed_result, sign_bit, 1);

		UML_TEST(b, recomputed_result, mask);
		UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

		drc_gen_set_pf(b, recomputed_result);

		UML_XOR(b, of_scratch, uml::mem(&m_core->flags_data_a), op2);
		if (!is_sub)
			UML_XOR(b, of_scratch, of_scratch, mask);
		UML_XOR(b, sign_xor, uml::mem(&m_core->flags_data_a), recomputed_result);
		UML_AND(b, of_scratch, of_scratch, sign_xor);

		const uml::code_label of_done = NEW_SLBL();

		UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
		UML_JMPc(b, COND_NZ, of_done);

		UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);

		UML_LABEL(b, of_done);
	}
}


void i386_device::drc_gen_eval_flags_all_adcsbb(drcuml_block &b, int &label_ctr, bool is_sub, int width_bits)
{
	const uml::parameter &recomputed_result = I5;
	const uml::parameter &of_scratch        = I4;
	const uml::parameter &sign_xor          = I3;

	if (is_sub)
	{
		UML_SUB(b, recomputed_result, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
		UML_SUB(b, recomputed_result, recomputed_result, uml::mem(&m_core->flags_carry));
	}
	else
	{
		UML_ADD(b, recomputed_result, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
		UML_ADD(b, recomputed_result, recomputed_result, uml::mem(&m_core->flags_carry));
	}

	uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
	int      sign_bit = width_bits - 1;

	UML_BFXU(b, uml::mem(&m_core->SF), recomputed_result, sign_bit, 1);

	UML_TEST(b, recomputed_result, mask);
	UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));

	drc_gen_set_pf(b, recomputed_result);

	UML_XOR(b, of_scratch, uml::mem(&m_core->flags_data_a), uml::mem(&m_core->flags_data_b));
	if (!is_sub)
		UML_XOR(b, of_scratch, of_scratch, mask);
	UML_XOR(b, sign_xor, uml::mem(&m_core->flags_data_a), recomputed_result);
	UML_AND(b, of_scratch, of_scratch, sign_xor);

	const uml::code_label of_done = NEW_SLBL();

	UML_CMP(b, uml::mem(&m_core->flags_of_direct), 0);
	UML_JMPc(b, COND_NZ, of_done);

	UML_BFXU(b, uml::mem(&m_core->OF), of_scratch, sign_bit, 1);

	UML_LABEL(b, of_done);
}


void i386_device::drc_gen_eval_flags_all_shift(drcuml_block &b, int width_bits)
{
	uint32_t mask     = (width_bits >= 32) ? 0xffffffff : ((1 << width_bits) - 1);
	int      sign_bit = width_bits - 1;

	UML_BFXU(b, uml::mem(&m_core->SF), uml::mem(&m_core->flags_data_a), sign_bit, 1);
	UML_TEST(b, uml::mem(&m_core->flags_data_a), mask);
	UML_SETc(b, COND_Z, uml::mem(&m_core->ZF));
	drc_gen_set_pf(b, uml::mem(&m_core->flags_data_a));
}

void i386_device::drc_gen_eval_flags_all(drcuml_block &b, int &label_ctr, uint32_t optype)
{
	switch (optype)
	{
		case FLAGS_OPTYPE_CMP32:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, uml::mem(&m_core->flags_data_b), 32);
			break;
		case FLAGS_OPTYPE_ADD32:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, uml::mem(&m_core->flags_data_b), 32);
			break;
		case FLAGS_OPTYPE_LOGICAL32:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 0, 32);
			break;
		case FLAGS_OPTYPE_CMP8:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, uml::mem(&m_core->flags_data_b), 8);
			break;
		case FLAGS_OPTYPE_ADD8:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, uml::mem(&m_core->flags_data_b), 8);
			break;
		case FLAGS_OPTYPE_LOGICAL8:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 0, 8);
			break;
		case FLAGS_OPTYPE_CMP16:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, uml::mem(&m_core->flags_data_b), 16);
			break;
		case FLAGS_OPTYPE_ADD16:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, uml::mem(&m_core->flags_data_b), 16);
			break;
		case FLAGS_OPTYPE_LOGICAL16:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 0, 16);
			break;
		case FLAGS_OPTYPE_INC32:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, 1, 32);
			break;
		case FLAGS_OPTYPE_DEC32:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 1, 32);
			break;
		case FLAGS_OPTYPE_INC8:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, 1, 8);
			break;
		case FLAGS_OPTYPE_DEC8:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 1, 8);
			break;
		case FLAGS_OPTYPE_INC16:
			drc_gen_eval_flags_all_arith(b, label_ctr, false, 1, 16);
			break;
		case FLAGS_OPTYPE_DEC16:
			drc_gen_eval_flags_all_arith(b, label_ctr, true, 1, 16);
			break;
		case FLAGS_OPTYPE_ADC32:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, false, 32);
			break;
		case FLAGS_OPTYPE_SBB32:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, true, 32);
			break;
		case FLAGS_OPTYPE_ADC8:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, false, 8);
			break;
		case FLAGS_OPTYPE_SBB8:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, true, 8);
			break;
		case FLAGS_OPTYPE_ADC16:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, false, 16);
			break;
		case FLAGS_OPTYPE_SBB16:
			drc_gen_eval_flags_all_adcsbb(b, label_ctr, true, 16);
			break;
		case FLAGS_OPTYPE_SHIFT32:
			drc_gen_eval_flags_all_shift(b, 32);
			break;
		case FLAGS_OPTYPE_SHIFT8:
			drc_gen_eval_flags_all_shift(b, 8);
			break;
		case FLAGS_OPTYPE_SHIFT16:
			drc_gen_eval_flags_all_shift(b, 16);
			break;
	}
}

// ----------------------------------------------------------------------------
// static_generate_pack_eflags helpers
// ----------------------------------------------------------------------------

inline void i386_device::drc_gen_pack_eflags(drcuml_block &b, const uml::parameter dest)
{
	const uml::parameter &packed_result = I0;
	const uml::parameter &save_slot     = I8;

	if (dest == packed_result)
	{
		UML_CALLH(b, *m_pack_eflags);
	}
	else
	{
		UML_MOV(b, save_slot, packed_result);
		UML_CALLH(b, *m_pack_eflags);
		UML_MOV(b, dest, packed_result);
		UML_MOV(b, packed_result, save_slot);
	}
}

// ----------------------------------------------------------------------------
// CLC/CMC/STC/CLD/CLI/STD/STI
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_cmc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_XOR(b, uml::mem(&m_core->CF), uml::mem(&m_core->CF), 1);

	drc_record_cycles(ctx, CYCLES_CMC);

	return true;
}

bool i386_device::drc_pri_clc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, uml::mem(&m_core->CF), 0);

	drc_record_cycles(ctx, CYCLES_CLC);

	return true;
}

bool i386_device::drc_pri_stc(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, uml::mem(&m_core->CF), 1);

	drc_record_cycles(ctx, CYCLES_STC);

	return true;
}

bool i386_device::drc_pri_cli(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_MOV(b, uml::mem(&m_core->IF), 0);

	drc_record_cycles(ctx, CYCLES_CLI);

	return true;
}

bool i386_device::drc_pri_sti(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	drc_flush_cycles(ctx);
	UML_MOV(b, uml::mem(&m_core->delayed_interrupt_enable), 1);

	drc_record_cycles(ctx, CYCLES_STI);

	return true;
}

bool i386_device::drc_pri_cld(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, uml::mem(&m_core->DF), 0);

	drc_record_cycles(ctx, CYCLES_CLD);

	return true;
}

bool i386_device::drc_pri_std(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, uml::mem(&m_core->DF), 1);

	drc_record_cycles(ctx, CYCLES_STD);

	return true;
}

// ----------------------------------------------------------------------------
// LAHF/SAHF
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_lahf(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ah_build = I0;
	const uml::parameter &flag_bit = I1;

	drc_flush_cycles(ctx);

	drc_gen_flags_all(ctx);

	UML_MOV(b, ah_build, uml::mem(&m_core->CF));
	UML_OR(b, ah_build, ah_build, 2);
	UML_MOV(b, flag_bit, uml::mem(&m_core->PF));
	UML_SHL(b, flag_bit, flag_bit, 2);
	UML_OR(b, ah_build, ah_build, flag_bit);
	UML_MOV(b, flag_bit, uml::mem(&m_core->AF));
	UML_SHL(b, flag_bit, flag_bit, 4);
	UML_OR(b, ah_build, ah_build, flag_bit);
	UML_MOV(b, flag_bit, uml::mem(&m_core->ZF));
	UML_SHL(b, flag_bit, flag_bit, 6);
	UML_OR(b, ah_build, ah_build, flag_bit);
	UML_MOV(b, flag_bit, uml::mem(&m_core->SF));
	UML_SHL(b, flag_bit, flag_bit, 7);
	UML_OR(b, ah_build, ah_build, flag_bit);

	UML_BREG_WRITE(b, AH, ah_build);

	drc_record_cycles(ctx, CYCLES_LAHF);

	return true;
}

bool i386_device::drc_pri_sahf(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	ctx.invalidate_rscratch();

	drc_gen_flags_all(ctx);
	drc_gen_clear_flags(ctx);

	const uml::parameter &ah_value = I0;

	UML_AND(b, ah_value, DRC_REG8(AH), 0xff);
	UML_AND(b, uml::mem(&m_core->CF), ah_value, 1);
	UML_BFXU(b, uml::mem(&m_core->PF), ah_value, 2, 1);
	UML_BFXU(b, uml::mem(&m_core->AF), ah_value, 4, 1);
	UML_BFXU(b, uml::mem(&m_core->ZF), ah_value, 6, 1);
	UML_BFXU(b, uml::mem(&m_core->SF), ah_value, 7, 1);

	drc_record_cycles(ctx, CYCLES_SAHF);

	return true;
}
