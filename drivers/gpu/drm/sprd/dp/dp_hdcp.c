// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */


#include "sprd_hdcp.h"
#include "dw_dptx.h"
#include "dp_api.h"

void dptx_en_dis_hdcp13(struct dptx *dptx, u8 enable)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_HDCP_CFG);
	if (enable == 1) {
		reg |= DPTX_CFG_EN_HDCP13;
		reg |= DPTX_CFG_EN_HDCP;
	} else {
		reg &= ~DPTX_CFG_EN_HDCP13;
		reg &= ~DPTX_CFG_EN_HDCP;
	}
	dptx_writel(dptx, DPTX_HDCP_CFG, reg);
}

int dptx_configure_hdcp13(void)
{
	int retval;
	u8 rev, major_n, minor_n;
	u32 reg;
	struct dptx *dptx = dptx_get_handle();

	retval = drm_dp_dpcd_readb(&dptx->aux_dev, DP_DPCD_REV, &rev);
	if (retval)
		return retval;

	major_n = (rev & 0xf0) >> 4;
	minor_n = rev & 0xf;

	reg = dptx_readl(dptx, DPTX_HDCP_CFG);
	reg &= ~DPTX_HDCP_CFG_DPCD_PLUS_MASK;
	if (major_n == 1 && minor_n >= 2)
		reg |= 1 << DPTX_HDCP_CFG_DPCD_PLUS_SHIFT;

	dptx_writel(dptx, DPTX_HDCP_CFG, reg);
	dptx_en_dis_hdcp13(dptx, 1);

	return 0;
}

int dptx_en_hdcp22(struct dptx *dptx)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_HDCP_CFG);
	reg |= DPTX_CFG_EN_HDCP;
	dptx_writel(dptx, DPTX_HDCP_CFG, reg);

	return 0;
}

void dptx_en_dis_hdcp22(struct dptx *dptx, u8 enable)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_HDCP_CFG);
	if (enable == 1) {
		reg |= DPTX_CFG_EN_HDCP;
	} else {
		reg &= ~DPTX_CFG_EN_HDCP;
	}
	dptx_writel(dptx, DPTX_HDCP_CFG, reg);
}

