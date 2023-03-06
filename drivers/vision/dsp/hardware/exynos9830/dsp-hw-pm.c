// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-pm.h"

static unsigned int dnc_table[] = {
	800000,
	666000,
	533000,
	466000,
	400000,
	333000,
	266000,
	100000,
};

static unsigned int dsp_table[] = {
	933000,
	800000,
	666000,
	533000,
	400000,
	333000,
	200000,
	100000,
};

int dsp_pm_devfreq_active(struct dsp_pm *pm)
{
	dsp_check();
	return pm_qos_request_active(&pm->devfreq[DSP_DEVFREQ_DSP].req);
}

static int __dsp_pm_check_valid(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_check();
	if (val < 0 || val > devfreq->min_qos) {
		dsp_err("devfreq[%s] value(%d) is invalid(L0 ~ L%u)\n",
				devfreq->name, val, devfreq->min_qos);
		return -EINVAL;
	} else {
		return 0;
	}
}

static void __dsp_pm_update_freq_info(struct dsp_pm *pm, int id)
{
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = &pm->devfreq[id];

	if (id == DSP_DEVFREQ_DNC)
		dsp_ctrl_sm_writel(DSP_SM_RESERVED(DNC_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
	else if (id == DSP_DEVFREQ_DSP)
		dsp_ctrl_sm_writel(DSP_SM_RESERVED(DSP_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
	else
		dsp_err("Failed to update freq info as invalid id(%d)\n", id);
	dsp_leave();
}

static int __dsp_pm_update_devfreq(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_pm_check_valid(devfreq, val);
	if (ret)
		goto p_err;

	if (devfreq->current_qos != val) {
		pm_qos_update_request(&devfreq->req, devfreq->table[val]);
		dsp_dbg("devfreq[%s] is changed from L%d to L%d\n",
				devfreq->name, devfreq->current_qos, val);
		devfreq->current_qos = val;
	} else {
		dsp_dbg("devfreq[%s] is already running L%d\n",
				devfreq->name, val);
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_update_devfreq_nolock(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (!dsp_pm_devfreq_active(pm)) {
		ret = -EINVAL;
		dsp_warn("dsp is not running\n");
		goto p_err;
	}

	if (id < 0 || id >= DSP_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	/*
	 * MIF min lock (1352000) required when using L0 of pd-dsp
	 * due to power domain source
	 */
	if (id == DSP_DEVFREQ_DSP) {
		if (pm->devfreq[DSP_DEVFREQ_DSP].current_qos && !val) {
			pm_qos_update_request(&pm->mif_qos, 1352000);
			dsp_dbg("devfreq[mif] change high\n");
		} else if (!pm->devfreq[DSP_DEVFREQ_DSP].current_qos && val) {
			pm_qos_update_request(&pm->mif_qos, 0);
			dsp_dbg("devfreq[mif] change low\n");
		}
	}

	ret = __dsp_pm_update_devfreq(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	__dsp_pm_update_freq_info(pm, id);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_set_force_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= DSP_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	if (val >= 0) {
		ret = __dsp_pm_check_valid(&pm->devfreq[id], val);
		if (ret)
			goto p_err;
	}

	pm->devfreq[id].force_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_pm_del_static(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->static_count[val]) {
		ret = -EINVAL;
		dsp_warn("static count is unstable([%s][L%d]%u)\n",
				devfreq->name, val, devfreq->static_count[val]);
		goto p_err;
	} else {
		devfreq->static_count[val]--;
		if (devfreq->static_total_count) {
			devfreq->static_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("static total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->static_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->static_qos) && (!devfreq->static_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->static_qos = idx;
				break;
			}
			if (devfreq->static_count[idx]) {
				devfreq->static_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_pm_add_static(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_enter();
	devfreq->static_count[val]++;
	devfreq->static_total_count++;

	if (devfreq->static_total_count == 1)
		devfreq->static_qos = val;
	else if (val < devfreq->static_qos)
		devfreq->static_qos = val;

	dsp_leave();
}

int dsp_pm_dvfs_enable(struct dsp_pm *pm, int val)
{
	int ret, dnc_run, dsp_run;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs_disable_count) {
		ret = -EINVAL;
		dsp_warn("dvfs disable count is unstable(%u)\n",
				pm->dvfs_disable_count);
		goto p_err;
	}

	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DSP], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_del_static(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_del_static(&pm->devfreq[DSP_DEVFREQ_DSP], val);
	if (ret) {
		__dsp_pm_add_static(&pm->devfreq[DSP_DEVFREQ_DNC], val);
		goto p_err;
	}

	if (!(--pm->dvfs_disable_count)) {
		pm->dvfs = true;
		dsp_info("DVFS enabled\n");
	}

	if (!dsp_pm_devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (pm->devfreq[DSP_DEVFREQ_DNC].static_total_count) {
		if (pm->devfreq[DSP_DEVFREQ_DNC].force_qos < 0)
			dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].static_qos;
		else
			dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].force_qos;
	} else {
		dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].dynamic_qos;
	}

	if (pm->devfreq[DSP_DEVFREQ_DSP].static_total_count) {
		if (pm->devfreq[DSP_DEVFREQ_DSP].force_qos < 0)
			dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].static_qos;
		else
			dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].force_qos;
	} else {
		dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].dynamic_qos;
	}

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, dnc_run);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, dsp_run);
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_pm_dvfs_disable(struct dsp_pm *pm, int val)
{
	int ret, dnc_run, dsp_run;

	dsp_enter();
	mutex_lock(&pm->lock);
	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DSP], val);
	if (ret)
		goto p_err;

	pm->dvfs = false;
	if (!pm->dvfs_disable_count)
		dsp_info("DVFS disabled\n");
	pm->dvfs_disable_count++;

	__dsp_pm_add_static(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	__dsp_pm_add_static(&pm->devfreq[DSP_DEVFREQ_DSP], val);

	if (!dsp_pm_devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (pm->devfreq[DSP_DEVFREQ_DNC].force_qos < 0)
		dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].static_qos;
	else
		dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].force_qos;

	if (pm->devfreq[DSP_DEVFREQ_DSP].force_qos < 0)
		dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].static_qos;
	else
		dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].force_qos;

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, dnc_run);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, dsp_run);
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static void __dsp_pm_add_dynamic(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_enter();
	devfreq->dynamic_count[val]++;
	devfreq->dynamic_total_count++;

	if (devfreq->dynamic_total_count == 1)
		devfreq->dynamic_qos = val;
	else if (val < devfreq->dynamic_qos)
		devfreq->dynamic_qos = val;

	dsp_leave();
}

int dsp_pm_update_devfreq_busy(struct dsp_pm *pm, int val)
{
	int ret, dnc_run, dsp_run;

	dsp_enter();
	mutex_lock(&pm->lock);
	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DSP], val);
	if (ret)
		goto p_err;

	__dsp_pm_add_dynamic(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	__dsp_pm_add_dynamic(&pm->devfreq[DSP_DEVFREQ_DSP], val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(busy)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!dsp_pm_devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (pm->devfreq[DSP_DEVFREQ_DNC].force_qos < 0)
		dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].dynamic_qos;
	else
		dnc_run = pm->devfreq[DSP_DEVFREQ_DNC].force_qos;

	if (pm->devfreq[DSP_DEVFREQ_DSP].force_qos < 0)
		dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].dynamic_qos;
	else
		dsp_run = pm->devfreq[DSP_DEVFREQ_DSP].force_qos;

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, dnc_run);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, dsp_run);
	dsp_dbg("DVFS busy\n");
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static int __dsp_pm_del_dynamic(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->dynamic_count[val]) {
		ret = -EINVAL;
		dsp_warn("dynamic count is unstable([%s][L%d]%u)\n",
				devfreq->name, val,
				devfreq->dynamic_count[val]);
		goto p_err;
	} else {
		devfreq->dynamic_count[val]--;
		if (devfreq->dynamic_total_count) {
			devfreq->dynamic_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("dynamic total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->dynamic_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->dynamic_qos) && (!devfreq->dynamic_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->dynamic_qos = idx;
				break;
			}
			if (devfreq->dynamic_count[idx]) {
				devfreq->dynamic_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_update_devfreq_idle(struct dsp_pm *pm, int val)
{
	int ret;

	dsp_enter();
	mutex_lock(&pm->lock);
	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_check_valid(&pm->devfreq[DSP_DEVFREQ_DSP], val);
	if (ret)
		goto p_err;

	__dsp_pm_del_dynamic(&pm->devfreq[DSP_DEVFREQ_DNC], val);
	__dsp_pm_del_dynamic(&pm->devfreq[DSP_DEVFREQ_DSP], val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(idle)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!dsp_pm_devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC,
			pm->devfreq[DSP_DEVFREQ_DNC].dynamic_qos);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP,
			pm->devfreq[DSP_DEVFREQ_DSP].dynamic_qos);
	dsp_dbg("DVFS idle\n");
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_pm_update_devfreq_boot(struct dsp_pm *pm)
{
	int dnc_boot, dsp_boot;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(boot)\n");
		dsp_clk_dump(&pm->sys->clk);
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (pm->devfreq[DSP_DEVFREQ_DNC].force_qos < 0)
		dnc_boot = pm->devfreq[DSP_DEVFREQ_DNC].boot_qos;
	else
		dnc_boot = pm->devfreq[DSP_DEVFREQ_DNC].force_qos;

	if (pm->devfreq[DSP_DEVFREQ_DSP].force_qos < 0)
		dsp_boot = pm->devfreq[DSP_DEVFREQ_DSP].boot_qos;
	else
		dsp_boot = pm->devfreq[DSP_DEVFREQ_DSP].force_qos;

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, dnc_boot);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, dsp_boot);
	dsp_dbg("DVFS boot\n");
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

int dsp_pm_update_devfreq_max(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(max)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, 0);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, 0);
	dsp_dbg("DVFS max\n");
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

int dsp_pm_update_devfreq_min(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(min)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC,
			pm->devfreq[DSP_DEVFREQ_DNC].min_qos);
	dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP,
			pm->devfreq[DSP_DEVFREQ_DSP].min_qos);
	dsp_dbg("DVFS min\n");
	dsp_clk_dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int __dsp_pm_set_boot_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= DSP_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_pm_check_valid(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	pm->devfreq[id].boot_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_set_boot_qos(struct dsp_pm *pm, int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_pm_set_boot_qos(pm, DSP_DEVFREQ_DNC, val);
	if (ret)
		goto p_err;

	ret = __dsp_pm_set_boot_qos(pm, DSP_DEVFREQ_DSP, val);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_boost_enable(struct dsp_pm *pm)
{
	int ret;

	dsp_enter();
	ret = dsp_pm_dvfs_disable(pm, 0);
	if (ret)
		goto p_err_dvfs;

	mutex_lock(&pm->lock);
	pm_qos_update_request(&pm->mif_qos, 2730000);
	pm_qos_update_request(&pm->int_qos, 533000);
	//pm_qos_update_request(&pm->cl0_qos, 1950000);
	//pm_qos_update_request(&pm->cl1_qos, 2314000);
	pm_qos_update_request(&pm->cl2_qos, 2730000);
	mutex_unlock(&pm->lock);

	dsp_info("boost mode of pm is enabled\n");
	dsp_leave();
	return 0;
p_err_dvfs:
	return ret;
}

int dsp_pm_boost_disable(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_lock(&pm->lock);
	/*
	 * MIF min lock (1352000) required when using L0 of pd-dsp
	 * due to power domain source
	 */
	if (!pm->devfreq[DSP_DEVFREQ_DSP].current_qos)
		pm_qos_update_request(&pm->mif_qos, 1352000);
	else
		pm_qos_update_request(&pm->mif_qos, 0);

	pm_qos_update_request(&pm->int_qos, 0);
	//pm_qos_update_request(&pm->cl0_qos, 0);
	//pm_qos_update_request(&pm->cl1_qos, 0);
	pm_qos_update_request(&pm->cl2_qos, 0);
	mutex_unlock(&pm->lock);

	dsp_pm_dvfs_enable(pm, 0);
	dsp_info("boost mode of pm is disabled\n");
	dsp_leave();
	return 0;
}

static void __dsp_pm_enable(struct dsp_pm_devfreq *devfreq, bool dvfs)
{
	int init_qos;

	dsp_enter();
	if (devfreq->force_qos < 0) {
		if (!dvfs) {
			init_qos = devfreq->static_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(static)\n",
					devfreq->name, init_qos);
		} else {
			init_qos = devfreq->min_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(dynamic)\n",
					devfreq->name, init_qos);
		}
	} else {
		init_qos = devfreq->force_qos;
		dsp_info("devfreq[%s] is enabled(L%d)(force)\n",
				devfreq->name, init_qos);
	}

	pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[init_qos]);
	devfreq->current_qos = init_qos;
	dsp_leave();
}

int dsp_pm_enable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);

	for (idx = 0; idx < DSP_DEVFREQ_COUNT; ++idx) {
		__dsp_pm_enable(&pm->devfreq[idx], pm->dvfs);
		__dsp_pm_update_freq_info(pm, idx);
	}

	/*
	 * MIF min lock (1352000) required when using L0 of pd-dsp
	 * due to power domain source
	 */
	if (!pm->devfreq[DSP_DEVFREQ_DSP].current_qos) {
		pm_qos_add_request(&pm->mif_qos, PM_QOS_BUS_THROUGHPUT,
				1352000);
		dsp_info("devfreq[mif] set high\n");
	} else {
		pm_qos_add_request(&pm->mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
		dsp_info("devfreq[mif] set low\n");
	}
	pm_qos_add_request(&pm->int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
	pm_qos_add_request(&pm->cl0_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&pm->cl1_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
	pm_qos_add_request(&pm->cl2_qos, PM_QOS_CLUSTER2_FREQ_MIN, 0);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static void __dsp_pm_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	pm_qos_remove_request(&devfreq->req);
	dsp_info("devfreq[%s] is disabled\n", devfreq->name);
	dsp_leave();
}

int dsp_pm_disable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);

	for (idx = 0; idx < DSP_DEVFREQ_COUNT; ++idx)
		__dsp_pm_disable(&pm->devfreq[idx]);
	pm_qos_remove_request(&pm->mif_qos);
	pm_qos_remove_request(&pm->int_qos);
	pm_qos_remove_request(&pm->cl0_qos);
	pm_qos_remove_request(&pm->cl1_qos);
	pm_qos_remove_request(&pm->cl2_qos);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

int dsp_pm_open(struct dsp_pm *pm)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_pm_init(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	devfreq->static_qos = devfreq->min_qos;
	devfreq->static_total_count = 0;
	memset(devfreq->static_count, 0x0, DSP_DEVFREQ_RESERVED_COUNT << 2);
	devfreq->dynamic_qos = devfreq->min_qos;
	devfreq->dynamic_total_count = 0;
	memset(devfreq->dynamic_count, 0x0, DSP_DEVFREQ_RESERVED_COUNT << 2);
	devfreq->force_qos = -1;
	dsp_leave();
}

int dsp_pm_close(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	if (!pm->dvfs_lock) {
		dsp_info("DVFS is reinitialized\n");
		pm->dvfs = true;
		pm->dvfs_disable_count = 0;
		for (idx = 0; idx < DSP_DEVFREQ_COUNT; ++idx)
			__dsp_pm_init(&pm->devfreq[idx]);
	}
	dsp_leave();
	return 0;
}

int dsp_pm_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_pm *pm;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	pm = &sys->pm;
	pm->sys = sys;

	pm->devfreq = kzalloc(sizeof(*devfreq) * DSP_DEVFREQ_COUNT, GFP_KERNEL);
	if (!pm->devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_pm_devfreq\n");
		goto p_err;
	}

	devfreq = &pm->devfreq[DSP_DEVFREQ_DNC];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "dnc");
	devfreq->count = sizeof(dnc_table) / sizeof(*dnc_table);
	devfreq->table = dnc_table;
	devfreq->class_id = PM_QOS_DNC_THROUGHPUT;
	devfreq->force_qos = -1;
	devfreq->min_qos = devfreq->count - 1;
	devfreq->dynamic_qos = devfreq->min_qos;
	devfreq->static_qos = devfreq->min_qos;

	devfreq = &pm->devfreq[DSP_DEVFREQ_DSP];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "dsp");
	devfreq->count = sizeof(dsp_table) / sizeof(*dsp_table);
	devfreq->table = dsp_table;
	devfreq->class_id = PM_QOS_DSP_THROUGHPUT;
	devfreq->force_qos = -1;
	devfreq->min_qos = devfreq->count - 1;
	devfreq->dynamic_qos = devfreq->min_qos;
	devfreq->static_qos = devfreq->min_qos;

	pm_runtime_enable(sys->dev);
	mutex_init(&pm->lock);
	pm->dvfs = true;
	pm->dvfs_lock = false;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_pm_remove(struct dsp_pm *pm)
{
	dsp_enter();
	mutex_destroy(&pm->lock);
	pm_runtime_disable(pm->sys->dev);
	kfree(pm->devfreq);
	dsp_leave();
}
