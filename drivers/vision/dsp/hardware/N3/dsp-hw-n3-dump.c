// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-kernel.h"
#include "dsp-hw-n3-system.h"
#include "dsp-hw-n3-ctrl.h"
#include "dsp-hw-n3-mailbox.h"
#include "dsp-hw-n3-dump.h"

static const char *const n3_dump_value_name[] = {
	"DSP_N3_DUMP_VALUE_MBOX_ERROR",
	"DSP_N3_DUMP_VALUE_MBOX_DEBUG",
	"DSP_N3_DUMP_VALUE_DL_LOG_ON",
	"DSP_N3_DUMP_VALUE_DL_LOG_RESERVED",
	"DSP_N3_DUMP_VALUE_SYSCTRL_DSPC",
	"DSP_N3_DUMP_VALUE_AAREG",
	"DSP_N3_DUMP_VALUE_WDT",
	"DSP_N3_DUMP_VALUE_SDMA_SS",
	"DSP_N3_DUMP_VALUE_PWM",
	"DSP_N3_DUMP_VALUE_CPU_SS",
	"DSP_N3_DUMP_VALUE_GIC",
	"DSP_N3_DUMP_VALUE_SYSCTRL_DSPC0",
	"DSP_N3_DUMP_VALUE_SYSCTRL_DSPC1",
	"DSP_N3_DUMP_VALUE_DHCP",
	"DSP_N3_DUMP_VALUE_USERDEFINED",
	"DSP_N3_DUMP_VALUE_FW_INFO",
	"DSP_N3_DUMP_VALUE_PC",
	"DSP_N3_DUMP_VALUE_TASK_COUNT",
};

static void dsp_hw_n3_dump_set_value(struct dsp_dump *dump,
		unsigned int dump_value)
{
	dsp_enter();
	dump->dump_value = dump_value;
	dsp_leave();
}

static void dsp_hw_n3_dump_print_value(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_info("current dump_value : 0x%x\n", dump->dump_value);
	dsp_leave();
}

static void dsp_hw_n3_dump_print_status_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	unsigned int idx;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	seq_printf(file, "current dump_value : 0x%x / default : 0x%x\n",
			dump_value, DSP_N3_DUMP_DEFAULT_VALUE);

	for (idx = 0; idx < DSP_N3_DUMP_VALUE_END; idx++) {
		seq_printf(file, "[%2d][%s] - %s\n", idx,
				dump_value & BIT(idx) ? "*" : " ",
				n3_dump_value_name[idx]);
	}

	dsp_leave();
}

static void dsp_hw_n3_dump_ctrl(struct dsp_dump *dump)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	dsp_dump_print_value();
	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC)) {
		start = DSP_N3_DSPC_CA5_INTR_STATUS_NS;
		end = DSP_N3_DSPC_TO_ABOX_MAILBOX_S + 1;
		dsp_notice("SYSCTRL_DSPC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_AAREG)) {
		start = DSP_N3_SEMAPHORE_REG;
		end = DSP_N3_CLEAR_EXCL + 1;
		dsp_notice("AAREG register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_WDT)) {
		start = DSP_N3_DSPC_WTCON;
		end = DSP_N3_DSPC_WTMINCNT + 1;
		dsp_notice("WDT register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SDMA_SS)) {
		start = DSP_N3_VERSION;
		end = DSP_N3_NPUFMT_CFG_VC23 + 1;
		dsp_notice("SDMA_SS register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_PWM)) {
		start = DSP_N3_PWM_CONFIG0;
		end = DSP_N3_TINT_CSTAT + 1;
		dsp_notice("PMW register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_CPU_SS)) {
		start = DSP_N3_DSPC_CPU_REMAPS0_NS;
		end = DSP_N3_DSPC_WR2C_EN + 1;
		dsp_notice("CPU_SS register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_GIC)) {
		start = DSP_N3_GICD_CTLR;
		end = DSP_N3_GICC_DIR + 1;
		dsp_notice("GIC register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC0)) {
		start = DSP_N3_DSP0_MCGEN;
		end = DSP_N3_DSP0_IVP_MAILBOX_TH1 + 1;
		dsp_notice("SYSCTRL_DSPC0 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC1)) {
		start = DSP_N3_DSP1_MCGEN;
		end = DSP_N3_DSP1_IVP_MAILBOX_TH1 + 1;
		dsp_notice("SYSCTRL_DSPC1 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_DHCP))
		dsp_ctrl_dhcp_dump();

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_userdefined_dump();

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_FW_INFO))
		dsp_ctrl_fw_info_dump();

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_PC))
		dsp_ctrl_pc_dump();

	dsp_leave();
}

static void dsp_hw_n3_dump_ctrl_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC)) {
		start = DSP_N3_DSPC_CA5_INTR_STATUS_NS;
		end = DSP_N3_DSPC_TO_ABOX_MAILBOX_S + 1;
		seq_printf(file, "SYSCTRL_DSPC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_AAREG)) {
		start = DSP_N3_SEMAPHORE_REG;
		end = DSP_N3_CLEAR_EXCL + 1;
		seq_printf(file, "AAREG register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_WDT)) {
		start = DSP_N3_DSPC_WTCON;
		end = DSP_N3_DSPC_WTMINCNT + 1;
		seq_printf(file, "WDT register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SDMA_SS)) {
		start = DSP_N3_VERSION;
		end = DSP_N3_NPUFMT_CFG_VC23 + 1;
		seq_printf(file, "SDMA_SS register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_PWM)) {
		start = DSP_N3_PWM_CONFIG0;
		end = DSP_N3_TINT_CSTAT + 1;
		seq_printf(file, "PMW register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_CPU_SS)) {
		start = DSP_N3_DSPC_CPU_REMAPS0_NS;
		end = DSP_N3_DSPC_WR2C_EN + 1;
		seq_printf(file, "CPU_SS register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_GIC)) {
		start = DSP_N3_GICD_CTLR;
		end = DSP_N3_GICC_DIR + 1;
		seq_printf(file, "GIC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC0)) {
		start = DSP_N3_DSP0_MCGEN;
		end = DSP_N3_DSP0_IVP_MAILBOX_TH1 + 1;
		seq_printf(file, "SYSCTRL_DSPC0 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_SYSCTRL_DSPC1)) {
		start = DSP_N3_DSP1_MCGEN;
		end = DSP_N3_DSP1_IVP_MAILBOX_TH1 + 1;
		seq_printf(file, "SYSCTRL_DSPC1 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_DHCP))
		dsp_ctrl_user_dhcp_dump(file);

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_user_userdefined_dump(file);

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_FW_INFO))
		dsp_ctrl_user_fw_info_dump(file);

	if (dump_value & BIT(DSP_N3_DUMP_VALUE_PC))
		dsp_ctrl_user_pc_dump(file);

	dsp_leave();
}

static void dsp_hw_n3_dump_mailbox_pool_error(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_N3_DUMP_VALUE_MBOX_ERROR))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_n3_dump_mailbox_pool_debug(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_N3_DUMP_VALUE_MBOX_DEBUG))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_n3_dump_task_manager_count(struct dsp_dump *dump,
		struct dsp_task_manager *tmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_N3_DUMP_VALUE_TASK_COUNT))
		dsp_task_manager_dump_count(tmgr);
	dsp_leave();
}

static void dsp_hw_n3_dump_kernel(struct dsp_dump *dump,
		struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_N3_DUMP_VALUE_DL_LOG_ON))
		dsp_kernel_dump(kmgr);
	dsp_leave();
}

static int dsp_hw_n3_dump_open(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_dump_close(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_dump_probe(struct dsp_system *sys)
{
	struct dsp_dump *dump;

	dsp_enter();
	dump = &sys->dump;
	dump->dump_value = DSP_N3_DUMP_DEFAULT_VALUE;
	dsp_leave();
	return 0;
}

static void dsp_hw_n3_dump_remove(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_dump_ops n3_dump_ops = {
	.set_value		= dsp_hw_n3_dump_set_value,
	.print_value		= dsp_hw_n3_dump_print_value,
	.print_status_user	= dsp_hw_n3_dump_print_status_user,
	.ctrl			= dsp_hw_n3_dump_ctrl,
	.ctrl_user		= dsp_hw_n3_dump_ctrl_user,

	.mailbox_pool_error	= dsp_hw_n3_dump_mailbox_pool_error,
	.mailbox_pool_debug	= dsp_hw_n3_dump_mailbox_pool_debug,
	.task_manager_count	= dsp_hw_n3_dump_task_manager_count,
	.kernel			= dsp_hw_n3_dump_kernel,

	.open			= dsp_hw_n3_dump_open,
	.close			= dsp_hw_n3_dump_close,
	.probe			= dsp_hw_n3_dump_probe,
	.remove			= dsp_hw_n3_dump_remove,
};

int dsp_hw_n3_dump_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_dump_register_ops(DSP_DEVICE_ID_N3, &n3_dump_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
