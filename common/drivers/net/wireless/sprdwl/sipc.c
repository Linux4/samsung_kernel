/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sipc.c
 * Abstract : This file is a implementation for sprdwl SIPC command/event
 *
 * Authors	:
 * Leon Liu <leon.liu@spreadtrum.com>
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 * Danny Deng <danny.deng@spreadtrum.com>
 * Jingxiang Li <jingxiang.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/sipc.h>
#include <asm/byteorder.h>
#include <linux/sipc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>

#include "cfg80211.h"
#include "sipc.h"
#include "sprdwl.h"

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
static void wlan_sipc_event_rx_handler(struct sprdwl_priv *priv);
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

#define SIPC_PRI	"net:"
#ifdef CONFIG_OF
/* sblock configs
	[0] dst		[1] channel
	[2] txblocknum	[3] txblocksize
	[4] rxblocknum	[5] rxblocksize
*/
#define SIPC_SBLOCKS_NUM	2
static int sipc_sblocks[][6] = {
	{
	 WLAN_CP_ID, WLAN_SBLOCK_CH,
	 WLAN_SBLOCK_NUM, WLAN_SBLOCK_SIZE,
	 WLAN_SBLOCK_RX_NUM, WLAN_SBLOCK_SIZE},
	{
	 WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
	 WLAN_EVENT_SBLOCK_NUM, WLAN_EVENT_SBLOCK_SIZE,
	 WLAN_EVENT_SBLOCK_NUM, WLAN_EVENT_SBLOCK_SIZE}
};

void sprdwl_sipc_sblock_deinit(int sblock_ch)
{
	int i;
	for (i = 0; i < SIPC_SBLOCKS_NUM; i++) {
		if (sblock_ch == sipc_sblocks[i][1])
			break;
	}
	if (i == SIPC_SBLOCKS_NUM)
		return;
	sblock_destroy(sipc_sblocks[i][0], sipc_sblocks[i][1]);
}

int sprdwl_sipc_sblock_init(int sblock_ch,
			    void (*handler) (int event, void *data), void *data)
{
	int i, ret = 0;

	for (i = 0; i < SIPC_SBLOCKS_NUM; i++) {
		if (sblock_ch == sipc_sblocks[i][1])
			break;
	}
	if (i == SIPC_SBLOCKS_NUM)
		return -EINVAL;

	ret = sblock_create(sipc_sblocks[i][0], sipc_sblocks[i][1],
			    sipc_sblocks[i][2], sipc_sblocks[i][3],
			    sipc_sblocks[i][4], sipc_sblocks[i][5]);
	if (ret) {
		pr_err(SIPC_PRI "create sblock %d-%d failed:%d\n",
		       sipc_sblocks[i][0], sipc_sblocks[i][1], ret);
		return ret;
	}

	ret =
	    sblock_register_notifier(sipc_sblocks[i][0], sipc_sblocks[i][1],
				     handler, data);
	if (ret) {
		pr_err(SIPC_PRI "regitster sblock %d-%d notifier failed:%d\n",
		       sipc_sblocks[i][0], sipc_sblocks[i][1], ret);
		sblock_destroy(sipc_sblocks[i][0], sipc_sblocks[i][1]);
	}

	return ret;
}
#endif

/* sbuf should be created and initilized */
int wlan_sipc_cmd_send(struct wlan_sipc *wlan_sipc, u16 len, u8 type, u8 id)
{
	struct wlan_sipc_data *send_buf =
	    (struct wlan_sipc_data *)wlan_sipc->send_buf;
	u32 msg_hdr;
	u16 cmd_tag;
	int send_len;

	msg_hdr = SPRDWL_MSG(SPRDWL_MSG_CMD, len);
	cmd_tag = SPRDWL_CMD_TAG(type, CMD_DIR_AP2CP, CMD_LEVEL_NORMAL, id);

	send_buf->msg_hdr = cpu_to_le32(msg_hdr);
	send_buf->u.cmd.cmd_tag = cpu_to_le16(cmd_tag);
/*      send_buf->u.seq_no = cpu_to_1e16(seq_no);*/

	pr_debug(SIPC_PRI "send cmd [%d %d]\n", id, type);
	/* sipc-sbuf ops */
	send_len = sbuf_write(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			      send_buf, len,
			      msecs_to_jiffies(DEFAULT_WAIT_SBUF_TIME));

	if (send_len < 0) {
		pr_err(SIPC_PRI "cmd [%d %d] sbuf_write error (%d)\n",
		       id, type, send_len);
		return send_len;
	} else if (send_len != len) {
		pr_err(SIPC_PRI
		       "cmd [%d %d] sbuf_write not completely with send len %d\n",
		       id, type, send_len);
		return -EIO;
	} else {
#ifdef DUMP_COMMAND
		print_hex_dump(KERN_DEBUG, "cmd: ", DUMP_PREFIX_OFFSET,
			       16, 1, send_buf, send_len, 0);
#endif
		return 0;
	}
}

/* sbuf should be created and initilized */
int wlan_sipc_cmd_receive(struct wlan_sipc *wlan_sipc, u16 len, u8 id)
{
	struct wlan_sipc_data *recv_buf =
	    (struct wlan_sipc_data *)wlan_sipc->recv_buf;
	u32 msg_hdr;
	u16 msg_type, msg_len;
	u16 cmd_tag;
/*      u16 seq_no;*/
	u8 cmd_dir;
	u8 cmd_id;
	int recv_len;

	/* TODO sipc-sbuf ops */
	recv_len = sbuf_read(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			     recv_buf, len,
			     msecs_to_jiffies(DEFAULT_WAIT_SBUF_TIME));

	if (recv_len < 0) {
		pr_err(SIPC_PRI "cmd %d sbuf_read error (%d)\n", id, recv_len);
		return recv_len;
	} else if (recv_len != len) {
		pr_err(SIPC_PRI "cmd %d sbuf_read not completely,len %d\n",
		       id, recv_len);
		return -EIO;
	}
#ifdef DUMP_COMMAND_RESPONSE
	print_hex_dump(KERN_DEBUG, "cmd rsp: ", DUMP_PREFIX_OFFSET,
		       16, 1, recv_buf, recv_len, 0);
#endif

	msg_hdr = le32_to_cpu(recv_buf->msg_hdr);
	cmd_tag = le16_to_cpu(recv_buf->u.cmd_resp.cmd_tag);
/*      seq_no  = le16_to_cpu(recv_buf->u.cmd_resp.seq_no);*/

	msg_type = SPRDWL_MSG_GET_TYPE(msg_hdr);
	msg_len = SPRDWL_MSG_GET_LEN(msg_hdr);

	cmd_dir = SPRDWL_CMD_TAG_GET_DIR(cmd_tag);
	cmd_id = SPRDWL_CMD_TAG_GET_ID(cmd_tag);

	if ((msg_len != recv_len) || (msg_type != SPRDWL_MSG_CMD_RSP)) {
		pr_err(SIPC_PRI "cmd %d msg_len:%d msg_type:%d\n",
		       id, msg_len, msg_type);
		return -EIO;
	}

	if (cmd_dir != CMD_DIR_CP2AP) {
		pr_err(SIPC_PRI "cmd %d dir is not CP to AP:%d\n", id, cmd_dir);
		return -EIO;
	}

	if (cmd_id != id) {
		pr_err(SIPC_PRI "cmd %d recv wrong cmd id %d\n", id, cmd_id);
		return -EIO;
	}

	return 0;
}

int sprdwl_cmd_send_recv(struct wlan_sipc *wlan_sipc, u8 type, u8 id)
{
	u16 status;
	int ret = 0;

	if (!mutex_trylock(&wlan_sipc->pm_lock)) {
		pr_err(SIPC_PRI "cmd [%d %d] could not get pm mutex\n",
		       id, type);
		ret = -EBUSY;
		return ret;
	}

	/* make sure cmd more safe */
	ret = sbuf_read(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			wlan_sipc->recv_buf, WLAN_SBUF_SIZE, 0);
	if (ret > 0) {
		pr_err(SIPC_PRI "cmd [%d %d] clean dirty data,len:%d<%d\n",
		       id, type, ret, WLAN_SBUF_SIZE);
	} else if (ret != -ENODATA) {
		pr_err(SIPC_PRI "cmd [%d %d] sbuf recv channel error(%d)\n",
		       id, type, ret);
		goto out;
	}

	ret =
	    wlan_sipc_cmd_send(wlan_sipc, wlan_sipc->wlan_sipc_send_len,
			       type, id);

	if (ret) {
		pr_err(SIPC_PRI "cmd [%d %d] send error %d\n", id, type, ret);
		goto out;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, wlan_sipc->wlan_sipc_recv_len,
				    id);

	if (ret) {
		pr_err(SIPC_PRI "cmd [%d %d] recv error %d\n", id, type, ret);
		goto out;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	/* FIXME consider return status */
	if (status) {
		pr_err(SIPC_PRI "cmd [%d %d] status error %d\n",
		       id, type, status);
		mutex_unlock(&wlan_sipc->pm_lock);
		return status;
	}

out:
	mutex_unlock(&wlan_sipc->pm_lock);
	return ret;
}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
int sprdwl_set_scan_channels_cmd(struct wlan_sipc *wlan_sipc,
				 u8 *channel_info, int len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug("set channels!\n");

	mutex_lock(&wlan_sipc->cmd_lock);

	if (channel_info && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, channel_info, len);
		send_len = len;
	}

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret =
	    itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				   WIFI_CMD_SCAN_CHANNELS);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

int sprdwl_scan_cmd(struct wlan_sipc *wlan_sipc, const u8 *ssid, int len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug(SIPC_PRI "trigger scan\n");

	mutex_lock(&wlan_sipc->cmd_lock);

	if (ssid && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, ssid, len);
		send_len = len;
	}

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_SCAN);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_wpa_version_cmd(struct wlan_sipc *wlan_sipc, u32 wpa_version)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 version = wpa_version;
	int ret;

	if (wpa_version == 0) {
		/*Disable */
		pr_debug(SIPC_PRI "set wpa_version is zero\n");
		return 0;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &version, sizeof(version));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(version);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc,
				   CMD_TYPE_SET, WIFI_CMD_WPA_VERSION);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_auth_type_cmd(struct wlan_sipc *wlan_sipc, u32 type)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 auth_type = type;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);
	memcpy(send_buf->u.cmd.variable, &auth_type, sizeof(auth_type));

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(auth_type);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_AUTH_TYPE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

/* unicast cipher or group cipher */
int sprdwl_set_cipher_cmd(struct wlan_sipc *wlan_sipc, u32 cipher, u8 cmd_id)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 chipher_val = cipher;
	u8 id = cmd_id;
	int ret;

	if ((id != WIFI_CMD_PAIRWISE_CIPHER) && (id != WIFI_CMD_GROUP_CIPHER)) {
		pr_err(SIPC_PRI "not support this type cipher: %d\n", id);
		return -EINVAL;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &chipher_val, sizeof(chipher_val));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(chipher_val);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, cmd_id);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_key_management_cmd(struct wlan_sipc *wlan_sipc, u8 key_mgmt)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u8 key = key_mgmt;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key, sizeof(key));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(key);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_AKM_SUITE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_get_device_mode_cmd(struct wlan_sipc *wlan_sipc)
{
	s32 device_mode;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE
	    + sizeof(device_mode);

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_DEV_MODE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&device_mode, wlan_sipc->recv_buf->u.cmd_resp.variable,
	       sizeof(device_mode));

	mutex_unlock(&wlan_sipc->cmd_lock);

	return device_mode;
}

int sprdwl_set_psk_cmd(struct wlan_sipc *wlan_sipc, const u8 *key, u32 key_len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	if (key_len == 0) {
		pr_err(SIPC_PRI "PSK length is 0\n");
		return 0;
	}

	if (key_len > 32) {
		pr_err(SIPC_PRI "PSK length is larger than 32\n");
		return -EINVAL;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, key, key_len);
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + key_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_PSK);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_channel_cmd(struct wlan_sipc *wlan_sipc, u32 channel)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 channel_num = channel;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &channel_num, sizeof(channel_num));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(channel_num);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_CHANNEL);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_bssid_cmd(struct wlan_sipc *wlan_sipc, const u8 *addr)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, 6);
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + 6;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_BSSID);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_essid_cmd(struct wlan_sipc *wlan_sipc,
			 const u8 *essid, int essid_len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, essid, essid_len);
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + essid_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_ESSID);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_pmksa_cmd(struct wlan_sipc *wlan_sipc, const u8 *bssid,
		     const u8 *pmkid, u8 type)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_pmkid *cmd;
	int ret;

	if (bssid == NULL) {
		pr_err(SIPC_PRI "pmksa->bssid is null\n");
		return -EINVAL;
	}

	if ((type != CMD_TYPE_FLUSH) && pmkid == NULL) {
		pr_err(SIPC_PRI "pmksa->pmkid is null\n");
		return -EINVAL;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	cmd = (struct wlan_sipc_pmkid *)send_buf->u.cmd.variable;
	memcpy(cmd->bssid, bssid, sizeof(cmd->bssid));
	if (pmkid)
		memcpy(cmd->pmkid, pmkid, sizeof(cmd->pmkid));

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_pmkid);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, type, WIFI_CMD_PMKSA);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_disconnect_cmd(struct wlan_sipc *wlan_sipc, u16 reason_code)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &reason_code, sizeof(reason_code));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(reason_code);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc,
				   CMD_TYPE_SET, WIFI_CMD_DISCONNECT);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_add_key_cmd(struct wlan_sipc *wlan_sipc, const u8 *key_data,
		       u8 key_len, u8 pairwise, u8 key_index, const u8 *key_seq,
		       u8 cypher_type, const u8 *pmac)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_add_key *add_key =
	    (struct wlan_sipc_add_key *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	if (pmac != NULL)
		memcpy(add_key->mac, pmac, 6);
	if (key_seq != NULL)
		memcpy(add_key->keyseq, key_seq, 8);
	add_key->pairwise = pairwise;
	add_key->cypher_type = cypher_type;
	add_key->key_index = key_index;
	add_key->key_len = key_len;
	memcpy(add_key->value, key_data, key_len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + key_len
	    + sizeof(struct wlan_sipc_add_key);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_ADD, WIFI_CMD_KEY);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_del_key_cmd(struct wlan_sipc *wlan_sipc, u16 key_index,
		       const u8 *mac_addr)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_del_key *del_key =
	    (struct wlan_sipc_del_key *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	if (mac_addr != NULL)
		memcpy(del_key->mac, mac_addr, 6);
	del_key->key_index = key_index;

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_del_key);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_DEL, WIFI_CMD_KEY);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_key_cmd(struct wlan_sipc *wlan_sipc, u8 key_index)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key_index, sizeof(key_index));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(key_index);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_KEY);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_rts_cmd(struct wlan_sipc *wlan_sipc, u16 rts_threshold)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u16 threshold = rts_threshold;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(threshold);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				   WIFI_CMD_RTS_THRESHOLD);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_frag_cmd(struct wlan_sipc *wlan_sipc, u16 frag_threshold)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u16 threshold = frag_threshold;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(threshold);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				   WIFI_CMD_FRAG_THRESHOLD);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_get_rssi_cmd(struct wlan_sipc *wlan_sipc, s8 *signal, s8 *noise)
{
	struct wlan_sipc_data *recv_buf = wlan_sipc->recv_buf;
	u32 rssi;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE + sizeof(rssi);
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_GET, WIFI_CMD_RSSI);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&rssi, recv_buf->u.cmd_resp.variable, sizeof(rssi));
	*signal = (u8) (le32_to_cpu(rssi) | 0xffff0000);
	*noise = (u8) ((le32_to_cpu(rssi) | 0x0000ffff) >> 16);

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_get_txrate_cmd(struct wlan_sipc *wlan_sipc, s32 *rate)
{
	struct wlan_sipc_data *recv_buf = wlan_sipc->recv_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE
	    + sizeof(*rate);
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_GET, WIFI_CMD_TXRATE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(rate, recv_buf->u.cmd_resp.variable, sizeof(*rate));
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_start_ap_cmd(struct wlan_sipc *wlan_sipc, u8 *beacon, u16 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_beacon *beacon_ptr =
	    (struct wlan_sipc_beacon *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	beacon_ptr->len = len;
	memcpy(beacon_ptr->value, beacon, len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(beacon_ptr->len) + len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_START_AP);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
int sprdwl_set_p2p_ie_cmd(struct wlan_sipc *wlan_sipc,
			  u8 type, const u8 *ie, u8 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_p2p_ie *p2p_ptr =
	    (struct wlan_sipc_p2p_ie *)send_buf->u.cmd.variable;
	int ret;

	if (type != P2P_ASSOC_IE && type != P2P_BEACON_IE &&
	    type != P2P_PROBERESP_IE && type != P2P_ASSOCRESP_IE &&
	    type != P2P_BEACON_IE_HEAD && type != P2P_BEACON_IE_TAIL) {
		pr_err("wrong ie type is %d\n", type);
		return -EIO;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	p2p_ptr->type = type;
	p2p_ptr->len = len;
	memcpy(p2p_ptr->value, ie, len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_p2p_ie) + len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_P2P_IE);

	if (ret) {
		pr_err(SIPC_PRI "return wrong status code is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	pr_debug(SIPC_PRI "set p2p ie return status code successfully\n");

	return 0;
}
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

int sprdwl_set_wps_ie_cmd(struct wlan_sipc *wlan_sipc,
			  u8 type, const u8 *ie, u8 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_wps_ie *wps_ptr =
	    (struct wlan_sipc_wps_ie *)send_buf->u.cmd.variable;
	int ret;

	if (type != WPS_REQ_IE && type != WPS_ASSOC_IE) {
		pr_err(SIPC_PRI "wrong ie type is %d\n", type);
		return -EIO;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	wps_ptr->type = type;
	wps_ptr->len = len;
	memcpy(wps_ptr->value, ie, len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(wps_ptr) + len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_WPS_IE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_blacklist_cmd(struct wlan_sipc *wlan_sipc, u8 *addr, u8 flag)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, 6);
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + 6;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	if (flag)
		ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_ADD,
					   WIFI_CMD_BLACKLIST);
	else
		ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_DEL,
					   WIFI_CMD_BLACKLIST);

	if (ret) {
		pr_err(SIPC_PRI "blacklist wrong status code is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	pr_debug(SIPC_PRI "blacklist return status code successfully\n");

	return 0;
}

int sprdwl_mac_open_cmd(struct wlan_sipc *wlan_sipc, u8 mode, u8 *mac_addr)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_mac_open *open =
	    (struct wlan_sipc_mac_open *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	sprdwl_nvm_init();

	open->mode = mode;
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	open->mode |= FW_ZEROCOPY;
#endif
	if (mac_addr)
		memcpy(open->mac, mac_addr, 6);
	memcpy((unsigned char *)(&(open->nvm_data)),
	       (unsigned char *)(get_gwifi_nvm_data()),
	       sizeof(struct wifi_nvm_data));
	/*Addded AP timestamp members */
	sprdwl_get_ap_time(open->ap_timestamp);
	wlan_sipc->wlan_sipc_send_len =
	    SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_mac_open);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	pr_debug(SIPC_PRI "%s mode %#x cmd_len %d\n", __func__, open->mode,
		 wlan_sipc->wlan_sipc_send_len);
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_DEV_OPEN);
	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		switch (ret) {
		case ERR_AP_ZEROCOPY_CP_NOT:
			pr_err(SIPC_PRI
			       "%s AP: zero copy; CP: non zero copy\n",
			       __func__);
			break;
		case ERR_CP_ZEROCOPY_AP_NOT:
			pr_err(SIPC_PRI
			       "%s AP: non zero copy; CP: zero copy\n",
			       __func__);
			break;
		default:
			break;
		}
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_mac_close_cmd(struct wlan_sipc *wlan_sipc, u8 mode)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u8 flag = mode;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	flag |= FW_ZEROCOPY;
#endif
	memcpy(send_buf->u.cmd.variable, &flag, sizeof(flag));
	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + sizeof(flag);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	pr_debug(SIPC_PRI "%s mode %#x\n", __func__, flag);
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_DEV_CLOSE);

	if (ret) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

/* EVENT SBLOCK implementation */
static void wlan_sipc_event_rx_handler(struct sprdwl_priv *priv)
{
	struct sblock blk;
	struct wlan_sipc_data *recv_buf;
	u32 msg_hdr;
	u16 msg_type, msg_len;
	u16 event_tag;
/*      u16 seq_no;*/
	u8 event_type;
	u8 event_id;
	int ret;

	ret = sblock_receive(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk, 0);
	if (ret) {
		pr_err(SIPC_PRI "Failed to receive sblock (%d)\n", ret);
		return;
	}
	recv_buf = (struct wlan_sipc_data *)blk.addr;

	msg_hdr = le32_to_cpu(recv_buf->msg_hdr);
	event_tag = le16_to_cpu(recv_buf->u.event.event_tag);
/*      seq_no  = le16_to_cpu(recv_buf->u.event_tag.seq_no);*/

	msg_type = SPRDWL_MSG_GET_TYPE(msg_hdr);
	msg_len = SPRDWL_MSG_GET_LEN(msg_hdr);
	if ((msg_len != blk.length) || (msg_type != SPRDWL_MSG_EVENT)) {
		pr_err(SIPC_PRI
		       "Recv msg_len(%d), blk.length(%d) and type(%d) is wrong\n",
		       msg_len, blk.length, msg_type);
		ret = sblock_release(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk);
		if (ret)
			pr_err(SIPC_PRI "release sblock failed (%d)\n", ret);
		return;
	}

	event_type = SPRDWL_EVENT_TAG_GET_TYPE(event_tag);
	event_id = SPRDWL_EVENT_TAG_GET_ID(event_tag);
	if (event_type == EVENT_TYPE_SYSERR)
		pr_err(SIPC_PRI "Recv event type is syserr\n");

	priv->wlan_sipc->wlan_sipc_event_len = msg_len - SPRDWL_EVENT_HDR_SIZE;

	recv_buf = priv->wlan_sipc->event_buf;
	memcpy((u8 *)recv_buf, (u8 *)blk.addr, blk.length);

#ifdef DUMP_EVENT
	if (event_id != WIFI_EVENT_SCANDONE) {
		print_hex_dump(KERN_DEBUG, "event: ", DUMP_PREFIX_OFFSET,
			       16, 1, blk.addr, blk.length, 0);
	}
#endif
	ret = sblock_release(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk);
	if (ret)
		pr_err(SIPC_PRI "release sblock failed (%d)\n", ret);

	switch (event_id) {
	case WIFI_EVENT_CONNECT:
		pr_debug(SIPC_PRI "Recv sblock8 connect event\n");
		sprdwl_event_connect_result(priv);
		break;
	case WIFI_EVENT_DISCONNECT:
		pr_debug(SIPC_PRI "Recv sblock8 disconnect event\n");
		sprdwl_event_disconnect(priv);
		break;
	case WIFI_EVENT_SCANDONE:
		pr_debug(SIPC_PRI "Recv sblock8 scan result event\n");
		sprdwl_event_scan_results(priv, false);
		break;
	case WIFI_EVENT_READY:
		pr_debug(SIPC_PRI "Recv sblock8 CP2 ready event\n");
		sprdwl_event_ready(priv);
		break;
	case WIFI_EVENT_TX_BUSY:
		pr_debug(SIPC_PRI "Recv data tx sblock busy event\n");
		sprdwl_event_tx_busy(priv);
		break;
	case WIFI_EVENT_SOFTAP:
		pr_debug(SIPC_PRI "Recv sblock8 softap event\n");
		sprdwl_event_softap(priv);
		break;
#ifdef CONFIG_SPRDWL_WIFI_DIRECT

	case WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED:
		pr_debug(SIPC_PRI
			 "Recv p2p  remain on channel EXPIRED event\n");
		sprdwl_event_remain_on_channel_expired(priv);
		break;
	case WIFI_EVENT_MGMT_DEAUTH:
		pr_debug(SIPC_PRI "Recv p2p deauth event\n");
		sprdwl_event_mgmt_deauth(priv);
		break;
	case WIFI_EVENT_MGMT_DISASSOC:
		pr_debug(SIPC_PRI "Recv p2p disassoc event\n");
		sprdwl_event_mgmt_disassoc(priv);
		break;
	case WIFI_EVENT_REPORT_FRAME:
		pr_debug(SIPC_PRI "Recv p2p EVENT_REPORT_FRAME\n");
		sprdwl_event_report_frame(priv);
		break;
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

	default:
		pr_err(SIPC_PRI "Recv sblock8 unknow event id %d\n", event_id);
		break;
	}
}

void wlan_sipc_sblock_handler(int event, void *data)
{
	struct sprdwl_priv *priv = (struct sprdwl_priv *)data;

	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		wlan_sipc_event_rx_handler(priv);
		break;
	case SBLOCK_NOTIFY_GET:
	case SBLOCK_NOTIFY_STATUS:
	case SBLOCK_NOTIFY_OPEN:
	case SBLOCK_NOTIFY_CLOSE:
		pr_debug(SIPC_PRI "report event:%d received\n", event);
		break;
	default:
		pr_err(SIPC_PRI "report event is invalid(%d)\n", event);
	}
}

/* TODO -- add sbuf register and free */
int sprdwl_sipc_alloc(struct sprdwl_priv *sprdwl_priv)
{
	struct wlan_sipc *wlan_sipc;
	struct wlan_sipc_data *sipc_buf;
	int ret;

	wlan_sipc = kzalloc(sizeof(struct wlan_sipc), GFP_KERNEL);
	if (!wlan_sipc) {
		pr_err(SIPC_PRI "failed to alloc wlan_sipc struct\n");
		return -ENOMEM;
	}

	sipc_buf = kzalloc(WLAN_SBUF_SIZE, GFP_KERNEL);
	if (!sipc_buf) {
		pr_err(SIPC_PRI "get new send buf failed\n");
		ret = -ENOMEM;
		goto fail_sendbuf;
	}
	wlan_sipc->send_buf = sipc_buf;

	sipc_buf = kzalloc(WLAN_SBUF_SIZE, GFP_KERNEL);
	if (!sipc_buf) {
		pr_err(SIPC_PRI "get new recv buf failed\n");
		ret = -ENOMEM;
		goto fail_recvbuf;
	}
	wlan_sipc->recv_buf = sipc_buf;

	sipc_buf = kzalloc(WLAN_EVENT_SBLOCK_SIZE, GFP_KERNEL);
	if (!sipc_buf) {
		pr_err(SIPC_PRI "get new recv buf failed\n");
		ret = -ENOMEM;
		goto fail_eventbuf;
	}
	wlan_sipc->event_buf = sipc_buf;

	spin_lock_init(&wlan_sipc->lock);
	mutex_init(&wlan_sipc->cmd_lock);
	mutex_init(&wlan_sipc->pm_lock);

	sprdwl_priv->wlan_sipc = wlan_sipc;
#ifndef CONFIG_OF
	ret =
	    sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
				     wlan_sipc_sblock_handler, sprdwl_priv);
	if (ret) {
		pr_err("Failed to regitster event sblock notifier (%d)\n", ret);
		ret = -ENOMEM;
		goto fail_notifier;
	}
#endif

	return 0;

#ifndef CONFIG_OF
fail_notifier:
	sprdwl_priv->wlan_sipc = NULL;
#endif
fail_eventbuf:
	kfree(wlan_sipc->recv_buf);
fail_recvbuf:
	kfree(wlan_sipc->send_buf);
fail_sendbuf:
	kfree(wlan_sipc);
	return ret;
}

void sprdwl_sipc_free(struct sprdwl_priv *sprdwl_priv)
{
#ifndef CONFIG_OF
	int ret;
	ret = sblock_register_notifier(WLAN_CP_ID,
				       WLAN_EVENT_SBLOCK_CH, NULL, NULL);
	if (ret)
		pr_err("%s regitster event sblock notifier (%d) err\n",
		       __func__, ret);
#endif
	kfree(sprdwl_priv->wlan_sipc->send_buf);
	kfree(sprdwl_priv->wlan_sipc->recv_buf);
	kfree(sprdwl_priv->wlan_sipc);
	sprdwl_priv->wlan_sipc = NULL;
}

int sprdwl_get_ip_cmd(struct sprdwl_priv *sprdwl_priv, u8 *ip)
{
	int ret;
	u16 status;
	struct wlan_sipc *wlan_sipc = sprdwl_priv->wlan_sipc;
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;

	if (ip != NULL)
		memcpy(send_buf->u.cmd.variable, ip, 4);
	else
		memset(send_buf->u.cmd.variable, 0, 4);

	pr_debug("enter get ip send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, SPRDWL_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_LINK_STATUS);

	if (ret) {
		pr_err(SIPC_PRI "get ip cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_LINK_STATUS);
	if (ret) {
		pr_err(SIPC_PRI "get ip cmd recv error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_pm_enter_ps_cmd(struct sprdwl_priv *sprdwl_priv)
{
	int ret;
	struct wlan_sipc *wlan_sipc = sprdwl_priv->wlan_sipc;
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct in_device *ip = (struct in_device *)sprdwl_priv->ndev->ip_ptr;
	struct in_ifaddr *in = ip->ifa_list;

	if (in != NULL)
		memcpy(send_buf->u.cmd.variable, &in->ifa_address, 4);

	pr_debug(SIPC_PRI "enter ps cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, SPRDWL_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_PM_ENTER_PS);

	if (ret) {
		pr_err(SIPC_PRI "cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_pm_exit_ps_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;

	pr_debug(SIPC_PRI "exit ps cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EXIT_PS);

	if (ret) {
		pr_err(SIPC_PRI "cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_pm_early_suspend_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;
	u16 status;

	pr_debug(SIPC_PRI "early suspend cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EARLY_SUSPEND);

	if (ret) {
		pr_err(SIPC_PRI "cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_EARLY_SUSPEND);
	if (ret) {
		pr_err(SIPC_PRI "early suspend cmd recv error with ret is %d\n",
		       ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);
	return 0;
}

int sprdwl_pm_later_resume_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;
	u16 status;

	pr_debug(SIPC_PRI "later resume cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_LATER_RESUME);

	if (ret) {
		pr_err(SIPC_PRI "cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_LATER_RESUME);
	if (ret) {
		pr_err(SIPC_PRI "later resume cmd recv error with ret is %d\n",
		       ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err(SIPC_PRI "%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);
	return 0;
}

int sprdwl_set_regdom_cmd(struct wlan_sipc *wlan_sipc, u8 *regdom, u16 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_regdom *regdom_ptr =
	    (struct wlan_sipc_regdom *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	regdom_ptr->len = len;
	memcpy(regdom_ptr->value, regdom, len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(regdom_ptr->len) + len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_REGDOM);

	if (ret) {
		pr_err(SIPC_PRI "command error %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

void sprdwl_get_ap_time(u8 *ts)
{
	unsigned long long t;
	unsigned long ms;

	/*Get ap time*/
	t = cpu_clock(smp_processor_id());
	ms = do_div(t, 1000000000);
	ms /= 1000000;
	memcpy(ts, &t, 4);
	memcpy(ts + 4, &ms, 4);
}
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
int sprdwl_set_scan_chan_cmd(struct wlan_sipc *wlan_sipc,
			     u8 *channel_info, int len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug("set channels!\n");

	mutex_lock(&wlan_sipc->cmd_lock);

	if (channel_info && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, channel_info, len);
		send_len = len;
	}

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret =
	    sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				 WIFI_CMD_SCAN_CHANNELS);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int sprdwl_set_tx_mgmt_cmd(struct wlan_sipc *wlan_sipc,
			   struct ieee80211_channel *channel,
			   unsigned int wait, const u8 *mac, size_t mac_len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_mgmt_tx *mgmt_tx =
	    (struct wlan_sipc_mgmt_tx *)send_buf->u.cmd.variable;

	int ret;
	u8 send_chan;

	send_chan = ieee80211_frequency_to_channel(channel->center_freq);

	mutex_lock(&wlan_sipc->cmd_lock);

	mgmt_tx->chan = send_chan;
	mgmt_tx->wait = wait;
	mgmt_tx->len = mac_len;
	memcpy(mgmt_tx->value, mac, mac_len);

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_mgmt_tx) + mac_len;
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_TX_MGMT);

	if (ret) {
		pr_err(SIPC_PRI "tx_mgmt return wrong status code is %d\n",
		       ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	pr_debug(SIPC_PRI "set tx mgmt return status code successfully\n");

	return 0;
}

int sprdwl_cancel_remain_chan_cmd(struct wlan_sipc *wlan_sipc, u64 cookie)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_cancel_remain_chan *cancel_remain_chan =
	    (struct wlan_sipc_cancel_remain_chan *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	cancel_remain_chan->cookie = cookie;

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_cancel_remain_chan);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				   WIFI_CMD_CANCEL_REMAIN_CHAN);

	if (ret) {
		pr_err(SIPC_PRI
		       "cancel remain on_chan return wrong status code is %d\n",
		       ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);
	pr_debug(SIPC_PRI
		 "cancel remain channel return status code successfully\n");

	return 0;
}

int sprdwl_remain_chan_cmd(struct wlan_sipc *wlan_sipc,
			   struct ieee80211_channel *channel,
			   enum nl80211_channel_type channel_type,
			   unsigned int duration, u64 *cookie)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_remain_chan *remain_chan =
	    (struct wlan_sipc_remain_chan *)send_buf->u.cmd.variable;
	int ret;
	u8 send_chan;

	send_chan = ieee80211_frequency_to_channel(channel->center_freq);

	mutex_lock(&wlan_sipc->cmd_lock);

	remain_chan->chan = send_chan;
	remain_chan->chan_type = channel_type;
	remain_chan->duraion = duration;
	remain_chan->cookie = *cookie;

	wlan_sipc->wlan_sipc_send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_remain_chan);
	wlan_sipc->wlan_sipc_recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				   WIFI_CMD_REMAIN_CHAN);

	if (ret) {
		pr_err(SIPC_PRI "remain_chan return wrong status code is %d\n",
		       ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	pr_debug(SIPC_PRI "remain channel return status code successfully\n");

	return 0;
}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

