/*
 * MELFAS MMS500 Touchscreen
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * Firmware update functions for MMS500 (MFSB)
 *
 */

#include "melfas_mms500.h"

//ISC Info
#define ISC_PAGE_SIZE 128

//ISC Command
#define ISC_CMD_ERASE_MASS {0xFB,0x4A,0x00,0x15,0x00,0x00}
#define ISC_CMD_ERASE_PAGE {0xFB,0x4A,0x00,0x8F,0x00,0x00}
#define ISC_CMD_READ_PAGE {0xFB,0x4A,0x00,0xC2,0x00,0x00}
#define ISC_CMD_WRITE_PAGE {0xFB,0x4A,0x00,0xA5,0x00,0x00}
#define ISC_CMD_READ_CAL_DATA {0xFB,0xB7,0xC0,0xC2,0x03,0x80}
#define ISC_CMD_READ_STATUS {0xFB,0x4A,0x36,0xC2,0x00,0x00}
#define ISC_CMD_EXIT {0xFB,0x4A,0x00,0x66,0x00,0x00}

//ISC Status
#define ISC_STATUS_BUSY 0x96
#define ISC_STATUS_DONE 0xAD

/**
 * Firmware binary header info
 */
struct melfas_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;
	u32	reserved1;
} __attribute__ ((packed));

/**
 * Firmware image info
 */
struct melfas_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;
} __attribute__ ((packed));

/**
 * Read ISC status
 */
static int mms_isc_read_status(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[6] =  ISC_CMD_READ_STATUS;
	u8 result = 0;
	int cnt = 100;
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

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	do {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))!=ARRAY_SIZE(msg)) {
			input_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
			return -1;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			usleep_range(200, 300);
		} else {
			input_err(true, &info->client->dev, "%s [ERROR] wrong value [0x%02X]\n",
				__func__, result);
			ret = -1;
			usleep_range(200, 300);
		}
	} while (--cnt);

	if (!cnt) {
		input_err(true, &info->client->dev,
			"%s [ERROR] count overflow - cnt [%d] status [0x%02X]\n",
			__func__, cnt, result);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return ret;

ERROR:
	return ret;
}

/**
 * Command : Erase Mass
 */
static int mms_isc_erase_mass(struct mms_ts_info *info)
{
	u8 write_buf[6] = ISC_CMD_ERASE_MASS;

	struct i2c_msg msg[1] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = 6,
		},
	};

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		input_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	if (mms_isc_read_status(info) != 0) {
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return 0;

ERROR:
	return -1;
}

/*
* Command : Erase Page
*/
static int __maybe_unused mms_isc_erase_page(struct mms_ts_info *info, int offset)
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

    if (mms_isc_read_status(info) != 0) {
        goto ERROR;
    }

    dev_dbg(&info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);
    return 0;

ERROR:
    dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
    return -1;
}

/**
 * Command : Read Page
 */
static int mms_isc_read_page(struct mms_ts_info *info, int offset, u8 *data)
{
	u8 write_buf[6] = ISC_CMD_READ_PAGE;

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

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)((offset >> 8) & 0xFF);
	write_buf[5] = (u8)(offset & 0xFF);
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		input_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);

	return 0;

ERROR:
	return -1;
}

/**
 * Command : Write Page
 */
static int mms_isc_write_page(struct mms_ts_info *info, int offset,
				const u8 *data, int length)
{
	u8 write_buf[6 + ISC_PAGE_SIZE] = ISC_CMD_WRITE_PAGE;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (length > ISC_PAGE_SIZE) {
		input_err(true, &info->client->dev, "%s [ERROR] page length overflow\n", __func__);
		goto ERROR;
	}

	write_buf[4] = (u8)((offset >> 8) & 0xFF);
	write_buf[5] = (u8)(offset & 0xFF);

	memcpy(&write_buf[6], data, length);

	if (i2c_master_send(info->client, write_buf, (6 + length)) != (6 + length)) {
		input_err(true, &info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	if (mms_isc_read_status(info) != 0) {
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE] - Offset[0x%04X] Length[%d]\n",
		__func__, offset, length);

	return 0;

ERROR:
	return -1;
}

/**
* Command : Read calibration data
*/
static int mms_isc_read_cal_data(struct mms_ts_info *info, u8 *data)
{
	u8 write_buf[6] = ISC_CMD_READ_CAL_DATA;
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

/**
 * Command : Exit ISC
 */
static int mms_isc_exit(struct mms_ts_info *info)
{
	u8 write_buf[6] = ISC_CMD_EXIT;

	input_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (i2c_master_send(info->client, write_buf, 6) != 6) {
		input_err(true, &info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	input_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return 0;

ERROR:
	return -1;
}

/*
* Test flash memory
*/
int mms_flash_test(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size)
{
	struct i2c_client *client = info->client;
	struct melfas_bin_hdr *fw_hdr;
	struct melfas_fw_img **img;
	int ret = fw_err_none;
	u8 rbuf[ISC_PAGE_SIZE];
	int offset = 0;
	int offset_start = 0;
	int bin_size = 0;
	int flash_size = 64 * 1024;
	int cal_data_size = 128;
	int cal_data_addr = 64 * 1024 - 128;
	u8 *image_data;
	int i = 0;
	u8 wbuf_cmd[4];
	u8 wbuf_config[6] = {0xFB, 0xB7, 0xDD, 0xC2, 0x00, 0x00};
	u8 rbuf_config[16];
	u8 wbuf_verify[22] = {0xFB, 0xB7, 0xDD, 0xA5, 0x00, 0x00};
	int ret_i2c = 0;
	int i_write = 0;
	int i_verify = 0;
	int i_test = 0;
	int test_count = 200;
	int write_count = 1;
	u8 __maybe_unused xor = 0;
	int __maybe_unused i_xor = 0;

	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = wbuf_config,
			.len = 6,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf_config,
			.len = 16,
		},
	};

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Read firmware file
	fw_hdr = (struct melfas_bin_hdr *)fw_data;
	img = vzalloc(sizeof(*img) * fw_hdr->section_num);

	if (!img) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc img\n", __func__);

		ret = fw_err_file_type;
		goto ERROR_ALLOC;
	}
	
	//Check firmware file
	if (memcmp(CHIP_FW_CODE, &fw_hdr->tag[4], 4)) {
		dev_err(&client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);

		ret = fw_err_file_type;
		goto ERROR_FILE;
	}
	
	//Read bin data
	bin_size = fw_hdr->binary_length;
	image_data = vzalloc(sizeof(u8) * flash_size);

	if (!image_data) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc image_data\n", __func__);

		ret = fw_err_file_type;
		goto ERROR_FILE;
	}
	
	memset(image_data, 0xFF, flash_size);
	memcpy(image_data, fw_data + fw_hdr->binary_offset, fw_hdr->binary_length);

	//Pre-ISC command
	dev_info(&client->dev,"%s - Pre-ISC command\n", __func__);
	wbuf_cmd[0] = MIP_R0_CTRL;
	wbuf_cmd[1] = MIP_R1_CTRL_PREPARE_FLASH;
	wbuf_cmd[2] = 1;
	ret_i2c = i2c_master_send(info->client, wbuf_cmd, 3);
	if (ret_i2c != 3) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send [%d]\n", __func__, ret_i2c);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}
	usleep_range(10 * 1000, 20 * 1000);

	//Enter ISC mode
	dev_info(&client->dev,"%s - Enter ISC mode\n", __func__);
	ret_i2c = mms_isc_read_status(info);
	if (ret_i2c != 0) {
		dev_err(&client->dev,"%s [ERROR] mms_isc_read_status\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}

	//Read calibration data
	if (mms_isc_read_cal_data(info, rbuf)) {
		dev_err(&client->dev, "%s [ERROR] mms_isc_read_cal_data\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Cal Data : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, cal_data_size, false);
#endif

	//Prepare image
	memcpy(&image_data[cal_data_addr], rbuf, cal_data_size);
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Image : ", DUMP_PREFIX_OFFSET, 16, 1, image_data, flash_size, false);
#endif

	//Backup config value
	if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		dev_err(&info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Read config : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf_config, 16, false);

	//Test info
	dev_info(&client->dev, "%s - Test setting : Test[%d] Write[%d]\n", __func__, test_count, write_count);

	//Address range
	dev_info(&client->dev, "%s - Range : Page[1~%d] Addr[0~%d] Bit[0~7]\n", __func__, flash_size / ISC_PAGE_SIZE, flash_size - 1);
	
	//Erase
	dev_info(&client->dev, "%s - Erase\n", __func__);
	ret_i2c = mms_isc_erase_mass(info);
	if (ret_i2c != 0) {
		dev_err(&client->dev,"%s [ERROR] mms_isc_erase_mass\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}

	//Test (repeat)
	for (i_test = 0; i_test < test_count; i_test++) {
		dev_info(&client->dev, "%s - Test [#%d]\n", __func__, i_test + 1);

		//Write (repeat)
		for (i_write = 0; i_write < write_count; i_write++) {
			dev_info(&client->dev, "%s - Write [#%d]\n", __func__, i_write + 1);
			offset_start = 0;
			offset = flash_size - ISC_PAGE_SIZE;
			while (offset >= offset_start) {
				//Write page
				if (mms_isc_write_page(info, offset, &image_data[offset], ISC_PAGE_SIZE)) {
					dev_err(&client->dev, "%s [ERROR] mms_isc_write_page : offset[0x%04X]\n", __func__, offset);
					ret = fw_err_download;
					goto ERROR_UPDATE;
				}
				dev_dbg(&client->dev, "%s - mms_isc_write_page : offset[0x%04X]\n", __func__, offset);

				offset -= ISC_PAGE_SIZE;
			}
		}

		//Verify
		for (i_verify = 1; i_verify <= 2; i_verify++) {
			dev_info(&client->dev, "%s - Verify %d\n", __func__, i_verify);

			//Config
			if (i_verify == 1) {
				wbuf_verify[6 + 0] = 0x98;
				wbuf_verify[6 + 1] = (rbuf_config[1] & 0xF0) | 0x03;	
				for (i = 2; i < 16; i++) {
					wbuf_verify[6 + i] = rbuf_config[i];
				}
			} else if (i_verify == 2) {
				wbuf_verify[6 + 0] = 0x18;
				wbuf_verify[6 + 1] = (rbuf_config[1] & 0xF0) | 0x00;
				for (i = 2; i < 16; i++) {
					wbuf_verify[6 + i] = rbuf_config[i];
				}
			} else {
				break;
			}
			print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Write config : ", DUMP_PREFIX_OFFSET, 22, 1, wbuf_verify, 22, false);

			ret_i2c = i2c_master_send(info->client, wbuf_verify, 22);
			if (ret_i2c != 22) {
				dev_err(&info->client->dev, "%s [ERROR] i2c_master_send [%d]\n", __func__, ret_i2c);
				ret = fw_err_download;
				goto ERROR_UPDATE;
			}
			usleep_range(2000, 3000);

			//Read
			offset_start = 0;
			offset = flash_size - ISC_PAGE_SIZE;
			while (offset >= offset_start) {
				//Read page
				if (mms_isc_read_page(info, offset, rbuf)) {
					dev_err(&client->dev, "%s [ERROR] mms_isc_read_page : offset[0x%04X]\n", __func__, offset);
					ret = fw_err_download;
					goto ERROR_UPDATE;
				}
				dev_dbg(&client->dev, "%s - mms_isc_read_page : offset[0x%04X]\n", __func__, offset);

#if MMS_FW_UPDATE_DEBUG
				print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W File : ", DUMP_PREFIX_OFFSET, 16, 1, &image_data[offset], ISC_PAGE_SIZE, false);
				print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

#if 1
				//Compare page
				if (memcmp(rbuf, &image_data[offset], ISC_PAGE_SIZE)) {
					dev_err(&client->dev, "%s [ERROR] Verify %d failed : Page[%d] PageAddr[0x%04X]\n", __func__, i_verify, offset / ISC_PAGE_SIZE + 1, offset);

					//Page number
					//ret = offset / ISC_PAGE_SIZE + 1;

					//Test number
					ret = i_test + 1;
					dev_err(&client->dev, "%s [ERROR] Flash test failed : Test [#%d]\n", __func__, ret);

					goto EXIT;
				}
#else
				//Compare bit
				for (i = 0; i < ISC_PAGE_SIZE; i++) {
					if (image_data[offset + i] != rbuf[i]) {
						dev_err(&client->dev, "%s [ERROR] Verify %d failed : Write[0x%02X] Read[0x%02X]\n", __func__, i_verify, image_data[offset + i], rbuf[i]);
						
						xor = image_data[offset + i] ^ rbuf[i];
						for (i_xor = 0; i_xor < 8; i_xor++) {
							if (((xor >> i_xor) & 0x01) == 0x01) {
								dev_err(&client->dev, "%s [ERROR] Verify %d failed : Page[%d] Addr[%d] Bit[%d]\n", __func__, i_verify, offset / ISC_PAGE_SIZE + 1, offset + i, i_xor);
								ret = offset / ISC_PAGE_SIZE + 1;
							}
						}
					}
				}
#endif

				offset -= ISC_PAGE_SIZE;
			}
		}

		//Restore config
		for (i = 0; i < 16; i++) {
			wbuf_verify[6 + i] = rbuf_config[i];
		}
		print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Restore config : ", DUMP_PREFIX_OFFSET, 22, 1, wbuf_verify, 22, false);

		ret_i2c = i2c_master_send(info->client, wbuf_verify, 22);
		if (ret_i2c != 22) {
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send [%d]\n", __func__, ret_i2c);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}
		usleep_range(2000, 3000);				
	}

	ret = fw_err_none;

EXIT:
	//Exit ISC mode
	dev_info(&client->dev, "%s - Exit\n", __func__);
	mms_isc_exit(info);

	//Reset chip
	mms_reboot(info);

	vfree(image_data);
	vfree(img);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return ret;

ERROR_UPDATE:
	vfree(image_data);

	//Reset chip
	mms_reboot(info);

ERROR_FILE:
	vfree(img);

ERROR_ALLOC:
	dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/*
* Flash chip firmware (target pages only)
*/
int mip_ts_flash_fw_page(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, int addr, int length)
{
	struct i2c_client *client = info->client;
	struct melfas_bin_hdr *fw_hdr;
	struct melfas_fw_img **img;
	int ret = 0;
	//int retry = 3;
	u8 rbuf[ISC_PAGE_SIZE];
	int offset = 0;
	int offset_start = 0;
	int offset_end = 0;
	int bin_size = 0;
	int flash_size = 64 * 1024;
	int cal_data_size = 128;
	int cal_data_addr = 64 * 1024 - 128;
	u8 *image_data;
	u16 ver_chip[MMS_FW_MAX_SECT_NUM];
	u16 ver_file[MMS_FW_MAX_SECT_NUM];
	bool mass_erase = false;
	int i;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Read firmware file
	fw_hdr = (struct melfas_bin_hdr *)fw_data;
	img = vzalloc(sizeof(*img) * fw_hdr->section_num);

	if (!img) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc img\n", __func__);

		ret = fw_err_file_type;
		goto ERROR_ALLOC;
	}
	
	//Check firmware file
	if (memcmp(CHIP_FW_CODE, &fw_hdr->tag[4], 4)) {
		dev_err(&client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);

		ret = fw_err_file_type;
		goto ERROR_FILE;
	}
	
	//Get address offset
	if (length >= (22 * 1024)) {
		mass_erase = true;
		offset_start = 0;
		offset_end = flash_size;
		dev_dbg(&client->dev, "%s - Mass erase\n", __func__);
	} else {
		mass_erase = false;
		offset_start = addr - (addr % ISC_PAGE_SIZE);
	        if (offset_start < 0) {
        	    offset_start = 0;
        	}
//		offset_end = addr + length - ((addr + length) % ISC_PAGE_SIZE) + ISC_PAGE_SIZE;
		offset_end = addr + length - ((addr + length) % ISC_PAGE_SIZE) + (((addr + length) % ISC_PAGE_SIZE) ? ISC_PAGE_SIZE : 0);

	        if (offset_end > flash_size) {
	            offset_end = flash_size;
	        }
		dev_info(&client->dev, "%s - offset_start[0x%04X] offset_end[0x%04X]\n", __func__, offset_start, offset_end);
	}

	//Prepare flash data
	bin_size = fw_hdr->binary_length;
	image_data = vzalloc(sizeof(u8) * flash_size);

	if (!image_data) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc image_data\n", __func__);

		ret = fw_err_file_type;
		goto ERROR_FILE;
	}
	
	memset(image_data, 0xFF, flash_size);
	memcpy(image_data, fw_data + fw_hdr->binary_offset, fw_hdr->binary_length);

	//Enter ISC mode
	dev_dbg(&client->dev,"%s - Enter ISC mode\n", __func__);
	ret = mms_isc_read_status(info);
	if (ret != 0) {
		dev_err(&client->dev,"%s [ERROR] mms_isc_read_status\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}

	//Read calibration data
	if (mms_isc_read_cal_data(info, rbuf)) {
		dev_err(&client->dev, "%s [ERROR] mms_isc_read_cal_data\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	}
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Cal Data : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, cal_data_size, false);
#endif

	//Prepare image
	memcpy(&image_data[cal_data_addr], rbuf, cal_data_size);
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Image : ", DUMP_PREFIX_OFFSET, 16, 1, image_data, flash_size, false);
#endif

	if (mass_erase == true) {
		//Erase
		ret = mms_isc_erase_mass(info);
		if (ret != 0) {
			dev_err(&client->dev,"%s [ERROR] mms_isc_erase_mass\n", __func__);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - mms_isc_erase_mass\n", __func__);
	}

	//Download & Verify
	offset = offset_end - ISC_PAGE_SIZE;
	while (offset >= offset_start) {
		if (mass_erase == false) {
			//Erase page
			ret = mms_isc_erase_page(info, offset);
			if (ret != 0) {
				dev_err(&client->dev,"%s [ERROR] mms_isc_erase_page : offset[0x%04X]\n", __func__, offset);
				ret = fw_err_download;
				goto ERROR_UPDATE;
			}
			dev_dbg(&client->dev, "%s - mms_isc_erase_page : offset[0x%04X]\n", __func__, offset);
		}

		//Write page
		if (mms_isc_write_page(info, offset, &image_data[offset], ISC_PAGE_SIZE)) {
			dev_err(&client->dev, "%s [ERROR] mms_isc_write_page : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - mms_isc_write_page : offset[0x%04X]\n", __func__, offset);

		//Verify page
		if (mms_isc_read_page(info, offset, rbuf)) {
			dev_err(&client->dev, "%s [ERROR] mms_isc_read_page : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}
		dev_dbg(&client->dev, "%s - mms_isc_read_page : offset[0x%04X]\n", __func__, offset);

#if MMS_FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W File : ", DUMP_PREFIX_OFFSET, 16, 1, &image_data[offset], ISC_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

		if (memcmp(rbuf, &image_data[offset], ISC_PAGE_SIZE)) {
			dev_err(&client->dev, "%s [ERROR] Verify failed : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}

		offset -= ISC_PAGE_SIZE;
	}
	offset = sizeof(struct melfas_bin_hdr);

	//Exit ISC mode
	dev_dbg(&client->dev, "%s - Exit\n", __func__);
	mms_isc_exit(info);

	//Reset chip
	mms_reboot(info);

	//Check chip firmware version
	if (mms_get_fw_version_u16(info, ver_chip)) {
		dev_err(&client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = fw_err_download;
		goto ERROR_UPDATE;
	} else {
		//Compare version
		dev_dbg(&client->dev, "%s - Firmware file info : Sections[%d] Offset[0x%08X] Length[0x%08X]\n", __func__, fw_hdr->section_num, fw_hdr->binary_offset, fw_hdr->binary_length);

		for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct melfas_fw_img)) {
			img[i] = (struct melfas_fw_img *)(fw_data + offset);
			ver_file[i] = img[i]->version;

			dev_dbg(&client->dev, "%s - Section info : Section[%d] Version[0x%04X] StartPage[%d]"" EndPage[%d] Offset[0x%08X] Length[0x%08X]\n", __func__, i, img[i]->version, img[i]->start_page, img[i]->end_page,img[i]->offset, img[i]->length);

			dev_info(&client->dev, "%s - Section[%d] IC: %04X / BIN: %04X\n", __func__, i, ver_chip[i], ver_file[i]);

		}
		
		if ((ver_chip[0] == ver_file[0]) && (ver_chip[1] == ver_file[1]) && (ver_chip[2] == ver_file[2]) && (ver_chip[3] == ver_file[3])) {
			dev_dbg(&client->dev, "%s - Version check OK\n", __func__);
		} else {
			dev_err(&client->dev, "%s [ERROR] Version mismatch after flash. Chip[0x%04X 0x%04X 0x%04X 0x%04X] File[0x%04X 0x%04X 0x%04X 0x%04X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3], ver_file[0], ver_file[1], ver_file[2], ver_file[3]);
			ret = fw_err_download;
			goto ERROR_UPDATE;
		}
	}

	vfree(image_data);
	vfree(img);

	ret = fw_err_none;

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	goto EXIT;

ERROR_UPDATE:
	vfree(image_data);

	//Reset chip
	mms_reboot(info);

ERROR_FILE:
	vfree(img);
	
ERROR_ALLOC:	
	dev_err(&client->dev, "%s [ERROR]\n", __func__);

EXIT:
	return ret;
}

/**
 * Flash chip firmware (main function)
 */
int mms_flash_fw(struct mms_ts_info *info, const u8 *fw_data,
				size_t fw_size, bool force, bool section)
{
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	struct i2c_client *client = info->client;
	int i;
	int retry = 3;
	int ret;
	int nRet = fw_err_download;
	int nOffset;
	u8 rbuf[ISC_PAGE_SIZE];
	int bin_size;
	int cal_data_size = ISC_PAGE_SIZE;
	int cal_data_addr = 64 * 1024 - ISC_PAGE_SIZE;
	int flash_size = 64 * 1024;
	u8 *flash_data;
	
	int offset = sizeof(struct mms_bin_hdr);

	bool update_flag = false;

	u16 ver_chip[MMS_FW_MAX_SECT_NUM];
	u16 ver_file[MMS_FW_MAX_SECT_NUM];

	input_dbg(true, &client->dev, "%s [START]\n", __func__);

	//Read firmware file
	fw_hdr = (struct mms_bin_hdr *)fw_data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	if (!img) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc img\n", __func__);

		ret = fw_err_file_type;
		goto ERROR_ALLOC;
	}

	//Check firmware file
	if (memcmp(CHIP_FW_CODE, &fw_hdr->tag[4], 4)) {
		input_err(true, &client->dev, "%s [ERROR] F/W file is not for %s, reason(%s)\n",
			__func__, CHIP_NAME, fw_hdr->tag[4]);

		nRet = fw_err_file_type;
		goto EXIT;
	}

	//Check chip firmware version
	while (retry--) {
		if (!mms_get_fw_version_u16(info, ver_chip))
			break;
		else
			mms_reboot(info);
	}

	//Set update flag
	if (retry < 0) {
		input_err(true, &client->dev,
			"%s [ERROR] cannot read chip firmware version\n",__func__);

		memset(ver_chip, 0xFFFF, sizeof(ver_chip));
		input_dbg(true, &client->dev,
			"%s - Chip firmware version is set to [0xFFFF]\n", __func__);

		update_flag = true;
	} else {
		input_dbg(true, &client->dev,
			"%s - Chip firmware version [0x%04X 0x%04X 0x%04X 0x%04X]\n",
			__func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3]);
	}

	input_dbg(true, &client->dev,
		"%s - Firmware file info : Sections[%d] Offset[0x%08X] Length[0x%08X]\n",
		__func__, fw_hdr->section_num, fw_hdr->binary_offset, fw_hdr->binary_length);

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw_data + offset);
		ver_file[i] = img[i]->version;

		input_dbg(true, &client->dev,
			"%s - Section info : Section[%d] Version[0x%04X] StartPage[%d]"
			" EndPage[%d] Offset[0x%08X] Length[0x%08X]\n",
			__func__, i, img[i]->version, img[i]->start_page,
			img[i]->end_page,img[i]->offset, img[i]->length);

		input_info(true, &client->dev,
			"%s - Section[%d] IC: %04X / BIN: %04X\n",
			__func__, i, ver_chip[i], ver_file[i]);

		//Compare section version
		if (ver_chip[i] < ver_file[i]) {
			//Set update flag
			update_flag = true;
		}
	}

	if(mms_get_chip_name(info))
		strcpy(info->chip_name, "M5H0");
	else {
		if(!strcmp(info->chip_name, "M4HP")) {
			update_flag = false;

			input_err(true, &client->dev, "%s - this is MMS449 IC, don't update\n", __func__);
		}
	}

	// temp code for blocking TSP FPCB 00 fw update
	if(ver_chip[2] == 0x0199 && ver_chip[3] == 0x0199) {
		update_flag = false;

		input_err(true, &client->dev, "%s - this is MMS549 IC with FPCB 00, don't update\n", __func__);
	}

#if 1
	// flash emptyp IC
	if( ver_chip[0] == 0xFFFF && ver_chip[1] == 0xFFFF && ver_chip[2] == 0xFFFF && ver_chip[3] == 0xFFFF) {
		update_flag = true;
		
		input_err(true, &client->dev, "%s this is empty TSP IC - Force update\n", __func__);
	}
#endif

	//Set force update flag
	if (force == true) {
		update_flag = true;

		input_err(true, &client->dev, "%s - Force update\n", __func__);
	}

	//Exit when up-to-date
	if (update_flag == false) {
		nRet = fw_err_uptodate;
		input_err(true, &client->dev,
			"%s [DONE] Chip firmware is already up-to-date\n", __func__);
		goto EXIT;
	}

	//Prepare flash data
	bin_size = fw_hdr->binary_length;
	flash_data = kzalloc(sizeof(u8) * flash_size, GFP_KERNEL);

	if (!flash_data) {
		dev_err(&client->dev, "%s [ERROR] failed to memory alloc flash_data\n", __func__);

		ret = fw_err_file_type;
		goto EXIT;
	}
	
	memset(flash_data, 0xFF, flash_size);
	memcpy(flash_data, fw_data + fw_hdr->binary_offset, fw_hdr->binary_length);

	//Check firmware size
	if (flash_size % ISC_PAGE_SIZE != 0) {
		nRet = fw_err_file_type;
		input_err(true, &client->dev, "%s [ERROR] Firmware size mismatch\n", __func__);
		goto ERROR;
	}

	//Enter ISC mode
	dev_dbg(&client->dev,"%s - Enter ISC mode\n", __func__);
	ret = mms_isc_read_status(info);
	if (ret != 0) {
		dev_err(&client->dev,"%s [ERROR] mip_isc_read_status\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	//Read calibration data
	if (mms_isc_read_cal_data(info, rbuf)) {
		dev_err(&client->dev, "%s [ERROR] mip_isc_read_cal_data\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, "Cal Data : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, cal_data_size, false);
#endif

	//Save calibration data
	memcpy(&flash_data[cal_data_addr], rbuf, cal_data_size);
#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, "Flash Data : ", DUMP_PREFIX_OFFSET, 16, 1, flash_data, flash_size, false);
#endif

	//Erase
	input_info(true, &client->dev, "%s - Erase mass\n", __func__);
	nRet = mms_isc_erase_mass(info);
	if (nRet != 0) {
		input_err(true, &client->dev, "%s [ERROR] erase failed\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	//Flash firmware
	input_info(true, &client->dev, "%s - Start Download\n", __func__);
	nOffset = flash_size - ISC_PAGE_SIZE;
	while (nOffset >= 0) {
		//Write page
		input_dbg(true, &client->dev, "%s - Write page : Offset[0x%04X]\n", __func__, nOffset);
		nRet = mms_isc_write_page(info, nOffset, &flash_data[nOffset], ISC_PAGE_SIZE);
		if (nRet != 0) {
			input_err(true, &client->dev, "%s [ERROR] mms_isc_program_page\n", __func__);
			ret = fw_err_download;
			goto ERROR;
		}

		//Verify page (read and compare)
		input_dbg(true, &client->dev, "%s - Read page : Offset[0x%04X]\n", __func__, nOffset);
		if (mms_isc_read_page(info, nOffset, rbuf)) {
			input_err(true, &client->dev, "%s [ERROR] mms_isc_read_page\n", __func__);
			ret = fw_err_download;
			goto ERROR;
		}

		if (memcmp(&flash_data[nOffset], rbuf, ISC_PAGE_SIZE)) {
#if MMS_FW_UPDATE_DEBUG
			print_hex_dump(KERN_ERR, "Firmware Page Write : ",
				DUMP_PREFIX_OFFSET, 16, 1, &flash_data[nOffset], ISC_PAGE_SIZE, false);
			print_hex_dump(KERN_ERR, "Firmware Page Read : ",
				DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif
			input_err(true, &client->dev, "%s [ERROR] verify page failed\n", __func__);

			ret = -1;
			goto ERROR;
		}

		nOffset -= ISC_PAGE_SIZE;
	}

	//Exit ISC
	nRet = mms_isc_exit(info);
	if (nRet != 0) {
		input_err(true, &client->dev, "%s [ERROR] mms_isc_exit\n", __func__);
		goto ERROR;
	}

	//Reboot chip
	mms_reboot(info);

	//Check chip firmware version
	if (mms_get_fw_version_u16(info, ver_chip)) {
		input_err(true, &client->dev,
			"%s [ERROR] cannot read chip firmware version after flash\n", __func__);

		nRet = -1;
		goto ERROR;
	} else {
		for (i = 0; i < fw_hdr->section_num; i++) {
			if (ver_chip[i] != ver_file[i]) {
				input_err(true, &client->dev,
					"%s [ERROR] version mismatch after flash."
					" Section[%d] : Chip[0x%04X] != File[0x%04X]\n",
					__func__, i, ver_chip[i], ver_file[i]);

				nRet = -1;
				goto ERROR;
			}
		}
	}

	nRet = 0;
	input_err(true, &client->dev, "%s [DONE]\n", __func__);
	goto DONE;

ERROR:
	input_err(true, &client->dev, "%s [ERROR]\n", __func__);

DONE:
	kfree(flash_data);

EXIT:
	kfree(img);

ERROR_ALLOC:
	return nRet;
}
