/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header for Link Forward Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINKFORWARD_OFFLOAD_H
#define __LINKFORWARD_OFFLOAD_H

#include <linux/wait.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>

#include "linkforward-ioctl.h"

enum {
	OFFLOAD_RX_FORWARD,
	OFFLOAD_TX_FORWARD,
	OFFLOAD_MAX_FORWARD
};

enum offload_state {
	STATE_OFFLOAD_OFFLINE,
	STATE_OFFLOAD_ONLINE,
	STATE_OFFLOAD_LIMIT_REACHED,
};

struct linkforward_offload_stat_t {
	atomic64_t fwd_bytes;
	atomic64_t reqst_fwd_bytes;
	atomic64_t limit_fwd_bytes;
};

struct linkforward_offload_ctl_t {
	struct iface_info upstream;
	struct iface_info downstream;

	struct linkforward_offload_stat_t stat[OFFLOAD_MAX_FORWARD]; /* 0: RX(DL), 1: TX(UL) */
	enum offload_state	status;

	wait_queue_head_t	wq;
	struct list_head	events;
	spinlock_t		lock;
};

struct linkforward_event {
	int32_t cb_event; /* offload_cb_event */
	struct list_head	list;
};

void offload_gen_event(int event);
bool offload_keeping_bw(void);
int offload_get_status(void);
bool offload_enabled(void);
void offload_update_reqst(int DIR, int len);
void offload_update_tx_stat(int len);
void offload_update_rx_stat(int len);
u64 get_rx_offload_fwd_bytes(void);
u64 get_tx_offload_fwd_bytes(void);
struct linkforward_offload_ctl_t *offload_init_ctrl(void);
int offload_initialize(void);

#endif /* __LINKFORWARD_OFFLOAD_H */
