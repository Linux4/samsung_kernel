/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_PM_H__
#define __DSP_PM_H__

#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>

#include "dsp-hw-pm.h"

#define DSP_DEVFREQ_NAME_LEN	(10)

struct dsp_system;

struct dsp_pm_devfreq {
	char			name[DSP_DEVFREQ_NAME_LEN];
	struct pm_qos_request	req;
	int			count;
	unsigned int		*table;
	int			class_id;
	int			default_qos;
	int			resume_qos;
	int			current_qos;
};

int dsp_pm_devfreq_active(struct dsp_pm *pm);
int dsp_pm_update_devfreq_nolock(struct dsp_pm *pm, int id, int val);
int dsp_pm_update_devfreq(struct dsp_pm *pm, int id, int val);
int dsp_pm_update_devfreq_max(struct dsp_pm *pm);
int dsp_pm_update_devfreq_min(struct dsp_pm *pm);
void dsp_pm_resume(struct dsp_pm *pm);
void dsp_pm_suspend(struct dsp_pm *pm);
int dsp_pm_set_default_devfreq_nolock(struct dsp_pm *pm, int id, int val);
int dsp_pm_set_default_devfreq(struct dsp_pm *pm, int id, int val);

int dsp_pm_enable(struct dsp_pm *pm);
int dsp_pm_disable(struct dsp_pm *pm);

int dsp_pm_open(struct dsp_pm *pm);
int dsp_pm_close(struct dsp_pm *pm);
int dsp_pm_probe(struct dsp_system *sys);
void dsp_pm_remove(struct dsp_pm *pm);

#endif
