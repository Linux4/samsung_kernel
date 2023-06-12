/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TZDEV_H__
#define __TZDEV_H__

#include <linux/compiler.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/version.h>		/* for linux kernel version information */
#include <linux/workqueue.h>

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
#if defined(CONFIG_ARCH_APQ8084)
#include <soc/qcom/scm.h>
#elif defined(CONFIG_ARCH_MSM8939) || defined(CONFIG_ARCH_MSM8996)
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>
#elif defined(CONFIG_ARCH_MSM8974)
#include <mach/scm.h>
#endif
#endif

#include "tz_common.h"
#include "tz_iwlog.h"
#include "tz_iwlog_polling.h"
#include "tz_hotplug.h"
#include "tz_iwnotify.h"
#include "tzdev_smc.h"
#include "tzlog.h"
#include "tzprofiler.h"

#define TZDEV_DRIVER_VERSION(a,b,c)	(((a) << 16) | ((b) << 8) | (c))
#define TZDEV_DRIVER_CODE		TZDEV_DRIVER_VERSION(2,1,1)

#define SMC_NO(x)			"" # x
#define SMC(x)				"smc " SMC_NO(x)

/* TODO: Need to check which version should be used */
#define SMC_CURRENT_AARCH SMC_AARCH_32

#define TZDEV_SMC_CONNECT_RAW		0x00000000
#define TZDEV_SMC_TELEMETRY_CONTROL_RAW	0x00000001
#define TZDEV_SMC_COMMMAND_RAW		0x00000002
#define TZDEV_SMC_NWD_DEAD_RAW		0x00000003
#define TZDEV_SMC_GET_EVENT_RAW		0x00000004
#define TZDEV_SMC_NWD_ALIVE_RAW		0x00000005
#define TZDEV_SMC_SHMEM_LIST_REG_RAW	0x00000006
#define TZDEV_SMC_SHMEM_LIST_RLS_RAW	0x00000007
#define TZDEV_SMC_UPDATE_REE_TIME_RAW	0x00000008
#define TZDEV_SMC_GET_SWD_SYSCONF_RAW	0x00000009
#define TZDEV_SMC_CHECK_VERSION_RAW	0x0000000d
#define TZDEV_SMC_MEM_REG_RAW		0x0000000e
#define TZDEV_SMC_SHUTDOWN_RAW		0x0000000f

#ifdef CONFIG_TZDEV_USE_ARM_CALLING_CONVENTION

#define TZDEV_SMC_MAGIC			0

#define TZDEV_SMC_CONNECT		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_CONNECT_RAW)
#define TZDEV_SMC_TELEMETRY_CONTROL	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_TELEMETRY_CONTROL_RAW)
#define TZDEV_SMC_COMMMAND		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_COMMMAND_RAW)
#define TZDEV_SMC_NWD_DEAD		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_NWD_DEAD_RAW)
#define TZDEV_SMC_GET_EVENT		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_GET_EVENT_RAW)
#define TZDEV_SMC_NWD_ALIVE		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_NWD_ALIVE_RAW)
#define TZDEV_SMC_SHMEM_LIST_REG	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHMEM_LIST_REG_RAW)
#define TZDEV_SMC_SHMEM_LIST_RLS	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHMEM_LIST_RLS_RAW)
#define TZDEV_SMC_UPDATE_REE_TIME	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_UPDATE_REE_TIME_RAW)
#define TZDEV_SMC_GET_SWD_SYSCONF	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_GET_SWD_SYSCONF_RAW)
#define TZDEV_SMC_CHECK_VERSION		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_CHECK_VERSION_RAW)
#define TZDEV_SMC_MEM_REG		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_MEM_REG_RAW)
#define TZDEV_SMC_SHUTDOWN		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHUTDOWN_RAW)

#else

#define TZDEV_SMC_MAGIC			(0xE << 16)

#define TZDEV_SMC_CONNECT		TZDEV_SMC_CONNECT_RAW
#define TZDEV_SMC_TELEMETRY_CONTROL	TZDEV_SMC_TELEMETRY_CONTROL_RAW
#define TZDEV_SMC_COMMMAND		TZDEV_SMC_COMMMAND_RAW
#define TZDEV_SMC_NWD_DEAD		TZDEV_SMC_NWD_DEAD_RAW
#define TZDEV_SMC_GET_EVENT		TZDEV_SMC_GET_EVENT_RAW
#define TZDEV_SMC_NWD_ALIVE		TZDEV_SMC_NWD_ALIVE_RAW
#define TZDEV_SMC_SHMEM_LIST_REG	TZDEV_SMC_SHMEM_LIST_REG_RAW
#define TZDEV_SMC_SHMEM_LIST_RLS	TZDEV_SMC_SHMEM_LIST_RLS_RAW
#define TZDEV_SMC_UPDATE_REE_TIME	TZDEV_SMC_UPDATE_REE_TIME_RAW
#define TZDEV_SMC_GET_SWD_SYSCONF	TZDEV_SMC_GET_SWD_SYSCONF_RAW
#define TZDEV_SMC_CHECK_VERSION		TZDEV_SMC_CHECK_VERSION_RAW
#define TZDEV_SMC_MEM_REG		TZDEV_SMC_MEM_REG_RAW
#define TZDEV_SMC_SHUTDOWN		TZDEV_SMC_SHUTDOWN_RAW

#endif /* CONFIG_TZDEV_USE_ARM_CALLING_CONVENTION */

#define TZDEV_CONNECT_LOG		0x0
#define TZDEV_CONNECT_AUX		0x1
#define TZDEV_CONNECT_TRANSPORT		0x2
#define TZDEV_CONNECT_PROFILER		0x3

#define TZDEV_SHMEM_REG			0x0
#define TZDEV_SHMEM_RLS			0x1

/* Define type for exchange with Secure kernel */
#ifdef CONFIG_TZDEV_SK_PFNS_64BIT
typedef	u64	sk_pfn_t;
#else
typedef	u32	sk_pfn_t;
#endif

#ifdef CONFIG_TZDEV_SK_MULTICORE
#define NR_SW_CPU_IDS nr_cpu_ids
#else
#define NR_SW_CPU_IDS 1
#endif

extern unsigned long tzdev_qc_clk;

#define TZDEV_AUX_BUF_SIZE		PAGE_SIZE

struct tzio_aux_channel {
	char buffer[TZDEV_AUX_BUF_SIZE];
} __packed;

struct tzdev_smc_data {
	union {
		unsigned long args[NR_SMC_ARGS];
		struct {
			u16 pipe;
			u8 reserved1;
			u8 event_mask;
			struct tz_iwd_cpu_mask cpu_mask;
			u32 reserved2;
			u32 reserved3;
		} __packed;
	};
};

#define tzdev_smc_cmd(p0, p1, p2, p3, swd_ctx_present)			\
({									\
	int ret;							\
	struct tzdev_smc_data data = { .args = {p0, p1, p2, p3} };	\
	ret = __tzdev_smc_cmd(&data, swd_ctx_present);			\
									\
	if (!ret)							\
		ret = data.args[0];					\
									\
	ret;								\
})

#define tzdev_smc_cmd_extended(p0, p1, p2, p3, swd_ctx_present)		\
({									\
	int ret;							\
	struct tzdev_smc_data data = { .args = {p0, p1, p2, p3} };	\
									\
	ret = __tzdev_smc_cmd(&data, swd_ctx_present);			\
									\
	if (ret)							\
		data.args[0] = ret;					\
									\
	data;								\
})

#define tzdev_smc_command(tid, pfn)				tzdev_smc_cmd_extended(TZDEV_SMC_COMMMAND, (tid), (pfn), 0x0ul, 1)
#define tzdev_smc_get_event()					tzdev_smc_cmd_extended(TZDEV_SMC_GET_EVENT, 0x0ul, 0x0ul, 0x0ul, 1)
#define tzdev_smc_update_ree_time(tv_sec, tv_nsec)		tzdev_smc_cmd_extended(TZDEV_SMC_UPDATE_REE_TIME, (tv_sec), (tv_nsec), 0x0ul, 1);
#define tzdev_smc_connect(mode, pfn, nr_pages)			tzdev_smc_cmd(TZDEV_SMC_CONNECT, (mode), (pfn), (nr_pages), 0)
#define tzdev_smc_telemetry_control(mode, type, arg)		tzdev_smc_cmd(TZDEV_SMC_TELEMETRY_CONTROL, (mode), (type), (arg), 0)
#define tzdev_smc_nwd_dead()					tzdev_smc_cmd(TZDEV_SMC_NWD_DEAD, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_nwd_alive()					tzdev_smc_cmd(TZDEV_SMC_NWD_ALIVE, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_shmem_list_reg(id, tot_pfns, write)		tzdev_smc_cmd(TZDEV_SMC_SHMEM_LIST_REG, (id), (tot_pfns), (write), 0)
#define tzdev_smc_shmem_list_rls(id)				tzdev_smc_cmd(TZDEV_SMC_SHMEM_LIST_RLS, (id), 0x0ul, 0x0ul, 0)
#define tzdev_smc_get_swd_sysconf()				tzdev_smc_cmd(TZDEV_SMC_GET_SWD_SYSCONF, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_check_version()				tzdev_smc_cmd(TZDEV_SMC_CHECK_VERSION, LINUX_VERSION_CODE, TZDEV_DRIVER_CODE, 0x0ul, 0)
#define tzdev_smc_mem_reg(pfn, order)				tzdev_smc_cmd(TZDEV_SMC_MEM_REG, pfn, order, 0x0ul, 0)
#define tzdev_smc_shutdown()					tzdev_smc_cmd(TZDEV_SMC_SHUTDOWN, 0x0ul, 0x0ul, 0x0ul, 0)

static inline void tzdev_convert_result(struct tzio_smc_data *s, struct tzdev_smc_data *d)
{
	int i;

	for (i = 0; i < NR_SMC_ARGS; i++)
		s->args[i] = d->args[i];
}

#ifdef CONFIG_ARM
#define REGISTERS_NAME	"r"
#define ARCH_EXTENSION	".arch_extension sec\n"
#elif defined CONFIG_ARM64
#define REGISTERS_NAME	"x"
#define ARCH_EXTENSION	""
#endif

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
/* svc_id and cmd_id to call QSEE 3-rd party smc handler */
#define TZ_SVC_EXECUTIVE_EXT		250
#define TZ_CMD_ID_EXEC_SMC_EXT		1

#define SCM_EBUSY			-55
#define SCM_V2_EBUSY			-12

#define SCM_TZM_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) | 0x33000000)

#define TZ_EXECUTIVE_EXT_ID_PARAM_ID \
		TZ_SYSCALL_CREATE_PARAM_ID_4( \
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL,\
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL)

struct tzdev_msm_msg {
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
	uint32_t p3;
#ifdef CONFIG_QCOM_SCM_ARMV8
	__s64 tv_sec;
	__s32 tv_nsec;
#endif
	uint32_t crypto_clk;
};

struct tzdev_msm_ret_msg {
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
	uint32_t p3;
#ifdef CONFIG_QCOM_SCM_ARMV8
	uint32_t timer_remains_ms;
#endif
};

#if defined(CONFIG_QCOM_SCM_ARMV8)

#include "tzpm.h"

#define BF_SMC_SUCCESS		0
#define BF_SMC_INTERRUPTED	1
#define BF_SMC_KERNEL_PANIC	2

extern struct mutex tzdev_smc_lock;
extern struct hrtimer tzdev_get_event_timer;

static inline int __tzdev_smc_call(struct tzdev_smc_data *data)
{
	struct tzdev_msm_msg msm_msg = {
		data->args[0], data->args[1], data->args[2], data->args[3],
		0, 0, tzdev_qc_clk
	};
	struct tzdev_msm_ret_msg ret_msg = {0, 0, 0, 0, 0};
	struct scm_desc desc = {0};
	struct timespec ts;
	void* scm_buf;
	int rv;

	scm_buf = kzalloc(PAGE_ALIGN(sizeof(msm_msg)), GFP_KERNEL);
	if (!scm_buf)
		return -ENOMEM;

	getnstimeofday(&ts);
	msm_msg.tv_sec = ts.tv_sec;
	msm_msg.tv_nsec = ts.tv_nsec;
	memcpy(scm_buf, &msm_msg, sizeof(msm_msg));
	dmac_flush_range(scm_buf, (unsigned char *)scm_buf + sizeof(msm_msg));

	desc.arginfo = TZ_EXECUTIVE_EXT_ID_PARAM_ID;
	desc.args[0] = virt_to_phys(scm_buf);
	desc.args[1] = sizeof(msm_msg);
	desc.args[2] = virt_to_phys(scm_buf);
	desc.args[3] = sizeof(ret_msg);

	mutex_lock(&tzdev_smc_lock);
	hrtimer_cancel(&tzdev_get_event_timer);

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT) && !defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	tzdev_qc_pm_clock_enable();
#endif
	do {
		rv = scm_call2(SCM_TZM_FNID(TZ_SVC_EXECUTIVE_EXT,
					TZ_CMD_ID_EXEC_SMC_EXT), &desc);
		if (rv) {
			kfree(scm_buf);
			printk(KERN_ERR "scm_call() failed: %d\n", rv);
			if (rv == SCM_V2_EBUSY)
				rv = -EBUSY;

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT) && !defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
			tzdev_qc_pm_clock_disable();
#endif
			mutex_unlock(&tzdev_smc_lock);
			return rv;
		}
	} while (desc.ret[0] == BF_SMC_INTERRUPTED);

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT) && !defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	tzdev_qc_pm_clock_disable();
#endif

	if (desc.ret[0] == BF_SMC_KERNEL_PANIC) {
		static unsigned int panic_msg_printed;

		tz_iwlog_read_channels();

		if (!panic_msg_printed) {
			tzdev_print(0, "Secure kernel panicked\n");
			panic_msg_printed = 1;
		}

#ifdef CONFIG_TZDEV_SWD_PANIC_IS_CRITICAL
		panic("Secure kernel panicked\n");
#else
		mutex_unlock(&tzdev_smc_lock);
		kfree(scm_buf);
		return -EIO;
#endif
	}

	dmac_flush_range(scm_buf, (unsigned char *)scm_buf + sizeof(ret_msg));
	memcpy(&ret_msg, scm_buf, sizeof(ret_msg));

	if (ret_msg.timer_remains_ms) {
		unsigned long secs;
		unsigned long nsecs;
		secs = ret_msg.timer_remains_ms / MSEC_PER_SEC;
		nsecs = (ret_msg.timer_remains_ms % MSEC_PER_SEC) * NSEC_PER_MSEC;

		hrtimer_start(&tzdev_get_event_timer,
				ktime_set(secs, nsecs), HRTIMER_MODE_REL);
	}

	mutex_unlock(&tzdev_smc_lock);

	kfree(scm_buf);

	data->args[0] = ret_msg.p0;
	data->args[1] = ret_msg.p1;
	data->args[2] = ret_msg.p2;
	data->args[3] = ret_msg.p3;

	return 0;
}
#else /* defined(CONFIG_QCOM_SCM_ARMV8) */
static inline int __tzdev_smc_call(struct tzdev_smc_data *data)
{
	struct tzdev_msm_msg msm_msg = {
		data->args[0], data->args[1], data->args[2], data->args[3],
		tzdev_qc_clk
	};
	struct tzdev_msm_ret_msg ret = {0, 0, 0, 0};
	int rv;

	rv = scm_call(TZ_SVC_EXECUTIVE_EXT, TZ_CMD_ID_EXEC_SMC_EXT,
		      &msm_msg, sizeof(msm_msg),
		      &ret, sizeof(ret));

	if (rv) {
		printk(KERN_ERR "scm_call() failed: %d\n", rv);
		if (rv == SCM_EBUSY)
			rv = -EBUSY;
		return rv;
	}

	data->args[0] = ret_msg.p0;
	data->args[1] = ret_msg.p1;
	data->args[2] = ret_msg.p2;
	data->args[3] = ret_msg.p3;

	return 0;
}
#endif /* CONFIG_QCOM_SCM_ARMV8 */

#else /* defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM) */
static inline int __tzdev_smc_call(struct tzdev_smc_data *data)
{
	register unsigned long _r0 __asm__(REGISTERS_NAME "0") = data->args[0] | TZDEV_SMC_MAGIC;
	register unsigned long _r1 __asm__(REGISTERS_NAME "1") = data->args[1];
	register unsigned long _r2 __asm__(REGISTERS_NAME "2") = data->args[2];
	register unsigned long _r3 __asm__(REGISTERS_NAME "3") = data->args[3];

	__asm__ __volatile__(ARCH_EXTENSION SMC(0): "+r"(_r0) , "+r" (_r1) , "+r" (_r2), "+r" (_r3) : : "memory");

	data->args[0] = _r0;
	data->args[1] = _r1;
	data->args[2] = _r2;
	data->args[3] = _r3;

	return 0;
}
#endif /* defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM) */

static inline int __tzdev_smc_cmd(struct tzdev_smc_data *data,
		unsigned int swd_ctx_present)
{
	int ret;

#ifdef TZIO_DEBUG
	printk(KERN_ERR "tzdev_smc_cmd: p0=0x%lx, p1=0x%lx, p2=0x%lx, p3=0x%lx\n",
			data->a0, data->a1, data->a2, data->a3);
#endif
	tzprofiler_enter_sw();
	tz_iwlog_schedule_delayed_work();

	ret = __tzdev_smc_call(data);

	tz_iwlog_cancel_delayed_work();

	if (swd_ctx_present) {
		tzdev_check_cpu_mask(&data->cpu_mask);
		tz_iwnotify_call_chains(data->event_mask);
		data->event_mask &= ~TZ_IWNOTIFY_EVENT_MASK;
	}

	tzprofiler_wait_for_bufs();
	tzprofiler_exit_sw();
	tz_iwlog_read_channels();

	return ret;
}

unsigned int tzdev_is_initialized(void);
struct tzio_aux_channel *tzdev_get_aux_channel(void);
void tzdev_put_aux_channel(void);

#endif /* __TZDEV_H__ */
