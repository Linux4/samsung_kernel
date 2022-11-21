/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_DEVICE_H_
#define __SHUB_DEVICE_H_

#include "../utility/shub_wait_event.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>

enum {
	RESET_TYPE_HUB_CRASHED = 0,
	RESET_TYPE_KERNEL_SYSFS,
	RESET_TYPE_KERNEL_NO_EVENT,
	RESET_TYPE_KERNEL_COM_FAIL,
	RESET_TYPE_HUB_NO_EVENT,
	RESET_TYPE_MAX,
};

struct reset_info_t {
	u64 timestamp;
	struct rtc_time time;
	int reason;
};

struct shub_data_t {
	struct platform_device *pdev;
	struct device *sysfs_dev;

	bool is_probe_done;
	bool is_working;

	int cnt_reset;
	int reset_type;
	unsigned int cnt_shub_reset[RESET_TYPE_MAX + 1]; /* index RESET_TYPE_MAX : total reset count */
	struct work_struct work_reset;
	struct shub_waitevent reset_lock;
	struct reset_info_t reset_info;

	u8 ap_status;
	u8 lcd_status;

	struct workqueue_struct *shub_wq;

	struct work_struct work_refresh;

	struct timer_list ts_sync_timer;
	struct work_struct work_ts_sync;
};

struct device *get_shub_device(void); // ssp_sensor sysfs 위한 device pdev랑 다름
struct shub_data_t *get_shub_data(void);

void reset_mcu(int reason);
int get_reset_type(void);
void init_reset_type(void);
int get_reset_count(void);
struct reset_info_t get_reset_info(void);

bool is_shub_working(void);
int shub_send_status(u8);
int queue_refresh_task(void);

int shub_probe(struct platform_device *pdev);
void shub_shutdown(struct platform_device *pdev);
int shub_suspend(struct device *dev);
void shub_resume(struct device *dev);

void shub_queue_work(struct work_struct *work);
#endif /* __SHUB_DEVICE_H_ */
