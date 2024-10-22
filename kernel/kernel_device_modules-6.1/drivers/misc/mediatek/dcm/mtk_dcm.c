// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>

#include "mtk_dcm_common.h"
#include <mtk_dcm.h>

DEFINE_MUTEX(dcm_lock);
static short dcm_debug;
static short dcm_initiated;
struct DCM *common_dcm_array;
struct DCM_OPS *common_dcm_ops;
unsigned int common_init_dcm_type_by_k;

/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to restore to default DCM HW status, and remove force_disable flag.
 * 2. dcm_get_default() to get default DCM HW status.
 * 3. dcm_force_disable(type) to disable some dcm, and set force_disable flag prevent DCM on.
 * 4. dcm_set_state(type) to enable/disable some dcm.
 * 5. dcm_dump_state(type) to show DCM HW status.
 * 6. /sys/dcm/dcm_state interface:
 *			'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/

static void dcm_get_default(void)
{
	struct DCM *dcm;

	mutex_lock(&dcm_lock);

	for (dcm = common_dcm_array; dcm->name != NULL; dcm++) {
		if (dcm->is_on_func)
			dcm->default_state = dcm->is_on_func();
		else // HW status un-known, is-on-func not defined
			dcm->default_state = -1;
	}

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	struct DCM *dcm;

	if (!is_dcm_initialized()) {
		dcm_pr_info("[%s]DCM common driver not initialized!\n", __func__);
		return;
	}

	dcm_pr_info("[%s]type:0x%08x, set:%d\n", __func__, type, state);

	mutex_lock(&dcm_lock);

	for (dcm = common_dcm_array; dcm->name != NULL; dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->force_disable == false) {
				if (dcm->preset_func)
					dcm->preset_func();
				dcm->func(state);
			}

			dcm_pr_info("[%-16s 0x%08x] current state:%d, default state: %d, force_disable: %d\n",
				 dcm->name, dcm->typeid,
				 dcm->is_on_func != NULL ? dcm->is_on_func() : -1,
				 dcm->default_state,
				 dcm->force_disable);

		}
	}

	mutex_unlock(&dcm_lock);
}

static void dcm_force_disable(unsigned int type)
{
	struct DCM *dcm;

	dcm_pr_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (dcm = common_dcm_array; dcm->name != NULL; dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->preset_func)
				dcm->preset_func();
			dcm->func(DCM_OFF);
			dcm->force_disable = true;

			dcm_pr_info("[%-16s 0x%08x] current state:%d, default state: %d, force_disable: %d\n",
				 dcm->name, dcm->typeid,
				 dcm->is_on_func != NULL ? dcm->is_on_func() : -1,
				 dcm->default_state,
				 dcm->force_disable);

		}
	}

	mutex_unlock(&dcm_lock);
}

static void dcm_restore(unsigned int type)
{
	struct DCM *dcm;

	dcm_pr_info("[%s]type:0x%08x, INIT_DCM_TYPE_BY_K=0x%x\n",
		 __func__, type, common_init_dcm_type_by_k);

	mutex_lock(&dcm_lock);

	for (dcm = common_dcm_array; dcm->name != NULL; dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			/* Restore DCM setting to default */
			if (dcm->default_state >= 0) {
				if (dcm->preset_func)
					dcm->preset_func();
				dcm->func(dcm->default_state);
			}
			dcm->force_disable = false;

			dcm_pr_info("[%-16s 0x%08x] current state:%d, default state: %d, force_disable: %d\n",
				 dcm->name, dcm->typeid,
				 dcm->is_on_func != NULL ? dcm->is_on_func() : -1,
				 dcm->default_state,
				 dcm->force_disable);
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_dump_state(int type)
{
	struct DCM *dcm;

	if (!is_dcm_initialized()) {
		dcm_pr_info("[%s]DCM common driver not initialized!\n", __func__);
		return;
	}

	dcm_pr_info("\n******** Kernel dcm dump state *********\n");
	for (dcm = common_dcm_array; dcm->name != NULL; dcm++) {
		if (type & dcm->typeid) {
			dcm_pr_info("[%-16s 0x%08x] current state:%d, default state: %d, force_disable: %d\n",
				 dcm->name, dcm->typeid,
				 dcm->is_on_func != NULL ? dcm->is_on_func() : -1,
				 dcm->default_state,
				 dcm->force_disable);
		}
	}
}
EXPORT_SYMBOL(dcm_dump_state);

#ifdef CONFIG_PM
static ssize_t dcm_state_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	int len = 0;
	struct DCM *dcm;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n******** dcm dump state *********\n");
	for (dcm = common_dcm_array; dcm->name != NULL; dcm++)
		len += snprintf(buf+len, PAGE_SIZE-len,
				"[%-16s 0x%08x] current state:%d, default state: %d, force_disable: %d\n",
				dcm->name, dcm->typeid,
				dcm->is_on_func != NULL ? dcm->is_on_func() : -1,
				dcm->default_state,
				dcm->force_disable);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"set:       echo set [mask] [mode] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"Force disable:   echo disable [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"restore:   echo restore [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dump:      echo dump [mask] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"debug:     echo debug [0/1] > /sys/dcm/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mask] is hexl bit mask of dcm;\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mode] is type of DCM to set and retained\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dcm_debug=%d, ", dcm_debug);

	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj,
				   struct kobj_attribute *attr, const char *buf,
				   size_t n)
{
	char cmd[16];
	unsigned int mask;
	int ret, mode;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= ALL_DCM_TYPE;

		if (!strcmp(cmd, "restore")) {
			dcm_restore(mask);
		} else if (!strcmp(cmd, "disable")) {
			dcm_force_disable(mask);
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_state(mask);
			common_dcm_ops->dump_regs();
		} else if (!strcmp(cmd, "debug")) {
			if (common_dcm_ops->set_debug_mode == NULL) {
				dcm_pr_info("SORRY, this platform does not support debug mode\n");
			} else if (mask == 0) {
				dcm_debug = 0;
				common_dcm_ops->set_debug_mode(dcm_debug);
			} else if (mask == 1) {
				dcm_debug = 1;
				common_dcm_ops->set_debug_mode(dcm_debug);
			}
		} else if (!strcmp(cmd, "set")) {
			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= ALL_DCM_TYPE;
				if (mode == 0 || mode == 1) {
					dcm_set_state(mask, mode);
				} else {
					dcm_pr_info("SORRY, unsupported mode(must be 1 or 0): %s\n",
							cmd);
				}
			}
		} else {
			dcm_pr_info("SORRY, do not support your command: %s\n",
				    cmd);
		}
		ret = n;
	} else {
		dcm_pr_info("SORRY, do not support your command.\n");
		ret = -EINVAL;
	}

	return ret;
}

static struct kobj_attribute dcm_state_attr = {
	.attr = {
		 .name = "dcm_state",
		 .mode = 0644,
		 },
	.show = dcm_state_show,
	.store = dcm_state_store,
};
static struct kobject *kobj;
#endif /* #ifdef CONFIG_PM */

int mt_dcm_common_init(void)
{
	int state;
	int ret = 0;

	/*dcm_pr_info("[%s]: dcm common init\n", __func__);*/
	if (common_dcm_ops == NULL || common_dcm_array == NULL) {
		dcm_pr_notice("[%s] dcm common ops or array is null\n",
					__func__);
		return -1;
	}
	/* Dump DCM RG setting between LK init & Kernel init */
	common_dcm_ops->dump_regs();

	/* DCM_ALL_OFF or DCM_INIT  */
	common_dcm_ops->get_init_state_and_type(&common_init_dcm_type_by_k, &state);
	if (state == DCM_INIT)
		dcm_restore(common_init_dcm_type_by_k);
	else if (state == DCM_OFF)
		dcm_set_state(common_init_dcm_type_by_k, DCM_OFF);

	/* Read back DCM RG status, save to DCM default_state */
	dcm_get_default();

#ifdef CONFIG_PM
	{
		kobj = kobject_create_and_add("dcm", NULL);
		if (!kobj)
			return -ENOMEM;

		ret = sysfs_create_file(kobj, &dcm_state_attr.attr);
		if (ret)
			dcm_pr_notice("[%s]: fail to create sysfs\n", __func__);
	}
#endif /* #ifdef CONFIG_PM */

	dcm_initiated = 1;

	return ret;
}
EXPORT_SYMBOL(mt_dcm_common_init);

void mt_dcm_array_register(struct DCM *array, struct DCM_OPS *ops)
{
	common_dcm_array = array;
	common_dcm_ops = ops;
}
EXPORT_SYMBOL(mt_dcm_array_register);

/**** public APIs *****/
bool is_dcm_initialized(void)
{
	bool ret = true;

	if (!dcm_initiated)
		ret = false;
	return ret;
}
EXPORT_SYMBOL(is_dcm_initialized);

void mt_dcm_force_disable(void)
{
	if (!dcm_initiated)
		return;

	dcm_force_disable(ALL_DCM_TYPE);
}

void mt_dcm_restore(void)
{
	if (!dcm_initiated)
		return;

	dcm_restore(ALL_DCM_TYPE);
}

static int __init mtk_dcm_init(void)
{
	dcm_debug = 0;
	dcm_initiated = 0;
	return 0;
}

static void mtk_dcm_exit(void)
{
}
module_init(mtk_dcm_init);
module_exit(mtk_dcm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek DCM driver");
