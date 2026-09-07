// license:BSD-3-Clause
// copyright-holders:wtvemac

// Description here

#include "x87priv.h"

// Softfloat exception (softfloat_exceptionFlags) to x87 exception (m_core->x87_sw) table
// It's what "Update the exceptions from SoftFloat" in "x87_check_exceptions" does
static const uint8_t s_x87_sf_exc_to_sw[32] = {
	0x00, 0x20, 0x10, 0x30, 0x08, 0x28, 0x18, 0x38, 0x04, 0x24, 0x14, 0x34, 0x0c, 0x2c, 0x1c, 0x3c, 0x01, 0x21, 0x11, 0x31, 0x09, 0x29, 0x19, 0x39, 0x05, 0x25, 0x15, 0x35, 0x0d, 0x2d, 0x1d, 0x3d,
};

// ----------------------------------------------------------------------------
// Static UML_CALLC targets
// ----------------------------------------------------------------------------

static void cfunc_x87_interpreter_d8(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xd8);
}
static void cfunc_x87_interpreter_d9(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xd9);
}
static void cfunc_x87_interpreter_da(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xda);
}
static void cfunc_x87_interpreter_db(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xdb);
}
static void cfunc_x87_interpreter_dc(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xdc);
}
static void cfunc_x87_interpreter_dd(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xdd);
}
static void cfunc_x87_interpreter_de(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xde);
}
static void cfunc_x87_interpreter_df(void *p)
{
	((i386_device *)p)->drc_x87_interpreter_cb(0xdf);
}

// ----------------------------------------------------------------------------
// Dispatch logic
// ----------------------------------------------------------------------------

void i386_device::build_drc_x87_table()
{
	build_drc_x87_table_d8();
	build_drc_x87_table_d9();
	build_drc_x87_table_da();
	build_drc_x87_table_db();
	build_drc_x87_table_dc();
	build_drc_x87_table_dd();
	build_drc_x87_table_de();
	build_drc_x87_table_df();
}

void i386_device::build_drc_x87_table_d8()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm < 0xc0)
		{
			switch (MRM_OPCODE(modrm))
			{
				case 0:
					ptr = &i386_device::drc_x87_fadd_m32real;
					break;
				case 1:
					ptr = &i386_device::drc_x87_fmul_m32real;
					break;
				case 4:
					ptr = &i386_device::drc_x87_fsub_m32real;
					break;
			}
		}
		else
		{
			if (modrm >= 0xc0 && modrm <= 0xc7)
				ptr = &i386_device::drc_x87_fadd_st_sti;
			else if (modrm >= 0xc8 && modrm <= 0xcf)
				ptr = &i386_device::drc_x87_fmul_st_sti;
			else if (modrm >= 0xe0 && modrm <= 0xe7)
				ptr = &i386_device::drc_x87_fsub_st_sti;
			else if (modrm >= 0xe8 && modrm <= 0xef)
				ptr = &i386_device::drc_x87_fsubr_st_sti;
			else if (modrm >= 0xf0 && modrm <= 0xf7)
				ptr = &i386_device::drc_x87_fdiv_st_sti;
			else if (modrm >= 0xf8 && modrm <= 0xff)
				ptr = &i386_device::drc_x87_fdivr_st_sti;
		}
		m_drc_x87_table_d8[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_d9()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm < 0xc0)
		{
			switch (MRM_OPCODE(modrm))
			{
				case 0:
					ptr = &i386_device::drc_x87_fld_mem32;
					break;
				case 3:
					ptr = &i386_device::drc_x87_fstp_mem32;
					break;
			}
		}
		else
		{
			if (modrm == 0xd0)
				ptr = &i386_device::drc_gen_x87_fnop;
			else if (modrm == 0xf6)
				ptr = &i386_device::drc_gen_x87_fdecstp;
			else if (modrm == 0xf7)
				ptr = &i386_device::drc_gen_x87_fincstp;
			else if (modrm == 0xe0)
				ptr = &i386_device::drc_gen_x87_fchs;
			else if (modrm == 0xe1)
				ptr = &i386_device::drc_gen_x87_fabs;
			else if (modrm >= 0xd8 && modrm <= 0xdf)
				ptr = &i386_device::drc_gen_x87_fstp_sti;
			else if (modrm >= 0xc8 && modrm <= 0xcf)
				ptr = &i386_device::drc_gen_x87_fxch_sti;
			else if (modrm >= 0xc0 && modrm <= 0xc7)
				ptr = &i386_device::drc_gen_x87_fld_sti;
		}

		m_drc_x87_table_d9[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_da()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
		m_drc_x87_table_da[modrm] = nullptr;
}

void i386_device::build_drc_x87_table_db()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm < 0xc0)
		{
			if (MRM_OPCODE(modrm) == 0)
				ptr = &i386_device::drc_gen_x87_fild_m32int;
		}
		else
		{
			if (modrm == 0xe3)
				ptr = &i386_device::drc_gen_x87_finit;
		}

		m_drc_x87_table_db[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_dc()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm >= 0xc0 && modrm <= 0xc7)
			ptr = &i386_device::drc_x87_fadd_sti_st;
		else if (modrm >= 0xc8 && modrm <= 0xcf)
			ptr = &i386_device::drc_x87_fmul_sti_st;
		else if (modrm >= 0xe0 && modrm <= 0xe7)
			ptr = &i386_device::drc_x87_fsubr_sti_st;
		else if (modrm >= 0xe8 && modrm <= 0xef)
			ptr = &i386_device::drc_x87_fsub_sti_st;
		else if (modrm >= 0xf0 && modrm <= 0xf7)
			ptr = &i386_device::drc_x87_fdivr_sti_st;
		else if (modrm >= 0xf8 && modrm <= 0xff)
			ptr = &i386_device::drc_x87_fdiv_sti_st;

		m_drc_x87_table_dc[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_dd()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm < 0xc0)
		{
			switch (MRM_OPCODE(modrm))
			{
				case 0:
					ptr = &i386_device::drc_x87_fld_mem64;
					break;
				case 3:
					ptr = &i386_device::drc_x87_fstp_mem64;
					break;
			}
		}
		else
		{
			if (modrm >= 0xc0 && modrm <= 0xc7)
				ptr = &i386_device::drc_gen_x87_ffree;
			else if (modrm >= 0xd0 && modrm <= 0xd7)
				ptr = &i386_device::drc_gen_x87_fst_sti;
			else if (modrm >= 0xd8 && modrm <= 0xdf)
				ptr = &i386_device::drc_gen_x87_fstp_sti;
		}

		m_drc_x87_table_dd[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_de()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm >= 0xc0 && modrm <= 0xc7)
			ptr = &i386_device::drc_x87_faddp_sti_st;
		else if (modrm >= 0xc8 && modrm <= 0xcf)
			ptr = &i386_device::drc_x87_fmulp_sti_st;
		else if (modrm >= 0xe0 && modrm <= 0xe7)
			ptr = &i386_device::drc_x87_fsubrp_sti_st;
		else if (modrm >= 0xe8 && modrm <= 0xef)
			ptr = &i386_device::drc_x87_fsubp_sti_st;
		else if (modrm >= 0xf0 && modrm <= 0xf7)
			ptr = &i386_device::drc_x87_fdivrp_sti_st;
		else if (modrm >= 0xf8 && modrm <= 0xff)
			ptr = &i386_device::drc_x87_fdivp_sti_st;

		m_drc_x87_table_de[modrm] = ptr;
	}
}

void i386_device::build_drc_x87_table_df()
{
	for (int modrm = 0; modrm < 0x100; modrm++)
	{
		drc_x87_func ptr = nullptr;

		if (modrm == 0xe0)
			ptr = &i386_device::drc_gen_x87_fstsw_ax;

		m_drc_x87_table_df[modrm] = ptr;
	}
}

bool i386_device::drc_pri_x87(compiler_state &ctx)
{
	if (ctx.desc->seg_override >= 0 || ctx.desc->osz_override || ctx.desc->asz_override)
		return drc_gen_interpreter_fallback(ctx);

	uint8_t modrm = drc_get_modrm(ctx);

	drc_x87_func *table;
	switch (ctx.desc->opcode0)
	{
		case 0xd8:
			table = m_drc_x87_table_d8;
			break;
		case 0xd9:
			table = m_drc_x87_table_d9;
			break;
		case 0xda:
			table = m_drc_x87_table_da;
			break;
		case 0xdb:
			table = m_drc_x87_table_db;
			break;
		case 0xdc:
			table = m_drc_x87_table_dc;
			break;
		case 0xdd:
			table = m_drc_x87_table_dd;
			break;
		case 0xde:
			table = m_drc_x87_table_de;
			break;
		default:
			table = m_drc_x87_table_df;
			break;
	}

	const drc_x87_func handler = table[modrm];

	if (handler)
	{
		return (this->*handler)(ctx, modrm);
	}
	else
	{
		((this->*ctx.skip_ea)(ctx));

		drc_gen_x87_interpreter_fallback(ctx);

		return true;
	}
}

inline void i386_device::drc_x87_interpreter_cb(uint8_t group_op)
{
	drc_enter_interpreter();

	drc_catch_fault_inplace(
			[&]
			{
				m_operand_size     = m_core->sreg[CS].d;
				m_xmm_operand_size = 0;
				m_address_size     = m_core->sreg[CS].d;
				m_operand_prefix   = 0;
				m_address_prefix   = 0;
				m_segment_prefix   = 0;
				m_core->prev_eip   = m_core->eip;
				m_core->ext        = 1;

				m_opcode = drc_fetch8inter();
				switch (group_op)
				{
					case 0xd8:
						i386_x87_group_d8();
						break;
					case 0xd9:
						i386_x87_group_d9();
						break;
					case 0xda:
						i386_x87_group_da();
						break;
					case 0xdb:
						i386_x87_group_db();
						break;
					case 0xdc:
						i386_x87_group_dc();
						break;
					case 0xdd:
						i386_x87_group_dd();
						break;
					case 0xde:
						i386_x87_group_de();
						break;
					case 0xdf:
						i386_x87_group_df();
						break;
				}
			}
	);
	drc_leave_interpreter();
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

// i486 and up:
// Check if native x87 instructions are turned off for x87 software emulation
// A device-not-available exception exception will trigger, allowing software to pick it up
inline void i386_device::drc_gen_x87_mf_check(compiler_state &ctx, const uml::code_label fallback)
{
	if (m_features & OP_I486)
	{
		drcuml_block &b = ctx.block;

		const uml::code_label done = NEW_LBL(ctx);

		UML_TEST(b, DRC_CR(0), CR0_TS | CR0_EM);
		UML_JMPc(b, COND_NZ, fallback);

		UML_TEST(b, uml::mem(&m_core->x87_sw), X87_SW_ES);
		UML_JMPc(b, COND_Z, done);

		UML_TEST(b, DRC_CR(0), CR0_NE);
		UML_JMPc(b, COND_NZ, fallback);

		UML_LABEL(b, done);
	}
}

inline void i386_device::drc_gen_x87_cycles(compiler_state &ctx, int table_index)
{
	drcuml_block &b = ctx.block;

	const uint32_t pm = m_cycle_table_pm[table_index];
	const uint32_t rm = m_cycle_table_rm[table_index];

	drc_flush_cycles(ctx);

	if (pm == rm)
	{
		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, pm);
	}
	else
	{
		const uml::code_label use_rm = NEW_LBL(ctx);
		const uml::code_label done   = NEW_LBL(ctx);

		UML_TEST(b, DRC_CR(0), CR0_PE);
		UML_JMPc(b, COND_Z, use_rm);

		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, pm);
		UML_JMP(b, done);

		UML_LABEL(b, use_rm);
		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, rm);

		UML_LABEL(b, done);
	}
}

inline void i386_device::drc_gen_x87_set_stack_top(compiler_state &ctx, const uml::parameter new_top, const uml::parameter sw_scratch)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, sw_scratch, uml::mem(&m_core->x87_sw));
	UML_ROLINS(b, sw_scratch, new_top, X87_SW_TOP_SHIFT, X87_SW_TOP_MASK << X87_SW_TOP_SHIFT);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw_scratch);
}

inline void i386_device::drc_gen_x87_load_reg(compiler_state &ctx, const uml::parameter physx2, const uml::parameter signif, const uml::parameter signexp)
{
	drcuml_block &b = ctx.block;

	UML_DLOAD(b, signif, X87REG_PTR(0, signif), physx2, SIZE_QWORD, SCALE_x8);
	UML_LOAD(b, signexp, X87REG_PTR(0, signExp), physx2, SIZE_WORD, SCALE_x8);
}

inline void i386_device::drc_gen_x87_store_reg(compiler_state &ctx, const uml::parameter physx2, const uml::parameter signif, const uml::parameter signexp)
{
	drcuml_block &b = ctx.block;

	UML_DSTORE(b, X87REG_PTR(0, signif), physx2, signif, SIZE_QWORD, SCALE_x8);
	UML_STORE(b, X87REG_PTR(0, signExp), physx2, signexp, SIZE_WORD, SCALE_x8);
}

inline void i386_device::drc_gen_x87_indefinite(compiler_state &ctx, const uml::parameter signif, const uml::parameter signexp)
{
	drcuml_block &b = ctx.block;

	UML_DMOV(b, signif, fx80_inan.signif);
	UML_MOV(b, signexp, fx80_inan.signExp);
}

inline void i386_device::drc_gen_x87_record_operand(compiler_state &ctx, uint8_t modrm, const uml::parameter address)
{
	drcuml_block &b = ctx.block;

	const int seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)ctx.desc->opcode0 << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), address);
	UML_MOV(b, uml::mem(&m_core->x87_ds), DRC_SEG(seg, selector));
}

inline void i386_device::drc_gen_x87_set_tag(compiler_state &ctx, const uml::parameter physx2, const uml::parameter tag_value)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &tw   = I8;
	const uml::parameter &mask = I9;

	UML_MOV(b, mask, X87_TW_MASK);
	UML_SHL(b, mask, mask, physx2);
	UML_XOR(b, mask, mask, 0xffffffff);

	UML_MOV(b, tw, uml::mem(&m_core->x87_tw));
	UML_AND(b, tw, tw, mask);
	UML_MOV(b, mask, tag_value);
	UML_SHL(b, mask, mask, physx2);
	UML_OR(b, tw, tw, mask);
	UML_MOV(b, uml::mem(&m_core->x87_tw), tw);
}

inline void i386_device::drc_gen_x87_get_tag(compiler_state &ctx, const uml::parameter physx2, const uml::parameter tag_dst)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, tag_dst, uml::mem(&m_core->x87_tw));
	UML_BFXU(b, tag_dst, tag_dst, physx2, 2);
}

inline void i386_device::drc_gen_x87_classify_tag(compiler_state &ctx, const uml::parameter signif, const uml::parameter signexp, const uml::parameter tag_dst)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &exp = I8;
	const uml::parameter &sh1 = I9;

	const uml::code_label not_zero = NEW_LBL(ctx);
	const uml::code_label is_valid = NEW_LBL(ctx);
	const uml::code_label done     = NEW_LBL(ctx);

	UML_AND(b, exp, signexp, 0x7fff);
	UML_DSHL(b, sh1, signif, 1);

	UML_CMP(b, exp, 0);
	UML_JMPc(b, COND_NZ, not_zero);

	UML_DCMP(b, sh1, 0);
	UML_JMPc(b, COND_NZ, not_zero);

	UML_MOV(b, tag_dst, X87_TW_ZERO);
	UML_JMP(b, done);

	UML_LABEL(b, not_zero);

	UML_CMP(b, exp, 0x7fff);
	UML_JMPc(b, COND_NE, is_valid);

	UML_MOV(b, tag_dst, X87_TW_SPECIAL);
	UML_JMP(b, done);

	UML_LABEL(b, is_valid);
	UML_MOV(b, tag_dst, X87_TW_VALID);

	UML_LABEL(b, done);
}

inline void i386_device::drc_gen_x87_check_exceptions(compiler_state &ctx, bool store)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw_work = I8;
	const uml::parameter &scratch = I9;

	drc_flush_cycles(ctx);

	if (ctx.mode == 1)
	{
		UML_MOV(b, uml::mem(&m_core->x87_cs), ctx.eip);
	}
	else
	{
		const uml::code_label real_ip = NEW_LBL(ctx);
		const uml::code_label ip_done = NEW_LBL(ctx);

		UML_TEST(b, DRC_CR(0), CR0_PE);
		UML_JMPc(b, COND_Z, real_ip);

		UML_CMP(b, uml::mem(&m_core->VM), 0);
		UML_JMPc(b, COND_NE, real_ip);

		UML_MOV(b, uml::mem(&m_core->x87_cs), ctx.eip);
		UML_JMP(b, ip_done);

		UML_LABEL(b, real_ip);
		UML_MOV(b, uml::mem(&m_core->x87_cs), ctx.pc);

		UML_LABEL(b, ip_done);
	}

	// Update the exceptions from SoftFloat
	UML_LOAD(b, scratch, (void *)&softfloat_exceptionFlags, 0, SIZE_BYTE, SCALE_x1);
	UML_AND(b, scratch, scratch, 0x1f);
	UML_LOAD(b, scratch, (void *)s_x87_sf_exc_to_sw, scratch, SIZE_BYTE, SCALE_x1);
	UML_MOV(b, sw_work, uml::mem(&m_core->x87_sw));
	UML_OR(b, sw_work, sw_work, scratch);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw_work);
	UML_STORE(b, (void *)&softfloat_exceptionFlags, 0, 0, SIZE_BYTE, SCALE_x1);

	// sw_work = (m_core->x87_sw & ~m_core->x87_cw) & 0x3f
	UML_MOV(b, scratch, uml::mem(&m_core->x87_cw));
	UML_XOR(b, scratch, scratch, 0xffffffff);
	UML_AND(b, sw_work, sw_work, scratch);
	UML_AND(b, sw_work, sw_work, 0x3f);

	UML_MOV(b, uml::mem(&m_core->x87_check_result), 1);

	const uml::code_label no_unmasked = NEW_LBL(ctx);

	// if((m_core->x87_sw & ~m_core->x87_cw) & 0x3f)
	UML_CMP(b, sw_work, 0);
	UML_JMPc(b, COND_E, no_unmasked);

	UML_CALLC(b, store ? I386_CB(x87_drc_check_exceptions_store_cb) : I386_CB(x87_drc_check_exceptions_cb), this);

	UML_LABEL(b, no_unmasked);
}

inline void i386_device::drc_gen_x87_interpreter_fallback(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, DRC_PC, ctx.pc);
	UML_MOV(b, DRC_EIP, ctx.eip);

	drc_flush_cycles(ctx);
	switch (ctx.desc->opcode0)
	{
		case 0xd8:
			UML_CALLC(b, cfunc_x87_interpreter_d8, this);
			break;
		case 0xd9:
			UML_CALLC(b, cfunc_x87_interpreter_d9, this);
			break;
		case 0xda:
			UML_CALLC(b, cfunc_x87_interpreter_da, this);
			break;
		case 0xdb:
			UML_CALLC(b, cfunc_x87_interpreter_db, this);
			break;
		case 0xdc:
			UML_CALLC(b, cfunc_x87_interpreter_dc, this);
			break;
		case 0xdd:
			UML_CALLC(b, cfunc_x87_interpreter_dd, this);
			break;
		case 0xde:
			UML_CALLC(b, cfunc_x87_interpreter_de, this);
			break;
		case 0xdf:
			UML_CALLC(b, cfunc_x87_interpreter_df, this);
			break;
	}

	const uml::code_label no_fault = NEW_LBL(ctx);

	UML_CMP(b, DRC_PC, ctx.cursor);
	UML_JMPc(b, COND_E, no_fault);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, no_fault);
}

inline void i386_device::drc_gen_x87_epilogue(compiler_state &ctx, const uml::code_label fallback)
{
	drcuml_block &b = ctx.block;

	const uml::code_label done = NEW_LBL(ctx);

	UML_JMP(b, done);

	UML_LABEL(b, fallback);
	drc_gen_x87_interpreter_fallback(ctx);

	UML_LABEL(b, done);
}

// ----------------------------------------------------------------------------
// x87 FPU instructions
// ----------------------------------------------------------------------------

bool i386_device::drc_x87_fld_mem32(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_fld_mem(ctx, modrm, false);
}
bool i386_device::drc_x87_fld_mem64(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_fld_mem(ctx, modrm, true);
}
bool i386_device::drc_x87_fstp_mem32(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_fstp_mem(ctx, modrm, false);
}
bool i386_device::drc_x87_fstp_mem64(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_fstp_mem(ctx, modrm, true);
}
bool i386_device::drc_x87_fadd_m32real(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_m32real(ctx, modrm, I386_CB(x87_drc_add_cb), 8);
}
bool i386_device::drc_x87_fmul_m32real(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_m32real(ctx, modrm, I386_CB(x87_drc_mul_cb), 11);
}
bool i386_device::drc_x87_fsub_m32real(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_m32real(ctx, modrm, I386_CB(x87_drc_sub_cb), 8);
}
bool i386_device::drc_x87_fadd_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_add_cb), 8, false, false, false);
}
bool i386_device::drc_x87_fmul_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_mul_cb), 16, false, false, false);
}
bool i386_device::drc_x87_fsub_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, false, false, false);
}
bool i386_device::drc_x87_fsubr_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, true, false, false);
}
bool i386_device::drc_x87_fdiv_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, false, false, false);
}
bool i386_device::drc_x87_fdivr_st_sti(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, true, false, false);
}
bool i386_device::drc_x87_fadd_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_add_cb), 8, false, true, false);
}
bool i386_device::drc_x87_fmul_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_mul_cb), 16, false, true, false);
}
bool i386_device::drc_x87_fsubr_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, false, true, false);
}
bool i386_device::drc_x87_fsub_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, true, true, false);
}
bool i386_device::drc_x87_fdivr_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, false, true, false);
}
bool i386_device::drc_x87_fdiv_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, true, true, false);
}
bool i386_device::drc_x87_faddp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_add_cb), 8, false, true, true);
}
bool i386_device::drc_x87_fmulp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_mul_cb), 16, false, true, true);
}
bool i386_device::drc_x87_fsubrp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, false, true, true);
}
bool i386_device::drc_x87_fsubp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_sub_cb), 8, true, true, true);
}
bool i386_device::drc_x87_fdivrp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, false, true, true);
}
bool i386_device::drc_x87_fdivp_sti_st(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_arith_sti(ctx, modrm, I386_CB(x87_drc_div_cb), 73, true, true, true);
}

bool i386_device::drc_gen_x87_stacktop_native(compiler_state &ctx, int delta)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw  = I0;
	const uml::parameter &top = I1;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_BFXU(b, top, sw, X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, top, top, delta);
	UML_AND(b, top, top, X87_SW_TOP_MASK);
	UML_AND(b, sw, sw, ~(X87_SW_TOP_MASK << X87_SW_TOP_SHIFT));
	UML_SHL(b, top, top, X87_SW_TOP_SHIFT);
	UML_OR(b, sw, sw, top);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fincstp(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_stacktop_native(ctx, 1);
}

bool i386_device::drc_gen_x87_fdecstp(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_stacktop_native(ctx, 7);
}

bool i386_device::drc_gen_x87_fnop(compiler_state &ctx, uint8_t modrm)
{
	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_ffree(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &phys = I0;
	const uml::parameter &mask = I1;
	const uml::parameter &tw   = I2;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	UML_MOV(b, phys, uml::mem(&m_core->x87_sw));
	UML_BFXU(b, phys, phys, X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, phys, phys, MRM_RM(modrm));
	UML_AND(b, phys, phys, X87_SW_TOP_MASK);
	UML_SHL(b, phys, phys, 1);

	UML_MOV(b, mask, X87_TW_EMPTY);
	UML_SHL(b, mask, mask, phys);

	UML_MOV(b, tw, uml::mem(&m_core->x87_tw));
	UML_OR(b, tw, tw, mask);
	UML_MOV(b, uml::mem(&m_core->x87_tw), tw);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fstsw_ax(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw = I0;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_ROLINS(b, DRC_REG32(EAX), sw, 0, 0x0000ffff);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_finit(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_INTERP(x87_reset), this);
	drc_gen_x87_cycles(ctx, 17);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

void i386_device::drc_wait_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				m_core->prev_eip = m_core->eip;
				m_core->ext      = 1;
				i386_wait();
			}
	);
	drc_leave_interpreter();
}

bool i386_device::drc_pri_wait(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &tmp = I0;

	const uml::code_label fault = NEW_LBL(ctx);
	const uml::code_label done  = NEW_LBL(ctx);

	UML_AND(b, tmp, DRC_CR(0), CR0_TS | CR0_MP);

	UML_CMP(b, tmp, CR0_TS | CR0_MP);
	UML_JMPc(b, COND_E, fault);

	UML_JMP(b, done);

	UML_LABEL(b, fault);
	UML_MOV(b, DRC_PC, ctx.pc);
	UML_MOV(b, DRC_EIP, ctx.eip);
	drc_flush_cycles(ctx);
	UML_CALLC(b, I386_CB(drc_wait_cb), this);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	UML_LABEL(b, done);

	return true;
}

void i386_device::x87_drc_check_exceptions_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				m_core->x87_check_result = x87_check_exceptions(false);
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_check_exceptions_store_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				m_core->x87_check_result = x87_check_exceptions(true);
			}
	);
	drc_leave_interpreter();
}


void i386_device::x87_drc_add_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t a = m_core->x87_data_a;
				extFloat80_t b = m_core->x87_data_b;
				if (extF80_isSignalingNaN(a) || extF80_isSignalingNaN(b) || (floatx80_is_inf(a) && floatx80_is_inf(b) && ((a.signExp ^ b.signExp) & 0x8000)))
				{
					m_core->x87_sw |= X87_SW_IE;
					m_core->x87_data_result = fx80_inan;
				}
				else
				{
					m_core->x87_data_result = x87_add(a, b);
				}
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_sub_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t a = m_core->x87_data_a;
				extFloat80_t b = m_core->x87_data_b;
				if (extF80_isSignalingNaN(a) || extF80_isSignalingNaN(b) || (floatx80_is_inf(a) && floatx80_is_inf(b) && ((a.signExp ^ b.signExp) & 0x8000)))
				{
					m_core->x87_sw |= X87_SW_IE;
					m_core->x87_data_result = fx80_inan;
				}
				else
				{
					m_core->x87_data_result = x87_sub(a, b);
				}
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_mul_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t a = m_core->x87_data_a;
				extFloat80_t b = m_core->x87_data_b;
				if (extF80_isSignalingNaN(a) || extF80_isSignalingNaN(b))
				{
					m_core->x87_sw |= X87_SW_IE;
					m_core->x87_data_result = fx80_inan;
				}
				else
				{
					m_core->x87_data_result = x87_mul(a, b);
				}
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_div_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t a = m_core->x87_data_a;
				extFloat80_t b = m_core->x87_data_b;
				if (extF80_isSignalingNaN(a) || extF80_isSignalingNaN(b))
				{
					m_core->x87_sw |= X87_SW_IE;
					m_core->x87_data_result = fx80_inan;
				}
				else
				{
					m_core->x87_data_result = x87_div(a, b);
				}
			}
	);
	drc_leave_interpreter();
}


void i386_device::x87_drc_load_f32_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				float32_t    m32real{ (uint32_t)m_core->x87_data_a.signif };
				extFloat80_t value = f32_to_extF80(m32real);
				if (extF80_isSignalingNaN(value) || floatx80_is_denormal(value))
				{
					m_core->x87_sw |= X87_SW_IE;
					value = fx80_inan;
				}
				m_core->x87_data_result = value;
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_load_f64_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				float64_t    m64real{ m_core->x87_data_a.signif };
				extFloat80_t value = f64_to_extF80(m64real);
				if (extF80_isSignalingNaN(value) || floatx80_is_denormal(value))
				{
					m_core->x87_sw |= X87_SW_IE;
					value = fx80_inan;
				}
				m_core->x87_data_result = value;
			}
	);
	drc_leave_interpreter();
}


void i386_device::x87_drc_store_f32_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t value        = m_core->x87_data_a;
				float32_t    m32real      = extF80_to_f32(value);
				m_core->x87_data_a.signif = (uint64_t)m32real.v;
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_store_f64_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				extFloat80_t value        = m_core->x87_data_a;
				float64_t    m64real      = extF80_to_f64(value);
				m_core->x87_data_a.signif = m64real.v;
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_load_arith_b_f32_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				float32_t m32real{ (uint32_t)m_core->x87_data_b.signif };
				m_core->x87_data_b = f32_to_extF80(m32real);
			}
	);
	drc_leave_interpreter();
}

void i386_device::x87_drc_load_i32_cb()
{
	drc_enter_interpreter();
	drc_catch_fault_inplace(
			[&]
			{
				int32_t m32int          = (int32_t)(uint32_t)m_core->x87_data_a.signif;
				m_core->x87_data_result = i32_to_extF80(m32int);
			}
	);
	drc_leave_interpreter();
}

bool i386_device::drc_gen_x87_signop_native(compiler_state &ctx, uint8_t modrm, uint32_t sign_xor_mask, uint32_t sign_and_mask)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw      = I0;
	const uml::parameter &phys    = I1;
	const uml::parameter &physx2  = I2;
	const uml::parameter &tag     = I3;
	const uml::parameter &signif  = I4;
	const uml::parameter &signexp = I5;

	const uml::code_label fallback   = NEW_LBL(ctx);
	const uml::code_label is_empty   = NEW_LBL(ctx);
	const uml::code_label have_value = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_BFXU(b, phys, sw, X87_SW_TOP_SHIFT, 3);
	UML_SHL(b, physx2, phys, 1);

	drc_gen_x87_get_tag(ctx, physx2, tag);

	UML_CMP(b, tag, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, is_empty);

	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_load_reg(ctx, physx2, signif, signexp);
	UML_AND(b, signexp, signexp, sign_and_mask);
	UML_XOR(b, signexp, signexp, sign_xor_mask);
	UML_JMP(b, have_value);

	UML_LABEL(b, is_empty);
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_indefinite(ctx, signif, signexp);
	UML_LABEL(b, have_value);

	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)ctx.desc->opcode0 << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), 0);
	UML_MOV(b, uml::mem(&m_core->x87_ds), 0);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	drc_gen_x87_store_reg(ctx, physx2, signif, signexp);
	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 6);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fchs(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_signop_native(ctx, 0xe0, 0x8000, 0xffff);
}

bool i386_device::drc_gen_x87_fabs(compiler_state &ctx, uint8_t modrm)
{
	return drc_gen_x87_signop_native(ctx, 0xe1, 0, 0x7fff);
}

bool i386_device::drc_gen_x87_fst_sti(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw      = I0;
	const uml::parameter &phys0   = I1;
	const uml::parameter &phys0x2 = I2;
	const uml::parameter &physi   = I3;
	const uml::parameter &physix2 = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;
	const uml::parameter &tag     = I7;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	int i = MRM_RM(modrm);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_BFXU(b, phys0, sw, X87_SW_TOP_SHIFT, 3);
	UML_SHL(b, phys0x2, phys0, 1);

	UML_ADD(b, physi, phys0, i);
	UML_AND(b, physi, physi, X87_SW_TOP_MASK);
	UML_SHL(b, physix2, physi, 1);

	drc_gen_x87_get_tag(ctx, phys0x2, tag);

	const uml::code_label is_empty   = NEW_LBL(ctx);
	const uml::code_label have_value = NEW_LBL(ctx);

	UML_CMP(b, tag, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, is_empty);

	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_load_reg(ctx, phys0x2, signif, signexp);
	UML_JMP(b, have_value);

	UML_LABEL(b, is_empty);
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_indefinite(ctx, signif, signexp);
	UML_LABEL(b, have_value);

	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)0xdd << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), 0);
	UML_MOV(b, uml::mem(&m_core->x87_ds), 0);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	drc_gen_x87_store_reg(ctx, physix2, signif, signexp);
	drc_gen_x87_classify_tag(ctx, signif, signexp, tag);
	drc_gen_x87_set_tag(ctx, physix2, tag);
	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fstp_sti(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw      = I0;
	const uml::parameter &phys0   = I1;
	const uml::parameter &phys0x2 = I2;
	const uml::parameter &physi   = I3;
	const uml::parameter &physix2 = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;
	const uml::parameter &tag     = I7;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	int i = MRM_RM(modrm);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_BFXU(b, phys0, sw, X87_SW_TOP_SHIFT, 3);
	UML_SHL(b, phys0x2, phys0, 1);

	UML_ADD(b, physi, phys0, i);
	UML_AND(b, physi, physi, X87_SW_TOP_MASK);
	UML_SHL(b, physix2, physi, 1);

	drc_gen_x87_get_tag(ctx, phys0x2, tag);

	const uml::code_label is_empty   = NEW_LBL(ctx);
	const uml::code_label have_value = NEW_LBL(ctx);

	UML_CMP(b, tag, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, is_empty);

	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_load_reg(ctx, phys0x2, signif, signexp);
	UML_JMP(b, have_value);

	UML_LABEL(b, is_empty);
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_indefinite(ctx, signif, signexp);
	UML_LABEL(b, have_value);

	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)ctx.desc->opcode0 << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), 0);
	UML_MOV(b, uml::mem(&m_core->x87_ds), 0);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	drc_gen_x87_store_reg(ctx, physix2, signif, signexp);
	drc_gen_x87_classify_tag(ctx, signif, signexp, tag);
	drc_gen_x87_set_tag(ctx, physix2, tag);

	if (i == 0)
	{
		drc_gen_x87_set_tag(ctx, phys0x2, X87_TW_EMPTY);

		const uml::parameter &newtop = signif;

		UML_ADD(b, newtop, phys0, 1);
		UML_AND(b, newtop, newtop, X87_SW_TOP_MASK);
		drc_gen_x87_set_stack_top(ctx, newtop, tag);
	}
	else
	{
		const uml::parameter &tag0_now = signif;

		drc_gen_x87_get_tag(ctx, phys0x2, tag0_now);

		const uml::code_label pop_not_empty   = NEW_LBL(ctx);
		const uml::code_label pop_update_done = NEW_LBL(ctx);

		UML_CMP(b, tag0_now, X87_TW_EMPTY);
		UML_JMPc(b, COND_NE, pop_not_empty);

		UML_MOV(b, tag, uml::mem(&m_core->x87_sw));
		UML_OR(b, tag, tag, (X87_SW_IE | X87_SW_SF));
		UML_MOV(b, uml::mem(&m_core->x87_sw), tag);

		UML_MOV(b, tag0_now, uml::mem(&m_core->x87_cw));

		UML_TEST(b, tag0_now, 1);
		UML_JMPc(b, COND_Z, pop_update_done);

		UML_LABEL(b, pop_not_empty);
		drc_gen_x87_set_tag(ctx, phys0x2, X87_TW_EMPTY);

		const uml::parameter &newtop = signexp;

		UML_ADD(b, newtop, phys0, 1);
		UML_AND(b, newtop, newtop, X87_SW_TOP_MASK);
		drc_gen_x87_set_stack_top(ctx, newtop, tag);
		UML_LABEL(b, pop_update_done);
	}

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fxch_sti(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw            = I0;
	const uml::parameter &phys0         = I1;
	const uml::parameter &phys0x2       = I2;
	const uml::parameter &physi         = I3;
	const uml::parameter &physix2       = I4;
	const uml::parameter &tagchk        = I5;
	const uml::parameter &empty_signif  = I6;
	const uml::parameter &empty_signexp = I7;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	int i = MRM_RM(modrm);

	UML_BFXU(b, phys0, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);
	UML_SHL(b, phys0x2, phys0, 1);

	UML_ADD(b, physi, phys0, i);
	UML_AND(b, physi, physi, X87_SW_TOP_MASK);
	UML_SHL(b, physix2, physi, 1);

	drc_gen_x87_get_tag(ctx, phys0x2, tagchk);

	const uml::code_label st0_done = NEW_LBL(ctx);

	UML_CMP(b, tagchk, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, st0_done);

	drc_gen_x87_indefinite(ctx, empty_signif, empty_signexp);
	drc_gen_x87_store_reg(ctx, phys0x2, empty_signif, empty_signexp);
	drc_gen_x87_set_tag(ctx, phys0x2, X87_TW_SPECIAL);
	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	UML_LABEL(b, st0_done);

	drc_gen_x87_get_tag(ctx, physix2, tagchk);

	const uml::code_label sti_done = NEW_LBL(ctx);

	UML_CMP(b, tagchk, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, sti_done);

	drc_gen_x87_indefinite(ctx, empty_signif, empty_signexp);
	drc_gen_x87_store_reg(ctx, physix2, empty_signif, empty_signexp);
	drc_gen_x87_set_tag(ctx, physix2, X87_TW_SPECIAL);
	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	UML_LABEL(b, sti_done);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &signif0  = I5;
	const uml::parameter &signexp0 = I6;
	const uml::parameter &signifi  = I7;
	const uml::parameter &signexpi = I8;

	drc_gen_x87_load_reg(ctx, phys0x2, signif0, signexp0);
	drc_gen_x87_load_reg(ctx, physix2, signifi, signexpi);
	drc_gen_x87_store_reg(ctx, phys0x2, signifi, signexpi);
	drc_gen_x87_store_reg(ctx, physix2, signif0, signexp0);

	const uml::parameter &tag0 = I5;
	const uml::parameter &tagi = I6;

	drc_gen_x87_get_tag(ctx, phys0x2, tag0);
	drc_gen_x87_get_tag(ctx, physix2, tagi);
	drc_gen_x87_set_tag(ctx, phys0x2, tagi);
	drc_gen_x87_set_tag(ctx, physix2, tag0);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 4);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fld_sti(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw      = I0;
	const uml::parameter &top     = I1;
	const uml::parameter &phys7   = I2;
	const uml::parameter &tag7    = I3;
	const uml::parameter &tmp     = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;
	const uml::parameter &physsrc = I7;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	int i           = MRM_RM(modrm);
	int src_logical = (i + 1) & 7;

	UML_BFXU(b, top, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);

	UML_ADD(b, phys7, top, 7);
	UML_AND(b, phys7, phys7, X87_SW_TOP_MASK);

	UML_SHL(b, tmp, phys7, 1);
	drc_gen_x87_get_tag(ctx, tmp, tag7);

	const uml::code_label is_overflow   = NEW_LBL(ctx);
	const uml::code_label top_finalized = NEW_LBL(ctx);

	UML_CMP(b, tag7, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, is_overflow);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_AND(b, sw, sw, ~(X87_SW_TOP_MASK << X87_SW_TOP_SHIFT));
	UML_SHL(b, tmp, phys7, X87_SW_TOP_SHIFT);
	UML_OR(b, sw, sw, tmp);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	UML_MOV(b, top, phys7);

	UML_ADD(b, physsrc, top, src_logical);
	UML_AND(b, physsrc, physsrc, X87_SW_TOP_MASK);
	UML_SHL(b, physsrc, physsrc, 1);
	drc_gen_x87_load_reg(ctx, physsrc, signif, signexp);
	UML_JMP(b, top_finalized);

	UML_LABEL(b, is_overflow);
	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_OR(b, sw, sw, (X87_SW_C1 | X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_indefinite(ctx, signif, signexp);

	UML_MOV(b, tmp, uml::mem(&m_core->x87_cw));

	const uml::code_label im_masked = NEW_LBL(ctx);

	UML_TEST(b, tmp, 1);
	UML_JMPc(b, COND_NZ, im_masked);

	UML_JMP(b, top_finalized);

	UML_LABEL(b, im_masked);
	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~(X87_SW_TOP_MASK << X87_SW_TOP_SHIFT));
	UML_SHL(b, tmp, phys7, X87_SW_TOP_SHIFT);
	UML_OR(b, sw, sw, tmp);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	UML_MOV(b, top, phys7);

	UML_LABEL(b, top_finalized);
	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)0xd9 << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), 0);
	UML_MOV(b, uml::mem(&m_core->x87_ds), 0);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &dstx2 = tmp;

	UML_SHL(b, dstx2, top, 1);
	drc_gen_x87_store_reg(ctx, dstx2, signif, signexp);

	const uml::parameter &newtag = phys7;

	drc_gen_x87_classify_tag(ctx, signif, signexp, newtag);
	drc_gen_x87_set_tag(ctx, dstx2, newtag);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 4);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fld_mem(compiler_state &ctx, uint8_t modrm, bool is_64)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &top     = I1;
	const uml::parameter &tag7    = I2;
	const uml::parameter &phys7   = I3;
	const uml::parameter &tmp     = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	(this->*ctx.gen_ea)(ctx);

	drc_gen_x87_record_operand(ctx, modrm, I0);

	UML_BFXU(b, top, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, phys7, top, 7);
	UML_AND(b, phys7, phys7, X87_SW_TOP_MASK);

	UML_SHL(b, tmp, phys7, 1);
	drc_gen_x87_get_tag(ctx, tmp, tag7);

	const uml::code_label is_overflow = NEW_LBL(ctx);
	const uml::code_label write_gate  = NEW_LBL(ctx);

	UML_CMP(b, tag7, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, is_overflow);

	const uml::parameter &mem_data = I2;

	drc_flush_cycles(ctx);
	UML_CALLH(b, is_64 ? *m_mem_read64 : *m_mem_read32);
	if (is_64)
		UML_DMOV(b, DRC_X87SCR(a, signif), mem_data);
	else
		UML_DAND(b, DRC_X87SCR(a, signif), mem_data, 0xffffffffULL);
	UML_CALLC(b, is_64 ? I386_CB(x87_drc_load_f64_cb) : I386_CB(x87_drc_load_f32_cb), this);
	UML_DMOV(b, signif, DRC_X87SCR(result, signif));
	UML_MOV(b, signexp, DRC_X87SCR(result, signExp));

	const uml::parameter &sw = tmp;

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	UML_JMP(b, write_gate);

	UML_LABEL(b, is_overflow);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_OR(b, sw, sw, (X87_SW_C1 | X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	drc_gen_x87_indefinite(ctx, signif, signexp);

	UML_LABEL(b, write_gate);
	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &dstx2  = I2;
	const uml::parameter &newtag = I3;

	UML_BFXU(b, top, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, phys7, top, 7);
	UML_AND(b, phys7, phys7, X87_SW_TOP_MASK);

	drc_gen_x87_set_stack_top(ctx, phys7, sw);

	UML_SHL(b, dstx2, phys7, 1);
	drc_gen_x87_store_reg(ctx, dstx2, signif, signexp);
	drc_gen_x87_classify_tag(ctx, signif, signexp, newtag);
	drc_gen_x87_set_tag(ctx, dstx2, newtag);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 3);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fild_m32int(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &top     = I1;
	const uml::parameter &tag7    = I2;
	const uml::parameter &phys7   = I3;
	const uml::parameter &tmp     = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	(this->*ctx.gen_ea)(ctx);

	drc_gen_x87_record_operand(ctx, modrm, I0);

	UML_BFXU(b, top, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, phys7, top, 7);
	UML_AND(b, phys7, phys7, X87_SW_TOP_MASK);

	UML_SHL(b, tmp, phys7, 1);
	drc_gen_x87_get_tag(ctx, tmp, tag7);

	const uml::code_label is_overflow = NEW_LBL(ctx);
	const uml::code_label write_gate  = NEW_LBL(ctx);

	UML_CMP(b, tag7, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, is_overflow);

	const uml::parameter &mem_data = I2;

	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read32);
	UML_DAND(b, DRC_X87SCR(a, signif), mem_data, 0xffffffffULL);
	UML_CALLC(b, I386_CB(x87_drc_load_i32_cb), this);
	UML_DMOV(b, signif, DRC_X87SCR(result, signif));
	UML_MOV(b, signexp, DRC_X87SCR(result, signExp));

	const uml::parameter &sw = tmp;

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	UML_JMP(b, write_gate);

	UML_LABEL(b, is_overflow);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_OR(b, sw, sw, (X87_SW_C1 | X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	drc_gen_x87_indefinite(ctx, signif, signexp);

	UML_LABEL(b, write_gate);
	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &dstx2  = I2;
	const uml::parameter &newtag = I3;

	UML_BFXU(b, top, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);
	UML_ADD(b, phys7, top, 7);
	UML_AND(b, phys7, phys7, X87_SW_TOP_MASK);

	drc_gen_x87_set_stack_top(ctx, phys7, sw);

	UML_SHL(b, dstx2, phys7, 1);
	drc_gen_x87_store_reg(ctx, dstx2, signif, signexp);
	drc_gen_x87_classify_tag(ctx, signif, signexp, newtag);
	drc_gen_x87_set_tag(ctx, dstx2, newtag);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, 9);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_fstp_mem(compiler_state &ctx, uint8_t modrm, bool is_64)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &phys0   = I1;
	const uml::parameter &tag0    = I2;
	const uml::parameter &tmp     = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	(this->*ctx.gen_ea)(ctx);

	drc_gen_x87_record_operand(ctx, modrm, I0);

	UML_BFXU(b, phys0, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);

	UML_SHL(b, tmp, phys0, 1);
	drc_gen_x87_get_tag(ctx, tmp, tag0);

	const uml::code_label is_empty  = NEW_LBL(ctx);
	const uml::code_label converted = NEW_LBL(ctx);

	UML_CMP(b, tag0, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, is_empty);

	const uml::parameter &sw = tmp;

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	const uml::parameter &phys0x2 = tag0;

	UML_SHL(b, phys0x2, phys0, 1);
	drc_gen_x87_load_reg(ctx, phys0x2, signif, signexp);

	UML_JMP(b, converted);

	UML_LABEL(b, is_empty);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	drc_gen_x87_indefinite(ctx, signif, signexp);

	UML_LABEL(b, converted);
	UML_DMOV(b, DRC_X87SCR(a, signif), signif);
	UML_MOV(b, DRC_X87SCR(a, signExp), signexp);
	drc_flush_cycles(ctx);
	UML_CALLC(b, is_64 ? I386_CB(x87_drc_store_f64_cb) : I386_CB(x87_drc_store_f32_cb), this);

	drc_gen_x87_check_exceptions(ctx, true);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &mem_data = I2;
	if (is_64)
		UML_DMOV(b, mem_data, DRC_X87SCR(a, signif));
	else
		UML_MOV(b, mem_data, DRC_X87SCR(a, signif));
	drc_flush_cycles(ctx);
	UML_CALLH(b, is_64 ? *m_mem_write64 : *m_mem_write32);

	const uml::parameter &was_tag = I2;
	const uml::parameter &shift   = I3;

	UML_SHL(b, shift, phys0, 1);
	drc_gen_x87_get_tag(ctx, shift, was_tag);

	const uml::code_label do_pop   = NEW_LBL(ctx);
	const uml::code_label pop_done = NEW_LBL(ctx);

	UML_CMP(b, was_tag, X87_TW_EMPTY);
	UML_JMPc(b, COND_NE, do_pop);

	UML_MOV(b, sw, uml::mem(&m_core->x87_cw));

	UML_TEST(b, sw, 1);
	UML_JMPc(b, COND_Z, pop_done);

	const uml::parameter &tw   = sw;
	const uml::parameter &mask = was_tag;

	UML_LABEL(b, do_pop);
	UML_MOV(b, mask, X87_TW_EMPTY);
	UML_SHL(b, mask, mask, shift);
	UML_MOV(b, tw, uml::mem(&m_core->x87_tw));
	UML_OR(b, tw, tw, mask);
	UML_MOV(b, uml::mem(&m_core->x87_tw), tw);

	const uml::parameter &new_top = shift;

	UML_ADD(b, new_top, phys0, 1);
	UML_AND(b, new_top, new_top, X87_SW_TOP_MASK);
	drc_gen_x87_set_stack_top(ctx, new_top, sw);

	UML_LABEL(b, pop_done);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, is_64 ? 8 : 7);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_arith_m32real(compiler_state &ctx, uint8_t modrm, void (*arith_cfunc)(void *), int cycle_index)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &phys0   = I1;
	const uml::parameter &tag0    = I2;
	const uml::parameter &tmp     = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	(this->*ctx.gen_ea)(ctx);

	drc_gen_x87_record_operand(ctx, modrm, I0);

	UML_BFXU(b, phys0, uml::mem(&m_core->x87_sw), X87_SW_TOP_SHIFT, 3);

	UML_SHL(b, tmp, phys0, 1);
	drc_gen_x87_get_tag(ctx, tmp, tag0);

	const uml::code_label is_empty   = NEW_LBL(ctx);
	const uml::code_label write_gate = NEW_LBL(ctx);

	UML_CMP(b, tag0, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, is_empty);

	const uml::parameter &phys0x2 = tmp;

	UML_SHL(b, phys0x2, phys0, 1);
	drc_gen_x87_load_reg(ctx, phys0x2, signif, signexp);
	UML_DMOV(b, DRC_X87SCR(a, signif), signif);
	UML_MOV(b, DRC_X87SCR(a, signExp), signexp);

	const uml::parameter &mem_data = I2;

	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read32);
	UML_DAND(b, DRC_X87SCR(b, signif), mem_data, 0xffffffffULL);
	UML_CALLC(b, I386_CB(x87_drc_load_arith_b_f32_cb), this);
	UML_CALLC(b, arith_cfunc, this);
	UML_DMOV(b, signif, DRC_X87SCR(result, signif));
	UML_MOV(b, signexp, DRC_X87SCR(result, signExp));

	UML_JMP(b, write_gate);

	const uml::parameter &sw = tmp;

	UML_LABEL(b, is_empty);
	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	drc_gen_x87_indefinite(ctx, signif, signexp);

	UML_LABEL(b, write_gate);
	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	const uml::parameter &dstx2  = I3;
	const uml::parameter &newtag = I2;

	UML_SHL(b, dstx2, phys0, 1);
	drc_gen_x87_store_reg(ctx, dstx2, signif, signexp);
	drc_gen_x87_classify_tag(ctx, signif, signexp, newtag);
	drc_gen_x87_set_tag(ctx, dstx2, newtag);

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, cycle_index);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}

bool i386_device::drc_gen_x87_arith_sti(compiler_state &ctx, uint8_t modrm, void (*arith_cfunc)(void *), int cycle_index, bool a_is_sti, bool dest_is_i, bool do_pop)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sw      = I0;
	const uml::parameter &phys0   = I1;
	const uml::parameter &phys0x2 = I2;
	const uml::parameter &physi   = I3;
	const uml::parameter &physix2 = I4;
	const uml::parameter &signif  = I5;
	const uml::parameter &signexp = I6;
	const uml::parameter &tag0    = I7;
	const uml::parameter &tagi    = I9;
	const uml::parameter &tw      = I8;
	const uml::parameter &destx2  = dest_is_i ? physix2 : phys0x2;

	const uml::code_label fallback = NEW_LBL(ctx);

	drc_gen_x87_mf_check(ctx, fallback);

	int i = MRM_RM(modrm);

	UML_MOV(b, sw, uml::mem(&m_core->x87_sw));
	UML_BFXU(b, phys0, sw, X87_SW_TOP_SHIFT, 3);
	UML_SHL(b, phys0x2, phys0, 1);

	UML_ADD(b, physi, phys0, i);
	UML_AND(b, physi, physi, X87_SW_TOP_MASK);
	UML_SHL(b, physix2, physi, 1);

	UML_MOV(b, tw, uml::mem(&m_core->x87_tw));
	UML_BFXU(b, tag0, tw, phys0x2, 2);
	UML_BFXU(b, tagi, tw, physix2, 2);

	const uml::code_label either_empty = NEW_LBL(ctx);
	const uml::code_label values_ready = NEW_LBL(ctx);

	UML_CMP(b, tag0, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, either_empty);

	UML_CMP(b, tagi, X87_TW_EMPTY);
	UML_JMPc(b, COND_E, either_empty);

	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);

	const uml::parameter &signif0  = I5;
	const uml::parameter &signexp0 = I6;
	const uml::parameter &signifi  = I8;
	const uml::parameter &signexpi = I9;
	drc_gen_x87_load_reg(ctx, phys0x2, signif0, signexp0);
	drc_gen_x87_load_reg(ctx, physix2, signifi, signexpi);

	const uml::parameter &a_signif  = a_is_sti ? signifi : signif0;
	const uml::parameter &a_signexp = a_is_sti ? signexpi : signexp0;
	const uml::parameter &b_signif  = a_is_sti ? signif0 : signifi;
	const uml::parameter &b_signexp = a_is_sti ? signexp0 : signexpi;
	UML_DMOV(b, DRC_X87SCR(a, signif), a_signif);
	UML_MOV(b, DRC_X87SCR(a, signExp), a_signexp);
	UML_DMOV(b, DRC_X87SCR(b, signif), b_signif);
	UML_MOV(b, DRC_X87SCR(b, signExp), b_signexp);

	drc_flush_cycles(ctx);
	UML_CALLC(b, arith_cfunc, this);
	UML_DMOV(b, signif, DRC_X87SCR(result, signif));
	UML_MOV(b, signexp, DRC_X87SCR(result, signExp));
	UML_JMP(b, values_ready);

	UML_LABEL(b, either_empty);
	UML_AND(b, sw, sw, ~X87_SW_C1);
	UML_OR(b, sw, sw, (X87_SW_IE | X87_SW_SF));
	UML_MOV(b, uml::mem(&m_core->x87_sw), sw);
	drc_gen_x87_indefinite(ctx, signif, signexp);
	UML_LABEL(b, values_ready);

	UML_MOV(b, uml::mem(&m_core->x87_opcode), ((uint32_t)ctx.desc->opcode0 << 8 | modrm) & 0x7ff);
	UML_MOV(b, uml::mem(&m_core->x87_data_ptr), 0);
	UML_MOV(b, uml::mem(&m_core->x87_ds), 0);

	drc_gen_x87_check_exceptions(ctx, false);

	const uml::code_label skip_write_stack = NEW_LBL(ctx);

	UML_CMP(b, uml::mem(&m_core->x87_check_result), 0);
	UML_JMPc(b, COND_E, skip_write_stack);

	drc_gen_x87_store_reg(ctx, destx2, signif, signexp);

	const uml::parameter &newtag = tag0;

	drc_gen_x87_classify_tag(ctx, signif, signexp, newtag);
	drc_gen_x87_set_tag(ctx, destx2, newtag);

	if (do_pop)
	{
		bool commit_hit_phys0 = !dest_is_i || (i == 0);
		if (commit_hit_phys0)
		{
			const uml::parameter &newtop = signif;

			drc_gen_x87_set_tag(ctx, phys0x2, X87_TW_EMPTY);

			UML_ADD(b, newtop, phys0, 1);
			UML_AND(b, newtop, newtop, X87_SW_TOP_MASK);
			drc_gen_x87_set_stack_top(ctx, newtop, tag0);
		}
		else
		{
			const uml::parameter &tag0_now = signif;

			drc_gen_x87_get_tag(ctx, phys0x2, tag0_now);

			const uml::code_label pop_not_empty   = NEW_LBL(ctx);
			const uml::code_label pop_update_done = NEW_LBL(ctx);

			UML_CMP(b, tag0_now, X87_TW_EMPTY);
			UML_JMPc(b, COND_NE, pop_not_empty);

			UML_MOV(b, tag0, uml::mem(&m_core->x87_sw));
			UML_OR(b, tag0, tag0, (X87_SW_IE | X87_SW_SF));
			UML_MOV(b, uml::mem(&m_core->x87_sw), tag0);

			UML_MOV(b, tag0_now, uml::mem(&m_core->x87_cw));

			UML_TEST(b, tag0_now, 1);
			UML_JMPc(b, COND_Z, pop_update_done);

			UML_LABEL(b, pop_not_empty);
			drc_gen_x87_set_tag(ctx, phys0x2, X87_TW_EMPTY);

			const uml::parameter &newtop = signexp;

			UML_ADD(b, newtop, phys0, 1);
			UML_AND(b, newtop, newtop, X87_SW_TOP_MASK);
			drc_gen_x87_set_stack_top(ctx, newtop, tag0);

			UML_LABEL(b, pop_update_done);
		}
	}

	UML_LABEL(b, skip_write_stack);

	drc_gen_x87_cycles(ctx, cycle_index);

	drc_gen_x87_epilogue(ctx, fallback);

	return true;
}
