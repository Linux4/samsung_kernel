/* abc_spec_manager.c
 *
 * Abnormal Behavior Catcher's spec manager.
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

#if IS_ENABLED(CONFIG_OF)
int abc_parse_dt(struct device *dev)
{
	struct abc_platform_data *pdata = dev->platform_data;
	struct spec_data_type1 *spec_type1;
	struct device_node *np;
	struct device_node *type1_np;
	int idx, rc;

	np = dev->of_node;
	pdata->nItem = of_get_child_count(np);
	if (!pdata->nItem) {
		dev_err(dev, "There are no items");
		return -ENODEV;
	}

	/* spec_type_1 */
	type1_np = of_find_node_by_name(np, "abc_spec_type1");
	rc = of_property_count_strings(type1_np, ERROR_KEY);

	for (idx = 0; idx < rc; idx++) {
		spec_type1 = devm_kzalloc(dev, sizeof(struct spec_data_type1), GFP_KERNEL);

		if (!spec_type1)
			return -ENOMEM;

		if (abc_parse_dt_type1(dev, type1_np, idx, spec_type1)) {
			ABC_PRINT("failed parse dt spec_type1 idx : %d", idx);
			continue;
		}
		list_add_tail(&spec_type1->node, &pdata->abc_spec_list);
	}
	return 0;
}
#endif

void sec_abc_make_key_data(struct abc_key_data *key_data, char *str)
{
	char *event_strings[ABC_UEVENT_MAX] = {0,};
	char temp[ABC_BUFFER_MAX];
	char *c, *p;
	int idx = 0, i;

	ABC_PRINT("start : %s", str);

	strlcpy(temp, str, ABC_BUFFER_MAX);
	p = temp;

	while ((c = strsep(&p, "@")) != NULL) {
		event_strings[idx] = c;
		idx++;
	}

	strlcpy(key_data->event_module, event_strings[0] + 7, ABC_EVENT_STR_MAX);

	p = strsep(&event_strings[1], "=");

	if (!strncmp(p, "INFO", 4))
		strlcpy(key_data->event_type, "INFO", ABC_TYPE_STR_MAX);
	else
		strlcpy(key_data->event_type, "WARN", ABC_TYPE_STR_MAX);

	p = strsep(&event_strings[1], "=");
	strlcpy(key_data->event_name, p, ABC_EVENT_STR_MAX);

	for (i = 2; i < idx; i++) {
		if (!strncmp(event_strings[i], "HOST=", 5)) {
			p = strstr(event_strings[i], "=");
			strlcpy(key_data->host_name, p + 1, ABC_EVENT_STR_MAX);
		} else if (!strncmp(event_strings[i], "EXT_LOG=", 8)) {
			p = strstr(event_strings[i], "=");
			strlcpy(key_data->ext_log, p + 1, ABC_EVENT_STR_MAX);
		}
	}

	ABC_PRINT("Module(%s) Level(%s) Event(%s) Host(%s) EXT_LOG(%s)",
			  key_data->event_module,
			  key_data->event_type,
			  key_data->event_name,
			  key_data->host_name,
			  key_data->ext_log);
}

struct abc_common_spec_data *sec_abc_get_matched_common_spec(struct abc_key_data *key_data)
{
	return sec_abc_get_matched_common_spec_type1(key_data);
}

int sec_abc_get_buffer_size_from_th_cnt(int th_max)
{
	int i, min_buffer_size = 16;

	for (i = 1; i < th_max; i *= 2)
		;

	return min_buffer_size > i ? min_buffer_size : i;
}

void sec_abc_enqueue_event_data(struct abc_key_data *key_data, unsigned int cur_time)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("There is no matched buffer");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer");
#endif
	} else {
		ABC_PRINT("There is a matched buffer. Enqueue data : %s", common_spec->error_name);
		sec_abc_enqueue_event_data_type1(common_spec, cur_time);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Enqueue data");
#endif
	}

}

void sec_abc_dequeue_event_data(struct abc_key_data *key_data)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("There is no matched buffer");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer");
#endif
	} else {
		ABC_PRINT("There is a matched buffer. Dequeue data : %s", common_spec->error_name);
		sec_abc_dequeue_event_data_type1(common_spec);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Dequeue data");
#endif
	}
}

void sec_abc_reset_event_buffer(struct abc_key_data *key_data)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);
	struct spec_data_type1 *spec_type1;

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("There is no matched buffer");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer");
#endif
	} else {
		ABC_PRINT("There is a matched buffer. Reset buffer : %s", common_spec->error_name);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Reset buffer");
#endif
		spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);
		sec_abc_reset_buffer_type1(spec_type1);
	}
}

bool sec_abc_reached_spec(struct abc_key_data *key_data, unsigned int cur_time)
{
	struct abc_common_spec_data *common_spec;

	if (!strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("INFO doesn't have spec");
		return false;
	}

	common_spec = sec_abc_get_matched_common_spec(key_data);
	if (!common_spec)
		return true;

	return sec_abc_reached_spec_type1(common_spec, cur_time);
}

void sec_abc_apply_changed_spec(char *str, int mode)
{
	struct abc_common_spec_data *common_spec;
	struct abc_key_data key_data;
	struct spec_data_type1 *spec_type1;
	int idx;

	sec_abc_make_key_data(&key_data, str);
	idx = sec_abc_get_idx_of_registered_event(&key_data);

	if (idx < 0)
		goto invalid;

	if (abc_event_list[idx].singular_spec && mode > 0)
		goto invalid;

	abc_event_list[idx].enabled = (mode >= 0) ? true : false;

	if (abc_event_list[idx].singular_spec || !abc_event_list[idx].enabled)
		goto out;

	common_spec = sec_abc_get_matched_common_spec(&key_data);

	if (!common_spec || common_spec->spec_cnt < mode + 1)
		goto invalid;

	spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);
	common_spec->current_spec = mode;
	spec_type1->buffer.size = spec_type1->th_cnt[common_spec->current_spec] + 1;
	sec_abc_reset_event_buffer(&key_data);

out:
	ABC_PRINT("MODULE(%s) ERROR(%s) MODE(%d) Enabled(%d)",
			  key_data.event_module,
			  key_data.event_name,
			  mode,
			  abc_event_list[idx].enabled);
	return;

invalid:
	ABC_PRINT("Invalid change cmd. Check the Input");
#if IS_ENABLED(CONFIG_SEC_KUNIT)
	abc_common_test_get_log_str("Invalid change cmd. Check the Input");
#endif
}

/* Correct format
 *
 *	MODULE=gpu@WARN=gpu_fault@MODE=0
 *	(MODE must be equal to or greater than zero.)
 *	MODULE=gpu@WARN=gpu_fault@OFF
 */

bool sec_abc_is_valid_sysfs_spec_input(char *str, int *mode)
{
	char temp[ABC_BUFFER_MAX];
	char *token[3], *p, *c;
	int idx = 0, token_cnt = 3;

	strlcpy(temp, str, ABC_BUFFER_MAX);
	p = temp;

	while ((c = strsep(&p, "@")) != NULL) {

		if (idx >= token_cnt)
			goto invalid;

		token[idx] = c;
		idx++;
	}

	if (idx != token_cnt)
		goto invalid;

	if (strncmp(token[0], "MODULE=", 7) || !*(token[0] + 7))
		goto invalid;

	if (strncmp(token[1], "WARN=", 5) || !*(token[0] + 5))
		goto invalid;

	if (!strncmp(token[2], "MODE=", 5)) {

		if (kstrtoint(token[2] + 5, 0, mode))
			goto invalid;

		if (*mode < 0)
			goto invalid;

		goto valid;

	} else if (!strcmp(token[2], "OFF")) {
		*mode = -1;
		goto valid;
	}
invalid:
	ABC_PRINT("%s invalid", str);
	return false;
valid:
	ABC_PRINT("%s valid", str);
	return true;
}

void sec_abc_change_spec(const char *str)
{
	char *cmd_string, *p;
	char temp[ABC_BUFFER_MAX * 5];
	int mode = -1, cnt = 0;

	ABC_PRINT("start : %s", str);

	strlcpy(temp, str, ABC_BUFFER_MAX * 5);
	p = temp;

	while ((cmd_string = strsep(&p, ",\n")) != NULL && cnt < REGISTERED_ABC_EVENT_TOTAL) {

		if (sec_abc_is_valid_sysfs_spec_input(cmd_string, &mode))
			sec_abc_apply_changed_spec(cmd_string, mode);
		else {
#if IS_ENABLED(CONFIG_SEC_KUNIT)
			abc_common_test_get_log_str("Invalid change cmd. Check the Input");
#endif
			ABC_PRINT("Invalid change cmd. Check the Input");
			break;

		}
		cnt++;
	}

	ABC_PRINT("end : %s", str);
}

int sec_abc_read_spec(char *buf)
{
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct spec_data_type1 *spec_type1;
	int len = 0, i, spec_cnt;

	len += scnprintf(buf + len, PAGE_SIZE - len, "spec type1\n");

	list_for_each_entry(spec_type1, &pinfo->pdata->abc_spec_list, node) {

		len += scnprintf(buf + len, PAGE_SIZE - len, "MODULE=%s ",
						 spec_type1->common_spec.module_name);
		len += scnprintf(buf + len, PAGE_SIZE - len, "WARN=%s ",
						 spec_type1->common_spec.error_name);
		len += scnprintf(buf + len, PAGE_SIZE - len, "CURRENT_SPEC=%d ",
						 spec_type1->common_spec.current_spec);

		spec_cnt = spec_type1->common_spec.spec_cnt;
		for (i = 0; i < spec_cnt; i++) {
			len += scnprintf(buf + len, PAGE_SIZE - len, "(SPEC%d ", i);
			len += scnprintf(buf + len, PAGE_SIZE - len, "THRESHOLD_TIME=%d ",
							 spec_type1->th_time[i]);
			len += scnprintf(buf + len, PAGE_SIZE - len, "THRESHOLD_CNT=%d)",
							 spec_type1->th_cnt[i]);
		}
		len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	ABC_PRINT("%d", len);
	return len;
}

MODULE_DESCRIPTION("Samsung ABC Driver's spec manager");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
