/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_ADAPTER_H
#define PABLO_ICPU_ADAPTER_H

#include <linux/types.h>
#include "pablo-crta-cmd-interface.h"

struct is_sensor_interface;
struct pablo_crta_bufmgr;
struct pablo_crta_buf_info;

enum pablo_crta_buf_cdaf_data_type {
	PABLO_CRTA_CDAF_RAW = 0,
	PABLO_CRTA_PDAF_TAIL,
	PABLO_CRTA_LASER_AF_DATA,
	PABLO_CRTA_CDAF_DATA_MAX,
};

enum pablo_crta_buf_cstat_end_data_type {
	PABLO_CRTA_CSTAT_END_PRE_THUMB = 0,
	PABLO_CRTA_CSTAT_END_AE_THUMB,
	PABLO_CRTA_CSTAT_END_AWB_THUMB,
	PABLO_CRTA_CSTAT_END_RGBY_HIST,
	PABLO_CRTA_CSTAT_END_CDAF_MW,
	PABLO_CRTA_CSTAT_END_VPDAF,
	PABLO_CRTA_CSTAT_END_MAX
};

struct pablo_icpu_adt {
	void					*priv;
	atomic_t				rsccount;

	const struct pablo_icpu_adt_ops		*ops;
	const struct pablo_icpu_adt_msg_ops	*msg_ops;
};

#define CALL_ADT_OPS(adt, op, args...)	\
	((adt && (adt)->ops && (adt)->ops->op) ? ((adt)->ops->op(adt, args)) : 0)

struct pablo_icpu_adt_ops {
	int (*open)(struct pablo_icpu_adt *icpu_adt, u32 instance,
		    struct is_sensor_interface *sensor_itf, struct pablo_crta_bufmgr *bufmgr);
	int (*close)(struct pablo_icpu_adt *icpu_adt, u32 instance);
};

/**
 * struct pablo_icpu_adt_rsp_msg - msg returned from icpu
 * @rsp: response value from icpu's msg
 * @instance: instance from icpu's msg
 * @fcount: instance from icpu's msg
 *
 * It is used in pablo_response_msg_cb
 */
struct pablo_icpu_adt_rsp_msg {
	int rsp;
	u32 instance;
	u32 fcount;
};

/**
 * typedef pablo_response_msg_cb - This is the required signature for msg callback method
 * @caller: caller that is supplied when the send_msg method is called.
 * @ctx: user data that is supplied when the send_msg method is called.
 * @msg_data: additional information. The contents of this parameter are dependent on the msg
 *            It generally indicates struct pablo_icpu_adt_rsp_msg.
 *
 * Return:
 * 0 - OK
 * otherwise - FAIL
 */
typedef int (*pablo_response_msg_cb)(void *caller, void *ctx, void *rsp_msg);

#define CALL_ADT_MSG_OPS(adt, op, args...)	\
	((adt && (adt)->msg_ops && (adt)->msg_ops->op) ? ((adt)->msg_ops->op(adt, args)) : 0)

struct pablo_icpu_adt_msg_ops {
	int (*register_response_msg_cb)(struct pablo_icpu_adt *icpu_adt, u32 instance,
					enum pablo_hic_cmd_id msg,
					pablo_response_msg_cb cb);
	int (*send_msg_open)(struct pablo_icpu_adt *icpu_adt, u32 instance,
			u32 hw_id, u32 rep_flag, u32 position, u32 f_type);
	int (*send_msg_put_buf)(struct pablo_icpu_adt *icpu_adt, u32 instance,
				enum pablo_buffer_type type,
				struct pablo_crta_buf_info *buf);
	int (*send_msg_set_shared_buf_idx)(struct pablo_icpu_adt *icpu_adt, u32 instance,
					   enum pablo_buffer_type type,
					   u32 fcount);
	int (*send_msg_start)(struct pablo_icpu_adt *icpu_adt, u32 instance,
			      struct pablo_crta_buf_info *pcsi);
	int (*send_msg_shot)(struct pablo_icpu_adt *icpu_adt, u32 instance,
			     void *cb_user, void *cb_ctx, u32 fcount,
			     struct pablo_crta_buf_info *shot, struct pablo_crta_buf_info *pcfi_buf);
	int (*send_msg_cstat_frame_start)(struct pablo_icpu_adt *icpu_adt, u32 instance,
					  void *cb_user, void *cb_ctx,
					  u32 fcount, struct pablo_crta_buf_info *pcsi_buf);
	int (*send_msg_cstat_cdaf_end)(struct pablo_icpu_adt *icpu_adt, u32 instance,
				       void *cb_user, void *cb_ctx,
				       u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf);
	int (*send_msg_pdp_stat0_end)(struct pablo_icpu_adt *icpu_adt, u32 instance,
				      void *cb_user, void *cb_ctx,
				      u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf);
	int (*send_msg_pdp_stat1_end)(struct pablo_icpu_adt *icpu_adt, u32 instance,
				      void *cb_user, void *cb_ctx,
				      u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf);
	int (*send_msg_cstat_frame_end)(struct pablo_icpu_adt *icpu_adt, u32 instance,
					void *cb_user, void *cb_ctx,
					u32 fcount, struct pablo_crta_buf_info *shot_buf, u32 stat_num,
					struct pablo_crta_buf_info *stat_buf, u32 edge_score);
	int (*send_msg_stop)(struct pablo_icpu_adt *icpu_adt, u32 instance, u32 suspend);
	int (*send_msg_close)(struct pablo_icpu_adt *icpu_adt, u32 instance);
};

#if IS_ENABLED(CONFIG_PABLO_ICPU_ADT_V2)
struct pablo_icpu_adt *pablo_get_icpu_adt(void);
void pablo_set_icpu_adt(struct pablo_icpu_adt *icpu_adt);
int pablo_icpu_adt_probe(void);
int pablo_icpu_adt_remove(void);
#else
#define pablo_get_icpu_adt(void)	({ NULL; })
#define pablo_set_icpu_adt(icpu_adt)
#define pablo_icpu_adt_probe(void)	({ 0; })
#define pablo_icpu_adt_remove(void)	({ 0; })
#endif

#endif /* PABLO_ICPU_ADAPTER_H */
