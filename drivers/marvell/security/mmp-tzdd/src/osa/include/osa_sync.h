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
 */

#ifndef _OSA_SYNC_H_
#define _OSA_SYNC_H_

#include "osa.h"

extern osa_sem_t osa_create_sem(int32_t init_val);
extern void osa_destroy_sem(osa_sem_t handle);
extern osa_err_t osa_wait_for_sem(osa_sem_t handle, int32_t msec);
extern osa_err_t osa_try_to_wait_for_sem(osa_sem_t handle);
extern void osa_release_sem(osa_sem_t handle);

extern osa_sem_t osa_create_rw_sem(int32_t init_val);
extern void osa_destroy_rw_sem(osa_sem_t handle);
extern osa_err_t osa_wait_for_rw_sem(osa_sem_t handle, int32_t msec);
extern osa_err_t osa_try_to_wait_for_rw_sem(osa_sem_t handle);
extern void osa_release_rw_sem(osa_sem_t handle);

extern osa_mutex_t osa_create_mutex(void);
extern osa_mutex_t osa_create_mutex_locked(void);
extern void osa_destroy_mutex(osa_mutex_t handle);
extern osa_err_t osa_wait_for_mutex(osa_mutex_t handle, int32_t msec);
extern osa_err_t osa_try_to_wait_for_mutex(osa_mutex_t handle);
extern void osa_release_mutex(osa_mutex_t handle);

extern osa_event_t osa_create_event(bool is_set);
extern void osa_destroy_event(osa_event_t handle);
extern osa_err_t osa_wait_for_event(osa_event_t handle, int32_t msec);
extern osa_err_t osa_try_to_wait_for_event(osa_event_t handle);
extern void osa_set_event(osa_event_t handle);
extern void osa_reset_event(osa_event_t handle);

#endif /* _OSA_SYNC_H_ */
