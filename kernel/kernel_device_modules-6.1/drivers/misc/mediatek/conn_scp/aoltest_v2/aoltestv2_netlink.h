/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _AOLTESTV2_NETLINK_H_
#define _AOLTESTV2_NETLINK_H_

#include <linux/types.h>
#include <linux/compiler.h>

enum aoltest_cmd_type {
	AOLTEST_CMD_DEFAULT = 0,
	AOLTEST_CMD_START_TEST = 1,
	AOLTEST_CMD_STOP_TEST = 2,
	AOLTEST_CMD_START_DATA_TRANS = 3,
	AOLTEST_CMD_STOP_DATA_TRANS = 4,
	AOLTEST_CMD_MAX
};

struct netlink_client_info {
	uint32_t port;
	uint32_t seqnum;
};

struct aoltestv2_netlink_event_cb {
	int (*aoltestv2_bind)(uint32_t port, uint32_t module_id);
	int (*aoltestv2_unbind)(uint32_t module_id);
	int (*aoltestv2_start)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*aoltestv2_stop)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*aoltestv2_handler)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
	int (*aoltestv2_data_trans)(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz);
};

int aoltestv2_netlink_init(struct aoltestv2_netlink_event_cb *cb);
void aoltestv2_netlink_deinit(void);
int aoltestv2_netlink_send_to_native(struct netlink_client_info *client,
			char *tag, unsigned int msg_id, char *buf, unsigned int length);

#endif /*_AOLTESTV2_NETLINK_H_ */
