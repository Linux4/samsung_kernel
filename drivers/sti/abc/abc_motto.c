/* abc_motto.c
 *
 * Abnormal Behavior Catcher MOTTO Support
 *
 * Copyright (C) 2020 Samsung Electronics
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

extern struct device *sec_abc;

struct motto_event_type motto_event_type_list[] ={
	{MOTTO_MODULE_GPU, "gpu_fault"},
	{MOTTO_MODULE_DECON, "fence_timeout"},
	{MOTTO_MODULE_CAMERA, "camera_error"},
	{MOTTO_MODULE_CAMERA, "i2c_fail"},
	// we can add more like
	// {MOTTO_MODULE_GPU, "gpu_new_event"}
	// {MOTTO_MODULE_CAMERA, "camera_new_event"},
	// {MOTTO_MODULE_NEW, "new_something"}
};

static void motto_update_event(enum motto_event_module module)
{
	struct abc_info *pinfo;
	struct abc_motto_data *cmotto;

	pinfo = dev_get_drvdata(sec_abc);
	cmotto = pinfo->pdata->motto_data;

	if(cmotto->dev_err_count[module] == 0xff) // 0xff means max value of 8bit variable dev_err_count[module]
		return;
	
	cmotto->dev_err_count[module]++;
	
	return;
}

static enum motto_event_module motto_event_to_idx(char *event)
{
	int i, items;

	items = sizeof(motto_event_type_list) / sizeof(motto_event_type_list[0]);

	for (i = 0; i < items; i++) {
		if (!strcmp(motto_event_type_list[i].motto_event_type_str, event))
			return motto_event_type_list[i].module;
	}
	return MOTTO_MODULE_NONE;
}
void get_motto_uevent_str(char *uevent_str)
{
	struct abc_info *pinfo;
	struct abc_motto_data *cmotto;

	pinfo = dev_get_drvdata(sec_abc);
	cmotto = pinfo->pdata->motto_data;

	sprintf(uevent_str, "AME=%08x%08x%08x%08x", cmotto->boot_time, cmotto->dev_err_count[0],
			cmotto->dev_err_count[1], cmotto->dev_err_count[2]);
	return;
}
void motto_send_uevent(void)
{
	char temp[ABC_BUFFER_MAX];
	char *uevent_str[3] = {0,};

	get_motto_uevent_str(temp);
	
	uevent_str[0] = temp;
	//uevent_str[1] = &timestamp[0];
	uevent_str[2] = NULL;

	kobject_uevent_env(&sec_abc->kobj, KOBJ_CHANGE, uevent_str);
}
void motto_send_device_info(char *event_type)
{
	enum motto_event_module module = motto_event_to_idx(event_type);

	if (module > MOTTO_MODULE_NONE) {
		ABC_PRINT("%s : %s\n", __func__, event_type);
		motto_update_event(module);
		
		motto_send_uevent();
	}
}
static ssize_t show_abc_motto_info(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct abc_motto_data *cmotto;
	struct abc_info *pinfo = dev_get_drvdata(dev);
	char temp[ABC_BUFFER_MAX];

	get_motto_uevent_str(temp);
	
	cmotto = pinfo->pdata->motto_data;
	ABC_PRINT("%s\n", temp);
	ABC_PRINT("boot 0 : %d\n", cmotto->boot_time);
	ABC_PRINT("dev 0 %d , dev 1 %d, dev 2 %d\n",
			cmotto->dev_err_count[0],cmotto->dev_err_count[1], cmotto->dev_err_count[2]);

	return sprintf(buf, "%d boot %d, dev 0 %d , dev 1 %d, dev 2 %d\n",
				sec_abc_get_enabled(), cmotto->boot_time,	cmotto->dev_err_count[0],
				cmotto->dev_err_count[1], cmotto->dev_err_count[2]);
}
static DEVICE_ATTR(motto_info, 0444, show_abc_motto_info, NULL);

void motto_init(struct platform_device *pdev)
{
	struct abc_info *pinfo;
	int ret = 0;

	pinfo = dev_get_drvdata(sec_abc);

	ret = device_create_file(pinfo->dev, &dev_attr_motto_info);
	if (ret) {
		pr_err("%s: Failed to create device motto_info file\n", __func__);
		goto err_create_abc_motto_info_sysfs;
	}

	pinfo->pdata->motto_data = devm_kzalloc(&pdev->dev,
					 sizeof(struct abc_motto_data), GFP_KERNEL);
	if (pinfo->pdata->motto_data == NULL)
		goto err_alloc_motto_data;

	return;

err_alloc_motto_data:
	device_remove_file(pinfo->dev, &dev_attr_motto_info);
err_create_abc_motto_info_sysfs:
	return;
}

void motto_send_bootcheck_info(int boot_time)
{
	struct abc_info *pinfo;
	struct abc_motto_data *cmotto;

	ABC_PRINT("%s : %d\n", __func__, boot_time);
	pinfo = dev_get_drvdata(sec_abc);
	cmotto = pinfo->pdata->motto_data;
	cmotto->boot_time = (boot_time >= 0xFF) ? 0xFF : (u32)boot_time;

	motto_send_uevent();
}
EXPORT_SYMBOL(motto_send_bootcheck_info);

MODULE_DESCRIPTION("Samsung ABC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
