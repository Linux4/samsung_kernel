/****************************************************************************
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/notifier.h>
#include <linux/kernel.h>

#include "scsc_mx_impl.h"
#include "miframman.h"
#include "mifmboxman.h"
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "mxman_if.h"
#else
#include "mxman.h"
#endif
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "fw_panic_record.h"
#include "panicmon.h"
#include "mxproc.h"
#include "mxsyserr.h"
#include "scsc/scsc_log_collector.h"

#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>

/* If limits below are exceeded, a service level reset will be raised to level 7 */
#define SYSERR_RESET_HISTORY_SIZE      (4)
/* Minimum time between system error service resets (ms) */
#define SYSERR_RESET_MIN_INTERVAL      (300000)
/* No more then SYSERR_RESET_HISTORY_SIZE system error service resets in this period (ms)*/
#define SYSERR_RESET_MONITOR_PERIOD    (3600000)
/* SYSERR buffer size */
#define SYSERR_BUFFER_SIZE		(512)
#define FAILED				(u8)(0)
#define SUCCESS				(u8)(1)

/* Time stamps of last service resets in jiffies */
static unsigned long syserr_reset_history[SYSERR_RESET_HISTORY_SIZE] = {0};
static int syserr_reset_history_index;

static uint syserr_reset_min_interval = SYSERR_RESET_MIN_INTERVAL;
module_param(syserr_reset_min_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_reset_min_interval, "Minimum time between system error service resets (ms)");

static uint syserr_reset_monitor_period = SYSERR_RESET_MONITOR_PERIOD;
module_param(syserr_reset_monitor_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_reset_monitor_period, "No more then 4 system error service resets in this period (ms)");


void mx_syserr_init(void)
{
	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_INIT: syserr_reset_min_interval %lu syserr_reset_monitor_period %lu\n",
		syserr_reset_min_interval, syserr_reset_monitor_period);
}

static inline u8 render_syserr_log(struct mxman *mxman,
				   const struct mx_syserr_msg *msg) {
	u32 smap = 0, lmap = 0, strmap = 0, num_args = 0, count = 0;
	char decoded_msgs[SYSERR_BUFFER_SIZE] = {0};
	size_t str_sz = 0;
	u8 percent_detected = 0, long_detected = 0, num_detected = 0;
	char *fmt_str = NULL;
	struct mxlog *mxlog;

	mxlog = scsc_mx_get_mxlog(mxman->mx);
	if (msg->syserr.string_index >= MXLS_SZ(mxlog)) {
		SCSC_TAG_ERR(
		    MXMAN,
		    "Received fmtstr OFFSET(%d) is OUT OF range(%zd)...skip..\n",
		    msg->syserr.string_index, MXLS_SZ(mxlog));
		return FAILED;
	}

	fmt_str = (char *)(MXLS_DATA(mxlog) + msg->syserr.string_index);
	str_sz = strnlen(fmt_str, MXLS_SZ(mxlog) - msg->syserr.string_index);
	for (count = 0; count < str_sz; count++) {
		if ('%' == fmt_str[count]) {
			percent_detected++;
			long_detected = 0;
			num_detected = 0;
			continue;
		}
		if ((long_detected > 2) || (percent_detected > 1)) {
			/* Handler of the invalid cases ..*/
			/* case of %% .. escape and reset*/
			/* Invalid arg %ll..l ? report error msg ?*/
			percent_detected = 0;
			long_detected = 0;
			num_detected = 0;
			continue;
		}
		if (!percent_detected)
			continue;

		switch (fmt_str[count]) {
			/* % was detected*/

		case 'X':		    /*%X or %nX, %nnX ... where n is number from 0->9 */
		case 'x':		    /*%x or %nx, %nnx, %nnnx, where n is number from 0->9 */
			if (!long_detected) /* %l(x/X) is invalid, report err msg? */
				num_args++;
			long_detected = 0;
			percent_detected = 0;
			num_detected = 0;
			break;
		case 'd':		   /*%d or %ld or %lld*/
		case 'i':		   /*%i or %li or %lli*/
		case 'u':		   /*%u or %lu or %llu*/
			if (!num_detected) /* %num(d/u/i) is invalid, report err msg? */
				num_args++;
			long_detected = 0;
			percent_detected = 0;
			num_detected = 0;
			break;
		case 'l':
			if (num_detected) {
				/* %(num)l is invalid, so reset */
				long_detected = 0;
				percent_detected = 0;
				num_detected = 0;
			} else {
				long_detected++;
			}
			break;
		case '0' ... '9': /*Number was detected after */
			if (long_detected) {
				/* %lnum is invalid, so reset */
				long_detected = 0;
				percent_detected = 0;
				num_detected = 0;
			} else {
				num_detected++;
			}
			break;
		default:
			percent_detected = 0;
			num_detected = 0;
			long_detected = 0;
			break;
		}
	}
	snprintf(decoded_msgs, MXLOG_BUFFER_SIZE - 1,
		 SCSC_DBG_FMT("MM_SYSERR_IND slowclock: %d, fastclock: %d, "),
		 __func__, msg->syserr.slow_clock, msg->syserr.fast_clock);
	snprintf(&decoded_msgs[strlen(decoded_msgs)],
		 (MXLOG_BUFFER_SIZE - strlen(decoded_msgs) - 1), "%s%c",
		 fmt_str, (fmt_str[str_sz] != '\n') ? '\n' : '\0');

	/* Pre-Process fmt string to be able to do proper casting */
	if (num_args)
		build_len_sign_maps(decoded_msgs, &smap, &lmap, &strmap);

	switch (num_args) {
	case 1:
		SCSC_TAG_LVL(MXMAN, DEFAULT_DBGLEVEL, decoded_msgs,
			     MXLOG_CAST(msg->syserr.syserr_code, 0, smap, lmap,
					strmap, MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
		break;
	case 2:
		SCSC_TAG_LVL(MXMAN, DEFAULT_DBGLEVEL, decoded_msgs,
			     MXLOG_CAST(msg->syserr.syserr_code, 0, smap, lmap,
					strmap, MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
			     MXLOG_CAST(msg->syserr.param[0], 1, smap, lmap, strmap,
					MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
		break;
	case 3:
		SCSC_TAG_LVL(MXMAN, DEFAULT_DBGLEVEL, decoded_msgs,
			     MXLOG_CAST(msg->syserr.syserr_code, 0, smap, lmap,
					strmap, MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
			     MXLOG_CAST(msg->syserr.param[0], 1, smap, lmap, strmap,
					MXLS_DATA(mxlog), MXLS_SZ(mxlog)),
			     MXLOG_CAST(msg->syserr.param[1], 2, smap, lmap, strmap,
					MXLS_DATA(mxlog), MXLS_SZ(mxlog)));
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "The number of arguments is not correct.\n");
		return FAILED;
	}
	return SUCCESS;
}

void mx_syserr_handler(struct mxman *mxman, const void *message)
{
	const struct mx_syserr_msg *msg = (const struct mx_syserr_msg *)message;
	struct mx_syserr_decode decode;
	struct srvman *srvman;
	srvman = scsc_mx_get_srvman(mxman->mx);

	decode.subsys = (u8)((msg->syserr.syserr_code >> SYSERR_SUB_SYSTEM_POSN) & SYSERR_SUB_SYSTEM_MASK);
	decode.level = (u8)((msg->syserr.syserr_code >> SYSERR_LEVEL_POSN) & SYSERR_LEVEL_MASK);
	decode.type = (u8)((msg->syserr.syserr_code >> SYSERR_TYPE_POSN) & SYSERR_TYPE_MASK);
	decode.subcode = (u16)((msg->syserr.syserr_code >> SYSERR_SUB_CODE_POSN) & SYSERR_SUB_CODE_MASK);
	if (render_syserr_log(mxman, msg) == FAILED) {
		/* Due to rendering failure, printing the syserr message in its raw format */
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND len: %u, ts: 0x%08X, tf: 0x%08X, str: 0x%x, code: 0x%08x, p0: 0x%x, p1: 0x%x\n",
			      msg->syserr.length,
			      msg->syserr.slow_clock,
			      msg->syserr.fast_clock,
			      msg->syserr.string_index,
			      msg->syserr.syserr_code,
			      msg->syserr.param[0],
			      msg->syserr.param[1]);
	}
	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND Subsys %d, Level %d, Type %d, Subcode 0x%04x\n",
		      decode.subsys, decode.level, decode.type, decode.subcode);

	/* Level 1 just gets logged without bothering anyone else */
	if (decode.level == MX_SYSERR_LEVEL_1) {
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x log only\n",
			msg->syserr.syserr_code);
		return;
	}

	/* Ignore if panic reset in progress */
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEMS)
	if (srvman_in_error_safe(srvman) || mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN_WPAN)) {
#else
	if (srvman_in_error_safe(srvman)) {
#endif
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x ignored (reset in progess)\n",
			msg->syserr.syserr_code);
		return;
	}

	/* Ignore any system errors for the same sub-system if recovery is in progress */
	if ((mxman->syserr_recovery_in_progress) && (mxman->last_syserr.subsys == decode.subsys)) {
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x ignored (recovery in progess)\n",
			msg->syserr.syserr_code);
		return;
	}


	/* Let affected sevices escalate if needed - this also checks if only one sub-system is running
	 * and handles race conditions with shutting down service
	 */
	decode.level = srvman_notify_sub_system(srvman, &decode);

	if (decode.level >= MX_SYSERR_LEVEL_5) {
		unsigned long now = jiffies;
		int i;

		/* We use 0 as a NULL timestamp so avoid this */
		now = (now) ? now : 1;

		if ((decode.level >= MX_SYSERR_LEVEL_7) || (mxman->syserr_recovery_in_progress)) {
			/* If full reset has been requested or a service restart is needed and one is
			 * already in progress, trigger a full reset
			 */
			SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset\n",
				msg->syserr.syserr_code);

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
			mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__, decode.subsys);
#else
			mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
#endif
			return;
		}

		/* last_syserr_recovery_time is always zero-ed before we restart the chip */
		if (mxman->last_syserr_recovery_time) {
			/* Have we had a too recent system error service reset
			 * Chance of false positive here is low enough to be acceptable
			 */
			if ((syserr_reset_min_interval) && (time_in_range(now, mxman->last_syserr_recovery_time,
					mxman->last_syserr_recovery_time + msecs_to_jiffies(syserr_reset_min_interval)))) {

				SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset (less than %dms after last)\n",
					msg->syserr.syserr_code, syserr_reset_min_interval);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
				mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__, decode.subsys);
#else
				mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
#endif
				return;
			} else if (syserr_reset_monitor_period) {
				/* Have we had too many system error service resets in one period? */
				/* This will be the case if all our stored history was in this period */
				bool out_of_danger_period_found = false;

				for (i = 0; (i < SYSERR_RESET_HISTORY_SIZE) && (!out_of_danger_period_found); i++)
					out_of_danger_period_found = ((!syserr_reset_history[i]) ||
							      (!time_in_range(now, syserr_reset_history[i],
								syserr_reset_history[i] + msecs_to_jiffies(syserr_reset_monitor_period))));

				if (!out_of_danger_period_found) {
					SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset (too many within %dms)\n",
						msg->syserr.syserr_code, syserr_reset_monitor_period);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
						mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__, decode.subsys);
#else
						mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
#endif
					return;
				}
			}
		} else
			/* First syserr service reset since chip was (re)started - zap history */
			for (i = 0; i < SYSERR_RESET_HISTORY_SIZE; i++)
				syserr_reset_history[i] = 0;

		/* Otherwise trigger recovery of the affected subservices */
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered service recovery\n",
			msg->syserr.syserr_code);
		syserr_reset_history[syserr_reset_history_index++ % SYSERR_RESET_HISTORY_SIZE] = now;
		mxman->last_syserr_recovery_time = now;
		mxman_syserr(mxman, &decode);
	}

#ifdef CONFIG_SCSC_WLBTD
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Trigger sable log collection */
	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x requested log collection\n", msg->syserr.syserr_code);
	scsc_log_collector_schedule_collection(SCSC_LOG_SYS_ERR, decode.subcode);
#endif
#endif
}
