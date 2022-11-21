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

#ifndef _TZDD_INTERNAL_H_
#define _TZDD_INTERNAL_H_

#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/irqflags.h>
#include <linux/cred.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/atomic.h>

#include "tee_mrvl_imp.h"
#include "tee_client_api.h"
#include "tzdd_ioctl.h"
#include "osa.h"
#include "tee_msgm_ntw.h"
#include "tee_cm.h"
#include "tee_memm.h"
#include "tzdd_sstd.h"
#include "tee_perf.h"

#include "teec_cb_local.h"
#include "teec_time.h"

/*
 * Trustzone driver struct
 */
#define DEVICE_NAME         "tzdd"
#define DEVICE_NAME_SIZE    4

typedef struct {
	struct cdev cdev;
	struct task_struct *proxy_thd;

	osa_list_t req_list;
	osa_mutex_t req_mutex;

	osa_sem_t pt_sem;

} tzdd_dev_t;

extern tzdd_dev_t *tzdd_dev;

extern TEEC_Result _teec_initialize_context(const char *name,
					    tee_impl *tee_ctx_ntw);
extern TEEC_Result _teec_final_context(tee_impl tee_ctx_ntw);
extern TEEC_Result _teec_map_shared_mem(teec_map_shared_mem_args
					teec_map_shared_mem,
					tee_impl *tee_shm_ntw);
extern TEEC_Result _teec_unmap_shared_mem(teec_unmap_shared_mem_args
					  teec_unmap_shared_mem,
					  tee_impl tee_shm_ntw);
extern TEEC_Result _teec_open_session(tee_impl tee_ctx_ntw,
				      tee_impl *tee_ss_ntw,
				      const TEEC_UUID *destination,
				      uint32_t connectionMethod,
				      const void *connectionData,
				      uint32_t flags, uint32_t paramTypes,
				      TEEC_Parameter(*params)[4],
				      tee_impl *tee_op_ntw,
				      tee_impl tee_op_ntw_for_cancel,
				      uint32_t *returnOrigin,
				      unsigned long arg,
				      teec_open_ss_args open_ss_args);
extern TEEC_Result _teec_close_session(tee_impl tee_ss_ntw);
extern TEEC_Result _teec_invoke_command(tee_impl tee_ss_ntw,
					uint32_t cmd_ID,
					uint32_t started,
					uint32_t flags,
					uint32_t paramTypes,
					TEEC_Parameter(*params)[4],
					tee_impl *tee_op_ntw,
					tee_impl tee_op_ntw_for_cancel,
					uint32_t *returnOrigin,
					unsigned long arg,
					teec_invoke_cmd_args invoke_cmd_args);
extern TEEC_Result _teec_request_cancellation(tee_impl tee_op_ntw);

extern void tzdd_add_node_to_req_list(osa_list_t *list);
extern bool tzdd_chk_node_on_req_list(osa_list_t *list);
extern bool tzdd_del_node_from_req_list(osa_list_t *list);

/* get request node from request list */
extern bool tzdd_get_first_req(tee_msg_head_t **tee_msg_head);

/* check request list and run list are empty or not */
extern bool tzdd_have_req(void);
extern uint32_t tzdd_get_req_num_in_list(void);

extern int tzdd_proxy_thread_init(tzdd_dev_t *dev);
extern void tzdd_proxy_thread_cleanup(tzdd_dev_t *dev);

extern int32_t tzdd_init_core(tzdd_dev_t *dev);
extern void tzdd_cleanup_core(tzdd_dev_t *dev);

#ifdef TZDD_DEBUG
#define TZDD_DBG(fmt, args...) \
			osa_dbg_print(DBG_INFO, DEVICE_NAME ": " fmt, ##args)
#else
#define TZDD_DBG(fmt, args...)
#endif

#define osa_memset      memset
#define osa_memcmp      memcmp
#define osa_memcpy      memcpy

#define container_of(ptr, type, member) ({          \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})


#endif /* _TZDD_INTERNAL_H_ */
