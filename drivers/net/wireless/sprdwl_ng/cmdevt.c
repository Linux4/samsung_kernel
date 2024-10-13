/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sprdwl.h"
#include "cmdevt.h"
#include "cfg80211.h"
#include "msg.h"
#include "txrx.h"
#include "intf_ops.h"
#include "vendor.h"

struct sprdwl_cmd {
	u8 cmd_id;
	int init_ok;
	u32 mstime;
	void *data;
	atomic_t refcnt;
	/* spin lock for command */
	spinlock_t lock;
	/* mutex for command */
	struct mutex cmd_lock;
	wait_queue_head_t waitq;
};

static struct sprdwl_cmd g_sprdwl_cmd;

static int bss_count;
static const char *cmd2str(u8 cmd)
{
#define C2S(x) case x: return #x;
	switch (cmd) {
	C2S(WIFI_CMD_GET_INFO)
	C2S(WIFI_CMD_SET_REGDOM)
	C2S(WIFI_CMD_OPEN)
	C2S(WIFI_CMD_CLOSE)
	C2S(WIFI_CMD_POWER_SAVE)
	C2S(WIFI_CMD_SET_PARAM)
	C2S(WIFI_CMD_REQ_LTE_CONCUR)

	C2S(WIFI_CMD_CONNECT)

	C2S(WIFI_CMD_SCAN)
	C2S(WIFI_CMD_SCHED_SCAN)
	C2S(WIFI_CMD_DISCONNECT)
	C2S(WIFI_CMD_KEY)
	C2S(WIFI_CMD_SET_PMKSA)
	C2S(WIFI_CMD_GET_STATION)
	C2S(WIFI_CMD_SET_CHANNEL)

	C2S(WIFI_CMD_START_AP)
	C2S(WIFI_CMD_DEL_STATION)
	C2S(WIFI_CMD_SET_BLACKLIST)
	C2S(WIFI_CMD_SET_WHITELIST)
	C2S(WIFI_CMD_MULTICAST_FILTER)

	C2S(WIFI_CMD_TX_MGMT)
	C2S(WIFI_CMD_REGISTER_FRAME)
	C2S(WIFI_CMD_REMAIN_CHAN)
	C2S(WIFI_CMD_CANCEL_REMAIN_CHAN)

	C2S(WIFI_CMD_SET_IE)
	C2S(WIFI_CMD_NOTIFY_IP_ACQUIRED)

	C2S(WIFI_CMD_SET_CQM)
	C2S(WIFI_CMD_SET_ROAM_OFFLOAD)
	C2S(WIFI_CMD_SET_MEASUREMENT)
	C2S(WIFI_CMD_SET_QOS_MAP)
	C2S(WIFI_CMD_TDLS)
	C2S(WIFI_CMD_11V)
	C2S(WIFI_CMD_NPI_MSG)
	C2S(WIFI_CMD_NPI_GET)

	C2S(WIFI_CMD_ASSERT)
	C2S(WIFI_CMD_FLUSH_SDIO)
	C2S(WIFI_CMD_ADD_TX_TS)
	C2S(WIFI_CMD_DEL_TX_TS)
	C2S(WIFI_CMD_LLSTAT)

	C2S(WIFI_CMD_GSCAN)
	default : return "WIFI_CMD_UNKNOWN";
	}
#undef C2S
}

void sprdwl_cmd_init(void)
{
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;
	/* memset(cmd, 0, sizeof(*cmd)); */
	cmd->data = NULL;
	spin_lock_init(&cmd->lock);
	mutex_init(&cmd->cmd_lock);
	init_waitqueue_head(&cmd->waitq);
	cmd->init_ok = 1;
}

void sprdwl_cmd_wake_upall(void)
{
	wake_up_all(&g_sprdwl_cmd.waitq);
}

static void sprdwl_cmd_set(struct sprdwl_cmd_hdr *hdr)
{
	s64 msec;
	struct timeval tv;
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;

	do_gettimeofday(&tv);
	msec = ((s64)tv.tv_sec * MSEC_PER_SEC) + tv.tv_usec / USEC_PER_MSEC;
	hdr->mstime = cpu_to_le32(msec);
	spin_lock_bh(&cmd->lock);
	kfree(cmd->data);
	cmd->data = NULL;
	cmd->mstime = (u32)msec;
	cmd->cmd_id = hdr->cmd_id;
	spin_unlock_bh(&cmd->lock);
}

static void sprdwl_cmd_clean(struct sprdwl_cmd *cmd)
{
	spin_lock_bh(&cmd->lock);
	kfree(cmd->data);
	cmd->data = NULL;
	cmd->mstime = 0;
	cmd->cmd_id = 0;
	spin_unlock_bh(&cmd->lock);
}

#define SPRDWL_CMD_EXIT_VAL 0x8000
void sprdwl_cmd_deinit(void)
{
	unsigned long timeout;
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;

	atomic_add(SPRDWL_CMD_EXIT_VAL, &cmd->refcnt);
	wake_up_all(&cmd->waitq);
	timeout = jiffies + msecs_to_jiffies(1000);
	while (atomic_read(&cmd->refcnt) > SPRDWL_CMD_EXIT_VAL) {
		if (time_after(jiffies, timeout)) {
			pr_err("%s cmd lock timeout\n", __func__);
			break;
		}
		usleep_range(2000, 2500);
	}
	sprdwl_cmd_clean(cmd);
	mutex_destroy(&cmd->cmd_lock);
}

static int sprdwl_cmd_lock(struct sprdwl_cmd *cmd)
{
	if (atomic_inc_return(&cmd->refcnt) >= SPRDWL_CMD_EXIT_VAL) {
		atomic_dec(&cmd->refcnt);
		pr_err("%s failed\n", __func__);
		return -1;
	}
	mutex_lock(&cmd->cmd_lock);

	return 0;
}

static void sprdwl_cmd_unlock(struct sprdwl_cmd *cmd)
{
	mutex_unlock(&cmd->cmd_lock);
	atomic_dec(&cmd->refcnt);
}

struct sprdwl_msg_buf *sprdwl_cmd_getbuf(struct sprdwl_priv *priv,
					 u16 len, enum sprdwl_mode mode,
					 enum sprdwl_head_rsp rsp, u8 cmd_id)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_hdr *hdr;
	u16 plen = sizeof(*hdr) + len;

	msg = sprdwl_intf_get_msg_buf(priv, SPRDWL_TYPE_CMD, mode);
	if (!msg)
		return NULL;

	msg->skb = dev_alloc_skb(plen);
	if (msg->skb) {
		memset(msg->skb->data, 0, plen);
		hdr = (struct sprdwl_cmd_hdr *)msg->skb->data;
		hdr->common.type = SPRDWL_TYPE_CMD;
		hdr->common.reserv = 0;
		hdr->common.rsp = rsp;
		hdr->common.mode = mode;
		hdr->plen = cpu_to_le16(plen);
		hdr->cmd_id = cmd_id;
		sprdwl_fill_msg(msg, msg->skb, msg->skb->data, plen);
		msg->data = hdr + 1;
	} else {
		pr_err("%s failed to allocate skb\n", __func__);
		sprdwl_intf_free_msg_buf(priv, msg);
		return NULL;
	}

	return msg;
}

/* if erro, data is released in this function
 * if OK, data is released in sdiom
 */
static int sprdwl_cmd_send_to_ic(struct sprdwl_priv *priv,
				 struct sprdwl_msg_buf *msg)
{
	struct sprdwl_cmd_hdr *hdr;
	u8 mode;

	hdr = (struct sprdwl_cmd_hdr *)msg->skb->data;
	mode = hdr->common.mode;
	if (hdr->common.rsp)
		sprdwl_cmd_set(hdr);

	wiphy_info(priv->wiphy, "[%u]mode %d send[%s]\n",
		   le32_to_cpu(hdr->mstime), mode, cmd2str(hdr->cmd_id));

	print_hex_dump_debug("CMD: ", DUMP_PREFIX_OFFSET, 16, 1,
			     ((u8 *)hdr + sizeof(*hdr)),
			     hdr->plen - sizeof(*hdr), 0);

	return sprdwl_send_cmd(priv, msg);
}

static int sprdwl_timeout_recv_rsp(struct sprdwl_priv *priv,
				   unsigned int timeout)
{
	int ret;
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;

	ret = wait_event_timeout(cmd->waitq,
				 (cmd->data || sprdwl_intf_is_exit(priv)),
				 msecs_to_jiffies(timeout));
	if (!ret) {
		wiphy_err(priv->wiphy, "[%s]timeout\n", cmd2str(cmd->cmd_id));
		return -1;
	} else if (sprdwl_intf_is_exit(priv) ||
		   atomic_read(&cmd->refcnt) >= SPRDWL_CMD_EXIT_VAL)
		return -1;

	spin_lock_bh(&cmd->lock);
	ret = cmd->data ? 0 : -1;
	spin_unlock_bh(&cmd->lock);

	return ret;
}

/* msg is released in this function or the realy driver
 * rbuf: the msg after sprdwl_cmd_hdr
 * rlen: input the length of rbuf
 *       output the length of the msg,if *rlen == 0, rbuf get nothing
 */
int sprdwl_cmd_send_recv(struct sprdwl_priv *priv,
			 struct sprdwl_msg_buf *msg,
			 unsigned int timeout, u8 *rbuf, u16 *rlen)
{
	u8 cmd_id;
	u16 plen;
	int ret = 0;
	struct sprdwl_cmd_rsp_state_code state;
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;
	struct sprdwl_cmd_hdr *hdr;

	if (sprdwl_cmd_lock(cmd)) {
		sprdwl_intf_free_msg_buf(priv, msg);
		dev_kfree_skb(msg->skb);
		if (rlen)
			*rlen = 0;
		goto out;
	}
	state.code = 0;
	hdr = (struct sprdwl_cmd_hdr *)msg->skb->data;
	cmd_id = hdr->cmd_id;

	ret = sprdwl_cmd_send_to_ic(priv, msg);
	if (ret) {
		sprdwl_cmd_unlock(cmd);
		return -1;
	}
	ret = sprdwl_timeout_recv_rsp(priv, timeout);
	if (ret != -1) {
		if (rbuf && rlen && *rlen) {
			hdr = (struct sprdwl_cmd_hdr *)cmd->data;
			plen = le16_to_cpu(hdr->plen) - sizeof(*hdr);
			*rlen = min(*rlen, plen);
			memcpy(rbuf, hdr->paydata, *rlen);
		}
	} else
		wiphy_err(priv->wiphy, "mode %d [%s]rsp timeout\n",
			  hdr->common.mode, cmd2str(cmd_id));
	sprdwl_cmd_unlock(cmd);
out:
	return ret;
}

/* Commands */
int sprdwl_get_fw_info(struct sprdwl_priv *priv)
{
	int ret;
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_fw_info *p;
	u16 r_len = sizeof(*p);
	u8 r_buf[r_len];

	msg = sprdwl_cmd_getbuf(priv, 0, SPRDWL_MODE_NONE,
				SPRDWL_HEAD_RSP, WIFI_CMD_GET_INFO);
	if (!msg)
		return -ENOMEM;

	ret = sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, &r_len);
	if (!ret && r_len) {
		p = (struct sprdwl_cmd_fw_info *)r_buf;
		priv->chip_model = p->chip_model;
		priv->chip_ver = p->chip_version;
		priv->fw_ver = p->fw_version;
		priv->fw_capa = p->fw_capa;
		priv->fw_std = p->fw_std;
		priv->max_ap_assoc_sta = p->max_ap_assoc_sta;
		priv->max_acl_mac_addrs = p->max_acl_mac_addrs;
		priv->max_mc_mac_addrs = p->max_mc_mac_addrs;
		priv->wnm_ft_support = p->wnm_ft_support;
		wiphy_info(priv->wiphy, "chip_model:0x%x, chip_ver:0x%x\n",
			   priv->chip_model, priv->chip_ver);
		wiphy_info(priv->wiphy,
			   "fw_ver:%d, fw_std:0x%x, fw_capa:0x%x\n",
			   priv->fw_ver, priv->fw_std, priv->fw_capa);
	}

	return ret;
}

int sprdwl_set_regdom(struct sprdwl_priv *priv, u8 *regdom, u32 len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_ieee80211_regdomain *p;

	msg = sprdwl_cmd_getbuf(priv, len, SPRDWL_MODE_NONE, SPRDWL_HEAD_RSP,
				WIFI_CMD_SET_REGDOM);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_ieee80211_regdomain *)msg->data;
	memcpy(p, regdom, len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_open_fw(struct sprdwl_priv *priv, u8 vif_mode,
		   u8 mode, u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_open *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_OPEN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_open *)msg->data;
	p->mode = mode;
	if (mac_addr)
		memcpy(&p->mac[0], mac_addr, sizeof(p->mac));

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_close_fw(struct sprdwl_priv *priv, u8 vif_mode, u8 mode)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_close *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_CLOSE);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_close *)msg->data;
	p->mode = mode;

	sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
	/* FIXME - in case of close failure */
	return 0;
}

int sprdwl_power_save(struct sprdwl_priv *priv, u8 vif_mode,
		      u8 sub_type, u8 status)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_power_save *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_POWER_SAVE);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_power_save *)msg->data;
	p->sub_type = sub_type;
	p->value = status;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_add_key(struct sprdwl_priv *priv, u8 vif_mode, const u8 *key_data,
		   u8 key_len, u8 pairwise, u8 key_index, const u8 *key_seq,
		   u8 cypher_type, const u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_add_key *p;
	u8 *sub_cmd;
	int datalen = sizeof(*p) + sizeof(*sub_cmd) + key_len;

	msg = sprdwl_cmd_getbuf(priv, datalen, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SPRDWL_SUBCMD_ADD;
	p = (struct sprdwl_cmd_add_key *)(++sub_cmd);

	p->key_index = key_index;
	p->pairwise = pairwise;
	p->cypher_type = cypher_type;
	p->key_len = key_len;
	if (key_seq)
		memcpy(p->keyseq, key_seq, 8);
	if (mac_addr)
		memcpy(p->mac, mac_addr, ETH_ALEN);
	if (key_data)
		memcpy(p->value, key_data, key_len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_del_key(struct sprdwl_priv *priv, u8 vif_mode,
		   u16 key_index, const u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_del_key *p;
	u8 *sub_cmd;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + sizeof(*sub_cmd), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SPRDWL_SUBCMD_DEL;
	p = (struct sprdwl_cmd_del_key *)(++sub_cmd);

	p->key_index = key_index;
	if (mac_addr)
		memcpy(p->mac, mac_addr, ETH_ALEN);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_def_key(struct sprdwl_priv *priv, u8 vif_mode, u8 key_index)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_def_key *p;
	u8 *sub_cmd;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + sizeof(*sub_cmd), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SPRDWL_SUBCMD_SET;
	p = (struct sprdwl_cmd_set_def_key *)(++sub_cmd);

	p->key_index = key_index;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_ie(struct sprdwl_priv *priv, u8 vif_mode, u8 type,
		  const u8 *ie, u16 len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_ie *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + len, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_IE);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_set_ie *)msg->data;
	p->type = type;
	p->len = len;
	memcpy(p->data, ie, len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_start_ap(struct sprdwl_priv *priv, u8 vif_mode, u8 *beacon, u16 len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_start_ap *p;
	u16 datalen = sizeof(*p) + len;

	msg = sprdwl_cmd_getbuf(priv, datalen, vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_START_AP);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_start_ap *)msg->data;
	p->len = cpu_to_le16(len);
	memcpy(p->value, beacon, len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_del_station(struct sprdwl_priv *priv, u8 vif_mode,
		       const u8 *mac_addr, u16 reason_code)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_del_station *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_DEL_STATION);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_del_station *)msg->data;
	if (mac_addr)
		memcpy(&p->mac[0], mac_addr, sizeof(p->mac));
	p->reason_code = cpu_to_le16(reason_code);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_get_station(struct sprdwl_priv *priv, u8 vif_mode,
		       u8 *signal, u8 *noise, u8 *rate, u32 *failed)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_get_station sta;
	u8 *r_buf = (u8 *)&sta;
	u16 r_len = sizeof(sta);
	int ret;

	msg = sprdwl_cmd_getbuf(priv, 0, vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_GET_STATION);
	if (!msg)
		return -ENOMEM;
	ret = sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, &r_len);
	if (!ret && r_len) {
		*rate = sta.txrate;
		*failed = sta.txfailed;
		*signal = sta.signal;
		*noise = sta.noise;
	}

	return ret;
}

int sprdwl_set_channel(struct sprdwl_priv *priv, u8 vif_mode, u8 channel)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_channel *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_CHANNEL);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_set_channel *)msg->data;
	p->channel = channel;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_scan(struct sprdwl_priv *priv, u8 vif_mode, u32 channels,
		int ssid_len, const u8 *ssid_list)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_scan *p;
	struct sprdwl_cmd_rsp_state_code state;
	u16 rlen;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + ssid_len, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SCAN);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_scan *)msg->data;
	p->channels = channels;
	if (ssid_len > 0) {
		memcpy(p->ssid, ssid_list, ssid_len);
		p->ssid_len = cpu_to_le16(ssid_len);
	}

	rlen = sizeof(state);
	/* FIXME, cp may return err state here */
	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT,
				    (u8 *)&state, &rlen);
}

int sprdwl_sched_scan_start(struct sprdwl_priv *priv, u8 vif_mode,
			    struct sprdwl_sched_scan_buf *buf)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_sched_scan_hd *sscan_head = NULL;
	struct sprdwl_cmd_sched_scan_ie_hd *ie_head = NULL;
	struct sprdwl_cmd_sched_scan_ifrc *sscan_ifrc = NULL;
	u16 datalen;
	u8 *p = NULL;
	int len = 0, i, hd_len;

	datalen = sizeof(*sscan_head) + sizeof(*ie_head) + sizeof(*sscan_ifrc)
	    + buf->n_ssids * IEEE80211_MAX_SSID_LEN
	    + buf->n_match_ssids * IEEE80211_MAX_SSID_LEN + buf->ie_len;
	hd_len = sizeof(*ie_head);
	datalen = datalen + (buf->n_ssids ? hd_len : 0)
	    + (buf->n_match_ssids ? hd_len : 0)
	    + (buf->ie_len ? hd_len : 0);

	msg = sprdwl_cmd_getbuf(priv, datalen, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SCHED_SCAN);
	if (!msg)
		return -ENOMEM;

	p = msg->data;

	sscan_head = (struct sprdwl_cmd_sched_scan_hd *)(p + len);
	sscan_head->started = 1;
	sscan_head->buf_flags = SPRDWL_SCHED_SCAN_BUF_END;
	len += sizeof(*sscan_head);

	ie_head = (struct sprdwl_cmd_sched_scan_ie_hd *)(p + len);
	ie_head->ie_flag = SPRDWL_SEND_FLAG_IFRC;
	ie_head->ie_len = sizeof(*sscan_ifrc);
	len += sizeof(*ie_head);

	sscan_ifrc = (struct sprdwl_cmd_sched_scan_ifrc *)(p + len);

	sscan_ifrc->interval = buf->interval;
	sscan_ifrc->flags = buf->flags;
	sscan_ifrc->rssi_thold = buf->rssi_thold;
	memcpy(sscan_ifrc->chan, buf->channel, 16);
	len += ie_head->ie_len;

	if (buf->n_ssids > 0) {
		ie_head = (struct sprdwl_cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRDWL_SEND_FLAG_SSID;
		ie_head->ie_len = buf->n_ssids * IEEE80211_MAX_SSID_LEN;
		len += sizeof(*ie_head);
		for (i = 0; i < buf->n_ssids; i++) {
			memcpy((p + len + i * IEEE80211_MAX_SSID_LEN),
			       buf->ssid[i], IEEE80211_MAX_SSID_LEN);
		}
		len += ie_head->ie_len;
	}

	if (buf->n_match_ssids > 0) {
		ie_head = (struct sprdwl_cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRDWL_SEND_FLAG_MSSID;
		ie_head->ie_len = buf->n_match_ssids * IEEE80211_MAX_SSID_LEN;
		len += sizeof(*ie_head);
		for (i = 0; i < buf->n_match_ssids; i++) {
			memcpy((p + len + i * IEEE80211_MAX_SSID_LEN),
			       buf->mssid[i], IEEE80211_MAX_SSID_LEN);
		}
		len += ie_head->ie_len;
	}

	if (buf->ie_len > 0) {
		ie_head = (struct sprdwl_cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRDWL_SEND_FLAG_IE;
		ie_head->ie_len = buf->ie_len;
		len += sizeof(*ie_head);

		wiphy_info(priv->wiphy, "%s: ie len is %zu\n",
			   __func__, buf->ie_len);
		wiphy_info(priv->wiphy, "ie:%s", buf->ie);
		memcpy((p + len), buf->ie, buf->ie_len);
		len += ie_head->ie_len;
	}

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_sched_scan_stop(struct sprdwl_priv *priv, u8 vif_mode)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_sched_scan_hd *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SCHED_SCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_sched_scan_hd *)msg->data;
	p->started = 0;
	p->buf_flags = SPRDWL_SCHED_SCAN_BUF_END;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_connect(struct sprdwl_priv *priv, u8 vif_mode,
		   struct sprdwl_cmd_connect *p)
{
	struct sprdwl_msg_buf *msg;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_CONNECT);
	if (!msg)
		return -ENOMEM;

	memcpy(msg->data, p, sizeof(*p));

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_disconnect(struct sprdwl_priv *priv, u8 vif_mode, u16 reason_code)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_disconnect *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_DISCONNECT);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_disconnect *)msg->data;
	p->reason_code = cpu_to_le16(reason_code);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_param(struct sprdwl_priv *priv, u16 rts, u16 frag)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_param *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), SPRDWL_MODE_NONE,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_PARAM);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_set_param *)msg->data;
	p->rts = cpu_to_le16(rts);
	p->frag = cpu_to_le16(frag);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_pmksa(struct sprdwl_priv *priv, u8 vif_mode,
		 const u8 *bssid, const u8 *pmkid, u8 type)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_pmkid *p;
	u8 *sub_cmd;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + sizeof(*sub_cmd), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_PMKSA);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = type;
	p = (struct sprdwl_cmd_pmkid *)(++sub_cmd);

	if (bssid)
		memcpy(p->bssid, bssid, sizeof(p->bssid));
	if (pmkid)
		memcpy(p->pmkid, pmkid, sizeof(p->pmkid));

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_qos_map(struct sprdwl_priv *priv, u8 vif_mode, void *qos_map)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_qos_map *p;

	msg =
	    sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, 1,
			      WIFI_CMD_SET_QOS_MAP);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_qos_map *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));
	memcpy((u8 *)p, qos_map, sizeof(*p));

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_gscan_config(struct sprdwl_priv *priv, u8 vif_mode,
			    void *data, u16 len, u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;
	struct sprd_cmd_gscan_header *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + len,
				vif_mode, 1, WIFI_CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	p->subcmd = SPRDWL_GSCAN_SUBCMD_SET_CONFIG;
	p->data_len = len;
	memcpy(p->data, data, len);
	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_set_gscan_scan_config(struct sprdwl_priv *priv, u8 vif_mode,
				 void *data, u16 len, u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;
	struct sprd_cmd_gscan_header *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + len,
				vif_mode, 1, WIFI_CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_cmd_gscan_header *)(msg->skb->data +
					     sizeof(struct sprdwl_cmd_hdr));
	p->subcmd = SPRDWL_GSCAN_SUBCMD_SET_SCAN_CONFIG;
	p->data_len = len;
	memcpy(p->data, data, len);
	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_enable_gscan(struct sprdwl_priv *priv, u8 vif_mode, void *data,
			u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;
	struct sprd_cmd_gscan_header *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + sizeof(int),
				vif_mode, 1, WIFI_CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	p->subcmd = SPRDWL_GSCAN_SUBCMD_ENABLE_GSCAN;
	p->data_len = sizeof(int);
	memcpy(p->data, data, p->data_len);
	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_get_gscan_capabilities(struct sprdwl_priv *priv, u8 vif_mode,
				  u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;
	struct sprd_cmd_gscan_header *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	p->subcmd = SPRDWL_GSCAN_SUBCMD_GET_CAPABILITIES;
	p->data_len = 0;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_get_gscan_channel_list(struct sprdwl_priv *priv, u8 vif_mode,
				  void *data, u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;
	int *band;
	struct sprd_cmd_gscan_header *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p)+sizeof(*band), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	p->subcmd = SPRDWL_GSCAN_SUBCMD_GET_CHANNEL_LIST;
	p->data_len = sizeof(*band);

	band = (int *)(msg->skb->data + sizeof(struct sprdwl_cmd_hdr) +
		       sizeof(struct sprd_cmd_gscan_header));

	*band = *((int *)data);
	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_add_tx_ts(struct sprdwl_priv *priv, u8 vif_mode, u8 tsid,
		     const u8 *peer, u8 user_prio, u16 admitted_time)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_tx_ts *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, 1,
				WIFI_CMD_ADD_TX_TS);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_tx_ts *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));

	p->tsid = tsid;
	memcpy(p->peer, peer, ETH_ALEN);
	p->user_prio = user_prio;
	p->admitted_time = cpu_to_le16(admitted_time);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_del_tx_ts(struct sprdwl_priv *priv, u8 vif_mode, u8 tsid,
		     const u8 *peer)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_tx_ts *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, 1,
				WIFI_CMD_DEL_TX_TS);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_tx_ts *)
	    (msg->skb->data + sizeof(struct sprdwl_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));

	p->tsid = tsid;
	memcpy(p->peer, peer, ETH_ALEN);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_remain_chan(struct sprdwl_priv *priv, u8 vif_mode,
		       struct ieee80211_channel *channel,
		       enum nl80211_channel_type channel_type,
		       u32 duration, u64 *cookie)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_remain_chan *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_REMAIN_CHAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_remain_chan *)msg->data;
	p->chan = ieee80211_frequency_to_channel(channel->center_freq);
	p->chan_type = channel_type;
	p->duraion = cpu_to_le32(duration);
	p->cookie = cpu_to_le64(*cookie);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_cancel_remain_chan(struct sprdwl_priv *priv, u8 vif_mode, u64 cookie)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_cancel_remain_chan *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_CANCEL_REMAIN_CHAN);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_cancel_remain_chan *)msg->data;
	p->cookie = cpu_to_le64(cookie);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_tx_mgmt(struct sprdwl_priv *priv, u8 vif_mode, u8 channel,
		   u8 dont_wait_for_ack, u32 wait, u64 *cookie,
		   const u8 *buf, size_t len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_mgmt_tx *p;
	u16 datalen = sizeof(*p) + len;

	msg = sprdwl_cmd_getbuf(priv, datalen, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_TX_MGMT);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_mgmt_tx *)msg->data;

	p->chan = channel;
	p->dont_wait_for_ack = dont_wait_for_ack;
	p->wait = cpu_to_le32(wait);
	if (cookie)
		p->cookie = cpu_to_le64(*cookie);
	p->len = cpu_to_le16(len);
	memcpy(p->value, buf, len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_register_frame(struct sprdwl_priv *priv, u8 vif_mode,
			  u16 type, u8 reg)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_register_frame *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_REGISTER_FRAME);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_register_frame *)msg->data;
	p->type = type;
	p->reg = reg;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_cqm_rssi(struct sprdwl_priv *priv, u8 vif_mode,
			s32 rssi_thold, u32 rssi_hyst)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_cqm_rssi *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_CQM);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_cqm_rssi *)msg->data;
	p->rssih = cpu_to_le32(rssi_thold);
	p->rssil = cpu_to_le32(rssi_hyst);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_roam_offload(struct sprdwl_priv *priv, u8 vif_mode,
			    u8 sub_type, const u8 *data, u8 len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_roam_offload_data *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + len, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_SET_ROAM_OFFLOAD);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_roam_offload_data *)msg->data;
	p->type = sub_type;
	p->len = len;
	memcpy(p->value, data, len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_tdls_mgmt(struct sprdwl_vif *vif, struct sk_buff *skb)
{
	struct sprdwl_msg_buf *msg;
	u8 type;
	u8 ret;

	msg = sprdwl_intf_get_msg_buf(vif->priv, SPRDWL_TYPE_DATA, vif->mode);
	if (!msg) {
		if (vif->priv->hw_type == SPRDWL_HW_SDIO_BA)
			sprdwl_stop_net(vif);
		vif->ndev->stats.tx_fifo_errors++;
		return NETDEV_TX_BUSY;
	}
	type = SPRDWL_DATA_TYPE_NORMAL;
	ret = sprdwl_send_data(vif, msg, skb, type, 2);
	if (ret) {
		wiphy_err(vif->priv->wiphy, "%s drop msg due to TX Err\n",
			  __func__);
		goto out;
	}

	vif->ndev->stats.tx_bytes += skb->len;
	vif->ndev->stats.tx_packets++;
	vif->ndev->trans_start = jiffies;

	print_hex_dump_debug("TX packet: ", DUMP_PREFIX_OFFSET,
			     16, 1, skb->data, skb->len, 0);
out:
	return ret;
}

int sprdwl_tdls_oper(struct sprdwl_priv *priv, u8 vif_mode, const u8 *peer,
		     int oper)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_tdls *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_tdls *)msg->data;
	if (peer)
		memcpy(p->da, peer, ETH_ALEN);
	p->tdls_sub_cmd_mgmt = oper;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_start_tdls_channel_switch(struct sprdwl_priv *priv, u8 vif_mode,
				     const u8 *peer_mac, u8 primary_chan,
				     u8 second_chan_offset, u8 band)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_tdls *p;
	struct sprdwl_cmd_tdls_channel_switch chan_switch;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + sizeof(chan_switch),
				vif_mode, SPRDWL_HEAD_RSP, WIFI_CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_tdls *)msg->data;
	p->tdls_sub_cmd_mgmt = SPRDWL_TDLS_START_CHANNEL_SWITCH;
	if (peer_mac)
		memcpy(p->da, peer_mac, ETH_ALEN);
	p->initiator = 1;
	chan_switch.primary_chan = primary_chan;
	chan_switch.second_chan_offset = second_chan_offset;
	chan_switch.band = band;
	p->paylen = sizeof(chan_switch);
	memcpy(p->payload, &chan_switch, p->paylen);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_cancel_tdls_channel_switch(struct sprdwl_priv *priv, u8 vif_mode,
				      const u8 *peer_mac)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_tdls *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_tdls *)msg->data;
	p->tdls_sub_cmd_mgmt = SPRDWL_TDLS_CANCEL_CHANNEL_SWITCH;
	if (peer_mac)
		memcpy(p->da, peer_mac, ETH_ALEN);
	p->initiator = 1;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_notify_ip(struct sprdwl_priv *priv, u8 vif_mode, u8 ip_type,
		     u8 *ip_addr)
{
	struct sprdwl_msg_buf *msg;
	u8 *ip_value;
	u8 ip_len;

	if (ip_type != SPRDWL_IPV4 && ip_type != SPRDWL_IPV6)
		return -EINVAL;
	ip_len = (ip_type == SPRDWL_IPV4) ?
	    SPRDWL_IPV4_ADDR_LEN : SPRDWL_IPV6_ADDR_LEN;
	msg = sprdwl_cmd_getbuf(priv, ip_len, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_NOTIFY_IP_ACQUIRED);
	if (!msg)
		return -ENOMEM;
	ip_value = (unsigned char *)msg->data;
	memcpy(ip_value, ip_addr, ip_len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_blacklist(struct sprdwl_priv *priv,
			 u8 vif_mode, u8 sub_type, u8 num, u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_blacklist *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + num * ETH_ALEN,
				vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_SET_BLACKLIST);
	if (!msg)
		return -ENOMEM;

	p = (struct sprdwl_cmd_blacklist *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_whitelist(struct sprdwl_priv *priv, u8 vif_mode,
			 u8 sub_type, u8 num, u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_mac_addr *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + num * ETH_ALEN,
				vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_SET_WHITELIST);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_set_mac_addr *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_set_mc_filter(struct sprdwl_priv *priv,  u8 vif_mode,
			 u8 sub_type, u8 num, u8 *mac_addr)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_set_mac_addr *p;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p) + num * ETH_ALEN,
				vif_mode, SPRDWL_HEAD_RSP,
				WIFI_CMD_MULTICAST_FILTER);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_set_mac_addr *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (num && mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, NULL, NULL);
}

int sprdwl_npi_send_recv(struct sprdwl_priv *priv, u8 vif_mode, u8 *s_buf,
			 u16 s_len, u8 *r_buf, u16 *r_len)
{
	struct sprdwl_msg_buf *msg;

	msg = sprdwl_cmd_getbuf(priv, s_len, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_NPI_MSG);
	if (!msg)
		return -ENOMEM;
	memcpy(msg->data, s_buf, s_len);

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf, r_len);
}

int sprdwl_set_11v_feature_support(struct sprdwl_priv *priv,
				   u8 vif_mode, u16 val)
{
	struct sprdwl_msg_buf *msg = NULL;
	struct sprdwl_cmd_rsp_state_code state;
	struct sprdwl_cmd_11v *p = NULL;
	u16 rlen = sizeof(state);

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, 1, WIFI_CMD_11V);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_11v *)msg->data;

	p->cmd = SPRDWL_SUBCMD_SET;
	p->value = (val << 16) | val;
	/* len  only 8 =  cmd(2) + len(2) +value(4)*/
	p->len = 8;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT,
				    (u8 *)&state, &rlen);
}

int sprdwl_set_11v_sleep_mode(struct sprdwl_priv *priv, u8 vif_mode,
			      u8 status, u16 interval)
{
	struct sprdwl_msg_buf *msg = NULL;
	struct sprdwl_cmd_rsp_state_code state;
	struct sprdwl_cmd_11v *p = NULL;
	u16 rlen = sizeof(state);
	u32 value = 0;

	msg = sprdwl_cmd_getbuf(priv, sizeof(*p), vif_mode, 1, WIFI_CMD_11V);
	if (!msg)
		return -ENOMEM;
	p = (struct sprdwl_cmd_11v *)msg->data;

	p->cmd = SPRDWL_SUBCMD_ENABLE;
	/* 24-31 feature 16-23 status 0-15 interval */
	value = SPRDWL_11V_SLEEP << 8;
	value = (value | status) << 16;
	value = value | interval;
	p->value = value;
	/* len  only 8 =  cmd(2) + len(2) +value(4)*/
	p->len = 8;

	return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT,
				    (u8 *)&state, &rlen);
}

int sprdwl_xmit_data2mgmt(struct sk_buff *skb, struct net_device *ndev)
{
	/*
	 * default set channel to 0
	 * frequency information:
	 * GC/STA: wdev->current_bss->pub.channel
	 * GO/SotfAP: wdev->channel
	 */
	u8 channel = 0;
	int ret;
	struct ethhdr ehdr;
	struct ieee80211_hdr_3addr *hdr;
	struct sprdwl_llc_hdr *llc;
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sk_buff *tmp = skb;
	unsigned int extra =
		sizeof(struct ieee80211_hdr_3addr) +
		sizeof(struct sprdwl_llc_hdr) -
		sizeof(struct ethhdr);

	memcpy(&ehdr, skb->data, sizeof(struct ethhdr));
	/* 802.3 to 802.11 */
	skb = skb_realloc_headroom(tmp, extra);
	dev_kfree_skb(tmp);
	if (skb == NULL) {
		netdev_err(ndev, "%s realloc failed\n", __func__);
		return NETDEV_TX_BUSY;
	}
	skb_push(skb, extra);

	hdr = (struct ieee80211_hdr_3addr *)skb->data;
	/* data type:to ds */
	hdr->frame_control = 0x0208;
	hdr->duration_id = 0x00;
	memcpy(hdr->addr1, ehdr.h_dest, ETH_ALEN);
	memcpy(hdr->addr2, ehdr.h_source, ETH_ALEN);
	memcpy(hdr->addr3, ehdr.h_source, ETH_ALEN);
	hdr->seq_ctrl = 0x00;

	llc = (struct sprdwl_llc_hdr *)
		 (((u8 *)skb->data) + sizeof(struct ieee80211_hdr_3addr));
	llc->dsap = 0xAA;
	llc->ssap = 0xAA;
	llc->cntl = 0x03;
	memset(llc->org_code, 0x0, sizeof(llc->org_code));
	llc->eth_type = ehdr.h_proto;
	/* send 80211 Eap failure frame */
	ret = sprdwl_tx_mgmt(vif->priv, vif->mode, channel, 1, 0, NULL,
			     skb->data, skb->len);
	if (ret) {
		dev_kfree_skb(skb);
		netdev_err(ndev, "%s send failed\n", __func__);
		return NETDEV_TX_BUSY;
	}
	vif->ndev->stats.tx_bytes += skb->len;
	vif->ndev->stats.tx_packets++;
	ndev->trans_start = jiffies;
	dev_kfree_skb(skb);

	netdev_info(ndev, "%s send successfully\n", __func__);
	print_hex_dump_debug("TX packet: ", DUMP_PREFIX_OFFSET,
			     16, 1, skb->data, skb->len, 0);

	return NETDEV_TX_OK;
}

/* retrun the msg length or 0 */
unsigned short sprdwl_rx_rsp_process(struct sprdwl_priv *priv, u8 *msg)
{
	u8 mode;
	u16 plen;
	void *data;
	struct sprdwl_cmd *cmd = &g_sprdwl_cmd;
	struct sprdwl_cmd_hdr *hdr;

	if (unlikely(!cmd->init_ok)) {
		pr_info("%s cmd coming too early, drop it\n", __func__);
		return 0;
	}

	hdr = (struct sprdwl_cmd_hdr *)msg;
	mode = hdr->common.mode;
	plen = SPRDWL_GET_LE16(hdr->plen);

#ifdef DUMP_COMMAND_RESPONSE
	print_hex_dump(KERN_DEBUG, "CMD RSP: ", DUMP_PREFIX_OFFSET, 16, 1,
		       ((u8 *)hdr + sizeof(*hdr)), hdr->plen - sizeof(*hdr), 0);
#endif
	/* 2048 use mac */
	if (mode > SPRDWL_MODE_MAX || hdr->cmd_id > WIFI_CMD_MAX ||
	    plen > 2048) {
		pr_err("%s wrong CMD_RSP: %d\n", __func__, (int)hdr->cmd_id);
		return 0;
	}
	if (atomic_inc_return(&cmd->refcnt) >= SPRDWL_CMD_EXIT_VAL) {
		atomic_dec(&cmd->refcnt);
		return 0;
	}
	data = kmalloc(plen, GFP_KERNEL);
	if (!data) {
		atomic_dec(&cmd->refcnt);
		return plen;
	}
	memcpy(data, (void *)hdr, plen);

	spin_lock_bh(&cmd->lock);
	if (!cmd->data && SPRDWL_GET_LE32(hdr->mstime) == cmd->mstime &&
	    hdr->cmd_id == cmd->cmd_id) {
		wiphy_info(priv->wiphy, "mode %d recv rsp[%s]\n",
			   (int)mode, cmd2str(hdr->cmd_id));
		cmd->data = data;
		wake_up(&cmd->waitq);
	} else {
		kfree(data);
		pr_err("%s mode %d recv mismatched rsp[%s] mstime:[%u %u]\n",
		       __func__, (int)mode, cmd2str(hdr->cmd_id),
		       SPRDWL_GET_LE32(hdr->mstime), cmd->mstime);
	}
	spin_unlock_bh(&cmd->lock);
	atomic_dec(&cmd->refcnt);

	return plen;
}

/* Events */
void sprdwl_event_station(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_new_station *sta =
	    (struct sprdwl_event_new_station *)data;

	sprdwl_report_softap(vif, sta->is_connect,
			     sta->mac, sta->ie, sta->ie_len);
}

void sprdwl_event_scan_done(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_scan_done *p =
	    (struct sprdwl_event_scan_done *)data;
	u8 bucket_id = 0;

	switch (p->type) {
	case SPRDWL_SCAN_DONE:
		sprdwl_scan_done(vif, false);
		netdev_info(vif->ndev, "%s got %d BSSes\n", __func__,
			    bss_count);
		break;
	case SPRDWL_SCHED_SCAN_DONE:
		sprdwl_sched_scan_done(vif, false);
		netdev_info(vif->ndev, "%s schedule scan got %d BSSes\n",
			    __func__, bss_count);
		break;
	case SPRDWL_GSCAN_DONE:
		bucket_id = ((struct sprdwl_event_gscan_done *)data)->bucket_id;
		sprdwl_gscan_done(vif, bucket_id);
		netdev_info(vif->ndev, "%s gscan got %d bucketid done\n",
			    __func__, bucket_id);
		break;
	case SPRDWL_SCAN_ERROR:
	default:
		sprdwl_scan_done(vif, true);
		sprdwl_sched_scan_done(vif, false);
		if (p->type == SPRDWL_SCAN_ERROR)
			netdev_err(vif->ndev, "%s error!\n", __func__);
		else
			netdev_err(vif->ndev, "%s invalid scan done type: %d\n",
				   __func__, p->type);
		break;
	}
	bss_count = 0;
}

void sprdwl_event_connect(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	u8 *bssid = NULL, *req_ie = NULL, *resp_ie = NULL, *bea_ie = NULL;
	u8 *pos = data;
	u8 status_code, channel_num = 0;
	u16 resp_ie_len = 0, bea_ie_len = 0, req_ie_len = 0;
	s8 signal = 0;
	int left = len;

	/* the first byte is status code */
	memcpy(&status_code, pos, sizeof(status_code));
	if (status_code != SPRDWL_CONNECT_SUCCESS &&
	    status_code != SPRDWL_ROAM_SUCCESS)
		goto out;
	pos += sizeof(status_code);
	left -= sizeof(status_code);

	/* parse BSSID */
	if (left < ETH_ALEN)
		goto out;
	bssid = pos;
	pos += ETH_ALEN;
	left -= ETH_ALEN;

	/* get channel */
	if (left < sizeof(channel_num))
		goto out;
	memcpy(&channel_num, pos, sizeof(channel_num));
	pos += sizeof(channel_num);
	left -= sizeof(channel_num);

	/* get signal */
	if (left < sizeof(signal))
		goto out;
	memcpy(&signal, pos, sizeof(signal));
	pos += sizeof(signal);
	left -= sizeof(signal);

	/* parse REQ IE */
	if (left < req_ie_len)
		goto out;
	memcpy(&req_ie_len, pos, sizeof(req_ie_len));
	pos += sizeof(req_ie_len);
	left -= sizeof(req_ie_len);
	req_ie = pos;
	pos += req_ie_len;
	left -= req_ie_len;

	/* parse RESP IE */
	if (left < resp_ie_len)
		goto out;
	memcpy(&resp_ie_len, pos, sizeof(resp_ie_len));
	pos += sizeof(resp_ie_len);
	left -= sizeof(resp_ie_len);
	resp_ie = pos;
	pos += resp_ie_len;
	left -= resp_ie_len;

	/* parse BEA IE */
	if (left < bea_ie_len)
		goto out;
	memcpy(&bea_ie_len, pos, sizeof(bea_ie_len));
	pos += sizeof(bea_ie_len);
	left -= sizeof(bea_ie_len);
	bea_ie = pos;
out:
	sprdwl_report_connection(vif, bssid, channel_num, signal, bea_ie,
				 bea_ie_len, req_ie, req_ie_len, resp_ie,
				 resp_ie_len, status_code);
}

void sprdwl_event_disconnect(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	u16 reason_code;

	memcpy(&reason_code, data, sizeof(reason_code));
	sprdwl_report_disconnection(vif, reason_code);
}

void sprdwl_event_mic_failure(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_mic_failure *mic_failure =
	    (struct sprdwl_event_mic_failure *)data;

	sprdwl_report_mic_failure(vif, mic_failure->is_mcast,
				  mic_failure->key_id);
}

void sprdwl_event_remain_on_channel_expired(struct sprdwl_vif *vif,
					    u8 *data, u16 len)
{
	sprdwl_report_remain_on_channel_expired(vif);
}

void sprdwl_event_mlme_tx_status(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_mgmt_tx_status *tx_status =
	    (struct sprdwl_event_mgmt_tx_status *)data;

	sprdwl_report_mgmt_tx_status(vif, SPRDWL_GET_LE64(tx_status->cookie),
				     tx_status->buf,
				     SPRDWL_GET_LE16(tx_status->len),
				     tx_status->ack);
}

void sprdwl_event_frame(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_mgmt_frame *frame =
	    (struct sprdwl_event_mgmt_frame *)data;
	u16 buf_len;
	u8 *buf = NULL;
	u8 channel, type;

	channel = frame->channel;
	type = frame->type;
	buf = frame->data;
	buf_len = SPRDWL_GET_LE16(frame->len);

	sprdwl_cfg80211_dump_frame_prot_info(0, 0, buf, buf_len);

	switch (type) {
	case SPRDWL_FRAME_NORMAL:
		sprdwl_report_rx_mgmt(vif, channel, buf, buf_len);
		break;
	case SPRDWL_FRAME_DEAUTH:
		sprdwl_report_mgmt_deauth(vif, buf, buf_len);
		break;
	case SPRDWL_FRAME_DISASSOC:
		sprdwl_report_mgmt_disassoc(vif, buf, buf_len);
		break;
	case SPRDWL_FRAME_SCAN:
		sprdwl_report_scan_result(vif, channel, frame->signal,
					  buf, buf_len);
		++bss_count;
		break;
	default:
		netdev_err(vif->ndev, "%s invalid frame type: %d!\n",
			   __func__, type);
		break;
	}
}

void sprdwl_event_gscan_frame(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	u32 report_event;
	u8 *pos = data;
	s32 avail_len = len;
	struct sprdwl_event_mgmt_frame *frame;
	u16 buf_len;
	u8 *buf = NULL;
	u8 channel, type, bucket_id;

	report_event = *(u32 *)pos;
	avail_len -= sizeof(u32);
	pos += sizeof(u32);
	while (avail_len > 0) {
		if (avail_len < sizeof(struct sprdwl_event_mgmt_frame)) {
			netdev_err(vif->ndev,
				   "%s invalid available length: %d!\n",
				   __func__, avail_len);
			break;
		}
		frame = (struct sprdwl_event_mgmt_frame *)pos;
		channel = frame->channel;
		type = frame->type;
		bucket_id = frame->reserved;
		buf = frame->data;
		buf_len = SPRDWL_GET_LE16(frame->len);
		sprdwl_cfg80211_dump_frame_prot_info(0, 0, buf, buf_len);
		sprdwl_report_gscan_result(vif, report_event,
					   bucket_id, channel,
					   frame->signal, buf, buf_len);
		avail_len -= sizeof(struct sprdwl_event_mgmt_frame) + buf_len;
		pos += sizeof(struct sprdwl_event_mgmt_frame) + buf_len;
		netdev_info(vif->ndev, "%s ch:%d ty:%d id:%d len:%d aval:%d\n",
			    __func__, channel, type, bucket_id, buf_len,
			    avail_len);
	}
	if (report_event & REPORT_EVENTS_EACH_SCAN)
		sprdwl_available_event(vif);

	if (report_event == REPORT_EVENTS_BUFFER_FULL)
		sprdwl_buffer_full_event(vif);
}

void sprdwl_event_cqm(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_event_cqm *p;
	u8 rssi_event;

	p = (struct sprdwl_event_cqm *)data;
	switch (p->status) {
	case SPRDWL_CQM_RSSI_LOW:
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		break;
	case SPRDWL_CQM_RSSI_HIGH:
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH;
		break;
	case SPRDWL_CQM_BEACON_LOSS:
		/* TODO wpa_supplicant not support the event ,
		 * so we workaround this issue
		 */
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		vif->beacon_loss = 1;
		break;
	default:
		netdev_err(vif->ndev, "%s invalid event!\n", __func__);
		return;
	}

	sprdwl_report_cqm(vif, rssi_event);
}

void sprdwl_event_tdls(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	unsigned char *peer;
	u8 oper;
	u16 reason_code;
	struct sprdwl_event_tdls *report_tdls = NULL;

	report_tdls = (struct sprdwl_event_tdls *)data;
	peer = kzalloc(ETH_ALEN, GFP_KERNEL);
	memcpy(peer, &report_tdls->mac[0], ETH_ALEN);
	oper = report_tdls->tdls_sub_cmd_mgmt;
	if (SPRDWL_TDLS_TEARDOWN == oper)
		oper = NL80211_TDLS_TEARDOWN;
	else
		oper = NL80211_TDLS_SETUP;
	reason_code = 0;
	sprdwl_report_tdls(vif, peer, oper, reason_code);
}

static const char *evt2str(u8 evt)
{
#define E2S(x) case x: return #x;
	switch (evt) {
	E2S(WIFI_EVENT_CONNECT)
	E2S(WIFI_EVENT_DISCONNECT)
	E2S(WIFI_EVENT_SCAN_DONE)
	E2S(WIFI_EVENT_MGMT_FRAME)
	E2S(WIFI_EVENT_MGMT_TX_STATUS)
	E2S(WIFI_EVENT_REMAIN_CHAN_EXPIRED)
	E2S(WIFI_EVENT_MIC_FAIL)
	E2S(WIFI_EVENT_NEW_STATION)
	E2S(WIFI_EVENT_CQM)
	E2S(WIFI_EVENT_MEASUREMENT)
	E2S(WIFI_EVENT_TDLS)
	E2S(WIFI_EVENT_SDIO_SEQ_NUM)
	E2S(WIFI_EVENT_SDIO_FLOWCON)
	default : return "WIFI_EVENT_UNKNOWN";
	}
#undef E2S
}

/* retrun the msg length or 0 */
unsigned short sprdwl_rx_event_process(struct sprdwl_priv *priv, u8 *msg)
{
	struct sprdwl_cmd_hdr *hdr = (struct sprdwl_cmd_hdr *)msg;
	struct sprdwl_vif *vif;
	u8 mode;
	u16 len, plen;
	u8 *data;

	mode = hdr->common.mode;
	if (mode > SPRDWL_MODE_MAX) {
		wiphy_info(priv->wiphy, "%s invalid mode: %d\n", __func__,
			   mode);
		return 0;
	}

	plen = SPRDWL_GET_LE16(hdr->plen);
	if (!priv) {
		pr_err("%s priv is NULL [%u]mode %d recv[%s]len: %d\n",
		       __func__, le32_to_cpu(hdr->mstime), mode,
		       evt2str(hdr->cmd_id), hdr->plen);
		return plen;
	}

	wiphy_info(priv->wiphy, "[%u]mode %d recv[%s]len: %d\n",
		   le32_to_cpu(hdr->mstime), mode, evt2str(hdr->cmd_id), plen);

	print_hex_dump_debug("EVENT: ", DUMP_PREFIX_OFFSET, 16, 1,
			     ((u8 *)hdr + sizeof(*hdr)),
			     hdr->plen - sizeof(*hdr), 0);

	len = plen - sizeof(*hdr);
	vif = mode_to_vif(priv, mode);
	if (!vif) {
		wiphy_info(priv->wiphy, "%s NULL vif for mode: %d, len:%d\n",
			   __func__, mode, plen);
		return plen;
	}

	if (priv->hw_type == SPRDWL_HW_SDIO) {
		/* fixme temp use memcpy here */
		data = kmalloc(len, GFP_KERNEL);
		if (!data) {
			sprdwl_put_vif(vif);
			return plen;
		}
		memcpy(data, msg + sizeof(*hdr), len);
	} else {
		/* SPRDWL_HW_SIPC SPRDWL_HW_SDIO_BA*/
		data = (u8 *)msg;
		data += sizeof(*hdr);
	}

	switch (hdr->cmd_id) {
	case WIFI_EVENT_CONNECT:
		sprdwl_event_connect(vif, data, len);
		break;
	case WIFI_EVENT_DISCONNECT:
		sprdwl_event_disconnect(vif, data, len);
		break;
	case WIFI_EVENT_REMAIN_CHAN_EXPIRED:
		sprdwl_event_remain_on_channel_expired(vif, data, len);
		break;
	case WIFI_EVENT_NEW_STATION:
		sprdwl_event_station(vif, data, len);
		break;
	case WIFI_EVENT_MGMT_FRAME:
		sprdwl_event_frame(vif, data, len);
		break;
	case WIFI_EVENT_GSCAN_FRAME:
		sprdwl_event_gscan_frame(vif, data, len);
		break;
	case WIFI_EVENT_SCAN_DONE:
		sprdwl_event_scan_done(vif, data, len);
		break;
	case WIFI_EVENT_SDIO_SEQ_NUM:
		break;
	case WIFI_EVENT_MIC_FAIL:
		sprdwl_event_mic_failure(vif, data, len);
		break;
	case WIFI_EVENT_CQM:
		sprdwl_event_cqm(vif, data, len);
		break;
	case WIFI_EVENT_MGMT_TX_STATUS:
		sprdwl_event_mlme_tx_status(vif, data, len);
		break;
	case WIFI_EVENT_TDLS:
		sprdwl_event_tdls(vif, data, len);
		break;
	default:
		wiphy_info(priv->wiphy, "unsupported event: %d\n", hdr->cmd_id);
		break;
	}

	sprdwl_put_vif(vif);
	if (priv->hw_type == SPRDWL_HW_SDIO)
		kfree(data);

	return plen;
}
