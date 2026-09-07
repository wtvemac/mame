// license:BSD-3-Clause
// copyright-holders:wtvemac

// Description here

const i386_device::DRC_OPCODE i386_device::s_drc_opcode_table[] = {
	//  Opcode  Feature Flags      16-bit handler                         32-bit handler                         DRC flags

	//
	// Primary instructions listed using its first byte
	//

	{ 0x00, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADD
	{ 0x01, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADD
	{ 0x02, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADD
	{ 0x03, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADD
	{ 0x04, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // ADD
	{ 0x05, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // ADD
	{ 0x06, OP_I386,               &i386_device::drc_pri_push_es16,       &i386_device::drc_pri_push_es32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH ES
	{ 0x07, OP_I386,               &i386_device::drc_pri_pop_es,          &i386_device::drc_pri_pop_es,         DRC_NONE                                                                                     }, // POP ES
	{ 0x08, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // OR
	{ 0x09, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // OR
	{ 0x0a, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // OR
	{ 0x0b, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // OR
	{ 0x0c, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // OR
	{ 0x0d, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // OR
	{ 0x0e, OP_I386,               &i386_device::drc_pri_push_cs16,       &i386_device::drc_pri_push_cs32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH CS
	// 0x0f two-byte instruction prefix. Dispatching on the second byte value (table below)
	{ 0x10, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADC
	{ 0x11, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADC
	{ 0x12, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADC
	{ 0x13, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // ADC
	{ 0x14, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // ADC
	{ 0x15, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // ADC
	{ 0x16, OP_I386,               &i386_device::drc_pri_push_ss16,       &i386_device::drc_pri_push_ss32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH SS
	{ 0x17, OP_I386,               &i386_device::drc_pri_pop_ss,          &i386_device::drc_pri_pop_ss,         DRC_NONE                                                                                     }, // POP SS
	{ 0x18, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SBB
	{ 0x19, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SBB
	{ 0x1a, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SBB
	{ 0x1b, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SBB
	{ 0x1c, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // SBB
	{ 0x1d, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // SBB
	{ 0x1e, OP_I386,               &i386_device::drc_pri_push_ds16,       &i386_device::drc_pri_push_ds32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH DS
	{ 0x1f, OP_I386,               &i386_device::drc_pri_pop_ds,          &i386_device::drc_pri_pop_ds,         DRC_NONE                                                                                     }, // POP DS
	{ 0x20, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // AND
	{ 0x21, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // AND
	{ 0x22, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // AND
	{ 0x23, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // AND
	{ 0x24, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // AND
	{ 0x25, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // AND
	// 0x26 Segment override to ES prefix, not dispatched
	// 0x27 falls back to the interpreter (DAA)
	{ 0x28, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SUB
	{ 0x29, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SUB
	{ 0x2a, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SUB
	{ 0x2b, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // SUB
	{ 0x2c, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // SUB
	{ 0x2d, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // SUB
	// 0x2e Segment override to CS prefix, not dispatched
	// 0x2f falls back to the interpreter (DAS)
	{ 0x30, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // XOR
	{ 0x31, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // XOR
	{ 0x32, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // XOR
	{ 0x33, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // XOR
	{ 0x34, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // XOR
	{ 0x35, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // XOR
	// 0x36 Segment override to SS prefix, not dispatched
	// 0x37 falls back to the interpreter (AAA)
	{ 0x38, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // CMP
	{ 0x39, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // CMP
	{ 0x3a, OP_I386,               &i386_device::drc_pri_alu8,            &i386_device::drc_pri_alu8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // CMP
	{ 0x3b, OP_I386,               &i386_device::drc_pri_alu16,           &i386_device::drc_pri_alu32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // CMP
	{ 0x3c, OP_I386,               &i386_device::drc_pri_alu_acc_imm,     &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_IMM_B                                     }, // CMP
	{ 0x3d, OP_I386,               &i386_device::drc_pri_alu16_acc_imm,   &i386_device::drc_pri_alu_acc_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_IMM_Z                                     }, // CMP
	// 0x3e Segment override to DS prefix, not dispatched
	// 0x3f falls back to the interpreter (AAS)
	{ 0x40, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x41, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x42, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x43, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x44, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x45, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x46, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x47, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // INC
	{ 0x48, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x49, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4a, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4b, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4c, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4d, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4e, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x4f, OP_I386,               &i386_device::drc_pri_incdec_r16,      &i386_device::drc_pri_incdec_r32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS                                               }, // DEC
	{ 0x50, OP_I386,               &i386_device::drc_pri_push_eax16,      &i386_device::drc_pri_push_eax32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x51, OP_I386,               &i386_device::drc_pri_push_ecx16,      &i386_device::drc_pri_push_ecx32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x52, OP_I386,               &i386_device::drc_pri_push_edx16,      &i386_device::drc_pri_push_edx32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x53, OP_I386,               &i386_device::drc_pri_push_ebx16,      &i386_device::drc_pri_push_ebx32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x54, OP_I386,               &i386_device::drc_pri_push_esp16,      &i386_device::drc_pri_push_esp32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x55, OP_I386,               &i386_device::drc_pri_push_ebp16,      &i386_device::drc_pri_push_ebp32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x56, OP_I386,               &i386_device::drc_pri_push_esi16,      &i386_device::drc_pri_push_esi32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x57, OP_I386,               &i386_device::drc_pri_push_edi16,      &i386_device::drc_pri_push_edi32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH
	{ 0x58, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x59, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5a, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5b, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5c, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5d, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5e, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x5f, OP_I386,               &i386_device::drc_pri_pop_r16,         &i386_device::drc_pri_pop_r32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // POP
	{ 0x60, OP_I386,               &i386_device::drc_pri_pusha,           &i386_device::drc_pri_pusha,          DRC_NONE                                                                                     }, // PUSHA/PUSHAD
	{ 0x61, OP_I386,               &i386_device::drc_pri_popa,            &i386_device::drc_pri_popa,           DRC_NONE                                                                                     }, // POPA/POPAD
	// 0x62 falls back to the interpreter (BOUND)
	// 0x63 falls back to the interpreter (ARPL)
	// 0x64 Segment override to FS prefix, not dispatched
	// 0x65 Segment override to GS prefix, not dispatched
	// 0x66 Operand-size override prefix, not dispatched
	// 0x67 Address-size override prefix, not dispatched
	{ 0x68, OP_I386,               &i386_device::drc_pri_push_i16,        &i386_device::drc_pri_push_i32,       DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // PUSH
	{ 0x69, OP_I386,               &i386_device::drc_pri_imul_imm,        &i386_device::drc_pri_imul_imm,       DRC_HAS_MODRM | DRC_IMM_Z                                                                    }, // IMUL
	{ 0x6a, OP_I386,               &i386_device::drc_pri_push_i8_16,      &i386_device::drc_pri_push_i8_32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_BS                                                  }, // PUSH
	{ 0x6b, OP_I386,               &i386_device::drc_pri_imul_imm,        &i386_device::drc_pri_imul_imm,       DRC_HAS_MODRM | DRC_IMM_BS                                                                   }, // IMUL
	// 0x6c falls back to the interpreter (INSB)
	// 0x6d falls back to the interpreter (INSW/INSD)
	// 0x6e falls back to the interpreter (OUTSB)
	// 0x6f falls back to the interpreter (OUTSW/OUTSD)
	{ 0x70, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JO
	{ 0x71, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JNO
	{ 0x72, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JB/JNAE
	{ 0x73, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JAE/JNB
	{ 0x74, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JE/JZ
	{ 0x75, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JNE/JNZ
	{ 0x76, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JBE/JNA
	{ 0x77, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JA/JNBE
	{ 0x78, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JS
	{ 0x79, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JNS
	{ 0x7a, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JP/JPE
	{ 0x7b, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JNP/JPO
	{ 0x7c, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JL/JNGE
	{ 0x7d, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JGE/JNL
	{ 0x7e, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JLE/JNG
	{ 0x7f, OP_I386,               &i386_device::drc_pri_jcc_rel8_16,     &i386_device::drc_pri_jcc_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B         }, // JG/JNLE
	{ 0x80, OP_I386,               &i386_device::drc_pri_group80_8,       &i386_device::drc_pri_group80_8,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_B | DRC_GROUP    }, // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
	{ 0x81, OP_I386,               &i386_device::drc_pri_group81_16,      &i386_device::drc_pri_group81_32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_Z | DRC_GROUP    }, // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
	{ 0x82, OP_I386,               &i386_device::drc_pri_group80_8,       &i386_device::drc_pri_group80_8,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_B | DRC_GROUP    }, // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
	{ 0x83, OP_I386,               &i386_device::drc_pri_group83_16,      &i386_device::drc_pri_group83_32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_BS | DRC_GROUP   }, // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
	{ 0x84, OP_I386,               &i386_device::drc_pri_test8,           &i386_device::drc_pri_test8,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // TEST
	{ 0x85, OP_I386,               &i386_device::drc_pri_test16,          &i386_device::drc_pri_test32,         DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_HAS_MODRM                                 }, // TEST
	{ 0x86, OP_I386,               &i386_device::drc_pri_xchg8,           &i386_device::drc_pri_xchg8,          DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // XCHG
	{ 0x87, OP_I386,               interpreter_fallback,                  &i386_device::drc_pri_xchg32_modrm,   DRC_HAS_MODRM                                                                                }, // XCHG
	{ 0x88, OP_I386,               &i386_device::drc_pri_mov8,            &i386_device::drc_pri_mov8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	{ 0x89, OP_I386,               &i386_device::drc_pri_mov16,           &i386_device::drc_pri_mov32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	{ 0x8a, OP_I386,               &i386_device::drc_pri_mov8,            &i386_device::drc_pri_mov8,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	{ 0x8b, OP_I386,               &i386_device::drc_pri_mov16,           &i386_device::drc_pri_mov32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	{ 0x8c, OP_I386,               &i386_device::drc_pri_mov_from_sreg,   &i386_device::drc_pri_mov_from_sreg,  DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	{ 0x8d, OP_I386,               &i386_device::drc_pri_lea,             &i386_device::drc_pri_lea,            DRC_HAS_MODRM                                                                                }, // LEA
	{ 0x8e, OP_I386,               &i386_device::drc_pri_mov_to_sreg,     &i386_device::drc_pri_mov_to_sreg,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOV
	// 0x8f falls back to the interpreter (POP)
	{ 0x90, OP_I386,               &i386_device::drc_pri_nop,             &i386_device::drc_pri_nop,            DRC_MODE0_RDY | DRC_NO_RSCR                                                                  }, // NOP
	{ 0x91, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x92, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x93, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x94, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x95, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x96, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x97, OP_I386,               &i386_device::drc_pri_xchg16,          &i386_device::drc_pri_xchg32_reg,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // XCHG
	{ 0x98, OP_I386,               &i386_device::drc_pri_cbw_cwde,        &i386_device::drc_pri_cbw_cwde,       DRC_NONE                                                                                     }, // CBW/CWDE
	{ 0x99, OP_I386,               &i386_device::drc_pri_cwd_cdq,         &i386_device::drc_pri_cwd_cdq,        DRC_NONE                                                                                     }, // CWD/CDQ
	{ 0x9a, OP_I386,               interpreter_fallback,                  interpreter_fallback,                 DRC_BR_END | DRC_IMM_FARPTR                                                                  }, // CALL - interpreter fallback, entry is just to pass flags to the frontend
	{ 0x9b, OP_I486,               &i386_device::drc_pri_wait,            &i386_device::drc_pri_wait,           DRC_NONE                                                                                     }, // WAIT/FWAIT
	{ 0x9c, OP_I386,               &i386_device::drc_pri_pushf16,         &i386_device::drc_pri_pushfd,         DRC_MODE0_RDY                                                                                }, // PUSHF/PUSHFD
	{ 0x9d, OP_I386,               &i386_device::drc_pri_popf16,          &i386_device::drc_pri_popfd,          DRC_MODE0_RDY                                                                                }, // POPF/POPFD
	{ 0x9e, OP_I386,               &i386_device::drc_pri_sahf,            &i386_device::drc_pri_sahf,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED                                            }, // SAHF
	{ 0x9f, OP_I386,               &i386_device::drc_pri_lahf,            &i386_device::drc_pri_lahf,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // LAHF
	{ 0xa0, OP_I386,               &i386_device::drc_pri_mov_al_m8,       &i386_device::drc_pri_mov_al_m8,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_MOFFS                                               }, // MOV
	{ 0xa1, OP_I386,               &i386_device::drc_pri_mov_acc_moffs,   &i386_device::drc_pri_mov_acc_moffs,  DRC_IMM_MOFFS                                                                                }, // MOV
	{ 0xa2, OP_I386,               &i386_device::drc_pri_mov_m8_al,       &i386_device::drc_pri_mov_m8_al,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_MOFFS                                               }, // MOV
	{ 0xa3, OP_I386,               &i386_device::drc_pri_mov_acc_moffs16, &i386_device::drc_pri_mov_acc_moffs,  DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_MOFFS                                               }, // MOV
	{ 0xa4, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // MOVSB
	{ 0xa5, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // MOVSW/MOVSD
	{ 0xa6, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CMPSB
	{ 0xa7, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CMPSW/CMPSD
	{ 0xa8, OP_I386,               &i386_device::drc_pri_test8,           &i386_device::drc_pri_test8,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_IMM_B                                     }, // TEST
	{ 0xa9, OP_I386,               &i386_device::drc_pri_test_acc_imm16,  &i386_device::drc_pri_test_acc_imm32, DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_IMM_Z                                     }, // TEST
	{ 0xaa, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // STOSB
	{ 0xab, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // STOSW/STOSD
	{ 0xac, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // LODSB
	{ 0xad, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // LODSW/LODSD
	{ 0xae, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // SCASB
	{ 0xaf, OP_I386,               &i386_device::drc_pri_mem16,           &i386_device::drc_pri_mem32,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // SCASW/SCASD
	{ 0xb0, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb1, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb2, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb3, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb4, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb5, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb6, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb7, OP_I386,               &i386_device::drc_pri_mov_r8_imm,      &i386_device::drc_pri_mov_r8_imm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // MOV
	{ 0xb8, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xb9, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xba, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xbb, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xbc, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xbd, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xbe, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xbf, OP_I386,               &i386_device::drc_pri_mov_r16_imm,     &i386_device::drc_pri_mov_r32_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_Z                                                   }, // MOV
	{ 0xc0, OP_I386,               &i386_device::drc_pri_shift8,          &i386_device::drc_pri_shift8,         DRC_MODE0_RDY | DRC_HAS_MODRM | DRC_IMM_B | DRC_GROUP                                        }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	{ 0xc1, OP_I386,               &i386_device::drc_pri_shift16,         &i386_device::drc_pri_shift32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM | DRC_IMM_B | DRC_GROUP                       }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	{ 0xc2, OP_I386,               &i386_device::drc_pri_ret32,           &i386_device::drc_pri_ret32,          DRC_BR_END | DRC_IMM_W                                                                       }, // RET
	{ 0xc3, OP_I386,               &i386_device::drc_pri_ret32,           &i386_device::drc_pri_ret32,          DRC_BR_END                                                                                   }, // RET
	// 0xc4 falls back to the interpreter (LES)
	// 0xc5 falls back to the interpreter (LDS)
	{ 0xc6, OP_I386,               &i386_device::drc_pri_mov_rm8_imm,     &i386_device::drc_pri_mov_rm8_imm,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM | DRC_IMM_B                                   }, // MOV
	{ 0xc7, OP_I386,               &i386_device::drc_pri_mov_rm16_imm,    &i386_device::drc_pri_mov_rm32_imm,   DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM | DRC_IMM_Z                                   }, // MOV
	// 0xc8 falls back to the interpreter (ENTER)
	{ 0xc9, OP_I386,               &i386_device::drc_pri_leave16,         &i386_device::drc_pri_leave32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // LEAVE
	{ 0xca, OP_I386,               &i386_device::drc_pri_retf_i16,        &i386_device::drc_pri_retf32,         DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END | DRC_IMM_W                                      }, // RETF
	{ 0xcb, OP_I386,               &i386_device::drc_pri_retf16,          &i386_device::drc_pri_retf32,         DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END                                                  }, // RETF
	{ 0xcc, OP_I386,               &i386_device::drc_pri_int3,            &i386_device::drc_pri_int3,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END                                                  }, // INT3
	{ 0xcd, OP_I386,               &i386_device::drc_pri_int,             &i386_device::drc_pri_int,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END | DRC_IMM_B                                      }, // INT
	{ 0xce, OP_I386,               &i386_device::drc_pri_into,            &i386_device::drc_pri_into,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END                                                  }, // INTO
	{ 0xcf, OP_I386,               &i386_device::drc_pri_iret16,          &i386_device::drc_pri_iret32,         DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_END                                                  }, // IRET/IRETD
	{ 0xd0, OP_I386,               &i386_device::drc_pri_shift8,          &i386_device::drc_pri_shift8,         DRC_MODE0_RDY | DRC_HAS_MODRM | DRC_GROUP                                                    }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	{ 0xd1, OP_I386,               &i386_device::drc_pri_shift16,         &i386_device::drc_pri_shift32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM | DRC_GROUP                                   }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	{ 0xd2, OP_I386,               &i386_device::drc_pri_shift8,          &i386_device::drc_pri_shift8,         DRC_MODE0_RDY | DRC_HAS_MODRM | DRC_GROUP                                                    }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	{ 0xd3, OP_I386,               &i386_device::drc_pri_shift16,         &i386_device::drc_pri_shift32,        DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM | DRC_GROUP                                   }, // ROL/ROR/RCL/RCR/SHL/SHR/SAL/SAR
	// 0xd4 falls back to the interpreter (AAM)
	// 0xd5 falls back to the interpreter (AAD)
	// 0xd6 falls back to the interpreter (reserved/SALC)
	{ 0xd7, OP_I386,               &i386_device::drc_pri_xlat,            &i386_device::drc_pri_xlat,           DRC_MODE0_RDY                                                                                }, // XLAT
	{ 0xd8, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xd9, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xda, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xdb, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xdc, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xdd, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xde, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xdf, OP_FPU,                &i386_device::drc_pri_x87,             &i386_device::drc_pri_x87,            DRC_HAS_MODRM                                                                                }, // x87 (ESC)
	{ 0xe0, OP_I386,               &i386_device::drc_pri_loop,            &i386_device::drc_pri_loop,           DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B                                                        }, // LOOPNE/LOOPNZ
	{ 0xe1, OP_I386,               &i386_device::drc_pri_loop,            &i386_device::drc_pri_loop,           DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B                                                        }, // LOOPE/LOOPZ
	{ 0xe2, OP_I386,               &i386_device::drc_pri_loop,            &i386_device::drc_pri_loop,           DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B                                                        }, // LOOP
	{ 0xe3, OP_I386,               &i386_device::drc_pri_jcxz,            &i386_device::drc_pri_jcxz,           DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_REL8 | DRC_BR_COND | DRC_IMM_B                       }, // JCXZ/JECXZ
	{ 0xe4, OP_I386,               &i386_device::drc_pri_io_read8,        &i386_device::drc_pri_io_read8,       DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // IN
	{ 0xe5, OP_I386,               &i386_device::drc_pri_io_read32,       &i386_device::drc_pri_io_read32,      DRC_IMM_B                                                                                    }, // IN
	{ 0xe6, OP_I386,               &i386_device::drc_pri_io_write8,       &i386_device::drc_pri_io_write8,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_IMM_B                                                   }, // OUT
	{ 0xe7, OP_I386,               &i386_device::drc_pri_io_write32,      &i386_device::drc_pri_io_write32,     DRC_IMM_B                                                                                    }, // OUT
	{ 0xe8, OP_I386,               &i386_device::drc_pri_call16,          &i386_device::drc_pri_call32,         DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_BR_RELW | DRC_IMM_Z                                     }, // CALL
	{ 0xe9, OP_I386,               &i386_device::drc_pri_jmp_rel16,       &i386_device::drc_pri_jmp_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_IMM_Z                       }, // JMP
	{ 0xea, OP_I386,               &i386_device::drc_pri_jmp_abs,         &i386_device::drc_pri_jmp_abs,        DRC_BR_END | DRC_IMM_FARPTR                                                                  }, // JMP
	{ 0xeb, OP_I386,               &i386_device::drc_pri_jmp_rel8_16,     &i386_device::drc_pri_jmp_rel8_32,    DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_REL8 | DRC_IMM_B                       }, // JMP
	{ 0xec, OP_I386,               &i386_device::drc_pri_io_read8,        &i386_device::drc_pri_io_read8,       DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // IN
	{ 0xed, OP_I386,               &i386_device::drc_pri_io_read16,       &i386_device::drc_pri_io_read32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // IN
	{ 0xee, OP_I386,               &i386_device::drc_pri_io_write8,       &i386_device::drc_pri_io_write8,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // OUT
	{ 0xef, OP_I386,               &i386_device::drc_pri_io_write16,      &i386_device::drc_pri_io_write32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // OUT
	// 0xf0 LOCK prefix, not dispatched. Currently ignored with DRC-backed instructions. Will be seen during a fallback to the interpreter.
	// 0xf1 falls back to the interpreter (reserved/ICEBP)
	{ 0xf2, OP_I386,               &i386_device::drc_pri_repne16,         &i386_device::drc_pri_repne32,        DRC_MODE0_RDY                                                                                }, // REPNE/REPNZ
	{ 0xf3, OP_I386,               &i386_device::drc_pri_rep16,           &i386_device::drc_pri_rep32,          DRC_MODE0_RDY                                                                                }, // REP/REPE
	{ 0xf4, OP_I386,               &i386_device::drc_pri_hlt,             &i386_device::drc_pri_hlt,            DRC_MODE0_RDY | DRC_BR_END                                                                   }, // HLT
	{ 0xf5, OP_I386,               &i386_device::drc_pri_cmc,             &i386_device::drc_pri_cmc,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CMC
	{ 0xf6, OP_I386,               &i386_device::drc_pri_groupF6_8,       &i386_device::drc_pri_groupF6_8,      DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_GRP3 | DRC_GROUP                                  }, // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV
	{ 0xf7, OP_I386,               &i386_device::drc_pri_groupF7_16,      &i386_device::drc_pri_groupF7_32,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_IMM_GRP3 | DRC_GROUP }, // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV
	{ 0xf8, OP_I386,               &i386_device::drc_pri_clc,             &i386_device::drc_pri_clc,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CLC
	{ 0xf9, OP_I386,               &i386_device::drc_pri_stc,             &i386_device::drc_pri_stc,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // STC
	{ 0xfa, OP_I386,               &i386_device::drc_pri_cli,             &i386_device::drc_pri_cli,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CLI
	{ 0xfb, OP_I386,               &i386_device::drc_pri_sti,             &i386_device::drc_pri_sti,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // STI
	{ 0xfc, OP_I386,               &i386_device::drc_pri_cld,             &i386_device::drc_pri_cld,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // CLD
	{ 0xfd, OP_I386,               &i386_device::drc_pri_std,             &i386_device::drc_pri_std,            DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // STD
	{ 0xfe, OP_I386,               &i386_device::drc_pri_incdec8_rm,      &i386_device::drc_pri_incdec8_rm,     DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_LFLAGS | DRC_HAS_MODRM                               }, // INC/DEC
	{ 0xff, OP_I386,               &i386_device::drc_pri_groupFF_16,      &i386_device::drc_pri_groupFF_32,     DRC_CAN_OSZ_OV | DRC_SELF_MANAGED | DRC_HAS_MODRM | DRC_GROUP                                }, // INC/DEC/CALL/JMP/PUSH

	//
	// Instructions with the '0x0f' prefix, second byte listed below
	//

	{ 0x00, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_group0f00,       &i386_device::drc_x0f_group0f00,      DRC_SELF_MANAGED | DRC_HAS_MODRM                                                             }, // SLDT/STR/LLDT/LTR/VERR/VERW
	{ 0x01, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_group0f01,       &i386_device::drc_x0f_group0f01,      DRC_HAS_MODRM                                                                                }, // SGDT/SIDT/LGDT/LIDT/SMSW/LMSW/INVLPG
	// 0x02 falls back to the interpreter (LAR)
	// 0x03 falls back to the interpreter (LSL)
	{ 0x06, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_clts,            &i386_device::drc_x0f_clts,           DRC_MODE0_RDY                                                                                }, // CLTS
	// 0x07 falls back to the interpreter (LOADALL)
	// 0x08 falls back to the interpreter (INVD)
	// 0x09 falls back to the interpreter (WBINVD)
	// 0x0b falls back to the interpreter (UD2)
	// 0x10 falls back to the interpreter (MOVUPS)
	// 0x11 falls back to the interpreter (MOVUPS)
	// 0x12 falls back to the interpreter (MOVLPS/MOVHLPS)
	// 0x13 falls back to the interpreter (MOVLPS)
	// 0x14 falls back to the interpreter (UNPCKLPS)
	// 0x15 falls back to the interpreter (UNPCKHPS)
	// 0x16 falls back to the interpreter (MOVHPS/MOVLHPS)
	// 0x17 falls back to the interpreter (MOVHPS)
	// 0x18 falls back to the interpreter (PREFETCHNTA/PREFETCHT0/PREFETCHT1/PREFETCHT2)
	{ 0x20, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_mov_r32_cr,      &i386_device::drc_x0f_mov_r32_cr,     DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // MOV
	// 0x21 falls back to the interpreter (MOV, from DR)
	{ 0x22, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_mov_cr_r32,      &i386_device::drc_x0f_mov_cr_r32,     DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // MOV
	// 0x23 falls back to the interpreter (MOV, to DR)
	// 0x24 falls back to the interpreter (MOV, from TR)
	// 0x26 falls back to the interpreter (MOV, to TR)
	// 0x28 falls back to the interpreter (MOVAPS)
	// 0x29 falls back to the interpreter (MOVAPS)
	// 0x2a falls back to the interpreter (CVTPI2PS)
	// 0x2b falls back to the interpreter (MOVNTPS)
	// 0x2c falls back to the interpreter (CVTTPS2PI)
	// 0x2d falls back to the interpreter (CVTPS2PI)
	// 0x2e falls back to the interpreter (UCOMISS)
	// 0x2f falls back to the interpreter (COMISS)
	{ 0x30, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_wrmsr,           &i386_device::drc_x0f_wrmsr,          DRC_MODE0_RDY                                                                                }, // WRMSR
	{ 0x31, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_rdtsc,           &i386_device::drc_x0f_rdtsc,          DRC_MODE0_RDY                                                                                }, // RDTSC
	{ 0x32, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_rdmsr,           &i386_device::drc_x0f_rdmsr,          DRC_MODE0_RDY                                                                                }, // RDMSR
	// 0x38 falls back to the interpreter (3-byte opcode escape)
	// 0x3a falls back to the interpreter (3-byte opcode escape)
	// 0x3b falls back to the interpreter (reserved)
	// 0x3c falls back to the interpreter (reserved)
	// 0x3d falls back to the interpreter (reserved)
	{ 0x40, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVO
	{ 0x41, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVNO
	{ 0x42, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVB/CMOVNAE
	{ 0x43, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVAE/CMOVNB
	{ 0x44, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVE/CMOVZ
	{ 0x45, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVNE/CMOVNZ
	{ 0x46, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVBE/CMOVNA
	{ 0x47, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVA/CMOVNBE
	{ 0x48, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVS
	{ 0x49, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVNS
	{ 0x4a, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVP/CMOVPE
	{ 0x4b, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVNP/CMOVPO
	{ 0x4c, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVL/CMOVNGE
	{ 0x4d, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVGE/CMOVNL
	{ 0x4e, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVLE/CMOVNG
	{ 0x4f, OP_2BYTE | OP_PENTIUM, &i386_device::drc_x0f_cmovcc_16,       &i386_device::drc_x0f_cmovcc_32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // CMOVG/CMOVNLE
	// 0x50 falls back to the interpreter (MOVMSKPS)
	// 0x51 falls back to the interpreter (SQRTPS)
	// 0x52 falls back to the interpreter (RSQRTPS)
	// 0x53 falls back to the interpreter (RCPPS)
	// 0x54 falls back to the interpreter (ANDPS)
	// 0x55 falls back to the interpreter (ANDNPS)
	// 0x56 falls back to the interpreter (ORPS)
	// 0x57 falls back to the interpreter (XORPS)
	// 0x58 falls back to the interpreter (ADDPS)
	// 0x59 falls back to the interpreter (MULPS)
	// 0x5a falls back to the interpreter (CVTPS2PD)
	// 0x5b falls back to the interpreter (CVTDQ2PS)
	// 0x5c falls back to the interpreter (SUBPS)
	// 0x5d falls back to the interpreter (MINPS)
	// 0x5e falls back to the interpreter (DIVPS)
	// 0x5f falls back to the interpreter (MAXPS)
	{ 0x60, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpcklbw,   &i386_device::drc_x0f_mmx_punpcklbw,  DRC_HAS_MODRM                                                                                }, // PUNPCKLBW
	{ 0x61, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpckl,     &i386_device::drc_x0f_mmx_punpckl,    DRC_HAS_MODRM                                                                                }, // PUNPCKLWD
	{ 0x62, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpckl,     &i386_device::drc_x0f_mmx_punpckl,    DRC_HAS_MODRM                                                                                }, // PUNPCKLDQ
	// 0x63 falls back to the interpreter (PACKSSWB)
	// 0x64 falls back to the interpreter (PCMPGTB)
	// 0x65 falls back to the interpreter (PCMPGTW)
	// 0x66 falls back to the interpreter (PCMPGTD)
	{ 0x67, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_packuswb,    &i386_device::drc_x0f_mmx_packuswb,   DRC_HAS_MODRM                                                                                }, // PACKUSWB
	{ 0x68, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpckhbw,   &i386_device::drc_x0f_mmx_punpckhbw,  DRC_HAS_MODRM                                                                                }, // PUNPCKHBW
	{ 0x69, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpckh,     &i386_device::drc_x0f_mmx_punpckh,    DRC_HAS_MODRM                                                                                }, // PUNPCKHWD
	{ 0x6a, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_punpckh,     &i386_device::drc_x0f_mmx_punpckh,    DRC_HAS_MODRM                                                                                }, // PUNPCKHDQ
	// 0x6b falls back to the interpreter (PACKSSDW)
	{ 0x6e, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_movd_load,   &i386_device::drc_x0f_mmx_movd_load,  DRC_HAS_MODRM                                                                                }, // MOVD
	{ 0x6f, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_movq_load,   &i386_device::drc_x0f_mmx_movq_load,  DRC_HAS_MODRM                                                                                }, // MOVQ
	// 0x70 falls back to the interpreter (PSHUFW)
	{ 0x71, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_group_0f71,  &i386_device::drc_x0f_mmx_group_0f71, DRC_HAS_MODRM | DRC_IMM_B                                                                    }, // PSRLW/PSRAW/PSLLW
	// 0x72 falls back to the interpreter (PSRLD/PSRAD/PSLLD)
	// 0x73 falls back to the interpreter (PSRLQ/PSLLQ)
	// 0x74 falls back to the interpreter (PCMPEQB)
	// 0x75 falls back to the interpreter (PCMPEQW)
	// 0x76 falls back to the interpreter (PCMPEQD)
	{ 0x77, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_emms,        &i386_device::drc_x0f_mmx_emms,       DRC_NONE                                                                                     }, // EMMS
	// 0x78 falls back to the interpreter (reserved)
	// 0x79 falls back to the interpreter (reserved)
	// 0x7a falls back to the interpreter (reserved)
	// 0x7b falls back to the interpreter (reserved)
	// 0x7c falls back to the interpreter (reserved)
	// 0x7d falls back to the interpreter (reserved)
	// 0x7e falls back to the interpreter (MOVD)
	{ 0x7f, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_movq_store,  &i386_device::drc_x0f_mmx_movq_store, DRC_HAS_MODRM                                                                                }, // MOVQ
	{ 0x80, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JO
	{ 0x81, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JNO
	{ 0x82, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JB/JNAE
	{ 0x83, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JAE/JNB
	{ 0x84, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JE/JZ
	{ 0x85, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JNE/JNZ
	{ 0x86, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JBE/JNA
	{ 0x87, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JA/JNBE
	{ 0x88, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JS
	{ 0x89, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JNS
	{ 0x8a, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JP/JPE
	{ 0x8b, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JNP/JPO
	{ 0x8c, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JL/JNGE
	{ 0x8d, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JGE/JNL
	{ 0x8e, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JLE/JNG
	{ 0x8f, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_jcc_rel16,       &i386_device::drc_x0f_jcc_rel32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_NO_RSCR | DRC_BR_RELW | DRC_BR_COND | DRC_IMM_Z         }, // JG/JNLE
	{ 0x90, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETO
	{ 0x91, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETNO
	{ 0x92, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETB/SETNAE
	{ 0x93, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETAE/SETNB
	{ 0x94, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETE/SETZ
	{ 0x95, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETNE/SETNZ
	{ 0x96, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETBE/SETNA
	{ 0x97, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETA/SETNBE
	{ 0x98, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETS
	{ 0x99, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETNS
	{ 0x9a, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETP/SETPE
	{ 0x9b, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETNP/SETPO
	{ 0x9c, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETL/SETNGE
	{ 0x9d, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETGE/SETNL
	{ 0x9e, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETLE/SETNG
	{ 0x9f, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_setcc_rm8,       &i386_device::drc_x0f_setcc_rm8,      DRC_MODE0_RDY | DRC_HAS_MODRM                                                                }, // SETG/SETNLE
	{ 0xa0, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_push_fs16,       &i386_device::drc_x0f_push_fs32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH FS
	{ 0xa1, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_pop_fs,          &i386_device::drc_x0f_pop_fs,         DRC_NONE                                                                                     }, // POP FS
	{ 0xa2, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_cpuid,           &i386_device::drc_x0f_cpuid,          DRC_MODE0_RDY                                                                                }, // CPUID
	{ 0xa3, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bt_group,        &i386_device::drc_x0f_bt_group,       DRC_HAS_MODRM                                                                                }, // BT
	{ 0xa4, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_shld,            &i386_device::drc_x0f_shld,           DRC_NO_LFLAGS | DRC_HAS_MODRM | DRC_IMM_B                                                    }, // SHLD
	{ 0xa5, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_shld,            &i386_device::drc_x0f_shld,           DRC_NO_LFLAGS | DRC_HAS_MODRM                                                                }, // SHLD
	{ 0xa8, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_push_gs16,       &i386_device::drc_x0f_push_gs32,      DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // PUSH GS
	{ 0xa9, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_pop_gs,          &i386_device::drc_x0f_pop_gs,         DRC_NONE                                                                                     }, // POP GS
	// 0xaa falls back to the interpreter (RSM)
	{ 0xab, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bt_group,        &i386_device::drc_x0f_bt_group,       DRC_HAS_MODRM                                                                                }, // BTS
	{ 0xac, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_shrd,            &i386_device::drc_x0f_shrd,           DRC_NO_LFLAGS | DRC_HAS_MODRM | DRC_IMM_B                                                    }, // SHRD
	{ 0xad, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_shrd,            &i386_device::drc_x0f_shrd,           DRC_NO_LFLAGS | DRC_HAS_MODRM                                                                }, // SHRD
	// 0xae falls back to the interpreter (FXSAVE/FXRSTOR/LDMXCSR/STMXCSR/SFENCE)
	{ 0xaf, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_imul,            &i386_device::drc_x0f_imul,           DRC_HAS_MODRM                                                                                }, // IMUL
	// 0xb0 falls back to the interpreter (CMPXCHG)
	{ 0xb1, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_cmpxchg,         &i386_device::drc_x0f_cmpxchg,        DRC_HAS_MODRM                                                                                }, // CMPXCHG
	// 0xb2 falls back to the interpreter (LSS)
	{ 0xb3, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bt_group,        &i386_device::drc_x0f_bt_group,       DRC_HAS_MODRM                                                                                }, // BTR
	// 0xb4 falls back to the interpreter (LFS)
	// 0xb5 falls back to the interpreter (LGS)
	{ 0xb6, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_movzx_sx16,      &i386_device::drc_x0f_movzx_sx,       DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOVZX
	{ 0xb7, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_movzx_sx,        &i386_device::drc_x0f_movzx_sx,       DRC_HAS_MODRM                                                                                }, // MOVZX
	{ 0xba, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bt_group,        &i386_device::drc_x0f_bt_group,       DRC_HAS_MODRM | DRC_IMM_B | DRC_GROUP                                                        }, // BT/BTS/BTR/BTC
	{ 0xbb, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bt_group,        &i386_device::drc_x0f_bt_group,       DRC_HAS_MODRM                                                                                }, // BTC
	{ 0xbc, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bsf_bsr,         &i386_device::drc_x0f_bsf_bsr,        DRC_NO_LFLAGS | DRC_HAS_MODRM                                                                }, // BSF
	{ 0xbd, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_bsf_bsr,         &i386_device::drc_x0f_bsf_bsr,        DRC_NO_LFLAGS | DRC_HAS_MODRM                                                                }, // BSR
	{ 0xbe, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_movzx_sx16,      &i386_device::drc_x0f_movzx_sx,       DRC_MODE0_RDY | DRC_CAN_OSZ_OV | DRC_HAS_MODRM                                               }, // MOVSX
	{ 0xbf, OP_2BYTE | OP_I386,    &i386_device::drc_x0f_movzx_sx,        &i386_device::drc_x0f_movzx_sx,       DRC_HAS_MODRM                                                                                }, // MOVSX
	// 0xc0 falls back to the interpreter (XADD)
	{ 0xc1, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_xadd,            &i386_device::drc_x0f_xadd,           DRC_HAS_MODRM                                                                                }, // XADD
	// 0xc2 falls back to the interpreter (CMPPS)
	// 0xc3 falls back to the interpreter (MOVNTI)
	// 0xc4 falls back to the interpreter (PINSRW)
	// 0xc5 falls back to the interpreter (PEXTRW)
	// 0xc6 falls back to the interpreter (SHUFPS)
	// 0xc7 falls back to the interpreter (CMPXCHG8B)
	{ 0xc8, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xc9, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xca, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xcb, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xcc, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xcd, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xce, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	{ 0xcf, OP_2BYTE | OP_I486,    &i386_device::drc_x0f_bswap,           &i386_device::drc_x0f_bswap,          DRC_MODE0_RDY | DRC_CAN_OSZ_OV                                                               }, // BSWAP
	// 0xd1 falls back to the interpreter (PSRLW)
	// 0xd2 falls back to the interpreter (PSRLD)
	// 0xd3 falls back to the interpreter (PSRLQ)
	{ 0xd4, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_paddq,       &i386_device::drc_x0f_mmx_paddq,      DRC_HAS_MODRM                                                                                }, // PADDQ
	{ 0xd5, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_pmullw,      &i386_device::drc_x0f_mmx_pmullw,     DRC_HAS_MODRM                                                                                }, // PMULLW
	// 0xd7 falls back to the interpreter (PMOVMSKB)
	{ 0xd8, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_usat,    &i386_device::drc_x0f_mmx_sub_usat,   DRC_HAS_MODRM                                                                                }, // PSUBUSB
	{ 0xd9, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_usat,    &i386_device::drc_x0f_mmx_sub_usat,   DRC_HAS_MODRM                                                                                }, // PSUBUSW
	// 0xda falls back to the interpreter (PMINUB)
	{ 0xdb, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_bitwise,     &i386_device::drc_x0f_mmx_bitwise,    DRC_HAS_MODRM                                                                                }, // PAND
	{ 0xdc, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_paddusb,     &i386_device::drc_x0f_mmx_paddusb,    DRC_HAS_MODRM                                                                                }, // PADDUSB
	{ 0xdd, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_add_usat,    &i386_device::drc_x0f_mmx_add_usat,   DRC_HAS_MODRM                                                                                }, // PADDUSW
	// 0xde falls back to the interpreter (PMAXUB)
	{ 0xdf, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_bitwise,     &i386_device::drc_x0f_mmx_bitwise,    DRC_HAS_MODRM                                                                                }, // PANDN
	// 0xe0 falls back to the interpreter (PAVGB)
	// 0xe1 falls back to the interpreter (PSRAW)
	// 0xe2 falls back to the interpreter (PSRAD)
	// 0xe3 falls back to the interpreter (PAVGW)
	// 0xe4 falls back to the interpreter (PMULHUW)
	// 0xe5 falls back to the interpreter (PMULHW)
	// 0xe6 falls back to the interpreter (reserved)
	// 0xe7 falls back to the interpreter (MOVNTQ)
	{ 0xe8, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_ssat,    &i386_device::drc_x0f_mmx_sub_ssat,   DRC_HAS_MODRM                                                                                }, // PSUBSB
	{ 0xe9, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_ssat,    &i386_device::drc_x0f_mmx_sub_ssat,   DRC_HAS_MODRM                                                                                }, // PSUBSW
	// 0xea falls back to the interpreter (PMINSW)
	{ 0xeb, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_bitwise,     &i386_device::drc_x0f_mmx_bitwise,    DRC_HAS_MODRM                                                                                }, // POR
	{ 0xec, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_add_ssat,    &i386_device::drc_x0f_mmx_add_ssat,   DRC_HAS_MODRM                                                                                }, // PADDSB
	{ 0xed, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_add_ssat,    &i386_device::drc_x0f_mmx_add_ssat,   DRC_HAS_MODRM                                                                                }, // PADDSW
	// 0xee falls back to the interpreter (PMAXSW)
	{ 0xef, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_bitwise,     &i386_device::drc_x0f_mmx_bitwise,    DRC_HAS_MODRM                                                                                }, // PXOR
	// 0xf1 falls back to the interpreter (PSLLW)
	// 0xf2 falls back to the interpreter (PSLLD)
	// 0xf3 falls back to the interpreter (PSLLQ)
	// 0xf4 falls back to the interpreter (PMULUDQ)
	// 0xf5 falls back to the interpreter (PMADDWD)
	// 0xf6 falls back to the interpreter (PSADBW)
	// 0xf7 falls back to the interpreter (MASKMOVQ)
	{ 0xf8, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_wrap,    &i386_device::drc_x0f_mmx_sub_wrap,   DRC_HAS_MODRM                                                                                }, // PSUBB
	{ 0xf9, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_wrap,    &i386_device::drc_x0f_mmx_sub_wrap,   DRC_HAS_MODRM                                                                                }, // PSUBW
	{ 0xfa, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_sub_wrap,    &i386_device::drc_x0f_mmx_sub_wrap,   DRC_HAS_MODRM                                                                                }, // PSUBD
	// 0xfb falls back to the interpreter (PSUBQ)
	{ 0xfc, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_add_wrap,    &i386_device::drc_x0f_mmx_add_wrap,   DRC_HAS_MODRM                                                                                }, // PADDB
	{ 0xfd, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_paddw,       &i386_device::drc_x0f_mmx_paddw,      DRC_HAS_MODRM                                                                                }, // PADDW
	{ 0xfe, OP_2BYTE | OP_MMX,     &i386_device::drc_x0f_mmx_add_wrap,    &i386_device::drc_x0f_mmx_add_wrap,   DRC_HAS_MODRM                                                                                }, // PADDD
};
