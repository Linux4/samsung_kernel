// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2021 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/sched.h>	/* local_clock */
#include <linux/version.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/clock.h>	/* local_clock */
#endif

#include "mci/mcifc.h"

#include "main.h"
#include "mmu.h"
#include "mmu_internal.h"
#include "fastcall.h"
#include "nq.h"
#include "protocol.h"

#ifdef MC_FFA_FASTCALL
#include "ffa.h"
#endif

#ifdef CONFIG_XEN
#include <xen/xen.h>
#endif

/* Unknown SMC Function Identifier (SMC Calling Convention) */
#define UNKNOWN_SMC -1

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#endif

union fc_sched_init {
	union fc_common common;

	struct {
		u32 cmd;
		u32 buffer_low;
		u32 buffer_high;
		u32 size;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

union fc_mci_init {
	union fc_common common;

	struct {
		u32 cmd;
		u32 phys_addr_low;
		u32 phys_addr_high;
		u32 page_count;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 flags;
		u32 rfu;
	} out;
};

union fc_info {
	union fc_common common;

	struct {
		u32 cmd;
		u32 ext_info_id;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 state;
		u32 ext_info;
	} out;
};

union fc_trace {
	union fc_common common;

	struct {
		u32 cmd;
		u32 buffer_low;
		u32 buffer_high;
		u32 size;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

union fc_nsiq {
	union fc_common common;

	struct {
		u32 cmd;
		u32 debug_ret;
		u32 debug_session_id;
		u32 debug_payload;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 code;
	} out;
};

union fc_yield {
	union fc_common common;

	struct {
		u32 cmd;
		u32 debug_ret;
		u32 debug_session_id;
		u32 debug_payload;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 code;
	} out;
};

union fc_register_buffer {
	union fc_common common;

	struct {
		u32 cmd;
		u32 phys_addr_low;
		u32 phys_addr_high;
		u32 pte_count;
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 handle_low;
		u32 handle_high;
	} out;
};

union fc_reclaim_buffer {
	union fc_common common;

	struct {
		u32 cmd;
		u32 handle_low;
		u32 handle_high;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};

#ifdef MC_TEE_HOTPLUG
union fc_cpu_off {
	union fc_common common;

	struct {
		u32 cmd;
	} in;

	struct {
		u32 resp;
		u32 ret;
	} out;
};
#endif

/* Structure to log SMC calls */
struct smc_log_entry {
	u64 cpu_clk;
	int cpu_id;
	union fc_common fc;
};

#define SMC_LOG_SIZE 1024
static struct smc_log_entry smc_log[SMC_LOG_SIZE];
static int smc_log_index;

/*
 * convert fast call return code to linux driver module error code
 */
static int convert_fc_ret(u32 ret)
{
	switch (ret) {
	case MC_FC_RET_OK:
		return 0;
	case MC_FC_RET_ERR_INVALID:
		return -EINVAL;
	case MC_FC_RET_ERR_ALREADY_INITIALIZED:
		return -EBUSY;
	default:
		return -EFAULT;
	}
}

/*
 * __smc() - fast call to MobiCore
 *
 * @data: pointer to fast call data
 */
static inline int __smc(union fc_common *fc, const char *func)
{
	int ret = 0;

	/* Log SMC call */
	smc_log[smc_log_index].cpu_clk = local_clock();
	smc_log[smc_log_index].cpu_id  = raw_smp_processor_id();
	smc_log[smc_log_index].fc = *fc;
	if (++smc_log_index >= SMC_LOG_SIZE)
		smc_log_index = 0;

#ifdef MC_FFA_FASTCALL
	ret = ffa_fastcall(fc);
#else
	{
		/* SMC expect values in x0-x7 */
		register u64 reg0 __asm__("x0") = fc->in.cmd;
		register u64 reg1 __asm__("x1") = fc->in.param[0];
		register u64 reg2 __asm__("x2") = fc->in.param[1];
		register u64 reg3 __asm__("x3") = fc->in.param[2];
		register u64 reg4 __asm__("x4") = 0;
		register u64 reg5 __asm__("x5") = 0;
		register u64 reg6 __asm__("x6") = 0;
		register u64 reg7 __asm__("x7") = 0;

		/*
		 * According to AARCH64 SMC Calling Convention (ARM DEN 0028A),
		 * section 3.1: registers x8-x17 are unpredictable/scratch
		 * registers.  So we have to make sure that the compiler does
		 * not allocate any of those registers by letting him know that
		 * the asm code might clobber them.
		 */
		__asm__ volatile (
			"smc #0\n"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3),
			  "+r"(reg4), "+r"(reg5), "+r"(reg6), "+r"(reg7)
			:
			: "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
			  "x16", "x17"
		);

		/* set response */
		fc->out.resp     = reg0;
		fc->out.ret      = reg1;
		fc->out.param[0] = reg2;
		fc->out.param[1] = reg3;

		/* The TEE has not been loaded */
		if (reg0 == UNKNOWN_SMC)
			ret = -EIO;
	}
#endif /* !MC_FFA_FASTCALL */

	if (ret) {
		mc_dev_err(ret, "failed for %s", func);
	} else {
		ret = convert_fc_ret(fc->out.ret);
		if (ret)
			mc_dev_err(ret, "%s failed (%x)", func, fc->out.ret);
	}

	return ret;
}

#define smc(__fc__) __smc(__fc__.common, __func__)

int fc_sched_init(phys_addr_t buffer, u32 size)
{
	union fc_sched_init fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_SCHED_INIT;
	fc.in.buffer_low = (u32)buffer;
	fc.in.buffer_high = (u32)(buffer >> 32);
	fc.in.size = size;
	return smc(&fc);
}

int fc_mci_init(uintptr_t phys_addr, u32 page_count)
{
	union fc_mci_init fc;
	u32 phys_addr_high = (u32)(phys_addr >> 32);

	/* Call the INIT fastcall to setup MobiCore initialization */
	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_MCI_INIT;
	fc.in.phys_addr_low = (u32)phys_addr;
	fc.in.phys_addr_high = phys_addr_high;
	fc.in.page_count = page_count;
	return smc(&fc);
}

int fc_info(u32 ext_info_id, u32 *state, u32 *ext_info)
{
	union fc_info fc;
	int ret = 0;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_INFO;
	fc.in.ext_info_id = ext_info_id;
	ret = smc(&fc);
	if (ret) {
		if (state)
			*state = MC_STATUS_NOT_INITIALIZED;

		if (ext_info)
			*ext_info = 0;

		mc_dev_err(ret, "failed for index %d", ext_info_id);
	} else {
		if (state)
			*state = fc.out.state;

		if (ext_info)
			*ext_info = fc.out.ext_info;
	}

	return ret;
}

int fc_trace_init(phys_addr_t buffer, u32 size)
{
	union fc_trace fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_MEM_TRACE;
	fc.in.buffer_low = (u32)buffer;
	fc.in.buffer_high = (u32)(buffer >> 32);
	fc.in.size = size;
	return smc(&fc);
}

int fc_trace_set_level(u32 level)
{
	union fc_trace fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_MEM_TRACE;
	fc.in.buffer_low = level;
	return smc(&fc);
}

int fc_trace_deinit(void)
{
	return fc_trace_init(0, 0);
}

/* sid, payload only used for debug purpose */
int fc_nsiq(u32 session_id, u32 payload)
{
	int ret;
	union fc_nsiq fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_SMC_N_SIQ;
	fc.in.debug_session_id = session_id;
	fc.in.debug_payload = payload;

	ret = smc(&fc);
	if (ret)
		return ret;

	/* SWd return status must always be zero */
	if (fc.out.ret)
		return -EIO;

	return 0;
}

/* sid, payload only used for debug purpose */
int fc_yield(u32 session_id, u32 payload, struct fc_s_yield *resp)
{
	int ret;
	union fc_yield fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_SMC_N_YIELD;
	fc.in.debug_session_id = session_id;
	fc.in.debug_payload = payload;

	ret = smc(&fc);
	if (ret)
		return ret;

	/* SWd return status must always be zero */
	if (fc.out.ret)
		return -EIO;

	if (resp) {
		resp->resp = fc.out.resp;
		resp->ret  = fc.out.ret;
		resp->code = fc.out.code;
	}

	return 0;
}

#ifdef CONFIG_XEN
static int fc_register_shm(u64 phys_addr, u32 pte_count, u64 *handle)
{
	int ret;
	union fc_register_buffer fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_REGISTER_SHM;
	fc.in.phys_addr_low = (u32)phys_addr;
	fc.in.phys_addr_high = (u32)(phys_addr >> 32);
	fc.in.pte_count = pte_count;
	ret = smc(&fc);
	if (ret)
		return ret;

	/* SWd return status must always be zero */
	if (fc.out.ret)
		return -EIO;

	if (handle)
		*handle = (u64)fc.out.handle_high << 32 | fc.out.handle_low;

	return 0;
}

static int fc_reclaim_shm(u64 handle)
{
	int ret;
	union fc_reclaim_buffer fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_RECLAIM_SHM;
	fc.in.handle_low = (u32)handle;
	fc.in.handle_high = (u32)(handle >> 32);
	ret = smc(&fc);
	if (ret)
		return ret;

	/* SWd return status must always be zero */
	if (fc.out.ret)
		return -EIO;

	return 0;
}
#endif

int fc_register_buffer(struct page **pages, struct tee_mmu *mmu, u64 tag)
{
	int ret = 0;

#ifdef MC_FFA_FASTCALL
	ret = ffa_register_buffer(pages, mmu, tag);
#else
#ifdef CONFIG_XEN
	/*
	 * CONFIG_XEN can be active even if xen isn't running as EL2.
	 * xen_domain() ensures that Xen is running as Hypervisor.
	 * Issue the FC when Xen is present and the module isn't running as FE.
	 */
	if (xen_domain() && !protocol_is_fe()) {
		phys_addr_t phys = virt_to_phys(mmu->pmd_table.addr);

		ret = fc_register_shm(phys, mmu->nr_pages, &mmu->handle);
	} else {
		mmu->handle = virt_to_phys(mmu->pmd_table.addr);
	}
#else
	mmu->handle = virt_to_phys(mmu->pmd_table.addr);
#endif
#endif

	if (ret)
		mc_dev_err(ret, "sharing buffer, handle %llx",
			   mmu->handle);

	return ret;
}

void fc_reclaim_buffer(struct tee_mmu *mmu)
{
	int ret = 0;

#ifdef MC_FFA_FASTCALL
	ret = ffa_reclaim_buffer(mmu);
#else
#ifdef CONFIG_XEN
	if (xen_domain() && !protocol_is_fe())
		ret = fc_reclaim_shm(mmu->handle);

#endif
#endif

	if (ret)
		mc_dev_err(ret, "reclaiming buffer, handle %llx",
			   mmu->handle);
}

#ifdef MC_TEE_HOTPLUG
int fc_cpu_off(void)
{
	int ret;
	union fc_cpu_off fc;

	memset(&fc, 0, sizeof(fc));
	fc.in.cmd = MC_FC_CPU_OFF;

	ret = smc(&fc);
	if (ret)
		return ret;

	/* SWd return status must always be zero */
	if (fc.out.ret)
		return -EIO;
	return 0;
}
#endif

static int show_smc_log_entry(struct kasnprintf_buf *buf,
			      struct smc_log_entry *entry)
{
	return kasnprintf(buf, "%10d %20llu 0x%08x 0x%08x 0x%08x 0x%08x\n",
			  entry->cpu_id, entry->cpu_clk, entry->fc.in.cmd,
			  entry->fc.in.param[0], entry->fc.in.param[1],
			  entry->fc.in.param[2]);
}

/*
 * Dump SMC log circular buffer, starting from oldest command. It is assumed
 * nothing goes in any more at this point.
 */
int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf)
{
	int i, ret = 0;

	ret = kasnprintf(buf, "%10s %20s %10s %-10s %-10s %-10s\n", "CPU id",
			 "CPU clock", "command", "param1", "param2", "param3");
	if (ret < 0)
		return ret;

	if (smc_log[smc_log_index].cpu_clk)
		/* Buffer has wrapped around, dump end (oldest records) */
		for (i = smc_log_index; i < SMC_LOG_SIZE; i++) {
			ret = show_smc_log_entry(buf, &smc_log[i]);
			if (ret < 0)
				return ret;
		}

	/* Dump first records */
	for (i = 0; i < smc_log_index; i++) {
		ret = show_smc_log_entry(buf, &smc_log[i]);
		if (ret < 0)
			return ret;
	}

	return ret;
}
