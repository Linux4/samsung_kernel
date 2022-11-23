// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __DP_VIDEO_H__
#define __DP_VIDEO_H__

#define AUDIO_INFOFREAME_HEADER 0x481B8400
struct dptx;

struct audio_params {
	u8 data_width;
	u8 num_channels;
	u8 inf_type;
	u8 mute;
	u8 ats_ver;
	u32 rate;
};

enum audio_sample_freq {
	SAMPLE_FREQ_32 = 0,
	SAMPLE_FREQ_44_1 = 1,
	SAMPLE_FREQ_48 = 2,
	SAMPLE_FREQ_88_2 = 3,
	SAMPLE_FREQ_96 = 4,
	SAMPLE_FREQ_176_4 = 5,
	SAMPLE_FREQ_192 = 6
};

struct audio_short_desc {
	u8 max_num_of_channels;
	enum audio_sample_freq max_sampling_freq;
	u8 max_bit_per_sample;
};

void dptx_audio_params_reset(struct audio_params *aparams);
void dptx_audio_core_config(struct dptx *dptx);
void dptx_en_audio_channel(struct dptx *dptx, int ch_num, int enable);
void dptx_audio_mute(struct dptx *dptx, bool enable);

void dptx_audio_inf_type_change(struct dptx *dptx);
void dptx_audio_num_ch_change(struct dptx *dptx);
void dptx_audio_data_width_change(struct dptx *dptx);
void dptx_audio_infoframe_sdp_send(struct dptx *dptx);
int sprd_dp_audio_codec_init(struct device *dev);

enum pixel_enc_type {
	RGB = 0,
	YCBCR420 = 1,
	YCBCR422 = 2,
	YCBCR444 = 3,
	YONLY = 4,
	RAW = 5
};

enum color_depth {
	COLOR_DEPTH_INVALID = 0,
	COLOR_DEPTH_6 = 6,
	COLOR_DEPTH_8 = 8,
	COLOR_DEPTH_10 = 10,
	COLOR_DEPTH_12 = 12,
	COLOR_DEPTH_16 = 16
};

enum pattern_mode {
	TILE = 0,
	RAMP = 1,
	CHESS = 2,
	COLRAMP = 3
};

enum dynamic_range_type {
	CEA = 1,
	VESA = 2
};

enum colorimetry_type {
	ITU601 = 1,
	ITU709 = 2
};

enum video_format_type {
	VCEA = 0,
	CVT = 1,
	DMT = 2
};

struct dtd {
	u16 pixel_repetition_input;
	int pixel_clock;
	/** 1 for interlaced, 0 progressive */
	u8 interlaced;
	u16 h_active;
	u16 h_blanking;
	u16 h_image_size;
	u16 h_sync_offset;
	u16 h_sync_pulse_width;
	u8 h_sync_polarity;
	u16 v_active;
	u16 v_blanking;
	u16 v_image_size;
	u16 v_sync_offset;
	u16 v_sync_pulse_width;
	u8 v_sync_polarity;
};

struct video_params {
	u8 pix_enc;
	u8 pattern_mode;
	struct dtd mdtd;
	u8 mode;
	u8 bpc;
	u8 colorimetry;
	u8 dynamic_range;
	u8 aver_bytes_per_tu;
	u8 aver_bytes_per_tu_frac;
	u8 init_threshold;
	u32 refresh_rate;
	u8 video_format;
};

int dptx_dtd_parse(struct dptx *dptx, struct dtd *mdtd, u8 data[18]);
void dptx_video_params_reset(struct dptx *dptx);
void dptx_misc_reset(struct dptx *dptx);

void dptx_video_timing_change(struct dptx *dptx, int stream);
void dptx_video_core_config(struct dptx *dptx, int stream);
void dptx_video_set_core_bpc(struct dptx *dptx, int stream);
void dptx_video_set_sink_bpc(struct dptx *dptx, int stream);
void dptx_video_set_sink_col(struct dptx *dpx, int stream);
void dptx_video_bpc_change(struct dptx *dptx, int stream);
void dptx_video_ts_change(struct dptx *dptx, int stream);
int dptx_video_ts_calculate(struct dptx *dptx, int lane_num, int rate,
			    int bpc, int encoding, int pixel_clock);
void dptx_enable_default_video_stream(struct dptx *dptx, int stream);
void dptx_disable_default_video_stream(struct dptx *dptx, int stream);
void dptx_video_reset(struct dptx *dptx, int enable, int stream);
void dptx_vsc_ycbcr420_send(struct dptx *dptx, u8 enable);
int dptx_timing_cfg(struct dptx *dptx, const struct drm_display_mode *mode,
		    const struct drm_display_info *info);

#endif
