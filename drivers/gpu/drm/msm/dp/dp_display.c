/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/extcon.h>
#ifndef CONFIG_SEC_DISPLAYPORT
#include <linux/soc/qcom/fsa4480-i2c.h>
#endif

#include "sde_connector.h"

#include "msm_drv.h"
#include "dp_hpd.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_link.h"
#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_audio.h"
#include "dp_display.h"
#include "sde_hdcp.h"
#include "dp_debug.h"

#ifdef CONFIG_SEC_DISPLAYPORT
#include <linux/switch.h>
#include <linux/reboot.h>
#include <linux/sec_displayport.h>
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include <linux/displayport_bigdata.h>
#endif
#include "secdp.h"
#include "secdp_sysfs.h"
#define CCIC_DP_NOTI_REG_DELAY		30000

struct secdp_event {
	struct dp_display_private *dp;
	u32 id;
};

#define SECDP_EVENT_Q_MAX 4

struct secdp_event_data {
	wait_queue_head_t event_q;
	u32 pndx;
	u32 gndx;
	struct secdp_event event_list[SECDP_EVENT_Q_MAX];
	spinlock_t event_lock;
};
#endif

#define DP_MST_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

static struct dp_display *g_dp_display;
#define HPD_STRING_SIZE 30

#if defined(CONFIG_SEC_DISPLAYPORT) && defined(CONFIG_SWITCH)
static struct switch_dev switch_secdp_hpd = {
	.name = "secdp",
};

static struct switch_dev switch_secdp_msg = {
	.name = "secdp_msg",
};
#endif

struct dp_hdcp_dev {
	void *fd;
	struct sde_hdcp_ops *ops;
	enum sde_hdcp_version ver;
};

struct dp_hdcp {
	void *data;
	struct sde_hdcp_ops *ops;

	u32 source_cap;

	struct dp_hdcp_dev dev[HDCP_VERSION_MAX];
};

struct dp_mst {
	bool mst_active;

	bool drm_registered;
	struct dp_mst_drm_cbs cbs;
};

struct dp_display_private {
	char *name;
	int irq;

	/* state variables */
	bool core_initialized;
	bool power_on;
	bool is_connected;

	atomic_t aborted;

	struct platform_device *pdev;
	struct device_node *aux_switch_node;		/* SECDP: not used, dummy */
	struct dentry *root;
	struct completion notification_comp;

	struct dp_hpd     *hpd;
	struct dp_parser  *parser;
	struct dp_power   *power;
	struct dp_catalog *catalog;
	struct dp_aux     *aux;
	struct dp_link    *link;
	struct dp_panel   *panel;
	struct dp_ctrl    *ctrl;
	struct dp_debug   *debug;

	struct dp_panel *active_panels[DP_STREAM_MAX];
	struct dp_hdcp hdcp;

#ifdef CONFIG_SEC_DISPLAYPORT
	struct completion dp_off_comp;
	struct completion dp_disconnect_comp;
	int is_dp_disconnecting;

	struct secdp_sysfs	*sysfs;
	struct secdp_misc	 sec;

	/* for ccic event handler */
	struct secdp_event_data dp_event;
	struct task_struct *ev_thread;
	struct list_head attention_head;
	struct mutex attention_lock;
	struct workqueue_struct *workq;
	atomic_t notification_status;
	struct notifier_block reboot_nb;
#endif

	struct dp_hpd_cb hpd_cb;
	struct dp_display_mode mode;
	struct dp_display dp_display;
	struct msm_drm_private *priv;

	struct workqueue_struct *wq;
	struct delayed_work hdcp_cb_work;
	struct work_struct connect_work;
	struct work_struct attention_work;
	struct mutex session_lock;
	bool suspended;
	bool hdcp_delayed_off;

	u32 active_stream_cnt;
	struct dp_mst mst;

	u32 tot_dsc_blks_in_use;

	bool process_hpd_connect;

	struct notifier_block usb_nb;
};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

#ifdef CONFIG_SEC_DISPLAYPORT
struct dp_display_private *g_secdp_priv;

static void secdp_handle_attention(struct dp_display_private *dp);

bool secdp_check_if_lpm_mode(void)
{
	bool retval = false;

	if (lpcharge)
		retval = true;

	pr_debug("LPM: %d\n", retval);
	return retval;
}

void secdp_send_poor_connection_event(void)
{
	struct dp_display_private *dp = g_secdp_priv;

	pr_info("poor connection!");
	dp->link->poor_connection = true;
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_secdp_msg, 1);
	switch_set_state(&switch_secdp_msg, 0);
#endif

	dp->sec.dex.prev = dp->sec.dex.curr = DEX_DISABLED;
}

bool secdp_get_power_status(void)
{
	if (!g_secdp_priv)
		return false;

	return g_secdp_priv->power_on;
}

/* check if dp cable has connected or not */
bool secdp_get_cable_status(void)
{
	if (!g_secdp_priv)
		return false;

	return g_secdp_priv->sec.cable_connected;
}

/* check if hpd high has come or not */
int secdp_get_hpd_status(void)
{
	if (!g_secdp_priv)
		return 0;

	return atomic_read(&g_secdp_priv->sec.hpd);
}

int secdp_read_branch_revision(struct dp_display_private *dp)
{
	int rlen = 0;
	struct drm_dp_aux *drm_aux;
	char *fw_ver;

	if (!dp || !dp->aux || !dp->aux->drm_aux) {
		pr_err("invalid param\n");
		goto end;
	}

	drm_aux = dp->aux->drm_aux;
	fw_ver = dp->sec.dex.fw_ver;

	rlen = drm_dp_dpcd_read(drm_aux, DPCD_BRANCH_HW_REVISION, fw_ver,
						LEN_BRANCH_REVISION);
	if (rlen < 3) {
		pr_err("read fail, rlen(%d)\n", rlen);
		goto end;
	}

	pr_info("branch revision: HW(0x%X), SW(0x%X, 0x%X)\n",
		fw_ver[0], fw_ver[1], fw_ver[2]);

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_ADAPTER_HWID, fw_ver[0]);
	secdp_bigdata_save_item(BD_ADAPTER_FWVER, (fw_ver[1] << 8) | fw_ver[2]);
#endif

end:
	return rlen;
}

void secdp_clear_branch_info(struct dp_display_private *dp)
{
	int i;
	char *fw_ver;

	if (!dp)
		goto end;

	fw_ver = dp->sec.dex.fw_ver;

	for (i = 0; i < LEN_BRANCH_REVISION; i++)
		fw_ver[i] = 0;

end:
	return;
}

static enum dex_support_res_t secdp_check_adapter_type(uint64_t ven_id, uint64_t prod_id)
{
	struct dp_display_private *dp = g_secdp_priv;
	enum dex_support_res_t type = DEX_RES_DFT;	/* default resolution */

	pr_info("ven_id(0x%04x), prod_id(0x%04x)\n", (uint)ven_id, (uint)prod_id);

#ifdef NOT_SUPPORT_DEX_RES_CHANGE
	return DEX_RES_NOT_SUPPORT;
#endif

	if (dp->parser->dex_dft_res > DEX_RES_NOT_SUPPORT) {
		type = dp->parser->dex_dft_res;
		goto end;
	}

	if (ven_id == SAMSUNG_VENDOR_ID) {
		switch (prod_id) {
			case 0xa029: /* PAD */
			case 0xa020: /* Station */
			case 0xa02a:
			case 0xa02b:
			case 0xa02c:
			case 0xa02d:
			case 0xa02e:
			case 0xa02f:
			case 0xa030:
			case 0xa031:
			case 0xa032:
			case 0xa033:
				type = DEX_RES_MAX;
				break;
		default:
			pr_info("it's SS dongle but UNKNOWN\n");
			break;
		}
	} else {
		pr_info("it's NOT SS dongle\n");
	}

end:
	pr_info("type: %s\n", secdp_dex_res_to_string(type));
	return type;
}
#endif

#ifdef SECDP_USE_WAKELOCK
static void secdp_init_wakelock(struct dp_display_private *dp)
{
	struct wake_lock * wlock = &dp->sec.wlock;
	wake_lock_init(wlock, WAKE_LOCK_SUSPEND, "secdp_wlock");
}

static void secdp_destroy_wakelock(struct dp_display_private *dp)
{
	struct wake_lock * wlock = &dp->sec.wlock;
	wake_lock_destroy(wlock);
}

static void secdp_set_wakelock(struct dp_display_private *dp, bool en)
{
	struct wake_lock * wlock = &dp->sec.wlock;
	
	pr_debug("++ en<%d>, active<%d>\n", en, wake_lock_active(wlock));

	if (en) {
		if (!wake_lock_active(wlock)) {
			wake_lock(wlock);
			pr_debug("set!\n");
		}
	} else {
		if (wake_lock_active(wlock)) {
			wake_unlock(wlock);
			pr_debug("clear!\n");
		}
	}

	pr_debug("-- en<%d>, active<%d>\n", en, wake_lock_active(wlock));
}
#endif

static inline bool dp_display_is_hdcp_enabled(struct dp_display_private *dp)
{
	return dp->link->hdcp_status.hdcp_version && dp->hdcp.ops;
}

static irqreturn_t dp_display_irq(int irq, void *dev_id)
{
	struct dp_display_private *dp = dev_id;

	if (!dp) {
		pr_err("invalid data\n");
		return IRQ_NONE;
	}

	/* DP HPD isr */
	if (dp->hpd->type ==  DP_HPD_LPHW)
		dp->hpd->isr(dp->hpd);

	/* DP controller isr */
	dp->ctrl->isr(dp->ctrl);

	/* DP aux isr */
	dp->aux->isr(dp->aux);

	/* HDCP isr */
	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->isr) {
		if (dp->hdcp.ops->isr(dp->hdcp.data))
			pr_err("dp_hdcp_isr failed\n");
	}

	return IRQ_HANDLED;
}
static bool dp_display_is_ds_bridge(struct dp_panel *panel)
{
	return (panel->dpcd[DP_DOWNSTREAMPORT_PRESENT] &
		DP_DWN_STRM_PORT_PRESENT);
}

static bool dp_display_is_sink_count_zero(struct dp_display_private *dp)
{
	return dp_display_is_ds_bridge(dp->panel) &&
		(dp->link->sink_count.count == 0);
}

static bool dp_display_is_ready(struct dp_display_private *dp)
{
	return dp->hpd->hpd_high && dp->is_connected &&
		!dp_display_is_sink_count_zero(dp) &&
		dp->hpd->alt_mode_cfg_done;
}

static void dp_display_audio_enable(struct dp_display_private *dp, bool enable)
{
	struct dp_panel *dp_panel;
	int idx = 0;

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];

		if (dp_panel->audio_supported) {
			if (enable) {
				dp_panel->audio->bw_code =
					dp->link->link_params.bw_code;
				dp_panel->audio->lane_count =
					dp->link->link_params.lane_count;
				dp_panel->audio->on(dp_panel->audio);
			} else {
				dp_panel->audio->off(dp_panel->audio);
			}
		}
	}
}

static void dp_display_update_hdcp_status(struct dp_display_private *dp,
					bool reset)
{
	if (reset) {
		dp->link->hdcp_status.hdcp_state = HDCP_STATE_INACTIVE;
		dp->link->hdcp_status.hdcp_version = HDCP_VERSION_NONE;
	}

	memset(dp->debug->hdcp_status, 0, sizeof(dp->debug->hdcp_status));

	snprintf(dp->debug->hdcp_status, sizeof(dp->debug->hdcp_status),
		"%s: %s\ncaps: %d\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version),
		sde_hdcp_state_name(dp->link->hdcp_status.hdcp_state),
		dp->hdcp.source_cap);
}

static void dp_display_update_hdcp_info(struct dp_display_private *dp)
{
	void *fd = NULL;
	struct dp_hdcp_dev *dev = NULL;
	struct sde_hdcp_ops *ops = NULL;
	int i = HDCP_VERSION_2P2;

	dp_display_update_hdcp_status(dp, true);

	dp->hdcp.data = NULL;
	dp->hdcp.ops = NULL;

	if (dp->debug->hdcp_disabled || dp->debug->sim_mode)
		return;

	while (i) {
		dev = &dp->hdcp.dev[i];
		ops = dev->ops;
		fd = dev->fd;

		i >>= 1;

		if (!(dp->hdcp.source_cap & dev->ver))
			continue;

		if (ops->sink_support(fd)) {
			dp->hdcp.data = fd;
			dp->hdcp.ops = ops;
			dp->link->hdcp_status.hdcp_version = dev->ver;
			break;
		}
	}

	pr_debug("HDCP version supported: %s\n",
		sde_hdcp_version(dp->link->hdcp_status.hdcp_version));
}

static void dp_display_check_source_hdcp_caps(struct dp_display_private *dp)
{
	int i;
	struct dp_hdcp_dev *hdcp_dev = dp->hdcp.dev;

	if (dp->debug->hdcp_disabled) {
		pr_debug("hdcp disabled\n");
		return;
	}

	pr_debug("+++\n");

	for (i = 0; i < HDCP_VERSION_MAX; i++) {
		struct dp_hdcp_dev *dev = &hdcp_dev[i];
		struct sde_hdcp_ops *ops = dev->ops;
		void *fd = dev->fd;

		if (!fd || !ops)
			continue;

		if (ops->set_mode && ops->set_mode(fd, dp->mst.mst_active))
			continue;

		if (!(dp->hdcp.source_cap & dev->ver) &&
				ops->feature_supported &&
				ops->feature_supported(fd))
			dp->hdcp.source_cap |= dev->ver;
	}

	pr_debug("hdcp.source_cap: 0x%x\n", dp->hdcp.source_cap);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_HDCP_VER,
			(dp->hdcp.source_cap & HDCP_VERSION_2P2) ? "hdcp2" :
			((dp->hdcp.source_cap & HDCP_VERSION_1X) ? "hdcp1" : "X"));
#endif

	dp_display_update_hdcp_status(dp, false);
}

static void dp_display_hdcp_register_streams(struct dp_display_private *dp)
{
	int rc;
	size_t i;
	struct sde_hdcp_ops *ops = dp->hdcp.ops;
	void *data = dp->hdcp.data;

	if (dp_display_is_ready(dp) && dp->mst.mst_active && ops &&
			ops->register_streams){
		struct stream_info streams[DP_STREAM_MAX];
		int index = 0;

		pr_debug("Registering all active panel streams with HDCP\n");
		for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
			if (!dp->active_panels[i])
				continue;
			streams[index].stream_id = i;
			streams[index].virtual_channel =
				dp->active_panels[i]->vcpi;
			index++;
		}

		if (index > 0) {
			rc = ops->register_streams(data, index, streams);
			if (rc)
				pr_err("failed to register streams. rc = %d\n",
					rc);
		}
	}
}

static void dp_display_hdcp_deregister_stream(struct dp_display_private *dp,
		enum dp_stream_id stream_id)
{
	if (dp->hdcp.ops->deregister_streams) {
		struct stream_info stream = {stream_id,
				dp->active_panels[stream_id]->vcpi};

		pr_debug("Deregistering stream within HDCP library");
		dp->hdcp.ops->deregister_streams(dp->hdcp.data, 1, &stream);
	}
}

static void dp_display_hdcp_cb_work(struct work_struct *work)
{
	struct dp_display_private *dp;
	struct delayed_work *dw = to_delayed_work(work);
	struct sde_hdcp_ops *ops;
	struct dp_link_hdcp_status *status;
	void *data;
	int rc = 0;
	u32 hdcp_auth_state;
	u8 sink_status = 0;

	pr_debug("+++\n");

	dp = container_of(dw, struct dp_display_private, hdcp_cb_work);

	if (!dp->power_on || !dp->is_connected || atomic_read(&dp->aborted))
		return;

	if (dp->suspended) {
		pr_debug("System suspending. Delay HDCP operations\n");
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
		return;
	}

	if (dp->hdcp_delayed_off) {
		if (dp->hdcp.ops && dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);
		dp_display_update_hdcp_status(dp, true);
		dp->hdcp_delayed_off = false;
	}

	drm_dp_dpcd_readb(dp->aux->drm_aux, DP_SINK_STATUS, &sink_status);
	sink_status &= (DP_RECEIVE_PORT_0_STATUS | DP_RECEIVE_PORT_1_STATUS);
#ifndef CONFIG_SEC_DISPLAYPORT
	if (sink_status < 1) {
		pr_debug("Sink not synchronized. Queuing again then exiting\n");
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
		return;
	}
#else
	if (sink_status < 1 && !dp->ctrl->get_link_train_status(dp->ctrl)) {
		pr_info("hdcp_retry: %d\n", dp->sec.hdcp_retry);
		if (dp->sec.hdcp_retry >= MAX_CNT_HDCP_RETRY) {
			pr_debug("stop queueing!\n");
			schedule_delayed_work(&dp->sec.poor_discon_work, msecs_to_jiffies(10));
			return;
		}
		dp->sec.hdcp_retry++;

		pr_debug("Sink not synchronized. Queuing again then exiting\n");
		queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
		return;
	}
#endif

	status = &dp->link->hdcp_status;

	if (status->hdcp_state == HDCP_STATE_INACTIVE) {
		dp_display_check_source_hdcp_caps(dp);
		dp_display_update_hdcp_info(dp);

		if (dp_display_is_hdcp_enabled(dp)) {
			if (dp->hdcp.ops && dp->hdcp.ops->on &&
					dp->hdcp.ops->on(dp->hdcp.data)) {
				dp_display_update_hdcp_status(dp, true);
				return;
			}
			status->hdcp_state = HDCP_STATE_AUTHENTICATING;
		} else {
			dp_display_update_hdcp_status(dp, true);
			return;
		}
	}

	rc = dp->catalog->ctrl.read_hdcp_status(&dp->catalog->ctrl);
	if (rc >= 0) {
		hdcp_auth_state = (rc >> 20) & 0x3;
		pr_debug("hdcp auth state %d\n", hdcp_auth_state);
	}

	ops = dp->hdcp.ops;
	data = dp->hdcp.data;

	pr_debug("%s: %s\n", sde_hdcp_version(status->hdcp_version),
		sde_hdcp_state_name(status->hdcp_state));

	dp_display_update_hdcp_status(dp, false);

#ifdef CONFIG_SEC_DISPLAYPORT
	if (secdp_get_reboot_status()) {
		pr_info("shutdown\n");
		return;
	}
#endif

	if (dp->debug->force_encryption && ops && ops->force_encryption)
		ops->force_encryption(data, dp->debug->force_encryption);

	switch (status->hdcp_state) {
	case HDCP_STATE_AUTHENTICATING:
		pr_info("start authenticaton\n");

		dp_display_hdcp_register_streams(dp);

#ifdef CONFIG_SEC_DISPLAYPORT
		if (status->hdcp_version < HDCP_VERSION_2P2)
			secdp_reset_link_status(dp->link);			
#endif
		if (dp->hdcp.ops && dp->hdcp.ops->authenticate)
			rc = dp->hdcp.ops->authenticate(data);
		break;
	case HDCP_STATE_AUTH_FAIL:
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_inc_error_cnt(ERR_HDCP_AUTH);
#endif
		if (dp_display_is_ready(dp) && dp->power_on) {

			if (ops && ops->on && ops->on(data)) {
				dp_display_update_hdcp_status(dp, true);
				return;
			}

			pr_info("Reauthenticating\n");

			dp_display_hdcp_register_streams(dp);
			status->hdcp_state = HDCP_STATE_AUTHENTICATING;
			if (ops && ops->reauthenticate) {
				rc = ops->reauthenticate(data);
				if (rc)
					pr_err("reauth failed rc=%d\n", rc);
			}
		} else {
			pr_debug("not reauthenticating, cable disconnected\n");
		}
		break;
	default:
		dp_display_hdcp_register_streams(dp);
		break;
	}
}

static void dp_display_notify_hdcp_status_cb(void *ptr,
		enum sde_hdcp_state state)
{
	struct dp_display_private *dp = ptr;

	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	pr_debug("+++\n");

	dp->link->hdcp_status.hdcp_state = state;

	queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ/4);
}

static void dp_display_deinitialize_hdcp(struct dp_display_private *dp)
{
	if (!dp) {
		pr_err("invalid input\n");
		return;
	}

	sde_dp_hdcp2p2_deinit(dp->hdcp.data);
}

static int dp_display_initialize_hdcp(struct dp_display_private *dp)
{
	struct sde_hdcp_init_data hdcp_init_data;
	struct dp_parser *parser;
	void *fd;
	int rc = 0;

	if (!dp) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	parser = dp->parser;

	hdcp_init_data.client_id     = HDCP_CLIENT_DP;
	hdcp_init_data.drm_aux       = dp->aux->drm_aux;
	hdcp_init_data.cb_data       = (void *)dp;
	hdcp_init_data.workq         = dp->wq;
	hdcp_init_data.sec_access    = true;
	hdcp_init_data.notify_status = dp_display_notify_hdcp_status_cb;
	hdcp_init_data.dp_ahb        = &parser->get_io(parser, "dp_ahb")->io;
	hdcp_init_data.dp_aux        = &parser->get_io(parser, "dp_aux")->io;
	hdcp_init_data.dp_link       = &parser->get_io(parser, "dp_link")->io;
	hdcp_init_data.dp_p0         = &parser->get_io(parser, "dp_p0")->io;
	hdcp_init_data.qfprom_io     = &parser->get_io(parser,
						"qfprom_physical")->io;
	hdcp_init_data.hdcp_io       = &parser->get_io(parser,
						"hdcp_physical")->io;
	hdcp_init_data.revision      = &dp->panel->link_info.revision;
	hdcp_init_data.msm_hdcp_dev  = dp->parser->msm_hdcp_dev;

	fd = sde_hdcp_1x_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(fd)) {
		pr_err("Error initializing HDCP 1.x\n");
		rc = -EINVAL;
		goto error;
	}

	dp->hdcp.dev[HDCP_VERSION_1X].fd = fd;
	dp->hdcp.dev[HDCP_VERSION_1X].ops = sde_hdcp_1x_get(fd);
	dp->hdcp.dev[HDCP_VERSION_1X].ver = HDCP_VERSION_1X;
	pr_debug("HDCP 1.3 initialized\n");

	fd = sde_dp_hdcp2p2_init(&hdcp_init_data);
	if (IS_ERR_OR_NULL(fd)) {
		pr_err("Error initializing HDCP 2.x\n");
		rc = -EINVAL;
		goto error;
	}

	dp->hdcp.dev[HDCP_VERSION_2P2].fd = fd;
	dp->hdcp.dev[HDCP_VERSION_2P2].ops = sde_dp_hdcp2p2_get(fd);
	dp->hdcp.dev[HDCP_VERSION_2P2].ver = HDCP_VERSION_2P2;
	pr_debug("HDCP 2.2 initialized\n");

	return 0;
error:
	dp_display_deinitialize_hdcp(dp);

	return rc;
}

static int dp_display_bind(struct device *dev, struct device *master,
		void *data)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto end;
	}

	drm = dev_get_drvdata(master);
	dp = platform_get_drvdata(pdev);
	if (!drm || !dp) {
		pr_err("invalid param(s), drm %pK, dp %pK\n",
				drm, dp);
		rc = -EINVAL;
		goto end;
	}

	dp->dp_display.drm_dev = drm;
	dp->priv = drm->dev_private;

end:
	return rc;
}

static void dp_display_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct dp_display_private *dp;
	struct platform_device *pdev = to_platform_device(dev);

	pr_debug("+++\n");

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	dp = platform_get_drvdata(pdev);
	if (!dp) {
		pr_err("Invalid params\n");
		return;
	}

	if (dp->power)
		(void)dp->power->power_client_deinit(dp->power);
	if (dp->aux)
		(void)dp->aux->drm_aux_deregister(dp->aux);
	dp_display_deinitialize_hdcp(dp);
}

static const struct component_ops dp_display_comp_ops = {
	.bind = dp_display_bind,
	.unbind = dp_display_unbind,
};

static void dp_display_send_hpd_event(struct dp_display_private *dp)
{
	struct drm_device *dev = NULL;
	struct drm_connector *connector;
	char name[HPD_STRING_SIZE], status[HPD_STRING_SIZE],
		bpp[HPD_STRING_SIZE], pattern[HPD_STRING_SIZE];
	char *envp[5];

	if (dp->mst.mst_active) {
		pr_debug("skip notification for mst mode\n");
		return;
	}

	connector = dp->dp_display.base_connector;

	if (!connector) {
		pr_err("connector not set\n");
		return;
	}

#ifdef CONFIG_SEC_DISPLAYPORT
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_secdp_hpd, (int)dp->is_connected);
	pr_info("secdp displayport uevent: %d\n", dp->is_connected);
#endif
	msleep(100);
#endif
	connector->status = connector->funcs->detect(connector, false);

	dev = connector->dev;

	snprintf(name, HPD_STRING_SIZE, "name=%s", connector->name);
	snprintf(status, HPD_STRING_SIZE, "status=%s",
		drm_get_connector_status_name(connector->status));
	snprintf(bpp, HPD_STRING_SIZE, "bpp=%d",
		dp_link_bit_depth_to_bpp(
		dp->link->test_video.test_bit_depth));
	snprintf(pattern, HPD_STRING_SIZE, "pattern=%d",
		dp->link->test_video.test_video_pattern);

	pr_debug("[%s]:[%s] [%s] [%s]\n", name, status, bpp, pattern);
	envp[0] = name;
	envp[1] = status;
	envp[2] = bpp;
	envp[3] = pattern;
	envp[4] = NULL;
	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
			envp);
}

static int dp_display_send_hpd_notification(struct dp_display_private *dp)
{
	int ret = 0;
	bool hpd = dp->is_connected;

#ifdef CONFIG_SEC_DISPLAYPORT
	if (hpd && !secdp_get_cable_status()) {
		pr_info("cable is out\n");
		return -EIO;
	}
	pr_debug("+++\n");
#endif

	dp->aux->state |= DP_STATE_NOTIFICATION_SENT;

	if (!dp->mst.mst_active)
		dp->dp_display.is_sst_connected = hpd;
	else
		dp->dp_display.is_sst_connected = false;

	reinit_completion(&dp->notification_comp);
	dp_display_send_hpd_event(dp);

#ifdef CONFIG_SEC_DISPLAYPORT
	if (!hpd && !dp->power_on) {
		pr_info("DP is already off, no wait\n");
		return 0;
	}

	atomic_set(&dp->notification_status, 1);
#endif

	if (hpd && dp->mst.mst_active)
		goto skip_wait;

	if (!dp->mst.mst_active && (dp->power_on == hpd))
		goto skip_wait;

	if (!wait_for_completion_timeout(&dp->notification_comp,
						HZ * 5)) {
		pr_warn("%s timeout\n", hpd ? "connect" : "disconnect");
		ret = -EINVAL;
	}

	pr_debug("---, ret(%d)\n", ret);
	return ret;
skip_wait:
	return 0;
}

static void dp_display_update_mst_state(struct dp_display_private *dp,
					bool state)
{
	dp->mst.mst_active = state;
	dp->panel->mst_state = state;
}

static void dp_display_process_mst_hpd_high(struct dp_display_private *dp,
						bool mst_probe)
{
	bool is_mst_receiver;
	struct dp_mst_hpd_info info;
	const int clear_mstm_ctrl_timeout = 100000;
	u8 old_mstm_ctrl;
	int ret;

	if (!dp->parser->has_mst || !dp->mst.drm_registered) {
		DP_MST_DEBUG("mst not enabled. has_mst:%d, registered:%d\n",
				dp->parser->has_mst, dp->mst.drm_registered);
		return;
	}

	DP_MST_DEBUG("mst_hpd_high work. mst_probe:%d\n", mst_probe);

	if (!dp->mst.mst_active) {
		is_mst_receiver = dp->panel->read_mst_cap(dp->panel);

		if (!is_mst_receiver) {
			DP_MST_DEBUG("sink doesn't support mst\n");
			return;
		}

		/* clear sink mst state */
		drm_dp_dpcd_readb(dp->aux->drm_aux, DP_MSTM_CTRL,
				&old_mstm_ctrl);
		drm_dp_dpcd_writeb(dp->aux->drm_aux, DP_MSTM_CTRL, 0);

		/* add extra delay if MST state is not cleared */
		if (old_mstm_ctrl) {
			DP_MST_DEBUG("MSTM_CTRL is not cleared, wait %dus\n",
					clear_mstm_ctrl_timeout);
			usleep_range(clear_mstm_ctrl_timeout,
				clear_mstm_ctrl_timeout + 1000);
		}

		ret = drm_dp_dpcd_writeb(dp->aux->drm_aux, DP_MSTM_CTRL,
				 DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC);
		if (ret < 0) {
			pr_err("sink mst enablement failed\n");
			return;
		}

		dp_display_update_mst_state(dp, true);
	} else if (dp->mst.mst_active && mst_probe) {
		info.mst_protocol = dp->parser->has_mst_sideband;
		info.mst_port_cnt = dp->debug->mst_port_cnt;
		info.edid = dp->debug->get_edid(dp->debug);

		if (dp->mst.cbs.hpd)
			dp->mst.cbs.hpd(&dp->dp_display, true, &info);
	}

	DP_MST_DEBUG("mst_hpd_high. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_host_init(struct dp_display_private *dp)
{
	bool flip = false;
	bool reset;

	if (dp->core_initialized)
		return;

	pr_debug("+++\n");

	if (dp->hpd->orientation == ORIENTATION_CC2)
		flip = true;

	reset = dp->debug->sim_mode ? false : !dp->hpd->multi_func;

	dp->power->init(dp->power, flip);
	dp->hpd->host_init(dp->hpd, &dp->catalog->hpd);
	dp->ctrl->init(dp->ctrl, flip, reset);
	dp->aux->init(dp->aux, dp->parser->aux_cfg);
	enable_irq(dp->irq);
	dp->core_initialized = true;

	/* log this as it results from user action of cable connection */
	pr_info("[OK]\n");
}

static void dp_display_host_deinit(struct dp_display_private *dp)
{
	if (!dp->core_initialized)
		return;

	if (dp->active_stream_cnt) {
		pr_debug("active stream present\n");
		return;
	}

	pr_debug("+++\n");

	dp->aux->deinit(dp->aux);
	dp->ctrl->deinit(dp->ctrl);
	dp->hpd->host_deinit(dp->hpd, &dp->catalog->hpd);
	dp->power->deinit(dp->power);
	disable_irq(dp->irq);
	dp->core_initialized = false;
	dp->aux->state = 0;

	/* log this as it results from user action of cable dis-connection */
	pr_info("[OK]\n");
}

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc = -EINVAL;
#ifdef CONFIG_SEC_DISPLAYPORT
	bool is_poor_connection = false;
#endif

	mutex_lock(&dp->session_lock);

	if (dp->is_connected) {
		pr_debug("dp already connected, skipping hpd high processing");
#ifndef CONFIG_SEC_DISPLAYPORT
		mutex_unlock(&dp->session_lock);
#endif
		rc = -EISCONN;
		goto end;
	}

	dp->is_connected = true;

	pr_debug("+++\n");

	dp->dp_display.max_pclk_khz = min(dp->parser->max_pclk_khz,
					dp->debug->max_pclk_khz);

	dp_display_host_init(dp);

	if (dp->debug->psm_enabled) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, false);
		dp->debug->psm_enabled = false;
	}

	if (!dp->dp_display.base_connector)
		goto end;

	rc = dp->panel->read_sink_caps(dp->panel,
			dp->dp_display.base_connector, dp->hpd->multi_func);

	/*
	 * ETIMEDOUT --> cable may have been removed
	 * ENOTCONN --> no downstream device connected
	 */
#ifndef CONFIG_SEC_DISPLAYPORT
	if (rc == -ETIMEDOUT || rc == -ENOTCONN) {
		dp->is_connected = false;
		goto end;
	}
#else
	if (rc) {
		if (!secdp_get_hpd_status() || !secdp_get_cable_status()
			|| rc == -EIO) {
			pr_info("hpd_low or cable_lost or AUX failure\n");
			is_poor_connection = true;
			goto off;
		}

		if (rc == -ENOTCONN) {
			pr_debug("no downstream devices connected.\n");
			rc = -EINVAL;
			goto off;
		}

		pr_info("fall through failsafe\n");
		is_poor_connection = true;
		goto notify;
	}
	dp->sec.dex.prev = secdp_check_dex_mode();
	pr_info("dex.setting_ui: %d, dex.curr: %d\n", dp->sec.dex.setting_ui, dp->sec.dex.curr);
	secdp_read_branch_revision(dp);
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (dp->sec.dex.prev)
		secdp_bigdata_save_item(BD_DP_MODE, "DEX");
	else
		secdp_bigdata_save_item(BD_DP_MODE, "MIRROR");
#endif

	dp->link->process_request(dp->link);
	dp->panel->handle_sink_request(dp->panel);

	dp_display_process_mst_hpd_high(dp, false);

#ifdef CONFIG_SEC_DISPLAYPORT
notify:
#endif
	rc = dp->ctrl->on(dp->ctrl, dp->mst.mst_active,
				dp->panel->fec_en, false);
	if (rc) {
		dp->is_connected = false;
#ifndef CONFIG_SEC_DISPLAYPORT
		goto end;
#else
		is_poor_connection = true;
		goto off;
#endif
	}

	dp->process_hpd_connect = false;
	dp_display_process_mst_hpd_high(dp, true);

#ifdef SECDP_USE_WAKELOCK
	secdp_set_wakelock(dp, true);
#endif
#ifdef CONFIG_SEC_DISPLAYPORT
	if (is_poor_connection)
		secdp_send_poor_connection_event();
#endif

end:
	mutex_unlock(&dp->session_lock);

	if (!rc)
		dp_display_send_hpd_notification(dp);

	return rc;

#ifdef CONFIG_SEC_DISPLAYPORT
off:
	dp->is_connected = false;
	mutex_unlock(&dp->session_lock);

	if (is_poor_connection)
		secdp_send_poor_connection_event();

	dp_display_host_deinit(dp);
	return rc;
#endif
}

static void dp_display_process_mst_hpd_low(struct dp_display_private *dp)
{
	struct dp_mst_hpd_info info = {0};

	if (dp->mst.mst_active) {
		DP_MST_DEBUG("mst_hpd_low work\n");

		if (dp->mst.cbs.hpd) {
			info.mst_protocol = dp->parser->has_mst_sideband;
			dp->mst.cbs.hpd(&dp->dp_display, false, &info);
		}
		dp_display_update_mst_state(dp, false);
	}

	DP_MST_DEBUG("mst_hpd_low. mst_active:%d\n", dp->mst.mst_active);
}

static int dp_display_process_hpd_low(struct dp_display_private *dp)
{
	int rc = 0;
	struct dp_link_hdcp_status *status;

	pr_debug("++\n");

	mutex_lock(&dp->session_lock);
	pr_debug("+++\n");

	status = &dp->link->hdcp_status;
	dp->is_connected = false;
	dp->process_hpd_connect = false;

#ifdef CONFIG_SEC_DISPLAYPORT
	cancel_delayed_work_sync(&dp->sec.hdcp_start_work);
	cancel_delayed_work(&dp->sec.link_status_work);
	cancel_delayed_work(&dp->sec.poor_discon_work);
#endif

#ifdef SECDP_USE_WAKELOCK
	secdp_set_wakelock(dp, false);
#endif

	if (dp_display_is_hdcp_enabled(dp) &&
			status->hdcp_state != HDCP_STATE_INACTIVE) {
#ifndef CONFIG_SEC_DISPLAYPORT
		cancel_delayed_work_sync(&dp->hdcp_cb_work);
#else
		cancel_delayed_work(&dp->hdcp_cb_work);
		usleep_range(3000, 5000);
#endif
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);

		dp_display_update_hdcp_status(dp, true);
	}

#ifdef CONFIG_SEC_DISPLAYPORT
	if (!dp->power_on) {
		pr_info("DP is already off, skip\n");

		if (!dp->active_stream_cnt)
			dp->ctrl->off(dp->ctrl);

		dp->panel->video_test = false;
		dp_display_update_mst_state(dp, false);
		mutex_unlock(&dp->session_lock);
		return rc;
	}
#endif

	dp_display_audio_enable(dp, false);

	mutex_unlock(&dp->session_lock);

	dp_display_process_mst_hpd_low(dp);

	rc = dp_display_send_hpd_notification(dp);

	mutex_lock(&dp->session_lock);
	if (!dp->active_stream_cnt)
		dp->ctrl->off(dp->ctrl);
	mutex_unlock(&dp->session_lock);

	dp->panel->video_test = false;

	return rc;
}

#ifdef CONFIG_SEC_DISPLAYPORT
void secdp_dex_do_reconnecting(void)
{
	struct dp_display_private *dp = g_secdp_priv;

	if (dp->link->poor_connection) {
		pr_info("poor connection, return!\n");
		return;
	}

	mutex_lock(&dp->attention_lock);
	pr_info("dex_reconnect hpd low++\n");
	dp->sec.dex.reconnecting = 1;

	if (dp->sec.dex.curr == DEX_ENABLED)
		dp->sec.dex.curr = DEX_DURING_MODE_CHANGE;

	dp->hpd->hpd_high = false;
	dp_display_host_init(dp);
	dp_display_process_hpd_low(dp);

	pr_info("dex_reconnect hpd low--\n");
	mutex_unlock(&dp->attention_lock);

	msleep(400);

	mutex_lock(&dp->attention_lock);
	if (!dp->power_on && dp->sec.dex.reconnecting) {
		pr_info("dex_reconnect hpd high++\n");

		dp->hpd->hpd_high = true;
		dp_display_host_init(dp);
		dp_display_process_hpd_high(dp);

		pr_info("dex_reconnect hpd high--\n");
	}
	dp->sec.dex.reconnecting = 0;
	mutex_unlock(&dp->attention_lock);
}

/** check if dex is running now */
bool secdp_check_dex_mode(void)
{
	struct dp_display_private *dp = g_secdp_priv;
	bool mode = false;

	if (dp->sec.dex.res == DEX_RES_NOT_SUPPORT)
		goto end;

	if (dp->sec.dex.setting_ui == DEX_DISABLED && dp->sec.dex.curr == DEX_DISABLED)
		goto end;

	mode = true;
end:
	return mode;
}

/** get dex resolution. it depends on which dongle/adapter is connected */
enum dex_support_res_t secdp_get_dex_res(void)
{
	struct dp_display_private *dp = g_secdp_priv;

	return dp->sec.dex.res;
}
#endif

#ifndef CONFIG_SEC_DISPLAYPORT
static int dp_display_usbpd_configure_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch) {
		rc = dp->aux->aux_switch(dp->aux, true, dp->hpd->orientation);
		if (rc)
			goto end;
	}

	mutex_lock(&dp->session_lock);
	dp_display_host_init(dp);

	/* check for hpd high */
	if (dp->hpd->hpd_high)
		queue_work(dp->wq, &dp->connect_work);
	else
		dp->process_hpd_connect = true;
	mutex_unlock(&dp->session_lock);
end:
	return rc;
}
#else
static int secdp_display_usbpd_configure_cb(void)
{
	int rc = 0;
	struct dp_display_private *dp = g_secdp_priv;

	pr_debug("+++\n");

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch) {
		rc = dp->aux->aux_switch(dp->aux, true, dp->hpd->orientation);
		if (rc)
			goto end;
	}

	mutex_lock(&dp->session_lock);
	dp_display_host_init(dp);

	/* check for hpd high */
	if (dp->hpd->hpd_high)
		queue_work(dp->wq, &dp->connect_work);
	else
		dp->process_hpd_connect = true;
	mutex_unlock(&dp->session_lock);
end:
	return rc;
}
#endif

static int dp_display_stream_pre_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp->ctrl->stream_pre_off(dp->ctrl, dp_panel);

	return 0;
}

static void dp_display_stream_disable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	if (!dp->active_stream_cnt) {
		pr_err("invalid active_stream_cnt (%d)\n",
				dp->active_stream_cnt);
		return;
	}

	pr_debug("stream_id=%d, active_stream_cnt=%d\n",
			dp_panel->stream_id, dp->active_stream_cnt);

	dp->ctrl->stream_off(dp->ctrl, dp_panel);
	dp->active_panels[dp_panel->stream_id] = NULL;
	dp->active_stream_cnt--;
}

static void dp_display_clean(struct dp_display_private *dp)
{
	int idx;
	struct dp_panel *dp_panel;
	struct dp_link_hdcp_status *status = &dp->link->hdcp_status;

	pr_debug("+++\n");

#ifdef CONFIG_SEC_DISPLAYPORT
	cancel_delayed_work_sync(&dp->sec.hdcp_start_work);
	cancel_delayed_work(&dp->sec.link_status_work);
	cancel_delayed_work(&dp->sec.poor_discon_work);
#endif

	if (dp_display_is_hdcp_enabled(dp) &&
			status->hdcp_state != HDCP_STATE_INACTIVE) {
		cancel_delayed_work_sync(&dp->hdcp_cb_work);
		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);

		dp_display_update_hdcp_status(dp, true);
	}

	for (idx = DP_STREAM_0; idx < DP_STREAM_MAX; idx++) {
		if (!dp->active_panels[idx])
			continue;

		dp_panel = dp->active_panels[idx];

		dp_display_stream_pre_disable(dp, dp_panel);
		dp_display_stream_disable(dp, dp_panel);
		dp_panel->deinit(dp_panel, 0);
	}

	dp->power_on = false;

	dp->ctrl->off(dp->ctrl);
}

static int dp_display_handle_disconnect(struct dp_display_private *dp)
{
	int rc;
	pr_debug("+++\n");

	rc = dp_display_process_hpd_low(dp);
	if (rc) {
		/* cancel any pending request */
		dp->ctrl->abort(dp->ctrl);
		dp->aux->abort(dp->aux);
	}

	mutex_lock(&dp->session_lock);
	if (rc && dp->power_on)
		dp_display_clean(dp);

	dp_display_host_deinit(dp);

	mutex_unlock(&dp->session_lock);

	return rc;
}

static void dp_display_disconnect_sync(struct dp_display_private *dp)
{
	pr_debug("+++\n");

#ifdef CONFIG_SEC_DISPLAYPORT
	if (dp->link->poor_connection) {
		secdp_send_poor_connection_event();
		dp->link->status_update_cnt = 0;
		dp->sec.hdcp_retry = 0;
	}
#endif

	/* cancel any pending request */
	atomic_set(&dp->aborted, 1);
	dp->ctrl->abort(dp->ctrl);
	dp->aux->abort(dp->aux);

	/* wait for idle state */
	cancel_work(&dp->connect_work);
	cancel_work(&dp->attention_work);
	flush_workqueue(dp->wq);

	dp_display_handle_disconnect(dp);

	/* Reset abort value to allow future connections */
	atomic_set(&dp->aborted, 0);
}

#ifndef CONFIG_SEC_DISPLAYPORT
static int dp_display_usbpd_disconnect_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	mutex_lock(&dp->session_lock);
	if (dp->debug->psm_enabled && dp->core_initialized)
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);
	mutex_unlock(&dp->session_lock);

	dp_display_disconnect_sync(dp);

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch)
		dp->aux->aux_switch(dp->aux, false, ORIENTATION_NONE);
end:
	return rc;
}
#else
static int secdp_display_usbpd_disconnect_cb(void)
{
	int rc = 0;
	struct dp_display_private *dp = g_secdp_priv;

	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	pr_debug("+++, psm(%d)\n", dp->debug->psm_enabled);

	if (atomic_read(&dp->notification_status)) {
		reinit_completion(&dp->notification_comp);

		pr_info("wait for connection logic++\n");
		if (atomic_read(&dp->notification_status) &&
			!wait_for_completion_timeout(&dp->notification_comp, HZ * 5)) {
			pr_warn("notification_comp timeout\n");
		}
		pr_info("wait for connection logic--\n");

		atomic_set(&dp->notification_status, 0);
	}

	mutex_lock(&dp->session_lock);
	if (dp->debug->psm_enabled && dp->core_initialized)
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);
	mutex_unlock(&dp->session_lock);

	/* cancel any pending request */
	atomic_set(&dp->aborted, 1);

	dp_display_disconnect_sync(dp);

	if (!dp->debug->sim_mode && !dp->parser->no_aux_switch
	    && !dp->parser->gpio_aux_switch)
		dp->aux->aux_switch(dp->aux, false, ORIENTATION_NONE);

	/* needed here because it's set at "dp_display_disconnect_sync()" above */
	atomic_set(&dp->notification_status, 0);
	complete(&dp->dp_off_comp);
end:
	pr_debug("---\n");
	return rc;
}
#endif

static int dp_display_stream_enable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	int rc = 0;

	rc = dp->ctrl->stream_on(dp->ctrl, dp_panel);

	if (dp->debug->tpg_state)
		dp_panel->tpg_config(dp_panel, true);

	if (!rc) {
		dp->active_panels[dp_panel->stream_id] = dp_panel;
		dp->active_stream_cnt++;
	}

	pr_debug("dp active_stream_cnt:%d\n", dp->active_stream_cnt);

	return rc;
}

static void dp_display_mst_attention(struct dp_display_private *dp)
{
	struct dp_mst_hpd_info hpd_irq = {0};

	if (dp->mst.mst_active && dp->mst.cbs.hpd_irq) {
		hpd_irq.mst_hpd_sim = dp->debug->mst_hpd_sim;
		dp->mst.cbs.hpd_irq(&dp->dp_display, &hpd_irq);
		dp->debug->mst_hpd_sim = false;
	}

	DP_MST_DEBUG("mst_attention_work. mst_active:%d\n", dp->mst.mst_active);
}

static void dp_display_attention_work(struct work_struct *work)
{
	struct dp_display_private *dp = container_of(work,
			struct dp_display_private, attention_work);

#ifdef CONFIG_SEC_DISPLAYPORT
	if (!secdp_get_hpd_status() || !secdp_get_cable_status()) {
		pr_info("hpd_low or cable_lost\n");
		return;
	}
#endif

	pr_debug("+++, sink_request: 0x%08x\n", dp->link->sink_request);

	mutex_lock(&dp->session_lock);

	if (dp->debug->mst_hpd_sim || !dp->core_initialized) {
		mutex_unlock(&dp->session_lock);
		goto mst_attention;
	}

	if (dp->link->process_request(dp->link)) {
		mutex_unlock(&dp->session_lock);
		goto cp_irq;
	}

	mutex_unlock(&dp->session_lock);

	if (dp->link->sink_request & DS_PORT_STATUS_CHANGED) {
		if (dp_display_is_sink_count_zero(dp)) {
			dp_display_handle_disconnect(dp);
		} else {
			if (!dp->mst.mst_active)
				queue_work(dp->wq, &dp->connect_work);
		}

#ifdef CONFIG_SEC_DISPLAYPORT
		/* add some delay to guarantee hpd event handling in framework side */
		msleep(60);
#endif

		goto mst_attention;
	}

	if (dp->link->sink_request & DP_TEST_LINK_VIDEO_PATTERN) {
		dp_display_handle_disconnect(dp);

		dp->panel->video_test = true;
		queue_work(dp->wq, &dp->connect_work);

		goto mst_attention;
	}

	if ((dp->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) ||
			(dp->link->sink_request & DP_TEST_LINK_TRAINING) ||
			(dp->link->sink_request & DP_LINK_STATUS_UPDATED)) {
		mutex_lock(&dp->session_lock);
		dp_display_audio_enable(dp, false);
		mutex_unlock(&dp->session_lock);

		if (dp->link->sink_request & DP_TEST_LINK_PHY_TEST_PATTERN) {
			dp->ctrl->process_phy_test_request(dp->ctrl);
		} else if (dp->link->sink_request & DP_TEST_LINK_TRAINING) {
			dp->link->send_test_response(dp->link);
			dp->ctrl->link_maintenance(dp->ctrl);
		} else if (dp->link->sink_request & DP_LINK_STATUS_UPDATED) {
			dp->ctrl->link_maintenance(dp->ctrl);
		}

		mutex_lock(&dp->session_lock);
		dp_display_audio_enable(dp, true);
		mutex_unlock(&dp->session_lock);
		goto mst_attention;
	}
cp_irq:
	if (dp_display_is_hdcp_enabled(dp) && dp->hdcp.ops->cp_irq)
		dp->hdcp.ops->cp_irq(dp->hdcp.data);
mst_attention:
	dp_display_mst_attention(dp);

#ifdef CONFIG_SEC_DISPLAYPORT
	if (dp->link->status_update_cnt > 9 && !dp->link->poor_connection) {
		dp->link->poor_connection = true; 
		dp->sec.dex.prev = dp->sec.dex.curr = DEX_DISABLED;
		schedule_delayed_work(&dp->sec.link_status_work,
							msecs_to_jiffies(10));
	}
#endif
}

#ifndef CONFIG_SEC_DISPLAYPORT
static int dp_display_usbpd_attention_cb(struct device *dev)
{
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		return -EINVAL;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		return -ENODEV;
	}

	pr_debug("hpd_irq:%d, hpd_high:%d, power_on:%d, is_connected:%d\n",
			dp->hpd->hpd_irq, dp->hpd->hpd_high,
			dp->power_on, dp->is_connected);

	if (!dp->hpd->hpd_high)
		dp_display_disconnect_sync(dp);
	else if ((dp->hpd->hpd_irq && dp->core_initialized) ||
			dp->debug->mst_hpd_sim)
		queue_work(dp->wq, &dp->attention_work);
	else if (dp->process_hpd_connect || !dp->is_connected)
		queue_work(dp->wq, &dp->connect_work);
	else
		pr_debug("ignored\n");

	return 0;
}
#else
static int secdp_event_thread(void *data)
{
	unsigned long flag;
	u32 todo = 0;

	struct secdp_event_data *ev_data;
	struct secdp_event *ev;
	struct dp_display_private *dp = NULL;

	if (!data)
		return -EINVAL;

	ev_data = (struct secdp_event_data *)data;
	init_waitqueue_head(&ev_data->event_q);
	spin_lock_init(&ev_data->event_lock);

	while (!kthread_should_stop()) {
		wait_event(ev_data->event_q,
			(ev_data->pndx != ev_data->gndx) ||
			kthread_should_stop());
		spin_lock_irqsave(&ev_data->event_lock, flag);
		ev = &(ev_data->event_list[ev_data->gndx++]);
		todo = ev->id;
		dp = ev->dp;
		ev->id = 0;
		ev_data->gndx %= SECDP_EVENT_Q_MAX;
		spin_unlock_irqrestore(&ev_data->event_lock, flag);

		pr_debug("todo=%s\n", secdp_ev_event_to_string(todo));

		switch (todo) {
		case EV_USBPD_ATTENTION:
			secdp_handle_attention(dp);
			break;
		default:
			pr_err("Unknown event:%d\n", todo);
		}
	}

	return 0;
}

static int secdp_event_setup(struct dp_display_private *dp)
{

	dp->ev_thread = kthread_run(secdp_event_thread,
		(void *)&dp->dp_event, "secdp_event");
	if (IS_ERR(dp->ev_thread)) {
		pr_err("unable to start event thread\n");
		return PTR_ERR(dp->ev_thread);
	}

	dp->workq = create_workqueue("secdp_hpd");
	if (!dp->workq) {
		pr_err("error creating workqueue\n");
		return -EPERM;
	}

	INIT_LIST_HEAD(&dp->attention_head);
	return 0;
}

static void secdp_event_cleanup(struct dp_display_private *dp)
{
	destroy_workqueue(dp->workq);

	if (dp->ev_thread == current)
		return;

	kthread_stop(dp->ev_thread);
}

static void secdp_send_events(struct dp_display_private *dp, u32 event)
{
	struct secdp_event *ev;
	struct secdp_event_data *ev_data = &dp->dp_event;

	pr_debug("event=%s\n", secdp_ev_event_to_string(event));

	spin_lock(&ev_data->event_lock);
	ev = &ev_data->event_list[ev_data->pndx++];
	ev->id = event;
	ev->dp = dp;
	ev_data->pndx %= SECDP_EVENT_Q_MAX;
	wake_up(&ev_data->event_q);
	spin_unlock(&ev_data->event_lock);
}

static void secdp_process_attention(struct dp_display_private *dp,
		CC_NOTI_TYPEDEF *noti)
{
	int rc = 0;

	if (!noti || !dp)
		goto end;

	pr_debug("sub1(%d), sub2(%d), sub3(%d)\n", noti->sub1, noti->sub2, noti->sub3);

	dp->sec.dex.reconnecting = 0;

	if (noti->id == CCIC_NOTIFY_ID_DP_CONNECT && noti->sub1 == CCIC_NOTIFY_DETACH) {
		dp->hpd->hpd_high = false;

		rc = secdp_display_usbpd_disconnect_cb();
		goto end;
	}

	if (noti->id == CCIC_NOTIFY_ID_DP_HPD &&
		noti->sub1 == CCIC_NOTIFY_HIGH && noti->sub2 == CCIC_NOTIFY_IRQ) {
		if (secdp_get_reboot_status()) {
			pr_info("shutdown\n");
			goto end;
		}

		if (!secdp_get_cable_status()) {
			pr_info("cable is out\n");
			goto end;
		}

		if (dp->link->poor_connection) {
			pr_info("poor connection\n");
			goto end;
		}

#ifndef SECDP_USE_MST
		if (!dp->power_on) {
			flush_workqueue(dp->wq);
			if (!dp->power_on) {
				pr_debug("handle it as hpd high\n");

				if (dp->link->poor_connection) {
					pr_info("poor connection\n");
					goto end;
				}

				if (dp->is_connected) {
					pr_info("platform reset!\n");
					secdp_display_usbpd_disconnect_cb();
					msleep(100);
				} else {
					/* aux timeout happens whenever DeX reconnecting scenario.
					 * init aux here */
					dp_display_host_deinit(dp);
					usleep_range(5000, 6000);
				}

				goto handle_hpd_high;
			}
		}
#endif

		/* irq_hpd: do the same with: dp_display_usbpd_attention_cb */
		queue_work(dp->wq, &dp->attention_work);

		goto end;
	}

	if (noti->id == CCIC_NOTIFY_ID_DP_HPD && noti->sub1 == CCIC_NOTIFY_LOW) {
		dp->sec.dex.prev = dp->sec.dex.curr = DEX_DISABLED;
		secdp_clear_link_status_update_cnt(dp->link);

		dp_display_disconnect_sync(dp);
		goto end;
	}

#ifndef SECDP_USE_MST
handle_hpd_high:
#endif
	/* hpd high: do the same with: dp_display_usbpd_attention_cb */
	pr_info("power_on <%d>\n", dp->power_on);
	if (!dp->power_on) {
		secdp_clear_link_status_update_cnt(dp->link);
		queue_work(dp->wq, &dp->connect_work);
	}

end:
	return;
}

static void secdp_handle_attention(struct dp_display_private *dp)
{
	int i = 0;

	while (!list_empty_careful(&dp->attention_head)) {
		struct secdp_attention_node *node;

		pr_debug("+++ processing item %d in the list +++\n", ++i);

		mutex_lock(&dp->attention_lock);
		node = list_first_entry(&dp->attention_head,
				struct secdp_attention_node, list);

		secdp_process_attention(dp, &node->noti);

#ifdef CONFIG_SEC_DISPLAYPORT
		/* add some delay to guarantee hpd event handling in framework side */
		msleep(60);
#endif

		list_del(&node->list);
		mutex_unlock(&dp->attention_lock);

		kzfree(node);

		pr_debug("--- processing item %d in the list ---\n", i);
	};

}

#ifdef SECDP_SELF_TEST
static void secdp_hdcp_start_work(struct work_struct *work);

void secdp_self_test_edid_clear(void)
{
	struct dp_display_private *dp = g_secdp_priv;
	dp->panel->set_edid(dp->panel, NULL); 
}

void secdp_self_test_hdcp_on(void)
{
	pr_debug("+++\n");
	secdp_hdcp_start_work(NULL);
}

void secdp_self_test_hdcp_off(void)
{
	struct dp_display_private *dp = g_secdp_priv;
	struct dp_link_hdcp_status *status;

	pr_debug("+++\n");

	if (secdp_get_cable_status() && dp->power_on) {
		status = &dp->link->hdcp_status;

		if (dp_display_is_hdcp_enabled(dp) &&
				status->hdcp_state != HDCP_STATE_INACTIVE) {
			cancel_delayed_work(&dp->hdcp_cb_work);
			usleep_range(3000, 5000);
			if (dp->hdcp.ops->off)
				dp->hdcp.ops->off(dp->hdcp.data);

			dp_display_update_hdcp_status(dp, true);
		}

	}
}	
#endif

static int secdp_ccic_noti_cb(struct notifier_block *nb, unsigned long action,
		void *data)
{
	struct dp_display_private *dp = g_secdp_priv;
	CC_NOTI_TYPEDEF noti = *(CC_NOTI_TYPEDEF *)data;
	struct secdp_attention_node *node;
	int rc = 0;

	if (noti.dest != CCIC_NOTIFY_DEV_DP) {
		/*pr_debug("not DP, skip\n");*/
		goto end;
	}

	switch (noti.id) {
	case CCIC_NOTIFY_ID_ATTACH:
		pr_info("CCIC_NOTIFY_ID_ATTACH\n");
		break;

	case CCIC_NOTIFY_ID_DP_CONNECT:
		pr_info("CCIC_NOTIFY_ID_DP_CONNECT, <%d>\n", noti.sub1);

		switch (noti.sub1) {
		case CCIC_NOTIFY_ATTACH:
			dp->sec.cable_connected = true;
			dp->sec.hdcp_retry = 0;
			dp->hpd->alt_mode_cfg_done = true;
			dp->hpd->multi_func = false;
			secdp_clear_link_status_update_cnt(dp->link);
			dp->hpd->orientation = secdp_get_plug_orientation();
			dp->sec.dex.res =
				secdp_check_adapter_type(noti.sub2, noti.sub3);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_connection();
			secdp_bigdata_save_item(BD_ORIENTATION,
				dp->hpd->orientation == ORIENTATION_CC1 ? "CC1" : "CC2");
			secdp_bigdata_save_item(BD_ADT_VID, noti.sub2);
			secdp_bigdata_save_item(BD_ADT_PID, noti.sub3);
#endif
			secdp_logger_set_max_count(300);

#ifndef SECDP_USB_CONCURRENCY
			/* see dp_display_usbpd_configure_cb() */
			dp_display_host_init(dp);
#endif
#ifdef SECDP_SELF_TEST
			if (secdp_self_test_status(ST_CONNECTION_TEST) >= 0)
				secdp_self_test_start_reconnect(secdp_dex_do_reconnecting);

			secdp_self_register_clear_func(ST_EDID, secdp_self_test_edid_clear);
 
			if (secdp_self_test_status(ST_EDID) >= 0)
				dp->panel->set_edid(dp->panel, secdp_self_test_get_edid()); 
#endif

			break;

		case CCIC_NOTIFY_DETACH:
			if (!secdp_get_cable_status()) {
				pr_info("already disconnected\n");
				goto end;
			}
			dp->is_dp_disconnecting = 1;
			secdp_logger_set_max_count(300);
#ifdef CONFIG_COMBO_REDRIVER_PTN36502
			secdp_redriver_onoff(false, 0);
#endif
			/* set flags here as soon as disconnected
			 * resource clear will be made later at "secdp_process_attention"
			 */
			dp->sec.dex.prev = dp->sec.dex.curr = DEX_DISABLED;
			dp->sec.dex.res = DEX_RES_NOT_SUPPORT;
			dp->sec.dex.reconnecting = 0;
			dp->sec.cable_connected = false;
			dp->sec.hdcp_retry = 0;
			dp->sec.link_conf = false;
			atomic_set(&dp->sec.hpd, 0);
			dp->hpd->orientation = ORIENTATION_NONE;
			dp->hpd->multi_func = false;
			secdp_clear_link_status_update_cnt(dp->link);
			dp->hpd->alt_mode_cfg_done = false;

			secdp_clear_branch_info(dp);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_disconnection();
#endif
			break;

		default:
			break;
		}
		break;

	case CCIC_NOTIFY_ID_DP_LINK_CONF:
		pr_info("CCIC_NOTIFY_ID_DP_LINK_CONF, <%c>\n",
			noti.sub1 + 'A' - 1);
		if (!secdp_get_cable_status()) {
			pr_info("cable is out\n");
			goto end;
		}

#ifdef SECDP_USB_CONCURRENCY
		if (noti.sub1 == CCIC_NOTIFY_DP_PIN_B ||
			noti.sub1 == CCIC_NOTIFY_DP_PIN_D ||
			noti.sub1 == CCIC_NOTIFY_DP_PIN_F) {
			dp->hpd->multi_func = true;
#if defined(CONFIG_COMBO_REDRIVER_PTN36502)
			secdp_redriver_onoff(true, 2);
#endif
		} else {
			dp->hpd->multi_func = false;
#if defined(CONFIG_COMBO_REDRIVER_PTN36502)
			secdp_redriver_onoff(true, 4);
#endif
		}

		pr_info("multi_func: <%d>\n", dp->hpd->multi_func);

		/* see dp_display_usbpd_configure_cb() */

		/* host_init is commented out to fix phy cts failure.
		 * it's called at hpd_high function.
		 */
		/* dp_display_host_init(dp); */
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_LINK_CONFIGURE, noti.sub1 + 'A' - 1);
#endif
		dp->sec.link_conf = true;
		break;

	case CCIC_NOTIFY_ID_DP_HPD:
		pr_info("CCIC_NOTIFY_ID_DP_HPD, sub1 <%s>, sub2<%s>\n",
			(noti.sub1 == CCIC_NOTIFY_HIGH) ? "high" :
				((noti.sub1 == CCIC_NOTIFY_LOW) ? "low" : "??"),
			(noti.sub2 == CCIC_NOTIFY_IRQ) ? "irq" : "??");
		if (!secdp_get_cable_status()) {
			pr_info("cable is out\n");
			goto end;
		}

		if (noti.sub1 == CCIC_NOTIFY_HIGH) {
			secdp_logger_set_max_count(300);
			atomic_set(&dp->sec.hpd, 1);
			dp->hpd->hpd_high = true;
		} else /* if (noti.sub1 == CCIC_NOTIFY_LOW) */ {
			atomic_set(&dp->sec.hpd, 0);
			dp->hpd->hpd_high = false;
		}

		break;

	default:
		break;
	}

	pr_debug("sec.link_conf(%d), sec.hpd(%d)\n", dp->sec.link_conf, atomic_read(&dp->sec.hpd));
	if ((dp->sec.link_conf && atomic_read(&dp->sec.hpd)) || /*hpd high? or hpd_irq?*/
		(noti.id == CCIC_NOTIFY_ID_DP_HPD && noti.sub1 == CCIC_NOTIFY_LOW) || /*hpd low?*/
		(noti.id == CCIC_NOTIFY_ID_DP_CONNECT && noti.sub1 == CCIC_NOTIFY_DETACH)) { /*cable disconnect?*/

		node = kzalloc(sizeof(*node), GFP_KERNEL);

		node->noti.src  = noti.src;
		node->noti.dest = noti.dest;
		node->noti.id   = noti.id;
		node->noti.sub1 = noti.sub1;
		node->noti.sub2 = noti.sub2;
		node->noti.sub3 = noti.sub3;

		mutex_lock(&dp->attention_lock);
		list_add_tail(&node->list, &dp->attention_head);
		mutex_unlock(&dp->attention_lock);

		secdp_send_events(dp, EV_USBPD_ATTENTION);

		if (noti.id == CCIC_NOTIFY_ID_DP_CONNECT && noti.sub1 == CCIC_NOTIFY_DETACH) {
			if (dp->power_on == true || atomic_read(&dp->notification_status)) {
				int ret;

				pr_debug("wait for detach complete\n");

				init_completion(&dp->dp_off_comp);
				ret = wait_for_completion_timeout(&dp->dp_off_comp, msecs_to_jiffies(14000));
				if (ret <= 0)
					pr_err("dp_off_comp timedout\n");
				else
					pr_debug("detach complete!\n");

				atomic_set(&dp->notification_status, 0);
			}
			dp->is_dp_disconnecting = 0;
			complete(&dp->dp_disconnect_comp);
		}
	}

end:
	return rc;
}

int secdp_wait_for_disconnect_complete(void)
{
	struct dp_display_private *dp = g_secdp_priv;
	int ret = 0;

	if (!dp) {
		pr_info("dp driver is not initialized completely");
		return -1;
	}

	pr_info("+++\n");

	if (dp->is_dp_disconnecting) {
		init_completion(&dp->dp_disconnect_comp);
		ret = wait_for_completion_timeout(&dp->dp_disconnect_comp, msecs_to_jiffies(17000));

		if (ret > 0) {
			pr_debug("dp_disconnect_comp complete!\n");
			ret = 0;
		} else {
			pr_err("dp_disconnect_comp timeout\n");
			ret = -1;
		}
	}

	pr_info("ret %d\n", ret);
	return ret;
} 

int secdp_ccic_noti_register_ex(struct secdp_misc *sec, bool retry)
{
	int rc;

	sec->ccic_noti_registered = true;
	rc = manager_notifier_register(&sec->ccic_noti_block,
				secdp_ccic_noti_cb, MANAGER_NOTIFY_CCIC_DP);
	if (!rc) {
		pr_debug("success\n");
		goto exit;
	}

	sec->ccic_noti_registered = false;
	if (!retry) {
		pr_debug("no more try\n");
		goto exit;
	}

	pr_err("manager_dev is not ready yet, rc(%d)\n", rc);
	pr_err("try again in %d[ms]\n", CCIC_DP_NOTI_REG_DELAY);

	schedule_delayed_work(&sec->ccic_noti_reg_work,
			msecs_to_jiffies(CCIC_DP_NOTI_REG_DELAY));

exit:
	return rc;
}

static void secdp_ccic_noti_register(struct work_struct *work)
{
	int rc;
	struct dp_display_private *dp = g_secdp_priv;
	struct secdp_misc *sec = &dp->sec;

	mutex_lock(&sec->notifier_lock);

	if (secdp_check_if_lpm_mode()) {
		pr_info("it's LPM mode. skip\n");
		goto exit;
	}

	if (sec->ccic_noti_registered) {
		pr_info("already registered\n");
		goto exit;
	}

	rc = secdp_ccic_noti_register_ex(sec, true);
	if (rc) {
		pr_err("fail, rc(%d)\n", rc);
		goto exit;
	}

	pr_debug("success\n");
	sec->ccic_noti_registered = true;

	/* cancel immediately */
	rc = cancel_delayed_work(&sec->ccic_noti_reg_work);
	pr_debug("cancel_work, rc(%d)\n", rc);
	destroy_delayed_work_on_stack(&sec->ccic_noti_reg_work);

exit:
	mutex_unlock(&sec->notifier_lock);
	return;
}

static void secdp_hdcp_start_work(struct work_struct *work)
{
	struct dp_display_private *dp = g_secdp_priv;

	pr_debug("+++\n");

	if (secdp_get_cable_status() && dp->power_on) {
		dp_display_check_source_hdcp_caps(dp);
		dp_display_update_hdcp_info(dp);

		if (dp_display_is_hdcp_enabled(dp)) {
			cancel_delayed_work_sync(&dp->hdcp_cb_work);
			if (dp->hdcp.ops && dp->hdcp.ops->on &&
					dp->hdcp.ops->on(dp->hdcp.data)) {
				dp_display_update_hdcp_status(dp, true);
				return;
			}
			dp->link->hdcp_status.hdcp_state = HDCP_STATE_AUTHENTICATING;
			queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ / 2);
		}
	}
}

static void secdp_poor_disconnect_work(struct work_struct *work)
{
	struct dp_display_private *dp = g_secdp_priv;

	pr_debug("+++, poor_connection: %d\n", dp->link->poor_connection);

	if (!dp->link->poor_connection)
		dp->link->poor_connection = true;

	dp_display_disconnect_sync(dp);
}

/* This logic is to check poor DP connection. if link train is failed or
 * irq hpd is coming more than 4th times in 13 sec, regard it as a poor connection
 * and do disconnection of displayport
 */
static void secdp_link_status_work(struct work_struct *work)
{
	struct dp_display_private *dp = g_secdp_priv;

	pr_info("+++ status_update_cnt %d\n", dp->link->status_update_cnt);

	if (dp->link->poor_connection) {
		pr_info("poor_connection!\n");
		goto poor_disconnect;
	}

	if (secdp_get_cable_status() && dp->power_on && dp->sec.dex.curr) {

		if (!dp->ctrl->get_link_train_status(dp->ctrl) ||
				dp->link->status_update_cnt > MAX_CNT_LINK_STATUS_UPDATE) {
			pr_info("poor!\n");
			goto poor_disconnect;
		}

		if (!secdp_check_link_stable(dp->link)) {
			pr_info("Check poor connection, again\n");
			schedule_delayed_work(&dp->sec.link_status_work, msecs_to_jiffies(3000));
		}
	}

	pr_info("---\n");
	return;

poor_disconnect:
	schedule_delayed_work(&dp->sec.poor_discon_work, msecs_to_jiffies(10));
	return;
}

static int secdp_init(struct dp_display_private *dp)
{
	int rc = -1;

	if (!dp) {
		pr_err("error! no dp structure\n");
		goto end;
	}

	INIT_DELAYED_WORK(&dp->sec.hdcp_start_work, secdp_hdcp_start_work);
	INIT_DELAYED_WORK(&dp->sec.link_status_work, secdp_link_status_work);
	INIT_DELAYED_WORK(&dp->sec.poor_discon_work, secdp_poor_disconnect_work);

	INIT_DELAYED_WORK(&dp->sec.ccic_noti_reg_work, secdp_ccic_noti_register);
	if (!secdp_check_if_lpm_mode()) {
		schedule_delayed_work(&dp->sec.ccic_noti_reg_work,
				msecs_to_jiffies(CCIC_DP_NOTI_REG_DELAY));
	}

	rc = secdp_power_request_gpios(dp->power);
	if (rc)
		pr_err("DRM DP gpio request failed\n");

	dp->sysfs = secdp_sysfs_init();
	if (!dp->sysfs)
		pr_err("secdp_sysfs_init failed\n");

	mutex_init(&dp->attention_lock);
	mutex_init(&dp->sec.notifier_lock);
#ifdef SECDP_USE_WAKELOCK
	secdp_init_wakelock(dp);
#endif

	rc = secdp_event_setup(dp);
	if (rc)
		pr_err("secdp_event_setup failed\n");

end:
	pr_info("exit, rc(%d)\n", rc);
	return rc;
}

static void secdp_deinit(struct dp_display_private *dp)
{
	if (!dp) {
		pr_err("error! no dp structure\n");
		goto end;
	}

#ifdef SECDP_USE_WAKELOCK
	secdp_destroy_wakelock(dp);
#endif
	secdp_sysfs_deinit(dp->sysfs);
	dp->sysfs = NULL;

	secdp_event_cleanup(dp);

end:
	return;
}

struct dp_panel *secdp_get_panel_info(void)
{
	struct dp_display_private *dp = g_secdp_priv;
	struct dp_panel *panel = NULL;

	if (dp)
		panel = dp->panel;

	return panel;
}

struct drm_connector *secdp_get_connector(void)
{
	struct dp_display *dp_disp = g_dp_display;
	struct drm_connector *connector = NULL;

	if (dp_disp)
		connector = dp_disp->base_connector;

	return connector;
}

static int secdp_reboot_cb(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct dp_display_private *dp = container_of(nb,
			struct dp_display_private, reboot_nb);

	if (IS_ERR_OR_NULL(dp)) {
		pr_err("dp is null!\n");
		goto end;
	}

	if (!secdp_get_cable_status()) {
		pr_debug("cable is out\n");
		goto end;
	}

	pr_debug("+++, reboot:%d\n", dp->sec.reboot);

	dp->sec.reboot = true;
#ifndef SECDP_TEST_HDCP2P2_REAUTH
	msleep(300);
#endif

end:
	return NOTIFY_OK;
}

bool secdp_get_reboot_status(void)
{
	struct dp_display_private *dp;
	bool ret = false;

	if (!g_dp_display) {
		pr_debug("dp display not initialized\n");
		goto end;
	}

	dp = container_of(g_dp_display, struct dp_display_private, dp_display);
	if (IS_ERR_OR_NULL(dp)) {
		pr_err("dp is null!\n");
		goto end;
	}

	pr_debug("+++, reboot:%d\n", dp->sec.reboot);

	ret = dp->sec.reboot;
#ifdef SECDP_TEST_HDCP2P2_REAUTH
	ret = false;
	pr_debug("[SECDP_TEST_HDCP2P2_REAUTH]\n");
#endif

end:
	return ret;
}
#endif

static void dp_display_connect_work(struct work_struct *work)
{
	int rc = 0;
	struct dp_display_private *dp = container_of(work,
			struct dp_display_private, connect_work);

	if (atomic_read(&dp->aborted)) {
		pr_warn("HPD off requested\n");
		return;
	}

	if (!dp->hpd->hpd_high) {
		pr_warn("Sink disconnected\n");
		return;
	}

	pr_debug("+++\n");

#ifdef CONFIG_SEC_DISPLAYPORT
	dp_display_host_init(dp);

	/* fix for PHY CTS v1.2 - 8.1 AUX Manchester - Channel EYE Test failure.
	 * whenever HPD goes high, AUX init makes RC delay and actual AUX transfer
	 * starts even RC is not yet stabilized.
	 * To make RC waveform be stable, put some delay */
	msleep(200);
#endif

	rc = dp_display_process_hpd_high(dp);

	if (!rc && dp->panel->video_test)
		dp->link->send_test_response(dp->link);
}

static int dp_display_usb_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct extcon_dev *edev = ptr;
	struct dp_display_private *dp = container_of(nb,
			struct dp_display_private, usb_nb);
	if (!edev)
		goto end;

	pr_debug("+++\n");

	if (!event && dp->debug->sim_mode) {
		dp_display_disconnect_sync(dp);
		dp->debug->abort(dp->debug);
	}
end:
	return NOTIFY_DONE;
}

static int dp_display_get_usb_extcon(struct dp_display_private *dp)
{
	struct extcon_dev *edev;
	int rc;

	pr_debug("+++\n");

	edev = extcon_get_edev_by_phandle(&dp->pdev->dev, 0);
	if (IS_ERR(edev))
		return PTR_ERR(edev);

	dp->usb_nb.notifier_call = dp_display_usb_notifier;
	dp->usb_nb.priority = 2;
	rc = extcon_register_notifier(edev, EXTCON_USB, &dp->usb_nb);
	if (rc)
		pr_err("failed to register for usb event: %d\n", rc);

	return rc;
}

static void dp_display_deinit_sub_modules(struct dp_display_private *dp)
{
#ifdef CONFIG_SEC_DISPLAYPORT
	secdp_deinit(dp);
	secdp_sysfs_put(dp->sysfs);
#endif
	dp_audio_put(dp->panel->audio);
	dp_ctrl_put(dp->ctrl);
	dp_link_put(dp->link);
	dp_panel_put(dp->panel);
	dp_aux_put(dp->aux);
	dp_power_put(dp->power);
	dp_catalog_put(dp->catalog);
	dp_parser_put(dp->parser);
	dp_hpd_put(dp->hpd);
	mutex_destroy(&dp->session_lock);
	dp_debug_put(dp->debug);
}

static int dp_init_sub_modules(struct dp_display_private *dp)
{
	int rc = 0;
	bool hdcp_disabled;
	struct device *dev = &dp->pdev->dev;
	struct dp_hpd_cb *cb = &dp->hpd_cb;
	struct dp_ctrl_in ctrl_in = {
		.dev = dev,
	};
	struct dp_panel_in panel_in = {
		.dev = dev,
	};
	struct dp_debug_in debug_in = {
		.dev = dev,
	};

	mutex_init(&dp->session_lock);

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		pr_err("failed to initialize parser, rc = %d\n", rc);
		dp->parser = NULL;
		goto error;
	}

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		pr_err("device tree parsing failed\n");
		goto error_catalog;
	}

	g_dp_display->is_mst_supported = dp->parser->has_mst;

	dp->catalog = dp_catalog_get(dev, dp->parser);
	if (IS_ERR(dp->catalog)) {
		rc = PTR_ERR(dp->catalog);
		pr_err("failed to initialize catalog, rc = %d\n", rc);
		dp->catalog = NULL;
		goto error_catalog;
	}

	dp->power = dp_power_get(dp->parser);
	if (IS_ERR(dp->power)) {
		rc = PTR_ERR(dp->power);
		pr_err("failed to initialize power, rc = %d\n", rc);
		dp->power = NULL;
		goto error_power;
	}

	rc = dp->power->power_client_init(dp->power, &dp->priv->phandle);
	if (rc) {
		pr_err("Power client create failed\n");
		goto error_aux;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux, dp->parser,
			dp->aux_switch_node);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		pr_err("failed to initialize aux, rc = %d\n", rc);
		dp->aux = NULL;
		goto error_aux;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		pr_err("DRM DP AUX register failed\n");
		goto error_link;
	}

	dp->link = dp_link_get(dev, dp->aux);
	if (IS_ERR(dp->link)) {
		rc = PTR_ERR(dp->link);
		pr_err("failed to initialize link, rc = %d\n", rc);
		dp->link = NULL;
		goto error_link;
	}

	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;
	panel_in.connector = dp->dp_display.base_connector;
	panel_in.base_panel = NULL;
	panel_in.parser = dp->parser;

	dp->panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp->panel)) {
		rc = PTR_ERR(dp->panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		dp->panel = NULL;
		goto error_panel;
	}

	ctrl_in.link = dp->link;
	ctrl_in.panel = dp->panel;
	ctrl_in.aux = dp->aux;
	ctrl_in.power = dp->power;
	ctrl_in.catalog = &dp->catalog->ctrl;
	ctrl_in.parser = dp->parser;

	dp->ctrl = dp_ctrl_get(&ctrl_in);
	if (IS_ERR(dp->ctrl)) {
		rc = PTR_ERR(dp->ctrl);
		pr_err("failed to initialize ctrl, rc = %d\n", rc);
		dp->ctrl = NULL;
		goto error_ctrl;
	}

	dp->panel->audio = dp_audio_get(dp->pdev, dp->panel,
						&dp->catalog->audio);
	if (IS_ERR(dp->panel->audio)) {
		rc = PTR_ERR(dp->panel->audio);
		pr_err("failed to initialize audio, rc = %d\n", rc);
		dp->panel->audio = NULL;
		goto error_audio;
	}

#ifdef CONFIG_SEC_DISPLAYPORT
	dp->sysfs = secdp_sysfs_get(dev, &dp->sec);
	if (IS_ERR(dp->sysfs)) {
		rc = PTR_ERR(dp->sysfs);
		pr_err("failed to initialize sysfs, rc = %d\n", rc);
		dp->sysfs = NULL;
		goto error_sysfs;
	}
	
	rc = secdp_init(dp);
	if (rc)
		pr_err("secdp_init failed\n");
#endif

	memset(&dp->mst, 0, sizeof(dp->mst));
	dp->active_stream_cnt = 0;

#ifndef CONFIG_SEC_DISPLAYPORT
	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;
#else
	cb->configure  = secdp_display_usbpd_configure_cb;
	cb->disconnect = secdp_display_usbpd_disconnect_cb;
#endif
	dp->hpd = dp_hpd_get(dev, dp->parser, &dp->catalog->hpd, cb);
	if (IS_ERR(dp->hpd)) {
		rc = PTR_ERR(dp->hpd);
		pr_err("failed to initialize hpd, rc = %d\n", rc);
		dp->hpd = NULL;
		goto error_hpd;
	}

	hdcp_disabled = !!dp_display_initialize_hdcp(dp);

	debug_in.panel = dp->panel;
	debug_in.hpd = dp->hpd;
	debug_in.link = dp->link;
	debug_in.aux = dp->aux;
	debug_in.connector = &dp->dp_display.base_connector;
	debug_in.catalog = dp->catalog;
	debug_in.parser = dp->parser;
	debug_in.ctrl = dp->ctrl;

	dp->debug = dp_debug_get(&debug_in);
	if (IS_ERR(dp->debug)) {
		rc = PTR_ERR(dp->debug);
		pr_err("failed to initialize debug, rc = %d\n", rc);
		dp->debug = NULL;
		goto error_debug;
	}

	dp->tot_dsc_blks_in_use = 0;

	dp->debug->hdcp_disabled = hdcp_disabled;
	dp_display_update_hdcp_status(dp, true);

	dp_display_get_usb_extcon(dp);

#ifndef CONFIG_SEC_DISPLAYPORT
	rc = dp->hpd->register_hpd(dp->hpd);
	if (rc) {
		pr_err("failed register hpd\n");
		goto error_hpd_reg;
	}
#endif

	return rc;
#ifndef CONFIG_SEC_DISPLAYPORT
error_hpd_reg:
	dp_debug_put(dp->debug);
#endif
error_debug:
	dp_hpd_put(dp->hpd);
error_hpd:
	dp_audio_put(dp->panel->audio);
error_audio:
	dp_ctrl_put(dp->ctrl);
error_ctrl:
	dp_panel_put(dp->panel);
error_panel:
	dp_link_put(dp->link);
#ifdef CONFIG_SEC_DISPLAYPORT
error_sysfs:
	secdp_sysfs_put(dp->sysfs);
#endif
error_link:
	dp_aux_put(dp->aux);
error_aux:
	dp_power_put(dp->power);
error_power:
	dp_catalog_put(dp->catalog);
error_catalog:
	dp_parser_put(dp->parser);
error:
	mutex_destroy(&dp->session_lock);
	return rc;
}

static int dp_display_post_init(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (IS_ERR_OR_NULL(dp)) {
		pr_err("invalid params\n");
		rc = -EINVAL;
		goto end;
	}

	rc = dp_init_sub_modules(dp);
	if (rc)
		goto end;

	dp_display->post_init = NULL;
end:
	pr_debug("%s\n", rc ? "failed" : "success");
	return rc;
}

static int dp_display_set_mode(struct dp_display *dp_display, void *panel,
		struct dp_display_mode *mode)
{
	const u32 num_components = 3, default_bpp = 24;
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	mode->timing.bpp =
		dp_panel->connector->display_info.bpc * num_components;
	if (!mode->timing.bpp)
		mode->timing.bpp = default_bpp;

	mode->timing.bpp = dp->panel->get_mode_bpp(dp->panel,
			mode->timing.bpp, mode->timing.pixel_clk_khz);

	dp_panel->pinfo = mode->timing;
	dp_panel->init(dp_panel);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_prepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;
	int rc = 0;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector input\n");
		return -EINVAL;
	}

	pr_debug("+++\n");

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (atomic_read(&dp->aborted))
		goto end;

	if (dp->power_on)
		goto end;

	if (!dp_display_is_ready(dp))
		goto end;

	dp_display_host_init(dp);

	if (dp->debug->psm_enabled) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, false);
		dp->debug->psm_enabled = false;
	}

	/*
	 * Execute the dp controller power on in shallow mode here.
	 * In normal cases, controller should have been powered on
	 * by now. In some cases like suspend/resume or framework
	 * reboot, we end up here without a powered on controller.
	 * Cable may have been removed in suspended state. In that
	 * case, link training is bound to fail on system resume.
	 * So, we execute in shallow mode here to do only minimal
	 * and required things.
	 */
	rc = dp->ctrl->on(dp->ctrl, dp->mst.mst_active, dp_panel->fec_en, true);
	if (rc)
		goto end;

end:
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_set_stream_info(struct dp_display *dp_display,
			void *panel, u32 strm_id, u32 start_slot,
			u32 num_slots, u32 pbn, int vcpi)
{
	int rc = 0;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;
	const int max_slots = 64;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (strm_id >= DP_STREAM_MAX) {
		pr_err("invalid stream id:%d\n", strm_id);
		return -EINVAL;
	}

	if (start_slot + num_slots > max_slots) {
		pr_err("invalid channel info received. start:%d, slots:%d\n",
				start_slot, num_slots);
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	dp->ctrl->set_mst_channel_info(dp->ctrl, strm_id,
			start_slot, num_slots);

	if (panel) {
		dp_panel = panel;
		dp_panel->set_stream_info(dp_panel, strm_id, start_slot,
				num_slots, pbn, vcpi);
	}

	mutex_unlock(&dp->session_lock);

	return rc;
}

static void dp_display_update_dsc_resources(struct dp_display_private *dp,
		struct dp_panel *panel, bool enable)
{
	u32 dsc_blk_cnt = 0;

	if (panel->pinfo.comp_info.comp_type == MSM_DISPLAY_COMPRESSION_DSC &&
		panel->pinfo.comp_info.comp_ratio) {
		dsc_blk_cnt = panel->pinfo.h_active /
				dp->parser->max_dp_dsc_input_width_pixs;
		if (panel->pinfo.h_active %
				dp->parser->max_dp_dsc_input_width_pixs)
			dsc_blk_cnt++;
	}

	if (enable) {
		dp->tot_dsc_blks_in_use += dsc_blk_cnt;
		panel->tot_dsc_blks_in_use += dsc_blk_cnt;
	} else {
		dp->tot_dsc_blks_in_use -= dsc_blk_cnt;
		panel->tot_dsc_blks_in_use -= dsc_blk_cnt;
	}
}

static int dp_display_enable(struct dp_display *dp_display, void *panel)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	
	pr_debug("+++\n");

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->core_initialized) {
		pr_err("host not initialized\n");
		goto end;
	}

	rc = dp_display_stream_enable(dp, panel);
	dp_display_update_dsc_resources(dp, panel, true);
	dp->power_on = true;
end:
	mutex_unlock(&dp->session_lock);
	return rc;
}

static void dp_display_stream_post_enable(struct dp_display_private *dp,
			struct dp_panel *dp_panel)
{
	dp_panel->spd_config(dp_panel);
	dp_panel->setup_hdr(dp_panel, NULL);
}

static int dp_display_post_enable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}
	
	pr_debug("+++\n");

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	mutex_lock(&dp->session_lock);

	if (!dp->power_on) {
		pr_debug("stream not setup, return\n");
		goto end;
	}

#ifndef CONFIG_SEC_DISPLAYPORT
	if (atomic_read(&dp->aborted))
		goto end;
#endif

#ifndef CONFIG_SEC_DISPLAYPORT
	if (!dp_display_is_ready(dp) || !dp->core_initialized) {
#else
	if (!dp->core_initialized) {
#endif
		pr_err("display not ready\n");
		goto end;
	}

	dp_display_stream_post_enable(dp, dp_panel);

	if (dp_panel->audio_supported) {
		dp_panel->audio->bw_code = dp->link->link_params.bw_code;
		dp_panel->audio->lane_count = dp->link->link_params.lane_count;
		dp_panel->audio->on(dp_panel->audio);
	}

#ifndef CONFIG_SEC_DISPLAYPORT
	cancel_delayed_work_sync(&dp->hdcp_cb_work);
	queue_delayed_work(dp->wq, &dp->hdcp_cb_work, HZ);
#else
#ifdef SECDP_HDCP_DISABLE
	pr_info("skip hdcp\n");
#else
	schedule_delayed_work(&dp->sec.hdcp_start_work,
					msecs_to_jiffies(3500));
#endif

#ifdef SECDP_SELF_TEST
	if (secdp_self_test_status(ST_HDCP_TEST) >= 0) {
		cancel_delayed_work_sync(&dp->sec.hdcp_start_work);
		secdp_self_test_start_hdcp_test(secdp_self_test_hdcp_on,
						secdp_self_test_hdcp_off);
	}
#endif
#endif

#ifdef CONFIG_SEC_DISPLAYPORT
	/* check poor connection only if it's dex mode */
	if (secdp_check_dex_mode())
		schedule_delayed_work(&dp->sec.link_status_work,
						msecs_to_jiffies(13000));
#endif

end:
	dp->aux->state |= DP_STATE_CTRL_POWERED_ON;

#ifdef CONFIG_SEC_DISPLAYPORT
	atomic_set(&dp->notification_status, 0);
#endif
	complete_all(&dp->notification_comp);
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_pre_disable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel = panel;
	struct dp_link_hdcp_status *status;
	int i, rc = 0;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	pr_debug("+++\n");

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	status = &dp->link->hdcp_status;

	if (!dp->power_on) {
		pr_debug("stream already powered off, return\n");
		goto end;
	}

#ifdef CONFIG_SEC_DISPLAYPORT
	cancel_delayed_work_sync(&dp->sec.hdcp_start_work);
	cancel_delayed_work(&dp->sec.link_status_work);
	cancel_delayed_work(&dp->sec.poor_discon_work);
#endif

	if (dp_display_is_hdcp_enabled(dp) &&
			status->hdcp_state != HDCP_STATE_INACTIVE) {

		if (dp->suspended) {
			pr_debug("Can't perform HDCP cleanup while suspended. Defer\n");
			dp->hdcp_delayed_off = true;
			goto stream;
		}

		flush_delayed_work(&dp->hdcp_cb_work);
		if (dp->mst.mst_active) {
			dp_display_hdcp_deregister_stream(dp,
				dp_panel->stream_id);
			for (i = DP_STREAM_0; i < DP_STREAM_MAX; i++) {
				if (i != dp_panel->stream_id &&
						dp->active_panels[i]) {
					pr_debug("Streams are still active. Skip disabling HDCP\n");
					goto stream;
				}
			}
		}

		if (dp->hdcp.ops->off)
			dp->hdcp.ops->off(dp->hdcp.data);

		dp_display_update_hdcp_status(dp, true);
	}

stream:
	if (dp_panel->audio_supported)
		dp_panel->audio->off(dp_panel->audio);

	rc = dp_display_stream_pre_disable(dp, dp_panel);

end:
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_disable(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp = NULL;
	struct dp_panel *dp_panel = NULL;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	pr_debug("+++\n");

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	mutex_lock(&dp->session_lock);

	if (!dp->power_on || !dp->core_initialized) {
		pr_debug("Link already powered off, return\n");
		goto end;
	}

	dp_display_stream_disable(dp, dp_panel);
	dp_display_update_dsc_resources(dp, dp_panel, false);
end:
#ifdef CONFIG_SEC_DISPLAYPORT
	atomic_set(&dp->notification_status, 0);
#endif
	mutex_unlock(&dp->session_lock);

	pr_debug("---\n");
	return 0;
}

static int dp_request_irq(struct dp_display *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->irq = irq_of_parse_and_map(dp->pdev->dev.of_node, 0);
	if (dp->irq < 0) {
		rc = dp->irq;
		pr_err("failed to get irq: %d\n", rc);
		return rc;
	}

	rc = devm_request_irq(&dp->pdev->dev, dp->irq, dp_display_irq,
		IRQF_TRIGGER_HIGH, "dp_display_isr", dp);
	if (rc < 0) {
		pr_err("failed to request IRQ%u: %d\n",
				dp->irq, rc);
		return rc;
	}
	disable_irq(dp->irq);

	return 0;
}

static struct dp_debug *dp_get_debug(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	return dp->debug;
}

static int dp_display_unprepare(struct dp_display *dp_display, void *panel)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel = panel;
	u32 flags = 0;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	pr_debug("+++, active_stream_cnt <%d>\n", dp->active_stream_cnt);

	mutex_lock(&dp->session_lock);

	/*
	 * Check if the power off sequence was triggered
	 * by a source initialated action like framework
	 * reboot or suspend-resume but not from normal
	 * hot plug.
	 */
	if (dp_display_is_ready(dp))
		flags |= DP_PANEL_SRC_INITIATED_POWER_DOWN;

	if (dp->active_stream_cnt)
		goto end;

	if (flags & DP_PANEL_SRC_INITIATED_POWER_DOWN) {
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);
		dp->debug->psm_enabled = true;

		dp->ctrl->off(dp->ctrl);
		dp_display_host_deinit(dp);
	}

	dp->power_on = false;
	dp->aux->state = DP_STATE_CTRL_POWERED_OFF;

	complete_all(&dp->notification_comp);

	/* log this as it results from user action of cable dis-connection */
	pr_info("[OK]\n");
end:
	dp_panel->deinit(dp_panel, flags);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static enum drm_mode_status dp_display_validate_mode(
		struct dp_display *dp_display,
		void *panel, struct drm_display_mode *mode)
{
	struct dp_display_private *dp;
	struct drm_dp_link *link_info;
	u32 mode_rate_khz = 0, supported_rate_khz = 0, mode_bpp = 0;
	struct dp_panel *dp_panel;
	struct dp_debug *debug;
	enum drm_mode_status mode_status = MODE_BAD;
	bool in_list = false;
	struct dp_mst_connector *mst_connector;
	int hdis, vdis, vref, ar, _hdis, _vdis, _vref, _ar, rate;
	struct dp_display_mode dp_mode;
	bool dsc_en;

	if (!dp_display || !mode || !panel) {
		pr_err("invalid params\n");
		return mode_status;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector\n");
		goto end;
	}

	link_info = &dp->panel->link_info;

	debug = dp->debug;
	if (!debug)
		goto end;

	dp_display->convert_to_dp_mode(dp_display, panel, mode, &dp_mode);

	dsc_en = dp_mode.timing.comp_info.comp_ratio ? true : false;
	mode_bpp = dsc_en ? dp_mode.timing.comp_info.dsc_info.bpp :
			dp_mode.timing.bpp;

	mode_rate_khz = mode->clock * mode_bpp;
	rate = drm_dp_bw_code_to_link_rate(dp->link->link_params.bw_code);
	supported_rate_khz = link_info->num_lanes * rate * 8;

	if (mode_rate_khz > supported_rate_khz) {
		DP_MST_DEBUG("pclk:%d, supported_rate:%d\n",
				mode->clock, supported_rate_khz);
		goto end;
	}

	if (mode->clock > dp_display->max_pclk_khz) {
		DP_MST_DEBUG("clk:%d, max:%d\n", mode->clock,
				dp_display->max_pclk_khz);
		goto end;
	}

	/*
	 * If the connector exists in the mst connector list and if debug is
	 * enabled for that connector, use the mst connector settings from the
	 * list for validation. Otherwise, use non-mst default settings.
	 */
	mutex_lock(&debug->dp_mst_connector_list.lock);

	if (list_empty(&debug->dp_mst_connector_list.list)) {
		mutex_unlock(&debug->dp_mst_connector_list.lock);
		goto verify_default;
	}

	list_for_each_entry(mst_connector, &debug->dp_mst_connector_list.list,
			list) {
		if (mst_connector->con_id == dp_panel->connector->base.id) {
			in_list = true;

			if (!mst_connector->debug_en) {
				mode_status = MODE_OK;
				mutex_unlock(
				&debug->dp_mst_connector_list.lock);
				goto end;
			}

			hdis = mst_connector->hdisplay;
			vdis = mst_connector->vdisplay;
			vref = mst_connector->vrefresh;
			ar = mst_connector->aspect_ratio;

			_hdis = mode->hdisplay;
			_vdis = mode->vdisplay;
			_vref = mode->vrefresh;
			_ar = mode->picture_aspect_ratio;

			if (hdis == _hdis && vdis == _vdis && vref == _vref &&
					ar == _ar) {
				mode_status = MODE_OK;
				mutex_unlock(
				&debug->dp_mst_connector_list.lock);
				goto end;
			}

			break;
		}
	}

	mutex_unlock(&debug->dp_mst_connector_list.lock);

	if (in_list)
		goto end;

verify_default:
	if (debug->debug_en && (mode->hdisplay != debug->hdisplay ||
			mode->vdisplay != debug->vdisplay ||
			mode->vrefresh != debug->vrefresh ||
			mode->picture_aspect_ratio != debug->aspect_ratio))
		goto end;

	mode_status = MODE_OK;
end:
#ifdef CONFIG_SEC_DISPLAYPORT
	if (!secdp_check_supported_resolution(mode, mode_status == MODE_OK))
		mode_status = MODE_BAD;
#endif

	mutex_unlock(&dp->session_lock);

#ifdef CONFIG_SEC_DISPLAYPORT
	pr_info("%s@%dhz | %s | max_pclk: %d | cur_pclk: %d | video_test(%d) | bpp(%u)\n", mode->name,
		drm_mode_vrefresh(mode), mode_status == MODE_BAD ? "not_supported" : "supported",
		dp_display->max_pclk_khz, mode->clock, dp_panel->video_test, mode_bpp);
#endif

	return mode_status;
}

static int dp_display_get_modes(struct dp_display *dp, void *panel,
	struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp_display;
	struct dp_panel *dp_panel;
	int ret = 0;

	if (!dp || !panel) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_panel = panel;
	if (!dp_panel->connector) {
		pr_err("invalid connector\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = dp_panel->get_modes(dp_panel, dp_panel->connector, dp_mode);
	if (dp_mode->timing.pixel_clk_khz)
		dp->max_pclk_khz = dp_mode->timing.pixel_clk_khz;

#ifdef CONFIG_SEC_DISPLAYPORT
	pr_info("get_modes ret : %d\n", ret);
#endif
	return ret;
}

static void dp_display_convert_to_dp_mode(struct dp_display *dp_display,
		void *panel,
		const struct drm_display_mode *drm_mode,
		struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;
	u32 free_dsc_blks = 0, required_dsc_blks = 0;

	if (!dp_display || !drm_mode || !dp_mode || !panel) {
		pr_err("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	memset(dp_mode, 0, sizeof(*dp_mode));

	free_dsc_blks = dp->parser->max_dp_dsc_blks -
				dp->tot_dsc_blks_in_use +
				dp_panel->tot_dsc_blks_in_use;
	required_dsc_blks = drm_mode->hdisplay /
				dp->parser->max_dp_dsc_input_width_pixs;
	if (drm_mode->hdisplay % dp->parser->max_dp_dsc_input_width_pixs)
		required_dsc_blks++;

	if (free_dsc_blks >= required_dsc_blks)
		dp_mode->capabilities |= DP_PANEL_CAPS_DSC;

#ifndef CONFIG_SEC_DISPLAYPORT
	pr_debug("in_use:%d, max:%d, free:%d, req:%d, caps:0x%x, width:%d",
			dp->tot_dsc_blks_in_use, dp->parser->max_dp_dsc_blks,
			free_dsc_blks, required_dsc_blks, dp_mode->capabilities,
			dp->parser->max_dp_dsc_input_width_pixs);
#endif

	dp_panel->convert_to_dp_mode(dp_panel, drm_mode, dp_mode);
}

static int dp_display_config_hdr(struct dp_display *dp_display, void *panel,
			struct drm_msm_ext_hdr_metadata *hdr)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct dp_panel *dp_panel;

	if (!dp_display || !panel) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	dp_panel = panel;

	rc = dp_panel->setup_hdr(dp_panel, hdr);

	return rc;
}

static int dp_display_create_workqueue(struct dp_display_private *dp)
{
	dp->wq = create_singlethread_workqueue("drm_dp");
	if (IS_ERR_OR_NULL(dp->wq)) {
		pr_err("Error creating wq\n");
		return -EPERM;
	}

	INIT_DELAYED_WORK(&dp->hdcp_cb_work, dp_display_hdcp_cb_work);
	INIT_WORK(&dp->connect_work, dp_display_connect_work);
	INIT_WORK(&dp->attention_work, dp_display_attention_work);

	return 0;
}

/* aux switch error is coming repeatedly during boot time when samsung displayport feature is disabled.
 * so, disabled following codes.
 */
#if 0 /* CONFIG_SEC_DISPLAYPORT */
static int dp_display_fsa4480_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	return 0;
}

static int dp_display_init_aux_switch(struct dp_display_private *dp)
{
	int rc = 0;
	const char *phandle = "qcom,dp-aux-switch";
	struct notifier_block nb;

	if (!dp->pdev->dev.of_node) {
		pr_err("cannot find dev.of_node\n");
		rc = -ENODEV;
		goto end;
	}

	dp->aux_switch_node = of_parse_phandle(dp->pdev->dev.of_node,
			phandle, 0);
	if (!dp->aux_switch_node) {
		pr_warn("cannot parse %s handle\n", phandle);
		goto end;
	}

	nb.notifier_call = dp_display_fsa4480_callback;
	nb.priority = 0;

	rc = fsa4480_reg_notifier(&nb, dp->aux_switch_node);
	if (rc) {
		pr_err("failed to register notifier (%d)\n", rc);
		goto end;
	}

	fsa4480_unreg_notifier(&nb, dp->aux_switch_node);

end:
	return rc;
}
#endif

static int dp_display_mst_install(struct dp_display *dp_display,
			struct dp_mst_drm_install_info *mst_install_info)
{
	struct dp_display_private *dp;

	if (!dp_display || !mst_install_info) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!mst_install_info->cbs->hpd || !mst_install_info->cbs->hpd_irq) {
		pr_err("invalid mst cbs\n");
		return -EINVAL;
	}

	dp_display->dp_mst_prv_info = mst_install_info->dp_mst_prv_info;

	if (!dp->parser->has_mst) {
		pr_debug("mst not enabled\n");
		return -EPERM;
	}

	memcpy(&dp->mst.cbs, mst_install_info->cbs, sizeof(dp->mst.cbs));
	dp->mst.drm_registered = true;

	DP_MST_DEBUG("dp mst drm installed\n");

	return 0;
}

static int dp_display_mst_uninstall(struct dp_display *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	dp = container_of(dp_display, struct dp_display_private,
				dp_display);
	memset(&dp->mst.cbs, 0, sizeof(dp->mst.cbs));
	dp->mst.drm_registered = false;

	DP_MST_DEBUG("dp mst drm uninstalled\n");

	return 0;
}

static int dp_display_mst_connector_install(struct dp_display *dp_display,
		struct drm_connector *connector)
{
	int rc = 0;
	struct dp_panel_in panel_in;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;
	struct dp_mst_connector *mst_connector;

	if (!dp_display || !connector) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	panel_in.dev = &dp->pdev->dev;
	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;
	panel_in.connector = connector;
	panel_in.base_panel = dp->panel;
	panel_in.parser = dp->parser;

	dp_panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp_panel)) {
		rc = PTR_ERR(dp_panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		mutex_unlock(&dp->session_lock);
		return rc;
	}

	dp_panel->audio = dp_audio_get(dp->pdev, dp_panel, &dp->catalog->audio);
	if (IS_ERR(dp_panel->audio)) {
		rc = PTR_ERR(dp_panel->audio);
		pr_err("[mst] failed to initialize audio, rc = %d\n", rc);
		dp_panel->audio = NULL;
		mutex_unlock(&dp->session_lock);
		return rc;
	}

	DP_MST_DEBUG("dp mst connector installed. conn:%d\n",
			connector->base.id);

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);

	mst_connector = kmalloc(sizeof(struct dp_mst_connector),
			GFP_KERNEL);
	if (!mst_connector) {
		mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
		mutex_unlock(&dp->session_lock);
		return -ENOMEM;
	}

	mst_connector->debug_en = false;
	mst_connector->conn = connector;
	mst_connector->con_id = connector->base.id;
	mst_connector->state = connector_status_unknown;
	INIT_LIST_HEAD(&mst_connector->list);

	list_add(&mst_connector->list,
			&dp->debug->dp_mst_connector_list.list);

	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);

	return 0;
}

static int dp_display_mst_connector_uninstall(struct dp_display *dp_display,
			struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;
	struct dp_mst_connector *con_to_remove, *temp_con;

	if (!dp_display || !connector) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid panel for connector:%d\n", connector->base.id);
		mutex_unlock(&dp->session_lock);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	dp_audio_put(dp_panel->audio);
	dp_panel_put(dp_panel);

	DP_MST_DEBUG("dp mst connector uninstalled. conn:%d\n",
			connector->base.id);

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);

	list_for_each_entry_safe(con_to_remove, temp_con,
			&dp->debug->dp_mst_connector_list.list, list) {
		if (con_to_remove->conn == connector) {
			list_del(&con_to_remove->list);
			kfree(con_to_remove);
		}
	}

	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);

	return rc;
}

static int dp_display_mst_get_connector_info(struct dp_display *dp_display,
			struct drm_connector *connector,
			struct dp_mst_connector *mst_conn)
{
	struct dp_display_private *dp;
	struct dp_mst_connector *conn, *temp_conn;

	if (!connector || !mst_conn) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mutex_lock(&dp->session_lock);
	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		mutex_unlock(&dp->session_lock);
		return -EPERM;
	}

	mutex_lock(&dp->debug->dp_mst_connector_list.lock);
	list_for_each_entry_safe(conn, temp_conn,
			&dp->debug->dp_mst_connector_list.list, list) {
		if (conn->con_id == connector->base.id)
			memcpy(mst_conn, conn, sizeof(*mst_conn));
	}
	mutex_unlock(&dp->debug->dp_mst_connector_list.lock);
	mutex_unlock(&dp->session_lock);
	return 0;
}

static int dp_display_mst_connector_update_edid(struct dp_display *dp_display,
			struct drm_connector *connector,
			struct edid *edid)
{
	int rc = 0;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	if (!dp_display || !connector || !edid) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return -EPERM;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	rc = dp_panel->update_edid(dp_panel, edid);

	DP_MST_DEBUG("dp mst connector:%d edid updated. mode_cnt:%d\n",
			connector->base.id, rc);

	return rc;
}

static int dp_display_update_pps(struct dp_display *dp_display,
		struct drm_connector *connector, char *pps_cmd)
{
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_private *dp;

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid panel for connector:%d\n", connector->base.id);
		return -EINVAL;
	}

	dp_panel = sde_conn->drv_panel;
	dp_panel->update_pps(dp_panel, pps_cmd);
	return 0;
}

static int dp_display_get_mst_caps(struct dp_display *dp_display,
			struct dp_mst_caps *mst_caps)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display || !mst_caps) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	mst_caps->has_mst = dp->parser->has_mst;
	mst_caps->max_streams_supported = (mst_caps->has_mst) ? 2 : 0;
	mst_caps->max_dpcd_transaction_bytes = (mst_caps->has_mst) ? 16 : 0;
	mst_caps->drm_aux = dp->aux->drm_aux;

	return rc;
}

static void dp_display_wakeup_phy_layer(struct dp_display *dp_display,
		bool wakeup)
{
	struct dp_display_private *dp;
	struct dp_hpd *hpd;

	if (!dp_display) {
		pr_err("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (!dp->mst.drm_registered) {
		pr_debug("drm mst not registered\n");
		return;
	}

	hpd = dp->hpd;
	if (hpd && hpd->wakeup_phy)
		hpd->wakeup_phy(hpd, wakeup);
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		rc = -ENODEV;
		goto bail;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp) {
		rc = -ENOMEM;
		goto bail;
	}

	init_completion(&dp->notification_comp);
#ifdef CONFIG_SEC_DISPLAYPORT
	init_completion(&dp->dp_off_comp);
	init_completion(&dp->dp_disconnect_comp);

	dp->reboot_nb.notifier_call = secdp_reboot_cb;
	register_reboot_notifier(&dp->reboot_nb);

#ifdef CONFIG_SWITCH
	rc = switch_dev_register(&switch_secdp_hpd);
	if (rc)
		pr_info("Failed to register secdp switch(%d)\n", rc);

	rc = switch_dev_register(&switch_secdp_msg);
	if (rc)
		pr_info("Failed to register secdp_msg switch(%d)\n", rc);
#endif
#endif

	dp->pdev = pdev;
	dp->name = "drm_dp";

	memset(&dp->mst, 0, sizeof(dp->mst));
	atomic_set(&dp->aborted, 0);

/* aux switch error is coming repeatedly during boot time when samsung displayport feature is disabled.
 * so, disabled following codes.
 */
#if 0 /* CONFIG_SEC_DISPLAYPORT */
	rc = dp_display_init_aux_switch(dp);
	if (rc) {
		rc = -EPROBE_DEFER;
		goto error;
	}
#endif

	rc = dp_display_create_workqueue(dp);
	if (rc) {
		pr_err("Failed to create workqueue\n");
		goto error;
	}

#ifdef CONFIG_SEC_DISPLAYPORT
	g_secdp_priv = dp;
	secdp_logger_init();
	atomic_set(&dp->notification_status, 0);
#endif

	platform_set_drvdata(pdev, dp);

	g_dp_display = &dp->dp_display;

	g_dp_display->enable        = dp_display_enable;
	g_dp_display->post_enable   = dp_display_post_enable;
	g_dp_display->pre_disable   = dp_display_pre_disable;
	g_dp_display->disable       = dp_display_disable;
	g_dp_display->set_mode      = dp_display_set_mode;
	g_dp_display->validate_mode = dp_display_validate_mode;
	g_dp_display->get_modes     = dp_display_get_modes;
	g_dp_display->prepare       = dp_display_prepare;
	g_dp_display->unprepare     = dp_display_unprepare;
	g_dp_display->request_irq   = dp_request_irq;
	g_dp_display->get_debug     = dp_get_debug;
	g_dp_display->post_open     = NULL;
	g_dp_display->post_init     = dp_display_post_init;
	g_dp_display->config_hdr    = dp_display_config_hdr;
	g_dp_display->mst_install   = dp_display_mst_install;
	g_dp_display->mst_uninstall = dp_display_mst_uninstall;
	g_dp_display->mst_connector_install = dp_display_mst_connector_install;
	g_dp_display->mst_connector_uninstall =
					dp_display_mst_connector_uninstall;
	g_dp_display->mst_connector_update_edid =
					dp_display_mst_connector_update_edid;
	g_dp_display->get_mst_caps = dp_display_get_mst_caps;
	g_dp_display->set_stream_info = dp_display_set_stream_info;
	g_dp_display->update_pps = dp_display_update_pps;
	g_dp_display->convert_to_dp_mode = dp_display_convert_to_dp_mode;
	g_dp_display->mst_get_connector_info =
					dp_display_mst_get_connector_info;
	g_dp_display->wakeup_phy_layer =
					dp_display_wakeup_phy_layer;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc) {
		pr_err("component add failed, rc=%d\n", rc);
		goto error;
	}

	pr_info("exit, rc(%d)\n", rc);

	return 0;
error:
	devm_kfree(&pdev->dev, dp);
bail:
	return rc;
}

int dp_display_get_displays(void **displays, int count)
{
	if (!displays) {
		pr_err("invalid data\n");
		return -EINVAL;
	}

	if (count != 1) {
		pr_err("invalid number of displays\n");
		return -EINVAL;
	}

	displays[0] = g_dp_display;
	return count;
}

int dp_display_get_num_of_displays(void)
{
	if (!g_dp_display)
		return 0;

	return 1;
}

int dp_display_get_num_of_streams(void)
{
	return DP_STREAM_MAX;
}

static void dp_display_set_mst_state(void *dp_display,
		enum dp_drv_state mst_state)
{
	struct dp_display_private *dp;

	if (!g_dp_display) {
		pr_debug("dp display not initialized\n");
		return;
	}
	dp = container_of(g_dp_display, struct dp_display_private, dp_display);
	if (dp->mst.mst_active && dp->mst.cbs.set_drv_state)
		dp->mst.cbs.set_drv_state(g_dp_display, mst_state);
}

static int dp_display_remove(struct platform_device *pdev)
{
	struct dp_display_private *dp;

	if (!pdev)
		return -EINVAL;

	dp = platform_get_drvdata(pdev);

	dp_display_deinit_sub_modules(dp);

	if (dp->wq)
		destroy_workqueue(dp->wq);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, dp);

#if defined(CONFIG_SEC_DISPLAYPORT) && defined(CONFIG_SWITCH)
	switch_dev_unregister(&switch_secdp_hpd); 
	switch_dev_unregister(&switch_secdp_msg); 
#endif
	return 0;
}

static int dp_pm_prepare(struct device *dev)
{
	struct dp_display_private *dp = container_of(g_dp_display,
			struct dp_display_private, dp_display);

	dp_display_set_mst_state(g_dp_display, PM_SUSPEND);

	dp->suspended = true;

	return 0;
}

static void dp_pm_complete(struct device *dev)
{
	struct dp_display_private *dp = container_of(g_dp_display,
			struct dp_display_private, dp_display);

	dp_display_set_mst_state(g_dp_display, PM_DEFAULT);

	dp->suspended = false;
}

static const struct dev_pm_ops dp_pm_ops = {
	.prepare = dp_pm_prepare,
	.complete = dp_pm_complete,
};

static struct platform_driver dp_display_driver = {
	.probe  = dp_display_probe,
	.remove = dp_display_remove,
	.driver = {
		.name = "msm-dp-display",
		.of_match_table = dp_dt_match,
		.suppress_bind_attrs = true,
		.pm = &dp_pm_ops,
	},
};

static int __init dp_display_init(void)
{
	int ret;

	ret = platform_driver_register(&dp_display_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}

	return ret;
}
late_initcall(dp_display_init);

static void __exit dp_display_cleanup(void)
{
	platform_driver_unregister(&dp_display_driver);
}
module_exit(dp_display_cleanup);

