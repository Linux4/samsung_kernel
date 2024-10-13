/*
 * Samsung Mobile VE Group.
 *
 * drivers/battery/sec_batt_selfdchg_exynos7420_ap_use.c
 *
 * Drivers for samsung battery self discharging by S/W Policy, with no IC.
 * For only Exynos 7420
 *
 * Copyright (C) 2015, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

/****************************************************************/
/* Check to Compile Time */
/****************************************************************/
/* Common Feature */
#ifndef CONFIG_OF
#error "[SDCHG] CONFIG_OF is not defined, Check policy!"
#endif

#ifndef CONFIG_FB
#error "[SDCHG] CONFIG_FB is not defined, Check policy!"
#endif

#ifndef CONFIG_PM_SLEEP
#error "[SDCHG] CONFIG_PM_SLEEP is not defined, Check policy!"
#endif

#ifndef CONFIG_BATTERY_SAMSUNG
#error "[SDCHG] CONFIG_BATTERY_SAMSUNG is not defined, Check policy!"
#endif

/* Exynos Feature */
#ifndef CONFIG_ARM_EXYNOS_MP_CPUFREQ
#error "CONFIG_ARM_EXYNOS_MP_CPUFREQ is not defined, Check policy!"
#endif

#ifndef CONFIG_ARM_EXYNOS7420_BUS_DEVFREQ
#error "CONFIG_ARM_EXYNOS7420_BUS_DEVFREQ is not defined, Check policy!"
#endif
/****************************************************************/

//#define SDCHG_SUB_POLICY_SET				// AP Policy Only

#include <linux/sec_batt_selfdchg_common.h>

/******************************************/
// Samsung Custom Header
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
#include <mach/cpufreq.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/battery/sec_battery.h>
#endif
/******************************************/

/******************************************/
/* Code Control Feature (for no discharging IC) */
/******************************************/
#ifdef SDCHG_SELF_TEST
#define SDCHG_BATT_CHECK_DELAY_NORMAL		60
#define SDCHG_BATT_CHECK_DELAY_IMMINENT	60
#define SDCHG_TEMP_FOR_BATT_CHECK_DELAY	400
#else
#define SDCHG_BATT_CHECK_DELAY_NORMAL		600
#define SDCHG_BATT_CHECK_DELAY_IMMINENT	300
#define SDCHG_TEMP_FOR_BATT_CHECK_DELAY	400
#endif
#define SDCHG_DISCHARGING_FIRST_START_DELAY	1
/******************************************/
/* Extra Feature (for no discharging IC) */
/******************************************/
#define SDCHG_STATE_MACHINE_RETRY_AT_END_COND
/******************************************/
/* AP Policy Feature */
/******************************************/
#define SDCHG_POLICY_CL0_FREQ_MIN_LIMIT			BIT(0)
#define SDCHG_POLICY_CL1_FREQ_MIN_LIMIT			BIT(1)
#define SDCHG_POLICY_CPU_IDLE_POLL_CTRL			BIT(2)
#define SDCHG_POLICY_CPU_IDLE_POLL_AUTO_CTRL		BIT(3)
#define SDCHG_POLICY_PM_QOS_DMA_CTRL				BIT(4)
#define SDCHG_POLICY_PM_QOS_BUS_CTRL				BIT(5)
#define SDCHG_POLICY_PM_QOS_DEVICE_CTRL			BIT(6)
/******************************************/
struct sdchg_info_personal_t
{
	struct pm_qos_request pm_qos_req_cl0_min;
	struct pm_qos_request pm_qos_req_mif;
	struct pm_qos_request pm_qos_req_int;
};
struct sdchg_info_personal_t sdchg_info_personal;
/******************************************/
static char sdchg_state_str[SDCHG_STATE_MAX][20] = 
	{ "NONE", "SET", "SET(DISP ON)",
#ifdef SDCHG_SUB_POLICY_SET
		"SET LOW", "SET LOW(DISP ON)"
#endif
	};
/******************************************/

static struct sdchg_info_t *sdchg_info;
static char sdchg_type[] = "sdchg_ap";

extern bool bsdchg_cl0_idle_policy_set;

static unsigned int policy_state[SDCHG_STATE_MAX] = {
	/*************************************/
	/* SDCHG_STATE_NONE */
	0,
	/*************************************/
	/* SDCHG_STATE_SET */
	SDCHG_POLICY_CL0_FREQ_MIN_LIMIT |
	SDCHG_POLICY_CPU_IDLE_POLL_CTRL |
#ifndef SDCHG_SUB_POLICY_SET
	SDCHG_POLICY_PM_QOS_BUS_CTRL | SDCHG_POLICY_PM_QOS_DEVICE_CTRL |
#endif
	0,
	/*************************************/
	/* SDCHG_STATE_SET_DISPLAY_ON */
	//SDCHG_POLICY_CL0_FREQ_MIN_LIMIT |
#ifndef SDCHG_SUB_POLICY_SET
	//SDCHG_POLICY_PM_QOS_BUS_CTRL | SDCHG_POLICY_PM_QOS_DEVICE_CTRL |
#endif
	0,
	/*************************************/
#ifdef SDCHG_SUB_POLICY_SET
	/*************************************/
	/* SDCHG_STATE_SET_LOW */
	SDCHG_POLICY_CL0_FREQ_MIN_LIMIT |
	SDCHG_POLICY_CPU_IDLE_POLL_CTRL |
	SDCHG_POLICY_PM_QOS_BUS_CTRL | SDCHG_POLICY_PM_QOS_DEVICE_CTRL |
	0,
	/*************************************/
	/* SDCHG_STATE_SET_LOW_DISPLAY_ON */
	//SDCHG_POLICY_CL0_FREQ_MIN_LIMIT |
	//SDCHG_POLICY_PM_QOS_BUS_CTRL | SDCHG_POLICY_PM_QOS_DEVICE_CTRL |
	0,
	/*************************************/
#endif
};

#define SDCHG_MIN_FREQ	1000000
#define SDCHG_MIF_THROUGHPUT 1026000
#define SDCHG_INT_THROUGHPUT 400000

static void sdchg_policy_set(struct sdchg_info_nochip_t *info)
{
	static unsigned int policy_mask = 0;
	static unsigned int policy_mask_prev = 0;
	struct sdchg_info_personal_t *pData = info->pData;

	pr_info("[SDCHG][%s] state set (%s -> %s)\n", __func__,
		sdchg_state_str[info->set_state], sdchg_state_str[info->need_state]);

	policy_mask = policy_state[info->need_state];
	/*********************************************/
	if (policy_mask & SDCHG_POLICY_PM_QOS_BUS_CTRL) {
		if (!(policy_mask_prev & SDCHG_POLICY_PM_QOS_BUS_CTRL)) {
			// enable max : 1552000
			pm_qos_update_request(&pData->pm_qos_req_mif, SDCHG_MIF_THROUGHPUT);
		}
	} else {
		if (policy_mask_prev & SDCHG_POLICY_PM_QOS_BUS_CTRL) {
			pm_qos_update_request(&pData->pm_qos_req_mif, PM_QOS_DEFAULT_VALUE);
		}
	}
	/*********************************************/
	if (policy_mask & SDCHG_POLICY_PM_QOS_DEVICE_CTRL) {
		if (!(policy_mask_prev & SDCHG_POLICY_PM_QOS_DEVICE_CTRL)) {
			pm_qos_update_request(&pData->pm_qos_req_int, SDCHG_INT_THROUGHPUT);
		}
	} else {
		if (policy_mask_prev & SDCHG_POLICY_PM_QOS_DEVICE_CTRL) {
			pm_qos_update_request(&pData->pm_qos_req_int, PM_QOS_DEFAULT_VALUE);
		}
	}
	/*********************************************/
	if (policy_mask & SDCHG_POLICY_CL0_FREQ_MIN_LIMIT) {
		if ( !(policy_mask_prev & SDCHG_POLICY_CL0_FREQ_MIN_LIMIT)) {
			pm_qos_update_request(&pData->pm_qos_req_cl0_min, SDCHG_MIN_FREQ);
		}
	} else {
		if ( policy_mask_prev & SDCHG_POLICY_CL0_FREQ_MIN_LIMIT) {
			pm_qos_update_request(&pData->pm_qos_req_cl0_min, PM_QOS_DEFAULT_VALUE);
		}
	}
	/*********************************************/
	if (policy_mask & SDCHG_POLICY_CPU_IDLE_POLL_CTRL) {
		if (!(policy_mask_prev & SDCHG_POLICY_CPU_IDLE_POLL_CTRL)) {
			bsdchg_cl0_idle_policy_set = true; 
		}
	} else {
		if (policy_mask_prev & SDCHG_POLICY_CPU_IDLE_POLL_CTRL) {
			bsdchg_cl0_idle_policy_set = false; 
		}
	}
	/*********************************************/
	policy_mask_prev = policy_mask;
	info->set_state = info->need_state;

	pr_info("[SDCHG][%s] state set end!\n", __func__);
	return;
}

static bool sdchg_ta_attach(struct sec_battery_info *battery)
{
	if (battery->wc_status || battery->ps_status)
		return true;

	if (battery->wire_status != POWER_SUPPLY_TYPE_BATTERY)
		return true;

	return false;
}

static void sdchg_exynos7420_ap_use_monitor(void *arg,
					__kernel_time_t curr_sec, bool skip_monitor)
{
	struct sdchg_info_nochip_t *info = sdchg_info->nochip;
	struct sec_battery_info *battery = (struct sec_battery_info *)arg;

	int temperature = 0;
	static int battcond = 0;
	bool need_set_state = false;
	bool need_set_alarm = false;
	int set_alarm_time = 0;

	bool runned_by_sdchg_poll;
#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
	static bool state_machine_retry = false;
#endif

#if defined(SDCHG_SUB_POLICY_SET) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
	int i = 0;
	int cpu_CL0_online_num = 0;
#endif		// #ifdef SDCHG_SUB_POLICY_SET

	/******************************************/
	runned_by_sdchg_poll = sdchg_check_polling_time(curr_sec);

	temperature = battery->temperature;

	if (skip_monitor) {
#ifdef SDCHG_CHECK_TYPE_SOC
#if 0
		/**************************************/
		/* Example Code ( If you need, 
		     Define this code in accordance 
		     with the specification of each BB platform */
		{
			union power_supply_propval value;

			/* To get SOC value (NOT raw SOC), need to reset value */
			value.intval = 0;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_CAPACITY, value);

			battcond = value.intval;
		}
		/**************************************/
#else
#error "SDCHG : Implement SOC Read Code!!"
#endif
#else
		battcond = battery->voltage_now;
#endif
	}
	else {
#ifdef SDCHG_CHECK_TYPE_SOC
#if 0
		/**************************************/
		/* Example Code ( If you need, 
			 Define this code in accordance 
			 with the specification of each BB platform */

		battcond = battery->capacity;
		/**************************************/
#else
#error "SDCHG : Implement SOC Read Code!!"
#endif
#else
		battcond = battery->voltage_avg;
#endif
	}

	/******************************************/

	if ( temperature >= (int)sdchg_info->temp_start
			&& battcond >= SDCHG_BATTCOND_START)
	{
		/******************************************/
		if (!info->wake_lock_set) {
			wake_lock(&info->wake_lock);
			info->wake_lock_set = true;
		}
		/******************************************/
#if defined(SDCHG_SUB_POLICY_SET) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
		for (i = 0; i < NR_CLUST0_CPUS; i++) {
			if (cpu_online(i)) {
				cpu_CL0_online_num++;
			}
		}
		if (cpu_CL0_online_num < 3) {
			info->need_state = SDCHG_STATE_SET_LOW;
		} else
#endif
		{
			info->need_state = SDCHG_STATE_SET;
		}
	}
	/******************************************/
	else if (temperature <= (int)sdchg_info->temp_end
			|| battcond <= SDCHG_BATTCOND_END)
	{
		info->need_state = SDCHG_STATE_NONE;
	}
	/******************************************/
	else {
		if (info->need_state != SDCHG_STATE_NONE) {
#if defined(SDCHG_SUB_POLICY_SET) && defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
			for (i = 0; i < NR_CLUST0_CPUS; i++) {
				if (cpu_online(i)) {
					cpu_CL0_online_num++;
				}
			}
			if (cpu_CL0_online_num < 3) {
				info->need_state = SDCHG_STATE_SET_LOW;
			} else	
#endif
			{
				info->need_state = SDCHG_STATE_SET;
			}
		}
	}
	/******************************************/
#if defined(SDCHG_SUB_POLICY_SET) && \
	defined(CONFIG_SCHED_HMP) && defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG)
	if (!info->display_on && info->need_state == SDCHG_STATE_SET_LOW) {
		SDCHG_LOG("[SDCHG][%s] cluster0_core1_hotplug_in() ++\n", __func__);
		cluster0_core1_hotplug_in(true);
		SDCHG_LOG("[SDCHG][%s] cluster0_core1_hotplug_in() --\n", __func__);
		mdelay(5);
#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ)
		for (i = 0; i < NR_CLUST0_CPUS; i++) {
			if (cpu_online(i)) {
				cpu_CL0_online_num++;
			}
		}
		if (cpu_CL0_online_num < 4)
			info->need_state = SDCHG_STATE_SET_LOW;
		else
			info->need_state = SDCHG_STATE_SET;
#endif
	}
#endif

	/****************************************/
	if (info->display_on) {
		if (info->need_state == SDCHG_STATE_SET)
			info->need_state = SDCHG_STATE_SET_DISPLAY_ON;
#ifdef SDCHG_SUB_POLICY_SET
		if (info->need_state == SDCHG_STATE_SET_LOW)
			info->need_state = SDCHG_STATE_SET_LOW_DISPLAY_ON;
#endif
	}

#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
	if (info->need_state != SDCHG_STATE_NONE)
		state_machine_retry = false;
#endif

	/****************************************/
	if (info->set_state != info->need_state) {
		need_set_state = true;

		if (info->set_state == SDCHG_STATE_NONE) {	// none -> discharing
			{
				//need_set_alarm = info->state_machine_run = true;
				//set_alarm_time = SDCHG_DISCHARGING_DELAY;
				need_set_alarm = false; // in discharing, according to original monitor work time
				info->state_machine_run = true;
			}
		} else {		// prev : discharging
			if (info->need_state == SDCHG_STATE_NONE) {	// discharging -> none
				if (sdchg_ta_attach(battery) || battcond >= SDCHG_BATTCOND_START) {
					need_set_alarm = info->state_machine_run = true;
					if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL;
					else
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
					state_machine_retry = false;
#endif
				} else {
#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
#ifdef SDCHG_SELF_TEST
					need_set_alarm = info->state_machine_run = true;
					if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
					else
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
#else
					if (state_machine_retry) {
						if (runned_by_sdchg_poll) {
							need_set_alarm = info->state_machine_run = false;
							state_machine_retry = false;							
						} else {
							need_set_alarm = info->state_machine_run = true;
							if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
								set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
							else
								set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
						}
					} else {
						need_set_alarm = info->state_machine_run = true;
						if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
							set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
						else
							set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
						state_machine_retry = true;
					}
#endif	// #ifdef SDCHG_SELF_TEST
#else
					need_set_alarm = info->state_machine_run = false;
#endif	// #ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
				}
			} else {	// discharging -> discharging
				//need_set_alarm = info->state_machine_run = true;
				//set_alarm_time = SDCHG_DISCHARGING_DELAY;
				need_set_alarm = false; // in discharing, according to original monitor work time
				info->state_machine_run = true;
			}
		}
	}
	/****************************************/
	else {
//KEEP_GOING:
		need_set_state = false;

		if (info->need_state == SDCHG_STATE_NONE) {	// prev : none
			if (sdchg_ta_attach(battery) || battcond >= SDCHG_BATTCOND_START) {
				need_set_alarm = info->state_machine_run = true;
				if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
					set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
				else
					set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
				state_machine_retry = false;
#endif
			} else {
#ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
#ifdef SDCHG_SELF_TEST
				need_set_alarm = info->state_machine_run = true;
				if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
					set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
				else
					set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
#else
				if (state_machine_retry) {
					if (runned_by_sdchg_poll) {
						need_set_alarm = info->state_machine_run = false;
						state_machine_retry = false;							
					} else {
						need_set_alarm = info->state_machine_run = true;
						if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
							set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
						else
							set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
					}
				} else {
					need_set_alarm = info->state_machine_run = true;
					if (temperature < SDCHG_TEMP_FOR_BATT_CHECK_DELAY)
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_NORMAL; 
					else
						set_alarm_time = SDCHG_BATT_CHECK_DELAY_IMMINENT;
					state_machine_retry = true;
				}
#endif	// #ifdef SDCHG_SELF_TEST

#else
				need_set_alarm = info->state_machine_run = false;
#endif	// #ifdef SDCHG_STATE_MACHINE_RETRY_AT_END_COND
			}
		} else {		// prev : discharging
			//need_set_alarm = info->state_machine_run = true;
			//set_alarm_time = SDCHG_DISCHARGING_DELAY;
			need_set_alarm = false;	// in discharing, according to original monitor work time
			info->state_machine_run = true;
		}
	}
	/****************************************/
	if (need_set_state) {
		sdchg_policy_set(info);
	}

	if (need_set_alarm)
		sdchg_set_polling_time(set_alarm_time);
	else
		sdchg_set_polling_time(0);

#ifdef SDCHG_CHECK_TYPE_SOC
		pr_info("[SDCHG][%s] soc : %d , temp : %d, state : %s\n", 
			__func__, battcond, temperature, sdchg_state_str[info->set_state]);
#else
		pr_info("[SDCHG][%s] volt : %d , temp : %d, state : %s\n", 
			__func__, battcond, temperature, sdchg_state_str[info->set_state]);
#endif

	if (info->set_state == SDCHG_STATE_NONE)
	{
		if (info->wake_lock_set) {
			wake_lock_timeout(&info->end_wake_lock, HZ * 5);
			wake_unlock(&info->wake_lock);
			info->wake_lock_set = false;
		}
	}

	return;
}

/*************************************************************************/
static int sdchg_fb_notifier_callback(struct notifier_block *nb, 
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *fb_info = evdata->info;
	struct sdchg_info_nochip_t *info =
		(struct sdchg_info_nochip_t *)container_of(nb, struct sdchg_info_nochip_t, fb_nb);

	int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (fb_info->node)
		return NOTIFY_OK;

	blank = *(int *)evdata->data;

	switch (blank) {
	/**************************************/
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		info->display_on = false;
		break;
	/**************************************/
	case FB_BLANK_UNBLANK:
		info->display_on = true;
		break;
	default:
		return NOTIFY_OK;
	}

	return NOTIFY_OK;
}


/*************************************************************************/
static void init_info_data(struct sdchg_info_nochip_t *info)
{
	struct sdchg_info_personal_t *pData = info->pData;

	info->need_state = SDCHG_STATE_NONE;
	info->set_state = SDCHG_STATE_NONE;

	wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND,
		   "sdchg");
	wake_lock_init(&info->end_wake_lock, WAKE_LOCK_SUSPEND,
		   "sdchg_end");
	info->wake_lock_set =false;

	info->state_machine_run = false;

#ifdef CONFIG_FB
	info->display_on = false;
	info->fb_nb.notifier_call = sdchg_fb_notifier_callback;
	fb_register_client(&info->fb_nb);
#endif

	pm_qos_add_request(&pData->pm_qos_req_cl0_min,
					PM_QOS_CLUSTER0_FREQ_MIN,
					PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&pData->pm_qos_req_mif,
					PM_QOS_BUS_THROUGHPUT,
					PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&pData->pm_qos_req_int,
					PM_QOS_DEVICE_THROUGHPUT,
					PM_QOS_DEFAULT_VALUE);

	return;
}

static void remove_info_data(struct sdchg_info_nochip_t *info)
{
	struct sdchg_info_personal_t *pData = info->pData;

	pm_qos_remove_request(&pData->pm_qos_req_cl0_min);
	pm_qos_remove_request(&pData->pm_qos_req_mif);
	pm_qos_remove_request(&pData->pm_qos_req_int);

	fb_unregister_client(&info->fb_nb);
}

/*************************************************************************/
static void sdchg_exynos7420_ap_use_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *data_np;

	data_np = of_get_child_by_name(np, sdchg_type);

#ifdef SDCHG_SELF_TEST
	sdchg_info->temp_start = SDCHG_TEMP_START;
	sdchg_info->temp_end = SDCHG_TEMP_END;
	sdchg_info->soc_start = SDCHG_SOC_START;
	sdchg_info->soc_end = SDCHG_SOC_END;
	sdchg_info->voltage_start = SDCHG_VOLTAGE_START;
	sdchg_info->voltage_end = SDCHG_VOLTAGE_END;
#else
	if (of_property_read_u32(data_np, "sdchg,temperature_start",
							&sdchg_info->temp_start)) {
		sdchg_info->temp_start = SDCHG_TEMP_START;
		pr_info("[SDCHG][%s] temp_start dt is Empty (defalut:%d)", 
			__func__, sdchg_info->temp_start);
	}

	if (of_property_read_u32(data_np, "sdchg,temperature_end",
							 &sdchg_info->temp_end)) {
		sdchg_info->temp_end = SDCHG_TEMP_END;
		pr_info("[SDCHG][%s] temp_end dt is Empty  (defalut:%d)", 
			__func__, sdchg_info->temp_end);
	}

	if (of_property_read_u32(data_np, "sdchg,soc_start",
							   &sdchg_info->soc_start)) {
		sdchg_info->soc_start = SDCHG_SOC_START;
		pr_info("[SDCHG][%s] soc_start dt is Empty  (defalut:%d)", 
			__func__, sdchg_info->soc_start);
	}

	if (of_property_read_u32(data_np, "sdchg,soc_end",
							   &sdchg_info->soc_end)) {
		sdchg_info->soc_end = SDCHG_SOC_END;
		pr_info("[SDCHG][%s] soc_end dt is Empty  (defalut:%d)", 
			__func__, sdchg_info->soc_end);
	}

	if (of_property_read_u32(data_np, "sdchg,voltage_start",
							   &sdchg_info->voltage_start)) {
		sdchg_info->voltage_start = SDCHG_VOLTAGE_START;
		pr_info("[SDCHG][%s] voltage_start dt is Empty  (defalut:%d)", 
			__func__, sdchg_info->voltage_start);
	}

	// voltage_end : using swelling_drop_float_voltage value
	if (of_property_read_u32(np, "battery,swelling_drop_float_voltage",
							   &sdchg_info->voltage_end)) {
		sdchg_info->voltage_end = SDCHG_VOLTAGE_END;
		pr_info("[SDCHG][%s] voltage_end dt is Empty  (defalut:%d)", 
			__func__, sdchg_info->voltage_end);
	}
#endif
	 pr_info("[SDCHG][%s] temp_start = %u\n", __func__, sdchg_info->temp_start);
	 pr_info("[SDCHG][%s] temp_end = %u\n", __func__, sdchg_info->temp_end);
	 pr_info("[SDCHG][%s] soc_start = %u\n", __func__, sdchg_info->soc_start);
	 pr_info("[SDCHG][%s] soc_end = %u\n", __func__, sdchg_info->soc_end);
	 pr_info("[SDCHG][%s] voltage_start = %u\n", __func__, sdchg_info->voltage_start);
	 pr_info("[SDCHG][%s] voltage_end = %u\n", __func__, sdchg_info->voltage_end);

	return;
 }

/* prev func : sec_bat_self_discharging_check */
 static void sdchg_exynos7420_ap_use_adc_check(void *arg)
 {
	struct sec_battery_info *battery = (struct sec_battery_info *)arg;
 
	if (battery->force_discharging) {
		battery->self_discharging_adc = 2100;
		battery->self_discharging = true;
	}
	else {
		battery->self_discharging_adc = 0;
		battery->self_discharging = false;
	}

 	 pr_info("%s : SELF_DISCHARGING(%d) SELF_DISCHARGING_ADC(%d)\n",
		 __func__, battery->self_discharging, battery->self_discharging_adc);
  	 return;
 }


/* prev func : sec_bat_self_discharging_ntc_check */
static void sdchg_exynos7420_ap_use_ntc_check(void *arg)
{
	struct sec_battery_info *battery = (struct sec_battery_info *)arg;

	battery->discharging_ntc_adc = 0;
	battery->discharging_ntc = false;

	pr_info("%s : DISCHARGING_NTC(%d) DISCHARGING_NTC_ADC(%d)\n",
		__func__,battery->discharging_ntc, battery->discharging_ntc_adc);

	return;
}

 /* prev func : sec_bat_self_discharging_control */
 static void sdchg_exynos7420_ap_use_force_control(void *arg, bool dis_en)
 {
	 struct sec_battery_info *battery = (struct sec_battery_info *)arg;
 
	 if (dis_en) {
		 battery->force_discharging = true;
	 } else {
		 battery->force_discharging = false;
	 }
 
	 return;
 }

 /* prev func : sec_bat_discharging_check */
 static void sdchg_exynos7420_ap_use_discharging_check(void *arg)
 {
	 return;
 }
 
static int sdchg_exynos7420_ap_use_force_check(void *arg)
{
	struct sec_battery_info *battery = (struct sec_battery_info *)arg;

	return (int)battery->force_discharging;
}



static int sdchg_exynos7420_ap_use_probe(void *battery)
{
	int ret = 0;
	struct sdchg_info_nochip_t *info = sdchg_info->nochip;
	pr_info("[SDCHG][%s] ++\n", __func__);

	sdchg_info->battery = battery;

	init_info_data(info);

	pr_info("[SDCHG][%s] --(%d)\n", __func__, ret);

	return ret;
}

static int sdchg_exynos7420_ap_use_remove(void)
{
	struct sdchg_info_nochip_t *info = sdchg_info->nochip;
	pr_info("[SDCHG][%s] ++\n", __func__);

	if (info->set_state != SDCHG_STATE_NONE) {
		info->need_state = SDCHG_STATE_NONE;

		sdchg_policy_set(info);
		info->state_machine_run = false;

		/**************************************/
		if (info->wake_lock_set) {
			wake_unlock(&info->wake_lock);
			info->wake_lock_set = false;
		}
		/**************************************/
	}

	/* need to run after sdchg_policy_set */
	remove_info_data(info);

	pr_info("[SDCHG][%s] --\n", __func__);
	
	return 0;
}

 static int __init sdchg_exynos7420_ap_use_init(void)
 {
	 int ret;

	pr_info("[SDCHG][%s] ++\n", __func__);

	sdchg_info = kzalloc(sizeof(struct sdchg_info_t), GFP_KERNEL);
	if (!sdchg_info) {
		pr_err("Failed to allocate memory for self discharging\n");
		ret = -ENOMEM;
		goto fail_out;
	}
	sdchg_info->type = sdchg_type;

	/*****************************************/
	sdchg_info->nochip = kzalloc(sizeof(struct sdchg_info_nochip_t), GFP_KERNEL);
	if (!sdchg_info) {
		pr_err("Failed to allocate nochip memory for self discharging\n");
		ret = -ENOMEM;
		goto after_alloc_info;
	}
	sdchg_info->nochip->pinfo = sdchg_info;
	sdchg_info->nochip->pData = (void *)&sdchg_info_personal;

	sdchg_info->nochip->sdchg_monitor = sdchg_exynos7420_ap_use_monitor;
	/*****************************************/
	sdchg_info->sdchg_probe = sdchg_exynos7420_ap_use_probe;
	sdchg_info->sdchg_remove = sdchg_exynos7420_ap_use_remove;
	sdchg_info->sdchg_parse_dt = sdchg_exynos7420_ap_use_parse_dt;

	sdchg_info->sdchg_adc_check = sdchg_exynos7420_ap_use_adc_check;
	sdchg_info->sdchg_ntc_check = sdchg_exynos7420_ap_use_ntc_check;
	sdchg_info->sdchg_force_control = sdchg_exynos7420_ap_use_force_control;
	sdchg_info->sdchg_discharging_check = sdchg_exynos7420_ap_use_discharging_check;

	sdchg_info->sdchg_force_check = sdchg_exynos7420_ap_use_force_check;
	/*****************************************/

	list_add(&sdchg_info->info_list, &sdchg_info_head);

	pr_info("[SDCHG][%s] --\n", __func__);

	return 0;

after_alloc_info:
	if (sdchg_info) {
		kfree(sdchg_info);
		sdchg_info = NULL;
	}
fail_out:
	pr_info("[SDCHG][%s] --(%d)\n", __func__, ret);

	return ret;
 }


static void __exit sdchg_exynos7420_ap_use_exit(void)
{
	pr_info("[SDCHG][%s] ++\n", __func__);

	if (sdchg_info) {
		if (sdchg_info->nochip) {
			kfree(sdchg_info->nochip);
			sdchg_info->nochip = NULL;
		}
		kfree(sdchg_info);
		sdchg_info = NULL;
	}
	pr_info("[SDCHG][%s] --\n", __func__);

	return;
}

arch_initcall(sdchg_exynos7420_ap_use_init);
module_exit(sdchg_exynos7420_ap_use_exit);

MODULE_AUTHOR("jeeon.park@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Co. Battery Self Discharging \
	for Preventing Swelling by S/W Policy, with no IC");
MODULE_LICENSE("GPL");

