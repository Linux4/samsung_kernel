/*
 *  Copyright (C) 2012 Marvell International Ltd.
 *  All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <linux/notifier.h>

#define CAMFREQ_POSTCHANGE_UP   (0xF1)
#define CAMFREQ_POSTCHANGE_DOWN (0xF2)

#ifdef CONFIG_DEVFREQ_GOV_THROUGHPUT
int camfeq_register_dev_notifier(struct srcu_notifier_head *cam_notifier_chain);
#endif
