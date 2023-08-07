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
#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sprd-debuglog.h>
#include "sprd_debuglog_drv.h"

#define INTC_TO_GIC(i)			((i) + 32)

struct intc_desc {
	char *bit[32];
};

#define AP_INTC0_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"AP_UART0",			\
	"AP_UART1",			\
	"AP_UART2",			\
	"AP_SPI0",			\
	"AP_SPI1",			\
	"AP_SPI2",			\
	"AP_SPI3",			\
	"AP_SIM",			\
	"AP_EMMC",			\
	"AP_I2C0",			\
	"AP_I2C1",			\
	"AP_I2C2",			\
	"AP_I2C3",			\
	"AP_I2C4",			\
	"AP_IIS0",			\
	"AP_IIS1",			\
	"AP_IIS2",			\
	"AP_SDIO0",			\
	"AP_SDIO1",			\
	"AP_SDIO2",			\
	"CE_SEC",			\
	"CE_PUB",			\
	"AP_DMA",			\
	"DMA_SEC_AP",			\
	"GSP",				\
	"DISPC",			\
	"SLV_FW_AP_INTERRUPT",		\
	"DSI_PLL",			\
	"DSI0",				\
	"DSI1",				\
}

#define AP_INTC1_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"VSP",				\
	"AP_I2C5",			\
	"AP_I2C6",			\
	"DVFS_TOP",			\
	"NULL",				\
	"NULL",				\
	"FD_ARM_INT",			\
	"CPP_ARM_INT",			\
	"JPG_ARM_INT",			\
	"ISP_CH0",			\
	"ISP_CH1",			\
	"CSI0_CAL_FAILED",		\
	"CSI0_CAL_DONE",		\
	"CSI0_R2",			\
	"CSI0_R1",			\
	"CSI1_CAL_FAILED",		\
	"CSI1_CAL_DONE",		\
	"CSI1_R2",			\
	"CSI1_R1",			\
	"CSI2_CAL_FAILED",		\
	"CSI2_CAL_DONE",		\
	"CSI2_R2",			\
	"CSI2_R1",			\
	"DCAM0_ARM",			\
	"DCAM1_ARM",			\
	"DCAM2_ARM",			\
	"GPU",				\
	"GPIO",				\
	"THM0",				\
	"THM1",				\
}

#define AP_INTC2_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"THM2",				\
	"KPD",				\
	"AON_I2C",			\
	"OTG",				\
	"ADI",				\
	"AON_TMR",			\
	"EIC",				\
	"AP_TMR0",			\
	"AP_TMR1",			\
	"AP_TMR2",			\
	"AP_TMR3",			\
	"AP_TMR4",			\
	"AP_SYST",			\
	"APCPU_WDG",			\
	"AP_WDG",			\
	"BUSMON_AON",			\
	"MBOX_SRC_AP",			\
	"MBOX_TAR_AP",			\
	"MBOX_TAR_SIPC_AP_NOWAKEUP",	\
	"PWR_UP_AP",			\
	"PWR_UP_PUB",			\
	"PWR_UP_ALL",			\
	"PWR_DOWN_ALL",			\
	"SEC_EIC",			\
	"SEC_EIC_NON_LAT",		\
	"SEC_WDG",			\
	"SEC_RTC",			\
	"SEC_TMR",			\
	"SEC_GPIO",			\
	"SLV_FW_AON",			\
}

#define AP_INTC3_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"MEM_FW_AON",			\
	"AON_32K_DET",			\
	"SCC",				\
	"IIS",				\
	"APB_BUSMON",			\
	"EXT_RSTB_APCPU",		\
	"AON_SYST",			\
	"MEM_FW_PUB",			\
	"PUB_HDW_DFS_EXIT",		\
	"PUB_DFS_ERROR",		\
	"PUB_DFS_COMPLETE",		\
	"PUB_PTM",			\
	"DFI_MONITOR",			\
	"PUB_DMC_MPU_VIO",		\
	"NPMUIRQ0",			\
	"NPMUIRQ1",			\
	"NPMUIRQ2",			\
	"NPMUIRQ3",			\
	"NPMUIRQ4",			\
	"NPMUIRQ5",			\
	"NPMUIRQ6",			\
	"NPMUIRQ7",			\
	"NCTIIRQ0",			\
	"NCTIIRQ1",			\
	"NCTIIRQ2",			\
	"NCTIIRQ3",			\
	"NCTIIRQ4",			\
	"NCTIIRQ5",			\
	"NCTIIRQ6",			\
	"NCTIIRQ7",			\
}

#define AP_INTC4_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"NERRIRQ1",			\
	"NERRIRQ2",			\
	"NERRIRQ3",			\
	"NERRIRQ4",			\
	"NERRIRQ5",			\
	"NERRIRQ6",			\
	"NERRIRQ7",			\
	"NERRIRQ8",			\
	"NFAULTIRQ1",			\
	"NFAULTIRQ2",			\
	"NFAULTIRQ3",			\
	"NFAULTIRQ4",			\
	"NFAULTIRQ5",			\
	"NFAULTIRQ6",			\
	"NFAULTIRQ7",			\
	"NFAULTIRQ8",			\
	"UFS_HC",			\
	"UFS_STA",			\
	"WCN_GNSS_WDG",			\
	"WCN_BTWF_WDG",			\
	"DCAM_SW_INT_COMB",		\
	"ADI_2ND",			\
	"WIFI_SPI_SLV",			\
	"HEARTBEAT",			\
	"UFS_CRYPTO",			\
	"ADIE_RFI_SLV",			\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
}

#define AP_INTC5_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NERRIRQ0",			\
	"NFAULTIRQ0",			\
	"APCPU_PMU",			\
	"APCPU_ERR",			\
	"APCPU_FAULTY",			\
	"NULL",				\
	"DBG_AXI_MON",			\
	"APCPU_MODE_ST",		\
	"NCLUSTERPMUIRQ",		\
	"ANA",				\
	"WTLCP_TG_WDG_RST",		\
	"WTLCP_LTE_WDG_RST",		\
	"MDAR",				\
	"AUDCP_CHN_START_CHN0",		\
	"AUDCP_CHN_START_CHN1",		\
	"AUDCP_CHN_START_CHN2",		\
	"AUDCP_CHN_START_CHN3",		\
	"AUDCP_DMA",			\
	"AUDCP_MCDT",			\
	"AUDCP_VBC_AUDRCD",		\
	"AUDCP_VBC_AUDPLY",		\
	"AUDCP_WDG_RST",		\
	"ACC_PROT_PMU",			\
	"ACC_PROT_AON_APB_REG",		\
	"EIC_NON_LAT",			\
	"PUB_DFS_GIVEUP",		\
	"PUB_DFS_DENY",			\
	"PUB_DFS_VOTE_DONE",		\
}

static struct intc_desc ap_intc[] = {
	AP_INTC0_BIT_NAME, AP_INTC1_BIT_NAME, AP_INTC2_BIT_NAME,
	AP_INTC3_BIT_NAME, AP_INTC4_BIT_NAME, AP_INTC5_BIT_NAME,
};

/* AP_AHB */
static struct reg_bit ahb_eb[] = {
	REG_BIT_INIT("DSI_EB",     0x01, 0, 0),
	REG_BIT_INIT("DISPC_EB",   0x01, 1, 0),
	REG_BIT_INIT("VSP_EB",     0x01, 2, 0),
	REG_BIT_INIT("DMA_ENABLE", 0x01, 4, 0),
	REG_BIT_INIT("DMA_EB",     0x01, 5, 0),
};

static struct reg_bit ap_sys_force_sleep_cfg[] = {
	REG_BIT_INIT("PERI_FORCE_OFF",      0x01, 1, 0),
	REG_BIT_INIT("PERI_FORCE_ON",       0x01, 2, 0),
	REG_BIT_INIT("AXI_LP_CTRL_DISABLE", 0x01, 3, 0),
};

static struct reg_bit ap_m0_lpc[] = {
	REG_BIT_INIT("MAIN_M0_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m1_lpc[] = {
	REG_BIT_INIT("MAIN_M1_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m2_lpc[] = {
	REG_BIT_INIT("MAIN_M2_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m3_lpc[] = {
	REG_BIT_INIT("MAIN_M3_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m4_lpc[] = {
	REG_BIT_INIT("MAIN_M4_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m5_lpc[] = {
	REG_BIT_INIT("MAIN_M5_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m6_lpc[] = {
	REG_BIT_INIT("MAIN_M6_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m7_lpc[] = {
	REG_BIT_INIT("MAIN_M7_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_main_lpc[] = {
	REG_BIT_INIT("MAIN_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MATRIX_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s0_lpc[] = {
	REG_BIT_INIT("MAIN_S0_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S0_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s1_lpc[] = {
	REG_BIT_INIT("MAIN_S1_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S1_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s2_lpc[] = {
	REG_BIT_INIT("MAIN_S2_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S2_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s3_lpc[] = {
	REG_BIT_INIT("MAIN_S3_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S3_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s4_lpc[] = {
	REG_BIT_INIT("MAIN_S4_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S4_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s5_lpc[] = {
	REG_BIT_INIT("MAIN_S5_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S5_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s6_lpc[] = {
	REG_BIT_INIT("MAIN_S6_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S6_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_merge_m1_lpc[] = {
	REG_BIT_INIT("MERGE_M1_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_merge_s0_lpc[] = {
	REG_BIT_INIT("MERGE_S0_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MERGE_S0_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit disp_async_brg[] = {
	REG_BIT_INIT("DISP_ASYNC_BRG_LP_EB", 0x01, 0, 1),
};

static struct reg_bit ap_async_brg[] = {
	REG_BIT_INIT("AP_ASYNC_BRG_LP_EB", 0x01, 0, 1),
};

static struct reg_info ap_ahb[] = {
	REG_INFO_INIT("AHB_EB", 0x0000, ahb_eb),

	REG_INFO_INIT("AP_SYS_FORCE_SLEEP_CFG", 0x000C, ap_sys_force_sleep_cfg),

	REG_INFO_INIT("M0_LPC", 0x0060, ap_m0_lpc),
	REG_INFO_INIT("M1_LPC", 0x0064, ap_m1_lpc),
	REG_INFO_INIT("M2_LPC", 0x0068, ap_m2_lpc),
	REG_INFO_INIT("M3_LPC", 0x006C, ap_m3_lpc),
	REG_INFO_INIT("M4_LPC", 0x0070, ap_m4_lpc),
	REG_INFO_INIT("M5_LPC", 0x0074, ap_m5_lpc),
	REG_INFO_INIT("M6_LPC", 0x0078, ap_m6_lpc),
	REG_INFO_INIT("M7_LPC", 0x007C, ap_m7_lpc),

	REG_INFO_INIT("MAIN_LPC", 0x0088, ap_main_lpc),

	REG_INFO_INIT("S0_LPC", 0x008C, ap_s0_lpc),
	REG_INFO_INIT("S1_LPC", 0x0090, ap_s1_lpc),
	REG_INFO_INIT("S2_LPC", 0x0094, ap_s2_lpc),
	REG_INFO_INIT("S3_LPC", 0x0098, ap_s3_lpc),
	REG_INFO_INIT("S4_LPC", 0x009C, ap_s4_lpc),
	REG_INFO_INIT("S5_LPC", 0x0058, ap_s5_lpc),
	REG_INFO_INIT("S6_LPC", 0x0054, ap_s6_lpc),

	REG_INFO_INIT("MERGE_M1_LPC", 0x00A4, ap_merge_m1_lpc),
	REG_INFO_INIT("MERGE_S0_LPC", 0x00AC, ap_merge_s0_lpc),

	REG_INFO_INIT("DISP_ASYNC_BRG", 0x0050, disp_async_brg),
	REG_INFO_INIT("AP_ASYNC_BRG",   0x005C, ap_async_brg),
};

/* AP_APB */
static struct reg_bit apb_eb[] = {
	REG_BIT_INIT("SIM0_EB",   0x01,  0, 0),
	REG_BIT_INIT("IIS0_EB",   0x01,  1, 0),
	REG_BIT_INIT("IIS1_EB",   0x01,  2, 0),
	REG_BIT_INIT("IIS2_EB",   0x01,  3, 0),
	REG_BIT_INIT("SPI0_EB",   0x01,  5, 0),
	REG_BIT_INIT("SPI1_EB",   0x01,  6, 0),
	REG_BIT_INIT("SPI2_EB",   0x01,  7, 0),
	REG_BIT_INIT("SPI3_EB",   0x01,  8, 0),
	REG_BIT_INIT("I2C0_EB",   0x01,  9, 0),
	REG_BIT_INIT("I2C1_EB",   0x01, 10, 0),
	REG_BIT_INIT("I2C2_EB",   0x01, 11, 0),
	REG_BIT_INIT("I2C3_EB",   0x01, 12, 0),
	REG_BIT_INIT("I2C4_EB",   0x01, 13, 0),
	REG_BIT_INIT("UART0_EB",  0x01, 14, 0),
	REG_BIT_INIT("UART1_EB",  0x01, 15, 1), // uart1 disable in sml
	REG_BIT_INIT("UART2_EB",  0x01, 16, 0),
	REG_BIT_INIT("SDIO0_EB",  0x01, 22, 0),
	REG_BIT_INIT("SDIO1_EB",  0x01, 23, 0),
	REG_BIT_INIT("SDIO2_EB",  0x01, 24, 0),
	REG_BIT_INIT("EMMC_EB",   0x01, 25, 0),
	REG_BIT_INIT("I2C5_EB",   0x01, 26, 0),
	REG_BIT_INIT("I2C6_EB",   0x01, 27, 0),
	REG_BIT_INIT("CE_SEC_EB", 0x01, 30, 0),
	REG_BIT_INIT("CE_PUB_EB", 0x01, 31, 0),
};

static struct reg_bit apb_misc_ctrl[] = {
	REG_BIT_INIT("SPI0_SEC_EB", 0x01,  6, 0),
	REG_BIT_INIT("SPI1_SEC_EB", 0x01,  7, 0),
	REG_BIT_INIT("SPI2_SEC_EB", 0x01,  8, 0),
	REG_BIT_INIT("SPI3_SEC_EB", 0x01,  9, 0),
	REG_BIT_INIT("I2C0_SEC_EB", 0x01, 10, 0),
	REG_BIT_INIT("I2C1_SEC_EB", 0x01, 11, 0),
	REG_BIT_INIT("I2C2_SEC_EB", 0x01, 12, 0),
	REG_BIT_INIT("I2C3_SEC_EB", 0x01, 13, 0),
	REG_BIT_INIT("I2C4_SEC_EB", 0x01, 14, 0),
	REG_BIT_INIT("I2C5_SEC_EB", 0x01, 16, 0),
	REG_BIT_INIT("I2C6_SEC_EB", 0x01, 17, 0),
};

static struct reg_info ap_apb[] = {
	REG_INFO_INIT("APB_EB", 0x0000, apb_eb),
	REG_INFO_INIT("APB_MISC_CTRL", 0x0008, apb_misc_ctrl),
};

/* AON APB */
static struct reg_bit sp_cfg_bus[] = {
	REG_BIT_INIT("SP_CFG_BUS_SLEEP", 0x01, 0, 1),
};

static struct reg_bit apcpu_top_mtx_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_TOP_MTX_M0_LP_EB",   0x01, 0, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M1_LP_EB",   0x01, 1, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M2_LP_EB",   0x01, 2, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M3_LP_EB",   0x01, 3, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S0_LP_EB",   0x01, 4, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S1_LP_EB",   0x01, 5, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S3_LP_EB",   0x01, 7, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_MAIN_LP_EB", 0x01, 8, 1),
};

static struct reg_bit apcpu_ddr_ab_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_DDR_AB_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_dbg_blk_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_DBG_BLK_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_gic600_gic_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_GIC600_GIC_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_scu_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_SCU_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_gic_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_GIC_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_apb_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_APB_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_atb_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_ATB_LP_EB", 0x01, 0, 1),
};

static struct reg_info aon_apb[] = {
	REG_INFO_INIT("SP_CFG_BUS",                 0x0124,
		      sp_cfg_bus),
	REG_INFO_INIT("APCPU_TOP_MTX_M0_LPC_CTRL",  0x0300,
		      apcpu_top_mtx_lpc_ctrl),
	REG_INFO_INIT("APCPU_DDR_AB_LPC_CTRL",      0x0324,
		      apcpu_ddr_ab_lpc_ctrl),
	REG_INFO_INIT("APCPU_DBG_BLK_LPC_CTRL",     0x029C,
		      apcpu_dbg_blk_lpc_ctrl),
	REG_INFO_INIT("APCPU_GIC600_GIC_LPC_CTRL",  0x0298,
		      apcpu_gic600_gic_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_SCU_LPC_CTRL", 0x0320,
		      apcpu_cluster_scu_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_GIC_LPC_CTRL", 0x0294,
		      apcpu_cluster_gic_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_APB_LPC_CTRL", 0x0290,
		      apcpu_cluster_apb_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_ATB_LPC_CTRL", 0x028C,
		      apcpu_cluster_atb_lpc_ctrl),
};

/* PMU APB */
static struct reg_bit apcpu_mode_state0[] = {
	REG_BIT_INIT("APCPU_CORE0_POWER_MODE_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("APCPU_CORE1_POWER_MODE_STATE", 0xFF,  8, 0x07),
	REG_BIT_INIT("APCPU_CORE2_POWER_MODE_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("APCPU_CORE3_POWER_MODE_STATE", 0xFF, 24, 0x07),
};

static struct reg_bit apcpu_mode_state1[] = {
	REG_BIT_INIT("APCPU_CORE4_POWER_MODE_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("APCPU_CORE5_POWER_MODE_STATE", 0xFF,  8, 0x07),
	REG_BIT_INIT("APCPU_CORE6_POWER_MODE_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("APCPU_CORE7_POWER_MODE_STATE", 0xFF, 24, 0x07),
};

static struct reg_bit apcpu_mode_state4[] = {
	REG_BIT_INIT("APCPU_CORINTH_POWER_MODE_STATE", 0xFF, 0, 0x00),
};

static struct reg_bit apcpu_pchannel_state0[] = {
	REG_BIT_INIT("APCPU_CORE0_PCHANNEL_STATE", 0x0F,  0, 0x00),
	REG_BIT_INIT("APCPU_CORE1_PCHANNEL_STATE", 0x0F,  4, 0x00),
	REG_BIT_INIT("APCPU_CORE2_PCHANNEL_STATE", 0x0F,  8, 0x00),
	REG_BIT_INIT("APCPU_CORE3_PCHANNEL_STATE", 0x0F, 12, 0x00),
	REG_BIT_INIT("APCPU_CORE4_PCHANNEL_STATE", 0x0F, 16, 0x00),
	REG_BIT_INIT("APCPU_CORE5_PCHANNEL_STATE", 0x0F, 20, 0x00),
	REG_BIT_INIT("APCPU_CORE6_PCHANNEL_STATE", 0x0F, 24, 0x00),
	REG_BIT_INIT("APCPU_CORE7_PCHANNEL_STATE", 0x0F, 28, 0x00),
};

static struct reg_bit apcpu_pchannel_state2[] = {
	REG_BIT_INIT("APCPU_CORINTH_PCHANNEL_STATE", 0x0F, 0, 0x00),
};

static struct reg_bit pwr_status_dbg0[] = {
	REG_BIT_INIT("PD_AP_SYS_STATE",      0x0F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_TOP_STATE",   0x0F,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE0_STATE", 0x0F, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE1_STATE", 0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg1[] = {
	REG_BIT_INIT("PD_APCPU_CORE2_STATE", 0x0F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE3_STATE", 0x0F,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE4_STATE", 0x0F, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE5_STATE", 0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg2[] = {
	REG_BIT_INIT("PD_APCPU_CORE6_STATE", 0x0F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE7_STATE", 0x0F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg4[] = {
	REG_BIT_INIT("PD_GPU_TOP_STATE", 0x0F, 16, 0x07),
	REG_BIT_INIT("PD_GPU_C0_STATE",  0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg6[] = {
	REG_BIT_INIT("PD_MM_STATE",      0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg9[] = {
	REG_BIT_INIT("PD_AP_VSP_STATE",  0x0F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg11[] = {
	REG_BIT_INIT("PD_PUBCP_STATE",   0x0F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg12[] = {
	REG_BIT_INIT("PD_WTLCP_STATE",      0x0F, 16, 0x07),
	REG_BIT_INIT("PD_WTLCP_LDSP_STATE", 0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg13[] = {
	REG_BIT_INIT("PD_WTLCP_TGDSP_STATE",    0x0F,  0, 0x07),
	REG_BIT_INIT("PD_WTLCP_TD_PROC_STATE",  0x0F, 16, 0x07),
	REG_BIT_INIT("PD_WTLCP_LTE_PROC_STATE", 0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg14[] = {
	REG_BIT_INIT("PD_WTLCP_LTE_CE_STATE",    0x0F,  0, 0x07),
	REG_BIT_INIT("PD_WTLCP_LTE_DPFEC_STATE", 0x0F,  8, 0x07),
	REG_BIT_INIT("PD_WTLCP_HU3GE_A_STATE",   0x0F, 16, 0x07),
	REG_BIT_INIT("PD_WTLCP_HU3GE_B_STATE",   0x0F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg18[] = {
	REG_BIT_INIT("PD_WCN_STATE",             0x0F, 24, 0x07),
};

// if wcn is power off, all subsys state from wcn is invalid, keep 0.
static struct reg_bit pwr_status_dbg19[] = {
	REG_BIT_INIT("PD_WIFI_WRAP_STATE",       0x0F,  0, 0x00),
	REG_BIT_INIT("PD_WIFI_MAC_STATE",        0x0F,  8, 0x00),
	REG_BIT_INIT("PD_WIFI_PHY_STATE",        0x0F, 16, 0x00),
	REG_BIT_INIT("PD_BTWF_SS_STATE",         0x0F, 24, 0x00),
};

static struct reg_bit pwr_status_dbg20[] = {
	REG_BIT_INIT("PD_GNSS_SS_STATE",         0x0F,  0, 0x00),
	REG_BIT_INIT("PD_GNSS_IP_STATE",         0x0F,  8, 0x00),
	REG_BIT_INIT("PD_BT_STATE",              0x0F, 16, 0x00),
};

static struct reg_bit pwr_status_dbg21[] = {
	REG_BIT_INIT("PD_AUDCP_STATE",           0x0F,  8, 0x07),
	REG_BIT_INIT("PD_AUDCP_DSP_STATE",       0x0F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg23[] = {
	REG_BIT_INIT("PD_PUB_STATE",             0x0F, 24, 0x07),
};

static struct reg_bit deep_sleep_mon0[] = {
	REG_BIT_INIT("AP_DEEP_SLEEP",      0x01, 0, 1),
	REG_BIT_INIT("PUBCP_DEEP_SLEEP",   0x01, 1, 1),
	REG_BIT_INIT("WTLCP_DEEP_SLEEP",   0x01, 2, 1),
	REG_BIT_INIT("AUDCP_DEEP_SLEEP",   0x01, 3, 1),
	REG_BIT_INIT("WCN_DEEP_SLEEP",     0x01, 7, 1),
	REG_BIT_INIT("SP_DEEP_SLEEP",      0x01, 8, 1),
};

static struct reg_bit deep_sleep_mon1[] = {
	REG_BIT_INIT("PUB_DEEP_SLEEP",     0x01, 0, 1),
};

static struct reg_bit light_sleep_mon0[] = {
	REG_BIT_INIT("AP_LIGHT_SLEEP",      0x01, 0, 1),
	REG_BIT_INIT("PUBCP_LIGHT_SLEEP",   0x01, 1, 1),
	REG_BIT_INIT("WTLCP_LIGHT_SLEEP",   0x01, 2, 1),
	REG_BIT_INIT("AUDCP_LIGHT_SLEEP",   0x01, 3, 1),
	REG_BIT_INIT("WCN_LIGHT_SLEEP",     0x01, 7, 1),
	REG_BIT_INIT("AON_LIGHT_SLEEP",     0x01, 8, 1),
};

static struct reg_bit light_sleep_mon1[] = {
	REG_BIT_INIT("PUB_LIGHT_SLEEP",     0x01, 0, 1),
};

static struct reg_bit sleep_status0[] = {
	REG_BIT_INIT("AP_SLP_SLEEP",        0x0F,  0, 0),
	REG_BIT_INIT("PUBCP_SLP_SLEEP",     0x0F,  4, 0),
	REG_BIT_INIT("WTLCP_SLP_SLEEP",     0x0F,  8, 0),
	REG_BIT_INIT("AUDCP_SLP_SLEEP",     0x0F, 12, 0),
	REG_BIT_INIT("WCN_SLP_SLEEP",       0x0F, 28, 0),
};

static struct reg_bit sleep_status1[] = {
	REG_BIT_INIT("SP_SLP_SLEEP",        0x0F, 0, 0),
};

static struct reg_bit ddr_slp_status[] = {
	REG_BIT_INIT("DDR_SLP_SLEEP",       0x0F, 0, 0),
};

static struct reg_bit ap_deep_sleep_cnt[] = {
	REG_BIT_INIT("AP_DEEP_SLEEP_CNT",    0xFF, 0, 0),
};

static struct reg_bit pubcp_deep_sleep_cnt[] = {
	REG_BIT_INIT("PUBCP_DEEP_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wtlcp_deep_sleep_cnt[] = {
	REG_BIT_INIT("WTLCP_DEEP_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit audcp_deep_sleep_cnt[] = {
	REG_BIT_INIT("AUDCP_DEEP_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wcn_deep_sleep_cnt[] = {
	REG_BIT_INIT("WCN_DEEP_SLEEP_CNT",   0xFF, 0, 0),
};

static struct reg_bit sp_deep_sleep_cnt[] = {
	REG_BIT_INIT("SP_DEEP_SLEEP_CNT",   0xFF, 0, 0),
};

static struct reg_bit pub_deep_sleep_cnt[] = {
	REG_BIT_INIT("PUB_DEEP_SLEEP_CNT",  0xFF, 0, 0),
};

static struct reg_bit apcpu_top_deep_stop_final_cnt[] = {
	REG_BIT_INIT("APCPU_TOP_DEEP_STOP_FINAL_CNT", 0xFF, 0, 0),
};

static struct reg_bit ap_light_sleep_cnt[] = {
	REG_BIT_INIT("AP_LIGHT_SLEEP_CNT",    0xFF, 0, 0),
};

static struct reg_bit pubcp_light_sleep_cnt[] = {
	REG_BIT_INIT("PUBCP_LIGHT_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wtlcp_light_sleep_cnt[] = {
	REG_BIT_INIT("WTLCP_LIGHT_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit audcp_light_sleep_cnt[] = {
	REG_BIT_INIT("AUDCP_LIGHT_SLEEP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wcn_light_sleep_cnt[] = {
	REG_BIT_INIT("WCN_LIGHT_SLEEP_CNT",   0xFF, 0, 0),
};

static struct reg_bit aon_light_sleep_cnt[] = {
	REG_BIT_INIT("AON_LIGHT_SLEEP_CNT",   0xFF, 0, 0),
};

static struct reg_bit pub_light_sleep_cnt[] = {
	REG_BIT_INIT("PUB_LIGHT_SLEEP_CNT",  0xFF, 0, 0),
};

static struct reg_bit apcpu_top_light_stop_final_cnt[] = {
	REG_BIT_INIT("APCPU_TOP_LIGHT_STOP_FINAL_CNT", 0xFF, 0, 0),
};

static struct reg_bit ap_sys_stop_cnt[] = {
	REG_BIT_INIT("AP_SYS_STOP_CNT",    0xFF, 0, 0),
};

static struct reg_bit pubcp_sys_stop_cnt[] = {
	REG_BIT_INIT("PUBCP_SYS_STOP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wtlcp_sys_stop_cnt[] = {
	REG_BIT_INIT("WTLCP_SYS_STOP_CNT", 0xFF, 0, 0),
};

static struct reg_bit audcp_sys_stop_cnt[] = {
	REG_BIT_INIT("AUDCP_SYS_STOP_CNT", 0xFF, 0, 0),
};

static struct reg_bit wcn_sys_stop_cnt[] = {
	REG_BIT_INIT("WCN_SYS_STOP_CNT",   0xFF, 0, 0),
};

static struct reg_bit apcpu_top_sys_stop_cnt[] = {
	REG_BIT_INIT("APCPU_TOP_SYS_STOP_CNT", 0xFF, 0, 0),
};

static struct reg_info pmu_apb[] = {
	/* Pchannel state*/
	REG_INFO_INIT("APCPU_PCHANNEL_STATE0", 0x0714, apcpu_pchannel_state0),
	REG_INFO_INIT("APCPU_PCHANNEL_STATE2", 0x071C, apcpu_pchannel_state2),

	/* Mode state */
	REG_INFO_INIT("APCPU_MODE_STATE0", 0x0700, apcpu_mode_state0),
	REG_INFO_INIT("APCPU_MODE_STATE1", 0x0704, apcpu_mode_state1),
	REG_INFO_INIT("APCPU_MODE_STATE4", 0x0710, apcpu_mode_state4),

	/* Power state */
	REG_INFO_INIT("PWR_STATUS_DBG0",  0x04F0, pwr_status_dbg0),
	REG_INFO_INIT("PWR_STATUS_DBG1",  0x04F4, pwr_status_dbg1),
	REG_INFO_INIT("PWR_STATUS_DBG2",  0x04F8, pwr_status_dbg2),
	REG_INFO_INIT("PWR_STATUS_DBG4",  0x0500, pwr_status_dbg4),
	REG_INFO_INIT("PWR_STATUS_DBG6",  0x0508, pwr_status_dbg6),
	REG_INFO_INIT("PWR_STATUS_DBG9",  0x0514, pwr_status_dbg9),
	REG_INFO_INIT("PWR_STATUS_DBG11", 0x051C, pwr_status_dbg11),
	REG_INFO_INIT("PWR_STATUS_DBG12", 0x0520, pwr_status_dbg12),
	REG_INFO_INIT("PWR_STATUS_DBG13", 0x0524, pwr_status_dbg13),
	REG_INFO_INIT("PWR_STATUS_DBG14", 0x0528, pwr_status_dbg14),
	REG_INFO_INIT("PWR_STATUS_DBG18", 0x0538, pwr_status_dbg18),
	REG_INFO_INIT("PWR_STATUS_DBG19", 0x053C, pwr_status_dbg19),
	REG_INFO_INIT("PWR_STATUS_DBG20", 0x0540, pwr_status_dbg20),
	REG_INFO_INIT("PWR_STATUS_DBG21", 0x0544, pwr_status_dbg21),
	REG_INFO_INIT("PWR_STATUS_DBG23", 0x054C, pwr_status_dbg23),

	/* Deep sleep check */
	REG_INFO_INIT("DEEP_SLEEP_MON0", 0x0850, deep_sleep_mon0),
	REG_INFO_INIT("DEEP_SLEEP_MON1", 0x0854, deep_sleep_mon1),

	/* Light sleep check */
	REG_INFO_INIT("LIGHT_SLEEP_MON0", 0x0858, light_sleep_mon0),
	REG_INFO_INIT("LIGHT_SLEEP_MON1", 0x085C, light_sleep_mon1),

	/* Sleep state */
	REG_INFO_INIT("SLEEP_STATUS0",  0x0860,  sleep_status0),
	REG_INFO_INIT("SLEEP_STATUS1",  0x0864,  sleep_status1),
	REG_INFO_INIT("DDR_SLP_STATUS", 0x08C4, ddr_slp_status),

	/* Deep sleep count */
	REG_INFO_INIT("AP_DEEP_SLEEP_CNT",    0x0DFC,    ap_deep_sleep_cnt),
	REG_INFO_INIT("PUBCP_DEEP_SLEEP_CNT", 0x0E00, pubcp_deep_sleep_cnt),
	REG_INFO_INIT("WTLCP_DEEP_SLEEP_CNT", 0x0E04, wtlcp_deep_sleep_cnt),
	REG_INFO_INIT("AUDCP_DEEP_SLEEP_CNT", 0x0E08, audcp_deep_sleep_cnt),
	REG_INFO_INIT("WCN_DEEP_SLEEP_CNT",   0x0E18,   wcn_deep_sleep_cnt),
	REG_INFO_INIT("SP_DEEP_SLEEP_CNT",    0x0E1C,    sp_deep_sleep_cnt),
	REG_INFO_INIT("PUB_DEEP_SLEEP_CNT",   0x0E60,   pub_deep_sleep_cnt),
	REG_INFO_INIT("APCPU_TOP_DEEP_STOP_FINAL_CNT", 0x0E48,
		      apcpu_top_deep_stop_final_cnt),

	/* Light sleep count */
	REG_INFO_INIT("AP_LIGHT_SLEEP_CNT",    0x0E68,    ap_light_sleep_cnt),
	REG_INFO_INIT("PUBCP_LIGHT_SLEEP_CNT", 0x0E6C, pubcp_light_sleep_cnt),
	REG_INFO_INIT("WTLCP_LIGHT_SLEEP_CNT", 0x0E70, wtlcp_light_sleep_cnt),
	REG_INFO_INIT("AUDCP_LIGHT_SLEEP_CNT", 0x0E74, audcp_light_sleep_cnt),
	REG_INFO_INIT("WCN_LIGHT_SLEEP_CNT",   0x0E84,   wcn_light_sleep_cnt),
	REG_INFO_INIT("AON_LIGHT_SLEEP_CNT",   0x0E88,   aon_light_sleep_cnt),
	REG_INFO_INIT("PUB_LIGHT_SLEEP_CNT",   0x0ECC,   pub_light_sleep_cnt),
	REG_INFO_INIT("APCPU_TOP_LIGHT_STOP_FINAL_CNT", 0x0EB4,
		      apcpu_top_light_stop_final_cnt),

	/* Stop count */
	REG_INFO_INIT("AP_SYS_STOP_CNT",    0x0ED4,    ap_sys_stop_cnt),
	REG_INFO_INIT("PUBCP_SYS_STOP_CNT", 0x0ED8, pubcp_sys_stop_cnt),
	REG_INFO_INIT("WTLCP_SYS_STOP_CNT", 0x0EDC, wtlcp_sys_stop_cnt),
	REG_INFO_INIT("AUDCP_SYS_STOP_CNT", 0x0EE0, audcp_sys_stop_cnt),
	REG_INFO_INIT("WCN_SYS_STOP_CNT",   0x0EF0,   wcn_sys_stop_cnt),
	REG_INFO_INIT("APCPU_TOP_SYS_STOP_CNT", 0x0F20, apcpu_top_sys_stop_cnt),
};

// Register table
static struct reg_table reg_table_check[] = {
	REG_TABLE_INIT("AP_AHB",  "sprd,sys-ap-ahb",  ap_ahb),
	REG_TABLE_INIT("AP_APB",  "sprd,sys-ap-apb",  ap_apb),
	REG_TABLE_INIT("PMU_APB", "sprd,sys-pmu-apb", pmu_apb),
	REG_TABLE_INIT("AON_APB", "sprd,sys-aon-apb", aon_apb),
};

static struct reg_table reg_table_monitor[] = {
	REG_TABLE_INIT("PMU_APB", "sprd,sys-pmu-apb", pmu_apb),
	REG_TABLE_INIT("AON_APB", "sprd,sys-aon-apb", aon_apb),
};

static struct wakeup_info wakeup_info;
static char wakeup_name[WAKEUP_NAME_LEN];
static char *wakeup_source[2] = {NULL, NULL};

/* For plat to register node */

struct wakeup_node {
	int gic_num, sub_num;
	int (*get)(void *info, void *data);
	void *data;
	struct list_head list;
};

LIST_HEAD(wakeup_node_list);

/**
 * wakeup_info_register - Register wakeup source to debug log
 */
int wakeup_info_register(int gic_num, int sub_num,
			 int (*get)(void *info, void *data), void *data)
{
	struct wakeup_node *pos;
	struct wakeup_node *pn;

	if (!get) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	list_for_each_entry(pos, &wakeup_node_list, list) {
		if (pos->gic_num != gic_num || pos->sub_num != sub_num)
			continue;
		pr_err("%s: Node(%u:%u) exist\n", __func__, gic_num, sub_num);
		return -EEXIST;
	}

	pn = kzalloc(sizeof(struct wakeup_node), GFP_KERNEL);
	if (!pn) {
		pr_err("%s: Wakeup node memory alloc error\n", __func__);
		return -ENOMEM;
	}

	pn->gic_num = gic_num;
	pn->sub_num = sub_num;
	pn->get = get;
	pn->data = data;

	list_add_tail(&pn->list, &wakeup_node_list);

	return 0;
}
EXPORT_SYMBOL_GPL(wakeup_info_register);

/**
 * wakeup_info_register - Register wakeup source to debug log
 */
int wakeup_info_unregister(int gic_num, int sub_num)
{
	struct wakeup_node *pos;

	list_for_each_entry(pos, &wakeup_node_list, list) {
		if (pos->gic_num != gic_num || pos->sub_num != sub_num)
			continue;
		list_del(&pos->list);
		kfree(pos);
		return 0;
	}

	pr_err("%s: Node(%u:%u) not found\n", __func__, gic_num, sub_num);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(wakeup_info_unregister);

/* Wakeup source parse */

#define INT_HANDLE_INIT(bit_num, phandle)			\
{								\
	.bit = bit_num,						\
	.ph = phandle,						\
}

#define INT_HANDLE_SET_INIT(handle_set)				\
{								\
	.set = handle_set,					\
	.num = ARRAY_SIZE(handle_set),				\
}

struct int_handle {
	u32 bit;
	int (*ph)(char **o1, char **o2, u32 m, u32 s, u32 t);
};

struct int_handle_set {
	struct int_handle *set;
	u32 num;
};

/**
 * GPIO int anlyise
 */
static int gpio_int_handler(char **o1, char **o2, u32 m, u32 s, u32 t)
{
	#define GPIO_BIT_SHIFT		4

	struct wakeup_node *node;
	int inum, ibit, gnum;
	int grp, bit;
	char *iname;
	int ret;

	inum = (m >> 16) & 0xFFFF;
	ibit = m & 0xFFFF;
	iname = ap_intc[inum].bit[ibit];

	grp = (s >> 16) & 0xFFFF;
	bit = s & 0xFFFF;

	sprintf(wakeup_name, "%s(%u)", iname, (grp << GPIO_BIT_SHIFT) + bit);
	*o1 = wakeup_name;

	gnum = INTC_TO_GIC((inum << 5) + ibit);

	list_for_each_entry(node, &wakeup_node_list, list) {
		if (node->gic_num != gnum)
			continue;
		ret = node->get(&wakeup_info, node->data);
		if (ret) {
			pr_err("%s: Get wakeup info error\n", __func__);
			return -EINVAL;
		}
		break;
	}

	if (&node->list != &wakeup_node_list)
		*o2 = wakeup_info.name;

	return 0;
}

/**
 * AP EIC int anlyise
 */
static int eic_int_handler(char **o1, char **o2, u32 m, u32 s, u32 t)
{
	/* EIC int list */
	#define EIC_EXT_NUM			6
	#define EIC_TYPE_NUM			4
	#define EIC_NUM				8

	static const char *eic_index[] = {
		"AON_EIC_EXT0", "AON_EIC_EXT1", "AON_EIC_EXT2",
		"AON_EIC_EXT3", "AON_EIC_EXT4", "AON_EIC_EXT5",
	};

	static const char *eic_type[] = {"DBNC", "LATCH", "ASYNC", "SYNC"};

	struct wakeup_node *node;
	int inum, ibit, gnum;
	int grp, bit, num;
	int ret;

	inum = (m >> 16) & 0xFFFF;
	ibit = m & 0xFFFF;
	gnum = INTC_TO_GIC((inum << 5) + ibit);

	grp = (s >> 16) & 0xFFFF;
	bit = s & 0xFFFF;
	num = t & 0xFFFF;

	if (grp >= EIC_EXT_NUM || bit >= EIC_TYPE_NUM || num >= EIC_NUM) {
		pr_err("%s: EIC index out of range\n", __func__);
		return -EINVAL;
	}

	sprintf(wakeup_name, "%s(%s.%s.%u)",
		   ap_intc[inum].bit[ibit], eic_index[grp], eic_type[bit], num);
	*o1 = wakeup_name;

	list_for_each_entry(node, &wakeup_node_list, list) {
		if (node->gic_num != gnum)
			continue;
		ret = node->get(&wakeup_info, node->data);
		if (ret) {
			pr_err("%s: Get wakeup info error\n", __func__);
			return -EINVAL;
		}
		break;
	}

	if (&node->list != &wakeup_node_list)
		*o2 = wakeup_info.name;

	return 0;
}

/**
 * Mailbox int anlyise
 */
static int mbox_int_handler(char **o1, char **o2, u32 m, u32 s, u32 t)
{
	#define MBOX_SRC_NUM		9

	static const char *mbox_src[] = {
		"APCPU", "AON CMSTAR", "CR5",
		"TGDSP", "LDSP", "ADSP",
		"APCPU", "GNSS", "BTWF",
	};

	struct wakeup_node *node;
	int inum, ibit, gnum;
	int grp, bit;
	int ret;

	inum = (m >> 16) & 0xFFFF;
	ibit = m & 0xFFFF;
	gnum = INTC_TO_GIC((inum << 5) + ibit);

	grp = (s >> 16) & 0xFFFF;
	bit = s & 0xFFFF;

	if (bit >= MBOX_SRC_NUM) {
		pr_err("%s: Mailbox index out of range\n", __func__);
		return -EINVAL;
	}

	sprintf(wakeup_name, "%s(%s)", ap_intc[inum].bit[ibit], mbox_src[bit]);
	*o1 = wakeup_name;

	list_for_each_entry(node, &wakeup_node_list, list) {
		if (node->gic_num != gnum)
			continue;
		ret = node->get(&wakeup_info, node->data);
		if (ret) {
			pr_err("%s: Get wakeup info error\n", __func__);
			return -EINVAL;
		}
		break;
	}

	if (&node->list != &wakeup_node_list)
		*o2 = wakeup_info.name;

	return 0;
}

/**
 * PMIC int anlyise
 */
static int ana_int_handler(char **o1, char **o2, u32 m, u32 s, u32 t)
{
	/* PMIC ANA int list */
	#define ANA_INT_NUM			10
	#define ANA_EIC_IDX			4
	#define ANA_EIC_NUM			8

	static const char *ana_int[ANA_INT_NUM] = {
		"ADC_INT", "RTC_INT", "WDG_INT", "FGU_INT", "EIC_INT",
		"FAST_CHG_INT", "TMR_INT", "CAL_INT", "TYPEC_INT", "USB_PD_INT"
	};

	static const char *ana_eic[ANA_EIC_NUM] = {
		"CHGR_INT", "PBINT", "PBINT2", "BATDET_OK", "KEY2_7S_EXT_RSTN",
		"EXT_XTL0_EN", "AUD_INT_ALL", "ENDURA_OPTION"
	};

	struct wakeup_node *node;
	int inum, ibit, gnum;
	int bit, num;
	int pos, ret;
	char *iname;

	inum = (m >> 16) & 0xFFFF;
	ibit = m & 0xFFFF;
	iname = ap_intc[inum].bit[ibit];

	bit = s & 0xFFFF;
	num = t & 0xFFFF;

	if (bit >= ANA_INT_NUM) {
		pr_err("%s: Ana int out of range\n", __func__);
		return -EINVAL;
	}

	pos = sprintf(wakeup_name, "%s(%s", iname, ana_int[bit]);
	if (bit == ANA_EIC_IDX && num < ANA_EIC_NUM)
		pos += sprintf(wakeup_name + pos, ".%s", ana_eic[num]);
	sprintf(wakeup_name + pos, ")");

	*o1 = wakeup_name;

	gnum = INTC_TO_GIC((inum << 5) + ibit);

	list_for_each_entry(node, &wakeup_node_list, list) {
		if (node->gic_num != gnum || node->sub_num != bit)
			continue;
		ret = node->get(&wakeup_info, node->data);
		if (ret) {
			pr_err("%s: Get wakeup info error\n", __func__);
			return -EINVAL;
		}
		break;
	}

	if (&node->list != &wakeup_node_list)
		*o2 = wakeup_info.name;

	return 0;
}

static struct int_handle ap_intc0_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc1_handle[] = {
	INT_HANDLE_INIT(29, gpio_int_handler),
};

static struct int_handle ap_intc2_handle[] = {
	INT_HANDLE_INIT(8, eic_int_handler),
	INT_HANDLE_INIT(19, mbox_int_handler),
};

static struct int_handle ap_intc3_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc4_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc5_handle[] = {
	INT_HANDLE_INIT(13, ana_int_handler),
};

static struct int_handle_set ap_intc_handle_set[] = {
	INT_HANDLE_SET_INIT(ap_intc0_handle),
	INT_HANDLE_SET_INIT(ap_intc1_handle),
	INT_HANDLE_SET_INIT(ap_intc2_handle),
	INT_HANDLE_SET_INIT(ap_intc3_handle),
	INT_HANDLE_SET_INIT(ap_intc4_handle),
	INT_HANDLE_SET_INIT(ap_intc5_handle),
};

/* Wakeup source match */
static int plat_match(u32 major, u32 second, u32 thrid, void *data, int num)
{
	struct int_handle_set *pset;
	int inum, ibit;
	char **pstr;
	int pos, i;
	int ret;

	if (!data || num != 2) {
		pr_err("%s: Data is error\n", __func__);
		return -EINVAL;
	}

	inum = (major >> 16) & 0xFFFF;
	ibit = major & 0xFFFF;

	if (inum >= ARRAY_SIZE(ap_intc_handle_set) || ibit >= 32) {
		pr_err("%s: Wakeup source out of range\n", __func__);
		return -EINVAL;
	}

	pstr = (char **)data;
	pset = &ap_intc_handle_set[inum];

	for (i = 0; i < pset->num; ++i) {
		if (ibit != pset->set[i].bit || !pset->set[i].ph)
			continue;
		ret = pset->set[i].ph(&pstr[0], &pstr[1], major, second, thrid);
		if (!ret)
			return 0;
		break;
	}

	pos = sprintf(wakeup_name, "%s", ap_intc[inum].bit[ibit]);
	pstr[0] = wakeup_name;

	return 0;
}

struct plat_data sprd_sharkl6_debug_data = {
	.sleep_condition_table = reg_table_check,
	.sleep_condition_table_num = 4,
	.subsys_state_table = reg_table_monitor,
	.subsys_state_table_num = 2,
	.wakeup_source_info = wakeup_source,
	.wakeup_source_info_num = 2,
	.wakeup_source_match = plat_match,
};
