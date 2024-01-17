// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/uaccess.h>

#include <trace/hooks/traps.h>

#include "sec_qc_debug.h"

/* In line with aarch64_insn_read from arch/arm64/kernel/insn.c */
static int instruction_read(void *addr, u32 *insnp)
{
	int ret;
	__le32 val;

	ret = probe_kernel_read(&val, addr, AARCH64_INSN_SIZE);
	if (!ret)
		*insnp = le32_to_cpu(val);

	return ret;
}

/* In line with dump_kernel_instr from arch/arm64/kernel/traps.c */
static void dump_instr(const char *rname, u64 instr)
{
	char str[sizeof("00000000 ") * 5 + 2 + 1], *p = str;
	ssize_t i;

	for (i = -4; i < 1; i++) {
		unsigned int val, bad;

		bad = instruction_read(&((u32 *)instr)[i], &val);
		if (!bad)
			p += scnprintf(p, sizeof(str) - (p - str),
					i == 0 ? "(%08x) " : "%08x ", val);
		else {
			p += scnprintf(p, sizeof(str) - (p - str), "bad value");
			break;
		}
	}

	pr_emerg("%s Code: %s\n", rname, str);
}

/* In line with __show_regs from arch/arm64/kernel/process.c */
static void show_regs_min(struct pt_regs *regs)
{
	ssize_t i = 29;

	pr_emerg("pc : %pS\n", (void *)regs->pc);
	pr_emerg("lr : %pS\n", (void *)regs->regs[30]);
	pr_emerg("sp : %016llx\n", regs->sp);

	while (i >= 0) {
		pr_emerg("x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			pr_cont("x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		pr_cont("\n");
	}
}

/* NOTE: BACKPORT from android12-5.10/msm-kernel, qcom_cpu_vendor_hooks.c */
static void sec_trace_android_rvh_do_undefinstr(void *unused,
		struct pt_regs *regs, bool user)
{
	if (!user) {
		dump_instr("PC", regs->pc);
		dump_instr("LR", regs->regs[30]);
		show_regs_min(regs);
	}
}

int sec_qc_vendor_hooks_init(struct builder *bd)
{
	struct device *dev = bd->dev;
	int err;

	if (!IS_BUILTIN(CONFIG_SEC_QC_DEBUG))
		return 0;

	err = register_trace_android_rvh_do_undefinstr(
			sec_trace_android_rvh_do_undefinstr, NULL);
	if (err)
		dev_warn(dev, "Failed to android_rvh_do_undefinstr hook\n");

	return 0;
}
