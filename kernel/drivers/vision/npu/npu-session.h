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

#ifndef _NPU_SESSION_H_
#define _NPU_SESSION_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include "vs4l.h"

#include "npu-vertex.h"
#include "npu-queue.h"
#include "npu-config.h"
#include "npu-common.h"
#include "npu-scheduler.h"
#include "npu-if-session-protodrv.h"
#include "ncp_header.h"
#include "drv_usr_if.h"
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "dsp-common-type.h"
#include "dsp-common.h"
#endif
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
#include "npu-fence.h"
#endif

#define FM_SHARED_NUM	1
#define MAX_USER_KERNEL 2
#define NPU_SESSION_DSP_MAGIC	0xC0DEC0DE
#define NPU_SESSION_LAZY_UNMAP_ENABLE	0x0
#define NPU_SESSION_LAZY_UNMAP_DISABLE	0x531F

enum channel_flag {
	MEDIUM_CHANNEL = 0,
	HIGH_CHANNEL = 1,
};

enum npu_session_state {
	NPU_SESSION_STATE_OPEN,
	NPU_SESSION_STATE_REGISTER,
	NPU_SESSION_STATE_GRAPH_ION_MAP,
	NPU_SESSION_STATE_WGT_KALLOC,
	NPU_SESSION_STATE_IOFM_KALLOC,
	NPU_SESSION_STATE_IOFM_ION_ALLOC,
	NPU_SESSION_STATE_IMB_ION_ALLOC,
	NPU_SESSION_STATE_FORMAT_IN,
	NPU_SESSION_STATE_FORMAT_OT,
	NPU_SESSION_STATE_START,
	NPU_SESSION_STATE_STOP,
	NPU_SESSION_STATE_CLOSE,
};

struct imb_alloc_info;
enum npu_imb_async_result {
	NPU_IMB_ASYNC_RESULT_SKIP = 0x4896,
	NPU_IMB_ASYNC_RESULT_START,
	NPU_IMB_ASYNC_RESULT_DONE,
	NPU_IMB_ASYNC_RESULT_ERROR,
};

struct ion_info {
	dma_addr_t		daddr;
	void				*vaddr;
	size_t			size;
};

struct ncp_info {
	u32 address_vector_cnt;
	struct addr_info ncp_addr;
};

#define NPU_Q_TIMEDIFF_WIN_MAX 5

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#define MAX_FREQ_FRAME_NUM 5
#if IS_ENABLED(CONFIG_SOC_S5E9945)
#define MAX_FREQ_LEVELS 8
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#define MAX_FREQ_LEVELS 6
#endif

struct npu_execution_info {
	u32 time[MAX_FREQ_FRAME_NUM];
	u32 cur;
	s64 x;
	s64 z;
};

struct kkt_param_info {
	s64 x2;
	s64 yz;
	s64 xy;
	s64 xz;
	s64 z2;
};

struct npu_model_info_hash {
	s64 alpha;
	s64 beta;
	u32 miss_cnt;

	char model_name[NCP_MODEL_NAME_LEN];
	u32 computational_workload;
	u32 io_workload;

	struct npu_execution_info exec[MAX_FREQ_LEVELS];
	struct kkt_param_info kkt_param;

	struct hlist_node hlist;
};
#endif

struct npu_session {
	u32 uid;
	u32 frame_id;
	u32 unified_id;
	u64 unified_op_id;
	void *cookie;
	void *memory;
	struct npu_vertex_ctx vctx;
	int (*undo_cb)(struct npu_session *);

	unsigned long ss_state;
	u32 hids;
	u32 IFM_cnt;
	u32 OFM_cnt;
	u32 IMB_cnt;
	u32 WGT_cnt;
	u32 IOFM_cnt;
	size_t IMB_size;
	u32 preference;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct list_head active_session;
	s64 last_qbuf_arrival;
	s64 arrival_interval;
	bool is_control_dsp;
	bool is_control_npu;
	bool check_core;

	bool tpf_requested;
	struct npu_model_info_hash *model_hash_node;
	u32 tpf;

	s32 tune_npu_level;
	s32 predict_error[MAX_FREQ_FRAME_NUM];
	s32 predict_error_idx;

	s64 earliest_qbuf_time;
	s64 prev_npu_level;
	s32 next_mif_level;
#endif
	struct ncp_info ncp_info;

	// dynamic allocation, free is required, for ion alloc memory
	struct npu_memory_buffer *ncp_mem_buf;
	struct npu_memory_buffer *ncp_hdr_buf;
	struct npu_memory_buffer *ncp_payload;
	unsigned long ncp_mem_buf_offset;
	struct npu_memory_buffer *shared_mem_buf;
	struct npu_memory_buffer *IOFM_mem_buf; // NPU_FRAME_MAX_CONTAINERLIST * NPU_FRAME_MAX_BUFFER
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	struct npu_memory_buffer *IMB_mem_buf; // IMB_cnt
#endif
	struct npu_memory_buffer *weight_mem_buf;

	// kmalloc, not ion alloc
	struct av_info IFM_info;
	struct av_info OFM_info;
	struct addr_info *IMB_info;
	struct addr_info *WGT_info;

	int qbuf_IOFM_idx; // IOFM Buffer index
	int dqbuf_IOFM_idx; // IOFM Buffer index

	struct npu_nw_sched_param sched_param;/* For NW priority and boundness */

	struct nw_result nw_result;
	wait_queue_head_t wq;

	struct npu_frame *current_frame;
	struct mutex *global_lock;

	struct mbox_process_dat mbox_process_dat;

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	struct npu_sync_timeline *in_fence;
	struct npu_sync_timeline *out_fence;
	int out_fence_exe_value;
	int out_fence_value;
	unsigned long out_fence_fd[NPU_FRAME_MAX_CONTAINERLIST];
	unsigned long in_fence_fd[NPU_FRAME_MAX_CONTAINERLIST];
#endif
	pid_t	pid;
	char	comm[TASK_COMM_LEN];
	pid_t	p_pid;
	char	p_comm[TASK_COMM_LEN];

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	u32 llc_ways;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	/* DSP kernel */
	unsigned int			kernel_count;
	unsigned int			kernel_param_size;
	struct list_head		kernel_list;
	void				*kernel_name;
	struct dsp_dl_lib_info		*dl_libs;
	bool				kernel_loaded;
	struct list_head update_list;
	u32 update_count;
	struct list_head buf_list;
	u32 buf_count;
	u32 global_id;
	struct dsp_common_mem_v4 buf_info[NPU_FRAME_MAX_BUFFER];
	struct dsp_common_execute_info_v4 *exe_info;
	u32 user_kernel_count;
	u32 dl_unique_id;
	void *user_kernel_elf[MAX_USER_KERNEL];
	u32 user_kernel_elf_size[MAX_USER_KERNEL];
	struct string_manager str_manager;
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
	struct npu_scheduler_dvfs_sess_info *dvfs_info;
#endif
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
	struct delayed_work 			imb_async_work;
	wait_queue_head_t		imb_wq;
	int 	imb_async_result_code;
	struct workqueue_struct 		*imb_async_wq;
	struct addr_info **imb_av;
#endif
	u32 lazy_unmap_disable;
	u32 key;
	char model_name[NCP_MODEL_NAME_LEN];
	u32 computational_workload;
	u32 io_workload;
	u32 featuremapdata_type;
	bool is_instance_1;
};

typedef int (*session_cb)(struct npu_session *);

struct npu_session_ops {
	int (*control)(struct npu_session *, struct npu_frame *);
	int (*request)(struct npu_session *, struct npu_frame *);
	int (*process)(struct npu_session *, struct npu_frame *);
	int (*cancel)(struct npu_session *, struct npu_frame *);
	int (*done)(struct npu_session *, struct npu_frame *);
	int (*get_resource)(struct npu_session *, struct npu_frame *);
	int (*put_resource)(struct npu_session *, struct npu_frame *);
	int (*update_param)(struct npu_session *, struct npu_frame *);
};

npu_errno_t chk_nw_result_no_error(struct npu_session *session);
int npu_session_open(struct npu_session **session, void *cookie, void *memory);
int npu_session_close(struct npu_session *session);
int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info);
int npu_session_format(struct npu_queue *queue, struct vs4l_format_bundle *fbundle);
int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist);
int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param);
int npu_session_prepare(struct npu_queue *q, struct npu_queue_list *clist);
int npu_session_unprepare(struct npu_queue *q, struct npu_queue_list *clist);
int npu_session_update_iofm(struct npu_queue *queue, struct npu_queue_list *clist);
int npu_session_queue(struct npu_queue *queue, struct npu_queue_list *incl, struct npu_queue_list *otcl);
int npu_session_deque(struct npu_queue *queue);
void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, enum dma_data_direction dir);
int npu_session_NW_CMD_LOAD(struct npu_session *session);
int npu_session_NW_CMD_UNLOAD(struct npu_session *session);
int npu_session_NW_CMD_STREAMOFF(struct npu_session *session);
int npu_session_NW_CMD_SUSPEND(struct npu_session *session);
int npu_session_NW_CMD_RESUME(struct npu_session *session);
int npu_session_NW_CMD_POWER_NOTIFY(struct npu_session *session, bool on);
int npu_session_register_undo_cb(struct npu_session *session, session_cb cb);
int npu_session_execute_undo_cb(struct npu_session *session);
int npu_session_undo_open(struct npu_session *session);
int npu_session_undo_s_graph(struct npu_session *session);
int npu_session_undo_close(struct npu_session *session);
int npu_session_flush(struct npu_session *session);
int npu_session_profile_ready(struct npu_session *session, struct vs4l_profile_ready *pread);
int kpi_frame_mbox_put(struct npu_frame *frame);
int npu_session_restore_cnt(struct npu_session *session);
void npu_session_restart(void);
struct nq_buffer *get_buffer_of_execution_info(struct npu_frame *frame);
struct dsp_common_execute_info_v4 *get_execution_info_for_dsp(struct npu_frame *frame);
int npu_session_queue_cancel(struct npu_queue *queue, struct npu_queue_list *incl, struct npu_queue_list *otcl);
int npu_session_put_nw_req(struct npu_session *session, nw_cmd_e nw_cmd);

#ifdef CONFIG_NPU_KUNIT_TEST
#define dsp_exec_graph_info(x) do {} while(0)
#define npu_session_dump(x) do {} while(0)
#else
void dsp_exec_graph_info(struct dsp_common_execute_info_v4 *info);
void npu_session_dump(struct npu_session *session);
#endif
#if IS_ENABLED(CONFIG_NPU_IMB_ASYNC_ALLOC)
int npu_session_imb_async_alloc_init(struct npu_session *session);
int npu_session_imb_async_init(struct npu_session *session);
#else	/* CONFIG_NPU_IMB_ASYNC_ALLOC is not defined */
#define npu_session_imb_async_alloc_init(p)	(0)
#define npu_session_imb_async_init(p)	(0)
#endif
#endif
