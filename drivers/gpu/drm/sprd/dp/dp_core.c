// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "dw_dptx.h"

/*
 * Core Access Layer
 *
 * Provides low-level register access to the DPTX core.
 */

/**
 * dptx_intr_en() - Enables interrupts
 * @dptx: The dptx struct
 * @bits: The interrupts to enable
 *
 * This function enables (unmasks) all interrupts in the INTERRUPT
 * register specified by @bits.
 */
static void dptx_intr_en(struct dptx *dptx, u32 bits)
{
	u32 ien;

	DRM_DEBUG("%s:\n", __func__);
	ien = dptx_readl(dptx, DPTX_IEN);
	ien |= bits;
	dptx_writel(dptx, DPTX_IEN, ien);
}

/**
 * dptx_intr_dis() - Disables interrupts
 * @dptx: The dptx struct
 * @bits: The interrupts to disable
 *
 * This function disables (masks) all interrupts in the INTERRUPT
 * register specified by @bits.
 */
static void dptx_intr_dis(struct dptx *dptx, u32 bits)
{
	u32 ien;

	DRM_DEBUG("%s:\n", __func__);
	ien = dptx_readl(dptx, DPTX_IEN);
	ien &= ~bits;
	dptx_writel(dptx, DPTX_IEN, ien);
}

/**
 * dptx_global_intr_en() - Enables top-level interrupts
 * @dptx: The dptx struct
 *
 * Enables (unmasks) all top-level interrupts.
 */
void dptx_global_intr_en(struct dptx *dptx)
{
	dptx_intr_en(dptx, DPTX_IEN_ALL_INTR &
		     ~(DPTX_ISTS_AUX_REPLY | DPTX_ISTS_AUX_CMD_INVALID));
}

/**
 * dptx_global_intr_dis() - Disables top-level interrupts
 * @dptx: The dptx struct
 *
 * Disables (masks) all top-level interrupts.
 */
void dptx_global_intr_dis(struct dptx *dptx)
{
	dptx_intr_dis(dptx, DPTX_IEN_ALL_INTR);
}

/**
 * dptx_soft_reset() - Performs a core soft reset
 * @dptx: The dptx struct
 * @bits: The components to reset
 *
 * Resets specified parts of the core by writing @bits into the core
 * soft reset control register and clearing them 10-20 microseconds
 * later.
 */
void dptx_soft_reset(struct dptx *dptx, u32 bits)
{
	u32 rst;

	bits &= (DPTX_SRST_CTRL_ALL);

	/* Set reset bits */
	rst = dptx_readl(dptx, DPTX_SRST_CTRL);
	rst |= bits;
	dptx_writel(dptx, DPTX_SRST_CTRL, rst);

	udelay(20);

	/* Clear reset bits */
	rst = dptx_readl(dptx, DPTX_SRST_CTRL);
	rst &= ~bits;
	dptx_writel(dptx, DPTX_SRST_CTRL, rst);
}

/**
 * dptx_soft_reset_all() - Reset all core modules
 * @dptx: The dptx struct
 */
void dptx_soft_reset_all(struct dptx *dptx)
{
	dptx_soft_reset(dptx, DPTX_SRST_CTRL_ALL);
}

void dptx_phy_soft_reset(struct dptx *dptx)
{
	dptx_soft_reset(dptx, DPTX_SRST_CTRL_PHY);
}

/**
 * dptx_core_init_phy() - Initializes the DP TX PHY module
 * @dptx: The dptx struct
 *
 * Initializes the PHY layer of the core. This needs to be called
 * whenever the PHY layer is reset.
 */
void dptx_core_init_phy(struct dptx *dptx)
{
	u32 phyifctrl;

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_WIDTH;
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
}

/**
 * dptx_sink_enabled_ssc() - Returns true, if sink is enabled ssc
 * @dptx: The dptx struct
 *
 */
bool dptx_sink_enabled_ssc(struct dptx *dptx)
{
	u8 byte = 0;

	drm_dp_dpcd_readb(&dptx->aux_dev, DP_MAX_DOWNSPREAD, &byte);

	return byte & 1;
}

/**
 * dptx_core_program_ssc() - Move phy to P3 state and programs SSC
 * @dptx: The dptx struct
 *
 * Enables SSC should be called during hot plug.
 *
 */
int dptx_core_program_ssc(struct dptx *dptx, bool sink_ssc)
{
	u32 phyifctrl;
	u8 retval;

	/* Enable 4 lanes, before programming SSC */
	dptx_phy_set_lanes(dptx, 4);

	/* Move PHY to P3 to program SSC */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl |= (3 << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	retval = dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);
	if (retval) {
		DRM_ERROR("Timed out waiting for PHY BUSY\n");
		return retval;
	}

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	if (dptx->ssc_en && sink_ssc)
		phyifctrl &= ~DPTX_PHYIF_CTRL_SSC_DIS;
	else
		phyifctrl |= DPTX_PHYIF_CTRL_SSC_DIS;

	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	retval = dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);
	if (retval) {
		DRM_ERROR("Timed out waiting for PHY BUSY\n");
		return retval;
	}

	return 0;
}

/**
 * dptx_enable_ssc() - Enables SSC based on automation request,
 * if DPTX controller enables ssc
 *
 * @dptx: The dptx struct
 */
void dptx_enable_ssc(struct dptx *dptx)
{
	bool sink_ssc = dptx_sink_enabled_ssc(dptx);

	if (sink_ssc)
		DRM_DEBUG("%s: SSC enable on the sink side\n", __func__);
	else
		DRM_DEBUG("%s: SSC disabled on the sink side\n", __func__);
	dptx_core_program_ssc(dptx, sink_ssc);
}

void dptx_init_hwparams(struct dptx *dptx)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_CONFIG1);

	/* Num MST streams */
	dptx->streams = (reg & DPTX_CONFIG1_NUM_STREAMS_MASK) >>
		DPTX_CONFIG1_NUM_STREAMS_SHIFT;

	/* Combo PHY */
	dptx->hwparams.gen2phy = !!(reg & DPTX_CONFIG1_GEN2_PHY);

	/* DSC */
	dptx->hwparams.dsc = !!(reg & DPTX_CONFIG1_DSC_EN);

	/* Multi pixel mode */
	switch ((reg & DPTX_CONFIG1_MP_MODE_MASK) >>
					DPTX_CONFIG1_MP_MODE_SHIFT) {
	default:
	case DPTX_CONFIG1_MP_MODE_SINGLE:
		dptx->hwparams.multipixel = DPTX_MP_SINGLE_PIXEL;
		break;
	case DPTX_CONFIG1_MP_MODE_DUAL:
		dptx->hwparams.multipixel = DPTX_MP_DUAL_PIXEL;
		break;
	case DPTX_CONFIG1_MP_MODE_QUAD:
		dptx->hwparams.multipixel = DPTX_MP_QUAD_PIXEL;
		break;
	}
}

/**
 * dptx_core_init() - Initializes the DP TX core
 * @dptx: The dptx struct
 *
 * Initialize the DP TX core and put it in a known state.
 */
int dptx_core_init(struct dptx *dptx)
{
	char str[15];
	u32 version;
	u32 hpd_ien;

	/* Reset the core */
	dptx_soft_reset_all(dptx);

	/* Enable MST */
	dptx_writel(dptx, DPTX_CCTL,
		    DPTX_CCTL_ENH_FRAME_EN |
		    (dptx->mst ? DPTX_CCTL_ENABLE_MST_MODE : 0));

	/* Check the core version */
	memset(str, 0, sizeof(str));
	version = dptx_readl(dptx, DPTX_VER_NUMBER);
	str[0] = (version >> 24) & 0xff;
	str[1] = '.';
	str[2] = (version >> 16) & 0xff;
	str[3] = (version >> 8) & 0xff;
	str[4] = version & 0xff;

	version = dptx_readl(dptx, DPTX_VER_TYPE);
	str[5] = '-';
	str[6] = (version >> 24) & 0xff;
	str[7] = (version >> 16) & 0xff;
	str[8] = (version >> 8) & 0xff;
	str[9] = version & 0xff;

	DRM_DEBUG("Core version: %s\n", str);
	dptx->version = version;

	/* Enable all HPD interrupts */
	hpd_ien = dptx_readl(dptx, DPTX_HPD_IEN);
	hpd_ien |= (DPTX_HPD_IEN_IRQ_EN |
		    DPTX_HPD_IEN_HOT_PLUG_EN |
		    DPTX_HPD_IEN_HOT_UNPLUG_EN);

	dptx_writel(dptx, DPTX_HPD_IEN, hpd_ien);

	/* Enable all top-level interrupts */
	dptx_global_intr_en(dptx);

	dptx_writel(dptx, DPTX_TYPE_C_CTRL, 0x5);

	return 0;
}

/**
 * dptx_core_deinit() - Deinitialize the core
 * @dptx: The dptx struct
 *
 * Disable the core in preparation for module shutdown.
 */
int dptx_core_deinit(struct dptx *dptx)
{
	dptx_global_intr_dis(dptx);
	dptx_soft_reset_all(dptx);
	return 0;
}

/*
 * PHYIF core access functions
 */

u32 dptx_phy_get_lanes(struct dptx *dptx)
{
	u32 phyifctrl;
	u32 val;

	DRM_DEBUG("%s:\n", __func__);

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	val = (phyifctrl & DPTX_PHYIF_CTRL_LANES_MASK) >>
		DPTX_PHYIF_CTRL_LANES_SHIFT;

	return (1 << val);
}

void dptx_phy_set_lanes(struct dptx *dptx, u32 lanes)
{
	u32 phyifctrl;
	u32 val;

	DRM_DEBUG("%s: lanes=%d\n", __func__, lanes);

	switch (lanes) {
	case 1:
		val = 0;
		break;
	case 2:
		val = 1;
		break;
	case 4:
		val = 2;
		break;
	default:
		DRM_ERROR("Invalid number of lanes %d\n", lanes);
		return;
	}

	phyifctrl = 0;
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

	phyifctrl &= ~DPTX_PHYIF_CTRL_LANES_MASK;
	phyifctrl |= (val << DPTX_PHYIF_CTRL_LANES_SHIFT);
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
}

void dptx_phy_set_rate(struct dptx *dptx, u32 rate)
{
	u32 phyifctrl;

	DRM_DEBUG("%s: rate=%d\n", __func__, rate);

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_RATE_MASK;
	phyifctrl |= rate << DPTX_PHYIF_CTRL_RATE_SHIFT;
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
}

u32 dptx_phy_get_rate(struct dptx *dptx)
{
	u32 phyifctrl;
	u32 rate;

	DRM_DEBUG("%s:\n", __func__);

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	rate = (phyifctrl & DPTX_PHYIF_CTRL_RATE_MASK) >>
		DPTX_PHYIF_CTRL_RATE_SHIFT;

	return rate;
}

int dptx_phy_wait_busy(struct dptx *dptx, u32 lanes)
{
	u32 count;
	u32 phyifctrl;
	u32 mask = 0;

	DRM_DEBUG("%s: lanes=%d\n", __func__, lanes);

	switch (lanes) {
	case 4:
		mask |= DPTX_PHYIF_CTRL_BUSY(3);
		mask |= DPTX_PHYIF_CTRL_BUSY(2);
		/* fall through */
	case 2:
		mask |= DPTX_PHYIF_CTRL_BUSY(1);
		/* fall through */
	case 1:
		mask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	default:
		DRM_ERROR("Invalid number of lanes %d\n", lanes);
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	}

	count = 0;

	while (1) {
		phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

		if (!(phyifctrl & mask))
			break;

		count++;
		if (count > 100000) {
			DRM_WARN("%s: PHY BUSY timed out\n", __func__);
			return 0;
		}

		udelay(1);
	}

	return 0;
}

void dptx_phy_set_pre_emphasis(struct dptx *dptx,
			       u32 lane,
			       u32 level)
{
	u32 phytxeq;

	DRM_DEBUG("%s: lane=%d, level=0x%x\n", __func__, lane, level);

	if (lane > 3) {
		DRM_ERROR("Invalid lane %d", lane);
		return;
	}

	if (level > 3) {
		DRM_ERROR("Invalid pre-emphasis level %d, using 3", level);
		level = 3;
	}

	phytxeq = dptx_readl(dptx, DPTX_PHY_TX_EQ);
	phytxeq &= ~DPTX_PHY_TX_EQ_PREEMP_MASK(lane);
	phytxeq |= (level << DPTX_PHY_TX_EQ_PREEMP_SHIFT(lane)) &
		DPTX_PHY_TX_EQ_PREEMP_MASK(lane);

	dptx_writel(dptx, DPTX_PHY_TX_EQ, phytxeq);
}

void dptx_phy_set_vswing(struct dptx *dptx,
			 u32 lane,
			 u32 level)
{
	u32 phytxeq;

	DRM_DEBUG("%s: lane=%d, level=0x%x\n", __func__, lane, level);

	if (lane > 3) {
		DRM_ERROR("Invalid lane %d", lane);
		return;
	}

	if (level > 3) {
		DRM_ERROR("Invalid vswing level %d, using 3", level);
		level = 3;
	}

	phytxeq = dptx_readl(dptx, DPTX_PHY_TX_EQ);
	phytxeq &= ~DPTX_PHY_TX_EQ_VSWING_MASK(lane);
	phytxeq |= (level << DPTX_PHY_TX_EQ_VSWING_SHIFT(lane)) &
		DPTX_PHY_TX_EQ_VSWING_MASK(lane);

	dptx_writel(dptx, DPTX_PHY_TX_EQ, phytxeq);
}

void dptx_phy_set_pattern(struct dptx *dptx,
			  u32 pattern)
{
	u32 phyifctrl = 0;

	DRM_DEBUG("%s: Setting PHY pattern=0x%x\n", __func__, pattern);

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_TPS_SEL_MASK;
	phyifctrl |= ((pattern << DPTX_PHYIF_CTRL_TPS_SEL_SHIFT) &
		      DPTX_PHYIF_CTRL_TPS_SEL_MASK);
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
}

void dptx_phy_enable_xmit(struct dptx *dptx, u32 lanes, bool enable)
{
	u32 phyifctrl;
	u32 mask = 0;

	DRM_DEBUG("%s: lanes=%d, enable=%d\n", __func__, lanes, enable);

	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

	switch (lanes) {
	case 4:
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(3);
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(2);
		/* fall through */
	case 2:
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(1);
		/* fall through */
	case 1:
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	default:
		DRM_ERROR("Invalid number of lanes %d\n", lanes);
		mask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	}

	if (enable)
		phyifctrl |= mask;
	else
		phyifctrl &= ~mask;

	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
}

int dptx_phy_rate_to_bw(u32 rate)
{
	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		return DP_LINK_BW_1_62;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		return DP_LINK_BW_2_7;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		return DP_LINK_BW_5_4;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		return DP_LINK_BW_8_1;
	default:
		DRM_ERROR("Invalid rate 0x%x\n", rate);
		return -EINVAL;
	}
}

int dptx_bw_to_phy_rate(u32 bw)
{
	switch (bw) {
	case DP_LINK_BW_1_62:
		return DPTX_PHYIF_CTRL_RATE_RBR;
	case DP_LINK_BW_2_7:
		return DPTX_PHYIF_CTRL_RATE_HBR;
	case DP_LINK_BW_5_4:
		return DPTX_PHYIF_CTRL_RATE_HBR2;
	case DP_LINK_BW_8_1:
		return DPTX_PHYIF_CTRL_RATE_HBR3;
	default:
		DRM_ERROR("Invalid bw 0x%x\n", bw);
		return -EINVAL;
	}
}

bool dptx_get_hotplug_status(struct dptx *dptx)
{
	u32 hpdsts;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
	return !!(hpdsts & DPTX_HPDSTS_STATUS);
}

