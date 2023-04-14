/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_ITF_H
#define PABLO_ICPU_ITF_H

#include "pablo-icpu-cmd.h"
#include "pablo-icpu-enum.h"

/* ICPU_MSG_PRIO_0 is TOP priority
   Send message by FIFO order in same priority
   To avoid mistake to set high priority, let 0 to lowest priority
 */
enum icpu_msg_priority {
	ICPU_MSG_PRIO_0 = 2,
	ICPU_MSG_PRIO_HIGH = ICPU_MSG_PRIO_0,
	ICPU_MSG_PRIO_1 = 1,
	ICPU_MSG_PRIO_NORMAL = ICPU_MSG_PRIO_1,
	ICPU_MSG_PRIO_2 = 0,
	ICPU_MSG_PRIO_LOW = ICPU_MSG_PRIO_2,
	ICPU_MSG_PRIO_MAX = 3,
};
#define for_each_msg_prio_queue(x) for (x = ICPU_MSG_PRIO_0; i >= 0; --i)

#define MESSAGE_MAX_COUNT 64

typedef void (*icpu_itf_callback_func_t)(void *sender, void *cookie, u32 *data);
typedef void (*icpu_itf_ihc_msg_handler_t)(void *cookie, u32 *data);

union icpu_itf_msg_cmd {
	u64 key;
	u32 data[2];
};

struct callback_ctx {
	void *sender;
	void *cookie;
	icpu_itf_callback_func_t callback;
};

struct icpu_itf_msg {
	struct list_head list;
	struct callback_ctx cb_ctx;

	union icpu_itf_msg_cmd cmd;
	enum icpu_msg_priority prio;

	u32 data[32]; /* TODO: Fix count */
	u32 len;
};

struct pablo_icpu_itf_api {
	int (*open)(void);
	void (*close)(void);

	int (*send_message)(void *sender, void *cookie, icpu_itf_callback_func_t callback,
			enum icpu_msg_priority msg_prio, unsigned int num, u32 *data);

	int (*register_msg_handler)(void *cookie, icpu_itf_ihc_msg_handler_t callback);
	int (*register_err_handler)(void *cookie, icpu_itf_ihc_msg_handler_t callback);
	int (*wait_boot_complete)(u32 timeout_ms);
};

#if IS_ENABLED(CONFIG_PABLO_ICPU)
struct pablo_icpu_itf_api *pablo_icpu_itf_api_get(void);
struct device *pablo_itf_get_icpu_dev(void);
int pablo_icpu_itf_preload_firmware(unsigned long flag);
#else
#define pablo_icpu_itf_api_get(void)	({ NULL; })
#define pablo_itf_get_icpu_dev(void)	({ NULL; })
#define pablo_icpu_itf_preload_firmware(f)	({ 0; })
#endif

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct icpu_mbox_client;
struct pablo_icpu_mbox_chan;
int pablo_icpu_test_itf_open(struct icpu_mbox_client cl,
		struct pablo_icpu_mbox_chan *tx,
		struct pablo_icpu_mbox_chan *rx);
void pablo_icpu_test_itf_close(void);
void pablo_icpu_itf_shutdown_firmware(void);
struct icpu_mbox_client pablo_icpu_test_itf_get_client(void);
struct icpu_itf_msg *get_free_msg_wrap(void);
int set_free_msg_wrap(struct icpu_itf_msg *msg);
struct icpu_itf_msg *get_pending_msg_prio_wrap(void);
int set_pending_msg_prio_wrap(struct icpu_itf_msg *msg);
struct icpu_itf_msg *get_response_msg_match_wrap(union icpu_itf_msg_cmd cmd);
int set_response_msg_wrap(struct icpu_itf_msg *msg);
u32 get_free_msg_cnt_wrap(void);
u32 get_pending_msg_cnt_wrap(void);
u32 get_response_msg_cnt_wrap(void);
void print_all_message_queue(void);

enum itf_test_mode {
	ITF_API_MODE_NORMAL,
	ITF_API_MODE_TEST,
};
void pablo_itf_api_mode_change(enum itf_test_mode mode);
#endif

#endif
