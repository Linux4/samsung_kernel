/*
 * Samsung Mobile VE Group.
 *
 * drivers/video/sec_dct.c
 *
 * Drivers for samsung display clock tunning.
 *
 * Copyright (C) 2014, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */


#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <linux/sec_dct.h>

struct sec_dct_info_t *sec_dct_info;

static ssize_t dct_data_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_state_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_state_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_log_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_enabled_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_interface_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_libname_show(
	struct device *dev, struct device_attribute *attr, char *buf);

static DEVICE_ATTR(data, 0220 , NULL, dct_data_store);
static DEVICE_ATTR(state, 0440 , dct_state_show, dct_state_store);
static DEVICE_ATTR(log, 0440 , dct_log_show, NULL);
static DEVICE_ATTR(enabled, 0220, NULL, dct_enabled_store);
static DEVICE_ATTR(interface, 0440, dct_interface_show, NULL);
static DEVICE_ATTR(libname, 0440, dct_libname_show, NULL);

static struct attribute *sec_dct_attributes[] = {
	&dev_attr_data.attr,
	&dev_attr_state.attr,
	&dev_attr_log.attr,
	&dev_attr_enabled.attr,
	&dev_attr_interface.attr,
	&dev_attr_libname.attr,
	NULL
};

static struct attribute_group sec_dct_attr_group = {
	.attrs = sec_dct_attributes
};

static struct sec_dct_typeinfo_t dct_tinfo[DCT_TYPE_MAX] = {
	//DCT_TYPE_U8
	{	.name = "u8",
		//.format = "%hhu",
		.size = sizeof(u8) },
	//DCT_TYPE_U16
	{	.name = "u16",
		//.format = "%hu",
		.size = sizeof(u16)	},
	//DCT_TYPE_U32	
	{	.name = "u32",
		//.format = "%u",
		.size = sizeof(u32)	},
	//DCT_TYPE_U64
	{	.name = "u64",
		//.format = "%lu",
		.size = sizeof(u64)	},
};

static struct sec_dct_ctypeinfo_t dct_ctinfo[CTYPE_MAX] = {
	{	//.name = "unsigned_char",
		.format = "%hhu",
		.size = sizeof(unsigned char) },
	{	//.name = "unsigned_short",
		.format = "%hu",
		.size = sizeof(unsigned short)	},
	{	//.name = "unsigned_int",
		.format = "%u",
		.size = sizeof(unsigned int)	},
	{	//.name = "unsigned_long",
		.format = "%lu",
		.size = sizeof(unsigned long)	},
	{	//.name = "unsigned_long long",
		.format = "%llu",
		.size = sizeof(unsigned long long)	}
};

char *dct_recv_str;
//extern struct sec_dct_info_t dct_info;

static void dct_save_log(char *log, bool berror)
{
	if (sec_dct_info) {
		if (sec_dct_info->log == NULL)
			sec_dct_info->log = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!sec_dct_info->log)
			pr_err("[DCT][%s] Malloc of dct log buffer is failed!\n", __func__);
		else
			strcat(sec_dct_info->log, log);
		if (berror)
			pr_err("[DCT][%s] %s", __func__, log);
		else
			pr_info("[DCT][%s] %s", __func__, log);
	}
	return;
}

int check_type_info(char *tmp_log)
{
	int i, j;
	int ret = 0;
	bool bfind;
	static bool bcheck_type = false;

	if (!bcheck_type) {
		for (i = 0; i < DCT_TYPE_MAX; i++) {
			bfind = false;
			for (j = 0; j < CTYPE_MAX; j++) {
				if (dct_tinfo[i].size == dct_ctinfo[j].size) {
					dct_tinfo[i].format = dct_ctinfo[j].format;
					bfind =true;
					break;
				}
			}
			if (!bfind) {
				sprintf(tmp_log, "This system value type is stange!\n%s type is not exist!\n", dct_tinfo[i].name);
				dct_save_log(tmp_log, true);
				ret = EINVAL;
			}
		}
	}
	return ret;
}

// error return value
// recv data error : -ENOMSG or -ERANGE
// kernel data or processing error : -EINVAL
int dct_setData(char *name, int type, void *data, int size)
{
	int ret = 0;
	char *sep = "\n";
	char *sep2 = ",";
	char temp_text[30] = "%s is set to ";
	char *line = NULL;

	char *read_name;
	char *read_type;
	char *read_value;
	char *tmp_log = NULL;
	u64 temp_value;

	DCT_LOG("[DCT][%s] ++\n", __func__);

	if ((tmp_log = kmalloc(PAGE_SIZE, GFP_KERNEL)) == NULL) {
		pr_err("[DCT][%s] Malloc of tmp_log is fail!\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	if((ret = check_type_info(tmp_log))) {
		goto error;
	}
	
	line = strsep(&dct_recv_str, sep);
	if(line == NULL) {
		sprintf(tmp_log, "requested line is null!\nRequest : %s %s\n", name, dct_tinfo[type].name);
		dct_save_log(tmp_log, true);
		ret = -ENOMSG;
		goto error;
	}

	if (type < 0 || type >= DCT_TYPE_MAX) {
		sprintf(tmp_log, "[%s] request type error(%d)\n", __func__, type);
		dct_save_log(tmp_log, true);
		ret = -EINVAL;
		goto error;
	}
		
	if (size != dct_tinfo[type].size) {
		sprintf(tmp_log, "size error\nrequest name : %s\n(request : %s(%d), target : %d)\nCheck the kernel code",
				name, dct_tinfo[type].name, dct_tinfo[type].size, size);
		dct_save_log(tmp_log, true);
		ret = -EINVAL;
		goto error;
	}

	if (line[strlen(line)-1] == '\r')
		line[strlen(line)-1] = 0;

	if((read_name = strsep(&line, sep2)) == NULL)
	{
		sprintf(tmp_log, "Read data name is NULL\n");
		dct_save_log(tmp_log, true);		
		ret = -ENOMSG;
		goto error;
	}
	if((read_type = strsep(&line, sep2)) == NULL)
	{
		sprintf(tmp_log, "Read data type is NULL\n");
		dct_save_log(tmp_log, true);
		ret = -ENOMSG;
		goto error;
	}
	if((read_value = strsep(&line, sep2)) == NULL)
	{
		sprintf(tmp_log, "Read data name is NULL\n");
		dct_save_log(tmp_log, true);
		ret = -ENOMSG;
		goto error;
	}

	if(strcmp(read_name, name) || strcmp(read_type, dct_tinfo[type].name)) {
		sprintf(tmp_log, "data is wrong!\n");
		sprintf(tmp_log, "%s\tname : Request - %s , Read - %s\n", tmp_log, name, read_name);
		sprintf(tmp_log, "%s\ttype : Request - %s , Read - %s\n", tmp_log, dct_tinfo[type].name, read_type);
		dct_save_log(tmp_log, true);
		ret = -ENOMSG;
		goto error;
	}

	if (read_value[0] == '-') {
		sprintf(tmp_log, "Read value is negative\n(%s,%s,%s)\n", read_name, read_type, read_value);
		dct_save_log(tmp_log, true);
		ret = -ENOMSG;
		goto error;
	}

	ret = kstrtou64(read_value, 10, &temp_value);
	if (ret) {
		//int i=0;
		sprintf(tmp_log, "Data value translation error!\n(%s,%s,%s)\nerrno = %d\n", 
			read_name, read_type, read_value, ret);
		//sprintf(tmp_log, "%s\tread_value len = %d\n", 
		//	tmp_log, strlen(read_value));
		//for(i = 0; i < strlen(read_value); i++) {
		//	sprintf(tmp_log, "%s%hhd ", tmp_log, read_value[i]);	
		//}
		//strcat(tmp_log, "\n");
		ret = -ERANGE;		// unify result value
		dct_save_log(tmp_log, true);
		goto error;
	}

	
	strcat(temp_text, dct_tinfo[type].format);
	strcat(temp_text, "\n");
	switch (type) {
	case DCT_TYPE_U8:
		if (temp_value != (unsigned long long)(u8)temp_value) {
			sprintf(tmp_log, "Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffLL);
			dct_save_log(tmp_log, true);
			ret = -ERANGE;
			goto error; 	
		}
		*(u8 *)data = (u8)temp_value;

		sprintf(tmp_log, temp_text, name, *(u8 *)data);
		dct_save_log(tmp_log, false);
		break;
	case DCT_TYPE_U16:
		if (temp_value != (unsigned long long)(u16)temp_value) {
			sprintf(tmp_log, "Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffLL);
			dct_save_log(tmp_log, true);
			ret = -ERANGE;
			goto error; 	
		}
		*(u16 *)data = (u16)temp_value;

		sprintf(tmp_log, temp_text, name, *(u16 *)data);
		dct_save_log(tmp_log, false);
		break;
	case DCT_TYPE_U32:
		if (temp_value != (unsigned long long)(u32)temp_value) {
			sprintf(tmp_log, "Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffffffLL);
			dct_save_log(tmp_log, true);
			ret = -ERANGE;
			goto error; 	
		}
		*(u32 *)data = (u32)temp_value;

		sprintf(tmp_log, temp_text, name, *(u32 *)data);
		dct_save_log(tmp_log, false);
		break;
	case DCT_TYPE_U64:
		if (temp_value != (unsigned long long)(u64)temp_value) {
			sprintf(tmp_log, "Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffffffffffffffLL);
			dct_save_log(tmp_log, true);
			ret = -ERANGE;
			goto error; 	
		}
		*(u64 *)data = (u64)temp_value;

		sprintf(tmp_log, temp_text, name, *(u64 *)data);
		dct_save_log(tmp_log, false);
		break;
	
	}

error:
	if (tmp_log)
		kfree(tmp_log);

	DCT_LOG("[DCT][%s] -- (%d)\n", __func__, ret);

	return ret;
}

static ssize_t dct_data_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *tofree = NULL;
	struct sec_dct_info_t *info = dev_get_drvdata(dev);
	
	DCT_LOG("\n[DCT][%s] ++\n", __func__);

	tofree = dct_recv_str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (tofree == NULL) {
		pr_err("[DCT][%s] Malloc of dct_recv_str is fail!\n", __func__);
		return -ENOMEM;
	}
	strcpy(tofree, buf);

	pr_info("[DCT][%s][LOW DATA]\n%s\n", __func__, tofree);

	if ((ret = info->set_allData(tofree)) == 0) {
		info->state = DCT_STATE_DATA;
	}
	else {
		if (ret == -EINVAL)
			info->state = DCT_STATE_ERROR_KERNEL;
		else if (ret == -ENOMSG || ret == -ERANGE)
			info->state = DCT_STATE_ERROR_DATA;
		else
			info->state = DCT_STATE_ERROR_KERNEL;
		size = ret;
	}

	kfree(tofree);

	DCT_LOG("[DCT][%s] --(%d)\n\n", __func__, ret);

	return size;
}


static ssize_t dct_state_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	DCT_LOG("[DCT][%s] +- (%d)\n", __func__, info->state);
	switch (info->state) {
	case DCT_STATE_NODATA:
		strcpy(buf, "NODATA");
		break;
	case DCT_STATE_DATA:
		strcpy(buf, "DATA");
		break;
	case DCT_STATE_APPLYED:
		strcpy(buf, "APPLYED");
		info->state = DCT_STATE_NODATA;
		break;
	case DCT_STATE_ERROR_KERNEL:
		strcpy(buf, "ERROR_KERNEL");
		info->state = DCT_STATE_NODATA;	
		break;
	case DCT_STATE_ERROR_DATA:
		strcpy(buf, "ERROR_DATA");
		info->state = DCT_STATE_NODATA;
		break;
	default:
		pr_err("[DCT][%s] unknown state(%d)\n", __func__, info->state);
		
	}

	return strlen(buf);
}

static ssize_t dct_state_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	if (!strcmp(buf, "NODATA") || !strcmp(buf, "NODATA\n") || !strcmp(buf, "NODATA\r\n"))
		info->state = DCT_STATE_NODATA;
	else if (!strcmp(buf, "DATA") || !strcmp(buf, "DATA\n") || !strcmp(buf, "DATA\r\n"))
		info->state = DCT_STATE_DATA;
	else if (!strcmp(buf, "APPLYED") || !strcmp(buf, "APPLYED\n") || !strcmp(buf, "APPLYED\r\n"))
		info->state = DCT_STATE_APPLYED;
	else if (!strcmp(buf, "ERROR_KERNEL") || !strcmp(buf, "ERROR_KERNEL\n") || !strcmp(buf, "ERROR_KERNEL\r\n"))
		info->state = DCT_STATE_ERROR_KERNEL;
	else if (!strcmp(buf, "ERROR_DATA") || !strcmp(buf, "ERROR_DATA\n") || !strcmp(buf, "ERROR_DATA\r\n"))
		info->state = DCT_STATE_ERROR_DATA;
	else
		pr_err("[DCT][%s] data error! (%s)\n", __func__, buf);

	DCT_LOG("[DCT][%s] state stored (%s)\n", __func__, buf); 

	return size;
}


static ssize_t dct_log_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	buf[0] = 0;
	if (info->log) {
		strcpy(buf, info->log);
		kfree(info->log);
		info->log = NULL;
	}

	return strlen(buf);
}


static ssize_t dct_interface_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	strcpy(buf, DCT_INTERFACE_VER);
	return strlen(buf);
}

static ssize_t dct_libname_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);
	
	info->get_libname(buf);
	return strlen(buf);
}


static ssize_t dct_enabled_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	if (!strcmp(buf, "1") || !strcmp(buf, "1\n") || !strcmp(buf, "1\r\n")) {
		info->enabled = true;
		pr_info("[DCT][%s] enabled!\n", __func__);
	}
	else if (!strcmp(buf, "0") || !strcmp(buf, "0\n") || !strcmp(buf, "0\r\n")) {
		info->enabled = false;
		pr_info("[DCT][%s] disabled!\n", __func__);
	}

	return size;
}

static int dct_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct class *dct_class;
	struct device *dct_dev;
	struct sec_dct_info_t *dct_info = dev_get_platdata(&pdev->dev);

	pr_info("[DCT] %s has been created!!!\n", __func__);

	dct_class = class_create(THIS_MODULE, "dct");
	if (IS_ERR(dct_class)) {
		ret = PTR_ERR(dct_class);
		pr_err("Failed to create class(dct)");
		goto fail_out;
	}

	dct_dev = device_create(dct_class, NULL, 0, NULL, "dct_node");
	if (IS_ERR(dct_dev)) {
		ret = PTR_ERR(dct_dev);
		pr_err("Failed to create device(dct)");
		goto fail_out;
	}
	dev_set_drvdata(dct_dev, dct_info);
	sec_dct_info = dct_info;

	ret = sysfs_create_group(&dct_dev->kobj,
			&sec_dct_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group");
		goto fail_out;
	}

fail_out:
	if (ret)
		pr_err(" (err = %d)!\n", ret);

	return ret;
};

static int dct_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sec_dct = {
	.probe = dct_probe,
	.remove = dct_remove,
	.driver = {
		.name = "sec_dct",
		.owner = THIS_MODULE,
	},
};

static int __init sec_dct_init(void)
{
	int ret;
	ret = platform_driver_register(&sec_dct);
	printk("[DCT][%s] initialized!!! %d\n", __func__, ret);
	return ret;
}

static void __exit sec_dct_exit(void)
{
	platform_driver_unregister(&sec_dct);
}

module_init(sec_dct_init);
module_exit(sec_dct_exit);

MODULE_AUTHOR("jeeon.park@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Co. Display Clock Tunning module");
MODULE_LICENSE("GPL");
