/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd. 
** under the terms of the GNU General Public License Version 2, June 1991 (the "License"). 
** You may use, redistribute and/or modify this File in accordance with the terms and 
** conditions of the License, a copy of which is available along with the File in the 
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place, 
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES 
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.  
** The License provides additional details about this warranty disclaimer.
*/

/* (C) Copyright 2009 Marvell International Ltd. All Rights Reserved */

/* This header defines constants and macros for target board */
#ifndef _TARGET_DEFINE_H_
#define _TARGET_DEFINE_H_

#include "constants.h"

#ifdef __cplusplus
extern "C" {
#endif 

enum ProfilingState {
	PS_READY       = 1,
	PS_STARTING    = 2,
	PS_WAITIMG     = 3,
	PS_DELAY       = 4,
	PS_RUNNING     = 5,
	PS_CALIBRATING = 6,
	PS_PAUSING     = 7,
	PS_PAUSED      = 8,
	PS_RESUMING    = 9,
	PS_STOPPING    = 10,
	PS_DUMMY = 0xffffffff       /* this dummy state is to make sure that sizeof(enum ProfilingState) is 4 */
};

/* Profiler description */
struct ProfilerDesc {
	unsigned long id;
	unsigned long type;
	unsigned long version;
};

/* target information */
struct TargetInfo {
	unsigned char endian;
	unsigned char encoding;
	unsigned char coreNum;		/* number of total cores */
	unsigned char coreNumOnline;/* number of cores online */
	unsigned long memSize;		/* memory size, in KBytes */
	// unsigned long cpuId;
	char targetName[TARGET_NAME_MAXLEN];
	char osVerStr[OSVER_MAXLEN];
	unsigned long rawDataLength;
	unsigned long *rawData; 	/* raw data */
};

/* activity setting */
typedef struct ActivitySetting_s{
	char	name[NAME_LEN_MAX];                  /* activity name */
	unsigned long	profType;                    /* profiler type */
	char	resultPath[PATH_LEN_MAX];            /* where the result file is saved */
	int     seqNum;                              /* result index */
	char    comments[COMMENTS_LEN_MAX];          /* additional notes */
	void	*profSetting;                        /* pointer to the profiler settings */
} PXActivitySetting;

// indicators
typedef struct IndicatorSetting_s{
	int num;
	char **indicators;
} PXIndicatorSetting;

struct ProfilerStatusChange
{
	enum ProfilingState state;                   /* current profiling state */ 
	unsigned long long  timestamp;               /* the corresponding timestamp of the current profiling state */
};

/* profiler summary information */
struct ProfilerSummary {
	unsigned long                 timestampFreq;      /* timestamp frequency used in profiler */
	unsigned int                  pscNum;             /* number of the profiler status change array */
	struct ProfilerStatusChange * psc;                /* the profiler status change array */
};

#ifdef  __cplusplus
}
#endif 

#endif  // _TARGET_DEFINE_H_
