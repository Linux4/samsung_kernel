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

static int pkt_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	/* Do nothing */
	return 0;
}

static u32 pkt_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm pkt_algo = {
	.master_xfer = pkt_i2c_xfer,
	.functionality = pkt_i2c_func,
};

static struct pablo_kunit_test_ctx {
	struct is_cis *cis;
	struct i2c_client* i2c_cl;
	struct i2c_adapter* i2c_adt;
	const struct i2c_algorithm *i2c_algo;
} pkt_ctx;

/* Define the test cases. */
static void pablo_i2c_check_adapter_kunit_test(struct kunit *test)
{
	struct i2c_client tmp_cl;
	int test_result;

	test_result = pablo_kunit_i2c_check_adapter(pkt_ctx.i2c_cl);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);

	tmp_cl.adapter = 0;
	test_result = pablo_kunit_i2c_check_adapter(&tmp_cl);
	KUNIT_EXPECT_LT(test, test_result, (int)0);
}

static void pablo_i2c_sensor_addr8_read8_kunit_test(struct kunit *test)
{
	u8 addr = 0x6a;
	u8 val = 0xFF;
	struct i2c_msg msg[2];
	u8 wbuf[1];
	int test_result;

	pablo_kunit_i2c_fill_msg_addr8_read8(pkt_ctx.i2c_cl, addr, &val, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, *(msg[0].buf), addr);
	KUNIT_EXPECT_EQ(test, msg[1].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, (u64)(msg[1].buf), (u64)&val);

	test_result = pkt_ctx.cis->ixc_ops->addr8_read8((void *)pkt_ctx.i2c_cl, addr, &val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
	KUNIT_EXPECT_EQ(test, *(msg[1].buf), val);
}

static void pablo_i2c_sensor_read8_kunit_test(struct kunit *test)
{
	u16 addr = 0x1234;
	u8 val = 0xFF;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	int test_result;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	pablo_kunit_i2c_fill_msg_read8(pkt_ctx.i2c_cl, addr, &val, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	KUNIT_EXPECT_EQ(test, msg[1].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, (u64)(msg[1].buf), (u64)&val);

	test_result = pkt_ctx.cis->ixc_ops->read8((void *)pkt_ctx.i2c_cl, addr, &val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
	KUNIT_EXPECT_EQ(test, *(msg[1].buf), val);
}

static void pablo_i2c_sensor_read16_kunit_test(struct kunit *test)
{
	u16 addr = 0xFDAB;
	u16 val = 0x1234;
	struct i2c_msg msg[2];
	u8 wbuf[2], rbuf[2] = {0, 0};
	int test_result;
	u8 addr_h, addr_l;
	u16 val_r;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	pablo_kunit_i2c_fill_msg_read16(pkt_ctx.i2c_cl, addr, msg, wbuf, rbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	KUNIT_EXPECT_EQ(test, msg[1].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, (u64)(msg[1].buf), (u64)rbuf);

	test_result = pkt_ctx.cis->ixc_ops->read16((void *)pkt_ctx.i2c_cl, addr, &val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
	val_r = ((msg[1].buf[0] << 8) | msg[1].buf[1]);
	KUNIT_EXPECT_EQ(test, val_r, val);
}

static void pablo_i2c_sensor_data_read16_kunit_test(struct kunit *test)
{
	u16 val = 0x1234;
	struct i2c_msg msg[1];
	u8 wbuf[2] = {0, 0};
	int test_result;
	u16 val_r;

	pablo_kunit_i2c_fill_msg_data_read16(pkt_ctx.i2c_cl, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);

	test_result = pkt_ctx.cis->ixc_ops->data_read16((void *)pkt_ctx.i2c_cl, &val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
	val_r = ((msg[0].buf[0] << 8) | msg[0].buf[1]);
	KUNIT_EXPECT_EQ(test, val_r, val);
}

static void pablo_i2c_sensor_read8_size_kunit_test(struct kunit *test)
{
	u8 addr = 0x12;
	char val[4] = {'a','b','c','d'};
	unsigned long size = 0x1234;
	int test_result;

	test_result = pkt_ctx.cis->ixc_ops->read8_size((void *)pkt_ctx.i2c_cl, val, addr, size);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_addr8_write8_kunit_test(struct kunit *test)
{
	u8 addr = 0x6a;
	u8 val = 0x78;
	struct i2c_msg msg[1];
	u8 wbuf[2];
	int test_result;

	pablo_kunit_i2c_fill_msg_addr8_write8(pkt_ctx.i2c_cl, addr, val, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], val);

	test_result = pkt_ctx.cis->ixc_ops->addr8_write8((void *)pkt_ctx.i2c_cl, addr, val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write8_kunit_test(struct kunit *test)
{
	u16 addr = 0x3456;
	u8 val = 0x56;
	struct i2c_msg msg[1];
	u8 wbuf[3];
	int test_result;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	pablo_kunit_i2c_fill_msg_write8(pkt_ctx.i2c_cl, addr, val, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	KUNIT_EXPECT_EQ(test, msg[0].buf[2], val);

	test_result = pkt_ctx.cis->ixc_ops->write8((void *)pkt_ctx.i2c_cl, addr, val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write8_array_kunit_test(struct kunit *test)
{
	u16 addr = 0x0202;
	u8 arr_val[4];
	struct i2c_msg msg[1];
	u8 wbuf[10];
	u16 num = 2;
	int test_result;
	int i;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	arr_val[0] = 0x12;
	arr_val[1] = 0x34;
	arr_val[2] = 0x56;
	arr_val[3] = 0x78;

	pablo_kunit_i2c_fill_msg_write8_array(pkt_ctx.i2c_cl, addr, arr_val, num, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	for (i = 0; i < num; i++)
		KUNIT_EXPECT_EQ(test, msg[0].buf[(i * 1) + 2], arr_val[i]);

	test_result = pkt_ctx.cis->ixc_ops->write8_array((void *)pkt_ctx.i2c_cl, addr, arr_val, i);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write16_kunit_test(struct kunit *test)
{
	u16 addr = 0xFCFC;
	u16 val = 0x1234;
	struct i2c_msg msg[1];
	u8 wbuf[4];
	int test_result;
	u8 addr_h, addr_l, val_h, val_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);
	val_h = (val & 0xFF00) >> 8;
	val_l = (val & 0xFF);

	pablo_kunit_i2c_fill_msg_write16(pkt_ctx.i2c_cl, addr, val, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	KUNIT_EXPECT_EQ(test, msg[0].buf[2], val_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[3], val_l);

	test_result = pkt_ctx.cis->ixc_ops->write16((void *)pkt_ctx.i2c_cl, addr, val);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_data_write16_kunit_test(struct kunit *test)
{
	struct i2c_msg msg[1];
	u8 wbuf[2];
	u16 val = 0x1234;
	int test_result;
	u8 val_h, val_l;

	val_h = (val & 0xFF00) >> 8;
	val_l = (val & 0xFF);

	pablo_kunit_i2c_fill_msg_data_write16(pkt_ctx.i2c_cl, val_h, val_l, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], val_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], val_l);

	test_result = pkt_ctx.cis->ixc_ops->data_write16((void *)pkt_ctx.i2c_cl, val_h, val_l);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write16_array_kunit_test(struct kunit *test)
{
	u16 addr = 0x0202;
	u16 arr_val[4];
	struct i2c_msg msg[1];
	u8 wbuf[10];
	u32 num = 4;
	int test_result;
	u8 addr_h, addr_l, val_h, val_l;
	int i;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	arr_val[0] = 0x1234;
	arr_val[1] = 0x5678;
	arr_val[2] = 0x9ABC;
	arr_val[3] = 0xDEFF;

	pablo_kunit_i2c_fill_msg_write16_array(pkt_ctx.i2c_cl, addr, arr_val, num, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	for (i = 0; i < num; i++) {
		val_h = (arr_val[i] & 0xFF00) >> 8;
		val_l = (arr_val[i] & 0xFF);
		KUNIT_EXPECT_EQ(test, msg[0].buf[(i * 2) + 2], val_h);
		KUNIT_EXPECT_EQ(test, msg[0].buf[(i * 2) + 3], val_l);
	}

	test_result = pkt_ctx.cis->ixc_ops->write16_array((void *)pkt_ctx.i2c_cl, addr, arr_val, i);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write16_burst_kunit_test(struct kunit *test)
{
	u16 addr = 0xFCFC;
	u16 arr_val[4];
	int burst_num = 1;
	struct i2c_msg msg[1];
	u8 *wbuf;
	int test_result;
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
		pr_err("failed to alloc buffer for burst i2c\n");
		return;
	}
	pablo_kunit_i2c_fill_msg_write16_burst(pkt_ctx.i2c_cl, addr, arr_val, burst_num, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);
	for (i = 0; i < burst_num; i++) {
		val_h = (arr_val[(i * 2) * IXC_NEXT] & 0xFF00) >> 8;
		val_l = (arr_val[(i * 2) * IXC_NEXT] & 0xFF);
		KUNIT_EXPECT_EQ(test, msg[0].buf[(i * 2) + 2], val_h);
		KUNIT_EXPECT_EQ(test, msg[0].buf[(i * 2) + 3], val_l);
	}
	kfree(wbuf);

	test_result = pkt_ctx.cis->ixc_ops->write16_burst((void *)pkt_ctx.i2c_cl, addr, arr_val, burst_num);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_i2c_sensor_write8_sequential_kunit_test(struct kunit *test)
{
	u16 addr = 0x7520; /* IMX576 */
	u8 cal_data[216] = {0, }; /* IMX576 */
	u32 num = 216;
	struct i2c_msg msg[1];
	u8 *wbuf;
	int test_result;
	u8 addr_h, addr_l;

	addr_h = (addr & 0xFF00) >> 8;
	addr_l = (addr & 0xFF);

	wbuf = kzalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i2c\n");
		return;
	}

	pablo_kunit_i2c_fill_msg_write8_sequential(pkt_ctx.i2c_cl, addr, cal_data, num, msg, wbuf);
	KUNIT_EXPECT_EQ(test, msg[0].addr, pkt_ctx.i2c_cl->addr);
	KUNIT_EXPECT_EQ(test, msg[0].buf[0], addr_h);
	KUNIT_EXPECT_EQ(test, msg[0].buf[1], addr_l);

	kfree(wbuf);

	test_result = pkt_ctx.cis->ixc_ops->write8_sequential((void *)pkt_ctx.i2c_cl, addr, cal_data, num);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static int pablo_i2c_sensor_kunit_test_init(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];
	struct is_module_enum *module = &sensor->module_enum[0];
	struct is_device_sensor_peri *s_peri;

	s_peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, s_peri);

	pkt_ctx.cis = &(s_peri->cis);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pkt_ctx.cis->ixc_ops);

	pkt_ctx.i2c_cl = (struct i2c_client *)pkt_ctx.cis->client;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pkt_ctx.i2c_cl);

	pkt_ctx.i2c_adt = pkt_ctx.i2c_cl->adapter;

	/* Hooking the I2C algorithm for Kunit test */
	pkt_ctx.i2c_algo = pkt_ctx.i2c_adt->algo;
	pkt_ctx.i2c_adt->algo = &pkt_algo;

	return 0;
}

static void pablo_i2c_sensor_kunit_test_exit(struct kunit *test)
{
	/* Restore the I2C algorithm */
	pkt_ctx.i2c_adt->algo = pkt_ctx.i2c_algo;
}

static struct kunit_case pablo_hw_i2c_sensor_kunit_test_cases[] = {
	KUNIT_CASE(pablo_i2c_check_adapter_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_addr8_read8_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_read8_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_read16_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_data_read16_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_read8_size_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_addr8_write8_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write8_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write8_array_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write16_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_data_write16_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write16_array_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write16_burst_kunit_test),
	KUNIT_CASE(pablo_i2c_sensor_write8_sequential_kunit_test),
	{},
};

struct kunit_suite pablo_hw_i2c_sensor_kunit_test_suite = {
	.name = "pablo-hw-i2c-sensor-kunit-test",
	.init = pablo_i2c_sensor_kunit_test_init,
	.exit = pablo_i2c_sensor_kunit_test_exit,
	.test_cases = pablo_hw_i2c_sensor_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_i2c_sensor_kunit_test_suite);

MODULE_LICENSE("GPL");
