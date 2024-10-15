/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/pm_runtime.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "include/npu-common.h"
#include "npu-device.h"
#include "npu-session.h"
#include "npu-vertex.h"
#include "npu-hw-device.h"
#include "npu-log.h"
#include "npu-util-regs.h"
#include "npu-pm.h"
#include "npu-afm.h"
#include "dsp-dhcp.h"
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif

static unsigned int suspend_resume;

static struct npu_device *npu_pm_dev;

extern u32 g_hwdev_num;
extern struct npu_proto_drv *protodr;
extern struct npu_precision *npu_precision;

void npu_pm_wake_lock(struct npu_session *session)
{
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* prevent the system to suspend */
	if (!npu_wake_lock_active(system->ws)) {
		npu_wake_lock(system->ws);
		npu_dbg("wake_lock, now(%d)\n", npu_wake_lock_active(system->ws));
	}
#endif
}

void npu_pm_wake_unlock(struct npu_session *session)
{
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;

#if IS_ENABLED(CONFIG_PM_SLEEP)
	if (npu_wake_lock_active(system->ws)) {
		npu_wake_unlock(system->ws);
		npu_dbg("wake_unlock, now(%d)\n", npu_wake_lock_active(system->ws));
	}
#endif
}

static bool npu_pm_check_power_on(struct npu_system *system, const char *domain)
{
	int hi;
	struct npu_hw_device *hdev;

	for (hi = 0; hi < g_hwdev_num; hi++) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			if (!strcmp(hdev->name, domain)) {
				return true;
			}
		}
	}

	return false;
}

static int npu_pm_suspend_setting_sfr(struct npu_system *system)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_NPU_AFM)
	npu_afm_control_global(system, HTU_DNC, NPU_AFM_DISABLE);
	npu_afm_onoff_dnc_interrupt(NPU_AFM_DISABLE);
	npu_afm_clear_dnc_interrupt();
#endif
	if (npu_pm_check_power_on(system, "NPU")) {
#if IS_ENABLED(CONFIG_NPU_AFM)
		npu_afm_control_global(system, HTU_GNPU1, NPU_AFM_DISABLE);
		npu_afm_onoff_gnpu1_interrupt(NPU_AFM_DISABLE);
		npu_afm_clear_gnpu1_interrupt();

		__npu_afm_work("NPU0", NPU_AFM_FREQ_LEVEL[0]);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
		__npu_afm_work("NPU1", NPU_AFM_FREQ_LEVEL[0]);
#endif
#endif
	}

	if (exynos_soc_info.main_rev == 1) {
		if (exynos_soc_info.sub_rev < 2) {
			ret = npu_cmd_map(system, "hwacgenlh");
		} else { // exynos_sec_info.sub_rev >= 2
			ret = npu_cmd_map(system, "hwacgenlh2");
		}
	}

	if (ret) {
		npu_err("fail(%d) in npu_hwacgenlh\n", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_pm_resume_setting_sfr(struct npu_system *system)
{
	int ret = 0;

	if (exynos_soc_info.main_rev == 1) {
		if (exynos_soc_info.sub_rev < 2) {
			ret = npu_cmd_map(system, "hwacgdislh");
		} else { // exynos_sec_info.sub_rev >= 2
			ret = npu_cmd_map(system, "hwacgdislh2");
		}
	}

	if (ret) {
		npu_err("fail(%d) in npu_hwacgdislh\n", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_AFM)
	npu_afm_control_global(system, HTU_DNC, NPU_AFM_ENABLE);
	npu_afm_onoff_dnc_interrupt(NPU_AFM_ENABLE);
#endif
	if (npu_pm_check_power_on(system, "NPU")) {
#if IS_ENABLED(CONFIG_NPU_AFM)
		npu_afm_control_global(system, HTU_GNPU1, NPU_AFM_ENABLE);
		npu_afm_onoff_gnpu1_interrupt(NPU_AFM_ENABLE);
#endif
	}

#if IS_ENABLED(CONFIG_NPU_AFM)
	{
		int i = 0;
		for (i = 0; i < BUCK_CNT; i++) {
			system->afm_max_lock_level[i] = 0;
			atomic_set(&(system->ocp_warn_status[i]), NPU_AFM_OCP_NONE);
			atomic_set(&(system->restore_status[i]), NPU_AFM_OCP_NONE);
		}
	}
#endif

p_err:
	return ret;
}

static const char *npu_check_fw_arch(struct npu_system *system,
				struct npu_memory_buffer *fwmem)
{
	if (!strncmp(FW_64_SYMBOL, fwmem->vaddr + FW_SYMBOL_OFFSET, FW_64_SYMBOL_S)) {
		npu_info("FW is 64 bit, cmd map %s\n", FW_64_BIT);
		return FW_64_BIT;
	}

	npu_info("FW is 32 bit, cmd map : %s\n", FW_32_BIT);
	return FW_32_BIT;
}

static int npu_pm_check_power_and_on(struct npu_system *system)
{
	int hi;
	int ret = 0;
	struct npu_hw_device *hdev;

	for (hi = 0; hi < g_hwdev_num; hi++) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			npu_info("find %s, and try power on\n", hdev->name);
			ret = pm_runtime_get_sync(hdev->dev);
			if (ret && (ret != 1)) {
				npu_err("fail in runtime resume(%d)\n", ret);
				goto err_exit;
			}
		}
	}
	return 0;
err_exit:
	return ret;
}

static int npu_pm_check_power_and_off(struct npu_system *system)
{
	int hi;
	int ret = 0;
	struct npu_hw_device *hdev;

	for (hi = g_hwdev_num - 1; hi >= 0; hi--) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			npu_info("find %s, and try power off\n", hdev->name);
			ret = pm_runtime_put_sync(hdev->dev);
			if (ret) {
				npu_err("fail in runtime suspend(%d)\n", ret);
				goto err_exit;
			}
		}
	}
err_exit:
	return ret;
}

static void npu_pm_update_dhcp_power_status(struct npu_system *system, struct npu_session *session, bool on)
{
	int hi;
	struct npu_hw_device *hdev;

	if (on) {
		for (hi = 0; hi < g_hwdev_num; hi++) {
			hdev = system->hwdev_list[hi];
			if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
				npu_info("find %s, and update dhcp for power on\n", hdev->name);
				if (!strcmp(hdev->name, "NPU") || !strcmp(hdev->name, "DSP")) {
					dsp_dhcp_update_pwr_status(npu_pm_dev, hdev->id, true);
					if (!strcmp(hdev->name, "NPU")) {
						session->hids = NPU_HWDEV_ID_NPU;
					} else { // DSP
						session->hids = NPU_HWDEV_ID_DSP;
					}
					session->nw_result.result_code = NPU_NW_JUST_STARTED;
					npu_session_put_nw_req(session, NPU_NW_CMD_POWER_CTL);
					wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
				}
			}
		}
	} else {
		for (hi = g_hwdev_num - 1; hi >= 0; hi--) {
			hdev = system->hwdev_list[hi];
			if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
				npu_info("find %s, and update dhcp for power off\n", hdev->name);
				if (!strcmp(hdev->name, "NPU") || !strcmp(hdev->name, "DSP")) {
					dsp_dhcp_update_pwr_status(npu_pm_dev, hdev->id, false);
					if (!strcmp(hdev->name, "NPU")) {
						session->hids = NPU_HWDEV_ID_NPU;
					} else { // DSP
						session->hids = NPU_HWDEV_ID_DSP;
					}
					session->nw_result.result_code = NPU_NW_JUST_STARTED;
					npu_session_put_nw_req(session, NPU_NW_CMD_SUSPEND);
					wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
				}
			}
		}
	}
}

int npu_pm_suspend(struct device *dev)
{
	int ret = 0;
	int wptr, rptr;
	int i, irq_num;
	u32 pwr_value;
	struct npu_system *system = &npu_pm_dev->system;
	struct dsp_dhcp *dhcp = system->dhcp;
	struct npu_session *session;
	volatile struct npu_interface interface;
	struct npu_scheduler_info *info = npu_pm_dev->sched;

	pwr_value = dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_PWR_CTRL);
	npu_dbg("start(0x%x)\n", pwr_value);
	if (pwr_value) {
		session = npu_pm_dev->first_session;
		interface = *(system->interface);
		cancel_delayed_work_sync(&info->sched_work);
#if IS_ENABLED(CONFIG_NPU_AFM)
		cancel_delayed_work_sync(&system->afm_dnc_work);
		cancel_delayed_work_sync(&system->afm_gnpu1_work);
		cancel_delayed_work_sync(&system->afm_restore_dnc_work);
		cancel_delayed_work_sync(&system->afm_restore_gnpu1_work);
#endif
		cancel_delayed_work_sync(&npu_pm_dev->npu_log_work);
		cancel_delayed_work_sync(&system->precision_work);

		npu_pm_update_dhcp_power_status(system, session, false);

		/* if do net working well with only this func.
		   we need to change using proto_drv_open / proto_drv_close.
		*/
		auto_sleep_thread_terminate(&protodr->ast);

		fw_will_note(FW_LOGSIZE);
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		npu_cmdq_table_read_close(&npu_pm_dev->cmdq_table_info);
#endif
		if (likely(interface.mbox_hdr))
			mailbox_deinit(interface.mbox_hdr, system);

		irq_num = MAILBOX_IRQ_CNT;

		for (i = 0; i < irq_num; i++)
			irq_set_affinity_hint(system->irq[i], NULL);

		for (i = 0; i < irq_num; i++)
			devm_free_irq(dev, system->irq[i], NULL);

		if (list_empty(&system->work_report.entry))
			queue_work(system->wq, &system->work_report);
		if ((system->wq) && (interface.mbox_hdr)) {
			if (work_pending(&system->work_report)) {
				cancel_work_sync(&system->work_report);
				wptr = interface.mbox_hdr->f2hctrl[1].wptr;
				rptr = interface.mbox_hdr->f2hctrl[1].rptr;
				npu_dbg("work was canceled due to interface close, rptr/wptr : %d/%d\n", wptr, rptr);
			}
			flush_workqueue(system->wq);
		}

		npu_pm_suspend_setting_sfr(system);

		ret = npu_cmd_map(system, "cpuoff");
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for cpu_off\n", ret);
			goto err_exit;
		}

		if (suspend_resume) {
			ret = npu_pm_check_power_and_off(system);
			if (ret) {
				npu_err("fail(%d) in try power on\n", ret);
				goto err_exit;
			}
		}

		system->enter_suspend = 0xCAFE;
	}

err_exit:
	npu_dbg("end(%d)\n", ret);
	return ret;
}

int npu_pm_resume(struct device *dev)
{
	int i, irq_num;
	int ret = 0;
	struct npu_system *system = &npu_pm_dev->system;
	struct npu_memory_buffer *fwmem;
	struct npu_session *session;
	volatile struct npu_interface interface;
	struct npu_scheduler_info *info = npu_pm_dev->sched;

	npu_dbg("start(0x%x)\n", system->enter_suspend);
	if (system->enter_suspend == 0xCAFE) {
		interface = *(system->interface);
		session = npu_pm_dev->first_session;

		fwmem = npu_get_mem_area(system, "fwmem");

		print_ufw_signature(fwmem);

		if (suspend_resume) {
			ret = npu_pm_check_power_and_on(system);
			if (ret) {
				npu_err("fail(%d) in try power on\n", ret);
				goto err_exit;
			}
		}

		ret = npu_cmd_map(system, npu_check_fw_arch(system, fwmem));
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for cpu on\n", ret);
			goto err_exit;
		}

		ret = npu_interface_irq_set(dev, system);
		if (ret) {
			npu_err("error(%d) in npu_interface_irq_set\n", ret);
			goto err_probe_irq;
		}

		ret = mailbox_init(interface.mbox_hdr, system);
		if (ret) {
			npu_err("error(%d) in npu_mailbox_init\n", ret);
			goto err_exit;
		}

		ret = npu_pm_resume_setting_sfr(system);
		if (ret) {
			npu_err("error(%d) in npu_pm_resume_setting_sfr\n", ret);
			goto err_exit;
		}

		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(0));
		queue_delayed_work(npu_pm_dev->npu_log_wq,
				&npu_pm_dev->npu_log_work,
				msecs_to_jiffies(1000));

		/* if do net working well with only this func.
		   we need to change using proto_drv_open / proto_drv_close.
		*/
		ret = auto_sleep_thread_start(&protodr->ast, protodr->ast_param);
		if (unlikely(ret)) {
			npu_err("fail(%d) in AST start\n", ret);
			goto err_exit;
		}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		ret = start_cmdq_table_read(&npu_pm_dev->cmdq_table_info);
		if (ret) {
			npu_err("start_cmdq_table_read is fail(%d)\n", ret);
			goto err_exit;
		}
#endif
		npu_pm_update_dhcp_power_status(system, session, true);

		system->enter_suspend = 0;

		return ret;
err_probe_irq:
		irq_num = MAILBOX_IRQ_CNT;
		for (i = 0; i < irq_num; i++) {
			irq_set_affinity_hint(system->irq[i], NULL);
			devm_free_irq(dev, system->irq[i], NULL);
		}
	}

err_exit:
	npu_dbg("end(%d)\n", ret);
	return ret;
}

static ssize_t suspend_resume_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "suspend_resume_test : 0x%x\n", suspend_resume);

	return ret;
}

static ssize_t suspend_resume_test_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 4) {
			npu_err("Invalid npu_pm_suspend_resume setting : 0x%x, please input 0 ~ 2\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	suspend_resume = x;

	if (suspend_resume == 1) {
		ret = npu_pm_suspend(npu_pm_dev->dev);
	} else if (suspend_resume == 2) {
		ret = npu_pm_resume(npu_pm_dev->dev);
	} else if (suspend_resume == 3) {
		ret = npu_pm_suspend(npu_pm_dev->dev);
		ret = npu_pm_resume(npu_pm_dev->dev);
	}

	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(suspend_resume_test);

int npu_pm_probe(struct npu_device *device)
{
	int ret = 0;

	suspend_resume = 0;

	npu_pm_dev = device;

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_suspend_resume_test.attr);
	if (ret) {
		probe_err("sysfs_create_file error : ret = %d\n", ret);
		goto err_exit;
	}

err_exit:
	return ret;
}
