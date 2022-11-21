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

#ifndef __PX_HOTSPOT_PROF_SETTINGS_H__
#define __PX_HOTSPOT_PROF_SETTINGS_H__

#include "HSEventType.h"
#include "HSProfilerDef.h"

#ifdef _cplusplus
extern "C"
{
#endif

#define HS_APP_OPTION_STOP_PROFILER_WHEN_APP_EXITS          0x00000001

struct HSLaunchAppSettings
{
	char *          appFullPath;            /* full path of the application to be launched */
	char *          parameters;             /* command line parameters of the application */
	char *          workingDir;             /* working directory of the application */
	unsigned long   options;                /* auto launch application options */
};

struct HSWaitImageSettings
{
	char *  imageFullPath;                  /* full path of the image to be waited */
};

struct HSTimerSettings
{
	unsigned long interval;                 /* interval of the timer, in microseconds */
};

struct HSEventConfig
{
	unsigned long registerId;               /* register id to set the event */
	unsigned long eventId;                  /* id of the event */
	unsigned long eventsPerSample;          /* after how many events' occurrence, one sample will be recorded */
	unsigned long samplesPerSecond;         /* in calibration mode, how many samples you want to get in one second */
};

struct HSEventSettings
{
	long           eventNumber;                        /* number of the event */
	struct HSEventConfig    event[MAX_HS_COUNTER_NUM];    /* configuration for each event */                 
};

/* infinite duration */
#define HOTSPOT_INFINITE_DURATION    0

/* set this option for calibration mode */
#define HOTSPOT_OPTION_CALIBRATION            0x00000001

/* set this option to allow CPU entering the idle mode */
#define HOTSPOT_OPTION_ALLOW_CPU_IDLE         0x00000002

struct HSProfSettings
{
	unsigned long                duration;            /* duration in seconds, set to HOTSPOT_INFINITE_DURATION for infinite duration */
	unsigned long                bufferSize;          /* buffer size in KB */
	unsigned long                options;             /* option of the hotspot profiler, check HOTSPOT_OPTION_* macros */
	struct HSLaunchAppSettings * appSettings;         /* auto launch application settings, set to NULL to disable it */
	struct HSWaitImageSettings * imageSettings;       /* wait for image load settings, set to NULL to disable it */
	struct HSTimerSettings     * timerSettings;       /* the timer settings, set to NULL to disable it */
	struct HSEventSettings     * eventSettings;       /* the event settings, set to NULL to disable it */
};

#ifdef _cplusplus
}
#endif

#endif /* __PX_HOTSPOT_PROF_SETTINGS_H__ */
