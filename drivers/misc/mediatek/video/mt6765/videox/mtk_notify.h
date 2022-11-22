/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Author Harry.Lee <Harry.Lee@mediatek.com>
 */

#ifndef __MTK_NOTIFY_H
#define __MTK_NOTIFY_H

#include <linux/of.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/errno.h>
#include <linux/trace_events.h>
#include "ddp_hal.h"
#include "ddp_irq.h"
#include "primary_display.h"

struct mtk_uevent_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;
};

int uevent_dev_register(struct mtk_uevent_dev *sdev);
int noti_uevent_user(struct mtk_uevent_dev *sdev, int state);

extern struct mtk_uevent_dev uevent_data;

void vsync_print_handler(enum DISP_MODULE_ENUM module,
		unsigned int reg_val);

#endif
