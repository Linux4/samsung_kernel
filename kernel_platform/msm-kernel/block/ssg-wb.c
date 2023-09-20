// SPDX-License-Identifier: GPL-2.0
/*
 *  Write Booster of SamSung Generic I/O scheduler
 *
 *  Copyright (C) 2022 Jisoo Oh <jisoo2146.oh@samsung.com>
 */

#include <linux/blkdev.h>
#include <linux/sbitmap.h>
#include <linux/blk-mq.h>
#include "blk-sec-stats.h"
#include "blk-mq-tag.h"
#include "ssg.h"
#include "../drivers/scsi/ufs/ufs-sec-feature.h"

struct ssg_wb_data {
	int off_delay_jiffies;
	int on_threshold_bytes;
	int on_threshold_rqs;
	int on_threshold_sync_write_bytes;
	int off_threshold_sync_write_bytes;
	int off_threshold_bytes;
	int off_threshold_rqs;
	atomic_t wb_trigger;
	struct request_queue *queue;
	struct workqueue_struct *wb_ctrl_wq;
	struct delayed_work wb_ctrl_work;
	struct delayed_work wb_deferred_off_work;
};

struct io_amount_data {
	unsigned int allocated_tags;
	unsigned int write_bytes;
	unsigned int sync_write_bytes;
};

struct ssg_wb_iter_data {
	struct blk_mq_tags *tags;
	void *data;
	bool reserved;
};

static const int off_delay_msecs = 5000;
static const int on_threshold_bytes = 30*1024*1024;
static const int on_threshold_ratio = 90;
static const int on_threshold_sync_write_bytes = 2*1024*1024;
static const int off_threshold_sync_write_bytes = 1*1024*1024;
static const int off_threshold_bytes = 4*1024*1024;
static const int off_threshold_ratio = 40;

#define wb_may_try_on(io_amount, ssg_wb) \
	(io_amount).allocated_tags >= (ssg_wb)->on_threshold_rqs || \
	(io_amount).write_bytes >= (ssg_wb)->on_threshold_bytes || \
	(io_amount).sync_write_bytes >= (ssg_wb)->on_threshold_sync_write_bytes
#define wb_may_try_off(io_amount, ssg_wb) \
	(io_amount).allocated_tags < (ssg_wb)->off_threshold_rqs && \
	(io_amount).write_bytes < (ssg_wb)->off_threshold_bytes && \
	(io_amount).sync_write_bytes < (ssg_wb)->off_threshold_sync_write_bytes

static void wb_try_on(struct ssg_wb_data *ssg_wb)
{
	cancel_delayed_work_sync(&ssg_wb->wb_deferred_off_work);

	ufs_sec_wb_ctrl(WB_ON, ssg_wb->queue->queuedata);

	atomic_set(&ssg_wb->wb_trigger, true);
}

static void wb_try_off_work(struct work_struct *work)
{
	struct ssg_wb_data *ssg_wb = container_of(to_delayed_work(work),
						struct ssg_wb_data, wb_deferred_off_work);

	ufs_sec_wb_ctrl(WB_OFF, ssg_wb->queue->queuedata);
}

static void wb_try_off(struct ssg_wb_data *ssg_wb)
{
	queue_delayed_work(ssg_wb->wb_ctrl_wq,
				&ssg_wb->wb_deferred_off_work, ssg_wb->off_delay_jiffies);

	atomic_set(&ssg_wb->wb_trigger, false);
}

static bool wb_count_io(struct sbitmap *bitmap, unsigned int bitnr, void *data)
{
	struct ssg_wb_iter_data *iter_data = data;
	struct blk_mq_tags *tags = iter_data->tags;
	struct io_amount_data *io_amount = iter_data->data;
	bool reserved = iter_data->reserved;
	struct request *rq;

	if (!reserved)
		bitnr += tags->nr_reserved_tags;

	rq = tags->static_rqs[bitnr];
	if (!rq)
		return true;

	io_amount->allocated_tags++;
	if (req_op(rq) == REQ_OP_WRITE) {
		io_amount->write_bytes += blk_rq_bytes(rq);
		if (rq->cmd_flags & REQ_SYNC)
			io_amount->sync_write_bytes += blk_rq_bytes(rq);
	}

	return true;
}

static void wb_get_io_amount(struct request_queue *q, struct io_amount_data *io_amount)
{
	struct blk_mq_hw_ctx *hctx = *q->queue_hw_ctx;
	struct blk_mq_tags *tags = hctx->sched_tags;
	struct ssg_wb_iter_data iter_data = {
		.tags = tags,
		.data = io_amount,
	};

	if (tags->nr_reserved_tags) {
		iter_data.reserved = true;
		sbitmap_for_each_set(&tags->breserved_tags->sb, wb_count_io, &iter_data);
	}

	iter_data.reserved = false;
	sbitmap_for_each_set(&tags->bitmap_tags->sb, wb_count_io, &iter_data);
}

static void wb_ctrl_work(struct work_struct *work)
{
	struct ssg_wb_data *ssg_wb = container_of(to_delayed_work(work),
						struct ssg_wb_data, wb_ctrl_work);
	struct io_amount_data io_amount = {
		.allocated_tags = 0,
		.write_bytes = 0,
	};

	wb_get_io_amount(ssg_wb->queue, &io_amount);

	if (atomic_read(&ssg_wb->wb_trigger)) {
		if (wb_may_try_off(io_amount, ssg_wb))
			wb_try_off(ssg_wb);
	} else {
		if (wb_may_try_on(io_amount, ssg_wb))
			wb_try_on(ssg_wb);
	}

	if (atomic_read(&ssg_wb->wb_trigger))
		queue_delayed_work(ssg_wb->wb_ctrl_wq, &ssg_wb->wb_ctrl_work,
				ssg_wb->off_delay_jiffies);
}

void ssg_wb_ctrl(struct ssg_data *ssg)
{
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return;

	if (atomic_read(&ssg_wb->wb_trigger))
		return;

	if (!work_busy(&ssg_wb->wb_ctrl_work.work))
		queue_delayed_work(ssg_wb->wb_ctrl_wq, &ssg_wb->wb_ctrl_work, 0);
}

void ssg_wb_init(struct ssg_data *ssg)
{
	struct ssg_wb_data *ssg_wb;
	char ssg_prefix[20] = "ssg_";
	struct gendisk *gd = ssg->queue->disk;

	if (!gd)
		return;

	if (!is_internal_disk(gd))
		return;

	if (!ufs_sec_is_wb_supported(ssg->queue->queuedata))
		return;

	ssg_wb = kzalloc(sizeof(*ssg_wb), GFP_KERNEL);
	if (!ssg_wb)
		return;
	ssg->wb_data = ssg_wb;

	ssg_wb->wb_ctrl_wq = create_freezable_workqueue(strncat(ssg_prefix, gd->disk_name, 3));
	INIT_DELAYED_WORK(&ssg_wb->wb_ctrl_work, wb_ctrl_work);
	INIT_DELAYED_WORK(&ssg_wb->wb_deferred_off_work, wb_try_off_work);

	ssg_wb->off_delay_jiffies = msecs_to_jiffies(off_delay_msecs);
	ssg_wb->on_threshold_bytes = on_threshold_bytes;
	ssg_wb->on_threshold_rqs = ssg->queue->nr_requests * on_threshold_ratio / 100U;
	ssg_wb->on_threshold_sync_write_bytes = on_threshold_sync_write_bytes;
	ssg_wb->off_threshold_sync_write_bytes = off_threshold_sync_write_bytes;
	ssg_wb->off_threshold_bytes = off_threshold_bytes;
	ssg_wb->off_threshold_rqs = ssg->queue->nr_requests * off_threshold_ratio / 100U;
	ssg_wb->queue = ssg->queue;

	atomic_set(&ssg_wb->wb_trigger, false);
}

void ssg_wb_exit(struct ssg_data *ssg)
{
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return;

	cancel_delayed_work_sync(&ssg_wb->wb_ctrl_work);
	cancel_delayed_work_sync(&ssg_wb->wb_deferred_off_work);

	if (atomic_read(&ssg_wb->wb_trigger))
		ufs_sec_wb_ctrl(WB_OFF, ssg_wb->queue->queuedata);

	ssg->wb_data = NULL;
	kfree(ssg_wb);
}

/* sysfs */
ssize_t ssg_wb_off_delay_msecs_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", jiffies_to_msecs(ssg_wb->off_delay_jiffies));
}

ssize_t ssg_wb_off_delay_msecs_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= 0)
		ssg_wb->off_delay_jiffies = msecs_to_jiffies(val);

	return count;
}

ssize_t ssg_wb_on_threshold_rqs_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->on_threshold_rqs);
}

ssize_t ssg_wb_on_threshold_rqs_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= ssg_wb->off_threshold_rqs)
		ssg_wb->on_threshold_rqs = val;

	return count;
}

ssize_t ssg_wb_on_threshold_bytes_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->on_threshold_bytes);
}

ssize_t ssg_wb_on_threshold_bytes_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= ssg_wb->off_threshold_bytes)
		ssg_wb->on_threshold_bytes = val;

	return count;
}

ssize_t ssg_wb_off_threshold_rqs_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->off_threshold_rqs);
}

ssize_t ssg_wb_off_threshold_rqs_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= 0 && val <= ssg_wb->on_threshold_rqs)
		ssg_wb->off_threshold_rqs = val;

	return count;
}


ssize_t ssg_wb_off_threshold_bytes_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->off_threshold_bytes);
}

ssize_t ssg_wb_off_threshold_bytes_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= 0 && val <= ssg_wb->on_threshold_bytes)
		ssg_wb->off_threshold_bytes = val;

	return count;
}

ssize_t ssg_wb_on_threshold_sync_write_bytes_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->on_threshold_sync_write_bytes);
}

ssize_t ssg_wb_on_threshold_sync_write_bytes_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= ssg_wb->off_threshold_sync_write_bytes)
		ssg_wb->on_threshold_sync_write_bytes = val;

	return count;
}

ssize_t ssg_wb_off_threshold_sync_write_bytes_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", ssg_wb->off_threshold_sync_write_bytes);
}

ssize_t ssg_wb_off_threshold_sync_write_bytes_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;
	int val, ret;

	if (!ssg_wb)
		return 0;

	ret = kstrtoint(page, 10, &val);
	if (ret)
		return ret;

	if (val >= 0 && val <= ssg_wb->on_threshold_sync_write_bytes)
		ssg_wb->off_threshold_sync_write_bytes = val;

	return count;
}

ssize_t ssg_wb_trigger_show(struct elevator_queue *e, char *page)
{
	struct ssg_data *ssg = e->elevator_data;
	struct ssg_wb_data *ssg_wb = ssg->wb_data;

	if (!ssg_wb)
		return 0;

	return snprintf(page, PAGE_SIZE, "%d\n", atomic_read(&ssg_wb->wb_trigger));
}
