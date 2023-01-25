#include "next_fw_update.h"

static int next_i2c_write_flash_in(struct i2c_client *client,
		unsigned char deviceID, unsigned short slaveAddr,
		unsigned char *data, unsigned char length)
{
	int ret;
	u8 w_data[NEXT_MAX_NUM_WR];

	client->addr = deviceID;

	w_data[0] = slaveAddr >> 8;
	w_data[1] = slaveAddr &= 0xFF;
	w_data[2] = data[0];

	ret = i2c_master_send(client, w_data, length + 2);

	printk(KERN_ERR "%s: nextchip: return i2c_write data= %d\n",
			__func__, ret);

	return ret;
}

static int next_i2c_write(struct i2c_client *client,
		unsigned char deviceID, unsigned short slaveAddr,
		const u8 *data, unsigned char length)
{
	int ret, i;

	u8 w_data[NEXT_MAX_NUM_WR];


	client->addr = deviceID;

	w_data[0] = slaveAddr >> 8;
	w_data[1] = slaveAddr &= 0xFF;

	for (i = 0; i < length; i++)
		w_data[i+2] = data[i];

	ret = i2c_master_send(client, w_data, length + 2);

	printk(KERN_ERR "%s: nextchip: return i2c_write data= %d\n",
			__func__, ret);

	return ret;
}

static int next_i2c_read(struct i2c_client *client,
		unsigned char deviceID, unsigned short slaveAddr,
		unsigned char *data, unsigned char length)
{
	int ret, i;
	u8 w_data[NEXT_MAX_NUM_WR];

	client->addr = deviceID;

	w_data[0] = slaveAddr >> 8;
	w_data[1] = slaveAddr &= 0xFF;

	for (i = 0; i < length; i++)
		w_data[i + 2] = data[i];

	ret = i2c_master_send(client, w_data, 2);
	if (!ret)
		return 0;

	ret = i2c_master_recv(client, data, length);
	if (!ret)
		return 0;

	return 1;
}

static int next_enter_flash_mode(struct i2c_client *client)
{
	unsigned char	tmpBuf[3];
	int ret;

	tmpBuf[2] = 0;
	tmpBuf[0] = 0x2D;
	printk(KERN_ERR "%s: nextchip! start first function.!\n", __func__);

	/* Nextchip IC
	* In FW version under 0x05,
	* have to send flash mode command within 1msec, after power on.
	* it will be removed after FW version 0x06
	*/
	ret = next_i2c_write_flash_in(client, NEXT_DEVICEID_FLASH_IN,
				NEXT_REG_TEST_ENTER, tmpBuf, 1);
	if (ret < 0)
		return 0;

	msleep(30);

	tmpBuf[0] = 0x81;
	ret = next_i2c_write_flash_in(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_EFLASH_GPROTECT, tmpBuf, 1);

	tmpBuf[1] = 0xFF;
	ret = next_i2c_write_flash_in(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_ERASE_PROTECT, &tmpBuf[1], 1);

	tmpBuf[2] = 0x00;
	ret = next_i2c_write_flash_in(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_ERASE_PROTECT, &tmpBuf[0], 0);

	ret = i2c_master_recv(client, &tmpBuf[2], 1);
	printk(KERN_ERR "%s: nextchip enter ret = %x\n", __func__, ret);

	return 1;
}

static void next_out_flash_mode(struct i2c_client *client)
{
	u8 tmpBuf[2];

	tmpBuf[0] = 0xE1;

	printk(KERN_ERR "%s start\n", __func__);

	next_i2c_write(client, NEXT_DEVICEID_FLASH_IN,
			NEXT_REG_TEST_ENTER, tmpBuf, 1);
}

static void next_erase_flash(struct i2c_client *client)
{
	int i;
	unsigned char tmpBuf[4];

	printk(KERN_ERR "%s start\n", __func__);

	for (i = 0; i < 16; i++) {
		tmpBuf[0] = 0x00;
		next_i2c_write(client, NEXT_DEVICEID_FLASH,
				i * 0x400, tmpBuf, 1);
		tmpBuf[0] = 0x0B;
		next_i2c_write(client, NEXT_DEVICEID_FLASH,
				NEXT_REG_ERASE, tmpBuf, 1);

		msleep(20);
	}
}


static void next_write_flash(struct i2c_client *client,
				const u8 *data, int size)
{
	int i;
	int tmpReminder = size % 64;

	printk(KERN_ERR "%s start\n", __func__);

	for (i = 0; i < size / 64; i++) {
		next_i2c_write(client, NEXT_DEVICEID_FLASH, i * 64,
				data + (i * 64), 64);
		usleep_range(1000, 1100);
	}

	if (tmpReminder) {
		next_i2c_write(client, NEXT_DEVICEID_FLASH, i * 64,
				data + (i * 64), tmpReminder);
		usleep_range(1000, 1100);
	}
}

/*
static void next_read_flash(struct i2c_client *client, unsigned char *readData)
{
	int		i;
	unsigned char	tmpBuf[2];

	printk(KERN_ERR "%s start\n", __func__);

	for (i = 0; i < NEXT_FW_SIZE / 64; i++) {
		tmpBuf[0] = 0x0A;
		next_i2c_write(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_ERASE_PROTECT_INFO, tmpBuf, 1);
		next_i2c_read(client, NEXT_DEVICEID_FLASH,
			i * 64, readData + (i * 64), 64);
		tmpBuf[0] = 0x09;
		next_i2c_write(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_ERASE_PROTECT_INFO, tmpBuf, 1);
	}
}
*/
static void next_flash_checksum(struct i2c_client *client,
			unsigned short *checkSum)
{
	unsigned char	tmpBuf[2];

	printk(KERN_ERR "%s start\n", __func__);

	next_i2c_read(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_CHKSUM_LOW, &tmpBuf[0], 1);
	next_i2c_read(client, NEXT_DEVICEID_FLASH,
			NEXT_REG_CHKSUM_HIGH, &tmpBuf[1], 1);

	*checkSum = (unsigned short)((unsigned short) tmpBuf[1] << 8) |
					(unsigned short) tmpBuf[0];
}

int next_i2c_fw_update(struct i2c_client *client)
{
	int i, ret, size;
	unsigned short CheckSum = 0, rdCheckSum = 0;
	const struct firmware *fw = NULL;

	printk(KERN_ERR "%s: request firmware\n", __func__);
	ret = request_firmware(&fw, NEXT_FW_NAME, &client->dev);
	if (ret) {
		printk(KERN_ERR "%s: unable request firmware\n", __func__);
		goto err_request_fw;
	}
	size = (int)fw->size;
	printk(KERN_ERR "%s: nextchip fw download go, fw size : %d!\n",
				__func__, size);


	ret = next_enter_flash_mode(client);
	if (!ret) {
		printk(KERN_ERR "%s: nextchip don't enter flash mode\n",
				__func__);
		goto err_writing;
	}

	for (i = 0; i < size; i++)
		CheckSum += fw->data[i];
	printk(KERN_ERR "%s: nextchip checksum = %04x\n", __func__, CheckSum);

	next_erase_flash(client);
	msleep(100);

	next_write_flash(client, fw->data, size);
	msleep(30);

	next_flash_checksum(client, &rdCheckSum);
	printk(KERN_ERR "%s: compare checksum: read :  %04x, header : %04x\n",
			__func__, rdCheckSum, CheckSum);

	next_out_flash_mode(client);

	client->addr = 0x20;

	printk(KERN_ERR "%s : nextchip fw done\n", __func__);
	release_firmware(fw);
	return 1;

err_writing:
	release_firmware(fw);
err_request_fw:
	return 0;
}
