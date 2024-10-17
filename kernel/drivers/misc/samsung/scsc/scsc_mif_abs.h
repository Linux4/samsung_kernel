/****************************************************************************
*
* Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

#ifndef __SCSC_MIF_ABS_H
#define __SCSC_MIF_ABS_H

#include <linux/version.h>

#ifdef CONFIG_SCSC_QOS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/cpufreq.h>
#else
#include <linux/pm_qos.h>
#endif
#endif
#include <linux/types.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_mx.h>

#include "scsc_mif_abs/scsc_mif_abs.h"

#ifdef CONFIG_SCSC_SMAPPER
#define SCSC_MIF_SMAPPER_MAX_BANKS 32

struct scsc_mif_smapper_info {
	u32 num_entries;
	u32 mem_range_bytes;
};

enum scsc_mif_abs_bank_type {
	SCSC_MIF_ABS_NO_BANK = 0,
	SCSC_MIF_ABS_SMALL_BANK = 1,
	SCSC_MIF_ABS_LARGE_BANK = 2
};
#endif

struct scsc_mif_abs_driver {
	char *name;
	void (*probe)(struct scsc_mif_abs_driver *abs_driver, struct scsc_mif_abs *abs);
	void (*remove)(struct scsc_mif_abs *abs);
};

extern void scsc_mif_abs_register(struct scsc_mif_abs_driver *driver);
extern void scsc_mif_abs_unregister(struct scsc_mif_abs_driver *driver);

/* mmap-debug driver */
struct scsc_mif_mmap_driver {
	char *name;
	void (*probe)(struct scsc_mif_mmap_driver *mmap_driver, struct scsc_mif_abs *abs);
	void (*remove)(struct scsc_mif_abs *abs);
};

extern void scsc_mif_mmap_register(struct scsc_mif_mmap_driver *mmap_driver);
extern void scsc_mif_mmap_unregister(struct scsc_mif_mmap_driver *mmap_driver);
#endif
