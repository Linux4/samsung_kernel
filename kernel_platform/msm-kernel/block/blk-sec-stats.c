// SPDX-License-Identifier: GPL-2.0
/*
 *  Samsung Block Statistics
 *
 *  Copyright (C) 2021 Manjong Lee <mj0123.lee@samsung.com>
 *  Copyright (C) 2021 Junho Kim <junho89.kim@samsung.com>
 *  Copyright (C) 2021 Changheun Lee <nanich.lee@samsung.com>
 *  Copyright (C) 2021 Seunghwan Hyun <seunghwan.hyun@samsung.com>
 */

#include <linux/types.h>
#include <linux/time64.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/part_stat.h>
#include <linux/blk-mq.h>

#include "blk-mq.h"
#include "blk-mq-tag.h"

#define PROC_NAME_LEN		16
#define MAX_PIO_NODE_NUM	10000
#define SORT_PIO_NODE_NUM	100

struct request_info {
	pid_t tgid;
	char tg_name[PROC_NAME_LEN];
	u64 tg_start_time;

	struct request *rq;
	unsigned int data_size;
};

struct disk_info {
	/* fields related with target device itself */
	struct gendisk *gd;
	struct request_queue *queue;

	/* fields related with IO accoutning */
	struct request_info *rq_infos;
	int nr_rq_info;
};

struct accumulated_stats {
	struct timespec64 uptime;
	unsigned long sectors[3];       /* READ, WRITE, DISCARD */
	unsigned long ios[3];
	unsigned long iot;
};

struct pio_node {
	struct list_head list;
	pid_t tgid;
	char name[PROC_NAME_LEN];
	u64 start_time;
	unsigned long long bytes[REQ_OP_DISCARD + 1];
};

static unsigned long long transferred_bytes;

static struct disk_info internal_disk;

static struct accumulated_stats old, new;

static DEFINE_SPINLOCK(pio_list_lock);
static DEFINE_SPINLOCK(others_pio_lock);
LIST_HEAD(pio_list);
static int pio_cnt;
static int pio_enabled;
static unsigned int pio_duration_ms = 5000;
static unsigned long pio_timeout;
static struct kmem_cache *pio_cache;

static struct pio_node others = {
	.list = LIST_HEAD_INIT(others.list),
	.tgid = 99999,
	.name = "others",
	.start_time = 9999999,
	.bytes = {0, 0, 0, 0},
};

static struct gendisk *get_internal_disk(void)
{
	struct gendisk *gd = NULL;
	struct block_device *bdev;
	int idx;
	/* In some project which is powered by MediaTek AP, the internal
	 * storage device is "sdc / MKDEV(8, 32)". So we have to try (8, 32)
	 * before trying (179, 0).
	 */
	dev_t devno[] = {
		MKDEV(8, 0),
		MKDEV(8, 32),
		MKDEV(179, 0),
		MKDEV(0, 0)
	};

	for (idx = 0; devno[idx] != MKDEV(0, 0); idx++) {
		bdev = blkdev_get_by_dev(devno[idx], FMODE_READ, NULL);
		if (!IS_ERR(bdev)) {
			gd = bdev->bd_disk;
			break;
		}
	}

	return gd;
}

static inline int init_internal_disk_info(void)
{
	/* it only sets internal_disk.gd info.
	 * internal_disk.rq_infos have to be allocated later.
	 */
	if (!internal_disk.gd) {
		internal_disk.gd = get_internal_disk();
		if (unlikely(!internal_disk.gd)) {
			pr_err("%s: can't find internal disk\n", __func__);
			return -ENODEV;
		}
	}
	internal_disk.queue = internal_disk.gd->queue;

	return 0;
}

static inline void clear_internal_disk_info(void)
{
	internal_disk.gd = NULL;
	internal_disk.queue = NULL;
}

static inline bool has_valid_disk_info(void)
{
	return !!internal_disk.queue;
}

static struct request_info *__alloc_request_infos(unsigned int nr_rq_info)
{
	struct request_info *rq_infos;
	size_t size;

	size = sizeof(struct request_info) * nr_rq_info;
	rq_infos = kmalloc(size, GFP_KERNEL);
	if (!rq_infos)
		return NULL;

	memset(rq_infos, 0x0, size);

	return rq_infos;
}

static int alloc_request_infos(struct disk_info *disk, unsigned int nr)
{
	if (disk->rq_infos)
		return 0;

	if (!disk->queue)
		return -EINVAL;

	disk->rq_infos = __alloc_request_infos(nr);
	if (!disk->rq_infos) {
		disk->nr_rq_info = 0;
		pr_err("%s: rq_infos allocation is failed\n", __func__);
		return -ENOMEM;
	}

	disk->nr_rq_info = nr;

	return 0;
}

static inline void free_request_infos(struct disk_info *disk)
{
	struct request_info *tmp;

	tmp = disk->rq_infos;
	disk->nr_rq_info = 0;
	disk->rq_infos = NULL;
	kfree(tmp);
}

/*
 * 3 cases are possible for the value of (nr_rq_info, rq_infos)
 * --------------------
 * - nr_rq_info > 0 && rq_infos != null : valid
 * - nr_rq_info == 0 && rq_infos == null: not initialized or freed already
 * - nr_rq_info == 0 && rq_infos != null: disabled. will be freed later
 */
static inline void disable_request_infos(struct disk_info *disk)
{
	/* Let disk->rq_infos untouched.  It will be freed in
	 * blk_sec_stats_account_exit or blk_sec_stats_depth_updated.
	 */
	disk->nr_rq_info = 0;
}

static inline bool has_valid_request_infos(void)
{
	return (internal_disk.nr_rq_info > 0);
}

void blk_sec_stats_account_init(struct request_queue *q)
{
	int ret;

	if (!has_valid_disk_info()) {
		ret = init_internal_disk_info();
		if (ret) {
			clear_internal_disk_info();
			pr_err("%s: Can't find internal disk info!", __func__);
			return;
		}
	}

	if (internal_disk.queue != q)
		return;

	/* Initializing internal_disk data could be divided into 2 parts.
	 *   1. setting up gendisk info (internal_disk.gd).
	 *   2. allocating request_info array (internal_disk.rq_infos).
	 * Part 1 could be run concurrently, but part 2 have to be serialized.
	 * To that end, we run part 2 only when "internal_disk.gd->queue != q",
	 * which means it's in the middle of initializing ssg scheduler of
	 * internal storage device.
	 * So, it is guaranteed that following code couldn't run concurrently.
	 */
	ret = alloc_request_infos(&internal_disk, q->nr_requests);
	if (ret)
		pr_err("%s: request-accounting is disabled.", __func__);
}
EXPORT_SYMBOL(blk_sec_stats_account_init);

void blk_sec_stats_account_exit(struct elevator_queue *eq)
{
	if (!has_valid_disk_info())
		return;

	if (internal_disk.queue->elevator != eq)
		return;

	/* it is called while queue is empty and freezed. just free rq_infos */
	free_request_infos(&internal_disk);
}
EXPORT_SYMBOL(blk_sec_stats_account_exit);

void blk_sec_stats_depth_updated(struct blk_mq_hw_ctx *hctx)
{
	unsigned int nr;
	int ret;

	if (!has_valid_disk_info())
		return;

	if (internal_disk.queue != hctx->queue)
		return;

	/* this function is called with following callstack.
	 *    queue_requests_store
	 *      > blk_mq_update_nr_requests
	 *        > blk_mq_freeze_queue
	 *        > blk_mq_quiesce_queue
	 *        ...
	 *        > ssg_depth_updated
	 *          > blk_sec_stats_depth_updated
	 *
	 * since the queue is freezed now, we don't need to care about
	 * any on-ging IO requests. The queue is empty and any process
	 * trying to sumbit IO requests will be waiting inside of
	 * bio_queue_enter > wait_event function.
	 * Just clear rq_infos to NULL, and allocate it again.
	 */
	free_request_infos(&internal_disk);

	nr = hctx->sched_tags->bitmap_tags->sb.depth;
	ret = alloc_request_infos(&internal_disk, nr);
	if (ret)
		pr_err("%s: request-accounting is disabled.", __func__);
}
EXPORT_SYMBOL(blk_sec_stats_depth_updated);


#define UNSIGNED_DIFF(n, o) (((n) >= (o)) ? ((n) - (o)) : ((n) + (0 - (o))))
#define SECTORS2KB(x) ((x) / 2)

static inline void get_monotonic_boottime(struct timespec64 *ts)
{
	*ts = ktime_to_timespec64(ktime_get_boottime());
}

static ssize_t diskios_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct hd_struct *hd;
	long hours;

	if (unlikely(!has_valid_disk_info()))
		return -EINVAL;

	hd = &(internal_disk.gd->part0);

	new.ios[STAT_READ] = part_stat_read(hd, ios[STAT_READ]);
	new.ios[STAT_WRITE] = part_stat_read(hd, ios[STAT_WRITE]);
	new.ios[STAT_DISCARD] = part_stat_read(hd, ios[STAT_DISCARD]);
	new.sectors[STAT_READ] = part_stat_read(hd, sectors[STAT_READ]);
	new.sectors[STAT_WRITE] = part_stat_read(hd, sectors[STAT_WRITE]);
	new.sectors[STAT_DISCARD] = part_stat_read(hd, sectors[STAT_DISCARD]);
	new.iot = jiffies_to_msecs(part_stat_read(hd, io_ticks)) / 1000;

	get_monotonic_boottime(&(new.uptime));
	hours = (new.uptime.tv_sec - old.uptime.tv_sec) / 60;
	hours = (hours + 30) / 60;

	ret = sprintf(buf, "\"ReadC\":\"%lu\",\"ReadKB\":\"%lu\","
			"\"WriteC\":\"%lu\",\"WriteKB\":\"%lu\","
			"\"DiscardC\":\"%lu\",\"DiscardKB\":\"%lu\","
			"\"IOT\":\"%lu\","
			"\"Hours\":\"%ld\"\n",
			UNSIGNED_DIFF(new.ios[STAT_READ], old.ios[STAT_READ]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_READ], old.sectors[STAT_READ])),
			UNSIGNED_DIFF(new.ios[STAT_WRITE], old.ios[STAT_WRITE]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_WRITE], old.sectors[STAT_WRITE])),
			UNSIGNED_DIFF(new.ios[STAT_DISCARD], old.ios[STAT_DISCARD]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_DISCARD], old.sectors[STAT_DISCARD])),
			UNSIGNED_DIFF(new.iot, old.iot),
			hours);

	old.ios[STAT_READ] = new.ios[STAT_READ];
	old.ios[STAT_WRITE] = new.ios[STAT_WRITE];
	old.ios[STAT_DISCARD] = new.ios[STAT_DISCARD];
	old.sectors[STAT_READ] = new.sectors[STAT_READ];
	old.sectors[STAT_WRITE] = new.sectors[STAT_WRITE];
	old.sectors[STAT_DISCARD] = new.sectors[STAT_DISCARD];
	old.uptime = new.uptime;
	old.iot = new.iot;

	return ret;
}

static void add_pio_node(struct request_info *rq_info)
{
	struct pio_node *pio = NULL;

	if (pio_cnt >= MAX_PIO_NODE_NUM) {
add_others:
		spin_lock_bh(&others_pio_lock);
		others.bytes[req_op(rq_info->rq)] += rq_info->data_size;
		spin_unlock_bh(&others_pio_lock);
		return;
	}

	pio = kmem_cache_alloc(pio_cache, GFP_NOWAIT);
	if (!pio)
		goto add_others;

	INIT_LIST_HEAD(&pio->list);

	pio->tgid = rq_info->tgid;
	strncpy(pio->name, rq_info->tg_name, PROC_NAME_LEN - 1);
	pio->name[PROC_NAME_LEN - 1] = '\0';
	pio->start_time = rq_info->tg_start_time;

	pio->bytes[REQ_OP_READ] = 0;
	pio->bytes[REQ_OP_WRITE] = 0;
	pio->bytes[REQ_OP_FLUSH] = 0;
	pio->bytes[REQ_OP_DISCARD] = 0;
	pio->bytes[req_op(rq_info->rq)] = rq_info->data_size;

	spin_lock_bh(&pio_list_lock);
	list_add(&pio->list, &pio_list);
	spin_unlock_bh(&pio_list_lock);

	pio_cnt++;
}

static void free_pio_node(struct list_head *remove_list)
{
	struct list_head *ptr, *ptrn;
	struct pio_node *node;

	list_for_each_safe(ptr, ptrn, remove_list) {
		node = list_entry(ptr, struct pio_node, list);
		list_del(&node->list);
		kmem_cache_free(pio_cache, node);
	}

	spin_lock_bh(&others_pio_lock);
	others.bytes[REQ_OP_READ] = 0;
	others.bytes[REQ_OP_WRITE] = 0;
	others.bytes[REQ_OP_FLUSH] = 0;
	others.bytes[REQ_OP_DISCARD] = 0;
	spin_unlock_bh(&others_pio_lock);

	pio_cnt = 0;
}

static void update_process_IO(struct request_info *rq_info)
{
	struct request *rq = rq_info->rq;
	struct pio_node *node;
	struct list_head *ptr;
	unsigned long size = 0;
	LIST_HEAD(remove_list);

	if (pio_enabled == 0)
		return;
	if (time_after(jiffies, pio_timeout))
		return;
	if (req_op(rq) > REQ_OP_DISCARD)
		return;

	size = (req_op(rq) == REQ_OP_FLUSH) ? 1 : rq_info->data_size;

	spin_lock_bh(&pio_list_lock);
	list_for_each(ptr, &pio_list) {
		node = list_entry(ptr, struct pio_node, list);
		if (node->tgid == rq_info->tgid) {
			if (node->start_time != rq_info->tg_start_time)
				continue;
			node->bytes[req_op(rq)] += size;
			spin_unlock_bh(&pio_list_lock);
			return;
		}
	}
	spin_unlock_bh(&pio_list_lock);

	add_pio_node(rq_info);
}

static inline bool may_account_rq(struct request *rq)
{
	if (unlikely(!has_valid_disk_info()))
		return false;

	if (unlikely(!has_valid_request_infos()))
		return false;

	if (internal_disk.queue != rq->q)
		return false;

	if (unlikely(rq->internal_tag < 0)) {
		pr_warn_ratelimited("%s: rq->internal_tag < 0!", __func__);
		return false;
	}
	if (unlikely(rq->internal_tag >= internal_disk.nr_rq_info)) {
		pr_err("%s: rq->internal_tag[%d] > nr_rq_info[%d]!",
		       __func__, rq->internal_tag, internal_disk.nr_rq_info);

		disable_request_infos(&internal_disk);
		pr_err("%s: request-accounting is disabled.", __func__);

		return false;
	}

	return true;
}

void blk_sec_stats_account_rq_insert(struct request *rq)
{
	struct request_info *rq_info;
	struct task_struct *gleader = current->group_leader;

	if (unlikely(!may_account_rq(rq)))
		return;

	rq_info = &(internal_disk.rq_infos[rq->internal_tag]);

	rq_info->rq = rq;
	rq_info->tgid = task_tgid_nr(gleader);
	strncpy(rq_info->tg_name, gleader->comm, PROC_NAME_LEN - 1);
	rq_info->tg_name[PROC_NAME_LEN - 1] = '\0';
	rq_info->tg_start_time = gleader->start_time;
}
EXPORT_SYMBOL(blk_sec_stats_account_rq_insert);

void blk_sec_stats_account_rq_dispatch(struct request *rq)
{
	struct request_info *rq_info;

	if (unlikely(!may_account_rq(rq)))
		return;

	rq_info = &(internal_disk.rq_infos[rq->internal_tag]);

	if (likely(rq_info->rq == rq))
		rq_info->data_size = blk_rq_bytes(rq);
}
EXPORT_SYMBOL(blk_sec_stats_account_rq_dispatch);

void blk_sec_stats_account_rq_finish(struct request *rq)
{
	struct request_info *rq_info;

	if (unlikely(!may_account_rq(rq)))
		return;

	rq_info = &(internal_disk.rq_infos[rq->internal_tag]);

	if (likely(rq_info->rq == rq)) {
		transferred_bytes += rq_info->data_size;
		update_process_IO(rq_info);
	}
}
EXPORT_SYMBOL(blk_sec_stats_account_rq_finish);

#define GET_PIO_PRIO(pio) \
	((pio)->bytes[REQ_OP_READ] + (pio)->bytes[REQ_OP_WRITE]*2)

static void sort_pios(struct list_head *remove_list)
{
	struct list_head *tmp;
	struct pio_node *max_node = NULL;
	struct pio_node *node;
	unsigned long long max = 0;
	LIST_HEAD(sorted_list);
	int i;

	for (i = 0; i < SORT_PIO_NODE_NUM; i++) {
		list_for_each(tmp, remove_list) {
			node = list_entry(tmp, struct pio_node, list);
			if (GET_PIO_PRIO(node) > max) {
				max = GET_PIO_PRIO(node);
				max_node = node;
			}
		}
		if (max_node != NULL)
			list_move_tail(&max_node->list, &sorted_list);

		max = 0;
		max_node = NULL;
	}
	list_splice_init(&sorted_list, remove_list);
}

static ssize_t pio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct list_head *ptr;
	struct pio_node *node;
	int len = 0;
	LIST_HEAD(remove_list);

	spin_lock_bh(&pio_list_lock);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock_bh(&pio_list_lock);

	if (pio_cnt > SORT_PIO_NODE_NUM)
		sort_pios(&remove_list);

	list_for_each(ptr, &remove_list) {
		node = list_entry(ptr, struct pio_node, list);
		if (PAGE_SIZE - len > 80) {
			/* pid read(KB) write(KB) comm printed */
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
					node->tgid, node->bytes[REQ_OP_READ] / 1024,
					node->bytes[REQ_OP_WRITE] / 1024,  node->name);
		} else {
			spin_lock_bh(&others_pio_lock);
			others.bytes[REQ_OP_READ] += node->bytes[REQ_OP_READ];
			others.bytes[REQ_OP_WRITE] += node->bytes[REQ_OP_WRITE];
			others.bytes[REQ_OP_FLUSH] += node->bytes[REQ_OP_FLUSH];
			others.bytes[REQ_OP_DISCARD] += node->bytes[REQ_OP_DISCARD];
			spin_unlock_bh(&others_pio_lock);
		}
	}
	spin_lock_bh(&others_pio_lock);

	if (others.bytes[REQ_OP_READ] + others.bytes[REQ_OP_WRITE])
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
				others.tgid, others.bytes[REQ_OP_READ] / 1024,
				others.bytes[REQ_OP_WRITE] / 1024,  others.name);
	spin_unlock_bh(&others_pio_lock);

	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return len;
}

static ssize_t pio_enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	LIST_HEAD(remove_list);

	kstrtoint(buf, 10, &enable);
	pio_enabled = (enable >= 1) ? 1 : 0;

	spin_lock_bh(&pio_list_lock);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock_bh(&pio_list_lock);
	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return count;
}

static ssize_t pio_enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%d\n", pio_enabled);

	return len;
}

static ssize_t pio_duration_ms_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	kstrtoint(buf, 10, &pio_duration_ms);

	if (pio_duration_ms > 10000)
		pio_duration_ms = 10000;

	return count;
}

static ssize_t pio_duration_ms_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%u\n", pio_duration_ms);

	return len;
}

static ssize_t transferred_bytes_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%llu\n", transferred_bytes);
}

static struct kobj_attribute diskios_attr = __ATTR(diskios, 0444, diskios_show,  NULL);
static struct kobj_attribute pios_attr = __ATTR(pios, 0444, pio_show,  NULL);
static struct kobj_attribute pios_enable_attr = __ATTR(pios_enable, 0644,
		pio_enabled_show,  pio_enabled_store);
static struct kobj_attribute pios_duration_ms_attr = __ATTR(pios_duration_ms, 0644,
		pio_duration_ms_show, pio_duration_ms_store);
static struct kobj_attribute transferred_bytes_attr = __ATTR(transferred_bytes, 0444,
		transferred_bytes_show,  NULL);

static struct attribute *blk_sec_stats_attrs[] = {
	&diskios_attr.attr,
	&pios_attr.attr,
	&pios_enable_attr.attr,
	&pios_duration_ms_attr.attr,
	&transferred_bytes_attr.attr,
	NULL,
};

static struct attribute_group blk_sec_stats_group = {
	.attrs = blk_sec_stats_attrs,
	NULL,
};

static struct kobject *blk_sec_stats_kobj;

static int __init blk_sec_stats_init(void)
{
	int retval;

	blk_sec_stats_kobj = kobject_create_and_add("blk_sec_stats", kernel_kobj);
	if (!blk_sec_stats_kobj)
		return -ENOMEM;

	pio_cache = kmem_cache_create("pio_node", sizeof(struct pio_node), 0, 0, NULL);
	if (!pio_cache)
		return -ENOMEM;

	retval = sysfs_create_group(blk_sec_stats_kobj, &blk_sec_stats_group);
	if (retval)
		kobject_put(blk_sec_stats_kobj);

	retval = init_internal_disk_info();
	if (retval) {
		clear_internal_disk_info();
		pr_err("%s: Can't find internal disk info!", __func__);
	}

	return 0;
}

static void __exit blk_sec_stats_exit(void)
{
	free_request_infos(&internal_disk);
	clear_internal_disk_info();
	kmem_cache_destroy(pio_cache);
	kobject_put(blk_sec_stats_kobj);
}

module_init(blk_sec_stats_init);
module_exit(blk_sec_stats_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Manjong Lee <mj0123.lee@samsung.com>");
MODULE_DESCRIPTION("SEC Storage stats module for various purposes");
MODULE_VERSION("0.1");
