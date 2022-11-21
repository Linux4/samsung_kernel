/*
 * Samsung Mobile VE Group.
 *
 * drivers/video/sec_dct.c
 * (Interface 2.0)
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

//#define PJE_LOG_SAVE_TEST

static ssize_t dct_data_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_data_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_state_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_state_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_log_show(
	struct device *dev, struct device_attribute *attr, char *buf);
#ifdef PJE_LOG_SAVE_TEST
static ssize_t dct_log_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
#endif
static ssize_t dct_enabled_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t dct_interface_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_libname_show(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dct_tunned_show(
	struct device *dev, struct device_attribute *attr, char *buf);

static DEVICE_ATTR(data, 0660 , dct_data_show, dct_data_store);
static DEVICE_ATTR(state, 0660 , dct_state_show, dct_state_store);
#ifdef PJE_LOG_SAVE_TEST
static DEVICE_ATTR(log, 0660 , dct_log_show, dct_log_store);
#else
static DEVICE_ATTR(log, 0440 , dct_log_show, NULL);
#endif
static DEVICE_ATTR(enabled, 0220, NULL, dct_enabled_store);
static DEVICE_ATTR(interface, 0440, dct_interface_show, NULL);
static DEVICE_ATTR(libname, 0440, dct_libname_show, NULL);
static DEVICE_ATTR(tunned, 0440, dct_tunned_show, NULL);

static struct attribute *sec_dct_attributes[] = {
	&dev_attr_data.attr,
	&dev_attr_state.attr,
	&dev_attr_log.attr,
	&dev_attr_enabled.attr,
	&dev_attr_interface.attr,
	&dev_attr_libname.attr,
	&dev_attr_tunned.attr,
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

struct list_head dct_map_info_head = LIST_HEAD_INIT(dct_map_info_head);

#define SAVE_LOG_INFO(...)	save_log(false, __func__, __VA_ARGS__)
#define SAVE_LOG_ERR(...)	save_log(true, __func__, __VA_ARGS__)


#define LOG_FULL_MESSAGE	"BUF FULL!"
#define LOG_MAX_LEN		PAGE_SIZE-sizeof(LOG_FULL_MESSAGE)
char g_log_str[PAGE_SIZE];
bool blog_full;
static void save_log(bool berror, const char *func_name, const char *fmt, ...)
{
	va_list args;
	int remain;
	int log_len = strlen(g_log_str);
	remain = LOG_MAX_LEN-log_len;

	if(blog_full)
		goto out;
		
	if (remain == 0) {
		strcat(g_log_str, LOG_FULL_MESSAGE);
		blog_full = true;
		goto out;		
	}
	
	va_start(args, fmt);
	vsnprintf(&g_log_str[log_len], remain + 1, fmt, args);
	va_end(args);

	if (!berror)
		pr_info("[DCT][%s] %s", func_name, &g_log_str[log_len]);
	else
		pr_err("[DCT][%s] %s", func_name, &g_log_str[log_len]);

	return;

out:
	pr_err("[DCT][%s] %s\n", __func__, LOG_FULL_MESSAGE);
	return;
}


static int check_type_info(void)
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
					bfind = true;
					break;
				}
			}
			if (!bfind) {
				SAVE_LOG_ERR("This system value type is stange!\n");
				SAVE_LOG_ERR("%s type is not exist!\n", dct_tinfo[i].name);
				ret = EINVAL;
				goto out;
			}
		}
	}
out:
	return ret;
}



// error return value
// recv data error : -ENOMSG or -ERANGE
// kernel data or processing error : -EINVAL
static int dct_setData(DCT_DATA_INFO *dinfo, char *line)
{
	int ret = 0;
	char *sep2 = ",";
	char temp_text[30] = "%s is set to ";

	char *read_name;
	char *read_type;
	char *read_value;
	u64 temp_value;

	//DCT_LOG("[DCT][%s] ++\n", __func__);
		
	if (line[strlen(line)-1] == '\r')
		line[strlen(line)-1] = 0;

	if((read_name = strsep(&line, sep2)) == NULL)
	{
		SAVE_LOG_ERR("Read data name is NULL\n");
		ret = -ENOMSG;
		goto out;
	}
	if((read_type = strsep(&line, sep2)) == NULL)
	{
		SAVE_LOG_ERR("Read data type is NULL\n");
		ret = -ENOMSG;
		goto out;
	}
	if((read_value = strsep(&line, sep2)) == NULL)
	{
		SAVE_LOG_ERR("Read data name is NULL\n");
		ret = -ENOMSG;
		goto out;
	}

	if(strcmp(read_name, dinfo->name) || strcmp(read_type, dct_tinfo[dinfo->type].name)) {
		SAVE_LOG_ERR("data is wrong!\n");
		SAVE_LOG_ERR("\tname : Request - %s , Read - %s\n", dinfo->name, read_name);
		SAVE_LOG_ERR("\ttype : Request - %s , Read - %s\n", dct_tinfo[dinfo->type].name, read_type);
		ret = -ENOMSG;
		goto out;
	}

	if (read_value[0] == '-') {
		SAVE_LOG_ERR("Read value is negative\n(%s,%s,%s)\n", read_name, read_type, read_value);
		ret = -ENOMSG;
		goto out;
	}

	ret = kstrtou64(read_value, 10, &temp_value);
	if (ret) {
		//int i=0;
		SAVE_LOG_ERR("Data value translation error!\n(%s,%s,%s)\nerrno = %d\n", 
			read_name, read_type, read_value, ret);
		//SAVE_LOG_ERR("\tread_value len = %d\n", strlen(read_value));
		//for(i = 0; i < strlen(read_value); i++) {
		//	SAVE_LOG_ERR("%hhd ", read_value[i]);
		//}
		//SAVE_LOG_ERR("\n");
		ret = -ERANGE;		// unify result value
		goto out;
	}


	switch (dinfo->type) {
	case DCT_TYPE_U8:
		if (temp_value != (u64)(u8)temp_value) {
			SAVE_LOG_ERR("Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffLL);
			ret = -ERANGE;
			goto out; 	
		}
		break;
	case DCT_TYPE_U16:
		if (temp_value != (u64)(u16)temp_value) {
			SAVE_LOG_ERR("Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffLL);
			ret = -ERANGE;
			goto out; 	
		}
		break;
	case DCT_TYPE_U32:
		if (temp_value != (u64)(u32)temp_value) {
			SAVE_LOG_ERR("Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffffffLL);
			ret = -ERANGE;
			goto out; 	
		}
		break;
	case DCT_TYPE_U64:
		/*
		if (temp_value != (u64)(u64)temp_value) {
			SAVE_LOG_ERR("Data range error!\ndata : %llu\nEnable Max Value: %llu\n", temp_value, 0xffffffffffffffffLL);
			ret = -ERANGE;
			goto out; 	
		}
		*/
		break;
	default:
		SAVE_LOG_ERR("Unknown Type!(%s %d)\n", dinfo->name, dinfo->type);
		ret = -EINVAL;
		goto out;	
	}
	
	dinfo->data = temp_value;
	strcat(temp_text, dct_tinfo[DCT_TYPE_U64].format);
	strcat(temp_text, "\n");
	SAVE_LOG_INFO(temp_text, dinfo->name, dinfo->data);

out:
	//DCT_LOG("[DCT][%s] -- (%d)\n", __func__, ret);

	return ret;
}


static int setAllData(char *recv_data)
{
	int ret = 0;
	DCT_DATA_INFO *dinfo = NULL;
	char *tosep = recv_data;
	char *sep = "\n";
	char *line = NULL;

	pr_info("[DCT][%s] ++\n", __func__);

	list_for_each_entry(dinfo, &dct_map_info_head, info_list) {
		
		line = strsep(&tosep, sep);
		if(line == NULL) {
			SAVE_LOG_ERR("requested line is null!\nRequest : %s %s\n", 
				dinfo->name, dct_tinfo[dinfo->type].name);
			ret = -ENOMSG;
			goto out;
		}

		if ((ret = dct_setData(dinfo, line)) < 0)
			goto out;
	}

out:
	pr_info("[DCT][%s] --(%d)\n", __func__, ret);

	return ret;
}


int dct_init_data(char *name, int type, void *addr, int size)
{
	int ret = 0;
	DCT_DATA_INFO *pinfo = kzalloc(sizeof(DCT_DATA_INFO), GFP_KERNEL);

	if (pinfo == NULL) {
		pr_err("[DCT][%s] Malloc of DCT_MAP_INFO is fail!\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	list_add_tail(&pinfo->info_list, &dct_map_info_head);

	strcpy(pinfo->name, name);
	pinfo->type = type;

	if (size != dct_tinfo[type].size) {
		SAVE_LOG_ERR("mapping size error, Check the kernel code\n");
		SAVE_LOG_ERR("request name : %s\n", name);
		SAVE_LOG_ERR("(request : %s(%d), target : %d)\n\n", 
			dct_tinfo[type].name, dct_tinfo[type].size, size);
		ret = -EINVAL;
		goto out;
	}

	switch (type) {
	case DCT_TYPE_U8:
		pinfo->data_org = *(u8 *)addr;
		break;
	case DCT_TYPE_U16:
		pinfo->data_org = *(u16 *)addr;
		break;
	case DCT_TYPE_U32:		
		pinfo->data_org = *(u32 *)addr;
		break;
	case DCT_TYPE_U64:		
		pinfo->data_org = *(u64 *)addr;
		break;
	default:
		SAVE_LOG_ERR("Unknown Type!(%s %d)\n", name, type);
		ret = -EINVAL;
		goto out;
	}
	pinfo->apply_addr = addr;
	pinfo->apply_size = size;

	pr_info("[DCT][%s] %s is set to %llu (type : %d, size: %d)\n", 
		__func__, pinfo->name, (unsigned long long)pinfo->data_org, 
		pinfo->type, pinfo->apply_size);
	
out:
	return ret;
}

static void remove_data(void)
{
	DCT_DATA_INFO *pinfo_cur = NULL, *pinfo_next = NULL;

	list_for_each_entry_safe(pinfo_cur, pinfo_next, &dct_map_info_head, info_list) {
		list_del_init(&pinfo_cur->info_list);
		kfree(pinfo_cur);
	}

	return;
}


static int dct_get_data(char *buf, DCT_DATA_INFO *dinfo, bool tunned)
{
	int ret = 0;
	char temp_text[20] = "%s,%s,";
	void *pdata;

	DCT_LOG("[DCT][%s] ++\n", __func__);

	strcat(temp_text, dct_tinfo[DCT_TYPE_U64].format);
	strcat(temp_text, "\n");

	if(!tunned)
		pdata = &dinfo->data_org;
	else
		pdata = &dinfo->data;

	ret = sprintf(buf, temp_text, dinfo->name, dct_tinfo[dinfo->type].name, *(u64 *)pdata);

	DCT_LOG("[DCT][%s] --(%d)\n", __func__, ret);
	return ret;
}


static ssize_t dct_data_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);
	DCT_DATA_INFO *dinfo = NULL;
	char *buf_temp = NULL;
	int len = 0, ret = 0;

	DCT_LOG("[DCT][%s] ++\n", __func__);
	
	if (!info->enabled)
		goto out;

	if (!info->tunned)
		strcpy(buf, "[Original Kernel]\n");
	else
		strcpy(buf, "[Tunned Kernel]\n");
	buf_temp = &buf[strlen(buf)];

	list_for_each_entry(dinfo, &dct_map_info_head, info_list) {
		pr_info("[DCT][%s] name = %s, type = %d\n", 
			__func__, dinfo->name, dinfo->type);
		if((len = dct_get_data(buf_temp, dinfo, info->tunned)) < 0) {
			ret = len;
			goto out;
		}
		else {
			buf_temp +=len;
		}
	}

	ret = strlen(buf);
	
out:
	DCT_LOG("[DCT][%s] --(%d)\n", __func__, ret);
	return ret;
}



static ssize_t dct_data_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *tofree = NULL;
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	DCT_LOG("\n[DCT][%s] ++\n", __func__);

	if (!info->enabled)
		goto out;
	
	tofree = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (tofree == NULL) {
		pr_err("[DCT][%s] Malloc of dct_recv_str is fail!\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	strcpy(tofree, buf);

	pr_info("[DCT][%s][LOW DATA]\n%s\n", __func__, tofree);

	if ((ret = setAllData(tofree)) < 0) {
		if (ret == -EINVAL)
			info->state = DCT_STATE_ERROR_KERNEL;
		else if (ret == -ENOMSG || ret == -ERANGE)
			info->state = DCT_STATE_ERROR_DATA;
		else
			info->state = DCT_STATE_ERROR_KERNEL;
		size = ret;
	}
	else
		info->state = DCT_STATE_DATA;

out:
	if (tofree) {
		kfree(tofree);
		tofree = NULL;
	}

	DCT_LOG("[DCT][%s] --(%d)\n\n", __func__, ret);

	return size;
}


static void dct_applyOneData(DCT_DATA_INFO *dinfo, int state)
{
	u64 temp_value;
	
	if (state == DCT_STATE_DATA)
		temp_value = dinfo->data;
	else if (state == DCT_STATE_NODATA)
		temp_value = dinfo->data_org;
		

	switch(dinfo->type) {
	case DCT_TYPE_U8:
		*(u8 *)dinfo->apply_addr = (u8)temp_value;
		break;
	case DCT_TYPE_U16:
		*(u16 *)dinfo->apply_addr = (u16)temp_value;
		break;
	case DCT_TYPE_U32:
		*(u32 *)dinfo->apply_addr = (u32)temp_value;
		break;
	case DCT_TYPE_U64:
		*(u64 *)dinfo->apply_addr = (u64)temp_value;
		break;
/*
	default:
		//pr_err("[DCT][%s] Unknown Type!(%s:%d)\n", __func__, dinfo->name, dinfo->type);
		SAVE_LOG_ERR("Unknown Type!(%s:%d)\n", __func__, dinfo->name, dinfo->type);
		ret = -EINVAL;
		goto error;		
*/
	}

	return;
}


static void applyData(void)
{
	DCT_DATA_INFO *dinfo = NULL;

	DCT_LOG("\n[DCT][%s] ++\n", __func__);

	if (sec_dct_info->state == DCT_STATE_DATA) {
		list_for_each_entry(dinfo, &dct_map_info_head, info_list) {
			dct_applyOneData(dinfo, DCT_STATE_DATA);
		}
		sec_dct_info->tunned = true;
		//sec_dct_info->post_applyData();
		pr_info("[DCT][%s] set data of dct application!\n", __func__);
	}
	else if (sec_dct_info->state == DCT_STATE_NODATA && sec_dct_info->tunned) {
		list_for_each_entry(dinfo, &dct_map_info_head, info_list) {
			dct_applyOneData(dinfo, DCT_STATE_NODATA);
		}
		sec_dct_info->tunned = false;
		pr_info("[DCT][%s] set original data!\n", __func__);
	}
	else { 	// nothing
		;
	}
	DCT_LOG("[DCT][%s] --\n\n", __func__);

	return;
}

static void finish_applyData(void)
{
	pr_info("[DCT][%s] +-\n", __func__);

	if (sec_dct_info->state == DCT_STATE_DATA)
		sec_dct_info->state = DCT_STATE_APPLYED;
	return;
}


static ssize_t dct_state_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	if (!info->enabled)
		goto out;

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
out:
	return strlen(buf);
}

static ssize_t dct_state_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	if (!info->enabled)
		goto out;

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

out:
	return size;
}


static ssize_t dct_log_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	buf[0] = 0;

	strcpy(buf, g_log_str);
	memset(g_log_str, 0, PAGE_SIZE);
	blog_full = false;

	return strlen(buf);
}

#ifdef PJE_LOG_SAVE_TEST
static ssize_t dct_log_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	SAVE_LOG_INFO(buf);
	
	return size;
}
#endif

static ssize_t dct_enabled_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);
	int ret = 0;

	if (!strcmp(buf, "1") || !strcmp(buf, "1\n") || !strcmp(buf, "1\r\n")) {
		if (!info->enabled) {
			if (info->ref_addr == NULL) {
				SAVE_LOG_ERR("Reference Address is not Initialize!\n Ask system S/W team\n");
				size = -EINVAL;
				goto out;
			}
			
			if ((ret = info->init_data()) < 0) {
				size = ret;
				goto out;
			}
			info->enabled = true;
			pr_info("[DCT][%s] enabled!\n", __func__);
		}		
	}
	else if (!strcmp(buf, "0") || !strcmp(buf, "0\n") || !strcmp(buf, "0\r\n")) {
		if (info->enabled && !info->tunned) {
			info->remove_data();
			info->enabled = false;
			pr_info("[DCT][%s] disabled!\n", __func__);
		}
	}

out:
	return size;
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

static ssize_t dct_tunned_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_dct_info_t *info = dev_get_drvdata(dev);

	if (info->tunned)
		strcpy(buf, "1");
	else
		strcpy(buf, "0");
	return strlen(buf);
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
		goto out;
	}

	dct_dev = device_create(dct_class, NULL, 0, NULL, "dct_node");
	if (IS_ERR(dct_dev)) {
		ret = PTR_ERR(dct_dev);
		pr_err("Failed to create device(dct)");
		goto out;
	}
	dev_set_drvdata(dct_dev, dct_info);
	sec_dct_info = dct_info;

	ret = sysfs_create_group(&dct_dev->kobj,
			&sec_dct_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group");
		goto out;
	}

	sec_dct_info->applyData = applyData;
	sec_dct_info->finish_applyData = finish_applyData;
	sec_dct_info->remove_data = remove_data;
	
out:
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
	if((ret = check_type_info())) {
		goto out;
	}
	
	ret = platform_driver_register(&sec_dct);
	pr_info("[DCT][%s] initialized!!! %d\n", __func__, ret);

out:
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
