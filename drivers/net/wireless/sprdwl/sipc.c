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
#ifdef CONFIG_SPRDWL_RX_IRQ
#include "ringbuf.h"
#endif

#define SIPC_PRI	"sprdwl:"
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
int wlan_sipc_cmd_send(struct wlan_sipc *sipc, u16 len, u8 type, u8 id)
{
	struct wlan_sipc_data *send_buf =
	    (struct wlan_sipc_data *)sipc->send_buf;
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
int wlan_sipc_cmd_receive(struct wlan_sipc *sipc, u16 len, u8 id)
{
	struct wlan_sipc_data *recv_buf =
	    (struct wlan_sipc_data *)sipc->recv_buf;
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

int sprdwl_cmd_send_recv(struct wlan_sipc *sipc, u8 type, u8 id)
{
	u16 status;
	int ret = 0;

	if (!mutex_trylock(&sipc->pm_lock)) {
		pr_err(SIPC_PRI "cmd [%d %d] could not get pm mutex\n",
		       id, type);
		ret = -EBUSY;
		return ret;
	}

	/* TODO sipc-sbuf ops */
	ret = sbuf_read(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			sipc->recv_buf, WLAN_SBUF_SIZE, 0);
	if (ret > 0) {
		pr_err(SIPC_PRI "cmd [%d %d] clean dirty data,len:%d<%d\n",
		       id, type, ret, WLAN_SBUF_SIZE);
	} else if (ret != -ENODATA) {
		pr_err(SIPC_PRI "cmd [%d %d] sbuf recv channel error(%d)\n",
		       id, type, ret);
		goto out;
	}

	ret = wlan_sipc_cmd_send(sipc, sipc->send_len, type, id);

	if (ret) {
		pr_err(SIPC_PRI "cmd [%d %d] send error %d\n", id, type, ret);
		goto out;
	}

	ret = wlan_sipc_cmd_receive(sipc, sipc->recv_len, id);

	if (ret) {
		pr_err(SIPC_PRI "cmd [%d %d] recv error %d\n", id, type, ret);
		goto out;
	}

	status = sipc->recv_buf->u.cmd_resp.status_code;

	/* FIXME consider return status */
	if (status) {
		pr_err(SIPC_PRI "cmd [%d %d] status error %d\n",
		       id, type, status);
		mutex_unlock(&sipc->pm_lock);
		return status;
	}

out:
	mutex_unlock(&sipc->pm_lock);
	return ret;
}

int sprdwl_scan_cmd(struct wlan_sipc *sipc, const u8 *ssid, int len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug("trigger scan\n");

	mutex_lock(&sipc->cmd_lock);

	if (ssid && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, ssid, len);
		send_len = len;
	}

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_SCAN);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_wpa_version_cmd(struct wlan_sipc *sipc, u32 wpa_version)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u32 version = wpa_version;
	int ret;

	if (wpa_version == 0) {
		/*Disable */
		pr_debug("set wpa_version is zero\n");
		return 0;
	}

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &version, sizeof(version));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(version);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_WPA_VERSION);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_auth_type_cmd(struct wlan_sipc *sipc, u32 type)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u32 auth_type = type;
	int ret;

	mutex_lock(&sipc->cmd_lock);
	memcpy(send_buf->u.cmd.variable, &auth_type, sizeof(auth_type));

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(auth_type);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_AUTH_TYPE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

/* unicast cipher or group cipher */
int sprdwl_set_cipher_cmd(struct wlan_sipc *sipc, u32 cipher, u8 cmd_id)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u32 chipher_val = cipher;
	u8 id = cmd_id;
	int ret;

	if ((id != WIFI_CMD_PAIRWISE_CIPHER) && (id != WIFI_CMD_GROUP_CIPHER)) {
		pr_err("not support this type cipher: %d\n", id);
		return -EINVAL;
	}

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &chipher_val, sizeof(chipher_val));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(chipher_val);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, cmd_id);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_key_management_cmd(struct wlan_sipc *sipc, u8 key_mgmt)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u8 key = key_mgmt;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key, sizeof(key));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(key);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_AKM_SUITE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_get_device_mode_cmd(struct wlan_sipc *sipc)
{
	s32 device_mode;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE + sizeof(device_mode);

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_DEV_MODE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&device_mode, sipc->recv_buf->u.cmd_resp.variable,
	       sizeof(device_mode));

	mutex_unlock(&sipc->cmd_lock);

	return device_mode;
}

int sprdwl_pmksa_cmd(struct wlan_sipc *sipc, const u8 *bssid,
		       const u8 *pmkid, u8 type)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_pmkid *cmd;
	int ret;

	if (bssid == NULL) {
		pr_err("pmksa->bssid is null\n");
		return -EINVAL;
	}

	if ((type != CMD_TYPE_FLUSH) && pmkid == NULL) {
		pr_err("pmksa->pmkid is null\n");
		return -EINVAL;
	}

	mutex_lock(&sipc->cmd_lock);

	cmd = (struct wlan_sipc_pmkid *)send_buf->u.cmd.variable;
	memcpy(cmd->bssid, bssid, sizeof(cmd->bssid));
	if (pmkid)
		memcpy(cmd->pmkid, pmkid, sizeof(cmd->pmkid));

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_pmkid);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, type, WIFI_CMD_PMKSA);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_psk_cmd(struct wlan_sipc *sipc, const u8 *key, u32 key_len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	if (key_len == 0) {
		pr_err("PSK length is 0\n");
		return 0;
	}

	if (key_len > 32) {
		pr_err("PSK length is larger than 32\n");
		return -EINVAL;
	}

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, key, key_len);
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + key_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_PSK);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_channel_cmd(struct wlan_sipc *sipc, u32 channel)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u32 channel_num = channel;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &channel_num, sizeof(channel_num));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(channel_num);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_CHANNEL);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_bssid_cmd(struct wlan_sipc *sipc, const u8 *addr)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, ETH_ALEN);
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + ETH_ALEN;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_BSSID);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_essid_cmd(struct wlan_sipc *sipc,
			 const u8 *essid, int essid_len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, essid, essid_len);
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + essid_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_ESSID);

	if (ret) {
		pr_err("SIPC_PRI %s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_disconnect_cmd(struct wlan_sipc *sipc, u16 reason_code)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &reason_code, sizeof(reason_code));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(reason_code);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_DISCONNECT);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_add_key_cmd(struct wlan_sipc *sipc, const u8 *key_data,
		       u8 key_len, u8 pairwise, u8 key_index,
		       const u8 *key_seq, u8 cypher_type, const u8 *pmac)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_add_key *add_key =
	    (struct wlan_sipc_add_key *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	if (pmac != NULL)
		memcpy(add_key->mac, pmac, sizeof(add_key->mac));
	if (key_seq != NULL)
		memcpy(add_key->keyseq, key_seq, sizeof(add_key->keyseq));
	add_key->pairwise = pairwise;
	add_key->cypher_type = cypher_type;
	add_key->key_index = key_index;
	add_key->key_len = key_len;
	memcpy(add_key->value, key_data, key_len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + key_len
	    + sizeof(struct wlan_sipc_add_key);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_ADD, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_del_key_cmd(struct wlan_sipc *sipc, u16 key_index,
		       const u8 *mac_addr)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_del_key *del_key =
	    (struct wlan_sipc_del_key *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	if (mac_addr != NULL)
		memcpy(del_key->mac, mac_addr, sizeof(del_key->mac));
	del_key->key_index = key_index;

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_del_key);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_DEL, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_key_cmd(struct wlan_sipc *sipc, u8 key_index)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key_index, sizeof(key_index));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(key_index);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_rts_cmd(struct wlan_sipc *sipc, u16 rts_threshold)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u16 threshold = rts_threshold;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(threshold);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_RTS_THRESHOLD);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_frag_cmd(struct wlan_sipc *sipc, u16 frag_threshold)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u16 threshold = frag_threshold;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(threshold);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_FRAG_THRESHOLD);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_get_rssi_cmd(struct wlan_sipc *sipc, s8 *signal, s8 *noise)
{
	struct wlan_sipc_data *recv_buf = sipc->recv_buf;
	u32 rssi;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE + sizeof(rssi);
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_GET, WIFI_CMD_RSSI);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&rssi, recv_buf->u.cmd_resp.variable, sizeof(rssi));
	*signal = (u8) (le32_to_cpu(rssi) | 0xffff0000);
	*noise = (u8) ((le32_to_cpu(rssi) | 0x0000ffff) >> 16);

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_get_txrate_cmd(struct wlan_sipc *sipc, s32 *rate)
{
	struct wlan_sipc_data *recv_buf = sipc->recv_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE + sizeof(*rate);
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_GET, WIFI_CMD_TXRATE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	memcpy(rate, recv_buf->u.cmd_resp.variable, sizeof(*rate));
	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_start_ap_cmd(struct wlan_sipc *sipc, u8 *beacon, u16 len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_beacon *beacon_ptr =
	    (struct wlan_sipc_beacon *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	beacon_ptr->len = len;
	memcpy(beacon_ptr->value, beacon, len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(beacon_ptr->len) + len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_START_AP);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_disassoc_cmd(struct wlan_sipc *sipc, u8 *mac, u16 reason_code)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_disassoc *disassoc_ptr =
	    (struct wlan_sipc_disassoc *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	disassoc_ptr->reason_code = reason_code;
	memcpy(disassoc_ptr->mac, mac, ETH_ALEN);

	sipc->send_len =
	    SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_disassoc);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_DISASSOC);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_wps_ie_cmd(struct wlan_sipc *sipc,
			  u8 type, const u8 *ie, u8 len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_wps_ie *wps_ptr =
	    (struct wlan_sipc_wps_ie *)send_buf->u.cmd.variable;
	int ret;

	if (type != WPS_REQ_IE && type != WPS_ASSOC_IE) {
		pr_err("wrong ie type is %d\n", type);
		return -EIO;
	}

	mutex_lock(&sipc->cmd_lock);

	wps_ptr->type = type;
	wps_ptr->len = len;
	memcpy(wps_ptr->value, ie, len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(wps_ptr) + len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_WPS_IE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_blacklist_cmd(struct wlan_sipc *sipc, u8 *addr, u8 flag)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, ETH_ALEN);
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + ETH_ALEN;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	if (flag)
		ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_ADD,
					   WIFI_CMD_BLACKLIST);
	else
		ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_DEL,
					   WIFI_CMD_BLACKLIST);

	if (ret) {
		pr_err("blacklist wrong status code is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	pr_debug("blacklist return status code successfully\n");

	return 0;
}

int sprdwl_get_ip_cmd(struct sprdwl_priv *priv, u8 *ip)
{
	int ret;
	u16 status;
	struct wlan_sipc *sipc = priv->sipc;
	struct wlan_sipc_data *send_buf = sipc->send_buf;

	if (ip != NULL)
		memcpy(send_buf->u.cmd.variable, ip, 4);
	else
		memset(send_buf->u.cmd.variable, 0, 4);

	pr_debug("enter get ip send\n");
	mutex_lock(&sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(sipc, SPRDWL_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_LINK_STATUS);

	if (ret) {
		pr_err("get ip cmd send error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_LINK_STATUS);
	if (ret) {
		pr_err("get ip cmd recv error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	status = sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}
	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

/*int sprdwl_pm_enter_ps_cmd(struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_priv *priv = vif->priv;
	struct wlan_sipc *sipc = priv->sipc;
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct in_device *ip = (struct in_device *)ndev->ip_ptr;
	struct in_ifaddr *in = ip->ifa_list;
	int ret;

	if (in != NULL)
		memcpy(send_buf->u.cmd.variable, &in->ifa_address, 4);

	pr_debug("enter ps cmd send\n");
	mutex_lock(&sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(sipc, SPRDWL_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_PM_ENTER_PS);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_pm_exit_ps_cmd(struct wlan_sipc *sipc)
{
	int ret;

	pr_debug("exit ps cmd send\n");
	mutex_lock(&sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EXIT_PS);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&sipc->cmd_lock);

	return 0;
}
*/

int sprdwl_pm_early_suspend_cmd(struct wlan_sipc *sipc)
{
	int ret;
	u16 status;

	pr_debug("early suspend cmd send\n");
	mutex_lock(&sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EARLY_SUSPEND);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_EARLY_SUSPEND);
	if (ret) {
		pr_err("early suspend cmd recv error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	status = sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);
	return 0;
}

int sprdwl_pm_later_resume_cmd(struct wlan_sipc *sipc)
{
	int ret;
	u16 status;

	pr_debug("later resume cmd send\n");
	mutex_lock(&sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(sipc, SPRDWL_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_LATER_RESUME);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(sipc, SPRDWL_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_LATER_RESUME);
	if (ret) {
		pr_err("later resume cmd recv error with ret is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return ret;
	}

	status = sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);
	return 0;
}

int sprdwl_set_cqm_rssi(struct wlan_sipc *sipc, s32 rssi_thold, u32 rssi_hyst)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_cqm_rssi *cmd;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	cmd = (struct wlan_sipc_cqm_rssi *)send_buf->u.cmd.variable;
	cmd->rssih = rssi_thold;
	cmd->rssil = rssi_hyst;

	sipc->send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_cqm_rssi);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_SET_CQM_RSSI);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_regdom_cmd(struct wlan_sipc *sipc, u8 *regdom, u16 len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_regdom *regdom_ptr =
	    (struct wlan_sipc_regdom *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	regdom_ptr->len = len;
	memcpy(regdom_ptr->value, regdom, len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(regdom_ptr->len) + len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_REGDOM);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

#ifdef CONFIG_SPRDWL_POWER_CONTROL
int sprdwl_set_power_control_cmd(struct wlan_sipc *sipc, u8 value, u8 reason)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_power_control power_control;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	power_control.power_control_enable = value;
	power_control.reason = reason;
	memcpy(send_buf->u.cmd.variable, &power_control, sizeof(power_control));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(power_control);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc,
				   CMD_TYPE_SET, WIFI_CMD_SET_POWER_CONTROL);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);
	return 0;
}
#endif

int sprdwl_mac_open_cmd(struct wlan_sipc *sipc, u8 mode, u8 *mac_addr)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_mac_open *open =
	    (struct wlan_sipc_mac_open *)send_buf->u.cmd.variable;
	int ret;
#ifdef CONFIG_SPRDWL_RX_IRQ
	uint32_t addr;
#endif

	sprdwl_nvm_init();

	mutex_lock(&sipc->cmd_lock);

	open->mode = mode;
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	open->mode |= FW_ZEROCOPY;
#endif
	if (mac_addr)
		memcpy(open->mac, mac_addr, sizeof(open->mac));
	memcpy((unsigned char *)(&(open->nvm_data)),
	       (unsigned char *)(get_gwifi_nvm_data()),
	       sizeof(struct wifi_nvm_data));
#ifdef CONFIG_SPRDWL_RX_IRQ
	ringbuf_getbase_addr(&addr);
	pr_err("%s base addr 0x%x\n", __func__, addr);
	memcpy(open->address, &addr, sizeof(addr));
#endif
	sipc->send_len =
	    SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_mac_open);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	pr_debug("%s mode %#x cmd_len %d\n", __func__, open->mode,
		 sipc->send_len);
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_DEV_OPEN);
	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		switch (ret) {
		case ERR_AP_ZEROCOPY_CP_NOT:
			pr_err("%s AP: zero copy; CP: non zero copy\n",
			       __func__);
			break;
		case ERR_CP_ZEROCOPY_AP_NOT:
			pr_err("%s AP: non zero copy; CP: zero copy\n",
			       __func__);
			break;
		default:
			break;
		}
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_mac_close_cmd(struct wlan_sipc *sipc, u8 mode)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	u8 flag = mode;
	int ret;

	mutex_lock(&sipc->cmd_lock);
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	flag |= FW_ZEROCOPY;
#endif
	memcpy(send_buf->u.cmd.variable, &flag, sizeof(flag));
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(flag);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	pr_debug("%s mode %#x\n", __func__, flag);
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_DEV_CLOSE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
int sprdwl_set_p2p_enable_cmd(struct wlan_sipc *sipc, int enable)
{
	struct wlan_sipc_p2p_enable p2p_enable;
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int ret;

	pr_debug("%s:set p2p_enable %d\n", __func__, enable);

	mutex_lock(&sipc->cmd_lock);

	p2p_enable.p2p_enable = enable;
	memcpy(send_buf->u.cmd.variable, &p2p_enable, sizeof(p2p_enable));

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(p2p_enable);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_P2P_ENABLE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_scan_chan_cmd(struct wlan_sipc *sipc, u8 *channel_info, int len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug("set channels!\n");

	mutex_lock(&sipc->cmd_lock);

	if (channel_info && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, channel_info, len);
		send_len = len;
	}

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_SCAN_CHANNELS);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	return 0;
}

int sprdwl_set_p2p_ie_cmd(struct wlan_sipc *sipc,
			  u8 type, const u8 *ie, u8 len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_p2p_ie *p2p_ptr =
	    (struct wlan_sipc_p2p_ie *)send_buf->u.cmd.variable;
	int ret;

	if (type != P2P_ASSOC_IE && type != P2P_BEACON_IE &&
	    type != P2P_PROBERESP_IE && type != P2P_ASSOCRESP_IE &&
	    type != P2P_BEACON_IE_HEAD && type != P2P_BEACON_IE_TAIL) {
		pr_err("wrong ie type is %d\n", type);
		return -EIO;
	}

	mutex_lock(&sipc->cmd_lock);

	p2p_ptr->type = type;
	p2p_ptr->len = len;
	memcpy(p2p_ptr->value, ie, len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_p2p_ie) + len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_P2P_IE);

	if (ret) {
		pr_err("return wrong status code is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	pr_debug("set p2p ie return status code successfully\n");

	return 0;
}

int sprdwl_set_tx_mgmt_cmd(struct wlan_sipc *sipc,
			   struct ieee80211_channel *channel,
			   u8 dont_wait_for_ack, u32 wait, u64 *cookie,
			   const u8 *mac, size_t mac_len)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_mgmt_tx *mgmt_tx =
	    (struct wlan_sipc_mgmt_tx *)send_buf->u.cmd.variable;

	int ret;
	u8 send_chan;

	send_chan = ieee80211_frequency_to_channel(channel->center_freq);

	mutex_lock(&sipc->cmd_lock);

	mgmt_tx->chan = send_chan;
	mgmt_tx->dont_wait_for_ack = dont_wait_for_ack;
	mgmt_tx->wait = wait;
	mgmt_tx->cookie = *cookie;
	mgmt_tx->len = mac_len;
	memcpy(mgmt_tx->value, mac, mac_len);

	sipc->send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_mgmt_tx) + mac_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_TX_MGMT);

	if (ret) {
		pr_err("tx_mgmt return wrong status code is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	pr_debug("set tx mgmt return status code successfully\n");

	return 0;
}

int sprdwl_cancel_remain_chan_cmd(struct wlan_sipc *sipc, u64 cookie)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_cancel_remain_chan *cancel_remain_chan =
	    (struct wlan_sipc_cancel_remain_chan *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	cancel_remain_chan->cookie = cookie;

	sipc->send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_cancel_remain_chan);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET,
				   WIFI_CMD_CANCEL_REMAIN_CHAN);

	if (ret) {
		pr_err("cancel remain on_chan return wrong status code is %d\n",
		       ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);
	pr_debug("cancel remain channel return status code successfully\n");

	return 0;
}

int sprdwl_remain_chan_cmd(struct wlan_sipc *sipc,
			   struct ieee80211_channel *channel,
			   enum nl80211_channel_type channel_type,
			   unsigned int duration, u64 *cookie)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_remain_chan *remain_chan =
	    (struct wlan_sipc_remain_chan *)send_buf->u.cmd.variable;
	int ret;
	u8 send_chan;

	send_chan = ieee80211_frequency_to_channel(channel->center_freq);

	mutex_lock(&sipc->cmd_lock);

	remain_chan->chan = send_chan;
	remain_chan->chan_type = channel_type;
	remain_chan->duraion = duration;
	remain_chan->cookie = *cookie;

	sipc->send_len = SPRDWL_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_remain_chan);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_REMAIN_CHAN);

	if (ret) {
		pr_err("remain_chan return wrong status code is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);

	pr_debug("remain channel return status code successfully\n");

	return 0;
}

int sprdwl_set_ap_sme_cmd(struct wlan_sipc *sipc, u8 value)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_ap_sme *ap_sme_ptr =
	    (struct wlan_sipc_ap_sme *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	ap_sme_ptr->value = value;
	sipc->send_len = SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_ap_sme);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;
	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_SET_AP_SME);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&sipc->cmd_lock);
	return 0;
}

void register_frame_work_func(struct work_struct *work)
{
	struct sprdwl_vif *vif = container_of(work, struct sprdwl_vif, work);
	struct wlan_sipc *sipc = vif->priv->sipc;
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_register_frame *data =
	    (struct wlan_sipc_register_frame *)send_buf->u.cmd.variable;
	int ret = 0;

	mutex_lock(&sipc->cmd_lock);

	data->type = vif->frame_type;
	data->reg = vif->reg ? 1 : 0;
	vif->frame_type = 0xffff;
	vif->reg = 0;

	sipc->send_len =
	    SPRDWL_CMD_HDR_SIZE + sizeof(struct wlan_sipc_register_frame);
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE;

	ret = sprdwl_cmd_send_recv(sipc, CMD_TYPE_SET, WIFI_CMD_REGISTER_FRAME);

	if (ret)
		pr_err("wrong status code(%d)!\n", ret);

	mutex_unlock(&sipc->cmd_lock);
}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

/* EVENT SBLOCK implementation */
static void wlan_sipc_event_rx_handler(struct sprdwl_priv *priv)
{
	struct wlan_sipc_data *recv_buf = priv->sipc->event_buf;
	struct sblock blk;
	struct sprdwl_vif *vif;
	u32 msg_hdr;
	u16 msg_type, msg_len;
	u16 event_tag;
/*      u16 seq_no;*/
	u8 event_type;
	u8 event_id;
	int ret;

	ret = sblock_receive(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk, 0);
	if (ret) {
		pr_err("Failed to receive sblock (%d)\n", ret);
		return;
	}

	memcpy((u8 *)recv_buf, (u8 *)blk.addr, blk.length);

	msg_hdr = le32_to_cpu(recv_buf->msg_hdr);
	event_tag = le16_to_cpu(recv_buf->u.event.event_tag);
/*      seq_no  = le16_to_cpu(recv_buf->u.event_tag.seq_no);*/

	msg_type = SPRDWL_MSG_GET_TYPE(msg_hdr);
	msg_len = SPRDWL_MSG_GET_LEN(msg_hdr);

	event_type = SPRDWL_EVENT_TAG_GET_TYPE(event_tag);
	event_id = SPRDWL_EVENT_TAG_GET_ID(event_tag);

	priv->sipc->event_len = msg_len - SPRDWL_EVENT_HDR_SIZE;

	if ((msg_len != blk.length) || (msg_type != SPRDWL_MSG_EVENT)) {
		pr_err
		    ("Recv msg_len(%d), blk.length(%d) and type(%d) is wrong\n",
		     msg_len, blk.length, msg_type);
		goto out;
	}

	if (event_type == EVENT_TYPE_SYSERR)
		pr_err("Recv event type is syserr\n");

#ifdef DUMP_EVENT
	if (event_id != WIFI_EVENT_SCANDONE) {
		print_hex_dump(KERN_DEBUG, "event: ", DUMP_PREFIX_OFFSET,
			       16, 1, blk.addr, blk.length, 0);
	}
#endif

	vif = sprdwl_get_report_vif(priv);
	switch (event_id) {
	case WIFI_EVENT_SCANDONE:
		pr_debug("Recv sblock8 scan result event\n");
		sprdwl_event_scan_results(priv, 0);
		break;
	case WIFI_EVENT_INTERNAL_BSS_INFO:
		pr_debug("Recv sblock8 internal bss info event\n");
		sprdwl_event_scan_results(priv, 1);
		break;
	case WIFI_EVENT_CONNECT:
		pr_debug("Recv sblock8 connect event\n");
		sprdwl_event_connect_result(priv->cur_vif);
		break;
	case WIFI_EVENT_DISCONNECT:
		pr_debug("Recv sblock8 disconnect event\n");
		sprdwl_event_disconnect(vif);
		break;
	case WIFI_EVENT_READY:
		pr_debug("Recv sblock8 CP2 ready event\n");
		sprdwl_event_ready(priv);
		break;
	case WIFI_EVENT_TX_BUSY:
		pr_debug("Recv data tx sblock busy event\n");
		sprdwl_event_tx_busy(vif);
		break;
	case WIFI_EVENT_SOFTAP:
		pr_debug("Recv sblock8 softap event\n");
		sprdwl_event_softap(vif);
		break;
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	case WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED:
		pr_debug("Recv p2p  remain on channel EXPIRED event\n");
		sprdwl_event_remain_on_channel_expired(priv->p2p_vif);
		break;
	case WIFI_EVENT_MGMT_DEAUTH:
		pr_debug("Recv p2p deauth event\n");
		sprdwl_event_mgmt_deauth(priv->p2p_vif);
		break;
	case WIFI_EVENT_MGMT_DISASSOC:
		pr_debug("Recv p2p disassoc event\n");
		sprdwl_event_mgmt_disassoc(priv->p2p_vif);
		break;
	case WIFI_EVENT_REPORT_FRAME:
		pr_debug("Recv p2p EVENT_REPORT_FRAME\n");
		sprdwl_event_report_frame(priv->p2p_vif);
		break;
	case WIFI_EVENT_MLME_TX_STATUS:
		pr_debug("Recv p2p EVENT_MLME_TX_STATUS\n");
		sprdwl_event_mlme_tx_status(priv->p2p_vif);
		break;
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */
	case WIFI_EVENT_REPORT_MIC_FAIL:
		sprdwl_event_report_mic_failure(vif);
		break;
	case WIFI_EVENT_REPORT_CQM_RSSI_LOW:
		vif->cqm = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		sprdwl_event_report_cqm(vif);
		break;
	case WIFI_EVENT_REPORT_CQM_RSSI_HIGH:
		vif->cqm = NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH;
		sprdwl_event_report_cqm(vif);
		break;
	/* Walk around this event because wpa_supplicant doesn't support it */
	case WIFI_EVENT_REPORT_CQM_RSSI_LOSS_BEACON:
		vif->cqm = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		vif->beacon_loss = 1;
		sprdwl_event_report_cqm(vif);
		break;
	default:
		pr_err("Recv sblock8 unknow event id %d\n", event_id);
		break;
	}

out:
	ret = sblock_release(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk);
	if (ret)
		pr_err("release sblock failed (%d)\n", ret);
}

void wlan_sipc_sblock_handler(int event, void *data)
{
	struct sprdwl_priv *priv = (struct sprdwl_priv *)data;

	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		pr_debug("SBLOCK_NOTIFY_RECV is received\n");
		wlan_sipc_event_rx_handler(priv);
		break;
	case SBLOCK_NOTIFY_GET:
	case SBLOCK_NOTIFY_STATUS:
		break;
	default:
		pr_err("Received event is invalid(event=%d)\n", event);
	}
}

/* TODO -- add sbuf register and free */
int sprdwl_alloc_sipc_buf(struct sprdwl_priv *priv)
{
	struct wlan_sipc *sipc;

	sipc = kzalloc(sizeof(struct wlan_sipc), GFP_KERNEL);
	if (!sipc) {
		pr_err("failed to alloc wlan_sipc struct\n");
		return -ENOMEM;
	}

	sipc->send_buf = kzalloc(WLAN_SBUF_SIZE, GFP_KERNEL);
	if (!sipc->send_buf) {
		pr_err("get new send buf failed\n");
		goto fail_sendbuf;
	}

	sipc->recv_buf = kzalloc(WLAN_SBUF_SIZE, GFP_KERNEL);
	if (!sipc->recv_buf) {
		pr_err("get new recv buf failed\n");
		goto fail_recvbuf;
	}

	sipc->event_buf = kzalloc(WLAN_EVENT_SBLOCK_SIZE, GFP_KERNEL);
	if (!sipc->event_buf) {
		pr_err("get new event buf failed\n");
		goto fail_eventbuf;
	}
	spin_lock_init(&sipc->lock);
	mutex_init(&sipc->cmd_lock);
	mutex_init(&sipc->pm_lock);

	priv->sipc = sipc;

	return 0;

fail_eventbuf:
	kfree(sipc->recv_buf);
fail_recvbuf:
	kfree(sipc->send_buf);
fail_sendbuf:
	kfree(sipc);
	return -ENOMEM;
}

void sprdwl_free_sipc_buf(struct sprdwl_priv *priv)
{
	kfree(priv->sipc->send_buf);
	kfree(priv->sipc->recv_buf);
	kfree(priv->sipc->event_buf);
	kfree(priv->sipc);
	priv->sipc = NULL;
}
