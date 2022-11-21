/*
 * pap.h -- pap header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _PAP_H_
#define _PAP_H_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_types.h"

void PapHandleIncomingFrame(PppControlS * pppControl);

#if defined __cplusplus
}
#endif				//__cplusplus
#endif				//_PAP_H_
