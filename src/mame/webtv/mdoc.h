// license: BSD-3-Clause
// copyright-holders: wtvemac

// Description here

#ifndef MAME_MACHINE_MDOC_H
#define MAME_MACHINE_MDOC_H

#pragma once

#define DEFINE_MDOC_CHIP(A_type, A_class, A_short_name, A_full_name, A_chip_type, A_flash_mfg_id, A_flash_dev_id, A_page_usr_size_bytes, A_page_oob_size_bytes, A_total_usr_size_mbytes) \
	DEFINE_DEVICE_TYPE(A_type, A_class, A_short_name, A_full_name) \
	A_class::A_class(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) : \
		mdoc_chip_base( \
			mconfig, \
			A_type, \
			tag, \
			owner, \
			clock, \
			mdoc_chip_id_t::A_chip_type, \
			A_flash_mfg_id, \
			A_flash_dev_id, \
			A_page_usr_size_bytes, \
			A_page_oob_size_bytes, \
			(A_total_usr_size_mbytes * 1024 * 1024) \
		) {}

#define DECLARE_MDOC_CHIP(A_type, A_class) \
	class A_class : public mdoc_chip_base \
	{ \
		public: \
			A_class(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock); \
	}; \
	DECLARE_DEVICE_TYPE(A_type, A_class)

enum mdoc_chip_id_t : uint8_t
{
	MDOC_CHIPID_2K        = 0x20,
	MDOC_CHIPID_2K_TSOP   = 0x21,
	MDOC_CHIPID_MIL       = 0x30,
	MDOC_CHIPID_MILPLUS0  = 0x40,
	MDOC_CHIPID_MILPLUS1  = 0x41
};

enum mdoc_flash_manufacturer_t : uint8_t
{
	MDOC_FLASH_MFG_TOSHIBA = 0x98,
	MDOC_FLASH_MFG_SAMSUNG = 0xec
};

/*
	Main M-systems DiskOnChip Device
*/

class mdoc_chip_base : public device_t, public device_nvram_interface
{

	public:

		mdoc_chip_base(
			const machine_config &mconfig,
			device_type type,
			const char *tag,
			device_t *owner,
			uint32_t clock = 0,
			mdoc_chip_id_t chip_id = MDOC_CHIPID_2K,
			mdoc_flash_manufacturer_t flash_mfg_id = mdoc_flash_manufacturer_t::MDOC_FLASH_MFG_TOSHIBA,
			uint8_t flash_dev_id = 0x73,
			uint16_t page_usr_size_bytes = 0x200,
			uint16_t page_oob_size_bytes = 0x10,
			uint32_t total_size_bytes = (16 * 1024 * 1024)
		);

		uint32_t read32(offs_t offset);
		void write32(offs_t offset, uint32_t data);

	private:

		enum mdoc_reg_t : uint16_t
		{
			MDOC_REG_CHIPID                   = 0x1000,
			MDOC_REG_STAT                     = 0x1001,
			MDOC_REG_CNTL                     = 0x1002,
			MDOC_REG_FLOOR_SELECT             = 0x1003,
			MDOC_REG_CDSN_CNTL                = 0x1004,
			MDOC_REG_CDSN_DEVICE_SELECT       = 0x1005,
			MDOC_REG_ECC_CONFIG               = 0x1006,
			MDOC_REG_ECC_STAT                 = 0x1007,
			MDOC_REG_CDSN_SLOW_IO             = 0x100d,
			MDOC_REG_ECC_SYNDROME0            = 0x1010,
			MDOC_REG_ECC_SYNDROME1            = 0x1011,
			MDOC_REG_ECC_SYNDROME2            = 0x1012,
			MDOC_REG_ECC_SYNDROME3            = 0x1013,
			MDOC_REG_ECC_SYNDROME4            = 0x1014,
			MDOC_REG_ECC_SYNDROME5            = 0x1015,
			MDOC_REG_ALIAS_RESOLUTION         = 0x101b,
			MDOC_REG_IN_CONFIG                = 0x101c,
			MDOC_REG_READPIPE_INIT            = 0x101d,
			MDOC_REG_WRITEPIPE_TERM           = 0x101e,
			MDOC_REG_LASTDATA_READ            = 0x101f,
			MDOC_REG_NOP                      = 0x1020,
			MDOC_REG_CDSN_IO0                 = 0x1800,
			MDOC_REG_CDSN_IO1                 = 0x1801,
			MDOC_REG_MIL_CDSN_IO0             = 0x0800,
			MDOC_REG_MIL_CDSN_IO1             = 0x0801,
			MDOC_REG_MILPLUS_ACCESS_STAT      = 0x1008,
			MDOC_REG_MILPLUS_CONFIG           = 0x100a,
			MDOC_REG_MILPLUS_OUT_CNTL         = 0x100c,
			MDOC_REG_MILPLUS_FLASH_CNTL       = 0x1020,
			MDOC_REG_MILPLUS_FLASH_SELECT     = 0x1022,
			MDOC_REG_MILPLUS_FLASH_CMD        = 0x1024,
			MDOC_REG_MILPLUS_FLASH_ADDR       = 0x1026,
			MDOC_REG_MILPLUS_FLASH_DATA0      = 0x1028,
			MDOC_REG_MILPLUS_FLASH_DATA1      = 0x1029,
			MDOC_REG_MILPLUS_READPIPE_INIT    = 0x102a,
			MDOC_REG_MILPLUS_LASTDATA_READ0   = 0x102c,
			MDOC_REG_MILPLUS_LASTDATA_READ1   = 0x102d,
			MDOC_REG_MILPLUS_WRITEPIPE_TERM   = 0x102e,
			MDOC_REG_MILPLUS_ECC_SYNDROME0    = 0x1040,
			MDOC_REG_MILPLUS_ECC_SYNDROME1    = 0x1041,
			MDOC_REG_MILPLUS_ECC_SYNDROME2    = 0x1042,
			MDOC_REG_MILPLUS_ECC_SYNDROME3    = 0x1043,
			MDOC_REG_MILPLUS_ECC_SYNDROME4    = 0x1044,
			MDOC_REG_MILPLUS_ECC_SYNDROME5    = 0x1045,
			MDOC_REG_MILPLUS_ECC_CONFIG       = 0x1046,
			MDOC_REG_MILPLUS_TOGGLE           = 0x1046,
			MDOC_REG_MILPLUS_DOWNLOAD_STAT    = 0x1074,
			MDOC_REG_MILPLUS_CNTL_CONFIRM     = 0x1076,
			MDOC_REG_MILPLUS_POWER            = 0x1fff
		};

		enum mdoc_cdsn_ctrl_t : uint16_t
		{
			MDOC_CDSN_CTRL_MIL_READY = 0xc0, // bit 6+7: millenium ready bits (not busy)
			MDOC_CDSN_CTRL_READY     = 0x80, // bit 7: ready bit (not busy)
			MDOC_CDSN_CTRL_ECC_IO    = 0x20, // bit 5: do ECC IO
			MDOC_CDSN_CTRL_FLASH_IO  = 0x10, // bit 4: do flash IO
			MDOC_CDSN_CTRL_WP        = 0x08, // bit 3: write protect
			MDOC_CDSN_DATA_MODE      = 0x06, // bits 1 and 2
			MDOC_CDSN_CTRL_ALE       = 0x04, // bit 2: address latch enable
			MDOC_CDSN_CTRL_CLE       = 0x02, // bit 1: command latch enable
			MDOC_CDSN_CTRL_CE        = 0x01  // bit 0: chip enable
		};


		enum mdoc_ecc_conf_t : uint16_t
		{
			MDOC_ECC_ERROR       = 0x80,
			MDOC_ECC_ENABLE      = 0x08,
			MDOC_ECC_TOGGLE_BIT  = 0x04,
			MDOC_ECC_TOGGLE_EN   = 0x02,
			MDOC_ECC_IGNORE      = 0x01,
			MDOC_ECC_RESET       = 0x00,
			MDOC_ECC_SET_ENABLE  = 0x0a, // MDOC_ECC_TOGGLE_EN | MDOC_ECC_ENABLE
			MDOC_ECC_SET_DISABLE = 0x02  // MDOC_ECC_TOGGLE_EN
		};

		enum mdoc_nand_cmd_t : uint8_t
		{
			MDOC_NAND_CMD_READ0        = 0x00,
			MDOC_NAND_CMD_READ1        = 0x01,
			MDOC_NAND_CMD_RNDOUT       = 0x05,
			MDOC_NAND_CMD_PAGEPROG     = 0x10,
			MDOC_NAND_CMD_CACHEDPROG   = 0x15,
			MDOC_NAND_CMD_UNLOCK1      = 0x23,
			MDOC_NAND_CMD_UNLOCK2      = 0x24,
			MDOC_NAND_CMD_LOCK         = 0x2a,
			MDOC_NAND_CMD_READSTART    = 0x30,
			MDOC_NAND_CMD_READOOB      = 0x50, // Read out-of-band area
			MDOC_NAND_CMD_ERASE1       = 0x60,
			MDOC_NAND_CMD_STATUS       = 0x70,
			MDOC_NAND_CMD_SEQIN        = 0x80,
			MDOC_NAND_CMD_RNDIN        = 0x85,
			MDOC_NAND_CMD_READID       = 0x90,
			MDOC_NAND_CMD_ERASE2       = 0xd0,
			MDOC_NAND_CMD_RNDOUTSTART  = 0xe0,
			MDOC_NAND_CMD_PARAM        = 0xec,
			MDOC_NAND_CMD_GET_FEATURES = 0xee,
			MDOC_NAND_CMD_SET_FEATURES = 0xef,
			MDOC_NAND_CMD_RESET        = 0xff
		};

		enum mdoc_data_mode_t : uint8_t
		{
			MDOC_MODE_DATA    = 0x00,
			MDOC_MODE_ADDRESS = 0x20,
			MDOC_MODE_COMMAND = 0x40
		};

		enum mdoc_data_area_t : uint8_t
		{
			MDOC_DATA_NONE          = 0x00,
			MDOC_DATA_CHIPID        = 0x01,
			MDOC_DATA0_USER         = 0x02,
			MDOC_DATA1_USER         = 0x03,
			MDOC_DATA2_SPARE        = 0x04,
			MDOC_DATA_STATUS        = 0x05,
			MDOC_DATA_ERASE         = 0x06,
		};

		uint8_t  m_chip_id;

		std::unique_ptr<uint8_t[]> m_data_usr;
		std::unique_ptr<uint8_t[]> m_data_oob;
		uint16_t m_data_chipid;
		uint8_t  m_chip_flash_mfg_id;
		uint8_t  m_chip_flash_dev_id;
		uint16_t m_page_usr_size_bytes;
		uint16_t m_page_oob_size_bytes;
		uint32_t m_total_usr_size_bytes;
		uint32_t m_total_oob_size_bytes;
		uint32_t m_total_size_bytes;
		uint32_t m_addressable_pages;
		uint8_t  m_alias;

		uint32_t  m_io_out_index;
		uint32_t  m_io_in_index;
		uint32_t m_address;
		uint32_t m_command;
		uint8_t m_selected_data;
		bool m_seqin_enabled;
		uint8_t  m_cdsn_ctrl;
		uint8_t  m_flash_ctrl;
		uint8_t  m_ecc_config;
		uint8_t  m_ecc_stat;

		void reset_state();
		uint32_t read_selected_data(bool final_read);
		uint32_t get_data_offset();
		uint32_t read_chipid_data(bool final_read);
		uint32_t read_user_data(bool final_read);
		uint32_t read_spare_data(bool final_read);
		uint32_t read_ecc_config();
		uint8_t get_raw_cdsn_ctrl();
		bool is_enabled();
		bool is_write_enabled();
		uint8_t get_data_mode();
		bool is_flash_io_enabled();
		bool is_ecc_io_enabled();
		uint8_t get_data_area();
		void handle_cdsn_io_write(uint32_t data);
		void write_selected_data(uint32_t data);
		void write_user_data(uint8_t data);
		void write_spare_data(uint8_t data);
		void erase_selected_data();
		void set_cdsn_ctrl(uint32_t data);
		void set_command(uint8_t data);
		void set_address_byte(uint32_t data);
		void set_raw_cdsn_ctrl(uint8_t cdsn_ctrl);
		void set_enabled(bool value);
		void set_write_enabled(bool value);
		void set_flash_io_enabled(bool value);
		void set_flash_ecc_io_enabled(bool value);
		void set_other_cdsn_ctrl(uint8_t cdsn_ctrl);
		void set_data_mode(mdoc_data_mode_t data_mode);
		void set_data_area(uint8_t data_area);

	protected:

		virtual void device_start() override ATTR_COLD;
		virtual void device_reset() override ATTR_COLD;

		virtual void nvram_default() override;
		virtual bool nvram_read(util::read_stream &file) override;
		virtual bool nvram_write(util::write_stream &file) override;

};

/*
	M-systems DiskOnChip chips supported on WebTV's retail bootrom
*/

// M-systems DiskOnChip 2000 MD2200

// DiskOnChip MD2200-D02                        //    2MB
// DiskOnChip MD2200-D04                        //    4MB
// DiskOnChip MD2200-D08                        //    8MB
// DiskOnChip MD2200-D12                        //   12MB
// DiskOnChip MD2200-D16                        //   16MB
// DiskOnChip MD2200-D24                        //   24MB
// DiskOnChip MD2200-D48                        //   48MB

// M-systems DiskOnChip 2000 MD2202

// DiskOnChip MD2200-D16                        //   16MB
// DiskOnChip MD2200-D24                        //   24MB
// DiskOnChip MD2200-D32                        //   32MB
// DiskOnChip MD2200-D48                        //   48MB
// DiskOnChip MD2200-D64                        //   64MB
// DiskOnChip MD2200-D96                        //   96MB
// DiskOnChip MD2200-D128                       //  128MB
// DiskOnChip MD2200-D192                       //  192MB
// DiskOnChip MD2200-D384                       //  384MB

// M-systems DiskOnChip Millenium MD2800 (DIP), MD2810 (TSOP), MD2811 (Plus TSOP), MD3331/MD3831 (Plus BGA)

DECLARE_MDOC_CHIP(MD2810_D02,   mdoc_2810_0002) //    2MB (doesn't exist)
DECLARE_MDOC_CHIP(MD2810_D04,   mdoc_2810_0004) //    4MB (doesn't exist)
DECLARE_MDOC_CHIP(MD2810_D08,   mdoc_2810_0008) //    8MB
DECLARE_MDOC_CHIP(MD2810_D16,   mdoc_2810_0016) //   16MB (doesn't exist)
// DiskOnChip MD2811-D16                        //   32MB
// DiskOnChip MD2811-D32                        //   32MB
// DiskOnChip MD3331-D64                        //   64MB

// M-systems DiskOnChip 2000 MD2203

// DiskOnChip MD2203-D80                        //   80MB
// DiskOnChip MD2203-D112                       //  112MB
// DiskOnChip MD2203-D114                       //  114MB
// DiskOnChip MD2203-D160                       //  160MB
// DiskOnChip MD2203-D192                       //  192MB
// DiskOnChip MD2203-D224                       //  224MB
// DiskOnChip MD2203-D256                       //  256MB
// DiskOnChip MD2203-D288                       //  288MB
// DiskOnChip MD2203-D576                       //  575MB
// DiskOnChip MD2203-D768                       //  768MB
// DiskOnChip MD2203-D1024                      // 1024MB

/*
	M-systems DiskOnChip Collection

	Manages floors and chips

*/

class mdoc_collection : public device_t
{

	public:

		mdoc_collection(
			const machine_config &mconfig,
			const char *tag,
			device_t *owner,
			uint32_t clock = 0,
			std::function<void (mdoc_collection &, machine_config &)> setup_chips_func = nullptr
		);

		template <class CHIP_IMPL>
		void add_chip(machine_config &dconfig, uint8_t floor_index, uint8_t chip_index, CHIP_IMPL &CHIP)
		{
			floor_index &= (mdoc_collection::MAX_FLOORS - 1);
			chip_index  &= (mdoc_collection::MAX_CHIPS_PER_FLOOR - 1);

			CHIP(dconfig, m_chip[mdoc_collection::get_device_index(floor_index, chip_index)], 0);
		}

		uint32_t read32(offs_t offset);
		void write32(offs_t offset, uint32_t data);

	private:

		static constexpr uint32_t MAX_FLOORS          = 4;
		static constexpr uint32_t MAX_CHIPS_PER_FLOOR = 2;

		enum mdoc_reg_t : uint16_t
		{
			MDOC_REG_FLOOR_SELECT             = 0x1003,
			MDOC_REG_CDSN_DEVICE_SELECT       = 0x1005,
			MDOC_REG_MILPLUS_DEVICE_SELECT    = 0x1008
		};

		optional_device_array<mdoc_chip_base, (mdoc_collection::MAX_FLOORS * mdoc_collection::MAX_CHIPS_PER_FLOOR)> m_chip;

		uint8_t m_selected_floor;
		uint8_t m_selected_chip;

		std::function<void (mdoc_collection &, machine_config &)> m_setup_chips_func = nullptr;

		uint8_t get_device_index(uint8_t floor_index, uint8_t chip_index);

	protected:

		virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
		virtual void device_start() override ATTR_COLD;
		virtual void device_reset() override ATTR_COLD;

};
DECLARE_DEVICE_TYPE(MDOC_COLLECTION, mdoc_collection)

#endif // MAME_MACHINE_MDOC_H