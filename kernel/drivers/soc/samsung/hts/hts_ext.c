/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_ext.h"
#include "hts_var.h"
#include "hts_data.h"

#include <linux/plist.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <soc/samsung/exynos-smc.h>

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include <linux/ems.h>

static struct hts_emstune_data
{
	struct hts_data		*data;
	struct notifier_block	notifier;
} emstune_notifier;

#endif

# define plist_check_head(h)    do { } while (0)
/**
 * plist_add - add @node to @head
 *
 * @node:       &struct plist_node pointer
 * @head:       &struct plist_head pointer
 */
static void hts_plist_add(struct plist_node *node, struct plist_head *head)
{
	struct plist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	plist_check_head(head);
	WARN_ON(!plist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (plist_head_empty(head))
		goto ins_node;

	first = iter = plist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct plist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);

	plist_check_head(head);
}

/**
 * plist_del - Remove a @node from plist.
 *
 * @node:       &struct plist_node pointer - entry to be removed
 * @head:       &struct plist_head pointer - list head
 */
static void hts_plist_del(struct plist_node *node, struct plist_head *head)
{
	plist_check_head(head);

	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct plist_node *next;

			next = list_entry(node->node_list.next,
					struct plist_node, node_list);

			/* add the next plist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);

	plist_check_head(head);
}

void hts_remove_request(struct plist_node *req, struct hts_data *data)
{
	unsigned long flags;
	int val;

	spin_lock_irqsave(&data->req_lock, flags);
	hts_plist_del(req, &data->req_list);
	val = plist_last(&data->req_list)->prio;
	spin_unlock_irqrestore(&data->req_lock, flags);

	atomic_set(&data->req_value, val);
}

void hts_update_request(struct plist_node *req, struct hts_data *data, int value)
{
	unsigned long flags;
	int val;

	spin_lock_irqsave(&data->req_lock, flags);
	hts_plist_del(req, &data->req_list);
	plist_node_init(req, value);
	hts_plist_add(req, &data->req_list);
	val = plist_last(&data->req_list)->prio;
	spin_unlock_irqrestore(&data->req_lock, flags);

	atomic_set(&data->req_value, val);
}

#if IS_ENABLED(CONFIG_SCHED_EMS)
static int hts_emstune_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct hts_emstune_data *hts_emsdata = container_of(nb,
						struct hts_emstune_data, notifier);
	int curset_mode = *(int *)v;

	hts_emsdata->data->enable_emsmode = (curset_mode == EMSTUNE_GAME_MODE);

	return NOTIFY_OK;
}

static int hts_emstune_notifier_register(struct platform_device *pdev)
{
	int ret;
	struct hts_data *dev_data = platform_get_drvdata(pdev);

	emstune_notifier.data = dev_data;
	emstune_notifier.notifier.notifier_call = hts_emstune_notifier;

	ret = emstune_register_notifier(&emstune_notifier.notifier);
	if (ret)
		return ret;

	return 0;
}
#else
static int hts_emstune_notifier_register(struct platform_device __unused *pdev)
{
	return 0;
}

#endif

int hts_external_initialize(struct platform_device *pdev)
{
	int ret;

	ret = hts_emstune_notifier_register(pdev);
	if (ret)
		return ret;

	return 0;
}
