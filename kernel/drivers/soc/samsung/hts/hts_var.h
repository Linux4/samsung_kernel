/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <linux/cpumask.h>

#define PID_BUFFER_LENGTH                       (128)
#define MAX_TARGET_CGROUP                       (20)
#define MAX_MASK_BUFFER				(5)
#define MAX_PREDEFINED_BUFFER			(5) 

void hts_add_target_cgroup(char *cgroup_name);
int hts_get_target_cgroup(int index);
int hts_get_cgroup_count(void);
void hts_clear_target_cgroup(void);

void hts_add_mask(unsigned long mask);
int hts_get_mask_index(int cpu);
struct cpumask *hts_get_mask(int index);
int hts_get_mask_count(void);
void hts_clear_mask(void);

void hts_set_predefined_value(int type, unsigned long *value, int count);
void hts_increase_predefined_index(void);
struct hts_config_control *hts_get_predefined_value(int index);
int hts_get_predefined_count(void);
void hts_clear_predefined_value(void);
