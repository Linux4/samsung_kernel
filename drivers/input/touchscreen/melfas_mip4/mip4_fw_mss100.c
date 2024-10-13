/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2018 MELFAS Inc.
 *
 *
 * mip4_fw_mss100.c : Firmware update functions for MSS100
 *
 * Version : 2019.03.14
 */

#include "mip4_ts.h"

/* Firmware info */
#define FW_CHIP_CODE "S1H0"
#define FW_TYPE_TAIL

/* ISC info */
#define ISC_PAGE_SIZE 128

/* ISC command */
#define ISC_CMD_ENTER      {0xFB, 0x4A, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_MASS_ERASE {0xFB, 0x4A, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_PAGE_READ  {0xFB, 0x4A, 0x00, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_PAGE_WRITE {0xFB, 0x4A, 0x00, 0xA5, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_ECC_READ   {0xFB, 0x4A, 0x00, 0xC2, 0x00, 0x01, 0xFF, 0x80}
#define ISC_CMD_MODE_READ  {0xFB, 0xB7, 0xDD, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_MODE_WRITE {0xFB, 0xB7, 0xDD, 0xA5, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_STATUS     {0xFB, 0x4A, 0x36, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_EXIT       {0xFB, 0x4A, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00}

/* ISC value */
#define ISC_ENTER_NORMAL  0
#define ISC_ENTER_ECC_OFF 1
#define ISC_ENTER_ECC_ON  2

#define ISC_STATUS_BUSY 0x96
#define ISC_STATUS_DONE 0xAD

#define ISC_ECC_ERROR {0xDE, 0xAD, 0xDE, 0xAD}

/*
 * Firmware binary tail info
 */
struct melfas_bin_tail {
	u8 tail_mark[4];
	char chip_name[4];
	u32 bin_start_addr;
	u32 bin_length;

	u8 version[8];
	u8 boot_start;
	u8 boot_end;
	u8 fw_start;
	u8 fw_end;
	u8 section3_start;
	u8 section3_end;
	u8 section4_start;
	u8 section4_end;

	u8 checksum_type;
	u8 hw_category;
	u16 param_id;
	u32 param_length;
	u32 build_date;
	u32 build_time;

	u32 reserved1;
	u32 reserved2;
	u16 reserved3;
	u16 tail_size;
	u32 crc;
} __attribute__ ((packed));

#define MELFAS_BIN_TAIL_MARK {0x4D, 0x42, 0x54, 0x01} /* M B T 0x01 */
#define MELFAS_BIN_TAIL_SIZE 64

/*
 * ISC : Read status
 */
static int isc_read_status(struct mip4_ts_info *info)
{
	u8 cmd[8] = ISC_CMD_STATUS;
	u8 result = 0;
	int cnt = 300;
	int ret = 0;
	struct i2c_msg msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = cmd,
			.len = 8,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = &result,
			.len = 1,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	do {
		if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
			ret = -1;
			goto error;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			msleep(1);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] wrong status [0x%02X]\n", __func__, result);
			ret = -1;
			msleep(1);
		}
	} while (--cnt);

	if (!cnt) {
		dev_err(&info->client->dev, "%s [ERROR] count overflow - count[%d] status[0x%02X]\n", __func__, cnt, result);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/*
 * ISC : Erase - Mass
 */
static int isc_erase_mass(struct mip4_ts_info *info)
{
	u8 write_buf[8] = ISC_CMD_MASS_ERASE;
	struct i2c_msg msg[1] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	if (isc_read_status(info) != 0) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Read - Page
 */
static int isc_read_page(struct mip4_ts_info *info, int addr, u8 *data)
{
	u8 write_buf[8] = ISC_CMD_PAGE_READ;
	struct i2c_msg msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = ISC_PAGE_SIZE,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)((addr >> 24) & 0xFF);
	write_buf[5] = (u8)((addr >> 16) & 0xFF);
	write_buf[6] = (u8)((addr >> 8) & 0xFF);
	write_buf[7] = (u8)(addr & 0xFF);
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s - addr[0x%06X]\n", __func__, addr);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Write - Page
 */
static int isc_write_page(struct mip4_ts_info *info, int addr, const u8 *data, int length)
{
	u8 write_buf[8 + ISC_PAGE_SIZE] = ISC_CMD_PAGE_WRITE;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (length > ISC_PAGE_SIZE) {
		dev_err(&info->client->dev, "%s [ERROR] page size limit [%d]\n", __func__, length);
		goto error;
	}

	write_buf[4] = (u8)((addr >> 24) & 0xFF);
	write_buf[5] = (u8)((addr >> 16) & 0xFF);
	write_buf[6] = (u8)((addr >> 8) & 0xFF);
	write_buf[7] = (u8)(addr & 0xFF);

	memcpy(&write_buf[8], data, length);

	if (i2c_master_send(info->client, write_buf, (8 + length)) != (8 + length)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	if (isc_read_status(info) != 0) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s - addr[0x%06X] length[%d]\n", __func__, addr, length);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Read - ECC
 */
static int isc_read_ecc(struct mip4_ts_info *info, u8 *data)
{
	u8 write_buf[8] = ISC_CMD_ECC_READ;
	struct i2c_msg msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = 4,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Enter
 */
static int isc_enter(struct mip4_ts_info *info, u8 option)
{
	u8 write_buf[8] = ISC_CMD_ENTER;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	write_buf[7] = option;

	if (i2c_master_send(info->client, write_buf, 8) != 8) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	if (isc_read_status(info) != 0) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Exit
 */
static int isc_exit(struct mip4_ts_info *info)
{
	u8 write_buf[8] = ISC_CMD_EXIT;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_master_send(info->client, write_buf, 8) != 8) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Enable
 */
static int isc_enable(struct mip4_ts_info *info)
{
	u8 read_wbuf[8] = ISC_CMD_MODE_READ;
	u8 read_rbuf[16] = {0, };
	struct i2c_msg read_msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = read_wbuf,
			.len = 8,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = read_rbuf,
			.len = 16,
		},
	};
	u8 write_wbuf[8 + 16] = ISC_CMD_MODE_WRITE;
	int i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(info->client->adapter, read_msg, ARRAY_SIZE(read_msg)) != ARRAY_SIZE(read_msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	for (i = 0; i < 12; i++) {
		write_wbuf[8 + i] = read_rbuf[i];
	}
	write_wbuf[8 + 12] = 0x70;
	write_wbuf[8 + 13] = 0x32;
	write_wbuf[8 + 14] = 0x30;
	write_wbuf[8 + 15] = 0x00;

	if (i2c_master_send(info->client, write_wbuf, (8 + 16)) != (8 + 16)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * Flash firmware to the chip
 */
int mip4_ts_flash_fw(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section)
{
	struct melfas_bin_tail *bin_info;
	int ret = 0;
	int retry = 3;
	u8 rbuf[ISC_PAGE_SIZE];
	int addr = 0;
	int addr_start = 0;
	int bin_size = 0;
	u8 *bin_data;
	u16 tail_size = 0;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;
	u8 ver_chip[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	u8 ecc_error[4] = ISC_ECC_ERROR;
	u8 fw_uptodate = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		dev_err(&info->client->dev, "%s [ERROR] Wrong tail size [%d]\n", __func__, tail_size);
		ret = FW_ERR_FILE_TYPE;
		goto error_file;
	}

	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		dev_err(&info->client->dev, "%s [ERROR] Wrong tail mark\n", __func__);
		ret = FW_ERR_FILE_TYPE;
		goto error_file;
	}

	/* Read bin info */
	bin_info = (struct melfas_bin_tail *) &fw_data[fw_size - tail_size];
	dev_dbg(&info->client->dev, "%s - bin_info : bin_len[%d] hw_cat[0x%02X] date[%04X] time[%04X] tail_size[%d]\n", __func__, bin_info->bin_length, bin_info->hw_category, bin_info->build_date, bin_info->build_time, bin_info->tail_size);

#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Bin Info : ", DUMP_PREFIX_OFFSET, 16, 1, bin_info, tail_size, false);
#endif

	/* Check chip code */
	if (memcmp(bin_info->chip_name, FW_CHIP_CODE, 4)) {
		dev_err(&info->client->dev, "%s [ERROR] Firmware binary is not for %s\n", __func__, CHIP_NAME);
		ret = FW_ERR_FILE_TYPE;
		goto error_file;
	}

	/* Reset */
#if 0
	mip4_ts_power_off(info, POWER_OFF_DELAY);
	mip4_ts_power_on(info, POWER_ON_DELAY_ISC);
#endif

	/* Load bin data */
	bin_size = bin_info->bin_length;
	bin_data = kzalloc(sizeof(u8) * bin_size, GFP_KERNEL);
	memcpy(bin_data, fw_data, bin_size);

	/* Check F/W version */
	dev_info(&info->client->dev, "%s - Firmware binary version [%02X%02X %02X%02X %02X%02X %02X%02X]\n", __func__, bin_info->version[0], bin_info->version[1], bin_info->version[2], bin_info->version[3], bin_info->version[4], bin_info->version[5], bin_info->version[6], bin_info->version[7]);

	if (force == 1) {
		/* Force update */
		dev_info(&info->client->dev, "%s - Force update\n", __func__);
	} else {
		while (retry--) {
			if (!mip4_ts_get_fw_version(info, ver_chip)) {
				break;
			} else {
				mip4_ts_reset(info);
			}
		}
		if (retry <= 0) {
			dev_err(&info->client->dev, "%s [ERROR] Failed to read chip firmware version\n", __func__);
		} else {
			dev_info(&info->client->dev, "%s - Chip firmware version [%02X%02X %02X%02X %02X%02X %02X%02X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3], ver_chip[4], ver_chip[5], ver_chip[6], ver_chip[7]);

			if (memcmp(bin_info->version, ver_chip, 8) == 0) {
				fw_uptodate = 1;
			}
		}
	}

	/* Enter ISC mode */
	dev_dbg(&info->client->dev, "%s - Enter ISC mode\n", __func__);
	ret = isc_enter(info, ISC_ENTER_ECC_ON);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_enter[0x%02X]\n", __func__, ISC_ENTER_ECC_ON);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	if (force == 0) {
		/* Check ECC */
		if (isc_read_ecc(info, rbuf)) {
			dev_err(&info->client->dev, "%s [ERROR] isc_read_ecc\n", __func__);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
		dev_info(&info->client->dev, "%s - ECC [0x%02X%02X%02X%02X]\n", __func__, rbuf[0], rbuf[1], rbuf[2], rbuf[3]);

		if (memcmp(ecc_error, rbuf, 4) == 0) {
			dev_info(&info->client->dev, "%s - ECC error\n", __func__);
		} else {
			/* Check version */
			if (fw_uptodate == 1) {
				dev_info(&info->client->dev, "%s - Chip firmware is already up-to-date\n", __func__);
				ret = FW_ERR_UPTODATE;
				goto uptodate;
			}
		}
	}

	/* Enable */
	dev_dbg(&info->client->dev, "%s - Enable\n", __func__);
	ret = isc_enable(info);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_enable\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	/* Erase */
	dev_dbg(&info->client->dev, "%s - Erase\n", __func__);
	ret = isc_erase_mass(info);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_erase_mass\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	/* Write & Verify */
	dev_dbg(&info->client->dev, "%s - Write & Verify\n", __func__);
	addr = bin_size - ISC_PAGE_SIZE;
	while (addr >= addr_start) {
		/* Write page */
		if (isc_write_page(info, addr, &bin_data[addr], ISC_PAGE_SIZE)) {
			dev_err(&info->client->dev, "%s [ERROR] isc_write_page : addr[0x%06X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
		dev_dbg(&info->client->dev, "%s - isc_write_page : addr[0x%06X]\n", __func__, addr);

		/* Verify page */
		if (isc_read_page(info, addr, rbuf)) {
			dev_err(&info->client->dev, "%s [ERROR] isc_read_page : addr[0x%06X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
		dev_dbg(&info->client->dev, "%s - isc_read_page : addr[0x%06X]\n", __func__, addr);

#if FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Write : ", DUMP_PREFIX_OFFSET, 16, 1, &bin_data[addr], ISC_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Read  : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

		if (memcmp(rbuf, &bin_data[addr], ISC_PAGE_SIZE)) {
			dev_err(&info->client->dev, "%s [ERROR] Verify failed : addr[0x%06X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}

		/* Next address */
		addr -= ISC_PAGE_SIZE;
	}

uptodate:
	/* Exit ISC mode */
	dev_dbg(&info->client->dev, "%s - Exit ISC mode\n", __func__);
	isc_exit(info);

	/* Reset */
#if 0
	mip4_ts_reset(info);
#else
	msleep(POWER_ON_DELAY);
#endif

	/* Check chip firmware version */
	if (mip4_ts_get_fw_version(info, ver_chip)) {
		dev_err(&info->client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	} else {
		if (memcmp(bin_info->version, ver_chip, 8) == 0) {
			dev_dbg(&info->client->dev, "%s - Version check OK\n", __func__);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Version mismatch after flash\n", __func__);
			dev_err(&info->client->dev, "%s [ERROR] Chip[%02X%02X %02X%02X %02X%02X %02X%02X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3], ver_chip[4], ver_chip[5], ver_chip[6], ver_chip[7]);
			dev_err(&info->client->dev, "%s [ERROR] File[%02X%02X %02X%02X %02X%02X %02X%02X]\n", __func__, bin_info->version[0], bin_info->version[1], bin_info->version[2], bin_info->version[3], bin_info->version[4], bin_info->version[5], bin_info->version[6], bin_info->version[7]);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
	}

	kfree(bin_data);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	goto exit;

error_update:
	kfree(bin_data);
error_file:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
exit:
	return ret;
}

/*
 * Get version of firmware binary file
 */
int mip4_ts_bin_fw_version(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct melfas_bin_tail *bin_info;
	u16 tail_size = 0;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;
	int i = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		dev_err(&info->client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		goto error;
	}

	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		dev_err(&info->client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		goto error;
	}

	/* Read bin info */
	bin_info = (struct melfas_bin_tail *) &fw_data[fw_size - tail_size];

	/* F/W version */
	for (i = 0; i < 8; i++) {
		ver_buf[i] = bin_info->version[i];
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

int mip4_ts_isc_for_power_off(struct mip4_ts_info *info)
{
	int ret = 0;

	/* Enter ISC mode for power off */
	dev_dbg(&info->client->dev, "%s - Enter ISC mode for power off\n", __func__);
	ret = isc_enter(info, ISC_ENTER_ECC_ON);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_enter[0x%02X]\n", __func__, ISC_ENTER_ECC_ON);
		goto error;
	}
	msleep(2);
	ret = isc_enter(info, ISC_ENTER_ECC_ON);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_enter[0x%02X]\n", __func__, ISC_ENTER_ECC_ON);
		goto error;
	}
	msleep(2);
	ret = isc_enter(info, ISC_ENTER_ECC_ON);
	if (ret != 0) {
		dev_err(&info->client->dev, "%s [ERROR] isc_enter[0x%02X]\n", __func__, ISC_ENTER_ECC_ON);
		goto error;
	}
	msleep(6);

	return 0;

error:
	return 1;
}