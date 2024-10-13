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
#include "gsp_sync.h"
#include "gsp_kcfg.h"
#include "scaler_coef_cal.h"
#include "gsp_config_if.h"
#include "gsp_work_queue.h"
#include "gsp_drv_common.h"
#include "gsp_debug.h"

/*
gsp_kcfg_leak_check
description: in ioctl, if user process killed after get kcfg from empty but before put to fill,
                 the kcfg maybe dissociate from managed list, maybe there is the other scenario exsit,
                 that cause kcfg leak, this function check this case and recover it.
*/
static void gsp_work_queue_leak_check(struct gsp_work_queue *wq)
{
    int32_t count = 0;
	mutex_lock(&wq->empty_lock);	
	count = wq->empty_cnt;
	mutex_unlock(&wq->empty_lock);	
    /*if empty-list-cnt not equal the total-kcfg-cnt, reinitialize the kcfg list*/
    if(count != GSP_EMPTY_MAX) {
        GSP_ERR("leak check: empty cnt %d != total cnt %d\n",
				count, GSP_EMPTY_MAX);

        /*cmd list initialize*/
		gsp_work_queue_init(wq);	
    }
}

static void gsp_suspend_process(struct gsp_context *ctx)
{
    struct gsp_kcfg_info *kcfg = NULL;

	if (ctx->suspend_flag == 0) {
		GSP_ERR("no suspend\n");
		return;
	}

	kcfg = gsp_work_queue_get_fill_kcfg(ctx->wq);
	while(kcfg != NULL) {
		if(kcfg->cfg.misc_info.async_flag) {
			/*put all acq fence */
			gsp_layer_list_wait_fence_free(kcfg->data.wait_fen_arr,
					&kcfg->data.wait_cnt);
			/*signal release fence*/
			gsp_sync_fence_signal(ctx->timeline);
		} else {
			if (kcfg->done == 0)
				kcfg->done++;
			wake_up_interruptible_all(&(ctx->sync_wait));
		}
		gsp_work_queue_put_back_kcfg(kcfg, ctx->wq);
		kcfg = gsp_work_queue_get_fill_kcfg(ctx->wq);
	};

	/*if gsp composed done, notify suspend thread*/
	if(GSP_WORKSTATUS_GET(ctx->gsp_reg_addr) == 0) {
		/*if gsp free, and sep list is not empty, move it to empty list*/
		gsp_work_queue_put_sep_list(ctx->wq);
		gsp_work_queue_leak_check(ctx->wq);
		init_waitqueue_head(&ctx->sync_wait);
		GSP_DEBUG("work thread suspend done.\n");
		up(&ctx->suspend_clean_done_sema);
	}
}

/*
func:gsp_calc_exe_time
desc:calc and return the execute time of core, in us unit.
*/
static uint32_t gsp_calc_exe_time(struct gsp_kcfg_info *kcfg)
{
    struct timespec start_time = {-1UL,-1UL};// the earliest start core
    struct timespec current_time = {-1UL,-1UL};
    long long elapse = 0;
	if (NULL == kcfg) {
		GSP_ERR("calc exe time with null kcfg\n");
		return -1;	
	}

    get_monotonic_boottime(&current_time);

    start_time.tv_sec = kcfg->start_time.tv_sec;
    start_time.tv_nsec = kcfg->start_time.tv_nsec;
    elapse = (current_time.tv_sec - start_time.tv_sec)*1000000000 + current_time.tv_nsec - start_time.tv_nsec;
    return NS_TO_US(elapse);
}

static long gsp_show_current_time(void)
{
	struct timespec current_time = {-1UL,-1UL};
	get_monotonic_boottime(&current_time);

	return timespec_to_ns(&current_time);
}

int32_t gsp_wait_fence(struct gsp_kcfg_info *kcfg)
{
    uint32_t i = 0;
    int32_t ret = 0;
    long timeout = GSP_WAIT_FENCE_TIMEOUT;
	uint32_t count = kcfg->data.wait_cnt;
	struct sync_fence **fen_arr = kcfg->data.wait_fen_arr;
    struct timespec start_time = {-1UL,-1UL};// the earliest start core
    struct timespec current_time = {-1UL,-1UL};
    long long elapse = 0;

    GSP_DEBUG("enter %s\n", __func__);
    get_monotonic_boottime(&start_time);
    for (i = 0; i < count; i++) {
        ret = sync_fence_wait(fen_arr[i], timeout);
        GSP_DEBUG("work thread wait fence %d/%d %s.\n",
				i+1, count, ret?"failed":"success");
        if(ret) {
            timeout = 200;/*timeout occurs, decrease the next timeout*/
			GSP_ERR("work thread wait %d/%d fence timeout\n", i+1, count);
			GSP_ERR("fence name: %s\n", fen_arr[i]->name);
        }
        sync_fence_put(kcfg->data.wait_fen_arr[i]);
    }
    get_monotonic_boottime(&current_time);
    elapse = (current_time.tv_sec - start_time.tv_sec)*1000000000
			+ current_time.tv_nsec - start_time.tv_nsec;
	kcfg->wait_cost = div_u64(NS_TO_US(elapse), 1000);
    GSP_DEBUG("exit %s\n", __func__);

    return ret;
}

static int gsp_work_pre_process(struct gsp_context *ctx)
{
	int ret = -1;
	struct gsp_kcfg_info *kcfg = NULL;

	GSP_DEBUG("work thread fetch a kcfg from fill-list.\n");

	ret = gsp_work_queue_has_fill_kcfg(ctx->wq);
	if (ret == 0) {
		GSP_DEBUG("work thread has no task\n");	
		ret = 0;
		goto exit;
	}

	/*fetch a kcfg from fill list head, and add it to dissociation list tail*/
	kcfg = gsp_work_queue_pull_kcfg(ctx->wq);
	if(kcfg == NULL) {
		GSP_ERR("work thread get fill kcfg failed!\n");
		goto exit;
	} 

	ret = gsp_enable(ctx);
	if(ret) {
		GSP_ERR("work thread enable failed!\n");
		goto err_status_set;
	}

	ret = gsp_kcfg_iommu_map(kcfg);
	if(ret) {
		GSP_ERR("work thread iommu map failed!\n");
		goto err_status_set;
	}

	gsp_coef_gen_and_cfg(&ctx->gsp_coef_force_calc, &kcfg->cfg,
			&ctx->cache_coef_init_flag, ctx->gsp_reg_addr, &ctx->coef_list);
	ret = gsp_kcfg_reg_set(kcfg, ctx->gsp_reg_addr);
	if(ret) {
		GSP_ERR("work thread cfg failed!\n");
		goto err_status_set;
	}

	if(kcfg->cfg.misc_info.async_flag == 1) {
		ret = gsp_wait_fence(kcfg);
		if(ret) {
			GSP_ERR("work thread wait fence failed!\n");
			goto err_status_set;
		}
	}
	ret = gsp_trigger(ctx->gsp_reg_addr, ctx->mmu_reg_addr);
	if(ret) {
		GSP_ERR("work thread trigger failed!\n");
		goto err_status_set;
	} else {
		GSP_DEBUG("work thread trigger %u kcfg success!\n",
				kcfg->frame_id);
		gsp_status_set(ctx, GSP_STATUS_BUSY);
		gsp_current_kcfg_set(ctx, kcfg);
		GSP_DEBUG("work thread at %ld start to write buffer: 0x%x\n",
				gsp_show_current_time(), gsp_kcfg_get_des_addr(kcfg));
		goto exit;
	}

err_status_set:
	gsp_status_set(ctx, GSP_STATUS_ERR);
exit:
	return ret;
}

static void gsp_timeout_process(struct gsp_context *ctx)
{
	struct timespec start_time = {-1UL,-1UL};// the earliest start core
	struct timespec current_time = {-1UL,-1UL};
	long long elapse = 0;
	int32_t i = 0;
	struct gsp_kcfg_info *kcfg = NULL;
	kcfg = gsp_current_kcfg_get(ctx);

	get_monotonic_boottime(&current_time);
	i = 0;
	/*means module enabled, or register read only get zero*/
	if((gsp_status_get(ctx))
		&& (GSP_WORKSTATUS_GET(ctx->gsp_reg_addr))) {
		start_time.tv_sec = kcfg->start_time.tv_sec;
		start_time.tv_nsec = kcfg->start_time.tv_nsec;

		elapse = (current_time.tv_sec - start_time.tv_sec)*1000000000
			+ current_time.tv_nsec - start_time.tv_nsec;
		if(elapse < 0 || elapse >= MS_TO_NS(GSP_CFG_EXE_TIMEOUT)) {
			GSP_ERR("work thread wait timeout, try to recovery.\n");
			gsp_timeout_print(ctx);
			/*recovery this core*/
			gsp_try_to_recover(ctx);
		}
	}
}

/*what this function handle is as follows:
 * 1.If gsp is busy then coninue to wait it done
 * 2.If gsp completed work then release resources and
 *   sync mode: notify waitqueue
 *   async mode: signal fence*/
static int gsp_work_post_process(struct gsp_context *ctx)
{
	struct gsp_kcfg_info *kcfg = NULL;
	int ret = 0;
	unsigned long gsp_addr = ctx->gsp_reg_addr;
	int real_cost = -1;
	int cost = -1;
	int err = 0;
	if (NULL == ctx) {
		GSP_ERR("post with null context\n");
		return -1;
	}

	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp contexti at ISR\n");
        return -1;
    }
	GSP_DEBUG("enter %s\n", __func__);

	ret = gsp_status_get(ctx);
	if (ret == GSP_STATUS_IDLE) {
		GSP_DEBUG("work thread no resource to release\n");
		return 0;
	} else if (ret == GSP_STATUS_ERR) {
		err = 1;/*means pre-process failed*/
		GSP_DEBUG("release resource because pre-process failed\n");
	} else if (ret == GSP_STATUS_ISR_DONE)
		GSP_DEBUG("task has been completed so release hw\n");

	if(GSP_WORKSTATUS_GET(gsp_addr) != 0) {
		kcfg = gsp_current_kcfg_get(ctx);
		if (NULL != kcfg)
			GSP_DEBUG("work thread %u kcfg still busy\n",
					kcfg->frame_id);
		return -1;
	}

	kcfg = gsp_work_queue_get_sep_kcfg(ctx->wq);
	if (kcfg == NULL) {
		GSP_ERR("work thread get sep kcfg failed\n");
		return -1;
	}

	gsp_status_set(ctx, GSP_STATUS_IDLE);

	/*if has err, only release resources*/
	if (err != 1) {
		GSP_DEBUG("work thread %u kcfg composition done.\n",
				kcfg->frame_id);
		cost = gsp_calc_exe_time(kcfg)/1000;
		real_cost = cost - kcfg->wait_cost - kcfg->sched_cost;	
		GSP_DEBUG("work thread wait fence cost %d ms, "
				"schedule cost %d ms, real cost %d ms.\n",
				kcfg->wait_cost, kcfg->sched_cost, real_cost);
		kcfg->done = 1;
	}
	gsp_kcfg_iommu_unmap(kcfg);/*must before gsp disable*/
	gsp_disable(ctx);

	gsp_current_kcfg_set(ctx, NULL);
	/*async process*/
	if(kcfg->cfg.misc_info.async_flag) {
		/*signal release fence*/
		GSP_DEBUG("work thread signal fence\n");
		gsp_sync_fence_signal(ctx->timeline);
		gsp_work_queue_put_back_kcfg(kcfg, ctx->wq);
		wake_up_interruptible_all(&ctx->wq->empty_wait);
	} else {/*sync process*/
		GSP_DEBUG("work thread wake up sync ioctl process\n");
		gsp_work_queue_put_back_kcfg(kcfg, ctx->wq);
		wake_up_interruptible_all(&ctx->sync_wait);
		wake_up_interruptible_all(&ctx->wq->empty_wait);
	}
	return 0;
}


void gsp_coef_cache_init(struct gsp_context *ctx)
{
    uint32_t i = 0;

    if(ctx->cache_coef_init_flag == 0) {
        i = 0;
        INIT_LIST_HEAD(&ctx->coef_list);
        while(i < GSP_COEF_CACHE_MAX) {
            list_add_tail(&ctx->coef_cache[i].entry_list, &ctx->coef_list);
            i++;
        }
        ctx->cache_coef_init_flag = 1;
    }
}

int gsp_work_thread(void *data)
{
    struct gsp_context *ctx = (struct gsp_context *)data;
    int32_t ret = 0;

    if(ctx == NULL) {
        GSP_ERR("ctx == NULL, return!\n");
        return -1;
    }

	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context, return!\n");
		return -1;
	}

    GSP_DEBUG("%s enter!\n", __func__);
    gsp_coef_cache_init(ctx);

    while(1) {
        if(ctx->suspend_flag == 1) {
            gsp_suspend_process(ctx);
        }

        ret = down_interruptible(&ctx->wake_sema);
        if(ret == -EINTR) {/*INT */
			GSP_ERR("work thread wait encounter INT\n");
            gsp_timeout_process(ctx);
        }

        ret = gsp_work_post_process(ctx);
		if (ret)
			continue;
        if(ctx->suspend_flag == 0) {
			/*get kcfgs from filled list and assign them to gsp*/
            ret = gsp_work_pre_process(ctx);
            if(ret) {
                GSP_ERR("work thread pre process err!\n");
                up(&ctx->wake_sema);/*leave the err kcfg to post process*/
            }
        }
    }
    return 0;
}

