/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_pmu.h"

#include <linux/timekeeping.h>

#include <asm/string.h>

static struct hts_pmu __percpu *hts_core_pmu;

struct hts_pmu *hts_get_core_handle(int cpu)
{
	return per_cpu_ptr(hts_core_pmu, cpu);
}

static inline void hts_serialize_data(int cpu, struct hts_pmu *pmu, u64 *data, int data_count)
{
	unsigned long flags;
	unsigned int head_idx, tail_idx, left_size, chunk_left_size, new_head_idx, new_tail_idx = 0;
	unsigned int *idxBuffer = (unsigned int *)pmu->pmu_buffer;

	spin_lock_irqsave(&pmu->lock, flags);
	if (pmu->pmu_buffer != NULL) {
		head_idx = idxBuffer[OFFSET_HEAD];
		tail_idx = idxBuffer[OFFSET_TAIL];

		new_head_idx = (head_idx + data_count) % pmu->buffer_size;
		idxBuffer[OFFSET_HEAD] = FLAG_UPDATING | new_head_idx;

		left_size = (pmu->buffer_size + tail_idx - head_idx) % pmu->buffer_size;

		if (left_size &&
			left_size <= data_count) {
			new_tail_idx = (tail_idx + NON_EVENT_DATA_COUNT + pmu->event_count) % pmu->buffer_size;
			idxBuffer[OFFSET_TAIL] = FLAG_UPDATING | new_tail_idx;
		}

		chunk_left_size = pmu->buffer_size - head_idx;
		if (chunk_left_size < data_count) {
			memcpy(&pmu->pmu_buffer[head_idx + OFFSET_DEFAULT], data, DATA_UNIT_SIZE * chunk_left_size);
			memcpy(&pmu->pmu_buffer[OFFSET_DEFAULT], data + chunk_left_size, DATA_UNIT_SIZE * (data_count - chunk_left_size));
		} else {
			memcpy(&pmu->pmu_buffer[head_idx + OFFSET_DEFAULT], data, DATA_UNIT_SIZE * data_count);
		}

		if (left_size &&
			left_size <= data_count)
			idxBuffer[OFFSET_TAIL] = new_tail_idx;

		idxBuffer[OFFSET_HEAD] = new_head_idx;
	}
	spin_unlock_irqrestore(&pmu->lock, flags);
}

void hts_read_core_event(int cpu, struct task_struct *task)
{
	u64 count[MAXIMUM_EVENT_COUNT] = { task->pid, ktime_get() }, read_value;
	int index = NON_EVENT_DATA_COUNT, isFirstPhase = 0;
	struct hts_pmu *pmu = per_cpu_ptr(hts_core_pmu, cpu);
	struct hts_pmu_node *pmu_node;
	struct perf_event *pmuHandle;
	unsigned long flags;

	if (pmu == NULL)
		return;

	spin_lock_irqsave(&pmu->lock, flags);
	list_for_each_entry(pmu_node, &pmu->pmu_list, list) {
		pmuHandle = pmu_node->eventHandle;
		spin_unlock_irqrestore(&pmu->lock, flags);

		if (!IS_ERR_OR_NULL(pmuHandle)) {
			perf_event_read_local(pmuHandle, &read_value, NULL, NULL);

			if (!pmu_node->eventCount) {
				isFirstPhase = true;
			} else {
				count[index] = read_value - pmu_node->eventCount;
			}
			pmu_node->eventCount = read_value;
		}
		else
			count[index] = 0;

		index++;

		spin_lock_irqsave(&pmu->lock, flags);
	}
	spin_unlock_irqrestore(&pmu->lock, flags);

	if (index <= NON_EVENT_DATA_COUNT ||
		isFirstPhase)
		return;

	hts_serialize_data(cpu, pmu, count, index);
}

int hts_configure_core_event(int cpu, int event)
{
	unsigned long flags;
	struct hts_pmu *pmu = per_cpu_ptr(hts_core_pmu, cpu);
	struct hts_pmu_node *node;

	if (pmu == NULL) {
		pr_err("HTS : Couldn't get handle for Core %d configuring", cpu);
		return -EINVAL;
	}

	node = kzalloc(sizeof(struct hts_pmu_node), GFP_KERNEL);
	if (node == NULL)
		return -ENOMEM;

	node->eventAttr.size = sizeof(struct perf_event_attr);
	node->eventAttr.pinned = 1;
	node->eventAttr.type = PERF_TYPE_RAW;
	node->eventAttr.config = event;
	node->eventAttr.config1 = 1;

	INIT_LIST_HEAD(&node->list);

	spin_lock_irqsave(&pmu->lock, flags);
	list_add_tail(&node->list, &pmu->pmu_list);
	pmu->event_count++;
	spin_unlock_irqrestore(&pmu->lock, flags);

	return 0;
}

int hts_configure_core_events(int cpu, int *events, int event_count)
{
	int i, ret, eventNo;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu)
		return -EINVAL;

	if (events == NULL)
		return -EINVAL;

	for (i = 0; i < event_count; ++i) {
		eventNo = events[i];

		ret = hts_configure_core_event(cpu, eventNo);
		if (ret) {
			pr_err("HTS : Couldn't configure event %dth %d for Core %d", i, eventNo, cpu);
			return ret;
		}
	}

	return 0;
}

void hts_clear_core_event(int cpu)
{
	unsigned long flags;
	struct hts_pmu *pmu = NULL;
	struct hts_pmu_node *node, *tmp;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu)
		return;

	pmu = per_cpu_ptr(hts_core_pmu, cpu);
	if (pmu == NULL) {
		pr_err("HTS : Couldn't get handle for Core %d clearing", cpu);
		return;
	}

	spin_lock_irqsave(&pmu->lock, flags);
	list_for_each_entry_safe(node, tmp, &pmu->pmu_list, list) {
	        list_del(&node->list);
		pmu->event_count--;

		kfree(node);
	}
	spin_unlock_irqrestore(&pmu->lock, flags);
}

static void hts_enable_core_event(struct hts_pmu *pmu)
{
	struct hts_pmu_node *pmu_node;
	struct perf_event *pmuHandle = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pmu->lock, flags);
	list_for_each_entry(pmu_node, &pmu->pmu_list, list) {
		pmuHandle = pmu_node->eventHandle;
		spin_unlock_irqrestore(&pmu->lock, flags);

		if (!IS_ERR_OR_NULL(pmuHandle))
			perf_event_enable(pmuHandle);

		spin_lock_irqsave(&pmu->lock, flags);
	}
	spin_unlock_irqrestore(&pmu->lock, flags);
}

int hts_register_all_core_event(void)
{
	int cpu, ret;

	for_each_online_cpu(cpu) {
		ret = hts_register_core_event(cpu);
		if (ret)
			return ret;
	}

	return 0;
}

int hts_register_core_event(int cpu)
{
	int ret = 0;
	struct hts_pmu *pmu = per_cpu_ptr(hts_core_pmu, cpu);
	struct hts_pmu_node *pmu_node;
	unsigned long flags;

	if (pmu == NULL) {
		pr_info("HTS : Doesn't exist event to register");
		return -ENODEV;
	}

	spin_lock_irqsave(&pmu->lock, flags);
	list_for_each_entry(pmu_node, &pmu->pmu_list, list) {
		spin_unlock_irqrestore(&pmu->lock, flags);

		pmu_node->eventHandle = perf_event_create_kernel_counter(&pmu_node->eventAttr, cpu, NULL, NULL, NULL);
		if (IS_ERR_OR_NULL(pmu_node->eventHandle)) {
			pr_err("HTS : Couldn't register event for Core %d", cpu);
			ret = -EINVAL;
			goto err_create_counter;
		}
		spin_lock_irqsave(&pmu->lock, flags);
	}
	spin_unlock_irqrestore(&pmu->lock, flags);

	hts_enable_core_event(pmu);

	return 0;

err_create_counter:
	hts_release_core_event(cpu);

	return ret;
}

void hts_release_core_event(int cpu)
{
	struct hts_pmu *pmu = per_cpu_ptr(hts_core_pmu, cpu);
	struct hts_pmu_node *pmu_node;
	struct perf_event *pmuHandle = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pmu->lock, flags);
	list_for_each_entry(pmu_node, &pmu->pmu_list, list) {
		pmuHandle = pmu_node->eventHandle;
		pmu_node->eventHandle = NULL;
		spin_unlock_irqrestore(&pmu->lock, flags);

		if (!IS_ERR_OR_NULL(pmuHandle))
			perf_event_release_kernel(pmuHandle);

		spin_lock_irqsave(&pmu->lock, flags);
	}
	spin_unlock_irqrestore(&pmu->lock, flags);
}

int initialize_hts_percpu(void)
{
	int cpu;
	struct hts_pmu *pmu;

	hts_core_pmu = alloc_percpu(struct hts_pmu);

	for_each_possible_cpu(cpu) {
		pmu = hts_get_core_handle(cpu);
		pmu->cpu = cpu;
		pmu->event_count = 0;

		spin_lock_init(&pmu->lock);
		INIT_LIST_HEAD(&pmu->pmu_list);
	}

	pr_info("HTS : event preparing has been completed");

	return 0;
}
