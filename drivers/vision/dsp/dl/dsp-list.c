// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-list.h"

void dsp_list_head_init(struct dsp_list_head *head)
{
	head->num = 0;
	head->next = NULL;
	head->prev = NULL;
}

void dsp_list_node_init(struct dsp_list_node *node)
{
	node->next = NULL;
	node->prev = NULL;
}

void dsp_list_node_push_back(struct dsp_list_head *head,
	struct dsp_list_node *node)
{
	head->num++;

	if (head->prev == NULL)
		head->next = node;
	else {
		head->prev->next = node;
		node->prev = head->prev;
	}

	head->prev = node;
}

void dsp_list_node_insert_back(struct dsp_list_head *head,
	struct dsp_list_node *loc, struct dsp_list_node *node)
{
	head->num++;

	if (loc->next == NULL)
		head->prev = node;
	else
		loc->next->prev = node;

	node->prev = loc;
	node->next = loc->next;
	loc->next = node;
}

void dsp_list_node_remove(struct dsp_list_head *head,
	struct dsp_list_node *node)
{
	head->num--;

	if (node->prev == NULL)
		head->next = node->next;
	else
		node->prev->next = node->next;

	if (node->next == NULL)
		head->prev = node->prev;
	else
		node->next->prev = node->prev;
}

void dsp_list_merge(struct dsp_list_head *head, struct dsp_list_head *head1,
	struct dsp_list_head *head2)
{
	head1->prev->next = head2->next;
	head2->next->prev = head1->prev;
	head->prev = head2->prev;
	head->next = head1->next;
	head->num = head1->num + head2->num;
}

int dsp_list_is_empty(struct dsp_list_head *head)
{
	if (head->next == NULL && head->prev == NULL)
		return 1;
	else
		return 0;
}
