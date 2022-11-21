/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "./../lastbus.h"
#include "plat_lastbus.h"
#include <linux/seqlock.h>
#include <linux/irqflags.h>

void timeout_dump(void)
{
	unsigned int i;
	unsigned long meter, w_cnt = 0, r_cnt = 0;
	void __iomem *meter_add;

	for (i = 0; i <= 1; ++i) {
		meter_add = ioremap_nocache((0x102004B0 + 4*i), 0x100);
		meter = ioread32(meter_add);
		w_cnt = meter & 0x3f;
		r_cnt = (meter >> 8) & 0x3f;
		if ((w_cnt < 10) || (r_cnt < 10))
			pr_info("No bus Timeout!\n");
	}

	meter_add = ioremap_nocache(0x102004B8, 0x100);
	meter = ioread32(meter_add);
	w_cnt = meter & 0x3f;
	r_cnt = (meter >> 8) & 0x3f;
	if ((w_cnt < 10) || (r_cnt < 10))
		pr_info("No bus Timeout!!\n");
}
