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

#include "teec_types.h"
#include "tee_cb_common_local.h"
#include "teec_cb.h"
#include "osa.h"
#include "tzdd_internal.h"

#define INIT_CB_MAGIC(_m)       do {                \
                                    (_m)[0] = 'T';  \
                                    (_m)[1] = 'e';  \
                                    (_m)[2] = 'C';  \
                                    (_m)[3] = 'b';  \
                                } while (0)
#define CLEANUP_CB_MAGIC(_m)   do {                 \
                                    (_m)[0] = 0;    \
                                    (_m)[1] = 0;    \
                                    (_m)[2] = 0;    \
                                    (_m)[3] = 0;    \
                                } while (0)
#define IS_CB_MAGIC_VALID(_m)   (('T' == (_m)[0]) && \
                                ('e' == (_m)[1]) && \
                                ('C' == (_m)[2]) && \
                                ('b' == (_m)[3]))

#define TEE_CB_DBG

#ifdef TEE_CB_DBG
#define _teec_cb_err_log(_f, _a...) osa_dbg_print(DBG_ERR, _f, ##_a)
#define _teec_cb_dbg_log(_f, _a...) osa_dbg_print(DBG_INFO, _f, ##_a)
#else /* !TEE_CB_DBG */
#define _teec_cb_dbg_log(_f, _a...)
#endif /* TEE_CB_DBG */

typedef struct __teec_cb_struct_t
{
    /* mgmt */
    uint8_t         magic[4];
    osa_list_t      node;
    osa_mutex_t     lock;
    osa_mutex_t     ready;  /* create/destroy by api, post by worker */
    teec_cb_stat_t  err;    /* returned by worker to api when init-ing */
    bool            stop;   /* stop flag */

    /* data xchg */
    void            *shm;   /* set in worker */

    /* data */
    teec_cb_uuid_t  uuid;
    teec_cb_fn_t    func;
    teec_cb_cntx_t  cntx;
    osa_thread_t    worker;
} _teec_cb_struct_t;

static TEEC_Context _g_cb_cntx; /* no lock */

static OSA_LIST(_g_cb_list);
static osa_mutex_t _g_cb_list_lock;

static const TEEC_UUID _g_cb_uuid = TEE_CB_SRV_UUID;

static void _teec_cb_worker(void *arg)
{
    _teec_cb_struct_t   *cb;
    bool                is_stop;

    TEEC_SharedMemory   shm;
    TEEC_Session        ss;
    TEEC_Operation      op;
    TEEC_Result         res;

    osa_err_t           err;
    teec_cb_stat_t      ret;

    OSA_ASSERT(arg);
    cb = (_teec_cb_struct_t *)arg;
    OSA_ASSERT(IS_CB_MAGIC_VALID(cb->magic));

    memset(&shm, 0, sizeof(TEEC_SharedMemory));
    shm.buffer = NULL;
    shm.size = TEE_CB_SHM_MAX_SZ;
    shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
    res = TEEC_AllocateSharedMemory(&_g_cb_cntx, &shm);
    if (res) {
        _teec_cb_err_log("ERROR - alloc shm err = 0x%08x in %s\n",
                res, __func__);
        cb->err = res;
        osa_release_mutex(cb->ready);
        return;
    }

    /*
     * open-ss:
     * +-----------+--------+------+------+
     * | tmpRefMem | memRef | none | none |
     * |  cb_uuid  |  args  |      |      |
     * +-----------+--------+------+------+
     */
    memset(&op, 0, sizeof(TEEC_Operation));
    op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = &cb->uuid;
    op.params[0].tmpref.size = sizeof(teec_cb_uuid_t);
    op.params[1].memref.parent = &shm;
    op.params[1].memref.size = TEE_CB_SHM_MAX_SZ;
    op.params[1].memref.offset = 0;

    res = TEEC_OpenSession(&_g_cb_cntx, &ss,
            &_g_cb_uuid, MRVL_LOGIN_APPLICATION_DAEMON,
            NULL, &op, NULL);
    if (res) {
        _teec_cb_err_log("ERROR - open ss err = 0x%08x in %s\n", res, __func__);
        TEEC_ReleaseSharedMemory(&shm);
        cb->err = res;
        osa_release_mutex(cb->ready);
        return;
    }

    /* release reg-api */
    cb->err = TEEC_SUCCESS;

    /* *BEFORE* releasing cb->ready, cb->lock is a must */

    err = osa_wait_for_mutex(cb->lock, INFINITE);
    OSA_ASSERT(OSA_OK == err);

    osa_release_mutex(cb->ready);

    /* now ready to trap in tw */
    ret = TEEC_SUCCESS;
    while (1) {
        is_stop = cb->stop;
        /*
         * inv-op:
         * +-------+------+------+------+
         * | val.a | none | none | none |
         * |  ret  |      |      |      |
         * | val.b |      |      |      |
         * |  ctrl |      |      |      |
         * +-------+------+------+------+
         */
        memset(&op, 0, sizeof(TEEC_Operation));
        op.paramTypes = TEEC_PARAM_TYPES(
                TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        op.params[0].value.a = ret;
        if (is_stop) {
            op.params[0].value.b = TEE_CB_CTRL_BIT_DYING; /* stop-flag */
        }

        osa_release_mutex(cb->lock);

        res = TEEC_InvokeCommand(&ss, TEE_CB_FETCH_REQ, &op, NULL);
        OSA_ASSERT(res == TEEC_SUCCESS);

        err = osa_wait_for_mutex(cb->lock, INFINITE);
        OSA_ASSERT(OSA_OK == err);

        /*
         * NOTE
         * here comes is_stop rather than cp->stop
         * we need to let tw know stop-flag first
         * so tw-callers will not hang
         */
        if (!is_stop && res) {
            /* FIXME how to handle this err? */
            continue;
        }

        if (!is_stop) {
            if (!(TEE_CB_CTRL_BIT_FAKE & op.params[0].value.b)) {
                /* functionality flow */
                if (TEE_CB_IS_ARG_NULL(((tee_cb_arg_t *)shm.buffer)->nbytes)) {
                    ret = cb->func(NULL, cb->cntx);
                } else {
                    ret = cb->func(shm.buffer, cb->cntx);
                }
            } else {
                /* no real call for fake ntw-calling */
                ret = TEEC_SUCCESS;
            }
        } else {
            /* for real exit*/
            osa_release_mutex(cb->lock);
            break;
        }
    }

    TEEC_CloseSession(&ss);
    TEEC_ReleaseSharedMemory(&shm);
}

teec_cb_handle_t teec_reg_cb(const teec_cb_uuid_t *uuid,
                    teec_cb_fn_t func, teec_cb_cntx_t cntx)
{
    _teec_cb_struct_t       *cb;
    osa_list_t              *entry;
    bool                    found = false;
    struct osa_thread_attr  attr;
    osa_err_t               err;

    if ((NULL == uuid) || (NULL == func)) {
        _teec_cb_err_log("ERROR - invalid param(s) in %s\n", __func__);
        return NULL;
    }

    err =osa_wait_for_mutex(_g_cb_list_lock, INFINITE);
    OSA_ASSERT(OSA_OK == err);

    osa_list_iterate(&_g_cb_list, entry) {
        OSA_LIST_ENTRY(entry, _teec_cb_struct_t, node, cb);
        if (!memcmp(uuid, &cb->uuid, sizeof(teec_cb_uuid_t))) {
            found = true;
            break;
        }
    }

    if (found) {
        osa_release_mutex(_g_cb_list_lock);
        _teec_cb_err_log("ERROR - uuid has been reg-ed in %s\n", __func__);
        return NULL;
    }

    cb = osa_kmem_alloc(sizeof(_teec_cb_struct_t));
    OSA_ASSERT(cb);

    INIT_CB_MAGIC(cb->magic);
    memcpy(&cb->uuid, uuid, sizeof(teec_cb_uuid_t));
    cb->func = func;
    cb->cntx = cntx;
    cb->lock = osa_create_mutex();
    OSA_ASSERT(cb->lock);
    cb->ready = osa_create_mutex_locked();
    OSA_ASSERT(cb->ready);
    cb->stop = false;
    attr.name = "tee-cb";
    attr.prio = 0;             /* highest prio */
    attr.stack_addr = NULL;    /* not used */
    attr.stack_size = 0;       /* not used */
    cb->worker = osa_create_thread(_teec_cb_worker, cb, &attr);
    OSA_ASSERT(cb->worker);

    err = osa_wait_for_mutex(cb->ready, INFINITE);
    OSA_ASSERT(OSA_OK == err);

    if (cb->err) {
        /* cb is not available so safe to free list-lock */
        osa_release_mutex(_g_cb_list_lock);

        /* free res, no lock req */
        osa_destroy_thread(cb->worker);
        osa_destroy_mutex(cb->ready);
        osa_destroy_mutex(cb->lock);
        CLEANUP_CB_MAGIC(cb->magic);
        osa_kmem_free(cb);

        return NULL;
    } else {
        osa_list_add(&cb->node, &_g_cb_list);
        osa_release_mutex(_g_cb_list_lock);
        return (teec_cb_handle_t)cb;
    }
}
EXPORT_SYMBOL(teec_reg_cb);

void teec_unreg_cb(teec_cb_handle_t handle)
{
    _teec_cb_struct_t   *cb;
    TEEC_Session        ss;
    TEEC_Operation      op;
    TEEC_Result         res;
    osa_err_t           err;

    if (NULL == handle) {
        _teec_cb_err_log("ERROR - invalid param(s) in %s\n", __func__);
        return;
    }

    err = osa_wait_for_mutex(_g_cb_list_lock, INFINITE);
    OSA_ASSERT(OSA_OK == err);

    cb = (_teec_cb_struct_t *)handle;
    if (!IS_CB_MAGIC_VALID(cb->magic)) {
        osa_release_mutex(_g_cb_list_lock);
        _teec_cb_err_log("ERROR - invalid param(s) in %s\n", __func__);
        return;
    }

    /* cb lock should be acquired before releasing cb-list-lock */
    err = osa_wait_for_mutex(cb->lock, INFINITE);
    OSA_ASSERT(OSA_OK == err);
    osa_list_del(&cb->node);
    /* make cb invalid before release list-lock */
    CLEANUP_CB_MAGIC(cb->magic);
    /* no cb now, safe to unlick list */
    osa_release_mutex(_g_cb_list_lock);

    /* set stop */
    cb->stop = true;
    osa_release_mutex(cb->lock);

    /*
     * open-ss:
     * +-----------+------+------+------+
     * | tmpRefMem | none | none | none |
     * |  cb_uuid  |      |      |      |
     * +-----------+------+------+------+
     *
     * the 2nd param is none for trig-stop ss.
     */
    memset(&op, 0, sizeof(TEEC_Operation));
    op.paramTypes = TEEC_PARAM_TYPES(
            TEEC_MEMREF_TEMP_INPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = &cb->uuid;
    op.params[0].tmpref.size = sizeof(teec_cb_uuid_t);
    res = TEEC_OpenSession(&_g_cb_cntx, &ss,
            &_g_cb_uuid, TEEC_LOGIN_APPLICATION, NULL, &op, NULL);
    if (TEEC_SUCCESS == res) {
        (void)TEEC_InvokeCommand(&ss, TEE_CB_TRIGGER_STOP, NULL, NULL);
        TEEC_CloseSession(&ss);
    } else {
        /* FIXME - may hang here, no solution */
    }

    /* free res */
    osa_destroy_thread(cb->worker);
    osa_destroy_mutex(cb->ready);
    osa_destroy_mutex(cb->lock);
    osa_kmem_free(cb);
}
EXPORT_SYMBOL(teec_unreg_cb);

teec_cb_stat_t teec_cb_module_init(void)
{
    TEEC_Result res = TEEC_InitializeContext(NULL, &_g_cb_cntx);
    if (res) {
        _teec_cb_err_log("ERROR - init cntx err = 0x%08x in %s\n",
                res, __func__);
        return res;
    }
    _g_cb_list_lock = osa_create_mutex();
    OSA_ASSERT(_g_cb_list_lock);
    return TEEC_SUCCESS;
}

void teec_cb_module_cleanup(void)
{
    /* we do not cover list-not-empty case */
    OSA_ASSERT(osa_list_empty(&_g_cb_list));
    osa_destroy_mutex(_g_cb_list_lock);
    TEEC_FinalizeContext(&_g_cb_cntx);
}
