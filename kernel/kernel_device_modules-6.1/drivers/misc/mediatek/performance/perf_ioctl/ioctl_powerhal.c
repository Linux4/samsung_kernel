// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "ioctl_powerhal.h"

#define TAG "IOCTL_POWERHAL"

// ADPF
int (*powerhal_adpf_create_session_hint_fp)
	(unsigned int sid, unsigned int tgid, unsigned int uid,
	int *threadIds, int threadIds_size, long durationNanos);
EXPORT_SYMBOL_GPL(powerhal_adpf_create_session_hint_fp);
int (*powerhal_adpf_get_hint_session_preferred_rate_fp)(long long *preferredRate);
EXPORT_SYMBOL_GPL(powerhal_adpf_get_hint_session_preferred_rate_fp);
int (*powerhal_adpf_update_work_duration_fp)(unsigned int sid, long targetDurationNanos);
EXPORT_SYMBOL_GPL(powerhal_adpf_update_work_duration_fp);
int (*powerhal_adpf_report_actual_work_duration_fp)
	(unsigned int sid, struct _ADPF_WORK_DURATION *workDuration, int work_duration_size);
EXPORT_SYMBOL_GPL(powerhal_adpf_report_actual_work_duration_fp);
int (*powerhal_adpf_pause_fp)(unsigned int sid);
EXPORT_SYMBOL_GPL(powerhal_adpf_pause_fp);
int (*powerhal_adpf_resume_fp)(unsigned int sid);
EXPORT_SYMBOL_GPL(powerhal_adpf_resume_fp);
int (*powerhal_adpf_close_fp)(unsigned int sid);
EXPORT_SYMBOL_GPL(powerhal_adpf_close_fp);
int (*powerhal_adpf_sent_hint_fp)(unsigned int sid, int hint);
EXPORT_SYMBOL_GPL(powerhal_adpf_sent_hint_fp);
int (*powerhal_adpf_set_threads_fp)(unsigned int sid, int *threadIds, int threadIds_size);
EXPORT_SYMBOL_GPL(powerhal_adpf_set_threads_fp);
void (*boost_get_cmd_fp)(int *cmd, int *value);
EXPORT_SYMBOL_GPL(boost_get_cmd_fp);

// DSU
int (*powerhal_dsu_sport_mode_fp)(unsigned int mode);
EXPORT_SYMBOL_GPL(powerhal_dsu_sport_mode_fp);


struct proc_dir_entry *perfmgr_root;

static unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long adpf_device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _ADPF_PACKAGE *t_msgKM = NULL,
			*t_msgUM = (struct _ADPF_PACKAGE *)arg;
	struct _ADPF_PACKAGE t_smsgKM;
	__s32 *threadIds = NULL;
	struct _ADPF_WORK_DURATION *workDuration = NULL;

	t_msgKM = &t_smsgKM;
	if (perfctl_copy_from_user(t_msgKM, t_msgUM,
				sizeof(struct _ADPF_PACKAGE))) {
		pr_debug("POWERHAL_SET_ADPF_DATA error: %d", cmd);
		ret = -EFAULT;
		goto ret_ioctl;
	}

	pr_debug(TAG "cmd: %d\n", cmd);

	switch (cmd) {
	case POWERHAL_SET_ADPF_DATA:

		if (t_msgKM->cmd == ADPF_CREATE_HINT_SESSION) {
			if (t_msgKM->threadIds_size > ADPF_MAX_THREAD || !t_msgKM->threadIds_size) {
				pr_debug("ADPF_CREATE_HINT_SESSION threadIds_size bigger than ADPF_MAX_THREAD or 0");
				ret = -EFAULT;
				goto ret_ioctl;
			}

			threadIds = kcalloc(t_msgKM->threadIds_size,
				t_msgKM->threadIds_size*sizeof(__s32),
				GFP_KERNEL);

			if(!threadIds) {
				pr_debug("ADPF_CREATE_HINT_SESSION threadIds is NULL");
				ret = -EFAULT;
				goto ret_ioctl;
			}

			if (perfctl_copy_from_user(threadIds, t_msgKM->threadIds,
					t_msgKM->threadIds_size*sizeof(__s32))) {
				pr_debug("ADPF_CREATE_HINT_SESSION copy from user error");
				ret = -EFAULT;
				kfree(threadIds);
				goto ret_ioctl;
			}

			if (powerhal_adpf_create_session_hint_fp) {
				powerhal_adpf_create_session_hint_fp(
					t_msgKM->sid,
					t_msgKM->tgid,
					t_msgKM->uid,
					threadIds,
					t_msgKM->threadIds_size,
					t_msgKM->durationNanos);
			}

			kfree(threadIds);
		} else if (t_msgKM->cmd == ADPF_UPDATE_TARGET_WORK_DURATION) {
			if (powerhal_adpf_update_work_duration_fp)
				powerhal_adpf_update_work_duration_fp(t_msgKM->sid,
					t_msgKM->targetDurationNanos);
		} else if (t_msgKM->cmd == ADPF_REPORT_ACTUAL_WORK_DURATION) {
			if (t_msgKM->work_duration_size <= 0) {
				pr_debug("ADPF_REPORT_ACTUAL_WORK_DURATION threadIds_size error: %d",
					t_msgKM->work_duration_size);
				ret = -EFAULT;
				goto ret_ioctl;
			}

			workDuration = kcalloc(t_msgKM->work_duration_size,
				t_msgKM->work_duration_size*sizeof(struct _ADPF_WORK_DURATION),
				GFP_KERNEL);

			if(!workDuration) {
				pr_debug("ADPF_REPORT_ACTUAL_WORK_DURATION workDuration is NULL");
				ret = -EFAULT;
				goto ret_ioctl;
			}

			if (perfctl_copy_from_user(workDuration, t_msgKM->workDuration,
				t_msgKM->work_duration_size
				*sizeof(struct _ADPF_WORK_DURATION))) {
				pr_debug("ADPF_REPORT_ACTUAL_WORK_DURATION copy from user error");
				ret = -EFAULT;
				kfree(workDuration);
				goto ret_ioctl;
			}

			if (powerhal_adpf_report_actual_work_duration_fp)
				powerhal_adpf_report_actual_work_duration_fp(t_msgKM->sid,
					workDuration, t_msgKM->work_duration_size);

			kfree(workDuration);
		} else if (t_msgKM->cmd == ADPF_PAUSE) {
			if (powerhal_adpf_pause_fp)
				powerhal_adpf_pause_fp(t_msgKM->sid);
		} else if (t_msgKM->cmd == ADPF_RESUME) {
			if (powerhal_adpf_resume_fp)
				powerhal_adpf_resume_fp(t_msgKM->sid);
		} else if (t_msgKM->cmd == ADPF_CLOSE) {
			if (powerhal_adpf_close_fp)
				powerhal_adpf_close_fp(t_msgKM->sid);
		} else if (t_msgKM->cmd == ADPF_SENT_HINT) {
			if (powerhal_adpf_sent_hint_fp)
				powerhal_adpf_sent_hint_fp(t_msgKM->sid, t_msgKM->hint);
		} else if (t_msgKM->cmd == ADPF_SET_THREADS) {
			if (t_msgKM->threadIds_size > ADPF_MAX_THREAD || !t_msgKM->threadIds_size) {
				pr_debug("ADPF_SET_THREADS threadIds_size bigger than ADPF_MAX_THREAD or 0");
				ret = -EFAULT;
				goto ret_ioctl;
			}

			threadIds = kcalloc(t_msgKM->threadIds_size,
				t_msgKM->threadIds_size*sizeof(__s32), GFP_KERNEL);

			if(!threadIds) {
				pr_debug("ADPF_SET_THREADS threadIds is NULL");
				ret = -EFAULT;
				goto ret_ioctl;
			}

			if (perfctl_copy_from_user(threadIds, t_msgKM->threadIds,
							t_msgKM->threadIds_size*sizeof(__s32))) {
				pr_debug("ADPF_SET_THREADS copy from user error");
				ret = -EFAULT;
				kfree(threadIds);
				goto ret_ioctl;
			}

			if (powerhal_adpf_set_threads_fp)
				powerhal_adpf_set_threads_fp(t_msgKM->sid, threadIds,
							t_msgKM->threadIds_size);

			kfree(threadIds);
		} else {
			ret = -1;
		}
		break;
	case POWERHAL_GET_ADPF_DATA:
		if (t_msgKM->cmd == ADPF_GET_HINT_SESSION_PREFERED_RATE) {
			powerhal_adpf_get_hint_session_preferred_rate_fp(&t_msgKM->preferredRate);

			if (perfctl_copy_to_user(t_msgUM, t_msgKM,
					sizeof(struct _ADPF_PACKAGE))) {
				pr_debug("POWERHAL_GET_ADPF_DATA copy from user error");
				ret = -EFAULT;
				goto ret_ioctl;
			}
		}
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n", __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _POWERHAL_PACKAGE *t_msgKM = NULL,
			*t_msgUM = (struct _POWERHAL_PACKAGE *)arg;
	struct _POWERHAL_PACKAGE t_smsgKM;
	int _cmd = -1;
	int _value = -1;
	struct _CPU_CTRL_PACKAGE *t_msgKM_boost = NULL,
			*t_msgUM_boost = (struct _CPU_CTRL_PACKAGE *)arg;
	struct _CPU_CTRL_PACKAGE t_smsgKM_boost;

	t_msgKM = &t_smsgKM;

	pr_debug(TAG "cmd: %d\n", cmd);

	switch (cmd) {
	case NOTIFY_BOOST:
		t_msgKM_boost = &t_smsgKM_boost;

		if (perfctl_copy_from_user(t_msgKM_boost, t_msgUM_boost,
				sizeof(struct _CPU_CTRL_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (boost_get_cmd_fp) {
			boost_get_cmd_fp(&_cmd, &_value);
			t_msgKM_boost->cmd = _cmd;
			t_msgKM_boost->value = _value;
		}

		if (perfctl_copy_to_user(t_msgUM_boost, t_msgKM_boost,
				sizeof(struct _CPU_CTRL_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}
		break;
	case DSU_CCI_SPORT_MODE:
		if (perfctl_copy_from_user(t_msgKM, t_msgUM,
			sizeof(struct _POWERHAL_PACKAGE))) {
			pr_debug("POWERHAL_SET_DATA error: %d", cmd);
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (powerhal_dsu_sport_mode_fp)
			powerhal_dsu_sport_mode_fp(t_msgKM->value);
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n", __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static const struct proc_ops adpf_Fops = {
	.proc_compat_ioctl = adpf_device_ioctl,
	.proc_ioctl = adpf_device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops Fops = {
	.proc_compat_ioctl = device_ioctl,
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void __exit exit_perfctl(void) {}
static int __init init_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;

	pr_debug(TAG"START to init ioctl_powerhal driver\n");

	parent = proc_mkdir("perfmgr_powerhal", NULL);
	perfmgr_root = parent;

	pe = proc_create("ioctl_powerhal_adpf", 0440, parent, &adpf_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pe = proc_create("ioctl_powerhal", 0440, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pr_debug(TAG"FINISH init ioctl_powerhal driver\n");

	return 0;

out_wq:
	return ret_val;
}

module_init(init_perfctl);
module_exit(exit_perfctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek POWERHAL perf_ioctl");
MODULE_AUTHOR("MediaTek Inc.");
