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

#define DSP_DEVFREQ_NAME_LEN		(10)
#define DSP_DEVFREQ_RESERVED_COUNT	(16)

struct dsp_system;

struct dsp_pm_devfreq {
	char			name[DSP_DEVFREQ_NAME_LEN];
	struct pm_qos_request	req;
	int			count;
	unsigned int		*table;
	int			class_id;
	int			boot_qos;
	int			dynamic_qos;
	unsigned int		dynamic_total_count;
	unsigned int		dynamic_count[DSP_DEVFREQ_RESERVED_COUNT];
	int			static_qos;
	unsigned int		static_total_count;
	unsigned int		static_count[DSP_DEVFREQ_RESERVED_COUNT];
	int			current_qos;
	int			force_qos;
	int			min_qos;
};

int dsp_pm_update_devfreq_nolock(struct dsp_pm *pm, int id, int val);
int dsp_pm_set_force_qos(struct dsp_pm *pm, int id, int val);

int dsp_pm_dvfs_enable(struct dsp_pm *pm, int val);
int dsp_pm_dvfs_disable(struct dsp_pm *pm, int val);

int dsp_pm_devfreq_active(struct dsp_pm *pm);
int dsp_pm_update_devfreq_busy(struct dsp_pm *pm, int val);
int dsp_pm_update_devfreq_idle(struct dsp_pm *pm, int val);
int dsp_pm_update_devfreq_boot(struct dsp_pm *pm);
int dsp_pm_update_devfreq_max(struct dsp_pm *pm);
int dsp_pm_update_devfreq_min(struct dsp_pm *pm);
int dsp_pm_set_boot_qos(struct dsp_pm *pm, int val);

int dsp_pm_boost_enable(struct dsp_pm *pm);
int dsp_pm_boost_disable(struct dsp_pm *pm);

int dsp_pm_enable(struct dsp_pm *pm);
int dsp_pm_disable(struct dsp_pm *pm);

int dsp_pm_open(struct dsp_pm *pm);
int dsp_pm_close(struct dsp_pm *pm);
int dsp_pm_probe(struct dsp_system *sys);
void dsp_pm_remove(struct dsp_pm *pm);

#endif
