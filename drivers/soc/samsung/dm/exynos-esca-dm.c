/* linux/drivers/soc/samsung/exynos-dm.c
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung Exynos SoC series DVFS Manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <linux/sched/clock.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-cpupm.h>
#include <linux/delay.h>

#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/cal-if.h>

#include "../acpm/acpm.h"
#include "../acpm/acpm_ipc.h"
#include "../cal-if/acpm_dvfs.h"

#define DM_EMPTY	0xFF
static struct exynos_dm_device *exynos_dm;

static int dm_idle_ip_index;
static int dm_fast_switch_idle_ip_index;
static spinlock_t fast_switch_glb_lock;

void exynos_dm_dynamic_disable(int flag);

/* ACPM API */
static void __exynos_acpm_dm_ipc_send_data(union dm_ipc_message *message,
		unsigned int ipc_ch, bool response)
{
	struct ipc_config config;
	int ret;

	config.cmd = message->data;
	config.response = response;
	config.indirection = false;

	ret = esca_ipc_send_data(ipc_ch, &config);
}

static inline void exynos_acpm_dm_ipc_send_data(union dm_ipc_message *message)
{
	__exynos_acpm_dm_ipc_send_data(message, exynos_dm->dm_ch, true);
}

static inline void exynos_acpm_dm_ipc_send_data_async(union dm_ipc_message *message)
{
	__exynos_acpm_dm_ipc_send_data(message, exynos_dm->dm_req_ch, false);
}

static void exynos_acpm_dm_set_freq_async(int dm_type, unsigned int target_freq)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(union dm_ipc_message));
	message.freq_req.msg = DM_FREQ_REQ;
	message.freq_req.dm_type = dm_type;
	message.freq_req.target_freq = target_freq;
	message.freq_req.start_time = acpm_get_peri_timer();
	exynos_acpm_dm_ipc_send_data_async(&message);
}

static void exynos_acpm_dm_set_freq_sync(int dm_type, unsigned int target_freq)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(union dm_ipc_message));
	message.freq_req.msg = DM_FREQ_REQ;
	message.freq_req.dm_type = dm_type;
	message.freq_req.target_freq = target_freq;
	message.freq_req.start_time = acpm_get_peri_timer();
	exynos_acpm_dm_ipc_send_data(&message);
}

static void exynos_acpm_dm_set_const_table(int dm_type, struct exynos_dm_constraint *constraint)
{
	union dm_ipc_message message;
	int i, j;

	memset(&message, 0, sizeof(message));
	message.const_req.msg = DM_REG_CONST_START;
	message.const_req.dm_type = dm_type;
	message.const_req.slave_dm_type = constraint->dm_slave;
	message.const_req.constraint_type = constraint->constraint_type;
	message.const_req.guidance = constraint->guidance;
	message.const_req.support_dynamic_disable = constraint->support_dynamic_disable;
	message.const_req.support_variable_freq_table = constraint->support_variable_freq_table;
	message.const_req.num_table_index = constraint->num_table_index;
	message.const_req.current_table_idx = constraint->current_table_idx;
	message.const_req.table_length = constraint->table_length;
	exynos_acpm_dm_ipc_send_data(&message);

	for (i = 0; i < constraint->num_table_index; i++) {
		for (j = 0; j < constraint->table_length; j++) {
			memset(&message, 0, sizeof(message));
			message.const_req.msg = DM_REG_CONST_FREQ;
			message.const_req.dm_type = dm_type;
			message.const_req.slave_dm_type = constraint->dm_slave;
			message.const_req.table_length = j;
			message.const_req.table_idx = i;
			message.const_req.master_freq = constraint->variable_freq_table[i][j].master_freq;
			message.const_req.slave_freq = constraint->variable_freq_table[i][j].slave_freq;
			exynos_acpm_dm_ipc_send_data(&message);
		}
	}

	memset(&message, 0, sizeof(message));
	message.const_req.msg = DM_REG_CONST_END;
	message.const_req.dm_type = dm_type;
	message.const_req.slave_dm_type = constraint->dm_slave;
	exynos_acpm_dm_ipc_send_data(&message);

	constraint->constraint_id = message.const_req.slave_dm_type;
}

static void exynos_acpm_dm_set_policy(int dm_type, u32 min_freq, u32 max_freq)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(message));
	message.policy_req.msg = DM_POLICY_UPDATE;
	message.policy_req.dm_type = dm_type;
	message.policy_req.min_freq = min_freq;
	message.policy_req.max_freq = max_freq;
	exynos_acpm_dm_ipc_send_data(&message);
}

static void exynos_acpm_dm_set_dynamic_disable(u32 status)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(message));
	message.req.msg = DM_DYNAMIC_DISABLE;
	message.req.dm_type = 0;
	message.req.req_rsvd0 = status;
	exynos_acpm_dm_ipc_send_data(&message);

	exynos_dm->dynamic_disable = message.resp.resp_rsvd0;
}

static void exynos_acpm_dm_change_freq_table(int constraint_id, u32 table_idx)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(message));
	message.req.msg = DM_CHANGE_FREQ_TABLE;
	message.req.dm_type = constraint_id;
	message.req.req_rsvd0 = table_idx;
	exynos_acpm_dm_ipc_send_data(&message);
}

static void exynos_acpm_dm_set_dm_init(int dm_type, u32 min_freq, u32 max_freq, u32 cur_freq)
{
	union dm_ipc_message message;

	memset(&message, 0, sizeof(message));
	message.init_req.msg = DM_DATA_INIT;
	message.init_req.dm_type = dm_type;
	message.init_req.min_freq = min_freq;
	message.init_req.max_freq = max_freq;
	message.init_req.cur_freq = cur_freq;

	exynos_acpm_dm_ipc_send_data(&message);
}

static void exynos_acpm_dm_get_policy(int dm_type, 
		struct exynos_dm_domain_policy *policy)
{
	union dm_ipc_message message;
	int i;

	memset(&message, 0, sizeof(union dm_ipc_message));
	message.req.msg = DM_START_GET_DOMAIN_POLICY;
	message.req.dm_type = dm_type;
	exynos_acpm_dm_ipc_send_data(&message);

	policy->dm_type = dm_type;
	policy->policy_min = message.get_policy_resp.values[1] * 1000;
	policy->policy_max = message.get_policy_resp.values[2] * 1000;
	policy->const_min = message.get_policy_resp.values[3] * 1000;
	policy->const_max = message.get_policy_resp.values[4] * 1000;

	// Parse domain status
	policy->num_const = message.get_policy_resp.values[0];
	policy->const_policy = kcalloc(policy->num_const,
			sizeof(struct exynos_dm_domain_constraint_policy), GFP_KERNEL);

	memset(&message, 0, sizeof(union dm_ipc_message));
	message.req.msg = DM_CONTINUE_GET_DOMAIN_POLICY;
	message.req.dm_type = dm_type;
	exynos_acpm_dm_ipc_send_data(&message);

	policy->gov_min = message.get_policy_resp.values[0] * 1000;
	policy->governor_freq = message.get_policy_resp.values[1] * 1000;
	policy->cur_freq = message.get_policy_resp.values[2] * 1000;

	for (i = 0; i < policy->num_const; i++) {
		memset(&message, 0, sizeof(union dm_ipc_message));
		message.get_policy_req.msg = DM_GET_DOMAIN_CONST_POLICY;
		message.get_policy_req.dm_type = dm_type;
		message.get_policy_req.index = i;
		exynos_acpm_dm_ipc_send_data(&message);

		policy->const_policy[i].type = (message.get_policy_resp.dm_type & 0x80) >> 7;
		policy->const_policy[i].guidance = (message.get_policy_resp.dm_type & 0x40) >> 6;
		policy->const_policy[i].master_dm_type = (message.get_policy_resp.dm_type) & 0x3f;
		policy->const_policy[i].policy_minmax = message.get_policy_resp.values[0] * 1000;
		policy->const_policy[i].const_minmax = message.get_policy_resp.values[1] * 1000;
		policy->const_policy[i].const_freq = message.get_policy_resp.values[2] * 1000;
		policy->const_policy[i].gov_min = message.get_policy_resp.values[3] * 1000;
		policy->const_policy[i].governor_freq = message.get_policy_resp.values[4] * 1000;
		policy->const_policy[i].gov_freq = message.get_policy_resp.values[5] * 1000;
	}
}

static void exynos_acpm_dm_get_constraint_table(int dm_type,
		struct exynos_dm_domain_constraint *domain_constraint)
{
	union dm_ipc_message message;
	int i, j, k;

	memset(&message, 0, sizeof(union dm_ipc_message));
	message.req.msg = DM_GET_DM_CONST_INFO;
	message.req.dm_type = dm_type;
	exynos_acpm_dm_ipc_send_data(&message);

	/* Get the number of constraints */
	domain_constraint->num_constraints = message.domain_const_info.num_constraints;

	domain_constraint->constraint = kcalloc(domain_constraint->num_constraints,
			sizeof(struct exynos_dm_constraint), GFP_KERNEL);

	for (i = 0; i < domain_constraint->num_constraints; i++) {
		struct exynos_dm_constraint *constraint = &domain_constraint->constraint[i];
		/* Get the data for each constraint condition between two domains */
		memset(&message, 0, sizeof(union dm_ipc_message));
		message.const_table_info.msg = DM_GET_DM_CONST_TABLE_INFO;
		message.const_table_info.dm_type = dm_type;
		message.const_table_info.constraint_idx = i;
		exynos_acpm_dm_ipc_send_data(&message);

		constraint->num_table_index = message.const_table_info.num_table_index;
		constraint->table_length = message.const_table_info.table_length;
		constraint->guidance = message.const_table_info.guidance;
		constraint->support_dynamic_disable = message.const_table_info.support_dynamic_disable;
		constraint->support_variable_freq_table = message.const_table_info.support_variable_freq_table;
		constraint->current_table_idx = message.const_table_info.current_table_idx;
		constraint->dm_slave = message.const_table_info.slave_dm_type;
		constraint->const_freq = message.const_table_info.const_freq;
		constraint->gov_freq = message.const_table_info.gov_freq;
		constraint->dm_master = dm_type;

		constraint->variable_freq_table = kcalloc(constraint->num_table_index,
				sizeof(struct exynos_dm_freq *), GFP_KERNEL);
		for (j = 0; j < constraint->num_table_index; j++) {
			struct exynos_dm_freq *freq_table;

			constraint->variable_freq_table[j] = kcalloc(constraint->table_length,
					sizeof(struct exynos_dm_freq), GFP_KERNEL);
			freq_table = constraint->variable_freq_table[j];

			for (k = 0; k < constraint->table_length; k++) {
				memset(&message, 0, sizeof(union dm_ipc_message));
				message.const_table_entry.dm_type = dm_type;
				message.const_table_entry.msg = DM_GET_DM_CONST_TABLE_ENTRY;
				message.const_table_entry.const_idx = i;
				message.const_table_entry.table_idx = j;
				message.const_table_entry.entry_idx = k;
				exynos_acpm_dm_ipc_send_data(&message);

				freq_table[k].master_freq = message.const_table_entry.master_freq;
				freq_table[k].slave_freq = message.const_table_entry.slave_freq;
			}
		}
	}
}

static struct exynos_dm_domain_constraint *exynos_dm_get_constraint_info(int dm_type)
{
	struct exynos_dm_domain_constraint *constraint = kzalloc(sizeof(struct exynos_dm_domain_constraint), GFP_KERNEL);

	exynos_acpm_dm_get_constraint_table(dm_type, constraint);

	return constraint;
}

static void exynos_dm_free_constraint_info(struct exynos_dm_domain_constraint *domain_constraint)
{
	int i, j;

	if (!domain_constraint)
		return;

	for (i = 0; i < domain_constraint->num_constraints; i++) {
		struct exynos_dm_constraint *constraint = &domain_constraint->constraint[i];
		for (j = 0; j < constraint->num_table_index; j++)
			kfree(constraint->variable_freq_table[j]);
		kfree(constraint->variable_freq_table);
	}

	kfree(domain_constraint);
}

static void exynos_dm_freq_sync_callback(unsigned int *cmd, unsigned int size)
{
	union dm_ipc_message *message = (union dm_ipc_message *)cmd;
	u32 start_time, end_time;
	ktime_t duration = 0;
	unsigned int domain;
	unsigned long freq;
	struct exynos_dm_data *data;

	domain = message->sync_resp.dm_type;
	data = &exynos_dm->dm_data[domain];

	if (message->req.msg == DM_FREQ_SYNC) {
		freq = message->sync_resp.target_freq;
		start_time = message->sync_resp.start_time;
		end_time = message->sync_resp.end_time;

		cal_dfs_update_rate(data->cal_id, freq);

		if (data->fast_switch && (start_time || end_time)) {
			duration = acpm_time_calc(start_time, end_time);
//			pr_info("sync_callback: domain %d start %d end %d time %llu\n",
//					domain, start_time, end_time, duration);
		}

		if (data->freq_scaler)
			data->freq_scaler(data->dm_type, data->devdata, freq, duration);
	}
}

static void exynos_dm_fast_switch_callback(unsigned int *cmd, unsigned int size)
{
	u32 domain, freq, start_time, end_time;
	ktime_t time;
	unsigned long flags;
	struct exynos_dm_data *data;

	spin_lock_irqsave(&fast_switch_glb_lock, flags);
	if (!is_esca_ipc_busy(exynos_dm->dm_req_ch) && !is_acpm_ipc_busy(exynos_dm->fast_switch_ch))
		// enable sicd
		exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);
	spin_unlock_irqrestore(&fast_switch_glb_lock, flags);

	domain = cmd[1];
	freq = cmd[2];

	if (!freq)
		return;

	start_time = cmd[3] & 0xFFFF;
	end_time = (cmd[3] >> 16) & 0xFFFF;
	time = acpm_time_calc(start_time, end_time);
	data = &exynos_dm->dm_data[domain];

//	pr_info("%s: domain %d start %d end %d time %llu\n",
//			__func__, domain, start_time, end_time, time);

	if (data->freq_scaler)
		data->freq_scaler(data->dm_type, data->devdata, freq, time);
}

static void exynos_dm_freq_req_callback(unsigned int *cmd, unsigned int size)
{
	union dm_ipc_message *message = (union dm_ipc_message *)cmd;
	unsigned int domain;
	struct exynos_dm_data *data;
	unsigned long flags;

	domain = message->sync_resp.dm_type;
	data = &exynos_dm->dm_data[domain];

	if (message->req.msg == DM_FREQ_REQ) {
		spin_lock_irqsave(&fast_switch_glb_lock, flags);
		if (!is_esca_ipc_busy(exynos_dm->dm_req_ch) && !is_acpm_ipc_busy(exynos_dm->fast_switch_ch))
			exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);

		spin_unlock_irqrestore(&fast_switch_glb_lock, flags);
	}
}

/*
 * SYSFS for Debugging
 */
static ssize_t show_available(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		count += snprintf(buf + count, PAGE_SIZE,
				"dm_type: %d(%s), available = %s\n",
				dm->dm_data[i].dm_type, dm->dm_data[i].dm_type_name,
				dm->dm_data[i].available ? "true" : "false");
	}

	return count;
}

static ssize_t show_constraint_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exynos_dm_domain_constraint *domain_constraint;
	struct exynos_dm_data *dm;
	struct exynos_dm_attrs *dm_attrs;
	ssize_t count = 0;
	int i, j, k;

	dm_attrs = container_of(attr, struct exynos_dm_attrs, attr);
	dm = container_of(dm_attrs, struct exynos_dm_data, constraint_table_attr);

	if (!dm->available) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type is not available\n");
		goto out;
	}

	mutex_lock(&exynos_dm->lock);

	domain_constraint = exynos_dm_get_constraint_info(dm->dm_type);

	if (!domain_constraint) {
		count += snprintf(buf + count, PAGE_SIZE,
				"Cannot get domain constraint informations\n");
		goto out;
	}

	count += snprintf(buf + count, PAGE_SIZE, "dm_type: %s\n",
				dm->dm_type_name);

	if (!domain_constraint->num_constraints) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type have not constraint tables\n\n");
		goto out_free;
	}

	for (i = 0; i < domain_constraint->num_constraints; i++) {
		struct exynos_dm_constraint *constraint = &domain_constraint->constraint[i];
		struct exynos_dm_freq *freq_table;

		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
		count += snprintf(buf + count, PAGE_SIZE,
				"constraint_dm_type = %s\n", exynos_dm->dm_data[constraint->dm_slave].dm_type_name);
		count += snprintf(buf + count, PAGE_SIZE, "constraint_type: %s\n",
				constraint->constraint_type ? "MAX" : "MIN");
		count += snprintf(buf + count, PAGE_SIZE, "guidance: %s\n",
				constraint->guidance ? "true" : "false");
		count += snprintf(buf + count, PAGE_SIZE,
				"const_freq = %u, gov_freq = %u\n",
				constraint->const_freq, constraint->gov_freq);		\
		count += snprintf(buf + count, PAGE_SIZE,
				"support_variable_freq_table: %s, num_tables = %u, current_table = %u\n",
				constraint->support_variable_freq_table ? "true" : "false",
				constraint->num_table_index, constraint->current_table_idx);		\
		for (j = 0; j < constraint->num_table_index; j++) {
			count += snprintf(buf + count, PAGE_SIZE,
					"-------------------------------------------------\n");
			count += snprintf(buf + count, PAGE_SIZE,
					"freq_table_id: %d\n", j);
			count += snprintf(buf + count, PAGE_SIZE,
					"master_freq\t constraint_freq\n");
			freq_table = constraint->variable_freq_table[j];
			for (k = 0; k < constraint->table_length; k++)
				count += snprintf(buf + count, PAGE_SIZE, "%10u\t %10u\n",
						freq_table[k].master_freq,
						freq_table[k].slave_freq);
			count += snprintf(buf + count, PAGE_SIZE,
					"-------------------------------------------------\n");
		}
		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
	}

out_free:
	exynos_dm_free_constraint_info(domain_constraint);

out:
	mutex_unlock(&exynos_dm->lock);
	return count;
}

static ssize_t show_dm_policy(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exynos_dm_data *dm;
	struct exynos_dm_attrs *dm_attrs;
	struct exynos_dm_domain_policy policy;
	ssize_t count = 0;
	u32 find = 0;
	int i;

	dm_attrs = container_of(attr, struct exynos_dm_attrs, attr);
	dm = container_of(dm_attrs, struct exynos_dm_data, dm_policy_attr);

	if (!dm->available) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type is not available\n");
		return count;
	}

	mutex_lock(&exynos_dm->lock);

	exynos_acpm_dm_get_policy(dm->dm_type, &policy);

	count += snprintf(buf + count, PAGE_SIZE, "dm_type: %s\n",
				dm->dm_type_name);

	count += snprintf(buf + count, PAGE_SIZE,
			"policy_min = %u, policy_max = %u\n",
			policy.policy_min, policy.policy_max);
	count += snprintf(buf + count, PAGE_SIZE,
			"const_min = %u, const_max = %u\n",
			policy.const_min, policy.const_max);
	count += snprintf(buf + count, PAGE_SIZE,
			"gov_min = %u, governor_freq = %u\n", policy.gov_min, policy.governor_freq);
	count += snprintf(buf + count, PAGE_SIZE, "current_freq = %u\n", policy.cur_freq);
	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");

	if (policy.num_const) {
		count += snprintf(buf + count, PAGE_SIZE, "min constraint by\n");

		for (i = 0; i < policy.num_const; i++) {
			if (policy.const_policy[i].type == CONSTRAINT_MAX)
				continue;

			count += snprintf(buf + count, PAGE_SIZE,
					"%s ---> %s\n"
					"policy_min(%u), const_min(%u) ---> const_freq(%u)\n"
					"gov_min(%u), gov_freq(%u) ---> gov_freq(%u)\n",
					exynos_dm->dm_data[policy.const_policy[i].master_dm_type].dm_type_name,
					dm->dm_type_name,
					policy.const_policy[i].policy_minmax,
					policy.const_policy[i].const_minmax,
					policy.const_policy[i].const_freq,
					policy.const_policy[i].gov_min,
					policy.const_policy[i].governor_freq,
					policy.const_policy[i].gov_freq);
			if (policy.const_policy[i].guidance)
				count += snprintf(buf+count, PAGE_SIZE,
						" [guidance]\n");
			else
				count += snprintf(buf+count, PAGE_SIZE, "\n");
			find = max(find, policy.const_policy[i].const_freq);
		}

		count += snprintf(buf + count, PAGE_SIZE,
				"min constraint freq = %u\n", find);
	} else {
		count += snprintf(buf + count, PAGE_SIZE,
				"There is no min constraint\n\n");
	}

	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");
	count += snprintf(buf + count, PAGE_SIZE, "max constraint by\n");
	find = INT_MAX;

	if (policy.num_const) {
		for (i = 0; i < policy.num_const; i++) {
			if (policy.const_policy[i].type == CONSTRAINT_MIN)
				continue;

			count += snprintf(buf + count, PAGE_SIZE,
					"%s ---> %s\n"
					"policy_max(%u), const_max(%u) ---> const_freq(%u)\n",
					exynos_dm->dm_data[policy.const_policy[i].master_dm_type].dm_type_name,
					dm->dm_type_name,
					policy.const_policy[i].policy_minmax,
					policy.const_policy[i].const_minmax,
					policy.const_policy[i].const_freq);
			if (policy.const_policy[i].guidance)
				count += snprintf(buf+count, PAGE_SIZE,
						" [guidance]\n");
			else
				count += snprintf(buf+count, PAGE_SIZE, "\n");
			find = min(find, policy.const_policy[i].const_freq);
		}

		count += snprintf(buf + count, PAGE_SIZE,
				"max constraint freq = %u\n", find);
	} else {
		count += snprintf(buf + count, PAGE_SIZE,
				"There is no max constraint\n\n");
	}
	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");

	if (policy.num_const)
		kfree(policy.const_policy);

	mutex_unlock(&exynos_dm->lock);

	return count;
}

static ssize_t show_dynamic_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE,
			"%s\n", dm->dynamic_disable ? "true" : "false");

	return count;
}

static ssize_t store_dynamic_disable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int flag, ret = 0;

	ret = sscanf(buf, "%u", &flag);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&exynos_dm->lock);
	exynos_dm_dynamic_disable(flag);
	mutex_unlock(&exynos_dm->lock);

	return count;
}

static ssize_t show_dm_freq_scaler(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	struct exynos_dm_data *data;
	ssize_t count = 0;
	int i;

	count += snprintf(buf + count, PAGE_SIZE,
			"DomainName(ID):     Mode\n");
	for (i = 0; i < dm->domain_count; i++) {
		data = &dm->dm_data[i];

		if (!data->available)
			continue;
		count += snprintf(buf + count, PAGE_SIZE,
				"%10s(%2d): %8s\n", data->dm_type_name, data->dm_type, data->freq_scaler ? "enabled" : "disabled");
	}

	count += snprintf(buf + count, PAGE_SIZE,
			"Usage: echo [id] [mode] > freq_scaler (mode = 0/1)\n");

	return count;
}

static ssize_t store_dm_freq_scaler(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int domain, flag, ret = 0;
	struct exynos_dm_data *data;
	static int (**scaler_func_list)(int, void*, u32, unsigned int) = NULL;

	if (!scaler_func_list)
		scaler_func_list = kzalloc(sizeof(*scaler_func_list) * exynos_dm->domain_count, GFP_KERNEL);

	ret = sscanf(buf, "%u %u", &domain, &flag);
	if (ret != 2)
		return -EINVAL;

	if (domain < 0 || domain >= exynos_dm->domain_count)
		return count;

	data = &exynos_dm->dm_data[domain];

	if (data->freq_scaler)
		scaler_func_list[domain] = data->freq_scaler;

	if (!flag && data->freq_scaler) {
		unregister_exynos_dm_freq_scaler(domain);
	} else if (flag && !data->freq_scaler) {
		register_exynos_dm_freq_scaler(domain, scaler_func_list[domain]);
	}

	return count;
}

static ssize_t show_fast_switch(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	struct exynos_dm_data *data;
	ssize_t count = 0;
	int i;

	count += snprintf(buf + count, PAGE_SIZE,
			"DomainName(ID):     Mode\n");
	for (i = 0; i < dm->domain_count; i++) {
		data = &dm->dm_data[i];

		if (!data->available)
			continue;
		count += snprintf(buf + count, PAGE_SIZE,
				"%10s(%2d): %8s\n", data->dm_type_name, data->dm_type, data->fast_switch ? "enabled" : "disabled");
	}

	count += snprintf(buf + count, PAGE_SIZE,
			"Usage: echo [id] [mode] > fast_switch (mode = 0/1)\n");

	return count;
}

static ssize_t store_fast_switch(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int domain, flag, ret = 0;
	struct exynos_dm_data *data;

	ret = sscanf(buf, "%u %u", &domain, &flag);
	if (ret != 2)
		return -EINVAL;

	if (domain < 0 || domain >= exynos_dm->domain_count)
		return count;

	data = &exynos_dm->dm_data[domain];

	mutex_lock(&exynos_dm->lock);
	data->fast_switch = !!flag;
	mutex_unlock(&exynos_dm->lock);

	return count;
}

static DEVICE_ATTR(available, 0440, show_available, NULL);
static DEVICE_ATTR(dynamic_disable, 0640, show_dynamic_disable, store_dynamic_disable);
static DEVICE_ATTR(fast_switch, 0640, show_fast_switch, store_fast_switch);
static DEVICE_ATTR(dm_freq_scaler, 0640, show_dm_freq_scaler, store_dm_freq_scaler);

static struct attribute *exynos_dm_sysfs_entries[] = {
	&dev_attr_available.attr,
	&dev_attr_dynamic_disable.attr,
	&dev_attr_fast_switch.attr,
	&dev_attr_dm_freq_scaler.attr,
	NULL,
};

static struct attribute_group exynos_dm_attr_group = {
	.name	= "exynos_dm",
	.attrs	= exynos_dm_sysfs_entries,
};
/*
 * SYSFS for Debugging end
 */


void exynos_dm_dynamic_disable(int flag)
{
	mutex_lock(&exynos_dm->lock);

	exynos_acpm_dm_set_dynamic_disable(!!flag);

	mutex_unlock(&exynos_dm->lock);
}
EXPORT_SYMBOL(exynos_dm_dynamic_disable);

static void print_available_dm_data(struct exynos_dm_device *dm)
{
	int i;

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		dev_info(dm->dev, "dm_type: %d(%s), available = %s\n",
				dm->dm_data[i].dm_type, dm->dm_data[i].dm_type_name,
				dm->dm_data[i].available ? "true" : "false");
	}
}

static int exynos_dm_index_validate(int index)
{
	if (index < 0) {
		dev_err(exynos_dm->dev, "invalid dm_index (%d)\n", index);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int exynos_dm_parse_dt(struct device_node *np, struct exynos_dm_device *dm)
{
	struct device_node *child_np, *domain_np = NULL;
	const char *name;
	int ret = 0;

	if (!np)
		return -ENODEV;

	domain_np = of_get_child_by_name(np, "dm_domains");
	if (!domain_np)
		return -ENODEV;

	dm->domain_count = of_get_child_count(domain_np);
	if (!dm->domain_count)
		return -ENODEV;

	dm->dm_data = kzalloc(sizeof(struct exynos_dm_data) * dm->domain_count, GFP_KERNEL);
	if (!dm->dm_data) {
		dev_err(dm->dev, "failed to allocate dm_data\n");
		return -ENOMEM;
	}
	dm->domain_order = kzalloc(sizeof(int) * dm->domain_count, GFP_KERNEL);
	if (!dm->domain_order) {
		dev_err(dm->dev, "failed to allocate domain_order\n");
		return -ENOMEM;
	}

	for_each_child_of_node(domain_np, child_np) {
		int index;
		const char *available;

		if (of_property_read_u32(child_np, "dm-index", &index))
			return -ENODEV;

		ret = exynos_dm_index_validate(index);
		if (ret)
			return ret;

		if (of_property_read_string(child_np, "available", &available))
			return -ENODEV;

		if (of_property_read_u32(child_np, "cal_id", &dm->dm_data[index].cal_id))
			return -ENODEV;

		if (!strcmp(available, "true")) {
			dm->dm_data[index].dm_type = index;
			dm->dm_data[index].available = true;

			if (!of_property_read_string(child_np, "dm_type_name", &name))
				strncpy(dm->dm_data[index].dm_type_name, name, EXYNOS_DM_TYPE_NAME_LEN);

			dm->dm_data[index].fast_switch = of_property_read_bool(child_np, "fast_switch");
		} else {
			dm->dm_data[index].available = false;
		}
	}

	// For Kernel-DM plugin request channel
	child_np = of_get_child_by_name(np, "dm_channel");
	if (child_np) {
		int size;

		ret = esca_ipc_request_channel(child_np,
				NULL,
				&dm->dm_ch,
				&size);
	} else {
		pr_err("%s : Failed to get dm_ch\n", __func__);
	}

	// For ACPM DM freq change request channel
	child_np = of_get_child_by_name(np, "dm_req_channel");
	if (child_np) {
		int size;

		ret = esca_ipc_request_channel(child_np,
				exynos_dm_freq_req_callback,
				&dm->dm_req_ch,
				&size);
	} else {
		pr_err("%s : Failed to get dm_req_ch\n", __func__);
	}

	// For ACPM DM freq change sync channel
	child_np = of_get_child_by_name(np, "dm_sync_channel");
	if (child_np) {
		int size;

		ret = esca_ipc_request_channel(child_np,
				exynos_dm_freq_sync_callback,
				&dm->dm_sync_ch,
				&size);
	} else {
		pr_err("%s : Failed to get dm_sync_ch\n", __func__);
	}

	// For FVP direct channel to Fast switch
	child_np = of_get_child_by_name(np, "dm-fast-switch");
	if (child_np) {
		int size;

		ret = acpm_ipc_request_channel(child_np,
				exynos_dm_fast_switch_callback,
				&dm->fast_switch_ch,
				&size);
		exynos_acpm_set_fast_switch(dm->fast_switch_ch);
	} else {
		pr_err("%s : Failed to get dm_sync_ch\n", __func__);
	}

	return ret;
}
#else
static int exynos_dm_parse_dt(struct device_node *np, struct exynos_dm_device *dm)
{
	return -ENODEV;
}
#endif

/*
 * This function should be called from each DVFS drivers
 * before DVFS driver registration to DVFS framework.
 * 	Initialize sequence Step.1
 */
int exynos_dm_data_init(int dm_type, void *data,
			u32 min_freq, u32 max_freq, u32 cur_freq)
{
	struct exynos_dm_data *dm;
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	mutex_lock(&exynos_dm->lock);

	dm = &exynos_dm->dm_data[dm_type];

	if (!dm->available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	dm->devdata = data;

	exynos_acpm_dm_set_dm_init(dm_type, min_freq, max_freq, cur_freq);
out:
	mutex_unlock(&exynos_dm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_dm_data_init);

/*
 * 	Initialize sequence Step.2
 */
int exynos_dm_change_freq_table(struct exynos_dm_constraint *constraint, int idx)
{
	mutex_lock(&exynos_dm->lock);

	exynos_acpm_dm_change_freq_table(constraint->constraint_id, idx);

	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_dm_change_freq_table);

int register_exynos_dm_constraint_table(int dm_type,
				struct exynos_dm_constraint *constraint)
{
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	if (!constraint) {
		dev_err(exynos_dm->dev, "constraint is not valid\n");
		return -EINVAL;
	}

	/* check member invalid */
	if ((constraint->constraint_type < CONSTRAINT_MIN) ||
		(constraint->constraint_type > CONSTRAINT_MAX)) {
		dev_err(exynos_dm->dev, "constraint_type is invalid\n");
		return -EINVAL;
	}

	ret = exynos_dm_index_validate(constraint->dm_slave);
	if (ret)
		return ret;

	if (!constraint->freq_table) {
		if (constraint->support_variable_freq_table)
			constraint->freq_table = constraint->variable_freq_table[0];
		else {
			dev_err(exynos_dm->dev, "No frequency table for constraint\n");
			return -EINVAL;
		}
	} else if (!constraint->support_variable_freq_table) {
		constraint->variable_freq_table = kzalloc(sizeof(struct exynos_dm_freq *), GFP_KERNEL);
		constraint->num_table_index = 1;
		constraint->current_table_idx = 0;
		constraint->variable_freq_table[0] = constraint->freq_table;
	}

	mutex_lock(&exynos_dm->lock);

	exynos_acpm_dm_set_const_table(dm_type, constraint);

	mutex_unlock(&exynos_dm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(register_exynos_dm_constraint_table);

/*
 * This function should be called from each DVFS driver registration function
 * before return to corresponding DVFS drvier.
 * 	Initialize sequence Step.3
 */
int register_exynos_dm_freq_scaler(int dm_type,
			int (*scaler_func)(int dm_type, void *devdata, u32 target_freq, unsigned int relation))
{
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	if (!scaler_func) {
		dev_err(exynos_dm->dev, "function is not valid\n");
		return -EINVAL;
	}

	mutex_lock(&exynos_dm->lock);

	if (!exynos_dm->dm_data[dm_type].available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	if (!exynos_dm->dm_data[dm_type].freq_scaler)
		exynos_dm->dm_data[dm_type].freq_scaler = scaler_func;

out:
	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(register_exynos_dm_freq_scaler);

int unregister_exynos_dm_freq_scaler(int dm_type)
{
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	mutex_lock(&exynos_dm->lock);

	if (!exynos_dm->dm_data[dm_type].available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	if (exynos_dm->dm_data[dm_type].freq_scaler)
		exynos_dm->dm_data[dm_type].freq_scaler = NULL;

out:
	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_exynos_dm_freq_scaler);

/*
 * Policy Updater
 *
 * @dm_type: DVFS domain type for updating policy
 * @min_freq: Minimum frequency decided by policy
 * @max_freq: Maximum frequency decided by policy
 *
 * In this function, policy_min_freq and policy_max_freq will be changed.
 * After that, DVFS Manager will decide min/max freq. of current domain
 * and check dependent domains whether update is necessary.
 */

#define POLICY_REQ	4

int policy_update_call_to_DM(int dm_type, u32 min_freq, u32 max_freq)
{
	u64 pre, before, after;
	s32 time = 0, pre_time = 0;

	dbg_snapshot_dm((int)dm_type, min_freq, max_freq, pre_time, time);

	pre = sched_clock();
	mutex_lock(&exynos_dm->lock);
	before = sched_clock();

	exynos_acpm_dm_set_policy(dm_type, min_freq, max_freq);

	after = sched_clock();
	mutex_unlock(&exynos_dm->lock);

	pre_time = (unsigned int)(before - pre);
	time = (unsigned int)(after - before);

	dbg_snapshot_dm((int)dm_type, min_freq, max_freq, pre_time, time);

	return 0;
}
EXPORT_SYMBOL_GPL(policy_update_call_to_DM);

/*
 * DM CALL
 */
static int exynos_dm_suspend(struct device *dev)
{
	/* Suspend callback function might be registered if necessary */

	return 0;
}

static int exynos_dm_resume(struct device *dev)
{
	/* Resume callback function might be registered if necessary */

	return 0;
}

int DM_CALL(int dm_type, unsigned long *target_freq)
{
	struct exynos_dm_data *data = &exynos_dm->dm_data[dm_type];
	unsigned long flags;

	if (data->fast_switch) {
		spin_lock_irqsave(&fast_switch_glb_lock, flags);
		if (!is_esca_ipc_busy(exynos_dm->dm_req_ch) && !is_acpm_ipc_busy(exynos_dm->fast_switch_ch)) {
			// disable sicd
			exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 0);
		}

		// Call scale callback
		cal_dfs_set_rate_fast(data->cal_id, *target_freq);
		exynos_acpm_dm_set_freq_async(dm_type, *target_freq);

		spin_unlock_irqrestore(&fast_switch_glb_lock, flags);
	} else {
		mutex_lock(&exynos_dm->lock);
		exynos_update_ip_idle_status(dm_idle_ip_index, 0);

		exynos_acpm_dm_set_freq_sync(dm_type, *target_freq);

		exynos_update_ip_idle_status(dm_idle_ip_index, 1);
		mutex_unlock(&exynos_dm->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(DM_CALL);

static int exynos_dm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_dm_device *dm;
	int i;

	dm = kzalloc(sizeof(struct exynos_dm_device), GFP_KERNEL);
	if (dm == NULL) {
		dev_err(&pdev->dev, "failed to allocate DVFS Manager device\n");
		ret = -ENOMEM;
		goto err_device;
	}

	dm->dev = &pdev->dev;

	mutex_init(&dm->lock);

	/* parsing devfreq dts data for exynos-dvfs-manager */
	ret = exynos_dm_parse_dt(dm->dev->of_node, dm);
	if (ret) {
		dev_err(dm->dev, "failed to parse private data\n");
		goto err_parse_dt;
	}

	print_available_dm_data(dm);

	ret = sysfs_create_group(&dm->dev->kobj, &exynos_dm_attr_group);
	if (ret)
		dev_warn(dm->dev, "failed create sysfs for DVFS Manager\n");

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		snprintf(dm->dm_data[i].dm_policy_attr.name, EXYNOS_DM_ATTR_NAME_LEN,
				"dm_policy_%s", dm->dm_data[i].dm_type_name);
		sysfs_attr_init(&dm->dm_data[i].dm_policy_attr.attr.attr);
		dm->dm_data[i].dm_policy_attr.attr.attr.name =
			dm->dm_data[i].dm_policy_attr.name;
		dm->dm_data[i].dm_policy_attr.attr.attr.mode = (S_IRUSR | S_IRGRP);
		dm->dm_data[i].dm_policy_attr.attr.show = show_dm_policy;

		ret = sysfs_add_file_to_group(&dm->dev->kobj, &dm->dm_data[i].dm_policy_attr.attr.attr, exynos_dm_attr_group.name);
		if (ret)
			dev_warn(dm->dev, "failed create sysfs for DM policy %s\n", dm->dm_data[i].dm_type_name);


		snprintf(dm->dm_data[i].constraint_table_attr.name, EXYNOS_DM_ATTR_NAME_LEN,
				"constaint_table_%s", dm->dm_data[i].dm_type_name);
		sysfs_attr_init(&dm->dm_data[i].constraint_table_attr.attr.attr);
		dm->dm_data[i].constraint_table_attr.attr.attr.name =
			dm->dm_data[i].constraint_table_attr.name;
		dm->dm_data[i].constraint_table_attr.attr.attr.mode = (S_IRUSR | S_IRGRP);
		dm->dm_data[i].constraint_table_attr.attr.show = show_constraint_table;

		ret = sysfs_add_file_to_group(&dm->dev->kobj, &dm->dm_data[i].constraint_table_attr.attr.attr, exynos_dm_attr_group.name);
		if (ret)
			dev_warn(dm->dev, "failed create sysfs for constraint_table %s\n", dm->dm_data[i].dm_type_name);
	}

	exynos_dm = dm;

	dm_idle_ip_index = exynos_get_idle_ip_index(EXYNOS_DM_MODULE_NAME, 1);
	exynos_update_ip_idle_status(dm_idle_ip_index, 1);

	dm_fast_switch_idle_ip_index = exynos_get_idle_ip_index("EXYNOS_DM_FAST_SWITCH", 1);
	exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);

	spin_lock_init(&fast_switch_glb_lock);

	platform_set_drvdata(pdev, dm);

	return 0;

err_parse_dt:
	mutex_destroy(&dm->lock);
	kfree(dm);
err_device:

	return ret;
}

static int exynos_dm_remove(struct platform_device *pdev)
{
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);

	sysfs_remove_group(&dm->dev->kobj, &exynos_dm_attr_group);
	mutex_destroy(&dm->lock);
	kfree(dm);

	return 0;
}

static struct platform_device_id exynos_dm_driver_ids[] = {
	{
		.name		= EXYNOS_DM_MODULE_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_dm_driver_ids);

static const struct of_device_id exynos_dm_match[] = {
	{
		.compatible	= "samsung,exynos-dvfs-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dm_match);

static const struct dev_pm_ops exynos_dm_pm_ops = {
	.suspend	= exynos_dm_suspend,
	.resume		= exynos_dm_resume,
};

static struct platform_driver exynos_dm_driver = {
	.probe		= exynos_dm_probe,
	.remove		= exynos_dm_remove,
	.id_table	= exynos_dm_driver_ids,
	.driver	= {
		.name	= EXYNOS_DM_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &exynos_dm_pm_ops,
		.of_match_table = exynos_dm_match,
	},
};

static int exynos_dm_init(void)
{
	return platform_driver_register(&exynos_dm_driver);
}
subsys_initcall(exynos_dm_init);

static void __exit exynos_dm_exit(void)
{
	platform_driver_unregister(&exynos_dm_driver);
}
module_exit(exynos_dm_exit);

MODULE_AUTHOR("Taekki Kim <taekki.kim@samsung.com>");
MODULE_AUTHOR("Eunok Jo <eunok25.jo@samsung.com>");
MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS SoC series DVFS Manager");
MODULE_SOFTDEP("pre: esca");
MODULE_LICENSE("GPL");
