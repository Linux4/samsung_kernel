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
 * Filename     : osa_thread.h
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the header file of thread-related functions in osa.
 *
 */

#ifndef _OSA_THREAD_H_
#define _OSA_THREAD_H_

#include "osa.h"

#define OSA_THRD_PRIO_TIME_CRITICAL (0)
#define OSA_THRD_PRIO_HIGHEST       (1)
#define OSA_THRD_PRIO_ABOVE_NORMAL  (2)
#define OSA_THRD_PRIO_NORMAL        (3)
#define OSA_THRD_PRIO_BELOW_NORMAL  (4)
#define OSA_THRD_PRIO_LOWEST        (5)
#define OSA_THRD_PRIO_ABOVE_IDLE    (6)
#define OSA_THRD_PRIO_IDLE          (7)

struct osa_thread_attr {
	uint8_t *name;
	uint32_t prio;
	void *stack_addr;
	uint32_t stack_size;
};

/* for OSA in secure world */
typedef enum osa_thread_status {
	OSA_THRD_STATUS_RUNNING = 0,
	OSA_THRD_STATUS_SLEEPING,
	OSA_THRD_STATUS_SUSPENDED,
	OSA_THRD_STATUS_CREATING,
	OSA_THRD_STATUS_EXITED,
	OSA_THRD_STATUS_UNKOWN,
} osa_thread_status_t;

extern osa_thread_t osa_create_thread(void (*func) (void *), void *arg,
					struct osa_thread_attr *attr);
extern void osa_destroy_thread(osa_thread_t handle);
/* stop thread synchronously */
extern osa_err_t osa_stop_thread(osa_thread_t handle);
/* stop thread asynchronously */
extern osa_err_t osa_stop_thread_async(osa_thread_t handle);
/*
 * user should check the return value of the function frequently
 * in his main loop.
 * if true, the user function should terminate itself as quickly as possible.
 */
extern bool osa_thread_should_stop(void);

extern osa_thread_id_t osa_get_thread_id(osa_thread_t handle);
extern osa_thread_id_t osa_get_thread_group_id(osa_thread_t handle);
extern osa_thread_id_t osa_get_current_thread_id(void);
extern osa_thread_id_t osa_get_current_thread_group_id(void);

#endif /* _OSA_THREAD_H_ */
