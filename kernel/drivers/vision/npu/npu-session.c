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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "npu-log.h"
#include "npu-profile-v2.h"
#include "npu-device.h"
#include "npu-common.h"
#include "npu-session.h"
#include "npu-system.h"
#include "npu-scheduler.h"
#include "npu-util-common.h"
#include "npu-hw-device.h"
#include "npu-dvfs.h"
#include "dsp-dhcp.h"
#include "npu-protodrv.h"
#include "npu-precision.h"
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "dsp-kernel.h"
#include "dl/dsp-dl-engine.h"
#include "dsp-common.h"
#endif
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif

#ifdef NPU_LOG_TAG
#undef NPU_LOG_TAG
#endif
#define NPU_LOG_TAG		"npu-sess"

//#define SYSMMU_FAULT_TEST
#define KPI_FRAME_MAX_BUFFER	(16)
#define KPI_FRAME_PRIORITY	(200)
static struct npu_frame kpi_frame[KPI_FRAME_MAX_BUFFER];
const struct npu_session_ops npu_session_ops;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
static u32 dl_unique_id = 0;
#endif

static void __npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf,
						enum dma_data_direction dir)
{
	if (dir == DMA_TO_DEVICE)
		dma_sync_sg_for_device(pbuf->attachment->dev,
			pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_TO_DEVICE);
	else // dir == DMA_FROM_DEVICE
		dma_sync_sg_for_cpu(pbuf->attachment->dev,
			pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_FROM_DEVICE);
}

void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, enum dma_data_direction dir)
{
	if (likely(pbuf->vaddr))
		__npu_session_ion_sync_for_device(pbuf, dir);
}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
static int __remove_buf_from_load_msg(struct npu_session *session)
{
	struct npu_memory_buffer *buf, *temp;

	if (!(session->buf_count) && !(session->hids & NPU_HWDEV_ID_DSP))
		return 0;

	list_for_each_entry_safe(buf, temp, &session->buf_list, dsp_list) {
		npu_info("load unmap fd : %d\n", buf->m.fd);
		npu_memory_unmap(session->memory, buf);
		list_del(&buf->dsp_list);
		kfree(buf);
		session->buf_count--;
	}
	if (session->buf_count) {
		npu_err("fail to remove buf in buf list.(%d)\n", session->buf_count);
		return -1;
	}
	return 0;
}

static int __remove_buf_from_exe_msg(struct npu_session *session)
{
	struct npu_memory_buffer *buf, *temp;

	if (!session->update_count)
		return 0;

	list_for_each_entry_safe(buf, temp, &session->update_list, dsp_list) {
		npu_info("exe unmap fd : %d\n", buf->m.fd);
		npu_memory_unmap(session->memory, buf);
		list_del(&buf->dsp_list);
		kfree(buf);
		session->update_count--;
	}
	if (session->update_count) {
		npu_err("fail to remove buf in update list.(%d)\n", session->update_count);
		return -1;
	}
	return 0;
}
#endif

npu_errno_t chk_nw_result_no_error(struct npu_session *session)
{
	return session->nw_result.result_code;
}

int npu_session_save_result(struct npu_session *session, struct nw_result nw_result)
{
	session->nw_result = nw_result;
	wake_up(&session->wq);
	return 0;
}

void npu_session_queue_done(struct npu_queue *queue, struct npu_queue_list *inclist, struct npu_queue_list *otclist, unsigned long flag)
{
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	u32 buff_cnt, freq_index;

	buff_cnt = otclist->containers[0].count;
	freq_index = npu_update_cmdq_progress_from_done(queue);
	npu_queue_done(queue, inclist, otclist, flag);
	npu_update_frequency_from_done(queue, freq_index);
#else
	u32 buff_cnt;

	buff_cnt = otclist->containers[0].count;
	npu_queue_done(queue, inclist, otclist, flag);
#endif
}

static save_result_func get_notify_func(const nw_cmd_e nw_cmd)
{
	if (unlikely(nw_cmd == NPU_NW_CMD_CLEAR_CB))
		return NULL;
	else
		return npu_session_save_result;
}

static int npu_session_put_nw_req(struct npu_session *session, nw_cmd_e nw_cmd)
{
	int ret = 0;

	struct npu_nw req = {
		.uid = session->uid,
		.bound_id = session->sched_param.bound_id,
		.priority = session->sched_param.priority,
		.deadline = session->sched_param.deadline,
		.preference = session->preference,
		.session = session,
		.cmd = nw_cmd,
		.ncp_addr = session->ncp_info.ncp_addr,
		.magic_tail = NPU_NW_MAGIC_TAIL,
	};

	BUG_ON(!session);

	req.notify_func = get_notify_func(nw_cmd);

	ret = npu_ncp_mgmt_put(&req);
	if (unlikely(!ret)) {
		npu_uerr("npu_ncp_mgmt_put failed(%d)", session, ret);
		return ret;
	}
	return ret;
}

#if (IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR) || IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2))
static inline struct npu_system *get_session_system(struct npu_session *session)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	WARN_ON(!session);
	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;
	return system;
}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
static struct imb_size_control *imb_size_ctl_ref;

static inline u32 __calc_imb_instance_n(struct npu_session *session, bool free)
{
	u32 idx, largest = 0, second_largest = 0;
	struct npu_session *session_ref;
	struct npu_sessionmgr *sessionmgr = session->cookie;

	WARN_ON(!sessionmgr);
	mutex_lock(session->global_lock);
	for (idx = 0; idx < NPU_MAX_SESSION; idx++) {
		if (sessionmgr->session[idx]) {
			session_ref = sessionmgr->session[idx];

			if (!session_ref->IMB_mem_buf)
				continue;

			if (free && (session_ref == session))
				continue;

			if (session_ref->is_instance_1)
				continue;

			npu_uinfo("[INSTANCE N] idx : %d - imb_size : %ld\n",
					session_ref, idx, session_ref->IMB_mem_buf->ncp_max_size);
			if (session_ref->IMB_mem_buf->ncp_max_size > largest) {
				second_largest = largest;
				largest = session_ref->IMB_mem_buf->ncp_max_size;
			} else if (session_ref->IMB_mem_buf->ncp_max_size > second_largest) {
				second_largest = session_ref->IMB_mem_buf->ncp_max_size;
			}
		}
	}
	mutex_unlock(session->global_lock);

	npu_uinfo("[INSTANCE_N] largest : %u, second_largest : %u\n", session, largest, second_largest);
	return largest + second_largest;
}

static inline u32 __calc_imb_instance_1(struct npu_session *session, bool free)
{
	u32 idx, largest = 0;
	struct npu_session *session_ref;
	struct npu_sessionmgr *sessionmgr = session->cookie;

	WARN_ON(!sessionmgr);
	mutex_lock(session->global_lock);
	for (idx = 0; idx < NPU_MAX_SESSION; idx++) {
		if (sessionmgr->session[idx]) {
			session_ref = sessionmgr->session[idx];

			if (!session_ref->IMB_mem_buf)
				continue;

			if (free && (session_ref == session))
				continue;

			if (!session_ref->is_instance_1)
				continue;

			npu_uinfo("[INSTANCE 1] idx : %d - imb_size : %ld\n",
					session_ref, idx, session_ref->IMB_mem_buf->ncp_max_size);
			if (session_ref->IMB_mem_buf->ncp_max_size > largest)
				largest = session_ref->IMB_mem_buf->ncp_max_size;
		}
	}
	mutex_unlock(session->global_lock);

	npu_uinfo("[INSTANCE_1] largest : %u\n", session, largest);
	return largest;
}

static inline u32 __calc_imb_req_chunk_v1(struct npu_memory_buffer *IMB_mem_buf, u32 core_num)
{
	u32 req_chunk_cnt, req_size;

	WARN_ON(!IMB_mem_buf);

	req_size = IMB_mem_buf->ncp_max_size * core_num;
	req_size = ALIGN(req_size, NPU_IMB_GRANULARITY);
	req_chunk_cnt = req_size / NPU_IMB_CHUNK_SIZE;
	if (unlikely(req_chunk_cnt > NPU_IMB_CHUNK_MAX_NUM))
		req_chunk_cnt = NPU_IMB_CHUNK_MAX_NUM;

	return req_chunk_cnt;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
static inline u32 __calc_imb_req_chunk_v2(struct npu_session *session, bool free)
{
	u32 req_chunk_cnt, req_size;

	req_size = MAX(__calc_imb_instance_1(session, free), __calc_imb_instance_n(session, free));
	req_size = ALIGN(req_size, NPU_IMB_GRANULARITY);
	req_chunk_cnt = req_size / NPU_IMB_CHUNK_SIZE;
	if (unlikely(req_chunk_cnt > NPU_IMB_CHUNK_MAX_NUM))
		req_chunk_cnt = NPU_IMB_CHUNK_MAX_NUM;

	return req_chunk_cnt;
}

static inline u32 __calc_imb_req_chunk(struct npu_session *session,
					struct npu_memory_buffer *IMB_mem_buf, bool free)
{
	struct npu_system *system = get_session_system(session);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev < 2))
		return __calc_imb_req_chunk_v1(IMB_mem_buf, system->max_npu_core);
	else
		return __calc_imb_req_chunk_v2(session, free);
#else
	return __calc_imb_req_chunk_v1(IMB_mem_buf, system->max_npu_core);
#endif
}

static int __npu_update_imb_size_save_result(struct npu_session *dummy_session, struct nw_result nw_result)
{
	int ret = 0;

	WARN_ON(!imb_size_ctl_ref);

	imb_size_ctl_ref->result_code = nw_result.result_code;
	imb_size_ctl_ref->fw_size = nw_result.nw.result_value;
	wake_up(&imb_size_ctl_ref->waitq);

	return ret;
}

static int __npu_update_imb_size(struct npu_session *session, u32 new_chunk_cnt)
{
	int ret_req;
	struct npu_system *system = get_session_system(session);
	struct npu_nw req = {
		.uid = session->uid,
		.cmd = NPU_NW_CMD_IMB_SIZE,
		.notify_func = __npu_update_imb_size_save_result,
		.param0 = new_chunk_cnt * NPU_IMB_CHUNK_SIZE,
	};

	WARN_ON(!system);
	imb_size_ctl_ref = &system->imb_size_ctl;
	imb_size_ctl_ref->result_code = NPU_SYSTEM_JUST_STARTED;

	/* queue the message for sending */
	ret_req = npu_ncp_mgmt_put(&req);
	if (unlikely(!ret_req)) {
		npu_uerr("IMB: npu_ncp_mgmt_put failed(%d)\n", session, ret_req);
		return -EWOULDBLOCK;
	}

	/* wait for response from FW */
	wait_event(imb_size_ctl_ref->waitq, imb_size_ctl_ref->result_code != NPU_SYSTEM_JUST_STARTED);

	if (unlikely(imb_size_ctl_ref->result_code != NPU_ERR_NO_ERROR)) {
		/* firmware failed or did not accept the change in IMB size */
		npu_uerr("IMB: failure response for IMB_SIZE message, result code = %d\n",
			 session, imb_size_ctl_ref->result_code);
	}

	return imb_size_ctl_ref->result_code;
}

static int __fw_sync_CMD_IMB_SIZE(struct npu_session *session, u32 new_chunk_cnt)
{
	int ret;
	u32 fw_chunk_cnt;
	struct npu_system *system = get_session_system(session);

	WARN_ON(!system);
	ret = __npu_update_imb_size(session, new_chunk_cnt);
	if (likely(!ret)) {
		fw_chunk_cnt = system->imb_size_ctl.fw_size / NPU_IMB_CHUNK_SIZE;
		if (fw_chunk_cnt == new_chunk_cnt) {
			npu_udbg("IMB: new size 0x%X was accepted by FW\n", session, fw_chunk_cnt * NPU_IMB_CHUNK_SIZE);
		} else {
			npu_udbg("IMB: new size 0x%X will be updated. currently, FW uses IMB size 0x%X\n",
				 session, new_chunk_cnt * NPU_IMB_CHUNK_SIZE, fw_chunk_cnt * NPU_IMB_CHUNK_SIZE);
		}
	} else {
		npu_uerr("IMB: sending IMB_SIZE command to FW failed\n", session);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
#define	NPU_IMB_ASYNC_THRESHOLD		(0x1000000)

static void npu_imb_async_work(struct work_struct *work)
{
	int ret = 0;
	u32 i = 0;
	u32 address_vector_offset;
	struct address_vector *av;
	char *ncp_vaddr;
	u32 IMB_cnt;
	u32 addr_offset = 0;
	dma_addr_t dAddr;
	u32 prev_chunk_cnt;
	struct npu_session *session;
	struct npu_system *system;
	struct npu_memory_buffer *IMB_mem_buf;
	struct addr_info **imb_av;
	u32 req_chunk_cnt;

	session = container_of(work, struct npu_session, imb_async_work.work);
	IMB_cnt = session->IMB_cnt;
	IMB_mem_buf = session->IMB_mem_buf;
	imb_av = session->imb_av;
	system = get_session_system(session);
	session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_START;
	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	address_vector_offset = session->address_vector_offset;

	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	npu_uinfo("Start async alloc for IMB\n", session);

	WARN_ON(!IMB_mem_buf);

	mutex_lock(&system->imb_alloc_data.imb_alloc_lock);

	req_chunk_cnt = __calc_imb_req_chunk(session, IMB_mem_buf, false);

	prev_chunk_cnt = system->imb_alloc_data.alloc_chunk_cnt;
	for (i = system->imb_alloc_data.alloc_chunk_cnt; i < req_chunk_cnt; i++) {
		dAddr = system->imb_alloc_data.chunk_imb->paddr + (i * NPU_IMB_CHUNK_SIZE);
		system->imb_alloc_data.chunk[i].size = NPU_IMB_CHUNK_SIZE;
		ret = npu_memory_alloc_from_heap(system->pdev, &(system->imb_alloc_data.chunk[i]), dAddr,
					&system->memory, "IMB", 0);
		if (unlikely(ret)) {
			npu_uerr("IMB: npu_memory_alloc_from_heap failed, err: %d\n", session, ret);
			mutex_unlock(&system->imb_alloc_data.imb_alloc_lock);
			session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_ERROR;
			ret = -EFAULT;
			goto p_err;
		}
		system->imb_alloc_data.alloc_chunk_cnt++;
		npu_udbg("IMB: successfully allocated chunk = %u with size = 0x%X\n", session, i, NPU_IMB_CHUNK_SIZE);
	}

	/* in allocation case we're not interested in FW size acceptance */
	__fw_sync_CMD_IMB_SIZE(session, system->imb_alloc_data.alloc_chunk_cnt);

	if (prev_chunk_cnt != system->imb_alloc_data.alloc_chunk_cnt) {
		npu_udbg("IMB: system total size 0x%X -> 0x%X\n", session,
			 prev_chunk_cnt * NPU_IMB_CHUNK_SIZE,
			 system->imb_alloc_data.alloc_chunk_cnt * NPU_IMB_CHUNK_SIZE);
	}

	IMB_mem_buf->daddr = system->imb_alloc_data.chunk_imb->paddr;

	mutex_unlock(&system->imb_alloc_data.imb_alloc_lock);

	for (i = 0; i < IMB_cnt; i++) {
		(av + (*imb_av + i)->av_index)->m_addr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->daddr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->vaddr = ((void *)((char *)(IMB_mem_buf->vaddr)) + addr_offset);
		(session->IMB_info + i)->size = (*imb_av + i)->size;
		addr_offset += ALIGN((*imb_av + i)->size, (u32)NPU_IMB_ALIGN);
		npu_udbg("IMB: (IMB_mem_buf + %d)->vaddr(%pad), daddr(%pad), size(%zu)\n",
			 session, i, (session->IMB_info + i)->vaddr, &(session->IMB_info + i)->daddr,
			 (session->IMB_info + i)->size);
	}

	npu_session_ion_sync_for_device(IMB_mem_buf, DMA_TO_DEVICE);
	session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_DONE;
	npu_uinfo("End async alloc for IMB\n", session);
p_err:
	wake_up(&session->imb_wq);
	if (likely(imb_av)) {
		kfree(imb_av);
		session->imb_av = NULL;
	}
	return;
}

static int npu_imb_alloc_async(struct npu_session *session,
		struct addr_info **IMB_av, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;

	session->imb_av = (struct addr_info **)kzalloc(sizeof(struct addr_info*) * session->IMB_cnt, GFP_KERNEL);
	if (unlikely(!session->imb_av)) {
		npu_err("failed in a_IMB_av(ENOMEM)\n");
		return -ENOMEM;
	}

	memcpy(session->imb_av, IMB_av, sizeof(struct addr_info*) * session->IMB_cnt);
	session->IMB_mem_buf = IMB_mem_buf;
	queue_delayed_work(session->imb_async_wq, &session->imb_async_work, msecs_to_jiffies(0));

	return ret;
}

int npu_session_imb_async_init(struct npu_session *session)
{
	int ret = 0;
	struct npu_device *device;
	struct npu_sessionmgr *sess_mgr;

	sess_mgr = (struct npu_sessionmgr *)(session->cookie);
	device = container_of(sess_mgr, struct npu_device, sessionmgr);
	INIT_DELAYED_WORK(&session->imb_async_work, npu_imb_async_work);

	session->imb_async_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!session->imb_async_wq) {
		npu_err("fail to create workqueue -> system->imb_async_wq\n");
		ret = -EFAULT;
		goto err_probe;
	}

	init_waitqueue_head(&(session->imb_wq));
err_probe:
	return ret;
}
#endif
/* full - is used while release graph only, otherwise it's dequeue with guaranteed fw syncing */
static void __free_imb_mem_buf(struct npu_session *session, bool full)
{
	struct npu_session *session_ref;
	int ret;
	u32 idx, fw_chunk, alloc_chunk, max_next_req_chunk = 0, max_next_size = 0;
	struct npu_system *system = get_session_system(session);
	struct npu_sessionmgr *sessionmgr = session->cookie;

	WARN_ON(!sessionmgr);
	if (likely(session->IMB_mem_buf)) {
		mutex_lock(&system->imb_alloc_data.imb_alloc_lock);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
		if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev >= 2)) {
			max_next_req_chunk = __calc_imb_req_chunk_v2(session, true);
			goto skip_v1;
		}
#endif

		for (idx = 0; idx < NPU_MAX_SESSION; idx++) {
			if (sessionmgr->session[idx]) {
				session_ref = sessionmgr->session[idx];
				if ((session != session_ref) && (session_ref->IMB_mem_buf) &&
				    (max_next_size < session_ref->IMB_mem_buf->ncp_max_size)) {
					max_next_size = session_ref->IMB_mem_buf->ncp_max_size;
					max_next_req_chunk = __calc_imb_req_chunk_v1(session_ref->IMB_mem_buf, system->max_npu_core);
				}
			}
		}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
skip_v1:
#endif
		if (likely(!full)) {
			/* if deque, deallocate not all memory but up to the THRESHOLD limit */
			max_next_req_chunk = max_next_req_chunk ?
				max_next_req_chunk : (npu_get_configs(NPU_IMB_THRESHOLD_SIZE) / NPU_IMB_CHUNK_SIZE);
		}
		if (max_next_req_chunk < system->imb_alloc_data.alloc_chunk_cnt) {
			if (!full || max_next_req_chunk) {
				/* if deque or there are other non-closed sessions with buffers,
				 * we should sync it with FW
				 */
				ret = __fw_sync_CMD_IMB_SIZE(session, max_next_req_chunk);
				if (likely(!ret)) {
					/* deallocate depending on the memory that fw is using */
					fw_chunk = system->imb_size_ctl.fw_size / NPU_IMB_CHUNK_SIZE;
					alloc_chunk = (fw_chunk >= max_next_req_chunk)? fw_chunk : max_next_req_chunk;
					__free_imb_chunk(alloc_chunk, system, &system->imb_alloc_data);
				}
			} else {
				/* if we release graph and there no any sessions with buffers,
				 * deallocate it anyway without FW syncing
				 */
				npu_dbg("IMB: buffer full free w/o FW syncing\n");
				__free_imb_chunk(0, system, &system->imb_alloc_data);
			}
		}
		mutex_unlock(&system->imb_alloc_data.imb_alloc_lock);
	}
}
#endif
#endif

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
struct nq_buffer *get_buffer_of_execution_info(struct npu_frame *frame)
{
	struct npu_queue_list *input;
	struct nq_container *containers;
	struct nq_buffer *buffers;

	input = frame->input;
	if (!input) {
		npu_err("failed to get input of execution info for dsp\n");
		return NULL;
	}

	containers = &input->containers[input->count - 1];
	if (!containers) {
		npu_err("failed to get containers of execution info for dsp\n");
		return NULL;
	}

	buffers = containers->buffers;
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return NULL;
	}

	return buffers;
}

struct dsp_common_execute_info_v4 *get_execution_info_for_dsp(struct npu_frame *frame)
{
	struct nq_buffer *buffers = get_buffer_of_execution_info(frame);

	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return NULL;
	}

	return buffers->vaddr;
}

int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr, int index);
#endif

int npu_session_put_frame_req(
	struct npu_session *session, struct npu_queue *queue,	struct npu_queue_list *incl, struct npu_queue_list *otcl,
	frame_cmd_e frame_cmd, struct av_info *IFM_info, struct av_info *OFM_info)
{
	int ret = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_vertex_ctx *vctx;
	struct npu_frame frame = {
		.uid = session->uid,
		.frame_id = incl->id,
		.npu_req_id = 0,	/* Determined in manager */
		.result_code = 0,
		.session = session,
		.cmd = frame_cmd,
		.priority = session->sched_param.priority,   /* Read priority passed by app */
		.src_queue = queue,
		.input = incl,
		.output = otcl,
		.magic_tail = NPU_FRAME_MAGIC_TAIL,
		.mbox_process_dat = session->mbox_process_dat,
		.IFM_info = IFM_info,
		.OFM_info = OFM_info,
	};
	struct npu_frame *f;

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	if (session->hids & NPU_HWDEV_ID_DSP) {
		session->exe_info = get_execution_info_for_dsp(&frame);
		__update_iova_of_exe_message_of_dsp(session, session->exe_info, incl->index);
	}
#endif

	if (is_kpi_mode_enabled(false)) {
		f = &(kpi_frame[incl->index % KPI_FRAME_MAX_BUFFER]);
		f->uid = session->uid;
		f->frame_id = incl->id;
		f->npu_req_id = 0;       /* Determined in manager */
		f->result_code = 0;
		f->session = session;
		f->cmd = frame_cmd;
		/* setting high priority for KPI frames even though app may not have set */
		f->priority = KPI_FRAME_PRIORITY;
		f->src_queue = queue;
		f->input = incl;
		f->output = otcl;
		f->magic_tail = NPU_FRAME_MAGIC_TAIL;
		f->mbox_process_dat = session->mbox_process_dat;
		f->IFM_info = IFM_info;
		f->OFM_info = OFM_info;

		npu_dbg("Sending kpi frames to mailbox : uid %d, index %d/%d, frame id %d, frame %p\n",
				session->uid, incl->index, otcl->index, incl->id, f);
		session->current_frame = f;
		ret = kpi_frame_mbox_put(f);
		if (unlikely(!ret)) {
			npu_uerr("fail(%d) in kpi_frame_mobx_put\n",
					session, ret);
			ret = -EFAULT;
			goto p_err;
		}
	} else {
		npu_dbg("Sending frames to mailbox : uid %d, index %d/%d, frame id %d\n",
				session->uid, incl->index, otcl->index, incl->id);
		session->current_frame = &frame;

		mutex_lock(session->global_lock);
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		npu_update_frequency_from_queue(session, 1);
		ret = proto_drv_handler_frame_requested(&frame);

		if (unlikely(ret)) {
			npu_uerr("fail(%d) in npu_buffer_q_put\n",
					session, ret);
			mutex_unlock(session->global_lock);
			npu_update_frequency_from_queue(session, 1);
			npu_revert_cmdq_list(session);
			ret = -EFAULT;
			goto p_err;
		}
#else
		ret = proto_drv_handler_frame_requested(&frame);

		if (unlikely(ret)) {
			npu_uerr("fail(%d) in npu_buffer_q_put\n", session, ret);
			mutex_unlock(session->global_lock);
			ret = -EFAULT;
			goto p_err;
		}
#endif
		mutex_unlock(session->global_lock);
	}
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	npu_dvfs_set_initial_freq(device, session->uid);
#endif
	npu_udbg("success\n", session);

	return 0;
p_err:
	return ret;
}

int add_ion_mem(struct npu_session *session, struct npu_memory_buffer *mem_buf, u32 memory_type)
{
	switch (memory_type) {
	case MEMORY_TYPE_NCP:
		session->ncp_mem_buf = mem_buf;
		break;
	case MEMORY_TYPE_NCP_HDR:
		session->ncp_hdr_buf = mem_buf;
		break;
	case MEMORY_TYPE_IN_FMAP:
		session->IOFM_mem_buf = mem_buf;
		break;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	case MEMORY_TYPE_IM_FMAP:
		session->IMB_mem_buf = mem_buf;
		break;
#endif
	case MEMORY_TYPE_WEIGHT:
		session->weight_mem_buf = mem_buf;
		break;
	default:
		break;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
static inline void __release_imb_mem_buf(struct npu_session *session)
{
	struct npu_memory_buffer *ion_mem_buf = session->IMB_mem_buf;

	if (likely(ion_mem_buf)) {
#if !IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		if (ion_mem_buf->daddr)
			npu_memory_free(session->memory, ion_mem_buf);
#else /* CONFIG_NPU_USE_IMB_ALLOCATOR */
#ifdef CONFIG_NPU_IMB_ASYNC_ALLOC
		session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_SKIP;
#endif
		/* deallocate all memory chunks */
		__free_imb_mem_buf(session, true);
#endif
		mutex_lock(session->global_lock);
		session->IMB_mem_buf = NULL;
		mutex_unlock(session->global_lock);

		kfree(ion_mem_buf);
	}
}
#endif

void __release_graph_ion(struct npu_session *session)
{
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;

	memory = session->memory;
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	session->ncp_mem_buf_offset = 0;

	if (unlikely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	__release_imb_mem_buf(session);
#endif
}

int npu_session_NW_CMD_UNLOAD(struct npu_session *session)
{
	/* check npu_device emergency error */
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_UNLOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (unlikely(npu_device_is_emergency_err(device)))
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR\n", session);

	/* Post unload command */
	npu_udbg("sending UNLOAD command.\n", session);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	if (dsp_dhcp_get_master(session, NPU_HWDEV_ID_NPU))
		npu_profile_iter_reupdate(0, 0);
	else if (dsp_dhcp_get_master(session, NPU_HWDEV_ID_DSP))
		npu_profile_iter_reupdate(0, 0);

	if (session->ncp_payload) {
		npu_memory_free(session->memory, session->ncp_payload);
		kfree(session->ncp_payload);
		session->ncp_payload = NULL;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if (device->sched->mode != NPU_PERF_MODE_NPU_BOOST &&
			device->sched->mode != NPU_PERF_MODE_NPU_BOOST_PRUNE) {
		npu_precision_active_hash_delete(session);
	}
#endif
	return 0;
}

int npu_session_close(struct npu_session *session)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_sessionmgr *sessionmgr;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct vs4l_param param;
#endif

	BUG_ON(!session);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	param.target = NPU_S_PARAM_QOS_MIF;
	param.offset = NPU_QOS_DEFAULT_VALUE;
	npu_qos_param_handler(session, &param);
#endif

	npu_scheduler_unregister_session(session);

	session->ss_state |= BIT(NPU_SESSION_STATE_CLOSE);
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
	sessionmgr = session->cookie;

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->imb_async_wq) {
		destroy_workqueue(session->imb_async_wq);
		session->imb_async_wq = NULL;
	}
	session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_SKIP;
#endif

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	if (session->check_core) {
		if (session->is_control_dsp)
			sessionmgr->dsp_flag--;

		if (session->is_control_npu)
			sessionmgr->npu_flag--;

		session->check_core = false;
	}
#endif

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	mutex_lock(session->global_lock);
	if (session->hids & NPU_HWDEV_ID_DSP) {
		ret = __remove_buf_from_load_msg(session);
		if (ret)
			goto p_err;

		ret = __remove_buf_from_exe_msg(session);
		if (ret)
			goto p_err;

		dsp_graph_remove_kernel(device, session);
	}
	mutex_unlock(session->global_lock);
#endif

	ret = npu_session_undo_close(session);
	if (ret)
		goto p_err;

	npu_sessionmgr_unset_unifiedID(session);

	ret = npu_session_undo_open(session);
	if (ret)
		goto p_err;
	return ret;
p_err:
	npu_err("fail(%d)\n", ret);
	return ret;
}

static void npu_session_process_register(struct npu_session *session)
{
	struct task_struct *parent;

	session->comm[0] = '\0';
	session->p_comm[0] = '\0';

	session->pid = task_pid_nr(current);

	strncpy(session->comm, current->comm, TASK_COMM_LEN);

	if (current->parent != NULL) {
		parent = current->parent;
		session->p_pid = task_pid_nr(parent);
		strncpy(session->p_comm, parent->comm, TASK_COMM_LEN);
	}

	npu_info("pid(%d), current(%s), parent pid(%d), parent(%s)\n",
		session->pid, session->comm, session->p_pid, session->p_comm);
}

int npu_session_open(struct npu_session **session, void *cookie, void *memory)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC) || IS_ENABLED(CONFIG_DSP_USE_VS4L) || IS_ENABLED(CONFIG_NPU_GOVERNOR)
	int i = 0;
#endif
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct npu_sessionmgr *sessionmgr;
#endif

	*session = kzalloc(sizeof(struct npu_session), GFP_KERNEL);
	if (unlikely(*session == NULL)) {
		npu_err("fail in kzalloc\n");
		ret = -ENOMEM;
		return ret;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_OPEN);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	INIT_LIST_HEAD(&(*session)->active_session);
#endif
	ret = npu_sessionmgr_regID(cookie, *session);
	if (unlikely(ret)) {
		npu_err("fail(%d) in npu_sessionmgr_regID\n", ret);
		npu_session_register_undo_cb(*session, npu_session_undo_open);
		goto p_err;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_REGISTER);

	init_waitqueue_head(&((*session)->wq));

	(*session)->cookie = cookie;
	(*session)->memory = memory;

	(*session)->IMB_info = NULL;

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	for (i = 0; i < NPU_FRAME_MAX_CONTAINERLIST; i++) {
		(*session)->out_fence_fd[i] = -1;
		(*session)->in_fence_fd[i] = -1;
	}
#endif

	(*session)->IFM_info.addr_info = NULL;
	(*session)->IFM_info.state = SS_BUF_STATE_UNPREPARE;
	(*session)->OFM_info.addr_info = NULL;
	(*session)->OFM_info.state = SS_BUF_STATE_UNPREPARE;
	(*session)->ncp_mem_buf = NULL;
	(*session)->weight_mem_buf = NULL;
	(*session)->shared_mem_buf = NULL;
	(*session)->ncp_mem_buf_offset = 0;
	(*session)->IOFM_mem_buf = NULL;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	(*session)->IMB_mem_buf = NULL;
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	(*session)->imb_av = NULL;
#endif
#endif

	(*session)->qbuf_IOFM_idx = -1;
	(*session)->dqbuf_IOFM_idx = -1;
	(*session)->hids = NPU_HWDEV_ID_MISC;
	(*session)->unified_id = 0;
	(*session)->unified_op_id = 0;

	/* Set default scheduling parameters */
	(*session)->sched_param.bound_id = NPU_BOUND_UNBOUND;
	(*session)->sched_param.priority = NPU_PRIORITY_MID_PRIO; /* Mid priority */
	(*session)->sched_param.deadline = NPU_MAX_DEADLINE;
	(*session)->preference = CMD_LOAD_FLAG_IMB_PREFERENCE2;

	npu_session_process_register(*session);

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	INIT_LIST_HEAD(&(*session)->kernel_list);
	INIT_LIST_HEAD(&(*session)->buf_list);
	INIT_LIST_HEAD(&(*session)->update_list);
	(*session)->buf_count = 0;
	(*session)->update_count = 0;
	(*session)->user_kernel_count = 0;
	(*session)->dl_unique_id = 0;
	(*session)->str_manager.count = 0;
	(*session)->str_manager.strings = NULL;
	for (i = 0; i < NPU_FRAME_MAX_BUFFER; i++)
		(*session)->buf_info[i].fd = 0;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	(*session)->dvfs_info = NULL;
#endif
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	(*session)->in_fence = NULL;
	(*session)->out_fence = npu_sync_timeline_create("enn-out-fence");
	if (!(*session)->out_fence) {
		npu_err("fail(%d) in npu_sync_timeline_create\n", ret);
		ret = -ENOMEM;
		goto p_err;
	}
	(*session)->out_fence_value = 0;
	(*session)->out_fence_exe_value = 0;
#endif
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	(*session)->arrival_interval = NPU_SCHEDULER_DEFAULT_REQUESTED_TPF;
	(*session)->last_qbuf_arrival = npu_get_time_us();
	sessionmgr = cookie;
	(*session)->model_hash_node = 0;
	(*session)->tune_npu_level = 0;
	(*session)->earliest_qbuf_time = 0;
	(*session)->prev_npu_level = 0;
	(*session)->next_mif_level = 0;
	(*session)->tpf = NPU_SCHEDULER_DEFAULT_TPF;
	(*session)->tpf_requested = false;
	(*session)->is_control_dsp= false;
	(*session)->is_control_npu= true;

	for(i = 0; i < 5; i++){
		(*session)->predict_error[i] = 0;
	}
	(*session)->predict_error_idx = 0x0;
#endif
	(*session)->is_instance_1 = false;

	ret = npu_scheduler_register_session(*session);
	if (unlikely(ret)) {
		npu_err("fail(%d) in register_session\n", ret);
		npu_session_register_undo_cb(*session, npu_session_undo_open);
		goto p_err;
	}

	npu_session_imb_async_init(*session);

	return ret;
p_err:
	npu_session_execute_undo_cb(*session);
	return ret;
}

int _undo_s_graph_each_state(struct npu_session *session)
{
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;
	struct addr_info *addr_info;

	BUG_ON(!session);

	memory = session->memory;

	if (session->ss_state <= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP))
		goto graph_ion_unmap;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_WGT_KALLOC))
		goto wgt_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_KALLOC))
		goto iofm_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC))
		goto iofm_ion_unmap;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))
		goto imb_ion_unmap;

imb_ion_unmap:
	addr_info = session->IMB_info;
	session->IMB_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

	__release_imb_mem_buf(session);
#endif
iofm_ion_unmap:
	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

iofm_kfree:
	addr_info = session->IFM_info.addr_info;
	session->IFM_info.addr_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

	addr_info = session->OFM_info.addr_info;
	session->OFM_info.addr_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

wgt_kfree:
	addr_info = session->WGT_info;
	session->WGT_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

graph_ion_unmap:
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->weight_mem_buf;
	session->weight_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->shared_mem_buf;
	session->shared_mem_buf = NULL;
	if (ion_mem_buf) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	return 0;
}

int _undo_s_format_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_FORMAT_OT))
		goto init_format;

init_format:

p_err:
	return ret;
}

int _undo_streamoff_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_START))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_STOP))
		goto release_streamoff;

release_streamoff:

p_err:
	return ret;
}

int _undo_close_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_STOP))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_CLOSE))
		goto session_close;

session_close:

p_err:
	return ret;
}

bool EVER_FIND_FM(u32 *FM_cnt, struct addr_info *FM_av, u32 address_vector_index)
{
	bool ret = false;
	u32 i;

	for (i = 0; i < (*FM_cnt); i++) {
		if ((FM_av + i)->av_index == address_vector_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

void __set_unique_id(struct npu_session *session, struct drv_usr_share *usr_data)
{
	u32 uid = session->uid;
	usr_data->id = uid;
}

int __set_unified_id(struct npu_session *session, struct drv_usr_share *usr_data)
{
	int ret = 0;

	session->unified_op_id = usr_data->unified_op_id;
	ret = npu_sessionmgr_set_unifiedID(session);
	return ret;
}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
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

bool is_matching_ncp_for_hash(struct npu_model_info_hash *h, struct ncp_header *ncp) {
	return (!strcmp(h->model_name, ncp->model_name)) &&
		(h->computational_workload == ncp->computational_workload) &&
		(h->io_workload == ncp->io_workload);
}
#endif

#define DBG_STR_ENTRY(idx) \
	[idx] = #idx

#define PARAM_INDENT    "    "
#define MEM_PARAM_WIDTH(param_mem) (param_mem->param[0])
#define MEM_PARAM_HEIGHT(param_mem) (param_mem->param[1])
#define MEM_PARAM_CHANNEL(param_mem) (param_mem->param[2])

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
const char *get_dsp_mem_type_str(enum dsp_common_param_type param_type)
{
	static const char * const p_type_str[] = {
		DBG_STR_ENTRY(DSP_COMMON_MEM_GRAPH_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_CMDQ_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_BIN_STR),
		DBG_STR_ENTRY(DSP_COMMON_MEM_IFM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_OFM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_IMB),
		DBG_STR_ENTRY(DSP_COMMON_MEM_WGT),
		DBG_STR_ENTRY(DSP_COMMON_MEM_BIAS),
		DBG_STR_ENTRY(DSP_COMMON_MEM_SCALAR),
		DBG_STR_ENTRY(DSP_COMMON_MEM_CUSTOM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_NPU_IM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_DESC),
	};

	static const char *MEM_EMPTY = "DSP_COMMON_MEM_EMPTY";
	static const char *UNKNOWN = "UNKNOWN";

	if (param_type == DSP_COMMON_MEM_EMPTY)
		return MEM_EMPTY;

	if (param_type >= ARRAY_SIZE(p_type_str))
		return UNKNOWN;

	return p_type_str[param_type];
}

#ifndef CONFIG_NPU_KUNIT_TEST
static void dsp_param_hdr(void)
{
	npu_dump(PARAM_INDENT
		 "%18s %6s | %9s %8s %8s %5s %8s %4s %10s %7s %4s %4s %4s\n",
		 "pr_type", "pr_idx", "addr_type", "mem_attr", "mem_type",
		 "manda", "size", "fd", "iova", "mem_off", "w", "h", "c");
}

static void dsp_param_info(struct dsp_common_param_v4 *param)
{
	struct dsp_common_mem_v4 *mem = &param->param_mem;

	npu_dump(
		PARAM_INDENT
		"%15s(%d) %6d | %9d %8d %8d %5s %#8x %4d %#10x %#7x %4d %4d %4d\n",
		get_dsp_mem_type_str(param->param_type), param->param_type,
		param->param_index, mem->addr_type, mem->mem_attr,
		mem->mem_type, mem->is_mandatory ? "T" : "F", mem->size,
		mem->fd, mem->iova, mem->offset, MEM_PARAM_WIDTH(mem),
		MEM_PARAM_HEIGHT(mem), MEM_PARAM_CHANNEL(mem));
}

void dsp_load_graph_info(struct dsp_common_graph_info_v4 *info)
{
	int i;

	if (!info)
		return;

	npu_dump(
		"========================= ORCA Load Graph info ==========================\n");
	npu_dump("graph_id(%#08x) n_tsgd(%d) n_param(%d) n_kernel(%d)\n",
		 info->global_id, info->n_tsgd, info->n_param, info->n_kernel);

	dsp_param_hdr();

	for (i = 0; i < (int)(info->n_param + 1); i++) { // +1 for TSGD.
		dsp_param_info(&info->param_list[i]);
	}

	npu_dump(
		"--------------------------------------------------------------------------\n");

}

void dsp_exec_graph_info(struct dsp_common_execute_info_v4 *info)
{
	int32_t i;

	if (!info)
		return;

	npu_dump(
		"========================= ORCA Exec Graph info ==========================\n");
	npu_dump("graph_id(%#08x) n_update_param(%d)\n", info->global_id,
		 info->n_update_param);

	dsp_param_hdr();

	for (i = 0; i < (int32_t)info->n_update_param; i++)
		dsp_param_info(&info->param_list[i]);

	npu_dump(
		"--------------------------------------------------------------------------\n");

}
#endif
#endif

int update_ncp_info(struct npu_session *session, struct npu_memory_buffer *model_mem_buf, u32 memory_type)
{
	int ret = 0;

	if (!session || !model_mem_buf) {
		npu_err("invalid session or model_mem_buf\n");
		return -EINVAL;
	}

	switch (memory_type) {
		case MEMORY_TYPE_NCP:
			{
				if (session->ncp_mem_buf_offset > model_mem_buf->dma_buf->size) {
					npu_err("ncp_offset(%lu) is larger than buffer size(%zu).\n",
						session->ncp_mem_buf_offset, model_mem_buf->dma_buf->size);
					ret = -EINVAL;
					goto p_err;
				}

				session->ncp_info.ncp_addr.size = model_mem_buf->size;
				session->ncp_info.ncp_addr.vaddr = model_mem_buf->vaddr + session->ncp_mem_buf_offset;
				session->ncp_info.ncp_addr.daddr = model_mem_buf->daddr + session->ncp_mem_buf_offset;

				npu_dbg("ncp offset (%lu) + ncp_ion_map(0x%pad) vaddr(0x%pK)\n",
				session->ncp_mem_buf_offset,
				&model_mem_buf->daddr, model_mem_buf->vaddr);
				break;
			}
		case MEMORY_TYPE_WEIGHT:
			{
				break;
			}
		default:
			npu_err("can't support memory type\n");
			ret = -EINVAL;
			break;
	}

p_err:
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_session_dump(struct npu_session *session)
{
	size_t IFM_cnt;
	size_t OFM_cnt;
	size_t IMB_cnt;
	size_t WGT_cnt;
	size_t idx1 = 0;

	struct av_info *IFM_info;
	struct av_info *OFM_info;
	struct addr_info *IFM_addr;
	struct addr_info *OFM_addr;
	struct addr_info *IMB_addr;
	struct addr_info *WGT_addr;

	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;

	if (!session)
		return;

	IFM_cnt = session->IFM_cnt;
	OFM_cnt = session->OFM_cnt;
	IMB_cnt = session->IMB_cnt;
	WGT_cnt = session->WGT_cnt;

	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_dump("npu session id = %d\n", session->uid);
	npu_dump("npu session state = %lu\n", session->ss_state);
	npu_dump(
		"npu session unified id = %d, unified op id = %lld, hw id = %d\n",
		session->unified_id, session->unified_op_id, session->hids);
	npu_dump("- index type vaddr                daddr              size\n");
	npu_dump("------------ session IFM/OFM memory ------------\n");

	IFM_info = &session->IFM_info;
	OFM_info = &session->OFM_info;
	IFM_addr = IFM_info->addr_info;
	OFM_addr = OFM_info->addr_info;

	for (idx1 = 0; idx1 < IFM_cnt; idx1++) {
		npu_dump("- %zu      %d   %pK   %pad   %zu\n", idx1,
			 MEMORY_TYPE_IN_FMAP,
			 (IFM_addr + idx1)->vaddr,
			 &((IFM_addr + idx1)->daddr),
			 (IFM_addr + idx1)->size);
	}
	for (idx1 = 0; idx1 < OFM_cnt; idx1++) {
		npu_dump("- %zu      %d   %pK   %pad   %zu\n", idx1,
			 MEMORY_TYPE_OT_FMAP,
			 (OFM_addr + idx1)->vaddr,
			 &((OFM_addr + idx1)->daddr),
			 (OFM_addr + idx1)->size);
	}
	npu_dump("------------------------------------------------\n");

	if (session->hids & NPU_HWDEV_ID_NPU) {
		npu_dump("------------ session IMB/WGT memory ------------\n");
		IMB_addr = session->IMB_info;
		WGT_addr = session->WGT_info;
		for (idx1 = 0; idx1 < IMB_cnt; idx1++) {
			npu_dump("- %zu      %d   %pK   %pad   %zu\n", idx1,
				 MEMORY_TYPE_IM_FMAP, (IMB_addr + idx1)->vaddr,
				 &((IMB_addr + idx1)->daddr),
				 (IMB_addr + idx1)->size);
		}
		for (idx1 = 0; idx1 < WGT_cnt; idx1++) {
			npu_dump("- %zu      %d   %pK   %pad   %zu\n", idx1,
				 MEMORY_TYPE_WEIGHT, (WGT_addr + idx1)->vaddr,
				 &((WGT_addr + idx1)->daddr),
				 (WGT_addr + idx1)->size);
		}
		npu_dump("------------------------------------------------\n");
	}

	npu_dump("--------- session NCP/IOFM/IMB memory ----------\n");
	npu_memory_buffer_dump(session->ncp_mem_buf);
	npu_memory_buffer_dump(session->ncp_hdr_buf);
	npu_memory_buffer_dump(session->IOFM_mem_buf);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	npu_memory_buffer_dump(session->IMB_mem_buf);
#endif
	npu_dump("------------------------------------------------\n");
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	npu_dump("-------------- session DSP kernel/graph --------------\n");
	if (session->hids & NPU_HWDEV_ID_DSP) {
		dsp_kernel_dump(&device->kmgr);
		dsp_exec_graph_info(session->exe_info);
	}
	npu_dump("------------------------------------------------\n");
#endif
}
#endif

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#ifndef CONFIG_NPU_KUNIT_TEST
void display_info(unsigned int *ginfo, int i, int size)
{
	int value = 0, j = 0;

	while (i < size) {
		j = i;
		npu_info("DSP common param v4 %p (%ld)\n", ginfo + i, sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		value = *(ginfo + j++);
		npu_info("param type : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param index (kernel id) : 0x%x %d\n", value, value);

		value = *(ginfo + j++);
		npu_info("addr type : 0x%x %d\n", (value >> 24) & 0xff, (value >> 24) & 0xff);
		npu_info("mem attr : 0x%x %d\n", (value >> 16) & 0xff, (value >> 16) & 0xff);
		npu_info("mem type : 0x%x %d\n", (value >> 8) & 0xff, (value >> 8) & 0xff);
		npu_info("mandatory : 0x%x %d\n", value & 0xff, value & 0xff);

		value = *(ginfo + j++);
		npu_info("size : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("offset : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("reserved : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 0 : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 1 : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 2 : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 3 : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 4 : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("param 5 : 0x%x %d\n", value, value);

		value = *(ginfo + j++);
		npu_info("fd : 0x%x %d\n", value, value);
		value = *(ginfo + j++);
		npu_info("iova : 0x%x %d\n", value, value);

		i += (sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		npu_info("i %d, j %d\n", i, j);
	}
}

void display_graph_info(struct dsp_common_graph_info_v4 *ginfo_src, int size_src)
{
	int i, n, size;
	unsigned int *ginfo;

	i = 0;
	ginfo = (unsigned int *)ginfo_src;
	size = size_src / sizeof(unsigned int);

	npu_info("### graph info : %p (%d)\n", ginfo, size);

	n = *(ginfo + i++);
	npu_info("global id : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("tsgd # : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("param # : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("kernel # : 0x%x %d\n", n, n);

	display_info(ginfo, i, size);
}

void display_execute_info(struct dsp_common_execute_info_v4 *ginfo_src, int size_src)
{
	int i, n, size;
	unsigned int *ginfo;

	i = 0;
	ginfo = (unsigned int *)ginfo_src;
	size = size_src / sizeof(unsigned int);

	npu_info("### execute info : %p (%d)\n", ginfo, size);

	n = *(ginfo + i++);
	npu_info("global id : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("update # : 0x%x %d\n", n, n);

	display_info(ginfo, i, size);
}
#endif

static inline u32 __get_dl_unique_id(void)
{
	u32 id;

	id = ++dl_unique_id;
	if (dl_unique_id == U32_MAX) {
		dl_unique_id = 0;
	}

	return id;
}

struct npu_memory_buffer * npu_session_check_fd(struct npu_session *session, int fd)
{
	struct npu_memory_buffer *buf, *temp;

	list_for_each_entry_safe(buf, temp, &session->buf_list, dsp_list) {
		if (buf->m.fd == fd)
			return buf;
	}

	return NULL;
}

int __update_iova_of_load_message_of_dsp(struct npu_session *session)
{
	int i = 0;
	int ret = 0;
	int num_param = 0;
	size_t kernel_elf_size = 0;
	struct dsp_dl_lib_file gkt;
	struct dsp_common_param_v4 *param = NULL;
	struct npu_memory_buffer *mem_buf = NULL;
	struct dsp_common_graph_info_v4 *load_msg =
		(struct dsp_common_graph_info_v4 *)session->ncp_mem_buf->vaddr;

	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	const char dsp_kernel[]="dsp_kernel";
	struct npu_model_info_hash *h;
	struct npu_sessionmgr *sessionmgr;
	u32 key = 0;
#endif

	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	sessionmgr = session->cookie;
#endif

	if (!load_msg) {
		npu_err("Pointer of load message is NULL.\n");
		ret = -ENOMEM;
		goto p_err;
	}

	load_msg += session->ncp_mem_buf_offset;
	num_param = load_msg->n_tsgd + load_msg->n_param;
	npu_info("param num : %d\n", num_param);

	for (i = 0; i < num_param; i++) {
		param = &load_msg->param_list[i];
		if (param->param_type == DSP_COMMON_MEM_EMPTY ||
			param->param_mem.fd < 0) {
			npu_dbg("%dth param is empty.\n", i);
			continue;
		}

		if (param->param_mem.mem_type == DSP_COMMON_NONE) {
			npu_dbg("%dth param memory type is none.\n", i);
			continue;
		}

		mem_buf = npu_session_check_fd(session, param->param_mem.fd);
		if (!mem_buf) {
			mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
			if (mem_buf == NULL) {
				npu_err("fail in npu_ion_map kzalloc\n");
				ret = -ENOMEM;
				goto p_err;
			}

			mem_buf->m.fd = param->param_mem.fd;
			mem_buf->size = param->param_mem.size;
			strncpy(mem_buf->name, get_dsp_mem_type_str(param->param_type), sizeof(mem_buf->name) - 1);
			ret = npu_memory_map(session->memory, mem_buf, npu_get_configs(NPU_PBHA_HINT_11),
										session->lazy_unmap_disable);
			if (unlikely(ret)) {
				npu_err("npu_memory_map is fail(%d).\n", ret);
				if (mem_buf)
					kfree(mem_buf);
				goto p_err;
			}

			session->buf_count++;
			list_add_tail(&mem_buf->dsp_list, &session->buf_list);
		}

		param->param_mem.iova = mem_buf->daddr + param->param_mem.offset;
		npu_dbg("param daddr : (fd %d) 0x%x/%ld\n",
				mem_buf->m.fd, param->param_mem.iova, param->param_mem.size);

		if (param->param_type == DSP_COMMON_MEM_KERNEL_DESC) {
			npu_info("description\n");
			gkt.mem = mem_buf->vaddr + param->param_mem.offset;
			gkt.size = param->param_mem.size;
			gkt.r_ptr = 0;
			mutex_lock(session->global_lock);
			session->dl_unique_id = __get_dl_unique_id();
			ret = dsp_dl_add_gkt(&gkt, &session->str_manager, session->dl_unique_id);
			mutex_unlock(session->global_lock);

			if (ret) {
				npu_err("failed to parse gkt.xml(%d).\n", ret);
				goto p_err;
			}
		}

		if (param->param_type == DSP_COMMON_MEM_KERNEL_BIN) {
			npu_info("kernel_bin\n");
			if (session->user_kernel_count >= MAX_USER_KERNEL) {
				npu_err("invalid number of user kernels(%d).\n", session->user_kernel_count);
				ret = -EINVAL;
				goto p_err;
			}
			session->user_kernel_elf[session->user_kernel_count] = mem_buf->vaddr + param->param_mem.offset;
			session->user_kernel_elf_size[session->user_kernel_count] = param->param_mem.size;
			session->user_kernel_count++;
			kernel_elf_size = param->param_mem.size;
		}
	}

	if (session->user_kernel_count) {
		if (session->kernel_count > kernel_elf_size || session->kernel_count != session->user_kernel_count) {
			npu_err("Failed to ensure allowed size\n");
			goto p_err;
		}
		ret = dsp_graph_add_kernel(device, session,
				session->kernel_name, session->dl_unique_id);
		if (ret) {
			npu_err("Failed(%d) to add user kernel\n", ret);
			goto p_err;
		}
	}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	key = npu_get_hash_name_key(dsp_kernel, 100000, 100000);
	mutex_lock(&sessionmgr->model_lock);
	hash_for_each_possible(sessionmgr->model_info_hash_head, h, hlist, key) {
		if(!strcmp(h->model_name, dsp_kernel)) {
			session->model_hash_node = h;
			break;
		}
	}
	mutex_unlock(&sessionmgr->model_lock);
	if (session->model_hash_node == 0) {
		session->model_hash_node = kcalloc(1, sizeof(struct npu_model_info_hash), GFP_KERNEL);
		session->model_hash_node->alpha = MAX_ALPHA;
		session->model_hash_node->beta = MAX_BETA;
		session->check_core = false;
		strcpy(session->model_hash_node->model_name, dsp_kernel);
		session->model_hash_node->computational_workload = 100000;
		session->model_hash_node->io_workload = 100000;
		init_workload_params(session);

		mutex_lock(&sessionmgr->model_lock);
		hash_add(sessionmgr->model_info_hash_head, &session->model_hash_node->hlist, key);
		mutex_unlock(&sessionmgr->model_lock);
	}
	npu_info("\n session->compute_workload = 0x%x session->io_workload = 0x%x\n",
			session->model_hash_node->computational_workload, session->model_hash_node->io_workload);
#endif

	session->global_id = load_msg->global_id;

	strcpy(session->model_name, dsp_kernel);
	return 0;
p_err:
	return ret;
}

int __update_dsp_input_daddr(struct npu_session *session, struct dsp_common_param_v4 *param, int index)
{
	struct npu_queue_list *q;
	struct nq_container *container;
	int j, k, buf_fd, param_fd;

	if (!session)
		return -EINVAL;

	q = &session->vctx.queue.inqueue[index];
	param_fd = param->param_mem.fd;

	for (j = 0; j < q->count; ++j) {
		container = &q->containers[j];
		for (k = 0; k < container->count; ++k) {
			buf_fd = container->buffers[k].m.fd;
			if (param_fd == buf_fd) {
				param->param_mem.iova = container->buffers[k].daddr + param->param_mem.offset;
				npu_dbg("input daddr : (fd %d) 0x%x\n", param_fd, param->param_mem.iova);
				return 0;
			}
		}
	}
	npu_err("failed to get input fd.\n");
	return -EBADF;
}

int __update_dsp_output_daddr(struct npu_session *session, struct dsp_common_param_v4 *param, int index)
{
	struct npu_queue_list *q;
	struct nq_container *container;
	int j, k, buf_fd, param_fd;

	if (!session)
		return -EINVAL;

	q = &session->vctx.queue.otqueue[index];
	param_fd = param->param_mem.fd;

	for (j = 0; j < q->count; ++j) {
		container = &q->containers[j];
		for (k = 0; k < container->count; ++k) {
			buf_fd = container->buffers[k].m.fd;
			if (param_fd == buf_fd) {
				param->param_mem.iova = container->buffers[k].daddr + param->param_mem.offset;
				npu_dbg("output daddr : (fd %d) 0x%x\n", param_fd, param->param_mem.iova);
				return 0;
			}
		}
	}
	npu_err("failed to get output fd.\n");
	return -EBADF;
}

int __update_dsp_buffer_daddr(struct npu_session *session, struct dsp_common_param_v4 *param)
{
	int i = 0;
	int ret = 0;
	struct npu_memory_buffer *mem_buf = NULL;

	for (i = 0; i < NPU_FRAME_MAX_BUFFER; i++) {
		if (session->buf_info[i].fd == param->param_mem.fd)
			return ret;
	}

	npu_dbg("buffer type is = %d\n", param->param_type);

	mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (mem_buf == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	mem_buf->m.fd = param->param_mem.fd;
	mem_buf->size = param->param_mem.size;
	strncpy(mem_buf->name, get_dsp_mem_type_str(param->param_type), sizeof(mem_buf->name) - 1);
	ret = npu_memory_map(session->memory, mem_buf, npu_get_configs(NPU_PBHA_HINT_11),
								session->lazy_unmap_disable);
	if (unlikely(ret)) {
		npu_err("npu_memory_map is fail(%d).\n", ret);
		kfree(mem_buf);
		return ret;
	}
	param->param_mem.iova = mem_buf->daddr + param->param_mem.offset;
	npu_dbg("param daddr : (fd %d) 0x%x/%ld\n",
			mem_buf->m.fd, param->param_mem.iova, mem_buf->size);
	list_add_tail(&mem_buf->dsp_list, &session->update_list);
	session->buf_info[session->update_count].fd = param->param_mem.fd;
	session->update_count++;
	return ret;
}

int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr, int index)
{
	int i = 0;
	int ret = 0;
	int num_param = 0;
	struct dsp_common_param_v4 *param = NULL;
	struct dsp_common_execute_info_v4 *exe_msg =
		(struct dsp_common_execute_info_v4 *)msg_kaddr;
	if (!exe_msg) {
		npu_err("Pointer of execution message is NULL.\n");
		ret = -ENOMEM;
		goto p_err;
	}

	num_param = exe_msg->n_update_param;
	npu_dbg("param num : %d\n", num_param);

	for (i = 0; i < num_param; i++) {
		param = &exe_msg->param_list[i];
		if (param->param_type == DSP_COMMON_MEM_EMPTY ||
			param->param_mem.fd < 0) {
			npu_dbg("%dth param is empty.\n", i);
			continue;
		}

		if (param->param_mem.mem_type == DSP_COMMON_NONE) {
			npu_dbg("%dth param memory type is none.\n", i);
			continue;
		}

		if (param->param_type == DSP_COMMON_MEM_IFM) {
			npu_dbg("%dth param is ifm.\n", i);
			ret = __update_dsp_input_daddr(session, param, index);
			if (ret < 0) {
				npu_err("fail(%d) to find input device virtual address\n", ret);
				ret = -ENOMEM;
				goto p_err;
			}
			continue;
		}

		if (param->param_type == DSP_COMMON_MEM_OFM) {
			npu_dbg("%dth param is ofm.\n", i);
			ret = __update_dsp_output_daddr(session, param, index);
			if (ret < 0) {
				npu_err("fail(%d) to find output device virtual address\n", ret);
				ret = -ENOMEM;
				goto p_err;
			}
			continue;
		}

		ret = __update_dsp_buffer_daddr(session, param);
		if (ret < 0) {
			npu_err("can not update buffer daddr(%d)\n", ret);
			ret = -ENOMEM;
			goto p_err;
		}
	}

	//esd_print_exec_graph_info(exe_msg);
	return 0;
p_err:
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_SOC_S5E9945)
static void __cfifo_sync_for_device(struct npu_session *sess)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct sg_page_iter sg_iter;
	struct page *page;

	/* check over EVT1.0 */
	if (exynos_soc_info.main_rev) {
		npu_info("Skip cfifo sync on EVT1\n");
		return;
	}

	vctx = &sess->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	for_each_sg_page(sess->ncp_hdr_buf->sgt->sgl, &sg_iter,
				sess->ncp_hdr_buf->sgt->orig_nents, 0) {
		page = sg_page_iter_page(&sg_iter);
		dma_sync_single_for_device(&device->ncp_sync_dev,
			page_to_phys(page), page_size(page), DMA_TO_DEVICE);
	}

	for_each_sg_page(sess->ncp_mem_buf->sgt->sgl, &sg_iter,
				sess->ncp_mem_buf->sgt->orig_nents, 0) {
		page = sg_page_iter_page(&sg_iter);
		dma_sync_single_for_device(&device->ncp_sync_dev,
			page_to_phys(page), page_size(page), DMA_TO_DEVICE);
	}
}
#endif

int __shared_ion_map(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	struct npu_memory_buffer *shared_mem_buf = NULL;
	struct npu_system *system = get_session_system(session);
	struct dsp_dhcp *dhcp = system->dhcp;

	shared_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (shared_mem_buf == NULL) {
		npu_err("fail in npu_ion_map kzalloc\n");
		ret = -ENOMEM;
		goto p_err;
	}

	shared_mem_buf->m.fd = info->priority;
	shared_mem_buf->size = info->size;
	ret = npu_memory_map(session->memory, shared_mem_buf, npu_get_configs(NPU_PBHA_HINT_11),
								session->lazy_unmap_disable);
	if (ret < 0) {
		npu_err("fail in npu_memory_map\n");
		kfree(shared_mem_buf);
		goto p_err;
	}

	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_SHARED_MEM_IOVA, shared_mem_buf->daddr);
	session->shared_mem_buf = shared_mem_buf;

p_err:
	return ret;
}

static struct npu_memory_buffer *
copy_ncp_header(struct npu_session *session, struct npu_memory_buffer *ncp_mem_buf)
{
	struct npu_memory_buffer *buffer;
	struct ncp_header *src_hdr = ncp_mem_buf->vaddr + session->ncp_mem_buf_offset;

	buffer = npu_memory_copy(session->memory, ncp_mem_buf,
				session->ncp_mem_buf_offset, src_hdr->hdr_size);
	if (IS_ERR_OR_NULL(buffer))
		return buffer;

	add_ion_mem(session, buffer, MEMORY_TYPE_NCP_HDR);

	return buffer;
}

static int model_ion_map(struct npu_session *session, struct drv_usr_share *usr_data, u32 memory_type)
{
	int ret = 0;
	struct npu_memory_buffer *model_mem_buf = NULL, *ncp_hdr_buf = NULL;

	model_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (model_mem_buf == NULL) {
		npu_err("fail in npu_ion_map kzalloc\n");
		return -ENOMEM;
	}

	switch (memory_type) {
		case MEMORY_TYPE_NCP:
			{
				model_mem_buf->m.fd = usr_data->ncp_fd;
				model_mem_buf->size = usr_data->ncp_size;
				model_mem_buf->ncp = true;

				session->ncp_mem_buf_offset = usr_data->ncp_offset;

				if (session->hids == NPU_HWDEV_ID_NPU)
					strcpy(model_mem_buf->name, "NCP");
				else
					strcpy(model_mem_buf->name, "UCGO");

				break;
			}
		case MEMORY_TYPE_WEIGHT:
			{
				model_mem_buf->m.fd = usr_data->ncp_fd;
				model_mem_buf->size = usr_data->ncp_size;

				break;
			}
		default:
			npu_err("can't support memory type\n");
			ret = -EINVAL;
			goto err_free;
	}

	if (model_mem_buf->m.fd <= 0) {
		ret = -EBADF;
		goto err_free;
	}

	ret = npu_memory_map(session->memory, model_mem_buf, npu_get_configs(NPU_PBHA_HINT_11), session->lazy_unmap_disable);
	if (unlikely(ret)) {
		npu_err("npu_memory_map is fail(%d).\n", ret);
		goto err_free;
	}

	ret = update_ncp_info(session, model_mem_buf, memory_type);
	if (ret) {
		npu_err("fail(%d).\n", ret);
		goto err_unmap;
	}

	ret = add_ion_mem(session, model_mem_buf, memory_type);
	if (ret) {
		npu_err("fail(%d).\n", ret);
		goto err_unmap;
	}

	if ((session->hids & NPU_HWDEV_ID_NPU) && (memory_type == MEMORY_TYPE_NCP)) {
		ncp_hdr_buf = copy_ncp_header(session, model_mem_buf);
		if (IS_ERR_OR_NULL(ncp_hdr_buf))
			return PTR_ERR(ncp_hdr_buf); /* free buffer from __release_graph_ion */

		ret = npu_util_validate_user_ncp(session, ncp_hdr_buf->vaddr, model_mem_buf->size);
		if (ret)
			return ret; /* free buffer from __release_graph_ion */
	}

	return ret;
err_unmap:
	npu_memory_unmap(session->memory, model_mem_buf);
err_free:
	kfree(model_mem_buf);
	return ret;
}

int __get_session_info(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	size_t fd_size = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct drv_usr_share *usr_data = NULL;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);

	// To be removed
	if (info->size == 0)
		info->size++;

	if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE)
		fd_size = 1;
	else
		fd_size = info->size;

	usr_data = kzalloc(fd_size * sizeof(struct drv_usr_share), GFP_KERNEL);
	if (unlikely(!usr_data)) {
		npu_err("Not enough Memory\n");
		ret = -ENOMEM;
		return ret;
	}
	ret = copy_from_user((void *)usr_data, (void *)info->addr, fd_size * sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_from_user failed(%d)\n", ret);
		goto p_err;
	}
	__set_unique_id(session, usr_data);

	ret = __set_unified_id(session, usr_data);
	if (ret < 0) {
		npu_err("__set_unified_id failed(%d)\n", ret);
		goto p_err;
	}

	ret = copy_to_user((void *)info->addr, (void *)usr_data, fd_size * sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_to_user failed(%d)\n", ret);
		goto p_err;
	}
	ret = model_ion_map(session, usr_data, MEMORY_TYPE_NCP);
	if (unlikely(ret)) {
		npu_uerr("model_ion_map is fail(%d)\n", session, ret);
		goto p_err;
	}

	if (fd_size > 1) {
		ret = model_ion_map(session, &usr_data[1], MEMORY_TYPE_WEIGHT);
		if (unlikely(ret)) {
			npu_uerr("model_ion_map is fail(%d)\n", session, ret);
			goto p_err;
		}
	}

	session->ss_state |= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP);
p_err:
	if (usr_data)
		kfree(usr_data);
	return ret;
}

int __pilot_parsing_ncp(struct npu_session *session, u32 *IFM_cnt, u32 *OFM_cnt, u32 *IMB_cnt, u32 *WGT_cnt)
{
	int ret = 0;
	u32 i = 0;
	char *ncp_vaddr;
	u32 memory_vector_cnt;
	u32 memory_vector_offset;
	struct memory_vector *mv;
	struct ncp_header *ncp;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct npu_model_info_hash *h;
	struct npu_sessionmgr *sessionmgr;
	u32 key;

	sessionmgr = session->cookie;
#endif
	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;

	ncp = (struct ncp_header *)ncp_vaddr;

	if (ncp->thread_vector_cnt == 2)
		session->is_instance_1 = true;

	npu_uinfo("session is instance %s\n", session, session->is_instance_1 ? "1" : "N");

	memory_vector_offset = ncp->memory_vector_offset;
	if (unlikely(memory_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("memory vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				(u32)memory_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	memory_vector_cnt = ncp->memory_vector_cnt;
	if (unlikely(((memory_vector_cnt * sizeof(struct memory_vector)) + memory_vector_offset) >  session->ncp_mem_buf->size)) {
		npu_err("memory vector count(0x%x) seems abnormal ;out of bounds\n", memory_vector_cnt);
		return -EFAULT;
	}
	session->memory_vector_offset = memory_vector_offset;
	session->memory_vector_cnt = memory_vector_cnt;

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;

		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				(*IFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				(*OFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				(*IMB_cnt)++;
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				(*WGT_cnt)++;
				break;
			}
		}
	}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	ncp->model_name[NCP_MODEL_NAME_LEN - 1] = '\0';
	if (!ncp->computational_workload) ncp->computational_workload = ncp->io_workload;
	key = npu_get_hash_name_key(ncp->model_name, ncp->computational_workload, ncp->io_workload);
	mutex_lock(&sessionmgr->model_lock);
	hash_for_each_possible(sessionmgr->model_info_hash_head, h, hlist, key) {
		if(is_matching_ncp_for_hash(h, ncp)) {
			session->model_hash_node = h;
			break;
		}
	}
	mutex_unlock(&sessionmgr->model_lock);
	if (session->model_hash_node == 0) {
		session->model_hash_node = kcalloc(1, sizeof(struct npu_model_info_hash), GFP_KERNEL);
		session->model_hash_node->alpha = MAX_ALPHA;
		session->model_hash_node->beta = MAX_BETA;
		session->check_core = false;
		session->model_hash_node->miss_cnt = 0;
		session->model_hash_node->computational_workload = ncp->computational_workload;
		session->model_hash_node->io_workload = ncp->io_workload;
		strcpy(session->model_hash_node->model_name, ncp->model_name);
		init_workload_params(session);

		mutex_lock(&sessionmgr->model_lock);
		hash_add(sessionmgr->model_info_hash_head, &session->model_hash_node->hlist, key);
		mutex_unlock(&sessionmgr->model_lock);
	}
	npu_info("\n session->compute_workload = 0x%x session->io_workload = 0x%x\n",
			session->model_hash_node->computational_workload, session->model_hash_node->io_workload);
#endif
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	{
		struct npu_device *device;
		struct npu_vertex *vertex;

		vertex = session->vctx.vertex;
		device = container_of(vertex, struct npu_device, vertex);

		if (device->sched->mode != NPU_PERF_MODE_NPU_BOOST &&
				device->sched->mode != NPU_PERF_MODE_NPU_BOOST_PRUNE) {
			session->key = key;
			session->computational_workload = ncp->computational_workload;
			session->io_workload = ncp->io_workload;
			session->featuremapdata_type = ncp->featuremapdata_type;
			strncpy(session->model_name, ncp->model_name, NCP_MODEL_NAME_LEN);
			npu_precision_hash_check(session);
		}
	}
#else
	strncpy(session->model_name, ncp->model_name, NCP_MODEL_NAME_LEN);
#endif
	return ret;
}

int __second_parsing_ncp(
	struct npu_session *session,
	struct addr_info **IFM_av, struct addr_info **OFM_av,
	struct addr_info **IMB_av, struct addr_info **WGT_av)
{
	int ret = 0;
	u32 i = 0;
	u32 weight_size;

	u32 IFM_cnt = 0;
	u32 OFM_cnt = 0;
	u32 IMB_cnt = 0;
	u32 WGT_cnt = 0;

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	struct ncp_header *ncp;
	struct address_vector *av;
	struct memory_vector *mv;

	char *ncp_vaddr;
	dma_addr_t ncp_daddr;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;

	ncp = (struct ncp_header *)ncp_vaddr;

	if (ncp->model_name[0] != '\0')
		strncpy(session->ncp_hdr_buf->name, ncp->model_name, sizeof(session->ncp_hdr_buf->name) - 1);

	address_vector_offset = ncp->address_vector_offset;
	if (unlikely(address_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("address vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				address_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}


	address_vector_cnt = ncp->address_vector_cnt;
	if (unlikely(((address_vector_cnt * sizeof(struct address_vector)) + address_vector_offset) >
									session->ncp_mem_buf->size)) {
		npu_err("address vector count(0x%x) seems abnormal ;out of bounds\n", address_vector_cnt);
		return -EFAULT;
	}

	session->address_vector_offset = address_vector_offset;
	session->address_vector_cnt = address_vector_cnt;

	session->ncp_info.address_vector_cnt = address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;
	ncp->unique_id |= session->unified_id << 16;
	ncp->fence_flag = 1 << session->uid;

	if (address_vector_cnt > memory_vector_cnt) {
		npu_err("address_vector_cnt(%d) should not exceed memory_vector_cnt(%d)\n",
				address_vector_cnt, memory_vector_cnt);
		return -EFAULT;
	}

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;
		u32 address_vector_index;
		u32 weight_offset;

		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				if (IFM_cnt >= session->IFM_cnt) {
					npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							IFM_cnt, session->IFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IFM_cnt, *IFM_av, address_vector_index))) {
					(*IFM_av + IFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IFM_av + IFM_cnt)->size = (av + address_vector_index)->size;
					(*IFM_av + IFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IFM_av + IFM_cnt)->width = (mv + i)->width;
					(*IFM_av + IFM_cnt)->height = (mv + i)->height;
					(*IFM_av + IFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IFM_av + IFM_cnt)->stride = (mv + i)->wstride;
					npu_udbg("(IFM_av + %u)->index = %u\n", session,
						IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
					npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
					npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
					npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
					npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
					npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

					IFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				if (OFM_cnt >= session->OFM_cnt) {
					npu_err("OFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							OFM_cnt, session->OFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&OFM_cnt, *OFM_av, address_vector_index))) {
					(*OFM_av + OFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*OFM_av + OFM_cnt)->size = (av + address_vector_index)->size;
					(*OFM_av + OFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*OFM_av + OFM_cnt)->width = (mv + i)->width;
					(*OFM_av + OFM_cnt)->height = (mv + i)->height;
					(*OFM_av + OFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*OFM_av + OFM_cnt)->stride = (mv + i)->wstride;
					npu_udbg("(OFM_av + %u)->index = %u\n", session, OFM_cnt, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("OFM, index(%d)\n", session, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("[session] OFM, size(%zu)\n", session, (*OFM_av + OFM_cnt)->size);
					npu_utrace("[session] OFM, pixel_format(%u)\n", session, (*OFM_av + OFM_cnt)->pixel_format);
					npu_utrace("[session] OFM, width(%u)\n", session, (*OFM_av + OFM_cnt)->width);
					npu_utrace("[session] OFM, height(%u)\n", session, (*OFM_av + OFM_cnt)->height);
					npu_utrace("[session] OFM, channels(%u)\n", session, (*OFM_av + OFM_cnt)->channels);
					npu_utrace("[session] OFM, wstride(%u)\n", session, (*OFM_av + OFM_cnt)->stride);
					OFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				if (IMB_cnt >= session->IMB_cnt) {
					npu_err("IMB: IMB_cnt(%d) should not exceed size of allocated array(%d)\n",
							IMB_cnt, session->IMB_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IMB_cnt, *IMB_av, address_vector_index))) {
					(*IMB_av + IMB_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("IMB: address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IMB_av + IMB_cnt)->size = (av + address_vector_index)->size;
					(*IMB_av + IMB_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IMB_av + IMB_cnt)->width = (mv + i)->width;
					(*IMB_av + IMB_cnt)->height = (mv + i)->height;
					(*IMB_av + IMB_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IMB_av + IMB_cnt)->stride = (mv + i)->wstride;
					npu_info("IMB: (*IMB_av + %u)->index = %x\n",
							IMB_cnt, (*IMB_av + IMB_cnt)->av_index);
					IMB_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				ncp_daddr = session->ncp_mem_buf->daddr;
				ncp_daddr += session->ncp_mem_buf_offset;

				ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;
				ncp_vaddr += session->ncp_mem_buf_offset;

				if (WGT_cnt >= session->WGT_cnt) {
					npu_err("WGT_cnt(%d) should not exceed size of allocated array(%d)\n",
							WGT_cnt, session->WGT_cnt);
					return -EFAULT;
				}
				// update address vector, m_addr with ncp_alloc_daddr + offset
				address_vector_index = (mv + i)->address_vector_index;
				if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
					npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
							address_vector_index, address_vector_cnt);
					return -EFAULT;
				}
				weight_offset = (av + address_vector_index)->m_addr;
				if (unlikely(weight_offset > (u32)session->ncp_mem_buf->size)) {
					ret = -EINVAL;
					npu_uerr("weight_offset is invalid, offset(0x%x), ncp_daddr(0x%x)\n",
						session, (u32)weight_offset, (u32)session->ncp_mem_buf->size);
					goto p_err;
				}

				if (weight_offset == 0) {
					if (!session->weight_mem_buf || session->weight_mem_buf->m.fd <= 0)
						return -EFAULT;
					if ((av + address_vector_index)->size > session->weight_mem_buf->size)
						return -EFAULT;

					(av + address_vector_index)->m_addr = session->weight_mem_buf->daddr;

					(*WGT_av + WGT_cnt)->size = session->weight_mem_buf->size;
					(*WGT_av + WGT_cnt)->daddr = session->weight_mem_buf->daddr;
					(*WGT_av + WGT_cnt)->vaddr = session->weight_mem_buf->vaddr;
					(*WGT_av + WGT_cnt)->memory_type = memory_type;
				} else {
					(av + address_vector_index)->m_addr = weight_offset + ncp_daddr;

					(*WGT_av + WGT_cnt)->av_index = address_vector_index;
					weight_size = (av + address_vector_index)->size;
					if (unlikely((weight_offset + weight_size) < weight_size)) {
						npu_err("weight_offset(0x%x) + weight size (0x%x) seems to be overflow.\n",
								weight_offset, weight_size);
						return -ERANGE;
					}
					if (unlikely((weight_offset + weight_size) > (u32)session->ncp_mem_buf->size)) {
						npu_err("weight_offset(0x%x) + weight size (0x%x) seems to go beyond ncp size(0x%x)\n",
								weight_offset, weight_size, (u32)session->ncp_mem_buf->size);
						return -EFAULT;
					}

					(*WGT_av + WGT_cnt)->size = weight_size;
					(*WGT_av + WGT_cnt)->daddr = weight_offset + ncp_daddr;
					(*WGT_av + WGT_cnt)->vaddr = weight_offset + ncp_vaddr;
					(*WGT_av + WGT_cnt)->memory_type = memory_type;
				}
				WGT_cnt++;
				break;
			}
		default:
			break;
		}
	}
	session->IOFM_cnt = IFM_cnt + OFM_cnt;
	return ret;
p_err:
	return ret;
}

int __ion_alloc_IOFM(struct npu_session *session, struct addr_info **IFM_av, struct addr_info **OFM_av)
{
	int ret = 0;
	u32 i = 0, j = 0, k = 0;
	struct npu_memory_buffer *IOFM_mem_buf;
	struct address_vector *vaddr;

	u32 IFM_cnt = session->IFM_cnt;
	u32 IOFM_cnt = session->IOFM_cnt;

	IOFM_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (unlikely(!IOFM_mem_buf)) {
		npu_err("failed in (ENOMEM)\n");
		ret = -ENOMEM;
		return ret;
	}
	npu_udbg("(0x%pK)\n", session, IOFM_mem_buf);

	IOFM_mem_buf->size = NPU_FRAME_MAX_CONTAINERLIST * NPU_FRAME_MAX_BUFFER * IOFM_cnt * sizeof(struct address_vector);
	strcpy(IOFM_mem_buf->name, "IOFM");
	ret = npu_memory_alloc(session->memory, IOFM_mem_buf, npu_get_configs(NPU_PBHA_HINT_00));
	if (unlikely(ret)) {
		npu_uerr("npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}
	npu_udbg(" (IOFM_mem_buf + %d)->vaddr(0x%pK), daddr(0x%pad), size(%zu)\n",
		session, 0, (IOFM_mem_buf + 0)->vaddr, &(IOFM_mem_buf + 0)->daddr, (IOFM_mem_buf + 0)->size);

	vaddr = IOFM_mem_buf->vaddr;
	for (i = 0; i < IOFM_cnt; i++) {
		for (k = 0; k < NPU_FRAME_MAX_CONTAINERLIST; k++) {
			for (j = 0; j < NPU_FRAME_MAX_BUFFER; j++) {
				if (i < IFM_cnt) {
					(vaddr + (k*NPU_FRAME_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*IFM_av + i)->av_index;
					(vaddr + (k*NPU_FRAME_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*IFM_av + i)->size;
				} else {
					(vaddr + (k*NPU_FRAME_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*OFM_av + (i - IFM_cnt))->av_index;
					(vaddr + (k*NPU_FRAME_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*OFM_av + (i - IFM_cnt))->size;
				}
			}
		}
	}
	ret = add_ion_mem(session, IOFM_mem_buf, MEMORY_TYPE_IN_FMAP);
	return ret;
p_err:
	npu_memory_free(session->memory, IOFM_mem_buf);
	if (IOFM_mem_buf)
		kfree(IOFM_mem_buf);
	return ret;
}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
int __make_IMB_info(struct npu_session *session, struct addr_info **temp_IMB_av)
{
	int ret = 0;
	u32 i = 0;
	u32 IMB_cnt = session->IMB_cnt;
	struct addr_info *IMB_info;

	IMB_info = session->IMB_info;
	for (i = 0; i < IMB_cnt; i++) {
		(IMB_info + i)->av_index = (*temp_IMB_av + i)->av_index;
		(IMB_info + i)->vaddr = (*temp_IMB_av + i)->vaddr;
		(IMB_info + i)->daddr = (*temp_IMB_av + i)->daddr;
		(IMB_info + i)->size = (*temp_IMB_av + i)->size;
		(IMB_info + i)->pixel_format = (*temp_IMB_av + i)->pixel_format;
		(IMB_info + i)->width = (*temp_IMB_av + i)->width;
		(IMB_info + i)->height = (*temp_IMB_av + i)->height;
		(IMB_info + i)->channels = (*temp_IMB_av + i)->channels;
		(IMB_info + i)->stride = (*temp_IMB_av + i)->stride;
	}
	return ret;
}

int __prepare_IMB_info(struct npu_session *session, struct addr_info **IMB_av, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;
	u32 i = 0;
	u32 IMB_cnt = session->IMB_cnt;
	struct addr_info *IMB_info;

	IMB_info = kcalloc(IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	if (unlikely(!IMB_info)) {
		npu_err("IMB: IMB_info alloc is fail\n");
		ret = -ENOMEM;
		goto p_err;
	}

	session->IMB_info = IMB_info;
	__make_IMB_info(session, IMB_av);

	for (i = 0; i < IMB_cnt; i++) {
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		IMB_mem_buf->ncp_max_size = max(IMB_mem_buf->ncp_max_size, ALIGN((*IMB_av + i)->size, NPU_IMB_ALIGN));
#endif
		IMB_mem_buf->size += ALIGN((*IMB_av + i)->size, NPU_IMB_ALIGN);
	}
	session->IMB_size = IMB_mem_buf->size;
	session->IMB_mem_buf = IMB_mem_buf;

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	if (session->IMB_size > (NPU_IMB_CHUNK_SIZE * NPU_IMB_CHUNK_MAX_NUM)) {
		npu_uerr("IMB_size(%zu) is larger than %u\n",
			session, session->IMB_size, NPU_IMB_CHUNK_SIZE * NPU_IMB_CHUNK_MAX_NUM);
		ret = -ENOMEM;
		goto p_err;
	}
#endif

	return ret;

p_err:
	if (likely(IMB_info))
		kfree(IMB_info);
	if (likely(IMB_mem_buf))
		kfree(IMB_mem_buf);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
int __ion_alloc_IMB(struct npu_session *session, struct addr_info **IMB_av, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;
	u32 i = 0;
	u32 address_vector_offset;
	struct address_vector *av;
	char *ncp_vaddr;
	u32 IMB_cnt = session->IMB_cnt;
	u32 addr_offset = 0;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	struct npu_system *system = get_session_system(session);
	u32 req_chunk_cnt = __calc_imb_req_chunk(session, IMB_mem_buf, false);
#endif
	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	address_vector_offset = session->address_vector_offset;

	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->IMB_size >= NPU_IMB_ASYNC_THRESHOLD) {
		npu_udbg("Async IMB allocation starts\n", session);
		ret = npu_imb_alloc_async(session, IMB_av, IMB_mem_buf);
		if (unlikely(ret)) {
			session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_ERROR;
			goto p_err;
		} else {
			 npu_udbg("Async IMB allocation ends\n", session);
			 return ret;
		}
	} else {
		if (session->imb_async_wq) {
			destroy_workqueue(session->imb_async_wq);
			session->imb_async_wq = NULL;
		}
	}
#endif
	npu_udbg("Sync IMB allocation starts\n", session);
	mutex_lock(&system->imb_alloc_data.imb_alloc_lock);
	ret = __alloc_imb_chunk(IMB_mem_buf, system,
				&system->imb_alloc_data, req_chunk_cnt);
	if (unlikely(ret)) {
		npu_err("IMB: __alloc_imb_chunk is fail(%d)\n", ret);
		mutex_unlock(&system->imb_alloc_data.imb_alloc_lock);
		goto p_err;
	}
	/* in allocation case we're not interested in FW size acceptance */
	__fw_sync_CMD_IMB_SIZE(session, system->imb_alloc_data.alloc_chunk_cnt);
	mutex_unlock(&system->imb_alloc_data.imb_alloc_lock);
#else /* !CONFIG_NPU_USE_IMB_ALLOCATOR */
	ret = npu_memory_alloc(session->memory, IMB_mem_buf, npu_get_configs(NPU_PBHA_HINT_00));
	if (unlikely(ret)) {
		npu_uerr("IMB: npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}
#endif
	for (i = 0; i < IMB_cnt; i++) {
		(av + (*IMB_av + i)->av_index)->m_addr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->daddr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->vaddr = ((void *)((char *)(IMB_mem_buf->vaddr)) + addr_offset);
		(session->IMB_info + i)->size = (*IMB_av + i)->size;
		addr_offset += ALIGN((*IMB_av + i)->size, (u32)NPU_IMB_ALIGN);
		npu_udbg("IMB: (IMB_mem_buf + %d)->vaddr(%pad), daddr(%pad), size(%zu)\n",
			 session, i, (session->IMB_info + i)->vaddr, &(session->IMB_info + i)->daddr,
			 (session->IMB_info + i)->size);
	}

	npu_session_ion_sync_for_device(IMB_mem_buf, DMA_TO_DEVICE);
	return ret;

p_err:
#if !IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	npu_memory_free(session->memory, IMB_mem_buf);
#endif
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->imb_async_wq) {
		destroy_workqueue(session->imb_async_wq);
		session->imb_async_wq = NULL;
	}
#endif

	kfree(IMB_mem_buf);
	session->IMB_mem_buf = NULL;

	return ret;
}
#endif

int __config_session_info(struct npu_session *session)
{
	int ret = 0;

	struct addr_info *IFM_av = NULL;
	struct addr_info *OFM_av = NULL;
	struct addr_info *IMB_av = NULL;
	struct addr_info *WGT_av = NULL;

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	struct npu_memory_buffer *IMB_mem_buf = NULL;
#endif
	ret = __pilot_parsing_ncp(session, &session->IFM_cnt, &session->OFM_cnt, &session->IMB_cnt, &session->WGT_cnt);
	if (ret) {
		npu_err("failed in __pilot_parsing_ncp(%d)\n", ret);
		goto p_err;
	}

	IFM_av = kcalloc(session->IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	OFM_av = kcalloc(session->OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	IMB_av = kcalloc(session->IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	WGT_av = kcalloc(session->WGT_cnt, sizeof(struct addr_info), GFP_KERNEL);

	session->WGT_info = WGT_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_WGT_KALLOC);

	if (unlikely(!IFM_av || !OFM_av || !IMB_av || !WGT_av)) {
		npu_err("failed in (ENOMEM)\n");
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __second_parsing_ncp(session, &IFM_av, &OFM_av, &IMB_av, &WGT_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in second_parsing_ncp\n", session, ret);
		goto p_err;
	}

	session->IFM_info.addr_info = IFM_av;
	session->OFM_info.addr_info = OFM_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_KALLOC);

	ret = __ion_alloc_IOFM(session, &IFM_av, &OFM_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IOFM\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	IMB_mem_buf = kcalloc(1, sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (unlikely(!IMB_mem_buf)) {
		npu_err("IMB: IMB_mem_buf alloc is fail\n");
		ret = -ENOMEM;
		goto p_err;
	}
	strcpy(IMB_mem_buf->name, "IMB");

	ret = __prepare_IMB_info(session, &IMB_av, IMB_mem_buf);
	if (unlikely(ret)) {
		npu_uerr("IMB: fail(%d) in __prepare_IMB_info\n", session, ret);
		goto p_err;
	}
#endif
	npu_udbg("IMB: buffer summary size = 0x%zX\n", session, session->IMB_size);
#if IS_ENABLED(CONFIG_NPU_IMB_THRESHOLD)
	npu_udbg("IMB: threshold size = 0x%X\n", session, npu_get_configs(NPU_IMB_THRESHOLD_SIZE));
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	npu_udbg("IMB: NCP max size = 0x%zX\n", session, session->IMB_mem_buf->ncp_max_size);
	npu_udbg("IMB: granularity size = 0x%X\n", session, NPU_IMB_GRANULARITY);

	/* For CONFIG_NPU_USE_IMB_ALLOCATOR case we allocate/deallocate additional memory chunks also dynamically
	 * in qbuf, but for now as we're using chunks nature we can allocate all that we need here to skip it at
	 * first qbuf iteration and just deallocate additional chunks in dequeu buf by NPU_IMB_THRESHOLD_SIZE limit.
	 */
	if (session->IMB_size <= npu_get_configs(NPU_IMB_THRESHOLD_SIZE)) {
		ret = __ion_alloc_IMB(session, &IMB_av, IMB_mem_buf);
		if (unlikely(ret)) {
			npu_uerr("IMB: fail(%d) in ion alloc IMB\n", session, ret);
			goto p_err;
		}
		session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);
	}
#endif
	npu_session_ion_sync_for_device(session->ncp_hdr_buf, DMA_TO_DEVICE);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	__cfifo_sync_for_device(session);
#endif

#if !IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (likely(IMB_av))
		kfree(IMB_av);
#endif

	return ret;
p_err:
	if (likely(IFM_av)) {
		kfree(IFM_av);
		session->IFM_info.addr_info = NULL;
	}

	if (likely(OFM_av)) {
		kfree(OFM_av);
		session->OFM_info.addr_info = NULL;
	}

	if (likely(WGT_av)) {
		kfree(WGT_av);
		session->WGT_info = NULL;
	}

	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int __pilot_parsing_ncp_for_dsp(struct npu_session *session, u32 *IFM_cnt, u32 *OFM_cnt, u32 *IMB_cnt, u32 *WGT_cnt)
{
	int ret = 0;
	u32 i = 0;
	char *msg_vaddr;
	struct dsp_common_graph_info_v4 *msg;
	struct dsp_common_param_v4 *param;
	u32 param_cnt;

	msg_vaddr = (char *)session->ncp_mem_buf->vaddr;
	msg_vaddr += session->ncp_mem_buf_offset;

	msg = (struct dsp_common_graph_info_v4 *)msg_vaddr;
	param = (struct dsp_common_param_v4 *)msg->param_list;
	param_cnt = msg->n_tsgd + msg->n_param;

	if (unlikely((param_cnt * sizeof(struct dsp_common_param_v4)) >  session->ncp_mem_buf->size)) {
  		npu_err("param count(0x%x) seems abnormal ;out of bounds\n", param_cnt);
  		return -EFAULT;
	}

	/* do NOT use memory vector for DSP */
	session->memory_vector_offset = 0;
	session->memory_vector_cnt = 0;

	for (i = 0; i < param_cnt; i++) {
		u32 param_type = (param + i)->param_type;

		switch (param_type) {
		case DSP_COMMON_MEM_IFM:
			{
				(*IFM_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_OFM:
			{
				(*OFM_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_IMB:
			{
				(*IMB_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_WGT:
			{
				(*WGT_cnt)++;
				break;
			}
		}
	}

	/* DSP has at lease 1 IFM as execute msg */
	if (!(*IFM_cnt))
		(*IFM_cnt)++;
	return ret;
}

int __second_parsing_ncp_for_dsp(
	struct npu_session *session,
	struct addr_info **IFM_av, struct addr_info **OFM_av,
	struct addr_info **IMB_av, struct addr_info **WGT_av)
{
	int ret = 0;
	u32 i = 0;

	u32 IFM_cnt = 0;
	u32 OFM_cnt = 0;
	u32 IMB_cnt = 0;
	u32 WGT_cnt = 0;

	char *msg_vaddr;
	struct dsp_common_graph_info_v4 *msg;
	struct dsp_common_param_v4 *param;
	struct dsp_common_mem_v4 *mem;
	u32 param_type, param_cnt;

	msg_vaddr = (char *)session->ncp_mem_buf->vaddr;
	msg_vaddr += session->ncp_mem_buf_offset;

	msg = (struct dsp_common_graph_info_v4 *)msg_vaddr;
	param = (struct dsp_common_param_v4 *)msg->param_list;
	param_cnt = msg->n_tsgd + msg->n_param;

	/* do NOT use address vector for DSP */
	session->address_vector_offset = 0;
	session->address_vector_cnt = 0;

	session->ncp_info.address_vector_cnt = 0;

	for (i = 0; i < param_cnt; i++) {
		param_type = (param + i)->param_type;
		mem = &((param + i)->param_mem);
		switch (param_type) {
		case DSP_COMMON_MEM_IFM:
			{
				if (IFM_cnt >= session->IFM_cnt) {
					npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							IFM_cnt, session->IFM_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*IFM_av + IFM_cnt)->av_index = 0;
				(*IFM_av + IFM_cnt)->size = mem->size;
				/* use param in 'struct dsp_common_mem_vx'
				 * for format info
				 */
				(*IFM_av + IFM_cnt)->pixel_format = mem->param[3];
				(*IFM_av + IFM_cnt)->width = mem->param[0];
				(*IFM_av + IFM_cnt)->height = mem->param[1];
				(*IFM_av + IFM_cnt)->channels = mem->param[2];
				(*IFM_av + IFM_cnt)->stride = 0;
				npu_udbg("(IFM_av + %u)->index = %u\n", session,
						IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
				npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
				npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
				npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
				npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
				npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
				npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
				npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

				IFM_cnt++;
				break;
			}
		case DSP_COMMON_MEM_OFM:
			{
				if (OFM_cnt >= session->OFM_cnt) {
					npu_err("OFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							OFM_cnt, session->OFM_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*OFM_av + OFM_cnt)->av_index = 0;
				(*OFM_av + OFM_cnt)->size = mem->size;
				/* use param in 'struct dsp_common_mem_vx'
				 * for format info
				 */
				(*OFM_av + OFM_cnt)->pixel_format = mem->param[3];
				(*OFM_av + OFM_cnt)->width = mem->param[0];
				(*OFM_av + OFM_cnt)->height = mem->param[1];
				(*OFM_av + OFM_cnt)->channels = mem->param[2];
				(*OFM_av + OFM_cnt)->stride = 0;
				npu_udbg("(OFM_av + %u)->index = %u\n", session, OFM_cnt, (*OFM_av + OFM_cnt)->av_index);
				npu_utrace("OFM, index(%d)\n", session, (*OFM_av + OFM_cnt)->av_index);
				npu_utrace("[session] OFM, size(%zu)\n", session, (*OFM_av + OFM_cnt)->size);
				npu_utrace("[session] OFM, pixel_format(%u)\n", session, (*OFM_av + OFM_cnt)->pixel_format);
				npu_utrace("[session] OFM, width(%u)\n", session, (*OFM_av + OFM_cnt)->width);
				npu_utrace("[session] OFM, height(%u)\n", session, (*OFM_av + OFM_cnt)->height);
				npu_utrace("[session] OFM, channels(%u)\n", session, (*OFM_av + OFM_cnt)->channels);
				npu_utrace("[session] OFM, wstride(%u)\n", session, (*OFM_av + OFM_cnt)->stride);
				OFM_cnt++;
				break;
			}
		case DSP_COMMON_MEM_IMB:
			{
				if (IMB_cnt >= session->IMB_cnt) {
					npu_err("IMB_cnt(%d) should not exceed size of allocated array(%d)\n",
							IMB_cnt, session->IMB_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*IMB_av + IMB_cnt)->av_index = 0;
				(*IMB_av + IMB_cnt)->size = mem->size;
				(*IMB_av + IMB_cnt)->pixel_format = mem->param[3];
				(*IMB_av + IMB_cnt)->width = mem->param[0];
				(*IMB_av + IMB_cnt)->height = mem->param[1];
				(*IMB_av + IMB_cnt)->channels = mem->param[2];
				(*IMB_av + IMB_cnt)->stride = 0;
				IMB_cnt++;
				break;
			}
		case DSP_COMMON_MEM_WGT:
			{
				if (WGT_cnt >= session->WGT_cnt) {
					npu_err("WGT_cnt(%d) should not exceed size of allocated array(%d)\n",
							WGT_cnt, session->WGT_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*WGT_av + WGT_cnt)->av_index = 0;
				(*WGT_av + WGT_cnt)->size = mem->size;
				(*WGT_av + WGT_cnt)->daddr = mem->iova;
				(*WGT_av + WGT_cnt)->memory_type = mem->mem_type;
				npu_utrace("(*WGT_av + %u)->av_index = %u\n", session,	WGT_cnt, (*WGT_av + WGT_cnt)->av_index);
				npu_utrace("(*WGT_av + %u)->size = %zu\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->size);
				npu_utrace("(*WGT_av + %u)->daddr = 0x%pad\n", session, WGT_cnt, &((*WGT_av + WGT_cnt)->daddr));
				WGT_cnt++;
				break;
			}
		default:
			break;
		}
	}

	/* DSP has at lease 1 IFM as execute msg */
	if (!IFM_cnt) {
		if (IFM_cnt >= session->IFM_cnt) {
			npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
					IFM_cnt, session->IFM_cnt);
			return -EFAULT;
		}
		/* do NOT use address vector for DSP */
		(*IFM_av + IFM_cnt)->av_index = 0;
		(*IFM_av + IFM_cnt)->size = 0;
		/* use param in 'struct dsp_common_mem_vx'
		 * for format info
		 */
		(*IFM_av + IFM_cnt)->pixel_format = 0;
		(*IFM_av + IFM_cnt)->width = 0;
		(*IFM_av + IFM_cnt)->height = 0;
		(*IFM_av + IFM_cnt)->channels = 0;
		(*IFM_av + IFM_cnt)->stride = 0;
		npu_udbg("(IFM_av + %u)->index = %u\n", session,
				IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
		npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
		npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
		npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
		npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
		npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
		npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
		npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

		IFM_cnt++;
	}

	session->IOFM_cnt = IFM_cnt + OFM_cnt;
	return ret;
}

int __config_session_info_for_dsp(struct npu_session *session)
{
	int ret = 0;

	struct addr_info *IFM_av = NULL;
	struct addr_info *OFM_av = NULL;
	struct addr_info *IMB_av = NULL;
	struct addr_info *WGT_av = NULL;

	ret = __pilot_parsing_ncp_for_dsp(session, &session->IFM_cnt, &session->OFM_cnt, &session->IMB_cnt, &session->WGT_cnt);
	if (ret) {
		npu_err("failed in __pilot_parsing_ncp(%d)\n", ret);
		goto p_err;
	}

	IFM_av = kcalloc(session->IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	OFM_av = kcalloc(session->OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	IMB_av = kcalloc(session->IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	WGT_av = kcalloc(session->WGT_cnt, sizeof(struct addr_info), GFP_KERNEL);

	session->WGT_info = WGT_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_WGT_KALLOC);

	if (unlikely(!IFM_av || !OFM_av || !IMB_av || !WGT_av)) {
		npu_err("failed in __config_session_info(ENOMEM)\n");
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __second_parsing_ncp_for_dsp(session, &IFM_av, &OFM_av, &IMB_av, &WGT_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in second_parsing_ncp\n", session, ret);
		goto p_err;
	}

	session->IFM_info.addr_info = IFM_av;
	session->OFM_info.addr_info = OFM_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_KALLOC);

	ret = __ion_alloc_IOFM(session, &IFM_av, &OFM_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IOFM\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC);

	session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);

	ret = __update_iova_of_load_message_of_dsp(session);
	if (unlikely(ret)) {
		npu_uerr("invalid(%d) in __update_iova_of_load_message\n",
				session, ret);
		goto p_err;
	}

	npu_session_ion_sync_for_device(session->ncp_mem_buf, DMA_TO_DEVICE);

	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
p_err:
	if (likely(IFM_av)) {
		kfree(IFM_av);
		session->IFM_info.addr_info = NULL;
	}

	if (likely(OFM_av)) {
		kfree(OFM_av);
		session->OFM_info.addr_info = NULL;
	}

	if (likely(WGT_av)) {
		kfree(WGT_av);
		session->WGT_info = NULL;
	}

	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
}
#endif

int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);

	BUG_ON(!session);
	BUG_ON(!info);
	ret = __get_session_info(session, info);
	if (unlikely(ret)) {
		npu_uerr("invalid(%d) in __get_session_info\n", session, ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	/* jump by hids */
	if (session->hids & NPU_HWDEV_ID_NPU &&
		session->hids & NPU_HWDEV_ID_DSP) {
			/* need to support unified model */
			npu_uerr("can't support HWDEV NPU&DSP\n", session);
			return -EINVAL;
	} else if (session->hids & NPU_HWDEV_ID_NPU) {
#endif
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
		session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_SKIP;
#endif

		ret = __config_session_info(session);
		if (unlikely(ret)) {
			npu_uerr("invalid in __config_session_info\n", session);
			goto p_err;
		}

		if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE) {
			ret = __shared_ion_map(session, info);
			if (unlikely(ret)) {
				npu_uerr("__shared_ion_map is fail(%d)\n", session, ret);
				goto p_err;
			}
			info->reserved1 = session->IOFM_mem_buf->daddr;
		}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	}	else if (session->hids & NPU_HWDEV_ID_DSP) {
		ret = __config_session_info_for_dsp(session);
		if (unlikely(ret)) {
			npu_uerr("invalid(%d) in __config_session_info_for_dsp\n",
					session, ret);
			goto p_err_dsp;
		}
	} else {
		npu_uerr("Invalid HW device ids\n", session);
	}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IFD)
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	npu_dvfs_set_info_to_session(session);
#endif
#endif

	return ret;

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
p_err_dsp:
	npu_uerr("Clean-up buffers for messages\n", session);
	__remove_buf_from_load_msg(session);
#endif

p_err:
	npu_uerr("Clean-up buffers for graph\n", session);
	__release_graph_ion(session);

	return ret;
}

int npu_session_start(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

	session->ss_state |= BIT(NPU_SESSION_STATE_START);
	return ret;
}

int npu_session_streamoff(struct npu_queue *queue)
{
	BUG_ON(!queue);
	return 0;
}

int npu_session_NW_CMD_STREAMOFF(struct npu_session *session)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_STREAMOFF;

	BUG_ON(!session);

	/* check npu_device emergency error */
	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_udbg("sending STREAM OFF command.\n", session);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	if (npu_device_is_emergency_err(device)) {
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR !\n", session);
		npu_udbg("sending CLEAR_CB command.\n", session);
		npu_session_put_nw_req(session, NPU_NW_CMD_CLEAR_CB);
		/* Clear CB has no notify function */
	}

	return 0;
}

int npu_session_stop(struct npu_queue *queue)
{
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->ss_state |= BIT(NPU_SESSION_STATE_STOP);

	return 0;
}

int npu_session_format(struct npu_queue *queue, struct vs4l_format_bundle *fbundle)
{
	int ret = 0;
	u32 i = 0, j = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	struct vs4l_format_list *flist;

	struct address_vector *av = NULL;
	struct memory_vector *mv = NULL;

	char *ncp_vaddr;
	struct vs4l_format *formats;
	struct addr_info *FM_av;
	u32 address_vector_offset;
	u32 address_vector_cnt;

	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	u32 cal_size;
	u32 bpp;
	u32 channels;
	u32 width;
	u32 height;

	u32 FM_cnt;

	BUG_ON(!queue);
	BUG_ON(!fbundle);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

	if (session->hids & NPU_HWDEV_ID_NPU) {
		ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	} else {
		ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;
		ncp_vaddr += session->ncp_mem_buf_offset;
	}

	address_vector_offset = session->address_vector_offset;
	address_vector_cnt = session->address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;

	if (memory_vector_cnt && memory_vector_offset)
		mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	if (address_vector_cnt && address_vector_offset)
		av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	for (i = 0; i < 2; i++) {
		flist = fbundle->m[i].flist;
		formats = flist->formats;

		if (flist->count <= 0)
			continue;

		if (flist->count > NPU_FRAME_MAX_CONTAINERLIST) {
			npu_err("flist->count(%d) cannot be greater to NPU_FRAME_MAX_CONTAINERLIST(%d)\n", flist->count, NPU_FRAME_MAX_CONTAINERLIST);
			ret = -EINVAL;
			goto p_err;
		}

		if (flist->direction == VS4L_DIRECTION_IN) {
			FM_av = session->IFM_info.addr_info;
			FM_cnt = session->IFM_cnt;
		} else {
			FM_av = session->OFM_info.addr_info;
			FM_cnt = session->OFM_cnt;
		}

		npu_udbg("flist %d, FM_cnt %d\n", session, flist->count, FM_cnt);
		if (!(session->hids & NPU_HWDEV_ID_DSP)) {
			if (FM_cnt > flist->count || FM_cnt + FM_SHARED_NUM < flist->count) {
				npu_ierr("FM_cnt(%d) is not same as flist_cnt(%d)\n", vctx, FM_cnt, flist->count);
				ret = -EINVAL;
				goto p_err;
			}

			for (j = 0; j < FM_cnt; j++) {
				if (mv) {
					(mv + (FM_av + j)->av_index)->wstride = (formats + j)->stride;
					(mv + (FM_av + j)->av_index)->cstride = (formats + j)->cstride;
				}

				bpp = (formats + j)->pixel_format;
				channels = (formats + j)->channels;
				width = (formats + j)->width;
				height = (formats + j)->height;
				cal_size = (bpp / 8) * channels * width * height;

				if (flist->direction == VS4L_DIRECTION_IN)
					queue->insize[j] = cal_size;
				else
					queue->otsize[j] = cal_size;

				npu_udbg("dir(%d), av_index(%d)\n", session, flist->direction, (FM_av + j)->av_index);
				npu_udbg("width(%u), height(%u), stride(%u)\n", session, width, height, (formats + j)->stride);
				npu_udbg("cstride(%u), channels(%u), pixel_format(%u)\n", session, (formats + j)->cstride, channels, bpp);
				npu_udbg("dir(%d), av_size(%zu), cal_size(%u)\n", session, flist->direction, (FM_av + j)->size, cal_size);
#if !IS_ENABLED(SYSMMU_FAULT_TEST)
				if ((FM_av + j)->size > cal_size) {
					if ((FM_av + j)->size % cal_size) {
						npu_udbg("in_size(%zu), cal_size(%u) invalid\n", session, (FM_av + j)->size, cal_size);
						ret = NPU_ERR_DRIVER(NPU_ERR_SIZE_NOT_MATCH);
						goto p_err;
					}
				}
#endif
			}
		}
	}

	session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_IN);
	session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_OT);

p_err:
	return ret;
}

int npu_session_NW_CMD_POWER_NOTIFY(struct npu_session *session, bool on)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_hw_device *hdev;
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	hdev = npu_get_hdev_by_id(session->hids);
	if (!hdev) {
		npu_err("no hwdevice found\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->imb_async_wq && !on) {
		cancel_delayed_work_sync(&session->imb_async_work);
	}
#endif

	if (atomic_read(&hdev->init_cnt.refcount) > 1) {
		mutex_unlock(session->global_lock);
		return ret;
	}
	dsp_dhcp_update_pwr_status(device, session->hids, on);

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	mutex_unlock(session->global_lock);

	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_session_restore_cnt(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;
	struct npu_hw_device *hdev_npu = npu_get_hdev_by_id(NPU_HWDEV_ID_NPU);

	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	atomic_xchg(&hdev_npu->init_cnt.refcount, sessionmgr->npu_hw_cnt_saved);
	atomic_xchg(&hdev_npu->init_cnt.refcount, sessionmgr->npu_hw_cnt_saved);

	mutex_unlock(session->global_lock);

	return ret;
}

int npu_session_NW_CMD_RESUME(struct npu_session *session)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	dsp_dhcp_update_pwr_status(device, session->hids, true);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	mutex_unlock(session->global_lock);
	return ret;
}

int npu_session_NW_CMD_SUSPEND(struct npu_session *session)
{
	int ret = 0;
	int cnt = 0;

	struct npu_sessionmgr *sessionmgr;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_hw_device *hdev_npu = npu_get_hdev_by_id(NPU_HWDEV_ID_NPU);
	struct npu_hw_device *hdev_dsp = npu_get_hdev_by_id(NPU_HWDEV_ID_DSP);
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	sessionmgr->npu_hw_cnt_saved = 0;
	sessionmgr->npu_hw_cnt_saved = atomic_xchg(&hdev_npu->init_cnt.refcount, sessionmgr->npu_hw_cnt_saved);
	sessionmgr->dsp_hw_cnt_saved = 0;
	sessionmgr->dsp_hw_cnt_saved = atomic_xchg(&hdev_dsp->init_cnt.refcount, sessionmgr->dsp_hw_cnt_saved);

	dsp_dhcp_update_pwr_status(device, session->hids, false);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	/* Waiting for FW to update the flag */
#define BOOT_TYPE_WARM_BOOT (0xb0080c0d)
	ret = -EFAULT;
	do {
		if (device->system.mbox_hdr->boottype == BOOT_TYPE_WARM_BOOT) {
			ret = 0;
			break;
		}
		msleep(1);
	} while (cnt++ < 18); /* Maximum 100 retries */
	if (!ret) {
		sessionmgr->npu_hw_cnt_saved = 0;
		sessionmgr->npu_hw_cnt_saved = atomic_xchg(&hdev_npu->init_cnt.refcount, sessionmgr->npu_hw_cnt_saved);
		sessionmgr->dsp_hw_cnt_saved = 0;
		sessionmgr->dsp_hw_cnt_saved = atomic_xchg(&hdev_dsp->init_cnt.refcount, sessionmgr->dsp_hw_cnt_saved);
	}

	mutex_unlock(session->global_lock);

	return ret;
}
#endif

static struct npu_memory_buffer *alloc_npu_load_payload(struct npu_session *session)
{
	struct npu_memory_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	/* 2 payloads.
	 * - User NCP
	 * - Kernel copy and updated NCP header.
	 */
	buffer->size = sizeof(struct cmd_load_payload) * 2;

	ret = npu_memory_alloc(session->memory, buffer, npu_get_configs(NPU_PBHA_HINT_00));
	if (ret) {
		npu_err("failed to allocate payload buffer(%d)\n", ret);
		goto err_free_buffer;
	}

	return buffer;
err_free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

int npu_session_NW_CMD_LOAD(struct npu_session *session)
{
	int ret = 0;
	nw_cmd_e nw_cmd = NPU_NW_CMD_LOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	if (session->hids & NPU_HWDEV_ID_NPU) {
		session->ncp_payload = alloc_npu_load_payload(session);
		if (IS_ERR_OR_NULL(session->ncp_payload)) {
			ret = PTR_ERR(session->ncp_payload);
			session->ncp_payload = NULL;
			return ret;
		}
	}

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_session_restart(void)
{
	npu_buffer_q_restart();
}
#endif

int npu_session_put_cancel_req(
	struct npu_session *session, struct npu_queue *queue,	struct npu_queue_list *incl, struct npu_queue_list *otcl,
	frame_cmd_e frame_cmd)
{
	int ret = 0;
	struct npu_sessionmgr *sess_mgr;
	struct npu_frame frame = {
		.uid = session->uid,
		.frame_id = incl->id,
		.result_code = 0,
		.session = session,
		.cmd = frame_cmd,
		.src_queue = queue,
		.input = incl,
		.output = otcl,
		.IFM_info = NULL,
		.OFM_info = NULL,
	};

	sess_mgr = session->cookie;
	mutex_lock(&sess_mgr->cancel_lock);
	ret = proto_drv_handler_frame_requested(&frame);
	if (unlikely(ret)) {
			npu_uerr("fail(%d) in proto_drv_handler_frame_requested\n", session, ret);
			ret = -EFAULT;
	}
	mutex_unlock(&sess_mgr->cancel_lock);

	return ret;
}

int npu_session_queue_cancel(struct npu_queue *queue, struct npu_queue_list *incl, struct npu_queue_list *otcl)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	frame_cmd_e frame_cmd = NPU_FRAME_CMD_Q_CANCEL;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	mutex_lock(session->global_lock);

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	ret = npu_session_put_cancel_req(session, queue, incl, otcl, frame_cmd);
	if (unlikely(ret))
		goto p_err;
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

p_err:
	mutex_unlock(session->global_lock);
	return ret;
}

int npu_session_queue(struct npu_queue *queue, struct npu_queue_list *incl, struct npu_queue_list *otcl)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	struct mbox_process_dat *mbox_process_dat;
	struct av_info *IFM_info;
	struct av_info *OFM_info;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	s64 now;
#endif

	frame_cmd_e frame_cmd = NPU_FRAME_CMD_Q;

	BUG_ON(!queue);
	BUG_ON(!incl);
	BUG_ON(!otcl);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	now = npu_get_time_us();
	/* 40% weightage to old value; and 60% to new computation */
	session->arrival_interval = ((4 * session->arrival_interval) + (6 * (now - session->last_qbuf_arrival)))/10;
	if(!session->tpf_requested) {
		session->tpf = (session->arrival_interval * 70)/100;
	}

	session->last_qbuf_arrival = now;
#endif

	session->qbuf_IOFM_idx = incl->index;

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	/* prepare sync timeline for out-fence */
	if (test_bit(VS4L_CL_FLAG_ENABLE_FENCE, &incl->flags)) {
		session->out_fence_value++;
		if (session->out_fence_fd[otcl->index] == -1) {
			ret = npu_sync_create_out_fence(session->out_fence, session->out_fence_value);
			if (ret <= 0) {
				npu_uerr("failed in npu_sync_create_out_fence\n", session);
				goto p_err;
			} else {
				session->out_fence_fd[otcl->index] = ret;
				session->in_fence_fd[incl->index] = incl->containers->buffers->reserved;
				session->in_fence = npu_sync_get_in_fence(session->in_fence_fd[incl->index]);
				if (session->in_fence == NULL) {
					npu_uerr("In fence is NULL, Please check sync pt\n", session);
					goto p_err;
				}
			}
		} else {
			session->in_fence_fd[incl->index] = incl->containers->buffers->reserved;
			ret = npu_sync_update_out_fence(session->out_fence, session->out_fence_value, session->out_fence_fd[otcl->index]);
			if (ret < 0) {
				npu_uerr("failed in npu_sync_update_out_fence\n", session);
				goto p_err;
			}
		}
	}

	if (session->out_fence_fd[otcl->index] != 0)
		otcl->containers->buffers->reserved = session->out_fence_fd[otcl->index];
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	if ((session->hids == NPU_HWDEV_ID_NPU) &&
	    (session->IMB_size > npu_get_configs(NPU_IMB_THRESHOLD_SIZE))) {
		npu_udbg("IMB: alloc in qbuf\n", session);
		ret = __ion_alloc_IMB(session, &session->IMB_info, session->IMB_mem_buf);
		if (unlikely(ret)) {
			npu_uerr("IMB: fail(%d) in ion alloc IMB\n", session, ret);
			goto p_err;
		}
		session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);

		npu_session_ion_sync_for_device(session->ncp_hdr_buf, DMA_TO_DEVICE);
	}
#endif

	IFM_info = &session->IFM_info;
	OFM_info = &session->OFM_info;
	IFM_info->IOFM_buf_idx = incl->index;
	OFM_info->IOFM_buf_idx = otcl->index;

	npu_udbg("uid %d, in id %d, idx %d, out id %d, idx %d\n", session,
			session->uid, incl->id, incl->index, otcl->id, otcl->index);

	mbox_process_dat = &session->mbox_process_dat;
	mbox_process_dat->address_vector_cnt = session->IOFM_cnt;
	mbox_process_dat->address_vector_start_daddr = (session->IOFM_mem_buf->daddr)
				+ ((session->qbuf_IOFM_idx * NPU_FRAME_MAX_BUFFER) * session->IOFM_cnt) * (sizeof(struct address_vector));

	ret = npu_session_put_frame_req(session, queue, incl, otcl, frame_cmd, IFM_info, OFM_info);
	if (unlikely(ret))
		goto p_err;

	IFM_info->state = SS_BUF_STATE_QUEUE;
	OFM_info->state = SS_BUF_STATE_QUEUE;

	return 0;
p_err:
	return ret;
}

int npu_session_deque(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	struct av_info *FM_info;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	if ((session->hids == NPU_HWDEV_ID_NPU) &&
	    (session->IMB_size > npu_get_configs(NPU_IMB_THRESHOLD_SIZE))) {
		npu_udbg("IMB: buffer free in dqbuf\n", session);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		/* deallocate of unnecessary memory chunks over the NPU_IMB_THRESHOLD_SIZE limit */
		__free_imb_mem_buf(session, false);
#else /* !CONFIG_NPU_USE_IMB_ALLOCATOR */
		npu_memory_free(session->memory, session->IMB_mem_buf);
#endif
	}
#endif

	FM_info = &session->IFM_info;
	FM_info->state = SS_BUF_STATE_DEQUEUE;

	FM_info = &session->OFM_info;
	FM_info->state = SS_BUF_STATE_DEQUEUE;

	npu_udbg("success\n", session);
	return ret;
}

int npu_session_update_iofm(struct npu_queue *queue, struct npu_queue_list *clist)
{
	int ret = 0;

	struct npu_vertex_ctx *vctx;
	struct npu_session *session;
	struct nq_buffer *buffer;
	struct av_info *FM_info;
	struct nq_container *container;
	struct address_vector *IOFM_av;
	struct npu_memory_buffer *IOFM_mem_buf;

	u32 FM_cnt, count;
	u32 i, j;
	u32 buf_cnt;
	u32 clindex;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	clindex = clist->index;
	IOFM_mem_buf = session->IOFM_mem_buf;
	IOFM_av = (struct address_vector *)(IOFM_mem_buf->vaddr);

	if (session->hids == NPU_HWDEV_ID_DSP && clist->direction == 1)
		count = clist->count - 1;
	else
		count = clist->count;

	for (i = 0; i < count ; i++) {
		container = &clist->containers[i];
		buf_cnt = clist->containers[i].count;

		for (j = 0; j < buf_cnt; j++) {
			npu_udbg("i %d/%d, j %d/%d\n", session, i, count, j, buf_cnt);
			buffer = &container->buffers[j];
			if (clist->direction == VS4L_DIRECTION_IN) {
				FM_info = &session->IFM_info;
				FM_cnt = session->IFM_cnt;
				(IOFM_av + ((clindex * NPU_FRAME_MAX_BUFFER + j) * session->IOFM_cnt) + i)->m_addr = buffer->daddr + buffer->roi.offset;
			} else {
				FM_info = &session->OFM_info;
				FM_cnt = session->OFM_cnt;
				(IOFM_av + ((clindex * NPU_FRAME_MAX_BUFFER + j) * session->IOFM_cnt) + i + session->IFM_cnt)->m_addr = buffer->daddr + buffer->roi.offset;
			}

			npu_udbg("clist %d, FM_cnt %d\n", session, count, FM_cnt);
			if (FM_cnt > count || FM_cnt + FM_SHARED_NUM < count) {
				npu_uerr("dir(%d), FM_cnt(%u) != count(%u)\n", session, clist->direction, FM_cnt, count);
				ret = -EINVAL;
				goto p_err;
			}
		}
		FM_info->state = SS_BUF_STATE_PREPARE;
	}

	return ret;

p_err:
	return ret;
}

int npu_session_prepare(struct npu_queue *queue, struct npu_queue_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	ret = npu_session_update_iofm(queue, clist);
	if (ret) {
		npu_err("fail(%d) in npu_session_update_iofm\n", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->IMB_size >= NPU_IMB_ASYNC_THRESHOLD) {
		npu_udbg("wait async allocation result:0x%x\n",
				session, session->imb_async_result_code);
		wait_event(session->imb_wq,
			(session->imb_async_result_code == NPU_IMB_ASYNC_RESULT_DONE) ||
			(session->imb_async_result_code == NPU_IMB_ASYNC_RESULT_ERROR));

		if (session->imb_async_wq) {
			destroy_workqueue(session->imb_async_wq);
			session->imb_async_wq = NULL;
		}

		if (session->imb_async_result_code == NPU_IMB_ASYNC_RESULT_ERROR) {
			npu_err("IMB: fail in ion alloc IMB async\n");
			ret = -EINVAL;
			goto p_err;
		}

		npu_udbg("wait async allocation done result:0x%x\n",
				session, session->imb_async_result_code);
	}
#endif
p_err:
	return ret;
}

int npu_session_unprepare(struct npu_queue *queue, struct npu_queue_list *clist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	struct av_info *FM_info;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (clist->direction == VS4L_DIRECTION_IN)
		FM_info = &session->IFM_info;
	else
		FM_info = &session->OFM_info;

	FM_info->state = SS_BUF_STATE_UNPREPARE;

	return ret;
}

#if !IS_ENABLED(CONFIG_DEBUG_FS)
static npu_s_param_ret fw_test_s_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	/* No OP */
	return S_PARAM_NOMB;
}
#endif

/* S_PARAM handler list definition - Chain of responsibility pattern */
typedef npu_s_param_ret(*fn_s_param_handler)(struct npu_session *, struct vs4l_param *);
static fn_s_param_handler s_param_handlers[] = {
	NULL,	/* NULL termination is required to denote EOL */
	npu_scheduler_param_handler,
	npu_qos_param_handler,
	fw_test_s_param_handler,
	npu_preference_param_handler,
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	dsp_kernel_param_handler,
#endif
};

int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist)
{
	npu_s_param_ret		ret = 0;
	struct vs4l_param	*param;
	u32			i, target;
	fn_s_param_handler	*p_fn;

	/* Search over each set param request over handlers */
	for (i = 0; i < plist->count; i++) {
		param = &plist->params[i];
		target = param->target;
		npu_info("Try set param [%u/%u] - target: [0x%x] - offset: [%d]\n",
			 i+1, plist->count, target, param->offset);

		p_fn = s_param_handlers;
		if ((target >= NPU_S_PARAM_PERF_MODE) &&
				(target < NPU_S_PARAM_SCH_PARAM_END)) {
			p_fn += 1;
		} else if ((target >= NPU_S_PARAM_QOS_NPU) &&
				(target < NPU_S_PARAM_QOS_PARAM_END)) {
			p_fn += 2;
		} else if ((target >= NPU_S_PARAM_FW_UTC_LOAD) &&
				(target < NPU_S_PARAM_FW_PARAM_END)) {
			p_fn += 3;
		} else if (target == NPU_S_PARAM_PREF_MODE) {
			p_fn += 4;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		} else if ((target >= NPU_S_PARAM_DSP_KERNEL) &&
				(target < NPU_S_PARAM_DSP_PARAM_END)) {
			p_fn += 5;
#endif
		} else {
			/* Continue process others */
			npu_uwarn("No handler defined for target [0x%x]."
				"Skipping.\n", session, param->target);
			continue;
		}

		ret = (*p_fn)(session, param);
		if (ret != S_PARAM_NOMB)
			npu_udbg("Handled by handler at [%pK](%d)\n",
						session, *p_fn, ret);
	}

	if (ret == S_PARAM_ERROR)
		return ret;

	return 0;
}

int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;

	int		retval = 0;
	u32		priority;
	u32		bound_id;
	u32		max_npu_core;

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	priority = param->priority;
	bound_id = param->bound_id;
	max_npu_core = session->sched_param.max_npu_core;
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev < 2))
		max_npu_core = 2;
#endif
	session->sched_param.deadline = param->deadline;
	/* Check priority range */
	if (priority <= NPU_PRIORITY_MAX_VAL)
		session->sched_param.priority = priority;
	else
		retval = -EINVAL;

	/* Check boundness to the core */
	if ((bound_id < max_npu_core) || (bound_id == NPU_BOUND_UNBOUND))
		session->sched_param.bound_id = bound_id;
	else
		retval = -EINVAL;

	/* Set the default value if anything is wrong */
	if (retval != 0) {
		session->sched_param.priority = NPU_PRIORITY_MID_PRIO;
		session->sched_param.bound_id = NPU_BOUND_UNBOUND;
		session->sched_param.deadline = NPU_MAX_DEADLINE;
	}

	npu_dbg("r[%d], p[%u], b[%u], c[%u]\n",
			retval, priority, bound_id, max_npu_core);
#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
	npu_scheduler_update_sched_param(device, session);
#endif
	return retval;
}

int npu_session_register_undo_cb(struct npu_session *session, session_cb cb)
{
	BUG_ON(!session);

	session->undo_cb = cb;

	return 0;
}

int npu_session_execute_undo_cb(struct npu_session *session)
{
	BUG_ON(!session);

	if (session->undo_cb)
		session->undo_cb(session);

	return 0;
}

int npu_session_undo_open(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (session->ss_state < BIT(NPU_SESSION_STATE_OPEN)) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_OPEN))
		goto session_free;

	npu_sessionmgr_unregID(session->cookie, session);

session_free:
	kfree(session);
	session = NULL;
p_err:
	return ret;
}

int npu_session_undo_s_graph(struct npu_session *session)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	if (session->imb_async_wq) {
		destroy_workqueue(session->imb_async_wq);
		session->imb_async_wq = NULL;
	}
#endif

	if (!_undo_s_graph_each_state(session)) {
		npu_session_register_undo_cb(session, NULL);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_undo_close(struct npu_session *session)
{
	int ret = 0;

	if (!_undo_close_each_state(session)) {
		npu_session_register_undo_cb(session, npu_session_undo_s_graph);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_flush(struct npu_session *session)
{
	int ret = 0;

	if (!session) {
		ret = -EINVAL;
		return ret;
	} else {
		struct vs4l_param	param;

		param.target = NPU_S_PARAM_PERF_MODE;
		param.offset = NPU_PERF_MODE_NORMAL;

#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
		if (session->imb_async_wq) {
			cancel_delayed_work_sync(&session->imb_async_work);
			destroy_workqueue(session->imb_async_wq);
			session->imb_async_wq = NULL;
		}
		session->imb_async_result_code = NPU_IMB_ASYNC_RESULT_SKIP;
#endif

		ret = npu_scheduler_param_handler(session, &param);
		if (ret == S_PARAM_ERROR)
			npu_err("fail(%d) in npu_session_param\n", ret);

		_undo_s_graph_each_state(session);
	}

	return 0;
}

int npu_session_profile_ready(struct npu_session *session, struct vs4l_profile_ready *pread)
{
	int ret = 0;
	struct npu_memory_buffer *profile_mem_buf = NULL;
	struct npu_memory_buffer *profile_mem_sub_buf = NULL;
	struct dsp_dhcp *dhcp;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	int uid;

	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
	dhcp = device->system.dhcp;
	uid = session->uid;

	profile_mem_sub_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (profile_mem_sub_buf == NULL) {
		npu_err("fail in npu_ion_map kzalloc\n");
		ret = -ENOMEM;
		goto p_err;
	}

	profile_mem_sub_buf->size = sizeof(int) * 4194304;
	ret = npu_memory_alloc(session->memory, profile_mem_sub_buf, npu_get_configs(NPU_PBHA_HINT_00));
	if (unlikely(ret)) {
		npu_uerr("npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}

	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_MODE, pread->mode);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_SUB_MAX_ITER0 + (session->uid * 3), pread->max_iter);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_SUB_SIZE0 + (session->uid * 3), profile_mem_sub_buf->size);
	dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_SUB_IOVA0 + (session->uid * 3), profile_mem_sub_buf->daddr);

	if (pread->fd > 0) {
		if (pread->offset >= pread->size) {
			npu_err("please check profile offset & size\n");
			ret = -EINVAL;
			goto p_err;
		}

		profile_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
		if (profile_mem_buf == NULL) {
			npu_err("fail in npu_ion_map kzalloc\n");
			ret = -ENOMEM;
			goto p_err;
		}

		profile_mem_buf->m.fd = pread->fd;
		profile_mem_buf->size = pread->size;

		ret = npu_memory_map(session->memory, profile_mem_buf, npu_get_configs(NPU_PBHA_HINT_11),
										session->lazy_unmap_disable);
		if (unlikely(ret)) {
			npu_uerr("npu_memory_map is fail(%d).\n", session, ret);
			kfree(profile_mem_buf);
			goto p_err;
		}

		memset(profile_mem_buf->vaddr, 0, profile_mem_buf->size);
		dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_MAIN_MAX_ITER0 + (session->uid * 3), pread->max_iter);
		dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_MAIN_SIZE0 + (session->uid * 3), pread->size - pread->offset);
		dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_PROFILE_MAIN_IOVA0 + (session->uid * 3), profile_mem_buf->daddr + pread->offset);
		if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE)
			uid = 0;
	}

	npu_profile_init(profile_mem_buf, profile_mem_sub_buf, pread, device->sched->mode, uid);

p_err:
	return ret;
}
