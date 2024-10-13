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
#include "platform_mif_module.h"
#include "miframman.h"
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

#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
#include "pmu_cal.h"
#endif

#include <linux/cpufreq.h>
#include "pcie_s6165.h"
#include "pmu_host_if.h"

#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/suspend.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif

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

struct platform_mif {
	struct scsc_mif_abs interface;
	struct scsc_mbox_s *mbox;
	struct platform_device *pdev;
	struct device *dev;
	int pmic_gpio;
	int wakeup_irq;
	int gpio_num;
	int clk_irq;

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
	void __iomem *base;
	void __iomem *base_wpan;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	size_t mem_size_region2;
	void __iomem *mem_region2;
#endif
	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t mem_size;
	void __iomem *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void (*wlan_handler)(int irq, void *data);
	void (*wpan_handler)(int irq, void *data);
	void *irq_dev;
	void *irq_dev_wpan;
	/* spinlock to serialize driver access */
	spinlock_t mif_spinlock;
	void (*reset_request_handler)(int irq, void *data);
	void *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
	struct {
		/* For CPUCL-QoS */
		struct cpufreq_policy *cpucl0_policy;
		struct cpufreq_policy *cpucl1_policy;
		struct cpufreq_policy *cpucl2_policy;
		struct dvfs_rate_volt *int_domain_fv;
		struct dvfs_rate_volt *mif_domain_fv;
	} qos;
	bool qos_enabled;
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dma_addr_t paddr;
	int (*platform_mif_set_mem_region2)(struct scsc_mif_abs *interface, void __iomem *_mem_region2,
					    size_t _mem_size_region2);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Callback function to check recovery status */
	bool (*recovery_disabled)(void);
#endif
	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void (*pmu_handler)(int irq, void *data);
	void (*pmu_pcie_off_handler)(int irq, void *data);
	void (*pmu_error_handler)(int irq, void *data);
	void *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

	struct pcie_mif *pcie;
	int pcie_ch_num;
	struct device_node *np; /* cache device node */

	int (*pcie_wakeup_cb)(void *first, void *second); /* claim_complete callback fn to call after turning PCIE on */
	void *pcie_wakeup_cb_data_service;
	void *pcie_wakeup_cb_data_dev;

	/* FSM */
	spinlock_t kfifo_lock;
	struct task_struct *t;
	wait_queue_head_t event_wait_queue;
	struct completion pcie_on;
	struct kfifo ev_fifo;
	int host_users;
	int fw_users;
	/* required variables for CB serialization */
	bool off_req;
	spinlock_t cb_sync;
	struct notifier_block	pm_nb;

	/* This mutex_lock is only used for exynos_acpm_write_reg function */
	struct mutex acpm_lock;

	/* wakelock to stop suspend during wake# IRQ handler and kthread processing it */
	struct wake_lock wakehash_wakelock;
};

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
