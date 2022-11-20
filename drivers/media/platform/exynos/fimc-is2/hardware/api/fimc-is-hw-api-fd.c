/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/delay.h>
#include "fimc-is-hw-api-fd.h"
#include "sfr/fimc-is-sfr-fd-v111.h"

u32 fimc_is_fd_get_version(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_reg(base_addr, &fd_regs[FD_R_VERSION]);
}

u32 fimc_is_fd_get_intr_status(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_reg(base_addr, &fd_regs[FD_R_INTSTATUS]);
}

u32 fimc_is_fd_get_intr_mask(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_reg(base_addr, &fd_regs[FD_R_INTMASK]);
}

u32 fimc_is_fd_get_buffer_status(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_reg(base_addr, &fd_regs[FD_R_STATUS]);
}

u32 fimc_is_fd_get_input_width(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_WIDTH], &fd_fields[FD_F_WIDTH]);
}

u32 fimc_is_fd_get_input_height(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_HEIGHT], &fd_fields[FD_F_HEIGHT]);
}

u32 fimc_is_fd_get_input_start_x(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_START_X], &fd_fields[FD_F_STARTX]);
}

u32 fimc_is_fd_get_input_start_y(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_START_Y], &fd_fields[FD_F_STARTY]);
}

u32 fimc_is_fd_get_down_sampling_x(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_DS_SCALE_X], &fd_fields[FD_F_DSSCALEX]);
}

u32 fimc_is_fd_get_down_sampling_y(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_DS_SCALE_Y], &fd_fields[FD_F_DSSCALEY]);
}

u32 fimc_is_fd_get_output_width(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_OUTPUT_WIDTH], &fd_fields[FD_F_OUTPUTWIDTH]);
}

u32 fimc_is_fd_get_output_height(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_OUTPUT_HEIGHT], &fd_fields[FD_F_OUTPUTHEIGHT]);
}

u32 fimc_is_fd_get_output_size(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_OUTPUT_SIZE], &fd_fields[FD_F_OUTPUTSIZE]);
}

u32 fimc_is_fd_get_ycc_format(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_RESULT_SAT], &fd_fields[FD_F_SAT]);
}

u32 fimc_is_fd_get_playback_mode(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_PLAYBACKMODE]);
}

u32 fimc_is_fd_get_sat(void __iomem *base_addr)
{
	BUG_ON(!base_addr);

	return fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_RESULT_SAT], &fd_fields[FD_F_SAT]);
}

void fimc_is_fd_clear_intr(void __iomem *base_addr, u32 intr)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_reg(base_addr, &fd_regs[FD_R_INTSTATUS], intr);
}

void fimc_is_fd_enable_intr(void __iomem *base_addr, u32 intr)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_reg(base_addr, &fd_regs[FD_R_INTMASK], intr);
}

void fimc_is_fd_enable(void __iomem *base_addr, bool enable)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_START], enable);
}

void fimc_is_fd_input_mode(void __iomem *base_addr, u32 mode)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_PLAYBACKMODE], mode);
}

void fimc_is_fd_set_input_point(void __iomem *base_addr, u32 start_x, u32 start_y)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_START_X], &fd_fields[FD_F_STARTX], start_x);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_START_Y], &fd_fields[FD_F_STARTY], start_y);
}

void fimc_is_fd_set_input_size(void __iomem *base_addr, u32 width, u32 height, u32 size)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_WIDTH], &fd_fields[FD_F_WIDTH], width);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_HEIGHT], &fd_fields[FD_F_HEIGHT], height);

	/*
	 *  ToDo: consider DMA input mode
	 *  fimc_is_hw_set_field(base_addr, fimc_is_fd_field.PbWidth, in_width);
	 *  ...
	 */

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_INPUT_SIZE], &fd_fields[FD_F_INPUTSIZE], size);
}

void fimc_is_fd_set_down_sampling(void __iomem *base_addr, u32 scale_x, u32 scale_y)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_DS_SCALE_X], &fd_fields[FD_F_DSSCALEX], scale_x);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_DS_SCALE_Y], &fd_fields[FD_F_DSSCALEY], scale_y);
}

void fimc_is_fd_set_output_size(void __iomem *base_addr, u32 width, u32 height, u32 size)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_OUTPUT_WIDTH], &fd_fields[FD_F_OUTPUTWIDTH], width);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_OUTPUT_HEIGHT], &fd_fields[FD_F_OUTPUTHEIGHT], height);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_OUTPUT_SIZE], &fd_fields[FD_F_OUTPUTSIZE], size);
}

void fimc_is_fd_ycc_format(void __iomem *base_addr, u32 format)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_YCC_FORMAT], format);
}

void fimc_is_fd_set_cbcr_align(void __iomem *base_addr, enum fimc_is_lib_fd_cbcr_format cbcr)
{
	BUG_ON(!base_addr);

	switch (cbcr) {
	case FD_FORMAT_FIRST_CBCR:
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CBALIGNMENT], 1);
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CRALIGNMENT], 1);
		break;
	case FD_FORMAT_SECOND_CBCR:
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CBALIGNMENT], 0);
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CRALIGNMENT], 0);
		break;
	case FD_FORMAT_CBCR:
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CBALIGNMENT], 1);
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CRALIGNMENT], 0);
		break;
	case FD_FORMAT_CRCB:
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CBALIGNMENT], 0);
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_CRALIGNMENT], 1);
		break;
	}
}

void fimc_is_fd_generate_end_intr(void __iomem *base_addr, bool generate)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_FIELD_01], generate);
}

void fimc_is_fd_shadow_sw(void __iomem *base_addr, bool generate)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SHADOWSW], generate);
}

void fimc_is_fd_set_coefk(void __iomem *base_addr, u32 coef_k)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_COEF_K], &fd_fields[FD_F_COEFK], coef_k);
}

void fimc_is_fd_set_shift(void __iomem *base_addr, int shift)
{
	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHIFT], &fd_fields[FD_F_SHIFT], shift);
}

void fimc_is_fd_set_map_addr(void __iomem *base_addr, struct hw_api_fd_map *out_map)
{
	BUG_ON(!base_addr);
	BUG_ON(!out_map);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP0_OUT_ADDRESS], &fd_fields[FD_F_MAP0ADDRESS], out_map->dvaddr_0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP1_OUT_ADDRESS], &fd_fields[FD_F_MAP1ADDRESS], out_map->dvaddr_1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP2_OUT_ADDRESS], &fd_fields[FD_F_MAP2ADDRESS], out_map->dvaddr_2);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP3_OUT_ADDRESS], &fd_fields[FD_F_MAP3ADDRESS], out_map->dvaddr_3);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP4_OUT_ADDRESS], &fd_fields[FD_F_MAP4ADDRESS], out_map->dvaddr_4);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP5_OUT_ADDRESS], &fd_fields[FD_F_MAP5ADDRESS], out_map->dvaddr_5);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP6_OUT_ADDRESS], &fd_fields[FD_F_MAP6ADDRESS], out_map->dvaddr_6);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_MAP7_OUT_ADDRESS], &fd_fields[FD_F_MAP7ADDRESS], out_map->dvaddr_7);
}

void fimc_is_fd_set_ymap_addr(void __iomem *base_addr, u8 *y_map)
{
	u32 cnt = 0, max_cnt = 0;
	enum fimc_is_fd_reg_format reg_index;
	enum fimc_is_fd_reg_field field_index;

	BUG_ON(!base_addr);
	BUG_ON(!y_map);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_PROGRAMMODERAM], 1);

	reg_index = FD_R_Y_MAP0;
	field_index = FD_F_Y_MAP0;
	max_cnt = (FD_F_Y_MAP255 - FD_F_Y_MAP0);

	/* ToDo: trans y_map dvaddr -> kvaddr */
	for (cnt = 0; cnt <= max_cnt; cnt++, reg_index++, field_index++)
		fimc_is_hw_set_field(base_addr, &fd_regs[reg_index], &fd_fields[field_index], y_map[cnt]);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_PROGRAMMODERAM], 0);
}

void fimc_is_fd_config_by_setfile(void __iomem *base_addr, struct hw_api_fd_setfile_half *setfile)
{
	BUG_ON(!base_addr);

	if (setfile) {
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_SKIPFRAMES], setfile->skip_frames);
	} else {
		dbg_hw("not finded setfile address.");
		fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_SKIPFRAMES], 0);
	}

	/* ToDo: load from FD setfile structure */
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_ENDIANNESS], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_INTERRUPTTYPE], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_LP_ENABLE], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_ONESHOT], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_FIELD_01], 0);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_AXI_CTRL], &fd_fields[FD_F_BURSTSIZE], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_AXI_CTRL], &fd_fields[FD_F_PENDINGWRITEREQUESTS], 7);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_AXI_CTRL], &fd_fields[FD_F_PENDINGREADREQUESTS], 7);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SHADOWMODE], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_FIELD_02], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SHADOWDISABLE], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SHADOWTRIGGERMODE], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SHADOWENABLEMODE], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_FIELD_03], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SHADOW_CTRL], &fd_fields[FD_F_SELREGISTERMODE], 0);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_REGISTER_01], &fd_fields[FD_F_FIELD_04], 1);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_FRAME_END], &fd_fields[FD_F_FRAMEENDTOTRIGGER], 0);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_FIELD_05], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_FIELD_06], 0);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_FIELD_07], 1);
	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_SENSOR_CTRL], &fd_fields[FD_F_FIELD_08], 1);
}

int fimc_is_fd_sw_reset(void __iomem *base_addr)
{
	int ret = 0;
	int timeout = 10;

	BUG_ON(!base_addr);

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_SOFTWARERESET], 1);

	while (timeout) {
		if (fimc_is_hw_get_field(base_addr, &fd_regs[FD_R_INTSTATUS], &fd_fields[FD_F_IS_SOFTWARERESETDONE]))
			break;

		timeout--;
		/* ToDo: Remove delay operation */
		udelay(100);
	}
	if (!timeout) {
		err_hw("[FD] SW reset fail!!\n");
		ret = -EIO;
		goto exit;
	}

	fimc_is_hw_set_field(base_addr, &fd_regs[FD_R_CTRL], &fd_fields[FD_F_SOFTWARERESET], 0);

exit:
	return ret;
}
