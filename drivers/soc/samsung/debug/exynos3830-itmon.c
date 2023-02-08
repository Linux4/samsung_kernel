/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos3830 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-itmon.h>
#include <soc/samsung/exynos-debug.h>
#include <linux/debug-snapshot.h>
#include <linux/sec_debug.h>

//#define MULTI_IRQ_SUPPORT_ITMON

#define OFFSET_TMOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)
#define OFFSET_PROT_CHK			(0x100)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)

#define REG_DBG_CTL			(0x10)
#define REG_TMOUT_INIT_VAL		(0x14)
#define REG_TMOUT_FRZ_EN		(0x18)
#define REG_TMOUT_BUF_WR_OFFSET		(0x20)

#define REG_TMOUT_BUF_STATUS		(0x1C)
#define REG_TMOUT_BUF_POINT_ADDR	(0x20)
#define REG_TMOUT_BUF_ID		(0x24)
#define REG_TMOUT_BUF_PAYLOAD		(0x28)
#define REG_TMOUT_BUF_PAYLOAD_SRAM1	(0x30)
#define REG_TMOUT_BUF_PAYLOAD_SRAM2	(0x34)
#define REG_TMOUT_BUF_PAYLOAD_SRAM3	(0x38)

#define REG_PROT_CHK_CTL		(0x4)
#define REG_PROT_CHK_INT		(0x8)
#define REG_PROT_CHK_INT_ID		(0xC)
#define REG_PROT_CHK_START_ADDR_LOW	(0x10)
#define REG_PROT_CHK_END_ADDR_LOW	(0x14)
#define REG_PROT_CHK_START_END_ADDR_UPPER	(0x18)

#define RD_RESP_INT_ENABLE		(1 << 0)
#define WR_RESP_INT_ENABLE		(1 << 1)
#define ARLEN_RLAST_INT_ENABLE		(1 << 2)
#define AWLEN_WLAST_INT_ENABLE		(1 << 3)
#define INTEND_ACCESS_INT_ENABLE	(1 << 4)

#define BIT_PROT_CHK_ERR_OCCURRED(x)	(((x) & (0x1 << 0)) >> 0)
#define BIT_PROT_CHK_ERR_CODE(x)	(((x) & (0xF << 1)) >> 28)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFF << 16)) >> 16)
#define BIT_AXBURST(x)			(((x) & (0x3)))
#define BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

#define M_NODE				(0)
#define T_S_NODE			(1)
#define T_M_NODE			(2)
#define S_NODE				(3)
#define NODE_TYPE			(4)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPORTED		(2)
#define ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define ERRCODE_UNKNOWN_5		(5)
#define ERRCODE_TMOUT			(6)

#define BUS_DATA			(0)
#define BUS_PERI			(1)
#define BUS_PATH_TYPE			(2)

#define TRANS_TYPE_WRITE		(0)
#define TRANS_TYPE_READ			(1)
#define TRANS_TYPE_NUM			(2)

#define FROM_CPU			(0)
#define FROM_IP				(2)
#define FROM_OTHERS			(8)

#define NOT_AVAILABLE_STR		"N/A"
#define CP_COMMON_STR			"CP_"

#define TMOUT				(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define PANIC_GO_THRESHOLD		(100)
#define INVALID_REMAPPING		(0x03000000)

static struct itmon_dev *g_itmon = NULL;

struct itmon_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
	unsigned int shift_bits;
};

struct itmon_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct itmon_nodegroup;

struct itmon_traceinfo {
	char *port;
	char *master;
	char *dest;
	unsigned long target_addr;
	unsigned int errcode;
	bool read;
	bool path_dirty;
	bool snode_dirty;
	bool dirty;
	unsigned long from;
	int path_type;
	char buf[SZ_32];
};

struct itmon_tracedata {
	unsigned int int_info;
	unsigned int ext_info_0;
	unsigned int ext_info_1;
	unsigned int ext_info_2;
	unsigned int dbg_mo_cnt;
	unsigned int prot_chk_ctl;
	unsigned int prot_chk_info;
	unsigned int prot_chk_int_id;
	unsigned int offset;
	bool logging;
	bool read;
};

struct itmon_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	unsigned int time_val;
	bool tmout_enabled;
	bool tmout_frz_enabled;
	bool err_enabled;
	bool hw_assert_enabled;
	bool retention;
	struct itmon_tracedata tracedata;
	struct itmon_nodegroup *group;
	struct list_head list;
};

struct itmon_nodegroup {
	int irq;
	char *name;
	unsigned int phy_regs;
	bool ex_table;
	void __iomem *regs;
	struct itmon_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int bus_type;
};

struct itmon_platdata {
	const struct itmon_rpathinfo *rpathinfo;
	const struct itmon_masterinfo *masterinfo;
	struct itmon_nodegroup *nodegroup;
	struct itmon_traceinfo traceinfo[TRANS_TYPE_NUM];
	struct list_head tracelist[TRANS_TYPE_NUM];
	ktime_t last_time;
	bool cp_crash_in_progress;
	unsigned int sysfs_tmout_val;

	bool err_fatal;
	bool err_ip;
	bool err_drex_tmout;
	bool err_cpu;
	bool err_cp;
	bool err_chub;

	unsigned int err_cnt;
	unsigned int err_cnt_by_cpu;
	bool probed;
};

struct itmon_dev {
	struct device *dev;
	struct itmon_platdata *pdata;
	struct of_device_id *match;
	int irq;
	int id;
	void __iomem *regs;
	spinlock_t ctrl_lock;
	struct itmon_notifier notifier_info;
};

struct itmon_panic_block {
	struct notifier_block nb_panic_block;
	struct itmon_dev *pdev;
};

const static struct itmon_rpathinfo rpathinfo[] = {
	/* Data BUS: Master ----> 0x2000_0000 -- 0xF_FFFF_FFFF */
	{0,	"ALIVE",	"NRT_MEM",	GENMASK(3, 0),	0},
	{1,	"CHUB",		"NRT_MEM",	GENMASK(3, 0),	0},
	{2,	"WLBT",		"NRT_MEM",	GENMASK(3, 0),	0},
	{3,	"COREX",	"NRT_MEM",	GENMASK(3, 0),	0},
	{4,	"CSSYS",	"NRT_MEM",	GENMASK(3, 0),	0},
	{5,	"HSI",		"NRT_MEM",	GENMASK(3, 0),	0},
	{6,	"G3D",		"NRT_MEM",	GENMASK(3, 0),	0},
	{7,	"GNSS",		"NRT_MEM",	GENMASK(3, 0),	0},
	{8,	"IS1",		"NRT_MEM",	GENMASK(3, 0),	0},
	{9,	"MFCMSCL",	"NRT_MEM",	GENMASK(3, 0),	0},
	{10,	"CP_1",		"NRT_MEM",	GENMASK(3, 0),	0},

	/* RT_MEM: Master ----> 0x2000_0000 -- 0xF_FFFF_FFFF */
	{0,	"IS0",		"RT_MEM",	GENMASK(2, 0),	0},
	{1,	"DPU",		"RT_MEM",	GENMASK(2, 0),	0},
	{2,	"WLBT",		"RT_MEM",	GENMASK(2, 0),	0},
	{3,	"GNSS",		"RT_MEM",	GENMASK(2, 0),	0},
	{4,	"CP_1",		"RT_MEM",	GENMASK(2, 0),	0},
	{5,	"IS1",		"RT_MEM",	GENMASK(2, 0),	0},
	{6,	"AUD",		"RT_MEM",	GENMASK(2, 0),	0},

	/* CP_MEM: Master ----> 0x2000_0000 -- 0xF_FFFF_FFFF */
	{0,	"CP_0",		"CP_MEM",	GENMASK(2, 0),	0},
	{1,	"AUD",		"CP_MEM",	GENMASK(2, 0),	0},
	{2,	"WLBT",		"CP_MEM",	GENMASK(2, 0),	0},
	{3,	"CP_1",		"CP_MEM",	GENMASK(2, 0),	0},

	/* Peri BUS: Master ----> 0x0 -- 0x1FFF-FFFF */
	{0,	"AUD",		"PERI",		GENMASK(3, 0),	0},
	{1,	"APM",		"PERI",		GENMASK(3, 0),	0},
	{2,	"IS1",		"PERI",		GENMASK(3, 0),	0},
	{3,	"MFCMSCL",	"PERI",		GENMASK(3, 0),	0},
	{4,	"CP_0",		"PERI",		GENMASK(3, 0),	0},
	{5,	"CP_1",		"PERI",		GENMASK(3, 0),	0},
	{6,	"WLBT",		"PERI",		GENMASK(3, 0),	0},
	{7,	"CHUB",		"PERI",		GENMASK(3, 0),	0},
	{8,	"COREX",	"PERI",		GENMASK(3, 0),	0},
	{9,	"CSSYS",	"PERI",		GENMASK(3, 0),	0},
	{10,	"DPU",		"PERI",		GENMASK(3, 0),	0},
	{11,	"HSI",		"PERI",		GENMASK(3, 0),	0},
	{12,	"G3D",		"PERI",		GENMASK(3, 0),	0},
	{13,	"GNSS",		"PERI",		GENMASK(3, 0),	0},
	{14,	"IS0",		"PERI",		GENMASK(3, 0),	0},
};

/* XIU ID Information */
const static struct itmon_masterinfo masterinfo[] = {
	{"DPU",		BIT(0),				"SYSMMU_D_DPU",		GENMASK(0, 0)},	/* XXXXXX1 */
	{"DPU",		0,				"DPU_DMA",		GENMASK(0, 0)},	/* XXXXXX0 */

	{"AUD",		BIT(0),				"SYSMMU_D_AUD",		GENMASK(0, 0)},	/* XXXXXX0 */
	{"AUD",		0,				"SPUS/SPUM",		GENMASK(2, 0)},	/* XXXX000 */
	{"AUD",		BIT(1),				"ABOX CA7",		GENMASK(2, 0)},	/* XXXX010 */
	{"AUD",		BIT(1) | BIT(2),		"AUDEN",		GENMASK(2, 0)},	/* XXXX110 */

	{"IS0",		BIT(0),				"SYSMMU_D0_IS",		GENMASK(0, 0)},	/* XXXXXX1 */
	{"IS0",		0,				"CSIS",			GENMASK(1, 0)},	/* XXXXX00 */
	{"IS0",		BIT(1),				"IPP",			GENMASK(1, 0)},	/* XXXXX10 */

	{"IS1",		BIT(0),				"SYSMMU_D1_IS",		GENMASK(0, 0)},	/* XXXXXX1 */
	{"IS1",		0,				"ITP",			GENMASK(2, 0)},	/* XXXX000 */
	{"IS1",		BIT(1),				"MCSC",			GENMASK(2, 0)},	/* XXXX010 */
	{"IS1",		BIT(2),				"GDC",			GENMASK(2, 0)},	/* XXXX100 */
	{"IS1",		BIT(1) | BIT(2),		"VRA",			GENMASK(2, 0)},	/* XXXX110 */

	{"HSI",		0,				"MMC_CARD",		GENMASK(0, 0)},	/* XXXXXX0 */
	{"HSI",		BIT(0),				"USB",			GENMASK(0, 0)},	/* XXXXXX1 */

	{"CHUB",	0,				"CM4_CHUB",		GENMASK(0, 0)},	/* XXXXXX0 */

	{"COREX",	0,				"RTIC",			GENMASK(2, 0)},	/* XXXX000 */
	{"COREX",	BIT(0),				"SSS",			GENMASK(2, 0)},	/* XXXX001 */
	{"COREX",	BIT(1),				"PDMA",			GENMASK(2, 0)},	/* XXXX010 */
	{"COREX",	BIT(0) | BIT(1),		"SPDMA",		GENMASK(2, 0)},	/* XXXX011 */
	{"COREX",	BIT(2),				"MMC_EMBD",		GENMASK(2, 0)},	/* XXXX100 */

	{"G3D",		0,				"G3D",			0},		/* XXXXXXX */
	{"CSSYS",	0,				"CSSYS",		0},		/* XXXXXXX */
	{"APM",		0,				"APM",			0},		/* XXXXXXX */

	{"MFCMSCL",	BIT(0),				"SYSMMU_D_MFCMSCL",	GENMASK(0, 0)},	/* XXXXXX1 */
	{"MFCMSCL",	0,				"MFC",			GENMASK(1, 0)},	/* XXXXXX1 */
	{"MFCMSCL",	BIT(0),				"JPEG",			GENMASK(1, 0)},	/* XXXXX01 */
	{"MFCMSCL",	BIT(1),				"MCSC",			GENMASK(1, 0)},	/* XXXXX10 */
	{"MFCMSCL",	BIT(0) | BIT(1),		"M2M",			GENMASK(1, 0)},	/* XXXXX10 */

	{"CP_",		0,				"U-CPU",		GENMASK(6, 5)},	/* 00XXXXX */
	{"CP_",		BIT(3) | BIT(4) | BIT(5),	"L-CPU",		GENMASK(6, 4)},	/* 0111XXX */
	{"CP_",		BIT(5),				"DMA0",			GENMASK(6, 0)},	/* 0111XXX */
	{"CP_",		BIT(0) | BIT(5),		"DMA1",			GENMASK(6, 0)},	/* 0100001 */
	{"CP_",		BIT(1) | BIT(5),		"DMA2",			GENMASK(6, 0)},	/* 0100010 */
	{"CP_",		BIT(3) | BIT(5) | BIT(6),	"CSXAP",		GENMASK(6, 0)},	/* 1101000 */
	{"CP_",		BIT(3) | BIT(5),		"DataMover",		GENMASK(6, 0)},	/* 0101000 */
	{"CP_",		BIT(4) | BIT(5),		"LMAC",			GENMASK(6, 0)},	/* 0110000 */
	{"CP_",		BIT(5) | BIT(6),		"HarqMover",		GENMASK(6, 0)},	/* 1100000 */

	{"WLBT",	0,				"CR4",			GENMASK(0, 0)},	/* XXXXXX0 */
	{"WLBT",	BIT(0),				"ENC_DMA",		GENMASK(4, 0)},	/* XX00001 */

	{"GNSS",	BIT(1),				"CM7F",			GENMASK(5, 0)},	/* X000010 */
	{"GNSS",	0,				"XMDA0",		GENMASK(5, 0)},	/* X000000 */
	{"GNSS",	BIT(0),				"XMDA1",		GENMASK(5, 0)},	/* X000001 */
};

static struct itmon_nodeinfo data_path[] = {
	{M_NODE, "APM",			0x12413000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "AUD",			0x12403000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CHUB",		0x12423000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "COREX",		0x12433000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CSSYS",		0x12443000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "DPU",			0x12453000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "G3D",			0x12473000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "GNSS",		0x12483000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "HSI",			0x12463000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "IS0",			0x12493000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "IS1",			0x124A3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "MFCMSCL",		0x124B3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CP_0",		0x124C3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "CP_1",		0x124D3000, NULL, 0,	   false, false,  true, true, false},
	{M_NODE, "WLBT",		0x124E3000, NULL, 0,	   false, false,  true, true, false},
	{S_NODE, "CP_MEM0",		0x12533000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CP_MEM1",		0x12543000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "NRT_MEM0",		0x12513000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "NRT_MEM1",		0x12523000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "RT_MEM0",		0x124F3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "RT_MEM1",		0x12503000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "PERI",		0x12553000, NULL, TMOUT,   true,  false,  true, true, false},
};

static struct itmon_nodeinfo peri_path[] = {
	{M_NODE, "M_CPU",		0x12803000, NULL, 0,	   false, false, true, true, false},
	{M_NODE, "M_PERI",		0x12813000, NULL, 0,	   false, false, true, true, false},
	{S_NODE, "ALIVE",		0x12823000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "AUD",			0x12833000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CHUB",		0x12953000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "COREP_SFR",		0x12883000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "COREP_TREX",		0x12873000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CPU_CL0",		0x12843000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CPU_CL1",		0x12853000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CSSYS",		0x12863000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "DPU",			0x12893000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "G3D",			0x128B3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "GIC",			0x128C3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "GNSS",		0x128D3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "HSI",			0x128A3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "ISP",			0x128E3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "MFCMSCL",		0x128F3000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "MFC0",		0x12903000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "MFC1",		0x12913000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "CP",			0x12923000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "PERI",		0x12933000, NULL, TMOUT,   true,  false,  true, true, false},
	{S_NODE, "WLBTP",		0x12943000, NULL, TMOUT,   true,  false,  true, true, false},
};

static struct itmon_nodegroup nodegroup[] = {
	{462, "DATA",	0x12563000, false, NULL, data_path, ARRAY_SIZE(data_path), BUS_DATA},
	{461, "PERI",	0x12973000, false, NULL, peri_path, ARRAY_SIZE(peri_path), BUS_PERI},
};

const static char *itmon_pathtype[] = {
	"DATA Path transaction (0x2000_0000 ~ 0xf_ffff_ffff)",
	"PERI(SFR) Path transaction (0x0 ~ 0x1fff_ffff)",
};

/* Error Code Description */
const static char *itmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout in timeout value",
	"Invalid errorcode",
};

const static char *itmon_nodestring[] = {
	"M_NODE",
	"TAXI_S_NODE",
	"TAXI_M_NODE",
	"S_NODE",
};

/* declare notifier_list */
ATOMIC_NOTIFIER_HEAD(itmon_notifier_list);

static const struct of_device_id itmon_dt_match[] = {
	{.compatible = "samsung,exynos-itmon",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, itmon_dt_match);

static struct itmon_rpathinfo *itmon_get_rpathinfo(struct itmon_dev *itmon,
					       unsigned int id,
					       char *dest_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_rpathinfo *rpath = NULL;
	int i;

	if (!dest_name)
		return NULL;

	for (i = 0; i < (int)ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
						  dest_name,
						  strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = (struct itmon_rpathinfo *)&pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct itmon_masterinfo *itmon_get_masterinfo(struct itmon_dev *itmon,
						 char *port_name,
						 unsigned int user)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_masterinfo *master = NULL;
	unsigned int val;
	int i;

	if (!port_name)
		return NULL;

	for (i = 0; i < (int)ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name, port_name, strlen(masterinfo[i].port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = (struct itmon_masterinfo *)&pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void itmon_init(struct itmon_dev *itmon, bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node;
	unsigned int offset;
	int i, j;

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].tmout_enabled) {
				offset = OFFSET_TMOUT_REG;
				/* Enable Timeout setting */
				__raw_writel(enabled, node[j].regs + offset + REG_DBG_CTL);
				/* set tmout interval value */
				__raw_writel(node[j].time_val,
					     node[j].regs + offset + REG_TMOUT_INIT_VAL);
				dev_dbg(itmon->dev, "Exynos ITMON - %s timeout enabled\n", node[j].name);
				if (node[j].tmout_frz_enabled) {
					/* Enable freezing */
					__raw_writel(enabled,
						     node[j].regs + offset + REG_TMOUT_FRZ_EN);
				}
			}
			if (node[j].err_enabled) {
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
				dev_dbg(itmon->dev,
					"Exynos ITMON - %s error reporting enabled\n", node[j].name);
			}
			if (node[j].hw_assert_enabled) {
				offset = OFFSET_PROT_CHK;
				__raw_writel(RD_RESP_INT_ENABLE | WR_RESP_INT_ENABLE |
					     ARLEN_RLAST_INT_ENABLE | AWLEN_WLAST_INT_ENABLE,
						node[j].regs + offset + REG_PROT_CHK_CTL);
			}
		}
	}
}

void itmon_enable(bool enabled)
{
	if (g_itmon)
		itmon_init(g_itmon, enabled);
}

void itmon_set_errcnt(int cnt)
{
	struct itmon_platdata *pdata;

	if (g_itmon) {
		pdata = g_itmon->pdata;
		pdata->err_cnt = cnt;
	}
}

static void itmon_post_handler_apply_policy(struct itmon_dev *itmon,
					    int ret_value)
{
	struct itmon_platdata *pdata = itmon->pdata;

	switch (ret_value) {
	case NOTIFY_OK:
		pr_err("all notify calls response NOTIFY_OK\n"
			    "ITMON doesn't do anything, just logging");
		break;
	case NOTIFY_STOP:
		pr_err("notify calls response NOTIFY_STOP, refer to notifier log\n"
			    "ITMON counts the error count : %d\n", pdata->err_cnt++);
		break;
	case NOTIFY_BAD:
		pr_err("notify calls response NOTIFY_BAD\n"
			    "ITMON goes the PANIC & Debug Action\n");
		pdata->err_ip = true;
		break;
	}
}

static void itmon_post_handler_to_notifier(struct itmon_dev *itmon,
					   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	int ret = 0;

	/* After treatment by port */
	if (!traceinfo->port || strlen(traceinfo->port) < 1)
		return;

	itmon->notifier_info.port = traceinfo->port;
	itmon->notifier_info.master = traceinfo->master;
	itmon->notifier_info.dest = traceinfo->dest;
	itmon->notifier_info.read = traceinfo->read;
	itmon->notifier_info.target_addr = traceinfo->target_addr;
	itmon->notifier_info.errcode = traceinfo->errcode;

	pr_err("\n     +ITMON Notifier Call Information\n\n");

	/* call notifier_call_chain of itmon */
	ret = atomic_notifier_call_chain(&itmon_notifier_list, 0, &itmon->notifier_info);
	itmon_post_handler_apply_policy(itmon, ret);

	pr_err("\n     -ITMON Notifier Call Information\n"
		"--------------------------------------------------------------------------\n");
}

static void itmon_post_handler_by_dest(struct itmon_dev *itmon,
					unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	if (!traceinfo->dest || strlen(traceinfo->dest) < 1)
		return;

	if (traceinfo->errcode == ERRCODE_TMOUT &&
		traceinfo->snode_dirty == true &&
		traceinfo->path_type == BUS_DATA) {
		pdata->err_drex_tmout = true;
	}
}

static void itmon_post_handler_by_master(struct itmon_dev *itmon,
					unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	/* After treatment by port */
	if (!traceinfo->port || strlen(traceinfo->port) < 1)
		return;

	/* This means that the master is CPU */
	if ((!strncmp(traceinfo->port, "CPU", strlen("CPU")))) {
		ktime_t now, interval;

		now = ktime_get();
		interval = ktime_sub(now, pdata->last_time);
		pdata->last_time = now;
		pdata->err_cnt_by_cpu++;
		pdata->err_cpu = true;

		pr_err("CPU transaction detected - "
			"err_cnt_by_cpu: %u, interval: %lluns\n",
			pdata->err_cnt_by_cpu,
			(unsigned long long)ktime_to_ns(interval));
	} else if (!strncmp(traceinfo->port, CP_COMMON_STR, strlen(CP_COMMON_STR))) {
		pdata->err_cp = true;
	}
}

static void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	unsigned int info, axid, valid, timeout, payload;
	unsigned long addr;
	char *master_name, *port_name;
	struct itmon_rpathinfo *port;
	struct itmon_masterinfo *master;
	int i, num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
	int fz_offset = (trans_type == TRANS_TYPE_READ ? 0 : REG_TMOUT_BUF_WR_OFFSET);

	pr_err("\n      TIMEOUT_BUFFER Information\n\n");
	pr_err("      > NUM|   BLOCK|  MASTER|   VALID| TIMEOUT|      ID|   ADDRESS|    INFO|\n");

	for (i = 0; i < num; i++) {
		writel(i, node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_POINT_ADDR + fz_offset);
		axid = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_ID + fz_offset);
		payload = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD + fz_offset);
		addr = (((unsigned long)readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM1 + fz_offset) &
				GENMASK(15, 0)) << 32ULL);
		addr |= (readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM2 + fz_offset));
		info = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_SRAM3 + fz_offset);

		valid = payload & BIT(0);
		timeout = (payload & GENMASK(19, 16)) >> 16;

		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
		if (port) {
			port_name = port->port_name;
			master = (struct itmon_masterinfo *)
				itmon_get_masterinfo(itmon, port_name,
							axid >> port->shift_bits);
			if (master)
				master_name = master->master_name;
			else
				master_name = NOT_AVAILABLE_STR;
		} else {
			port_name = NOT_AVAILABLE_STR;
			master_name = NOT_AVAILABLE_STR;
		}
		pr_err("      > %03d|%8s|%8s|%8u|%8x|%08x|%010zx|%08x|\n",
			i, port_name, master_name, valid, timeout, axid, addr, info);
	}
	pr_err("--------------------------------------------------------------------------\n");
}

static unsigned int power(unsigned int param, unsigned int num)
{
	if (num == 0)
		return 1;
	return param * (power(param, num - 1));
}

static void itmon_report_traceinfo(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct itmon_nodegroup *group = NULL;
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	char temp_buf[SZ_128];
#endif

	if (!traceinfo->dirty)
		return;

	pr_auto(ASL3,
		"\n--------------------------------------------------------------------------\n"
		"      Transaction Information\n\n"
		"      > Master         : %s %s\n"
		"      > Target         : %s\n"
		"      > Target Address : 0x%lX %s\n"
		"      > Type           : %s\n"
		"      > Error code     : %s\n",
		traceinfo->port, traceinfo->master ? traceinfo->master : "",
		traceinfo->dest ? traceinfo->dest : NOT_AVAILABLE_STR,
		traceinfo->target_addr,
		(unsigned int)traceinfo->target_addr == INVALID_REMAPPING ?
		"(BAAW Remapped address)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[traceinfo->errcode]);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	snprintf(temp_buf, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
		traceinfo->port, traceinfo->master ? traceinfo->master : "",
		traceinfo->dest ? traceinfo->dest : NOT_AVAILABLE_STR,
		traceinfo->target_addr,
		traceinfo->target_addr == INVALID_REMAPPING ?
		"(by CP maybe)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[traceinfo->errcode]);
	secdbg_exin_set_busmon(temp_buf);
#endif

	if (node) {
		struct itmon_tracedata *tracedata = &node->tracedata;

		pr_auto(ASL3,
			"\n      > Size           : %u bytes x %u burst => %u bytes\n"
			"      > Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
			"      > Level          : %s\n"
			"      > Protection     : %s\n",
			power(2, BIT_AXSIZE(tracedata->ext_info_1)), BIT_AXLEN(tracedata->ext_info_1) + 1,
			power(2, BIT_AXSIZE(tracedata->ext_info_1)) * (BIT_AXLEN(tracedata->ext_info_1) + 1),
			BIT_AXBURST(tracedata->ext_info_2),
			(BIT_AXPROT(tracedata->ext_info_2) & 0x1) ? "Privileged access" : "Unprivileged access",
			(BIT_AXPROT(tracedata->ext_info_2) & 0x2) ? "Non-secure access" : "Secure access");

		group = node->group;
		pr_auto(ASL3,
			"\n      > Path Type      : %s\n"
			"--------------------------------------------------------------------------\n",
			itmon_pathtype[traceinfo->path_type == -1 ? group->bus_type : traceinfo->path_type]);

	} else {
		pr_err("\n--------------------------------------------------------------------------\n");
	}
}

static void itmon_report_pathinfo(struct itmon_dev *itmon,
				  struct itmon_nodeinfo *node,
				  unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *tracedata = &node->tracedata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	if (!traceinfo->path_dirty) {
		pr_auto(ASL3,
			"\n--------------------------------------------------------------------------\n"
			"      ITMON Report (%s)\n"
			"--------------------------------------------------------------------------\n"
			"      PATH Information\n\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE");
		traceinfo->path_dirty = true;
	}
	switch (node->type) {
	case M_NODE:
		pr_auto(ASL3,
			"\n      > %14s, %8s(0x%08X)\n",
			node->name, "M_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_S_NODE:
		pr_auto(ASL3,
			"\n      > %14s, %8s(0x%08X)\n",
			node->name, "T_S_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_M_NODE:
		pr_auto(ASL3,
			"\n      > %14s, %8s(0x%08X)\n",
			node->name, "T_M_NODE", node->phy_regs + tracedata->offset);
		break;
	case S_NODE:
		pr_auto(ASL3,
			"\n      > %14s, %8s(0x%08X)\n",
			node->name, "S_NODE", node->phy_regs + tracedata->offset);
		break;
	}
}

static void itmon_report_tracedata(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node,
				   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *tracedata = &node->tracedata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct itmon_nodegroup *group = NULL;
	struct itmon_masterinfo *master;
	struct itmon_rpathinfo *port;
	unsigned int errcode, axid;
	unsigned int userbit;
	//bool port_find;

	errcode = BIT_ERR_CODE(tracedata->int_info);
	axid = (unsigned int)BIT_AXID(tracedata->int_info);
	userbit = BIT_AXUSER(tracedata->ext_info_2);

	switch (node->type) {
	case M_NODE:
		/*
		 * In this case, we can get information from M_NODE
		 * Fill traceinfo->port / target_addr / read / master
		 */
		if (BIT_ERR_VALID(tracedata->int_info) && tracedata->ext_info_2) {
			/* If only detecting M_NODE only */
			traceinfo->port = node->name;
			master = (struct itmon_masterinfo *)
				itmon_get_masterinfo(itmon, node->name, userbit);
			if (master)
				traceinfo->master = master->master_name;
			else
				traceinfo->master = NULL;

			traceinfo->target_addr = (((unsigned long)node->tracedata.ext_info_1
				& GENMASK(3, 0)) << 32ULL);
			traceinfo->target_addr |= node->tracedata.ext_info_0;
			traceinfo->read = tracedata->read;
			traceinfo->errcode = errcode;
			traceinfo->dirty = true;
		} else {
			if (!strncmp(node->name, "M_CPU", strlen(node->name)))
				set_bit(FROM_CPU, &traceinfo->from);
			else if (!strncmp(node->name, "M_PERI", strlen(node->name)))
				set_bit(FROM_IP, &traceinfo->from);
			else
				set_bit(FROM_OTHERS, &traceinfo->from);
		}
		/* Pure M_NODE and it doesn't have any information */
		if (!traceinfo->dirty) {
			traceinfo->master = NULL;
			traceinfo->target_addr = 0;
			traceinfo->read = tracedata->read;
			traceinfo->port = node->name;
			traceinfo->errcode = errcode;
			traceinfo->dirty = true;
		}
		itmon_report_pathinfo(itmon, node, trans_type);
		break;
	case S_NODE:
		group = node->group;
		if (group->bus_type == BUS_PERI) {
			if (test_bit(FROM_CPU, &traceinfo->from) && (axid & BIT(1)) == 0) {
			/*
				* This case is slave error
				* Master is CPU cluster
				* user & GENMASK(1, 0) = core number
			*/
				int cluster_num, core_num;

				core_num = userbit & GENMASK(1, 0);
				cluster_num = (userbit & BIT(2)) >> 2;
				snprintf(traceinfo->buf, SZ_32 - 1, "CPU%d Cluster%d", core_num, cluster_num);
				traceinfo->port = traceinfo->buf;
			} else if (test_bit(FROM_IP, &traceinfo->from) && (axid & BIT(0)) == 1) {
				snprintf(traceinfo->buf, SZ_32 - 1, "Other IPs(Refer NODE)");
				traceinfo->port = traceinfo->buf;
			}
		}

		/* If it has traceinfo->port, keep previous information */
		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);

		if (port)
			traceinfo->port = port->port_name;

		if (!traceinfo->master && traceinfo->port) {
			master = (struct itmon_masterinfo *)
					itmon_get_masterinfo(itmon, traceinfo->port,
						userbit);
			if (master)
				traceinfo->master = master->master_name;
		}

		/* Update targetinfo with S_NODE */
		traceinfo->target_addr =
			(((unsigned long)node->tracedata.ext_info_1
			& GENMASK(3, 0)) << 32ULL);
		traceinfo->target_addr |= node->tracedata.ext_info_0;
		traceinfo->errcode = errcode;
		traceinfo->dest = node->name;
		traceinfo->dirty = true;
		traceinfo->snode_dirty = true;
		itmon_report_pathinfo(itmon, node, trans_type);
		itmon_report_traceinfo(itmon, node, trans_type);
		break;
	case T_S_NODE:
	case T_M_NODE:
		itmon_report_pathinfo(itmon, node, trans_type);
		break;
	default:
		pr_err("Unknown Error - offset:%u\n", tracedata->offset);
		break;
	}
}

static void itmon_report_prot_chk_rawdata(struct itmon_dev *itmon,
				     struct itmon_nodeinfo *node)
{
	unsigned int dbg_mo_cnt, prot_chk_ctl, prot_chk_info, prot_chk_int_id;
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	char temp_buf[SZ_128];
#endif

	dbg_mo_cnt = __raw_readl(node->regs +  OFFSET_PROT_CHK);
	prot_chk_ctl = __raw_readl(node->regs +  OFFSET_PROT_CHK + REG_PROT_CHK_CTL);
	prot_chk_info = __raw_readl(node->regs +  OFFSET_PROT_CHK + REG_PROT_CHK_INT);
	prot_chk_int_id = __raw_readl(node->regs + OFFSET_PROT_CHK + REG_PROT_CHK_INT_ID);

	/* Output Raw register information */
	pr_err("\n--------------------------------------------------------------------------\n"
		"      Protocol Checker Raw Register Information(ITMON information)\n\n");
	pr_err("\n      > %s(%s, 0x%08X)\n"
		"      > REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_nodestring[node->type],
		node->phy_regs,
		dbg_mo_cnt,
		prot_chk_ctl,
		prot_chk_info,
		prot_chk_int_id);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	snprintf(temp_buf, SZ_128, "%s/ %s/ 0x%08X/ %s/ 0x%08X, 0x%08X, 0x%08X, 0x%08X",
		"Protocol Error", node->name, node->phy_regs,
		itmon_nodestring[node->type],
		dbg_mo_cnt,
		prot_chk_ctl,
		prot_chk_info,
		prot_chk_int_id);
	secdbg_exin_set_busmon(temp_buf);
#endif
}

static void itmon_report_rawdata(struct itmon_dev *itmon,
				 struct itmon_nodeinfo *node,
				 unsigned int trans_type)
{
	struct itmon_tracedata *tracedata = &node->tracedata;

	/* Output Raw register information */
	pr_err("\n      * %s(%s, 0x%08X)\n"
		"      > REG(0x08~0x18)        : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n"
		"      > REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_nodestring[node->type],
		node->phy_regs + tracedata->offset,
		tracedata->int_info,
		tracedata->ext_info_0,
		tracedata->ext_info_1,
		tracedata->ext_info_2,
		tracedata->dbg_mo_cnt,
		tracedata->prot_chk_ctl,
		tracedata->prot_chk_info,
		tracedata->prot_chk_int_id);
}

static void itmon_route_tracedata(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo;
	struct itmon_nodeinfo *node, *next_node;
	unsigned int trans_type;
	int i;

	/* To call function is sorted by declaration */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry(node, &pdata->tracelist[trans_type], list) {
				if (i == node->type)
					itmon_report_tracedata(itmon, node, trans_type);
			}
		}
		/* If there is no S_NODE information, check one more */
		traceinfo = &pdata->traceinfo[trans_type];
		if (!traceinfo->snode_dirty)
			itmon_report_traceinfo(itmon, NULL, trans_type);
	}

	if (pdata->traceinfo[TRANS_TYPE_READ].dirty ||
		pdata->traceinfo[TRANS_TYPE_WRITE].dirty)
		pr_auto(ASL3,
			"\n      Raw Register Information(ITMON Internal Information)\n\n");

	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry_safe(node, next_node, &pdata->tracelist[trans_type], list) {
				if (i == node->type) {
					itmon_report_rawdata(itmon, node, trans_type);
					/* clean up */
					list_del(&node->list);
					kfree(node);
				}
			}
		}
	}

	if (pdata->traceinfo[TRANS_TYPE_READ].dirty ||
		pdata->traceinfo[TRANS_TYPE_WRITE].dirty)
		pr_err("\n--------------------------------------------------------------------------\n");

	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		itmon_post_handler_to_notifier(itmon, trans_type);
		itmon_post_handler_by_dest(itmon, trans_type);
		itmon_post_handler_by_master(itmon, trans_type);
	}
}

static void itmon_trace_data(struct itmon_dev *itmon,
			    struct itmon_nodegroup *group,
			    struct itmon_nodeinfo *node,
			    unsigned int offset)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *new_node = NULL;
	unsigned int int_info, info0, info1, info2;
	unsigned int prot_chk_ctl, prot_chk_info, prot_chk_int_id, dbg_mo_cnt;
	bool read = TRANS_TYPE_WRITE;
	bool req = false;

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);

	dbg_mo_cnt = __raw_readl(node->regs +  OFFSET_PROT_CHK);
	prot_chk_ctl = __raw_readl(node->regs +  OFFSET_PROT_CHK + REG_PROT_CHK_CTL);
	prot_chk_info = __raw_readl(node->regs +  OFFSET_PROT_CHK + REG_PROT_CHK_INT);
	prot_chk_int_id = __raw_readl(node->regs + OFFSET_PROT_CHK + REG_PROT_CHK_INT_ID);

	switch (offset) {
	case OFFSET_REQ_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		/* Only S-Node is able to make log to registers */
		break;
	case OFFSET_RESP_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		/* Only NOT S-Node is able to make log to registers */
		break;
	default:
		pr_auto(ASL3,
			"Unknown Error - node:%s offset:%u\n", node->name, offset);
		break;
	}

	new_node = kmalloc(sizeof(struct itmon_nodeinfo), GFP_ATOMIC);
	if (new_node) {
		/* Fill detected node information to tracedata's list */
		memcpy(new_node, node, sizeof(struct itmon_nodeinfo));
		new_node->tracedata.int_info = int_info;
		new_node->tracedata.ext_info_0 = info0;
		new_node->tracedata.ext_info_1 = info1;
		new_node->tracedata.ext_info_2 = info2;
		new_node->tracedata.dbg_mo_cnt = dbg_mo_cnt;
		new_node->tracedata.prot_chk_ctl = prot_chk_ctl;
		new_node->tracedata.prot_chk_info = prot_chk_info;
		new_node->tracedata.prot_chk_int_id = prot_chk_int_id;

		new_node->tracedata.offset = offset;
		new_node->tracedata.read = read;
		new_node->group = group;
		if (BIT_ERR_VALID(int_info))
			node->tracedata.logging = true;
		else
			node->tracedata.logging = false;

		list_add(&new_node->list, &pdata->tracelist[read]);
	} else {
		pr_auto(ASL3,
			"failed to kmalloc for %s node %x offset\n",
			node->name, offset);
	}
}

static int itmon_search_node(struct itmon_dev *itmon, struct itmon_nodegroup *group, bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	unsigned int val, offset, freeze;
	unsigned long vec, flags, bit = 0;
	int i, j, ret = 0;

	spin_lock_irqsave(&itmon->ctrl_lock, flags);
	memset(pdata->traceinfo, 0, sizeof(struct itmon_traceinfo) * 2);
	ret = 0;

	if (group) {
		/* Processing only this group and select detected node */
		if (group->phy_regs) {
			if (group->ex_table)
				vec = (unsigned long)__raw_readq(group->regs);
			else
				vec = (unsigned long)__raw_readl(group->regs);
		} else {
			vec = GENMASK(group->nodesize, 0);
		}

		if (!vec)
			goto exit;

		node = group->nodeinfo;
		for_each_set_bit(bit, &vec, group->nodesize) {
			/* exist array */
			for (i = 0; i < OFFSET_NUM; i++) {
				offset = i * OFFSET_ERR_REPT;
				/* Check Request information */
				val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
				if (BIT_ERR_OCCURRED(val)) {
					/* This node occurs the error */
					itmon_trace_data(itmon, group, &node[bit], offset);
					if (clear)
						__raw_writel(1, node[bit].regs
								+ offset + REG_INT_CLR);
					ret = true;
				}
			}
			/* Check H/W assertion */
			if (node[bit].hw_assert_enabled) {
				val = __raw_readl(node[bit].regs + OFFSET_PROT_CHK +
							REG_PROT_CHK_INT);
				/* if timeout_freeze is enable,
				 * PROT_CHK interrupt is able to assert without any information */
				if (BIT_PROT_CHK_ERR_OCCURRED(val) && (val & GENMASK(31, 1))) {
					itmon_report_prot_chk_rawdata(itmon, &node[bit]);
					pdata->err_fatal = true;
					ret = true;
				}
			}
			/* Check freeze enable node */
			if (node[bit].type == S_NODE && node[bit].tmout_frz_enabled) {
				val = __raw_readl(node[bit].regs + OFFSET_TMOUT_REG  +
							REG_TMOUT_BUF_STATUS);
				freeze = val & (unsigned int)GENMASK(1, 0);
				if (freeze) {
					if (freeze & BIT(0))
						itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_WRITE);
					if (freeze & BIT(1))
						itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_READ);
					pdata->err_fatal = true;
					ret = true;
				}
			}
		}
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
			group = &nodegroup[i];
			if (group->phy_regs) {
				if (group->ex_table)
					vec = (unsigned long)__raw_readq(group->regs);
			else
					vec = (unsigned long)__raw_readl(group->regs);
			} else {
				vec = GENMASK(group->nodesize, 0);
			}

			node = group->nodeinfo;
			bit = 0;

			for_each_set_bit(bit, &vec, group->nodesize) {
				for (j = 0; j < OFFSET_NUM; j++) {
					offset = j * OFFSET_ERR_REPT;
					/* Check Request information */
					val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
					if (BIT_ERR_OCCURRED(val)) {
						/* This node occurs the error */
						itmon_trace_data(itmon, group, &node[bit], offset);
						if (clear)
							__raw_writel(1, node[bit].regs
									+ offset + REG_INT_CLR);
						ret = true;
					}
				}
				/* Check H/W assertion */
				if (node[bit].hw_assert_enabled) {
					val = __raw_readl(node[bit].regs + OFFSET_PROT_CHK +
								REG_PROT_CHK_INT);
					/* if timeout_freeze is enable,
					 * PROT_CHK interrupt is able to assert without any information */
					if (BIT_PROT_CHK_ERR_OCCURRED(val) && (val & GENMASK(31, 1))) {
						itmon_report_prot_chk_rawdata(itmon, &node[bit]);
						pdata->err_fatal = true;
						ret = true;
					}
				}
				/* Check freeze enable node */
				if (node[bit].type == S_NODE && node[bit].tmout_frz_enabled) {
					val = __raw_readl(node[bit].regs + OFFSET_TMOUT_REG  +
								REG_TMOUT_BUF_STATUS);
					freeze = val & (unsigned int)(GENMASK(1, 0));
					if (freeze) {
						if (freeze & BIT(0))
							itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_WRITE);
						if (freeze & BIT(1))
							itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_READ);
						pdata->err_fatal = true;
						ret = true;
					}
				}
			}
		}
	}
	itmon_route_tracedata(itmon);
 exit:
	spin_unlock_irqrestore(&itmon->ctrl_lock, flags);
	return ret;
}

static void itmon_do_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int policy;

	if (pdata->err_fatal) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_FATAL);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_fatal = false;
	}

	if (pdata->err_drex_tmout) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_DREX_TMOUT);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_drex_tmout = false;
	}

	if (pdata->err_ip) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_IP);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_ip = false;
	}

	if (pdata->err_chub) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CHUB);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_chub = false;
	}

	if (pdata->err_cp) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CP);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_cp = false;
	}

	if (pdata->err_cpu) {
		policy = dbg_snapshot_get_dpm_item_policy(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CPU);
		if (policy != GO_DEFAULT_ID)
			dbg_snapshot_soc_do_dpm_policy(policy);
		pdata->err_cpu = false;
	}
}

static irqreturn_t itmon_irq_handler(int irq, void *data)
{
	struct itmon_dev *itmon = (struct itmon_dev *)data;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = NULL;
	bool ret;
	int i;

	/* Search itmon group */
	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		if (irq == nodegroup[i].irq) {
			group = &pdata->nodegroup[i];
			pr_err("\n%s: %d irq, %s group, 0x%x vec",
				__func__, irq, group->name,
				group->phy_regs == 0 ? 0 : __raw_readl(group->regs));
			break;
		}
	}

	ret = itmon_search_node(itmon, NULL, true);
	if (!ret) {
		pr_err("ITMON could not detect any error\n");
	} else {
		pr_err("\nITMON Detected: err_fatal:%d, err_drex_tmout:%d, err_cpu:%d, err_cnt:%u, err_cnt_by_cpu:%u\n",
			pdata->err_fatal,
			pdata->err_drex_tmout,
			pdata->err_cpu,
			pdata->err_cnt,
			pdata->err_cnt_by_cpu);

		itmon_do_dpm_policy(itmon);
	}

	return IRQ_HANDLED;
}

void itmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itmon_notifier_list, block);
}

static struct bus_type itmon_subsys = {
	.name = "itmon",
	.dev_name = "itmon",
};

static ssize_t itmon_timeout_fix_val_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;

	n = scnprintf(buf + n, 24, "set timeout val: 0x%x\n", pdata->sysfs_tmout_val);

	return n;
}

static ssize_t itmon_timeout_fix_val_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;
	int ret;

	ret = kstrtoul(buf, 16, &val);
	if (!ret) {
		if (val > 0UL && val <= 0xFFFFFUL)
			pdata->sysfs_tmout_val = (unsigned int)val;
	} else {
		pr_err("%s: kstrtoul return value is %d\n", __func__, ret);
	}

	return count;
}

static ssize_t itmon_timeout_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout : %x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_DBG_CTL));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_val_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout : 0x%x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_TMOUT_INIT_VAL));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_freeze_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	unsigned long i, offset;
	ssize_t n = 0;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	/* Processing all group & nodes */
	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE) {
				n += scnprintf(buf + n, 60, "%-12s : 0x%08X, timeout_freeze : %x\n",
					node[bit].name, node[bit].phy_regs,
					__raw_readl(node[bit].regs + offset + REG_TMOUT_FRZ_EN));
			}
		}
	}
	return n;
}

static ssize_t itmon_timeout_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int val, offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				val = __raw_readl(node[bit].regs + offset + REG_DBG_CTL);
				if (!val)
					val = 1;
				else
					val = 0;
				__raw_writel(val, node[bit].regs + offset + REG_DBG_CTL);
				node[bit].tmout_enabled = val;
			}
		}
	}
	kfree(name);
	return count;
}

static ssize_t itmon_timeout_val_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;
	struct itmon_platdata *pdata = g_itmon->pdata;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				__raw_writel(pdata->sysfs_tmout_val,
						node[bit].regs + offset + REG_TMOUT_INIT_VAL);
				node[bit].time_val = pdata->sysfs_tmout_val;
			}
		}
	}
	kfree(name);
	return count;
}

static ssize_t itmon_timeout_freeze_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *name;
	unsigned int val, offset, i;
	unsigned long vec, bit = 0;
	struct itmon_nodegroup *group = NULL;
	struct itmon_nodeinfo *node;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	offset = OFFSET_TMOUT_REG;
	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		group = &nodegroup[i];
		node = group->nodeinfo;
		vec = GENMASK(group->nodesize, 0);
		bit = 0;
		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE &&
				!strncmp(name, node[bit].name, strlen(name))) {
				val = __raw_readl(node[bit].regs + offset + REG_TMOUT_FRZ_EN);
				if (!val)
					val = 1;
				else
					val = 0;
				__raw_writel(val, node[bit].regs + offset + REG_TMOUT_FRZ_EN);
				node[bit].tmout_frz_enabled = val;
			}
		}
	}
	kfree(name);
	return count;
}

static struct device_attribute itmon_timeout_attr =
	__ATTR(timeout_en, 0644, itmon_timeout_show, itmon_timeout_store);
static struct device_attribute itmon_timeout_fix_attr =
	__ATTR(set_val, 0644, itmon_timeout_fix_val_show, itmon_timeout_fix_val_store);
static struct device_attribute itmon_timeout_val_attr =
	__ATTR(timeout_val, 0644, itmon_timeout_val_show, itmon_timeout_val_store);
static struct device_attribute itmon_timeout_freeze_attr =
	__ATTR(timeout_freeze, 0644, itmon_timeout_freeze_show, itmon_timeout_freeze_store);

static struct attribute *itmon_sysfs_attrs[] = {
	&itmon_timeout_attr.attr,
	&itmon_timeout_fix_attr.attr,
	&itmon_timeout_val_attr.attr,
	&itmon_timeout_freeze_attr.attr,
	NULL,
};

static struct attribute_group itmon_sysfs_group = {
	.attrs = itmon_sysfs_attrs,
};

static const struct attribute_group *itmon_sysfs_groups[] = {
	&itmon_sysfs_group,
	NULL,
};

static int __init itmon_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&itmon_subsys, itmon_sysfs_groups);
	if (ret)
		dev_err(g_itmon->dev, "fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(itmon_sysfs_init);

static int itmon_logging_panic_handler(struct notifier_block *nb,
				     unsigned long l, void *buf)
{
	struct itmon_panic_block *itmon_panic = (struct itmon_panic_block *)nb;
	struct itmon_dev *itmon = itmon_panic->pdev;
	struct itmon_platdata *pdata = itmon->pdata;
	int ret;

	if (!IS_ERR_OR_NULL(itmon)) {
		/* Check error has been logged */
		ret = itmon_search_node(itmon, NULL, false);
		if (!ret) {
			dev_info(itmon->dev, "No found error in %s\n", __func__);
		} else {
			pr_err("\nITMON Detected: err_fatal:%d, err_drex_tmout:%d, err_cpu:%d, err_cnt:%u, err_cnt_by_cpu:%u\n",
				pdata->err_fatal,
				pdata->err_drex_tmout,
				pdata->err_cpu,
				pdata->err_cnt,
				pdata->err_cnt_by_cpu);

			itmon_do_dpm_policy(itmon);
		}
	}
	return 0;
}

static int itmon_probe(struct platform_device *pdev)
{
	struct itmon_dev *itmon;
	struct itmon_panic_block *itmon_panic = NULL;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int irq_option = 0, irq;
	char *dev_name;
	int ret, i, j;

	itmon = devm_kzalloc(&pdev->dev, sizeof(struct itmon_dev), GFP_KERNEL);
	if (!itmon)
		return -ENOMEM;

	itmon->dev = &pdev->dev;

	spin_lock_init(&itmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct itmon_platdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	itmon->pdata = pdata;
	itmon->pdata->masterinfo = masterinfo;
	itmon->pdata->rpathinfo = rpathinfo;
	itmon->pdata->nodegroup = nodegroup;

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		dev_name = nodegroup[i].name;
		node = nodegroup[i].nodeinfo;

		if (nodegroup[i].phy_regs) {
			nodegroup[i].regs = devm_ioremap_nocache(&pdev->dev,
							 nodegroup[i].phy_regs, SZ_16K);
			if (nodegroup[i].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}
#ifdef MULTI_IRQ_SUPPORT_ITMON
			irq_option = IRQF_GIC_MULTI_TARGET;
#endif
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		nodegroup[i].irq = irq;

		ret = devm_request_irq(&pdev->dev, irq,
				       itmon_irq_handler, irq_option, dev_name, itmon);
		if (ret == 0) {
			dev_info(&pdev->dev, "success to register request irq%u - %s\n", irq, dev_name);
		} else {
			dev_err(&pdev->dev, "failed to request irq - %s\n", dev_name);
			return -ENOENT;
		}

		for (j = 0; j < nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev, node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
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
	dev_set_socdata(&pdev->dev, "Exynos", "ITMON");

	for (i = 0; i < TRANS_TYPE_NUM; i++)
		INIT_LIST_HEAD(&pdata->tracelist[i]);

	pdata->cp_crash_in_progress = false;

	itmon_init(itmon, true);

	g_itmon = itmon;
	pdata->probed = true;

	dev_info(&pdev->dev, "success to probe Exynos ITMON driver\n");

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
	return 0;
}

static int itmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;

	/* re-enable ITMON if cp-crash progress is not starting */
	if (!pdata->cp_crash_in_progress)
		itmon_init(itmon, true);

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
		   .name = "exynos-itmon",
		   .of_match_table = itmon_dt_match,
		   .pm = &itmon_pm_ops,
		   },
};

module_platform_driver(exynos_itmon_driver);

MODULE_DESCRIPTION("Samsung Exynos ITMON DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itmon");
