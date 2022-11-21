/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "scsc_wlbtd.h"

#define MAX_TIMEOUT	30000 /* in milisecounds */

/* completion to indicate when moredump is done */
static struct completion moredump_completion;

/**
 * This callback runs whenever the socket receives messages.
 */
static int msg_from_wlbtd_cb(struct sk_buff *skb, struct genl_info *info)
{
	int status;

	if (info->attrs[1])
		SCSC_TAG_INFO(WLBTD, "wlbtd returned data : %s\n",
				(char *)nla_data(info->attrs[1]));

	if (info->attrs[2]) {
		status = *((__u32 *)nla_data(info->attrs[2]));
		if (!status)
			SCSC_TAG_INFO(WLBTD,
				"wlbtd returned status : %u\n",	status);
		else
			SCSC_TAG_ERR(WLBTD,
				"wlbtd returned error : %u\n",	status);
	}

	complete(&moredump_completion);
	return 0;
}

/**
 * Here you can define some constraints for the attributes so Linux will
 * validate them for you.
 */
static struct nla_policy policies[] = {
	[ATTR_STR] = { .type = NLA_STRING, },
	[ATTR_INT] = { .type = NLA_U32, },
};

/**
 * Actual message type definition.
 */
struct genl_ops scsc_ops[] = {
	{
		.cmd = EVENT_PANIC,
		.flags = 0,
		.policy = policies,
		.doit = msg_from_wlbtd_cb,
		.dumpit = NULL,
	},
};

/* The netlink family */
static struct genl_family scsc_mdp_fam = {
	.id = GENL_ID_GENERATE, /* Don't bother with a hardcoded ID */
	.name = "scsc_mdp_family",     /* Have users key off the name instead */
	.hdrsize = 0,           /* No private header */
	.version = 1,
	.maxattr = __ATTR_MAX,
};

int scsc_wlbtd_init(void)
{
	int r = 0;
	/* register the family so that wlbtd can bind */
	r = genl_register_family_with_ops_groups(&scsc_mdp_fam, scsc_ops,
						scsc_mdp_mcgrp);
	if (r) {
		SCSC_TAG_ERR(WLBTD, "Failed to register family. (%d)\n", r);
		return -1;
	}

	init_completion(&moredump_completion);

	return r;
}

int coredump_wlbtd(void)
{
	struct sk_buff *skb;
	void *msg;
	int rc = 0;
	unsigned long ms_for_moredump;

	SCSC_TAG_DEBUG(WLBTD, "start\n");

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb) {
		SCSC_TAG_ERR(WLBTD, "Failed to construct message\n");
		goto error;
	}

	SCSC_TAG_INFO(WLBTD, "create message\n");
	msg = genlmsg_put(skb,
			0,           // PID is whatever
			0,           // Sequence number (don't care)
			&scsc_mdp_fam,   // Pointer to family struct
			0,           // Flags
			EVENT_PANIC // Generic netlink command
			);
	if (!msg) {
		SCSC_TAG_ERR(WLBTD, "Failed to create message\n");
		goto error;
	}

	SCSC_TAG_DEBUG(WLBTD, "add values to msg\n");
	rc = nla_put_u32(skb, ATTR_INT, 9999);
	if (rc) {
		SCSC_TAG_ERR(WLBTD, "nla_put_u32 failed. rc = %d\n", rc);
		genlmsg_cancel(skb, msg);
		goto error;
	}

	rc = nla_put_string(skb, ATTR_STR, "take moredump");
	if (rc) {
		SCSC_TAG_ERR(WLBTD, "nla_put_string failed. rc = %d\n", rc);
		genlmsg_cancel(skb, msg);
		goto error;
	}

	genlmsg_end(skb, msg);

	SCSC_TAG_INFO(WLBTD, "finalize & send msg\n");
	rc = genlmsg_multicast_allns(&scsc_mdp_fam, skb, 0, 0, GFP_KERNEL);

	if (rc) {
		SCSC_TAG_ERR(WLBTD, "Failed to send message. rc = %d\n", rc);
		goto error;
	}

	SCSC_TAG_INFO(WLBTD, "waiting for completion\n");

	/* wait for moredump to finish */
	ms_for_moredump = wait_for_completion_timeout(&moredump_completion,
						msecs_to_jiffies(MAX_TIMEOUT));
	ms_for_moredump = MAX_TIMEOUT - jiffies_to_msecs(ms_for_moredump);

	SCSC_TAG_INFO(WLBTD, "moredump done in %d seconds\n",
				(int)(ms_for_moredump/1000U));

	SCSC_TAG_DEBUG(WLBTD, "end\n");

	return rc;

error:
	if (rc == -ESRCH) {
		/* If no one registered to scsc_mdp_mcgrp (e.g. in case wlbtd
		 * is not running) genlmsg_multicast_allns returns -ESRCH.
		 * Ignore and return.
		 */
		SCSC_TAG_WARNING(WLBTD, "WLBTD not running ?\n");
		return rc;
	}

	/* free skb */
	nlmsg_free(skb);

	return -1;
}

int scsc_wlbtd_deinit(void)
{
	int ret = 0;

	/* unregister family */
	ret = genl_unregister_family(&scsc_mdp_fam);
	if (ret) {
		SCSC_TAG_ERR(WLBTD, "genl_unregister_family failed (%d)\n",
				ret);
		return -1;
	}

	return ret;
}
