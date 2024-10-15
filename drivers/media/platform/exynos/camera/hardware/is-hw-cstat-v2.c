// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo Image Subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/moduleparam.h>

#include "exynos-is.h"
#include "is-hw-cstat.h"
#include "is-err.h"
#include "is-hw-api-cstat.h"
#include "is-hw.h"
#include "is-dvfs.h"
#include "is-votfmgr.h"
#include "pablo-obte.h"
#include "pablo-icpu-adapter.h"
#include "is-interface-sensor.h"

#define REQ_START_2_SHOT	2000	/* 2ms */
#define REQ_SHOT_2_END		10000	/* 10ms */
#define REQ_END_2_START		1000	/* 1ms */
#define REQ_CINROW_RATIO	15	/* 15% */
#define DVFS_LEVEL_RATIO	2	/* cstat clock change ratio */
#define IGNORE_COREX_DEALY	4	/* for 3 frames */

static DEFINE_MUTEX(cmn_reg_lock);

enum is_hw_cstat_dbg_type {
	CSTAT_DBG_NONE = 0,
	CSTAT_DBG_REG_DUMP,
	CSTAT_DBG_S2D,
	CSTAT_DBG_WDMA_OFF,
	CSTAT_DBG_TYPE_NUM
};

struct is_hw_cstat_dbg_info {
	u32 type;
	u32 instance;
	u32 fcount;
	u32 src1;
	u32 src2;
	ulong dma_id;
};

static struct is_hw_cstat_dbg_info dbg_info = {
	.type = CSTAT_DBG_NONE,
	.instance = 0,
	.fcount = 0,
	.src1 = 0,
	.src2 = 0,
	.dma_id = 0,
};

static struct is_cstat_config conf_default = {
	.thstatpre_bypass = true,
	.thstatawb_bypass = true,
	.thstatae_bypass = true,
	.rgbyhist_bypass = true,
	.lmeds_bypass = true,
	.drcclct_bypass = true,
	.cdaf_bypass = true,
	.cdaf_mw_bypass = true,
};

static void is_hw_cstat_print_stall_status(struct is_hw_ip *hw_ip)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_ip *hw_ip_ctx;
	int hw_id;
	void __iomem *base;

	for (hw_id = DEV_HW_3AA0; hw_id <= DEV_HW_3AA3; hw_id++) {
		hw_ip_ctx = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, hw_id);
		if (!hw_ip_ctx) {
			serr_hw("invalid hw_id (%d)", hw_ip, hw_id);
			continue;
		}

		base = hw_ip_ctx->regs[REG_SETA];

		cstat_hw_print_stall_status(base, (hw_id - DEV_HW_3AA0));
	}
}

static inline void is_hw_cstat_check_dbg_info(struct is_hw_ip *hw_ip,
		u32 int_src1, u32 int_src2, u32 instance, u32 fcount)
{
	bool is_int;
	bool is_ins;
	bool is_fcount;

	is_int = ((int_src1 & dbg_info.src1) || (int_src2 & dbg_info.src2));
	is_ins = (instance == dbg_info.instance);
	is_fcount = (!dbg_info.fcount || (fcount == dbg_info.fcount));

	if (!is_int || !is_ins || !is_fcount)
		return;

	msinfo_hw("check_dbg_info: 0x%x(0x%x) 0x%x(0x%x) fcount %d(%d)\n", instance, hw_ip,
			int_src1, dbg_info.src1,
			int_src2, dbg_info.src2,
			fcount, dbg_info.fcount);

	switch (dbg_info.type) {
	case CSTAT_DBG_REG_DUMP:
		cstat_hw_dump(hw_ip->regs[REG_SETA]);
		break;
	case CSTAT_DBG_S2D:
		is_hw_cstat_print_stall_status(hw_ip);
		is_debug_s2d(true, "CSTAT_DBG_S2D");
		break;
	default:
		/* Do nothing */
		break;
	};
};

static int is_hw_cstat_dbg_set(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 dbg_type;
	u32 instance, fcount, src1, src2;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &dbg_type);
	if (ret || dbg_type >= CSTAT_DBG_TYPE_NUM) {
		err("Invalid dbg_type %u ret %d", dbg_type, ret);
		goto func_exit;
	}

	switch (dbg_type) {
	case CSTAT_DBG_REG_DUMP:
	case CSTAT_DBG_S2D:
		if (argc < 5) {
			err("Not enough parameters. %d < 5", argc);
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &instance);
		if (ret) {
			err("Invalid instance %d ret %d", instance, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[2], 0, &fcount);
		if (ret) {
			err("Invalid fcount %d ret %d", fcount, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[3], 0, &src1);
		if (ret) {
			err("Invalid src1 %d ret %d", src1, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[4], 0, &src2);
		if (ret) {
			err("Invalid src2 %d ret %d", src2, ret);
			goto func_exit;
		}

		dbg_info.type = dbg_type;
		dbg_info.instance = instance;
		dbg_info.fcount = fcount;
		dbg_info.src1 = src1;
		dbg_info.src2 = src2;

		info("Set regdump/scandump trigger:[T%d][I%d][F%d] 0x%x 0x%x\n",
				dbg_type, instance, fcount, src1, src2);

		break;
	case CSTAT_DBG_WDMA_OFF:
		if (argc < 2) {
			err("Not enough parameters. %d < 2", argc);
			goto func_exit;
		}

		ret = kstrtoul(argv[1], 0, &dbg_info.dma_id);
		if (ret) {
			err("Invalid dma_id %ld ret %d", dbg_info.dma_id, ret);
			goto func_exit;
		}

		info("Disable WDMA: 0x%lx\n", dbg_info.dma_id);

		break;
	default:
		err("NOT supported dbg_type %u", dbg_type);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int is_hw_cstat_dbg_get(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "> Set regdump/scandump trigger\n");
	ret += sprintf(buffer + ret, "  # echo <type> <instance> <fcount> <int1_msk> <int2_msk> > debug_cstat\n");
	ret += sprintf(buffer + ret, "    - type: 1=REG_DUMP, 2=S2D\n");
	ret += sprintf(buffer + ret, "    - fcount 0: Trigger for every interrupt.\n");
	ret += sprintf(buffer + ret, "    - int1/2_msk: Trigger when the specific bit is set.\n");
	ret += sprintf(buffer + ret, "================================================================\n");
	ret += sprintf(buffer + ret, "> Disable specific WDMA\n");
	ret += sprintf(buffer + ret, "  # echo 3 <dma_id> > debug_cstat\n");
	ret += sprintf(buffer + ret, "    - dma_id: 1 << Actual WDMA ID\n");
	ret += sprintf(buffer + ret, "================================================================\n");
	ret += sprintf(buffer + ret, "type %d instance %d fcount %d int1_msk 0x%x int2_msk 0x%x dma_id 0x%lx\n",
			dbg_info.type,
			dbg_info.instance,
			dbg_info.fcount,
			dbg_info.src1,
			dbg_info.src2,
			dbg_info.dma_id);

	return ret;
}

static const struct kernel_param_ops is_hw_cstat_dbg_ops = {
	.set = is_hw_cstat_dbg_set,
	.get = is_hw_cstat_dbg_get,
};

module_param_cb(debug_cstat, &is_hw_cstat_dbg_ops, NULL, 0644);

static inline void is_hw_cstat_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 hw_fcount;
	struct lib_callback_result cb_result;
	struct pablo_crta_buf_info pcsi_buf = { 0, };

	/* shot_fcount -> start_fcount */
	hw_fcount = atomic_read(&hw_ip->fcount);
	atomic_set(&hw->start_fcount, hw_fcount);

	_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START, 0);
	msdbg_hw(1, "[F%d]%s", instance, hw_ip, hw_fcount, __func__);

	if (IS_ENABLED(CRTA_CALL)) {
		ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_free_buf,
				PABLO_CRTA_BUF_PCSI, hw_fcount, true, &pcsi_buf);
		if (ret)
			mserr_hw("[PCFI]icpu_adt get_free_buf fail", instance, hw_ip);

		ret = CALL_ADT_MSG_OPS(hw->icpu_adt, send_msg_cstat_frame_start, instance,
			(void *)hw_ip, NULL, hw_fcount, &pcsi_buf);
		if (ret)
			mserr_hw("icpu_adt send_msg_cstat_frame_start fail", instance, hw_ip);

		cb_result.fcount = hw_fcount;
		is_lib_camera_callback(hw_ip, LIB_EVENT_FRAME_START_ISR, instance, &cb_result);
	} else {
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
	}
}

static int is_hw_cstat_frame_start_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount;
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info pcsi_buf = { 0, };

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore start callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore start callback invalid HW state(0x%lx)", instance, hw_ip,
				fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);

	if (msg->rsp)
		mserr_hw("frame_start fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_process_buf,
				PABLO_CRTA_BUF_PCSI, fcount, &pcsi_buf);
	if (!ret)
		CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], put_buf, &pcsi_buf);

	return 0;
}

static void is_hw_cstat_s_global_enable(struct is_hw_ip *hw_ip, bool enable, bool fro_en);
static inline void is_hw_cstat_frame_end(struct is_hw_ip *hw_ip, u32 instance, bool frame_done)
{
	int ret;
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	void __iomem *base;
	enum cstat_event_id e_id;
	u32 start_fcount;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	u32 edge_score = 0, buf_id, index;
	struct pablo_crta_buf_info stat_buf[PABLO_CRTA_CSTAT_END_MAX] = { 0, };
	struct pablo_crta_buf_info shot_buf = { 0, };
	struct is_sensor_interface *sensor_itf;
	struct vc_buf_info_t vc_buf_info;

	start_fcount = atomic_read(&hw->start_fcount);

	_is_hw_frame_dbg_ext_trace(hw_ip, start_fcount, DEBUG_POINT_FRAME_END, 0);
	msdbg_hw(1, "[F%d]%s", instance, hw_ip, start_fcount, __func__);

	base = hw_ip->regs[REG_SETA];

	if (frame_done || hw->input == DMA) {
		e_id = EVENT_CSTAT_FRAME_END;
	} else {
		e_id = EVENT_CSTAT_FRAME_END_REG_WRITE_FAILURE;
		mswarn_hw("[F%d] CONFIG_LOCK_DELAY 0x%lx 0x%lx", instance, hw_ip,
				start_fcount,
				hw->irq_state[CSTAT_INT1],
				hw->irq_state[CSTAT_INT2]);
	}

	if (IS_ENABLED(CRTA_CALL)) {
		framemgr = hw_ip->framemgr;
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = find_frame(framemgr, FS_HW_WAIT_DONE, frame_fcount, (void *)(ulong)start_fcount);
		framemgr_x_barrier_common(framemgr, 0, flags);

		if (frame) {
			shot_buf.kva = frame->shot;
			shot_buf.dva = frame->shot_dva;
		} else {
			shot_buf.kva = NULL;
			shot_buf.dva = 0;
		}

		/* Drop Current frame and Skip stat data */
		if (test_and_clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state)) {
			if (frame)
				frame->result = IS_SHOT_DROP;
		} else {
			cstat_hw_g_stat_data(hw_ip->regs[REG_SETA],
				CSTAT_STAT_EDGE_SCORE, (void *)&edge_score);

			for (buf_id = PABLO_CRTA_CSTAT_END_PRE_THUMB; buf_id <= PABLO_CRTA_CSTAT_END_CDAF_MW; buf_id++)
				CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_process_buf,
					PABLO_CRTA_BUF_PRE_THUMB + buf_id, start_fcount, &stat_buf[buf_id]);

			sensor_itf = hw->sensor_itf[instance];
			if (sensor_itf) {
				ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(sensor_itf,
					VC_BUF_DATA_TYPE_SENSOR_STAT3, &vc_buf_info);
				if (!ret)
					get_vc_dma_buf_by_fcount(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT3,
						start_fcount, &index,
						&stat_buf[PABLO_CRTA_CSTAT_END_VPDAF].dva);
			}
		}

		if (frame && frame->shot)
			copy_ctrl_to_dm(frame->shot);

		ret = CALL_ADT_MSG_OPS(hw->icpu_adt, send_msg_cstat_frame_end, instance,
					(void *)hw_ip, NULL,
					start_fcount, &shot_buf,
					PABLO_CRTA_CSTAT_END_MAX, stat_buf, edge_score);
		if (ret)
			mserr_hw("icpu_adt send_msg_cstat_frame_end fail", instance, hw_ip);
	} else {
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, start_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F%d]F.E\n", instance, hw_ip, start_fcount);

		CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END,
				IS_SHOT_SUCCESS, true);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe))
			mserr_hw("[F%d]fs %d fe %d", instance, hw_ip,
					start_fcount,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe));

		wake_up(&hw_ip->status.wait_queue);

		if (test_and_clear_bit(HW_SUSPEND, &hw_ip->state)) {
			is_hw_cstat_s_global_enable(hw_ip, false, false);
			clear_bit(HW_CONFIG, &hw_ip->state);
		}
	}
}

static int is_hw_cstat_frame_end_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount, buf_id;
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info buf_info;
	struct lib_callback_result cb_result;
	struct is_sensor_interface *sensor_itf;
	struct vc_buf_info_t vc_buf_info;

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore end callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore end callback invalid HW state(0x%lx)", instance, hw_ip,
				fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);

	if (msg->rsp)
		mserr_hw("frame_end fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	for (buf_id = PABLO_CRTA_BUF_PRE_THUMB; buf_id < PABLO_CRTA_BUF_MAX; buf_id++) {
		ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_process_buf, buf_id, fcount,
					     &buf_info);
		if (!ret)
			CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], put_buf, &buf_info);
	}

	sensor_itf = hw->sensor_itf[instance];
	if (!sensor_itf) {
		mserr_hw("[F%d] sensor_interface is NULL", instance, hw_ip, fcount);
		return -EINVAL;
	}

	ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT3, &vc_buf_info);
	if (!ret)
		put_vc_dma_buf_by_fcount(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT3, fcount);

	cb_result.fcount = fcount;
	is_lib_camera_callback(caller, LIB_EVENT_FRAME_END, instance, &cb_result);

	if (test_and_clear_bit(HW_SUSPEND, &hw_ip->state)) {
		is_hw_cstat_s_global_enable(hw_ip, false, false);
		clear_bit(HW_CONFIG, &hw_ip->state);
	}

	return 0;
}

static void is_hw_cstat_end_tasklet(unsigned long data)
{
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)data;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 instance = atomic_read(&hw_ip->instance);

	msdbg_hw(2, "%s: src 0x%lx 0x%lx\n", instance, hw_ip, __func__,
			hw->irq_state[CSTAT_INT1], hw->irq_state[CSTAT_INT2]);
}

static int is_hw_cstat_isr1(u32 hw_id, void *data)
{
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)data;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_hardware *hardware = hw_ip->hardware;
	ulong int1, int2;
	u32 instance, hw_fcount;
	u32 fs, line, fe, corex_end;
	u32 int_status;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	atomic_inc(&hw->isr_run_count);

	int1 = (ulong)cstat_hw_g_int1_status(hw_ip->regs[REG_SETA], true, hw->hw_fro_en);
	int2 = (ulong)cstat_hw_g_int2_status(hw_ip->regs[REG_SETA], false, hw->hw_fro_en);
	hw->irq_state[CSTAT_INT1] = int1;
	hw->irq_state[CSTAT_INT2] = int2;

	msdbg_hw(2, "[INT1][F%d] src 0x%lx 0x%lx\n", instance, hw_ip, hw_fcount,
			int1, int2);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[INT1] Ignore ISR before HW config 0x%lx 0x%lx\n",
				instance, hw_ip, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[INT1][F%d] invalid HW state 0x%lx src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, hw_ip->state, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("[INT1][F%d] during recovery. src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	/* Handle FS/LINE/FE interrupts */
	fs = cstat_hw_is_occurred(int1, CSTAT_FS);
	line = cstat_hw_is_occurred(int1, CSTAT_LINE);
	corex_end = cstat_hw_is_occurred(int1, CSTAT_COREX_END);
	fe = cstat_hw_is_occurred(int1, CSTAT_FE);

	if (fs && fe)
		mswarn_hw("[INT1][F%d] start/end overlapped!! src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);

	if ((fs && line) || (line && fe))
		mswarn_hw("[INT1][F%d] line overlapped!! src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);

	msdbg_hw(1, "[F%d] int_status: fs(%d), line(%d), corex_end(%d), fe(%d)\n", instance, hw_ip,
		hw_fcount, (fs > 0), (line > 0), (corex_end > 0), (fe > 0));

	if (corex_end)
		cstat_hw_s_corex_type(hw_ip->regs[REG_SETA], COREX_IGNORE);

	int_status = fs | line | corex_end | fe;

	while (int_status) {
		switch (hw->event_state) {
		case CSTAT_FS:
			if (line) {
				_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_CONFIG_LOCK_E);
				atomic_add(1, &hw_ip->count.cl);
				if (corex_end) {
					mswarn_hw("[INT1][F%d] clear invalid corex_end event",
							instance, hw_ip, hw_fcount);
					int_status &= ~corex_end;
					corex_end = 0;
				}

				CALL_HW_OPS(hw_ip, config_lock, hw_ip, instance, hw_fcount);

				hw->event_state = CSTAT_LINE;
				int_status &= ~line;
				line = 0;
				_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_CONFIG_LOCK_X);
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_LINE:
			if (corex_end) {
				hw->event_state = CSTAT_COREX_END;
				int_status &= ~corex_end;
				corex_end = 0;
			} else if (fe) {
				mserr_hw("[F%d] config_lock isr is delayed",
						instance, hw_ip, hw_fcount);
				hw->event_state = CSTAT_FE;
				framemgr = hw_ip->framemgr;
				if (framemgr) {
					framemgr_e_barrier(framemgr, 0);
					/* Drop Current frame */
					frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
					if (frame)
						frame->result = IS_SHOT_CONFIG_LOCK_DELAY;
					/* Drop Next frame */
					frame = peek_frame(framemgr, FS_HW_CONFIGURE);
					if (frame)
						frame->result = IS_SHOT_CONFIG_LOCK_DELAY;

					framemgr_x_barrier(framemgr, 0);
				}
				is_hw_cstat_frame_end(hw_ip, instance, false);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_COREX_END:
			if (fe) {
				hw->event_state = CSTAT_FE;
				is_hw_cstat_frame_end(hw_ip, instance, true);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_FE:
			if (fs) {
				hw->event_state = CSTAT_FS;
				is_hw_cstat_frame_start(hw_ip, instance);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_INIT:
			if (corex_end) {
				int_status &= ~corex_end;
				corex_end = 0;
			}
			if (fs) {
				hw->event_state = CSTAT_FS;
				is_hw_cstat_frame_start(hw_ip, instance);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		}

		/* Skip CSTAT_LINE, CSTAT_COREX_END event for DMA input */
		if (hw->input == DMA && hw->event_state == CSTAT_FS)
			hw->event_state = CSTAT_COREX_END;
	}
skip_isr_event:
	if (int_status)
		mswarn_hw("[F%d] invalid interrupt: event_state(%ld), int_status(%x)",
				instance, hw_ip, hw_fcount, hw->event_state, int_status);


	if (cstat_hw_is_occurred(int1, CSTAT_ERR)) {
		cstat_hw_print_err(hw_ip->name, instance, hw_fcount, int1, int2);
		is_hw_cstat_print_stall_status(hw_ip);

		if ((IS_ENABLED(USE_SKIP_DUMP_LIC_OVERFLOW)
		    && clear_gather_crc_status(instance, hw_ip) > 0)
			|| IS_ENABLED(USE_PDP_OVERFLOW_RECOVERY)) {
			set_bit(IS_SENSOR_ESD_RECOVERY,
				&hw_ip->group[instance]->device->sensor->state);
			mswarn_hw("skip to s2d dump", instance, hw_ip);
		} else if ((IS_ENABLED(USE_CSTAT_LIC_RECOVERY)
		    && cstat_hw_is_occurred(int1, CSTAT_LIC_ERR))
		    || test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state)) {
			set_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state);
			mswarn_hw("skip to s2d dump (lic recovery)", instance, hw_ip);
		} else {
			is_debug_s2d(true, "CSTAT HW Error");
		}
	}

	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	is_hw_cstat_check_dbg_info(hw_ip, int1, 0, instance, hw_fcount);

	return 0;
}

static inline void is_hw_cstat_cdaf(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct cstat_cdaf cdaf;
	struct pablo_crta_buf_info cdaf_buf[PABLO_CRTA_CDAF_DATA_MAX] = { 0, };
	struct is_sensor_interface *sensor_itf;
	u32 hw_fcount = atomic_read(&hw->start_fcount);
	u32 index, af_type;
	struct vc_buf_info_t vc_buf_info;

	/* Skip cdaf data*/
	if (test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state))
		return;

	ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_free_buf, PABLO_CRTA_BUF_CDAF,
		hw_fcount, true, &cdaf_buf[PABLO_CRTA_CDAF_RAW]);

	if (ret || !cdaf_buf[PABLO_CRTA_CDAF_RAW].kva) {
		mserr_hw("[F%d] There is no valid CDAF buffer", instance, hw_ip, hw_fcount);
		return;
	}

	cdaf.kva = (ulong)cdaf_buf[PABLO_CRTA_CDAF_RAW].kva;
	if (cstat_hw_g_stat_data(hw_ip->regs[REG_SETA], CSTAT_STAT_CDAF, (void *)&cdaf)) {
		mserr_hw("[F%d] Failed to get CDAF stat", instance, hw_ip,
				hw_fcount);
		return;
	}

	sensor_itf = hw->sensor_itf[instance];
	if (!sensor_itf) {
		mserr_hw("[F%d] sensor_interface is NULL", instance, hw_ip, hw_fcount);
		return;
	}

	/* PDAF */
	ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1, &vc_buf_info);
	if (!ret)
		get_vc_dma_buf_by_fcount(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1,
			hw_fcount, &index, &cdaf_buf[PABLO_CRTA_PDAF_TAIL].dva);

	/*LAF*/
	if(sensor_itf->cis_itf_ops.is_laser_af_available(sensor_itf)) {
		ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_free_buf, PABLO_CRTA_BUF_LAF,
			hw_fcount, true, &cdaf_buf[PABLO_CRTA_LASER_AF_DATA]);

		if (ret || !cdaf_buf[PABLO_CRTA_LASER_AF_DATA].kva) {
			mserr_hw("[F%d] There is no valid LAF buffer", instance, hw_ip, hw_fcount);
			return;
		}

		sensor_itf->laser_af_itf_ops.get_distance(sensor_itf, &af_type, cdaf_buf[PABLO_CRTA_LASER_AF_DATA].kva);
	}

	ret = CALL_ADT_MSG_OPS(hw->icpu_adt, send_msg_cstat_cdaf_end, instance,
		(void *)hw_ip, NULL, hw_fcount, 3, cdaf_buf);
	if (ret)
		mserr_hw("icpu_adt send_msg_cstat_cdaf_end fail", instance, hw_ip);
}

static int is_hw_cstat_cdaf_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount;
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info buf_info;
	struct is_sensor_interface *sensor_itf;
	struct vc_buf_info_t vc_buf_info;

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore cdaf callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore cdaf callback invalid HW state(0x%lx)", instance, hw_ip,
				fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);

	if (msg->rsp)
		mserr_hw("frame_end fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_process_buf, PABLO_CRTA_BUF_CDAF, fcount,
					&buf_info);
	if (!ret)
		CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], put_buf, &buf_info);

	sensor_itf = hw->sensor_itf[instance];
	if (!sensor_itf) {
		mserr_hw("[F%d] sensor_interface is NULL", instance, hw_ip, fcount);
		return -EINVAL;
	}

	ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1, &vc_buf_info);
	if (!ret)
		put_vc_dma_buf_by_fcount(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1, fcount);

	if(sensor_itf->cis_itf_ops.is_laser_af_available(sensor_itf)) {
		ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_process_buf, PABLO_CRTA_BUF_LAF, fcount,
						&buf_info);
		if (!ret)
			CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], put_buf, &buf_info);
	}

	return 0;
}

static int is_hw_cstat_isr2(u32 hw_id, void *data)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	u32 src, instance, hw_fcount;

	hw_ip = (struct is_hw_ip *)data;
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	atomic_inc(&hw->isr_run_count);

	src = cstat_hw_g_int2_status(hw_ip->regs[REG_SETA], true, hw->hw_fro_en);

	msdbg_hw(2, "[INT2][F%d] src 0x%x\n", instance, hw_ip, hw_fcount, src);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[INT2] Ignore ISR before HW config 0x%x\n",
				instance, hw_ip, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[INT2][F%d] invalid HW state 0x%lx src 0x%x", instance, hw_ip,
				hw_fcount, hw_ip->state, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag)) {
		mserr_hw("[INT2][F%d] during recovery. src 0x%x", instance, hw_ip,
				hw_fcount, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (cstat_hw_is_occurred(src, CSTAT_CDAF))
		is_hw_cstat_cdaf(hw_ip, instance);

	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	is_hw_cstat_check_dbg_info(hw_ip, 0, src, instance, hw_fcount);

	return 0;
}

static int is_hw_cstat_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_cstat *hw;
	u32 reg_cnt = cstat_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWCSTAT");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw = vzalloc(sizeof(struct is_hw_cstat));
	if (!hw) {
		mserr_hw("failed to alloc is_hw_cstat", instance, hw_ip);
		ret = -ENOMEM;
		goto err_hw_alloc;
	}

	hw_ip->priv_info = (void *)hw;
	hw->icpu_adt = pablo_get_icpu_adt();

	hw->iq_set.regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
	if (!hw->iq_set.regs) {
		mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_regs_alloc;
	}

	hw->cur_iq_set.regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
	if (!hw->cur_iq_set.regs) {
		mserr_hw("failed to alloc cur_iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_cur_regs_alloc;
	}

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);
	spin_lock_init(&hw->slock_shot);

	hw->hw_fro_en = false;
	hw->input = CSTST_INPUT_PATH_NUM;

	hw->is_sfr_dumped = false;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	atomic_set(&hw->isr_run_count, 0);
	init_waitqueue_head(&hw->isr_wait_queue);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_cur_regs_alloc:
	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;

err_regs_alloc:
	vfree(hw);
	hw_ip->priv_info = NULL;

err_hw_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_cstat_register_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw->icpu_adt;
	enum pablo_hic_cmd_id cmd_id;
	int ret;

	cmd_id = PABLO_HIC_CSTAT_FRAME_START;
	ret = CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			cmd_id, is_hw_cstat_frame_start_callback);
	if (ret)
		goto exit;

	cmd_id = PABLO_HIC_CSTAT_FRAME_END;
	ret = CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			cmd_id, is_hw_cstat_frame_end_callback);
	if (ret)
		goto exit;

	cmd_id = PABLO_HIC_CSTAT_CDAF_END;
	ret = CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			cmd_id, is_hw_cstat_cdaf_callback);
	if (ret)
		goto exit;

exit:
	if (ret)
		mserr_hw("icpu_adt register_response_msg_cb fail. cmd_id %d", instance, hw_ip, cmd_id);

	return ret;
}

static void is_hw_cstat_unregister_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw->icpu_adt;

	CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			PABLO_HIC_CSTAT_CDAF_END, NULL);
	CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			PABLO_HIC_CSTAT_FRAME_END, NULL);
	CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance,
			PABLO_HIC_CSTAT_FRAME_START, NULL);
}

static int is_hw_cstat_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret;
	struct is_hw_cstat *hw;
	u32 dma_id;

	hw_ip->frame_type = f_type;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("is_hw_cstat is null", instance, hw_ip);
		return -ENODATA;
	}

	hw->bufmgr[instance] = &is_get_is_core()->resourcemgr.crta_bufmgr[instance];

	for (dma_id = CSTAT_R_CL; dma_id < CSTAT_DMA_NUM; dma_id++) {
		if (cstat_hw_create_dma(hw_ip->regs[REG_SETA], dma_id,
				&hw->dma[dma_id])) {
			mserr_hw("[D%d] create_dma error", instance, hw_ip,
					dma_id);
			ret = -ENODATA;
			goto err_dma_create;
		}
	}

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_init_3aa(instance, flag);

	hw_ip->changed_hw_ip[instance] = NULL;

	ret = is_hw_cstat_register_icpu_msg_cb(hw_ip, instance);
	if (ret)
		goto err_register_icpu_msg_cb;

	if (hw_ip->group[instance] && hw_ip->group[instance]->device)
		hw->sensor_itf[instance] = is_sensor_get_sensor_interface(hw_ip->group[instance]->device->sensor);
	else
		hw->sensor_itf[instance] = NULL;

	set_bit(HW_INIT, &hw_ip->state);

	return 0;

err_register_icpu_msg_cb:
	is_hw_cstat_unregister_icpu_msg_cb(hw_ip, instance);

err_dma_create:
	return ret;
}

static int is_hw_cstat_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	is_hw_cstat_unregister_icpu_msg_cb(hw_ip, instance);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_deinit_3aa(instance);

	return 0;
}

static int is_hw_cstat_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw;
	struct is_hardware *hardware;
	struct exynos_platform_is *pdata;
	int i, max_num;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;
	vfree(hw->cur_iq_set.regs);
	hw->cur_iq_set.regs = NULL;

	vfree(hw);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.taa;

	/*
	 * For safe power off
	 * This is common register for all CSTAT channel.
	 * So, it should be set one time in final instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		struct is_hw_ip *hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip,
								DEV_HW_3AA0 + i);

		if (hw_ip_phys && test_bit(HW_OPEN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		if (cstat_hw_s_reset(hw_ip->regs[REG_EXT1]))
			mserr_hw("reset fail", instance, hw_ip);
	}
	mutex_unlock(&cmn_reg_lock);

	return 0;
}

/* Initialize the CSTAT core which is only valid for CTX0 */
static int is_hw_cstat_core_init(struct is_hw_ip *hw_ip, u32 instance)
{
	if (cstat_hw_s_reset(hw_ip->regs[REG_EXT1])) {
		mserr_hw("reset fail", instance, hw_ip);
		return -EINVAL;
	}

	cstat_hw_core_init(hw_ip->regs[REG_EXT1]);

	return 0;
}

/* Initialize each CSTAT context core */
static void is_hw_cstat_ctx_init(struct is_hw_ip *hw_ip)
{
	cstat_hw_ctx_init(hw_ip->regs[REG_SETA]);

	/* Put COREX in the disabled state */
	cstat_hw_s_corex_enable(hw_ip->regs[REG_SETA], false);
}

static void is_hw_cstat_s_global_enable(struct is_hw_ip *hw_ip, bool enable, bool fro_en)
{
	msinfo_hw("%s: enable(%d), fro_en(%d)\n", atomic_read(&hw_ip->instance), hw_ip,
			__func__, enable, fro_en);

	cstat_hw_s_global_enable(hw_ip->regs[REG_SETA], enable, fro_en);
}

static void is_hw_cstat_wait_isr_done(struct is_hw_ip *hw_ip)
{
	long timetowait;
	struct is_hw_cstat *hw;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	cstat_hw_wait_isr_clear(hw_ip->regs[REG_SETA], hw->hw_fro_en);

	timetowait = wait_event_timeout(hw->isr_wait_queue,
					!atomic_read(&hw->isr_run_count),
					IS_HW_STOP_TIMEOUT);
	if (!timetowait)
		mserr_hw("wait isr done timeout (%ld)", atomic_read(&hw_ip->instance),
							hw_ip, timetowait);
}

static void is_hw_cstat_s_rdma_byr(void *data, unsigned long id, struct is_frame *frame)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 instance;

	hw_ip = (struct is_hw_ip *)data;
	if (!hw_ip) {
		err("failed to get CSTAT hw_ip");
		return;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	instance = atomic_read(&hw_ip->instance);
	p_set = &hw->param_set[instance];

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_byr_in",
			hw_ip, frame, frame->cur_buf_index, frame->num_buffers,
			&p_set->dma_input.cmd,
			p_set->dma_input.plane,
			p_set->input_dva,
			frame->dvaddr_buffer);

	if (cstat_hw_s_rdma_cfg(&hw->dma[CSTAT_R_BYR], p_set, (hw_ip->num_buffers & 0xFFFF)))
		mserr_hw("failed to s_rdma_cfg", instance, hw_ip);
}

static const struct votf_ops cstat_votf_ops = {
	.s_addr		= is_hw_cstat_s_rdma_byr,
};

static int is_hw_cstat_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_hardware *hardware;
	struct exynos_platform_is *pdata;
	struct cstat_param *param;
	int i, max_num;

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		mswarn_hw("Not mapped. hw_map 0x%lx", instance, hw_ip, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	tasklet_init(&hw->end_tasklet, is_hw_cstat_end_tasklet, (unsigned long)hw_ip);
	hw->event_state = CSTAT_INIT;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	/* Prepare for VOTF */
	param = &hw_ip->region[instance]->parameter.cstat[0];
	if (param->dma_input.v_otf_enable) {
		struct is_group *group = hw_ip->group[instance];

		if (is_votf_register_framemgr(group, TRS, hw_ip,
					&cstat_votf_ops, group->leader.id)) {
			mserr_hw("Failed to is_votf_register_framemgr", instance, hw_ip);
			return -EINVAL;
		}
	}

	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.taa;

	/*
	 * This is common register for all CSTAT channel.
	 * So, it should be set one time in first instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		struct is_hw_ip *hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip,
								DEV_HW_3AA0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num)
		is_hw_cstat_core_init(hw_ip, instance);

	mutex_unlock(&cmn_reg_lock);

	hw->sensor_mode = hw_ip->group[instance]->device->sensor->cfg->special_mode;

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static void is_hw_cstat_cleanup_stat_buf(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 buf_id;

	for (buf_id = PABLO_CRTA_BUF_CDAF; buf_id < PABLO_CRTA_BUF_MAX; buf_id++)
		CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], flush_buf, buf_id, 0);
}

static int is_hw_cstat_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_cstat *hw;
	unsigned long flag;

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		mswarn_hw("Not mapped. hw_map 0x%lx", instance, hw_ip, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("%s: Vvalid(%d)\n", instance, hw_ip, __func__,
		atomic_read(&hw_ip->status.Vvalid));

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	/*
	 * Vvalid state is only needed to be checked when HW works at same instance.
	 * If HW works at different instance, skip Vvalid checking.
	 */
	if (atomic_read(&hw_ip->instance) == instance) {
		if (!hw->hw_fro_en) {
			timetowait = wait_event_timeout(hw_ip->status.wait_queue,
				!atomic_read(&hw_ip->status.Vvalid),
				IS_HW_STOP_TIMEOUT);

			if (!timetowait) {
				mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip,
						timetowait);
				ret = -ETIME;
			}
		}
		if (-ETIME != ret)
			is_hw_cstat_s_global_enable(hw_ip, false, false);
		is_hw_cstat_wait_isr_done(hw_ip);
		spin_lock_irqsave(&hw->slock_shot, flag);
		is_hw_cstat_cleanup_stat_buf(hw_ip, instance);
		memset(&hw->param_set[instance], 0x00, sizeof(hw->param_set[instance]));
		memset(&hw->config, 0x00, sizeof(hw->config));
		hw->cur_iq_set.size = 0;
		hw->event_state = CSTAT_INIT;
		hw->ignore_corex_delay = 0;
		clear_bit(HW_CONFIG, &hw_ip->state);
		spin_unlock_irqrestore(&hw->slock_shot, flag);
	}

	if (hw_ip->run_rsc_state)
		return ret;

	tasklet_kill(&hw->end_tasklet);

	clear_bit(HW_RUN, &hw_ip->state);

	return ret;
}

static void is_hw_cstat_update_param(struct is_hw_ip *hw_ip,
		struct is_param_region *p_region, struct cstat_param_set *p_set,
		IS_DECLARE_PMAP(pmap), u32 instance, u32 ctx)
{
	struct cstat_param *param;
	u32 pindex;

	param = &p_region->cstat[ctx];
	p_set->instance_id = instance;

	p_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	/* Sensor CFG */
	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&p_set->sensor_config, &p_region->sensor.config,
			sizeof(struct param_sensor_config));

	/* Input CFG */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_OTF_INPUT, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_DMA_INPUT, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));

	/* Output CFG */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_LME_DS0, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_lme_ds0, &param->lme_ds0,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_LME_DS1, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_lme_ds1, &param->lme_ds1,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_FDPIG, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_fdpig, &param->fdpig,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_DRC, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_drc, &param->drc,
			sizeof(struct param_dma_output));

#if defined(ENABLE_SAT_CAV)
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_SAT, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_sat, &param->sat,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_CAV, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_cav, &param->cav,
			sizeof(struct param_dma_output));
#else
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_CDS, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_cds, &param->cds,
			sizeof(struct param_dma_output));
#endif
}

static void is_hw_cstat_internal_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct cstat_param_set *p_set)
{
	u32 instance = atomic_read(&hw_ip->instance);

	hw_ip->internal_fcount[instance] = frame->fcount;

	/* Disable ALL WDMAs */
	p_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_lme_ds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_lme_ds1.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_fdpig.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#if defined(ENABLE_SAT_CAV)
	p_set->dma_output_sat.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_cav.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#else
	p_set->dma_output_cds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#endif
}

static void is_hw_cstat_external_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct cstat_param_set *p_set)
{
	struct is_param_region *p_region;
	u32 b_idx = frame->cur_buf_index;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 ctx_idx;

	FIMC_BUG_VOID(!frame->shot);

	hw_ip->internal_fcount[instance] = 0;

	/*
	 * Due to async shot,
	 * it should refer the param_region from the current frame,
	 * not from the device region.
	 */
	p_region = frame->parameter;

	/* For now, it only considers 2 contexts */
	if (hw_ip->frame_type == LIB_FRAME_HDR_SHORT)
		ctx_idx = 1;
	else
		ctx_idx = 0;

	is_hw_cstat_update_param(hw_ip, p_region, p_set, frame->pmap, instance, ctx_idx);

	/* DMA settings */
	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_byr_in",
			hw_ip, frame, b_idx, frame->num_buffers,
			&p_set->dma_input.cmd,
			p_set->dma_input.plane,
			p_set->input_dva,
			frame->dvaddr_buffer);

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_drc",
			hw_ip, frame, b_idx, frame->num_buffers,
			&p_set->dma_output_drc.cmd,
			p_set->dma_output_drc.plane,
			p_set->output_dva_drc,
			frame->txdgrTargetAddress[ctx_idx]);

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_lme_ds0",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_lme_ds0.cmd,
			p_set->dma_output_lme_ds0.plane,
			p_set->output_dva_lme_ds0,
			frame->txldsTargetAddress[ctx_idx]);

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_lme_ds1",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_lme_ds1.cmd,
			p_set->dma_output_lme_ds1.plane,
			p_set->output_dva_lme_ds1,
			frame->dva_cstat_lmeds1[ctx_idx]);

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_fdpig",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_fdpig.cmd,
			p_set->dma_output_fdpig.plane,
			p_set->output_dva_fdpig,
			frame->efdTargetAddress[ctx_idx]);

#if defined(ENABLE_SAT_CAV)
	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_sat",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_sat.cmd,
			p_set->dma_output_sat.plane,
			p_set->output_dva_sat,
			frame->dva_cstat_sat[ctx_idx]);

	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_cav",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_cav.cmd,
			p_set->dma_output_cav.plane,
			p_set->output_dva_cav,
			frame->dva_cstat_cav[ctx_idx]);
#else
	CALL_HW_OPS(hw_ip, dma_cfg, "cstat_cds",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_cds.cmd,
			p_set->dma_output_cds.plane,
			p_set->output_dva_cds,
			frame->txpTargetAddress[ctx_idx]);
#endif
}

static void is_hw_cstat_check_ignore_delay_corex(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 vvalid_time = 0, vblank_time = 0;

	if (hw->ignore_corex_delay)
		hw->ignore_corex_delay--;

	/* get sensor time and check seamless mode change */
	if (hw->sensor_itf[instance]) {
		hw->sensor_itf[instance]->cis_itf_ops.get_sensor_frame_timing(hw->sensor_itf[instance],
			&vvalid_time, &vblank_time);
		if (test_bit(HW_CONFIG, &hw_ip->state) && (vvalid_time != hw->vvalid_time)) {
			hw->ignore_corex_delay = IGNORE_COREX_DEALY;
			msinfo_hw("detect seamless modechange & disable corex delay", instance, hw_ip);
		}
	}

	/* store sensor time */
	hw->vvalid_time = vvalid_time;
	hw->vblank_time = vblank_time;
}

static void is_hw_cstat_g_delay_corex(struct is_hw_ip *hw_ip, u32 instance, u32 frame_rate,
		u32 *vvalid_time, u32 *corex_delay_time, u32 *cinrow_time, u32 *cinrow_ratio)
{
	struct is_hw_cstat *hw;
	u32 vblank_time, frame_time, req_vvalid;
	u32 req_min_frame_time = REQ_START_2_SHOT + REQ_SHOT_2_END + (REQ_END_2_START * DVFS_LEVEL_RATIO);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	*vvalid_time = hw->vvalid_time;
	vblank_time = hw->vblank_time;
	frame_time = 1000000U / frame_rate;
	vblank_time = frame_time - *vvalid_time;

	if (*vvalid_time && frame_time > req_min_frame_time) {
		if (*vvalid_time < (REQ_START_2_SHOT + REQ_SHOT_2_END)) {
			/* calculate corex delay time to get required v valid  */
			*corex_delay_time = (REQ_START_2_SHOT + REQ_SHOT_2_END) - *vvalid_time;
			/* limit corex delay time considering dvfs level change */
			if (*corex_delay_time > vblank_time / DVFS_LEVEL_RATIO)
				*corex_delay_time = vblank_time / DVFS_LEVEL_RATIO;
			req_vvalid = *vvalid_time + *corex_delay_time;
			/* 1. limit cinrow time over REQ_START_2_SHOT */
			/* 2. CRTA requires SHOT_2_END time */
			*cinrow_time = (req_vvalid > (REQ_START_2_SHOT + REQ_SHOT_2_END)) ?
					(req_vvalid - REQ_SHOT_2_END) : REQ_START_2_SHOT;
		} else {
			*corex_delay_time = 0;
			/* CRTA requires SHOT_2_END time */
			*cinrow_time = *vvalid_time - REQ_SHOT_2_END;
		}
		*cinrow_ratio = 0;
	} else {
		*corex_delay_time = 0;
		*cinrow_time = 0;
		*cinrow_ratio = 0;
	}

	/* for seamless mode change */
	if (hw->ignore_corex_delay == IGNORE_COREX_DEALY) {
		/* set previous corex_delay_time for current frame */
		*corex_delay_time = hw->corex_delay_time;
		/* set cinrow ratio for next frame */
		*cinrow_time = 0;
		*cinrow_ratio = REQ_CINROW_RATIO;
	} else if (hw->ignore_corex_delay > 1) {
		/* disable corex delay for current frame */
		*corex_delay_time = 0;
		/* set cinrow ratio for next frame */
		*cinrow_time = 0;
		*cinrow_ratio = REQ_CINROW_RATIO;
	} else if (hw->ignore_corex_delay == 1) {
		/* disable corex delay for current frame */
		*corex_delay_time = 0;
	}

	if (*cinrow_ratio == 0 && hw->sensor_mode == IS_SPECIAL_MODE_FASTAE)
		*cinrow_ratio = 30;

	hw->corex_delay_time = *corex_delay_time;

	if (!test_bit(HW_CONFIG, &hw_ip->state))
		msinfo_hw("vvalid(%d+%d->%d), cinrow(%dus/%d%%), vblank(%d->%d), frame(%d): ", instance, hw_ip,
			*vvalid_time, *corex_delay_time, (*vvalid_time + *corex_delay_time),
			*cinrow_time, *cinrow_ratio,
			vblank_time, (vblank_time - *corex_delay_time),
			frame_time);
}
/*
 * Set proper line interrupt timing for DDK
 *  The line interrupt timing should be in middle of FS & FE event.
 *  The FE means the final frame end interrupt which includes corex delay.
 * @base: CSTAT base address
 * @width: CSTAT input IMG width
 * @height: CSTAT input IMG height (== number of line)
 * @time_cfg: Sensor vvalid duration information
 * @en: Enable line interrupt
 */
static void is_hw_cstat_s_col_row(void __iomem *base,
		bool en, u32 width, u32 height, u32 vvalid_time, u32 cinrow_time, u32 cinrow_ratio)
{
	u32 nsec_per_line, row;

	if (!en) {
		cstat_hw_s_int_on_col_row(base, false, CSTAT_BCROP0, 0, 0);
		return;
	}

	if (cinrow_ratio) {
		row = height * cinrow_ratio / 100;
	} else if (cinrow_time) {
		nsec_per_line = (vvalid_time * 1000) / height; /* nsec */
		row = (cinrow_time * 1000) / nsec_per_line; /* lines */
	} else {
		row = height / 2;
	}

	/* row cannot exceed the CSTAT input IMG height */
	row = (row < height) ? row : (height - 1);

	dbg_hw(2, "[CSTAT]ratio:%d, time:%d -> s_col_row: %d@%dx%d\n",
		cinrow_ratio, cinrow_time, row, width, height);

	cstat_hw_s_int_on_col_row(base, true, CSTAT_BCROP0, 0, row);
}

static int is_hw_cstat_g_bitwidth(u32 bitwidth)
{
	switch (bitwidth) {
	case 10:
		return INPUT_10B;
	case 12:
		return INPUT_12B;
	case 14:
		return INPUT_14B;
	default:
		return -EINVAL;
	}
}

static int is_hw_cstat_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_param_set *p_set;
	struct param_otf_input *otf_in;
	struct param_dma_input *dma_in;
	struct cstat_simple_lic_cfg lic_cfg;
	enum cstat_lic_mode lic_mode = CSTAT_LIC_DYNAMIC;
	u32 ch, width, height;
	u32 pixel_order;
	enum cstat_input_bitwidth bitwidth;
	u32 seed;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	p_set = &hw->param_set[instance];

	otf_in = &p_set->otf_input;
	dma_in = &p_set->dma_input;
	ch = hw_ip->id - DEV_HW_3AA0;

	if (otf_in->cmd == OTF_INPUT_COMMAND_ENABLE) {
		hw->input = OTF;
		width = otf_in->width;
		height = otf_in->height;
		pixel_order = otf_in->order;
		bitwidth = is_hw_cstat_g_bitwidth(otf_in->bitwidth);
	} else if (dma_in->v_otf_enable == DMA_INPUT_VOTF_ENABLE) {
		hw->input = VOTF;
		width = dma_in->width;
		height = dma_in->height;
		pixel_order = dma_in->order;
		bitwidth = is_hw_cstat_g_bitwidth(dma_in->msb + 1);
	} else if (dma_in->cmd == DMA_INPUT_COMMAND_ENABLE) {
		hw->input = DMA;
		width = dma_in->width;
		height = dma_in->height;
		pixel_order = dma_in->order;
		bitwidth = is_hw_cstat_g_bitwidth(dma_in->msb + 1);
	} else {
		mserr_hw("No input", instance, hw_ip);
		return -EINVAL;
	}

	is_hw_cstat_ctx_init(hw_ip);

	cstat_hw_s_post_frame_gap(base, 0);
	cstat_hw_s_input(base, hw->input);
	cstat_hw_s_int_enable(base);
	cstat_hw_s_seqid(base, 0);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		cstat_hw_s_crc_seed(base, seed);

	lic_cfg.bypass = false;
	lic_cfg.input_path = hw->input;
	lic_cfg.input_bitwidth = bitwidth;
	lic_cfg.input_width = width;
	lic_cfg.input_height = height;

	cstat_hw_s_simple_lic_cfg(base, &lic_cfg);
	cstat_hw_s_pixel_order(base, pixel_order);

	if (cstat_hw_s_sram_offset(hw_ip->regs[REG_EXT1]))
		return -EINVAL;

	atomic_set(&hw->start_fcount, 0);

	msinfo_hw("init_config: %s_IN size %dx%d lic_mode %d\n",
			atomic_read(&hw_ip->instance), hw_ip,
			(hw->input == OTF) ? "OTF" : "DMA",
			width, height, lic_mode);

	return 0;
}

static void is_hw_cstat_s_stat_cfg(struct is_hw_ip *hw_ip, u32 instance,
		struct cstat_param_set *p_set)
{
	int ret;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct pablo_crta_buf_info buf_info = { 0, };
	struct param_dma_output *dma_out;
	u32 buf_id;

	for (buf_id = PABLO_CRTA_BUF_PRE_THUMB; buf_id < PABLO_CRTA_BUF_MAX; buf_id++) {
		ret = CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], get_free_buf, buf_id,
			p_set->fcount, true, &buf_info);

		if (ret || !buf_info.dva_cstat) {
			msdbg_hw(2, "Not enough frame(%d)", instance, hw_ip, buf_id);
			continue;
		}
		dma_out = cstat_hw_s_stat_cfg(buf_id - PABLO_CRTA_BUF_LAF, buf_info.dva_cstat, p_set);
		if (!dma_out)
			CALL_CRTA_BUFMGR_OPS(hw->bufmgr[instance], put_buf, &buf_info);
	}
}

static int is_hw_cstat_s_iq_regs(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = hw_ip->priv_info;
	void __iomem *base, *corex_base;
	struct is_hw_cstat_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size, i;
	bool configured = false;

	iq_set = &hw->iq_set;

	corex_base = hw_ip->regs[REG_SETA] + CSTAT_COREX_SDW_OFFSET;
	base = hw_ip->regs[REG_SETA];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;

		if (!test_bit(HW_CONFIG, &hw_ip->state) ||
		    hw->cur_iq_set.size != regs_size ||
		    memcmp(hw->cur_iq_set.regs, regs, sizeof(struct cr_set) * regs_size) != 0) {
			for (i = 0; i < regs_size; i++) {
				if (cstat_hw_is_corex_offset(regs[i].reg_addr))
					writel_relaxed(regs[i].reg_data, corex_base + regs[i].reg_addr);
				else
					writel_relaxed(regs[i].reg_data, base + regs[i].reg_addr);

				if (regs[i].reg_addr == 0) {
					mserr_hw("regs[%d].reg_addr (%d, %d) is not valid, regs_size(%d)\n",
							instance, hw_ip, i, regs[i].reg_addr, regs[i].reg_data,
							regs_size);
					panic("invalid cstat reg");
				}
			}
			memcpy(hw->cur_iq_set.regs, regs, sizeof(struct cr_set) * regs_size);
			hw->cur_iq_set.size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
				instance, hw_ip,
				atomic_read(&hw_ip->fcount),
				iq_set->fcount, iq_set->state, hw->cur_iq_set.fcount);
		return -EINVAL;
	}

	return 0;
}

static void is_hw_cstat_s_block_reg(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	msdbg_hw(4, "%s\n", p_set->instance_id, hw_ip, __func__);

	cstat_hw_s_default_blk_cfg(hw_ip->regs[REG_SETA]);
}

static int is_hw_cstat_s_size_regs(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set,
	struct is_frame *frame)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_size_cfg *size_cfg;
	struct is_crop in_crop, cur_crop;
	u32 full_w, full_h;
	enum cstat_input_bitwidth bitwidth;
	u32 crop_id;
	bool crop_en;
	struct cstat_grid_cfg grid_cfg;
	struct is_cstat_config *cfg;
	u32 start_pos_x, start_pos_y;
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;
	u32 binned_sensor_width, binned_sensor_height;
	u32 sensor_crop_x, sensor_crop_y, sensor_center_x, sensor_center_y;
	u32 prev_binning_x, prev_binning_y;
	bool mono_mode_en;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	size_cfg = &hw->size;
	cfg = &hw->config;
	mono_mode_en = p_set->mono_mode;

	if (hw->input == OTF) {
		in_crop.x = p_set->otf_input.bayer_crop_offset_x;
		in_crop.y = p_set->otf_input.bayer_crop_offset_y;
		in_crop.w = p_set->otf_input.bayer_crop_width;
		in_crop.h = p_set->otf_input.bayer_crop_height;
		full_w = p_set->otf_input.width;
		full_h = p_set->otf_input.height;
		bitwidth = is_hw_cstat_g_bitwidth(p_set->otf_input.bitwidth);
	} else {
		in_crop.x = p_set->dma_input.bayer_crop_offset_x;
		in_crop.y = p_set->dma_input.bayer_crop_offset_y;
		in_crop.w = p_set->dma_input.bayer_crop_width;
		in_crop.h = p_set->dma_input.bayer_crop_height;
		full_w = p_set->dma_input.width;
		full_h = p_set->dma_input.height;
		bitwidth = is_hw_cstat_g_bitwidth(p_set->dma_input.msb + 1);
	}

	if (bitwidth < 0) {
		mserr_hw("Invalid bitwidth %d", p_set->instance_id, hw_ip, bitwidth);
		return -EINVAL;
	}

	if (cstat_hw_s_format(base, full_w, full_h, bitwidth))
		return -EINVAL;

	cstat_hw_s_mono_mode(base, mono_mode_en);

	/* CROP configuration before BNS */
	cur_crop.w = full_w;
	cur_crop.h = full_h;
	for (crop_id = 0; crop_id < CSTAT_CROP_BNS; crop_id++) {
		if (cstat_hw_is_bcrop(crop_id)) {
			size_cfg->bcrop.x = cur_crop.x = in_crop.x;
			size_cfg->bcrop.y = cur_crop.y = in_crop.y;
			size_cfg->bcrop.w = cur_crop.w = in_crop.w;
			size_cfg->bcrop.h = cur_crop.h = in_crop.h;
			crop_en = true;
		} else {
			cur_crop.x = 0;
			cur_crop.y = 0;
			crop_en = false;
		}

		cstat_hw_s_crop(base, crop_id, crop_en, &cur_crop, p_set, NULL);
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		sensor_binning_ratio_x = p_set->sensor_config.sensor_binning_ratio_x;
		sensor_binning_ratio_y = p_set->sensor_config.sensor_binning_ratio_y;
	} else {
		sensor_binning_ratio_x = frame->shot->udm.frame_info.sensor_binning[0];
		sensor_binning_ratio_y = frame->shot->udm.frame_info.sensor_binning[1];
		p_set->sensor_config.sensor_binning_ratio_x = sensor_binning_ratio_x;
		p_set->sensor_config.sensor_binning_ratio_y = sensor_binning_ratio_y;
	}

	/* GRID configuration for CGRAS */
	/* Center */
	sensor_center_x = cfg->sensor_center_x;
	sensor_center_y = cfg->sensor_center_y;
	if (sensor_center_x == 0 || sensor_center_y == 0) {
		sensor_center_x = (p_set->sensor_config.calibrated_width >> 1);
		sensor_center_y = (p_set->sensor_config.calibrated_height >> 1);
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)",
				p_set->instance_id, hw_ip, __func__,
				sensor_center_x, sensor_center_y);
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	grid_cfg.binning_x = (1024ULL * sensor_binning_ratio_x *
			p_set->sensor_config.bns_binning_ratio_x) / 1000 / 1000;
	grid_cfg.binning_y = (1024ULL * sensor_binning_ratio_y *
			p_set->sensor_config.bns_binning_ratio_y) / 1000 / 1000;

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned_sensor_crop */
	if (p_set->sensor_config.freeform_sensor_crop_enable == 1) {
		sensor_crop_x = p_set->sensor_config.freeform_sensor_crop_offset_x;
		sensor_crop_y = p_set->sensor_config.freeform_sensor_crop_offset_y;
	} else {
		binned_sensor_width = p_set->sensor_config.calibrated_width * 1000 /
				sensor_binning_ratio_x;
		binned_sensor_height = p_set->sensor_config.calibrated_height * 1000 /
				sensor_binning_ratio_y;
		sensor_crop_x = ((binned_sensor_width - p_set->sensor_config.width) >> 1)
				& (~0x1);
		sensor_crop_y = ((binned_sensor_height - p_set->sensor_config.height) >> 1)
				& (~0x1);
	}

	start_pos_x = (sensor_crop_x * sensor_binning_ratio_x) / 1000;
	start_pos_y = (sensor_crop_y * sensor_binning_ratio_y) / 1000;

	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (sensor_center_y - start_pos_y));

	cstat_hw_s_grid_cfg(base, CSTAT_GRID_CGRAS, &grid_cfg);

	msdbg_hw(2, "%s:[CGR] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			p_set->sensor_config.calibrated_width,
			p_set->sensor_config.calibrated_height,
			sensor_center_x,
			sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[CGR] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);

	/* BNS configuration */
	cstat_hw_s_bns_cfg(base, &cur_crop, size_cfg);

	/* Total_binning = sensor_binning * csis_bns_binning * cstat_bns_binning */
	switch (size_cfg->bns_r) {
	case CSTAT_BNS_X1P0:
		size_cfg->bns_binning = 1000;
		break;
	case CSTAT_BNS_X1P25:
		size_cfg->bns_binning = 1250;
		break;
	case CSTAT_BNS_X1P5:
		size_cfg->bns_binning = 1500;
		break;
	case CSTAT_BNS_X1P75:
		size_cfg->bns_binning = 1750;
		break;
	case CSTAT_BNS_X2P0:
		size_cfg->bns_binning = 2000;
		break;
	default:
		mserr_hw("unidentified bns_r(%d) use CSTAT_BNS_X2P0", p_set->instance_id, hw_ip,
				size_cfg->bns_r);
		size_cfg->bns_binning = 2000;
		break;
	}

	/* CROP configuration after BNS */
	for (crop_id = CSTAT_CROP_BNS; crop_id < CSTAT_CROP_NUM; crop_id++) {
		cur_crop.x = 0;
		cur_crop.y = 0;
		crop_en = false;

		cstat_hw_s_crop(base, crop_id, crop_en, &cur_crop, p_set, size_cfg);
	}

	prev_binning_x = (1024ULL * sensor_binning_ratio_x *
			p_set->sensor_config.bns_binning_ratio_x) / 1000 / 1000;
	grid_cfg.binning_x = (prev_binning_x * size_cfg->bns_binning) / 1000;
	prev_binning_y = (1024ULL * sensor_binning_ratio_y *
			p_set->sensor_config.bns_binning_ratio_y) / 1000 / 1000;
	grid_cfg.binning_y = (prev_binning_y * size_cfg->bns_binning) / 1000;

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned(CSTAT_bcrop) + (unbinned)sensor_crop */
	/* Not use crop_bns. if it is used, please, include it above formula */
	if (p_set->sensor_config.freeform_sensor_crop_enable == 1) {
		sensor_crop_x = p_set->sensor_config.freeform_sensor_crop_offset_x;
		sensor_crop_y = p_set->sensor_config.freeform_sensor_crop_offset_y;
	} else {
		binned_sensor_width = p_set->sensor_config.calibrated_width * 1000 /
				sensor_binning_ratio_x;
		binned_sensor_height = p_set->sensor_config.calibrated_height * 1000 /
				sensor_binning_ratio_y;
		sensor_crop_x = ((binned_sensor_width - p_set->sensor_config.width) >> 1)
				& (~0x1);
		sensor_crop_y = ((binned_sensor_height - p_set->sensor_config.height) >> 1)
				& (~0x1);
	}

	start_pos_x = ((size_cfg->bcrop.x * prev_binning_x) / 1024) +
			(sensor_crop_x * sensor_binning_ratio_x) / 1000;
	start_pos_y = ((size_cfg->bcrop.y * prev_binning_y) / 1024) +
			(sensor_crop_y * sensor_binning_ratio_y) / 1000;
	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (sensor_center_y - start_pos_y));

	cstat_hw_s_grid_cfg(base, CSTAT_GRID_LSC, &grid_cfg);

	msdbg_hw(2, "%s:[LSC] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			p_set->sensor_config.calibrated_width,
			p_set->sensor_config.calibrated_height,
			sensor_center_x,
			sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[LSC] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);

	msdbg_hw(2, "%s:[CGR,LSC] bcrop: (%d,%d %dx%d), bns: (%dx%d, ratio:%d)\n",
			p_set->instance_id, hw_ip, __func__,
			size_cfg->bcrop.x, size_cfg->bcrop.y,
			size_cfg->bcrop.w, size_cfg->bcrop.h,
			cur_crop.w, cur_crop.h, size_cfg->bns_binning);

	return 0;
}

static int is_hw_cstat_s_ds(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_size_cfg *size_cfg;
	u32 dma_id;
	int ret = 0;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	size_cfg = &hw->size;

	for (dma_id = CSTAT_W_LMEDS0; dma_id < CSTAT_DMA_NUM; dma_id++)
		ret |= cstat_hw_s_ds_cfg(base, dma_id, size_cfg, p_set);

	FIMC_BUG(ret);

	return 0;
}

static int is_hw_cstat_s_dma(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 dma_id;
	int disable;

	/* RDMA cfg */
	for (dma_id = CSTAT_R_BYR; dma_id < CSTAT_RDMA_NUM; dma_id++) {
		if (cstat_hw_s_rdma_cfg(&hw->dma[dma_id], p_set, (hw_ip->num_buffers & 0xffff)))
			return -EINVAL;
	}

	/* WDMA cfg */
	for (dma_id = CSTAT_W_RGBYHIST; dma_id < CSTAT_DMA_NUM; dma_id++) {
		disable = test_bit(dma_id, &dbg_info.dma_id);
		if (cstat_hw_s_wdma_cfg(hw_ip->regs[REG_SETA], &hw->dma[dma_id],
					p_set, (hw_ip->num_buffers & 0xffff), disable))
			return -EINVAL;
	}

	return 0;
}

static void is_hw_cstat_s_fro(struct is_hw_ip *hw_ip, u32 instance)
{
	return cstat_hw_s_fro(hw_ip->regs[REG_SETA], (hw_ip->num_buffers & 0xffff), 1);
}

static int is_hw_cstat_trigger(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct param_sensor_config *sensor_cfg;
	struct cstat_time_cfg time_cfg;
	u32 cinrow_time, cinrow_ratio;

	switch (hw->input) {
	case OTF:
	case VOTF:
		/* Set COREX hw_trigger delay */
		is_hw_cstat_check_ignore_delay_corex(hw_ip, instance);

		sensor_cfg = &hw->param_set[instance].sensor_config;
		is_hw_cstat_g_delay_corex(hw_ip, instance, sensor_cfg->max_target_fps,
			&time_cfg.vvalid, &time_cfg.corex_delay, &cinrow_time, &cinrow_ratio);
		time_cfg.clk = is_dvfs_get_freq(IS_DVFS_CSIS) * MHZ;
		time_cfg.fro_en = hw->hw_fro_en;

		is_hw_cstat_s_col_row(base, true, hw->param_set[instance].otf_input.width,
			hw->param_set[instance].otf_input.height, time_cfg.vvalid, cinrow_time, cinrow_ratio);
		cstat_hw_s_corex_delay(base, &time_cfg);

		if (IS_ENABLED(USE_CSTAT_LIC_RECOVERY))
			cstat_hw_s_lic_hblank(base, &time_cfg);

		if (!test_bit(HW_CONFIG, &hw_ip->state)) {
			is_hw_cstat_s_fro(hw_ip, instance);
			/* SW trigger & enable hw before sensor on */
			is_hw_cstat_s_global_enable(hw_ip, true, hw->hw_fro_en);
		} else if (test_and_clear_bit(HW_SUSPEND, &hw_ip->state)) {
			/* Resume corex with latest CR setting */
			cstat_hw_corex_resume(base);
		} else {
			/* HW trigger after next configuration */
			cstat_hw_s_corex_type(base, COREX_COPY);
		}
		break;
	case DMA:
		is_hw_cstat_s_col_row(base, false, 0, 0, 0, 0, 0);
		return cstat_hw_s_one_shot_enable(base);
	default:
		mserr_hw("Not supported input %d", instance, hw_ip, hw->input);
		return -EINVAL;
	}

	return 0;
}

static int is_hw_cstat_set_config(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, void *conf)
{
	struct is_hw_cstat *hw;
	struct is_cstat_config *cstat_config = (struct is_cstat_config *)conf;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	msdbg_hw(2, "[F%d] set_config\n", instance, hw_ip, fcount);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (hw->config.sensor_center_x != cstat_config->sensor_center_x ||
	    hw->config.sensor_center_y != cstat_config->sensor_center_y)
		msinfo_hw("[F:%d] CSTAT cgras/luma_shading sensor_center(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->sensor_center_x,
				cstat_config->sensor_center_y);

	if (hw->config.lmeds_bypass != cstat_config->lmeds_bypass ||
	    hw->config.lmeds_w != cstat_config->lmeds_w ||
	    hw->config.lmeds_h != cstat_config->lmeds_h)
		msinfo_hw("[F:%d] CSTAT lmeds_bypass(%d), lmeds0(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->lmeds_bypass,
				cstat_config->lmeds_w, cstat_config->lmeds_h);

#if defined(ENABLE_LMEDS1)
	if (hw->config.lmeds1_w != cstat_config->lmeds1_w ||
	    hw->config.lmeds1_h != cstat_config->lmeds1_h)
		msinfo_hw("[F:%d] CSTAT lmeds1(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->lmeds1_w, cstat_config->lmeds1_h);
#endif

	if (hw->config.drcclct_bypass != cstat_config->drcclct_bypass ||
	    hw->config.drc_grid_w != cstat_config->drc_grid_w ||
	    hw->config.drc_grid_h != cstat_config->drc_grid_h)
		msinfo_hw("[F:%d] CSTAT drcclct_bypass(%d), drc_grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->drcclct_bypass,
				cstat_config->drc_grid_w, cstat_config->drc_grid_h);

	if (hw->config.thstatpre_bypass != cstat_config->thstatpre_bypass)
		msinfo_hw("[F:%d] CSTAT thstatpre bypass(%d), grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->thstatpre_bypass,
				cstat_config->pre_grid_w, cstat_config->pre_grid_h);

	if (hw->config.thstatawb_bypass != cstat_config->thstatawb_bypass)
		msinfo_hw("[F:%d] CSTAT thstatawb bypass(%d), grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->thstatawb_bypass,
				cstat_config->awb_grid_w, cstat_config->awb_grid_h);

	if (hw->config.thstatae_bypass != cstat_config->thstatae_bypass)
		msinfo_hw("[F:%d] CSTAT thstatae bypass(%d), grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->thstatae_bypass,
				cstat_config->ae_grid_w, cstat_config->ae_grid_h);

	if (hw->config.rgbyhist_bypass != cstat_config->rgbyhist_bypass)
		msinfo_hw("[F:%d] CSTAT rgbyhist bypass(%d), bin(%d), hist(%d)\n",
				instance, hw_ip, fcount,
				cstat_config->rgbyhist_bypass,
				cstat_config->rgby_bin_num, cstat_config->rgby_hist_num);

	if (hw->config.cdaf_bypass != cstat_config->cdaf_bypass)
		msinfo_hw("[F:%d] CSTAT cdaf bypass(%d)\n",
				instance, hw_ip, fcount,
				cstat_config->cdaf_bypass);

	if (hw->config.cdaf_mw_bypass != cstat_config->cdaf_mw_bypass)
		msinfo_hw("[F:%d] CSTAT cdafmw bypass(%d), grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->cdaf_mw_bypass,
				cstat_config->cdaf_mw_x, cstat_config->cdaf_mw_x);

	memcpy(&hw->config, conf, sizeof(hw->config));

	return 0;
}

static int _is_hw_cstat_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 b_idx, batch_num;
	ulong debug_iq = (unsigned long) is_get_debug_param(IS_DEBUG_PARAM_IQ);

	msdbgs_hw(2, "[F%d][T%d]shot\n", instance, hw_ip, frame->fcount, frame->type);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}
	
	if (!test_bit(instance, &hw_ip->run_rsc_state)) {
		mserr_hw("not run!!", instance, hw_ip);
		return -EINVAL;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];
	b_idx = frame->cur_buf_index;


	if (frame->type == SHOT_TYPE_INTERNAL)
		is_hw_cstat_internal_shot(hw_ip, frame, p_set);
	else
		is_hw_cstat_external_shot(hw_ip, frame, p_set);

	p_set->instance_id = instance;
	p_set->fcount = frame->fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	hw->hw_fro_en = (hw_ip->num_buffers & 0xffff) > 1 ? true : false;
	batch_num = hw_ip->framemgr->batch_num;

	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= b_idx << CURR_INDEX_SHIFT;
	}

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		ret = is_hw_cstat_init_config(hw_ip, instance, frame);
		if (ret) {
			mserr_hw("init_config fail", instance, hw_ip);
			return ret;
		}

		if (hw_ip->frame_type == LIB_FRAME_HDR_SHORT)
			is_hw_cstat_internal_shot(hw_ip, frame, p_set);
	}

	/* Shot */

	cstat_hw_s_corex_type(hw_ip->regs[REG_SETA], COREX_IGNORE);

	if (IS_ENABLED(CRTA_CALL)) {
		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			ret = is_hw_cstat_s_iq_regs(hw_ip, instance);
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret)
				mserr_hw("is_hw_cstat_s_iq_regs is fail", instance, hw_ip);
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}
	}

	if (unlikely(!hw->cur_iq_set.size)) {
		is_hw_cstat_s_block_reg(hw_ip, p_set);
		is_hw_cstat_set_config(hw_ip, 0, instance, frame->fcount, &conf_default);
	}

	/* update stat param */
	cstat_hw_s_dma_cfg(&hw->param_set[instance], &hw->config);
	is_hw_cstat_s_stat_cfg(hw_ip, instance, p_set);

	ret = is_hw_cstat_s_size_regs(hw_ip, p_set, frame);
	if (ret) {
		mserr_hw("s_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = is_hw_cstat_s_ds(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_ds fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = is_hw_cstat_s_dma(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_dma fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = is_hw_cstat_trigger(hw_ip, instance);
	if (ret) {
		mserr_hw("trigger fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	return ret;
}

static int is_hw_cstat_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	int ret;
	unsigned long flag;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;

	spin_lock_irqsave(&hw->slock_shot, flag);
	ret = _is_hw_cstat_shot(hw_ip, frame, hw_map);
	spin_unlock_irqrestore(&hw->slock_shot, flag);

	return ret;
}

static int is_hw_cstat_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 ctx_idx;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];

	/* For now, it only considers 2 contexts */
	if (hw_ip->frame_type == LIB_FRAME_HDR_SHORT)
		ctx_idx = 1;
	else
		ctx_idx = 0;

	is_hw_cstat_update_param(hw_ip, &region->parameter, p_set, pmap, instance, ctx_idx);

	return 0;
}

static int is_hw_cstat_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int ret = 0;

	if (done_type == IS_SHOT_TIMEOUT)
		is_hw_cstat_print_stall_status(hw_ip);

	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
				IS_HW_CORE_END, done_type, false);

	return ret;
}

static int is_hw_cstat_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	is_hw_cstat_ctx_init(hw_ip);

	return 0;
}

static int is_hw_cstat_change_chain(struct is_hw_ip *hw_ip, u32 instance,
	u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	struct is_hw_cstat *hw, *next_hw;
	u32 curr_id;
	u32 next_hw_id = DEV_HW_3AA0 + next_id;
	struct is_hw_ip *next_hw_ip;
	enum exynos_sensor_position sensor_position;

	curr_id = hw_ip->id - DEV_HW_3AA0;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip,
			curr_id, next_id);
		goto p_err;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("failed to get HW CSTAT", instance, hw_ip);
		return -ENODEV;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	next_hw = (struct is_hw_cstat *)next_hw_ip->priv_info;
	if (!next_hw) {
		mserr_hw("failed to get next HW CSTAT", instance, next_hw_ip);
		return -ENODEV;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = is_hw_cstat_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_cstat_disable is fail", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Copy instance information.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be referred at close time.
	 */
	next_hw->lib[instance] = hw->lib[instance];
	next_hw->param_set[instance] = hw->param_set[instance];
	next_hw->bufmgr[instance] = hw->bufmgr[instance];
	next_hw->prfi[instance] = hw->prfi[instance];
	next_hw->sensor_itf[instance] = hw->sensor_itf[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];

	sensor_position = hardware->sensor_position[instance];
	next_hw_ip->setfile[sensor_position] = hw_ip->setfile[sensor_position];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	if (test_and_set_bit(instance, &next_hw_ip->run_rsc_state))
		mswarn_hw("try to enable enabled instance", instance, next_hw_ip);

	ret = is_hw_cstat_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_cstat_enable is fail", instance, next_hw_ip);
		return -EINVAL;
	}

	/*
	 * There is no change about rsccount when change_chain processed
	 * because there is no open/close operation.
	 * But if it isn't increased, abnormal situation can be occurred
	 * according to hw close order among instances.
	 */
	if (!test_bit(hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_dec(&hw_ip->rsccount);
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip, atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip, atomic_read(&next_hw_ip->rsccount));
	}

	/* HACK: for DDK */
	hw_ip->changed_hw_ip[instance] = next_hw_ip;

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);
p_err:
	return ret;
}

static int is_hw_cstat_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_cstat *hw;
	struct is_hw_cstat_iq *iq_set;
	ulong flag = 0;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	iq_set = &hw->iq_set;

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	msdbg_hw(2, "[F%d]Store IQ regs set: %p, size(%d)\n", instance, hw_ip,
			fcount, regs, regs_size);

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, (sizeof(struct cr_set) * regs_size));

	set_bit(CR_SET_CONFIG, &iq_set->state);

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return 0;
}

int is_hw_cstat_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_cstat *hw_cstat = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		cstat_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = CSTAT_R_BYR; i < CSTAT_RDMA_NUM; i++) {
			hw_cstat = (struct is_hw_cstat *)hw_ip->priv_info;
			if (!hw_cstat)
				break;
			dma = &hw_cstat->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = CSTAT_W_RGBYHIST; i < CSTAT_DMA_NUM; i++) {
			hw_cstat = (struct is_hw_cstat *)hw_ip->priv_info;
			if (!hw_cstat)
				break;
			dma = &hw_cstat->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_cstat_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (!hw->is_sfr_dumped) {
		cstat_hw_dump(hw_ip->regs[REG_SETA]);
		hw->is_sfr_dumped = true;
	}

	return 0;
}

static void is_hw_cstat_suspend(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	int ret;

	if (!IS_ENABLED(CRTA_CALL))
		return;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		mswarn_hw("Not configured. Skip suspend.", instance, hw_ip);
		return;
	}

	msinfo_hw("suspend\n", instance, hw_ip);

	ret = CALL_ADT_MSG_OPS(hw->icpu_adt, send_msg_stop, instance, PABLO_STOP_SUSPEND);
	if (ret)
		mswarn_hw("icpu_adt send_msg_stop fail. ret %d", instance, hw_ip, ret);

	set_bit(HW_SUSPEND, &hw_ip->state);
}

static void cstat_hw_g_pcfi(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
				struct pablo_rta_frame_info *prfi)
{
	struct is_hw_cstat *hw;
	u32 ctx_idx;
	struct cstat_param *cstat_p;
	struct is_crop in_crop, bns_size;
	struct is_sensor_interface *sensor_itf;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	/* handle internal shot prfi */
	if (frame->type == SHOT_TYPE_INTERNAL) {
		memcpy(prfi, &hw->prfi[instance], sizeof(struct pablo_rta_frame_info));
		return;
	}

	/* For now, it only considers 2 contexts */
	if (hw_ip->frame_type == LIB_FRAME_HDR_SHORT)
		ctx_idx = 1;
	else
		ctx_idx = 0;

	cstat_p = &frame->parameter->cstat[ctx_idx];

	prfi->sensor_calibration_size.width = frame->shot->udm.frame_info.sensor_size[0];
	prfi->sensor_calibration_size.height = frame->shot->udm.frame_info.sensor_size[1];
	prfi->sensor_binning.x = frame->shot->udm.frame_info.sensor_binning[0];
	prfi->sensor_binning.y = frame->shot->udm.frame_info.sensor_binning[1];
	prfi->sensor_crop.offset.x = frame->shot->udm.frame_info.sensor_crop_region[0];
	prfi->sensor_crop.offset.y = frame->shot->udm.frame_info.sensor_crop_region[1];
	prfi->sensor_crop.size.width = frame->shot->udm.frame_info.sensor_crop_region[2];
	prfi->sensor_crop.size.height = frame->shot->udm.frame_info.sensor_crop_region[3];

	prfi->csis_bns_binning.x = frame->shot->udm.frame_info.bns_binning[0];
	prfi->csis_bns_binning.y = frame->shot->udm.frame_info.bns_binning[1];
	prfi->csis_mcb_binning.x = 1000;
	prfi->csis_mcb_binning.y = 1000;

	sensor_itf = hw->sensor_itf[instance];
	if (!sensor_itf) {
		mserr_hw("sensor_interface is NULL", instance, hw_ip);
		prfi->sensor_remosaic_crop_zoom_ratio = 0;
	} else {
		get_remosaic_zoom_ratio(sensor_itf, &prfi->sensor_remosaic_crop_zoom_ratio);
	}

	if (cstat_p->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		prfi->cstat_input_bayer_bit = cstat_p->otf_input.bitwidth;
		prfi->cstat_input_size.width = cstat_p->otf_input.width;
		prfi->cstat_input_size.height = cstat_p->otf_input.height;
	} else {
		prfi->cstat_input_bayer_bit = cstat_p->dma_input.msb + 1;
		prfi->cstat_input_size.width = cstat_p->dma_input.width;
		prfi->cstat_input_size.height = cstat_p->dma_input.height;
	}
	prfi->cstat_crop_in.offset.x = 0;
	prfi->cstat_crop_in.offset.y = 0;
	prfi->cstat_crop_in.size.width = prfi->cstat_input_size.width;
	prfi->cstat_crop_in.size.height = prfi->cstat_input_size.height;

	if (cstat_p->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		in_crop.x = cstat_p->otf_input.bayer_crop_offset_x;
		in_crop.y = cstat_p->otf_input.bayer_crop_offset_y;
		in_crop.w = cstat_p->otf_input.bayer_crop_width;
		in_crop.h = cstat_p->otf_input.bayer_crop_height;
	} else {
		in_crop.x = cstat_p->dma_input.bayer_crop_offset_x;
		in_crop.y = cstat_p->dma_input.bayer_crop_offset_y;
		in_crop.w = cstat_p->dma_input.bayer_crop_width;
		in_crop.h = cstat_p->dma_input.bayer_crop_height;
	}

	prfi->cstat_crop_dzoom.offset.x = in_crop.x;
	prfi->cstat_crop_dzoom.offset.y = in_crop.y;
	prfi->cstat_crop_dzoom.size.width = in_crop.w;
	prfi->cstat_crop_dzoom.size.height = in_crop.h;

	cstat_hw_g_bns_size(&in_crop, &bns_size);

	prfi->cstat_bns_size.width = bns_size.w;
	prfi->cstat_bns_size.height = bns_size.h;

	prfi->cstat_crop_bns.offset.x = 0;
	prfi->cstat_crop_bns.offset.y = 0;
	prfi->cstat_crop_bns.size.width = bns_size.w;
	prfi->cstat_crop_bns.size.height = bns_size.h;

	prfi->cstat_fdpig_crop.offset.x = cstat_p->fdpig.dma_crop_offset_x;
	prfi->cstat_fdpig_crop.offset.y = cstat_p->fdpig.dma_crop_offset_y;
	prfi->cstat_fdpig_crop.size.width = cstat_p->fdpig.dma_crop_width;
	prfi->cstat_fdpig_crop.size.height = cstat_p->fdpig.dma_crop_height;

	prfi->cstat_fdpig_ds_size.width = cstat_p->fdpig.width;
	prfi->cstat_fdpig_ds_size.height = cstat_p->fdpig.height;

	prfi->cstat_out_drcclct_buffer = cstat_p->drc.cmd;
	prfi->cstat_out_meds_buffer = cstat_p->lme_ds0.cmd;

	prfi->batch_num = frame->num_buffers;

	prfi->magic = PABLO_CRTA_MAGIC_NUMBER;

	memcpy(&hw->prfi[instance], prfi, sizeof(struct pablo_rta_frame_info));

	msdbg_hw(2, "[F%d] cstat_out_drcclct_buffer: %d, cstat_out_meds_buffer: %d\n",
			instance, hw_ip, frame->fcount,
			prfi->cstat_out_drcclct_buffer, prfi->cstat_out_meds_buffer);
}

static void pablo_hw_cstat_query(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out)
{
	switch (type) {
	case PABLO_QUERY_GET_PCFI:
		cstat_hw_g_pcfi(ip, instance, (struct is_frame *)in, (struct pablo_rta_frame_info *)out);
		break;
	default:
		break;
	}
}

const static struct is_hw_ip_ops is_hw_cstat_ops = {
	.open			= is_hw_cstat_open,
	.init			= is_hw_cstat_init,
	.deinit			= is_hw_cstat_deinit,
	.close			= is_hw_cstat_close,
	.enable			= is_hw_cstat_enable,
	.disable		= is_hw_cstat_disable,
	.shot			= is_hw_cstat_shot,
	.set_param		= is_hw_cstat_set_param,
	.frame_ndone		= is_hw_cstat_frame_ndone,
	.restore		= is_hw_cstat_restore,
	.change_chain		= is_hw_cstat_change_chain,
	.set_regs		= is_hw_cstat_set_regs,
	.dump_regs		= is_hw_cstat_dump_regs,
	.set_config		= is_hw_cstat_set_config,
	.notify_timeout		= is_hw_cstat_notify_timeout,
	.suspend		= is_hw_cstat_suspend,
	.query			= pablo_hw_cstat_query,
};

int is_hw_cstat_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	/* initialize device hardware */
	hw_ip->ops = &is_hw_cstat_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_cstat_isr1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_cstat_isr2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
