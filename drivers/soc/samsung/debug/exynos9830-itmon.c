/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos9830 SOC
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
#include <soc/samsung/exynos-pd.h>
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

#define REG_TMOUT_BUF_USER_0		(0x2C)
#define REG_TMOUT_BUF_USER_1		(0x30)

#define REG_TMOUT_BUF_PAYLOAD_1		(0x30)
#define REG_TMOUT_BUF_PAYLOAD_2		(0x34)
#define REG_TMOUT_BUF_PAYLOAD_3		(0x38)

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
#define BIT_PROT_CHK_ERR_CODE(x)	(((x) & (0x7 << 1)) >> 1)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFF << 16)) >> 16)
#define BIT_AXBURST(x)			(((x) & (0x3)))
#define BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

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

#define FROM_CP				(0)
#define FROM_CPU			(2)
#define FROM_M_NODE			(4)

#define CP_COMMON_STR			"CP_"
#define DREX_COMMON_STR			"DREX_IRPS"
#define NOT_AVAILABLE_STR		"N/A"

#define TMOUT				(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define PANIC_GO_THRESHOLD		(100)
#define INVALID_REMAPPING		(0x03000000)

/* This value will be fixed */
#define INTEND_ADDR_START		(0)
#define INTEND_ADDR_END			(0)

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
	bool onoff;
	char *pd_name;
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
	bool addr_detect_enabled;
	char *pd_name;
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
	/* 0x8000_0000 - 0xf_ffff_ffff */

	{0,	"AUD",		"DREX_IRPS",	0x3F, 0},
	{1,	"HSI1",		"DREX_IRPS",	0x3F, 0},
	{2,	"DNS",		"DREX_IRPS",	0x3F, 0},
	{3,	"CSIS0",	"DREX_IRPS",	0x3F, 0},
	{4,	"CSIS1",	"DREX_IRPS",	0x3F, 0},
	{5,	"IPP",		"DREX_IRPS",	0x3F, 0},
	{6,	"TNR0",		"DREX_IRPS",	0x3F, 0},
	{7,	"TNR1",		"DREX_IRPS",	0x3F, 0},
	{8,	"MCSC0",	"DREX_IRPS",	0x3F, 0},
	{9,	"MCSC1",	"DREX_IRPS",	0x3F, 0},
	{10,	"VRA",		"DREX_IRPS",	0x3F, 0},
	{11,	"SSP",		"DREX_IRPS",	0x3F, 0},
	{12,	"AUD",		"DREX_IRPS",	0x3F, 0},
	{13,	"HSI1",		"DREX_IRPS",	0x3F, 0},
	{14,	"DNS",		"DREX_IRPS",	0x3F, 0},
	{15,	"CSIS0",	"DREX_IRPS",	0x3F, 0},
	{16,	"CSIS1",	"DREX_IRPS",	0x3F, 0},
	{17,	"IPP",		"DREX_IRPS",	0x3F, 0},
	{18,	"TNR0",		"DREX_IRPS",	0x3F, 0},
	{19,	"TNR1",		"DREX_IRPS",	0x3F, 0},
	{20,	"MCSC0",	"DREX_IRPS",	0x3F, 0},
	{21,	"MCSC1",	"DREX_IRPS",	0x3F, 0},
	{22,	"VRA",		"DREX_IRPS",	0x3F, 0},
	{23,	"SSP",		"DREX_IRPS",	0x3F, 0},
	{24,	"DPU0",		"DREX_IRPS",	0x3F, 0},
	{25,	"DPU1",		"DREX_IRPS",	0x3F, 0},
	{26,	"DPU2",		"DREX_IRPS",	0x3F, 0},
	{27,	"DIT",		"DREX_IRPS",	0x3F, 0},
	{28,	"SBIC",		"DREX_IRPS",	0x3F, 0},
	{29,	"AVPS",		"DREX_IRPS",	0x3F, 0},
	{30,	"SECU",		"DREX_IRPS",	0x3F, 0},
	{31,	"HSI0",		"DREX_IRPS",	0x3F, 0},
	{32,	"HSI2",		"DREX_IRPS",	0x3F, 0},
	{33,	"DNC0",		"DREX_IRPS",	0x3F, 0},
	{34,	"DNC1",		"DREX_IRPS",	0x3F, 0},
	{35,	"DNC2",		"DREX_IRPS",	0x3F, 0},
	{36,	"DNC3",		"DREX_IRPS",	0x3F, 0},
	{37,	"DNC4",		"DREX_IRPS",	0x3F, 0},
	{38,	"G2D0",		"DREX_IRPS",	0x3F, 0},
	{39,	"G2D1",		"DREX_IRPS",	0x3F, 0},
	{40,	"G2D2",		"DREX_IRPS",	0x3F, 0},
	{41,	"MFC00",	"DREX_IRPS",	0x3F, 0},
	{42,	"MFC01",	"DREX_IRPS",	0x3F, 0},
	{43,	"DPU0",		"DREX_IRPS",	0x3F, 0},
	{44,	"DPU1",		"DREX_IRPS",	0x3F, 0},
	{45,	"DPU2",		"DREX_IRPS",	0x3F, 0},
	{46,	"DIT",		"DREX_IRPS",	0x3F, 0},
	{47,	"SBIC",		"DREX_IRPS",	0x3F, 0},
	{48,	"AVPS",		"DREX_IRPS",	0x3F, 0},
	{49,	"SECU",		"DREX_IRPS",	0x3F, 0},
	{50,	"HSI0",		"DREX_IRPS",	0x3F, 0},
	{51,	"HSI2",		"DREX_IRPS",	0x3F, 0},
	{52,	"DNC0",		"DREX_IRPS",	0x3F, 0},
	{53,	"DNC1",		"DREX_IRPS",	0x3F, 0},
	{54,	"DNC2",		"DREX_IRPS",	0x3F, 0},
	{55,	"DNC3",		"DREX_IRPS",	0x3F, 0},
	{56,	"DNC4",		"DREX_IRPS",	0x3F, 0},
	{57,	"G2D0",		"DREX_IRPS",	0x3F, 0},
	{58,	"G2D1",		"DREX_IRPS",	0x3F, 0},
	{59,	"G2D2",		"DREX_IRPS",	0x3F, 0},
	{60,	"MFC00",	"DREX_IRPS",	0x3F, 0},
	{61,	"MFC01",	"DREX_IRPS",	0x3F, 0},
	{62,	"CORESIGHT",	"DREX_IRPS",	0x3F, 0},
	{63,	"MMUG3D",	"DREX_IRPS",	0x3F, 0},

	/* 0x0000_0000 - 0x7fff_ffff */

	{0,	"AUD",		"BUS0_DP",	0xF, 0},
	{1,	"HSI1",		"BUS0_DP",	0xF, 0},
	{2,	"DNS",		"BUS0_DP",	0xF, 0},
	{3,	"CSIS0",	"BUS0_DP",	0xF, 0},
	{4,	"CSIS1",	"BUS0_DP",	0xF, 0},
	{5,	"IPP",		"BUS0_DP",	0xF, 0},
	{6,	"TNR0",		"BUS0_DP",	0xF, 0},
	{7,	"TNR1",		"BUS0_DP",	0xF, 0},
	{8,	"MCSC0",	"BUS0_DP",	0xF, 0},
	{9,	"MCSC1",	"BUS0_DP",	0xF, 0},
	{10,	"VRA",		"BUS0_DP",	0xF, 0},
	{11,	"SSP",		"BUS0_DP",	0xF, 0},

	{0,	"DPU0",		"BUS1_DP",	0x1F, 0},
	{1,	"DPU1",		"BUS1_DP",	0x1F, 0},
	{2,	"DPU2",		"BUS1_DP",	0x1F, 0},
	{3,	"DIT",		"BUS1_DP",	0x1F, 0},
	{4,	"SBIC",		"BUS1_DP",	0x1F, 0},
	{5,	"AVPS",		"BUS1_DP",	0x1F, 0},
	{6,	"SECU",		"BUS1_DP",	0x1F, 0},
	{7,	"HSI0",		"BUS1_DP",	0x1F, 0},
	{8,	"HSI2",		"BUS1_DP",	0x1F, 0},
	{9,	"DNC0",		"BUS1_DP",	0x1F, 0},
	{10,	"DNC1",		"BUS1_DP",	0x1F, 0},
	{11,	"DNC2",		"BUS1_DP",	0x1F, 0},
	{12,	"DNC3",		"BUS1_DP",	0x1F, 0},
	{13,	"DNC4",		"BUS1_DP",	0x1F, 0},
	{14,	"G2D0",		"BUS1_DP",	0x1F, 0},
	{15,	"G2D1",		"BUS1_DP",	0x1F, 0},
	{16,	"G2D2",		"BUS1_DP",	0x1F, 0},
	{17,	"MFC00",	"BUS1_DP",	0x1F, 0},
	{18,	"MFC01",	"BUS1_DP",	0x1F, 0},
};

const static struct itmon_masterinfo masterinfo[] = {
	{"CSIS0",	0x0,	/*XXX000*/	"CSIS_DMA1",			0x7},
	{"CSIS0",	0x4,	/*XXX100*/	"ZSL",				0x7},
	{"CSIS0",	0x2,	/*XXXX10*/	"SYSMMU_D0_CSIS",		0x3},
	{"CSIS0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_CSIS_S2MPU",		0x1},
	{"CSIS1",	0x0,	/*XX0000*/	"CSIS_DMA0",			0xF},
	{"CSIS1",	0x4,	/*XX0100*/	"STRP",				0xF},
	{"CSIS1",	0x8,	/*XX1000*/	"PDP_STAT",			0xF},
	{"CSIS1",	0x2,	/*XXXX10*/	"SYSMMU_D1_CSIS",		0x3},
	{"CSIS1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_CSIS_S2MPU",		0x1},
	{"IPP",		0x0,	/*XXXX00*/	"SIPU_IPP",			0x3},
	{"IPP",		0x2,	/*XXXX10*/	"SYSMMU_IPP",			0x3},
	{"IPP",		0x1,	/*XXXXX1*/	"SYSMMU_IPP_S2MPU",		0x1},
	{"TNR0",	0x0,	/*XX0000*/	"TNR_D",			0xF},
	{"TNR0",	0x4,	/*XX0100*/	"TNR_W",			0xF},
	{"TNR0",	0x8,	/*XX1000*/	"TNR_WRAP",			0xF},
	{"TNR0",	0xC,	/*XX1100*/	"TNR",				0xF},
	{"TNR0",	0x2,	/*XXXX10*/	"SYSMMU_D0_TNR",		0x3},
	{"TNR0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_TNR_S2MPU",		0x1},
	{"TNR1",	0x0,	/*XX0000*/	"ORBMCH0",			0xF},
	{"TNR1",	0x4,	/*XX0100*/	"ORBMCH1",			0xF},
	{"TNR1",	0x8,	/*XX1000*/	"ORBMCH2",			0xF},
	{"TNR1",	0xC,	/*XX1100*/	"TNR_B",			0xF},
	{"TNR1",	0x2,	/*XXXX10*/	"SYSMMU_D1_TNR",		0x3},
	{"TNR1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_TNR_S2MPU",		0x1},
	{"DNS",		0x0,	/*XXXX00*/	"DNS",				0x3},
	{"DNS",		0x2,	/*XXXX10*/	"SYSMMU_DNS",			0x3},
	{"DNS",		0x1,	/*XXXXX1*/	"SYSMMU_DNS_S2MPU",		0x1},
	{"MCSC0",	0x0,	/*XXX000*/	"MEIP",				0x7},
	{"MCSC0",	0x4,	/*XXX100*/	"GDC",				0x7},
	{"MCSC0",	0x2,	/*XXXX10*/	"SYSMMU_D0_MCSC",		0x3},
	{"MCSC0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_MCSC_S2MPU",		0x1},
	{"MCSC1",	0x0,	/*XXX000*/	"ITSC",				0x7},
	{"MCSC1",	0x4,	/*XXX100*/	"MCSC",				0x7},
	{"MCSC1",	0x2,	/*XXXX10*/	"SYSMMU_D1_MCSC",		0x3},
	{"MCSC1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_MCSC_S2MPU",		0x1},
	{"VRA",		0x0,	/*XXX000*/	"VRA",				0x7},
	{"VRA",		0x4,	/*XXX100*/	"STR",				0x7},
	{"VRA",		0x2,	/*XXXX10*/	"SYSMMU_VRA",			0x3},
	{"VRA",		0x1,	/*XXXXX1*/	"SYSMMU_VRA_S2MPU",		0x1},
	{"HSI1",	0x0,	/*XX0000*/	"MMC_CARD",			0xF},
	{"HSI1",	0x2,	/*XX0010*/	"PCIE_GEN2",			0xF},
	{"HSI1",	0x4,	/*XX0100*/	"PCIE_GEN4B",			0xF},
	{"HSI1",	0x6,	/*XX0110*/	"UFS_EMBD",			0xF},
	{"HSI1",	0x8,	/*XX1000*/	"UFS_CARD",			0xF},
	{"HSI1",	0x1,	/*XXXXX1*/	"SYSMMU_HSI1_S2MPU",		0x1},
	{"AUD",		0x0,	/*XX0000*/	"SPUS/SPUM",			0xF},
	{"AUD",		0x4,	/*XX0100*/	"CA7",				0xF},
	{"AUD",		0xC,	/*XX1100*/	"AUDEN",			0xF},
	{"AUD",		0x2,	/*XXXX10*/	"SYSMMU_ABOX",			0x3},
	{"AUD",		0x1,	/*XXXXX1*/	"SYSMMU_ABOX_S2MPU",		0x1},
	{"DPU0",	0x0,	/*XXXX00*/	"DPU",				0x3},
	{"DPU0",	0x2,	/*XXXX10*/	"SYSMMU_D0_DPU",		0x3},
	{"DPU0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_DPU_S2MPU",		0x1},
	{"DPU1",	0x0,	/*XXXX00*/	"DPU",				0x3},
	{"DPU1",	0x2,	/*XXXX10*/	"SYSMMU_D1_DPU",		0x3},
	{"DPU1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_DPU_S2MPU",		0x1},
	{"DPU2",	0x0,	/*XXXX00*/	"DPU",				0x3},
	{"DPU2",	0x2,	/*XXXX10*/	"SYSMMU_D2_DPU",		0x3},
	{"DPU2",	0x1,	/*XXXXX1*/	"SYSMMU_D2_DPU_S2MPU",		0x1},
	{"DNC0",	0x0,	/*XXXX00*/	"DSPM0",			0x3},
	{"DNC0",	0x2,	/*XXXX10*/	"SYSMMU_D0_DNC",		0x3},
	{"DNC0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_DNC_S2MPU",		0x1},
	{"DNC1",	0x0,	/*XXXX00*/	"DSPM1",			0x3},
	{"DNC1",	0x2,	/*XXXX10*/	"SYSMMU_D1_DNC",		0x3},
	{"DNC1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_DNC_S2MPU",		0x1},
	{"DNC2",	0x0,	/*X00000*/	"CA5",				0x1F},
	{"DNC2",	0x4,	/*000100*/	"DSP0.CACHE",			0x3F},
	{"DNC2",	0x24,	/*100100*/	"DSP0.DRAM",			0x3F},
	{"DNC2",	0x8,	/*001000*/	"DSP1.CACHE",			0x3F},
	{"DNC2",	0x28,	/*101000*/	"DSP1.DRAM",			0x3F},
	{"DNC2",	0x8,	/*001100*/	"DSP2.CACHE",			0x3F},
	{"DNC2",	0x2C,	/*101100*/	"DSP2.DRAM",			0x3F},
	{"DNC2",	0x10,	/*X10000*/	"IP_NPUC0_CMD",			0x1F},
	{"DNC2",	0x14,	/*X10100*/	"IP_NPUC1_CMD",			0x1F},
	{"DNC2",	0x2,	/*XXXX10*/	"SYSMMU_D2_DNC",		0x3},
	{"DNC2",	0x1,	/*XXXXX1*/	"SYSMMU_D2_DNC_S2MPU",		0x1},
	{"DNC3",	0x0,	/*XXXX00*/	"DSPM3",			0x3},
	{"DNC3",	0x2,	/*XXXX10*/	"SYSMMU_D3_DNC",		0x3},
	{"DNC3",	0x1,	/*XXXXX1*/	"SYSMMU_D3_DNC_S2MPU",		0x1},
	{"DNC4",	0x0,	/*XXXX00*/	"DSPM4",			0x3},
	{"DNC4",	0x2,	/*XXXX10*/	"SYSMMU_D4_DNC",		0x3},
	{"DNC4",	0x1,	/*XXXXX1*/	"SYSMMU_D4_DNC_S2MPU",		0x1},
	{"G2D0",	0x0,	/*XXXX00*/	"G2D",				0x3},
	{"G2D0",	0x2,	/*XXXX10*/	"SYSMMU_D0_G2D",		0x3},
	{"G2D0",	0x1,	/*XXXXX1*/	"SYSMMU_D0_G2D_S2MPU",		0x1},
	{"G2D1",	0x0,	/*XXXX00*/	"G2D",				0x3},
	{"G2D1",	0x2,	/*XXXX10*/	"SYSMMU_D1_G2D",		0x3},
	{"G2D1",	0x1,	/*XXXXX1*/	"SYSMMU_D1_G2D_S2MPU",		0x1},
	{"G2D2",	0x0,	/*XX0000*/	"JPEG",				0xF},
	{"G2D2",	0x4,	/*XX0100*/	"MSCL",				0xF},
	{"G2D2",	0x8,	/*XX1000*/	"ASTC",				0xF},
	{"G2D2",	0xC,	/*XX1100*/	"JSQZ",				0xF},
	{"G2D2",	0x2,	/*XXXX10*/	"SYSMMU_G2D2",			0x3},
	{"G2D2",	0x1,	/*XXXXX1*/	"SYSMMU_G2D2_S2MPU",		0x1},
	{"MFC00",	0x0,	/*XXXX00*/	"MFC",				0x3},
	{"MFC00",	0x2,	/*XXXX10*/	"SYSMMU_D0_MFC0",		0x3},
	{"MFC00",	0x1,	/*XXXXX1*/	"SYSMMU_D0_MFC0_S2MPU",		0x1},
	{"MFC01",	0x0,	/*XXX000*/	"MFC",				0x7},
	{"MFC01",	0x4,	/*XXX100*/	"WFD",				0x7},
	{"MFC01",	0x2,	/*XXXX10*/	"SYSMMU_D1_MFC0",		0x3},
	{"MFC01",	0x1,	/*XXXXX1*/	"SYSMMU_D1_MFC0_S2MPU",		0x1},
	{"HSI0",	0x0,	/*XXXXX0*/	"USB31DRD",			0x1},
	{"HSI0",	0x1,	/*XXXXX1*/	"SYSMMU_HSI0_S2MPU",		0x1},
	{"HSI2",	0x0,	/*XXXXX0*/	"PCIE_GEN4A",			0x1},
	{"HSI2",	0x1,	/*XXXXX1*/	"SYSMMU_HSI2_S2MPU",		0x1},
	{"DIT",		0x0,	/*XXXXX0*/	"DIT",				0x1},
	{"DIT",		0x1,	/*XXXXX1*/	"SYSMMU_DIT_S2MPU",		0x1},
	{"SBIC",	0x0,	/*XXXXX0*/	"SBIC",				0x1},
	{"SBIC",	0x1,	/*XXXXX1*/	"SYSMMU_SBIC_S2MPU",		0x1},
	{"AVPS",	0x0,	/*X00000*/	"ALIVE(APM)",			0x1F},
	{"AVPS",	0x8,	/*X01000*/	"ALIVE(Debug_Core)",		0x1F},
	{"AVPS",	0x10,	/*X10000*/	"ALIVE(PEM)",			0x1F},
	{"AVPS",	0x2,	/*XXX010*/	"VTS(CM4F_System)",		0x7},
	{"AVPS",	0x4,	/*XXX100*/	"PDMA",				0x7},
	{"AVPS",	0x6,	/*XXX110*/	"SPDMA",			0x7},
	{"AVPS",	0x1,	/*XXXXX1*/	"SYSMMU_AVPS_S2MPU",		0x1},
	{"SECU",	0x0,	/*XXXX00*/	"SSS",				0x3},
	{"SECU",	0x2,	/*XXXX10*/	"RTIC",				0x3},
	{"SECU",	0x1,	/*XXXXX1*/	"SYSMMU_AVPS_S2MPU",		0x1},
	{"SSP", 0, "SSP", 0},
	{"CORESIGHT", 0, "CORESIGHT", 0},
	{"MMUG3D", 0, "MMUG3D", 0},
};

static struct itmon_nodeinfo data_bus0[] = {
	{M_NODE,	"AUD",		0x1B0A3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"CSIS0",	0x1B003000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"CSIS1",	0x1B013000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNS",		0x1B053000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"HSI1",		0x1B093000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"IPP",		0x1B023000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"MCSC0",	0x1B063000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"MCSC1",	0x1B073000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SSP",		0x1B0B3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"TNR0",		0x1B033000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"TNR1",		0x1B043000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"VRA",		0x1B083000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"BUS0_DP",	0x1B103000, NULL, TMOUT, true, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_S0",	0x1B0C3000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_S1",	0x1B0D3000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_S2",	0x1B0E3000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_S3",	0x1B0F3000, NULL, 0,	false, false, true, true, false, NULL},
};

static struct itmon_nodeinfo data_bus1[] = {
	{M_NODE,	"AVPS",		0x1B513000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DIT",		0x1B4F3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNC0",		0x1B433000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNC1",		0x1B443000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNC2",		0x1B453000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNC3",		0x1B463000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DNC4",		0x1B473000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DPU0",		0x1B403000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DPU1",		0x1B413000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"DPU2",		0x1B423000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"G2D0",		0x1B483000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"G2D1",		0x1B493000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"G2D2",		0x1B4A3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"HSI0",		0x1B4D3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"HSI2",		0x1B4E3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"MFC00",	0x1B4B3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"MFC01",	0x1B4C3000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SBIC",		0x1B503000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SECU",		0x1B523000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"BUS1_DP",	0x1B573000, NULL, TMOUT, true, false, true, true, false, NULL},
	{T_S_NODE,	"BUS1_S0",	0x1B533000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS1_S1",	0x1B543000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS1_S2",	0x1B553000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS1_S3",	0x1B563000, NULL, 0,	false, false, true, true, false, NULL},
};

static struct itmon_nodeinfo data1_bus0[] = {
	{T_M_NODE,	"BUS0_M0",	0x1B203000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS0_M1",	0x1B213000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS0_M2",	0x1B223000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS0_M3",	0x1B233000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE0_M0",	0x1B243000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE0_M1",	0x1B253000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE0_M2",	0x1B263000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE0_M3",	0x1B273000, NULL, 0,	false, false, true, true, false, NULL},
};

static struct itmon_nodeinfo data1_bus1[] = {
	{T_M_NODE,	"BUS1_M0",	0x1B603000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS1_M1",	0x1B613000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS1_M2",	0x1B623000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"BUS1_M3",	0x1B633000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE1_M0",	0x1B643000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE1_M1",	0x1B653000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE1_M2",	0x1B663000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE1_M3",	0x1B673000, NULL, 0,	false, false, true, true, false, NULL},
};

static struct itmon_nodeinfo data_core[] = {
	{T_M_NODE,	"CORE0_M0",	0x1A803000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE0_M1",	0x1A813000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE0_M2",	0x1A823000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE0_M3",	0x1A833000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE1_M0",	0x1A843000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE1_M1",	0x1A853000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE1_M2",	0x1A863000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE1_M3",	0x1A873000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"CORESIGHT",	0x1A883000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"MMUG3D",	0x1A893000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"CORE_DP",	0x1A8C3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"DREX_IRPS0",	0x1A8A3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"DREX_IRPS1",	0x1A8B3000, NULL, TMOUT, true, false, true, true, false, NULL},
};

static struct itmon_nodeinfo peri0_core[] = {
	{M_NODE,	"BUS1_DP",	0x1AA63000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"CORE_DP",	0x1AA53000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SCI_CCM0",	0x1AA23000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SCI_CCM1",	0x1AA33000, NULL, 0,	false, false, true, true, false, NULL},
	{M_NODE,	"SCI_IRPM",	0x1AA43000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE_BUS0",	0x1AA13000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"CORE_P0P1",	0x1AA03000, NULL, 0,	false, false, true, true, false, NULL},
};

static struct itmon_nodeinfo peri1_core[] = {
	{T_M_NODE,	"BUS0_CORE",	0x1AC13000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE_P0P1",	0x1AC03000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"ALIVE",	0x1AC73000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"CPUCL0",	0x1AC43000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"CPUCL1",	0x1AC53000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"G3D",		0x1AC63000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"SFR_CORE",	0x1AC33000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"TREX_DP_CORE",	0x1AC23000, NULL, TMOUT, true, false, true, true, false, NULL},
};

static struct itmon_nodeinfo peri_bus0[] = {
	{M_NODE,	"BUS0_DP",	0x1B933000, NULL, 0,	false, false, true, true, false, NULL},
	{T_M_NODE,	"CORE_BUS0",	0x1B943000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"AUD",		0x1B903000, NULL, TMOUT, true, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_BUS1",	0x1B833000, NULL, 0,	false, false, true, true, false, NULL},
	{T_S_NODE,	"BUS0_CORE",	0x1B823000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"CSIS",		0x1B8A3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"HSI1",		0x1B8F3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"IPP",		0x1B8B3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"ITP",		0x1B8D3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MCSC",		0x1B8E3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MIF0",		0x1B843000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MIF1",		0x1B853000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MIF2",		0x1B863000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MIF3",		0x1B873000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"PERIC1",	0x1B893000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"PERIS",	0x1B883000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"SFR_BUS0",	0x1B813000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"SSP",		0x1B923000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"TNR",		0x1B8C3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"TREX_DP_BUS0",	0x1B803000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"VRA",		0x1B913000, NULL, TMOUT, true, false, true, true, false, NULL},
};

static struct itmon_nodeinfo peri_bus1[] = {
	{T_M_NODE,	"BUS0_BUS1",	0x1BAF3000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"DNC",		0x1BA53000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"DPU",		0x1BAB3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"G2D",		0x1BA33000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"HSI0",		0x1BAC3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"HSI2",		0x1BAD3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"MFC0",		0x1BA43000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"NPU00",	0x1BA63000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"NPU01",	0x1BA73000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"NPU10",	0x1BA83000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"NPU11",	0x1BA93000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"PERIC0",	0x1BAE3000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"SFR0_BUS1",	0x1BA13000, NULL, TMOUT, true, false, true, true, false, NULL},
	{S_NODE,	"SFR1_BUS1",	0x1BA23000, NULL, TMOUT, true, false, true, true, false, NULL},
	{M_NODE,	"TREX_DP_BUS1",	0x1BA03000, NULL, 0,	false, false, true, true, false, NULL},
	{S_NODE,	"VTS",		0x1BAA3000, NULL, TMOUT, true, false, true, true, false, NULL},
};

static struct itmon_nodegroup nodegroup[] = {
	{58,	"DATA_BUS0",	0x1B123000, false,	NULL, data_bus0,	ARRAY_SIZE(data_bus0),	BUS_DATA},
	{79,	"DATA_BUS1",	0x1B593000, false,	NULL, data_bus1,	ARRAY_SIZE(data_bus1),	BUS_DATA},
	{59,	"DATA1_BUS0",	0x1B293000, false,	NULL, data1_bus0,	ARRAY_SIZE(data1_bus0),	BUS_DATA},
	{80,	"DATA1_BUS1",	0x1B693000, false,	NULL, data1_bus1,	ARRAY_SIZE(data1_bus1),	BUS_DATA},
	{133,	"DATA_CORE",	0x1A8E3000, false,	NULL, data_core,	ARRAY_SIZE(data_core),	BUS_DATA},
	{134,	"PERI0_CORE",	0x1AA83000, false,	NULL, peri0_core,	ARRAY_SIZE(peri0_core),	BUS_PERI},
	{135,	"PERI1_CORE",	0x1AC83000, false,	NULL, peri1_core,	ARRAY_SIZE(peri1_core),	BUS_PERI},
	{60,	"PERI_BUS0",	0x1B963000, false,	NULL, peri_bus0,	ARRAY_SIZE(peri_bus0),	BUS_PERI},
	{81,	"PERI_BUS1",	0x1BB03000, false,	NULL, peri_bus1,	ARRAY_SIZE(peri_bus1),	BUS_PERI},
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

static struct itmon_dev *g_itmon;

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
		if (!strncmp(pdata->masterinfo[i].port_name, port_name, strlen(port_name))) {
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
			if (node[j].addr_detect_enabled) {
				/* This feature is only for M_NODE */
				unsigned int tmp, val;

				offset = OFFSET_PROT_CHK;
				val = __raw_readl(node[j].regs + offset + REG_PROT_CHK_CTL);
				val |= INTEND_ACCESS_INT_ENABLE;
				__raw_writel(val, node[j].regs + offset + REG_PROT_CHK_CTL);

				val = ((unsigned int)INTEND_ADDR_START & 0xFFFFFFFF);
				__raw_writel(val, node[j].regs + offset + REG_PROT_CHK_START_ADDR_LOW);
				val = (unsigned int)(((unsigned long)INTEND_ADDR_START >> 32) & 0XFFFF);
				__raw_writel(val, node[j].regs + offset + REG_PROT_CHK_START_END_ADDR_UPPER);

				val = ((unsigned int)INTEND_ADDR_END & 0xFFFFFFFF);
				__raw_writel(val, node[j].regs + offset + REG_PROT_CHK_END_ADDR_LOW);
				val = ((unsigned int)(((unsigned long)INTEND_ADDR_END >> 32) & 0XFFFF0000) << 16);
				tmp = readl(node[j].regs + offset + REG_PROT_CHK_START_END_ADDR_UPPER);
				__raw_writel(tmp | val, node[j].regs + offset + REG_PROT_CHK_START_END_ADDR_UPPER);
			}
		}
	}
}

void itmon_wa_enable(const char *node_name, int node_type, bool enabled)
{
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int offset;
	int i, j;

	if (!g_itmon)
		return;

	pdata = g_itmon->pdata;

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].err_enabled &&
				node[j].type == node_type &&
				!strncmp(node[j].name, node_name, strlen(node_name))) {
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
				break;
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
		dev_err(itmon->dev, "all notify calls response NOTIFY_OK\n"
				    "ITMON doesn't do anything, just logging");
		break;
	case NOTIFY_STOP:
		dev_err(itmon->dev, "notify calls response NOTIFY_STOP, refer to notifier log\n"
				    "ITMON counts the error count : %d\n", pdata->err_cnt);
		break;
	case NOTIFY_BAD:
		dev_err(itmon->dev, "notify calls response NOTIFY_BAD\n"
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
	itmon->notifier_info.onoff = traceinfo->onoff;
	itmon->notifier_info.pd_name = traceinfo->pd_name;

	dev_err(itmon->dev, "     +ITMON Notifier Call Information\n\n");

	/* call notifier_call_chain of itmon */
	ret = atomic_notifier_call_chain(&itmon_notifier_list, 0, &itmon->notifier_info);
	itmon_post_handler_apply_policy(itmon, ret);

	dev_err(itmon->dev, "\n     -ITMON Notifier Call Information\n"
		"--------------------------------------------------------------------------\n");
}

static void itmon_post_handler_by_dest(struct itmon_dev *itmon,
					unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];

	if (!traceinfo->dest || strlen(traceinfo->dest) < 1)
		return;

	if (traceinfo->errcode == ERRCODE_TMOUT && traceinfo->snode_dirty == true) {
		if (!strncmp(traceinfo->dest, DREX_COMMON_STR, strlen(DREX_COMMON_STR)) ||
			 !strncmp(traceinfo->dest, "BUS0_DP", strlen("BUS0_DP")) ||
			 !strncmp(traceinfo->dest, "BUS1_DP", strlen("BUS1_DP")) ||
			 !strncmp(traceinfo->dest, "CORE_DP", strlen("CORE_DP"))) {
			pdata->err_drex_tmout = true;
		}
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
	if ((!strncmp(traceinfo->port, "SCI_CCM", strlen("SCI_CCM"))) ||
	    (!strncmp(traceinfo->port, "SCI_IRPM", strlen("SCI_IRPM")))) {
		ktime_t now, interval;

		now = ktime_get();
		interval = ktime_sub(now, pdata->last_time);
		pdata->last_time = now;
		pdata->err_cnt_by_cpu++;
		pdata->err_cpu = true;

		if (traceinfo->errcode == ERRCODE_TMOUT &&
			traceinfo->snode_dirty == true) {
			pdata->err_fatal = true;
			dev_err(itmon->dev,
				"ITMON try to run PANIC, even CPU transaction detected - %s",
				itmon_errcode[traceinfo->errcode]);
		} else {
			dev_err(itmon->dev, "ITMON skips CPU transaction detected - "
				"err_cnt_by_cpu: %u, interval: %lluns\n",
				pdata->err_cnt_by_cpu,
				(unsigned long long)ktime_to_ns(interval));
		}
		dev_err(itmon->dev, "CPU transaction detected - "
				"err_cnt_by_cpu: %u, interval: %lluns\n",
				pdata->err_cnt_by_cpu,
				(unsigned long long)ktime_to_ns(interval));
	} else if (!strncmp(traceinfo->port, CP_COMMON_STR, strlen(CP_COMMON_STR))) {
		pdata->err_cp = true;
	} else {
		pdata->err_cnt++;
	}
}

static void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	unsigned int id, payload, axid, user, valid, timeout, info, user0, user1;
	unsigned long addr;
	char *master_name, *port_name;
	struct itmon_rpathinfo *port;
	struct itmon_masterinfo *master;
	int i, num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
	int rw_offset = (trans_type == TRANS_TYPE_READ ? 0 : REG_TMOUT_BUF_WR_OFFSET);
	int path_offset = 0;

	dev_err(itmon->dev,
		"\n----------------------------------------------------------------------------------\n"
		"      ITMON Report (%s)\n"
		"----------------------------------------------------------------------------------\n"
		"      Timeout Error Occurred : Master --> %s (DRAM)\n\n",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE", node->name);
	dev_err(itmon->dev,
		"      TIMEOUT_BUFFER Information(NODE: %s)\n"
		"	> NUM|   BLOCK|  MASTER|VALID|TIMEOUT|      ID| PAYLOAD|   ADDRESS|   SRAM3|    USER0|    USER1|\n",
			node->name);

	if (node && node->name) {
		if (!strncmp(node->name, DREX_COMMON_STR, strlen(DREX_COMMON_STR)) ||
			 !strncmp(node->name, "BUS0_DP", strlen("BUS0_DP")) ||
			 !strncmp(node->name, "BUS1_DP", strlen("BUS1_DP")) ||
			 !strncmp(node->name, "CORE_DP", strlen("CORE_DP"))) {
			/* This path is data path */
			path_offset = SZ_4;
		}
	}

	for (i = 0; i < num; i++) {
		writel(i, node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_POINT_ADDR + rw_offset);
		id = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_ID + rw_offset);
		payload = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD + rw_offset);
		addr = (((unsigned long)readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_1 + rw_offset + path_offset) &
				GENMASK(15, 0)) << 32ULL);
		addr |= (readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_2 + rw_offset + path_offset));
		info = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_3 + rw_offset + path_offset);

		if (path_offset == SZ_4) {
			user0 = readl(node->regs + OFFSET_TMOUT_REG +
					REG_TMOUT_BUF_USER_0 + rw_offset + path_offset);
			user1 = readl(node->regs + OFFSET_TMOUT_REG +
					REG_TMOUT_BUF_USER_1 + rw_offset + path_offset);

			/* PAYLOAD[19:16] : Timeout */
			user = (user0 & GENMASK(7, 0));
			timeout = (payload & (unsigned int)(GENMASK(7, 4))) >> 7;
		} else {
			user0 = 0;
			user1 = 0;
			/* PAYLOAD[15:8] : USER */
			user = (payload & GENMASK(15, 8)) >> 8;
			/* PAYLOAD[19:16] : Timeout */
			timeout = (payload & (unsigned int)(GENMASK(19, 16))) >> 16;
		}

		/* ID[5:0] 6bit : R-PATH */
		axid = id & GENMASK(5, 0);
		/* PAYLOAD[0] : Valid or Not valid */
		valid = payload & BIT(0);

		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
		if (port) {
			port_name = port->port_name;
			if (user) {
				master = (struct itmon_masterinfo *)
					itmon_get_masterinfo(itmon, port_name, user);
				if (master)
					master_name = master->master_name;
				else
					master_name = NOT_AVAILABLE_STR;
			} else {
				master_name = NOT_AVAILABLE_STR;
			}
		} else {
			port_name = NOT_AVAILABLE_STR;
			master_name = NOT_AVAILABLE_STR;
		}
		dev_err(itmon->dev,
			"      > %03d|%8s|%8s|%5u|%7x|%08x|%08X|%010zx|%08x|%08x|%08x\n",
				i, port_name, master_name, valid, timeout,
				id, payload, addr, info, user0, user1);
	}
	dev_err(itmon->dev,
		"----------------------------------------------------------------------------------\n");
}

static unsigned int power(unsigned int param, unsigned int num)
{
	if (num == 0)
		return 1;
	return param * (power(param, num - 1));
}

static void itmon_report_powerinfo(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	static const char * const gpd_status_lookup[] = {
		[GPD_STATE_ACTIVE] = "on",
		[GPD_STATE_POWER_OFF] = "off"
	};
	static const char * const rpm_status_lookup[] = {
		[RPM_ACTIVE] = "active",
		[RPM_RESUMING] = "resuming",
		[RPM_SUSPENDED] = "suspended",
		[RPM_SUSPENDING] = "suspending"
	};
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *traceinfo = &pdata->traceinfo[trans_type];
	struct exynos_pm_domain *exynos_pd;
	struct generic_pm_domain *genpd;
	const char *p = "";
	struct pm_domain_data *pm_data;

	if (!traceinfo->dirty || !node->pd_name)
		return;

	exynos_pd = exynos_pd_lookup_name(node->pd_name);
	if (exynos_pd) {
		genpd = &exynos_pd->genpd;
		if (!genpd)
			return;
		dev_err(itmon->dev,
			"      Block Power Information - %s\n\n"
			"      > pd-domain name   : %s\n"
			"      > pd-domain status : %s\n\n"
			"--------------------------------------------------------------------------\n"
			"      Runtime-PM Information\n\n"
			"      > genpd name       : %s\n"
			"      > genpd status     : %s\n",
			traceinfo->dest ? traceinfo->dest : NOT_AVAILABLE_STR,
			exynos_pd->name,
			cal_pd_status(exynos_pd->cal_pdid) ? "on" : "off",
			genpd->name,
			gpd_status_lookup[genpd->status]);

		list_for_each_entry(pm_data, &genpd->dev_list, list_node) {
			if (pm_data->dev->power.runtime_error)
				p = "error";
			else if (pm_data->dev->power.disable_depth)
				p = "unsupported";
			else if (pm_data->dev->power.runtime_status < ARRAY_SIZE(rpm_status_lookup))
				p = rpm_status_lookup[pm_data->dev->power.runtime_status];
			else
				WARN_ON(1);

			dev_err(itmon->dev,
				"      > dev name         : %s\n"
				"      > runtime-pm status: %s\n",
				dev_name(pm_data->dev), p);
		}
		traceinfo->onoff = cal_pd_status(exynos_pd->cal_pdid);
		traceinfo->pd_name = exynos_pd->name;
		dev_err(itmon->dev,
			"--------------------------------------------------------------------------\n");
	}
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
		"      > Error code     : %s\n\n",
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
		group = node->group;

		pr_auto(ASL3,
			"\n      > Size           : %u bytes x %u burst => %u bytes\n"
			"      > Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
			"      > Level          : %s\n"
			"      > Protection     : %s\n"
			"      > Path Type      : %s\n\n",
			power(BIT_AXSIZE(tracedata->ext_info_1), 2), BIT_AXLEN(tracedata->ext_info_1) + 1,
			power(BIT_AXSIZE(tracedata->ext_info_1), 2) * (BIT_AXLEN(tracedata->ext_info_1) + 1),
			BIT_AXBURST(tracedata->ext_info_2),
			(BIT_AXPROT(tracedata->ext_info_2) & 0x1) ? "Privileged access" : "Unprivileged access",
			(BIT_AXPROT(tracedata->ext_info_2) & 0x2) ? "Non-secure access" : "Secure access",
			itmon_pathtype[traceinfo->path_type == -1 ? group->bus_type : traceinfo->path_type]);
	}

	/* Summary */
	pr_err("\nITMON: %s %s / %s / 0x%lx / %s / %s\n",
		traceinfo->port, traceinfo->master ? traceinfo->master : "",
		traceinfo->dest ? traceinfo->dest : NOT_AVAILABLE_STR,
		traceinfo->target_addr,
		trans_type == TRANS_TYPE_READ ? "R" : "W",
		itmon_errcode[traceinfo->errcode]);
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
			"      > %14s, %8s(0x%08X)\n",
			node->name, "M_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_S_NODE:
		pr_auto(ASL3,
			"      > %14s, %8s(0x%08X)\n",
			node->name, "T_S_NODE", node->phy_regs + tracedata->offset);
		break;
	case T_M_NODE:
		pr_auto(ASL3,
			"      > %14s, %8s(0x%08X)\n",
			node->name, "T_M_NODE", node->phy_regs + tracedata->offset);
		break;
	case S_NODE:
		pr_auto(ASL3,
			"      > %14s, %8s(0x%08X)\n",
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
	struct itmon_masterinfo *master;
	struct itmon_rpathinfo *port;
	unsigned int errcode, axid;
	unsigned int userbit;
	bool port_find;

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

			if (!strncmp(node->name, CP_COMMON_STR, strlen(CP_COMMON_STR)))
				set_bit(FROM_CP, &traceinfo->from);

			traceinfo->target_addr = (((unsigned long)node->tracedata.ext_info_1
								& GENMASK(3, 0)) << 32ULL);
			traceinfo->target_addr |= node->tracedata.ext_info_0;
			traceinfo->read = tracedata->read;
			traceinfo->errcode = errcode;
			traceinfo->dirty = true;
		} else {
			if (!strncmp(node->name, "SCI_IRPM", strlen(node->name)))
				set_bit(FROM_CPU, &traceinfo->from);
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
		if (test_bit(FROM_CPU, &traceinfo->from)) {
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
		} else if (test_bit(FROM_CP, &traceinfo->from) && (!traceinfo->port))
			traceinfo->port = CP_COMMON_STR;

		/* S_NODE = BUSC_DP or CORE_DP => PERI */
		if (!strncmp(node->name, "BUSC_DP", strlen(node->name)) ||
			!strncmp(node->name, "CORE_DP", strlen(node->name))) {
			port_find = true;
			traceinfo->path_type = BUS_PERI;
		} else if (!strncmp(node->name, DREX_COMMON_STR, strlen(DREX_COMMON_STR))) {
			port_find = true;
			traceinfo->path_type = BUS_DATA;
		} else {
			port_find = false;
			traceinfo->path_type = -1;
		}
		if (port_find && !test_bit(FROM_CPU, &traceinfo->from)) {
			port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
			/* If it couldn't find port, keep previous information */
			if (port) {
				traceinfo->port = port->port_name;
				master = (struct itmon_masterinfo *)
						itmon_get_masterinfo(itmon, traceinfo->port,
							userbit);
				if (master)
					traceinfo->master = master->master_name;
			}
		} else {
			/* If it has traceinfo->port, keep previous information */
			if (!traceinfo->port) {
				port = (struct itmon_rpathinfo *)
						itmon_get_rpathinfo(itmon, axid, node->name);
				if (port) {
					if (!strncmp(port->port_name, CP_COMMON_STR,
							strlen(CP_COMMON_STR)))
						traceinfo->port = CP_COMMON_STR;
					else
						traceinfo->port = port->port_name;
				}
			}
			if (!traceinfo->master && traceinfo->port) {
				master = (struct itmon_masterinfo *)
						itmon_get_masterinfo(itmon, traceinfo->port,
							userbit);
				if (master)
					traceinfo->master = master->master_name;
			}
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
		itmon_report_powerinfo(itmon, node, trans_type);
		break;
	case T_S_NODE:
	case T_M_NODE:
		itmon_report_pathinfo(itmon, node, trans_type);
		break;
	default:
		dev_err(itmon->dev,
			"Unknown Error - offset:%u\n", tracedata->offset);
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
	dev_err(itmon->dev,
		"\n--------------------------------------------------------------------------\n"
		"      Protocol Checker Raw Register Information(ITMON information)\n\n");
	dev_err(itmon->dev,
		"      > %s(%s, 0x%08X)\n"
		"      > REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_nodestring[node->type],
		node->phy_regs,
		dbg_mo_cnt,
		prot_chk_ctl,
		prot_chk_info,
		prot_chk_int_id);

	/* Summary */
	pr_err("\nITMON: Protocol Error / %s / 0x%08X / %s / 0x%08X / 0x%08X / 0x%08X / 0x%08X\n",
		node->name,
		node->phy_regs,
		itmon_nodestring[node->type],
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
	dev_err(itmon->dev,
		"      > %s(%s, 0x%08X)\n"
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
			"\n--------------------------------------------------------------------------\n"
			"      Raw Register Information(ITMON Internal Information)\n\n");

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

	if ((BIT_ERR_CODE(int_info) == ERRCODE_DECERR) && info0 != 0 &&
		!strncmp(node->name, "SCI_IRPM", strlen(node->name)))
		dbg_snapshot_soc_do_dpm_policy(GO_S2D_ID);

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
				if (BIT_PROT_CHK_ERR_OCCURRED(val) && (val & GENMASK(8, 1))) {
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
					if (BIT_PROT_CHK_ERR_OCCURRED(val) && (val & GENMASK(8, 1))) {
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
		group = &pdata->nodegroup[i];
		dev_err(itmon->dev,
				"%s: %d irq, %s group, 0x%x vec",
				__func__, irq, group->name,
				group->phy_regs == 0 ? 0 : __raw_readl(group->regs));
	}

	ret = itmon_search_node(itmon, NULL, true);
	if (!ret) {
		dev_err(itmon->dev, "ITMON could not detect any error\n");
	} else {
		dev_err(itmon->dev,
			"\nITMON Detected: err_fatal:%d, err_drex_tmout:%d, err_cpu:%d, err_cnt:%u, err_cnt_by_cpu:%u\n",
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

static ssize_t itmon_timeout_fix_val_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;

	n = scnprintf(buf + n, 24, "set timeout val: 0x%x \n", pdata->sysfs_tmout_val);

	return n;
}

static ssize_t itmon_timeout_fix_val_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val = 0;
	struct itmon_platdata *pdata = g_itmon->pdata;
	int ret;

	ret = kstrtoul(buf, 16, &val);
	if (!ret) {
		if (val > 0 && val <= 0xFFFFF)
			pdata->sysfs_tmout_val = val;
	} else {
		dev_err(g_itmon->dev, "%s: kstrtoul return value is %d\n", __func__, ret);
	}

	return count;
}

static ssize_t itmon_timeout_val_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
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

static ssize_t itmon_timeout_freeze_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
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

static ssize_t itmon_timeout_val_store(struct kobject *kobj,
				struct kobj_attribute *attr,
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

static ssize_t itmon_timeout_freeze_store(struct kobject *kobj,
				struct kobj_attribute *attr,
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

static struct kobj_attribute itmon_timeout_fix_attr =
        __ATTR(set_val, 0644, itmon_timeout_fix_val_show, itmon_timeout_fix_val_store);
static struct kobj_attribute itmon_timeout_val_attr =
        __ATTR(timeout_val, 0644, itmon_timeout_val_show, itmon_timeout_val_store);
static struct kobj_attribute itmon_timeout_freeze_attr =
        __ATTR(timeout_freeze, 0644, itmon_timeout_freeze_show, itmon_timeout_freeze_store);

static struct attribute *itmon_sysfs_attrs[] = {
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
			dev_err(itmon->dev,
				"\nITMON Detected: err_fatal:%d, err_drex_tmout:%d"
				"err_cpu:%d, err_cnt:%u, err_cnt_by_cpu:%u\n",
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
