/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "sfp: " fmt

#include <net/genetlink.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "sfp.h"

#define SFP_GENL_NAME		"sfp"
#define SFP_GENL_VERSION	0x1
#define SFP_RULE_SIZE 200

static struct genl_family sfp_genl_family;

static struct nla_policy sfp_genl_policy[SFP_A_MAX + 1] = {
	[SFP_A_FILTER] = { .len = sizeof(struct sfp_nl_filter) },
};

static int sfp_nl_filter_validation(struct sfp_nl_filter *filter)
{
	int err = 0;
	int chain = filter->chain;
	int family = filter->af_family;

	if (chain <= __SFP_CHAIN_UNSPEC || chain >= SFP_CHAIN_MAX) {
		pr_err("illegal chain, %d\n", chain);
		err = -EINVAL;
		goto out;
	}

	if (family != AF_INET && family != AF_INET6) {
		pr_err("illegal family, %d\n", family);
		err = -EINVAL;
		goto out;
	}

out:
	return err;
}

static int sfp_nl_get_rule_type(struct sfp_nl_filter *filter)
{
	if (filter->af_family == AF_INET) {
		if (filter->chain == SFP_CHAIN_PRE)
			return SFP_RULE_TYPE_V4_PRE;
		else if (filter->chain == SFP_CHAIN_POST)
			return SFP_RULE_TYPE_V4_POST;
	} else if (filter->af_family == AF_INET6) {
		if (filter->chain == SFP_CHAIN_PRE)
			return SFP_RULE_TYPE_V6_PRE;
		else if (filter->chain == SFP_CHAIN_POST)
			return SFP_RULE_TYPE_V6_POST;
	}
	return SFP_RULE_TYPE_UNSPEC;
}

static int sfp_ipa_filter_sync_ext(enum sfp_rule_type type)
{
#ifdef CONFIG_SPRD_IPA_V3_SUPPORT
	return sfp_ipa_filter_sync(type);
#else
	return 0;
#endif
}

int sfp_nl_add_rule(struct sfp_nl_filter *filter, bool insert)
{
	int ret = 0;
	struct sfp_filter_v4_rule *rule4;
	struct sfp_filter_v6_rule *rule6;
	struct sfp_filter_v4_node *node4;
	struct sfp_filter_v6_node *node6;
	enum sfp_rule_type type;

	spin_lock(&filter_lock);

	type = sfp_nl_get_rule_type(filter);
	switch (type) {
	case SFP_RULE_TYPE_V4_PRE:
		node4 = kzalloc(sizeof(*node4), GFP_KERNEL);
		if (!node4) {
			spin_unlock(&filter_lock);
			return -ENOMEM;
		}

		memcpy(&node4->rule4, &filter->v4_rule, sizeof(*rule4));
		if (insert)
			list_add(&node4->list, &sfp_filter_pre4_entries);
		else
			list_add_tail(&node4->list, &sfp_filter_pre4_entries);
		break;
	case SFP_RULE_TYPE_V4_POST:
		node4 = kzalloc(sizeof(*node4), GFP_KERNEL);
		if (!node4) {
			spin_unlock(&filter_lock);
			return -ENOMEM;
		}

		memcpy(&node4->rule4, &filter->v4_rule, sizeof(*rule4));
		if (insert)
			list_add(&node4->list, &sfp_filter_post4_entries);
		else
			list_add_tail(&node4->list, &sfp_filter_post4_entries);
		break;
	case SFP_RULE_TYPE_V6_PRE:
		node6 = kzalloc(sizeof(*node6), GFP_KERNEL);
		if (!node6) {
			spin_unlock(&filter_lock);
			return -ENOMEM;
		}
		memcpy(&node6->rule6, &filter->v6_rule, sizeof(*rule6));

		if (insert)
			list_add(&node6->list, &sfp_filter_pre6_entries);
		else
			list_add_tail(&node6->list, &sfp_filter_pre6_entries);
		break;
	case SFP_RULE_TYPE_V6_POST:
		node6 = kzalloc(sizeof(*node6), GFP_KERNEL);
		if (!node6) {
			spin_unlock(&filter_lock);
			return -ENOMEM;
		}
		memcpy(&node6->rule6, &filter->v6_rule, sizeof(*rule6));

		if (insert)
			list_add(&node6->list, &sfp_filter_post6_entries);
		else
			list_add_tail(&node6->list, &sfp_filter_post6_entries);
		break;
	default:
		pr_err("unknown rule type %d\n", type);
		spin_unlock(&filter_lock);
		return -EINVAL;
	}

	ret = sfp_ipa_filter_sync_ext(type);

	spin_unlock(&filter_lock);

	return ret;
}

static int sfp_nl_do_append_rule(struct sk_buff *skb, struct genl_info *info)
{
	int err = 0;
	struct nlattr *nla;
	struct sfp_nl_filter *filter;

	pr_info("do %s\n", __func__);
	nla = info->attrs[SFP_A_FILTER];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	filter = (struct sfp_nl_filter *)nla_data(nla);

	err = sfp_nl_filter_validation(filter);
	if (err)
		return err;

	err = sfp_nl_add_rule(filter, false);
	return err;
}

static int sfp_nl_do_insert_rule(struct sk_buff *skb, struct genl_info *info)
{
	int err = 0;
	struct nlattr *nla;
	struct sfp_nl_filter *filter;

	pr_info("do %s\n", __func__);
	nla = info->attrs[SFP_A_FILTER];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	filter = (struct sfp_nl_filter *)nla_data(nla);

	err = sfp_nl_filter_validation(filter);
	if (err)
		return err;

	err = sfp_nl_add_rule(filter, false);
	return err;
}

static bool sfp_nl_rule4_compare(struct sfp_filter_v4_rule *r1,
				 struct sfp_filter_v4_rule *r2)
{
	return (r1->src == r2->src && r1->dst == r2->dst &&
		r1->src_mask == r2->src_mask &&
		r1->dst_mask == r2->dst_mask &&
		r1->iif == r2->iif && r1->oif == r2->oif &&
		r1->max_spts == r2->max_spts &&
		r1->min_spts == r2->min_spts &&
		r1->max_dpts == r2->max_dpts &&
		r1->min_dpts == r2->min_dpts &&
		r1->type == r2->type &&
		r1->max_code == r2->max_code &&
		r1->min_code == r2->min_code &&
		r1->proto == r2->proto &&
		r1->invflags == r2->invflags &&
		r1->reserve == r2->reserve &&
		r1->verdict == r2->verdict);
}

static bool sfp_nl_rule6_compare(struct sfp_filter_v6_rule *r1,
				 struct sfp_filter_v6_rule *r2)
{
	return (r1->src[0] == r2->src[0] && r1->src[1] == r2->src[1] &&
		r1->src[2] == r2->src[2] && r1->src[3] == r2->src[3] &&
		r1->dst[0] == r2->dst[0] && r1->dst[1] == r2->dst[1] &&
		r1->dst[2] == r2->dst[2] && r1->dst[3] == r2->dst[3] &&
		r1->src_mask == r2->src_mask &&
		r1->dst_mask == r2->dst_mask &&
		r1->iif == r2->iif && r1->oif == r2->oif &&
		r1->max_spts == r2->max_spts &&
		r1->min_spts == r2->min_spts &&
		r1->max_dpts == r2->max_dpts &&
		r1->min_dpts == r2->min_dpts &&
		r1->type == r2->type &&
		r1->max_code == r2->max_code &&
		r1->min_code == r2->min_code &&
		r1->proto == r2->proto &&
		r1->invflags == r2->invflags &&
		r1->reserve == r2->reserve &&
		r1->verdict == r2->verdict);
}

int sfp_nl_del_rule(struct sfp_nl_filter *filter, struct genl_info *info)
{
	int ret = 0;
	bool match = false;
	struct sfp_filter_v4_node *node4, *tmp4;
	struct sfp_filter_v6_node *node6, *tmp6;
	enum sfp_rule_type type;

	spin_lock(&filter_lock);

	type = sfp_nl_get_rule_type(filter);
	switch (type) {
	case SFP_RULE_TYPE_V4_PRE:
		list_for_each_entry_safe(node4, tmp4,
					 &sfp_filter_pre4_entries, list) {
			match = sfp_nl_rule4_compare(&node4->rule4,
						     &filter->v4_rule);
			if (match) {
				list_del(&node4->list);
				kfree(node4);
				break;
			}
		}
		break;
	case SFP_RULE_TYPE_V4_POST:
		list_for_each_entry_safe(node4, tmp4,
					 &sfp_filter_post4_entries, list) {
			match = sfp_nl_rule4_compare(&node4->rule4,
						     &filter->v4_rule);
			if (match) {
				list_del(&node4->list);
				pr_info("post4 rule match, delete it\n");
				kfree(node4);
				break;
			}
		}
		break;
	case SFP_RULE_TYPE_V6_PRE:
		list_for_each_entry_safe(node6, tmp6,
					 &sfp_filter_pre6_entries, list) {
			match = sfp_nl_rule6_compare(&node6->rule6,
						     &filter->v6_rule);
			if (match) {
				list_del(&node6->list);
				pr_info("pre6 rule match, delete it\n");
				kfree(node6);
				break;
			}
		}
		break;
	case SFP_RULE_TYPE_V6_POST:
		list_for_each_entry_safe(node6, tmp6,
					 &sfp_filter_post6_entries, list) {
			match = sfp_nl_rule6_compare(&node6->rule6,
						     &filter->v6_rule);
			if (match) {
				list_del(&node6->list);
				pr_info("post6 rule match, delete it\n");
				kfree(node6);
				break;
			}
		}
		break;
	default:
		pr_err("unknown rule type %d\n", type);
		spin_unlock(&filter_lock);
		return -EINVAL;
	}

	if (!match) {
		struct sk_buff *reply_skb;
		void *hdr;

		spin_unlock(&filter_lock);
		pr_info("no rule is matched\n");

		reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
		if (!reply_skb) {
			ret = -ENOMEM;
			goto out;
		}

		hdr = genlmsg_put(reply_skb, info->snd_portid, info->snd_seq,
				  &sfp_genl_family, 0, SFP_NL_CMD_DELETE);
		if (!hdr) {
			nlmsg_free(reply_skb);
			ret = -EMSGSIZE;
			goto out;
		}
		if (nla_put_u32(reply_skb, SFP_NLA_RULE_NO_ENTRY, match)) {
			nlmsg_free(reply_skb);
			ret = -ENOBUFS;
			goto out;
		}
		genlmsg_end(reply_skb, hdr);
		genlmsg_reply(reply_skb, info);
		return -EINVAL;
	}
	ret = sfp_ipa_filter_sync_ext(type);
	spin_unlock(&filter_lock);

out:
	return ret;
}

static int sfp_nl_do_delete_rule(struct sk_buff *skb, struct genl_info *info)
{
	int err;
	struct nlattr *nla;
	struct sfp_nl_filter *filter;

	pr_info("sfp_do_delete_rule\n");

	nla = info->attrs[SFP_A_FILTER];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}
	filter = (struct sfp_nl_filter *)nla_data(nla);
	err = sfp_nl_filter_validation(filter);
	if (err)
		return err;

	err = sfp_nl_del_rule(filter, info);
	return err;
}

int sfp_nl_flush_rule(struct sfp_nl_filter *filter)
{
	int ret = 0;
	struct sfp_filter_v4_node *node4, *tmp4;
	struct sfp_filter_v6_node *node6, *tmp6;
	enum sfp_rule_type type;

	spin_lock(&filter_lock);
	type = sfp_nl_get_rule_type(filter);
	switch (type) {
	case SFP_RULE_TYPE_V4_PRE:
		list_for_each_entry_safe(node4, tmp4,
					 &sfp_filter_pre4_entries, list) {
			list_del(&node4->list);
			kfree(node4);
		}
		break;
	case SFP_RULE_TYPE_V4_POST:
		list_for_each_entry_safe(node4, tmp4,
					 &sfp_filter_post4_entries, list) {
			list_del(&node4->list);
			kfree(node4);
		}
		break;
	case SFP_RULE_TYPE_V6_PRE:
		list_for_each_entry_safe(node6, tmp6,
					 &sfp_filter_pre6_entries, list) {
			list_del(&node6->list);
			kfree(node6);
		}
		break;
	case SFP_RULE_TYPE_V6_POST:
		list_for_each_entry_safe(node6, tmp6,
					 &sfp_filter_post6_entries, list) {
			list_del(&node6->list);
			kfree(node6);
		}
		break;
	default:
		pr_err("unknown rule type %d\n", type);
		spin_unlock(&filter_lock);
		return -EINVAL;
	}

	ret = sfp_ipa_filter_sync_ext(type);
	spin_unlock(&filter_lock);

	return ret;
}

static int sfp_nl_do_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	int err;
	struct nlattr *nla;
	struct sfp_nl_filter *filter;

	pr_info("sfp_do_flush_rule\n");

	nla = info->attrs[SFP_A_FILTER];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}
	filter = (struct sfp_nl_filter *)nla_data(nla);
	err = sfp_nl_filter_validation(filter);
	if (err)
		return err;

	err = sfp_nl_flush_rule(filter);
	return err;
}

static void sfp_nl_print_rule(char *p, int len)
{
	int i;
	char str[60] = {0};
	char *off = str;

	if (!p)
		return;

	for (i = 0; i < len; i++) {
		if (i > 0 && i % 16 == 0) {
			pr_info("%s", str);
			memset(str, 0, sizeof(str));
			off = str;
		}
		off += sprintf(off, "%02x ", *(p + i));
	}
	pr_info("%s\n", str);
}

static int sfp_nl_do_list_rule(struct sk_buff *skb, struct genl_info *info)
{
	int err = 0;
	struct nlattr *nla;
	struct sfp_nl_filter *filter;
	struct sfp_filter_v4_node *node4;
	struct sfp_filter_v6_node *node6;
	struct sk_buff *reply_skb;
	void *hdr;
	enum sfp_rule_type type;

	pr_info("do %s\n", __func__);

	nla = info->attrs[SFP_A_FILTER];
	if (!nla) {
		pr_err("tuple attr not exist!");
		return -EINVAL;
	}

	filter = (struct sfp_nl_filter *)nla_data(nla);
	err = sfp_nl_filter_validation(filter);
	if (err)
		return err;

	spin_lock(&filter_lock);
	type = sfp_nl_get_rule_type(filter);
	switch (type) {
	case SFP_RULE_TYPE_V4_PRE:
		pr_info("FILTER PRE CHAIN - V4: ");
		list_for_each_entry(node4, &sfp_filter_pre4_entries, list) {
			sfp_nl_print_rule((char *)&node4->rule4,
					  sizeof(struct sfp_filter_v4_rule));
			reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
			if (!reply_skb) {
				err = -ENOMEM;
				goto out;
			}

			hdr = genlmsg_put(reply_skb, info->snd_portid,
					  info->snd_seq, &sfp_genl_family,
					  0, SFP_NL_CMD_LIST);
			if (!hdr) {
				nlmsg_free(reply_skb);
				err = -EMSGSIZE;
				goto out;
			}
			if (nla_put(reply_skb, SFP_NLA_RULE_PRE4,
				    sizeof(struct sfp_filter_v4_rule),
				    &node4->rule4)) {
				nlmsg_free(reply_skb);
				err = -ENOBUFS;
				goto out;
			}
			genlmsg_end(reply_skb, hdr);
			genlmsg_reply(reply_skb, info);
		}
		spin_unlock(&filter_lock);
		/* The reason we return 1 is to force userspace nl_recvmsgs
		 * func to get a non-zero ret, so it can break from its
		 * while loop.
		 */
		return 1;
	case SFP_RULE_TYPE_V4_POST:
		pr_info("FILTER PRE CHAIN - V6: ");
		list_for_each_entry(node4, &sfp_filter_post4_entries, list) {
			sfp_nl_print_rule((char *)&node4->rule4,
					  sizeof(struct sfp_filter_v4_rule));
			reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
			if (!reply_skb) {
				err = -ENOMEM;
				goto out;
			}

			hdr = genlmsg_put(reply_skb, info->snd_portid,
					  info->snd_seq, &sfp_genl_family,
					  0, SFP_NL_CMD_LIST);
			if (!hdr) {
				nlmsg_free(reply_skb);
				err = -EMSGSIZE;
				goto out;
			}
			if (nla_put(reply_skb, SFP_NLA_RULE_POST4,
				    sizeof(struct sfp_filter_v4_rule),
				    &node4->rule4)) {
				nlmsg_free(reply_skb);
				err = -ENOBUFS;
				goto out;
			}
			genlmsg_end(reply_skb, hdr);
			genlmsg_reply(reply_skb, info);
		}
		spin_unlock(&filter_lock);
		return 1;
	case SFP_RULE_TYPE_V6_PRE:
		pr_info("FILTER PRE CHAIN - V6: ");
		list_for_each_entry(node6, &sfp_filter_pre6_entries, list) {
			sfp_nl_print_rule((char *)&node6->rule6,
					  sizeof(struct sfp_filter_v6_rule));
			reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
			if (!reply_skb) {
				err = -ENOMEM;
				goto out;
			}

			hdr = genlmsg_put(reply_skb, info->snd_portid,
					  info->snd_seq, &sfp_genl_family,
					  0, SFP_NL_CMD_LIST);
			if (!hdr) {
				nlmsg_free(reply_skb);
				err = -EMSGSIZE;
				goto out;
			}
			if (nla_put(reply_skb, SFP_NLA_RULE_PRE6,
				    sizeof(struct sfp_filter_v6_rule),
				    &node6->rule6)) {
				nlmsg_free(reply_skb);
				err = -ENOBUFS;
				goto out;
			}
			genlmsg_end(reply_skb, hdr);
			genlmsg_reply(reply_skb, info);
		}
		spin_unlock(&filter_lock);
		return 1;
	case SFP_RULE_TYPE_V6_POST:
		pr_info("FILTER POST CHAIN - V6: ");
		list_for_each_entry(node6, &sfp_filter_post6_entries, list) {
			sfp_nl_print_rule((char *)&node6->rule6,
					  sizeof(struct sfp_filter_v6_rule));
			reply_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
			if (!reply_skb) {
				err = -ENOMEM;
				goto out;
			}

			hdr = genlmsg_put(reply_skb, info->snd_portid,
					  info->snd_seq, &sfp_genl_family,
					  0, SFP_NL_CMD_LIST);
			if (!hdr) {
				nlmsg_free(reply_skb);
				err = -EMSGSIZE;
				goto out;
			}
			if (nla_put(reply_skb, SFP_NLA_RULE_POST6,
				    sizeof(struct sfp_filter_v6_rule),
				    &node6->rule6)) {
				nlmsg_free(reply_skb);
				err = -ENOBUFS;
				goto out;
			}
			genlmsg_end(reply_skb, hdr);
			genlmsg_reply(reply_skb, info);
		}
		spin_unlock(&filter_lock);
		return 1;
	default:
		pr_err("unknown rule type %d\n", type);
		spin_unlock(&filter_lock);
		return -EINVAL;
	}

out:
	if (err)
		pr_err("fail to do list cmd err %d\n", err);
	spin_unlock(&filter_lock);

	return err;
}

static struct genl_ops sfp_genl_ops[] = {
	{
		.cmd = SFP_NL_CMD_APPEND,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_append_rule,
	},
	{
		.cmd = SFP_NL_CMD_INSERT,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_insert_rule,
	},
	{
		.cmd = SFP_NL_CMD_DELETE,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_delete_rule,
	},
	{
		.cmd = SFP_NL_CMD_FLUSH,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_flush_rule,
	},
	{
		.cmd = SFP_NL_CMD_LIST,
		.flags = GENL_ADMIN_PERM,
		.policy = sfp_genl_policy,
		.doit = sfp_nl_do_list_rule,
	},
};

static struct genl_family sfp_genl_family = {
	.hdrsize	= 0,
	.name		= SFP_GENL_NAME,
	.version	= SFP_GENL_VERSION,
	.maxattr	= SFP_A_MAX,
	.ops		= sfp_genl_ops,
	.n_ops		= ARRAY_SIZE(sfp_genl_ops),
};

int __init sfp_netlink_init(void)
{
	int err;

	INIT_LIST_HEAD(&sfp_filter_pre4_entries);
	INIT_LIST_HEAD(&sfp_filter_pre6_entries);

	INIT_LIST_HEAD(&sfp_filter_post4_entries);
	INIT_LIST_HEAD(&sfp_filter_post6_entries);

	spin_lock_init(&filter_lock);
	err = genl_register_family(&sfp_genl_family);

	return err;
}

void sfp_netlink_exit(void)
{
	genl_unregister_family(&sfp_genl_family);
}

