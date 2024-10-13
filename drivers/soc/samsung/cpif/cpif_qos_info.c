/*
 * Copyright (C) 2019, Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "cpif_qos_info.h"
#include "modem_v1.h"

static struct hiprio_uid_list g_hiprio_uid_list;

int cpif_qos_init_list(void)
{
	hash_init(g_hiprio_uid_list.uid_map);
	return 0;
}

struct hiprio_uid_list *cpif_qos_get_list(void)
{
	return &g_hiprio_uid_list;
}

struct hiprio_uid *cpif_qos_get_node(u32 uid)
{
	struct hiprio_uid *node;

	hash_for_each_possible(g_hiprio_uid_list.uid_map, node, h_node, uid) {
		if (node->uid == uid)
			return node;
	}

	return NULL;
}

bool cpif_qos_add_uid(u32 uid)
{
	struct hiprio_uid *new_uid;

	if (cpif_qos_get_node(uid)) {
		mif_err("---- uid(%d) already exists in the list\n", uid);
		return false;
	}
	new_uid = kzalloc(sizeof(struct hiprio_uid), GFP_ATOMIC);
	if (!new_uid)
		return false;

	new_uid->uid = uid;
	hash_add(g_hiprio_uid_list.uid_map, &new_uid->h_node, uid);

	return true;
}

bool cpif_qos_remove_uid(u32 uid)
{
	struct hiprio_uid *node = cpif_qos_get_node(uid);

	if (!node) {
		mif_err("---- uid(%d) does not exist in the list\n", uid);
		return false;
	}
	hash_del(&node->h_node);
	kfree(node);

	return true;
}
