/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *	Jonghwan Choi <Jonghwan Choi@samsung.com>
 *
 * EXYNOS - PPMU polling support
 *	This version supports EXYNOS5250 only. This changes bus frequencies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <mach/map.h>

#include "exynos3470_ppmu.h"

static DEFINE_SPINLOCK(exynos4270_ppmu_lock);
static LIST_HEAD(exynos4270_ppmu_handle_list);

struct exynos4270_ppmu_handle {
	struct list_head node;
	struct exynos4270_ppmu ppmu[PPMU_END];
};

static struct exynos4270_ppmu ppmu[PPMU_END] = {
	[PPMU_DMC_0] = {
		.hw_base = S5P_VA_PPMU_DMC0,
		.event[PPMU_PMNCNT3] = RDWR_DATA_COUNT,
	},
	[PPMU_DMC_1] = {
		.hw_base = S5P_VA_PPMU_DMC1,
		.event[PPMU_PMNCNT3] = RDWR_DATA_COUNT,
	},
	[PPMU_CPU] = {
		.hw_base = S5P_VA_PPMU_CPU,
		.event[PPMU_PMNCNT3] = RDWR_DATA_COUNT,
	},
	[PPMU_RIGHT] = {
		.hw_base = S5P_VA_PPMU_RIGHT,
		.event[PPMU_PMNCNT3] = RDWR_DATA_COUNT,
	},
	[PPMU_LEFT] = {
		.hw_base = S5P_VA_PPMU_LEFT,
		.event[PPMU_PMNCNT3] = RDWR_DATA_COUNT,
	},
};

static void exynos4270_ppmu_setevent(void __iomem *ppmu_base,
					unsigned int ch,
					unsigned int evt)
{
	__raw_writel(evt, ppmu_base + PPMU_BEVTSEL(ch));
}


static void exynos4270_ppmu_start(void __iomem *ppmu_base)
{
	__raw_writel(PPMU_ENABLE, ppmu_base);
}

static void exynos4270_ppmu_stop(void __iomem *ppmu_base)
{
	__raw_writel(PPMU_DISABLE, ppmu_base);
}

static void exynos4270_ppmu_reset(struct exynos4270_ppmu *ppmu)
{
	void __iomem *ppmu_base = ppmu->hw_base;
	unsigned int cntens_val=0, flag_val=0, pmnc_val=0;
	unsigned long flags;

	cntens_val = PPMU_ENABLE_CYCLE | PPMU_ENABLE_COUNT0 |
			PPMU_ENABLE_COUNT1 | PPMU_ENABLE_COUNT2 |
			PPMU_ENABLE_COUNT3;

	flag_val = cntens_val;

	pmnc_val = PPMU_CC_RESET | PPMU_COUNT_RESET;

	/* Reset PPMU */
	__raw_writel(cntens_val, ppmu_base + PPMU_CNTENS);
	__raw_writel(flag_val, ppmu_base + PPMU_FLAG);
	__raw_writel(pmnc_val, ppmu_base);
	__raw_writel(0x0, ppmu_base + PPMU_CCNT);

	/* Set PPMU Event */
	ppmu->event[PPMU_PMNCNT0] = RD_DATA_COUNT;
	exynos4270_ppmu_setevent(ppmu_base, PPMU_PMNCNT0,
				ppmu->event[PPMU_PMNCNT0]);

	ppmu->event[PPMU_PMCCNT1] = WR_DATA_COUNT;
	exynos4270_ppmu_setevent(ppmu_base, PPMU_PMCCNT1,
				ppmu->event[PPMU_PMCCNT1]);

	ppmu->event[PPMU_PMNCNT3] = RDWR_DATA_COUNT;
	exynos4270_ppmu_setevent(ppmu_base, PPMU_PMNCNT3,
				ppmu->event[PPMU_PMNCNT3]);

	/* Start PPMU */
	local_irq_save(flags);
	exynos4270_ppmu_start(ppmu_base);
	local_irq_restore(flags);

}

static void exynos4270_ppmu_read(struct exynos4270_ppmu *ppmu)
{
	void __iomem *ppmu_base = ppmu->hw_base;
	u32 overflow;
	int i;
	unsigned long flags;

	/* Stop PPMU */
	local_irq_save(flags);
	exynos4270_ppmu_stop(ppmu_base);
	local_irq_restore(flags);

	/* Update local data from PPMU */
	ppmu->ccnt = __raw_readl(ppmu_base + PPMU_CCNT);
	overflow = __raw_readl(ppmu_base + PPMU_FLAG);
	ppmu->ccnt_overflow = overflow & PPMU_CCNT_OVERFLOW;

	for (i = PPMU_PMNCNT0; i < PPMU_PMNCNT_MAX; i++) {
		if (ppmu->event[i] == 0)
			ppmu->count[i] = 0;
		else {
			if (i == PPMU_PMNCNT3)		/* RDWR data count */
				ppmu->count[i] = ((__raw_readl(ppmu_base + PMCNT_OFFSET(i)) << 8) |
						__raw_readl(ppmu_base + PMCNT_OFFSET(i + 1)));
			else
				ppmu->count[i] = __raw_readl(ppmu_base + PMCNT_OFFSET(i));
		}
	}
}

static void exynos4270_ppmu_add(struct exynos4270_ppmu *to, struct exynos4270_ppmu *from)
{
	int i, j;

	for (i = 0; i < PPMU_END; i++) {
		for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++)
			to[i].count[j] += from[i].count[j];

		to[i].ccnt += from[i].ccnt;
		if (to[i].ccnt < from[i].ccnt)
			to[i].ccnt_overflow = true;

		to[i].ns += from[i].ns;

		if (from[i].ccnt_overflow)
			to[i].ccnt_overflow = true;
	}
}

static void exynos4270_ppmu_handle_clear(struct exynos4270_ppmu_handle *handle)
{
	memset(&handle->ppmu, 0, sizeof(struct exynos4270_ppmu) * PPMU_END);
}

int exynos4270_ppmu_update(enum exynos4270_ppmu_sets filter)
{
	int i, ret = 0;
	struct exynos4270_ppmu_handle *handle;
	enum exynos4270_ppmu_list start, end;

	ret = exynos4270_ppmu_get_filter(filter, &start, &end);
	if (ret < 0)
		return ret;

	for (i = start; i <= end; i++) {
		exynos4270_ppmu_read(&ppmu[i]);
		exynos4270_ppmu_reset(&ppmu[i]);
	}

	list_for_each_entry(handle, &exynos4270_ppmu_handle_list, node)
		exynos4270_ppmu_add(handle->ppmu, ppmu);

	return ret;
}

int exynos4270_ppmu_get_filter(enum exynos4270_ppmu_sets filter,
	enum exynos4270_ppmu_list *start, enum exynos4270_ppmu_list *end)
{
	switch (filter) {
	case PPMU_SET_DDR:
		*start = PPMU_DMC_0;
		*end = PPMU_DMC_1;
		break;
	case PPMU_SET_INT:
		*start = PPMU_RIGHT;
		*end = PPMU_LEFT;
		break;
	case PPMU_SET_CPU:
		*start = PPMU_CPU;
		*end = PPMU_CPU;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int exynos4270_ppmu_get_busy(struct exynos4270_ppmu_handle *handle,
				enum exynos4270_ppmu_sets filter,
				unsigned int *ccnt, unsigned long *pmcnt)
{
	unsigned long flags;
	int i, ret, temp = 0;
	int busy = 0;
	enum exynos4270_ppmu_list start, end;
	unsigned int max_ccnt = 0, temp_ccnt = 0;
	unsigned long max_pmcnt = 0, temp_pmcnt = 0;

	ret = exynos4270_ppmu_get_filter(filter, &start, &end);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&exynos4270_ppmu_lock, flags);

	exynos4270_ppmu_update(filter);

	for (i = start; i <= end; i++) {
		if (handle->ppmu[i].ccnt_overflow) {
			busy = -EOVERFLOW;
			break;
		}

		temp_ccnt = handle->ppmu[i].ccnt;
		temp_pmcnt = handle->ppmu[i].count[PPMU_PMNCNT3];

		if (temp_ccnt > 0 && temp_pmcnt > 0) {
			temp = temp_pmcnt * 100;
			temp /= temp_ccnt;
		} else {
			continue;
		}

		if (temp > busy) {
			busy = temp;
			max_ccnt = temp_ccnt;
			max_pmcnt = temp_pmcnt;
		}

	}

	*ccnt = max_ccnt;
	*pmcnt = max_pmcnt;

	exynos4270_ppmu_handle_clear(handle);

	spin_unlock_irqrestore(&exynos4270_ppmu_lock, flags);

	return busy;

}

void exynos4270_ppmu_put(struct exynos4270_ppmu_handle *handle)
{
	unsigned long flags;

	spin_lock_irqsave(&exynos4270_ppmu_lock, flags);

	list_del(&handle->node);

	spin_unlock_irqrestore(&exynos4270_ppmu_lock, flags);

	kfree(handle);
}

struct exynos4270_ppmu_handle *exynos4270_ppmu_get(enum exynos4270_ppmu_sets filter)
{
	struct exynos4270_ppmu_handle *handle;
	unsigned long flags;

	handle = kzalloc(sizeof(struct exynos4270_ppmu_handle), GFP_KERNEL);
	if (!handle)
		return NULL;

	spin_lock_irqsave(&exynos4270_ppmu_lock, flags);

	exynos4270_ppmu_update(filter);
	list_add_tail(&handle->node, &exynos4270_ppmu_handle_list);

	spin_unlock_irqrestore(&exynos4270_ppmu_lock, flags);

	return handle;
}
