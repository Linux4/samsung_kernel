// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/regmap.h>
#include "dw_dptx.h"

static int handle_test_link_training(struct dptx *dptx)
{
	struct video_params *vparams;
	struct dtd *mdtd;
	int retval;
	int ret_dpcd;
	u8 lanes = 0;
	u8 rate = 0;
	u32 phyifctrl;

	dptx_enable_ssc(dptx);

	/* Move to P0 */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_LINK_RATE, &rate);
	if (ret_dpcd < 0)
		return ret_dpcd;

	retval = dptx_bw_to_phy_rate(rate);
	if (retval < 0)
		return retval;

	rate = retval;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_LANE_COUNT, &lanes);
	if (ret_dpcd < 0)
		return ret_dpcd;

	DRM_DEBUG("%s: Strating link training rate=%d, lanes=%d\n",
		 __func__, rate, lanes);

	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;

	retval = dptx_video_ts_calculate(dptx, lanes, rate,
					 vparams->bpc, vparams->pix_enc,
					 mdtd->pixel_clock);
	if (retval)
		return retval;

	retval = dptx_link_training(dptx, rate, lanes);
	if (retval)
		DRM_ERROR("Link training failed %d\n", retval);
	else
		DRM_DEBUG("Link training succeeded\n");

	return retval;
}

static int handle_test_link_video_pattern(struct dptx *dptx, int stream)
{
	int retval;
	int ret_dpcd;
	u8 pattern, bpc, bpc_map, dynamic_range,
	    dynamic_range_map, color_format, color_format_map,
	    ycbcr_coeff, ycbcr_coeff_map, misc = 0;
	struct video_params *vparams;
	struct dtd *mdtd;

	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;
	retval = 0;

	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_PATTERN, &pattern);
	if (ret_dpcd < 0)
		return ret_dpcd;
	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_MISC, &misc);
	if (ret_dpcd < 0)
		return ret_dpcd;

	dynamic_range = (misc & DP_TEST_DYNAMIC_RANGE_MASK)
	    >> DP_TEST_DYNAMIC_RANGE_SHIFT;
	switch (dynamic_range) {
	case DP_TEST_DYNAMIC_RANGE_VESA:
		DRM_DEBUG("DP_TEST_DYNAMIC_RANGE_VESA\n");
		dynamic_range_map = VESA;
		break;
	case DP_TEST_DYNAMIC_RANGE_CEA:
		DRM_DEBUG("DP_TEST_DYNAMIC_RANGE_CEA\n");
		dynamic_range_map = CEA;
		break;
	default:
		DRM_DEBUG("Invalid TEST_BIT_DEPTH\n");
		return -EINVAL;
	}

	ycbcr_coeff = (misc & DP_TEST_YCBCR_COEFF_MASK)
	    >> DP_TEST_YCBCR_COEFF_SHIFT;

	switch (ycbcr_coeff) {
	case DP_TEST_YCBCR_COEFF_ITU601:
		DRM_DEBUG("DP_TEST_YCBCR_COEFF_ITU601\n");
		ycbcr_coeff_map = ITU601;
		break;
	case DP_TEST_YCBCR_COEFF_ITU709:
		DRM_DEBUG("DP_TEST_YCBCR_COEFF_ITU709:\n");
		ycbcr_coeff_map = ITU709;
		break;
	default:
		DRM_DEBUG("Invalid TEST_BIT_DEPTH\n");
		return -EINVAL;
	}
	color_format = misc & DP_TEST_COLOR_FORMAT_MASK;

	switch (color_format) {
	case DP_TEST_COLOR_FORMAT_RGB:
		DRM_DEBUG("DP_TEST_COLOR_FORMAT_RGB\n");
		color_format_map = RGB;
		break;
	case DP_TEST_COLOR_FORMAT_YCBCR422:
		DRM_DEBUG("DP_TEST_COLOR_FORMAT_YCBCR422\n");
		color_format_map = YCBCR422;
		break;
	case DP_TEST_COLOR_FORMAT_YCBCR444:
		DRM_DEBUG("DP_TEST_COLOR_FORMAT_YCBCR444\n");
		color_format_map = YCBCR444;
		break;
	default:
		DRM_DEBUG("Invalid  DP_TEST_COLOR_FORMAT\n");
		return -EINVAL;
	}

	bpc = (misc & DP_TEST_BIT_DEPTH_MASK)
	    >> DP_TEST_BIT_DEPTH_SHIFT;

	switch (bpc) {
	case DP_TEST_BIT_DEPTH_6:
		bpc_map = COLOR_DEPTH_6;
		DRM_DEBUG("TEST_BIT_DEPTH_6\n");
		break;
	case DP_TEST_BIT_DEPTH_8:
		bpc_map = COLOR_DEPTH_8;
		DRM_DEBUG("TEST_BIT_DEPTH_8\n");
		break;
	case DP_TEST_BIT_DEPTH_10:
		bpc_map = COLOR_DEPTH_10;
		DRM_DEBUG("TEST_BIT_DEPTH_10\n");
		break;
	case DP_TEST_BIT_DEPTH_12:
		bpc_map = COLOR_DEPTH_12;
		DRM_DEBUG("TEST_BIT_DEPTH_12\n");
		break;
	case DP_TEST_BIT_DEPTH_16:
		bpc_map = COLOR_DEPTH_16;
		DRM_DEBUG("TEST_BIT_DEPTH_16\n");
		break;
	default:
		DRM_DEBUG("Invalid TEST_BIT_DEPTH\n");
		return -EINVAL;
	}

	vparams->dynamic_range = dynamic_range_map;
	DRM_DEBUG("Change video dynamic range to %d\n", dynamic_range_map);

	vparams->colorimetry = ycbcr_coeff_map;
	DRM_DEBUG("Change video colorimetry to %d\n", ycbcr_coeff_map);

	retval = dptx_video_ts_calculate(dptx, dptx->link.lanes,
					 dptx->link.rate,
					 bpc_map, color_format_map,
					 mdtd->pixel_clock);
	if (retval)
		return retval;

	vparams->pix_enc = color_format_map;
	DRM_DEBUG("Change pixel encoding to %d\n", color_format_map);

	vparams->bpc = bpc_map;
	dptx_video_bpc_change(dptx, stream);
	DRM_DEBUG("Change bits per component to %d\n", bpc_map);

	dptx_video_ts_change(dptx, stream);

	switch (pattern) {
	case DP_TEST_PATTERN_NONE:
		DRM_DEBUG("TEST_PATTERN_NONE %d\n", pattern);
		break;
	case DP_TEST_PATTERN_COLOR_RAMPS:
		DRM_DEBUG("TEST_PATTERN_COLOR_RAMPS %d\n", pattern);
		vparams->pattern_mode = RAMP;
		DRM_DEBUG("Change video pattern to RAMP\n");
		break;
	case DP_TEST_PATTERN_BW_VERITCAL_LINES:
		DRM_DEBUG("TEST_PATTERN_BW_VERTICAL_LINES %d\n", pattern);
		break;
	case DP_TEST_PATTERN_COLOR_SQUARE:
		DRM_DEBUG("TEST_PATTERN_COLOR_SQUARE %d\n", pattern);
		vparams->pattern_mode = COLRAMP;
		DRM_DEBUG("Change video pattern to COLRAMP\n");
		break;
	default:
		DRM_DEBUG("Invalid TEST_PATTERN %d\n", pattern);
		return -EINVAL;
	}

	return 0;
}

static int dptx_set_custom_pattern(struct dptx *dptx)
{
	int ret_dpcd;
	u8 pattern0, pattern1, pattern2, pattern3, pattern4, pattern5,
	    pattern6, pattern7, pattern8, pattern9;

	u32 custompat0;
	u32 custompat1;
	u32 custompat2;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_0,
			      &pattern0);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_1,
			      &pattern1);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_2,
			      &pattern2);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_3,
			      &pattern3);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_4,
			      &pattern4);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_5,
			      &pattern5);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_6,
			      &pattern6);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_7,
			      &pattern7);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_8,
			      &pattern8);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_80BIT_CUSTOM_PATTERN_9,
			      &pattern9);
	if (ret_dpcd < 0)
		return ret_dpcd;

	/*
	 * Calculate 30,30 and 20 bits custom patterns
	 * depending on TEST_80BIT_CUSTOM_PATTERN sequence
	 */
	custompat0 =
	    ((((((pattern3 & (0xff >> 2)) << 8) | pattern2) << 8) |
	     pattern1) << 8) | pattern0;
	custompat1 =
	    ((((((((pattern7 & (0xf)) << 8) | pattern6) << 8) | pattern5) << 8)
	      | pattern4) << 2) | ((pattern3 >> 6) & 0x3);
	custompat2 =
	    (((pattern9 << 8) | pattern8) << 4) | ((pattern7 >> 4) & 0xf);

	dptx_writel(dptx, DPTX_CUSTOMPAT0, custompat0);
	dptx_writel(dptx, DPTX_CUSTOMPAT1, custompat1);
	dptx_writel(dptx, DPTX_CUSTOMPAT2, custompat2);

	return 0;
}

static int adjust_vswing_and_preemphasis(struct dptx *dptx)
{
	int ret_dpcd, i;
	u8 lane_01 = 0;
	u8 lane_23 = 0;
	u8 pe = 0;
	u8 vs = 0;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_ADJUST_REQUEST_LANE0_1,
			      &lane_01);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_ADJUST_REQUEST_LANE2_3,
			      &lane_23);
	if (ret_dpcd < 0)
		return ret_dpcd;

	for (i = 0; i < dptx->link.lanes; i++) {
		switch (i) {
		case 0:
			pe = (lane_01 & DP_ADJUST_PRE_EMPHASIS_LANE0_MASK)
			    >> DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
			vs = (lane_01 & DP_ADJUST_VOLTAGE_SWING_LANE0_MASK)
			    >> DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT;
			break;
		case 1:
			pe = (lane_01 & DP_ADJUST_PRE_EMPHASIS_LANE1_MASK)
			    >> DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT;
			vs = (lane_01 & DP_ADJUST_VOLTAGE_SWING_LANE1_MASK)
			    >> DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT;
			break;
		case 2:
			pe = (lane_23 & DP_ADJUST_PRE_EMPHASIS_LANE0_MASK)
			    >> DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
			vs = (lane_23 & DP_ADJUST_VOLTAGE_SWING_LANE0_MASK)
			    >> DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT;
			break;
		case 3:
			pe = (lane_23 & DP_ADJUST_PRE_EMPHASIS_LANE1_MASK)
			    >> DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT;
			vs = (lane_23 & DP_ADJUST_VOLTAGE_SWING_LANE1_MASK)
			    >> DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT;
			break;
		default:
			break;
		}

		dptx_phy_set_pre_emphasis(dptx, i, pe);
		dptx_phy_set_vswing(dptx, i, vs);
	}

	/*
	 * 0x8 bit7:PHY_REG_EN; bit0~15:PHY_REG_ADDR.
	 * 0xc bit0-31: PHY_REG_DATA.
	 */

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

	return 0;
}

static int handle_test_phy_pattern(struct dptx *dptx)
{
	u8 pattern = 0;
	int retval;
	int ret_dpcd;

	ret_dpcd =
	    drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_PHY_PATTERN, &pattern);
	if (ret_dpcd < 0)
		return ret_dpcd;

	pattern &= DP_TEST_PHY_PATTERN_SEL_MASK;

	switch (pattern) {
	case DP_TEST_PHY_PATTERN_NONE:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("No test pattern selected\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_NONE);
		break;
	case DP_TEST_PHY_PATTERN_D10:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("D10.2 without scrambling test phy pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_1);
		break;
	case DP_TEST_PHY_PATTERN_SEMC:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("Symbol error measurement count test phy pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_SYM_ERM);
		break;
	case DP_TEST_PHY_PATTERN_PRBS7:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("PRBS7 test phy pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_PRBS7);
		break;
	case DP_TEST_PHY_PATTERN_CUSTOM:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("80-bit custom pattern transmitted test phy pattern\n");

		retval = dptx_set_custom_pattern(dptx);
		if (retval)
			return retval;
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_CUSTOM80);
		break;
	case DP_TEST_PHY_PATTERN_CP2520_1:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("CP2520_1 - HBR2 Compliance EYE pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_CP2520_1);
		break;
	case DP_TEST_PHY_PATTERN_CP2520_2:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("CP2520_2 - pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_CP2520_2);
		break;
	case DP_TEST_PHY_PATTERN_CP2520_3_TPS4:
		retval = adjust_vswing_and_preemphasis(dptx);
		if (retval)
			return retval;
		DRM_DEBUG("DP_TEST_PHY_PATTERN_CP2520_3_TPS4 - pattern\n");
		dptx_phy_set_pattern(dptx, DPTX_PHYIF_CTRL_TPS_4);
		break;
	default:
		DRM_DEBUG("Invalid TEST_PHY_PATTERN\n");
		return -EINVAL;
	}
	return retval;
}

int handle_automated_test_request(struct dptx *dptx)
{
	int retval;
	int ret_dpcd;
	u8 test = 0;

	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev, DP_TEST_REQUEST, &test);
	if (ret_dpcd < 0)
		return ret_dpcd;

	if (test & DP_TEST_LINK_TRAINING) {
		DRM_DEBUG("%s: DP_TEST_LINK_TRAINING\n", __func__);

		ret_dpcd =
		    drm_dp_dpcd_writeb(&dptx->aux_dev, DP_TEST_RESPONSE,
				       DP_TEST_ACK);
		if (ret_dpcd < 0)
			return ret_dpcd;

		retval = handle_test_link_training(dptx);
		if (retval)
			return retval;
	}

	if (test & DP_TEST_LINK_VIDEO_PATTERN) {
		DRM_DEBUG("%s:DP_TEST_LINK_VIDEO_PATTERN\n", __func__);

		ret_dpcd =
		    drm_dp_dpcd_writeb(&dptx->aux_dev, DP_TEST_RESPONSE,
				       DP_TEST_ACK);
		if (ret_dpcd < 0)
			return ret_dpcd;

		retval = handle_test_link_video_pattern(dptx, 0);
		if (retval)
			return retval;
	}

	if (test & DP_TEST_LINK_EDID_READ) {
		/* Invalid, this should happen on HOTPLUG */
		DRM_DEBUG("%s:DP_TEST_LINK_EDID_READ\n", __func__);
		return -ENOTSUPP;
	}
	if (test & DP_TEST_LINK_PHY_TEST_PATTERN) {
		DRM_DEBUG("%s:DP_TEST_LINK_PHY_TEST_PATTERN\n", __func__);
		retval = handle_test_phy_pattern(dptx);
		if (retval)
			return retval;
	}
	return 0;
}

