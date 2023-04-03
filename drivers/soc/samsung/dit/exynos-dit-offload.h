/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header for EXYNOS DIT(Direct IP Translator) Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DIT_OFFLOAD_H
#define __EXYNOS_DIT_OFFLOAD_H

#include <linux/wait.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>

#include "exynos-dit-ioctl.h"

#define OFFLOAD_MAX_DOWNSTREAM 2

enum {
	OFFLOAD_RX__FORWARD,
	OFFLOAD_TX__FORWARD,
	OFFLOAD_MAX_FORWARD
};

enum offload_state {
	STATE_OFFLOAD_OFFLINE,
	STATE_OFFLOAD_ONLINE,
	STATE_OFFLOAD_LIMIT_REACHED,
};

struct dit_offload_stat_t {
	uint64_t fwd_bytes[2];	/* double updating stat */
	uint64_t reqst_fwd_bytes;
	uint64_t limit_fwd_bytes;
};

struct dit_offload_ctl_t {
	struct iface_info upstream;
	struct iface_info downstream[OFFLOAD_MAX_DOWNSTREAM];

	struct dit_offload_stat_t stat[OFFLOAD_MAX_FORWARD]; /* 0: RX(DL), 1: TX(UL) */
	int	stat_read_idx;
	enum offload_state	status;
	bool	config_enabled;

	wait_queue_head_t	wq;
	struct list_head	events;
	spinlock_t		lock;
};

struct dit_event {
	int32_t cb_event; /* offload_cb_event */
	struct list_head	list;
};

void offload_gen_event(int event);
bool offload_keeping_bw(void);
bool offload_config_enabled(void);
int offload_get_status(void);
void offload_update_reqst(int DIR, int len);
void offload_update_stat(int DIR, int len);
struct dit_offload_ctl_t *offload_init_ctrl(void);
void offload_reset(void);
int offload_initialize(void);

/* Test purpose */
void perftest_offload_change_status(enum offload_state st);

#endif
