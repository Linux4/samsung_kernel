/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/mutex.h>

#include <media/v4l2-subdev.h>
#include <soc/samsung/exynos-bcm_dbg.h>

#include "votf/camerapp-votf.h"
#include "is-config.h"
#include "is-hw-pdp.h"
#include "is-device-sensor-peri.h"
#include "api/is-hw-api-pdp-v1.h"
#include "is-votfmgr.h"
#include "is-debug.h"
#include "is-resourcemgr.h"

static DEFINE_MUTEX(cmn_reg_lock);

static void pdp_s_rdma_addr(void *data, unsigned long id, struct is_frame *frame)
{
	struct is_pdp *pdp;

	pdp = (struct is_pdp *)data;
	if (!pdp) {
		err("failed to get PDP");
		return;
	}

	pdp_hw_s_rdma_addr(pdp->base, frame->dvaddr_buffer, frame->num_buffers);
}

static void pdp_s_af_rdma_addr(void *data, unsigned long id, struct is_frame *frame)
{
	struct is_pdp *pdp;

	pdp = (struct is_pdp *)data;
	if (!pdp) {
		err("failed to get PDP");
		return;
	}

	pdp_hw_s_af_rdma_addr(pdp->base, frame->dvaddr_buffer, frame->num_buffers);
}

#if defined(VOTF_ONESHOT)
static void pdp_s_one_shot_enable(void *data, unsigned long id)
{
	int ret = 0;
	struct is_pdp *pdp;

	pdp = (struct is_pdp *)data;
	if (!pdp) {
		err("failed to get PDP");
		return;
	}

	if (test_and_clear_bit(IS_PDP_VOTF_ONESHOT_FIRST_FRAME, &pdp->state)) {
		pdp_hw_s_path(pdp->base, DMA);
		pdp_hw_s_corex_enable(pdp->base, true);
		pdp->err_cnt_oneshot = 0;

		if (pdp_hw_g_idle_state(pdp->base))
			ret = pdp_hw_s_one_shot_enable(pdp);
	} else {
		ret = pdp_hw_s_one_shot_enable(pdp);
	}

	if (ret) {
		if (pdp->err_cnt_oneshot == 0) {
			pdp_hw_dump(pdp->base);
			votf_sfr_dump();
		}

		if (pdp->err_cnt_oneshot == PDP_STUCK_CNT) {
			err("PDP%d stuck", pdp->id);
			is_kernel_log_dump(false);
		}

		pdp->err_cnt_oneshot++;
	} else {
		pdp->err_cnt_oneshot = 0;
	}
}
#endif

static int pdp_print_work_list(struct is_work_list *work_list)
{
	err("");
	print_fre_work_list(work_list);
	print_req_work_list(work_list);

	return 0;
}

static int get_free_set_req_work(struct is_work_list *work_list, unsigned int fcount)
{
	struct is_work *work;
	int ret;

	ret = get_free_work(work_list, &work);
	if (ret || !work) {
		err("failed to get FREE work from work_list(%d)", work_list->id);
		return ret;
	}

	work->fcount = fcount;
	set_req_work(work_list, work);

	if (IS_ENABLED(PDP_TRACE_WORK))
		pdp_print_work_list(work_list);

	return ret;
}

static inline void wq_func_schedule(struct is_pdp *pdp, struct work_struct *work_wq)
{
	if (pdp->wq_stat)
		queue_work(pdp->wq_stat, work_wq);
	else
		schedule_work(work_wq);
}

#if defined(USE_SKIP_DUMP_LIC_OVERFLOW)
static int crc_flag;
static int get_crc_flag(u32 instance, struct is_hw_ip *hw_ip)
{
	int ret = 0;
	struct is_group *group;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;

	FIMC_BUG(!hw_ip);

	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	sensor = device->sensor;
	if (!sensor) {
		mserr_hw("failed to get sensor", instance, hw_ip);
		return -ENODEV;
	}

	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	if (!csi) {
		mserr_hw("csi is null\n", instance, hw_ip);
		return -ENODEV;
	}

	if (csi->crc_flag)
		crc_flag |= csi->crc_flag << instance;
	else
		crc_flag &= ~(csi->crc_flag << instance);

	csi->crc_flag = false;

	return ret;
}
#endif

static int is_hw_pdp_set_pdstat_reg(struct is_pdp *pdp)
{
	unsigned long flag;
	struct pdp_stat_reg *regs;
	u32 regs_size;
	int i;
	int ret = 0;

	FIMC_BUG(!pdp);

	spin_lock_irqsave(&pdp->slock_paf_s_param, flag);

	if (pdp->stat_enable && test_bit(IS_PDP_SET_PARAM_COPY, &pdp->state)) {
		pdp_hw_s_corex_type(pdp->base, COREX_IGNORE);

		regs_size = pdp->regs_size;
		regs = pdp->regs;
		for (i = 0; i < regs_size; i++)
			writel(regs[i].reg_data, pdp->base + regs[i].reg_addr + COREX_OFFSET);

		clear_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);

		pdp_hw_s_wdma_init(pdp->base, pdp->id);
		pdp_hw_s_line_row(pdp->base, pdp->stat_enable, pdp->vc_ext_sensor_mode, pdp->binning);

		pdp_hw_s_corex_type(pdp->base, COREX_COPY);

		dbg_pdp(1, " load ofs done", pdp);
	}

	spin_unlock_irqrestore(&pdp->slock_paf_s_param, flag);

	return ret;
}

static irqreturn_t is_isr_pdp_int1(int irq, void *data)
{
	struct is_hw_ip *hw_ip;
	struct is_pdp *pdp;
	unsigned int state;
	u32 instance;
	unsigned long err_state;
	int err;
	u32 work_id;
	struct is_framemgr *stat_framemgr;
	struct is_frame *stat_frame;
	struct is_work_list *work_list;
	struct paf_rdma_param *param;
	u32 en_votf;

	hw_ip = (struct is_hw_ip *)data;
	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		err("failed to get PDP");
		return IRQ_HANDLED;
	}

	instance = atomic_read(&hw_ip->instance);

	state = pdp_hw_g_int1_state(pdp->base, true, &pdp->irq_state[PDP_INT1])
		& pdp_hw_g_int1_mask(pdp->base);
	dbg_pdp(1, "INT1: 0x%x\n", pdp, state);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("INT1 invalid interrupt: 0x%x", instance, hw_ip, state);
		return IRQ_NONE;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag)) {
		mserr_hw("INT1 During recovery : invalid interrupt: 0x%x", instance, hw_ip, state);
		return IRQ_NONE;
	}

	if (pdp_hw_is_occured(state, PE_START) && pdp_hw_is_occured(state, PE_END))
		mswarn_hw(" end/start both occur(0x%x)", instance, hw_ip, state);

	param = &hw_ip->region[instance]->parameter.paf;
	en_votf = param->dma_input.v_otf_enable;

	if (pdp_hw_is_occured(state, PE_START)) {
		is_hw_pdp_set_pdstat_reg(pdp); /* PDP core setting */

		_is_hw_frame_dbg_trace(hw_ip, atomic_read(&hw_ip->fcount), DEBUG_POINT_FRAME_START);

		atomic_add(1, &hw_ip->count.fs);
		if (hw_ip->is_leader)
			is_hardware_frame_start(hw_ip, instance);

		if (pdp->stat_enable) {
			stat_framemgr = pdp->stat_framemgr;
			FIMC_BUG(!stat_framemgr);

			framemgr_e_barrier(stat_framemgr, FMGR_IDX_30);

			stat_frame = get_frame(stat_framemgr, FS_REQUEST);
			if (stat_frame) {
				put_frame(stat_framemgr, stat_frame, FS_FREE);

				framemgr_x_barrier(stat_framemgr, FMGR_IDX_30);

				dbg_pdp(2, "index %d, dva: %x, kva: %lx\n",
					pdp, stat_frame->index,
					(u32)stat_frame->dvaddr_buffer[0],
					stat_frame->kvaddr_buffer[0]);


				for (work_id = WORK_PDP_STAT0; work_id < WORK_PDP_MAX; work_id++) {
					work_list = &pdp->work_list[work_id];
					get_free_set_req_work(work_list, stat_frame->fcount);
				}
			} else {
				dbg_pdp(0, " PE_PAF_STAT0 req frame is NULL\n", pdp);
				frame_manager_print_queues(stat_framemgr);

				framemgr_x_barrier(stat_framemgr, FMGR_IDX_30);
			}
		}

#if defined(VOTF_GLOBAL_ENABLE)
		if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
			struct is_group *group = hw_ip->group[instance];
			/*
			 * Call interrupt handler function of rear IP connected by OTF.
			 * This is only used with col_row interrupt when v-blank is "0".
			 * TODO: next IP don't have to use this relay.
			 */
			is_hw_interrupt_relay(group, hw_ip);
		}
#endif
	}

	if (pdp_hw_is_occured(state, PE_END)) {
		_is_hw_frame_dbg_trace(hw_ip, atomic_read(&hw_ip->fcount), DEBUG_POINT_FRAME_END);

		atomic_add(1, &hw_ip->count.fe);

		if (hw_ip->is_leader)
			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
						IS_SHOT_SUCCESS, false);

		if (pdp->stat_enable)
			wq_func_schedule(pdp, &pdp->work_stat[WORK_PDP_STAT1]);
#if defined(USE_SKIP_DUMP_LIC_OVERFLOW)
		/* clear crc flag */
		get_crc_flag(instance, hw_ip);
#endif
		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		wake_up(&hw_ip->status.wait_queue);

		spin_lock(&pdp->slock_oneshot);
		if (test_and_clear_bit(IS_PDP_ONESHOT_PENDING, &pdp->state)) {
			spin_unlock(&pdp->slock_oneshot);

			pdp_hw_s_one_shot_enable(pdp);
			msinfo_hw("[F:%d] clear oneshot pending", instance, hw_ip,
				atomic_read(&hw_ip->count.fe));
		} else {
			spin_unlock(&pdp->slock_oneshot);
		}

	}

	if (pdp_hw_is_occured(state, PE_PAF_STAT0)) {
		if (pdp->stat_enable)
			wq_func_schedule(pdp, &pdp->work_stat[WORK_PDP_STAT0]);
	}

	err_state = (unsigned long)pdp_hw_is_occured(state, PE_ERR_INT1);
	if (err_state) {
		unsigned long long time;
		ulong usec;

		pdp->time_err = local_clock();
		time = pdp->time_err - pdp->time_rta_cfg;
		usec = do_div(time, NSEC_PER_SEC) / NSEC_PER_USEC;

		err = find_first_bit(&err_state, SZ_32);
		while (err < SZ_32) {
			if (usec < 100000)
				mserr_hw(" err INT1(%d):RTA wrong config", instance, hw_ip, err);
			else
				mserr_hw(" err INT1(%d):%s", instance, hw_ip, err, pdp->int1_str[err]);

			err = find_next_bit(&err_state, SZ_32, err + 1);
		}
		is_hardware_sfr_dump(hw_ip->hardware, hw_ip->id, false);

		if (pdp_hw_is_occured(state, PE_PAF_OVERFLOW)) {
#if IS_ENABLED(CONFIG_EXYNOS_BCM_DBG_AUTO)
			//exynos_bcm_dbg_stop(CAMERA_DRIVER);
#endif
			print_all_hw_frame_count(hw_ip->hardware);
			is_hardware_sfr_dump(hw_ip->hardware, DEV_HW_END, false);
#if defined(USE_SKIP_DUMP_LIC_OVERFLOW)
			if (!get_crc_flag(instance, hw_ip) && crc_flag) {
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
				warn("skip to s2d dump");
				pdp_hw_s_reset(pdp->cmn_base);
			} else {
				is_debug_s2d(true, "LIC overflow");
			}
#else
			is_debug_s2d(true, "LIC overflow");
#endif
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t is_isr_pdp_int2(int irq, void *data)
{
	struct is_hw_ip *hw_ip;
	struct is_pdp *pdp;
	unsigned int state;
	u32 instance;
	unsigned long err_state, err_rdma;
	int err;

	hw_ip = (struct is_hw_ip *)data;
	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		err("failed to get PDP");
		return IRQ_HANDLED;
	}

	instance = atomic_read(&hw_ip->instance);

	state = pdp_hw_g_int2_state(pdp->base, true, &pdp->irq_state[PDP_INT2])
		& pdp_hw_g_int2_mask(pdp->base);
	dbg_pdp(1, "INT2: 0x%x\n", pdp, state);

	if (pdp_hw_is_occured(state, PE_PAF_STAT0)) {
		if (pdp->stat_enable)
			wq_func_schedule(pdp, &pdp->work_stat[WORK_PDP_STAT0]);
	}

	err_state = (unsigned long)pdp_hw_is_occured(state, PE_ERR_INT2);
	if (err_state) {
		err = find_first_bit(&err_state, SZ_32);
		while (err < SZ_32) {
			mserr_hw(" err INT2(%d):%s", instance, hw_ip, err, pdp->int2_str[err]);
			err = find_next_bit(&err_state, SZ_32, err + 1);
		}

		if (pdp_hw_is_occured(state, PE_ERR_RDMA_IRQ)) {
			err_rdma = pdp_hw_g_int2_rdma_state(pdp->base, true);
			err = find_first_bit(&err_rdma, SZ_32);
			while (err < SZ_32) {
				mserr_hw(" err RDMA(%d):%s", instance, hw_ip,
					err, pdp->int2_rdma_str[err]);
				err = find_next_bit(&err_rdma, SZ_32, err + 1);
			}
		}

		is_hardware_sfr_dump(hw_ip->hardware, hw_ip->id, false);

		if (pdp_hw_is_occured(state, PE_PAF_OVERFLOW)) {
#if IS_ENABLED(CONFIG_EXYNOS_BCM_DBG_AUTO)
			//exynos_bcm_dbg_stop(CAMERA_DRIVER);
#endif
			print_all_hw_frame_count(hw_ip->hardware);
			is_hardware_sfr_dump(hw_ip->hardware, DEV_HW_END, false);
#if defined(USE_SKIP_DUMP_LIC_OVERFLOW)
			if (!get_crc_flag(instance, hw_ip) && crc_flag) {
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
				warn("skip to s2d dump");
				pdp_hw_s_reset(pdp->cmn_base);
			} else {
				is_debug_s2d(true, "LIC overflow");
			}
#else
			is_debug_s2d(true, "LIC overflow");
#endif
		}
	}

	return IRQ_HANDLED;
}

static void __nocfi pdp_worker_stat0(struct work_struct *data)
{
	struct is_pdp *pdp;
	struct paf_action *pa, *temp;
	unsigned long flag;
	unsigned int fcount;
	struct is_work_list *work_list;
	struct is_work *work;

	FIMC_BUG_VOID(!data);

	pdp = container_of(data, struct is_pdp, work_stat[WORK_PDP_STAT0]);

	work_list = &pdp->work_list[WORK_PDP_STAT0];
	get_req_work(work_list, &work);
	while (work) {
		fcount = work->fcount;

		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp, &pdp->list_of_paf_action, list) {
			switch (pa->type) {
			case VC_STAT_TYPE_PDP_1_0_PDAF_STAT0:
			case VC_STAT_TYPE_PDP_1_1_PDAF_STAT0:
			case VC_STAT_TYPE_PDP_3_0_PDAF_STAT0:
			case VC_STAT_TYPE_PDP_3_1_PDAF_STAT0:
#ifdef ENABLE_FPSIMD_FOR_USER
				is_fpsimd_get_func();
				pa->notifier(pa->type, fcount, pa->data);
				is_fpsimd_put_func();
#else
				pa->notifier(pa->type, fcount, pa->data);
#endif
				break;
			default:
				break;
			}
		}
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);

		dbg_pdp(3, "%s, fcount: %d\n", pdp, __func__, fcount);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);

		if (IS_ENABLED(PDP_TRACE_WORK))
			pdp_print_work_list(work_list);
	}
}

static void __nocfi pdp_worker_stat1(struct work_struct *data)
{
	struct is_pdp *pdp;
	struct paf_action *pa, *temp;
	unsigned long flag;
	unsigned int fcount;
	struct is_work_list *work_list;
	struct is_work *work;

	FIMC_BUG_VOID(!data);

	pdp = container_of(data, struct is_pdp, work_stat[WORK_PDP_STAT1]);

	work_list = &pdp->work_list[WORK_PDP_STAT1];
	get_req_work(work_list, &work);
	while (work) {
		fcount = work->fcount;

		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp, &pdp->list_of_paf_action, list) {
			switch (pa->type) {
			case VC_STAT_TYPE_PDP_1_0_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_1_1_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_3_0_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_3_1_PDAF_STAT1:
#ifdef ENABLE_FPSIMD_FOR_USER
				is_fpsimd_get_func();
				pa->notifier(pa->type, fcount, pa->data);
				is_fpsimd_put_func();
#else
				pa->notifier(pa->type, fcount, pa->data);
#endif
				break;
			default:
				break;
			}
		}
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);

		dbg_pdp(3, "%s, fcount: %d\n", pdp, __func__, fcount);

		set_free_work(work_list, work);
		get_req_work(work_list, &work);

		if (IS_ENABLED(PDP_TRACE_WORK))
			pdp_print_work_list(work_list);
	}
}

int pdp_set_param(struct v4l2_subdev *subdev, struct paf_setting_t *regs, u32 regs_size)
{
	int i;
	struct is_pdp *pdp;
	unsigned long flag;
	u32 corex_enable;

	pdp = (struct is_pdp *)v4l2_get_subdevdata(subdev);
	if (!pdp) {
		err("failed to get PDP");
		return -ENODEV;
	}

	/* EVT0 only, when use PDP CH2, always off pdstat_path_on to not map pdp core
	 * It will removed at EVT1
	 */
	if (IS_ENABLED(CONFIG_PDP_V3_1) && pdp->id == 2) {
		info("skip setting PDP2 set_param in EVT0");
		return 0;
	}

	/* CAUTION: PD path must be on before algorithm block setting. */
	pdp_hw_s_pdstat_path(pdp->base, true);

	pdp_hw_g_corex_state(pdp->base, &corex_enable);
	dbg_pdp(1, "state[0x%X], corex[%d]", pdp, pdp->state, corex_enable);

	if (test_bit(IS_PDP_SET_PARAM, &pdp->state) && corex_enable) {
		dbg_pdp(0, "PDP(%d) store RTA set, size(%d)\n", pdp, pdp->base, regs_size);

		for (i = 0; i < regs_size; i++)
			dbg_pdp(1, "[%d] store ofs: 0x%x, val: 0x%x\n", pdp,
					i, regs[i].reg_addr, regs[i].reg_data);

		spin_lock_irqsave(&pdp->slock_paf_s_param, flag);

		pdp->regs_size = regs_size;
		memcpy((void *)pdp->regs, (void *)regs, sizeof(struct pdp_stat_reg) * regs_size);
		set_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);

		spin_unlock_irqrestore(&pdp->slock_paf_s_param, flag);
	} else {
		dbg_pdp(0, "PDP(%d) RTA setting, size(%d)\n", pdp, pdp->base, regs_size);

		for (i = 0; i < regs_size; i++) {
			dbg_pdp(1, "[%d] ofs: 0x%x, val: 0x%x\n", pdp,
					i, regs[i].reg_addr, regs[i].reg_data);
			writel(regs[i].reg_data, pdp->base + regs[i].reg_addr + COREX_OFFSET);
		}

		/* CAUTION: WDMA size must be set after PDSTAT_ROI block setting. */
		pdp_hw_s_wdma_init(pdp->base, pdp->id);

		if (pdp->stat_enable)
			pdp_hw_s_line_row(pdp->base, pdp->stat_enable, pdp->vc_ext_sensor_mode, pdp->binning);

		set_bit(IS_PDP_SET_PARAM, &pdp->state);
	}

	pdp->time_rta_cfg = local_clock();

	return 0;
}

int pdp_get_ready(struct v4l2_subdev *subdev, u32 *ready)
{
	struct is_pdp *pdp;

	pdp = (struct is_pdp *)v4l2_get_subdevdata(subdev);
	if (!pdp) {
		err("failed to get PDP");
		return -ENODEV;
	}

	if (test_bit(IS_PDP_SET_PARAM_COPY, &pdp->state))
		*ready = 0;
	else
		*ready = 1;

	return 0;
}

int pdp_register_notifier(struct v4l2_subdev *subdev, enum itf_vc_stat_type type,
		vc_dma_notifier_t notifier, void *data)
{
	struct is_pdp *pdp;
	struct paf_action *pa;
	unsigned long flag;

	pdp = (struct is_pdp *)v4l2_get_subdevdata(subdev);
	if (!pdp) {
		err("failed to get PDP");
		return -ENODEV;
	}

	switch (type) {
	case VC_STAT_TYPE_PDP_1_0_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_1_1_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_1_0_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_1_1_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_3_0_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_3_0_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_3_1_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_3_1_PDAF_STAT1:
		pa = kzalloc(sizeof(struct paf_action), GFP_ATOMIC);
		if (!pa) {
			err_lib("failed to allocate a PAF action");
			return -ENOMEM;
		}

		pa->type = type;
		pa->notifier = notifier;
		pa->data = data;

		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_add(&pa->list, &pdp->list_of_paf_action);
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);

		break;
	default:
		return -EINVAL;
	}

	dbg_pdp(2, "%s, type: %d, notifier: %p\n", pdp, __func__, type, notifier);

	return 0;
}

int pdp_unregister_notifier(struct v4l2_subdev *subdev, enum itf_vc_stat_type type,
		vc_dma_notifier_t notifier)
{
	struct is_pdp *pdp;
	struct paf_action *pa, *temp;
	unsigned long flag;

	pdp = (struct is_pdp *)v4l2_get_subdevdata(subdev);
	if (!pdp) {
		err("failed to get PDP");
		return -ENODEV;
	}

	switch (type) {
	case VC_STAT_TYPE_PDP_1_0_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_1_1_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_1_0_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_1_1_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_3_0_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_3_0_PDAF_STAT1:
	case VC_STAT_TYPE_PDP_3_1_PDAF_STAT0:
	case VC_STAT_TYPE_PDP_3_1_PDAF_STAT1:
		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp,
				&pdp->list_of_paf_action, list) {
			if ((pa->notifier == notifier)
					&& (pa->type == type)) {
				list_del(&pa->list);
				kfree(pa);
			}
		}
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);

		break;
	default:
		return -EINVAL;
	}

	dbg_pdp(2, "%s, type: %d, notifier: %p\n", pdp, __func__, type, notifier);

	return 0;
}

void __nocfi pdp_notify(struct v4l2_subdev *subdev, unsigned int type, void *data)
{
	struct is_pdp *pdp;
	struct paf_action *pa, *temp;
	unsigned long flag;

	pdp = (struct is_pdp *)v4l2_get_subdevdata(subdev);
	if (!pdp) {
		err("failed to get PDP");
		return;
	}

	switch (type) {
	case CSIS_NOTIFY_DMA_END_VC_MIPISTAT:
		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp, &pdp->list_of_paf_action, list) {
			switch (pa->type) {
			case VC_STAT_TYPE_PDP_1_0_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_1_1_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_3_0_PDAF_STAT0:
			case VC_STAT_TYPE_PDP_3_0_PDAF_STAT1:
			case VC_STAT_TYPE_PDP_3_1_PDAF_STAT0:
			case VC_STAT_TYPE_PDP_3_1_PDAF_STAT1:
#ifdef ENABLE_FPSIMD_FOR_USER
				is_fpsimd_get_func();
				pa->notifier(pa->type, *(unsigned int *)data, pa->data);
				is_fpsimd_put_func();
#else
				pa->notifier(pa->type, *(unsigned int *)data, pa->data);
#endif
				break;
			default:
				break;
			}
		}
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);

	default:
		break;
	}

	dbg_pdp(2, "%s, sensor fcount: %d\n", pdp, __func__, *(unsigned int *)data);
}

static DEFINE_SPINLOCK(cmn_reg_slock);
static int is_hw_pdp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame)
{
	int ret = 0;
	int pd_mode;
	bool enable;
	unsigned int sensor_type;
	struct is_pdp *pdp;
	struct is_group *group;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	struct is_device_csi *csi;
	struct paf_rdma_param *param;
	u32 csi_ch, en_val, i;
	struct is_crop img_full_size = {0, 0, 0, 0};
	struct is_crop img_crop_size = {0, 0, 0, 0};
	struct is_crop img_comp_size = {0, 0, 0, 0};
	u32 img_hwformat, img_pixelsize;
	u32 pd_width, pd_height, pd_hwformat;
	u32 path;
	u32 en_votf;
	u32 en_sdc;
	struct is_module_enum *module;
	u32 fps;
	ulong flags = 0;
	u32 extformat;
	u32 position;

	FIMC_BUG(!hw_ip);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	sensor = device->sensor;
	if (!sensor) {
		mserr_hw("failed to get sensor", instance, hw_ip);
		return -ENODEV;
	}

	sensor_cfg = sensor->cfg;
	if (!sensor_cfg) {
		mserr_hw("failed to get sensor_cfg", instance, hw_ip);
		return -EINVAL;
	}
	position = sensor->position;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state) ||
		!test_bit(IS_PDP_SET_PARAM, &pdp->state)) {
		pd_mode = PD_NONE; /* Reprocessing does not support PAF stat. */
		pdp->vc_ext_sensor_mode = 0;
	} else {
		module = (struct is_module_enum *)v4l2_get_subdevdata(sensor->subdev_module);
		if (!module) {
			mserr_hw("failed to get module", instance, hw_ip);
			return -EINVAL;
		}

		pd_mode = sensor_cfg->pd_mode;
		pdp->vc_ext_sensor_mode =
			module->vc_extra_info[VC_BUF_DATA_TYPE_GENERAL_STAT1].sensor_mode;
		pdp->binning = sensor_cfg->binning;
	}

	enable = pdp_hw_to_sensor_type(pd_mode, &sensor_type);
	pdp->stat_enable = enable;

	param = &hw_ip->region[instance]->parameter.paf;
	en_votf = param->dma_input.v_otf_enable;
	if (param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE)
		path = DMA;
	else
		path = OTF;

	/* WDMA */
	if (enable) {
		struct is_subdev *subdev = &device->pdst;
		struct is_framemgr *stat_framemgr = GET_SUBDEV_FRAMEMGR(subdev);
		struct is_frame *stat_frame;
		unsigned long flags;

		FIMC_BUG(!stat_framemgr);

		pdp->stat_framemgr = stat_framemgr;
		framemgr_e_barrier_irqs(stat_framemgr, FMGR_IDX_30, flags);

		stat_frame = get_frame(stat_framemgr, FS_FREE);
		if (stat_frame) {
			stat_frame->fcount = frame->fcount;
			put_frame(stat_framemgr, stat_frame, FS_REQUEST);
			framemgr_x_barrier_irqr(stat_framemgr, FMGR_IDX_30, flags);

			pdp_hw_s_wdma_enable(pdp->base, (u32)stat_frame->dvaddr_buffer[0]);

			dbg_pdp(2, "index %d, hw_ip fcount: %d, dva: %x, kva: %lx\n",
				pdp, stat_frame->index, atomic_read(&hw_ip->fcount),
				(u32)stat_frame->dvaddr_buffer[0],
				stat_frame->kvaddr_buffer[0]);

		} else {
			mswarn_hw(" PDSTAT free frame is NULL", instance, hw_ip);
			frame_manager_print_queues(stat_framemgr);
			framemgr_x_barrier_irqr(stat_framemgr, FMGR_IDX_30, flags);
		}
	} else {
		pdp_hw_s_wdma_disable(pdp->base);
	}

	if (path == OTF || en_votf) {
		/* A shot after stream on is prevented. */
		if (atomic_read(&group->head->scount))
			return 0;

		hw_ip->is_leader = false;
	} else {
		set_bit(hw_ip->id, &frame->core_flag);

		if (pdp->prev_instance == instance)
			return 0;

		hw_ip->is_leader = true;
	}

	pdp->prev_instance = instance;

	extformat = sensor_cfg->input[CSI_VIRTUAL_CH_0].extformat;
	if (extformat == HW_FORMAT_RAW10_SDC)
		pdp->en_sdc = en_sdc = 1;
	else
		pdp->en_sdc = en_sdc = 0;

	/* VOTF RDMA settings */
	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		struct is_frame *votf_frame;
		struct is_framemgr *votf_fmgr;

		votf_frame = is_votf_get_frame(group, TRS, ENTRY_PAF, 0);
		if (votf_frame) {
			msinfo_hw(" VOTF Bayer 1st shot(index: %d, dva: 0x%pad)\n", instance, hw_ip,
				votf_frame->index, &votf_frame->dvaddr_buffer[0]);

			votf_fmgr = is_votf_get_framemgr(group, TRS, ENTRY_PAF);
			ret = votf_fmgr_call(votf_fmgr, master, s_addr, votf_frame);
			if (ret)
				mswarn_hw("votf_fmgr_call(master) is fail(%d)",
					instance, hw_ip, ret);

			ret = votf_fmgr_call(votf_fmgr, slave, s_addr, votf_frame);
			if (ret)
				mswarn_hw("votf_fmgr_call(slave) is fail(%d)",
					instance, hw_ip, ret);
		}

		votf_frame = is_votf_get_frame(group, TRS, ENTRY_PDAF, 0);
		if (votf_frame) {
			msinfo_hw(" VOTF AF 1st shot(index: %d, dva: 0x%pad)\n", instance, hw_ip,
				votf_frame->index, &votf_frame->dvaddr_buffer[0]);

			votf_fmgr = is_votf_get_framemgr(group, TRS, ENTRY_PDAF);
			ret = votf_fmgr_call(votf_fmgr, master, s_addr, votf_frame);
			if (ret)
				mswarn_hw("votf_fmgr_call(master) is fail(%d)",
					instance, hw_ip, ret);

			ret = votf_fmgr_call(votf_fmgr, slave, s_addr, votf_frame);
			if (ret)
				mswarn_hw("votf_fmgr_call(slave) is fail(%d)",
					instance, hw_ip, ret);
		}
	}

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		fps = 0; /* Reprocessing is not real-time procesing. So, a fps is not needed. */
	else
		fps = sensor_cfg->max_fps;

	if (path == OTF || en_votf) {
		img_crop_size.w = sensor_cfg->width;
		img_crop_size.h = sensor_cfg->height;
		img_full_size.w = sensor_cfg->input[CSI_VIRTUAL_CH_0].width;
		img_full_size.h = sensor_cfg->input[CSI_VIRTUAL_CH_0].height;

		if (sensor_cfg->input[CSI_VIRTUAL_CH_0].hwformat == HW_FORMAT_RAW14)
			img_pixelsize = OTF_INPUT_BIT_WIDTH_14BIT;
		else
			img_pixelsize = OTF_INPUT_BIT_WIDTH_10BIT; /* default */

		/* used for votf enabled, check whether un-packed is used */
		img_hwformat = DMA_INPUT_FORMAT_BAYER_PACKED;
	} else {
		img_full_size.w = param->dma_input.width;
		img_full_size.h = param->dma_input.height;
		img_crop_size.w = param->dma_input.dma_crop_width;
		img_crop_size.h = param->dma_input.dma_crop_height;

		img_hwformat = param->dma_input.format;
		img_pixelsize = param->dma_input.msb + 1;
	}

	if (en_sdc) {
		img_comp_size.w = img_full_size.w;
		img_comp_size.h = img_full_size.h;
	} else {
		img_comp_size.w = img_crop_size.w;
		img_comp_size.h = img_crop_size.h;
	}

	pd_width = sensor_cfg->input[CSI_VIRTUAL_CH_1].width;
	pd_height = sensor_cfg->input[CSI_VIRTUAL_CH_1].height;
	pd_hwformat = sensor_cfg->input[CSI_VIRTUAL_CH_1].hwformat;

	/* PDP context setting */
	pdp_hw_s_sensor_type(pdp->base, sensor_type);
	pdp_hw_s_core(pdp, enable, sensor_cfg, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, img_pixelsize,
		pd_width, pd_height, pd_hwformat, sensor_type, path, pdp->vc_ext_sensor_mode,
		fps, en_sdc, en_votf, frame->num_buffers, pdp->freq, pdp->binning, position);

	spin_lock_irqsave(&cmn_reg_slock, flags);
	/* ch0 context */
	pdp_hw_s_context(pdp->cmn_base, pdp->id, path);
	spin_unlock_irqrestore(&cmn_reg_slock, flags);

	if (enable && debug_pdp >= 5) {
		msinfo_hw(" is configured as default values\n", instance, hw_ip);
		pdp_hw_s_config_default(pdp->base);

		/* CAUTION: WDMA size must be set after PDSTAT_ROI block setting. */
		pdp_hw_s_wdma_init(pdp->base, pdp->id);
	}

	/* PDP OTF input mux & CSIS OTF output enable */
	if (path == OTF) {
		csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
		if (!csi) {
			mserr_hw("csi is null\n", instance, hw_ip);
			return -ENODEV;
		}

		csi_ch = csi->ch;
		writel(pdp->mux_val[csi_ch], pdp->mux_base);

		if (pdp->vc_mux_base)
			writel(pdp->vc_mux_val[csi_ch], pdp->vc_mux_base);
		else
			mserr_hw("pdp_vc_mux_base is null: need to check DT define\n", instance, hw_ip);

		spin_lock_irqsave(&cmn_reg_slock, flags);
		en_val = readl(pdp->en_base);

		for (i = 0; i < pdp->en_elems; i++)
			en_val &= ~(1 << pdp->en_val[i]);

		en_val |= 1 << pdp->en_val[csi_ch];
		writel(en_val, pdp->en_base);
		spin_unlock_irqrestore(&cmn_reg_slock, flags);

		msinfo_hw("[NS]CSI(%d) --> PDP(%d)\n", instance, hw_ip, csi_ch, pdp->id);
	}

	switch (path) {
	case OTF:
	case STRGEN:
		/* This path selection must be set at the end. */
		pdp_hw_s_path(pdp->base, path);
		pdp_hw_s_global_enable(pdp->base, true);
		break;
		break;
	case DMA:
#if defined(VOTF_ONESHOT)
		if (en_votf)
			set_bit(IS_PDP_VOTF_ONESHOT_FIRST_FRAME, &pdp->state);
#else
		/* This path selection must be set at the end. */
		if (en_votf) {
			pdp_hw_s_path(pdp->base, path);
			pdp_hw_s_global_enable(pdp->base, true);
		}
#endif
		break;
	default:
		mserr_hw("Invalid input path(%d)\n", instance, hw_ip, path);
		ret = -EINVAL;
		break;
	}

	if (debug_pdp >= 2)
		pdp_hw_dump(pdp->base);

	msinfo_hw(" %s as PD mode: %d, INT1: 0x%x, INT2: 0x%x\n", instance, hw_ip,
		enable ? "enabled" : "disabled", pd_mode,
		pdp_hw_g_int1_state(pdp->base, false, &pdp->irq_state[PDP_INT1])
			& pdp_hw_g_int1_mask(pdp->base),
		pdp_hw_g_int2_state(pdp->base, false, &pdp->irq_state[PDP_INT2])
			& pdp_hw_g_int2_mask(pdp->base));

	return ret;
}

static int is_hw_pdp_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret = 0;
	struct is_pdp *pdp;
	int work_id;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWPDP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	spin_lock_init(&pdp->slock_paf_action);
	INIT_LIST_HEAD(&pdp->list_of_paf_action);
	spin_lock_init(&pdp->slock_paf_s_param);
	spin_lock_init(&pdp->slock_oneshot);

	for (work_id = WORK_PDP_STAT0; work_id < WORK_PDP_MAX; work_id++)
		init_work_list(&pdp->work_list[work_id], work_id, MAX_WORK_COUNT);

	msinfo_hw(" is registered\n", instance, hw_ip);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return ret;
}

static int is_hw_pdp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_pdp *pdp;
	struct is_device_ischain *device;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		struct is_subdev *subdev = &device->pdst;
		u32 w, h, b;

		if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
			ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN, subdev);
			if (ret) {
				merr("is_subdev_internal_open is fail(%d)", device, ret);
				return ret;
			}

			if (IS_ENABLED(USE_DEBUG_PD_DUMP) && ((DEBUG_PD_DUMP_CH >> pdp->id) & 0x1)) {
				struct is_sensor_cfg *sensor_cfg;

				if (!device->sensor) {
					mserr_hw("failed to get sensor", instance, hw_ip);
					return -EINVAL;
				}

				sensor_cfg = device->sensor->cfg;
				if (!sensor_cfg) {
					mserr_hw("failed to get senso_cfg", instance, hw_ip);
					return -EINVAL;
				}

				w = sensor_cfg->input[1].width;
				h = sensor_cfg->input[1].height;
				b = 2;

				msinfo_hw("[DBG] set PDSTAT Dump mode", instance, hw_ip);
			} else {
				pdp_hw_g_pdstat_size(&w, &h, &b);
			}

			ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, subdev,
						w, h, b, SUBDEV_INTERNAL_BUF_MAX, "PDSTAT");
			if (ret) {
				merr("is_subdev_internal_s_format is fail(%d)", device, ret);
				return ret;
			}

			ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, subdev);
			if (ret) {
				merr("subdev internal start is fail(%d)", device, ret);
				return ret;
			}
		}
	}

	set_bit(HW_INIT, &hw_ip->state);

	return ret;
}

static int is_hw_pdp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_pdp *pdp;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		struct is_subdev *subdev = &device->pdst;

		if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
			ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, subdev);
			if (ret)
				merr("subdev internal stop is fail(%d)", device, ret);

			ret = is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, subdev);
			if (ret)
				merr("is_subdev_internal_close is fail(%d)", device, ret);
		}
	}

	clear_bit(IS_PDP_SET_PARAM, &pdp->state);
	clear_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);

	return ret;
}

static int is_hw_pdp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_pdp *pdp;
	struct paf_action *pa, *temp;
	unsigned long flag;
	int i;
	struct is_hardware *hardware;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	if (!list_empty(&pdp->list_of_paf_action)) {
		mserr_hw("flush remaining notifiers...", instance, hw_ip);
		spin_lock_irqsave(&pdp->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp,
				&pdp->list_of_paf_action, list) {
			list_del(&pa->list);
			kfree(pa);
		}
		spin_unlock_irqrestore(&pdp->slock_paf_action, flag);
	}

	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	clear_bit(IS_PDP_SET_PARAM, &pdp->state);
	clear_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);
	clear_bit(IS_PDP_ONESHOT_PENDING, &pdp->state);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * For safe power off
	 * This is common register for all PDP channel.
	 * So, it should be set one time in final instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < pdp->max_num; i++) {
		int hw_id = DEV_HW_PAF0 + i;
		int hw_slot = is_hw_slot_id(hw_id);
		struct is_hw_ip *hw_ip_phys;

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			mutex_unlock(&cmn_reg_lock);
			return -EINVAL;
		}

		hw_ip_phys = &hardware->hw_ip[hw_slot];

		if (hw_ip_phys && test_bit(HW_OPEN, &hw_ip_phys->state))
			break;
	}

	if (i == pdp->max_num) {
		pdp_hw_s_reset(pdp->cmn_base);

		ret = pdp_hw_wait_idle(pdp->base, pdp->state);
		if (ret)
			mserr_hw("failed to pdp_hw_wait_idle", instance, hw_ip);

		msinfo_hw("final finished pdp ch%d\n", instance, hw_ip, pdp->id);
	}
	mutex_unlock(&cmn_reg_lock);

	msinfo_hw(" is unregistered\n", instance, hw_ip);

	return ret;
}

static int is_hw_pdp_set_subdev(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_pdp *pdp;
	struct is_device_ischain *device;
	struct is_group *group;

	FIMC_BUG(!hw_ip);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	/* subdev change for RTA */
	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		struct is_device_sensor *sensor;
		struct is_module_enum *module;
		struct is_device_sensor_peri *sensor_peri;
		struct v4l2_subdev *subdev_module;

		sensor = device->sensor;
		if (!sensor) {
			mserr_hw("failed to get sensor", instance, hw_ip);
			return -EINVAL;
		}

		module = (struct is_module_enum *)v4l2_get_subdevdata(sensor->subdev_module);

		subdev_module = module->subdev;
		sensor_peri = module->private_data;

		sensor_peri->pdp = pdp;
		sensor_peri->subdev_pdp = pdp->subdev;
	}

	return ret;
}

static int is_hw_pdp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_pdp *pdp;
	struct paf_rdma_param *param;
	struct is_hardware *hardware;
	struct pdp_lic_lut *lic_lut = NULL;
	u32 path;
	u32 en_votf;
	int i;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	/* Change or set subdev for RTA */
	ret = is_hw_pdp_set_subdev(hw_ip, instance);
	if (ret) {
		mserr_hw("is_hw_pdp_set_subdev is fail", instance, hw_ip);
		return ret;
	}

	pdp->prev_instance = IS_STREAM_COUNT;
	atomic_inc(&hw_ip->run_rsccount);

	param = &hw_ip->region[instance]->parameter.paf;
	if (param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE)
		path = DMA;
	else
		path = OTF;

	en_votf = param->dma_input.v_otf_enable;
	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		struct is_group *group = hw_ip->group[instance];

		ret = is_votf_register_framemgr(group, TRS, pdp, pdp_s_rdma_addr, ENTRY_PAF);
		if (ret) {
			mserr_hw("(Bayer) is_votf_register_framemgr is failed", instance, hw_ip);
			return ret;
		}

		ret = is_votf_register_framemgr(group, TRS, pdp, pdp_s_af_rdma_addr, ENTRY_PDAF);
		if (ret) {
			mserr_hw("(AF) is_votf_register_framemgr is failed", instance, hw_ip);
			return ret;
		}

#if defined(VOTF_ONESHOT)
		ret = is_votf_register_oneshot(group, TRS, pdp, pdp_s_one_shot_enable, ENTRY_PAF);
		if (ret) {
			mserr_hw(" is_votf_register_oneshot is failed", instance, hw_ip);
			return ret;
		}
#endif
	}

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * This is common register for all PDP channel.
	 * So, it should be set one time in first instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < pdp->max_num; i++) {
		int hw_id = DEV_HW_PAF0 + i;
		int hw_slot = is_hw_slot_id(hw_id);
		struct is_hw_ip *hw_ip_phys;

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			mutex_unlock(&cmn_reg_lock);
			return -EINVAL;
		}

		hw_ip_phys = &hardware->hw_ip[hw_slot];

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == pdp->max_num) {
		pdp_hw_s_reset(pdp->cmn_base);

		/* ch0 global */
		if (param->control.lut_index) {
			lic_lut = &pdp->lic_lut[param->control.lut_index];
			msinfo_hw("LIC LUT %d is selected. (%s, %d, %d, %d)\n", instance, hw_ip,
				param->control.lut_index, lic_lut->mode ? "STATIC" : "DYNAMIC",
				lic_lut->param0, lic_lut->param1, lic_lut->param2);
		}

		pdp_hw_g_ip_version(pdp->cmn_base);
		pdp_hw_s_init(pdp->cmn_base);
		pdp_hw_s_global(pdp->cmn_base, pdp->id, pdp->lic_mode_def, lic_lut);

		/*
		 * Due to limitation, mapping between LIC and core need to do sequencially.
		 * So, If other PDP uses first time, need to map LIC0 - Core0 first
		 */
		pdp_hw_s_pdstat_path(pdp->cmn_base, false);
		if (hw_ip->id != DEV_HW_PAF0) {
			msinfo_hw("Need to map LIC0 - Core0\n", instance, hw_ip);
		}

		msinfo_hw("first enterance pdp ch%d\n", instance, hw_ip, pdp->id);
	}

	set_bit(HW_RUN, &hw_ip->state);
	mutex_unlock(&cmn_reg_lock);

	return ret;
}

static int is_hw_pdp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_pdp *pdp;
	struct is_framemgr *stat_framemgr;
	struct is_frame *stat_frame;
	unsigned long flags;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	msinfo_hw("disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	stat_framemgr = pdp->stat_framemgr;
	if (stat_framemgr) {
		framemgr_e_barrier_irqs(stat_framemgr, FMGR_IDX_30, flags);
		stat_frame = get_frame(stat_framemgr, FS_REQUEST);
		while (stat_frame) {
			put_frame(stat_framemgr, stat_frame, FS_FREE);
			stat_frame = get_frame(stat_framemgr, FS_REQUEST);
		}
		framemgr_x_barrier_irqr(stat_framemgr, FMGR_IDX_30, flags);
	}

	if (atomic_read(&hw_ip->run_rsccount) == 0) {
		mswarn_hw("run_rsccount is not paired.\n", instance, hw_ip);
		return 0;
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	if (flush_work(&pdp->work_stat[WORK_PDP_STAT0]))
		msinfo_hw("flush pdp wq for stat0\n", instance, hw_ip);
	if (flush_work(&pdp->work_stat[WORK_PDP_STAT1]))
		msinfo_hw("flush pdp wq for stat1\n", instance, hw_ip);

	clear_bit(HW_RUN, &hw_ip->state);

	return ret;
}

static int is_hw_pdp_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance)
{
	int ret = 0;
	u32 hw_format, bitwidth;
	size_t width, height;
	struct is_pdp *pdp;
	struct paf_rdma_param *param;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!region);
	FIMC_BUG(!hw_ip->priv_info);

	pdp = (struct is_pdp *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter.paf;

	hw_format = param->dma_input.format;
	bitwidth = param->dma_input.bitwidth;
	width = param->dma_input.width;
	height = param->dma_input.height;

	return ret;
}

static int is_hw_pdp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int i, cur_idx;
	struct is_pdp *pdp;
	struct is_region *region;
	struct paf_rdma_param *param;
	u32 lindex = 0, hindex = 0, instance;
	u32 num_buffers;
	u32 en_votf;
	struct is_crop img_full_size = {0, 0, 0, 0};
	struct is_crop img_crop_size = {0, 0, 0, 0};
	struct is_crop img_comp_size = {0, 0, 0, 0};
	u32 img_hwformat, img_pixelsize;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	num_buffers = frame->num_buffers;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	pdp = (struct is_pdp *)hw_ip->priv_info;
	region = hw_ip->region[instance];
	FIMC_BUG(!region);

	if (frame->type != SHOT_TYPE_INTERNAL) {
		FIMC_BUG(!frame->shot);

		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;
	}

	/* multi-buffer */
	hw_ip->num_buffers = num_buffers;

	ret = is_hw_pdp_update_param(hw_ip,
		region,
		lindex, hindex, instance);

	ret = is_hw_pdp_init_config(hw_ip, instance, frame);
	if (ret) {
		mserr_hw("is_hw_pdp_init_config is fail", instance, hw_ip);
		return -EINVAL;
	}

	/* RDMA settings */

	param = &hw_ip->region[instance]->parameter.paf;
	en_votf = param->dma_input.v_otf_enable;
	if ((param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE)
		&& (en_votf == OTF_INPUT_COMMAND_DISABLE)) {
		cur_idx = frame->cur_buf_index;

		for (i = 0; i < num_buffers; i++) {
			if (frame->dvaddr_buffer[i + cur_idx] == 0) {
				msinfo_hw("[F:%d]dvaddr_buffer[%d] is zero\n",
					instance, hw_ip, frame->fcount, i);
				FIMC_BUG(1);
			}
		}

		if (lindex & LOWBIT_OF(PARAM_PAF_DMA_INPUT) ||
			hindex & HIGHBIT_OF(PARAM_PAF_DMA_INPUT)) {
			img_full_size.w = param->dma_input.width;
			img_full_size.h = param->dma_input.height;
			img_crop_size.w = param->dma_input.dma_crop_width;
			img_crop_size.h = param->dma_input.dma_crop_height;

			if (pdp->en_sdc) {
				img_comp_size.w = img_full_size.w;
				img_comp_size.h = img_full_size.h;
			} else {
				img_comp_size.w = img_crop_size.w;
				img_comp_size.h = img_crop_size.h;
			}

			img_hwformat = param->dma_input.format;
			img_pixelsize = param->dma_input.msb + 1;

			pdp_hw_s_img_size(pdp->base, img_full_size, img_crop_size, img_comp_size,
				img_hwformat, img_pixelsize);
		}

		pdp_hw_s_fro(pdp->base, num_buffers);
		pdp_hw_s_rdma_addr(pdp->base, &frame->dvaddr_buffer[cur_idx], num_buffers);
		pdp_hw_s_path(pdp->base, DMA);
		if (pdp_hw_g_idle_state(pdp->base))
			pdp_hw_s_one_shot_enable(pdp);
	}

	return ret;
}

static int is_hw_pdp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	return ret;
}

static int is_hw_pdp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (test_bit_variables(hw_ip->id, &frame->core_flag))
		ret = is_hardware_frame_done(hw_ip, frame, -1,
				IS_HW_CORE_END, done_type, false);

	return ret;
}

static int is_hw_pdp_sensor_start(struct is_hw_ip *hw_ip, u32 instance)
{
	int i;
	struct is_hardware *hardware;
	struct is_pdp *pdp, *pdp_phys;
	u32 en_votf;
	int hw_id;
	int hw_slot;
	struct is_hw_ip *hw_ip_phys;
	u32 instance_phys;
	struct is_region *region;
	struct is_group *group;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	ulong freq;
	ulong fps;
	ulong ratio = 10; /* Portion of V-blank is 10% */
	u32 val;

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	freq = is_get_rate_dt(pdp->dev, pdp->aclk_name);
	if (!freq) {
		mswarn_hw("failed to get PDP clock", instance, hw_ip);
		return 0;
	}

	pdp->freq = freq;

	/*
	 * Below function is not necessary currently,
	 * because 3AA receive interrupt relay from PDP instead of 3AA start interrupt.
	 * So, 3AA start and end interrutp is not overlapped anymore when using VOTF.
	 * If post frame gap have to be needed, remove this return.
	 */
	return 0;

	hardware = hw_ip->hardware;

	for (i = 0; i < pdp->max_num; i++) {
		hw_id = DEV_HW_PAF0 + i;
		hw_slot = is_hw_slot_id(hw_id);

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			return -EINVAL;
		}

		hw_ip_phys = &hardware->hw_ip[hw_slot];

		if (!hw_ip_phys || !test_bit(HW_RUN, &hw_ip_phys->state))
			continue;

		instance_phys = atomic_read(&hw_ip_phys->instance);
		region = hw_ip_phys->region[instance_phys];
		if (!region)
			continue;

		en_votf = region->parameter.paf.dma_input.v_otf_enable;
		if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
			pdp_phys = (struct is_pdp *)hw_ip_phys->priv_info;
			if (!pdp_phys) {
				mserr_hw("failed to get PDP", instance_phys, hw_ip_phys);
				return -ENODEV;
			}

			group = hw_ip_phys->group[instance_phys];
			if (!group) {
				mserr_hw("failed to get group", instance_phys, hw_ip_phys);
				return -ENODEV;
			}

			device = group->device;
			if (!device) {
				mserr_hw("failed to get devcie", instance_phys, hw_ip);
				return -ENODEV;
			}

			sensor = device->sensor;
			if (!sensor) {
				mserr_hw("failed to get sensor", instance_phys, hw_ip);
				return -ENODEV;
			}

			sensor_cfg = sensor->cfg;
			if (!sensor_cfg) {
				mserr_hw("failed to get senso_cfgr", instance_phys, hw_ip);
				return -EINVAL;
			}

			/*
			 * Calculate v-blank time
			 * 1 = 1 / freq (sec).
			 */
			fps = (ulong)sensor_cfg->framerate;
			val = (u32)(freq / (fps * ratio));
			msinfo_hw("Force V-Blank (freq: %lu, fps: %lu, ratio: %lu, val: %x)\n",
				instance_phys, hw_ip_phys, freq, fps, ratio, val);

			/* V-blnak control in VOTF case */
			pdp_hw_s_post_frame_gap(pdp_phys->base, val);
		}
	}

	return 0;
}

static int is_hw_pdp_sensor_stop(struct is_hw_ip *hw_ip, u32 instance)
{
	int i, ret = 0;
	struct is_pdp *pdp;
	struct is_region *region;
	u32 en_val, en_votf;
	ulong flags = 0;

	pdp = (struct is_pdp *)hw_ip->priv_info;
	if (!pdp) {
		mserr_hw("failed to get PDP", instance, hw_ip);
		return -ENODEV;
	}

	/* This is for corex imediate setting after stream off. */
	pdp_hw_s_global_enable(pdp->base, false);

	region = hw_ip->region[instance];
	if (!region) {
		mserr_hw("region is NULL", instance, hw_ip);
		return -EINVAL;
	}

	en_votf = region->parameter.paf.dma_input.v_otf_enable;
	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		ret = pdp_hw_wait_idle(pdp->base, pdp->state);
		if (ret)
			mserr_hw("failed to pdp_hw_wait_idle: last INT1(0x%X) INT2(0x%X)",
				instance, hw_ip,
				pdp->irq_state[PDP_INT1],
				pdp->irq_state[PDP_INT2]);
	}

	clear_bit(IS_PDP_SET_PARAM, &pdp->state);
	clear_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);
	clear_bit(IS_PDP_ONESHOT_PENDING, &pdp->state);

	spin_lock_irqsave(&cmn_reg_slock, flags);
	en_val = readl(pdp->en_base);

	for (i = 0; i < pdp->en_elems; i++)
		en_val &= ~(1 << pdp->en_val[i]);

	writel(en_val, pdp->en_base);
	spin_unlock_irqrestore(&cmn_reg_slock, flags);

	return 0;
}

static int is_hw_pdp_change_chain(struct is_hw_ip *hw_ip, u32 instance,
	u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	u32 curr_id;
	u32 next_hw_id;
	int hw_slot;
	struct is_hw_ip *next_hw_ip;

	curr_id = hw_ip->id - DEV_HW_PAF0;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip,
			curr_id, next_id);
		goto p_err;
	}

	next_hw_id = DEV_HW_PAF0 + next_id;
	hw_slot = is_hw_slot_id(next_hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		merr_hw("[ID:%d]invalid next hw_slot_id [SLOT:%d]", instance,
			next_hw_id, hw_slot);
		return -EINVAL;
	}

	next_hw_ip = &hardware->hw_ip[hw_slot];

	/* This is for decreasing run_rsccount */
	ret = is_hw_pdp_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_pdp_disable is fail", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Copy instance infromation.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be refered at close time.
	 */
	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];
	next_hw_ip->hindex[instance] = hw_ip->hindex[instance];
	next_hw_ip->lindex[instance] = hw_ip->lindex[instance];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	/* This is for increasing run_rsccount & set LIC config */
	ret = is_hw_pdp_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_pdp_enable is fail", instance, next_hw_ip);
		return -EINVAL;
	}

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);
p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = NULL
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = NULL,
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = NULL
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

struct is_pdp_ops pdp_ops = {
	.set_param = pdp_set_param,
	.get_ready = pdp_get_ready,
	.register_notifier = pdp_register_notifier,
	.unregister_notifier = pdp_unregister_notifier,
	.notify = pdp_notify,
};

const struct is_hw_ip_ops is_hw_pdp_ops = {
	.open			= is_hw_pdp_open,
	.init			= is_hw_pdp_init,
	.deinit			= is_hw_pdp_deinit,
	.close			= is_hw_pdp_close,
	.enable			= is_hw_pdp_enable,
	.disable		= is_hw_pdp_disable,
	.shot			= is_hw_pdp_shot,
	.set_param		= is_hw_pdp_set_param,
	.frame_ndone		= is_hw_pdp_frame_ndone,
	.sensor_start		= is_hw_pdp_sensor_start,
	.sensor_stop		= is_hw_pdp_sensor_stop,
	.change_chain		= is_hw_pdp_change_chain,
};

static int pdp_get_array_val(struct device *dev, struct device_node *dnode, char *name, u32 **val,
	u32 *elems)
{
	int ret = 0;

	*elems = of_property_count_u32_elems(dnode, name);
	if (*elems < 0) {
		dev_err(dev, "failed to get mux_val property\n");
		ret = -EINVAL;
		return ret;
	}

	*val = devm_kcalloc(dev, *elems, sizeof(**val), GFP_KERNEL);
	if (!*val) {
		dev_err(dev, "out of memory for mux_val\n");
		ret = -ENOMEM;
		return ret;
	}

	ret = of_property_read_u32_array(dnode, name, *val, *elems);
	if (ret) {
		dev_err(dev, "failed to get mux_val resources\n");
		return ret;
	}

	return 0;
}

static int pdp_get_resource_mem_byname(struct platform_device *pdev, char *name, void __iomem **base, u32 **val,
	u32 *elems)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *dnode = dev->of_node;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		dev_err(dev, "can't get memory resource for %s_base\n", name);
		return -ENODEV;
	}

	if (!base) {
		dev_err(dev, "invalid base for %s_base\n", name);
		return -EINVAL;
	}

	*base = devm_ioremap(dev, res->start, resource_size(res));
	if (!*base) {
		dev_err(dev, "ioremap failed for %s_base\n", name);
		return -ENOMEM;
	}

	if (of_find_property(dnode, name, NULL) && val && elems) {
		ret = pdp_get_array_val(dev, dnode, name, val, elems);
		if (ret) {
			dev_err(dev, "failed to pdp_get_array_val\n");
			return ret;
		}
	}

	return 0;
}

static int pdp_probe(struct platform_device *pdev)
{
	int ret;
	int id;
	bool leader;
	struct resource *res;
	struct is_pdp *pdp;
	struct device *dev = &pdev->dev;
	struct is_core *core;
	struct is_hw_ip *hw_ip;
	int hw_id, hw_slot;
	int max_num_of_pdp;
	int num_scen;
	struct device_node *lic_lut_np, *scen_np;
	int i, work_id;
	int reg_size;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)) && defined(ENABLE_IRQ_MULTI_TARGET_CL0)
	const char *buf = "0-3";
	struct cpumask cpumask;
#endif

	if (is_dev == NULL) {
		warn("is_dev is not yet probed(pdp)");
		return -EPROBE_DEFER;
	}

	max_num_of_pdp = of_alias_get_highest_id("pdp");
	if (max_num_of_pdp < 0) {
		dev_err(dev, "invalid alias name\n");
		return max_num_of_pdp;
	}
	max_num_of_pdp += 1;

	id = of_alias_get_id(dev->of_node, "pdp");
	leader = of_property_read_bool(dev->of_node, "leader");

	core = (struct is_core *)dev_get_drvdata(is_dev);

	switch (id) {
	case 0:
		hw_id = DEV_HW_PAF0;
		if (leader)
			is_paf0s_video_probe(core);
		break;
	case 1:
		hw_id = DEV_HW_PAF1;
		if (leader)
			is_paf1s_video_probe(core);
		break;
	case 2:
		hw_id = DEV_HW_PAF2;
		if (leader)
			is_paf2s_video_probe(core);
		break;
	case 3:
		hw_id = DEV_HW_PAF3;
		if (leader)
			is_paf3s_video_probe(core);
		break;
	default:
		dev_err(dev, "invalid id (out-of-range)\n");
		return -EINVAL;
	}

	hw_slot = is_hw_slot_id(hw_id);
	hw_ip = &core->hardware.hw_ip[hw_slot];

	pdp = devm_kzalloc(&pdev->dev, sizeof(struct is_pdp), GFP_KERNEL);
	if (!pdp) {
		dev_err(dev, "failed to alloc memory for pdp\n");
		return -ENOMEM;
	}

	/* LIC LUT */
	lic_lut_np = of_parse_phandle(dev->of_node, "lic_lut", 0);
	if (lic_lut_np) {
		ret = of_property_read_u32(lic_lut_np, "lic_mode_default", &pdp->lic_mode_def);
		if (ret) {
			dev_warn(dev, "lic_mode_default read is fail(%d)", ret);
			pdp->lic_mode_def = 0;
		}

		num_scen = of_get_child_count(lic_lut_np);

		pdp->lic_lut = devm_kcalloc(&pdev->dev, num_scen, sizeof(struct pdp_lic_lut), GFP_KERNEL);
		if (!pdp->lic_lut) {
			dev_err(dev, "failed to alloc memory for pdp->lic_lut\n");
			return -ENOMEM;
		}

		i = 0;
		for_each_child_of_node(lic_lut_np, scen_np) {
			ret = of_property_read_u32(scen_np, "mode", &pdp->lic_lut[i].mode);
			if (ret) {
				dev_err(dev, "failed to parse mode in LIC LUT\n");
				return -EINVAL;
			}

			ret = of_property_read_u32(scen_np, "param0", &pdp->lic_lut[i].param0);
			if (ret) {
				dev_err(dev, "failed to parse param0 in LIC LUT\n");
				return -EINVAL;
			}

			ret = of_property_read_u32(scen_np, "param1", &pdp->lic_lut[i].param1);
			if (ret) {
				dev_err(dev, "failed to parse param1 in LIC LUT\n");
				return -EINVAL;
			}

			ret = of_property_read_u32(scen_np, "param2", &pdp->lic_lut[i].param2);
			if (ret) {
				dev_err(dev, "failed to parse param2 in LIC LUT\n");
				return -EINVAL;
			}
			i++;
		}
	} else {
		dev_warn(dev, "LIC LUT is not defined in DT\n");
	}

	pdp->id = id;
	pdp->max_num = max_num_of_pdp;
	pdp->prev_instance = IS_STREAM_COUNT;
	pdp->dev = dev;
	of_property_read_string(dev->of_node, "clock-names", &pdp->aclk_name);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get memory resource\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start, resource_size(res),
				dev_name(dev))) {
		dev_err(dev, "can't request region for resource %pR\n", res);
		return -EBUSY;
	}

	pdp->regs_start = res->start;
	pdp->regs_end = res->end;

	pdp->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!pdp->base) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}

	/* alloc sfr dump memory */
	hw_ip->regs_start[REG_SETA] = pdp->regs_start;
	hw_ip->regs_end[REG_SETA] = pdp->regs_end;
	hw_ip->regs[REG_SETA] = pdp->base;

	reg_size = (hw_ip->regs_end[REG_SETA] - hw_ip->regs_start[REG_SETA] + 1);
	hw_ip->sfr_dump[REG_SETA] = kzalloc(reg_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(hw_ip->sfr_dump[REG_SETA])) {
		serr_hw("sfr dump memory alloc fail", hw_ip);
		goto err_sfr_dump_alloc;
	} else {
		sinfo_hw("sfr dump memory (V/P/S):(%lx/%lx/0x%X)[0x%llX~0x%llX]\n", hw_ip,
			(ulong)hw_ip->sfr_dump[REG_SETA], (ulong)virt_to_phys(hw_ip->sfr_dump[REG_SETA]),
			reg_size, hw_ip->regs_start[REG_SETA], hw_ip->regs_end[REG_SETA]);
	}

	/* Common Register */
	ret = pdp_get_resource_mem_byname(pdev, "common", &pdp->cmn_base, NULL, NULL);
	if (ret && (ret != -ENODEV)) {
		dev_err(dev, "can't get memory resource for cmn_base\n");
		goto err_get_cmn_base;
	}

	/* SYSREG: PDP input mux */
	ret = pdp_get_resource_mem_byname(pdev, "mux", &pdp->mux_base, &pdp->mux_val,
		&pdp->mux_elems);
	if (ret && (ret != -ENODEV)) {
		dev_err(dev, "can't get memory resource for mux_base\n");
		goto err_get_mux_base;
	}

	/* SYSREG: PDP input vc mux */
	ret = pdp_get_resource_mem_byname(pdev, "vc_mux", &pdp->vc_mux_base, &pdp->vc_mux_val,
		&pdp->vc_mux_elems);
	if (ret && (ret != -ENODEV))
		dev_err(dev, "can't get memory resource for vc_mux_base\n");

	/* SYSREG: PDP input enable */
	ret = pdp_get_resource_mem_byname(pdev, "en", &pdp->en_base, &pdp->en_val,
		&pdp->en_elems);
	if (ret && (ret != -ENODEV)) {
		dev_err(dev, "can't get memory resource for en_base\n");
		goto err_get_en_base;
	}

	pdp->irq[PDP_INT1] = platform_get_irq(pdev, 0);
	if (pdp->irq[PDP_INT1] < 0) {
		dev_err(dev, "failed to get INT1 resource: %d\n", pdp->irq[PDP_INT1]);
		ret = pdp->irq[PDP_INT1];
		goto err_get_int1;
	}
	ret = devm_request_irq(dev, pdp->irq[PDP_INT1], is_isr_pdp_int1,
			IRQF_SHARED | IS_HW_IRQ_FLAG,
			dev_name(dev), hw_ip);
	if (ret) {
		dev_err(dev, "failed to request INT1(%d): %d\n", pdp->irq[PDP_INT1], ret);
		goto err_req_int1;
	}
#if defined(ENABLE_IRQ_MULTI_TARGET_CL0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	cpulist_parse(buf, &cpumask);
	irq_set_affinity_hint(pdp->irq[PDP_INT1], &cpumask);
#else
	irq_force_affinity(pdp->irq[PDP_INT1], cpumask_of(0));
#endif
#endif

	pdp->irq[PDP_INT2] = platform_get_irq(pdev, 1);
	if (pdp->irq[PDP_INT2] < 0) {
		dev_err(dev, "failed to get INT2 resource: %d\n", pdp->irq[PDP_INT2]);
		ret = pdp->irq[PDP_INT2];
		goto err_get_int2;
	}
	ret = devm_request_irq(dev, pdp->irq[PDP_INT2], is_isr_pdp_int2,
			IRQF_SHARED | IS_HW_IRQ_FLAG,
			dev_name(dev), hw_ip);
	if (ret) {
		dev_err(dev, "failed to request INT2(%d): %d\n", pdp->irq[PDP_INT2], ret);
		goto err_req_int2;
	}
#if defined(ENABLE_IRQ_MULTI_TARGET_CL0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	irq_set_affinity_hint(pdp->irq[PDP_INT2], &cpumask);
#else
	irq_force_affinity(pdp->irq[PDP_INT2], cpumask_of(0));
#endif
#endif

	pdp->subdev = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!pdp->subdev) {
		dev_err(dev, "failed to alloc memory for pdp-subdev\n");
		ret = -ENOMEM;
		goto err_alloc_subdev;
	}

	v4l2_subdev_init(pdp->subdev, &subdev_ops);
	v4l2_set_subdevdata(pdp->subdev, pdp);
	snprintf(pdp->subdev->name, V4L2_SUBDEV_NAME_SIZE, "pdp-subdev.%d", pdp->id);

	pdp->pdp_ops = &pdp_ops;

	mutex_init(&pdp->control_lock);

	pdp->wq_stat = alloc_workqueue("pdp-stat0/[H/U]", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!pdp->wq_stat)
		probe_warn("failed to alloc PDP own workqueue, will be use global one");

	INIT_WORK(&pdp->work_stat[WORK_PDP_STAT0], pdp_worker_stat0);
	INIT_WORK(&pdp->work_stat[WORK_PDP_STAT1], pdp_worker_stat1);

	platform_set_drvdata(pdev, pdp);

	/* initialize device hardware */
	hw_ip->priv_info = pdp;
	hw_ip->id = hw_id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s%d", "PDP", id);
	hw_ip->ops = &is_hw_pdp_ops;
	hw_ip->itf  = &core->interface;
	hw_ip->itfc = NULL; /* interface_ischain is not neccessary */
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = leader;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->status.otf_start, 0);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	for (work_id = WORK_PDP_STAT0; work_id < WORK_PDP_MAX; work_id++)
		init_work_list(&pdp->work_list[work_id], work_id, MAX_WORK_COUNT);

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	clear_bit(IS_PDP_SET_PARAM, &pdp->state);
	clear_bit(IS_PDP_SET_PARAM_COPY, &pdp->state);
	clear_bit(IS_PDP_ONESHOT_PENDING, &pdp->state);

	pdp_hw_g_int1_str(pdp->int1_str);
	pdp_hw_g_int2_str(pdp->int2_str);
	pdp_hw_g_int2_rdma_str(pdp->int2_rdma_str);

	probe_info("%s device probe success\n", dev_name(dev));

	return 0;

err_alloc_subdev:
#if defined(ENABLE_IRQ_MULTI_TARGET_CL0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	irq_set_affinity_hint(pdp->irq[PDP_INT2], NULL);
#endif
#endif
	devm_free_irq(dev, pdp->irq[PDP_INT2], pdp);
err_get_int2:
err_req_int2:
#if defined(ENABLE_IRQ_MULTI_TARGET_CL0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	irq_set_affinity_hint(pdp->irq[PDP_INT1], NULL);
#endif
#endif
	devm_free_irq(dev, pdp->irq[PDP_INT1], pdp);
err_get_int1:
err_req_int1:
	devm_iounmap(dev, pdp->en_base);
err_get_en_base:
	devm_iounmap(dev, pdp->mux_base);
err_get_mux_base:
	devm_iounmap(dev, pdp->cmn_base);
err_get_cmn_base:
	kfree(hw_ip->sfr_dump[REG_SETA]);
err_sfr_dump_alloc:
	devm_iounmap(dev, pdp->base);
err_ioremap:
	devm_release_mem_region(dev, res->start, resource_size(res));

	return ret;
}

static const struct of_device_id sensor_paf_pdp_match[] = {
	{
		.compatible = "samsung,sensor-paf-pdp",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_paf_pdp_match);

struct platform_driver sensor_paf_pdp_platform_driver = {
	.probe = pdp_probe,
	.driver = {
		.name   = "Sensor-PAF-PDP",
		.owner  = THIS_MODULE,
		.of_match_table = sensor_paf_pdp_match,
	}
};

#ifndef MODULE
static int __init sensor_paf_pdp_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_paf_pdp_platform_driver, pdp_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_paf_pdp_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_paf_pdp_init);
#endif
