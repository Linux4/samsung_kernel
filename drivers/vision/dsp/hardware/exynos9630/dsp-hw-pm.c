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
	444000,
	266000,
	133000,
};

static unsigned int dsp_table[] = {
	800000,
	666000,
	533000,
	444000,
	266000,
	133000,
};

int dsp_pm_devfreq_active(struct dsp_pm *pm)
{
	dsp_check();
	return pm_qos_request_active(&pm->devfreq[DSP_DEVFREQ_DSP].req);
}

static int __dsp_pm_check_valid(struct dsp_pm_devfreq *devfreq, int val)
{
	dsp_check();
	if (val < 0 || val >= devfreq->count) {
		dsp_err("devfreq[%s] value(%d) is invalid(L0 ~ L%u)\n",
				devfreq->name, val, devfreq->count - 1);
		return -EINVAL;
	} else {
		return 0;
	}
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
		dsp_info("devfreq[%s] is changed from L%d to L%d\n",
				devfreq->name, devfreq->current_qos, val);
		devfreq->current_qos = val;
	} else {
		dsp_info("devfreq[%s] is already running L%d\n",
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

	ret = __dsp_pm_update_devfreq(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_update_devfreq(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	mutex_lock(&pm->lock);

	ret = dsp_pm_update_devfreq_nolock(pm, id, val);
	if (ret)
		goto p_err;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_pm_update_devfreq_max(struct dsp_pm *pm)
{
	int ret;

	dsp_enter();
	mutex_lock(&pm->lock);

	ret = dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC, 0);
	if (ret)
		goto p_err;

	ret = dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP, 0);
	if (ret)
		goto p_err;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_pm_update_devfreq_min(struct dsp_pm *pm)
{
	int ret;

	dsp_enter();
	mutex_lock(&pm->lock);

	ret = dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DNC,
			pm->devfreq[DSP_DEVFREQ_DNC].count - 1);
	if (ret)
		goto p_err;

	ret = dsp_pm_update_devfreq_nolock(pm, DSP_DEVFREQ_DSP,
			pm->devfreq[DSP_DEVFREQ_DSP].count - 1);
	if (ret)
		goto p_err;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

void dsp_pm_resume(struct dsp_pm *pm)
{
	dsp_enter();
	dsp_pm_update_devfreq(pm, DSP_DEVFREQ_DNC,
			pm->devfreq[DSP_DEVFREQ_DNC].resume_qos);
	dsp_pm_update_devfreq(pm, DSP_DEVFREQ_DSP,
			pm->devfreq[DSP_DEVFREQ_DSP].resume_qos);
	pm->devfreq[DSP_DEVFREQ_DNC].resume_qos = -1;
	pm->devfreq[DSP_DEVFREQ_DSP].resume_qos = -1;
	dsp_leave();
}

void dsp_pm_suspend(struct dsp_pm *pm)
{
	dsp_enter();
	pm->devfreq[DSP_DEVFREQ_DNC].resume_qos =
		pm->devfreq[DSP_DEVFREQ_DNC].current_qos;
	pm->devfreq[DSP_DEVFREQ_DSP].resume_qos =
		pm->devfreq[DSP_DEVFREQ_DSP].current_qos;
	dsp_pm_update_devfreq_min(pm);
	dsp_leave();
}

static int __dsp_pm_set_default_devfreq(struct dsp_pm_devfreq *devfreq, int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_pm_check_valid(devfreq, val);
	if (ret)
		goto p_err;

	dsp_info("devfreq[%s] default value is set to L%d\n",
			devfreq->name, val);
	devfreq->default_qos = val;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_set_default_devfreq_nolock(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (dsp_pm_devfreq_active(pm)) {
		ret = -EBUSY;
		dsp_warn("dsp is already activated\n");
		goto p_err;
	}

	if (id < 0 || id >= DSP_DEVFREQ_COUNT) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_pm_set_default_devfreq(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_pm_set_default_devfreq(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	mutex_lock(&pm->lock);

	ret = dsp_pm_set_default_devfreq_nolock(pm, id, val);
	if (ret)
		goto p_err;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static void __dsp_pm_enable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[devfreq->default_qos]);
	devfreq->current_qos = devfreq->default_qos;
	dsp_info("devfreq[%s] is enabled(L%d)\n",
			devfreq->name, devfreq->current_qos);
	dsp_leave();
}

int dsp_pm_enable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);

	for (idx = 0; idx < DSP_DEVFREQ_COUNT; ++idx)
		__dsp_pm_enable(&pm->devfreq[idx]);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static void __dsp_pm_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	pm_qos_remove_request(&devfreq->req);
	devfreq->current_qos = -1;
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

int dsp_pm_close(struct dsp_pm *pm)
{
	dsp_enter();
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

	devfreq = kzalloc(sizeof(*devfreq) * DSP_DEVFREQ_COUNT, GFP_KERNEL);
	if (!devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_pm_devfreq\n");
		goto p_err;
	}

	snprintf(devfreq[DSP_DEVFREQ_DNC].name, DSP_DEVFREQ_NAME_LEN, "dnc");
	devfreq[DSP_DEVFREQ_DNC].count = sizeof(dnc_table) / sizeof(*dnc_table);
	devfreq[DSP_DEVFREQ_DNC].table = dnc_table;
	devfreq[DSP_DEVFREQ_DNC].class_id = PM_QOS_DNC_THROUGHPUT;
	devfreq[DSP_DEVFREQ_DNC].default_qos = 0;
	devfreq[DSP_DEVFREQ_DNC].resume_qos = -1;
	devfreq[DSP_DEVFREQ_DNC].current_qos = -1;

	snprintf(devfreq[DSP_DEVFREQ_DSP].name, DSP_DEVFREQ_NAME_LEN, "dsp");
	devfreq[DSP_DEVFREQ_DSP].count = sizeof(dsp_table) / sizeof(*dsp_table);
	devfreq[DSP_DEVFREQ_DSP].table = dsp_table;
	devfreq[DSP_DEVFREQ_DSP].class_id = PM_QOS_DSP_THROUGHPUT;
	devfreq[DSP_DEVFREQ_DSP].default_qos = 0;
	devfreq[DSP_DEVFREQ_DSP].resume_qos = -1;
	devfreq[DSP_DEVFREQ_DSP].current_qos = -1;

	pm->devfreq = devfreq;
	pm_runtime_enable(sys->dev);
	mutex_init(&pm->lock);

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
