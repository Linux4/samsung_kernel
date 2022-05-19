// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-kernel.h"
#include "hardware/dsp-ctrl.h"
#include "hardware/dsp-dump.h"
#include "hardware/dsp-mailbox.h"

static int g_dump_value;

void dsp_dump_set_value(unsigned int dump_value)
{
	dsp_enter();

	g_dump_value = dump_value;

	dsp_leave();
}

void dsp_dump_print_value(void)
{
	dsp_enter();

	dsp_info("current dump_value : 0x%x\n", g_dump_value);

	dsp_leave();
}

void dsp_dump_print_status_user(struct seq_file *file)
{
	unsigned int idx;

	dsp_enter();

	seq_printf(file, "current dump_value : 0x%x / default : 0x%x\n",
			g_dump_value, DSP_DUMP_DEFAULT_VALUE);

	for (idx = 0; idx < DSP_DUMP_VALUE_END; idx++) {
		seq_printf(file, "[%2d][%s] - %s\n", idx,
				g_dump_value & BIT(idx) ? "*" : " ",
				dump_value_name[idx]);
	}

	dsp_leave();
}

void dsp_dump_ctrl(void)
{
	int idx;
	int start, end;
	unsigned int dump_value = g_dump_value;

	dsp_enter();

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC)) {
		start = DSPC_CA5_INTR_STATUS_NS;
		end = DSPC_TO_ABOX_MAILBOX_S + 1;
		dsp_notice("SYSCTRL_DSPC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_AAREG)) {
		start = SEMAPHORE_REG;
		end = CLEAR_EXCL + 1;
		dsp_notice("AAREG register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_WDT)) {
		start = DSPC_WTCON;
		end = DSPC_WTMINCNT + 1;
		dsp_notice("WDT register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SDMA_SS)) {
		start = VERSION;
		end = NPUFMT_CFG_VC23 + 1;
		dsp_notice("SDMA_SS register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_PWM)) {
		start = PWM_CONFIG0;
		end = TINT_CSTAT + 1;
		dsp_notice("PMW register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_CPU_SS)) {
		start = DSPC_CPU_REMAPS0_NS;
		end = DSPC_WR2C_EN + 1;
		dsp_notice("CPU_SS register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_GIC)) {
		start = GICD_CTLR;
		end = GICC_DIR + 1;
		dsp_notice("GIC register dump (count:%d)\n", end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC0)) {
		start = DSP0_MCGEN;
		end = DSP0_IVP_MAILBOX_TH1 + 1;
		dsp_notice("SYSCTRL_DSPC0 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC1)) {
		start = DSP1_MCGEN;
		end = DSP1_IVP_MAILBOX_TH1 + 1;
		dsp_notice("SYSCTRL_DSPC1 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC2)) {
		start = DSP2_MCGEN;
		end = DSP2_IVP_MAILBOX_TH1 + 1;
		dsp_notice("SYSCTRL_DSPC2 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_RESERVED_SM))
		dsp_ctrl_reserved_sm_dump();

	if (dump_value & BIT(DSP_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_userdefined_dump();

	if (dump_value & BIT(DSP_DUMP_VALUE_FW_INFO))
		dsp_ctrl_fw_info_dump();

	if (dump_value & BIT(DSP_DUMP_VALUE_PC))
		dsp_ctrl_pc_dump();

	dsp_leave();
}

void dsp_dump_ctrl_user(struct seq_file *file)
{
	int idx;
	int start, end;
	unsigned int dump_value = g_dump_value;

	dsp_enter();

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC)) {
		start = DSPC_CA5_INTR_STATUS_NS;
		end = DSPC_TO_ABOX_MAILBOX_S + 1;
		seq_printf(file, "SYSCTRL_DSPC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_AAREG)) {
		start = SEMAPHORE_REG;
		end = CLEAR_EXCL + 1;
		seq_printf(file, "AAREG register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_WDT)) {
		start = DSPC_WTCON;
		end = DSPC_WTMINCNT + 1;
		seq_printf(file, "WDT register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SDMA_SS)) {
		start = VERSION;
		end = NPUFMT_CFG_VC23 + 1;
		seq_printf(file, "SDMA_SS register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_PWM)) {
		start = PWM_CONFIG0;
		end = TINT_CSTAT + 1;
		seq_printf(file, "PMW register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_CPU_SS)) {
		start = DSPC_CPU_REMAPS0_NS;
		end = DSPC_WR2C_EN + 1;
		seq_printf(file, "CPU_SS register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_GIC)) {
		start = GICD_CTLR;
		end = GICC_DIR + 1;
		seq_printf(file, "GIC register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC0)) {
		start = DSP0_MCGEN;
		end = DSP0_IVP_MAILBOX_TH1 + 1;
		seq_printf(file, "SYSCTRL_DSPC0 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC1)) {
		start = DSP1_MCGEN;
		end = DSP1_IVP_MAILBOX_TH1 + 1;
		seq_printf(file, "SYSCTRL_DSPC1 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_SYSCTRL_DSPC2)) {
		start = DSP2_MCGEN;
		end = DSP2_IVP_MAILBOX_TH1 + 1;
		seq_printf(file, "SYSCTRL_DSPC2 register dump (count:%d)\n",
				end - start);
		for (idx = start; idx < end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_DUMP_VALUE_RESERVED_SM))
		dsp_ctrl_user_reserved_sm_dump(file);

	if (dump_value & BIT(DSP_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_user_userdefined_dump(file);

	if (dump_value & BIT(DSP_DUMP_VALUE_FW_INFO))
		dsp_ctrl_user_fw_info_dump(file);

	if (dump_value & BIT(DSP_DUMP_VALUE_PC))
		dsp_ctrl_user_pc_dump(file);

	dsp_leave();
}

void dsp_dump_mailbox_pool_error(struct dsp_mailbox_pool *pool)
{
	dsp_enter();

	if (g_dump_value & BIT(DSP_DUMP_VALUE_MBOX_ERROR))
		dsp_mailbox_dump_pool(pool);

	dsp_leave();
}

void dsp_dump_mailbox_pool_debug(struct dsp_mailbox_pool *pool)
{
	dsp_enter();

	if (g_dump_value & BIT(DSP_DUMP_VALUE_MBOX_DEBUG))
		dsp_mailbox_dump_pool(pool);

	dsp_leave();
}

void dsp_dump_task_manager_count(struct dsp_task_manager *tmgr)
{
	dsp_enter();

	if (g_dump_value & BIT(DSP_DUMP_VALUE_TASK_COUNT))
		dsp_task_manager_dump_count(tmgr);

	dsp_leave();
}

void dsp_dump_kernel(struct dsp_kernel_manager *kmgr)
{
	dsp_enter();

	if (g_dump_value & BIT(DSP_DUMP_VALUE_DL_LOG_ON))
		dsp_kernel_dump(kmgr);

	dsp_leave();
}
