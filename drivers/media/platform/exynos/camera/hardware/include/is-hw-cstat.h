/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef IS_HW_CSTAT_H
#define IS_HW_CSTAT_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-hw-api-cstat.h"
#include "pablo-internal-subdev-ctrl.h"

struct is_hw_cstat_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_hw_cstat_stat {
	struct cstat_pre pre;
	struct cstat_awb awb;
	struct cstat_ae ae;
	struct cstat_rgby rgby;
};

struct is_hw_cstat {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct cstat_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		dma[CSTAT_DMA_NUM];
	struct is_cstat_config		config;
	ulong				irq_state[CSTAT_INT_NUM];
	unsigned long			event_state;
	bool				hw_fro_en;

	bool				is_sfr_dumped;

	struct is_hw_cstat_iq		iq_set;
	struct is_hw_cstat_iq		cur_iq_set;
	struct is_hw_cstat_stat		stat[CSAT_STAT_BUF_NUM];

	enum cstat_input_path		input;
	struct cstat_size_cfg		size;

	struct tasklet_struct		end_tasklet;
	atomic_t			isr_run_count;
	wait_queue_head_t		isr_wait_queue;
	spinlock_t			slock_shot;

	struct pablo_internal_subdev	subdev[IS_CSTAT_SUBDEV_NUM];
	/* icpu */
	struct pablo_icpu_adt		*icpu_adt;
	struct pablo_crta_bufmgr	*bufmgr[IS_STREAM_COUNT];
	struct pablo_rta_frame_info 	prfi[IS_STREAM_COUNT];

	/* for frame count management */
	atomic_t			start_fcount;

	/* for corex delay */
	/* sensor itf */
	struct is_sensor_interface	*sensor_itf[IS_STREAM_COUNT];
	/* store sensor vvalid time */
	u32				vvalid_time;
	/* store sensor vvalid time */
	u32				vblank_time;
	/* store corex delay time */
	u32				corex_delay_time;
	/* ignore corex delay for seamless mode change*/
	u32				ignore_corex_delay;
	int				sensor_mode;
};

#endif
