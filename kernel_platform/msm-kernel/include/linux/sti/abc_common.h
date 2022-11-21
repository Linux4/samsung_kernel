/* abc_common.h
 *
 * Abnormal Behavior Catcher Common Driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * Hyeokseon Yu <hyeokseon.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SEC_ABC_H
#define SEC_ABC_H
#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <kunit/test.h>
#include <kunit/mock.h>
#else
#define __visible_for_testing static
#endif
#if (KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE)
#include <linux/sched/clock.h>
#else
#include <linux/sched.h>
#endif
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
#include <linux/sti/abc_motto.h>
#endif
#define ABC_CMD_MAX					8
#define ABC_UEVENT_MAX				4
#define ABC_BUFFER_MAX				64
#define ABC_CMD_STR_MAX				16
#define ABC_TYPE_STR_MAX			8
#define ABC_EVENT_STR_MAX			60
#define ABC_WORK_MAX				5
#define ABC_WAIT_ENABLE_TIMEOUT		10000
#define ERROR_REPORT_MODE_BIT		(1<<0)
#define ALL_REPORT_MODE_BIT			(1<<1)
#define PRE_EVENT_ENABLE_BIT		(1<<7)
#define ABC_DISABLED				0
#define ABC_PREOCCURRED_EVENT_MAX	30
#define TIME_STAMP_STR_MAX			25
#define ABC_CLEAR_EVENT_TIMEOUT		300000
#define SPEC_DISABLED				0

enum abc_enable_cmd {
	ERROR_REPORT_MODE_ENABLE = 0,
	ERROR_REPORT_MODE_DISABLE,
	ALL_REPORT_MODE_ENABLE,
	ALL_REPORT_MODE_DISABLE,
	PRE_EVENT_ENABLE,
	PRE_EVENT_DISABLE,
	ABC_ENABLE_CMD_MAX,
};

struct abc_enable_cmd_struct {
	enum abc_enable_cmd cmd;
	int enable_value;
	char abc_cmd_str[ABC_CMD_STR_MAX];
};

enum DATA_TYPE {
	TYPE_STRING,
	TYPE_INT,
};

struct abc_key_data {
	char event_type[ABC_TYPE_STR_MAX];
	char event_module[ABC_EVENT_STR_MAX];
	char event_name[ABC_EVENT_STR_MAX];
};

struct abc_pre_event {
	struct list_head node;
	unsigned int cur_time;
	int cnt;
	char abc_str[ABC_BUFFER_MAX];
};

struct abc_fault_info {
	unsigned int cur_time;
	int cur_cnt;
};

struct abc_event_buffer {
	int size;
	int rear;
	int front;
	int warn_cnt;
	int buffer_max;
	struct abc_fault_info *abc_element;
};

struct abc_common_spec_data {
	const char *module_name;
	const char *error_name;
	int enabled;
	int spec_cnt;
	int current_spec;
	//int spec_type;		In case a new spec type is added
};

struct spec_data_type1 {
	int *th_cnt;
	int *th_time;
	struct list_head node;
	struct abc_common_spec_data common_spec;
	struct abc_event_buffer buffer;
};

struct abc_platform_data {
	unsigned int nItem;
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	struct abc_motto_data *motto_data;
#endif
	struct list_head abc_spec_list;
};

struct abc_event_work {
	char abc_str[ABC_BUFFER_MAX];
	int cur_time;
	struct work_struct work;
};

struct abc_info {
	struct device *dev;
	struct workqueue_struct *workqueue;
	struct abc_event_work event_work_data[ABC_WORK_MAX];
	struct abc_event_work pre_event_work_data[ABC_WORK_MAX];
	struct completion enable_done;
	struct delayed_work clear_pre_events;
	struct work_struct save_pre_events;
	struct list_head pre_event_list;
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	struct completion test_work_done;
	struct completion test_uevent_done;
#endif
	struct abc_platform_data *pdata;
	struct mutex work_mutex;
	struct mutex spec_mutex;
	struct mutex pre_event_mutex;
	struct mutex enable_mutex;
	int pre_event_cnt;
	bool abc_enabled_flag;
};
void abc_common_test_get_log_str(char *log_str);
bool sec_abc_is_valid_abc_string(char *str);
extern void sec_abc_change_spec(const char *str);
extern int sec_abc_read_spec(char *str);
extern void sec_abc_send_event(char *str);
extern int sec_abc_get_enabled(void);
extern int sec_abc_wait_enabled(void);

#define ABC_PRINT(format, ...) pr_info("[sec_abc] " format, ##__VA_ARGS__)

#endif
