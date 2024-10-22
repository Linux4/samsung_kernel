// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include "mmqos-vcp.h"
#include "mmqos-test.h"
#include "vcp_helper.h"
#include "vcp_status.h"

static phys_addr_t mmqos_memory_iova;
static phys_addr_t mmqos_memory_pa;
static void *mmqos_memory_va;
static bool mmqos_vcp_cb_ready;
static bool mmqos_vcp_init_done;
static struct notifier_block vcp_ready_notifier;
static int vcp_power;
static DEFINE_MUTEX(mmqos_vcp_pwr_mutex);

static int mmqos_ipi_status;
static DEFINE_MUTEX(mmqos_vcp_ipi_mutex);
static int vcp_mmqos_log;
static int vcp_smi_log;

void *mmqos_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = mmqos_memory_pa;
	return mmqos_memory_va;
}
EXPORT_SYMBOL_GPL(mmqos_get_vcp_base);

bool mmqos_is_init_done(void)
{
	return (mmqos_state != MMQOS_DISABLE) ? mmqos_vcp_init_done : false;
}
EXPORT_SYMBOL_GPL(mmqos_is_init_done);

int mmqos_vcp_ipi_send(const u8 func, const u8 idx, u32 *data)
{
	struct mmqos_ipi_data slot = {
		func, idx, mmqos_memory_iova >> 32, (u32)mmqos_memory_iova};
	int gen, ret = 0, retry = 0;
	static u8 times;
	u32 val;

	if (!mmqos_is_init_done())
		return -ENODEV;

	mutex_lock(&mmqos_vcp_ipi_mutex);
	writel(vcp_mmqos_log, MEM_LOG_FLAG);
	writel(vcp_smi_log, MEM_SMI_LOG_FLAG);
	writel(mmqos_state, MEM_MMQOS_STATE);
	switch (func) {
	case FUNC_MMQOS_INIT:
		// trigger mmqos in vcp to create topology
		break;
	case FUNC_TEST:
		writel(idx, MEM_TEST);
		break;
	case FUNC_SYNC_STATE:
		// change mmqos_state by adb command, should trigger sync state
		break;
	}
	val = readl(MEM_IPI_SYNC_FUNC);
	mutex_unlock(&mmqos_vcp_ipi_mutex);

	while (!is_vcp_ready_ex(VCP_A_ID) || (!mmqos_vcp_cb_ready && func != FUNC_MMQOS_INIT)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			ret = -ETIMEDOUT;
			MMQOS_ERR(
				"ret:%d retry:%d ready:%d cb_ready:%d",
				ret, retry, is_vcp_ready_ex(VCP_A_ID), mmqos_vcp_cb_ready);
			goto ipi_send_end;
		}
		mdelay(1);
	}

	mutex_lock(&mmqos_vcp_ipi_mutex);
	writel(0, MEM_IPI_SYNC_DATA);
	writel(val | (1 << func), MEM_IPI_SYNC_FUNC);
	gen = vcp_cmd_ex(VCP_GET_GEN, "mmqos_ipi_task");

	ret = mtk_ipi_send(vcp_get_ipidev(), IPI_OUT_MMQOS, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMQOS, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE)
		goto ipi_lock_end;

	retry = 0;
	while (!(readl(MEM_IPI_SYNC_DATA) & (1 << func))) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			ret = IPI_COMPL_TIMEOUT;
			MMQOS_ERR(
				"ret:%d retry:%d ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
				ret, retry, is_vcp_ready_ex(VCP_A_ID), mmqos_vcp_cb_ready,
				*(u64 *)&slot, vcp_power, val);
			break;
		}
		if (!is_vcp_ready_ex(VCP_A_ID)) {
			ret = -ETIMEDOUT;
			MMQOS_ERR(
				"ret:%d retry:%d ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
				ret, retry, is_vcp_ready_ex(VCP_A_ID), mmqos_vcp_cb_ready,
				*(u64 *)&slot, vcp_power, val);
			break;
		}
		mdelay(1);
	}

	if (!ret)
		writel(val & ~readl(MEM_IPI_SYNC_DATA), MEM_IPI_SYNC_FUNC);
	else if (gen == vcp_cmd_ex(VCP_GET_GEN, "mmqos_ipi_task")) {
		if (!times) {
			MMQOS_ERR("VCP_SET_HALT");
			vcp_cmd_ex(VCP_SET_HALT, "mmqos_ipi_task");
		}
		times += 1;
	}

ipi_lock_end:
	val = readl(MEM_IPI_SYNC_FUNC);
	mutex_unlock(&mmqos_vcp_ipi_mutex);

ipi_send_end:
	if (ret)
		MMQOS_ERR(
			"ret:%d retry:%d ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
			ret, retry, is_vcp_ready_ex(VCP_A_ID), mmqos_vcp_cb_ready,
			*(u64 *)&slot, vcp_power, val);
	if (log_level & (1 << log_ipi))
		MMQOS_DBG(
			"ret:%d retry:%d ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
			ret, retry, is_vcp_ready_ex(VCP_A_ID), mmqos_vcp_cb_ready,
			*(u64 *)&slot, vcp_power, val);
	mmqos_ipi_status = ret;
	return ret;
}
EXPORT_SYMBOL_GPL(mmqos_vcp_ipi_send);

int mtk_mmqos_enable_vcp(const bool enable)
{
	int ret = 0;

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmqos_vcp_pwr_mutex);
	if (enable) {
		if (!vcp_power) {
			ret = vcp_register_feature_ex(MMQOS_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power += 1;
	} else {
		if (!vcp_power) {
			ret = -EINVAL;
			goto enable_vcp_end;
		}
		if (vcp_power == 1) {
			ret = vcp_deregister_feature_ex(MMQOS_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power -= 1;
	}

enable_vcp_end:
	if (ret)
		MMQOS_ERR("ret:%d enable:%d vcp_power:%d",
			ret, enable, vcp_power);
	if (log_level & log_vcp_pwr)
		MMQOS_DBG("ret:%d enable:%d vcp_power:%d",
			ret, enable, vcp_power);
	mutex_unlock(&mmqos_vcp_pwr_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_enable_vcp);

static int mmqos_vcp_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		MMQOS_DBG("receive VCP_EVENT_READY IPI_SYNC_FUNC=%#x IPI_SYNC_DATA=%#x",
			readl(MEM_IPI_SYNC_FUNC), readl(MEM_IPI_SYNC_DATA));
		mmqos_vcp_ipi_send(FUNC_MMQOS_INIT, 0, NULL);
		mmqos_vcp_cb_ready = true;
		break;
	case VCP_EVENT_STOP:
	case VCP_EVENT_SUSPEND:
		mmqos_vcp_cb_ready = false;
		break;
	}
	return NOTIFY_DONE;
}

int mmqos_vcp_init_thread(void *data)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	struct iommu_domain *domain;
	int retry = 0;

	while (mtk_mmqos_enable_vcp(true)) {
		if (++retry > 100) {
			MMQOS_ERR("vcp is not power on yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	retry = 0;
	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			MMQOS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		mdelay(1);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		if (++retry > 100) {
			MMQOS_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmqos_memory_iova = vcp_get_reserve_mem_phys_ex(MMQOS_MEM_ID);
	domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain)
		mmqos_memory_pa = iommu_iova_to_phys(domain, mmqos_memory_iova);
	mmqos_memory_va = (void *)vcp_get_reserve_mem_virt_ex(MMQOS_MEM_ID);

	writel_relaxed(mmqos_state, MEM_MMQOS_STATE);

	mmqos_vcp_init_done = true;

	MMQOS_DBG("shared memory iova:%pa pa:%pa va:%#lx init_done:%d",
		&mmqos_memory_iova, &mmqos_memory_pa,
		(unsigned long)mmqos_memory_va, mmqos_vcp_init_done);

	vcp_ready_notifier.notifier_call = mmqos_vcp_notifier_callback;
	vcp_A_register_notify_ex(&vcp_ready_notifier);

	mtk_mmqos_enable_vcp(false);

	return 0;
}
EXPORT_SYMBOL_GPL(mmqos_vcp_init_thread);

int mmqos_get_vcp_mmqos_log(char *buf, const struct kernel_param *kp)
{
	int len = 0, ret;

	if (!mmqos_is_init_done())
		return 0;

	ret = readl(MEM_LOG_FLAG);
	len += snprintf(buf + len, PAGE_SIZE - len, "MEM_LOG_FLAG:%#x", ret);
	return len;
}

int mmqos_set_vcp_mmqos_log(const char *val, const struct kernel_param *kp)
{
	u32 log = 0;
	int ret;

	if (!mmqos_is_init_done())
		return 0;

	ret = kstrtou32(val, 0, &log);
	if (ret) {
		MMQOS_ERR("failed:%d log:%#x", ret, log);
		return ret;
	}

	vcp_mmqos_log = log;
	mtk_mmqos_enable_vcp(true);
	ret = mmqos_vcp_ipi_send(FUNC_SYNC_STATE, mmqos_state, NULL);
	mtk_mmqos_enable_vcp(false);
	return 0;
}

static const struct kernel_param_ops mmqos_set_vcp_mmqos_log_ops = {
	.get = mmqos_get_vcp_mmqos_log,
	.set = mmqos_set_vcp_mmqos_log,
};

module_param_cb(vcp_mmqos_log, &mmqos_set_vcp_mmqos_log_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_mmqos_log, "mmqos vcp log");

int mmqos_get_vcp_smi_log(char *buf, const struct kernel_param *kp)
{
	int len = 0, ret;

	if (!mmqos_is_init_done())
		return 0;

	ret = readl(MEM_SMI_LOG_FLAG);
	len += snprintf(buf + len, PAGE_SIZE - len, "MEM_SMI_LOG_FLAG:%#x", ret);
	return len;
}

int mmqos_set_vcp_smi_log(const char *val, const struct kernel_param *kp)
{
	u32 log = 0;
	int ret;

	if (!mmqos_is_init_done())
		return 0;

	ret = kstrtou32(val, 0, &log);
	if (ret) {
		MMQOS_ERR("failed:%d log:%#x", ret, log);
		return ret;
	}

	vcp_smi_log = log;
	mtk_mmqos_enable_vcp(true);
	ret = mmqos_vcp_ipi_send(FUNC_SYNC_STATE, mmqos_state, NULL);
	mtk_mmqos_enable_vcp(false);

	return 0;
}

static const struct kernel_param_ops mmqos_set_vcp_smi_log_ops = {
	.get = mmqos_get_vcp_smi_log,
	.set = mmqos_set_vcp_smi_log,
};
module_param_cb(vcp_smi_log, &mmqos_set_vcp_smi_log_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_smi_log, "smi vcp log");

void mmqos_start_test_id(u32 test_id)
{
	int ret = 0;

	MMQOS_DBG("start test_id:%d", test_id);
	if (test_id < VCP_TEST_NUM) {
		mtk_mmqos_enable_vcp(true);
		ret = mmqos_vcp_ipi_send(FUNC_TEST, test_id, NULL);
		mtk_mmqos_enable_vcp(false);
	} else {
		mmqos_kernel_test(test_id);
	}
	MMQOS_DBG("test done ret:%d", ret);
}

static int mmqos_set_test_id(const char *val, const struct kernel_param *kp)
{
	u32 test_id = 0;
	int ret;

	ret = kstrtou32(val, 0, &test_id);
	if (ret) {
		MMQOS_ERR("failed:%d test_id:%#x", ret, test_id);
		return ret;
	}

	mmqos_start_test_id(test_id);
	return ret;
}

static const struct kernel_param_ops mmqos_test_ops = {
	.set = mmqos_set_test_id,
};
module_param_cb(test_id, &mmqos_test_ops, NULL, 0644);
MODULE_PARM_DESC(test_id, "mmqos vcp test id");

static int mmqos_vcp_stress(const char *val, const struct kernel_param *kp)
{
	u32 test_id = 0;
	u32 node_id, srt_r_bw, srt_w_bw;
	int ret;

	ret = sscanf(val, "%d %x %d %d", &test_id, &node_id, &srt_r_bw, &srt_w_bw);
	if (ret != 4) {
		MMQOS_ERR("failed:%d test_id:%#x", ret, test_id);
		return ret;
	}
	MMQOS_DBG("test_id:%d, node_id:%#x, srt_r_bw:%d, srt_w_bw:%d",
		test_id, node_id, srt_r_bw, srt_w_bw);
	writel(node_id, MEM_TEST_NODE_ID);
	writel(srt_r_bw, MEM_TEST_SRT_R_BW);
	writel(srt_w_bw, MEM_TEST_SRT_W_BW);

	mmqos_start_test_id(test_id);
	return ret;
}

static const struct kernel_param_ops mmqos_stress_ops = {
	.set = mmqos_vcp_stress,
};
module_param_cb(vcp_stress, &mmqos_stress_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_stress, "mmqos vcp stress");
MODULE_LICENSE("GPL");
