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

#ifndef CONFIG_SHUB_MODULE
#include <linux/module.h>
#endif

#include "../comm/misc/shub_misc.h"
#include "../comm/shub_comm.h"
#include "../comm/shub_iio.h"
#include "../debug/shub_debug.h"
#include "../debug/shub_system_checker.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_sysfs.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_wait_event.h"
#include "../utility/shub_wakelock.h"
#include "../utility/shub_file_manager.h"
#include "../vendor/shub_vendor.h"
#include "../others/shub_motor_callback.h"
#ifdef CONFIG_SHUB_OIS
#include "../others/shub_ois.h"
#endif
#include "../factory/shub_factory.h"
#include "shub_device.h"
#include "shub_sysfs.h"
#include "../sensor/light.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "shub_firmware.h"

#ifdef CONFIG_SHUB_DUMP
#include "../debug/shub_dump.h"
#endif

struct shub_system_info {
	uint32_t fw_version;
	uint64_t scan[3];
	uint32_t support_ddi;
	uint32_t reserved_1;
	uint32_t reserved_2;
} __attribute__((__packed__));

static struct shub_data_t *shub_data;

#define SHUB_TIMESTAMP_SYNC_TIMER_SEC (10 * HZ)

struct shub_data_t *get_shub_data(void)
{
	return shub_data;
}

struct device *get_shub_device(void)
{
	return &shub_data->pdev->dev;
}

static void timestamp_sync_work_func(struct work_struct *work)
{
	int ret;

	ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, RTC_TIME, NULL, 0);
	if (ret < 0)
		shub_errf("comm fail %d", ret);
}

static void timestamp_sync_timer_func(struct timer_list *timer)
{
	struct shub_data_t *data = container_of(timer, struct shub_data_t, ts_sync_timer);

	queue_work(data->shub_wq, &shub_data->work_ts_sync);
	mod_timer(&shub_data->ts_sync_timer, round_jiffies_up(jiffies + SHUB_TIMESTAMP_SYNC_TIMER_SEC));
}

static void enable_timestamp_sync_timer(void)
{
	mod_timer(&shub_data->ts_sync_timer, round_jiffies_up(jiffies + SHUB_TIMESTAMP_SYNC_TIMER_SEC));
}

static void disable_timestamp_sync_timer(void)
{
	del_timer_sync(&shub_data->ts_sync_timer);
	cancel_work_sync(&shub_data->work_ts_sync);
}

static int initialize_timestamp_sync_timer(void)
{
	timer_setup(&shub_data->ts_sync_timer, timestamp_sync_timer_func, 0);

	INIT_WORK(&shub_data->work_ts_sync, timestamp_sync_work_func);
	return 0;
}

static int get_shub_system_info(void)
{
	int ret = 0;
	char *buffer = NULL;
	unsigned int buffer_length;
	struct shub_system_info system_info;

	ret = shub_send_command_wait(CMD_GETVALUE, TYPE_MCU, HUB_SYSTEM_INFO, 1000, NULL, 0, &buffer, &buffer_length,
				     true);

	if (ret < 0) {
		shub_errf("fail %d", ret);
		return ret;
	}

	if (buffer_length != sizeof(system_info)) {
		shub_errf("buffer length error %d", buffer_length);
		return -EINVAL;
	}

	memcpy(&system_info, buffer, sizeof(system_info));
	kfree(buffer);

	set_firmware_rev(system_info.fw_version);
	set_sensor_probe_state(system_info.scan);
	set_light_ddi_support(system_info.support_ddi);

	return ret;
}

static int init_sensorhub(void)
{
	int ret = 0;

	ret = get_shub_system_info();
	if (ret < 0)
		return ret;

	shub_send_status(shub_data->ap_status);
	shub_send_status(shub_data->lcd_status);

	return ret;
}

void init_others(void)
{
	sync_motor_state();
#ifdef CONFIG_SHUB_OIS
	notify_ois_reset();
#endif
}

struct reset_info_t get_reset_info(void)
{
	return shub_data->reset_info;
}

void save_reset_info(void)
{
	get_tm(&(shub_data->reset_info.time));
	shub_data->reset_info.timestamp = get_current_timestamp();
	if (shub_data->reset_type < RESET_TYPE_MAX)
		shub_data->reset_info.reason = shub_data->reset_type;
	else
		shub_data->reset_info.reason = RESET_TYPE_HUB_CRASHED;
	shub_data->reset_type = RESET_TYPE_MAX;
}

static void refresh_task(struct work_struct *work)
{
	if (!is_shub_working()) {
		shub_errf("shub is not working");
		return;
	}
	shub_infof();

	shub_wake_lock();
	shub_data->cnt_reset++;
	save_reset_info();

	if (sensorhub_refresh_func() < 0)
		goto exit;

	if (init_sensorhub() < 0)
		goto exit;

	if (refresh_sensors(&shub_data->pdev->dev)  < 0)
		goto exit;

	if (shub_data->cnt_reset == 0) {
		initialize_factory();
#ifdef CONFIG_SHUB_DUMP
		initialize_shub_dump();
#endif
	}

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		goto exit;
#endif
	init_others();
	enable_timestamp_sync_timer();
exit:
	shub_wake_up_wait_event(&shub_data->reset_lock);
	shub_wake_unlock();

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		system_ready_cb();
#endif
}

int queue_refresh_task(void)
{
	cancel_work_sync(&shub_data->work_refresh);

	shub_infof();
	queue_work(shub_data->shub_wq, &shub_data->work_refresh);

	return 0;
}

int shub_send_status(u8 state_sub_cmd)
{
	int ret;

	ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, state_sub_cmd, NULL, 0);
	if (ret < 0)
		shub_errf("command %d failed", state_sub_cmd);
	else
		shub_infof("command %d", state_sub_cmd);

	return ret;
}

bool is_shub_working(void)
{
	return shub_data->is_probe_done && sensorhub_is_working() && !work_busy(&shub_data->work_reset);
}

int get_reset_count(void)
{
	return shub_data->cnt_reset;
}

static void reset_task(struct work_struct *work)
{
	int ret;

	shub_infof("");
	disable_timestamp_sync_timer();
	ret = sensorhub_reset();
	if (ret < 0) {
		shub_errf("reset failed");
		shub_wake_up_wait_event(&shub_data->reset_lock);
	}
}

void reset_mcu(int reason)
{
	if (work_busy(&shub_data->work_reset)) {
		shub_infof("reset work state : pending or running");
		return;
	}

	shub_infof("- reason(%u)", reason);
	shub_data->reset_type = reason;

	shub_data->cnt_shub_reset[RESET_TYPE_MAX]++;
	if (shub_data->reset_type < RESET_TYPE_MAX)
		shub_data->cnt_shub_reset[shub_data->reset_type]++;

	shub_lock_wait_event(&shub_data->reset_lock);
	queue_work(shub_data->shub_wq, &shub_data->work_reset);
}

int init_sensorhub_device(void)
{
	int ret;

#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
	ret = initialize_shub_firmware();
	if (ret < 0) {
		shub_errf("init firmware failed");
		return ret;
	}
#endif

	shub_data->is_probe_done = false;
	shub_data->is_working = false;

	/* Reset */
	shub_data->cnt_reset = -1;
	{
		int type;

		for (type = 0; type <= RESET_TYPE_MAX; type++)
			shub_data->cnt_shub_reset[type] = 0;
	}

	shub_data->ap_status = AP_RESUME;
	shub_data->lcd_status = LCD_OFF;

	shub_data->shub_wq = create_singlethread_workqueue("shub_dev_wq");
	if (!shub_data->shub_wq)
		return -ENOMEM;

	INIT_WORK(&shub_data->work_refresh, refresh_task);
	INIT_WORK(&shub_data->work_reset, reset_task);
	init_waitqueue_head(&shub_data->reset_lock.waitqueue);
	atomic_set(&shub_data->reset_lock.state, 1);

	ret = initialize_timestamp_sync_timer();
	if (ret < 0) {
		shub_errf("could not create ts_sync workqueue");
		return ret;
	}
	shub_wake_lock_register();

	return 0;
}

void exit_sensorhub_device(void)
{
	shub_wake_lock_unregister();
	disable_timestamp_sync_timer();

	cancel_work_sync(&shub_data->work_refresh);
	cancel_work_sync(&shub_data->work_reset);

	del_timer(&shub_data->ts_sync_timer);
	cancel_work_sync(&shub_data->work_ts_sync);
	destroy_workqueue(shub_data->shub_wq);
#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
	remove_shub_firmware();
#endif
}

int shub_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	shub_infof();

	shub_data = kzalloc(sizeof(struct shub_data_t), GFP_KERNEL);
	shub_data->pdev = pdev;

	if (!dev->of_node) {
		shub_errf("failed to get device node");
		return -ENODEV;
	}

	if (init_sensor_manager(dev) < 0) {
		shub_errf("failed to init sensor manager");
		goto err_init_sensor_manager;
	}

	if (init_sensorhub_device() < 0) {
		shub_errf("failed to init sensorhub device");
		goto err_init_device;
	}

	if (init_shub_debug() < 0) {
		shub_errf("failed to init debug");
		goto err_init_debug;
	}

	if (init_comm_to_hub() < 0) {
		shub_errf("failed to init comm");
		goto err_init_comm;
	}

	if (initialize_shub_misc_dev() < 0) {
		shub_errf("failed to init misc device");
		goto err_init_misc_dev;
	}

	if (init_shub_sysfs() < 0) {
		shub_errf("failed to init shub sysfs");
		goto err_init_shub_sysfs;
	}

	if (init_shub_sensor_sysfs() < 0) {
		shub_errf("failed to init sensor sysfs");
		goto err_init_sensor_sysfs;
	}

	if (init_shub_debug_sysfs() < 0) {
		shub_errf("failed to init debug sysfs");
		goto err_init_debug_sysfs;
	}

	if (init_file_manager() < 0) {
		shub_errf("failed to init file_manager");
		goto err_init_file_manager;
	}

#ifdef CONFIG_SHUB_DEBUG
	shub_system_checker_init();
#endif
	enable_debug_timer();

	shub_data->is_probe_done = true;

	sensorhub_probe();

	dev_set_drvdata(dev, shub_data);
	shub_infof("probe success");

	return ret;

err_init_file_manager:
	remove_shub_debug_sysfs();
err_init_debug_sysfs:
	remove_shub_sysfs();
err_init_sensor_sysfs:
	remove_shub_misc_dev();
err_init_shub_sysfs:
	remove_shub_sensor_sysfs();
err_init_misc_dev:
	exit_comm_to_hub();
err_init_comm:
	exit_shub_debug();
err_init_debug:
	exit_sensorhub_device();
err_init_device:
	exit_sensor_manager(&pdev->dev);
err_init_sensor_manager:
	shub_errf("probe fail");
	return ret;
}

void shub_shutdown(struct platform_device *pdev)
{
	shub_infof();
	if (shub_data->is_probe_done == false)
		return;

	shub_data->is_probe_done = false;

	sensorhub_shutdown();
#ifdef CONFIG_SHUB_DUMP
	remove_shub_dump();
#endif
	remove_shub_motor_callback();
	remove_factory();
	remove_indio_dev();
	remove_file_manager();
	remove_shub_debug_sysfs();
	remove_shub_sysfs();
	remove_shub_sensor_sysfs();
	remove_shub_misc_dev();
	exit_comm_to_hub();
	exit_shub_debug();
	exit_sensorhub_device();
	exit_sensor_manager(&pdev->dev);
	shub_infof("done");
}

int shub_suspend(struct device *dev)
{
	shub_infof();

	disable_debug_timer();
	disable_timestamp_sync_timer();

	shub_data->ap_status = AP_SUSPEND;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	shub_send_status(AP_SUSPEND);
#endif
	return 0;
}

void shub_resume(struct device *dev)
{
	shub_infof();

	enable_debug_timer();
	enable_timestamp_sync_timer();

	shub_data->ap_status = AP_RESUME;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	shub_send_status(AP_RESUME);
#endif
}

void shub_queue_work(struct work_struct *work)
{
	queue_work(shub_data->shub_wq, work);
}
