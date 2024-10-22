//SPDX-License-Identifier: GPL-2.0
/*
 * drivers/input/sec_input/stm_fst1ba90a_i2c/stm_fw.h
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "stm_ts.h"

#define STM_TS_FILE_SIGNATURE 0xAA55AA55

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	SPU,
};

/**
 * @brief  Type definitions - header structure of firmware file (ftb)
 */

struct stm_ts_header {
	u32	signature;
	u32	ftb_ver;
	u32	target;
	u32	fw_id;
	u32	fw_ver;
	u32	cfg_id;
	u32	cfg_ver;
	u8	fw_area_bs;	// bs : block size
	u8	panel_area_bs;
	u8	cx_area_bs;
	u8	cfg_area_bs;
	u32	reserved1;
	u32	ext_release_ver;
	u8	project_id;
	u8	ic_name;
	u8	module_ver;
	u8	reserved2;
	u32	sec0_size;
	u32	sec1_size;
	u32	sec2_size;
	u32	sec3_size;
	u32	hdr_crc;
} __packed;

#define FW_HEADER_SIZE  64

#define WRITE_CHUNK_SIZE 1024
#define DRAM_SIZE (64 * 1024) // 64kB

#define CODE_ADDR_START 0x00000000
#define CX_ADDR_START 0x00007000
#define CONFIG_ADDR_START 0x00007C00

#define	SIGNEDKEY_SIZE		(256)

int stm_ts_check_dma_startanddone(struct stm_ts_data *ts)
{
	int timeout = 60;
	u8 regAdd[6] = { 0xFA, 0x20, 0x00, 0x00, 0x71, 0xC0 };
	u8 val[1];
	int ret;

	ret = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write\n", __func__);
		return ret;
	}

	sec_delay(10);

	do {
		ret = ts->stm_ts_read(ts, &regAdd[0], 5, (u8 *)val, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
			return ret;
		}

		if ((val[0] & 0x80) != 0x80)
			break;

		sec_delay(50);
		timeout--;
	} while (timeout != 0);

	if (timeout == 0) {
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
		return -EIO;
	}

	return 0;
}

static int stm_ts_check_erase_done(struct stm_ts_data *ts)
{
	int timeout = 60;  // 3 sec timeout
	u8 regAdd[5] = { 0xFA, 0x20, 0x00, 0x00, 0x6A };
	u8 val[1];
	int ret;

	do {
		ret = ts->stm_ts_read(ts, &regAdd[0], 5, (u8 *)val, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
			return ret;
		}

		if ((val[0] & 0x80) != 0x80)
			break;

		sec_delay(50);
		timeout--;
	} while (timeout != 0);

	if (timeout == 0) {
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
		return -EIO;
	}

	return 0;
}

int stm_ts_fw_fillflash(struct stm_ts_data *ts, u32 address, u8 *data, int size)
{
	int remaining, index = 0;
	int toWrite = 0;
	int byteBlock = 0;
	int wheel = 0;
	u32 addr = 0;
	int rc;
	int delta;

	u8 buff[WRITE_CHUNK_SIZE + 5] = {0};
	u8 buff2[12] = {0};

	remaining = size;
	while (remaining > 0) {
		byteBlock = 0;
		addr = 0x00100000;

		while ((byteBlock < DRAM_SIZE) && remaining > 0) {
			if (remaining >= WRITE_CHUNK_SIZE) {
				if ((byteBlock + WRITE_CHUNK_SIZE) <= DRAM_SIZE) {
					toWrite = WRITE_CHUNK_SIZE;
					remaining -= WRITE_CHUNK_SIZE;
					byteBlock += WRITE_CHUNK_SIZE;
				} else {
					delta = DRAM_SIZE - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			} else {
				if ((byteBlock + remaining) <= DRAM_SIZE) {
					toWrite = remaining;
					byteBlock += remaining;
					remaining = 0;
				} else {
					delta = DRAM_SIZE - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			}

			index = 0;
			buff[index++] = 0xFA;
			buff[index++] = (u8) ((addr & 0xFF000000) >> 24);
			buff[index++] = (u8) ((addr & 0x00FF0000) >> 16);
			buff[index++] = (u8) ((addr & 0x0000FF00) >> 8);
			buff[index++] = (u8) ((addr & 0x000000FF));

			memcpy(&buff[index], data, toWrite);

			rc = ts->stm_ts_write(ts, &buff[0], index + toWrite);
			if (rc < 0) {
				input_err(true, &ts->client->dev,
						"%s failed to write i2c register. ret:%d\n",
						__func__, rc);
				return rc;
			}
			sec_delay(5);

			addr += toWrite;
			data += toWrite;
		}

		input_info(true, &ts->client->dev, "%s: Write %d Bytes\n", __func__, byteBlock);

		//configuring the DMA
		byteBlock = byteBlock / 4 - 1;

		index = 0;
		buff2[index++] = 0xFA;
		buff2[index++] = 0x20;
		buff2[index++] = 0x00;
		buff2[index++] = 0x00;
		buff2[index++] = 0x72;
		buff2[index++] = 0x00;
		buff2[index++] = 0x00;

		addr = address + ((wheel * DRAM_SIZE) / 4);
		buff2[index++] = (u8) ((addr & 0x000000FF));
		buff2[index++] = (u8) ((addr & 0x0000FF00) >> 8);
		buff2[index++] = (u8) (byteBlock & 0x000000FF);
		buff2[index++] = (u8) ((byteBlock & 0x0000FF00) >> 8);
		buff2[index++] = 0x00;

		rc = ts->stm_ts_write(ts, &buff2[0], index);
		if (rc < 0) {
			input_err(true, &ts->client->dev,
					"%s failed to write i2c register. ret:%d\n",
					__func__, rc);
			return rc;
		}
		sec_delay(10);

		rc = stm_ts_check_dma_startanddone(ts);
		if (rc < 0)
			return rc;

		wheel++;
	}

	return 0;
}

static int stm_ts_fw_burn(struct stm_ts_data *ts, u8 *fw_data)
{
	const struct stm_ts_header *fw_header;
	u8 *pFWData;
	int rc;
	int i;
	u8 regAdd[STM_TS_EVENT_SIZE] = {0};
	int NumberOfMainBlock = 0;

	fw_header = (struct stm_ts_header *) &fw_data[0];

	if ((fw_header->fw_area_bs) > 0)
		NumberOfMainBlock = fw_header->fw_area_bs;
	else
		NumberOfMainBlock = 26; // original value

	input_info(true, &ts->client->dev, "%s: Number Of MainBlock: %d\n",
			__func__, NumberOfMainBlock);

	// System Reset and Hold
	regAdd[0] = 0xFA;	regAdd[1] = 0x20;	regAdd[2] = 0x00;
	regAdd[3] = 0x00;	regAdd[4] = 0x24;	regAdd[5] = 0x01;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// Change application mode
	regAdd[0] = 0xFA;	regAdd[1] = 0x20;	regAdd[2] = 0x00;
	regAdd[3] = 0x00;	regAdd[4] = 0x25;	regAdd[5] = 0x20;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// Unlock Flash
	regAdd[0] = 0xFA;	regAdd[1] = 0x20;	regAdd[2] = 0x00;
	regAdd[3] = 0x00;	regAdd[4] = 0xDE;	regAdd[5] = 0x03;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;
	sec_delay(200);

	//==================== Erase Partial Flash ====================
	input_info(true, &ts->client->dev, "%s: Start Flash(Main Block) Erasing\n", __func__);
	for (i = 0; i < NumberOfMainBlock; i++) {
		regAdd[0] = 0xFA; regAdd[1] = 0x20; regAdd[2] = 0x00;
		regAdd[3] = 0x00; regAdd[4] = 0x6B; regAdd[5] = 0x00;
		rc = ts->stm_ts_write(ts, &regAdd[0], 6);
		if (rc < 0)
			return rc;
		sec_delay(50);

		regAdd[0] = 0xFA; regAdd[1] = 0x20; regAdd[2] = 0x00;
		regAdd[3] = 0x00; regAdd[4] = 0x6A;
		regAdd[5] = (0x80 + i) & 0xFF;
		rc = ts->stm_ts_write(ts, &regAdd[0], 6);
		if (rc < 0)
			return rc;
		rc = stm_ts_check_erase_done(ts);
		if (rc < 0)
			return rc;
	}

	input_info(true, &ts->client->dev, "%s: Start Flash(Config Block) Erasing\n", __func__);
	regAdd[0] = 0xFA; regAdd[1] = 0x20; regAdd[2] = 0x00;
	regAdd[3] = 0x00; regAdd[4] = 0x6B; regAdd[5] = 0x00;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;
	sec_delay(50);

	regAdd[0] = 0xFA; regAdd[1] = 0x20; regAdd[2] = 0x00;
	regAdd[3] = 0x00; regAdd[4] = 0x6A;
	regAdd[5] = (0x80 + 31) & 0xFF;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;

	rc = stm_ts_check_erase_done(ts);
	if (rc < 0)
		return rc;

	// Code Area
	if (fw_header->sec0_size > 0) {
		pFWData = (u8 *) &fw_data[FW_HEADER_SIZE];

		input_info(true, &ts->client->dev, "%s: Start Flashing for Code\n", __func__);
		rc = stm_ts_fw_fillflash(ts, CODE_ADDR_START, &pFWData[0], fw_header->sec0_size);
		if (rc < 0)
			return rc;

		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for Code\n",
				__func__, fw_header->sec0_size);
	}

	// Config Area
	if (fw_header->sec1_size > 0) {
		input_info(true, &ts->client->dev, "%s: Start Flashing for Config\n", __func__);
		pFWData = (u8 *) &fw_data[FW_HEADER_SIZE + fw_header->sec0_size];
		rc = stm_ts_fw_fillflash(ts, CONFIG_ADDR_START, &pFWData[0], fw_header->sec1_size);
		if (rc < 0)
			return rc;
		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for Config\n",
				__func__, fw_header->sec1_size);
	}

	// CX Area
	if (fw_header->sec2_size > 0) {
		input_info(true, &ts->client->dev, "%s: Start Flashing for CX\n", __func__);
		pFWData = (u8 *) &fw_data[FW_HEADER_SIZE + fw_header->sec0_size + fw_header->sec1_size];
		rc = stm_ts_fw_fillflash(ts, CX_ADDR_START, &pFWData[0], fw_header->sec2_size);
		if (rc < 0)
			return rc;
		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for CX\n",
				__func__, fw_header->sec2_size);
	}

	regAdd[0] = 0xFA;	regAdd[1] = 0x20;	regAdd[2] = 0x00;
	regAdd[3] = 0x00;	regAdd[4] = 0x24;	regAdd[5] = 0x80;
	rc = ts->stm_ts_write(ts, &regAdd[0], 6);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// System Reset
	ts->stm_ts_systemreset(ts, 0);

	return 0;
}

int stm_ts_wait_for_echo_event(struct stm_ts_data *ts, u8 *cmd, u8 cmd_cnt, int delay)
{
	int rc;
	int i;
	bool matched = false;
	u8 regAdd = STM_TS_READ_ONE_EVENT;
	u8 data[STM_TS_EVENT_SIZE];
	int retry = 0;

	mutex_lock(&ts->wait_for);

	rc = ts->stm_ts_write(ts, cmd, cmd_cnt);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write command\n", __func__);
		mutex_unlock(&ts->wait_for);
		return rc;
	}

	if (delay)
		sec_delay(delay);

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	rc = -EIO;

	while (ts->stm_ts_read(ts, &regAdd, 1, (u8 *)data, STM_TS_EVENT_SIZE) > 0) {
		if (data[0] != 0x00)
			input_info(true, &ts->client->dev,
					"%s: event %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X,"
					" %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7],
					data[8], data[9], data[10], data[11],
					data[12], data[13], data[14], data[15]);

		if ((data[0] == STM_TS_EVENT_STATUS_REPORT) && (data[1] == 0x01)) {  // Check command ECHO
			int loop_cnt;

			if (cmd_cnt > 4)
				loop_cnt = 4;
			else
				loop_cnt = cmd_cnt;

			for (i = 0; i < loop_cnt; i++) {
				if (data[i + 2] != cmd[i]) {
					matched = false;
					break;
				}
				matched = true;
			}

			if (matched == true) {
				rc = 0;
				break;
			}
		} else if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			input_info(true, &ts->client->dev, "%s: Error detected %02X,%02X,%02X,%02X,%02X,%02X\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);

			if (retry >= STM_TS_RETRY_COUNT)
				break;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 25) {
			input_err(true, &ts->client->dev, "%s: Time Over (%02X,%02X,%02X,%02X,%02X,%02X)\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);
			break;
		}
		sec_delay(20);
	}

	mutex_unlock(&ts->wait_for);

	return rc;
}

#ifdef TCLM_CONCEPT
int stm_ts_tclm_execute_force_calibration(struct device *dev, int cal_mode)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)dev_get_drvdata(dev);

	return stm_ts_execute_autotune(ts, true);
}
#endif

int stm_ts_execute_autotune(struct stm_ts_data *ts, bool IsSaving)
{
	u8 regAdd[STM_TS_EVENT_SIZE] = {0,};
	u8 DataType = 0x00;
	int rc;

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	rc = ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	if (rc < 0) {
		ts->factory_position = OFFSET_FAC_NOSAVE;
		return rc;
	}

	sec_input_irq_disable(ts->plat_data);

	// w A4 00 03
	if (IsSaving == true) {
		// full panel init
		regAdd[0] = 0xA4; regAdd[1] = 0x00; regAdd[2] = 0x03;

		rc = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 500);
#ifdef TCLM_CONCEPT
		if (ts->tdata->nvdata.cal_fail_cnt == 0xFF)
			ts->tdata->nvdata.cal_fail_cnt = 0;
		if (rc < 0) {
			ts->tdata->nvdata.cal_fail_cnt++;
			ts->tdata->nvdata.cal_fail_falg = 0;
		} else {
			ts->tdata->nvdata.cal_fail_falg = SEC_CAL_PASS;
			ts->is_cal_done = true;
		}
		ts->tdata->tclm_write(ts->tdata->dev, SEC_TCLM_NVM_ALL_DATA);
#endif
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	} else {
		// SS ATune
		//DataType = 0x0C;
		DataType = 0x3F;

		regAdd[0] = 0xA4; regAdd[1] = 0x03; regAdd[2] = (u8)DataType; regAdd[3] = 0x00;

		rc = stm_ts_wait_for_echo_event(ts, &regAdd[0], 4, 500);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	}

ERROR:
	ts->factory_position = OFFSET_FAC_NOSAVE;
	sec_input_irq_enable(ts->plat_data);

	return rc;
}

static const int stm_ts_fw_updater(struct stm_ts_data *ts, u8 *fw_data)
{
	const struct stm_ts_header *header;
	int retval;
	int retry;
	u8 fw_main_version;

	if (!fw_data) {
		input_err(true, &ts->client->dev, "%s: Firmware data is NULL\n",
				__func__);
		return -ENODEV;
	}

	atomic_set(&ts->fw_update_is_running, 1);

	header = (struct stm_ts_header *)fw_data;
	fw_main_version = header->ext_release_ver & 0xFF;

	input_info(true, &ts->client->dev,
			"%s: Starting firmware update : 0x%02X\n", __func__,
			fw_main_version);

	retry = 0;

	sec_input_irq_disable(ts->plat_data);
	while (1) {
		retval = stm_ts_fw_burn(ts, fw_data);
		if (retval >= 0) {
			ts->stm_ts_get_version_info(ts);

			if (fw_main_version == ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER]) {
				input_info(true, &ts->client->dev,
						"%s: Success Firmware update\n",
						__func__);
				ts->fw_corruption = false;

				retval = ts->stm_ts_systemreset(ts, 0);

				if (ts->osc_trim_error) {
					retval = stm_ts_osc_trim_recovery(ts);
					if (retval < 0)
						input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
					else
						ts->fw_corruption = false;
				}

				stm_ts_set_scanmode(ts, ts->scan_mode);
				retval = 0;
				break;
			}
		}

		if (retry++ > 3) {
			input_err(true, &ts->client->dev, "%s: Fail Firmware update\n",
					__func__);
			retval = -EIO;
			break;
		}
	}
	sec_input_irq_enable(ts->plat_data);

	atomic_set(&ts->fw_update_is_running, 0);

	return retval;
}

int stm_ts_fw_update_on_probe(struct stm_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	u8 *fw_data = NULL;
	char fw_path[STM_TS_MAX_FW_PATH];
	const struct stm_ts_header *header;
	u8 *ver_bin, *ver_ic;
#ifdef TCLM_CONCEPT
	int ret = 0;
	bool restore_cal = false;

	if (ts->tdata->support_tclm_test) {
		ret = sec_tclm_test_on_probe(ts->tdata);
		if (ret < 0)
			input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);
	}
#endif

	if (ts->plat_data->bringup == 1)
		return STM_TS_NOT_ERROR;

	if (!ts->plat_data->firmware_name) {
		input_err(true, &ts->client->dev, "%s: firmware name does not declair in dts\n", __func__);
		retval = -ENOENT;
		goto exit_fwload;
	}

	snprintf(fw_path, STM_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	input_info(true, &ts->client->dev, "%s: Load firmware : %s\n", __func__, fw_path);

	retval = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (retval) {
		input_err(true, &ts->client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		retval = -ENOENT;
		goto done;
	}

	fw_data = (u8 *)fw_entry->data;
	header = (struct stm_ts_header *)fw_data;

	snprintf(ts->plat_data->ic_vendor_name, sizeof(ts->plat_data->ic_vendor_name), "ST");
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_IC_VER] = header->ic_name;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER_PROJECT_ID] = header->project_id;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_MODULE_VER] = header->module_ver;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER] = header->ext_release_ver & 0xFF;
	ver_bin = ts->plat_data->img_version_of_bin;
	ver_ic = ts->plat_data->img_version_of_ic;

	input_info(true, &ts->client->dev,
			"%s: [BIN:STM] Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X\n", __func__,
			(u16)header->fw_ver, (u16)header->cfg_ver, (u16)header->ext_release_ver);
	input_info(true, &ts->client->dev,
			"%s: [BIN:SEC] IC Ver: 0x%02X, Project ID: 0x%02X, Module Ver: 0x%02X, FW Ver: 0x%02X\n",
			__func__, ver_bin[SEC_INPUT_FW_IC_VER],
			ver_bin[SEC_INPUT_FW_VER_PROJECT_ID],
			ver_bin[SEC_INPUT_FW_MODULE_VER],
			ver_bin[SEC_INPUT_FW_VER]);

	if (ts->plat_data->bringup == 2) {
		input_err(true, &ts->client->dev, "%s: skip fw_update for bringup\n", __func__);
		retval = STM_TS_NOT_ERROR;
		goto done;
	}

	if (ts->plat_data->hw_param.checksum_result)
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ver_ic[SEC_INPUT_FW_IC_VER] != ver_bin[SEC_INPUT_FW_IC_VER])
		retval = STM_TS_NOT_UPDATE;
	else if (ver_ic[SEC_INPUT_FW_VER_PROJECT_ID] != ver_bin[SEC_INPUT_FW_VER_PROJECT_ID])
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ver_ic[SEC_INPUT_FW_MODULE_VER] != ver_bin[SEC_INPUT_FW_MODULE_VER])
		retval = STM_TS_NOT_UPDATE;
	else if (ver_ic[SEC_INPUT_FW_VER] < ver_bin[SEC_INPUT_FW_VER])
		retval = STM_TS_NEED_FW_UPDATE;
	else
		retval = STM_TS_NOT_ERROR;

	if ((ts->plat_data->bringup == 3) &&
			(ver_ic[SEC_INPUT_FW_VER] != ver_bin[SEC_INPUT_FW_VER])) {
		input_info(true, &ts->client->dev,
				"%s: bringup 3, force update because version is different\n", __func__);
		retval = STM_TS_NEED_FW_UPDATE;
	}

#if 0
	/* ic fw ver > bin fw ver && force is false */
	if (retval != STM_TS_NEED_FW_UPDATE) {
		input_err(true, &ts->client->dev, "%s: skip fw update\n", __func__);

		goto done;
	}
#endif
	retval = stm_ts_fw_updater(ts, fw_data);
	if (retval < 0)
		goto done;

	stm_ts_execute_autotune(ts, true);

#ifdef TCLM_CONCEPT
	ret = ts->tdata->tclm_read(ts->tdata->dev, SEC_TCLM_NVM_ALL_DATA);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);
		goto done;
	}

	input_info(true, &ts->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
		__func__, ts->tdata->nvdata.tune_fix_ver, ts->tdata->afe_base);

	if ((ts->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
		((ts->tdata->nvdata.tune_fix_ver == 0xffff)
		|| (ts->tdata->afe_base > ts->tdata->nvdata.tune_fix_ver))) {
		/* tune version up case */
		sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TUNEUP);
		restore_cal = true;
	} else if (ts->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
		/* firmup case */
		sec_tclm_root_of_cal(ts->tdata, CALPOSITION_FIRMUP);
		restore_cal = true;
	}

	if (restore_cal) {
		input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
		if (sec_execute_tclm_package(ts->tdata, 0) < 0)
			input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
	}
#endif

done:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	if (fw_entry)
		release_firmware(fw_entry);
exit_fwload:
	return retval;
}
EXPORT_SYMBOL(stm_ts_fw_update_on_probe);

static int stm_ts_load_fw_from_kernel(struct stm_ts_data *ts,
		const char *fw_path)
{
	int retval;
	const struct firmware *fw_entry = NULL;
	u8 *fw_data = NULL;

	if (!fw_path) {
		input_err(true, &ts->client->dev, "%s: Firmware name is not defined\n",
				__func__);
		return -EINVAL;
	}

	input_info(true, &ts->client->dev, "%s: Load firmware : %s\n", __func__,
			fw_path);

	retval = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (retval) {
		input_err(true, &ts->client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		retval = -ENOENT;
		goto done;
	}

	fw_data = (u8 *)fw_entry->data;

	ts->stm_ts_systemreset(ts, 20);

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
#endif

	retval = stm_ts_fw_updater(ts, fw_data);
	if (retval < 0)
		input_err(true, &ts->client->dev, "%s: failed update firmware\n",
				__func__);

#ifdef TCLM_CONCEPT
	input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
	if (sec_execute_tclm_package(ts->tdata, 0) < 0)
		input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);

	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

done:
	if (fw_entry)
		release_firmware(fw_entry);

	return retval;
}

static int stm_ts_load_fw(struct stm_ts_data *ts, int update_type)
{
	const struct firmware *fw_entry;
	int error = 0;
	const struct stm_ts_header *header;
	const char *file_path;
	bool is_fw_signed = false;

	switch (update_type) {
	case UMS:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		file_path = TSP_EXTERNAL_FW;
#else
		file_path = TSP_EXTERNAL_FW_SIGNED;
		is_fw_signed = true;
#endif
		break;
	case SPU:
		file_path = TSP_SPU_FW_SIGNED;
		is_fw_signed = true;
		break;
	default:
		return -EINVAL;
	}

	error = request_firmware(&fw_entry, file_path, &ts->client->dev);
	if (error) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d\n", __func__, error);
		return -ENOENT;
	}

	if (fw_entry->size <= 0) {
		input_err(true, &ts->client->dev, "%s: fw size error %ld\n", __func__, fw_entry->size);
		error = -ENOENT;
		goto done;
	}

	input_info(true, &ts->client->dev,
			"%s: start, file path %s, size %ld Bytes\n",
			__func__, file_path, fw_entry->size);

	header = (struct stm_ts_header *)fw_entry->data;
	if (header->signature != STM_TS_FILE_SIGNATURE) {
		error = -EIO;
		input_err(true, &ts->client->dev,
				"%s: File type is not match with STM_TS64 file. [%8x]\n",
				__func__, header->signature);
		goto done;
	}

	input_info(true, &ts->client->dev,
			"%s:%s [FILE:STM] fw:0x%04X, main:0x%04X | [FILE:SEC] IC:0x%02X, Proj:0x%02X, Module:0x%02X, FW ver:0x%02X\n",
			__func__, is_fw_signed ? " [SIGNED]":"", (u16)header->fw_ver, (u16)header->ext_release_ver,
			header->ic_name, header->project_id, header->module_ver, header->ext_release_ver & 0xFF);

#ifdef SUPPORT_FW_SIGNED
	if (is_fw_signed) {
		long spu_ret = 0, org_size = 0;
		u8 *ver_ic = ts->plat_data->img_version_of_ic;

		if (update_type == SPU && ver_ic[SEC_INPUT_FW_IC_VER] == header->ic_name
				&& ver_ic[SEC_INPUT_FW_VER_PROJECT_ID] == header->project_id
				&& ver_ic[SEC_INPUT_FW_MODULE_VER] == header->module_ver) {
			if (ver_ic[SEC_INPUT_FW_VER] >= header->ext_release_ver & 0xFF) {
				input_info(true, &ts->client->dev, "%s: skip spu update\n", __func__);
				error = 0;
				goto done;
			} else {
				input_info(true, &ts->client->dev, "%s: run spu update\n", __func__);
			}
		} else if (update_type == UMS &&
				(ver_ic[SEC_INPUT_FW_IC_VER] == header->ic_name)) {
			input_info(true, &ts->client->dev, "%s: run signed fw update\n", __func__);
		} else {
			input_err(true, &ts->client->dev,
					"%s: not matched product version\n", __func__);
			error = -ENOENT;
			goto done;
		}

		/* digest 32, signature 512, TSP tag 3 */
		org_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != org_size) {
			input_err(true, &ts->client->dev,
					"%s: signature verify failed, %ld/%ld\n",
					__func__, spu_ret, org_size);
			error = -EIO;
			goto done;
		}
	}
#endif

	ts->stm_ts_systemreset(ts, 0);

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
#endif

	error = stm_ts_fw_updater(ts, (u8 *)fw_entry->data);

#ifdef TCLM_CONCEPT
	input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
	if (sec_execute_tclm_package(ts->tdata, 0) < 0)
		input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);

	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

done:
	if (error < 0)
		input_err(true, &ts->client->dev, "%s: failed update firmware\n",
				__func__);

	release_firmware(fw_entry);
	return error;
}

int stm_ts_fw_update_on_hidden_menu(struct stm_ts_data *ts, int update_type)
{
	int retval = 0;

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [SPU] Getting firmware from apk.
	 */
	switch (update_type) {
	case BUILT_IN:
		retval = stm_ts_load_fw_from_kernel(ts, ts->plat_data->firmware_name);
		break;

	case UMS:
	case SPU:
		retval = stm_ts_load_fw(ts, update_type);
		break;

	default:
		input_err(true, &ts->client->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	return retval;
}
EXPORT_SYMBOL(stm_ts_fw_update_on_hidden_menu);
