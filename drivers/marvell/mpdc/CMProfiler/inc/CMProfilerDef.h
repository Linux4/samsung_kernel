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

#ifndef __CM_PROFILER_DEF_H__
#define __CM_PROFILER_DEF_H__

#include "RegisterId.h"
#define PROFILERTYPE_CM		3
#define MAX_CM_COUNTER_NUM	20


//#define CM_BUFFER_ID_PROCESS_CREATE   0
//#define CM_BUFFER_ID_THREAD_CREATE    1
//#define CM_BUFFER_ID_THREAD_SWITCH    2
//#define CM_BUFFER_NUMBER              3

#define IDLE_PROCESS_NAME       _T("PXIdle")
#define CM_DSA_NAME             _T("PX.CM.DSA")

#include "constants.h"
#endif /* __CM_PROFILER_DEF_H__ */
