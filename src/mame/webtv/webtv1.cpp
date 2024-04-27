// license:BSD-3-Clause
// copyright-holders:FairPlay137

/***************************************************************************************
 *
 * WebTV FCS (1996)
 * 
 * The WebTV line of products was an early attempt to bring the Internet to the
 * television. Later on in its life, it was rebranded as MSN TV.
 * 
 * FCS, shorthand for First Customer Ship, was the first generation of WebTV hardware.
 * Its ASIC, known as SPOT or FIDO, is much simpler than SOLO.
 * 
 * A typical retail configuration uses a MIPS IDT R4640 clocked at 112MHz, with 2MB of
 * on-board RAM, 2MB of flash memory (using two 16-bit 1MB AMD 29F800B chips) for the
 * updatable software, and 2MB of mask ROM.
 * 
 * This driver would not have been possible without the efforts of the WebTV community
 * to preserve technical specifications, as well as the various reverse-engineering
 * efforts that were made.
 * 
 * Known issues:
 * - The CPU gets thrown into the exception handler loop at bfc0c030, a little before
 *   the memory check. This has to be manually bypassed to continue the boot process.
 *   (Worked around in mips3 and mips3drc)
 * - AppROMs appear to crash shortly after power on, with debug AppROMs throwing up
 *   "### CACHE ERROR" when serial communication is wired up to the SmartCard slot.
 * 
 ***************************************************************************************/

#include "emu.h"

#include "bus/pc_kbd/keyboards.h"
#include "cpu/mips/mips3.h"
#include "machine/ds2401.h"
#include "machine/intelfsh.h"
#include "spot_asic.h"

#include "webtv.lh"

#include "main.h"
#include "screen.h"

#define SYSCLOCK 56000000 // TODO: confirm this is correct

class webtv1_state : public driver_device
{
public:
	// constructor
	webtv1_state(const machine_config& mconfig, device_type type, const char* tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_spotasic(*this, "spot"),
		m_serial_id(*this, "serial_id"),
		m_nvram(*this, "nvram"),
		m_flash0(*this, "bank0_flash0"), // labeled U0501, contains upper bits
		m_flash1(*this, "bank0_flash1")  // labeled U0502, contains lower bits
	{ }
	
	DECLARE_INPUT_CHANGED_MEMBER(pbuff_index_changed);
	
	void webtv1_base(machine_config& config);
	void webtv1_sony(machine_config& config);
	void webtv1_philips(machine_config& config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;
    
private:
	required_device<mips3_device> m_maincpu;
	required_device<spot_asic_device> m_spotasic;
	required_device<ds2401_device> m_serial_id;
	required_device<i2cmem_device> m_nvram;

	//required_device<amd_29f800b_16bit_device> m_flash0;
	//required_device<amd_29f800b_16bit_device> m_flash1;
	required_device<macronix_16161616_device> m_flash0;
	required_device<macronix_16161616_device> m_flash1;

	void bank0_flash_w(offs_t offset, uint32_t data);
	uint32_t bank0_flash_r(offs_t offset);

	void webtv1_map(address_map& map);
};

void webtv1_state::bank0_flash_w(offs_t offset, uint32_t data)
{
	// WebTV FCS uses two AMD AM29F800BT chips on the board for storing its software.
	// One chip is for the lower 16 bits (labeled U0502), and the other is for the upper 16 bits (labeled U0501).

	uint16_t upper_value = (data >> 16) & 0xffff;
	m_flash0->write(offset, upper_value);
	
	uint16_t lower_value = data & 0xffff;
	m_flash1->write(offset, lower_value);
}

uint32_t webtv1_state::bank0_flash_r(offs_t offset)
{
	uint16_t upper_value = m_flash0->read(offset);
	uint16_t lower_value = m_flash1->read(offset);

    return (upper_value << 16) | (lower_value);
}

void webtv1_state::webtv1_map(address_map &map)
{
	map.global_mask(0x1fffffff);

	// RAM
	map(0x00000000, 0x007fffff).ram().share("ram"); // 8MB is not accurate to retail hardware! Ideally this would be 2MB or 4MB, mirrored across this memory space

	// SPOT
	map(0x04000000, 0x04000fff).m(m_spotasic, FUNC(spot_asic_device::bus_unit_map));
	map(0x04001000, 0x04001fff).m(m_spotasic, FUNC(spot_asic_device::rom_unit_map));
	map(0x04002000, 0x04002fff).m(m_spotasic, FUNC(spot_asic_device::aud_unit_map));
	map(0x04003000, 0x04003fff).m(m_spotasic, FUNC(spot_asic_device::vid_unit_map));
	map(0x04004000, 0x04004fff).m(m_spotasic, FUNC(spot_asic_device::dev_unit_map));
	map(0x04005000, 0x04005fff).m(m_spotasic, FUNC(spot_asic_device::mem_unit_map));

	// ROM
	map(0x1f000000, 0x1f3fffff).rw(FUNC(webtv1_state::bank0_flash_r), FUNC(webtv1_state::bank0_flash_w)).share("bank0"); // Flash ROM, 4MB (retail configuration 2MB)
	map(0x1f800000, 0x1fffffff).rom().region("bank1", 0); // Mask ROM
}

void webtv1_state::webtv1_base(machine_config &config)
{
	config.set_default_layout(layout_webtv);

	R4640BE(config, m_maincpu, SYSCLOCK*2);
	m_maincpu->set_icache_size(0x2000);
	m_maincpu->set_dcache_size(0x2000);
	m_maincpu->set_addrmap(AS_PROGRAM, &webtv1_state::webtv1_map);

	// From bf0.diag.627.cachefix.o:

	// 00000002  57696C64  22232223  00100000  0000000B  FFFFFFFF  *9F14A66C 32-bit AMD-Style 4Mbit Bottom
	// 00000002  57696C64  22AB22AB  00100000  0000000B  FFFFFFFF  *9F14A69C 32-bit AMD-Style 4Mbit Bottom
	// 00000002  57696C64  22D622D6  00200000  00000013  FFFFFFFF  *9F14A6CC 32-bit AMD-Style 8Mbit Top
	// 00000002  57696C64  22582258  00200000  00000013  FFFFFFFF  *9F14A718 32-bit AMD-Style 8Mbit Bottom
	// 00000002  57696C64  22DA22DA  00200000  00000013  FFFFFFFF  *9F14A6CC 32-bit AMD-Style 8Mbit Top, 3V
	// 00000002  57696C64  225B225B  00200000  00000013  FFFFFFFF  *9F14A718 32-bit AMD-Style 8Mbit Bottom, 3V
	// 00000003  57696C64  00F100F1  00400000  00000010  FFFFFFFF  *9F14A764 32-bit MX-Style 16Mbit
	//	*9F14A764 = 00040000000400000004000000040000000400000004000000040000000400000004000000040000000400000004000000040000000400000004000000040000

	// 00000002  57696C64  000022D6  00100000  00000013  0000FFFF  *9F14A7A4 16-bit (lower half) AMD-Style 8Mbit Top
	// 00000002  57696C64  00002258  00100000  00000013  0000FFFF  *9F14A7F0 16-bit (lower half) AMD-Style 8Mbit Bottom
	// 00000002  57696C64  000022DA  00100000  00000013  0000FFFF  *9F14A7A4 16-bit (lower half) AMD-Style 8Mbit Top, 3V
	// 00000002  57696C64  0000225B  00100000  00000013  0000FFFF  *9F14A7F0 16-bit (lower half) AMD-Style 8Mbit Bottom, 3V
	// 00000003  57696C64  000000F1  00200000  00000010  0000FFFF  *9F14A83C 16-bit (lower half) MX-Style 16Mbit

	// 00000002  57696C64  22D60000  00100000  00000013  FFFF0000  *9F14A7A4 16-bit (upper half) AMD-Style 8Mbit Top
	// 00000002  57696C64  22580000  00100000  00000013  FFFF0000  *9F14A7F0 16-bit (upper half) AMD-Style 8Mbit Bottom
	// 00000002  57696C64  22DA0000  00100000  00000013  FFFF0000  *9F14A7A4 16-bit (upper half) AMD-Style 8Mbit Top, 3V
	// 00000002  57696C64  225B0000  00100000  00000013  FFFF0000  *9F14A7F0 16-bit (upper half) AMD-Style 8Mbit Bottom, 3V
	// 00000003  57696C64  00F10000  00200000  00000010  FFFF0000  *9F14A83C 16-bit (upper half) MX-Style 16Mbit
	//	*9F14A83C = 00020000000200000002000000020000000200000002000000020000000200000002000000020000000200000002000000020000000200000002000000020000
	/*

	NOTE: IDFlashRAM is moved to 0x80000000M to run, called by CallRAMFlashFunction
	ROM:9F01CA24 b_IDFlashRAM_:
	ROM:9F01CA24                 li      $t0, 0xFFFFFFFE
	ROM:9F01CA28                 and     $t0, $t5, $t0
	ROM:9F01CA2C                 mtc0    $t0, SR          # Status register
	ROM:9F01CA30                 lw      $t0, 0($a1)
	ROM:9F01CA34                 lw      $t1, 4($a1)
	ROM:9F01CA38                 li      $t4, 0x90909090
	ROM:9F01CA40                 sw      $t4, 0($a1)
	ROM:9F01CA44                 lw      $t2, 0($a1)
	ROM:9F01CA48                 lw      $t3, 4($a1)
	ROM:9F01CA4C                 bne     $t0, $t2, loc_9F01CA5C
	ROM:9F01CA50                 nop
	ROM:9F01CA54                 beq     $t1, $t3, loc_9F01CA70
	ROM:9F01CA58                 nop
	ROM:9F01CA5C
	ROM:9F01CA5C loc_9F01CA5C:                            # CODE XREF: b_IDFlashRAM_+28↑j
	ROM:9F01CA5C                 li      $t4, 0xFFFFFFFF
	ROM:9F01CA60                 sw      $t4, 0($a1)
	ROM:9F01CA64                 lw      $t4, 0($a1)
	ROM:9F01CA68                 b       loc_9F01CB10
	ROM:9F01CA6C                 li      $v0, 1
	ROM:9F01CA70  # ---------------------------------------------------------------------------
	ROM:9F01CA70
	ROM:9F01CA70 loc_9F01CA70:                            # CODE XREF: b_IDFlashRAM_+30↑j
	ROM:9F01CA70                 li      $t4, 0x15554
	ROM:9F01CA78                 or      $a2, $a1, $t4
	ROM:9F01CA7C                 ori     $a3, $a1, 0xAAA8
	ROM:9F01CA80                 li      $t4, 0xAA00AA
	ROM:9F01CA88                 sw      $t4, 0($a2)
	ROM:9F01CA8C                 li      $t4, 0x550055
	ROM:9F01CA94                 sw      $t4, 0($a3)
	ROM:9F01CA98                 li      $t4, 0x900090
	ROM:9F01CAA0                 sw      $t4, 0($a2)
	ROM:9F01CAA4                 lw      $t4, 0($a2)
	ROM:9F01CAA8                 lw      $t2, 0($a1)
	ROM:9F01CAAC                 lw      $t3, 4($a1)
	ROM:9F01CAB0                 bne     $t0, $t2, loc_9F01CAC0
	ROM:9F01CAB4                 nop
	ROM:9F01CAB8                 beq     $t1, $t3, loc_9F01CB10
	ROM:9F01CABC                 li      $v0, 0
	ROM:9F01CAC0
	ROM:9F01CAC0 loc_9F01CAC0:                            # CODE XREF: b_IDFlashRAM_+8C↑j
	ROM:9F01CAC0                 li      $t4, 0xF000F0
	ROM:9F01CAC8                 sw      $t4, 0($a1)
	ROM:9F01CACC                 lw      $t0, 0($a1)
	ROM:9F01CAD0                 lw      $t1, 4($a1)
	ROM:9F01CAD4                 bne     $t0, $t2, loc_9F01CB10
	ROM:9F01CAD8                 li      $v0, 2
	ROM:9F01CADC                 bne     $t1, $t3, loc_9F01CB10
	ROM:9F01CAE0                 nop
	ROM:9F01CAE4                 li      $t4, 0xAA00AA
	ROM:9F01CAEC                 sw      $t4, 0($a2)
	ROM:9F01CAF0                 li      $t4, 0x550055
	ROM:9F01CAF8                 sw      $t4, 0($a3)
	ROM:9F01CAFC                 li      $t4, 0xF000F0
	ROM:9F01CB04                 sw      $t4, 0($a2)
	ROM:9F01CB08                 lw      $t4, 0($a2)
	ROM:9F01CB0C                 li      $v0, 3
	ROM:9F01CB10
	ROM:9F01CB10 loc_9F01CB10:                            # CODE XREF: b_IDFlashRAM_+44↑j
	ROM:9F01CB10                                          # b_IDFlashRAM_+94↑j ...
	ROM:9F01CB10                 beqz    $a0, loc_9F01CB20
	ROM:9F01CB14                 nop
	ROM:9F01CB18                 sw      $t2, 0($a0)
	ROM:9F01CB1C                 sw      $t3, 4($a0)
	ROM:9F01CB20
	ROM:9F01CB20 loc_9F01CB20:                            # CODE XREF: b_IDFlashRAM_:loc_9F01CB10↑j
	ROM:9F01CB20                 mtc0    $t5, SR          # Status register
	ROM:9F01CB24                 jr      $ra
	ROM:9F01CB28                 nop
	ROM:9F01CB28  # End of function b_IDFlashRAM_


	ROM:9F00CD08 b_idFlash_:                              # CODE XREF: b_IdentifyFlash_+34↑p
	ROM:9F00CD08
	ROM:9F00CD08 var_8           = -8
	ROM:9F00CD08 var_4           = -4
	ROM:9F00CD08 var_s0          =  0
	ROM:9F00CD08 var_s4          =  4
	ROM:9F00CD08 var_s8          =  8
	ROM:9F00CD08
	ROM:9F00CD08                 addiu   $sp, -0x28
	ROM:9F00CD0C                 sw      $s0, 0x18+var_s0($sp)
	ROM:9F00CD10                 move    $s0, $a0
	ROM:9F00CD14                 sw      $ra, 0x18+var_s8($sp)
	ROM:9F00CD18                 jal     b_DupeIdentifier_
	ROM:9F00CD1C                 sw      $s1, 0x18+var_s4($sp)
	ROM:9F00CD20                 addiu   $a0, $sp, 0x18+var_8
	ROM:9F00CD24                 move    $a1, $s0
	ROM:9F00CD28                 move    $a2, $zero
	ROM:9F00CD2C                 jal     b_CallRAMFlashFunction_
	ROM:9F00CD30                 move    $a3, $zero
	ROM:9F00CD34                 move    $s1, $v0
	ROM:9F00CD38                 bnez    $s1, loc_9F00CD4C
	ROM:9F00CD3C                 nop
	ROM:9F00CD40                 jal     b_CheckForBungedMX_
	ROM:9F00CD44                 move    $a0, $s0
	ROM:9F00CD48                 move    $s1, $v0
	ROM:9F00CD4C
	ROM:9F00CD4C loc_9F00CD4C:                            # CODE XREF: b_idFlash_+30↑j
	ROM:9F00CD4C                 li      $a0, a32BitFlash08lx  # "(32 bit) Flash @ %08lx is: "
	ROM:9F00CD54                 jal     b_printf_
	ROM:9F00CD58                 move    $a1, $s0
	ROM:9F00CD5C                 lui     $a0, 0x9F15
	ROM:9F00CD60                 jal     b_printf_
	ROM:9F00CD64                 li      $a0, asc_9F149F78  # "\n"
	ROM:9F00CD68                 lw      $v0, b_flashDescriptors_
	ROM:9F00CD70                 beqz    $v0, loc_9F00CE10
	ROM:9F00CD74                 lui     $a3, 0x5769
	ROM:9F00CD78                 li      $a3, 0x57696C64
	ROM:9F00CD7C                 li      $a0, b_flashDescriptors_
	ROM:9F00CD84                 move    $a2, $zero
	ROM:9F00CD88                 move    $s0, $a0
	ROM:9F00CD8C                 li      $a1, 0x800008B0
	ROM:9F00CD94                 lw      $v0, (b_flashDescriptors_ - 0x9F14A474)($a0)
	ROM:9F00CD98
	ROM:9F00CD98 loc_9F00CD98:                            # CODE XREF: b_idFlash_+100↓j
	ROM:9F00CD98                 bnel    $v0, $s1, loc_9F00CDFC
	ROM:9F00CD9C                 addiu   $a0, 0x1C
	ROM:9F00CDA0                 lw      $v1, dword_9F14A47C($a2)
	ROM:9F00CDAC                 lw      $v0, 0x18+var_4($sp)
	ROM:9F00CDB0                 bnel    $v1, $v0, loc_9F00CDFC
	ROM:9F00CDB4                 addiu   $a0, 0x1C
	ROM:9F00CDB8                 lw      $v1, dword_9F14A478($a2)
	ROM:9F00CDC4                 beq     $v1, $a3, loc_9F00CDD8
	ROM:9F00CDC8                 nop
	ROM:9F00CDCC                 lw      $v0, 0x18+var_8($sp)
	ROM:9F00CDD0                 bne     $v1, $v0, loc_9F00CDFC
	ROM:9F00CDD4                 addiu   $a0, 0x1C
	ROM:9F00CDD8
	ROM:9F00CDD8 loc_9F00CDD8:                            # CODE XREF: b_idFlash_+BC↑j
	ROM:9F00CDD8                 lw      $a1, 0($a1)
	ROM:9F00CDDC                 lui     $a0, 0x9F15
	ROM:9F00CDE0                 jal     b_printf_
	ROM:9F00CDE4                 li      $a0, aS          # "%s"
	ROM:9F00CDE8                 lui     $a0, 0x9F15
	ROM:9F00CDEC                 jal     b_printf_
	ROM:9F00CDF0                 li      $a0, asc_9F149F78  # "\n"
	ROM:9F00CDF4                 j       loc_9F00CE38
	ROM:9F00CDF8                 move    $v0, $s0
	ROM:9F00CDFC  # ---------------------------------------------------------------------------
	ROM:9F00CDFC
	ROM:9F00CDFC loc_9F00CDFC:                            # CODE XREF: b_idFlash_:loc_9F00CD98↑j
	ROM:9F00CDFC                                          # b_idFlash_+A8↑j ...
	ROM:9F00CDFC                 addiu   $a2, 0x1C
	ROM:9F00CE00                 addiu   $s0, 0x1C
	ROM:9F00CE04                 lw      $v0, 0($a0)
	ROM:9F00CE08                 bnez    $v0, loc_9F00CD98
	ROM:9F00CE0C                 addiu   $a1, 4
	ROM:9F00CE10
	ROM:9F00CE10 loc_9F00CE10:                            # CODE XREF: b_idFlash_+68↑j
	ROM:9F00CE10                 li      $a0, aUnrecognizedSt  # "UNRECOGNIZED: style = %08lx, mfrID = %0"...
	ROM:9F00CE18                 lw      $a2, 0x18+var_8($sp)
	ROM:9F00CE1C                 lw      $a3, 0x18+var_4($sp)
	ROM:9F00CE20                 jal     b_printf_
	ROM:9F00CE24                 move    $a1, $s1
	ROM:9F00CE28                 lui     $a0, 0x9F15
	ROM:9F00CE2C                 jal     b_printf_
	ROM:9F00CE30                 li      $a0, asc_9F149F78  # "\n"
	ROM:9F00CE34                 move    $v0, $zero
	ROM:9F00CE38
	ROM:9F00CE38 loc_9F00CE38:                            # CODE XREF: b_idFlash_+EC↑j
	ROM:9F00CE38                 lw      $ra, 0x18+var_s8($sp)
	ROM:9F00CE3C                 lw      $s1, 0x18+var_s4($sp)
	ROM:9F00CE40                 lw      $s0, 0x18+var_s0($sp)
	ROM:9F00CE44                 jr      $ra
	ROM:9F00CE48                 addiu   $sp, 0x28
	ROM:9F00CE48  # End of function b_idFlash_
	ROM:9F00CE48

	*/	

	//AMD_29F800B_16BIT(config, m_flash0, 0);
	//AMD_29F800B_16BIT(config, m_flash1, 0);
	MACRONIX_16161616(config, m_flash0, 0);
	MACRONIX_16161616(config, m_flash1, 0);

	DS2401(config, m_serial_id, 0);

	I2C_24C01(config, m_nvram, 0);
	m_nvram->set_e0(0);
	m_nvram->set_wc(1);

	SPOT_ASIC(config, m_spotasic, SYSCLOCK);
	m_spotasic->set_hostcpu(m_maincpu);
	m_spotasic->set_serial_id(m_serial_id);
	m_spotasic->set_nvram(m_nvram);
}

void webtv1_state::webtv1_sony(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv1_base(config);
}

void webtv1_state::webtv1_philips(machine_config& config)
{
	// manufacturer is determined by the contents of DS2401
	webtv1_base(config);
}

void webtv1_state::machine_start()
{

}

void webtv1_state::machine_reset()
{

}

INPUT_CHANGED_MEMBER(webtv1_state::pbuff_index_changed)
{
	m_spotasic->pixel_buffer_index_update();
}

// Sysconfig options are usually configured via resistors on the board.
static INPUT_PORTS_START( sys_config )
	PORT_START("sys_config")

	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")
	
	PORT_DIPNAME(0x0c, 0x0c, "Board type")
	PORT_DIPSETTING(0x00, "Reserved")
	PORT_DIPSETTING(0x04, "Reserved")
	PORT_DIPSETTING(0x08, "Trial-type board")
	PORT_DIPSETTING(0x0c, "FCS board (retail)")

	PORT_DIPNAME(0xf0, 0x80, "Board revision");

	PORT_DIPUNUSED_DIPLOC(0x100, 0x100, "SW1:8")
	PORT_DIPUNUSED_DIPLOC(0x200, 0x200, "SW1:9")
	PORT_DIPUNUSED_DIPLOC(0x400, 0x400, "SW1:10")

	PORT_DIPNAME(0x800, 0x800, "NTSC/PAL");
	PORT_DIPSETTING(0x000, "PAL mode w/ 14.75MHz pixel clock")
	PORT_DIPSETTING(0x800, "NTSC mode w/ 12.26MHz pixel clock")

	PORT_DIPUNUSED_DIPLOC(0x1000, 0x1000, "SW1:12")

	PORT_DIPNAME(0x2000, 0x2000, "CPU output buffers")
	PORT_DIPSETTING(0x0000, "83% CPU output buffers on reset")
	PORT_DIPSETTING(0x2000, "50% CPU output buffers on reset (emulated behavior)")

	PORT_DIPNAME(0xc000, 0x8000, "CPU clock multiplier");
	PORT_DIPSETTING(0x0000, "CPU clock = 5X bus clock")
	PORT_DIPSETTING(0x4000, "CPU clock = 4X bus clock")
	PORT_DIPSETTING(0x8000, "CPU clock = 2X bus clock (emulated behavior)")
	PORT_DIPSETTING(0xc000, "CPU clock = 3X bus clock")

	PORT_DIPNAME(0x10000, 0x00000, "vidUnit clock source")
	PORT_DIPSETTING(0x00000, "Use external video clock")
	PORT_DIPSETTING(0x10000, "SPOT controlled video clock")

	PORT_DIPNAME(0x20000, 0x00000, "audUnit clock source")
	PORT_DIPSETTING(0x00000, "SPOT controlled DAC clock")
	PORT_DIPSETTING(0x20000, "Use external DAC clock")

	PORT_DIPNAME(0xc0000, 0xc0000, "audUnit DAC type");
	PORT_DIPSETTING(0x00000, "Reserved")
	PORT_DIPSETTING(0x40000, "Reserved")
	PORT_DIPSETTING(0x80000, "Reserved")
	PORT_DIPSETTING(0xc0000, "AKM 4310/4309")
	
	PORT_DIPNAME(0x300000, 0x200000, "Memory vendor")
	PORT_DIPSETTING(0x000000, "Other")
	PORT_DIPSETTING(0x100000, "Samsung")
	PORT_DIPSETTING(0x200000, "Fujitsu")
	PORT_DIPSETTING(0x300000, "NEC")

	PORT_DIPNAME(0xc00000, 0xC00000, "Memory speed")
	PORT_DIPSETTING(0x000000, "100MHz parts")
	PORT_DIPSETTING(0x400000, "66MHz parts")
	PORT_DIPSETTING(0x800000, "77MHz parts")
	PORT_DIPSETTING(0xc00000, "83MHz parts")

	PORT_DIPNAME(0x3000000, 0x3000000, "Bank 1 ROM speed")
	PORT_DIPSETTING(0x0000000, "200ns/100ns")
	PORT_DIPSETTING(0x1000000, "100ns/50ns")
	PORT_DIPSETTING(0x2000000, "90ns/45ns")
	PORT_DIPSETTING(0x3000000, "120ns/60ns")

	PORT_DIPNAME(0x4000000, 0x0000000, "Bank 1 ROM mode")
	PORT_DIPSETTING(0x0000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x4000000, "Supports page mode")

	PORT_DIPNAME(0x8000000, 0x8000000, "Bank 1 ROM type")
	PORT_DIPSETTING(0x0000000, "Flash ROM")
	PORT_DIPSETTING(0x8000000, "Mask ROM (emulated behavior)")
	
	PORT_DIPNAME(0x30000000, 0x20000000, "Bank 0 ROM speed")
	PORT_DIPSETTING(0x00000000, "200ns/100ns")
	PORT_DIPSETTING(0x10000000, "100ns/50ns")
	PORT_DIPSETTING(0x20000000, "90ns/45ns")
	PORT_DIPSETTING(0x30000000, "120ns/60ns")

	PORT_DIPNAME(0x40000000, 0x00000000, "Bank 0 ROM mode")
	PORT_DIPSETTING(0x00000000, "No page mode (emulated behavior)")
	PORT_DIPSETTING(0x40000000, "Supports page mode")

	PORT_DIPNAME(0x80000000, 0x00000000, "Bank 0 ROM type")
	PORT_DIPSETTING(0x00000000, "Flash ROM (emulated behavior)")
	PORT_DIPSETTING(0x80000000, "Mask ROM")
INPUT_PORTS_END

// This is emulator-specific config options that go beyond sysconfig offers.
static INPUT_PORTS_START( emu_config )
	PORT_START("emu_config")

	PORT_CONFNAME(0x03, 0x00, "Pixel buffer index") PORT_CHANGED_MEMBER(DEVICE_SELF, webtv1_state, pbuff_index_changed, 0)
	PORT_CONFSETTING(0x00, "Use pixel buffer 0")
	PORT_CONFSETTING(0x01, "Use pixel buffer 1")

	PORT_CONFNAME(0x0c, 0x04, "Smartcard bangserial")
	PORT_CONFSETTING(0x00, "Off")
	PORT_CONFSETTING(0x04, "V1 bangserial data")
	PORT_CONFSETTING(0x08, "V2 bangserial data")

	PORT_CONFNAME(0x10, 0x10, "Allow real-time screen size updates")
	PORT_CONFSETTING(0x00, "No")
	PORT_CONFSETTING(0x10, "Yes")

	PORT_CONFNAME(0x20, 0x20, "Interrupts enabled")
	PORT_CONFSETTING(0x00, "No")
	PORT_CONFSETTING(0x20, "Yes")
INPUT_PORTS_END

static INPUT_PORTS_START( webtv1_input )
	PORT_INCLUDE(sys_config)
	PORT_INCLUDE(emu_config)
INPUT_PORTS_END

ROM_START( wtv1sony )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
ROM_END

ROM_START( wtv1phil )
	ROM_REGION(0x8, "serial_id", 0)     /* Electronic Serial DS2401 */
	ROM_LOAD("ds2401.bin", 0x0000, 0x0008, NO_DUMP)

	ROM_REGION32_BE(0x800000, "bank1", 0)
	ROM_LOAD("bootrom.o", 0x000000, 0x200000, NO_DUMP) /* pre-decoded; from archival efforts of the WebTV update servers */
	ROM_RELOAD(0x200000, 0x200000)
	ROM_RELOAD(0x400000, 0x200000)
ROM_END

//    YEAR  NAME      PARENT  COMPAT  MACHINE         INPUT         CLASS         INIT        COMPANY               FULLNAME                            FLAGS
CONS( 1996, wtv1sony,      0,      0, webtv1_sony,    webtv1_input, webtv1_state, empty_init, "Sony",               "INT-W100 WebTV Internet Terminal", MACHINE_NOT_WORKING + MACHINE_NO_SOUND )
CONS( 1996, wtv1phil,      0,      0, webtv1_philips, webtv1_input, webtv1_state, empty_init, "Philips-Magnavox",   "MAT960 WebTV Internet Terminal",   MACHINE_NOT_WORKING + MACHINE_NO_SOUND )