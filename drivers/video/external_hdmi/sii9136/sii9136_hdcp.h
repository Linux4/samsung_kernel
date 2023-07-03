
#ifndef __HDCP_H_
#define __HDCP_H_

#include "def.h"

#ifdef DEV_SUPPORT_HDCP

extern bool HDCP_TxSupports;			// True if the TX supports HDCP and has legal AKSVs... False otherwise.
extern bool HDCP_Started;				// True if the HDCP enable bit has been set... False otherwise.

extern byte HDCP_LinkProtectionLevel;	// The most recently set link protection level from 0x29[7:6]

void HDCP_Init (void);

void HDCP_CheckStatus (byte InterruptStatusImage);

bool IsHDCP_Supported(void);

void HDCP_On(void);
void HDCP_Off(void);

#else

#define HDCP_TxSupports FALSE

#endif

#endif
