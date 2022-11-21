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

#ifndef _OSA_ERR_H_
#define _OSA_ERR_H_

#define OSA_OK                  (0)
#define OSA_ERR                 (-1)

#define OSA_BAD_ARG             (-10)
#define OSA_OUT_OF_MEM          (-11)
#define OSA_BUSY                (-12)
#define OSA_ITEM_NOT_FOUND      (-13)

#define OSA_SEM_WAIT_FAILED     (-100)
#define OSA_SEM_WAIT_TO         (-101)
#define OSA_MUTEX_WAIT_FAILED   (-102)
#define OSA_MUTEX_WAIT_TO       (-103)
#define OSA_EVENT_WAIT_FAILED   (-102)
#define OSA_EVENT_WAIT_TO       (-103)

#define OSA_THREAD_BAD_STATUS   (-110)
#define OSA_THREAD_STOP_FAILED  (-111)

#define OSA_BAD_USER_VIR_ADDR   (-120)

#define OSA_DUP_DRV             (-130)

#define OSA_TIMER_BAD_STATUS    (-140)
#define OSA_INIT_TIMER_FAILED   (-141)

#endif /* _OSA_ERR_H_ */
