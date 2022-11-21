/*
 * linux/arch/arm/mach-mmp/include/mach/gpio-edge.h
 *
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MMP_ARCH_GPIO_EDGE_H
#define MMP_ARCH_GPIO_EDGE_H

/*
 * struct gpio_edge_desc - gpio edge wakeup source descriptor
 * @list:	list control of the descriptors
 * @gpio:	the gpio number of the wakeup source
 * @mfp:	the mfp pin index of the wakeup source
 * @type:      the type of edge detect for the pin, rising/falling/both
 * @data:	any kind private data passed to the handler
 * @handler:	optional, the handler for the certain wakeup source detected
 */
struct gpio_edge_desc {
	struct list_head list;
	int gpio;
	int mfp;
	unsigned int type;
	void *data;
	void (*handler)(int, void *);
};

extern int mmp_gpio_edge_add(struct gpio_edge_desc *edge);
extern int mmp_gpio_edge_del(struct gpio_edge_desc *edge);
extern void mmp_gpio_edge_enable(void);
extern void mmp_gpio_edge_disable(void);
extern void mmp_gpio_edge_init(void __iomem *base, int mfp_num, int gpio_num);

#endif /* MMP_ARCH_GPIO_EDGE_H */
