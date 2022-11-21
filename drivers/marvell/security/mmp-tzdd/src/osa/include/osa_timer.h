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
 * Filename     : osa_timer.h
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the header file of timer-related functions in osa.
 *
 */

#ifndef _OSA_TIMER_H_
#define _OSA_TIMER_H_

#include <osa.h>

/* create a timer to call func with arg as the argument */
osa_timer_t osa_create_timer(void (*func) (void *), void *arg, uint32_t msec);
/* destroy the timer */
void osa_destroy_timer(osa_timer_t handle);
/* start the timer */
osa_err_t osa_start_timer(osa_timer_t handle);
/* cancel the timer */
osa_err_t osa_cancel_timer(osa_timer_t handle);
/* modify the timer */
osa_err_t osa_modify_timer(osa_timer_t handle, uint32_t msec);

/* APIs called by TZDD */
/* init timer module */
osa_err_t osa_init_timer_module(void);
/* cleanup timer module */
void osa_cleanup_timer_module(void);

#endif /* _OSA_TIMER_H_ */
