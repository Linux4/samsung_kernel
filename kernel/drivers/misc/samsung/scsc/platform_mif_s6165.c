/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

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
static struct platform_mif *g_platform;

extern void acpm_init_eint_clk_req(u32 eint_num);
extern int exynos_pcie_rc_chk_link_status(int ch_num);

#ifdef CONFIG_SCSC_BB_PAEAN
extern int exynos_acpm_write_reg(struct device_node *acpm_mfd_node, u8 sid, u16 type, u8 reg, u8 value);
#endif

static unsigned long sharedmem_base;
static size_t sharedmem_size;

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

#ifdef CONFIG_SCSC_BB_REDWOOD
static uint on_pinctrl_delay = 5000;
module_param(on_pinctrl_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(on_pinctrl_delay, "wlbt_pinctrl on delay(us)");

static uint off_pinctrl_delay = 160;
module_param(off_pinctrl_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(off_pinctrl_delay, "wlbt_pinctrl off delay(us)");
#endif

static uint max_linkup_count = 3;
module_param(max_linkup_count, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_linkup_count,"max count for linkup re-try");

static uint wlbt_status_timeout_value = 500;
module_param(wlbt_status_timeout_value, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wlbt_status_timeout_value, "wlbt_status_timeout(ms)");

static uint scan2mem_timeout_value = 5000;
module_param(scan2mem_timeout_value, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scan2mem_timeout_value, "scan2mem_timeout(ms)");

static bool enable_scandump_wlbt_status_timeout;
module_param(enable_scandump_wlbt_status_timeout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_scandump_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool force_to_wlbt_status_timeout;
module_param(force_to_wlbt_status_timeout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_to_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

#ifdef CONFIG_SCSC_BB_PAEAN
static bool keep_powered = true;
#else
static bool keep_powered = false;
#endif
module_param(keep_powered, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(keep_powered, "Keep WLBT powered after reset, to sleep flash module");

enum pcie_ctrl_state {
	PCIE_STATE_OFF = 0,
	PCIE_STATE_ON,
	PCIE_STATE_SLEEP,
	PCIE_STATE_HOLD_HOST,
	PCIE_STATE_ERROR,
};

static const char *states[5] = {"OFF", "ON", "SLEEP", "HOLD", "ERROR"};

static enum pcie_ctrl_state state = PCIE_STATE_OFF;

enum pcie_event_type {
	PCIE_EVT_RST_ON = 0,
	PCIE_EVT_RST_OFF,
	PCIE_EVT_CLAIM,
	PCIE_EVT_CLAIM_CB,
	PCIE_EVT_RELEASE,
	PCIE_EVT_PMU_OFF_REQ,
	PCIE_EVT_PMU_ON_REQ,
};

static const char *events[7] = {"RST ON", "RST OFF", "CLAIM", "CLAIM CB", "RELEASE", "PMU OFF", "PMU ON"};

#ifdef CONFIG_SCSC_QOS
#if defined(CONFIG_SOC_S5E9945)
/* cpucl0: 400Mhz / 864Mhz / 1248Mhz for cpucl0 */
static uint qos_cpucl0_lv[] = {0, 4, 8};
/* cpucl1: 672Mhz / 1440Mhz / 2112Mhz for cpucl1 */
static uint qos_cpucl1_lv[] = {0, 8, 15};
/* cpucl2: 672Mhz / 1536Mhz / 2304Mhz for cpucl2 */
static uint qos_cpucl2_lv[] = {0, 9, 17};
/* cpucl3: 672Mhz / 1536Mhz / 2304Mhz for cpucl3 */
static uint qos_cpucl3_lv[] = {0, 9, 17};
/* int: 133Mhz / 332Mhz / 800Mhz */
static uint qos_int_lv[] = {7, 4, 0};
/* mif: 421Mhz / 2028Mhz / 7206Mhz */
static uint qos_mif_lv[] = {12, 5, 0};
#else
/* cpucl0: 400Mhz / 864Mhz / 1248Mhz for cpucl0 */
static uint qos_cpucl0_lv[] = {0, 4, 8};
/* cpucl1: 576Mhz / 1344Mhz / 2208Mhz for cpucl1 */
static uint qos_cpucl1_lv[] = {0, 8, 17};
/* cpucl2: 576Mhz / 1344Mhz / 2208Mhz for cpucl2 */
static uint qos_cpucl2_lv[] = {0, 8, 17};
/* int: 133Mhz / 332Mhz / 800Mhz */
static uint qos_int_lv[] = {7, 4, 0};
/* mif: 421Mhz / 1716Mhz / 3172Mhz */
static uint qos_mif_lv[] = {12, 5, 0};
#endif /*CONFIG_SOC_S5E9945*/
module_param_array(qos_cpucl0_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl0_lv, "S5E9945 DVFS Lv of CPUCL0 to apply Min/Med/Max PM QoS");

module_param_array(qos_cpucl1_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl1_lv, "S5E9945 DVFS Lv of CPUCL1 to apply Min/Med/Max PM QoS");

module_param_array(qos_cpucl2_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl2_lv, "S5E9945 DVFS Lv of CPUCL2 to apply Min/Med/Max PM QoS");

#if defined(CONFIG_SOC_S5E9945)
module_param_array(qos_cpucl3_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl3_lv, "S5E9945 DVFS Lv of CPUCL3 to apply Min/Med/Max PM QoS");
#endif

module_param_array(qos_int_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_int_lv, "DVFS Level of INT");

module_param_array(qos_mif_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_mif_lv, "DVFS Level of MIF");
#endif /*CONFIG_SCSC_QOS*/

static bool is_deferred;

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
#ifdef CONFIG_SCSC_BB_REDWOOD
	int reset_gpio;
	int suspend_gpio;
#endif

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
#if defined(CONFIG_SOC_S5E9945)
		struct cpufreq_policy *cpucl3_policy;
#endif
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

#ifdef CONFIG_SCSC_BB_PAEAN
	/* This mutex_lock is only used for exynos_acpm_write_reg function */
	struct mutex acpm_lock;
#endif

	/* wakelock to stop suspend during wake# IRQ handler and kthread processing it */
	struct wake_lock wakehash_wakelock;
};

inline void platform_int_debug(struct platform_mif *platform);

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)
#define PCIE_LINK_ON	0
#define PCIE_LINK_OFF	1

#ifdef CONFIG_SCSC_BB_REDWOOD
static void platform_mif_control_suspend_gpio(struct scsc_mif_abs *interface, u8 value)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8 ret;
	SCSC_TAG_DEBUG_DEV(PLAT_MIF,platform->dev, "set suspend_gpio %d\n", value);
	gpio_set_value(platform->suspend_gpio, value);
	ret = gpio_get_value(platform->suspend_gpio);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF,platform->dev, "set suspend_gpio %d\n", ret);
}
#endif

static inline int platform_mif_update_users(enum pcie_event_type event, struct platform_mif *platform)
{
	if (event == PCIE_EVT_CLAIM || event == PCIE_EVT_CLAIM_CB) {
		platform->host_users = 1;
	} else if (event == PCIE_EVT_RELEASE) {
		platform->host_users = 0;
	} else if (event == PCIE_EVT_PMU_ON_REQ) {
		platform->fw_users = 1;
	} else if (event == PCIE_EVT_PMU_OFF_REQ) {
		if (platform->fw_users) platform->fw_users -= 1;
	}

	return platform->fw_users + platform->host_users;
}

static void platform_mif_pcie_control_fsm_prepare_fw(struct platform_mif *platform)
{
	/* FW users start with '1' as per design */
	platform->fw_users = 1;
	platform->off_req = false;
	state = PCIE_STATE_OFF;
}

static int platform_mif_pcie_control_fsm(void *data)
{
	struct platform_mif *platform = data;
	struct event_record *ev_data;
	enum pcie_ctrl_state old_state;
	int ret;
	int current_users;
	unsigned long flags;

	ev_data =  kmalloc(sizeof(*ev_data), GFP_KERNEL);
	if (!ev_data)
		return -ENOMEM;

	while (true) {
		wait_event_interruptible(platform->event_wait_queue,
					 kthread_should_stop() ||
					 !kfifo_is_empty(&platform->ev_fifo) ||
				         kthread_should_park());

		if (kthread_should_park()) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "park thread\n");
			kthread_parkme();
		}
		if (kthread_should_stop()) {
			break;
		}

		spin_lock_irqsave(&platform->kfifo_lock, flags);
		ret =  kfifo_out(&platform->ev_fifo, ev_data, sizeof(*ev_data));
		old_state = state;
		spin_unlock_irqrestore(&platform->kfifo_lock, flags);

		if (!ret) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "no event to process\n");
			continue;
		}

		current_users = platform_mif_update_users(ev_data->event, platform);

		switch(state) {
		case PCIE_STATE_OFF:
			if (ev_data->event == PCIE_EVT_RST_ON) {
				ret = pcie_mif_poweron(platform->pcie);
				if (ret)
					state = PCIE_STATE_ERROR;
				else
					state = PCIE_STATE_ON;
			}
			break;

		case PCIE_STATE_ON:
			if (ev_data->event == PCIE_EVT_RST_OFF) {
				pcie_mif_poweroff(platform->pcie);
				state = PCIE_STATE_OFF;
			} else if (ev_data->event == PCIE_EVT_PMU_OFF_REQ) {
				if (current_users == 0) {
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "No PCIe users. Send PMU_AP_MSG_WLBT_PCIE_OFF_ACCEPT\n");
					pcie_set_mbox_pmu_pcie_off(platform->pcie, PMU_AP_MSG_WLBT_PCIE_OFF_ACCEPT);
					pcie_mif_poweroff(platform->pcie);
					state = PCIE_STATE_SLEEP;
				} else {
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PCIe users. Send PMU_AP_MSG_WLBT_PCIE_OFF_REJECT\n");
					/* Since off is rejected, we have to
					 * increase the number of fw_users */
					platform->fw_users += 1;
					pcie_set_mbox_pmu_pcie_off(platform->pcie, PMU_AP_MSG_WLBT_PCIE_OFF_REJECT);
					state = PCIE_STATE_HOLD_HOST;
				}
				/* Required to be tested for CB calls */
				platform->off_req = false;
			}
			break;

		case PCIE_STATE_HOLD_HOST:
			/* PCIE is hold by the host */
			if (ev_data->event == PCIE_EVT_RST_OFF) {
				pcie_mif_poweroff(platform->pcie);
				state = PCIE_STATE_OFF;
			} else if (ev_data->event == PCIE_EVT_RELEASE && platform->host_users == 0) {
				/* If host releases the pcie usage, send the message
			 	* to PMU */
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Host PCIe release. Send PMU_AP_MSG_WLBT_PCIE_OFF_REJECT_CANCEL\n");
				pcie_set_mbox_pmu_pcie_off(platform->pcie, PMU_AP_MSG_WLBT_PCIE_OFF_REJECT_CANCEL);
				state = PCIE_STATE_ON;
			}
			break;

		case PCIE_STATE_SLEEP:
			if (ev_data->event == PCIE_EVT_RST_OFF) {
				state = PCIE_STATE_OFF;
			} else if (ev_data->event == PCIE_EVT_CLAIM ||
				   ev_data->event == PCIE_EVT_CLAIM_CB ||
				   ev_data->event == PCIE_EVT_PMU_ON_REQ) {
				ret = pcie_mif_poweron(platform->pcie);
				if (ret)
					state = PCIE_STATE_ERROR;
				else
					state = PCIE_STATE_ON;
			}
			break;

		case PCIE_STATE_ERROR:
		default:
			break;
		}

		if (old_state != state) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PCIE state (%s) -> (%s) on event %s. Users %d complete %d\n",
					  states[old_state], states[state], events[ev_data->event], current_users, ev_data->complete);
		}

		if (ev_data->event == PCIE_EVT_PMU_ON_REQ)
			wake_unlock(&platform->wakehash_wakelock);

		/* If State is in ERROR, skip the CB/completion so services
		 * can fail properly */
		if (state == PCIE_STATE_ERROR) {
			SCSC_TAG_ERR(PLAT_MIF, "PCIE_STATE_ERROR in turning PCIE On : ret = %d\n", ret);
			continue;
		}

		/* Check if a Callback is required */
		if (ev_data->event == PCIE_EVT_CLAIM_CB) {
			/* TODO: we can't spinlock here as the callback might
			 * sleep */
			if (ev_data->pcie_wakeup_cb)
				ev_data->pcie_wakeup_cb(ev_data->pcie_wakeup_cb_data_service,
						ev_data->pcie_wakeup_cb_data_dev);
			is_deferred = false;
		}

		if (ev_data->complete)
			complete(&platform->pcie_on);
	}

	kfree(ev_data);

	return 0;
}

static int __platform_mif_send_event_to_fsm(struct platform_mif *platform, int event, bool comp)
{
	u32 val;
	struct event_record ev;

	unsigned long flags;
	if (event == PCIE_EVT_CLAIM_CB)
		is_deferred = true;

	spin_lock_irqsave(&platform->kfifo_lock, flags);
	ev.event = event;
	ev.complete = comp;
	/**
	 * TODO: we can move pcie_wakeup_cb, pcie_wakeup_cb_data_service, and pcie_wakeup_cb_data_dev
	 * from platform MIF
	 */
	ev.pcie_wakeup_cb = (event == PCIE_EVT_CLAIM_CB ? platform->pcie_wakeup_cb : NULL);
	ev.pcie_wakeup_cb_data_service = (event == PCIE_EVT_CLAIM_CB ? platform->pcie_wakeup_cb_data_service : NULL);
	ev.pcie_wakeup_cb_data_dev = (event == PCIE_EVT_CLAIM_CB ? platform->pcie_wakeup_cb_data_dev : NULL);
	val = kfifo_in(&platform->ev_fifo, &ev, sizeof(ev));
	wake_up_interruptible(&platform->event_wait_queue);
	spin_unlock_irqrestore(&platform->kfifo_lock, flags);

	return 0;
}

static int platform_mif_send_event_to_fsm(struct platform_mif *platform, enum pcie_event_type event)
{
	return __platform_mif_send_event_to_fsm(platform, event, false);
}

static int platform_mif_send_event_to_fsm_wait_completion(struct platform_mif *platform, enum pcie_event_type event)
{
	if (!platform->t)
		return -EIO;

	return __platform_mif_send_event_to_fsm(platform, event, true);
}

#if !defined(CONFIG_SOC_S5E9945)
irqreturn_t platform_mif_set_wlbt_clk_handler(int irq, void *data)
{
	/*This isr can't be called because of clearing pending intr by ohters(ex ACPM, etc) */

	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq : %d\n", irq);

	return IRQ_HANDLED;
}
#endif

irqreturn_t platform_wlbt_wakeup_handler(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	bool wake_val = gpio_get_value(platform->gpio_num);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "wake_val %d\n", wake_val);
	if (wake_val == PCIE_LINK_ON) {
		wake_lock(&platform->wakehash_wakelock);
		platform_mif_send_event_to_fsm(platform, PCIE_EVT_PMU_ON_REQ);
	}

	return IRQ_HANDLED;
}

static int platform_mif_hostif_wakeup(struct scsc_mif_abs *interface, int (*cb)(void *first, void *second), void *service, void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	/* Link is on, and OFF_REQ hasn't been signalled
	 * it is safe to return 0*/
	spin_lock_irqsave(&platform->cb_sync, flags);
	if (platform->off_req == false && pcie_is_on(platform->pcie)) {
		platform_mif_send_event_to_fsm(platform, PCIE_EVT_CLAIM);
		spin_unlock_irqrestore(&platform->cb_sync, flags);
		return 0;
	}
	spin_unlock_irqrestore(&platform->cb_sync, flags);

	/* Callback and parameter to call upon PCIE link-ready */
	platform->pcie_wakeup_cb = cb;
	platform->pcie_wakeup_cb_data_service = service;
	platform->pcie_wakeup_cb_data_dev = dev;

	platform_mif_send_event_to_fsm(platform, PCIE_EVT_CLAIM_CB);

	return -EAGAIN;
}

#if !defined(CONFIG_SOC_S5E9945)
static int platform_mif_set_wlbt_clk(struct platform_mif *platform)
{
	int ret;
	u32 eint_num;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");
#if defined(SCSC_SEP_VERSION)
	eint_num = 15;
#elif defined(CONFIG_SOC_S5E9925)
	eint_num = 32;
#elif defined(CONFIG_SOC_S5E9935)
	eint_num = 33;
#endif
	acpm_init_eint_clk_req(eint_num);

	platform->clk_irq = gpio_to_irq(of_get_named_gpio(platform->dev->of_node,
				"wlbt,clk_gpio", 0));
	if (platform->clk_irq <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get clk_gpio\n");
		return -EINVAL;
	}
	ret = devm_request_threaded_irq(platform->dev, platform->clk_irq, NULL, platform_mif_set_wlbt_clk_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DRV_NAME, platform);

	if (IS_ERR_VALUE((unsigned long)ret)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to register Clk handler: %d. Aborting.\n", ret);
		return -EINVAL;
	}

	return ret;
}
#endif

static void platform_mif_unregister_wakeup_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

	disable_irq_wake(platform->wakeup_irq);
	devm_free_irq(platform->dev, platform->wakeup_irq, platform);
}

static int platform_mif_register_wakeup_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	err = devm_request_irq(platform->dev, platform->wakeup_irq, platform_wlbt_wakeup_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DRV_NAME, platform);

	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to register Wakeup handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	enable_irq_wake(platform->wakeup_irq);

	return 0;
}

#ifdef CONFIG_SCSC_QOS
static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 msi, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_mif_set_affinity_cpu(platform->pcie, msi, cpu);
}

int qos_get_param(char *buffer, const struct kernel_param *kp)
{
	/* To get cpu_policy of cl0 and cl1*/
	struct cpufreq_policy *cpucl0_policy = cpufreq_cpu_get(0);
	struct cpufreq_policy *cpucl1_policy = cpufreq_cpu_get(4);
	struct cpufreq_policy *cpucl2_policy = cpufreq_cpu_get(7);
#if defined(CONFIG_SOC_S5E9945)
	struct cpufreq_policy *cpucl3_policy = cpufreq_cpu_get(9);
#endif
	struct dvfs_rate_volt *int_domain_fv;
	struct dvfs_rate_volt *mif_domain_fv;
	u32 int_lv, mif_lv;
	int count = 0;

	count += sprintf(buffer + count, "CPUCL0 QoS: %d, %d, %d\n",
			cpucl0_policy->freq_table[qos_cpucl0_lv[0]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[1]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[2]].frequency);
	count += sprintf(buffer + count, "CPUCL1 QoS: %d, %d, %d\n",
			cpucl1_policy->freq_table[qos_cpucl1_lv[0]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[1]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[2]].frequency);

	count += sprintf(buffer + count, "CPUCL2 QoS: %d, %d, %d\n",
			cpucl2_policy->freq_table[qos_cpucl2_lv[0]].frequency,
			cpucl2_policy->freq_table[qos_cpucl2_lv[1]].frequency,
			cpucl2_policy->freq_table[qos_cpucl2_lv[2]].frequency);

#if defined(CONFIG_SOC_S5E9945)
	count += sprintf(buffer + count, "CPUCL3 QoS: %u, %u, %u\n",
			cpucl3_policy->freq_table[qos_cpucl3_lv[0]].frequency,
			cpucl3_policy->freq_table[qos_cpucl3_lv[1]].frequency,
			cpucl3_policy->freq_table[qos_cpucl3_lv[2]].frequency);
#endif

	int_lv = cal_dfs_get_lv_num(ACPM_DVFS_INT);
	int_domain_fv =
		kcalloc(int_lv, sizeof(struct dvfs_rate_volt), GFP_KERNEL);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_INT, int_domain_fv);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	count += sprintf(buffer + count, "INT QoS: %lx, %lx, %lx\n",
#else
	count += sprintf(buffer + count, "INT QoS: %d, %d, %d\n",
#endif
			int_domain_fv[qos_int_lv[0]].rate, int_domain_fv[qos_int_lv[1]].rate,
			int_domain_fv[qos_int_lv[2]].rate);

	mif_lv = cal_dfs_get_lv_num(ACPM_DVFS_MIF);
	mif_domain_fv =
		kcalloc(mif_lv, sizeof(struct dvfs_rate_volt), GFP_KERNEL);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_MIF, mif_domain_fv);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	count += sprintf(buffer + count, "MIF QoS: %lx, %lx, %lx\n",
#else
	count += sprintf(buffer + count, "MIF QoS: %d, %d, %d\n",
#endif
			mif_domain_fv[qos_mif_lv[0]].rate, mif_domain_fv[qos_mif_lv[1]].rate,
			mif_domain_fv[qos_mif_lv[2]].rate);

	cpufreq_cpu_put(cpucl0_policy);
	cpufreq_cpu_put(cpucl1_policy);
	cpufreq_cpu_put(cpucl2_policy);
#if defined(CONFIG_SOC_S5E9945)
	cpufreq_cpu_put(cpucl3_policy);
#endif
	kfree(int_domain_fv);
	kfree(mif_domain_fv);

	return count;
}

const struct kernel_param_ops domain_id_ops = {
	.set = NULL,
	.get = &qos_get_param,
};
module_param_cb(qos_info, &domain_id_ops, NULL, 0444);

static void platform_mif_verify_qos_table(struct platform_mif *platform)
{
	u32 index;
#if !defined(CONFIG_SOC_S5E9945)
	u32 cl0_max_idx, cl1_max_idx, cl2_max_idx;
#else
	u32 cl0_max_idx, cl1_max_idx, cl2_max_idx, cl3_max_idx;
#endif
	u32 int_max_idx, mif_max_idx;

	cl0_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl0_policy,
							platform->qos.cpucl0_policy->max);
	cl1_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl1_policy,
							platform->qos.cpucl1_policy->max);
	cl2_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl2_policy,
							platform->qos.cpucl2_policy->max);
#if defined(CONFIG_SOC_S5E9945)
	cl3_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl3_policy,
							platform->qos.cpucl3_policy->max);
#endif

	int_max_idx = (cal_dfs_get_lv_num(ACPM_DVFS_INT) - 1);
	mif_max_idx = (cal_dfs_get_lv_num(ACPM_DVFS_MIF) - 1);

	for (index = SCSC_QOS_DISABLED; index < SCSC_QOS_MAX; index++) {
		qos_cpucl0_lv[index] = min(qos_cpucl0_lv[index], cl0_max_idx);
		qos_cpucl1_lv[index] = min(qos_cpucl1_lv[index], cl1_max_idx);
		qos_cpucl2_lv[index] = min(qos_cpucl2_lv[index], cl2_max_idx);
#if defined(CONFIG_SOC_S5E9945)
		qos_cpucl3_lv[index] = min(qos_cpucl3_lv[index], cl3_max_idx);
#endif
		qos_int_lv[index] = min(qos_int_lv[index], int_max_idx);
		qos_mif_lv[index] = min(qos_mif_lv[index], mif_max_idx);
	}
}

static int platform_mif_qos_init(struct platform_mif *platform)
{
	u32 int_lv, mif_lv;

	int_lv = cal_dfs_get_lv_num(ACPM_DVFS_INT);
	mif_lv = cal_dfs_get_lv_num(ACPM_DVFS_MIF);

	platform->qos.cpucl0_policy = cpufreq_cpu_get(0);
	platform->qos.cpucl1_policy = cpufreq_cpu_get(4);
	platform->qos.cpucl2_policy = cpufreq_cpu_get(7);
#if defined(CONFIG_SOC_S5E9945)
	platform->qos.cpucl3_policy = cpufreq_cpu_get(9);
#endif

	platform->qos_enabled = false;

#if !defined(CONFIG_SOC_S5E9945)
	if ((!platform->qos.cpucl0_policy) || (!platform->qos.cpucl1_policy) ||
	    (!platform->qos.cpucl2_policy)) {
#else
	if ((!platform->qos.cpucl0_policy) || (!platform->qos.cpucl1_policy) ||
	    (!platform->qos.cpucl2_policy) || (!platform->qos.cpucl3_policy)) {
#endif
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. CPU policy is not loaded\n");
		return -ENOENT;
	}

	platform->qos.int_domain_fv =
		devm_kzalloc(platform->dev, sizeof(struct dvfs_rate_volt) * int_lv, GFP_KERNEL);
	platform->qos.mif_domain_fv =
		devm_kzalloc(platform->dev, sizeof(struct dvfs_rate_volt) * mif_lv, GFP_KERNEL);

	cal_dfs_get_rate_asv_table(ACPM_DVFS_INT, platform->qos.int_domain_fv);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_MIF, platform->qos.mif_domain_fv);

	if ((!platform->qos.int_domain_fv) || (!platform->qos.mif_domain_fv)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. DEV(MIF,INT) policy is not loaded\n");
		return -ENOENT;
	}

	/* verify pre-configured freq-levels of cpucl0/1/2/3 */
	platform_mif_verify_qos_table(platform);

	platform->qos_enabled = true;

	return 0;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(4);
	qos_req->cpu_cluster2_policy = cpufreq_cpu_get(7);
#if defined(CONFIG_SOC_S5E9945)
	qos_req->cpu_cluster3_policy = cpufreq_cpu_get(9);
#endif

#if !defined(CONFIG_SOC_S5E9945)
	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy) ||
	    (!qos_req->cpu_cluster2_policy)) {
#else
	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy) ||
	    (!qos_req->cpu_cluster2_policy) || (!qos_req->cpu_cluster3_policy)) {
#endif
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}

	if (config == SCSC_QOS_DISABLED) {
		/* Update MIF/INT QoS */
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);

		/* Update Clusters QoS */
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
						&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
						&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN, 0);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster2_policy->constraints,
						&qos_req->pm_qos_req_cl2, FREQ_QOS_MIN, 0);
#if defined(CONFIG_SOC_S5E9945)
		freq_qos_tracer_add_request(&qos_req->cpu_cluster3_policy->constraints,
						&qos_req->pm_qos_req_cl3, FREQ_QOS_MIN, 0);
#endif

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request: SCSC_QOS_DISABLED\n");
	} else {
		u32 qos_lv = config -1;
		/* Update MIF/INT QoS*/
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT,
				platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate);
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT,
				platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);

		/* Update Clusters QoS*/
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
										&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN,
				platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
										&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN,
				platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster2_policy->constraints,
										&qos_req->pm_qos_req_cl2, FREQ_QOS_MIN,
				platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency);
#if defined(CONFIG_SOC_S5E9945)
		freq_qos_tracer_add_request(&qos_req->cpu_cluster3_policy->constraints,
										&qos_req->pm_qos_req_cl3, FREQ_QOS_MIN,
				platform->qos.cpucl3_policy->freq_table[qos_cpucl3_lv[qos_lv]].frequency);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"PM QoS add request: %u. CL0 %u CL1 %u CL2 %u CL3 %u MIF %u INT %u\n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.cpucl3_policy->freq_table[qos_cpucl3_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
#else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"PM QoS add request: %u. CL0 %u CL1 %u CL2 %u MIF %u INT %u\n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
#endif
	}

	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	if (config == SCSC_QOS_DISABLED) {
		/* Update MIF/INT QoS */
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif, 0);
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_int, 0);

		/* Update Clusters QoS */
		freq_qos_update_request(&qos_req->pm_qos_req_cl0, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl2, 0);
#if defined(CONFIG_SOC_S5E9945)
		freq_qos_update_request(&qos_req->pm_qos_req_cl3, 0);
#endif
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS update request: SCSC_QOS_DISABLED\n");
	} else {
		u32 qos_lv = config -1;
		/* Update MIF/INT QoS */
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif,
				platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate);
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_int,
				platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);

		/* Update Clusters QoS */
		freq_qos_update_request(&qos_req->pm_qos_req_cl0,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl2,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency);
#if defined(CONFIG_SOC_S5E9945)
		freq_qos_update_request(&qos_req->pm_qos_req_cl3,
			platform->qos.cpucl3_policy->freq_table[qos_cpucl3_lv[qos_lv]].frequency);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"PM QoS update request: %u. CL0 %u CL1 %u CL2 %u CL3 %u MIF %u INT %u \n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.cpucl3_policy->freq_table[qos_cpucl3_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
#else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"PM QoS update request: %u. CL0 %u CL1 %u CL2 %u MIF %u INT %u \n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
#endif
	}

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"PM QoS remove request\n");

	/* Remove MIF/INT QoS */
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_int);

	/* Remove Clusters QoS */
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl2);
#if defined(CONFIG_SOC_S5E9945)
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl3);
#endif
	return 0;
}
#endif /*CONFIG_SCSC_QOS*/

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void platform_recovery_disabled_reg(struct scsc_mif_abs *interface, bool (*handler)(void))
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif recovery %pS\n", handler);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = handler;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif recovery\n");
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
#endif

static void platform_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT handler not registered\n");
}

static void platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT reset_request handler not registered\n");
}

static void platform_mif_wlan_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->wlan_handler != platform_mif_irq_default_handler)
		platform->wlan_handler(irq, platform->irq_dev);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF wlan Interrupt Handler not registered\n");
}

static void platform_mif_wpan_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->wpan_handler != platform_mif_irq_default_handler)
		platform->wpan_handler(irq, platform->irq_dev_wpan);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF wpan Interrupt Handler not registered\n");
}

static void platform_mif_pmu_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->pmu_handler != platform_mif_irq_default_handler)
		platform->pmu_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU Interrupt Handler not registered\n");
}

static void platform_mif_pmu_pcie_off_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;
	u32 cmd = pcie_get_mbox_pmu_pcie_off(platform->pcie);
	unsigned long flags;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "PCIE OFF IRQ received\n");

	switch (cmd) {
	case PMU_AP_MSG_PCIE_OFF_REQ:
		SCSC_TAG_INFO(PLAT_MIF, "Received PMU IRQ cmd [PCIE OFF REQ]\n");
		spin_lock_irqsave(&platform->cb_sync, flags);
		platform->off_req = true;
		platform_mif_send_event_to_fsm(platform, PCIE_EVT_PMU_OFF_REQ);
		spin_unlock_irqrestore(&platform->cb_sync, flags);
		return; /* don't proceed to pmu_handler as we handle PCIE OFF REQ only here */
	default:
		break;
	}

	if (platform->pmu_pcie_off_handler != platform_mif_irq_default_handler)
		platform->pmu_pcie_off_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU pcie_off Interrupt Handler not registered\n");

}

static void platform_mif_pmu_error_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "PMU ERROR IRQ received\n");

	if (platform->pmu_error_handler != platform_mif_irq_default_handler)
		platform->pmu_error_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU pcie_err Interrupt Handler not registered\n");

}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void platform_mif_set_memlog_baaw(struct platform_mif *platform, dma_addr_t paddr)
{
#define MEMLOG_CHECK(x)                                                                                                \
	do {                                                                                                           \
		int retval = (x);                                                                                      \
		if (retval < 0) {                                                                                      \
			pr_err("%s failed at L%d", __func__, __LINE__);                                                \
			goto baaw1_error;                                                                              \
		}                                                                                                      \
	} while (0)

baaw1_error:
	return;
}
#endif

static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_set_mem_region2(struct scsc_mif_abs *interface, void __iomem *_mem_region2,
					size_t _mem_size_region2)
{
	/* If memlogger API is enabled, mxlogger's mem should be set
	 * by another routine
	 */
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->mem_region2 = _mem_region2;
	platform->mem_size_region2 = _mem_size_region2;
	return 0;
}

static void platform_mif_set_memlog_paddr(struct scsc_mif_abs *interface, dma_addr_t paddr)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->paddr = paddr;
}

#endif

int scsc_pcie_complete(void)
{
	if(!wait_for_completion_timeout(&g_platform->pcie_on, 5*HZ)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, g_platform->dev, "wait for complete timeout\n");
		return -EFAULT;
	}
	reinit_completion(&g_platform->pcie_on);
	return 0;
}
EXPORT_SYMBOL(scsc_pcie_complete);

static void platform_mif_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"reset_gpio and pmic_gpio set low for retry\n");
#ifdef CONFIG_SCSC_BB_REDWOOD
	gpio_set_value(platform->reset_gpio, 0);
	udelay(off_pinctrl_delay);
#endif
	gpio_set_value(platform->pmic_gpio, 0);
	msleep(200);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"delay 200ms after low gpios\n");
}

static bool platform_mif_get_scan2mem_mode(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	return pcie_mif_get_scan2mem_mode(platform->pcie);
}

static void platform_mif_set_scan2mem_mode(struct scsc_mif_abs *interface, bool enable)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_mif_set_scan2mem_mode(platform->pcie, enable);
}

static void platform_mif_set_s2m_dram_offset(struct scsc_mif_abs *interface, u32 offset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_set_s2m_dram_offset(platform->pcie, offset);
}

static u32 platform_mif_get_s2m_size_octets(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_s2m_size_octets(platform->pcie);
}

static unsigned long platform_mif_get_mem_start(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	return platform->mem_start;
}

/* reset=0 - release from reset */
/* reset=1 - hold reset */
static int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long timeout;
	bool init;
	int i;
	u32 ret = 0;

	if (enable_platform_mif_arm_reset) {
		if (!reset) { /* Release from reset */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, " - reset : false\n");

			if (!platform_mif_get_scan2mem_mode(interface)) {
				if (keep_powered) {
					/* If WLBT was kept powered after reset, to allow the PMU to
					* sleep the flash module, power cycle it now to reset the HW.
					*/
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "power cycle WLBT\n");

					gpio_set_value(platform->pmic_gpio, 0);
					msleep(200);
				}
			}

			for (i = 0; i < max_linkup_count; i++) {
				gpio_set_value(platform->pmic_gpio, 1);
#ifdef CONFIG_SCSC_BB_REDWOOD
				udelay(on_pinctrl_delay);
				gpio_set_value(platform->reset_gpio, 1);
#endif
				/* Start always with fw_user to one */
				platform_mif_pcie_control_fsm_prepare_fw(platform);
				platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_ON);
				ret = scsc_pcie_complete();
				if (ret) {
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"trying to link up again\n");
					platform_mif_reset_assert(interface);
				}
				else
					break;
			}
			if (ret)
				return ret;

			pcie_register_driver();

			/* We many need to wait until pcie is ready */
			timeout = jiffies + msecs_to_jiffies(500);
			do {
				init = pcie_is_initialized();
				if (init) {
					SCSC_TAG_INFO(PLAT_MIF, "PCIe has been initialized\n");
					break;
				}
			} while (time_before(jiffies, timeout));

			if (!init) {
				SCSC_TAG_INFO(PLAT_MIF, "PCIe has not been initialized\n");
				if (pcie_is_on(platform->pcie)) {
					SCSC_TAG_INFO(PLAT_MIF, "Trying to link down\n");
					platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_OFF);
					scsc_pcie_complete();
				}
				pcie_unregister_driver();
				return -EIO;
			}

			pcie_prepare_boot(platform->pcie);

			/* Enable L1.2 substate, l1ss is abbreviation for L1 PM Substates */
			pcie_mif_l1ss_ctrl(platform->pcie, 1);
		} else {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, " - reset : true\n");
			platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_OFF);
			scsc_pcie_complete();

			pcie_unregister_driver();

			if (platform_mif_get_scan2mem_mode(interface)) {
				SCSC_TAG_INFO(PLAT_MIF, "Wait for %dmsec to guarantee time for scandump\n", scan2mem_timeout_value);
				msleep(scan2mem_timeout_value);
			}

#ifdef CONFIG_SCSC_BB_REDWOOD
			gpio_set_value(platform->reset_gpio, 0);
#endif
			if (!platform_mif_get_scan2mem_mode(interface)) {
				if (!keep_powered) {
#ifdef CONFIG_SCSC_BB_REDWOOD
					udelay(off_pinctrl_delay);
#endif
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
							"pmic_gpio is set as low\n");
					gpio_set_value(platform->pmic_gpio, 0);
				} else {
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
							"pmic_gpio power preserved\n");
				}
			}
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
				enable_platform_mif_arm_reset);
	return ret;
}

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	state = PCIE_STATE_OFF;

	/* register interrupts */
	if (platform_mif_register_wakeup_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		pcie_unmap(platform->pcie);
		return NULL;
	}

	return pcie_map(platform->pcie, allocated);
}

/* HERE: Not sure why mem is passed in - its stored in platform -
 * as it should be
 */
static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	platform_mif_unregister_wakeup_irq(platform);

	pcie_unmap(platform->pcie);

}

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMR0 0x%x target %s\n", val, (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");
	return val;
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_irq_get(platform->pcie, target);
	return val;
}

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_set(platform->pcie, bit_num, target);
}

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_clear(platform->pcie, bit_num, target);
}

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_mask(platform->pcie, bit_num, target);
}

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_unmask(platform->pcie, bit_num, target);
}

static void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr,
				   enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	pcie_remap_set(platform->pcie, remap_addr, target);
}

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					 void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = handler;
	platform->irq_dev = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_get_msi_range(struct scsc_mif_abs *interface, u8 *start, u8 *end,
				       enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_get_msi_range(platform->pcie, start, end, target);
}

static void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					      void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler for WPAN %pS in %p %p\n", handler,
			  platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = handler;
	platform->irq_dev_wpan = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler for WPAN %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev_wpan = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface,
						       void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif reset_request int handler %pS in %p %p\n", handler,
			  platform, interface);
	platform->reset_request_handler = handler;
	platform->irq_reset_request_dev = dev;
	if (atomic_read(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Default WDOG handler disabled by spurios IRQ...re-enabling.\n");
		enable_irq(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	}
}

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
}

static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
					     int (*suspend)(struct scsc_mif_abs *abs, void *data),
					     void (*resume)(struct scsc_mif_abs *abs, void *data), void *data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif suspend/resume handlers in %p %p\n", platform,
			  interface);
	platform->suspend_handler = suspend;
	platform->resume_handler = resume;
	platform->suspendresume_data = data;
}

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif suspend/resume handlers in %p %p\n", platform,
			  interface);
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;
}

static __iomem void *platform_get_ramrp_ptr(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_ramrp_ptr(platform->pcie);
}

static int platform_get_ramrp_buff(struct scsc_mif_abs *interface, void** buff, int count, u64 offset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_ramrp_buff(platform->pcie, buff, count, offset);
}

static int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

static int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write AP2WB_MAILBOX: %u\n", val);

	pcie_set_mbox_pmu(platform->pcie, val);

	return 0;
}

static int platform_mif_get_mbox_pmu_pcie_off(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu_pcie_off(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU_PCIE_OFF_MAILBOX: %u\n", val);
	return val;
}

static int platform_mif_set_mbox_pmu_pcie_off(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write PMU_PCIE_OFF_MAILBOX: %u\n", val);

	pcie_set_mbox_pmu_pcie_off(platform->pcie, val);

	return 0;
}

static int platform_mif_get_mbox_pmu_error(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu_error(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU_ERROR_MAILBOX: %u\n", val);
	return val;
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done\n", ka_patch_len);

	pcie_load_pmu_fw(platform->pcie, ka_patch, ka_patch_len);

	return 0;
}

static void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					     void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_pmu_error_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					     void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu pcie_error int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_error_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pci_get_mifram_ref(platform->pcie, ptr, ref);

	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_get_mifram_ref_region2(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem_region2 + platform->mem_size_region2))
		return -ENOMEM;

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem_region2 + platform->mem_size);
	return 0;
}
#endif

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_mifram_ptr(platform->pcie, ref);
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void *platform_mif_get_mifram_ptr_region2(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}
	ref -= platform->mem_size;

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size_region2)
		return (void *)((uintptr_t)platform->mem_region2 + (uintptr_t)ref);
	else
		return NULL;
}
#endif

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_mifram_phy_ptr(platform->pcie, ref);
}

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pci_get_mif_pfn(platform->pcie);
}

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	return of_property_read_bool(platform->np, propname);
}

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u8_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname, size);

	if (of_property_read_u16_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u32_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int ret;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);


	ret = of_property_read_string_array(platform->np, propname, (const char **)out_value, size);
	if (ret <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist (ret=%d)\n", ret);
		return ret;
	}
	return ret;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = { PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG };
	char *irqs_name[] = { "MBOX", "WDOG" };

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE, &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED, &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n", irq, irqs_name[i],
					  pending, active, masked);
	}
	platform_mif_dump_register(&platform->interface);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem)
{
	SCSC_TAG_DEBUG(PLAT_MIF, "memory reserved: mem_base=%#lx, mem_size=%zd\n", (unsigned long)remem->base,
		       (size_t)remem->size);

	sharedmem_base = remem->base;
	sharedmem_size = remem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifibt_if, "exynos,wifibt_if", platform_mif_wifibt_if_reserved_mem_setup);
#endif

static void platform_mif_irqdump(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_s6165_irqdump(platform->pcie);
}

#ifdef CONFIG_SCSC_BB_PAEAN
int platform_mif_acpm_write_reg(struct scsc_mif_abs *interface, u8 reg, u8 value)
{
	int ret;
	const int wlbt_channel = 8;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	mutex_lock(&platform->acpm_lock);
	ret = exynos_acpm_write_reg(platform->np, wlbt_channel, 0, reg, value);
	mutex_unlock(&platform->acpm_lock);

	return ret;
}
#endif

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	struct pcie_mif *pcie;
	int err = 0;

	if (!platform)
		return NULL;

	/* TODO, this should be fixed and come from a mx instance!*/
	g_platform = platform;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev, "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->map = platform_mif_map;
	platform_if->unmap = platform_mif_unmap;
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
	platform_if->remap_set = platform_mif_remap_set;
	platform_if->irq_reg_handler = platform_mif_irq_reg_handler;
	platform_if->irq_unreg_handler = platform_mif_irq_unreg_handler;
	platform_if->get_msi_range = platform_mif_get_msi_range;
	platform_if->irq_reg_handler_wpan = platform_mif_irq_reg_handler_wpan;
	platform_if->irq_unreg_handler_wpan = platform_mif_irq_unreg_handler_wpan;
	platform_if->irq_reg_reset_request_handler = platform_mif_irq_reg_reset_request_handler;
	platform_if->irq_unreg_reset_request_handler = platform_mif_irq_unreg_reset_request_handler;
	platform_if->suspend_reg_handler = platform_mif_suspend_reg_handler;
	platform_if->suspend_unreg_handler = platform_mif_suspend_unreg_handler;
	platform_if->get_mifram_ptr = platform_mif_get_mifram_ptr;
	platform_if->get_mifram_ref = platform_mif_get_mifram_ref;
	platform_if->get_mifram_pfn = platform_mif_get_mif_pfn;
	platform_if->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_dump_registers = platform_mif_dump_register;
	platform_if->mif_read_register = NULL;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
#ifdef CONFIG_SCSC_QOS
	platform_if->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	platform_if->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	platform_if->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	platform_if->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	platform_if->get_mifram_ptr_region2 = platform_mif_get_mifram_ptr_region2;
	platform_if->get_mifram_ref_region2 = platform_mif_get_mifram_ref_region2;
	platform_if->set_mem_region2 = platform_mif_set_mem_region2;
	platform_if->set_memlog_paddr = platform_mif_set_memlog_paddr;
	platform->paddr = 0;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	platform_if->recovery_disabled_reg = platform_recovery_disabled_reg;
	platform_if->recovery_disabled_unreg = platform_recovery_disabled_unreg;
#endif
	platform_if->get_ramrp_ptr = platform_get_ramrp_ptr;
	platform_if->get_ramrp_buff = platform_get_ramrp_buff;
	platform_if->get_mbox_pmu = platform_mif_get_mbox_pmu;
	platform_if->set_mbox_pmu = platform_mif_set_mbox_pmu;
	platform_if->get_mbox_pmu_pcie_off = platform_mif_get_mbox_pmu_pcie_off;
	platform_if->set_mbox_pmu_pcie_off = platform_mif_set_mbox_pmu_pcie_off;
	platform_if->get_mbox_pmu_error = platform_mif_get_mbox_pmu_error;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
	platform_if->irq_reg_pmu_handler = platform_mif_irq_reg_pmu_handler;
	platform_if->irq_reg_pmu_error_handler = platform_mif_irq_reg_pmu_error_handler;
	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->pmu_pcie_off_handler = platform_mif_irq_default_handler;
	platform->irq_dev_pmu = NULL;
	/* Reset ka_patch pointer & size */
	platform->ka_patch_fw = NULL;
	platform->ka_patch_len = 0;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;

	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

	platform->np = pdev->dev.of_node;
	platform_if->wlbt_property_read_bool = platform_mif_wlbt_property_read_bool;
	platform_if->wlbt_property_read_u8 = platform_mif_wlbt_property_read_u8;
	platform_if->wlbt_property_read_u16 = platform_mif_wlbt_property_read_u16;
	platform_if->wlbt_property_read_u32 = platform_mif_wlbt_property_read_u32;
	platform_if->wlbt_property_read_string = platform_mif_wlbt_property_read_string;
	platform_if->hostif_wakeup = platform_mif_hostif_wakeup;

	platform_if->wlbt_irqdump = platform_mif_irqdump;
#ifdef CONFIG_SCSC_BB_PAEAN
	platform_if->acpm_write_reg = platform_mif_acpm_write_reg; //SPMI write
	mutex_init(&platform->acpm_lock);
#elif CONFIG_SCSC_BB_REDWOOD
	platform_if->control_suspend_gpio = platform_mif_control_suspend_gpio;
#endif
	platform_if->get_scan2mem_mode = platform_mif_get_scan2mem_mode;
	platform_if->set_scan2mem_mode = platform_mif_set_scan2mem_mode;
	platform_if->get_s2m_size_octets = platform_mif_get_s2m_size_octets;
	platform_if->set_s2m_dram_offset = platform_mif_set_s2m_dram_offset;
	platform_if->get_mem_start = platform_mif_get_mem_start;
	platform->pm_nb.notifier_call = wlbt_rc_pm_notifier;
	register_pm_notifier(&platform->pm_nb);
#ifdef CONFIG_OF_RESERVED_MEM
	if (!sharedmem_base) {
		struct device_node *np;

		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "module build register sharedmem np %x\n", np);
		if (np && of_reserved_mem_lookup(np)) {
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "built-in register sharedmem\n");
		platform->mem_start = sharedmem_base;
		platform->mem_size = sharedmem_size;
	}
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			  (u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any
		 * sense. This may be an indication of an incorrect platform
		 * device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

	if (of_property_read_u32(platform->dev->of_node, "pci_ch_num", &platform->pcie_ch_num)) {
		err = -EINVAL;
		goto error_exit;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"platform->pcie_ch_num : %d\n", platform->pcie_ch_num);

	platform->pmic_gpio = of_get_gpio(platform->dev->of_node, 0);
	if (platform->pmic_gpio < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get pmic_gpio\n");
	} else {
		if (devm_gpio_request_one(platform->dev, platform->pmic_gpio,
					GPIOF_OUT_INIT_LOW, dev_name(platform->dev))) {
			err = -EINVAL;
			goto error_exit;
		}
	}

#ifdef CONFIG_SCSC_BB_REDWOOD
	platform->reset_gpio = of_get_named_gpio(platform->dev->of_node, "wlbt,reset_gpio", 0);
	platform->suspend_gpio = of_get_named_gpio(platform->dev->of_node, "wlbt,suspend_gpio", 0);
#endif
	platform->gpio_num = of_get_named_gpio(platform->dev->of_node, "wlbt,wakeup_gpio", 0);
	platform->wakeup_irq = gpio_to_irq(of_get_named_gpio(platform->dev->of_node,
				"wlbt,wakeup_gpio", 0));

	if (platform->wakeup_irq <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get wakeup_gpio\n");
		err = -EINVAL;
		goto error_exit;
	}

#if !defined(CONFIG_SOC_S5E9945)
	err = platform_mif_set_wlbt_clk(platform);
	if (err) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot set wlbt clk");
		goto error_exit;
	}
#endif

#ifdef CONFIG_SCSC_QOS
	platform_mif_qos_init(platform);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

	pcie_init();
	/* get the singleton instance of pcie */
	pcie = pcie_get_instance();
	if (!pcie) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "Getting PCIE instance\n");
		return NULL;
	}

	pcie_set_mem_range(pcie, platform->mem_start, platform->mem_size);

	pcie_set_ch_num(pcie, platform->pcie_ch_num);

	platform->pcie = pcie;
	pcie_irq_reg_pmu_handler(pcie, platform_mif_pmu_isr, (void *)platform);
	pcie_irq_reg_pmu_pcie_off_handler(pcie, platform_mif_pmu_pcie_off_isr, (void *)platform);
	pcie_irq_reg_pmu_error_handler(pcie, platform_mif_pmu_error_isr, (void *)platform);
	pcie_irq_reg_wlan_handler(pcie, platform_mif_wlan_isr, (void *)platform);
	pcie_irq_reg_wpan_handler(pcie, platform_mif_wpan_isr, (void *)platform);
	platform_mif_set_scan2mem_mode(platform_if, false);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	wake_lock_init(NULL, &(platform->wakehash_wakelock.ws), "wakehash_wakelock");
#else
	wake_lock_init(&platform->wakehash_wakelock, WAKE_LOCK_SUSPEND, "wakehash_wakelock");
#endif
	spin_lock_init(&platform->kfifo_lock);
	spin_lock_init(&platform->cb_sync);
	init_completion(&platform->pcie_on);
	init_waitqueue_head(&platform->event_wait_queue);

	err = kfifo_alloc(&platform->ev_fifo, 256 * sizeof(struct event_record), GFP_KERNEL);
	if (err)
		goto error_exit;

	platform->t = kthread_run(platform_mif_pcie_control_fsm, platform, "pcie_control_fsm");
	if (IS_ERR(platform->t)) {
		err = PTR_ERR(platform->t);
		kfifo_free(&platform->ev_fifo);
		SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "%s: Failed to start pcie_control_fsm (%d)\n",
			__func__, err);
		goto error_exit;
	}

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	wake_lock_destroy(&platform->wakehash_wakelock);
	unregister_pm_notifier(&platform->pm_nb);
	kthread_stop(platform->t);
	platform->t = NULL;

	kfifo_free(&platform->ev_fifo);
	devm_kfree(&pdev->dev, platform);

	pcie_deinit();
}

struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}

int wlbt_rc_pm_notifier(struct notifier_block *nb, unsigned long mode, void *_unused)
{
	struct platform_mif *platform = container_of(nb, struct platform_mif, pm_nb);

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "kthread_park\n");
		kthread_park(platform->t);
		break;
	case PM_POST_SUSPEND:
		kthread_unpark(platform->t);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "kthread_unpark\n");
		if (platform->resume_handler)
			platform->resume_handler(&platform->interface, platform->suspendresume_data);
		break;
	default:
		break;
	}

	return 0;
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "suspending platform driver\n");

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "resuming platform driver\n");
}

int scsc_pcie_claim(void)
{
	int ret = 0;

	/* TODO, this should be fixed and come from a mx instance!*/
	ret = platform_mif_send_event_to_fsm_wait_completion(g_platform, PCIE_EVT_CLAIM);
	return ret;
}
EXPORT_SYMBOL(scsc_pcie_claim);

void scsc_pcie_release(void)
{
	platform_mif_send_event_to_fsm(g_platform, PCIE_EVT_RELEASE);
}
EXPORT_SYMBOL(scsc_pcie_release);

bool scsc_pcie_in_deferred(void)
{
       return is_deferred;
}
EXPORT_SYMBOL(scsc_pcie_in_deferred);
