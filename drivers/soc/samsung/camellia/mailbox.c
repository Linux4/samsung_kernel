/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>

#include <drivers/soc/samsung/strong/strong_mailbox_common.h>
#include <drivers/soc/samsung/strong/strong_mailbox_ree.h>
#include <drivers/soc/samsung/strong/strong_error.h>

#include "mailbox.h"
#include "log.h"
#include "data_path.h"
#include "control_path.h"
#include "logsink.h"
#include "cache.h"

#define CM_CACHE_ORDER (4)
#define CM_CACHE_CNT (50)
#define CM_MAX_SIZE (1 << (CM_CACHE_ORDER + PAGE_SHIFT))
#define CM_USER_NAME_SIZE (16)

struct camellia_cache_header {
	struct list_head list;
	unsigned int size; /* debug fields */
	u64 time; /* debug fields */
	struct list_head dnode; /* specific field for data */
	uint8_t buf[];
};

atomic_t cache_cnt;
static LIST_HEAD(freelist);
static struct spinlock freelist_lock;
static struct camellia_cache_header *cacheptr_dbg[CM_CACHE_CNT];

int camellia_cache_cnt(bool loop_search)
{
	int ca_cnt = atomic_read(&cache_cnt);
	int buf_io_cnt;
	struct camellia_cache_header *tmp;
	int i;
	u8 *buf;

	if (likely(!loop_search))
		return ca_cnt;
	else {
		struct camellia_control_msg *cmsg;
		struct camellia_data_msg *dmsg;
		struct task_struct *dtask;
		char dstname[CM_USER_NAME_SIZE];
		u64 time;

		buf_io_cnt = 0;
		list_for_each_entry(tmp, &freelist, list)
			buf_io_cnt++;

		if (buf_io_cnt != ca_cnt)
			LOG_INFO("ssp cache cnt is different: %d / %d(%d)",
				buf_io_cnt, ca_cnt, atomic_read(&cache_cnt));
		else
			LOG_INFO("ssp cache cnt is %d", buf_io_cnt);

		/* check list user */
		for(i = 0; i < CM_CACHE_CNT; i++) {
			buf = &cacheptr_dbg[i]->buf[0];
			time = cacheptr_dbg[i]->time ?
				((ktime_get_boottime_ns() - cacheptr_dbg[i]->time) / NSEC_PER_MSEC) : 0;

			if (buf[0] == REE_CONTROL_FLAG) {
				cmsg = (void *)buf;

				LOG_INFO("cache[%d] for CTRL: cmd:%d, sub_cmd:%d during %lld ms",
					i, cmsg->cmd, cmsg->sub_cmd, time);
			} else {
				dmsg = (void *)buf;
				dtask = dmsg->header.destination ?
					find_task_by_vpid(dmsg->header.destination) : NULL;
				snprintf(dstname, CM_USER_NAME_SIZE, dtask ? dtask->comm : "No task");

				LOG_INFO("cache[%d] for DATA: src:%d, dst:%d(%s), len:%d during %lld ms",
					i, dmsg->header.source, dmsg->header.destination,
					dstname, dmsg->header.length, time);
			}
		}
		return buf_io_cnt;
	}
}

static void *camellia_cache_alloc_aligned(ssize_t size, unsigned int align)
{
	unsigned long flags;
	struct camellia_cache_header *hdr;
	void *buf = NULL;

	/* We currently always return cacheline aligned. */
	BUG_ON(align > CAMELLIA_CACHE_ALIGN);
	BUG_ON((size + sizeof(struct camellia_cache_header)) > CM_MAX_SIZE);

	/* Search free list. */
	spin_lock_irqsave(&freelist_lock, flags);

	if (!list_empty(&freelist)) {
		hdr = list_first_entry(&freelist, struct camellia_cache_header, list);
		list_del(&hdr->list);
		buf = hdr->buf;
		atomic_dec(&cache_cnt);
		hdr->time = ktime_get_boottime_ns();
		LOG_DBG("ssp Allocate memory %ld@%p", size, buf);
	} else {
		LOG_INFO("ssp Failed to allocate memory (%ld bytes)", size);
		camellia_cache_cnt(1);
	}
	spin_unlock_irqrestore(&freelist_lock, flags);

	return buf;
}

static void *camellia_cache_get_header(void *p)
{
	struct camellia_cache_header *hdr;

	if (!p) {
		LOG_ERR("ssp invalid input buffer with null pointer");
		return NULL;
	}

	hdr = container_of(p, struct camellia_cache_header, buf);
	if (!hdr) {
		LOG_ERR("ssp invalid header with null pointer");
		return NULL;
	}
	BUG_ON(hdr->size != CM_MAX_SIZE);

	return hdr;
}

void *camellia_cache_get_buf(void *p)
{
	struct camellia_cache_header *hdr = camellia_cache_get_header(p);

	LOG_DBG("ssp get buffer %p", p);
	if (hdr == NULL) {
		LOG_ERR("fails to get header");
		return NULL;
	}

	return &hdr->dnode;
}

void camellia_cache_free(void *p)
{
	struct camellia_cache_header *hdr = camellia_cache_get_header(p);
	unsigned long flags;

	LOG_DBG("ssp freeing memory %p", p);
	if (hdr == NULL) {
		LOG_ERR("fails to get header");
		return;
	}

	/* Merge with other free block, or put in list. */
	spin_lock_irqsave(&freelist_lock, flags);
	list_add_tail(&hdr->list, &freelist);
	atomic_inc(&cache_cnt);
	hdr->time = 0;
	spin_unlock_irqrestore(&freelist_lock, flags);
}

void *camellia_cache_alloc(ssize_t size)
{
	return camellia_cache_alloc_aligned(size, CAMELLIA_CACHE_ALIGN);
}

static int camellia_mailbox_receive(void *buf, unsigned int len, unsigned int status)
{
	static char *msg_data = NULL;
	static unsigned char msg_flag;
	static unsigned int msg_read = 0;
	static unsigned int msg_len = 0;
	int ret = -EINVAL;

	LOG_ENTRY;

	LOG_DBG("Receive message, len = (%u), status = (%u)", len, status);

	if (buf == NULL || len == 0 || status != 0) {
		LOG_ERR("Invalid inputs: %p, %u, %u", buf, len, status);
		goto free_memory;
	}

	LOG_DUMP_BUF(buf, len);

	if (!msg_data) {
		msg_data = buf;
		msg_flag = msg_data[0];
		msg_read = 0;

		if (msg_flag == REE_CONTROL_FLAG) {
			msg_len = sizeof(struct camellia_control_msg);
		} else if (msg_flag == REE_DATA_FLAG) {
			msg_len = ((struct camellia_data_msg *)msg_data)->header.length +
						sizeof(struct camellia_msg_flag) +
						sizeof(struct camellia_data_msg_header);
		} else {
			LOG_ERR("Invalid message flag: %x", msg_flag);
			msg_data = NULL;
			ret = -EPROTO;
			goto free_memory;
		}

		if (msg_len == len) {
			buf = NULL;
			msg_read = len;
		} else {
			msg_data = camellia_cache_alloc(msg_len);
			if (!msg_data) {
				ret = -ENOMEM;
				goto free_memory;
			}
		}
	}

	if (buf) {
		memcpy(msg_data + msg_read, buf, len);
		camellia_cache_free(msg_data);
		msg_read += len;
	}

	LOG_DBG("msg_read = %d, msg_len = %d, len = %d", msg_read, msg_len, len);

	if (msg_read == msg_len) {
		if (msg_flag == REE_CONTROL_FLAG) {
			camellia_dispatch_control_msg((struct camellia_control_msg *)msg_data);
		} else {
			camellia_dispatch_data_msg((struct camellia_data_msg *)msg_data);
		}

		msg_data = NULL;
	} else {
		LOG_ERR("Invalid message frame, expected %ld, but %ld received", msg_len, msg_read);
	}

	return 0;

free_memory:
	camellia_cache_free(buf);

	return ret;
}

int camellia_mailbox_init(void)
{
	int i;

	spin_lock_init(&freelist_lock);
	for (i = 0; i < CM_CACHE_CNT; i++) {
		struct camellia_cache_header *hd;
		unsigned long pg = __get_free_pages(GFP_KERNEL, CM_CACHE_ORDER);
		if (!pg) {
			return -1;
		}

		hd = (struct camellia_cache_header *)pg;
		hd->size = 1 << (CM_CACHE_ORDER + PAGE_SHIFT);
		list_add(&hd->list, &freelist);
		LOG_INFO("Cache: %d bytes allocated", hd->size);
		cacheptr_dbg[i] = hd;
	}
	atomic_set(&cache_cnt, CM_CACHE_CNT);
	mailbox_register_allocator(camellia_cache_alloc);
	mailbox_register_message_receiver(camellia_mailbox_receive);
	mailbox_register_message_mlog_writer(camellia_write_log_file);

	return 0;
}

void camellia_mailbox_deinit(void)
{
}
