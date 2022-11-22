/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MTK_NOTIFY_H
#define __MTK_NOTIFY_H

#include <linux/of.h>
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
#include <linux/notifier.h>

enum {
	MTK_FPS_CHANGE = 0,
};

struct mtk_uevent_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;
};

struct mtk_notifier {
	struct notifier_block notifier;
	int fps;
};

int uevent_dev_register(struct mtk_uevent_dev *sdev);
int noti_uevent_user(struct mtk_uevent_dev *sdev, int state);
int mtk_notifier_activate(void);
int mtk_register_client(struct notifier_block *nb);

int noti_uevent_user_by_drm(struct drm_device *drm, int state);

#endif
