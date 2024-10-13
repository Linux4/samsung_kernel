/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-afm.h"
#include "npu-hw-device.h"
#include "npu-scheduler.h"

static bool afm_local_onoff;

static struct npu_system *npu_afm_system;

static void __npu_afm_work(int freq);

extern int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub_pmic_get_i2c(struct i2c_client **i2c);

static void npu_afm_control_grobal(struct npu_system *system,
					int location, int enable)
{
	if (location == HTU_DNC) {
		if (enable)
			npu_cmd_map(system, "afmdncen");
		else
			npu_cmd_map(system, "afmdncdis");
	} else { /* location == HTU_GNPU1 */
		if (enable)
			npu_cmd_map(system, "afmgnpu1en");
		else
			npu_cmd_map(system, "afmgnpu1dis");
	}
}

static void npu_afm_clear_dnc_interrupt(void)
{
	npu_cmd_map(npu_afm_system, "clrdncitr");
}

static void npu_afm_clear_dnc_tdc(void)
{
	npu_cmd_map(npu_afm_system, "clrdnctdc");
}

static void npu_afm_clear_gnpu1_interrupt(void)
{
	npu_cmd_map(npu_afm_system, "clrgnpu1itr");
}

static void npu_afm_clear_gnpu1_tdc(void)
{
	npu_cmd_map(npu_afm_system, "clrgnpu1tdc");
}


static irqreturn_t npu_afm_isr0(int irq, void *data)
{
	/* TODO : implement. */

	npu_afm_clear_dnc_interrupt();
	npu_afm_clear_dnc_tdc();

//	npu_cmd_map(npu_afm_system, "printafmst");

//	queue_delayed_work(npu_afm_system->afm_dnc_wq,
//			&npu_afm_system->afm_dnc_work,
//			msecs_to_jiffies(1));

	return IRQ_HANDLED;
}

static irqreturn_t npu_afm_isr1(int irq, void *data)
{
	/* TODO : implement. */
	npu_afm_clear_gnpu1_interrupt();
	npu_afm_clear_gnpu1_tdc();
	return IRQ_HANDLED;
}

static irqreturn_t (*afm_isr_list[])(int, void *) = {
	npu_afm_isr0,
	npu_afm_isr1,
};


static void __npu_afm_work(int freq)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name)) {
			npu_scheduler_set_freq(d, &d->qos_req_max, freq);
		}
	}
	mutex_unlock(&info->exec_lock);

	/* TODO : Implement control freq. */
	return;
}

static void npu_afm_dnc_work(struct work_struct *work)
{
/*
	struct npu_system *system;

	system = container_of(work, struct npu_system, afm_dnc_work.work);

	system->ocp_warn_status = 0;

	npu_dbg("afm work start(ocp_status : %d)\n", system->ocp_warn_status);
	if (!system->ocp_warn_status) {
		npu_dbg("Trying release afm_limit_freq\n");
		__npu_afm_work();

		npu_afm_clear_dnc_tdc();
		npu_afm_clear_dnc_interrupt();

		npu_dbg("End release afm_limit_freq\n");
		return;
	}
*/
	__npu_afm_work(800000);
	queue_delayed_work(npu_afm_system->afm_dnc_wq,
			&npu_afm_system->afm_dnc_work,
			msecs_to_jiffies(1000));
	npu_dbg("afm work end\n");
}

void npu_afm_open(struct npu_system *system, int hid)
{
	int ret = 0;
	int npu_idx = 0;

	ret = sub_pmic_get_i2c(&system->i2c);
	if (ret) {
		npu_err("sub regulator_i2c get failed : %d\n", ret);
		return;
	}

	npu_afm_control_grobal(system, HTU_DNC, NPU_AFM_ENABLE);

	if (afm_local_onoff) {
		sub_pmic_update_reg(system->i2c,
				system->afm_buck_offset[npu_idx],
				(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]),
				S2MPS_AFM_WARN_LVL_MASK);
		npu_info("SUB PMIC AFM enable offset : 0x%x, level : 0x%x\n",
				system->afm_buck_offset[npu_idx],
				(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]));
	}

	if (hid & NPU_HWDEV_ID_NPU) {
		if (afm_local_onoff) {
			npu_idx++;
			sub_pmic_update_reg(system->i2c,
					system->afm_buck_offset[npu_idx],
					(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]),
					S2MPS_AFM_WARN_LVL_MASK);
			npu_info("SUB PMIC AFM enable offset : 0x%x, level : 0x%x\n",
					system->afm_buck_offset[npu_idx],
					(S2MPS_AFM_WARN_EN | system->afm_buck_level[npu_idx]));
		}

		npu_afm_control_grobal(system, HTU_GNPU1, NPU_AFM_ENABLE);
	} else if (hid & NPU_HWDEV_ID_DSP) {
		/* TODO */
	}

	system->ocp_warn_status = 0;

	npu_info("open success, hid : %d\n", hid);
}

void npu_afm_close(struct npu_system *system, int hid)
{
	int npu_idx = 0;
	//npu_cmd_map(npu_afm_system, "afmthrdis");
	npu_afm_control_grobal(system, HTU_DNC, NPU_AFM_DISABLE);
	//npu_afm_control_throttling(system, NPU_AFM_DISABLE);
	if (afm_local_onoff) {
		sub_pmic_update_reg(system->i2c,
				system->afm_buck_offset[npu_idx],
				S2MPS_AFM_WARN_DEFAULT_LVL,
				S2MPS_AFM_WARN_LVL_MASK);
		npu_info("SUB PMIC AFM disable offset : 0x%x, level : 0x%x\n",
				system->afm_buck_offset[npu_idx],
				S2MPS_AFM_WARN_DEFAULT_LVL);
	}

	if (hid & NPU_HWDEV_ID_NPU) {
		if (afm_local_onoff) {
			npu_idx++;
			sub_pmic_update_reg(system->i2c,
					system->afm_buck_offset[npu_idx],
					S2MPS_AFM_WARN_DEFAULT_LVL,
					S2MPS_AFM_WARN_LVL_MASK);
			npu_info("SUB PMIC AFM disable offset : 0x%x, level : 0x%x\n",
					system->afm_buck_offset[npu_idx],
					S2MPS_AFM_WARN_DEFAULT_LVL);
		}
		npu_afm_control_grobal(system, HTU_GNPU1, NPU_AFM_DISABLE);
	}

	cancel_delayed_work_sync(&npu_afm_system->afm_dnc_work);

	npu_info("close success, hid : %d\n", hid);
}

static void npu_afm_dt_parsing(struct npu_system *system)
{
	int i, ret = 0;
	char tmp_num[2];
	char tmpname[32];

	struct device *dev = &system->pdev->dev;

	for (i = 0; i < BUCK_CNT; i++) {

		tmp_num[0] = '\0';
		sprintf(tmp_num, "%d", i);
		tmpname[0] = '\0';
		strcpy(tmpname, "samsung,npuafm-gnpu");
		strcat(tmpname, tmp_num);
		strcat(tmpname, "-offset");
		ret = of_property_read_u32(dev->of_node, tmpname, &(system->afm_buck_offset[i]));
		if (ret)
			system->afm_buck_offset[i] = S2MPS_AFM_WARN_DEFAULT_LVL;

		tmpname[0] = '\0';
		strcpy(tmpname, "samsung,npuafm-gnpu");
		strcat(tmpname, tmp_num);
		strcat(tmpname, "-level");
		ret = of_property_read_u32(dev->of_node, tmpname, &(system->afm_buck_level[i]));
		if (ret)
			system->afm_buck_level[i] = S2MPS_AFM_WARN_DEFAULT_LVL;

		probe_info("buck[%d] (offset : 0x%x, level : 0x%x\n", i,
				system->afm_buck_offset[i], system->afm_buck_level[i]);
	}
}

static ssize_t afm_onoff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_onoff : %s\n", afm_local_onoff ? "on" : "off");

	return ret;
}

static ssize_t afm_onoff_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 2) {
			npu_err("Invalid afm onff : %u, please input 0(off) or 1(on)\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	afm_local_onoff = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_onoff);

static int npu_afm_register_debug_sysfs(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device = container_of(system, struct npu_device, system);

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_onoff.attr);
	if (ret) {
		probe_err("sysfs_create_file error : ret = %d\n", ret);
		goto err_exit;
	}

err_exit:
	return ret;
}

int npu_afm_probe(struct npu_device *device)
{
	int i, ret = 0, afm_irq_idx = 0;
	const char *buf;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;
	struct cpumask cpu_mask;

	system->ocp_warn_status = 0;

	for (i = system->irq_num; i < (system->irq_num + system->afm_irq_num); i++, afm_irq_idx++) {
		ret = devm_request_irq(dev, system->irq[i], afm_isr_list[afm_irq_idx],
					IRQF_TRIGGER_HIGH, "exynos-npu", NULL);
		if (ret)
			probe_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
	}

	ret = of_property_read_string(dev->of_node, "samsung,npuinter-isr-cpu-affinity", &buf);
	if (ret) {
		probe_info("AFM set the CPU affinity of ISR to 5\n");
		cpumask_set_cpu(5, &cpu_mask);
	}	else {
		probe_info("AFM set the CPU affinity of ISR to %s\n", buf);
		cpulist_parse(buf, &cpu_mask);
	}

	for (i = system->irq_num; i < (system->irq_num + system->afm_irq_num); i++) {
		ret = irq_set_affinity_hint(system->irq[i], &cpu_mask);
		if (ret) {
			probe_err("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	INIT_DELAYED_WORK(&system->afm_dnc_work, npu_afm_dnc_work);

	system->afm_dnc_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!system->afm_dnc_wq) {
		probe_err("fail to create workqueue -> system->afm_wq\n");
		ret = -EFAULT;
		goto err_probe_irq;
	}

	ret = npu_afm_register_debug_sysfs(system);
	if (ret)
		probe_err("fail(%d) in npu_afm_register_debug_sysfs()\n", ret);

	npu_afm_dt_parsing(system);

	npu_afm_system = system;

	afm_local_onoff = false;

	probe_info("NPU AFM probe success\n");
	return ret;
err_probe_irq:
	for (i = system->irq_num; i < (system->irq_num + system->afm_irq_num); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	probe_err("NPU AFM probe failed(%d)\n", ret);
	return ret;
}

int npu_afm_release(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	for (i = system->irq_num; i < (system->irq_num + system->afm_irq_num); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	npu_afm_system = NULL;
	probe_info("NPU AFM release success\n");
	return ret;
}
