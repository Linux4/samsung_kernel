/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef __CLK_PLAT_H
#define __CLK_PLAT_H

#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "clkbuf-util.h"

struct platform_operation {
	/*redfine function*/
	/*example:  int (*get_pmrcen)(void *data, u32 *out);*/
};

struct platform_hdlr {
	void *data;
	spinlock_t *lock;
	struct platform_operation *ops;
};

#endif
