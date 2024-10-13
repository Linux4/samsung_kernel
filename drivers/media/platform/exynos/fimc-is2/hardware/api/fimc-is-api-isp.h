/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_API_ISP_H
#define FIMC_IS_API_ISP_H

#include "fimc-is-core.h"
#include "fimc-is-api-common.h"

#define CHAIN_ID_MASK		(0x0000000F)
#define CHAIN_ID_SHIFT		(0)
#define INSTANCE_ID_MASK	(0x000000F0)
#define INSTANCE_ID_SHIFT	(4)
#define REPROCESSING_FLAG_MASK	(0x00000F00)
#define REPROCESSING_FLAG_SHIFT	(8)
#define INPUT_TYPE_MASK		(0x0000F000)
#define INPUT_TYPE_SHIFT	(12)
#define MODULE_ID_MASK		(0x0FFF0000)
#define MODULE_ID_SHIFT		(16)
#define DMA_INPUT_ID_MASK	(0xFF000000)
#define DMA_INPUT_ID_SHIFT	(24)

#define CONVRES(src, src_max, tar_max) \
	((src <= 0) ? (0) : ((src * tar_max + (src_max >> 1)) / src_max))

struct fimc_is_lib_isp {
	void				*object;	/* 3aa_object or isp_object */
	struct lib_interface_func	*func;		/* lib_interface_func */
};

enum dma_type {
	DMA_TYPE_SDMA   = 1,
	DMA_TYPE_VDMA1  = 2,
	DMA_TYPE_VDMA2  = 3,
	DMA_TYPE_VDMA3  = 4,
	DMA_TYPE_VDMA4  = 5,
	DMA_TYPE_VDMA5  = 6,
	DMA_TYPE_END
};

enum lib_isp_callback_event_type {
	LIB_ISP_EVENT_CONFIG_LOCK	= 1,
	LIB_ISP_EVENT_FRAME_START	= 2,
	LIB_ISP_EVENT_FRAME_END		= 3,
	LIB_ISP_EVENT_DMA_A_OUT_DONE	= 4,
	LIB_ISP_EVENT_DMA_B_OUT_DONE	= 5,
	LIB_ISP_EVENT_FRAME_START_ISR	= 6,
	LIB_ISP_EVENT_END
};

struct taa_param_set {
	struct param_sensor_config	sensor_config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;			/* ParameterVDma1Input */
	struct param_dma_input		ddma_input;			/* ParameterDDmaInput */
	struct param_otf_output		otf_output;
	struct param_dma_output		vdma4_output;			/* ParameterVDma4Output, before_bds */
	struct param_dma_output		vdma2_output;			/* ParameterVDma2Output, after_bds */
	struct param_dma_output		ddma_output;			/* ParameterDDmaOutput */
	unsigned int			input_buffer_addr;		/* VDMA1 */
	unsigned int			output_buffer_addr_vdma4;	/* VDMA4 */
	unsigned int			output_buffer_addr_vdma2;	/* VDMA2 */
	unsigned int			instance_id;
	unsigned int			fcount;
	bool				reprocessing;
};

struct isp_param_set {
	struct taa_param_set		*taa_param;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;			/* ParameterVDma1Input */
	struct param_dma_input		vdma3_input;			/* ParameterVDma3Input */
	struct param_otf_output		otf_output;
	struct param_dma_output		vdma4_output;			/* ParameterVdma4Output */
	struct param_dma_output		vdma5_output;			/* ParameterVdma5Output */
	unsigned int			input_buffer_addr;		/* VDMA1 */
	unsigned int			output_buffer_addr_vdma4;	/* VDMA4 */
	unsigned int			output_buffer_addr_vdma5;	/* VDMA5 */
	unsigned int			instance_id;
	unsigned int			fcount;
	bool				reprocessing;
};

struct lib_callback_func {
	/* for camera callback */
	void (*camera_callback)(void *hw_ip,
		enum lib_isp_callback_event_type event_id,
		u32 instance_id,
		void *data);

	/* for io callback */
	void (*io_callback)(void *hw_ip,
		enum lib_isp_callback_event_type event_id,
		u32 instance_id);
};

struct lib_tune_set {
	u32 index;
	u32 addr;
	u32 size;
	int decrypt_flag;
};

struct cal_info {
	u32 data[16];
};

#define LIB_ISP_ADDR		(LIB_ISP_BASE_ADDR + LIB_ISP_OFFSET)
enum lib_func_type {
	LIB_FUNC_3AA = 1,
	LIB_FUNC_ISP,
	LIB_FUNC_TPU,
	LIB_FUNC_VRA,
	LIB_FUNC_TYPE_END
};

typedef unsigned int (*register_lib_isp)(u32 type, void **lib_func);
static const register_lib_isp get_lib_func = (register_lib_isp)LIB_ISP_ADDR;

struct lib_interface_func {
	int (*chain_create)(unsigned int chain_id, unsigned int addr, unsigned int offset, const struct lib_callback_func *cb);
	int (*object_create)(void **pobject, unsigned int obj_info, void *hw);
	int (*chain_destroy)(unsigned int chain_id);
	int (*object_destroy)(void *object, unsigned int sensor_id);
	int (*stop)(void *object, unsigned int instance_id);	/* suspend */
	int (*reset)(unsigned int addr);
	int (*set_param)(void *object, void *param_set);
	int (*set_ctrl)(void *object, unsigned int instance, unsigned frame, struct camera2_shot *shot);
	int (*shot)(void *object, void *param_set, struct camera2_shot *shot);	/* commit & activate */
	int (*get_meta)(void *object, unsigned int instance, unsigned frame, struct camera2_shot *shot);
	int (*create_tune_set)(void *isp_object, unsigned int instance, struct lib_tune_set *set);
	int (*apply_tune_set)(void *isp_object, unsigned int instance_id, unsigned int index);
	int (*delete_tune_set)(void *isp_object, unsigned int instance_id, unsigned int index);
	int (*set_parameter_flash)(void *object, struct param_isp_flash *p_flash);
	int (*set_parameter_af)(void *isp_object, struct param_isp_aa *param_aa,
		unsigned int frame_num, unsigned int instance);
	int (*load_cal_data)(void *isp_object, unsigned int instance_id, unsigned int addr);
	int (*get_cal_data)(void *isp_object, unsigned int instance_id, struct cal_info *data, int type);
	int (*sensor_info_mode_chg)(void *isp_object, unsigned int instance_id, struct camera2_shot *shot);
	int (*sensor_update_ctl)(void *isp_object, unsigned int instance_id, unsigned int frame_count, struct camera2_shot *shot);
};

int fimc_is_lib_isp_chain_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id);
int fimc_is_lib_isp_object_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id);
void fimc_is_lib_isp_chain_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id);
void fimc_is_lib_isp_object_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id);
int fimc_is_lib_isp_set_param(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	void *param);
int fimc_is_lib_isp_set_ctrl(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	struct fimc_is_frame *frame);
void fimc_is_lib_isp_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	void *param_set,
	struct camera2_shot *shot);
int fimc_is_lib_isp_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	struct fimc_is_frame *frame);
void fimc_is_lib_isp_stop(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id);
int fimc_is_lib_isp_create_tune_set(struct fimc_is_lib_isp *this,
	u32 addr, u32 size, u32 index, int flag, unsigned int instance_id);
int fimc_is_lib_isp_apply_tune_set(struct fimc_is_lib_isp *this,
	unsigned int index, unsigned int instance_id);
int fimc_is_lib_isp_delete_tune_set(struct fimc_is_lib_isp *this,
	unsigned int instance_id, unsigned int addr);
int fimc_is_lib_isp_load_cal_data(struct fimc_is_lib_isp *this,
	unsigned int index, unsigned int instance_id);
int fimc_is_lib_isp_get_cal_data(struct fimc_is_lib_isp *this,
	unsigned int instance_id, struct cal_info *data, int type);
int fimc_is_lib_isp_sensor_info_mode_chg(struct fimc_is_lib_isp *this,
	unsigned int instance_id, struct camera2_shot *shot);
int fimc_is_lib_isp_sensor_update_control(struct fimc_is_lib_isp *this,
	unsigned int instance_id, u32 frame_count, struct camera2_shot *shot);
int fimc_is_lib_isp_convert_face_map(struct fimc_is_hardware *hardware,
	struct taa_param_set *param_set, struct fimc_is_frame *frame);
void fimc_is_lib_isp_configure_algorithm(void);
void fimc_is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height);
#endif
