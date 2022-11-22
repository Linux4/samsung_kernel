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

#include <linux/pm_qos.h>
#include <linux/pm_opp.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "npu-vs4l.h"
#include "npu-device.h"
#include "npu-memory.h"
#include "npu-system.h"
#include "npu-qos.h"

static struct npu_qos_setting *qos_setting;
static LIST_HEAD(qos_list);

int npu_qos_probe(struct npu_system *system)
{
	qos_setting = &(system->qos_setting);

	mutex_init(&qos_setting->npu_qos_lock);

	/* qos add request(default_freq) */
	pm_qos_add_request(&qos_setting->npu_qos_req_dnc, PM_QOS_DNC_THROUGHPUT, 0);
	pm_qos_add_request(&qos_setting->npu_qos_req_npu, PM_QOS_NPU_THROUGHPUT, 0);
	pm_qos_add_request(&qos_setting->npu_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&qos_setting->npu_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);

	pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl0, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl1, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	//pm_qos_add_request(&qos_setting->npu_qos_req_cpu_cl2, NPU_PM_CPU_CL2, 0);

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
//	qos_setting->req_cl2_freq = 0;

	return 0;
}

int npu_qos_release(struct npu_system *system)
{
	return 0;
}

int npu_qos_start(struct npu_system *system)
{
	BUG_ON(!system);

	mutex_lock(&qos_setting->npu_qos_lock);

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;

	mutex_unlock(&qos_setting->npu_qos_lock);

	return 0;
}

int npu_qos_stop(struct npu_system *system)
{
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	BUG_ON(!system);

	mutex_lock(&qos_setting->npu_qos_lock);

	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		list_del(pos);
		kfree(qr);
	}
	list_del_init(&qos_list);

	pm_qos_update_request(&qos_setting->npu_qos_req_dnc, 0);
	pm_qos_update_request(&qos_setting->npu_qos_req_npu, 0);
	pm_qos_update_request(&qos_setting->npu_qos_req_mif, 0);
	pm_qos_update_request(&qos_setting->npu_qos_req_int, 0);

	pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
	pm_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;

	mutex_unlock(&qos_setting->npu_qos_lock);

	return 0;
}


s32 __update_freq_from_showcase(__u32 nCategory)
{
	s32 nValue = 0;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if (qr->eCategory == nCategory) {
			nValue = qr->req_freq > nValue ? qr->req_freq : nValue;
			npu_dbg("[U%u]Candidate Freq. category : %u  freq : %d\n",
				qr->sessionUID, nCategory, nValue);
		}
	}

	return nValue;
}

int __req_param_qos(int uid, __u32 nCategory, struct pm_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			cur_value = qr->req_freq;
			npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
					uid, nCategory, cur_value, new_value);
			list_del(pos);
			qr->sessionUID = uid;
			qr->req_freq = new_value;
			qr->eCategory = nCategory;
			list_add_tail(&qr->list, &qos_list);
			rec_value = __update_freq_from_showcase(nCategory);

			if (new_value > rec_value) {
				pm_qos_update_request(req, new_value);
				npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
					uid, nCategory, cur_value, new_value);
			} else {
				pm_qos_update_request(req, rec_value);
				npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
					uid, nCategory, cur_value, rec_value);
			}
			return ret;
		}
	}
	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr) {
		npu_err("memory alloc fail.\n");
		return -ENOMEM;
	}
	qr->sessionUID = uid;
	qr->req_freq = new_value;
	qr->eCategory = nCategory;
	list_add_tail(&qr->list, &qos_list);

	//If new_value is lager than current value, update the freq
	cur_value = (s32)pm_qos_read_req_value(req->pm_qos_class, req);
	npu_dbg("[U%u]New Freq. category : %u freq : %u\n",
		qr->sessionUID, qr->eCategory, qr->req_freq);
	if (cur_value < new_value) {
		npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
			qr->sessionUID, qr->eCategory, qr->req_freq);
		pm_qos_update_request(req, new_value);
	}
	return ret;
}

npu_s_param_ret npu_qos_param_handler(struct npu_session *sess, struct vs4l_param *param, int *retval)
{
	BUG_ON(!sess);
	BUG_ON(!param);

	mutex_lock(&qos_setting->npu_qos_lock);

	switch (param->target) {
	case NPU_S_PARAM_QOS_DNC:
		qos_setting->req_dnc_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dnc,
					qos_setting->req_dnc_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;

	case NPU_S_PARAM_QOS_NPU:
		qos_setting->req_npu_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu,
				qos_setting->req_npu_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;
	case NPU_S_PARAM_QOS_MIF:
		qos_setting->req_mif_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif,
				qos_setting->req_mif_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;
	case NPU_S_PARAM_QOS_INT:
		qos_setting->req_int_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_int,
				qos_setting->req_int_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;
	case NPU_S_PARAM_QOS_CL0:
		qos_setting->req_cl0_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;
	case NPU_S_PARAM_QOS_CL1:
		qos_setting->req_cl1_freq = param->offset;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);

		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_HANDLED;
/*
 *	case NPU_S_PARAM_QOS_CL2:
 *		qos_setting->req_cl2_freq = param->offset;
 *		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2, qos_setting->req_cl2_freq);
 *
 *		mutex_unlock(&qos_setting->npu_qos_lock);
 *		return S_PARAM_HANDLED;
*/
	case NPU_S_PARAM_CPU_AFF:
	case NPU_S_PARAM_QOS_RST:
	default:
		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_NOMB;
	}
}
