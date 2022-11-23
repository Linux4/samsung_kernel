// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc sipa driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Qingsheng Li <qingsheng.li@unisoc.com>
 */

#ifndef _SIPA_RM_DEP_GRAPH_H_
#define _SIPA_RM_DEP_GRAPH_H_

#include <linux/list.h>
#include <linux/sipa.h>
#include "sipa_rm_res.h"

struct sipa_rm_dep_graph {
	struct sipa_rm_resource *resource_table[SIPA_RM_RES_MAX];
};

int sipa_rm_dep_graph_get_resource(
	struct sipa_rm_dep_graph *graph,
	enum sipa_rm_res_id name,
	struct sipa_rm_resource **resource);

int sipa_rm_dep_graph_create(struct sipa_rm_dep_graph **dep_graph);

void sipa_rm_dep_graph_delete(struct sipa_rm_dep_graph *graph);

int sipa_rm_dep_graph_add(struct sipa_rm_dep_graph *graph,
			  struct sipa_rm_resource *resource);

int sipa_rm_dep_graph_remove(struct sipa_rm_dep_graph *graph,
			     enum sipa_rm_res_id resource_name);

int sipa_rm_dep_graph_add_dependency(struct sipa_rm_dep_graph *graph,
				     enum sipa_rm_res_id resource_name,
				     enum sipa_rm_res_id depends_on_name,
				     bool userspsace_dep);

int sipa_rm_dep_graph_delete_dependency(struct sipa_rm_dep_graph *graph,
					enum sipa_rm_res_id resource_name,
					enum sipa_rm_res_id depends_on_name,
					bool userspsace_dep);

#endif /* _SIPA_RM_DEP_GRAPH_H_ */
