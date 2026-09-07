// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

// ----------------------------------------------------------------------------
// Invariant: the shared fastmem accessor and tlb resolve handles
// ----------------------------------------------------------------------------

void i386_device::static_generate_memory_accessors()
{
	static_generate_memory_accessor(1, false, "mem_read8", m_mem_read8);
	static_generate_memory_accessor(1, true, "mem_write8", m_mem_write8);
	static_generate_memory_accessor(2, false, "mem_read16", m_mem_read16);
	static_generate_memory_accessor(2, true, "mem_write16", m_mem_write16);
	static_generate_memory_accessor(4, false, "mem_read32", m_mem_read32);
	static_generate_memory_accessor(4, true, "mem_write32", m_mem_write32);
	static_generate_memory_accessor(8, false, "mem_read64", m_mem_read64);
	static_generate_memory_accessor(8, true, "mem_write64", m_mem_write64);
	static_generate_memory_accessor(16, false, "mem_read128", m_mem_read128);
	static_generate_memory_accessor(16, true, "mem_write128", m_mem_write128);
}

void i386_device::static_generate_memory_accessor(int size, bool iswrite, const char *name, uml::code_handle *&handleptr)
{
	alloc_handle(*m_drc_uml, handleptr, name);

	drcuml_block *b_ptr;
	if (m_drc_options & I386DRC_INVARIANT_FASTRAM)
		b_ptr = &m_drc_uml->begin_invariant_block(4096);
	else
		b_ptr = &m_drc_uml->begin_block(4096);

	drcuml_block &b = *b_ptr;

	UML_HANDLE(b, *handleptr);

	const uml::parameter &raddr                = I0;
	const uml::parameter &data                 = I2;
	const uml::parameter &page_overflow_amount = I3;
	const uml::parameter &tlb_addr             = I4;
	const uml::parameter &tlb_flag_check       = I3;

	int label_ctr = 1;

	const uml::code_label do_nonpage_op = NEW_SLBL();

	// Check if page mode is enabled
	UML_TEST(b, DRC_CR(0), CR0_PG);
	UML_JMPc(b, COND_Z, do_nonpage_op);

	// Paging memory access

	const uml::code_label access_memory     = NEW_SLBL();
	const uml::code_label cross_page_access = NEW_SLBL();

	if (size > 1)
	{
		UML_AND(b, page_overflow_amount, raddr, (0x1000 - 1));
		UML_SUB(b, page_overflow_amount, page_overflow_amount, (0x1000 - size));

		UML_CMP(b, page_overflow_amount, 0);
		UML_JMPc(b, COND_G, cross_page_access);
	}

	uint32_t tlb_valid_flags = FLAG_VALID | (iswrite ? FLAG_DIRTY : 0);

	UML_SHR(b, tlb_addr, raddr, 12);
	UML_LOAD(b, tlb_addr, vtlb_table(), tlb_addr, SIZE_DWORD, SCALE_x4);

	const uml::code_label tlb_is_valid = NEW_SLBL();

	UML_AND(b, tlb_flag_check, tlb_addr, tlb_valid_flags);

	UML_CMP(b, tlb_flag_check, tlb_valid_flags);
	UML_JMPc(b, COND_E, tlb_is_valid);

	UML_MOV(b, uml::mem(&m_core->mem_laddr), raddr);
	UML_MOV(b, uml::mem(&m_core->mem_iswrite), iswrite);
	UML_MOV(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_CALLH(b, *m_resolve_tlb);

	const uml::code_label tlb_resolve_faulted = NEW_SLBL();

	UML_CMP(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_JMPc(b, COND_NE, tlb_resolve_faulted);

	UML_MOV(b, raddr, uml::mem(&m_core->mem_paddr));
	UML_JMP(b, access_memory);

	UML_LABEL(b, tlb_resolve_faulted);
	UML_CALLC(b, I386_CB(drc_tlb_fault_cb), this); // page fault
	UML_EXH(b, *m_fault, DRC_PC);

	UML_LABEL(b, tlb_is_valid);

	UML_ROLINS(b, raddr, tlb_addr, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, raddr, raddr, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, access_memory);
	// Writing direct to the buffer is incompatible with the debugger (and will make things confusing)
	if (!debugger_enabled())
	{
		drc_gen_fastram_memory_access(b, label_ctr, raddr, data, size, iswrite);
		drc_gen_fastpaged_memory_access(b, label_ctr, raddr, data, size, iswrite);
	}
	drc_gen_addrspace_memory_access(b, label_ctr, raddr, data, size, iswrite);

	UML_LABEL(b, cross_page_access);
	drc_gen_cross_page_memory_access(b, raddr, data, size, iswrite);

	// Non-paging memory access

	UML_LABEL(b, do_nonpage_op);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, raddr, raddr, uml::mem(&m_core->a20_mask));

	// Skip TLB steps and access memory as-is
	UML_JMP(b, access_memory);

	b.end();
}

void i386_device::static_generate_resolve_tlb()
{
	alloc_handle(*m_drc_uml, m_resolve_tlb, "resolve_tlb");

	drcuml_block &b(m_drc_uml->begin_invariant_block(4096));

	UML_HANDLE(b, *m_resolve_tlb);

	const uml::parameter &pde_pa = I3;
	const uml::parameter &pde    = I3;
	const uml::parameter &pte_pa = I3;
	const uml::parameter &pte    = I3;
	const uml::parameter &phys   = I3;

	int label_ctr = 1;

	const uml::code_label do_4k        = NEW_SLBL();
	const uml::code_label have_phys    = NEW_SLBL();
	const uml::code_label not_present  = NEW_SLBL();
	const uml::code_label fault_finish = NEW_SLBL();
	const uml::code_label ff_no_write  = NEW_SLBL();
	const uml::code_label ff_no_user   = NEW_SLBL();
	const uml::code_label done         = NEW_SLBL();

	auto read_phys32 = [&](const uml::parameter &addr, const uml::parameter &data)
	{
		const uml::code_label fr_done = NEW_SLBL();
		if (m_drc_options & I386DRC_INLINE_FASTRAM && !debugger_enabled())
		{
			for (int r = 0; r < m_fastram_select; r++)
			{
				const fastram_entry &fi = m_fastram[r];
				if (fi.virtual_base || fi.excluded_count > 0)
					continue;
				if (fi.access_size != 0 && fi.access_size != 4)
					continue;

				const uml::code_label skip = NEW_SLBL();
				if (fi.start != 0x00000000)
				{
					UML_CMP(b, addr, fi.start);
					UML_JMPc(b, COND_B, skip);
				}

				UML_CMP(b, addr, fi.end - 3);
				UML_JMPc(b, COND_A, skip);

				UML_LOAD(b, data, (uint8_t *)fi.base - fi.start, addr, SIZE_DWORD, SCALE_x1);

				UML_JMP(b, fr_done);

				UML_LABEL(b, skip);
			}
		}

		UML_READ_NOFP(b, data, addr, SIZE_DWORD, SPACE_PROGRAM);

		UML_LABEL(b, fr_done);
	};

	UML_MOV(b, pde_pa, DRC_CR(3));
	UML_AND(b, pde_pa, pde_pa, 0xfffff000);
	UML_ROLINS(b, pde_pa, uml::mem(&m_core->mem_laddr), 12, 0x00000ffc);
	read_phys32(pde_pa, pde);
	UML_MOV(b, uml::mem(&m_core->tw_pde), pde);

	UML_TEST(b, pde, 1);
	UML_JMPc(b, COND_Z, not_present);

	UML_TEST(b, pde, 0x80);
	UML_JMPc(b, COND_Z, do_4k);

	UML_TEST(b, DRC_CR(4), CR4_PSE);
	UML_JMPc(b, COND_Z, do_4k);

	UML_MOV(b, uml::mem(&m_core->tw_is4m), 1);
	UML_MOV(b, uml::mem(&m_core->tw_pte), 0);
	UML_AND(b, phys, pde, 0xffc00000);
	UML_ROLINS(b, phys, uml::mem(&m_core->mem_laddr), 0, 0x003fffff);
	UML_JMP(b, have_phys);

	UML_LABEL(b, do_4k);
	UML_MOV(b, uml::mem(&m_core->tw_is4m), 0);

	UML_AND(b, pte_pa, pde, 0xfffff000);
	UML_ROLINS(b, pte_pa, uml::mem(&m_core->mem_laddr), 22, 0x00000ffc);
	read_phys32(pte_pa, pte);
	UML_MOV(b, uml::mem(&m_core->tw_pte), pte);

	UML_TEST(b, pte, 1);
	UML_JMPc(b, COND_Z, not_present);

	UML_AND(b, phys, pte, 0xfffff000);
	UML_ROLINS(b, phys, uml::mem(&m_core->mem_laddr), 0, 0x00000fff);

	UML_LABEL(b, have_phys);
	UML_MOV(b, uml::mem(&m_core->mem_paddr), phys);

	const uml::code_label deny          = NEW_SLBL();
	const uml::code_label perm_is_pte   = NEW_SLBL();
	const uml::code_label ps_have_ur    = NEW_SLBL();
	const uml::code_label ps_have_w     = NEW_SLBL();
	const uml::code_label ps_ro         = NEW_SLBL();
	const uml::code_label dc_wr_checks  = NEW_SLBL();
	const uml::code_label perms_ok      = NEW_SLBL();
	const uml::code_label no_setdirty   = NEW_SLBL();
	const uml::code_label pde_maybe20   = NEW_SLBL();
	const uml::code_label pde_done      = NEW_SLBL();
	const uml::code_label pte_maybe20   = NEW_SLBL();
	const uml::code_label ad_done       = NEW_SLBL();
	const uml::code_label dl_skip_evict = NEW_SLBL();
	const uml::code_label dl_no_old     = NEW_SLBL();
	const uml::code_label dl_di_ok      = NEW_SLBL();

	UML_MOV(b, phys, uml::mem(&m_core->tw_pte));

	UML_CMP(b, uml::mem(&m_core->tw_is4m), 0);
	UML_JMPc(b, COND_E, perm_is_pte);

	UML_MOV(b, phys, uml::mem(&m_core->tw_pde));
	UML_LABEL(b, perm_is_pte);

	UML_MOV(b, uml::mem(&m_core->tw_perm), READ_ALLOWED);

	UML_TEST(b, phys, 4);
	UML_JMPc(b, COND_Z, ps_have_ur);

	UML_OR(b, uml::mem(&m_core->tw_perm), uml::mem(&m_core->tw_perm), USER_READ_ALLOWED);
	UML_LABEL(b, ps_have_ur);

	UML_TEST(b, DRC_CR(0), CR0_WP);
	UML_JMPc(b, COND_NZ, ps_have_w);

	UML_OR(b, uml::mem(&m_core->tw_perm), uml::mem(&m_core->tw_perm), WRITE_ALLOWED);
	UML_LABEL(b, ps_have_w);

	UML_TEST(b, phys, 2);
	UML_JMPc(b, COND_Z, ps_ro);

	UML_OR(b, uml::mem(&m_core->tw_perm), uml::mem(&m_core->tw_perm), WRITE_ALLOWED);

	UML_TEST(b, phys, 4);
	UML_JMPc(b, COND_Z, ps_ro);

	UML_OR(b, uml::mem(&m_core->tw_perm), uml::mem(&m_core->tw_perm), USER_WRITE_ALLOWED);
	UML_LABEL(b, ps_ro);

	UML_CMP(b, uml::mem(&m_core->tw_supervisor_read), 0);
	UML_JMPc(b, COND_NZ, dc_wr_checks);

	UML_CMP(b, uml::mem(&m_core->CPL), 3);
	UML_JMPc(b, COND_NE, dc_wr_checks);

	UML_TEST(b, uml::mem(&m_core->tw_perm), USER_READ_ALLOWED);
	UML_JMPc(b, COND_Z, deny);

	UML_LABEL(b, dc_wr_checks);

	UML_CMP(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_JMPc(b, COND_E, perms_ok);

	UML_TEST(b, uml::mem(&m_core->tw_perm), WRITE_ALLOWED);
	UML_JMPc(b, COND_Z, deny);

	UML_CMP(b, uml::mem(&m_core->CPL), 3);
	UML_JMPc(b, COND_NE, perms_ok);

	UML_TEST(b, uml::mem(&m_core->tw_perm), USER_WRITE_ALLOWED);
	UML_JMPc(b, COND_Z, deny);

	UML_LABEL(b, perms_ok);

	UML_CMP(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_JMPc(b, COND_E, no_setdirty);

	UML_OR(b, uml::mem(&m_core->tw_perm), uml::mem(&m_core->tw_perm), FLAG_DIRTY);
	UML_LABEL(b, no_setdirty);

	auto ad_write = [&](const uml::parameter &addr, uint32_t *entry_field, uint32_t orbits)
	{
		const uml::code_label fr_done = NEW_SLBL();

		UML_MOV(b, uml::mem(&m_core->tw_scratch), uml::mem(entry_field));
		UML_OR(b, uml::mem(&m_core->tw_scratch), uml::mem(&m_core->tw_scratch), orbits);

		if (m_drc_options & I386DRC_INLINE_FASTRAM && !debugger_enabled())
		{
			for (int r = 0; r < m_fastram_select; r++)
			{
				const fastram_entry &fi = m_fastram[r];
				if (fi.virtual_base || fi.excluded_count > 0)
					continue;
				if (fi.access_size != 0 && fi.access_size != 4)
					continue;

				const uml::code_label skip = NEW_SLBL();

				if (fi.start != 0x00000000)
				{
					UML_CMP(b, addr, fi.start);
					UML_JMPc(b, COND_B, skip);
				}

				UML_CMP(b, addr, fi.end - 3);
				UML_JMPc(b, COND_A, skip);

				UML_STORE(b, (uint8_t *)fi.base - fi.start, addr, uml::mem(&m_core->tw_scratch), SIZE_DWORD, SCALE_x1);
				UML_JMP(b, fr_done);
				UML_LABEL(b, skip);
			}
		}

		UML_WRITE_NOFP(b, addr, uml::mem(&m_core->tw_scratch), SIZE_DWORD, SPACE_PROGRAM);

		UML_LABEL(b, fr_done);
	};

	UML_MOV(b, phys, DRC_CR(3));
	UML_AND(b, phys, phys, 0xfffff000);
	UML_ROLINS(b, phys, uml::mem(&m_core->mem_laddr), 12, 0x00000ffc);

	UML_CMP(b, uml::mem(&m_core->tw_is4m), 0);
	UML_JMPc(b, COND_E, pde_maybe20);

	UML_CMP(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_JMPc(b, COND_E, pde_maybe20);

	UML_TEST(b, uml::mem(&m_core->tw_pde), 0x40);
	UML_JMPc(b, COND_NZ, pde_maybe20);

	ad_write(phys, &m_core->tw_pde, 0x60);
	UML_JMP(b, pde_done);

	UML_LABEL(b, pde_maybe20);

	UML_TEST(b, uml::mem(&m_core->tw_pde), 0x20);
	UML_JMPc(b, COND_NZ, pde_done);

	ad_write(phys, &m_core->tw_pde, 0x20);
	UML_LABEL(b, pde_done);

	UML_CMP(b, uml::mem(&m_core->tw_is4m), 0);
	UML_JMPc(b, COND_NE, ad_done);

	UML_MOV(b, phys, uml::mem(&m_core->tw_pde));
	UML_AND(b, phys, phys, 0xfffff000);
	UML_ROLINS(b, phys, uml::mem(&m_core->mem_laddr), 22, 0x00000ffc);

	UML_CMP(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_JMPc(b, COND_E, pte_maybe20);

	UML_TEST(b, uml::mem(&m_core->tw_pte), 0x40);
	UML_JMPc(b, COND_NZ, pte_maybe20);

	ad_write(phys, &m_core->tw_pte, 0x60);
	UML_JMP(b, ad_done);

	UML_LABEL(b, pte_maybe20);

	UML_TEST(b, uml::mem(&m_core->tw_pte), 0x20);
	UML_JMPc(b, COND_NZ, ad_done);

	ad_write(phys, &m_core->tw_pte, 0x20);
	UML_LABEL(b, ad_done);

	UML_SHR(b, phys, uml::mem(&m_core->mem_laddr), 12);
	UML_LOAD(b, uml::mem(&m_core->tw_scratch), vtlb_table(), phys, SIZE_DWORD, SCALE_x4);

	UML_TEST(b, uml::mem(&m_core->tw_scratch), FLAG_VALID);
	UML_JMPc(b, COND_NZ, dl_skip_evict);

	UML_LOAD(b, uml::mem(&m_core->tw_scratch), vtlb_dynindex(), 0, SIZE_DWORD, SCALE_x1);
	UML_LOAD(b, phys, vtlb_live(), uml::mem(&m_core->tw_scratch), SIZE_DWORD, SCALE_x4);

	UML_CMP(b, phys, 0);
	UML_JMPc(b, COND_E, dl_no_old);

	UML_SUB(b, phys, phys, 1);
	UML_STORE(b, vtlb_table(), phys, 0, SIZE_DWORD, SCALE_x4);
	UML_LABEL(b, dl_no_old);
	UML_SHR(b, phys, uml::mem(&m_core->mem_laddr), 12);
	UML_ADD(b, phys, phys, 1);
	UML_STORE(b, vtlb_live(), uml::mem(&m_core->tw_scratch), phys, SIZE_DWORD, SCALE_x4);
	UML_LABEL(b, dl_skip_evict);

	UML_LOAD(b, phys, vtlb_dynindex(), 0, SIZE_DWORD, SCALE_x1);
	UML_ADD(b, phys, phys, 1);

	UML_CMP(b, phys, vtlb_dynamic_count());
	UML_JMPc(b, COND_B, dl_di_ok);

	UML_MOV(b, phys, 0);
	UML_LABEL(b, dl_di_ok);
	UML_STORE(b, vtlb_dynindex(), 0, phys, SIZE_DWORD, SCALE_x1);

	UML_MOV(b, uml::mem(&m_core->tw_scratch), uml::mem(&m_core->mem_paddr));
	UML_AND(b, uml::mem(&m_core->tw_scratch), uml::mem(&m_core->tw_scratch), 0xfffff000);
	UML_OR(b, uml::mem(&m_core->tw_scratch), uml::mem(&m_core->tw_scratch), FLAG_VALID);
	UML_OR(b, uml::mem(&m_core->tw_scratch), uml::mem(&m_core->tw_scratch), uml::mem(&m_core->tw_perm));
	UML_SHR(b, phys, uml::mem(&m_core->mem_laddr), 12);
	UML_STORE(b, vtlb_table(), phys, uml::mem(&m_core->tw_scratch), SIZE_DWORD, SCALE_x4);

	UML_JMP(b, done);

	UML_LABEL(b, not_present);
	UML_MOV(b, uml::mem(&m_core->tlb_fault_code), 0); // P = 0 (page not present)
	UML_JMP(b, fault_finish);

	UML_LABEL(b, deny);
	UML_MOV(b, uml::mem(&m_core->tlb_fault_code), 1); // P = 1 (protection violation)

	UML_LABEL(b, fault_finish);

	UML_CMP(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_JMPc(b, COND_E, ff_no_write);

	UML_OR(b, uml::mem(&m_core->tlb_fault_code), uml::mem(&m_core->tlb_fault_code), 2);

	UML_LABEL(b, ff_no_write);
	UML_CMP(b, uml::mem(&m_core->tw_supervisor_read), 0);
	UML_JMPc(b, COND_NE, ff_no_user);

	UML_CMP(b, uml::mem(&m_core->CPL), 3);
	UML_JMPc(b, COND_NE, ff_no_user);

	UML_OR(b, uml::mem(&m_core->tlb_fault_code), uml::mem(&m_core->tlb_fault_code), 4);

	UML_LABEL(b, ff_no_user);
	UML_MOV(b, uml::mem(&m_core->tlb_miss_faulted), 1);

	UML_LABEL(b, done);
	UML_MOV(b, uml::mem(&m_core->tw_supervisor_read), 0);
	UML_RET(b);

	b.end();
}

// ----------------------------------------------------------------------------
// C++ helpers
// ----------------------------------------------------------------------------

void i386_device::allocate_memory_accessors()
{
	alloc_handle(*m_drc_uml, m_mem_read8, "mem_read8");
	alloc_handle(*m_drc_uml, m_mem_write8, "mem_write8");
	alloc_handle(*m_drc_uml, m_mem_read16, "mem_read16");
	alloc_handle(*m_drc_uml, m_mem_write16, "mem_write16");
	alloc_handle(*m_drc_uml, m_mem_read32, "mem_read32");
	alloc_handle(*m_drc_uml, m_mem_write32, "mem_write32");
	alloc_handle(*m_drc_uml, m_mem_read64, "mem_read64");
	alloc_handle(*m_drc_uml, m_mem_write64, "mem_write64");
	alloc_handle(*m_drc_uml, m_mem_read128, "mem_read128");
	alloc_handle(*m_drc_uml, m_mem_write128, "mem_write128");
}

void i386_device::drc_tlb_fault_cb()
{
	m_core->tlb_miss_faulted = 1;
	drc_catch_fault_inplace_sync(
			[&]
			{
				offs_t address = m_core->mem_laddr;
				PF_THROW(m_core->tlb_fault_code);
			}
	);
}


void *i386_device::drc_try_fastram(offs_t address, uint32_t size, bool iswrite)
{
	if (debugger_enabled())
		return nullptr;

	offs_t phys = address;

	// Translate address if in page mode
	if (PAGE_MODE_ENABLED)
	{
		if (((address & 0xfff) + size) > 0x1000)
			return nullptr;

		const vtlb_entry *table = vtlb_table();
		const vtlb_entry  entry = table[address >> 12];

		vtlb_entry need = FLAG_VALID | (iswrite ? (FLAG_DIRTY | WRITE_ALLOWED) : READ_ALLOWED);
		if (m_core->CPL == 3)
			need |= iswrite ? USER_WRITE_ALLOWED : USER_READ_ALLOWED;

		if ((entry & need) != need)
			return nullptr;

		phys = (entry & 0xfffff000) | (address & 0xfff);
	}

	phys &= m_core->a20_mask;

	for (int r = 0; r < m_fastram_select; r++)
	{
		const fastram_entry &fi = m_fastram[r];

		if (iswrite && (fi.readonly || fi.reg32_write_only))
			continue;
		if (fi.access_size != 0 && fi.access_size != size)
			continue;
		if (phys < fi.start || phys > (fi.end - (size - 1)))
			continue;

		if (fi.excluded_count > 0)
		{
			const offs_t lane     = phys & 0xfffffffc;
			bool         excluded = false;
			for (int i = 0; i < fi.excluded_count; i++)
			{
				if (lane == fi.excluded[i])
				{
					excluded = true;
					break;
				}
			}
			if (excluded)
				continue;
		}

		return (uint8_t *)fi.base + (phys - fi.start);
	}

	return nullptr;
}

inline uint8_t i386_device::drc_fetch8(offs_t &cursor)
{
	if (void *p = drc_try_fastram(cursor, 1, false))
	{
		cursor++;
		return *(uint8_t *)p;
	}
	else
	{
		offs_t   address = cursor;
		uint32_t err;
		if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
			PF_THROW(err);
		cursor++;
		return macache32.read_byte(address & m_core->a20_mask);
	}
}


inline uint8_t i386_device::drc_peek8(offs_t cursor)
{
	if (void *p = drc_try_fastram(cursor, 1, false))
	{
		return *(uint8_t *)p;
	}
	else
	{
		offs_t   address = cursor;
		uint32_t err;
		if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
			PF_THROW(err);
		return macache32.read_byte(address & m_core->a20_mask);
	}
}

// Used instead of FETCH() (added fastram read)
// Does PC and EIP advance for the interpreter
inline uint8_t i386_device::drc_fetch8inter()
{
	offs_t cursor = m_core->pc;

	const uint8_t value = drc_fetch8(cursor);

	m_core->pc++;
	m_core->eip++;

	return value;
}


inline uint16_t i386_device::drc_fetch16(offs_t &cursor)
{
	if (WORD_ALIGNED(cursor))
	{
		if (void *p = drc_try_fastram(cursor, 2, false))
		{
			cursor += 2;
			return *(uint16_t *)p;
		}
		else
		{
			offs_t   address = cursor;
			uint32_t err;
			if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
				PF_THROW(err);
			cursor += 2;
			return macache32.read_word(address & m_core->a20_mask);
		}
	}
	else
	{
		uint16_t v = (uint16_t)drc_fetch8(cursor);
		v |= (uint16_t)drc_fetch8(cursor) << 8;
		return v;
	}
}


inline uint16_t i386_device::drc_peek16(offs_t cursor)
{
	if (WORD_ALIGNED(cursor))
	{
		if (void *p = drc_try_fastram(cursor, 2, false))
		{
			return *(uint16_t *)p;
		}
		else
		{
			offs_t   address = cursor;
			uint32_t err;
			if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
				PF_THROW(err);
			return macache32.read_word(address & m_core->a20_mask);
		}
	}
	else
	{
		uint16_t v = (uint16_t)drc_peek8(cursor);
		v |= (uint16_t)drc_peek8(cursor + 1) << 8;
		return v;
	}
}


inline uint32_t i386_device::drc_fetch32(offs_t &cursor)
{
	if (DWORD_ALIGNED(cursor))
	{
		if (void *p = drc_try_fastram(cursor, 4, false))
		{
			cursor += 4;
			return *(uint32_t *)p;
		}
		else
		{
			offs_t   address = cursor;
			uint32_t err;
			if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
				PF_THROW(err);
			cursor += 4;
			return macache32.read_dword(address & m_core->a20_mask);
		}
	}
	else
	{
		uint32_t v = (uint32_t)drc_fetch8(cursor);
		v |= (uint32_t)drc_fetch8(cursor) << 8;
		v |= (uint32_t)drc_fetch8(cursor) << 16;
		v |= (uint32_t)drc_fetch8(cursor) << 24;
		return v;
	}
}


inline uint32_t i386_device::drc_peek32(offs_t cursor)
{
	if (DWORD_ALIGNED(cursor))
	{
		if (void *p = drc_try_fastram(cursor, 4, false))
		{
			return *(uint32_t *)p;
		}
		else
		{
			offs_t   address = cursor;
			uint32_t err;
			if (!translate_address(m_core->CPL, TR_FETCH, &address, &err))
				PF_THROW(err);
			return macache32.read_dword(address & m_core->a20_mask);
		}
	}
	else
	{
		uint32_t v = (uint32_t)drc_peek8(cursor);
		v |= (uint32_t)drc_peek8(cursor + 1) << 8;
		v |= (uint32_t)drc_peek8(cursor + 2) << 16;
		v |= (uint32_t)drc_peek8(cursor + 3) << 24;
		return v;
	}
}

uint32_t i386_device::drc_peek32phys(offs_t physical_address)
{
	if (!debugger_enabled())
	{
		for (int r = 0; r < m_fastram_select; r++)
		{
			const fastram_entry &fi = m_fastram[r];
			if (fi.virtual_base || fi.excluded_count > 0)
				continue;
			if (fi.access_size != 0 && fi.access_size != 4)
				continue;
			if (physical_address < fi.start || physical_address > (fi.end - 3))
				continue;

			return *(const uint32_t *)((const uint8_t *)fi.base + (physical_address - fi.start));
		}
	}

	return m_program->read_dword(physical_address);
}

uint32_t i386_device::drc_resolve_tlb_page(offs_t address)
{
	if (!PAGE_MODE_ENABLED)
		return address & 0xfffff000;

	const offs_t pdbr = m_core->cr[3] & 0xfffff000;

	const uint32_t pde = drc_peek32phys(pdbr + ((address >> 22) & 0x3ff) * 4);
	if (!(pde & 1))
		return 0xffffffff;

	if ((pde & 0x80) && (m_core->cr[4] & CR4_PSE))
		return (pde & 0xffc00000) | (address & 0x003ff000);

	const uint32_t pte = drc_peek32phys((pde & 0xfffff000) + ((address >> 12) & 0x3ff) * 4);
	if (!(pte & 1))
		return 0xffffffff;

	return pte & 0xfffff000;
}

// ----------------------------------------------------------------------------
// UML Helpers
// ----------------------------------------------------------------------------

void i386_device::drc_gen_inline_fastram32(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, const uml::code_label done, bool iswrite)
{
	if (debugger_enabled())
		return;

	const uml::parameter &addr_scratch = I3;

	for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
	{
		fastram_entry &fi = m_fastram[ramnum];
		if (iswrite && fi.readonly)
			continue;

		if (fi.access_size != 0 && fi.access_size != 4)
			continue;

		const uml::code_label skip = NEW_SLBL();

		const bool need_lower = (fi.start != 0x00000000);

		if (need_lower)
		{
			UML_SUB(b, addr_scratch, addr, fi.start);

			UML_CMP(b, addr_scratch, (fi.end - 3) - fi.start);
			UML_JMPc(b, COND_A, skip);
		}
		else
		{
			UML_CMP(b, addr, fi.end - 3);
			UML_JMPc(b, COND_A, skip);
		}

		if (fi.excluded_count > 0)
		{
			UML_AND(b, addr_scratch, addr, 0xfffffffc);
			for (int i = 0; i < fi.excluded_count; i++)
			{
				UML_CMP(b, addr_scratch, fi.excluded[i]);
				UML_JMPc(b, COND_E, skip);
			}
		}

		uml::parameter region_index = addr;

		void *region_base;
		if (fi.virtual_base)
		{
			UML_SUB(b, addr_scratch, addr, fi.start);
			region_base  = fi.base;
			region_index = addr_scratch;
		}
		else
		{
			region_base = (uint8_t *)fi.base - fi.start;
		}

		if (iswrite)
			UML_STORE(b, region_base, region_index, data, SIZE_DWORD, SCALE_x1);
		else
			UML_LOAD(b, data, region_base, region_index, SIZE_DWORD, SCALE_x1);

		UML_JMP(b, done);

		UML_LABEL(b, skip);
	}
}

void i386_device::drc_gen_privileged_read32(drcuml_block &b, int &label_ctr, const uml::parameter dest, const uml::parameter addr, const uml::parameter scratch, const uml::code_label take_slow)
{
	if (!PAGE_MODE_ENABLED)
	{
		UML_MOV(b, dest, addr);
		if (!(m_drc_options & I386DRC_SKIP_A20MASK))
			UML_AND(b, dest, dest, uml::mem(&m_core->a20_mask));
	}
	else
	{
		UML_SHR(b, scratch, addr, 12);
		UML_LOAD(b, scratch, vtlb_table(), scratch, SIZE_DWORD, SCALE_x4);
		UML_AND(b, dest, scratch, FLAG_VALID | READ_ALLOWED);

		UML_CMP(b, dest, FLAG_VALID | READ_ALLOWED);
		UML_JMPc(b, COND_NE, take_slow);

		UML_MOV(b, dest, addr);
		UML_ROLINS(b, dest, scratch, 0, 0xfffff000);
		if (!(m_drc_options & I386DRC_SKIP_A20MASK))
			UML_AND(b, dest, dest, uml::mem(&m_core->a20_mask));
	}

	if (m_drc_options & I386DRC_INLINE_FASTRAM)
	{
		const uml::code_label fastram_done = NEW_SLBL();
		drc_gen_inline_fastram32(b, label_ctr, dest, dest, fastram_done, false);
		UML_READ_NOFP(b, dest, dest, SIZE_DWORD, SPACE_PROGRAM);
		UML_LABEL(b, fastram_done);
	}
	else
	{
		UML_READ_NOFP(b, dest, dest, SIZE_DWORD, SPACE_PROGRAM);
	}
}

void i386_device::drc_gen_privileged_walk(drcuml_block &b, int &label_ctr, const uml::parameter address, const uml::parameter phys_out, const uml::code_label take_slow)
{
	const uml::parameter &walk_scratch = I3;

	const uml::code_label nonpaged = NEW_SLBL();
	const uml::code_label warm     = NEW_SLBL();
	const uml::code_label done     = NEW_SLBL();

	UML_TEST(b, DRC_CR(0), CR0_PG);
	UML_JMPc(b, COND_Z, nonpaged);

	UML_SHR(b, walk_scratch, address, 12);
	UML_LOAD(b, walk_scratch, vtlb_table(), walk_scratch, SIZE_DWORD, SCALE_x4);
	UML_AND(b, phys_out, walk_scratch, FLAG_VALID | READ_ALLOWED);

	UML_CMP(b, phys_out, FLAG_VALID | READ_ALLOWED);
	UML_JMPc(b, COND_E, warm);

	UML_MOV(b, uml::mem(&m_core->mem_laddr), address);
	UML_MOV(b, uml::mem(&m_core->mem_iswrite), 0);
	UML_MOV(b, uml::mem(&m_core->tw_supervisor_read), 1);
	UML_MOV(b, uml::mem(&m_core->tlb_miss_faulted), 0);

	UML_CALLH(b, *m_resolve_tlb);

	UML_CMP(b, uml::mem(&m_core->tlb_miss_faulted), 0);
	UML_JMPc(b, COND_NE, take_slow);

	UML_MOV(b, phys_out, uml::mem(&m_core->mem_paddr));
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_out, phys_out, uml::mem(&m_core->a20_mask));

	UML_JMP(b, done);

	UML_LABEL(b, warm);
	UML_MOV(b, phys_out, address);
	UML_ROLINS(b, phys_out, walk_scratch, 0, 0xfffff000);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_out, phys_out, uml::mem(&m_core->a20_mask));

	UML_JMP(b, done);

	UML_LABEL(b, nonpaged);
	UML_MOV(b, phys_out, address);
	if (!(m_drc_options & I386DRC_SKIP_A20MASK))
		UML_AND(b, phys_out, phys_out, uml::mem(&m_core->a20_mask));

	UML_LABEL(b, done);
}

// Similar to drc_gen_privileged_read32 but does a full page table walk via m_resolve_tlb when the vtlb_table() entry is invalid
void i386_device::drc_gen_privileged_read32walk(drcuml_block &b, int &label_ctr, const uml::parameter dest, const uml::parameter address, const uml::code_label take_slow)
{
	drc_gen_privileged_walk(b, label_ctr, address, dest, take_slow);

	UML_READ_NOFP(b, dest, dest, SIZE_DWORD, SPACE_PROGRAM);
}

void i386_device::drc_gen_fastram_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite)
{
	if (debugger_enabled())
		return;

	const uml::parameter &index_scratch = I3;
	const uml::parameter &lane_scratch  = I4;
	const uml::parameter &vdata         = F0;

	for (int ramnum = 0; ramnum < m_fastram_select; ramnum++)
	{
		fastram_entry &fi = m_fastram[ramnum];
		if (iswrite && fi.readonly)
			continue;

		if (fi.access_size != 0 && fi.access_size != size)
			continue;

		const uml::code_label skip = NEW_SLBL();

		const bool need_upper = (fi.end != 0xffffffff || size > 1);
		const bool need_lower = (fi.start != 0x00000000);

		if (need_upper && need_lower)
		{
			UML_SUB(b, index_scratch, addr, fi.start);

			UML_CMP(b, index_scratch, (fi.end - (size - 1)) - fi.start);
			UML_JMPc(b, COND_A, skip);
		}
		else if (need_upper)
		{
			UML_CMP(b, addr, fi.end - (size - 1));
			UML_JMPc(b, COND_A, skip);
		}
		else if (need_lower)
		{
			UML_CMP(b, addr, fi.start);
			UML_JMPc(b, COND_B, skip);
		}

		uml::parameter region_index = addr;

		void *region_base;
		if (fi.virtual_base)
		{
			UML_SUB(b, index_scratch, addr, fi.start);
			region_base  = fi.base;
			region_index = index_scratch;
		}
		else
		{
			region_base = (uint8_t *)fi.base - fi.start;
		}

		if (fi.excluded_count > 0)
		{
			UML_AND(b, lane_scratch, addr, 0xfffffffc);
			for (int i = 0; i < fi.excluded_count; i++)
			{
				UML_CMP(b, lane_scratch, fi.excluded[i]);
				UML_JMPc(b, COND_E, skip);
			}
		}

		if (DEBUG_MEMORY_ACCESS)
		{
			UML_DMOV(b, mem(&m_core->mem_diag_addr), addr);
			UML_MOV(b, mem(&m_core->mem_diag_is_write), iswrite);
			UML_CALLC(b, I386_CB(func_ramlog_epoch), this);
		}

		switch (size)
		{
			case 1:
			{
				if (iswrite && fi.reg32_write_only)
				{
					UML_AND(b, data, data, 0xff);
					UML_AND(b, lane_scratch, addr, 3);
					UML_SHL(b, lane_scratch, lane_scratch, 3);
					UML_SHL(b, data, data, lane_scratch);
					UML_AND(b, index_scratch, region_index, 0xfffffffc);
					UML_STORE(b, region_base, index_scratch, data, SIZE_DWORD, SCALE_x1);
				}
				else if (iswrite)
				{
					UML_STORE(b, region_base, region_index, data, SIZE_BYTE, SCALE_x1);
				}
				else
				{
					UML_LOAD(b, data, region_base, region_index, SIZE_BYTE, SCALE_x1);
				}
				break;
			}

			case 2:
			{
				if (iswrite && fi.reg32_write_only)
				{
					UML_AND(b, lane_scratch, addr, 3);

					UML_CMP(b, lane_scratch, 3);
					UML_JMPc(b, COND_E, skip);

					UML_AND(b, data, data, 0xffff);
					UML_SHL(b, lane_scratch, lane_scratch, 3);
					UML_SHL(b, data, data, lane_scratch);
					UML_AND(b, index_scratch, region_index, 0xfffffffc);
					UML_STORE(b, region_base, index_scratch, data, SIZE_DWORD, SCALE_x1);
				}
				else if (iswrite)
				{
					UML_STORE(b, region_base, region_index, data, SIZE_WORD, SCALE_x1);
				}
				else
				{
					UML_LOAD(b, data, region_base, region_index, SIZE_WORD, SCALE_x1);
				}
				break;
			}

			case 4:
			{
				if (iswrite)
					UML_STORE(b, region_base, region_index, data, SIZE_DWORD, SCALE_x1);
				else
					UML_LOAD(b, data, region_base, region_index, SIZE_DWORD, SCALE_x1);
				break;
			}

			case 8:
			{
				if (iswrite)
					UML_DSTORE(b, region_base, region_index, data, SIZE_QWORD, SCALE_x1);
				else
					UML_DLOAD(b, data, region_base, region_index, SIZE_QWORD, SCALE_x1);
				break;
			}

			case 16:
			{
				if (iswrite)
				{
					UML_VLOAD(b, vdata, &m_core->data128[0], 0);
					UML_VSTORE(b, region_base, region_index, vdata);
				}
				else
				{
					UML_VLOAD(b, vdata, region_base, region_index);
					UML_VSTORE(b, &m_core->data128[0], 0, vdata);
				}
				break;
			}
		}

		if (DEBUG_MEMORY_ACCESS)
			UML_CALLC(b, I386_CB(func_log_fastram), this);

		UML_RET(b);

		UML_LABEL(b, skip);
	}
}

void i386_device::drc_gen_fastpaged_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite)
{
	const uml::parameter &addr_scratch  = I3;
	const uml::parameter &lane_scratch  = I4;
	const uml::parameter &window_offset = I8;
	const uml::parameter &walk_scratch  = I9;

	if (size == 8 || size == 16)
		return;

	for (int pagednum = 0; pagednum < m_fastpaged_select; pagednum++)
	{
		fastpaged_entry &fi = m_fastpaged[pagednum];
		if (fi.access_size != 0 && fi.access_size != size)
			continue;

		const uml::code_label skip = NEW_SLBL();

		const bool need_upper = (fi.end != 0xffffffff || size > 1);
		const bool need_lower = (fi.start != 0x00000000);

		if (need_upper && need_lower)
		{
			UML_SUB(b, addr_scratch, addr, fi.start);

			UML_CMP(b, addr_scratch, (fi.end - (size - 1)) - fi.start);
			UML_JMPc(b, COND_A, skip);
		}
		else if (need_upper)
		{
			UML_CMP(b, addr, fi.end - (size - 1));
			UML_JMPc(b, COND_A, skip);
		}
		else if (need_lower)
		{
			UML_CMP(b, addr, fi.start);
			UML_JMPc(b, COND_B, skip);
		}

		if (size == 2)
		{
			UML_AND(b, lane_scratch, addr, 3);

			UML_CMP(b, lane_scratch, 3);
			UML_JMPc(b, COND_E, skip);
		}

		UML_SUB(b, window_offset, addr, fi.start);
		UML_SHR(b, window_offset, window_offset, 2);

		UML_SHR(b, walk_scratch, window_offset, fi.page_shift);
		UML_ADD(b, walk_scratch, walk_scratch, fi.page_index_base);
		UML_AND(b, walk_scratch, walk_scratch, fi.page_table_mask);
		UML_LOAD(b, walk_scratch, fi.page_table, walk_scratch, SIZE_DWORD, SCALE_x4);

		UML_AND(b, addr_scratch, walk_scratch, fi.page_valid_bit);

		UML_CMP(b, addr_scratch, fi.page_valid_bit);
		UML_JMPc(b, COND_NE, skip);

		UML_CMP(b, walk_scratch, fi.ram_limit);
		UML_JMPc(b, COND_AE, skip);

		UML_SHR(b, walk_scratch, walk_scratch, 2);
		UML_AND(b, addr_scratch, window_offset, fi.page_offset_mask);
		UML_ADD(b, walk_scratch, walk_scratch, addr_scratch);

		switch (size)
		{
			case 1:
			case 2:
			{
				UML_AND(b, lane_scratch, addr, 3);
				if (iswrite)
				{
					UML_SHL(b, lane_scratch, lane_scratch, 3);
					UML_AND(b, addr_scratch, data, size == 1 ? 0xff : 0xffff);
					UML_SHL(b, addr_scratch, addr_scratch, lane_scratch);
					UML_STORE(b, fi.ram_base, walk_scratch, addr_scratch, SIZE_DWORD, SCALE_x4);
				}
				else
				{
					UML_SHL(b, addr_scratch, walk_scratch, 2);
					UML_ADD(b, addr_scratch, addr_scratch, lane_scratch);
					UML_LOAD(b, data, fi.ram_base, addr_scratch, size == 1 ? SIZE_BYTE : SIZE_WORD, SCALE_x1);
				}
				break;
			}

			case 4:
			{
				if (iswrite)
					UML_STORE(b, fi.ram_base, walk_scratch, data, SIZE_DWORD, SCALE_x4);
				else
					UML_LOAD(b, data, fi.ram_base, walk_scratch, SIZE_DWORD, SCALE_x4);
				break;
			}
		}

		UML_RET(b);

		UML_LABEL(b, skip);
	}
}

void i386_device::drc_gen_addrspace_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite)
{
	const uml::parameter &scratch0 = I3;
	const uml::parameter &scratch1 = I4;

	const uml::code_label done = NEW_SLBL();

	if (DEBUG_MEMORY_ACCESS)
	{
		UML_DMOV(b, mem(&m_core->mem_diag_addr), addr);
		UML_MOV(b, mem(&m_core->mem_diag_is_write), iswrite);
		UML_CALLC(b, I386_CB(func_ramlog_epoch), this);
	}

	switch (size)
	{
		case 1:
		{
			if (iswrite)
				UML_WRITE_NOFP(b, addr, data, SIZE_BYTE, SPACE_PROGRAM);
			else
				UML_READ_NOFP(b, data, addr, SIZE_BYTE, SPACE_PROGRAM);
			break;
		}

		case 2:
		{
			const uml::code_label aligned = NEW_SLBL();

			UML_TEST(b, addr, 1);
			UML_JMPc(b, COND_Z, aligned);

			// address isn't 2-byte aligned, it's one off
			if (iswrite)
			{
				UML_WRITE_NOFP(b, addr, data, SIZE_BYTE, SPACE_PROGRAM);
				UML_SHR(b, scratch0, data, 8);
				UML_ADD(b, addr, addr, 1);
				UML_WRITE_NOFP(b, addr, scratch0, SIZE_BYTE, SPACE_PROGRAM);
			}
			else
			{
				UML_READ_NOFP(b, data, addr, SIZE_BYTE, SPACE_PROGRAM);
				UML_ADD(b, addr, addr, 1);
				UML_READ_NOFP(b, scratch0, addr, SIZE_BYTE, SPACE_PROGRAM);
				UML_SHL(b, scratch0, scratch0, 8);
				UML_OR(b, data, data, scratch0);
			}
			UML_JMP(b, done);

			// address is 2-bute aligned, simple read or write
			UML_LABEL(b, aligned);
			if (iswrite)
				UML_WRITE_NOFP(b, addr, data, SIZE_WORD, SPACE_PROGRAM);
			else
				UML_READ_NOFP(b, data, addr, SIZE_WORD, SPACE_PROGRAM);
			break;
		}

		case 4:
		{
			const uml::code_label aligned   = NEW_SLBL();
			const uml::code_label case_2off = NEW_SLBL();
			const uml::code_label case_1off = NEW_SLBL();

			UML_TEST(b, addr, 3);
			UML_JMPc(b, COND_Z, aligned);

			UML_TEST(b, addr, 2);
			UML_JMPc(b, COND_Z, case_1off);

			UML_TEST(b, addr, 1);
			UML_JMPc(b, COND_Z, case_2off);

			// address isn't 4-byte aligned, it's three off
			if (iswrite)
			{
				UML_WRITE_NOFP(b, addr, data, SIZE_BYTE, SPACE_PROGRAM);
				UML_SHR(b, scratch0, data, 8);
				UML_ADD(b, addr, addr, 1);
				UML_WRITEM_NOFP(b, addr, scratch0, 0x00ffffff, SIZE_DWORD, SPACE_PROGRAM);
			}
			else
			{
				UML_READ_NOFP(b, data, addr, SIZE_BYTE, SPACE_PROGRAM);
				UML_ADD(b, addr, addr, 1);
				UML_READM_NOFP(b, scratch0, addr, 0x00ffffff, SIZE_DWORD, SPACE_PROGRAM);
				UML_SHL(b, scratch0, scratch0, 8);
				UML_OR(b, data, data, scratch0);
			}
			UML_JMP(b, done);

			// address isn't 4-byte aligned, it's two off
			UML_LABEL(b, case_1off);
			if (iswrite)
			{
				UML_SUB(b, scratch1, addr, 1);
				UML_SHL(b, scratch0, data, 8);
				UML_WRITEM_NOFP(b, scratch1, scratch0, 0xffffff00, SIZE_DWORD, SPACE_PROGRAM);
				UML_ADD(b, scratch1, addr, 3);
				UML_SHR(b, scratch0, data, 24);
				UML_WRITE_NOFP(b, scratch1, scratch0, SIZE_BYTE, SPACE_PROGRAM);
			}
			else
			{
				UML_SUB(b, scratch1, addr, 1);
				UML_READM_NOFP(b, data, scratch1, 0xffffff00, SIZE_DWORD, SPACE_PROGRAM);
				UML_SHR(b, data, data, 8);
				UML_ADD(b, scratch1, addr, 3);
				UML_READ_NOFP(b, scratch0, scratch1, SIZE_BYTE, SPACE_PROGRAM);
				UML_SHL(b, scratch0, scratch0, 24);
				UML_OR(b, data, data, scratch0);
			}
			UML_JMP(b, done);

			// address isn't 4-byte aligned, it's one off
			UML_LABEL(b, case_2off);
			if (iswrite)
			{
				UML_WRITE_NOFP(b, addr, data, SIZE_WORD, SPACE_PROGRAM);
				UML_SHR(b, scratch0, data, 16);
				UML_ADD(b, addr, addr, 2);
				UML_WRITE_NOFP(b, addr, scratch0, SIZE_WORD, SPACE_PROGRAM);
			}
			else
			{
				UML_READ_NOFP(b, data, addr, SIZE_WORD, SPACE_PROGRAM);
				UML_ADD(b, addr, addr, 2);
				UML_READ_NOFP(b, scratch0, addr, SIZE_WORD, SPACE_PROGRAM);
				UML_SHL(b, scratch0, scratch0, 16);
				UML_OR(b, data, data, scratch0);
			}
			UML_JMP(b, done);

			// address is 4-byte aligned, simple read or write
			UML_LABEL(b, aligned);

			if (iswrite)
				UML_WRITE_NOFP(b, addr, data, SIZE_DWORD, SPACE_PROGRAM);
			else
				UML_READ_NOFP(b, data, addr, SIZE_DWORD, SPACE_PROGRAM);
			break;
		}

		case 8:
		{
			const uml::code_label aligned = NEW_SLBL();

			UML_TEST(b, addr, 7);
			UML_JMPc(b, COND_Z, aligned);

			// address isn't 8-byte aligned, will always read 1 byte at a time to reduce code complexity
			for (int i = 0; i < 8; i++)
			{
				uml::parameter cur_addr = addr;

				if (i > 0)
				{
					UML_ADD(b, scratch0, cur_addr, i);
					cur_addr = scratch0;
				}

				if (iswrite)
				{
					if (i == 0)
					{
						UML_WRITE_NOFP(b, cur_addr, data, SIZE_BYTE, SPACE_PROGRAM);
					}
					else
					{
						UML_DSHR(b, scratch1, data, i * 8);
						UML_WRITE_NOFP(b, cur_addr, scratch1, SIZE_BYTE, SPACE_PROGRAM);
					}
				}
				else
				{
					UML_READ_NOFP(b, scratch1, cur_addr, SIZE_BYTE, SPACE_PROGRAM);
					if (i == 0)
					{
						UML_DMOV(b, data, scratch1);
					}
					else
					{
						UML_DSHL(b, scratch1, scratch1, i * 8);
						UML_DOR(b, data, data, scratch1);
					}
				}
			}

			// address is 8-byte aligned, simple read or write
			UML_LABEL(b, aligned);
			if (iswrite)
				UML_DWRITE(b, addr, data, SIZE_QWORD, SPACE_PROGRAM);
			else
				UML_DREAD(b, data, addr, SIZE_QWORD, SPACE_PROGRAM);
			break;
		}

		case 16:
		{
			for (int i = 0; i < 16; i++)
			{
				uml::parameter cur_addr = addr;

				if (i > 0)
				{
					UML_ADD(b, scratch0, cur_addr, i);
					cur_addr = scratch0;
				}

				if (iswrite)
					UML_WRITE_NOFP(b, cur_addr, DRC_SCR128(i), SIZE_BYTE, SPACE_PROGRAM);
				else
					UML_READ_NOFP(b, DRC_SCR128(i), cur_addr, SIZE_BYTE, SPACE_PROGRAM);
			}
			break;
		}

		default:
			break;
	}

	UML_LABEL(b, done);

	if (DEBUG_MEMORY_ACCESS)
		UML_CALLC(b, I386_CB(func_log_slowram), this);

	UML_RET(b);
}

void i386_device::drc_gen_cross_page_memory_access(drcuml_block &b, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite)
{
	const uml::parameter &addr_scratch = I8;
	const uml::parameter &data_scratch = I9;

	UML_MOV(b, addr_scratch, addr);

	switch (size)
	{
		case 16:
		{
			if (iswrite)
			{
				for (int i = 0; i < 16; i++)
				{
					if (i > 0)
						UML_ADD(b, addr, addr_scratch, i);
					UML_MOV(b, data, DRC_SCR128(i));
					UML_CALLH(b, *m_mem_write8);
				}
			}
			else
			{
				for (int i = 0; i < 16; i++)
				{
					if (i > 0)
						UML_ADD(b, addr, addr_scratch, i);
					UML_CALLH(b, *m_mem_read8);
					UML_MOV(b, DRC_SCR128(i), data);
				}
			}
			break;
		}

		case 2:
		case 4:
		case 8:
		default:
		{
			if (iswrite)
			{
				UML_DMOV(b, data_scratch, data);
				for (int i = 0; i < size; i++)
				{
					if (i > 0)
					{
						UML_ADD(b, addr, addr_scratch, i);
						UML_DSHR(b, data, data_scratch, i * 8);
					}
					UML_CALLH(b, *m_mem_write8);
				}
			}
			else
			{
				for (int i = 0; i < size; i++)
				{
					if (i > 0)
						UML_ADD(b, addr, addr_scratch, i);
					UML_CALLH(b, *m_mem_read8);
					if (i == 0)
					{
						UML_DMOV(b, data_scratch, data);
					}
					else
					{
						UML_DSHL(b, data, data, i * 8);
						UML_DOR(b, data_scratch, data_scratch, data);
					}
				}

				UML_DMOV(b, data, data_scratch);
			}
			break;
		}
	}

	UML_RET(b);
}

// ----------------------------------------------------------------------------
// MOV
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_mov_al_m8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;

	uint32_t off = ctx.desc->addr32 ? drc_get_imm32(ctx) : drc_get_imm16(ctx);
	int      seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_MOV(b, mem_addr, off);
	UML_ADD(b, mem_addr, mem_addr, DRC_SEG(seg, base));
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read8);
	UML_AND(b, mem_addr, mem_data, 0xff);
	UML_BREG_WRITE(b, AL, mem_addr);

	drc_record_cycles(ctx, CYCLES_MOV_MEM_ACC);

	return true;
}

bool i386_device::drc_pri_mov_m8_al(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;

	uint32_t off = ctx.desc->addr32 ? drc_get_imm32(ctx) : drc_get_imm16(ctx);
	int      seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_MOV(b, mem_addr, off);
	UML_ADD(b, mem_addr, mem_addr, DRC_SEG(seg, base));
	UML_AND(b, mem_data, DRC_REG8(AL), 0xff);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_write8);

	drc_record_cycles(ctx, CYCLES_MOV_ACC_MEM);

	return true;
}

bool i386_device::drc_pri_mov_acc_moffs(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;

	uint32_t off = (ctx.desc->addr32) ? drc_get_imm32(ctx) : drc_get_imm16(ctx);
	int      seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_MOV(b, mem_addr, off);
	UML_ADD(b, mem_addr, mem_addr, DRC_SEG(seg, base));
	// 0xa1 MOV eAX,moffs (load); else reached via 0xa3 MOV moffs,eAX (store)
	if (ctx.desc->opcode0 == 0xa1)
	{
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, DRC_REG32(EAX), mem_data);

		drc_record_cycles(ctx, CYCLES_MOV_MEM_ACC);
	}
	else
	{
		UML_MOV(b, mem_data, DRC_REG32(EAX));
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);

		drc_record_cycles(ctx, CYCLES_MOV_ACC_MEM);
	}

	return true;
}

bool i386_device::drc_pri_mov_acc_moffs16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;

	uint32_t off = (ctx.desc->addr32) ? drc_get_imm32(ctx) : drc_get_imm16(ctx);
	int      seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_MOV(b, mem_addr, off);
	UML_ADD(b, mem_addr, mem_addr, DRC_SEG(seg, base));
	UML_AND(b, mem_data, DRC_REG32(EAX), 0xffff);
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_write16);

	drc_record_cycles(ctx, CYCLES_MOV_ACC_MEM);

	return true;
}

bool i386_device::drc_pri_mov8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &mem_data     = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_b = MRM_REG8(modrm);
	bool    mod3  = MRM_MOD(modrm) == 3;

	// 0x8a MOV r8, r/m8; 0x88 MOV r/m8, r8
	if (ctx.desc->opcode0 == 0x8a)
	{
		drc_gen_rm8(ctx, modrm);
		UML_AND(b, loaded_value, loaded_value, 0xff);
		UML_BREG_WRITE(b, reg_b, loaded_value);

		drc_record_cycles(ctx, mod3 ? CYCLES_MOV_REG_REG : CYCLES_MOV_REG_MEM);
	}
	else
	{
		if (!ctx.desc->is_mem)
		{
			UML_AND(b, loaded_value, DRC_REG8(reg_b), 0xff);
			UML_BREG_WRITE(b, MRM_RM8(modrm), loaded_value);

			drc_record_cycles(ctx, CYCLES_MOV_REG_REG);
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_AND(b, mem_data, DRC_REG8(reg_b), 0xff);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write8);

			drc_record_cycles(ctx, CYCLES_MOV_MEM_REG);
		}
	}

	return true;
}

bool i386_device::drc_pri_mov16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data     = I2;
	const uml::parameter &loaded_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);
	// 0x89 MOV r/m16, r16; 0x8b MOV r16, r/m16
	bool to_rm    = (ctx.desc->opcode0 == 0x89);

	if (to_rm)
	{
		if (!ctx.desc->is_mem)
		{
			UML_ROLINS(b, DRC_REG32(MRM_RM32(modrm)), DRC_REG32(reg_d), 0, 0xffff);
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_AND(b, mem_data, DRC_REG32(reg_d), 0xffff);
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write16);
		}
	}
	else
	{
		drc_gen_rm16(ctx, modrm);
		UML_ROLINS(b, DRC_REG32(reg_d), loaded_value, 0, 0xffff);
	}

	drc_record_cycles(ctx, MRM_MOD(modrm) == 3 ? CYCLES_MOV_REG_REG : CYCLES_MOV_REG_MEM);

	return true;
}

bool i386_device::drc_pri_mov32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &mem_data     = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_d = MRM_REG32(modrm);
	bool    mod3  = MRM_MOD(modrm) == 3;

	// 0x8b MOV r32, r/m32; 0x89 MOV r/m32, r32
	if (ctx.desc->opcode0 == 0x8b)
	{
		drc_gen_rm32(ctx, modrm);
		UML_MOV(b, DRC_REG32(reg_d), loaded_value);

		drc_record_cycles(ctx, mod3 ? CYCLES_MOV_REG_REG : CYCLES_MOV_REG_MEM);
	}
	else
	{
		if (!ctx.desc->is_mem)
		{
			UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), DRC_REG32(reg_d));

			drc_record_cycles(ctx, CYCLES_MOV_REG_REG);
		}
		else
		{
			(this->*ctx.gen_ea)(ctx);

			UML_MOV(b, mem_data, DRC_REG32(reg_d));
			drc_flush_cycles(ctx);
			UML_CALLH(b, *m_mem_write32);

			drc_record_cycles(ctx, CYCLES_MOV_MEM_REG);
		}
	}

	return true;
}

bool i386_device::drc_pri_mov_r8_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t imm  = drc_get_imm8(ctx);
	int     breg = OP_RM8(ctx.desc->opcode0);

	UML_BREG_WRITE(b, breg, (uint32_t)imm);

	drc_record_cycles(ctx, CYCLES_MOV_IMM_REG);

	return true;
}

bool i386_device::drc_pri_mov_r16_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint16_t imm = drc_get_imm16(ctx);

	UML_ROLINS(b, DRC_GET_OP_RM32(ctx.desc->opcode0), imm, 0, 0x0000ffff);

	drc_record_cycles(ctx, CYCLES_MOV_IMM_REG);

	return true;
}

bool i386_device::drc_pri_mov_r32_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint32_t imm  = drc_get_imm32(ctx);
	int      dreg = OP_RM32(ctx.desc->opcode0);

	UML_MOV(b, DRC_REG32(dreg), imm);

	ctx.set_rscratch(dreg, imm);

	drc_record_cycles(ctx, CYCLES_MOV_IMM_REG);

	return true;
}

bool i386_device::drc_pri_mov_rm8_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_OPCODE(modrm) != 0)
		return false;

	bool is_m = ctx.desc->is_mem;
	if (is_m)
		(this->*ctx.gen_ea)(ctx);
	uint32_t imm = (uint32_t)drc_get_imm8(ctx);

	if (!is_m)
	{
		UML_BREG_WRITE(b, MRM_RM8(modrm), imm);
	}
	else
	{
		const uml::parameter &mem_data = I2;

		UML_MOV(b, mem_data, imm);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write8);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_MOV_REG_MEM : CYCLES_MOV_REG_REG);

	return true;
}

bool i386_device::drc_pri_mov_rm16_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_data = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_OPCODE(modrm) != 0)
		return false;

	bool is_m = ctx.desc->is_mem;
	if (is_m)
		(this->*ctx.gen_ea)(ctx);
	uint32_t imm = (uint32_t)drc_get_imm16(ctx) & 0xffff;

	if (!is_m)
	{
		UML_ROLINS(b, DRC_REG32(MRM_RM32(modrm)), imm, 0, 0xffff);
	}
	else
	{
		UML_MOV(b, mem_data, imm);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);
	}

	drc_record_cycles(ctx, CYCLES_MOV_IMM_MEM);

	return true;
}

bool i386_device::drc_pri_mov_rm32_imm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_OPCODE(modrm) != 0)
		return false;

	bool is_m = ctx.desc->is_mem;
	if (is_m)
		(this->*ctx.gen_ea)(ctx);
	uint32_t imm = drc_get_imm32(ctx);
	if (!is_m)
	{
		UML_MOV(b, DRC_REG32(MRM_RM32(modrm)), imm);

		drc_record_cycles(ctx, CYCLES_MOV_IMM_REG);
	}
	else
	{
		const uml::parameter &mem_data = I2;

		UML_MOV(b, mem_data, imm);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write32);

		drc_record_cycles(ctx, CYCLES_MOV_IMM_MEM);
	}

	return true;
}

bool i386_device::drc_pri_mov_from_sreg(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &sreg_value = I3;
	const uml::parameter &mem_data   = I2;

	uint8_t modrm = drc_get_modrm(ctx);
	uint8_t seg   = MRM_REG(modrm);
	if (seg >= 6)
		return false;

	drc_flush_cycles(ctx);

	UML_AND(b, sreg_value, DRC_SEG(seg, selector), 0xffff);

	if (!ctx.desc->is_mem)
	{
		UML_ROLINS(b, DRC_REG32(MRM_RM32(modrm)), sreg_value, 0, 0x0000ffff);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_data, sreg_value);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write16);
	}
	bool is_reg = MRM_MOD(modrm) == 3;

	drc_record_cycles(ctx, is_reg ? CYCLES_MOV_SREG_REG : CYCLES_MOV_SREG_MEM);

	return true;
}

// ----------------------------------------------------------------------------
// XCHG/LEA/MOVZX/MOVSX
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_xchg8(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &reg_value     = I5;
	const uml::parameter &rm_value      = I1;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm = drc_get_modrm(ctx);
	int     reg_b = MRM_REG8(modrm);

	if (MRM_MOD(modrm) == 3)
	{
		int rm_b = MRM_RM8(modrm);
		UML_AND(b, reg_value, DRC_REG8(reg_b), 0xff);
		UML_AND(b, rm_value, DRC_REG8(rm_b), 0xff);
		UML_BREG_WRITE(b, reg_b, rm_value);
		UML_BREG_WRITE(b, rm_b, reg_value);

		drc_record_cycles(ctx, CYCLES_XCHG_REG_REG);
	}
	else
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		UML_AND(b, reg_value, DRC_REG8(reg_b), 0xff);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read8);
		UML_AND(b, rm_value, mem_data, 0xff);

		UML_MOV(b, mem_addr, mem_addr_save);
		UML_MOV(b, mem_data, reg_value);
		UML_CALLH(b, *m_mem_write8);

		UML_BREG_WRITE(b, reg_b, rm_value);

		drc_record_cycles(ctx, CYCLES_XCHG_REG_MEM);
	}

	return true;
}

bool i386_device::drc_pri_xchg16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &eax_scratch = I3;
	const uml::parameter &rn_scratch  = I0;

	int rn = OP_RM32(ctx.desc->opcode0);

	UML_AND(b, eax_scratch, DRC_REG32(EAX), 0xffff);
	UML_AND(b, rn_scratch, DRC_REG32(rn), 0xffff);
	UML_ROLINS(b, DRC_REG32(EAX), rn_scratch, 0, 0xffff);
	UML_ROLINS(b, DRC_REG32(rn), eax_scratch, 0, 0xffff);

	drc_record_cycles(ctx, CYCLES_XCHG_REG_REG);

	return true;
}

bool i386_device::drc_pri_xchg32_reg(compiler_state &ctx)
{
	const uml::parameter &xchg_scratch = I3;

	drcuml_block &b = ctx.block;

	int rn = OP_RM32(ctx.desc->opcode0);
	UML_MOV(b, xchg_scratch, DRC_REG32(EAX));
	UML_MOV(b, DRC_REG32(EAX), DRC_REG32(rn));
	UML_MOV(b, DRC_REG32(rn), xchg_scratch);

	drc_record_cycles(ctx, CYCLES_XCHG_REG_REG);

	return true;
}

bool i386_device::drc_pri_xchg32_modrm(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &xchg_scratch  = I3;
	const uml::parameter &reg_value     = I5;
	const uml::parameter &rm_value      = I1;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm = drc_get_modrm(ctx);
	if (MRM_MOD(modrm) == 3)
	{
		int rm_d  = MRM_RM32(modrm);
		int reg_d = MRM_REG32(modrm);
		UML_MOV(b, xchg_scratch, DRC_REG32(rm_d));
		UML_MOV(b, DRC_REG32(rm_d), DRC_REG32(reg_d));
		UML_MOV(b, DRC_REG32(reg_d), xchg_scratch);

		drc_record_cycles(ctx, CYCLES_XCHG_REG_REG);

		return true;
	}
	else
	{
		int reg_d = MRM_REG32(modrm);
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, mem_addr);
		UML_MOV(b, reg_value, DRC_REG32(reg_d));
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_read32);
		UML_MOV(b, rm_value, mem_data);

		UML_MOV(b, mem_addr, mem_addr_save);
		UML_MOV(b, mem_data, reg_value);
		UML_CALLH(b, *m_mem_write32);

		UML_MOV(b, DRC_REG32(reg_d), rm_value);

		drc_record_cycles(ctx, CYCLES_XCHG_REG_MEM);

		return true;
	}
}

bool i386_device::drc_pri_lea(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	if (!ctx.desc->is_mem)
		return false;

	(this->*ctx.gen_ea)(ctx);

	int seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;
	UML_SUB(b, ea_result, ea_result, DRC_SEG(seg, base));
	UML_MOV(b, DRC_GET_MRM_REG32(modrm), ea_result);

	drc_record_cycles(ctx, CYCLES_LEA);

	return true;
}

bool i386_device::drc_x0f_movzx_sx(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	// 0x0f 0xb6 MOVZX r, r/m8 · 0xb7 MOVZX r, r/m16 · 0xbe MOVSX r, r/m8 · 0xbf MOVSX r, r/m16
	bool is_sx    = (ctx.desc->opcode1 == 0xbe || ctx.desc->opcode1 == 0xbf);
	bool is_16    = (ctx.desc->opcode1 == 0xb7 || ctx.desc->opcode1 == 0xbf);
	bool is_mem   = MRM_MOD(modrm) != 3;

	if (is_16)
		drc_gen_rm16(ctx, modrm);
	else
		drc_gen_rm8(ctx, modrm);

	X86_CYCLES cyc;
	if (is_sx)
	{
		UML_SEXT(b, loaded_value, loaded_value, (is_16) ? SIZE_WORD : SIZE_BYTE);

		cyc = is_mem ? CYCLES_MOVSX_MEM_REG : CYCLES_MOVSX_REG_REG;
	}
	else
	{
		cyc = is_mem ? CYCLES_MOVZX_MEM_REG : CYCLES_MOVZX_REG_REG;
	}

	UML_MOV(b, DRC_GET_MRM_REG32(modrm), loaded_value);

	drc_record_cycles(ctx, cyc);

	return true;
}

bool i386_device::drc_x0f_movzx_sx16(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;

	uint8_t modrm = drc_get_modrm(ctx);
	// 0x0f 0xb6 MOVZX r, r/m8; 0xbe MOVSX r, r/m8 (16-bit dest handler)
	bool is_sx    = (ctx.desc->opcode1 == 0xbe);
	bool is_mem   = MRM_MOD(modrm) != 3;

	X86_CYCLES cyc;
	drc_gen_rm8(ctx, modrm);
	if (is_sx)
	{
		UML_SEXT(b, loaded_value, loaded_value, SIZE_BYTE);
		UML_AND(b, loaded_value, loaded_value, 0xffff);

		cyc = is_mem ? CYCLES_MOVSX_MEM_REG : CYCLES_MOVSX_REG_REG;
	}
	else
	{
		UML_AND(b, loaded_value, loaded_value, 0xff);

		cyc = is_mem ? CYCLES_MOVZX_MEM_REG : CYCLES_MOVZX_REG_REG;
	}
	UML_ROLINS(b, DRC_GET_MRM_REG32(modrm), loaded_value, 0, 0xffff);

	drc_record_cycles(ctx, cyc);

	return true;
}

// ----------------------------------------------------------------------------
// BSWAP/CMOVCC/SETSS
// ----------------------------------------------------------------------------

inline bool i386_device::drc_gen_bswap32(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	uint8_t reg = OP_RM32(ctx.desc->opcode1);

	UML_BSWAP(b, DRC_REG32(reg), DRC_REG32(reg));

	drc_record_cycles(ctx, CYCLES_BSWAP);

	return true;
}

bool i386_device::drc_x0f_bswap(compiler_state &ctx)
{
	return drc_gen_bswap32(ctx);
}

inline bool i386_device::drc_gen_cmovcc(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &cond_result  = I0;
	const uml::parameter &staged_value = I2;

	uint8_t modrm = drc_get_modrm(ctx);

	drc_gen_rm32(ctx, modrm);
	UML_MOV(b, staged_value, loaded_value);
	drc_gen_flags_cc(ctx, cc);

	const uml::code_label skip = NEW_LBL(ctx);

	UML_CMP(b, cond_result, 0);
	UML_JMPc(b, COND_Z, skip);

	UML_MOV(b, DRC_GET_MRM_REG32(modrm), staged_value);
	UML_LABEL(b, skip);

	drc_record_cycles(ctx, CYCLES_MOVSX_REG_REG);

	return true;
}

inline bool i386_device::drc_gen_cmovcc16(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &loaded_value = I0;
	const uml::parameter &cond_result  = I0;
	const uml::parameter &staged_value = I2;

	uint8_t modrm = drc_get_modrm(ctx);

	drc_gen_rm16(ctx, modrm);
	UML_MOV(b, staged_value, loaded_value);
	drc_gen_flags_cc(ctx, cc);

	const uml::code_label skip = NEW_LBL(ctx);

	UML_CMP(b, cond_result, 0);
	UML_JMPc(b, COND_Z, skip);

	UML_ROLINS(b, DRC_GET_MRM_REG32(modrm), staged_value, 0, 0xffff);
	UML_LABEL(b, skip);

	drc_record_cycles(ctx, CYCLES_MOVSX_REG_REG);

	return true;
}

bool i386_device::drc_x0f_cmovcc_32(compiler_state &ctx)
{
	return drc_gen_cmovcc(ctx, ctx.desc->opcode1 & FLAGS_CC_MASK);
}

bool i386_device::drc_x0f_cmovcc_16(compiler_state &ctx)
{
	return drc_gen_cmovcc16(ctx, ctx.desc->opcode1 & FLAGS_CC_MASK);
}

inline bool i386_device::drc_gen_setcc(compiler_state &ctx, uint8_t cc)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &ea_result     = I0;
	const uml::parameter &cond_result   = I0;
	const uml::parameter &mem_addr      = I0;
	const uml::parameter &mem_data      = I2;
	const uml::parameter &mem_addr_save = I6;

	uint8_t modrm = drc_get_modrm(ctx);

	bool is_m = ctx.desc->is_mem;

	if (is_m)
	{
		(this->*ctx.gen_ea)(ctx);

		UML_MOV(b, mem_addr_save, ea_result);
	}

	drc_gen_flags_cc(ctx, cc);

	if (!is_m)
	{
		UML_BREG_WRITE(b, MRM_RM8(modrm), cond_result);
	}
	else
	{

		UML_MOV(b, mem_data, cond_result);
		UML_MOV(b, mem_addr, mem_addr_save);
		drc_flush_cycles(ctx);
		UML_CALLH(b, *m_mem_write8);
	}

	drc_record_cycles(ctx, is_m ? CYCLES_SETCC_MEM : CYCLES_SETCC_REG);

	return true;
}

bool i386_device::drc_x0f_setcc_rm8(compiler_state &ctx)
{
	return drc_gen_setcc(ctx, ctx.desc->opcode1 & FLAGS_CC_MASK);
}

// ----------------------------------------------------------------------------
// XLAT
// ----------------------------------------------------------------------------

bool i386_device::drc_pri_xlat(compiler_state &ctx)
{
	drcuml_block &b = ctx.block;

	const uml::parameter &mem_addr = I0;
	const uml::parameter &mem_data = I2;
	const uml::parameter &ebx16    = I5;

	int seg = (ctx.desc->seg_override >= 0) ? ctx.desc->seg_override : DS;

	UML_AND(b, mem_addr, DRC_REG8(AL), 0xff);
	if ((ctx.mode == 1) != ctx.desc->asz_override)
	{
		UML_ADD(b, mem_addr, mem_addr, DRC_REG32(EBX));
	}
	else
	{
		UML_AND(b, ebx16, DRC_REG32(EBX), 0xffff);
		UML_ADD(b, mem_addr, mem_addr, ebx16);
	}
	UML_ADD(b, mem_addr, mem_addr, DRC_SEG(seg, base));
	drc_flush_cycles(ctx);
	UML_CALLH(b, *m_mem_read8);
	UML_AND(b, mem_addr, mem_data, 0xff);
	UML_BREG_WRITE(b, AL, mem_addr);

	drc_record_cycles(ctx, CYCLES_XLAT);

	return true;
}