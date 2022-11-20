/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * KQ(Kernel Quality) NAD driver implementation
 *	: SeungYoung Lee <seung0.lee@samsung.com>
 */

#ifndef __KQ_NAD_H__
#define __KQ_NAD_H__

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100)
#include <linux/kq/kq_nad_exynos2100.h>
#elif IS_ENABLED(CONFIG_SOC_EXYNOS3830)
#include <linux/kq/kq_nad_exynos3830.h>
#elif IS_ENABLED(CONFIG_SEC_KQ_NAD_TEST)
#include <linux/kq/kq_nad_exynos2100.h>
#else
#include <linux/kq/kq_nad_default.h>
#endif

#include <linux/kq/kq_nad_result.h>

#define KQ_NAD_MAX_CMD_NAME		10
#define KQ_NAD_MAX_CMD_SIZE		3

#define KQ_NAD_FLAG_SMD_FIRST		1005
#define KQ_NAD_FLAG_SMD_SECOND		1006
#define KQ_NAD_FLAG_ACAT_FIRST		5001
#define KQ_NAD_FLAG_ACAT_SECOND		5002
#define KQ_NAD_FLAG_EXTEND_FIRST	1007
#define KQ_NAD_FLAG_EXTEND_SECOND	1008

#define KQ_NAD_BIT_PASS			0x00002000

#define KQ_NAD_MAGIC_ERASE			0xA5000000
#define KQ_NAD_MAGIC_ACAT			0x00A50000
#define KQ_NAD_MAGIC_DRAM_TEST			0x0000A500
#define KQ_NAD_MAGIC_ACAT_DRAM_TEST		0x00A5A500
#define KQ_NAD_MAGIC_NADX_TEST			0x000000A5
#define KQ_NAD_MAGIC_NADX_DONE			0x0000005A
#define KQ_NAD_MAGIC_REBOOT			0x5A000000
#define KQ_NAD_MAGIC_CONSTANT_XOR		0xABCDABCD

#define KQ_NAD_CHECK_NAD_STATUS			(0x1)
#define KQ_NAD_CHECK_NAD_SECOND_STATUS		(0x2)
#define KQ_NAD_CHECK_ACAT_STATUS		(0x4)
#define KQ_NAD_CHECK_ACAT_SECOND_STATUS		(0x8)
#define KQ_NAD_CHECK_NADX_STATUS		(0x10)
#define KQ_NAD_CHECK_NADX_SECOND_STATUS		(0x20)

#define KQ_NAD_LOOP_COUNT_NADX		444

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)
/*
 * disable cpu boot qos lock : flexable.cpuboot value is valid ( 1, 2, 3, 4 )
 * cpufreq_disable_boot_qos_lock does not apply max lock
 */
enum {
	FLEXBOOT_LIT = 1,
	FLEXBOOT_MID,
	FLEXBOOT_BIG,
	FLEXBOOT_ALL,
};
extern int flexable_cpu_boot;
#endif //IS_ENABLED(CONFIG_ARM_EXYNOS_ACME_DISABLE_BOOT_LOCK)

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)
/*
 * disable exynos devfreq boot qos lock
 */
extern int flexable_dev_boot;
#endif //IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DISABLE_BOOT_LOCK)

enum {
	KQ_NAD_DAS_NAME_NONE = 0,
	KQ_NAD_DAS_NAME_DRAM,
	KQ_NAD_DAS_NAME_AP,
	KQ_NAD_DAS_NAME_SET,
	KQ_NAD_DAS_NAME_CP,
};

enum {
	KQ_NAD_RESULT_PASS = 0,
	KQ_NAD_RESULT_FAIL,
	KQ_NAD_RESULT_MAIN,

	KQ_NAD_RESULT_END,
};

enum {
	KQ_NAD_SYSFS_STAT = 0,
	KQ_NAD_SYSFS_ERASE,
	KQ_NAD_SYSFS_ACAT,
	KQ_NAD_SYSFS_DRAM,
	KQ_NAD_SYSFS_ALL,
	KQ_NAD_SYSFS_SUPPORT,
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_API)
	KQ_NAD_SYSFS_API,
#endif
	KQ_NAD_SYSFS_VERSION,
#if IS_ENABLED(CONFIG_SEC_KQ_NAD_X)
	KQ_NAD_SYSFS_FAC_RESULT,
	KQ_NAD_SYSFS_NADX_RUN,
#endif
	KQ_NAD_SYSFS_REBOOT,
	KQ_NAD_SYSFS_END,
};

static ssize_t kq_nad_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t kq_nad_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

#define KQ_NAD_ATTR(_name)	\
{	\
	.attr = { .name = #_name, .mode = 0664 },	\
	.show = kq_nad_show_attrs,	\
	.store = kq_nad_store_attrs,	\
}

struct kq_nad_fail {
	unsigned int das;
	unsigned int block;
	unsigned int level;
	unsigned int vector;
};

struct kq_nad_mparam_inform {
	unsigned int result;
	unsigned int inform1;
	unsigned int inform2;
	unsigned int inform3;
	struct kq_nad_fail fail;
};

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
struct kq_nad_mparam_vddcal {
	unsigned int lit;
	unsigned int mid;
	unsigned int big;
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
struct kq_nad_mparam_correlation {
	char info[KQ_NAD_CORRELATION_MPARAM_MAX_LEN];
};
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)

struct kq_nad_env {
	unsigned int status;
	void __iomem *inform4;
	struct kq_nad_mparam_inform smd;
	struct kq_nad_mparam_inform smd_second;
	struct kq_nad_mparam_inform acat;
	struct kq_nad_mparam_inform acat_second;
	struct kq_nad_mparam_inform nadx;
	struct kq_nad_mparam_inform nadx_second;

#if IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)
	struct kq_nad_mparam_vddcal vddcal;
#endif //IS_ENABLED(CONFIG_SEC_KQ_NAD_VDD_CAL)

#if IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
	unsigned int correlation_magic;
	struct kq_nad_mparam_correlation correlation[KQ_NAD_MPARAM_CORRELATION_LOGIC_BLOCK_END];
#endif //IS_ENABLED(CONFIG_SEC_KQ_CORRELATION_RESULT)
};

static struct kq_nad_env kq_sec_nad_env;

#endif /* __KQ_NAD_H__ */
