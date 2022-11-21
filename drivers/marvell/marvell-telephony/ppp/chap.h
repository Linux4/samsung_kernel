/*
 * chap.h -- chap header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _CHAP_H_
#define _CHAP_H_

#if defined __cplusplus
extern "C" {
#endif				//__cplusplus

#include "ppp_types.h"

//For now - use fixed CHAP challenge
#define PPP_USE_FIXED_CHAP_CHALLENGE

#define CHAP_MAX_CHALLENGE_TRIES 10

void ChapHandleIncomingFrame(PppControlS * pppControl);
void ChapSendAuthChallenge(PppControlS * pppControl);

#if defined __cplusplus
}
#endif				//__cplusplus
#endif
