/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2018 MELFAS Inc.
 *
 *
 * mip4_fw_mfs10.c : Firmware update functions for MFS10
 *
 * Version : 2017.11.25
 */

#include "mip4_ts.h"

//Firmware Info
#define FW_CHIP_CODE	"MF10"
#define FW_TYPE_TAIL

//ISC Info
#define ISC_PAGE_SIZE			128

//ISC Command
#define ISC_CMD_ERASE_PAGE		{0xFB, 0x4A, 0x00, 0x8F, 0x00, 0x00}
#define ISC_CMD_READ_PAGE		{0xFB, 0x4A, 0x00, 0xC2, 0x00, 0x00}
#define ISC_CMD_WRITE_PAGE		{0xFB, 0x4A, 0x00, 0xA5, 0x00, 0x00}
#define ISC_CMD_READ_CAL_DATA	{0xFB, 0xB7, 0xC0, 0xC2, 0x03, 0x80}
#define ISC_CMD_READ_STATUS	{0xFB, 0x4A, 0x36, 0xC2, 0x00, 0x00}
#define ISC_CMD_EXIT				{0xFB, 0x4A, 0x00, 0x66, 0x00, 0x00}

//ISC Status
#define ISC_STATUS_BUSY			0x96
#define ISC_STATUS_DONE			0xAD

//Calibration table
#define CAL_TABLE_PATH			"/sdcard/mfs10_cal.bin"

/*
* Firmware binary tail info
*/
struct melfas_bin_tail {
	u8 tail_mark[4];
	char chip_name[4];
	u32 bin_start_addr;
	u32 bin_length;

	u16 ver_boot;
	u16 ver_core;
	u16 ver_app;
	u16 ver_param;
	u8 boot_start;
	u8 boot_end;
	u8 core_start;
	u8 core_end;
	u8 app_start;
	u8 app_end;
	u8 param_start;
	u8 param_end;

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

#define MELFAS_BIN_TAIL_MARK	{0x4D, 0x42, 0x54, 0x01}	// M B T 0x01
#define MELFAS_BIN_TAIL_SIZE		64

/*
* Read ISC status
*/
static int isc_read_status(struct mip4_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[6] = ISC_CMD_READ_STATUS;
	u8 result = 0;
	int cnt = 200;
	int ret = 0;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = cmd,
			.len = 6,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = &result,
			.len = 1,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	do {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
			ret = -1;
			goto ERROR;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			usleep_range(50, 200);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] wrong value [0x%02X]\n", __func__, result);
			ret = -1;
			usleep_range(50, 200);
		}
	} while (--cnt);

	if (!cnt) {
		dev_err(&info->client->dev, "%s [ERROR] count overflow - cnt [%d] status [0x%02X]\n", __func__, cnt, result);
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/*
* Command : Erase Page
*/
static int isc_erase_page(struct mip4_ts_info *info, int offset)
{
	u8 write_buf[6] = ISC_CMD_ERASE_PAGE;
	struct i2c_msg msg[1] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 6,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)((offset >> 8) & 0xFF);
	write_buf[5] = (u8)(offset & 0xFF);
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	if (isc_read_status(info) != 0) {
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Command : Read Page
*/
static int isc_read_page(struct mip4_ts_info *info, int offset, u8 *data)
{
	u8 write_buf[6] =ISC_CMD_READ_PAGE;
	struct i2c_msg msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 6,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = ISC_PAGE_SIZE,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)((offset >> 8) & 0xFF);
	write_buf[5] = (u8)(offset & 0xFF);
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Command : Write Page
*/
static int isc_write_page(struct mip4_ts_info *info, int offset, const u8 *data, int length)
{
	u8 write_buf[6 + ISC_PAGE_SIZE] = ISC_CMD_WRITE_PAGE;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (length > ISC_PAGE_SIZE) {
		dev_err(&info->client->dev, "%s [ERROR] page length overflow\n", __func__);
		goto ERROR;
	}

	write_buf[4] = (u8)((offset >> 8) & 0xFF);
	write_buf[5] = (u8)(offset & 0xFF);

	memcpy(&write_buf[6], data, length);

	if (i2c_master_send(info->client, write_buf, (length + 6)) != (length + 6)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	if (isc_read_status(info) != 0) {
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE] - Offset[0x%04X] Length[%d]\n", __func__, offset, length);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Command : Read calibration data
*/
static int isc_read_cal_data(struct mip4_ts_info *info, u8 *data)
{
	u8 write_buf[6] =ISC_CMD_READ_CAL_DATA;
	struct i2c_msg msg[2] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 6,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = data,
			.len = ISC_PAGE_SIZE,
		},
	};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Command : Exit ISC
*/
static int isc_exit(struct mip4_ts_info *info)
{
	u8 write_buf[6] = ISC_CMD_EXIT;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (i2c_master_send(info->client, write_buf, 6 ) != 6) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Verify calibration table
*/
static bool verify_cal_table(struct mip4_ts_info *info, u8 *bin)
{
	bool ret = true;
	int i, k;
	u16 crc = 0;
	u16 crc_bin = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	for (k = 0; k < (128 - 3); k++) {
		crc ^= bin[k];

		for (i = 0; i < 8; i++) {
		    if ((crc & 0x1) == 0x1) {
		        crc = (crc >> 1) ^ 0xa001;
		    } else {
		        crc = crc >> 1;
		    }
		}
	}

	crc_bin = (bin[128 - 1] << 8) | bin[128 - 2];
	if (crc != crc_bin) {
		ret = false;
	}

	dev_dbg(&info->client->dev, "%s - CRC : Binary[0x%04X] Verify[0x%04X]\n", __func__, crc_bin, crc);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

/*
* Read calibration table
*/
static int read_cal_table(struct mip4_ts_info *info, u8 *data, char *file_path)
{
	struct file *fp; 
	mm_segment_t old_fs;
	size_t size;
	unsigned char *file_data;
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);	

	fp = filp_open(file_path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, file_path);
		goto ERROR;
	}

 	size = fp->f_path.dentry->d_inode->i_size;
	if (size == 128) {
		file_data = kzalloc(size, GFP_KERNEL);
		ret = vfs_read(fp, (char __user *)file_data, size, &fp->f_pos);
		dev_dbg(&info->client->dev, "%s - path[%s] size[%zu]\n", __func__, file_path, size);
		
		if (ret != size) {
			dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%zu] read[%d]\n", __func__, size, ret);
			goto ERROR;
		} else {
			memcpy(data, file_data, 128);
		}
		
		kfree(file_data);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] size mismatch [%zu]\n", __func__, size);
		goto ERROR;
	}

	filp_close(fp, current->files);
	
	set_fs(old_fs); 
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	set_fs(old_fs); 

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/*
* Flash calibration table
*/
int mip4_ts_flash_cal_table(struct mip4_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 rbuf[ISC_PAGE_SIZE];
	int offset = 0;
	int flash_size = 64 * 1024;
	int cal_data_size = 128;
	int cal_table_size = 128;
	u8 cal_table[128];

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Read calibration table
	if (read_cal_table(info, cal_table, CAL_TABLE_PATH)) {
		dev_err(&client->dev, "%s [ERROR] read_cal_table\n", __func__);
		ret = FW_ERR_FILE_TYPE;
		goto ERROR_FILE;
	}
#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Cal Table : ", DUMP_PREFIX_OFFSET, 16, 1, cal_table, cal_table_size, false);
#endif

	//Verify calibration table
	if (verify_cal_table(info, cal_table) == false) {
		dev_err(&client->dev, "%s [ERROR] verify_cal_table\n", __func__);
		ret = FW_ERR_FILE_TYPE;
		goto ERROR_FILE;
	}

	//Enter ISC mode
	dev_dbg(&client->dev,"%s - Enter ISC mode\n", __func__);
	ret = isc_read_status(info);
	if (ret != 0) {
		dev_err(&client->dev,"%s [ERROR] isc_read_status\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}

	offset = flash_size - cal_data_size - cal_table_size;	
	dev_dbg(&client->dev, "%s - Offset[0x%04X]\n", __func__, offset);

	//Erase page
	if (isc_erase_page(info, offset)) {
		dev_err(&client->dev, "%s [ERROR] isc_erase_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_erase_page : offset[0x%04X]\n", __func__, offset);

	//Write page
	if (isc_write_page(info, offset, &cal_table[0], ISC_PAGE_SIZE)) {
		dev_err(&client->dev, "%s [ERROR] isc_write_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_write_page : offset[0x%04X]\n", __func__, offset);

	//Verify page
	if (isc_read_page(info, offset, rbuf)) {
		dev_err(&client->dev, "%s [ERROR] isc_read_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_read_page : offset[0x%04X]\n", __func__, offset);

#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME "Data : ", DUMP_PREFIX_OFFSET, 16, 1, &cal_table[0], ISC_PAGE_SIZE, false);
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME "Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

	if (memcmp(rbuf, &cal_table[0], ISC_PAGE_SIZE)) {
		dev_err(&client->dev, "%s [ERROR] Verify failed : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}

	//Exit ISC mode
	dev_dbg(&client->dev, "%s - Exit\n", __func__);
	isc_exit(info);

	//Reset chip
	mip4_ts_restart(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	goto EXIT;

ERROR_UPDATE:
	//Reset chip
	mip4_ts_restart(info);

ERROR_FILE:
	dev_err(&client->dev, "%s [ERROR]\n", __func__);

EXIT:
	return ret;
}

/*
* Flash chip firmware (main function)
*/
int mip4_ts_flash_fw(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section)
{
	struct i2c_client *client = info->client;
	struct melfas_bin_tail *bin_info;
	int ret = 0;
	int retry = 3;
	u8 rbuf[ISC_PAGE_SIZE];
	int offset = 0;
	int offset_start = 0;
	int bin_size = 0;
	int flash_size = 64 * 1024;
	int cal_data_size = 128;
	int cal_table_size = 128;
	u8 cal_data[128];
	u8 *image_data;
	int image_size = 0;
	u16 tail_size = 0;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;
	u16 ver_chip[FW_MAX_SECT_NUM];

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Check tail size
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		dev_err(&client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		ret = FW_ERR_FILE_TYPE;
		goto ERROR_FILE;
	}

	//Check bin format
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		dev_err(&client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		ret = FW_ERR_FILE_TYPE;
		goto ERROR_FILE;
	}

	//Read bin info
	bin_info = (struct melfas_bin_tail *)&fw_data[fw_size - tail_size];
	dev_dbg(&client->dev, "%s - bin_info : bin_len[%d] hw_cat[0x%2X] date[%4X] time[%4X] tail_size[%d]\n", __func__, bin_info->bin_length, bin_info->hw_category, bin_info->build_date, bin_info->build_time, bin_info->tail_size);

#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Bin Info : ", DUMP_PREFIX_OFFSET, 16, 1, bin_info, tail_size, false);
#endif

	//Check chip code
	if (memcmp(bin_info->chip_name, FW_CHIP_CODE, 4)) {
		dev_err(&client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);
		ret = FW_ERR_FILE_TYPE;
		goto ERROR_FILE;
	}

	//Check F/W version
	dev_info(&client->dev, "%s - F/W file version [0x%04X 0x%04X 0x%04X 0x%04X]\n", __func__, bin_info->ver_boot, bin_info->ver_core, bin_info->ver_app, bin_info->ver_param);

	if (force == true) {
		//Force update
		dev_info(&client->dev, "%s - Skip chip firmware version check\n", __func__);
	} else {
		//Read firmware version from chip
		while (retry--) {
			if (!mip4_ts_get_fw_version_u16(info, ver_chip)) {
				break;
 			} else {
				mip4_ts_reset(info);
			}
		}

		if (retry < 0) {
			dev_err(&client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
			offset_start = 0;
		} else {
			dev_info(&client->dev, "%s - Chip firmware version [0x%04X 0x%04X 0x%04X 0x%04X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3]);

			//Compare version
			if ((ver_chip[0] == bin_info->ver_boot) && (ver_chip[1] == bin_info->ver_core) && (ver_chip[2] == bin_info->ver_app) && (ver_chip[3] == bin_info->ver_param)) {
				dev_info(&client->dev, "%s - Chip firmware is already up-to-date\n", __func__);
				ret = FW_ERR_UPTODATE;
				goto UPTODATE;
			} else {
				offset_start = 0;
			}
		}
	}

	//Read bin data
	bin_size = bin_info->bin_length;
	image_size = flash_size - cal_table_size - cal_data_size;
	image_data = kzalloc((sizeof(u8) * image_size), GFP_KERNEL);
	memset(image_data, 0xFF, image_size);
	memcpy(image_data, fw_data, bin_size);

	//Enter ISC mode
	dev_dbg(&client->dev,"%s - Enter ISC mode\n", __func__);
	ret = isc_read_status(info);
	if (ret != 0) {
		dev_err(&client->dev,"%s [ERROR] isc_read_status\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}

	//Erase first page
	dev_dbg(&client->dev, "%s - Erase first page : Offset[0x%04X]\n", __func__, offset_start);
	ret = isc_erase_page(info, offset_start);
	if (ret != 0) {
		dev_err(&client->dev,"%s [ERROR] isc_erase_page\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}

	//Firmware - Download & Verify
	dev_dbg(&client->dev, "%s - Firmware\n", __func__);
	dev_dbg(&client->dev, "%s - Download & Verify\n", __func__);
	offset = image_size - ISC_PAGE_SIZE;
	while (offset >= offset_start) {
		//Erase page
		if (isc_erase_page(info, offset)) {
			dev_err(&client->dev, "%s [ERROR] isc_erase_page : offset[0x%04X]\n", __func__, offset);
			ret = FW_ERR_DOWNLOAD;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - isc_erase_page : offset[0x%04X]\n", __func__, offset);

		//Write page
		if (isc_write_page(info, offset, &image_data[offset], ISC_PAGE_SIZE)) {
			dev_err(&client->dev, "%s [ERROR] isc_write_page : offset[0x%04X]\n", __func__, offset);
			ret = FW_ERR_DOWNLOAD;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - isc_write_page : offset[0x%04X]\n", __func__, offset);

		//Verify page
		if (isc_read_page(info, offset, rbuf)) {
			dev_err(&client->dev, "%s [ERROR] isc_read_page : offset[0x%04X]\n", __func__, offset);
			ret = FW_ERR_DOWNLOAD;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - isc_read_page : offset[0x%04X]\n", __func__, offset);

#if FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " F/W File : ", DUMP_PREFIX_OFFSET, 16, 1, &image_data[offset], ISC_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " F/W Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

		if (memcmp(rbuf, &image_data[offset], ISC_PAGE_SIZE)) {
			dev_err(&client->dev, "%s [ERROR] Verify failed : offset[0x%04X]\n", __func__, offset);
			ret = FW_ERR_DOWNLOAD;
			goto ERROR_UPDATE;
		}

		offset -= ISC_PAGE_SIZE;
	}

	//Calibration data
	dev_dbg(&client->dev, "%s - Calibration data\n", __func__);

	//Read calibration data
	dev_dbg(&client->dev, "%s - Read\n", __func__);
	if (isc_read_cal_data(info, cal_data)) {
		dev_err(&client->dev, "%s [ERROR] isc_read_cal_data\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Cal Data : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, cal_data_size, false);
#endif

	//Write calibration data
	dev_dbg(&client->dev, "%s - Download & Verify\n", __func__);
	offset = flash_size - cal_data_size;
	dev_dbg(&client->dev, "%s - Offset[0x%04X]\n", __func__, offset);

	//Erase page
	if (isc_erase_page(info, offset)) {
		dev_err(&client->dev, "%s [ERROR] isc_erase_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_erase_page : offset[0x%04X]\n", __func__, offset);

	//Write page
	if (isc_write_page(info, offset, &cal_data[0], ISC_PAGE_SIZE)) {
		dev_err(&client->dev, "%s [ERROR] isc_write_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_write_page : offset[0x%04X]\n", __func__, offset);

	//Verify page
	if (isc_read_page(info, offset, rbuf)) {
		dev_err(&client->dev, "%s [ERROR] isc_read_page : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}
	dev_dbg(&client->dev, "%s - isc_read_page : offset[0x%04X]\n", __func__, offset);

#if FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME "Data : ", DUMP_PREFIX_OFFSET, 16, 1, &cal_data[0], ISC_PAGE_SIZE, false);
	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME "Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

	if (memcmp(rbuf, &cal_data[0], ISC_PAGE_SIZE)) {
		dev_err(&client->dev, "%s [ERROR] Verify failed : offset[0x%04X]\n", __func__, offset);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	}

	//Exit ISC mode
	dev_dbg(&client->dev, "%s - Exit\n", __func__);
	isc_exit(info);

	//Reset chip
	mip4_ts_restart(info);

	//Check chip firmware version
	if (mip4_ts_get_fw_version_u16(info, ver_chip)) {
		dev_err(&client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = FW_ERR_DOWNLOAD;
		goto ERROR_UPDATE;
	} else {
		if ((ver_chip[0] == bin_info->ver_boot) && (ver_chip[1] == bin_info->ver_core) && (ver_chip[2] == bin_info->ver_app) && (ver_chip[3] == bin_info->ver_param)) {
			dev_dbg(&client->dev, "%s - Version check OK\n", __func__);
		} else {
			dev_err(&client->dev, "%s [ERROR] Version mismatch after flash. Chip[0x%04X 0x%04X 0x%04X 0x%04X] File[0x%04X 0x%04X 0x%04X 0x%04X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3], bin_info->ver_boot, bin_info->ver_core, bin_info->ver_app, bin_info->ver_param);
			ret = FW_ERR_DOWNLOAD;
			goto ERROR_UPDATE;
		}
	}

UPTODATE:
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	goto EXIT;

ERROR_UPDATE:
	//Reset chip
	mip4_ts_restart(info);

ERROR_FILE:
	dev_err(&client->dev, "%s [ERROR]\n", __func__);

EXIT:
	return ret;
}

/*
* Get version of F/W bin file
*/
int mip4_ts_bin_fw_version(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct melfas_bin_tail *bin_info;
	u16 tail_size = 0;
	u8 tail_mark[4] = MELFAS_BIN_TAIL_MARK;

	dev_dbg(&info->client->dev,"%s [START]\n", __func__);

	//Check tail size
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MELFAS_BIN_TAIL_SIZE) {
		dev_err(&info->client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		goto ERROR;
	}

	//Check bin format
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		dev_err(&info->client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		goto ERROR;
	}

	//Read bin info
	bin_info = (struct melfas_bin_tail *)&fw_data[fw_size - tail_size];

	//F/W version
	ver_buf[0] = (bin_info->ver_boot >> 8) & 0xFF;
	ver_buf[1] = (bin_info->ver_boot) & 0xFF;
	ver_buf[2] = (bin_info->ver_core >> 8) & 0xFF;
	ver_buf[3] = (bin_info->ver_core) & 0xFF;
	ver_buf[4] = (bin_info->ver_app >> 8) & 0xFF;
	ver_buf[5] = (bin_info->ver_app) & 0xFF;
	ver_buf[6] = (bin_info->ver_param >> 8) & 0xFF;
	ver_buf[7] = (bin_info->ver_param) & 0xFF;

	dev_dbg(&info->client->dev,"%s [DONE]\n", __func__);
	return 0;

ERROR:
	dev_err(&info->client->dev,"%s [ERROR]\n", __func__);
	return 1;
}

