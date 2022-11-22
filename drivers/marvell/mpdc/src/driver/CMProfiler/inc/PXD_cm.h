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

/* defines the constants and structures for demo profiler */
#ifndef _CM_PROFILER_STRUCT_H
#define _CM_PROFILER_STRUCT_H

#include "pxdTypes.h"
#include "CMProfilerDef.h"

#ifdef _cplusplus
extern "C" {
#endif

/* counter monitoring profiler data types */
#define CM_DATA_PROCESS_CREATE           1
#define CM_DATA_THREAD_CREATE            2
#define CM_DATA_THREAD_SWITCH            3
#define CM_DATA_OVERALL_COUNT            4
#define CM_EXTRA_DATA                    5
#define CM_DATA_SUMMARY                  6             // obselete, but don't remove this line

#define PX_MAX_PATH    512

typedef PXD32_Byte CM_FULLPATH[PX_MAX_PATH];

typedef struct PXD32_CMCounterSetting_s {
	PXD32_Word cid;                /* counter id */
	PXD32_Word cevent;             /* event id */
} PXD32_CMCounterSetting;

typedef struct PXD32_CMSettings_s {
	PXD32_Word bufferSize;          /* buffer size in KB */
	PXD32_Word flag;                /* flag of the settings */
	PXD32_Word refreshInterval;     /* refresh interval in milliseconds, 0 means flush data only when buffer is full */
	PXD32_Word pid;                 /* pid of the process in which we are interested */
	PXD32_Word tid;                 /* tid of the thread in which we are interested */
	PXD32_Word duration;            /* duration in seconds, 0 means infinite */
	PXD32_Word appEnabled;          /* none 0 means the application settings enabled */
	                                /* 0 means application settings disabled */       
	struct Application_s 
	{
		CM_FULLPATH path;
		CM_FULLPATH workingDir;
		PXD32_Byte  parameter[512];
		PXD32_Word  options;
	} appLaunched;
	                              
	struct CounterConfigs_s 
	{
		PXD32_Word number;          /* the number of appended PXD32_CMCounterSetting struct */
		PXD32_CMCounterSetting settings[MAX_CM_COUNTER_NUM];
	} counterConfigs;

} PXD32_CMSetting;

#define CM_PROCESS_FLAG_IDLE_PROCESS   0x00000001

/* This process has the same pid as the auto launched application */
#define CM_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID          0x00000002


/* process creation data */
typedef struct PXD32_CMProcessCreate_s {
	PXD32_Word recLen;                  /* total record length */
	PXD32_Word pid;
	PXD32_DWord timestamp;
	PXD32_Word flag;                    /* process attribute flag */
	PXD32_Word nameOffset;              /* file name offset in the file path string */
	PXD32_Byte pathName[1];             /* file path string */
} PXD32_CMProcessCreate;

/* thread creation data */
typedef struct PXD32_CMThreadCreate_s {
	PXD32_DWord timestamp;
	PXD32_Word pid;
	PXD32_Word tid;          
} PXD32_CMThreadCreate;

/* thread switch counter value data */
typedef struct PXD32_CMThreadSwitch_s {
	PXD32_DWord timestamp;
	PXD32_Word tid;
	PXD32_DWord counterVal[1];
} PXD32_CMThreadSwitch;

typedef struct PXD32_CMAllEventCount_s {
	PXD32_DWord timestamp;
	PXD32_DWord counterVal[1];          /* counter values */
} PXD32_CMAllEventCount;

/* extra data for counter monitoring */
typedef struct PXD32_CMExtraData_s{
	PXD32_DWord osTimerFreq;            /* os timer frequency */
	PXD32_DWord timestampFreq;          /* timestamp frequency */
} PXD32_CMExtraData;

/* general data for the counter montioring */
typedef struct PXD32_CMSummary_s {
	PXD32_Time startTime;               /* start time */
	PXD32_Time stopTime;                /* stop time */
}PXD32_CMSummary;

#ifdef _cplusplus
}
#endif

#endif /* _CM_PROFILER_STRUCT_H */
