/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>

#include "is-helper-ixc.h"
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#ifdef IS_VIRTUAL_MODULE
static u8 emul_reg[1024*20] = {0};
static bool init_reg = false;

static void i2c_init()
{
	if (init_reg == false) {
		memset(emul_reg, 0x1, sizeof(emul_reg));
		init_reg = true;
	}
}

static int i2c_emulator(struct i2c_adapter *adapter, struct i2c_msg *msg, u32 size)
{
	int ret = 0;
	int addr = 0;

	i2c_init();
	if (size == IXC_READ_SIZE) { /* READ */
		if (msg[0].len == 1) {
			addr = *(msg[0].buf);
		} else if (msg[0].len == 2) {
			addr = (msg[0].buf[0] << 8 | msg[0].buf[1]);
		}

		if (msg[1].len == 1) {
			msg[1].buf[0] = emul_reg[addr];
		} else if (msg[1].len == 2) {
			msg[1].buf[0] = emul_reg[addr];
			msg[1].buf[1] = emul_reg[addr+1];
		}
	} else if (size == IXC_WRITE_SIZE) { /* WRITE */
		if (msg[0].len == 2) {
			addr = msg[0].buf[0];
			emul_reg[addr] = msg[0].buf[1];
		} else if (msg[0].len == 3) {
			addr = (msg[0].buf[0] << 8 | msg[0].buf[1]);
			emul_reg[addr] = msg[0].buf[2];
		} else if (msg[0].len == 4) {
			addr = (msg[0].buf[0] << 8 | msg[0].buf[1]);
			emul_reg[addr] = msg[0].buf[2];
			emul_reg[addr+1] = msg[0].buf[3];
		}
	} else {
		pr_err("err, unknown size(%d)\n", size);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}
#endif

static int i2c_check_adapter(struct i2c_client *client)
{
	if (!client->adapter) {
		pr_err("Could not find adapter!\n");
		return -ENODEV;
	}

	return 0;
}

int is_i2c_transfer(struct i2c_adapter *adapter, struct i2c_msg *msg, u32 size)
{
	int ret = 0;
#ifdef IS_VIRTUAL_MODULE
	ret = i2c_emulator(adapter, msg, size);
#else
	ret = i2c_transfer(adapter, msg, size);
#endif

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (ret < 0) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=camera@INFO=i2c_fail");
#else
		sec_abc_send_event("MODULE=camera@WARN=i2c_fail");
#endif
		pr_err("i2c transfer fail(%d)", ret);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(is_i2c_transfer);

static int i2c_fill_msg_addr8_read8(struct i2c_client *client,
	u8 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf)
{
	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 1;
	msg[0].buf = wbuf;
	wbuf[0] = addr;

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	return 0;
}

static int is_sensor_addr8_read8(void *client,
	u8 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_READ_SIZE];
	u8 wbuf[1];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_addr8_read8(cur_client, addr, val, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_READ_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CR08(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, *val);
	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_read8(struct i2c_client *client,
	u16 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf)
{
	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	return 0;
}

static int is_sensor_read8(void *client,
	u16 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_READ_SIZE];
	u8 wbuf[2];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_read8(cur_client, addr, val, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_READ_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CR08(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, *val);
	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_read16(struct i2c_client *client,
	u16 addr, struct i2c_msg *msg, u8 *wbuf, u8 *rbuf)
{
	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rbuf;

	return 0;
}

static int is_sensor_read16(void *client,
	u16 addr, u16 *val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_READ_SIZE];
	u8 wbuf[2], rbuf[2] = {0, 0};
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_read16(cur_client, addr, msg, wbuf, rbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_READ_SIZE);
	if (ret < 0)
		goto p_err;

	*val = ((rbuf[0] << 8) | rbuf[1]);

	ixc_info("I2CR16(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, *val);

	return 0;
p_err:
	return ret;
}

#define ADDR_SIZE	2
static int is_sensor_read8_size(void *client, void *buf,
	u16 addr, size_t size)
{
	int ret = 0;
	u8 addr_buf[ADDR_SIZE];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		return ret;

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	ret = i2c_master_send(cur_client, addr_buf, ADDR_SIZE);
	if (ret != ADDR_SIZE) {
		pr_err("%s: failed to i2c send(%d)\n", __func__, ret);
		return ret;
	}

	/* Receive data */
	ret = i2c_master_recv(cur_client, buf, size);
	if (ret != size) {
		pr_err("%s: failed to i2c receive ret(%d), size(%lu)\n", __func__, ret, size);
		return ret;
	}

	return ret;
}

static int i2c_fill_msg_addr8_write8(struct i2c_client *client,
	u8 addr, u8 val, struct i2c_msg *msg, u8 *wbuf)
{
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = wbuf;
	wbuf[0] = addr;
	wbuf[1]  = val;

	return 0;
}

static int is_sensor_addr8_write8(void *client,
	u8 addr, u8 val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[2];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_addr8_write8(cur_client, addr, val, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CR08(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, val);
	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_write8(struct i2c_client *client,
	u16 addr, u8 val, struct i2c_msg *msg, u8 *wbuf)
{
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = val;

	return 0;
}

static int is_sensor_write8(void *client,
	u16 addr, u8 val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[3];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_write8(cur_client, addr, val, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CW08(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, val);

	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_write8_array(struct i2c_client *client,
	u16 addr, u8 *val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	int i = 0;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 1);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	for (i = 0; i < num; i++)
		wbuf[(i * 1) + 2] = val[i];

	return 0;
}

static int is_sensor_write8_array(void *client,
	u16 addr, u8 *val, u32 num)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[10];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (num > 8) {
		pr_err("currently limit max num is 8, need to fix it!\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_write8_array(cur_client, addr, val, num, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CW08(%d) [0x%04x] : 0x%04x\n", cur_client->addr, addr, *val);

	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_write16(struct i2c_client *client,
	u16 addr, u16 val, struct i2c_msg *msg, u8 *wbuf)
{
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = (val & 0xFF00) >> 8;
	wbuf[3] = (val & 0xFF);

	return 0;
}

static int is_sensor_write16(void *client,
	u16 addr, u16 val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[4];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_write16(cur_client, addr, val, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CW16(%d) [0x%04X] : 0x%04X\n", cur_client->addr, addr, val);

	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_write16_array(struct i2c_client *client,
	u16 addr, u16 *val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	int i = 0;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	for (i = 0; i < num; i++) {
		wbuf[(i * 2) + 2] = (val[i] & 0xFF00) >> 8;
		wbuf[(i * 2) + 3] = (val[i] & 0xFF);
	}

	return 0;
}

static int is_sensor_write16_array(void *client,
	u16 addr, u16 *val, u32 num)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[10];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (num > 4) {
		pr_err("currently limit max num is 4, need to fix it!\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_write16_array(cur_client, addr, val, num, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CW16(%d) [0x%04x] : 0x%04x\n", cur_client->addr, addr, *val);

	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_write16_burst(struct i2c_client *client,
	u16 addr, u16 *val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	int i = 0;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	for (i = 0; i < num; i++) {
		wbuf[(i * 2) + 2] = (val[(i * 2) * IXC_NEXT] & 0xFF00) >> 8;
		wbuf[(i * 2) + 3] = (val[(i * 2) * IXC_NEXT] & 0xFF);
	}

	return 0;
}

static int is_sensor_write16_burst(void *client,
	u16 addr, u16 *val, u32 num)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 *wbuf;
	struct i2c_client *cur_client = (struct i2c_client *)client;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i2c\n");
		ret = -ENODEV;
		goto p_err;
	}

	i2c_fill_msg_write16_burst(cur_client, addr, val, num, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err_free;

	ixc_info("I2CW16(%d) [0x%04x] : 0x%04x\n", cur_client->addr, addr, *val);
	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}

static int i2c_fill_msg_write8_sequential(struct i2c_client *client,
	u16 addr, u8 *val, u16 num, struct i2c_msg *msg, u8 *wbuf)
{
	int i = 0;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 1);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	for (i = 0; i < num; i++)
		wbuf[(i * 1) + 2] = val[i];

	return 0;
}

static int is_sensor_write8_sequential(void *client,
	u16 addr, u8 *val, u16 num)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 *wbuf;
	struct i2c_client *cur_client = (struct i2c_client *)client;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	wbuf = kzalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i2c\n");
		ret = -ENODEV;
		goto p_err;
	}

	i2c_fill_msg_write8_sequential(cur_client, addr, val, num, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err_free;

	ixc_info("I2CW08(%d) [0x%04x] : 0x%04x\n", cur_client->addr, addr, *val);
	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}

static int i2c_fill_msg_data_read16(struct i2c_client *client,
	struct i2c_msg *msg, u8 *wbuf)
{
	/* 1. I2C operation for reading data */
	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;

	return 0;
}

static int is_sensor_data_read16(void *client,
		u16 *val)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[2] = {0, 0};
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_data_read16(cur_client, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	*val = ((wbuf[0] << 8) | wbuf[1]);

	ixc_info("I2CR16(%d) : 0x%08X\n", cur_client->addr, *val);

	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_data_write16(struct i2c_client *client,
	u8 val_high, u8 val_low, struct i2c_msg *msg, u8 *wbuf)
{
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = wbuf;
	wbuf[0] = val_high;
	wbuf[1] = val_low;

	return 0;
}

static int is_sensor_data_write16(void *client,
		u8 val_high, u8 val_low)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[2];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_data_write16(cur_client, val_high, val_low, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CR08(%d) : 0x%04X%04X\n", cur_client->addr, val_high, val_low);
	return 0;
p_err:
	return ret;
}

static int i2c_fill_msg_addr_data_write16(struct i2c_client *client,
	u8 addr, u8 val_high, u8 val_low, struct i2c_msg *msg, u8 *wbuf)
{
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = wbuf;
	wbuf[0] = addr;
	wbuf[1] = val_high;
	wbuf[2] = val_low;

	return 0;
}

static int is_sensor_addr_data_write16(void *client,
		u8 addr, u8 val_high, u8 val_low)
{
	int ret = 0;
	struct i2c_msg msg[IXC_WRITE_SIZE];
	u8 wbuf[3];
	struct i2c_client *cur_client = (struct i2c_client *)client;

	ret = i2c_check_adapter(cur_client);
	if (ret < 0)
		goto p_err;

	i2c_fill_msg_addr_data_write16(cur_client, addr, val_high, val_low, msg, wbuf);
	ret = is_i2c_transfer(cur_client->adapter, msg, IXC_WRITE_SIZE);
	if (ret < 0)
		goto p_err;

	ixc_info("I2CW16(%d) : 0x%04X%04X\n", cur_client->addr, val_high, val_low);

	return 0;
p_err:
	return ret;
}

static struct is_ixc_ops i2c_ops = {
	.addr8_read8 = is_sensor_addr8_read8,
	.read8 = is_sensor_read8,
	.read16 = is_sensor_read16,
	.read8_size = is_sensor_read8_size,
	.addr8_write8 = is_sensor_addr8_write8,
	.write8 = is_sensor_write8,
	.write8_array = is_sensor_write8_array,
	.write16 = is_sensor_write16,
	.write16_array = is_sensor_write16_array,
	.write16_burst = is_sensor_write16_burst,
	.write8_sequential = is_sensor_write8_sequential,
	.data_read16 = is_sensor_data_read16,
	.data_write16 = is_sensor_data_write16,
	.addr_data_write16 = is_sensor_addr_data_write16,
};

struct is_ixc_ops *pablo_get_i2c(void)
{
	return &i2c_ops;
}
EXPORT_SYMBOL_GPL(pablo_get_i2c);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_i2c_check_adapter(struct i2c_client *client)
{
	return i2c_check_adapter(client);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_check_adapter);
int pablo_kunit_i2c_fill_msg_addr8_read8(struct i2c_client *client,
	u8 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_addr8_read8(client, addr, val, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_addr8_read8);
int pablo_kunit_i2c_fill_msg_read8(struct i2c_client *client,
	u16 addr, u8 *val, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_read8(client, addr, val, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_read8);
int pablo_kunit_i2c_fill_msg_read16(struct i2c_client *client,
	u16 addr, struct i2c_msg *msg, u8 *wbuf, u8 *rbuf)
{
	return i2c_fill_msg_read16(client, addr, msg, wbuf, rbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_read16);
int pablo_kunit_i2c_fill_msg_data_read16(struct i2c_client *client,
	struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_data_read16(client, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_data_read16);
int pablo_kunit_i2c_fill_msg_addr8_write8(struct i2c_client *client,
	u8 addr, u8 val, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_addr8_write8(client, addr, val, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_addr8_write8);
int pablo_kunit_i2c_fill_msg_write8(struct i2c_client *client,
	u16 addr, u8 val, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_write8(client, addr, val, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write8);
int pablo_kunit_i2c_fill_msg_write8_array(struct i2c_client *client,
	u16 addr, u8 *val, u32 num, struct i2c_msg *msg, u8 *wbuf) {
	return i2c_fill_msg_write8_array(client, addr, val, num, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write8_array);
int pablo_kunit_i2c_fill_msg_write16(struct i2c_client *client,
	u16 addr, u16 val, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_write16(client, addr, val, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write16);
int pablo_kunit_i2c_fill_msg_data_write16(struct i2c_client *client,
	u8 val_h, u8 val_l, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_data_write16(client, val_h, val_l, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_data_write16);
int pablo_kunit_i2c_fill_msg_write16_array(struct i2c_client *client,
	u16 addr, u16 *arr_val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_write16_array(client, addr, arr_val, num, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write16_array);
int pablo_kunit_i2c_fill_msg_write16_burst(struct i2c_client *client,
	u16 addr, u16 *arr_val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_write16_burst(client, addr, arr_val, num, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write16_burst);
int pablo_kunit_i2c_fill_msg_write8_sequential(struct i2c_client *client,
	u16 addr, u8 *val, u32 num, struct i2c_msg *msg, u8 *wbuf)
{
	return i2c_fill_msg_write8_sequential(client, addr, val, num, msg, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i2c_fill_msg_write8_sequential);
#endif
