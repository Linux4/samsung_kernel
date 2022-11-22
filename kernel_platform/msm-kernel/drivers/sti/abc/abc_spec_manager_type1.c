/* abc_spec_manager_type1.c
 *
 * Abnormal Behavior Catcher's spec(type1) manager
 *
 * Copyright 2021 Samsung Electronics
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
#include <linux/sti/abc_spec_manager.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <linux/sti/abc_kunit.h>
#endif
__visible_for_testing
int abc_alloc_memory_to_buffer_type1(struct spec_data_type1 *spec_type1, int max_cnt)
{
	int buffer_max;

	buffer_max = sec_abc_get_buffer_size_from_th_cnt(max_cnt);

	spec_type1->buffer.abc_element = kcalloc(buffer_max, sizeof(spec_type1->buffer.abc_element[0]), GFP_KERNEL);

	if (!spec_type1->buffer.abc_element) {

		ABC_PRINT("Failed to allocate memory to buffer");
		spec_type1->buffer.buffer_max = 0;
		spec_type1->buffer.size = 0;
		return -ENOMEM;
	}

	spec_type1->buffer.size = spec_type1->th_cnt[spec_type1->common_spec.current_spec] + 1;
	spec_type1->buffer.buffer_max = buffer_max;
	sec_abc_reset_buffer_type1(spec_type1);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
int abc_parse_dt_type1(struct device *dev,
					   struct device_node *np,
					   int idx,
					   struct spec_data_type1 *spec_type1)
{
	char temp[ABC_BUFFER_MAX];
	int i, spec_cnt, max_cnt = 0;

	if (of_property_read_string_index(np, MODULE_KEY, idx, &spec_type1->common_spec.module_name)) {
		dev_err(dev, "Failed to get module name : node not exist");
		return -EINVAL;
	}

	if (of_property_read_string_index(np, ERROR_KEY, idx, &spec_type1->common_spec.error_name)) {
		dev_err(dev, "Failed to get error name : node not exist");
		return -EINVAL;
	}

	if (of_property_read_u32_index(np, SPEC_CNT_KEY, idx, &spec_type1->common_spec.spec_cnt)) {
		dev_err(dev, "Failed to get mode cnt : node not exist");
		return -EINVAL;
	}

	spec_cnt = spec_type1->common_spec.spec_cnt;

	spec_type1->th_cnt = kcalloc(spec_cnt, sizeof(int), GFP_KERNEL);
	if (!spec_type1->th_cnt)
		return -ENOMEM;

	spec_type1->th_time = kcalloc(spec_cnt, sizeof(int), GFP_KERNEL);
	if (!spec_type1->th_time)
		return -ENOMEM;

	for (i = 0; i < spec_cnt; i++) {

		snprintf(temp, ABC_BUFFER_MAX, "th_cnt_list%d", i);
		if (of_property_read_u32_index(np, temp, idx, &spec_type1->th_cnt[i])) {
			dev_err(dev, "Failed to get th count : node not exist");
			return -EINVAL;
		}

		snprintf(temp, ABC_BUFFER_MAX, "th_time_list%d", i);
		if (of_property_read_u32_index(np, temp, idx, &spec_type1->th_time[i])) {
			dev_err(dev, "Failed to get th time : node not exist");
			return -EINVAL;
		}

		if (spec_type1->th_cnt[i] > max_cnt)
			max_cnt = spec_type1->th_cnt[i];

		if (spec_type1->th_time[i] == 0)
			spec_type1->th_time[i] = INT_MAX;

		ABC_PRINT("type1-spec(%d) : module(%s) error(%s) th_cnt(%d) th_time(%d)",
			  i,
			  spec_type1->common_spec.module_name,
			  spec_type1->common_spec.error_name,
			  spec_type1->th_cnt[i],
			  spec_type1->th_time[i]);
	}

	if (abc_alloc_memory_to_buffer_type1(spec_type1, max_cnt + 1))
		return -ENOMEM;

	ABC_PRINT("module(%s) error(%s)",
			  spec_type1->common_spec.module_name,
			  spec_type1->common_spec.error_name);

	return 0;
}
#endif

struct abc_common_spec_data *sec_abc_get_matched_common_spec_type1(struct abc_key_data *key_data)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct spec_data_type1 *spec_type1;

	list_for_each_entry(spec_type1, &pinfo->pdata->abc_spec_list, node) {
		if (!strcmp(spec_type1->common_spec.module_name, key_data->event_module)
			&& !strcmp(spec_type1->common_spec.error_name, key_data->event_name))
			return &(spec_type1->common_spec);
	}
	return NULL;
}

bool sec_abc_reached_spec_type1(struct abc_common_spec_data *common_spec, unsigned int cur_time)
{
	struct spec_data_type1 *spec_type1;
	int current_spec = common_spec->current_spec;

	spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);

	do_div(cur_time, MSEC_PER_SEC);

	ABC_PRINT("MODULE(%s) WARN(%s) current_spec(%d) warn_cnt(%d) time(%d)",
			  spec_type1->common_spec.module_name,
			  spec_type1->common_spec.error_name,
			  current_spec,
			  spec_type1->buffer.warn_cnt,
			  cur_time);

	if (spec_type1->buffer.warn_cnt >= spec_type1->th_cnt[current_spec]) {
		if (sec_abc_get_diff_time_type1(&spec_type1->buffer) < spec_type1->th_time[current_spec]) {
			ABC_PRINT("%s occurred. current_spec(%d). Send uevent",
					 spec_type1->common_spec.error_name, current_spec);
			return true;
		}
		sec_abc_dequeue_event_data_type1(common_spec);
	}
	return false;
}

void sec_abc_reset_buffer_type1(struct spec_data_type1 *spec_type1)
{
	spec_type1->buffer.rear = 0;
	spec_type1->buffer.front = 0;
	spec_type1->buffer.warn_cnt = 0;
}

bool sec_abc_is_full_type1(struct abc_event_buffer *buffer)
{
	if ((buffer->rear + 1) % buffer->size == buffer->front)
		return true;
	else
		return false;
}

bool sec_abc_is_empty_type1(struct abc_event_buffer *buffer)
{
	if (buffer->front == buffer->rear)
		return true;
	else
		return false;
}

void sec_abc_enqueue_type1(struct abc_event_buffer *buffer, struct abc_fault_info in)
{
	if (sec_abc_is_full_type1(buffer)) {
		ABC_PRINT("queue is full");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("queue is full");
#endif
	} else {
		buffer->rear = (buffer->rear + 1) % buffer->size;
		buffer->abc_element[buffer->rear] = in;
	}
}

struct abc_fault_info sec_abc_dequeue_type1(struct abc_event_buffer *buffer)
{
	struct abc_fault_info out;

	if (sec_abc_is_empty_type1(buffer)) {
		ABC_PRINT("queue is empty");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("queue is empty");
#endif
		out.cur_time = out.cur_cnt = 0;
	} else {
		buffer->front = (buffer->front + 1) % buffer->size;
		out = buffer->abc_element[buffer->front];
	}
	return out;
}

void sec_abc_enqueue_event_data_type1(struct abc_common_spec_data *common_spec, unsigned int cur_time)
{
	struct spec_data_type1 *spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);
	struct abc_fault_info in;

	do_div(cur_time, MSEC_PER_SEC);

	in.cur_time = (int)cur_time;
	in.cur_cnt = spec_type1->buffer.warn_cnt++;

	sec_abc_enqueue_type1(&spec_type1->buffer, in);
}

void sec_abc_dequeue_event_data_type1(struct abc_common_spec_data *common_spec)
{
	struct spec_data_type1 *spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);
	struct abc_fault_info out;

	out = sec_abc_dequeue_type1(&spec_type1->buffer);
	spec_type1->buffer.warn_cnt--;
}

int sec_abc_get_diff_time_type1(struct abc_event_buffer *buffer)
{
	int front_time, rear_time;

	front_time = buffer->abc_element[(buffer->front + 1) % buffer->size].cur_time;
	rear_time = buffer->abc_element[buffer->rear].cur_time;

	ABC_PRINT("front time : %d sec (%d) rear_time %d sec (%d) diff : %d",
		  front_time,
		  buffer->front + 1,
		  rear_time,
		  buffer->rear,
		  rear_time - front_time);

	return rear_time - front_time;
}

MODULE_DESCRIPTION("Samsung ABC Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
