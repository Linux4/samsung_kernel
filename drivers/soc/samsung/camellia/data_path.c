/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_CAMELLIA_LOG_DMESG)
#include <linux/string.h>
#endif

#include <drivers/soc/samsung/strong/strong_mailbox_common.h>
#include <drivers/soc/samsung/strong/strong_mailbox_ree.h>

#include "pm.h"
#include "log.h"
#include "utils.h"
#include "data_path.h"
#include "cache.h"

extern struct camellia_device camellia_dev;

static LIST_HEAD(peer_list);
static DEFINE_RWLOCK(peer_list_lock);
static struct kmem_cache *peer_cache;
static const u32 REE_DMESG_PRINT = 0;
static const u32 LOGGER_SERVICE_ID = 30;

struct camellia_data_peer {
	u32 id;
	ssize_t pos;
	struct list_head msg_queue;
	wait_queue_head_t msg_wait_queue;
	struct spinlock msg_queue_lock;
	struct kref refcount;
	struct list_head node;
};

struct camellia_data_msg_entry {
	struct list_head node;
	u8 msg[];
};

void camellia_data_peer_release(struct kref *ref)
{
	struct camellia_data_peer *peer = container_of(ref,
									  struct camellia_data_peer,
									  refcount);

	LOG_ENTRY;

	kmem_cache_free(peer_cache, peer);
}

static struct camellia_data_peer *camellia_get_data_peer(unsigned int id)
{
	struct camellia_data_peer *peer = NULL;
	list_for_each_entry (peer, &peer_list, node) {
		if (peer->id == id)
			return peer;
	}

	return NULL;
}

void camellia_log_print(const struct camellia_data_msg *msg)
{
#if IS_ENABLED(CONFIG_CAMELLIA_LOG_DMESG)
	char *const delimiter = "\x0a"; // This means newline
	char *token;
	char *cur;
	char *print_msg = NULL;

	if (msg->header.source != LOGGER_SERVICE_ID)
		return;

	// The function prints camellia kernel console messages(println!, print!)
	// and secure app console, log messages(debug!, info! etc)
	if (msg->payload[0] == 0x0) {
		print_msg = cur = kstrndup(msg->payload + 1, msg->header.length - 1,
								   GFP_KERNEL);
		while ((token = strsep(&cur, delimiter))) {
			if (strlen(token) != 0)
				CAMELLIA_PRINT(" %s", token);
		}
	} else if (msg->payload[0] != 0x1) {
		print_msg = cur = kstrndup(msg->payload, msg->header.length, GFP_KERNEL);
		while ((token = strsep(&cur, delimiter))) {
			if (strlen(token) != 0)
				CAMELLIA_PRINT(" %s", token);
		}
	}

	if (print_msg)
		kfree(print_msg);
#endif
}

void camellia_dispatch_data_msg(struct camellia_data_msg *msg)
{
	struct camellia_data_msg_entry *msg_entry = NULL;
	struct camellia_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;

	LOG_ENTRY;

#if IS_ENABLED(CONFIG_CAMELLIA_LOG_DMESG)
	if (msg->header.destination == REE_DMESG_PRINT) {
		camellia_log_print(msg);
		camellia_cache_free(msg);
		return;
	}
#endif

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = camellia_get_data_peer(msg->header.destination);
	if (!peer) {
		LOG_ERR("Received SEE response to inactive session, dest = %d",
				msg->header.destination);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		camellia_cache_free(msg);
		return;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	msg_entry = camellia_cache_get_buf(msg);
	if (!msg_entry) {
		LOG_ERR("Unable to allocate message list entry");
		return;
	}

	camellia_log_print(msg);

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	list_add_tail(&msg_entry->node, &peer->msg_queue);
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	wake_up_interruptible(&peer->msg_wait_queue);

	kref_put(&peer->refcount, camellia_data_peer_release);
}

unsigned int camellia_poll_wait_data_msg(unsigned int id,
		struct file *file,
		struct poll_table_struct *poll_tab)
{
	struct camellia_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;
	unsigned int mask = 0;
	int is_empty;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = camellia_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		return -ENOENT;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	poll_wait(file, &peer->msg_wait_queue, poll_tab);

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	is_empty = list_empty(&peer->msg_queue);
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	kref_put(&peer->refcount, camellia_data_peer_release);

	if (!is_empty)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

size_t camellia_send_data_msg(unsigned int id, struct camellia_data_msg *msg)
{
	int ret;
	int msg_len;
	uint32_t len_out;
	uint32_t offset;
	int i;
	static u8 send_cnt;

	msg->flag.value = REE_DATA_FLAG;
	msg->header.source = id;

	msg_len = sizeof(struct camellia_msg_flag) +
			  sizeof(struct camellia_data_msg_header) +
			  msg->header.length;

	LOG_DUMP_BUF((uint8_t *)msg, msg_len);

	del_timer_sync(&camellia_dev.pm_timer);

	mutex_lock(&camellia_dev.lock);

	if (camellia_dev.state == CAMELLIA_STATE_OFFLINE) {
		camellia_dev.state = CAMELLIA_STATE_UP;
		if (strong_power_on()) {
			camellia_dev.state = CAMELLIA_STATE_OFFLINE;
			mutex_unlock(&camellia_dev.lock);
			LOG_ERR("Failed to turn on STRONG for sending msg");
			return 0;
		}
		camellia_dev.state = CAMELLIA_STATE_ONLINE;
	}

	send_cnt++;
	send_cnt &= SEQ_MASK;
	msg->flag.value |= (send_cnt << SEQ_SHIFT);
	LOG_DBG("Send, flag = (%x)\n", msg->flag.value);
	offset = 0;
	len_out = 0;
	for (i = 0; offset < msg_len && i < MAX_NUMBER_OF_RETRY; i++) {
		ret = mailbox_send((uint8_t *)(msg + offset), (msg_len - offset), &len_out);
		offset = offset + len_out;
		if (ret == RV_MB_WAIT1_TIMEOUT) {
			LOG_ERR("Send Timeout, total = (%d), offset = (%d), this turn = (%d), remain = (%d), try = (%d)\n",
					msg_len, offset, len_out, (msg_len - offset), i + 1);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(msecs_to_jiffies(1000 * (i + 1)));
			if (signal_pending(current)) {
				LOG_ERR("Task receives signal. id = (%d)\n", current->pid);
				break;
			}
			continue;
		} else if (ret == RV_MB_WAIT2_TIMEOUT) {
			LOG_ERR("Send Timeout[2]\n");
			break;
		} else {
			break;
		}
	}

	mutex_unlock(&camellia_dev.lock);

	LOG_INFO("Send Message, perr = (%u:%u), source = (%d), msg_len = (%d), ret = (%d) offset = (%d)",
			 id, current->pid, msg->header.source, msg_len, ret, offset);

	return offset;
}

int camellia_is_data_msg_queue_empty(unsigned int id)
{
	struct camellia_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;
	int is_empty = 1;

	LOG_ENTRY;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = camellia_get_data_peer(id);
	if (peer) {
		spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
		is_empty = list_empty(&peer->msg_queue);
		spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
	} else
		LOG_ERR("Peer %u dose not exist", id);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	return is_empty;
}

int camellia_read_data_msg_timeout(unsigned int id, char __user *buffer,
								   size_t len, uint64_t timeout_jiffies)
{
	struct camellia_data_msg_entry *msg_entry = NULL;
	struct camellia_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;
	size_t msg_len, read;
	int handled = 0;
	int ret;
	struct camellia_data_msg *msg;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = camellia_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		return -ENOENT;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	ret = wait_event_interruptible_timeout(
			  peer->msg_wait_queue, !list_empty(&peer->msg_queue),
			  timeout_jiffies);

	if (ret == 0) {
		kref_put(&peer->refcount, camellia_data_peer_release);
		LOG_ERR("Peer %u's waiting for read was timeout", id);
		return -ETIME;
	} else if (ret == -ERESTARTSYS) {
		kref_put(&peer->refcount, camellia_data_peer_release);
		LOG_ERR("Peer %u's waiting was canceled", id);
		return -ERESTARTSYS;
	}

	while (handled < len) {
		spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
		msg_entry = list_first_entry(&peer->msg_queue,
									 struct camellia_data_msg_entry, node);

		if (!msg_entry) {
			//TODO consider using wait_event_interruptible_timeout again
			//     if handled is zero.
			spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
			break;
		}

		msg = (struct camellia_data_msg *)&msg_entry->msg[0];
		msg_len =msg->header.length + sizeof(struct camellia_data_msg_header);

		LOG_DUMP_BUF(msg_entry->msg, sizeof(struct camellia_msg_flag) + msg_len);

		read = min(len - handled, msg_len - peer->pos);
		if (copy_to_user(buffer + handled,
						 ((char *)msg_entry->msg) + peer->pos +
						 sizeof(struct camellia_msg_flag),
						 read)) {
			LOG_ERR("Unable to copy message to userspace");
			spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
			return -EFAULT;
		}

		handled += read;
		peer->pos += read;

		if (peer->pos == msg_len) {
			list_del(&msg_entry->node);
			camellia_cache_free(msg_entry->msg);
			peer->pos = 0;

			if (list_empty(&peer->msg_queue))
				len = handled;
		}
		spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
	}
	kref_put(&peer->refcount, camellia_data_peer_release);

	LOG_INFO("Peer %u, Read finished, final cnt = %u", id, handled);
	return handled;
}

int camellia_data_peer_create(unsigned int id)
{
	struct camellia_data_peer *peer = NULL;
	unsigned long flags;
	int ret = 0;

	write_lock_irqsave(&peer_list_lock, flags);

	peer = camellia_get_data_peer(id);
	if (peer)
		goto out;

	LOG_DBG("Create new peer %u", id);

	peer = kmem_cache_alloc(peer_cache, GFP_KERNEL);
	if (!peer) {
		LOG_ERR("Failed to allocate peer %u", id);
		ret = -ENOMEM;
		goto out;
	}

	peer->id = id;
	peer->pos = 0;

	INIT_LIST_HEAD(&peer->msg_queue);
	spin_lock_init(&peer->msg_queue_lock);
	init_waitqueue_head(&peer->msg_wait_queue);

	kref_init(&peer->refcount);

	list_add_tail(&peer->node, &peer_list);

out:
	write_unlock_irqrestore(&peer_list_lock, flags);
	return ret;
}

void camellia_data_peer_destroy(unsigned int id)
{
	struct camellia_data_msg_entry *msg_entry = NULL;
	struct camellia_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;

	write_lock_irqsave(&peer_list_lock, flags_peer);

	peer = camellia_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		write_unlock_irqrestore(&peer_list_lock, flags_peer);
		return;
	}

	LOG_DBG("Delete peer %u", id);
	list_del(&peer->node);

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	while (!list_empty(&peer->msg_queue)) {
		msg_entry = list_first_entry(&peer->msg_queue,
									 struct camellia_data_msg_entry, node);
		list_del(&msg_entry->node);
		camellia_cache_free(msg_entry->msg);
	}
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	kref_put(&peer->refcount, camellia_data_peer_release);

	write_unlock_irqrestore(&peer_list_lock, flags_peer);
}

int camellia_data_path_init(void)
{
	peer_cache = kmem_cache_create("camellia-data-peer",
								   sizeof(struct camellia_data_peer),
								   0,
								   SLAB_HWCACHE_ALIGN,
								   NULL
								  );
	if (!peer_cache) {
		LOG_ERR("Unable to initialize cache for data peers");
		return -1;
	}

	return 0;
}

void camellia_data_path_deinit(void)
{
	kmem_cache_destroy(peer_cache);
}

