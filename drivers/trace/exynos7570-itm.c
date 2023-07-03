/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITM) Driver for Samsung Exynos7570 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define pr_fmt(fmt) "[ITM] detect: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/exynos-itm.h>
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
#include <linux/exynos-modem-ctrl.h>
#endif

/* S-NODE, M-NODE Common */
#define OFFSET_TIMEOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)

#define REG_DBG_CTL			(0x10)
#define REG_TIMEOUT_INIT_VAL		(0x14)
#define REG_R_TIMEOUT_MO		(0x18)
#define REG_W_TIMEOUT_MO		(0x1C)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))

#define S_NODE				(0)
#define M_NODE				(1)
#define T_S_NODE			(2)
#define T_M_NODE			(3)

#define	DATA_BACKBONE_PATH		(0)
#define PERI_PATH			(1)
#define PATH_NUM			(2)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPORTED		(2)
#define	ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define	ERRCODE_UNKNOWN_5		(5)
#define	ERRCODE_TIMEOUT			(6)

#define TIMEOUT				(0xFFFFF)
#define TIMEOUT_TEST			(0x1)

#define NEED_TO_CHECK			(0xCAFE)
#define BAAW_RETURN			(0x08000000)

struct itm_dev *gitm;

struct itm_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
	unsigned int shift_bits;
};

struct itm_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct itm_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	unsigned int time_val;
	bool timeout_enabled;
	bool err_rpt_enabled;
	bool retention;
};

/* Error Code Description */
static char *itm_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout",
	"Invalid errorcode",
};

struct itm_nodegroup {
	int irq;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	struct itm_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int irq_occurred;
	bool panic_delayed;
};

struct itm_platdata {
	struct itm_rpathinfo	*rpathinfo;
	struct itm_masterinfo	*masterinfo;
	struct itm_nodegroup	*nodegroup;
	bool probed;
};

static struct itm_rpathinfo rpathinfo[] = {
	{0,	"CPU",		"DREX_CPU",	GENMASK(0, 0),	1},
	{1,	"DBG",		"DREX_CPU",	GENMASK(0, 0),	1},

	{0,	"GNSS",		"DREX_CP",	GENMASK(1, 0),	2},
	{1,	"CP",		"DREX_CP",	GENMASK(1, 0),	2},
	{2,	"WFBT",		"DREX_CP",	GENMASK(1, 0),	2},

	{0,	"MFCMSCL",	"DREX_MM",	GENMASK(2, 0),	3},
	{1,	"G3D",		"DREX_MM",	GENMASK(2, 0),	3},
	{2,	"FSYS",		"DREX_MM",	GENMASK(2, 0),	3},
	{3,	"PDMA",		"DREX_MM",	GENMASK(2, 0),	3},
	{4,	"APM",		"DREX_MM",	GENMASK(2, 0),	3},
	{5,	"ISP",		"DREX_MM",	GENMASK(2, 0),	3},
	{6,	"DISPAUD",	"DREX_MM",	GENMASK(2, 0),	3},

	{0,	"CPU",		"PERI",		GENMASK(3, 0),	4},
	{1,	"DBG",		"PERI",		GENMASK(3, 0),	4},
	{2,	"MFCMSCL",	"PERI",		GENMASK(3, 0),	4},
	{3,	"G3D",		"PERI",		GENMASK(3, 0),	4},
	{4,	"FSYS",		"PERI",		GENMASK(3, 0),	4},
	{5,	"PDMA",		"PERI",		GENMASK(3, 0),	4},
	{6,	"APM",		"PERI",		GENMASK(3, 0),	4},
	{7,	"GNSS",		"PERI",		GENMASK(3, 0),	4},
	{8,	"CP",		"PERI",		GENMASK(3, 0),	4},
	{9,	"WFBT",		"PERI",		GENMASK(3, 0),	4},

	{0,	"CPU",		"APMP",		GENMASK(3, 0),	4},
	{1,	"DBG",		"APMP",		GENMASK(3, 0),	4},
	{2,	"MFCMSCL",	"APMP",		GENMASK(3, 0),	4},
	{3,	"G3D",		"APMP",		GENMASK(3, 0),	4},
	{4,	"FSYS",		"APMP",		GENMASK(3, 0),	4},
	{5,	"PDMA",		"APMP",		GENMASK(3, 0),	4},
	{6,	"APM",		"APMP",		GENMASK(3, 0),	4},
	{7,	"GNSS",		"APMP",		GENMASK(3, 0),	4},
	{8,	"CP",		"APMP",		GENMASK(3, 0),	4},
	{9,	"WFBT",		"APMP",		GENMASK(3, 0),	4},
};

/* XIU ID Information */
static struct itm_masterinfo masterinfo[] = {
	{"DISPAUD",	BIT(3),				"IDMA0",	GENMASK(3, 2)},
	{"DISPAUD",	0,				"IDMA1",	GENMASK(3, 2)},
	{"DISPAUD",	BIT(2),				"IDMA2",	GENMASK(3, 2)},

	{"ISP",		0,				"CSIS0",	GENMASK(3, 1)},
	{"ISP",		BIT(1),				"FIMC_ISP",	GENMASK(3, 1)},
	{"ISP",		BIT(2),				"MC_SCALER",	GENMASK(3, 1)},
	{"ISP",		BIT(1) | BIT(2),		"FIMC_VRA",	GENMASK(3, 1)},

	{"FSYS",	0,				"MMC55-0",	GENMASK(2, 0)},
	{"FSYS",	BIT(0),				"MMC50-2",	GENMASK(2, 0)},
	{"FSYS",	BIT(1),				"SSS0",		GENMASK(2, 0)},
	{"FSYS",	BIT(0) | BIT(1),		"RTIC",		GENMASK(2, 0)},
	{"FSYS",	BIT(2),				"USB20DRD",	GENMASK(2, 0)},

	{"MFCMSCL",	0,				"JPEG",		GENMASK(2, 1)},
	{"MFCMSCL",	BIT(1),				"M2M_Poly",	GENMASK(2, 1)},
	{"MFCMSCL",	BIT(2),				"M2M_Bl",	GENMASK(2, 1)},
	{"MFCMSCL",	BIT(1) | BIT(2),		"MFC",		GENMASK(2, 1)},

	{"CP",		0,				"CR4M",		GENMASK(4, 3)},
	{"CP",		BIT(3),				"TL3MtoL2",	GENMASK(4, 3)},
	{"CP",		BIT(4),				"DMA",		GENMASK(4, 2)},
	{"CP",		BIT(2) | BIT(4),		"CSXAP",	GENMASK(4, 0)},
	{"CP",		BIT(0) | BIT(2) | BIT(4),	"DMtoL2",	GENMASK(4, 0)},
	{"CP",		BIT(1) | BIT(2) | BIT(4),	"LMAC",		GENMASK(4, 0)},
	{"CP",		BIT(0) | BIT(1) | BIT(2) | BIT(4),"HMtoL2",	GENMASK(4, 0)},

	{"WFBT",	0,				"SXCR4",	GENMASK(1, 0)},
	{"WFBT",	BIT(0),				"SXCR4",	GENMASK(5, 3) | GENMASK(1, 0)},
	{"WFBT",	BIT(1),				"SHBWL",	GENMASK(3, 0)},

	/* Others */
	{"CPU",		0,				"",		0},
	{"DBG",		0,				"",		0},
	{"G3D",		0,				"",		0},
	{"PDMA",	0,				"",		0},
	{"APM",		0,				"",		0},
};

/* data_backbone_path is sorted by INT_VEC_DEBUG_INTERRUPT_VECTOR_TABLE bits */
static struct itm_nodeinfo data_backbone_path[] = {
	{M_NODE, "APM",		0x12543000, NULL, TIMEOUT, true,  true, false},
	{M_NODE, "CP",		0x12483000, NULL, 0,	   true,  true, false},
	{M_NODE, "CPU",		0x12403000, NULL, 0,	   true,  true, false},
	{M_NODE, "DBG",		0x12413000, NULL, 0,	   false, true, false},
	{M_NODE, "DISPAUD",	0x12463000, NULL, 0,	   false, true, false},
	{M_NODE, "FSYS",	0x12443000, NULL, 0,	   false, true, false},
	{M_NODE, "G3D",		0x12433000, NULL, 0,	   false, true, false},
	{M_NODE, "GNSS",	0x12473000, NULL, 0,	   false, true, false},
	{M_NODE, "ISP",		0x12453000, NULL, 0,	   false, true, false},
	{M_NODE, "MFCMSCL",	0x12423000, NULL, 0,	   false, true, false},
	{M_NODE, "PDMA",	0x12533000, NULL, 0,	   false, true, false},
	{M_NODE, "WFBT",	0x12493000, NULL, 0,	   false, true, false},
	{S_NODE, "APMP",	0x124E3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "DREX_CP",	0x124B3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "DREX_CPU",    0x124A3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "DREX_MM",	0x124C3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "PERI",	0x124D3000, NULL, TIMEOUT, true,  true, false},
};

/* peri_path is sorted by INT_VEC_DEBUG_INTERRUPT_VECTOR_TABLE bits */
static struct itm_nodeinfo peri_path[] = {
	{M_NODE, "MIFND",	0x12603000, NULL, 0,	   false, true, false},
	{S_NODE, "BUS_SFR",	0x12693000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "CPU_SFR",	0x12623000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "DISPAUD_SFR",	0x12673000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "FSYS_SFR",	0x12653000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "G3D_SFR",	0x12643000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "GIC_SFR",	0x126B3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "ISP_SFR",	0x12663000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "MFCMSCL_SFR", 0x12663000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "MIF_SFR",	0x126A3000, NULL, TIMEOUT, true,  true, false},
	{S_NODE, "PERI_SFR",	0x12683000, NULL, TIMEOUT, true,  true, false},
};

static struct itm_nodegroup nodegroup[] = {
	{378,	"DATA_BACKBONE",0x125F3000, NULL, data_backbone_path,	ARRAY_SIZE(data_backbone_path), 0, false},
	{382,	"PERI",		0x126F3000, NULL, peri_path,		ARRAY_SIZE(peri_path),		0, false},
};

struct itm_dev {
	struct device			*dev;
	struct itm_platdata		*pdata;
	struct of_device_id		*match;
	int				irq;
	int				id;
	void __iomem			*regs;
	spinlock_t			ctrl_lock;
	struct itm_notifier		notifier_info;
};

struct itm_panic_block {
	struct notifier_block nb_panic_block;
	struct itm_dev *pdev;
};

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(itm_notifier_list);

static const struct of_device_id itm_dt_match[] = {
	{ .compatible = "samsung,exynos-itm",
	  .data = NULL, },
	{},
};
MODULE_DEVICE_TABLE(of, itm_dt_match);

static struct itm_rpathinfo* itm_get_rpathinfo
					(struct itm_dev *itm,
					 unsigned int id,
					 char *dest_name)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_rpathinfo *rpath = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
					dest_name, strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = &pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct itm_masterinfo* itm_get_masterinfo
					(struct itm_dev *itm,
					 char *port_name,
					 unsigned int user)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_masterinfo *master = NULL;
	unsigned int val;
	int i;

	for (i = 0; i < ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name, port_name, strlen(port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = &pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void itm_init(struct itm_dev *itm, bool enabled)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_nodeinfo *node;
	unsigned int offset;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].timeout_enabled) {
				offset = OFFSET_TIMEOUT_REG;
				/* Enable Timeout setting */
				__raw_writel(enabled, node[j].regs + offset + REG_DBG_CTL);
				/* set timeout interval value */
				__raw_writel(node[j].time_val,
					node[j].regs + offset + REG_TIMEOUT_INIT_VAL);
				pr_debug("Exynos IPM - %s timeout enabled\n", node[j].name);
			}
			if (node[j].err_rpt_enabled) {
				/* clear previous interrupt of req_read */
				offset = OFFSET_REQ_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of req_write */
				offset = OFFSET_REQ_W;
				if (pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_read */
				offset = OFFSET_RESP_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_write */
				offset = OFFSET_RESP_W;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);
				pr_debug("Exynos IPM - %s error reporting enabled\n", node[j].name);
			}
		}
	}
}

static void itm_post_handler_by_master(struct itm_dev *itm,
					struct itm_nodegroup *group,
					char *port, char *master, unsigned int target)
{
	/* After treatment by port */
	if (!port || strlen(port) < 1)
		return;

	if (!strncmp(port, "CP", strlen(port))) {
		/*
		 * if master is DSP and baaw returns remap address
		 * we don't care this
		 */
		if (master && !strncmp(master, "TL3MtoL2",strlen(master)) && target == BAAW_RETURN) {
			group->panic_delayed = true;
			group->irq_occurred = 0;
			pr_info("ITM skips CP's DSP(TL3MtoL2) detected\n");
		} else {
			/* Disable busmon all interrupts */
			itm_init(itm, false);
			group->panic_delayed = true;
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
			ss310ap_force_crash_exit_ext();
#endif
		}
	} else if (!strncmp(port, "CPU", strlen(port))) {
		pr_info("ITM is disabled for CPU exception\n");
		/* Disable busmon all interrupts */
		itm_init(itm, false);
		group->panic_delayed = true;
	}
}

static void itm_report_route(struct itm_dev *itm,
				struct itm_nodegroup *group,
				struct itm_nodeinfo *node,
				unsigned int offset, unsigned int target)
{
	struct itm_masterinfo *master = NULL;
	struct itm_rpathinfo *rpath = NULL;
	unsigned int val, id;
	char *port = NULL, *source = NULL, *dest = NULL;

	val = __raw_readl(node->regs + offset + REG_INT_INFO);
	id = BIT_AXID(val);

	if (!strncmp(group->name, "DATA_BACKBONE", strlen(group->name))) {
		/* Data Path */
		if (node->type == S_NODE) {
			rpath = itm_get_rpathinfo(itm, id, node->name);
			if (!rpath) {
				pr_info("failed to get route path - %s, id:%x\n",
						node->name, id);
				return;
			}
			id = (id >> rpath->shift_bits);
			master = itm_get_masterinfo(itm, rpath->port_name, id);
			if (!master) {
				pr_info("failed to get master IP with "
					"port:%s, id:%x\n", rpath->port_name, id);
				return;
			}
			port = rpath->port_name;
			source = master->master_name;
			dest = rpath->dest_name;
		} else {
			master = itm_get_masterinfo(itm, node->name, id);
			if (!master) {
				pr_info("failed to get master IP with "
					"port:%s, id:%x\n", node->name, id);
				return;
			}
			port = node->name;
			source = master->master_name;
		}
	} else {
		/*
		 * PERI_PATH
		 * In this case, we will get the route table from DATA_BACKBONE interrupt
		 * of PERI port
		 */
		if (node->type == S_NODE) {
			dest = node->name;
		} else {
			port = node->name;
		}
	}

	pr_info("\n--------------------------------------------------------------------------------\n\n"
		"ROUTE INFORMATION - %s\n"
		"> Master : %s %s\n"
		"> Slave  : %s\n\n",
		group->name,
		port ? port : "Note other NODE Information",
		source ? source : "",
		dest ? dest : "Note other NODE Information");

	itm_post_handler_by_master(itm, group, port, source, target);
}

static void itm_report_info(struct itm_dev *itm,
			       struct itm_nodegroup *group,
			       struct itm_nodeinfo *node,
			       unsigned int offset)
{
	unsigned int errcode, int_info, info0, info1, info2;
	bool read = false, req = false;

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	if (!BIT_ERR_VALID(int_info)) {
		pr_info("no information, %s/offset:%x is stopover, "
			"check other node\n", node->name, offset);
		return;
	}

	errcode = BIT_ERR_CODE(int_info);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);

	switch(offset) {
	case OFFSET_REQ_R:
		read = true;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		if (node->type == S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logged, see more following information\n");
			goto out;
		}
		break;
	case OFFSET_RESP_R:
		read = true;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		if (node->type != S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logged, see more following information\n");
			goto out;
		}
		break;
	default:
		pr_info("Unknown Error - offset:%u\n", offset);
		goto out;
	}
	/* Normally fall down to here */
	itm_report_route(itm, group, node, offset, info0);

	pr_info("\n--------------------------------------------------------------------------------\n\n"
		"TRANSACTION INFORMATION\n"
		"> Path Type       : %s in %s %s \n"
		"> Target address  : 0x%08X\n"
		"> Error type      : %s\n\n",
		group->name,
		read ? "READ" : "WRITE",
		req ? "REQUEST" : "RESPONSE",
		info0,
		itm_errcode[errcode]);

out:
	/* report extention raw information of register */
	pr_info("\n--------------------------------------------------------------------------------\n\n"
		"NODE RAW INFORMATION\n"
		"> NODE NAME       : %s(%s)\n"
		"> NODE ADDRESS    : 0x%08X\n"
		"> INTERRUPT_INFO  : 0x%08X\n"
		"> EXT_INFO_0      : 0x%08X\n"
		"> EXT_INFO_1      : 0x%08X\n"
		"> EXT_INFO_2      : 0x%08X\n\n"
		"--------------------------------------------------------------------------------\n",
		node->name,
		node->type ? "M_NODE" : "S_NODE",
		node->phy_regs + offset,
		int_info,
		info0,
		info1,
		info2);
}

void itm_dump_all(struct itm_dev *itm)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_nodeinfo *node;
	u32 int_info, info0, info1, info2, offset, i, j, k;

	pr_info("NODE RAW INFORMATION\n"
		"> NODE NAME   : NODE ADDRESS:  INTERRUPT_INFO:   EXT_INFO_0:   EXT_INFO_1:    EXT_INFO_2: \n");
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			for (k = 0; k < OFFSET_NUM; k++) {
				offset = k * OFFSET_ERR_REPT;
				int_info = __raw_readl(node[j].regs + offset + REG_INT_INFO);
				info0 = __raw_readl(node[j].regs + offset + REG_EXT_INFO_0);
				info1 = __raw_readl(node[j].regs + offset + REG_EXT_INFO_1);
				info2 = __raw_readl(node[j].regs + offset + REG_EXT_INFO_2);
				/* report extention raw information of register */
				pr_info(
					">  %s(%s)  0x%08X  0x%08X  0x%08X  0x%08X  0x%08X\n",
					node[j].name,
					node[j].type ? "M_NODE" : "S_NODE",
					node[j].phy_regs + offset,
					int_info,
					info0,
					info1,
					info2);
			}
		}
	}
}

static int itm_parse_info(struct itm_dev *itm,
			      struct itm_nodegroup *group,
			      bool clear)
{
	//struct itm_platdata *pdata = itm->pdata;
	struct itm_nodeinfo *node = NULL;
	unsigned int val, offset, vec;
	unsigned long flags, bit = 0;
	int i, j, ret = 0;

	spin_lock_irqsave(&itm->ctrl_lock, flags);
	if (group) {
		/* Processing only this group and select detected node */
		vec = __raw_readl(group->regs);
		node = group->nodeinfo;
		if (!vec)
			pr_info("invalid detection\n");

		for_each_set_bit(bit, (unsigned long *)&vec, group->nodesize) {
			/* exist array */
			for (i = 0; i < OFFSET_NUM; i++) {
				offset = i * OFFSET_ERR_REPT;
				/* Check Request information */
				val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
				if (BIT_ERR_OCCURRED(val)) {
					/* This node occurs the error */
					itm_report_info(itm, group, &node[bit], offset);
					if (clear)
						__raw_writel(1, node[bit].regs + offset + REG_INT_CLR);
					ret = true;
				}
			}

		}
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
			group = &nodegroup[i];
			vec = __raw_readl(group->regs);
			node = group->nodeinfo;
			bit = 0;

			for_each_set_bit(bit, (unsigned long *)&vec, group->nodesize) {
				for (j = 0; j < OFFSET_NUM; j++) {
					offset = j * OFFSET_ERR_REPT;
					/* Check Request information */
					val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
					if (BIT_ERR_OCCURRED(val)) {
						/* This node occurs the error */
						itm_report_info(itm, group, &node[bit], offset);
						if (clear)
							__raw_writel(1,
								node[j].regs + offset + REG_INT_CLR);
						ret = true;
					}
				}
			}
		}
	}
	spin_unlock_irqrestore(&itm->ctrl_lock, flags);
	return ret;
}

static irqreturn_t itm_irq_handler(int irq, void *data)
{
	struct itm_dev *itm = (struct itm_dev *)data;
	struct itm_platdata *pdata = itm->pdata;
	struct itm_nodegroup *group = NULL;
	bool ret;
	int i;

	/* convert from raw irq source to SPI irq number */
	irq = irq - 32;

	/* Search itm group */
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		if (irq == nodegroup[i].irq) {
			pr_info("%s group, %d interrupt occurrs \n",
				pdata->nodegroup[i].name, irq);
			group = &pdata->nodegroup[i];
			break;
		}
	}

	if (group) {
		ret = itm_parse_info(itm, group, true);
		if (!ret) {
			pr_info("can't process %s irq:%d IRQ_VECTOR:%x\n",
				group->name, irq, __raw_readl(group->regs));
		} else {
			if (group->irq_occurred && !group->panic_delayed)
				panic("STOP infinite output by Exynos ITM");
			else
				group->irq_occurred++;
		}
		group->panic_delayed = false;
	} else {
		pr_info("can't the group - irq:%d\n", irq);
	}

	return IRQ_HANDLED;
}

void itm_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itm_notifier_list, block);
}

static int itm_logging_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct itm_panic_block *itm_panic = (struct itm_panic_block *)nb;
	struct itm_dev *itm = itm_panic->pdev;
	int ret;

	if (!IS_ERR_OR_NULL(itm)) {
		/* Check error has been logged */
		ret = itm_parse_info(itm, NULL, false);
		if (!ret)
			pr_info("No found error in %s\n", __func__);
		else
			pr_info("Found errors in %s\n", __func__);
	}
	return 0;
}

static int itm_probe(struct platform_device *pdev)
{
	struct itm_dev *itm;
	struct itm_panic_block *itm_panic = NULL;
	struct itm_platdata *pdata;
	struct itm_nodeinfo *node;
	char *dev_name;
	int ret, i, j;

	itm = devm_kzalloc(&pdev->dev, sizeof(struct itm_dev), GFP_KERNEL);
	if (!itm) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}
	itm->dev = &pdev->dev;

	spin_lock_init(&itm->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct itm_platdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"platform data\n");
		return -ENOMEM;
	}
	itm->pdata = pdata;
	itm->pdata->masterinfo = masterinfo;
	itm->pdata->rpathinfo = rpathinfo;
	itm->pdata->nodegroup = nodegroup;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++)
	{
		dev_name = nodegroup[i].name;
		node = nodegroup[i].nodeinfo;

		nodegroup[i].regs = devm_ioremap_nocache(&pdev->dev, nodegroup[i].phy_regs, SZ_16K);
		if (nodegroup[i].regs == NULL) {
			dev_err(&pdev->dev, "failed to claim register region - %s\n", dev_name);
			return -ENOENT;
		}

		ret = devm_request_irq(&pdev->dev, nodegroup[i].irq + 32,
					itm_irq_handler, 0, //IRQF_GIC_MULTI_TARGET,
					dev_name, itm);

		for (j = 0; j < nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev, node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n", dev_name);
				return -ENOENT;
			}
		}
	}

	itm_panic = devm_kzalloc(&pdev->dev,
			sizeof(struct itm_panic_block), GFP_KERNEL);

	if (!itm_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"panic handler data\n");
	} else {
		itm_panic->nb_panic_block.notifier_call =
					itm_logging_panic_handler;
		itm_panic->pdev = itm;
		atomic_notifier_chain_register(&panic_notifier_list,
					&itm_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itm);

	itm_init(itm, true);
	pdata->probed = true;
	gitm = itm;

	dev_info(&pdev->dev, "success to probe ITM driver\n");

	return 0;
}

void itm_dump(void)
{
	itm_dump_all(gitm);
}

static int itm_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itm_suspend(struct device *dev)
{
	return 0;
}

static int itm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itm_dev *itm = platform_get_drvdata(pdev);

	itm_init(itm, true);

	return 0;
}

static SIMPLE_DEV_PM_OPS(itm_pm_ops,
			 itm_suspend,
			 itm_resume);
#define ITM_PM	(itm_pm_ops)
#else
#define ITM_PM	NULL
#endif

static struct platform_driver exynos_itm_driver = {
	.probe		= itm_probe,
	.remove		= itm_remove,
	.driver		= {
		.name		= "exynos-itm",
		.of_match_table	= itm_dt_match,
		.pm		= &itm_pm_ops,
	},
};

module_platform_driver(exynos_itm_driver);

MODULE_DESCRIPTION("Samsung Exynos ITM DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itm");
