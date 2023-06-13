// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-helper-ixc.h"

static struct is_core *i3c_core;
static struct is_device_sensor *i3c_sensor;
static struct is_module_enum *i3c_module;
static struct is_device_sensor_peri *i3c_sensor_peri;
static struct is_cis *i3c_cis;
static void *Device;
static struct i3c_device *tmp_device;
static struct is_ixc_ops *backup_ixc_ops;

/* Define the test cases. */
static void pablo_i3c_sensor_addr8_read8_kunit_test(struct kunit *test)
{
	u8 addr = 0x6a;
	u8 val = 0xFF;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[1];

	pablo_kunit_i3c_fill_msg_addr8_read8(addr, &val, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)(xfers[0].data.out)), addr);
	KUNIT_EXPECT_EQ(test, (u64)(xfers[1].data.in), (u64)&val);
}

static void pablo_i3c_sensor_read16_kunit_test(struct kunit *test)
{
	u16 addr = 0xFDAB;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2], rbuf[2] = {0, 0};
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	pablo_kunit_i3c_fill_msg_read16(addr, xfers, wbuf, rbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	KUNIT_EXPECT_EQ(test, (u64)(xfers[1].data.in), (u64)rbuf);
}

static void pablo_i3c_sensor_data_read16_kunit_test(struct kunit *test)
{
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2] = {0, 0};

	pablo_kunit_i3c_fill_msg_data_read16(xfers, wbuf);
	KUNIT_EXPECT_EQ(test, (u64)(xfers[1].data.in), (u64)wbuf);
}

static void pablo_i3c_sensor_read8_size_kunit_test(struct kunit *test)
{
	u8 addr = 0x12;
	char val[4] = {'a','b','c','d'};
	unsigned long size = 0x1234;
	struct i3c_priv_xfer xfers[IXC_READ_SIZE];
	u8 wbuf[2];
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	pablo_kunit_i3c_fill_msg_read8_size(val, addr, size, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	KUNIT_EXPECT_EQ(test, (u64)(xfers[1].data.in), (u64)val);
}

static void pablo_i3c_sensor_addr8_write8_kunit_test(struct kunit *test)
{
	u8 addr = 0x6a;
	u8 val = 0x78;
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[2];

	pablo_kunit_i3c_fill_msg_addr8_write8(addr, val, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), val);
}

static void pablo_i3c_sensor_write8_kunit_test(struct kunit *test)
{
	u16 addr = 0x3456;
	u8 val = 0x56;
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[3];
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	pablo_kunit_i3c_fill_msg_write8(addr, val, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[2])), val);
}

static void pablo_i3c_sensor_write8_array_kunit_test(struct kunit *test)
{
	u16 addr = 0x0202;
	u8 arr_val[4];
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[10];
	u16 num = 2;
	int i;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	arr_val[0] = 0x12;
	arr_val[1] = 0x34;
	arr_val[2] = 0x56;
	arr_val[3] = 0x78;

	pablo_kunit_i3c_fill_msg_write8_array(addr, arr_val, num, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	for (i = 0; i < num; i++)
		KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[(i * 1) + 2])), arr_val[i]);
}

static void pablo_i3c_sensor_write16_kunit_test(struct kunit *test)
{
	u16 addr = 0xFCFC;
	u16 val = 0x1234;
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[4];
	u8 addr_h, addr_l, val_h, val_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	val_h = (val & 0xFF00) >> 8;
	val_l = (val & 0xFF);

	pablo_kunit_i3c_fill_msg_write16(addr, val, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[2])), val_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[3])), val_l);
}

static void pablo_i3c_sensor_data_write16_kunit_test(struct kunit *test)
{
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[2];
	u16 val = 0x1234;
	u8 val_h, val_l;

	val_h = (val & 0xFF00) >> 8;
	val_l = (val & 0xFF);

	pablo_kunit_i3c_fill_msg_data_write16(val_h, val_l, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), val_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), val_l);
}

static void pablo_i3c_sensor_write16_array_kunit_test(struct kunit *test)
{
	u16 addr = 0x0202;
	u16 arr_val[4];
	struct i3c_priv_xfer xfers[1];
	u8 wbuf[10];
	u32 num = 4;
	u8 addr_h, addr_l, val_h, val_l;
	int i;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	arr_val[0] = 0x1234;
	arr_val[1] = 0x5678;
	arr_val[2] = 0x9ABC;
	arr_val[3] = 0xDEFF;

	pablo_kunit_i3c_fill_msg_write16_array(addr, arr_val, num, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	for (i = 0; i < num; i++) {
		val_h = (arr_val[i] & 0xFF00) >> 8;
		val_l = (arr_val[i] & 0xFF);
		KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[(i * 2) + 2])), val_h);
		KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[(i * 2) + 3])), val_l);
	}
}

static void pablo_i3c_sensor_write16_burst_kunit_test(struct kunit *test)
{
	u16 addr = 0xFCFC;
	u16 arr_val[4];
	int burst_num = 1;
	struct i3c_priv_xfer xfers[1];
	u8 *wbuf;
	u8 addr_h, addr_l, val_h, val_l;
	int i;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	arr_val[0] = 0x1234;
	arr_val[1] = 0x5678;
	arr_val[2] = 0x9ABC;
	arr_val[3] = 0xDEFF;

	wbuf = kmalloc((2 + (burst_num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i3c\n");
		return;
	}
	pablo_kunit_i3c_fill_msg_write16_burst(addr, arr_val, burst_num, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);
	for (i = 0; i < burst_num; i++) {
		val_h = (arr_val[(i * 2) * IXC_NEXT] & 0xFF00) >> 8;
		val_l = (arr_val[(i * 2) * IXC_NEXT] & 0xFF);
		KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[(i * 2) + 2])), val_h);
		KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[(i * 2) + 3])), val_l);
	}
	kfree(wbuf);
}

static void pablo_i3c_sensor_write8_sequential_kunit_test(struct kunit *test)
{
	u16 addr = 0x7520; /* IMX576 */
	u8 cal_data[216] = {0, }; /* IMX576 */
	u32 num = 216;
	struct i3c_priv_xfer xfers[1];
	u8 *wbuf;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	wbuf = kzalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i3c\n");
		return;
	}

	pablo_kunit_i3c_fill_msg_write8_sequential(addr, cal_data, num, xfers, wbuf);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[0])), addr_h);
	KUNIT_EXPECT_EQ(test, *((u8 *)&(xfers[0].data.out[1])), addr_l);

	kfree(wbuf);
}

static int pablo_i3c_sensor_kunit_test_init(struct kunit *test)
{
	i3c_core = is_get_is_core();
	i3c_sensor = &i3c_core->sensor[RESOURCE_TYPE_SENSOR0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, i3c_sensor);

	i3c_module = &i3c_sensor->module_enum[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, i3c_module);

	i3c_sensor_peri = (struct is_device_sensor_peri *)i3c_module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, i3c_sensor_peri);

	i3c_cis = &(i3c_sensor_peri->cis);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, i3c_cis);

	backup_ixc_ops = i3c_cis->ixc_ops;
	i3c_cis->ixc_ops = pablo_get_i3c();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, i3c_cis->ixc_ops);

	Device = i3c_cis->client;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, Device);

	tmp_device = (struct i3c_device *)Device;

	return 0;
}

static void pablo_i3c_sensor_kunit_test_exit(struct kunit *test)
{
	i3c_cis->ixc_ops = backup_ixc_ops;
}

static struct kunit_case pablo_hw_i3c_sensor_kunit_test_cases[] = {
	KUNIT_CASE(pablo_i3c_sensor_addr8_read8_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_read16_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_data_read16_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_read8_size_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_addr8_write8_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write8_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write8_array_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write16_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_data_write16_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write16_array_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write16_burst_kunit_test),
	KUNIT_CASE(pablo_i3c_sensor_write8_sequential_kunit_test),
	{},
};

struct kunit_suite pablo_hw_i3c_sensor_kunit_test_suite = {
	.name = "pablo-hw-i3c-sensor-kunit-test",
	.init = pablo_i3c_sensor_kunit_test_init,
	.exit = pablo_i3c_sensor_kunit_test_exit,
	.test_cases = pablo_hw_i3c_sensor_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_i3c_sensor_kunit_test_suite);

MODULE_LICENSE("GPL");
