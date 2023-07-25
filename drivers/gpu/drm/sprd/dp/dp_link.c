// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/regmap.h>
#include "dw_dptx.h"

static int dptx_link_check_cr_done(struct dptx *dptx, bool *out_done)
{
	int ret_dpcd;

	DRM_DEBUG("%s:\n", __func__);

	if (!out_done)
		return -EINVAL;

	drm_dp_link_train_channel_eq_delay(dptx->rx_caps);

	ret_dpcd = drm_dp_dpcd_read_link_status(&dptx->aux_dev,
				dptx->link.status);
	if (ret_dpcd < 0)
		return ret_dpcd;

	*out_done = drm_dp_clock_recovery_ok(dptx->link.status,
					     dptx->link.lanes);

	DRM_DEBUG("%s: CR_DONE = %d\n", __func__, *out_done);

	return 0;
}

static int dptx_link_check_ch_eq_done(struct dptx *dptx,
				      bool *out_cr_done,
				      bool *out_ch_eq_done)
{
	int retval;
	bool done = false;

	DRM_DEBUG("%s:\n", __func__);

	if (!out_cr_done || !out_ch_eq_done)
		return -EINVAL;

	retval = dptx_link_check_cr_done(dptx, &done);
	if (retval)
		return retval;

	*out_cr_done = false;
	*out_ch_eq_done = false;

	if (!done)
		return 0;

	*out_cr_done = true;
	*out_ch_eq_done = drm_dp_channel_eq_ok(dptx->link.status,
					       dptx->link.lanes);

	DRM_DEBUG("%s: CH_EQ_DONE = %d\n", __func__, *out_ch_eq_done);

	return 0;
}

void dptx_link_set_preemp_vswing(struct dptx *dptx)
{
	int i;
	u8 pe = 0, vs = 0;
	u32 reg = 0;

	for (i = 0; i < dptx->link.lanes; i++) {
		pe = dptx->link.preemp_level[i];
		vs = dptx->link.vswing_level[i];

		dptx_phy_set_pre_emphasis(dptx, i, pe);
		dptx_phy_set_vswing(dptx, i, vs);
	}

	regmap_read(dptx->ipa_usb31_dp, 0x20, &reg);
	/* enable cr apb */
	reg |= BIT(4);
	regmap_write(dptx->ipa_usb31_dp, 0x20, reg);
	/* enable cr clk */
	reg |= BIT(3);
	regmap_write(dptx->ipa_usb31_dp, 0x20, reg);

	/*
	 * 0x8 bit7:PHY_REG_EN; bit0~15:PHY_REG_ADDR.
	 * 0xc bit0-31: PHY_REG_DATA.
	 */

	/* phy ram addr 0x21: SUP_DIG_LVL_OVER_IN. */
	reg = 0x10021;
	regmap_write(dptx->ipa_usb31_dp, 0x8, reg);
	regmap_read(dptx->ipa_usb31_dp, 0xc, &reg);
	/* bit7: TX_VBOOST_LVL_EN; bit6L4=5 TX_TXVBOOST_LVL */
	reg &= ~BIT(5);
	reg |= BIT(7) | BIT(6) | BIT(4);
	regmap_write(dptx->ipa_usb31_dp, 0xc, reg);

	/* tuning override phy eq value */
	if (vs == 0 && pe == 0) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x570);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2000);
	}

	if (vs == 0 && pe == 1) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x5d0);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2300);
	}

	if (vs == 0 && pe == 2) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x620);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2580);
	}

	if (vs == 0 && pe == 3) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x6a0);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2980);
	}

	if (vs == 1 && pe == 0) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x620);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2000);
	}

	if (vs == 1 && pe == 1) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x6a0);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2400);
	}

	if (vs == 1 && pe == 2) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x700);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2700);
	}

	if (vs == 2 && pe == 0) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x6e0);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2000);
	}

	if (vs == 2 && pe == 1) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x760);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2400);
	}

	if (vs == 3 && pe == 0) {
		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19002);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x7e0);

		regmap_write(dptx->ipa_usb31_dp, 0x8, 0x19003);
		regmap_write(dptx->ipa_usb31_dp, 0xc, 0x2000);
	}
}

int dptx_link_training_lanes_set(struct dptx *dptx)
{
	int ret_dpcd, i;
	u8 bytes[4] = { 0xff, 0xff, 0xff, 0xff };

	for (i = 0; i < dptx->link.lanes; i++) {
		u8 byte = 0;

		byte |= ((dptx->link.vswing_level[i] <<
			  DP_TRAIN_VOLTAGE_SWING_SHIFT) &
			 DP_TRAIN_VOLTAGE_SWING_MASK);

		if (dptx->link.vswing_level[i] == 3)
			byte |= DP_TRAIN_MAX_SWING_REACHED;

		byte |= ((dptx->link.preemp_level[i] <<
			  DP_TRAIN_PRE_EMPHASIS_SHIFT) &
			 DP_TRAIN_PRE_EMPHASIS_MASK);

		if (dptx->link.preemp_level[i] == 3)
			byte |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		bytes[i] = byte;
	}

	ret_dpcd =
	    drm_dp_dpcd_write(&dptx->aux_dev, DP_TRAINING_LANE0_SET, bytes,
			      dptx->link.lanes);
	if (ret_dpcd < 0)
		return ret_dpcd;

	return 0;
}

int dptx_link_adjust_drive_settings(struct dptx *dptx, int *out_changed)
{
	int retval;
	int i;
	u8 v = 0;
	u8 p = 0;
	int changed = false;

	for (i = 0; i < dptx->link.lanes; i++) {
		u8 this_v = drm_dp_get_adjust_request_voltage(
			dptx->link.status, i);
		u8 this_p = drm_dp_get_adjust_request_pre_emphasis(
			dptx->link.status, i) >> DP_TRAIN_PRE_EMPHASIS_SHIFT;

		if (this_v > v)
			v = this_v;
		if (this_p > p)
			p = this_p;

		if (dptx->link.vswing_level[i] != v ||
			dptx->link.preemp_level[i] != p)
			changed = true;

		dptx->link.vswing_level[i] = v;
		dptx->link.preemp_level[i] = p;
	}

	dptx_link_set_preemp_vswing(dptx);

	retval = dptx_link_training_lanes_set(dptx);
	if (retval)
		return retval;

	if (out_changed)
		*out_changed = changed;

	return 0;
}

static int dptx_link_training_init(struct dptx *dptx,
				   u8 rate,
				   u8 lanes)
{
	u8 sink_max_rate;
	u8 sink_max_lanes;

	DRM_DEBUG("%s: lanes=%d, rate=%d\n", __func__, lanes, rate);

	if (rate > DPTX_PHYIF_CTRL_RATE_HBR3) {
		DRM_ERROR("Invalid rate %d\n", rate);
		rate = DPTX_PHYIF_CTRL_RATE_RBR;
	}

	if (!lanes || (lanes == 3) || (lanes > 4)) {
		DRM_ERROR("Invalid lanes %d\n", lanes);
		lanes = 1;
	}

	/* Initialize link parameters */
	memset(dptx->link.preemp_level, 0, sizeof(u8) * 4);
	memset(dptx->link.vswing_level, 0, sizeof(u8) * 4);
	memset(dptx->link.status, 0, DP_LINK_STATUS_SIZE);

	sink_max_lanes = drm_dp_max_lane_count(dptx->rx_caps);

	if (lanes > sink_max_lanes)
		lanes = sink_max_lanes;

	sink_max_rate = dptx->rx_caps[DP_MAX_LINK_RATE];
	sink_max_rate = dptx_bw_to_phy_rate(sink_max_rate);

	if (rate > sink_max_rate)
		rate = sink_max_rate;

	dptx->link.lanes = lanes;
	dptx->link.rate = rate;
	dptx->link.trained = false;

	return 0;
}

int dptx_link_training_pattern_set(struct dptx *dptx, u8 pattern)
{
	int ret_dpcd;

	ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev,
					DP_TRAINING_PATTERN_SET,
					pattern);
	if (ret_dpcd < 0)
		return ret_dpcd;

	return 0;
}

static int dptx_link_training_start(struct dptx *dptx)
{
	int retval;
	int ret_dpcd;
	u32 cctl, phyifctrl;
	u8 byte;

	/* Initialize PHY */

	/* Move PHY to P3 */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

	phyifctrl |= (3 << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);

	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	retval = dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);
	if (retval) {
		DRM_ERROR("Timed out waiting for PHY BUSY\n");
		return retval;
	}

	dptx_phy_set_lanes(dptx, dptx->link.lanes);
	dptx_phy_set_rate(dptx, dptx->link.rate);
	udelay(100);

	/* Move PHY to P0 */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

	phyifctrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;

	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	/* Wait for PHY busy */
	retval = dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);
	if (retval) {
		DRM_ERROR("Timed out waiting for PHY BUSY\n");
		return retval;
	}

	/* Set PHY_TX_EQ */
	dptx_link_set_preemp_vswing(dptx);

	dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_NONE);
	retval = dptx_link_training_pattern_set(dptx,
						DP_TRAINING_PATTERN_DISABLE);
	if (retval)
		return retval;

	dptx_phy_enable_xmit(dptx, dptx->link.lanes, true);

	retval = dptx_phy_rate_to_bw(dptx->link.rate);
	if (retval < 0)
		return retval;

	byte = retval;
	ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev, DP_LINK_BW_SET, byte);
	if (ret_dpcd < 0)
		return ret_dpcd;

	byte = dptx->link.lanes;
	cctl = dptx_readl(dptx, DPTX_CCTL);

	if (drm_dp_enhanced_frame_cap(dptx->rx_caps)) {
		byte |= DP_ENHANCED_FRAME_CAP;
		cctl |= DPTX_CCTL_ENH_FRAME_EN;
	} else {
		cctl &= ~DPTX_CCTL_ENH_FRAME_EN;
	}

	dptx_writel(dptx, DPTX_CCTL, cctl);

	ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev, DP_LANE_COUNT_SET, byte);
	if (ret_dpcd < 0)
		return ret_dpcd;

	/* check if SSC is enabled and program DPCD */
	if (dptx->ssc_en && dptx_sink_enabled_ssc(dptx))
		byte = DP_SPREAD_AMP_0_5;
	else
		byte = 0;

	ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev, DP_DOWNSPREAD_CTRL, byte);
	if (ret_dpcd < 0)
		return ret_dpcd;

	byte = 1;
	ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev,
				DP_MAIN_LINK_CHANNEL_CODING_SET,
			    byte);
	if (ret_dpcd < 0)
		return ret_dpcd;

	return 0;
}

int dptx_link_wait_cr_and_adjust(struct dptx *dptx, bool ch_eq)
{
	int i;
	int retval;
	int changed = 0;
	bool done = false;

	DRM_DEBUG("%s:\n", __func__);

	retval = dptx_link_check_cr_done(dptx, &done);
	if (retval)
		return retval;

	if (done)
		return 0;

	/* Try each adjustment setting 5 times */
	for (i = 0; i < 5; i++) {
		retval = dptx_link_adjust_drive_settings(dptx, &changed);
		if (retval)
			return retval;

		/* Reset iteration count if we changed settings */
		if (changed)
			i = 0;

		retval = dptx_link_check_cr_done(dptx, &done);
		if (retval)
			return retval;

		if (done)
			return 0;
	}

	return -EPROTO;
}

int dptx_link_cr(struct dptx *dptx)
{
	int retval;

	DRM_DEBUG("%s:\n", __func__);

	dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_1);

	retval = dptx_link_training_pattern_set(dptx,
						DP_TRAINING_PATTERN_1 | 0x20);
	if (retval)
		return retval;

	return dptx_link_wait_cr_and_adjust(dptx, false);
}

int dptx_link_ch_eq(struct dptx *dptx)
{
	int retval;
	bool cr_done;
	bool ch_eq_done;
	u32 pattern;
	u32 i;
	u8 dp_pattern;

	DRM_DEBUG("%s:\n", __func__);

	switch (dptx->max_rate) {
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		if (drm_dp_tps4_supported(dptx->rx_caps)) {
			pattern = DPTX_PHYIF_CTRL_TPS_4;
			dp_pattern = DP_TRAINING_PATTERN_4;
			break;
		}
		/* fall through */
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		if (drm_dp_tps3_supported(dptx->rx_caps)) {
			pattern = DPTX_PHYIF_CTRL_TPS_3;
			dp_pattern = DP_TRAINING_PATTERN_3;
			break;
		}
		/* fall through */
	case DPTX_PHYIF_CTRL_RATE_RBR:
	case DPTX_PHYIF_CTRL_RATE_HBR:
		pattern = DPTX_PHYIF_CTRL_TPS_2;
		dp_pattern = DP_TRAINING_PATTERN_2;
		break;
	default:
		DRM_ERROR("Invalid rate %d\n", dptx->link.rate);
		return -EINVAL;
	}

	dptx_phy_set_pattern(dptx, pattern);

	/* TODO this needs to be different for other versions of
	 * DPRX
	 */
	if (dp_pattern != DP_TRAINING_PATTERN_4) {
		retval = dptx_link_training_pattern_set(dptx,
							dp_pattern | 0x20);
	} else {
		retval = dptx_link_training_pattern_set(dptx, dp_pattern);

		DRM_DEBUG("%s:  Enabling scrambling for TPS4\n", __func__);
	}
	if (retval)
		return retval;

	for (i = 0; i < 6; i++) {
		retval = dptx_link_check_ch_eq_done(dptx,
						    &cr_done, &ch_eq_done);

		if (retval)
			return retval;

		if (!cr_done)
			return -EPROTO;

		if (ch_eq_done)
			return 0;

		retval = dptx_link_adjust_drive_settings(dptx, NULL);
		if (retval)
			return retval;
	}

	return -EPROTO;
}

int dptx_link_reduce_rate(struct dptx *dptx)
{
	u32 rate = dptx->link.rate;

	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		return -EPROTO;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		rate = DPTX_PHYIF_CTRL_RATE_RBR;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		rate = DPTX_PHYIF_CTRL_RATE_HBR;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		rate = DPTX_PHYIF_CTRL_RATE_HBR2;
		break;
	}

	DRM_DEBUG("%s: Reducing rate from %d to %d\n",
		 __func__, dptx->link.rate, rate);
	dptx->link.rate = rate;

	return 0;
}

int dptx_link_reduce_lanes(struct dptx *dptx)
{
	u32 lanes;

	switch (dptx->link.lanes) {
	case 4:
		lanes = 2;
		break;
	case 2:
		lanes = 1;
		break;
	case 1:
	default:
		return -EPROTO;
	}

	DRM_DEBUG("%s: Reducing lanes from %d to %d\n",
		 __func__, dptx->link.lanes, lanes);
	dptx->link.lanes = lanes;
	dptx->link.rate = dptx->max_rate;
	return 0;
}

int dptx_link_training(struct dptx *dptx, u8 rate, u8 lanes)
{
	int retval, retval1;
	u32 hpd_sts;

	retval = dptx_link_training_init(dptx, rate, lanes);
	if (retval)
		goto fail;

again:
	DRM_DEBUG("%s: Starting link training\n", __func__);
	retval = dptx_link_training_start(dptx);
	if (retval)
		goto fail;

	retval = dptx_link_cr(dptx);
	if (retval) {
		if (retval == -EPROTO) {
			if (dptx_link_reduce_rate(dptx)) {
				if (dptx_link_reduce_lanes(dptx)) {
					retval = -EPROTO;
					goto fail;
				}
			}

			dptx_link_training_init(dptx,
						dptx->link.rate,
						dptx->link.lanes);
			goto again;
		} else {
			goto fail;
		}
	}

	retval = dptx_link_ch_eq(dptx);
	if (retval) {
		if (retval == -EPROTO) {
			if (dptx_link_reduce_rate(dptx)) {
				if (dptx_link_reduce_lanes(dptx)) {
					retval = -EPROTO;
					goto fail;
				}
			}

			dptx_link_training_init(dptx,
						dptx->link.rate,
						dptx->link.lanes);
			goto again;
		} else {
			goto fail;
		}
	}

	dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_NONE);

	retval = dptx_link_training_pattern_set(dptx,
						DP_TRAINING_PATTERN_DISABLE);
	if (retval)
		goto fail;

	dptx_phy_enable_xmit(dptx, dptx->link.lanes, true);
	dptx->link.trained = true;

	DRM_INFO("Link training succeeded rate=%d lanes=%d\n",
		 dptx->link.rate, dptx->link.lanes);

	return 0;

fail:
	hpd_sts = dptx_readl(dptx, DPTX_HPDSTS);

	if (hpd_sts & DPTX_HPDSTS_STATUS) {
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_NONE);
		retval1 =
		    dptx_link_training_pattern_set(dptx,
						   DP_TRAINING_PATTERN_DISABLE);
		DRM_ERROR("Link training failed %d\n", retval);
		if (retval1)
			return retval1;
	} else {
		DRM_ERROR("Link training failed as sink is disconnected %d\n",
			 retval);
	}
	return retval;
}

int dptx_link_check_status(struct dptx *dptx)
{
	int ret_dpcd;
	u8 byte;
	u8 bytes[2];

	ret_dpcd = drm_dp_dpcd_read(&dptx->aux_dev, DP_SINK_COUNT, bytes, 2);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd = drm_dp_dpcd_read_link_status(&dptx->aux_dev,
				dptx->link.status);
	if (ret_dpcd < 0)
		return ret_dpcd;

	byte = dptx->link.status[DP_LANE_ALIGN_STATUS_UPDATED -
				 DP_LANE0_1_STATUS];

	if (!(byte & DP_LINK_STATUS_UPDATED))
		return 0;

	/* Check if need to retrain link */
	if (dptx->link.trained &&
	    (!drm_dp_channel_eq_ok(dptx->link.status, dptx->link.lanes) ||
	     !drm_dp_clock_recovery_ok(dptx->link.status, dptx->link.lanes))) {
		DRM_DEBUG("%s: Retraining link\n", __func__);

		return dptx_link_training(dptx,
					  DPTX_MAX_LINK_RATE,
					  DPTX_MAX_LINK_LANES);
	}

	return 0;
}
