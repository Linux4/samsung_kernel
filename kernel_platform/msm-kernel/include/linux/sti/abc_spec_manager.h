/* abc_spec_manager.h
 *
 * Abnormal Behavior Catcher's spec manager module.
 *
 * Copyright 2021 Samsung Electronics
 *
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

#ifndef SEC_ABC_SPEC_MANAGER_H
#define SEC_ABC_SPEC_MANAGER_H
#include <linux/sti/abc_common.h>
#define ERROR_KEY					"name_list"
#define MODULE_KEY					"module_list"
#define SPEC_CNT_KEY				"spec_cnt_list"

extern struct device *sec_abc;
extern int abc_enable_mode;
extern int abc_init;

int abc_parse_dt(struct device *dev);
bool sec_abc_is_enabled_error(struct abc_key_data *key_data);
void sec_abc_dequeue_event_data(struct abc_key_data *key_data);
void sec_abc_reset_event_buffer(struct abc_key_data *key_data);
void sec_abc_make_key_data(struct abc_key_data *key_data, char *str);
bool sec_abc_reached_spec(struct abc_key_data *key_data, unsigned int cur_time);
void sec_abc_enqueue_event_data(struct abc_key_data *key_data, unsigned int cur_time);
void sec_abc_send_uevent(struct abc_key_data *key_data, unsigned int cur_time, char *event_type);
struct abc_common_spec_data *sec_abc_get_matched_common_spec(struct abc_key_data *key_data);
bool sec_abc_reached_spec_pre(struct abc_key_data *key_data, struct abc_pre_event *pre_event);
int sec_abc_get_buffer_size_from_th_cnt(int th_max);
/* spec_type1 */
int abc_parse_dt_type1(struct device *dev,
			  struct device_node *np, int idx,
			  struct spec_data_type1 *spec_type1);
bool sec_abc_is_full_type1(struct abc_event_buffer *buffer);
bool sec_abc_is_empty_type1(struct abc_event_buffer *buffer);
int sec_abc_get_diff_time_type1(struct abc_event_buffer *buffer);
void sec_abc_reset_buffer_type1(struct spec_data_type1 *common_spec);
struct abc_fault_info sec_abc_dequeue_type1(struct abc_event_buffer *buffer);
void sec_abc_dequeue_event_data_type1(struct abc_common_spec_data *common_spec);
void sec_abc_enqueue_type1(struct abc_event_buffer *buffer, struct abc_fault_info in);
bool sec_abc_reached_spec_type1(struct abc_common_spec_data *common_spec, unsigned int cur_time);
struct abc_common_spec_data *sec_abc_get_matched_common_spec_type1(struct abc_key_data *key_data);
void sec_abc_enqueue_event_data_type1(struct abc_common_spec_data *common_spec, unsigned int cur_time);
bool sec_abc_reached_spec_type1_pre(struct abc_common_spec_data *common_spec, struct abc_pre_event *pre_event);
#endif
