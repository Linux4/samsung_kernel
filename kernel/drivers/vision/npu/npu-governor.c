/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-governor.h"
#include "npu-device.h"
#include "npu-qos.h"
#include "npu-util-common.h"
#include "npu-util-regs.h"
#include "dsp-dhcp.h"
#include <linux/delay.h>
#include <uapi/linux/unistd.h>
#include "npu-hw-device.h"
#include "npu-protodrv.h"
#include "npu-dvfs.h"
#include <soc/samsung/exynos/exynos-soc.h>

#define MAX_MIF_FREQ_LEVELS 13
#define MAX_INT_FREQ_LEVELS 13

static struct npu_freq_info freq_level[MAX_FREQ_LEVELS];
static u32 mif_freq_level[MAX_MIF_FREQ_LEVELS];
static u32 int_freq_level[MAX_INT_FREQ_LEVELS];

static u32 npu_make_requester_id(u32 uid, u32 from)
{
	return (uid << 16 | from);
}

static bool npu_governor_is_unavailable(void) {
	return npu_get_perf_mode();
}

static inline void set_int_mif_freq(struct npu_sessionmgr *sess_mgr, int freq_idx)
{
	u32 request_freq_idx = max(0, min(MAX_MIF_FREQ_LEVELS - 1, freq_idx));

	npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_mif,
			mif_freq_level[request_freq_idx]);
	npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_int,
			int_freq_level[request_freq_idx]);
}


static int npu_governor_parsing_dt(struct npu_device *device)
{
	int i = 0, ret = 0;
	u32 governor_num[NPU_GOVERNOR_NUM];
	struct device *dev = device->dev;

	ret = of_property_read_u32_array(dev->of_node, "samsung,npugovernor-num", governor_num, NPU_GOVERNOR_NUM);
	if (ret) {
		probe_err("failed parsing governor dt(npugovernor-num)\n");
		return ret;
	}

	/* npu - dnc engine */
	ret = of_property_read_u32_array(dev->of_node, "samsung,npugovernor-npufreq",
			(u32 *)freq_level, NPU_FREQ_INFO_NUM * MAX_FREQ_LEVELS);
	if (ret) {
		probe_err("failed parsing governor dt(npugovernor-npufreq)\n");
		return ret;
	}

	probe_info("Governor NPU (DNC NPU-core)\n");
	for (i = 0; i < governor_num[GOV_NPU_FREQ]; i++)
		probe_info("[%d] %u %u\n", i, freq_level[i].dnc, freq_level[i].npu);

	/* MIF */
	ret = of_property_read_u32_array(dev->of_node, "samsung,npugovernor-miffreq",
			mif_freq_level, MAX_MIF_FREQ_LEVELS);
	if (ret) {
		probe_err("failed parsing governor dt(npugovernor-miffreq)\n");
		return ret;
	}

	probe_info("Governor MIF\n");
	for (i = 0; i < governor_num[GOV_MIF_FREQ]; i++)
		probe_info("[%d] %u\n", i, mif_freq_level[i]);

	/* INT */
	ret = of_property_read_u32_array(dev->of_node, "samsung,npugovernor-intfreq",
			int_freq_level, MAX_INT_FREQ_LEVELS);
	if (ret) {
		probe_err("failed parsing governor dt(npugovernor-intfreq)\n");
		return ret;
	}

	probe_info("Governor INT\n");
	for (i = 0; i < governor_num[GOV_INT_FREQ]; i++)
		probe_info("[%d] %u\n", i, int_freq_level[i]);

	return ret;
}

static void npu_update_frequency_change(struct npu_sessionmgr *sess_mgr, u32 update_freq_index)
{
	struct npu_device *device;
	struct npu_memory_buffer *cmdq_table_buf;
	struct cmdq_table *cmdq_table_addr, *cur_cmdq_table;
	struct npu_cmdq_progress *cur, *next;

	device = container_of(sess_mgr, struct npu_device, sessionmgr);
	cmdq_table_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!cmdq_table_buf) {
		npu_err("\n CMDQ Table buffer not present\n");
		return;
	}

	cmdq_table_addr = (struct cmdq_table *)cmdq_table_buf->vaddr;

	list_for_each_entry_safe(cur, next, &sess_mgr->cmdq_head, list) {
		cur_cmdq_table = &cmdq_table_addr[cur->session->uid];

		if ((cur_cmdq_table->status & CMDQ_PROCESSING)
				&& (cur->infer_freq_index != update_freq_index)) {
			cur->freq_changed = true;
		} else if (cur_cmdq_table->status == CMDQ_FREE) {
			cur->infer_freq_index = update_freq_index;
		}
	}
}


void set_new_freq(struct npu_sessionmgr *sess_mgr, u32 freq_index, u32 requester)
{
	s64 start, end;
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	struct npu_device *device = container_of(sess_mgr, struct npu_device, sessionmgr);

	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev < 2)) {
		if (atomic_read(&device->power_active) != NPU_DEVICE_PWR_ACTIVE) {
			return;
		}
	}
#endif

	if (sess_mgr->cmdq_list_changed_from != requester)
		return;

	if (mutex_is_locked(&sess_mgr->freq_set_lock) && ((freq_index - atomic_read(&sess_mgr->freq_index_cur) < 2))) {
		return;
	}

	mutex_lock(&sess_mgr->freq_set_lock);
	if (((requester & 0xffff) == 0x3) && (freq_index <= atomic_read(&sess_mgr->freq_index_cur))) {
		mutex_unlock(&sess_mgr->freq_set_lock);
		return;
	} else if (freq_index == atomic_read(&sess_mgr->freq_index_cur)) {
		mutex_unlock(&sess_mgr->freq_set_lock);
		return;
	}

	atomic_set(&sess_mgr->freq_index_cur, freq_index);

	start = npu_get_time_us();
	npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_dnc,
			freq_level[freq_index].dnc);

	if (sess_mgr->npu_flag || !freq_index) {
		npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_npu0,
				freq_level[freq_index].npu);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
		npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_npu1,
				freq_level[freq_index].npu);
#endif
	}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if (sess_mgr->dsp_flag || !freq_index)
		npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_dsp,
				freq_level[freq_index].npu);
#endif

	if (!sess_mgr->npu_flag && !sess_mgr->dsp_flag) {
		npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_npu0,
				freq_level[freq_index].npu);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
		npu_dvfs_pm_qos_update_request(&sess_mgr->npu_qos_req_npu1,
				freq_level[freq_index].npu);
#endif
	}

	end = npu_get_time_us();
	sess_mgr->time_for_setting_freq =
		(3 * sess_mgr->time_for_setting_freq + 2 * (end - start)) >> 2;

	mutex_lock(&sess_mgr->cmdq_lock);
	npu_update_frequency_change(sess_mgr, atomic_read(&sess_mgr->freq_index_cur));
	mutex_unlock(&sess_mgr->cmdq_lock);

	mutex_unlock(&sess_mgr->freq_set_lock);
}

static u32 get_next_set_freq(struct npu_sessionmgr *sess_mgr, u32 *next_mif_level)
{
	struct npu_session *cur = NULL;
	bool is_happening_soon;

	u32 max_freq = 0;
	s64 remain_to_next_qbuf = 0;
	s64 now = 0;

	mutex_lock(&sess_mgr->active_list_lock);
	list_for_each_entry(cur, &sess_mgr->active_session_list, active_session) {
		if(!cur->earliest_qbuf_time) {
			continue;
		}
		now = npu_get_time_us();
		remain_to_next_qbuf = abs(cur->earliest_qbuf_time - now)
			-(sess_mgr->time_for_setting_freq + 1000);
		is_happening_soon = remain_to_next_qbuf < 0;

		if(is_happening_soon && (max_freq < cur->prev_npu_level)) {
			max_freq = cur->prev_npu_level;
			if(next_mif_level) *next_mif_level = cur->next_mif_level;
		}
	}

#ifdef GOVERNOR_LOG
	if (cur)
		npu_dbg("uid %d cnt : %d, "
				"earliest_qbuf_time : %lld, "
				"now : %lld, "
				"|sess->earliest_qbuf_time - now| = %lld, "
				"CLKSETTIME : %d\n" ,
				cur->uid,
				atomic_read(&sess_mgr->queue_cnt),
				cur->earliest_qbuf_time,
				now,
				remain_to_next_qbuf,
				sess_mgr->time_for_setting_freq);
#endif

	mutex_unlock(&sess_mgr->active_list_lock);
	return max_freq;
}

int thr_func(void *data)
{
	struct npu_device *device;
	struct npu_sessionmgr *sess_mgr;
	struct thr_data *thr_data = (struct thr_data *)data;
	struct cmdq_table_info *cmdq_info = thr_data->ptr;
	u32 prev_npu_level = 0, next_mif_level = 0;

	device = container_of(cmdq_info, struct npu_device, cmdq_table_info);
	sess_mgr = &device->sessionmgr;

	while(!kthread_should_stop()) {
		prev_npu_level = get_next_set_freq(sess_mgr, &next_mif_level);
		if (!atomic_read(&sess_mgr->queue_cnt)) {
			if (prev_npu_level > 0) {
				sess_mgr->cmdq_list_changed_from = 1;
				set_int_mif_freq(sess_mgr, next_mif_level);
				set_new_freq(sess_mgr, prev_npu_level, 1);
			} else {
				sess_mgr->cmdq_list_changed_from = 2;
				set_int_mif_freq(sess_mgr, 0);
				set_new_freq(sess_mgr, prev_npu_level, 2);
			}
		}

		usleep_range(800, 1000);
	}
	set_int_mif_freq(sess_mgr, 0);
	set_new_freq(sess_mgr, 0, sess_mgr->cmdq_list_changed_from);

	return 0;
}

int start_cmdq_table_read(struct cmdq_table_info *cmdq_info)
{

	if (npu_governor_is_unavailable()) {
		npu_cmdq_table_read_close(cmdq_info);
		return 0;
	}

	mutex_lock(&cmdq_info->lock);
	if (!cmdq_info->kthread) {
		cmdq_info->kthread = kthread_run(thr_func, &cmdq_info->thr_data, "thr");
		if (IS_ERR_OR_NULL(cmdq_info->kthread)) {
			cmdq_info->kthread = NULL;
			mutex_unlock(&cmdq_info->lock);
			return -ENOMEM;
		}
	}
	mutex_unlock(&cmdq_info->lock);
	return 0;
}

static inline u32 npu_get_predict_time(struct npu_session *session, u32 freq_index)
{
	return((s64)(session->model_hash_node->alpha *
				session->model_hash_node->exec[freq_index].x)
			+ (s64)(session->model_hash_node->beta *
				session->model_hash_node->exec[freq_index].z));
}

static u32 npu_get_current_progress(struct npu_sessionmgr *sess_mgr, struct npu_session *session, s32 predict_time)
{
	struct npu_device *device;
	struct npu_memory_buffer *cmdq_table_buf;
	struct cmdq_table *cmdq_table_addr, *cur_cmdq_table;
	s64 cur_progress = predict_time;
	s32 cur_pc;
	struct npu_iomem_area *gnpu0;

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	struct npu_iomem_area *gnpu1;
#endif

	device = container_of(sess_mgr, struct npu_device, sessionmgr);
	cmdq_table_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!cmdq_table_buf) {
		npu_err("\n CMDQ Table buffer not present\n");
		return cur_progress;
	}

	gnpu0 = npu_get_io_area(&device->system, "sfrgnpu0");
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	gnpu1 = npu_get_io_area(&device->system, "sfrgnpu1");
#endif

	cmdq_table_addr = (struct cmdq_table *)cmdq_table_buf->vaddr;
	cur_cmdq_table = &cmdq_table_addr[session->uid];

	switch (cur_cmdq_table->status) {
		case CMDQ_PROCESSING_INST1 :
#if IS_ENABLED(CONFIG_SOC_S5E9945)
			cur_pc = npu_read_hw_reg(gnpu0, 0x2300, 0xFFFFFFFF, 1)
				+ npu_read_hw_reg(gnpu1, 0x2300, 0xFFFFFFFF, 1);
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
			cur_pc = npu_read_hw_reg(gnpu0, 0x2300, 0xFFFFFFFF, 1);
#endif
			break;
		case CMDQ_PROCESSING_INSTN0 :
			cur_pc = npu_read_hw_reg(gnpu0, 0x2300, 0xFFFFFFFF, 1);
			break;
#if IS_ENABLED(CONFIG_SOC_S5E9945)
		case CMDQ_PROCESSING_INSTN1 :
			cur_pc = npu_read_hw_reg(gnpu1, 0x2300, 0xFFFFFFFF, 1);
			break;
#endif
		case CMDQ_PENDING :
			cur_pc = cur_cmdq_table->cmdq_pending_pc;
			break;
		default :
			return predict_time;
	}

	cur_progress = (((s64)predict_time * ((s64)cur_cmdq_table->cmdq_total - (s64)cur_pc))/(s64)cur_cmdq_table->cmdq_total);

	return cur_progress;
}



u32 npu_get_predict_frequency(struct npu_sessionmgr *sess_mgr, struct npu_session *session, s32 tune_npu_level, int from)
{
	s64 predict_duration = 0, deadline = 0;
	s64 cur_predict_time = 0;
	s32 freq_index = LOWEST_FREQ_IDX, earliest_freq_index;
	u32 i = 0;
	bool found = false, cancel_request = false;
	struct npu_cmdq_progress *cur, *base, *cur_next, *base_next;
	s64 now = npu_get_time_us();
	s64 remain_to_next_qbuf;
	bool is_happening_soon;

	if (list_empty(&sess_mgr->cmdq_head))
		return 0;

	mutex_lock(&sess_mgr->cancel_lock);
	cancel_request = proto_check_already_been_requested();
	list_for_each_entry_safe(base, base_next, &sess_mgr->cmdq_head, list) {
		deadline = base->deadline;
		found = false;

		for (i = freq_index; i < HIGEST_FREQ_IDX; i++) {
			if (found) {
				break;
			}

			predict_duration = 0;
			list_for_each_entry_safe(cur, cur_next, &sess_mgr->cmdq_head, list) {
				cur_predict_time = npu_get_predict_time(cur->session, i) * cur->buff_cnt;
				if (!cancel_request) {
					predict_duration += npu_get_current_progress(sess_mgr, cur->session, cur_predict_time);
				}

				if (cur == base) {
					if (deadline > (predict_duration + now)) {
						freq_index = i;
						found = true;
					}
					break;
				}
			}
		}
	}
	mutex_unlock(&sess_mgr->cancel_lock);

	npu_dbg("freq_level : %d, "
			"deadline : %lld, "
			"E(workload) : %lld, "
			"now : %lld, "
			"(deadline-now) : %lld, "
			"from : %d\n",
			i,
			deadline,
			predict_duration,
			now,
			deadline - now,
			from);

	if (i == HIGEST_FREQ_IDX) {
		freq_index = i;
	}

	if (session) {
		earliest_freq_index = session->prev_npu_level;

		npu_dbg("calculate freq_index %d earliest freq_index %d\n",
				freq_index, earliest_freq_index);

		remain_to_next_qbuf = abs(session->earliest_qbuf_time - npu_get_time_us());
		is_happening_soon = remain_to_next_qbuf < sess_mgr->time_for_setting_freq;
		if ((freq_index < earliest_freq_index) && is_happening_soon)
			freq_index = earliest_freq_index;
	}

	freq_index = max(LOWEST_FREQ_IDX, freq_index);
	freq_index = min(HIGEST_FREQ_IDX, freq_index);

	if (session)
		session->prev_npu_level = freq_index;

	return freq_index;
}

void update_earliest_arrival_time(struct npu_session *session, struct npu_cmdq_progress *cmdq_ptr)
{
	s64 now, earliest_qbuf_time, arrival_time;
	struct npu_sessionmgr *sess_mgr;
	bool is_update_needed;
	bool is_wrong_prediction;

	sess_mgr = (struct npu_sessionmgr *)session->cookie;
	arrival_time = session->last_qbuf_arrival + session->arrival_interval;

	earliest_qbuf_time = session->earliest_qbuf_time;
	now = npu_get_time_us();

	npu_dbg("earliest_qbuf_time = %lld, "
			"now = %lld, "
			"arrival_time = %lld, "
			"session->last_qbuf_arrival = %lld, "
			"session->arrival_interval = %lld\n",
			earliest_qbuf_time,
			now,
			arrival_time,
			session->last_qbuf_arrival,
			session->arrival_interval);

	is_wrong_prediction = earliest_qbuf_time < now;
	is_update_needed = !is_wrong_prediction && (earliest_qbuf_time > arrival_time);

	if (is_wrong_prediction || is_update_needed) {
		cmdq_ptr->deadline = now + session->tpf;
		npu_get_predict_frequency(sess_mgr, session, 0, 1);

		session->earliest_qbuf_time = arrival_time;

		npu_dbg("session->earliest_qbuf_time = %lld, "
				"freq = %lld\n",
				session->earliest_qbuf_time,
				session->prev_npu_level);
	}
}

static inline bool check_kkt_condition(s64 a, s64 b)
{
	if(a >= MIN_ALPHA && b >= MIN_BETA && a <= MAX_ALPHA && b <= MAX_BETA) {
		return true;
	}
	return false;
}

static inline u32 get_prev_execution_time(struct npu_model_info_hash *model,
		u32 freq_index)
{
	return model->exec[freq_index].time[model->exec[freq_index].cur];
}

static inline void put_cur_execution_time(struct npu_model_info_hash *model,
		u32 freq_index, u32 time)
{
	u32 frame_index = model->exec[freq_index].cur;
	model->exec[freq_index].time[frame_index] = time;

	model->exec[freq_index].cur++;
	if(model->exec[freq_index].cur == MAX_FREQ_FRAME_NUM) {
		model->exec[freq_index].cur = 0;
	}
}

int npu_cmdq_update_alpha_beta(struct npu_session *session,
		struct npu_sessionmgr *sess_mgr, s32 real_workload,
		u8 cur_freq_index)
{
	s32 alpha, beta;
	s64 eq0, eq1;
	s64 x, y, z;

	s64 estimated_workload;
	u32 prev_y;

	struct npu_model_info_hash *model = session->model_hash_node;
	struct kkt_param_info *sum = &model->kkt_param;

	y = real_workload;
	x = model->exec[cur_freq_index].x;
	z = model->exec[cur_freq_index].z;

	estimated_workload = (model->alpha * x) + (model->beta * z);
	prev_y = get_prev_execution_time(model, cur_freq_index);

	npu_dbg("cur_freq_index %d real_workload %d estimated_workload %lld alpha %lld beta %lld error_rate %lld\n",
			cur_freq_index, real_workload, estimated_workload, model->alpha, model->beta, ((real_workload - estimated_workload)*100)/real_workload);

	// If 5 frames are not saved at the begining
	if(prev_y == 0) {
		sum->x2 += x*x;
		sum->xz += x*z;
		sum->z2 += z*z;
	}

	// Update 5 frames previous value to current
	sum->yz += (y*z - prev_y*z);
	sum->xy += (x*y - prev_y*x);

	put_cur_execution_time(model, cur_freq_index, y);

	eq0 = (sum->x2*sum->yz - sum->xy*sum->xz);
	eq1 = (sum->x2*sum->z2 - sum->xz*sum->xz);

	if (eq1 != 0) {
		beta = (eq0/eq1);

		eq0 = (-sum->yz + (beta)*sum->z2);
		eq1 = -sum->xz;

		alpha = (eq0/eq1);

		if(check_kkt_condition(alpha, beta)) {
			model->alpha = alpha;
			model->beta = beta;
			return 0;
		}
	}

	beta = MIN_BETA;

	eq0 = (-sum->xy + (beta) * sum->xz);
	eq1 = -sum->x2;

	alpha = (eq0/eq1);

	if(check_kkt_condition(alpha, beta)) {
		model->alpha = alpha;
		model->beta = beta;
		return 0;
	}

	alpha = MIN_ALPHA;

	eq0 = ((alpha)*(-sum->xz)+sum->yz);
	eq1 = sum->z2;

	beta = (eq0/eq1);

	if(check_kkt_condition(alpha, beta)) {
		model->alpha = alpha;
		model->beta = beta;
		return 0;
	}

	if(model->alpha == MAX_ALPHA && model->beta == MAX_BETA) {
		model->alpha = MIN_ALPHA;
		model->beta = MIN_BETA;
	}

	return 0;
}
/* Read all entries of the shared cmdq status table for each PROCESS
   command sent to FW. Reduce the computed MAC and SDMA load based on
   cmdq progress, and recompute the frequencies to complete the inference
   within the deadline.
   */

static void npu_read_cmdq_table(struct npu_sessionmgr *sess_mgr, struct npu_session *session, struct npu_cmdq_progress *cmdq_ptr)
{
	struct npu_device *device;
	struct npu_memory_buffer *cmdq_table_buf;
	struct cmdq_table *cmdq_table_addr, *cur_cmdq_table;
	struct npu_model_info_hash *model;

	device = container_of(sess_mgr, struct npu_device, sessionmgr);
	cmdq_table_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!cmdq_table_buf) {
		npu_err("CMDQ Table buffer not present\n");
		return;
	}
	model = session->model_hash_node;
	if (!model) {
		npu_err("model is not present\n");
		return;
	}

	cmdq_table_addr = (struct cmdq_table *)cmdq_table_buf->vaddr;
	cur_cmdq_table = &cmdq_table_addr[session->uid];

	if (cur_cmdq_table->status == CMDQ_COMPLETE) {
#if IS_ENABLED(CONFIG_SOC_S5E9945)
		if(!cmdq_ptr->freq_changed && cmdq_ptr->infer_freq_index) {
#else
		if(!cmdq_ptr->freq_changed) {
#endif
			npu_cmdq_update_alpha_beta(session, sess_mgr, cur_cmdq_table->total_time, cmdq_ptr->infer_freq_index);
			npu_dbg("Frame Complete: oid: 0x%x cmdq_total: 0x%x time_taken = %dus\n",
					session->uid,
					cur_cmdq_table->cmdq_total,
					cur_cmdq_table->total_time);
		}
		cmdq_ptr->total_time = cur_cmdq_table->total_time;
		cur_cmdq_table->status = CMDQ_FREE;
		if (!session->check_core) {
			if (10 * cur_cmdq_table->dsp_time > cmdq_ptr->total_time) {
				sess_mgr->dsp_flag++;
				session->is_control_dsp = true;
			}
			if(cur_cmdq_table->dsp_time == cmdq_ptr->total_time) {
				session->is_control_npu = false;
			} else {
				sess_mgr->npu_flag++;
			}

			session->check_core = true;
		}
	}
}

void npu_sessionmgr_init_vars(struct npu_sessionmgr *sessionmgr)
{
	u32 i;
	sessionmgr->time_for_setting_freq = npu_get_configs(CLKSETTIME);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_dnc, PM_QOS_DNC_THROUGHPUT, 0);
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_npu0, PM_QOS_NPU0_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_npu1, PM_QOS_NPU1_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_dsp, PM_QOS_DSP_THROUGHPUT, 0);
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_npu0, PM_QOS_NPU_THROUGHPUT, 0);
#endif
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);

	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_mif_for_bw, PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&sessionmgr->npu_qos_req_int_for_bw, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

	INIT_LIST_HEAD(&sessionmgr->active_session_list);
	mutex_init(&sessionmgr->active_list_lock);
	mutex_init(&sessionmgr->cmdq_lock);
	mutex_init(&sessionmgr->freq_set_lock);
	mutex_init(&sessionmgr->model_lock);
	mutex_init(&sessionmgr->cancel_lock);
	INIT_LIST_HEAD(&sessionmgr->cmdq_head);
	for (i = 0; i < NPU_MAX_SESSION; i++)
		INIT_LIST_HEAD(&sessionmgr->cmdqp[i].list);

	atomic_set(&sessionmgr->freq_index_cur, 0);

	sessionmgr->dsp_flag = 0;
	sessionmgr->npu_flag = 0;
}

static inline void npu_add_cmdq_list(struct npu_sessionmgr *sess_mgr,
		struct npu_cmdq_progress *cmdq_ptr)
{
	struct npu_cmdq_progress *cur;

	list_for_each_entry(cur, &sess_mgr->cmdq_head, list) {
		if(cur == cmdq_ptr) {
			return;
		}
	}

	list_add_tail(&cmdq_ptr->list, &sess_mgr->cmdq_head);
}

static inline void npu_delete_cmdq_list(struct npu_sessionmgr *sess_mgr,
		struct npu_cmdq_progress *cmdq_ptr)
{
	struct npu_cmdq_progress *cur, *next;

	list_for_each_entry_safe(cur, next, &sess_mgr->cmdq_head, list) {
		if (cmdq_ptr == cur) {
			cmdq_ptr->freq_changed = false;
			list_del(&cur->list);
			return;
		}
	}
}

void npu_delete_all_cmdq_node(struct npu_sessionmgr *sess_mgr) {
	struct npu_cmdq_progress *cur, *next;

	list_for_each_entry_safe(cur, next, &sess_mgr->cmdq_head, list) {
		cur->freq_changed = false;
		list_del(&cur->list);
	}
}

void npu_update_frequency_from_queue(struct npu_session *session, u32 buff_cnt)
{
	struct npu_sessionmgr *sess_mgr;
	struct npu_cmdq_progress *cmdq_ptr;
	struct npu_device *device;

	u32 freq_index;
	s64 now;
	u32 requester_id = npu_make_requester_id(session->uid, 3);

	if (npu_governor_is_unavailable()) return;

	sess_mgr = session->cookie;
	device = container_of(sess_mgr, struct npu_device, sessionmgr);

	now = npu_get_time_us();

	atomic_inc(&sess_mgr->queue_cnt);

	cmdq_ptr = &sess_mgr->cmdqp[session->uid];
	cmdq_ptr->session = session;
	cmdq_ptr->deadline = now + session->tpf;
	cmdq_ptr->buff_cnt = buff_cnt;

	mutex_lock(&sess_mgr->cmdq_lock);
	sess_mgr->cmdq_list_changed_from = requester_id;
	npu_add_cmdq_list(sess_mgr, cmdq_ptr);
	freq_index = npu_get_predict_frequency(sess_mgr, session, session->tune_npu_level, requester_id);
	mutex_unlock(&sess_mgr->cmdq_lock);

	set_new_freq(sess_mgr, freq_index, requester_id);

	session->earliest_qbuf_time = now + session->arrival_interval;
	cmdq_ptr->infer_freq_index = atomic_read(&sess_mgr->freq_index_cur);
}

static inline u32 get_cur_mif_level(void)
{
	int i = 0;
	u32 mif_value = exynos_pm_qos_request(PM_QOS_BUS_THROUGHPUT);
	for (i = 0; i < MAX_MIF_FREQ_LEVELS; i++) {
		if(mif_value == mif_freq_level[i]) {
			return i;
		}
	}

	return 0;
}

static inline void npu_governor_check_wrong_converge(struct npu_session *session,
		s64 predicted_diff, s64 predicted_time)
{
	int i, j;
	struct npu_model_info_hash *model = session->model_hash_node;
	if ((10 * abs(predicted_diff)) > (3 * predicted_time)) {
		model->miss_cnt++;
	} else {
		model->miss_cnt = 0;
	}

	if (session->model_hash_node->miss_cnt > 10) {
		model->miss_cnt = 0;
		model->alpha = MAX_ALPHA;
		model->beta = MAX_BETA;
		for(i = 0; i < MAX_FREQ_LEVELS; i++) {
			for(j = 0; j < MAX_FREQ_FRAME_NUM; j++) {
				model->exec[i].time[j] = 0;
			}
			model->exec[i].cur = 0;
		}
		model->kkt_param.x2 = model->kkt_param.yz = model->kkt_param.xy =
			model->kkt_param.xz = model->kkt_param.z2 = 0;
	}
}

static inline void npu_tune_prediction_error(struct npu_sessionmgr *sess_mgr, struct npu_session *session,
		struct npu_cmdq_progress *cmdq_ptr)
{
	int total_error = 0, i, request_mif_level = 0;
	s64 predicted_diff = 0, error_rate;
	s64 executed_time, now;
	bool tune_flag = false;

	executed_time = cmdq_ptr->total_time;
	now = npu_get_time_us();

	// Check if frequency was changed during execution model
	if (cmdq_ptr->freq_changed == false) {
		predicted_diff = executed_time - npu_get_predict_time(session, cmdq_ptr->infer_freq_index);
		npu_governor_check_wrong_converge(session, predicted_diff, executed_time);

		if ((cmdq_ptr->infer_freq_index == HIGEST_FREQ_IDX) && (now - cmdq_ptr->deadline) > 0) {
			predicted_diff = now - cmdq_ptr->deadline;
			tune_flag = true;
		} else if((10*abs(predicted_diff)) > executed_time) {
			tune_flag = true;
		}

	} else if ((now - cmdq_ptr->deadline) > 0) {
		predicted_diff = now - cmdq_ptr->deadline;
		tune_flag = true;
	}

	// Calculate error value using PID
	error_rate = (predicted_diff * 100) / executed_time;
	session->predict_error[session->predict_error_idx++] = error_rate;
	session->predict_error_idx = (MAX_FREQ_FRAME_NUM <= session->predict_error_idx) ? 0 : session->predict_error_idx;

	for(i = 0; i < MAX_FREQ_FRAME_NUM; i++){
		total_error += session->predict_error[i];
	}

	total_error = (total_error + 3 * error_rate) >> 3;

	// Set mif/int frequency if high frequency is not enough, and control frequency level
	if (tune_flag) {
		if (cmdq_ptr->infer_freq_index == HIGEST_FREQ_IDX) {
			request_mif_level = (total_error >> 4) + get_cur_mif_level();
			total_error = min(total_error, 0);
		}

		if (total_error > 10) {
			session->tune_npu_level++;
			session->tune_npu_level = min(session->tune_npu_level, 5);
		} else if (total_error < -10){
			session->tune_npu_level--;
			session->tune_npu_level = max(session->tune_npu_level, -5);
		}
	} else {
		session->tune_npu_level = 0;
	}

	session->next_mif_level = request_mif_level;

	npu_dbg("executed_time : %lld,"
			"deadline : %lld, "
			"(executed_time - predicted_time) : %lld, "
			"error_rate : %lld, "
			"npu_freq : %d, "
			"mif_freq : %d, "
			"miss_cnt : %d, "
			"total_error : %d\n",
			executed_time,
			cmdq_ptr->deadline,
			predicted_diff,
			error_rate,
			cmdq_ptr->infer_freq_index,
			request_mif_level,
			session->model_hash_node->miss_cnt,
			total_error);
}

void npu_revert_cmdq_list(struct npu_session *session)
{
	struct npu_sessionmgr *sess_mgr;
	struct npu_cmdq_progress *cmdq_ptr;

	if (npu_governor_is_unavailable()) return;

	sess_mgr = session->cookie;
	cmdq_ptr = &sess_mgr->cmdqp[session->uid];

	atomic_dec(&sess_mgr->queue_cnt);

	mutex_lock(&sess_mgr->cmdq_lock);
	npu_delete_cmdq_list(sess_mgr, cmdq_ptr);
	mutex_unlock(&sess_mgr->cmdq_lock);
}

u32 npu_update_cmdq_progress_from_done(struct npu_queue *queue)
{
	struct npu_session *session;
	struct npu_sessionmgr *sess_mgr;
	struct npu_cmdq_progress *cmdq_ptr;
	struct npu_vertex_ctx *vctx;
	u32 freq_index_max;
	u32 requester_id;

	if (npu_governor_is_unavailable()) return 0;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	requester_id = npu_make_requester_id(session->uid, 4);
	sess_mgr = session->cookie;

	cmdq_ptr = &sess_mgr->cmdqp[session->uid];

	mutex_lock(&sess_mgr->cmdq_lock);

	sess_mgr->cmdq_list_changed_from = requester_id;
	npu_read_cmdq_table(sess_mgr, session, cmdq_ptr);
	npu_tune_prediction_error(sess_mgr, session, cmdq_ptr);

	update_earliest_arrival_time(session, cmdq_ptr);
	npu_delete_cmdq_list(sess_mgr, cmdq_ptr);
	freq_index_max = npu_get_predict_frequency(sess_mgr, session, 0, requester_id);

	mutex_unlock(&sess_mgr->cmdq_lock);

	atomic_dec(&sess_mgr->queue_cnt);

	return freq_index_max;
}

void npu_update_frequency_from_done(struct npu_queue *queue, u32 freq_index)
{
	struct npu_session *session;
	struct npu_sessionmgr *sess_mgr;
	struct npu_vertex_ctx *vctx;
	u32 requester_id;
	struct npu_device *device;

	if (npu_governor_is_unavailable()) return;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);
	requester_id = npu_make_requester_id(session->uid, 4);

	sess_mgr = session->cookie;
	device = container_of(sess_mgr, struct npu_device, sessionmgr);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if (!get_next_set_freq(sess_mgr, NULL) && freq_index) {
#else
	if (!get_next_set_freq(sess_mgr, NULL)) {
#endif
		set_new_freq(sess_mgr, freq_index, requester_id);
	}
}

/* Read table for one last time, and dont schedule anymore
   since all sessions are getting closed.
   */
void npu_cmdq_table_read_close(struct cmdq_table_info *cmdq_info)
{
	struct npu_device *device;
	struct npu_sessionmgr *sess_mgr;

	device = container_of(cmdq_info, struct npu_device, cmdq_table_info);
	sess_mgr = &device->sessionmgr;

	mutex_lock(&sess_mgr->cmdq_lock);
	npu_delete_all_cmdq_node(sess_mgr);
	mutex_unlock(&sess_mgr->cmdq_lock);

	mutex_lock(&cmdq_info->lock);
	if (!IS_ERR_OR_NULL(cmdq_info->kthread)) {
		kthread_stop(cmdq_info->kthread);
	}
	cmdq_info->kthread = NULL;
	mutex_unlock(&cmdq_info->lock);
}

void init_workload_params(struct npu_session *session)
{
	// calculate per model at once
	struct npu_model_info_hash *model = session->model_hash_node;
	s32 i, shift = 0, max_freq = freq_level[HIGEST_FREQ_IDX].npu;

	npu_dbg("origin mac : %d, origin dma : %d\n", model->computational_workload, model->io_workload);

	memset(model->exec, 0, sizeof(struct npu_execution_info)*MAX_FREQ_LEVELS);
	memset(&model->kkt_param, 0, sizeof(struct kkt_param_info));

	while(model->computational_workload < max_freq || model->io_workload < max_freq){
		shift++;
		max_freq = (freq_level[HIGEST_FREQ_IDX].npu >> shift);
	}

	for(i = LOWEST_FREQ_IDX; i <= HIGEST_FREQ_IDX; i++) {
		model->exec[i].x = model->computational_workload / (freq_level[i].npu >> shift);
		model->exec[i].z = model->io_workload / (freq_level[i].dnc >> shift);
		npu_dbg("x %lld z %lld divide %d compute_workload %d io_workload %d\n",
				model->exec[i].x, model->exec[i].z, shift, model->computational_workload, model->io_workload);
	}
}

int npu_cmdq_table_read_probe(struct cmdq_table_info *cmdq_info)
{
	struct npu_device *device;
	int ret = 0;
	struct npu_memory_buffer *cmdq_table_buf;
	struct dsp_dhcp *dhcp;
	struct cmdq_table *cmdq_table_addr;
	s32 i;

	mutex_init(&cmdq_info->lock);

	device = container_of(cmdq_info, struct npu_device, cmdq_table_info);
	dhcp = device->system.dhcp;

	cmdq_table_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!cmdq_table_buf) {
		probe_err("\n CMDQ Table buffer not present\n");
		ret = -ENOMEM;
		return ret;
	}

	cmdq_table_addr = (struct cmdq_table *)cmdq_table_buf->vaddr;
	for (i = 0; i < NPU_MAX_SESSION; i++)
		cmdq_table_addr[i].status = CMDQ_FREE;

	ret = npu_governor_parsing_dt(device);
	if (ret) {
		probe_err("npu_governor parsing dt failed\n");
		return ret;
	}

	dsp_dhcp_write_reg_idx(dhcp, DSP_NPU_CMDQ_TABLE_DADDR, cmdq_table_buf->daddr);

	cmdq_info->kthread = NULL;
	cmdq_info->thr_data.ptr = cmdq_info;

	return ret;
}
