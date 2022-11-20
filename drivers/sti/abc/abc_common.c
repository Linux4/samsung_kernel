/* abc_common.c
 *
 * Abnormal Behavior Catcher Common Driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
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
#include <linux/sti/abc_common.h>
#include <linux/sti/abc_spec_manager.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <linux/sti/abc_kunit.h>
#endif
#define DEBUG_ABC
#define WARNING_REPORT

struct device *sec_abc;
int abc_enable_mode;
int abc_init;

struct abc_enable_cmd_struct enable_cmd_list[] = {
	{ERROR_REPORT_MODE_ENABLE, ERROR_REPORT_MODE_BIT, "ERROR_REPORT=1"},
	{ERROR_REPORT_MODE_DISABLE, ERROR_REPORT_MODE_BIT, "ERROR_REPORT=0"},
	{ALL_REPORT_MODE_ENABLE, ALL_REPORT_MODE_BIT, "ALL_REPORT=1"},
	{ALL_REPORT_MODE_DISABLE, ALL_REPORT_MODE_BIT, "ALL_REPORT=0"},
	{PRE_EVENT_ENABLE, PRE_EVENT_ENABLE_BIT, "PRE_EVENT=1"},
	{PRE_EVENT_DISABLE, PRE_EVENT_ENABLE_BIT, "PRE_EVENT=0"},
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_abc_dt_match[] = {
	{ .compatible = "samsung,sec_abc" },
	{ }
};
#endif

static int sec_abc_resume(struct device *dev)
{
	return 0;
}

static int sec_abc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct dev_pm_ops sec_abc_pm = {
	.resume = sec_abc_resume,
};

__visible_for_testing
bool sec_abc_is_enabled(void)
{
	return (abc_enable_mode & (ERROR_REPORT_MODE_BIT | ALL_REPORT_MODE_BIT)) ? true : false;
}

bool sec_abc_is_valid_abc_string(char *str)
{
	char *event_str[ABC_UEVENT_MAX];
	char temp[ABC_BUFFER_MAX];
	char *c, *p;
	int i, idx = 0;

	ABC_PRINT("%s : check validation : %s", __func__, str);

	strlcpy(temp, str, ABC_BUFFER_MAX);
	p = temp;

	while ((c = strsep(&p, "@")) != NULL) {

		if (idx >= ABC_UEVENT_MAX)
			return false;

		event_str[idx] = c;
		idx++;
		if (!*c)
			return false;
	}

	if (idx < 2)
		return false;

	if (strncmp(event_str[0], "MODULE=", 7) ||
	   (strncmp(event_str[1], "WARN=", 5) && strncmp(event_str[1], "INFO=", 5)))
		return false;

	for (i = 0; i < idx; i++) {

		c = strstr(event_str[i], "=");

		if (!c || !*(c + 1))
			return false;
	}

	ABC_PRINT("%s : %s is valid", __func__, str);
	return true;
}

void sec_abc_send_uevent(struct abc_key_data *key_data, unsigned int cur_time, char *uevent_type)
{
	char *uevent_str[ABC_UEVENT_MAX] = {0,};
	char uevent_module_str[ABC_EVENT_STR_MAX + 7];
	char uevent_event_str[ABC_EVENT_STR_MAX + ABC_TYPE_STR_MAX];
	char timestamp[TIME_STAMP_STR_MAX];
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
#endif

	snprintf(uevent_module_str, ABC_EVENT_STR_MAX, "MODULE=%s", key_data->event_module);
	snprintf(uevent_event_str, ABC_EVENT_STR_MAX, "%s=%s", uevent_type, key_data->event_name);
	snprintf(timestamp, TIME_STAMP_STR_MAX, "TIMESTAMP=%d", cur_time);

	uevent_str[0] = uevent_module_str;
	uevent_str[1] = uevent_event_str;
	uevent_str[2] = timestamp;
	uevent_str[3] = NULL;

#if IS_ENABLED(CONFIG_SEC_KUNIT)
	abc_common_test_get_work_str(uevent_str);
	complete(&pinfo->test_uevent_done);
#endif
	kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
}

__visible_for_testing
struct abc_pre_event *sec_abc_get_pre_event_node(char *str)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_pre_event *pre_event;

	list_for_each_entry(pre_event, &pinfo->pre_event_list, node) {
		if (!strcmp(pre_event->abc_str, str)) {
			ABC_PRINT("%s : return matched node.\n", __func__);
			return pre_event;
		}
	}

	pre_event = NULL;

	if (pinfo->pre_event_cnt < ABC_PREOCCURRED_EVENT_MAX) {

		pre_event = kzalloc(sizeof(*pre_event), GFP_KERNEL);

		if (pre_event) {
			list_add_tail(&pre_event->node, &pinfo->pre_event_list);
			strlcpy(pre_event->abc_str, str, ABC_BUFFER_MAX);
			pinfo->pre_event_cnt++;
			ABC_PRINT("%s : return new node.\n", __func__);
		} else
			ABC_PRINT("%s : failed to get node.\n", __func__);
	}

	return pre_event;
}

__visible_for_testing
int sec_abc_clear_pre_events(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_pre_event *pre_event;
	int cnt = 0;

	ABC_PRINT("%s : start", __func__);

	while (!list_empty(&pinfo->pre_event_list)) {
		pre_event = list_first_entry(&pinfo->pre_event_list,
										struct abc_pre_event,
										node);
		list_del(&pre_event->node);
		kfree(pre_event);
		cnt++;
	}
	pinfo->pre_event_cnt = 0;

	/* Once Pre_events were cleared, don't save pre_event anymore till pre_event is enabled again. */
	abc_enable_mode &= (~PRE_EVENT_ENABLE_BIT);

#if IS_ENABLED(CONFIG_SEC_KUNIT)
	complete(&pinfo->test_work_done);
#endif
	ABC_PRINT("%s : end", __func__);
	return cnt;
}

__visible_for_testing
int sec_abc_process_pre_events(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_pre_event *pre_event;
	struct abc_key_data key_data;
	int i, cnt = 0;

	ABC_PRINT("%s : start", __func__);

	list_for_each_entry(pre_event, &pinfo->pre_event_list, node) {
		sec_abc_make_key_data(&key_data, pre_event->abc_str);

		if (abc_enable_mode & ALL_REPORT_MODE_BIT) {
			ABC_PRINT("%s : All report mode. Send uevent.\n", __func__);
			sec_abc_send_uevent(&key_data, pre_event->cur_time, key_data.event_type);
		}

		if (sec_abc_reached_spec_pre(&key_data, pre_event)) {
			ABC_PRINT("%s : Pre_event spec out! Send uevent.\n", __func__);
			sec_abc_send_uevent(&key_data, pre_event->cur_time, "ERROR");
		} else {
			ABC_PRINT("%s : Pre_event spec in! Enqueue %d pre_events.\n", __func__, pre_event->cnt);
			for (i = 0; i < pre_event->cnt; i++)
				sec_abc_enqueue_event_data(&key_data, pre_event->cur_time);
		}
		cnt++;
	}

	ABC_PRINT("%s : end", __func__);
	return cnt;
}

__visible_for_testing
int sec_abc_save_pre_events(char *str, unsigned int cur_time)
{
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
#endif
	struct abc_pre_event *pre_event;
	int ret = 0;

	ABC_PRINT("%s : start %s\n", __func__, str);

	pre_event = sec_abc_get_pre_event_node(str);

	if (!pre_event) {
		ABC_PRINT("%s : Failed to add Pre_event : %s.\n", __func__, str);
		ret = -EINVAL;
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("Failed to add Pre_event.\n");
#endif
		goto save_pre_event_out;
	}

	pre_event->cur_time = cur_time;
	pre_event->cnt++;

save_pre_event_out:
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	complete(&pinfo->test_work_done);
#endif
	return ret;
}

__visible_for_testing
void sec_abc_process_changed_enable_mode(void)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct spec_data_type1 *spec_type1;

	if (sec_abc_is_enabled()) {
		if (abc_enable_mode & PRE_EVENT_ENABLE_BIT) {
			ABC_PRINT("%s : %d Pre_events processed.\n", __func__, sec_abc_process_pre_events());
#if IS_ENABLED(CONFIG_SEC_KUNIT)
			abc_common_test_get_log_str("Pre_events processed.\n");
#endif
		} else {
			ABC_PRINT("%s : ABC enabled. Pre_event disabled.\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
			abc_common_test_get_log_str("ABC enabled. Pre_event disabled.\n");
#endif
		}
		pinfo->abc_enabled_flag = true;
		complete(&pinfo->enable_done);
	} else {
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC is disabled. Clear events.\n");
#endif
		list_for_each_entry(spec_type1, &pinfo->pdata->abc_spec_list, node) {
			sec_abc_reset_buffer_type1(spec_type1);
		}
	}

	ABC_PRINT("%s : %d Pre_events cleared.\n", __func__, sec_abc_clear_pre_events());
}

/* Change ABC driver's enable mode.
 *
 *   Interface with ACT : write "1" or "0"
 *   Interfcae with LABO
 *   ex) "ERROR_REPORT=1,PRE_EVENT=1", "ALL_REPORT=1,ERROR_REPORT=0,PRE_EVENT=0" ...
 *
 */
__visible_for_testing
ssize_t enabled_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t count)
{
	char *c, *p, *enable_cmd[ABC_CMD_MAX];
	char temp[ABC_CMD_STR_MAX * 3];
	int items, i, j, idx = 0;
	int chk = 0;
	int origin_enable_mode = abc_enable_mode;

	if (!strncmp(buf, "1", 1)) {
		/* Interface with ACT */
		ABC_PRINT("Error report mode enabled.\n");
		abc_enable_mode |= ERROR_REPORT_MODE_BIT;
	} else if (!strncmp(buf, "0", 1)) {
		ABC_PRINT("Error report mode disabled.\n");
		abc_enable_mode &= (~ERROR_REPORT_MODE_BIT);
	} else {

		strlcpy(temp, buf, ABC_CMD_STR_MAX * 3);
		p = temp;
		items = ARRAY_SIZE(enable_cmd_list);

		while ((c = strsep(&p, ",")) != NULL && idx < ABC_CMD_MAX) {
			enable_cmd[idx] = c;
			idx++;
		}

		for (i = 0; i < idx; i++) {
			chk = 0;
			for (j = 0; j < items; j++) {
				if (!strncmp(enable_cmd[i],
					enable_cmd_list[j].abc_cmd_str,
					strlen(enable_cmd_list[j].abc_cmd_str))) {
					ABC_PRINT("%s : %s.\n", __func__, enable_cmd_list[j].abc_cmd_str);
					if (strstr(enable_cmd_list[j].abc_cmd_str, "=1"))
						abc_enable_mode |= enable_cmd_list[j].enable_value;
					else
						abc_enable_mode &= ~(enable_cmd_list[j].enable_value);
					chk = 1;
				}
			}
			if (!chk) {
				ABC_PRINT("%s : Invalid string. Check the Input : %s.\n", __func__, buf);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
				abc_common_test_get_log_str("Invalid string. Check the Input.\n");
#endif
				abc_enable_mode = origin_enable_mode;
				return count;
			}
		}
	}

	sec_abc_process_changed_enable_mode();
	return count;
}

__visible_for_testing
ssize_t enabled_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	if (sec_abc_is_enabled())
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}
static DEVICE_ATTR_RW(enabled);

__visible_for_testing
ssize_t spec_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	mutex_lock(&pinfo->spec_mutex);

	sec_abc_change_spec(buf);

	mutex_unlock(&pinfo->spec_mutex);

	return count;
}

__visible_for_testing
ssize_t spec_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	int count = 0;

	mutex_lock(&pinfo->spec_mutex);

	count = sec_abc_read_spec(buf);

	mutex_unlock(&pinfo->spec_mutex);

	return count;
}
static DEVICE_ATTR_RW(spec);

int sec_abc_get_enabled(void)
{
	return abc_enable_mode;
}
EXPORT_SYMBOL(sec_abc_get_enabled);

static void sec_abc_work_func_save_pre_events(struct work_struct *work)
{
	struct abc_event_work *pre_event_work_data;
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	ABC_PRINT("%s : start", __func__);
	mutex_lock(&pinfo->pre_event_mutex);

	pre_event_work_data = container_of(work, struct abc_event_work, work);
	sec_abc_save_pre_events(pre_event_work_data->abc_str, pre_event_work_data->cur_time);

	mutex_unlock(&pinfo->pre_event_mutex);
	ABC_PRINT("%s : end", __func__);
}

static void sec_abc_work_func_clear_pre_events(struct work_struct *work)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	ABC_PRINT("%s : start", __func__);
	mutex_lock(&pinfo->pre_event_mutex);

	sec_abc_clear_pre_events();

	mutex_unlock(&pinfo->pre_event_mutex);
	ABC_PRINT("%s : end", __func__);
}

static void sec_abc_work_func(struct work_struct *work)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_event_work *event_work_data;
	struct abc_key_data key_data;

	mutex_lock(&pinfo->work_mutex);

	event_work_data = container_of(work, struct abc_event_work, work);

	ABC_PRINT("%s : work start. str : %s\n", __func__, event_work_data->abc_str);

	sec_abc_make_key_data(&key_data, event_work_data->abc_str);

#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	motto_send_device_info(key_data.event_module, key_data.event_name);
#endif

	if (abc_enable_mode & ALL_REPORT_MODE_BIT) {
		ABC_PRINT("%s : All report mode enabled. Send uevent.\n", __func__);
		sec_abc_send_uevent(&key_data, event_work_data->cur_time, key_data.event_type);
	}

	if (!sec_abc_is_enabled_error(&key_data)) {
		ABC_PRINT("%s : event_name : %s isn't enabled.\n", __func__, key_data.event_name);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("Disabled event.\n");
#endif
		goto abc_work_end;
	}

	sec_abc_enqueue_event_data(&key_data, event_work_data->cur_time);

	if (sec_abc_reached_spec(&key_data, event_work_data->cur_time)) {
		sec_abc_send_uevent(&key_data, event_work_data->cur_time, "ERROR");
		ABC_PRINT("%s : Send uevent : %s.\n", __func__, event_work_data->abc_str);
		sec_abc_reset_event_buffer(&key_data);
	}

abc_work_end:
	ABC_PRINT("%s : work done.\n", __func__);
	mutex_unlock(&pinfo->work_mutex);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	complete(&pinfo->test_work_done);
#endif
}

/* event string format
 *
 * ex)
 *	 MODULE=tsp@WARN=power_status_mismatch
 *   MODULE=gpu@INFO=gpu_fault
 *	 MODULE=tsp@ERROR=power_status_mismatch@EXT_LOG=fw_ver(0108)
 *
 */
__visible_for_testing
void sec_abc_enqueue_work(struct abc_event_work work_data[], char *str)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	u64 ktime;
	int idx;

	/* Calculate current kernel time */
	ktime = local_clock();
	do_div(ktime, NSEC_PER_MSEC);

	for (idx = 0; idx < ABC_WORK_MAX; idx++) {
		if (!work_pending(&work_data[idx].work)) {
			ABC_PRINT("%s : Event %s use work[%d].\n", __func__, str, idx);
			strlcpy(work_data[idx].abc_str, str, ABC_BUFFER_MAX);
			queue_work(pinfo->workqueue, &work_data[idx].work);
			work_data[idx].cur_time = (int)ktime;
			return;
		}
	}

	ABC_PRINT("%s : Failed. All works are in queue.\n", __func__);
}

void sec_abc_send_event(char *str)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	if (!abc_init) {
		ABC_PRINT("%s : ABC driver is not initialized!(%s)\n", __func__, str);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC driver is not initialized!\n");
#endif
		return;
	}

	if (!sec_abc_is_valid_abc_string(str)) {
		ABC_PRINT("%s : Event string isn't valid. Check Input : %s\n", __func__, str);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("Event string isn't valid. Check Input!\n");
#endif
		return;
	}

	if (sec_abc_is_enabled()) {
		ABC_PRINT("%s : ABC is enabled. Queue work.(%s)\n", __func__, str);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC is enabled. Queue work.\n");
#endif
		sec_abc_enqueue_work(pinfo->event_work_data, str);
	} else if (abc_enable_mode & PRE_EVENT_ENABLE_BIT) {
		ABC_PRINT("%s : ABC is disabled. Queue pre_event_work.(%s)\n", __func__, str);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC is disabled. Queue pre_event_work.\n");
#endif
		sec_abc_enqueue_work(pinfo->pre_event_work_data, str);
	} else {
		ABC_PRINT("%s : ABC is disabled and pre_event is disabled.(%s)\n", __func__, str);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC is disabled and pre_event is disabled.\n");
#endif
	}
}
EXPORT_SYMBOL(sec_abc_send_event);

/**
 * sec_abc_wait_enable() - wait for abc enable done
 * Return : 0 for success, -1 for fail(timeout or abc not initialized)
 */
int sec_abc_wait_enabled(void)
{
	struct abc_info *pinfo;
	unsigned long timeout;

	if (!abc_init) {
		ABC_PRINT("%s : ABC driver is not initialized!\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("ABC driver is not initialized!\n");
#endif
		return -1;
	}

	if (sec_abc_is_enabled())
		return 0;

	pinfo = dev_get_drvdata(sec_abc);

	reinit_completion(&pinfo->enable_done);

	timeout = wait_for_completion_timeout(&pinfo->enable_done,
						  msecs_to_jiffies(ABC_WAIT_ENABLE_TIMEOUT));

	if (timeout == 0) {
		ABC_PRINT("%s : timeout!\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("timeout!\n");
#endif
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(sec_abc_wait_enabled);

static int sec_abc_probe(struct platform_device *pdev)
{
	struct abc_platform_data *pdata;
	struct abc_info *pinfo;
	int ret = 0;
	int idx = 0;

	ABC_PRINT("%s\n", __func__);

	abc_init = false;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
					 sizeof(struct abc_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate platform data\n");
			ret = -ENOMEM;
			goto out;
		}

		pdev->dev.platform_data = pdata;
		INIT_LIST_HEAD(&pdata->abc_spec_list);
		ret = abc_parse_dt(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to parse dt data\n");
			goto err_parse_dt;
		}

		pr_info("%s: parse dt done\n", __func__);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "There are no platform data\n");
		ret = -EINVAL;
		goto out;
	}

	pinfo = kzalloc(sizeof(*pinfo), GFP_KERNEL);

	if (!pinfo) {
		ret = -ENOMEM;
		goto err_alloc_pinfo;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	pinfo->dev = sec_device_create(pinfo, "sec_abc");
#else
	pinfo->dev = device_create(sec_class, NULL, 0, NULL, "sec_abc");
#endif
	if (IS_ERR(pinfo->dev)) {
		pr_err("%s Failed to create device(sec_abc)!\n", __func__);
		ret = -ENODEV;
		goto err_create_device;
	}
	sec_abc = pinfo->dev;

	ret = device_create_file(pinfo->dev, &dev_attr_enabled);
	if (ret) {
		pr_err("%s: Failed to create device enabled file\n", __func__);
		goto err_create_abc_enable_mode_sysfs;
	}

	ret = device_create_file(pinfo->dev, &dev_attr_spec);
	if (ret) {
		pr_err("%s: Failed to create device error_spec file\n", __func__);
		goto err_create_abc_error_spec_sysfs;
	}

	INIT_LIST_HEAD(&pinfo->pre_event_list);
	pinfo->workqueue = create_singlethread_workqueue("sec_abc_wq");

	INIT_DELAYED_WORK(&pinfo->clear_pre_events, sec_abc_work_func_clear_pre_events);

	/* After timeout clear abc events occurred when abc disalbed. */
	queue_delayed_work(pinfo->workqueue,
					   &pinfo->clear_pre_events,
					   msecs_to_jiffies(ABC_CLEAR_EVENT_TIMEOUT));

	/* Work for abc_events & pre_events (events occurred before enabled) */
	for (idx = 0; idx < ABC_WORK_MAX; idx++) {
		INIT_WORK(&pinfo->event_work_data[idx].work, sec_abc_work_func);
		INIT_WORK(&pinfo->pre_event_work_data[idx].work, sec_abc_work_func_save_pre_events);
	}

	if (!pinfo->workqueue)
		goto err_create_abc_wq;

#if IS_ENABLED(CONFIG_SEC_KUNIT)
	init_completion(&pinfo->test_uevent_done);
	init_completion(&pinfo->test_work_done);
#endif
	init_completion(&pinfo->enable_done);
	mutex_init(&pinfo->pre_event_mutex);
	mutex_init(&pinfo->enable_mutex);
	mutex_init(&pinfo->work_mutex);
	mutex_init(&pinfo->spec_mutex);
	pinfo->pdata = pdata;
	platform_set_drvdata(pdev, pinfo);
#if IS_ENABLED(CONFIG_SEC_ABC_MOTTO)
	motto_init(pdev);
#endif
	abc_init = true;
	abc_enable_mode |= PRE_EVENT_ENABLE_BIT;
	ABC_PRINT("%s success\n", __func__);
	return ret;
err_create_abc_wq:
	device_remove_file(pinfo->dev, &dev_attr_spec);
err_create_abc_error_spec_sysfs:
	device_remove_file(pinfo->dev, &dev_attr_enabled);
err_create_abc_enable_mode_sysfs:
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(sec_abc->devt);
#else
	device_destroy(sec_class, sec_abc->devt);
#endif
err_create_device:
	kfree(pinfo);
err_alloc_pinfo:
err_parse_dt:
	devm_kfree(&pdev->dev, pdata);
	pdev->dev.platform_data = NULL;
out:
	return ret;
}

__visible_for_testing
void sec_abc_free_allocated_memory(void)
{
	struct abc_pre_event *pre_event;
	struct spec_data_type1 *spec_buf;
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);

	while (!list_empty(&pinfo->pre_event_list)) {

		pre_event = list_first_entry(
							 &pinfo->pre_event_list,
							 struct abc_pre_event,
							 node);

		list_del(&pre_event->node);
		kfree(pre_event);
	}

	list_for_each_entry(spec_buf, &pinfo->pdata->abc_spec_list, node) {
		kfree(spec_buf->buffer.abc_element);
		spec_buf->buffer.abc_element = NULL;
	}
}
static struct platform_driver sec_abc_driver = {
	.probe = sec_abc_probe,
	.remove = sec_abc_remove,
	.driver = {
		.name = "sec_abc",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &sec_abc_pm,
#endif
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_abc_dt_match),
#endif
	},
};

#ifdef CONFIG_SEC_KUNIT
kunit_notifier_chain_init(abc_common_test_module);
kunit_notifier_chain_init(abc_spec_type1_test_module);
#endif

static int __init sec_abc_init(void)
{
	ABC_PRINT("%s\n", __func__);

#ifdef CONFIG_SEC_KUNIT
	kunit_notifier_chain_register(abc_common_test_module);
	kunit_notifier_chain_register(abc_spec_type1_test_module);
#endif

	return platform_driver_register(&sec_abc_driver);
}

static void __exit sec_abc_exit(void)
{
	ABC_PRINT("%s\n", __func__);
	sec_abc_free_allocated_memory();

#ifdef CONFIG_SEC_KUNIT
	kunit_notifier_chain_unregister(abc_common_test_module);
	kunit_notifier_chain_unregister(abc_spec_type1_test_module);
#endif

	return platform_driver_unregister(&sec_abc_driver);
}

module_init(sec_abc_init);
module_exit(sec_abc_exit);

MODULE_DESCRIPTION("Samsung ABC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
