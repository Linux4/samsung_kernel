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

#ifndef __PX_THREAD_PROF_SETTINGS_H__
#define __PX_THREAD_PROF_SETTINGS_H__

#include "TPProfilerDef.h"

#ifdef _cplusplus
extern "C"
{
#endif

#define TP_APP_OPTION_STOP_PROFILER_WHEN_APP_EXITS          0x00000001

struct TPLaunchAppSettings
{
	char *          appFullPath;                      /* full path of the application to be launched */
	char *          parameters;                       /* command line parameters of the application */
	char *          workingDir;                       /* working directory of the application */
	unsigned long   options;                          /* auto launch application options */
};

struct TPWaitImageSettings
{
	char *  imageFullPath;                            /* full path of the image to be waited */
};

/* infinite duration */
#define TP_INFINITE_DURATION    0

struct TPProfSettings
{
	unsigned long                duration;            /* duration in seconds, set to TP_INFINITE_DURATION for infinite duration */
	unsigned long                bufferSize;          /* buffer size in KB */
	unsigned long                options;             /* option of the thread profiler, check TP_OPTION_* macros */
	unsigned long                refreshInterval;     /* refresh interval in milliseconds */
	struct TPLaunchAppSettings * appSettings;         /* auto launch application settings, set to NULL to disable it */
	struct TPWaitImageSettings * imageSettings;       /* wait for image load settings, set to NULL to disable it */
};

#ifdef _cplusplus
}
#endif

#endif /* __PX_THREAD_PROF_SETTINGS_H__ */
