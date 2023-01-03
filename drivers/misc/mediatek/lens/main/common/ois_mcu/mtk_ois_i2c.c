#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/i2c.h>

#include "mtk_ois_define.h"
#include "mtk_ois_i2c.h"

void ois_mcu_i2c_set_client_addr(struct i2c_client *i2c_client_in, int subaddr)
{
	i2c_client_in->addr = subaddr >> 1;
}

int ois_mcu_i2c_write(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)a_u2Addr, (char)a_u2Data};

	ois_mcu_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_send(client, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}
int ois_mcu_i2c_write_one(struct i2c_client *client, int subaddr, u16 addr, u8 data)
{
	return ois_mcu_i2c_write_multi(client, subaddr, addr, &data, 3);
}

int ois_mcu_i2c_write_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size)
{
	int retries = OIS_I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	unsigned long i = 0;
	unsigned char buf[258] = {0,};
	struct i2c_msg msg = {
				.flags  = 0,
				.len    = size,
				.buf    = buf,
				.addr   = subaddr >> 1,
	};

	if (client == NULL) {
		LOG_ERR("[%s]i2c clinet is NULL\n", __func__);
		return -EIO;
	}

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size - 2; i++)
		buf[i + 2] = *(data + i);

	do {
		client->addr = subaddr >> 1;
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;

		usleep_range(10000, 11000);
		err = ret;
	} while (--retries > 0);

	/* Retry occurred */
	if (unlikely(retries < OIS_I2C_RETRY_COUNT)) {
		LOG_ERR("i2c_write: error %d, write (%04X, %04X), retry %d\n",
				err, addr, *data, retries);
	}

	if (unlikely(ret != 1)) {
		LOG_ERR("I2C does not work\n");
		return -EIO;
	}

	return 0;
}

int ois_mcu_i2c_read(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[1] = {(char)(a_u2Addr)};

	if (client == NULL) {
		LOG_ERR("[%s]i2c clinet is NULL\n", __func__);
		return -1;
	}

	ois_mcu_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_send(client, puReadCmd, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}
	ois_mcu_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_recv(client, (char *)a_puBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}

	return 0;
}

int ois_mcu_i2c_read_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size)
{
	int err;
	u8 rxbuf[256], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = subaddr >> 1;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = subaddr >> 1;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = rxbuf;

	client->addr = subaddr >> 1;
	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		LOG_ERR("register read fail\n");
		return -EIO;
	}

	memcpy(data, rxbuf, size);
	return 0;
}
