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
 * Filename     : osa_delay.c
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the source code of delay-related functions in osa.
 *
 */

/*
 ******************************
 *          HEADERS
 ******************************
 */

#include <osa.h>

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

/*
 * Name:        osa_sleep
 *
 * Description: sleep in about n micro-seconds.
 *
 * Params:      msec - number of the micro-seconds.
 *
 * Returns:     none
 *
 * Notes:       the function does yield CPU and cause task re-scheduling.
 */
void osa_sleep(uint32_t msec)
{
	msleep(msec);
}
OSA_EXPORT_SYMBOL(osa_sleep);
