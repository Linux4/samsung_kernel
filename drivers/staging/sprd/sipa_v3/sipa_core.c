// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc sipa driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Qingsheng Li <qingsheng.li@unisoc.com>
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/sipa.h>

#include "sipa_hal.h"
#include "sipa_rm.h"
#include "sipa_debug.h"

#define DRV_NAME "sipa"

static struct sipa_cmn_fifo_info sipa_cmn_fifo_statics[SIPA_FIFO_MAX] = {
	{
		.fifo_name = "sprd,usb-ul",
		.tx_fifo = "sprd,usb-ul-tx",
		.rx_fifo = "sprd,usb-ul-rx",
		.relate_ep = SIPA_EP_USB,
		.src_id = SIPA_TERM_USB,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 1,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,wifi-ul",
		.tx_fifo = "sprd,wifi-ul-tx",
		.rx_fifo = "sprd,wifi-ul-rx",
		.relate_ep = SIPA_EP_WIFI,
		.src_id = SIPA_TERM_WIFI1,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 1,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,pcie-ul",
		.tx_fifo = "sprd,pcie-ul-tx",
		.rx_fifo = "sprd,pcie-ul-rx",
		.relate_ep = SIPA_EP_PCIE,
		.src_id = SIPA_TERM_PCIE0,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 1,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,wiap-dl",
		.tx_fifo = "sprd,wiap-dl-tx",
		.rx_fifo = "sprd,wiap-dl-rx",
		.relate_ep = SIPA_EP_WIAP,
		.src_id = SIPA_TERM_VAP0,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 1,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,map-in",
		.tx_fifo = "sprd,map-in-tx",
		.rx_fifo = "sprd,map-in-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_VCP,
		.is_to_ipa = 1,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,usb-dl",
		.tx_fifo = "sprd,usb-dl-tx",
		.rx_fifo = "sprd,usb-dl-rx",
		.relate_ep = SIPA_EP_USB,
		.src_id = SIPA_TERM_USB,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 0,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,wifi-dl",
		.tx_fifo = "sprd,wifi-dl-tx",
		.rx_fifo = "sprd,wifi-dl-rx",
		.relate_ep = SIPA_EP_WIFI,
		.src_id = SIPA_TERM_WIFI1,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 0,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,pcie-dl",
		.tx_fifo = "sprd,pcie-dl-tx",
		.rx_fifo = "sprd,pcie-dl-rx",
		.relate_ep = SIPA_EP_PCIE,
		.src_id = SIPA_TERM_PCIE0,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 0,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,wiap-ul",
		.tx_fifo = "sprd,wiap-ul-tx",
		.rx_fifo = "sprd,wiap-ul-rx",
		.relate_ep = SIPA_EP_WIAP,
		.src_id = SIPA_TERM_VAP0,
		.dst_id = SIPA_TERM_AP,
		.is_to_ipa = 0,
		.is_pam = 1,
	},
	{
		.fifo_name = "sprd,map0-out",
		.tx_fifo = "sprd,map0-out-tx",
		.rx_fifo = "sprd,map0-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map1-out",
		.tx_fifo = "sprd,map1-out-tx",
		.rx_fifo = "sprd,map1-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map2-out",
		.tx_fifo = "sprd,map2-out-tx",
		.rx_fifo = "sprd,map2-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map3-out",
		.tx_fifo = "sprd,map3-out-tx",
		.rx_fifo = "sprd,map3-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map4-out",
		.tx_fifo = "sprd,map4-out-tx",
		.rx_fifo = "sprd,map4-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map5-out",
		.tx_fifo = "sprd,map5-out-tx",
		.rx_fifo = "sprd,map5-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map6-out",
		.tx_fifo = "sprd,map6-out-tx",
		.rx_fifo = "sprd,map6-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
	{
		.fifo_name = "sprd,map7-out",
		.tx_fifo = "sprd,map7-out-tx",
		.rx_fifo = "sprd,map7-out-rx",
		.relate_ep = SIPA_EP_AP,
		.src_id = SIPA_TERM_AP,
		.dst_id = SIPA_TERM_USB,
		.is_to_ipa = 0,
		.is_pam = 0,
	},
};

static const char * const sipa_eb_name_tb[] = {
	"enable-ipa",
	"enable-tft",
};

static struct sipa_plat_drv_cfg *s_sipa_core;

static void sipa_resume_for_pam(struct device *dev)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo = ipa->cmn_fifo_cfg;

	mutex_lock(&ipa->resume_lock);
	if (!ipa->suspend_stage) {
		mutex_unlock(&ipa->resume_lock);
		return;
	}

	if (sipa_hal_get_pause_status() ||
	    sipa_hal_get_resume_status() ||
	    !(ipa->suspend_stage & SIPA_BACKUP_SUSPEND))
		goto early_resume;

	for (i = 0; i < SIPA_FIFO_MAX; i++)
		if (fifo[i].tx_fifo.in_iram && fifo[i].rx_fifo.in_iram)
			sipa_hal_resume_fifo_node(dev, i);

	sipa_hal_resume_glb_reg_cfg(dev);
	sipa_hal_resume_cmn_fifo(dev);
	ipa->suspend_stage &= ~SIPA_BACKUP_SUSPEND;

early_resume:
	if (ipa->suspend_stage & SIPA_THREAD_SUSPEND) {
		sipa_receiver_prepare_resume(ipa->receiver);
		ipa->suspend_stage &= ~SIPA_THREAD_SUSPEND;
	}

	if (ipa->suspend_stage & SIPA_ACTION_SUSPEND) {
		sipa_hal_ctrl_ipa_action(true);
		ipa->suspend_stage &= ~SIPA_ACTION_SUSPEND;
	}

	mutex_unlock(&ipa->resume_lock);
}

/**
 * sipa_prepare_resume() - Restore IPA related configuration
 * @dev: sipa driver dev
 *
 * IPA EB bit enable, restore IPA glb reg and common fifo, resume
 * ep and receiver recv function.
 */
static void sipa_prepare_resume(struct device *dev)
{
	int i;
	struct sipa_endpoint *ep;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo = ipa->cmn_fifo_cfg;

	mutex_lock(&ipa->resume_lock);
	if (!ipa->suspend_stage) {
		mutex_unlock(&ipa->resume_lock);
		return;
	}

	if (ipa->suspend_stage & SIPA_EB_SUSPEND) {
		sipa_set_enabled(true);
		ipa->suspend_stage &= ~SIPA_EB_SUSPEND;
	}

	if (sipa_hal_get_pause_status() || sipa_hal_get_resume_status())
		goto early_resume;

	if (!(ipa->suspend_stage & SIPA_BACKUP_SUSPEND))
		goto early_resume;

	for (i = 0; i < SIPA_FIFO_MAX; i++)
		if (fifo[i].tx_fifo.in_iram && fifo[i].rx_fifo.in_iram)
			sipa_hal_resume_fifo_node(dev, i);

	sipa_hal_resume_glb_reg_cfg(dev);
	sipa_hal_resume_cmn_fifo(dev);
	ipa->suspend_stage &= ~SIPA_BACKUP_SUSPEND;

early_resume:
	ep = ipa->eps[SIPA_EP_USB];
	if (ep && ep->connected)
		sipa_hal_cmn_fifo_stop_recv(dev, ep->recv_fifo.idx, false);
	ep = ipa->eps[SIPA_EP_WIFI];
	if (ep && ep->connected)
		sipa_hal_cmn_fifo_stop_recv(dev, ep->recv_fifo.idx, false);

	ipa->suspend_stage &= ~SIPA_EP_SUSPEND;

	if (ipa->suspend_stage & SIPA_THREAD_SUSPEND) {
		sipa_receiver_prepare_resume(ipa->receiver);
		ipa->suspend_stage &= ~SIPA_THREAD_SUSPEND;
	}

	if (ipa->suspend_stage & SIPA_ACTION_SUSPEND) {
		sipa_hal_ctrl_ipa_action(true);
		ipa->suspend_stage &= ~SIPA_ACTION_SUSPEND;
	}

	mutex_unlock(&ipa->resume_lock);
}

/**
 * sipa_resume_work() - resume ipa all profile
 * @dev: sipa driver dev
 *
 * resume ipa all profile, after this function finished,
 * ipa will work normally.
 *
 */
static int sipa_resume_work(struct device *dev)
{
	int ret;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (ipa->suspend_stage & SIPA_FORCE_SUSPEND) {
		ipa->suspend_stage &= ~SIPA_FORCE_SUSPEND;

		ret = pm_runtime_get_sync(dev);
		if (ret) {
			pm_runtime_put(dev);
			dev_warn(dev, "pm runtime get err ret = %d\n", ret);
			return ret;
		}
	}

	sipa_prepare_resume(dev);

	sipa_sender_prepare_resume(ipa->sender);

	sipa_rm_notify_completion(SIPA_RM_EVT_GRANTED,
				  SIPA_RM_RES_PROD_IPA);

	ipa->suspend_stage = 0;

	return 0;
}

/**
 * sipa_check_ep_suspend() - Check ep whether have the conditions for sleep.
 * @dev: Sipa driver device.
 * @id: The endpoint id that need to be checked.
 *
 * Determine if the node description sent out is completely free,
 * if not free completely, wake lock 500ms, return -EAGAIN.
 *
 * Return:
 *	0: succeed.
 *	-EAGAIN: check err.
 */
static int sipa_check_ep_suspend(struct device *dev, enum sipa_ep_id id)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_endpoint *ep = ipa->eps[id];

	if (!ep || !ep->connected)
		return 0;

	sipa_hal_cmn_fifo_stop_recv(dev, ep->recv_fifo.idx, true);

	if (!sipa_hal_check_send_cmn_fifo_com(dev, ep->send_fifo.idx)) {
		dev_err(dev, "check send cmn fifo finish status fail fifo id = %d\n",
			ep->send_fifo.idx);
		pm_wakeup_dev_event(dev, 500, true);
		return -EAGAIN;
	}

	return 0;
}

/**
 * sipa_ep_prepare_suspend() - Check usb/wifi/vcp ep suspend conditions
 * @dev: sipa driver device
 *
 * Check usb/wifi/vcp end pointer suspend conditions, if conditions
 * satisfaction, turn off its receiving function.
 *
 * Return:
 *	0: success.
 *	-EAGAIN: suspend fail.
 */
static int sipa_ep_prepare_suspend(struct device *dev)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (ipa->suspend_stage & SIPA_EP_SUSPEND)
		return 0;

	if (sipa_nic_check_suspend_condition() ||
	    sipa_check_ep_suspend(dev, SIPA_EP_USB) ||
	    sipa_check_ep_suspend(dev, SIPA_EP_WIFI) ||
	    sipa_check_ep_suspend(dev, SIPA_EP_PCIE))
		return -EAGAIN;

	ipa->suspend_stage |= SIPA_EP_SUSPEND;

	return 0;
}

/**
 * sipa_thread_prepare_suspend() - Check sender/receiver suspend conditions.
 * @dev: sipa driver device.
 *
 * Check sender/receiver suspend conditions. if conditions not satisfaction,
 * wake lock 500ms.
 *
 * Return:
 *	0: success.
 *	-EAGAIN: check fail.
 */
static int sipa_thread_prepare_suspend(struct device *dev)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (ipa->suspend_stage & SIPA_THREAD_SUSPEND)
		return 0;

	if (!sipa_sender_prepare_suspend(ipa->sender) &&
	    !sipa_receiver_prepare_suspend(ipa->receiver)) {
		ipa->suspend_stage |= SIPA_THREAD_SUSPEND;
	} else {
		dev_err(dev, "thread prepare suspend err\n");
		pm_wakeup_dev_event(dev, 500, true);
		return -EAGAIN;
	}

	return 0;
}

static int sipa_fifo_prepare_suspend(struct device *dev)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	struct sipa_cmn_fifo_cfg_tag *fifo = ipa->cmn_fifo_cfg;

	if (ipa->suspend_stage & SIPA_BACKUP_SUSPEND)
		return 0;

	for (i = 0; i < SIPA_FIFO_MAX; i++)
		if (fifo[i].tx_fifo.in_iram && fifo[i].rx_fifo.in_iram)
			sipa_hal_bk_fifo_node(dev, i);

	ipa->suspend_stage |= SIPA_BACKUP_SUSPEND;

	return 0;
}

static int sipa_prepare_suspend(struct device *dev)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (ipa->power_flag)
		return 0;

	if (sipa_ep_prepare_suspend(dev) ||
	    sipa_thread_prepare_suspend(dev) ||
	    sipa_fifo_prepare_suspend(dev))
		return -EAGAIN;

	if (!(ipa->suspend_stage & SIPA_EB_SUSPEND)) {
		sipa_set_enabled(false);
		ipa->suspend_stage |= SIPA_EB_SUSPEND;
	}

	if (!(ipa->suspend_stage & SIPA_FORCE_SUSPEND)) {
		pm_runtime_put(dev);
		ipa->suspend_stage |= SIPA_FORCE_SUSPEND;
	}

	ipa->suspend_stage |= SIPA_ACTION_SUSPEND;

	dev_info(dev, "sipa prepare suspend finished\n");

	return 0;
}

static int sipa_rm_prepare_release(void *priv)
{
	struct sipa_plat_drv_cfg *ipa = priv;

	ipa->suspend_cnt++;
	ipa->power_flag = false;
	cancel_delayed_work(&ipa->power_work);
	queue_delayed_work(ipa->power_wq, &ipa->power_work, 0);

	return 0;
}

static int sipa_rm_prepare_resume(void *priv)
{
	struct sipa_plat_drv_cfg *ipa = priv;

	ipa->resume_cnt++;
	ipa->power_flag = true;
	cancel_delayed_work(&ipa->power_work);
	queue_delayed_work(ipa->power_wq, &ipa->power_work, 0);

	/* TODO: will remove the error code in future */
	return -EINPROGRESS;
}

/**
 * sipa_get_ep_info() - get the configuration information of the endpoint.
 * @id: endpoint id.
 * @out: the endpoint related information is output to this structure.
 *
 * The pam peripheral obtains the configuration information it needs through
 * this interface.
 */
int sipa_get_ep_info(enum sipa_ep_id id,
		     struct sipa_to_pam_info *out)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_endpoint *ep = ipa->eps[id];

	if (!ep) {
		dev_err(ipa->dev, "ep id:%d not create!", id);
		return -EPROBE_DEFER;
	}
	if (SIPA_EP_USB == id || SIPA_EP_WIFI == id || SIPA_EP_PCIE == id)
		sipa_hal_init_pam_param(ep->recv_fifo.idx,
					ep->send_fifo.idx, out);
	else
		sipa_hal_init_pam_param(ep->send_fifo.idx,
					ep->recv_fifo.idx, out);

	return 0;
}
EXPORT_SYMBOL(sipa_get_ep_info);

/**
 * sipa_check_endpoint_complete() - check whether the endpoint data transfer
 *                                  is complete.
 * @id: endpoint id.
 *
 */
bool sipa_check_endpoint_complete(enum sipa_ep_id id)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_endpoint *ep = ipa->eps[id];

	return sipa_hal_check_send_cmn_fifo_com(ipa->dev, ep->send_fifo.idx) &&
		sipa_hal_check_send_cmn_fifo_com(ipa->dev, ep->recv_fifo.idx);
}
EXPORT_SYMBOL(sipa_check_endpoint_complete);

/**
 * sipa_pam_connect() - pam peripheral connects to the corresponding
 *                      ipa endpoint.
 * @in: store the parameters required by the ipa endpoint.
 *
 * The pam peripheral needs to call this interface before working, otherwise the
 * data cannot be transmitted normally.
 */
int sipa_pam_connect(const struct sipa_connect_params *in)
{
	u32 i;
	struct sipa_node_desc_tag fifo_item;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_endpoint *ep = ipa->eps[in->id];

	if (!ep) {
		dev_err(ipa->dev, "sipa pam connect ep id:%d not create!",
			in->id);
		return -EPROBE_DEFER;
	}

	sipa_set_enabled(true);
	ep->connected = true;
	ep->suspended = false;

	if (ipa->suspend_stage)
		sipa_resume_for_pam(ipa->dev);

	if (ep->inited)
		goto set_recv;

	memset(&fifo_item, 0, sizeof(fifo_item));
	ep->send_notify = in->send_notify;
	ep->recv_notify = in->recv_notify;
	ep->send_priv = in->send_priv;
	ep->recv_priv = in->recv_priv;
	ep->inited = true;
	memcpy(&ep->send_fifo_param, &in->send_param,
	       sizeof(struct sipa_comm_fifo_params));
	memcpy(&ep->recv_fifo_param, &in->recv_param,
	       sizeof(struct sipa_comm_fifo_params));
	sipa_hal_open_cmn_fifo(ipa->dev, ep->send_fifo.idx,
			       &ep->send_fifo_param, NULL, false,
			       (sipa_hal_notify_cb)ep->send_notify, ep);
	sipa_hal_open_cmn_fifo(ipa->dev, ep->recv_fifo.idx,
			       &ep->recv_fifo_param, NULL, false,
			       (sipa_hal_notify_cb)ep->recv_notify, ep);

	if (ep->send_fifo_param.data_ptr) {
		for (i = 0; i < ep->send_fifo_param.data_ptr_cnt; i++) {
			fifo_item.address = ep->send_fifo_param.data_ptr +
				i * ep->send_fifo_param.buf_size;
			fifo_item.length = ep->send_fifo_param.buf_size;
			sipa_hal_put_node_to_tx_fifo(ipa->dev,
						     ep->send_fifo.idx,
						     &fifo_item, 1);
		}
	}
	if (ep->recv_fifo_param.data_ptr) {
		for (i = 0; i < ep->recv_fifo_param.data_ptr_cnt; i++) {
			fifo_item.address = ep->recv_fifo_param.data_ptr +
				i * ep->recv_fifo_param.buf_size;
			fifo_item.length = ep->recv_fifo_param.buf_size;
			sipa_hal_put_node_to_rx_fifo(ipa->dev,
						     ep->recv_fifo.idx,
						     &fifo_item, 1);
		}
	}

set_recv:
	sipa_hal_cmn_fifo_stop_recv(ipa->dev, ep->recv_fifo.idx, false);

	return 0;
}
EXPORT_SYMBOL(sipa_pam_connect);

/**
 * sipa_ext_open_pcie() - open pcie endpoint.
 * @in: store the parameters required by the ipa pcie endpoint.
 *
 */
int sipa_ext_open_pcie(struct sipa_pcie_open_params *in)
{
	struct sipa_endpoint *ep;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa)
		return -EPROBE_DEFER;

	if (ipa->eps[SIPA_EP_PCIE]) {
		dev_err(ipa->dev, "pcie already create!");
		return -EBUSY;
	}

	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	ipa->eps[SIPA_EP_PCIE] = ep;

	ep->dev = ipa->dev;
	ep->id = SIPA_EP_PCIE;

	ep->send_fifo.idx = SIPA_FIFO_PCIE_UL;
	ep->send_fifo.rx_fifo.fifo_depth = in->ext_send_param.rx_depth;
	ep->send_fifo.tx_fifo.fifo_depth = in->ext_send_param.tx_depth;
	ep->send_fifo.src_id = SIPA_TERM_PCIE0;
	ep->send_fifo.dst_id = SIPA_TERM_VCP;

	ep->recv_fifo.idx = SIPA_FIFO_PCIE_DL;
	ep->recv_fifo.rx_fifo.fifo_depth = in->ext_recv_param.rx_depth;
	ep->recv_fifo.tx_fifo.fifo_depth = in->ext_recv_param.tx_depth;
	ep->recv_fifo.src_id = SIPA_TERM_PCIE0;
	ep->recv_fifo.dst_id = SIPA_TERM_VCP;

	ep->send_notify = in->send_notify;
	ep->recv_notify = in->recv_notify;
	ep->send_priv = in->send_priv;
	ep->recv_priv = in->recv_priv;
	ep->connected = true;
	ep->suspended = false;
	memcpy(&ep->send_fifo_param, &in->send_param,
	       sizeof(struct sipa_comm_fifo_params));
	memcpy(&ep->recv_fifo_param, &in->recv_param,
	       sizeof(struct sipa_comm_fifo_params));

	sipa_hal_open_cmn_fifo(ipa->dev, ep->send_fifo.idx,
			       &ep->send_fifo_param, &in->ext_send_param, false,
			       (sipa_hal_notify_cb)ep->send_notify, ep);

	sipa_hal_open_cmn_fifo(ipa->dev, ep->recv_fifo.idx,
			       &ep->recv_fifo_param,
			       &in->ext_recv_param, false,
			       (sipa_hal_notify_cb)ep->recv_notify, ep);
	return 0;
}
EXPORT_SYMBOL(sipa_ext_open_pcie);

/**
 * sipa_pam_init_free_fifo() - Pre-filled free buf.
 * @id: the endpoint id to be filled.
 * @addr: free buf address storage space.
 * @num: number of filling.
 *
 * Some endpoint have dma copy function, such as pcie, so free buf needs to be
 * pre-filled.
 */
int sipa_pam_init_free_fifo(enum sipa_ep_id id,
			    const dma_addr_t *addr, u32 num)
{
	u32 i;
	struct sipa_node_desc_tag node;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_endpoint *ep = ipa->eps[id];

	for (i = 0; i < num; i++) {
		node.address = addr[i];
		sipa_hal_put_node_to_tx_fifo(ipa->dev, ep->recv_fifo.idx,
					     &node, 1);
	}

	return 0;
}
EXPORT_SYMBOL(sipa_pam_init_free_fifo);

/**
 * sipa_disconnect() - pam peripheral is disconnected from its
 *                     dependent endpoint.
 * @ep_id: the endpoint id.
 * @stage: currently disconnected stage.
 *
 * The disconnection stage is divided into SIPA_DISCONNECT_START and
 * SIPA_DISCONNECT_END, which must correspond to the call of
 * sipa_pam_connect.
 */
int sipa_disconnect(enum sipa_ep_id ep_id, enum sipa_disconnect_id stage)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_endpoint *ep = ipa->eps[ep_id];

	if (!ep) {
		dev_err(ipa->dev,
			"sipa disconnect ep id:%d not create!", ep_id);
		return -ENODEV;
	}

	ep->connected = false;
	ep->send_notify = NULL;
	ep->send_priv = 0;
	ep->recv_notify = NULL;
	ep->recv_priv = 0;

	switch (stage) {
	case SIPA_DISCONNECT_START:
		if (ipa->suspend_stage & SIPA_EP_SUSPEND)
			return 0;
		sipa_hal_cmn_fifo_stop_recv(ipa->dev, ep->recv_fifo.idx, true);
		break;
	case SIPA_DISCONNECT_END:
		ep->suspended = true;
		if (ep->id == SIPA_EP_USB)
			sipa_hal_reclaim_unuse_node(ipa->dev,
						    ep->recv_fifo.idx);
		sipa_set_enabled(false);
		break;
	default:
		dev_err(ipa->dev, "don't have this stage\n");
		return -EPERM;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_disconnect);

/**
 * sipa_get_ctrl_pointer() - get the main structure of th sipa driver.
 */
struct sipa_plat_drv_cfg *sipa_get_ctrl_pointer(void)
{
	return s_sipa_core;
}
EXPORT_SYMBOL(sipa_get_ctrl_pointer);

/**
 * sipa_set_enabled() - enable/disable ipa.
 * @enable: true or false.
 *
 * Pass in true or false to control the switch of ipa, because there is refcount
 * in it, so true or false must be matched.
 */
int sipa_set_enabled(bool enable)
{
	int ret = 0;
	unsigned long flags;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	spin_lock_irqsave(&ipa->enable_lock, flags);
	if (enable) {
		ipa->enable_cnt++;
	} else {
		if (WARN_ON(ipa->enable_cnt == 0)) {
			spin_unlock_irqrestore(&ipa->enable_lock, flags);
			return -EINVAL;
		}

		ipa->enable_cnt--;
	}

	if (ipa->enable_cnt == 0)
		ret = sipa_hal_set_enabled(ipa->dev, false);
	else if (ipa->enable_cnt == 1)
		ret = sipa_hal_set_enabled(ipa->dev, true);

	spin_unlock_irqrestore(&ipa->enable_lock, flags);

	return ret;
}
EXPORT_SYMBOL(sipa_set_enabled);

static int sipa_parse_dts_configuration(struct platform_device *pdev,
					struct sipa_plat_drv_cfg *ipa)
{
	int i, j, ret, count;
	char name[20];
	u32 *fifo_info;
	u32 reg_info[2];
	struct resource *resource;
	struct regmap *rmap;
	const char *sipa_eb_name;
	const struct sipa_hw_data *pdata;
	struct sipa_cmn_fifo_info *cmn_fifo_info;
	const char *fifo_name;

	/* get IPA  global  register  offset */
	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}
	ipa->hw_data = pdata;
	/* get IPA global register base  address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"ipa-base");
	if (!resource) {
		dev_err(&pdev->dev, "get resource failed for glb-base!\n");
		return -ENODEV;
	}

	ipa->glb_phy = resource->start;
	ipa->glb_size = resource_size(resource);
	ipa->glb_virt_base = devm_ioremap_nocache(&pdev->dev, ipa->glb_phy,
						  ipa->glb_size);

	/* get IRQ numbers */
	ipa->general_intr = platform_get_irq_byname(pdev, "ipa_general");
	if (ipa->general_intr == -ENXIO) {
		dev_err(&pdev->dev, "get ipa-irq fail!\n");
		return -ENODEV;
	}
	dev_info(&pdev->dev, "general intr num = %d\n", ipa->general_intr);
	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		sprintf(name, "ipa_irq%d", i);
		ipa->multi_intr[i] = platform_get_irq_byname(pdev, name);
		dev_info(&pdev->dev, "multi intr num = %d\n",
			 ipa->multi_intr[i]);
	}

	/* get IPA bypass mode */
	ret = of_property_read_u32(pdev->dev.of_node, "sprd,sipa-bypass-mode",
				   &ipa->is_bypass);
	if (ret)
		dev_info(&pdev->dev, "use normal mode = %d\n", ipa->is_bypass);
	else
		dev_info(&pdev->dev, "use bypass mode by default\n");

	/* get through pcie flag */
	ipa->need_through_pcie =
		of_property_read_bool(pdev->dev.of_node,
				      "sprd,need-through-pcie");

	/* get wiap ul dma flag */
	ipa->wiap_ul_dma = of_property_read_bool(pdev->dev.of_node,
						 "sprd,wiap-ul-dma");

	ipa->pcie_dl_dma = of_property_read_bool(pdev->dev.of_node,
						 "sprd,pcie-dl-dma");

	/* get enable register information */
	for (i = 0; i < SIPA_EB_NUM; i++) {
		sipa_eb_name = sipa_eb_name_tb[i];
		rmap = syscon_regmap_lookup_by_name(pdev->dev.of_node,
						    sipa_eb_name);
		if (IS_ERR(rmap)) {
			dev_err(&pdev->dev, "get enable %s regmap fail!\n",
				sipa_eb_name);
			continue;
		}

		ret = syscon_get_args_by_name(pdev->dev.of_node, sipa_eb_name,
					      2, reg_info);
		if (ret < 0 || ret != 2) {
			dev_err(&pdev->dev, "get enable %s register info fail!\n",
				sipa_eb_name);
			continue;
		}
		ipa->eb_regs[i].enable_rmap = rmap;
		ipa->eb_regs[i].enable_reg = reg_info[0];
		ipa->eb_regs[i].enable_mask = reg_info[1];
	}

	/* config IPA fifo default memory settings */
	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		cmn_fifo_info = &sipa_cmn_fifo_statics[i];

		ipa->cmn_fifo_cfg[i].fifo_name = cmn_fifo_info->fifo_name;
		ipa->cmn_fifo_cfg[i].tx_fifo.in_iram = 0;
		ipa->cmn_fifo_cfg[i].tx_fifo.depth = 4096;
		ipa->cmn_fifo_cfg[i].rx_fifo.in_iram = 0;
		ipa->cmn_fifo_cfg[i].rx_fifo.depth = 4096;

		if (cmn_fifo_info->is_to_ipa)
			ipa->cmn_fifo_cfg[i].is_recv = false;
		else
			ipa->cmn_fifo_cfg[i].is_recv = true;

		ipa->cmn_fifo_cfg[i].fifo_id = i;
		ipa->cmn_fifo_cfg[i].src = cmn_fifo_info->src_id;
		ipa->cmn_fifo_cfg[i].dst = cmn_fifo_info->dst_id;
		ipa->cmn_fifo_cfg[i].is_pam = cmn_fifo_info->is_pam;
	}

	/* config IPA fifo memory settings */
	count = of_property_count_strings(pdev->dev.of_node, "fifo-names");
	if (count < 0) {
		dev_err(&pdev->dev, "no fifo-names need to set\n");
		return 0;
	}

	fifo_info = kzalloc(count * 4 * sizeof(*fifo_info), GFP_KERNEL);
	if (!fifo_info)
		return -ENOMEM;

	of_property_read_u32_array(pdev->dev.of_node, "fifo-sizes",
				   fifo_info, count * 4);

	for (i = 0; i < count; i++) {
		of_property_read_string_index(pdev->dev.of_node,
					      "fifo-names", i,
					      &fifo_name);
		for (j = 0; j < SIPA_FIFO_MAX; j++) {
			if (!strcmp(fifo_name,
				    ipa->cmn_fifo_cfg[j].fifo_name)) {
				ipa->cmn_fifo_cfg[j].tx_fifo.in_iram
					= fifo_info[count * i];
				ipa->cmn_fifo_cfg[j].tx_fifo.depth
					= fifo_info[count * i + 1];
				ipa->cmn_fifo_cfg[j].rx_fifo.in_iram
					= fifo_info[count * i + 2];
				ipa->cmn_fifo_cfg[j].rx_fifo.depth
					= fifo_info[count * i + 3];
				break;
			}
		}
	}
	kfree(fifo_info);

	return 0;
}

static int sipa_create_ep_from_fifo_idx(struct device *dev,
					enum sipa_cmn_fifo_index fifo_idx)
{
	enum sipa_ep_id ep_id;
	struct sipa_common_fifo *fifo;
	struct sipa_endpoint *ep = NULL;
	struct sipa_cmn_fifo_info *fifo_info;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	fifo_info = (struct sipa_cmn_fifo_info *)sipa_cmn_fifo_statics;
	ep_id = (fifo_info + fifo_idx)->relate_ep;

	ep = ipa->eps[ep_id];
	if (!ep) {
		ep = kzalloc(sizeof(*ep), GFP_KERNEL);
		if (!ep)
			return -ENOMEM;

		ipa->eps[ep_id] = ep;
	} else if (fifo_idx > SIPA_FIFO_MAP0_OUT) {
		dev_info(dev, "ep %d has already create\n", ep->id);
		return 0;
	}

	ep->dev = dev;
	ep->id = (fifo_info + fifo_idx)->relate_ep;
	dev_info(dev, "idx = %d ep = %d ep_id = %d is_to_ipa = %d\n",
		 fifo_idx, ep->id, ep_id,
		 (fifo_info + fifo_idx)->is_to_ipa);

	ep->connected = false;
	ep->suspended = true;

	if (!(fifo_info + fifo_idx)->is_to_ipa) {
		fifo = &ep->recv_fifo;
		fifo->is_receiver = true;
	} else {
		fifo = &ep->send_fifo;
		fifo->is_receiver = false;
	}

	fifo->rx_fifo.fifo_depth = ipa->cmn_fifo_cfg[fifo_idx].rx_fifo.depth;
	fifo->tx_fifo.fifo_depth = ipa->cmn_fifo_cfg[fifo_idx].tx_fifo.depth;
	fifo->dst_id = (fifo_info + fifo_idx)->dst_id;
	fifo->src_id = (fifo_info + fifo_idx)->src_id;

	fifo->idx = fifo_idx;

	return 0;
}

static void sipa_destroy_ep_from_fifo_idx(struct device *dev,
					  enum sipa_cmn_fifo_index fifo_idx)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);
	enum sipa_ep_id ep_id = sipa_cmn_fifo_statics[fifo_idx].relate_ep;

	if (!ipa->eps[ep_id])
		return;

	kfree(ipa->eps[ep_id]);
	ipa->eps[ep_id] = NULL;
}

static void sipa_destroy_eps(struct device *dev)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		if (ipa->cmn_fifo_cfg[i].tx_fifo.depth > 0)
			sipa_destroy_ep_from_fifo_idx(dev, i);
	}
}

static int sipa_create_eps(struct device *dev)
{
	int i, ret = 0;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	dev_info(dev, "create eps start\n");
	for (i = 0; i < SIPA_FIFO_MAX; i++) {
		if (ipa->cmn_fifo_cfg[i].tx_fifo.depth > 0) {
			ret = sipa_create_ep_from_fifo_idx(dev, i);
			if (ret) {
				dev_err(dev, "create eps fifo %d fail\n", i);
				return ret;
			}
		}
	}

	return 0;
}

static int sipa_create_skb_xfer(struct device *dev)
{
	int ret = 0;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	ret = sipa_create_skb_sender(ipa, ipa->eps[SIPA_EP_AP], &ipa->sender);
	if (ret)
		return ret;

	ret = sipa_create_skb_receiver(ipa, ipa->eps[SIPA_EP_AP],
				       &ipa->receiver);
	if (ret)
		return ret;

	ret = sipa_rm_inactivity_timer_init(SIPA_RM_RES_CONS_WWAN_UL,
					    SIPA_WWAN_CONS_TIMER);
	if (ret)
		return ret;

	return 0;
}

static int sipa_create_rm_cons(void)
{
	int ret;
	struct sipa_rm_create_params rm_params = {};

	/* WWAN UL */
	rm_params.name = SIPA_RM_RES_CONS_WWAN_UL;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret)
		return ret;

	/* WWAN DL */
	rm_params.name = SIPA_RM_RES_CONS_WWAN_DL;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret) {
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_UL);
		return ret;
	}

	/* USB */
	rm_params.name = SIPA_RM_RES_CONS_USB;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret) {
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_UL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_DL);
		return ret;
	}

	/* WLAN UL */
	rm_params.name = SIPA_RM_RES_CONS_WIFI_UL;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret) {
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_UL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_DL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_USB);
		return ret;
	}

	/* WLAN DL */
	rm_params.name = SIPA_RM_RES_CONS_WIFI_DL;

	ret = sipa_rm_create_resource(&rm_params);
	if (ret) {
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_UL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_DL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_WIFI_UL);
		sipa_rm_delete_resource(SIPA_RM_RES_CONS_USB);
		return ret;
	}

	return 0;
}

static void sipa_destroy_rm_cons(void)
{
	sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_UL);
	sipa_rm_delete_resource(SIPA_RM_RES_CONS_WWAN_DL);
	sipa_rm_delete_resource(SIPA_RM_RES_CONS_USB);
	sipa_rm_delete_resource(SIPA_RM_RES_CONS_WIFI_UL);
	sipa_rm_delete_resource(SIPA_RM_RES_CONS_WIFI_DL);
}

static int sipa_create_ipa_prod(struct device *dev)
{
	int ret;
	struct sipa_rm_create_params rm_params = {};

	/* create prod */
	rm_params.name = SIPA_RM_RES_PROD_IPA;
	rm_params.reg_params.notify_cb = NULL;
	rm_params.reg_params.user_data = dev_get_drvdata(dev);
	rm_params.request_resource = sipa_rm_prepare_resume;
	rm_params.release_resource = sipa_rm_prepare_release;
	ret = sipa_rm_create_resource(&rm_params);
	if (ret)
		return ret;

	/* add dependencys */
	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WWAN_UL,
				     SIPA_RM_RES_PROD_IPA);
	if (ret < 0 && ret != -EINPROGRESS) {
		dev_err(dev, "add_dependency WWAN_UL fail.\n");
		sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
		return ret;
	}
	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WWAN_DL,
				     SIPA_RM_RES_PROD_IPA);
	if (ret < 0 && ret != -EINPROGRESS) {
		dev_err(dev, "add_dependency WWAN_DL fail.\n");
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_UL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
		return ret;
	}
	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_USB,
				     SIPA_RM_RES_PROD_IPA);
	if (ret < 0 && ret != -EINPROGRESS) {
		dev_err(dev, "add_dependency USB fail.\n");
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_UL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_DL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
		return ret;
	}

	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_UL,
				     SIPA_RM_RES_PROD_IPA);
	if (ret < 0 && ret != -EINPROGRESS) {
		dev_err(dev, "add_dependency wifi ul fail.\n");
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_UL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_DL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
		return ret;
	}

	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_DL,
				     SIPA_RM_RES_PROD_IPA);
	if (ret < 0 && ret != -EINPROGRESS) {
		dev_err(dev, "add_dependency wifi dl fail.\n");
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_UL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_DL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WIFI_UL,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
		return ret;
	}

	return 0;
}

static void sipa_destroy_ipa_prod(void)
{
	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_UL,
				  SIPA_RM_RES_PROD_IPA);
	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_WWAN_DL,
				  SIPA_RM_RES_PROD_IPA);
	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
				  SIPA_RM_RES_PROD_IPA);
	sipa_rm_delete_resource(SIPA_RM_RES_PROD_IPA);
}

static int sipa_init(struct device *dev)
{
	int ret = 0;

	/* init sipa hal */
	ret = sipa_hal_init(dev);
	if (ret)
		return ret;

	/* init sipa eps */
	ret = sipa_create_eps(dev);
	if (ret)
		goto ep_fail;

	/* init resource manager */
	ret = sipa_rm_init();
	if (ret)
		goto ep_fail;

	/* create basic cons */
	ret = sipa_create_rm_cons();
	if (ret)
		goto cons_fail;

	/* init usb cons */
	ret = sipa_rm_usb_cons_init();
	if (ret)
		goto usb_fail;

	/* create basic prod */
	ret = sipa_create_ipa_prod(dev);
	if (ret)
		goto prod_fail;

	/* init sipa skb transfer layer */
	ret = sipa_create_skb_xfer(dev);
	if (ret)
		goto xfer_fail;

	return 0;

xfer_fail:
	sipa_destroy_ipa_prod();
prod_fail:
	sipa_destroy_rm_cons();
usb_fail:
	sipa_rm_usb_cons_deinit();
cons_fail:
	sipa_rm_exit();
ep_fail:
	sipa_destroy_eps(dev);

	return ret;
}

static void sipa_notify_sender_flow_ctrl(struct work_struct *work)
{
	struct sipa_plat_drv_cfg *ipa = container_of(work,
						     struct sipa_plat_drv_cfg,
						     flow_ctrl_work);

	if (ipa->sender && ipa->sender->free_notify_net)
		wake_up(&ipa->sender->free_waitq);
}

static void sipa_prepare_suspend_work(struct device *dev)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (sipa_prepare_suspend(ipa->dev) && !ipa->power_flag) {
		/* 200ms can ensure that the skb data has been recycled */
		queue_delayed_work(ipa->power_wq, &ipa->power_work,
				   msecs_to_jiffies(200));
		dev_info(dev, "schedule suspend delayed work\n");
	}
}

static void sipa_prepare_resume_work(struct device *dev)
{
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (sipa_resume_work(dev) && ipa->power_flag) {
		/* 200ms can ensure that the skb data has been recycled */
		queue_delayed_work(ipa->power_wq, &ipa->power_work,
				   msecs_to_jiffies(200));
		dev_info(dev, "schedule_resume delayed_work\n");
	}
}

static void sipa_power_work(struct work_struct *work)
{
	struct delayed_work *power_delay_work = to_delayed_work(work);
	struct sipa_plat_drv_cfg *ipa = container_of(power_delay_work,
						     struct sipa_plat_drv_cfg,
						     power_work);

	if (ipa->power_flag)
		sipa_prepare_resume_work(ipa->dev);
	else
		sipa_prepare_suspend_work(ipa->dev);
}

static void sipa_record_reg(void)
{
	int i;
	int index = 0;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	if (!ipa->enable_cnt)
		return;

	for (i = 0; i < 0x2000; i += 0x4) {
		ipa->panic_reg[index] = readl_relaxed(ipa->glb_virt_base + i);
		index++;
	}

	return;
}

static int sipa_panic(struct notifier_block *self,
		      unsigned long val, void *reason)
{
	sipa_record_reg();

	return NOTIFY_DONE;
}

static int sipa_restart_handler(struct notifier_block *this,
				unsigned long mode, void *cmd)
{
	sipa_record_reg();

	return NOTIFY_DONE;
}

static struct notifier_block sipa_panic_cb = {
	.notifier_call = sipa_panic,
};

static struct notifier_block sipa_restart_nb = {
	.notifier_call = sipa_restart_handler,
	.priority = 128,
};

static int sipa_plat_drv_probe(struct platform_device *pdev_p)
{
	int ret;
	dma_addr_t dma_addr;
	struct device *dev = &pdev_p->dev;
	struct sipa_plat_drv_cfg *ipa;

	/*
	 * SIPA probe function can be called for multiple
	 * times as the same probe function handles multiple
	 * compatibilities
	 */
	dev_info(dev, "driver probing start\n");

	ipa = devm_kzalloc(dev, sizeof(*ipa), GFP_KERNEL);
	if (!ipa)
		return -ENOMEM;

	s_sipa_core = ipa;
	dev_set_drvdata(dev, ipa);

	ipa->dev = dev;
	ret = sipa_parse_dts_configuration(pdev_p, ipa);
	if (ret) {
		dev_err(dev, "dts parsing failed\n");
		return ret;
	}

	if (dma_set_mask_and_coherent(dev, ipa->hw_data->dma_mask))
		dev_warn(dev, "no suitable DMA availabld\n");

	ipa->suspend_stage = SIPA_SUSPEND_MASK;

	spin_lock_init(&ipa->enable_lock);
	ipa->enable_cnt = 0;

	spin_lock_init(&ipa->mode_lock);

	mutex_init(&ipa->resume_lock);
	INIT_WORK(&ipa->flow_ctrl_work, sipa_notify_sender_flow_ctrl);
	INIT_DELAYED_WORK(&ipa->power_work, sipa_power_work);

	ret = sipa_init(dev);
	if (ret) {
		dev_err(dev, "init failed %d\n", ret);
		return ret;
	}

	device_init_wakeup(dev, true);

	ipa->power_wq = create_workqueue("sipa_power_wq");
	if (!ipa->power_wq) {
		dev_err(dev, "power wq create failed\n");
		return -ENOMEM;
	}

	ipa->panic_reg = dma_alloc_coherent(dev, 0x2000, &dma_addr, GFP_KERNEL);
	if (!ipa->panic_reg)
		return -ENOMEM;
	memset(ipa->panic_reg, 0, 0x2000);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &sipa_panic_cb);
	register_restart_handler(&sipa_restart_nb);

	pm_runtime_enable(dev);
	sipa_init_debugfs(ipa);

	return ret;
}

static struct sipa_hw_data sipa_n6pro_hw_data = {
	.standalone_subsys = true,
	.dma_mask = DMA_BIT_MASK(36),
};

static const struct of_device_id sipa_plat_drv_match[] = {
	{ .compatible = "sprd,qogirn6pro-sipa", .data = &sipa_n6pro_hw_data},
	{}
};

/**
 * sipa_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
 */
static int sipa_ap_suspend(struct device *dev)
{
	return 0;
}

/**
 * sipa_ap_resume() - resume callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP resume
 * operation is invoked.
 *
 * Always returns 0 since resume should always succeed.
 */
static int sipa_ap_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sipa_pm_ops = {
	.suspend_noirq = sipa_ap_suspend,
	.resume_noirq = sipa_ap_resume,
};

static struct platform_driver sipa_plat_drv = {
	.probe = sipa_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &sipa_pm_ops,
		.of_match_table = sipa_plat_drv_match,
	},
};
module_platform_driver(sipa_plat_drv);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc IPA HW device driver");
