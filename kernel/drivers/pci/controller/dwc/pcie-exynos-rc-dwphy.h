/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PCIE_EXYNOS_RC_DWCPHY_H
#define __PCIE_EXYNOS_RC_DWCPHY_H

/* Root PCIe Sub-System Custom Registers */
#define PCIE_SSC_APB_CLKREQ_TOUT	0x4
#define SSC_CFG_APB_TIMER_DIS		BIT(10)

#define PCIE_SSC_RST_CTRL		0x48
#define SSC_RST_CTRL_COLD_RESET		BIT(0)
#define SSC_RST_CTRL_WARM_RESET		BIT(1)
#define SSC_RST_CTRL_PHY_RESET		BIT(2)

#define PCIE_SSC_DBG_SIGANL_1		0x1000
#define SSC_DBG_SIG1_0			(0)
#define SSC_DBG_SIG1_1			(8)
#define SSC_DBG_SIG1_2			(16)
#define SSC_DBG_SIG1_3			(24)

#define PCIE_SSC_GEN_CTRL_3		0x1058
#define SSC_GEN_CTRL_3_LTSSM_EN		BIT(0)
#define SSC_GEN_CTRL_3_SEL_GROUP_MASK	GENMASK(27, 24)
#define SSC_GEN_CTRL_3_SEL_LANE_MASK	GENMASK(31, 28)

#define PCIE_SSC_GEN_CTRL_4		0x105C
#define SSC_GEN_CTRL_4_LTSSM_EN_CLR	BIT(30)

#define PCIE_SSC_PM_CTRL		0x1060
#define SSC_PM_CTRL_PME_PF_INDEX	GENMASK(4, 0)
#define SSC_PM_CTRL_PM_PME_REQ		BIT(16)
#define SSC_PM_CTRL_ENTER_ASPM_L1	BIT(17)
#define SSC_PM_CTRL_EXIT_ASPM_L1	BIT(18)
#define SSC_PM_CTRL_READY_ENTER_L23	BIT(19)
#define SSC_PM_CTRL_CLK_REQ		BIT(20)
#define SSC_PM_CTRL_APP_CLK_PM_EN	BIT(21)
#define SSC_APP_L1SUB_DISABLE		BIT(22)

#define PCIE_SSC_PM_STS			0x1064
#define SSC_PM_STS_ELECIDLE_DIS		BIT(19)
#define SSC_PM_STS_TCOMMON_MODE_DIS	BIT(20)
#define SSC_PM_STS_L1SS_CHECK_MASK	GENMASK(20, 19)


#define PCIE_SSC_TX_MSG_REQ		0x1080
#define SSC_TX_MSG_REQ_VEN_MSG		BIT(17)
#define SSC_TX_MSG_REQ_PME_TURN_OFF	BIT(19)
#define SSC_TX_MSG_REQ_UNLOCK		BIT(20)

#define PCIE_SSC_RX_MSG_STS		0x10A0
#define SSC_RX_MSG_STS_PME_TO_ACK_STS	BIT(11)
#define SSC_RX_MSG_STS_PM_PME_STS	BIT(12)
#define SSC_RX_MSG_STS_LTR_STS		BIT(16)
#define SSC_RX_MSG_STS_VDM_TYPE1_STS	BIT(18)
#define SSC_RX_MSG_STS_PME_TURN_OFF_STS	BIT(19)
#define SSC_RX_MSG_STS_UNLOCK_STS	BIT(20)

#define PCIE_SSC_LINK_DBG_2		0x10B4
#define SSC_LINK_DBG_2_LTSSM_STATE	GENMASK(5, 0)
#define SSC_LINK_DBG_2_SLV_XFER_PEND	BIT(23)

#define PCIE_SSC_ERR_INT_STS		0x10E0
#define SSC_ERR_STS_SYS_ERR		BIT(0)
#define SSC_ERR_STS_ARE_ERR		BIT(8)
#define SSC_ERR_STS_QOVERFLOW_ERR	GENMASK(23, 16)
#define SSC_ERR_STS_SEND_COR_ERR	BIT(25)
#define SSC_ERR_STS_SEND_NF_ERR		BIT(26)
#define SSC_ERR_STS_SEND_F_ERR		BIT(27)
#define SSC_ERR_STS_RDLH_LINKUP		BIT(28)
#define SSC_ERR_STS_RADM_CPL_TIMEOUT	BIT(29)
#define SSC_ERR_STS_LINK_DOWN_EVENT	BIT(30)
#define SSC_ERR_STS_APB_TIMEOUT		BIT(31)

#define PCIE_SSC_ERR_INT_CTL		0x10E4
#define SSC_ERR_CTL_SYS_ERR_EN		BIT(0)
#define SSC_ERR_CTL_ARE_ERR_EN		BIT(8)
#define SSC_ERR_CTL_QOVERFLOW_ERR_EN	BIT(16)
#define SSC_ERR_CTL_SEND_COR_ERR_EN	BIT(25)
#define SSC_ERR_CTL_SEND_NF_ERR_EN	BIT(26)
#define SSC_ERR_CTL_SEND_F_ERR_EN	BIT(27)
#define SSC_ERR_CTL_RDLH_LINKUP_EN	BIT(28)
#define SSC_ERR_CTL_RADM_CPL_TIMEOUT_EN	BIT(29)
#define SSC_ERR_CTL_LINK_DOWN_EVENT_EN	BIT(30)
#define SSC_ERR_CTL_APB_TIMEOUT_EN	BIT(31)

#define PCIE_SSC_INT_STSCTL		0x10E8
#define SSC_INT_STS_RX_WAKE_UP		BIT(0)
#define SSC_INT_STS_HP_PME		BIT(1)
#define SSC_INT_STS_ATU_SLOC_MATCH	BIT(3)
#define SSC_INT_STS_BUS_MASTER_EN	BIT(4)
#define SSC_INT_STS_SYNC_PERST_N	BIT(5)
#define SSC_INT_STS_L12_ENTRY		BIT(6)
#define SSC_INT_STS_L12_EXIT		BIT(7)
#define SSC_INT_STS_RX_MSG_INT		BIT(9)
#define SSC_INT_STS_ERR_INT		BIT(10)
#define SSC_INT_STS_CFG_CAP_INT		BIT(11)
#define SSC_INT_STS_INTD		BIT(12)
#define SSC_INT_STS_INTC		BIT(13)
#define SSC_INT_STS_INTB		BIT(14)
#define SSC_INT_STS_INTA		BIT(15)
#define SSC_INT_CTL_RX_WAKE_UP		BIT(16)
#define SSC_INT_CTL_HP_PME		BIT(17)
#define SSC_INT_CTL_ATU_SLOC_MATCH	BIT(19)
#define SSC_INT_CTL_BUS_MASTER_EN	BIT(20)
#define SSC_INT_CTL_SYNC_PERST_N	BIT(21)
#define SSC_INT_CTL_L12_ENTRY		BIT(22)
#define SSC_INT_CTL_L12_EXIT		BIT(23)
#define SSC_INT_CTL_INTD		BIT(28)
#define SSC_INT_CTL_INTC		BIT(29)
#define SSC_INT_CTL_INTB		BIT(30)
#define SSC_INT_CTL_INTA		BIT(31)

#define PCIE_SSC_PHY_REFCLK_CTRL	0x2014
#define SSC_REFCLK_CTRL_REF_USE_PAD	BIT(1)

#define PCIE_SSC_PHY_SRAM_CTRL		0x2018
#define SSC_PHY_SRAM_BYPASS_MODE	GENMASK(1, 0)
#define SSC_PHY_SRAM_BL_BYPASS_MODE	GENMASK(3, 2)
#define SSC_PHY_SRAM_EXT_LD_DONE	BIT(4)

/* Root PCIe SOC Control Registers */
#define PCIE_CTRL_MAC_PCLK_CTRL		0x8000
#define MAC_PCLK_SW_SWITCHING		BIT(0)

#define PCIE_CTRL_BUS_CTRL		0x8008
#define CTRL_SLV_BUS_NCLK_OFF		BIT(1)
#define CTRL_BUS_SLV_EN			BIT(0)

#define PCIE_CTRL_LINK_CTRL		0x800C
#define CTRL_LINK_CTRL_PERSTN_IN_SEL	BIT(2)
#define CTRL_LINK_CTRL_PERSTN_IN	BIT(1)

#define PCIE_CTRL_QCH_CTRL		0x8010
#define CTRL_QCH_CTRL_SELECTION		GENMASK(7, 0)
#define CTRL_QCH_CTRL_SLV_QCH_SEL	GENMASK(11, 10)
#define CTRL_QCH_CTRL_ENABLE_VAL	0x101077
#define CTRL_QCH_CTRL_DISABLE_VAL	0x101000

#define PCIE_CTRL_DTB_SEL		0x8014
#define CTRL_DTB_SEL_CLKREQ		(0x8 << 4);

#define PCIE_CTRL_POWER_OSC_CTRL	0x801C
#define CTRL_PWR_OSC_CTL_AUX_CG_EN	BIT(9)

#define PCIE_CTRL_DEBUG_MODE		0x9000
#define CTRL_DEBUG_MODE_DEBUG_EN	BIT(31)
#define CTRL_DEBUG_MODE_STORE_START	BIT(15)
#define CTRL_DEBUG_MODE_BUFFER_MODE_EN	BIT(13)
#define CTRL_DEBUG_MODE_STM_MODE_EN	BIT(12)
#define CTRL_DEBUG_MODE_RX_STAT		GENMASK(7, 4)
#define CTRL_DEBUG_MODE_RX_STAT_VAL	(0x3 << 4)

#define CTRL_DEBUG_MODE_CP_TRACE	GENMASK(11, 8)

#define PCIE_CTRL_DEBUG_SIGNAL_MASK	0x9004
#define CTRL_DEBUG_SIG_MASK_LTSSM	GENMASK(31, 24)
#define CTRL_DEBUG_SIG_MASK_L1SUB	GENMASK(23, 20)
#define CTRL_DEBUG_SIG_MASK_LOCK_SIG	GENMASK(19, 16)
#define CTRL_DEBUG_SIG_MASK_PHY_DTB	GENMASK(15, 12)
#define CTRL_DEBUG_SIG_MASK_PHY_DTB_PAT	GENMASK(15, 14)
#define CTRL_DEBUG_SIG_MASK_PHY_ESSEN	GENMASK(11, 8)
#define CTRL_DEBUG_SIG_MASK_PHY_FIXED	GENMASK(7, 4)
#define CTRL_DEBUG_SIG_MASK_PHY_DYNAMIC	GENMASK(3, 0)
#define CTRL_DEBUG_SIG_MASK_PHY_DYN_3B	GENMASK(2, 0)

#define PCIE_CTRL_DEBUG_INTERRUPT_EN	0x9010

#define PCIE_CTRL_DEBUG_INT_CLR		0x9018
#define CTRL_DEBUG_INT_CLR_SPD_FALLING	BIT(8)
#define CTRL_DEBUG_INT_CLR_SPD_RISING	BIT(7)

#define PCIE_CTRL_DEBUG_INT_DIR_GEN	0x9020
#define CTRL_DEBUG_INT_GEN_SPD_FALLING	GENMASK(31, 29)
#define CTRL_DEBUG_INT_GEN_SPD_RISING	GENMASK(28, 26)
#define INT_GEN_SPD_RISING_SHIFT	(26)

#define PCIE_CTRL_REFCLK_CTRL_OPT0	0xA200
#define CTRL_REFCLK_OPT0_PLL_EN		BIT(0)

#define PCIE_CTRL_REFCLK_CTRL_OPT1	0xA204
#define CTRL_REFCLK_OPT1_OVR_EN_PLL	BIT(0)

#define PCIE_CTRL_REFCLK_CTL_OPT3	0xA20C
#define CTRL_REFCLK_OPT3_REFCLK_N_L1SS	BIT(27)
#define CTRL_REFCLK_OPT3_L2_EN		BIT(26)
#define CTRL_REFCLK_OPT3_PG_EN		BIT(24)
#define CTRL_REFCLK_OPT3_PWRDN_L2	GENMASK(19, 16)
#define CTRL_REFCLK_OPT3_CLKREQ_L2	BIT(20)

/* PHY side CG checking counter */
#define PCIE_CTRL_REFCLK_CTL_OPT6	0xA218
#define CTRL_REFCLK_OPT6_DEFAULT_VAL	0x1
#define CTRL_REFCLK_OPT6_DELAY_VAL	0x200

/* PHY side CG checking counter */
#define PCIE_CTRL_REFCLK_CTL_OPT7	0xA21C
#define CTRL_REFCLK_OPT7_DEFAULT_VAL	0x1
#define CTRL_REFCLK_OPT7_DELAY_VAL	0x200

#define PCIE_CTRL_SS_RW_9		0xB024
#define CTRL_SS_RW_9_DEV_TYPE_MASK	GENMASK(12, 9)
#define CTRL_SS_RW_9_DEV_TYPE_RC	(0x4 << 9)

#define PCIE_CTRL_SS_RW_15		0xB03C
#define CTRL_SS_RW_15_PHY_PWRDN		BIT(11)

/* History Buffer */
#define PCIE_CTRL_DEBUG_REG(x)		(0x9200 + ((x) * 0x4))
#define CTRL_DEBUG_LTSSM_STATE(x)	(((x) >> 24) & 0xff)
#define CTRL_DEBUG_L1SUB_STATE(x)	(((x) >> 20) & 0xf)
#define CTRL_DEBUG_LOCK_SIG_STATE(x)	(((x) >> 16) & 0xf)
#define CTRL_DEBUG_PHY_DTB_STATE(x)	(((x) >> 12) & 0xf)
#define CTRL_DEBUG_PHY_ESSEN_STATE(x)	(((x) >> 8) & 0xf)
#define CTRL_DEBUG_PHY_FIX_STATE(x)	(((x) >> 4) & 0xf)
#define CTRL_DEBUG_PHY_DYN_STATE(x)	((x) & 0xf)

#define PCIE_CTRL_LINK_STATE		0xC054
#define CTRL_LINK_STATE_LTSSM		GENMASK(5, 0)
#define CTRL_LINK_STATE_PM_DSTATE	GENMASK(11, 9)
#define CTRL_LINK_STATE_L1SUB		GENMASK(19, 17)

/* DWPHY registers */
#define PCIE_DWPHY_MPLLB_VCO_OVRD_IN_0	0x54
#define DWPHY_VCO_OVRD_CP_INT_GS	GENMASK(6, 0)
#define DWPHY_VCO_OVRD_CP_INT_MASK	GENMASK(14, 8)
#define DWPHY_VCO_OVRD_CP_INT_GS_VAL	(0x42)
#define DWPHY_VCO_OVRD_CP_INT_GS_OVEN	BIT(7)
#define DWPHY_VCO_OVRD_CP_INT_VAL	(0x28 << 8)
#define DWPHY_VCO_OVRD_CP_INT_OVEN	BIT(15)

#define PCIE_DWPHY_MPLLB_VCO_OVRD_IN_1	0x58
#define DWPHY_VCO_OVRD_CP_PROP_GS	GENMASK(6, 0)
#define DWPHY_VCO_OVRD_CP_PROP_MASK	GENMASK(14, 8)
#define DWPHY_VCO_OVRD_CP_PROP_GS_VAL	(0x1D)
#define DWPHY_VCO_OVRD_CP_PROP_GS_OVEN	BIT(7)
#define DWPHY_VCO_OVRD_CP_PROP_VAL	(0x3C << 8)
#define DWPHY_VCO_OVRD_CP_PROP_OVEN	BIT(15)

#define PCIE_DWPHY_MPLLB_ANA_CREG00	0x378
#define DWPHY_MPLLB_ANA_SPO		BIT(13)

#define PCIE_DWPHY_RAWMEM_CMN38_B15_R23	0x337DC
#define DWPHY_RAWMEM_SPO_MASK_VAL	GENMASK(13, 7)
#define DWPHY_RAWMEM_SPO_SET_VAL	(0x1D)

#define PCIE_DWPHY_RAWMEM_CMN39_B9_R20	0x33CD0
#define DWPHY_GEN1_DCC_BYPASS		BIT(10)

#define PCIE_DWPHY_LANE1_OVRD_2		0x4908
#define DWPHY_LANE1_RXDET_OVRD_VAL	BIT(6)
#define DWPHY_LANE1_RXDET_OVRD_EN	BIT(7)

#define PCIE_DWPHY_RAWLANEON1_OVRD_IN_0	0xCF04
#define DWPHY_RX_TERM_OVRD_VAL		BIT(2)
#define DWPHY_RX_TERM_OVRD_EN		BIT(3)

/* For RX VCO */
#define PCIE_DWPHY_RX_VCO_CAL_TIME0_LN0	0x42B4
#define PCIE_DWPHY_RX_VCO_CAL_TIME0_LN1	0x4AB4
#define DWPHY_RX_VCO_CNTR_PWRUP_MASK	GENMASK(11, 4)
#define DWPHY_RX_VCO_CNTR_PWRUP_TIME	(0x5F << 4)
#define DWPHY_RX_VCO_UPDATE_MASK	GENMASK(3, 0)
#define DWPHY_RX_VCO_UPDATE_TIME	(0xC)

#define PCIE_DWPHY_RX_VCO_CAL_TIME1_LN0	0x42B8
#define PCIE_DWPHY_RX_VCO_CAL_TIME1_LN1	0x4AB8
#define DWPHY_RX_VCO_STARTUP_TIME_MASK	GENMASK(9, 3)
#define DWPHY_RX_VCO_STARTUP_TIME	(0x7F << 3)
#define DWPHY_RX_VCO_CNTR_SETTLE_MASK	GENMASK(2, 0)
#define DWPHY_RX_VCO_CNTR_SETTLE_TIME	(0x7)

#define PCIE_DWPHY_RAWMEM_CMN24_B0_R1	0x2C004
#define PCIE_DWPHY_RAWMEM_CMN24_B2_R1	0x2C104
#define DWPHY_RAWMEM_CMN24_BX_R1_VAL	(0x2AE)

#define PCIE_DWPHY_RAWMEM_CMN24_B0_R2	0x2C008
#define PCIE_DWPHY_RAWMEM_CMN24_B2_R2	0x2C108
#define DWPHY_RAWMEM_CMN24_BX_R2_VAL	(0x3FF)

#define PCIE_DWPHY_RAWMEM_CMN24_B0_R3	0x2C00C
#define PCIE_DWPHY_RAWMEM_CMN24_B2_R3	0x2C10C
#define DWPHY_RAWMEM_CMN24_BX_R3_VAL	(0x2AD)

#define PCIE_DWPHY_RAWMEM_CMN24_B0_R4	0x2C010
#define PCIE_DWPHY_RAWMEM_CMN24_B2_R4	0x2C110
#define DWPHY_RAWMEM_CMN24_BX_R4_VAL	(0x5FC)

#define PCIE_DWPHY_RAWMEM_CMN24_B0_R5	0x2C014
#define PCIE_DWPHY_RAWMEM_CMN24_B2_R5	0x2C114
#define DWPHY_RAWMEM_CMN24_BX_R5_VAL	(0x9016)

#define PCIE_DWPHY_MEM_CMN24_B8_R1	0x2C404
#define DWPHY_MEM_CMN24_B8_R1_VAL	(0xDE)

#define PCIE_DWPHY_MEM_CMN24_B8_R2	0x2C408
#define DWPHY_MEM_CMN24_B8_R2_VAL	(0x1019)

#define PCIE_DWPHY_MEM_CMN24_B8_R3	0x2C40C
#define DWPHY_MEM_CMN24_B8_R3_VAL	(0x15)

#define PCIE_DWPHY_MEM_CMN24_B8_R4	0x2C410
#define DWPHY_MEM_CMN24_B8_R4_VAL	(0xA8C2)

#define PCIE_DWPHY_MEM_CMN24_B8_R5	0x2C414
#define DWPHY_MEM_CMN24_B8_R5_VAL	(0x16)

#define PCIE_DWPHY_MEM_CMN24_B8_R6	0x2C418
#define DWPHY_MEM_CMN24_B8_R6_VAL	(0xBC9D)

#define PCIE_DWPHY_MEM_CMN24_B8_R7	0x2C41C
#define DWPHY_MEM_CMN24_B8_R7_VAL	(0x9016)

/* PCIe PMU registers */
#define PCIE_PHY_CONTROL_MASK		0x1
#define PCIE_GPIO_RETENTION		(0x1 << 17)

/* PCIe DBI registers */
#define PORT_MLTI_DIR_LANE_CHANGE	BIT(6)

/* For Type1 Configuration */
#define PCI_CLASS_MASK			GENMASK(31, 8)
#define PCI_CLASS_SHIFT			(8)

/* For PCIe CAP */
#define PCI_EXP_LNKCAP_L1EL_64USEC	(0x7 << 15)
#define PCI_EXP_DEVCTL2_COMP_TOUT_6_2MS	0x2
#define PCI_EXP_LNKSTA_NEGO_LANE	(0x3F)

/* For L1SS CAP */
#define PCI_L1SS_TCOMMON_32US		(0x20 << 8)
#define PCI_L1SS_TCOMMON_70US		(0x46 << 8)
#define PCI_L1SS_CTL1_ALL_PM_EN		(0xf << 0)
#define PCI_L1SS_CTL1_LTR_THRE_VAL	(0x40a0 << 16)
#define PCI_L1SS_CTL1_LTR_THRE_VAL_0US	(0x4000 << 16)
#define PCI_L1SS_CTL2_TPOWERON_90US	(0x49 << 0)
#define PCI_L1SS_CTL2_TPOWERON_130US	(0x69 << 0)
#define PCI_L1SS_CTL2_TPOWERON_170US	(0x89 << 0)
#define PCI_L1SS_CTL2_TPOWERON_180US	(0x91 << 0)
#define PCI_L1SS_CTL2_TPOWERON_200US	(0xA1 << 0)
#define PCI_L1SS_CTL2_TPOWERON_3100US	(0xfa << 0)

/* For Port Logic Registers */
#define PORT_AFR_L1_ENTRANCE_LAT_8us 	(0x3 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_16us	(0x4 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_32us	(0x5 << 27)
#define PORT_AFR_L1_ENTRANCE_LAT_64us	(0x7 << 27)

#define PCIE_PORT_FORCE_OFF		0x708
#define PORT_FORCE_PART_LANE_RXEI	BIT(22)

#define PCIE_PORT_MSI_CTRL_INT1_MASK	0x838
#define PCIE_PORT_COHERENCY_CTR_3	0x8E8

#define PCIE_PORT_AUX_CLK_FREQ		0xB40
#define PORT_AUX_CLK_FREQ_MASK		GENMASK(9, 0)
#define PORT_AUX_CLK_FREQ_76MHZ		0x4C

#define PCIE_PORT_L1_SUBSTATES		0xB44
#define PORT_L1_SUBSTATES_T_L12_MASK	GENMASK(5, 2)
#define PORT_L1_SUBSTATES_T_L12_VAL	(0xa << 2)

/* For ATU CAP */
#define PCIE_ATU_CR1_OUTBOUND0		0x300000
#define PCIE_ATU_CR2_OUTBOUND0		0x300004
#define PCIE_ATU_LOWER_BASE_OUTBOUND0	0x300008
#define PCIE_ATU_UPPER_BASE_OUTBOUND0	0x30000C
#define PCIE_ATU_LIMIT_OUTBOUND0	0x300010
#define PCIE_ATU_LOWER_TARGET_OUTBOUND0	0x300014
#define PCIE_ATU_UPPER_TARGET_OUTBOUND0	0x300018

#define PCIE_ATU_CR1_OUTBOUND1		0x300200
#define PCIE_ATU_CR2_OUTBOUND1		0x300204
#define PCIE_ATU_LOWER_BASE_OUTBOUND1	0x300208
#define PCIE_ATU_UPPER_BASE_OUTBOUND1	0x30020C
#define PCIE_ATU_LIMIT_OUTBOUND1	0x300210
#define PCIE_ATU_LOWER_TARGET_OUTBOUND1	0x300214
#define PCIE_ATU_UPPER_TARGET_OUTBOUND1	0x300218

#define PCIE_ATU_CR1_OUTBOUND2		0x300400
#define PCIE_ATU_CR2_OUTBOUND2		0x300404
#define PCIE_ATU_LOWER_BASE_OUTBOUND2	0x300408
#define PCIE_ATU_UPPER_BASE_OUTBOUND2	0x30040C
#define PCIE_ATU_LIMIT_OUTBOUND2	0x300410
#define PCIE_ATU_LOWER_TARGET_OUTBOUND2	0x300414
#define PCIE_ATU_UPPER_TARGET_OUTBOUND2	0x300418

/* PCIe EOM related definitions */
#define PCIE_EOM_PH_SEL_MAX		72
#define PCIE_EOM_DEF_VREF_MAX		256

#define PCIE_EOM_RX_EFOM_DONE			0x140C
#define PCIE_EOM_RX_EFOM_EOM_PH_SEL		0x1558
#define PCIE_EOM_RX_EFOM_MODE			0x1548
#define PCIE_EOM_RX_SSLMS_ADAP_HOLD_PMAD	0x11A0
#define PCIE_EOM_RX_EFOM_NUM_OF_SAMPLE_13_8	0x154C
#define PCIE_EOM_RX_EFOM_NUM_OF_SAMPLE_7_0	0x154C
#define PCIE_EOM_MON_RX_EFOM_ERR_CNT_13_8	0x1564
#define PCIE_EOM_MON_RX_EFOM_ERR_CNT_7_0	0x1560
#define PCIE_EOM_RX_EFOM_DFE_VREF_CTRL		0x1550
#define PCIE_EOM_RX_EFOM_START			0x1540

/* PCIe SYSREG registers */
#define PCIE_SYSREG_HSI1_SHARABLE_MASK		0x3
#define PCIE_SYSREG_HSI1_SHARABLE_ENABLE	0x3
#define PCIE_SYSREG_HSI1_SHARABLE_DISABLE	0x0

/* ETC definitions */
#define PCI_DEVICE_ID_EXYNOS		(0xECEC)

#define CAP_NEXT_OFFSET_MASK 		(0xFF << 8)
#define CAP_ID_MASK			(0xFF)
#define EXYNOS_PCIE_AP_MASK		(0xFFFF00)
#define EXYNOS_PCIE_REV_MASK		(0xFF)

#define MAX_RC_NUM		2
#define PCIE_IS_IDLE		1
#define PCIE_IS_ACTIVE		0

#define MAX_PCIE_PIN_STATE	2
#define PCIE_PIN_ACTIVE		0
#define PCIE_PIN_IDLE		1

#define PCI_FIND_CAP_TTL	(48)

#define MAX_LINK_UP_TIMEOUT	12000
#define MAX_TIMEOUT_GEN1_GEN2	5000
#define MAX_L2_TIMEOUT		2000
#define MAX_L1_EXIT_TIMEOUT	300
#define ID_MASK			0xffff

#define DWPHY_SRAM_OFFSET	(0x20000)

/* PCIe driver macro definitions */
#define pcie_info(fmt, args...) \
		dev_info(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_err(fmt, args...) \
		dev_err(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_warn(fmt, args...) \
		dev_warn(exynos_pcie->pci->dev, fmt, ##args)
#define pcie_dbg(fmt, args...) \
		dev_dbg(exynos_pcie->pci->dev, fmt, ##args)

#define to_exynos_pcie(x)	dev_get_drvdata((x)->dev)
#define to_pci_dev_from_dev(dev) container_of((dev), struct pci_dev, dev)

#define PCIE_BUS_PRIV_DATA(pdev) \
	((struct dw_pcie_rp *)pdev->bus->sysdata)

#define CAP_ID_NAME(id)	\
	(id == PCI_CAP_ID_PM)	?	"Power Management" :	\
	(id == PCI_CAP_ID_MSI)	?	"Message Signalled Interrupts" :	\
	(id == PCI_CAP_ID_EXP)	?	"PCI Express" :	\
	(id == PCI_CAP_ID_MSIX)	?	"MSI-X" :	\
	" Unknown id..!!"

#define PCI_EXT_CAP_ID_LM	(0x27)

#define EXT_CAP_ID_NAME(id)	\
	(id == PCI_EXT_CAP_ID_ERR)	?	"Advanced Error Reporting" :	\
	(id == PCI_EXT_CAP_ID_VC)	?	"Virtual Channel Capability" :	\
	(id == PCI_EXT_CAP_ID_DSN)	?	"Device Serial Number" :	\
	(id == PCI_EXT_CAP_ID_PWR)	?	"Power Budgeting" :	\
	(id == PCI_EXT_CAP_ID_RCLD)	?	"RC Link Declaration" :	\
	(id == PCI_EXT_CAP_ID_SECPCI)	?	"Secondary PCIe Capability" :	\
	(id == PCI_EXT_CAP_ID_L1SS)	?	"L1 PM Substates" :	\
	(id == PCI_EXT_CAP_ID_DLF)	?	"Data Link Feature" :	\
	(id == PCI_EXT_CAP_ID_PL_16GT)	?	"Physical Layer 16GT/s"	:	\
	(id == PCI_EXT_CAP_ID_VNDR)	?	"Vendor-Specific"	:	\
	(id == PCI_EXT_CAP_ID_LTR)	?	"Latency Tolerance Reporting"	:	\
	(id == PCI_EXT_CAP_ID_LM)	?	"Lane Margining at the Receiver" :	\
	" Unknown id ..!!"

enum exynos_pcie_opr_state {
	OPR_STATE_PROBE = 0,
	OPR_STATE_POWER_ON,
	OPR_STATE_POWER_OFF,
	OPR_STATE_SETUP_RC,
	OPR_STATE_ROM_CHANGE,
	OPR_STATE_ADDITIONAL_PHY_SET,
};

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
	STATE_PHY_OPT_OFF,
};

enum __ltssm_states {
	S_DETECT_QUIET			= 0x00,
	S_DETECT_ACT			= 0x01,
	S_POLL_ACTIVE			= 0x02,
	S_POLL_COMPLIANCE		= 0x03,
	S_POLL_CONFIG			= 0x04,
	S_PRE_DETECT_QUIET		= 0x05,
	S_DETECT_WAIT			= 0x06,
	S_CFG_LINKWD_START		= 0x07,
	S_CFG_LINKWD_ACEPT		= 0x08,
	S_CFG_LANENUM_WAIT		= 0x09,
	S_CFG_LANENUM_ACEPT		= 0x0A,
	S_CFG_COMPLETE			= 0x0B,
	S_CFG_IDLE			= 0x0C,
	S_RCVRY_LOCK			= 0x0D,
	S_RCVRY_SPEED			= 0x0E,
	S_RCVRY_RCVRCFG			= 0x0F,
	S_RCVRY_IDLE			= 0x10,
	S_L0				= 0x11,
	S_L0S				= 0x12,
	S_L123_SEND_EIDLE		= 0x13,
	S_L1_IDLE			= 0x14,
	S_L2_IDLE			= 0x15,
	S_L2_WAKE			= 0x16,
	S_DISABLED_ENTRY		= 0x17,
	S_DISABLED_IDLE			= 0x18,
	S_DISABLED			= 0x19,
	S_LPBK_ENTRY			= 0x1A,
	S_LPBK_ACTIVE			= 0x1B,
	S_LPBK_EXIT			= 0x1C,
	S_LPBK_EXIT_TIMEOUT		= 0x1D,
	S_HOT_RESET_ENTRY		= 0x1E,
	S_HOT_RESET			= 0x1F,
};

#define LINK_STATE_DISP(state)  \
	(state == S_DETECT_QUIET)       ? "DETECT QUIET" : \
        (state == S_DETECT_ACT)         ? "DETECT ACT" : \
        (state == S_POLL_ACTIVE)        ? "POLL ACTIVE" : \
        (state == S_POLL_COMPLIANCE)    ? "POLL COMPLIANCE" : \
        (state == S_POLL_CONFIG)        ? "POLL CONFIG" : \
        (state == S_PRE_DETECT_QUIET)   ? "PRE DETECT QUIET" : \
        (state == S_DETECT_WAIT)        ? "DETECT WAIT" : \
        (state == S_CFG_LINKWD_START)   ? "CFG LINKWD START" : \
        (state == S_CFG_LINKWD_ACEPT)   ? "CFG LINKWD ACEPT" : \
        (state == S_CFG_LANENUM_WAIT)   ? "CFG LANENUM WAIT" : \
        (state == S_CFG_LANENUM_ACEPT)  ? "CFG LANENUM ACEPT" : \
        (state == S_CFG_COMPLETE)       ? "CFG COMPLETE" : \
        (state == S_CFG_IDLE)           ? "CFG IDLE" : \
        (state == S_RCVRY_LOCK)         ? "RCVRY LOCK" : \
        (state == S_RCVRY_SPEED)        ? "RCVRY SPEED" : \
        (state == S_RCVRY_RCVRCFG)      ? "RCVRY RCVRCFG" : \
        (state == S_RCVRY_IDLE)         ? "RCVRY IDLE" : \
        (state == S_L0)                 ? "L0" : \
        (state == S_L0S)                ? "L0s" : \
        (state == S_L123_SEND_EIDLE)    ? "L123 SEND EIDLE" : \
        (state == S_L1_IDLE)            ? "L1 IDLE " : \
        (state == S_L2_IDLE)            ? "L2 IDLE"  : \
        (state == S_L2_WAKE)            ? "L2 _WAKE" : \
        (state == S_DISABLED_ENTRY)     ? "DISABLED ENTRY" : \
        (state == S_DISABLED_IDLE)      ? "DISABLED IDLE" : \
        (state == S_DISABLED)           ? "DISABLED" : \
        (state == S_LPBK_ENTRY)         ? "LPBK ENTRY " : \
        (state == S_LPBK_ACTIVE)        ? "LPBK ACTIVE" : \
        (state == S_LPBK_EXIT)          ? "LPBK EXIT" : \
        (state == S_LPBK_EXIT_TIMEOUT)  ? "LPBK EXIT TIMEOUT" : \
        (state == S_HOT_RESET_ENTRY)    ? "HOT RESET ENTRY" : \
        (state == S_HOT_RESET)          ? "HOT RESET" : \
        " Unknown state..!! "

#define EXUNOS_PCIE_STATE_NAME(state)	\
	(state == STATE_LINK_DOWN)	?	"LINK_DOWN" :		\
	(state == STATE_LINK_UP_TRY)	?	"LINK_UP_TRY" :		\
	(state == STATE_LINK_DOWN_TRY)	?	"LINK_DOWN_TRY" :	\
	(state == STATE_LINK_UP)	?	"LINK_UP"	:	\
	" Unknown state ...!!"

/* PCIe driver structure definitions */
struct regmap;
struct exynos_pcie;

struct pcie_eom_result {
	unsigned int phase;
	unsigned int vref;
	unsigned long int err_cnt;
};

struct exynos_pcie_ep_cfg {
	/* ex.*/
	int (*msi_init)(struct exynos_pcie *exynos_pcie);
	int (*l1ss_ep_specific_cfg)(struct exynos_pcie *exynos_pcie);
	void (*set_dma_ops)(struct device *dev);
	void (*set_ia_code)(struct exynos_pcie *exynos_pcie);
	void (*modify_read_val)(struct exynos_pcie *exynos_pcie, int where, u32 *val);
	void (*history_dbg_ctrl)(struct exynos_pcie *exynos_pcie);
	unsigned long udelay_before_perst;
	unsigned long udelay_after_perst;
	unsigned long mdelay_before_perst_low;
	unsigned long mdelay_after_perst_low;
	int linkdn_callback_direct; /* Call callback function in ISR */
	int no_speed_check;
	int enable_eq; /* Enable EQ */
	int linkup_max_count;
	u32 tpoweron_time;
	u32 common_restore_time;
	u32 ltr_threshold;
};

struct exynos_pcie {
	struct dw_pcie		*pci;
	struct pci_bus		*ep_pci_bus;
	struct pci_dev		*ep_pci_dev;
	void __iomem		*phy_base;
	void __iomem		*sysreg_iocc_base;
	void __iomem		*sysreg_base;
	void __iomem		*rc_dbi_base;
	void __iomem		*ia_base;
	void __iomem		*ssc_base; /* for Sub-System Custom */
	void __iomem		*ctrl_base; /* for SOC Control */
	unsigned int		pci_cap[PCI_FIND_CAP_TTL];
	unsigned int		pci_ext_cap[PCI_FIND_CAP_TTL];
	struct regmap		*sysreg;
	int			perst_gpio;
	int			ch_num;
	enum exynos_pcie_state	state;
	int			is_ep_scaned;
	int			probe_done;
	int			linkdown_cnt;
	int			idle_ip_index;
	int			pcie_is_linkup;
	bool			use_pcieon_sleep;
	bool			use_sysmmu;
	bool			use_ia;
	bool			cpl_timeout_recovery;
	bool			sudden_linkdown;
	spinlock_t		conf_lock;
	spinlock_t              reg_lock;
	spinlock_t              pcie_l1_exit_lock;
	struct workqueue_struct	*pcie_wq;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct delayed_work	dislink_work;
	struct delayed_work	cpl_timeout_work;
	struct exynos_pcie_register_event *rc_event_reg[2];
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	unsigned int            int_min_lock;
#endif
	u32			ip_ver;
	struct exynos_pcie_phy	*phy;
	int			l1ss_ctrl_id_state;
	u32			ep_device_type;
	int			max_link_speed;
	int			target_speed;
	int			debug_irq;

	struct pinctrl		*pcie_pinctrl;
	struct pinctrl_state	*pin_state[MAX_PCIE_PIN_STATE];
	struct pcie_eom_result **eom_result;
	struct notifier_block	itmon_nb;

	const struct exynos_pcie_ep_cfg *ep_cfg;

	int ep_power_gpio;
	u32 pmu_phy_isolation;
	u32 pmu_gpio_retention;
	u32 sysreg_sharability;

	u32 btl_target_addr;
	u32 btl_offset;
	u32 btl_size;

	u32 sysreg_ia0_sel;
	u32 sysreg_ia1_sel;
	u32 sysreg_ia2_sel;

	struct device dup_ep_dev;
	int copy_dup_ep;
	int force_recover_linkdn;
	u32 current_busdev;
	int linkup_fail;

	int separated_msi_start_num;
	int phy_rom_change;
	u32 *phy_rom;
	int phy_rom_size;
};

struct separated_msi_vector {
	int is_used;
	int irq;
	void *context;
	irq_handler_t msi_irq_handler;
	int flags;
};

#define PCIE_EXYNOS_OP_READ(base, type)					\
static inline type exynos_##base##_read					\
	(struct exynos_pcie *pcie, u32 reg)				\
{									\
		u32 data = 0;						\
		data = readl(pcie->base##_base + reg);			\
		return (type)data;					\
}

#define PCIE_EXYNOS_OP_WRITE(base, type)				\
static inline void exynos_##base##_write				\
	(struct exynos_pcie *pcie, type value, type reg)		\
{									\
		writel(value, pcie->base##_base + reg);			\
}

PCIE_EXYNOS_OP_READ(phy, u32);
PCIE_EXYNOS_OP_READ(ia, u32);
PCIE_EXYNOS_OP_READ(ssc, u32);
PCIE_EXYNOS_OP_READ(ctrl, u32);
PCIE_EXYNOS_OP_WRITE(phy, u32);
PCIE_EXYNOS_OP_WRITE(ia, u32);
PCIE_EXYNOS_OP_WRITE(ssc, u32);
PCIE_EXYNOS_OP_WRITE(ctrl, u32);

/* PCIe driver function prototypes */
int exynos_pcie_rc_poweron(int ch_num);
void exynos_pcie_rc_poweroff(int ch_num);
int exynos_pcie_rc_l1ss_ctrl(int enable, int id);
void exynos_pcie_rc_dump_link_down_status(int ch_num);
int exynos_pcie_rc_rd_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 *val);
int exynos_pcie_rc_wr_own_conf(struct dw_pcie_rp *pp, int where, int size, u32 val);
int exynos_pcie_rc_rd_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 *val);
int exynos_pcie_rc_wr_other_conf(struct dw_pcie_rp *pp, struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 val);
/* PCIe debug related functions */
int create_pcie_eom_file(struct device *dev);
void remove_pcie_eom_file(struct device *dev);
void exynos_pcie_dbg_register_dump(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_link_history(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_dump_link_down_status(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_msi_register(struct exynos_pcie *exynos_pcie);
void exynos_pcie_dbg_print_oatu_register(struct exynos_pcie *exynos_pcie);
int create_pcie_sys_file(struct device *dev);
void remove_pcie_sys_file(struct device *dev);
void exynos_pcie_warm_reset(struct exynos_pcie *exynos_pcie);

/* For coverage check */
void exynos_pcie_rc_check_function_validity(struct exynos_pcie *exynos_pcie);

#endif
