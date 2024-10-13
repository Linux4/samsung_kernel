/*
 *  drivers/usb/notify/usblog_proc_notify.c
 *
 * Copyright (C) 2016 Samsung, Inc.
 * Author: Dongrak Shin <dongrak.shin@samsung.com>
 *
*/

 /* usb notify layer v2.0 */

 #define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/usb_notify.h>

#define USBLOG_MAX_BUF_SIZE	(1 << 7) /* 128 */
#define USBLOG_MAX_STRING_SIZE	(1 << 4) /* 16 */
#define USBLOG_CMP_INDEX	3

struct mode_buf {
	unsigned long long ts_nsec;
	char usbmode_str[USBLOG_MAX_STRING_SIZE];
};

struct state_buf {
	unsigned long long ts_nsec;
	int usbstate;
};

struct event_buf {
	unsigned long long ts_nsec;
	unsigned long event;
	int enable;
};

struct usblog_buf {
	unsigned long long mode_count;
	unsigned long long state_count;
	unsigned long long event_count;
	unsigned long mode_index;
	unsigned long state_index;
	unsigned long event_index;
	struct mode_buf mode_buffer[USBLOG_MAX_BUF_SIZE];
	struct state_buf state_buffer[USBLOG_MAX_BUF_SIZE];
	struct event_buf event_buffer[USBLOG_MAX_BUF_SIZE];
};

struct usblog_root_str {
	struct usblog_buf *usblog_buffer;
	spinlock_t usblog_lock;
};

static struct usblog_root_str usblog_root;

static const char *usbstate_string(enum usblog_state usbstate)
{
	switch (usbstate) {
	case NOTIFY_CONFIGURED:
		return "CONFIGURED";
	case NOTIFY_CONNECTED:
		return "CONNECTED";
	case NOTIFY_DISCONNECTED:
		return "DISCONNECTED";
	case NOTIFY_RESET:
		return "RESET";
	case NOTIFY_ACCSTART:
		return "ACCSTART";
	default:
		return "UNDEFINED";
	}
}

static int usblog_proc_show(struct seq_file *m, void *v)
{
	struct usblog_buf *temp_usblog_buffer;
	unsigned long long ts;
	unsigned long rem_nsec;
	int i;

	temp_usblog_buffer = usblog_root.usblog_buffer;

	if (!temp_usblog_buffer)
		goto err;

	seq_printf(m,
		"usblog USB_MODE: count=%llu maxline=%d\n",
			temp_usblog_buffer->mode_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->mode_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->mode_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->mode_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			temp_usblog_buffer->mode_buffer[i].usbmode_str);
		}
	}

	for (i = 0; i < temp_usblog_buffer->mode_index; i++) {
		ts = temp_usblog_buffer->mode_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		temp_usblog_buffer->mode_buffer[i].usbmode_str);
	}
	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog USB STATE: count=%llu maxline=%d\n",
			temp_usblog_buffer->state_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->state_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->state_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->state_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			usbstate_string(temp_usblog_buffer->
						state_buffer[i].usbstate));
		}
	}

	for (i = 0; i < temp_usblog_buffer->state_index; i++) {
		ts = temp_usblog_buffer->state_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		usbstate_string(temp_usblog_buffer->state_buffer[i].usbstate));
	}
	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog USB EVENT: count=%llu maxline=%d\n",
			temp_usblog_buffer->event_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->event_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->event_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->event_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			event_string(temp_usblog_buffer->event_buffer[i].event),
			status_string(temp_usblog_buffer->
					event_buffer[i].enable));
		}
	}

	for (i = 0; i < temp_usblog_buffer->event_index; i++) {
		ts = temp_usblog_buffer->event_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		event_string(temp_usblog_buffer->event_buffer[i].event),
		status_string(temp_usblog_buffer->event_buffer[i].enable));
	}
err:
	return 0;
}

static int usblog_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, usblog_proc_show, NULL);
}

static const struct file_operations usblog_proc_fops = {
	.open		= usblog_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void mode_store_usblog_notify(int type, char *param1)
{
	struct mode_buf *md_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;
	char buf[256], buf2[4];
	char *b, *name;

	target_count = &usblog_root.usblog_buffer->mode_count;
	target_index = &usblog_root.usblog_buffer->mode_index;
	md_buffer = &usblog_root.usblog_buffer->mode_buffer[*target_index];
	if (md_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	md_buffer->ts_nsec = local_clock();

	strlcpy(buf, param1, sizeof(buf));
	b = strim(buf);
	if (b) {
		name = strsep(&b, ",");
		strlcpy(buf2, name, sizeof(buf2));
		strncpy(md_buffer->usbmode_str, buf2,
			sizeof(md_buffer->usbmode_str)-1);
	}
	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;
		if (USBLOG_MAX_STRING_SIZE
			- strlen(md_buffer->usbmode_str) < 5) {
			strncpy(md_buffer->usbmode_str, "overflow",
					sizeof(md_buffer->usbmode_str)-1);
			b = NULL;
		} else {
			strncat(md_buffer->usbmode_str, ",", 1);
			strncat(md_buffer->usbmode_str, name, 3);
		}
	}

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void state_store_usblog_notify(int type, char *param1)
{
	struct state_buf *st_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;
	char buf[256], index;
	char *b, *name;
	int usbstate;

	target_count = &usblog_root.usblog_buffer->state_count;
	target_index = &usblog_root.usblog_buffer->state_index;
	st_buffer = &usblog_root.usblog_buffer->state_buffer[*target_index];
	if (st_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	st_buffer->ts_nsec = local_clock();

	strlcpy(buf, param1, sizeof(buf));
	b = strim(buf);
	name = strsep(&b, "=");

	index = *(b+USBLOG_CMP_INDEX);

	switch (index) {
	case 'F': /* CONFIGURED */
		usbstate = NOTIFY_CONFIGURED;
		break;
	case 'N':  /* CONNECTED */
		usbstate = NOTIFY_CONNECTED;
		break;
	case 'C':  /* DISCONNECTED */
		usbstate = NOTIFY_DISCONNECTED;
		break;
	case 'E':  /* RESET */
		usbstate = NOTIFY_RESET;
		break;
	case 'R':  /* ACCESSORY START */
		usbstate = NOTIFY_ACCSTART;
		break;
	default:
		pr_err("%s state param error. state=%s\n", __func__, param1);
		goto err;
	}

	st_buffer->usbstate = usbstate;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void event_store_usblog_notify(int type, unsigned long *param1, int *param2)
{
	struct event_buf *ev_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;

	target_count = &usblog_root.usblog_buffer->event_count;
	target_index = &usblog_root.usblog_buffer->event_index;
	ev_buffer = &usblog_root.usblog_buffer->event_buffer[*target_index];
	if (ev_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	ev_buffer->ts_nsec = local_clock();
	ev_buffer->event = *param1;
	ev_buffer->enable = *param2;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void store_usblog_notify(int type, void *param1, void *parma2)
{
	
	unsigned long flags = 0;
	
	spin_lock_irqsave(&usblog_root.usblog_lock, flags);

	if (!usblog_root.usblog_buffer) {
		pr_err("%s usblog_buffer is null\n", __func__);
		spin_unlock_irqrestore(&usblog_root.usblog_lock,flags);
		return;
	}

	if (type == NOTIFY_EVENT)
		event_store_usblog_notify(type,
			(unsigned long *)param1, (int *)parma2);
	else  if (type == NOTIFY_USBMODE)
		mode_store_usblog_notify(type, (char *)param1);
	else if (type == NOTIFY_USBSTATE)
		state_store_usblog_notify(type, (char *)param1);
	else
		pr_err("%s type error %d\n", __func__, type);

	spin_unlock_irqrestore(&usblog_root.usblog_lock, flags);
}
EXPORT_SYMBOL(store_usblog_notify);

int register_usblog_proc(void)
{
	int ret = 0;

	proc_create("usblog", 0, NULL, &usblog_proc_fops);

	usblog_root.usblog_buffer
		= kzalloc(sizeof(struct usblog_buf), GFP_KERNEL);
	if (!usblog_root.usblog_buffer) {
		ret = -ENOMEM;
		goto err;
	}
	pr_info("%s size=%zu\n", __func__, sizeof(struct usblog_buf));
	spin_lock_init(&usblog_root.usblog_lock);
err:
	return ret;
}
EXPORT_SYMBOL(register_usblog_proc);

