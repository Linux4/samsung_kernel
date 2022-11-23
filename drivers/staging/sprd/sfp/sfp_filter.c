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
#include <linux/types.h>

#include "sfp.h"
#include "sfp_ipa.h"

#define IPA_FILTER_DEFAULT_NUM 100
#define IPA_FILTER_V4_ENTRY_DEPTH 32
#define IPA_FILTER_V6_ENTRY_DEPTH 56
#define IPA_FILTER_V4_TOTAL_SIZE \
	(IPA_FILTER_V4_ENTRY_DEPTH * IPA_FILTER_DEFAULT_NUM)
#define IPA_FILTER_V6_TOTAL_SIZE \
	(IPA_FILTER_V6_ENTRY_DEPTH * IPA_FILTER_DEFAULT_NUM)

struct list_head sfp_filter_pre4_entries;
struct list_head sfp_filter_post4_entries;
struct list_head sfp_filter_pre6_entries;
struct list_head sfp_filter_post6_entries;

spinlock_t filter_lock; /* Spinlock for sfp filter entry */

struct sfp_ipa_filter_tbls ipa_filter_tbls;

static void sfp_clear_all_ipa_filter_tbl(void)
{
	memset(ipa_filter_tbls.pre4.v_addr,
	       0, IPA_FILTER_V4_TOTAL_SIZE);
	memset(ipa_filter_tbls.pre6.v_addr,
	       0, IPA_FILTER_V6_TOTAL_SIZE);
	memset(ipa_filter_tbls.post4.v_addr,
	       0, IPA_FILTER_V4_TOTAL_SIZE);
	memset(ipa_filter_tbls.post6.v_addr,
	       0, IPA_FILTER_V6_TOTAL_SIZE);
}

static int sfp_ipa_filter_alloc_tbl(void)
{
	u8 *v1, *v2, *v3, *v4;
	dma_addr_t h1, h2, h3, h4;
	int ret = 0;

	v1 = (u8 *)dma_ipa_alloc(NULL, IPA_FILTER_V4_TOTAL_SIZE,
				 &h1, GFP_KERNEL);

	if (!v1) {
		FP_PRT_DBG(FP_PRT_ERR, "IPA: alloc filter pre 4 fail!!\n");
		ret = -ENOMEM;
		goto out1;
	}
	ipa_filter_tbls.pre4.v_addr = v1;
	ipa_filter_tbls.pre4.handle = h1;

	v2 = (u8 *)dma_ipa_alloc(NULL, IPA_FILTER_V6_TOTAL_SIZE,
				 &h2, GFP_KERNEL);

	if (!v2) {
		FP_PRT_DBG(FP_PRT_ERR, "IPA: alloc filter pre 6 fail!!\n");
		ret = -ENOMEM;
		goto out2;
	}
	ipa_filter_tbls.pre6.v_addr = v2;
	ipa_filter_tbls.pre6.handle = h2;

	v3 = (u8 *)dma_ipa_alloc(NULL, IPA_FILTER_V4_TOTAL_SIZE,
				 &h3, GFP_KERNEL);

	if (!v3) {
		FP_PRT_DBG(FP_PRT_ERR, "IPA: alloc filter post 4 fail!!\n");
		ret = -ENOMEM;
		goto out3;
	}
	ipa_filter_tbls.post4.v_addr = v3;
	ipa_filter_tbls.post4.handle = h3;

	v4 = (u8 *)dma_ipa_alloc(NULL, IPA_FILTER_V6_TOTAL_SIZE,
				 &h4, GFP_KERNEL);

	if (!v4) {
		FP_PRT_DBG(FP_PRT_ERR, "IPA: alloc filter post 6 fail!!\n");
		ret = -ENOMEM;
		goto out4;
	}
	ipa_filter_tbls.post6.v_addr = v4;
	ipa_filter_tbls.post6.handle = h4;

	/*zero filter tbl*/
	sfp_clear_all_ipa_filter_tbl();

	return ret;

out4:
	dma_ipa_free(NULL, IPA_FILTER_V4_TOTAL_SIZE, v3, h3);
out3:
	dma_ipa_free(NULL, IPA_FILTER_V6_TOTAL_SIZE, v2, h2);
out2:
	dma_ipa_free(NULL, IPA_FILTER_V4_TOTAL_SIZE, v1, h1);
out1:
	return ret;
}

int sfp_init_ipa_filter_tbl(void)
{
	int ret = 0;

	ret = sfp_ipa_filter_alloc_tbl();
	if (ret == -ENOMEM)
		return ret;

	spin_lock_init(&ipa_filter_tbls.pre4.sp_lock);
	spin_lock_init(&ipa_filter_tbls.pre6.sp_lock);
	spin_lock_init(&ipa_filter_tbls.post4.sp_lock);
	spin_lock_init(&ipa_filter_tbls.post6.sp_lock);

	atomic_set(&ipa_filter_tbls.pre4.entry_cnt, 0);
	atomic_set(&ipa_filter_tbls.pre6.entry_cnt, 0);
	atomic_set(&ipa_filter_tbls.post4.entry_cnt, 0);
	atomic_set(&ipa_filter_tbls.post6.entry_cnt, 0);

	memset(&ipa_filter_tbls.pre4.sipa_filter, 0,
	       sizeof(struct sipa_filter_tbl));
	memset(&ipa_filter_tbls.pre4.sipa_filter, 0,
	       sizeof(struct sipa_filter_tbl));
	memset(&ipa_filter_tbls.pre4.sipa_filter, 0,
	       sizeof(struct sipa_filter_tbl));
	memset(&ipa_filter_tbls.pre4.sipa_filter, 0,
	       sizeof(struct sipa_filter_tbl));

	return ret;
}

void sfp_print_hash_tbl(char *p, int len)
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
	pr_info("%s\n\n", str);
}

static int sfp_ipa_filter_config_v4(enum sfp_chain chain,
				    struct sfp_ipa_filter_tbl *tbl)
{
	int i;
	int ret = 0;

	tbl->sipa_filter.depth = atomic_read(&tbl->entry_cnt);
	tbl->sipa_filter.is_ipv4 = true;
	tbl->sipa_filter.filter_pre_rule = tbl->v_addr;

	pr_info("%s depth %d is_ipv4 %d pre_rule %pK\n",
		__func__,
		tbl->sipa_filter.depth,
		tbl->sipa_filter.is_ipv4,
		tbl->sipa_filter.filter_pre_rule);

	for (i = 0; i < atomic_read(&tbl->entry_cnt); i++) {
		struct sfp_filter_v4_rule *v4_rule =
			(struct sfp_filter_v4_rule *)(tbl->v_addr) + i;

		pr_info("i %d addr of v4_rule is %pK\n", i, v4_rule);

		pr_info("%d/%d %d/%d %d-%d %d-%d t-%d c[%d-%d] p %d inv %d-%s",
			v4_rule->src, v4_rule->src_mask, v4_rule->dst,
			v4_rule->dst_mask, v4_rule->min_spts,
			v4_rule->max_spts, v4_rule->min_dpts,
			v4_rule->max_dpts, v4_rule->type,
			v4_rule->min_code, v4_rule->max_code,
			v4_rule->proto, v4_rule->invflags,
			v4_rule->verdict == 0 ? "DROP" : "PASS");
	}

	if (chain == SFP_CHAIN_PRE)
		ret = sipa_config_ifilter(&tbl->sipa_filter);
	else
		ret = sipa_config_ofilter(&tbl->sipa_filter);
	return ret;
}

static int sfp_ipa_filter_config_v6(enum sfp_chain chain,
				    struct sfp_ipa_filter_tbl *tbl)
{
	int ret = 0;

	tbl->sipa_filter.depth = atomic_read(&tbl->entry_cnt);
	tbl->sipa_filter.is_ipv6 = true;
	tbl->sipa_filter.filter_pre_rule = tbl->v_addr;
	if (chain == SFP_CHAIN_PRE)
		ret = sipa_config_ifilter(&tbl->sipa_filter);
	else
		ret = sipa_config_ofilter(&tbl->sipa_filter);
	return ret;
}

int sfp_ipa_filter_sync(enum sfp_rule_type type)
{
	int ret = 0;
	struct sfp_ipa_filter_tbl *tbl;
	struct sfp_filter_v4_node *node4;
	struct sfp_filter_v6_node *node6;

	switch (type) {
	case SFP_RULE_TYPE_V4_PRE:
		tbl = &ipa_filter_tbls.pre4;
		atomic_set(&tbl->entry_cnt, 0);
		memset(tbl->v_addr, 0, IPA_FILTER_V4_TOTAL_SIZE);
		list_for_each_entry(node4, &sfp_filter_pre4_entries, list) {
			struct sfp_filter_v4_rule *v4_rule =
				(struct sfp_filter_v4_rule *)(tbl->v_addr) +
					atomic_read(&tbl->entry_cnt);
			memcpy(v4_rule, &node4->rule4, sizeof(*v4_rule));
			atomic_inc(&tbl->entry_cnt);
		}
		pr_info("pre4 entry_cnt %d\n", atomic_read(&tbl->entry_cnt));
		ret = sfp_ipa_filter_config_v4(SFP_CHAIN_PRE, tbl);
		break;
	case SFP_RULE_TYPE_V4_POST:
		tbl = &ipa_filter_tbls.post4;
		atomic_set(&tbl->entry_cnt, 0);
		memset(tbl->v_addr, 0, IPA_FILTER_V4_TOTAL_SIZE);
		list_for_each_entry(node4, &sfp_filter_post4_entries, list) {
			struct sfp_filter_v4_rule *v4_rule =
				(struct sfp_filter_v4_rule *)(tbl->v_addr) +
					atomic_read(&tbl->entry_cnt);
			memcpy(v4_rule, &node4->rule4, sizeof(*v4_rule));
			atomic_inc(&tbl->entry_cnt);
		}
		pr_info("post4 entry_cnt %d\n", atomic_read(&tbl->entry_cnt));
		ret = sfp_ipa_filter_config_v4(SFP_CHAIN_POST, tbl);
		break;
	case SFP_RULE_TYPE_V6_PRE:
		tbl = &ipa_filter_tbls.pre6;
		atomic_set(&tbl->entry_cnt, 0);
		memset(tbl->v_addr, 0, IPA_FILTER_V6_TOTAL_SIZE);
		list_for_each_entry(node6, &sfp_filter_pre6_entries, list) {
			struct sfp_filter_v6_rule *v6_rule =
				(struct sfp_filter_v6_rule *)(tbl->v_addr) +
					atomic_read(&tbl->entry_cnt);
			memcpy(v6_rule, &node6->rule6, sizeof(*v6_rule));
			atomic_inc(&tbl->entry_cnt);
		}
		ret = sfp_ipa_filter_config_v6(SFP_CHAIN_PRE, tbl);
		break;
	case SFP_RULE_TYPE_V6_POST:
		tbl = &ipa_filter_tbls.post6;
		atomic_set(&tbl->entry_cnt, 0);
		memset(tbl->v_addr, 0, IPA_FILTER_V6_TOTAL_SIZE);
		list_for_each_entry(node6, &sfp_filter_post6_entries, list) {
			struct sfp_filter_v6_rule *v6_rule =
				(struct sfp_filter_v6_rule *)(tbl->v_addr) +
					atomic_read(&tbl->entry_cnt);
			memcpy(v6_rule, &node6->rule6, sizeof(*v6_rule));
			atomic_inc(&tbl->entry_cnt);
		}
		ret = sfp_ipa_filter_config_v6(SFP_CHAIN_POST, tbl);
		break;
	default:
		pr_err("unknown rule type\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}
