/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PM_RUNTIME_H
#define __ASM_ARCH_PM_RUNTIME_H __FILE__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#include <mach/regs-pmu.h>
#include <mach/sysmmu.h>

#include <plat/clock.h>
#include <plat/devs.h>

#include <plat/devs.h>

#define CONFIG_RUNTIME_PM_V2

#define PM_DOMAIN_PREFIX	"PM DOMAIN: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef PM_DOMAIN_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_DOMAIN_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

struct exynos_pm_domain;

struct exynos_pd_callback {
	const char *name;
	int (*on_pre)(struct exynos_pm_domain *pd);
	int (*on)(struct exynos_pm_domain *pd, int power_flags);
	int (*on_post)(struct exynos_pm_domain *pd);
	int (*off_pre)(struct exynos_pm_domain *pd);
	int (*off)(struct exynos_pm_domain *pd, int power_flags);
	int (*off_post)(struct exynos_pm_domain *pd);
	unsigned int status;
};

struct exynos_pm_domain {
	struct generic_pm_domain genpd;
	char const *name;
	void __iomem *base;
	int (*on)(struct exynos_pm_domain *pd, int power_flags);
	int (*off)(struct exynos_pm_domain *pd, int power_flags);
	int (*check_status)(struct exynos_pm_domain *pd);
	struct exynos_pd_callback *cb;
	unsigned int status;
	struct exynos_pm_domain *sub_pd;
	unsigned int bts;

	struct mutex access_lock;
	unsigned int enable;
};

struct exynos_device_pd_link {
	struct exynos_pm_domain *pd;
	struct platform_device *pdev;
	unsigned int status;
};

/* pd power on/off latency is less than 1ms */
#define EXYNOS_GPD(ENABLE, BASE, NAME, CB, SUB, ON, OFF, BTS)		\
{									\
	.genpd		=	{					\
		.name		=	NAME,				\
		.power_off	=	exynos_genpd_power_off,		\
		.power_on	=	exynos_genpd_power_on,		\
		.power_on_latency_ns = 1000000,				\
		.power_off_latency_ns = 1000000,			\
	},								\
	.name			=	NAME,				\
	.base			=	(void __iomem *)BASE,		\
	.on			=	ON,				\
	.off			=	OFF,				\
	.check_status		=	exynos_pd_status,		\
	.cb			=	CB,				\
	.status			=	true,				\
	.sub_pd			=	SUB,				\
	.bts			=	BTS,				\
	.enable			=	ENABLE,				\
}

#define EXYNOS_MASTER_GPD(ENABLE, _BASE, NAME, CB, SUB, BTS)	\
EXYNOS_GPD(ENABLE,						\
	_BASE,							\
	NAME,							\
	CB,							\
	SUB,							\
	exynos_pd_power,					\
	exynos_pd_power,					\
	BTS							\
)

#define EXYNOS_SUB_GPD(ENABLE, NAME, CB, BTS)			\
EXYNOS_GPD(ENABLE,						\
	NULL,							\
	NAME,							\
	CB,							\
	NULL,							\
	exynos_pd_power_dummy,					\
	exynos_pd_power_dummy,					\
	BTS							\
)

#define for_each_power_domain(index)		for(;index->name ;index++)
#define for_each_dev_pd_link(dev_pd_link)	for(;dev_pd_link->pd ;dev_pd_link++)

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd);

int exynos_genpd_power_off(struct generic_pm_domain *genpd);
int exynos_genpd_power_on(struct generic_pm_domain *genpd);
int exynos_pd_power(struct exynos_pm_domain *pd, int power_flags);
int exynos_pd_status(struct exynos_pm_domain *pd);
int exynos_pd_power_dummy(struct exynos_pm_domain *pd, int power_flags);
#ifdef CONFIG_SOC_EXYNOS5260
struct exynos_pm_domain * exynos5260_pm_domain(void);
struct exynos_device_pd_link * exynos5260_device_pd_link(void);
enum master_pd_index {
	ID_PD_G3D,
	ID_PD_MFC,
	ID_PD_DISP,
	ID_PD_GSCL,
	ID_PD_MAU,
	ID_PD_G2D,
	ID_PD_ISP,
	NUM_OF_PD,
};

extern int exynos_get_runtime_pd_status(enum master_pd_index index);
extern struct exynos_pm_domain * exynos_get_power_domain(const char * pd_name);
#endif

#ifdef CONFIG_SOC_EXYNOS4415
struct exynos_pm_domain *exynos4415_pm_domain(void);
struct exynos_device_pd_link *exynos4415_device_pd_link(void);
enum master_pd_index {
	ID_PD_CAM,
	ID_PD_TV,
	ID_PD_MFC,
	ID_PD_G3D,
	ID_PD_LCD0,
	ID_PD_MAU,
	ID_PD_ISP0,
	NUM_OF_PD,
};
#else
static inline struct exynos_pm_domain *exynos4415_pm_domain(void)
{ return NULL; }
static inline struct exynos_device_pd_link *exynos4415_device_pd_link(void)
{ return NULL; }
#endif /* CONFIG_SOC_EXYNOS4415 */

#endif /* __ASM_ARCH_PM_RUNTIME_H */
