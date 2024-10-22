// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clk-fhctl.h"
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <vcp_status.h>

#define FHCTL_TARGET FHCTL_VCP
#define FHCTL_IPI_TIMEOUT_MS 10
#define IPI_TIMEOUT_LONG_MS 3000

struct match {
	char *name;
	struct fh_hdlr *hdlr;
	int (*init)(struct pll_dts *array
			, struct match *match);
};
struct id_map {
	char *name;
	unsigned int base;
};
struct hdlr_data_v1 {
	struct pll_dts *array;
	spinlock_t *lock;
	struct fh_pll_domain *domain;
	u8 tr_id;
	void __iomem *reg_tr;
	struct id_map *map;
};
struct freqhopping_ioctl {
	unsigned int pll_id;
	struct freqhopping_ssc {
		unsigned int idx_pattern; /* idx_pattern: Deprecated Field */
		unsigned int dt;
		unsigned int df;
		unsigned int upbnd;
		unsigned int lowbnd;
		unsigned int dds; /* dds: Deprecated Field */
	} ssc_setting;    /* used only when user-define */
	int result;
};
struct fhctl_ipi_data {
	unsigned int cmd;
	union {
		struct freqhopping_ioctl fh_ctl;
		int args[8];
	} u;
};
enum FH_DEVCTL_CMD_ID {
	FH_DCTL_CMD_SSC_ENABLE = 0x1004,
	FH_DCTL_CMD_SSC_DISABLE = 0x1005,
	FH_DCTL_CMD_GENERAL_DFS = 0x1006,
	FH_DCTL_CMD_ARM_DFS = 0x1007,
	FH_DCTL_CMD_SSC_TBL_CONFIG = 0x100A,
	FH_DCTL_CMD_PLL_PAUSE = 0x100E,
	FH_DCTL_CMD_MAX,
	FH_DBG_CMD_TR_BEGIN64_LOW = 0x2001,
	FH_DBG_CMD_TR_BEGIN64_HIGH = 0x2002,
	FH_DBG_CMD_TR_END64_LOW = 0x2003,
	FH_DBG_CMD_TR_END64_HIGH = 0x2004,
	FH_DBG_CMD_TR_ID = 0x2005,
	FH_DBG_CMD_TR_VAL = 0x2006,
	FH_DBG_CMD_TR_BEGIN32 = 0x2007,
	FH_DBG_CMD_TR_END32 = 0x2008,
};
#define FHCTL_D_LEN (sizeof(struct fhctl_ipi_data)/\
	sizeof(int))
static unsigned int ack_data;

static unsigned int id_to_vcp(struct id_map *map, char *domain, unsigned int fh_id)
{
	if (!map)
		return fh_id;

	while (map->name != NULL) {
		if (strcmp(map->name,
					domain) == 0) {
			return (map->base + fh_id);
		}
		map++;
	}
	return fh_id;
}
static void ipi_get_data(unsigned int cmd)
{
	struct fhctl_ipi_data ipi_data;
	int ret;

	FHDBG("cmd<%x>\n", cmd);
	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.cmd = cmd;

	/* 3 sec for debug */
	ret = mtk_ipi_send_compl(vcp_get_ipidev(), IPI_OUT_C_FHCTL_DEBUG, //PIN_OUT_R_SIZE_FHCTL
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, 3000);
	FHDBG("ret<%d>, ack_data<%x>\n",
			ret, ack_data);
}

static void dump_hw(struct fh_pll_regs *regs,
		struct fh_pll_data *data)
{
	FHDBG("hp_en<%x>,clk_con<%x>,slope0<%x>,slope1<%x>\n",
			readl(regs->reg_hp_en), readl(regs->reg_clk_con),
			readl(regs->reg_slope0), readl(regs->reg_slope1));
	FHDBG("cfg<%x>,lmt<%x>,dds<%x>,dvfs<%x>,mon<%x>\n",
			readl(regs->reg_cfg), readl(regs->reg_updnlmt),
			readl(regs->reg_dds), readl(regs->reg_dvfs),
			readl(regs->reg_mon));
	FHDBG("pcw<%x>\n",
			readl(regs->reg_con_pcw));
}

static int vcp_hopping_v1(void *priv_data, char *domain_name, unsigned int fh_id,
		unsigned int new_dds, int postdiv)
{
	int ret;
	struct fhctl_ipi_data ipi_data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	ktime_t ktime;
	u64 time_ns;
	u8 tr_id_local;
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	unsigned int con_pcw;
	bool has_err = false;
	unsigned long flags = 0;

	spin_lock_irqsave(lock, flags);

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.cmd = FH_DCTL_CMD_GENERAL_DFS;
	ipi_data.u.args[0] = id_to_vcp(d->map, domain_name, fh_id);
	ipi_data.u.args[1] = new_dds;
	ipi_data.u.args[2] = postdiv;

	ktime = ktime_get();
	time_ns = ktime_to_ns(ktime);
	d->tr_id ^= (((u8)time_ns) | 0x1);
	ipi_data.u.args[7] = d->tr_id;
	tr_id_local = d->tr_id;
	/* make sure tr_id_local is set before send ipi */
	mb();

	ret = mtk_ipi_send_compl(vcp_get_ipidev(), IPI_OUT_C_FHCTL_DEBUG,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, FHCTL_IPI_TIMEOUT_MS);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	con_pcw = readl(regs->reg_con_pcw);
	con_pcw &= data->dds_mask;

	if (con_pcw != new_dds)
		has_err |= true;
	if (ret != 0)
		has_err |= true;
	if (ack_data != d->tr_id)
		has_err |= true;

	if (has_err) {
		u32 val;

		FHDBG("---------------------------\n");

		FHDBG("domain<%s>, fh_id<%d>\n",
				domain_name, fh_id);
		FHDBG("ret<%d>, ack<%x>, tr<%x>\n",
				ret, ack_data, d->tr_id);
		FHDBG("con_pcw<%x>, new_dds<%x>\n",
				con_pcw, new_dds);

		/* dump HW */
		dump_hw(regs, data);

		/* tr/time via HW */
		FHDBG("time_ns<%llx>\n", time_ns);
		if (d->reg_tr) {
			val = readl(d->reg_tr);
			FHDBG("reg_tr<%x>\n", val);
		}

		/* time via SW */
		ipi_get_data(FH_DBG_CMD_TR_BEGIN64_LOW);
		ipi_get_data(FH_DBG_CMD_TR_BEGIN64_HIGH);
		ipi_get_data(FH_DBG_CMD_TR_END64_LOW);
		ipi_get_data(FH_DBG_CMD_TR_END64_HIGH);
		ipi_get_data(FH_DBG_CMD_TR_ID);
		ipi_get_data(FH_DBG_CMD_TR_VAL);
		ipi_get_data(FH_DBG_CMD_TR_BEGIN32);
		ipi_get_data(FH_DBG_CMD_TR_END32);

		FHDBG("tr_id_local<%x>\n",
				++tr_id_local);
		FHDBG("---------------------------\n");

		/* notify user that err */
		mb();
		notify_err();

		ret = -1;
	} else
		ret = 0;

	spin_unlock_irqrestore(lock, flags);

	return ret;
}

static int vcp_ssc_enable_v1(void *priv_data,
		char *domain_name, unsigned int fh_id, int rate)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int ret;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	struct pll_dts *array = d->array;
	struct fh_pll_data *data = d->domain->data;
	unsigned long flags = 0;

	spin_lock_irqsave(lock, flags);

	FHDBG("rate<%d>\n", rate);

	fh_ctl.pll_id = id_to_vcp(d->map, domain_name, fh_id);
	fh_ctl.result = 0;
	fh_ctl.ssc_setting.dt = data->dt_val;
	fh_ctl.ssc_setting.df = data->df_val;
	fh_ctl.ssc_setting.upbnd = 0;
	fh_ctl.ssc_setting.lowbnd = rate;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	ipi_data.cmd = FH_DCTL_CMD_SSC_ENABLE;
	ret = mtk_ipi_send_compl(vcp_get_ipidev(), IPI_OUT_C_FHCTL_DEBUG,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, IPI_TIMEOUT_LONG_MS);
	FHDBG("ret<%d>\n", ret);

	array->ssc_rate = rate;

	spin_unlock_irqrestore(lock, flags);
	return 0;
}

static int vcp_ssc_disable_v1(void *priv_data,
		char *domain_name, unsigned int fh_id)
{
	struct freqhopping_ioctl fh_ctl;
	struct fhctl_ipi_data ipi_data;
	int ret;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	struct pll_dts *array = d->array;
	unsigned long flags = 0;

	spin_lock_irqsave(lock, flags);

	FHDBG("\n");

	fh_ctl.pll_id = id_to_vcp(d->map, domain_name, fh_id);
	fh_ctl.result = 0;

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	memcpy(&ipi_data.u.fh_ctl, &fh_ctl,
					sizeof(struct freqhopping_ioctl));

	ipi_data.cmd = FH_DCTL_CMD_SSC_DISABLE;
	ret = mtk_ipi_send_compl(vcp_get_ipidev(), IPI_OUT_C_FHCTL_DEBUG,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, IPI_TIMEOUT_LONG_MS);
	FHDBG("ret<%d>\n", ret);

	array->ssc_rate = 0;

	spin_unlock_irqrestore(lock, flags);
	return 0;
}

static int vcp_init_v1(struct pll_dts *array, struct match *match)
{
	static bool ipi_inited;
	static DEFINE_SPINLOCK(lock);
	struct hdlr_data_v1 *priv_data, *match_data;
	struct fh_hdlr *hdlr;

	FHDBG("array<%lx>, %s\n",
			(unsigned long)array,
			array->pll_name);

	if (!ipi_inited) {
		int ret;


		if (IPI_OUT_C_FHCTL_DEBUG <= 0) {
			FHDBG("get vcp channel ID fail!");
			return -1;
		}
		FHDBG("[vcp] ipi_get IPI_OUT_C_FHCTL_DEBUG: %d\n", IPI_OUT_C_FHCTL_DEBUG);

		ret = mtk_ipi_register(vcp_get_ipidev(), IPI_OUT_C_FHCTL_DEBUG, NULL,
				NULL, (void *)&ack_data);
		if (ret) {
			FHDBG("[vcp] ipi_register fail, ret %d\n", ret);
			return -1;
		}
		ipi_inited = true;
		FHDBG("\n");
	}

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)
		return -ENOMEM;
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	if (!hdlr)
		goto __fail;

	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

	match_data = match->hdlr->data;
	if (match_data->reg_tr) {
		priv_data->reg_tr = array->fhctl_base
			+ (unsigned long)match_data->reg_tr;
	}
	priv_data->map = match_data->map;

	/* hook to array */
	hdlr->data = priv_data;
	hdlr->ops = match->hdlr->ops;
	/* hook hdlr to array is the last step */
	mb();
	array->hdlr = hdlr;

	/* do SSC */
	if (array->ssc_rate) {
		struct fh_hdlr *hdlr;

		hdlr = array->hdlr;
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				array->ssc_rate);
	}

	return 0;

__fail:
	kfree(priv_data);
	return -ENOMEM;
}

static struct fh_operation vcp_ops_v1 = {
	.hopping = vcp_hopping_v1,
	.ssc_enable = vcp_ssc_enable_v1,
	.ssc_disable = vcp_ssc_disable_v1,
};


/*mt6985 begin*/
static struct id_map map_6985[] = {
	{"top1", 1000},
	{}
};
struct hdlr_data_v1 hdlr_data_6985_vcp = {
	.reg_tr = NULL,
	.map = map_6985,
};
static struct fh_hdlr vcp_hdlr_6985 = {
	.ops = &vcp_ops_v1,
	.data = &hdlr_data_6985_vcp,
};

static struct match mt6985_match = {
	.name = "mediatek,mt6985-fhctl",
	.hdlr = &vcp_hdlr_6985,
	.init = &vcp_init_v1,
};
/*mt6985 end*/




/*mt6989 begin*/
static struct id_map map_6989[] = {
	{"top1", 1000},
	{}
};
struct hdlr_data_v1 hdlr_data_6989_vcp = {
	.reg_tr = NULL,
	.map = map_6989,
};
static struct fh_hdlr vcp_hdlr_6989 = {
	.ops = &vcp_ops_v1,
	.data = &hdlr_data_6989_vcp,
};

static struct match mt6989_match = {
	.name = "mediatek,mt6989-fhctl",
	.hdlr = &vcp_hdlr_6989,
	.init = &vcp_init_v1,
};
/*mt6989 end*/




static struct match *matches[] = {
	&mt6985_match,
	&mt6989_match,
	NULL,
};

int fhctl_vcp_init(struct pll_dts *array)
{
	int i;
	int num_pll;
	struct match **match;

	FHDBG("\n");
	match = matches;
	num_pll = array->num_pll;

	/* find match by compatible */
	while (*match != NULL) {
		char *comp = (*match)->name;
		char *target = array->comp;

		if (strcmp(comp,
					target) == 0) {
			break;
		}
		match++;
	}

	if (*match == NULL) {
		FHDBG("no match!\n");
		return -1;
	}

	/* init flow for every pll */
	for (i = 0; i < num_pll ; i++, array++) {
		char *method = array->method;

		if (strcmp(method,
					FHCTL_TARGET) == 0) {
			(*match)->init(array, *match);
		}
	}

	FHDBG("\n");
	return 0;
}
