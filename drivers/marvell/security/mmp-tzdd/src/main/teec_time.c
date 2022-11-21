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

#include "teec_cb.h"
#include "teec_time.h"
#include "osa.h"

/* vvv sync with tw vvv */

typedef struct _ree_time
{
    uint32_t sec;
    uint32_t nsec;
} ree_time;

static const teec_cb_uuid_t _g_ree_time_uuid = {
    0x79b77788, 0x9789, 0x4a7a,
    { 0xa2, 0xbe, 0xb6, 0x1, 0x74, 0x69, 0x6d, 0x65 },
};

/* ^^^ sync with tw ^^^ */

static teec_cb_handle_t _g_time_cb_h = NULL;

teec_cb_stat_t _teec_get_ree_time(tee_cb_arg_t *args, teec_cb_cntx_t cntx)
{
    ree_time *t;
    struct timespec ts;

    OSA_ASSERT(sizeof(ree_time) == args->nbytes);

    getnstimeofday(&ts);

    t = (ree_time *)args->args;
    t->sec = ts.tv_sec;
    t->nsec = ts.tv_nsec;
    return TEEC_SUCCESS;
}

tee_stat_t teec_reg_time_cb(void)
{
    if (_g_time_cb_h) {
        return TEEC_SUCCESS;
    }

    _g_time_cb_h = teec_reg_cb(&_g_ree_time_uuid, _teec_get_ree_time, NULL);
    if (NULL == _g_time_cb_h) {
        return TEEC_ERROR_GENERIC;
    }

    return TEEC_SUCCESS;
}

void teec_unreg_time_cb(void)
{
    if (_g_time_cb_h) {
        teec_unreg_cb(_g_time_cb_h);
        _g_time_cb_h = NULL;
    }
}
