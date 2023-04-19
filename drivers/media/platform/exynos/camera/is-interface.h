/*
 * drivers/media/video/exynos/fimc-is-mc2/fimc-is-interface.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_INTERFACE_H
#define IS_INTERFACE_H
#include "is-config.h"
#include "is-time.h"
#include "pablo-work.h"

#define TRY_TIMEOUT_COUNT	2
#define SENSOR_TIMEOUT_COUNT	2

enum is_interface_state {
	IS_IF_STATE_OPEN,
	IS_IF_STATE_READY,
	IS_IF_STATE_START,
	IS_IF_STATE_BUSY,
	IS_IF_STATE_LOGGING
};

struct is_interface {
	unsigned long			state;

	struct workqueue_struct		*workqueue;
	struct work_struct		work_wq[WORK_MAX_MAP];
	struct is_work_list	work_list[WORK_MAX_MAP];

	/* shot timeout check */
	spinlock_t			shot_check_lock;
	atomic_t			shot_check[IS_STREAM_COUNT];
	atomic_t			shot_timeout[IS_STREAM_COUNT];
	/* sensor timeout check */
	atomic_t			sensor_check[IS_SENSOR_COUNT];
	atomic_t			sensor_timeout[IS_SENSOR_COUNT];
	struct timer_list		timer;

	void				*core;
};

int is_interface_probe(struct is_interface *this,
	void *core_data);
int is_interface_open(struct is_interface *this);
int is_interface_close(struct is_interface *this);
#endif
