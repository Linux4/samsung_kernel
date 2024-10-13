/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/wait.h>
#include <linux/sched.h>
#include "gsp_work_queue.h"
#include "gsp_sync.h"


void gsp_work_queue_init(struct gsp_work_queue *wq)
{
	int i = 0;
	struct gsp_kcfg_info *gsp_kcfg = NULL;

	INIT_LIST_HEAD(&wq->fill_list_head);
	INIT_LIST_HEAD(&wq->empty_list_head);
	INIT_LIST_HEAD(&wq->separate_list_head);
	init_waitqueue_head(&wq->empty_wait);

	mutex_init(&wq->fill_lock);
	mutex_init(&wq->empty_lock);
	mutex_init(&wq->sep_lock);

	wq->fill_cnt = 0;
	wq->empty_cnt = 0;
	wq->sep_cnt = 0;

	if (wq->init == 0) {
		/*create empty members*/
		for(i = 0; i < GSP_EMPTY_MAX; i++) {
			gsp_kcfg = kzalloc(sizeof(struct gsp_kcfg_info), GFP_KERNEL);
			if (gsp_kcfg == NULL) {
				GSP_ERR("gsp empty kcfg create failed\n");
				goto free;
			}
			gsp_kcfg->tag = i;
			list_add_tail(&gsp_kcfg->kcfg_list, &wq->empty_list_head);
			wq->empty_cnt++;
			wq->index[i] = &gsp_kcfg->kcfg_list;
		}
		wq->init = 1;
	} else {
		/*take advantage of index to prevent from leakage*/
		for(i = 0; i < GSP_EMPTY_MAX; i++) {
			list_add_tail(wq->index[i], &wq->empty_list_head);
			wq->empty_cnt++;
		}
	}
	GSP_DEBUG("gsp work queue init empty count: %d\n",
			wq->empty_cnt);
	return;

free:
	for(i = 0; i < wq->empty_cnt; i++) {
		gsp_kcfg = list_first_entry(&wq->empty_list_head,
				struct gsp_kcfg_info, kcfg_list);
		kfree(gsp_kcfg);
	}
}

int gsp_work_queue_push_kcfg(struct gsp_work_queue *wq,
		struct gsp_kcfg_info *kcfg)
{
	int ret = -1;
	mutex_lock(&wq->fill_lock);	
	if (wq->fill_cnt < GSP_FILL_MAX) {
		list_add_tail(&kcfg->kcfg_list, &wq->fill_list_head);
		wq->fill_cnt++;
		ret = 0;
	} 
	mutex_unlock(&wq->fill_lock);

	if (ret)
		GSP_ERR("gsp work queue push failed, fill_cnt: %d\n",
				wq->fill_cnt);
	else
		GSP_DEBUG("gsp push fill kcfg: %d\n", kcfg->tag);

	get_monotonic_boottime(&kcfg->start_time);
	return ret;
}

struct gsp_kcfg_info *gsp_work_queue_pull_kcfg(struct gsp_work_queue *wq)
{
	struct gsp_kcfg_info *kcfg = NULL;
	struct timespec current_time = {-1UL, -1UL};
	struct timespec start_time = {-1UL, -1UL};
	int cnt = -1;
	int elapse = -1;

	mutex_lock(&wq->fill_lock);
	if (wq->fill_cnt > 0) {
		kcfg = list_first_entry(&wq->fill_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		wq->fill_cnt--;
		cnt = wq->fill_cnt;
	}
	mutex_unlock(&wq->fill_lock);

	if (kcfg != NULL) {
		if (kcfg->tag > GSP_EMPTY_MAX
				|| kcfg->tag < 0)
			GSP_ERR("pull an error kcfg\n");
	}

	GSP_DEBUG("wq fill cnt: %d\n", wq->fill_cnt);
	mutex_lock(&wq->sep_lock);
	list_add_tail(&kcfg->kcfg_list, &wq->separate_list_head);
	wq->sep_cnt++;
	mutex_unlock(&wq->sep_lock);

	if (kcfg != NULL) {
		get_monotonic_boottime(&current_time);
		start_time.tv_sec = kcfg->start_time.tv_sec;
		start_time.tv_nsec = kcfg->start_time.tv_nsec;
		elapse = (current_time.tv_sec - start_time.tv_sec)*1000000000
			+ current_time.tv_nsec - start_time.tv_nsec;
		kcfg->sched_cost = div_u64(NS_TO_US(elapse), 1000);
	}
	return kcfg;
}

void gsp_work_queue_destroy(struct gsp_work_queue *wq)
{
	struct list_head temp_list_head;
	struct gsp_kcfg_info *kcfg = NULL;
	int i = 0;
	int temp_count = 0;
	INIT_LIST_HEAD(&temp_list_head);
	mutex_lock(&wq->sep_lock);
	for (i = 0; i < wq->sep_cnt; i++) {
		kcfg = list_first_entry(&wq->separate_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		list_add_tail(&kcfg->kcfg_list, &temp_list_head);
		wq->sep_cnt--;
		temp_count++;
	}
	mutex_unlock(&wq->sep_lock);

	mutex_lock(&wq->fill_lock);
	for (i = 0; i < wq->fill_cnt; i++) {
		kcfg = list_first_entry(&wq->fill_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		list_add_tail(&kcfg->kcfg_list, &temp_list_head);
		wq->fill_cnt--;
		temp_count++;
	}
	mutex_unlock(&wq->fill_lock);

	mutex_lock(&wq->empty_lock);
	for (i = 0; i < temp_count; i++) {
		kcfg = list_first_entry(&temp_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		list_add_tail(&kcfg->kcfg_list, &wq->empty_list_head);
		wq->empty_cnt++;
	}
	mutex_unlock(&wq->empty_lock);

	if (wq->empty_cnt != GSP_EMPTY_MAX)
		GSP_ERR("gsp kcfg memory leak\n");
}

struct gsp_kcfg_info *gsp_work_queue_get_empty_kcfg(
		struct gsp_work_queue *wq, struct gsp_context *ctx)
{
	struct gsp_kcfg_info *kcfg = NULL;
	int ret = -1;
	int count = -1;
retry:
	mutex_lock(&wq->empty_lock);
	count = wq->empty_cnt;
	mutex_unlock(&wq->empty_lock);
	if (count <= 0) {
		GSP_DEBUG("wait empty kcfg\n");
		ret = wait_event_interruptible_timeout(wq->empty_wait,
				(wq->empty_cnt > 0 || ctx->suspend_flag),
				msecs_to_jiffies(500));
		if (ret <= 0) {
			GSP_ERR("wait empty kcfg failed or timeout\n");
			return kcfg;
		}
	}

	mutex_lock(&wq->empty_lock);
	if (wq->empty_cnt > 0) {
		kcfg = list_first_entry(&wq->empty_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		wq->empty_cnt--;
		count = wq->empty_cnt;
		mutex_unlock(&wq->empty_lock);
		GSP_DEBUG("empty kcfg count: %d after acquire\n", count);
	} else {
		if (ctx->suspend_flag) {
			mutex_unlock(&wq->empty_lock);
			GSP_ERR("get null kcfg after enter suspend status\n");
		}
		else {
			count = wq->empty_cnt;
			mutex_unlock(&wq->empty_lock);
			GSP_ERR("wq no empty kcfg so retry\n");
			GSP_ERR("wq  empty num: %d.\n", count);
			goto retry;
		}
	}

	if (kcfg != NULL) {
		if (kcfg->tag > GSP_EMPTY_MAX
			|| kcfg->tag < 0)
			GSP_ERR("get empty kcfg tag error\n");
	}

	return kcfg;
}

struct gsp_kcfg_info *gsp_work_queue_get_sep_kcfg(
		struct gsp_work_queue* wq)
{
	struct gsp_kcfg_info *kcfg = NULL;

	mutex_lock(&wq->sep_lock);
	kcfg = list_first_entry(&wq->separate_list_head,
			struct gsp_kcfg_info, kcfg_list);
	list_del_init(&kcfg->kcfg_list);
	wq->sep_cnt--;
	mutex_unlock(&wq->sep_lock);

	if (kcfg != NULL) {
		if (kcfg->tag > GSP_EMPTY_MAX
				|| kcfg->tag < 0 )
			GSP_ERR("get an error sep kcfg\n");
	}

	return kcfg;
}

void gsp_work_queue_put_fill_list(
		struct gsp_work_queue* wq)
{
	struct gsp_kcfg_info *kcfg = NULL;
	struct list_head temp_list_head;
	struct list_head *temp_list;
	struct list_head *cursor = NULL;
	int i = 0, count = -1;

	GSP_DEBUG("start to put fill list\n");
	INIT_LIST_HEAD(&temp_list_head);
	mutex_lock(&wq->fill_lock);
	count = wq->fill_cnt;
	for (i = 0; i < count; i++) {
		kcfg = list_first_entry(&wq->fill_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		wq->fill_cnt--;
		list_add_tail(&kcfg->kcfg_list, &temp_list_head);
	}
	mutex_unlock(&wq->fill_lock);

	list_for_each_safe(cursor, temp_list, &temp_list_head) {
		kcfg = list_entry(cursor, struct gsp_kcfg_info, kcfg_list);	
		gsp_work_queue_put_back_kcfg(kcfg, wq);	
	}

	if (wq->fill_cnt > 0)
		GSP_ERR("fill list leakage\n");
}

void gsp_work_queue_put_sep_list(
		struct gsp_work_queue* wq)
{
	struct gsp_kcfg_info *kcfg = NULL;
	struct list_head temp_list_head;
	struct list_head *temp_list;
	struct list_head *cursor = NULL;
	int i = 0, count = -1;
	INIT_LIST_HEAD(&temp_list_head);
	GSP_DEBUG("put sep list\n");
	mutex_lock(&wq->sep_lock);
	count = wq->sep_cnt;
	for (i = 0; i < count; i++) {
		kcfg = list_first_entry(&wq->separate_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		wq->sep_cnt--;
		list_add_tail(&kcfg->kcfg_list, &temp_list_head);
	}
	mutex_unlock(&wq->sep_lock);

	GSP_DEBUG("wq sep cnt: %d\n", count);
	list_for_each_safe(cursor, temp_list, &temp_list_head) {
		kcfg = list_entry(cursor, struct gsp_kcfg_info, kcfg_list);	
		gsp_work_queue_put_back_kcfg(kcfg, wq);	
	}

	if (wq->sep_cnt > 0)
		GSP_ERR("sep list leakage\n");
}

void gsp_work_queue_put_back_kcfg(struct gsp_kcfg_info *kcfg,
		struct gsp_work_queue *wq)
{
	if (kcfg == NULL || wq == NULL) {
		GSP_ERR("put back null kcfg or null wq\n");
		return;
	}

	GSP_DEBUG("put back kcfg tag: %d\n", kcfg->tag);
	mutex_lock(&wq->empty_lock);	
	list_del_init(&kcfg->kcfg_list);
	list_add_tail(&kcfg->kcfg_list, &wq->empty_list_head);
	wq->empty_cnt++;
	mutex_unlock(&wq->empty_lock);
}

int gsp_work_queue_has_fill_kcfg(struct gsp_work_queue *wq)
{
	int ret = 0;
	mutex_lock(&wq->fill_lock);
	if (wq->fill_cnt)
		ret = 1;
	mutex_unlock(&wq->fill_lock);

	return ret;
}
struct gsp_kcfg_info *gsp_work_queue_get_fill_kcfg(
		struct gsp_work_queue *wq)
{
	struct gsp_kcfg_info *kcfg = NULL;
	mutex_lock(&wq->fill_lock);
	if (wq->fill_cnt) {
		kcfg = list_first_entry(&wq->fill_list_head,
				struct gsp_kcfg_info, kcfg_list);
		list_del_init(&kcfg->kcfg_list);
		wq->fill_cnt--;
	}
	mutex_unlock(&wq->fill_lock);
	GSP_DEBUG("get fill kcfg tag: %d\n", kcfg->tag);

	if (kcfg != NULL) {
		if (kcfg->tag > GSP_EMPTY_MAX
				|| kcfg->tag < 0 )
			GSP_ERR("get an error fill kcfg\n");
	}

	return kcfg;
}
