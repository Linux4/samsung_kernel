/*
 * sec_motto.c
 *
 *  Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Changhwi Seo <c.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/sec_class.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kq/sec_motto.h>

static bool motto_info_initialized;
static bool motto_work_initialized;
static struct motto_info motto_envs;

static void sec_print_motto_info() {
	int i = 0;
	int rr_cnt = motto_envs.rr_cnt < MOTTO_AUI_ARRAY_MAX_SIZE ? motto_envs.rr_cnt : MOTTO_AUI_ARRAY_MAX_SIZE;
	printk("%s: ++\n", __func__);
	printk("%s: %d\n", __func__, motto_envs.rr_cnt);
	for (i = 0; i < rr_cnt; i++) {
		printk("%s: %d. %s\t%s\t%s", __func__, i, motto_envs.act_up_info[i].reset_reason, motto_envs.act_up_info[i].script_version, motto_envs.act_up_info[i].test_name);
	}
	printk("%s: --\n", __func__);
}

static void sec_motto_param_init(void) {
	int ret;
	struct file *fp;

	printk("%s: initialize\n", __func__);

	fp = filp_open(NAD_MOTTO_PARAM_NAME, O_RDONLY, 0);

	if (IS_ERR_OR_NULL(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	else {
		printk("%s: search partition motto area\n", __func__);
		ret = fp->f_op->llseek(fp, -(MOTTO_REFER_PARTITION_SIZE), SEEK_END);
		if (ret < 0) {
			pr_err("%s: llseek error %d!\n", __func__, ret);
			goto close_fp_init_out;
		}

		printk("%s: read partition\n", __func__);
		ret = vfs_read(fp, (char *)&motto_envs, sizeof(struct motto_info), &(fp->f_pos));
		if (ret < 0) {
			pr_err("%s: 1th read error! %d\n", __func__, ret);
			goto close_fp_init_out;
		}
		motto_envs.magic = MOTTO_MAGIC;
		motto_info_initialized = true;
		sec_print_motto_info();
	}
close_fp_init_out:
	if (fp != NULL)
		filp_close(fp, NULL);
}

static int set_act_upload_info(char *data, int idx, int order) {
	int str_len = strlen(data) + 1;
	switch (order) {
		case 1:
			memcpy(motto_envs.act_up_info[idx].reset_reason, data, str_len < MOTTO_RESET_REASON_MAX_LEN ? str_len : MOTTO_RESET_REASON_MAX_LEN);
			motto_envs.act_up_info[idx].reset_reason[MOTTO_RESET_REASON_MAX_LEN - 1] = '\0';
			break;
		case 2:
			memcpy(motto_envs.act_up_info[idx].script_version, data, str_len < MOTTO_SCRIPT_VERSION_MAX_LEN ? str_len : MOTTO_SCRIPT_VERSION_MAX_LEN);
			motto_envs.act_up_info[idx].script_version[MOTTO_SCRIPT_VERSION_MAX_LEN - 1] = '\0';
			break;
		case 3:
			memcpy(motto_envs.act_up_info[idx].test_name, data, str_len < MOTTO_TEST_NAME_MAX_LEN ? str_len : MOTTO_TEST_NAME_MAX_LEN);
			motto_envs.act_up_info[idx].test_name[MOTTO_TEST_NAME_MAX_LEN - 1] = '\0';
			break;
		default:
			return str_len - 1;
	}
	return 0;
}

static char *motto_strtok(char *_Str, char *_Delim) {
	static char *pStr;
	const char *pDelim;

	if (_Str == NULL)
		_Str = pStr;
	else
		pStr = _Str;

	while (*pStr) {
		pDelim = _Delim;

		while (*pDelim) {
			if (*pStr == *pDelim) {
				*pStr = 0;
				pStr++;
				return _Str;
			}
			pDelim++;
		}
		pStr++;
	}

	return _Str;
}

static void sec_motto_param_data_clear() {
	int i = 0;
	int rr_cnt = motto_envs.rr_cnt < MOTTO_AUI_ARRAY_MAX_SIZE ? motto_envs.rr_cnt : MOTTO_AUI_ARRAY_MAX_SIZE;
	for (i = 0; i < rr_cnt; i++) {
		motto_envs.act_up_info[i].reset_reason[0] = '\0';
		motto_envs.act_up_info[i].script_version[0] = '\0';
		motto_envs.act_up_info[i].test_name[0] = '\0';
	}
	motto_envs.rr_cnt = 0;
}

static int sec_motto_check_clear_cmd(char *buf) {
	return strncmp(buf, "clear", 5);
}

static int sec_motto_param_data_parsing(char *buf) {
	char *ptr = NULL;
	int cur_idx = -1;
	int loop_cnt = 0;
	int rr_cnt = motto_envs.rr_cnt + 1;

	cur_idx = (rr_cnt - 1) % MOTTO_AUI_ARRAY_MAX_SIZE;

	ptr = motto_strtok(buf, ",");
	while (ptr != NULL && *ptr != '\0') {
		if (set_act_upload_info(ptr, cur_idx, ++loop_cnt)) {
			printk("%s: invalid buf=%s format\n", __func__, ptr);
			return -1;
		}
		ptr = motto_strtok(NULL, ",");
	}
	if (loop_cnt >= MOTTO_ACT_LOG_WORD_CNT) {
		motto_envs.rr_cnt = rr_cnt;
		return 0;
	} else
		return -1;
}

static void sec_motto_param_update(char *buf) {
	int ret;
	struct file *fp;

	if (!motto_info_initialized) {
		printk("%s : skip it's not initialized!\n", __func__);
		return;
	}

	if (sec_motto_check_clear_cmd(buf) == 0) {
		printk("%s : act upload log clear!\n", __func__);
		sec_motto_param_data_clear();
	}
	else if (sec_motto_param_data_parsing(buf)) {
		printk("%s : act upload log parsing error!\n", __func__);
		return;
	}

	fp = filp_open(NAD_MOTTO_PARAM_NAME, O_RDWR | O_SYNC, 0);

	if (IS_ERR_OR_NULL(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	else {
		ret = fp->f_op->llseek(fp, -(MOTTO_REFER_PARTITION_SIZE), SEEK_END);
		if (ret < 0) {
			pr_err("%s: llseek error %d!\n", __func__, ret);
			goto close_fp_out;
		}

		ret = vfs_write(fp, (char *)&motto_envs, sizeof(struct motto_info), &(fp->f_pos));
		if (ret < 0) {
			pr_err("%s: 1th write error! %d\n", __func__, ret);
			goto close_fp_out;
		}
		sec_print_motto_info();
	}
close_fp_out:
	if (fp != NULL)
		filp_close(fp, NULL);
}

static void sec_motto_param_work(struct work_struct *work) {
	struct sec_motto_param *motto_data = container_of(work, struct sec_motto_param, sec_motto_work);
	printk("%s\n", __func__);
	if (!motto_info_initialized)
		sec_motto_param_init();

	if (strlen(motto_data->buf) > 0)
		sec_motto_param_update(motto_data->buf);
}

void store_act_upload_information(const char *buf, size_t count) {
	printk("%s: ++\n", __func__);

	if (sec_abc_get_enabled() == 0)
		return;

	if (count >= UPLOAD_INFORMATION_MAX_LEN) {
		memcpy(sec_motto_param_data.buf, buf, UPLOAD_INFORMATION_MAX_LEN);
		sec_motto_param_data.buf[UPLOAD_INFORMATION_MAX_LEN - 1] = '\0';
	}
	else {
		memcpy(sec_motto_param_data.buf, buf, count);
		sec_motto_param_data.buf[count] = '\0';
	}

	if (!motto_work_initialized) {
		motto_work_initialized = true;
		INIT_WORK(&sec_motto_param_data.sec_motto_work, sec_motto_param_work);
	}

	schedule_work(&sec_motto_param_data.sec_motto_work);
}
EXPORT_SYMBOL(store_act_upload_information);
