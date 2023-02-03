/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_fw.c
 *
 * LXS touch fw control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lxs_ts_dev.h"
#include "lxs_ts_reg.h"

static u32 lxs_ts_fw_act_buf_size(struct lxs_ts_data *ts)
{
	return (ts->buf_size - BUS_BUF_MARGIN) & (~0x3FF);
}

enum {
	EQ_COND = 0,
	NOT_COND,
};

int lxs_ts_condition_wait(struct lxs_ts_data *ts,
					u32 addr, u32 *value, u32 expect,
				    u32 mask, u32 delay, u32 retry, int cond)
{
	struct device *dev = &ts->client->dev;
	u32 addr1 = 0;
	u32 addr2 = 0;
	u32 data = 0;
	u32 data1 = 0;
	u32 data2 = 0;
	int dual = 0;
	int match = 0;
	int ret = 0;

	dual = !!(addr & BIT(31));

	addr1 = addr & 0xFFFF;
	addr2 = (dual) ? ((addr>>16) & 0x7FFF) : 0;

	do {
		lxs_ts_delay(delay);

		ret = lxs_ts_read_value(ts, addr1, &data1);
		if (dual)
			lxs_ts_read_value(ts, addr2, &data2);

		data = data1 | data2;
		if (ret >= 0) {
			match = (cond == NOT_COND) ?	\
				!!((data & mask) != expect) : !!((data & mask) == expect);

			if (match) {
				if (value)
					*value = data;

				if (ts->dbg_mask & DBG_LOG_FW_WR_TRACE) {
					input_info(true, dev,
						"wait done: addr[%04Xh] data[%08Xh], "
						"mask[%08Xh], expect[%s%08Xh]\n",
						addr, data, mask,
						(cond == NOT_COND) ? "not " : "",
						expect);
				}
				return 0;
			}
		}
	} while (--retry);

	if (value)
		*value = data;

	input_err(true, dev,
		"wait fail: addr[%04Xh] data[%08Xh], "
		"mask[%08Xh], expect[%s%08Xh]\n",
		addr, data, mask,
		(cond == NOT_COND) ? "not " : "",
		expect);

	return -EPERM;
}

static int lxs_ts_fw_rd_value(struct lxs_ts_data *ts, u32 addr, u32 *value, char *title)
{
	u32 data;
	int ret;

	if (title == NULL)
		title = "FW download";

	if (ts->dbg_mask & DBG_LOG_FW_WR_TRACE) {
		input_info(true, &ts->client->dev,
			"%s: read: addr 0x%04X", title, addr);
	}

	ret = lxs_ts_read_value(ts, addr, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: read failed(%d) - addr 0x%04X\n",
			title, ret, addr);
		return ret;
	}

	if (value)
		*value = data;

	return 0;
}

static int lxs_ts_fw_wr_value(struct lxs_ts_data *ts, u32 addr, u32 value)
{
	int ret;

	if (ts->dbg_mask & DBG_LOG_FW_WR_TRACE) {
		input_info(true, &ts->client->dev,
			"FW download: write: addr 0x%04X, value 0x%08X\n",
			addr, value);
	}

	ret = lxs_ts_write_value(ts, addr, value);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: write failed(%d) - addr 0x%04X, value 0x%X\n",
			__func__, ret, addr, value);
		return ret;
	}

	return 0;
}

static void __fw_wr_data_dbg(struct lxs_ts_data *ts, u8 *buf, int size)
{
	int prt_len = 0;
	int prt_idx = 0;
	int prd_sz = size;

	if (!(ts->dbg_mask & DBG_LOG_FW_WR_DBG))
		return;

	while (size) {
		prt_len = min(size, 16);

		input_info(true, &ts->client->dev,
			" 0x%04X buf[%3d~%3d] %*ph\n",
			(u32)prd_sz, prt_idx, prt_idx + prt_len - 1,
			prt_len, &buf[prt_idx]);

		size -= prt_len;
		prt_idx += prt_len;
	}
}

static int lxs_ts_fw_wr_code(struct lxs_ts_data *ts, u32 addr, u8 *dn_buf, int dn_size)
{
	int ret = 0;

	if (!dn_size)
		goto out;

	if (ts->dbg_mask & DBG_LOG_FW_WR_TRACE) {
		input_info(true, &ts->client->dev,
			"FW download: wcode: addr 0x%04X, size 0x%08X\n",
			addr, dn_size);
		__fw_wr_data_dbg(ts, dn_buf, dn_size);
	}

	ret = lxs_ts_write_value(ts, SERIAL_CODE_OFFSET, addr);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: wcode failed(%d) - addr 0x%04X, value 0x%X\n",
			__func__, ret, SERIAL_CODE_OFFSET, addr);
		goto out;
	}

	ret = lxs_ts_reg_write(ts, CODE_BASE_ADDR, (void *)dn_buf, dn_size);
	if (ret < 0) {
		int prt_len = min(dn_size, 16);
		input_info(true, &ts->client->dev,
			"%s: wcode failed(%d) - addr 0x%04X, buf %*ph %s\n",
			__func__, ret, CODE_BASE_ADDR, prt_len, dn_buf,
			(dn_size > prt_len) ? "..." : "");
		goto out;
	}

out:
	return ret;
}

static int lxs_ts_fw_sram_prot(struct lxs_ts_data *ts, bool prot)
{
	int ret = 0;

	if (prot) {
		/* protection enable */
		ret = lxs_ts_fw_wr_value(ts, SPR_SRAM_CTL, 0);
	} else {
		/* protection disable */
		ret = lxs_ts_fw_wr_value(ts, SPR_SRAM_CTL, BIT(0) | BIT(1));
	}

	lxs_ts_delay(10);

	return ret;
}

static int lxs_ts_fw_boot_on(struct lxs_ts_data *ts)
{
	int ret = 0;

	ret = lxs_ts_fw_wr_value(ts, SPR_BOOT_CTL, 1);

	lxs_ts_delay(5);

	return ret;
}

static int lxs_ts_fw_sys_rst_ctl(struct lxs_ts_data *ts, int index)
{
	u32 data = BIT(0);
	int ret;

	switch (index) {
	case 1:
		data = BIT(1) | BIT(2) | BIT(5);
		break;
	case 2:
		data = BIT(2) | BIT(5);
		break;
	}

	ret = lxs_ts_fw_wr_value(ts, SPR_RST_CTL, data);

	lxs_ts_delay(5);

	return ret;
}

static int lxs_ts_fw_sys_hold(struct lxs_ts_data *ts)
{
	int ret;

	ret = lxs_ts_fw_sys_rst_ctl(ts, 1);
	if (ret < 0)
		return ret;

	ret = lxs_ts_fw_sram_prot(ts, true);
	if (ret < 0)
		return ret;

	return 0;
}

static int lxs_ts_fw_sys_post(struct lxs_ts_data *ts)
{
	int ret;

	ret = lxs_ts_fw_sys_rst_ctl(ts, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int lxs_ts_fw_sys_rst(struct lxs_ts_data *ts, bool boot_on)
{
	int ret;

	ret = lxs_ts_fw_sys_rst_ctl(ts, 0);
	if (ret < 0)
		return ret;

	ret = lxs_ts_fw_sram_prot(ts, true);
	if (ret < 0)
		return ret;

	if (boot_on) {
		ret = lxs_ts_fw_boot_on(ts);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int lxs_ts_fw_run_crc(struct lxs_ts_data *ts)
{
	u32 gdma_sr_busy = 0x8;
	int ret = 0;

	ret = lxs_ts_fw_wr_value(ts, GDMA_SADDR, 0);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_wr_value(ts, GDMA_DADDR, 0);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_wr_value(ts, GDMA_CTRL, GDMA_CTRL_CON);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_wr_value(ts, GDMA_START, 1);
	if (ret < 0)
		goto out;

	lxs_ts_delay(10);

	ret = lxs_ts_condition_wait(ts, GDMA_SR, NULL,
			gdma_sr_busy, gdma_sr_busy, 5, 2000, NOT_COND);
	if (ret < 0)
		goto out;

out:
	lxs_ts_fw_wr_value(ts, GDMA_CLR, 1);

	return ret;
}

static int lxs_ts_fw_chk_crc(struct lxs_ts_data *ts, u32 *crc_result, char *title)
{
	u32 rdata_crc = 0;
	int err = 0;
	int ret = 0;

	/*
	 * Check CRC
	 */
	ret = lxs_ts_fw_rd_value(ts, SRAM_CRC_RESULT, &rdata_crc, title);
	if (ret < 0)
		return ret;

	err |= (rdata_crc != CRC_FIXED_VALUE);

#if 0
	ret = lxs_ts_fw_rd_value(ts, SRAM_CRC_PASS, &rdata_crc, title);
	if (ret < 0)
		return ret;

	err |= (!rdata_crc)<<1;
#endif

	if (crc_result)
		*crc_result = rdata_crc;

	return err;
}

static int lxs_ts_fw_sw_crc(struct lxs_ts_data *ts)
{
	u32 rdata_crc = 0;
	int ret = 0;

	ret = lxs_ts_fw_sys_hold(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_run_crc(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_chk_crc(ts, &rdata_crc, NULL);
	if (ret < 0)
		goto out;

	lxs_ts_fw_sys_rst(ts, 1);

	if (ret)
		input_info(true, &ts->client->dev, "%s: fail 0x%X\n", __func__, rdata_crc);
	else
		input_info(true, &ts->client->dev, "%s: done\n", __func__);

out:
	lxs_ts_delay(20);

	return ret;
}

static int lxs_ts_fw_boot_result(struct lxs_ts_data *ts, char *title)
{
	u32 rdata_crc = 0;
	u32 rdata_chk = 0;
	int err = 0;
	int ret = 0;

	/*
	 * Check CRC
	 */
	err = lxs_ts_fw_chk_crc(ts, &rdata_crc, title);
	if (err < 0)
		return err;

	/*
	 * Check Boot
	 */
	ret = lxs_ts_fw_rd_value(ts, SPR_CHK, &rdata_chk, title);
	if (ret < 0)
		return ret;

	err |= (!rdata_chk)<<2;

	if (err) {
		input_err(true, &ts->client->dev,
			"%s: fail %Xh(%Xh, %Xh)\n",
			title, err, rdata_crc, rdata_chk);
	}

	return err;
}

static int lxs_ts_fw_sram_write(struct lxs_ts_data *ts, u8 *dn_buf, int dn_size)
{
	u8 *fw_data = dn_buf;
	u32 fw_size = dn_size;
	u32 fw_addr = 0;
	u32 buf_size = lxs_ts_fw_act_buf_size(ts);
	u32 curr_size;
	int ret = 0;

	ret = lxs_ts_fw_sram_prot(ts, false);
	if (ret < 0)
		return ret;

	while (fw_size) {
		curr_size = min(fw_size, buf_size);

		ret = lxs_ts_fw_wr_code(ts, fw_addr>>2, fw_data, curr_size);
		if (ret < 0)
			break;

		fw_data += curr_size;
		fw_addr += curr_size;
		fw_size -= curr_size;
	}

	lxs_ts_fw_sram_prot(ts, true);

	return ret;
}

static int lxs_ts_fw_download(struct lxs_ts_data *ts, u8 *fw_buf, int fw_size)
{
	struct device *dev = &ts->client->dev;
	int fw_size_max = SIZEOF_FW;
	int ret = 0;

//	input_info(true, dev, "%s\n", "FW download: begins (%d)");

	input_info(true, dev, "FW download: fw size %d(0x%X)\n", fw_size, fw_size);

	atomic_set(&ts->init, TC_IC_INIT_NEED);
	atomic_set(&ts->fwsts, 0);

	lxs_ts_init_recon(ts);

	lxs_ts_init_ctrl(ts);

	ret = lxs_ts_fw_sys_hold(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_sram_write(ts, fw_buf, fw_size_max);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_run_crc(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_boot_result(ts, "FW download");
	if (ret < 0)
		goto out;

	ret = lxs_ts_fw_sys_rst(ts, 1);
	if (ret < 0)
		goto out;

	input_info(true, dev, "%s\n", "FW download: done");

	atomic_set(&ts->fwsts, 1);

out:
	if (ret < 0)
		lxs_ts_fw_sram_prot(ts, true);

	lxs_ts_delay(30);

	return ret;
}

static int lxs_ts_fw_check(struct lxs_ts_data *ts, u8 *fw_buf, int size)
{
	struct device *dev = &ts->client->dev;
	struct lxs_ts_fw_info *fw = &ts->fw;
	struct lxs_ts_version_bin *bin_ver = NULL;
	int fw_size_max = SIZEOF_FW;
	u32 bin_ver_offset = 0;
	u32 bin_cod_offset = 0;
	u32 bin_pid_offset = 0;
	char pid[12] = {0, };

	if (size != fw_size_max) {
		input_err(true, dev,
			"%s: invalid code_size(%Xh), must be = %Xh\n",
			__func__, size, SIZEOF_FW);
		return -EINVAL;
	}

	bin_ver_offset = *((u32 *)&fw_buf[BIN_VER_OFFSET_POS]);
	if (!bin_ver_offset) {
		input_err(true, dev, "%s: zero ver offset\n", __func__);
		return -EINVAL;
	}

	bin_cod_offset = *((u32 *)&fw_buf[BIN_COD_OFFSET_POS]);
	if (!bin_ver_offset) {
		input_err(true, dev, "%s: zero cod offset\n", __func__);
		return -EINVAL;
	}

	bin_pid_offset = *((u32 *)&fw_buf[BIN_PID_OFFSET_POS]);
	if (!bin_pid_offset) {
		input_err(true, dev, "%s: zero pid offset\n", __func__);
		return -EINVAL;
	}

	if (((bin_ver_offset + 4) > fw_size_max) ||
		((bin_cod_offset + 4) > fw_size_max) ||
		((bin_pid_offset + 8) > fw_size_max)) {
		input_err(true, dev,
			"%s: invalid offset - ver %06Xh, cod %06Xh, pid %06Xh, max %06Xh\n",
			__func__, bin_ver_offset, bin_cod_offset, bin_pid_offset, fw_size_max);
		return -EINVAL;
	}

	memcpy(&ts->fw.bin_dev_code, &fw_buf[bin_cod_offset], 4);

	memcpy(pid, &fw_buf[bin_pid_offset], 8);

	bin_ver = (struct lxs_ts_version_bin *)&fw_buf[bin_ver_offset];
	fw->bin_ver.module = bin_ver->module;
	fw->bin_ver.ver = bin_ver->ver;

	fw->ver_code_bin = ((ts->fw.bin_dev_code>>8) & 0xFF)<<24;
	fw->ver_code_bin |= (ts->fw.bin_dev_code & 0xFF)<<16;
	fw->ver_code_bin |= fw->bin_ver.module<<8;
	fw->ver_code_bin |= fw->bin_ver.ver;

	input_info(true, dev,
		"FW img chk: bin-ver: LX%08X (%s)\n",
		fw->ver_code_bin, pid);

	if (atomic_read(&ts->boot) != TC_IC_BOOT_FAIL) {
		if (ts->dbg_mask & DBG_LOG_FW_WR_TRACE)
			input_info(true, dev,
				"FW img chk: dev-ver: LX%08X (%s)\n",
				ts->fw.ver_code_dev, fw->product_id);

	#if 0
		if (memcmp(pid, fw->product_id, 8))
			input_err(true, dev,
				"%s: bin-pid[%s] != dev-pid[%s], halted (up %02X)\n",
				__func__, pid, fw->product_id, update);
	#endif
	}

	return 0;
}

int lxs_ts_request_fw(struct lxs_ts_data *ts, const char *fw_name)
{
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	int ret = 0;
#if defined(SUPPORT_SIGNED_FW)
	long spu_ret = 0, org_size = 0;
#endif

	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", fw_name);

	input_info(true, &ts->client->dev, "%s: Load firmware: %s\n", __func__, fw_path);

	ret = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d\n", __func__, ret);
		return -ENOENT;
	}

	if (fw_entry->size <= 0) {
		input_err(true, &ts->client->dev, "%s: fw size error %ld\n", __func__, fw_entry->size);
		ret = -ENOENT;
		goto done;
	}

#if defined(SUPPORT_SIGNED_FW)
	if (strncmp(fw_path, TSP_SPU_FW_SIGNED, strlen(TSP_SPU_FW_SIGNED)) == 0
			|| strncmp(fw_path, TSP_EXTERNAL_FW_SIGNED, strlen(TSP_EXTERNAL_FW_SIGNED)) == 0) {
		/* name 3, digest 32, signature 512 */
		org_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		input_info(true, &ts->client->dev, "%s: spu_ret : %ld, spu_fw_size:%ld\n", __func__, spu_ret, fw_entry->size);

		if (spu_ret != org_size) {
			input_err(true, &ts->client->dev, "%s: signature verify failed, %ld\n", __func__, spu_ret);
			ret = -EIO;
			goto done;
		}
	} else {
		org_size = fw_entry->size;
	}
#endif

#if defined(SUPPORT_SIGNED_FW)
	ts->fw_size = org_size;
#else
	ts->fw_size = fw_entry->size;
#endif
	memcpy(ts->fw_data, fw_entry->data,  ts->fw_size);
done:
	release_firmware(fw_entry);
	return ret;
}

static int lxs_ts_update_fw(struct lxs_ts_data *ts, int type)
{
	int error = 0;
	int retry = 0;

	error = lxs_ts_fw_check(ts, ts->fw_data, ts->fw_size);
	if (error < 0)
		goto err_request_fw;

	/* use virtual tclm_control - magic cal 1 */
	while (1) {
		error = lxs_ts_fw_download(ts, ts->fw_data, ts->fw_size);
		if (error >= 0) {
			break;
		}

		if (retry++ > 3) {
			input_err(true, &ts->client->dev, "%s: Fail Firmware update\n",
					__func__);
			ts->fw_type = 0;
			return error;
		}
	}

	if (type)
		ts->fw_type = type;

err_request_fw:

	return error;
}

int lxs_ts_load_fw_from_ts_bin(struct lxs_ts_data *ts)
{
	int ret = 0;

	lxs_ts_irq_enable(ts, false);
	ret = lxs_ts_request_fw(ts, ts->plat_data->firmware_name_ts);
	if (ret < 0) {
		lxs_ts_irq_enable(ts, true);
		return ret;
	}

	ret = lxs_ts_update_fw(ts, FW_TYPE_TS);
	if (ret < 0) {
		lxs_ts_irq_enable(ts, true);
		return ret;
	}

	return ret;
}

int lxs_ts_load_fw_from_mp_bin(struct lxs_ts_data *ts)
{
#if defined(USE_FW_MP)
	if (!ts->plat_data->firmware_name_mp || !strlen(ts->plat_data->firmware_name_mp)) {
		input_info(true, &ts->client->dev, "%s: empty fw_path.\n", __func__);
		return -EEXIST;
	}

	lxs_ts_irq_enable(ts, false);

	return lxs_ts_update_fw(ts, ts->plat_data->firmware_name_mp, FW_TYPE_MP);
#else
	return 0;
#endif
}

static int lxs_ts_do_load_fw_from_external(struct lxs_ts_data *ts, const char *file_path)
{
	int ret = 0;

	lxs_ts_irq_enable(ts, false);
	ret = lxs_ts_request_fw(ts, file_path);
	if (ret < 0) {
		return ret;
	}

	ret = lxs_ts_fw_check(ts, ts->fw_data, ts->fw_size);
	if (ret < 0)
		goto done;

	ret = lxs_ts_fw_download(ts,  ts->fw_data, ts->fw_size);
	if (ret < 0)
		goto done;

done:
	lxs_ts_irq_enable(ts, true);

	return ret;
}

int lxs_ts_load_fw_from_external(struct lxs_ts_data *ts, const char *file_path)
{
	int ret = 0;

	if (!file_path || !strlen(file_path))
		return -EINVAL;

	lxs_ts_irq_enable(ts, false);

	mutex_lock(&ts->device_mutex);

	atomic_set(&ts->fwup, 1);

	ret = lxs_ts_do_load_fw_from_external(ts, file_path);

	atomic_set(&ts->fwup, 0);

	mutex_unlock(&ts->device_mutex);

	return ret;
}

int lxs_ts_reload_fw(struct lxs_ts_data *ts)
{
	int fwsts = atomic_read(&ts->fwsts);
	int err = 0;

	if (fwsts) {
		err = lxs_ts_fw_sw_crc(ts);
		if (err < 0)
			return err;

		if (!err)
			return 0;

	//	input_info(true, &ts->client->dev, "%s: reload, %d\n", __func__, err);
	}

	return lxs_ts_update_fw(ts, FW_TYPE_TS);
}

void lxs_ts_sw_reset(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	lxs_ts_fw_sys_rst(ts, 1);
}

int lxs_ts_boot_result(struct lxs_ts_data *ts)
{
	u32 rdata_chk = 0;
	int err = 0;
	int ret = 0;

	ret = lxs_ts_fw_rd_value(ts, SPR_CHK, &rdata_chk, "boot result");
	if (ret < 0)
		return ret;

	err |= (!rdata_chk)<<2;

	return err;
}

static int lxs_ts_fw_rd_code(struct lxs_ts_data *ts, u32 addr, u8 *rd_buf, int rd_size)
{
	int ret = 0;

	if (!rd_size)
		goto out;

	ret = lxs_ts_write_value(ts, SERIAL_CODE_OFFSET, addr);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"SRAM dump: rdata failed(%d) - addr 0x%04X, value 0x%X\n",
			ret, SERIAL_CODE_OFFSET, addr);
		goto out;
	}

	ret = lxs_ts_reg_read(ts, CODE_BASE_ADDR, (void *)rd_buf, rd_size);
	if (ret < 0) {
		int prt_len = min(rd_size, 16);
		input_info(true, &ts->client->dev,
			"SRAM dump: rdata failed(%d) - addr 0x%04X, buf %*ph %s\n",
			ret, CODE_BASE_ADDR, prt_len, rd_buf,
			(rd_size > prt_len) ? "..." : "");
		goto out;
	}

out:
	return ret;
}

static int lxs_ts_sram_dump(struct lxs_ts_data *ts, u8 *rd_buf, int rd_size)
{
	u8 *sram_data = rd_buf;
	u32 sram_size = rd_size;
	u32 sram_addr = 0;
	u32 buf_size = lxs_ts_fw_act_buf_size(ts);
	u32 curr_size;
	int ret = 0;

	while (sram_size) {
		curr_size = min(sram_size, buf_size);

		ret = lxs_ts_fw_rd_code(ts, sram_addr>>2, sram_data, curr_size);
		if (ret < 0)
			break;

		sram_data += curr_size;
		sram_addr += curr_size;
		sram_size -= curr_size;
	}

	lxs_ts_delay(10);

	return ret;
}

static int lxs_ts_sram_test_core(struct lxs_ts_data *ts, int test_cnt)
{
	u8 *wr_buf;
	u8 *rd_buf;
	u32 pattern_and_crc[][2] = {
		{ 0x55555555, 0x0FEE0FEE },
		{ 0xAAAAAAAA, 0x1FDC1FDC },
		{ 0xFFFFFFFF, 0x10321032 },
		{ 0x00000000, 0x00000000 },
	};
	u32 curr_pattern;
	u32 curr_crc;
	u32 read_crc;
	u32 wr_data, rd_data;
	int i, j;
	int count;
	int err = 0;
	int ret = 0;

	if (test_cnt < 1)
		test_cnt = 1;

//	input_info(true, &ts->client->dev, "SRAM test: begins\n");

	wr_buf = kzalloc(SIZEOF_FW<<1, GFP_KERNEL);
	if (wr_buf == NULL) {
		input_info(true, &ts->client->dev, "SRAM test: failed to alloacte memory\n");
		return -ENOMEM;
	}

	rd_buf = wr_buf + SIZEOF_FW;

	for (count = 0; count < test_cnt; count++) {
		for (i = 0; i < ARRAY_SIZE(pattern_and_crc); i++) {
			curr_pattern = pattern_and_crc[i][0];
			curr_crc = pattern_and_crc[i][1];

			memset(wr_buf, curr_pattern, SIZEOF_FW);
			memset(rd_buf, ~curr_pattern, SIZEOF_FW);

			input_info(true, &ts->client->dev, "SRAM test: (%d/%d) [%d] pattern 0x%08X\n",
				count + 1, test_cnt, i, curr_pattern);

			ret = lxs_ts_fw_sram_write(ts, wr_buf, SIZEOF_FW);
			if (ret < 0)
				goto out;

			ret = lxs_ts_fw_run_crc(ts);
			if (ret < 0)
				goto out;

			ret = lxs_ts_fw_chk_crc(ts, &read_crc, NULL);
			if (ret < 0)
				goto out;

			if (curr_crc == read_crc)
				continue;

			err |= BIT(i);

			input_err(true, &ts->client->dev,
				"SRAM test: test fail: [%d] pattern 0x%08X : curr_crc[0x%08X] != read_crc[0x%08X]\n",
				i, curr_pattern, curr_crc, read_crc);

			ret = lxs_ts_sram_dump(ts, rd_buf, SIZEOF_FW);
			if (ret < 0)
				goto out;

			for (j = 0; j < (SIZEOF_FW>>2); j++) {
				wr_data = ((u32 *)wr_buf)[j];
				rd_data = ((u32 *)rd_buf)[j];
				if (wr_data != rd_data) {
					input_err(true, &ts->client->dev,
						"SRAM test: data dump: [%d] pattern 0x%08X : buf[0x%X~0x%X]: wr %*ph vs. rd %*ph\n",
						i, curr_pattern, (j<<2), (j<<2) + 3, 4, &wr_buf[j<<2], 4, &rd_buf[j<<2]);
				}
			}
		}

		if (err)
			break;
	}

	if (err)
		ret = -EFAULT;

	input_info(true, &ts->client->dev, "SRAM test: %s\n",
		(ret < 0) ? "failed" : "success");

out:
	kfree(wr_buf);

	return ret;
}

int lxs_ts_sram_test(struct lxs_ts_data *ts, int cnt)
{
	int ret = 0;

	lxs_ts_irq_enable(ts, false);

	atomic_set(&ts->init, TC_IC_INIT_NEED);
	atomic_set(&ts->fwsts, 0);

	ret = lxs_ts_fw_sys_hold(ts);
	if (ret < 0)
		return ret;

	ret = lxs_ts_sram_test_core(ts, cnt);
	if (ret < 0)
		return ret;

	ret = lxs_ts_fw_sys_post(ts);
	if (ret < 0)
		return ret;

	ret = lxs_ts_fw_sys_rst(ts, 0);
	if (ret < 0)
		return ret;

	return 0;
}


MODULE_LICENSE("GPL");

