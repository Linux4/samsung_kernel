/*
 * ppp_log.h -- ppp debug log header
 *
 * (C) Copyright [2006-2008] Marvell International Ltd.
 * All Rights Reserved
 *
 */

#ifndef _PPP_LOG_INC_
#define _PPP_LOG_INC_

#include "ppp_types.h"

#define PPP_FUNC_END		0xFFFFFFFF
#define PPP_FUNC_ENTRY		0

#define PPP_LOG_SIZE		0x400

#define PPP_LOG_VAR(x)			PppAddLogEntry(localLogType,(x),0)
#if defined (PPP_DEBUG_ALL)
#define PPP_LOG()			PppAddLogEntry(localLogType,__LINE__,0)
#define PPP_LOG_1(v1)			PppAddLogEntry(localLogType,__LINE__,1,(v1))
#define PPP_LOG_2(v1,v2)		PppAddLogEntry(localLogType,__LINE__,2,(v1),(v2))
#define PPP_LOG_3(v1,v2,v3)		PppAddLogEntry(localLogType,__LINE__,3,(v1),(v2),(v3))
#else
#define PPP_LOG()
#define PPP_LOG_1(v1)
#define PPP_LOG_2(v1,v2)
#define PPP_LOG_3(v1,v2,v3)
#endif

#define PPP_LOG_ENTRY(x)\
	U_CHAR	localLogType=(x);\
	PPP_LOG_VAR(PPP_FUNC_ENTRY);\

#define PPP_LOG_EXIT()		PPP_LOG_VAR(PPP_FUNC_END)

typedef struct _PppLogEntryS {
	U_CHAR Type;
	U_INT EntryNumber;
	U_CHAR NumVars;
	U_CHAR Dummy[2];	//align to 4 bytes
	U_INT Timestamp;
	U_INT Vars[4];
}PppLogEntryS;

typedef struct _PppLogDbS {
	U_INT Size;
	U_INT LastEntry;
	U_INT IsEnabled;

	PppLogEntryS *LogEntry;

}PppLogDbS;

/************************************************************************/
/* APIs                                                                 */
/************************************************************************/
void PppLogInit(void);
void PppAddLogEntry(U_CHAR type, U_INT entryNum, U_CHAR numVariables, ...);


#endif				//_PPP_LOG_INC
