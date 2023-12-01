// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SELF_TEST_HW_IP_H
#define PABLO_SELF_TEST_HW_IP_H

#include "pablo-framemgr.h"
#include "is-hw.h"

#define PST_WAIT_TIME_START_ISR 3000

struct frame_param {
	u32 num_buffers;
	u32 reserved[PARAMETER_MAX_MEMBER-1];
};

struct param_size_byrp2yuvp {
	u32 x_start;
	u32 y_start;
	u32 w_start;
	u32 h_start;

	u32 x_byrp;
	u32 y_byrp;
	u32 w_byrp;
	u32 h_byrp;

	u32 x_rgbp;
	u32 y_rgbp;
	u32 w_rgbp;
	u32 h_rgbp;

	u32 x_mcfp;
	u32 y_mcfp;
	u32 w_mcfp;
	u32 h_mcfp;

	u32 x_yuvp;
	u32 y_yuvp;
	u32 w_yuvp;
	u32 h_yuvp;

	u32 reserved[PARAMETER_MAX_MEMBER-20];
};

struct param_size_mcsc {
	u32 out0_crop_x;
	u32 out0_crop_y;
	u32 out0_crop_w;
	u32 out0_crop_h;
	u32 out0_w;
	u32 out0_h;

	u32 out1_crop_x;
	u32 out1_crop_y;
	u32 out1_crop_w;
	u32 out1_crop_h;
	u32 out1_w;
	u32 out1_h;

	u32 out2_crop_x;
	u32 out2_crop_y;
	u32 out2_crop_w;
	u32 out2_crop_h;
	u32 out2_w;
	u32 out2_h;

	u32 out3_crop_x;
	u32 out3_crop_y;
	u32 out3_crop_w;
	u32 out3_crop_h;
	u32 out3_w;
	u32 out3_h;

	u32 out4_crop_x;
	u32 out4_crop_y;
	u32 out4_crop_w;
	u32 out4_crop_h;
	u32 out4_w;
	u32 out4_h;

	u32 reserved[PARAMETER_MAX_MEMBER-30];
};

struct b2m_param {
	struct param_size_byrp2yuvp sz_byrp2yuvp;
	struct param_size_mcsc sz_mcsc;
};

enum pst_hw_ip_state_act {
	PST_HW_IP_STOP,
	PST_HW_IP_START,
	PST_HW_IP_PRESET,
	PST_HW_IP_UPDATE_PARAM,
	PST_HW_IP_UPDATE_FRAME,
	PST_HW_IP_UPDATE_VECTOR,
};

enum pst_hw_ip_set {
	PST_SET_ACT,
	PST_SET_TC_IDX,
};

#define PST_UPDATE_STRIDE 3
enum pst_hw_ip_update_param {
	PST_UPDATE_PARAM_NUM = 1,
	PST_UPDATE_PARAM_IDX,
	PST_UPDATE_SUBPARAM_IDX,
	PST_UPDATE_VALUE,
	PST_UPDATE_MAX,
};

enum pst_hw_ip_type {
	PST_HW_IP_SINGLE,
	PST_HW_IP_GROUP,
};

#define CALL_PST_CB(cb, op, args...)	\
	(((cb) && (cb)->op) ? ((cb)->op(args)) : 0)

struct pst_callback_ops {
	void (*init_param)(unsigned int index, enum pst_hw_ip_type type);
	void (*set_param)(struct is_frame *frame);
	void (*clr_param)(struct is_frame *frame);
	void (*set_rta_info)(struct is_frame *frame, struct size_cr_set *cr_set);
	void (*set_size)(void *in_param, void *out_param);
};

int pst_set_hw_ip(const char *val,
		enum is_hardware_id hw_id,
		struct is_frame *frame,
		u32 param[][PARAMETER_MAX_MEMBER],
		struct size_cr_set *cr_set,
		size_t preset_size,
		unsigned long *result,
		const struct pst_callback_ops *pst_cb);
int pst_get_hw_ip(char *buffer,
		const char *name,
		const size_t preset_size,
		const unsigned long *result);

void *pst_get_param(struct is_frame *frame, u32 param_idx);
void pst_set_param(struct is_frame *frame, u32 *src_params, u32 param_idx);
int pst_cmp_param(struct is_frame *frame, void *param_dst, u32 param_idx);
struct is_priv_buf *pst_set_dva(struct is_frame *frame, dma_addr_t *dva, size_t *size, u32 vid);
void pst_clr_dva(struct is_priv_buf *pb);
void pst_get_size_of_dma_input(void *param, u32 align, u32 block_w, u32 block_h, size_t *size);
void pst_get_size_of_dma_output(void *param, u32 align, size_t *size);
void pst_get_size_of_mcs_output(void *param, u32 align, size_t *size);
void pst_get_size_of_lme_input(void *param, u32 align, size_t *size);

const struct pst_callback_ops *pst_get_hw_byrp_cb(void);
const struct pst_callback_ops *pst_get_hw_rgbp_cb(void);
const struct pst_callback_ops *pst_get_hw_mcfp_cb(void);
const struct pst_callback_ops *pst_get_hw_dlfe_cb(void);
const struct pst_callback_ops *pst_get_hw_yuvp_cb(void);
const struct pst_callback_ops *pst_get_hw_shrp_cb(void);
const struct pst_callback_ops *pst_get_hw_mcsc_cb(void);
#endif
