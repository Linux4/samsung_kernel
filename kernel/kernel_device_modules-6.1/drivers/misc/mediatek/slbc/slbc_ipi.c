// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/scmi_protocol.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <tinysys-scmi.h>
#endif /* CONFIG_MTK_TINYSYS_SCMI */

#include "slbc_ipi.h"
#include "slbc_ops.h"
#include "slbc.h"
#include <mtk_slbc_sram.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static int slbc_sspm_ready;
static int scmi_slbc_id;
static struct scmi_tinysys_info_st *_tinfo;
static unsigned int scmi_id;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
static struct slbc_ipi_ops *ipi_ops;
static DEFINE_MUTEX(slbc_scmi_lock);

int slbc_sspm_slb_disable(int disable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLB_DISABLE;
	slbc_ipi_d.arg = disable;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_sspm_slb_disable);

int slbc_sspm_slc_disable(int disable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLC_DISABLE;
	slbc_ipi_d.arg = disable;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_sspm_slc_disable);

int slbc_sspm_enable(int enable)
{
	struct slbc_ipi_data slbc_ipi_d;

	pr_info("#@# %s(%d) enable %d\n", __func__, __LINE__, enable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_ENABLE;
	slbc_ipi_d.arg = enable;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_sspm_enable);

int slbc_force_scmi_cmd(unsigned int force)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_FORCE;
	slbc_ipi_d.arg = force;

	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_force_scmi_cmd);

int slbc_mic_num_cmd(unsigned int num)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_MIC_NUM;
	slbc_ipi_d.arg = num;

	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_mic_num_cmd);

int slbc_inner_cmd(unsigned int inner)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_INNER;
	slbc_ipi_d.arg = inner;

	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_inner_cmd);

int slbc_outer_cmd(unsigned int outer)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_OUTER;
	slbc_ipi_d.arg = outer;

	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_outer_cmd);

int slbc_suspend_resume_notify(int suspend)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_SUSPEND_RESUME_NOTIFY;
	slbc_ipi_d.arg = suspend;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_suspend_resume_notify);

int slbc_table_gid_set(int gid, int quota, int pri)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_SET;
	slbc_ipi_d.arg = gid;
	slbc_ipi_d.arg2 = quota;
	slbc_ipi_d.arg3 = pri;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_set);

int slbc_table_gid_release(int gid)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_RELEASE;
	slbc_ipi_d.arg = gid;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_release);

int slbc_table_gid_get(int gid)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_GET;
	slbc_ipi_d.arg = gid;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_get);

int slbc_table_idt_set(int index, int arid, int idt)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_SET;
	slbc_ipi_d.arg = index;
	slbc_ipi_d.arg2 = arid;
	slbc_ipi_d.arg3 = idt;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_idt_set);

int slbc_table_idt_release(int index)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_RELEASE;
	slbc_ipi_d.arg = index;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_idt_release);

int slbc_table_idt_get(int index)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_GET;
	slbc_ipi_d.arg = index;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_idt_get);

int slbc_table_gid_axi_set(int index, int axiid, int pg)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_SET;
	slbc_ipi_d.arg = index;
	slbc_ipi_d.arg2 = axiid;
	slbc_ipi_d.arg3 = pg;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_set);

int slbc_table_gid_axi_release(int index)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_RELEASE;
	slbc_ipi_d.arg = index;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_release);

int slbc_table_gid_axi_get(int index)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_GET;
	slbc_ipi_d.arg = index;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_get);

int emi_slb_select(int argv1, int argv2, int argv3)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_SLB_SELECT;
	slbc_ipi_d.arg = argv1;
	slbc_ipi_d.arg2 = argv2;
	slbc_ipi_d.arg3 = argv3;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(emi_slb_select);

int emi_pmu_counter(int idx, int filter0, int bw_lat_sel)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_PMU_COUNTER;
	slbc_ipi_d.arg = idx;
	slbc_ipi_d.arg2 = filter0;
	slbc_ipi_d.arg3 = bw_lat_sel;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(emi_pmu_counter);

int emi_pmu_set_ctrl(int feature, int idx, int action)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_PMU_SET_CTRL;
	slbc_ipi_d.arg = feature;
	slbc_ipi_d.arg2 = idx;
	slbc_ipi_d.arg3 = action;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(emi_pmu_set_ctrl);

int emi_pmu_read_counter(int idx)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_EMI_PMU_READ_COUNTER, idx);

	ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return  -1;
	}

	return rvalue.r1;
}
EXPORT_SYMBOL_GPL(emi_pmu_read_counter);

int emi_gid_pmu_counter(int idx, int set)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_GID_PMU_COUNTER;
	slbc_ipi_d.arg = idx;
	slbc_ipi_d.arg2 = set;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(emi_gid_pmu_counter);

int emi_gid_pmu_read_counter(void *ptr)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_status rvalue = {0};
	struct slbc_data *d = (struct slbc_data *)ptr;
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_EMI_GID_PMU_READ_COUNTER, d->uid);

	ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return -1;
	}

	d->type = rvalue.r1;
	d->flag = rvalue.r2;
	d->timeout = rvalue.r3;

	return ret;
}
EXPORT_SYMBOL_GPL(emi_gid_pmu_read_counter);

int emi_slc_test_result(void)
{
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_EMI_SLC_TEST_RESULT, 0);

	ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return  -1;
	}

	return rvalue.r1;
}
EXPORT_SYMBOL_GPL(emi_slc_test_result);

int slbc_set_scmi_info(int uid, uint16_t cmd, int arg, int arg2, int arg3)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(cmd, uid);
	slbc_ipi_d.arg = arg;
	slbc_ipi_d.arg2 = arg2;
	slbc_ipi_d.arg3 = arg3;

	ret = slbc_scmi_set(&slbc_ipi_d);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_set_scmi_info);

int slbc_get_scmi_info(int uid, uint16_t cmd, void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(cmd, uid);

	ret = slbc_scmi_get(&slbc_ipi_d, (struct scmi_tinysys_status *)ptr);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_get_scmi_info);

int _slbc_request_cache_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_CACHE_REQUEST_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->size = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_request_cache_scmi);

int _slbc_release_cache_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_CACHE_RELEASE_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->size = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_release_cache_scmi);

int _slbc_buffer_status_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_BUFFER_STATUS, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
		if (!ret) {
			ret = rvalue.r1;
			pr_info("#@# %s(%d) uid %d return ref(%d)\n",
					__func__, __LINE__, d->uid, ret);
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_buffer_status_scmi);

int _slbc_request_buffer_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_BUFFER_REQUEST_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_set(&slbc_ipi_d);
		if (ret) {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;

			return ret;
		}

		ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->size = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_request_buffer_scmi);

int _slbc_release_buffer_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = SLBC_IPI(IPI_SLBC_BUFFER_RELEASE_FROM_AP, d->uid);
	slbc_ipi_d.arg = slbc_data_to_ui(d);
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_set(&slbc_ipi_d);
		if (ret) {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;

			return ret;
		}

		ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.r1;
			d->size = rvalue.r2;
			ret = d->ret = rvalue.r3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_release_buffer_scmi);

int _slbc_ach_scmi(unsigned int cmd, enum slc_ach_uid uid, int gid, struct slbc_gid_data *data)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd		= SLBC_IPI(cmd, uid);
	slbc_ipi_d.arg		= gid;
	if (cmd == IPI_SLBC_GID_REQUEST_FROM_AP || cmd == IPI_SLBC_ROI_UPDATE_FROM_AP) {
		slbc_ipi_d.arg2		= data->bw;
		slbc_ipi_d.arg3		= data->dma_size;
	} else if (cmd == IPI_SLBC_GID_READ_INVALID_FROM_AP) {
		slbc_ipi_d.arg2		= data->bw;	// re-use bw as enable argument
	}

	ret = slbc_scmi_set(&slbc_ipi_d);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;

		return ret;
	}

	ret = slbc_scmi_get(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_ach_scmi);

int slbc_sspm_sram_update(void)
{
	struct slbc_ipi_data slbc_ipi_d;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_SRAM_UPDATE;
	slbc_ipi_d.arg = 0;
	return slbc_scmi_set(&slbc_ipi_d);
}
EXPORT_SYMBOL_GPL(slbc_sspm_sram_update);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static void slbc_scmi_handler(u32 r_feature_id, scmi_tinysys_report *report)
{
	struct slbc_data d;
	unsigned int cmd;
	unsigned int arg;
	unsigned int arg2;
	unsigned int arg3;

	if (scmi_slbc_id != r_feature_id)
		return;

	cmd = report->p1;
	arg = report->p2;
	arg2 = report->p3;
	arg3 = report->p4;
	/* pr_info("#@# %s(%d) report 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__, */
			/* report->p1, report->p2, report->p3, report->p4); */

	switch (cmd) {
	case IPI_SLBC_SYNC_TO_AP:
		break;
	case IPI_SLBC_ACP_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_request_acp)
				ipi_ops->slbc_request_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_ACP_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_release_acp)
				ipi_ops->slbc_release_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_RELEASE_TO_AP",
					d.type);
		break;
	case IPI_SLBC_MEM_BARRIER:
		if (ipi_ops && ipi_ops->slbc_mem_barrier)
			ipi_ops->slbc_mem_barrier();
		break;
	case IPI_SLBC_BUFFER_CB_NOTIFY:
		if (ipi_ops && ipi_ops->slbc_buffer_cb_notify)
			ipi_ops->slbc_buffer_cb_notify(arg, arg2, arg3);
		break;
	default:
		pr_info("wrong slbc IPI command: %d\n",
				cmd);
	}
}
#endif /* CONFIG_MTK_TINYSYS_SCMI */

int slbc_scmi_set(void *buffer)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	int ret;
	unsigned int local_id;
	struct slbc_ipi_data *slbc_ipi_d = buffer;

	if (slbc_sspm_ready != 1) {
		ret = -1;
		pr_info("slbc scmi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	/* pr_info("#@# %s(%d) id 0x%x cmd 0x%x arg 0x%x\n", */
			/* __func__, __LINE__, */
			/* scmi_slbc_id, slbc_ipi_d->cmd, slbc_ipi_d->arg); */

	mutex_lock(&slbc_scmi_lock);

	local_id = ++scmi_id;
	slbc_sram_write(SLBC_SCMI_AP, local_id);

	mutex_unlock(&slbc_scmi_lock);

	ret = scmi_tinysys_common_set(_tinfo->ph, scmi_slbc_id,
			slbc_ipi_d->cmd, slbc_ipi_d->arg, slbc_ipi_d->arg2, slbc_ipi_d->arg3, 0);

	if (ret == -ETIMEDOUT) {
		mdelay(3);
		if (local_id == slbc_sram_read(SLBC_SCMI_SSPM)) {
			ret = 0;
			pr_info("slbc scmi timed out!\n");
		}
	}

	if (ret) {
		pr_info("slbc scmi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);

		goto error;
	}

error:
	return ret;
#else
	return -1;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}

int slbc_scmi_get(void *buffer, void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	int ret;
	unsigned int local_id;
	struct slbc_ipi_data *slbc_ipi_d = buffer;
	struct scmi_tinysys_status *rvalue = ptr;

	if (slbc_sspm_ready != 1) {
		ret = -1;
		pr_info("slbc scmi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	/* pr_info("#@# %s(%d) id 0x%x cmd 0x%x arg 0x%x\n", */
			/* __func__, __LINE__, */
			/* scmi_slbc_id, slbc_ipi_d->cmd, slbc_ipi_d->arg); */

	mutex_lock(&slbc_scmi_lock);

	local_id = ++scmi_id;
	slbc_sram_write(SLBC_SCMI_AP, local_id);

	mutex_unlock(&slbc_scmi_lock);

	ret = scmi_tinysys_common_get(_tinfo->ph, scmi_slbc_id,
			slbc_ipi_d->cmd, rvalue);

	if (ret == -ETIMEDOUT) {
		mdelay(3);
		if (local_id == slbc_sram_read(SLBC_SCMI_SSPM)) {
			ret = 0;
			rvalue->r1 = slbc_sram_read(SLBC_SCMI_RET1);
			rvalue->r2 = slbc_sram_read(SLBC_SCMI_RET2);
			rvalue->r3 = slbc_sram_read(SLBC_SCMI_RET3);
			pr_info("slbc scmi timed out! return 0x%x 0x%x 0x%x\n",
					rvalue->r1,
					rvalue->r2,
					rvalue->r3);
		}
	}

	if (ret) {
		pr_info("slbc scmi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);

		goto error;
	}

error:
	return ret;
#else
	return -1;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}

int slbc_scmi_init(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	if (!(_tinfo && _tinfo->sdev)) {
		pr_info("slbc call get_scmi_tinysys_info() fail\n");
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi-slbc",
			&scmi_slbc_id);
	if (ret) {
		pr_info("get slbc scmi_slbc fail, ret %d\n", ret);
		slbc_sspm_ready = -2;
		return -EINVAL;
	}
	pr_info("#@# %s(%d) scmi_slbc_id %d\n",
			__func__, __LINE__, scmi_slbc_id);

	scmi_tinysys_register_event_notifier(scmi_slbc_id,
			(f_handler_t)slbc_scmi_handler);

	ret = scmi_tinysys_event_notify(scmi_slbc_id, 1);

	if (ret) {
		pr_info("event notify fail ...");
		return -EINVAL;
	}

	slbc_sspm_ready = 1;

	pr_info("slbc scmi is ready!\n");

#endif /* CONFIG_MTK_TINYSYS_SCMI */
	return 0;
}
EXPORT_SYMBOL_GPL(slbc_scmi_init);

void slbc_register_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = ops;
}
EXPORT_SYMBOL_GPL(slbc_register_ipi_ops);

void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = NULL;
}
EXPORT_SYMBOL_GPL(slbc_unregister_ipi_ops);

MODULE_DESCRIPTION("SLBC scmi Driver v0.1");
MODULE_LICENSE("GPL");
