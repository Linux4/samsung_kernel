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

#ifndef IS_IRQ_H
#define IS_IRQ_H

#include <linux/interrupt.h>

struct pablo_irq_operations {
	int (*set_affinity_hint) (unsigned int irq, const struct cpumask *m);
	int (*request_irq) (unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
	const void * (*free_irq) (unsigned int irq, void *dev_id);
};

struct pablo_irq {
	const struct pablo_irq_operations *ops;
};

void pablo_set_affinity_irq(unsigned int irq, bool enable);
int pablo_request_irq(unsigned int irq,
		irqreturn_t (*func)(int, void *),
		const char *name,
		unsigned int added_irq_flags,
		void *dev);
void pablo_free_irq(unsigned int irq, void *dev);

int pablo_irq_probe(struct device *dev);
struct pablo_irq *pablo_irq_get(void);
#endif /* IS_IRQ_H */
