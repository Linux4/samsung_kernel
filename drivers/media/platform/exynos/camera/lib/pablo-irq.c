// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS IS (Imaging Subsystem) driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/irq.h>

#include "pablo-irq.h"
#include "pablo-debug.h"
#include "pablo-lib.h"
#include "is-common-config.h"
#include "is-core.h"

#define CALL_PIRQ_OPS(pirq, op, args...) \
	(((!ZERO_OR_NULL_PTR(pirq)) && (pirq)->ops && (pirq)->ops->op) ? ((pirq)->ops->op(args)) : 0)

static struct pablo_irq *pirq;

void pablo_set_affinity_irq(unsigned int irq, bool enable)
{
	struct pablo_lib_platform_data *plpd;
	char buf[10];
	struct cpumask cpumask;

	plpd = pablo_lib_get_platform_data();
	snprintf(buf, sizeof(buf), "%d-%d", plpd->mtarget_range[PABLO_MTARGET_RSTART],
			plpd->mtarget_range[PABLO_MTARGET_REND]);

	if (!irq) {
		err("irq num is zero");
		return;
	}

	if (IS_ENABLED(IRQ_MULTI_TARGET_CL0)) {
		cpulist_parse(buf, &cpumask);
		CALL_PIRQ_OPS(pirq, set_affinity_hint, irq, enable ? &cpumask : NULL);
	}
}
EXPORT_SYMBOL_GPL(pablo_set_affinity_irq);

int pablo_request_irq(unsigned int irq,
	irqreturn_t (*func)(int, void *),
	const char *name,
	unsigned int added_irq_flags,
	void *dev)
{
	int ret = 0;

	ret = CALL_PIRQ_OPS(pirq, request_irq, irq, func, IS_HW_IRQ_FLAG | added_irq_flags, name, dev);
	if (ret) {
		err("%s, request_irq fail", name);
		return ret;
	}

	pablo_set_affinity_irq(irq, true);

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_request_irq);

void pablo_free_irq(unsigned int irq, void *dev)
{
	pablo_set_affinity_irq(irq, false);
	CALL_PIRQ_OPS(pirq, free_irq, irq, dev);
}
EXPORT_SYMBOL_GPL(pablo_free_irq);

static const struct pablo_irq_operations pirq_ops = {
	.set_affinity_hint = irq_set_affinity_hint,
	.request_irq = request_irq,
	.free_irq = free_irq,
};

int pablo_irq_probe(struct device *dev)
{
	pirq = devm_kzalloc(dev, sizeof(struct pablo_irq), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(pirq)) {
		probe_err("failed to allocate pablo_irq");
		return -ENOMEM;
	}

	pirq->ops = &pirq_ops;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_irq_probe);

struct pablo_irq *pablo_irq_get(void)
{
	return pirq;
}
KUNIT_EXPORT_SYMBOL(pablo_irq_get);
