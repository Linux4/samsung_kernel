/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <linux/types.h>
#include <linux/perf/arm_pmu.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#define MAXIMUM_EVENT_COUNT             (64)

#define FLAG_UPDATING			(0x80000000)
#define OFFSET_HEAD			(0)
#define OFFSET_TAIL			(1)
#define OFFSET_DEFAULT			(1)

#define DATA_UNIT_SIZE			(sizeof(unsigned long))
#define NON_EVENT_DATA_COUNT		(2)

struct hts_pmu_node {
	struct list_head	list;

	struct perf_event_attr 	eventAttr;
	struct perf_event	*eventHandle;

	unsigned long		eventCount;
};

struct hts_pmu {
	int			cpu;
	int			event_count;

	struct list_head	pmu_list;

	spinlock_t		lock;

	struct page		*pmu_pages;
	unsigned long		*pmu_buffer;
	unsigned long		buffer_size;
	atomic_t		ref_count;
};

struct hts_pmu *hts_get_core_handle(int cpu);
void hts_read_core_event(int cpu, struct task_struct *task);
int hts_configure_core_buffer(int cpu, char *buffer);
int hts_configure_core_event(int cpu, int event);
int hts_configure_core_events(int cpu, int *events, int event_count);
void hts_clear_core_event(int cpu);
int hts_register_all_core_event(void);
int hts_register_core_event(int cpu);
void hts_release_core_event(int cpu);

int initialize_hts_percpu(void);
