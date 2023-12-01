/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include "data_path.h"

#define CAMELLIA_STATE_UP			1
#define CAMELLIA_STATE_ONLINE		2
#define CAMELLIA_STATE_DOWN			3
#define CAMELLIA_STATE_OFFLINE		4

#define PM_WAIT_TIME				2

#define MAX_NUMBER_OF_RETRY			10

#define CAMELLIA_PM_TIMER_ON		1
#define CAMELLIA_PM_TIMER_OFF		0

#define SHM_START_ADDR	0xD00000
#define SHM_START_PAGE	(SHM_START_ADDR >> PAGE_SHIFT)
#define SHM_SIZE	0x280000	// 0xD00000 ~ 0xF80000

#define SHM_USE_NO_ONE	0

#define SEQ_SHIFT	4
#define SEQ_MASK	0x07

#define SHM_SETTING_FLAG_TRUE 1
#define SHM_SETTING_FLAG_FALSE 0

struct camellia_log_work {
	struct work_struct work;
	u8 event;
};

struct camellia_coredump_work {
	struct work_struct work;
	u8 event;
};

struct camellia_device {
	struct mutex lock;
	struct mutex logsink_lock;
	struct mutex coredump_lock;
	unsigned int state;
	unsigned int pm_timer_set;
	atomic_t refcnt;
	struct timer_list pm_timer;
	struct workqueue_struct *pm_wq;
	struct work_struct pm_work;
	struct workqueue_struct *log_wq;
	struct workqueue_struct *coredump_wq;
	struct camellia_log_work log_work;
	struct camellia_coredump_work coredump_work;
	atomic_t shm_user;
	wait_queue_head_t shm_wait_queue;
};

struct camellia_memory_entry {
	struct list_head node;
	unsigned long addr;
	unsigned long size;
	dma_addr_t handle;
	struct camellia_client *client;
};

struct camellia_client {
	struct list_head node;
	unsigned int peer_id;
	struct list_head shmem_list;
	struct spinlock shmem_list_lock;
	unsigned int current_dump_offset;
	struct spinlock timeout_set_lock;
	uint64_t timeout_jiffies;
	void *dump_vbase;
	unsigned int pm_cnt;
};

struct camellia_reserved_memory {
	unsigned long base;
	unsigned long size;
};

extern struct camellia_device camellia_dev;
extern struct camellia_reserved_memory revmem;

struct camellia_client *camellia_create_client(unsigned int peer_id);
void camellia_destroy_client(struct camellia_client *client);

void *camellia_request_region(unsigned long addr, unsigned int size);

#endif /* __UTILS_H__ */
