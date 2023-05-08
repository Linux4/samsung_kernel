/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : npi.c
 * Abstract : This file is a implementation for npi subsystem
 *
 * Authors	:
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
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

#include <linux/uaccess.h>
#include <net/genetlink.h>
#include <linux/sipc.h>

#include "sipc.h"
#include "sprdwl.h"
#include "cfg80211.h"
#include "npi.h"

static int nlnpi_pre_doit(struct genl_ops *ops, struct sk_buff *skb,
			  struct genl_info *info)
{
	struct net_device *ndev;
	struct sprdwl_vif *vif;
	struct sprdwl_priv *priv;
	int ifindex;

	if (info->attrs[NLNPI_ATTR_IFINDEX]) {
		ifindex = nla_get_u32(info->attrs[NLNPI_ATTR_IFINDEX]);
		ndev = dev_get_by_index(genl_info_net(info), ifindex);
		if (!ndev) {
			pr_err("NPI: Could not find ndev\n");
			return -EFAULT;
		}
		vif = netdev_priv(ndev);
		priv = vif->priv;
		info->user_ptr[0] = ndev;
		info->user_ptr[1] = priv;
	} else {
		pr_err("nl80211_pre_doit: Not have attr_ifindex\n");
		return -EFAULT;
	}

	return 0;
}

static void nlnpi_post_doit(struct genl_ops *ops, struct sk_buff *skb,
			    struct genl_info *info)
{
	if (info->user_ptr[0])
		dev_put(info->user_ptr[0]);
}

/* NPI netlink family */
static struct genl_family npi_genl_family = {
	.id = NL_GENERAL_NPI_ID,
	.hdrsize = 0,
	.name = "nlnpi",
	.version = 1,
	.maxattr = NLNPI_ATTR_MAX,
	.pre_doit = nlnpi_pre_doit,
	.post_doit = nlnpi_post_doit,
};

int sprdwl_npi_cmd(struct wlan_sipc *sipc, u8 type,
		   u8 id, u16 s_len, u16 r_len, u8 *s_buf, u8 *r_buf)
{
	struct wlan_sipc_data *send_buf = sipc->send_buf;
	struct wlan_sipc_npi *npi =
	    (struct wlan_sipc_npi *)send_buf->u.cmd.variable;
	u16 send_len = 0;
	u16 recv_len = 0;
	int ret;

	mutex_lock(&sipc->cmd_lock);

	npi->id = id;

	if (type == CMD_TYPE_GET) {
		if (!r_len) {
			pr_err("Get cmd len should not be zero\n");
			return -EIO;
		}
		recv_len = sizeof(id) + sizeof(npi->len) + r_len;
		if (s_len) {
			npi->len = s_len;
			memcpy(npi->value, s_buf, s_len);
			send_len = sizeof(id) + sizeof(npi->len) + s_len;
		} else {
			send_len = sizeof(id);
		}
	} else if (type == CMD_TYPE_SET) {
		if (s_len) {
			npi->len = s_len;
			memcpy(npi->value, s_buf, s_len);
			send_len = sizeof(id) + sizeof(npi->len) + s_len;
		} else {
			send_len = sizeof(id);
		}
		recv_len = 0;
	}

	sipc->send_len = SPRDWL_CMD_HDR_SIZE + send_len;
	sipc->recv_len = SPRDWL_CMD_RESP_HDR_SIZE + recv_len;
	ret = sprdwl_cmd_send_recv(sipc, type, WIFI_CMD_NPI);

	if (ret) {
		pr_err("npi return wrong status code is %d\n", ret);
		mutex_unlock(&sipc->cmd_lock);
		return -EIO;
	}

	if (type == CMD_TYPE_GET) {
		if (r_buf) {
			struct wlan_sipc_npi *npi_resp =
			    (struct wlan_sipc_npi *)sipc->recv_buf->u.
			    cmd_resp.variable;
			memcpy(r_buf, npi_resp->value, npi_resp->len);
		} else {
			pr_err("get cmd buf shuold not be null\n");
		}
	}
	mutex_unlock(&sipc->cmd_lock);

	pr_debug("npi return status code successfully\n");

	return 0;
}

static int npi_nl_send_generic(struct genl_info *info, u8 attr,
			       u8 cmd, u32 len, u8 *data)
{
	struct sk_buff *skb;
	void *hdr;
	int ret;

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	hdr = genlmsg_put(skb, info->snd_portid, info->snd_seq,
			  &npi_genl_family, 0, cmd);
	if (IS_ERR(hdr)) {
		ret = PTR_ERR(hdr);
		goto err_put;
	}

	nla_put(skb, attr, len, data);
	genlmsg_end(skb, hdr);

	return genlmsg_reply(skb, info);

err_put:
	nlmsg_free(skb);
	return ret;

	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return -EMSGSIZE;
}

/* Max count of unsupport cmd in WIFI RF */
#define INWPI_UNSUPPORT_CMD_MAX_COUNT   (2)

/* unsupport commands because RF unsupport */
static char *rf_unsupport_cmd[INWPI_UNSUPPORT_CMD_MAX_COUNT] = {
};

/* unsupport commands in RF, */
enum rf_unsupport_cmd_id {
	NPI_CMD_UNSUPPORT = 0,
};

/*   name   npi_cmd_is_unsupport
 *   ---------------------------
 *   descrition: just cmd from APK whether support in RF
 *   ----------------------------
 *   para        IN/OUT      type            note
 *   cmd_name    IN          const char *    command name
 *   ----------------------------------------------------
 *   return
 *   1:cmd is unsupport
 *   0:cmd is support
 *   ------------------
 *   other:
 */
static int npi_cmd_is_unsupport(const char *cmd_name)
{
	int i = 0;

	for (; i < INWPI_UNSUPPORT_CMD_MAX_COUNT; i++) {
		if (NULL != rf_unsupport_cmd[i]
		    && 0 == strcmp(cmd_name, rf_unsupport_cmd[i])) {
			pr_err("ADL %s(), return 1, i = %d, name = %s\n",
			       __func__, i, cmd_name);
			return 1;
		}
	}

	return 0;
}

#define NPI_SET_CMD(name, npi_cmd, nl_cmd, attr)	\
static int npi_ ## name ## _cmd(struct sk_buff *skb_2,	\
				struct genl_info *info)	\
{							\
	struct sprdwl_priv *priv;			\
	int ret = -EINVAL;				\
	int attr_len = 0;				\
	char *attr_data = NULL;				\
							\
	if (info == NULL)				\
		goto out;				\
							\
	priv = info->user_ptr[1];			\
	if (priv == NULL)				\
		goto out;				\
							\
	if (info->attrs[attr]) {			\
		attr_data = nla_data(info->attrs[attr]);\
		attr_len = nla_len(info->attrs[attr]);	\
	}						\
							\
	if (npi_cmd_is_unsupport(#name)) {		\
		pr_err("%s: command not supported\n",	\
		       __func__);			\
		ret = 0;				\
	} else {					\
		ret = sprdwl_npi_cmd(priv->sipc,	\
				     CMD_TYPE_SET,	\
				     npi_cmd, attr_len,	\
				     0, attr_data,	\
				     NULL);		\
	}						\
							\
	if (ret != 0)					\
		pr_err("%s: command failed\n",		\
		       __func__);			\
							\
out:							\
	ret = npi_nl_send_generic(info, NLNPI_ATTR_REPLY_STATUS,\
				  nl_cmd, sizeof(ret), (u8 *)&ret);\
	return ret;					\
}
/* Have arg */
NPI_SET_CMD(set_tx_rate, NPI_CMD_RATE, NLNPI_CMD_SET_RATE, NLNPI_ATTR_SET_RATE)
NPI_SET_CMD(set_channel, NPI_CMD_CHANNEL, NLNPI_CMD_SET_CHANNEL,
	    NLNPI_ATTR_SET_CHANNEL)
NPI_SET_CMD(set_tx_power, NPI_CMD_TX_POWER, NLNPI_CMD_SET_TX_POWER,
	    NLNPI_ATTR_SET_TX_POWER)
NPI_SET_CMD(set_rx_count, NPI_CMD_RX_COUNT, NLNPI_CMD_CLEAR_RX_COUNT,
	    NLNPI_ATTR_SET_RX_COUNT)
NPI_SET_CMD(set_tx_mode, NPI_CMD_TX_MODE, NLNPI_CMD_SET_TX_MODE,
	    NLNPI_ATTR_SET_TX_MODE)
NPI_SET_CMD(set_tx_count, NPI_CMD_TX_COUNT, NLNPI_CMD_SET_TX_COUNT,
	    NLNPI_ATTR_TX_COUNT)

/* set_pkt_length */
NPI_SET_CMD(set_pkt_length, NPI_CMD_PKTLEN, NLNPI_CMD_SET_PKTLEN,
	    NLNPI_ATTR_SET_PKTLEN)

/* set_preamble */
NPI_SET_CMD(set_preamble, NPI_CMD_PREAMBLE_TYPE,
	    NLNPI_CMD_SET_PREAMBLE_TYPE, NLNPI_ATTR_SET_PREAMBLE_TYPE)

/* set_band */
NPI_SET_CMD(set_band, NPI_CMD_BAND, NLNPI_CMD_SET_BAND, NLNPI_ATTR_SET_BAND)

/* set_bandwidth */
NPI_SET_CMD(set_bandwidth, NPI_CMD_BANDWIDTH,
	    NLNPI_CMD_SET_BANDWIDTH, NLNPI_ATTR_SET_BANDWIDTH)

/* set_guard_interval */
NPI_SET_CMD(set_guard_interval, NPI_CMD_SHORTGI,
	    NLNPI_CMD_SET_SHORTGI, NLNPI_ATTR_SET_SHORTGI)

/* set_macfilter */
NPI_SET_CMD(set_macfilter, NPI_CMD_MACFILTER,
	    NLNPI_CMD_SET_MACFILTER, NLNPI_ATTR_SET_MACFILTER)

/* set_payload */
NPI_SET_CMD(set_payload, NPI_CMD_PAYLOAD, NLNPI_CMD_SET_PAYLOAD,
	    NLNPI_ATTR_SET_PAYLOAD)

NPI_SET_CMD(set_mac, NPI_CMD_MAC, NLNPI_CMD_SET_MAC, NLNPI_ATTR_SET_MAC)
NPI_SET_CMD(set_reg, NPI_CMD_REG, NLNPI_CMD_SET_REG, NLNPI_ATTR_SET_REG)
/* Not have arg */
NPI_SET_CMD(start_tx_data, NPI_CMD_TX_START, NLNPI_CMD_TX_START,
	    NLNPI_ATTR_TX_START)
NPI_SET_CMD(stop_tx_data, NPI_CMD_TX_STOP, NLNPI_CMD_TX_STOP,
	    NLNPI_ATTR_TX_STOP)
NPI_SET_CMD(start_rx_data, NPI_CMD_RX_START, NLNPI_CMD_RX_START,
	    NLNPI_ATTR_RX_START)
NPI_SET_CMD(stop_rx_data, NPI_CMD_RX_STOP, NLNPI_CMD_RX_STOP,
	    NLNPI_ATTR_RX_STOP)
NPI_SET_CMD(set_debug, NPI_CMD_DEBUG, NLNPI_CMD_SET_DEBUG, NLNPI_ATTR_SET_DEBUG)
NPI_SET_CMD(get_sblock, NPI_CMD_GET_SBLOCK, NLNPI_CMD_GET_SBLOCK,
	    NLNPI_ATTR_SBLOCK_ARG)
NPI_SET_CMD(sin_wave, NPI_CMD_SIN_WAVE, NLNPI_CMD_SIN_WAVE, NLNPI_ATTR_SIN_WAVE)
NPI_SET_CMD(lna_on, NPI_CMD_LNA_ON, NLNPI_CMD_LNA_ON, NLNPI_ATTR_LNA_ON)
NPI_SET_CMD(lna_off, NPI_CMD_LNA_OFF, NLNPI_CMD_LNA_OFF, NLNPI_ATTR_LNA_OFF)
NPI_SET_CMD(speed_up, NPI_CMD_SPEED_UP, NLNPI_CMD_SPEED_UP, NLNPI_ATTR_SPEED_UP)
NPI_SET_CMD(speed_down, NPI_CMD_SPEED_DOWN, NLNPI_CMD_SPEED_DOWN,
	    NLNPI_ATTR_SPEED_DOWN)
/*samsung hardware test interface*/
NPI_SET_CMD(set_long_preamble, NPI_CMD_PREAMBLE_TYPE,
	    NLNPI_CMD_SET_LONG_PREAMBLE, NLNPI_ATTR_SET_LONG_PREAMBLE)
NPI_SET_CMD(set_short_guard_interval, NPI_CMD_SHORTGI,
	    NLNPI_CMD_SET_SHORT_GUARD_INTERVAL,
	    NLNPI_ATTR_SET_SHORT_GUARD_INTERVAL)
NPI_SET_CMD(set_burst_interval, NPI_CMD_SET_BURST_INTERVAL,
	    NLNPI_CMD_SET_BURST_INTERVAL, NLNPI_ATTR_SET_BURST_INTERVAL)
/*NPI_SET_CMD(set_payload, NPI_CMD_SET_PAYLOAD, NLNPI_CMD_SET_PAYLOAD,
	    NLNPI_ATTR_SET_PAYLOAD)
*/
#define NPI_GET_CMD(name, npi_cmd, nl_cmd, attr, arg_attr)	\
static int npi_ ## name ## _cmd(struct sk_buff *skb_2,		\
			 struct genl_info *info)		\
{								\
	struct sprdwl_priv *priv;				\
	unsigned char data[1024];				\
	int attr_len = 0;					\
	int arg_attr_len = 0;					\
	char *arg_attr_data = NULL;				\
	int ret = -EINVAL;					\
								\
	if (info == NULL)					\
		goto out;					\
								\
	priv = info->user_ptr[1];				\
	if (priv == NULL)					\
		goto out;					\
								\
	if (info->attrs[attr]) {				\
		attr_len = nla_get_u32(info->attrs[attr]);	\
		if (attr_len > 1024) {				\
			pr_err("" #name ": Invalid arg\n");	\
			goto out;				\
		}						\
	} else {						\
		pr_err("" #name ": Invalid arg\n");		\
		goto out;					\
	}							\
								\
	if (info->attrs[arg_attr]) {				\
		arg_attr_data = nla_data(info->attrs[arg_attr]);\
		arg_attr_len = nla_len(info->attrs[arg_attr]);  \
	}							\
								\
	ret = sprdwl_npi_cmd(priv->sipc, CMD_TYPE_GET,	\
			       npi_cmd, arg_attr_len, attr_len, \
			       arg_attr_data, data);		\
	if (ret != 0) {						\
		pr_err("" #name ": sprdwl_npi_cmd failed\n");	\
		return -EIO;					\
	}							\
								\
	ret = npi_nl_send_generic(info, NLNPI_ATTR_REPLY_DATA,	\
				  nl_cmd, attr_len, data);	\
out:								\
	return ret;						\
}
NPI_GET_CMD(get_macfilter, NPI_CMD_MACFILTER,
	    NLNPI_CMD_GET_MACFILTER, NLNPI_ATTR_GET_MACFILTER,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_band, NPI_CMD_BAND, NLNPI_CMD_GET_BAND,
	    NLNPI_ATTR_GET_BAND, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_bandwidth, NPI_CMD_BANDWIDTH,
	    NLNPI_CMD_GET_BANDWIDTH, NLNPI_ATTR_GET_BANDWIDTH,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_guard_interval, NPI_CMD_SHORTGI,
	    NLNPI_CMD_GET_SHORTGI, NLNPI_ATTR_GET_SHORTGI,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_payload, NPI_CMD_PAYLOAD, NLNPI_CMD_GET_PAYLOAD,
	    NLNPI_ATTR_GET_PAYLOAD, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_tx_mode, NPI_CMD_TX_MODE, NLNPI_CMD_GET_TX_MODE,
	    NLNPI_ATTR_GET_TX_MODE, NLNPI_ATTR_GET_NO_ARG)

NPI_GET_CMD(get_channel, NPI_CMD_CHANNEL, NLNPI_CMD_GET_CHANNEL,
	    NLNPI_ATTR_GET_CHANNEL, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_pkt_length, NPI_CMD_PKTLEN, NLNPI_CMD_GET_PKTLEN,
	    NLNPI_ATTR_GET_PKTLEN, NLNPI_ATTR_GET_NO_ARG)

NPI_GET_CMD(get_preamble, NPI_CMD_PREAMBLE_TYPE,
	    NLNPI_CMD_GET_PREAMBLE_TYPE,
	    NLNPI_ATTR_GET_PREAMBLE_TYPE, NLNPI_ATTR_GET_NO_ARG)

NPI_GET_CMD(get_tx_power, NPI_CMD_TX_POWER,
	    NLNPI_CMD_GET_TX_POWER, NLNPI_ATTR_GET_TX_POWER,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_rx_error_count, NPI_CMD_RX_ERROR,
	    NLNPI_CMD_GET_RX_ERR_COUNT, NLNPI_ATTR_GET_RX_COUNT,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_rx_right_count, NPI_CMD_RX_RIGHT,
	    NLNPI_CMD_GET_RX_OK_COUNT, NLNPI_ATTR_GET_RX_COUNT,
	    NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_rssi, NPI_CMD_RSSI, NLNPI_CMD_GET_RSSI,
	    NLNPI_ATTR_RSSI, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_tx_rate, NPI_CMD_RATE, NLNPI_CMD_GET_RATE,
	    NLNPI_ATTR_GET_RATE, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_mac, NPI_CMD_MAC, NLNPI_CMD_GET_MAC,
	    NLNPI_ATTR_GET_MAC, NLNPI_ATTR_GET_NO_ARG)
NPI_GET_CMD(get_reg, NPI_CMD_REG, NLNPI_CMD_GET_REG,
	    NLNPI_ATTR_GET_REG, NLNPI_ATTR_GET_REG_ARG)
NPI_GET_CMD(get_debug, NPI_CMD_DEBUG, NLNPI_CMD_GET_DEBUG,
	    NLNPI_ATTR_GET_DEBUG, NLNPI_ATTR_GET_DEBUG_ARG)
NPI_GET_CMD(get_lna_status, NPI_CMD_GET_LNA_STATUS,
	    NLNPI_CMD_GET_LNA_STATUS, NLNPI_ATTR_GET_LNA_STATUS,
	    NLNPI_ATTR_GET_NO_ARG)

static int sblock_tx(unsigned int len, char data)
{
	struct sblock blk;
	int ret = -EINVAL;
	int i;
	/*Get a free sblock */
	ret = sblock_get(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk, 0);
	if (ret) {
		pr_err("Failed to get free sblock (%d)\n", ret);
		return -1;
	}

	if (blk.length < len) {
		pr_err("The size of sblock is so tiny!\n");
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		return -2;
	}

	blk.length = len;
	for (i = 0; i < len; i++)
		memcpy((blk.addr + i), &data, 1);
	ret = sblock_send(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
	if (ret) {
		pr_err("Failed to send sblock (%d)\n", ret);
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		return -3;
	}

	return 0;
}

static int npi_tx_sblock(struct sk_buff *skb_2, struct genl_info *info)
{
	int ret = -EINVAL;
	int attr_len = 0;
	char *attr_data = NULL;
	unsigned int len, count, i;
	if (info == NULL)
		return ret;
	if (info->attrs[NLNPI_ATTR_SBLOCK_ARG]) {
		attr_data = nla_data(info->attrs[NLNPI_ATTR_SBLOCK_ARG]);
		attr_len = nla_len(info->attrs[NLNPI_ATTR_SBLOCK_ARG]);
	} else {
		return ret;
	}
	memcpy(&len, attr_data, 4);
	memcpy(&count, (attr_data + 4), 4);
	if (count == 0) {
		while (1)
			sblock_tx(len, 0x55);
	} else {
		for (i = 0; i < count; i++) {
			pr_err("npi_tx_sblock: start send the %d count\n", i);
			sblock_tx(len, 0x55);
		}
	}

	return 0;
}

static int npi_stop_cmd(struct sk_buff *skb_2, struct genl_info *info)
{
	struct sprdwl_priv *priv;
	int ret = -EINVAL;
	if (info == NULL)
		goto out;
	priv = info->user_ptr[1];
	if (priv == NULL)
		goto out;
	ret = sprdwl_mac_close_cmd(priv->sipc, SPRDWL_NPI_MODE);
	if (ret != 0) {
		pr_err("npi_stop_npi: sprdwl_mac_close_cmd failed\n");
		goto out;
	}

out:
	ret =
	    npi_nl_send_generic(info, NLNPI_ATTR_REPLY_STATUS,
				NLNPI_CMD_STOP, sizeof(ret), (u8 *) &ret);
	return ret;
}

static int npi_start_cmd(struct sk_buff *skb_2, struct genl_info *info)
{
	struct net_device *ndev;
	struct sprdwl_priv *priv;
	int ret = -EINVAL;
	if (info == NULL)
		goto out;
	ndev = info->user_ptr[0];
	priv = info->user_ptr[1];
	if (ndev == NULL || priv == NULL)
		goto out;
	ret = sprdwl_mac_open_cmd(priv->sipc, SPRDWL_NPI_MODE, ndev->dev_addr);
	if (ret != 0) {
		pr_err("npi_start_npi: sprdwl_mac_open_cmd failed\n");
		goto out;
	}

out:
	ret =
	    npi_nl_send_generic(info, NLNPI_ATTR_REPLY_STATUS,
				NLNPI_CMD_START, sizeof(ret), (u8 *) &ret);
	return ret;
}

/* iwnpi netlink policy */

static struct nla_policy npi_genl_policy[NLNPI_ATTR_MAX + 1] = {
	[NLNPI_ATTR_REPLY_STATUS] = {.type = NLA_BINARY, .len = 32},
	[NLNPI_ATTR_REPLY_DATA] = {.type = NLA_BINARY, .len = 1024},
	[NLNPI_ATTR_START] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_STOP] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_TX_START] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_TX_STOP] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_RX_START] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_RX_STOP] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_SET_TX_MODE] = {.len = 4},
	[NLNPI_ATTR_TX_COUNT] = {.len = 4},
	[NLNPI_ATTR_RSSI] = {.type = NLA_U32},
	[NLNPI_ATTR_GET_RX_COUNT] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_RX_COUNT] = {.len = 4},
	[NLNPI_ATTR_GET_TX_POWER] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_TX_POWER] = {.len = 4},
	[NLNPI_ATTR_GET_CHANNEL] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_CHANNEL] = {.len = 4},
	[NLNPI_ATTR_SET_RATE] = {.len = 4},
	[NLNPI_ATTR_GET_RATE] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_MAC] = {.len = ETH_ALEN},
	[NLNPI_ATTR_GET_MAC] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_REG] = {.len = 9},
	[NLNPI_ATTR_GET_REG] = {.type = NLA_U32},
	[NLNPI_ATTR_GET_NO_ARG] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_GET_REG_ARG] = {.len = 6},
	[NLNPI_ATTR_SET_DEBUG] = {.type = NLA_BINARY, .len = 38}, /* max len */
	[NLNPI_ATTR_GET_DEBUG] = {.type = NLA_U32},
	[NLNPI_ATTR_GET_DEBUG_ARG] = {.type = NLA_BINARY, .len = 32},
	[NLNPI_ATTR_SBLOCK_ARG] = {.len = 8},
	[NLNPI_ATTR_SIN_WAVE] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_LNA_ON] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_LNA_OFF] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_SPEED_UP] = {.type = NLA_UNSPEC,},
	[NLNPI_ATTR_SPEED_DOWN] = {.type = NLA_UNSPEC,},
	/*samsung hardware test interface */
	[NLNPI_ATTR_SET_LONG_PREAMBLE] = {.len = 4},
	[NLNPI_ATTR_SET_SHORT_GUARD_INTERVAL] = {.len = 4},
	[NLNPI_ATTR_SET_BURST_INTERVAL] = {.len = 4},
	/* set_payload */
	[NLNPI_ATTR_SET_PAYLOAD] = {.len = 4},
	/* get_payload */
	[NLNPI_ATTR_GET_PAYLOAD] = {.type = NLA_U32},
	[NLNPI_ATTR_GET_TX_MODE] = {.type = NLA_U32},
	[NLNPI_ATTR_SET_PREAMBLE_TYPE] = {.len = 4},
	[NLNPI_ATTR_GET_PREAMBLE_TYPE] = {.type = NLA_U32},
	/* set_band */
	[NLNPI_ATTR_SET_BAND] = {.len = 4},
	/* get_band */
	[NLNPI_ATTR_GET_BAND] = {.type = NLA_U32},
	/* set_bandwidth */
	[NLNPI_ATTR_SET_BANDWIDTH] = {.len = 4},
	/* get_bandwidth */
	[NLNPI_ATTR_GET_BANDWIDTH] = {.type = NLA_U32},
	/* set_guard_interval */
	[NLNPI_ATTR_SET_SHORTGI] = {.len = 4},
	/* set_guard_interval */
	[NLNPI_ATTR_GET_SHORTGI] = {.type = NLA_U32},
	/* set_macfilter */
	[NLNPI_ATTR_SET_MACFILTER] = {.len = 4},
	/* get_macfilter */
	[NLNPI_ATTR_GET_MACFILTER] = {.type = NLA_U32},
	/* set_pktlen */
	[NLNPI_ATTR_SET_PKTLEN] = {.len = 4},
	/* get_pktlen */
	[NLNPI_ATTR_GET_PKTLEN] = {.type = NLA_U32},};

/* Generic Netlink operations array */
static struct genl_ops npi_ops[] = {
	{
		.cmd = NLNPI_CMD_SET_TX_COUNT,
		.policy = npi_genl_policy,
		.doit = npi_set_tx_count_cmd,
	},
	{
		.cmd = NLNPI_CMD_START,
		.policy = npi_genl_policy,
		.doit = npi_start_cmd,
	},
	{
		.cmd = NLNPI_CMD_STOP,
		.policy = npi_genl_policy,
		.doit = npi_stop_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_TX_MODE,
		.policy = npi_genl_policy,
		.doit = npi_set_tx_mode_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_RSSI,
		.policy = npi_genl_policy,
		.doit = npi_get_rssi_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_RX_OK_COUNT,
		.policy = npi_genl_policy,
		.doit = npi_get_rx_right_count_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_RX_ERR_COUNT,
		.policy = npi_genl_policy,
		.doit = npi_get_rx_error_count_cmd,
	},
	{
		.cmd = NLNPI_CMD_CLEAR_RX_COUNT,
		.policy = npi_genl_policy,
		.doit = npi_set_rx_count_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_TX_POWER,
		.policy = npi_genl_policy,
		.doit = npi_set_tx_power_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_TX_POWER,
		.policy = npi_genl_policy,
		.doit = npi_get_tx_power_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_CHANNEL,
		.policy = npi_genl_policy,
		.doit = npi_get_channel_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_CHANNEL,
		.policy = npi_genl_policy,
		.doit = npi_set_channel_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_RATE,
		.policy = npi_genl_policy,
		.doit = npi_set_tx_rate_cmd,
	},
	{
		.cmd = NLNPI_CMD_RX_STOP,
		.policy = npi_genl_policy,
		.doit = npi_stop_rx_data_cmd,
	},
	{
		.cmd = NLNPI_CMD_RX_START,
		.policy = npi_genl_policy,
		.doit = npi_start_rx_data_cmd,
	},
	{
		.cmd = NLNPI_CMD_TX_STOP,
		.policy = npi_genl_policy,
		.doit = npi_stop_tx_data_cmd,
	},
	{
		.cmd = NLNPI_CMD_TX_START,
		.policy = npi_genl_policy,
		.doit = npi_start_tx_data_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_RATE,
		.policy = npi_genl_policy,
		.doit = npi_get_tx_rate_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_MAC,
		.policy = npi_genl_policy,
		.doit = npi_set_mac_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_MAC,
		.policy = npi_genl_policy,
		.doit = npi_get_mac_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_REG,
		.policy = npi_genl_policy,
		.doit = npi_set_reg_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_REG,
		.policy = npi_genl_policy,
		.doit = npi_get_reg_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_DEBUG,
		.policy = npi_genl_policy,
		.doit = npi_set_debug_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_DEBUG,
		.policy = npi_genl_policy,
		.doit = npi_get_debug_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_SBLOCK,
		.policy = npi_genl_policy,
		.doit = npi_tx_sblock,
	},
	{
		.cmd = NLNPI_CMD_GET_SBLOCK,
		.policy = npi_genl_policy,
		.doit = npi_get_sblock_cmd,
	},
	{
		.cmd = NLNPI_CMD_SIN_WAVE,
		.policy = npi_genl_policy,
		.doit = npi_sin_wave_cmd,
	},
	{
		.cmd = NLNPI_CMD_LNA_ON,
		.policy = npi_genl_policy,
		.doit = npi_lna_on_cmd,
	},
	{
		.cmd = NLNPI_CMD_LNA_OFF,
		.policy = npi_genl_policy,
		.doit = npi_lna_off_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_LNA_STATUS,
		.policy = npi_genl_policy,
		.doit = npi_get_lna_status_cmd,
	},
	{
		.cmd = NLNPI_CMD_SPEED_UP,
		.policy = npi_genl_policy,
		.doit = npi_speed_up_cmd,
	},
	{
		.cmd = NLNPI_CMD_SPEED_DOWN,
		.policy = npi_genl_policy,
		.doit = npi_speed_down_cmd,
	},
	/*samsung hardware test interface */
	{
		.cmd = NLNPI_CMD_SET_LONG_PREAMBLE,
		.policy = npi_genl_policy,
		.doit = npi_set_long_preamble_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_SHORT_GUARD_INTERVAL,
		.policy = npi_genl_policy,
		.doit = npi_set_short_guard_interval_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_BURST_INTERVAL,
		.policy = npi_genl_policy,
		.doit = npi_set_burst_interval_cmd,
	},
	/* set_payload */
	{
		.cmd = NLNPI_CMD_SET_PAYLOAD,
		.policy = npi_genl_policy,
		.doit = npi_set_payload_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_PKTLEN,
		.policy = npi_genl_policy,
		.doit = npi_set_pkt_length_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_PKTLEN,
		.policy = npi_genl_policy,
		.doit = npi_get_pkt_length_cmd,
	},
	{
		.cmd = NLNPI_CMD_SET_PREAMBLE_TYPE,
		.policy = npi_genl_policy,
		.doit = npi_set_preamble_cmd,
	},
	{
		.cmd = NLNPI_CMD_GET_TX_MODE,
		.policy = npi_genl_policy,
		.doit = npi_get_tx_mode_cmd,
	},
	/* set_band */
	{
		.cmd = NLNPI_CMD_SET_BAND,
		.policy = npi_genl_policy,
		.doit = npi_set_band_cmd,
	},
	/* get_band */
	{
		.cmd = NLNPI_CMD_GET_BAND,
		.policy = npi_genl_policy,
		.doit = npi_get_band_cmd,
	},
	/* set_bandwidth */
	{
		.cmd = NLNPI_CMD_SET_BANDWIDTH,
		.policy = npi_genl_policy,
		.doit = npi_set_bandwidth_cmd,
	},
	/* get_bandwidth */
	{
		.cmd = NLNPI_CMD_GET_BANDWIDTH,
		.policy = npi_genl_policy,
		.doit = npi_get_bandwidth_cmd,
	},
	/* set_guard_interval */
	{
		.cmd = NLNPI_CMD_SET_SHORTGI,
		.policy = npi_genl_policy,
		.doit = npi_set_guard_interval_cmd,
	},
	/* get_guard_interval */
	{
		.cmd = NLNPI_CMD_GET_SHORTGI,
		.policy = npi_genl_policy,
		.doit = npi_get_guard_interval_cmd,
	},
	/* set_macfilter */
	{
		.cmd = NLNPI_CMD_SET_MACFILTER,
		.policy = npi_genl_policy,
		.doit = npi_set_macfilter_cmd,
	},
	/* get_macfilter */
	{
		.cmd = NLNPI_CMD_GET_MACFILTER,
		.policy = npi_genl_policy,
		.doit = npi_get_macfilter_cmd,
	},
#if 0
	/* set_payload */
	{
		.cmd = NLNPI_CMD_SET_PAYLOAD,
		.policy = npi_genl_policy,
		.doit = npi_set_payload_cmd,
	},
#endif
	/* get_payload */
	{
		.cmd = NLNPI_CMD_GET_PAYLOAD,
		.policy = npi_genl_policy,
		.doit = npi_get_payload_cmd,},
};

int npi_init_netlink(void)
{
	int ret;
	ret =
	    genl_register_family_with_ops(&npi_genl_family, npi_ops,
					  ARRAY_SIZE(npi_ops));
	if (ret)
		goto failure;
	return 0;
failure:
	pr_err("npi: error occurred in %s\n", __func__);
	return ret;
}

void npi_exit_netlink(void)
{
	int ret;
	/* unregister the family */
	ret = genl_unregister_family(&npi_genl_family);
	if (ret)
		pr_err("npi: " "unregister family %i\n", ret);
}
