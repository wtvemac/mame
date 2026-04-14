// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_WEBTV_I82830_VGA_H
#define MAME_WEBTV_I82830_VGA_H

#pragma once

#include "machine/pci.h"
#include "video/pc_vga.h"

class i82830_svga_device : public svga_device
{

public:

	static constexpr uint32_t VGA_IO_BASE                = 0x03b0;
	static constexpr uint32_t VGA_IO_SIZE                = 0x0030;
	static constexpr uint32_t VGA_MEM_BASE               = 0xa0000;
	static constexpr uint32_t VGA_MEM_SIZE               = 0x20000;

	// VGA and Extended VGA registers mapped into I/O space and GFX management map (0x00000h-0x01FFF)
	// VGA: SMRAM Registers
	static constexpr uint32_t MM_VGA_SMRAM               = 0x10;
	// VGA: Graphics Control Registers
	static constexpr uint32_t MM_VGA_GCNTL_INDEX         = 0x3ce;
	static constexpr uint32_t MM_VGA_GCNTL_DATA          = 0x3cf;
	static constexpr uint32_t MM_VGA_GCNTL_10            = 0x10;
	static constexpr uint32_t MM_VGA_GCNTL_11            = 0x11;
	// VGA: CRT Controller Registers
	static constexpr uint32_t MM_VGA_CRT_INDEX_MDA       = 0x3b4;
	static constexpr uint32_t MM_VGA_CRT_INDEX_CGA       = 0x3d4;
	static constexpr uint32_t MM_VGA_CRT_DATA_MDA        = 0x3b5;
	static constexpr uint32_t MM_VGA_CRT_DATA_CGA        = 0x3d5;
	static constexpr uint32_t MM_VGA_CRT_CR30            = 0x30;
	static constexpr uint32_t MM_VGA_CRT_CR31            = 0x31;
	static constexpr uint32_t MM_VGA_CRT_CR32            = 0x32;
	static constexpr uint32_t MM_VGA_CRT_CR33            = 0x33;
	static constexpr uint32_t MM_VGA_CRT_CR35            = 0x35;
	static constexpr uint32_t MM_VGA_CRT_CR39            = 0x39;
	static constexpr uint32_t MM_VGA_CRT_CR40            = 0x40;
	static constexpr uint32_t MM_VGA_CRT_CR41            = 0x41;
	static constexpr uint32_t MM_VGA_CRT_CR42            = 0x42;
	static constexpr uint32_t MM_VGA_CRT_CR70            = 0x70;
	static constexpr uint32_t MM_VGA_CRT_CR80            = 0x80;
	static constexpr uint32_t MM_VGA_CRT_CR81            = 0x82;
	// General Control and Status Registers
	static constexpr uint32_t MM_EVGA_ST00               = 0x3c2;
	static constexpr uint32_t MM_EVGA_ST01_MDA           = 0x3ba;
	static constexpr uint32_t MM_EVGA_ST01_CGA           = 0x3da;
	static constexpr uint32_t MM_EVGA_FRC_READ           = 0x3ca;
	static constexpr uint32_t MM_EVGA_FRC_WRITE_MDA      = 0x3ba;
	static constexpr uint32_t MM_EVGA_FRC_WRITE_CGA      = 0x3da;
	static constexpr uint32_t MM_EVGA_MSR_READ           = 0x3cc;
	static constexpr uint32_t MM_EVGA_MSR_WRITE          = 0x3c2;
	// Extended VGA: Sequencer Registers
	static constexpr uint32_t MM_EVGA_SEQ_INDEX          = 0x3c4;
	static constexpr uint32_t MM_EVGA_SEQ_DATA           = 0x3c5;
	static constexpr uint32_t MM_EVGA_SEQ_SR01           = 0x01;
	static constexpr uint32_t MM_EVGA_SEQ_SR02           = 0x02;
	static constexpr uint32_t MM_EVGA_SEQ_SR03           = 0x03;
	static constexpr uint32_t MM_EVGA_SEQ_SR04           = 0x04;
	static constexpr uint32_t MM_EVGA_SEQ_SR07           = 0x07;
	// Extended VGA: Graphics Controller Registers
	static constexpr uint32_t MM_EVGA_GCNTL_GR00         = 0x00;
	static constexpr uint32_t MM_EVGA_GCNTL_GR01         = 0x01;
	static constexpr uint32_t MM_EVGA_GCNTL_GR02         = 0x02;
	static constexpr uint32_t MM_EVGA_GCNTL_GR03         = 0x03;
	static constexpr uint32_t MM_EVGA_GCNTL_GR04         = 0x04;
	static constexpr uint32_t MM_EVGA_GCNTL_GR05         = 0x05;
	static constexpr uint32_t MM_EVGA_GCNTL_GR06         = 0x06;
	static constexpr uint32_t MM_EVGA_GCNTL_GR07         = 0x07;
	static constexpr uint32_t MM_EVGA_GCNTL_GR08         = 0x08;
	// Exended VGA: Attribute Controller Registers
	static constexpr uint32_t MM_EVGA_ATTR_WRITE         = 0x3c0;
	static constexpr uint32_t MM_EVGA_ATTR_READ          = 0x3c1;
	// Extended VGA: CULT
	static constexpr uint32_t MM_EVGA_CLUT_DAC_DATA      = 0x3c9;
	static constexpr uint32_t MM_EVGA_CLUT_DAC_READ_IDX  = 0x3c7;
	static constexpr uint32_t MM_EVGA_CLUT_DAC_WRITE_IDX = 0x3c8;
	static constexpr uint32_t MM_EVGA_CLUT_DAC_MASK      = 0x3c6;
	// Extended VGA: CRT Controller Registers
	static constexpr uint32_t MM_EVGA_CRT_CR00           = 0x00;
	static constexpr uint32_t MM_EVGA_CRT_CR01           = 0x01;
	static constexpr uint32_t MM_EVGA_CRT_CR02           = 0x02;
	static constexpr uint32_t MM_EVGA_CRT_CR03           = 0x03;
	static constexpr uint32_t MM_EVGA_CRT_CR04           = 0x04;
	static constexpr uint32_t MM_EVGA_CRT_CR05           = 0x05;
	static constexpr uint32_t MM_EVGA_CRT_CR06           = 0x06;
	static constexpr uint32_t MM_EVGA_CRT_CR07           = 0x07;
	static constexpr uint32_t MM_EVGA_CRT_CR08           = 0x08;
	static constexpr uint32_t MM_EVGA_CRT_CR09           = 0x09;
	static constexpr uint32_t MM_EVGA_CRT_CR0A           = 0x0a;
	static constexpr uint32_t MM_EVGA_CRT_CR0B           = 0x0b;
	static constexpr uint32_t MM_EVGA_CRT_CR0C           = 0x0c;
	static constexpr uint32_t MM_EVGA_CRT_CR0D           = 0x0d;
	static constexpr uint32_t MM_EVGA_CRT_CR0E           = 0x0e;
	static constexpr uint32_t MM_EVGA_CRT_CR0F           = 0x0f;
	static constexpr uint32_t MM_EVGA_CRT_CR10           = 0x10;
	static constexpr uint32_t MM_EVGA_CRT_CR11           = 0x11;
	static constexpr uint32_t MM_EVGA_CRT_CR12           = 0x12;
	static constexpr uint32_t MM_EVGA_CRT_CR13           = 0x13;
	static constexpr uint32_t MM_EVGA_CRT_CR14           = 0x14;
	static constexpr uint32_t MM_EVGA_CRT_CR15           = 0x15;
	static constexpr uint32_t MM_EVGA_CRT_CR16           = 0x16;
	static constexpr uint32_t MM_EVGA_CRT_CR17           = 0x17;
	static constexpr uint32_t MM_EVGA_CRT_CR18           = 0x18;

	i82830_svga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void io_map(address_map &map);
	void mem_map(address_map &map);

protected:

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	void set_default_values();

	uint8_t cool_address_r(offs_t offset);
	void cool_address_w(offs_t offset, uint8_t data);

};

DECLARE_DEVICE_TYPE(I82830_VGA, i82830_svga_device)

#endif // MAME_WEBTV_I82830_VGA_H
