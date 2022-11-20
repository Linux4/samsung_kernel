/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2018 MELFAS Inc.
 *
 *
 * mip4_fw_mss100.c : Firmware update functions for MSS100
 *
 * Version : 2018.10.18
 */

#include "melfas_dev.h"

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
#define ISC_CMD_STATUS     {0xFB, 0x4A, 0x36, 0xC2, 0x00, 0x00, 0x00, 0x00}
#define ISC_CMD_EXIT       {0xFB, 0x4A, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00}

/* ISC value */
#define ISC_ENTER_NORMAL	0
#define ISC_ENTER_ECC_OFF	1
#define ISC_ENTER_ECC_ON	2

#define ISC_STATUS_BUSY 0x96
#define ISC_STATUS_DONE 0xAD

#define ISC_ECC_ERROR {0xDE, 0xAD, 0xDE, 0xAD}

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	NONE,
	TSP_SPU,
	TSP_VERIFICATION,
};

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
static int isc_read_status(struct melfas_ts_data *ts)
{
	u8 cmd[8] = ISC_CMD_STATUS;
	u8 result = 0;
	int cnt = 300;
	int ret = 0;
	struct i2c_msg msg[2] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = cmd,
			.len = 8,
		}, {
			.addr = ts->client->addr,
			.flags = I2C_M_RD,
			.buf = &result,
			.len = 1,
		},
	};

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	do {
		if (i2c_transfer(ts->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
			input_err(true, &ts->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
			ret = -1;
			goto error;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			usleep_range(1 * 1000, 1 * 1000);
		} else {
			input_err(true, &ts->client->dev, "%s [ERROR] wrong status [0x%02X]\n", __func__, result);
			ret = -1;
			usleep_range(1 * 1000, 1 * 1000);
		}
	} while (--cnt);

	if (!cnt) {
		input_err(true, &ts->client->dev,
			"%s [ERROR] count overflow - cnt [%d] status [0x%02X]\n",
			__func__, cnt, result);
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);

	return ret;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/*
 * ISC : Erase - Mass
 */
static int isc_erase_mass(struct melfas_ts_data *ts)
{
	u8 write_buf[8] = ISC_CMD_MASS_ERASE;
	struct i2c_msg msg[1] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		},
	};

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(ts->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	if (isc_read_status(ts) != 0) {
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);

	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Read - Page
 */
static int isc_read_page(struct melfas_ts_data *ts, int addr, u8 *data)
{
	u8 write_buf[8] = ISC_CMD_PAGE_READ;
	struct i2c_msg msg[2] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		}, {
			.addr = ts->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = ISC_PAGE_SIZE,
		},
	};

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)((addr >> 24) & 0xFF);
	write_buf[5] = (u8)((addr >> 16) & 0xFF);
	write_buf[6] = (u8)((addr >> 8) & 0xFF);
	write_buf[7] = (u8)(addr & 0xFF);
	if (i2c_transfer(ts->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s - addr[0x%04X]\n", __func__, addr);

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Write - Page
 */
static int isc_write_page(struct melfas_ts_data *ts, int addr, const u8 *data, int length)
{
	u8 write_buf[8 + ISC_PAGE_SIZE] = ISC_CMD_PAGE_WRITE;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	if (length > ISC_PAGE_SIZE) {
		input_err(true, &ts->client->dev, "%s [ERROR] page size limit [%d]\n", __func__, length);
		goto error;
	}

	write_buf[4] = (u8)((addr >> 24) & 0xFF);
	write_buf[5] = (u8)((addr >> 16) & 0xFF);
	write_buf[6] = (u8)((addr >> 8) & 0xFF);
	write_buf[7] = (u8)(addr & 0xFF);

	memcpy(&write_buf[8], data, length);

	if (i2c_master_send(ts->client, write_buf, (8 + length)) != (8 + length)) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	if (isc_read_status(ts) != 0) {
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s - addr[0x%04X] length[%d]\n", __func__, addr, length);

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Read - ECC
 */
static int isc_read_ecc(struct melfas_ts_data *ts, u8 *data)
{
	u8 write_buf[8] = ISC_CMD_ECC_READ;
	struct i2c_msg msg[2] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 8,
		}, {
			.addr = ts->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = 4,
		},
	};

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(ts->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Enter
 */
static int isc_enter(struct melfas_ts_data *ts, u8 option)
{
	u8 write_buf[8] = ISC_CMD_ENTER;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	write_buf[7] = option;

	if (i2c_master_send(ts->client, write_buf, 8) != 8) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	if (isc_read_status(ts) != 0) {
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * ISC : Exit
 */
static int isc_exit(struct melfas_ts_data *ts)
{
	u8 write_buf[8] = ISC_CMD_EXIT;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	if (i2c_master_send(ts->client, write_buf, 8) != 8) {
		input_err(true, &ts->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto error;
	}

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * Flash firmware to the chip
 */
int melfas_ts_flash_fw(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, bool on_probe)
{
	struct melfas_bin_tail *bin_ts;
	int ret = 0;
	u8 rbuf[ISC_PAGE_SIZE];
	int addr = 0;
	int addr_start = 0;
	int bin_size = 0;
	u8 *bin_data;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;
	u16 tail_size = 0;
	u8 ver_chip[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	u8 ecc_error[4] = ISC_ECC_ERROR;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

		/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wrong tail size [%d]\n", __func__, tail_size);
		ret = FW_ERR_FILE_TYPE;
		goto error_file;
	}
	input_info(true, &ts->client->dev, "%s - ECC [0x%02X%02X%02X%02X]\n", __func__, rbuf[0], rbuf[1], rbuf[2], rbuf[3]);
	if (memcmp(ecc_error, rbuf, 4) == 0)
			input_info(true, &ts->client->dev, "%s - ECC error\n", __func__);

	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		input_err(true, &ts->client->dev, "%s [ERROR] Wrong tail mark\n", __func__);
		ret = FW_ERR_FILE_TYPE;
		goto error_file;
	}

	/* Read bin info */
	bin_ts = (struct melfas_bin_tail *) &fw_data[fw_size - tail_size];

		/* Load bin data */
	bin_size = bin_ts->bin_length;
	bin_data = kzalloc(sizeof(u8) * bin_size, GFP_KERNEL);
	if (!bin_data)
		goto err_mem_alloc;

	memcpy(bin_data, fw_data, bin_size);

	/* Enter ISC mode */
	input_dbg(false, &ts->client->dev, "%s - Enter ISC mode\n", __func__);
	ret = isc_enter(ts, ISC_ENTER_ECC_ON);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] isc_enter[0x%02X]\n", __func__, ISC_ENTER_NORMAL);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	if (isc_read_ecc(ts, rbuf)) {
		input_err(true, &ts->client->dev, "%s [ERROR] isc_read_ecc\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}
	
	/* Erase */
	input_dbg(false, &ts->client->dev, "%s - Erase\n", __func__);
	ret = isc_erase_mass(ts);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] isc_erase_mass\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	/* Write & Verify */
	input_dbg(false, &ts->client->dev, "%s - Write & Verify\n", __func__);
	addr = bin_size - ISC_PAGE_SIZE;
	while (addr >= addr_start) {
		/* Write page */
		if (isc_write_page(ts, addr, &bin_data[addr], ISC_PAGE_SIZE)) {
			input_err(true, &ts->client->dev, "%s [ERROR] isc_write_page : addr[0x%04X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
		input_dbg(false, &ts->client->dev, "%s - isc_write_page : addr[0x%04X]\n", __func__, addr);

		/* Verify page */
		if (isc_read_page(ts, addr, rbuf)) {
			input_err(true, &ts->client->dev, "%s [ERROR] isc_read_page : addr[0x%04X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}
		input_dbg(false, &ts->client->dev, "%s - isc_read_page : addr[0x%04X]\n", __func__, addr);

#if MMS_FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MELFAS_TS_DEVICE_NAME " Write : ", DUMP_PREFIX_OFFSET, 16, 1, &bin_data[addr], ISC_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MELFAS_TS_DEVICE_NAME " Read  : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

		if (memcmp(rbuf, &bin_data[addr], ISC_PAGE_SIZE)) {
			input_err(true, &ts->client->dev, "%s [ERROR] Verify failed : addr[0x%04X]\n", __func__, addr);
			ret = FW_ERR_DOWNLOAD;
			goto error_update;
		}

		/* Next address */
		addr -= ISC_PAGE_SIZE;
	}

	/* Exit ISC mode */
	input_dbg(false, &ts->client->dev, "%s - Exit ISC mode\n", __func__);
	isc_exit(ts);

	sec_delay(POWER_ON_DELAY);

	/* Check chip firmware version */
	if (melfas_ts_get_fw_version(ts, ver_chip)) {
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto error_update;
	}

	kfree(bin_data);

	if (!on_probe)
		ts->plat_data->init(ts);

	input_dbg(false, &ts->client->dev, "%s [DONE]\n", __func__);
	goto exit;

error_update:
	kfree(bin_data);
err_mem_alloc:
error_file:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
exit:
	return ret;
}


/*
 * Get version of firmware binary file
 */
int melfas_ts_bin_fw_version(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct melfas_bin_tail *bin_ts;
	u16 tail_size = 0;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;
	int i = 0;

	input_dbg(false, &ts->client->dev, "%s [START]\n", __func__);

	/* Check tail size */
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		input_err(true, &ts->client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		goto error;
	}

	/* Check bin format */
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		input_err(true, &ts->client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		goto error;
	}

	/* Read bin info */
	bin_ts = (struct melfas_bin_tail *) &fw_data[fw_size - tail_size];

	/* F/W version */
	for (i = 0; i < 8; i++)
		ver_buf[i] = bin_ts->version[i];

	ts->plat_data->img_version_of_bin[0] = ver_buf[4];
	ts->plat_data->img_version_of_bin[1] = ver_buf[5];
	ts->plat_data->img_version_of_bin[2] = ver_buf[6];
	ts->plat_data->img_version_of_bin[3] = ver_buf[7];

	input_info(true, &ts->client->dev,
			"%s: boot:%x.%x core:%x.%x %x.%x version:%x.%x\n",
			__func__, ver_buf[0], ver_buf[1], ver_buf[2], ver_buf[3], ver_buf[4],
			ver_buf[5], ver_buf[6], ver_buf[7]);

	return 0;

error:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}


int melfas_ts_check_firmware_version(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size)
{
	int i;
	int ret;
	u8 rbuf_ic[8] = {0, };
	u8 rbuf_bin[8] = {0, };
	/*
	 * melfas_ts_check_firmware_version
	 * return value = 1 : firmware download needed,
	 * return value = 0 : skip firmware download
	 */

	melfas_ts_bin_fw_version(ts, fw_data, fw_size, rbuf_bin);
	ret = melfas_ts_get_fw_version(ts, rbuf_ic);
	if (ret > 0) {
		input_err(true, &ts->client->dev, "%s: fail to read ic version\n", __func__);
		return -EIO;
	}

	if (ts->plat_data->bringup == 3) { 		
		input_err(true, &ts->client->dev, "%s: bringup. force update\n", __func__);
		return 1;
	}

	if (rbuf_ic[5] == 0xFF || rbuf_ic[6] == 0xFF) {
		input_err(true, &ts->client->dev, "%s: crc error\n", __func__);
		return 1;
	}

	/* ver[0][1] : boot version
	 * ver[2][3] : core version
	 * ver[4] : IC version
	 * ver[5] : project version
	 * ver[6] : panel version
	 * ver[7] : release version			 
	 */
	for (i = 4; i < 7; i++) {
		if (rbuf_bin[i] != rbuf_ic[i]) {
			if (i == 6) { 
				input_err(true, &ts->client->dev, "%s: panel version not force update.\n", __func__);
				return 0;
			}
			return 1;
		}
	}

	if (rbuf_ic[7] < rbuf_bin[7])
		return 1;

	return 0;
}

int melfas_ts_firmware_update_on_probe(struct melfas_ts_data *ts, bool force_update, bool on_probe)

{
	const char *fw_name = ts->plat_data->firmware_name;
	const struct firmware *fw;
	int ret;
	int retry = 3;

	if (ts->plat_data->bringup == 1) {
		input_err(true, &ts->client->dev, "%s: bringup. do not update\n", __func__);
		return 0; 
	}

	if (!fw_name) {
		input_err(true, &ts->client->dev, "%s fw_name does not exist\n", __func__);
		return -ENOENT;
	}

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	/* Get firmware */
	if (request_firmware(&fw, fw_name, &ts->client->dev) != 0) {
		input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
		return -EIO;
	}

	input_info(true, &ts->client->dev, "%s: request firmware done! size = %d\n", __func__, (int)fw->size);

	mutex_lock(&ts->device_mutex);
	disable_irq(ts->client->irq);
	melfas_ts_unlocked_release_all_finger(ts);

	ret = melfas_ts_check_firmware_version(ts, fw->data, fw->size);
	if ((ret <= 0) && (!force_update)) {
		input_info(true, &ts->client->dev, "%s: skip - fw update\n", __func__);
	} else {
		do {
			ret = melfas_ts_flash_fw(ts, fw->data, fw->size, on_probe);
			if (ret >= FW_ERR_NONE)
				break;
		} while (--retry);

		if (!retry) {
			input_err(true, &ts->client->dev, "%s mms_flash_fw failed\n", __func__);
			ret = -EIO;
		}
	}
	release_firmware(fw);

	/* Enable IRQ */
	enable_irq(ts->client->irq);
	mutex_unlock(&ts->device_mutex);

	input_info(true, &ts->client->dev, "%s %d\n", __func__, ret);
	return ret;
}

static int melfas_ts_load_fw(struct melfas_ts_data *ts, int update_type)
{
	int error = 0;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool is_fw_signed = false;
#ifdef SUPPORT_FW_SIGNED
	long spu_ret = 0;
	long ori_size = 0;
#endif
	struct melfas_bin_tail *bin_ts;
	u16 tail_size = 0;
	u8 ver_buf[8] = {0, };
	u8 rbuf_ic[8] = {0, };
	int i;

	if (ts->client->irq)
		disable_irq(ts->client->irq);

	switch (update_type) {
	case TSP_SDCARD:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW);
#else
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW_SIGNED);
		is_fw_signed = true;
#endif
		break;
	case TSP_SPU:
	case TSP_VERIFICATION:
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_SPU_FW_SIGNED);
		is_fw_signed = true;
		break;
	default:
		goto err_firmware_path;
	}

	error = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (error) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d path:%s\n", __func__, error, fw_path);
		goto err_request_fw;
	}
	input_info(true, &ts->client->dev, "%s: request firmware done! size = %d\n", __func__, (int)fw_entry->size);

	/* Check tail size */
	tail_size = (fw_entry->data[fw_entry->size - 5] << 8) | fw_entry->data[fw_entry->size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		input_err(true, &ts->client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		goto done;
	}

	bin_ts = (struct melfas_bin_tail *) &fw_entry->data[fw_entry->size - tail_size];

	/* F/W version */
	for (i = 0; i < 8; i++)
		ver_buf[i] = bin_ts->version[i];

	input_info(true, &ts->client->dev, "%s: firmware version %02X, parameter version %02X\n", __func__, ver_buf[6], ver_buf[7]);
	melfas_ts_get_fw_version(ts, rbuf_ic);

#ifdef SUPPORT_FW_SIGNED
	/* If SPU firmware version is lower than IC's version, do not run update routine */
	if (update_type == TSP_VERIFICATION) {
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
		}
		release_firmware(fw_entry);
		goto err_request_fw;

	} else if (is_fw_signed) {
		/* digest 32, signature 512 TSP 3 */
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		if ((update_type == TSP_SPU) && (ver_buf[4] == rbuf_ic[4]) && (ver_buf[5] == rbuf_ic[5])
					&& (ver_buf[6] == rbuf_ic[6])) {

			if (ver_buf[7] <= rbuf_ic[7]) {
				input_info(true, &ts->client->dev, "%s: ic version: %02X%02X%02X%02X/ bin:%02X%02X%02X%02X\n",
					__func__, rbuf_ic[4], rbuf_ic[5], rbuf_ic[6], rbuf_ic[7],
					ver_buf[4], ver_buf[5],	ver_buf[6], ver_buf[7]);
				error = 0;
				input_info(true, &ts->client->dev, "%s: skip spu\n", __func__);
				goto done;
			} else {
				input_info(true, &ts->client->dev, "%s: run spu\n", __func__);
			}
		} else if ((update_type == TSP_SDCARD) && (ver_buf[4] == rbuf_ic[4]) && (ver_buf[5] == rbuf_ic[5])) {
			input_info(true, &ts->client->dev, "%s: run sfu\n", __func__);
		} else {
			input_info(true, &ts->client->dev, "%s: not matched product version\n", __func__);
			error = -ENOENT;
			goto done;
		}
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
			goto done;
		}
	}

#endif
	error = melfas_ts_flash_fw(ts, fw_entry->data, fw_entry->size, 0);
	if (error < 0)
		goto done;

	melfas_ts_get_fw_version(ts, rbuf_ic);

done:
	if (error < 0)
		input_err(true, &ts->client->dev, "%s: failed update firmware\n", __func__);

	release_firmware(fw_entry);
err_request_fw:
err_firmware_path:
	if (ts->client->irq)
		enable_irq(ts->client->irq);
	return error;
}

int melfas_ts_firmware_update_on_hidden_menu(struct melfas_ts_data *ts, int update_type)
{
	int ret = SEC_ERROR;

	switch (update_type) {
	case TSP_BUILT_IN:
		ret = melfas_ts_firmware_update_on_probe(ts, true, false);
		break;
	case TSP_SDCARD:
	case TSP_SPU:
	case TSP_VERIFICATION:
		ret = melfas_ts_load_fw(ts, update_type);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(melfas_ts_firmware_update_on_hidden_menu);

MODULE_LICENSE("GPL");
