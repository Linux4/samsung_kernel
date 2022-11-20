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
		dev_err(dev, "There are no items\n");
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
			ABC_PRINT("%s : failed parse dt spec_type1 idx : %d\n", __func__, idx);
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
	int idx = 0;

	ABC_PRINT("%s : start : %s", __func__, str);

	strlcpy(temp, str, ABC_BUFFER_MAX);
	p = temp;

	while ((c = strsep(&p, "@")) != NULL) {
		event_strings[idx] = c;
		idx++;
	}

	strlcpy(key_data->event_module, event_strings[0] + 7, ABC_EVENT_STR_MAX);
	strlcpy(key_data->event_name, event_strings[1] + 5, ABC_EVENT_STR_MAX);

	p = strsep(&event_strings[1], "=");
	strlcpy(key_data->event_type, p, ABC_TYPE_STR_MAX);

	ABC_PRINT("%s : Module(%s) Level(%s) Event(%s)", __func__,
			  key_data->event_module, key_data->event_type, key_data->event_name);
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

bool sec_abc_is_enabled_error(struct abc_key_data *key_data)
{
	struct abc_common_spec_data *common_spec;

	common_spec = sec_abc_get_matched_common_spec(key_data);

	if (!common_spec)
		return true;

	return (common_spec->enabled == 1) ? true : false;
}

void sec_abc_enqueue_event_data(struct abc_key_data *key_data, unsigned int cur_time)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("%s : There is no matched buffer.\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer.\n");
#endif
	} else {
		ABC_PRINT("%s : There is a matched buffer. Enqueue data : %s.\n", __func__, common_spec->error_name);
		sec_abc_enqueue_event_data_type1(common_spec, cur_time);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Enqueue data.\n");
#endif
	}

}

void sec_abc_dequeue_event_data(struct abc_key_data *key_data)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("%s : There is no matched buffer.\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer.\n");
#endif
	} else {
		ABC_PRINT("%s : There is a matched buffer. Dequeue data : %s.\n", __func__, common_spec->error_name);
		sec_abc_dequeue_event_data_type1(common_spec);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Dequeue data.\n");
#endif
	}
}

void sec_abc_reset_event_buffer(struct abc_key_data *key_data)
{
	struct abc_common_spec_data *common_spec = sec_abc_get_matched_common_spec(key_data);
	struct spec_data_type1 *spec_type1;

	if (!common_spec || !strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("%s : There is no matched buffer.\n", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is no matched buffer.\n");
#endif
	} else {
		ABC_PRINT("%s : There is a matched buffer. Reset buffer : %s.\n", __func__, common_spec->error_name);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("There is a matched buffer. Reset buffer.\n");
#endif
		spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);
		sec_abc_reset_buffer_type1(spec_type1);
	}
}

bool sec_abc_reached_spec_pre(struct abc_key_data *key_data, struct abc_pre_event *pre_event)
{
	struct abc_common_spec_data *common_spec;

	if (!strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("%s : INFO doesn't have spec.\n", __func__);
		return false;
	}

	if (!sec_abc_is_enabled_error(key_data)) {
		ABC_PRINT("%s : %s isn't enabled.\n", __func__, pre_event->abc_str);
		return false;
	}

	common_spec = sec_abc_get_matched_common_spec(key_data);
	if (!common_spec)
		return true;

	return sec_abc_reached_spec_type1_pre(common_spec, pre_event);
}

bool sec_abc_reached_spec(struct abc_key_data *key_data, unsigned int cur_time)
{
	struct abc_common_spec_data *common_spec;

	if (!strcmp(key_data->event_type, "INFO")) {
		ABC_PRINT("%s : INFO doesn't have spec.\n", __func__);
		return false;
	}

	common_spec = sec_abc_get_matched_common_spec(key_data);
	if (!common_spec)
		return true;

	return sec_abc_reached_spec_type1(common_spec, cur_time);
}

int sec_abc_apply_changed_spec(char *str, int mode)
{
	struct abc_common_spec_data *common_spec;
	struct abc_key_data key_data;
	struct spec_data_type1 *spec_type1;

	sec_abc_make_key_data(&key_data, str);
	common_spec = sec_abc_get_matched_common_spec(&key_data);

	if (!common_spec || common_spec->spec_cnt < mode + 1 || mode < 0) {
		ABC_PRINT("%s : Invalid change cmd. Check the Input", __func__);
#if IS_ENABLED(CONFIG_SEC_KUNIT)
		abc_common_test_get_log_str("Invalid change cmd. Check the Input");
#endif
		return -EINVAL;
	}

	spec_type1 = container_of(common_spec, struct spec_data_type1, common_spec);

	common_spec->current_spec = mode;
	common_spec->enabled = spec_type1->th_cnt[common_spec->current_spec] == SPEC_DISABLED ? 0 : 1;
	spec_type1->buffer.size = spec_type1->th_cnt[common_spec->current_spec] + 1;

	sec_abc_reset_event_buffer(&key_data);

	ABC_PRINT("%s : MODULE(%s) ERROR(%s) MODE(%d) Enabled(%d)",
			  __func__, common_spec->module_name,
			  common_spec->error_name,
			  common_spec->current_spec,
			  common_spec->enabled);

	return 0;
}

void sec_abc_change_spec(const char *str)
{
	char *cmd_strings[ABC_CMD_MAX];
	char temp[ABC_BUFFER_MAX * 5];
	char *c, *p, *p2;
	int idx = 0, mode = -1, len, str_len;

	ABC_PRINT("%s : start : %s", __func__, str);

	strlcpy(temp, str, ABC_BUFFER_MAX * 5);

	str_len = strlen(str);

	if (!str_len) {
		ABC_PRINT("%s : Empty string!", __func__);
		return;
	}

	len = str_len < ABC_BUFFER_MAX * 5 ? str_len : ABC_BUFFER_MAX * 5;

	if (temp[len - 1] == '\n')
		temp[len - 1] = 0;

	p = temp;

	while ((c = strsep(&p, ",")) != NULL && idx < ABC_CMD_MAX) {

		cmd_strings[idx] = c;

		if (sec_abc_is_valid_abc_string(cmd_strings[idx])) {

			p2 = strstr(cmd_strings[idx], "MODE=");

			if (!p2 || !*(p2 + 5) || kstrtoint(p2 + 5, 0, &mode))
				continue;

			if (!sec_abc_apply_changed_spec(cmd_strings[idx], mode))
				idx++;

		} else {
#if IS_ENABLED(CONFIG_SEC_KUNIT)
			abc_common_test_get_log_str("Invalid change cmd. Check the Input");
#endif
			ABC_PRINT("%s : Invalid change cmd. Check the Input", __func__);

		}
	}
	ABC_PRINT("%s : end : %s", __func__, str);
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
			spec_type1->th_time[i] == SPEC_DISABLED ? 0 : spec_type1->th_time[i]);
			len += scnprintf(buf + len, PAGE_SIZE - len, "THRESHOLD_CNT=%d)",
			spec_type1->th_cnt[i] == SPEC_DISABLED ? 0 : spec_type1->th_cnt[i]);
		}
		len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	ABC_PRINT("%s : %d", __func__, len);
	return len;
}

MODULE_DESCRIPTION("Samsung ABC Driver's spec manager");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
