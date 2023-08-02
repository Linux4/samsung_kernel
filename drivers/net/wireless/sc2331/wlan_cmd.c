/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owner:
 *      jinglong.chen
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

#include "wlan_common.h"
#include "wlan_cmd.h"
#include "wlan_cfg80211.h"
#define  CMD_WAIT_TIMEOUT  (3000)
#define  CMD_ITEM(CMD)    { CMD,  (#CMD) }
typedef struct {
	unsigned short id;
	char *cmd;
} cmd_t;

static cmd_t g_cmd_table[] = {
	CMD_ITEM(WIFI_CMD_GET_MODE),
	CMD_ITEM(WIFI_CMD_GET_RSSI),
	CMD_ITEM(WIFI_CMD_GET_TXRATE_TXFAILED),
	CMD_ITEM(WIFI_CMD_SET_SCAN),
	CMD_ITEM(WIFI_CMD_SET_AUTH_TYPE),
	CMD_ITEM(WIFI_CMD_SET_WPA_VERSION),
	CMD_ITEM(WIFI_CMD_SET_PAIRWISE_CIPHER),
	CMD_ITEM(WIFI_CMD_SET_GROUP_CIPHER),
	CMD_ITEM(WIFI_CMD_SET_AKM_SUITE),
	CMD_ITEM(WIFI_CMD_SET_CHANNEL),
	CMD_ITEM(WIFI_CMD_SET_BSSID),
	CMD_ITEM(WIFI_CMD_SET_ESSID),
	CMD_ITEM(WIFI_CMD_KEY_ADD),
	CMD_ITEM(WIFI_CMD_KEY_DEL),
	CMD_ITEM(WIFI_CMD_KEY_SET),
	CMD_ITEM(WIFI_CMD_SET_DISCONNECT),
	CMD_ITEM(WIFI_CMD_SET_RTS_THRESHOLD),
	CMD_ITEM(WIFI_CMD_SET_FRAG_THRESHOLD),
	CMD_ITEM(WIFI_CMD_SET_PMKSA),
	CMD_ITEM(WIFI_CMD_DEL_PMKSA),
	CMD_ITEM(WIFI_CMD_FLUSH_PMKSA),
	CMD_ITEM(WIFI_CMD_SET_DEV_OPEN),
	CMD_ITEM(WIFI_CMD_SET_DEV_CLOSE),
	CMD_ITEM(WIFI_CMD_SET_PSK),
	CMD_ITEM(WIFI_CMD_START_BEACON),
	CMD_ITEM(WIFI_CMD_SET_WPS_IE),
	CMD_ITEM(WIFI_CMD_TX_MGMT),
	CMD_ITEM(WIFI_CMD_REMAIN_CHAN),
	CMD_ITEM(WIFI_CMD_CANCEL_REMAIN_CHAN),
	CMD_ITEM(WIFI_CMD_P2P_IE),
	CMD_ITEM(WIFI_CMD_CHANGE_BEACON),
	CMD_ITEM(WIFI_CMD_REGISTER_FRAME),
	CMD_ITEM(WIFI_CMD_NPI_MSG),
	CMD_ITEM(WIFI_CMD_SET_FT_IE),
	CMD_ITEM(WIFI_EVENT_CONNECT),
	CMD_ITEM(WIFI_EVENT_DISCONNECT),
	CMD_ITEM(WIFI_EVENT_SCANDONE),
	CMD_ITEM(WIFI_EVENT_MGMT_DEAUTH),
	CMD_ITEM(WIFI_EVENT_MGMT_DISASSOC),
	CMD_ITEM(WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED),
	CMD_ITEM(WIFI_EVENT_NEW_STATION),
	CMD_ITEM(WIFI_EVENT_REPORT_FRAME),
	CMD_ITEM(WIFI_EVENT_CONNECT_AP),
	CMD_ITEM(WIFI_CMD_ASSERT),
	CMD_ITEM(WIFI_EVENT_SDIO_SEQ_NUM),
	CMD_ITEM(WIFI_CMD_MAX),
	CMD_ITEM(WIFI_EVENT_REPORT_SCAN_FRAME),
	CMD_ITEM(WIFI_CMD_SLEEP),
	CMD_ITEM(WIFI_CMD_GET_IP),
	CMD_ITEM(WIFI_EVENT_REPORT_MIC_FAIL),
	CMD_ITEM(WIFI_CMD_REQ_LTE_CONCUR),
	CMD_ITEM(WIFI_CMD_SCAN_NOR_CHANNELS),
	CMD_ITEM(WIFI_EVENT_REPORT_CQM_RSSI_LOW),
	CMD_ITEM(WIFI_EVENT_REPORT_CQM_RSSI_HIGH),
	CMD_ITEM(WIFI_EVENT_REPORT_CQM_RSSI_LOSS_BEACON),
	CMD_ITEM(WIFI_EVENT_MLME_TX_STATUS),
	CMD_ITEM(WIFI_EVENT_REPORT_VERSION),
};

char *get_cmd_name(int id)
{
	int i;
	int id_num = sizeof(g_cmd_table) / sizeof(cmd_t);
	for (i = 0; i < id_num; i++) {
		if (id == g_cmd_table[i].id) {
			return g_cmd_table[i].cmd;
		}
	}
	return "NULL";
}

#define WLAN_CMD_MEM_LEN	(8 * 1024)
int wlan_cmd_init(void)
{
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	cmd->mem = kmalloc(WLAN_CMD_MEM_LEN, GFP_KERNEL);
	mutex_init(&(cmd->cmd_lock));
	mutex_init(&cmd->mem_lock);
	init_waitqueue_head(&(cmd->waitQ));
	cmd->wakeup = 0;
	return OK;
}

int wlan_cmd_deinit(void)
{
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	if (NULL != cmd->mem) {
		kfree(cmd->mem);
		cmd->mem = NULL;
	}
	mutex_destroy(&(cmd->cmd_lock));
	return OK;

}

static inline void wlan_cmd_clean(void)
{
	wlan_cmd_t *cmd = &g_wlan.cmd;
	r_msg_hdr_t *msg;
	mutex_lock(&cmd->mem_lock);
	if (cmd->wakeup) {
		msg = (r_msg_hdr_t *) cmd->mem;
		printke("%s drop msg [%d][%s][%d]\n",
			__func__, msg->mode,
			get_cmd_name(msg->subtype), msg->len);
		cmd->wakeup = 0;
	}
	mutex_unlock(&cmd->mem_lock);
}

int wlan_cmd_send_to_ic(unsigned char vif_id, unsigned char *data, int len,
			int subtype)
{
	int ret;
	tx_msg_t msg = { 0 };
	msg_q_t *msg_q = &(g_wlan.netif[vif_id].msg_q[0]);
	wlan_cmd_t *cmd = &(g_wlan.cmd);

	msg.hdr.mode = vif_id;
	msg.hdr.type = HOST_SC2331_CMD;
	msg.hdr.subtype = subtype;
	if ((len == 0) || (data == NULL)) {

		msg.hdr.len = 0;
		msg.slice[0].data = NULL;
		msg.slice[0].len = 0;
	} else {
		msg.hdr.len = len;
		msg.slice[0].data = data;
		msg.slice[0].len = len;
	}
	wlan_cmd_clean();
	ret = msg_q_in(msg_q, &msg);
	if (ret != OK) {
		printkd("[SEND_CMD][%d][%s][ERR][MSG_Q NULL]\n", vif_id,
			get_cmd_name(subtype));
		return ERROR;
	}
	printkd("[SEND_CMD][%d][%s]\n", vif_id, get_cmd_name(subtype));
	g_wlan.wlan_core.need_tx++;
	core_up();
	return OK;
}

int wlan_timeout_recv_rsp(unsigned char *r_buf, unsigned short *r_len,
			  unsigned int timeout)
{
	int ret;
	r_msg_hdr_t *msg;
	wlan_cmd_t  *cmd = &(g_wlan.cmd);

	ret =
	    wait_event_timeout(cmd->waitQ,
			       ((1 == cmd->wakeup)
				|| (1 == g_wlan.sync.exit)),
			       msecs_to_jiffies(timeout));
	if (0 == ret) {
		printke("[%s][%d][err]\n", __func__, __LINE__);
		cmd->wakeup = 0;
		return -1;
	}
	mutex_lock(&cmd->mem_lock);
	cmd->wakeup = 0;
	msg = (r_msg_hdr_t *)(cmd->mem);
	if (*r_len < msg->len) {
		printke("[%s][%d][err]\n", __func__, __LINE__);
		msg->len = *r_len;
	}
	*r_len = msg->len;
	memcpy(r_buf, cmd->mem, sizeof(r_msg_hdr_t) + msg->len);
	mutex_unlock(&cmd->mem_lock);
	return 0;
}

int wlan_cmd_send_recv(unsigned char vif_id, unsigned char *pData, int len,
		       int subtype, int timeout)
{

	int ret = 0;
	r_msg_hdr_t *msg;
	struct wlan_cmd_rsp_state_code *state;
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	unsigned char r_buf[sizeof(r_msg_hdr_t) +
			    sizeof(struct wlan_cmd_rsp_state_code)] = { 0 };
	unsigned short r_len = sizeof(struct wlan_cmd_rsp_state_code);

	msg = (r_msg_hdr_t *)(&r_buf[0]);

	if (1 == g_wlan.sync.exit) {
		printke("%s, exit flag\n", __func__);
		return ERROR;
	}

	mutex_lock(&(cmd->cmd_lock));

	if ((NULL == pData) || (0 == len))
		ret = wlan_cmd_send_to_ic(vif_id, NULL, 0, subtype);
	else
		ret = wlan_cmd_send_to_ic(vif_id, pData, len, subtype);
	if (OK != ret)
		return ERROR;

	ret = wlan_timeout_recv_rsp(r_buf, &r_len, timeout);
	if (-1 == ret) {
		printke("[SEND_CMD %s %d ERROR][rsp timeout]\n",
			get_cmd_name(subtype), vif_id);
	}

	if ((SC2331_HOST_RSP != msg->type) || (subtype != msg->subtype)) {
		printke("[SEND_CMD %s %d ERROR][rsp match %s]\n",
			get_cmd_name(subtype), vif_id,
			get_cmd_name(msg->subtype));
		ret = -1;
	}
	mutex_unlock(&(cmd->cmd_lock));

	if (-1 == ret) {
		wlan_cmd_assert(vif_id, 0);
		return ERROR;
	}
	state = (struct wlan_cmd_rsp_state_code *)(&r_buf[sizeof(r_msg_hdr_t)]);
	if(WIFI_CMD_SET_SCAN == subtype)
		return state->code;
	return OK;
}

int wlan_cmd_start_ap(unsigned char vif_id, unsigned char *beacon,
		      unsigned short len)
{
	int ret;
	unsigned short dataLen;
	struct wlan_cmd_beacon_t *beacon_ptr;
	dataLen = sizeof(struct wlan_cmd_beacon_t) + len;
	beacon_ptr = kmalloc(dataLen, GFP_KERNEL);
	beacon_ptr->len = len;
	memcpy(beacon_ptr->value, beacon, len);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)beacon_ptr, dataLen,
			       WIFI_CMD_START_BEACON, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_disassoc(unsigned char vif_id, const unsigned char *mac_addr,
		     unsigned short reason_code)
{
	int dataLen = 0;
	struct wlan_cmd_disassoc *ptr = NULL;

	dataLen = sizeof(struct wlan_cmd_disassoc);
	ptr = kzalloc(dataLen, GFP_KERNEL);
	if (NULL != mac_addr)
		memcpy(&(ptr->mac[0]), mac_addr, 6);
	ptr->reason_code = reason_code;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_DISASSOC, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_register_frame(unsigned char vif_id,
			    struct wlan_cmd_register_frame_t *data)
{
	int ret;
	struct wlan_cmd_register_frame_t *ptr;
	unsigned short dataLen;
	dataLen = sizeof(struct wlan_cmd_register_frame_t);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memcpy((unsigned char *)ptr, (unsigned char *)(data), dataLen);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			       WIFI_CMD_REGISTER_FRAME, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_set_p2p_ie(unsigned char vif_id, u8 type, const u8 *ie, u16 len)
{
	struct wlan_cmd_p2p_ie_t *p2p_ptr;
	int ret;
	unsigned short dataLen;

	if (type != P2P_ASSOC_IE && type != P2P_BEACON_IE &&
	    type != P2P_PROBERESP_IE && type != P2P_ASSOCRESP_IE &&
	    type != P2P_BEACON_IE_HEAD && type != P2P_BEACON_IE_TAIL) {
		printke("%s wrong ie type is %d\n", __func__, type);
		return -EIO;
	}
	printkd("%s type:%d ie_len:%d\n", __func__, type, len);
	dataLen = sizeof(struct wlan_cmd_p2p_ie_t) + len;
	p2p_ptr = kmalloc(dataLen, GFP_KERNEL);
	p2p_ptr->type = type;
	p2p_ptr->len = len;
	memcpy(p2p_ptr->value, ie, len);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)p2p_ptr, dataLen,
			       WIFI_CMD_P2P_IE, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_set_ft_ie(unsigned char vif_id, const unsigned char *ies,
		       unsigned short len)
{
	unsigned char *ptr = NULL;
	unsigned short dataLen = len + 2;

	ptr = kmalloc(dataLen, GFP_KERNEL);
	*((unsigned short *)ptr) = len;
	memcpy(ptr + 2, ies, len);

	hex_dump("ftie:", strlen("ftie:"), ptr, dataLen);
	return wlan_cmd_send_recv(vif_id, ptr, dataLen, WIFI_CMD_SET_FT_IE,
				  CMD_WAIT_TIMEOUT);
}

int wlan_cmd_set_tx_mgmt(unsigned char vif_id,
			struct ieee80211_channel *channel,
			u8 dont_wait_for_ack, unsigned int wait,
			u64 *cookie, const unsigned char *mac,
			size_t mac_len)
{
	unsigned short dataLen;
	struct wlan_cmd_mgmt_tx_t *mgmt_tx;
	int ret;
	unsigned char send_chan;

	dataLen = sizeof(struct wlan_cmd_mgmt_tx_t) + mac_len;
	mgmt_tx = kmalloc(dataLen, GFP_KERNEL);
	send_chan = ieee80211_frequency_to_channel(channel->center_freq);

	mgmt_tx->chan = send_chan;
	mgmt_tx->dont_wait_for_ack = dont_wait_for_ack;
	mgmt_tx->wait = wait;
	mgmt_tx->cookie = *cookie;
	mgmt_tx->len = mac_len;
	memcpy(mgmt_tx->value, mac, mac_len);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)mgmt_tx, dataLen,
			       WIFI_CMD_TX_MGMT, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_remain_chan(unsigned char vif_id,
			 struct ieee80211_channel *channel,
			 enum nl80211_channel_type channel_type,
			 unsigned int duration, u64 *cookie)
{
	int ret;
	unsigned short dataLen;
	struct wlan_cmd_remain_chan_t *remain_chan;

	dataLen = sizeof(struct wlan_cmd_remain_chan_t);
	remain_chan = kmalloc(dataLen, GFP_KERNEL);

	remain_chan->chan =
	    ieee80211_frequency_to_channel(channel->center_freq);;
	remain_chan->chan_type = channel_type;
	remain_chan->duraion = duration;
	remain_chan->cookie = *cookie;

	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)remain_chan, dataLen,
			       WIFI_CMD_REMAIN_CHAN, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_cancel_remain_chan(unsigned char vif_id, u64 cookie)
{

	int ret;
	unsigned short dataLen;

	struct wlan_cmd_cancel_remain_chan_t *ptr;

	dataLen = sizeof(struct wlan_cmd_cancel_remain_chan_t);

	ptr = kmalloc(dataLen, GFP_KERNEL);

	ptr->cookie = cookie;

	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			       WIFI_CMD_CANCEL_REMAIN_CHAN, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_scan(unsigned char vif_id, const unsigned char *ssid,
		  const unsigned char *channels, int len)
{
	int dataLen;
	struct wlan_cmd_scan *ptr = NULL;
	u8 *send = NULL;
	u8 *psend = NULL;
	u8 ch_num = channels[0] + 1;

	dataLen = sizeof(struct wlan_cmd_scan) + len + ch_num;
	psend = send = kmalloc(dataLen, GFP_KERNEL);

	memcpy(send, channels, ch_num);
	send += ch_num;

	ptr = (struct wlan_cmd_scan *)send;
	memcpy(ptr->ssid, ssid, len);
	ptr->len = len;

	hex_dump("scan data ", 10, psend, dataLen);
	wlan_cmd_send_recv(vif_id, (unsigned char *)psend, dataLen,
			   WIFI_CMD_SET_SCAN, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_set_wpa_version(unsigned char vif_id, unsigned int wpa_version)
{
	int dataLen;
	struct wlan_cmd_set_wpa_version *ptr;

	dataLen = sizeof(struct wlan_cmd_set_wpa_version);
	ptr = kmalloc(dataLen, GFP_KERNEL);

	ptr->wpa_version = wpa_version;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_WPA_VERSION, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_set_auth_type(unsigned char vif_id, unsigned int type)
{
	int dataLen;
	struct wlan_cmd_set_auth_type *ptr;

	dataLen = sizeof(struct wlan_cmd_set_auth_type);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->type = type;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_AUTH_TYPE, CMD_WAIT_TIMEOUT);
	return 0;
}

/* unicast cipher or group cipher */
int wlan_cmd_set_cipher(unsigned char vif_id, unsigned int cipher,
			unsigned char cmd_id)
{
	int dataLen;
	struct wlan_cmd_set_cipher *ptr;

	dataLen = sizeof(struct wlan_cmd_set_cipher);
	if ((cmd_id != WIFI_CMD_SET_PAIRWISE_CIPHER)
	    && (cmd_id != WIFI_CMD_SET_GROUP_CIPHER)) {
		printkd("not support this type cipher \n");
		return -EINVAL;
	}
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->cipher = cipher;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen, cmd_id,
			   CMD_WAIT_TIMEOUT);
	return 0;

}

int wlan_cmd_set_key_management(unsigned char vif_id, unsigned char key_mgmt)
{
	int dataLen;
	struct wlan_cmd_set_key_management *ptr;

	dataLen = sizeof(struct wlan_cmd_set_key_management);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->key_mgmt = key_mgmt;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_AKM_SUITE, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_set_psk(unsigned char vif_id, const unsigned char *key,
		     unsigned int key_len)
{
	int dataLen;
	struct wlan_cmd_set_psk *ptr;

	dataLen = sizeof(struct wlan_cmd_set_psk) + key_len;
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->len = key_len;
	memcpy(ptr->key, key, key_len);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_PSK, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_set_channel(unsigned char vif_id, unsigned int channel)
{
	int dataLen;
	struct wlan_cmd_set_channel *ptr;

	dataLen = sizeof(struct wlan_cmd_set_channel);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->channel = channel;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_CHANNEL, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_set_bssid(unsigned char vif_id, const unsigned char *addr)
{
	int dataLen;
	struct wlan_cmd_set_bssid *ptr;

	dataLen = sizeof(struct wlan_cmd_set_bssid);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memcpy(&(ptr->addr[0]), addr, 6);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_BSSID, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_get_ip(unsigned char vif_id, u8 *ip)
{
	int dataLen;
	struct wlan_cmd_get_ip *ptr;

	dataLen = sizeof(struct wlan_cmd_get_ip);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memcpy(&(ptr->ip[0]), ip, 4);
	hex_dump("inetaddr ip", strlen("inetaddr ip"), ip, 4);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_GET_IP, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_set_essid(unsigned char vif_id, const unsigned char *essid,
		       int essid_len)
{

	int dataLen;
	struct wlan_cmd_set_essid *ptr;

	dataLen = sizeof(struct wlan_cmd_set_essid) + essid_len;
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->len = essid_len;
	memcpy(ptr->essid, essid, essid_len);

	return wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
				  WIFI_CMD_SET_ESSID, CMD_WAIT_TIMEOUT);
}

int wlan_cmd_req_lte_concur(unsigned char vif_id, const unsigned char *val,
			    int len)
{
	unsigned char *ptr;
	ptr = kmalloc(len, GFP_KERNEL);
	memcpy(ptr, val, len);
	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, len,
			   WIFI_CMD_REQ_LTE_CONCUR, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_pmksa(unsigned char vif_id, const unsigned char *bssid,
		   const unsigned char *pmkid, unsigned char type)
{
	int dataLen;
	struct wlan_cmd_pmkid *cmd;

	dataLen = sizeof(struct wlan_cmd_pmkid);
	cmd = kmalloc(dataLen, GFP_KERNEL);
	memset((unsigned char *)cmd, 0, dataLen);
	if (NULL != bssid)
		memcpy(cmd->bssid, bssid, sizeof(cmd->bssid));
	if (pmkid)
		memcpy(cmd->pmkid, pmkid, sizeof(cmd->pmkid));

	wlan_cmd_send_recv(vif_id, (unsigned char *)cmd, dataLen, type,
			   CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_cmq_rssi(unsigned char vif_id,
		      s32 rssi_thold, u32 rssi_hyst, unsigned char type)
{
	int dataLen;
	struct wlan_cmd_cqm_rssi *cmd;

	dataLen = sizeof(struct wlan_cmd_cqm_rssi);
	cmd = kmalloc(dataLen, GFP_KERNEL);
	memset((char *)cmd, 0, dataLen);

	cmd->rssih = rssi_thold;
	cmd->rssil = rssi_hyst;

	wlan_cmd_send_recv(vif_id, (unsigned char *)cmd, dataLen,
			   type, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_disconnect(unsigned char vif_id, unsigned short reason_code)
{

	int dataLen;
	struct wlan_cmd_disconnect *ptr;

	dataLen = sizeof(struct wlan_cmd_disconnect);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->reason_code = reason_code;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_DISCONNECT, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_add_key(unsigned char vif_id, const unsigned char *key_data,
		     unsigned char key_len, unsigned char pairwise,
		     unsigned char key_index, const unsigned char *key_seq,
		     unsigned char cypher_type, const unsigned char *pmac)
{

	int dataLen;
	struct wlan_cmd_add_key *ptr;

	dataLen = sizeof(struct wlan_cmd_add_key) + key_len;
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memset(ptr, 0, dataLen);

	ptr->cypher_type = cypher_type;
	if (key_seq != NULL)
		memcpy(ptr->keyseq, key_seq, 8);
	ptr->key_index = key_index;
	ptr->key_len = key_len;
	if (pmac != NULL)
		memcpy(ptr->mac, pmac, 6);
	ptr->pairwise = pairwise;
	if (NULL != key_data)
		memcpy(ptr->value, key_data, key_len);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_KEY_ADD, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_del_key(unsigned char vif_id, unsigned short key_index,
		     const unsigned char *mac_addr)
{
	int dataLen = 0;
	struct wlan_cmd_del_key *ptr = NULL;

	dataLen = sizeof(struct wlan_cmd_del_key);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memset(ptr, 0, dataLen);
	ptr->key_index = key_index;
	if (NULL != mac_addr)
		memcpy(&(ptr->mac[0]), mac_addr, 6);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_KEY_DEL, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_set_key(unsigned char vif_id, unsigned char key_index)
{

	int dataLen;
	struct wlan_cmd_set_key *ptr;

	dataLen = sizeof(struct wlan_cmd_set_key);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->key_index = key_index;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_KEY_SET, CMD_WAIT_TIMEOUT);

	return OK;
}

int wlan_cmd_set_rts(unsigned char vif_id, unsigned short rts_threshold)
{
	int dataLen;
	struct wlan_cmd_set_rts *ptr;
	dataLen = sizeof(struct wlan_cmd_set_rts);
	ptr = kmalloc(dataLen, GFP_KERNEL);
	ptr->threshold = rts_threshold;

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SET_RTS_THRESHOLD, CMD_WAIT_TIMEOUT);

	return OK;
}

int wlan_cmd_set_frag(unsigned char vif_id, unsigned short frag_threshold)
{
	struct wlan_cmd_set_frag *frag;

	frag = kmalloc(sizeof(struct wlan_cmd_set_frag), GFP_KERNEL);
	frag->frag = frag_threshold;

	wlan_cmd_send_recv(vif_id, (unsigned char *)frag,
			   sizeof(struct wlan_cmd_set_frag),
			   WIFI_CMD_SET_FRAG_THRESHOLD, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_set_wps_ie(unsigned char vif_id, unsigned char type,
			const unsigned char *ie, unsigned char len)
{
	struct wlan_cmd_wps_ie *wps_ptr = NULL;

	wps_ptr = kmalloc(sizeof(struct wlan_cmd_wps_ie) + len, GFP_KERNEL);
	wps_ptr->type = type;
	wps_ptr->len = len;
	memcpy(wps_ptr->value, ie, len);

	wlan_cmd_send_recv(vif_id, (unsigned char *)wps_ptr,
			   sizeof(struct wlan_cmd_wps_ie) + len,
			   WIFI_CMD_SET_WPS_IE, CMD_WAIT_TIMEOUT);

	return 0;
}

int wlan_cmd_update_ft_ies(unsigned char vif_id,
			   struct cfg80211_update_ft_ies_params *ft_ies)
{
	int ret, dataLen;
	struct wlan_cmd_ft_ies_params *ptr;
	dataLen = sizeof(struct wlan_cmd_ft_ies_params) + ft_ies->ie_len;
	ptr = kmalloc(dataLen, GFP_KERNEL);
	if (NULL == ptr)
		return -1;
	ptr->md = ft_ies->md;
	ptr->ie_len = ft_ies->ie_len;
	memcpy(&(ptr->ie[0]), ft_ies->ie, ft_ies->ie_len);
	hex_dump("update_ft_ie:", strlen("update_ft_ie:"), (unsigned char *)ptr,
		 dataLen);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			       WIFI_CMD_UPDATE_FT_IE, CMD_WAIT_TIMEOUT);
	return ret;
}

int wlan_cmd_mac_open(unsigned char vif_id, unsigned char mode,
		      unsigned char *mac_addr)
{
	int ret;
	static int sync_flag = -1;
	struct wlan_cmd_mac_open *open;
	if (-1 == sync_flag) {
		g_wlan.hw.tx_cnt = 0;
		g_wlan.hw.rx_cnt = 0;
		g_wlan.hw.rx_record = 0;
		sync_flag = 0;
	}
	open = kmalloc(sizeof(struct wlan_cmd_mac_open), GFP_KERNEL);
	memset((unsigned char *)open, 0, sizeof(struct wlan_cmd_mac_open));
	open->mode = mode;
	if (NULL != mac_addr)
		memcpy((unsigned char *)(&(open->mac[0])), mac_addr, 6);
	set_wlan_status(1);
	ret =
	    wlan_cmd_send_recv(vif_id, (unsigned char *)open,
			       sizeof(struct wlan_cmd_mac_open),
			       WIFI_CMD_SET_DEV_OPEN, 8000);
	return ret;
}

int wlan_cmd_mac_close(unsigned char vif_id, unsigned char mode)
{
	struct wlan_cmd_mac_close *close;
	close = kmalloc(sizeof(struct wlan_cmd_mac_close), GFP_KERNEL);
	close->mode = mode;
	set_wlan_status(0);
	wlan_cmd_send_recv(vif_id, (unsigned char *)(close),
			   sizeof(struct wlan_cmd_mac_close),
			   WIFI_CMD_SET_DEV_CLOSE, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_assert(unsigned char vif_id, unsigned int reason_code)
{
	return 0;
	struct wlan_cmd_assert_t *assert_cmd = NULL;
	unsigned int dataLen = sizeof(struct wlan_cmd_assert_t);
	assert_cmd = kmalloc(sizeof(struct wlan_cmd_assert_t), GFP_KERNEL);
	assert_cmd->reason_code = reason_code;
	assert_cmd->tx_cnt = g_wlan.hw.tx_cnt;
	assert_cmd->rx_cnt = g_wlan.hw.rx_cnt;
	wlan_cmd_send_to_ic(vif_id, (unsigned char *)(assert_cmd), dataLen,
			    WIFI_CMD_ASSERT);
	return 0;
}

#ifdef CONFIG_MACH_SAMSUNG
int wlan_cmd_set_cap(unsigned int cap)
{
#define WLNPI_CMD_SET_WLAN_CAP (40)
	int ret;
	unsigned char  r_buf[512] = {0};
	unsigned short r_len = 0;
	struct npi_cmd_hdr *msg;
	int s_len = 0;
	unsigned char s_buff[8];

	msg = (struct npi_cmd_hdr *)s_buff;
	msg->type = HOST_TO_MARLIN_CMD;
	msg->subtype = WLNPI_CMD_SET_WLAN_CAP;
	msg->len = 4;   /*sizeof(unsigned int)*/
	s_len = msg->len + sizeof(struct npi_cmd_hdr);
	memcpy(s_buff + sizeof(struct npi_cmd_hdr), &cap,
		   sizeof(unsigned int));
	ret = wlan_cmd_npi_send_recv(s_buff, s_len,
								 r_buf, &r_len);
	printke("[Send wlan capability = 0x%x %s!]\n",
			cap, ret ? "failed" : "success");

	return ret;
}

int wlan_cmd_set_psm_cap(void)
{
#define PSM_PATH "/data/.psm.info"
/**
 ** enable: 0x0
 ** disable: 0x1
 ** STA: bit 0
 ** GC: bit 1
 **/
#define STA_GC_EN_SLEEP		(0x3)
#define STA_GC_NO_SLEEP		(0x0)
	struct file  *fp = 0;
	mm_segment_t  fs;
	loff_t       *pos;
	unsigned int flag = 0;
	unsigned char file_data;

	fp = filp_open(PSM_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		printke("/data/.psm.info file is not exist\n");
		flag = STA_GC_EN_SLEEP;
		goto OUT;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = &(fp->f_pos);
	vfs_read(fp, &file_data, sizeof(file_data), pos);

	filp_close(fp, NULL);
	set_fs(fs);

	if (simple_strtoul((char *)&file_data, NULL, 10))
		flag = STA_GC_EN_SLEEP;
	else
		flag = STA_GC_NO_SLEEP;
OUT:
	printke("[%s psm is:%s]\n", __func__,
			flag ? "normal mode" : "rf mode");
	/*set_wlan_cap*/
	return wlan_cmd_set_cap(flag);
}
#endif

int wlan_cmd_sleep(int ops)
{
	unsigned char *ops_code;
	ops_code = (unsigned char *)kmalloc(4, GFP_KERNEL);
	memcpy(ops_code, (unsigned char *)(&ops), 4);
	wlan_cmd_send_to_ic(0, ops_code, 4, WIFI_CMD_SLEEP);
	return 0;
}

int wlan_cmd_get_rssi(unsigned char vif_id, unsigned char *signal,
		      unsigned char *noise)
{
	int ret;
	r_msg_hdr_t *msg;
	int *rssi = NULL;
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	unsigned char r_buf[sizeof(r_msg_hdr_t) + 8] = { 0 };
	unsigned short r_len = 8;
	msg = (r_msg_hdr_t *) (&r_buf[0]);
	if (1 == g_wlan.sync.exit) {
		printke("%s, exit flag\n", __func__);
		return ERROR;
	}
	mutex_lock(&(cmd->cmd_lock));
	ret = wlan_cmd_send_to_ic(vif_id, NULL, 0, WIFI_CMD_GET_RSSI);
	ret = wlan_timeout_recv_rsp(r_buf, &r_len, CMD_WAIT_TIMEOUT);
	if (-1 == ret) {
		printke("[SEND_CMD %s %d ERROR][rsp timeout]\n",
			get_cmd_name(WIFI_CMD_GET_RSSI), vif_id);
		goto err;
	}
	if ((SC2331_HOST_RSP != msg->type)
	    || (WIFI_CMD_GET_RSSI != msg->subtype)) {
		printke("[SEND_CMD %s %d ERROR][rsp match %s]\n",
			get_cmd_name(WIFI_CMD_GET_RSSI), vif_id,
			get_cmd_name(msg->subtype));
		goto err;
	} else {
		rssi = (int *)(&r_buf[sizeof(r_msg_hdr_t) + 4]);
		*signal = (unsigned char)(le32_to_cpu(*rssi) | 0xffff0000);
		*noise =
		    (unsigned char)((le32_to_cpu(*rssi) | 0x0000ffff) >> 16);
	}
	mutex_unlock(&(cmd->cmd_lock));
	return OK;
err:
	wlan_cmd_assert(vif_id, 3);
	mutex_unlock(&(cmd->cmd_lock));
	return ERROR;
}

int wlan_cmd_get_txrate_txfailed(unsigned char vif_id, unsigned int *rate,
				 unsigned int *failed)
{
	int ret;
	r_msg_hdr_t *msg;
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	unsigned char r_buf[sizeof(r_msg_hdr_t) + 12] = { 0 };
	unsigned short r_len = 12;
	msg = (r_msg_hdr_t *) (&r_buf[0]);
	if (1 == g_wlan.sync.exit) {
		printke("%s, exit flag\n", __func__);
		return ERROR;
	}
	mutex_lock(&(cmd->cmd_lock));
	ret =
	    wlan_cmd_send_to_ic(vif_id, NULL, 0, WIFI_CMD_GET_TXRATE_TXFAILED);
	ret = wlan_timeout_recv_rsp(r_buf, &r_len, CMD_WAIT_TIMEOUT);
	if (-1 == ret) {
		printke("[SEND_CMD %s %d ERROR][rsp timeout]\n",
			get_cmd_name(WIFI_CMD_GET_TXRATE_TXFAILED), vif_id);
		goto err;
	}
	if ((SC2331_HOST_RSP != msg->type)
	    || (WIFI_CMD_GET_TXRATE_TXFAILED != msg->subtype)) {
		printke("[SEND_CMD %s %d ERROR][rsp match %s]\n",
			get_cmd_name(WIFI_CMD_GET_TXRATE_TXFAILED), vif_id,
			get_cmd_name(msg->subtype));
		goto err;
	} else {
		memcpy((unsigned char *)rate, &r_buf[sizeof(r_msg_hdr_t) + 4],
		       4);
		memcpy((unsigned char *)failed, &r_buf[sizeof(r_msg_hdr_t) + 8],
		       4);
	}
	mutex_unlock(&(cmd->cmd_lock));
	return OK;
err:
	mutex_unlock(&(cmd->cmd_lock));
	return ERROR;
}

int wlan_cmd_set_regdom(unsigned char vif_id, unsigned char *regdom,
			unsigned int len)
{
	int dataLen;
	wlan_ieee80211_regdomain *ptr;

	dataLen = len;
	ptr = kmalloc(dataLen, GFP_KERNEL);
	memcpy(ptr, regdom, dataLen);

	wlan_cmd_send_recv(vif_id, (unsigned char *)ptr, dataLen,
			   WIFI_CMD_SCAN_NOR_CHANNELS, CMD_WAIT_TIMEOUT);
	return 0;
}

int wlan_cmd_npi_send_recv(unsigned char *s_buf, unsigned short s_len,
			   unsigned char *r_buf, unsigned short *r_len)
{
	r_msg_hdr_t *msg;
	int ret;
	wlan_cmd_t *cmd = &(g_wlan.cmd);
	unsigned char *s_data = NULL;

	s_data = kmalloc(s_len, GFP_KERNEL);
	memcpy(s_data, s_buf, s_len);

	mutex_lock(&(cmd->cmd_lock));
	ret = wlan_cmd_send_to_ic(0, s_data, s_len, WIFI_CMD_NPI_MSG);
	if (1 == g_wlan.sync.exit) {
		printke("%s, exit flag\n", __func__);
		goto err;
	}
	ret =
	    wait_event_timeout(cmd->waitQ,
			       ((1 == cmd->wakeup)
				|| (1 == g_wlan.sync.exit)),
			       msecs_to_jiffies(5000));
	cmd->wakeup = 0;
	if (0 == ret) {
		printke("%s(), wait timeout\n", __func__);
		goto err;
	}
	msg = (r_msg_hdr_t *) cmd->mem;
	if ((SC2331_HOST_RSP == msg->type)
	    && (WIFI_CMD_NPI_MSG == msg->subtype)) {

		memcpy(r_buf, (unsigned char *)(cmd->mem) + sizeof(r_msg_hdr_t),
		       msg->len);
		*r_len = msg->len;
	} else {
		printke("[%s] rsp not match, rsp:[%s]\n",
			get_cmd_name(WIFI_CMD_NPI_MSG),
			get_cmd_name(msg->subtype));
		goto err;
	}
	mutex_unlock(&(cmd->cmd_lock));
	printkd("%s cmd ok!\n", get_cmd_name(WIFI_CMD_NPI_MSG));
	return OK;
err:
	mutex_unlock(&(cmd->cmd_lock));
	printke(" ################### %s(), ERROR  ####################\n",
		get_cmd_name(WIFI_CMD_NPI_MSG));
	g_dbg = 0xffffffff;
	return ERROR;

}

int wlan_rx_rsp_process(const unsigned char vif_id, r_msg_hdr_t *msg)
{
	wlan_cmd_t  *cmd = &(g_wlan.cmd);

	if (mutex_trylock(&cmd->mem_lock)) {
		printkd("[RECV_RSP][%d][%s][%d][%d]\n", vif_id,
			get_cmd_name(msg->subtype), msg->len,
			*((int *)(msg + 1)));
		if (msg->len + sizeof(r_msg_hdr_t) > WLAN_CMD_MEM_LEN)
			BUG_ON(1);
		memcpy(cmd->mem, (unsigned char *)msg,
		       msg->len + sizeof(r_msg_hdr_t));
		cmd->wakeup = 1;
		wake_up(&cmd->waitQ);
		mutex_unlock(&cmd->mem_lock);
	} else {
		printke("[RECV_RSP][%d][%s][%d], but drop it!\n",
			vif_id, get_cmd_name(msg->subtype), msg->len);
	}

	return OK;
}

int wlan_rx_event_process(const unsigned char vif_id, unsigned char event,
			  unsigned char *pData, unsigned short len)
{
	if ((WIFI_EVENT_REPORT_SCAN_FRAME != event)
	    && (WIFI_EVENT_SDIO_SEQ_NUM != event))
		printkd("[RECV_EVENT][%d][%s][%d]\n", vif_id,
			get_cmd_name(event), len);
	switch (event) {
	case WIFI_EVENT_CONNECT:
		cfg80211_report_connect_result(vif_id, pData, len);
		break;
	case WIFI_EVENT_DISCONNECT:
		cfg80211_report_disconnect_done(vif_id, pData, len);
		break;
	case WIFI_EVENT_SCANDONE:
		cfg80211_report_scan_done(vif_id, pData, len, false);
		break;
	case WIFI_EVENT_MGMT_DEAUTH:
		cfg80211_report_mgmt_deauth(vif_id, pData, len);
		break;
	case WIFI_EVENT_MGMT_DISASSOC:
		cfg80211_report_mgmt_disassoc(vif_id, pData, len);
		break;
	case WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED:
		cfg80211_report_remain_on_channel_expired(vif_id, pData, len);
		break;
	case WIFI_EVENT_NEW_STATION:
		cfg80211_report_station(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_FRAME:
		cfg80211_report_frame(vif_id, pData, len);
		break;
	case WIFI_EVENT_CONNECT_AP:
		break;
	case WIFI_EVENT_SDIO_SEQ_NUM:
		break;
	case WIFI_EVENT_REPORT_SCAN_FRAME:
		cfg80211_report_scan_frame(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_MIC_FAIL:
		cfg80211_report_mic_failure(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_CQM_RSSI_LOW:
		cfg80211_report_cqm_low(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_CQM_RSSI_HIGH:
		cfg80211_report_cqm_high(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_CQM_RSSI_LOSS_BEACON:
		cfg80211_report_cqm_beacon_loss(vif_id, pData, len);
		break;
	case WIFI_EVENT_MLME_TX_STATUS:
		cfg80211_report_mlme_tx_status(vif_id, pData, len);
		break;
	case WIFI_EVENT_REPORT_VERSION:
		cfg80211_report_version(vif_id, pData, len);
		break;
	default:
		break;
	}
	return OK;
}

int hex_dump(unsigned char *name, unsigned short nLen, unsigned char *pData,
	     unsigned short len)
{
	unsigned char *str;
	int i, p, ret;
	if (len > 1024)
		len = 1024;
	str = kmalloc(((len + 1) * 3 + nLen), GFP_KERNEL);
	memset(str, 0, (len + 1) * 3 + nLen);
	memcpy(str, name, nLen);
	if ((NULL == pData) || (0 == len)) {
		printke("%s\n", str);
		kfree(str);
		return 0;
	}
	p = 0;
	for (i = 0; i < len; i++) {
		ret = sprintf((str + nLen + p), "%02x ", *(pData + i));
		p = p + ret;
	}
	printke("%s\n\n", str);
	kfree(str);
	return 0;
}
