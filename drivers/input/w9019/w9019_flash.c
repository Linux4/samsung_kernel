/*
 * Wacom Penabled Driver for I2C
 *
 * Copyright (c) 2011-2014 Tatsunosuke Tobita, Wacom.
 * <tobita.tatsunosuke@wacom.co.jp>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version of 2 of the License,
 * or (at your option) any later version.
 */

#include "w9019_flash.h"
#include "wacom_dev.h"

static bool wacom_i2c_set_feature(struct wacom_i2c *wac_i2c, u8 report_id,
		unsigned int buf_size, u8 *data, u16 cmdreg,
		u16 datareg)
{
	int i, ret = -1;
	int total = SFEATURE_SIZE + buf_size;
	u8 *sFeature = NULL;
	bool bRet = false;

	sFeature = kzalloc(sizeof(u8) * total, GFP_KERNEL);
	if (!sFeature) {
		input_err(true, &wac_i2c->client->dev,
				"%s cannot preserve memory\n", __func__);
		goto out;
	}
	memset(sFeature, 0, sizeof(u8) * total);

	sFeature[0] = (u8)(cmdreg & 0x00ff);
	sFeature[1] = (u8)((cmdreg & 0xff00) >> 8);
	sFeature[2] = (RTYPE_FEATURE << 4) | report_id;
	sFeature[3] = CMD_SET_FEATURE;
	sFeature[4] = (u8)(datareg & 0x00ff);
	sFeature[5] = (u8)((datareg & 0xff00) >> 8);

	if ( (buf_size + 2) > 255) {
		sFeature[6] = (u8)((buf_size + 2) & 0x00ff);
		sFeature[7] = (u8)(( (buf_size + 2) & 0xff00) >> 8);
	} else {
		sFeature[6] = (u8)(buf_size + 2);
		sFeature[7] = (u8)(0x00);
	}

	for (i = 0; i < buf_size; i++)
		sFeature[i + SFEATURE_SIZE] = *(data + i);

	ret = w9019_i2c_send_boot(wac_i2c, sFeature, total);
	if (ret != total) {
		input_err(true, &wac_i2c->client->dev,
				"Sending Set_Feature failed sent bytes: %d\n", ret);
		goto err;
	}

	usleep_range(60, 61);
	bRet = true;
 err:
	kfree(sFeature);
	sFeature = NULL;

 out:
	return bRet;
}

static bool wacom_i2c_get_feature(struct wacom_i2c *wac_i2c, u8 report_id,
		unsigned int buf_size, u8 *data, u16 cmdreg,
		u16 datareg, int delay)
{
	int ret = -1;
	u8 *recv = NULL;
	bool bRet = false;
	u8 gFeature[] = {
		(u8)(cmdreg & 0x00ff),
		(u8)((cmdreg & 0xff00) >> 8),
		(RTYPE_FEATURE << 4) | report_id,
		CMD_GET_FEATURE,
		(u8)(datareg & 0x00ff),
		(u8)((datareg & 0xff00) >> 8)
	};

	/*"+ 2", adding 2 more spaces for organizeing again later in the passed data, "data"*/
	recv = kzalloc(sizeof(u8) * (buf_size + 0), GFP_KERNEL);
	if (!recv) {
		input_err(true, &wac_i2c->client->dev,
			"%s cannot preserve memory\n", __func__);
		goto out;
	}

	memset(recv, 0, sizeof(u8) * (buf_size + 0)); /*Append 2 bytes for length low and high of the byte*/

	ret = w9019_i2c_send_boot(wac_i2c, gFeature, GFEATURE_SIZE);
	if (ret != GFEATURE_SIZE) {
		input_err(true, &wac_i2c->client->dev,
				"%s Sending Get_Feature failed; sent bytes: %d\n",
				__func__, ret);
		goto err;
	}

	udelay(delay);

	ret = w9019_i2c_recv_boot(wac_i2c, recv, buf_size);
	if (ret != buf_size) {
		input_err(true, &wac_i2c->client->dev,
				"%s Receiving data failed; received bytes: %d\n", __func__, ret);
		goto err;
	}

	/*Coppy data pointer, subtracting the first two bytes of the length*/
	memcpy(data, (recv + 0), buf_size);

	bRet = true;
 err:
	kfree(recv);
	recv = NULL;

 out:
	return bRet;
}

static int wacom_flash_cmd(struct wacom_i2c *wac_i2c)
{
	u8 command[10];
	int len = 0;
	int ret = -1;

	command[len++] = 0x0d;
	command[len++] = FLASH_START0;
	command[len++] = FLASH_START1;
	command[len++] = FLASH_START2;
	command[len++] = FLASH_START3;
	command[len++] = FLASH_START4;
	command[len++] = FLASH_START5;
	command[len++] = 0x0d;

	ret = w9019_i2c_send_boot(wac_i2c, command, len);
	if(ret < 0){
		input_err(true, &wac_i2c->client->dev, "Sending flash command failed\n");
		return -EXIT_FAIL;
	}

	msleep(300);

	return 0;
}

int flash_query_w9019(struct wacom_i2c *wac_i2c)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;		/* Report:ReportID */
	command[len++] = BOOT_QUERY;				/* Report:Boot Query command */
	command[len++] = ECH = 7;				/* Report:echo */

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to get feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[3] != QUERY_CMD) ||
	     (response[4] != ECH) ) {
		input_err(true, &wac_i2c->client->dev,
				"%s res3:%x res4:%x\n", __func__, response[3], response[4]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if (response[5] != QUERY_RSP) {
		input_err(true, &wac_i2c->client->dev,
				"%s res5:%x\n", __func__, response[5]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	input_info(true, &wac_i2c->client->dev, "QUERY SUCCEEDED\n");
	return 0;
}

static bool flash_blver_w9019(struct wacom_i2c *wac_i2c, int *blver)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[len++] = BOOT_BLVER;					/* Report:Boot Version command */
	command[len++] = ECH = 7;							/* Report:echo */

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to set feature1\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s 2 failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[3] != BOOT_CMD) ||
	     (response[4] != ECH) ) {
		input_err(true, &wac_i2c->client->dev,
				"%s res3:%x res4:%x\n", __func__, response[3], response[4]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if (response[5] != QUERY_RSP) {
		input_err(true, &wac_i2c->client->dev,
				"%s res5:%x\n", __func__, response[5]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	*blver = (int)response[5];

	return true;
}

static bool flash_mputype_w9019(struct wacom_i2c *wac_i2c, int *pMpuType)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[RSP_SIZE];
	int ECH, len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;		/* Report:ReportID */
	command[len++] = BOOT_MPU;				/* Report:Boot Query command */
	command[len++] = ECH = 7;					/* Report:echo */

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, RSP_SIZE, response, COMM_REG, DATA_REG, (10 * 1000));
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to get feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	if ( (response[3] != MPU_CMD) ||
	     (response[4] != ECH) ) {
		input_err(true, &wac_i2c->client->dev,
				"%s res3:%x res4:%x\n", __func__, response[3], response[4]);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	*pMpuType = (int)response[5];
	return true;
}

static bool flash_end_w9019(struct wacom_i2c *wac_i2c)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	int ECH, len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;
	command[len++] = BOOT_EXIT;
	command[len++] = ECH = 7;

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to set feature 1\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	return true;
}


#ifdef PARTIAL_ERASE
static bool erase_datamem(struct wacom_i2c *wac_i2c)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ECH, j;
	int len = 0;

	command[len++] = BOOT_CMD_REPORT_ID;			/* Report:ReportID */
	command[len++] = BOOT_ERASE_DATAMEM;			/* Report:erase datamem command */
	command[len++] = ECH = BOOT_ERASE_DATAMEM;	/* Report:echo */
	command[len++] = DATAMEM_SECTOR0;				/* Report:erased block No. */

	/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/	
	sum = 0;
	sum += 0x05;
	sum += 0x07;
	for (j = 0; j < 4; j++)
		sum += command[j];

	cmd_chksum = ~sum + 1;					/* Report:check sum */
	command[len++] = cmd_chksum;

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev, "%s failed to set feature 1\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}
	
	udelay(50);

	do {

		bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
		if (!bRet) {
			input_err(true, &wac_i2c->client->dev, "%s failed to get feature\n", __func__);
			return -EXIT_FAIL_SEND_QUERY_COMMAND;
		}
		if ((response[3] != 0x0e || response[4] != ECH) || (response[5] != 0xff && response[5] != 0x00)) {
			input_err(true, &wac_i2c->client->dev, "%s failing resp3: %x resp4: %x resp5: %x\n",
				__func__, response[3], response[4], response[5]);
			return false;
		}

	} while (response[3] == 0x0e && response[4] == ECH && response[5] == 0xff);


	return true;
}

static bool erase_codemem(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	bool bRet = false;
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ECH, len = 0;
	int i, j;

	for (i = 0; i < num; i++) {

		len = 0;		
		command[len++] = BOOT_CMD_REPORT_ID;		/* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;		/* Report:erase command */
		command[len++] = ECH = i;					/* Report:echo */
		command[len++] = *eraseBlock;				/* Report:erased block No. */
		eraseBlock++;
		
		/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/	
		sum = 0;
		sum += 0x05;
		sum += 0x07;
		for (j = 0; j < 4; j++)
			sum += command[j];

		cmd_chksum = ~sum + 1;					/* Report:check sum */
		command[len++] = cmd_chksum;
	
		bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
		if (!bRet) {
			input_err(true, &wac_i2c->client->dev, "%s failed to set feature\n", __func__);
			return -EXIT_FAIL_SEND_QUERY_COMMAND;
		}	

		udelay(50);

		do {	

			bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
			if (!bRet) {
				input_err(true, &wac_i2c->client->dev, "%s failed to get feature\n", __func__);
				return -EXIT_FAIL_SEND_QUERY_COMMAND;
			}			
			if ((response[3] != 0x00 || response[4] != ECH) || (response[5] != 0xff && response[5] != 0x00)) {
				input_err(true, &wac_i2c->client->dev, "%s failing resp3: %x resp4: %x resp5: %x\n",
						__func__, response[3], response[4], response[5]);
				return false;
			}
			
		} while (response[3] == 0x00 && response[4] == ECH && response[5] == 0xff);
	}

	return true;
}

static bool flash_erase_w9019(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	bool ret;

	ret = erase_datamem(wac_i2c);
	if (!ret) {
		input_err(true, &wac_i2c->client->dev, "%s erasing datamem failed\n", __func__);
		return false;
	}

	ret = erase_codemem(wac_i2c, eraseBlock, num);
	if (!ret) {
		input_err(true, &wac_i2c->client->dev, "%s erasing codemem failed\n", __func__);
		return false;
	}

	return true;
}
#else
static bool flash_erase_all(struct wacom_i2c *wac_i2c)
{
	bool bRet = false;
	u8 command[BOOT_CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	int i, len = 0;
	int ECH, sum = 0;

	command[len++] = 7;
	command[len++] = 16;
	command[len++] = ECH = 2;
	command[len++] = 3;

	/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/
	sum += 0x05;
	sum += 0x07;
	for (i = 0; i < len; i++)
		sum += command[i];

	command[len++] = ~sum + 1;

	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, len, command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev, "%s failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	do {


		bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 0);
		if (!bRet) {
			input_err(true, &wac_i2c->client->dev, "%s failed to set feature\n", __func__);
			return -EXIT_FAIL_SEND_QUERY_COMMAND;
		}
		if (!(response[3] & 0x10) || !(response[4] & ECH) ||
		    (!(response[5] & 0xff) && response[5] & 0x00)) {
			input_err(true, &wac_i2c->client->dev, "%s failing 4 resp1: %x resp2: %x resp3: %x\n",
			       __func__, response[3], response[4], response[5]);
			return false;
		}

		msleep(200);

	} while(response[3] == 0x10 && response[4] == ECH && response[5] == 0xff);

	return true;
}
#endif

static bool flash_write_block_w9019(struct wacom_i2c *wac_i2c, char *flash_data,
				    unsigned long ulAddress, u8 *pcommand_id, int *ECH)
{
	const int MAX_COM_SIZE = (8 + FLASH_BLOCK_SIZE + 2);	//8: num of command[0] to command[7]
							//FLASH_BLOCK_SIZE: unit to erase the block
							//Num of Last 2 checksums
	bool bRet = false;
	u8 command[300];
	unsigned char sum = 0;
	int i;

	command[0] = BOOT_CMD_REPORT_ID;				/* Report:ReportID */
	command[1] = BOOT_WRITE_FLASH;				/* Report:program  command */
	command[2] = *ECH = ++(*pcommand_id);			/* Report:echo */
	command[3] = ulAddress & 0x000000ff;
	command[4] = (ulAddress & 0x0000ff00) >> 8;
	command[5] = (ulAddress & 0x00ff0000) >> 16;
	command[6] = (ulAddress & 0xff000000) >> 24;		/* Report:address(4bytes) */
	command[7] = 8;		/* Report:size(8*8=64) */

	/*Preliminarily store the data that cannnot appear here, but in wacom_set_feature()*/
	sum = 0;
	sum += 0x05;
	sum += 0x4c;
	for (i = 0; i < 8; i++)
		sum += command[i];
	command[MAX_COM_SIZE - 2] = ~sum + 1;					/* Report:command checksum */

	sum = 0;
	for (i = 8; i < (FLASH_BLOCK_SIZE + 8); i++){
		command[i] = flash_data[ulAddress+(i - 8)];
		sum += flash_data[ulAddress+(i - 8)];
	}

	command[MAX_COM_SIZE - 1] = ~sum+1;				/* Report:data checksum */

	/*Subtract 8 for the first 8 bytes*/
	bRet = wacom_i2c_set_feature(wac_i2c, REPORT_ID_1, (BOOT_CMD_SIZE + 4 - 8), command, COMM_REG, DATA_REG);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev, "%s failed to set feature\n", __func__);
		return -EXIT_FAIL_SEND_QUERY_COMMAND;
	}

	udelay(50);

	return true;
}

static bool flash_write_w9019(struct wacom_i2c *wac_i2c, unsigned char *flash_data,
			      unsigned long start_address, unsigned long *max_address)
{
	bool bRet = false;
	u8 command_id = 0;
	u8 response[BOOT_RSP_SIZE];
	int i, j, ECH = 0, ECH_len = 0;
	int ECH_ARRAY[3];
	unsigned long ulAddress;

	j = 0;
	for (ulAddress = start_address; ulAddress < *max_address; ulAddress += FLASH_BLOCK_SIZE) {
		for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
			if (flash_data[ulAddress+i] != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE))
			continue;

		bRet = flash_write_block_w9019(wac_i2c, flash_data, ulAddress, &command_id, &ECH);
		if(!bRet)
			return false;
		if (ECH_len == 3)
			ECH_len = 0;

		ECH_ARRAY[ECH_len++] = ECH;
		if (ECH_len == 3) {
			for (j = 0; j < 3; j++) {
				do {

					bRet = wacom_i2c_get_feature(wac_i2c, REPORT_ID_2, BOOT_RSP_SIZE, response, COMM_REG, DATA_REG, 50);
					if (!bRet) {
						input_err(true, &wac_i2c->client->dev,
								"%s failed to set feature\n", __func__);
						return -EXIT_FAIL_SEND_QUERY_COMMAND;
					}

					if ((response[3] != 0x01 || response[4] != ECH_ARRAY[j]) || (response[5] != 0xff && response[5] != 0x00)) {
						input_err(true, &wac_i2c->client->dev,
								"%s mismatched echo array addr: %ld res:%x\n",
								__func__, ulAddress, response[5]);
						return false;
					}
				} while (response[3] == 0x01 && response[4] == ECH_ARRAY[j] && response[5] == 0xff);
			}
		}
	}
	return true;
}

static int wacom_i2c_flash_w9019(struct wacom_i2c *wac_i2c, unsigned char *fw_data)
{
	bool bRet = false;
	int result, i;
	int eraseBlock[200], eraseBlockNum;
	int iBLVer = 0, iMpuType = 0;
	unsigned long max_address = 0;			/* Max.address of Load data */
	unsigned long start_address = 0x2000;		/* Start.address of Load data */

	wac_i2c->bl_mpu_match = true;

	/*Obtain boot loader version*/
	if (!flash_blver_w9019(wac_i2c, &iBLVer)) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to get Boot Loader version\n", __func__);
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;
	}
	input_info(true, &wac_i2c->client->dev, "BL version: %x\n", iBLVer);

	/*Obtain MPU type: this can be manually done in user space*/
	if (!flash_mputype_w9019(wac_i2c, &iMpuType)) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to get MPU type\n", __func__);
		wac_i2c->bl_mpu_match = false;
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	if (iMpuType != MPU_W9019) {
		input_err(true, &wac_i2c->client->dev,
				"%s: MPU is not W9019 : %x\n", __func__, iMpuType);
		wac_i2c->bl_mpu_match = false;
		return -EXIT_FAIL_GET_MPU_TYPE;
	}
	input_info(true, &wac_i2c->client->dev, "MPU type: %x\n", iMpuType);

	/*-----------------------------------*/
	/*Flashing operation starts from here*/

	/*Set start and end address and block numbers*/
	eraseBlockNum = 0;
	start_address = W9019_START_ADDR;
	max_address = W9019_END_ADDR;
	for (i = BLOCK_NUM; i >= 8; i--) {
		eraseBlock[eraseBlockNum] = i;
		eraseBlockNum++;
	}

	msleep(300);

	/*Erase the old program*/
	input_info(true, &wac_i2c->client->dev,
			"epen - %s erasing the current firmware\n", __func__);
#ifdef PARTIAL_ERASE
	bRet = flash_erase_w9019(wac_i2c, eraseBlock,  eraseBlockNum);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to erase the user program\n", __func__);
		result = -EXIT_FAIL_ERASE;
		goto fail;
	}
#else
	bRet = flash_erase_all(wac_i2c);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to erase the user program\n", __func__);
		result = -EXIT_FAIL_ERASE;
		goto fail;
	}
#endif

	/*Write the new program*/
	input_info(true, &wac_i2c->client->dev, "%s writing new firmware\n", __func__);
	bRet = flash_write_w9019(wac_i2c, fw_data, start_address, &max_address);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s failed to write firmware\n", __func__);
		result = -EXIT_FAIL_WRITE_FIRMWARE;
		goto fail;
	}

	/*Return to the user mode*/
	input_info(true, &wac_i2c->client->dev,
			"%s closing the boot mode\n", __func__);

	bRet = flash_end_w9019(wac_i2c);
	if (!bRet) {
		input_err(true, &wac_i2c->client->dev,
				"%s closing boot mode failed\n", __func__);
		result = -EXIT_FAIL_WRITING_MARK_NOT_SET;
		goto fail;
	}

	input_info(true, &wac_i2c->client->dev, "%s write and verify completed\n", __func__);
	result = EXIT_OK;

 fail:
	return result;
}

int w9019_i2c_flash(struct wacom_i2c *wac_i2c)
{
	int ret;

	if (wac_i2c->fw_data == NULL) {
		input_info(true, &wac_i2c->client->dev, "%s Data is NULL. Exit.\n", __func__);
		return -EINVAL;
	}

	w9019_compulsory_flash_mode(wac_i2c, true);
	w9019_reset_hw(wac_i2c);
	msleep(200);


	ret = wacom_flash_cmd(wac_i2c);
	if (ret < 0) {
		input_err(true, &wac_i2c->client->dev, "%s cannot send flash command\n", __func__);
		ret = -EXIT_FAIL;
		goto end_wacom_flash;
	}

	input_info(true, &wac_i2c->client->dev, "%s pass wacom_flash_cmd\n", __func__);

	ret = flash_query_w9019(wac_i2c);
	if(ret < 0) {
		input_err(true, &wac_i2c->client->dev, "%s Error: cannot send query\n", __func__);
		ret = -EXIT_FAIL;
		goto end_wacom_flash;
	}

	input_info(true, &wac_i2c->client->dev, "%s pass flash_query_w9014\n", __func__);

	ret = wacom_i2c_flash_w9019(wac_i2c, wac_i2c->fw_data);
	if (ret < 0) {
		input_err(true, &wac_i2c->client->dev,
				"%s Error: flash failed\n", __func__);
		ret = -EXIT_FAIL;
		goto end_wacom_flash;
	}

	msleep(200);
end_wacom_flash:
	w9019_compulsory_flash_mode(wac_i2c, false);
	w9019_reset_hw(wac_i2c);
	msleep(200);

	return ret;
}
