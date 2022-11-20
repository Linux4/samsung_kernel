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

#ifndef __PX_CSS_PROF_SETTINGS_H__
#define __PX_CSS_PROF_SETTINGS_H__

#include "CSSEventType.h"
#include "CSSProfilerDef.h"

#ifdef _cplusplus
extern "C"
{
#endif

#define CSS_APP_OPTION_STOP_PROFILER_WHEN_APP_EXITS          0x00000001

struct CSSLaunchAppSettings
{
	char *          appFullPath;            /* full path of the application to be launched */
	char *          parameters;             /* command line parameters of the application */
	char *          workingDir;             /* working directory of the application */
	unsigned long   options;                /* auto launch application options */
};

struct CSSWaitImageSettings
{
	char *  imageFullPath;                  /* full path of the image to be waited */
};

struct CSSTimerSettings
{
	unsigned long interval;                 /* interval of the timer, in microseconds */
};

struct CSSEventConfig
{
	unsigned long registerId;              /* register id to set the event */
	unsigned long eventId;                 /* id of the event */
	unsigned long eventsPerSample;         /* sample after value of the event */
	unsigned long samplesPerSecond;        /* the calibration target sample count per second */
};

struct CSSEventSettings
{
	long                     eventNumber;                   /* number of the event */
	struct CSSEventConfig    event[MAX_CSS_COUNTER_NUM];    /* configuration for each event */                 
};

/* infinite duration */
#define CSS_INFINITE_DURATION    0

/* set this option if sample after value of the event will be calibrated */
#define CSS_OPTION_CALIBRATION           0x00000001

/* set this option if idle process is not launched by profiler automatically 
 * by default, idle process is launched by CSS profiler
 */
#define CSS_OPTION_ALLOW_CPU_IDLE       0x00000002

struct CSSProfSettings
{
	unsigned long                 duration;            /* duration in seconds, set to CSS_INFINITE_DURATION for infinite duration */
	unsigned long                 bufferSize;          /* buffer size in KB */
	unsigned long                 options;             /* option of the CSS profiler, check CSS_OPTION_* macros */
	struct CSSLaunchAppSettings * appSettings;         /* auto launch application settings, set to NULL to disable it */
	struct CSSWaitImageSettings * imageSettings;       /* wait for image load settings, set to NULL to disable it */
	struct CSSTimerSettings     * timerSettings;       /* the timer settings, set to NULL to disable it */
	struct CSSEventSettings     * eventSettings;       /* the event settings, set to NULL to disable it */
};

#ifdef _cplusplus
}
#endif

#endif /* __PX_CSS_PROF_SETTINGS_H__ */
