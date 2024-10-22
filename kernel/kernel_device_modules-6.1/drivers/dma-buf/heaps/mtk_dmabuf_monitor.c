// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * It can record information such as dmabuf alloc time and pool size,
 * and then output it through cat /proc/dma_heap/monitor.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mtk_heap.h"
#include "mtk_heap_priv.h"
#include "mtk_page_pool.h"

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)

#define MONITOR_MAX_LOG_COUNT 8192  //256kb

static mtk_refill_order_cb refill_order_callback;

enum {
	DMABUF_LOG_ALLOC_TIME,
	DMABUF_LOG_ALLOCATE,
	DMABUF_LOG_FREE,
	DMABUF_LOG_REFILL,
	DMABUF_LOG_PREFILL,
	DMABUF_LOG_RECYCLE,
	DMABUF_LOG_SHRINK,
	DMABUF_LOG_POOL_SIZE,
	DMABUF_LOG_NUM,
};

struct  monitor_log_t {
	u64 time_tick;
	u64 log_info;
	u64 data1;
	u64 data2;
};

#define MONITOR_LOG_TYPE_GET(logvlu)		((logvlu) & 0xF)
#define MONITOR_LOG_TYPE_SET(logvlu, type)	((logvlu) |= (type) & 0xF)

#define MONITOR_LOG_HEAP_GET(logvlu)		(((logvlu) & 0xF0) >> 4)
#define MONITOR_LOG_HEAP_SET(logvlu, heap)	((logvlu) |= (((heap) << 4) & 0xF0))

#define MONITOR_LOG_TIME_GET(logvlu)		((logvlu) >> 8)
#define MONITOR_LOG_TIME_SET(logvlu, tm)		((logvlu) |= ((tm) << 8))


#define MONITOR_FILL_RET_GET(data)		((int)((data) & 0xF))
#define MONITOR_FILL_RET_SET(data, ret)		((data) |= (ret) & 0xF)

#define MONITOR_FILL_ORD_GET(data)		(((data) & 0xF0) >> 4)
#define MONITOR_FILL_ORD_SET(data, order)	((data) |= (((order) << 4) & 0xF0))

#define MONITOR_FILL_MEMFREE_GET(data)		(((data) & 0xFFFF00) >> 8)
#define MONITOR_FILL_MEMFREE_SET(data, free)	((data) |= (((free) << 8) & 0xFFFF00))

#define MONITOR_FILL_MEMAVAIL_GET(data)		((data) >> 24)
#define MONITOR_FILL_MEMAVAIL_SET(data, avail)	((data) |= ((avail) << 24))

struct monitor_log_stats_alloc {
	u64 tm;
	u64 size;
	u64 nent;
};

struct monitor_log_stats_refill {
	u64 tm;
	u32 page_fill;
	u32 page_fill8;
	u32 page_fill4;
	int ret_0;
	int ret_fill;
};

struct  monitor_log_stats_t {
	u32 count;

	union {
		struct monitor_log_stats_alloc alloc;
		struct monitor_log_stats_refill refill;
	};
};

struct  monitor_log_stats_common_t {
	u32 count;

	union {
		struct monitor_log_stats_alloc alloc;
		struct monitor_log_stats_refill refill;
	};
};


struct monitor_global_t {
	unsigned int enable;
	unsigned int start;
	atomic_t write_pointer;
	//spinlock_t	lock;
	struct monitor_log_t *record;

	unsigned int dump_kernel;
	unsigned int dump_detail;
	unsigned int dump_help;
	unsigned int dump_pool;

	unsigned int log_pool_interval;
	unsigned int log_alloc_time;
	unsigned int log_allocate;
	unsigned int log_free;
	unsigned int log_refill;
	unsigned int log_prefill;
	unsigned int log_recycle;
	unsigned int log_shrink;
};
static struct monitor_global_t monitor_globals;

#define DMA_MONITOR_CMDLINE_LEN      (30)

struct dma_monitor_dbg {
	const char *str;
	int *flag;
	int cmd_type;
};

enum {
	MONITOR_CMD_NULL,
	MONITOR_CMD_CLEAR_LOG,
	MONITOR_CMD_MTKMM_PREFILL,
	MONITOR_CMD_REFILL_HIGH,
	MONITOR_CMD_REFILL_LOW,
	MONITOR_CMD_RECYCLE_HIGH,
	MONITOR_CMD_RECYCLE_LOW,
	MONITOR_CMD_SHRINK_ENABLE,
	MONITOR_CMD_POOL_TIME,
	MONITOR_CMD_NUM,
};

struct dump_heap_info {
	const char *heap_name;
	int heap_tag;
	struct dma_heap *heap;
};

enum {
	HEAP_TAG_NULL,
	HEAP_TAG_MTK_MM,
	HEAP_TAG_MTK_MM_UNCACHED,
	HEAP_TAG_MTK_CAMERA,
	HEAP_TAG_MTK_CAMERA_UNCACHED,
	HEAP_TAG_SYSTEM,
	HEAP_TAG_SYSTEM_UNCACHED,
	HEAP_TAG_MTK_SLC,
	HEAP_TAG_NUM,
};

static struct dump_heap_info dump_heap_list[] = {
	{"mtk_mm", HEAP_TAG_MTK_MM, NULL},
	{"mtk_mm-uncached", HEAP_TAG_MTK_MM_UNCACHED, NULL},
	//{"mtk_camera", HEAP_TAG_MTK_CAMERA, NULL},
	//{"mtk_camera-uncached", HEAP_TAG_MTK_CAMERA_UNCACHED, NULL},
	{"system", HEAP_TAG_SYSTEM, NULL},
	{"system-uncached", HEAP_TAG_SYSTEM_UNCACHED, NULL},
};
#define _DUMP_HEAP_CNT_  (ARRAY_SIZE(dump_heap_list))

static int cmd_input_tmp;
static const struct dma_monitor_dbg monitor_helper[] = {
	{"log_alloc_time:", &monitor_globals.log_alloc_time, MONITOR_CMD_NULL},
	{"log_allocate:", &monitor_globals.log_allocate, MONITOR_CMD_NULL},
	{"log_free:", &monitor_globals.log_free, MONITOR_CMD_NULL},
	{"log_refill:", &monitor_globals.log_refill, MONITOR_CMD_NULL},
	{"log_prefill:", &monitor_globals.log_prefill, MONITOR_CMD_NULL},
	{"log_recycle:", &monitor_globals.log_recycle, MONITOR_CMD_NULL},
	{"log_shrink:", &monitor_globals.log_shrink, MONITOR_CMD_NULL},
	{"log_pool_interval:", &monitor_globals.log_pool_interval, MONITOR_CMD_POOL_TIME},

	{"dump_kernel:", &monitor_globals.dump_kernel, MONITOR_CMD_NULL},
	{"dump_detail:", &monitor_globals.dump_detail, MONITOR_CMD_NULL},
	{"dump_help:", &monitor_globals.dump_help, MONITOR_CMD_NULL},
	{"dump_pool:", &monitor_globals.dump_pool, MONITOR_CMD_NULL},

	{"refill_high_water8:", &cmd_input_tmp, MONITOR_CMD_REFILL_HIGH},
	{"refill_high_water4:", &cmd_input_tmp, MONITOR_CMD_REFILL_HIGH},
	{"refill_high_water0:", &cmd_input_tmp, MONITOR_CMD_REFILL_HIGH},
	{"clear_log:", &cmd_input_tmp, MONITOR_CMD_CLEAR_LOG},
	{"mtk_mm_prefill:", &cmd_input_tmp, MONITOR_CMD_MTKMM_PREFILL},
};

void heap_monitor_init(void)
{
	int total_size = MONITOR_MAX_LOG_COUNT * sizeof(struct monitor_log_t);
	struct mtk_heap_priv_info *heap_priv;
	struct mtk_dmabuf_page_pool **pool;
	int i, j;

	monitor_globals.record = vmalloc(total_size);
	if (!monitor_globals.record) {
		monitor_globals.enable = 0;
		return;
	}

	pr_info("%s:%d find heap\n", __func__, __LINE__);
	for (i = 0; i < _DUMP_HEAP_CNT_; i++) {
		dump_heap_list[i].heap = dma_heap_find(dump_heap_list[i].heap_name);
		if (!dump_heap_list[i].heap)
			continue;

		pr_info("%s:%d find heap name:%s, tag:%d\n", __func__, __LINE__,
			dump_heap_list[i].heap_name, dump_heap_list[i].heap_tag);
		heap_priv = dma_heap_get_drvdata(dump_heap_list[i].heap);
		pool = heap_priv ? heap_priv->page_pools : NULL;
		if (!pool || heap_priv->uncached)
			continue;

		pr_info("%s:%d set heap tag\n", __func__, __LINE__);
		for (j = 0; j < NUM_ORDERS; j++)
			pool[j]->heap_tag = dump_heap_list[i].heap_tag;
	}

	//memset(monitor_globals.record, 0, total_size);
	//atomic_set(&monitor_globals.write_pointer, 0);
	//spin_lock_init(&monitor_globals.lock);

	//monitor_globals.log_alloc_time = 1;
	//monitor_globals.log_allocate = 1;
	//monitor_globals.log_refill = 1;
	//monitor_globals.log_prefill = 1;
	//monitor_globals.log_recycle = 1;
	//monitor_globals.log_shrink = 1;

	monitor_globals.log_pool_interval = 0;
	monitor_globals.dump_kernel = 0;
	monitor_globals.dump_detail = 1;
	monitor_globals.dump_help = 1;
	monitor_globals.dump_pool = 1;

	monitor_globals.enable = 1;
}
EXPORT_SYMBOL_GPL(heap_monitor_init);

void heap_monitor_exit(void)
{
	int i;

	monitor_globals.enable = 0;

	for (i = 0; i < _DUMP_HEAP_CNT_; i++) {
		if (dump_heap_list[i].heap) {
			dma_heap_put(dump_heap_list[i].heap);
			dump_heap_list[i].heap = NULL;
		}
	}

	if (monitor_globals.record) {
		vfree(monitor_globals.record);
		monitor_globals.record = NULL;
	}
}
EXPORT_SYMBOL_GPL(heap_monitor_exit);

ssize_t heap_monitor_proc_write(struct file *file, const char *buf,
				       size_t count, loff_t *data)
{
	char cmdline[DMA_MONITOR_CMDLINE_LEN];
	int i = 0, vlu = 0;
	int helper_len = 0;
	const char *helper_str = NULL;

	if (count >= DMA_MONITOR_CMDLINE_LEN)
		return count;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	pr_info("%s #%d: set info:%s", __func__, __LINE__, cmdline);

	for (i = 0; i < ARRAY_SIZE(monitor_helper); i++) {
		helper_len = strlen(monitor_helper[i].str);
		helper_str = monitor_helper[i].str;

		if (!strncmp(cmdline, helper_str, helper_len)) {
			if (!strncmp(cmdline + helper_len, "on", 2)) {
				*monitor_helper[i].flag = 1;
				pr_info("%s set as 1\n", helper_str);
			} else if (!strncmp(cmdline + helper_len, "off", 3)) {
				*monitor_helper[i].flag = 0;
				pr_info("%s set as 0\n", helper_str);
			} else {
				if (monitor_helper[i].cmd_type == MONITOR_CMD_REFILL_HIGH &&
					refill_order_callback &&
					sscanf(cmdline, "refill_high_water8:%u\n", &vlu) == 1)
					refill_order_callback(0, vlu);
				else if (monitor_helper[i].cmd_type == MONITOR_CMD_REFILL_HIGH &&
					 refill_order_callback &&
					 sscanf(cmdline, "refill_high_water4:%u\n", &vlu) == 1)
					refill_order_callback(1, vlu);
				else if (monitor_helper[i].cmd_type == MONITOR_CMD_REFILL_HIGH &&
					 refill_order_callback &&
					 sscanf(cmdline, "refill_high_water0:%u\n", &vlu) == 1)
					refill_order_callback(2, vlu);
				else if (monitor_helper[i].cmd_type == MONITOR_CMD_POOL_TIME &&
					 sscanf(cmdline, "log_pool_interval:%u\n", &vlu) == 1)
					monitor_globals.log_pool_interval = vlu;
				else if (monitor_helper[i].cmd_type == MONITOR_CMD_MTKMM_PREFILL &&
					 sscanf(cmdline, "mtk_mm_prefill:%u\n", &vlu) == 1)
					dma_heap_pool_prefill(vlu * SZ_1M, "mtk_mm");
				break;
			}

			if (monitor_helper[i].cmd_type == MONITOR_CMD_CLEAR_LOG &&
				*monitor_helper[i].flag == 1) {
				atomic_set(&monitor_globals.write_pointer, 0);
				*monitor_helper[i].flag = 0;
			}
		}
	}

	return count;
}
EXPORT_SYMBOL_GPL(heap_monitor_proc_write);

static int heap_monitor_proc_show(struct seq_file *s, void *v);

int heap_monitor_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, heap_monitor_proc_show, pde_data(inode));
}
EXPORT_SYMBOL_GPL(heap_monitor_proc_open);

int get_mtk_heap_tag(struct dma_heap *heap)
{
	int i;

	for (i = 0; i < _DUMP_HEAP_CNT_; i++) {
		if (heap == dump_heap_list[i].heap)
			return dump_heap_list[i].heap_tag;
	}

	return HEAP_TAG_NULL;
}

static const char *get_mtk_heap_name(int heap_tag)
{
	int i;

	for (i = 0; i < _DUMP_HEAP_CNT_; i++) {
		if (heap_tag == dump_heap_list[i].heap_tag)
			return dump_heap_list[i].heap_name;
	}

	return "NULL";
}

static void dmabuf_log_dump(struct monitor_log_t *p_log, struct seq_file *file)
{
	int index = p_log - monitor_globals.record;
	u64 log_info = p_log->log_info;
	u64 time_tick = p_log->time_tick;
	u64 data1 = p_log->data1;
	u64 data2 = p_log->data2;
	u32 log_type, heap_tag;
	u64 tm;

	log_type = MONITOR_LOG_TYPE_GET(log_info);
	heap_tag = MONITOR_LOG_HEAP_GET(log_info);
	tm = MONITOR_LOG_TIME_GET(log_info);

	switch (log_type) {
	case DMABUF_LOG_ALLOC_TIME:
		dmabuf_dump(file, "[DMA-MONITOR][alloctime];heap;%s(%d);ind;%d;tm;%llu;alloc;",
			    get_mtk_heap_name(heap_tag), heap_tag, index, tm);
		dmabuf_dump(file, "%llu;ord;%llu;free;%llu;suc;%llu;pool;%llu;tick;%llu,%06llu;\n",
			    data2, data1 >> 32, (data1 & 0xFFFFFFFF) >> 2,
			    data1 & 0x1, data1 & 0x2,
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_ALLOCATE:
		dmabuf_dump(file, "[DMA-MONITOR][allocat];heap;%s(%d);ind;%d;tm;%llu;",
			    get_mtk_heap_name(heap_tag), heap_tag, index, tm);
		dmabuf_dump(file, "size;%llu;nent;%llu;inode;%llu;tick;%llu,%06llu;\n",
			    (data1 & 0xFFFFFFFF) << PAGE_SHIFT, data1 >> 32, data2,
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_REFILL:
		dmabuf_dump(file, "[DMA-MONITOR][refill];heap;%s(%d);ind;%d;tm;%llu;ret;%d;ord;",
			    get_mtk_heap_name(heap_tag),
			    heap_tag, index, tm,
			    MONITOR_FILL_RET_GET(data1) * -1);
		dmabuf_dump(file, "%llu;avl-fre-fil-pool;%llu;%llu;%llu;%llu;tick;%llu,%06llu;\n",
			    MONITOR_FILL_ORD_GET(data1),
			    MONITOR_FILL_MEMAVAIL_GET(data1),
			    MONITOR_FILL_MEMFREE_GET(data1),
			    P2M(data2 >> 32), P2M((data2 & 0xFFFFFFFF)),
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_PREFILL:
		dmabuf_dump(file, "[DMA-MONITOR][prefill];heap;%s(%d);ind;%d;tm;%llu;",
			    get_mtk_heap_name(heap_tag), heap_tag, index, tm);
		dmabuf_dump(file, "ret;%d;size;%llu;avail;%llu;free;%llu;tick;%llu,%06llu;\n",
			    (int)(data1 >> 32), data1 & 0xFFFFFFFF,
			    data2 >> 32, data2 & 0xFFFFFFFF,
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_RECYCLE:
		dmabuf_dump(file, "[DMA-MONITOR][remove];heap;%s(%d);ind;%d;tm;%llu;",
			    get_mtk_heap_name(heap_tag), heap_tag, index, tm);
		dmabuf_dump(file, "order;%llu;remove;%llu;page_new;%llu;tick;%llu,%06llu;\n",
			    data1, P2M(data2 >> 32), P2M(data2 & 0xFFFFFFFF),
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_POOL_SIZE:
		dmabuf_dump(file, "[DMA-MONITOR][poolsize];heap;%s(%d);ind;%d;free-total-8-4-0;",
			    get_mtk_heap_name(heap_tag), heap_tag, index);
		dmabuf_dump(file, "%llu;%llu;%llu;%llu;%llu;tick;%llu,%06llu;\n",
			    (data1 >> 32)*4,
			    (data1 & 0xFFFFFFFF)*4 + (data2 >> 32)*4 +
			    (data2 & 0xFFFFFFFF)*4,
			    (data1 & 0xFFFFFFFF)*4,
			    (data2 >> 32)*4, (data2 & 0xFFFFFFFF)*4,
			    time_tick / 1000000, time_tick % 1000000);
		break;

	case DMABUF_LOG_SHRINK:
		dmabuf_dump(file, "[DMA-MONITOR][shrink];heap;%s(%d);ind;%d;tm;%llu;order;%llu;",
			    get_mtk_heap_name(heap_tag), heap_tag, index, tm, data1 >> 32);
		dmabuf_dump(file, "size;%llu;free;%llu;scan;%llu;kind;%llu;tick;%llu,%06llu;\n",
			    (data1 & 0xFFFFFFFF)*4, (data2 >> 32)*4,
			    ((data2 & 0xFFFFFFFF)>>2)*4, (data2 & 0x3),
			    time_tick / 1000000, time_tick % 1000000);
		break;
	}
}

static void dmabuf_log_stats_alloc(struct monitor_log_t *p_log,
				   struct monitor_log_stats_t *log_stats)
{
	u64 log_info = p_log->log_info;
	u32 log_type = MONITOR_LOG_TYPE_GET(log_info);
	u64 tm = MONITOR_LOG_TIME_GET(log_info);
	u64 data1 = p_log->data1;

	if (log_type != DMABUF_LOG_ALLOCATE)
		return;

	log_stats->count++;
	log_stats->alloc.tm += tm;
	log_stats->alloc.size += (data1 & 0xFFFFFFFF) << PAGE_SHIFT;
	log_stats->alloc.nent += data1 >> 32;
}

static void dmabuf_log_stats_refill(struct monitor_log_t *p_log,
				    struct monitor_log_stats_t *log_stats)
{
	u64 log_info = p_log->log_info;
	u32 log_type = MONITOR_LOG_TYPE_GET(log_info);
	u64 data1 = p_log->data1;
	u64 data2 = p_log->data2;
	int ret = MONITOR_FILL_RET_GET(data1) * -1;
	int order = MONITOR_FILL_ORD_GET(data1);
	u32 fill = data2 >> 32;

	if (log_type != DMABUF_LOG_REFILL)
		return;

	log_stats->count++;
	if (ret == 0)
		log_stats->refill.ret_0++;

	if (fill > 0) {
		log_stats->refill.ret_fill++;
		log_stats->refill.page_fill += fill;

		if (order == orders[0])
			log_stats->refill.page_fill8 += fill;
		else if (order == orders[1])
			log_stats->refill.page_fill4 += fill;
	}
}

static void dmabuf_log_show_heap(struct seq_file *s)
{
	struct mtk_heap_priv_info *heap_priv;
	struct mtk_dmabuf_page_pool **pool;
	struct dma_heap *heap;
	long pool_total_size;
	unsigned int i, j;

	dmabuf_dump(s, "-------- pool size --------\n");
	//dmabuf_dump(s, "dump heap count:%d\n", _DUMP_HEAP_CNT_);
	for (i = 0; i < _DUMP_HEAP_CNT_; i++) {
		//heap = dma_heap_find(dump_heap_list[i].heap_name);
		heap = dump_heap_list[i].heap;
		if (!heap)
			continue;

		heap_priv = dma_heap_get_drvdata(heap);
		if (heap_priv && !heap_priv->uncached) {
			pool_total_size = mtk_dmabuf_page_pool_size(heap);
			if (!pool_total_size)
				continue;

			dmabuf_dump(s, "--->%s_heap_pool total size: %lu KB\n",
				    dma_heap_get_name(heap), pool_total_size >> 10);

			pool = heap_priv->page_pools;
			for (j = 0; j < NUM_ORDERS; j++) {
				dmabuf_dump(s, "--->%s_heap_pool order:%u size: %d KB\n",
					    dma_heap_get_name(heap), pool[j]->order,
					    mtk_dmabuf_page_pool_total(pool[j], true)*4);
			}
		}
	}
}

static void show_memory_info(struct seq_file *s)
{
	struct sysinfo mem_inf;
	long mem_avail;

	memset(&mem_inf, 0, sizeof(mem_inf));
	mem_avail = si_mem_available();
	si_meminfo(&mem_inf);

	dmabuf_dump(s, "-------- memory --------\n");
	dmabuf_dump(s, "available(M):%lu\n", P2M(mem_avail));
	dmabuf_dump(s, "freeram(M):%lu\n", P2M(mem_inf.freeram));
	dmabuf_dump(s, "totalram(M):%lu\n", P2M(mem_inf.totalram));
	dmabuf_dump(s, "sharedram(M):%lu\n", P2M(mem_inf.sharedram));
	dmabuf_dump(s, "bufferram(M):%lu\n", P2M(mem_inf.bufferram));
	dmabuf_dump(s, "totalhigh(M):%lu\n", P2M(mem_inf.totalhigh));
	dmabuf_dump(s, "freehigh(M):%lu\n", P2M(mem_inf.freehigh));
}

static void dmabuf_log_show(struct seq_file *s)
{
	unsigned int write_pointer, log_count, log_type, i;
	u64 log_start = 0, log_end = 0;
	struct monitor_log_t *p_log;
	struct monitor_log_stats_t stats_alloc;
	struct monitor_log_stats_t stats_refill;
	u32 remove_cnt = 0, shrink_cnt = 0, alloc_time_cnt = 0;
	struct zone *zone;

	stats_alloc.count = 0;
	stats_refill.count = 0;
	write_pointer = atomic_read(&monitor_globals.write_pointer);
	if (write_pointer < MONITOR_MAX_LOG_COUNT) {
		p_log = monitor_globals.record;
		log_count = write_pointer;
	} else {
		p_log = &monitor_globals.record[write_pointer % MONITOR_MAX_LOG_COUNT];
		log_count = MONITOR_MAX_LOG_COUNT;
	}

	log_start = p_log->time_tick;

	if (monitor_globals.dump_help) {
		dmabuf_dump(s, "debug cmd:\n");
		for (i = 0; i < ARRAY_SIZE(monitor_helper); i++) {
			dmabuf_dump(s, "\t|- %s %s\n",
				    monitor_helper[i].str,
				    *monitor_helper[i].flag ? "on" : "off");
		}
		dmabuf_dump(s, "\n");
	}
	if (log_count == 0)
		dmabuf_dump(s, "log is empty\n");

	for (i = 0; i < log_count; i++) {
		log_type = MONITOR_LOG_TYPE_GET(p_log->log_info);
		if (monitor_globals.dump_detail)
			dmabuf_log_dump(p_log, s);

		if (log_type == DMABUF_LOG_ALLOCATE) {
			if (stats_alloc.count == 0)
				memset(&stats_alloc, 0, sizeof(stats_alloc));
			dmabuf_log_stats_alloc(p_log, &stats_alloc);
		} else if (log_type == DMABUF_LOG_REFILL) {
			if (stats_refill.count == 0)
				memset(&stats_refill, 0, sizeof(stats_refill));
			dmabuf_log_stats_refill(p_log, &stats_refill);
		} else if (log_type == DMABUF_LOG_RECYCLE) {
			remove_cnt++;
		} else if (log_type == DMABUF_LOG_SHRINK) {
			shrink_cnt++;
		} else if (log_type == DMABUF_LOG_ALLOC_TIME) {
			alloc_time_cnt++;
		}

		if (i == log_count-1)
			log_end = p_log->time_tick;

		p_log++;
		if (unlikely(p_log - monitor_globals.record >= MONITOR_MAX_LOG_COUNT))
			p_log = monitor_globals.record;
	}

	dmabuf_dump(s, "-------- info --------\n");
	dmabuf_dump(s, "start time:%llu.%llu\n", log_start / 1000000, log_start % 1000000);
	dmabuf_dump(s, "log count:%u\n", log_count);
	dmabuf_dump(s, "pool interval(ms):%u\n", monitor_globals.log_pool_interval);

	if (refill_order_callback) {
		dmabuf_dump(s, "refill high water(M):8(%d),4(%d),0(%d)\n",
				refill_order_callback(0, -1),
				refill_order_callback(1, -1),
				refill_order_callback(2, -1));
		dmabuf_dump(s, "refill low water(M):8(%d),4(%d),0(%d)\n",
				refill_order_callback(0, -1) * 4/10,
				refill_order_callback(1, -1) * 4/10,
				refill_order_callback(2, -1) * 4/10);
	}

	for (i = 0; i < MAX_NR_ZONES; i++) {
		zone = &contig_page_data.node_zones[i];
		dmabuf_dump(s, "memory high water %s-%u (P):%lu,(M):%lu\n",
			    (zone->name)?zone->name:"NULL", i,
			    high_wmark_pages(zone), P2M(high_wmark_pages(zone)));
	}

	if (log_end > log_start)
		dmabuf_dump(s, "log time(ms):%llu\n", (log_end - log_start)/1000000);
	else
		dmabuf_dump(s, "log time:NULL\n");

	show_memory_info(s);

	if (monitor_globals.dump_pool)
		dmabuf_log_show_heap(s);

	if (alloc_time_cnt > 0) {
		dmabuf_dump(s, "-------- stats alloc time out --------\n");
		dmabuf_dump(s, "count:%u;\n", alloc_time_cnt);
	}

	if (stats_alloc.count > 0) {
		u64 total_size = (stats_alloc.alloc.size >> 20);

		dmabuf_dump(s, "-------- stats alloc --------\n");
		if (total_size > 0) {
			dmabuf_dump(s, "count:%u;time(us/C):%llu;time(us/M):%llu;",
				    stats_alloc.count,
				    stats_alloc.alloc.tm/stats_alloc.count,
				    stats_alloc.alloc.tm/total_size);
			dmabuf_dump(s, "nent(p/M):%llu;alloc(M):%llu;alloc(M/C):%llu;\n",
				    stats_alloc.alloc.nent/total_size, total_size,
				    total_size/stats_alloc.count);
		} else {
			dmabuf_dump(s, "total size low 1M\n");
		}
	}

	if (stats_refill.count > 0) {
		dmabuf_dump(s, "-------- stats refill --------\n");
		dmabuf_dump(s, "count:%u;ret(0):%d;cnt:%d;mem(KB):%u,%03u;fill8:%u;fill4:%u;\n",
			    stats_refill.count, stats_refill.refill.ret_0,
			    stats_refill.refill.ret_fill,
			    P2K(stats_refill.refill.page_fill) / 1000,
			    P2K(stats_refill.refill.page_fill) % 1000,
			    P2K(stats_refill.refill.page_fill8),
			    P2K(stats_refill.refill.page_fill4));
	}

	if (remove_cnt > 0) {
		dmabuf_dump(s, "-------- stats remove --------\n");
		dmabuf_dump(s, "count:%u;\n", remove_cnt);
	}

	if (shrink_cnt > 0) {
		dmabuf_dump(s, "-------- stats shrink --------\n");
		dmabuf_dump(s, "count:%u;\n", shrink_cnt);
	}
}

static int heap_monitor_proc_show(struct seq_file *s, void *v)
{
	if (!s)
		return -EINVAL;

	dmabuf_log_show(s);
	return 0;
}

static void dmabuf_log_add(u64 log_info, u64 data1, u64 data2)
{
	unsigned int write_pointer, index;
	struct monitor_log_t *p_log;

	write_pointer = atomic_inc_return(&monitor_globals.write_pointer);
	index = (write_pointer - 1) % MONITOR_MAX_LOG_COUNT;
	p_log = &monitor_globals.record[index];
	p_log->log_info = log_info;
	p_log->data1 = data1;
	p_log->data2 = data2;
	p_log->time_tick = sched_clock();

	if (monitor_globals.dump_kernel)
		dmabuf_log_dump(p_log, NULL);
}

void dmabuf_log_alloc_time(struct mtk_dmabuf_page_pool *pool, u64 tm, u64 tm_alloc,
			   bool success, bool from_pool)
{
	u64 log_info = 0, data1 = 0;
	int heap_tag = pool->heap_tag;
	long mem_free;

	if (!monitor_globals.enable || !monitor_globals.log_alloc_time)
		return;

	mem_free = P2M(global_zone_page_state(NR_FREE_PAGES));
	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_ALLOC_TIME);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);
	data1 = ((u64)pool->order << 32);
	data1 |=  mem_free << 2;
	if (success)
		data1 |= 0x1;
	if (from_pool)
		data1 |= 0x2;
	dmabuf_log_add(log_info, data1, tm_alloc);
}

void dmabuf_log_allocate(struct dma_heap *heap, u64 tm, u32 nent, u32 pages, u32 inode)
{
	u64 log_info = 0, data1 = 0;
	int heap_tag = get_mtk_heap_tag(heap);

	if (!monitor_globals.enable || !monitor_globals.log_allocate)
		return;

	//if (heap_tag == HEAP_TAG_SYSTEM || heap_tag == HEAP_TAG_SYSTEM_UNCACHED)
	//	return;

	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_ALLOCATE);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);
	data1 = ((u64)nent << 32) | pages;
	dmabuf_log_add(log_info, data1, inode);
}

void dmabuf_log_pool_size(struct dma_heap *heap)
{
	u64 log_info = 0, data1, data2;
	u64 order0_page = 0, order4_page = 0, order8_page = 0, tm1, mem_free;
	struct mtk_heap_priv_info *heap_priv;
	struct mtk_dmabuf_page_pool **pools;
	int heap_tag;

	if (!monitor_globals.enable || !monitor_globals.log_pool_interval)
		return;

	heap_priv = dma_heap_get_drvdata(heap);
	pools = heap_priv ? heap_priv->page_pools : NULL;
	if (!pools)
		return;

	tm1 = sched_clock();
	if (monitor_globals.log_pool_interval > 1 &&
		(tm1 - pools[0]->log_time) / 1000000 < monitor_globals.log_pool_interval)
		return;

	mem_free = global_zone_page_state(NR_FREE_PAGES);
	order8_page = mtk_dmabuf_page_pool_total(pools[0], true);
	pools[0]->log_time = tm1;

	if (NUM_ORDERS > 1)
		order4_page = mtk_dmabuf_page_pool_total(pools[1], true);

	if (NUM_ORDERS > 2)
		order0_page = mtk_dmabuf_page_pool_total(pools[2], true);

	heap_tag = get_mtk_heap_tag(heap);
	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_POOL_SIZE);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);

	data1 = mem_free << 32 | order8_page;
	data2 = order4_page << 32 | order0_page;
	dmabuf_log_add(log_info, data1, data2);
}

void dmabuf_log_shrink(struct mtk_dmabuf_page_pool *pool, u64 tm, int freed, int scan)
{
	u64 log_info = 0, data1, data2;

	if (!monitor_globals.enable || !monitor_globals.log_shrink)
		return;

	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_SHRINK);
	MONITOR_LOG_HEAP_SET(log_info, pool->heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);

	data1 = ((u64)pool->order << 32) | mtk_dmabuf_page_pool_total(pool, true);
	data2 = ((u64)freed << 32) | (u32)scan;
	dmabuf_log_add(log_info, data1, data2);
}

void dmabuf_log_refill(int heap_tag, u64 tm, int ret, u32 order, u32 pages_refill, u32 pages_new)
{
	u64 log_info = 0, data1 = 0, data2;
	long mem_avail, mem_free;

	if (!monitor_globals.enable || !monitor_globals.log_refill)
		return;

	mem_free = P2M(global_zone_page_state(NR_FREE_PAGES));
	mem_avail = P2M(si_mem_available());

	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_REFILL);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);

	//data1 = ((u64)order << 32) | (u32)ret;
	ret *= -1;
	MONITOR_FILL_RET_SET(data1, ret);
	MONITOR_FILL_ORD_SET(data1, order);
	MONITOR_FILL_MEMFREE_SET(data1, mem_free);
	MONITOR_FILL_MEMAVAIL_SET(data1, mem_avail);

	data2 = ((u64)pages_refill << 32) | pages_new;
	dmabuf_log_add(log_info, data1, data2);
}
EXPORT_SYMBOL_GPL(dmabuf_log_refill);

void dmabuf_log_prefill(struct dma_heap *heap, u64 tm, int ret, u32 size)
{
	u64 log_info = 0, data1 = 0, data2;
	long mem_avail, mem_free;
	int heap_tag = HEAP_TAG_NULL;

	if (!monitor_globals.enable || !monitor_globals.log_prefill)
		return;

	if (heap)
		heap_tag = get_mtk_heap_tag(heap);

	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_PREFILL);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);

	mem_free = P2M(global_zone_page_state(NR_FREE_PAGES));
	mem_avail = P2M(si_mem_available());

	data1 = ((u64)ret << 32) | size;
	data2 = ((u64)mem_free << 32) | (u32)mem_avail;
	dmabuf_log_add(log_info, data1, data2);
}

void dmabuf_log_recycle(int heap_tag, u64 tm, u32 order, u32 pages_recycle, u32 pages_new)
{
	u64 log_info = 0, data2;

	if (!monitor_globals.enable || !monitor_globals.log_recycle)
		return;

	MONITOR_LOG_TYPE_SET(log_info, DMABUF_LOG_RECYCLE);
	MONITOR_LOG_HEAP_SET(log_info, heap_tag);
	MONITOR_LOG_TIME_SET(log_info, tm);
	data2 = ((u64)pages_recycle << 32) | pages_new;
	dmabuf_log_add(log_info, order, data2);
}
EXPORT_SYMBOL_GPL(dmabuf_log_recycle);

int mtk_register_refill_order_callback(mtk_refill_order_cb cb)
{
	if (refill_order_callback)
		return 0;

	refill_order_callback = cb;
	return 1;
}
EXPORT_SYMBOL_GPL(mtk_register_refill_order_callback);

#else
void dmabuf_log_alloc_time(struct mtk_dmabuf_page_pool *pool, u64 tm, u64 tm_alloc,
			   bool success, bool from_pool)
{
}

void dmabuf_log_allocate(struct dma_heap *heap, u64 tm, u32 nent, u32 pages, u32 inode)
{
}

void dmabuf_log_refill(int heap_tag, u64 tm, int ret, u32 order, u32 pages_refill, u32 pages_new)
{
}
EXPORT_SYMBOL_GPL(dmabuf_log_refill);

void dmabuf_log_recycle(int heap_tag, u64 tm, u32 order, u32 pages_recycle, u32 pages_new)
{
}
EXPORT_SYMBOL_GPL(dmabuf_log_recycle);

void dmabuf_log_shrink(struct mtk_dmabuf_page_pool *pool, u64 tm, int freed, int scan)
{
}

void dmabuf_log_pool_size(struct dma_heap *heap)
{
}

void dmabuf_log_prefill(struct dma_heap *heap, u64 tm, int ret, u32 size)
{
}
#endif
