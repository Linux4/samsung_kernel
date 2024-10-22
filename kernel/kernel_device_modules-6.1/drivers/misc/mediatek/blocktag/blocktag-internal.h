/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _MTK_BLOCKTAG_INTERNAL_H
#define _MTK_BLOCKTAG_INTERNAL_H

#ifndef pr_fmt
#define pr_fmt(fmt) "[blocktag]" fmt
#endif

#include <linux/blk_types.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include "mtk_blocktag.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

/*
 * MTK_BTAG_FEATURE_MICTX_IOSTAT
 *
 * Shall be defined if we can provide iostat
 * produced by mini context.
 *
 * This feature is used to extend kernel
 * trace events to have more I/O information.
 */
#define MTK_BTAG_FEATURE_MICTX_IOSTAT

#define BTAG_PIDLOG_ENTRIES	50
#define BTAG_PRINT_LEN		4096
#define BTAG_MAX_TAG		512

#define BTAG_RT(btag)	  (btag ? &btag->rt : NULL)
#define BTAG_CTX(btag)	  (btag ? btag->ctx.priv : NULL)

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define BTAG_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if ((buff) && (size) && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!(buff) && !(evt)) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

#if IS_ENABLED(CONFIG_MTK_USE_RESERVED_EXT_MEM)
extern void *extmem_malloc_page_align(size_t bytes);
#endif

enum mtk_btag_io_type {
	BTAG_IO_READ = 0,
	BTAG_IO_WRITE,
	BTAG_IO_TYPE_NR,
	BTAG_IO_UNKNOWN
};

struct page_pidlogger {
	__s16 pid;
};

struct mtk_btag_proc_pidlogger_entry {
	__u16 pid;
	__u32 cnt[BTAG_IO_TYPE_NR];
	__u32 len[BTAG_IO_TYPE_NR];
};

struct mtk_btag_proc_pidlogger {
	spinlock_t lock;
	struct mtk_btag_proc_pidlogger_entry info[BTAG_PIDLOG_ENTRIES];
};

struct mtk_btag_workload {
	__u64 period;  /* period time (ns) */
	__u64 usage;   /* busy time (ns) */
	__u32 percent; /* workload */
	__u32 count;   /* access count */
};

struct mtk_btag_throughput {
	__u64 usage;  /* busy time (ns) */
	__u32 size;   /* transferred bytes */
	__u32 speed;  /* KB/s */
};

struct mtk_btag_req {
	__u16 count;
	__u64 size; /* bytes */
	__u64 size_top; /* bytes */
};

/*
 * mini context for integration with
 * other performance analysis tools.
 */
struct mtk_btag_mictx_queue {
	spinlock_t lock;
	__u64 rq_size[BTAG_IO_TYPE_NR];
	__u64 rq_cnt[BTAG_IO_TYPE_NR];
	__u64 top_len;
	__u64 tp_size[BTAG_IO_TYPE_NR];
	__u64 tp_time[BTAG_IO_TYPE_NR];
};

struct mtk_btag_mictx {
	struct list_head list;
	struct mtk_btag_mictx_queue *q;
	struct __mictx_workload {
		spinlock_t lock;
		__u64 idle_begin;
		__u64 idle_total;
		__u64 window_begin;
		__u16 depth;
	} wl;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	struct __mictx_average_queue_depth {
		spinlock_t lock;
		__u64 latency;
		__u64 last_depth_chg;
		__u16 depth;
	} avg_qd;
#endif
	struct __mictx_tag {
		__u64 start_t;
		enum mtk_btag_io_type io_type;
		__u32 len;
	} tags[BTAG_MAX_TAG];
	__u16 queue_nr;
	__s8 id;
	bool full_logging;
};

#define EARAIO_EARLY_NOTIFY
#define THRESHOLD_MAX	0x7fffffff

#define EARAIO_BOOST_ENTRY_LEN 8
#define EARAIO_BOOST_ENTRY_NUM 4
#define EARAIO_BOOST_BUF_SZ (EARAIO_BOOST_ENTRY_LEN * EARAIO_BOOST_ENTRY_NUM)

struct mtk_btag_earaio_control {
	spinlock_t lock;
	bool enabled;	/* can send boost to earaio */
	bool start_collect;

	struct mtk_btag_mictx_id mictx_id;

	/* peeking window */
	__u64 pwd_begin;
	__u32 pwd_top_pages[BTAG_IO_TYPE_NR];

	bool boosted;

	struct proc_dir_entry *earaio_boost_entry;
	char msg_buf[EARAIO_BOOST_BUF_SZ];
	int msg_buf_start_idx;
	int msg_buf_used_entry;
	int msg_open_cnt;
	wait_queue_head_t msg_readable;

	int earaio_boost_state;
#ifdef EARAIO_EARLY_NOTIFY
	int rand_req_cnt;
	int rand_rw_threshold;
	int seq_r_threshold; /* in # of pages */
	int seq_w_threshold; /* in # of pages */
#endif
};

struct mtk_btag_vmstat {
	__u64 file_pages;
	__u64 file_dirty;
	__u64 dirtied;
	__u64 writeback;
	__u64 written;
	__u64 fmflt;
};

struct mtk_btag_cpu {
	__u64 user;
	__u64 nice;
	__u64 system;
	__u64 idle;
	__u64 iowait;
	__u64 irq;
	__u64 softirq;
};

/* Trace: entry of the ring buffer */
struct mtk_btag_trace {
	__u64 time;
	__u32 qid;
	__s16 pid;
	struct mtk_btag_workload workload;
	struct mtk_btag_throughput throughput[BTAG_IO_TYPE_NR];
	struct mtk_btag_vmstat vmstat;
	struct mtk_btag_proc_pidlogger pidlog;
	struct mtk_btag_cpu cpu;
};

/* Ring Trace */
struct mtk_btag_ringtrace {
	struct mtk_btag_trace *trace;
	spinlock_t lock;
	__s32 index;
	__s32 max;
};

struct mtk_btag_vops {
	size_t	(*seq_show)(char **buff, unsigned long *size,
			    struct seq_file *seq);
	ssize_t  (*sub_write)(const char __user *ubuf, size_t count);
	bool	boot_device;
	bool	earaio_enabled;
};

/* BlockTag */
struct mtk_blocktag {
	struct list_head list;
	char name[BTAG_NAME_LEN];
	enum mtk_btag_storage_type storage_type;
	bool ctx_enable;
	struct mtk_btag_ringtrace rt;

	struct context_t {
		void *priv;
		__u32 count;
		__u32 size;
		struct mictx_t {
			struct list_head list;
			spinlock_t list_lock;
			__u16 nr_list;
			__u16 last_unused_id;
		} mictx;
	} ctx;

	struct dentry_t {
		struct proc_dir_entry *droot;
		struct proc_dir_entry *dlog;
	} dentry;

	struct mtk_btag_vops *vops;
};

struct mtk_blocktag *mtk_btag_find_by_type(
	enum mtk_btag_storage_type storage_type);
short mtk_btag_page_pidlog_get(struct page *p);
void mtk_btag_page_pidlog_set(struct page *p, short pid);
void mtk_btag_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog,
			    __u16 *insert_pid, __u32 *insert_len,
			    __u32 insert_cnt, enum mtk_btag_io_type io_type);
void mtk_btag_vmstat_eval(struct mtk_btag_vmstat *vm);
void mtk_btag_pidlog_eval(
	struct mtk_btag_proc_pidlogger *pl,
	struct mtk_btag_proc_pidlogger *ctx_pl);
void mtk_btag_cpu_eval(struct mtk_btag_cpu *cpu);
void mtk_btag_throughput_eval(struct mtk_btag_throughput *tp);

struct mtk_btag_trace *mtk_btag_curr_trace(struct mtk_btag_ringtrace *rt);
struct mtk_btag_trace *mtk_btag_next_trace(struct mtk_btag_ringtrace *rt);

struct mtk_blocktag *mtk_btag_alloc(
	const char *name, enum mtk_btag_storage_type storage_type,
	__u32 ringtrace_count, size_t ctx_size, __u32 ctx_count,
	struct mtk_btag_vops *vops);
void mtk_btag_free(struct mtk_blocktag *btag);

void mtk_btag_get_aee_buffer(unsigned long *vaddr, unsigned long *size);

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id, bool force);
void mtk_btag_mictx_send_command(struct mtk_blocktag *btag, __u64 start_t,
				 enum mtk_btag_io_type io_type, __u64 tot_len,
				 __u64 top_len, __u32 tid, __u16 qid);
void mtk_btag_mictx_complete_command(struct mtk_blocktag *btag, __u64 end_t,
				     __u32 tid, __u16 qid);
void mtk_btag_mictx_set_full_logging(struct mtk_btag_mictx_id mictx_id,
				     bool enable);
int mtk_btag_mictx_full_logging(struct mtk_btag_mictx_id mictx_id);
void mtk_btag_mictx_free_all(struct mtk_blocktag *btag);
void mtk_btag_mictx_init(struct mtk_blocktag *btag);

void mtk_btag_earaio_clear_data(void);
void mtk_btag_earaio_init_mictx(
	struct mtk_btag_vops *vops,
	enum mtk_btag_storage_type storage_type,
	struct proc_dir_entry *btag_proc_root);
void mtk_btag_earaio_update_pwd(enum mtk_btag_io_type type, __u32 size);

/* seq file operations */
void *mtk_btag_seq_debug_start(struct seq_file *seq, loff_t *pos);
void *mtk_btag_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos);
void mtk_btag_seq_debug_stop(struct seq_file *seq, void *v);

#endif
#endif
