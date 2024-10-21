/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_VKMS_LOG_H_
#define _MTK_VKMS_LOG_H_

#include <linux/kernel.h>
#include <linux/sched/clock.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

extern unsigned long long mutex_time_start;
extern unsigned long long mutex_time_end;
extern long long mutex_time_period;
extern const char *mutex_locker;

enum DPREC_LOGGER_PR_TYPE {
	DPREC_LOGGER_ERROR,
	DPREC_LOGGER_FENCE,
	DPREC_LOGGER_DEBUG,
	DPREC_LOGGER_DUMP,
	DPREC_LOGGER_STATUS,
	DPREC_LOGGER_PR_NUM
};

#define DDPINFO(fmt, arg...)                                                   \
	do {                                                                   \
		if (1)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDPFUNC(fmt, arg...)		\
	pr_info("[DISP][%s line:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg)

#define DDPDBG(fmt, arg...)                                                    \
	do {                                                                   \
		if (1)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDP_PROFILE(fmt, arg...)                                               \
	do {                                                                   \
		if (1)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDPMSG(fmt, arg...)                                                    \
	do {                                                                   \
		if (1)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);             \
	} while (0)

#define DDPDUMP(fmt, arg...)                                                   \
	do {                                                                   \
		if (1)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDPFENCE(fmt, arg...)                                                  \
	do {                                                                   \
		if (1)                                               \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDPPR_ERR(fmt, arg...)                                                 \
	do {                                                                   \
		if (1)                                              \
			pr_err("[DISP][E]" pr_fmt(fmt), ##arg);              \
	} while (0)

#define DDP_MUTEX_LOCK_NESTED(lock, i, name, line)                             \
	do {                                                                   \
		DDPINFO("M_LOCK_NST[%d]:%s[%d] +\n", i, name, line);   \
		mutex_lock_nested(lock, i);		   \
	} while (0)

#define DDP_MUTEX_UNLOCK_NESTED(lock, i, name, line)                           \
	do {                                                                   \
		mutex_unlock(lock);		   \
		DDPINFO("M_ULOCK_NST[%d]:%s[%d] -\n", i, name, line);	\
	} while (0)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define DDPAEE(string, args...)                                                \
	do {                                                                   \
		char str[200];                                                 \
		int r;	\
		r = snprintf(str, 199, "DDP:" string, ##args);                     \
		if (r < 0) {	\
			pr_err("snprintf error\n");	\
		}	\
		aee_kernel_warning_api(__FILE__, __LINE__,                     \
				       DB_OPT_DEFAULT |                        \
					       DB_OPT_MMPROFILE_BUFFER,        \
				       str, string, ##args);                   \
		DDPPR_ERR("[DDP Error]" string, ##args);                       \
	} while (0)
#else /* !CONFIG_MTK_AEE_FEATURE */
#define DDPAEE(string, args...)                                                \
	do {                                                                   \
		char str[200];                                                 \
		int r;	\
		r = snprintf(str, 199, "DDP:" string, ##args);                     \
		if (r < 0) {	\
			pr_err("snprintf error\n");	\
		}	\
		pr_err("[DDP Error]" string, ##args);                          \
	} while (0)
#endif /* CONFIG_MTK_AEE_FEATURE */


#endif /* _MTK_VKMS_LOG_H_ */

