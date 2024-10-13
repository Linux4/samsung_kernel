/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-sessionmgr.h"
#include "npu-hw-device.h"
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-config.h"
#endif

int npu_sessionmgr_probe(struct npu_sessionmgr *sessionmgr)
{
	int i = 0, ret = 0;
	mutex_init(&sessionmgr->mlock);

	for (i = 0; i < NPU_MAX_SESSION; i++)
		sessionmgr->session[i] = NULL;
	atomic_set(&sessionmgr->session_cnt, 0);
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	atomic_set(&sessionmgr->queue_cnt, 0);
#endif
#if IS_ENABLED(CONFIG_NPU_USE_HW_DEVICE)
	for (i = 0; i < NPU_MAX_SESSION; i++)
		sessionmgr->unified_id_map_cnt[i] = 0;

	ret = npu_util_bitmap_init(&sessionmgr->unified_id_map, "unified_bitmap", NPU_MAX_SESSION);
	if (ret)
		probe_err("npu_util_bitmap_init failed(%d)\n", ret);
#endif

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	hash_init(sessionmgr->model_info_hash_head);

	npu_sessionmgr_init_vars(sessionmgr);
#endif
	return ret;
}
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
int npu_sessionmgr_release(struct npu_sessionmgr *sessionmgr)
{
	struct npu_model_info_hash *h;
	struct hlist_node *tmp;
	int i;
	hash_for_each_safe(sessionmgr->model_info_hash_head, i, tmp, h, hlist) {
		hash_del(&h->hlist);
		kfree(h);
	}
	return 0;
}

int npu_reduce_mif_all_session(struct npu_sessionmgr *sessionmgr)
{
	int i;
	struct vs4l_param param;

	mutex_lock(&sessionmgr->mlock);
	/*Search and reduce MIF from all sessions */
	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if (!sessionmgr->session[i])
			continue;
		param.target = NPU_S_PARAM_QOS_MIF;
		param.offset = NPU_QOS_DEFAULT_VALUE;
		npu_qos_param_handler(sessionmgr->session[i], &param);
		param.target = NPU_S_PARAM_QOS_INT;
		param.offset = NPU_QOS_DEFAULT_VALUE;
		npu_qos_param_handler(sessionmgr->session[i], &param);
	}
	mutex_unlock(&sessionmgr->mlock);
	return 0;
}
#endif
int npu_sessionmgr_open(struct npu_sessionmgr *sessionmgr)
{
	int i = 0, ret = 0;

	for (i = 0; i < NPU_MAX_SESSION; i++)
		sessionmgr->unified_id_map_cnt[i] = 0;

	npu_util_bitmap_zero(&sessionmgr->unified_id_map);
	return ret;
}

/*
int npu_sessionmgr_close(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;


	return ret;
}

int npu_sessionmgr_start(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;

	return ret;
}

int npu_sessionmgr_stop(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;

	return ret;
}
*/

int npu_sessionmgr_regID(struct npu_sessionmgr *sessionmgr, struct npu_session *session)
{
	int ret = 0;
	u32 index;

	BUG_ON(!sessionmgr);
	BUG_ON(!session);

	mutex_lock(&sessionmgr->mlock);

	for (index = 0; index < NPU_MAX_SESSION; index++) {
		if (!sessionmgr->session[index]) {
			sessionmgr->session[index] = session;
			session->uid = index;
			atomic_inc(&sessionmgr->session_cnt);
			break;
		}
	}

	mutex_unlock(&sessionmgr->mlock);
	session->global_lock = &sessionmgr->mlock;

	if (index >= NPU_MAX_SESSION)
		return -EINVAL;

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	mutex_lock(&sessionmgr->active_list_lock);
	list_add_tail(&session->active_session, &sessionmgr->active_session_list);
	mutex_unlock(&sessionmgr->active_list_lock);
#endif

	return ret;
}

int npu_sessionmgr_unregID(struct npu_sessionmgr *sessionmgr, struct npu_session *session)
{
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct npu_session *cur, *next;
#endif

	BUG_ON(!sessionmgr);
	BUG_ON(!session);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	mutex_lock(&sessionmgr->active_list_lock);
	list_for_each_entry_safe(cur, next, &sessionmgr->active_session_list, active_session) {
		if (cur == session) {
			list_del(&cur->active_session);
			break;
		}
	}
	mutex_unlock(&sessionmgr->active_list_lock);
#endif
	mutex_lock(&sessionmgr->mlock);
	sessionmgr->session[session->uid] = NULL;
	atomic_dec(&sessionmgr->session_cnt);
	mutex_unlock(&sessionmgr->mlock);

	return 0;
}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
struct npu_session *get_session(struct npu_sessionmgr *sessionmgr, int uid)
{
	return sessionmgr->session[uid];
}
#endif

int npu_sessionmgr_set_unifiedID(struct npu_session *session)
{
	int i = 0, ret = 0, id = -1;
	struct npu_sessionmgr *sessionmgr;

	WARN_ON(!session);
	mutex_lock(session->global_lock);
	sessionmgr = session->cookie;

	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if (sessionmgr->session[i]) {
			if ((sessionmgr->session[i]->uid != session->uid) &&
					(sessionmgr->session[i]->unified_op_id == session->unified_op_id)) {
				session->unified_id = sessionmgr->session[i]->unified_id;
				id = sessionmgr->session[i]->unified_id;
				sessionmgr->unified_id_map_cnt[id]++;
				npu_info("reuse unified id = %d\n", session->unified_id);
				break;
			}
		}
	}

	if (id == -1) {
		id = npu_util_bitmap_set_region(&sessionmgr->unified_id_map, 1);
		if (id <= -1) {
			npu_err("Failed to allocate unified bitmap\n");
			ret = -ENODATA;
			mutex_unlock(session->global_lock);
			return ret;
		}
		session->unified_id = id;
		sessionmgr->unified_id_map_cnt[id]++;
		npu_info("create unified id = %d\n", session->unified_id);
	}
	mutex_unlock(session->global_lock);

	return ret;
}

void npu_sessionmgr_unset_unifiedID(struct npu_session *session)
{
	struct npu_sessionmgr *sessionmgr;

	WARN_ON(!session);
	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	sessionmgr->unified_id_map_cnt[session->unified_id]--;
	if (sessionmgr->unified_id_map_cnt[session->unified_id]) {
		mutex_unlock(session->global_lock);
		return;
	}

	if (session->ss_state & BIT(NPU_SESSION_STATE_GRAPH_ION_MAP))
		npu_util_bitmap_clear_region(&sessionmgr->unified_id_map, session->unified_id, 1);

	mutex_unlock(session->global_lock);
}
