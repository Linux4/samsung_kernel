/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 *
 * $Revision: 33373 $
 * $Date: 2018-09-05 14:47:16 +0800 (週三, 05 九月 2018) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>

#include "nt36xxx.h"

#define NVT_TS_DEFAULT_UMS_FW		"/sdcard/firmware/tsp/nvt.bin"


#define FW_BIN_VER_OFFSET	0x1A000
#define FW_BIN_VER_BAR_OFFSET	0x1A001

#define NVT_FLASH_END_FLAG_LEN	3
#define NVT_FLASH_END_FLAG_ADDR	0x1AFFD

#define FLASH_SECTOR_SIZE 4096

void nvt_ts_stop_crc_reboot(struct nvt_ts_data *ts);


static int update_firmware_request(struct nvt_ts_data *ts, const char *filename)
{
	const struct firmware *fw_entry;
	int ret;

	ts->fw_ver_bin = 0;
	ts->fw_ver_bin_bar = 0;

	if (!filename)
		return -EPERM;

	input_info(true, &ts->client->dev, "%s: filename is %s\n", __func__, filename);

	ret = request_firmware(&fw_entry, filename, &ts->client->dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: firmware load failed, ret=%d\n",
				__func__, ret);
		return ret;
	}

	// check bin file size (116kb)
	if (fw_entry->size != FW_BIN_SIZE) {
		input_err(true, &ts->client->dev, "bin file size not match. (%zu)\n", fw_entry->size);
		release_firmware(fw_entry);
		return -EINVAL;
	}

	// check if FW version add FW version bar equals 0xFF
	if (fw_entry->data[FW_BIN_VER_OFFSET] + fw_entry->data[FW_BIN_VER_BAR_OFFSET] != 0xFF) {
		input_err(true, &ts->client->dev, "bin file FW_VER + FW_VER_BAR should be 0xFF!\n");
		input_err(true, &ts->client->dev, "FW_VER=0x%02X, FW_VER_BAR=0x%02X\n",
				fw_entry->data[FW_BIN_VER_OFFSET], fw_entry->data[FW_BIN_VER_BAR_OFFSET]);
		release_firmware(fw_entry);
		return -EPERM;
	}

	ts->fw_ver_bin = fw_entry->data[FW_BIN_VER_OFFSET];
	ts->fw_ver_bin_bar = fw_entry->data[FW_BIN_VER_BAR_OFFSET];

	ts->fw_entry = fw_entry;

	return 0;
}

static void update_firmware_release(struct nvt_ts_data *ts)
{
	if (ts->fw_entry)
		release_firmware(ts->fw_entry);

	ts->fw_entry = NULL;
}

static int nvt_ts_check_fw_ver(struct nvt_ts_data *ts)
{
	u8 buf[4] = {0};
	int ret = 0;

	//write i2c index to EVENT BUF ADDR
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write cmd\n");
		return ret;
	}

	//read Firmware Version
	buf[0] = EVENT_MAP_FWINFO;
	ret = nvt_ts_i2c_read(ts, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read cmd\n");
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: IC - FW Ver = 0x%02X, FW Ver Bar = 0x%02X\n",
			__func__, buf[1], buf[2]);
	input_info(true, &ts->client->dev, "%s: Bin - FW Ver = 0x%02X, FW ver Bar = 0x%02X\n",
			__func__, ts->fw_ver_bin, ts->fw_ver_bin_bar);

	// check IC FW_VER + FW_VER_BAR equals 0xFF or not, need to update if not
	if ((buf[1] + buf[2]) != 0xFF) {
		input_err(true, &ts->client->dev, "%s: IC - FW_VER + FW_VER_BAR not equals to 0xFF\n",
			__func__);
		return -EPERM;
	}

	if (buf[1] < ts->fw_ver_bin) {
		input_err(true, &ts->client->dev, "%s: need to firmware update\n",
			__func__);
		return -EPERM;
	} else if (buf[1] == ts->fw_ver_bin) {
		input_info(true, &ts->client->dev, "%s: need to check checksum data\n",
			__func__);
		return 0;
	} else {
		input_info(true, &ts->client->dev, "%s: skip firmware update\n",
			__func__);
		return 1;
	}
}

int nvt_ts_resume_pd(struct nvt_ts_data *ts)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// Resume Command
	buf[0] = 0x00;
	buf[1] = 0xAB;
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Write Enable error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Resume Command)
	retry = 0;
	while(1) {
		msleep(1);

		buf[0] = 0x00;
		ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Check 0xAA (Resume Command) error!!(%d)\n", ret);
			return ret;
		}

		if (buf[1] == 0xAA)
			break;

		retry++;
		if (unlikely(retry > 20)) {
			input_err(true, &ts->client->dev, "Check 0xAA (Resume Command) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}
	msleep(10);

	input_info(true, &ts->client->dev, "Resume PD OK\n");

	return 0;
}

static int nvt_ts_check_checksum(struct nvt_ts_data *ts)
{
	u8 buf[64] = {0};
	int xdata_addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;
	int ret = 0;
	int i, k;
	u16 WR_Filechksum[BLOCK_64KB_NUM] = {0};
	u16 RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t fw_bin_size = 0;
	size_t len_in_blk = 0;
	int retry = 0;

	if (nvt_ts_resume_pd(ts)) {
		input_err(true, &ts->client->dev, "Resume PD error!!\n");
		return -EPERM;
	}

	fw_bin_size = ts->fw_entry->size;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_bin_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_bin_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += ts->fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;

			ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);

				buf[0] = 0x00;
				ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					input_err(true, &ts->client->dev, "Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}

				if (buf[1] == 0xAA)
					break;

				retry++;
				if (unlikely(retry > 5)) {
					input_err(true, &ts->client->dev, "Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			buf[0] = 0xFF;
			buf[1] = xdata_addr >> 16;
			buf[2] = (xdata_addr >> 8) & 0xFF;

			ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (xdata_addr) & 0xFF;
			ret = nvt_ts_i2c_read(ts, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			input_info(true, &ts->client->dev, "RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				ts->checksum_result = 1;
				input_err(true, &ts->client->dev, "firmware checksum not match!!\n");
				return 0;
			}
		}
	}

	input_info(true, &ts->client->dev, "firmware checksum match\n");

	return 1;
}

/*******************************************************
Description:
	Novatek touchscreen initial bootloader and flash
	block function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int nvt_ts_init_bootloader(struct nvt_ts_data *ts)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// SW Reset & Idle
	nvt_ts_sw_reset_idle(ts);

	// Initiate Flash Block
	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = I2C_FW_Address;
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Inittial Flash Block error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Initiate Flash Block)
	retry = 0;
	while(1) {
		msleep(1);

		buf[0] = 0x00;
		ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Check 0xAA (Inittial Flash Block) error!!(%d)\n", ret);
			return ret;
		}

		if (buf[1] == 0xAA)
			break;

		retry++;
		if (unlikely(retry > 20)) {
			input_err(true, &ts->client->dev, "Check 0xAA (Inittial Flash Block) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	input_info(true, &ts->client->dev, "Init OK \n");
	msleep(20);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen erase flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int nvt_ts_erase_flash(struct nvt_ts_data *ts)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t count = 0;
	int32_t i = 0;
	int32_t Flash_Address = 0;
	int32_t retry = 0;

	// Write Enable
	buf[0] = 0x00;
	buf[1] = 0x06;
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Write Enable (for Write Status Register) error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Enable)
	retry = 0;
	while (1) {
		mdelay(1);

		buf[0] = 0x00;
		ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Check 0xAA (Write Enable for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		if (buf[1] == 0xAA)
			break;

		retry++;
		if (unlikely(retry > 20)) {
			input_err(true, &ts->client->dev, "Check 0xAA (Write Enable for Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Write Status Register
	buf[0] = 0x00;
	buf[1] = 0x01;
	buf[2] = 0x00;
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Write Status Register error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Status Register)
	retry = 0;
	while (1) {
		mdelay(1);

		buf[0] = 0x00;
		ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Check 0xAA (Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		if (buf[1] == 0xAA)
			break;

		retry++;
		if (unlikely(retry > 20)) {
			input_err(true, &ts->client->dev, "Check 0xAA (Write Status Register) error!! status=0x%02X\n", buf[1]);
			return -1;
		}
	}

	// Read Status
	retry = 0;
	while (1) {
		mdelay(5);
		buf[0] = 0x00;
		buf[1] = 0x05;
		ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Read Status (for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		// Check 0xAA (Read Status)
		buf[0] = 0x00;
		ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 3);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Check 0xAA (Read Status for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		if ((buf[1] == 0xAA) && (buf[2] == 0x00))
			break;

		retry++;
		if (unlikely(retry > 100)) {
			input_err(true, &ts->client->dev, "Check 0xAA (Read Status for Write Status Register) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
			return -1;
		}
	}

	if (ts->fw_entry->size % FLASH_SECTOR_SIZE)
		count = ts->fw_entry->size / FLASH_SECTOR_SIZE + 1;
	else
		count = ts->fw_entry->size / FLASH_SECTOR_SIZE;

	for(i = 0; i < count; i++) {
		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Write Enable error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			mdelay(1);

			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}

			if (buf[1] == 0xAA)
				break;

			retry++;
			if (unlikely(retry > 20)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		Flash_Address = i * FLASH_SECTOR_SIZE;

		// Sector Erase
		buf[0] = 0x00;
		buf[1] = 0x20;    // Command : Sector Erase
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 5);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Sector Erase error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Sector Erase)
		retry = 0;
		while (1) {
			mdelay(1);

			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Check 0xAA (Sector Erase) error!!(%d,%d)\n", ret, i);
				return ret;
			}

			if (buf[1] == 0xAA)
				break;

			retry++;
			if (unlikely(retry > 20)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Sector Erase) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}

		// Read Status
		retry = 0;
		while (1) {
			mdelay(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Status error!!(%d,%d)\n", ret, i);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Check 0xAA (Read Status) error!!(%d,%d)\n", ret, i);
				return ret;
			}

			if ((buf[1] == 0xAA) && (buf[2] == 0x00))
				break;

			retry++;
			if (unlikely(retry > 100)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
	}

	input_info(true, &ts->client->dev, "Erase OK \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen write flash sectors function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int nvt_ts_write_flash(struct nvt_ts_data *ts)
{
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = ts->mmap->RW_FLASH_DATA_ADDR;
	uint32_t Flash_Address = 0;
	int32_t i = 0, j = 0, k = 0;
	uint8_t tmpvalue = 0;
	int32_t count = 0;
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t percent = 0;
	int32_t previous_percent = -1;

	// change I2C buffer index
	buf[0] = 0xFF;
	buf[1] = XDATA_Addr >> 16;
	buf[2] = (XDATA_Addr >> 8) & 0xFF;
	ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "change I2C buffer index error!!(%d)\n", ret);
		return ret;
	}

	if (ts->fw_entry->size % 256)
		count = ts->fw_entry->size / 256 + 1;
	else
		count = ts->fw_entry->size / 256;

	for (i = 0; i < count; i++) {
		Flash_Address = i * 256;

		// Write Enable
		buf[0] = 0x00;
		buf[1] = 0x06;
		ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Write Enable error!!(%d)\n", ret);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			udelay(100);

			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}

			if (buf[1] == 0xAA)
				break;

			retry++;
			if (unlikely(retry > 20)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[1]);
				return -1;
			}
		}

		// Write Page : 256 bytes
		for (j = 0; j < min(ts->fw_entry->size - i * 256, (size_t)256); j += 32) {
			buf[0] = (XDATA_Addr + j) & 0xFF;
			for (k = 0; k < 32; k++) {
				buf[1 + k] = ts->fw_entry->data[Flash_Address + j + k];
			}
			ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 33);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Write Page error!!(%d), j=%d\n", ret, j);
				return ret;
			}
		}
		if (ts->fw_entry->size - Flash_Address >= 256)
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (255);
		else
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (ts->fw_entry->size - Flash_Address - 1);

		for (k = 0;k < min(ts->fw_entry->size - Flash_Address,(size_t)256); k++)
			tmpvalue += ts->fw_entry->data[Flash_Address + k];

		tmpvalue = 255 - tmpvalue + 1;

		// Page Program
		buf[0] = 0x00;
		buf[1] = 0x02;
		buf[2] = ((Flash_Address >> 16) & 0xFF);
		buf[3] = ((Flash_Address >> 8) & 0xFF);
		buf[4] = (Flash_Address & 0xFF);
		buf[5] = 0x00;
		buf[6] = min(ts->fw_entry->size - Flash_Address,(size_t)256) - 1;
		buf[7] = tmpvalue;
		ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 8);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "Page Program error!!(%d), i=%d\n", ret, i);
			return ret;
		}
		// Check 0xAA (Page Program)
		retry = 0;
		while (1) {
			mdelay(1);

			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Page Program error!!(%d)\n", ret);
				return ret;
			}

			if (buf[1] == 0xAA || buf[1] == 0xEA)
				break;

			retry++;
			if (unlikely(retry > 20)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Page Program) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			input_err(true, &ts->client->dev, "Page Program error!! i=%d\n", i);
			return -3;
		}

		// Read Status
		retry = 0;
		while (1) {
			mdelay(5);
			buf[0] = 0x00;
			buf[1] = 0x05;
			ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Status error!!(%d)\n", ret);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[0] = 0x00;
			ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Check 0xAA (Read Status) error!!(%d)\n", ret);
				return ret;
			}

			if (((buf[1] == 0xAA) && (buf[2] == 0x00)) || (buf[1] == 0xEA))
				break;

			retry++;
			if (unlikely(retry > 100)) {
				input_err(true, &ts->client->dev, "Check 0xAA (Read Status) failed, buf[1]=0x%02X, buf[2]=0x%02X, retry=%d\n", buf[1], buf[2], retry);
				return -1;
			}
		}
		if (buf[1] == 0xEA) {
			input_err(true, &ts->client->dev, "Page Program error!! i=%d\n", i);
			return -4;
		}

		percent = ((i + 1) * 100) / count;
		if (((percent % 10) == 0) && (percent != previous_percent)) {
			input_info(true, &ts->client->dev, "Programming...%2d%%\n", percent);
			previous_percent = percent;
		}
	}

	input_info(true, &ts->client->dev, "Program OK         \n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen verify checksum of written
	flash function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int nvt_ts_verify_flash(struct nvt_ts_data *ts)
{
	uint8_t buf[64] = {0};
	uint32_t XDATA_Addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t fw_bin_size = 0;
	size_t len_in_blk = 0;
	int32_t retry = 0;

	fw_bin_size = ts->fw_entry->size;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_bin_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_bin_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += ts->fw_entry->data[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[0] = 0x00;
			buf[1] = 0x07;
			buf[2] = i;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[6] = (len_in_blk - 1) & 0xFF;
			ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);

				buf[0] = 0x00;
				ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					input_err(true, &ts->client->dev, "Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}

				if (buf[1] == 0xAA)
					break;

				retry++;
				if (unlikely(retry > 5)) {
					input_err(true, &ts->client->dev, "Check 0xAA (Fast Read Command) failed, buf[1]=0x%02X, retry=%d\n", buf[1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			buf[0] = 0xFF;
			buf[1] = XDATA_Addr >> 16;
			buf[2] = (XDATA_Addr >> 8) & 0xFF;
			ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[0] = (XDATA_Addr) & 0xFF;
			ret = nvt_ts_i2c_read(ts, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2] << 8) | buf[1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				input_err(true, &ts->client->dev, "Verify Fail%d!!\n", i);
				input_err(true, &ts->client->dev, "RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				return -1;
			}
		}
	}

	input_info(true, &ts->client->dev, "Verify OK \n");
	return 0;
}

static int nvt_ts_update_firmware(struct nvt_ts_data *ts)
{
	int ret = 0;

	//---Stop CRC check to prevent IC auto reboot---
	nvt_ts_stop_crc_reboot(ts);

	// Step 1 : initial bootloader
	ret = nvt_ts_init_bootloader(ts);
	if (ret)
		return ret;

	// Step 2 : Resume PD
	ret = nvt_ts_resume_pd(ts);
	if (ret)
		return ret;

	// Step 3 : Erase
	ret = nvt_ts_erase_flash(ts);
	if (ret)
		return ret;

	// Step 4 : Program
	ret = nvt_ts_write_flash(ts);
	if (ret)
		return ret;

	// Step 5 : Verify
	ret = nvt_ts_verify_flash(ts);
	if (ret)
		return ret;

	//Step 6 : Bootloader Reset
	nvt_ts_bootloader_reset(ts);
	ret = nvt_ts_check_fw_reset_state(ts, RESET_STATE_INIT);

	return ret;
}

static int nvt_check_flash_end_flag(struct nvt_ts_data *ts)
{
	uint8_t buf[8] = {0};
	uint8_t nvt_end_flag[NVT_FLASH_END_FLAG_LEN + 1] = {0};
	int32_t ret = 0;

	// Step 1 : initial bootloader
	ret = nvt_ts_init_bootloader(ts);
	if (ret) {
		return ret;
	}

	// Step 2 : Resume PD
	ret = nvt_ts_resume_pd(ts);
	if (ret) {
		return ret;
	}

	// Step 3 : unlock
	buf[0] = 0x00;
	buf[1] = 0x35;
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "write unlock error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	//Step 4 : Flash Read Command
	buf[0] = 0x00;
	buf[1] = 0x03;
	buf[2] = (NVT_FLASH_END_FLAG_ADDR >> 16) & 0xFF; //Addr_H
	buf[3] = (NVT_FLASH_END_FLAG_ADDR >> 8) & 0xFF; //Addr_M
	buf[4] = NVT_FLASH_END_FLAG_ADDR & 0xFF; //Addr_L
	buf[5] = (NVT_FLASH_END_FLAG_LEN >> 8) & 0xFF; //Len_H
	buf[6] = NVT_FLASH_END_FLAG_LEN & 0xFF; //Len_L
	ret = nvt_ts_i2c_write(ts, I2C_HW_Address, buf, 7);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "write Read Command error!!(%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Check 0xAA (Read Command)
	buf[0] = 0x00;
	ret = nvt_ts_i2c_read(ts, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Check 0xAA (Read Command) error!!(%d)\n", ret);
		return ret;
	}

	if (buf[1] != 0xAA) {
		input_err(true, &ts->client->dev, "Check 0xAA (Read Command) error!! status=0x%02X\n", buf[1]);
		return -EPERM;
	}

	msleep(10);

	//Step 5 : Read Flash Data
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->READ_FLASH_CHECKSUM_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->READ_FLASH_CHECKSUM_ADDR >> 8) & 0xFF;
	ret = nvt_ts_i2c_write(ts, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "change index error!! (%d)\n", ret);
		return ret;
	}
	msleep(10);

	// Read Back
	buf[0] = ts->mmap->READ_FLASH_CHECKSUM_ADDR & 0xFF;
	ret = nvt_ts_i2c_read(ts, I2C_BLDR_Address, buf, 6);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Read Back error!! (%d)\n", ret);
		return ret;
	}

	//buf[3:5] => NVT End Flag
	strncpy(nvt_end_flag, &buf[3], NVT_FLASH_END_FLAG_LEN);
	input_info(true, &ts->client->dev, "nvt_end_flag=%s (%02X %02X %02X)\n", nvt_end_flag, buf[3], buf[4], buf[5]);

	if (strncmp(nvt_end_flag, "NVT", 3) == 0) {
		return 0;
	} else {
		input_err(true, &ts->client->dev, "\"NVT\" end flag not found!\n");
		return -EPERM;
	}
}

int nvt_ts_fw_update_from_ums(struct nvt_ts_data *ts)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	mutex_lock(&ts->lock);

	fp = filp_open(NVT_TS_DEFAULT_UMS_FW, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		input_err(true, &ts->client->dev, "%s: failed to open %s\n",
			__func__, NVT_TS_DEFAULT_UMS_FW);
			mutex_unlock(&ts->lock);
			set_fs(old_fs);
			return PTR_ERR(fp);
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size > 0) {
		struct firmware fw_entry;
		u8 *fw_data;

		fw_data = vzalloc(fw_size);
		if (!fw_data) {
			input_err(true, &ts->client->dev, "%s: failed to alloc mem\n", __func__);
			ret = -ENOMEM;
			goto out;
		}

		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		if (nread != fw_size) {
			input_err(true, &ts->client->dev, "%s: failed to read firmware file, nread %ld Bytes\n",
				__func__, nread);
			ret = -EIO;

		} else {
			input_info(true, &ts->client->dev, "%s: start, file path %s, size %ld Bytes\n",
				__func__, NVT_TS_DEFAULT_UMS_FW, fw_size);
			input_info(true, &ts->client->dev, "%s: firmware version %02X\n",
				__func__, fw_data[FW_BIN_VER_OFFSET]);

			if (ts->client->irq)
				disable_irq(ts->client->irq);

			fw_entry.data = fw_data;
			fw_entry.size = fw_size;

			ts->fw_entry = &fw_entry;

			nvt_ts_sw_reset_idle(ts);

			ret = nvt_ts_update_firmware(ts);

			ts->fw_entry = NULL;

			if (ts->client->irq)
				enable_irq(ts->client->irq);
		}

		vfree(fw_data);
	}

out:
	mutex_unlock(&ts->lock);

	filp_close(fp, NULL);
	set_fs(old_fs);

	return ret;
}

int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts)
{
	int ret = 0;

	ret = update_firmware_request(ts, ts->platdata->firmware_name);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
		return ret;
	}

	mutex_lock(&ts->lock);

	nvt_ts_sw_reset_idle(ts);

	ret = nvt_ts_update_firmware(ts);

	mutex_unlock(&ts->lock);

	update_firmware_release(ts);

	return ret;
}

int nvt_ts_firmware_update_on_probe(struct nvt_ts_data *ts, bool bforce)
{
	int ret;

	ret = update_firmware_request(ts, ts->platdata->firmware_name);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: request firmware done, size = %d\n",
		__func__, (int)ts->fw_entry->size);

	nvt_ts_sw_reset_idle(ts);

	if (bforce) {
		input_info(true, &ts->client->dev, "%s: fw update by force\n", __func__);
		goto start;
	}

	ret = nvt_ts_check_fw_ver(ts);
	if(ts->platdata->bringup){	
		input_info(true, &ts->client->dev, "%s: skip fw update when bringup\n", __func__);
		ret = 1;   // skip fw update when bringup
	}	
	if (!ret) {
		if (nvt_ts_check_checksum(ts)) {
			if (!nvt_check_flash_end_flag(ts)) {
				ret = 0;
				goto out;
			}
		}
	} else if (ret > 0) {
		ret = 0;
		goto out;
	}

start:
	ret = nvt_ts_update_firmware(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to firmware update",
			__func__);
	}
out:
	release_firmware(ts->fw_entry);

	nvt_ts_bootloader_reset(ts);
	nvt_ts_check_fw_reset_state(ts, RESET_STATE_INIT);

	return ret;
}
