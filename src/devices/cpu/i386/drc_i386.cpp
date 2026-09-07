// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#include "drc_i386.h"

#include "emu.h"

#include "emuopts.h"

#include "drc_i386fe.h"
#include "drc_i386ops.h"
#include "i386dasm.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>


// ----------------------------------------------------------------------------
// Core DRC engine: setup, invariant construction, compile block loop
// ----------------------------------------------------------------------------

void i386_device::init_drc()
{
	m_drc_cache->allocate_cache(mconfig().options().drc_rwx());
	m_core = m_drc_cache->alloc_near<internal_i386_state>();
	if (!m_core)
		fatalerror("i386 DRC: failed to allocate state in near cache\n");

	m_drc_enabled = allow_drc();
	if (m_drc_enabled)
	{

		m_drc_uml = std::make_unique<drcuml_state>(*this, *m_drc_cache, 0, 2, 32, 0, I386DRC_COMPILE_FORWARDS_BYTES);
		m_drcfe   = std::make_unique<frontend>(this, I386DRC_COMPILE_BACKWARDS_BYTES, I386DRC_COMPILE_FORWARDS_BYTES, I386DRC_COMPILE_MAX_SEQUENCE);

		m_core->drc_cache_dirty = true;

		drc_add_symbols();

		generate_invariant();
	}
}

void i386_device::execute_run_drc()
{
	int start_cycles    = m_core->cycles;
	m_core->base_cycles = start_cycles;

	CHANGE_PC(m_core->eip);

	if (m_halted)
	{
		m_core->tsc += start_cycles;
		m_core->cycles = 0;
		return;
	}

	if (m_core->drc_cache_dirty)
		code_flush_cache();

	m_core->drc_cache_dirty = false;

	drc_leave_interpreter();

	int execute_result;

	do
	{
		if (m_core->drc_cache_dirty)
		{
			code_flush_cache();
			m_core->drc_cache_dirty = false;
		}

		if (m_core->delayed_interrupt_enable)
		{
			m_core->IF = 1;

			m_core->delayed_interrupt_enable = 0;
		}

		execute_result = m_drc_uml->execute(*m_entry);

		switch (execute_result)
		{
			case EXECUTE_MISSING_CODE:
			case EXECUTE_CACHE_FAULT:
			case EXECUTE_FAULT:
				drc_catch_fault_inplace_sync(
						[&]
						{
							code_compile_block(m_core->sreg[CS].d, m_core->pc, execute_result);
						}
				);
				break;
			case EXECUTE_UNMAPPED_CODE:
				fatalerror("DRC: unmapped code at PC=%08X\n", m_core->pc);
				break;
			case EXECUTE_RESET_CACHE:
				code_flush_cache();
				break;
			default:
				break;
		}

	} while (execute_result != EXECUTE_OUT_OF_CYCLES);

	m_core->prev_eip = m_core->eip;
	drc_enter_interpreter();

	m_core->tsc += (start_cycles - m_core->cycles);
}

void i386_device::code_flush_cache()
{
	m_drc_uml->reset();
	m_page_variants.clear();

	try
	{
		if (!(m_drc_options & I386DRC_INVARIANT_FASTRAM))
			static_generate_memory_accessors();
	}
	catch (drcuml_block::abort_compilation &)
	{
		fatalerror("Unrecoverable error generating transient static code\n");
	}
}

void i386_device::generate_invariant()
{
	try
	{
		drcuml_block &hashjmp_block(m_drc_uml->begin_invariant_block(4096));
		static_generate_nocode_handler(hashjmp_block);
		static_generate_cachefault_handler(hashjmp_block);
		static_generate_fault_handler(hashjmp_block);
		static_generate_out_of_cycles(hashjmp_block);
		static_generate_entry_point(hashjmp_block);
		hashjmp_block.end();

		// Everything below was selected to reduce DRC cache compile time based on MSNTV2 benchmarks

		// TLB resolve (essentially translate_address + i386_translate_address + vtlb_dynload)
		static_generate_resolve_tlb();

		// Prepare memory accessor handles for push32, pop32 etc...
		// this is so they're not nullptr when they're used when I386DRC_INVARIANT_FASTRAM is turned off
		allocate_memory_accessors();

		if (m_drc_options & I386DRC_INVARIANT_FASTRAM)
			static_generate_memory_accessors();

		// Flags
		static_generate_flags_eval_cc();
		static_generate_flags_eval_all();
		static_generate_pack_eflags();

		// Stack
		static_generate_push32();
		static_generate_pop32();

		// Segments
		static_generate_read_descriptor();
		static_generate_commit_cs_same_priv();
		static_generate_write_descriptor_accessed();
		static_generate_unpack_descriptor_limit_base();

		// Protected mode IRQ delivery, software ints and IRET
		// Only valid when we're skipping stack checks, goes to interpreter otherwise
		if (m_drc_options & I386DRC_SKIP_STACKCHECKS)
		{
			static_generate_irq_deliver_protected_same_priv();
			static_generate_soft_int_deliver_protected();
			static_generate_iret_protected();
			static_generate_retf_protected();
		}
	}
	catch (drcuml_block::abort_compilation &)
	{
		fatalerror("Unrecoverable error generating invariant static code\n");
	}
}

void i386_device::static_generate_entry_point(drcuml_block &b)
{
	alloc_handle(*m_drc_uml, m_entry, "entry");

	UML_HANDLE(b, *m_entry);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);
}


void i386_device::static_generate_nocode_handler(drcuml_block &b)
{
	const uml::parameter &exception_pc = I0;

	alloc_handle(*m_drc_uml, m_nocode, "nocode");

	UML_HANDLE(b, *m_nocode);

	UML_GETEXP(b, exception_pc);
	UML_MOV(b, DRC_PC, exception_pc);
	UML_EXIT(b, EXECUTE_MISSING_CODE);
}


void i386_device::static_generate_cachefault_handler(drcuml_block &b)
{
	const uml::parameter &exception_pc = I0;

	alloc_handle(*m_drc_uml, m_cachefault, "cachefault");

	UML_HANDLE(b, *m_cachefault);

	UML_GETEXP(b, exception_pc);
	UML_MOV(b, DRC_PC, exception_pc);
	UML_EXIT(b, EXECUTE_CACHE_FAULT);
}


void i386_device::static_generate_fault_handler(drcuml_block &b)
{
	const uml::parameter &exception_pc = I0;

	alloc_handle(*m_drc_uml, m_fault, "fault");

	UML_HANDLE(b, *m_fault);

	UML_GETEXP(b, exception_pc);
	UML_MOV(b, DRC_PC, exception_pc);
	UML_EXIT(b, EXECUTE_FAULT);
}


void i386_device::static_generate_out_of_cycles(drcuml_block &b)
{
	const uml::parameter &exception_pc = I0;

	alloc_handle(*m_drc_uml, m_out_of_cycles, "out_of_cycles");

	UML_HANDLE(b, *m_out_of_cycles);

	UML_GETEXP(b, exception_pc);
	UML_MOV(b, DRC_PC, exception_pc);
	UML_EXIT(b, EXECUTE_OUT_OF_CYCLES);
}

void i386_device::code_compile_block(uint8_t mode, offs_t pc, int compile_reason)
{
	auto profile = g_profiler.start(PROFILER_DRC_COMPILE);

	uint64_t compile_fault = 0;

	if (compile_reason == EXECUTE_CACHE_FAULT)
	{
		auto variant_it = m_page_variants.find(pc);
		if (variant_it != m_page_variants.end())
		{
			std::vector<page_variant_entry> &variants = variant_it->second;

			uint32_t cur_phys_page = drc_resolve_tlb_page(pc);

			for (size_t i = 0; i < variants.size(); i++)
			{
				if (variants[i].physical_page != cur_phys_page)
					continue;
				if (variants[i].checkval != drc_page_variant_checkval(pc))
					continue;

				drccodeptr reuse_entry = variants[i].entry;
				if (i != 0)
				{
					page_variant_entry hit = variants[i];
					variants.erase(variants.begin() + i);
					variants.insert(variants.begin(), hit);
				}

				if (m_drc_uml->hash_set_codeptr(mode, pc, reuse_entry))
					return;

				break;
			}
		}
	}

	const opcode_desc *desclist = m_drcfe->describe_code(pc, mode);

	if (m_drc_uml->logging() || m_drc_uml->logging_native())
		drc_log_opcode_desc(desclist, 0);

	bool compile_next_seq = (compile_reason == EXECUTE_CACHE_FAULT);

	bool succeeded = false;
	while (!succeeded)
	{
		try
		{
			drcuml_block &b         = m_drc_uml->begin_block(65536);
			int           label_ctr = 0x10000000;

			try
			{
				for (opcode_desc const *seqhead = desclist, *seqlast = nullptr; seqhead != nullptr; seqhead = seqlast->next())
				{
					if (m_drc_uml->logging())
						b.append_comment("-------------------------");

					for (seqlast = seqhead; seqlast != nullptr; seqlast = seqlast->next())
						if (seqlast->end_sequence())
							break;
					assert(seqlast != nullptr);

					bool hashnotexists = !m_drc_uml->hash_exists(mode, seqhead->pc);
					if (compile_next_seq || hashnotexists)
					{
						UML_HASH(b, mode, seqhead->pc);
					}
					else
					{

						UML_LABEL(b, seqhead->pc);
						UML_HASHJMP(b, DRC_SEG(CS, d), seqhead->pc, *m_nocode);

						continue;
					}

					offs_t         pc  = seqhead->pc;
					offs_t         eip = (seqhead->pc - m_core->sreg[CS].base);
					compiler_state ctx = drc_create_compiler_state(b, label_ctr, pc, eip, mode);

					if (seqhead->is_branch_target())
						UML_LABEL(b, seqhead->pc);

					UML_MOV(b, DRC_PC, seqhead->pc);
					UML_SUB(b, DRC_EIP, seqhead->pc, DRC_SEG(CS, base));

					if (PAGE_MODE_ENABLED)
					{
						if ((m_drc_options & I386DRC_SMC_CHECK_PVARI) && mode != 0)
							drc_gen_page_check(ctx);
						if (m_drc_options & I386DRC_SMC_CHECK_PCONT)
							drc_gen_content_check(ctx);
					}
					else
					{
						if ((m_drc_options & I386DRC_SMC_CHECK_NCONT) && m_program->get_write_ptr(seqhead->pc) != nullptr)
							drc_gen_content_check(ctx);
					}

					// Check interrupts
					drc_flush_cycles(ctx);
					drc_gen_irq_poll(ctx);

					UML_CMP(b, DRC_CYCLES, 0);
					UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

					// The block ended most likely due to an interpreter fallback
					bool block_ended = false;

					uint32_t page_offset = seqhead->pc >> 12;

					for (opcode_desc const *curdesc = seqhead; curdesc != seqlast->next(); curdesc = curdesc->next())
					{
						if (generate_sequence_instruction(ctx, page_offset, curdesc))
						{
							block_ended = true;
							break;
						}
					}

					if (!block_ended)
					{
						const offs_t next_pc = seqlast->pc + seqlast->length;

						UML_MOV(b, DRC_PC, next_pc);
						UML_SUB(b, DRC_EIP, next_pc, DRC_SEG(CS, base));

						drc_flush_cycles(ctx);

						UML_CMP(b, DRC_CYCLES, 0);
						UML_EXHc(b, COND_LE, *m_out_of_cycles, next_pc);

						UML_HASHJMP(b, DRC_SEG(CS, d), next_pc, *m_nocode);
					}
				}

				b.end();
				succeeded = true;
			}
			catch (uint64_t fault)
			{
				compile_fault = fault;
				b.abort();
			}
		}
		catch (drcuml_block::abort_compilation &)
		{
			code_flush_cache();
			if (compile_fault)
			{
				uint64_t f    = compile_fault;
				compile_fault = 0;
				throw f;
			}
		}
	}

	if (compile_next_seq)
	{
		drccodeptr compiled_entry = m_drc_uml->hash_get_codeptr(mode, pc);
		if (compiled_entry)
		{
			std::vector<page_variant_entry> &variants = m_page_variants[pc];

			uint32_t checkval = drc_page_variant_checkval(pc);

			uint32_t cur_phys_page = drc_resolve_tlb_page(pc);

			auto existing = std::find_if(
					variants.begin(),
					variants.end(),
					[cur_phys_page](const page_variant_entry &e)
					{
						return e.physical_page == cur_phys_page;
					}
			);

			if (existing != variants.end())
			{
				existing->checkval = checkval;
				existing->entry    = compiled_entry;
			}
			else
			{
				if (variants.size() >= I386DRC_MAX_PAGE_VARIANTS)
					variants.pop_back();

				variants.insert(
						variants.begin(),
						page_variant_entry{
								cur_phys_page,
								checkval,
								compiled_entry,
						}
				);
			}
		}
	}
}

bool i386_device::generate_sequence_instruction(compiler_state &ctx, uint32_t &page_offset, opcode_desc const *desc)
{
	drcuml_block &b = ctx.block;

	ctx.cursor = desc->pc;

	const offs_t insn_eip = ctx.cursor - m_core->sreg[CS].base;

	ctx.pc          = ctx.cursor;
	ctx.eip         = insn_eip;
	ctx.block_ended = false;

	if (m_drc_uml->logging() && !desc->virtual_noop())
		drc_log_add_disasm_comment(b, desc);

	UML_MOV(b, DRC_PC, ctx.cursor);
	UML_SUB(b, DRC_EIP, ctx.cursor, DRC_SEG(CS, base));

	if (DEBUG_INTERP_FALLBACK)
		UML_MOV(b, uml::mem(&m_core->drc_ifallback_start_pc), ctx.cursor);


	if (PAGE_MODE_ENABLED)
	{
		const bool do_page_check    = (m_drc_options & I386DRC_SMC_CHECK_PVARI) && ctx.mode != 0;
		const bool do_content_check = (m_drc_options & I386DRC_SMC_CHECK_PCONT);
		if (do_page_check || do_content_check)
		{
			const uint32_t this_page = ctx.cursor >> 12;
			if (this_page != page_offset)
			{
				drc_flush_cycles(ctx);
				if (do_page_check)
					drc_gen_page_check(ctx);
				if (do_content_check)
					drc_gen_content_check(ctx);
				page_offset = this_page;
			}
		}
	}
	else
	{
		if (m_drc_options & I386DRC_SMC_CHECK_NCONT)
		{
			const uint32_t this_page = ctx.cursor >> 12;
			if (this_page != page_offset)
			{
				if (m_program->get_write_ptr(ctx.cursor) != nullptr)
				{
					drc_flush_cycles(ctx);
					drc_gen_content_check(ctx);
				}
				page_offset = this_page;
			}
		}
	}

	if (debugger_enabled())
		UML_DEBUG(b, ctx.cursor);


	if (drc_dispatch_one(ctx, desc))
	{
		return false;
	}
	else
	{
		if (!ctx.block_ended)
			drc_gen_interpreter_fallback(ctx);

		return true;
	}
}

void i386_device::build_drc_opcode_table(uint32_t features)
{
	for (int i = 0; i < 256; i++)
	{
		m_drc_sel_pri_table[i] = drc_dispatch_t{};
		m_drc_sel_x0f_table[i] = drc_dispatch_t{};
	}

	for (const auto &e : s_drc_opcode_table)
	{

		if (e.flags & features)
		{
			if (e.flags & OP_2BYTE)
				m_drc_sel_x0f_table[e.opcode] = drc_dispatch_t{ .handler16 = e.handler16, .handler32 = e.handler32, .drc_flags = e.drc_flags };
			else
				m_drc_sel_pri_table[e.opcode] = drc_dispatch_t{ .handler16 = e.handler16, .handler32 = e.handler32, .drc_flags = e.drc_flags };
		}
	}
}

bool i386_device::drc_dispatch_one(compiler_state &ctx, opcode_desc const *desc)
{
	drcuml_block &b = ctx.block;


	if (DEBUG_INTERP_FALLBACK)
		UML_CALLC(b, I386_CB(func_log_instruction_exec), this);

	ctx.desc = desc;

	const offs_t base_pc = ctx.cursor;

	ctx.gen_ea  = ctx.desc->addr32 ? &i386_device::drc_gen_ea32 : &i386_device::drc_gen_ea16;
	ctx.skip_ea = ctx.desc->addr32 ? &i386_device::drc_skip_ea32 : &i386_device::drc_skip_ea16;

	ctx.cursor = base_pc + desc->pfx_len + ctx.desc->opcode_len;

	drc_dispatch_t disp_desc;
	switch (ctx.desc->dispatch_type)
	{
		case DISPATCH_X0F_TABLE:
			disp_desc = m_drc_sel_x0f_table[desc->opcode1];
			break;
		case DISPATCH_PRI_TABLE:
		default:
			disp_desc = m_drc_sel_pri_table[desc->opcode0];
			break;
	}

	const drc_op_func handler = ctx.desc->op32 ? disp_desc.handler32 : disp_desc.handler16;
	const uint32_t    flags   = disp_desc.drc_flags;

	if (!handler || (desc->osz_override && !(flags & DRC_CAN_OSZ_OV)))
		return drc_gen_interpreter_fallback(ctx);

	if (!(flags & DRC_SELF_MANAGED))
	{
		if (!(flags & DRC_NO_RSCR))
			ctx.invalidate_rscratch();

		if (flags & DRC_NO_LFLAGS)
		{
			UML_MOV(b, uml::mem(&m_core->flags_optype), FLAGS_OPTYPE_UNKNOWN);
			ctx.compile_time_flags_ready = false;
		}
	}

	// Can't do mode 0 (16-bit mode)
	if (ctx.mode == 0 && !(flags & DRC_MODE0_RDY))
	{
		return false;
	}
	// Can do mode 0 (16-bit mode) or in mode 1 (32-bit mode)
	else
	{
		bool ok = (this->*handler)(ctx);

		if (ok)
			ctx.cursor = desc->pc + desc->length;

		return ok;
	}
}

void i386_device::drc_interpreter_step_one()
{
	if (DEBUG_INTERP_FALLBACK)
		func_log_fallback_exec(m_core->drc_ifallback_start_pc);

	drc_enter_interpreter();

	m_operand_size     = m_core->sreg[CS].d;
	m_xmm_operand_size = 0;
	m_address_size     = m_core->sreg[CS].d;
	m_operand_prefix   = 0;
	m_address_prefix   = 0;
	m_segment_prefix   = 0;
	m_core->prev_eip   = m_core->eip;
	m_core->ext        = 1;

	if (m_core->delayed_interrupt_enable)
	{
		m_core->IF = 1;

		m_core->delayed_interrupt_enable = 0;
	}
	uint8_t old_tf = m_core->TF;

	drc_catch_fault_inplace(
			[&]
			{
				i386_check_irq_line();
				i386_decode_opcode();
				if (m_core->TF && old_tf)
				{
					m_core->prev_eip = m_core->eip;
					m_core->ext      = 1;
					m_core->dr[6] |= (1 << 14);
					i386_trap(1, 0);
				}
				if (m_lock && m_opcode != 0xf0)
					m_lock = false;
			}
	);


	if (m_core->RF && m_auto_clear_RF)
		m_core->RF = 0;
	if (!m_auto_clear_RF)
		m_auto_clear_RF = true;

	drc_leave_interpreter();

	if (m_core->drc_cache_dirty)
		m_core->cycles = 0;
}

// ----------------------------------------------------------------------------
// DRC Configuration
// ----------------------------------------------------------------------------

void i386_device::drc_set_options(uint32_t options)
{
	if (!allow_drc())
		return;

	m_drc_options = options;
}

uint32_t i386_device::drc_get_options()
{
	return m_drc_options;
}

inline bool i386_device::fastram_excluded_compare(const offs_t *a, int acount, const offs_t *b, int bcount)
{
	if (acount != bcount)
		// The amount of entries is different
		return false;

	for (int i = 0; i < acount; i++)
		if (a[i] != b[i])
			// An entry is different
			return false;

	// Everything matches
	return true;
}

void i386_device::i386drc_add_fastram(offs_t start, offs_t end, bool readonly, void *base, uint32_t access_size, const offs_t *excluded, uint32_t excluded_count, bool reg32_write_only, bool virtual_base)
{
	if (!allow_drc())
		return;

	if ((m_core != nullptr && m_core->drc_cached_invariant) && (m_drc_options & I386DRC_INVARIANT_FASTRAM))
		fatalerror("Invariant fastram accessor cannot be modified after start\n");

	excluded_count = std::min(excluded_count, FASTRAM_MAX_EXCLUDED);

	// Find if this fastram pointer and start address already exists
	fastram_entry *target = nullptr;
	for (int i = 0; i < m_fastram_select; i++)
	{
		if (m_fastram[i].base == base && m_fastram[i].start == start)
		{
			target = &m_fastram[i];
			break;
		}
	}

	// An existing entry was found, so check if there's anything changed with it.
	if (target != nullptr)
	{
		bool unchanged = (target->end == end);
		unchanged      = (target->readonly == readonly) && unchanged;
		unchanged      = (target->access_size == access_size) && unchanged;
		unchanged      = (target->reg32_write_only == reg32_write_only) && unchanged;
		unchanged      = (target->virtual_base == virtual_base) && unchanged;
		unchanged      = (target->excluded_count == excluded_count) && unchanged;
		unchanged      = fastram_excluded_compare(target->excluded, target->excluded_count, excluded, excluded_count) && unchanged;

		// If nothing changed then exit, otherwise proceede to use the target pointer to update its properties
		if (unchanged)
			return;
	}
	// If nothing existing was found then create a new entry.
	else
	{
		if (m_fastram_select >= I386_MAX_FASTRAM)
			return;

		target        = &m_fastram[m_fastram_select++];
		target->base  = base;
		target->start = start;
	}

	target->end              = end;
	target->readonly         = readonly;
	target->access_size      = access_size;
	target->reg32_write_only = reg32_write_only;
	target->virtual_base     = virtual_base;
	target->excluded_count   = excluded_count;

	std::copy_n(excluded, excluded_count, target->excluded);

	if (m_core != nullptr)
		m_core->drc_cache_dirty = true;
}


void i386_device::clear_fastram(uint32_t select_start)
{
	if (!allow_drc())
		return;

	if ((m_core != nullptr && m_core->drc_cached_invariant) && m_drc_options & I386DRC_INVARIANT_FASTRAM)
		fatalerror("Invariant fastram accessor cannot be modified after start\n");

	m_fastram_select = select_start;

	if (m_core != nullptr)
		m_core->drc_cache_dirty = true;
}


void i386_device::i386drc_add_fastpaged(offs_t start, offs_t end, uint32_t *page_table, uint32_t page_table_mask, uint32_t page_index_base, uint32_t page_shift, uint32_t page_offset_mask, uint32_t page_valid_bit, uint32_t *ram_base, uint32_t ram_limit, uint32_t access_size)
{
	if (!allow_drc())
		return;

	if ((m_core != nullptr && m_core->drc_cached_invariant) && (m_drc_options & I386DRC_INVARIANT_FASTRAM))
		fatalerror("Invariant fastpaged accessor cannot be modified after start\n");

	// Find if this fastpaged table pointer and start address already exists
	fastpaged_entry *target = nullptr;
	for (int i = 0; i < m_fastpaged_select; i++)
	{
		if (m_fastpaged[i].page_table == page_table && m_fastpaged[i].start == start)
		{
			target = &m_fastpaged[i];
			break;
		}
	}

	// An existing entry was found, so check if there's anything changed with it.
	if (target != nullptr)
	{
		bool unchanged = (target->end == end);
		unchanged      = (target->page_table_mask == page_table_mask) && unchanged;
		unchanged      = (target->page_index_base == page_index_base) && unchanged;
		unchanged      = (target->page_shift == page_shift) && unchanged;
		unchanged      = (target->page_offset_mask == page_offset_mask) && unchanged;
		unchanged      = (target->page_valid_bit == page_valid_bit) && unchanged;
		unchanged      = (target->ram_base == ram_base) && unchanged;
		unchanged      = (target->ram_limit == ram_limit) && unchanged;
		unchanged      = (target->access_size == access_size) && unchanged;

		// If nothing changed then exit, otherwise proceede to use the target pointer to update its properties
		if (unchanged)
			return;
	}
	else
	{
		// If nothing existing was found then create a new entry.
		if (m_fastpaged_select >= I386_MAX_FASTPAGED)
			return;

		target             = &m_fastpaged[m_fastpaged_select++];
		target->page_table = page_table;
		target->start      = start;
	}

	target->end              = end;
	target->page_table_mask  = page_table_mask;
	target->page_index_base  = page_index_base;
	target->page_shift       = page_shift;
	target->page_offset_mask = page_offset_mask;
	target->page_valid_bit   = page_valid_bit;
	target->ram_base         = ram_base;
	target->ram_limit        = ram_limit;
	target->access_size      = access_size;

	if (m_core != nullptr)
		m_core->drc_cache_dirty = true;
}


void i386_device::clear_fastpaged(uint32_t select_start)
{
	if (!allow_drc())
		return;

	if ((m_core != nullptr && m_core->drc_cached_invariant) && m_drc_options & I386DRC_INVARIANT_FASTRAM)
		fatalerror("Invariant fastpaged accessor cannot be modified after start\n");

	m_fastpaged_select = select_start;

	if (m_core != nullptr)
		m_core->drc_cache_dirty = true;
}

// ----------------------------------------------------------------------------
// Self-modified code checking
// ----------------------------------------------------------------------------

inline uint32_t i386_device::drc_page_variant_checkval(offs_t pc)
{
	return drc_peek32(pc);
}

void i386_device::drc_gen_page_check(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &runtime_tlb_entry = I0;
	const uml::parameter &resolved_page     = I0;

	uint32_t vtlb_idx = ctx.cursor >> 12;

	uint32_t compile_time_page  = drc_resolve_tlb_page(ctx.cursor);
	uint32_t compile_time_entry = compile_time_page | FLAG_VALID;

	const uml::code_label mapping_ok = NEW_LBL(ctx);

	if (m_drc_uml->logging())
		b.append_comment("[Page Validation for %08X => %08X]", ctx.cursor, compile_time_page);

	UML_LOAD(b, runtime_tlb_entry, vtlb_table(), vtlb_idx, SIZE_DWORD, SCALE_x4);
	UML_AND(b, runtime_tlb_entry, runtime_tlb_entry, 0xfffff008);

	UML_CMP(b, runtime_tlb_entry, compile_time_entry);
	UML_JMPc(b, COND_E, mapping_ok);

	UML_MOV(b, uml::mem(&m_core->mem_laddr), ctx.cursor);
	UML_MOV(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_MOV(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_CALLH(b, *m_resolve_tlb);

	UML_CMP(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_EXHc(b, COND_NE, *m_cachefault, ctx.cursor);

	UML_MOV(b, resolved_page, uml::mem(&m_core->mem_paddr));
	UML_AND(b, resolved_page, resolved_page, 0xfffff000);

	UML_CMP(b, resolved_page, compile_time_page);
	UML_EXHc(b, COND_NE, *m_cachefault, ctx.cursor);

	UML_LABEL(b, mapping_ok);
}

inline uint32_t i386_device::drc_content_checkval(offs_t pc)
{
	return drc_peek32(pc);
}

void i386_device::drc_gen_content_check(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::code_label content_ok = NEW_LBL(ctx);

	const uint32_t compile_time_data = drc_content_checkval(ctx.cursor);

	if (m_drc_uml->logging())
		b.append_comment("[Content Validation for %08X]", ctx.cursor);

	if (!PAGE_MODE_ENABLED && !debugger_enabled())
	{
		for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
		{
			fastram_entry &fi = m_fastram[ramnum];
			if (fi.readonly)
				continue;
			if (ctx.cursor < fi.start || (ctx.cursor + 3) > fi.end)
				continue;

			const uml::parameter &runtime_data = I0;

			void *fastbase = (uint8_t *)fi.base - fi.start;

			UML_LOAD(b, runtime_data, fastbase, ctx.cursor, SIZE_DWORD, SCALE_x1);

			UML_CMP(b, runtime_data, compile_time_data);
			UML_EXHc(b, COND_NE, *m_cachefault, ctx.cursor);

			UML_LABEL(b, content_ok);

			return;
		}
	}

	const uml::parameter &runtime_data = I0;
	const uml::parameter &hi_word      = I5;
	const uml::parameter &scratch      = I2;
	const uml::code_label take_slow    = NEW_LBL(ctx);

	if (DWORD_ALIGNED(ctx.pc))
	{
		drc_gen_privileged_read32(b, ctx.label_ctr, runtime_data, ctx.pc, scratch, take_slow);

		UML_CMP(b, runtime_data, compile_time_data);
		UML_JMPc(b, COND_E, content_ok);
	}
	else
	{
		const uint32_t lo_bytes = 4 - (ctx.pc & 3);
		const uint32_t hi_bytes = 4 - lo_bytes;
		const uint32_t lo_shift = (ctx.pc & 3) * 8;
		const uint32_t splice   = lo_bytes * 8;

		const offs_t aligned_lo = ctx.pc & ~offs_t(3);
		const offs_t aligned_hi = aligned_lo + 4;

		drc_gen_privileged_read32(b, ctx.label_ctr, runtime_data, aligned_lo, scratch, take_slow);
		UML_SHR(b, runtime_data, runtime_data, lo_shift);
		UML_AND(b, runtime_data, runtime_data, (uint32_t)((1ULL << splice) - 1));

		drc_gen_privileged_read32(b, ctx.label_ctr, hi_word, aligned_hi, scratch, take_slow);
		UML_AND(b, hi_word, hi_word, (uint32_t)((1ULL << (hi_bytes * 8)) - 1));
		UML_SHL(b, hi_word, hi_word, splice);
		UML_OR(b, runtime_data, runtime_data, hi_word);

		UML_CMP(b, runtime_data, compile_time_data);
		UML_JMPc(b, COND_E, content_ok);
	}

	UML_LABEL(b, take_slow);
	UML_EXH(b, *m_cachefault, ctx.pc);

	UML_LABEL(b, content_ok);
}

// ----------------------------------------------------------------------------
// Common control transfer and state management helpers
// ----------------------------------------------------------------------------

template <void (i386_device::*Fn)()> void i386_device::cfunc_run_interp(void *p)
{
	i386_device &cpu = *static_cast<i386_device *>(p);

	if (DEBUG_INTERP_FALLBACK)
		cpu.func_log_fallback_exec(cpu.m_core->drc_ifallback_start_pc);

	cpu.drc_enter_interpreter();
	cpu.drc_catch_fault_inplace(
			[&cpu]
			{
				(cpu.*Fn)();
			}
	);
	cpu.drc_leave_interpreter();
}

template <void (i386_device::*Fn)()> void i386_device::cfunc_callback(void *p)
{
	(static_cast<i386_device *>(p)->*Fn)();
}

template <typename Body> inline void i386_device::drc_catch_fault_inplace(Body &&body)
{
	try
	{
		body();
	}
	catch (uint64_t fault)
	{
		m_core->ext = 1;
		i386_trap_with_error(fault & 0xffffffff, 0, 0, fault >> 32);
	}
}

template <typename Body> inline void i386_device::drc_catch_fault_inplace_sync(Body &&body)
{
	try
	{
		body();
	}
	catch (uint64_t fault)
	{
		m_core->prev_eip = m_core->eip;
		drc_enter_interpreter();

		m_core->ext = 1;
		i386_trap_with_error(fault & 0xffffffff, 0, 0, fault >> 32);

		drc_leave_interpreter();
	}
}

inline i386_device::compiler_state i386_device::drc_create_compiler_state(drcuml_block &b, int &label_ctr, offs_t pc, offs_t eip, uint8_t mode)
{
	return {
		.block       = b,
		.label_ctr   = label_ctr,
		.block_ended = false,

		.cpu    = this,
		.pc     = pc,
		.eip    = eip,
		.cursor = pc,
		.mode   = mode,

		.total_cycles   = 0,
		.pending_cycles = 0,

		.gen_ea  = nullptr,
		.skip_ea = nullptr,

		.desc = nullptr,

		.compile_time_flags_ready  = false,
		.compile_time_flags_optype = FLAGS_OPTYPE_UNKNOWN,

		.rscratch_valid = {},
		.rscratch_value = {},
	};
}

inline bool i386_device::drc_gen_control_transfer_cb(compiler_state &ctx, offs_t &cursor, void (*cfunc)(void *), void *param, bool end_block)
{
	drcuml_block &b = ctx.block;

	UML_MOV(b, DRC_PC, (cursor == CTRANSFER_USE_PC) ? ctx.pc : cursor);
	UML_SUB(b, DRC_EIP, (cursor == CTRANSFER_USE_PC) ? ctx.pc : cursor, DRC_SEG(CS, base));

	UML_SUB(b, DRC_PREV_EIP, ctx.pc, DRC_SEG(CS, base));

	drc_flush_cycles(ctx);
	UML_CALLC(b, cfunc, param);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_nocode);

	if (end_block)
		ctx.block_ended = true;

	return !end_block;
}

inline bool i386_device::drc_gen_interpreter_fallback(compiler_state &ctx)
{
	offs_t cursor = CTRANSFER_USE_PC;

	ctx.compile_time_flags_ready = false;

	return drc_gen_control_transfer_cb(ctx, cursor, I386_CB(drc_interpreter_step_one), this);
}

inline void i386_device::drc_gen_fault_inplace_check(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::code_label no_fault = NEW_LBL(ctx);

	UML_CMP(b, DRC_PC, ctx.pc);
	UML_JMPc(b, COND_E, no_fault);

	drc_flush_cycles(ctx);

	UML_CMP(b, DRC_CYCLES, 0);
	UML_EXHc(b, COND_LE, *m_out_of_cycles, DRC_PC);

	UML_HASHJMP(b, DRC_SEG(CS, d), DRC_PC, *m_fault);

	UML_LABEL(b, no_fault);
}

inline void i386_device::drc_enter_interpreter()
{
	if (m_core->flags_optype != FLAGS_OPTYPE_UNKNOWN)
		drc_flags_all();

	m_core->flags_optype    = FLAGS_OPTYPE_UNKNOWN;
	m_core->flags_of_direct = 0;
}


inline void i386_device::drc_leave_interpreter()
{
	m_core->flags_optype    = FLAGS_OPTYPE_UNKNOWN;
	m_core->flags_of_direct = 0;
}


inline void i386_device::drc_record_cycles(compiler_state &ctx, int cycles_id)
{
	int n = m_cycle_table_rm[cycles_id];
	ctx.total_cycles += n;
	ctx.pending_cycles += n;
}


inline void i386_device::drc_flush_cycles(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	if (ctx.pending_cycles != 0)
	{
		UML_SUB(b, DRC_CYCLES, DRC_CYCLES, ctx.pending_cycles);
		ctx.pending_cycles = 0;
	}
}

// ----------------------------------------------------------------------------
// Common instruction decode helpers
// ----------------------------------------------------------------------------

inline uint8_t i386_device::drc_get_modrm(compiler_state &ctx)
{
	ctx.cursor++;
	return ctx.desc->modrm;
}


inline uint8_t i386_device::drc_get_imm8(compiler_state &ctx)
{
	ctx.cursor++;
	return (uint8_t)ctx.desc->imm;
}


inline uint16_t i386_device::drc_get_imm16(compiler_state &ctx)
{
	ctx.cursor += 2;
	return (uint16_t)ctx.desc->imm;
}


inline uint32_t i386_device::drc_get_imm32(compiler_state &ctx)
{
	ctx.cursor += 4;
	return ctx.desc->imm;
}

void i386_device::drc_gen_ea16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;

	auto get_seg = [&](uint8_t default_seg)
	{
		return (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : default_seg;
	};

	if (ctx.desc->modrm_rm == 6 && ctx.desc->modrm_mode == 0)
	{
		UML_MOV(b, ea_result, (uint32_t)(uint16_t)ctx.desc->disp);
		UML_ADD(b, ea_result, ea_result, DRC_SEG(get_seg(DS), base));

		ctx.cursor += ctx.desc->disp_bytes;
	}
	else
	{
		uint8_t seg = get_seg(s_rm_sreg16[ctx.desc->modrm_rm & 7]);

		switch (ctx.desc->modrm_rm)
		{
			case 0:
				UML_ADD(b, ea_result, DRC_REG32(EBX), DRC_REG32(ESI));
				break;
			case 1:
				UML_ADD(b, ea_result, DRC_REG32(EBX), DRC_REG32(EDI));
				break;
			case 2:
				UML_ADD(b, ea_result, DRC_REG32(EBP), DRC_REG32(ESI));
				break;
			case 3:
				UML_ADD(b, ea_result, DRC_REG32(EBP), DRC_REG32(EDI));
				break;
			case 4:
				UML_MOV(b, ea_result, DRC_REG32(ESI));
				break;
			case 5:
				UML_MOV(b, ea_result, DRC_REG32(EDI));
				break;
			case 6:
				UML_MOV(b, ea_result, DRC_REG32(EBP));
				break;
			case 7:
				UML_MOV(b, ea_result, DRC_REG32(EBX));
				break;
		}

		if (ctx.desc->disp && (ctx.desc->modrm_mode == 1 || ctx.desc->modrm_mode == 2))
			UML_ADD(b, ea_result, ea_result, ctx.desc->disp);

		UML_AND(b, ea_result, ea_result, 0xffff);
		UML_ADD(b, ea_result, ea_result, DRC_SEG(seg, base));

		ctx.cursor += ctx.desc->disp_bytes;
	}
}

void i386_device::drc_gen_ea32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result       = I0;
	const uml::parameter &ea_index_scaled = I1;

	auto get_seg = [&](uint8_t default_seg)
	{
		return (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : default_seg;
	};

	if (ctx.desc->modrm_rm == 5 && ctx.desc->modrm_mode == 0)
	{
		UML_MOV(b, ea_result, (uint32_t)ctx.desc->disp);
		UML_ADD(b, ea_result, ea_result, DRC_SEG(get_seg(SS), base));

		ctx.cursor += ctx.desc->disp_bytes;
	}
	else
	{
		uint8_t seg;

		if (MRM_HAS_SIB(ctx.desc->modrm))
		{
			const uint8_t sib   = ctx.desc->sib;
			const uint8_t scale = SIB_SCALE(sib);
			const uint8_t idx   = SIB_INDEX(sib);
			const uint8_t base  = SIB_BASE(sib);

			seg = get_seg(s_rm_sreg32[base]);

			if (ctx.desc->modrm_mode == 0 && base == 5)
				UML_MOV(b, ea_result, (uint32_t)ctx.desc->disp);
			else
				UML_MOV(b, ea_result, DRC_GET_SIB_REG32(base));

			if (idx != 4)
			{
				if (scale == 0)
				{
					UML_ADD(b, ea_result, ea_result, DRC_GET_SIB_REG32(idx));
				}
				else
				{
					UML_SHL(b, ea_index_scaled, DRC_GET_SIB_REG32(idx), scale);
					UML_ADD(b, ea_result, ea_result, ea_index_scaled);
				}
			}
		}
		else
		{
			seg = get_seg(s_rm_sreg32[ctx.desc->modrm_rm & 7]);

			UML_MOV(b, ea_result, DRC_GET_MRM_RM32(ctx.desc->modrm));
		}

		if (ctx.desc->disp && (ctx.desc->modrm_mode == 1 || ctx.desc->modrm_mode == 2))
			UML_ADD(b, ea_result, ea_result, ctx.desc->disp);

		UML_ADD(b, ea_result, ea_result, DRC_SEG(seg, base));

		ctx.cursor += (ctx.desc->has_sib ? 1 : 0) + ctx.desc->disp_bytes;
	}
}

void i386_device::drc_skip_ea16(compiler_state &ctx)
{
	if (ctx.desc && ctx.desc->has_modrm && ctx.desc->is_mem)
		ctx.cursor += ctx.desc->disp_bytes;
}

void i386_device::drc_skip_ea32(compiler_state &ctx)
{
	if (ctx.desc && ctx.desc->has_modrm && ctx.desc->is_mem)
		ctx.cursor += (ctx.desc->has_sib ? 1 : 0) + ctx.desc->disp_bytes;
}

inline void i386_device::drc_gen_rm8(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data     = I2;
	const uml::parameter &loaded_value = I0;

	if (!ctx.desc->is_mem)
	{
		UML_AND(b, loaded_value, DRC_GET_MRM_RM8(modrm), 0xff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, loaded_value, mem_data, 0xff);
	}
}

inline void i386_device::drc_gen_rm16(compiler_state &ctx, uint8_t modrm)
{
	const uml::parameter &mem_data     = I2;
	const uml::parameter &loaded_value = I0;

	drcuml_block &b = ctx.block;

	if (!ctx.desc->is_mem)
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
}

inline void i386_device::drc_gen_rm32(compiler_state &ctx, uint8_t modrm)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data     = I2;
	const uml::parameter &loaded_value = I0;

	if (!ctx.desc->is_mem)
	{
		uint8_t rm32 = MRM_RM32(modrm);

		if (ctx.rscratch_is_valid(rm32))
			UML_MOV(b, loaded_value, ctx.get_rscratch(rm32));
		else
			UML_MOV(b, loaded_value, DRC_REG32(rm32));
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, loaded_value, mem_data);
	}
}

// ----------------------------------------------------------------------------
// -drc_log_uml and -drc_log_native
// ----------------------------------------------------------------------------

namespace
{

	class i386_buf : public util::disasm_interface::data_buffer
	{
	public:
		i386_buf(offs_t base_pc, const uint8_t *buf, unsigned len) :
			m_base_pc(base_pc),
			m_buf(buf),
			m_len(len)
		{
		}

		uint8_t r8(offs_t pc) const override
		{
			uint8_t val8 = byte(pc);

			return val8;
		}
		uint16_t r16(offs_t pc) const override
		{
			uint16_t val16 = byte(pc);
			val16 |= ((uint16_t)byte(pc + 1) << 8);

			return val16;
		}
		uint32_t r32(offs_t pc) const override
		{
			uint32_t val32 = byte(pc);
			val32 |= ((uint32_t)byte(pc + 1) << 8);
			val32 |= ((uint32_t)byte(pc + 2) << 16);
			val32 |= ((uint32_t)byte(pc + 3) << 24);

			return val32;
		}
		uint64_t r64(offs_t pc) const override
		{
			uint64_t val64 = r32(pc);
			val64 |= ((uint64_t)r32(pc + 4) << 32);

			return val64;
		}

	private:
		uint8_t byte(offs_t pc) const
		{
			const offs_t off = pc - m_base_pc;

			return (off < m_len) ? m_buf[off] : 0;
		}

		offs_t         m_base_pc;
		const uint8_t *m_buf;
		unsigned       m_len;
	};

} // namespace


std::string i386_device::drc_log_disasm_one(offs_t pc, uint32_t length)
{
	uint8_t peek_buf[16] = { 0 };

	const uint32_t peek_len = std::min(length, (uint32_t)sizeof(peek_buf));

	uint32_t buf_len = 0;
	try
	{
		for (; buf_len < peek_len; buf_len++)
			peek_buf[buf_len] = drc_peek8(pc + buf_len);
	}
	catch (uint64_t)
	{
		// rest of the buffer will be cut off and remain 0x00
	}

	i386_buf disam_buf(pc, peek_buf, buf_len);

	i386_disassembler dasm(this);

	std::ostringstream stream;

	dasm.disassemble(stream, pc, disam_buf, disam_buf);

	return stream.str();
}

std::string i386_device::drc_log_desc_flags(opcode_desc const &desc)
{
	std::string s;

	// Branches

	if (desc.is_unconditional_branch())
		s += 'U';
	else if (desc.is_conditional_branch())
		s += 'C';
	else
		s += '.';

	if (desc.intrablock_branch())
		s += 'i';
	else
		s += '.';

	if (desc.is_branch_target())
		s += 'B';
	else
		s += '.';

	// Exceptions

	if (desc.will_cause_exception())
		s += 'E';
	else if (desc.can_cause_exception())
		s += 'e';
	else
		s += '.';

	// Memory operand present (M = ModRM addresses memory rather than a register)

	if (desc.is_mem)
		s += 'M';
	else
		s += '.';


	if (desc.end_sequence())
		s += 'S';
	else
		s += '.';

	if (desc.redispatch())
		s += 'R';
	else
		s += '.';


	// x86 state (operande size, address size, and if there's any segment override)

	if (desc.op32)
		s += '4';
	else
		s += '2';

	if (desc.addr32)
		s += '4';
	else
		s += '2';

	if (desc.seg_override >= 0 && desc.seg_override < 6)
	{
		static const char seg[6] = { 'E', 'C', 'S', 'D', 'F', 'G' };

		s += seg[desc.seg_override];
	}
	else
	{
		s += '.';
	}

	return s;
}

void i386_device::drc_log_opcode_desc(opcode_desc const *desclist, int indent)
{
	if (desclist == nullptr)
		return;

	// Open the file, creating it if necessary
	if (indent == 0)
		m_drc_uml->log_printf("\nDescriptor list @ %08X\n", desclist->pc);

	// Output each descriptor
	for (; desclist != nullptr; desclist = desclist->next())
	{
		std::string dasm_string;
		if (desclist->virtual_noop())
			dasm_string = std::string("<virtual nop>");
		else
			dasm_string = drc_log_disasm_one(desclist->pc, desclist->length);

		m_drc_uml->log_printf("%08X t:%08X f:%-16s %-34s ; op=%02X", desclist->pc, desclist->targetpc, drc_log_desc_flags(*desclist).c_str(), dasm_string.c_str(), desclist->opcode0);
		if (desclist->opcode_len == 2)
			m_drc_uml->log_printf(" %02X", unsigned(desclist->opcode1));
		if (desclist->has_modrm)
			m_drc_uml->log_printf(" modrm=%02X", unsigned(desclist->modrm));
		if (desclist->disp_bytes != 0)
			m_drc_uml->log_printf(" disp=%08X", uint32_t(desclist->disp));
		if (desclist->imm_bytes != 0)
			m_drc_uml->log_printf(" imm=%08X", desclist->imm);
		m_drc_uml->log_printf(" len=%u\n", unsigned(desclist->length));

		// Register state purposly skipped in the frontend.
		// Mainly because it would make code a bit more of a mess on the frontend side
		// May add later if needed

		// At the end of a sequence add a dividing line
		if (desclist->end_sequence())
			m_drc_uml->log_printf("-----\n");
	}
}

void i386_device::drc_log_add_disasm_comment(drcuml_block &block, opcode_desc const *desc)
{
	if (m_drc_uml->logging())
		block.append_comment("%08X: %s", desc->pc, drc_log_disasm_one(desc->pc, desc->length).c_str());
}

void i386_device::drc_add_symbols()
{
	drcuml_state &d = *m_drc_uml;
	char          buf[24];

	d.symbol_add(&m_core->pc, sizeof(m_core->pc), "pc");
	d.symbol_add(&m_core->eip, sizeof(m_core->eip), "eip");
	d.symbol_add(&m_core->prev_eip, sizeof(m_core->prev_eip), "prev_eip");
	d.symbol_add(&m_core->cycles, sizeof(m_core->cycles), "cycles");
	d.symbol_add(&m_core->base_cycles, sizeof(m_core->base_cycles), "base_cycles");
	d.symbol_add(&m_core->pending_cycles, sizeof(m_core->pending_cycles), "pending_cycles");
	d.symbol_add(&m_core->tsc, sizeof(m_core->tsc), "tsc");

	// General-purpose registers or m_core->reg.d[]
	static char const *const regname32[8] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
	for (int i = 0; i < 8; i++)
		d.symbol_add(&m_core->reg.d[i], sizeof(m_core->reg.d[i]), regname32[i]);

	d.symbol_add(&m_core->eflags, sizeof(m_core->eflags), "eflags");
	d.symbol_add(&m_core->eflags_mask, sizeof(m_core->eflags_mask), "eflags_mask");

	// Individual flag values, unpacked from eflags
	d.symbol_add(&m_core->CF, sizeof(m_core->CF), "CF");
	d.symbol_add(&m_core->PF, sizeof(m_core->PF), "PF");
	d.symbol_add(&m_core->AF, sizeof(m_core->AF), "AF");
	d.symbol_add(&m_core->ZF, sizeof(m_core->ZF), "ZF");
	d.symbol_add(&m_core->SF, sizeof(m_core->SF), "SF");
	d.symbol_add(&m_core->TF, sizeof(m_core->TF), "TF");
	d.symbol_add(&m_core->IF, sizeof(m_core->IF), "IF");
	d.symbol_add(&m_core->DF, sizeof(m_core->DF), "DF");
	d.symbol_add(&m_core->OF, sizeof(m_core->OF), "OF");
	d.symbol_add(&m_core->IOPL, sizeof(m_core->IOPL), "IOPL");
	d.symbol_add(&m_core->NT, sizeof(m_core->NT), "NT");
	d.symbol_add(&m_core->RF, sizeof(m_core->RF), "RF");
	d.symbol_add(&m_core->VM, sizeof(m_core->VM), "VM");
	d.symbol_add(&m_core->AC, sizeof(m_core->AC), "AC");
	d.symbol_add(&m_core->VIF, sizeof(m_core->VIF), "VIF");
	d.symbol_add(&m_core->VIP, sizeof(m_core->VIP), "VIP");
	d.symbol_add(&m_core->ID, sizeof(m_core->ID), "ID");
	d.symbol_add(&m_core->CPL, sizeof(m_core->CPL), "CPL");

	// Control register
	for (int i = 0; i < 5; i++)
	{
		snprintf(buf, sizeof(buf), "cr%d", i);
		d.symbol_add(&m_core->cr[i], sizeof(m_core->cr[i]), buf);
	}

	// Debug registers
	for (int i = 0; i < 8; i++)
	{
		snprintf(buf, sizeof(buf), "dr%d", i);
		d.symbol_add(&m_core->dr[i], sizeof(m_core->dr[i]), buf);
	}

	// Common IRQ state
	d.symbol_add(&m_core->irq_state, sizeof(m_core->irq_state), "irq_state");
	d.symbol_add(&m_core->ext, sizeof(m_core->ext), "ext");
	d.symbol_add(&m_core->delayed_interrupt_enable, sizeof(m_core->delayed_interrupt_enable), "delayed_interrupt_enable");

	// segment registers sreg[] for ES, CS, SS, DS, FS, GS
	static char const *const segname[6] = { "es", "cs", "ss", "ds", "fs", "gs" };
	for (int i = 0; i < 6; i++)
	{
		snprintf(buf, sizeof(buf), "%s.selector", segname[i]);
		d.symbol_add(&m_core->sreg[i].selector, sizeof(m_core->sreg[i].selector), buf);
		snprintf(buf, sizeof(buf), "%s.flags", segname[i]);
		d.symbol_add(&m_core->sreg[i].flags, sizeof(m_core->sreg[i].flags), buf);
		snprintf(buf, sizeof(buf), "%s.base", segname[i]);
		d.symbol_add(&m_core->sreg[i].base, sizeof(m_core->sreg[i].base), buf);
		snprintf(buf, sizeof(buf), "%s.limit", segname[i]);
		d.symbol_add(&m_core->sreg[i].limit, sizeof(m_core->sreg[i].limit), buf);
		snprintf(buf, sizeof(buf), "%s.d", segname[i]);
		d.symbol_add(&m_core->sreg[i].d, sizeof(m_core->sreg[i].d), buf);
	}

	// Global Descriptor Table
	d.symbol_add(&m_core->gdtr.base, sizeof(m_core->gdtr.base), "gdtr.base");
	d.symbol_add(&m_core->gdtr.limit, sizeof(m_core->gdtr.limit), "gdtr.limit");

	// Interrupt Descriptor Table
	d.symbol_add(&m_core->idtr.base, sizeof(m_core->idtr.base), "idtr.base");
	d.symbol_add(&m_core->idtr.limit, sizeof(m_core->idtr.limit), "idtr.limit");

	// Task register
	d.symbol_add(&m_core->task.segment, sizeof(m_core->task.segment), "tr.segment");
	d.symbol_add(&m_core->task.flags, sizeof(m_core->task.flags), "tr.flags");
	d.symbol_add(&m_core->task.base, sizeof(m_core->task.base), "tr.base");
	d.symbol_add(&m_core->task.limit, sizeof(m_core->task.limit), "tr.limit");

	// Local Descriptor Table
	d.symbol_add(&m_core->ldtr.segment, sizeof(m_core->ldtr.segment), "ldtr.segment");
	d.symbol_add(&m_core->ldtr.flags, sizeof(m_core->ldtr.flags), "ldtr.flags");
	d.symbol_add(&m_core->ldtr.base, sizeof(m_core->ldtr.base), "ldtr.base");
	d.symbol_add(&m_core->ldtr.limit, sizeof(m_core->ldtr.limit), "ldtr.limit");

	// Current A20 mask
	d.symbol_add(&m_core->a20_mask, sizeof(m_core->a20_mask), "a20_mask");

	d.symbol_add(&m_core->smm, sizeof(m_core->smm), "smm");
	d.symbol_add(&m_core->smi, sizeof(m_core->smi), "smi");

	// x87 control/status
	d.symbol_add(&m_core->x87_cw, sizeof(m_core->x87_cw), "x87_cw");
	d.symbol_add(&m_core->x87_sw, sizeof(m_core->x87_sw), "x87_sw");
	d.symbol_add(&m_core->x87_tw, sizeof(m_core->x87_tw), "x87_tw");

	// DRC interrupt and exception state
	d.symbol_add(&m_core->performed_intersegment_jump, sizeof(m_core->performed_intersegment_jump), "performed_intersegment_jump");
	d.symbol_add(&m_core->irq_vector_pending, sizeof(m_core->irq_vector_pending), "irq_vector_pending");
	d.symbol_add(&m_core->irq_gate_is_trap, sizeof(m_core->irq_gate_is_trap), "irq_gate_is_trap");
	d.symbol_add(&m_core->irq_old_cs_selector, sizeof(m_core->irq_old_cs_selector), "irq_old_cs_selector");
	d.symbol_add(&m_core->irq_old_ss_selector, sizeof(m_core->irq_old_ss_selector), "irq_old_ss_selector");
	d.symbol_add(&m_core->irq_old_esp, sizeof(m_core->irq_old_esp), "irq_old_esp");
	d.symbol_add(&m_core->soft_int_vector, sizeof(m_core->soft_int_vector), "soft_int_vector");
	d.symbol_add(&m_core->soft_int_ret_eip, sizeof(m_core->soft_int_ret_eip), "soft_int_ret_eip");

	// Control-transfer scratch (iret, retf, IRQ delivery)
	d.symbol_add(&m_core->ctl_target_dpl, sizeof(m_core->ctl_target_dpl), "ctl_target_dpl");
	d.symbol_add(&m_core->ctl_new_ss, sizeof(m_core->ctl_new_ss), "ctl_new_ss");
	d.symbol_add(&m_core->ctl_new_esp, sizeof(m_core->ctl_new_esp), "ctl_new_esp");
	d.symbol_add(&m_core->ctl_newflags, sizeof(m_core->ctl_newflags), "ctl_newflags");
	d.symbol_add(&m_core->ctl_new_cs, sizeof(m_core->ctl_new_cs), "ctl_new_cs");
	d.symbol_add(&m_core->ctl_new_eip, sizeof(m_core->ctl_new_eip), "ctl_new_eip");
	d.symbol_add(&m_core->ctl_pop_count, sizeof(m_core->ctl_pop_count), "ctl_pop_count");

	// DRC deferred-flags state
	d.symbol_add(&m_core->flags_data_a, sizeof(m_core->flags_data_a), "flags_data_a");
	d.symbol_add(&m_core->flags_data_b, sizeof(m_core->flags_data_b), "flags_data_b");
	d.symbol_add(&m_core->flags_carry, sizeof(m_core->flags_carry), "flags_carry");
	d.symbol_add(&m_core->flags_optype, sizeof(m_core->flags_optype), "flags_optype");
	d.symbol_add(&m_core->flags_of_direct, sizeof(m_core->flags_of_direct), "flags_of_direct");
	d.symbol_add(&m_core->flags_cc, sizeof(m_core->flags_cc), "flags_cc");

	// Self-modified code helpers
	d.symbol_add(&m_core->page_invalidate_addr, sizeof(m_core->page_invalidate_addr), "page_invalidate_addr");
	d.symbol_add(&m_core->page_invalidate_mode, sizeof(m_core->page_invalidate_mode), "page_invalidate_mode");
	d.symbol_add(&m_core->tlb_miss_faulted, sizeof(m_core->tlb_miss_faulted), "tlb_miss_faulted");
	d.symbol_add(&m_core->tlb_fault_code, sizeof(m_core->tlb_fault_code), "tlb_fault_code");

	// Shared page-table walk scratch
	d.symbol_add(&m_core->tw_pde, sizeof(m_core->tw_pde), "tw_pde");
	d.symbol_add(&m_core->tw_pte, sizeof(m_core->tw_pte), "tw_pte");
	d.symbol_add(&m_core->tw_perm, sizeof(m_core->tw_perm), "tw_perm");
	d.symbol_add(&m_core->tw_is4m, sizeof(m_core->tw_is4m), "tw_is4m");
	d.symbol_add(&m_core->tw_scratch, sizeof(m_core->tw_scratch), "tw_scratch");
	d.symbol_add(&m_core->tw_supervisor_read, sizeof(m_core->tw_supervisor_read), "tw_supervisor_read");

	// DRC scratch / helpers
	d.symbol_add(&m_core->mem_laddr, sizeof(m_core->mem_laddr), "mem_laddr");
	d.symbol_add(&m_core->mem_paddr, sizeof(m_core->mem_paddr), "mem_paddr");
	d.symbol_add(&m_core->mem_iswrite, sizeof(m_core->mem_iswrite), "mem_iswrite");
	d.symbol_add(&m_core->data8, sizeof(m_core->data8), "data8");
	d.symbol_add(&m_core->data16, sizeof(m_core->data16), "data16");
	d.symbol_add(&m_core->data32, sizeof(m_core->data32), "data32");
	d.symbol_add(&m_core->data64, sizeof(m_core->data64), "data64");
	d.symbol_add(&m_core->data128, sizeof(m_core->data128), "data128");

	// REP chunking scratch
	d.symbol_add(&m_core->rep_chunked_physstart, sizeof(m_core->rep_chunked_physstart), "rep_chunked_physstart");
	d.symbol_add(&m_core->rep_chunked_bytes, sizeof(m_core->rep_chunked_bytes), "rep_chunked_bytes");
	d.symbol_add(&m_core->rep_chunked_fillvalue, sizeof(m_core->rep_chunked_fillvalue), "rep_chunked_fillvalue");
	d.symbol_add(&m_core->rep_chunked_pending_region, sizeof(m_core->rep_chunked_pending_region), "rep_chunked_pending_region");
	d.symbol_add(&m_core->rep_chunked_src_physstart, sizeof(m_core->rep_chunked_src_physstart), "rep_chunked_src_physstart");
	d.symbol_add(&m_core->rep_chunked_dst_physstart, sizeof(m_core->rep_chunked_dst_physstart), "rep_chunked_dst_physstart");
	d.symbol_add(&m_core->rep_chunked_movs_bytes, sizeof(m_core->rep_chunked_movs_bytes), "rep_chunked_movs_bytes");
	d.symbol_add(&m_core->rep_chunked_pending_src_region, sizeof(m_core->rep_chunked_pending_src_region), "rep_chunked_pending_src_region");
	d.symbol_add(&m_core->rep_chunked_pending_dst_region, sizeof(m_core->rep_chunked_pending_dst_region), "rep_chunked_pending_dst_region");

	// DRC cache state
	d.symbol_add(&m_core->drc_cache_dirty, sizeof(m_core->drc_cache_dirty), "drc_cache_dirty");
	d.symbol_add(&m_core->drc_cached_invariant, sizeof(m_core->drc_cached_invariant), "drc_cached_invariant");
}

// ----------------------------------------------------------------------------
// Other diagnostics
// ----------------------------------------------------------------------------

// Just a basic printf to signal basic info for debugging
void i386_device::func_log_printf()
{
	printf("func_log_printf: pc=%08x, eip=%08x, prev_eip=%08x, arg0=%08x, arg1=%08x, arg2=%08x\n", m_core->pc, m_core->eip, m_core->prev_eip, m_core->drc_debug_arg0, m_core->drc_debug_arg1, m_core->drc_debug_arg2);
}

void i386_device::func_printf_ramdiag()
{
	std::time_t now = std::time(nullptr);

	if (std::difftime(now, m_last_ramdiag_print) >= DEBUG_DIAGRAM_FREQ)
	{
		// Using fast top-k selection sort here

		using MapIterator = decltype(m_diag_slowram_alog)::const_iterator;
		std::vector<MapIterator> iterators;
		iterators.reserve(m_diag_slowram_alog.size());

		for (auto it = m_diag_slowram_alog.cbegin(); it != m_diag_slowram_alog.cend(); ++it)
			iterators.push_back(it);

		// The "k" value
		uint32_t print_count = std::min(iterators.size(), size_t(DEBUG_DIAGRAM_ACNT));

		auto sort_compare = [](const MapIterator &a, const MapIterator &b)
		{
			if (DEBUG_DIAGRAM_TSORT)
				return a->second.adur > b->second.adur;
			else
				return a->second.acnt > b->second.acnt;
		};

		std::nth_element(iterators.begin(), iterators.begin() + print_count, iterators.end(), sort_compare);

		std::sort(iterators.begin(), iterators.begin() + print_count, sort_compare);

		// Print out sorted addresses

		printf("\n=== MEMORY ACCESS STATS ==\n\n");

		uint64_t cnt_total = m_diag_slowram_acnt + m_diag_fastram_acnt;
		if (cnt_total == 0)
			cnt_total = 1;
		double fastram_cntpct = ((double)m_diag_fastram_acnt / (double)cnt_total) * 100.0;

		uint64_t dur_total = m_diag_slowram_adur + m_diag_fastram_adur;
		if (dur_total == 0)
			dur_total = 1;
		double fastram_durpct = ((double)m_diag_fastram_adur / (double)dur_total) * 100.0;

		printf("Fastpath coverage: %.2f%% hits %.2f%% time, popular slow addresses below:\n\n", fastram_cntpct, fastram_durpct);
		printf("%-22s | %-20s | %-18s | %s\n", "Slow Address", "Last PC", "Access Count", "Access Time");
		printf("-----------------------------------------------------------------------------------------\n");
		for (uint32_t i = 0; i < print_count; i++)
		{
			uint64_t mem_addr = iterators[i]->first;
			uint32_t pc_val   = iterators[i]->second.last_pc;
			uint32_t hits     = iterators[i]->second.acnt;
			uint32_t duration = iterators[i]->second.adur;

			bool is_write = (mem_addr & MEM_DIAG_WRITE_FLAG);

			mem_addr &= (~MEM_DIAG_WRITE_FLAG);

			printf("%c 0x%-18.8x | 0x%-18.8x | %-18u | %-18uns\n", ((is_write) ? 'W' : 'R'), (uint32_t)mem_addr, pc_val, hits, duration);
		}
		printf("=========================================================================================\n\n");

		m_last_ramdiag_print = now;
	}
}


void i386_device::func_ramlog_epoch()
{
	m_diag_ramlog_epoch = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}


void i386_device::func_log_fastram()
{
	m_diag_fastram_acnt++;

	uint64_t end = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	m_diag_fastram_adur += (end - m_diag_ramlog_epoch);

	i386_device::func_printf_ramdiag();
}


void i386_device::func_log_slowram()
{
	m_diag_slowram_acnt++;

	uint64_t end  = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	uint64_t adur = (end - m_diag_ramlog_epoch);
	m_diag_slowram_adur += adur;

	if (m_core->mem_diag_is_write == 1)
		m_core->mem_diag_addr |= MEM_DIAG_WRITE_FLAG;
	else
		m_core->mem_diag_addr &= (~MEM_DIAG_WRITE_FLAG);

	m_diag_slowram_alog[m_core->mem_diag_addr].acnt++;
	// This is just a general total time spend. Wont help in bursty situations, would need better diag for that.
	m_diag_slowram_alog[m_core->mem_diag_addr].adur += adur;
	m_diag_slowram_alog[m_core->mem_diag_addr].last_pc = m_core->pc;

	i386_device::func_printf_ramdiag();
}


void i386_device::func_printf_ifallbackdiag()
{
	std::time_t now = std::time(nullptr);

	if (std::difftime(now, m_last_ifallbackdiag_print) < DEBUG_DIAGIFALLB_FREQ)
		return;

	// Using fast top-k selection sort here

	using MapIterator = decltype(m_diag_ifallback_alog)::const_iterator;
	std::vector<MapIterator> iterators;
	iterators.reserve(m_diag_ifallback_alog.size());
	for (auto it = m_diag_ifallback_alog.cbegin(); it != m_diag_ifallback_alog.cend(); ++it)
		iterators.push_back(it);

	// The "k" value
	uint32_t print_count = std::min(iterators.size(), size_t(DEBUG_DIAGIFALLB_ACNT));

	auto sort_compare = [](const MapIterator &a, const MapIterator &b)
	{
		return a->second.acnt > b->second.acnt;
	};

	std::nth_element(iterators.begin(), iterators.begin() + print_count, iterators.end(), sort_compare);

	std::sort(iterators.begin(), iterators.begin() + print_count, sort_compare);

	// Print out sorted opcodes

	printf("\n=== INTERPRETER FALLBACK STATS ==\n\n");

	uint64_t cnt_total = m_diag_itotal_acnt;
	if (cnt_total == 0)
		cnt_total = 1;

	double drc_cntpct = ((double)(m_diag_itotal_acnt - m_diag_ifallback_acnt) / (double)cnt_total) * 100.0;

	printf("DRC coverage: %.2f%% hits, popular fallback instructions below:\n\n", drc_cntpct);

	fprintf(stderr, "%-17s | %-15s | %-15s | %s\n", "Exec Count", "% of Fallback", "Last PC", "First Bytes");
	fprintf(stderr, "-----------------------------------------------------------------------------------------\n");
	for (uint32_t i = 0; i < print_count; i++)
	{
		const diag_ifallback_entry &entry = iterators[i]->second;

		uint32_t pc_val = entry.last_pc;
		uint32_t hits   = entry.acnt;

		double pct = ((double)hits / (double)m_diag_ifallback_acnt) * 100.0;

		fprintf(stderr, "%-17u | %14.2f%% | 0x%-13.8x | %02x %02x %02x %02x %02x %02x\n", hits, pct, pc_val, entry.bytes[0], entry.bytes[1], entry.bytes[2], entry.bytes[3], entry.bytes[4], entry.bytes[5]);
	}
	fprintf(stderr, "=========================================================================================\n\n");

	m_last_ifallbackdiag_print = now;
}

uint8_t i386_device::drc_diag_read_byte(offs_t address)
{
	if (void *p = drc_try_fastram(address, 1, false))
	{
		return *(uint8_t *)p;
	}
	else
	{
		uint32_t error;
		if (!translate_address(m_core->CPL, TR_FETCH, &address, &error))
			return 0;
		return macache32.read_byte(address & m_core->a20_mask);
	}
}

uint32_t i386_device::drc_compute_fallback_key(offs_t pc)
{
	// NOTE: gotos in here are for improved performance.

	uint8_t opcode0 = 0;
	uint8_t opcode1 = 0;

	drc_dispatch_type dispatch_type = DISPATCH_PRI_TABLE;

	uint8_t opcode_len = 0;

	bool osz_override = false;

	offs_t cursor = pc;
	while (true)
	{
		uint8_t byte = drc_diag_read_byte(cursor++);

		switch (byte)
		{
			// ES segment override
			case 0x26:
			// CS segment override
			case 0x2e:
			// SS segment override
			case 0x36:
			// DS segment override
			case 0x3e:
			// FS segment override
			case 0x64:
			// GS segment override
			case 0x65:
			// Exclusive use of all shared memory (lock)
			case 0xf0:
			// Address size override (change address size 16-bit vs 32-bit)
			case 0x67:
				continue;
			// Operand size override (change data size 16-bit vs 32-bit)
			case 0x66:
				osz_override = true;
				continue;
			// INT n, categorizing based on each INT type
			case 0xcd:
				opcode_len = 2;
				opcode0    = byte;
				goto done_rewind_cursor;
			// 0x0f type-byte op
			case 0x0f:
				opcode_len    = 2;
				dispatch_type = DISPATCH_X0F_TABLE;
				opcode0       = byte;
				goto done_rewind_cursor;
			// REP/REPNE type-byte string op
			case 0xf2:
			case 0xf3:
				opcode_len = 2;
				opcode0    = byte;
				goto done_rewind_cursor;
			// Eveyrthing else isn't a prefix, so we need to rewind the cursor
			default:
				opcode0 = byte;
				goto done_rewind_cursor;
		}
	}

done_rewind_cursor:

	cursor--;
	opcode1 = (opcode_len == 2) ? drc_diag_read_byte(cursor++) : 0;

	uint32_t fallback_key = 0x00000000;

	drc_dispatch_t disp_desc;
	switch (dispatch_type)
	{
		case DISPATCH_X0F_TABLE:
			disp_desc = m_drc_sel_x0f_table[opcode1];
			break;
		case DISPATCH_PRI_TABLE:
		default:
			disp_desc = m_drc_sel_pri_table[opcode0];
			break;
	}

	const uint32_t flags = disp_desc.drc_flags;

	if (opcode_len == 2)
		fallback_key = (opcode0 << 8) | opcode1;
	else
		fallback_key = opcode0;

	if (flags & DRC_GROUP)
		fallback_key |= OP_GET_SUBOP(drc_diag_read_byte(cursor++)) << 16;

	if (osz_override)
		fallback_key |= (1 << 24);

	return fallback_key;
}

void i386_device::func_log_fallback_exec(offs_t pc)
{
	m_diag_ifallback_acnt++;

	uint32_t key = drc_compute_fallback_key(pc);

	diag_ifallback_entry &entry = m_diag_ifallback_alog[key];

	entry.acnt++;

	entry.last_pc = pc;

	if (entry.acnt == 1)
	{
		for (int i = 0; i < std::size(entry.bytes); i++)
			entry.bytes[i] = drc_diag_read_byte(pc + i);
	}

	func_printf_ifallbackdiag();
}

void i386_device::func_log_instruction_exec()
{
	m_diag_itotal_acnt++;
}

#include "drc_i386alu.hxx"
#include "drc_i386ctl.hxx"
#include "drc_i386flags.hxx"
#include "drc_i386irq.hxx"
#include "drc_i386mem.hxx"
#include "drc_i386rep.hxx"
#include "drc_i386segs.hxx"
#include "drc_i386stack.hxx"
#include "drc_i386sysio.hxx"
#include "drc_pentium.hxx"
#include "drc_x87.hxx"
