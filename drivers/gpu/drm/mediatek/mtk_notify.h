/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author Harry.Lee <Harry.Lee@mediatek.com>
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
	MTK_POWER_MODE_CHANGE = 1,
	MTK_POWER_MODE_DONE = 2,
};

enum {
	DISP_NONE = -1,
	DISP_OFF = 0,
	DISP_ON = 1,
	DISP_DOZE_SUSPEND = 2,
	DISP_DOZE = 3,
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
	int power_mode;
};

int uevent_dev_register(struct mtk_uevent_dev *sdev);
int noti_uevent_user(struct mtk_uevent_dev *sdev, int state);
int mtk_notifier_activate(void);
int mtk_register_client(struct notifier_block *nb);
int mtk_check_powermode(struct drm_atomic_state *state, int mode);
int noti_uevent_user_by_drm(struct drm_device *drm, int state);

#endif
