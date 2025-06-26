
/***************************************************************************************
 *
 * M-Systems DiskOnChip for WebTV
 *
 * WebTV's New Plus and BPS (New Classic) had a flashdisk device from M-Systems that replaced
 * previous boxes that had hard drives. This is the implementation of that flashdisk device.
 *
 * Here's some information about the some data that's stored on this device:
 *
 * WebTV writes this to sector 3 (offset 0x600) to indicate the addresses and length 
 * of core data used to configure the box.
 * 
 * I'm referneces these lengths in secors but the WebTV OS references these as blocks
 * but these are not the same length as what the DiskOnChip FTL (TrueFFS) uses as a block.
 * More about the DiskOnChip's FTL is below.
 * 
 * struct {
 *     uint32_t unknown00x00; // Def: 0x00004000; Desc: Primary NV allocate length (in bytes);          case 0/1   length in RWDiskSpecial
 *     uint32_t unknown01x04; // Def: 0x00004000; Desc: Secondary NV allocate length (in bytes);        case 2/3   length in RWDiskSpecial
 *     uint32_t unknown02x08; // Def: 0x00000000; Desc: Browser allocate length (in bytes);             case 4/5   length in RWDiskSpecial
 *     uint32_t unknown03x0c; // Def: 0x00000000; Desc: Select allocate length (in bytes);              case 6/7/8 length in RWDiskSpecial
 *     uint32_t unknown04x10; // Def: 0x00000000; Desc: Diag allocate length (in bytes);                case 9     length in RWDiskSpecial
 *     uint32_t unknown05x14; // Def: 0x00000000; Desc: Tier 3 diag allocate length (in bytes);         case 10    length in RWDiskSpecial
 *     uint32_t unknown06x18; // Def: 0x00000000
 *     uint32_t unknown07x1c; // Def: 0x00008000
 *     uint32_t unknown08x20; // Def: 0x00000020; Desc: Primary NV length (in sectors)
 *     uint32_t unknown09x24; // Def: 0x00000020; Desc: Secondary NV length (in sectors)
 *     uint32_t unknown0ax28; // Def: 0x00000000; Desc: Browser length (in sectors)
 *     uint32_t unknown0bx2c; // Def: 0x00000000
 *     uint32_t unknown0cx30; // Def: 0x00000000
 *     uint32_t unknown0dx34; // Def: 0x00000000; Desc: Diag build length (in sectors)
 *     uint32_t unknown0ex38; // Def: 0x00000000; Desc: Tier 3 diag build length (in sectors)
 *     uint32_t unknown0fx3c; // Def: 0x00000004
 *     uint32_t unknown10x40; // Def: 0x00000000
 *     uint32_t unknown11x44; // Def: 0x00000000; Desc: Primary NV 0 start address (in sectors);         case 0     offset in RWDiskSpecial
 *     uint32_t unknown12x48; // Def: 0x00000020
 *     uint32_t unknown13x4c; // Def: 0x00000020; Desc: Secondary NV 0 start address (in sectors);       case 2     offset in RWDiskSpecial
 *     uint32_t unknown14x50; // Def: 0x00000040; Desc: Diag build start address (in sectors);           case 9     offset in RWDiskSpecial
 *     uint32_t unknown15x54; // Def: 0x00000044; Desc: Browser 0 start address (in sectors);            case 4     offset in RWDiskSpecial
 *     uint32_t unknown16x58; // Def: 0x00000044; Desc: Browser 1 start address (in sectors);            case 5     offset in RWDiskSpecial
 *     uint32_t unknown17x5c; // Def: 0x00000040; Desc: Browser select toggle address (in sectors);      case 8     offset in RWDiskSpecial
 *     uint32_t unknown18x60; // Def: 0x00000000; Desc: Primary NV 1 start address (in sectors);         case 1     offset in RWDiskSpecial
 *     uint32_t unknown19x64; // Def: 0x00000040
 *     uint32_t unknown1ax68; // Def: 0x00000020; Desc: Secondary NV 1 start address (in sectors);       case 3     offset in RWDiskSpecial
 *     uint32_t unknown1bx6c; // Def: 0x00000040; Desc: Primary NV select toggle address (in sectors);   case 6     offset in RWDiskSpecial
 *     uint32_t unknown1cx70; // Def: 0x00000040; Desc: Secondary NV select toggle address (in sectors); case 7     offset in RWDiskSpecial
 *     uint32_t unknown1dx74; // Def: 0x00000040; Desc: Tier 3 diag build start address (in sectors);    case 10    offset in RWDiskSpecial
 *     uint32_t unknown1ex78; // Def: 0x00000040
 *     uint32_t unknown1fx7c; // Def: 0x00000044
 *     uint32_t unknown20x80; // Def: 0x00000000
 *     uint32_t unknown21x84; // Def: 0x000003b8; Desc: Checksum (per-byte) needs to be correct otherwise this admin info block is trashed
 *     uint32_t unknown22x88; // Def: 0xf7838254; Desc: not sure, always needs to be 0xf7838254 otherwise the approm will trash this admin info block. Bootrom doesn't check this.
 *     
 * }
 * 
 * Data is stored in sections on the DiskOnChip's NAND flash. The smallest readable and
 * writable section is called a page or sector, the smallest erasable section is called
 * a block or unit. Blocks contain multiple pages. You can read data at any time but you
 * can't write unless a block was erased first. If you want to update one byte of data you
 * need to erase an entire block. NAND flash only allows so many erases before a block 
 * cannot be erased anymore. There's a clever management (wear leveling) algorithm that 
 * runs in software and will help extend the life by optimizing for the least amount of
 * erases for every write required.
 * 
 * The wear leveling algorithm creates a table of all the blocks on the device with 
 * information about the state of each block. The table contains how many times each block
 * has been erased, if data is currently written to it, where in the sequence the block is
 * at and other state information. If one byte need to be updated in a page, you'd normally
 * need to do a full block erase then re-write each page. To reduce the amount of erases
 * needed, we look for a block that's fresh and re-write the page with the byte you want to
 * update then mark both the previous block (block A) and updated block (block B) so we use
 * data can be re-constructed. The pages that were updated in block B are merged into block A
 * to form the data read from the OS. Sometimess two blocks are physically merged into each
 * other on the flash in a "fold" operation if we need optimize for space. If a block is erased
 * the max amount of times then it's marked bad and that block is ignored.
 * 
 * As you'd expect, data can become out-of-order with this algorithm. There's a translation
 * layer ("NAND Flash Translation Layer") so the data is merged and appares in sequence 
 * correctly to the OS even though it's out of order on the NAND flash.
 * 
 * There's a special block reserved for the NFTL header information. This block is searched
 * at the start and contains this data:
 * 
 * struct {
 *     uint8_t unknown00[6]; // Always: ANAND\x00 this is the magic.
 *     uint8_t unknown01;    // Def: 0x0004; Desc: number of pages in a block. Used to tell how much data an erase operation wipes.
 *     uint16_t unknown02;   // Def: 0x0000; Desc: number of blocks reserved at the start for extra data. Usually 0.
 *     uint32_t unknown03;   // Def: 0x0040fa00; Desc: The total addressable size of the NAND flash
 * }
 * 
 * Each page (in m_data_usr) has spare data (in m_data_oob) that's usually 16 bytes long. MAME
 * creates a file with the usr m_data_usr at the top and m_data_oob appended to the bottom.
 * 
 * Each page normaly has 16 bytes of m_data_oob data. The first 8 bytes stores ECC and status
 * information for each page. The next 8 bytes for the first 3 pages in a block are used for 
 * NFTL data. This NFTL data is called the "Unit Control Information"
 * 
 * Reading in all the NFTL Unit Control Information for each block is done after the header
 * is found.
 * 
 * Unit Control Information 0 (first page):
 * 
 *     The user and spare state are separated but in practice the spare and user data share 
 *     the same state so the status is just repeated.
 * 
 *     struct {
 *         uint16_t unknown00; // Virtual address of this block (user)
 *         uint16_t unknown01; // If this isn't 0xffff then this indicates another block that has pages that replace data in this block.
 *         uint16_t unknown02; // Repeat of unknown00
 *         uint16_t unknown03; // Repeat of unknown01
 *     }
 * 
 * Unit Control Information 1 (second page):
 *     struct {
 *         uint32_t unknown00; // The number of times this block was erased (wear level)
 *         uint16_t unknown01; // Usually "0x3c69" (erase mark) if this block was erased at least once
 *         uint16_t unknown02; // Usually "0x3c69" (erase mark) if this block was erased at least once
 *     }
 * 
 * Unit Control Information 2 (thid page):
 * 
 *     If data is being merged (folded or consolidated) into this block then unknown00
 *     and unknown01 is marked with 0x5555, otherwise it's 0xffff. This is part of the 
 *     NFTL garbage collector.
 * 
 *     struct {
 *         uint16_t unknown00;
 *         uint16_t unknown01;
 *         uint32_t unused;
 *     }
 * 
 * 
 * The marketing term M-System's used for all this specific crazyiness is "TrueFFS" but this
 * type of algorithm is usually used in all NAND devices (including flash drives and SSDs)
 *
 ***************************************************************************************/

#include "emu.h"
#include "mdoc.h"

mdoc_chip_base::mdoc_chip_base(
		const machine_config &mconfig,
		device_type type,
		const char *tag,
		device_t *owner,
		uint32_t clock,
		mdoc_chip_id_t chip_id,
		mdoc_flash_manufacturer_t flash_mfg_id,
		uint8_t flash_dev_id,
		uint16_t page_usr_size_bytes,
		uint16_t page_oob_size_bytes,
		uint32_t total_usr_size_bytes
	) : device_t(mconfig, type, tag, owner, clock),
	device_nvram_interface(mconfig, *this),
	m_chip_id(chip_id),
	m_chip_flash_mfg_id(flash_mfg_id),
	m_chip_flash_dev_id(flash_dev_id),
	m_page_usr_size_bytes(page_usr_size_bytes),
	m_page_oob_size_bytes(page_oob_size_bytes),
	m_total_usr_size_bytes(total_usr_size_bytes)
{
}

void mdoc_chip_base::device_start()
{
	m_addressable_pages = (m_total_usr_size_bytes / m_page_usr_size_bytes);
	m_total_oob_size_bytes = (m_addressable_pages * m_page_oob_size_bytes);
	m_total_size_bytes = (m_total_usr_size_bytes + m_total_oob_size_bytes);

	mdoc_chip_base::reset_state();

	m_data_usr = make_unique_clear<uint8_t[]>(m_total_usr_size_bytes);
	m_data_oob = make_unique_clear<uint8_t[]>(m_total_oob_size_bytes);
	m_data_chipid = ((m_chip_flash_mfg_id << 8) | (m_chip_flash_dev_id << 0));

	save_item(NAME(m_chip_id));
	save_item(NAME(m_chip_flash_mfg_id));
	save_item(NAME(m_chip_flash_dev_id));
	save_item(NAME(m_data_chipid));
	save_item(NAME(m_page_usr_size_bytes));
	save_item(NAME(m_page_oob_size_bytes));
	save_item(NAME(m_total_usr_size_bytes));
	save_item(NAME(m_total_oob_size_bytes));
	save_item(NAME(m_total_size_bytes));
	save_item(NAME(m_addressable_pages));
	save_item(NAME(m_alias));
	save_item(NAME(m_io_in_index));
	save_item(NAME(m_io_out_index));
	save_item(NAME(m_address));
	save_item(NAME(m_command));
	save_item(NAME(m_selected_data));
	save_item(NAME(m_seqin_enabled));
	save_item(NAME(m_cdsn_ctrl));
	save_item(NAME(m_flash_ctrl));
	save_item(NAME(m_ecc_config));
	save_item(NAME(m_ecc_stat));
	save_pointer(NAME(m_data_usr), m_total_usr_size_bytes);
	save_pointer(NAME(m_data_oob), m_total_oob_size_bytes);
}

void mdoc_chip_base::device_reset()
{
	mdoc_chip_base::reset_state();
}

void mdoc_chip_base::reset_state()
{
	m_alias = 0x0;
	m_io_in_index = 0x0;
	m_io_out_index = 0x0;
	m_address = 0x0;
	m_command = 0x0;
	mdoc_chip_base::set_data_mode(mdoc_data_mode_t::MDOC_MODE_DATA);
	mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA_NONE);
	m_seqin_enabled = false;
	m_cdsn_ctrl = 0x0;
	m_flash_ctrl = 0x0;
	m_ecc_config = 0x0;
	m_ecc_stat = 0x0;
}

void mdoc_chip_base::nvram_default()
{
	/*
		The main flash data is in m_data_usr (user data) while m_data_oob (out-of-band data) stores data used in the FTL algorithm.

		The FTL data is used to keep track of wear leveling (ecc data, status, wear info, and erase/fold mark data)
	*/
	memset(m_data_usr.get(), 0xff, m_total_usr_size_bytes);
	memset(m_data_oob.get(), 0xff, m_total_oob_size_bytes);
}

bool mdoc_chip_base::nvram_read(util::read_stream &file)
{
	std::error_condition err;
	size_t actual;

	std::tie(err, actual) = read(file, m_data_usr.get(), m_total_usr_size_bytes);
	if (!err && (actual == m_total_usr_size_bytes)) {
		std::tie(err, actual) = read(file, m_data_oob.get(), m_total_oob_size_bytes);

		return (!err && (actual == m_total_oob_size_bytes));
	} else {
		return false;
	}
}

bool mdoc_chip_base::nvram_write(util::write_stream &file)
{
	std::error_condition err;
	size_t actual;

	std::tie(err, actual) = write(file, m_data_usr.get(), m_total_usr_size_bytes);
	if (!err && (actual == m_total_usr_size_bytes)) {
		std::tie(err, actual) = write(file, m_data_oob.get(), m_total_oob_size_bytes);

		return (!err && (actual == m_total_oob_size_bytes));
	} else {
		return false;
	}
}

uint32_t mdoc_chip_base::read32(offs_t offset)
{
	uint32_t data = 0x00000000;

	// Convert to map()?
	switch (offset)
	{
		case mdoc_reg_t::MDOC_REG_CHIPID:
			data = m_chip_id;
			break;

		case mdoc_reg_t::MDOC_REG_ALIAS_RESOLUTION:
			data = m_alias;
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_CNTL:
			data = mdoc_chip_base::get_raw_cdsn_ctrl();
			break;

		// Called before the start of an IO read.
		case mdoc_reg_t::MDOC_REG_READPIPE_INIT:
			m_io_out_index = 0x0;
			break;

		// Called at the end of an IO read.
		case mdoc_reg_t::MDOC_REG_LASTDATA_READ:
			data = mdoc_chip_base::read_selected_data(true);
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_SLOW_IO:
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_IO0:
		case mdoc_reg_t::MDOC_REG_CDSN_IO1:
		case mdoc_reg_t::MDOC_REG_MIL_CDSN_IO0:
		case mdoc_reg_t::MDOC_REG_MIL_CDSN_IO1:
			data = mdoc_chip_base::read_selected_data(false);
			break;

		case mdoc_reg_t::MDOC_REG_ECC_CONFIG:
			data = mdoc_chip_base::read_ecc_config();
			break;

		case mdoc_reg_t::MDOC_REG_ECC_STAT:
			data = m_ecc_stat;
			break;

		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME0:
		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME1:
		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME2:
		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME3:
		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME4:
		case mdoc_reg_t::MDOC_REG_ECC_SYNDROME5:
			data = 0x00000000; // No error
			break;
		
		case mdoc_reg_t::MDOC_REG_STAT:
		case mdoc_reg_t::MDOC_REG_NOP:
			data = 0x00000000;
			// Reads to this are used as a delay cycle.
			break;

		default:
			data = 0x00000000;
			break;
	}

	return data;
}

uint32_t mdoc_chip_base::read_selected_data(bool final_read)
{
	uint32_t data = 0x00000000;

	if (mdoc_chip_base::is_enabled())
	{
		switch (mdoc_chip_base::get_data_area())
		{
			case mdoc_data_area_t::MDOC_DATA_CHIPID:
				data = mdoc_chip_base::read_chipid_data(final_read);
				break;

			case mdoc_data_area_t::MDOC_DATA0_USER:
			case mdoc_data_area_t::MDOC_DATA1_USER:
				data = mdoc_chip_base::read_user_data(final_read);
				break;

			case mdoc_data_area_t::MDOC_DATA2_SPARE:
				data = mdoc_chip_base::read_spare_data(final_read);
				break;

			case mdoc_data_area_t::MDOC_DATA_STATUS:
				data = 0x00;
				break;

			default:
				data = 0x00;
				break;
		}


		if (final_read)
		{
			m_io_out_index = 0x00;
		}
		else
		{
			m_io_out_index++;
		}
	}

	return data;
}

uint32_t mdoc_chip_base::get_data_offset()
{
	uint32_t offset = 0x00000000;

	switch (mdoc_chip_base::get_data_area())
	{
		case mdoc_data_area_t::MDOC_DATA_ERASE:
		{
			offset = m_address;
			break;
		}

		case mdoc_data_area_t::MDOC_DATA2_SPARE:
		{
			offset = (m_address >> 8) * m_page_oob_size_bytes;
			offset += (m_address & 0xff);
			break;
		}

		case mdoc_data_area_t::MDOC_DATA0_USER:
		case mdoc_data_area_t::MDOC_DATA1_USER:
		{
			offset = (m_address >> 8) * m_page_usr_size_bytes;
			offset += (m_address & 0xff);
			break;
		}
	}

	return offset;
}

uint32_t mdoc_chip_base::read_chipid_data(bool final_read)
{
	return ((m_data_chipid >> (8 - ((m_io_out_index & 0x1) * 8))) & 0xff);
}

uint32_t mdoc_chip_base::read_user_data(bool final_read)
{
	uint32_t data = m_data_usr[mdoc_chip_base::get_data_offset() + m_io_out_index];

	if (final_read)
	{
		mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA2_SPARE);
	}

	return data;
}

uint32_t mdoc_chip_base::read_spare_data(bool final_read)
{
	uint32_t data = m_data_oob[mdoc_chip_base::get_data_offset() + m_io_out_index];

	if (final_read)
	{
		m_address += ((m_io_out_index + 1) / m_page_usr_size_bytes) * 0x100;
	}

	return data;
}

uint32_t mdoc_chip_base::read_ecc_config()
{
	m_ecc_config ^= MDOC_ECC_TOGGLE_BIT;

	return m_ecc_config;
}

uint8_t mdoc_chip_base::get_raw_cdsn_ctrl()
{
	return m_cdsn_ctrl | mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_READY; // we're always ready;
}

bool mdoc_chip_base::is_enabled()
{
	return ((mdoc_chip_base::get_raw_cdsn_ctrl() & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_CE) != 0x0);
}

bool mdoc_chip_base::is_write_enabled()
{
	return ((mdoc_chip_base::get_raw_cdsn_ctrl() & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_WP) != 0x0);
}

uint8_t mdoc_chip_base::get_data_mode()
{
	if ((m_cdsn_ctrl & MDOC_CDSN_CTRL_CLE) != 0x0)
	{
		return MDOC_MODE_COMMAND;
	}
	else if ((m_cdsn_ctrl & MDOC_CDSN_CTRL_ALE) != 0x0)
	{
		return MDOC_MODE_ADDRESS;
	}
	else
	{
		return MDOC_MODE_DATA;
	}
}

bool mdoc_chip_base::is_flash_io_enabled()
{
	return ((mdoc_chip_base::get_raw_cdsn_ctrl() & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_FLASH_IO) != 0x0);
}

bool mdoc_chip_base::is_ecc_io_enabled()
{
	return ((mdoc_chip_base::get_raw_cdsn_ctrl() & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_ECC_IO) != 0x0);
}

uint8_t mdoc_chip_base::get_data_area()
{
	return (m_selected_data & 0xff);
}

void mdoc_chip_base::write32(offs_t offset, uint32_t data)
{
	// Convert to map()?
	switch (offset)
	{
		case mdoc_reg_t::MDOC_REG_ALIAS_RESOLUTION:
			m_alias = (data & 0xff);
			break;

		case mdoc_reg_t::MDOC_REG_CNTL:
			// on Millennium chips this is used as a delay cycle.
			break;

		case mdoc_reg_t::MDOC_REG_ECC_CONFIG:
		case mdoc_reg_t::MDOC_REG_MILPLUS_ECC_CONFIG:
			m_ecc_config = (data & 0xff);
			break;

		case mdoc_reg_t::MDOC_REG_ECC_STAT:
			m_ecc_stat = data;
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_CNTL:
			mdoc_chip_base::set_cdsn_ctrl(data);
			break;

		case mdoc_reg_t::MDOC_REG_MILPLUS_FLASH_CNTL:
			m_flash_ctrl = ((data & 0xff) & (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_MIL_READY));
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_IO0:
		case mdoc_reg_t::MDOC_REG_CDSN_IO1:
		case mdoc_reg_t::MDOC_REG_MIL_CDSN_IO0:
		case mdoc_reg_t::MDOC_REG_MIL_CDSN_IO1:
			mdoc_chip_base::handle_cdsn_io_write(data);
			break;

		case mdoc_reg_t::MDOC_REG_WRITEPIPE_TERM:
			break;

		default:
			break;

	}
}

void mdoc_chip_base::handle_cdsn_io_write(uint32_t data)
{
	if (mdoc_chip_base::is_enabled())
	{
		switch (mdoc_chip_base::get_data_mode())
		{
			case MDOC_MODE_COMMAND:
				mdoc_chip_base::set_command(data & 0xff);
				break;

			case MDOC_MODE_ADDRESS:
				mdoc_chip_base::set_address_byte(data & 0xff);
				break;

			case MDOC_MODE_DATA:
			default:
				mdoc_chip_base::write_selected_data(data & 0xff);
				break;

		}
	}
}

void mdoc_chip_base::write_selected_data(uint32_t data)
{
	if (mdoc_chip_base::is_enabled())
	{
		if (m_seqin_enabled)
		{
			switch (mdoc_chip_base::get_data_area())
			{
				case mdoc_data_area_t::MDOC_DATA0_USER:
				case mdoc_data_area_t::MDOC_DATA1_USER:
					mdoc_chip_base::write_user_data(data);
					break;

				case mdoc_data_area_t::MDOC_DATA2_SPARE:
					mdoc_chip_base::write_spare_data(data);
					break;

				default:
					break;
			}
		}
	}
}

void mdoc_chip_base::write_user_data(uint8_t data)
{
	m_data_usr[mdoc_chip_base::get_data_offset() + m_io_in_index] = (data & 0xff);
	m_io_in_index++;
}

void mdoc_chip_base::write_spare_data(uint8_t data)
{
	m_data_oob[mdoc_chip_base::get_data_offset() + m_io_in_index] = (data & 0xff);
	m_io_in_index++;
}

void mdoc_chip_base::erase_selected_data()
{
	uint32_t page_index = mdoc_chip_base::get_data_offset();

	uint32_t usr_offset = (page_index * m_page_usr_size_bytes);
	for (int usr_idx = 0; usr_idx < m_page_usr_size_bytes; usr_idx++)
	{
		m_data_usr[(usr_offset + usr_idx)] = 0xff;
	}

	uint32_t oob_offset = (page_index * m_page_oob_size_bytes);
	for (int oob_idx = 0; oob_idx < m_page_oob_size_bytes; oob_idx++)
	{
		m_data_oob[(oob_offset + oob_idx)] = 0xff;
	}
}

void mdoc_chip_base::set_cdsn_ctrl(uint32_t data)
{
	uint8_t changed_bits = (data ^ m_cdsn_ctrl);

	if ((changed_bits & mdoc_cdsn_ctrl_t::MDOC_CDSN_DATA_MODE) != 0x0)
	{
		m_io_in_index = 0x0;

		if ((data & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_ALE) != 0x0)
		{
			m_address = 0x0;
			mdoc_chip_base::set_data_mode(mdoc_data_mode_t::MDOC_MODE_ADDRESS);
		}
		else if ((data & mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_CLE) != 0x0)
		{
			m_command = 0x0;
			mdoc_chip_base::set_data_mode(mdoc_data_mode_t::MDOC_MODE_COMMAND);
		}
		else
		{
			mdoc_chip_base::set_data_mode(mdoc_data_mode_t::MDOC_MODE_DATA);
		}
	}

	mdoc_chip_base::set_other_cdsn_ctrl(data);

	if (!mdoc_chip_base::is_flash_io_enabled() && mdoc_chip_base::is_ecc_io_enabled())
	{
		mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA2_SPARE);
		m_io_in_index = 0;
	}
}

void mdoc_chip_base::set_command(uint8_t data)
{
	m_command = data;

	switch (m_command)
	{
		case mdoc_nand_cmd_t::MDOC_NAND_CMD_READ0:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA0_USER);
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_READ1:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA1_USER);
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_READID:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA_CHIPID);
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_STATUS:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA_STATUS);
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_SEQIN:
			m_seqin_enabled = true;
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_READOOB:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA2_SPARE);
			break;
		
		case mdoc_nand_cmd_t::MDOC_NAND_CMD_PAGEPROG:
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_ERASE1:
			mdoc_chip_base::set_data_area(mdoc_data_area_t::MDOC_DATA_ERASE);
			break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_ERASE2:
		mdoc_chip_base::erase_selected_data();
		break;

		case mdoc_nand_cmd_t::MDOC_NAND_CMD_RESET:
			mdoc_chip_base::reset_state();
			break;

		default:
			break;
	}

	m_io_out_index = 0x0;
	m_io_in_index = 0x0;
}

void mdoc_chip_base::set_address_byte(uint32_t data)
{
	data = ((data & 0xff) << (m_io_in_index * 8));
	m_io_in_index++;
	m_address |= data;
}

void mdoc_chip_base::set_raw_cdsn_ctrl(uint8_t cdsn_ctrl)
{
	cdsn_ctrl &= (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_READY); // We're always ready!

	m_cdsn_ctrl = cdsn_ctrl;
}

void mdoc_chip_base::set_enabled(bool value)
{
	uint8_t cdsn_ctrl = mdoc_chip_base::get_raw_cdsn_ctrl();

	if (value)
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl | (mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_CE));
	}
	else
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl & (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_CE));
	}
}

void mdoc_chip_base::set_write_enabled(bool value)
{
	uint8_t cdsn_ctrl = mdoc_chip_base::get_raw_cdsn_ctrl();

	if (value)
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl | (mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_WP));
	}
	else
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl & (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_WP));
	}
}

void mdoc_chip_base::set_flash_io_enabled(bool value)
{
	uint8_t cdsn_ctrl = mdoc_chip_base::get_raw_cdsn_ctrl();

	if (value)
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl | (mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_FLASH_IO));
	}
	else
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl & (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_FLASH_IO));
	}
}

void mdoc_chip_base::set_flash_ecc_io_enabled(bool value)
{
	uint8_t cdsn_ctrl = mdoc_chip_base::get_raw_cdsn_ctrl();

	if (value)
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl | (mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_ECC_IO));
	}
	else
	{
		mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl & (~mdoc_cdsn_ctrl_t::MDOC_CDSN_CTRL_ECC_IO));
	}
}

void mdoc_chip_base::set_other_cdsn_ctrl(uint8_t cdsn_ctrl)
{
	cdsn_ctrl &= 0xff;
	cdsn_ctrl &= (~(mdoc_cdsn_ctrl_t::MDOC_CDSN_DATA_MODE)); // Data mode/latches are set elsewhere.

	mdoc_chip_base::set_raw_cdsn_ctrl((m_cdsn_ctrl & mdoc_cdsn_ctrl_t::MDOC_CDSN_DATA_MODE) | cdsn_ctrl);
}

void mdoc_chip_base::set_data_mode(mdoc_data_mode_t data_mode)
{
	uint8_t cdsn_ctrl = mdoc_chip_base::get_raw_cdsn_ctrl();
	cdsn_ctrl &= (~(mdoc_cdsn_ctrl_t::MDOC_CDSN_DATA_MODE));

	switch (data_mode)
	{
		case MDOC_MODE_COMMAND:
			cdsn_ctrl |= MDOC_CDSN_CTRL_CLE;
			break;

		case MDOC_MODE_ADDRESS:
			cdsn_ctrl = MDOC_CDSN_CTRL_ALE;
			break;

		case MDOC_MODE_DATA:
		default:
			break;
	}

	mdoc_chip_base::set_raw_cdsn_ctrl(cdsn_ctrl);
}

void mdoc_chip_base::set_data_area(uint8_t data_area)
{
	m_selected_data = (data_area & 0xff);
}

DEFINE_DEVICE_TYPE(MDOC_COLLECTION, mdoc_collection, "mdoc_collection", "M-Systems DiskOnChip Collection")
mdoc_collection::mdoc_collection(
		const machine_config &mconfig,
		const char *tag,
		device_t *owner,
		uint32_t clock,
		std::function<void (mdoc_collection &, machine_config &)> setup_chips_func
	) : device_t(mconfig, MDOC_COLLECTION, tag, owner, clock),
	m_chip(*this, "flash%u", 0u)
{
	m_setup_chips_func = setup_chips_func;
}

uint32_t mdoc_collection::read32(offs_t offset)
{
	uint32_t data = 0x00000000;

	// Convert to map()?
	switch (offset)
	{
		case mdoc_reg_t::MDOC_REG_FLOOR_SELECT:
			data = m_selected_floor;
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_DEVICE_SELECT:
		case mdoc_reg_t::MDOC_REG_MILPLUS_DEVICE_SELECT:
			data = m_selected_chip;
			break;

		default:
			auto finder_target = m_chip[mdoc_collection::get_device_index(m_selected_floor, m_selected_chip)].finder_target();
			mdoc_chip_base* target_chip = finder_target.first.subdevice<mdoc_chip_base>(finder_target.second);
			if (target_chip)
			{
				data = target_chip->read32(offset);
			}
			break;
}
	

	return data;
}

void mdoc_collection::write32(offs_t offset, uint32_t data)
{
	// Convert to map()?
	switch (offset)
	{
		case mdoc_reg_t::MDOC_REG_FLOOR_SELECT:
			m_selected_floor = (data & 0xff) & (mdoc_collection::MAX_FLOORS - 1);
			m_selected_chip = 0x0;
			break;

		case mdoc_reg_t::MDOC_REG_CDSN_DEVICE_SELECT:
		case mdoc_reg_t::MDOC_REG_MILPLUS_DEVICE_SELECT:
			m_selected_chip = (data & 0xff) & (mdoc_collection::MAX_CHIPS_PER_FLOOR - 1);
			break;

		default:
			auto finder_target = m_chip[mdoc_collection::get_device_index(m_selected_floor, m_selected_chip)].finder_target();
			mdoc_chip_base* target_chip = finder_target.first.subdevice<mdoc_chip_base>(finder_target.second);
			if (target_chip)
			{
				target_chip->write32(offset, data);
			}
			break;
	}
}

uint8_t mdoc_collection::get_device_index(uint8_t floor_index, uint8_t chip_index)
{
	return ((floor_index * mdoc_collection::MAX_CHIPS_PER_FLOOR) + chip_index);
}

void mdoc_collection::device_add_mconfig(machine_config &config)
{
	if (m_setup_chips_func != nullptr)
		m_setup_chips_func(*this, config);
}

void mdoc_collection::device_start()
{
	m_selected_floor = 0;
	m_selected_chip = 0;

	save_item(NAME(m_selected_floor));
	save_item(NAME(m_selected_chip));
}

void mdoc_collection::device_reset()
{
}

//               CHIP TAG,     CHIP CLASS,     SHORT NAME,       LONG NAME,                           CHIP ID,         FLASH MANUFACTURER ID,   FLASH DEVICE ID, PAGE SIZE, OOB SIZE, USER SIZE IN MB

// M-systems DiskOnChip 2000 MD2200

// DiskOnChip MD2200-D02
// DiskOnChip MD2200-D04
// DiskOnChip MD2200-D08
// DiskOnChip MD2200-D12
// DiskOnChip MD2200-D16
// DiskOnChip MD2200-D24
// DiskOnChip MD2200-D48

// M-systems DiskOnChip 2000 MD2202

// DiskOnChip MD2200-D16
// DiskOnChip MD2200-D24
// DiskOnChip MD2200-D32
// DiskOnChip MD2200-D48
// DiskOnChip MD2200-D64
// DiskOnChip MD2200-D96
// DiskOnChip MD2200-D128
// DiskOnChip MD2200-D192
// DiskOnChip MD2200-D384

// M-systems DiskOnChip Millenium MD2800 (DIP), MD2810 (TSOP), MD2811 (Plus TSOP), MD3331/MD3831 (Plus BGA)

// The 2MB, 4MB and 16MB chips don't exist but works because WebTV's bootrom only supports the MDOC_CHIPID_MIL chip id but supports the smaller than 8MB flash device IDs

DEFINE_MDOC_CHIP(MD2810_D02,   mdoc_2810_0002, "mdoc_2810_0002", "M-Systems DiskOnChip MD2810-D02",   MDOC_CHIPID_MIL, MDOC_FLASH_MFG_SAMSUNG, 0x64,             0x100,     0x10,     2)              // Also mfg=MDOC_FLASH_MFG_TOSHIBA, dev=0x64
DEFINE_MDOC_CHIP(MD2810_D04,   mdoc_2810_0004, "mdoc_2810_0004", "M-Systems DiskOnChip MD2810-D04",   MDOC_CHIPID_MIL, MDOC_FLASH_MFG_SAMSUNG, 0xe5,             0x200,     0x10,     4)              // Also mfg=MDOC_FLASH_MFG_TOSHIBA, dev=0xe5
DEFINE_MDOC_CHIP(MD2810_D08,   mdoc_2810_0008, "mdoc_2810_0008", "M-Systems DiskOnChip MD2810-D08",   MDOC_CHIPID_MIL, MDOC_FLASH_MFG_SAMSUNG, 0xe6,             0x200,     0x10,     8)              // Also mfg=MDOC_FLASH_MFG_TOSHIBA, dev=0xe6
DEFINE_MDOC_CHIP(MD2810_D16,   mdoc_2810_0016, "mdoc_2810_0016", "M-Systems DiskOnChip MD2810-D16",   MDOC_CHIPID_MIL, MDOC_FLASH_MFG_SAMSUNG, 0x73,             0x200,     0x10,    16)              // Also mfg=MDOC_FLASH_MFG_TOSHIBA, dev=0x73
// DiskOnChip MD2811-D16
// DiskOnChip MD2811-D32
// DiskOnChip MD3331-D64

// M-systems DiskOnChip 2000 MD2203

// DiskOnChip MD2203-D80
// DiskOnChip MD2203-D112
// DiskOnChip MD2203-D114
// DiskOnChip MD2203-D160
// DiskOnChip MD2203-D192
// DiskOnChip MD2203-D224
// DiskOnChip MD2203-D256
// DiskOnChip MD2203-D288
// DiskOnChip MD2203-D576
// DiskOnChip MD2203-D768
// DiskOnChip MD2203-D1024
