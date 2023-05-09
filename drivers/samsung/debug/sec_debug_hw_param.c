/*
 *sec_hw_param.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <soc/samsung/exynos-pm.h>

#include "sec_debug_internal.h"

#define DATA_SIZE 1024
#define LOT_STRING_LEN 5

static unsigned int sec_hw_rev;
static unsigned int chipid_fail_cnt;
static unsigned int lpi_timeout_cnt;
static unsigned int cache_err_cnt;
static unsigned int codediff_cnt;

#define MAX_NR_DRAMINFO	(16)
static char draminfo[MAX_NR_DRAMINFO];

#if defined(CONFIG_SAMSUNG_VST_CAL)
static unsigned int vst_result;
#endif
static unsigned long pcb_offset;
static unsigned long smd_offset;
static unsigned int lpddr4_size;
static char warranty = 'D';

static int __init sec_hw_param_get_hw_rev(char *arg)
{
	get_option(&arg, &sec_hw_rev);
	return 0;
}

early_param("androidboot.revision", sec_hw_param_get_hw_rev);

static int __init sec_hw_param_check_chip_id(char *arg)
{
	get_option(&arg, &chipid_fail_cnt);
	return 0;
}

early_param("sec_debug.chipidfail_cnt", sec_hw_param_check_chip_id);

static int __init sec_hw_param_check_lpi_timeout(char *arg)
{
	get_option(&arg, &lpi_timeout_cnt);
	return 0;
}

early_param("sec_debug.lpitimeout_cnt", sec_hw_param_check_lpi_timeout);

static int __init sec_hw_param_cache_error(char *arg)
{
	get_option(&arg, &cache_err_cnt);
	return 0;
}

early_param("sec_debug.cache_err_cnt", sec_hw_param_cache_error);

static int __init sec_hw_param_code_diff(char *arg)
{
	get_option(&arg, &codediff_cnt);
	return 0;
}

early_param("sec_debug.codediff_cnt", sec_hw_param_code_diff);

#if defined(CONFIG_SAMSUNG_VST_CAL)
static int __init sec_hw_param_vst_result(char *arg)
{
	get_option(&arg, &vst_result);
	return 0;
}

early_param("sec_debug.vst_result", sec_hw_param_vst_result);
#endif

static int __init sec_hw_param_pcb_offset(char *arg)
{
	unsigned long val;

	if (kstrtoul(arg, 10, &val) < 0) {
		pr_err("Bad sec_debug.pcb_offset value\n");
		return 0;
	}

	pcb_offset = val;
	return 0;
}

early_param("sec_debug.pcb_offset", sec_hw_param_pcb_offset);

static int __init sec_hw_param_smd_offset(char *arg)
{
	unsigned long val;

	if (kstrtoul(arg, 10, &val) < 0) {
		pr_err("Bad sec_debug.smd_offset value\n");
		return 0;
	}

	smd_offset = val;
	return 0;
}

early_param("sec_debug.smd_offset", sec_hw_param_smd_offset);

static int __init sec_hw_param_lpddr4_size(char *arg)
{
	get_option(&arg, &lpddr4_size);
	return 0;
}

early_param("sec_debug.lpddr4_size", sec_hw_param_lpddr4_size);

static int __init sec_hw_param_bin(char *arg)
{
	warranty = (char)*arg;
	return 0;
}

early_param("sec_debug.bin", sec_hw_param_bin);

static int __init sec_hw_param_draminfo(char *arg)
{
	snprintf(draminfo, MAX_NR_DRAMINFO, "%s", arg);
	pr_info("%s: %s\n", __func__, draminfo);

	return 0;
}

early_param("androidboot.dram_info", sec_hw_param_draminfo);

static u32 chipid_reverse_value(u32 value, u32 bitcnt)
{
	int tmp, ret = 0;
	int i;

	for (i = 0; i < bitcnt; i++) {
		tmp = (value >> i) & 0x1;
		ret += tmp << ((bitcnt - 1) - i);
	}

	return ret;
}

static void chipid_dec_to_36(u32 in, char *p)
{
	int mod;
	int i;

	for (i = LOT_STRING_LEN - 1; i >= 1; i--) {
		mod = in % 36;
		in /= 36;
		p[i] = (mod < 10) ? (mod + '0') : (mod - 10 + 'A');
	}

	p[0] = 'N';
	p[LOT_STRING_LEN] = '\0';
}

static ssize_t sec_hw_param_ap_info_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int reverse_id_0 = 0;
	u32 tmp = 0;
	char lot_id[LOT_STRING_LEN + 1];
	char val[32] = {0, };

	reverse_id_0 = chipid_reverse_value(exynos_soc_info.lot_id, 32);
	tmp = (reverse_id_0 >> 11) & 0x1FFFFF;
	chipid_dec_to_36(tmp, lot_id);

	info_size += snprintf(buf, DATA_SIZE, "\"HW_REV\":\"%d\",", sec_hw_rev);

	memset(val, 0, 32);
	get_bk_item_val_as_string("RSTCNT", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"RSTCNT\":\"%s\",", val);

	memset(val, 0, 32);
	get_bk_item_val_as_string("CHI", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"CHIPID_FAIL\":\"%s\",", val);

	memset(val, 0, 32);
	get_bk_item_val_as_string("LPI", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"LPI_TIMEOUT\":\"%s\",", val);

	memset(val, 0, 32);
	get_bk_item_val_as_string("CDI", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"CODE_DIFF\":\"%s\",", val);

	memset(val, 0, 32);
	get_bk_item_val_as_string("BIN", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"BIN\":\"%s\",", val);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASB\":\"%d\",", id_get_asb_ver());
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"PSITE\":\"%d\",", id_get_product_line());
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"LOT_ID\":\"%s\",", lot_id);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"CACHE_ERR\":\"%d\",", cache_err_cnt);
#if defined(CONFIG_SAMSUNG_VST_CAL)
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"VST_RESULT\":\"%d\",", vst_result);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"VST_ADJUST\":\"%d\",", volt_vst_cal_bdata);
#endif
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASV_BIG\":\"%d\",", asv_ids_information(bg));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASV_MID\":\"%d\",", asv_ids_information(mg));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASV_LIT\":\"%d\",", asv_ids_information(lg));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASV_G3D\":\"%d\",", asv_ids_information(g3dg));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"ASV_MIF\":\"%d\",", asv_ids_information(mifg));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"IDS_BIG\":\"%d\",", asv_ids_information(bids));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"IDS_MID\":\"%d\",", asv_ids_information(mids));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"IDS_LIT\":\"%d\",", asv_ids_information(lids));
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
				"\"IDS_G3D\":\"%d\",", asv_ids_information(gids));

	return info_size;
}

static ssize_t sec_hw_param_ddr_info_show(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	ssize_t info_size = 0;

	info_size += snprintf((char *)(buf), DATA_SIZE, "\"DDRV\":\"%s\"", draminfo);

	return info_size;
}

static ssize_t sec_hw_param_extra_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_A(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t sec_hw_param_extrb_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_B(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t sec_hw_param_extrc_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_C(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t sec_hw_param_extrm_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_M(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t sec_hw_param_extrf_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_F(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t sec_hw_param_extrt_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_T(buf);
	info_size = strlen(buf);

	return info_size;
}

/* TODO: these functions need sec_param driver. please test them after
 * sec_param is ported
 */
#ifdef CONFIG_SEC_PARAM
static ssize_t sec_hw_param_pcb_info_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char barcode[6] = {0,};
	int ret = -1;

	strncpy(barcode, buf, 5);

	ret = sec_set_param_str(pcb_offset, barcode, 5);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, pcb_offset, barcode);

	return count;
}

static ssize_t sec_hw_param_smd_info_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char smd_date[9] = {0,};
	int ret = -1;

	strncpy(smd_date, buf, 8);

	ret = sec_set_param_str(smd_offset, smd_date, 8);
	if (ret < 0)
		pr_err("%s : Set Param fail. offset (%lu), data (%s)", __func__, smd_offset, smd_date);

	return count;
}
#endif

static ssize_t sec_hw_param_thermal_info_show(struct kobject *kobj,
					      struct kobj_attribute *attr,
					      char *buf)
{
	ssize_t info_size = 0;

	return info_size;
}

static struct kobj_attribute sec_hw_param_ap_info_attr =
	__ATTR(ap_info, 0440, sec_hw_param_ap_info_show, NULL);

static struct kobj_attribute sec_hw_param_ddr_info_attr =
	__ATTR(ddr_info, 0440, sec_hw_param_ddr_info_show, NULL);

static struct kobj_attribute sec_hw_param_extra_info_attr =
	__ATTR(extra_info, 0440, sec_hw_param_extra_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrb_info_attr =
	__ATTR(extrb_info, 0440, sec_hw_param_extrb_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrc_info_attr =
	__ATTR(extrc_info, 0440, sec_hw_param_extrc_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrm_info_attr =
	__ATTR(extrm_info, 0440, sec_hw_param_extrm_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrf_info_attr =
	__ATTR(extrf_info, 0440, sec_hw_param_extrf_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrt_info_attr =
	__ATTR(extrt_info, 0440, sec_hw_param_extrt_info_show, NULL);

#ifdef CONFIG_SEC_PARAM
static struct kobj_attribute sec_hw_param_pcb_info_attr =
	__ATTR(pcb_info, 0660, NULL, sec_hw_param_pcb_info_store);

static struct kobj_attribute sec_hw_param_smd_info_attr =
	__ATTR(smd_info, 0660, NULL, sec_hw_param_smd_info_store);
#endif

static struct kobj_attribute sec_hw_param_thermal_info_attr =
	__ATTR(thermal_info, 0440, sec_hw_param_thermal_info_show, NULL);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_ap_info_attr.attr,
	&sec_hw_param_ddr_info_attr.attr,
	&sec_hw_param_extra_info_attr.attr,
	&sec_hw_param_extrb_info_attr.attr,
	&sec_hw_param_extrc_info_attr.attr,
	&sec_hw_param_extrm_info_attr.attr,
	&sec_hw_param_extrf_info_attr.attr,
	&sec_hw_param_extrt_info_attr.attr,
#ifdef CONFIG_SEC_PARAM
	&sec_hw_param_pcb_info_attr.attr,
	&sec_hw_param_smd_info_attr.attr,
#endif
	&sec_hw_param_thermal_info_attr.attr,
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
