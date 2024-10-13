// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos5 SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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

#if !IS_ENABLED(CONFIG_I3C)
static u8 emul_reg[1024*20] = {0};
static bool init_reg = false;

static void i3c_init()
{
	if (init_reg == false) {
		memset(emul_reg, 0x1, sizeof(emul_reg));
		init_reg = true;
	}
}

static int i3c_emulator(void *device, struct i3c_priv_xfer *xfers, u32 size)
{
	int ret = 0;
	int addr = 0;

	if (!device) {
		pr_err("Could not find device!\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (!xfers) {
		pr_err("Could not find xfers!\n");
		ret = -EINVAL;
		goto p_err;
	}

	i3c_init();

	if (size == IXC_READ_SIZE) { /* READ */
		if (xfers[0].len == 1)
			addr = (*(int *)(xfers[0].data.out));
		else if (xfers[0].len == 2)
			addr = (*((u8 *)&(xfers[0].data.out[0])) << 8)
				| *((u8 *)&(xfers[0].data.out[1]));
		if (xfers[1].len == 1) {
			*((u8 *)&(xfers[1].data.out[0])) = emul_reg[addr];
		} else if (xfers[1].len == 2) {
			*((u8 *)&(xfers[1].data.out[0])) = emul_reg[addr];
			*((u8 *)&(xfers[1].data.out[1])) = emul_reg[addr+1];
		}
	} else if (size == IXC_WRITE_SIZE) { /* WRITE */
		if (xfers[0].len == 2) {
			addr = *((u8 *)&(xfers[0].data.out[0]));
			emul_reg[addr] = *((u8 *)&(xfers[0].data.out[1]));
		} else if (xfers[0].len == 3) {
			addr = (*((u8 *)&(xfers[0].data.out[0])) << 8)
				| *((u8 *)&(xfers[0].data.out[1]));
			emul_reg[addr] = *((u8 *)&(xfers[0].data.out[2]));
		} else if (xfers[0].len == 4) {
			addr = (*((u8 *)&(xfers[0].data.out[0])) << 8)
				| *((u8 *)&(xfers[0].data.out[2]));
			emul_reg[addr] = *((u8 *)&(xfers[0].data.out[2]));
			emul_reg[addr+1] = *((u8 *)&(xfers[0].data.out[3]));
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

static int is_i3c_device_do_priv_xfers(struct i3c_device *device, struct i3c_priv_xfer *xfers, u32 size)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_I3C)
	ret = i3c_emulator(device, xfers, size);
#else
	ret = i3c_device_do_priv_xfers(device, xfers, size);
#endif

#if IS_ENABLED(CONFIG_SEC_ABC)
	if (ret < 0) {
		sec_abc_send_event("MODULE=camera@ERROR=i3c_fail");
		pr_err("i3c transfer fail(%d)", ret);
	}
#endif

	return ret;
}

static int i3c_fill_msg_addr8_read8(u8 addr, u8 *val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 1;

	/* 1. i3c operation for writing. */
	wbuf[0] = addr;
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	/* 2. i3c operation for reading data. */
	SET_I3C_TRANSFER_READ(xfers, len, val);

	return 0;
}

static int is_i3c_sensor_addr8_read8(void *device,
	u8 addr, u8 *val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[1];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_addr8_read8(addr, val, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cR08 [0x%04X] : 0x%04X\n", addr, *val);
	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_read8_size(void *buf, u16 addr, size_t size,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 2;

	/* Send addr */
	GET_IXC_ADDR16(wbuf, addr);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	len = (u16)size;
	SET_I3C_TRANSFER_READ(xfers, len, buf);

	return 0;
}

static int is_i3c_sensor_read8_size(void *device, void *buf,
	u16 addr, size_t size)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		return ret;
	}

	/* Send addr */
	i3c_fill_msg_read8_size(buf, addr, size, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cR16 [0x%04X] : 0x%04X\n", addr, *val);

	return 0;
p_err:
	return ret;
}

static int is_i3c_sensor_read8(void *device,
	u16 addr, u8 *val)
{
	return is_i3c_sensor_read8_size(device, (void *)val, addr, 1);
}

static int i3c_fill_msg_read16(u16 addr,
	struct i3c_priv_xfer *xfers, u8 *wbuf, u8 *rbuf)
{
	u16 len = 2;

	/* 1. i3c operation for writing. */
	/* TODO : consider other size of buffer */
	GET_IXC_ADDR16(wbuf, addr);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	/* 2. i3c operation for reading data. */
	SET_I3C_TRANSFER_READ(xfers, len, rbuf);

	return 0;
}

static int is_i3c_sensor_read16(void *device,
	u16 addr, u16 *val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2], rbuf[2] = {0, 0};
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_read16(addr, xfers, wbuf, rbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	*val = ((rbuf[0] << 8) | rbuf[1]);

	ixc_info("i3cR16 [0x%04X] : 0x%04X\n", addr, *val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_addr8_write8(	u8 addr, u8 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 2;

	wbuf[0] = addr;
	wbuf[1]  = val;
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_addr8_write8(void *device,
	u8 addr, u8 val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[2];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_addr8_write8(addr, val, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cR08 [0x%04X] : 0x%04X\n", addr, val);
	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_write8(u16 addr, u8 val, struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 3;

	GET_IXC_ADDR16(wbuf, addr);
	wbuf[2] = val;
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write8(void *device,
	u16 addr, u8 val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[3];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write8(addr, val, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cW08 [0x%04X] : 0x%04X\n", addr, val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_write8_array(	u16 addr, u8 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	int i = 0;
	u16 len = 2;

	GET_IXC_ADDR16(wbuf, addr);
	for (i = 0; i < num; i++)
		wbuf[(i * 1) + 2] = val[i];

	len = 2 + (num * 1);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write8_array(void *device,
	u16 addr, u8 *val, u32 num)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[10];
	struct i3c_device *cur_device = (struct i3c_device *)device;

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

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write8_array(addr, val, num, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cW08 [0x%04x] : 0x%04x\n", addr, *val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_write16(u16 addr, u16 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 4;

	GET_IXC_ADDR16(wbuf, addr);
	wbuf[2] = (val & 0xFF00) >> 8;
	wbuf[3] = (val & 0xFF);

	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write16(void *device,
	u16 addr, u16 val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[4];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write16(addr, val, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cW16 [0x%04X] : 0x%04X\n", addr, val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_write16_array(u16 addr, u16 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	int i = 0;
	u16 len = 2;

	GET_IXC_ADDR16(wbuf, addr);
	for (i = 0; i < num; i++) {
		wbuf[(i * 2) + 2] = (val[i] & 0xFF00) >> 8;
		wbuf[(i * 2) + 3] = (val[i] & 0xFF);
	}

	len = 2 + (num * 2);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write16_array(void *device,
	u16 addr, u16 *val, u32 num)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[10];
	struct i3c_device *cur_device = (struct i3c_device *)device;

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

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write16_array(addr, val, num, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cW16 [0x%04x] : 0x%04x\n", addr, *val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_write16_burst(u16 addr, u16 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	int i = 0;
	u16 len = 2;

	GET_IXC_ADDR16(wbuf, addr);
	for (i = 0; i < num; i++) {
		wbuf[(i * 2) + 2] = (val[(i * 2) * IXC_NEXT] & 0xFF00) >> 8;
		wbuf[(i * 2) + 3] = (val[(i * 2) * IXC_NEXT] & 0xFF);
	}

	len = 2 + (num * 2);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write16_burst(void *device,
	u16 addr, u16 *val, u32 num)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 *wbuf;
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i3c\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write16_burst(addr, val, num, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err_free;

	ixc_info("i3cW16 [0x%04x] : 0x%04x\n", addr, *val);
	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}

static int i3c_fill_msg_write8_sequential(u16 addr, u8 *val, u16 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	int i = 0;
	u16 len = 2;

	GET_IXC_ADDR16(wbuf, addr);
	for (i = 0; i < num; i++)
		wbuf[(i * 1) + 2] = val[i];

	len = 2 + (num * 1);
	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_write8_sequential(void *device,
	u16 addr, u8 *val, u16 num)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 *wbuf;
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kzalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i3c\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_write8_sequential(addr, val, num, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err_free;

	ixc_info("i3cW08 [0x%04x] : 0x%04x\n", addr, *val);
	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}

static int i3c_fill_msg_data_read16(struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 2;

	/* 1. i3c operation for reading data */
	SET_I3C_TRANSFER_READ(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_data_read16(void *device,
		u16 *val)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2] = {0, 0};
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* fill I2C operation */
	i3c_fill_msg_data_read16(xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	*val = ((wbuf[0] << 8) | wbuf[1]);

	ixc_info("i3cR16(%d) : 0x%08X\n", *val);

	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_data_write16(	u8 val_high, u8 val_low,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 2;

	wbuf[0] = val_high;
	wbuf[1] = val_low;

	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_data_write16(void *device,
		u8 val_high, u8 val_low)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[2];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	i3c_fill_msg_data_write16(val_high, val_low, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cR08 : 0x%04X%04X\n", val_high, val_low);
	return 0;
p_err:
	return ret;
}

static int i3c_fill_msg_addr_data_write16(u8 addr, u8 val_high, u8 val_low,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	u16 len = 3;

	wbuf[0] = addr;
	wbuf[1] = val_high;
	wbuf[2] = val_low;

	SET_I3C_TRANSFER_WRITE(xfers, len, wbuf);

	return 0;
}

static int is_i3c_sensor_addr_data_write16(void *device,
		u8 addr, u8 val_high, u8 val_low)
{
	int ret = 0;
	struct i3c_priv_xfer xfers[IXC_WRITE_SIZE];
	u8 wbuf[3];
	struct i3c_device *cur_device = (struct i3c_device *)device;

	if (!cur_device) {
		pr_err("Could not find device!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* fill I2C operation */
	i3c_fill_msg_addr_data_write16(addr, val_high, val_low, xfers, wbuf);
	ret = is_i3c_device_do_priv_xfers(cur_device, xfers, ARRAY_SIZE(xfers));
	if (ret < 0)
		goto p_err;

	ixc_info("i3cW16 : 0x%04X%04X\n", val_high, val_low);

	return 0;
p_err:
	return ret;
}

static struct is_ixc_ops i3c_ops = {
	.addr8_read8 = is_i3c_sensor_addr8_read8,
	.read8 = is_i3c_sensor_read8,
	.read16 = is_i3c_sensor_read16,
	.read8_size = is_i3c_sensor_read8_size,
	.addr8_write8 = is_i3c_sensor_addr8_write8,
	.write8 = is_i3c_sensor_write8,
	.write8_array = is_i3c_sensor_write8_array,
	.write16 = is_i3c_sensor_write16,
	.write16_array = is_i3c_sensor_write16_array,
	.write16_burst = is_i3c_sensor_write16_burst,
	.write8_sequential = is_i3c_sensor_write8_sequential,
	.data_read16 = is_i3c_sensor_data_read16,
	.data_write16 = is_i3c_sensor_data_write16,
	.addr_data_write16 = is_i3c_sensor_addr_data_write16,
};

struct is_ixc_ops *pablo_get_i3c(void)
{
	return &i3c_ops;
}
EXPORT_SYMBOL_GPL(pablo_get_i3c);
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_i3c_fill_msg_addr8_read8(u8 addr, u8 *val,
struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_addr8_read8(addr, val, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_addr8_read8);
int pablo_kunit_i3c_fill_msg_read16(u16 addr,
	struct i3c_priv_xfer *xfers, u8 *wbuf, u8 *rbuf)
{
	return i3c_fill_msg_read16(addr, xfers, wbuf, rbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_read16);
int pablo_kunit_i3c_fill_msg_data_read16(struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_data_read16(xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_data_read16);
int pablo_kunit_i3c_fill_msg_read8_size(void *buf, u8 addr, size_t size,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_read8_size(buf, addr, size, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_read8_size);
int pablo_kunit_i3c_fill_msg_addr8_write8(u8 addr, u8 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_addr8_write8(addr, val, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_addr8_write8);
int pablo_kunit_i3c_fill_msg_write8(u16 addr, u8 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write8(addr, val, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write8);
int pablo_kunit_i3c_fill_msg_write8_array(u16 addr, u8 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write8_array(addr, val, num, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write8_array);
int pablo_kunit_i3c_fill_msg_write16(u16 addr, u16 val,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write16(addr, val, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write16);
int pablo_kunit_i3c_fill_msg_data_write16(u8 val_h, u8 val_l,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_data_write16(val_h, val_l, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_data_write16);
int pablo_kunit_i3c_fill_msg_write16_array(u16 addr, u16 *arr_val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write16_array(addr, arr_val, num, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write16_array);
int pablo_kunit_i3c_fill_msg_write16_burst(u16 addr, u16 *arr_val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write16_burst(addr, arr_val, num, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write16_burst);
int pablo_kunit_i3c_fill_msg_write8_sequential(u16 addr, u8 *val, u32 num,
	struct i3c_priv_xfer *xfers, u8 *wbuf)
{
	return i3c_fill_msg_write8_sequential(addr, val, num, xfers, wbuf);
}
EXPORT_SYMBOL_GPL(pablo_kunit_i3c_fill_msg_write8_sequential);
#endif
