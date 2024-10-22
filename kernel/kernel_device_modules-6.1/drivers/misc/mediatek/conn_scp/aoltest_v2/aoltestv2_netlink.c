// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/*******************************************************************************/
/*                    E X T E R N A L   R E F E R E N C E S                    */
/*******************************************************************************/
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>
#include "aoltestv2_netlink.h"

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/
#define AOLTEST_NETLINK_FAMILY_NAME "AOL_TESTv2"
#define AOLTEST_PKT_SIZE NLMSG_DEFAULT_SIZE
#define MAX_BIND_PROCESS    4

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

#define AOLTEST_ATTR_MAX       (__AOL_ATTR_MAX - 1)

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
// Define netlink command id
enum aoltest_netlink_cmd_id {
	_AOLTEST_NL_CMD_INVALID,
	AOLTEST_NL_CMD_BIND,
	AOLTEST_NL_CMD_SEND,
	AOLTEST_NL_CMD_MSG_TO_SCP,
	AOLTEST_NL_CMD_START,
	AOLTEST_NL_CMD_STOP,
	AOLTEST_NL_CMD_DATA_TRANS,
	_AOLTEST_NL_CMD_MAX,
};

// Define netlink message formats
enum aoltest_attr {
	_AOLTEST_ATTR_DEFAULT,
	AOLTEST_ATTR_PORT,
	AOLTEST_ATTR_MODULE,
	AOLTEST_ATTR_HEADER,
	AOLTEST_ATTR_MSG,
	AOLTEST_ATTR_MSG_ID,
	AOLTEST_ATTR_MSG_SIZE,
	AOLTEST_ATTR_MSG_DATA,
	AOLTEST_ATTR_TEST_INFO,
	_AOLTEST_ATTR_MAX,
};

enum link_status {
	LINK_STATUS_INIT,
	LINK_STATUS_INIT_DONE,
	LINK_STATUS_MAX,
};

struct aoltestv2_netlink_ctx {
	pid_t bind_pid;
	struct genl_family gnl_family;
	unsigned int seqnum;
	struct mutex nl_lock;
	enum link_status status;
	struct aoltestv2_netlink_event_cb cb;
};

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/
static int aoltestv2_nl_bind(struct sk_buff *skb, struct genl_info *info);
static int aoltestv2_nl_msg_to_scp(struct sk_buff *skb, struct genl_info *info);
static int aoltestv2_nl_start(struct sk_buff *skb, struct genl_info *info);
static int aoltestv2_nl_stop(struct sk_buff *skb, struct genl_info *info);
static int aoltestv2_nl_data_trans(struct sk_buff *skb, struct genl_info *info);

/*******************************************************************************/
/*                  G L O B A L  V A R I A B L E                               */
/*******************************************************************************/
/* Attribute policy */
static struct nla_policy aoltestv2_genl_policy[_AOLTEST_ATTR_MAX + 1] = {
	[AOLTEST_ATTR_PORT] = {.type = NLA_U32},
	[AOLTEST_ATTR_MODULE] = {.type = NLA_U32},
	[AOLTEST_ATTR_HEADER] = {.type = NLA_NUL_STRING},
	[AOLTEST_ATTR_MSG_ID] = {.type = NLA_U32},
	[AOLTEST_ATTR_MSG_SIZE] = {.type = NLA_U32},
	[AOLTEST_ATTR_MSG_DATA] = {.type = NLA_BINARY},
};

/* Operation definition */
static struct genl_ops aoltest_gnl_ops_array[] = {
	{
		.cmd = AOLTEST_NL_CMD_BIND,
		.flags = 0,
		.policy = aoltestv2_genl_policy,
		.doit = aoltestv2_nl_bind,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_MSG_TO_SCP,
		.flags = 0,
		.policy = aoltestv2_genl_policy,
		.doit = aoltestv2_nl_msg_to_scp,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_START,
		.flags = 0,
		.policy = aoltestv2_genl_policy,
		.doit = aoltestv2_nl_start,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_STOP,
		.flags = 0,
		.policy = aoltestv2_genl_policy,
		.doit = aoltestv2_nl_stop,
		.dumpit = NULL,
	},
	{
		.cmd = AOLTEST_NL_CMD_DATA_TRANS,
		.flags = 0,
		.policy = aoltestv2_genl_policy,
		.doit = aoltestv2_nl_data_trans,
		.dumpit = NULL,
	},

};

static const struct genl_multicast_group g_mcgrps = {
	.name = "AOL_TEST",
};

static struct aoltestv2_netlink_ctx g_aoltest_netlink_ctx = {
	.gnl_family = {
		.id = GENL_ID_GENERATE,
		.hdrsize = 0,
		.name = AOLTEST_NETLINK_FAMILY_NAME,
		.version = 1,
		.maxattr = _AOLTEST_ATTR_MAX,
		.ops = aoltest_gnl_ops_array,
		.n_ops = ARRAY_SIZE(aoltest_gnl_ops_array),
	},
	.status = LINK_STATUS_INIT,
	.seqnum = 0,
};

static struct aoltestv2_netlink_ctx *g_ctx = &g_aoltest_netlink_ctx;

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/
static int aoltestv2_nl_bind(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_na;
	unsigned int port, module;

	if (info == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	/* port id */
	attr_na = info->attrs[AOLTEST_ATTR_PORT];
	if (attr_na == NULL) {
		pr_info("[%s] No attr_na found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}
	port = (unsigned int)nla_get_u32(attr_na);

	/* module id */
	attr_na = info->attrs[AOLTEST_ATTR_MODULE];
	if (attr_na == NULL) {
		pr_info("[%s] No module found\n", __func__);
		mutex_unlock(&g_ctx->nl_lock);
		return -1;
	}

	module = (unsigned int)nla_get_u32(attr_na);

	pr_info("[%s] port=[%u] module=[%u]", __func__, port, module);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltestv2_bind)
		g_ctx->cb.aoltestv2_bind(port, module);

	return 0;
}

static int aoltestv2_nl_msg_to_scp(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[AOLTEST_ATTR_MODULE] == NULL ||
		attrs[AOLTEST_ATTR_MSG_ID] == NULL ||
		attrs[AOLTEST_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[AOLTEST_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[AOLTEST_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[AOLTEST_ATTR_MSG_SIZE]);
	pr_info("[%s] msg [%d][%d][%d]", __func__, module_id, msg_id, msg_sz);

	attr_info = attrs[AOLTEST_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltestv2_handler) {
		pr_info("[%s] call aoltestv2_handler", __func__);
		g_ctx->cb.aoltestv2_handler(module_id, msg_id, buf, msg_sz);
	}

	return 0;
}

static int aoltest_netlink_msg_send(char *tag, unsigned int msg_id, char *buf, unsigned int length,
								pid_t pid, unsigned int seq)
{
	struct sk_buff *skb;
	void *msg_head = NULL;
	int ret = 0;

	/* Allocate a generic netlink message buffer */
	skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (skb != NULL) {
		/* Create message header */
		msg_head = genlmsg_put(skb, 0, seq, &g_ctx->gnl_family, 0, AOLTEST_NL_CMD_SEND);
		if (msg_head == NULL) {
			pr_info("[%s] genlmsg_put fail\n", __func__);
			nlmsg_free(skb);
			return -EMSGSIZE;
		}

		/* Add message attribute and content */
		ret = nla_put_string(skb, AOLTEST_ATTR_HEADER, tag);
		if (ret != 0) {
			pr_info("[%s] nla_put_string header fail, ret=[%d]\n", __func__, ret);
			genlmsg_cancel(skb, msg_head);
			nlmsg_free(skb);
			return ret;
		}

		if (length) {
			ret = nla_put(skb, AOLTEST_ATTR_MSG, length, buf);
			if (ret != 0) {
				pr_info("[%s] nla_put fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, AOLTEST_ATTR_MSG_ID, msg_id);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}

			ret = nla_put_u32(skb, AOLTEST_ATTR_MSG_SIZE, length);
			if (ret != 0) {
				pr_info("[%s] nal_put_u32 fail, ret=[%d]\n", __func__, ret);
				genlmsg_cancel(skb, msg_head);
				nlmsg_free(skb);
				return ret;
			}
		}

		/* Finalize the message */
		genlmsg_end(skb, msg_head);

		/* Send message */
		ret = genlmsg_unicast(&init_net, skb, pid);
		if (ret == 0)
			pr_info("[%s] Send msg succeed\n", __func__);
	} else {
		pr_info("[%s] Allocate message error\n", __func__);
		ret = -ENOMEM;
	}

	return ret;
}


static int aoltestv2_nl_start(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[AOLTEST_ATTR_MODULE] == NULL ||
		attrs[AOLTEST_ATTR_MSG_ID] == NULL ||
		attrs[AOLTEST_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[AOLTEST_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[AOLTEST_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[AOLTEST_ATTR_MSG_SIZE]);
	pr_info("[%s] msg [%d][%d][%d]", __func__, module_id, msg_id, msg_sz);

	attr_info = attrs[AOLTEST_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltestv2_start) {
		pr_info("[%s] call aoltestv2_start", __func__);
		g_ctx->cb.aoltestv2_start(module_id, msg_id, buf, msg_sz);
	}

	return 0;
}

static int aoltestv2_nl_stop(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[AOLTEST_ATTR_MODULE] == NULL ||
		attrs[AOLTEST_ATTR_MSG_ID] == NULL ||
		attrs[AOLTEST_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[AOLTEST_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[AOLTEST_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[AOLTEST_ATTR_MSG_SIZE]);
	pr_info("[%s] msg [%d][%d][%d]", __func__, module_id, msg_id, msg_sz);

	attr_info = attrs[AOLTEST_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltestv2_stop) {
		pr_info("[%s] call aoltestv2_stop", __func__);
		g_ctx->cb.aoltestv2_stop(module_id, msg_id, buf, msg_sz);
	}
	return 0;
}

static int aoltestv2_nl_data_trans(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *attr_info = NULL;
	u32 msg_id, msg_sz, module_id = 0;
	u8 *buf = NULL;
	struct nlattr **attrs;

	if (info == NULL)
		return -1;

	attrs = info->attrs;
	if (attrs[AOLTEST_ATTR_MODULE] == NULL ||
		attrs[AOLTEST_ATTR_MSG_ID] == NULL ||
		attrs[AOLTEST_ATTR_MSG_SIZE] == NULL)
		return -1;

	if (mutex_lock_killable(&g_ctx->nl_lock))
		return -1;

	module_id = nla_get_u32(attrs[AOLTEST_ATTR_MODULE]);
	msg_id = nla_get_u32(attrs[AOLTEST_ATTR_MSG_ID]);
	msg_sz = nla_get_u32(attrs[AOLTEST_ATTR_MSG_SIZE]);
	pr_info("[%s] msg [%d][%d][%d]", __func__, module_id, msg_id, msg_sz);

	attr_info = attrs[AOLTEST_ATTR_MSG_DATA];
	if (attr_info != NULL)
		buf = nla_data(attr_info);

	mutex_unlock(&g_ctx->nl_lock);

	if (g_ctx && g_ctx->cb.aoltestv2_data_trans) {
		pr_info("[%s] call aoltestv2_data_trans", __func__);
		g_ctx->cb.aoltestv2_data_trans(module_id, msg_id, buf, msg_sz);
	}
	return 0;

}

static int aoltest_netlink_send_to_native_internal(struct netlink_client_info *client, char *tag,
				unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = -EAGAIN;
	unsigned int retry = 0;

	while (retry < 100 && ret == -EAGAIN) {
		ret = aoltest_netlink_msg_send(tag, msg_id, buf, length,
								client->port, client->seqnum);
		pr_info("[%s] genlmsg_unicast retry(%d) ret=[%d] pid=[%d] seq=[%d] tag=[%s]",
					__func__, retry, ret, client->port, client->seqnum, tag);

		retry++;
	}

	client->seqnum++;

	return ret;
}

int aoltestv2_netlink_send_to_native(struct netlink_client_info *client,
			char *tag, unsigned int msg_id, char *buf, unsigned int length)
{
	int ret = 0;
	int idx = 0;
	unsigned int send_len;
	unsigned int remain_len = length;

	if (g_ctx->status != LINK_STATUS_INIT_DONE) {
		pr_notice("[%s] Netlink should be init\n", __func__);
		return -2;
	}

	if (client->port == 0) {
		pr_notice("[%s] No bind service", __func__);
		return -3;
	}

	while (remain_len) {
		send_len = (remain_len > AOLTEST_PKT_SIZE ? AOLTEST_PKT_SIZE : remain_len);
		ret = aoltest_netlink_send_to_native_internal(client, tag, msg_id,
							&buf[idx], send_len);

		if (ret) {
			pr_info("[%s] From %d with len=[%d] fail, ret=[%d]\n"
							, __func__, idx, send_len, ret);
			break;
		}

		remain_len -= send_len;
		idx += send_len;
	}

	return idx;
}

int aoltestv2_netlink_init(struct aoltestv2_netlink_event_cb *cb)
{
	int ret = 0;

	mutex_init(&g_ctx->nl_lock);
	ret = genl_register_family(&g_ctx->gnl_family);

	if (ret != 0) {
		pr_info("[%s] GE_NELINK family registration fail, ret=[%d]\n", __func__, ret);
		return -2;
	}

	g_ctx->status = LINK_STATUS_INIT_DONE;
	g_ctx->bind_pid = 0;
	memcpy(&(g_ctx->cb), cb, sizeof(struct aoltestv2_netlink_event_cb));
	pr_info("[%s] aoltestv2 netlink init succeed\n", __func__);

	return ret;
}

void aoltestv2_netlink_deinit(void)
{
	g_ctx->status = LINK_STATUS_INIT;
	genl_unregister_family(&g_ctx->gnl_family);
}
