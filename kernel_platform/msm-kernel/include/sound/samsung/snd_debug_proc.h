
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  snd_debug_proc.h - header for SAMSUNG Audio debugging.
 */

#ifndef _SND_DEBUG_PROC_H
#define _SND_DEBUG_PROC_H

#include <linux/mutex.h>
#include <linux/sizes.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <kunit/test.h>
#include <kunit/mock.h>
#else
#define __visible_for_testing static
#endif

#define AUD_LOG_BUF_SIZE	SZ_64K
#define MAX_LOG_LINE_LEN		256

#define PROC_SDP_DIR		"snd_debug_proc"
#define SDP_INFO_LOG_NAME	"sdp_info_log"
#define SDP_BOOT_LOG_NAME	"sdp_boot_log"

struct snd_debug_proc {
	char log_buf[AUD_LOG_BUF_SIZE];
	bool is_enabled;
	unsigned int buf_pos;
	unsigned int buf_full;
	struct mutex lock;
	void (*save_log)(char *buf, int len);
};

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_AUDIO)
void sdp_info_print(const char *fmt, ...);
void sdp_boot_print(const char *fmt, ...);
struct snd_debug_proc *get_sdp_info(void);
struct snd_debug_proc *get_sdp_boot(void);
#else
inline void sdp_info_print(const char *fmt, ...)
{
}

inline void sdp_boot_print(const char *fmt, ...)
{
}

inline struct snd_debug_proc *get_sdp_info(void)
{
	return NULL;
}

inline struct snd_debug_proc *get_sdp_boot(void)
{
	return NULL;
}
#endif

#endif

