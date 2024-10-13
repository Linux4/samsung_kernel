// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "pablo-icpu-itf.h"
#include "pablo-icpu-core.h"
#include "mbox/pablo-icpu-mbox.h"
#include "mem/pablo-icpu-mem.h"

#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
#define TIMEOUT_ICPU			10000
#else
#define TIMEOUT_ICPU			1000
#endif

static struct pablo_icpu_itf_api *itf;

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-SELFTEST]",
};
struct icpu_logger *get_icpu_selftest_log(void)
{
	return &_log;
}

const char *__token = " ";

struct message {
	int valid;
	u32 num_data;
	u32 data[16];
	void *buf;
};

struct msg_box {
	u32 num_msg;
	struct message msg[MESSAGE_MAX_COUNT];
};

static struct msg_box incoming = {
	.num_msg = 0,
};
static struct msg_box preset0 = {
	.num_msg = 1,
	.msg[0] = {
		.valid = 1,
		.num_data = 3,
		.data[0] = 0x1,
		.data[1] = 0x2,
		.data[2] = 0x3,
	},
};
static struct msg_box preset1 = {
	.num_msg = 2,
	.msg[0] = {
		.valid = 1,
		.num_data = 3,
		.data[0] = 0x1,
		.data[1] = 0x2,
		.data[2] = 0x3,
	},
	.msg[1] = {
		.valid = 1,
		.num_data = 2,
		.data[0] = 0x4,
		.data[1] = 0x5,
	},
};
static struct msg_box preset2 = {
	.num_msg = 3,
	.msg[0] = {
		.valid = 1,
		.num_data = 3,
		.data[0] = 0x1,
		.data[1] = 0x2,
		.data[2] = 0x3,
	},
	.msg[1] = {
		.valid = 1,
		.num_data = 2,
		.data[0] = 0x4,
		.data[1] = 0x5,
	},
	.msg[2] = {
		.valid = 1,
		.num_data = 3,
		.data[0] = 0x8,
		.data[1] = 0x9,
		.data[2] = 0x10,
	},
};

enum target_id {
	BOARD = 0,
	TARGET_INVALID,
};

static const char *__target_id_str[] = {
	"board",
};

static enum target_id __get_target_id(char *str)
{
	int i;
	enum target_id id = TARGET_INVALID;

	for (i = 0; i < TARGET_INVALID; i++) {
		if (strlen(str) != strlen(__target_id_str[i]))
				continue;

		if (!strncmp(str, __target_id_str[i], strlen(str))) {
			id = i;
			break;
		}
	}

	return id;
}

enum boot_state {
	POWERDOWN = 0,
	BOOTING,
	TIMEOUT,
	RUNNING,
	ERROR,
};

static const char *__boot_state_str[] = {
	"POWERDOWN",
	"BOOTING",
	"BOOT_TIMEOUT",
	"RUNNING",
	"ERROR",
};

enum scenario_mode {
	MANUAL = 0,
	PRE0,
	PRE1,
	PRE2,
	SECNARIO_INVALID,
};

static const char *__scenario_mode_str[] = {
	"manual",
	"preset0",
	"preset1",
	"preset2",
};

static struct test_status {
	enum target_id target;
	enum boot_state state;
	enum scenario_mode scenario;
} __icpu = {
	.target = BOARD,
	.state = POWERDOWN,
	.scenario = MANUAL,
};

enum control_id {
	CTRL_TARGET = 0,	/* board, virtual */
	CTRL_SCENARIO,		/* Manual, preset0,1,2... */
	CTRL_BOOT,		/* on, off */
	CTRL_MSG,		/* a for send all msg or num of msg */
	CTRL_PRELOAD,		/* Preload FW binary and verify */
	CTRL_INVALID,
};

static const char *__control_id_str[] = {
	"target",
	"scenario",
	"boot",
	"msg",
	"preload",
};

static enum control_id __get_control_id(char *str)
{
	int i;
	enum control_id id = CTRL_INVALID;

	for (i = 0; i < CTRL_INVALID; i++) {
		if (strlen(str) != strlen(__control_id_str[i]))
				continue;

		if (!strncmp(str, __control_id_str[i], strlen(str))) {
			id = i;
			break;
		}
	}

	return id;
}

static struct msg_box *__get_msg_box_by_scenario(enum scenario_mode scenario)
{
	if (scenario == MANUAL)
		return &incoming;
	if (scenario == PRE0)
		return &preset0;
	if (scenario == PRE1)
		return &preset1;
	if (scenario == PRE2)
		return &preset2;

	return NULL;

}

typedef int (*ctrl_func)(char **pstr);

static int __target_ctrl_func(char **pstr)
{
	char *str;
	enum target_id id;

	str = strsep(pstr, __token);

	id = __get_target_id(str);

	if (id == TARGET_INVALID) {
		ICPU_ERR("invalid target target (%s)\n", str);
		return -1;
	}

	__icpu.target = id;

	return 0;
}

static int __scenario_ctrl_func(char **pstr)
{
	ICPU_INFO("input %s", *pstr);

	return 0;
}

static int __boot_ctrl_func(char **pstr)
{
	int ret;

	if (!strncmp(*pstr, "on", strlen(*pstr))) {
		itf = pablo_icpu_itf_api_get();
		if (!itf) {
			ICPU_ERR("fail to get itf api");
			return -ENODEV;
		}

		ret = itf->open();
		if (ret) {
			ICPU_ERR("open failed. ret(%d)", ret);

			return ret;
		}

		ret = itf->wait_boot_complete(TIMEOUT_ICPU);
		if (ret) {
			ICPU_ERR("boot failed. ret(%d)", ret);
			itf->close();

			return ret;
		}

		__icpu.state = RUNNING;
	} else {
		if (itf)
			itf->close();
		__icpu.state = POWERDOWN;
	}

	return 0;
}

static void rsp_cb(void *sender, void *cookie, u32 *data)
{
	ICPU_INFO("response callback, data[0]: %d, data[1]: %d\n", data[0], data[1]);
}

static int __msg_ctrl_func(char **pstr)
{
	int ret;
	int send_all = 0;
	long num_msg;
	struct msg_box *box = __get_msg_box_by_scenario(__icpu.scenario);
	int i;

	ICPU_INFO("input: %s", *pstr);

	if (__icpu.state != RUNNING) {
		ICPU_ERR("ICPU is not running (state: %s)\n", __boot_state_str[__icpu.state]);
		return -1;
	}

	if (!box->num_msg) {
		ICPU_ERR("no msg\n");
		return -1;
	}

	if (!strncmp(*pstr, "all", strlen(*pstr)))
		send_all = 1;

	if (send_all) {
		ICPU_INFO("Send all messages, num_msg(%d)\n", box->num_msg);

		for (i = 0; i < box->num_msg; i++) {
			ICPU_DEBUG("send msg[%d], %d\n", i, box->msg[i].data[0]);
			ret = itf->send_message(NULL, NULL, rsp_cb, 0,
					box->msg[i].num_data, box->msg[i].data);
			if (ret)
				return ret;
		}
	} else {
		ret = kstrtol(*pstr, 0, &num_msg);
		if (ret) {
			ICPU_ERR("check input, ret(%d)\n", ret);
			return ret;
		}
		ICPU_INFO("send message %ld\n", num_msg);
	}

	return 0;
}

static int __preload_ctrl_func(char **pstr)
{
	int ret;
	unsigned long flag = 0;

	ICPU_INFO("input: %s", *pstr);

	ret = kstrtol(*pstr, 0, &flag);
	if (ret) {
		ICPU_ERR("check input, ret(%d)\n", ret);
		return ret;
	}

	ret = pablo_icpu_itf_preload_firmware(flag);
	if (ret) {
		ICPU_ERR("Fail to preload firmware, ret(%d)", ret);
		return ret;
	}

	return 0;
}

static ctrl_func __ctrl_ops[CTRL_INVALID] = {
	__target_ctrl_func,
	__scenario_ctrl_func,
	__boot_ctrl_func,
	__msg_ctrl_func,
	__preload_ctrl_func,
};

static int __set_control(const char *val, const struct kernel_param *kp)
{
	int ret;
	char *str;
	char *pbuf;
	char *pstr;
	enum control_id ctrl_id;
	size_t val_len;

	if (!val)
		return -EINVAL;

	ICPU_INFO("input val = %s", val);
	val_len = strlen(val);
	if (val_len == 0) {
		ICPU_ERR("input val is invalid, len(%zu)\n", val_len);
		return -1;
	}

	pbuf = vzalloc(val_len + 1);
	if (!pbuf) {
		ICPU_ERR("sorry, internal error. please retry\n");
		return -1;
	}

	strncpy(pbuf, val, val_len);
	strreplace(pbuf, '\n', '\0');

	pstr = pbuf;

	do {
		str = strsep(&pstr, __token);

		ctrl_id = __get_control_id(str);
		if (ctrl_id == CTRL_INVALID) {
			ICPU_ERR("unknown control (%s)\n", str);
			vfree(pbuf);
			return -1;
		}

		ret = __ctrl_ops[ctrl_id](&pstr);
		if (ret) {
			ICPU_ERR("fail to control ret(%d)\n", ret);
			vfree(pbuf);
			return ret;
		}

		/* TODO: multi control support */
		str = NULL;
	} while (str);

	vfree(pbuf);

	ICPU_INFO("OK\n");

	return 0;
}

static int __query_state(char *buffer, const struct kernel_param *kp)
{
	int len;

	if (!buffer)
		return -EINVAL;

	len = sprintf(buffer, "ICPU selftest state:\n");
	len += sprintf(buffer + len, "\tTarget: %s\n", __target_id_str[__icpu.target]);
	len += sprintf(buffer + len, "\tboot state: %s\n", __boot_state_str[__icpu.state]);

	return len;
}

static int __clear_messages(struct msg_box *box)
{
	int i;
	struct pablo_icpu_buf_info buf_info;

	if (__icpu.scenario != MANUAL) {
		ICPU_ERR("Cannot clear message of scenario(%s)", __scenario_mode_str[__icpu.scenario]);
		return -EINVAL;
	}
	ICPU_ERR("Clear all message\n");

	for (i = 0; i < MESSAGE_MAX_COUNT; i++) {
		if (box->msg[i].buf) {
			buf_info = pablo_icpu_mem_get_buf_info(box->msg[i].buf);
			ICPU_INFO("free dma_buf, size(%zu), kva(%lx), dva(%llx)",
					buf_info.size, buf_info.kva, buf_info.dva);

			pablo_icpu_mem_free(box->msg[i].buf);
		}

		box->msg[i].buf = NULL;
	}

	box->num_msg = 0;
	memset(box->msg, 0x0, sizeof(struct message) * MESSAGE_MAX_COUNT);

	return 0;
}

static int __check_buffer_param(char *str, long *dva_msb, long *dva_lsb, void **dma_buf)
{
	int ret;
	const char *open_tk = "[";
	const char *close_tk = "]";
	const char *comma_tk = ":";
	long size;
	char *data;
	void *buf;
	int buffer_type;
	char *temp_str;
	struct pablo_icpu_buf_info buf_info;

	ICPU_DEBUG("check buffer param, str(%s)\n", str);

	// check buffer type
	if (*str == 'b') {
		buffer_type = 1;
		temp_str = strsep(&str, open_tk);
		if (!temp_str || !str) {
			ICPU_ERR("invalid input\n");
			return -EINVAL;
		}
		temp_str = strsep(&str, comma_tk);
		if (!temp_str || !str) {
			ICPU_ERR("invalid input\n");
			return -EINVAL;
		}
		ret = kstrtol(temp_str, 0, &size);
		if (ret) {
			ICPU_ERR("check size, ret(%d)\n", ret);
			return -EINVAL;
		}
		data = strsep(&str, close_tk);
	} else {
		buffer_type = 0;
		ret = 0;
	}

	if (buffer_type) {
		buf = pablo_icpu_mem_alloc(ICPU_MEM_TYPE_PMEM, size, "system-uncached", 0);
		if (buf == NULL) {
			ICPU_ERR("dma_buf alloc fail\n");
			return -ENOMEM;
		}
		*dma_buf = buf;
		buf_info = pablo_icpu_mem_get_buf_info(buf);

		*dva_msb = buf_info.dva >> 4;
		*dva_lsb = buf_info.dva & 0xF;
		memcpy((void *)buf_info.kva, (void *)data, strlen(data));

		if (!IS_ENABLED(ICPU_IO_COHERENCY))
			pablo_icpu_mem_sync_for_device(buf);
		ret = size;
		ICPU_INFO("dma_buf size(%ld), kva(%lx), dva(%llx), data(0x%lx, 0x%lx)",
				size, buf_info.kva, buf_info.dva, *dva_msb, *dva_lsb);
	}

	return ret;
}

static int __set_msg(const char *val, const struct kernel_param *kp)
{
	int ret;
	const char *open_tk = "[";
	const char *close_tk = "]";
	const char *comma_tk = ",";
	char *str;
	char *pbuf;
	char *pstr;
	long num_data;
	int i;
	long data[16];
	struct msg_box *box = __get_msg_box_by_scenario(__icpu.scenario);
	void *dma_buf =  NULL;
	size_t val_len;

	if (!val)
		return -EINVAL;

	if (*val == 'c') {
		ret = __clear_messages(box);
		return ret;
	}

	if (box->num_msg >= MESSAGE_MAX_COUNT) {
		ICPU_ERR("Too many messages (%d)\n", MESSAGE_MAX_COUNT);
		return -1;
	}

	val_len = strlen(val);
	if (val_len == 0) {
		ICPU_ERR("input val is invalid, len(%zu)\n", val_len);
		return -1;
	}

	pbuf = vzalloc(val_len + 1);
	if (!pbuf) {
		ICPU_ERR("sorry, internal error. please retry\n");
		return -1;
	}

	strncpy(pbuf, val, val_len);
	strreplace(pbuf, '\n', '\0');

	pstr = pbuf;

	str = strsep(&pstr, open_tk);

	ret = kstrtol(str, 0, &num_data);
	if (ret) {
		ICPU_ERR("check input, ret(%d)\n", ret);
		vfree(pbuf);
		return ret;
	}

	if (num_data > 16) {
		ICPU_ERR("Max num of data is 16 but, %ld\n", num_data);
		vfree(pbuf);
		return -1;
	}

	for (i = 0; i < num_data; i++) {
		if (i < num_data - 1)
			str = strsep(&pstr, comma_tk);
		else /* last data */
			str = strsep(&pstr, close_tk);

		if (!str) {
			ICPU_ERR("invalid param, check input\n");
			vfree(pbuf);
			return -1;
		}

		ret = __check_buffer_param(str, &data[i], &data[i+1], &dma_buf);
		if (ret > 0) {
			/* In case buffer type param, it need two register for represent dva */
			i++;
			num_data++;
		} else if (ret == 0) {
			ret = kstrtol(str, 0, &data[i]);
		}
		if (ret < 0) {
			ICPU_ERR("data is invalid, check input. ret(%d)\n", ret);
			vfree(pbuf);
			return ret;
		}
	}

	vfree(pbuf);

	box->msg[box->num_msg].valid = 1;
	box->msg[box->num_msg].num_data = num_data;
	box->msg[box->num_msg].buf = dma_buf;
	for (i = 0; i < num_data; i++)
		box->msg[box->num_msg].data[i] = data[i];
	box->num_msg++;

	ICPU_INFO("num of message in the box: %d\n", box->num_msg);

	return 0;
}

static int __query_msg(char *buffer, const struct kernel_param *kp)
{
	struct msg_box *box = __get_msg_box_by_scenario(__icpu.scenario);
	int i, j;
	int len;

	if (!buffer)
		return -EINVAL;

	if (!box) {
		ICPU_ERR("Invalid scenario (%d)\n", __icpu.scenario);
		return -1;
	}

	len = sprintf(buffer, "Message box for %s\n", __scenario_mode_str[__icpu.scenario]);
	len += sprintf(buffer + len, "\tNum of msg: %d\n", box->num_msg);
	for (i = 0; i < box->num_msg; i++) {
		len += sprintf(buffer + len, "\tmsg[%d]\n", i);
		len += sprintf(buffer + len, "\t\t.valid: %d\n", box->msg[0].valid);
		len += sprintf(buffer + len, "\t\t.num of data: %d\n", box->msg[0].num_data);

		/* NOTE: buffer is only 4K, so we cannot print all messages.
		 *       We print only first 16 messages.
		 */
		if (i >= 16)
			continue;
		for (j = 0; j < box->msg[0].num_data; j++) {
			len += sprintf(buffer + len, "\t\t.data[%d]: 0x%x(dec. %d)\n", j,
					box->msg[i].data[j], box->msg[i].data[j]);
		}
	}

	return len;
}

static const struct kernel_param_ops __control_param_ops = {
	.set = __set_control,
	.get = __query_state,
};

static const struct kernel_param_ops __message_param_ops = {
	.set = __set_msg,
	.get = __query_msg,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_selftest_get_param_ops(const struct kernel_param_ops **control_ops, const struct kernel_param_ops **msg_ops) {
	if (!control_ops || !msg_ops)
		return -EINVAL;

	*control_ops = &__control_param_ops;
	*msg_ops = &__message_param_ops;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_selftest_get_param_ops);

void pablo_icpu_selftest_rsp_cb(void *sender, void *cookie, u32 *data)
{
	rsp_cb(sender, cookie, data);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_selftest_rsp_cb);

#endif

module_param_cb(test_icpu, &__control_param_ops, NULL, 0644);
module_param_cb(test_msg, &__message_param_ops, NULL, 0644);
