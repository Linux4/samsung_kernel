/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file  mt6653.h
*    \brief This file contains the info of mt6653
*/

#ifdef MT6653

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define CONN_INFRA_CFG_BASE			0x830C0000
#define CONNAC3X_CONN_CFG_ON_BASE		0x7C060000
#define MCU_SW_CR_BASE				0x7C05B100
#define MT6653_TX_DESC_APPEND_LENGTH		32
#define MT6653_HIF_TX_DESC_APPEND_LENGTH	44
#define MT6653_RX_INIT_DESC_LENGTH		32
#define MT6653_RX_DESC_LENGTH			32
#define MT6653_CHIP_ID				0x6653
#define MT6653_CONNINFRA_VERSION_ID		0x03040000
#define MT6653_WF_VERSION_ID			0x03040000
#define USB_VND_PWR_ON_ADDR			(MCU_SW_CR_BASE + 0x20)
#define USB_VND_PWR_ON_ACK_BIT			BIT(0)
#define CONNAC3X_TOP_HCR			0x88000000
#define CONNAC3X_TOP_HVR			0x88000000
#define CONNAC3X_TOP_FVR			0x88000004
#define MT6653_TOP_CFG_BASE			NIC_CONNAC_CFG_BASE
#define MT6653_PATCH_START_ADDR			0x00900000
#define MT6653_ARB_AC_MODE_ADDR			(0x820E315C)
#define RX_DATA_RING_BASE_IDX			2
#define CONNAC3X_CONN_CFG_ON_CONN_ON_MISC_DRV_FM_STAT_SYNC_SHFT	0
#define CONNAC3X_CONN_CFG_ON_CONN_ON_MISC_ADDR \
	(CONNAC3X_CONN_CFG_ON_BASE + 0xF0)
#define CONNAC3X_CONN_CFG_ON_CONN_ON_EMI_ADDR \
	(CONNAC3X_CONN_CFG_ON_BASE + 0xD1C)
#define MT6653_EMI_SIZE_ADDR \
	(MCU_SW_CR_BASE + 0x01E0)
#define MT6653_MCIF_MD_STATE_WHEN_WIFI_ON_ADDR	(MCU_SW_CR_BASE + 0x01E8)

#define WF_PP_TOP_BASE             0x820CC000
#define WF_PP_TOP_DBG_CTRL_ADDR    (WF_PP_TOP_BASE + 0x00FC)
#define WF_PP_TOP_DBG_CS_0_ADDR    (WF_PP_TOP_BASE + 0x0104)
#define WF_PP_TOP_DBG_CS_1_ADDR    (WF_PP_TOP_BASE + 0x0108)
#define WF_PP_TOP_DBG_CS_2_ADDR    (WF_PP_TOP_BASE + 0x010C)

#define MT6653_PCIE2AP_REMAP_BASE_ADDR		0x60000
#define MT6653_REMAP_BASE_ADDR			0x7c500000

#define MT6653_ROM_VERSION			1

#define MTK_CUSTOM_OID_INTERFACE_VERSION     0x00000200	/* for WPDWifi DLL */
#define MTK_EM_INTERFACE_VERSION		0x0001

#ifdef CFG_WFDMA_AP_MSI_NUM
#define WFDMA_AP_MSI_NUM		(CFG_WFDMA_AP_MSI_NUM)
#else
#define WFDMA_AP_MSI_NUM		1
#endif
#define WFDMA_MD_MSI_NUM		8

extern struct PLE_TOP_CR rMt6653PleTopCr;
extern struct PSE_TOP_CR rMt6653PseTopCr;
extern struct PP_TOP_CR rMt6653PpTopCr;

#define CONN_AON_WF_NAPPING_ENABLE	0
#define CONN_AON_WF_NAPPING_DISABLE	1

/*------------------------------------------------------------------------------
 * MACRO for MT6653 RXVECTOR Parsing
 *------------------------------------------------------------------------------
 */
/* Group3[0] */
#define MT6653_RX_VT_RX_RATE_MASK         BITS(0, 6)
#define MT6653_RX_VT_RX_RATE_OFFSET       0
#define MT6653_RX_VT_NSTS_MASK            BITS(7, 10)
#define MT6653_RX_VT_NSTS_OFFSET          7

/* Group3[2] */
#define MT6653_RX_VT_FR_MODE_MASK         BITS(0, 2) /* Group3[2] */
#define MT6653_RX_VT_FR_MODE_OFFSET       0
#define MT6653_RX_VT_GI_MASK              BITS(3, 4)
#define MT6653_RX_VT_GI_OFFSET            3
#define MT6653_RX_VT_DCM_MASK             BIT(5)
#define MT6653_RX_VT_DCM_OFFSET           5
#define MT6653_RX_VT_STBC_MASK            BITS(9, 10)
#define MT6653_RX_VT_STBC_OFFSET          9
#define MT6653_RX_VT_TXMODE_MASK          BITS(11, 14)
#define MT6653_RX_VT_TXMODE_OFFSET        11

#define RXV_GET_RX_RATE(_prRxVector)				\
		(((_prRxVector) & MT6653_RX_VT_RX_RATE_MASK)	\
			 >> MT6653_RX_VT_RX_RATE_OFFSET)

#define RXV_GET_RX_NSTS(_prRxVector)				\
		(((_prRxVector) & MT6653_RX_VT_NSTS_MASK)	\
			 >> MT6653_RX_VT_NSTS_OFFSET)

#define RXV_GET_FR_MODE(_prRxVector)				\
		(((_prRxVector) & MT6653_RX_VT_FR_MODE_MASK)	\
			 >> MT6653_RX_VT_FR_MODE_OFFSET)

#define RXV_GET_GI(_prRxVector)					\
		(((_prRxVector) & MT6653_RX_VT_GI_MASK)		\
			 >> MT6653_RX_VT_GI_OFFSET)

#define RXV_GET_STBC(_prRxVector)				\
		(((_prRxVector) & MT6653_RX_VT_STBC_MASK)	\
			 >> MT6653_RX_VT_STBC_OFFSET)

#define RXV_GET_TXMODE(_prRxVector)				\
		(((_prRxVector) & MT6653_RX_VT_TXMODE_MASK)	\
			 >> MT6653_RX_VT_TXMODE_OFFSET)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void mt6653_show_wfdma_info(struct ADAPTER *prAdapter);
void mt6653_show_ple_info(struct ADAPTER *prAdapter, u_int8_t fgDumpTxd);
void mt6653_show_pse_info(struct ADAPTER *prAdapter);
void mt6653_show_wfdma_dbg_probe_info(struct ADAPTER *prAdapter,
	enum _ENUM_WFDMA_TYPE_T enum_wfdma_type);
void mt6653_show_wfdma_wrapper_info(struct ADAPTER *prAdapter,
	enum _ENUM_WFDMA_TYPE_T enum_wfdma_type);

void mt6653_icapRiseVcoreClockRate(void);
void mt6653_icapDownVcoreClockRate(void);

#if defined(_HIF_PCIE)
void mt6653_dumpWfsyscpupcr(struct ADAPTER *ad);
void mt6653_DumpBusHangCr(struct ADAPTER *ad);
void mt6653_dumpPcGprLog(struct ADAPTER *ad);
void mt6653_dumpN45CoreReg(struct ADAPTER *ad);
void mt6653_dumpWfTopReg(struct ADAPTER *ad);
void mt6653_dumpWfBusReg(struct ADAPTER *ad);
void mt6653_dumpCbtopReg(struct ADAPTER *ad);
u_int8_t mt6653_is_ap2conn_off_readable(struct ADAPTER *ad);
u_int8_t mt6653_is_conn2wf_readable(struct ADAPTER *ad);
#endif

#if CFG_SUPPORT_LINK_QUALITY_MONITOR
int mt6653_get_rx_rate_info(const uint32_t *prRxV,
		struct RxRateInfo *prRxRateInfo);
#endif

void mt6653_get_rx_link_stats(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb, uint32_t *pu4RxV);

#endif  /* mt6653 */
