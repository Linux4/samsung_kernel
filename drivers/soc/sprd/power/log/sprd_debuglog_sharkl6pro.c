/*
 * Copyright (C) 2021 Spreadtrum Communications Inc.
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

#define AP_INTC0_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"APCPU_MODE_ST",			\
	"AP_PWR_UP",				\
	"PUB_PWR_UP",				\
	"PWR_UP_ALL",				\
	"PWR_DOWN_ALL",				\
	"BUSMON_AON_MAIN_MTX_MASTER",		\
	"AON_USB2",				\
	"CLK_32K_DET",				\
	"SEC_EIC",				\
	"SEC_GPIO",				\
	"SEC_WDG",				\
	"SEC_TMR",				\
	"AP_WDG",				\
	"APB_BUSMON_AON",			\
	"JTAG",					\
	"SEC_CNT_TOP",				\
	"EXT_RSTB_APCPU",			\
	"ADI_MST",				\
	"THM3_INT",				\
	"THM2_INT",				\
	"THM1_INT",				\
	"THM0_INT",				\
	"AON_EIC",				\
	"AON_IIS",				\
	"APCPU_WDG",				\
	"AP_SYST",				\
	"KPD",					\
	"AP_TMR2_TMR2",				\
	"AP_TMR2_TMR1",				\
	"AP_TMR2_TMR0",				\
}

#define AP_INTC1_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"AP_TMR1_TMR2",				\
	"AP_TMR1_TMR1",				\
	"AP_TMR1_TMR0",				\
	"AP_TMR0_TMR2",				\
	"AP_TMR0_TMR1",				\
	"AP_TMR0_TMR0",				\
	"AON_GPIO",				\
	"SCC_INT1",				\
	"SCC_INT0",				\
	"MAILBOX_OUT6",				\
	"MAILBOX_OUT0",				\
	"MAILBOX_IN0",				\
	"AON_SYS_FRT",				\
	"AON_TMR_TMR0",				\
	"ISE_TO_AP_TEE",			\
	"ISE_TO_AP_REE",			\
	"TDM_RX",				\
	"TDM_TX",				\
	"DMA_CHN1_GRP1_INT",			\
	"DMA_CHN2_GRP1_INT",			\
	"DMA_CHN1_GRP2_INT",			\
	"DMA_CHN2_GRP2_INT",			\
	"DMAAP",				\
	"MCDT_AP",				\
	"VBC_AUDRCD",				\
	"VBC_AUDPLY",				\
	"AUD_WDG_RST_EXTD",			\
	"EIC_INT_AP",				\
	"ESE_WDG_RST_FLAG",			\
	"BUSMON_TMC_AXI",			\
}

#define AP_INTC2_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"IPA_PWR_UP",				\
	"ETR_AXI_MON_INT",			\
	"BUSMON_DDC_AXI",			\
	"DDC_CTRL",				\
	"PCIE_PWR_UP",				\
	"G3_RADM_INTD",				\
	"G3_RADM_INTC",				\
	"G3_RADM_INTB",				\
	"G3_RADM_INTA",				\
	"G3_RX_BEACON_INT",			\
	"G3_CFG_BW_MGT_INT",			\
	"G3_CFG_LINK_AUTO_BW_INT",		\
	"G3_CFG_PME_MSI",			\
	"G3_CFG_PME_INT",			\
	"G3_ERR_INT",				\
	"G3_RX_MSG_INT",			\
	"G3_MSI_CTRL_INT",			\
	"G3_CFG_AER_RC_ERR_MSI",		\
	"G3_CFG_AER_RC_ERR_INT",		\
	"INBOX_SCR_IRQ_SIG0",			\
	"OUTBOX_SCR_IRQ_SIG0",			\
	"CSI3_CAL_FAILED",			\
	"CSI3_CAL_DONE",			\
	"CSI3_INT_REQ1",			\
	"CSI3_INT_REQ0",			\
	"CSI2_CAL_FAILED",			\
	"CSI2_CAL_DONE",			\
	"CSI2_INT_REQ1",			\
	"CSI2_INT_REQ0",			\
	"CSI1_CAL_FAILED",			\
}

#define AP_INTC3_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"CSI1_CAL_DONE",			\
	"CSI1_INT_REQ1",			\
	"CSI1_INT_REQ0",			\
	"CSI0_CAL_FAILED",			\
	"CSI0_CAL_DONE",			\
	"CSI0_INT_REQ1",			\
	"CSI0_INT_REQ0",			\
	"DCAM1_3A_INT",				\
	"DCAM0_3A_INT",				\
	"DCAM1_ARM_INT",			\
	"DCAM0_ARM_INT",			\
	"DCAM3_ARM_INT",			\
	"DCAM2_ARM_INT",			\
	"JPG_ARM_INT",				\
	"CPP_ARM_INT",				\
	"VDMA_VAU_INT",				\
	"VDMA_INT",				\
	"INT_MSTD_VAU",				\
	"INT_IDMA_VAU",				\
	"INT_FD",				\
	"SLV_FW_MM_INTERRUPT",			\
	"TCA_INT",				\
	"DPU",					\
	"TRNG_IRQ",				\
	"HDCP22_HPI_IRQ",			\
	"DP_INTERRUPT",				\
	"BUSMON_PAM_U3",			\
	"BUSMON_PAM_WIFI",			\
	"BUSMON_IPA_M0",			\
	"BUSMON_DBG_IRAM",			\
}

#define AP_INTC4_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"IPA_MAP7",				\
	"IPA_MAP6",				\
	"IPA_MAP5",				\
	"IPA_MAP4",				\
	"IPA_MAP3",				\
	"IPA_MAP2",				\
	"IPA_MAP1",				\
	"IPA_MAP0",				\
	"IPA_MAP",				\
	"PAM_WIFI_TO_AP",			\
	"PAMU3_COMFIFO",			\
	"PAMU3",				\
	"USB_0",				\
	"NULL",					\
	"SLV_FW_IPA_CFG",			\
	"SLV_FW_IPA_MAIN",			\
	"GPU",					\
	"VPU_DEC",				\
	"VPU_ENC1",				\
	"VPU_ENC0",				\
	"GSP1",					\
	"GSP0",					\
	"DISPC",				\
	"DSI1_PLL",				\
	"DSI1_1",				\
	"DSI1_0",				\
	"DSI0_PLL",				\
	"DSI0_1",				\
	"DSI0_0",				\
	"UFS",					\
}

#define AP_INTC5_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"UFS_FE_INT",				\
	"BUSMON_UFSHC",				\
	"CE_SEC",				\
	"CE_PUB",				\
	"AP_UART3",				\
	"AP_UART2",				\
	"APCPU_CPS",				\
	"PMU_INT",				\
	"ERR_INT",				\
	"FAULT_INT",				\
	"NERRIRQ0",				\
	"NERRIRQ1TO8",				\
	"NFAULTIRQ0",				\
	"NFAULTIRQ1TO8",			\
	"NCLUSTERPMUIRQ",			\
	"NPMUIRQ0",				\
	"NPMUIRQ1",				\
	"NPMUIRQ2",				\
	"NPMUIRQ3",				\
	"NPMUIRQ4",				\
	"NPMUIRQ5",				\
	"NPMUIRQ6",				\
	"NPMUIRQ7",				\
	"NCTIIRQ0",				\
	"NCTIIRQ1",				\
	"NCTIIRQ2",				\
	"NCTIIRQ3",				\
	"NCTIIRQ4",				\
	"NCTIIRQ5",				\
	"NCTIIRQ6",				\
}

#define AP_INTC6_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"NCTIIRQ7",				\
	"AP_UART1",				\
	"AP_UART0",				\
	"SPI2",					\
	"SPI1",					\
	"SPI0",					\
	"AI_PERF_BUSMON",			\
	"SDIO2",				\
	"SDIO1",				\
	"SDIO0",				\
	"IIS2",					\
	"IIS1",					\
	"IIS0",					\
	"I2C9",					\
	"I2C8",					\
	"I2C7",					\
	"I2C6",					\
	"I2C5",					\
	"I2C4",					\
	"I2C3",					\
	"I2C2",					\
	"I2C1",					\
	"I2C0",					\
	"EMMC",					\
	"INT_SEC_REQ_DMA",			\
	"DMA",					\
	"SLV_FW_AP",				\
	"AI_MEM_FW",				\
	"AI_POWERVR_2",				\
	"AI_POWERVR_1",				\
}

#define AP_INTC7_BIT_NAME			\
{						\
	"NULL",					\
	"NULL",					\
	"AI_POWERVR_0",				\
	"TOP_DVFS",				\
	"SLV_FW_AON",				\
	"MEM_FW_AON",				\
	"ANA_INT0",				\
	"ANA_INT1",				\
	"ANA_INT2",				\
	"CSI_SW_FALTAL_ERROR",			\
	"BUSMON_INT_VDSP_BLK",			\
	"BUSMON_INT_JPG",			\
	"BUSMON_INT_ISP_BLK",			\
	"DEP_CH0",				\
	"ISP_DEC_INT_REQ",			\
	"MM_SYS_VDSP_STOP",			\
	"ISP_CH0",				\
	"ISP_CH1",				\
	"VDSP_PFATALERROR",			\
	"VDSP_PWAITMODE",			\
	"PUB_DFS_COMPLETE",			\
	"PUB_DFS_EXIT",				\
	"PUB_DFS_DENY",				\
	"PUB_DFS_VOTE_DONE",			\
	"PUB_DFS_ERROR",			\
	"PUB_DFS_GIVEUP",			\
	"PUB_DFI_BUSMON",			\
	"PUB_PTM",				\
	"PUB_MEM_FW",				\
	"PUB_DMC_MPU_VIO",			\
	"ACC_PROT_AON_APB_REG",			\
	"ACC_PROT_PMU",				\
}

static struct intc_desc ap_intc[] = {
	AP_INTC0_BIT_NAME, AP_INTC1_BIT_NAME,
	AP_INTC2_BIT_NAME, AP_INTC3_BIT_NAME,
	AP_INTC4_BIT_NAME, AP_INTC5_BIT_NAME,
	AP_INTC6_BIT_NAME, AP_INTC7_BIT_NAME,
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
	REG_BIT_INIT("APCPU_CORE0_PCHANANEL_STATE", 0x0F,  0, 0x00),
	REG_BIT_INIT("APCPU_CORE1_PCHANANEL_STATE", 0x0F,  4, 0x00),
	REG_BIT_INIT("APCPU_CORE2_PCHANANEL_STATE", 0x0F,  8, 0x00),
	REG_BIT_INIT("APCPU_CORE3_PCHANANEL_STATE", 0x0F, 12, 0x00),
	REG_BIT_INIT("APCPU_CORE4_PCHANANEL_STATE", 0x0F, 16, 0x00),
	REG_BIT_INIT("APCPU_CORE5_PCHANANEL_STATE", 0x0F, 20, 0x00),
	REG_BIT_INIT("APCPU_CORE6_PCHANANEL_STATE", 0x0F, 24, 0x00),
	REG_BIT_INIT("APCPU_CORE7_PCHANANEL_STATE", 0x0F, 28, 0x00),
};

static struct reg_bit apcpu_pchannel_state2[] = {
	REG_BIT_INIT("APCPU_CORINTH_PCHANANEL_STATE", 0x0F,  0, 0x00),
};

static struct reg_bit pwr_status_dbg0[] = {
	REG_BIT_INIT("PD_AP_STATE",           0x1F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_TOP_STATE",    0x1F,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE0_STATE",  0x1F, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE1_STATE",  0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg1[] = {
	REG_BIT_INIT("PD_APCPU_CORE2_STATE",  0x1F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE3_STATE",  0x1F,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE4_STATE",  0x1F, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE5_STATE",  0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg2[] = {
	REG_BIT_INIT("PD_APCPU_CORE6_STATE",  0x1F,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_CORE7_STATE",  0x1F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg4[] = {
	REG_BIT_INIT("PD_GPU_STATE",          0x1F, 16, 0x07),
	REG_BIT_INIT("PD_GPU_CORE0_STATE",    0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg5[] = {
	REG_BIT_INIT("PD_GPU_CORE1_STATE",    0x1F,  0, 0x07),
	REG_BIT_INIT("PD_GPU_CORE2_STATE",    0x1F,  8, 0x07),
	REG_BIT_INIT("PD_GPU_CORE3_STATE",    0x1F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg6[] = {
	REG_BIT_INIT("PD_CAMERA_STATE",       0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg7[] = {
	REG_BIT_INIT("PD_DCAM_BLK_STATE",     0x1F,  0, 0x07),
	REG_BIT_INIT("PD_ISP_BLK_STATE",      0x1F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg8[] = {
	REG_BIT_INIT("PD_VDSP_BLK_STATE",     0x1F,  0, 0x07),
	REG_BIT_INIT("PD_DPU_VSP_STATE",      0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg9[] = {
	REG_BIT_INIT("PD_VENC0_STATE",        0x1F,  0, 0x07),
	REG_BIT_INIT("PD_VENC1_STATE",        0x1F,  8, 0x07),
	REG_BIT_INIT("PD_VDEC_STATE",         0x1F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg10[] = {
	REG_BIT_INIT("PD_AI_STATE",           0x1F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg11[] = {
	REG_BIT_INIT("PD_PS_CP_STATE",        0x1F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg12[] = {
	REG_BIT_INIT("PD_PHY_CP_STATE",       0x1F, 16, 0x07),
	REG_BIT_INIT("PD_CR8_PHY_TOP_STATE",  0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg13[] = {
	REG_BIT_INIT("PD_TD_STATE",           0x1F, 16, 0x07),
	REG_BIT_INIT("PD_LW_PROC_STATE",      0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg14[] = {
	REG_BIT_INIT("PD_LCE_STATE",          0x1F,  0, 0x07),
	REG_BIT_INIT("PD_DPFEC_STATE",        0x1F,  8, 0x07),
	REG_BIT_INIT("PD_WCE_STATE",          0x1F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg15[] = {
	REG_BIT_INIT("PD_NR_DL_TOP_STATE",    0x1F, 16, 0x07),
	REG_BIT_INIT("PD_NR_PDS_STATE",       0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg16[] = {
	REG_BIT_INIT("PD_NR_SPP_STATE",       0x1F,  0, 0x07),
	REG_BIT_INIT("PD_NR_CST_STATE",       0x1F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg17[] = {
	REG_BIT_INIT("PD_UNI_TOP_STATE",      0x1F,  0, 0x07),
};

static struct reg_bit pwr_status_dbg18[] = {
	REG_BIT_INIT("PD_CDMA_PROC0_STATE",   0x1F,  8, 0x07),
};

static struct reg_bit pwr_status_dbg21[] = {
	REG_BIT_INIT("PD_AUDIO_STATE",        0x1F,  8, 0x07),
	REG_BIT_INIT("PD_AUD_CEVA_STATE",     0x1F, 16, 0x07),
};

static struct reg_bit pwr_status_dbg22[] = {
	REG_BIT_INIT("PD_IPA_STATE",          0x1F,  8, 0x07),
	REG_BIT_INIT("PD_DPU_DP_STATE",       0x1F, 16, 0x07),
	REG_BIT_INIT("PD_PCIE_STATE",         0x1F, 24, 0x07),
};

static struct reg_bit pwr_status_dbg23[] = {
	REG_BIT_INIT("PD_ISE_STATE",          0x1F,  8, 0x07),
	REG_BIT_INIT("PD_PUB_STATE",          0x1F, 24, 0x07),
};

static struct reg_bit deep_sleep_mon0[] = {
	REG_BIT_INIT("AP_DEEP_SLEEP",         0x01,  0, 1),
	REG_BIT_INIT("PS_CP_DEEP_SLEEP",      0x01,  1, 1),
	REG_BIT_INIT("PHY_CP_DEEP_SLEEP",     0x01,  2, 1),
	REG_BIT_INIT("AUDIO_DEEP_SLEEP",      0x01,  3, 1),
	REG_BIT_INIT("IPA_DEEP_SLEEP",        0x01,  4, 1),
	REG_BIT_INIT("ISE_DEEP_SLEEP",        0x01,  5, 1),
	REG_BIT_INIT("SP_DEEP_SLEEP",         0x01,  8, 1),
	REG_BIT_INIT("CH_DEEP_SLEEP",         0x01,  9, 1),
	REG_BIT_INIT("CDMA_PROC0_DEEP_SLEEP", 0x01, 16, 1),
};

static struct reg_bit deep_sleep_mon1[] = {
	REG_BIT_INIT("PUB_DEEP_SLEEP",        0x01,  0, 1),
};

static struct reg_bit light_sleep_mon0[] = {
	REG_BIT_INIT("AP_LIGHT_SLEEP",     0x01, 0, 1),
	REG_BIT_INIT("PS_CP_LIGHT_SLEEP",  0x01, 1, 1),
	REG_BIT_INIT("PHY_CP_LIGHT_SLEEP", 0x01, 2, 1),
	REG_BIT_INIT("AUDIO_LIGHT_SLEEP",  0x01, 3, 1),
	REG_BIT_INIT("IPA_LIGHT_SLEEP",    0x01, 4, 1),
	REG_BIT_INIT("ISE_LIGHT_SLEEP",    0x01, 5, 1),
	REG_BIT_INIT("AON_LIGHT_SLEEP",    0x01, 8, 1),
};

static struct reg_bit light_sleep_mon1[] = {
	REG_BIT_INIT("PUB_LIGHT_SLEEP",    0x01, 0, 1),
};

static struct reg_info pmu_apb[] = {
	REG_INFO_INIT("APCPU_MODE_STATE0",     0x0700, apcpu_mode_state0),
	REG_INFO_INIT("APCPU_MODE_STATE1",     0x0704, apcpu_mode_state1),
	REG_INFO_INIT("APCPU_MODE_STATE4",     0x0710, apcpu_mode_state4),
	REG_INFO_INIT("APCPU_PCHANNEL_STATE0", 0x0714, apcpu_pchannel_state0),
	REG_INFO_INIT("APCPU_PCHANNEL_STATE2", 0x071C, apcpu_pchannel_state2),
	REG_INFO_INIT("PWR_STATUS_DBG00",      0x04F0, pwr_status_dbg0),
	REG_INFO_INIT("PWR_STATUS_DBG01",      0x04F4, pwr_status_dbg1),
	REG_INFO_INIT("PWR_STATUS_DBG02",      0x04F8, pwr_status_dbg2),
	REG_INFO_INIT("PWR_STATUS_DBG04",      0x0500, pwr_status_dbg4),
	REG_INFO_INIT("PWR_STATUS_DBG05",      0x0504, pwr_status_dbg5),
	REG_INFO_INIT("PWR_STATUS_DBG06",      0x0508, pwr_status_dbg6),
	REG_INFO_INIT("PWR_STATUS_DBG07",      0x050C, pwr_status_dbg7),
	REG_INFO_INIT("PWR_STATUS_DBG08",      0x0510, pwr_status_dbg8),
	REG_INFO_INIT("PWR_STATUS_DBG09",      0x0514, pwr_status_dbg9),
	REG_INFO_INIT("PWR_STATUS_DBG10",      0x0518, pwr_status_dbg10),
	REG_INFO_INIT("PWR_STATUS_DBG11",      0x051C, pwr_status_dbg11),
	REG_INFO_INIT("PWR_STATUS_DBG12",      0x0520, pwr_status_dbg12),
	REG_INFO_INIT("PWR_STATUS_DBG13",      0x0524, pwr_status_dbg13),
	REG_INFO_INIT("PWR_STATUS_DBG14",      0x0528, pwr_status_dbg14),
	REG_INFO_INIT("PWR_STATUS_DBG15",      0x052C, pwr_status_dbg15),
	REG_INFO_INIT("PWR_STATUS_DBG16",      0x0530, pwr_status_dbg16),
	REG_INFO_INIT("PWR_STATUS_DBG17",      0x0534, pwr_status_dbg17),
	REG_INFO_INIT("PWR_STATUS_DBG18",      0x0538, pwr_status_dbg18),
	REG_INFO_INIT("PWR_STATUS_DBG21",      0x0544, pwr_status_dbg21),
	REG_INFO_INIT("PWR_STATUS_DBG22",      0x0548, pwr_status_dbg22),
	REG_INFO_INIT("PWR_STATUS_DBG23",      0x054C, pwr_status_dbg23),
	REG_INFO_INIT("DEEP_SLEEP_MON0",       0x0850, deep_sleep_mon0),
	REG_INFO_INIT("DEEP_SLEEP_MON1",       0x0854, deep_sleep_mon1),
	REG_INFO_INIT("LIGHT_SLEEP_MON0",      0x0858, light_sleep_mon0),
	REG_INFO_INIT("LIGHT_SLEEP_MON1",      0x085C, light_sleep_mon1),
};

/*
 * Register table
 */
static struct reg_table reg_table_monitor[] = {
	REG_TABLE_INIT("PMU_APB", "sprd,sys-pmu-apb", pmu_apb),
};

/**
 * Wakeup source information
 */
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
		pr_err("%s: Node(%u:%u) is exist\n", __func__, gic_num, sub_num);
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
	#define EIC_EXT_NUM			6
	#define EIC_TYPE_NUM			4
	#define EIC_NUM				8

	static const char *eic_index[EIC_EXT_NUM] = {
		"AON_EIC_0", "AON_EIC_1", "AON_EIC_2",
		"AON_EIC_3", "AON_EIC_4", "AON_EIC_5"
	};

	static const char *eic_type[EIC_TYPE_NUM] = {
		"DBNC", "LATCH", "ASYNC", "SYNC"
	};

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
	#define MBOX_SRC_NUM		7

	static const char *mbox_src[MBOX_SRC_NUM] = {
		"APCPU", "SP CMStar", "CH CMStar", "PSCP CR8",
		"PHYCP CR8", "AUD DSP", "APCPU"
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
	#define ANA_INT_NUM			11
	#define ANA_EIC_IDX			4
	#define ANA_EIC_NUM			7

	static const char *ana_int[ANA_INT_NUM] = {
		"ADC_INT", "RTC_INT", "WDG_INT", "FGU_INT", "EIC_INT",
		"FAST_CHG_INT", "TMR_INT", "NULL", "TYPEC_INT",
		"USB_PD_INT", "CHG_INT"
	};

	static const char *ana_eic[ANA_EIC_NUM] = {
		"CHGR_INT", "PBINT", "PBINT2", "BATDET_OK",
		"KEY2_7S_EXT_RSTN", "EXT_XTL0_EN",
		"AUD_INT_ALL"
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
	INT_HANDLE_INIT(8,  gpio_int_handler),
	INT_HANDLE_INIT(12, mbox_int_handler),
	INT_HANDLE_INIT(29, eic_int_handler),
};

static struct int_handle ap_intc2_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc3_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc4_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc5_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc6_handle[] = {
	/* Empty */
};

static struct int_handle ap_intc7_handle[] = {
	INT_HANDLE_INIT(6, ana_int_handler),
};

static struct int_handle_set ap_intc_handle_set[] = {
	INT_HANDLE_SET_INIT(ap_intc0_handle),
	INT_HANDLE_SET_INIT(ap_intc1_handle),
	INT_HANDLE_SET_INIT(ap_intc2_handle),
	INT_HANDLE_SET_INIT(ap_intc3_handle),
	INT_HANDLE_SET_INIT(ap_intc4_handle),
	INT_HANDLE_SET_INIT(ap_intc5_handle),
	INT_HANDLE_SET_INIT(ap_intc6_handle),
	INT_HANDLE_SET_INIT(ap_intc7_handle),
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

struct plat_data sprd_sharkl6pro_debug_data = {
	.sleep_condition_table = NULL,
	.sleep_condition_table_num = 0,
	.subsys_state_table = reg_table_monitor,
	.subsys_state_table_num = 1,
	.wakeup_source_info = wakeup_source,
	.wakeup_source_info_num = 2,
	.wakeup_source_match = plat_match,
};
