/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2016 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <soc/samsung/exynos-pmu.h>
#include <linux/sizes.h>
/*
 * PMU register offset : MUST MODIFY ACCORDING TO SoC
 */
#define EXYNOS_PMU_INFORM2 0x0808
#define EXYNOS_PMU_INFORM3 0x080C
#define EXYNOS_PMU_PS_HOLD_CONTROL 0x330C

#ifdef CONFIG_SEC_DEBUG
extern int dynsyslog_on;
extern int  sec_debug_setup(void);
extern void sec_debug_reboot_handler(void);
extern void sec_debug_panic_handler(void *buf, bool dump);
extern void sec_debug_post_panic_handler(void);

extern int  sec_debug_get_debug_level(void);
extern void sec_debug_disable_printk_process(void);

/* getlog support */
extern void sec_getlog_supply_kernel(void *klog_buf);
extern void sec_getlog_supply_platform(unsigned char *buffer, const char *name);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset);
#else
#define sec_debug_setup()			(-1)
#define sec_debug_reboot_handler()		do { } while(0)
#define sec_debug_panic_handler(a,b)		do { } while(0)
#define sec_debug_post_panic_handler()		do { } while(0)

#define sec_debug_get_debug_level()		0
#define sec_debug_disable_printk_process()	do { } while(0)

#define sec_getlog_supply_kernel(a)		do { } while(0)
#define sec_getlog_supply_platform(a,b)		do { } while(0)

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset)
{
    return;
}
#endif /* CONFIG_SEC_DEBUG */

#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
extern int  sec_debug_is_enabled_for_ssr(void);
#endif

/* sec logging */
#ifdef CONFIG_SEC_AVC_LOG
extern void sec_debug_avc_log(char *fmt, ...);
#else
#define sec_debug_avc_log(a, ...)		do { } while(0)
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
extern void sec_debug_tsp_log(char *fmt, ...);
extern void sec_debug_tsp_log_msg(char *msg, char *fmt, ...);
#ifdef CONFIG_TOUCHSCREEN_FTS
extern void tsp_dump(void);
#endif
#else
#define sec_debug_tsp_log(a, ...)		do { } while(0)
#endif

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
extern void sec_debug_save_last_kmsg(unsigned char* head_ptr, unsigned char* curr_ptr, size_t buf_size);
#else
#define sec_debug_save_last_kmsg(a, b, c)		do { } while(0)
#endif

/* Last KMsg magickey */
#define SEC_LKMSG_MAGICKEY 0x0000000a6c6c7546

#ifdef CONFIG_SEC_PARAM
#define CM_OFFSET		0x700234
#define CM_OFFSET_LIMIT		1
#define GSP_OFFSET		0x700238
#define GSP_OFFSET_LIMIT 	0
int set_param(unsigned long offset, char val);
#endif

#ifdef CONFIG_KFAULT_AUTO_SUMMARY
#define AUTO_SUMMARY_MAGIC 0xcafecafe
#define AUTO_SUMMARY_TAIL_MAGIC 0x00c0ffee

#define PRIO_LV0 0
#define PRIO_LV1 1
#define PRIO_LV2 2
#define PRIO_LV3 3
#define PRIO_LV4 4
#define PRIO_LV5 5
#define PRIO_LV6 6
#define PRIO_LV7 7
#define PRIO_LV8 8
#define PRIO_LV9 9

#define SEC_DEBUG_AUTO_COMM_BUF_SIZE 10

enum sec_debug_FREQ_INFO
{
	FREQ_INFO_CLD0 = 0,
	FREQ_INFO_CLD1,
	FREQ_INFO_INT,
	FREQ_INFO_MIF,
	FREQ_INFO_MAX,
};

struct sec_debug_auto_comm_buf {
	atomic_t logging_entry;
	atomic_t logging_diable;
	atomic_t logging_count;
	unsigned int offset;
	char buf[SZ_4K];
};

struct sec_debug_auto_comm_freq_info {
	int old_freq;
	int new_freq;
	u64 time_stamp;
	u64 last_freq_info;
};

struct sec_debug_auto_summary {
	int haeder_magic;
	int fault_flag;
	int lv5_log_cnt;
	u64 lv5_log_order;
	int order_map_cnt;
	int order_map[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
	struct sec_debug_auto_comm_buf auto_Comm_buf[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
	struct sec_debug_auto_comm_freq_info freq_info[FREQ_INFO_MAX];
	// for code diff
	u64 pa_text;
	u64 pa_start_rodata;
	int tail_magic;
};

extern void sec_debug_auto_summary_log_disable(int type);
extern void sec_debug_auto_summary_log_once(int type);
extern void sec_debug_set_auto_comm_last_devfreq_buf(struct sec_debug_auto_comm_freq_info *freq_info);
extern void sec_debug_set_auto_comm_last_cpufreq_buf(struct sec_debug_auto_comm_freq_info *freq_info);
#endif

#endif /* SEC_DEBUG_H */
