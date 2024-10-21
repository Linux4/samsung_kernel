// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/time64.h>
#include <linux/uaccess.h>

#include "mtk_peak_power_budget.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#include "apusys_core.h"
#include "apu_power_budget.h"

#define APU_PBM_DRV_UT	(0)
#define LOCAL_DBG	(0)

#define APU_MBOX_BASE	(0x190E2000)
#define APU_PBM_MONITOR	(apu_mbox + 0x400)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apu_pbm_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			pr_notice("%s: %s\n", reason, module); \
			aee_kernel_exception(mod_name, \
					"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			pr_notice("%s: snprintf fail(%d)\n", \
					__func__, __LINE__); \
		} \
	} while (0)
#else
#define apu_pbm_aee_warn(module, reason)
#endif

struct apu_pbm_param {
	int mode;
	int budget;
	int counter;
	int enabled;
	uint32_t delay_off_flag;
	int delay_off_ms;
	struct timer_list off_timer;
};

#define _PBM_MODE(_mode, _budget, _delay_off_ms, _enabled) {	\
	.mode = _mode,						\
	.budget = _budget,					\
	.delay_off_ms = _delay_off_ms,				\
	.enabled = _enabled,					\
}

static struct apu_pbm_param apu_pbm_param_arr[] = {
	_PBM_MODE(PBM_MODE_NORMAL,	     PBM_NORMAL_PWR, NORM_OFF_DELAY, 1),
	_PBM_MODE(PBM_MODE_PERFORMANCE, PBM_PERFORMANCE_PWR, PERF_OFF_DELAY, 1),
};

static int soc_apu_budget;

static spinlock_t apu_pbm_lock;
static int max_budget;
static int prev_max_budget;
static int apu_pbm_func_sel;
static void *apu_mbox;

static int set_apu_pbm_func_param(const char *buf,
		const struct kernel_param *kp)
{
#if APU_PBM_DRV_UT
	struct apu_pbm_param pbm_test;
	int ret = 0, arg_cnt = 0;

	memset(&pbm_test, 0, sizeof(struct apu_pbm_param));

	arg_cnt = sscanf(buf, "%d %d",
			&pbm_test.mode,
			&pbm_test.counter);

	if (arg_cnt > 2) {
		pr_notice("%s invalid input: %s, arg_cnt(%d)\n",
				__func__, buf, arg_cnt);
		return -EINVAL;
	}

	pr_notice("%s (mode, counter): (%d,%d)\n",
			__func__, pbm_test.mode, pbm_test.counter);

	apu_power_budget((enum pbm_mode)pbm_test.mode, pbm_test.counter);

	return ret;
#else
	pr_notice("%s bypass this function!\n", __func__);
	return 0;
#endif
}

static int get_apu_pbm_func_param(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, 64, "soc_apu_budget:%d\n", soc_apu_budget);
}

static const struct kernel_param_ops apu_pbm_func_ops = {
	.set = set_apu_pbm_func_param,
	.get = get_apu_pbm_func_param,
};

__MODULE_PARM_TYPE(apu_pbm_func_sel, "int");
module_param_cb(apu_pbm_func_sel, &apu_pbm_func_ops, &apu_pbm_func_sel, 0644);
MODULE_PARM_DESC(apu_pbm_func_sel, "trigger apu pbm func by parameter");

static int soc_pbm_request(int budget)
{
	uint64_t t, nanosec_rem;
	uint32_t curr_us = 0;

	soc_apu_budget = budget;
	kicker_ppb_request_power(KR_APU, budget);

	t = sched_clock();
	nanosec_rem = do_div(t, 1000000000);
	curr_us = (uint32_t)(nanosec_rem / 1000);

	iowrite32(curr_us, APU_PBM_MONITOR);

	pr_debug("%s apu_pbm_monitor:%d(mW) curr_us:%u\n",
			__func__, budget, ioread32(APU_PBM_MONITOR));

	return 0;
}

/*
 * Decide apu max budget in this moment from different budgets of candidate mode
 * e.g. max_b = MAX(modeA_b, modeB_b, modeC_b, ...)
 */
static void apu_power_budget_judgement(void)
{
	int mode = 0;
	int _max_budget = 0;

	/*
	 * budget candidate conditions:
	 * 1. mode counter is not zero
	 * 2. mode is in the delay off duration
	 */
	for (mode = 0 ; mode < PBM_MODE_MAX ; mode++) {
		if (apu_pbm_param_arr[mode].counter > 0 ||
			apu_pbm_param_arr[mode].delay_off_flag == 1) {
			if (apu_pbm_param_arr[mode].budget > _max_budget)
				_max_budget = apu_pbm_param_arr[mode].budget;
		}
	}

	// max budget change, and we treat it as apu budget in this moment
	if (max_budget != _max_budget) {
		max_budget = _max_budget;
#if LOCAL_DBG
		pr_notice("%s max_budget %d -> %d (mW)\n",
				__func__, prev_max_budget, max_budget);
#endif
		soc_pbm_request(max_budget);
		prev_max_budget = max_budget;
	}
}

int apu_power_budget(enum pbm_mode mode, int counter)
{
	unsigned long flags;
	int ret = 0;

	if (mode >= PBM_MODE_MAX) {
		pr_notice("%s#%d mode is over range\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (apu_pbm_param_arr[mode].enabled == 0) {
#if LOCAL_DBG
		pr_debug("%s#%d mode_%d is disabled, bypass budget control\n",
				__func__, __LINE__, mode);
#endif
		return 0;
	}

	spin_lock_irqsave(&apu_pbm_lock, flags);

	if (counter > 0) {
		// mode 0 -> 1
		if (++apu_pbm_param_arr[mode].counter == 1) {

			// cancal off_timer which was triggered by mode 1 -> 0
			if (timer_pending(&apu_pbm_param_arr[mode].off_timer)) {
#if LOCAL_DBG
				pr_debug("%s mode:%d cancel off_timer\n",
						__func__, mode);
#endif
				del_timer(&apu_pbm_param_arr[mode].off_timer);
			}

			apu_power_budget_judgement();
		} else {
			apu_pbm_aee_warn(
				"APUSYS_POWER", "APU_PBM_COUNTER_UNBALANCE");
		}

	} else {
		// mode 1 -> 0
		if (--apu_pbm_param_arr[mode].counter == 0) {
#if LOCAL_DBG
			// delay N ms to recalculate budget
			pr_debug("%s mode:%d delay_off_flag set to 1\n",
					__func__, mode);
#endif
			apu_pbm_param_arr[mode].delay_off_flag = 1;
			mod_timer(&apu_pbm_param_arr[mode].off_timer,
				jiffies + msecs_to_jiffies(
					apu_pbm_param_arr[mode].delay_off_ms));
		} else {
			apu_pbm_aee_warn(
				"APUSYS_POWER", "APU_PBM_COUNTER_UNBALANCE");
		}
	}
#if LOCAL_DBG
	pr_debug("%s m:%d b:%d c:%d f:%u d:%d max_b:%d ret:%d\n",
			__func__,
			apu_pbm_param_arr[mode].mode,
			apu_pbm_param_arr[mode].budget,
			apu_pbm_param_arr[mode].counter,
			apu_pbm_param_arr[mode].delay_off_flag,
			apu_pbm_param_arr[mode].delay_off_ms,
			max_budget,
			ret);
#endif
	spin_unlock_irqrestore(&apu_pbm_lock, flags);

	return ret;
}

int apu_power_budget_checker(enum pbm_mode mode)
{
	unsigned long flags;
	int counter = 0;

	if (mode >= PBM_MODE_MAX) {
		pr_notice("%s#%d mode is over range\n", __func__, __LINE__);
		return -EINVAL;
	}

	spin_lock_irqsave(&apu_pbm_lock, flags);
	counter = apu_pbm_param_arr[mode].counter;
	spin_unlock_irqrestore(&apu_pbm_lock, flags);

	return counter;
}

static void apu_pbm_off_timer_func(struct timer_list *timer)
{
	unsigned long flags;
	struct apu_pbm_param *apu_pbm_instance;
	int mode;

#if LOCAL_DBG
	pr_debug("%s ++\n", __func__);
#endif
	spin_lock_irqsave(&apu_pbm_lock, flags);

	apu_pbm_instance = container_of(timer, struct apu_pbm_param, off_timer);
	mode = apu_pbm_instance->mode;

	if (mode < 0 || mode >= PBM_MODE_MAX) {
		pr_notice("%s#%d mode is over range\n", __func__, __LINE__);
	} else {
#if LOCAL_DBG
		pr_debug("%s mode:%d delay_off_flag set to 0\n",
				__func__, mode);
#endif
		// recalculate power budget since mode 1 -> 0 before N ms ago
		apu_pbm_param_arr[mode].delay_off_flag = 0;
		apu_power_budget_judgement();
	}

	spin_unlock_irqrestore(&apu_pbm_lock, flags);

#if LOCAL_DBG
	pr_debug("%s --\n", __func__);
#endif
}

// caller is middleware
int apu_pbm_drv_init(struct apusys_core_info *info)
{
	int mode;

	pr_notice("%s ++\n", __func__);

	spin_lock_init(&apu_pbm_lock);

	apu_mbox = ioremap(APU_MBOX_BASE, 0x4FF);

	if (IS_ERR((void const *)apu_mbox)) {
		pr_notice("%s remap apu mbox failed\n", __func__);
		return -ENOMEM;
	}

	for (mode = 0 ; mode < PBM_MODE_MAX ; mode++) {
		timer_setup(
			&apu_pbm_param_arr[mode].off_timer,
			apu_pbm_off_timer_func, 0);

		pr_notice("%s pbm_mode:%d, budget:%d(mW), off_delay:%d(ms)\n",
				__func__,
				apu_pbm_param_arr[mode].mode,
				apu_pbm_param_arr[mode].budget,
				apu_pbm_param_arr[mode].delay_off_ms);
	}

	pr_notice("%s --\n", __func__);
	return 0;
}

// caller is middleware
void apu_pbm_drv_exit(void)
{
	int mode;

	pr_notice("%s ++\n", __func__);

	for (mode = 0 ; mode < PBM_MODE_MAX ; mode++) {
		if (timer_pending(&apu_pbm_param_arr[mode].off_timer))
			del_timer(&apu_pbm_param_arr[mode].off_timer);
	}

	if (apu_mbox != NULL)
		iounmap(apu_mbox);

	pr_notice("%s --\n", __func__);
}
