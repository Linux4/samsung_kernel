// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 hi1337fumipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define pr_fmt(fmt) "[D/D]" fmt

#include "hi1337fumipiraw_Sensor.h"
static u16 get_gain2reg(u32 gain);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int hi1337fu_close(struct subdrv_ctx *ctx);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int hi1337fu_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337fu_set_streaming_suspned(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void hi1337fu_set_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int hi1337fu_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337fu_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337fu_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337fu_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int hi1337fu_open(struct subdrv_ctx *ctx);
static int hi1337fu_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 hi1337fu_dgain2reg(struct subdrv_ctx *ctx, u32 dgain);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_CHECK_SENSOR_ID, hi1337fu_check_sensor_id},
	{SENSOR_FEATURE_SET_STREAMING_RESUME, hi1337fu_set_streaming_resume},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, hi1337fu_set_streaming_suspned},
	{SENSOR_FEATURE_SET_TEST_PATTERN, hi1337fu_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, hi1337fu_set_test_pattern_data},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, hi1337fu_set_multi_dig_gain},
};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0,
			.vsize = 0x0BB8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0,
			.vsize = 0x0BB8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0, //4000
			.vsize = 0x08D0, //2256
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0,
			.vsize = 0x0BB8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0,
			.vsize = 0x0BB8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x03E8, // 1000
			.vsize = 0x02EC, // 748
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x07F0, //2032
			.vsize = 0x05F4, //1524
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x07F0, //2032
			.vsize = 0x05F4, //1524
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x07F0, //2032
			.vsize = 0x05F4, //1524
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x0FA0,
			.vsize = 0x0BB8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};


static struct subdrv_mode_struct mode_struct[] = {
	// frame_desc_prev
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = hi1337fu_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 760,
		.framelength = 3158,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 74,
			.w0_size = 4224,
			.h0_size = 3004,
			.scale_w = 4224,
			.scale_h = 3004,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cap
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = hi1337fu_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 760,
		.framelength = 3158,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 74,
			.w0_size = 4224,
			.h0_size = 3004,
			.scale_w = 4224,
			.scale_h = 3004,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_vid
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = hi1337fu_video_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 1010,
		.framelength = 2376,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000, //default: 1410.5 Mbps
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 446,
			.w0_size = 4224,
			.h0_size = 2260,
			.scale_w = 4224,
			.scale_h = 2260,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 2256,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2256,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_hs_vid - not used mode
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = hi1337fu_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 760,
		.framelength = 3158,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 74,
			.w0_size = 4224,
			.h0_size = 3004,
			.scale_w = 4224,
			.scale_h = 3004,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_slim_vid - not used mode
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = hi1337fu_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 760,
		.framelength = 3158,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 74,
			.w0_size = 4224,
			.h0_size = 3004,
			.scale_w = 4224,
			.scale_h = 3004,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus1
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = hi1337fu_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 720,
		.framelength = 833,
		.max_framerate = 1200,
		.mipi_pixel_rate = 144300000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 72,
			.w0_size = 4224,
			.h0_size = 3008,
			.scale_w = 1056,
			.scale_h = 752,
			.x1_offset = 28,
			.y1_offset = 2,
			.w1_size = 1000,
			.h1_size = 748,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1000,
			.h2_tg_size = 748,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000, // by IQ request, actual 4x4 binning
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 77, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus2 - FOV80 4:3
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = hi1337fu_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 1490,
		.framelength = 1611,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 120,
			.y0_offset = 812,
			.w0_size = 4000,
			.h0_size = 1528,
			.scale_w = 4000,
			.scale_h = 1528,
			.x1_offset = 984,
			.y1_offset = 2,
			.w1_size = 2032,
			.h1_size = 1524,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2032,
			.h2_tg_size = 1524,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus3 - FOV80
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = hi1337fu_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 1490,
		.framelength = 1611,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 120,
			.y0_offset = 812,
			.w0_size = 4000,
			.h0_size = 1528,
			.scale_w = 4000,
			.scale_h = 1528,
			.x1_offset = 984,
			.y1_offset = 2,
			.w1_size = 2032,
			.h1_size = 1524,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2032,
			.h2_tg_size = 1524,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus4 - FOV80 16:9
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = hi1337fu_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 1490,
		.framelength = 1611,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 120,
			.y0_offset = 812,
			.w0_size = 4000,
			.h0_size = 1528,
			.scale_w = 4000,
			.scale_h = 1528,
			.x1_offset = 984,
			.y1_offset = 2,
			.w1_size = 2032,
			.h1_size = 1524,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2032,
			.h2_tg_size = 1524,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus5 - not used mode
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = hi1337fu_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337fu_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 760,
		.framelength = 3158,
		.max_framerate = 300,
		.mipi_pixel_rate = 564200000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 0,
		.coarse_integ_step = 0,
		.imgsensor_winsize_info = {
			.full_w = 4240,
			.full_h = 3152,
			.x0_offset = 8,
			.y0_offset = 74,
			.w0_size = 4224,
			.h0_size = 3004,
			.scale_w = 4224,
			.scale_h = 3004,
			.x1_offset = 112,
			.y1_offset = 2,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = HI1337FU_SENSOR_ID,
	.reg_addr_sensor_id = {0x0716, 0x0717},
	.i2c_addr_table = {0x42, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = 0,
	.resolution = {4000, 3000},
	.mirror = IMAGE_NORMAL,

	.mclk = 26,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_CSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 3,
	.ana_gain_step = 1,
	.ana_gain_table = hi1337fu_ana_gain_table,
	.ana_gain_table_size = sizeof(hi1337fu_ana_gain_table),
	.min_gain_iso = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = (0xFFFF - 4),
	.exposure_step = 1,
	.exposure_margin = 4,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = 16382,
	.dig_gain_step = 2,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 5500000,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = PARAM_UNDEFINED,
	.s_cali = PARAM_UNDEFINED,

	.reg_addr_stream = 0x0b00,
	.reg_addr_mirror_flip = 0x0c34,
	.reg_addr_exposure = {{0x020D, 0x020A, 0x020B},{0x021F, 0x021C, 0x021D},},

	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x0212, 0x0213},},
	.reg_addr_dig_gain = {{0x0214, 0x0215},{0x0216, 0x0217},{0x0218, 0x0219},{0x021A, 0x021B},},
	.reg_addr_frame_length = {0x0211, 0x020E, 0x020F},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,
	.reg_addr_fast_mode = PARAM_UNDEFINED,
	.chk_s_off_end = 1,

	.init_setting_table = hi1337fu_init_setting,
	.init_setting_len = ARRAY_SIZE(hi1337fu_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.checksum_value = 0xd086e5a5,
};

static struct subdrv_ops ops = {
	.get_id = hi1337fu_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = hi1337fu_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = hi1337fu_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, 0, 1},
	{HW_ID_DVDD, 1, 0},
	{HW_ID_AVDD, 1, 0},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK, 26, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
	{HW_ID_RST, 1, 2},
};

const struct subdrv_entry hi1337fu_mipi_raw_entry = {
	.name = "hi1337fu_mipi_raw",
	.id = HI1337FU_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static u16 get_gain2reg(u32 gain)
{
	return gain * 16 / BASEGAIN - 1 * 16;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int hi1337fu_close(struct subdrv_ctx *ctx)
{
	DRV_LOG_MUST(ctx, "hi1337fu closed\n");
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	return 0;
}

static u16 hi1337fu_dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	// u32 step = max((ctx->s_ctx.dig_gain_step), (u32)1);
	u8 integ = (u8) (dgain / BASE_DGAIN); // integer parts (4bit)
	u16 dec = (u16) ((dgain % BASE_DGAIN) >> 1); // decimal parts (9bit)
	u16 ret = ((u16)integ << 9) | dec;

	DRV_LOG(ctx, "dgain reg = 0x%x\n", ret);

	return ret;
}

static int hi1337fu_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 *gains = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	int i = 0;
	u16 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_dig_gain_in_lut(ctx, gains, exp_cnt);
		return ERROR_NONE;
	}
	// skip if no porting digital gain
	if (!ctx->s_ctx.reg_addr_dig_gain[0].addr[0])
		return ERROR_NONE;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}

	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = hi1337fu_dgain2reg(ctx, gains[i]);
	}

	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	switch (exp_cnt) {
	case 1:
		rg_gains[0] = gains[0];
		break;
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		break;
	}
	for (i = 0;
	     (i < ARRAY_SIZE(rg_gains)) && (i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain));
	     i++) {
		if (!rg_gains[i])
			continue; // skip zero gain setting

		subdrv_i2c_wr_u16(ctx, 0x0214, rg_gains[i]);
		subdrv_i2c_wr_u16(ctx, 0x0216, rg_gains[i]);
		subdrv_i2c_wr_u16(ctx, 0x0218, rg_gains[i]);
		subdrv_i2c_wr_u16(ctx, 0x021A, rg_gains[i]);
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);

	return ERROR_NONE;
}

void hi1337fu_sensor_wait_stream_off(struct subdrv_ctx *ctx)
{
	u32 i = 0;
	u32 timeout = 0;
	u16 pll_enable = 0;
	u32 init_delay = 2;
	u32 check_delay = 1;

	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;

	mDELAY(init_delay);
	for (i = 0; i < timeout; i++) {
		pll_enable = subdrv_i2c_rd_u16(ctx, HI1337FU_PLL_ENABLE_ADDR);
		DRV_LOG(ctx,"wait_streamoff: pll_enable:%#x\n", pll_enable);
		if ((pll_enable & 0x0100) == 0) {
			DRV_LOG_MUST(ctx,"wait_streamoff delay:%d ms\n", init_delay + check_delay * i);
			return;
		}
		mDELAY(check_delay);
		DRV_LOG(ctx,"wait_streamoff delay:%d ms\n", init_delay + check_delay * i);
	}
	DRV_LOGE(ctx, "stream off fail!,cur_fps:%u,timeout:%u ms\n", ctx->current_fps, timeout);
}

static void hi1337fu_set_streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	u64 stream_ctrl_delay_timing = 0;

	DRV_LOG_MUST(ctx, "E: stream[%s]\n", enable? "ON": "OFF");
	check_current_scenario_id_bound(ctx);
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.streaming_ctrl_imp) {
		if (ctx->s_ctx.s_streaming_control != NULL)
			ctx->s_ctx.s_streaming_control((void *) ctx, enable);
		else
			DRV_LOG_MUST(ctx,
				"please implement drive own streaming control!(sid:%u)\n",
				ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"stream ctrl implement on scp side!(sid:%u)\n",
			ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}

	if (enable) {
		set_dummy(ctx);
		subdrv_i2c_wr_u16(ctx, ctx->s_ctx.reg_addr_stream, 0x0100);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
	} else {
		ctx->stream_ctrl_end_time = ktime_get_boottime_ns();
		if (ctx->s_ctx.custom_stream_ctrl_delay &&
			ctx->stream_ctrl_start_time && ctx->stream_ctrl_end_time) {
			stream_ctrl_delay_timing =
				(ctx->stream_ctrl_end_time - ctx->stream_ctrl_start_time) / 1000000;
			DRV_LOG_MUST(ctx,
				"custom_stream_ctrl_delay/stream_ctrl_delay_timing:%llu/%llu\n",
				ctx->s_ctx.custom_stream_ctrl_delay,
				stream_ctrl_delay_timing);
			if (stream_ctrl_delay_timing < ctx->s_ctx.custom_stream_ctrl_delay)
				mDELAY(ctx->s_ctx.custom_stream_ctrl_delay - stream_ctrl_delay_timing);
		}
		subdrv_i2c_wr_u16(ctx, ctx->s_ctx.reg_addr_stream, 0x0000);
		if (ctx->s_ctx.reg_addr_fast_mode && ctx->fast_mode_on) {
			ctx->fast_mode_on = FALSE;
			ctx->ref_sof_cnt = 0;
			DRV_LOG(ctx, "seamless_switch disabled.");
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
			commit_i2c_buffer(ctx);
		}
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			hi1337fu_sensor_wait_stream_off(ctx); /*Wait streamoff sensor specific logic*/
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG_MUST(ctx, "X: stream[%s]\n", enable? "ON": "OFF");
}

static int hi1337fu_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	hi1337fu_set_streaming_control(ctx, TRUE);
	return 0;
}

static int hi1337fu_set_streaming_suspned(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	hi1337fu_set_streaming_control(ctx, FALSE);
	return 0;
}

static int hi1337fu_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);
	u16 test_pattern_enable = 0;

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);

	/* 1:Solid Color 2:Color Bar */
	if (mode)
		subdrv_i2c_wr_u16(ctx, 0x0C0A, (mode << 8) & 0xff00);
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u16(ctx, 0x0C0A, 0x00); /*No pattern*/

	test_pattern_enable = subdrv_i2c_rd_u16(ctx, 0x0B04);
	test_pattern_enable |= 0x1;
	subdrv_i2c_wr_u16(ctx, 0x0B04, test_pattern_enable);

	DRV_LOG_MUST(ctx, "test pattern: %d\n", mode);

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int hi1337fu_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 16) & 0xffff;
	u16 Gr = (data->Channel_Gr >> 16) & 0xffff;
	u16 Gb = (data->Channel_Gb >> 16) & 0xffff;
	u16 B = (data->Channel_B >> 16) & 0xffff;

	subdrv_i2c_wr_u16(ctx, 0x0C0C, R);
	subdrv_i2c_wr_u16(ctx, 0x0C0E, Gr);
	subdrv_i2c_wr_u16(ctx, 0x0C10, B);
	subdrv_i2c_wr_u16(ctx, 0x0C12, Gb);

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = %#06x/%#06x/%#06x/%#06x\n", ctx->test_pattern, R, Gr, Gb, B);
	return ERROR_NONE;
}

static int hi1337fu_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return hi1337fu_get_imgsensor_id(ctx, (u32 *)para);
}

static int hi1337fu_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8(ctx, addr_ll);
			DRV_LOG(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == HI1337_SENSOR_ID) {
				*sensor_id = ctx->s_ctx.sensor_id;
				return ERROR_NONE;
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static int hi1337fu_open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (hi1337fu_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE) {
#ifdef IMGSENSOR_HW_PARAM
		imgsensor_increase_hw_param_sensor_err_cnt(imgsensor_get_sensor_position(ctx->s_ctx.sensor_id));
#endif
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	/* initail setting */
	if (ctx->s_ctx.aov_sensor_support && !ctx->s_ctx.init_in_open)
		DRV_LOG_MUST(ctx, "sensor init not in open stage!\n");
	else
		sensor_init(ctx);

	if (ctx->s_ctx.s_cali != NULL)
		ctx->s_ctx.s_cali((void *) ctx);
	else
		write_sensor_Cali(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->frame_length_rg = ctx->frame_length;
	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));

		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 2:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->frame_length -
				ctx->frame_length_in_lut[0];
			break;
		case 3:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[2] = ctx->frame_length -
				ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			break;
		default:
			break;
		}

		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	}

	return ERROR_NONE;
}

static u8 hi1337fu_otp_read_byte(struct i2c_client *client, u16 addr)
{
	u8 read_val = 0;
	adaptor_i2c_wr_u8(client, client->addr, 0x030A, (addr >> 8) & 0xFF);
	adaptor_i2c_wr_u8(client, client->addr, 0x030B, addr & 0xFF);
	adaptor_i2c_wr_u8(client, client->addr, 0x031C, 0x00);
	adaptor_i2c_wr_u8(client, client->addr, 0x031D, 0x00);
	adaptor_i2c_wr_u8(client, client->addr, 0x0302, 0x01);
	adaptor_i2c_rd_u8(client, client->addr, 0x0308, &read_val);
	return read_val;
}

static int hi1337fu_otp_init(struct i2c_client *client)
{
	return adaptor_i2c_wr_regs_u16(client, client->addr,
		hi1337fu_otp_init_setting, ARRAY_SIZE(hi1337fu_otp_init_setting));
}

static void hi1337fu_otp_off_setting(struct i2c_client *client)
{
	adaptor_i2c_wr_u8(client, client->addr, 0x0B00, 0x00);
	mDELAY(10);
	adaptor_i2c_wr_u8(client, client->addr, 0x0260, 0x00);
	adaptor_i2c_wr_u8(client, client->addr, 0x0B00, 0x01);
	mDELAY(1);
}

int hi1337fu_read_otp_cal(struct i2c_client *client, unsigned char *data, unsigned int size)
{
	u32 i = 0;
	u8 otp_bank = 0;
	u32 read_addr = 0;

	pr_info("%s-E\n", __func__);

	if (data == NULL) {
		pr_err("%s, fail, data is NULL\n", __func__);
		return -1;
	}

	pr_info("%s, check i2c_write_addr: %#04x\n", __func__, client->addr);

	// OTP initial setting
	hi1337fu_otp_init(client);

	adaptor_i2c_wr_u8(client, client->addr, 0x0B00, 0x00);

	mDELAY(10);

	adaptor_i2c_wr_u8(client, client->addr, 0x0260, 0x10);
	adaptor_i2c_wr_u8(client, client->addr, 0x030F, 0x14);
	adaptor_i2c_wr_u8(client, client->addr, 0x0B00, 0x01);

	mDELAY(1);

	otp_bank = hi1337fu_otp_read_byte(client, HI1337FU_OTP_CHECK_BANK);
	pr_info("%s, check OTP bank:%#x, size: %d\n", __func__, otp_bank, size);

	switch (otp_bank) {
	case HI1337FU_OTP_BANK1_MARK:
		read_addr = HI1337FU_OTP_BANK1_START_ADDR;
		break;
	case HI1337FU_OTP_BANK2_MARK:
		read_addr = HI1337FU_OTP_BANK2_START_ADDR;
		break;
	case HI1337FU_OTP_BANK3_MARK:
		read_addr = HI1337FU_OTP_BANK3_START_ADDR;
		break;
	case HI1337FU_OTP_BANK4_MARK:
		read_addr = HI1337FU_OTP_BANK4_START_ADDR;
		break;
	default:
		pr_info("%s, check bank: fail\n", __func__);
		hi1337fu_otp_off_setting(client);
		return -1;
	}

	for (i = 0; i < size; i++) {
		*(data + i) = hi1337fu_otp_read_byte(client, read_addr);
		pr_debug("%s, read data read_addr: %#x, data: %x\n", __func__, read_addr, *(data + i));
		read_addr++;
	}

	// OTP off setting
	hi1337fu_otp_off_setting(client);

	pr_info("%s-X\n", __func__);

	return (int)size;
}
