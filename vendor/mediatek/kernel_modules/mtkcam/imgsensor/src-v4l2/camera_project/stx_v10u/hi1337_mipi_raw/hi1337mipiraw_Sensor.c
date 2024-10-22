// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 hi1337mipiraw_Sensor.c
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
#include "hi1337mipiraw_Sensor.h"
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
#include "hi1337_Sensor_adaptive_mipi.h"
#endif

static u16 get_gain2reg(u32 gain);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int hi1337_close(struct subdrv_ctx *ctx);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static int hi1337_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_streaming_suspned(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void hi1337_set_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int hi1337_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int hi1337_set_night_hyperlapse(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 hi1337_dgain2reg(struct subdrv_ctx *ctx, u32 dgain);

static int sNightHyperlapseSpeed;

#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
static int adaptive_mipi_index = -1;
static int set_mipi_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id, struct subdrv_ctx *ctx);
static int hi1337_get_mipi_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int update_mipi_info(enum SENSOR_SCENARIO_ID_ENUM scenario_id);
static void hi1337_set_mipi_pixel_rate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate);
static u32 hi1337_get_custom_mipi_pixel_rate(enum SENSOR_SCENARIO_ID_ENUM scenario_id);
#endif

/* STRUCT */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 56,
	.i4OffsetY = 24,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 16,
	.i4SubBlkH = 8,
	/* MTK R means Hi1337 L*/
	.i4PosL = {{60, 29}, {60, 45}, {68, 33}, {68, 49},
			{76, 29}, {76, 45}, {84, 33}, {84, 49}},
	.i4PosR = {{60, 25}, {60, 41}, {68, 37}, {68, 53},
			{76, 25}, {76, 41}, {84, 37}, {84, 53}},
	.iMirrorFlip = 0,
	.i4BlockNumX = 128,
	.i4BlockNumY = 96,
	.i4LeFirst = 0,
	.i4Crop = {
		// <prev> <cap> <vid> <hs_vid> <slim_vid>
		{40, 12}, {40, 12}, {40, 398}, {0, 0}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	},
	.i4FullRawW = 4208,
	.i4FullRawH = 3120,
	.i4VCPackNum = 1,
	.i4ModeIndex = 1,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3, /* sparse PD, non-interleaved */
		.i4PDRepetition = 4,
		.i4PDOrder= {1, 0 ,0, 1}, /* R L L R */
	},
};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_STREAMING_RESUME, hi1337_set_streaming_resume},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, hi1337_set_streaming_suspned},
	{SENSOR_FEATURE_SET_TEST_PATTERN, hi1337_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, hi1337_set_test_pattern_data},
	{SENSOR_FEATURE_SET_ESHUTTER, hi1337_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, hi1337_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, hi1337_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_NIGHT_HYPERLAPSE_MODE, hi1337_set_night_hyperlapse},
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
	{SENSOR_FEATURE_GET_MIPI_PIXEL_RATE, hi1337_get_mipi_pixel_rate},
#endif
};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	/* PD node */
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2B,
			.hsize = 0x100, // 256
			.vsize = 0x300, // 768
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	/* PD node */
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2B,
			.hsize = 0x100, // 256
			.vsize = 0x300, // 768
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020, //4128
			.vsize = 0x0914, //2324
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	/* PD node */
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2B,
			.hsize = 0x100, // 256
			.vsize = 0x240, // 576
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	/* Image node */
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
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	/* Image node */
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2B,
			.hsize = 0x1020,
			.vsize = 0x0C18,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};


static struct subdrv_mode_struct mode_struct[] = {
	// frame_desc_prev
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
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
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
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
		.mode_setting_table = hi1337_video_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 720,
		.framelength = 3334,
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
			.y0_offset = 412,
			.w0_size = 4224,
			.h0_size = 2328,
			.scale_w = 4224,
			.scale_h = 2328,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 2324,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 2324,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
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
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
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
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
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
		.mode_setting_table = hi1337_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_custom1_setting),
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
	// frame_desc_cus2 - not used mode
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus3 - not used mode
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = PARAM_UNDEFINED,
		.delay_frame = 2,
		.csi_param = {
			.dphy_trail = 79, // Fill correct Ths-trail (in ns unit) value
		},
		.dpc_enabled = true,
	},
	// frame_desc_cus4 - not used mode
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
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
		.mode_setting_table = hi1337_preview_setting,
		.mode_setting_len = ARRAY_SIZE(hi1337_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72040000,
		.linelength = 735,
		.framelength = 3266,
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
			.y0_offset = 26,
			.w0_size = 4224,
			.h0_size = 3100,
			.scale_w = 4224,
			.scale_h = 3100,
			.x1_offset = 48,
			.y1_offset = 2,
			.w1_size = 4128,
			.h1_size = 3096,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4128,
			.h2_tg_size = 3096,
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
	.sensor_id = HI1337_SENSOR_ID,
	.reg_addr_sensor_id = {0x0716, 0x0717},
	.i2c_addr_table = {0x42, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = 0,
	.resolution = {4128, 3096},
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
	.ana_gain_table = hi1337_ana_gain_table,
	.ana_gain_table_size = sizeof(hi1337_ana_gain_table),
	.min_gain_iso = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = (0xFFFFFF - 4),
	.exposure_step = 1,
	.exposure_margin = 4,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = 16382,
	.dig_gain_step = 2,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = -971000,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
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

	.init_setting_table = hi1337_init_setting,
	.init_setting_len = ARRAY_SIZE(hi1337_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.checksum_value = 0xd086e5a5,
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = hi1337_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, 0, 1},
	{HW_ID_AFVDD, 2800000, 1},
	{HW_ID_DVDD, 1, 0},
	{HW_ID_AVDD, 1, 0},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK, 26, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
	{HW_ID_RST, 1, 2},
};

const struct subdrv_entry hi1337_mipi_raw_entry = {
	.name = "hi1337_mipi_raw",
	.id = HI1337_SENSOR_ID,
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

static int hi1337_close(struct subdrv_ctx *ctx)
{
	DRV_LOG_MUST(ctx, "hi1337 closed\n");
	sNightHyperlapseSpeed = 0;

	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	return 0;
}

static u16 hi1337_dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	// u32 step = max((ctx->s_ctx.dig_gain_step), (u32)1);
	u8 integ = (u8) (dgain / BASE_DGAIN); // integer parts (4bit)
	u16 dec = (u16) ((dgain % BASE_DGAIN) >> 1); // decimal parts (9bit)
	u16 ret = ((u16)integ << 9) | dec;

	DRV_LOG(ctx, "dgain reg = 0x%x\n", ret);

	return ret;
}

static int hi1337_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		gains[i] = hi1337_dgain2reg(ctx, gains[i]);
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

static int hi1337_set_night_hyperlapse(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	sNightHyperlapseSpeed = (int)*feature_data;
	DRV_LOG_MUST(ctx,"sNightHyperlapseSpeed:%d\n", sNightHyperlapseSpeed);
	return ERROR_NONE;
}

void hi1337_sensor_wait_stream_off(struct subdrv_ctx *ctx)
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
		pll_enable = subdrv_i2c_rd_u16(ctx, HI1337_PLL_ENABLE_ADDR);
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

static void hi1337_set_streaming_control(struct subdrv_ctx *ctx, bool enable)
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
#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
		set_mipi_mode(ctx->current_scenario_id, ctx);
#endif
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
			hi1337_sensor_wait_stream_off(ctx); /*Wait streamoff sensor specific logic*/
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG_MUST(ctx, "X: stream[%s]\n", enable? "ON": "OFF");
}

static int hi1337_set_streaming_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	hi1337_set_streaming_control(ctx, TRUE);
	return 0;
}

static int hi1337_set_streaming_suspned(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	hi1337_set_streaming_control(ctx, FALSE);
	return 0;
}

static int hi1337_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	test_pattern_enable |= 0x1; //B[0]: test pattern enable
	subdrv_i2c_wr_u16(ctx, 0x0B04, test_pattern_enable);

	DRV_LOG_MUST(ctx, "test pattern: %d\n", mode);

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int hi1337_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int hi1337_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return hi1337_set_shutter_frame_length(ctx, para, len);
}

static int hi1337_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	u32 shutter = *feature_data;
	u32 frame_length = *(feature_data + 1);

	int fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter, (u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter, (u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);

	/* check boundary of framelength */
	ctx->frame_length = max((u32)shutter + ctx->s_ctx.exposure_margin, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	if (sNightHyperlapseSpeed) {
		// ex)fps = 1/1.5 (Frame Duration : 1.5sec), 15: 2fps(500ms), 45: 2/3fps(1.5sec)
		// frame_length = pclk / (line_length * fps)
		ctx->frame_length = (((ctx->pclk / 30) * sNightHyperlapseSpeed)/ctx->line_length);

		// shutter <= frame length - exposure margin
		shutter = min(shutter, ctx->frame_length - ctx->s_ctx.exposure_margin);
	}

	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32)shutter;

	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);

	/* write shutter */
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0], (ctx->exposure[0] >> 16) & 0xFF);
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1], (ctx->exposure[0] >> 8) & 0xFF);
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2], ctx->exposure[0] & 0xFF);

	DRV_LOG(ctx, "exp[%#x], fll(input/output):%u/%u, flick_en:%d\n",
			ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */

	return ERROR_NONE;
}

#ifdef CONFIG_CAMERA_ADAPTIVE_MIPI
static int hi1337_get_mipi_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	hi1337_set_mipi_pixel_rate_by_scenario(ctx, scenario_id, (u32 *)(uintptr_t)(*(feature_data + 1)));

	return ERROR_NONE;
}

static u32 hi1337_get_custom_mipi_pixel_rate(enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	u32 mipi_pixel_rate = 0;

	mipi_pixel_rate = update_mipi_info(scenario_id);

	return mipi_pixel_rate;
}

static void hi1337_set_mipi_pixel_rate_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*mipi_pixel_rate = hi1337_get_custom_mipi_pixel_rate(scenario_id);
		break;
	default:
		break;
	}

	if (*mipi_pixel_rate == 0)
		*mipi_pixel_rate = ctx->s_ctx.mode[scenario_id].mipi_pixel_rate;

	DRV_LOG_MUST(ctx, "mipi_pixel_rate changed %u->%u\n",
		ctx->s_ctx.mode[scenario_id].mipi_pixel_rate, *mipi_pixel_rate);
}

static int update_mipi_info(enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int sensor_mode = scenario_id;

	pr_info("[%s], scenario=%d\n", __func__, sensor_mode);

	if (sensor_mode == SENSOR_SCENARIO_ID_CUSTOM1) {
		pr_info("skip adaptive mipi , mode invalid:%d\n", sensor_mode);
		return 0;
	}

	cur_mipi_sensor_mode = &hi1337_adaptive_mipi_sensor_mode[sensor_mode];

	if (cur_mipi_sensor_mode->mipi_cell_ratings_size == 0 ||
		cur_mipi_sensor_mode->mipi_cell_ratings == NULL) {
		pr_info("skip select mipi channel\n");
		return 0;
	}

	adaptive_mipi_index = imgsensor_select_mipi_by_rf_cell_infos(cur_mipi_sensor_mode);
	pr_info("adaptive_mipi_index : %d\n", adaptive_mipi_index);
	if (adaptive_mipi_index != -1) {
		if (adaptive_mipi_index < cur_mipi_sensor_mode->sensor_setting_size) {
			pr_info("mipi_rate:%d\n", cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate);
			return cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate;
		}
	}
	pr_info("adaptive_mipi_index invalid: %d\n", adaptive_mipi_index);

	return 0;
};

static int set_mipi_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id, struct subdrv_ctx *ctx)
{
	int ret = 0;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;

	DRV_LOG(ctx, "[%s] scenario_id=%d\n", __func__, scenario_id);

	if (scenario_id == SENSOR_SCENARIO_ID_CUSTOM1) {
		DRV_LOG_MUST(ctx, "NOT supported sensor mode : %d", scenario_id);
		return -1;
	}

	cur_mipi_sensor_mode = &hi1337_adaptive_mipi_sensor_mode[scenario_id];

	if (cur_mipi_sensor_mode->sensor_setting == NULL) {
		DRV_LOG_MUST(ctx, "no mipi setting for current sensor mode\n");
		return -1;
	}
	if (adaptive_mipi_index <= CAM_HI1337_SET_A_All_705p25_MHZ || adaptive_mipi_index >= CAM_HI1337_SET_MAX_NUM) {
		DRV_LOG_MUST(ctx, "adaptive Mipi is set to the default values of 705.25 MHz(1410.5Mbps)");
	} else {
		DRV_LOG(ctx, "adaptive mipi settings: %d: mipi clock [%d]\n",
			 scenario_id, cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].mipi_rate);

		subdrv_i2c_wr_regs_u16(ctx,
			cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].setting,
			cur_mipi_sensor_mode->sensor_setting[adaptive_mipi_index].setting_size);
	}
	DRV_LOG(ctx, "[%s]-X\n", __func__);
	return ret;
}
#endif
