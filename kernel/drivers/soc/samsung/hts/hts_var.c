/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_var.h"
#include "hts_vh.h"

#include <linux/cgroup.h>

static int hts_target_cgroup[MAX_TARGET_CGROUP];
static int hts_target_cgroup_count;

static struct cpumask hts_mask_cpu[MAX_MASK_BUFFER];
static int hts_mask_buffer[MAX_MASK_BUFFER];
static int hts_mask_buffer_count;

static struct hts_config_control hts_predefined_value[MAX_PREDEFINED_BUFFER];
static int hts_predefined_value_count;

static void hts_map_css_partid(struct cgroup_subsys_state *css, char *tmp, int pathlen, char *cgroup_name)
{
	cgroup_path(css->cgroup, tmp, pathlen);

	if (strcmp(tmp, cgroup_name))
		return;

	hts_target_cgroup[hts_target_cgroup_count] = css->id;
	hts_target_cgroup_count++;
}

static void hts_map_css_children(struct cgroup_subsys_state *css, char *tmp, int pathlen, char *cgroup_name)
{
	struct cgroup_subsys_state *child;

	list_for_each_entry_rcu(child, &css->children, sibling) {
		if (!child || !child->cgroup)
			continue;

		hts_map_css_partid(child, tmp, pathlen, cgroup_name);
		hts_map_css_children(child, tmp, pathlen, cgroup_name);
	}
}

void hts_add_target_cgroup(char *cgroup_name)
{
	struct cgroup *cg;
	struct cgroup_subsys_state *css;
	char buf[PID_BUFFER_LENGTH];

	if (hts_target_cgroup_count >= MAX_TARGET_CGROUP) {
		pr_err("HTS : Couldn't add cgroup anymore!");
		return;
	}

	cg = task_cgroup(current, cpuset_cgrp_id);
	if (IS_ERR_OR_NULL(cg)) {
		pr_err("HTS : Couldn't get matched cgroup (%s)", cgroup_name);
		return;
	}

	rcu_read_lock();
	css = rcu_dereference(cg->subsys[cpuset_cgrp_id]);
	if (!IS_ERR_OR_NULL(css)) {
		hts_map_css_partid(css, buf, PID_BUFFER_LENGTH - 1, cgroup_name);
		hts_map_css_children(css, buf, PID_BUFFER_LENGTH - 1, cgroup_name);
	}
	rcu_read_unlock();
}

int hts_get_target_cgroup(int index)
{
	return hts_target_cgroup[index];
}

int hts_get_cgroup_count(void)
{
	return hts_target_cgroup_count;
}

void hts_clear_target_cgroup(void)
{
	hts_target_cgroup_count = 0;
}

void hts_add_mask(unsigned long mask)
{
	int bit;
	struct cpumask *bufferMask = NULL;

	if (MAX_MASK_BUFFER <= hts_mask_buffer_count) {
		pr_err("HTS: Couldn't add mask anymore!");
		return;
	}

	bufferMask = &hts_mask_cpu[hts_mask_buffer_count];

	cpumask_clear(bufferMask);
	for_each_set_bit(bit, &mask, CONFIG_VENDOR_NR_CPUS)
                cpumask_set_cpu(bit, bufferMask);

	hts_mask_buffer[hts_mask_buffer_count] = mask;
	hts_mask_buffer_count++;
}

int hts_get_mask_index(int cpu)
{
	int i, cpu_mask = (1 << cpu);

	for (i = 0; i < hts_mask_buffer_count; ++i) {
		if (hts_mask_buffer[i] & cpu_mask)
			return i;
	}

	return -1;
}

struct cpumask *hts_get_mask(int index)
{
	if (index < 0 ||
		hts_mask_buffer_count <= index)
		return NULL;

	return &hts_mask_cpu[index];
}

int hts_get_mask_count(void)
{
	return hts_mask_buffer_count;
}

void hts_clear_mask(void)
{
	hts_mask_buffer_count = 0;
}

void hts_set_predefined_value(int index, unsigned long *value, int count)
{
	int i, maxIndex = count;

	if (index < 0 ||
		MAX_ENTRIES_MASK <= index ||
		value == NULL ||
		MAX_PREDEFINED_BUFFER <= hts_predefined_value_count) {
		pr_err("HTS: Couldn't set predefined value anymore!");
		return;
	}

	if (maxIndex > REG_CTRL_END)
		maxIndex = REG_CTRL_END;

	for (i = 0; i < maxIndex; ++i)
		hts_predefined_value[hts_predefined_value_count].ctrl[i].ext_ctrl[index] = value[i];
}

void hts_increase_predefined_index(void)
{
	hts_predefined_value_count++;
}

struct hts_config_control *hts_get_predefined_value(int index)
{
	if (index < 0 ||
		hts_predefined_value_count <= index)
		return NULL;

	return &hts_predefined_value[index];
}

int hts_get_predefined_count(void)
{
	return hts_predefined_value_count;
}

void hts_clear_predefined_value(void)
{
	hts_predefined_value_count = 0;
}
