/*
 * Samsung EXYNOS IS (Imaging Subsystem) driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_WORK_H
#define IS_WORK_H

/*#define TRACE_WORK*/

#define MAX_WORK_COUNT		40

enum pdp_work_map {
	WORK_PDP_STAT0,
	WORK_PDP_STAT1,
	WORK_PDP_MAX,
};

enum chain_work_map {
	WORK_SHOT_DONE,
	WORK_MAX_MAP
};

struct is_msg {
	u32	id;
	u32	command;
	u32	instance;
	u64	group;
	u32	param1;
	u32	param2;
	u32	param3;
	u32	param4;
};

struct is_work {
	struct list_head	list;
	struct is_msg		msg;
	u32			fcount;
};

struct is_work_list {
	u32			id;
	struct is_work		work[MAX_WORK_COUNT];
	spinlock_t		slock_free;
	spinlock_t		slock_request;
	struct list_head	work_free_head;
	u32			work_free_cnt;
	struct list_head	work_request_head;
	u32			work_request_cnt;
	wait_queue_head_t	wait_queue;
};

void init_work_list(struct is_work_list *this, u32 id, u32 count);

int set_free_work(struct is_work_list *this, struct is_work *work);
int get_free_work(struct is_work_list *this, struct is_work **work);
int set_req_work(struct is_work_list *this, struct is_work *work);
int get_req_work(struct is_work_list *this, struct is_work **work);

/*for debugging*/
int print_fre_work_list(struct is_work_list *this);
int print_req_work_list(struct is_work_list *this);

#endif /* IS_WORK_H */
