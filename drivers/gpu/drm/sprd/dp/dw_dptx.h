// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __DPTX_H__
#define __DPTX_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_fixed.h>
#include <drm/drm_crtc_helper.h>
#include <linux/extcon.h>

#include "drm_dp_helper_additions.h"
#include "dp_video.h"
#include "dp_reg.h"

#define DPTX_RECEIVER_CAP_SIZE		0x100
#define DPTX_SDP_NUM		0x10
#define DPTX_SDP_LEN	0x9
#define DPTX_SDP_SIZE	(9 * 4)
#define PARSE_EST_TIMINGS_FROM_BYTE3

/* The max rate and lanes supported by the core */
#define DPTX_MAX_LINK_RATE DPTX_PHYIF_CTRL_RATE_HBR3
#define DPTX_MAX_LINK_LANES 4

/* The default rate and lanes to use for link training */
#define DPTX_DEFAULT_LINK_RATE DPTX_MAX_LINK_RATE
#define DPTX_DEFAULT_LINK_LANES DPTX_MAX_LINK_LANES

/**
 * struct dptx_link - The link state.
 * @status: Holds the sink status register values.
 * @trained: True if the link is successfully trained.
 * @rate: The current rate that the link is trained at.
 * @lanes: The current number of lanes that the link is trained at.
 * @preemp_level: The pre-emphasis level used for each lane.
 * @vswing_level: The vswing level used for each lane.
 */
struct dptx_link {
	u8 status[DP_LINK_STATUS_SIZE];
	bool trained;
	u8 rate;
	u8 lanes;
	u8 preemp_level[4];
	u8 vswing_level[4];
};

/**
 * struct dptx_aux - The aux state
 * @sts: The AUXSTS register contents.
 * @data: The AUX data register contents.
 * @event: Indicates an AUX event ocurred.
 * @abort: Indicates that the AUX transfer should be aborted.
 */
struct dptx_aux {
	u32 sts;
	u32 data[4];
	atomic_t abort;
};

struct sdp_header {
	u8 HB0;
	u8 HB1;
	u8 HB2;
	u8 HB3;
} __packed;

struct sdp_full_data {
	u8 en;
	u32 payload[9];
	u8 blanking;
	u8 cont;
} __packed;

/**
 * struct dptx - The representation of the DP TX core
 * @mutex: dptx mutex
 * @base: Base address of the registers
 * @irq: IRQ number
 * @version: Contents of the IP_VERSION register
 * @max_rate: The maximum rate that the controller supports
 * @max_lanes: The maximum lane count that the controller supports
 * @dev: The struct device
 * @vparams: The video params to use
 * @aparams: The audio params to use
 * @hparams: The HDCP params to use
 * @c_connect: Signals that a HOT_PLUG or HOT_UNPLUG has occurred.
 * @sink_request: Signals the a HPD_IRQ has occurred.
 * @rx_caps: The sink's receiver capabilities.
 * @sdp_list: The array of SDP elements
 * @aux: AUX channel state for performing an AUX transfer.
 * @link: The current link state.
 * @multipixel: Controls multipixel configuration. 0-Single, 1-Dual, 2-Quad.
 */
struct dptx {
	struct mutex mutex;	/* generic mutex for dptx */
	struct drm_encoder *encoder;
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct extcon_dev *edev;
	struct drm_bridge *bridge;
	struct drm_dp_aux aux_dev;
	struct dptx_aux aux;
	struct {
		u8 multipixel;
		u8 streams;
		bool gen2phy;
		bool dsc;
	} hwparams;

	void __iomem *base;
	struct regmap *ipa_usb31_dp;

	int irq;
	u32 version;
	u8 max_rate;
	u8 max_lanes;
	bool ycbcr420;
	u8 streams;
	bool mst;
	u8 multipixel;
	bool ssc_en;

	struct video_params vparams;
	struct audio_params aparams;
	struct audio_short_desc audio_desc;

	atomic_t c_connect;
	atomic_t sink_request;

	u8 rx_caps[DPTX_RECEIVER_CAP_SIZE];

	struct sdp_full_data sdp_list[DPTX_SDP_NUM];
	struct dptx_link link;

	bool active;
};

/*
 * Core interface functions
 */
int dptx_core_init(struct dptx *dptx);
void dptx_init_hwparams(struct dptx *dptx);
void dptx_core_init_phy(struct dptx *dptx);
int dptx_core_program_ssc(struct dptx *dptx, bool sink_ssc);
bool dptx_sink_enabled_ssc(struct dptx *dptx);
void dptx_enable_ssc(struct dptx *dptx);

int dptx_core_deinit(struct dptx *dptx);
void dptx_soft_reset(struct dptx *dptx, u32 bits);
void dptx_soft_reset_all(struct dptx *dptx);
void dptx_phy_soft_reset(struct dptx *dptx);

irqreturn_t dptx_irq(int irq, void *dev);
irqreturn_t dptx_threaded_irq(int irq, void *dev);

void dptx_global_intr_en(struct dptx *dp);
void dptx_global_intr_dis(struct dptx *dp);
bool dptx_get_hotplug_status(struct dptx *dp);

/*
 * PHY IF Control
 */
void dptx_phy_set_lanes(struct dptx *dptx, u32 num);
u32 dptx_phy_get_lanes(struct dptx *dptx);
void dptx_phy_set_rate(struct dptx *dptx, u32 rate);
u32 dptx_phy_get_rate(struct dptx *dptx);
int dptx_phy_wait_busy(struct dptx *dptx, u32 lanes);
void dptx_phy_set_pre_emphasis(struct dptx *dptx,
			       u32 lane, u32 level);
void dptx_phy_set_vswing(struct dptx *dptx,
			 u32 lane, u32 level);
void dptx_phy_set_pattern(struct dptx *dptx, u32 pattern);
void dptx_phy_enable_xmit(struct dptx *dptx, u32 lane, bool enable);

int dptx_phy_rate_to_bw(u32 rate);
int dptx_bw_to_phy_rate(u32 bw);

/*
 * Link training
 */
int dptx_link_training(struct dptx *dptx, u8 rate, u8 lanes);
int dptx_link_check_status(struct dptx *dptx);

/*
 * Register read and write functions
 */
static inline void dptx_writel(struct dptx *dp, u32 offset, u32 data)
{
	writel(data, dp->base + offset);
}

static inline u32 dptx_readl(struct dptx *dp, u32 offset)
{
	return readl(dp->base + offset);
}

void dptx_fill_sdp(struct dptx *dptx, struct sdp_full_data *data);
struct dptx *dptx_init(struct device *dev, struct drm_device *drm_dev);

int dptx_aux_rw_bytes(struct dptx *dptx,
		      bool rw,
		      bool i2c,
		      u32 addr,
		      u8 *bytes,
		      u32 len);
int dptx_read_bytes_from_i2c(struct dptx *dptx,
			     u32 device_addr,
			     u8 *bytes,
			     u32 len);
int dptx_write_bytes_to_i2c(struct dptx *dptx,
			    u32 device_addr,
			    u8 *bytes,
			    u32 len);
int dptx_aux_transfer(struct dptx *dptx, struct drm_dp_aux_msg *aux_msg);

void dptx_audio_sdp_en(struct dptx *dptx);

void dptx_audio_timestamp_sdp_en(struct dptx *dptx);

int handle_automated_test_request(struct dptx *dptx);

void dptx_en_dis_hdcp13(struct dptx *dptx, u8 enable);

#endif
