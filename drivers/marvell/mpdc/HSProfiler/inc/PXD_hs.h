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

/* defines the constants and structures for hotspot profiler */
#ifndef _HS_PROFILER_STRUCT_H
#define _HS_PROFILER_STRUCT_H

#include "pxdTypes.h"
#include "HSProfilerDef.h"

#ifdef _cplusplus
extern "C" {
#endif

#define HS_MODULES          1
#define HS_SAMPLES          2
#define HS_SUMMARY          3         // obselete, but don't remove this line
#define HS_CONTROL          4         // obselete, but don't remove this line
#define HS_SAMPLES_V2       5
#define HS_OPROFILE         9

#define PX_MAX_PATH    512

typedef PXD32_Byte HS_FULLPATH[PX_MAX_PATH];

#define HS_CONTROL_START       0
#define HS_CONTROL_PAUSE       1
#define HS_CONTROL_RESUME      2
#define HS_CONTROL_STOP        3

typedef struct PXD32_HSControl_s
{
	PXD32_Word   operation;         /* Hotspot control operation, HS_CONTROL_* macros */
	PXD32_Word   sampleCount;       /* sampleCount */
	PXD32_DWord  time;              /* time, in micro-seconds */
}PXD32_HSControl;

typedef struct PXD32_HSEvent_s{
	PXD32_Word registerId;        /* register id */
	PXD32_Word eventId;           /* event id */
	PXD32_Word eventsPerSample;   /* after how many events' occurrence, one sample will be recorded */
	PXD32_Word samplesPerSecond;  /* in calibration mode, sample count per second */
} PXD32_HSEvent;

typedef struct PXD32_HSSettings_s
{
	PXD32_Word duration;          /* duration in seconds */
	PXD32_Word bufferSize;        /* buffer size in KB */
	PXD32_Word options;
	PXD32_Word appEnabled;        /* none 0 means the application settings enabled */
								  /* 0 means application settings disabled */
	PXD32_Word imageEnabled;      /* same as appEnabled but for image settings */
	PXD32_Word timerEnabled;      /* same as appEnabled but for timer settings */
	PXD32_Word eventEnabled;      /* same as appEnabled but for event settings */
	
	struct Application_s 
	{
		HS_FULLPATH path;
		HS_FULLPATH workingDir;
		PXD32_Byte  parameter[512];
		PXD32_Word  options;
	} appLaunched;

	struct Image_s 
	{
    	HS_FULLPATH path;
	} imageWaited;

	struct Timer_s
	{
        PXD32_Word interval;
	} timer;

	struct Events_s 
	{
        PXD32_Word number;          /* the number of appended PXD_HSEvent struct */
        PXD32_HSEvent events[MAX_HS_COUNTER_NUM];
	} events;
} PXD32_HSSettings;

/*
 * Following macros define the flag of the module
 */

/* This module is a exe */
#define HS_MODULE_RECORD_FLAG_EXE                      0x00000001

/* This module is a global module. Global module is a module mapped to the same virtual address in all processes */
#define HS_MODULE_RECORD_FLAG_GLOBAL_MODULE            0x00000002

/* This module is the first module of the process */
#define HS_MODULE_RECORD_FLAG_FIRST_MODULE_IN_PROCESS  0x00000004

/* This process has the same pid as the auto launched application */
#define HS_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID          0x00000008

typedef struct PXD32_Hotspot_Module_s
{
	PXD32_Word length;              /* total record length */
	PXD32_Word pid;                 /* pid of the process which loads the module */
	PXD32_Word lscount;             /* load sample count */
	PXD32_Word flag;                /* process attribute flag */
	PXD32_Word loadAddr;            /* load address */
	PXD32_Word codeSize;            /* code size in virtual memory */
	PXD32_Word reserved;            /* reserved for future */
	PXD32_Word nameOffset;          /* file name offset in the file path string */
	PXD32_Byte pathName[1];         /* file path and name string */
} PXD32_Hotspot_Module;

/* obselete */
typedef struct PXD32_Hotspot_Sample_s 
{
	PXD32_Word pc;	                /* pc */
	PXD32_Word pid;                 /* process id */
	PXD32_Word tid;                 /* thread id */
	PXD32_Byte registerId;          /* register id */
	PXD32_Byte timestamp[3];        /* timestamp */
} PXD32_Hotspot_Sample;

typedef struct PXD32_Hotspot_Sample_V2_s 
{
	PXD32_Word pc;	                /* pc */
	PXD32_Word pid;                 /* process id */
	PXD32_Word tid;                 /* thread id */
	PXD32_Byte registerId;          /* register id */
	PXD32_Byte timestamp[7];        /* timestamp */
} PXD32_Hotspot_Sample_V2;

typedef struct PXD32_Hotspot_Summary_s
{
	PXD32_Time startTime;           /* start time */
	PXD32_Time stopTime;            /* stop time */
	PXD32_Word timestampFreq;       /* timestamp frequency for SOT */
}PXD32_Hotspot_Summary;

typedef struct PXD32_Hotspot_Control_Set_s
{
	PXD32_Word        number;
	PXD32_HSControl   controlInfo[1];
}PXD32_Hotspot_Control_Set;

typedef struct PXD32_Hotspot_OProfile_s
{
	PXD32_Word  isAllTid;
    PXD32_Word  isAllPid;
}PXD32_Hotspot_OProfile;

#ifdef _cplusplus
}
#endif

#endif /* _HS_PROFILER_STRUCT_H */
