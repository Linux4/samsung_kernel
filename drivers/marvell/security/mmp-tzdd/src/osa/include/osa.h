/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 * Filename     : osa.h
 * Author       : Dafu Lv
 * Date Created : 30/09/2009
 * Description  : osa header file
 *
 */

#ifndef _OSA_H_
#define _OSA_H_

typedef struct osa_list {
	struct osa_list *next;
	struct osa_list *prev;
} osa_list_t;

typedef struct _osa_time_t {
	unsigned int sec;
	unsigned int msec;
} osa_time_t;

typedef void *osa_drv_t;
typedef int osa_err_t;
typedef void *osa_irq_t;
typedef void *osa_mpool_t;
typedef void *osa_mutex_t;
typedef void *osa_event_t;
typedef void *osa_rw_sem_t;
typedef void *osa_sem_t;
typedef void *osa_timer_t;
typedef void *osa_thread_id_t;
typedef void *osa_thread_t;

#define LINUX

#ifdef LINUX

#include "linux/osa_os_inc.h"

#elif defined(WINCE)

#include "wince/osa_os_inc.h"

#elif defined(ECOS)

#include "ecos/osa_os_inc.h"

#elif defined(MINOS)

#include "minos/osa_os_inc.h"

#else

#error "invalid OS or no OS defined."

#endif

typedef atomic_t osa_atomic_t;

#include "osa_err.h"

#include "osa_atomic.h"
#include "osa_dbg.h"
#include "osa_delay.h"
#include "osa_drv.h"
#include "osa_irq.h"
#include "osa_list.h"
#include "osa_mem.h"
#include "osa_misc.h"
#include "osa_sched.h"
#include "osa_sync.h"
#include "osa_time.h"
#include "osa_timer.h"
#include "osa_thread.h"

extern osa_err_t osa_init_module(void);
extern void osa_cleanup_module(void);

#endif /* _OSA_H_ */
