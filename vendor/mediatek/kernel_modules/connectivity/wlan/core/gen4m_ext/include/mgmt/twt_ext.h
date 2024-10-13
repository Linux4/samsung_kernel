/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /include/twt_ext.h
 */

/*! \file   "twt_ext.h"
 *  \brief This file includes twt ext support.
 *    Detail description.
 */


#ifndef _TWT_EXT_H
#define _TWT_EXT_H

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

#ifdef CFG_SUPPORT_TWT_EXT
int twt_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

int twt_teardown_all(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter);

void twtPlannerCheckTeardownSuspend(
	struct ADAPTER *prAdapter,
	bool fgForceTeardown,
	bool fgTeardown);

void twtPlannerCheckResume(
	struct ADAPTER *prAdapter);

int scheduledpm_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

int delay_wakeup_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);
#endif

#endif /* _TWT_EXT_H */
