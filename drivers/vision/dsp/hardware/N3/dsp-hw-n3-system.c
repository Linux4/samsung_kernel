// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-binary.h"
#include "dsp-hw-n3-bus.h"
#include "dsp-hw-n3-clk.h"
#include "dsp-hw-n3-ctrl.h"
#include "dsp-hw-n3-debug.h"
#include "dsp-hw-n3-dump.h"
#include "dsp-hw-n3-interface.h"
#include "dsp-hw-n3-mailbox.h"
#include "dsp-hw-n3-memory.h"
#include "dsp-hw-n3-pm.h"
#include "dsp-hw-n3-governor.h"
#include "dsp-hw-n3-system.h"

#define DSP_WAIT_BOOT_TIME	(100)
#define DSP_WAIT_MAILBOX_TIME	(1500)
#define DSP_WAIT_RESET_TIME	(100)

#define DSP_STATIC_KERNEL	(1)
#define DSP_DYNAMIC_KERNEL	(2)

struct dsp_hw_n3_system {
	void __iomem		*boot_mem;
	resource_size_t		boot_mem_size;
	unsigned char		boot_bin[SZ_256];
	size_t			boot_bin_size;
	void __iomem		*chip_id;
};

static int dsp_hw_n3_system_request_control(struct dsp_system *sys,
		unsigned int id, union dsp_control *cmd)
{
	int ret;

	dsp_enter();
	switch (id) {
	case DSP_CONTROL_ENABLE_DVFS:
		ret = sys->pm.ops->dvfs_enable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_DISABLE_DVFS:
		ret = sys->pm.ops->dvfs_disable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_ENABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (!sys->boost) {
			ret = sys->pm.ops->boost_enable(&sys->pm);
			if (ret) {
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			ret = sys->bus.ops->mo_get_by_id(&sys->bus,
					DSP_N3_MO_MAX);
			if (ret) {
				sys->pm.ops->boost_disable(&sys->pm);
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			sys->boost = true;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_DISABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (sys->boost) {
			sys->bus.ops->mo_put_by_id(&sys->bus, DSP_N3_MO_MAX);
			sys->pm.ops->boost_disable(&sys->pm);
			sys->boost = false;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_REQUEST_MO:
		ret = sys->bus.ops->mo_get_by_name(&sys->bus,
				cmd->mo.scenario_name);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_RELEASE_MO:
		sys->bus.ops->mo_put_by_name(&sys->bus, cmd->mo.scenario_name);
		break;
	case DSP_CONTROL_REQUEST_PRESET:
		sys->governor.ops->request(&sys->governor, &cmd->preset);
		break;
	default:
		ret = -EINVAL;
		dsp_err("control cmd id is invalid(%u)\n", id);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_n3_system_wait_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;
	long timeout;

	dsp_enter();
	timeout = wait_event_timeout(sys->task_manager.done_wq,
			task->state == DSP_TASK_STATE_COMPLETE,
			msecs_to_jiffies(sys->wait[DSP_SYSTEM_WAIT_MAILBOX]));
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("task wait time(%ums) is expired(%u/%u)\n",
				sys->wait[DSP_SYSTEM_WAIT_MAILBOX],
				task->id, task->message_id);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_TASK_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_N3_DUMP_DEFAULT_VALUE |
					1 << DSP_N3_DUMP_VALUE_SDMA_SS);
			dsp_dump_ctrl();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_n3_system_flush(struct dsp_system *sys)
{
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, true);
	dsp_task_manager_flush_process(tmgr, -ENOSTR);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	sys->ops->reset(sys);
	dsp_leave();
}

static void __dsp_hw_n3_system_recovery(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = sys->ops->boot(sys);
	if (ret) {
		dsp_err("Failed to recovery device\n");
		return;
	}

	dsp_graph_manager_recovery(&sys->dspdev->core.graph_manager);

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, false);
	spin_unlock_irqrestore(&tmgr->slock, flags);
	dsp_leave();
}

static int dsp_hw_n3_system_execute_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;

	dsp_enter();
	if (task->message_id == DSP_COMMON_EXECUTE_MSG)
		sys->governor.ops->check_done(&sys->governor);

	sys->pm.ops->update_devfreq_busy(&sys->pm, task->pool->pm_qos);
	ret = sys->mailbox.ops->send_task(&sys->mailbox, task);
	if (ret)
		goto p_err;

	dsp_dump_mailbox_pool_debug(task->pool);

	/* TODO Devfreq change criteria required if not waiting */
	if (task->wait) {
		ret = __dsp_hw_n3_system_wait_task(sys, task);
		if (ret) {
			if (task->recovery) {
				__dsp_hw_n3_system_flush(sys);
				__dsp_hw_n3_system_recovery(sys);
			}
			goto p_err;
		}

		if (task->result) {
			ret = task->result;
			dsp_err("task result is failure(%d/%u/%u)\n",
					ret, task->id, task->message_id);
			goto p_err;
		}
	}

	sys->pm.ops->update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->normal_count++;
	dsp_leave();
	return 0;
p_err:
	sys->pm.ops->update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->error_count++;
	dsp_dump_mailbox_pool_error(task->pool);
	dsp_dump_task_manager_count(task->owner);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	return ret;
}

static void dsp_hw_n3_system_iovmm_fault_dump(struct dsp_system *sys)
{
	dsp_enter();
	dsp_dump_ctrl();
	dsp_dump_task_manager_count(&sys->task_manager);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	dsp_leave();
}

static int __dsp_hw_n3_system_master_copy(struct dsp_system *sys)
{
	int ret;
	struct dsp_hw_n3_system *sys_sub;
	void __iomem *dst;
	unsigned char *src;
	size_t size;
	unsigned int remain = 0;

	dsp_enter();
	sys_sub = sys->sub_data;

	if (!sys_sub->boot_bin_size) {
		dsp_warn("master bin must not be zero(%zu)\n",
				sys_sub->boot_bin_size);
		ret = dsp_binary_load(DSP_N3_MASTER_FW_NAME, NULL,
				DSP_N3_FW_EXTENSION, sys_sub->boot_bin,
				sizeof(sys_sub->boot_bin),
				&sys_sub->boot_bin_size);
		if (ret)
			goto p_err;
	}
	dst = sys_sub->boot_mem;
	src = sys_sub->boot_bin;
	size = sys_sub->boot_bin_size;

	__iowrite32_copy(dst, src, size >> 2);
	if (size & 0x3) {
		memcpy(&remain, src + (size & ~0x3), size & 0x3);
		writel(remain, dst + (size & ~0x3));
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_n3_system_init(struct dsp_system *sys)
{
	struct dsp_priv_mem *pmem;
	struct dsp_hw_n3_system *sys_sub;
	unsigned int chip_id;

	dsp_enter();
	pmem = sys->memory.priv_mem;
	sys_sub = sys->sub_data;

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_CC_INT_STATUS), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_HOST_INT_STATUS),
			0x0);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_DEBUG_LAYER_START),
			sys->layer_start);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_DEBUG_LAYER_END),
			sys->layer_end);

	memcpy(pmem[DSP_N3_PRIV_MEM_FW].kvaddr,
			pmem[DSP_N3_PRIV_MEM_FW].bac_kvaddr,
			pmem[DSP_N3_PRIV_MEM_FW].used_size);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_FW_RESERVED_SIZE),
			pmem[DSP_N3_PRIV_MEM_FW].size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IVP_PM_IOVA),
			pmem[DSP_N3_PRIV_MEM_IVP_PM].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IVP_PM_SIZE),
			pmem[DSP_N3_PRIV_MEM_IVP_PM].size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IVP_DM_IOVA),
			pmem[DSP_N3_PRIV_MEM_IVP_DM].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IVP_DM_SIZE),
			pmem[DSP_N3_PRIV_MEM_IVP_DM].used_size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IAC_PM_IOVA),
			pmem[DSP_N3_PRIV_MEM_IAC_PM].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IAC_PM_SIZE),
			pmem[DSP_N3_PRIV_MEM_IAC_PM].used_size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IAC_DM_IOVA),
			pmem[DSP_N3_PRIV_MEM_IAC_DM].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_IAC_DM_SIZE),
			pmem[DSP_N3_PRIV_MEM_IAC_DM].used_size);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_MAILBOX_VERSION), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_MESSAGE_VERSION), 0x0);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_FW_LOG_MEMORY_IOVA),
			pmem[DSP_N3_PRIV_MEM_FW_LOG].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_FW_LOG_MEMORY_SIZE),
			pmem[DSP_N3_PRIV_MEM_FW_LOG].size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_CC_MBOX_MEMORY_IOVA),
			pmem[DSP_N3_PRIV_MEM_MBOX_MEMORY].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_CC_MBOX_MEMORY_SIZE),
			pmem[DSP_N3_PRIV_MEM_MBOX_MEMORY].size);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_CC_MBOX_POOL_IOVA),
			pmem[DSP_N3_PRIV_MEM_MBOX_POOL].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TO_CC_MBOX_POOL_SIZE),
			pmem[DSP_N3_PRIV_MEM_MBOX_POOL].size);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_KERNEL_MODE),
			DSP_DYNAMIC_KERNEL);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_DL_OUT_IOVA),
			pmem[DSP_N3_PRIV_MEM_DL_OUT].iova);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_DL_OUT_SIZE),
			pmem[DSP_N3_PRIV_MEM_DL_OUT].size);

	chip_id = readl(sys_sub->chip_id);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_CHIPID_REV), chip_id);
	dsp_info("CHIPID : %#x\n", chip_id);

	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_PRODUCT_ID), 0xE980);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FDC), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FE0), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FE4), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FE8), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FEC), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FF0), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FF4), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FF8), 0x0);
	dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_TEMP_1FFC), 0x0);
	dsp_leave();
}

static int __dsp_hw_n3_system_wait_boot(struct dsp_system *sys)
{
	int ret;
	long timeout;

	dsp_enter();
	timeout = wait_event_timeout(sys->system_wq,
			sys->system_flag & BIT(DSP_SYSTEM_BOOT),
			msecs_to_jiffies(sys->wait[DSP_SYSTEM_WAIT_BOOT]));
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to boot DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_BOOT]);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_BOOT_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_N3_DUMP_DEFAULT_VALUE |
					1 << DSP_N3_DUMP_VALUE_SDMA_SS);
			dsp_dump_ctrl();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	} else {
		dsp_info("Completed to boot DSP\n");
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_n3_system_check_kernel_mode(struct dsp_system *sys)
{
	int kernel_mode;

	dsp_enter();
	kernel_mode = dsp_ctrl_sm_readl(
			DSP_N3_SM_RESERVED(DSP_N3_SM_KERNEL_MODE));
	if (kernel_mode != DSP_DYNAMIC_KERNEL) {
		dsp_err("static kernel is no longer available\n");
		return -EINVAL;
	}

	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_boot(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	sys->system_flag = 0x0;

	if (sys->boot_init & BIT(DSP_SYSTEM_NPU_INIT)) {
		sys->ctrl.ops->extra_init(&sys->ctrl);
	} else {
		sys->ctrl.ops->all_init(&sys->ctrl);
		ret = __dsp_hw_n3_system_master_copy(sys);
		if (ret)
			goto p_err;
	}

	__dsp_hw_n3_system_init(sys);
	sys->debug.ops->log_start(&sys->debug);

	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->ctrl.ops->start(&sys->ctrl);
	ret = __dsp_hw_n3_system_wait_boot(sys);
	sys->pm.ops->update_devfreq_min(&sys->pm);
	if (ret) {
		sys->ctrl.ops->force_reset(&sys->ctrl);
		sys->debug.ops->log_stop(&sys->debug);
		goto p_err;
	}

	ret = __dsp_hw_n3_system_check_kernel_mode(sys);
	if (ret)
		goto p_err_reset;

	ret = sys->mailbox.ops->start(&sys->mailbox);
	if (ret)
		goto p_err_reset;

	sys->boot_init |= BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
p_err_reset:
	sys->ops->reset(sys);
p_err:
	return ret;
}

static int __dsp_hw_n3_system_wait_reset(struct dsp_system *sys)
{
	int ret;
	long timeout;

	dsp_enter();
	timeout = wait_event_timeout(sys->system_wq,
			sys->system_flag & BIT(DSP_SYSTEM_RESET),
			msecs_to_jiffies(sys->wait[DSP_SYSTEM_WAIT_RESET]));
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to reset DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_RESET]);
		if (sys->debug_mode & BIT(DSP_DEBUG_MODE_RESET_TIMEOUT_PANIC)) {
			dsp_dump_set_value(DSP_N3_DUMP_DEFAULT_VALUE |
					1 << DSP_N3_DUMP_VALUE_SDMA_SS);
			dsp_dump_ctrl();
			BUG();
		} else {
			dsp_dump_ctrl();
		}
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_n3_system_reset(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	if (!(sys->system_flag & BIT(DSP_SYSTEM_BOOT))) {
		dsp_warn("device is already reset(%x)\n", sys->system_flag);
		return 0;
	}

	sys->system_flag = 0x0;
	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->interface.ops->send_irq(&sys->interface, BIT(DSP_TO_CC_INT_RESET));
	ret = __dsp_hw_n3_system_wait_reset(sys);
	if (ret)
		sys->ctrl.ops->force_reset(&sys->ctrl);
	else
		sys->ctrl.ops->reset(&sys->ctrl);
	sys->pm.ops->update_devfreq_min(&sys->pm);

	sys->mailbox.ops->stop(&sys->mailbox);
	sys->debug.ops->log_stop(&sys->debug);
	sys->boot_init &= ~BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_power_active(struct dsp_system *sys)
{
	dsp_check();
	return sys->pm.ops->devfreq_active(&sys->pm);
}

static int dsp_hw_n3_system_set_boot_qos(struct dsp_system *sys, int val)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->set_boot_qos(&sys->pm, val);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_n3_system_runtime_resume(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->enable(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->enable(&sys->clk);
	if (ret)
		goto p_err_clk;

	dsp_leave();
	return 0;
p_err_clk:
	sys->pm.ops->disable(&sys->pm);
p_err_pm:
	return ret;
}

static int dsp_hw_n3_system_runtime_suspend(struct dsp_system *sys)
{
	dsp_enter();
	sys->governor.ops->flush_work(&sys->governor);
	sys->clk.ops->disable(&sys->clk);
	sys->pm.ops->disable(&sys->pm);
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_resume(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_n3_system_recovery(sys);
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_suspend(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_n3_system_flush(sys);
	dsp_leave();
	return 0;
}

static int __dsp_hw_n3_system_npu_boot(struct dsp_system *sys,
		dma_addr_t fw_iova)
{
	int ret;
	unsigned int release;

	dsp_enter();
	if (sys->boot_init & BIT(DSP_SYSTEM_DSP_INIT)) {
		release = dsp_ctrl_readl(DSP_N3_DSPC_CPU_RELEASE);
		dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_NPU_FW_IOVA),
				fw_iova);
		dsp_ctrl_writel(DSP_N3_DSPC_CPU_RELEASE, release | 0x6);
	} else {
		sys->ctrl.ops->common_init(&sys->ctrl);
		dsp_ctrl_sm_writel(DSP_N3_SM_RESERVED(DSP_N3_SM_NPU_FW_IOVA),
				fw_iova);

		ret = __dsp_hw_n3_system_master_copy(sys);
		if (ret)
			goto p_err;

		dsp_ctrl_writel(DSP_N3_DSPC_CPU_RELEASE, 0x6);
	}

	sys->boot_init |= BIT(DSP_SYSTEM_NPU_INIT);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_n3_system_npu_reset(struct dsp_system *sys)
{

	unsigned int release, wfi, wfe;

	dsp_enter();
	release = dsp_ctrl_readl(DSP_N3_DSPC_CPU_RELEASE);
	wfi = dsp_ctrl_readl(DSP_N3_DSPC_CPU_WFI_STATUS);
	wfe = dsp_ctrl_readl(DSP_N3_DSPC_CPU_WFE_STATUS);

	if (!(wfi & 0x2 || wfe & 0x2))
		dsp_warn("NPU CA5 status(%#x/%#x)\n", wfi, wfe);

	if (sys->boot_init & BIT(DSP_SYSTEM_DSP_INIT))
		dsp_ctrl_writel(DSP_N3_DSPC_CPU_RELEASE, release & ~0x2);
	else
		dsp_ctrl_writel(DSP_N3_DSPC_CPU_RELEASE, 0x0);

	sys->boot_init &= ~BIT(DSP_SYSTEM_NPU_INIT);
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_npu_start(struct dsp_system *sys, bool boot,
		dma_addr_t fw_iova)
{
	dsp_check();
	if (boot)
		return __dsp_hw_n3_system_npu_boot(sys, fw_iova);
	else
		return __dsp_hw_n3_system_npu_reset(sys);
}

static int __dsp_hw_n3_system_binary_load(struct dsp_system *sys)
{
	int ret;
	struct dsp_memory *mem;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem = &sys->memory;

	pmem = &mem->priv_mem[DSP_N3_PRIV_MEM_FW];
	ret = dsp_binary_load(DSP_N3_FW_NAME, sys->fw_postfix,
			DSP_N3_FW_EXTENSION, pmem->bac_kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_N3_PRIV_MEM_IVP_PM];
	ret = dsp_binary_load(DSP_N3_IVP_PM_NAME, sys->fw_postfix,
			DSP_N3_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_N3_PRIV_MEM_IVP_DM];
	ret = dsp_binary_load(DSP_N3_IVP_DM_NAME, sys->fw_postfix,
			DSP_N3_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_N3_PRIV_MEM_IAC_PM];
	ret = dsp_binary_load(DSP_N3_IAC_PM_NAME, sys->fw_postfix,
			DSP_N3_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_N3_PRIV_MEM_IAC_DM];
	ret = dsp_binary_load(DSP_N3_IAC_DM_NAME, sys->fw_postfix,
			DSP_N3_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	dsp_leave();
	return 0;
p_err_load:
	return ret;
}

static int dsp_hw_n3_system_start(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = __dsp_hw_n3_system_binary_load(sys);
	if (ret)
		goto p_err_load;

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, false);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	dsp_leave();
	return 0;
p_err_load:
	return ret;
}

static int dsp_hw_n3_system_stop(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_n3_system_open(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->open(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->open(&sys->clk);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->open(&sys->bus);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->open(&sys->llc);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->open(&sys->memory);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->open(&sys->interface);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->open(&sys->ctrl);
	if (ret)
		goto p_err_ctrl;

	ret = sys->mailbox.ops->open(&sys->mailbox);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->open(&sys->governor);
	if (ret)
		goto p_err_governor;

	ret = sys->debug.ops->open(&sys->debug);
	if (ret)
		goto p_err_hw_debug;

	ret = sys->dump.ops->open(&sys->dump);
	if (ret)
		goto p_err_dump;

	dsp_leave();
	return 0;
p_err_dump:
	sys->debug.ops->close(&sys->debug);
p_err_hw_debug:
	sys->governor.ops->close(&sys->governor);
p_err_governor:
	sys->mailbox.ops->close(&sys->mailbox);
p_err_mbox:
	sys->ctrl.ops->close(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->close(&sys->interface);
p_err_interface:
	sys->memory.ops->close(&sys->memory);
p_err_memory:
	sys->llc.ops->close(&sys->llc);
p_err_llc:
	sys->bus.ops->close(&sys->bus);
p_err_bus:
	sys->clk.ops->close(&sys->clk);
p_err_clk:
	sys->pm.ops->close(&sys->pm);
p_err_pm:
	return ret;
}

static int dsp_hw_n3_system_close(struct dsp_system *sys)
{
	dsp_enter();
	sys->dump.ops->close(&sys->dump);
	sys->debug.ops->close(&sys->debug);
	sys->governor.ops->close(&sys->governor);
	sys->mailbox.ops->close(&sys->mailbox);
	sys->ctrl.ops->close(&sys->ctrl);
	sys->interface.ops->close(&sys->interface);
	sys->memory.ops->close(&sys->memory);
	sys->llc.ops->close(&sys->llc);
	sys->bus.ops->close(&sys->bus);
	sys->clk.ops->close(&sys->clk);
	sys->pm.ops->close(&sys->pm);
	dsp_leave();
	return 0;
}

static void __dsp_hw_n3_system_master_load_async(const struct firmware *fw,
		void *context)
{
	int ret, idx, retry = 10;
	struct dsp_system *sys;
	struct dsp_hw_n3_system *sys_sub;
	char full_name[DSP_BINARY_NAME_SIZE];
	size_t size;

	dsp_enter();
	sys = context;
	sys_sub = sys->sub_data;

	snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s.%s",
			DSP_N3_MASTER_FW_NAME, DSP_N3_FW_EXTENSION);

	if (!fw) {
		for (idx = 0; idx < retry; ++idx) {
#if KERNEL_VERSION(4, 18, 0) > LINUX_VERSION_CODE
			ret = request_firmware_direct(&fw, full_name, sys->dev);
#else
			ret = firmware_request_nowarn(&fw, full_name, sys->dev);
#endif
			if (ret >= 0)
				break;
			/* Wait for the file system to be mounted at boot time*/
			msleep(500);
		}
		if (ret < 0) {
			dsp_err("Failed to request binary[%s]\n", full_name);
			return;
		}
	}

	size = sizeof(sys_sub->boot_bin);
	if (fw->size > size) {
		dsp_err("binary(%s) size is over(%zu/%zu)\n",
				full_name, fw->size, size);
		release_firmware(fw);
		return;
	}

	memcpy(sys_sub->boot_bin, fw->data, fw->size);
	sys_sub->boot_bin_size = fw->size;
	release_firmware(fw);
	dsp_info("binary[%s/%zu] is loaded\n", full_name,
			sys_sub->boot_bin_size);
	dsp_leave();
}

static int dsp_hw_n3_system_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_system *sys;
	struct platform_device *pdev;
	struct resource *res;
	void __iomem *regs;
	struct dsp_hw_n3_system *sys_sub;

	dsp_enter();
	sys = &dspdev->system;
	sys->dspdev = dspdev;
	sys->dev = dspdev->dev;
	pdev = to_platform_device(sys->dev);

	sys_sub = kzalloc(sizeof(*sys_sub), GFP_KERNEL);
	if (!sys_sub) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc sys_sub\n");
		goto p_err_sys_sub;
	}
	sys->sub_data = sys_sub;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		dsp_err("Failed to get resource0\n");
		goto p_err_resource0;
	}

	regs = devm_ioremap_resource(sys->dev, res);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap resource0(%d)\n", ret);
		goto p_err_ioremap0;
	}

	sys->sfr_pa = res->start;
	sys->sfr = regs;
	sys->sfr_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		ret = -EINVAL;
		dsp_err("Failed to get resource1\n");
		goto p_err_resource1;
	}

	regs = devm_ioremap_resource(sys->dev, res);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap resource1(%d)\n", ret);
		goto p_err_ioremap1;
	}

	sys_sub->boot_mem = regs;
	sys_sub->boot_mem_size = resource_size(res);

	/*
	 * CHIPID_REV[31:24] Reserved
	 * CHIPID_REV[23:20] Main revision
	 * CHIPID_REV[19:16] Sub revision
	 * CHIPID_REV[15:0]  Reserved
	 */
	regs = devm_ioremap(sys->dev, 0x10000010, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap chip_id(%d)\n", ret);
		goto p_err_ioremap2;
	}

	sys_sub->chip_id = regs;

	init_waitqueue_head(&sys->system_wq);

	sys->wait[DSP_SYSTEM_WAIT_BOOT] = DSP_WAIT_BOOT_TIME;
	sys->wait[DSP_SYSTEM_WAIT_MAILBOX] = DSP_WAIT_MAILBOX_TIME;
	sys->wait[DSP_SYSTEM_WAIT_RESET] = DSP_WAIT_RESET_TIME;
	sys->boost = false;
	mutex_init(&sys->boost_lock);

	sys->layer_start = DSP_SET_DEFAULT_LAYER;
	sys->layer_end = DSP_SET_DEFAULT_LAYER;

	ret = sys->pm.ops->probe(sys);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->probe(sys);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->probe(sys);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->probe(sys);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->probe(sys);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->probe(sys);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->probe(sys);
	if (ret)
		goto p_err_ctrl;

	ret = dsp_task_manager_probe(sys);
	if (ret)
		goto p_err_task;

	ret = sys->mailbox.ops->probe(sys);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->probe(sys);
	if (ret)
		goto p_err_governor;

	ret = sys->debug.ops->probe(dspdev);
	if (ret)
		goto p_err_hw_debug;

	ret = sys->dump.ops->probe(sys);
	if (ret)
		goto p_err_dump;

	ret = dsp_binary_load_async(DSP_N3_MASTER_FW_NAME, NULL,
			DSP_N3_FW_EXTENSION, sys,
			__dsp_hw_n3_system_master_load_async);
	if (ret < 0)
		goto p_err_bin_load;

	dsp_leave();
	return 0;
p_err_bin_load:
	sys->dump.ops->remove(&sys->dump);
p_err_dump:
	sys->debug.ops->remove(&sys->debug);
p_err_hw_debug:
	sys->governor.ops->remove(&sys->governor);
p_err_governor:
	sys->mailbox.ops->remove(&sys->mailbox);
p_err_mbox:
	dsp_task_manager_remove(&sys->task_manager);
p_err_task:
	sys->ctrl.ops->remove(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->remove(&sys->interface);
p_err_interface:
	sys->memory.ops->remove(&sys->memory);
p_err_memory:
	sys->llc.ops->remove(&sys->llc);
p_err_llc:
	sys->bus.ops->remove(&sys->bus);
p_err_bus:
	sys->clk.ops->remove(&sys->clk);
p_err_clk:
	sys->pm.ops->remove(&sys->pm);
p_err_pm:
	devm_iounmap(sys->dev, sys_sub->chip_id);
p_err_ioremap2:
	devm_iounmap(sys->dev, sys_sub->boot_mem);
p_err_ioremap1:
p_err_resource1:
	devm_iounmap(sys->dev, sys->sfr);
p_err_ioremap0:
p_err_resource0:
	kfree(sys->sub_data);
p_err_sys_sub:
	return ret;
}

static void dsp_hw_n3_system_remove(struct dsp_system *sys)
{
	struct dsp_hw_n3_system *sys_sub;

	dsp_enter();
	sys_sub = sys->sub_data;

	sys->dump.ops->remove(&sys->dump);
	sys->debug.ops->remove(&sys->debug);
	sys->governor.ops->remove(&sys->governor);
	sys->mailbox.ops->remove(&sys->mailbox);
	dsp_task_manager_remove(&sys->task_manager);
	sys->ctrl.ops->remove(&sys->ctrl);
	sys->interface.ops->remove(&sys->interface);
	sys->memory.ops->remove(&sys->memory);
	sys->llc.ops->remove(&sys->llc);
	sys->bus.ops->remove(&sys->bus);
	sys->clk.ops->remove(&sys->clk);
	sys->pm.ops->remove(&sys->pm);
	devm_iounmap(sys->dev, sys_sub->chip_id);
	devm_iounmap(sys->dev, sys_sub->boot_mem);
	devm_iounmap(sys->dev, sys->sfr);
	kfree(sys->sub_data);
	dsp_leave();
}

static const struct dsp_system_ops n3_system_ops = {
	.request_control	= dsp_hw_n3_system_request_control,
	.execute_task		= dsp_hw_n3_system_execute_task,
	.iovmm_fault_dump	= dsp_hw_n3_system_iovmm_fault_dump,
	.boot			= dsp_hw_n3_system_boot,
	.reset			= dsp_hw_n3_system_reset,
	.power_active		= dsp_hw_n3_system_power_active,
	.set_boot_qos		= dsp_hw_n3_system_set_boot_qos,
	.runtime_resume		= dsp_hw_n3_system_runtime_resume,
	.runtime_suspend	= dsp_hw_n3_system_runtime_suspend,
	.resume			= dsp_hw_n3_system_resume,
	.suspend		= dsp_hw_n3_system_suspend,

	.npu_start		= dsp_hw_n3_system_npu_start,
	.start			= dsp_hw_n3_system_start,
	.stop			= dsp_hw_n3_system_stop,

	.open			= dsp_hw_n3_system_open,
	.close			= dsp_hw_n3_system_close,
	.probe			= dsp_hw_n3_system_probe,
	.remove			= dsp_hw_n3_system_remove,
};

int dsp_hw_n3_system_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_system_register_ops(DSP_DEVICE_ID_N3, &n3_system_ops);
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_pm_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_clk_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_bus_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_memory_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_interface_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_ctrl_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_mailbox_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_governor_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_debug_init();
	if (ret)
		goto p_err;

	ret = dsp_hw_n3_dump_init();
	if (ret)
		goto p_err;

	dsp_info("hw_n3(%u) was initilized\n", DSP_DEVICE_ID_N3);
	dsp_leave();
	return 0;
p_err:
	return ret;
}
