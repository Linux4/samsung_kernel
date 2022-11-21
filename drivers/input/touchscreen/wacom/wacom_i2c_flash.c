/*
 *  wacom_i2c_flash.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "wacom_i2c.h"
#include "wacom_i2c_flash.h"

static int wacom_enter_flash_mode(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	char buf[8];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	memset(buf, 0, 8);
	strncpy(buf, "\rflash\r", 7);

	len = sizeof(buf) / sizeof(buf[0]);

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: Sending flash command failed, %d\n",
				__func__, ret);
		return ret;
	}

	msleep(300);

	return 0;
}

static int wacom_set_cmd_feature(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	u8 buf[4];

	usleep_range(60, 61);

	buf[len++] = 4;
	buf[len++] = 0;
	buf[len++] = 0x37;
	buf[len++] = CMD_SET_FEATURE;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s: failed send CMD_SET_FEATURE, %d\n",
				__func__, ret);

	usleep_range(60, 61);

	return ret;
}

static int wacom_get_cmd_feature(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	u8 buf[6];

	usleep_range(60, 61);

	buf[len++] = 4;
	buf[len++] = 0;
	buf[len++] = 0x38;
	buf[len++] = CMD_GET_FEATURE;
	buf[len++] = 5;
	buf[len++] = 0;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s: failed send CMD_GET_FEATURE, ret: %d\n",
				__func__, ret);

	usleep_range(60, 61);

	return ret;
}

/*
 * mode1. BOOT_QUERY: check enter boot mode for flash.
 * mode2. BOOT_BLVER : check bootloader version.
 * mode3. BOOT_MPU : check MPU version
 */
int wacom_check_flash_mode(struct wacom_i2c *wac_i2c, int mode)
{
	int ret, ECH;
	unsigned char response_cmd = 0;
	unsigned char command[CMD_SIZE];
	unsigned char response[RSP_SIZE];

	switch (mode) {
	case BOOT_QUERY:
		response_cmd = QUERY_CMD;
		break;
	case BOOT_BLVER:
		response_cmd = BOOT_CMD;
		break;
	case BOOT_MPU:
		response_cmd = MPU_CMD;
		break;
	default:
		break;
	}

	dev_info(&wac_i2c->client->dev, "%s, mode = %s\n", __func__,
			(mode == BOOT_QUERY) ? "BOOT_QUERY" :
			(mode == BOOT_BLVER) ? "BOOT_BLVER" :
			(mode == BOOT_MPU) ? "BOOT_MPU" : "Not Support");

	ret = wacom_set_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	command[0] = 5;
	command[1] = 0;
	command[2] = 5;
	command[3] = 0;
	command[4] = BOOT_CMD_REPORT_ID;
	command[5] = mode;
	command[6] = ECH = 7;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, 7, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed send REPORT_ID, %d\n",
				__func__, ret);
		return ret;
	}

	ret = wacom_get_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			    WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed receive response, %d\n",
				__func__, ret);
		return ret;
	}

	if ((response[3] != response_cmd) || (response[4] != ECH)) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed matching response3[%x], 4[%x]\n",
				__func__, response[3], response[4]);
		return -EIO;
	}

	return response[5];

}

int wacom_enter_bootloader(struct wacom_i2c *wac_i2c)
{
	int ret;
	int retry = 0;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	ret = wacom_enter_flash_mode(wac_i2c);
	if (ret < 0)
		msleep(500);

	do {
		msleep(100);
		ret = wacom_check_flash_mode(wac_i2c, BOOT_QUERY);
		if (ret == QUERY_RSP)
			dev_info(&wac_i2c->client->dev, "%s: enter flash mode\n", __func__);
		else
			dev_err(&wac_i2c->client->dev, "%s: retry BOOT_QUERY\n", __func__);

		retry++;
	} while (ret < 0 && retry < 10);

	if (ret < 0)
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;

	ret = wacom_check_flash_mode(wac_i2c, BOOT_MPU);
	wac_i2c->boot_ver = ret;
	if (ret < 0)
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;

	dev_info(&wac_i2c->client->dev, "%s: MPU:%X\n", __func__, wac_i2c->boot_ver);

	return ret;
}

static int wacom_flash_end(struct wacom_i2c *wac_i2c)
{
	int ret, ECH;
	unsigned char command[CMD_SIZE];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	command[0] = 4;
	command[1] = 0;
	command[2] = 0x37;
	command[3] = CMD_SET_FEATURE;
	command[4] = 5;
	command[5] = 0;
	command[6] = 5;
	command[7] = 0;
	command[8] = BOOT_CMD_REPORT_ID;
	command[9] = BOOT_EXIT;
/*	command[10] = ECH = 7;*/
	command[10] = ECH = 0;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, 11, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s failed, %d\n",
				__func__, ret);
	return ret;
}

static bool erase_datamem(struct wacom_i2c *wac_i2c)
{
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ret, ECH, j;
	int len = 0;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	command[len++] = 4;			/* Command Register-LSB */
	command[len++] = 0;			/* Command Register-MSB */
	command[len++] = 0x37;			/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[len++] = 5;			/* Data Register-LSB */
	command[len++] = 0;			/* Data-Register-MSB */
	command[len++] = 0x07;			/* Length Field-LSB */
	command[len++] = 0;			/* Length Field-MSB */
	command[len++] = BOOT_CMD_REPORT_ID;    /* Report:ReportID */
	command[len++] = BOOT_ERASE_DATAMEM;	/* Report:erase datamem command */
	command[len++] = ECH = BOOT_ERASE_DATAMEM;	/* Report:echo */
	command[len++] = DATAMEM_SECTOR0;		/* Report:erased block No. */

	sum = 0;
	for (j = 4; j < 12; j++)
		sum += command[j];
	cmd_chksum = ~sum + 1;					/* Report:check sum */
	command[len++] = cmd_chksum;

	udelay(50);

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev, "%s: failing 1: %d \n", __func__, ret);
		return false;
	}

	udelay(50);

	do {
		len = 0;
		command[len++] = 4;
		command[len++] = 0;
		command[len++] = 0x38;
		command[len++] = CMD_GET_FEATURE;
		command[len++] = 5;
		command[len++] = 0;

		ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev, "%s: failing 2:%d \n", __func__, ret);
			return false;
		}

		udelay(50);

		ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev, "%s: failing 3:%d \n", __func__, ret);
			return false;
		}

		if ((response[3] != 0x0e || response[4] != ECH) || (response[5] != 0xff && response[5] != 0x00))
			return false;

		udelay(50);

	} while (response[3] == 0x0e && response[4] == ECH && response[5] == 0xff);

	msleep(50);

	return true;
}

static bool erase_codemem(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	u8 command[CMD_SIZE];
	u8 response[BOOT_RSP_SIZE];
	unsigned char sum = 0;
	unsigned char cmd_chksum;
	int ret, ECH;
	int len = 0;
	int i, j;

	dev_info(&wac_i2c->client->dev,
		"%s: total erase code-block number = %d\n", __func__, num);

	for (i = 0; i < num; i++) {
		len = 0;

		command[len++] = 4;			/* Command Register-LSB */
		command[len++] = 0;			/* Command Register-MSB */
		command[len++] = 0x37;			/* Command-LSB, ReportType:Feature(11) ReportID:7 */
		command[len++] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
		command[len++] = 5;			/* Data Register-LSB */
		command[len++] = 0;			/* Data-Register-MSB */
		command[len++] = 7;			/* Length Field-LSB */
		command[len++] = 0;			/* Length Field-MSB */
		command[len++] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;	/* Report:erase command */
		command[len++] = ECH = i;		/* Report:echo */
		command[len++] = *eraseBlock;		/* Report:erased block No. */
		eraseBlock++;

		sum = 0;
		for (j = 4; j < 12; j++)
			sum += command[j];
		cmd_chksum = ~sum + 1;			/* Report:check sum */
		command[len++] = cmd_chksum;

		udelay(50);

		ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev, "%s: failing 1:%d \n", __func__, i);
			return false;
		}

		udelay(50);

		do {
			len = 0;
			command[len++] = 4;
			command[len++] = 0;
			command[len++] = 0x38;
			command[len++] = CMD_GET_FEATURE;
			command[len++] = 5;
			command[len++] = 0;

			ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
			if (ret < 0) {
				dev_err(&wac_i2c->client->dev, "%s: failing 2:%d \n", __func__, i);
				return false;
			}

			udelay(50);

			ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
				    WACOM_I2C_MODE_BOOT);
			if (ret < 0) {
				printk("%s failing 3:%d \n", __func__, i);
				return false;
			}

			if ((response[3] != 0x00 || response[4] != ECH) || (response[5] != 0xff && response[5] != 0x00))
				return false;

			udelay(50);

		} while (response[3] == 0x00 && response[4] == ECH && response[5] == 0xff);

		udelay(50);

	}

	msleep(50);

	return true;
}

static bool flash_erase_w9012(struct wacom_i2c *wac_i2c, int *eraseBlock, int num)
{
	bool ret;

	ret = erase_datamem(wac_i2c);
	if (!ret) {
		dev_err(&wac_i2c->client->dev, "%s: erasing datamem failed\n",
				__func__);
		return false;
	}

	ret = erase_codemem(wac_i2c, eraseBlock, num);
	if (!ret) {
		dev_err(&wac_i2c->client->dev, "%s: erasing codemem failed\n",
				__func__);
		return false;
	}

	return true;
}

static bool flash_write_block_w9012(struct wacom_i2c *wac_i2c, char *flash_data,
				    unsigned long ulAddress, u8 *pcommand_id, int *ECH)
{
	const int MAX_COM_SIZE = (16 + FLASH_BLOCK_SIZE + 2);
	/* 16: num of command[0] to command[15]
	** FLASH_BLOCK_SIZE: unit to erase the block
	** Num of Last 2 checksums
	*/

	u8 command[300];
	unsigned char sum = 0;
	int ret, i;

	command[0] = 4;		/* Command Register-LSB */
	command[1] = 0;		/* Command Register-MSB */
	command[2] = 0x37;	/* Command-LSB, ReportType:Feature(11) ReportID:7 */
	command[3] = CMD_SET_FEATURE;	/* Command-MSB, SET_REPORT */
	command[4] = 5;			/* Data Register-LSB */
	command[5] = 0;			/* Data-Register-MSB */
	command[6] = 76;		/* Length Field-LSB */
	command[7] = 0;			/* Length Field-MSB */
	command[8] = BOOT_CMD_REPORT_ID;	/* Report:ReportID */
	command[9] = BOOT_WRITE_FLASH;		/* Report:program  command */
	command[10] = *ECH = ++(*pcommand_id);	/* Report:echo */
	command[11] = ulAddress & 0x000000ff;
	command[12] = (ulAddress & 0x0000ff00) >> 8;
	command[13] = (ulAddress & 0x00ff0000) >> 16;
	command[14] = (ulAddress & 0xff000000) >> 24;	/* Report:address(4bytes) */
	command[15] = 8;				/* Report:size(8*8=64) */

	sum = 0;
	for (i = 4; i < 16; i++)
		sum += command[i];
	command[MAX_COM_SIZE - 2] = ~sum+1;		/* Report:command checksum */

	sum = 0;
	for (i = 16; i < (FLASH_BLOCK_SIZE + 16); i++) {
		command[i] = flash_data[ulAddress+(i-16)];
		sum += flash_data[ulAddress+(i-16)];
	}

	command[MAX_COM_SIZE - 1] = ~sum+1;		/* Report:data checksum */

	udelay(50);

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command,
			(BOOT_CMD_SIZE + 4), WACOM_FLASH_W9012);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev, "%s: 1 ret:%d \n",
						__func__, ret);
		return false;
	}

	udelay(50);

	return true;
}

static bool flash_write_w9012(struct wacom_i2c *wac_i2c, unsigned char *flash_data,
			      unsigned long start_address, unsigned long *max_address)
{
	u8 command_id = 0;
	u8 command[BOOT_RSP_SIZE];
	u8 response[BOOT_RSP_SIZE];
	int ret, i, j, len, ECH = 0, ECH_len = 0;
	int ECH_ARRAY[3];
	unsigned long ulAddress;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);
	j = 0;
	for (ulAddress = start_address; ulAddress < *max_address;
					ulAddress += FLASH_BLOCK_SIZE) {
		for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
			if (flash_data[ulAddress+i] != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE))
			continue;

		if (!flash_write_block_w9012(wac_i2c,
				flash_data, ulAddress, &command_id, &ECH))
			return false;

		if (ECH_len == 3)
			ECH_len = 0;

		ECH_ARRAY[ECH_len++] = ECH;

		if (ECH_len == 3) {
			for (j = 0; j < 3; j++) {
				do {
					len = 0;
					command[len++] = 4;
					command[len++] = 0;
					command[len++] = 0x38;
					command[len++] = CMD_GET_FEATURE;
					command[len++] = 5;
					command[len++] = 0;

					udelay(50);

					ret = wac_i2c->wacom_i2c_send(wac_i2c,
							command, len, WACOM_FLASH_W9012);
					if (ret < 0) {
						dev_err(&wac_i2c->client->dev,
							"%s: failing 2:%d; system returning: %d\n",
							__func__, i, ret);
						return false;
					}

					udelay(50);

					ret = wac_i2c->wacom_i2c_recv(wac_i2c,
							response, BOOT_RSP_SIZE, WACOM_FLASH_W9012);
					if (ret < 0) {
						dev_err(&wac_i2c->client->dev,
							"%s: failing 3:%d system returning %d \n",
							__func__, i, ret);
						return false;
					}

					if ((response[3] != 0x01 || response[4] != ECH_ARRAY[j])
							|| (response[5] != 0xff && response[5] != 0x00))
						return false;

					udelay(50);

				} while (response[3] == 0x01 && response[4]
						== ECH_ARRAY[j] && response[5] == 0xff);
			}
		}
	}
	return true;
}

int wacom_i2c_flash(struct wacom_i2c *wac_i2c)
{
	unsigned long max_address = 0;
	unsigned long start_address;
	int i, ret = 0;
	int eraseBlock[200], eraseBlockNum;

	if (wac_i2c->ic_mpu_ver != MPU_W9007 && wac_i2c->ic_mpu_ver != MPU_W9010 && wac_i2c->ic_mpu_ver != MPU_W9012)
		return -EXIT_FAIL_GET_MPU_TYPE;

	wac_i2c->compulsory_flash_mode(wac_i2c, true);
	/*Reset*/
	wac_i2c->reset_platform_hw(wac_i2c);
	msleep(300);
	dev_err(&wac_i2c->client->dev, "%s: Set FWE\n", __func__);

	ret = wacom_enter_bootloader(wac_i2c);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed to enter bootloader mode\n", __func__);
		goto flashing_fw_err;
	}

	dev_err(&wac_i2c->client->dev,
			"%s: enter bootloader mode, erase & writing firmware\n", __func__);

	/*Set start and end address and block numbers*/
	eraseBlockNum = 0;
	start_address = START_ADDR_W9012;
	max_address = MAX_ADDR_W9012;

	for (i = BLOCK_NUM_W9012; i >= 8; i--) {
		eraseBlock[eraseBlockNum] = i;
		eraseBlockNum++;
	}

	/*Erase the old program code */
	if (!flash_erase_w9012(wac_i2c, eraseBlock, eraseBlockNum)) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed erase old firmware, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	/*Write the new program */
	if (!flash_write_w9012(wac_i2c, wac_i2c->fw_data,
				start_address, &max_address)) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed writing new firmware, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	/*Enable */
	ret = wacom_flash_end(wac_i2c);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed flash mode close, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	dev_err(&wac_i2c->client->dev,
				"%s: Successed new Firmware writing\n",
				__func__);

flashing_fw_err:
	wac_i2c->compulsory_flash_mode(wac_i2c, false);
	msleep(150);

	return ret;
}

int wacom_i2c_firm_update(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	int retry = 3;

	while (retry--) {
		ret = wacom_i2c_flash(wac_i2c);
		if (ret < 0)
			dev_err(&wac_i2c->client->dev,
				"%s: failed to write firmware(%d)\n",
				__func__, ret);
		else
			dev_err(&wac_i2c->client->dev,
				"%s: Successed to write firmware(%d)\n",
				__func__, ret);
		/* Reset IC */
		wac_i2c->reset_platform_hw(wac_i2c);
		if (ret >= 0)
			return 0;
	}

	return ret;
}

int wacom_fw_load_from_UMS(struct wacom_i2c *wac_i2c)
	{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	const struct firmware *firm_data = NULL;

	dev_info(&wac_i2c->client->dev,
			"%s: Start firmware flashing (UMS).\n", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_UMS_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		dev_err(&wac_i2c->client->dev,
			"%s: failed to open %s.\n",
			__func__, WACOM_UMS_FW_PATH);
		ret = -ENOENT;
		goto open_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	dev_info(&wac_i2c->client->dev,
		"%s: start, file path %s, size %ld Bytes\n",
		__func__, WACOM_UMS_FW_PATH, fsize);

	firm_data = kzalloc(fsize, GFP_KERNEL);
	if (IS_ERR(firm_data)) {
		dev_err(&wac_i2c->client->dev,
			"%s, kmalloc failed\n", __func__);
			ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)firm_data,
		fsize, &fp->f_pos);

	dev_info(&wac_i2c->client->dev,
		"%s: nread %ld Bytes\n", __func__, nread);
	if (nread != fsize) {
		dev_err(&wac_i2c->client->dev,
			"%s: failed to read firmware file, nread %ld Bytes\n",
			__func__, nread);
		ret = -EIO;
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);
	/*start firm update*/
	wac_i2c->fw_data=(unsigned char *)firm_data;

	return 0;

read_err:
	kfree(firm_data);
malloc_error:
	filp_close(fp, current->files);
open_err:
	set_fs(old_fs);
	return ret;
}

int wacom_load_fw_from_req_fw(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	const struct firmware *firm_data = NULL;
	char fw_path[WACOM_MAX_FW_PATH];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	/*Obtain MPU type: this can be manually done in user space */
	dev_info(&wac_i2c->client->dev,
			"%s: MPU type: %x , BOOT ver: %x\n",
			__func__, wac_i2c->ic_mpu_ver, wac_i2c->boot_ver);

	memset(&fw_path, 0, WACOM_MAX_FW_PATH);
	if (wac_i2c->ic_mpu_ver == MPU_W9012) {
			snprintf(fw_path, WACOM_MAX_FW_PATH,
				"%s", WACOM_FW_NAME_W9012);
	} else {
		dev_info(&wac_i2c->client->dev,
			"%s: firmware name is NULL. return -1\n",
			__func__);
		ret = -ENOENT;
		goto firm_name_null_err;
	}

	ret = request_firmware(&firm_data, fw_path, &wac_i2c->client->dev);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
		       "%s: Unable to open firmware. ret %d\n",
				__func__, ret);
		goto request_firm_err;
	}

	dev_info(&wac_i2c->client->dev, "%s: firmware name: %s, size: %d\n",
			__func__, fw_path, firm_data->size);

	/* firmware version check */
	if (wac_i2c->ic_mpu_ver == MPU_W9010 || wac_i2c->ic_mpu_ver == MPU_W9007 || wac_i2c->ic_mpu_ver == MPU_W9012)
		wac_i2c->wac_query_data->fw_version_bin =
			(firm_data->data[FIRM_VER_LB_ADDR_W9012] |
			(firm_data->data[FIRM_VER_UB_ADDR_W9012] << 8));

	dev_info(&wac_i2c->client->dev, "%s: firmware version = %x\n",
			__func__, wac_i2c->wac_query_data->fw_version_bin);
	wac_i2c->fw_data=(unsigned char *)firm_data->data;
	release_firmware(firm_data);

firm_name_null_err:
request_firm_err:
	return ret;
}

int wacom_i2c_usermode(struct wacom_i2c *wac_i2c)
{
	int ret;
	bool bRet = false;

	wac_i2c->compulsory_flash_mode(wac_i2c, true);

	ret = wacom_enter_flash_mode(wac_i2c);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
			"%s cannot send flash command at user-mode \n", 
			__func__);
		return ret;
	}

	/*Return to the user mode */
	dev_info(&wac_i2c->client->dev,
		"%s closing the boot mode \n", __func__);
	bRet = wacom_flash_end(wac_i2c);
	if (!bRet) {
		dev_err(&wac_i2c->client->dev,
			"%s closing boot mode failed  \n", __func__);
		ret = -EXIT_FAIL_WRITING_MARK_NOT_SET;
		goto end_usermode;
	}

	wac_i2c->compulsory_flash_mode(wac_i2c, false);

	dev_info(&wac_i2c->client->dev,
		"%s making user-mode completed \n", __func__);
	ret = EXIT_OK;


 end_usermode:
	return ret;
}

