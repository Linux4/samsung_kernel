/*
 * sec-auth-ds28e30.h - header file of authon driver
 *
 * Copyright (C) 2023 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __SEC_AUTH_SLE956681_H__
#define __SEC_AUTH_SLE956681_H__
#include <linux/version.h>
ssize_t sec_auth_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_auth_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

ssize_t sec_auth_2nd_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_auth_2nd_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_AUTH_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_auth_show_attrs,			\
	.store = sec_auth_store_attrs,			\
}

#define SEC_AUTH_2ND_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_auth_2nd_show_attrs,			\
	.store = sec_auth_2nd_store_attrs,			\
}

#define MEMORY_MAP_PAGES				18
#define PAGE_BYTE_CNT					4

#define QR_CODE_PAGE					0
#define QR_CODE_START_BYTE				0
#define QR_CODE_BYTE_CNT				32

#define SYNC_START_PAGE					8

#define FIRST_USE_DATE_PAGE				8
#define	FIRST_USE_DATE_START_BYTE		0
#define FIRST_USE_DATE_BYTE_CNT			8

#define MMAP_VERSION_PAGE				10
#define	MMAP_VERSION_START_BYTE			0
#define MMAP_VERSION_BYTE_CNT			1

#define BATT_DISCHARGE_LEVEL_PAGE		10
#define	BATT_DISCHARGE_LEVEL_START_BYTE	1
#define BATT_DISCHARGE_LEVEL_BYTE_CNT	3

#define BSOH_PAGE						11
#define	BSOH_START_BYTE					0
#define BSOH_BYTE_CNT					2

#define BSOH_RAW_PAGE					11
#define	BSOH_RAW_START_BYTE				2
#define BSOH_RAW_BYTE_CNT				2

#define ASOC_PAGE						12
#define	ASOC_START_BYTE					0
#define ASOC_BYTE_CNT					2

#define RECHARGING_COUNT_PAGE			12
#define	RECHARGING_COUNT_START_BYTE		2
#define RECHARGING_COUNT_BYTE_CNT		2

#define CAP_NOM_PAGE					13
#define	CAP_NOM_START_BYTE				0
#define CAP_NOM_BYTE_CNT				2

#define CAP_MAX_PAGE					13
#define	CAP_MAX_START_BYTE				2
#define CAP_MAX_BYTE_CNT				2

#define BATT_THM_MIN_PAGE				14
#define	BATT_THM_MIN_START_BYTE			0
#define BATT_THM_MIN_BYTE_CNT			2

#define BATT_THM_MAX_PAGE				14
#define	BATT_THM_MAX_START_BYTE			2
#define BATT_THM_MAX_BYTE_CNT			2

#define VBAT_OVP_PAGE					15
#define	VBAT_OVP_START_BYTE				0
#define VBAT_OVP_BYTE_CNT				2

#define UNSAFETY_TEMP_PAGE				15
#define	UNSAFETY_TEMP_START_BYTE		2
#define UNSAFETY_TEMP_BYTE_CNT			2

#define SAFETY_TIMER_PAGE				16
#define	SAFETY_TIMER_START_BYTE			0
#define SAFETY_TIMER_BYTE_CNT			2

#define DROP_SENSOR_PAGE				16
#define	DROP_SENSOR_START_BYTE			2
#define DROP_SENSOR_BYTE_CNT			2

#define BATT_FULL_STATUS_USAGE_PAGE		17
#define	BATT_FULL_STATUS_USAGE_BYTE		0
#define BATT_FULL_STATUS_USAGE_CNT		3

#define FAI_EXPIRED_PAGE				17
#define	FAI_EXPIRED_START_BYTE			3
#define FAI_EXPIRED_BYTE_CNT			1

#define HEATMAP_FIRST_PAGE				18
#define	HEATMAP_FIRST_START_BYTE		0
#define HEATMAP_FIRST_BYTE_CNT			32

#define HEATMAP_SECOND_PAGE				26
#define	HEATMAP_SECOND_START_BYTE		0
#define HEATMAP_SECOND_BYTE_CNT			32

#define HEATMAP_THIRD_PAGE				34
#define	HEATMAP_THIRD_START_BYTE		0
#define HEATMAP_THIRD_BYTE_CNT			32

#if defined(CONFIG_SEC_FACTORY)
#define SYNC_END_PAGE					17
#else
#define SYNC_END_PAGE					41
#endif

#define MAX_SWI_GPIO						2

#define SEC_AUTH_RETRY		(10)
#define HOUR_SEC			(3600)
#define HOUR_NS				(3600000000000)   /* ((u64)HOUR_SEC * 1000000000) */
#define DAY_NS				(86400000000000)  /* ((u64)(24) * HOUR_NS) */
#define MAX_WORK_DELAY		(HOUR_SEC)

static bool pageLockStatus[MEMORY_MAP_PAGES] = {
		true,	/* 0 */
		true,	/* 1 */
		true,	/* 2 */
		true,	/* 3 */
		true,	/* 4 */
		true,	/* 5 */
		true,	/* 6 */
		true,	/* 7 */
		false,	/* 8 */
		false,	/* 9 */
		false,	/* 10 */
		false,	/* 11 */
		false,	/* 12 */
		false,	/* 13 */
		false,	/* 14 */
		false,	/* 15 */
		false,	/* 16 */
		false,	/* 17 */
};

static bool pageLockStatus2[MEMORY_MAP_PAGES] = {
		true,	/* 0 */
		true,	/* 1 */
		true,	/* 2 */
		true,	/* 3 */
		true,	/* 4 */
		true,	/* 5 */
		true,	/* 6 */
		true,	/* 7 */
		false,	/* 8 */
		false,	/* 9 */
		false,	/* 10 */
		false,	/* 11 */
		false,	/* 12 */
		false,	/* 13 */
		false,	/* 14 */
		false,	/* 15 */
		false,	/* 16 */
		false,	/* 17 */
};

enum {
	SYNC_FAILURE = -1,
	SYNC_NOT_STARTED,
	SYNC_SUCCESS,
	SYNC_PROGRESS,
};

enum {
	PRESENCE = 0,
	BATT_AUTH,
	DECREMENT_COUNTER,
	FIRST_USE_DATE,
	USE_DATE_WLOCK,
	BATT_DISCHARGE_LEVEL,
	BATT_FULL_STATUS_USAGE,
	BSOH,
	BSOH_RAW,
	QR_CODE,
	ASOC,
	CAP_NOM,
	CAP_MAX,
	BATT_THM_MIN,
	BATT_THM_MAX,
	UNSAFETY_TEMP,
	VBAT_OVP,
	RECHARGING_COUNT,
	SAFETY_TIMER,
	DROP_SENSOR,
	SYNC_BUF_MEM,
	SYNC_BUF_MEM_STS,
	FAI_EXPIRED,
	CHIPNAME,
	BATT_HEATMAP,
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	TAU_VALUES,
	CHECK_PASSRATE,
	WORK_START,
#endif
	AUTH_SYSFS_MAX,
};

struct authon_cisd {
	//int asoc;
	int cap_nom;
	int cap_max;
	int batt_thm_min;
	int batt_thm_max;
	int unsafety_temperature;
	int vbat_ovp;
	int recharging_count;
	int safety_timer;
	int drop_sensor;

	//int asoc_prev;
	int cap_nom_prev;
	int cap_max_prev;
	int batt_thm_min_prev;
	int batt_thm_max_prev;
	int unsafety_temperature_prev;
	int vbat_ovp_prev;
	int recharging_count_prev;
	int safety_timer_prev;
	int drop_sensor_prev;
};

struct authon_driver_platform_data {
	int swi_gpio[MAX_SWI_GPIO];
	int swi_gpio_cnt;
};

struct authon_driver_data {
	struct device *dev;
	struct mutex sec_auth_mutex;
	struct authon_driver_platform_data *pdata;
	struct power_supply	*psy_sec_auth;
	struct power_supply	*psy_sec_auth_2nd;
	struct workqueue_struct *sec_auth_poll_wqueue;
	struct delayed_work sec_auth_poll_work;
	u64 sync_time;
};

static int get_batt_discharge_level(unsigned char *data);

#endif  /* __SEC_AUTH_DS28E30_H__ */
