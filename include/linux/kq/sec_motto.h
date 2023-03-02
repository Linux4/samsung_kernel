/*
 * sec_motto.h
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Changhwi Seo <c.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SEC_MOTTO_WITH_ACT_H
#define SEC_MOTTO_WITH_ACT_H

#include <linux/device.h>
#include <linux/sti/abc_common.h>
#define MOTTO_MAGIC		0xABCDABCD
#define NAD_MOTTO_PARAM_NAME "/dev/block/NAD_REFER"
#define MOTTO_REFER_PARTITION_SIZE (1 * 256 * 1024)
#define MOTTO_RESET_REASON_MAX_LEN	8
#define MOTTO_SCRIPT_VERSION_MAX_LEN	48
#define MOTTO_TEST_NAME_MAX_LEN		48
#define MOTTO_AUI_ARRAY_MAX_SIZE		10
#define UPLOAD_INFORMATION_MAX_LEN	106
#define MOTTO_ACT_LOG_WORD_CNT		3

struct act_upload_info {
	char reset_reason[MOTTO_RESET_REASON_MAX_LEN];
	char script_version[MOTTO_SCRIPT_VERSION_MAX_LEN];
	char test_name[MOTTO_TEST_NAME_MAX_LEN];
};

struct motto_info {
	unsigned int magic;
	int rr_cnt;
	struct act_upload_info act_up_info[MOTTO_AUI_ARRAY_MAX_SIZE];
};

struct sec_motto_param {
	struct work_struct sec_motto_work;
	char buf[UPLOAD_INFORMATION_MAX_LEN];
};

static struct sec_motto_param sec_motto_param_data;

extern void store_act_upload_information(const char *buf, size_t count);
#endif
