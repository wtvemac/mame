// license:BSD-3-Clause
// copyright-holders:Ville Linde, Barry Rodewald, Carl, Philip Bennett
#ifndef MAME_CPU_I386_I386_H
#define MAME_CPU_I386_I386_H

#pragma once

#include "i386dasm.h"
#include "cpu/drcuml.h"
#include "divtlb.h"
#include "softfloat3/source/include/softfloat.h"
#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>


#define INPUT_LINE_A20      1
#define INPUT_LINE_SMI      2


// mingw has this defined for 32-bit compiles
#undef i386

#define X86_NUM_CPUS        4

class i386_device : public cpu_device, public device_vtlb_interface, public i386_disassembler::config
{
public:
	// construction/destruction
	i386_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual ~i386_device();

	// configuration helpers
	auto smiact() { return m_smiact.bind(); }
	auto ferr() { return m_ferr_handler.bind(); }

	uint64_t debug_segbase(int params, const uint64_t *param);
	uint64_t debug_seglimit(int params, const uint64_t *param);
	uint64_t debug_segofftovirt(int params, const uint64_t *param);
	uint64_t debug_virttophys(int params, const uint64_t *param);
	uint64_t debug_cacheflush(int params, const uint64_t *param);

	// Defer flag calculations until they're needed
	static constexpr uint32_t I386DRC_LAZY_FLAGS         = 0x00000001;
	// Inline lazy flag evaluation rather than using shared invariant code with a JMPT
	// guest code will run faster but will increase the need for DRC cache. Use if DRC cache size isn't an issue.
	static constexpr uint32_t I386DRC_INLINE_LAZY_FLAGS  = 0x00000002;
	// Self-modified-code check during non-page mode using content checksum
	static constexpr uint32_t I386DRC_SMC_CHECK_NCONT    = 0x00000004;
	// Self-modified-code check during page mode using content checksum
	static constexpr uint32_t I386DRC_SMC_CHECK_PCONT    = 0x00000008;
	// Self-modified-code check during page mode using faster page variant check
	static constexpr uint32_t I386DRC_SMC_CHECK_PVARI    = 0x00000010;
	// Use inlined fastram accessors in some hot code paths that already have TLB-resolved addresses
	static constexpr uint32_t I386DRC_INLINE_FASTRAM     = 0x00000020;
	// Use vector instructions in the backend to execute REP instructions. Only available on fastram entries.
	static constexpr uint32_t I386DRC_REP_VECTOR         = 0x00000040;
	// Use prefetch on the host processor (x64 only) for REP instructions. Only available on fastram entries.
	static constexpr uint32_t I386DRC_REP_PREFETCH       = 0x00000080;
	// Don't use intrablock branching
	static constexpr uint32_t I386DRC_DISABLE_INTRABLOCK = 0x00000100;
	// Causes memory accessor blocks only to be cached once (but prevents fastram modification after start)
	static constexpr uint32_t I386DRC_INVARIANT_FASTRAM  = 0x00000200;
	// Skip the A20 mask steps
	static constexpr uint32_t I386DRC_SKIP_A20MASK       = 0x000000400;
	// Skip the IN/OUT CPL>IOPL / V8086 privilege check and IO remap check (to clear DRC cache)
	static constexpr uint32_t I386DRC_SKIP_IOCHECKS      = 0x00000800;
	// Skip the limit, expand down and access checks during stack operations
	static constexpr uint32_t I386DRC_SKIP_STACKCHECKS   = 0x00001000;

	// Options that have a high chance of guest code working but it may not be fast.
	static constexpr uint32_t I386DRC_SAFE_OPTIONS    = (I386DRC_SMC_CHECK_NCONT | I386DRC_SMC_CHECK_PCONT);
	// Options that lean towards faster implementations but still trying to be correct
	static constexpr uint32_t I386DRC_FAST_OPTIONS    = (I386DRC_INLINE_FASTRAM | I386DRC_REP_VECTOR | I386DRC_REP_PREFETCH | I386DRC_INVARIANT_FASTRAM);
	// Options that prefer performance over correctness.
	static constexpr uint32_t I386DRC_FASTEST_OPTIONS = I386DRC_FAST_OPTIONS | (I386DRC_LAZY_FLAGS | I386DRC_INLINE_LAZY_FLAGS) | (I386DRC_SKIP_A20MASK | I386DRC_SKIP_IOCHECKS | I386DRC_SKIP_STACKCHECKS);

	void drc_set_cache_size(std::size_t bytes)
	{
		m_drc_cache->set_size(bytes);
	}
	void     drc_set_options(uint32_t options);
	uint32_t drc_get_options();
	void     i386drc_add_fastram(offs_t start, offs_t end, bool readonly, void *base, uint32_t access_size = 0, const offs_t *excluded = nullptr, uint32_t excluded_count = 0, bool reg32_write_only = false, bool virtual_base = false);
	void     i386drc_add_fastpaged(offs_t start, offs_t end, uint32_t *page_table, uint32_t page_table_mask, uint32_t page_index_base, uint32_t page_shift, uint32_t page_offset_mask, uint32_t page_valid_bit, uint32_t *ram_base, uint32_t ram_limit, uint32_t access_size = 0);
	void     clear_fastram(uint32_t select_start = 0);
	void     clear_fastpaged(uint32_t select_start = 0);

	void func_log_instruction_exec();
	void drc_stack_fault_ss_cb();
	void drc_stack_fault_gp_cb();

	// DRC core
	void drc_interpreter_step_one();
	void func_log_printf();
	void func_printf_ramdiag();
	void func_ramlog_epoch();
	void func_log_fastram();
	void func_log_slowram();
	void func_printf_ifallbackdiag();
	// DRC memory
	void drc_tlb_fault_cb();
	// DRC IRQ
	void drc_check_irq_native_cb();
	void drc_deliver_irq_vector_cb();
	void drc_get_irq_vector_cb();
	// DRC segments
	void drc_mov_to_sreg_cb();
	void drc_lldt_cb();
	void drc_ltr_cb();
	void drc_verr_cb();
	void drc_verw_cb();
	// DRC I/O, system management, cpuid
	void drc_flush_tlb_cb();
	void drc_invlpg_cb();
	void drc_write_cr0_cb();
	void drc_write_cr4_cb();
	void drc_lmsw_cb();
	void drc_cpuid_cb();
	void drc_rdmsr_cb();
	void drc_wrmsr_cb();
	// DRC x87 operations
	void drc_x87_interpreter_cb(uint8_t group_op);
	void drc_wait_cb();
	void x87_drc_check_exceptions_cb();
	void x87_drc_check_exceptions_store_cb();
	void x87_drc_add_cb();
	void x87_drc_sub_cb();
	void x87_drc_mul_cb();
	void x87_drc_div_cb();
	void x87_drc_load_f32_cb();
	void x87_drc_load_f64_cb();
	void x87_drc_store_f32_cb();
	void x87_drc_store_f64_cb();
	void x87_drc_load_arith_b_f32_cb();
	void x87_drc_load_i32_cb();
	// DRC Pentium operations
	void drc_mmx_nm_trap_cb();

protected:
	i386_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, int program_data_width, int program_addr_width, int io_data_width);

	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_debug_setup() override;

	// device_execute_interface overrides
	virtual uint32_t execute_min_cycles() const noexcept override { return 1; }
	virtual uint32_t execute_max_cycles() const noexcept override { return 40; }
	virtual bool execute_input_edge_triggered(int inputnum) const noexcept override { return inputnum == INPUT_LINE_NMI; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;
	virtual bool memory_translate(int spacenum, int intention, offs_t &address, address_space *&target_space) override;

	// device_state_interface overrides
	virtual void state_import(const device_state_entry &entry) override;
	virtual void state_export(const device_state_entry &entry) override;
	virtual void state_string_export(const device_state_entry &entry, std::string &str) const override;

	// device_disasm_interface overrides
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;
	virtual int get_mode() const override;

	// cpu-specific system management mode routines
	virtual void enter_smm();
	virtual void leave_smm();

	// routines for opcodes whose operation can vary between cpu models
	// default implementations usually just log an error message
	virtual void opcode_cpuid();
	virtual uint64_t opcode_rdmsr(bool &valid_msr);
	virtual void opcode_wrmsr(uint64_t data, bool &valid_msr);
	virtual void opcode_invd() { cache_invalidate(); }
	virtual void opcode_wbinvd() { cache_writeback(); cache_invalidate(); }

	// routines for the cache
	// default implementation assumes there is no cache
	virtual void cache_writeback() {}
	virtual void cache_invalidate() {}
	virtual void cache_clean() {}

	// routine to access memory
	virtual u8 mem_pr8(offs_t address) { return macache32.read_byte(address); }
	virtual u16 mem_pr16(offs_t address) { return macache32.read_word(address); }
	virtual u32 mem_pr32(offs_t address) { return macache32.read_dword(address); }

	address_space_config m_program_config;
	address_space_config m_io_config;

	std::unique_ptr<uint8_t[]> cycle_table_rm[X86_NUM_CPUS];
	std::unique_ptr<uint8_t[]> cycle_table_pm[X86_NUM_CPUS];


	union I386_GPR {
		uint32_t d[8];
		uint16_t w[16];
		uint8_t b[32];
	};

	struct I386_SREG {
		uint16_t selector;
		uint16_t flags;
		uint32_t base;
		uint32_t limit;
		int d;      // Operand size
		bool valid;
	};

	struct I386_SYS_TABLE {
		uint32_t base;
		uint32_t limit;
	};

	struct I386_SEG_DESC {
		uint32_t segment;
		uint32_t flags;
		uint32_t base;
		uint32_t limit;
	};

	union XMM_REG {
		uint8_t  b[16];
		uint16_t w[8];
		uint32_t d[4];
		uint64_t q[2];
		int8_t   c[16];
		int16_t  s[8];
		int32_t  i[4];
		int64_t  l[2];
		float  f[4];
		double  f64[2];
	};

	union MMX_REG {
		uint32_t d[2];
		int32_t  i[2];
		uint16_t w[4];
		int16_t  s[4];
		uint8_t  b[8];
		int8_t   c[8];
		float  f[2];
		uint64_t q;
		int64_t  l;
	};

	struct I386_CALL_GATE
	{
		uint16_t segment;
		uint16_t selector;
		uint32_t offset;
		uint8_t ar;  // access rights
		uint8_t dpl;
		uint8_t dword_count;
		uint8_t present;
	};

	enum FEATURE_FLAGS : uint32_t {
		// returned in the EDX register
		FF_PBE = (u32)1 << 31, // Pend. Brk. EN.
		FF_TM = 1 << 29,       // Thermal Monitor
		FF_HTT = 1 << 28,      // Multi-threading
		FF_SS = 1 << 27,       // Self Snoop
		FF_SSE2 = 1 << 26,     // SSE2 Extensions
		FF_SSE = 1 << 25,      // SSE Extensions
		FF_FXSR = 1 << 24,     // FXSAVE/FXRSTOR
		FF_MMX = 1 << 23,      // MMX Technology
		FF_ACPI = 1 << 22,     // Thermal Monitor and Clock Ctrl
		FF_DS = 1 << 21,       // Debug Store
		FF_CLFSH = 1 << 19,    // CLFLUSH instruction
		FF_PSN = 1 << 18,      // Processor Serial Number
		FF_PSE36 = 1 << 17,    // 36 Bit Page Size Extension
		FF_PAT = 1 << 16,      // Page Attribute Table
		FF_CMOV = 1 << 15,     // Conditional Move/Compare Instruction
		FF_MCA = 1 << 14,      // Machine Check Architecture
		FF_PGE = 1 << 13,      // PTE Global Bit
		FF_MTRR = 1 << 12,     // Memory Type Range Registers
		FF_SEP = 1 << 11,      // SYSENTER and SYSEXIT
		FF_APIC = 1 << 9,      // APIC on Chip
		FF_CX8 = 1 << 8,       // CMPXCHG8B Inst.
		FF_MCE = 1 << 7,       // Machine Check Exception
		FF_PAE = 1 << 6,       // Physical Address Extensions
		FF_MSR = 1 << 5,       // RDMSR and WRMSR Support
		FF_TSC = 1 << 4,       // Time Stamp Counter
		FF_PSE = 1 << 3,       // Page Size Extensions
		FF_DE = 1 << 2,        // Debugging Extensions
		FF_VME = 1 << 1,       // Virtual-8086 Mode Enhancement
		FF_FPU = 1 << 0,       // x87 FPU on Chip
		// retuned in the ECX register
		FF_RDRAND = 1 << 30,
		FF_F16C = 1 << 29,
		FF_AVX = 1 << 28,
		FF_OSXSAVE = 1 << 27,
		FF_XSAVE = 1 << 26,
		FF_AES = 1 << 25,
		FF_TSCD = 1 << 24,     // Deadline
		FF_POPCNT = 1 << 23,
		FF_MOVBE = 1 << 22,
		FF_x2APIC = 1 << 21,
		FF_SSE4_2 = 1 << 20,   // SSE4.2
		FF_SSE4_1 = 1 << 19,   // SSE4.1
		FF_DCA = 1 << 18,      // Direct Cache Access
		FF_PCID = 1 << 17,     // Process-context Identifiers
		FF_PDCM = 1 << 15,     // Perf/Debug Capability MSR
		FF_xTPR = 1 << 14,     // Update Control
		FF_CMPXCHG16B = 1 << 13,
		FF_FMA = 1 << 12,      // Fused Multiply Add
		FF_SDBG = 1 << 11,
		FF_CNXT_ID = 1 << 10,  // L1 Context ID
		FF_SSSE3 = 1 << 9,     // SSSE3 Extensions
		FF_TM2 = 1 << 8,       // Thermal Monitor 2
		FF_EIST = 1 << 7,      // Enhanced Intel SpeedStep Technology
		FF_SMX = 1 << 6,       // Safer Mode Extensions
		FF_VMX = 1 << 5,       // Virtual Machine Extensions
		FF_DS_CPL = 1 << 4,    // CPL Qualified Debug Store
		FF_MONITOR = 1 << 3,   // MONITOR/MWAIT
		FF_DTES64 = 1 << 2,    // 64 Bit DS Area
		FF_PCLMULQDQ = 1 << 1, // Carryless Multiplication
		FF_SSE3 = 1 << 0,      // SSE3 Extensions
	};

	enum CR0_BITS : uint32_t {
		CR0_PG = (u32)1 << 31, // Paging
		CR0_CD = 1 << 30,      // Cache disable
		CR0_NW = 1 << 29,      // Not writethrough
		CR0_AM = 1 << 18,      // Alignment mask
		CR0_WP = 1 << 16,      // Write protect
		CR0_NE = 1 << 5,       // Numeric error
		CR0_ET = 1 << 4,       // Extension type
		CR0_TS = 1 << 3,       // Task switched
		CR0_EM = 1 << 2,       // Emulation
		CR0_MP = 1 << 1,       // Monitor coprocessor
		CR0_PE = 1 << 0,       // Protection enabled
	};

	enum CR3_BITS : uint32_t {
		CR3_PCD = 1 << 4,
		CR3_PWT = 1 << 3,
	};

	enum CR4_BITS : uint32_t {
		CR4_SMAP = 1 << 21,
		CR4_SMEP = 1 << 20,
		CR4_OSXSAVE = 1 << 18,
		CR4_PCIDE = 1 << 17,
		CR4_FSGSBASE = 1 << 16,
		CR4_SMXE = 1 << 14,
		CR4_VMXE = 1 << 13,
		CR4_OSXMMEXCPT = 1 << 10,
		CR4_OSFXSR = 1 << 9,
		CR4_PCE = 1 << 8,
		CR4_PGE = 1 << 7,
		CR4_MCE = 1 << 6,
		CR4_PAE = 1 << 5,
		CR4_PSE = 1 << 4,
		CR4_DE = 1 << 3,
		CR4_TSD = 1 << 2,
		CR4_PVI = 1 << 1,
		CR4_VME = 1 << 0,
	};

	typedef void (i386_device::*i386_modrm_func)(uint8_t modrm);
	typedef void (i386_device::*i386_op_func)();
	struct X86_OPCODE {
		uint8_t opcode;
		uint32_t flags;
		i386_op_func handler16;
		i386_op_func handler32;
		bool lockable;
	};
	static const X86_OPCODE s_x86_opcode_table[];

	bool m_auto_clear_RF;

	memory_passthrough_handler m_dr_breakpoints[4];
	util::notifier_subscription m_notifier;
	bool m_dri_changed_active;

	//386 Debug Register change handlers.
	inline void dri_changed();
	inline void dr7_changed(uint32_t old_val, uint32_t new_val);

	int m_halted;

	int m_operand_size;
	int m_xmm_operand_size;
	int m_address_size;
	int m_operand_prefix;
	int m_address_prefix;

	int m_segment_prefix;
	int m_segment_override;

	uint8_t m_opcode;

	address_space *m_program;
	address_space *m_io;
	memory_access<32, 1, 0, ENDIANNESS_LITTLE>::cache macache16;
	memory_access<32, 2, 0, ENDIANNESS_LITTLE>::cache macache32;

	int m_cpuid_max_input_value_eax; // Highest CPUID standard function available
	uint32_t m_cpuid_id0, m_cpuid_id1, m_cpuid_id2;
	uint32_t m_cpu_version;
	uint32_t m_brand_id;
	uint32_t m_feature_flags;
	uint64_t m_perfctr[2];

	// FPU

	i386_modrm_func m_opcode_table_x87_d8[256];
	i386_modrm_func m_opcode_table_x87_d9[256];
	i386_modrm_func m_opcode_table_x87_da[256];
	i386_modrm_func m_opcode_table_x87_db[256];
	i386_modrm_func m_opcode_table_x87_dc[256];
	i386_modrm_func m_opcode_table_x87_dd[256];
	i386_modrm_func m_opcode_table_x87_de[256];
	i386_modrm_func m_opcode_table_x87_df[256];

	// SSE
	XMM_REG m_sse_reg[8];
	uint32_t m_mxcsr;

	i386_op_func m_opcode_table1_16[256];
	i386_op_func m_opcode_table1_32[256];
	i386_op_func m_opcode_table2_16[256];
	i386_op_func m_opcode_table2_32[256];
	i386_op_func m_opcode_table338_16[256];
	i386_op_func m_opcode_table338_32[256];
	i386_op_func m_opcode_table33a_16[256];
	i386_op_func m_opcode_table33a_32[256];
	i386_op_func m_opcode_table366_16[256];
	i386_op_func m_opcode_table366_32[256];
	i386_op_func m_opcode_table3f2_16[256];
	i386_op_func m_opcode_table3f2_32[256];
	i386_op_func m_opcode_table3f3_16[256];
	i386_op_func m_opcode_table3f3_32[256];
	i386_op_func m_opcode_table46638_16[256];
	i386_op_func m_opcode_table46638_32[256];
	i386_op_func m_opcode_table4f238_16[256];
	i386_op_func m_opcode_table4f238_32[256];
	i386_op_func m_opcode_table4f338_16[256];
	i386_op_func m_opcode_table4f338_32[256];
	i386_op_func m_opcode_table4663a_16[256];
	i386_op_func m_opcode_table4663a_32[256];
	i386_op_func m_opcode_table4f23a_16[256];
	i386_op_func m_opcode_table4f23a_32[256];

	bool m_lock_table[2][256];

	uint8_t *m_cycle_table_pm;
	uint8_t *m_cycle_table_rm;

	bool m_nmi_masked;
	bool m_nmi_latched;
	uint32_t m_smbase;
	devcb_write_line m_smiact;
	devcb_write_line m_ferr_handler;
	bool m_lock;

	// bytes in current opcode, debug only
	uint8_t m_opcode_bytes[16];
	uint32_t m_opcode_pc;
	int m_opcode_bytes_length;
	offs_t m_opcode_addrs[16];
	uint32_t m_opcode_addrs_index;

	uint64_t m_debugger_temp;

	uint32_t m_features;

	struct internal_i386_state {
		uint32_t pc;
		uint32_t eip;
		uint32_t prev_eip;

		int      cycles;
		int      base_cycles;
		uint64_t tsc;
		// Similar to pending_cycles in compiler_state but used in invariant code (compiler_state isn't passed)
		int      pending_cycles;

		I386_GPR reg;

		uint32_t eflags;
		uint32_t eflags_mask;

		uint32_t CF;   // Carry flag
		uint32_t PF;   // Parity Flag
		uint32_t AF;   // Auxiliary carry flag
		uint32_t ZF;   // Zero flag
		uint32_t SF;   // Sign flag
		uint32_t TF;   // Trap flag
		uint32_t IF;   // Interrupt enable flag
		uint32_t DF;   // Direction flag
		uint32_t OF;   // Overflow flag
		uint32_t IOPL; // I/O privilege level (0-3)
		uint32_t NT;   // Nested task flag
		uint32_t RF;   // Resume flag
		uint32_t VM;   // V8086 mode flag
		uint32_t AC;   // Alignment check
		uint32_t VIF;  // Virtual interrupt flag
		uint32_t VIP;  // Virtual interrupt pending
		uint32_t ID;   // Identification flag

		uint32_t flags_data_a;
		uint32_t flags_data_b;
		uint32_t flags_carry;
		uint32_t flags_optype;
		uint32_t flags_of_direct;
		uint32_t flags_cc;

		uint32_t CPL;  // current privilege level

		uint32_t cr[5]; // Control registers
		uint32_t dr[8]; // Debug registers
		uint32_t tr[8]; // Test registers

		uint32_t page_invalidate_addr;
		uint32_t page_invalidate_mode;
		uint32_t tlb_miss_faulted;
		uint32_t tlb_fault_code;

		uint32_t tw_pde;
		uint32_t tw_pte;
		uint32_t tw_perm;
		uint32_t tw_is4m;
		uint32_t tw_scratch;
		uint32_t tw_supervisor_read;

		uint32_t mem_laddr;
		uint32_t mem_paddr;
		bool     mem_iswrite;

		uint32_t data32;
		uint32_t data16;
		uint32_t data8;
		uint64_t data64;
		uint8_t data128[16];

		uint32_t rep_chunked_physstart;
		uint32_t rep_chunked_bytes;
		uint32_t rep_chunked_fillvalue;
		uint32_t rep_chunked_pending_region;
		uint32_t rep_chunked_src_physstart;
		uint32_t rep_chunked_dst_physstart;
		uint32_t rep_chunked_movs_bytes;
		uint32_t rep_chunked_pending_src_region;
		uint32_t rep_chunked_pending_dst_region;


		uint32_t irq_state;
		uint32_t ext;  // external interrupt
		uint32_t delayed_interrupt_enable;
		uint32_t performed_intersegment_jump;

		uint32_t irq_vector_pending;
		uint32_t irq_gate_is_trap;
		uint32_t irq_old_cs_selector;
		uint32_t irq_old_ss_selector;
		uint32_t irq_old_esp;
		uint32_t soft_int_vector;
		uint32_t soft_int_ret_eip;

		uint32_t ctl_target_dpl;
		uint32_t ctl_new_ss;
		uint32_t ctl_new_esp;
		uint32_t ctl_newflags;
		uint32_t ctl_new_cs;
		uint32_t ctl_new_eip;
		uint32_t ctl_pop_count;

		I386_SREG sreg[6]; // ES=0,CS=1,SS=2,DS=3,FS=4,GS=5

		I386_SYS_TABLE gdtr; // Global Descriptor Table Register
		I386_SYS_TABLE idtr; // Interrupt Descriptor Table Register
		I386_SEG_DESC task;  // Task register
		I386_SEG_DESC ldtr;  // Local Descriptor Table Register

		offs_t a20_mask;

		uint32_t smm;
		uint32_t smi;
		uint32_t smi_latched;

		extFloat80_t x87_reg[8];
		uint32_t x87_cw;
		uint32_t x87_sw;
		uint32_t x87_tw;
		uint32_t x87_ds;
		uint32_t x87_data_ptr;
		uint32_t x87_cs;
		uint32_t x87_inst_ptr;
		uint32_t x87_opcode;

		extFloat80_t x87_data_a;
		extFloat80_t x87_data_b;
		extFloat80_t x87_data_result;
		uint32_t     x87_check_result;

		uint32_t drc_cache_dirty;
		uint32_t drc_cached_invariant;

		uint64_t mem_diag_addr;
		uint32_t mem_diag_is_write;

		uint32_t drc_ifallback_start_pc;

		uint32_t drc_debug_arg0;
		uint32_t drc_debug_arg1;
		uint32_t drc_debug_arg2;
	};
	internal_i386_state *m_core;

	bool m_drc_enabled;

	enum : size_t
	{
		DRC_CACHE_SIZE = 32 * 1024 * 1024
	};

	class opcode_desc;
	class frontend;
	struct compiler_state;

	std::unique_ptr<drc_cache>    m_drc_cache;
	std::unique_ptr<drcuml_state> m_drc_uml;
	std::unique_ptr<frontend>     m_drcfe;

	static constexpr int    I386DRC_COMPILE_BACKWARDS_BYTES = 128;
	static constexpr int    I386DRC_COMPILE_FORWARDS_BYTES  = 512;
	static constexpr int    I386DRC_COMPILE_MAX_SEQUENCE    = 64;
	static constexpr size_t I386DRC_MAX_PAGE_VARIANTS       = 4;
	// How many CMP/JMP to chain rather than a direct memory lookup jmp
	// Mainly for branch predictor optimization
	static constexpr size_t   I386DRC_JMPT_REC_CHAIN_COUNT  = 0;
	static constexpr size_t   I386DRC_JMPT_MAT_CHAIN_COUNT  = 1;
	static constexpr int      VEC_OP_PC_RING_SIZE           = 10;
	static constexpr uint32_t I386_MAX_FASTRAM              = 4;
	static constexpr uint32_t I386_MAX_FASTPAGED            = 2;
	static constexpr uint32_t FASTRAM_MAX_EXCLUDED          = 16;

	uint32_t m_drc_options;

	uml::code_handle *m_entry                           = nullptr;
	uml::code_handle *m_nocode                          = nullptr;
	uml::code_handle *m_cachefault                      = nullptr;
	uml::code_handle *m_fault                           = nullptr;
	uml::code_handle *m_out_of_cycles                   = nullptr;
	uml::code_handle *m_flags_eval_cc                   = nullptr;
	uml::code_handle *m_flags_eval_all                  = nullptr;
	uml::code_handle *m_push32                          = nullptr;
	uml::code_handle *m_pop32                           = nullptr;
	uml::code_handle *m_pack_eflags                     = nullptr;
	uml::code_handle *m_read_descriptor                 = nullptr;
	uml::code_handle *m_commit_cs_same_priv             = nullptr;
	uml::code_handle *m_write_descriptor_accessed       = nullptr;
	uml::code_handle *m_unpack_descriptor_limit_base    = nullptr;
	uml::code_handle *m_irq_deliver_protected_same_priv = nullptr;
	uml::code_handle *m_soft_int_deliver_protected      = nullptr;
	uml::code_handle *m_iret_protected                  = nullptr;
	uml::code_handle *m_retf_protected                  = nullptr;
	uml::code_handle *m_resolve_tlb                     = nullptr;
	uml::code_handle *m_mem_read8                       = nullptr;
	uml::code_handle *m_mem_read16                      = nullptr;
	uml::code_handle *m_mem_read32                      = nullptr;
	uml::code_handle *m_mem_write8                      = nullptr;
	uml::code_handle *m_mem_write16                     = nullptr;
	uml::code_handle *m_mem_write32                     = nullptr;
	uml::code_handle *m_mem_read64                      = nullptr;
	uml::code_handle *m_mem_write64                     = nullptr;
	uml::code_handle *m_mem_read128                     = nullptr;
	uml::code_handle *m_mem_write128                    = nullptr;

	struct fastram_entry
	{
		offs_t   start;
		offs_t   end;
		bool     readonly;
		void    *base;
		uint32_t access_size                    = 0;
		uint32_t excluded_count                 = 0;
		offs_t   excluded[FASTRAM_MAX_EXCLUDED] = {};
		bool     reg32_write_only               = false;
		bool     virtual_base                   = false;
	};
	fastram_entry m_fastram[I386_MAX_FASTRAM];
	uint32_t      m_fastram_select = 0;

	struct fastpaged_entry
	{
		offs_t    start, end;
		uint32_t *page_table;
		uint32_t  page_table_mask;
		uint32_t  page_index_base;
		uint32_t  page_shift;
		uint32_t  page_offset_mask;
		uint32_t  page_valid_bit;
		uint32_t *ram_base;
		uint32_t  ram_limit;
		uint32_t  access_size = 0;
	};
	fastpaged_entry m_fastpaged[I386_MAX_FASTPAGED];
	uint32_t        m_fastpaged_select = 0;

	struct page_variant_entry
	{
		uint32_t   physical_page;
		uint32_t   checkval;
		drccodeptr entry;
	};
	std::unordered_map<offs_t, std::vector<page_variant_entry>> m_page_variants;

	enum : int
	{
		EXECUTE_OUT_OF_CYCLES = 0,
		EXECUTE_MISSING_CODE  = 1,
		EXECUTE_UNMAPPED_CODE = 2,
		EXECUTE_RESET_CACHE   = 3,
		EXECUTE_CACHE_FAULT   = 4,
		EXECUTE_FAULT         = 5
	};

	static constexpr auto interpreter_fallback = nullptr;

	using drc_op_func = bool (i386_device::*)(compiler_state &ctx);
	struct DRC_OPCODE
	{
		uint8_t     opcode    = 0x00;
		uint32_t    flags     = 0;
		drc_op_func handler16 = interpreter_fallback;
		drc_op_func handler32 = interpreter_fallback;
		uint32_t    drc_flags = 0;
	};
	static const DRC_OPCODE s_drc_opcode_table[];

	struct drc_dispatch_t
	{
		drc_op_func handler16 = interpreter_fallback;
		drc_op_func handler32 = interpreter_fallback;
		uint32_t    drc_flags = 0;
	};
	drc_dispatch_t m_drc_sel_pri_table[256];
	drc_dispatch_t m_drc_sel_x0f_table[256];

	using drc_gen_ea_func  = void (i386_device::*)(compiler_state &ctx);
	using drc_skip_ea_func = void (i386_device::*)(compiler_state &ctx);

	using drc_x87_func = bool (i386_device::*)(compiler_state &ctx, uint8_t modrm);
	drc_x87_func m_drc_x87_table_d8[256];
	drc_x87_func m_drc_x87_table_d9[256];
	drc_x87_func m_drc_x87_table_da[256];
	drc_x87_func m_drc_x87_table_db[256];
	drc_x87_func m_drc_x87_table_dc[256];
	drc_x87_func m_drc_x87_table_dd[256];
	drc_x87_func m_drc_x87_table_de[256];
	drc_x87_func m_drc_x87_table_df[256];

	uint64_t m_diag_ramlog_epoch;
	uint64_t m_diag_fastram_acnt;
	uint64_t m_diag_fastram_adur;
	uint64_t m_diag_slowram_acnt;
	uint64_t m_diag_slowram_adur;
	struct diag_addr_entry
	{
		uint64_t acnt    = 0;
		uint64_t adur    = 0;
		uint32_t last_pc = 0;
	};
	std::unordered_map<uintptr_t, diag_addr_entry> m_diag_slowram_alog;
	std::time_t                                    m_last_ramdiag_print;

	uint64_t m_diag_itotal_acnt;
	uint64_t m_diag_ifallback_acnt;
	struct diag_ifallback_entry
	{
		uint64_t acnt     = 0;
		uint32_t last_pc  = 0;
		uint8_t  bytes[6] = { 0 };
	};
	std::unordered_map<uint32_t, diag_ifallback_entry> m_diag_ifallback_alog;
	std::time_t                                        m_last_ifallbackdiag_print;

	// DRC core
	template <void (i386_device::*Fn)()> static void cfunc_run_interp(void *p);
	template <void (i386_device::*Fn)()> static void cfunc_callback(void *p);
	template <typename Body> inline void             drc_catch_fault_inplace(Body &&body);
	template <typename Body> inline void             drc_catch_fault_inplace_sync(Body &&body);

	void           init_drc();
	void           execute_run_drc();
	void           code_flush_cache();
	void           generate_invariant();
	void           static_generate_entry_point(drcuml_block &b);
	void           static_generate_nocode_handler(drcuml_block &b);
	void           static_generate_cachefault_handler(drcuml_block &b);
	void           static_generate_fault_handler(drcuml_block &b);
	void           static_generate_out_of_cycles(drcuml_block &b);
	void           code_compile_block(uint8_t mode, offs_t pc, int compile_reason);
	bool           generate_sequence_instruction(compiler_state &ctx, uint32_t &page_offset, opcode_desc const *desc);
	void           build_drc_opcode_table(uint32_t features);
	bool           drc_dispatch_one(compiler_state &ctx, opcode_desc const *desc);
	bool           fastram_excluded_compare(const offs_t *a, int acount, const offs_t *b, int bcount);
	uint32_t       drc_page_variant_checkval(offs_t pc);
	void           drc_gen_page_check(compiler_state &ctx);
	uint32_t       drc_content_checkval(offs_t pc);
	void           drc_gen_content_check(compiler_state &ctx);
	compiler_state drc_create_compiler_state(drcuml_block &b, int &label_ctr, offs_t pc, offs_t eip, uint8_t mode);
	bool           drc_gen_control_transfer_cb(compiler_state &ctx, offs_t &cursor, void (*cfunc)(void *), void *param, bool end_block = true);
	bool           drc_gen_interpreter_fallback(compiler_state &ctx);
	void           drc_gen_fault_inplace_check(compiler_state &ctx);
	void           drc_enter_interpreter();
	void           drc_leave_interpreter();
	void           drc_record_cycles(compiler_state &ctx, int cycles_id);
	void           drc_flush_cycles(compiler_state &ctx);
	uint8_t        drc_get_modrm(compiler_state &ctx);
	uint8_t        drc_get_imm8(compiler_state &ctx);
	uint16_t       drc_get_imm16(compiler_state &ctx);
	uint32_t       drc_get_imm32(compiler_state &ctx);
	void           drc_gen_ea16(compiler_state &ctx);
	void           drc_gen_ea32(compiler_state &ctx);
	void           drc_skip_ea16(compiler_state &ctx);
	void           drc_skip_ea32(compiler_state &ctx);
	void           drc_gen_rm8(compiler_state &ctx, uint8_t modrm);
	void           drc_gen_rm16(compiler_state &ctx, uint8_t modrm);
	void           drc_gen_rm32(compiler_state &ctx, uint8_t modrm);
	std::string    drc_log_disasm_one(offs_t pc, uint32_t length);
	std::string    drc_log_desc_flags(opcode_desc const &desc);
	void           drc_log_opcode_desc(opcode_desc const *desclist, int indent);
	void           drc_log_add_disasm_comment(drcuml_block &block, opcode_desc const *desc);
	void           drc_add_symbols();
	uint8_t        drc_diag_read_byte(offs_t address);
	uint32_t       drc_compute_fallback_key(offs_t pc);
	void           func_log_fallback_exec(offs_t pc);
	// DRC memory operations
	void     static_generate_memory_accessors();
	void     static_generate_memory_accessor(int size, bool iswrite, const char *name, uml::code_handle *&handleptr);
	void     static_generate_resolve_tlb();
	void     allocate_memory_accessors();
	void    *drc_try_fastram(offs_t address, uint32_t size, bool iswrite);
	uint8_t  drc_fetch8(offs_t &cursor);
	uint8_t  drc_peek8(offs_t cursor);
	uint8_t  drc_fetch8inter();
	uint16_t drc_fetch16(offs_t &cursor);
	uint16_t drc_peek16(offs_t cursor);
	uint32_t drc_fetch32(offs_t &cursor);
	uint32_t drc_peek32(offs_t cursor);
	uint32_t drc_peek32phys(offs_t physical_address);
	uint32_t drc_resolve_tlb_page(offs_t address);
	void     drc_gen_inline_fastram32(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, const uml::code_label done, bool iswrite);
	void     drc_gen_privileged_read32(drcuml_block &b, int &label_ctr, const uml::parameter dest, const uml::parameter addr, const uml::parameter scratch, const uml::code_label take_slow);
	void     drc_gen_privileged_walk(drcuml_block &b, int &label_ctr, const uml::parameter address, const uml::parameter phys_out, const uml::code_label take_slow);
	void     drc_gen_privileged_read32walk(drcuml_block &b, int &label_ctr, const uml::parameter dest, const uml::parameter address, const uml::code_label take_slow);
	void     drc_gen_fastram_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite);
	void     drc_gen_fastpaged_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite);
	void     drc_gen_addrspace_memory_access(drcuml_block &b, int &label_ctr, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite);
	void     drc_gen_cross_page_memory_access(drcuml_block &b, const uml::parameter &addr, const uml::parameter &data, uint32_t size, bool iswrite);
	bool     drc_pri_mov_al_m8(compiler_state &ctx);
	bool     drc_pri_mov_m8_al(compiler_state &ctx);
	bool     drc_pri_mov_acc_moffs(compiler_state &ctx);
	bool     drc_pri_mov_acc_moffs16(compiler_state &ctx);
	bool     drc_pri_mov8(compiler_state &ctx);
	bool     drc_pri_mov16(compiler_state &ctx);
	bool     drc_pri_mov32(compiler_state &ctx);
	bool     drc_pri_mov_r8_imm(compiler_state &ctx);
	bool     drc_pri_mov_r16_imm(compiler_state &ctx);
	bool     drc_pri_mov_r32_imm(compiler_state &ctx);
	bool     drc_pri_mov_rm8_imm(compiler_state &ctx);
	bool     drc_pri_mov_rm16_imm(compiler_state &ctx);
	bool     drc_pri_mov_rm32_imm(compiler_state &ctx);
	bool     drc_pri_mov_from_sreg(compiler_state &ctx);
	bool     drc_pri_xchg8(compiler_state &ctx);
	bool     drc_pri_xchg16(compiler_state &ctx);
	bool     drc_pri_xchg32_reg(compiler_state &ctx);
	bool     drc_pri_xchg32_modrm(compiler_state &ctx);
	bool     drc_pri_lea(compiler_state &ctx);
	bool     drc_x0f_movzx_sx(compiler_state &ctx);
	bool     drc_x0f_movzx_sx16(compiler_state &ctx);
	bool     drc_gen_bswap32(compiler_state &ctx);
	bool     drc_x0f_bswap(compiler_state &ctx);
	bool     drc_gen_cmovcc(compiler_state &ctx, uint8_t cc);
	bool     drc_gen_cmovcc16(compiler_state &ctx, uint8_t cc);
	bool     drc_x0f_cmovcc_32(compiler_state &ctx);
	bool     drc_x0f_cmovcc_16(compiler_state &ctx);
	bool     drc_gen_setcc(compiler_state &ctx, uint8_t cc);
	bool     drc_x0f_setcc_rm8(compiler_state &ctx);
	bool     drc_pri_xlat(compiler_state &ctx);
	// DRC ALU / math operations
	void drc_gen_alu_op(compiler_state &ctx, uint8_t alu_opcode, const uml::parameter lhs, const uml::parameter rhs, const uml::parameter result, int width_bits);
	bool drc_pri_alu8(compiler_state &ctx);
	bool drc_gen_alu8_imm(compiler_state &ctx);
	bool drc_pri_alu_acc_imm(compiler_state &ctx);
	bool drc_pri_alu16(compiler_state &ctx);
	bool drc_gen_alu16_imm(compiler_state &ctx);
	bool drc_pri_alu16_acc_imm(compiler_state &ctx);
	bool drc_pri_alu32(compiler_state &ctx);
	bool drc_gen_alu32_imm(compiler_state &ctx);
	bool drc_pri_test8(compiler_state &ctx);
	bool drc_pri_test16(compiler_state &ctx);
	bool drc_pri_test32(compiler_state &ctx);
	bool drc_pri_test_acc_imm16(compiler_state &ctx);
	bool drc_pri_test_acc_imm32(compiler_state &ctx);
	bool drc_pri_group81_32(compiler_state &ctx);
	bool drc_pri_group81_16(compiler_state &ctx);
	bool drc_pri_group83_32(compiler_state &ctx);
	bool drc_pri_group83_16(compiler_state &ctx);
	bool drc_pri_group80_8(compiler_state &ctx);
	bool drc_pri_incdec_r32(compiler_state &ctx);
	bool drc_pri_incdec_r16(compiler_state &ctx);
	bool drc_gen_incdec_group(compiler_state &ctx, int width_bits, bool short_form);
	bool drc_gen_incdec32(compiler_state &ctx, bool short_form);
	bool drc_gen_incdec16(compiler_state &ctx, bool short_form);
	bool drc_pri_incdec8_rm(compiler_state &ctx);
	bool drc_pri_cbw_cwde(compiler_state &ctx);
	bool drc_pri_cwd_cdq(compiler_state &ctx);
	bool drc_gen_cwde(compiler_state &ctx);
	bool drc_gen_cdq(compiler_state &ctx);
	bool drc_gen_not_neg32(compiler_state &ctx);
	void drc_gen_f6_test(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_not(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_neg(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_mul(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_imul(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_div(compiler_state &ctx, uint8_t modrm);
	void drc_gen_f6_idiv(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_f6_group(compiler_state &ctx);
	bool drc_pri_groupF6_8(compiler_state &ctx);
	bool drc_gen_f7_16_test(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_f7_16_not(compiler_state &ctx, uint8_t rm16, bool is_m);
	bool drc_gen_f7_16_neg(compiler_state &ctx, uint8_t rm16, bool is_m);
	bool drc_gen_f7_16_mul(compiler_state &ctx, bool is_m);
	bool drc_gen_f7_16_imul(compiler_state &ctx, bool is_m);
	bool drc_gen_f7_16_div(compiler_state &ctx, bool is_m);
	bool drc_gen_f7_16_idiv(compiler_state &ctx, bool is_m);
	bool drc_gen_f7_group16(compiler_state &ctx);
	bool drc_pri_groupF7_16(compiler_state &ctx);
	bool drc_gen_mul32_acc(compiler_state &ctx);
	bool drc_gen_div32_acc(compiler_state &ctx);
	bool drc_pri_groupF7_32(compiler_state &ctx);
	bool drc_pri_imul_imm(compiler_state &ctx);
	bool drc_x0f_imul(compiler_state &ctx);
	bool drc_pri_shift8(compiler_state &ctx);
	bool drc_pri_shift16(compiler_state &ctx);
	bool drc_pri_shift32(compiler_state &ctx);
	bool drc_gen_shift_group_load(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t &rm_reg);
	void drc_gen_shift_group_store(compiler_state &ctx, uint8_t modrm, int width_bits, bool is_m, uint8_t rm_reg);
	bool drc_gen_shift_group_rotate_carry(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t shift_opcode);
	bool drc_gen_shift_group_count(compiler_state &ctx, uint8_t modrm, int width_bits, uint8_t shift_opcode);
	bool drc_gen_shift_group(compiler_state &ctx, int width_bits);
	void drc_gen_bt_apply_modify(compiler_state &ctx, uint8_t bt_opcode, const uml::parameter &new_value, const uml::parameter &bit_mask, const uml::parameter &bit_pos);
	int  drc_gen_bt_cycles(uint8_t bt_opcode, bool is_m, bool is_imm) const;
	bool drc_gen_bt_group_mem_dynamic(compiler_state &ctx, uint8_t modrm, uint8_t bt_opcode);
	bool drc_gen_bt_group_general(compiler_state &ctx, uint8_t modrm, uint8_t bt_opcode, bool is_imm, bool is_m);
	bool drc_x0f_bt_group(compiler_state &ctx);
	bool drc_x0f_shld(compiler_state &ctx);
	bool drc_x0f_shrd(compiler_state &ctx);
	bool drc_x0f_bsf_bsr(compiler_state &ctx);
	bool drc_x0f_xadd(compiler_state &ctx);
	bool drc_x0f_cmpxchg(compiler_state &ctx);
	// DRC control flow operations (jmp, jcc, call etc...)
	void drc_gen_intrablock_jump(compiler_state &ctx, offs_t target_pc);
	bool drc_gen_jcc32(compiler_state &ctx, uint8_t cc, bool is_rel32);
	bool drc_gen_jcc_short16(compiler_state &ctx, uint8_t cc);
	bool drc_gen_jcc16(compiler_state &ctx, uint8_t cc);
	bool drc_pri_jcc_rel8_32(compiler_state &ctx);
	bool drc_pri_jcc_rel8_16(compiler_state &ctx);
	bool drc_x0f_jcc_rel32(compiler_state &ctx);
	bool drc_x0f_jcc_rel16(compiler_state &ctx);
	bool drc_pri_jcxz(compiler_state &ctx);
	bool drc_pri_jmp_rel8_32(compiler_state &ctx);
	bool drc_pri_jmp_rel16(compiler_state &ctx);
	bool drc_pri_jmp_rel8_16(compiler_state &ctx);
	bool drc_pri_jmp_rel32(compiler_state &ctx);
	bool drc_pri_jmp_rm32(compiler_state &ctx);
	bool drc_pri_jmp_rm16(compiler_state &ctx);
	bool drc_pri_jmp_abs(compiler_state &ctx);
	bool drc_pri_loop(compiler_state &ctx);
	bool drc_pri_call32(compiler_state &ctx);
	bool drc_pri_call16(compiler_state &ctx);
	bool drc_pri_ret32(compiler_state &ctx);
	bool drc_pri_retf_i16(compiler_state &ctx);
	bool drc_pri_retf16(compiler_state &ctx);
	bool drc_pri_retf32(compiler_state &ctx);
	bool drc_pri_groupFF_32(compiler_state &ctx);
	bool drc_pri_groupFF_16(compiler_state &ctx);
	bool drc_pri_nop(compiler_state &ctx);
	// DRC IRQ operations
	void static_generate_irq_deliver_protected_same_priv();
	void static_generate_soft_int_deliver_protected();
	void static_generate_iret_protected();
	void static_generate_retf_protected();
	void drc_gen_irq_poll(compiler_state &ctx);
	void drc_gen_irq_deliver_realmode(compiler_state &ctx);
	void drc_gen_deliver_protected_same_priv(compiler_state &ctx, bool is_software, const uml::code_label bail, const uml::code_label done, const uml::parameter ret_eip, int cycle_count);
	void drc_gen_deliver_protected_inner_priv(compiler_state &ctx, const uml::parameter new_eip, const uml::parameter target_cs_selector, const uml::parameter target_cs_v1, const uml::parameter target_cs_v2, const uml::parameter dpl, const uml::parameter ret_eip, int cycle_count, const uml::code_label bail);
	void drc_gen_int_finish(compiler_state &ctx, const uml::parameter new_eip, const uml::parameter ret_eip, int cycle_count);
	void drc_gen_soft_int_realmode(compiler_state &ctx, uint32_t vector, uint32_t ret_eip, int trap_cycles);
	void drc_gen_iret_protected(compiler_state &ctx, const uml::code_label bail);
	void drc_gen_retf_protected(compiler_state &ctx, const uml::code_label bail);
	void drc_gen_iret_restore_eflags(compiler_state &ctx, const uml::parameter working_eflags, const uml::parameter packed_eflags);
	bool drc_pri_int3(compiler_state &ctx);
	bool drc_pri_int(compiler_state &ctx);
	bool drc_pri_into(compiler_state &ctx);
	bool drc_pri_iret16(compiler_state &ctx);
	bool drc_pri_iret32(compiler_state &ctx);
	bool drc_pri_hlt(compiler_state &ctx);
	// DRC stack operations
	void static_generate_push32();
	void static_generate_pop32();
	void drc_gen_push32(compiler_state &ctx, const uml::parameter val);
	void drc_gen_pop32(compiler_state &ctx);
	void drc_gen_push16(compiler_state &ctx, const uml::parameter val);
	void drc_gen_pop16(compiler_state &ctx);
	bool drc_gen_pusha(compiler_state &ctx);
	bool drc_gen_popa(compiler_state &ctx);
	void drc_gen_stack_peek_range(drcuml_block &b, int &label_ctr, const uml::code_label take_slow, uint32_t byte_count);
	void drc_gen_stack_fault_check(drcuml_block &b, int &label_ctr, const uml::parameter &offset, uint32_t byte_count, bool is_write);
	bool drc_gen_leave(compiler_state &ctx);
	bool drc_gen_leave16(compiler_state &ctx);
	bool drc_pri_pusha(compiler_state &ctx);
	bool drc_pri_popa(compiler_state &ctx);
	bool drc_pri_leave32(compiler_state &ctx);
	bool drc_pri_leave16(compiler_state &ctx);
	bool drc_pri_push_eax32(compiler_state &ctx);
	bool drc_pri_push_eax16(compiler_state &ctx);
	bool drc_pri_push_ecx32(compiler_state &ctx);
	bool drc_pri_push_ecx16(compiler_state &ctx);
	bool drc_pri_push_edx32(compiler_state &ctx);
	bool drc_pri_push_edx16(compiler_state &ctx);
	bool drc_pri_push_ebx32(compiler_state &ctx);
	bool drc_pri_push_ebx16(compiler_state &ctx);
	bool drc_pri_push_esp32(compiler_state &ctx);
	bool drc_pri_push_esp16(compiler_state &ctx);
	bool drc_pri_push_ebp32(compiler_state &ctx);
	bool drc_pri_push_ebp16(compiler_state &ctx);
	bool drc_pri_push_esi32(compiler_state &ctx);
	bool drc_pri_push_esi16(compiler_state &ctx);
	bool drc_pri_push_edi32(compiler_state &ctx);
	bool drc_pri_push_edi16(compiler_state &ctx);
	bool drc_pri_pop_r32(compiler_state &ctx);
	bool drc_pri_pop_r16(compiler_state &ctx);
	bool drc_pri_push_i32(compiler_state &ctx);
	bool drc_pri_push_i16(compiler_state &ctx);
	bool drc_pri_push_i8_32(compiler_state &ctx);
	bool drc_pri_push_i8_16(compiler_state &ctx);
	bool drc_pri_push_rm32(compiler_state &ctx);
	bool drc_pri_push_rm16(compiler_state &ctx);
	bool drc_pri_pushfd(compiler_state &ctx);
	bool drc_pri_pushf16(compiler_state &ctx);
	bool drc_pri_popfd(compiler_state &ctx);
	bool drc_pri_popf16(compiler_state &ctx);
	// DRC segment operations
	void static_generate_read_descriptor();
	void static_generate_commit_cs_same_priv();
	void static_generate_write_descriptor_accessed();
	void static_generate_unpack_descriptor_limit_base();
	void drc_gen_write_descriptor_accessed_inline(drcuml_block &b, int &label_ctr, const uml::parameter access_byte, const uml::code_label take_slow);
	void drc_gen_unpack_descriptor_limit_inline(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi);
	void drc_gen_unpack_descriptor_limit_base_inline(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter base_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi);
	void drc_gen_commit_seg_descriptor(drcuml_block &b, int seg, const uml::parameter access_flags, const uml::parameter new_access_byte);
	void drc_gen_read_descriptor(drcuml_block &b, int &label_ctr, const uml::code_label take_slow);
	void drc_gen_privileged_read_descriptor(drcuml_block &b, int &label_ctr, const uml::parameter out_lo, const uml::parameter out_hi, const uml::code_label take_slow);
	void drc_gen_commit_cs_same_priv(drcuml_block &b, int &label_ctr, const uml::parameter new_eip, const uml::code_label take_slow);
	void drc_gen_write_descriptor_accessed(drcuml_block &b, int &label_ctr, const uml::parameter access_byte, const uml::code_label take_slow);
	void drc_gen_unpack_descriptor_limit_base(drcuml_block &b, int &label_ctr, const uml::parameter limit_dest, const uml::parameter base_dest, const uml::parameter descriptor_lo, const uml::parameter descriptor_hi);
	void drc_gen_commit_ss_for_privilege_change(drcuml_block &b, int &label_ctr, const uml::parameter dpl, const uml::code_label take_slow);
	bool drc_gen_push_seg32(compiler_state &ctx, int seg_reg);
	bool drc_gen_push_seg16(compiler_state &ctx, int seg_reg);
	bool drc_gen_pop_sreg(compiler_state &ctx, int seg, void (*slow_cfunc)(void *));
	void drc_gen_pop_seg_peek(drcuml_block &b, int &label_ctr, const uml::code_label take_slow);
	bool drc_pri_push_es16(compiler_state &ctx);
	bool drc_pri_push_es32(compiler_state &ctx);
	bool drc_pri_push_cs16(compiler_state &ctx);
	bool drc_pri_push_cs32(compiler_state &ctx);
	bool drc_pri_push_ss16(compiler_state &ctx);
	bool drc_pri_push_ss32(compiler_state &ctx);
	bool drc_pri_push_ds16(compiler_state &ctx);
	bool drc_pri_push_ds32(compiler_state &ctx);
	bool drc_x0f_push_fs16(compiler_state &ctx);
	bool drc_x0f_push_fs32(compiler_state &ctx);
	bool drc_x0f_push_gs16(compiler_state &ctx);
	bool drc_x0f_push_gs32(compiler_state &ctx);
	bool drc_pri_pop_es(compiler_state &ctx);
	bool drc_pri_pop_ss(compiler_state &ctx);
	bool drc_pri_pop_ds(compiler_state &ctx);
	bool drc_x0f_pop_fs(compiler_state &ctx);
	bool drc_x0f_pop_gs(compiler_state &ctx);
	bool drc_pri_mov_to_sreg(compiler_state &ctx);
	// DRC I/O, system management, cpuid
	void drc_gen_get_io_port(compiler_state &ctx);
	void drc_gen_io_permission_check(compiler_state &ctx);
	void drc_gen_io_remap_check(compiler_state &ctx);
	void drc_io_fault_cb();
	bool drc_pri_io_read8(compiler_state &ctx);
	bool drc_pri_io_write8(compiler_state &ctx);
	bool drc_pri_io_read16(compiler_state &ctx);
	bool drc_pri_io_write16(compiler_state &ctx);
	bool drc_pri_io_read32(compiler_state &ctx);
	bool drc_pri_io_write32(compiler_state &ctx);
	bool drc_gen_0f00_sldt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f00_str(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f00_lldt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f00_ltr(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f00_verr(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f00_verw(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_x0f_group0f00(compiler_state &ctx);
	bool drc_gen_0f01_sgdt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_sidt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_lgdt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_lidt(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_smsw(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_lmsw(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_gen_0f01_invlpg(compiler_state &ctx, uint8_t modrm, bool is_reg);
	bool drc_x0f_group0f01(compiler_state &ctx);
	bool drc_x0f_clts(compiler_state &ctx);
	bool drc_x0f_mov_r32_cr(compiler_state &ctx);
	bool drc_x0f_mov_cr_r32(compiler_state &ctx);
	bool drc_x0f_cpuid(compiler_state &ctx);
	bool drc_x0f_rdmsr(compiler_state &ctx);
	bool drc_x0f_wrmsr(compiler_state &ctx);
	bool drc_x0f_rdtsc(compiler_state &ctx);
	// DRC string ops and rep
	template <typename EmitFlush> inline void drc_gen_chunked_tlb_translate(compiler_state &ctx, const uml::parameter &address, const uml::parameter &entry_reg, const uml::parameter &phys_addr, bool is_write, EmitFlush &&flush);
	template <typename OnChunk> inline void   drc_gen_stos_vector_fill_tiers(compiler_state &ctx, void *fastbase, const uml::parameter &fill_addr, const uml::parameter &fill_vector, const uml::parameter &remaining_bytes, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes, OnChunk &&on_chunk);
	template <typename OnChunk> inline void   drc_gen_movs_vector_copy_tiers(compiler_state &ctx, void *src_fastbase, void *dst_fastbase, const uml::parameter &src_addr, const uml::parameter &dst_addr, const uml::parameter &copy_vector, const uml::parameter &remaining_bytes, bool wide_tier_active, uint32_t wide_chunk_bytes, OnChunk &&on_chunk);

	void drc_gen_mem_ptr_add(compiler_state &ctx, int reg, const uml::parameter &seg_base);
	void drc_gen_mem_ptr_advance(compiler_state &ctx, int reg);
	void drc_gen_rep_zf_early_exit(compiler_state &ctx, const uml::code_label done);
	void drc_gen_rep_movs_chunked_flush(compiler_state &ctx, const uml::parameter &esi_reg, const uml::parameter &edi_reg, const uml::parameter &ecx_reg, uint32_t shift, int cycles_per_element, bool wide_tier_active, uint32_t wide_chunk_bytes);
	void drc_gen_stos_chunked_fill_region(compiler_state &ctx, void *fastbase, const uml::parameter &flush_addr, const uml::parameter &flush_remaining, const uml::parameter &fill_byte, const uml::parameter &fill_vector, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes, const uml::code_label flush_done);
	void drc_gen_rep_stos_chunked_flush(compiler_state &ctx, const uml::parameter &edi_reg, const uml::parameter &ecx_reg, uint32_t shift, int cycles_per_element, bool wide_fill_tier_active, uint32_t wide_fill_chunk_bytes);
	bool drc_gen_mem_movs(compiler_state &ctx, uint32_t operand_bytes);
	bool drc_gen_mem_cmps(compiler_state &ctx, uint32_t operand_bytes);
	bool drc_gen_mem_stos(compiler_state &ctx, uint32_t operand_bytes);
	bool drc_gen_mem_lods(compiler_state &ctx, uint32_t operand_bytes);
	bool drc_gen_mem_scas(compiler_state &ctx, uint32_t operand_bytes);
	bool drc_gen_mem(compiler_state &ctx, uint8_t opcode);
	bool drc_pri_mem32(compiler_state &ctx);
	bool drc_pri_mem16(compiler_state &ctx);
	void drc_gen_rep_stos_paged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar);
	void drc_gen_rep_stos_nonpaged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar);
	void drc_gen_rep_movs_paged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar);
	void drc_gen_rep_movs_nonpaged_chunked(compiler_state &ctx, offs_t rep_pc, uint32_t operand_bytes, const uml::code_label done, const uml::code_label bail_to_scalar);
	bool drc_gen_rep(compiler_state &ctx);
	bool drc_pri_repne16(compiler_state &ctx);
	bool drc_pri_repne32(compiler_state &ctx);
	bool drc_pri_rep16(compiler_state &ctx);
	bool drc_pri_rep32(compiler_state &ctx);
	// DRC flag management / lazy flags
	void static_generate_flags_eval_cc();
	void static_generate_flags_eval_all();
	void static_generate_pack_eflags();
	void drc_flags_all();
	void drc_gen_set_pf(drcuml_block &b, const uml::parameter src);
	void drc_gen_flags_rotate(drcuml_block &b);
	void drc_gen_combine_flags(drcuml_block &b, uint8_t cc);
	void drc_gen_unpack_eflags(drcuml_block &b, const uml::parameter src, const uml::parameter scratch);
	void drc_gen_clear_flags(compiler_state &ctx);
	void drc_gen_defer_flags_arith(compiler_state &ctx, uint32_t optype, int width_bits);
	void drc_gen_defer_flags_logical(compiler_state &ctx, uint32_t optype);
	void drc_gen_defer_flags_incdec(compiler_state &ctx, uint32_t optype);
	void drc_gen_defer_flags_adcsbb(compiler_state &ctx, uint32_t optype, int width_bits);
	void drc_gen_defer_flags_shift(compiler_state &ctx, uint32_t optype, int width_bits);
	void drc_gen_defer_flags_alu(compiler_state &ctx, uint8_t alu_opcode, int width_bits);
	void drc_gen_flags_cc(compiler_state &ctx, uint8_t cc);
	void drc_gen_flags_cc_dispatch(compiler_state &ctx, uint8_t cc);
	void drc_gen_eval_flags_cc_arith(drcuml_block &b, int &label_ctr, uint8_t cc, bool is_sub, const uml::parameter op2, int width_bits, bool fast_path);
	void drc_gen_eval_flags_cc_adcsbb(drcuml_block &b, int &label_ctr, uint8_t cc, bool is_sub, int width_bits);
	void drc_gen_eval_flags_cc_shift(drcuml_block &b, uint8_t cc, int width_bits);
	void drc_gen_eval_flags_cc(drcuml_block &b, int &label_ctr, uint8_t cc, uint32_t optype);
	void drc_gen_flags_all(compiler_state &ctx);
	void drc_gen_flags_all_dispatch(compiler_state &ctx);
	void drc_gen_eval_flags_all_arith(drcuml_block &b, int &label_ctr, bool is_sub, const uml::parameter op2, int width_bits);
	void drc_gen_eval_flags_all_adcsbb(drcuml_block &b, int &label_ctr, bool is_sub, int width_bits);
	void drc_gen_eval_flags_all_shift(drcuml_block &b, int width_bits);
	void drc_gen_eval_flags_all(drcuml_block &b, int &label_ctr, uint32_t optype);
	void drc_gen_pack_eflags(drcuml_block &b, const uml::parameter dest);
	bool drc_pri_cmc(compiler_state &ctx);
	bool drc_pri_clc(compiler_state &ctx);
	bool drc_pri_stc(compiler_state &ctx);
	bool drc_pri_cli(compiler_state &ctx);
	bool drc_pri_sti(compiler_state &ctx);
	bool drc_pri_cld(compiler_state &ctx);
	bool drc_pri_std(compiler_state &ctx);
	bool drc_pri_lahf(compiler_state &ctx);
	bool drc_pri_sahf(compiler_state &ctx);
	// DRC x87 operations
	void build_drc_x87_table();
	void build_drc_x87_table_d8();
	void build_drc_x87_table_d9();
	void build_drc_x87_table_da();
	void build_drc_x87_table_db();
	void build_drc_x87_table_dc();
	void build_drc_x87_table_dd();
	void build_drc_x87_table_de();
	void build_drc_x87_table_df();
	bool drc_pri_x87(compiler_state &ctx);
	void drc_gen_x87_mf_check(compiler_state &ctx, const uml::code_label fallback);
	void drc_gen_x87_cycles(compiler_state &ctx, int table_index);
	void drc_gen_x87_set_stack_top(compiler_state &ctx, const uml::parameter new_top, const uml::parameter sw_scratch);
	void drc_gen_x87_load_reg(compiler_state &ctx, const uml::parameter physx2, const uml::parameter signif, const uml::parameter signexp);
	void drc_gen_x87_store_reg(compiler_state &ctx, const uml::parameter physx2, const uml::parameter signif, const uml::parameter signexp);
	void drc_gen_x87_indefinite(compiler_state &ctx, const uml::parameter signif, const uml::parameter signexp);
	void drc_gen_x87_record_operand(compiler_state &ctx, uint8_t modrm, const uml::parameter address);
	void drc_gen_x87_set_tag(compiler_state &ctx, const uml::parameter physx2, const uml::parameter tag_value);
	void drc_gen_x87_get_tag(compiler_state &ctx, const uml::parameter physx2, const uml::parameter tag_dst);
	void drc_gen_x87_classify_tag(compiler_state &ctx, const uml::parameter signif, const uml::parameter signexp, const uml::parameter tag_dst);
	void drc_gen_x87_check_exceptions(compiler_state &ctx, bool store);
	void drc_gen_x87_interpreter_fallback(compiler_state &ctx);
	void drc_gen_x87_epilogue(compiler_state &ctx, const uml::code_label fallback);
	bool drc_x87_fld_mem32(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fld_mem64(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fstp_mem32(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fstp_mem64(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fadd_m32real(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fmul_m32real(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsub_m32real(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fadd_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fmul_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsub_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsubr_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdiv_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdivr_st_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fadd_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fmul_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsubr_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsub_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdivr_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdiv_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_faddp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fmulp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsubrp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fsubp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdivrp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_x87_fdivp_sti_st(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_stacktop_native(compiler_state &ctx, int delta);
	bool drc_gen_x87_fincstp(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fdecstp(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fnop(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_ffree(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fstsw_ax(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_finit(compiler_state &ctx, uint8_t modrm);
	bool drc_pri_wait(compiler_state &ctx);
	bool drc_gen_x87_signop_native(compiler_state &ctx, uint8_t modrm, uint32_t sign_xor_mask, uint32_t sign_and_mask);
	bool drc_gen_x87_fchs(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fabs(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fst_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fstp_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fxch_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fld_sti(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fld_mem(compiler_state &ctx, uint8_t modrm, bool is_64);
	bool drc_gen_x87_fild_m32int(compiler_state &ctx, uint8_t modrm);
	bool drc_gen_x87_fstp_mem(compiler_state &ctx, uint8_t modrm, bool is_64);
	bool drc_gen_x87_arith_m32real(compiler_state &ctx, uint8_t modrm, void (*arith_cfunc)(void *), int cycle_index);
	bool drc_gen_x87_arith_sti(compiler_state &ctx, uint8_t modrm, void (*arith_cfunc)(void *), int cycle_index, bool a_is_sti, bool dest_is_i, bool do_pop);
	// DRC Pentium operations
	template <typename ApplyFn> inline bool drc_gen_mmx_binop(compiler_state &ctx, bool mem_is_32bit, ApplyFn &&apply);

	void drc_gen_mmx_prolog(compiler_state &ctx);
	bool drc_x0f_mmx_group_0f71(compiler_state &ctx);
	bool drc_x0f_mmx_paddw(compiler_state &ctx);
	bool drc_x0f_mmx_movq_store(compiler_state &ctx);
	bool drc_x0f_mmx_movq_load(compiler_state &ctx);
	bool drc_x0f_mmx_movd_load(compiler_state &ctx);
	bool drc_x0f_mmx_pmullw(compiler_state &ctx);
	bool drc_x0f_mmx_punpcklbw(compiler_state &ctx);
	bool drc_x0f_mmx_punpckhbw(compiler_state &ctx);
	bool drc_x0f_mmx_packuswb(compiler_state &ctx);
	bool drc_x0f_mmx_paddusb(compiler_state &ctx);
	bool drc_x0f_mmx_bitwise(compiler_state &ctx);
	bool drc_x0f_mmx_add_wrap(compiler_state &ctx);
	bool drc_x0f_mmx_sub_wrap(compiler_state &ctx);
	bool drc_x0f_mmx_paddq(compiler_state &ctx);
	bool drc_x0f_mmx_add_usat(compiler_state &ctx);
	bool drc_x0f_mmx_sub_usat(compiler_state &ctx);
	bool drc_x0f_mmx_add_ssat(compiler_state &ctx);
	bool drc_x0f_mmx_sub_ssat(compiler_state &ctx);
	bool drc_x0f_mmx_punpckl(compiler_state &ctx);
	bool drc_x0f_mmx_punpckh(compiler_state &ctx);
	bool drc_x0f_mmx_emms(compiler_state &ctx);

	void register_state_i386();
	void register_state_i386_x87();
	void register_state_i386_x87_xmm();
	uint32_t i386_translate(int segment, uint32_t ip, int rwn, int size = 1);
	inline vtlb_entry get_permissions(uint32_t pte, int wp);
	bool i386_translate_address(int intention, bool debug, offs_t *address, vtlb_entry *entry);
	bool translate_address(int pl, int type, offs_t *address, uint32_t *error);
	void CHANGE_PC(uint32_t pc);
	inline void NEAR_BRANCH(int32_t offs);
	inline uint8_t FETCH();
	inline uint16_t FETCH16();
	inline uint32_t FETCH32();
	inline uint8_t READ8(uint32_t ea) { return READ8PL(ea, m_core->CPL); }
	inline uint16_t READ16(uint32_t ea) { return READ16PL(ea, m_core->CPL); }
	inline uint32_t READ32(uint32_t ea) { return READ32PL(ea, m_core->CPL); }
	inline uint64_t READ64(uint32_t ea) { return READ64PL(ea, m_core->CPL); }
	virtual uint8_t READ8PL(uint32_t ea, uint8_t privilege);
	virtual uint16_t READ16PL(uint32_t ea, uint8_t privilege);
	virtual uint32_t READ32PL(uint32_t ea, uint8_t privilege);
	virtual uint64_t READ64PL(uint32_t ea, uint8_t privilege);
	inline void WRITE_TEST(uint32_t ea);
	inline void WRITE8(uint32_t ea, uint8_t value) { WRITE8PL(ea, m_core->CPL, value); }
	inline void WRITE16(uint32_t ea, uint16_t value) { WRITE16PL(ea, m_core->CPL, value); }
	inline void WRITE32(uint32_t ea, uint32_t value) { WRITE32PL(ea, m_core->CPL, value); }
	inline void WRITE64(uint32_t ea, uint64_t value) { WRITE64PL(ea, m_core->CPL, value); }
	virtual void WRITE8PL(uint32_t ea, uint8_t privilege, uint8_t value);
	virtual void WRITE16PL(uint32_t ea, uint8_t privilege, uint16_t value);
	virtual void WRITE32PL(uint32_t ea, uint8_t privilege, uint32_t value);
	virtual void WRITE64PL(uint32_t ea, uint8_t privilege, uint64_t value);
	inline uint8_t OR8(uint8_t dst, uint8_t src);
	inline uint16_t OR16(uint16_t dst, uint16_t src);
	inline uint32_t OR32(uint32_t dst, uint32_t src);
	inline uint8_t AND8(uint8_t dst, uint8_t src);
	inline uint16_t AND16(uint16_t dst, uint16_t src);
	inline uint32_t AND32(uint32_t dst, uint32_t src);
	inline uint8_t XOR8(uint8_t dst, uint8_t src);
	inline uint16_t XOR16(uint16_t dst, uint16_t src);
	inline uint32_t XOR32(uint32_t dst, uint32_t src);
	inline uint8_t SBB8(uint8_t dst, uint8_t src, uint8_t b);
	inline uint16_t SBB16(uint16_t dst, uint16_t src, uint16_t b);
	inline uint32_t SBB32(uint32_t dst, uint32_t src, uint32_t b);
	inline uint8_t ADC8(uint8_t dst, uint8_t src, uint8_t c);
	inline uint16_t ADC16(uint16_t dst, uint16_t src, uint8_t c);
	inline uint32_t ADC32(uint32_t dst, uint32_t src, uint32_t c);
	inline uint8_t INC8(uint8_t dst);
	inline uint16_t INC16(uint16_t dst);
	inline uint32_t INC32(uint32_t dst);
	inline uint8_t DEC8(uint8_t dst);
	inline uint16_t DEC16(uint16_t dst);
	inline uint32_t DEC32(uint32_t dst);
	inline void PUSH16(uint16_t value);
	inline void PUSH32(uint32_t value);
	inline void PUSH32SEG(uint32_t value);
	inline void PUSH8(uint8_t value);
	inline uint8_t POP8();
	inline uint16_t POP16();
	inline uint32_t POP32();
	inline void BUMP_SI(int adjustment);
	inline void BUMP_DI(int adjustment);
	inline void check_ioperm(offs_t port, uint8_t mask);
	inline uint8_t READPORT8(offs_t port);
	inline void WRITEPORT8(offs_t port, uint8_t value);
	virtual uint16_t READPORT16(offs_t port);
	virtual void WRITEPORT16(offs_t port, uint16_t value);
	virtual uint32_t READPORT32(offs_t port);
	virtual void WRITEPORT32(offs_t port, uint32_t value);
	uint32_t i386_load_protected_mode_segment(I386_SREG *seg, uint64_t *desc );
	void i386_load_call_gate(I386_CALL_GATE *gate);
	void i386_set_descriptor_accessed(uint16_t selector);
	void i386_load_segment_descriptor(int segment );
	uint32_t i386_get_stack_segment(uint8_t privilege);
	uint32_t i386_get_stack_ptr(uint8_t privilege);
	uint32_t get_flags() const;
	void set_flags(uint32_t f );
	void sib_byte(uint8_t mod, uint32_t* out_ea, uint8_t* out_segment);
	void modrm_to_EA(uint8_t mod_rm, uint32_t* out_ea, uint8_t* out_segment);
	uint32_t GetNonTranslatedEA(uint8_t modrm,uint8_t *seg);
	uint32_t GetEA(uint8_t modrm, int rwn);
	uint32_t Getx87EA(uint8_t modrm, int rwn);
	void i386_check_sreg_validity(int reg);
	int i386_limit_check(int seg, uint32_t offset, int size = 1);
	void i386_sreg_load(uint16_t selector, uint8_t reg, bool *fault);
	void i386_trap(int irq, int irq_gate);
	void i386_trap_with_error(int irq, int irq_gate, int trap_level, uint32_t error);
	void i286_task_switch(uint16_t selector, uint8_t nested);
	void i386_task_switch(uint16_t selector, uint8_t nested);
	void i386_check_irq_line();
	void i386_protected_mode_jump(uint16_t seg, uint32_t off, int indirect, int operand32);
	void i386_protected_mode_call(uint16_t seg, uint32_t off, int indirect, int operand32);
	void i386_protected_mode_retf(uint8_t count, uint8_t operand32);
	void i386_protected_mode_iret(int operand32);
	void build_cycle_table();
	void report_invalid_opcode();
	void report_invalid_modrm(const char* opcode, uint8_t modrm);
	void i386_decode_opcode();
	void i386_decode_two_byte();
	void i386_decode_three_byte38();
	void i386_decode_three_byte3a();
	void i386_decode_three_byte66();
	void i386_decode_three_bytef2();
	void i386_decode_three_bytef3();
	void i386_decode_four_byte3866();
	void i386_decode_four_byte3a66();
	void i386_decode_four_byte38f2();
	void i386_decode_four_byte3af2();
	void i386_decode_four_byte38f3();
	uint8_t read8_debug(uint32_t ea, uint8_t *data);
	uint32_t i386_get_debug_desc(I386_SREG *seg);
	void CYCLES(int x);
	inline void CYCLES_RM(int modrm, int r, int m);
	uint8_t i386_shift_rotate8(uint8_t modrm, uint32_t value, uint8_t shift);
	void i386_adc_rm8_r8();
	void i386_adc_r8_rm8();
	void i386_adc_al_i8();
	void i386_add_rm8_r8();
	void i386_add_r8_rm8();
	void i386_add_al_i8();
	void i386_and_rm8_r8();
	void i386_and_r8_rm8();
	void i386_and_al_i8();
	void i386_clc();
	void i386_cld();
	void i386_cli();
	void i386_cmc();
	void i386_cmp_rm8_r8();
	void i386_cmp_r8_rm8();
	void i386_cmp_al_i8();
	void i386_cmpsb();
	void i386_in_al_i8();
	void i386_in_al_dx();
	void i386_ja_rel8();
	void i386_jbe_rel8();
	void i386_jc_rel8();
	void i386_jg_rel8();
	void i386_jge_rel8();
	void i386_jl_rel8();
	void i386_jle_rel8();
	void i386_jnc_rel8();
	void i386_jno_rel8();
	void i386_jnp_rel8();
	void i386_jns_rel8();
	void i386_jnz_rel8();
	void i386_jo_rel8();
	void i386_jp_rel8();
	void i386_js_rel8();
	void i386_jz_rel8();
	void i386_jmp_rel8();
	void i386_lahf();
	void i386_lodsb();
	void i386_mov_rm8_r8();
	void i386_mov_r8_rm8();
	void i386_mov_rm8_i8();
	void i386_mov_r32_cr();
	void i386_mov_r32_dr();
	void i386_mov_cr_r32();
	void i386_mov_dr_r32();
	void i386_mov_al_m8();
	void i386_mov_m8_al();
	void i386_mov_rm16_sreg();
	void i386_mov_sreg_rm16();
	void i386_mov_al_i8();
	void i386_mov_cl_i8();
	void i386_mov_dl_i8();
	void i386_mov_bl_i8();
	void i386_mov_ah_i8();
	void i386_mov_ch_i8();
	void i386_mov_dh_i8();
	void i386_mov_bh_i8();
	void i386_movsb();
	void i386_or_rm8_r8();
	void i386_or_r8_rm8();
	void i386_or_al_i8();
	void i386_out_al_i8();
	void i386_out_al_dx();
	void i386_arpl();
	void i386_push_i8();
	void i386_ins_generic(int size);
	void i386_insb();
	void i386_insw();
	void i386_insd();
	void i386_outs_generic(int size);
	void i386_outsb();
	void i386_outsw();
	void i386_outsd();
	void i386_repeat(int invert_flag);
	void i386_rep();
	void i386_repne();
	void i386_sahf();
	void i386_sbb_rm8_r8();
	void i386_sbb_r8_rm8();
	void i386_sbb_al_i8();
	void i386_scasb();
	void i386_setalc();
	void i386_seta_rm8();
	void i386_setbe_rm8();
	void i386_setc_rm8();
	void i386_setg_rm8();
	void i386_setge_rm8();
	void i386_setl_rm8();
	void i386_setle_rm8();
	void i386_setnc_rm8();
	void i386_setno_rm8();
	void i386_setnp_rm8();
	void i386_setns_rm8();
	void i386_setnz_rm8();
	void i386_seto_rm8();
	void i386_setp_rm8();
	void i386_sets_rm8();
	void i386_setz_rm8();
	void i386_stc();
	void i386_std();
	void i386_sti();
	void i386_stosb();
	void i386_sub_rm8_r8();
	void i386_sub_r8_rm8();
	void i386_sub_al_i8();
	void i386_test_al_i8();
	void i386_test_rm8_r8();
	void i386_xchg_r8_rm8();
	void i386_xor_rm8_r8();
	void i386_xor_r8_rm8();
	void i386_xor_al_i8();
	void i386_group80_8();
	void i386_groupC0_8();
	void i386_groupD0_8();
	void i386_groupD2_8();
	void i386_groupF6_8();
	void i386_groupFE_8();
	void i386_segment_CS();
	void i386_segment_DS();
	void i386_segment_ES();
	void i386_segment_FS();
	void i386_segment_GS();
	void i386_segment_SS();
	void i386_operand_size();
	void i386_address_size();
	void i386_nop();
	void i386_int3();
	void i386_int();
	void i386_into();
	void i386_escape();
	void i386_hlt();
	void i386_decimal_adjust(int direction);
	void i386_daa();
	void i386_das();
	void i386_aaa();
	void i386_aas();
	void i386_aad();
	void i386_aam();
	void i386_clts();
	void i386_wait();
	void i486_wait();
	void i386_lock();
	void i386_mov_r32_tr();
	void i386_mov_tr_r32();
	void i386_loadall();
	void i386_invalid();
	void i386_xlat();
	uint16_t i386_shift_rotate16(uint8_t modrm, uint32_t value, uint8_t shift);
	void i386_adc_rm16_r16();
	void i386_adc_r16_rm16();
	void i386_adc_ax_i16();
	void i386_add_rm16_r16();
	void i386_add_r16_rm16();
	void i386_add_ax_i16();
	void i386_and_rm16_r16();
	void i386_and_r16_rm16();
	void i386_and_ax_i16();
	void i386_bsf_r16_rm16();
	void i386_bsr_r16_rm16();
	void i386_bt_rm16_r16();
	void i386_btc_rm16_r16();
	void i386_btr_rm16_r16();
	void i386_bts_rm16_r16();
	void i386_call_abs16();
	void i386_call_rel16();
	void i386_cbw();
	void i386_cmp_rm16_r16();
	void i386_cmp_r16_rm16();
	void i386_cmp_ax_i16();
	void i386_cmpsw();
	void i386_cwd();
	void i386_dec_ax();
	void i386_dec_cx();
	void i386_dec_dx();
	void i386_dec_bx();
	void i386_dec_sp();
	void i386_dec_bp();
	void i386_dec_si();
	void i386_dec_di();
	void i386_imul_r16_rm16();
	void i386_imul_r16_rm16_i16();
	void i386_imul_r16_rm16_i8();
	void i386_in_ax_i8();
	void i386_in_ax_dx();
	void i386_inc_ax();
	void i386_inc_cx();
	void i386_inc_dx();
	void i386_inc_bx();
	void i386_inc_sp();
	void i386_inc_bp();
	void i386_inc_si();
	void i386_inc_di();
	void i386_iret16();
	void i386_ja_rel16();
	void i386_jbe_rel16();
	void i386_jc_rel16();
	void i386_jg_rel16();
	void i386_jge_rel16();
	void i386_jl_rel16();
	void i386_jle_rel16();
	void i386_jnc_rel16();
	void i386_jno_rel16();
	void i386_jnp_rel16();
	void i386_jns_rel16();
	void i386_jnz_rel16();
	void i386_jo_rel16();
	void i386_jp_rel16();
	void i386_js_rel16();
	void i386_jz_rel16();
	void i386_jcxz16();
	void i386_jmp_rel16();
	void i386_jmp_abs16();
	void i386_lea16();
	void i386_enter16();
	void i386_leave16();
	void i386_lodsw();
	void i386_loop16();
	void i386_loopne16();
	void i386_loopz16();
	void i386_mov_rm16_r16();
	void i386_mov_r16_rm16();
	void i386_mov_rm16_i16();
	void i386_mov_ax_m16();
	void i386_mov_m16_ax();
	void i386_mov_ax_i16();
	void i386_mov_cx_i16();
	void i386_mov_dx_i16();
	void i386_mov_bx_i16();
	void i386_mov_sp_i16();
	void i386_mov_bp_i16();
	void i386_mov_si_i16();
	void i386_mov_di_i16();
	void i386_movsw();
	void i386_movsx_r16_rm8();
	void i386_movzx_r16_rm8();
	void i386_or_rm16_r16();
	void i386_or_r16_rm16();
	void i386_or_ax_i16();
	void i386_out_ax_i8();
	void i386_out_ax_dx();
	void i386_pop_ax();
	void i386_pop_cx();
	void i386_pop_dx();
	void i386_pop_bx();
	void i386_pop_sp();
	void i386_pop_bp();
	void i386_pop_si();
	void i386_pop_di();
	bool i386_pop_seg16(int segment);
	void i386_pop_ds16();
	void i386_pop_es16();
	void i386_pop_fs16();
	void i386_pop_gs16();
	void i386_pop_ss16();
	void i386_pop_rm16();
	void i386_popa();
	void i386_popf();
	void i386_push_ax();
	void i386_push_cx();
	void i386_push_dx();
	void i386_push_bx();
	void i386_push_sp();
	void i386_push_bp();
	void i386_push_si();
	void i386_push_di();
	void i386_push_cs16();
	void i386_push_ds16();
	void i386_push_es16();
	void i386_push_fs16();
	void i386_push_gs16();
	void i386_push_ss16();
	void i386_push_i16();
	void i386_pusha();
	void i386_pushf();
	void i386_ret_near16_i16();
	void i386_ret_near16();
	void i386_sbb_rm16_r16();
	void i386_sbb_r16_rm16();
	void i386_sbb_ax_i16();
	void i386_scasw();
	void i386_shld16_i8();
	void i386_shld16_cl();
	void i386_shrd16_i8();
	void i386_shrd16_cl();
	void i386_stosw();
	void i386_sub_rm16_r16();
	void i386_sub_r16_rm16();
	void i386_sub_ax_i16();
	void i386_test_ax_i16();
	void i386_test_rm16_r16();
	void i386_xchg_ax_cx();
	void i386_xchg_ax_dx();
	void i386_xchg_ax_bx();
	void i386_xchg_ax_sp();
	void i386_xchg_ax_bp();
	void i386_xchg_ax_si();
	void i386_xchg_ax_di();
	void i386_xchg_r16_rm16();
	void i386_xor_rm16_r16();
	void i386_xor_r16_rm16();
	void i386_xor_ax_i16();
	void i386_group81_16();
	void i386_group83_16();
	void i386_groupC1_16();
	void i386_groupD1_16();
	void i386_groupD3_16();
	void i386_groupF7_16();
	void i386_groupFF_16();
	void i386_group0F00_16();
	void i386_group0F01_16();
	void i386_group0FBA_16();
	void i386_lar_r16_rm16();
	void i386_lsl_r16_rm16();
	void i386_bound_r16_m16_m16();
	void i386_retf16();
	void i386_retf_i16();
	bool i386_load_far_pointer16(int s);
	void i386_lds16();
	void i386_lss16();
	void i386_les16();
	void i386_lfs16();
	void i386_lgs16();
	uint32_t i386_shift_rotate32(uint8_t modrm, uint32_t value, uint8_t shift);
	void i386_adc_rm32_r32();
	void i386_adc_r32_rm32();
	void i386_adc_eax_i32();
	void i386_add_rm32_r32();
	void i386_add_r32_rm32();
	void i386_add_eax_i32();
	void i386_and_rm32_r32();
	void i386_and_r32_rm32();
	void i386_and_eax_i32();
	void i386_bsf_r32_rm32();
	void i386_bsr_r32_rm32();
	void i386_bt_rm32_r32();
	void i386_btc_rm32_r32();
	void i386_btr_rm32_r32();
	void i386_bts_rm32_r32();
	void i386_call_abs32();
	void i386_call_rel32();
	void i386_cdq();
	void i386_cmp_rm32_r32();
	void i386_cmp_r32_rm32();
	void i386_cmp_eax_i32();
	void i386_cmpsd();
	void i386_cwde();
	void i386_dec_eax();
	void i386_dec_ecx();
	void i386_dec_edx();
	void i386_dec_ebx();
	void i386_dec_esp();
	void i386_dec_ebp();
	void i386_dec_esi();
	void i386_dec_edi();
	void i386_imul_r32_rm32();
	void i386_imul_r32_rm32_i32();
	void i386_imul_r32_rm32_i8();
	void i386_in_eax_i8();
	void i386_in_eax_dx();
	void i386_inc_eax();
	void i386_inc_ecx();
	void i386_inc_edx();
	void i386_inc_ebx();
	void i386_inc_esp();
	void i386_inc_ebp();
	void i386_inc_esi();
	void i386_inc_edi();
	void i386_iret32();
	void i386_ja_rel32();
	void i386_jbe_rel32();
	void i386_jc_rel32();
	void i386_jg_rel32();
	void i386_jge_rel32();
	void i386_jl_rel32();
	void i386_jle_rel32();
	void i386_jnc_rel32();
	void i386_jno_rel32();
	void i386_jnp_rel32();
	void i386_jns_rel32();
	void i386_jnz_rel32();
	void i386_jo_rel32();
	void i386_jp_rel32();
	void i386_js_rel32();
	void i386_jz_rel32();
	void i386_jcxz32();
	void i386_jmp_rel32();
	void i386_jmp_abs32();
	void i386_lea32();
	void i386_enter32();
	void i386_leave32();
	void i386_lodsd();
	void i386_loop32();
	void i386_loopne32();
	void i386_loopz32();
	void i386_mov_rm32_r32();
	void i386_mov_r32_rm32();
	void i386_mov_rm32_i32();
	void i386_mov_eax_m32();
	void i386_mov_m32_eax();
	void i386_mov_eax_i32();
	void i386_mov_ecx_i32();
	void i386_mov_edx_i32();
	void i386_mov_ebx_i32();
	void i386_mov_esp_i32();
	void i386_mov_ebp_i32();
	void i386_mov_esi_i32();
	void i386_mov_edi_i32();
	void i386_movsd();
	void i386_movsx_r32_rm8();
	void i386_movsx_r32_rm16();
	void i386_movzx_r32_rm8();
	void i386_movzx_r32_rm16();
	void i386_or_rm32_r32();
	void i386_or_r32_rm32();
	void i386_or_eax_i32();
	void i386_out_eax_i8();
	void i386_out_eax_dx();
	void i386_pop_eax();
	void i386_pop_ecx();
	void i386_pop_edx();
	void i386_pop_ebx();
	void i386_pop_esp();
	void i386_pop_ebp();
	void i386_pop_esi();
	void i386_pop_edi();
	bool i386_pop_seg32(int segment);
	void i386_pop_ds32();
	void i386_pop_es32();
	void i386_pop_fs32();
	void i386_pop_gs32();
	void i386_pop_ss32();
	void i386_pop_rm32();
	void i386_popad();
	void i386_popfd();
	void i386_push_eax();
	void i386_push_ecx();
	void i386_push_edx();
	void i386_push_ebx();
	void i386_push_esp();
	void i386_push_ebp();
	void i386_push_esi();
	void i386_push_edi();
	void i386_push_cs32();
	void i386_push_ds32();
	void i386_push_es32();
	void i386_push_fs32();
	void i386_push_gs32();
	void i386_push_ss32();
	void i386_push_i32();
	void i386_pushad();
	void i386_pushfd();
	void i386_ret_near32_i16();
	void i386_ret_near32();
	void i386_sbb_rm32_r32();
	void i386_sbb_r32_rm32();
	void i386_sbb_eax_i32();
	void i386_scasd();
	void i386_shld32_i8();
	void i386_shld32_cl();
	void i386_shrd32_i8();
	void i386_shrd32_cl();
	void i386_stosd();
	void i386_sub_rm32_r32();
	void i386_sub_r32_rm32();
	void i386_sub_eax_i32();
	void i386_test_eax_i32();
	void i386_test_rm32_r32();
	void i386_xchg_eax_ecx();
	void i386_xchg_eax_edx();
	void i386_xchg_eax_ebx();
	void i386_xchg_eax_esp();
	void i386_xchg_eax_ebp();
	void i386_xchg_eax_esi();
	void i386_xchg_eax_edi();
	void i386_xchg_r32_rm32();
	void i386_xor_rm32_r32();
	void i386_xor_r32_rm32();
	void i386_xor_eax_i32();
	void i386_group81_32();
	void i386_group83_32();
	void i386_groupC1_32();
	void i386_groupD1_32();
	void i386_groupD3_32();
	void i386_groupF7_32();
	void i386_groupFF_32();
	void i386_group0F00_32();
	void i386_group0F01_32();
	void i386_group0FBA_32();
	void i386_lar_r32_rm32();
	void i386_lsl_r32_rm32();
	void i386_bound_r32_m32_m32();
	void i386_retf32();
	void i386_retf_i32();
	bool i386_load_far_pointer32(int s);
	void i386_lds32();
	void i386_lss32();
	void i386_les32();
	void i386_lfs32();
	void i386_lgs32();
	void i486_cpuid();
	void i486_invd();
	void i486_wbinvd();
	void i486_cmpxchg_rm8_r8();
	void i486_cmpxchg_rm16_r16();
	void i486_cmpxchg_rm32_r32();
	void i486_xadd_rm8_r8();
	void i486_xadd_rm16_r16();
	void i486_xadd_rm32_r32();
	void i486_group0F01_16();
	void i486_group0F01_32();
	void i486_bswap_eax();
	void i486_bswap_ecx();
	void i486_bswap_edx();
	void i486_bswap_ebx();
	void i486_bswap_esp();
	void i486_bswap_ebp();
	void i486_bswap_esi();
	void i486_bswap_edi();
	void i486_mov_cr_r32();
	inline bool MMXPROLOG();
	inline bool SSEPROLOG();
	inline void READMMX(uint32_t ea,MMX_REG &r);
	inline void WRITEMMX(uint32_t ea,MMX_REG &r);
	inline void READXMM(uint32_t ea,XMM_REG &r);
	inline void WRITEXMM(uint32_t ea,XMM_REG &r);
	inline void READXMM_LO64(uint32_t ea,XMM_REG &r);
	inline void WRITEXMM_LO64(uint32_t ea,XMM_REG &r);
	inline void READXMM_HI64(uint32_t ea,XMM_REG &r);
	inline void WRITEXMM_HI64(uint32_t ea,XMM_REG &r);
	void pentium_rdmsr();
	void pentium_wrmsr();
	void pentium_rdtsc();
	void pentium_ud2();
	void pentium_rsm();
	void pentium_prefetch_m8();
	void pentium_cmovo_r16_rm16();
	void pentium_cmovo_r32_rm32();
	void pentium_cmovno_r16_rm16();
	void pentium_cmovno_r32_rm32();
	void pentium_cmovb_r16_rm16();
	void pentium_cmovb_r32_rm32();
	void pentium_cmovae_r16_rm16();
	void pentium_cmovae_r32_rm32();
	void pentium_cmove_r16_rm16();
	void pentium_cmove_r32_rm32();
	void pentium_cmovne_r16_rm16();
	void pentium_cmovne_r32_rm32();
	void pentium_cmovbe_r16_rm16();
	void pentium_cmovbe_r32_rm32();
	void pentium_cmova_r16_rm16();
	void pentium_cmova_r32_rm32();
	void pentium_cmovs_r16_rm16();
	void pentium_cmovs_r32_rm32();
	void pentium_cmovns_r16_rm16();
	void pentium_cmovns_r32_rm32();
	void pentium_cmovp_r16_rm16();
	void pentium_cmovp_r32_rm32();
	void pentium_cmovnp_r16_rm16();
	void pentium_cmovnp_r32_rm32();
	void pentium_cmovl_r16_rm16();
	void pentium_cmovl_r32_rm32();
	void pentium_cmovge_r16_rm16();
	void pentium_cmovge_r32_rm32();
	void pentium_cmovle_r16_rm16();
	void pentium_cmovle_r32_rm32();
	void pentium_cmovg_r16_rm16();
	void pentium_cmovg_r32_rm32();
	void pentium_movnti_m16_r16();
	void pentium_movnti_m32_r32();
	void i386_cyrix_special();
	void i386_cyrix_unknown();
	void pentium_cmpxchg8b_m64();
	void pentium_movntq_m64_r64();
	void pentium_maskmovq_r64_r64();
	void pentium_popcnt_r16_rm16();
	void pentium_popcnt_r32_rm32();
	void pentium_tzcnt_r16_rm16();
	void pentium_tzcnt_r32_rm32();
	void mmx_group_0f71();
	void mmx_group_0f72();
	void mmx_group_0f73();
	void mmx_psrlw_r64_rm64();
	void mmx_psrld_r64_rm64();
	void mmx_psrlq_r64_rm64();
	void mmx_paddq_r64_rm64();
	void mmx_pmullw_r64_rm64();
	void mmx_psubusb_r64_rm64();
	void mmx_psubusw_r64_rm64();
	void mmx_pand_r64_rm64();
	void mmx_paddusb_r64_rm64();
	void mmx_paddusw_r64_rm64();
	void mmx_pandn_r64_rm64();
	void mmx_psraw_r64_rm64();
	void mmx_psrad_r64_rm64();
	void mmx_pmulhw_r64_rm64();
	void mmx_psubsb_r64_rm64();
	void mmx_psubsw_r64_rm64();
	void mmx_por_r64_rm64();
	void mmx_paddsb_r64_rm64();
	void mmx_paddsw_r64_rm64();
	void mmx_pxor_r64_rm64();
	void mmx_psllw_r64_rm64();
	void mmx_pslld_r64_rm64();
	void mmx_psllq_r64_rm64();
	void mmx_pmaddwd_r64_rm64();
	void mmx_psubb_r64_rm64();
	void mmx_psubw_r64_rm64();
	void mmx_psubd_r64_rm64();
	void mmx_paddb_r64_rm64();
	void mmx_paddw_r64_rm64();
	void mmx_paddd_r64_rm64();
	void mmx_emms();
	void i386_cyrix_svdc();
	void i386_cyrix_rsdc();
	void i386_cyrix_svldt();
	void i386_cyrix_rsldt();
	void i386_cyrix_svts();
	void i386_cyrix_rsts();
	void mmx_movd_r64_rm32();
	void mmx_movq_r64_rm64();
	void mmx_movd_rm32_r64();
	void mmx_movq_rm64_r64();
	void mmx_pcmpeqb_r64_rm64();
	void mmx_pcmpeqw_r64_rm64();
	void mmx_pcmpeqd_r64_rm64();
	void mmx_pshufw_r64_rm64_i8();
	void mmx_punpcklbw_r64_r64m32();
	void mmx_punpcklwd_r64_r64m32();
	void mmx_punpckldq_r64_r64m32();
	void mmx_packsswb_r64_rm64();
	void mmx_pcmpgtb_r64_rm64();
	void mmx_pcmpgtw_r64_rm64();
	void mmx_pcmpgtd_r64_rm64();
	void mmx_packuswb_r64_rm64();
	void mmx_punpckhbw_r64_rm64();
	void mmx_punpckhwd_r64_rm64();
	void mmx_punpckhdq_r64_rm64();
	void mmx_packssdw_r64_rm64();
	void sse_group_0fae();
	void sse_group_660f71();
	void sse_group_660f72();
	void sse_group_660f73();
	void sse_cvttps2dq_r128_rm128();
	void sse_cvtss2sd_r128_r128m32();
	void sse_cvttss2si_r32_r128m32();
	void sse_cvtss2si_r32_r128m32();
	void sse_cvtsi2ss_r128_rm32();
	void sse_cvtpi2ps_r128_rm64();
	void sse_cvttps2pi_r64_r128m64();
	void sse_cvtps2pi_r64_r128m64();
	void sse_cvtps2pd_r128_r128m64();
	void sse_cvtdq2ps_r128_rm128();
	void sse_cvtdq2pd_r128_r128m64();
	void sse_movss_r128_rm128();
	void sse_movss_rm128_r128();
	void sse_movsldup_r128_rm128();
	void sse_movshdup_r128_rm128();
	void sse_movaps_r128_rm128();
	void sse_movaps_rm128_r128();
	void sse_movups_r128_rm128();
	void sse_movups_rm128_r128();
	void sse_movlps_r128_m64();
	void sse_movlps_m64_r128();
	void sse_movhps_r128_m64();
	void sse_movhps_m64_r128();
	void sse_movntps_m128_r128();
	void sse_movmskps_r16_r128();
	void sse_movmskps_r32_r128();
	void sse_movq2dq_r128_r64();
	void sse_movdqu_r128_rm128();
	void sse_movdqu_rm128_r128();
	void sse_movd_m128_rm32();
	void sse_movdqa_m128_rm128();
	void sse_movq_r128_r128m64();
	void sse_movd_rm32_r128();
	void sse_movdqa_rm128_r128();
	void sse_pmovmskb_r16_r64();
	void sse_pmovmskb_r32_r64();
	void sse_xorps();
	void sse_addps();
	void sse_sqrtps_r128_rm128();
	void sse_rsqrtps_r128_rm128();
	void sse_rcpps_r128_rm128();
	void sse_andps_r128_rm128();
	void sse_andnps_r128_rm128();
	void sse_orps_r128_rm128();
	void sse_mulps();
	void sse_subps();
	void sse_minps();
	void sse_divps();
	void sse_maxps();
	void sse_maxss_r128_r128m32();
	void sse_addss();
	void sse_subss();
	void sse_mulss();
	void sse_divss();
	void sse_rcpss_r128_r128m32();
	void sse_sqrtss_r128_r128m32();
	void sse_rsqrtss_r128_r128m32();
	void sse_minss_r128_r128m32();
	void sse_comiss_r128_r128m32();
	void sse_ucomiss_r128_r128m32();
	void sse_shufps();
	void sse_punpcklbw_r128_rm128();
	void sse_punpcklwd_r128_rm128();
	void sse_punpckldq_r128_rm128();
	void sse_punpcklqdq_r128_rm128();
	void sse_unpcklps_r128_rm128();
	void sse_unpckhps_r128_rm128();
	void sse_cmpps_r128_rm128_i8();
	void sse_cmpss_r128_r128m32_i8();
	void sse_pinsrw_r64_r16m16_i8();
	void sse_pinsrw_r64_r32m16_i8();
	void sse_pinsrw_r128_r32m16_i8();
	void sse_pextrw_r16_r64_i8();
	void sse_pextrw_r32_r64_i8();
	void sse_pextrw_reg_r128_i8();
	void sse_pminub_r64_rm64();
	void sse_pmaxub_r64_rm64();
	void sse_pavgb_r64_rm64();
	void sse_pavgw_r64_rm64();
	void sse_pmulhuw_r64_rm64();
	void sse_pminsw_r64_rm64();
	void sse_pmaxsw_r64_rm64();
	void sse_pmuludq_r64_rm64();
	void sse_psadbw_r64_rm64();
	void sse_psubq_r64_rm64();
	void sse_pshufhw_r128_rm128_i8();
	void sse_packsswb_r128_rm128();
	void sse_packssdw_r128_rm128();
	void sse_pcmpgtb_r128_rm128();
	void sse_pcmpgtw_r128_rm128();
	void sse_pcmpgtd_r128_rm128();
	void sse_packuswb_r128_rm128();
	void sse_punpckhbw_r128_rm128();
	void sse_punpckhwd_r128_rm128();
	void sse_unpckhdq_r128_rm128();
	void sse_punpckhqdq_r128_rm128();
	void sse_pcmpeqb_r128_rm128();
	void sse_pcmpeqw_r128_rm128();
	void sse_pcmpeqd_r128_rm128();
	void sse_paddq_r128_rm128();
	void sse_pmullw_r128_rm128();
	void sse_pmuludq_r128_rm128();
	void sse_psubq_r128_rm128();
	void sse_paddb_r128_rm128();
	void sse_paddw_r128_rm128();
	void sse_paddd_r128_rm128();
	void sse_psubusb_r128_rm128();
	void sse_psubusw_r128_rm128();
	void sse_pminub_r128_rm128();
	void sse_pand_r128_rm128();
	void sse_pandn_r128_rm128();
	void sse_paddusb_r128_rm128();
	void sse_paddusw_r128_rm128();
	void sse_pmaxub_r128_rm128();
	void sse_pmulhuw_r128_rm128();
	void sse_pmulhw_r128_rm128();
	void sse_psubsw_r128_rm128();
	void sse_psubsb_r128_rm128();
	void sse_pminsw_r128_rm128();
	void sse_pmaxsw_r128_rm128();
	void sse_paddsb_r128_rm128();
	void sse_paddsw_r128_rm128();
	void sse_por_r128_rm128();
	void sse_pxor_r128_rm128();
	void sse_pmaddwd_r128_rm128();
	void sse_psubb_r128_rm128();
	void sse_psubw_r128_rm128();
	void sse_psubd_r128_rm128();
	void sse_psadbw_r128_rm128();
	void sse_pavgb_r128_rm128();
	void sse_pavgw_r128_rm128();
	void sse_pmovmskb_r32_r128();
	void sse_maskmovdqu_r128_r128();
	void sse_andpd_r128_rm128();
	void sse_andnpd_r128_rm128();
	void sse_orpd_r128_rm128();
	void sse_xorpd_r128_rm128();
	void sse_unpcklpd_r128_rm128();
	void sse_unpckhpd_r128_rm128();
	void sse_shufpd_r128_rm128_i8();
	void sse_pshufd_r128_rm128_i8();
	void sse_pshuflw_r128_rm128_i8();
	void sse_movmskpd_r32_r128();
	void sse_ucomisd_r128_r128m64();
	void sse_comisd_r128_r128m64();
	void sse_psrlw_r128_rm128();
	void sse_psrld_r128_rm128();
	void sse_psrlq_r128_rm128();
	void sse_psllw_r128_rm128();
	void sse_pslld_r128_rm128();
	void sse_psllq_r128_rm128();
	void sse_psraw_r128_rm128();
	void sse_psrad_r128_rm128();
	void sse_movntdq_m128_r128();
	void sse_cvttpd2dq_r128_rm128();
	void sse_movq_r128m64_r128();
	void sse_addsubpd_r128_rm128();
	void sse_cmppd_r128_rm128_i8();
	void sse_haddpd_r128_rm128();
	void sse_hsubpd_r128_rm128();
	void sse_sqrtpd_r128_rm128();
	void sse_cvtpi2pd_r128_rm64();
	void sse_cvttpd2pi_r64_rm128();
	void sse_cvtpd2pi_r64_rm128();
	void sse_cvtpd2ps_r128_rm128();
	void sse_cvtps2dq_r128_rm128();
	void sse_addpd_r128_rm128();
	void sse_mulpd_r128_rm128();
	void sse_subpd_r128_rm128();
	void sse_minpd_r128_rm128();
	void sse_divpd_r128_rm128();
	void sse_maxpd_r128_rm128();
	void sse_movntpd_m128_r128();
	void sse_movapd_r128_rm128();
	void sse_movapd_rm128_r128();
	void sse_movhpd_r128_m64();
	void sse_movhpd_m64_r128();
	void sse_movupd_r128_rm128();
	void sse_movupd_rm128_r128();
	void sse_movlpd_r128_m64();
	void sse_movlpd_m64_r128();
	void sse_movsd_r128_r128m64();
	void sse_movsd_r128m64_r128();
	void sse_movddup_r128_r128m64();
	void sse_cvtsi2sd_r128_rm32();
	void sse_cvttsd2si_r32_r128m64();
	void sse_cvtsd2si_r32_r128m64();
	void sse_sqrtsd_r128_r128m64();
	void sse_addsd_r128_r128m64();
	void sse_mulsd_r128_r128m64();
	void sse_cvtsd2ss_r128_r128m64();
	void sse_subsd_r128_r128m64();
	void sse_minsd_r128_r128m64();
	void sse_divsd_r128_r128m64();
	void sse_maxsd_r128_r128m64();
	void sse_haddps_r128_rm128();
	void sse_hsubps_r128_rm128();
	void sse_cmpsd_r128_r128m64_i8();
	void sse_addsubps_r128_rm128();
	void sse_movdq2q_r64_r128();
	void sse_cvtpd2dq_r128_rm128();
	void sse_lddqu_r128_m128();
	inline void sse_predicate_compare_single(uint8_t imm8, XMM_REG d, XMM_REG s);
	inline void sse_predicate_compare_double(uint8_t imm8, XMM_REG d, XMM_REG s);
	inline void sse_predicate_compare_single_scalar(uint8_t imm8, XMM_REG d, XMM_REG s);
	inline void sse_predicate_compare_double_scalar(uint8_t imm8, XMM_REG d, XMM_REG s);
	inline extFloat80_t READ80(uint32_t ea);
	inline void WRITE80(uint32_t ea, extFloat80_t t);
	inline void x87_set_stack_top(int top);
	inline void x87_set_tag(int reg, int tag);
	void x87_write_stack(int i, extFloat80_t value, bool update_tag);
	inline void x87_set_stack_underflow();
	inline void x87_set_stack_overflow();
	int x87_inc_stack();
	int x87_dec_stack();
	int x87_ck_over_stack();
	int x87_check_exceptions(bool store = false);
	int x87_mf_fault();
	inline void x87_write_cw(uint16_t cw);
	void x87_reset();
	extFloat80_t x87_add(extFloat80_t a, extFloat80_t b);
	extFloat80_t x87_sub(extFloat80_t a, extFloat80_t b);
	extFloat80_t x87_mul(extFloat80_t a, extFloat80_t b);
	extFloat80_t x87_div(extFloat80_t a, extFloat80_t b);
	void x87_fadd_m32real(uint8_t modrm);
	void x87_fadd_m64real(uint8_t modrm);
	void x87_fadd_st_sti(uint8_t modrm);
	void x87_fadd_sti_st(uint8_t modrm);
	void x87_faddp(uint8_t modrm);
	void x87_fiadd_m32int(uint8_t modrm);
	void x87_fiadd_m16int(uint8_t modrm);
	void x87_fsub_m32real(uint8_t modrm);
	void x87_fsub_m64real(uint8_t modrm);
	void x87_fsub_st_sti(uint8_t modrm);
	void x87_fsub_sti_st(uint8_t modrm);
	void x87_fsubp(uint8_t modrm);
	void x87_fisub_m32int(uint8_t modrm);
	void x87_fisub_m16int(uint8_t modrm);
	void x87_fsubr_m32real(uint8_t modrm);
	void x87_fsubr_m64real(uint8_t modrm);
	void x87_fsubr_st_sti(uint8_t modrm);
	void x87_fsubr_sti_st(uint8_t modrm);
	void x87_fsubrp(uint8_t modrm);
	void x87_fisubr_m32int(uint8_t modrm);
	void x87_fisubr_m16int(uint8_t modrm);
	void x87_fdiv_m32real(uint8_t modrm);
	void x87_fdiv_m64real(uint8_t modrm);
	void x87_fdiv_st_sti(uint8_t modrm);
	void x87_fdiv_sti_st(uint8_t modrm);
	void x87_fdivp(uint8_t modrm);
	void x87_fidiv_m32int(uint8_t modrm);
	void x87_fidiv_m16int(uint8_t modrm);
	void x87_fdivr_m32real(uint8_t modrm);
	void x87_fdivr_m64real(uint8_t modrm);
	void x87_fdivr_st_sti(uint8_t modrm);
	void x87_fdivr_sti_st(uint8_t modrm);
	void x87_fdivrp(uint8_t modrm);
	void x87_fidivr_m32int(uint8_t modrm);
	void x87_fidivr_m16int(uint8_t modrm);
	void x87_fmul_m32real(uint8_t modrm);
	void x87_fmul_m64real(uint8_t modrm);
	void x87_fmul_st_sti(uint8_t modrm);
	void x87_fmul_sti_st(uint8_t modrm);
	void x87_fmulp(uint8_t modrm);
	void x87_fimul_m32int(uint8_t modrm);
	void x87_fimul_m16int(uint8_t modrm);
	void x87_fprem(uint8_t modrm);
	void x87_fprem1(uint8_t modrm);
	void x87_fsqrt(uint8_t modrm);
	void x87_f2xm1(uint8_t modrm);
	void x87_fyl2x(uint8_t modrm);
	void x87_fyl2xp1(uint8_t modrm);
	void x87_fptan(uint8_t modrm);
	void x87_fpatan(uint8_t modrm);
	void x87_fsin(uint8_t modrm);
	void x87_fcos(uint8_t modrm);
	void x87_fsincos(uint8_t modrm);
	void x87_fld_m32real(uint8_t modrm);
	void x87_fld_m64real(uint8_t modrm);
	void x87_fld_m80real(uint8_t modrm);
	void x87_fld_sti(uint8_t modrm);
	void x87_fild_m16int(uint8_t modrm);
	void x87_fild_m32int(uint8_t modrm);
	void x87_fild_m64int(uint8_t modrm);
	void x87_fbld(uint8_t modrm);
	void x87_fst_m32real(uint8_t modrm);
	void x87_fst_m64real(uint8_t modrm);
	void x87_fst_sti(uint8_t modrm);
	void x87_fstp_m32real(uint8_t modrm);
	void x87_fstp_m64real(uint8_t modrm);
	void x87_fstp_m80real(uint8_t modrm);
	void x87_fstp_sti(uint8_t modrm);
	void x87_fist_m16int(uint8_t modrm);
	void x87_fist_m32int(uint8_t modrm);
	void x87_fistp_m16int(uint8_t modrm);
	void x87_fistp_m32int(uint8_t modrm);
	void x87_fistp_m64int(uint8_t modrm);
	void x87_fbstp(uint8_t modrm);
	void x87_fld1(uint8_t modrm);
	void x87_fldl2t(uint8_t modrm);
	void x87_fldl2e(uint8_t modrm);
	void x87_fldpi(uint8_t modrm);
	void x87_fldlg2(uint8_t modrm);
	void x87_fldln2(uint8_t modrm);
	void x87_fldz(uint8_t modrm);
	void x87_fnop(uint8_t modrm);
	void x87_fchs(uint8_t modrm);
	void x87_fabs(uint8_t modrm);
	void x87_fscale(uint8_t modrm);
	void x87_frndint(uint8_t modrm);
	void x87_fxtract(uint8_t modrm);
	void x87_ftst(uint8_t modrm);
	void x87_fxam(uint8_t modrm);
	void x87_fcmovb_sti(uint8_t modrm);
	void x87_fcmove_sti(uint8_t modrm);
	void x87_fcmovbe_sti(uint8_t modrm);
	void x87_fcmovu_sti(uint8_t modrm);
	void x87_fcmovnb_sti(uint8_t modrm);
	void x87_fcmovne_sti(uint8_t modrm);
	void x87_fcmovnbe_sti(uint8_t modrm);
	void x87_fcmovnu_sti(uint8_t modrm);
	void x87_ficom_m16int(uint8_t modrm);
	void x87_ficom_m32int(uint8_t modrm);
	void x87_ficomp_m16int(uint8_t modrm);
	void x87_ficomp_m32int(uint8_t modrm);
	void x87_fcom_m32real(uint8_t modrm);
	void x87_fcom_m64real(uint8_t modrm);
	void x87_fcom_sti(uint8_t modrm);
	void x87_fcomp_m32real(uint8_t modrm);
	void x87_fcomp_m64real(uint8_t modrm);
	void x87_fcomp_sti(uint8_t modrm);
	void x87_fcomi_sti(uint8_t modrm);
	void x87_fcomip_sti(uint8_t modrm);
	void x87_fucomi_sti(uint8_t modrm);
	void x87_fucomip_sti(uint8_t modrm);
	void x87_fcompp(uint8_t modrm);
	void x87_fucom_sti(uint8_t modrm);
	void x87_fucomp_sti(uint8_t modrm);
	void x87_fucompp(uint8_t modrm);
	void x87_fdecstp(uint8_t modrm);
	void x87_fincstp(uint8_t modrm);
	void x87_fclex(uint8_t modrm);
	void x87_ffree(uint8_t modrm);
	void x87_finit(uint8_t modrm);
	void x87_fldcw(uint8_t modrm);
	void x87_fstcw(uint8_t modrm);
	void x87_fldenv(uint8_t modrm);
	void x87_fstenv(uint8_t modrm);
	void x87_fsave(uint8_t modrm);
	void x87_frstor(uint8_t modrm);
	void x87_fxch(uint8_t modrm);
	void x87_fxch_sti(uint8_t modrm);
	void x87_fstsw_ax(uint8_t modrm);
	void x87_fstsw_m2byte(uint8_t modrm);
	void x87_invalid(uint8_t modrm);
	void i386_x87_group_d8();
	void i386_x87_group_d9();
	void i386_x87_group_da();
	void i386_x87_group_db();
	void i386_x87_group_dc();
	void i386_x87_group_dd();
	void i386_x87_group_de();
	void i386_x87_group_df();
	void build_x87_opcode_table_d8();
	void build_x87_opcode_table_d9();
	void build_x87_opcode_table_da();
	void build_x87_opcode_table_db();
	void build_x87_opcode_table_dc();
	void build_x87_opcode_table_dd();
	void build_x87_opcode_table_de();
	void build_x87_opcode_table_df();
	void build_x87_opcode_table();
	void i386_postload();
	void i386_common_init();
	void build_opcode_table(uint32_t features);
	void zero_state();
	void i386_set_a20_line(int state);

};


class i386sx_device : public i386_device
{
public:
	// construction/destruction
	i386sx_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual u8 mem_pr8(offs_t address) override { return macache16.read_byte(address); }
	virtual u16 mem_pr16(offs_t address) override { return macache16.read_word(address); }
	virtual u32 mem_pr32(offs_t address) override { return macache16.read_dword(address); }

	virtual uint16_t READ16PL(uint32_t ea, uint8_t privilege) override;
	virtual uint32_t READ32PL(uint32_t ea, uint8_t privilege) override;
	virtual uint64_t READ64PL(uint32_t ea, uint8_t privilege) override;
	virtual void WRITE16PL(uint32_t ea, uint8_t privilege, uint16_t value) override;
	virtual void WRITE32PL(uint32_t ea, uint8_t privilege, uint32_t value) override;
	virtual void WRITE64PL(uint32_t ea, uint8_t privilege, uint64_t value) override;
	virtual uint16_t READPORT16(offs_t port) override;
	virtual void WRITEPORT16(offs_t port, uint16_t value) override;
	virtual uint32_t READPORT32(offs_t port) override;
	virtual void WRITEPORT32(offs_t port, uint32_t value) override;
};

class i486_device : public i386_device
{
public:
	// construction/destruction
	i486_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	i486_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};

class i486dx4_device : public i486_device
{
public:
	// construction/destruction
	i486dx4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_reset() override ATTR_COLD;
};


class pentium_device : public i386_device
{
public:
	// construction/destruction
	pentium_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	pentium_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual bool execute_input_edge_triggered(int inputnum) const noexcept override { return inputnum == INPUT_LINE_NMI || inputnum == INPUT_LINE_SMI; }
	virtual void execute_set_input(int inputnum, int state) override;
	virtual uint64_t opcode_rdmsr(bool &valid_msr) override;
	virtual void opcode_wrmsr(uint64_t data, bool &valid_msr) override;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


class pentium_mmx_device : public pentium_device
{
public:
	// construction/destruction
	pentium_mmx_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


class mediagx_device : public i386_device
{
public:
	// construction/destruction
	mediagx_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


class pentium_pro_device : public pentium_device
{
public:
	// construction/destruction
	pentium_pro_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	pentium_pro_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual uint64_t opcode_rdmsr(bool &valid_msr) override;
	virtual void opcode_wrmsr(uint64_t data, bool &valid_msr) override;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


class pentium2_device : public pentium_pro_device
{
public:
	// construction/destruction
	pentium2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


class pentium3_device : public pentium_pro_device
{
public:
	// construction/destruction
	pentium3_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void opcode_cpuid() override;
};


class p3celeron_device : public pentium_pro_device
{
public:
	// construction/destruction
	p3celeron_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

	virtual void opcode_cpuid() override;
};


class pentium4_device : public pentium_device
{
public:
	// construction/destruction
	pentium4_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	virtual uint64_t opcode_rdmsr(bool &valid_msr) override;
	virtual void opcode_wrmsr(uint64_t data, bool &valid_msr) override;
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
};


DECLARE_DEVICE_TYPE(I386,        i386_device)
DECLARE_DEVICE_TYPE(I386SX,      i386sx_device)
DECLARE_DEVICE_TYPE(I486,        i486_device)
DECLARE_DEVICE_TYPE(I486DX4,     i486dx4_device)
DECLARE_DEVICE_TYPE(PENTIUM,     pentium_device)
DECLARE_DEVICE_TYPE(PENTIUM_MMX, pentium_mmx_device)
DECLARE_DEVICE_TYPE(MEDIAGX,     mediagx_device)
DECLARE_DEVICE_TYPE(PENTIUM_PRO, pentium_pro_device)
DECLARE_DEVICE_TYPE(PENTIUM2,    pentium2_device)
DECLARE_DEVICE_TYPE(PENTIUM3,    pentium3_device)
DECLARE_DEVICE_TYPE(P3CELERON,   p3celeron_device)
DECLARE_DEVICE_TYPE(PENTIUM4,    pentium4_device)

#endif // MAME_CPU_I386_I386_H
