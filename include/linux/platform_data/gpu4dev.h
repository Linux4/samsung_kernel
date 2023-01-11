/*
 * linux/arch/arm/mach-mmp/include/mach/gpu4dev.h
 *
 *  Copyright (C) 2012 Marvell International Ltd.
 *  All rights reserved.
 *
 *  2014-02-27	Liang Peng <pengl@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <linux/notifier.h>

#define GPUFREQ_POSTCHANGE_UP   (0xBEAD)
#define GPUFREQ_POSTCHANGE_DOWN (0xDAEB)

#ifdef CONFIG_DEVFREQ_GOV_THROUGHPUT
#define generate_evoc(gpu, cpu) ((gpu) || (cpu))
int gpufeq_register_dev_notifier(struct srcu_notifier_head *gpu_notifier_chain);
#endif

