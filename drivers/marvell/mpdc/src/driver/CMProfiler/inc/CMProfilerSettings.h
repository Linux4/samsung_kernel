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

#ifndef _CM_PROFILER_SETTINGS_H__
#define _CM_PROFILER_SETTINGS_H__

#include "CMEventType.h"
#include "CMProfilerDef.h"

#ifdef _cplusplus
extern "C" {
#endif

#define CM_APP_OPTION_STOP_PROFILER_WHEN_APP_EXITS          0x00000001

struct CMLaunchAppSettings
{
	char *          appFullPath;            /* full path of the application to be launched */
	char *          parameters;             /* command line parameters of the application */
	char *          workingDir;             /* working directory of the application */
	unsigned long   options;                /* auto launch application options */
};

struct CMCounterSettings
{
    int             cid;        /* counter id */
    unsigned int    cevent;     /* event id */
};

struct CMCounterConfigs
{
    int              number;
    struct CMCounterSettings  settings[MAX_CM_COUNTER_NUM];
};

/* counter monitor duration is infinite */
#define CM_INFINITE_DURATION                    0

/* don't need to display the data periodically */
#define CM_DISPLAY_DATA_INFINITE_INTERVAL       0

/*
 * Get the counter values for system wide,
 * the counter values are NOT associated with processes/threads
 */
#define CM_MODE_SYSTEM 0

/*
 * Get the counter values for system wide
 * the counter values are associated with each process/thread
 */
#define CM_MODE_PER_THREAD  1

/*
 * Get the counter values only on the specific process
 */
#define CM_MODE_SPECIFIC_PROCESS    2

/*
 * Get the counter values only on the specific thread
 */
#define CM_MODE_SPECIFIC_THREAD     3

/*
 * Internal Use:
 * If we need to display the counter monitor result in real-time
 * User don't set this flag
 */
#define CM_DISPLAY_MODE_REALTIME    0x80000000

/*
 * Internal Use:
 * set this flag in Counter Monitor Extension SDK
 * User don't set this flag
 */
#define CM_EXT_SDK_MODE             0x40000000


/* profiler setting */
struct CMProfSettings
{
    unsigned long   bufferSize;                 /* buffer size in KB */
    unsigned long   flag;                       /* see CM_MODE_* macros */
    unsigned long   refreshInterval;            /* display the data periodically, in milliseconds */
    unsigned long   pid;                        /* the pid of the specific process in CM_MODE_SPECIFIC_PROCESS or CM_MODE_SPECIFIC_THREAD mode */
    unsigned long   tid;                        /* the tid of the specific thread in CM_MODE_SPECIFIC_THREAD mode */
    unsigned long   duration;                   /* duration in seconds, CM_INFINITE_DURATION means infinitely */
    struct CMCounterConfigs counterConfigs;		/* Counter configuration */
    struct CMLaunchAppSettings * appSettings;	/* auto launch application settings, set to NULL to disable it */
};

#ifdef _cplusplus
}
#endif

#endif /* _CM_PROFILER_SETTINGS_H__ */
