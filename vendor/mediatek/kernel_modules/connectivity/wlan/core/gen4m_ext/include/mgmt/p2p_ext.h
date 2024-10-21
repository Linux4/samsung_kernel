/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /include/p2p_ext.h
 */

/*! \file   "p2p_ext.h"
 *  \brief This file includes p2p ext support.
 *    Detail description.
 */


#ifndef _P2P_EXT_H
#define _P2P_EXT_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */
#if CFG_SAP_RPS_SUPPORT
u_int8_t p2pFuncIsRpsEnable(struct ADAPTER *prAdapter,
		u_int32_t u32Period,
		signed long * rxPacketDiff,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo);

void p2pFuncRpsAisCheck(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo,
		uint8_t ucIsEnable);

void p2pFuncRpsKalCheck(struct ADAPTER *prAdapter,
		u_int32_t u32Period,
		signed long * rxPacketDiff);

uint32_t p2pSetSapRps(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnSapRps,
	uint8_t ucSetRpsPhase,
	uint8_t ucBssIdx);

uint32_t p2pSetSapSus(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, u_int8_t fgEnSapSus);

#endif

#if CFG_EXT_FEATURE
void p2pExtStopAp(
	struct GLUE_INFO *prGlueInfo);
#endif

#endif /* _P2P_EXT_H */
