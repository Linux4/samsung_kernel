/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
*/

#include <linux/cred.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#ifdef DEFEX_LOG_BUFFER_ENABLE
#include <linux/list.h>
#include <linux/rculist.h>
#endif /* DEFEX_LOG_BUFFER_ENABLE */
#include <linux/sched.h>
#include <linux/slab.h>
#ifdef DEFEX_LOG_BUFFER_ENABLE
#include <linux/spinlock.h>
#endif /* DEFEX_LOG_BUFFER_ENABLE */
#include <linux/string.h>
#include <linux/sysfs.h>
#include "include/defex_debug.h"
#include "include/defex_internal.h"

static int last_cmd;

#ifdef DEFEX_LOG_BUFFER_ENABLE
unsigned char *log_buf;
DEFINE_SPINLOCK(log_buf_lock);
static unsigned int log_buf_in_index, log_buf_out_index;
static unsigned long log_buf_size, log_buf_mask;

static int log_buf_init(void)
{
	log_buf_size = LOG_BUF_SIZE;
	do {
		log_buf = vmalloc(log_buf_size);
		if (log_buf)
			break;
		pr_crit("[DEFEX] Log buffer allocation failed %ld bytes.\n", log_buf_size);
		log_buf_size >>= 1;
	} while (log_buf_size > PAGE_SIZE);
	if (!log_buf)
		return -1;

	pr_info("[DEFEX] Log buffer size set to %ld bytes.\n", log_buf_size);

	log_buf_in_index = 0;
	log_buf_out_index = 0;
	log_buf_mask = log_buf_size - 1;

	log_buffer_msg("[DEFEX] Logging started...");
	return 0;
}

static int log_buffer_put(const char *msg, int msg_len)
{
	char c;
	int avail, ret = -1;

	if (!log_buf_size) {
		pr_info("[DEFEX] %s", msg);
		return ret;
	}

	spin_lock(&log_buf_lock);
	avail = log_buf_size - ((log_buf_in_index - log_buf_out_index) & log_buf_mask);
	if (avail > msg_len) {
		do {
			c = *msg++;
			log_buf[log_buf_in_index++] = c;
			log_buf_in_index &= log_buf_mask;
		} while (--msg_len);
		if (c != '\n') {
			log_buf[log_buf_in_index++] = '\n';
			log_buf_in_index &= log_buf_mask;
		}
		ret = 0;
	}
	spin_unlock(&log_buf_lock);
	return ret;
}

static int log_buffer_get(char *str, int buff_size)
{
	char value;
	int index = 0;
	unsigned int log_buf_out_local;

	if (log_buf_in_index == log_buf_out_index || buff_size < 2)
		return 0;

	spin_lock(&log_buf_lock);
	log_buf_out_local = log_buf_out_index;
	do {
		value = log_buf[log_buf_out_local++];
		str[index++] = value;
		log_buf_out_local &= log_buf_mask;
		if (value == '\n') {
			str[index] = 0;
			log_buf_out_index = log_buf_out_local;
			spin_unlock(&log_buf_lock);
			return index;
		}
	} while (--buff_size > 1 && (log_buf_in_index != log_buf_out_local));
	spin_unlock(&log_buf_lock);
	return 0;
}

int log_buffer_msg(const char *format, ...)
{
	char msg[MAX_LEN];
	int msg_len;
	va_list aptr;
	ktime_t cur_time;

	cur_time = ktime_get_boottime();
	msg_len = snprintf(msg, MAX_LEN, "[% 4lld.%04lld] ", cur_time/1000000000, (cur_time%1000000000)/100000);

	va_start(aptr, format);
	msg_len += vsnprintf(msg + msg_len, MAX_LEN - msg_len, format, aptr);
	va_end(aptr);
	return log_buffer_put(msg, msg_len);
}

int log_buffer_printf(const char *format, ...)
{
	char msg[MAX_LEN];
	int msg_len;
	va_list aptr;

	va_start(aptr, format);
	msg_len = vsnprintf(msg, MAX_LEN, format, aptr);
	va_end(aptr);
	return log_buffer_put(msg, msg_len);
}
#else
int log_buffer_msg(const char *format, ...)
{
	char msg[MAX_LEN];
	int msg_len;
	va_list aptr;
	ktime_t cur_time;

	cur_time = ktime_get_boottime();
	msg_len = snprintf(msg, MAX_LEN, "[% 4lld.%04lld] ", cur_time/1000000000, (cur_time%1000000000)/100000);

	va_start(aptr, format);
	msg_len += vsnprintf(msg + msg_len, MAX_LEN - msg_len, format, aptr);
	va_end(aptr);
	pr_info("[DEFEX] %s", msg);
	return 0;
}

int log_buffer_printf(const char *format, ...)
{
	char msg[MAX_LEN];
	int msg_len;
	va_list aptr;

	va_start(aptr, format);
	msg_len = vsnprintf(msg, MAX_LEN, format, aptr);
	va_end(aptr);
	pr_info("[DEFEX] %s", msg);
	return 0;
}
#endif /* DEFEX_LOG_BUFFER_ENABLE */

void blob(const char *buffer, const size_t bufLen, const int lineSize)
{
	size_t i = 0, line;
	size_t j = 0, len = bufLen;
	int offset = 0;
	char c, stringToPrint[MAX_DATA_LEN];

	do {
		line = (len > lineSize)?lineSize:len;
		offset  = 0;
		offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, "| 0x%08lX | ", i);

		for (j = 0; j < line; j++)
			offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, "%02X ",
							   (unsigned char)buffer[i + j]);
		if (line < lineSize) {
			for (j = 0; j < lineSize - line; j++)
				offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, "   ");
		}
		offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, "| ");

		for (j = 0; j < line; j++) {
			c = buffer[i + j];
			c = (c < 0x20) || (c >= 0x7F)?'.':c;
			offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, "%c", c);
		}
		if (line < lineSize) {
			for (j = 0; j < lineSize - line; j++)
				offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, " ");
		}

		offset += snprintf(stringToPrint + offset, MAX_DATA_LEN - offset, " |");
		pr_info("%s\n", stringToPrint);
		memset(stringToPrint, 0, MAX_DATA_LEN);
		i += line;
		len -= line;
	} while (len);
}

__visible_for_testing int set_user(struct cred *new_cred)
{
	struct user_struct *new_user;

	new_user = alloc_uid(new_cred->uid);
	if (!new_user)
		return -EAGAIN;

	free_uid(new_cred->user);
	new_cred->user = new_user;
	return 0;
}

/*
 * target_id = (0 - set all uids, 1 - set fsuid, 2 - set all gids)
 */
__visible_for_testing int set_cred(int target_id, int new_val)
{
	struct user_namespace *ns = current_user_ns();
	const struct cred *old_cred;
	struct cred *new_cred;
	kuid_t kuid;
	kgid_t kgid;
	uid_t new_uid;
	gid_t new_gid;

	new_cred = prepare_creds();
	if (!new_cred)
		return -ENOMEM;
	old_cred = current_cred();

	switch (target_id) {
	case 0:
		new_uid = new_val;
		kuid = make_kuid(ns, new_uid);
		if (!uid_valid(kuid))
			goto do_abort;
		new_cred->uid = new_cred->euid = new_cred->suid = new_cred->fsuid = kuid;
		if (!uid_eq(new_cred->uid, old_cred->uid)) {
			if (set_user(new_cred) < 0)
				goto do_abort;
		}
		break;

	case 1:
		new_uid = new_val;
		kuid = make_kuid(ns, new_uid);
		if (!uid_valid(kuid))
			goto do_abort;
		new_cred->fsuid = kuid;
		break;

	case 2:
		new_gid = new_val;
		kgid = make_kgid(ns, new_gid);
		if (!gid_valid(kgid))
			goto do_abort;
		new_cred->gid = new_cred->egid = new_cred->sgid = new_cred->fsgid = kgid;
		break;
	}
	return commit_creds(new_cred);

do_abort:
	abort_creds(new_cred);
	return -EPERM;
}

__visible_for_testing ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct task_struct *p = current;
	int i, l, new_val = -1;
	int ret = 0;

	static const char *prefix[] = {
		"uid=",
		"fsuid=",
		"gid=",
		"pe_status=",
		"im_status=",
		"sp_status=",
		"int_status=",
		"get_log"
	};

	if (!buf || !p)
		return -EINVAL;

	for (i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++) {
		l = strlen(prefix[i]);
		if (!strncmp(buf, prefix[i], l))
			break;
	}
	if (i == (sizeof(prefix) / sizeof(prefix[0])))
		return -EINVAL;

	switch (i) {
	case DBG_SETUID ... DBG_SETGID:
		ret = kstrtoint(buf + l, 10, &new_val);
		if (ret != 0)
			return -EINVAL;
		ret = set_cred(i, new_val);
		break;
	case DBG_SET_PE_STATUS:
		privesc_status_store(buf + l);
		break;
	case DBG_SET_IM_STATUS:
#ifdef DEFEX_IMMUTABLE_ENABLE
		immutable_status_store(buf + l);
#endif /* DEFEX_IMMUTABLE_ENABLE */
		break;
	case DBG_SET_SP_STATUS:
		safeplace_status_store(buf + l);
		break;
	case DBG_SET_INT_STATUS:
		integrity_status_store(buf + l);
		break;
	case DBG_GET_LOG:
		break;
	default:
		break;
	}

	last_cmd = i;
	return (!ret)?count:ret;
}

__visible_for_testing ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct task_struct *p = current;
	int res = 0;
#ifdef DEFEX_LOG_BUFFER_ENABLE
	int buff_size, msg_len;
#endif /* DEFEX_LOG_BUFFER_ENABLE */

	if (!p)
		return 0;

	switch (last_cmd) {
	case DBG_SETUID ... DBG_SETGID:
		res = snprintf(buf, MAX_LEN + 1, "pid=%d\nuid=%d\ngid=%d\neuid=%d\negid=%d\n",
			p->pid,
			uid_get_value(p->cred->uid),
			uid_get_value(p->cred->gid),
			uid_get_value(p->cred->euid),
			uid_get_value(p->cred->egid));
		break;
	case DBG_GET_LOG:
#ifdef DEFEX_LOG_BUFFER_ENABLE
		buff_size = PAGE_SIZE;
		do {
			msg_len = log_buffer_get(&buf[res], buff_size);
			res += msg_len;
			buff_size -= msg_len;
		} while (msg_len && buff_size >= MAX_LEN);
		if (!msg_len)
			res += snprintf(&buf[res], buff_size, "=== EOF ===\n");
		else
			res += snprintf(&buf[res], buff_size, "=== %lu bytes left ===\n",
				(unsigned long)(log_buf_in_index - log_buf_out_index) & LOG_BUF_MASK);
#else
		res = snprintf(buf, MAX_LEN + 1, "Log buffer disabled...\n");
#endif /* DEFEX_LOG_BUFFER_ENABLE */
		break;
	}

	return res;
}

__visible_for_testing struct kobj_attribute debug_attribute = __ATTR(debug, 0660, debug_show, debug_store);

int defex_create_debug(struct kset *defex_kset)
{
	int retval;

#ifdef DEFEX_LOG_BUFFER_ENABLE
	retval = log_buf_init();
	if (retval)
		pr_crit("[DEFEX] Log buffer init failed.\n");
#endif /* DEFEX_LOG_BUFFER_ENABLE */
	last_cmd = DBG_GET_LOG;

	retval = sysfs_create_file(&defex_kset->kobj, &debug_attribute.attr);
	if (retval)
		return DEFEX_NOK;

	kobject_uevent(&defex_kset->kobj, KOBJ_ADD);
	return DEFEX_OK;
}
