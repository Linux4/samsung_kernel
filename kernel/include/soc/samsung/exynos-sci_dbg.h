/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_SCI_DBG_H_
#define __EXYNOS_SCI_DBG_H_

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#define EXYNOS_SCI_DBG_MODULE_NAME	"exynos-sci_dbg"
#define LLC_DSS_NAME			"log_llc"
#define BCM_ESS_NAME			"log_bcm"

#define LLC_DUMP_DATA_Q_SIZE			8
#define LLC_SLICE_END				(0x1)
#define LLC_BANK_END				(0x1)
#define LLC_SET_END				(0x1FF)
#define LLC_WAY_END				(0xF)
#define LLC_QWORD_END				(0x7)

#define SCI_BASE				0x22800000
#define ArrDbgCntl				0x05C
#define ArrDbgRDataHi				0x06C
#define ArrDbgRDataMi				0x070
#define ArrDbgRDataLo				0x074
#define LLCAddrMatch				0x4C0
#define LLCAddrMask				0x4C4
#define DebugSrc10_offset			0x2C8
#define DebugSrc32_offset			0x2CC
#define DebugCtrl_offset			0x2e0

#define SMC_MIF0_BASE			0x2303F000
#define SMC_MIF1_BASE			0x2403F000
#define SMC_MIF2_BASE			0x2503F000
#define SMC_MIF3_BASE			0x2603F000

#define SYSREG_MIF0_BASE		0x23020000
#define SYSREG_MIF1_BASE		0x24020000
#define SYSREG_MIF2_BASE		0x25020000
#define SYSREG_MIF3_BASE		0x26020000

#define PPC_DEBUG0_BASE			0x23050000
#define PPC_DEBUG1_BASE			0x24050000
#define PPC_DEBUG2_BASE			0x25050000
#define PPC_DEBUG3_BASE			0x26050000
#define PPC_DEBUG_CCI			0x22A30000
#define SYSREG_CORE_PPC_BASE		0x22821000

#define TREX_D_NOCL0		0x229EA000
#define LLC_USER_CONFIG_DEFAULT		0x800
#define LLC_USER_CONFIG_MATCH		0x900
#define LLC_USER_CONFIG_USER		0x904

#define CACHEAID_BASE			0x22B00000
#define CACHEAID_USER			0x100
#define CACHEAID_CTRL			0x104
#define CACHEAID_GLOBAL_CTRL		0x0
#define CACHEAID_DEFAULT_CTRL		0x10
#define CACHEAID_PMU_ACCESS_CTRL	0x20
#define CACHEAID_PMU_ACCESS_INFO	0x24

#define SCI_EVENT_SEL_OFFSET	0x100
#define PPC_DEBUG_CON		0x1000
#define PPC_DEBUG_CON_EXT		0x1004			
#define DBG_CLK_CONTROL		0x102C			
/* SCI_PPC_WRAPPER offset */
#define SCI_PPC_PMNC			0x4
#define SCI_PPC_CNTENS			0x8
#define SCI_PPC_INTENS			0x10
#define SCI_PPC_FLAG			0x18
#define SCI_PPC_PMCNT0_LOW		0x34
#define SCI_PPC_PMCNT1_LOW		0x38
#define SCI_PPC_PMCNT2_LOW		0x3C
#define SCI_PPC_PMCNT3_LOW		0x40
#define SCI_PPC_CCNT_LOW		0x48

/* SMC_PPC_WRAPPER offset */
#define SMC_PPC_PMNC			0x4
#define SMC_PPC_CNTENS			0x8
#define SMC_PPC_CCNT			0x2C

#define SMC_PPC_CCNT_LOW		0x0048
#define SMC_PPC_CCNT_HIGH		0x0058
#define SMC_PPC_PMCNT0			0x0034
#define SMC_PPC_PMCNT1			0x0038
#define SMC_PPC_PMCNT2			0x003C
#define SMC_PPC_PMCNT3			0x0040
#define SMC_PPC_PMCNT4			0x00B4
#define SMC_PPC_PMCNT5			0x00B8
#define SMC_PPC_PMCNT6			0x00BC
#define SMC_PPC_PMCNT7			0x00C0
#define SMC_PPC_PMCNT7_H		0x00C4
#define SMC_MAX_CNT			11

/* SMC_ALL_BASE offset */
#define SMC_DBG_BLK_CTL0		0x294
#define SMC_DBG_BLK_CTL1		0x298
#define SMC_DBG_BLK_CTL2		0x29C
#define SMC_DBG_BLK_CTL3		0x2A0
#define SMC_DBG_BLK_CTL4		0x2A4
#define SMC_DBG_BLK_CTL5		0x2A8
#define SMC_DBG_BLK_CTL6		0x2AC
#define SMC_DBG_BLK_CTL7		0x2B0
#define SMC_DBG_BLK_CTL8		0x2B4
#define SMC_DBG_BLK_CTL(x)		(SMC_DBG_BLK_CTL0 + ((x)*4))
#define SMC_GLOBAL_DBG_CTL		0x290
#define SMC_SPARE_CFG_CTL		0x48C

#define LLC_En_Bit				(25)
#define DisableLlc_Bit				(9)

#define NUM_OF_SMC_DBG_BLK_CTL			(9)
#define NUM_OF_SYSREG_MIF			(4)

/* IPC common definition */
#define SCI_DBG_ONE_BIT_MASK			(0x1)
#define SCI_DBG_ERR_MASK				(0x7)
#define SCI_DBG_ERR_SHIFT				(13)

#define SCI_DBG_CMD_IDX_MASK			(0x3F)
#define SCI_DBG_CMD_IDX_SHIFT			(0)
#define SCI_DBG_DATA_MASK				(0x3F)
#define SCI_DBG_DATA_SHIFT				(6)
#define SCI_DBG_IPC_DIR_SHIFT			(12)

#define SCI_DBG_CMD_GET(cmd_data, mask, shift)	((cmd_data & (mask << shift)) >> shift)
#define SCI_DBG_CMD_CLEAR(mask, shift)		(~(mask << shift))
#define SCI_DBG_CMD_SET(data, mask, shift)		((data & mask) << shift)

#define SCI_DBG_DBGGEN
#ifdef SCI_DBG_DBGGEN
#define SCI_DBG_DBG(x...)	pr_info("sci_dbg_dbg: " x)
#else
#define SCI_DBG_DBG(x...)	do {} while (0)
#endif

#define SCI_DBG_INFO(x...)	pr_info("sci_dbg_info: " x)
#define SCI_DBG_ERR(x...)	pr_err("sci_dbg_err: " x)

#define	LLC_PPC_EVENT_MAX		3
#define	NUM_OF_MIF_BASE			4

struct exynos_ppc_dump_addr {
	u32				p_addr;
	u32				p_size;
};

struct exynos_sci_dbg_dump_addr {
	u32				p_addr;
	u32				p_size;
	u32				buff_size;
	void __iomem			*v_addr;
	void __iomem			*cnt_sfr_base;
	void __iomem			*trex_core_base;
	void __iomem			*sci_base;
	void __iomem			*smc_base;
	void __iomem			*sysreg_mif_base[4];
	void __iomem			*smc_mif_base[NUM_OF_MIF_BASE];
	void __iomem			*ppc_dbg_base[4];
	void __iomem			*trex_irps_base;
	void __iomem			*debug_base;
};

struct exynos_sci_dbg_dump_data {
	u32				index;
	u64				time;
	u32				count[5];
} __attribute__((packed));

struct exynos_smc_dump_data {
	u32				index;
	u32				smc_ch;
	u64				time;
	u32				count[SMC_MAX_CNT];
} __attribute__((packed));

struct exynos_sci_ppc_event {
	int sci_event_sel;
	int dbgsrc10;
	int dbgsrc32;
};

struct exynos_sci_dbg_data {
	struct device			*dev;
	spinlock_t			lock;

	struct exynos_sci_dbg_dump_addr	dump_addr;
	struct exynos_sci_dbg_dump_data	dump_data;
	bool				dump_enable;
	struct hrtimer			hrtimer;
	struct exynos_smc_dump_data	smc_dump_data[4];
	bool				smc_dump_enable;
	struct hrtimer			smc_hrtimer;

	void __iomem			*sci_base;
	void __iomem			*cacheaid_base;
	int				cur_event;
	struct exynos_sci_ppc_event 	ppc_event[LLC_PPC_EVENT_MAX];
};

bool get_exynos_sci_llc_debug_mode(void);

extern void smc_ppc_enable(unsigned int enable);
extern void sci_ppc_enable(unsigned int enable);

extern struct platform_driver exynos_sci_dbg_driver;

#endif	/* __EXYNOS_SCI_DBG_H_ */
