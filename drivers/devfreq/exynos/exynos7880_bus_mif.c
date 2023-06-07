/* linux/drivers/devfreq/exynos/exynos7880_bus_mif.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS7880 SoC MIF devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>

#ifdef CONFIG_UMTS_MODEM_SS310AP
#include <linux/exynos-modem-ctrl.h>
#endif
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/bts.h>
#include <linux/apm-exynos.h>
#include <soc/samsung/asv-exynos.h>
#include <linux/mcu_ipc.h>

#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../../drivers/soc/samsung/pwrcal/S5E7880/S5E7880-vclk.h"
#include "../governor.h"

#define DEVFREQ_MIF_REBOOT_FREQ	(3078000/2)
/* TODO: Update the correct Switching frequency */
#define DEVFREQ_MIF_SWITCH_FREQ	800000

#define DREX0_BASE      0x10400000
#define DREX1_BASE      0x10500000

#define MRSTATUS_THERMAL_BIT_SHIFT	7
#define MRSTATUS_THERMAL_BIT_MASK	1
#define DREX_DIRECTCMD			0x10
#define MRSTATUS_THERMAL_LV_MASK	0x7
#define MRSTATUS			0x54
#define TIMINGAREF			0x30
#define MEMCONTROL			0x04
#define PB_REF_EN 			BIT(27)

static void __iomem	*base_drex0;
static void __iomem	*base_drex1;
static u32		tREFI;
static unsigned long origin_suspend_freq = 0;

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	int channel;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_ch0_cs0;
	unsigned int thermal_level_ch0_cs1;
	unsigned int thermal_level_ch1_cs0;
	unsigned int thermal_level_ch1_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

static struct workqueue_struct *devfreq_mif_thermal_wq;
struct devfreq_thermal_work devfreq_mif_thermal_work = {
	.polling_period = 500,
};

u32 sw_volt_table[1];

int is_dll_on(void)
{
	return cal_dfs_ext_ctrl(dvfs_mif, cal_dfs_mif_is_dll_on, 0);
}
EXPORT_SYMBOL_GPL(is_dll_on);

static struct exynos_devfreq_data *mif_data;

static int exynos7880_devfreq_mif_cmu_dump(struct device *dev,
					struct exynos_devfreq_data *data)
{
	mutex_lock(&data->devfreq->lock);
	cal_vclk_dbg_info(dvfs_mif);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static int exynos7880_devfreq_mif_reboot(struct device *dev,
					struct exynos_devfreq_data *data)
{
	u32 freq = DEVFREQ_MIF_REBOOT_FREQ;

	data->max_freq = freq;
	data->devfreq->max_freq = data->max_freq;

	mutex_lock(&data->devfreq->lock);
	update_devfreq(data->devfreq);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static int exynos7880_devfreq_cl_dvfs_start(struct exynos_devfreq_data *data)
{
	int ret = 0;

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	ret = exynos_cl_dvfs_start(ID_MIF);
#endif

	return ret;
}

static int exynos7880_devfreq_cl_dvfs_stop(u32 target_idx,
					struct exynos_devfreq_data *data)
{
	int ret = 0;

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	ret = exynos_cl_dvfs_stop(ID_MIF, target_idx);
#endif

	return ret;
}

static int exynos7880_devfreq_mif_get_switch_freq(u32 cur_freq, u32 new_freq,
						u32 *switch_freq)
{
	*switch_freq = DEVFREQ_MIF_SWITCH_FREQ;

	return 0;
}

static int exynos7880_devfreq_mif_get_switch_voltage(u32 cur_freq, u32 new_freq,
						struct exynos_devfreq_data *data)
{
	if (DEVFREQ_MIF_SWITCH_FREQ >= cur_freq)
		if (new_freq >= DEVFREQ_MIF_SWITCH_FREQ)
			data->switch_volt = data->new_volt;
		else
			data->switch_volt = sw_volt_table[0];
	else
		if (cur_freq >= new_freq)
			data->switch_volt = data->old_volt;
		else
			data->switch_volt = data->new_volt;

	//pr_info("Selected switching voltage: %uuV\n", data->switch_volt);
	return 0;
}

static int exynos7880_devfreq_mif_get_freq(struct device *dev, u32 *cur_freq,
					struct exynos_devfreq_data *data)
{
	*cur_freq = (u32)clk_get_rate(data->clk);
	if (*cur_freq == 0) {
		dev_err(dev, "failed to get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos7880_devfreq_mif_change_to_switch_freq(struct device *dev,
					struct exynos_devfreq_data *data)
{
	if (clk_set_rate(data->sw_clk, data->switch_freq)) {
		dev_err(dev, "failed to set switching frequency by CAL (%uKhz for %uKhz)\n",
				data->switch_freq, data->new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos7880_devfreq_mif_restore_from_switch_freq(struct device *dev,
					struct exynos_devfreq_data *data)
{
	if (clk_set_rate(data->clk, data->new_freq)) {
		dev_err(dev, "failed to set frequency by CAL (%uKhz)\n",
				data->new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos7880_devfreq_mif_set_freq_post(struct device *dev,
						struct exynos_devfreq_data *data)
{
	/* Send information about MIF frequency to mailbox */
	mbox_set_value(MCU_CP, MCU_IPC_INT13, data->new_freq);

	return 0;
}

static int exynos7880_devfreq_mif_init_freq_table(struct device *dev,
						struct exynos_devfreq_data *data)
{
	u32 max_freq, min_freq, cur_freq;
	unsigned long tmp_max, tmp_min;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i, ret;

	ret = cal_clk_enable(dvfs_mif);
	if (ret) {
		dev_err(dev, "failed to enable MIF\n");
		return -EINVAL;
	}

	max_freq = (u32)cal_dfs_get_max_freq(dvfs_mif);
	if (!max_freq) {
		dev_err(dev, "failed to get max frequency\n");
		return -EINVAL;
	}

	dev_info(dev, "max_freq: %uKhz, get_max_freq: %uKhz\n",
			data->max_freq, max_freq);

	if (max_freq < data->max_freq) {
		rcu_read_lock();
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_max = (unsigned long)max_freq;
		target_opp = devfreq_recommended_opp(dev, &tmp_max, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(dev, "not found valid OPP for max_freq\n");
			return PTR_ERR(target_opp);
		}

		data->max_freq = (u32)dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	min_freq = (u32)cal_dfs_get_min_freq(dvfs_mif);
	if (!min_freq) {
		dev_err(dev, "failed to get min frequency\n");
		return -EINVAL;
	}

	dev_info(dev, "min_freq: %uKhz, get_min_freq: %uKhz\n",
			data->min_freq, min_freq);

	if (min_freq > data->min_freq) {
		rcu_read_lock();
		flags &= ~DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_min = (unsigned long)min_freq;
		target_opp = devfreq_recommended_opp(dev, &tmp_min, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(dev, "not found valid OPP for min_freq\n");
			return PTR_ERR(target_opp);
		}

		data->min_freq = (u32)dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	dev_info(dev, "min_freq: %uKhz, max_freq: %uKhz\n",
			data->min_freq, data->max_freq);

	cur_freq = clk_get_rate(data->clk);
	dev_info(dev, "current frequency: %uKhz\n", cur_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(dev, (unsigned long)data->opp_list[i].freq);
	}

	return 0;
}

static int exynos7880_devfreq_mif_get_volt_table(struct device *dev, u32 *volt_table,
						struct exynos_devfreq_data *data)
{
	struct dvfs_rate_volt mif_rate_volt[data->max_state];
	int table_size;
	int i;

	table_size = cal_dfs_get_rate_asv_table(dvfs_mif, mif_rate_volt);
	if (!table_size) {
		dev_err(dev, "failed to get ASV table\n");
		return -ENODEV;
	}

	if (table_size != data->max_state) {
		dev_err(dev, "ASV table size is not matched\n");
		return -ENODEV;
	}

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq != (u32)(mif_rate_volt[i].rate)) {
			dev_err(dev, "Freq table is not matched(%u:%u)\n",
				data->opp_list[i].freq, (u32)mif_rate_volt[i].rate);
			return -EINVAL;
		}
		volt_table[i] = (u32)mif_rate_volt[i].volt;

		/* Fill switch voltage table */
		if (!sw_volt_table[0] &&
			data->opp_list[i].freq < DEVFREQ_MIF_SWITCH_FREQ)
			sw_volt_table[0] = (u32)mif_rate_volt[i-1].volt;
	}

	dev_info(dev, "SW_volt %uuV in freq %uKhz\n",
			sw_volt_table[0], DEVFREQ_MIF_SWITCH_FREQ);

	return 0;
}

static void exynos7880_devfreq_mif_set_PBR_en(bool en)
{
	u32 tmp0, tmp1;
	tmp0 = __raw_readl(base_drex0 + MEMCONTROL);
	tmp1 = __raw_readl(base_drex1 + MEMCONTROL);

	if (en) {
		if ((tmp0 & (1 << 27)) && (tmp1 & (1 << 27)))
			return;
	} else {
		if (((tmp0 & (1 << 27)) == 0) && ((tmp1 & (1 << 27)) == 0))
			return;
	}

	tmp0 = (u32)((tmp0 & ~PB_REF_EN) | (en << 27));
	tmp1 = (u32)((tmp1 & ~PB_REF_EN) | (en << 27));
	__raw_writel(tmp0, base_drex0 + MEMCONTROL);
	__raw_writel(tmp1, base_drex1 + MEMCONTROL);
	return;
}

static void exynos7880_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

enum devfreq_mif_thermal_autorate {
	RATE_TWO = 0x001900CB,
	RATE_ONE = 0x000C0065,
	RATE_HALF = 0x00060032,
	RATE_QUARTER = 0x00030019,
};

static enum devfreq_mif_thermal_autorate exynos7880_devfreq_get_autorate(unsigned int thermal_level)
{
	enum devfreq_mif_thermal_autorate timingaref_value = RATE_ONE;
	switch (thermal_level) {
	case 0:
	case 1:
	case 2:
		timingaref_value = RATE_TWO;
		break;
	case 3:
		timingaref_value = RATE_ONE;
		break;
	case 4:
		timingaref_value = RATE_HALF;
		break;
	case 5:
	case 6:
	case 7:
		timingaref_value = RATE_QUARTER;
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return -1;
	}
	return timingaref_value;
}

static void exynos7880_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int max_thermal_level = 0;
	unsigned int ch0_timingaref_value = RATE_ONE;
	unsigned int ch1_timingaref_value = RATE_ONE;
	unsigned int ch0_cs0_thermal_level = 0;
	unsigned int ch0_cs1_thermal_level = 0;
	unsigned int ch1_cs0_thermal_level = 0;
	unsigned int ch1_cs1_thermal_level = 0;
	unsigned int ch0_max_thermal_level = 0;
	unsigned int ch1_max_thermal_level = 0;
	unsigned int mrstatus0 = 0;
	unsigned int mrstatus1 = 0;

	mutex_lock(&mif_data->lock);

	__raw_writel(0x09001000, base_drex0 + DREX_DIRECTCMD);
	mrstatus0 = __raw_readl(base_drex0 + MRSTATUS);
	ch0_cs0_thermal_level = (mrstatus0 & MRSTATUS_THERMAL_LV_MASK);
	if (ch0_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch0_cs0_thermal_level;
	thermal_work->thermal_level_ch0_cs0 = ch0_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex0 + DREX_DIRECTCMD);
	mrstatus0 = __raw_readl(base_drex0 + MRSTATUS);
	ch0_cs1_thermal_level = (mrstatus0 & MRSTATUS_THERMAL_LV_MASK);
	if (ch0_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch0_cs1_thermal_level;
	thermal_work->thermal_level_ch0_cs1 = ch0_cs1_thermal_level;

	ch0_max_thermal_level = max(ch0_cs0_thermal_level, ch0_cs1_thermal_level);

	__raw_writel(0x09001000, base_drex1 + DREX_DIRECTCMD);
	mrstatus1 = __raw_readl(base_drex1 + MRSTATUS);
	ch1_cs0_thermal_level = (mrstatus1 & MRSTATUS_THERMAL_LV_MASK);
	if (ch1_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch1_cs0_thermal_level;
	thermal_work->thermal_level_ch1_cs0 = ch1_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex1 + DREX_DIRECTCMD);
	mrstatus1 = __raw_readl(base_drex1 + MRSTATUS);
	ch1_cs1_thermal_level = (mrstatus1 & MRSTATUS_THERMAL_LV_MASK);
	if (ch1_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch1_cs1_thermal_level;
	thermal_work->thermal_level_ch1_cs1 = ch1_cs1_thermal_level;

	ch1_max_thermal_level = max(ch1_cs0_thermal_level, ch1_cs1_thermal_level);

	mutex_unlock(&mif_data->lock);

	switch (max_thermal_level) {
	case 0:
	case 1:
	case 2:
	case 3:
		thermal_work->polling_period = 100;
		break;
	case 4:
		thermal_work->polling_period = 100;
		break;
	case 5:
	case 6:
		thermal_work->polling_period = 100;
		break;
	case 7:
		thermal_work->polling_period = 100;
		orderly_poweroff(true);
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	/* set auto refresh rate */
	mutex_lock(&mif_data->lock);
	ch0_timingaref_value = exynos7880_devfreq_get_autorate(ch0_max_thermal_level);
	if (ch0_timingaref_value > 0) {
		__raw_writel(ch0_timingaref_value, base_drex0 + TIMINGAREF);
		/* tREFI is the worst tREFI of 2 channels */
		tREFI = ch0_timingaref_value;
	}
	ch1_timingaref_value = exynos7880_devfreq_get_autorate(ch1_max_thermal_level);
	if (ch1_timingaref_value > 0) {
		__raw_writel(ch1_timingaref_value, base_drex1 + TIMINGAREF);
		tREFI = min(tREFI, ch1_timingaref_value);
	}
	mutex_unlock(&mif_data->lock);

	exynos7880_devfreq_thermal_event(thermal_work);
}

static void exynos7880_devfreq_init_thermal(void)
{
	/* enable per-bank refresh mode */
	exynos7880_devfreq_mif_set_PBR_en(1);
	devfreq_mif_thermal_wq = alloc_workqueue("devfreq_mif_thermal_wq", WQ_FREEZABLE | WQ_MEM_RECLAIM, 0);
	INIT_DELAYED_WORK(&devfreq_mif_thermal_work.devfreq_mif_thermal_work,
			exynos7880_devfreq_thermal_monitor);
	devfreq_mif_thermal_work.work_queue = devfreq_mif_thermal_wq;
	exynos7880_devfreq_thermal_event(&devfreq_mif_thermal_work);
}

static int exynos7880_devfreq_mif_notify_panic(struct notifier_block *nb,
	unsigned long code, void *unused)
{
	u32 tmp0, tmp1;

	tmp0 = __raw_readl(base_drex0 + TIMINGAREF);
	tmp1 = __raw_readl(base_drex1 + TIMINGAREF);

	pr_info("Refresh Rate DREX0: 0x%x\t DREX1: 0x%x\n", tmp0, tmp1);

	return NOTIFY_DONE;
}

static struct notifier_block exynos7880_devfreq_mif_panic_nb = {
	.notifier_call = exynos7880_devfreq_mif_notify_panic,
};

static int exynos7880_devfreq_mif_init(struct device *dev,
					struct exynos_devfreq_data *data)
{
	data->clk = clk_get(dev, "dvfs_mif");
	if (IS_ERR_OR_NULL(data->clk)) {
		dev_err(dev, "failed to get dvfs vclk\n");
		return -ENODEV;
	}

	data->sw_clk = clk_get(dev, "dvfs_mif_sw");
	if (IS_ERR_OR_NULL(data->sw_clk)) {
		dev_err(dev, "failed to get dvfs sw vclk\n");
		clk_put(data->clk);
		return -ENODEV;
	}

	base_drex0 = devm_ioremap(data->dev, DREX0_BASE, SZ_64K);
	base_drex1 = devm_ioremap(data->dev, DREX1_BASE, SZ_64K);

	exynos7880_devfreq_init_thermal();

	atomic_notifier_chain_register(&panic_notifier_list,
			&exynos7880_devfreq_mif_panic_nb);

	return 0;
}

static int exynos7880_devfreq_mif_exit(struct device *dev,
					struct exynos_devfreq_data *data)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&exynos7880_devfreq_mif_panic_nb);
	cancel_delayed_work_sync(&devfreq_mif_thermal_work.devfreq_mif_thermal_work);
	flush_delayed_work(&devfreq_mif_thermal_work.devfreq_mif_thermal_work);
	flush_workqueue(devfreq_mif_thermal_wq);
	destroy_workqueue(devfreq_mif_thermal_wq);
	clk_put(data->sw_clk);
	clk_put(data->clk);

	return 0;
}

static int exynos7880_devfreq_mif_pm_suspend_prepare(struct device *dev,
					struct exynos_devfreq_data *data)
{
#ifdef CONFIG_UMTS_MODEM_SS310AP
	u32 evs_suspend_freq = 0;
#endif
	if (!origin_suspend_freq)
		origin_suspend_freq = data->devfreq_profile.suspend_freq;

#ifdef CONFIG_UMTS_MODEM_SS310AP
	evs_suspend_freq = ss310ap_get_evs_mode_ext();

	if (evs_suspend_freq)
		data->devfreq_profile.suspend_freq = evs_suspend_freq;
	else
		data->devfreq_profile.suspend_freq = origin_suspend_freq;
#endif

	return 0;
}

/* MIF devfreq gets suspended last amongst all devfreq devices */
static int exynos7880_devfreq_mif_set_cp_voltage(struct device *dev,
			struct exynos_devfreq_data *data)
{
	struct regulator *vdd_buck2_out2;
	int cp_voltage;
	int ap_voltage;
	int ret = 0;

	vdd_buck2_out2 = regulator_get(NULL, "BUCK2_OUT2");
	if (IS_ERR(vdd_buck2_out2)) {
		dev_err(data->dev, "%s: failed get regulator\n",
				__func__);
		return -ENODEV;
	}

	cp_voltage =  regulator_get_voltage(vdd_buck2_out2);
	ap_voltage =  regulator_get_voltage(data->vdd);
	if (ap_voltage < cp_voltage) {
		ret = regulator_set_voltage(data->vdd, cp_voltage, cp_voltage);
		if (ret) {
			dev_err(dev, "failed to set CP voltage to %d\n",
				cp_voltage);
			WARN_ON(1);
		}
	}
	regulator_put(vdd_buck2_out2);

	return ret;
}

static int __init exynos7880_devfreq_mif_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.init = exynos7880_devfreq_mif_init;
	data->ops.exit = exynos7880_devfreq_mif_exit;
	data->ops.get_volt_table = exynos7880_devfreq_mif_get_volt_table;
	data->ops.get_switch_freq = exynos7880_devfreq_mif_get_switch_freq;
	data->ops.get_switch_voltage = exynos7880_devfreq_mif_get_switch_voltage;
	data->ops.get_freq = exynos7880_devfreq_mif_get_freq;
	data->ops.change_to_switch_freq = exynos7880_devfreq_mif_change_to_switch_freq;
	data->ops.restore_from_switch_freq = exynos7880_devfreq_mif_restore_from_switch_freq;
	data->ops.set_freq_post = exynos7880_devfreq_mif_set_freq_post;
	data->ops.init_freq_table = exynos7880_devfreq_mif_init_freq_table;
	data->ops.cl_dvfs_start = exynos7880_devfreq_cl_dvfs_start;
	data->ops.cl_dvfs_stop = exynos7880_devfreq_cl_dvfs_stop;
	data->ops.pm_suspend_prepare = exynos7880_devfreq_mif_pm_suspend_prepare;
	data->ops.reboot = exynos7880_devfreq_mif_reboot;
	data->ops.cmu_dump = exynos7880_devfreq_mif_cmu_dump;
	data->ops.suspend = exynos7880_devfreq_mif_set_cp_voltage;

	/* disable MIF HWACG */
	cal_dfs_ext_ctrl(dvfs_mif, cal_dfs_ctrl_clk_gate, 0);

	mif_data = data;

	return 0;
}

static int __init exynos7880_devfreq_mif_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_MIF,
				exynos7880_devfreq_mif_init_prepare))
		return -EINVAL;

	return 0;
}
fs_initcall(exynos7880_devfreq_mif_initcall);
