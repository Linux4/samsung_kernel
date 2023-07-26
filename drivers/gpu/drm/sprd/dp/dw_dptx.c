// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/platform_device.h>
#include "sprd_dp.h"
#include "dw_dptx.h"
#include "sprd_hdcp.h"

static struct dptx *handle;

static const unsigned int dptx_extcon_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

struct dptx *dptx_get_handle(void)
{
	return handle;
}

static int handle_sink_request(struct dptx *dptx)
{
	int retval;
	int ret_dpcd;
	u8 vector = 0;
	u8 bytes[14];
	u32 reg;

	retval = dptx_link_check_status(dptx);
	if (retval)
		return retval;

	ret_dpcd = drm_dp_dpcd_read(&dptx->aux_dev,
				DP_SINK_COUNT_ESI, bytes, 14);
	if (ret_dpcd < 0)
		return ret_dpcd;

	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev,
		DP_DEVICE_SERVICE_IRQ_VECTOR, &vector);
	if (ret_dpcd < 0)
		return ret_dpcd;

	DRM_DEBUG("%s: IRQ_VECTOR: 0x%02x\n", __func__, vector);

	/* TODO handle sink interrupts */
	if (!vector)
		return 0;

	if (vector & DP_REMOTE_CONTROL_COMMAND_PENDING) {
		/* TODO */
		DRM_WARN("%s: DP_REMOTE_CONTROL_COMMAND_PENDING: Not yet implemented",
			  __func__);
	}

	if (vector & DP_AUTOMATED_TEST_REQUEST) {
		DRM_DEBUG("%s: DP_AUTOMATED_TEST_REQUEST", __func__);
		retval = handle_automated_test_request(dptx);
		if (retval) {
			DRM_ERROR("Automated test request failed\n");
			if (retval == -ENOTSUPP) {
				ret_dpcd =
				    drm_dp_dpcd_writeb(&dptx->aux_dev,
						       DP_TEST_RESPONSE,
						       DP_TEST_NAK);
				if (ret_dpcd < 0)
					return ret_dpcd;
			}
		}
	}

	if (vector & DP_CP_IRQ) {
		DRM_WARN("%s: DP_CP_IRQ", __func__);
		ret_dpcd =
		    drm_dp_dpcd_writeb(&dptx->aux_dev,
				       DP_DEVICE_SERVICE_IRQ_VECTOR, DP_CP_IRQ);
		reg = dptx_readl(dptx, DPTX_HDCP_CONFIG);
		reg |= DPTX_CFG_CP_IRQ;
		dptx_writel(dptx, DPTX_HDCP_CONFIG, reg);
		reg = dptx_readl(dptx, DPTX_HDCP_CONFIG);
		reg &= ~DPTX_CFG_CP_IRQ;
		dptx_writel(dptx, DPTX_HDCP_CONFIG, reg);
		if (ret_dpcd < 0)
			return ret_dpcd;
	}

	if (vector & DP_MCCS_IRQ) {
		/* TODO */
		DRM_WARN("%s: DP_MCCS_IRQ: Not yet implemented",
					__func__);
		retval = -ENOTSUPP;
	}

	if (vector & DP_DOWN_REP_MSG_RDY) {
		/* TODO */
		DRM_WARN("%s: DP_DOWN_REP_MSG_RDY: Not yet implemented",
			  __func__);
		retval = -ENOTSUPP;
	}

	if (vector & DP_UP_REQ_MSG_RDY) {
		/* TODO */
		DRM_WARN("%s: DP_UP_REQ_MSG_RDY: Not yet implemented",
			  __func__);
		retval = -ENOTSUPP;
	}

	if (vector & DP_SINK_SPECIFIC_IRQ) {
		/* TODO */
		DRM_WARN("%s: DP_SINK_SPECIFIC_IRQ: Not yet implemented",
			  __func__);
		retval = -ENOTSUPP;
	}

	return retval;
}

static int handle_hotunplug(struct dptx *dptx)
{
	u32 phyifctrl;
	u8 retval;

	/* Clear xmit enables */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_XMIT_EN_MASK;

	/* Move PHY to P3 state */
	phyifctrl |= (3 << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	retval = dptx_phy_wait_busy(dptx, dptx->link.lanes);
	if (retval) {
		DRM_ERROR("Timed out waiting for PHY BUSY\n");
		return retval;
	}

	/* Power down all lanes */

	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);
	atomic_set(&dptx->sink_request, 0);
	dptx->link.trained = false;

	dptx->active = false;

	return 0;
}

static int handle_hotplug(struct dptx *dptx)
{
	int retval, ret_dpcd;
	u32 phyifctrl;
	u8 byte = 0;
	struct video_params *vparams;
	u8 rev;

	vparams = &dptx->vparams;

	dptx_enable_ssc(dptx);

	dptx_soft_reset(dptx, DPTX_SRST_CTRL_AUX);

	/* Move PHY to P0 */
	phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);
	phyifctrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;
	dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

	ret_dpcd = drm_dp_dpcd_readb(&dptx->aux_dev, DP_MSTM_CAP, &byte);
	if (ret_dpcd < 0) {
		DRM_ERROR("read DP_MSTM_CAP err %d\n", ret_dpcd);
		return ret_dpcd;
	}

	if (byte & DP_MST_CAP) {
		ret_dpcd = drm_dp_dpcd_writeb(&dptx->aux_dev, DP_MSTM_CTRL, 0);
		if (ret_dpcd < 0) {
			DRM_ERROR("write DP_MSTM_CTRL err %d\n", ret_dpcd);
			return ret_dpcd;
		}
	}

	if (!dptx->mst)
		dptx->streams = 1;

	memset(dptx->rx_caps, 0, DPTX_RECEIVER_CAP_SIZE);
	ret_dpcd = drm_dp_dpcd_read(&dptx->aux_dev, DP_DPCD_REV,
					   dptx->rx_caps,
					   DPTX_RECEIVER_CAP_SIZE);
	if (ret_dpcd < 0) {
		DRM_ERROR("read rx_caps err %d\n", ret_dpcd);
		return ret_dpcd;
	}

	if (dptx->rx_caps[DP_TRAINING_AUX_RD_INTERVAL] &
	    DP_EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT) {
		ret_dpcd = drm_dp_dpcd_read(&dptx->aux_dev, 0x2200,
						   dptx->rx_caps,
						   DPTX_RECEIVER_CAP_SIZE);
		if (ret_dpcd < 0) {
			DRM_ERROR("read rx EXTENDED caps err %d\n",
				 ret_dpcd);
			return ret_dpcd;
		}
	}

	rev = dptx->rx_caps[0];
	DRM_DEBUG("DP vision %x.%x\n", (rev & 0xf0) >> 4, rev & 0xf);

	retval = dptx_link_training(dptx, dptx->max_rate, dptx->max_lanes);
	if (retval)
		return retval;

	dptx->active = true;

	return 0;
}

irqreturn_t dptx_threaded_irq(int irq, void *dev)
{
	int retval;
	struct dptx *dptx = dev;
	u32 hpdsts;

	DRM_DEBUG("%s:\n", __func__);

	mutex_lock(&dptx->mutex);

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
	DRM_DEBUG("%s: HPDSTS = 0x%08x\n", __func__, hpdsts);

	/*
	 * TODO this should be set after all AUX transactions that are
	 * queued are aborted.
	 */
	atomic_set(&dptx->aux.abort, 0);

	if (atomic_read(&dptx->c_connect)) {
		atomic_set(&dptx->c_connect, 0);

		hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
		if (hpdsts & DPTX_HPDSTS_STATUS) {
			handle_hotplug(dptx);
			extcon_set_state_sync(dptx->edev, EXTCON_DISP_HDMI, 1);
		} else {
			handle_hotunplug(dptx);
			extcon_set_state_sync(dptx->edev, EXTCON_DISP_HDMI, 0);
		}

		if (dptx->drm_dev)
			drm_helper_hpd_irq_event(dptx->drm_dev);
	}

	if (atomic_read(&dptx->sink_request)) {
		atomic_set(&dptx->sink_request, 0);
		retval = handle_sink_request(dptx);
		if (retval)
			DRM_ERROR("Unable to handle sink request %d\n",
				 retval);
	}

	DRM_DEBUG("%s: DONE\n", __func__);

	mutex_unlock(&dptx->mutex);

	return IRQ_HANDLED;
}

static void handle_hpd_irq(struct dptx *dptx)
{
	DRM_DEBUG("%s: HPD_IRQ\n", __func__);
	atomic_set(&dptx->sink_request, 1);
}

irqreturn_t dptx_irq(int irq, void *dev)
{
	irqreturn_t retval = IRQ_HANDLED;
	struct dptx *dptx = dev;
	u32 ists, reg, phyifctrl;

	ists = dptx_readl(dptx, DPTX_ISTS);
	DRM_DEBUG("%s: ISTS=0x%08x\n", __func__, ists);

	if (!(ists & DPTX_ISTS_ALL_INTR)) {
		DRM_DEBUG("%s: IRQ_NONE\n", __func__);
		return IRQ_NONE;
	}

	if (ists & (DPTX_ISTS_AUX_REPLY | DPTX_ISTS_AUX_CMD_INVALID))
		DRM_DEBUG("%s: Should not receive AUX interrupts\n",
					__func__);

	if (ists & DPTX_ISTS_SDP) {
		DRM_DEBUG("%s: DPTX_ISTS_SDP\n", __func__);
		dptx_writel(dptx, DPTX_ISTS, DPTX_ISTS_SDP);
	}

	if (ists & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) {
		DRM_DEBUG("%s: DPTX_ISTS_AUDIO_FIFO_OVERFLOW\n", __func__);
		dptx_writel(dptx, DPTX_ISTS, DPTX_ISTS_AUDIO_FIFO_OVERFLOW);
	}

	if (ists & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) {
		DRM_DEBUG("%s: DPTX_ISTS_VIDEO_FIFO_OVERFLOW\n", __func__);
		dptx_writel(dptx, DPTX_ISTS, DPTX_ISTS_VIDEO_FIFO_OVERFLOW);
	}

	if (ists & DPTX_ISTS_TYPE_C) {
		DRM_INFO("%s: DPTX_ISTS_TYPE_C\n", __func__);
		dptx_writel(dptx, DPTX_ISTS, DPTX_ISTS_TYPE_C);

		reg = dptx_readl(dptx, DPTX_TYPE_C_CTRL);
		if (reg & 0x2) {
			/* Move PHY to P3 */
			phyifctrl = dptx_readl(dptx, DPTX_PHYIF_CTRL);

			phyifctrl |= (3 << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);

			dptx_writel(dptx, DPTX_PHYIF_CTRL, phyifctrl);

			dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);

			dptx_phy_soft_reset(dptx);

			dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);

			dptx_writel(dptx, DPTX_TYPE_C_CTRL, 0x5);
		} else {
			dptx_phy_soft_reset(dptx);
			dptx_phy_wait_busy(dptx, DPTX_MAX_LINK_LANES);
			dptx_writel(dptx, DPTX_TYPE_C_CTRL, 0x4);
		}
	}

	if (ists & DPTX_ISTS_HPD) {
		u32 hpdsts;

		DRM_INFO("%s: DPTX_ISTS_HPD\n", __func__);
		hpdsts = dptx_readl(dptx, DPTX_HPDSTS);

		DRM_DEBUG("%s: HPDSTS = 0x%08x\n", __func__, hpdsts);

		if (hpdsts & DPTX_HPDSTS_IRQ) {
			DRM_DEBUG("%s: DPTX_HPDSTS_IRQ\n", __func__);
			dptx_writel(dptx, DPTX_HPDSTS, DPTX_HPDSTS_IRQ);
			handle_hpd_irq(dptx);
			retval = IRQ_WAKE_THREAD;
		}

		if (hpdsts & DPTX_HPDSTS_HOT_PLUG) {
			DRM_INFO("%s: DPTX_HPDSTS_HOT_PLUG\n", __func__);
			dptx_writel(dptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_PLUG);
			atomic_set(&dptx->aux.abort, 1);
			atomic_set(&dptx->c_connect, 1);
			retval = IRQ_WAKE_THREAD;
		}

		if (hpdsts & DPTX_HPDSTS_HOT_UNPLUG) {
			DRM_INFO("%s: DPTX_HPDSTS_HOT_UNPLUG\n",
				 __func__);
			dptx_writel(dptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG);
			atomic_set(&dptx->aux.abort, 1);
			atomic_set(&dptx->c_connect, 1);
			retval = IRQ_WAKE_THREAD;
		}
	}

	return retval;
}

static ssize_t dptx_dpaux_transfer(struct drm_dp_aux *aux,
				   struct drm_dp_aux_msg *msg)
{
	struct dptx *dp = container_of(aux, struct dptx, aux_dev);

	return dptx_aux_transfer(dp, msg);
}

struct dptx *dptx_init(struct device *dev, struct drm_device *drm_dev)
{
	struct dptx *dptx;
	int retval;
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);

	dptx = devm_kzalloc(dev, sizeof(*dptx), GFP_KERNEL);
	if (!dptx)
		return ERR_PTR(-ENOMEM);

	dptx->dev = dev;
	dptx->base = dp->ctx.base;
	dptx->ipa_usb31_dp = dp->ctx.ipa_usb31_dp;

	if (IS_ERR(dptx->base)) {
		dev_err(dev, "Failed to map memory resource\n");
		return ERR_PTR(-EFAULT);
	}

	dptx->irq = platform_get_irq(pdev, 0);
	if (dptx->irq < 0) {
		dev_err(dev, "No IRQ\n");
		return ERR_PTR(dptx->irq);
	}

	mutex_init(&dptx->mutex);
	dptx_misc_reset(dptx);
	dptx_video_params_reset(dptx);
	dptx_audio_params_reset(&dptx->aparams);
	atomic_set(&dptx->sink_request, 0);
	atomic_set(&dptx->c_connect, 0);

	dptx_global_intr_dis(dptx);

	retval = devm_request_threaded_irq(dev,
					   dptx->irq,
					   dptx_irq,
					   dptx_threaded_irq,
					   IRQF_SHARED, "dw_dptx", dptx);
	if (retval) {
		dev_err(dev, "Request for irq %d failed\n", dptx->irq);
		return ERR_PTR(retval);
	}

	/* Allocate extcon device */
	dptx->edev = devm_extcon_dev_allocate(dptx->dev, dptx_extcon_cable);
	if (IS_ERR(dptx->edev)) {
		dev_err(dev, "failed to allocate memory for extcon\n");
		retval = -ENOMEM;
	};

	/* Register extcon device */
	retval = devm_extcon_dev_register(dptx->dev, dptx->edev);
	if (retval) {
		dev_err(dev, "failed to register extcon device\n");
		goto fail;
	}

	dptx_init_hwparams(dptx);

	retval = dptx_core_init(dptx);
	if (retval)
		goto fail;

	dptx->drm_dev = drm_dev;
	dptx->aux_dev.name = "DP-AUX";
	dptx->aux_dev.transfer = dptx_dpaux_transfer;
	dptx->aux_dev.dev = &pdev->dev;

	retval = drm_dp_aux_register(&dptx->aux_dev);
	if (retval)
		goto fail;

	handle = dptx;
	return dptx;

fail:
	return ERR_PTR(retval);
}
