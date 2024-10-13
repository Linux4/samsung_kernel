/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-device.h"
#include "npu-system.h"
#include "npu-precision.h"
#include "npu-util-memdump.h"

static struct npu_system *npu_precision_system;

static void __npu_precision_work(int freq)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name)) {
			npu_dvfs_set_freq(d, &d->qos_req_max_precision, freq);
		} else if (!strcmp("DNC", d->name)) {
			if (freq == 1066000) {
				npu_dvfs_set_freq(d, &d->qos_req_max_precision, 800000);
			} else if (freq == 1300000) {
				npu_dvfs_set_freq(d, &d->qos_req_max_precision, 935000);
			}
		}
	}
	mutex_unlock(&info->exec_lock);

	return;
}

static int __npu_precision_active_check(void)
{
	int i, freq = 1300000;
	struct npu_precision_model_info *h;
	struct hlist_node *tmp;

	mutex_lock(&npu_precision_system->model_lock);
	hash_for_each_safe(npu_precision_system->precision_active_hash_head, i, tmp, h, hlist) {
		if (h->active) {
			if (!h->type || (h->type == PRE_INT16) || (h->type == PRE_FP16)) {
				npu_info(" model : %s is %u type\n", h->model_name, h->type);
				freq = 1066000;
				break;
			}
		}
	}
	mutex_unlock(&npu_precision_system->model_lock);

	npu_info("next maxlock is %d\n", freq);
	return freq;
}

static void npu_precision_work(struct work_struct *work)
{
	__npu_precision_work(__npu_precision_active_check());
}

static u32 npu_get_hash_name_key(const char *model_name,
		unsigned int computational_workload,
		unsigned int io_workload) {
	u32 key = 0;
	char c = 0;
	int i = 0;

	key |= ((u32)computational_workload) << 16;
	key |= ((u32)io_workload) & 0x0000FFFF;



	while ((c = *model_name++)) {
		key |= ((u32)c) << ((8 * i++) & 31);
	}

	return key;
}

static bool is_matching_ncp_for_hash_with_session(struct npu_precision_model_info *h, struct npu_session *session) {
	return (!strcmp(h->model_name, session->model_name)) &&
		(h->computational_workload == session->computational_workload) &&
		(h->io_workload == session->io_workload);
}

void npu_precision_hash_check(struct npu_session *session)
{
	u32 key, index;
	u32 find = 0;
	struct npu_precision_model_info *h;

	key = session->key;
	mutex_lock(&npu_precision_system->model_lock);
	hash_for_each_possible(npu_precision_system->precision_info_hash_head, h, hlist, key) {
		if(is_matching_ncp_for_hash_with_session(h, session)) {
			find = PRECISION_REGISTERED;
			npu_dbg("ref : %d, find model : %s[index : %u, type : %u]\n", atomic_read(&npu_precision_system->active_info[h->index].ref), h->model_name, h->index, h->type);
			if ((atomic_inc_return(&npu_precision_system->active_info[h->index].ref) == 0x1)) {
				npu_precision_system->active_info[h->index].active = true;
				npu_precision_system->active_info[h->index].index = h->index;
				npu_precision_system->active_info[h->index].type = h->type;
				npu_precision_system->active_info[h->index].computational_workload = h->computational_workload;
				npu_precision_system->active_info[h->index].io_workload = h->io_workload;
				strncpy(npu_precision_system->active_info[h->index].model_name, h->model_name, NCP_MODEL_NAME_LEN);

				hash_add(npu_precision_system->precision_active_hash_head,
						&npu_precision_system->active_info[h->index].hlist, key);
				break;
			}
		}
	}

	if (!find) {
		u32 ncp_type = 0;
		index = npu_precision_system->precision_index;
		if (index >= PRECISION_LEN) {
			npu_err("Registered Model(index : %u) bigger than precision len(%u)\n", index, PRECISION_LEN);
			BUG_ON(1);
		}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
		ncp_type = session->featuremapdata_type;
#endif
		npu_dbg("new model : %s [ncp_type : %u, index : %u]\n", session->model_name, ncp_type, index);
		npu_precision_system->model_info[index].active = true;
		npu_precision_system->model_info[index].index = index;
		npu_precision_system->model_info[index].type = ncp_type;
		npu_precision_system->model_info[index].computational_workload = session->computational_workload;
		npu_precision_system->model_info[index].io_workload = session->io_workload;
		strncpy(npu_precision_system->model_info[index].model_name, session->model_name, NCP_MODEL_NAME_LEN);

		atomic_inc(&(npu_precision_system->active_info[index].ref));
		npu_precision_system->active_info[index].active = true;
		npu_precision_system->active_info[index].index = index;
		npu_precision_system->active_info[index].type = ncp_type;
		npu_precision_system->active_info[index].computational_workload = session->computational_workload;
		npu_precision_system->active_info[index].io_workload = session->io_workload;
		strncpy(npu_precision_system->active_info[index].model_name, session->model_name, NCP_MODEL_NAME_LEN);

		hash_add(npu_precision_system->precision_info_hash_head,
			&npu_precision_system->model_info[index].hlist, key);
		hash_add(npu_precision_system->precision_active_hash_head,
			&npu_precision_system->active_info[index].hlist, key);

		npu_precision_system->precision_index++;

	}

	mutex_unlock(&npu_precision_system->model_lock);

	__npu_precision_work(__npu_precision_active_check());
}

void npu_precision_hash_update(struct npu_session *session, u32 type)
{
	u32 key, find = 0;
	struct npu_precision_model_info *h;

	{
		struct npu_device *device;

		device = container_of(npu_precision_system, struct npu_device, system);
		if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST ||
				device->sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE) {
			npu_info("skip hash_update on BOOST or PDCL\n");
			return;
		}

	}

	key = session->key;
	mutex_lock(&npu_precision_system->model_lock);
	hash_for_each_possible(npu_precision_system->precision_active_hash_head, h, hlist, key) {
		if(is_matching_ncp_for_hash_with_session(h, session)) {
			find = 0xCAFE;
			h->type = type;
			npu_precision_system->model_info[h->index].type = type;
			break;
		}
	}

	if (!find) {
		npu_err("Active hash is corruptioned - index is not found\n");
		BUG_ON(1);
	}

	mutex_unlock(&npu_precision_system->model_lock);

	queue_delayed_work(npu_precision_system->precision_wq,
			&npu_precision_system->precision_work,
			msecs_to_jiffies(0));
}

void npu_precision_active_hash_delete(struct npu_session *session)
{
	u32 key;
	struct npu_precision_model_info *h;

	key = npu_get_hash_name_key(session->model_name, session->computational_workload, session->io_workload);
	mutex_lock(&npu_precision_system->model_lock);
	hash_for_each_possible(npu_precision_system->precision_active_hash_head, h, hlist, key) {
		if(is_matching_ncp_for_hash_with_session(h, session)) {
			if ((atomic_dec_return(&h->ref) == 0x0)) {
				h->active = false;
				hash_del(&h->hlist);
				break;
			}
		}
	}
	mutex_unlock(&npu_precision_system->model_lock);

	queue_delayed_work(npu_precision_system->precision_wq,
			&npu_precision_system->precision_work,
			msecs_to_jiffies(0));
}

int npu_precision_open(struct npu_system *system)
{
	int ret = 0, i = 0;

	for (i = 0; i < PRECISION_LEN; i++) {
		atomic_set(&system->active_info[i].ref, 0);
	}

	return ret;
}

int npu_precision_close(struct npu_system *system)
{
	int ret = 0;

	cancel_delayed_work_sync(&system->precision_work);

	__npu_precision_work(PRECISION_RELEASE_FREQ);

	return ret;
}

int npu_precision_probe(struct npu_device *device)
{
	int ret = 0;
	struct npu_system *system = &device->system;

	npu_precision_system = system;

	system->precision_index = 0;

	hash_init(system->precision_info_hash_head);
	hash_init(system->precision_active_hash_head);

	mutex_init(&system->model_lock);

	INIT_DELAYED_WORK(&system->precision_work, npu_precision_work);

	system->precision_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->precision_wq) {
		probe_err("fail to create workqueue -> system->precision_wq\n");
		ret = -EFAULT;
		goto err_probe;
	}

	probe_info("NPU precision probe success\n");
	return ret;

err_probe:
	probe_err("NPU precision probe failed\n");
	return ret;
}

int npu_precision_release(struct npu_device *device)
{
	int ret = 0;
	struct npu_precision_model_info *h;
	struct hlist_node *tmp;
	int i;
	struct npu_system *system = &device->system;

	mutex_lock(&system->model_lock);
	hash_for_each_safe(system->precision_info_hash_head, i, tmp, h, hlist) {
		h->active = false;
		h->index = 0;
		h->type = 0;
		h->computational_workload = 0;
		h->io_workload = 0;
		strncpy(h->model_name, "already_unregistered", NCP_MODEL_NAME_LEN);
		hash_del(&h->hlist);
	}
	mutex_unlock(&system->model_lock);

	npu_precision_system = NULL;

	probe_info("NPU precision probe success\n");
	return ret;
}
