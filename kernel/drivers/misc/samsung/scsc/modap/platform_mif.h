/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
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
#include <scsc/scsc_exynos-smc.h>
#include <scsc/scsc_exynos-itmon.h>
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
#include <linux/of_irq.h>
#include <scsc/scsc_logring.h>
#include "../platform_mif_module.h"
#include "../scsc_mif_abs.h"
#include "../miframman.h"

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

#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
#include "../pmu_cal.h"
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

enum wlbt_irqs {
	PLATFORM_MIF_MBOX,
	PLATFORM_MIF_MBOX_WPAN,
	PLATFORM_MIF_ALIVE,
	PLATFORM_MIF_WDOG,
	PLATFORM_MIF_CFG_REQ,
	PLATFORM_MIF_MBOX_PMU,
	/* must be last */
	PLATFORM_MIF_NUM_IRQS
};

enum wlbt_boot_state {
	WLBT_BOOT_IN_RESET = 0,
	WLBT_BOOT_WAIT_CFG_REQ,
	WLBT_BOOT_ACK_CFG_REQ,
	WLBT_BOOT_CFG_DONE,
	WLBT_BOOT_CFG_ERROR
};

struct platform_mif {
	struct scsc_mif_abs    interface;
	struct scsc_mbox_s     *mbox;
	struct platform_device *pdev;

	struct device          *dev;

	struct {
		int irq_num;
		int flags;
		atomic_t irq_disabled_cnt;
	} wlbt_irq[PLATFORM_MIF_NUM_IRQS];

	/* MIF registers preserved during suspend */
	struct {
		u32 irq_bit_mask;
		u32 irq_bit_mask_wpan;
	} mif_preserve;

	/* register MBOX memory space */
	void __iomem  *base;
	void __iomem  *base_wpan;
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	void __iomem  *base_pmu;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	size_t        mem_size_region2;
	void __iomem  *mem_region2;
#endif
	/* pmu syscon regmap */
	struct regmap **regmap;

	/* Signalled when CFG_REQ IRQ handled */
	struct completion cfg_ack;

	/* State of CFG_REQ handler */
	enum wlbt_boot_state boot_state;

	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t        mem_size;
	void __iomem  *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void          (*wlan_handler)(int irq, void *data);
	void          (*wpan_handler)(int irq, void *data);
	void          *irq_dev;
	void          *irq_dev_wpan;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	void          (*reset_request_handler)(int irq, void *data);
	void          *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
		bool qos_enabled;
#if IS_ENABLED(CONFIG_WLBT_QOS_CPUFREQ_POLICY)
		struct {
			/* For CPUCL-QoS */
			struct cpufreq_policy *cpucl0_policy;
			struct cpufreq_policy *cpucl1_policy;
#if defined(CONFIG_SOC_S5E8845)
			struct dvfs_rate_volt *mif_domain_fv;
#endif
		} qos;
#else
	/* QoS table */
	struct qos_table *qos;
#endif
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct work_struct cfgreq_wq;
	struct workqueue_struct *cfgreq_workq;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dma_addr_t paddr;
	int (*platform_mif_set_mem_region2)
		(struct scsc_mif_abs *interface,
		void __iomem *_mem_region2,
		size_t _mem_size_region2);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Callback function to check recovery status */
	bool (*recovery_disabled)(void);
#endif
	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void          (*pmu_handler)(int irq, void *data);
	void          *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
        struct notifier_block itmon_nb;
#endif

#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
	struct device_node *np; /* cache device node */
#endif
#ifdef CONFIG_SOC_S5E8845
	int dbus_baaw0_allowed_list_set, dbus_baaw1_allowed_list_set;
#endif
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
