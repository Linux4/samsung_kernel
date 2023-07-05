#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/i2c.h>

#include "cam_ois_define.h"
#include "cam_ois_i2c.h"
#include "cam_ois_aois_if.h"
#include "cam_ois_common.h"

#define OIS_I2C_RETRY_COUNT 2

void cam_ois_i2c_set_client_addr(struct i2c_client *i2c_client_in, int subaddr)
{
	i2c_client_in->addr = subaddr >> 1;
}

int cam_ois_i2c_master_send(struct i2c_client *client, char *buf, int count)
{
#if ENABLE_AOIS == 0
	cam_ois_i2c_set_client_addr(client, MCU_I2C_SLAVE_W_ADDR);
	return i2c_master_send(client, buf, count);
#else
	LOG_INF("AOIS i2c_master_send dummy");
	return 0;
#endif
}

int cam_ois_i2c_master_recv(struct i2c_client *client, char *buf, int count)
{
#if ENABLE_AOIS == 0
	cam_ois_i2c_set_client_addr(client, MCU_I2C_SLAVE_W_ADDR);
	return i2c_master_recv(client, buf, count);
#else
	LOG_INF("AOIS i2c_master_recv dummy");
	return 0;
#endif
}

int cam_ois_i2c_write(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)a_u2Addr, (char)a_u2Data};

	cam_ois_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_send(client, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}
int cam_ois_i2c_write_one(struct i2c_client *client, int subaddr, u16 addr, u8 data)
{
	return cam_ois_i2c_write_multi(client, subaddr, addr, &data, 1);
}

int cam_ois_i2c_write_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size)
{
	int retries = OIS_I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	unsigned long i = 0;
	unsigned char buf[258] = {0,};
	struct i2c_msg msg = {
				.flags  = 0,
				.len    = size + 2,
				.buf    = buf,
				.addr   = subaddr >> 1,
	};

	if (client == NULL) {
		LOG_ERR("[%s]i2c clinet is NULL\n", __func__);
		return -EIO;
	}

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size; i++)
		buf[i + 2] = *(data + i);

	do {
		client->addr = subaddr >> 1;
#if ENABLE_AOIS == 0
		ret = i2c_transfer(client->adapter, &msg, 1);
#else
		LOG_DBG("AOIS write, client addr 0x%x", msg.addr);
		ret = cam_ois_cmd_notifier_call_chain(0, addr, data, size);
#endif
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

	//LOG_DBG("[KYH OIS_I2C_Write_Success! Addr : [%04X] , Data : [%04X]\n",addr, *data);

	return 0;
}

int cam_ois_i2c_read(struct i2c_client *client, int subaddr, u16 a_u2Addr, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[1] = {(char)(a_u2Addr)};

	if (client == NULL) {
		LOG_ERR("[%s]i2c clinet is NULL\n", __func__);
		return -1;
	}

	cam_ois_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_send(client, puReadCmd, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}
	cam_ois_i2c_set_client_addr(client, subaddr);
	i4RetValue = i2c_master_recv(client, (char *)a_puBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}

	return 0;
}

int cam_ois_i2c_read_multi(struct i2c_client *client, int subaddr, u16 addr, u8 *data, size_t size)
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

#if ENABLE_AOIS == 0
	client->addr = subaddr >> 1;
	err = i2c_transfer(client->adapter, msg, 2);
#else
	LOG_DBG("AOIS read, client addr 0x%x", msg[0].addr);
	err = cam_ois_reg_read_notifier_call_chain(0, addr, rxbuf, size);
#endif
	if (unlikely(err != 2)) {
		LOG_ERR("register read fail. err: %d\n", err);
		return -EIO;
	}
	memcpy(data, rxbuf, size);

	return 0;
}

int cam_ois_i2c_read_two(struct i2c_client *client, int subaddr, u16 addr, u16 *val)
{
	int ret = 0;
	u8 data[2] = {0, };

	ret = cam_ois_i2c_read_multi(client, subaddr, addr, data, 2);
	if (ret != 0) {
		*val = 0;
		LOG_ERR("i2c read fail\n");
	} else {
		*val = (data[1] << 8) | data[0];
	}
	return ret;
}
