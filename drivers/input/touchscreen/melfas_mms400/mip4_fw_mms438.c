/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_fw_mms438.c : Firmware update functions for MMS438/449/458
 *
 *
 * Version : 2015.07.20
 *
 */

#include "melfas_mms400.h"

#if 0
#define FW_TYPE_MFSB
#else
#define FW_TYPE_TAIL
#endif

//ISC Info
#define ISC_PAGE_SIZE				128

//ISC Command
#define ISC_CMD_ERASE_PAGE			{0xFB,0x4A,0x00,0x8F,0x00,0x00}
#define ISC_CMD_READ_PAGE			{0xFB,0x4A,0x00,0xC2,0x00,0x00}
#define ISC_CMD_PROGRAM_PAGE		{0xFB,0x4A,0x00,0x54,0x00,0x00}
#define ISC_CMD_READ_STATUS		{0xFB,0x4A,0x36,0xC2,0x00,0x00}
#define ISC_CMD_EXIT				{0xFB,0x4A,0x00,0x66,0x00,0x00}

//ISC Status
#define ISC_STATUS_BUSY			0x96
#define ISC_STATUS_DONE			0xAD

#ifdef FW_TYPE_MFSB
/**
* Firmware binary header info
*/
struct mip_bin_hdr {
	char tag[8];
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
struct mip_fw_img {
	u16	type;
	u16	version;
	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;
} __attribute__ ((packed));
#endif

#ifdef FW_TYPE_TAIL
/**
* Firmware binary tail info
*/
struct mip_bin_tail {
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

#define MIP_BIN_TAIL_MARK		{0x4D, 0x42, 0x54, 0x01}	// M B T 0x01
#define MIP_BIN_TAIL_SIZE		64
#endif

/**
* Read ISC status
*/
static int mip_isc_read_status(struct mms_ts_info *info)
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

	tsp_debug_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	do {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
			tsp_debug_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
			return -1;
		}

		if (result == ISC_STATUS_DONE) {
			ret = 0;
			break;
		} else if (result == ISC_STATUS_BUSY) {
			ret = -1;
			msleep(1);
		} else {
			tsp_debug_err(true, &info->client->dev, "%s [ERROR] wrong value [0x%02X]\n", __func__, result);
			ret = -1;
			msleep(1);
		}
	} while (--cnt);

	if (!cnt) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] count overflow - cnt [%d] status [0x%02X]\n",
			__func__, cnt, result);
		goto ERROR;
	}

	tsp_debug_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return ret;

ERROR:
	return ret;
}

/**
* Command : Erase Page
*/
static int mip_isc_erase_page(struct mms_ts_info *info, int offset)
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

	tsp_debug_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)(((offset)>>8)&0xFF );
	write_buf[5] = (u8)(((offset)>>0)&0xFF );
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	if (mip_isc_read_status(info) != 0) {
		goto ERROR;
	}

	tsp_debug_dbg(true, &info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);

	return 0;

ERROR:
	return -1;
}

/**
* Command : Read Page
*/
static int mip_isc_read_page(struct mms_ts_info *info, int offset, u8 *data)
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

	tsp_debug_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	write_buf[4] = (u8)(((offset)>>8)&0xFF );
	write_buf[5] = (u8)(((offset)>>0)&0xFF );
	if (i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] i2c_transfer\n", __func__);
		goto ERROR;
	}

	tsp_debug_dbg(true, &info->client->dev, "%s [DONE] - Offset [0x%04X]\n", __func__, offset);

	return 0;

ERROR:
	return -1;
}

/**
* Command : Program Page
*/
static int mip_isc_program_page(struct mms_ts_info *info, int offset, const u8 *data, int length)
{
	u8 write_buf[134] = ISC_CMD_PROGRAM_PAGE;

	tsp_debug_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (length > ISC_PAGE_SIZE) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] page length overflow\n", __func__);
		goto ERROR;
	}

	write_buf[4] = (u8)(((offset)>>8)& 0xFF );
	write_buf[5] = (u8)(((offset)>>0)& 0xFF );

	memcpy(&write_buf[6], data, length);

	if (i2c_master_send(info->client, write_buf, (length + 6)) != (length + 6)) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	if (mip_isc_read_status(info) != 0) {
		goto ERROR;
	}

	tsp_debug_dbg(true, &info->client->dev, "%s [DONE] - Offset[0x%04X] Length[%d]\n",
		__func__, offset, length);

	return 0;

ERROR:
	return -1;
}

/**
* Command : Exit ISC
*/
static int mip_isc_exit(struct mms_ts_info *info)
{
	u8 write_buf[6] = ISC_CMD_EXIT;

	tsp_debug_dbg(true, &info->client->dev, "%s [START]\n", __func__);

	if (i2c_master_send(info->client, write_buf, 6 ) != 6) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] i2c_master_send\n", __func__);
		goto ERROR;
	}

	tsp_debug_dbg(true, &info->client->dev, "%s [DONE]\n", __func__);

	return 0;

ERROR:
	return -1;
}

#ifdef FW_TYPE_MFSB
/**
* Flash chip firmware (main function)
*/
int mms_flash_fw(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section)
{
	struct mip_bin_hdr *fw_hdr;
	struct mip_fw_img **img;
	struct i2c_client *client = info->client;
	int i;
	int retires = 3;
	int nRet;
	int nStartAddr;
	int nWriteLength;
	int nLast;
	int nOffset;
	int nTransferLength;
	int size;
	u8 *data;
	u8 *cpydata;

	int offset = sizeof(struct mip_bin_hdr);

	bool update_flag = false;
	bool update_flags[MMS_FW_MAX_SECT_NUM] = {false, };

	u16 ver_chip[MMS_FW_MAX_SECT_NUM];
	u16 ver_file[MMS_FW_MAX_SECT_NUM];

	int offsetStart = 0;
	u8 initData[ISC_PAGE_SIZE];
	memset(initData, 0xFF, sizeof(initData));

	tsp_debug_dbg(true, &client->dev,"%s [START]\n", __func__);

	//Read firmware file
	fw_hdr = (struct mip_bin_hdr *)fw_data;
	img = vzalloc(sizeof(*img) * fw_hdr->section_num);
	if (!img) {
		tsp_debug_err(true, &client->dev, "Failed to img allocate memory\n");
		nRet = -ENOMEM;
		goto err_alloc_img;
	}

	//Check firmware file
	if (memcmp(FW_CHIP_CODE, &fw_hdr->tag[4], 4)) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);

		nRet = fw_err_file_type;
		goto ERROR;
	}

	//Reboot chip
	mms_reboot(info);

	//Check chip firmware version
	while (retires--) {
		if (!mms_get_fw_version_u16(info, ver_chip))
			break;
		else
			mms_reboot(info);
	}

	if (retires < 0) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] cannot read chip firmware version\n", __func__);
		memset(ver_chip, 0xFFFF, sizeof(ver_chip));
		tsp_debug_info(true, &client->dev, "%s - Chip firmware version is set to [0xFFFF]\n", __func__);
	} else {
		tsp_debug_info(true, &client->dev, "%s - Chip firmware version [0x%04X 0x%04X 0x%04X 0x%04X]\n",
			__func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3]);
	}

	//Set update flag
	tsp_debug_info(true, &client->dev,
		"%s - Firmware file info : Sections[%d] Offset[0x%08X] Length[0x%08X]\n",
		__func__, fw_hdr->section_num, fw_hdr->binary_offset, fw_hdr->binary_length);

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mip_fw_img)) {
		img[i] = (struct mip_fw_img *)(fw_data + offset);
		ver_file[i] = img[i]->version;

		tsp_debug_info(true, &client->dev,"%s - Section info : Section[%d] Version[0x%04X] \
			StartPage[%d] EndPage[%d] Offset[0x%08X] Length[0x%08X]\n",__func__,
			i, img[i]->version, img[i]->start_page, img[i]->end_page, img[i]->offset, img[i]->length);

		//Compare section version
		if (ver_chip[i] != ver_file[i]) {
			//Set update flag
			update_flag = true;
			update_flags[i] = true;

			tsp_debug_info(true, &client->dev,
				"%s - Section [%d] is need to be updated. Version : Chip[0x%04X] File[0x%04X]\n",
				__func__, i, ver_chip[i], ver_file[i]);
		}
	}

	//Set force update flag
	if (force == true) {
		update_flag = true;
		update_flags[0] = true;
		update_flags[1] = true;
		update_flags[2] = true;
		update_flags[3] = true;

		tsp_debug_info(true, &client->dev, "%s - Force update\n", __func__);
	}

	//Exit when up-to-date
	if (update_flag == false) {
		nRet = fw_err_uptodate;
		tsp_debug_dbg(true, &client->dev, "%s [DONE] Chip firmware is already up-to-date\n", __func__);
		goto EXIT;
	}

	//Set start addr offset
	if (section == true) {
		if (update_flags[0] == true)	//boot
			offsetStart = img[0]->start_page;
		else if (update_flags[1] == true)	//core
			offsetStart = img[1]->start_page;
		else if(update_flags[2] == true)	//custom
			offsetStart = img[2]->start_page;
		else if(update_flags[3] == true)	//param
			offsetStart = img[3]->start_page;
	} else {
		offsetStart = 0;
	}

	offsetStart = offsetStart * 1024;

	//Load firmware data
	data = vzalloc(sizeof(u8) * fw_hdr->binary_length);
	if (!data) {
		tsp_debug_err(true, &client->dev, "Failed to data allocate memory\n");
		nRet = -ENOMEM;
		goto err_alloc_data;
	}

	size = fw_hdr->binary_length;
	cpydata = vzalloc(ISC_PAGE_SIZE);
	if (!cpydata) {
		tsp_debug_err(true, &client->dev, "Failed to cpydata allocate memory\n");
		nRet = -ENOMEM;
		goto err_alloc_cpydata;
	}

	//Check firmware size
	if (size % ISC_PAGE_SIZE != 0)
		size += ( ISC_PAGE_SIZE - (size % ISC_PAGE_SIZE));

	nStartAddr = 0;
	nWriteLength = size;
	nLast = nStartAddr + nWriteLength;

	if ((nLast) % 8 != 0) {
		nRet = fw_err_file_type;
		tsp_debug_err(true, &client->dev, "%s [ERROR] Firmware size mismatch\n", __func__);
		goto ERROR;
	} else {
		memcpy(data, fw_data + fw_hdr->binary_offset, fw_hdr->binary_length);
	}

	//Set address
	nOffset = nStartAddr + nWriteLength - ISC_PAGE_SIZE;
	nTransferLength = ISC_PAGE_SIZE;

	//Erase first page
	tsp_debug_info(true, &client->dev, "%s - Erase first page : Offset[0x%04X]\n", __func__, offsetStart);
	nRet = mip_isc_erase_page(info, offsetStart);
	if (nRet != 0) {
		tsp_debug_err(true, &client->dev,"%s [ERROR] clear first page failed\n", __func__);
		goto ERROR;
	}

	//Flash firmware
	tsp_debug_info(true, &client->dev, "%s - Start Download : Offset Start[0x%04X] End[0x%04X]\n",
		__func__, nOffset, offsetStart);

	while (nOffset >= offsetStart) {
		tsp_debug_info(true, &client->dev, "%s - Downloading : Offset[0x%04X]\n", __func__, nOffset);

		//Program (erase and write) a page
		nRet = mip_isc_program_page(info, nOffset, &data[nOffset], nTransferLength);
		if (nRet != 0) {
			tsp_debug_err(true, &client->dev,"%s [ERROR] isc_program_page\n", __func__);
			goto ERROR;
		}

		//Verify (read and compare)
		if (mip_isc_read_page(info, nOffset, cpydata)) {
			tsp_debug_err(true, &client->dev,"%s [ERROR] mip_isc_read_page\n", __func__);
			goto ERROR;
		}

		if (memcmp(&data[nOffset], cpydata, ISC_PAGE_SIZE)) {
#if MMS_FW_UPDATE_DEBUG
			print_hex_dump(KERN_ERR, "Firmware Page Write : ", DUMP_PREFIX_OFFSET, 16, 1, data, ISC_PAGE_SIZE, false);
			print_hex_dump(KERN_ERR, "Firmware Page Read : ", DUMP_PREFIX_OFFSET, 16, 1, cpydata, ISC_PAGE_SIZE, false);
#endif
			tsp_debug_err(true, &client->dev, "%s [ERROR] verify page failed\n", __func__);

			nRet = -1;
			goto ERROR;
		}

		nOffset -= nTransferLength;
	}

	//Exit ISC
	nRet = mip_isc_exit(info);
	if (nRet != 0) {
		tsp_debug_err(true, &client->dev,"%s [ERROR] mip_isc_exit\n", __func__);
		goto ERROR;
	}

	//Reboot chip
	mms_reboot(info);

	//Check chip firmware version
	if (mms_get_fw_version_u16(info, ver_chip)) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] cannot read chip firmware version after flash\n",
			__func__);

		nRet = -1;
		goto ERROR;
	} else {
		for (i = 0; i < fw_hdr->section_num; i++) {
			if (ver_chip[i] != ver_file[i]) {
				tsp_debug_err(true, &client->dev, "%s [ERROR] version mismatch after flash. Section[%d] : \
					Chip[0x%04X] != File[0x%04X]\n", __func__, i, ver_chip[i], ver_file[i]);

				nRet = -1;
				goto ERROR;
			}
		}
	}

	nRet = 0;
	tsp_debug_dbg(true, &client->dev,"%s [DONE]\n", __func__);
	tsp_debug_info(true, &client->dev,"Firmware update completed\n");
	goto DONE;

ERROR:
	tsp_debug_err(true, &client->dev,"%s [ERROR]\n", __func__);
	tsp_debug_err(true, &client->dev,"Firmware update failed\n");
DONE:
	vfree(cpydata);
err_alloc_cpydata:
	vfree(data);
err_alloc_data:
EXIT:
	vfree(img);
err_alloc_img:
	return nRet;
}

/**
* Get version of F/W bin file
*/
int mip_bin_fw_version(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct mip_bin_hdr *fw_hdr;
	struct mip_fw_img **img;
	int offset = sizeof(struct mip_bin_hdr);
	int i = 0;

	tsp_debug_dbg(true, &info->client->dev,"%s [START]\n", __func__);

	fw_hdr = (struct mip_bin_hdr *)fw_data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mip_fw_img)) {
		img[i] = (struct mip_fw_img *)(fw_data + offset);
		ver_buf[i * 2] = ((img[i]->version) >> 8) & 0xFF;
		ver_buf[i * 2 + 1] = (img[i]->version) & 0xFF;
	}

	tsp_debug_dbg(true, &info->client->dev,"%s [DONE]\n", __func__);

	return 0;
}
#endif

#ifdef FW_TYPE_TAIL
/**
* Flash chip firmware (main function)
*/
int mms_flash_fw(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section)
{
	struct i2c_client *client = info->client;
	struct mip_bin_tail *bin_info;
	int ret = 0;
	int retry = 3;
	u8 rbuf[ISC_PAGE_SIZE];
	int offset = 0;
	int offset_start = 0;
	int bin_size = 0;
	u8 *bin_data = 0;
	u16 tail_size = 0;
	u8 tail_mark[4] = MIP_BIN_TAIL_MARK;
	u16 ver_chip[MMS_FW_MAX_SECT_NUM];

	tsp_debug_dbg(true, &client->dev, "%s [START]\n", __func__);

	//Check tail size
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MIP_BIN_TAIL_SIZE) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		ret = fw_err_file_type;
		goto ERROR;
	}

	//Check bin format
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		ret = fw_err_file_type;
		goto ERROR;
	}

	//Read bin info
	bin_info = (struct mip_bin_tail *)&fw_data[fw_size - tail_size];

	tsp_debug_dbg(true, &client->dev, "%s - bin_info : bin_len[%d] hw_cat[0x%2X] date[%4X] \
		time[%4X] tail_size[%d]\n", __func__, bin_info->bin_length, bin_info->hw_category,
		bin_info->build_date, bin_info->build_time, bin_info->tail_size);

#if MMS_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " Bin Info : ", DUMP_PREFIX_OFFSET, 16, 1, bin_info, tail_size, false);
#endif

	//Check chip code
	if (memcmp(bin_info->chip_name, FW_CHIP_CODE, 4)) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);
		ret = fw_err_file_type;
		goto ERROR;
	}

	//Check F/W version
	tsp_debug_info(true, &client->dev, "%s - F/W file version [0x%04X 0x%04X 0x%04X 0x%04X]\n",
		__func__, bin_info->ver_boot, bin_info->ver_core, bin_info->ver_app, bin_info->ver_param);

	if (force == true) {
		//Force update
		tsp_debug_info(true, &client->dev, "%s - Skip chip firmware version check\n", __func__);
	} else {
		//Read firmware version from chip
		while (retry--) {
			if (!mms_get_fw_version_u16(info, ver_chip)) {
				break;
			} else {
				mms_reboot(info);
			}
		}

		if (retry < 0) {
			tsp_debug_err(true, &client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		} else {
			tsp_debug_info(true, &client->dev, "%s - Chip firmware version [0x%04X 0x%04X 0x%04X 0x%04X]\n",
					__func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3]);

			//Compare version
			if (ver_chip[2] == 0xFFFF || ver_chip[3] == 0xFFFF) {
				tsp_debug_info(true, &client->dev, "%s [ERROR] Chip firmware is broken\n", __func__);
			}else if ((ver_chip[2] >= bin_info->ver_app) && (ver_chip[3] >= bin_info->ver_param)) {
				tsp_debug_info(true, &client->dev, "%s - Chip firmware is already up-to-date\n", __func__);
				ret = fw_err_uptodate;
				goto ERROR;
			}
		}
	}

	//Read bin data
	bin_size = bin_info->bin_length;
	bin_data = vzalloc(sizeof(u8) * bin_size);
	if (!bin_data) {
		tsp_debug_err(true, &client->dev, "Failed to img allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_bin_data;
	}

	memcpy(bin_data, fw_data, bin_size);

	//Erase first page
	offset = 0;
	tsp_debug_dbg(true, &client->dev, "%s - Erase first page : Offset[0x%04X]\n", __func__, offset);
	ret = mip_isc_erase_page(info, offset);
	if (ret != 0) {
		tsp_debug_err(true, &client->dev,"%s [ERROR] mip_isc_erase_page\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	//Program & Verify
	tsp_debug_dbg(true, &client->dev, "%s - Program & Verify\n", __func__);
	offset_start = 0;
	offset = bin_size - ISC_PAGE_SIZE;
	while (offset >= offset_start) {
		//Program page
		if (mip_isc_program_page(info, offset, &bin_data[offset], ISC_PAGE_SIZE)) {
			tsp_debug_err(true, &client->dev, "%s [ERROR] mip_isc_program_page : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR;
		}
		tsp_debug_dbg(true, &client->dev, "%s - mip_isc_program_page : offset[0x%04X]\n", __func__, offset);

		//Verify page
		if (mip_isc_read_page(info, offset, rbuf)) {
			tsp_debug_err(true, &client->dev, "%s [ERROR] mip_isc_read_page : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR;
		}
		tsp_debug_dbg(true, &client->dev, "%s - mip_isc_read_page : offset[0x%04X]\n", __func__, offset);

#if MMS_FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W File : ", DUMP_PREFIX_OFFSET, 16, 1, &bin_data[offset], ISC_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MMS_DEVICE_NAME " F/W Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISC_PAGE_SIZE, false);
#endif

		if (memcmp(rbuf, &bin_data[offset], ISC_PAGE_SIZE)) {
			tsp_debug_err(true, &client->dev, "%s [ERROR] Verify failed : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR;
		}
		offset -= ISC_PAGE_SIZE;
	}

	//Exit ISC mode
	tsp_debug_dbg(true, &client->dev, "%s - Exit\n", __func__);
	mip_isc_exit(info);

	//Reset chip
	mms_reboot(info);

	//Check chip firmware version
	if (mms_get_fw_version_u16(info, ver_chip)) {
		tsp_debug_err(true, &client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	} else {
		if ((ver_chip[0] == bin_info->ver_boot) && (ver_chip[1] == bin_info->ver_core) && (ver_chip[2] == bin_info->ver_app) && (ver_chip[3] == bin_info->ver_param)) {
			tsp_debug_dbg(true, &client->dev, "%s - Version check OK\n", __func__);
		} else {
			tsp_debug_err(true, &client->dev, "%s [ERROR] Version mismatch after flash. Chip[0x%04X 0x%04X 0x%04X 0x%04X] File[0x%04X 0x%04X 0x%04X 0x%04X]\n", __func__, ver_chip[0], ver_chip[1], ver_chip[2], ver_chip[3], bin_info->ver_boot, bin_info->ver_core, bin_info->ver_app, bin_info->ver_param);
			ret = fw_err_download;
			goto ERROR;
		}
	}

	tsp_debug_dbg(true, &client->dev, "%s [DONE]\n", __func__);
	goto EXIT;

ERROR:
	//Reset chip
	mms_reboot(info);
	tsp_debug_err(true, &client->dev, "%s [ERROR]\n", __func__);
EXIT:
	vfree(bin_data);
err_alloc_bin_data:
	return ret;
}

/**
* Get version of F/W bin file
*/
int mip_bin_fw_version(struct mms_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf)
{
	struct mip_bin_tail *bin_info;
	u16 tail_size = 0;
	u8 tail_mark[4] = MIP_BIN_TAIL_MARK;

	tsp_debug_dbg(true, &info->client->dev,"%s [START]\n", __func__);

	//Check tail size
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if (tail_size != MIP_BIN_TAIL_SIZE) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		goto ERROR;
	}

	//Check bin format
	if (memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)) {
		tsp_debug_err(true, &info->client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		goto ERROR;
	}

	//Read bin info
	bin_info = (struct mip_bin_tail *)&fw_data[fw_size - tail_size];

	//F/W version
	ver_buf[0] = (bin_info->ver_boot >> 8) & 0xFF;
	ver_buf[1] = (bin_info->ver_boot) & 0xFF;
	ver_buf[2] = (bin_info->ver_core >> 8) & 0xFF;
	ver_buf[3] = (bin_info->ver_core) & 0xFF;
	ver_buf[4] = (bin_info->ver_app >> 8) & 0xFF;
	ver_buf[5] = (bin_info->ver_app) & 0xFF;
	ver_buf[6] = (bin_info->ver_param >> 8) & 0xFF;
	ver_buf[7] = (bin_info->ver_param) & 0xFF;

	tsp_debug_dbg(true, &info->client->dev,"%s [DONE]\n", __func__);
	return 0;

ERROR:
	tsp_debug_err(true, &info->client->dev,"%s [ERROR]\n", __func__);
	return 1;
}
#endif

