// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/delay.h>
#if defined(CONFIG_EXYNOS_DSP_HW_O3)
#include <soc/samsung/exynos-smc.h>
#endif
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o3-system.h"
#include "hardware/dsp-reg.h"
#include "dsp-hw-o3-dump.h"
#include "dsp-hw-o3-memory.h"
#include "dsp-hw-o3-ctrl.h"

#if defined(CONFIG_EXYNOS_DSP_HW_O3) && !defined(ENABLE_DSP_VELOCE)
#define ENABLE_SECURE_READ
#define ENABLE_SECURE_WRITE
#endif

#define VPD_CPU_CONFIG	(0x10863600)

static struct dsp_reg_format o3_sfr_reg[] = {
#ifdef ENABLE_DSP_O3_REG_IAC_S
	{ 0x80000, 0x0000, REG_SEC_RW, 0, 0, "IAC_S_IAC_CTRL" },
	{ 0x80000, 0x0004, REG_SEC_RW, 0, 0, "IAC_S_IAC_IRQ" },
	{ 0x80000, 0x0008, REG_SEC_RW, 0, 0, "IAC_S_IAC_STACK_ADDR" },
	{ 0x80000, 0x000C, REG_SEC_RW, 0, 0, "IAC_S_IAC_STATUS" },
	{ 0x80000, 0x0010, REG_SEC_RW, 0, 0, "IAC_S_IAC_IRQ_CNT" },
	{ 0x80000, 0x0014, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_STATUS_TO_CC" },
	{ 0x80000, 0x0018, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_MSTATUS_TO_CC" },
	{ 0x80000, 0x001C, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_ENABLE_TO_CC" },
	{ 0x80000, 0x0020, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_SET_TO_CC" },
	{ 0x80000, 0x0024, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_CLR_TO_CC" },
	{ 0x80000, 0x0028, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_STATUS_TO_IAC" },
	{ 0x80000, 0x002C, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_MSTATUS_TO_IAC" },
	{ 0x80000, 0x0030, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_ENABLE_TO_IAC" },
	{ 0x80000, 0x0034, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_SET_TO_IAC" },
	{ 0x80000, 0x0038, REG_SEC_RW, 0, 0, "IAC_S_IRQ_SWI_CLR_TO_IAC" },
	{ 0x80000, 0x003C, REG_SEC_RW, 0, 0, "IAC_S_IRQ_MBOX_ENABLE_TO_CC" },
	{ 0x80000, 0x0040, REG_SEC_RW, 0, 0, "IAC_S_IRQ_MBOX_STATUS_TO_CC" },
	{ 0x80000, 0x0044, REG_SEC_RW, 0, 0, "IAC_S_IRQ_MBOX_MSTATUS_TO_CC" },
	{ 0x80000, 0x0048, REG_SEC_RW, 0, 0, "IAC_S_MBOX_IAC_TO_CC_INTR" },
	{ 0x80000, 0x004C, REG_SEC_RW, 0, 4, "IAC_S_MBOX_IAC_TO_CC$" },
	{ 0x80000, 0x0070, REG_SEC_RW, 0, 0, "IAC_S_MBOX_IVP0_TH0_TO_CC_INTR" },
	{ 0x80000, 0x0074, REG_SEC_RW, 0, 4, "IAC_S_MBOX_IVP0_TH0_TO_CC$" },
	{ 0x80000, 0x0098, REG_SEC_RW, 0, 0, "IAC_S_MBOX_IVP0_TH1_TO_CC_INTR" },
	{ 0x80000, 0x009C, REG_SEC_RW, 0, 4, "IAC_S_MBOX_IVP0_TH1_TO_CC$" },
	{ 0x80000, 0x00C0, REG_SEC_RW, 0, 0, "IAC_S_MBOX_IVP1_TH0_TO_CC_INTR" },
	{ 0x80000, 0x00C4, REG_SEC_RW, 0, 4, "IAC_S_MBOX_IVP1_TH0_TO_CC$" },
	{ 0x80000, 0x00E8, REG_SEC_RW, 0, 0, "IAC_S_MBOX_IVP1_TH1_TO_CC_INTR" },
	{ 0x80000, 0x00EC, REG_SEC_RW, 0, 4, "IAC_S_MBOX_IVP1_TH1_TO_CC$" },
#endif // ENABLE_DSP_O3_REG_IAC_S
#ifdef ENABLE_DSP_O3_REG_AAREG_S
	{ 0x82000, 0x0000, REG_SEC_RW, 0, 4, "AAREG_S_SEMAPHORE_REG$" },
#endif // ENABLE_DSP_O3_REG_AAREG_S
#ifdef ENABLE_DSP_O3_REG_IAC_NS
	{ 0x84000, 0x0000, 0, 0, 0, "IAC_NS_SRESET" },
	{ 0x84000, 0x0004, 0, 0, 0, "IAC_NS_IAC_CTRL" },
	{ 0x84000, 0x0008, 0, 0, 0, "IAC_NS_IAC_IRQ" },
	{ 0x84000, 0x000C, 0, 0, 0, "IAC_NS_IAC_STACK_ADDR" },
	{ 0x84000, 0x0010, 0, 0, 0, "IAC_NS_IAC_STATUS" },
	{ 0x84000, 0x0014, 0, 0, 0, "IAC_NS_IAC_IRQ_CNT" },
	{ 0x84000, 0x0018, 0, 0, 0, "IAC_NS_IAC_SLVERRORADDR_PM" },
	{ 0x84000, 0x001C, 0, 0, 0, "IAC_NS_IAC_SLVERRORADDR_DM" },
	{ 0x84000, 0x0020, 0, 0, 0, "IAC_NS_IAC_PC" },
	{ 0x84000, 0x0024, 0, 0, 0, "IAC_NS_IAC_EPC0" },
	{ 0x84000, 0x0028, 0, 0, 0, "IAC_NS_IAC_EPC1" },
	{ 0x84000, 0x002C, 0, 0, 0, "IAC_NS_IAC_EPC2" },
	{ 0x84000, 0x0030, 0, 0, 0, "IAC_NS_IAC_EPC3" },
	{ 0x84000, 0x0034, 0, 0, 0, "IAC_NS_DBG_ENABLE" },
	{ 0x84000, 0x0038, 0, 0, 0, "IAC_NS_IRQ_DBG_STATUS_FR_PBOX" },
	{ 0x84000, 0x003C, 0, 0, 0, "IAC_NS_IRQ_DBG_MSTATUS_FR_PBOX" },
	{ 0x84000, 0x0040, 0, 0, 0, "IAC_NS_IRQ_DBG_ENABLE_FR_PBOX" },
	{ 0x84000, 0x0044, 0, 0, 0, "IAC_NS_IRQ_DBG_CLR_FR_PBOX" },
	{ 0x84000, 0x0048, 0, 0, 0, "IAC_NS_IRQ_SWI_STATUS_TO_CC" },
	{ 0x84000, 0x004C, 0, 0, 0, "IAC_NS_IRQ_SWI_MSTATUS_TO_CC" },
	{ 0x84000, 0x0050, 0, 0, 0, "IAC_NS_IRQ_SWI_ENABLE_TO_CC" },
	{ 0x84000, 0x0054, 0, 0, 0, "IAC_NS_IRQ_SWI_SET_TO_CC" },
	{ 0x84000, 0x0058, 0, 0, 0, "IAC_NS_IRQ_SWI_CLR_TO_CC" },
	{ 0x84000, 0x005C, 0, 0, 0, "IAC_NS_IRQ_SWI_STATUS_TO_IAC" },
	{ 0x84000, 0x0060, 0, 0, 0, "IAC_NS_IRQ_SWI_MSTATUS_TO_IAC" },
	{ 0x84000, 0x0064, 0, 0, 0, "IAC_NS_IRQ_SWI_ENABLE_TO_IAC" },
	{ 0x84000, 0x0068, 0, 0, 0, "IAC_NS_IRQ_SWI_SET_TO_IAC" },
	{ 0x84000, 0x006C, 0, 0, 0, "IAC_NS_IRQ_SWI_CLR_TO_IAC" },
	{ 0x84000, 0x0070, 0, 0, 0, "IAC_NS_IRQ_MBOX_ENABLE_TO_CC" },
	{ 0x84000, 0x0074, 0, 0, 0, "IAC_NS_IRQ_MBOX_STATUS_TO_CC" },
	{ 0x84000, 0x0078, 0, 0, 0, "IAC_NS_IRQ_MBOX_MSTATUS_TO_CC" },
	{ 0x84000, 0x007C, 0, 0, 0, "IAC_NS_MBOX_IAC_TO_CC_INTR" },
	{ 0x84000, 0x0080, 0, 0, 4, "IAC_NS_MBOX_IAC_TO_CC$" },
	{ 0x84000, 0x00A4, 0, 0, 0, "IAC_NS_MBOX_IVP0_TH0_TO_CC_INTR" },
	{ 0x84000, 0x00A8, 0, 0, 4, "IAC_NS_MBOX_IVP0_TH0_TO_CC$" },
	{ 0x84000, 0x00CC, 0, 0, 0, "IAC_NS_MBOX_IVP0_TH1_TO_CC_INTR" },
	{ 0x84000, 0x00D0, 0, 0, 4, "IAC_NS_MBOX_IVP0_TH1_TO_CC$" },
	{ 0x84000, 0x00F4, 0, 0, 0, "IAC_NS_MBOX_IVP1_TH0_TO_CC_INTR" },
	{ 0x84000, 0x00F8, 0, 0, 4, "IAC_NS_MBOX_IVP1_TH0_TO_CC$" },
	{ 0x84000, 0x011C, 0, 0, 0, "IAC_NS_MBOX_IVP1_TH1_TO_CC_INTR" },
	{ 0x84000, 0x0120, 0, 0, 4, "IAC_NS_MBOX_IVP1_TH1_TO_CC$" },
#endif // ENABLE_DSP_O3_REG_IAC_NS
#ifdef ENABLE_DSP_O3_REG_AAREG_NS
	{ 0x86000, 0x0000, 0, 0, 4, "AAREG_NS_SEMAPHORE_REG$" },
#endif // ENABLE_DSP_O3_REG_AAREG_NS
#ifdef ENABLE_DSP_O3_REG_GIC
	{ 0x100000, 0x1000, 0, 0, 0, "GICD_CTLR" },
	{ 0x100000, 0x1004, 0, 0, 0, "GICD_TYPER" },
	{ 0x100000, 0x1008, 0, 0, 0, "GICD_IIDR" },
	{ 0x100000, 0x1080, 0, 0, 0, "GICD_IGROUPR0" },
	{ 0x100000, 0x1084, 0, 0, 4, "GICD_IGROUPR$" },
	{ 0x100000, 0x1100, 0, 0, 0, "GICD_ISENABLER0" },
	{ 0x100000, 0x1104, 0, 0, 4, "GICD_ISENABLER$" },
	{ 0x100000, 0x1180, 0, 0, 0, "GICD_ICENABLER0" },
	{ 0x100000, 0x1184, 0, 0, 4, "GICD_ICENABLER$" },
	{ 0x100000, 0x1200, 0, 0, 0, "GICD_ISPENDR0" },
	{ 0x100000, 0x1204, 0, 0, 4, "GICD_ISPENDR$" },
	{ 0x100000, 0x1280, 0, 0, 0, "GICD_ICPENDR0" },
	{ 0x100000, 0x1284, 0, 0, 4, "GICD_ICPENDR$" },
	{ 0x100000, 0x1300, 0, 0, 0, "GICD_ISACTIVER0" },
	{ 0x100000, 0x1304, 0, 0, 4, "GICD_ISACTIVER$" },
	{ 0x100000, 0x1380, 0, 0, 0, "GICD_ICACTIVER0" },
	{ 0x100000, 0x1384, 0, 0, 4, "GICD_ICACTIVER$" },
	{ 0x100000, 0x1400, 0, 0, 4, "GICD_IPRIORITYR_SGI$" },
	{ 0x100000, 0x1418, 0, 0, 0, "GICD_IPRIORITYR_PPI0" },
	{ 0x100000, 0x141C, 0, 0, 0, "GICD_IPRIORITYR_PPI1" },
	{ 0x100000, 0x1420, 0, 0, 4, "GICD_IPRIORITYR_SPI$" },
	{ 0x100000, 0x1800, 0, 0, 4, "GICD_ITARGETSR_SGI$" },
	{ 0x100000, 0x1818, 0, 0, 0, "GICD_ITARGETSR_PPI0" },
	{ 0x100000, 0x181c, 0, 0, 0, "GICD_ITARGETSR_PPI1" },
	{ 0x100000, 0x1820, 0, 0, 4, "GICD_ITARGETSR_SPI$" },
	{ 0x100000, 0x1C00, 0, 0, 0, "GICD_ICFGR_SGI" },
	{ 0x100000, 0x1C04, 0, 0, 0, "GICD_ICFGR_PPI" },
	{ 0x100000, 0x1C08, 0, 0, 4, "GICD_ICFGR_SPI$" },
	{ 0x100000, 0x1D00, 0, 0, 0, "GICD_PPISR" },
	{ 0x100000, 0x1D04, 0, 0, 4, "GICD_SPISRn$" },
	{ 0x100000, 0x1F00, 0, 0, 0, "GICD_SGIR" },
	{ 0x100000, 0x1F10, 0, 0, 4, "GICD_CPENDSGIR$" },
	{ 0x100000, 0x1F20, 0, 0, 4, "GICD_SPENDSGIR$" },
	{ 0x100000, 0x2000, 0, 0, 0, "GICC_CTLR" },
	{ 0x100000, 0x2004, 0, 0, 0, "GICC_PMR" },
	{ 0x100000, 0x2008, 0, 0, 0, "GICC_BPR" },
	{ 0x100000, 0x200C, 0, 0, 0, "GICC_IAR" },
	{ 0x100000, 0x2010, 0, 0, 0, "GICC_EOIR" },
	{ 0x100000, 0x2014, 0, 0, 0, "GICC_RPR" },
	{ 0x100000, 0x2018, 0, 0, 0, "GICC_HPPIR" },
	{ 0x100000, 0x201c, 0, 0, 0, "GICC_ABPR" },
	{ 0x100000, 0x2020, 0, 0, 0, "GICC_AIAR" },
	{ 0x100000, 0x2024, 0, 0, 0, "GICC_AEOIR" },
	{ 0x100000, 0x2028, 0, 0, 0, "GICC_AHPPIR" },
	{ 0x100000, 0x20D0, 0, 0, 0, "GICC_APR0" },
	{ 0x100000, 0x20E0, 0, 0, 0, "GICC_NSAPR0" },
	{ 0x100000, 0x20FC, 0, 0, 0, "GICC_IIDR" },
	{ 0x100000, 0x3000, 0, 0, 0, "GICC_DIR" },
#endif // ENABLE_DSP_O3_REG_GIC
#ifdef ENABLE_DSP_O3_REG_NPUS
	{ 0x110000, 0x0000, REG_SEC_RW, 0, 0, "NPUS_USER_REG0" },
	{ 0x110000, 0x0004, REG_SEC_RW, 0, 0, "NPUS_USER_REG1" },
	{ 0x110000, 0x0008, REG_SEC_RW, 0, 0, "NPUS_USER_REG2" },
	{ 0x110000, 0x000C, REG_SEC_RW, 0, 0, "NPUS_USER_REG3" },
	{ 0x110000, 0x0010, REG_SEC_RW, 0, 0, "NPUS_USER_REG4" },
	{ 0x110000, 0x0100, REG_SEC_RW, 0, 0, "NPUS_SFR_APB" },
	{ 0x110000, 0x0104, REG_SEC_RW, 0, 0, "NPUS_DBGL1RST_DISABLE" },
	{ 0x110000, 0x0108, REG_SEC_RW, 0, 0, "NPUS_CFGTE0" },
	{ 0x110000, 0x0808, REG_SEC_RW, 0, 0, "NPUS_CFGTE1" },
	{ 0x110000, 0x010C, REG_SEC_RW, 0, 0, "NPUS_VINITHI0" },
	{ 0x110000, 0x080C, REG_SEC_RW, 0, 0, "NPUS_VINITHI1" },
	{ 0x110000, 0x0110, REG_SEC_RW, 0, 0, "NPUS_CFGEND0" },
	{ 0x110000, 0x0810, REG_SEC_RW, 0, 0, "NPUS_CFGEND1" },
	{ 0x110000, 0x011C, REG_SEC_RW, 0, 0, "NPUS_PMUEVENT0" },
	{ 0x110000, 0x081C, REG_SEC_RW, 0, 0, "NPUS_PMUEVENT1" },
	{ 0x110000, 0x0120, REG_SEC_RW, 0, 0, "NPUS_EDBGRQ0" },
	{ 0x110000, 0x0820, REG_SEC_RW, 0, 0, "NPUS_EDBGRQ1" },
	{ 0x110000, 0x0124, REG_SEC_RW, 0, 0, "NPUS_EVENTI" },
	{ 0x110000, 0x0128, REG_SEC_RW, 0, 0, "NPUS_STANDBY_WFI0" },
	{ 0x110000, 0x0828, REG_SEC_RW, 0, 0, "NPUS_STANDBY_WFI1" },
	{ 0x110000, 0x012C, REG_SEC_RW, 0, 0, "NPUS_STANDBY_WFE0" },
	{ 0x110000, 0x082C, REG_SEC_RW, 0, 0, "NPUS_STANDBY_WFE1" },
	{ 0x110000, 0x0130, REG_SEC_RW, 0, 0, "NPUS_STANDBY_WFIL2" },
	{ 0x110000, 0x0134, REG_SEC_RW, 0, 0, "NPUS_CP15S_DISABLE20" },
	{ 0x110000, 0x0834, REG_SEC_RW, 0, 0, "NPUS_CP15S_DISABLE21" },
	{ 0x110000, 0x0138, REG_SEC_RW, 0, 0, "NPUS_PRESETDBG_CTRL" },
	{ 0x110000, 0x0140, REG_SEC_RW, 0, 0, "NPUS_PRESETDBG" },
	{ 0x110000, 0x0144, REG_SEC_RW, 0, 0, "NPUS_L2RESET_CTRL" },
	{ 0x110000, 0x0148, REG_SEC_RW, 0, 0, "NPUS_L2RESET" },
	{ 0x110000, 0x014C, REG_SEC_RW, 0, 0, "NPUS_CFGS_DISABLE" },
	{ 0x110000, 0x0300, REG_SEC_RW, 0, 0, "NPUS_CORE0_S" },
	{ 0x110000, 0x0304, REG_SEC_RW, 0, 0, "NPUS_CORE1_S" },
	{ 0x110000, 0x0308, REG_SEC_RW, 0, 0, "NPUS_CORE2_S" },
	{ 0x110000, 0x030C, REG_SEC_RW, 0, 0, "NPUS_CORE3_S" },
	{ 0x110000, 0x0310, REG_SEC_RW, 0, 0, "NPUS_SHR_SRAM0_S" },
	{ 0x110000, 0x0314, REG_SEC_RW, 0, 0, "NPUS_SHR_SRAM1_S" },
	{ 0x110000, 0x0318, REG_SEC_RW, 0, 0, "NPUS_SHR_SRAM2_S" },
	{ 0x110000, 0x031C, REG_SEC_RW, 0, 0, "NPUS_SHR_SRAM3_S" },
	{ 0x110000, 0x0320, REG_SEC_RW, 0, 0, "NPUS_SDMA_TOTAL_S" },
	{ 0x110000, 0x0324, REG_SEC_RW, 0, 0, "NPUS_RQGL_S" },
	{ 0x110000, 0x0328, REG_SEC_RW, 0, 0, "NPUS_SDMA_CM_S" },
	{ 0x110000, 0x032C, REG_SEC_RW, 0, 0, "NPUS_VC0_S" },
	{ 0x110000, 0x0330, REG_SEC_RW, 0, 0, "NPUS_VC1_S" },
	{ 0x110000, 0x0334, REG_SEC_RW, 0, 0, "NPUS_VC2_S" },
	{ 0x110000, 0x0338, REG_SEC_RW, 0, 0, "NPUS_VC3_S" },
	{ 0x110000, 0x033C, REG_SEC_RW, 0, 0, "NPUS_VC4_S" },
	{ 0x110000, 0x0340, REG_SEC_RW, 0, 0, "NPUS_VC5_S" },
	{ 0x110000, 0x0344, REG_SEC_RW, 0, 0, "NPUS_VC6_S" },
	{ 0x110000, 0x0348, REG_SEC_RW, 0, 0, "NPUS_VC7_S" },
	{ 0x110000, 0x034C, REG_SEC_RW, 0, 0, "NPUS_VC8_S" },
	{ 0x110000, 0x0350, REG_SEC_RW, 0, 0, "NPUS_VC9_S" },
	{ 0x110000, 0x0354, REG_SEC_RW, 0, 0, "NPUS_VC10_S" },
	{ 0x110000, 0x0358, REG_SEC_RW, 0, 0, "NPUS_VC11_S" },
	{ 0x110000, 0x035C, REG_SEC_RW, 0, 0, "NPUS_VC12_S" },
	{ 0x110000, 0x0360, REG_SEC_RW, 0, 0, "NPUS_VC13_S" },
	{ 0x110000, 0x0364, REG_SEC_RW, 0, 0, "NPUS_VC14_S" },
	{ 0x110000, 0x0368, REG_SEC_RW, 0, 0, "NPUS_VC15_S" },
	{ 0x110000, 0x036C, REG_SEC_RW, 0, 0, "NPUS_VC16_S" },
	{ 0x110000, 0x0370, REG_SEC_RW, 0, 0, "NPUS_VC17_S" },
	{ 0x110000, 0x0374, REG_SEC_RW, 0, 0, "NPUS_VC18_S" },
	{ 0x110000, 0x0378, REG_SEC_RW, 0, 0, "NPUS_VC19_S" },
	{ 0x110000, 0x037C, REG_SEC_RW, 0, 0, "NPUS_VC20_S" },
	{ 0x110000, 0x0380, REG_SEC_RW, 0, 0, "NPUS_VC21_S" },
	{ 0x110000, 0x0384, REG_SEC_RW, 0, 0, "NPUS_VC22_S" },
	{ 0x110000, 0x0388, REG_SEC_RW, 0, 0, "NPUS_VC23_S" },
	{ 0x110000, 0x038C, REG_SEC_RW, 0, 0, "NPUS_C2A0_S_GATING" },
	{ 0x110000, 0x0390, REG_SEC_RW, 0, 0, "NPUS_C2A1_S_GATING" },
	{ 0x110000, 0x0394, REG_SEC_RW, 0, 0, "NPUS_C2A0_S_IP_AXI" },
	{ 0x110000, 0x0398, REG_SEC_RW, 0, 0, "NPUS_C2A1_S_IP_AXI" },
	{ 0x110000, 0x039C, REG_SEC_RW, 0, 0, "NPUS_C2A0_S_OVERRIDE" },
	{ 0x110000, 0x0400, REG_SEC_RW, 0, 0, "NPUS_C2A1_S_OVERRIDE" },
	{ 0x110000, 0x0404, REG_SEC_RW, 0, 0, "NPUS_C2A0_S_OVERRIDE_SEL" },
	{ 0x110000, 0x0408, REG_SEC_RW, 0, 0, "NPUS_C2A1_S_OVERRIDE_SEL" },
	{ 0x110000, 0x0480, REG_SEC_RW, 0, 0, "NPUS_C2A0_SWRESET" },
	{ 0x110000, 0x0484, REG_SEC_RW, 0, 0, "NPUS_C2A1_SWRESET" },
	{ 0x110000, 0x0500, REG_SEC_RW, 0, 0, "NPUS_SSRAM_DEBUG_EN" },
	{ 0x110000, 0x0504, REG_SEC_RW, 0, 0, "NPUS_SSRAM_CLK_GATE_DISABLE" },
	{ 0x110000, 0x0508, REG_SEC_RW, 0, 0, "NPUS_STM_ENABLE" },
	{ 0x110000, 0x050C, REG_SEC_RW, 0, 0, "NPUS_CPU_PC_VALUE0" },
	{ 0x110000, 0x0510, REG_SEC_RW, 0, 0, "NPUS_PDMA_BOOT_FROM_PC" },
	{ 0x110000, 0x0514, REG_SEC_RW, 0, 0, "NPUS_PDMA_BOOT_ADDR" },
	{ 0x110000, 0x0518, REG_SEC_RW, 0, 0, "NPUS_PDMA_BOOT_MGR_NS" },
	{ 0x110000, 0x051C, REG_SEC_RW, 0, 0, "NPUS_PDMA_BOOT_IRQ_NS" },
	{ 0x110000, 0x0520, REG_SEC_RW, 0, 0, "NPUS_PDMA_BOOT_PERI_NS" },
	{ 0x110000, 0x0600, REG_SEC_RW, 0, 0, "NPUS_QOS_CPU" },
	{ 0x110000, 0x0604, REG_SEC_RW, 0, 0, "NPUS_QOS_CMDQ" },
	{ 0x110000, 0x0608, REG_SEC_RW, 0, 0, "NPUS_QOS_SDMA_TCM" },
	{ 0x110000, 0x060C, REG_SEC_RW, 0, 0, "NPUS_QOS_C2A" },
	{ 0x110000, 0x0610, REG_SEC_RW, 0, 0, "NPUS_AXI_DSP" },
	{ 0x110000, 0x0700, REG_SEC_RW, 0, 0, "NPUS_CPU_REMAP_EN" },
	{ 0x110000, 0x0704, REG_SEC_RW, 0, 0, "NPUS_CPU_REMAP_SRC" },
	{ 0x110000, 0x0708, REG_SEC_RW, 0, 0, "NPUS_CPU_REMAP_DEST" },
	{ 0x110000, 0x070C, REG_SEC_RW, 0, 0, "NPUS_CPU_PC_VALUE1" },
	{ 0x110000, 0x0280, REG_SEC_RW, 0, 0, "NPUS_INTC_CONTROL" },
	{ 0x110000, 0x0284, REG_SEC_RW, 0, 0, "NPUS_INTC0_IGROUP" },
	{ 0x110000, 0x0288, REG_SEC_RW, 0, 0, "NPUS_INTC0_IEN_SET" },
	{ 0x110000, 0x028C, REG_SEC_RW, 0, 0, "NPUS_INTC0_IEN_CLR" },
	{ 0x110000, 0x0290, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPEND" },
	{ 0x110000, 0x0294, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIO_PEND" },
	{ 0x110000, 0x0298, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIORITY0" },
	{ 0x110000, 0x029C, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIORITY1" },
	{ 0x110000, 0x02A0, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIORITY2" },
	{ 0x110000, 0x02A4, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIORITY3" },
	{ 0x110000, 0x02A8, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIO_S_PEND" },
	{ 0x110000, 0x02AC, REG_SEC_RW, 0, 0, "NPUS_INTC0_IPRIO_NS_PEND" },
	{ 0x110000, 0x02B4, REG_SEC_RW, 0, 0, "NPUS_INTC1_IGROUP" },
	{ 0x110000, 0x02B8, REG_SEC_RW, 0, 0, "NPUS_INTC1_IEN_SET" },
	{ 0x110000, 0x02BC, REG_SEC_RW, 0, 0, "NPUS_INTC1_IEN_CLR" },
	{ 0x110000, 0x02C0, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPEND" },
	{ 0x110000, 0x02C4, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIO_PEND" },
	{ 0x110000, 0x02C8, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIORITY0" },
	{ 0x110000, 0x02CC, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIORITY1" },
	{ 0x110000, 0x02D0, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIORITY2" },
	{ 0x110000, 0x02D4, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIORITY3" },
	{ 0x110000, 0x02D8, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIO_S_PEND" },
	{ 0x110000, 0x02DC, REG_SEC_RW, 0, 0, "NPUS_INTC1_IPRIO_NS_PEND" },
#endif // ENABLE_DSP_O3_REG_NPUS
#ifdef ENABLE_DSP_O3_REG_SDMA
	{ 0x130000, 0x0000, 0, 0, 0, "SDMA_VERSION" },
	{ 0x130000, 0x0004, 0, 0, 0, "SDMA_SRESET" },
	{ 0x130000, 0x0008, 0, 0, 0, "SDMA_CLOCK_GATE_EN" },
	{ 0x130000, 0x000C, 0, 0, 0, "SDMA_CLOCK_GATE_SE_SET_0" },
	{ 0x130000, 0x0010, 0, 0, 0, "SDMA_CLOCK_GATE_SE_SET_1" },
	{ 0x130000, 0x0040, 0, 0, 0, "SDMA_MO_CTRL_EXT" },
	{ 0x130000, 0x0044, 0, 0, 0, "SDMA_MO_CTRL_INT_RD" },
	{ 0x130000, 0x0048, 0, 0, 0, "SDMA_MO_CTRL_INT_WR" },
	{ 0x130000, 0x004C, 0, 0, 0, "SDMA_ARQOS_LUT" },
	{ 0x130000, 0x0050, 0, 0, 0, "SDMA_AWQOS_LUT" },
	{ 0x130000, 0x0080, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC0" },
	{ 0x130000, 0x0084, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC1" },
	{ 0x130000, 0x0088, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC2" },
	{ 0x130000, 0x008C, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC3" },
	{ 0x130000, 0x0090, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC4" },
	{ 0x130000, 0x0094, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC5" },
	{ 0x130000, 0x0098, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC6" },
	{ 0x130000, 0x009C, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC7" },
	{ 0x130000, 0x00A0, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC8" },
	{ 0x130000, 0x00A4, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC9" },
	{ 0x130000, 0x00A8, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC10" },
	{ 0x130000, 0x00AC, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC11" },
	{ 0x130000, 0x00B0, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC12" },
	{ 0x130000, 0x00B4, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC13" },
	{ 0x130000, 0x00B8, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC14" },
	{ 0x130000, 0x00BC, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC15" },
	{ 0x130000, 0x00C0, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC16" },
	{ 0x130000, 0x00C4, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC17" },
	{ 0x130000, 0x00C8, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC18" },
	{ 0x130000, 0x00CC, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC19" },
	{ 0x130000, 0x00D0, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC20" },
	{ 0x130000, 0x00D4, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC21" },
	{ 0x130000, 0x00D8, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC22" },
	{ 0x130000, 0x00DC, 0, 0, 0, "SDMA_DESCRIPTOR_QUEUE_CFG_VC23" },
	{ 0x130000, 0x00F0, 0, 0, 0, "SDMA_ERR_CODES" },
	{ 0x130000, 0x00F4, 0, 0, 0, "SDMA_ERR_MASK" },
	{ 0x130000, 0x00F8, 0, 0, 0, "SDMA_INTERRUPT_CM" },
	{ 0x130000, 0x00FC, 0, 0, 0, "SDMA_INTERRUPT_MASK_CM" },
	{ 0x130000, 0x0100, 0, 0, 0, "SDMA_INTERRUPT_DONE_NS" },
	{ 0x130000, 0x0104, 0, 0, 0, "SDMA_INTERRUPT_BLK_DONE_NS" },
	{ 0x130000, 0x0108, 0, 0, 0, "SDMA_INTERRUPT_FORCE_NS" },
	{ 0x130000, 0x010C, 0, 0, 0, "SDMA_INTERRUPT_GROUP_NS" },
	{ 0x130000, 0x0110, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC0" },
	{ 0x130000, 0x0114, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC1" },
	{ 0x130000, 0x0118, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC2" },
	{ 0x130000, 0x011C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC3" },
	{ 0x130000, 0x0120, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC4" },
	{ 0x130000, 0x0124, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC5" },
	{ 0x130000, 0x0128, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC6" },
	{ 0x130000, 0x012C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC7" },
	{ 0x130000, 0x0130, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC8" },
	{ 0x130000, 0x0134, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC9" },
	{ 0x130000, 0x0138, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC10" },
	{ 0x130000, 0x013C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC11" },
	{ 0x130000, 0x0140, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC12" },
	{ 0x130000, 0x0144, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC13" },
	{ 0x130000, 0x0148, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC14" },
	{ 0x130000, 0x014C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC15" },
	{ 0x130000, 0x0150, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC16" },
	{ 0x130000, 0x0154, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC17" },
	{ 0x130000, 0x0158, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC18" },
	{ 0x130000, 0x015C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC19" },
	{ 0x130000, 0x0160, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC20" },
	{ 0x130000, 0x0164, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC21" },
	{ 0x130000, 0x0168, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC22" },
	{ 0x130000, 0x016C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_NS_VC23" },
	{ 0x130000, 0x0180, 0, 0, 0, "SDMA_INTERRUPT_DONE_S" },
	{ 0x130000, 0x0184, 0, 0, 0, "SDMA_INTERRUPT_BLK_DONE_S" },
	{ 0x130000, 0x0188, 0, 0, 0, "SDMA_INTERRUPT_FORCE_S" },
	{ 0x130000, 0x018C, 0, 0, 0, "SDMA_INTERRUPT_GROUP_S" },
	{ 0x130000, 0x0190, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC0" },
	{ 0x130000, 0x0194, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC1" },
	{ 0x130000, 0x0198, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC2" },
	{ 0x130000, 0x019C, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC3" },
	{ 0x130000, 0x01A0, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC4" },
	{ 0x130000, 0x01A4, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC5" },
	{ 0x130000, 0x01A8, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC6" },
	{ 0x130000, 0x01AC, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC7" },
	{ 0x130000, 0x01B0, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC8" },
	{ 0x130000, 0x01B4, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC9" },
	{ 0x130000, 0x01B8, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC10" },
	{ 0x130000, 0x01BC, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC11" },
	{ 0x130000, 0x01C0, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC12" },
	{ 0x130000, 0x01C4, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC13" },
	{ 0x130000, 0x01C8, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC14" },
	{ 0x130000, 0x01CC, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC15" },
	{ 0x130000, 0x01D0, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC16" },
	{ 0x130000, 0x01D4, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC17" },
	{ 0x130000, 0x01D8, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC18" },
	{ 0x130000, 0x01DC, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC19" },
	{ 0x130000, 0x01E0, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC20" },
	{ 0x130000, 0x01E4, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC21" },
	{ 0x130000, 0x01E8, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC22" },
	{ 0x130000, 0x01EC, 0, 0, 0, "SDMA_INTERRUPT_QUEUE_S_VC23" },
	{ 0x130000, 0x0200, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC0" },
	{ 0x130000, 0x0204, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC0" },
	{ 0x130000, 0x0208, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC1" },
	{ 0x130000, 0x020C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC1" },
	{ 0x130000, 0x0210, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC2" },
	{ 0x130000, 0x0214, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC2" },
	{ 0x130000, 0x0218, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC3" },
	{ 0x130000, 0x021C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC3" },
	{ 0x130000, 0x0220, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC4" },
	{ 0x130000, 0x0224, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC4" },
	{ 0x130000, 0x0228, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC5" },
	{ 0x130000, 0x022C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC5" },
	{ 0x130000, 0x0230, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC6" },
	{ 0x130000, 0x0234, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC6" },
	{ 0x130000, 0x0238, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC7" },
	{ 0x130000, 0x023C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC7" },
	{ 0x130000, 0x0240, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC8" },
	{ 0x130000, 0x0244, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC8" },
	{ 0x130000, 0x0248, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC9" },
	{ 0x130000, 0x024C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC9" },
	{ 0x130000, 0x0250, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC10" },
	{ 0x130000, 0x0254, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC10" },
	{ 0x130000, 0x0258, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC11" },
	{ 0x130000, 0x025C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC11" },
	{ 0x130000, 0x0260, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC12" },
	{ 0x130000, 0x0264, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC12" },
	{ 0x130000, 0x0268, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC13" },
	{ 0x130000, 0x026C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC13" },
	{ 0x130000, 0x0270, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC14" },
	{ 0x130000, 0x0274, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC14" },
	{ 0x130000, 0x0278, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC15" },
	{ 0x130000, 0x027C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC15" },
	{ 0x130000, 0x0280, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC16" },
	{ 0x130000, 0x0284, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC16" },
	{ 0x130000, 0x0288, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC17" },
	{ 0x130000, 0x028C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC17" },
	{ 0x130000, 0x0290, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC18" },
	{ 0x130000, 0x0294, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC18" },
	{ 0x130000, 0x0298, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC19" },
	{ 0x130000, 0x029C, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC19" },
	{ 0x130000, 0x02A0, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC20" },
	{ 0x130000, 0x02A4, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC20" },
	{ 0x130000, 0x02A8, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC21" },
	{ 0x130000, 0x02AC, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC21" },
	{ 0x130000, 0x02B0, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC22" },
	{ 0x130000, 0x02B4, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC22" },
	{ 0x130000, 0x02B8, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_VC23" },
	{ 0x130000, 0x02BC, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_VC23" },
	{ 0x130000, 0x0300, 0, 0, 0, "SDMA_FLT_INFO_SET_CFG" },
	{ 0x130000, 0x0304, 0, 0, 0, "SDMA_FLT_INFO_MAX_SET0" },
	{ 0x130000, 0x0308, 0, 0, 0, "SDMA_FLT_INFO_MIN_SET0" },
	{ 0x130000, 0x030C, 0, 0, 0, "SDMA_FLT_INFO_SUM_SET0" },
	{ 0x130000, 0x0310, 0, 0, 0, "SDMA_FLT_INFO_SUM2_SET0" },
	{ 0x130000, 0x0314, 0, 0, 0, "SDMA_FLT_INFO_MAX_SET1" },
	{ 0x130000, 0x0318, 0, 0, 0, "SDMA_FLT_INFO_MIN_SET1" },
	{ 0x130000, 0x031C, 0, 0, 0, "SDMA_FLT_INFO_SUM_SET1" },
	{ 0x130000, 0x0320, 0, 0, 0, "SDMA_FLT_INFO_SUM2_SET1" },
	{ 0x130000, 0x0324, 0, 0, 0, "SDMA_FLT_INFO_MAX_SET2" },
	{ 0x130000, 0x0328, 0, 0, 0, "SDMA_FLT_INFO_MIN_SET2" },
	{ 0x130000, 0x032C, 0, 0, 0, "SDMA_FLT_INFO_SUM_SET2" },
	{ 0x130000, 0x0330, 0, 0, 0, "SDMA_FLT_INFO_SUM2_SET2" },
	{ 0x130000, 0x0334, 0, 0, 0, "SDMA_FLT_INFO_MAX_SET3" },
	{ 0x130000, 0x0338, 0, 0, 0, "SDMA_FLT_INFO_MIN_SET3" },
	{ 0x130000, 0x033C, 0, 0, 0, "SDMA_FLT_INFO_SUM_SET3" },
	{ 0x130000, 0x0340, 0, 0, 0, "SDMA_FLT_INFO_SUM2_SET3" },
	{ 0x130000, 0x0500, 0, 0, 0, "SDMA_STATUS_SE_0" },
	{ 0x130000, 0x0504, 0, 0, 0, "SDMA_STATUS_SE_1" },
	{ 0x130000, 0x0508, 0, 0, 0, "SDMA_STATUS_INT_SOURCE_ERR" },
	{ 0x130000, 0x050C, 0, 0, 0, "SDMA_STATUS_INT_SOURCE_DONE" },
	{ 0x130000, 0x0510, 0, 0, 0, "SDMA_STATUS_INT_SOURCE_BLK_DONE" },
	{ 0x130000, 0x0514, 0, 0, 0, "SDMA_STATUS_INT_SOURCE_GROUP" },
	{ 0x130000, 0x0518, 0, 0, 0, "SDMA_STATUS_VC_INFO_VC0_7" },
	{ 0x130000, 0x051C, 0, 0, 0, "SDMA_STATUS_VC_INFO_VC8_15" },
	{ 0x130000, 0x0520, 0, 0, 0, "SDMA_STATUS_VC_INFO_VC16_23" },
	{ 0x130000, 0x0524, 0, 0, 0, "SDMA_STATUS_TCM0_SDMA_MUTEX" },
	{ 0x130000, 0x0528, 0, 0, 0, "SDMA_STATUS_TCM1_SDMA_MUTEX" },
	{ 0x130000, 0x052C, 0, 0, 0, "SDMA_TCM0_TRANSFER_ON_UPDATE_VC" },
	{ 0x130000, 0x0530, 0, 0, 0, "SDMA_TCM1_TRANSFER_ON_UPDATE_VC" },
	{ 0x130000, 0x0534, 0, 0, 0, "SDMA_NSECURE" },
	{ 0x130000, 0x0700, 0, 0, 0, "SDMA_DEBUG_ADDR" },
	{ 0x130000, 0x0704, 0, 0, 0, "SDMA_DEBUG_REG0" },
#endif // ENABLE_DSP_O3_REG_SDMA
#ifdef ENABLE_DSP_O3_REG_MBOX_EXT_NS
	{ 0x1A0000, 0x0000, 0, 0, 0, "MBOX_EXT_NS_MCUCTRL" },
	{ 0x1A0000, 0x0004, 0, 0, 0, "MBOX_EXT_NS_IS_VERSION" },
	{ 0x1A0000, 0x0008, 0, 0, 0, "MBOX_EXT_NS_INT_0_GR" },
	{ 0x1A0000, 0x000C, 0, 0, 0, "MBOX_EXT_NS_INT_0_CR" },
	{ 0x1A0000, 0x0010, 0, 0, 0, "MBOX_EXT_NS_INT_0_MR" },
	{ 0x1A0000, 0x0014, 0, 0, 0, "MBOX_EXT_NS_INT_0_SR" },
	{ 0x1A0000, 0x0018, 0, 0, 0, "MBOX_EXT_NS_INT_0_MSR" },
#endif // ENABLE_DSP_O3_REG_MBOX_EXT_NS
#ifdef ENABLE_DSP_O3_REG_VPD0_CTRL
	{ 0x400000, 0x0000, 0, 0, 0, "VPD0_MCGEN" },
	{ 0x400000, 0x0004, 0, 0, 0, "VPD0_IVP_SWRESET" },
	{ 0x400000, 0x0010, 0, 0, 0, "VPD0_PERF_MON_ENABLE" },
	{ 0x400000, 0x0014, 0, 0, 0, "VPD0_PERF_MON_CLEAR" },
	{ 0x400000, 0x0018, 0, 0, 0, "VPD0_DBG_MON_ENABLE" },
	{ 0x400000, 0x0020, 0, 0, 0, "VPD0_DBG_INTR_STATUS" },
	{ 0x400000, 0x0024, 0, 0, 0, "VPD0_DBG_INTR_ENABLE" },
	{ 0x400000, 0x0028, 0, 0, 0, "VPD0_DBG_INTR_CLEAR" },
	{ 0x400000, 0x002c, 0, 0, 0, "VPD0_DBG_INTR_MSTATUS" },
	{ 0x400000, 0x0030, 0, 0, 0, "VPD0_IVP_SFR2AXI_SGMO" },
	{ 0x400000, 0x0034, 0, 0, 0, "VPD0_CLOCK_CONFIG" },
	{ 0x400000, 0x0040, 0, 0, 8, "VPD0_VM_STACK_START$" },
	{ 0x400000, 0x0044, 0, 0, 8, "VPD0_VM_STACK_END$" },
	{ 0x400000, 0x0060, 0, 0, 0, "VPD0_VM_MODE" },
	{ 0x400000, 0x0080, 0, 0, 0, "VPD0_PERF_IVP0_TH0_PC" },
	{ 0x400000, 0x0084, 0, 0, 0, "VPD0_PERF_IVP0_TH0_VALID_CNTL" },
	{ 0x400000, 0x0088, 0, 0, 0, "VPD0_PERF_IVP0_TH0_VALID_CNTH" },
	{ 0x400000, 0x008c, 0, 0, 0, "VPD0_PERF_IVP0_TH0_STALL_CNT" },
	{ 0x400000, 0x0090, 0, 0, 0, "VPD0_PERF_IVP0_TH1_PC" },
	{ 0x400000, 0x0094, 0, 0, 0, "VPD0_PERF_IVP0_TH1_VALID_CNTL" },
	{ 0x400000, 0x0098, 0, 0, 0, "VPD0_PERF_IVP0_TH1_VALID_CNTH" },
	{ 0x400000, 0x009c, 0, 0, 0, "VPD0_PERF_IVP0_TH1_STALL_CNT" },
	{ 0x400000, 0x00a0, 0, 0, 0, "VPD0_PERF_IVP0_IC_REQL" },
	{ 0x400000, 0x00a4, 0, 0, 0, "VPD0_PERF_IVP0_IC_REQH" },
	{ 0x400000, 0x00a8, 0, 0, 0, "VPD0_PERF_IVP0_IC_MISS" },
	{ 0x400000, 0x00ac, 0, 0, 8, "VPD0_PERF_IVP0_INST_CNTL$" },
	{ 0x400000, 0x00b0, 0, 0, 8, "VPD0_PERF_IVP0_INST_CNTH$" },
	{ 0x400000, 0x00cc, 0, 0, 0, "VPD0_PERF_IVP1_TH0_PC" },
	{ 0x400000, 0x00d0, 0, 0, 0, "VPD0_PERF_IVP1_TH0_VALID_CNTL" },
	{ 0x400000, 0x00d4, 0, 0, 0, "VPD0_PERF_IVP1_TH0_VALID_CNTH" },
	{ 0x400000, 0x00d8, 0, 0, 0, "VPD0_PERF_IVP1_TH0_STALL_CNT" },
	{ 0x400000, 0x00dc, 0, 0, 0, "VPD0_PERF_IVP1_TH1_PC" },
	{ 0x400000, 0x00e0, 0, 0, 0, "VPD0_PERF_IVP1_TH1_VALID_CNTL" },
	{ 0x400000, 0x00e4, 0, 0, 0, "VPD0_PERF_IVP1_TH1_VALID_CNTH" },
	{ 0x400000, 0x00e8, 0, 0, 0, "VPD0_PERF_IVP1_TH1_STALL_CNT" },
	{ 0x400000, 0x00ec, 0, 0, 0, "VPD0_PERF_IVP1_IC_REQL" },
	{ 0x400000, 0x00f0, 0, 0, 0, "VPD0_PERF_IVP1_IC_REQH" },
	{ 0x400000, 0x00f4, 0, 0, 0, "VPD0_PERF_IVP1_IC_MISS" },
	{ 0x400000, 0x00f8, 0, 0, 8, "VPD0_PERF_IVP1_INST_CNTL$" },
	{ 0x400000, 0x00fc, 0, 0, 8, "VPD0_PERF_IVP1_INST_CNTH$" },
	{ 0x400000, 0x0128, 0, 0, 0, "VPD0_DBG_IVP0_ADDR_PM" },
	{ 0x400000, 0x012c, 0, 0, 0, "VPD0_DBG_IVP0_ADDR_DM" },
	{ 0x400000, 0x0130, 0, 0, 0, "VPD0_DBG_IVP0_ERROR_INFO" },
	{ 0x400000, 0x0134, 0, 4, 4, "VPD0_DBG_IVP0_TH0_ERROR_PC$" },
	{ 0x400000, 0x0144, 0, 4, 4, "VPD0_DBG_IVP0_TH1_ERROR_PC$" },
	{ 0x400000, 0x0154, 0, 0, 0, "VPD0_DBG_IVP1_ADDR_PM" },
	{ 0x400000, 0x0158, 0, 0, 0, "VPD0_DBG_IVP1_ADDR_DM" },
	{ 0x400000, 0x015c, 0, 0, 0, "VPD0_DBG_IVP1_ERROR_INFO" },
	{ 0x400000, 0x0160, 0, 4, 4, "VPD0_DBG_IVP1_TH0_ERROR_PC$" },
	{ 0x400000, 0x0170, 0, 4, 4, "VPD0_DBG_IVP1_TH1_ERROR_PC$" },
	{ 0x400000, 0x0180, 0, 0, 4, "VPD0_DBG_MPU_SMONITOR$" },
	{ 0x400000, 0x01c0, 0, 0, 4, "VPD0_DBG_MPU_EMONITOR$" },
	{ 0x400000, 0x0208, 0, 0, 0, "VPD0_AXI_ERROR_RD" },
	{ 0x400000, 0x020c, 0, 0, 0, "VPD0_AXI_ERROR_WR" },
	{ 0x400000, 0x0210, 0, 0, 4, "VPD0_RD_MOCNT$" },
	{ 0x400000, 0x0240, 0, 0, 4, "VPD0_WR_MOCNT$" },
	{ 0x400000, 0x0400, 0, 0, 0, "VPD0_IVP0_WAKEUP" },
	{ 0x400000, 0x0404, 0, 2, 4, "VPD0_IVP0_INTR_STATUS_TH$" },
	{ 0x400000, 0x040c, 0, 2, 4, "VPD0_IVP0_INTR_ENABLE_TH$" },
	{ 0x400000, 0x0414, 0, 2, 4, "VPD0_IVP0_SWI_SET_TH$" },
	{ 0x400000, 0x041c, 0, 2, 4, "VPD0_IVP0_SWI_CLEAR_TH$" },
	{ 0x400000, 0x0424, 0, 2, 4, "VPD0_IVP0_MASKED_STATUS_TH$" },
	{ 0x400000, 0x042c, 0, 0, 0, "VPD0_IVP0_IC_BASE_ADDR" },
	{ 0x400000, 0x0430, 0, 0, 0, "VPD0_IVP0_IC_CODE_SIZE" },
	{ 0x400000, 0x0434, 0, 0, 0, "VPD0_IVP0_IC_INVALID_REQ" },
	{ 0x400000, 0x0438, 0, 0, 0, "VPD0_IVP0_IC_INVALID_STATUS" },
	{ 0x400000, 0x043c, 0, 0, 8, "VPD0_IVP0_DM_STACK_START$" },
	{ 0x400000, 0x0440, 0, 0, 8, "VPD0_IVP0_DM_STACK_END$" },
	{ 0x400000, 0x044c, 0, 0, 0, "VPD0_IVP0_STATUS" },
	{ 0x400000, 0x0480, 0, 0, 0, "VPD0_IVP1_WAKEUP" },
	{ 0x400000, 0x0484, 0, 2, 4, "VPD0_IVP1_INTR_STATUS_TH$" },
	{ 0x400000, 0x048c, 0, 2, 4, "VPD0_IVP1_INTR_ENABLE_TH$" },
	{ 0x400000, 0x0494, 0, 2, 4, "VPD0_IVP1_SWI_SET_TH$" },
	{ 0x400000, 0x049c, 0, 2, 4, "VPD0_IVP1_SWI_CLEAR_TH$" },
	{ 0x400000, 0x04a4, 0, 2, 4, "VPD0_IVP1_MASKED_STATUS_TH$" },
	{ 0x400000, 0x04ac, 0, 0, 0, "VPD0_IVP1_IC_BASE_ADDR" },
	{ 0x400000, 0x04b0, 0, 0, 0, "VPD0_IVP1_IC_CODE_SIZE" },
	{ 0x400000, 0x04b4, 0, 0, 0, "VPD0_IVP1_IC_INVALID_REQ" },
	{ 0x400000, 0x04b8, 0, 0, 0, "VPD0_IVP1_IC_INVALID_STATUS" },
	{ 0x400000, 0x04bc, 0, 0, 8, "VPD0_IVP1_DM_STACK_START$" },
	{ 0x400000, 0x04c0, 0, 0, 8, "VPD0_IVP1_DM_STACK_END$" },
	{ 0x400000, 0x04cc, 0, 0, 0, "VPD0_IVP1_STATUS" },
	{ 0x400000, 0x0800, 0, 0, 0, "VPD0_IVP0_MAILBOX_INTR_TH0" },
	{ 0x400000, 0x0804, 0, 0, 4, "VPD0_IVP0_MAILBOX_TH0_$" },
	{ 0x400000, 0x0a00, 0, 0, 0, "VPD0_IVP0_MAILBOX_INTR_TH1" },
	{ 0x400000, 0x0a04, 0, 0, 4, "VPD0_IVP0_MAILBOX_TH1_$" },
	{ 0x400000, 0x0c00, 0, 0, 0, "VPD0_IVP1_MAILBOX_INTR_TH0" },
	{ 0x400000, 0x0c04, 0, 0, 4, "VPD0_IVP1_MAILBOX_TH0_$" },
	{ 0x400000, 0x0e00, 0, 0, 0, "VPD0_IVP1_MAILBOX_INTR_TH1" },
	{ 0x400000, 0x0e04, 0, 0, 4, "VPD0_IVP1_MAILBOX_TH1_$" },
	{ 0x400000, 0x1000, 0, 0, 4, "VPD0_IVP0_MSG_TH0_$" },
	{ 0x400000, 0x1080, 0, 0, 4, "VPD0_IVP0_MSG_TH1_$" },
	{ 0x400000, 0x1100, 0, 0, 4, "VPD0_IVP1_MSG_TH0_$" },
	{ 0x400000, 0x1180, 0, 0, 4, "VPD0_IVP1_MSG_TH1_$" },
#endif // ENABLE_DSP_O3_REG_VPD0_CTRL
};

static unsigned int dsp_hw_o3_ctrl_remap_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	int ret;
	void __iomem *regs;
	unsigned int val;

	dsp_enter();
	regs = devm_ioremap(ctrl->dev, addr, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap once(%#x, %d)\n", addr, ret);
		return 0xdead0010;
	}

	val = readl(regs);
	devm_iounmap(ctrl->dev, regs);
	dsp_leave();
	return val;
}

static int dsp_hw_o3_ctrl_remap_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val)
{
	int ret;
	void __iomem *regs;

	dsp_enter();
	regs = devm_ioremap(ctrl->dev, addr, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap once(%#x, %d)\n", addr, ret);
		return ret;
	}

	writel(val, regs);
	devm_iounmap(ctrl->dev, regs);
	dsp_leave();
	return 0;
}

static unsigned int dsp_hw_o3_ctrl_sm_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_addr)
{
	dsp_check();
	return readl(ctrl->sfr + reg_addr);
}

static int dsp_hw_o3_ctrl_sm_writel(struct dsp_ctrl *ctrl,
		unsigned int reg_addr, int val)
{
	dsp_enter();
	writel(val, ctrl->sfr + reg_addr);
	dsp_leave();
	return 0;
}

static unsigned int dsp_hw_o3_ctrl_dhcp_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_addr)
{
	int ret;
	struct dsp_priv_mem *pmem;
	unsigned int *sm;

	dsp_enter();
	pmem = &ctrl->sys->memory.priv_mem[DSP_O3_PRIV_MEM_DHCP];

	if (!pmem->kvaddr) {
		ret = 0xdead0010;
		dsp_warn("DHCP mem is not allocated yet\n");
		goto p_err;
	}

	if (reg_addr > pmem->size) {
		ret = 0xdead0011;
		dsp_err("address is outside DHCP mem range(%#x/%#zx)\n",
				reg_addr, pmem->size);
		goto p_err;
	}

	sm = pmem->kvaddr + reg_addr;
	dsp_leave();
	return *sm;
p_err:
	return ret;
}

static int dsp_hw_o3_ctrl_dhcp_writel(struct dsp_ctrl *ctrl,
		unsigned int reg_addr, int val)
{
	int ret;
	struct dsp_priv_mem *pmem;
	unsigned int *sm;

	dsp_enter();
	pmem = &ctrl->sys->memory.priv_mem[DSP_O3_PRIV_MEM_DHCP];

	if (!pmem->kvaddr) {
		ret = 0xdead0015;
		dsp_warn("DHCP mem is not allocated yet\n");
		goto p_err;
	}

	if (reg_addr > pmem->size) {
		ret = 0xdead0016;
		dsp_err("address is outside DHCP mem range(%#x/%#zx)\n",
				reg_addr, pmem->size);
		goto p_err;
	}

	sm = pmem->kvaddr + reg_addr;
	*sm = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static unsigned int __dsp_hw_o3_ctrl_secure_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	int ret;
	unsigned long val;

	dsp_enter();
#ifdef ENABLE_SECURE_READ
	ret = exynos_smc_readsfr(ctrl->sfr_pa + addr, &val);
	if (ret) {
		dsp_err("Failed to read secure sfr(%#x)(%d)\n", addr, ret);
		return 0xdeadc002;
	}
#else
	ret = 0;
	val = readl(ctrl->sfr + addr);
#endif
	dsp_leave();
	return val;
}

static int __dsp_hw_o3_ctrl_secure_writel(struct dsp_ctrl *ctrl, int val,
		unsigned int addr)
{
	int ret;

	dsp_enter();
#ifdef ENABLE_SECURE_WRITE
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(ctrl->sfr_pa + addr),
			val, 0x0);
	if (ret) {
		dsp_err("Failed to write secure sfr(%#x)(%d)\n", addr, ret);
		return ret;
	}
#else
	ret = 0;
	writel(val, ctrl->sfr + addr);
#endif
	dsp_leave();
	return 0;
}

static unsigned int dsp_hw_o3_ctrl_offset_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset)
{
	unsigned int addr;

	dsp_check();
	if (reg_id >= DSP_O3_REG_END) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, DSP_O3_REG_END);
		return -EINVAL;
	}

	if (o3_sfr_reg[reg_id].flag & REG_SEC_DENY)
		return 0xdeadc000;
	else if (o3_sfr_reg[reg_id].flag & REG_WRONLY)
		return 0xdeadc001;

	addr = o3_sfr_reg[reg_id].base + o3_sfr_reg[reg_id].offset +
		offset * o3_sfr_reg[reg_id].interval;

	if (o3_sfr_reg[reg_id].flag & REG_SEC_R)
		return __dsp_hw_o3_ctrl_secure_readl(ctrl, addr);
	else
		return readl(ctrl->sfr + addr);
}

static int dsp_hw_o3_ctrl_offset_writel(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset, int val)
{
	unsigned int addr;

	dsp_enter();
	if (reg_id >= DSP_O3_REG_END) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, DSP_O3_REG_END);
		return -EINVAL;
	}

	if (o3_sfr_reg[reg_id].flag & (REG_SEC_DENY | REG_WRONLY))
		return -EACCES;

	addr = o3_sfr_reg[reg_id].base + o3_sfr_reg[reg_id].offset +
		offset * o3_sfr_reg[reg_id].interval;

	if (o3_sfr_reg[reg_id].flag & REG_SEC_W)
		__dsp_hw_o3_ctrl_secure_writel(ctrl, val, addr);
	else
		writel(val, ctrl->sfr + addr);
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_common_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CPU_REMAP_SRC, 0, 0x0);
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CPU_REMAP_DEST, 0,
			0x20000000);
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CPU_REMAP_EN, 0, 0x1);

	/* set non-secure VPD0 / IAC / Shared SRAM */
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CORE1_S, 0, 0x1);
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CORE2_S, 0, 0xff);
	ctrl->ops->offset_writel(ctrl, DSP_O3_NPUS_CORE3_S, 0, 0xffff);

	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_extra_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	// TODO check 'divide by zero'
	ctrl->ops->offset_writel(ctrl, DSP_O3_VPD0_DBG_MON_ENABLE, 0, 0xf);
	ctrl->ops->offset_writel(ctrl, DSP_O3_VPD0_DBG_INTR_ENABLE, 0, 0x7fff5);
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_all_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	ctrl->ops->common_init(ctrl);
	ctrl->ops->extra_init(ctrl);
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_start(struct dsp_ctrl *ctrl)
{
	dsp_enter();
#ifdef ENABLE_DSP_VELOCE
	/* disable S2MPU */
	ctrl->ops->remap_writel(ctrl, 0x18870000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x188a0000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x188d0000, 0x0);

	/* set TZPC */
	ctrl->ops->remap_writel(ctrl, 0x18810214, 0x3c3c);
#endif
	ctrl->ops->remap_writel(ctrl, VPD_CPU_CONFIG, 0x1);
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_reset(struct dsp_ctrl *ctrl)
{
	int ret;
	unsigned int wfi, wfe;
	unsigned int repeat = 1000;

	dsp_enter();
	while (1) {
		wfi = ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_STANDBY_WFI0, 0);
		wfe = ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_STANDBY_WFE0, 0);
		if (wfi & 0x1 || wfe & 0x1 || !repeat)
			break;
		repeat--;
		udelay(100);
	};

	if (!(wfi & 0x1 || wfe & 0x1)) {
		dsp_err("status of device is not idle(%#x/%#x)\n", wfi, wfe);
		ret = -ETIMEDOUT;
		dsp_dump_ctrl();
		goto p_err;
	}

	ctrl->ops->remap_writel(ctrl, VPD_CPU_CONFIG, 0x0);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o3_ctrl_force_reset(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	// TODO add force reset
	ctrl->ops->reset(ctrl);
	dsp_leave();
	return 0;
}

static void dsp_hw_o3_ctrl_reg_print(struct dsp_ctrl *ctrl, unsigned int reg_id)
{
	unsigned int addr, count;
	unsigned int reg_count, interval;

	dsp_enter();
	if (reg_id >= DSP_O3_REG_END) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, DSP_O3_REG_END);
		return;
	}

	addr = o3_sfr_reg[reg_id].base + o3_sfr_reg[reg_id].offset;
	reg_count = o3_sfr_reg[reg_id].count;

	if (reg_count) {
		for (count = 0; count < reg_count; ++count) {
			interval = o3_sfr_reg[reg_id].interval;
			dsp_notice("[%4d][%5x][%4x][%2x][%40s] %8x\n",
					reg_id, addr + count * interval,
					o3_sfr_reg[reg_id].flag, count,
					o3_sfr_reg[reg_id].name,
					ctrl->ops->offset_readl(ctrl, reg_id,
						count));
		}
	} else {
		dsp_notice("[%4d][%5x][%4x][--][%40s] %8x\n",
				reg_id, addr, o3_sfr_reg[reg_id].flag,
				o3_sfr_reg[reg_id].name,
				ctrl->ops->offset_readl(ctrl, reg_id, 0));
	}
	dsp_leave();
}

static void __dsp_hw_o3_ctrl_pc_dump(struct dsp_ctrl *ctrl, int idx)
{
	dsp_notice("[%2d][%22s] %8x [%22s] %8x\n",
			idx, o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE0].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE0, 0),
			o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE1].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE1, 0));
	dsp_notice("[%2d][%22s] %8x [%22s] %8x\n",
			idx, o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE0].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE0, 0),
			o3_sfr_reg[DSP_O3_IAC_NS_IAC_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_IAC_NS_IAC_PC, 0));
	dsp_notice("[%2d][%22s] %8x [%22s] %8x\n",
			idx, o3_sfr_reg[DSP_O3_VPD0_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP0_TH0_PC, 0),
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP0_TH1_PC, 0));
	dsp_notice("[%2d][%22s] %8x [%22s] %8x\n",
			idx, o3_sfr_reg[DSP_O3_VPD0_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP1_TH0_PC, 0),
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP1_TH1_PC, 0));
}

static void dsp_hw_o3_ctrl_pc_dump(struct dsp_ctrl *ctrl)
{
	int idx, repeat = 8;

	dsp_enter();
	dsp_notice("pc register dump (repeat:%d)\n", repeat);
	for (idx = 0; idx < repeat; ++idx)
		__dsp_hw_o3_ctrl_pc_dump(ctrl, idx);

	dsp_leave();
}

static void dsp_hw_o3_ctrl_dhcp_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("dhcp mem dump (count:%d)\n", DSP_O3_DHCP_USED_COUNT);
	for (idx = 0; idx < DSP_O3_DHCP_USED_COUNT; idx += 4) {
		addr = DSP_O3_DHCP_IDX(idx);
		val0 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx));
		val1 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 1));
		val2 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 2));
		val3 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 3));

		dsp_notice("[DHCP_INFO(%#06x-%#06x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_userdefined_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("userdefined dump (count:%d)\n",
			DSP_O3_SM_USERDEFINED_COUNT);
	for (idx = 0; idx < DSP_O3_SM_USERDEFINED_COUNT; idx += 4) {
		addr = DSP_O3_SM_USERDEFINED(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_USERDEFINED(idx));
		val1 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 3));

		dsp_notice("[USERDEFINED(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_fw_info_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("fw_info dump (count:%d)\n", DSP_O3_SM_FW_INFO_COUNT);
	for (idx = 0; idx < DSP_O3_SM_FW_INFO_COUNT; idx += 4) {
		addr = DSP_O3_SM_FW_INFO(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx));
		val1 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 3));

		dsp_notice("[FW_INFO(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_dump(struct dsp_ctrl *ctrl)
{
	int idx;

	dsp_enter();
	dsp_notice("register dump (count:%d)\n", DSP_O3_REG_END);
	for (idx = 0; idx < DSP_O3_REG_END; ++idx)
		ctrl->ops->reg_print(ctrl, idx);

	ctrl->ops->dhcp_dump(ctrl);
	ctrl->ops->userdefined_dump(ctrl);
	ctrl->ops->fw_info_dump(ctrl);
	ctrl->ops->pc_dump(ctrl);
	dsp_leave();
}

static void dsp_hw_o3_ctrl_user_reg_print(struct dsp_ctrl *ctrl,
		struct seq_file *file, unsigned int reg_id)
{
	unsigned int addr, count;
	unsigned int reg_count, interval;

	dsp_enter();
	if (reg_id >= DSP_O3_REG_END) {
		seq_printf(file, "register id is invalid(%u/%u)\n",
				reg_id, DSP_O3_REG_END);
		return;
	}

	addr = o3_sfr_reg[reg_id].base + o3_sfr_reg[reg_id].offset;
	reg_count = o3_sfr_reg[reg_id].count;

	if (reg_count) {
		for (count = 0; count < reg_count; ++count) {
			interval = o3_sfr_reg[reg_id].interval;
			seq_printf(file, "[%4d][%5x][%2x][%2x][%40s] %8x\n",
					reg_id, addr + count * interval,
					o3_sfr_reg[reg_id].flag, count,
					o3_sfr_reg[reg_id].name,
					ctrl->ops->offset_readl(ctrl, reg_id,
						count));
		}
	} else {
		seq_printf(file, "[%4d][%5x][%2x][--][%40s] %8x\n",
				reg_id, addr, o3_sfr_reg[reg_id].flag,
				o3_sfr_reg[reg_id].name,
				ctrl->ops->offset_readl(ctrl, reg_id, 0));
	}
	dsp_leave();
}

static void __dsp_hw_o3_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file, int idx)
{
	seq_printf(file, "[%2d][%22s] %8x [%22s] %8x\n",
			idx,
			o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE0].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE0, 0),
			o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE1].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE1, 0));
	seq_printf(file, "[%2d][%22s] %8x [%22s] %8x\n",
			idx,
			o3_sfr_reg[DSP_O3_NPUS_CPU_PC_VALUE0].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_NPUS_CPU_PC_VALUE0, 0),
			o3_sfr_reg[DSP_O3_IAC_NS_IAC_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_IAC_NS_IAC_PC, 0));
	seq_printf(file, "[%2d][%22s] %8x [%22s] %8x\n",
			idx,
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP0_TH0_PC, 0),
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP0_TH1_PC, 0));
	seq_printf(file, "[%2d][%22s] %8x [%22s] %8x\n",
			idx,
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP1_TH0_PC, 0),
			o3_sfr_reg[DSP_O3_VPD0_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_O3_VPD0_PERF_IVP1_TH1_PC, 0));
}

static void dsp_hw_o3_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx, repeat = 8;

	dsp_enter();
	seq_printf(file, "pc register dump (repeat:%d)\n", repeat);
	for (idx = 0; idx < repeat; ++idx)
		__dsp_hw_o3_ctrl_user_pc_dump(ctrl, file, idx);

	dsp_leave();
}

static void dsp_hw_o3_ctrl_user_dhcp_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "dhcp mem dump (count:%d)\n", DSP_O3_DHCP_USED_COUNT);
	for (idx = 0; idx < DSP_O3_DHCP_USED_COUNT; idx += 4) {
		addr = DSP_O3_DHCP_IDX(idx);
		val0 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx));
		val1 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 1));
		val2 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 2));
		val3 = ctrl->ops->dhcp_readl(ctrl, DSP_O3_DHCP_IDX(idx + 3));

		seq_printf(file, "[DHCP_INFO(%#06x-%#06x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_user_userdefined_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "userdefined dump (count:%d)\n",
			DSP_O3_SM_USERDEFINED_COUNT);
	for (idx = 0; idx < DSP_O3_SM_USERDEFINED_COUNT; idx += 4) {
		addr = DSP_O3_SM_USERDEFINED(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_USERDEFINED(idx));
		val1 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl,
				DSP_O3_SM_USERDEFINED(idx + 3));

		seq_printf(file, "[USERDEFINED(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_user_fw_info_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "fw_info dump (count:%d)\n", DSP_O3_SM_FW_INFO_COUNT);
	for (idx = 0; idx < DSP_O3_SM_FW_INFO_COUNT; idx += 4) {
		addr = DSP_O3_SM_FW_INFO(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx));
		val1 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl, DSP_O3_SM_FW_INFO(idx + 3));

		seq_printf(file, "[FW_INFO(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_o3_ctrl_user_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;

	dsp_enter();
	seq_printf(file, "register dump (count:%d)\n", DSP_O3_REG_END);

	for (idx = 0; idx < DSP_O3_REG_END; ++idx)
		ctrl->ops->user_reg_print(ctrl, file, idx);

	ctrl->ops->user_dhcp_dump(ctrl, file);
	ctrl->ops->user_userdefined_dump(ctrl, file);
	ctrl->ops->user_fw_info_dump(ctrl, file);
	ctrl->ops->user_pc_dump(ctrl, file);
	dsp_leave();
}

static int dsp_hw_o3_ctrl_open(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_close(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_ctrl_probe(struct dsp_system *sys)
{
	struct dsp_ctrl *ctrl;

	dsp_enter();
	ctrl = &sys->ctrl;
	ctrl->sys = sys;

	ctrl->dev = sys->dev;
	ctrl->sfr_pa = sys->sfr_pa;
	ctrl->sfr = sys->sfr;
	dsp_leave();
	return 0;
}

static void dsp_hw_o3_ctrl_remove(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_ctrl_ops o3_ctrl_ops = {
	.remap_readl		= dsp_hw_o3_ctrl_remap_readl,
	.remap_writel		= dsp_hw_o3_ctrl_remap_writel,
	.sm_readl		= dsp_hw_o3_ctrl_sm_readl,
	.sm_writel		= dsp_hw_o3_ctrl_sm_writel,
	.dhcp_readl		= dsp_hw_o3_ctrl_dhcp_readl,
	.dhcp_writel		= dsp_hw_o3_ctrl_dhcp_writel,
	.offset_readl		= dsp_hw_o3_ctrl_offset_readl,
	.offset_writel		= dsp_hw_o3_ctrl_offset_writel,

	.reg_print		= dsp_hw_o3_ctrl_reg_print,
	.dump			= dsp_hw_o3_ctrl_dump,
	.pc_dump		= dsp_hw_o3_ctrl_pc_dump,
	.dhcp_dump		= dsp_hw_o3_ctrl_dhcp_dump,
	.userdefined_dump	= dsp_hw_o3_ctrl_userdefined_dump,
	.fw_info_dump		= dsp_hw_o3_ctrl_fw_info_dump,

	.user_reg_print		= dsp_hw_o3_ctrl_user_reg_print,
	.user_dump		= dsp_hw_o3_ctrl_user_dump,
	.user_pc_dump		= dsp_hw_o3_ctrl_user_pc_dump,
	.user_dhcp_dump		= dsp_hw_o3_ctrl_user_dhcp_dump,
	.user_userdefined_dump	= dsp_hw_o3_ctrl_user_userdefined_dump,
	.user_fw_info_dump	= dsp_hw_o3_ctrl_user_fw_info_dump,

	.common_init		= dsp_hw_o3_ctrl_common_init,
	.extra_init		= dsp_hw_o3_ctrl_extra_init,
	.all_init		= dsp_hw_o3_ctrl_all_init,
	.start			= dsp_hw_o3_ctrl_start,
	.reset			= dsp_hw_o3_ctrl_reset,
	.force_reset		= dsp_hw_o3_ctrl_force_reset,

	.open			= dsp_hw_o3_ctrl_open,
	.close			= dsp_hw_o3_ctrl_close,
	.probe			= dsp_hw_o3_ctrl_probe,
	.remove			= dsp_hw_o3_ctrl_remove,
};

int dsp_hw_o3_ctrl_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_ctrl_register_ops(DSP_DEVICE_ID_O3, &o3_ctrl_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
