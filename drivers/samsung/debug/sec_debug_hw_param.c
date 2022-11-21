/*
 *sec_debug_hw_param.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sec_debug.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <linux/uaccess.h>

#define MAX_DDR_VENDOR 16
#define DATA_SIZE 1024

#define HW_PARAM_CHECK_SIZE(size)	\
	if (size >= 1024)		\
	return 1024;			\

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO	
extern char *sec_debug_extra_info_buf;
#endif

/*
 * LPDDR4 (JESD209-4) MR5 Manufacturer ID
 * 0000 0000B : Reserved
 * 0000 0001B : Samsung
 * 0000 0101B : Nanya
 * 0000 0110B : SK hynix
 * 0000 1000B : Winbond
 * 0000 1001B : ESMT
 * 1111 1111B : Micron
 * All others : Reserved
 */
static char *lpddr4_manufacture_name[MAX_DDR_VENDOR] = {
	"NA",
	"SEC", /* Samsung */
	"NA",
	"KIN", /* Kingston */
	"NA",
	"NAN", /* Nanya */
	"HYN", /* SK hynix */
	"NA",
	"WIN", /* Winbond */
	"ESM", /* ESMT */
	"NA",
	"NA",
	"NA",
	"NA",
	"NA",
	"MIC", /* Micron */
};

#define MAX_HWID	5
unsigned int sec_reset_cnt;
static char mtk_hw_id[MAX_HWID + 1];
static unsigned long dram_VendorID;
static unsigned long pcb_offset;
static unsigned long smd_offset;

static int __init sec_hw_param_get_hw_rev(char *arg)
{
	if (strlen(arg) > MAX_HWID)
		return 0;

	memcpy(mtk_hw_id, arg, (int)strlen(arg));
	return 0;
}

early_param("hw_id", sec_hw_param_get_hw_rev);

static int __init sec_hw_param_get_dram_info(char *arg)
{
	unsigned long dram_RevID1, dram_RevID2, dram_size;
	int ret;

	ret = sscanf(arg, "%lx,%lx,%lx,%lx", &dram_VendorID, &dram_RevID1, &dram_RevID2, &dram_size);

	if (ret != 4) {
		pr_notice("%s: It need to check DRAM Info. (ret=%d, arg=%s)\n", __func__, ret, arg);
	}
	
	return 0;
}

early_param("androidboot.dram_info", sec_hw_param_get_dram_info);

static int __init sec_hw_param_pcb_offset(char *arg)
{
	pcb_offset = simple_strtoul(arg, NULL, 10);	
	return 0;
}

early_param("sec_debug.pcb_offset", sec_hw_param_pcb_offset);

static int __init sec_hw_param_smd_offset(char *arg)
{
	smd_offset = simple_strtoul(arg, NULL, 10);	
	return 0;
}

early_param("sec_debug.smd_offset", sec_hw_param_smd_offset);

static int __init sec_hw_param_get_reset_count(char *arg)
{
	get_option(&arg, &sec_reset_cnt);
	return 0;
}

early_param("sec_debug.reset_rwc", sec_hw_param_get_reset_count);

extern u64 mtk_get_chip_info_version(void);

static ssize_t sec_hw_param_ap_info_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	//u64 lot_id;

	//lot_id = mtk_get_chip_info_version();

	info_size += snprintf(buf, DATA_SIZE, "\"HW_REV\":\"%s\",", mtk_hw_id);
	//info_size +=
	//    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
	//	     "\"LOT_ID\":\"%012llX\",", lot_id);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"PARAM0\":\"\"");

	return info_size;
}

static ssize_t sec_hw_param_ddr_info_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	ssize_t info_size = 0;

	info_size +=
	    snprintf((char *)(buf), DATA_SIZE, "\"DDRV\":\"%s\",",
		     lpddr4_manufacture_name[dram_VendorID]);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"C2D\":\"\",");
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"D2D\":\"\"");

	return info_size;
}

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
static ssize_t sec_hw_param_extra_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	
	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info();
		strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrb_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	info_size = snprintf((char *)buf, 64, " \n");

	return info_size;
}

static ssize_t sec_hw_param_extrc_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	info_size = snprintf((char *)buf, 64, " \n");

	return info_size;
}
#endif

static ssize_t sec_hw_param_pcb_info_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned char barcode[6] = {0,};
	int ret = -1;

	strncpy(barcode, buf, 5);

	ret = sec_set_param_str(pcb_offset , barcode, 5);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, pcb_offset, barcode);
	
	return count;
}

static ssize_t sec_hw_param_smd_info_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned char smd_date[9] = {0,};
	int ret = -1;

	strncpy(smd_date, buf, 8);

	ret = sec_set_param_str(smd_offset , smd_date, 8);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, smd_offset, smd_date);
	
	return count;
}

static struct kobj_attribute sec_hw_param_ap_info_attr =
        __ATTR(ap_info, 0440, sec_hw_param_ap_info_show, NULL);

static struct kobj_attribute sec_hw_param_ddr_info_attr =
        __ATTR(ddr_info, 0440, sec_hw_param_ddr_info_show, NULL);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
static struct kobj_attribute sec_hw_param_extra_info_attr =
		__ATTR(extra_info, 0440, sec_hw_param_extra_info_show, NULL);
		
static struct kobj_attribute sec_hw_param_extrb_info_attr =
		__ATTR(extrb_info, 0440, sec_hw_param_extrb_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrc_info_attr =
		__ATTR(extrc_info, 0440, sec_hw_param_extrc_info_show, NULL);
#endif
		
static struct kobj_attribute sec_hw_param_pcb_info_attr =
        __ATTR(pcb_info, 0660, NULL, sec_hw_param_pcb_info_store);

static struct kobj_attribute sec_hw_param_smd_info_attr =
		__ATTR(smd_info, 0660, NULL, sec_hw_param_smd_info_store);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_ap_info_attr.attr,
	&sec_hw_param_ddr_info_attr.attr,
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	&sec_hw_param_extra_info_attr.attr,
	&sec_hw_param_extrb_info_attr.attr,
	&sec_hw_param_extrc_info_attr.attr,
#endif
	&sec_hw_param_pcb_info_attr.attr,
	&sec_hw_param_smd_info_attr.attr,
	NULL,
};

static struct attribute_group sec_hw_param_attr_group = {
	.attrs = sec_hw_param_attributes,
};

static int __init sec_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &sec_hw_param_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}

device_initcall(sec_hw_param_init);
