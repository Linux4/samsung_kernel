// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc Qogir N6 pro sipa driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Qingsheng Li <qingsheng.li@unisoc.com>
 */

#ifndef _SIPA_RM_H_
#define _SIPA_RM_H_

#include <linux/workqueue.h>
#include <linux/sipa.h>

#define SIPA_RM_DRV_NAME "sipa_rm"

#define SIPA_RM_MAX_WAIT_TIME (HZ * 10)

#define SIPA_RM_RES_CONS_MAX \
	(SIPA_RM_RES_MAX - SIPA_RM_RES_PROD_MAX)
#define SIPA_RM_RESORCE_IS_PROD(x) \
	(x < SIPA_RM_RES_PROD_MAX)
#define SIPA_RM_RESORCE_IS_CONS(x) \
	(x >= SIPA_RM_RES_PROD_MAX && x < SIPA_RM_RES_MAX)
#define SIPA_RM_INDEX_INVALID	(-1)
#define SIPA_RM_RELEASE_DELAY_MS 1000

int sipa_rm_prod_index(enum sipa_rm_res_id resource_name);
int sipa_rm_cons_index(enum sipa_rm_res_id resource_name);

/**
 * enum sipa_rm_wq_cmd - workqueue commands
 */
enum sipa_rm_wq_cmd {
	SIPA_RM_WQ_NOTIFY_CONS,
	SIPA_RM_WQ_NOTIFY_PROD,
	SIPA_RM_WQ_RESOURCE_CB
};

/**
 * struct sipa_rm_wq_work_type - SIPA RM worqueue specific
 *				work type
 * @work: work struct
 * @wq_cmd: command that should be processed in workqueue context
 * @resource_name: name of the resource on which this work
 *			should be done
 * @dep_graph: data structure to search for resource if exists
 * @event: event to notify
 */
struct sipa_rm_wq_work_type {
	enum sipa_rm_wq_cmd		wq_cmd;
	enum sipa_rm_res_id	resource_name;
	enum sipa_rm_event		event;
};

int sipa_rm_wq_send_cmd(enum sipa_rm_wq_cmd wq_cmd,
			enum sipa_rm_res_id resource_name,
			enum sipa_rm_event event);

int sipa_rm_init(void);

const char *sipa_rm_res_str(enum sipa_rm_res_id resource_name);

int sipa_rm_stat(char *buf, int size);

void sipa_rm_exit(void);

#endif /* _SIPA_RM_H_ */

