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
#include "tzdd_internal.h"
#include "teec_shd.h"
#include "teec_cb.h"
#include "tee_shd_common.h"

#define SHD_DBG(format, a...) TZDD_DBG("%s(%d): " format, __FUNCTION__, __LINE__, ##a)

typedef enum _teec_shd_stat_t
{
	TEEC_SHD_INV,
	TEEC_SHD_READY,
	TEEC_SHD_SHARED,
} teec_shd_stat_t;

typedef struct _teec_shd_rec_t {
	teec_shd_t        shd;

	teec_shd_stat_t   stat;
	osa_mutex_t       stat_lock;

	void              *ntw_addr;
	void              *tw_addr;
	uint32_t          data_sz;

	teec_cb_handle_t  handle;
	osa_list_t        node;

	TEEC_SharedMemory shm;
} teec_shd_rec_t;

static osa_list_t _g_shd_list;
static osa_mutex_t _g_shd_list_lock;
static TEEC_Context _g_shd_context;

static void *_teec_shd_reg_shm(teec_shd_rec_t *shd_rec, void *ntw_data, uint32_t ntw_data_sz)
{
	TEEC_Result ret;

	shd_rec->shm.buffer = ntw_data;
	shd_rec->shm.size = ntw_data_sz;
	shd_rec->shm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	ret = TEEC_RegisterSharedMemory(&_g_shd_context, &shd_rec->shm);
	if (ret != TEEC_SUCCESS) {
		SHD_DBG("failed to register shm\n");
		return NULL;
	}

	return ((_TEEC_MRVL_SharedMemory *)shd_rec->shm.imp)->vaddr_tw;
}

static teec_cb_stat_t _teec_shd_cb_fn(tee_cb_arg_t *arg, teec_cb_cntx_t cntx)
{
	teec_shd_rec_t *rec;
	teec_shd_t *shd;
	tee_shd_cb_cmd_desc_t *cmd_desc;
	int32_t ret;

	OSA_ASSERT(arg->nbytes == sizeof(tee_shd_cb_cmd_desc_t));

	rec = (teec_shd_rec_t *)cntx;
	shd = &rec->shd;
	cmd_desc = (tee_shd_cb_cmd_desc_t *)arg->args;

	switch(cmd_desc->cmd) {
		case TEEC_SHD_CMD_SHARING:
			osa_wait_for_mutex(rec->stat_lock, INFINITE);
			if (rec->stat != TEEC_SHD_READY) {
				osa_release_mutex(rec->stat_lock);
				SHD_DBG("share device in error stat %d\n", rec->stat);
				return TEEC_ERROR_GENERIC;
			}
			if (shd->sharing == NULL) {
				osa_release_mutex(rec->stat_lock);
				return TEEC_SUCCESS;
			}
			ret = shd->sharing(shd, rec->ntw_addr);
			if (ret < 0) {
				osa_release_mutex(rec->stat_lock);
				SHD_DBG("failed to share device %s: %d (%d)\n",
						shd->name, shd->id, ret);
				return TEEC_ERROR_GENERIC;
			}
			cmd_desc->data = rec->tw_addr;

			rec->stat = TEEC_SHD_SHARED;
			osa_release_mutex(rec->stat_lock);
			break;
		case TEEC_SHD_CMD_REVERT:
			osa_wait_for_mutex(rec->stat_lock, INFINITE);
			if (rec->stat != TEEC_SHD_SHARED) {
				osa_release_mutex(rec->stat_lock);
				SHD_DBG("share device in error stat %d\n", rec->stat);
				return TEEC_ERROR_GENERIC;
			}
			if (shd->revert == NULL) {
				osa_release_mutex(rec->stat_lock);
				return TEEC_SUCCESS;
			}
			ret = shd->revert(shd, rec->ntw_addr);
			if (ret < 0) {
				osa_release_mutex(rec->stat_lock);
				SHD_DBG("failed to share device %s: %d (%d)\n",
						shd->name, shd->id, ret);
				return TEEC_ERROR_GENERIC;
			}
			cmd_desc->data = NULL;

			rec->stat = TEEC_SHD_READY;
			osa_release_mutex(rec->stat_lock);
			break;
		default:
			OSA_ASSERT(0);
			break;
	}

	return TEEC_SUCCESS;
}

int32_t teec_register_shd(teec_shd_t *shd)
{
	teec_shd_rec_t *new_rec;
	int32_t ret;
	teec_cb_uuid_t uuid;
	void *ntw_data;
	uint32_t ntw_data_sz;

	if (shd == NULL || shd->magic != TEEC_SHD_MAGIC) {
		return -EINVAL;
	}
	tee_shd_get_uuid(shd->name, shd->type, shd->id, &uuid);

	new_rec = kzalloc(sizeof(teec_shd_rec_t), GFP_KERNEL);
	if (new_rec == NULL) {
		SHD_DBG("failed to alloc new rec\n");
		return -ENOMEM;
	}
	new_rec->shd = *shd;
	osa_list_init_head(&new_rec->node);

	new_rec->stat_lock = osa_create_mutex();
	OSA_ASSERT(new_rec->stat_lock != NULL);

	/* check and register shd */
	new_rec->handle = teec_reg_cb(&uuid, _teec_shd_cb_fn, new_rec);
	if (new_rec->handle == NULL) {
		SHD_DBG("failed to register callback\n");
		kfree(new_rec);
		return -EIO;
	}

	if (shd->init) {
		/* init shd */
		ret = shd->init(&new_rec->shd, &ntw_data, &ntw_data_sz);
		if (ret < 0) {
			SHD_DBG("failed to init shd\n");
			teec_unreg_cb(new_rec->handle);
			kfree(new_rec);
			return ret;
		}
		if (ntw_data != NULL && ntw_data_sz != 0) {
			new_rec->tw_addr = _teec_shd_reg_shm(new_rec, ntw_data, ntw_data_sz);
			if (new_rec->tw_addr == NULL) {
				shd->cleanup(&new_rec->shd);
				teec_unreg_cb(new_rec->handle);
				kfree(new_rec);
				SHD_DBG("failed to register shm\n");
				return -ENODEV;
			}
			new_rec->ntw_addr = ntw_data;
			new_rec->data_sz = ntw_data_sz;
		}
	}

	new_rec->stat = TEEC_SHD_READY;

	osa_wait_for_mutex(_g_shd_list_lock, INFINITE);
	osa_list_add(&new_rec->node, &_g_shd_list);
	osa_release_mutex(_g_shd_list_lock);

	return 0;
}

int32_t teec_unregister_shd(teec_shd_t *shd)
{
	osa_list_t *entry;
	teec_shd_rec_t *rec;

	if (shd == NULL || shd->magic != TEEC_SHD_MAGIC) {
		return -EINVAL;
	}
	osa_wait_for_mutex(_g_shd_list_lock, INFINITE);
	osa_list_iterate(&_g_shd_list, entry) {
		OSA_LIST_ENTRY(entry, teec_shd_rec_t, node, rec);
		if (!strcmp(shd->name, rec->shd.name) && shd->id == rec->shd.id) {
			osa_wait_for_mutex(rec->stat_lock, INFINITE);
			if (rec->stat == TEEC_SHD_SHARED) {
				SHD_DBG("in error stat %d\n", rec->stat);
				return -EIO;
			} else {
				rec->stat = TEEC_SHD_INV;
			}
			osa_release_mutex(rec->stat_lock);
			osa_list_del(&rec->node);
			osa_release_mutex(_g_shd_list_lock);

			shd->cleanup(&rec->shd);

			TEEC_ReleaseSharedMemory(&rec->shm);
			rec->tw_addr = NULL;
			teec_unreg_cb(rec->handle);

			memset(rec, 0, sizeof(teec_shd_rec_t));
			kfree(rec);
			return 0;
		}
	}
	osa_release_mutex(_g_shd_list_lock);
	return -ENODEV;
}

int32_t teec_shd_init(void)
{
	TEEC_Result ret;

	ret = TEEC_InitializeContext(NULL, &_g_shd_context);
	if (ret != TEEC_SUCCESS) {
		SHD_DBG("failed to init context\n");
		return -EINVAL;
	}
	osa_list_init_head(&_g_shd_list);
	_g_shd_list_lock = osa_create_mutex();

	return 0;
}
void teec_shd_cleanup(void)
{
	osa_destroy_mutex(_g_shd_list_lock);
	TEEC_FinalizeContext(&_g_shd_context);
}
