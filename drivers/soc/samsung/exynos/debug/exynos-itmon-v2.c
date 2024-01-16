/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver v2 for Samsung Exynos SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched/clock.h>
#include <linux/panic_notifier.h>
#include <soc/samsung/exynos/exynos-itmon.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/debug-snapshot-log.h>
#include "exynos-itmon-local-v2.h"
#include "debug-snapshot-local.h"
#include <linux/sec_debug.h>

static struct itmon_policy err_policy[] = {
	[TMOUT]		= {"err_tmout",		0, 0, 0, false},
	[PRTCHKER]	= {"err_prtchker",	0, 0, 0, false},
	[DECERR]	= {"err_decerr",	0, 0, 0, false},
	[SLVERR]	= {"err_slverr",	0, 0, 0, false},
	[FATAL]		= {"err_fatal",		0, 0, 0, false},
};

static const char * const itmon_dpm_action[] = {
	GO_DEFAULT,
	GO_PANIC,
	GO_WATCHDOG,
	GO_S2D,
	GO_ARRAYDUMP,
	GO_SCANDUMP,
	GO_HALT,
};

static const char *itmon_pathtype[] = {
	"DATA Path transaction",
	"Configuration(SFR) Path transaction",
};

/* Error Code Description */
const static char *itmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Inteded Access Violation",
	"Unsupported transaction error",
	"Timeout error - response timeout in timeout value",
	"Unsupported transaction error",
	"Protocol Checker Error",
	"Protocol Checker Error",
	"Protocol Checker Error",
	"Protocol Checker Error",
	"Protocol Checker Error",
	"Protocol Checker Error",
	"Unsupported transaction error",
	"Timeout error - freeze mode",
};

/* Error Code Description */
const static char *itmon_errcode_short[] = {
	"SLVERR",
	"DECERR",
	"UNSUPPORTED",
	"PWRDOWNACCESS",
	"INTENDED",
	"UNSUPPORTED",
	"TIMEOUT",
	"UNSUPPORTED",
	"PRTCHKERR",
	"PRTCHKERR",
	"PRTCHKERR",
	"PRTCHKERR",
	"PRTCHKERR",
	"PRTCHKERR",
	"UNSUPPORTED",
	"TIMEOUT_FREEZE",
};

const static char *itmon_node_string[] = {
	"M_NODE",
	"TAXI_S_NODE",
	"TAXI_M_NODE",
	"S_NODE",
};

const static char *itmon_cpu_node_string[] = {
	"CLUSTER0_P",
	"M_CPU",
	"SCI_IRPM",
	"SCI_CCM",
	"CCI",
};

const static unsigned int itmon_invalid_addr[] = {
	0x03000000,
	0x04000000,
};

static struct itmon_history *p_itmon_history;

static struct itmon_dev *g_itmon;

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(itmon_notifier_list);

static const struct of_device_id itmon_dt_match[] = {
	{.compatible = "samsung,exynos-itmon-v2",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, itmon_dt_match);

struct itmon_nodeinfo *itmon_get_nodeinfo_by_group(struct itmon_dev *itmon,
						  struct itmon_nodegroup *group,
						  const char *name)
{
	struct itmon_nodeinfo *node = group->nodeinfo;
	int i;

	for (i = 0; i < group->nodesize; i++)
		if (!strncmp(node[i].name, name, strlen(name)))
			return &node[i];

	return NULL;
}

struct itmon_nodeinfo *itmon_get_nodeinfo(struct itmon_dev *itmon,
						 struct itmon_nodegroup *group,
						 const char *name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	int i;

	if (!name)
		return NULL;

	if (group) {
		return itmon_get_nodeinfo_by_group(itmon, group, name);
	} else {
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			node = itmon_get_nodeinfo_by_group(itmon, group, name);
			if (node)
				return node;
		}
	}

	return NULL;
}

struct itmon_nodeinfo *itmon_get_nodeinfo_group_by_eid(struct itmon_dev *itmon,
							  struct itmon_nodegroup *group,
							  u32 err_id)
{
	struct itmon_nodeinfo *node = group->nodeinfo;
	int i;

	for (i = 0; i < group->nodesize; i++) {
		if (node[i].err_id == err_id)
			return &node[i];
	}

	return NULL;
}

struct itmon_nodeinfo *itmon_get_nodeinfo_by_eid(struct itmon_dev *itmon,
						 struct itmon_nodegroup *group,
						 int path_type, u32 err_id)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	int i;

	if (group) {
		return itmon_get_nodeinfo_group_by_eid(itmon, group, err_id);
	} else {
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			if (group->path_type != path_type)
				continue;
			node = itmon_get_nodeinfo_group_by_eid(itmon, group, err_id);
			if (node)
				return node;
		}
	}

	return NULL;
}

struct itmon_nodeinfo *itmon_get_nodeinfo_group_by_tmout_offset(struct itmon_dev *itmon,
							  struct itmon_nodegroup *group,
							  u32 tmout_offset)
{
	struct itmon_nodeinfo *node = group->nodeinfo;
	int i;

	for (i = 0; i < group->nodesize; i++) {
		if (node[i].tmout_offset == tmout_offset)
			return &node[i];
	}

	return NULL;
}

struct itmon_nodeinfo *itmon_get_nodeinfo_by_tmout_offset(struct itmon_dev *itmon,
						 struct itmon_nodegroup *group,
						 u32 tmout_offset)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	int i;

	if (group) {
		return itmon_get_nodeinfo_group_by_tmout_offset(itmon, group, tmout_offset);
	} else {
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			node = itmon_get_nodeinfo_group_by_tmout_offset(itmon, group, tmout_offset);
			if (node)
				return node;
		}
	}

	return NULL;
}

static struct itmon_rpathinfo *itmon_get_rpathinfo(struct itmon_dev *itmon,
						   unsigned int id,
						   char *dest_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	if (!dest_name)
		return NULL;

	for (i = 0; i < pdata->num_rpathinfo; i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (!strncmp(pdata->rpathinfo[i].dest_name, dest_name,
				  strlen(pdata->rpathinfo[i].dest_name)))
				return &pdata->rpathinfo[i];
		}
	}

	return NULL;
}

static char *itmon_get_masterinfo(struct itmon_dev *itmon,
				     char *port_name,
				     u32 user)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	if (!port_name)
		return NULL;

	for (i = 0; i < pdata->num_masterinfo; i++) {
		if ((user & pdata->masterinfo[i].bits) ==
			pdata->masterinfo[i].user) {
			if (!strncmp(pdata->masterinfo[i].port_name,
				port_name, strlen(port_name)))
				return pdata->masterinfo[i].master_name;
		}
	}
	return NULL;
}

/* need to reconfiguable to the address */
static void itmon_en_addr_detect(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				struct itmon_nodeinfo *node,
				bool en)
{
	void __iomem *reg = group->regs;
	u32 tmp, val, offset, i;
	char *name;

	if (node) {
		if (node->type == M_NODE || node->type == T_M_NODE)
			offset = PRTCHK_M_CTL(node->prtchk_offset);
		else
			offset = PRTCHK_S_CTL(node->prtchk_offset);

		val = __raw_readl(reg + offset) | INTEND_ACCESS_INT_EN;
		__raw_writel(val, reg + offset);

		val = ((unsigned int)INTEND_ADDR_START & U32_MAX);
		__raw_writel(val, reg + PRTCHK_START_ADDR_LOW);

		val = (unsigned int)(((unsigned long)INTEND_ADDR_START >> 32) & U16_MAX);
		__raw_writel(val, reg + PRTCHK_START_END_ADDR_UPPER);

		val = ((unsigned int)INTEND_ADDR_END & 0xFFFFFFFF);
		__raw_writel(val, reg + PRTCHK_END_ADDR_LOW);
		val = ((unsigned int)(((unsigned long)INTEND_ADDR_END >> 32)
					& 0XFFFF0000) << 16);
		tmp = readl(reg + PRTCHK_START_END_ADDR_UPPER);
		__raw_writel(tmp | val, reg + PRTCHK_START_END_ADDR_UPPER);
		name = node->name;
	} else {
		node = group->nodeinfo;
		for (i = 0; i < group->nodesize; i++) {
			if (node[i].type == M_NODE || node[i].type == T_M_NODE)
				offset = PRTCHK_M_CTL(node[i].prtchk_offset);
			else
				offset = PRTCHK_S_CTL(node[i].prtchk_offset);

			val = readl(reg + offset) | INTEND_ACCESS_INT_EN;
			writel(val, reg + offset);

			val = ((unsigned int)INTEND_ADDR_START & U32_MAX);
			writel(val, reg + PRTCHK_START_ADDR_LOW);

			val = (unsigned int)(((unsigned long)INTEND_ADDR_START >> 32) & U16_MAX);
			writel(val, reg + PRTCHK_START_END_ADDR_UPPER);

			val = ((unsigned int)INTEND_ADDR_END & 0xFFFFFFFF);
			writel(val, reg + PRTCHK_END_ADDR_LOW);
			val = ((unsigned int)(((unsigned long)INTEND_ADDR_END >> 32)
						& 0XFFFF0000) << 16);
			tmp = readl(reg + PRTCHK_START_END_ADDR_UPPER);
			writel(tmp | val, reg + PRTCHK_START_END_ADDR_UPPER);

		}
		name = group->name;
	}
	dev_dbg(itmon->dev, "ITMON - %s addr detect %sabled\n",
				name, en == true ? "en" : "dis");
}

static void itmon_en_prt_chk(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				struct itmon_nodeinfo *node,
				bool en)
{
	void __iomem *reg = group->regs;
	u32 offset, val = 0, i;
	char *name;

	if (en)
		val = RD_RESP_INT_EN | WR_RESP_INT_EN |
		     ARLEN_RLAST_INT_EN | AWLEN_WLAST_INT_EN;

	if (node) {
		if (node->prtchk_offset == NOT_SUPPORT)
			return;
		if (node->type == M_NODE || node->type == T_M_NODE)
			offset = PRTCHK_M_CTL(node->prtchk_offset);
		else
			offset = PRTCHK_S_CTL(node->prtchk_offset);

		writel(val, reg + offset);
		name = node->name;
	} else {
		node = group->nodeinfo;
		for (i = 0; i < group->nodesize; i++) {
			if (node[i].prtchk_offset == NOT_SUPPORT)
				continue;
			if (node[i].type == M_NODE || node[i].type == T_M_NODE)
				offset = PRTCHK_M_CTL(node[i].prtchk_offset);
			else
				offset = PRTCHK_S_CTL(node[i].prtchk_offset);

			writel(val, reg + offset);
		}
		name = group->name;
	}
	dev_dbg(itmon->dev, "ITMON - %s hw_assert %sabled\n",
			name, en == true ? "en" : "dis");
}

static void itmon_en_err_report(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				struct itmon_nodeinfo *node,
				bool en)
{
	struct itmon_platdata *pdata = itmon->pdata;
	void __iomem *reg = group->regs;
	u32 val = 0, i;
	char *name;

	if (!pdata->probed)
		writel(1, reg + ERR_LOG_CLR);

	val = readl(reg + ERR_LOG_EN_NODE);

	if (node) {
		if (en)
			val |= BIT(node->id);
		else
			val &= ~BIT(node->id);
		writel(val, reg + ERR_LOG_EN_NODE);
		name = node->name;
	} else {
		node = group->nodeinfo;
		for (i = 0; i < group->nodesize; i++) {
			if (en)
				val |= (1 << node[i].id);
			else
				val &= ~(1 << node[i].id);
		}
		writel(val, reg + ERR_LOG_EN_NODE);
		name = group->name;
	}
	dev_dbg(itmon->dev, "ITMON - %s error reporting %sabled\n",
			name, en == true ? "en" : "dis");
}

static void itmon_en_timeout(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				struct itmon_nodeinfo *node,
				bool en)
{
	void __iomem *reg = group->regs;
	u32 offset, val = 0, i;
	char *name;

	if (node) {
		if (node->tmout_offset == NOT_SUPPORT)
			return;
		offset = TMOUT_CTL(node->tmout_offset);

		/* Enabled */
		val = (en << 0);
		val |= (node->tmout_frz_en << 1);
		val |= (node->time_val << 4);

		writel(val, reg + offset);
		name = node->name;
	} else {
		node = group->nodeinfo;
		for (i = 0; i < group->nodesize; i++) {
			if (node[i].type != S_NODE || node[i].tmout_offset == NOT_SUPPORT)
				continue;
			offset = TMOUT_CTL(node[i].tmout_offset);

			/* en */
			val = (en << 0);
			val |= (node[i].tmout_frz_en << 1);
			val |= (node[i].time_val << 4);

			writel(val, reg + offset);
		}
		name = group->name;
	}
	dev_dbg(itmon->dev, "ITMON - %s timeout %sabled\n",
			name, en == true ? "en" : "dis");
}

static void itmon_enable_nodepolicy(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node)
{
	struct itmon_nodepolicy *policy = &node->policy;
	struct itmon_nodegroup *group = node->group;

	if (!policy->chk_set)
		return;

	if (group->pd_support && !group->pd_status) {
		dev_err(g_itmon->dev, "%s group - %s node NOT pd on\n",
			group, node);
	}

	if (policy->chk_errrpt)
		itmon_en_err_report(itmon, group, node, policy->en_errrpt);
	if (policy->chk_tmout || policy->chk_tmout_val)
		itmon_en_timeout(itmon, group, node, policy->en_tmout);
	if (policy->chk_prtchk)
		itmon_en_prt_chk(itmon, group, node, policy->en_prtchk);
}

static void itmon_set_nodepolicy(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node,
				   struct itmon_nodepolicy policy,
				   bool now)
{
	node->policy.en = policy.en;

	if (policy.chk_errrpt)
		node->err_en = policy.en_errrpt;
	if (policy.chk_tmout)
		node->tmout_en = policy.en_tmout;
	if (policy.chk_prtchk)
		node->prt_chk_en = policy.en_prtchk;
	if (policy.chk_tmout_val)
		node->time_val = policy.en_tmout_val;
	if (policy.chk_freeze)
		node->tmout_frz_en = policy.en_freeze;

	if (policy.chk_errrpt || policy.chk_tmout ||
		policy.chk_prtchk || policy.chk_tmout_val ||
		policy.chk_errrpt || policy.chk_prtchk_job ||
		policy.chk_tmout || policy.chk_freeze ||
		policy.chk_decerr_job || policy.chk_slverr_job || policy.chk_tmout_job)
		node->policy.chk_set = 1;

	if (now)
		itmon_enable_nodepolicy(itmon, node);
}

int itmon_en_by_name(const char *name, bool en)
{
	struct itmon_nodeinfo *node;
	struct itmon_nodegroup *group;

	if (!g_itmon)
		return -ENODEV;

	node = itmon_get_nodeinfo(g_itmon, NULL, name);
	if (!node) {
		dev_err(g_itmon->dev, "%s node is not found\n", name);
		return -ENODEV;
	}

	group = node->group;
	if (!group) {
		dev_err(g_itmon->dev, "%s node's group is  not found\n", name);
		return -ENODEV;
	}

	if (group->pd_support && !group->pd_status) {
		dev_err(g_itmon->dev, "%s group - %s node NOT pd on\n",
			group, node);
		return -EIO;
	}

	itmon_en_err_report(g_itmon, group, node, en);
	itmon_en_prt_chk(g_itmon, group, node, en);
	if (node->type == S_NODE)
		itmon_en_timeout(g_itmon, group, node, en);

	return 0;
}
EXPORT_SYMBOL(itmon_en_by_name);

static void itmon_en_global(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				int int_en,
				int err_log_en,
				int tmout_en,
				int prtchk_en,
				int fixed_det_en)
{
	u32 val = (int_en) | (err_log_en << 1) | (tmout_en << 2) |
			(prtchk_en << 3) | (fixed_det_en << 5);

	writel(val, group->regs + DBG_CTL);
}

static void itmon_init_by_group(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				bool en)
{
	struct itmon_nodeinfo *node;
	int i;

	node = group->nodeinfo;
	for (i = 0; i < group->nodesize; i++) {
		if (en) {
			if (node[i].err_en)
				itmon_en_err_report(itmon, group, &node[i], en);
			if (node[i].prt_chk_en)
				itmon_en_prt_chk(itmon, group, &node[i], en);
		} else {
			/* as default en */
			itmon_en_err_report(itmon, group, &node[i], en);
			itmon_en_prt_chk(itmon, group, &node[i], en);
		}
		if (node[i].type == S_NODE && node[i].tmout_en)
			itmon_en_timeout(itmon, group, &node[i], en);
		if (node[i].addr_detect_en && node[i].prtchk_offset != NOT_SUPPORT)
			itmon_en_addr_detect(itmon, group, &node[i], en);
	}
}

static void itmon_init(struct itmon_dev *itmon, bool en)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group;
	int i;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (group->pd_support && !group->pd_status)
			continue;
		itmon_en_global(itmon, group, en, en, en, en, en);
		if (pdata->def_en)
			itmon_init_by_group(itmon, group, en);
	}

	pdata->en = en;
	dev_info(itmon->dev, "itmon %sabled\n", pdata->en ? "en" : "dis");
}

void itmon_pd_sync(const char *pd_name, bool en)
{
	struct itmon_dev *itmon = g_itmon;
	struct itmon_platdata *pdata;
	struct itmon_nodegroup *group;
	int i;

	if (!itmon || !itmon->pdata || !itmon->pdata->probed)
		return;

	pdata = itmon->pdata;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (group->pd_support &&
			!strncmp(pd_name, group->pd_name, strlen(pd_name))) {
			dev_dbg(itmon->dev,
				"%s: pd_name:%s enabled:%x, pd_status:%x\n",
				__func__, pd_name, en, group->pd_status);
			if (group->pd_status != en) {
				if (en) {
					itmon_en_global(itmon, group, en, en, en, en, en);
					if (!pdata->def_en)
						itmon_init_by_group(itmon, group, en);
				}
				group->pd_status = en;
			}
		}
	}
}
EXPORT_SYMBOL(itmon_pd_sync);

void itmon_en(bool en)
{
	if (g_itmon)
		itmon_init(g_itmon, en);
}
EXPORT_SYMBOL(itmon_en);

int itmon_get_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, policy = -1;

	for (i = 0; i < TYPE_MAX; i++) {
		if (pdata->policy[i].error && pdata->policy[i].policy > policy)
			policy = pdata->policy[i].policy;
	}

	return policy;
}

static void itmon_dump_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	pr_err("\t\tError Policy Information\n\n\t\t> NAME           |Error    |Policy-def |Policy-now\n");
	for (i = 0; i < TYPE_MAX; i++) {
		pr_info("\t\t> %15s%10s%10s%10s\n",
			pdata->policy[i].name,
			pdata->policy[i].error == true ? "TRUE" : "FALSE",
			itmon_dpm_action[pdata->policy[i].policy_def],
			itmon_dpm_action[pdata->policy[i].policy]);
	}
	pr_err("\n");
}

static void itmon_do_dpm_policy(struct itmon_dev *itmon, bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, sel, policy = -1;

	for (i = 0; i < TYPE_MAX; i++) {
		if (pdata->policy[i].error && pdata->policy[i].policy > policy) {
			policy = pdata->policy[i].policy;
			sel = i;
		}
		if (clear) {
			pdata->policy[i].policy = pdata->policy[i].policy_def;
			pdata->policy[i].prio = 0;
		}
	}

	if (policy >= GO_DEFAULT_ID) {
		dev_err(itmon->dev, "%s: %s: policy:%s\n", __func__,
					pdata->policy[sel].name, itmon_dpm_action[policy]);
		dbg_snapshot_do_dpm_policy(policy);
	}
}

static void itmon_reflect_policy_by_notifier(struct itmon_dev *itmon,
						struct itmon_traceinfo *info,
						int ret)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *pl_policy;
	int sig, num;

	if ((ret & ITMON_NOTIFY_MASK) != ITMON_NOTIFY_MASK)
		return;

	sig = ret & ~ITMON_NOTIFY_MASK;

	switch (info->err_code) {
	case ERR_DECERR:
		num = DECERR;
		break;
	case ERR_SLVERR:
		num = SLVERR;
		break;
	case ERR_TMOUT:
		num = TMOUT;
		break;
	default:
		dev_warn(itmon->dev, "%s: ETC or Invalid error code(%u) : %s\n",
				__func__, info->err_code, itmon_errcode[info->err_code]);
		return;
	}
	pl_policy = &pdata->policy[num];

	pr_err("\nPolicy Changes: %d -> %d in %s by notifier-call\n\n",
			pl_policy->policy, sig, itmon_errcode[info->err_code]);

	pl_policy->policy = sig;
	pl_policy->prio = CUSTOM_MAX_PRIO;
}

static int itmon_notifier_handler(struct itmon_dev *itmon,
					   struct itmon_traceinfo *info,
					   unsigned int trans_type)
{
	int ret = 0;

	/* After treatment by port */
	if ((!info->src || strlen(info->src) < 1) ||
		(!info->dest || strlen(info->dest) < 1))
		return -1;

	itmon->notifier_info.port = info->src;
	itmon->notifier_info.master = info->master;
	itmon->notifier_info.dest = info->dest;
	itmon->notifier_info.read = info->read;
	itmon->notifier_info.target_addr = info->target_addr;
	itmon->notifier_info.errcode = info->err_code;
	itmon->notifier_info.onoff = info->onoff;

	pr_err("----------------------------------------------------------------------------------\n\t\t+ITMON Notifier Call Information\n");

	/* call notifier_call_chain of itmon */
	ret = atomic_notifier_call_chain(&itmon_notifier_list,
				0, &itmon->notifier_info);

	pr_err("\t\t-ITMON Notifier Call Information\n----------------------------------------------------------------------------------\n");

	return ret;
}

/* TODO : implement protocol checker and timeout freeze */
//static void itmon_reflect_policy_by_node(struct itmon_dev *itmon,
//					      struct itmon_nodeinfo *node,
//					      int job)
//{
//	struct itmon_platdata *pdata = itmon->pdata;
//	struct itmon_policy *pl_policy = &pdata->policy[job];
//	struct itmon_nodepolicy *nd_policy = &node->policy;
//	struct itmon_nodegroup *group = node->group;
//	u64 chk_job = 0, en_job = 0, en_mask = 0;
//
//	pl_policy = &pdata->policy[job];
//	pl_policy->error = true;
//	nd_policy = &node->policy;
//
//	switch (job) {
//	case TMOUT:
//		chk_job = nd_policy->chk_tmout_job;
//		en_job = nd_policy->en_tmout_job;
//		en_mask = nd_policy->en_irq_mask;
//		if (nd_policy->en_irq_mask && node->type == S_NODE)
//			itmon_en_timeout(itmon, group, node, false);
//		break;
//	case PRTCHKER:
//		chk_job = nd_policy->chk_prtchk_job;
//		en_job = nd_policy->en_prtchk_job;
//		if (nd_policy->en_irq_mask)
//			itmon_en_prt_chk(itmon, group, node, false);
//		break;
//	case DECERR:
//		chk_job = nd_policy->chk_decerr_job;
//		en_job = nd_policy->en_decerr_job;
//		if (nd_policy->en_irq_mask)
//			itmon_en_err_report(itmon, group, node, false);
//		break;
//	case SLVERR:
//		chk_job = nd_policy->chk_slverr_job;
//		en_job = nd_policy->en_slverr_job;
//		if (nd_policy->en_irq_mask)
//			itmon_en_err_report(itmon, group, node, false);
//		break;
//	}
//
//	if (chk_job && pl_policy->prio <= nd_policy->prio) {
//		pl_policy->policy = en_job;
//		pl_policy->prio = nd_policy->prio;
//	}
//}

static void itmon_reflect_policy(struct itmon_dev *itmon,
				    struct itmon_traceinfo *info)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *pl_policy = NULL;
	struct itmon_nodepolicy nd_policy1 = {0,}, nd_policy2 = {0,}, nd_policy = {0,};
	u64 chk_job1 = 0, chk_job2 = 0, en_job1 = 0, en_job2 = 0, en_job = 0;

	if (info->s_node) {
		nd_policy1.en = info->s_node->policy.en;
		if (nd_policy1.en_irq_mask) {
			if (info->err_code == ERR_TMOUT)
				itmon_en_timeout(itmon, info->s_node->group, info->s_node, false);
			if (info->err_code == ERR_DECERR ||
				info->err_code == ERR_SLVERR)
				itmon_en_err_report(itmon, info->s_node->group, info->s_node, false);
		}
	}
	if (info->m_node) {
		nd_policy2.en = info->m_node->policy.en;
		if (nd_policy2.en_irq_mask) {
			if (info->err_code == ERR_DECERR ||
				info->err_code == ERR_SLVERR)
				itmon_en_err_report(itmon, info->m_node->group, info->m_node, false);
		}
	}

	switch (info->err_code) {
	case ERR_DECERR:
		pl_policy = &pdata->policy[DECERR];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_decerr_job;
			en_job1 = nd_policy1.en_decerr_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_decerr_job;
			en_job2 = nd_policy2.en_decerr_job;
		}
		break;
	case ERR_SLVERR:
		pl_policy = &pdata->policy[SLVERR];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_slverr_job;
			en_job1 = nd_policy1.en_slverr_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_slverr_job;
			en_job2 = nd_policy2.en_slverr_job;
		}
		break;
	case ERR_TMOUT:
		pl_policy = &pdata->policy[TMOUT];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_tmout_job;
			en_job1 = nd_policy1.en_tmout_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_tmout_job;
			en_job2 = nd_policy2.en_tmout_job;
		}
		break;
	default:
		dev_warn(itmon->dev, "%s: Invalid error code(%u)\n",
					__func__, info->err_code);
		return;
	}

	if (chk_job1 && chk_job2) {
		if (nd_policy1.prio >= nd_policy2.prio) {
			nd_policy.en = nd_policy1.en;
			en_job = en_job1;
		} else {
			nd_policy.en = nd_policy2.en;
			en_job = en_job2;
		}
	} else if (chk_job1) {
		nd_policy = nd_policy1;
		en_job = en_job1;
	} else if (chk_job2) {
		nd_policy = nd_policy2;
		en_job = en_job2;
	} /* nd_policy = NULL */

	if (nd_policy.chk_set && pl_policy->prio <= nd_policy.prio) {
		pl_policy->policy = en_job;
		pl_policy->prio = nd_policy.prio;
	}
}

static void itmon_set_history_magic(void)
{
	if (p_itmon_history)
		__raw_writel(HISTORY_MAGIC, &p_itmon_history->magic);
}

void itmon_print_history(void)
{
	int idx;

	pr_info("Too many ITMONs\n");
	if (p_itmon_history) {
		for (idx = p_itmon_history->idx + 1; idx < p_itmon_history->nr_entry; idx++) {
			if (strlen(p_itmon_history->data[idx]) == 0)
				break;
			pr_info("%s/", p_itmon_history->data[idx]);
		}
		for (idx = 0; idx < p_itmon_history->idx - 1; idx++) {
			if (strlen(p_itmon_history->data[idx]) == 0)
				break;
			pr_info("%s/", p_itmon_history->data[idx]);
		}
		if (strlen(p_itmon_history->data[idx]) != 0)
			pr_cont("%s\n", p_itmon_history->data[idx]);
	}
}
EXPORT_SYMBOL(itmon_print_history);

static void itmon_post_handler(struct itmon_dev *itmon, bool err)
{
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned long ts = pdata->last_time;
	unsigned long rem_nsec = do_div(ts, 1000000000);
	unsigned long cur_time = local_clock();
	unsigned long delta;

	if (!pdata->errcnt_window_start_time) {
		delta = 0;
		pdata->errcnt_window_start_time = cur_time;
	} else {
		delta = cur_time - pdata->errcnt_window_start_time;
	}

	dev_err(itmon->dev, "Before ITMON: [%5lu.%06lu], delta: %lu, last_errcnt: %d\n",
		(unsigned long)ts, rem_nsec / 1000, delta, pdata->last_errcnt);

	if (err)
		itmon_do_dpm_policy(itmon, true);

	/* delta < 1.2s */
	if (delta > 0 && delta < 1200000000UL) {
		pdata->last_errcnt++;

		/* Errors less than 1 second are tolerated no matter how many times they occur */
		if (delta >= 1000000000UL && pdata->last_errcnt > ERR_THRESHOLD) {
			itmon_print_history();
			dbg_snapshot_do_dpm_policy(GO_S2D_ID);
		}
	} else {
		pdata->last_errcnt = 0;
		pdata->errcnt_window_start_time = cur_time;
	}

	pdata->last_time = cur_time;
}

static int itmon_parse_cpuinfo_by_name(struct itmon_dev *itmon,
				       const char *name,
				       unsigned int userbit,
				       char *cpu_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int core_num = 0, el2 = 0, strong = 0, i;

	if (cpu_name == NULL)
		return 0;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_cpu_node_string); i++) {
		if (!strncmp(name, itmon_cpu_node_string[i],
			strlen(itmon_cpu_node_string[i]))) {
			if (userbit & BIT(0))
				el2 = 1;
			if (DSS_NR_CPUS > 8) {
				if (!(userbit & BIT(1)))
					strong = 1;
				core_num = ((userbit & (0xF << 2)) >> 2);
			} else {
				core_num = ((userbit & (0x7 << 1)) >> 1);
				strong = 0;
			}
			if (pdata->cpu_parsing) {
				snprintf(cpu_name, sizeof(cpu_name) - 1,
					"CPU%d%s%s", core_num, el2 == 1 ? "EL2" : "",
					strong == 1 ? "Strng" : "");
			} else {
				snprintf(cpu_name, sizeof(cpu_name) - 1, "CPU");
			}
			return 1;
		}
	};

	return 0;
}

static void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodegroup *group)
{
	unsigned int id, payload0, payload1, payload2, payload3, payload4, payload5;
	unsigned int axid, valid, timeout;
	unsigned long addr, user;
	int i, num = SZ_128;
	int rw_offset = 0;
	u32 axburst, axprot, axlen, axsize, domain;
	unsigned int trans_type;
	unsigned long tmout_frz_stat;
	unsigned int tmout_offset;
	struct itmon_nodeinfo * node;
	unsigned int bit = 0;

	if (group)
		dev_info(itmon->dev, "group %s frz_support %s\n",
				group->name, group->frz_support? "YES": "NO");
	else
		return;

	if (!group->frz_support)
		return;

	tmout_frz_stat = readl(group->regs + OFFSET_TMOUT_REG);

	if (tmout_frz_stat == 0x0)
		return;

	dev_info(itmon->dev, "TIMEOUT_FREEZE_STATUS 0x%08x", tmout_frz_stat);

	for_each_set_bit(bit, &tmout_frz_stat, group->nodesize) {
		tmout_offset = (1 << bit) >> 1;
		node = itmon_get_nodeinfo_group_by_tmout_offset(itmon, group, tmout_offset);

		pr_err("\nITMON Report Timeout Error Occurred : Master --> %s\n\n",
				node->name);
		pr_err("> NUM| TYPE|    MASTER BLOCK|          MASTER|VALID|TMOUT|    ID|DOMAIN|SIZE|PROT|         ADDRESS|PAYLOAD0|PAYLOAD1|PAYLOAD2|PAYLOAD3|PAYLOAD4|PAYLOAD5");

		for (trans_type = TRANS_TYPE_WRITE; trans_type < TRANS_TYPE_NUM; trans_type++) {
			num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
			rw_offset = (trans_type == TRANS_TYPE_READ ? 0 : TMOUT_RW_OFFSET);
			for (i = 0; i < num; i++) {
				struct itmon_rpathinfo *port = NULL;
				char *master_name = NULL;
				char cpu_name[16];

				/*
				TODO : rename PAYLOADn registers
				PAYLOAD0 -> PAYLOAD_FROM_BUFFER
				PAYLOAD1 -> PAYLOAD_FROM_SRAM_1
				PAYLOAD2 -> PAYLOAD_FROM_SRAM_2
				PAYLOAD3 -> PAYLOAD_FROM_SRAM_3
				PAYLOAD4 -> USER_FROM_BUFFER_0
				PAYLOAD5 -> USER_FROM_BUFFER_1
				*/

				writel(i | (bit << 16), group->regs + OFFSET_TMOUT_REG + TMOUT_POINT_ADDR + rw_offset);
				id = readl(group->regs + OFFSET_TMOUT_REG + TMOUT_ID + rw_offset);
				payload0 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_0 + rw_offset);
				payload1 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_1 + rw_offset);
				payload2 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_2 + rw_offset);	/* mid/small: payload 1 */
				payload3 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_3 + rw_offset);	/* mid/small: payload 2 */
				payload4 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_4 + rw_offset);	/* mid/small: payload 3 */
				payload5 = readl(group->regs + OFFSET_TMOUT_REG +
						TMOUT_PAYLOAD_5 + rw_offset);

				timeout = (payload0 & (unsigned int)(GENMASK(7, 4))) >> 4;
				user = ((unsigned long)payload4 << 32ULL) | payload5;
				addr = (((unsigned long)payload1 & GENMASK(15, 0)) << 32ULL) | payload2;
				axid = id & GENMASK(5, 0); /* ID[5:0] 6bit : R-PATH */
				valid = payload0 & BIT(0); /* PAYLOAD[0] : Valid or Not valid */
				axburst = TMOUT_BIT_AXBURST(payload3);
				axprot = TMOUT_BIT_AXPROT(payload3);
				axlen = (payload3 & (unsigned int)GENMASK(7, 4)) >> 4;
				axsize = (payload3 & (unsigned int)GENMASK(18, 16)) >> 16;
				domain = (payload3 & BIT(19));
				if (group->path_type == CONFIG) {
					port = itmon_get_rpathinfo(itmon, axid, "CONFIGURATION");
					if (port)
						itmon_parse_cpuinfo_by_name(itmon, port->port_name, user, cpu_name);
					master_name = cpu_name;
				} else {
					port = itmon_get_rpathinfo(itmon, axid, node->name);
					if (port)
						master_name = itmon_get_masterinfo(itmon, port->port_name, user);
				}

				if(valid)
					pr_err("> %03d|%05s|%16s|%16s|%5u|%5x|%06x|%6s|%4u|%4s|%016zx|%08x|%08x|%08x|%08x|%08x|%08x\n",
							i, (trans_type == TRANS_TYPE_READ)? "READ" : "WRITE",
							port ? port->port_name : NO_NAME,
							master_name ? master_name : NO_NAME,
							valid, timeout, id, domain ? "SHARE" : "NSHARE",
							int_pow(2, axsize * (axlen + 1)),
							axprot & BIT(1) ? "NSEC" : "SEC",
							addr, payload0, payload1, payload2, payload3, payload4, payload5);
			}
		}
	}
}

static void itmon_report_rawdata(struct itmon_dev *itmon,
				 struct itmon_tracedata *data)
{
	struct itmon_nodeinfo *m_node = data->m_node;
	struct itmon_nodeinfo *det_node = data->det_node;
	struct itmon_nodegroup *group = data->group;

	/* Output Raw register information */
	pr_err("\tRaw Register Information ---------------------------------------------------\n\n");

	if (m_node && det_node) {
		pr_err(	"\t> M_NODE     : %s(%s, id: %03u)\n"
			"\t> DETECT_NODE: %s(%s, id: %03u)\n",
			m_node->name, itmon_node_string[m_node->type], m_node->err_id,
			det_node->name, itmon_node_string[det_node->type], det_node->err_id);
	}

	pr_err("\t> BASE       : %s(0x%08llx)\n"
		"\t> INFO_0     : 0x%08X\n"
		"\t> INFO_1     : 0x%08X\n"
		"\t> INFO_2     : 0x%08X\n"
		"\t> INFO_3     : 0x%08X\n"
		"\t> INFO_4     : 0x%08X\n"
		"\t> INFO_5     : 0x%08X\n"
		"\t> INFO_6     : 0x%08X\n",
		group->name, group->phy_regs,
		data->info_0,
		data->info_1,
		data->info_2,
		data->info_3,
		data->info_4,
		data->info_5,
		data->info_6);
}

static void itmon_report_traceinfo(struct itmon_dev *itmon,
				   struct itmon_traceinfo *info,
				   unsigned int trans_type)
{
	u64 ts_nsec = local_clock();
	unsigned long rmem_nsec = do_div(ts_nsec, 1000000000);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif
	/* SEC DEBUG FEATURE */
	int acon = (itmon_get_dpm_policy(itmon) > 0) ? 1 : 0;
	/* SEC DEBUG FEATURE */

	if (!info->dirty)
		return;

	pr_auto_on(acon, ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"\tTransaction Information\n\n"
		"\t> Master         : %s %s\n"
		"\t> Target         : %s\n"
		"\t> Target Address : 0x%llX %s\n"
		"\t> Type           : %s\n"
		"\t> Error code     : %s\n",
		info->src, info->master ? info->master : "",
		info->dest ? info->dest : NO_NAME,
		info->target_addr,
		info->baaw_prot == true ? "(BAAW Remapped address)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[info->err_code]);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	if (acon) {
		snprintf(temp_buf, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
			info->src, info->master ? info->master : "",
			info->dest ? info->dest : NO_NAME,
			info->target_addr,
			info->baaw_prot == true ? "(by CP maybe)" : "",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
			itmon_errcode[info->err_code]);
		secdbg_exin_set_busmon(temp_buf);
	}
#endif
	pr_auto_on(acon, ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"\t> Size           : %u bytes x %u burst => %u bytes\n"
		"\t> Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
		"\t> Level          : %s\n"
		"\t> Protection     : %s\n"
		"\t> Path Type      : %s\n\n",
		int_pow(2, info->axsize), info->axlen + 1,
		int_pow(2, info->axsize) * (info->axlen + 1),
		info->axburst,
		info->axprot & BIT(0) ? "Privileged" : "Unprivileged",
		info->axprot & BIT(1) ? "Non-secure" : "Secure",
		itmon_pathtype[info->path_type]);

	if (p_itmon_history) {
		snprintf(p_itmon_history->data[p_itmon_history->idx], sizeof(p_itmon_history->data[0]),
				"%lu.%03lu,%s%s,%s,0x%llx,%s",
				(unsigned long)ts_nsec, rmem_nsec / 1000000,
				info->src, info->master ? info->master : "",
				info->dest ? info->dest : NO_NAME,
				info->target_addr,
				itmon_errcode_short[info->err_code]);

		p_itmon_history->idx = (p_itmon_history->idx + 1) % p_itmon_history->nr_entry;
	}
}

static void itmon_report_pathinfo(struct itmon_dev *itmon,
				  struct itmon_tracedata *data,
				  struct itmon_traceinfo *info,
				  unsigned int trans_type)

{
	struct itmon_nodeinfo *m_node = data->m_node;
	struct itmon_nodeinfo *det_node = data->det_node;
	/* SEC DEBUG FEATURE */
	int acon = (itmon_get_dpm_policy(itmon) > 0) ? 1 : 0;
	/* SEC DEBUG FEATURE */

	if (!m_node || !det_node)
		return;

	if (!info->path_dirty) {
		pr_auto_on(acon, ASL3,
			"\n-----------------------------------------"
			"-----------------------------------------\n"
			"\t\tITMON Report (%s)\n"
			"-----------------------------------------"
			"-----------------------------------------\n"
			"\tPATH Information\n\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE");
		info->path_dirty = true;
	}
	pr_auto_on(acon, ASL3, "\t> M_NODE     : %14s, %8s(id: %u)\n",
		m_node->name, itmon_node_string[m_node->type], m_node->err_id);
	pr_auto_on(acon, ASL3, "\t> DETECT_NODE: %14s, %8s(id: %u)\n",
		det_node->name, itmon_node_string[det_node->type], det_node->err_id);
}

static int itmon_parse_cpuinfo(struct itmon_dev *itmon,
			       struct itmon_tracedata *data,
			       struct itmon_traceinfo *info,
			       unsigned int userbit)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *m_node = data->m_node;
	int core_num = 0, el2 = 0, strong = 0, i;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_cpu_node_string); i++) {
		if (!strncmp(m_node->name, itmon_cpu_node_string[i],
			strlen(itmon_cpu_node_string[i]))) {
			if (userbit & BIT(0))
				el2 = 1;
			if (DSS_NR_CPUS > 8) {
				if (!(userbit & BIT(1)))
					strong = 1;
				core_num = ((userbit & (0xF << 2)) >> 2);
			} else {
				core_num = ((userbit & (0x7 << 1)) >> 1);
				strong = 0;
			}
			if (pdata->cpu_parsing) {
				snprintf(info->buf, sizeof(info->buf) - 1,
					"CPU%d_%s_%s", core_num, el2 == 1 ? "EL2" : "",
					strong == 1 ? "Strong" : "");
			} else {
				snprintf(info->buf, sizeof(info->buf) - 1, "CPU");
			}
			info->master = info->buf;
			return 1;
		}
	};

	return 0;
}

static void itmon_parse_traceinfo(struct itmon_dev *itmon,
				   struct itmon_tracedata *data,
				   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = data->group;
	struct itmon_traceinfo *new_info = NULL;
	u64 axuser;
	int i;

	if (!data->m_node || !data->det_node)
		return;

	new_info = kzalloc(sizeof(struct itmon_traceinfo), GFP_ATOMIC);
	if (!new_info) {
		dev_err(itmon->dev, "failed to kmalloc for %s -> %s\n",
			data->m_node->name, data->det_node->name);
		return;
	}

	axuser = data->info_5;
	new_info->m_id = data->m_id;
	new_info->s_id = data->det_id;
	new_info->m_node = data->m_node;
	new_info->s_node = data->det_node;
	new_info->src = data->m_node->name;
	new_info->dest = data->det_node->name;
	new_info->master = itmon_get_masterinfo(itmon, new_info->m_node->name, axuser);
	if (new_info->master == NULL)
		new_info->master = "";

	if (group->path_type == CONFIG)
		itmon_parse_cpuinfo(itmon, data, new_info, axuser);

	/* Common Information */
	new_info->path_type = group->path_type;
	new_info->target_addr = (((u64)data->info_3 & GENMASK(15, 0)) << 32ULL);
	new_info->target_addr |= data->info_2;
	new_info->err_code = data->err_code;
	new_info->dirty = true;
	new_info->axsize = AXSIZE(data->info_3);
	new_info->axlen = AXLEN(data->info_3);
	new_info->axburst = AXBURST(data->info_4);
	new_info->axprot = AXPROT(data->info_4);
	new_info->baaw_prot = false;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_invalid_addr); i++) {
		if (new_info->target_addr == itmon_invalid_addr[i]) {
			new_info->baaw_prot = true;
			break;
		}
	}
	data->ref_info = new_info;
	list_add_tail(&new_info->list, &pdata->infolist[trans_type]);
}

static void itmon_analyze_errlog(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *info, *next_info;
	struct itmon_tracedata *data, *next_data;
	unsigned int trans_type;
	int ret;

	/* Parse */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry(data, &pdata->datalist[trans_type], list)
			itmon_parse_traceinfo(itmon, data, trans_type);
	}

	/* Report and Clean-up traceinfo */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry_safe(info, next_info, &pdata->infolist[trans_type], list) {
			info->path_dirty = false;
			itmon_reflect_policy(itmon, info);
			ret = itmon_notifier_handler(itmon, info, trans_type);
			itmon_reflect_policy_by_notifier(itmon, info, ret);
			list_for_each_entry_safe(data, next_data, &pdata->datalist[trans_type], list) {
				if (data->ref_info == info)
					itmon_report_pathinfo(itmon, data, info, trans_type);
			}
			itmon_report_traceinfo(itmon, info, trans_type);
			list_del(&info->list);
			kfree(info);
		}
	}
	itmon_dump_dpm_policy(itmon);

	/* Report Raw all tracedata and Clean-up tracedata and node */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry_safe(data, next_data,
			&pdata->datalist[trans_type], list) {
				itmon_report_rawdata(itmon, data);
				list_del(&data->list);
				kfree(data);
		}
	}
}

static void *itmon_collect_errlog(struct itmon_dev *itmon,
			    struct itmon_nodegroup *group)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *new_data = NULL;
	void __iomem *reg = group->regs;

	new_data = kzalloc(sizeof(struct itmon_tracedata), GFP_ATOMIC);
	if (!new_data) {
		pr_auto(ASL3, "failed to kmalloc for %s group\n", group->name);
		return NULL;
	}

	new_data->info_0 = readl(reg + ERR_LOG_INFO_0);
	new_data->info_1 = readl(reg + ERR_LOG_INFO_1);
	new_data->info_2 = readl(reg + ERR_LOG_INFO_2);
	new_data->info_3 = readl(reg + ERR_LOG_INFO_3);
	new_data->info_4 = readl(reg + ERR_LOG_INFO_4);
	new_data->info_5 = readl(reg + ERR_LOG_INFO_5);
	new_data->info_6 = readl(reg + ERR_LOG_INFO_6);
	new_data->en_node = readl(reg + ERR_LOG_EN_NODE);

	new_data->m_id = MNODE_ID(new_data->info_0);
	new_data->det_id = DET_ID(new_data->info_0);

	new_data->m_node = itmon_get_nodeinfo_by_eid(itmon, NULL,
							group->path_type, new_data->m_id);
	new_data->det_node = itmon_get_nodeinfo_by_eid(itmon, NULL,
							group->path_type, new_data->det_id);

	new_data->read = !RW(new_data->info_0);
	new_data->err_code = ERR_CODE(new_data->info_0);
	new_data->ref_info = NULL;
	new_data->group = group;

	list_add_tail(&new_data->list, &pdata->datalist[new_data->read]);

	return (void *)new_data;
}

/* TODO : implement protocol checker and timeout freeze */
static int itmon_pop_errlog(struct itmon_dev *itmon,
			       struct itmon_nodegroup *group,
			       bool clear)
{
	struct itmon_tracedata *data;
	void __iomem *reg = group->regs;
	u32 num_log, max_log = 64;
	int ret = 0;

	num_log = readl(reg + ERR_LOG_STAT);

	while(num_log) {
		data = itmon_collect_errlog(itmon, group);
		writel(1, reg + ERR_LOG_POP);
		num_log = readl(group->regs + ERR_LOG_STAT);
		ret++;

		switch (data->err_code) {
		case ERR_TMOUT:
		/* Timeout */
		break;
		case ERR_PRTCHK_ARID_RID ... ERR_PRTCHK_AWLEN_WLAST_NL:
		/* Protocol Checker */
		break;
		}
		max_log--;

		if (max_log == 0)
			break;
	};

	if (clear)
		writel(1, reg + ERR_LOG_CLR);

	return ret;
}

static int itmon_search_errlog(struct itmon_dev *itmon,
			     struct itmon_nodegroup *group,
			     bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&itmon->ctrl_lock, flags);

	if (group) {
		ret = itmon_pop_errlog(itmon, group, clear);
		itmon_report_timeout(itmon, group);
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			if (group->pd_support && !group->pd_status)
				continue;
			ret += itmon_pop_errlog(itmon, group, clear);
			itmon_report_timeout(itmon, group);
		}
	}
	if (ret)
		itmon_analyze_errlog(itmon);

	spin_unlock_irqrestore(&itmon->ctrl_lock, flags);
	return ret;
}

static irqreturn_t itmon_irq_handler(int irq, void *data)
{
	struct itmon_dev *itmon = (struct itmon_dev *)data;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = NULL;
	bool ret;
	int i;

	/* Search itmon group */
	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (!group->pd_support) {
			dev_err(itmon->dev, "%d irq, %s group, pd %s, %x errors",
				irq, group->name, "on",
				readl(group->regs + ERR_LOG_STAT));
		} else {
			dev_err(itmon->dev, "%d irq, %s group, pd %s, %x errors",
				irq, group->name,
				group->pd_status ? "on" : "off",
				group->pd_status ? readl(group->regs + ERR_LOG_STAT) : 0);
		}
	}

	ret = itmon_search_errlog(itmon, NULL, true);
	if (!ret)
		dev_err(itmon->dev, "No errors found %s\n", __func__);
	else
		dev_err(itmon->dev, "Error detected %s, %d\n", __func__, ret);

	itmon_post_handler(itmon, ret);

	return IRQ_HANDLED;
}

void itmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itmon_notifier_list, block);
}
EXPORT_SYMBOL(itmon_notifier_chain_register);

static int itmon_logging_panic_handler(struct notifier_block *nb,
				     unsigned long l, void *buf)
{
	struct itmon_panic_block *itmon_panic = (struct itmon_panic_block *)nb;
	struct itmon_dev *itmon = itmon_panic->pdev;
	int ret;

	if (!IS_ERR_OR_NULL(itmon)) {
		/* Check error has been logged */
		ret = itmon_search_errlog(itmon, NULL, true);
		if (!ret) {
			dev_info(itmon->dev, "No errors found %s\n", __func__);
		} else {
			dev_err(itmon->dev, "Error detected %s, %d\n", __func__, ret);
			itmon_do_dpm_policy(itmon, true);
		}
	}
	return 0;
}

#define MAX_CUSTOMIZE		64

static void itmon_parse_dt_customize(struct itmon_dev *itmon,
				     struct device_node *np)
{
	struct device_node *node_np;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *plat_policy;
	struct itmon_nodepolicy nd_policy;
	struct itmon_nodeinfo *node;
	char snode_name[16];
	const char *node_name;
	unsigned int val;
	int i;

	for (i = 0; i < MAX_CUSTOMIZE; i++) {
		nd_policy.en = 0;
		snprintf(snode_name, sizeof(snode_name), "node%d", i);
		node_np = of_get_child_by_name(np, (const char *)snode_name);

		if (!node_np) {
			dev_info(itmon->dev, "%s device node is not found\n",
					snode_name);
			break;
		}

		if(of_property_read_string(node_np, "node", &node_name))
			continue;

		node = itmon_get_nodeinfo(itmon, NULL, node_name);
		if (!node) {
			dev_err(itmon->dev, "%s node is not found\n", node_name);
			continue;
		} else {
			dev_info(itmon->dev, "%s node found\n", node->name);
		}

		if (!of_property_read_u32(node_np, "irq_mask", &val)) {
			nd_policy.chk_irq_mask = 1;
			nd_policy.en_irq_mask = val;
			dev_info(itmon->dev, "%s node: irq mask -> [%d]\n",
					node->name, val);
		}

		if (!of_property_read_u32(node_np, "prio", &val)) {
			nd_policy.prio = val;
			dev_info(itmon->dev, "%s node: prio -> [%d]\n",
					node->name, val);
		}

		if (!of_property_read_u32(node_np, "errrpt", &val)) {
			nd_policy.chk_errrpt = 1;
			nd_policy.en_errrpt = val;
			dev_info(itmon->dev, "%s node: errrtp [%d] -> [%d]\n",
					node->name, node->err_en, val);
		}

		if (!of_property_read_u32(node_np, "prtchker", &val)) {
			nd_policy.chk_prtchk = 1;
			nd_policy.en_prtchk = val;
			dev_info(itmon->dev, "%s node: prtchker [%d] -> [%d]\n",
					node->name, node->prt_chk_en, val);
		}

		if (!of_property_read_u32(node_np, "tmout", &val)) {
			nd_policy.chk_tmout = 1;
			nd_policy.en_tmout = val;
			dev_info(itmon->dev, "%s node: tmout [%d] -> [%d]\n",
					node->name, node->tmout_en, val);
		}

		if (!of_property_read_u32(node_np, "tmout_val", &val)) {
			nd_policy.chk_tmout_val = 1;
			nd_policy.en_tmout_val = val;
			dev_info(itmon->dev, "%s node: tmout_val [%x] -> [%x]\n",
					node->name, node->time_val, val);
		}

		if (!of_property_read_u32(node_np, "freeze", &val)) {
			nd_policy.chk_freeze = 1;
			nd_policy.en_freeze = val;
			dev_info(itmon->dev, "%s node: freeze [%x] -> [%x]\n",
					node->name, node->tmout_frz_en, val);
		}

		if (!of_property_read_u32(node_np, "decerr_job", &val)) {
			plat_policy = &pdata->policy[DECERR];
			nd_policy.chk_decerr_job = 1;
			nd_policy.en_decerr_job = val;
			dev_info(itmon->dev, "%s node: decerr_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_decerr_job,
					itmon_dpm_action[nd_policy.en_decerr_job]);
		}

		if (!of_property_read_u32(node_np, "slverr_job", &val)) {
			plat_policy = &pdata->policy[SLVERR];
			nd_policy.chk_slverr_job = 1;
			nd_policy.en_slverr_job = val;
			dev_info(itmon->dev, "%s node: slverr_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_slverr_job,
					itmon_dpm_action[nd_policy.en_slverr_job]);
		}

		if (!of_property_read_u32(node_np, "tmout_job", &val)) {
			plat_policy = &pdata->policy[TMOUT];
			nd_policy.chk_tmout_job = 1;
			nd_policy.en_tmout_job = val;
			dev_info(itmon->dev, "%s node: tmout_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_tmout_job,
					itmon_dpm_action[nd_policy.en_tmout_job]);
		}

		if (!of_property_read_u32(node_np, "prtchker_job", &val)) {
			plat_policy = &pdata->policy[PRTCHKER];
			nd_policy.chk_prtchk_job = 1;
			nd_policy.en_prtchk_job = val;
			dev_info(itmon->dev, "%s node: prtchker_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_prtchk_job,
					itmon_dpm_action[nd_policy.en_prtchk_job]);
		}
		itmon_set_nodepolicy(itmon, node, nd_policy, false);
	}
	itmon_dump_dpm_policy(itmon);
}

static int itmon_load_keepdata(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct device *dev = itmon->dev;
	struct itmon_keepdata *kdata;
	pgprot_t prot = __pgprot(PROT_NORMAL);
	int page_size, i;
	struct page *page, **pages;
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	void *vaddr;

	rmem_np = of_parse_phandle(dev->of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(dev, "failed to acquire memory region\n");
		return -ENOMEM;
	}

	page_size = rmem->size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	page = phys_to_page(rmem->base);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	vaddr = vmap(pages, page_size, flags, prot);
	kfree(pages);
	if (!vaddr) {
		dev_err(dev, "paddr:%x page_size:%x failed to vmap\n",
				rmem->base, page_size);
		return -ENOMEM;
	}

	kdata = (struct itmon_keepdata *)(vaddr);

	if (kdata->magic != 0xBEEFBEEF) {
		dev_err(dev, "Invalid Keep data of ITMON\n");
		return -EFAULT;
	}

	dev_info(dev, "reserved - base: 0x%llx, magic: 0x%llx, rpathinfo: 0x%llx,"
		      "masterinfo: 0x%llx, nodegroup: 0x%llx\n",
			(u64)kdata->base,
			kdata->magic,
			kdata->mem_rpathinfo,
			kdata->mem_masterinfo,
			kdata->mem_nodegroup);

	pdata->rpathinfo = (struct itmon_rpathinfo *)
		((u64)vaddr + (u64)(kdata->mem_rpathinfo - kdata->base));
	pdata->num_rpathinfo = kdata->num_rpathinfo;

	pdata->masterinfo = (struct itmon_masterinfo *)
		((u64)vaddr + (u64)(kdata->mem_masterinfo - kdata->base));
	pdata->num_masterinfo = kdata->num_masterinfo;

	pdata->nodegroup = (struct itmon_nodegroup *)
		((u64)vaddr + (u64)(kdata->mem_nodegroup - kdata->base));
	pdata->num_nodegroup = kdata->num_nodegroup;

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		struct itmon_nodeinfo *temp;
		int j;
		u64 offset = (((u64)(pdata->nodegroup[i].nodeinfo_phy))
						- (u64)(kdata->base));
		pdata->nodegroup[i].nodeinfo =
			(struct itmon_nodeinfo *)((u64)vaddr + (u64)offset);

		/* TODO : Erase me */
		temp = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			pr_err("name %s type %x id %x err_id %d\n", temp[j].name, temp[j].type, temp[j].id, temp[j].err_id);
		}
	}

	return 0;
}

static int itmon_history_rmem_setup(struct itmon_dev *itmon)
{
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct device *dev = itmon->dev;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size, i;
	struct page *page, **pages;
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	void *vaddr;

	rmem_np = of_parse_phandle(dev->of_node, "memory-region", 1);
	if (!rmem_np) {
		dev_err(dev, "failed to acquire itmon history phandle\n");
		return -ENOMEM;
	}

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(dev, "failed to acquire memory region\n");
		return -ENOMEM;
	}

	page_size = (rmem->size - 1) / PAGE_SIZE + 1;
	pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	page = phys_to_page(rmem->base);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	vaddr = vmap(pages, page_size, flags, prot);
	kfree(pages);
	if (!vaddr) {
		dev_err(dev, "paddr:%llx page_size:%x failed to vmap\n",
				rmem->base, page_size);
		return -ENOMEM;
	}

	memset((void *)vaddr, 0, rmem->size);
	p_itmon_history = (struct itmon_history *)vaddr;
	p_itmon_history->idx = 0;
	p_itmon_history->nr_entry = rmem->size / ITMON_HISTROY_STRING_SIZE;
	itmon_set_history_magic();

	return 0;
}

static void itmon_parse_dt(struct itmon_dev *itmon)
{
	struct device_node *np = itmon->dev->of_node;
	struct device_node *child_np;
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int val;
	int i;

	if (of_property_read_bool(np, "no-def-en")) {
		dev_info(itmon->dev, "No default enable support\n");
		pdata->def_en = false;
	} else {
		pdata->def_en = true;
	}

	if (of_property_read_bool(np, "support-irq-oring")) {
		dev_info(itmon->dev, "only 1 irq support\n");
		pdata->irq_oring = true;
	} else {
		pdata->irq_oring = false;
	}

	if (of_property_read_bool(np, "no-support-cpu-parsing")) {
		dev_info(itmon->dev, "CPU Parser is not support\n");
		pdata->cpu_parsing = false;
	} else {
		pdata->cpu_parsing = true;
	}

	for (i = 0; i < TYPE_MAX; i++) {
		if (!of_property_read_u32(np, pdata->policy[i].name, &val)) {
			pdata->policy[i].policy_def = val;
			pdata->policy[i].policy = val;
		}
	}

	child_np = of_get_child_by_name(np, "customize");
	if (!child_np) {
		dev_info(itmon->dev, "customize is not found\n");
		return;
	}

	itmon_parse_dt_customize(itmon, child_np);

	child_np = of_get_child_by_name(np, "dbgc");
	if (!child_np) {
		dev_info(itmon->dev, "dbgc is not found\n");
		return;
	}
}

static void itmon_set_irq_affinity(struct itmon_dev *itmon,
				   unsigned int irq, unsigned long affinity)
{
	struct cpumask affinity_mask;
	unsigned long bit;
	char *buf = (char *)__get_free_page(GFP_KERNEL);

	cpumask_clear(&affinity_mask);

	if (affinity) {
		for_each_set_bit(bit, &affinity, nr_cpu_ids)
			cpumask_set_cpu(bit, &affinity_mask);
	} else {
		cpumask_copy(&affinity_mask, cpu_online_mask);
	}
	irq_set_affinity_hint(irq, &affinity_mask);

	cpumap_print_to_pagebuf(true, buf, &affinity_mask);
	dev_dbg(itmon->dev, "affinity of irq%d is %s", irq, buf);
	free_page((unsigned long)buf);
}

static int itmon_probe(struct platform_device *pdev)
{
	struct itmon_dev *itmon;
	struct itmon_panic_block *itmon_panic = NULL;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int irq, val = 0;
	char *dev_name;
	int ret, i, j;

	itmon = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_dev), GFP_KERNEL);
	if (!itmon)
		return -ENOMEM;

	itmon->dev = &pdev->dev;

	spin_lock_init(&itmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_platdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	itmon->pdata = pdata;

	if (itmon_load_keepdata(itmon) < 0)
		return -ENOMEM;

	if (itmon_history_rmem_setup(itmon) < 0)
		dev_err(&pdev->dev, "Failed to setup history rmem\n");

	itmon->pdata->policy = err_policy;

	itmon_parse_dt(itmon);

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		dev_name = pdata->nodegroup[i].name;
		node = pdata->nodegroup[i].nodeinfo;

		if (pdata->nodegroup[i].phy_regs) {
			pdata->nodegroup[i].regs =
				devm_ioremap(&pdev->dev,
						 pdata->nodegroup[i].phy_regs, SZ_16K);
			if (pdata->nodegroup[i].regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}

		for (j = 0; j < pdata->nodegroup[i].nodesize; j++)
			node[j].group = &pdata->nodegroup[i];

		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		pdata->nodegroup[i].irq = SET_IRQ(irq);
		if (!irq)
			continue;

		of_property_read_u32_index(pdev->dev.of_node, "interrupt-affinity", i, &val);
		itmon_set_irq_affinity(itmon, irq, val);

		pdata->nodegroup[i].irq |= SET_AFFINITY(val);

		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		disable_irq(irq);
		ret = devm_request_irq(&pdev->dev, irq,
				       itmon_irq_handler,
				       IRQF_NOBALANCING, dev_name, itmon);
		if (ret == 0) {
			dev_info(&pdev->dev,
				 "success to register request irq%u - %s\n",
				 irq, dev_name);
		} else {
			dev_err(&pdev->dev, "failed to request irq - %s\n",
				dev_name);
			return -ENOENT;
		}
	}
	itmon_panic = devm_kzalloc(&pdev->dev, sizeof(struct itmon_panic_block),
				 GFP_KERNEL);

	if (!itmon_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "panic handler data\n");
	} else {
		itmon_panic->nb_panic_block.notifier_call = itmon_logging_panic_handler;
		itmon_panic->pdev = itmon;
		atomic_notifier_chain_register(&panic_notifier_list,
					       &itmon_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itmon);

	itmon_init(itmon, true);

	pdata->last_time = 0;
	pdata->errcnt_window_start_time = 0;
	pdata->last_errcnt = 0;

	g_itmon = itmon;

	for (i = 0; i < TRANS_TYPE_NUM; i++) {
		INIT_LIST_HEAD(&pdata->datalist[i]);
		INIT_LIST_HEAD(&pdata->infolist[i]);
	}

	pdata->probed = true;
	dev_info(&pdev->dev, "success to probe Exynos ITMON driver\n");

	for (i = 0; i < (int)pdata->num_nodegroup; i++)
		if (pdata->nodegroup[i].irq)
			enable_irq(GET_IRQ(pdata->nodegroup[i].irq));

	return 0;
}

static int itmon_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itmon_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int itmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		unsigned int irq = pdata->nodegroup[i].irq;

		if (irq)
			itmon_set_irq_affinity(itmon, GET_IRQ(irq), GET_AFFINITY(irq));
	}
	itmon_init(itmon, true);

	dev_info(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(itmon_pm_ops, itmon_suspend, itmon_resume);
#define ITMON_PM	(itmon_pm_ops)
#else
#define ITM_ONPM	NULL
#endif

static struct platform_driver exynos_itmon_driver = {
	.probe = itmon_probe,
	.remove = itmon_remove,
	.driver = {
		   .name = "exynos-itmon-v2",
		   .of_match_table = itmon_dt_match,
		   .pm = &itmon_pm_ops,
		   },
};
module_platform_driver(exynos_itmon_driver);

MODULE_DESCRIPTION("Samsung Exynos ITMON COMMON DRIVER V2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itmon");
