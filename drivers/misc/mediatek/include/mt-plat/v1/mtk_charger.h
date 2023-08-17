/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_CHARGER_H__
#define __MTK_CHARGER_H__

#include <linux/ktime.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
//#include <mach/mtk_charger_init.h>

#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/charger_class.h>
/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/11 start */
#if defined(CONFIG_HQ_PROJECT_O22)
#include <linux/chg-tcpc_info.h>

extern enum tcpc_cc_supplier tcpc_info;
#endif
/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/11 end */

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
extern struct charger_manager *ssinfo;
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

/* charger_manager notify charger_consumer */
enum {
	CHARGER_NOTIFY_EOC,
	CHARGER_NOTIFY_START_CHARGING,
	CHARGER_NOTIFY_STOP_CHARGING,
	CHARGER_NOTIFY_ERROR,
	CHARGER_NOTIFY_NORMAL,
};

enum {
	MAIN_CHARGER = 0,
	SLAVE_CHARGER = 1,
	TOTAL_CHARGER = 2,
	DIRECT_CHARGER = 10,
	MAIN_DIVIDER_CHARGER = 20,
	SLAVE_DIVIDER_CHARGER = 21,
};

struct charger_consumer {
	struct device *dev;
	void *cm;
	struct notifier_block *pnb;
	struct list_head list;
	bool hv_charging_disabled;
};

/* ============================================= */
/* The following are charger consumer interfaces */
/* ============================================= */

/* @supply_name: name of charging port
 * use charger_port1, charger_port2, ...
 * for most cases, use charging_port1
 */
extern struct charger_consumer *charger_manager_get_by_name(
	struct device *dev,
	const char *supply_name);
extern int charger_manager_set_input_current_limit(
	struct charger_consumer *consumer,
	int idx,
	int input_current_uA);
extern int charger_manager_set_charging_current_limit(
	struct charger_consumer *consumer,
	int idx,
	int charging_current_uA);
extern int charger_manager_set_pe30_input_current_limit(
	struct charger_consumer *consumer,
	int idx,
	int input_current_uA);
extern int charger_manager_get_pe30_input_current_limit(
	struct charger_consumer *consumer,
	int idx,
	int *input_current_uA,
	int *min_current_uA,
	int *max_current_uA);
extern int charger_manager_get_current_charging_type(
	struct charger_consumer *consumer);
extern int register_charger_manager_notifier(
	struct charger_consumer *consumer,
	struct notifier_block *nb);
extern int charger_manager_get_charger_temperature(
	struct charger_consumer *consumer,
	int idx,
	int *tchg_min,
	int *tchg_max);
extern int unregister_charger_manager_notifier(
	struct charger_consumer *consumer,
	struct notifier_block *nb);
extern int charger_manager_enable_high_voltage_charging(
	struct charger_consumer *consumer,
	bool en);
extern int charger_manager_enable_power_path(
	struct charger_consumer *consumer,
	int idx,
	bool en);
extern int charger_manager_force_disable_power_path(
	struct charger_consumer *consumer,
	int idx,
	bool disable);
extern int charger_manager_enable_charging(
	struct charger_consumer *consumer,
	int idx,
	bool en);
extern int charger_manager_get_zcv(
	struct charger_consumer *consumer,
	int idx,
	u32 *uV);
extern int charger_manager_set_constant_voltage(
	struct charger_consumer *consumer,
	int idx,
	u32 uV);
extern int charger_manager_enable_chg_type_det(
	struct charger_consumer *consumer,
	bool en);
extern int mtk_chr_is_charger_exist(unsigned char *exist);
extern bool is_power_path_supported(void);
extern int charger_get_vbus(void);
extern bool mt_charger_plugin(void);

/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
extern int charger_manager_input_suspend(struct charger_consumer *consumer,
	int idx,
	bool en);
/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

/* hs14 code for SR-AL6528A-01-259 by qiaodan at 2022/10/17 start*/
extern int charger_manager_get_shipmode(struct charger_consumer *consumer,
	int idx);
extern int charger_manager_set_shipmode(struct charger_consumer *consumer,
	int idx);
/* hs14 code for SR-AL6528A-01-259 by qiaodan at 2022/10/17 end*/

/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
extern int charger_manager_get_chr_type(struct charger_consumer *consumer,
	int idx, int *chr_type);
/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/
/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
extern int charger_manager_dump_charger_ic(struct charger_consumer *consumer,
	int idx);
/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */
/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
extern int charger_manager_bypass_chgdet(struct charger_consumer *consumer,
	int idx, bool bypass_chgdet);
/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
extern bool ss_fast_charger_status(struct charger_manager *info);
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#endif /* __MTK_CHARGER_H__ */
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 start*/
#ifdef CONFIG_HQ_PROJECT_O22
extern int register_usb_check_notifier(struct notifier_block *nb);
extern int unregister_usb_check_notifier(struct notifier_block *nb);
#endif
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 end*/
