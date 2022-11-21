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
#include "dsp-hw-o3-system.h"
#include "dsp-hw-o3-ctrl.h"
#include "dsp-hw-o3-mailbox.h"
#include "dsp-hw-o3-dump.h"

static const char *const o3_dump_value_name[] = {
	"DSP_O3_DUMP_VALUE_MBOX_ERROR",
	"DSP_O3_DUMP_VALUE_MBOX_DEBUG",
	"DSP_O3_DUMP_VALUE_DL_LOG_ON",
	"DSP_O3_DUMP_VALUE_DL_LOG_RESERVED",
	"DSP_O3_DUMP_VALUE_IAC_S",
	"DSP_O3_DUMP_VALUE_AAREG_S",
	"DSP_O3_DUMP_VALUE_IAC_NS",
	"DSP_O3_DUMP_VALUE_AAREG_NS",
	"DSP_O3_DUMP_VALUE_GIC",
	"DSP_O3_DUMP_VALUE_NPUS",
	"DSP_O3_DUMP_VALUE_SDMA",
	"DSP_O3_DUMP_VALUE_MBOX_EXT_NS",
	"DSP_O3_DUMP_VALUE_VPD0_CTRL",
	"DSP_O3_DUMP_VALUE_DHCP",
	"DSP_O3_DUMP_VALUE_USERDEFINED",
	"DSP_O3_DUMP_VALUE_FW_INFO",
	"DSP_O3_DUMP_VALUE_PC",
	"DSP_O3_DUMP_VALUE_TASK_COUNT",
};

static void dsp_hw_o3_dump_set_value(struct dsp_dump *dump,
		unsigned int dump_value)
{
	dsp_enter();
	dump->dump_value = dump_value;
	dsp_leave();
}

static void dsp_hw_o3_dump_print_value(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_info("current dump_value : 0x%x\n", dump->dump_value);
	dsp_leave();
}

static void dsp_hw_o3_dump_print_status_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	unsigned int idx;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	seq_printf(file, "current dump_value : 0x%x / default : 0x%x\n",
			dump_value, DSP_O3_DUMP_DEFAULT_VALUE);

	for (idx = 0; idx < DSP_O3_DUMP_VALUE_END; idx++) {
		seq_printf(file, "[%2d][%s] - %s\n", idx,
				dump_value & BIT(idx) ? "*" : " ",
				o3_dump_value_name[idx]);
	}

	dsp_leave();
}

static void dsp_hw_o3_dump_ctrl(struct dsp_dump *dump)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	dsp_dump_print_value();
#ifdef ENABLE_DSP_O3_REG_IAC_S
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_IAC_S)) {
		start = DSP_O3_IAC_S_IAC_CTRL;
		end = DSP_O3_IAC_S_MBOX_IVP1_TH1_TO_CC$;
		dsp_notice("IAC_S register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_AAREG_S
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_AAREG_S)) {
		start = DSP_O3_AAREG_S_SEMAPHORE_REG$;
		end = DSP_O3_AAREG_S_SEMAPHORE_REG$;
		dsp_notice("AAREG_S register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_IAC_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_IAC_NS)) {
		start = DSP_O3_IAC_NS_SRESET;
		end = DSP_O3_IAC_NS_MBOX_IVP1_TH1_TO_CC$;
		dsp_notice("IAC_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_AAREG_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_AAREG_NS)) {
		start = DSP_O3_AAREG_NS_SEMAPHORE_REG$;
		end = DSP_O3_AAREG_NS_SEMAPHORE_REG$;
		dsp_notice("AAREG_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_GIC
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_GIC)) {
		start = DSP_O3_GICD_CTLR;
		end = DSP_O3_GICC_DIR;
		dsp_notice("GIC register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_NPUS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_NPUS)) {
		start = DSP_O3_NPUS_USER_REG0;
		end = DSP_O3_NPUS_INTC1_IPRIO_NS_PEND;
		dsp_notice("NPUS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_SDMA
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_SDMA)) {
		start = DSP_O3_SDMA_VERSION;
		end = DSP_O3_SDMA_DEBUG_REG0;
		dsp_notice("SDMA register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_MBOX_EXT_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_MBOX_EXT_NS)) {
		start = DSP_O3_MBOX_EXT_NS_MCUCTRL;
		end = DSP_O3_MBOX_EXT_NS_INT_0_MSR;
		dsp_notice("MBOX_EXT_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_VPD0_CTRL
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_VPD0_CTRL)) {
		start = DSP_O3_VPD0_MCGEN;
		end = DSP_O3_VPD0_IVP1_MSG_TH1_$;
		dsp_notice("VPD0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}
#endif

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_DHCP))
		dsp_ctrl_dhcp_dump();

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_userdefined_dump();

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_FW_INFO))
		dsp_ctrl_fw_info_dump();

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_PC))
		dsp_ctrl_pc_dump();

	dsp_leave();
}

static void dsp_hw_o3_dump_ctrl_user(struct dsp_dump *dump,
		struct seq_file *file)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
#ifdef ENABLE_DSP_O3_REG_IAC_S
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_IAC_S)) {
		start = DSP_O3_IAC_S_IAC_CTRL;
		end = DSP_O3_IAC_S_MBOX_IVP1_TH1_TO_CC$;
		seq_printf(file, "IAC_S register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_AAREG_S
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_AAREG_S)) {
		start = DSP_O3_AAREG_S_SEMAPHORE_REG$;
		end = DSP_O3_AAREG_S_SEMAPHORE_REG$;
		seq_printf(file, "AAREG_S register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_IAC_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_IAC_NS)) {
		start = DSP_O3_IAC_NS_SRESET;
		end = DSP_O3_IAC_NS_MBOX_IVP1_TH1_TO_CC$;
		seq_printf(file, "IAC_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_AAREG_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_AAREG_NS)) {
		start = DSP_O3_AAREG_NS_SEMAPHORE_REG$;
		end = DSP_O3_AAREG_NS_SEMAPHORE_REG$;
		seq_printf(file, "AAREG_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_GIC
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_GIC)) {
		start = DSP_O3_GICD_CTLR;
		end = DSP_O3_GICC_DIR;
		seq_printf(file, "GIC register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_NPUS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_NPUS)) {
		start = DSP_O3_NPUS_USER_REG0;
		end = DSP_O3_NPUS_INTC1_IPRIO_NS_PEND;
		seq_printf(file, "NPUS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_SDMA
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_SDMA)) {
		start = DSP_O3_SDMA_VERSION;
		end = DSP_O3_SDMA_DEBUG_REG0;
		seq_printf(file, "SDMA register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_MBOX_EXT_NS
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_MBOX_EXT_NS)) {
		start = DSP_O3_MBOX_EXT_NS_MCUCTRL;
		end = DSP_O3_MBOX_EXT_NS_INT_0_MSR;
		seq_printf(file, "MBOX_EXT_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

#ifdef ENABLE_DSP_O3_REG_VPD0_CTRL
	if (dump_value & BIT(DSP_O3_DUMP_VALUE_VPD0_CTRL)) {
		start = DSP_O3_VPD0_MCGEN;
		end = DSP_O3_VPD0_IVP1_MSG_TH1_$;
		seq_printf(file, "VPD0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}
#endif

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_DHCP))
		dsp_ctrl_user_dhcp_dump(file);

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_user_userdefined_dump(file);

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_FW_INFO))
		dsp_ctrl_user_fw_info_dump(file);

	if (dump_value & BIT(DSP_O3_DUMP_VALUE_PC))
		dsp_ctrl_user_pc_dump(file);

	dsp_leave();
}

static void dsp_hw_o3_dump_mailbox_pool_error(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O3_DUMP_VALUE_MBOX_ERROR))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_o3_dump_mailbox_pool_debug(struct dsp_dump *dump,
		struct dsp_mailbox_pool *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O3_DUMP_VALUE_MBOX_DEBUG))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_o3_dump_task_manager_count(struct dsp_dump *dump,
		struct dsp_task_manager *tmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O3_DUMP_VALUE_TASK_COUNT))
		dsp_task_manager_dump_count(tmgr);
	dsp_leave();
}

static void dsp_hw_o3_dump_kernel(struct dsp_dump *dump,
		struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_O3_DUMP_VALUE_DL_LOG_ON))
		dsp_kernel_dump(kmgr);
	dsp_leave();
}

static int dsp_hw_o3_dump_open(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_dump_close(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_dump_probe(struct dsp_system *sys)
{
	struct dsp_dump *dump;

	dsp_enter();
	dump = &sys->dump;
	dump->dump_value = DSP_O3_DUMP_DEFAULT_VALUE;
	dsp_leave();
	return 0;
}

static void dsp_hw_o3_dump_remove(struct dsp_dump *dump)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_dump_ops o3_dump_ops = {
	.set_value		= dsp_hw_o3_dump_set_value,
	.print_value		= dsp_hw_o3_dump_print_value,
	.print_status_user	= dsp_hw_o3_dump_print_status_user,
	.ctrl			= dsp_hw_o3_dump_ctrl,
	.ctrl_user		= dsp_hw_o3_dump_ctrl_user,

	.mailbox_pool_error	= dsp_hw_o3_dump_mailbox_pool_error,
	.mailbox_pool_debug	= dsp_hw_o3_dump_mailbox_pool_debug,
	.task_manager_count	= dsp_hw_o3_dump_task_manager_count,
	.kernel			= dsp_hw_o3_dump_kernel,

	.open			= dsp_hw_o3_dump_open,
	.close			= dsp_hw_o3_dump_close,
	.probe			= dsp_hw_o3_dump_probe,
	.remove			= dsp_hw_o3_dump_remove,
};

int dsp_hw_o3_dump_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_dump_register_ops(DSP_DEVICE_ID_O3, &o3_dump_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
