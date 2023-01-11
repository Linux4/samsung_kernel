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
#ifndef __TEE_CB_COMMON_LOCAL_H__
#define __TEE_CB_COMMON_LOCAL_H__

#include "tee_cb_common.h"

#define TEE_CB_SHM_MAX_SZ       (0x1000)

#define TEE_CB_SRV_UUID \
    { \
        0x00000004, 0x0000, 0x0000, \
        {                           \
            0x00, 0x00, 0x00, 0x00, \
            0x00, 0x00, 0x00, 0x00, \
        },                          \
	}

#define TEE_CB_FETCH_REQ        (0x12345678)
#define TEE_CB_TRIGGER_STOP     (0x87654321)

/*
 * this ctrl-bit indicates unreg-cb happens in ntw.
 * set by ntw and read by tw.
 */
#define TEE_CB_CTRL_BIT_DYING   (0x00000001)
/*
 * this ctrl-bit means a fake call from tw to ntw.
 * no need to call ntw callback actually.
 * set by tw and read by ntw.
 */
#define TEE_CB_CTRL_BIT_FAKE    (0x00010000)

/* indicator for NULL arg when calling ntw */
#define TEE_CB_ARG_NULL         (-1)
#define TEE_CB_IS_ARG_NULL(_n)  (TEE_CB_ARG_NULL == (_n))

#endif /* __TEE_CB_COMMON_LOCAL_H__ */
