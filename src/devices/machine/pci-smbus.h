// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
#ifndef MAME_MACHINE_PCI_SMBUS_H
#define MAME_MACHINE_PCI_SMBUS_H

#include "pci.h"

class smbus_device : public pci_device {
public:

	static constexpr uint8_t HST_STS_GOT_BBYTE = 1 << 7;
	static constexpr uint8_t HST_STS_INUSE     = 1 << 6;
	static constexpr uint8_t HST_STS_SMBALERT  = 1 << 5;
	static constexpr uint8_t HST_STS_FAILED    = 1 << 4;
	static constexpr uint8_t HST_STS_BUS_ERR   = 1 << 3;
	static constexpr uint8_t HST_STS_DEV_ERR   = 1 << 2;
	static constexpr uint8_t HST_STS_SUCCESS   = 1 << 1;
	static constexpr uint8_t HST_STS_BUSY      = 1 << 0;

	static constexpr uint8_t HST_AUX_STS_CRCER = 1 << 0;
	
	static constexpr uint8_t HST_CNT_PEC_EN    = 1 << 7;
	static constexpr uint8_t HST_CNT_START     = 1 << 6;
	static constexpr uint8_t HST_CNT_LAST      = 1 << 5;
	static constexpr uint8_t HST_CNT_SMB_CMD   = 7 << 2;
	static constexpr uint8_t HST_CNT_KILL      = 1 << 1;
	static constexpr uint8_t HST_CNT_INTEN     = 1 << 0;

	static constexpr uint8_t HST_AUX_CNT_E32B  = 1 << 1;
	static constexpr uint8_t HST_AUX_CNT_AAC   = 1 << 0;

	static constexpr uint8_t SMB_CMD_QUICK     = 0 << 2;
	static constexpr uint8_t SMB_CMD_SRBYTE    = 1 << 2;
	static constexpr uint8_t SMB_CMD_WRBYTE    = 2 << 2;
	static constexpr uint8_t SMB_CMD_WRWORD    = 3 << 2;
	static constexpr uint8_t SMB_CMD_CALL      = 4 << 2;
	static constexpr uint8_t SMB_CMD_WRBLOCK   = 5 << 2;
	static constexpr uint8_t SMB_CMD_I2CREAD   = 6 << 2;
	static constexpr uint8_t SMB_CMD_BLKCALL   = 7 << 2;

	smbus_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, uint32_t main_id, uint32_t revision, uint32_t subdevice_id)
		: smbus_device(mconfig, tag, owner, clock)
	{
		set_ids(main_id, revision, 0x0c0500, subdevice_id);
	}
	smbus_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	auto control_callback() { return m_hst_cnt_cb.bind(); }
	auto status_callback() { return m_hst_sts_cb.bind(); }
	auto block_byte_read_callback() { return m_hst_blkb_r_cb.bind(); }
	auto block_byte_write_callback() { return m_hst_blkb_w_cb.bind(); }

	void map(address_map &map) ATTR_COLD;

	uint8_t hst_sts_r();
	uint8_t hst_cnt_r();
	uint8_t hst_cmd_r();
	uint8_t xmit_slva_r();
	uint8_t hst_d0_r();
	uint8_t hst_d1_r();
	uint8_t host_block_db_r();
	uint8_t pec_r();
	uint8_t rcv_slva_r();
	uint16_t slv_data_r();
	uint8_t aux_sts_r();
	uint8_t aux_ctl_r();

	uint8_t get_slv_addr() { return ((xmit_slva >> 1) & 0x7f); }
	bool is_slv_read() { return (xmit_slva & 0x01); }
	void set_hst_sts(uint8_t data) { hst_sts = data; }
	void set_hst_d0(uint8_t data) { hst_d0 = data; }
	void set_hst_d1(uint8_t data) { hst_d1 = data; }
	uint8_t get_host_blkbyte() { return host_block_db; }
	void set_host_blkbyte(uint8_t data) { host_block_db = data; }
	void set_aux_sts(uint8_t data) { aux_sts = data; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:

	void hst_sts_w(uint8_t data);
	void hst_cnt_w(uint8_t data);
	void hst_cmd_w(uint8_t data);
	void xmit_slva_w(uint8_t data);
	void hst_d0_w(uint8_t data);
	void hst_d1_w(uint8_t data);
	void host_block_db_w(uint8_t data);
	void pec_w(uint8_t data);
	void rcv_slva_w(uint8_t data);
	void slv_data_w(uint16_t data);
	void aux_sts_w(uint8_t data);
	void aux_ctl_w(uint8_t data);
	uint8_t smlink_pin_ctl_r();
	void smlink_pin_ctl_w(uint8_t data);
	uint8_t smbus_pin_ctl_r();
	void smbus_pin_ctl_w(uint8_t data);
	uint8_t slv_sts_r();
	void slv_sts_w(uint8_t data);
	uint8_t slv_cmd_r();
	void slv_cmd_w(uint8_t data);
	uint8_t notify_daddr_r();
	uint8_t notify_dlow_r();
	uint8_t notify_dhigh_r();

	devcb_write_line m_hst_cnt_cb;
	devcb_write_line m_hst_sts_cb;
	devcb_write_line m_hst_aux_cnt_cb;
	devcb_write_line m_hst_aux_sts_cb;
	devcb_read8 m_hst_blkb_r_cb;
	devcb_write_line m_hst_blkb_w_cb;

	uint16_t slv_data;

	uint8_t hst_sts, hst_cnt, hst_cmd, xmit_slva, hst_d0, hst_d1;
	uint8_t host_block_db, pec, rcv_slva, aux_sts, aux_ctl;
	uint8_t smlink_pin_ctl, smbus_pin_ctl, slv_sts, slv_cmd, notify_daddr, notify_dlow, notify_dhigh;
};

DECLARE_DEVICE_TYPE(SMBUS, smbus_device)

#endif // MAME_MACHINE_PCI_SMBUS_H
