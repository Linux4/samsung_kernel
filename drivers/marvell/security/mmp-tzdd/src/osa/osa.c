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
 * Filename     : osa.c
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the source code of init/cleanup functions in osa.
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
 * Name:        osa_init_module
 *
 * Description: the init function of osa module
 *
 * Params:      none
 *
 * Returns:     osa_err_t
 *
 * Notes:       the function is called in tzdd
 */
osa_err_t osa_init_module(void)
{
	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_init_module);

/*
 * Name:        osa_cleanup_module
 *
 * Description: the cleanup function of osa module
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       the function is called in tzdd
 */
void osa_cleanup_module(void)
{
	/* nothing */ ;
}
OSA_EXPORT_SYMBOL(osa_cleanup_module);
