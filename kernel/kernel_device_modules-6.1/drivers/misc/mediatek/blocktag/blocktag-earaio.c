// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Chiayu Ku <chiayu.ku@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][earaio]" fmt

#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include "blocktag-internal.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define EARA_IOCTL_MAX_SIZE 27
struct _EARA_IOCTL_PACKAGE {
	union {
		__s32 cmd;
		__s32 data[EARA_IOCTL_MAX_SIZE];
	};
};

#define EARA_GETINDEX                _IOW('g', 1, struct _EARA_IOCTL_PACKAGE)
#define EARA_COLLECT                 _IOW('g', 2, struct _EARA_IOCTL_PACKAGE)
#define EARA_GETINDEX2               _IOW('g', 3, struct _EARA_IOCTL_PACKAGE)
#define EARA_SET_PARAM               _IOW('g', 4, struct _EARA_IOCTL_PACKAGE)

#define PARAM_RANDOM_THRESHOLD       0
#define PARAM_SEQ_R_THRESHOLD        1
#define PARAM_SEQ_W_THRESHOLD        2

#define ACCEL_NO                     0
#define ACCEL_NORMAL                 1
#define ACCEL_RAND                   3
#define ACCEL_SEQ                    4

struct eara_iostat {
	int io_wl;
	int io_top;
	int io_reqc_r;
	int io_reqc_w;
	int io_q_dept;
	int io_reqsz_top_r;
	int io_reqsz_top_w;
	int io_reqc_rand;
};

/* mini context for major embedded storage only */
#define MICTX_PROC_CMD_BUF_SIZE (16)
#define PWD_WIDTH_NS 100000000 /* 100ms */

static DEFINE_MUTEX(eara_ioctl_lock);
static struct mtk_btag_earaio_control earaio_ctrl;

static int earaio_boost_open(struct inode *inode, struct file *file)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	earaio_ctrl.msg_open_cnt++;
	if (earaio_ctrl.msg_open_cnt == 1)
		earaio_ctrl.pwd_begin = sched_clock();

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return 0;
}

static int earaio_boost_release(struct inode *inode, struct file *file)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	earaio_ctrl.msg_open_cnt--;

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	return 0;
}

static ssize_t earaio_boost_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int len = 0;

	if (earaio_ctrl.msg_buf_used_entry == 0) {
		/* buf empty */
		return 0;
	}

	if (access_ok(buf, count)) {
		len = (count < EARAIO_BOOST_ENTRY_LEN) ?
			count : EARAIO_BOOST_ENTRY_LEN;
		(void)__copy_to_user(buf,
			&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx],
			len);
		memset(&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx], 0,
			EARAIO_BOOST_ENTRY_LEN);
		earaio_ctrl.msg_buf_start_idx += EARAIO_BOOST_ENTRY_LEN;
		if (earaio_ctrl.msg_buf_start_idx == EARAIO_BOOST_BUF_SZ)
			earaio_ctrl.msg_buf_start_idx = 0;
		earaio_ctrl.msg_buf_used_entry--;
	}

	return len;
}

static unsigned int earaio_boost_pool(struct file *file, poll_table *pt)
{
	if (earaio_ctrl.msg_buf_used_entry)
		return (POLLIN | POLLRDNORM);

	poll_wait(file, &earaio_ctrl.msg_readable, pt);

	if (earaio_ctrl.msg_buf_used_entry)
		return (POLLIN | POLLRDNORM);

	return 0;
}

static const struct proc_ops earaio_boost_fops = {
	.proc_open = earaio_boost_open,
	.proc_release = earaio_boost_release,
	.proc_read = earaio_boost_read,
	.proc_poll = earaio_boost_pool,
};

static void mtk_btag_earaio_boost_fill(int boost)
{
	if (earaio_ctrl.msg_buf_used_entry == EARAIO_BOOST_ENTRY_NUM) {
		/* buf full */
		return;
	}

	earaio_ctrl.earaio_boost_state = boost;

	sprintf(&earaio_ctrl.msg_buf[earaio_ctrl.msg_buf_start_idx], "boost=%d",
		boost);
	earaio_ctrl.msg_buf_used_entry++;

	if (boost)
		wake_up(&earaio_ctrl.msg_readable);
}

static void mtk_btag_earaio_clear_data_internal(void)
{
#ifdef EARAIO_EARLY_NOTIFY
	earaio_ctrl.rand_req_cnt = 0;
#endif
	earaio_ctrl.pwd_begin = sched_clock();
	earaio_ctrl.pwd_top_pages[BTAG_IO_READ] = 0;
	earaio_ctrl.pwd_top_pages[BTAG_IO_WRITE] = 0;
}

void mtk_btag_earaio_clear_data(void)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);
	mtk_btag_earaio_clear_data_internal();
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
}

#if IS_ENABLED(CONFIG_CGROUP_SCHED)
static void mtk_btag_eara_get_data(struct eara_iostat *data)
{
	struct mtk_btag_mictx_iostat_struct iostat = {0};
	unsigned long flags;
	__u32 *top;

	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));

	if (mtk_btag_mictx_get_data(earaio_ctrl.mictx_id, &iostat))
		mtk_btag_mictx_enable(&earaio_ctrl.mictx_id, 1);

	top = earaio_ctrl.pwd_top_pages;
	spin_lock_irqsave(&earaio_ctrl.lock, flags);
#ifdef EARAIO_EARLY_NOTIFY
	data->io_reqc_rand = earaio_ctrl.rand_req_cnt;
#endif
	data->io_reqsz_top_r = (top[BTAG_IO_READ]) << PAGE_SHIFT;
	data->io_reqsz_top_w = (top[BTAG_IO_WRITE]) << PAGE_SHIFT;
	mtk_btag_earaio_clear_data_internal();
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	data->io_wl = iostat.wl;
	data->io_top = iostat.top;
	data->io_reqc_r = iostat.reqcnt_r;
	data->io_reqc_w = iostat.reqcnt_w;
	data->io_q_dept = iostat.q_depth;
}

#define EARAIO_BOOST_EVAL_THRESHOLD_PAGES ((32 * 1024 * 1024) >> 12)
static void mtk_btag_earaio_try_boost(bool boost)
{
	unsigned long flags;
	int changed = 0; // 0: not try, 1: try and success, 2: try but fail
	__u32 *top;

	if (!earaio_ctrl.enabled || !earaio_ctrl.earaio_boost_entry)
		return;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	if (!boost) {
		/*
		 * It does not harm to set start_collect as false without
		 * checking original start_collect
		 */
		earaio_ctrl.start_collect = false;
		earaio_ctrl.earaio_boost_state = ACCEL_NO;
		earaio_ctrl.boosted = boost;
		goto end;
	}

	if (earaio_ctrl.boosted)
		goto end;

	top = earaio_ctrl.pwd_top_pages;
	/* Establish threshold */
	if (top[BTAG_IO_READ] >= EARAIO_BOOST_EVAL_THRESHOLD_PAGES ||
	    top[BTAG_IO_WRITE] >= EARAIO_BOOST_EVAL_THRESHOLD_PAGES) {
		if (earaio_ctrl.msg_open_cnt &&
			!earaio_ctrl.earaio_boost_state) {
			mtk_btag_earaio_boost_fill(ACCEL_NORMAL);
			earaio_ctrl.boosted = boost;
			changed = 1;
		}
	}

end:
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	if (boost && changed == 0)
		mtk_btag_mictx_check_window(earaio_ctrl.mictx_id, false);
}

static void mtk_btag_eara_start_collect(void)
{
	unsigned long flags;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));

	/*
	 * Check original start_collect to prevent from calling
	 * mtk_btag_earaio_clear_data_internal()
	 * when original start_collect is true
	 */
	if (!earaio_ctrl.start_collect)
		earaio_ctrl.start_collect = true;
	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
	mtk_btag_mictx_check_window(earaio_ctrl.mictx_id, true);
}

static void mtk_btag_eara_stop_collect(void)
{
	/*
	 * Set earaio_ctrl.start_collect as false in mtk_btag_earaio_try_boost()
	 * to avoid get earaio_ctrl.lock twice
	 */
	mtk_btag_earaio_try_boost(false);

	WARN_ON(!mutex_is_locked(&eara_ioctl_lock));
}

static void mtk_btag_eara_switch_collect(int cmd)
{
	mutex_lock(&eara_ioctl_lock);

	if (cmd)
		mtk_btag_eara_start_collect();
	else
		mtk_btag_eara_stop_collect();

	mutex_unlock(&eara_ioctl_lock);
}

static void mtk_btag_eara_transfer_data(__s32 *data, __s32 input_size)
{
	struct eara_iostat eara_io_data = {0};
	int limit_size;

	mutex_lock(&eara_ioctl_lock);
	mtk_btag_eara_get_data(&eara_io_data);
	mutex_unlock(&eara_ioctl_lock);

	limit_size = MIN(input_size, sizeof(struct eara_iostat));
	memcpy(data, &eara_io_data, limit_size);
}

static unsigned long eara_ioctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

#ifdef EARAIO_EARLY_NOTIFY
static void mtk_btag_eara_set_param(int cmd)
{
	unsigned int param_idx, param_val;

	param_idx = ((unsigned int)cmd) >> 24;
	param_val = cmd & 0xffffff;

	switch (param_idx) {
	case PARAM_RANDOM_THRESHOLD:
		earaio_ctrl.rand_rw_threshold = param_val;
		break;
	case PARAM_SEQ_R_THRESHOLD:
		/* << 8 to convert mb as pages*/
		earaio_ctrl.seq_r_threshold = param_val << 8;
		break;
	case PARAM_SEQ_W_THRESHOLD:
		/* << 8 to convert mb as pages*/
		earaio_ctrl.seq_w_threshold = param_val << 8;
		break;
	}
}
#endif

static unsigned long eara_ioctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static long mtk_btag_eara_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _EARA_IOCTL_PACKAGE *msgKM = NULL;
	struct _EARA_IOCTL_PACKAGE *msgUM = (struct _EARA_IOCTL_PACKAGE *)arg;
	struct _EARA_IOCTL_PACKAGE smsgKM = {0};
#ifdef EARAIO_EARLY_NOTIFY
	unsigned long flags;
#endif

	msgKM = &smsgKM;

	switch (cmd) {
	case EARA_GETINDEX:
#ifdef EARAIO_EARLY_NOTIFY
		spin_lock_irqsave(&earaio_ctrl.lock, flags);
		if (earaio_ctrl.earaio_boost_state != ACCEL_NORMAL)
			//fix me: determine shall be 1 or 0
			earaio_ctrl.earaio_boost_state = ACCEL_NORMAL;
		spin_unlock_irqrestore(&earaio_ctrl.lock, flags);
#endif
		mtk_btag_eara_transfer_data(smsgKM.data,
				sizeof(struct _EARA_IOCTL_PACKAGE));
		eara_ioctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _EARA_IOCTL_PACKAGE));
		break;
	case EARA_COLLECT:
		if (eara_ioctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _EARA_IOCTL_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		mtk_btag_eara_switch_collect(msgKM->cmd);
		break;
#ifdef EARAIO_EARLY_NOTIFY
	case EARA_GETINDEX2:
		mtk_btag_eara_transfer_data(smsgKM.data,
				sizeof(struct _EARA_IOCTL_PACKAGE));
		eara_ioctl_copy_to_user(msgUM, msgKM,
				sizeof(struct _EARA_IOCTL_PACKAGE));
		break;
	case EARA_SET_PARAM:
		if (eara_ioctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _EARA_IOCTL_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		mtk_btag_eara_set_param(msgKM->cmd);
		break;
#endif
	default:
		pr_debug("proc_ioctl: unknown cmd %x\n", cmd);
		ret = -EINVAL;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static int mtk_btag_eara_ioctl_show(struct seq_file *m, void *v)
{
	return 0;
}

static int mtk_btag_eara_ioctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_btag_eara_ioctl_show, inode->i_private);
}

static const struct proc_ops mtk_btag_eara_ioctl_fops = {
	.proc_ioctl = mtk_btag_eara_ioctl,
	.proc_compat_ioctl = mtk_btag_eara_ioctl,
	.proc_open = mtk_btag_eara_ioctl_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int mtk_btag_earaio_init(struct proc_dir_entry *parent)
{
	int ret = 0;
	struct proc_dir_entry *proc_entry;

	proc_entry = proc_create("eara_io",
		0664, parent, &mtk_btag_eara_ioctl_fops);

	if (IS_ERR(proc_entry)) {
		ret = PTR_ERR(proc_entry);
		pr_debug("failed to create proc entry (%d)\n", ret);
	}

	proc_entry = proc_create("eara_io_boost", 0440, parent,
		&earaio_boost_fops);
	if (IS_ERR(proc_entry)) {
		pr_notice("[BLOCK TAG] Creating node eara-io-boost failed\n");
		ret =  -ENOMEM;
	} else {
		earaio_ctrl.earaio_boost_entry = proc_entry;
		init_waitqueue_head(&earaio_ctrl.msg_readable);
	}

	return ret;
}

static ssize_t mtk_btag_earaio_ctrl_sub_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	int ret;
	char cmd[MICTX_PROC_CMD_BUF_SIZE] = {0};

	if (!count)
		goto err;

	if (count > MICTX_PROC_CMD_BUF_SIZE) {
		pr_info("proc_write: command too long\n");
		goto err;
	}

	ret = copy_from_user(cmd, ubuf, count);
	if (ret < 0)
		goto err;

	/* remove line feed */
	cmd[count - 1] = 0;

	if (!strcmp(cmd, "0")) {
		mtk_btag_earaio_try_boost(false);
		earaio_ctrl.enabled = false;
		pr_info("EARA-IO QoS Disable\n");
	} else if (!strcmp(cmd, "1")) {
		earaio_ctrl.enabled = true;
		pr_info("EARA-IO QoS Enable\n");
	} else if (!strcmp(cmd, "2")) {
		mtk_btag_mictx_set_full_logging(earaio_ctrl.mictx_id, false);
		pr_info("EARA-IO Full Logging Disable\n");
	} else if (!strcmp(cmd, "3")) {
		mtk_btag_mictx_set_full_logging(earaio_ctrl.mictx_id, true);
		pr_info("EARA-IO Full Logging Enable\n");
	} else {
		pr_info("proc_write: invalid cmd %s\n", cmd);
		goto err;
	}

	return count;
err:
	return -1;
}

static int mtk_btag_earaio_ctrl_sub_show(struct seq_file *s, void *data)
{
	struct mtk_blocktag *btag;
	char name[BTAG_NAME_LEN] = {' '};

	btag = mtk_btag_find_by_type(earaio_ctrl.mictx_id.storage);
	if (btag)
		strncpy(name, btag->name, BTAG_NAME_LEN - 1);

	seq_puts(s, "<MTK EARA-IO Control Unit>\n");
	seq_printf(s, "Monitor Storage Type: %s\n", name);
	seq_puts(s, "Status:\n");
	seq_printf(s, "  EARA-IO Control     : %s\n",
		   earaio_ctrl.enabled ? "Enable" : "Disable");
	seq_printf(s, "  EARA-IO Full Loging : %s\n",
		   mtk_btag_mictx_full_logging(earaio_ctrl.mictx_id) ?
		   "Enable" : "Disable");
	seq_puts(s, "Commands: echo n > earaio_ctrl, n presents\n");
	seq_puts(s, "  Disable EARA-IO QoS  : 0\n");
	seq_puts(s, "  Enable EARA-IO QoS   : 1\n");
	seq_puts(s, "  Disable Full Logging : 2\n");
	seq_puts(s, "  Enable Full Logging  : 3\n");
	return 0;
}

static const struct seq_operations mtk_btag_seq_earaio_ctrl_ops = {
	.start  = mtk_btag_seq_debug_start,
	.next   = mtk_btag_seq_debug_next,
	.stop   = mtk_btag_seq_debug_stop,
	.show   = mtk_btag_earaio_ctrl_sub_show,
};

static int mtk_btag_earaio_ctrl_sub_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mtk_btag_seq_earaio_ctrl_ops);
}

static const struct proc_ops mtk_btag_earaio_ctrl_sub_fops = {
	.proc_open		= mtk_btag_earaio_ctrl_sub_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mtk_btag_earaio_ctrl_sub_write,
};

void mtk_btag_earaio_update_pwd(enum mtk_btag_io_type type, __u32 size)
{
	int early_notification = 0;
	unsigned long flags;
	__u32 *top = earaio_ctrl.pwd_top_pages;

	spin_lock_irqsave(&earaio_ctrl.lock, flags);

	top[type] += (__u32)(size >> 12);

#ifdef EARAIO_EARLY_NOTIFY
	if (size == (1 << PAGE_SHIFT))
		earaio_ctrl.rand_req_cnt++;

	if (earaio_ctrl.earaio_boost_state != ACCEL_RAND) {
		if (earaio_ctrl.rand_req_cnt >
			earaio_ctrl.rand_rw_threshold) {
			early_notification = ACCEL_RAND;
			mtk_btag_earaio_boost_fill(ACCEL_RAND);
			if (!earaio_ctrl.start_collect)
				earaio_ctrl.boosted = true;
		}
	}

	if (earaio_ctrl.earaio_boost_state != ACCEL_SEQ
	 && earaio_ctrl.earaio_boost_state != ACCEL_RAND) {
		top = earaio_ctrl.pwd_top_pages;
		if (type == BTAG_IO_READ &&
		    top[BTAG_IO_READ] > earaio_ctrl.seq_r_threshold) {
			early_notification = ACCEL_SEQ;
			mtk_btag_earaio_boost_fill(ACCEL_SEQ);
		} else if (type == BTAG_IO_WRITE  &&
		    top[BTAG_IO_WRITE] > earaio_ctrl.seq_w_threshold &&
		    top[BTAG_IO_READ] == 0) {
			early_notification = ACCEL_SEQ;
			mtk_btag_earaio_boost_fill(ACCEL_SEQ);
			if (!earaio_ctrl.start_collect)
				earaio_ctrl.boosted = true;
		}
	}

#endif

	spin_unlock_irqrestore(&earaio_ctrl.lock, flags);

	if (!early_notification) {
		if ((sched_clock() - earaio_ctrl.pwd_begin) >= PWD_WIDTH_NS)
			mtk_btag_earaio_try_boost(true);
	}
}

void mtk_btag_earaio_init_mictx(
	struct mtk_btag_vops *vops,
	enum mtk_btag_storage_type storage_type,
	struct proc_dir_entry *btag_proc_root)
{
	struct proc_dir_entry *proc_entry;

	if (!vops->earaio_enabled)
		return;

	if (!earaio_ctrl.enabled) {
		spin_lock_init(&earaio_ctrl.lock);
		earaio_ctrl.enabled = true;
		mtk_btag_earaio_clear_data_internal();
#ifdef EARAIO_EARLY_NOTIFY
		earaio_ctrl.rand_rw_threshold = THRESHOLD_MAX;
		earaio_ctrl.seq_r_threshold = THRESHOLD_MAX;
		earaio_ctrl.seq_w_threshold = THRESHOLD_MAX;
#endif
		earaio_ctrl.mictx_id.storage = storage_type;
		strncpy(earaio_ctrl.mictx_id.name, "earaio", BTAG_NAME_LEN - 1);
	}

	/* Enable mictx by default if EARA-IO is enabled*/
	mtk_btag_mictx_enable(&earaio_ctrl.mictx_id, 1);

	/* Disable Full Logging for earaio by default */
	mtk_btag_mictx_set_full_logging(earaio_ctrl.mictx_id, false);

	if (mtk_btag_earaio_init(btag_proc_root) < 0) {
		earaio_ctrl.enabled = false;
	} else {
		proc_entry = proc_create("earaio_ctrl", S_IFREG | 0444,
			btag_proc_root, &mtk_btag_earaio_ctrl_sub_fops);
	}
}

#else
#define mtk_btag_earaio_init_mictx(...)

#endif
