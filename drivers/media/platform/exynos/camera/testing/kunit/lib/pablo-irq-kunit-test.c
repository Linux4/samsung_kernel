// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "pablo-irq.h"

#define PKT_IRQ_NUM	5

static struct pablo_kunit_test_ctx {
	struct pablo_irq *pirq;
	const struct pablo_irq_operations *ops_b;
	struct cpumask msk[PKT_IRQ_NUM];
	irq_handler_t fn[PKT_IRQ_NUM];
} pkt_ctx;

static int pkt_irq_set_affinity_hint(unsigned int irq, const struct cpumask *m)
{
	if (m)
		cpumask_copy(&pkt_ctx.msk[irq], m);

	return 0;
}

static int pkt_request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
		const char *name, void *dev)
{
	pkt_ctx.fn[irq] = handler;

	return 0;
}

static const void *pkt_free_irq(unsigned int irq, void *dev_id)
{
	pkt_ctx.fn[irq] = NULL;

	return NULL;
}

static const struct pablo_irq_operations pkt_pirq_ops = {
	.set_affinity_hint = pkt_irq_set_affinity_hint,
	.request_irq = pkt_request_irq,
	.free_irq = pkt_free_irq,
};

static irqreturn_t pkt_isr(int irq, void *data)
{
	/* Do nothing */
	return IRQ_HANDLED;
}

/* Define Testcases */
static void pablo_set_affinity_irq_kunit_test(struct kunit *test)
{
	u32 i;

	/* TC #1. Set affinity with IRQ 0 */
	pablo_set_affinity_irq(0, true);
	KUNIT_EXPECT_TRUE(test, cpumask_empty(&pkt_ctx.msk[0]));

	/* TC #2. Set affinity with disable flag */
	for (i = 1; i < PKT_IRQ_NUM; i++) {
		pablo_set_affinity_irq(i, false);
		KUNIT_EXPECT_TRUE(test, cpumask_empty(&pkt_ctx.msk[i]));
	}

	/* TC #3. Set affinity with enable flag */
	for (i = 1; i < PKT_IRQ_NUM; i++) {
		pablo_set_affinity_irq(i, true);
		KUNIT_EXPECT_TRUE(test, !cpumask_empty(&pkt_ctx.msk[i]));
	}
}

static void pablo_request_free_irq_kunit_test(struct kunit *test)
{
	u32 i;

	/* TC #1. Request IRQ */
	for (i = 1; i < PKT_IRQ_NUM; i++) {
		pablo_request_irq(i, pkt_isr, __func__, 0, NULL);
		KUNIT_EXPECT_PTR_EQ(test, (void *)pkt_ctx.fn[i], (void *)pkt_isr);
		KUNIT_EXPECT_TRUE(test, !cpumask_empty(&pkt_ctx.msk[i]));
	}

	/* TC #2. Free IRQ */
	for (i = 1; i < PKT_IRQ_NUM; i++) {
		pablo_free_irq(i, NULL);
		KUNIT_EXPECT_PTR_EQ(test, (void *)pkt_ctx.fn[i], (void *)NULL);
	}
}

static int pablo_irq_kunit_test_init(struct kunit *test)
{
	struct pablo_irq *pirq;

	pirq = pkt_ctx.pirq = pablo_irq_get();
	pkt_ctx.ops_b = pirq->ops;
	pirq->ops = &pkt_pirq_ops;

	memset(pkt_ctx.msk, 0, sizeof(struct cpumask) * PKT_IRQ_NUM);

	return 0;
}

static void pablo_irq_kunit_test_exit(struct kunit *test)
{
	pkt_ctx.pirq->ops = pkt_ctx.ops_b;
}

static struct kunit_case pablo_irq_kunit_test_cases[] = {
	KUNIT_CASE(pablo_set_affinity_irq_kunit_test),
	KUNIT_CASE(pablo_request_free_irq_kunit_test),
	{},
};

struct kunit_suite pablo_irq_kunit_test_suite = {
	.name = "pablo-irq-kunit-test",
	.init = pablo_irq_kunit_test_init,
	.exit = pablo_irq_kunit_test_exit,
	.test_cases = pablo_irq_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_irq_kunit_test_suite);

MODULE_LICENSE("GPL");
