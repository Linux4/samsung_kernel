/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - UFC(User Frequency Change) driver
 * Author : HEUNGSIK CHOI (hs0413.choi@samsung.com)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#define SCALE_SIZE 8

#define AUTO_FILL 1
#define NOT_FILL 0

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/ems.h>
#include <linux/exynos-ucc.h>

#include <soc/samsung/exynos-cpuhp.h>

#include "exynos-acme.h"
#include "exynos-ufc.h"

char *stune_group_name2[] = {
	"min-limit",
	"max-limit",
	"min-wo-boost-limit",
};

struct exynos_ufc ufc = {
	.last_min_input = -1,
	.last_min_wo_boost_input = -1,
	.last_max_input = -1,
};
struct cpufreq_policy *little_policy;
static struct emstune_mode_request emstune_req_ufc;

enum {
	UFC_COL_VFREQ,
	UFC_COL_BIG,
	UFC_COL_LIT,
	UFC_COL_EMSTUNE,
};

static struct ucc_req ucc_req =
{
	.name = "ufc",
};
static int ucc_requested;
static int ucc_requested_val;

#define DEFAULT_LEVEL 0

/*********************************************************************
 *                          HELPER FUNCTION                           *
 *********************************************************************/
static char* get_mode_string(enum exynos_ufc_execution_mode mode)
{
	switch (mode) {
	case AARCH64_MODE:
		return "AARCH64_MODE";
	case AARCH32_MODE:
		return "AARCH32_MODE";
	default :
		return NULL;
	}

	return NULL;
}

static char* get_ctrl_type_string(enum exynos_ufc_ctrl_type type)
{
	switch (type) {
	case PM_QOS_MIN_LIMIT:
		return "PM_QOS_MIN_LIMIT";
	case PM_QOS_MAX_LIMIT:
		return "PM_QOS_MAX_LIMIT";
	case PM_QOS_MIN_WO_BOOST_LIMIT:
		return "PM_QOS_MIN_WO_BOOST_LIMIT";
	default :
		return NULL;
	}

	return NULL;
}

static struct exynos_cpufreq_domain* first_domain(void)
{
	struct list_head *domain_list = get_domain_list();

	if (!domain_list)
		return NULL;

	return list_first_entry(domain_list,
			struct exynos_cpufreq_domain, list);
}

static bool ufc_check_little_domain(struct ufc_domain *ufc_dom)
{
	struct cpumask mask;
	struct exynos_cpufreq_domain *domain = first_domain();

	/* If domain is NULL, then return true for nothing to do */
	if (!domain)
		return true;

	cpumask_xor(&mask, &ufc_dom->cpus, &domain->cpus);

	if (cpumask_empty(&mask))
		return true;

	return false;
}

static void
disable_domain_cpus(struct ufc_domain *ufc_dom)
{
	struct cpumask mask;

	if (ufc_check_little_domain(ufc_dom))
		return;

	cpumask_andnot(&mask, cpu_online_mask, &ufc_dom->cpus);
	exynos_cpuhp_request("UFC", mask, 0);
}

static void
enable_domain_cpus(struct ufc_domain *ufc_dom)
{
	struct cpumask mask;

	if (ufc_check_little_domain(ufc_dom))
		return;

	cpumask_or(&mask, cpu_online_mask, &ufc_dom->cpus);
	exynos_cpuhp_request("UFC", mask, 0);
}

unsigned int get_cpufreq_max_limit(void)
{
	struct ufc_domain *ufc_dom;
	unsigned int pm_qos_max;

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		/* get value of minimum PM QoS */
		pm_qos_max = pm_qos_request(ufc_dom->pm_qos_max_class);
		if (pm_qos_max > 0) {
			pm_qos_max = min(pm_qos_max, ufc_dom->max_freq);
			pm_qos_max = max(pm_qos_max, ufc_dom->min_freq);

			return pm_qos_max;
		}
	}

	/*
	 * If there is no QoS at all domains, it prints big maxfreq
	 */
	return first_domain()->max_freq;
}

static unsigned int ufc_get_table_col(struct device_node *child)
{
	unsigned int col;

	of_property_read_u32(child, "table-col", &col);

	return col;
}

static unsigned int ufc_get_table_row(struct device_node *child)
{
	unsigned int row;

	of_property_read_u32(child, "table-row", &row);

	return row;
}

static unsigned int ufc_clamp_vfreq(unsigned int vfreq,
				struct ufc_table_info *table_info)
{
	unsigned int max_vfreq;
	unsigned int min_vfreq;

	max_vfreq = table_info->ufc_table[UFC_COL_VFREQ][0];
	min_vfreq = table_info->ufc_table[UFC_COL_VFREQ][ufc.table_row + ufc.lit_table_row - 1];

	if (max_vfreq < vfreq)
		return max_vfreq;
	if (min_vfreq > vfreq)
		return min_vfreq;

	return vfreq;
}

static int ufc_get_proper_min_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;
	if (table_info->ufc_table[0][index] < vfreq)
		index--;

	return index;
}

static int ufc_get_proper_max_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;

	return index;
}

static int ufc_get_proper_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info, int ctrl_type)
{
	int target_idx = 0;

	switch (ctrl_type) {
	case PM_QOS_MIN_LIMIT:
	case PM_QOS_MIN_WO_BOOST_LIMIT:
		target_idx = ufc_get_proper_min_table_index(vfreq, table_info);
		break;

	case PM_QOS_MAX_LIMIT:
		target_idx = ufc_get_proper_max_table_index(vfreq, table_info);
		break;
	}
	return target_idx;
}

static void ufc_clear_min_qos(int ctrl_type)
{
	struct ufc_domain *ufc_dom;
	struct pm_qos_request *pm_qos_handle = NULL;
	bool emstune_mode_change = false;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		pm_qos_handle = NULL;

		switch(ctrl_type) {
		case PM_QOS_MIN_LIMIT:
			pm_qos_handle = &ufc_dom->user_min_qos_req;
			emstune_mode_change = true;
			break;
		case PM_QOS_MIN_WO_BOOST_LIMIT:
			pm_qos_handle = &ufc_dom->user_min_qos_wo_boost_req;
			break;
		}

		if (pm_qos_handle == NULL)
			continue;

		pm_qos_update_request(pm_qos_handle, 0);
	}

	if (emstune_mode_change) {
		emstune_update_request(&emstune_req_ufc, DEFAULT_LEVEL);
		ucc_requested_val = 0;
		ucc_update_request(&ucc_req, ucc_requested_val);
	}
}

static void ufc_clear_max_qos(void)
{
	struct ufc_domain *ufc_dom;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		enable_domain_cpus(ufc_dom);
		pm_qos_update_request(&ufc_dom->user_max_qos_req, ufc_dom->max_freq);
	}
}

static void ufc_clear_pm_qos(int ctrl_type)
{
	switch (ctrl_type) {
	case PM_QOS_MIN_LIMIT:
	case PM_QOS_MIN_WO_BOOST_LIMIT:
		ufc_clear_min_qos(ctrl_type);
		break;

	case PM_QOS_MAX_LIMIT:
		ufc_clear_max_qos();
		break;
	}
}

static bool ufc_need_adjust_freq(unsigned int freq, struct ufc_domain *dom)
{
	if ((freq < dom->min_freq) || (freq > dom->max_freq))
		return true;

	return false;
}

static unsigned int ufc_adjust_freq(unsigned int freq, struct ufc_domain *dom, int ctrl_type)
{
	if (freq > dom->max_freq)
		return dom->max_freq;

	if (freq < dom->min_freq) {
		switch(ctrl_type) {
		case PM_QOS_MIN_LIMIT:
		case PM_QOS_MIN_WO_BOOST_LIMIT:
			return dom->min_freq;

		case PM_QOS_MAX_LIMIT:
			if (freq == 0)
				return 0;
			return dom->min_freq;
		}
	}
	return freq;
}

/*********************************************************************
 *                          SYSFS FUNCTION                           *
 *********************************************************************/
#define ufc_attr_ro(_name)						\
static struct ufc_attr _name =						\
__ATTR(_name, 0444, show_##_name, NULL)

#define ufc_attr_rw(_name)						\
static struct ufc_attr _name =						\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define to_attr(a) container_of(a, struct ufc_attr, attr)
#define to_ufc_table_info(a) container_of(a, struct ufc_table_info, kobj)

struct ufc_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

static ssize_t show_ufc_all_tables(struct kobject *kobj, struct attribute *attr,
					char *buf)
{
	struct ufc_table_info *table_info = to_ufc_table_info(kobj);
	int count = 0;
	int c_idx, r_idx;
	char *ctrl_str, *mode_str;

	ctrl_str = get_ctrl_type_string(table_info->ctrl_type);
	mode_str = get_mode_string(table_info->mode);

	count += snprintf(buf + count, PAGE_SIZE - count, "Table Ctrl Type: %s(%d), Mode: %s(%d)\n",
			ctrl_str, table_info->ctrl_type, mode_str, table_info->mode);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++) {
		for (c_idx = 0; c_idx < ufc.table_col; c_idx++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					table_info->ufc_table[c_idx][r_idx]);
		}
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}

static ssize_t store_ufc_table_change(struct kobject *kobj, const char *buf,
					size_t count)
{
	int ret;
	unsigned int edit_row, edit_col;
	unsigned int edit_freq;

	struct ufc_table_info *table_info = to_ufc_table_info(kobj);

	ret= sscanf(buf, "%d %d %d\n",
			&edit_row, &edit_col, &edit_freq);

	if (ret != 3) {
		pr_err("We need 3 inputs. Enter row, col, frequency");
		return -EINVAL;
	}

	if (edit_row < 0 || edit_row > ufc.table_row) {
		pr_err("input row is between 0 and %d\n", ufc.table_row);
		return -EINVAL;
	}

	if (edit_col < 0 || edit_col > ufc.table_col) {
		pr_err("input row is between 0 and %d\n", ufc.table_row);
		return -EINVAL;
	}

	table_info->ufc_table[edit_col][edit_row] = edit_freq;

	return ret;
}

static ssize_t show_ufc_table_change(struct kobject *kobj, struct attribute *attr,
					char *buf)
{
	struct ufc_table_info *table_info = to_ufc_table_info(kobj);
	int count = 0;
	int c_idx, r_idx;
	char *ctrl_str, *mode_str;

	ctrl_str = get_ctrl_type_string(table_info->ctrl_type);
	mode_str = get_mode_string(table_info->mode);

	count += snprintf(buf + count, PAGE_SIZE - count, "Table Ctrl Type: %s(%d), Mode: %s(%d)\n",
			ctrl_str, table_info->ctrl_type, mode_str, table_info->mode);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++) {
		for (c_idx = 0; c_idx < ufc.table_col; c_idx++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					table_info->ufc_table[c_idx][r_idx]);
		}
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct ufc_attr *hattr = to_attr(attr);
	ssize_t ret;

	ret = hattr->show(kobj, attr, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct ufc_attr *hattr = to_attr(attr);
	ssize_t ret;

	ret = hattr->store(kobj, buf, count);

	return ret;
}

ufc_attr_ro(ufc_all_tables);
ufc_attr_rw(ufc_table_change);

static struct attribute *ufc_debug_attrs[] = {
	&ufc_all_tables.attr,
	&ufc_table_change.attr,
	NULL
};

static const struct sysfs_ops ufc_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_ufc = {
	.sysfs_ops	= &ufc_sysfs_ops,
	.default_attrs	= ufc_debug_attrs,
};

static void ufc_update_limit(int input_freq, int ctrl_type, int mode)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info, *target_table_info = NULL;
	int target_idx = 0;
	bool emstune_mode_change = false;

	if (input_freq <= 0) {
		ufc_clear_pm_qos(ctrl_type);
		return;
	}
	list_for_each_entry(table_info, &ufc.ufc_table_list, list) {
		if (table_info->ctrl_type == ctrl_type && table_info->mode == mode) {
			target_table_info = table_info;
			break;
		}
	}

	if (!target_table_info) {
		pr_err("failed to find target table\n");
		return;
	}
	target_idx = ufc_get_proper_table_index(input_freq, target_table_info, ctrl_type);

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int target_freq;
		unsigned int col_idx = ufc_dom->table_idx;
		struct pm_qos_request *target_pm_qos = NULL;

		target_freq = target_table_info->ufc_table[col_idx][target_idx];
		if (ufc_need_adjust_freq(target_freq, ufc_dom))
			target_freq = ufc_adjust_freq(target_freq, ufc_dom, ctrl_type);

		switch (ctrl_type) {
		case PM_QOS_MIN_LIMIT:
			target_pm_qos = &ufc_dom->user_min_qos_req;
			emstune_mode_change = true;
			break;

		case PM_QOS_MAX_LIMIT:
			if (target_freq == 0) {
				disable_domain_cpus(ufc_dom);
				continue;
			}
			enable_domain_cpus(ufc_dom);
			target_pm_qos = &ufc_dom->user_max_qos_req;
			break;

		case PM_QOS_MIN_WO_BOOST_LIMIT:
			target_pm_qos = &ufc_dom->user_min_qos_wo_boost_req;
			break;
		}

		if (target_pm_qos)
			pm_qos_update_request(target_pm_qos, target_freq);
	}

	if (emstune_mode_change) {
		int level = target_table_info->ufc_table[UFC_COL_EMSTUNE][target_idx];

		emstune_update_request(&emstune_req_ufc, level);
		ucc_requested_val = level;
		ucc_update_request(&ucc_req, ucc_requested_val);
	}
}

static ssize_t ufc_show_cpufreq_table(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct ufc_table_info *table_info;
	ssize_t count = 0;
	int r_idx;

	table_info = list_first_entry(&ufc.ufc_table_list, struct ufc_table_info, list);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++)
		count += snprintf(&buf[count], 10, "%d ", table_info->ufc_table[0][r_idx]);

	return count - 1;
}

static ssize_t ufc_show_cpufreq_max_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", ufc.last_max_input);
}

static ssize_t ufc_show_cpufreq_min_limit(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", ufc.last_min_input);
}

static ssize_t ufc_show_cpufreq_min_limit_wo_boost(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", ufc.last_min_wo_boost_input);
}

static ssize_t ufc_show_execution_mode_change(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.sse_mode);
}

static ssize_t ufc_store_cpufreq_min_limit(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	/* Save the input for sse change */
	ufc.last_min_input = input;

	ufc_update_limit(input, PM_QOS_MIN_LIMIT, ufc.sse_mode);

	return count;
}

static ssize_t ufc_store_cpufreq_min_limit_wo_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	/* Save the input for sse change */
	ufc.last_min_wo_boost_input = input;

	ufc_update_limit(input, PM_QOS_MIN_WO_BOOST_LIMIT, ufc.sse_mode);

	return count;
}

static ssize_t ufc_store_cpufreq_max_limit(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	/* Save the input for sse change */
	ufc.last_max_input = input;

	ufc_update_limit(input, PM_QOS_MAX_LIMIT, ufc.sse_mode);

	return count;
}

static ssize_t ufc_store_execution_mode_change(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input;
	int prev_mode;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	prev_mode = ufc.sse_mode;
	ufc.sse_mode = !!input;

	if (prev_mode != ufc.sse_mode) {
		ufc_update_limit(ufc.last_max_input, PM_QOS_MAX_LIMIT, ufc.sse_mode);
		ufc_update_limit(ufc.last_min_input, PM_QOS_MIN_LIMIT, ufc.sse_mode);
		ufc_update_limit(ufc.last_min_wo_boost_input,
				PM_QOS_MIN_WO_BOOST_LIMIT, ufc.sse_mode);
	}

	return count;
}

static ssize_t show_cstate_control(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ucc_requested);
}

static ssize_t store_cstate_control(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	input = !!input;
	if (input == ucc_requested)
		goto out;

	ucc_requested = input;

	if (ucc_requested)
		ucc_add_request(&ucc_req, ucc_requested_val);
	else
		ucc_remove_request(&ucc_req);

out:
	return count;
}

static struct kobj_attribute cpufreq_table =
	__ATTR(cpufreq_table, 0444, ufc_show_cpufreq_table, NULL);
static struct kobj_attribute cpufreq_min_limit =
	__ATTR(cpufreq_min_limit, 0644,
		ufc_show_cpufreq_min_limit, ufc_store_cpufreq_min_limit);
static struct kobj_attribute cpufreq_min_limit_wo_boost =
	__ATTR(cpufreq_min_limit_wo_boost, 0644,
		ufc_show_cpufreq_min_limit_wo_boost, ufc_store_cpufreq_min_limit_wo_boost);
static struct kobj_attribute cpufreq_max_limit =
	__ATTR(cpufreq_max_limit, 0644,
		ufc_show_cpufreq_max_limit, ufc_store_cpufreq_max_limit);
static struct kobj_attribute execution_mode_change =
	__ATTR(execution_mode_change, 0644,
		ufc_show_execution_mode_change, ufc_store_execution_mode_change);
static struct kobj_attribute cstate_control =
	__ATTR(cstate_control, 0644, show_cstate_control, store_cstate_control);

/*********************************************************************
 *                          INIT FUNCTION                          *
 *********************************************************************/

static int __init init_sysfs_debug(void)
{
	int postfix_mode = 0;
	int ret = 0;
	char postfix_ctrltype[20];
	struct ufc_table_info *table_info;

	list_for_each_entry(table_info, &ufc.ufc_table_list, list) {
		switch (table_info->mode) {
			case AARCH64_MODE:
				postfix_mode = 64;
				break;
			case AARCH32_MODE:
				postfix_mode = 32;
				break;
			default :
				return -ENODEV;
		}

		switch (table_info->ctrl_type) {
			case PM_QOS_MIN_LIMIT:
				strcpy(postfix_ctrltype, "min");
				break;
			case PM_QOS_MAX_LIMIT:
				strcpy(postfix_ctrltype, "max");
				break;
			case PM_QOS_MIN_WO_BOOST_LIMIT:
				strcpy(postfix_ctrltype, "min_wo_limit");
				break;
			default :
				return -ENODEV;
		}

		ret = kobject_init_and_add(&table_info->kobj, &ktype_ufc,
				power_kobj, "ufc_debug_%s_%d",
				postfix_ctrltype, postfix_mode);
		if (ret) {
			pr_err("UFC: failed to init ufc.kobj\n");
			return -EINVAL;
		}
	}

	return ret;
}

static void ufc_free_all(void)
{
	struct ufc_table_info *table_info;
	struct ufc_domain *ufc_dom;
	int col_idx;

	pr_err("Failed: can't initialize ufc table and domain");

	while(!list_empty(&ufc.ufc_table_list)) {
		table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);
		list_del(&table_info->list);

		for (col_idx = 0; table_info->ufc_table[col_idx]; col_idx++)
			kfree(table_info->ufc_table[col_idx]);

		kfree(table_info->ufc_table);
		kfree(table_info);
	}

	while(!list_empty(&ufc.ufc_domain_list)) {
		ufc_dom = list_first_entry(&ufc.ufc_domain_list,
				struct ufc_domain, list);
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
	}
}

static __init int ufc_init_pm_qos(void)
{
	struct cpufreq_policy *policy;
	struct ufc_domain *ufc_dom;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));

		if (!policy)
			return -EINVAL;
		pm_qos_add_request(&ufc_dom->user_min_qos_req,
				ufc_dom->pm_qos_min_class, policy->user_policy.min);
		pm_qos_add_request(&ufc_dom->user_max_qos_req,
				ufc_dom->pm_qos_max_class, policy->user_policy.max);
		pm_qos_add_request(&ufc_dom->user_min_qos_wo_boost_req,
				ufc_dom->pm_qos_min_class, policy->user_policy.min);
	}

	/* Success */
	return 0;
}

static __init int ufc_init_sysfs(void)
{
	int ret = 0;

	ret = sysfs_create_file(power_kobj, &cpufreq_table.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(power_kobj, &cpufreq_min_limit.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(power_kobj, &cpufreq_min_limit_wo_boost.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(power_kobj, &cpufreq_max_limit.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(power_kobj, &execution_mode_change.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(power_kobj, &cstate_control.attr);
	if (ret)
		return ret;

	ret = init_sysfs_debug();
	if (ret)
		return ret;

	return ret;
}

static int ufc_parse_init_table(struct device_node *dn,
		struct ufc_table_info *ufc_info, struct cpufreq_policy *policy)
{
	int col_idx, row_idx, row_backup;
	int ret;
	struct cpufreq_frequency_table *pos;

	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		for (row_idx = 0; row_idx < ufc.table_row; row_idx++) {
			ret = of_property_read_u32_index(dn, "table",
					(col_idx + (row_idx * ufc.table_col)),
					&(ufc_info->ufc_table[col_idx][row_idx]));
			if (ret)
				return -EINVAL;
		}
	}

	if (!policy)
		return 0;

	cpufreq_for_each_entry(pos, policy->freq_table) {
		if (pos->frequency > policy->max)
			continue;
	}

	row_backup = row_idx;
	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		row_idx = row_backup;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency > policy->max)
				continue;

			if (col_idx == UFC_COL_VFREQ)
				ufc_info->ufc_table[col_idx][row_idx++]
						= pos->frequency / SCALE_SIZE;
			else if (col_idx == UFC_COL_LIT)
				ufc_info->ufc_table[col_idx][row_idx++]
						= pos->frequency;
			else
				ufc_info->ufc_table[col_idx][row_idx++] = 0;
		}
	}

	/* Success */
	return 0;
}

static int init_ufc_table(struct device_node *dn)
{
	struct ufc_table_info *table_info;
	int col_idx;

	table_info = kzalloc(sizeof(struct ufc_table_info), GFP_KERNEL);
	if (!table_info)
		return -ENOMEM;

	list_add_tail(&table_info->list, &ufc.ufc_table_list);

	if (of_property_read_u32(dn, "ctrl-type", &table_info->ctrl_type))
		return -EINVAL;

	if (of_property_read_u32(dn, "sse-mode", &table_info->mode))
		return -EINVAL;

	table_info->cur_index = 0;

	table_info->ufc_table = kzalloc(sizeof(u32 *) * ufc.table_col, GFP_KERNEL);
	if (!table_info->ufc_table)
		return -ENOMEM;

	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		table_info->ufc_table[col_idx] = kzalloc(sizeof(u32) *
				(ufc.table_row + ufc.lit_table_row), GFP_KERNEL);

		if (!table_info->ufc_table[col_idx])
			return -ENOMEM;
	}

	ufc_parse_init_table(dn, table_info, little_policy);

	/* Success */
	return 0;
}

static int init_ufc_domain(struct device_node *dn)
{
	struct ufc_domain *ufc_dom;
	struct cpufreq_policy *policy;
	const char *buf;

	ufc_dom = kzalloc(sizeof(struct ufc_domain), GFP_KERNEL);
	if (!ufc_dom)
		return -ENOMEM;

	list_add(&ufc_dom->list, &ufc.ufc_domain_list);

	if (of_property_read_string(dn, "shared-cpus", &buf))
		return -EINVAL;

	cpulist_parse(buf, &ufc_dom->cpus);
	cpumask_and(&ufc_dom->cpus, &ufc_dom->cpus, cpu_online_mask);
	if (cpumask_empty(&ufc_dom->cpus)) {
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
		return 0;
	}

	if (of_property_read_u32(dn, "table-idx", &ufc_dom->table_idx))
		return -EINVAL;

	if (of_property_read_u32(dn, "pm_qos-min-class", &ufc_dom->pm_qos_min_class))
		return -EINVAL;

	if (of_property_read_u32(dn, "pm_qos-max-class", &ufc_dom->pm_qos_max_class))
		return -EINVAL;

	policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
	if (!policy)
		return -EINVAL;

	ufc_dom->min_freq = policy->min;
	ufc_dom->max_freq = policy->max;

	if (ufc.fill_flag == NOT_FILL)
		return 0;

	if (ufc_dom->pm_qos_min_class == PM_QOS_CLUSTER0_FREQ_MIN) {
		struct cpufreq_frequency_table *pos;
		int lit_row = 0;

		little_policy = policy;

		cpufreq_for_each_entry(pos, little_policy->freq_table) {
			if (pos->frequency > little_policy->max)
				continue;

			lit_row++;
		}

		ufc.lit_table_row = lit_row;
	}

	/* Success */
	return 0;
}

static int init_ufc(struct device_node *dn)
{
	INIT_LIST_HEAD(&ufc.ufc_domain_list);
	INIT_LIST_HEAD(&ufc.ufc_table_list);

	ufc.table_row = ufc_get_table_row(dn);
	if (ufc.table_row < 0)
		return -EINVAL;

	ufc.table_col = ufc_get_table_col(dn);
	if (ufc.table_col < 0)
		return -EINVAL;

	if (of_property_read_u32(dn, "fill-flag", &ufc.fill_flag))
		return -EINVAL;

	ufc.sse_mode = 0;

	return 0;
}

void __init exynos_ufc_init(void)
{
	struct device_node *dn = NULL;

	dn = of_find_node_by_type(dn, "cpufreq_ufc");
	if (!dn) {
		pr_err("exynos-ufc: Failed to find cpufreq_ufc node\n");
		return;
	}

	if (init_ufc(dn)) {
		pr_err("exynos-ufc: Failed to init init_ufc\n");
		return;
	}

	while((dn = of_find_node_by_type(dn, "ufc_domain"))) {
		if (init_ufc_domain(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_domain\n");
			ufc_free_all();
			return;
		}
	}

	while((dn = of_find_node_by_type(dn, "ufc_table"))) {
		if (init_ufc_table(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_table\n");
			ufc_free_all();
			return;
		}
	}

	if (ufc_init_sysfs()){
		pr_err("exynos-ufc: Failed to init sysfs\n");
		ufc_free_all();
		return;
	}

	if (ufc_init_pm_qos()) {
		pr_err("exynos-ufc: Failed to init pm_qos\n");
		ufc_free_all();
		return;
	}

	if (exynos_cpuhp_register("UFC", *cpu_online_mask, 0)) {
		pr_err("exynos-ufc: Failed to register cpuhp\n");
		ufc_free_all();
		return;
	}

	pr_info("exynos-ufc: Complete UFC driver initialization\n");
}

static int __init exynos_ufc_emstune_init(void)
{
	emstune_add_request(&emstune_req_ufc);

	return 0;
}
late_initcall(exynos_ufc_emstune_init);
