/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : itm_sipc.c
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * Authors	:
 * Leon Liu <leon.liu@spreadtrum.com>
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 * Danny Deng <danny.deng@spreadtrum.com>
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

#include "sipc_types.h"
#include "sipc.h"
#include "cfg80211.h"

extern WIFI_nvm_data *get_gWIFI_nvm_data(void);

/* sbuf should be created and initilized */
int wlan_sipc_cmd_send(struct wlan_sipc *wlan_sipc, u16 len, u8 type, u8 id)
{
	struct wlan_sipc_data *send_buf =
	    (struct wlan_sipc_data *)wlan_sipc->send_buf;
	u32 msg_hdr;
	u16 cmd_tag;
	int send_len;

	msg_hdr = ITM_WLAN_MSG(ITM_WLAN_MSG_CMD, len);
	cmd_tag = ITM_WLAN_CMD_TAG(type, CMD_DIR_AP2CP, CMD_LEVEL_NORMAL, id);

	send_buf->msg_hdr = cpu_to_le32(msg_hdr);
	send_buf->u.cmd.cmd_tag = cpu_to_le16(cmd_tag);
/*      send_buf->u.seq_no = cpu_to_1e16(seq_no);*/

	/* sipc-sbuf ops */
	send_len = sbuf_write(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			      send_buf, len,
			      msecs_to_jiffies(DEFAULT_WAIT_SBUF_TIME));

	if (send_len < 0) {
		pr_err("sbuf_write error (%d)\n", send_len);
		return send_len;
	} else if (send_len != len) {
		pr_err("sbuf_write not completely with send len %d\n",
		       send_len);
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
		pr_err("sbuf_read error (%d)\n", recv_len);
		return recv_len;
	} else if (recv_len != len) {
		pr_err("sbuf_read not completely with recv len %d\n", recv_len);
		return -EIO;
	}
#ifdef DUMP_COMMAND_RESPONSE
	print_hex_dump(KERN_DEBUG, "cmd rsp: ", DUMP_PREFIX_OFFSET,
		       16, 1, recv_buf, recv_len, 0);
#endif

	msg_hdr = le32_to_cpu(recv_buf->msg_hdr);
	cmd_tag = le16_to_cpu(recv_buf->u.cmd_resp.cmd_tag);
/*      seq_no  = le16_to_cpu(recv_buf->u.cmd_resp.seq_no);*/

	msg_type = ITM_WLAN_MSG_GET_TYPE(msg_hdr);
	msg_len = ITM_WLAN_MSG_GET_LEN(msg_hdr);

	cmd_dir = ITM_WLAN_CMD_TAG_GET_DIR(cmd_tag);
	cmd_id = ITM_WLAN_CMD_TAG_GET_ID(cmd_tag);

	if ((msg_len != recv_len) || (msg_type != ITM_WLAN_MSG_CMD_RSP)) {
		pr_err("msg_len is not expected or msg_type is not resp cmd\n");
		return -EIO;
	}

	if (cmd_dir != CMD_DIR_CP2AP) {
		pr_err("cmd dir is not CP to AP\n");
		return -EIO;
	}

	if (cmd_id != id) {
		pr_err("cmd recv wrong cmd id %d\n", cmd_id);
		return -EIO;
	}

	return 0;
}

int itm_wlan_cmd_send_recv(struct wlan_sipc *wlan_sipc, u8 type, u8 id)
{
	u16 status;
	int ret = 0;

	if (!mutex_trylock(&wlan_sipc->pm_lock)) {
		pr_err("cmd could not get pm mutex\n");
		ret = -EBUSY;
		return ret;
	}

	/* TODO sipc-sbuf ops */
	ret = sbuf_read(WLAN_CP_ID, WLAN_SBUF_CH, WLAN_SBUF_ID,
			wlan_sipc->recv_buf, 256, 0);
	if (ret == -ENODATA) {
		pr_debug("do not have dirty data in sbuf\n");
	} else if (ret > 0) {
		pr_err("clean up dirty data in sbuf\n");
	} else {
		pr_err("sbuf recv channel error(%d)\n", ret);
		goto out;
	}

	ret =
	    wlan_sipc_cmd_send(wlan_sipc, wlan_sipc->wlan_sipc_send_len,
			       type, id);

	if (ret) {
		pr_err("cmd send error %d\n", ret);
		goto out;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, wlan_sipc->wlan_sipc_recv_len,
				    id);

	if (ret) {
		pr_err("cmd recv error %d\n", ret);
		goto out;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	/* FIXME consider return status */
	if (status) {
		pr_err("cmd status error %d\n", status);
		mutex_unlock(&wlan_sipc->pm_lock);
		return -EIO;
	}

out:
	mutex_unlock(&wlan_sipc->pm_lock);
	return ret;
}

int itm_wlan_scan_cmd(struct wlan_sipc *wlan_sipc, const u8 *ssid, int len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int send_len = 0;
	int ret;

	pr_debug("enable scan\n");

	mutex_lock(&wlan_sipc->cmd_lock);

	if (ssid && (len > 0)) {
		memcpy(send_buf->u.cmd.variable, ssid, len);
		send_len = len;
	}

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + send_len;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_SCAN);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_wpa_version_cmd(struct wlan_sipc *wlan_sipc, u32 wpa_version)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 version = wpa_version;
	int ret;

	if (wpa_version == 0) {
		/*Disable */
		pr_debug("set wpa_version is zero\n");
		return 0;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &version, sizeof(version));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + sizeof(version);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc,
				     CMD_TYPE_SET, WIFI_CMD_WPA_VERSION);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_auth_type_cmd(struct wlan_sipc *wlan_sipc, u32 type)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 auth_type = type;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);
	memcpy(send_buf->u.cmd.variable, &auth_type, sizeof(auth_type));

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(auth_type);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc,
				     CMD_TYPE_SET, WIFI_CMD_AUTH_TYPE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

/* unicast cipher or group cipher */
int itm_wlan_set_cipher_cmd(struct wlan_sipc *wlan_sipc, u32 cipher, u8 cmd_id)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 chipher_val = cipher;
	u8 id = cmd_id;
	int ret;

	if ((id != WIFI_CMD_PAIRWISE_CIPHER) && (id != WIFI_CMD_GROUP_CIPHER)) {
		pr_err("not support this type cipher: %d\n", id);
		return -EINVAL;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &chipher_val, sizeof(chipher_val));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(chipher_val);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, cmd_id);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_key_management_cmd(struct wlan_sipc *wlan_sipc, u8 key_mgmt)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u8 key = key_mgmt;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key, sizeof(key));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + sizeof(key);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc,
				     CMD_TYPE_SET, WIFI_CMD_AKM_SUITE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_get_device_mode_cmd(struct wlan_sipc *wlan_sipc)
{
	s32 device_mode;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE
	    + sizeof(device_mode);

	ret = itm_wlan_cmd_send_recv(wlan_sipc,
				     CMD_TYPE_SET, WIFI_CMD_DEV_MODE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&device_mode, wlan_sipc->recv_buf->u.cmd_resp.variable,
	       sizeof(device_mode));

	mutex_unlock(&wlan_sipc->cmd_lock);

	return device_mode;
}

int itm_wlan_set_psk_cmd(struct wlan_sipc *wlan_sipc,
			 const u8 *key, u32 key_len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	if (key_len == 0) {
		pr_err("PSK length is 0\n");
		return 0;
	}

	if (key_len > 32) {
		pr_err("PSK length is larger than 32\n");
		return -EINVAL;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, key, key_len);
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + key_len;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_PSK);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_channel_cmd(struct wlan_sipc *wlan_sipc, u32 channel)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u32 channel_num = channel;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &channel_num, sizeof(channel_num));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(channel_num);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_CHANNEL);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_bssid_cmd(struct wlan_sipc *wlan_sipc, const u8 *addr)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, 6);
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + 6;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_BSSID);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_essid_cmd(struct wlan_sipc *wlan_sipc,
			   const u8 *essid, int essid_len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, essid, essid_len);
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + essid_len;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_ESSID);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_pmksa_cmd(struct wlan_sipc *wlan_sipc, const u8 *bssid,
		       const u8 *pmkid, u8 type)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
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

	mutex_lock(&wlan_sipc->cmd_lock);

	cmd = (struct wlan_sipc_pmkid *)send_buf->u.cmd.variable;
	memcpy(cmd->bssid, bssid, sizeof(cmd->bssid));
	if (pmkid)
		memcpy(cmd->pmkid, pmkid, sizeof(cmd->pmkid));

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_pmkid);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, type, WIFI_CMD_PMKSA);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_disconnect_cmd(struct wlan_sipc *wlan_sipc, u16 reason_code)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &reason_code, sizeof(reason_code));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(reason_code);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc,
				     CMD_TYPE_SET, WIFI_CMD_DISCONNECT);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_add_key_cmd(struct wlan_sipc *wlan_sipc,
			 const u8 *key_data, u8 key_len,
			 u8 pairwise, u8 key_index, const u8 *key_seq,
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

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + key_len
	    + sizeof(struct wlan_sipc_add_key);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_ADD, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_del_key_cmd(struct wlan_sipc *wlan_sipc, u16 key_index,
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

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(struct wlan_sipc_del_key);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_DEL, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_key_cmd(struct wlan_sipc *wlan_sipc, u8 key_index)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &key_index, sizeof(key_index));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(key_index);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_KEY);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_rts_cmd(struct wlan_sipc *wlan_sipc, u16 rts_threshold)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u16 threshold = rts_threshold;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(threshold);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				     WIFI_CMD_RTS_THRESHOLD);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_frag_cmd(struct wlan_sipc *wlan_sipc, u16 frag_threshold)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u16 threshold = frag_threshold;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &threshold, sizeof(threshold));

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(threshold);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				     WIFI_CMD_FRAG_THRESHOLD);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_get_rssi_cmd(struct wlan_sipc *wlan_sipc, s8 *signal, s8 *noise)
{
	struct wlan_sipc_data *recv_buf = wlan_sipc->recv_buf;
	u32 rssi;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE
	    + sizeof(rssi);
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_GET, WIFI_CMD_RSSI);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(&rssi, recv_buf->u.cmd_resp.variable, sizeof(rssi));
	*signal = (u8) (le32_to_cpu(rssi) | 0xffff0000);
	*noise = (u8) ((le32_to_cpu(rssi) | 0x0000ffff) >> 16);

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_get_txrate_cmd(struct wlan_sipc *wlan_sipc, s32 *rate)
{
	struct wlan_sipc_data *recv_buf = wlan_sipc->recv_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE
	    + sizeof(*rate);
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_GET, WIFI_CMD_TXRATE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	memcpy(rate, recv_buf->u.cmd_resp.variable, sizeof(*rate));
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_beacon_cmd(struct wlan_sipc *wlan_sipc, u8 *beacon, u16 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_beacon *beacon_ptr =
	    (struct wlan_sipc_beacon *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	beacon_ptr->len = len;
	memcpy(beacon_ptr->value, beacon, len);

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(beacon_ptr->len) + len;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_BEACON);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_wps_ie_cmd(struct wlan_sipc *wlan_sipc,
			    u8 type, const u8 *ie, u8 len)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_wps_ie *wps_ptr =
	    (struct wlan_sipc_wps_ie *)send_buf->u.cmd.variable;
	int ret;

	if (type != WPS_REQ_IE && type != WPS_ASSOC_IE) {
		pr_err("wrong ie type is %d\n", type);
		return -EIO;
	}

	mutex_lock(&wlan_sipc->cmd_lock);

	wps_ptr->type = type;
	wps_ptr->len = len;
	memcpy(wps_ptr->value, ie, len);

	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE
	    + sizeof(wps_ptr) + len;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET, WIFI_CMD_WPS_IE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_set_blacklist_cmd(struct wlan_sipc *wlan_sipc, u8 *addr, u8 flag)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, addr, 6);
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + 6;
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;

	if (flag)
		ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_ADD,
					     WIFI_CMD_BLACKLIST);
	else
		ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_DEL,
					     WIFI_CMD_BLACKLIST);

	if (ret) {
		pr_err("blacklist wrong status code is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	pr_debug("blacklist return status code successfully\n");

	return 0;
}

int itm_wlan_mac_open_cmd(struct wlan_sipc *wlan_sipc, u8 mode, u8 *mac_addr)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct wlan_sipc_mac_open *open =
	    (struct wlan_sipc_mac_open *)send_buf->u.cmd.variable;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	open->mode = mode;
	if (mac_addr)
		memcpy(open->mac, mac_addr, 6);
	memcpy((unsigned char *)(&(open->nvm_data)),
	       (unsigned char *)(get_gWIFI_nvm_data()), sizeof(WIFI_nvm_data));
	wlan_sipc->wlan_sipc_send_len =
	    ITM_WLAN_CMD_HDR_SIZE + sizeof(struct wlan_sipc_mac_open);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				     WIFI_CMD_DEV_OPEN);
	pr_debug("%s(), send cmd len:%d\n", __func__,
		 wlan_sipc->wlan_sipc_send_len);
	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_mac_close_cmd(struct wlan_sipc *wlan_sipc, u8 mode)
{
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	u8 flag = mode;
	int ret;

	mutex_lock(&wlan_sipc->cmd_lock);

	memcpy(send_buf->u.cmd.variable, &flag, sizeof(flag));
	wlan_sipc->wlan_sipc_send_len = ITM_WLAN_CMD_HDR_SIZE + sizeof(flag);
	wlan_sipc->wlan_sipc_recv_len = ITM_WLAN_CMD_RESP_HDR_SIZE;
	ret = itm_wlan_cmd_send_recv(wlan_sipc, CMD_TYPE_SET,
				     WIFI_CMD_DEV_CLOSE);

	if (ret) {
		pr_err("%s command error %d\n", __func__, ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

/* EVENT SBLOCK implementation */
static void wlan_sipc_event_rx_handler(struct itm_priv *priv)
{
	struct sblock blk;
	struct wlan_sipc_data *recv_buf = priv->wlan_sipc->event_buf;
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

	memcpy((u8 *) recv_buf, (u8 *) blk.addr, blk.length);

	msg_hdr = le32_to_cpu(recv_buf->msg_hdr);
	event_tag = le16_to_cpu(recv_buf->u.event.event_tag);
/*      seq_no  = le16_to_cpu(recv_buf->u.event_tag.seq_no);*/

	msg_type = ITM_WLAN_MSG_GET_TYPE(msg_hdr);
	msg_len = ITM_WLAN_MSG_GET_LEN(msg_hdr);

	event_type = ITM_WLAN_EVENT_TAG_GET_TYPE(event_tag);
	event_id = ITM_WLAN_EVENT_TAG_GET_ID(event_tag);

	priv->wlan_sipc->wlan_sipc_event_len =
	    msg_len - ITM_WLAN_EVENT_HDR_SIZE;

	if ((msg_len != blk.length) || (msg_type != ITM_WLAN_MSG_EVENT)) {
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

	switch (event_id) {
	case WIFI_EVENT_CONNECT:
		pr_debug("Recv sblock8 connect event\n");
		itm_cfg80211_report_connect_result(priv);
		break;
	case WIFI_EVENT_DISCONNECT:
		pr_debug("Recv sblock8 disconnect event\n");
		itm_cfg80211_report_disconnect_done(priv);
		break;
	case WIFI_EVENT_SCANDONE:
		pr_debug("Recv sblock8 scan result event\n");
		itm_cfg80211_report_scan_done(priv, false);
		break;
	case WIFI_EVENT_READY:
		pr_debug("Recv sblock8 CP2 ready event\n");
		itm_cfg80211_report_ready(priv);
		break;
	case WIFI_EVENT_TX_BUSY:
		pr_debug("Recv data tx sblock busy event\n");
		itm_cfg80211_report_tx_busy(priv);
		break;
	case WIFI_EVENT_SOFTAP:
		pr_debug("Recv sblock8 softap event\n");
		itm_cfg80211_report_softap(priv);
	default:
		pr_err("Recv sblock8 unknow event id %d\n", event_id);
		break;
	}

out:
	ret = sblock_release(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, &blk);
	if (ret)
		pr_err("release sblock failed (%d)\n", ret);
}

static void wlan_sipc_sblock_handler(int event, void *data)
{
	struct itm_priv *priv = (struct itm_priv *)data;

	switch (event) {
	case SBLOCK_NOTIFY_GET:
		pr_debug("SBLOCK_NOTIFY_GET is received\n");
		break;
	case SBLOCK_NOTIFY_RECV:
		pr_debug("SBLOCK_NOTIFY_RECV is received\n");
		wlan_sipc_event_rx_handler(priv);
		break;
	case SBLOCK_NOTIFY_STATUS:
		pr_debug("SBLOCK_NOTIFY_STATUS is received\n");
		break;
	default:
		pr_err("Received event is invalid(event=%d)\n", event);
	}
}

/* TODO -- add sbuf register and free */
int itm_wlan_sipc_alloc(struct itm_priv *itm_priv)
{
	struct wlan_sipc *wlan_sipc;
	struct wlan_sipc_data *sipc_buf;
	int ret;

	wlan_sipc = kzalloc(sizeof(struct wlan_sipc), GFP_KERNEL);
	if (!wlan_sipc) {
		pr_err("failed to alloc wlan_sipc struct\n");
		return -ENOMEM;
	}

	sipc_buf = itm_wlan_get_new_buf(WLAN_SBUF_SIZE);
	if (!sipc_buf) {
		pr_err("get new send buf failed\n");
		ret = -ENOMEM;
		goto fail_sendbuf;
	}
	wlan_sipc->send_buf = sipc_buf;

	sipc_buf = itm_wlan_get_new_buf(WLAN_SBUF_SIZE);
	if (!sipc_buf) {
		pr_err("get new recv buf failed\n");
		ret = -ENOMEM;
		goto fail_recvbuf;
	}
	wlan_sipc->recv_buf = sipc_buf;

	sipc_buf = itm_wlan_get_new_buf(WLAN_EVENT_SBLOCK_SIZE);
	if (!sipc_buf) {
		pr_err("get new recv buf failed\n");
		ret = -ENOMEM;
		goto fail_eventbuf;
	}
	wlan_sipc->event_buf = sipc_buf;
/*
	ret = sblock_create(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
			    WLAN_EVENT_SBLOCK_NUM, WLAN_EVENT_SBLOCK_SIZE,
			    WLAN_EVENT_SBLOCK_NUM, WLAN_EVENT_SBLOCK_SIZE);
	if (ret) {
		pr_err("Failed to create event sblock (%d)\n", ret);
		ret = -ENOMEM;
		goto fail_sblock;
	}
*/
	ret =
	    sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
				     wlan_sipc_sblock_handler, itm_priv);
	if (ret) {
		pr_err("Failed to regitster event sblock notifier (%d)\n", ret);
		ret = -ENOMEM;
		goto fail_notifier;
	}

	spin_lock_init(&wlan_sipc->lock);
	mutex_init(&wlan_sipc->cmd_lock);
	mutex_init(&wlan_sipc->pm_lock);

	itm_priv->wlan_sipc = wlan_sipc;

	return 0;

fail_notifier:
	sblock_destroy(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH);
/*fail_sblock:
	kfree(wlan_sipc->event_buf);*/
fail_eventbuf:
	kfree(wlan_sipc->recv_buf);
fail_recvbuf:
	kfree(wlan_sipc->send_buf);
fail_sendbuf:
	kfree(wlan_sipc);
	return ret;
}

void itm_wlan_sipc_free(struct itm_priv *itm_priv)
{
	int ret;

	kfree(itm_priv->wlan_sipc->send_buf);
	kfree(itm_priv->wlan_sipc->recv_buf);
	kfree(itm_priv->wlan_sipc);
	itm_priv->wlan_sipc = NULL;

/*	sblock_destroy(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH);*/
	/* FIXME it is a ugly method */
	ret =
	    sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
				     NULL, NULL);
	if (ret) {
		pr_err("Failed to regitster event sblock notifier (%d)\n", ret);
		ret = -ENOMEM;
	}

	return;
}

int itm_wlan_get_ip_cmd(struct itm_priv *itm_priv, u8 *ip)
{
	int ret;
	u16 status;
	struct wlan_sipc *wlan_sipc = itm_priv->wlan_sipc;
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;

	if (ip != NULL)
		memcpy(send_buf->u.cmd.variable, ip, 4);
	else
		memset(send_buf->u.cmd.variable, 0, 4);

	pr_debug("enter get ip send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, ITM_WLAN_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_LINK_STATUS);

	if (ret) {
		pr_err("get ip cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, ITM_WLAN_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_LINK_STATUS);
	if (ret) {
		pr_err("get ip cmd recv error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_pm_enter_ps_cmd(struct itm_priv *itm_priv)
{
	int ret;
	struct wlan_sipc *wlan_sipc = itm_priv->wlan_sipc;
	struct wlan_sipc_data *send_buf = wlan_sipc->send_buf;
	struct in_device *ip = (struct in_device *)itm_priv->ndev->ip_ptr;
	struct in_ifaddr *in = ip->ifa_list;

	if (in != NULL)
		memcpy(send_buf->u.cmd.variable, &in->ifa_address, 4);

	pr_debug("enter ps cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, ITM_WLAN_CMD_HDR_SIZE + 4,
			       CMD_TYPE_SET, WIFI_CMD_PM_ENTER_PS);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_pm_exit_ps_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;

	pr_debug("exit ps cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, ITM_WLAN_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EXIT_PS);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}
	mutex_unlock(&wlan_sipc->cmd_lock);

	return 0;
}

int itm_wlan_pm_early_suspend_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;
	u16 status;

	pr_debug("early suspend cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, ITM_WLAN_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_EARLY_SUSPEND);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, ITM_WLAN_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_EARLY_SUSPEND);
	if (ret) {
		pr_err("early suspend cmd recv error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);
	return 0;
}

int itm_wlan_pm_later_resume_cmd(struct wlan_sipc *wlan_sipc)
{
	int ret;
	u16 status;

	pr_debug("later resume cmd send\n");
	mutex_lock(&wlan_sipc->cmd_lock);
	ret =
	    wlan_sipc_cmd_send(wlan_sipc, ITM_WLAN_CMD_HDR_SIZE, CMD_TYPE_SET,
			       WIFI_CMD_PM_LATER_RESUME);

	if (ret) {
		pr_err("cmd send error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	ret = wlan_sipc_cmd_receive(wlan_sipc, ITM_WLAN_CMD_RESP_HDR_SIZE,
				    WIFI_CMD_PM_LATER_RESUME);
	if (ret) {
		pr_err("later resume cmd recv error with ret is %d\n", ret);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return ret;
	}

	status = wlan_sipc->recv_buf->u.cmd_resp.status_code;

	if (status) {
		pr_err("%s command error %d\n", __func__, status);
		mutex_unlock(&wlan_sipc->cmd_lock);
		return -EIO;
	}

	mutex_unlock(&wlan_sipc->cmd_lock);
	return 0;
}
