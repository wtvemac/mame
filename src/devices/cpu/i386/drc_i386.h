// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_CPU_I386_DRC_I386_H
#define MAME_CPU_I386_DRC_I386_H

#pragma once

#include "cpu/drcfe.ipp"
#include "cpu/drcuml.h"
#include "cpu/drcumlsh.h"

#include "i386.h"
#include "i386priv.h"

#include <algorithm>
#include <array>
#include <deque>
#include <unordered_map>

using namespace uml;

static constexpr uint8_t s_rm_sreg32[8] = { DS, DS, DS, DS, SS, SS, DS, DS };
static constexpr uint8_t s_rm_sreg16[8] = { DS, DS, SS, SS, DS, DS, SS, DS };

static constexpr uint8_t UMLF_C = FLAG_C;
static constexpr uint8_t UMLF_V = FLAG_V;
static constexpr uint8_t UMLF_Z = FLAG_Z;
static constexpr uint8_t UMLF_S = FLAG_S;

static constexpr uint32_t VECTOR_FILL_CHUNK_BYTES = 64;

constexpr offs_t CTRANSFER_USE_PC = -1;

// This is ordered to improve performace rather than in any logical order.
// The order is based off a WinCE benchmark on the MSNTV2.
// This makes sure the first item (most hits) is checked first in a CMP/JMP chain.
enum : uint32_t
{
	FLAGS_OPTYPE_UNKNOWN = 0,

	FLAGS_OPTYPE_CMP32,
	FLAGS_OPTYPE_LOGICAL32,
	FLAGS_OPTYPE_CMP8,
	FLAGS_OPTYPE_LOGICAL8,
	FLAGS_OPTYPE_DEC32,

	FLAGS_OPTYPE_ADD32,
	FLAGS_OPTYPE_CMP16,
	FLAGS_OPTYPE_ADD8,
	FLAGS_OPTYPE_ADD16,
	FLAGS_OPTYPE_LOGICAL16,

	FLAGS_OPTYPE_INC32,
	FLAGS_OPTYPE_INC8,
	FLAGS_OPTYPE_DEC8,
	FLAGS_OPTYPE_INC16,
	FLAGS_OPTYPE_DEC16,

	FLAGS_OPTYPE_ADC32,
	FLAGS_OPTYPE_SBB32,
	FLAGS_OPTYPE_ADC8,
	FLAGS_OPTYPE_SBB8,
	FLAGS_OPTYPE_ADC16,
	FLAGS_OPTYPE_SBB16,

	FLAGS_OPTYPE_SHIFT32,
	FLAGS_OPTYPE_SHIFT8,
	FLAGS_OPTYPE_SHIFT16,

	FLAGS_OPTYPE_MAX, // highest valid value
};

enum : uint32_t
{
	FLAGS_CC_O = 0, // overflow                   (OF=1)
	FLAGS_CC_NO,    // not overflow               (OF=0)
	FLAGS_CC_B,     // below / carry              (CF=1)
	FLAGS_CC_AE,    // above or equal / not carry (CF=0)
	FLAGS_CC_Z,     // zero / equal               (ZF=1)
	FLAGS_CC_NZ,    // not zero / not equal       (ZF=0)
	FLAGS_CC_BE,    // below or equal             (CF=1 || ZF=1)
	FLAGS_CC_A,     // above                      (CF=0 && ZF=0)
	FLAGS_CC_S,     // sign                       (SF=1)
	FLAGS_CC_NS,    // not sign                   (SF=0)
	FLAGS_CC_P,     // parity even                (PF=1)
	FLAGS_CC_NP,    // parity odd                 (PF=0)
	FLAGS_CC_L,     // less                       (SF != OF)
	FLAGS_CC_GE,    // greater or equal           (SF == OF)
	FLAGS_CC_LE,    // less or equal              (ZF=1 || SF != OF)
	FLAGS_CC_G,     // greater                    (ZF=0 && SF == OF)

	FLAGS_CC_MAX, // highest valid value
};

enum : uint32_t
{
	FLAGS_CC_MASK       = FLAGS_CC_MAX - 1,
	FLAGS_CC_INVERT_BIT = 0x1,
	FLAGS_CC_SHIFT      = std::bit_width((uint32_t)FLAGS_CC_MAX - 1),
};

enum : uint32_t
{
	// bit 1: reserved
	EFLAG_RESERVED1 = 1 << 1,

	// Carry
	EFLAG_CF_SHIFT   = 0,
	EFLAG_CF         = 1 << EFLAG_CF_SHIFT,
	// Parity
	EFLAG_PF_SHIFT   = 2,
	EFLAG_PF         = 1 << EFLAG_PF_SHIFT,
	// Auxiliary carry
	EFLAG_AF_SHIFT   = 4,
	EFLAG_AF         = 1 << EFLAG_AF_SHIFT,
	// Zero
	EFLAG_ZF_SHIFT   = 6,
	EFLAG_ZF         = 1 << EFLAG_ZF_SHIFT,
	// Sign
	EFLAG_SF_SHIFT   = 7,
	EFLAG_SF         = 1 << EFLAG_SF_SHIFT,
	// Trap
	EFLAG_TF_SHIFT   = 8,
	EFLAG_TF         = 1 << EFLAG_TF_SHIFT,
	// Interrupt enable
	EFLAG_IF_SHIFT   = 9,
	EFLAG_IF         = 1 << EFLAG_IF_SHIFT,
	// Direction
	EFLAG_DF_SHIFT   = 10,
	EFLAG_DF         = 1 << EFLAG_DF_SHIFT,
	// Overflow
	EFLAG_OF_SHIFT   = 11,
	EFLAG_OF         = 1 << EFLAG_OF_SHIFT,
	// I/O privilege level (2-bit field)
	EFLAG_IOPL_SHIFT = 12,
	EFLAG_IOPL       = 3 << EFLAG_IOPL_SHIFT,
	// Nested task
	EFLAG_NT_SHIFT   = 14,
	EFLAG_NT         = 1 << EFLAG_NT_SHIFT,
	// Resume
	EFLAG_RF_SHIFT   = 16,
	EFLAG_RF         = 1 << EFLAG_RF_SHIFT,
	// Virtual 8086
	EFLAG_VM_SHIFT   = 17,
	EFLAG_VM         = 1 << EFLAG_VM_SHIFT,
	// Alignment check
	EFLAG_AC_SHIFT   = 18,
	EFLAG_AC         = 1 << EFLAG_AC_SHIFT,
	// Virtual interrupt
	EFLAG_VIF_SHIFT  = 19,
	EFLAG_VIF        = 1 << EFLAG_VIF_SHIFT,
	// Virtual interrupt pending
	EFLAG_VIP_SHIFT  = 20,
	EFLAG_VIP        = 1 << EFLAG_VIP_SHIFT,
	// Identification
	EFLAG_ID_SHIFT   = 21,
	EFLAG_ID         = 1 << EFLAG_ID_SHIFT,
};

// This is used as a quick directory to evaluate flags in drc_flags_all()

enum class op2_source : uint8_t
{
	none,
	direct,
	one,
	carry
};
struct optype_info
{
	uint8_t    width_bits;
	bool       is_sub;
	bool       is_logical;
	op2_source op2;
};
static constexpr optype_info s_flags_optype_info[] = {
	{ 0,  false, false, op2_source::none   }, // UNKNOWN
	{ 32, true,  false, op2_source::direct }, // CMP32
	{ 32, false, true,  op2_source::none   }, // LOGICAL32
	{ 8,  true,  false, op2_source::direct }, // CMP8
	{ 8,  false, true,  op2_source::none   }, // LOGICAL8
	{ 32, true,  false, op2_source::one    }, // DEC32
	{ 32, false, false, op2_source::direct }, // ADD32
	{ 16, true,  false, op2_source::direct }, // CMP16
	{ 8,  false, false, op2_source::direct }, // ADD8
	{ 16, false, false, op2_source::direct }, // ADD16
	{ 16, false, true,  op2_source::none   }, // LOGICAL16
	{ 32, false, false, op2_source::one    }, // INC32
	{ 8,  false, false, op2_source::one    }, // INC8
	{ 8,  true,  false, op2_source::one    }, // DEC8
	{ 16, false, false, op2_source::one    }, // INC16
	{ 16, true,  false, op2_source::one    }, // DEC16
	{ 32, false, false, op2_source::carry  }, // ADC32
	{ 32, true,  false, op2_source::carry  }, // SBB32
	{ 8,  false, false, op2_source::carry  }, // ADC8
	{ 8,  true,  false, op2_source::carry  }, // SBB8
	{ 16, false, false, op2_source::carry  }, // ADC16
	{ 16, true,  false, op2_source::carry  }, // SBB16
	{ 32, false, true,  op2_source::none   }, // SHIFT32
	{ 8,  false, true,  op2_source::none   }, // SHIFT8
	{ 16, false, true,  op2_source::none   }, // SHIFT16
};


struct i386_device::compiler_state
{
	drcuml_block &block;
	int          &label_ctr;
	bool          block_ended;

	i386_device *cpu    = nullptr;
	offs_t       pc     = 0;
	offs_t       eip    = 0;
	offs_t       cursor = 0;
	uint8_t      mode   = 0;

	int total_cycles   = 0;
	int pending_cycles = 0;

	i386_device::drc_gen_ea_func  gen_ea  = nullptr;
	i386_device::drc_skip_ea_func skip_ea = nullptr;

	const i386_device::opcode_desc *desc = nullptr;

	bool     compile_time_flags_ready  = false;
	uint32_t compile_time_flags_optype = FLAGS_OPTYPE_UNKNOWN;

	// Compile-time register value scratch space
	// Used to store register values during instruction parsing
	// For example one instruction might set a register value then another proceeding instruction might consume it.

	std::array<bool, 8>     rscratch_valid = {};
	std::array<uint32_t, 8> rscratch_value = {};

	inline void set_rscratch(int index, uint32_t value)
	{
		rscratch_valid[index] = true;
		rscratch_value[index] = value;
	}

	inline bool rscratch_is_valid(int index)
	{
		return rscratch_valid[index];
	}

	inline uint32_t get_rscratch(int index)
	{
		return rscratch_value[index];
	}

	inline void invalidate_rscratch()
	{
		std::fill(rscratch_valid.begin(), rscratch_valid.end(), false);
	}
};

#define NEW_SLBL()   (const uml::code_label)(label_ctr++)
#define NEW_LBL(ctx) (const uml::code_label)(ctx.label_ctr++)

#define BREG_DREG(r)                 ((r) >> 2)
#define BREG_SHIFT(r)                (((r) & 1) ? 8 : 0)
#define BREG_MASK(r)                 (((r) & 1) ? 0xff00 : 0xff)
#define UML_BREG_WRITE(b, breg, src) UML_ROLINS((b), DRC_REG32(BREG_DREG(breg)), (src), BREG_SHIFT(breg), BREG_MASK(breg))

#define DRC_PC       uml::mem(&m_core->pc)
#define DRC_EIP      uml::mem(&m_core->eip)
#define DRC_PREV_EIP uml::mem(&m_core->prev_eip)
#define DRC_CYCLES   uml::mem(&m_core->cycles)

#define PAGE_MODE_ENABLED (m_core->cr[0] & CR0_PG)

#define DRC_REG32(reg_idx) uml::mem(&m_core->reg.d[reg_idx])
#define DRC_REG16(reg_idx) uml::mem(&m_core->reg.w[reg_idx])
#define DRC_REG8(reg_idx)  uml::mem(&m_core->reg.b[reg_idx])

#define OP_GET_SUBOP(modrm) (((modrm) >> 3) & 7)

#define MRM_MOD(modrm)    (((modrm) >> 6) & 3)
#define MRM_REG(modrm)    (((modrm) >> 3) & 7)
#define MRM_OPCODE(modrm) (((modrm) >> 3) & 7)
#define MRM_RM(modrm)     ((modrm) & 7)

#define SIB_SCALE(sib) (((sib) >> 6) & 3)
#define SIB_INDEX(sib) (((sib) >> 3) & 7)
#define SIB_BASE(sib)  ((sib) & 7)

#define MRM_HAS_SIB(modrm) (MRM_RM(modrm) == 4 && MRM_MOD(modrm) != 3)

#define MRM_REG8(modrm)  i386_MODRM_table[modrm].reg.b
#define MRM_REG16(modrm) i386_MODRM_table[modrm].reg.w
#define MRM_REG32(modrm) i386_MODRM_table[modrm].reg.d
#define MRM_RM8(modrm)   i386_MODRM_table[modrm].rm.b
#define MRM_RM16(modrm)  i386_MODRM_table[modrm].rm.w
#define MRM_RM32(modrm)  i386_MODRM_table[modrm].rm.d

#define DRC_GET_MRM_REG8(modrm)  DRC_REG8(MRM_REG8(modrm))
#define DRC_GET_MRM_REG16(modrm) DRC_REG16(MRM_REG16(modrm))
#define DRC_GET_MRM_REG32(modrm) DRC_REG32(MRM_REG32(modrm))
#define DRC_GET_MRM_RM8(modrm)   DRC_REG8(MRM_RM8(modrm))
#define DRC_GET_MRM_RM16(modrm)  DRC_REG16(MRM_RM16(modrm))
#define DRC_GET_MRM_RM32(modrm)  DRC_REG32(MRM_RM32(modrm))

// Using the i386_MODRM_table table for these since it lines up even though we're not doing a ModR/M lookup.

#define OP_REG8(opcode)  MRM_REG8(opcode)
#define OP_REG16(opcode) MRM_REG16(opcode)
#define OP_REG32(opcode) MRM_REG32(opcode)
#define OP_RM8(opcode)   MRM_RM8(opcode)
#define OP_RM16(opcode)  MRM_RM16(opcode)
#define OP_RM32(opcode)  MRM_RM32(opcode)

#define DRC_GET_OP_REG8(opcode)  DRC_REG8(OP_REG8(opcode))
#define DRC_GET_OP_REG16(opcode) DRC_REG16(OP_REG16(opcode))
#define DRC_GET_OP_REG32(opcode) DRC_REG32(OP_REG32(opcode))
#define DRC_GET_OP_RM8(opcode)   DRC_REG8(OP_RM8(opcode))
#define DRC_GET_OP_RM16(opcode)  DRC_REG16(OP_RM16(opcode))
#define DRC_GET_OP_RM32(opcode)  DRC_REG32(OP_RM32(opcode))

// Same deal with SIB (32-bit only), it will line up in i386_MODRM_table

#define SIB_REG32(sib)         MRM_RM32(sib)
#define DRC_GET_SIB_REG32(sib) DRC_REG32(SIB_REG32(sib))

#define DRC_SCR8             uml::mem(&m_core->data8)
#define DRC_SCR16            uml::mem(&m_core->data16)
#define DRC_SCR32            uml::mem(&m_core->data32)
#define DRC_SCR64            uml::mem(&m_core->data64)
#define DRC_SCR128(data_idx) uml::mem(&m_core->data128[data_idx])

#define DRC_GDT(field)          uml::mem(&m_core->gdtr.field)
#define DRC_IDT(field)          uml::mem(&m_core->idtr.field)
#define DRC_LDT(field)          uml::mem(&m_core->ldtr.field)
#define DRC_TSK(field)          uml::mem(&m_core->task.field)
#define DRC_SEG(reg_idx, field) uml::mem(&m_core->sreg[reg_idx].field)
#define DRC_CR(reg_idx)         uml::mem(&m_core->cr[reg_idx])

#define DRC_X87SCR(reg_key, field) uml::mem(&m_core->x87_data_##reg_key.field)
#define X87SCR_PTR(reg_key, field) &m_core->x87_data_##reg_key.field
#define DRC_X87REG(reg_idx, field) uml::mem(&m_core->x87_reg[reg_idx].field)
#define X87REG_PTR(reg_idx, field) &m_core->x87_reg[reg_idx].field

#define I386_INTERP(fn) (&i386_device::cfunc_run_interp<&i386_device::fn>)
#define I386_CB(cb)     (&i386_device::cfunc_callback<&i386_device::cb>)

static inline void alloc_handle(drcuml_state &drc, uml::code_handle *&h, const char *name)
{
	if (!h)
		h = drc.handle_alloc(name);
}

#include <chrono>

#define DEBUG_MEMORY_ACCESS 0   // 1=enable, 0=disabled
#define DEBUG_DIAGRAM_TSORT 1   // 1=sorts by access duration/time, 0=sorts by access count/hits
#define DEBUG_DIAGRAM_FREQ  5.0 // How often to print memory access diagnostics in seconds. This is rough since it's dependant on mem access rate.
#define DEBUG_DIAGRAM_ACNT  10  // How many slow addresses to print

// Flag on the address to differentiate between writes and reads on an address.
// Using bit 32 to clear the entire 32-bit address space.
static constexpr uint64_t MEM_DIAG_WRITE_FLAG = 0x100000000ULL;

#define DEBUG_INTERP_FALLBACK 0   // 1=enable, 0=disabled
#define DEBUG_DIAGIFALLB_FREQ 5.0 // How often to print interpreter fallback diagnostics in seconds. This is rough since it's dependant on the fallback rate.
#define DEBUG_DIAGIFALLB_ACNT 20  // How many fallback opcodes to print

#endif // MAME_CPU_I386_DRC_I386_H
