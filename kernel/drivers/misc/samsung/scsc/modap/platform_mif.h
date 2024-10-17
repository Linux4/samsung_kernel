/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PLATFORM_MIF_H__
#define __PLATFORM_MIF_H__
/* Interfaces it Uses */
#include "scsc_mif_abs.h"
#include "platform_mif/modap/platform_mif.h"
#include "platform_mif_module.h"
#include "miframman.h"
#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
#include "pmu_cal.h"
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>

#include <scsc/scsc_exynos-smc.h>
#include <scsc/scsc_exynos-itmon.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#endif
#include <linux/mfd/syscon.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_irq.h>
#include <scsc/scsc_logring.h>
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/exynos-soc.h>
#else
#include <linux/soc/samsung/exynos-soc.h>
#endif
#endif

#ifdef CONFIG_SCSC_QOS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/cal-if.h>
#else
#include <linux/pm_qos.h>
#endif
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
/* Time to wait for CFG_REQ IRQ on 9610 */
#define WLBT_BOOT_TIMEOUT (HZ)

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#include <scsc/scsc_debug-snapshot.h>

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/memlogger.h>
#else
#include <soc/samsung/memlogger.h>
#endif
#endif

#include <linux/cpufreq.h>

//#include "pcie_s5e9925.h"
#include "../pmu_host_if.h"

#include <linux/kthread.h>
#include <linux/kfifo.h>

#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
#include "../common/platform_mif_property_api.h"
#endif
#include "platform_mif_irq_api.h"

static struct platform_mif *g_platform;

extern void acpm_init_eint_clk_req(u32 eint_num);
extern int exynos_pcie_rc_chk_link_status(int ch_num);

static unsigned long sharedmem_base;
static size_t sharedmem_size;

enum pcie_ctrl_state {
	PCIE_STATE_OFF = 0,
	PCIE_STATE_ON,
	PCIE_STATE_SLEEP,
	PCIE_STATE_HOLD_HOST,
	PCIE_STATE_ERROR,
};




#define PCIE_LINK_ON    0
#define PCIE_LINK_OFF   1

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev);
struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface);
void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface);
int platform_mif_suspend(struct scsc_mif_abs *interface);
void platform_mif_resume(struct scsc_mif_abs *interface);
void platform_mif_complete(struct scsc_mif_abs *interface);
void platform_wlbt_regdump(struct scsc_mif_abs *interface);
void platform_set_wlbt_regs(struct platform_mif *platform);
bool is_ka_patch_in_fw(struct platform_mif *platform);
#endif
