/*
 * Copyright (C) 2012-2016 Samsung Electronics, Inc.
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
#include <linux/slab.h>
#include <linux/version.h>		/* for linux kernel version information */

#include "tz_common.h"
#include "tz_iwlog.h"
#include "tz_iwlog_polling.h"
#include "tz_hotplug.h"
#include "tz_iwnotify.h"
#include "tzdev_smc.h"
#include "tzlog.h"
#include "tzprofiler.h"

#define TZDEV_DRIVER_VERSION(a,b,c)	(((a) << 16) | ((b) << 8) | (c))
#define TZDEV_DRIVER_CODE		TZDEV_DRIVER_VERSION(3,0,0)

#define SMC_NO(x)			"" # x
#define SMC(x)				"smc " SMC_NO(x)

#if defined(CONFIG_ARM)
#define REGISTERS_NAME	"r"
#define ARCH_EXTENSION	".arch_extension sec\n"
#elif defined(CONFIG_ARM64)
#define REGISTERS_NAME	"x"
#define ARCH_EXTENSION	""
#endif /* CONFIG_ARM */

/* TODO: Need to check which version should be used */
#define SMC_CURRENT_AARCH SMC_AARCH_32

#define TZDEV_SMC_CONNECT_RAW			0
#define TZDEV_SMC_TELEMETRY_CONTROL_RAW		1
#define TZDEV_SMC_COMMMAND_RAW			2
#define TZDEV_SMC_SWD_RESTART_USERSPACE_RAW	3
#define TZDEV_SMC_GET_EVENT_RAW			4
#define TZDEV_SMC_SHMEM_LIST_REG_RAW		6
#define TZDEV_SMC_SHMEM_LIST_RLS_RAW		7
#define TZDEV_SMC_UPDATE_REE_TIME_RAW		8
#define TZDEV_SMC_GET_SWD_SYSCONF_RAW		9
#define TZDEV_SMC_TZ_PANIC_DUMP_INIT_RAW	12
#define TZDEV_SMC_CHECK_VERSION_RAW		13
#define TZDEV_SMC_MEM_REG_RAW			14
#define TZDEV_SMC_SHUTDOWN_RAW			15
#define TZDEV_SMC_BOOT_LOG_READ_RAW		16
#define TZDEV_SMC_PROFILER_CONTROL_RAW		17
#define TZDEV_SMC_NW_KERNEL_API_CMD_RAW		18
#define TZDEV_SMC_SPI_SET_CLOCK_SPEED_RAW	19

#if defined(CONFIG_TZDEV_USE_ARM_CALLING_CONVENTION)

#define TZDEV_SMC_MAGIC			0

#define TZDEV_SMC_CONNECT		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_CONNECT_RAW)
#define TZDEV_SMC_TELEMETRY_CONTROL	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_TELEMETRY_CONTROL_RAW)
#define TZDEV_SMC_COMMMAND		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_COMMMAND_RAW)
#define TZDEV_SMC_SWD_RESTART_USERSPACE	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SWD_RESTART_USERSPACE_RAW)
#define TZDEV_SMC_GET_EVENT		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_GET_EVENT_RAW)
#define TZDEV_SMC_SHMEM_LIST_REG	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHMEM_LIST_REG_RAW)
#define TZDEV_SMC_SHMEM_LIST_RLS	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHMEM_LIST_RLS_RAW)
#define TZDEV_SMC_UPDATE_REE_TIME	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_UPDATE_REE_TIME_RAW)
#define TZDEV_SMC_GET_SWD_SYSCONF	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_GET_SWD_SYSCONF_RAW)
#define TZDEV_SMC_TZ_PANIC_DUMP_INIT	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_TZ_PANIC_DUMP_INIT_RAW)
#define TZDEV_SMC_CHECK_VERSION		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_CHECK_VERSION_RAW)
#define TZDEV_SMC_MEM_REG		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_MEM_REG_RAW)
#define TZDEV_SMC_SHUTDOWN		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SHUTDOWN_RAW)
#define TZDEV_SMC_BOOT_LOG_READ		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_BOOT_LOG_READ_RAW)
#define TZDEV_SMC_PROFILER_CONTROL	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_PROFILER_CONTROL_RAW)
#define TZDEV_SMC_NW_KERNEL_API_CMD	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_NW_KERNEL_API_CMD_RAW)
#define TZDEV_SMC_SPI_SET_CLOCK_SPEED	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_CURRENT_AARCH, SMC_TOS0_SERVICE_MASK, TZDEV_SMC_SPI_SET_CLOCK_SPEED_RAW)

#else /* CONFIG_TZDEV_USE_ARM_CALLING_CONVENTION */

#define TZDEV_SMC_MAGIC			(0xE << 16)

#define TZDEV_SMC_CONNECT		TZDEV_SMC_CONNECT_RAW
#define TZDEV_SMC_TELEMETRY_CONTROL	TZDEV_SMC_TELEMETRY_CONTROL_RAW
#define TZDEV_SMC_COMMMAND		TZDEV_SMC_COMMMAND_RAW
#define TZDEV_SMC_SWD_RESTART_USERSPACE	TZDEV_SMC_SWD_RESTART_USERSPACE_RAW
#define TZDEV_SMC_GET_EVENT		TZDEV_SMC_GET_EVENT_RAW
#define TZDEV_SMC_SHMEM_LIST_REG	TZDEV_SMC_SHMEM_LIST_REG_RAW
#define TZDEV_SMC_SHMEM_LIST_RLS	TZDEV_SMC_SHMEM_LIST_RLS_RAW
#define TZDEV_SMC_UPDATE_REE_TIME	TZDEV_SMC_UPDATE_REE_TIME_RAW
#define TZDEV_SMC_GET_SWD_SYSCONF	TZDEV_SMC_GET_SWD_SYSCONF_RAW
#define TZDEV_SMC_TZ_PANIC_DUMP_INIT	TZDEV_SMC_TZ_PANIC_DUMP_INIT_RAW
#define TZDEV_SMC_CHECK_VERSION		TZDEV_SMC_CHECK_VERSION_RAW
#define TZDEV_SMC_MEM_REG		TZDEV_SMC_MEM_REG_RAW
#define TZDEV_SMC_SHUTDOWN		TZDEV_SMC_SHUTDOWN_RAW
#define TZDEV_SMC_BOOT_LOG_READ		TZDEV_SMC_BOOT_LOG_READ_RAW
#define TZDEV_SMC_PROFILER_CONTROL	TZDEV_SMC_PROFILER_CONTROL_RAW
#define TZDEV_SMC_NW_KERNEL_API_CMD	TZDEV_SMC_NW_KERNEL_API_CMD_RAW
#define TZDEV_SMC_SPI_SET_CLOCK_SPEED	TZDEV_SMC_SPI_SET_CLOCK_SPEED_RAW

#endif /* CONFIG_TZDEV_USE_ARM_CALLING_CONVENTION */

#define TZDEV_CONNECT_LOG		0x0
#define TZDEV_CONNECT_AUX		0x1
#define TZDEV_CONNECT_TRANSPORT		0x2
#define TZDEV_CONNECT_PROFILER		0x3
#define TZDEV_CONNECT_PANIC_DUMP	0x4

#define TZDEV_SHMEM_REG			0x0
#define TZDEV_SHMEM_RLS			0x1

#define TZDEV_PROFILER_CMD_START	0x0

/* Define type for exchange with Secure kernel */
#if defined(CONFIG_TZDEV_SK_PFNS_64BIT)
typedef	u64	sk_pfn_t;
#else /* CONFIG_TZDEV_SK_PFNS_64BIT */
typedef	u32	sk_pfn_t;
#endif /* CONFIG_TZDEV_SK_PFNS_64BIT */

#if defined(CONFIG_TZDEV_SK_MULTICORE)
#define NR_SW_CPU_IDS nr_cpu_ids
#else /* CONFIG_TZDEV_SK_MULTICORE */
#define NR_SW_CPU_IDS 1
#endif /* CONFIG_TZDEV_SK_MULTICORE */

#define TZDEV_AUX_BUF_SIZE		PAGE_SIZE

struct tzio_aux_channel {
	char buffer[TZDEV_AUX_BUF_SIZE];
} __packed;

#define TZDEV_TARGET_DEAD_MASK		0x80000000
#define TZDEV_PIPE_TARGET_DEAD_MASK	(TZDEV_TARGET_DEAD_MASK | 0x0000ffff)

struct tzdev_smc_data {
	union {
		u32 args[NR_SMC_ARGS];
		struct {
			u16 pipe;
			u8 reserved1;
			u8 event_mask;
			struct tz_iwd_cpu_mask cpu_mask;
			u16 iwnotify_oem_flags;
			u16 reserved2;
			u32 reserved3;
		} __packed;
	};
};

int __tzdev_smc_cmd(struct tzdev_smc_data *data, unsigned int swd_ctx_present);

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

#define tzdev_smc_command(tid, shm_id)				tzdev_smc_cmd_extended(TZDEV_SMC_COMMMAND, (tid), (shm_id), 0x0ul, 1)
#define tzdev_smc_get_event()					tzdev_smc_cmd_extended(TZDEV_SMC_GET_EVENT, 0x0ul, 0x0ul, 0x0ul, 1)
#define tzdev_smc_update_ree_time(tv_sec, tv_nsec)		tzdev_smc_cmd_extended(TZDEV_SMC_UPDATE_REE_TIME, (tv_sec), (tv_nsec), 0x0ul, 1);
#define tzdev_smc_connect(mode, pfn, nr_pages)			tzdev_smc_cmd(TZDEV_SMC_CONNECT, (mode), (pfn), (nr_pages), 0)
#define tzdev_smc_telemetry_control(mode, type, arg)		tzdev_smc_cmd(TZDEV_SMC_TELEMETRY_CONTROL, (mode), (type), (arg), 0)
#define tzdev_smc_swd_restart_userspace()			tzdev_smc_cmd(TZDEV_SMC_SWD_RESTART_USERSPACE, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_shmem_list_reg(id, tot_pfns, write)		tzdev_smc_cmd(TZDEV_SMC_SHMEM_LIST_REG, (id), (tot_pfns), (write), 0)
#define tzdev_smc_shmem_list_rls(id)				tzdev_smc_cmd(TZDEV_SMC_SHMEM_LIST_RLS, (id), 0x0ul, 0x0ul, 0)
#define tzdev_smc_get_swd_sysconf()				tzdev_smc_cmd(TZDEV_SMC_GET_SWD_SYSCONF, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_tz_panic_dump_init()				tzdev_smc_cmd(TZDEV_SMC_TZ_PANIC_DUMP_INIT, LINUX_VERSION_CODE, TZDEV_DRIVER_CODE, 0x0ul, 0)
#define tzdev_smc_check_version()				tzdev_smc_cmd(TZDEV_SMC_CHECK_VERSION, LINUX_VERSION_CODE, TZDEV_DRIVER_CODE, 0x0ul, 0)
#define tzdev_smc_mem_reg(pfn, order)				tzdev_smc_cmd(TZDEV_SMC_MEM_REG, pfn, order, 0x0ul, 0)
#define tzdev_smc_shutdown()					tzdev_smc_cmd(TZDEV_SMC_SHUTDOWN, 0x0ul, 0x0ul, 0x0ul, 0)
#define tzdev_smc_boot_log_read(pfn, nr_pages)			tzdev_smc_cmd(TZDEV_SMC_BOOT_LOG_READ, (pfn), (nr_pages), 0x0ul, 0)
#define tzdev_smc_profiler_control(cmd)				tzdev_smc_cmd(TZDEV_SMC_PROFILER_CONTROL, (cmd), 0x0ul, 0x0ul, 0)
#define tzdev_smc_nw_kernel_api_cmd(cmd, ta_handle, arg)	tzdev_smc_cmd(TZDEV_SMC_NW_KERNEL_API_CMD, (cmd), (ta_handle), (arg), 0)
#define tzdev_smc_spi_set_clock_speed(port, sck)		tzdev_smc_cmd(TZDEV_SMC_SPI_SET_CLOCK_SPEED, (port), (sck), 0, 0)

enum {
	NW_KERNEL_API_OPEN,
	NW_KERNEL_API_CLOSE,
	NW_KERNEL_API_SEND,
	NW_KERNEL_API_RECV,
	NW_KERNEL_API_MEM_GRANT,
	NW_KERNEL_API_MEM_REVOKE,
};

unsigned int tzdev_is_up(void);
struct tzio_aux_channel *tzdev_get_aux_channel(void);
void tzdev_put_aux_channel(void);
void tzdev_rsp_convert(struct tzio_smc_data *s, struct tzdev_smc_data *d);

/* Low-level SWd interface for kernel callers */
int tzdev_mem_register(void *ptr, unsigned long size, unsigned int write);
int tzdev_mem_release(unsigned int id);
struct tzdev_smc_data tzdev_get_event(void);
struct tzdev_smc_data tzdev_send_command(unsigned int tid, unsigned int shm_id);

#endif /* __TZDEV_H__ */
