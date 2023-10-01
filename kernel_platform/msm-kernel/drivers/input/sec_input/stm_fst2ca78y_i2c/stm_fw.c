// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "stm_dev.h"
#include "stm_reg.h"

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#define SUPPORT_FW_SIGNED
#endif

#ifdef SUPPORT_FW_SIGNED
#include <linux/spu-verify.h>
#endif

#define SIGNEDKEY_SIZE (256)
#define FLASH_MAX_SECTIONS		10

#define FLASH_PAGE_SIZE			(4 * 1024)	/* page size of 4KB */
#define BIN_HEADER_SIZE			(32 + 4)	/* fw ubin main header size including crc */
#define SECTION_HEADER_SIZE		20		/* fw ubin section header size */
#define BIN_HEADER			0xBABEFACE	/* fw ubin main header identifier constant */
#define SECTION_HEADER			0xB16B00B5	/* fw ubin section header identifier constant */
#define CHIP_ID				0x3652		/* chip id of finger tip device, spruce */
#define NUM_FLASH_PAGES			48		/* number of flash pages in fingertip device, spruce */

#define STM_TS_FILE_SIGNATURE		BIN_HEADER

#define WRITE_CHUNK_SIZE 1024
#define DRAM_SIZE (64 * 1024)	// 64KB

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	TSP_SIGNED_SDCARD,
	TSP_SPU,
	TSP_VERIFICATION,
};


struct stm_ts_header {		// Total 36 bytes
	u32	fw_crc;				// 0
	u32	header_signature;	// 4
	u8 format_id;			// 8
	u8 chip_id[3];			// 9
	u32 ext_release_ver1;	// 12
	u32 ext_release_ver;	// 16
	u16 fw_ver;				// 20
	u16 cfg_ver;			// 22
	u8 project_id;			// 24
	u8 ic_name;				// 25
	u8 module_ver;			// 26
	u8 reserved[9];			// 27
} __packed;

/**
 * @brief  Type definitions - header structure of firmware file (.ubin)
 */

/**
 * Struct which contains  the header and data of a flash section
 */
struct fw_section {
	u16 sec_id;		/* FW section ID mapped for each flash sections */
	u16 sec_ver;		/* FW section version  */
	u32 sec_size;		/* FW sectionsize in bytes  */
	u8 *sec_data;		/* FW section data */
};

/**
 * Struct which contains information and data of the FW file to be burnt
 *into the IC
 */
struct firmware_file {
	u16 fw_ver;					/*  FW version of the FW file */
	u8 flash_code_pages;				/* size of fw code in pages*/
	u8 panel_info_pages;				/* size of fw sections in pages*/
	u32 fw_code_size;				/*  size of fw code in bytes*/
	u8 *fw_code_data;				/* fw code data*/
	u8 num_code_pages;				/* size depending on the FW code size*/
	u8 num_sections;								/* fw sections in fw file */
	struct fw_section sections[FLASH_MAX_SECTIONS];	/*  fw section data for each section*/
};

/**
 * Define section ID mapping for flash sections
 */
enum fw_section_t {
	FINGERTIP_FW_CODE = 0x0000,		/* FW main code*/
	FINGERTIP_FW_REG = 0x0001,		/* FW Reg section */
	FINGERTIP_MUTUAL_CX = 0x0002,		/* FW mutual cx */
	FINGERTIP_SELF_CX = 0x0003,		/* FW self cx */
};

unsigned int stm_ts_calculate_crc(const u8 *message, int size)
{
	int i, j;
	unsigned int byte, crc, mask;

	i = 0;
	crc = 0xFFFFFFFF;
	while (i < size) {
		byte = message[i];
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--) {
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		i = i + 1;
	}
	return ~crc;
}

int stm_ts_u8_to_u32_be(const u8 *src, u32 *dst)
{
	*dst = (u32)(((src[0] & 0xFF) << 24) + ((src[1] & 0xFF) << 16) +
				 ((src[2] & 0xFF) << 8) + (src[3] & 0xFF));
	return 0;
}

int stm_ts_u8_to_u16_be(const u8 *src, u16 *dst)
{
	*dst = (u16)(((src[0] & 0x00FF) << 8) + (src[1] & 0x00FF));
	return 0;
}

/**
 * Parse the raw data read from a FW file in order to fill properly the fields
 * of a Firmware variable
 * @param ubin_data raw FW data loaded from system
 * @param ubin_size size of ubin_data
 * @param fw_data pointer to a Firmware variable which will contain the
 *processed data
 * @return OK if success or an error code which specify the type of error
 */
int stm_ts_parse_bin_file(struct stm_ts_data *ts, const u8 *ubin_data, int ubin_size, struct firmware_file *fw_data)
{
	int index = 0;
	u32 temp = 0;
	u16 u16_temp = 0;
	u8 sec_index = 0;
	int code_data_found = 0;
	u32 crc = 0;

	if (ubin_size <= (BIN_HEADER_SIZE + SECTION_HEADER_SIZE) || ubin_data == NULL) {
		input_err(true, &ts->client->dev, "%s: Read only %d instead of %d\n",
					__func__, ubin_size, BIN_HEADER_SIZE);
		return -1;
	}

	crc = stm_ts_calculate_crc(ubin_data + 4, ubin_size - 4);
	if (crc == (u32)((ubin_data[0] << 24) + (ubin_data[1] << 16) +
					 (ubin_data[2] << 8) + ubin_data[3]))
		input_info(true, &ts->client->dev, "%s: BIN CRC OK\n", __func__);
	else {
		input_err(true, &ts->client->dev, "%s: BIN CRC error\n", __func__);
		return -1;
	}

	// Check firmware signature
	index += 4;
	stm_ts_u8_to_u32_be(&ubin_data[index], &temp);
	if (temp != BIN_HEADER) {
		input_err(true, &ts->client->dev, "%s: Wrong Signature 0x%08X\n", __func__, temp);
		return -1;
	}

	// Check Chip ID
	index += 5;
	stm_ts_u8_to_u16_be(&ubin_data[index], &u16_temp);
	if (u16_temp != CHIP_ID) {
		input_err(true, &ts->client->dev, "%s: Wrong Chip ID 0x%04X\n", __func__, u16_temp);
		return -1;
	}
	input_info(true, &ts->client->dev, "%s: Chip ID: 0x%04X\n", __func__, u16_temp);

	index += 27;
	while (index < ubin_size) {
		// Check Section signature
		stm_ts_u8_to_u32_be(&ubin_data[index], &temp);
		if (temp != SECTION_HEADER) {
			input_err(true, &ts->client->dev, "%s: Wrong Section Signature %08X\n", __func__, temp);
			return -1;
		}

		index += 4;
		stm_ts_u8_to_u16_be(&ubin_data[index], &u16_temp);
		if (u16_temp == FINGERTIP_FW_CODE) {
			if (code_data_found) {
				input_err(true, &ts->client->dev, "%s: Cannot have more than one code memh\n", __func__);
				return -1;
			}
			code_data_found = 1;

			// Get firmware code size
			index += 4;
			stm_ts_u8_to_u32_be(&ubin_data[index], &temp);
			fw_data->fw_code_size = temp;
			if (fw_data->fw_code_size == 0) {
				input_err(true, &ts->client->dev, "%s: Code data cannot be empty\n", __func__);
				return -1;
			}

			fw_data->fw_code_data = kmalloc(fw_data->fw_code_size * sizeof(u8), GFP_KERNEL);
			if (fw_data->fw_code_data == NULL)
				return -ENOMEM;

			fw_data->num_code_pages = (fw_data->fw_code_size / FLASH_PAGE_SIZE);

			if (fw_data->fw_code_size % FLASH_PAGE_SIZE)
				fw_data->num_code_pages++;

			input_info(true, &ts->client->dev, "%s: code pages: %d\n", __func__, fw_data->num_code_pages);
			input_info(true, &ts->client->dev, "%s: code size: %d bytes\n", __func__, fw_data->fw_code_size);

			index += 12;
			memcpy(fw_data->fw_code_data, &ubin_data[index], fw_data->fw_code_size);

			index += fw_data->fw_code_size;
			fw_data->fw_ver = (u16)((fw_data->fw_code_data[209] << 8) + fw_data->fw_code_data[208]);
			input_info(true, &ts->client->dev, "%s: FW version: 0x%04X\n", __func__, fw_data->fw_ver);

			fw_data->flash_code_pages = fw_data->fw_code_data[216];
			fw_data->panel_info_pages = fw_data->fw_code_data[217];
			input_info(true, &ts->client->dev, "%s: Code Pages(in org info): %02X,Panel Info Pages(in org info): %02X\n",
					   __func__, fw_data->flash_code_pages, fw_data->panel_info_pages);

			if ((fw_data->flash_code_pages + fw_data->panel_info_pages) > NUM_FLASH_PAGES) {
				input_err(true, &ts->client->dev, "%s:FW code + panel Info pages(%d) is more the maximum flash pages(%d)\n",
					__func__, (fw_data->flash_code_pages + fw_data->panel_info_pages), NUM_FLASH_PAGES);
				return -1;
			}

			if (fw_data->num_code_pages > fw_data->flash_code_pages) {
				input_err(true, &ts->client->dev, "%s: FW code size in the bin file(%d) is more than the FW code pages(%d) allocated by FW\n",
						  __func__, fw_data->num_code_pages, fw_data->flash_code_pages);
				return -1;
			}
		} else {
			fw_data->num_sections++;
			fw_data->sections[sec_index].sec_id = u16_temp;
			index += 4;
			stm_ts_u8_to_u32_be(&ubin_data[index], &temp);
			fw_data->sections[sec_index].sec_size = temp;
			if (fw_data->sections[sec_index].sec_size == 0) {
				input_err(true, &ts->client->dev, "%s: section data cannot be empty\n", __func__);
				return -1;
			}
			fw_data->sections[sec_index].sec_data = kmalloc(fw_data->sections[sec_index].sec_size * sizeof(u8), GFP_KERNEL);
			if (fw_data->sections[sec_index].sec_data == NULL)
				return -ENOMEM;

			input_info(true, &ts->client->dev, "%s: section%d type : 0x%02X\n", __func__, sec_index, fw_data->sections[sec_index].sec_id);
			input_info(true, &ts->client->dev, "%s: section%d size : %d bytes\n", __func__, sec_index, fw_data->sections[sec_index].sec_size);

			index += 12;
			memcpy(fw_data->sections[sec_index].sec_data, &ubin_data[index], fw_data->sections[sec_index].sec_size);
			index += fw_data->sections[sec_index].sec_size;
			if (fw_data->sections[sec_index].sec_id == FINGERTIP_FW_REG) {
				fw_data->sections[sec_index].sec_ver = (u16)((fw_data->sections[sec_index].sec_data[15] << 8) + fw_data->sections[sec_index].sec_data[14]);
				input_info(true, &ts->client->dev, "%s: section version : 0x%04X\n", __func__, fw_data->sections[sec_index].sec_ver);
			}
			sec_index++;
		}
	}
	input_info(true, &ts->client->dev, "%s: Total number of sections : %d\n", __func__, fw_data->num_sections);

	return 0;
}

int stm_ts_check_dma_startanddone(struct stm_ts_data *ts)
{
	int timeout = 60;
	u8 reg[6] = { STM_TS_CMD_REG_W, 0x20, 0x00, 0x00, 0x71, 0xC0 };
	u8 val[1];
	int ret;

	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write\n", __func__);
		return ret;
	}

	sec_delay(10);

	do {
		reg[0] = STM_TS_CMD_REG_R;
		ret = ts->stm_ts_read(ts, &reg[0], 5, (u8 *)val, 1);
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
	u8 reg[5] = { STM_TS_CMD_REG_R, 0x20, 0x00, 0x00, 0x6A };
	u8 val[1];
	int ret;

	do {
		ret = ts->stm_ts_read(ts, &reg[0], 5, (u8 *)val, 1);
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

int stm_ts_fw_fill_flash(struct stm_ts_data *ts, u32 address, u8 *data, int size)
{
	int remaining, index = 0;
	int towrite = 0;
	int byteblock = 0;
	int wheel = 0;
	u32 addr = 0;
	int rc;
	int delta;

	u8 buff[WRITE_CHUNK_SIZE + 5] = {0};
	u8 buff2[12] = {0};

	remaining = size;
	while (remaining > 0) {
		byteblock = 0;
		addr = 0x00100000;

		while ((byteblock < DRAM_SIZE) && remaining > 0) {
			if (remaining >= WRITE_CHUNK_SIZE) {
				if ((byteblock + WRITE_CHUNK_SIZE) <= DRAM_SIZE) {
					towrite = WRITE_CHUNK_SIZE;
					remaining -= WRITE_CHUNK_SIZE;
					byteblock += WRITE_CHUNK_SIZE;
				} else {
					delta = DRAM_SIZE - byteblock;
					towrite = delta;
					remaining -= delta;
					byteblock += delta;
				}
			} else {
				if ((byteblock + remaining) <= DRAM_SIZE) {
					towrite = remaining;
					byteblock += remaining;
					remaining = 0;
				} else {
					delta = DRAM_SIZE - byteblock;
					towrite = delta;
					remaining -= delta;
					byteblock += delta;
				}
			}

			index = 0;
			buff[index++] = 0xFA;
			buff[index++] = (u8) ((addr & 0xFF000000) >> 24);
			buff[index++] = (u8) ((addr & 0x00FF0000) >> 16);
			buff[index++] = (u8) ((addr & 0x0000FF00) >> 8);
			buff[index++] = (u8) ((addr & 0x000000FF));

			memcpy(&buff[index], data, towrite);

			rc = ts->stm_ts_write(ts, &buff[0], index + towrite, NULL, 0);
			if (rc < 0) {
				input_err(true, &ts->client->dev,
						"%s failed to write i2c register. ret:%d\n",
						__func__, rc);
				return rc;
			}
			sec_delay(5);

			addr += towrite;
			data += towrite;
		}

		input_info(true, &ts->client->dev, "%s: Write %d Bytes\n", __func__, byteblock);

		//configuring the DMA
		byteblock = byteblock / 4 - 1;

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
		buff2[index++] = (u8) (byteblock & 0x000000FF);
		buff2[index++] = (u8) ((byteblock & 0x0000FF00) >> 8);
		buff2[index++] = 0x00;

		rc = ts->stm_ts_write(ts, &buff2[0], index, NULL, 0);
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

int stm_ts_flash_erase(struct stm_ts_data *ts, int flash_pages)
{
	u8 i = 0;
	u8 mask[6] = {0};
	u8 mask_cnt = 6;
	int rc = 0;
	u8 data = 0x00;
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0};

	// Enable bit-by-bit mask
	for (i = 0; i < flash_pages; i++) {
		if (((int)((i) / 8)) < mask_cnt) {
			input_info(true, &ts->client->dev, "%s: ID = %d Index = %d Position = %d !\n",
					   __func__, i, ((int)((i) / 8)), (i % 8));
			mask[((int)((i) / 8))] |= 0x01 << (i % 8);
		}
	}

	// FLASH_PAGE_MASK_ADDR
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x01;
	reg[4] = 0x28;
	memcpy(&reg[5], &mask[0], mask_cnt);
	rc = ts->stm_ts_write(ts, &reg[0], 5 + mask_cnt, NULL, 0);
	if (rc < 0)
		return rc;

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6B;
	reg[5] = 0x00;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;

	// Read FLASH_MULTI_PAGE_ERASE_ADDR
	reg[0] = STM_TS_CMD_REG_R;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6E;
	rc = ts->stm_ts_read(ts, &reg[0], 5, &data, 1);
	if (rc < 0)
		return rc;

	// Enable Multi Page Erase
	data |= 0x80;
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6E;
	reg[5] = data;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;

	// Start Flash Erase
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6A;
	reg[5] = 0x80;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;

	sec_delay(100);

	rc = stm_ts_check_erase_done(ts);
	if (rc < 0)
		return rc;

	input_info(true, &ts->client->dev, "%s: Erase flash page by page DONE!\n", __func__);

	return rc;
}

int stm_ts_flash_update_preset(struct stm_ts_data *ts)
{
	int rc = 0;
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0};

	// System Reset and Hold
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x24;
	reg[5] = 0x01;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// UVLO setting
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x1B;
	reg[5] = 0x66;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x68;
	reg[5] = 0x13;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	// Change application mode
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x25;
	reg[5] = 0x20;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6B;
	reg[5] = 0x00;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	// Unlock Flash
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0xDE;
	reg[5] = 0x83;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	return rc;
}

int stm_ts_write_fw_reg(struct stm_ts_data *ts, u16 address, u8 *data, uint32_t length)
{
	int rc = 0;
	u8 reg[STM_TS_SPI_REG_W_CHUNK + 2] = {0};

	reg[0] = (u8)(address >> 8);
	reg[1] = (u8)(address & 0xFF);
	memcpy(&reg[2], &data[0], length);

	rc = ts->stm_ts_write(ts, &reg[0], length + 2, NULL, 0);
	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);

	return rc;
}

/**
 * Perform a firmware register read.
 * @param address varaiable to point register offset
 * @param read_data pointer to buffer to read data
 * @param read_length size of data to read
 * @return OK if success or an error code which specify the type of error
 */
int stm_ts_read_fw_reg(struct stm_ts_data *ts, u16 address, u8 *read_data, uint32_t read_length)
{
	int rc = 0;
	u8 reg[2] = {0};

	reg[0] = (u8)((address >> 8) & 0xFF);
	reg[1] = (u8)(address & 0xFF);

	rc = ts->stm_ts_read(ts, &reg[0], 2, &read_data[0], read_length);
	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);

	return rc;
}

/**
 * Perform a firmware request by setting the register bits.
 * conditional auto clear option for waiting for that bits to be
 * cleared by fw based on status of operation
 * @param address variable to point register offset
 * @param bit_to_set variable to set fw register bit
 * @param auto_clear flag to poll for the bit to clear after request
 * @param time_to_wait variable to set time out for auto clear
 * @return OK if success or an error code which specify the type of error
 */
int stm_ts_fw_request(struct stm_ts_data *ts, u16 address, u8 bit_to_set, int time_to_wait)
{
	int rc = 0;
	int i;
	u8 data = 0x00;

	rc = stm_ts_read_fw_reg(ts, address, &data, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
		return rc;
	}

	data = data | (0x01 << bit_to_set);
	rc = stm_ts_write_fw_reg(ts, address, &data, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
		return rc;
	}

	for (i = 0; i < time_to_wait; i++) {
		sec_delay(10);
		rc = stm_ts_read_fw_reg(ts, address, &data, 1);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
			return rc;
		}

		if ((data & (0x01 << bit_to_set)) == 0x00)
			break;
	}

	if (i == time_to_wait) {
		input_err(true, &ts->client->dev, "%s: FW reg status timeout.. RegVal(0x%02X)\n", __func__, data);
		return -1;
	}

	return rc;
}

int stm_ts_hdm_write_request(struct stm_ts_data *ts)
{
	int rc = 0;

	rc = stm_ts_fw_request(ts, STM_TS_HDM_WRITE_REQ_ADDR, 0, 300);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
		return rc;
	}

	rc = stm_ts_fw_request(ts, STM_TS_FLASH_SAVE_ADDR, 7, 300);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
		return rc;
	}

	return rc;
}

/**
 * Perform a hdm data write to frame buffer.
 * @param address varaiable to framebuffer
 * @param data address pointer  to array of data bytes
 * @param length size of data to write
 * @return OK if success or an error code which specify the type of error
 */
int stm_ts_write_hdm(struct stm_ts_data *ts, u16 address, u8 *data, int length)
{
	int rc = 0;
	u8 *reg;

	reg = kzalloc(length + 2, GFP_KERNEL);
	memset(reg, 0x00, length + 2);

	reg[0] = (u8)(address >> 8);
	reg[1] = (u8)(address & 0xFF);
	memcpy(&reg[2], &data[0], length);

	rc = ts->stm_ts_write(ts, &reg[0], length + 2, NULL, 0);
	kfree(reg);

	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);

	return rc;
}

int stm_ts_flash_section_burn(struct stm_ts_data *ts, struct firmware_file fw, enum fw_section_t section)
{
	int rc = 0;
	int i = 0;

	for (i = 0; i < fw.num_sections; i++) {
		if (fw.sections[i].sec_id == section) {
			rc = stm_ts_write_hdm(ts, FRAME_BUFFER_ADDR, fw.sections[i].sec_data, fw.sections[i].sec_size);
			if (rc < 0)
				return -1;

			rc = stm_ts_hdm_write_request(ts);
			if (rc < 0) {
				input_err(true, &ts->client->dev, "%s: ERROR\n", __func__);
				return -1;
			}
			break;
		}
	}
	return rc;
}

static int stm_ts_fw_burn(struct stm_ts_data *ts, const u8 *fw_data, int fw_size)
{
	struct firmware_file fw;
	int rc;
	int i;
	u8 data[STM_TS_EVENT_BUFF_SIZE] = {0x00};

	fw.fw_code_data = NULL;
	fw.num_sections = 0;
	fw.flash_code_pages = 0;
	fw.num_code_pages = 0;
	fw.fw_code_size = 0;
	fw.fw_ver = 0;
	fw.panel_info_pages = 0;

	for (i = 0; i < FLASH_MAX_SECTIONS; i++) {
		fw.sections[i].sec_data = NULL;
		fw.sections[i].sec_id = fw.sections[i].sec_ver = fw.sections[i].sec_size = 0;
	}

	rc = stm_ts_parse_bin_file(ts, fw_data, fw_size, &fw);
	if (rc < 0)
		return rc;

	rc = stm_ts_flash_update_preset(ts);
	if (rc < 0)
		return rc;

	//==================== Erase Flash ====================
	input_info(true, &ts->client->dev, "%s: Start Flash(Main Block) Erasing\n", __func__);
	rc = stm_ts_flash_erase(ts, fw.num_code_pages);
	if (rc < 0)
		return rc;

	//==================== Flash Code Area ====================
	input_info(true, &ts->client->dev, "%s: Start Flashing for Code Area\n", __func__);
	if (fw.fw_code_size > 0) {
		rc = stm_ts_fw_fill_flash(ts, CODE_ADDR_START, fw.fw_code_data, fw.fw_code_size);
		if (rc < 0)
			return rc;

		input_info(true, &ts->client->dev, "%s: Flash Code update finished..\n", __func__);

		rc = ts->stm_ts_systemreset(ts, 0);

		rc = stm_ts_read_fw_reg(ts, STM_TS_SYS_ERROR_ADDR + 4, data, 4);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: ERROR reading system error registers\n", __func__);
			return -1;
		}

		input_info(true, &ts->client->dev, "%s: Section System: reg section: %02X, ms_section: %02X, ss_section: %02X\n",
				   __func__, (data[0] & STM_TS_REG_CRC_MASK), (data[1] & STM_TS_MS_CRC_MASK), (data[1] & STM_TS_SS_CRC_MASK));

		input_info(true, &ts->client->dev, "%s: System Crc: misc: %02X, ioff: %02X, pure_raw_ms: %02X\n",
				   __func__, (data[0] & STM_TS_REG_MISC_MASK), (data[2] & STM_TS_IOFF_CRC_MASK), (data[3] & STM_TS_RAWMS_CRC_MASK));
	}

	//==================== Flash Config Area ====================
	input_info(true, &ts->client->dev, "%s: Start Flashing for Config Area\n", __func__);
	if (fw.sections[0].sec_size > 0) {
		rc = stm_ts_flash_section_burn(ts, fw, FINGERTIP_FW_REG);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: ERROR reading system error registers\n", __func__);
			return rc;
		}

		input_info(true, &ts->client->dev, "%s: Flash Config update finished..\n", __func__);

		ts->stm_ts_systemreset(ts, 0);

		rc = stm_ts_read_fw_reg(ts, STM_TS_SYS_ERROR_ADDR + 4, data, 2);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: ERROR reading system error registers\n", __func__);
			return rc;
		}

		input_info(true, &ts->client->dev, "%s: Section System Errors After section update: reg section: %02X, ms_section: %02X, ss_section: %02X\n",
				   __func__, (data[0] & STM_TS_REG_CRC_MASK), (data[1] & STM_TS_MS_CRC_MASK), (data[1] & STM_TS_SS_CRC_MASK));

		if ((data[0] & STM_TS_REG_CRC_MASK) != 0) {
			input_err(true, &ts->client->dev, "%s: Error updating flash reg section\n", __func__);
			return -1;
		}
	}

	// System Reset
	ts->stm_ts_systemreset(ts, 0);

	return 0;
}

static void stm_ts_set_factory_history_data(struct stm_ts_data *ts, u8 level)
{
	int ret;
	u8 regaddr[3] = { 0 };
	u8 wlevel;

	switch (level) {
	case OFFSET_FAC_NOSAVE:
		input_info(true, &ts->client->dev, "%s: not save to flash area\n", __func__);
		return;
	case OFFSET_FAC_SUB:
		wlevel = OFFSET_FW_SUB;
		break;
	case OFFSET_FAC_MAIN:
		wlevel = OFFSET_FW_MAIN;
		break;
	default:
		input_info(true, &ts->client->dev, "%s: wrong level %d\n", __func__, level);
		return;
	}

	regaddr[0] = 0xC7;
	regaddr[1] = 0x04;
	regaddr[2] = wlevel;

	ret = ts->stm_ts_write(ts, regaddr, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to write factory level %d\n", __func__, wlevel);
		return;
	}

	regaddr[0] = 0xA4;
	regaddr[1] = 0x05;
	regaddr[2] = 0x04; /* panel configuration area */

	ret = stm_ts_wait_for_echo_event(ts, regaddr, 3, 200);
	if (ret < 0)
		return;

	input_info(true, &ts->client->dev, "%s: save to flash area, level=%d\n", __func__, wlevel);
}

int stm_ts_execute_autotune(struct stm_ts_data *ts, bool issaving)
{
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0,};
	int rc;

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	stm_ts_set_scanmode(ts, STM_TS_SCAN_MODE_SCAN_OFF);
	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	// w A4 00 03
	if (issaving == true) {
		// full panel init
		reg[0] = 0x00;
		reg[1] = 0x22;
		reg[2] = 0x02;

		rc = stm_ts_wait_for_echo_event(ts, &reg[0], 3, 500);
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
		stm_tclm_data_write(ts, SEC_TCLM_NVM_ALL_DATA);
#endif
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	} else {
		// SS ATune
		//DataType = 0x0C;
		reg[0] = 0x00;
		reg[1] = 0x2C;
		reg[2] = 0x0F;

		rc = stm_ts_wait_for_echo_event(ts, &reg[0], 3, 500);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	}

	if (ts->plat_data->dump_ic_ver == STM_TS_GET_CMOFFSET_DUMP_V1)
		stm_ts_set_factory_history_data(ts, ts->factory_position);
	if (issaving == true)
		stm_ts_panel_ito_test(ts, SAVE_MISCAL_REF_RAW);

ERROR:
	stm_ts_set_scanmode(ts, ts->scan_mode);
	ts->factory_position = OFFSET_FAC_NOSAVE;

	return rc;
}

static const int stm_ts_fw_updater(struct stm_ts_data *ts, const u8 *fw_data, int fw_size)
{
	int retval;
	int retry;
	u32 fw_main_version;

	if (!fw_data) {
		input_err(true, &ts->client->dev, "%s: Firmware data is NULL\n",
				__func__);
		return -ENODEV;
	}

	stm_ts_u8_to_u32_be(&fw_data[16], &fw_main_version);

	input_info(true, &ts->client->dev,
			"%s: Starting firmware update : 0x%04X\n", __func__,
			fw_main_version);

	retry = 0;

	disable_irq(ts->client->irq);

	while (1) {
		retval = stm_ts_fw_burn(ts, fw_data, fw_size);
		if (retval >= 0) {
			stm_ts_get_version_info(ts);

			if (1/*fw_main_version == ts->fw_main_version_of_ic*/) {
				input_info(true, &ts->client->dev,
						"%s: Success Firmware update\n",
						__func__);
				ts->fw_corruption = false;

				retval = ts->stm_ts_systemreset(ts, 0);

				if (retval == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
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

	enable_irq(ts->client->irq);

	return retval;
}

int stm_ts_fw_update_on_probe(struct stm_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	char fw_path[STM_TS_MAX_FW_PATH];
	struct stm_ts_header *header;
#ifdef TCLM_CONCEPT
	int ret = 0;
	bool restore_cal = false;

	if (ts->tdata->support_tclm_test) {
		ret = sec_tclm_test_on_probe(ts->tdata);
		if (ret < 0)
			input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA read fail", __func__);
	}
#endif

	if (ts->plat_data->bringup == 1)
		return STM_TS_NOT_ERROR;

	if (!ts->plat_data->firmware_name) {
		input_err(true, &ts->client->dev, "%s: firmware name does not declair in dts\n", __func__);
		retval = -ENOENT;
		goto exit_fwload;
	}

	memset(fw_path, 0x00, STM_TS_MAX_FW_PATH);
	snprintf(fw_path, STM_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	input_info(true, &ts->client->dev, "%s: Load firmware : %s\n", __func__, fw_path);

	retval = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (retval) {
		input_err(true, &ts->client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		retval = STM_TS_NOT_ERROR;
		goto exit_fwload;
	}

	header = (struct stm_ts_header *)fw_entry->data;

	ts->fw_version_of_bin = (u16)(fw_entry->data[21] + (fw_entry->data[20] << 8));
	ts->fw_main_version_of_bin = (u16)(fw_entry->data[19] + (fw_entry->data[15] << 8));
	ts->config_version_of_bin = (u16)(fw_entry->data[23] + (fw_entry->data[22] << 8));
	ts->project_id_of_bin = (u8)(header->project_id);
	ts->ic_name_of_bin = (u8)(header->ic_name);
	ts->module_version_of_bin = (u8)(header->module_ver);
	ts->plat_data->img_version_of_bin[2] = (u8)(ts->module_version_of_bin);
	ts->plat_data->img_version_of_bin[3] = (u8)(ts->fw_main_version_of_bin & 0xFF);

	input_info(true, &ts->client->dev,
			"%s: [BIN] Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X\n", __func__,
			ts->fw_version_of_bin,
			ts->config_version_of_bin,
			ts->fw_main_version_of_bin);
	input_info(true, &ts->client->dev,
			"%s: [BIN] Project ID: 0x%02X, IC Name: 0x%02X, Module Ver: 0x%02X\n",
			__func__, ts->project_id_of_bin,
			ts->ic_name_of_bin, ts->module_version_of_bin);

	if (ts->plat_data->bringup == 2) {
		input_err(true, &ts->client->dev, "%s: skip fw_update for bringup\n", __func__);
		retval = STM_TS_NOT_ERROR;
		goto done;
	}

	if (ts->plat_data->hw_param.checksum_result)
		retval = STM_TS_NEED_FW_UPDATE;
	else if ((ts->ic_name_of_ic == 0xFF) && (ts->project_id_of_ic == 0xFF) && (ts->module_version_of_ic == 0xFF))
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ts->ic_name_of_ic != ts->ic_name_of_bin)
		retval = STM_TS_NOT_UPDATE;
	else if (ts->project_id_of_ic != ts->project_id_of_bin)
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ts->module_version_of_ic != ts->module_version_of_bin)
		retval = STM_TS_NOT_UPDATE;
	else if ((ts->fw_main_version_of_ic < ts->fw_main_version_of_bin)
			|| ((ts->config_version_of_ic < ts->config_version_of_bin))
			|| ((ts->fw_version_of_ic < ts->fw_version_of_bin)))
		retval = STM_TS_NEED_FW_UPDATE;
	else
		retval = STM_TS_NOT_ERROR;

	if ((ts->plat_data->bringup == 3) &&
			((ts->fw_main_version_of_ic != ts->fw_main_version_of_bin)
			|| (ts->config_version_of_ic != ts->config_version_of_bin)
			|| (ts->fw_version_of_ic != ts->fw_version_of_bin))) {
		input_info(true, &ts->client->dev,
				"%s: bringup 3, force update because version is different\n", __func__);
		retval = STM_TS_NEED_FW_UPDATE;
	}

	/* ic fw ver > bin fw ver && force is false */
	if (retval != STM_TS_NEED_FW_UPDATE) {
		input_err(true, &ts->client->dev, "%s: skip fw update\n", __func__);

		goto done;
	}

	retval = stm_ts_fw_updater(ts, fw_entry->data, fw_entry->size);
	if (retval < 0)
		goto done;

#ifdef TCLM_CONCEPT
	ret = stm_tclm_data_read(ts, SEC_TCLM_NVM_ALL_DATA);
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
	release_firmware(fw_entry);
exit_fwload:
	return retval;
}

static int stm_ts_load_fw_from_kernel(struct stm_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	char fw_path[STM_TS_MAX_FW_PATH];

	if (ts->plat_data->bringup == 1) {
		retval = -1;
		input_info(true, &ts->client->dev, "%s: can't update for bringup:%d\n",
				__func__, ts->plat_data->bringup);
		return retval;
	}

	if (ts->plat_data->firmware_name)
		snprintf(fw_path, STM_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	else
		return 0;

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


	ts->stm_ts_systemreset(ts, 20);

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
#endif

	retval = stm_ts_fw_updater(ts, fw_entry->data, fw_entry->size);
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
	int error = 0;
	struct stm_ts_header *header;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool is_fw_signed = false;

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
		return -EINVAL;
	}

	error = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (error) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d\n", __func__, error);
		goto err_request_fw;
	}

	header = kzalloc(sizeof(struct stm_ts_header), GFP_KERNEL);
	if (!header)
		goto err_header;
	memcpy(header, fw_entry->data, sizeof(struct stm_ts_header));

	// Get Binary Signature
	stm_ts_u8_to_u32_be(&(fw_entry->data[4]), &(header->header_signature));
	// Get External Release version
	stm_ts_u8_to_u32_be(&(fw_entry->data[16]), &(header->ext_release_ver));
	// Get Firmware version
	stm_ts_u8_to_u16_be(&(fw_entry->data[20]), &(header->fw_ver));
	// Get Config version
	stm_ts_u8_to_u16_be(&(fw_entry->data[22]), &(header->cfg_ver));

	if (header->header_signature != STM_TS_FILE_SIGNATURE) {
		error = -EIO;
		input_err(true, &ts->client->dev,
				"%s: File type is not match with stm_ts64 file. [%8x]\n",
				__func__, header->header_signature);
		goto done;
	}

	input_info(true, &ts->client->dev,
			"%s: %s FirmVer:0x%04X, MainVer:0x%04X, IC:0x%02X, Proj:0x%02X, Module:0x%02X\n",
			__func__, is_fw_signed ? "[SIGNED]" : "",
			(u16)header->fw_ver, (u16)header->ext_release_ver,
			header->ic_name, header->project_id, header->module_ver);

#ifdef SUPPORT_FW_SIGNED
	if (is_fw_signed) {
		long spu_ret = 0, org_size = 0;

		if (update_type == TSP_VERIFICATION) {
			org_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
			spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
			if (spu_ret != org_size) {
				input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, org_size:%ld\n",
					__func__, spu_ret, org_size);
				error = -EPERM;
			}
			goto done;
		} else if (update_type == TSP_SPU && ts->ic_name_of_ic == header->ic_name
				&& ts->project_id_of_ic == header->project_id
				&& ts->module_version_of_ic == header->module_ver) {
			if ((ts->fw_main_version_of_ic >= header->ext_release_ver)
				&& (ts->config_version_of_ic >= header->cfg_ver)
				&& (ts->fw_version_of_ic >= header->fw_ver)) {
				input_info(true, &ts->client->dev, "%s: skip spu update\n", __func__);
				error = 0;
				goto done;
			} else {
				input_info(true, &ts->client->dev, "%s: run spu update\n", __func__);
			}
		} else if (update_type == TSP_SDCARD && ts->ic_name_of_ic == header->ic_name) {
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

#ifdef SUPPORT_FW_SIGNED
	if (is_fw_signed)
		error = stm_ts_fw_updater(ts, fw_entry->data, fw_entry->size - SPU_METADATA_SIZE(TSP));
	else
#endif
		error = stm_ts_fw_updater(ts, fw_entry->data, fw_entry->size);

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
	kfree(header);
err_header:
	release_firmware(fw_entry);
err_request_fw:
	return error;
}

int stm_ts_fw_update_on_hidden_menu(struct stm_ts_data *ts, int update_type)
{
	int retval = SEC_ERROR;

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from apk.
	 */
	switch (update_type) {
	case TSP_BUILT_IN:
		retval = stm_ts_load_fw_from_kernel(ts);
		break;

	case TSP_SDCARD:
	case TSP_SPU:
	case TSP_VERIFICATION:
		retval = stm_ts_load_fw(ts, update_type);
		break;

	default:
		input_err(true, &ts->client->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	stm_ts_get_custom_library(ts);
	stm_ts_set_custom_library(ts);

	return retval;
}
MODULE_LICENSE("GPL");
