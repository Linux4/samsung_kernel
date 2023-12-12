/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_NOTIFY_H__
#define __MTK_DISP_NOTIFY_H__

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/notifier.h>

/* A hardware display blank change occurred */
#define MTK_DISP_EARLY_EVENT_BLANK	0x00
#define MTK_DISP_EVENT_BLANK		0x01

/*Tab A9 code for AX6739A-2213 by yuli at 20230711 start*/
#define MTK_DISP_ESD_RECOVERY_RESUME		0x10
#define MTK_DISP_ESD_RECOVERY_SUSPEND		0x11
/*Tab A9 code for AX6739A-2213 by yuli at 20230711 end*/

enum {
	/* disp power on */
	MTK_DISP_BLANK_UNBLANK,
	/* disp power off */
	MTK_DISP_BLANK_POWERDOWN,
};

int mtk_disp_notifier_register(const char *source, struct notifier_block *nb);
int mtk_disp_notifier_unregister(struct notifier_block *nb);
int mtk_disp_notifier_call_chain(unsigned long val, void *v);

#endif
