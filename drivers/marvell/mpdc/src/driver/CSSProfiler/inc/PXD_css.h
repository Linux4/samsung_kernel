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

/* defines the constants and structures for CSS profiler */
#ifndef _CSS_PROFILER_STRUCT_H
#define _CSS_PROFILER_STRUCT_H

#include "pxdTypes.h"
#include "CSSProfilerDef.h"

#ifdef _cplusplus
extern "C" {
#endif

#define CSS_MODULES          1
#define CSS_SAMPLES          2
#define CSS_SUMMARY          3           // obselete, but don't remove this line
#define CSS_SAMPLES_V2       4

#ifndef PX_MAX_PATH
#define PX_MAX_PATH    512
#endif

typedef PXD32_Byte CSS_FULLPATH[PX_MAX_PATH];

typedef struct PXD32_CSSEvent_s
{
	PXD32_Word registerId;         /* register id */
	PXD32_Word eventId;            /* event id */
	PXD32_Word eventsPerSample;	       /* sample after value */
	PXD32_Word samplesPerSecond;  /* calibrated sample count per second */
} PXD32_CSSEvent;

typedef struct PXD32_CSSSettings_s
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
		CSS_FULLPATH path;
		CSS_FULLPATH workingDir;
		PXD32_Byte   parameter[512];
		PXD32_Word   options;
	} appLaunched;

	struct Image_s 
	{
    	CSS_FULLPATH path;
	} imageWaited;

	struct Timer_s
	{
        PXD32_Word interval;
	} timer;

	struct Events_s 
	{
        PXD32_Word number;          /* the number of appended PXD_CSSEvent struct */
        PXD32_CSSEvent events[MAX_CSS_COUNTER_NUM];
	} events;
} PXD32_CSSSettings;

/*
 * Following macros define the flag of the module
 */

/* This module is a exe */
#define CSS_MODULE_RECORD_FLAG_EXE                      0x00000001

/* This module is a global module. Global module is a module mapped to the same virtual address in all processes */
#define CSS_MODULE_RECORD_FLAG_GLOBAL_MODULE            0x00000002

/* This module is the first module of the process */
#define CSS_MODULE_RECORD_FLAG_FIRST_MODULE_IN_PROCESS  0x00000004

/* This process has the same pid as the auto launched application */
#define CSS_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID          0x00000008

typedef struct PXD32_CSS_Module_s
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
} PXD32_CSS_Module;

typedef struct PXD32_CSS_Call_s
{
	PXD32_Word	address;                        /* the return address */
	PXD32_Word	pid;                            /* the pid of the current process */
}PXD32_CSS_Call;

typedef struct PXD32_CSS_Call_Stack_s 
{
	PXD32_Word          registerId;          /* register id */
	PXD32_Word          pid;                 /* process id */
	PXD32_Word          tid;                 /* thread id */
	PXD32_Word          depth;               /* call stack depth */
	PXD32_CSS_Call      cs[1];               /* the array of CSS call */
} PXD32_CSS_Call_Stack;

typedef struct PXD32_CSS_Call_Stack_V2_s 
{
	PXD32_DWord         timestamp;           /* timestamp */
	PXD32_Word          registerId;          /* register id */
	PXD32_Word          pid;                 /* process id */
	PXD32_Word          tid;                 /* thread id */
	PXD32_Word          depth;               /* call stack depth */
	PXD32_CSS_Call      cs[1];               /* the array of CSS call */
} PXD32_CSS_Call_Stack_V2;

typedef struct PXD32_CSS_Summary_s
{
	PXD32_Time startTime;           /* start time */
	PXD32_Time stopTime;            /* stop time */
}PXD32_CSS_Summary;


#ifdef _cplusplus
}
#endif

#endif /* _CSS_PROFILER_STRUCT_H */
