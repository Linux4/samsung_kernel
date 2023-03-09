// SPDX-License-Identifier: GPL-2.0
/*
 *  Samsung Block Statistics
 *
 *  Copyright (C) 2021 Manjong Lee <mj0123.lee@samsung.com>
 *  Copyright (C) 2021 Junho Kim <junho89.kim@samsung.com>
 *  Copyright (C) 2021 Changheun Lee <nanich.lee@samsung.com>
 *  Copyright (C) 2021 Seunghwan Hyun <seunghwan.hyun@samsung.com>
 *  Copyright (C) 2021 Tran Xuan Nam <nam.tx2@samsung.com>
 */

#include <linux/sysfs.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>

#define MAX_PIO_NODE_NUM	10000
#define SORT_PIO_NODE_NUM	100
#define PIO_HASH_SIZE		100

#define GET_HASH_KEY(tgid) ((unsigned int)(tgid) % PIO_HASH_SIZE)

struct pio_node {
	struct list_head list;

	pid_t tgid;
	char name[TASK_COMM_LEN];
	u64 start_time;

	unsigned long long bytes[REQ_OP_DISCARD + 1];

	struct pio_node *h_next; /* next pio_node for hash */
};

#define RESET_PIO_IO(pio) \
do { \
	(pio)->bytes[REQ_OP_READ] = 0; \
	(pio)->bytes[REQ_OP_WRITE] = 0; \
	(pio)->bytes[REQ_OP_FLUSH] = 0; \
	(pio)->bytes[REQ_OP_DISCARD] = 0; \
} while (0)

#define GET_PIO_PRIO(pio) \
	((pio)->bytes[REQ_OP_READ] + (pio)->bytes[REQ_OP_WRITE]*2)

static DEFINE_SPINLOCK(pio_list_lock);
static DEFINE_SPINLOCK(others_pio_lock);
LIST_HEAD(pio_list);
static int pio_cnt;
static int pio_enabled;
static unsigned int pio_duration_ms = 5000;
static unsigned long pio_timeout;
static struct kmem_cache *pio_cache;
static struct pio_node *pio_hash[PIO_HASH_SIZE];

static struct pio_node others = {
	.list = LIST_HEAD_INIT(others.list),
	.tgid = 99999,
	.name = "others",
	.start_time = 9999999,
	.bytes = {0, 0, 0, 0},
};

static void add_pio_node(struct request *rq, unsigned int data_size,
		pid_t tgid, const char *tg_name, u64 tg_start_time)
{
	struct pio_node *pio = NULL;
	unsigned int hash_key = 0;
	unsigned long flags;

	if (pio_cnt >= MAX_PIO_NODE_NUM) {
add_others:
		spin_lock_irqsave(&others_pio_lock, flags);
		others.bytes[req_op(rq)] += data_size;
		spin_unlock_irqrestore(&others_pio_lock, flags);
		return;
	}

	pio = kmem_cache_alloc(pio_cache, GFP_NOWAIT);
	if (!pio)
		goto add_others;

	INIT_LIST_HEAD(&pio->list);

	pio->tgid = tgid;
	strncpy(pio->name, tg_name, TASK_COMM_LEN - 1);
	pio->name[TASK_COMM_LEN - 1] = '\0';
	pio->start_time = tg_start_time;

	RESET_PIO_IO(pio);
	pio->bytes[req_op(rq)] = data_size;

	hash_key = GET_HASH_KEY(tgid);

	spin_lock_irqsave(&pio_list_lock, flags);
	list_add(&pio->list, &pio_list);
	pio->h_next = pio_hash[hash_key];
	pio_hash[hash_key] = pio;
	spin_unlock_irqrestore(&pio_list_lock, flags);

	pio_cnt++;
}

static struct pio_node *find_pio_node(pid_t tgid, u64 tg_start_time)
{
	struct pio_node *pio;

	for (pio = pio_hash[GET_HASH_KEY(tgid)]; pio; pio = pio->h_next) {
		if (pio->tgid != tgid)
			continue;
		if (pio->start_time != tg_start_time)
			continue;
		return pio;
	}

	return NULL;
}

static void free_pio_nodes(struct list_head *remove_list)
{
	struct pio_node *pio;
	struct pio_node *pion;

	list_for_each_entry_safe(pio, pion, remove_list, list) {
		list_del(&pio->list);
		kmem_cache_free(pio_cache, pio);
	}
}

void update_pio_node(struct request *rq, unsigned int data_size,
		pid_t tgid, const char *tg_name, u64 tg_start_time)
{
	struct pio_node *pio;
	unsigned long size = 0;
	unsigned long flags;

	if (pio_enabled == 0)
		return;
	if (time_after(jiffies, pio_timeout))
		return;
	if (req_op(rq) > REQ_OP_DISCARD)
		return;

	size = (req_op(rq) == REQ_OP_FLUSH) ? 1 : data_size;

	spin_lock_irqsave(&pio_list_lock, flags);
	pio = find_pio_node(tgid, tg_start_time);
	if (pio) {
		pio->bytes[req_op(rq)] += size;
		spin_unlock_irqrestore(&pio_list_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&pio_list_lock, flags);

	add_pio_node(rq, data_size, tgid, tg_name, tg_start_time);
}

static void sort_pios(struct list_head *pios)
{
	struct pio_node *max_pio = NULL;
	struct pio_node *pio;
	unsigned long long max = 0;
	LIST_HEAD(sorted_list);
	int i;

	for (i = 0; i < SORT_PIO_NODE_NUM; i++) {
		list_for_each_entry(pio, pios, list) {
			if (GET_PIO_PRIO(pio) > max) {
				max = GET_PIO_PRIO(pio);
				max_pio = pio;
			}
		}
		if (max_pio != NULL)
			list_move_tail(&max_pio->list, &sorted_list);

		max = 0;
		max_pio = NULL;
	}
	list_splice_init(&sorted_list, pios);
}

static ssize_t pio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	LIST_HEAD(curr_pios);
	int curr_pio_cnt;
	struct pio_node curr_others;
	struct pio_node *pio;
	int len = 0;
	unsigned long flags;

	spin_lock_irqsave(&pio_list_lock, flags);
	list_replace_init(&pio_list, &curr_pios);
	curr_pio_cnt = pio_cnt;
	curr_others = others;
	memset(pio_hash, 0x0, sizeof(pio_hash));
	pio_cnt = 0;
	RESET_PIO_IO(&others);
	spin_unlock_irqrestore(&pio_list_lock, flags);

	if (curr_pio_cnt > SORT_PIO_NODE_NUM)
		sort_pios(&curr_pios);

	list_for_each_entry(pio, &curr_pios, list) {
		if (PAGE_SIZE - len > 80) {
			/* pid read(KB) write(KB) comm printed */
			len += scnprintf(buf + len, PAGE_SIZE - len,
					"%d %llu %llu %s\n",
					pio->tgid,
					pio->bytes[REQ_OP_READ] / 1024,
					pio->bytes[REQ_OP_WRITE] / 1024,
					(pio->name[0]) ? pio->name : "-");
			continue;
		}

		curr_others.bytes[REQ_OP_READ] += pio->bytes[REQ_OP_READ];
		curr_others.bytes[REQ_OP_WRITE] += pio->bytes[REQ_OP_WRITE];
		curr_others.bytes[REQ_OP_FLUSH] += pio->bytes[REQ_OP_FLUSH];
		curr_others.bytes[REQ_OP_DISCARD] += pio->bytes[REQ_OP_DISCARD];
	}

	if (curr_others.bytes[REQ_OP_READ] + curr_others.bytes[REQ_OP_WRITE])
		len += scnprintf(buf + len, PAGE_SIZE - len,
				"%d %llu %llu %s\n",
				curr_others.tgid,
				curr_others.bytes[REQ_OP_READ] / 1024,
				curr_others.bytes[REQ_OP_WRITE] / 1024,
				curr_others.name);

	free_pio_nodes(&curr_pios);
	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return len;
}

static ssize_t pio_enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	int ret;
	unsigned long flags;
	LIST_HEAD(curr_pios);

	ret = kstrtoint(buf, 10, &enable);
	if (ret)
		return ret;

	pio_enabled = (enable >= 1) ? 1 : 0;

	spin_lock_irqsave(&pio_list_lock, flags);
	list_replace_init(&pio_list, &curr_pios);
	memset(pio_hash, 0x0, sizeof(pio_hash));
	pio_cnt = 0;
	RESET_PIO_IO(&others);
	spin_unlock_irqrestore(&pio_list_lock, flags);

	free_pio_nodes(&curr_pios);
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
	int ret;

	ret = kstrtoint(buf, 10, &pio_duration_ms);
	if (ret)
		return ret;

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

static struct kobj_attribute pios_attr = __ATTR(pios, 0444, pio_show,  NULL);
static struct kobj_attribute pios_enable_attr = __ATTR(pios_enable, 0644,
		pio_enabled_show,  pio_enabled_store);
static struct kobj_attribute pios_duration_ms_attr = __ATTR(pios_duration_ms, 0644,
		pio_duration_ms_show, pio_duration_ms_store);

static const struct attribute *blk_sec_stat_pio_attrs[] = {
	&pios_attr.attr,
	&pios_enable_attr.attr,
	&pios_duration_ms_attr.attr,
	NULL,
};

int blk_sec_stat_pio_init(struct kobject *kobj)
{
	int retval;

	if (!kobj)
		return -EINVAL;

	pio_cache = kmem_cache_create("pio_node", sizeof(struct pio_node), 0, 0, NULL);
	if (!pio_cache)
		return -ENOMEM;

	retval = sysfs_create_files(kobj, blk_sec_stat_pio_attrs);
	if (retval) {
		kmem_cache_destroy(pio_cache);
		return retval;
	}

	return 0;
}

void blk_sec_stat_pio_exit(struct kobject *kobj)
{
	if (!kobj)
		return;

	sysfs_remove_files(kobj, blk_sec_stat_pio_attrs);

	if (pio_cache)
		kmem_cache_destroy(pio_cache);
}
