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

#include <linux/io.h>
#include <linux/interrupt.h>

#include "pablo-kunit-test.h"
#include "icpu/pablo-icpu-core.h"
#include "icpu/hardware/pablo-icpu-hw.h"
#include "icpu/mbox/pablo-icpu-mbox.h"

/* Define the test cases. */

static struct test_ctx {
	void *test_addr;
	struct icpu_mbox_controller **mbox;
	struct icpu_mbox_tx_info tx_info;
	struct icpu_mbox_rx_info rx_info;
	struct pablo_icpu_mbox_chan tx_chan;
	struct pablo_icpu_mbox_chan rx_chan;
	struct icpu_mbox_client cl;
	struct kunit *test;
} test_ctx;

#define TX 0
#define RX 1

static void pablo_icpu_mbox_isr_handler(struct icpu_mbox_client *cl, u32 *data, u32 len)
{
	/* To nothing yet */
	struct kunit *test = test_ctx.test;
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cl);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, data);

	KUNIT_EXPECT_EQ(test, data[0], (u32)127);
	KUNIT_EXPECT_EQ(test, data[1], (u32)128);
	KUNIT_EXPECT_EQ(test, data[2], (u32)129);
	KUNIT_EXPECT_EQ(test, data[3], (u32)130);
}

static void pablo_icpu_mbox_send_message_kunit_test(struct kunit *test)
{
	int ret;
	u32 data[4] = { 1, 2, 3, 4 };
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* Check tx_data set 0 */
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg), (u32)0);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 4), (u32)0);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 8), (u32)0);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 12), (u32)0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->send_data);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->startup);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->shutdown);

	ret = test_ctx.tx_chan.mbox->ops->startup(&test_ctx.tx_chan);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = test_ctx.tx_chan.mbox->ops->send_data(&test_ctx.tx_chan, data, 4);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_ctx.tx_chan.mbox->ops->shutdown(&test_ctx.tx_chan);

	/* Verify data */
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg), data[0]);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 4), data[1]);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 8), data[2]);
	KUNIT_EXPECT_EQ(test, readl(ctx->tx_info.data_reg + 12), data[3]);
}

static void pablo_icpu_mbox_send_message_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 data[4] = { 1, 2, 3, 4 };
	struct pablo_icpu_mbox_chan invalid_chan = { 0, };
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->send_data);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->startup);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->shutdown);

	ret = test_ctx.tx_chan.mbox->ops->send_data(&test_ctx.tx_chan, data, 4);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ret = test_ctx.tx_chan.mbox->ops->startup(&test_ctx.tx_chan);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = test_ctx.tx_chan.mbox->ops->send_data(&invalid_chan, data, 4);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ret = test_ctx.tx_chan.mbox->ops->send_data(&test_ctx.tx_chan, NULL, 4);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	test_ctx.tx_chan.mbox->ops->shutdown(&test_ctx.tx_chan);
}

static void pablo_icpu_mbox_send_message_timeout_kunit_test(struct kunit *test)
{
	int ret;
	u32 data[4] = { 1, 2, 3, 4 };
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->send_data);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->startup);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]->ops->shutdown);

	/* Expect timeout from send message */
	*(u32 *)(ctx->tx_info.int_status_reg) = 1;

	ret = test_ctx.tx_chan.mbox->ops->startup(&test_ctx.tx_chan);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = test_ctx.tx_chan.mbox->ops->send_data(&test_ctx.tx_chan, data, 4);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);

	test_ctx.tx_chan.mbox->ops->shutdown(&test_ctx.tx_chan);
}

static void pablo_icpu_mbox_isr_kunit_test(struct kunit *test)
{
	int irqret;
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	*(u32 *)(ctx->rx_info.data_reg + 0) = 127;
	*(u32 *)(ctx->rx_info.data_reg + 4) = 128;
	*(u32 *)(ctx->rx_info.data_reg + 8) = 129;
	*(u32 *)(ctx->rx_info.data_reg + 12) = 130;

	irqret = pablo_icpu_mbox_isr_wrap(&test_ctx.rx_chan);
	KUNIT_EXPECT_EQ(test, irqret, IRQ_HANDLED);
}

static struct kunit_case pablo_icpu_mbox_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_mbox_send_message_kunit_test),
	KUNIT_CASE(pablo_icpu_mbox_send_message_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_mbox_send_message_timeout_kunit_test),
	KUNIT_CASE(pablo_icpu_mbox_isr_kunit_test),
	{},
};

static int pablo_icpu_mbox_kunit_test_init(struct kunit *test)
{
	test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.tx_info.int_enable_reg = test_ctx.test_addr + 0x50;
	test_ctx.tx_info.int_gen_reg = test_ctx.test_addr + 0x100;
	test_ctx.tx_info.int_status_reg = test_ctx.test_addr + 0x200;
	test_ctx.tx_info.data_reg = test_ctx.test_addr + 0x300;
	test_ctx.tx_info.data_max_len = 16;

	test_ctx.rx_info.int_status_reg = test_ctx.test_addr + 0x500;
	test_ctx.rx_info.data_reg = test_ctx.test_addr + 0x600;
	test_ctx.rx_info.data_max_len = 8;
	test_ctx.rx_info.irq = 99;

	test_ctx.mbox = kunit_kzalloc(test, sizeof(struct icpu_mbox_controller*) * 2, 0);

	test_ctx.mbox[TX] = pablo_icpu_mbox_request(ICPU_MBOX_MODE_TX, &test_ctx.tx_info);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[TX]);

	test_ctx.mbox[RX] = pablo_icpu_mbox_request(ICPU_MBOX_MODE_RX, &test_ctx.rx_info);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.mbox[RX]);

	test_ctx.cl.rx_callback = pablo_icpu_mbox_isr_handler;
	test_ctx.cl.tx_prepare = NULL;
	test_ctx.cl.tx_done = NULL;

	test_ctx.tx_chan.cl = &test_ctx.cl;
	test_ctx.tx_chan.mbox = test_ctx.mbox[TX];

	test_ctx.rx_chan.cl = &test_ctx.cl;
	test_ctx.rx_chan.mbox = test_ctx.mbox[RX];

	test->priv = &test_ctx;
	test_ctx.test = test;

	return 0;
}

static void pablo_icpu_mbox_kunit_test_exit(struct kunit *test)
{
	struct test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);

	pablo_icpu_mbox_free(test_ctx.mbox[TX]);
	pablo_icpu_mbox_free(test_ctx.mbox[RX]);

	kunit_kfree(test, ctx->mbox);
}

struct kunit_suite pablo_icpu_mbox_kunit_test_suite = {
	.name = "pablo-icpu-mbox-kunit-test",
	.init = pablo_icpu_mbox_kunit_test_init,
	.exit = pablo_icpu_mbox_kunit_test_exit,
	.test_cases = pablo_icpu_mbox_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_mbox_kunit_test_suite);

MODULE_LICENSE("GPL");
