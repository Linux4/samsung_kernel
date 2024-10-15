/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PLATFORM_MIF_H__
#define __PLATFORM_MIF_H__
/* Interfaces it Uses */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-smc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <scsc/scsc_logring.h>
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/exynos-soc.h>
#else
#include <linux/soc/samsung/exynos-soc.h>
#endif
#endif

#ifdef CONFIG_SCSC_QOS
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/cal-if.h>
#if defined(CONFIG_SOC_S5E9925)
#include <dt-bindings/clock/s5e9925.h>
#endif
#if defined(CONFIG_SOC_S5E9935)
#include <dt-bindings/clock/s5e9935.h>
#endif
#if defined(CONFIG_SOC_S5E9945)
#include <dt-bindings/clock/s5e9945.h>
#endif
#endif

#if !(defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E9935) || defined(CONFIG_SOC_S5E9945))
#error Target processor not selected
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
/* Time to wait for CFG_REQ IRQ on 9610 */
#define WLBT_BOOT_TIMEOUT (HZ)

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/debug-snapshot.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/memlogger.h>
#else
#include <soc/samsung/memlogger.h>
#endif
#endif

#include <linux/cpufreq.h>

#include "platform_mif_module.h"
#include "miframman.h"

#include "scsc_mif_abs.h"
#include <linux/exynos-pci-noti.h>
#include <linux/pci.h>
#include <linux/resource.h>

#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/suspend.h>

#include "pcie.h"
#include "pmu_host_if.h"

#include "platform_mif/pcie/platform_mif.h"

#if defined(CONFIG_SCSC_BB_REDWOOD)
#define GPIO_CMGP 0x14030000
#define GPM03_CON 0x60
#define GPM03_DAT 0x64
#define GPM03_PUD 0x68
#define GPM03_DRV 0x6C
#endif

enum pcie_ctrl_state {
	PCIE_STATE_OFF = 0,
	PCIE_STATE_ON,
	PCIE_STATE_SLEEP,
	PCIE_STATE_HOLD_HOST,
	PCIE_STATE_ERROR,
};

enum pcie_event_type {
	PCIE_EVT_RST_ON = 0,
	PCIE_EVT_RST_OFF,
	PCIE_EVT_CLAIM,
	PCIE_EVT_CLAIM_CB,
	PCIE_EVT_RELEASE,
	PCIE_EVT_PMU_OFF_REQ,
	PCIE_EVT_PMU_ON_REQ,
};


struct event_record {
	enum pcie_event_type event;
	bool complete;
	int (*pcie_wakeup_cb)(void *first, void *second); /* claim_complete callback fn to call after turning PCIE on */
	void *pcie_wakeup_cb_data_service;
	void *pcie_wakeup_cb_data_dev;
} __packed;

inline void platform_int_debug(struct platform_mif *platform);

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)
#define PCIE_LINK_ON	0
#define PCIE_LINK_OFF	1

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev);
struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface);
void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface);
int platform_mif_suspend(struct scsc_mif_abs *interface);
void platform_mif_resume(struct scsc_mif_abs *interface);
void platform_mif_complete(struct scsc_mif_abs *interface);
#endif
