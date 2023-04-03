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
#include <linux/uaccess.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-binary.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-dump.h"

#define DSP_WAIT_BOOT_TIME	(100)
#define DSP_WAIT_MAILBOX_TIME	(1500)
#define DSP_WAIT_RESET_TIME	(100)

#define DSP_STATIC_KERNEL	(1)
#define DSP_DYNAMIC_KERNEL	(2)

#define DSP_SET_DEFAULT_LAYER	(0xffffffff)

int dsp_system_request_control(struct dsp_system *sys, unsigned int id,
		union dsp_control *cmd)
{
	int ret;
	char boost_mo[DSP_BUS_SCENARIO_NAME_LEN] = "max";

	dsp_enter();
	switch (id) {
	case DSP_CONTROL_ENABLE_DVFS:
		ret = dsp_pm_dvfs_enable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_DISABLE_DVFS:
		ret = dsp_pm_dvfs_disable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_ENABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (!sys->boost) {
			ret = dsp_pm_boost_enable(&sys->pm);
			if (ret) {
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			ret = dsp_bus_mo_get(&sys->bus, boost_mo);
			if (ret) {
				dsp_pm_boost_disable(&sys->pm);
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
			dsp_bus_mo_put(&sys->bus, boost_mo);
			dsp_pm_boost_disable(&sys->pm);
			sys->boost = false;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_REQUEST_MO:
		ret = dsp_bus_mo_get(&sys->bus, cmd->mo.scenario_name);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_RELEASE_MO:
		dsp_bus_mo_put(&sys->bus, cmd->mo.scenario_name);
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

static int __dsp_system_wait_task(struct dsp_system *sys, struct dsp_task *task)
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
		dsp_dump_ctrl();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_system_flush(struct dsp_system *sys)
{
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	spin_lock_irqsave(&tmgr->slock, flags);
	dsp_task_manager_set_block_mode(tmgr, true);
	dsp_task_manager_flush_process(tmgr, -ENOSTR);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	dsp_system_reset(sys);
	dsp_leave();
}

static void __dsp_system_recovery(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = dsp_system_boot(sys);
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

int dsp_system_execute_task(struct dsp_system *sys, struct dsp_task *task)
{
	int ret;

	dsp_enter();

	dsp_pm_update_devfreq_busy(&sys->pm, task->pool->pm_qos);
	ret = dsp_mailbox_send_task(&sys->mailbox, task);
	if (ret)
		goto p_err;

	dsp_dump_mailbox_pool_debug(task->pool);

	/* TODO Devfreq change criteria required if not waiting */
	if (task->wait) {
		ret = __dsp_system_wait_task(sys, task);
		if (ret) {
			if (task->recovery) {
				__dsp_system_flush(sys);
				__dsp_system_recovery(sys);
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

	dsp_pm_update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->normal_count++;
	dsp_leave();
	return 0;
p_err:
	dsp_pm_update_devfreq_idle(&sys->pm, task->pool->pm_qos);
	task->owner->error_count++;
	dsp_dump_mailbox_pool_error(task->pool);
	dsp_dump_task_manager_count(task->owner);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	return ret;
}

void dsp_system_iovmm_fault_dump(struct dsp_system *sys)
{
	dsp_enter();
	dsp_dump_ctrl();
	dsp_dump_task_manager_count(&sys->task_manager);
	dsp_dump_kernel(&sys->dspdev->core.graph_manager.kernel_manager);
	dsp_leave();
}

static int __dsp_system_master_copy(struct dsp_system *sys)
{
	int ret;
	void __iomem *dst;
	unsigned char *src;
	size_t size;
	unsigned int remain = 0;

	dsp_enter();
	if (!sys->boot_bin_size) {
		dsp_warn("master bin must not be zero(%zu)\n",
				sys->boot_bin_size);
		ret = dsp_binary_load(DSP_MASTER_FW_NAME, NULL,
				DSP_FW_EXTENSION, sys->boot_bin,
				sizeof(sys->boot_bin), &sys->boot_bin_size);
		if (ret)
			goto p_err;
	}
	dst = sys->boot_mem;
	src = sys->boot_bin;
	size = sys->boot_bin_size;

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

static void __dsp_system_init(struct dsp_system *sys)
{
	struct dsp_priv_mem *pmem;
	unsigned int chip_id;

	dsp_enter();
	pmem = sys->memory.priv_mem;

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_CC_INT_STATUS), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_HOST_INT_STATUS), 0x0);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(DEBUG_LAYER_START),
			sys->layer_start);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(DEBUG_LAYER_END), sys->layer_end);

	memcpy(pmem[DSP_PRIV_MEM_FW].kvaddr, pmem[DSP_PRIV_MEM_FW].bac_kvaddr,
			pmem[DSP_PRIV_MEM_FW].used_size);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(FW_RESERVED_SIZE),
			pmem[DSP_PRIV_MEM_FW].size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IVP_PM_IOVA),
			pmem[DSP_PRIV_MEM_IVP_PM].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IVP_PM_SIZE),
			pmem[DSP_PRIV_MEM_IVP_PM].used_size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IVP_DM_IOVA),
			pmem[DSP_PRIV_MEM_IVP_DM].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IVP_DM_SIZE),
			pmem[DSP_PRIV_MEM_IVP_DM].used_size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IAC_PM_IOVA),
			pmem[DSP_PRIV_MEM_IAC_PM].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IAC_PM_SIZE),
			pmem[DSP_PRIV_MEM_IAC_PM].used_size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IAC_DM_IOVA),
			pmem[DSP_PRIV_MEM_IAC_DM].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(IAC_DM_SIZE),
			pmem[DSP_PRIV_MEM_IAC_DM].used_size);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(MAILBOX_VERSION), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(MESSAGE_VERSION), 0x0);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(FW_LOG_MEMORY_IOVA),
			pmem[DSP_PRIV_MEM_FW_LOG].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(FW_LOG_MEMORY_SIZE),
			pmem[DSP_PRIV_MEM_FW_LOG].size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_CC_MBOX_MEMORY_IOVA),
			pmem[DSP_PRIV_MEM_MBOX_MEMORY].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_CC_MBOX_MEMORY_SIZE),
			pmem[DSP_PRIV_MEM_MBOX_MEMORY].size);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_CC_MBOX_POOL_IOVA),
			pmem[DSP_PRIV_MEM_MBOX_POOL].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_CC_MBOX_POOL_SIZE),
			pmem[DSP_PRIV_MEM_MBOX_POOL].size);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(KERNEL_MODE), DSP_DYNAMIC_KERNEL);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(DL_OUT_IOVA),
			pmem[DSP_PRIV_MEM_DL_OUT].iova);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(DL_OUT_SIZE),
			pmem[DSP_PRIV_MEM_DL_OUT].size);

	chip_id = readl(sys->chip_id);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(CHIPID_REV), chip_id);
	dsp_info("CHIPID : %#x\n", chip_id);

	dsp_ctrl_sm_writel(DSP_SM_RESERVED(PRODUCT_ID), 0xE990);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FDC), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FE0), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FE4), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FE8), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FEC), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FF0), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FF4), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FF8), 0x0);
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TEMP_1FFC), 0x0);
	dsp_leave();
}

static int __dsp_system_wait_boot(struct dsp_system *sys)
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
		dsp_dump_ctrl();
		goto p_err;
	} else {
		dsp_info("Completed to boot DSP\n");
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_system_check_kernel_mode(struct dsp_system *sys)
{
	int kernel_mode;

	dsp_enter();
	kernel_mode = dsp_ctrl_sm_readl(DSP_SM_RESERVED(KERNEL_MODE));
	if (kernel_mode != DSP_DYNAMIC_KERNEL) {
		dsp_err("static kernel is no longer available\n");
		return -EINVAL;
	}

	dsp_leave();
	return 0;
}

int dsp_system_boot(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	sys->system_flag = 0x0;

	/* dsp2 gating */
	writel(0xffffff00, sys->dsp2_gating);

	if (sys->boot_init & BIT(DSP_SYSTEM_NPU_INIT)) {
		dsp_ctrl_init(&sys->ctrl);
	} else {
		dsp_ctrl_all_init(&sys->ctrl);
		ret = __dsp_system_master_copy(sys);
		if (ret)
			goto p_err;
	}

	__dsp_system_init(sys);
	dsp_hw_debug_log_start(&sys->debug);

	dsp_pm_update_devfreq_boot(&sys->pm);
	dsp_ctrl_start(&sys->ctrl);
	ret = __dsp_system_wait_boot(sys);
	dsp_pm_update_devfreq_min(&sys->pm);
	if (ret)
		goto p_err_reset;

	ret = __dsp_system_check_kernel_mode(sys);
	if (ret)
		goto p_err_reset;

	ret = dsp_mailbox_start(&sys->mailbox);
	if (ret)
		goto p_err_reset;

	sys->boot_init |= BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
p_err_reset:
	dsp_system_reset(sys);
p_err:
	return ret;
}

static int __dsp_system_wait_reset(struct dsp_system *sys)
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
		dsp_dump_ctrl();
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_system_reset(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	if (!(sys->system_flag & BIT(DSP_SYSTEM_BOOT))) {
		dsp_warn("device is already reset(%x)\n", sys->system_flag);
		return 0;
	}

	sys->system_flag = 0x0;
	dsp_pm_update_devfreq_boot(&sys->pm);
	dsp_interface_interrupt(&sys->interface, BIT(DSP_TO_CC_INT_RESET));
	ret = __dsp_system_wait_reset(sys);
	if (ret)
		dsp_ctrl_force_reset(&sys->ctrl);
	else
		dsp_ctrl_reset(&sys->ctrl);
	dsp_pm_update_devfreq_min(&sys->pm);

	dsp_mailbox_stop(&sys->mailbox);
	dsp_hw_debug_log_stop(&sys->debug);
	sys->boot_init &= ~BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
}

int dsp_system_power_active(struct dsp_system *sys)
{
	dsp_check();
	return dsp_pm_devfreq_active(&sys->pm);
}

int dsp_system_set_boot_qos(struct dsp_system *sys, int val)
{
	int ret;

	dsp_enter();
	ret = dsp_pm_set_boot_qos(&sys->pm, val);
	if (ret)
		goto p_err;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_system_runtime_resume(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = dsp_pm_enable(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = dsp_clk_enable(&sys->clk);
	if (ret)
		goto p_err_clk;

	dsp_leave();
	return 0;
p_err_clk:
	dsp_pm_disable(&sys->pm);
p_err_pm:
	return ret;
}

int dsp_system_runtime_suspend(struct dsp_system *sys)
{
	dsp_enter();
	dsp_clk_disable(&sys->clk);
	dsp_pm_disable(&sys->pm);
	dsp_leave();
	return 0;
}

int dsp_system_resume(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_system_recovery(sys);
	dsp_leave();
	return 0;
}

int dsp_system_suspend(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_system_flush(sys);
	dsp_leave();
	return 0;
}

static int __dsp_system_npu_boot(struct dsp_system *sys, dma_addr_t fw_iova)
{
	int ret;
	unsigned int release;

	dsp_enter();
	if (sys->boot_init & BIT(DSP_SYSTEM_DSP_INIT)) {
		release = dsp_ctrl_readl(DSPC_CPU_RELEASE);
		dsp_ctrl_sm_writel(DSP_SM_RESERVED(NPU_FW_IOVA), fw_iova);
		dsp_ctrl_writel(DSPC_CPU_RELEASE, release | 0x6);
	} else {
		dsp_ctrl_common_init(&sys->ctrl);
		dsp_ctrl_sm_writel(DSP_SM_RESERVED(NPU_FW_IOVA), fw_iova);

		ret = __dsp_system_master_copy(sys);
		if (ret)
			goto p_err;

		dsp_ctrl_writel(DSPC_CPU_RELEASE, 0x6);
	}

	sys->boot_init |= BIT(DSP_SYSTEM_NPU_INIT);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_system_npu_reset(struct dsp_system *sys)
{

	unsigned int release, wfi, wfe;

	dsp_enter();
	release = dsp_ctrl_readl(DSPC_CPU_RELEASE);
	wfi = dsp_ctrl_readl(DSPC_CPU_WFI_STATUS);
	wfe = dsp_ctrl_readl(DSPC_CPU_WFE_STATUS);

	if (!(wfi & 0x2 || wfe & 0x2))
		dsp_warn("NPU CA5 status(%#x/%#x)\n", wfi, wfe);

	if (sys->boot_init & BIT(DSP_SYSTEM_DSP_INIT))
		dsp_ctrl_writel(DSPC_CPU_RELEASE, release & ~0x2);
	else
		dsp_ctrl_writel(DSPC_CPU_RELEASE, 0x0);

	sys->boot_init &= ~BIT(DSP_SYSTEM_NPU_INIT);
	dsp_leave();
	return 0;
}

int dsp_system_npu_start(struct dsp_system *sys, bool boot, dma_addr_t fw_iova)
{
	dsp_check();
	if (boot)
		return __dsp_system_npu_boot(sys, fw_iova);
	else
		return __dsp_system_npu_reset(sys);
}

static int __dsp_system_binary_load(struct dsp_system *sys)
{
	int ret;
	struct dsp_memory *mem;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem = &sys->memory;

	pmem = &mem->priv_mem[DSP_PRIV_MEM_FW];
	ret = dsp_binary_load(DSP_FW_NAME, sys->fw_postfix, DSP_FW_EXTENSION,
			pmem->bac_kvaddr, pmem->size, &pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_PRIV_MEM_IVP_PM];
	ret = dsp_binary_load(DSP_IVP_PM_NAME, sys->fw_postfix,
			DSP_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_PRIV_MEM_IVP_DM];
	ret = dsp_binary_load(DSP_IVP_DM_NAME, sys->fw_postfix,
			DSP_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_PRIV_MEM_IAC_PM];
	ret = dsp_binary_load(DSP_IAC_PM_NAME, sys->fw_postfix,
			DSP_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_PRIV_MEM_IAC_DM];
	ret = dsp_binary_load(DSP_IAC_DM_NAME, sys->fw_postfix,
			DSP_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	dsp_leave();
	return 0;
p_err_load:
	return ret;
}

int dsp_system_start(struct dsp_system *sys)
{
	int ret;
	struct dsp_task_manager *tmgr;
	unsigned long flags;

	dsp_enter();
	tmgr = &sys->task_manager;

	ret = __dsp_system_binary_load(sys);
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

int dsp_system_stop(struct dsp_system *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_system_open(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = dsp_pm_open(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = dsp_clk_open(&sys->clk);
	if (ret)
		goto p_err_clk;

	ret = dsp_bus_open(&sys->bus);
	if (ret)
		goto p_err_bus;

	ret = dsp_memory_open(&sys->memory);
	if (ret)
		goto p_err_memory;

	ret = dsp_interface_open(&sys->interface);
	if (ret)
		goto p_err_interface;

	ret = dsp_ctrl_open(&sys->ctrl);
	if (ret)
		goto p_err_ctrl;

	ret = dsp_mailbox_open(&sys->mailbox);
	if (ret)
		goto p_err_mbox;

	ret = dsp_hw_debug_open(&sys->debug);
	if (ret)
		goto p_err_hw_debug;

	dsp_leave();
	return 0;
p_err_hw_debug:
	dsp_mailbox_close(&sys->mailbox);
p_err_mbox:
	dsp_ctrl_close(&sys->ctrl);
p_err_ctrl:
	dsp_interface_close(&sys->interface);
p_err_interface:
	dsp_memory_close(&sys->memory);
p_err_memory:
	dsp_bus_close(&sys->bus);
p_err_bus:
	dsp_clk_close(&sys->clk);
p_err_clk:
	dsp_pm_close(&sys->pm);
p_err_pm:
	return ret;
}

int dsp_system_close(struct dsp_system *sys)
{
	dsp_enter();
	dsp_hw_debug_close(&sys->debug);
	dsp_mailbox_close(&sys->mailbox);
	dsp_ctrl_close(&sys->ctrl);
	dsp_interface_close(&sys->interface);
	dsp_memory_close(&sys->memory);
	dsp_bus_close(&sys->bus);
	dsp_clk_close(&sys->clk);
	dsp_pm_close(&sys->pm);
	dsp_leave();
	return 0;
}

static void __dsp_system_master_load_async(const struct firmware *fw,
		void *context)
{
	int ret, idx, retry = 10;
	struct dsp_system *sys;
	char full_name[DSP_BINARY_NAME_SIZE];
	size_t size;

	dsp_enter();
	sys = context;
	snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s.%s", DSP_MASTER_FW_NAME,
			DSP_FW_EXTENSION);

	if (!fw) {
		for (idx = 0; idx < retry; ++idx) {
			ret = firmware_request_nowarn(&fw, full_name, sys->dev);
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

	size = sizeof(sys->boot_bin);
	if (fw->size > size) {
		dsp_err("binary(%s) size is over(%zu/%zu)\n",
				full_name, fw->size, size);
		release_firmware(fw);
		return;
	}

	memcpy(sys->boot_bin, fw->data, fw->size);
	sys->boot_bin_size = fw->size;
	release_firmware(fw);
	dsp_info("binary[%s/%zu] is loaded\n", full_name, sys->boot_bin_size);
	dsp_leave();
}

int dsp_system_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_system *sys;
	struct platform_device *pdev;
	struct resource *res;
	void __iomem *regs;

	dsp_enter();
	sys = &dspdev->system;
	sys->dspdev = dspdev;
	sys->dev = dspdev->dev;
	pdev = to_platform_device(sys->dev);

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

	sys->boot_mem = regs;
	sys->boot_mem_size = resource_size(res);

	/* it requires for dsp2, otherwise dsp2 can not be accessed */
	regs = devm_ioremap(sys->dev, 0x19a20104, 4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap dsp2_gating(%d)\n", ret);
		goto p_err_gating;
	}
	sys->dsp2_gating = regs;

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
		goto p_err_chipid;
	}

	sys->chip_id = regs;

	init_waitqueue_head(&sys->system_wq);

	sys->wait[DSP_SYSTEM_WAIT_BOOT] = DSP_WAIT_BOOT_TIME;
	sys->wait[DSP_SYSTEM_WAIT_MAILBOX] = DSP_WAIT_MAILBOX_TIME;
	sys->wait[DSP_SYSTEM_WAIT_RESET] = DSP_WAIT_RESET_TIME;
	sys->boost = false;
	mutex_init(&sys->boost_lock);

	sys->layer_start = DSP_SET_DEFAULT_LAYER;
	sys->layer_end = DSP_SET_DEFAULT_LAYER;

	dsp_dump_set_value(DSP_DUMP_DEFAULT_VALUE);

	ret = dsp_pm_probe(sys);
	if (ret)
		goto p_err_pm;

	ret = dsp_clk_probe(sys);
	if (ret)
		goto p_err_clk;

	ret = dsp_bus_probe(sys);
	if (ret)
		goto p_err_bus;

	ret = dsp_memory_probe(sys);
	if (ret)
		goto p_err_memory;

	ret = dsp_interface_probe(sys);
	if (ret)
		goto p_err_interface;

	ret = dsp_ctrl_probe(sys);
	if (ret)
		goto p_err_ctrl;

	ret = dsp_task_manager_probe(sys);
	if (ret)
		goto p_err_task;

	ret = dsp_mailbox_probe(sys);
	if (ret)
		goto p_err_mbox;

	ret = dsp_hw_debug_probe(dspdev);
	if (ret)
		goto p_err_hw_debug;

	ret = dsp_binary_load_async(DSP_MASTER_FW_NAME, NULL, DSP_FW_EXTENSION,
			sys, __dsp_system_master_load_async);
	if (ret < 0)
		goto p_err_bin_load;

	dsp_leave();
	return 0;
p_err_bin_load:
	dsp_hw_debug_remove(&sys->debug);
p_err_hw_debug:
	dsp_mailbox_remove(&sys->mailbox);
p_err_mbox:
	dsp_task_manager_remove(&sys->task_manager);
p_err_task:
	dsp_ctrl_remove(&sys->ctrl);
p_err_ctrl:
	dsp_interface_remove(&sys->interface);
p_err_interface:
	dsp_memory_remove(&sys->memory);
p_err_memory:
	dsp_bus_remove(&sys->bus);
p_err_bus:
	dsp_clk_remove(&sys->clk);
p_err_clk:
	dsp_pm_remove(&sys->pm);
p_err_pm:
	devm_iounmap(sys->dev, sys->chip_id);
p_err_chipid:
	devm_iounmap(sys->dev, sys->dsp2_gating);
p_err_gating:
	devm_iounmap(sys->dev, sys->boot_mem);
p_err_ioremap1:
p_err_resource1:
	devm_iounmap(sys->dev, sys->sfr);
p_err_ioremap0:
p_err_resource0:
	return ret;
}

void dsp_system_remove(struct dsp_system *sys)
{
	dsp_enter();
	dsp_hw_debug_remove(&sys->debug);
	dsp_mailbox_remove(&sys->mailbox);
	dsp_task_manager_remove(&sys->task_manager);
	dsp_ctrl_remove(&sys->ctrl);
	dsp_interface_remove(&sys->interface);
	dsp_memory_remove(&sys->memory);
	dsp_bus_remove(&sys->bus);
	dsp_clk_remove(&sys->clk);
	dsp_pm_remove(&sys->pm);
	devm_iounmap(sys->dev, sys->chip_id);
	devm_iounmap(sys->dev, sys->dsp2_gating);
	devm_iounmap(sys->dev, sys->boot_mem);
	devm_iounmap(sys->dev, sys->sfr);
	dsp_leave();
}
