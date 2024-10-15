// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "pablo-dvfs.h"
#include "pablo-debug.h"

#ifdef CONFIG_PM_DEVFREQ
static struct is_pm_qos_request pablo_exynos_pm_qos[IS_DVFS_END];
static struct is_dvfs_dt_t *pablo_dvfs_dt_arr;
static hw_dvfs_lv_getter_t hw_dvfs_lv_getter;
static pablo_clock_change_cb clock_change_cb[IS_DVFS_END];

static struct is_pm_qos_ops default_pm_qos_req_ops = {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	.add_request = is_pm_qos_add_request,
	.update_request = is_pm_qos_update_request,
	.remove_request = is_pm_qos_remove_request,
	.request_active = is_pm_qos_request_active,
#endif
};

static struct is_emstune_ops default_emstune_ops = {
	.add_request = pkv_emstune_add_request,
	.remove_request = pkv_emstune_remove_request,
	.boost = pkv_emstune_boost,
};

static struct is_dvfs_ext_func default_dvfs_ext_func = {
	.pm_qos_ops = &default_pm_qos_req_ops,
	.emstune_ops = &default_emstune_ops,
};

static struct is_dvfs_ext_func pablo_dvfs_ext_func;

static int is_hw_dvfs_init(struct is_dvfs_ctrl *dvfs_ctrl, struct is_dvfs_dt_t *dvfs_dt_arr)
{
	int i, ret = 0;

	dvfs_ctrl->static_ctrl->cur_scenario_id = 0;
	dvfs_ctrl->dynamic_ctrl->cur_scenario_id = 0;
	dvfs_ctrl->dynamic_ctrl->cur_frame_tick = -1;

	if (!dvfs_ctrl->dvfs_info.scenario_count) {
		warn("[DVFS] There is NO DVFS scenario.");
		return 0;
	}

	if (!dvfs_ctrl->scenario_idx) {
		dvfs_ctrl->scenario_idx =
			kvzalloc(sizeof(int) * dvfs_ctrl->dvfs_info.scenario_count, GFP_KERNEL);

		if (ZERO_OR_NULL_PTR(dvfs_ctrl->scenario_idx)) {
			err("[DVFS] dvfs_ctrl alloc is failed!!");
			return -ENOMEM;
		}
	}

	pablo_dvfs_dt_arr = dvfs_dt_arr;
	for (i = 0; i < dvfs_ctrl->dvfs_info.scenario_count; i++)
		dvfs_ctrl->scenario_idx[pablo_dvfs_dt_arr[i].scenario_id] = i;

	return ret;
}

static char *is_hw_dvfs_get_scenario_name(struct is_dvfs_ctrl *dvfs_ctrl, int id)
{
	return (char *)pablo_dvfs_dt_arr[dvfs_ctrl->scenario_idx[id]].parse_scenario_nm;
}

static int is_hw_dvfs_get_frame_tick(struct is_dvfs_ctrl *dvfs_ctrl, int id)
{
	return pablo_dvfs_dt_arr[dvfs_ctrl->scenario_idx[id]].keep_frame_tick;
}

static void is_dvfs_dec_dwork_fn(struct work_struct *data)
{
	struct is_dvfs_ctrl *dvfs_ctrl = container_of((void *)data, struct is_dvfs_ctrl, dec_dwork);

	mutex_lock(&dvfs_ctrl->lock);

	/* perform an internal dvfs setting for gradual CSIS level down control in otf path */
	is_set_dvfs_otf(dvfs_ctrl, dvfs_ctrl->static_ctrl->cur_scenario_id);

	mutex_unlock(&dvfs_ctrl->lock);
}

int is_dvfs_set_ext_func(struct is_dvfs_ext_func *ext_func)
{
	FIMC_BUG(!ext_func);

	if (ext_func->pm_qos_ops) {
		pablo_dvfs_ext_func.pm_qos_ops = ext_func->pm_qos_ops;
		if (IS_ENABLED(CONFIG_EXYNOS_PM_QOS)) {
			FIMC_BUG(!pablo_dvfs_ext_func.pm_qos_ops->add_request);
			FIMC_BUG(!pablo_dvfs_ext_func.pm_qos_ops->update_request);
			FIMC_BUG(!pablo_dvfs_ext_func.pm_qos_ops->remove_request);
			FIMC_BUG(!pablo_dvfs_ext_func.pm_qos_ops->request_active);
		}
	}

	if (ext_func->emstune_ops) {
		pablo_dvfs_ext_func.emstune_ops = ext_func->emstune_ops;

		FIMC_BUG(!pablo_dvfs_ext_func.emstune_ops->add_request);
		FIMC_BUG(!pablo_dvfs_ext_func.emstune_ops->remove_request);
		FIMC_BUG(!pablo_dvfs_ext_func.emstune_ops->boost);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_set_ext_func);

int is_dvfs_init(struct is_dvfs_ctrl *dvfs_ctrl, struct is_dvfs_dt_t *dvfs_dt_arr,
		 hw_dvfs_lv_getter_t hw_dvfs_lv_get_func)
{
	int ret = 0;
	u32 dvfs_t;

	FIMC_BUG(!dvfs_ctrl);
	FIMC_BUG(!dvfs_dt_arr);
	FIMC_BUG(!hw_dvfs_lv_get_func);

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++)
		dvfs_ctrl->cur_lv[dvfs_t] = IS_DVFS_LV_END;

	dvfs_ctrl->cur_hpg_qos = 0;
	dvfs_ctrl->cur_hmp_bst = 0;
	dvfs_ctrl->cur_cpus = NULL;

	/* init spin_lock for clock gating */
	mutex_init(&dvfs_ctrl->lock);

	if (!(dvfs_ctrl->static_ctrl))
		dvfs_ctrl->static_ctrl = kvzalloc(sizeof(struct is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(dvfs_ctrl->dynamic_ctrl))
		dvfs_ctrl->dynamic_ctrl =
			kvzalloc(sizeof(struct is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!(dvfs_ctrl->iter_mode))
		dvfs_ctrl->iter_mode = kzalloc(sizeof(struct is_dvfs_iteration_mode), GFP_KERNEL);

	if (!dvfs_ctrl->static_ctrl || !dvfs_ctrl->dynamic_ctrl || !dvfs_ctrl->iter_mode) {
		err("[DVFS] dvfs_ctrl alloc is failed!!");
		return -ENOMEM;
	}

	/* assign static / dynamic scenario check logic data */
	ret = is_hw_dvfs_init(dvfs_ctrl, dvfs_dt_arr);
	if (ret) {
		err("[DVFS] is_hw_dvfs_init is failed(%d)", ret);
		return ret;
	}

	dvfs_ctrl->dec_dtime = IS_DVFS_DEC_DTIME;
	INIT_DELAYED_WORK(&dvfs_ctrl->dec_dwork, is_dvfs_dec_dwork_fn);

	is_dvfs_set_ext_func(&default_dvfs_ext_func);
	hw_dvfs_lv_getter = hw_dvfs_lv_get_func;

	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_init);

int is_dvfs_sel_static(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	struct is_dvfs_scenario_ctrl *static_ctrl;

	FIMC_BUG(!dvfs_ctrl);

	static_ctrl = dvfs_ctrl->static_ctrl;
	FIMC_BUG(!static_ctrl);

	dbg_dvfs(1, "%s common: %d, vendor: %d\n", __func__,
		 dvfs_ctrl->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK,
		 (dvfs_ctrl->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) &
			 IS_DVFS_SCENARIO_VENDOR_MASK);

	if (!dvfs_ctrl->dvfs_info.scenario_count)
		return -EINVAL;

	if ((scenario_id < 0) ||
	    (scenario_id > 0 && scenario_id >= dvfs_ctrl->dvfs_info.scenario_count))
		scenario_id = IS_SCENARIO_DEFAULT;

	static_ctrl->cur_scenario_id = scenario_id;
	static_ctrl->cur_frame_tick = -1;
	static_ctrl->scenario_nm = is_hw_dvfs_get_scenario_name(dvfs_ctrl, scenario_id);

	return scenario_id;
}
EXPORT_SYMBOL_GPL(is_dvfs_sel_static);

static bool _is_dvfs_dynamic(struct is_dvfs_rnr_mode *rnr_mode, int vendor, bool reprocessing_mode)
{
	if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
		return false;

	if (reprocessing_mode)
		return true;

	if (rnr_mode->changed && rnr_mode->rnr) {
		dbg_dvfs(1, "[DVFS] recursiveNr on\n");
		rnr_mode->changed = 0;
		return true;
	}

	return false;
}

int is_dvfs_sel_dynamic(struct is_dvfs_ctrl *dvfs_ctrl, bool reprocessing_mode)
{
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct is_dvfs_rnr_mode *rnr_mode;
	int vendor;

	FIMC_BUG(!dvfs_ctrl);

	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;
	rnr_mode = &dvfs_ctrl->rnr_mode;
	FIMC_BUG(!dynamic_ctrl);

	if (dynamic_ctrl->cur_frame_tick >= 0) {
		(dynamic_ctrl->cur_frame_tick)--;
		/*
		 * when cur_frame_tick is lower than 0, clear current scenario.
		 * This means that current frame tick to keep dynamic scenario
		 * was expired.
		 */
		if (dynamic_ctrl->cur_frame_tick < 0)
			dynamic_ctrl->cur_scenario_id = -1;
	}

	vendor = (dvfs_ctrl->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) &
		 IS_DVFS_SCENARIO_VENDOR_MASK;
	if (!_is_dvfs_dynamic(rnr_mode, vendor, reprocessing_mode))
		return -EAGAIN;

	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_sel_dynamic);

bool is_dvfs_update_dynamic(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;

	FIMC_BUG(!dvfs_ctrl);

	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;
	FIMC_BUG(!dynamic_ctrl);

	dynamic_ctrl->cur_frame_tick = is_hw_dvfs_get_frame_tick(dvfs_ctrl, scenario_id);
	dynamic_ctrl->scenario_nm = is_hw_dvfs_get_scenario_name(dvfs_ctrl, scenario_id);

	if (dynamic_ctrl->cur_scenario_id != scenario_id)
		dynamic_ctrl->cur_scenario_id = scenario_id;
	else
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(is_dvfs_update_dynamic);

int is_dvfs_get_freq(struct is_dvfs_ctrl *dvfs_ctrl, u32 *qos_freq)
{
	int dvfs_t, cur_lv;

	FIMC_BUG(!dvfs_ctrl);
	FIMC_BUG(!qos_freq);

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++) {
		cur_lv = dvfs_ctrl->cur_lv[dvfs_t];
		if (cur_lv < IS_DVFS_LV_END) {
			qos_freq[dvfs_t] =
				dvfs_ctrl->dvfs_info.qos_tb[dvfs_t][cur_lv][IS_DVFS_VAL_FRQ];
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_get_freq);

static inline int is_get_qos_lv(struct is_dvfs_ctrl *dvfs_ctrl, u32 type, u32 scenario_id)
{
	if (type >= IS_DVFS_END) {
		err("[DVFS] Cannot find DVFS value");
		return -EINVAL;
	}

	if (scenario_id >= dvfs_ctrl->dvfs_info.scenario_count) {
		err("[DVFS] invalid scenario_id(%d)", scenario_id);
		return -EINVAL;
	}

	FIMC_BUG(!dvfs_ctrl->dvfs_info.dvfs_data);

	return dvfs_ctrl->dvfs_info.dvfs_data[scenario_id][type];
}

static inline u32 *is_get_qos_table(struct is_dvfs_ctrl *dvfs_ctrl, u32 type, u32 scenario_id)
{
	int lv;
	u32 *qos_tb;
	const char *name;

	name = dvfs_ctrl->dvfs_info.qos_name[type];
	if (!name)
		return NULL;

	lv = is_get_qos_lv(dvfs_ctrl, type, scenario_id);
	if (lv < 0) {
		err("[DVFS] fail is_get_qos_lv()");
		return NULL;
	}

	if (lv >= IS_DVFS_LV_END)
		return NULL;

	qos_tb = dvfs_ctrl->dvfs_info.qos_tb[type][lv];

	pr_cont("[%s(%d:%d)]", name, lv, qos_tb[IS_DVFS_VAL_FRQ]);

	return qos_tb;
}

static inline const char *is_get_cpus(struct is_dvfs_ctrl *dvfs_ctrl, u32 scenario_id)
{
	if (scenario_id >= dvfs_ctrl->dvfs_info.scenario_count) {
		err("invalid scenario_id(%d)", scenario_id);
		return NULL;
	}

	return dvfs_ctrl->dvfs_info.dvfs_cpu[scenario_id];
}

static inline void _is_add_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, u32 qos_thput,
				u32 dvfs_t)
{
	u32 *qos_tb;
	u32 qos, lv;

	qos_tb = is_get_qos_table(dvfs_ctrl, dvfs_t, scenario_id);
	if (!qos_tb)
		return;

	qos = qos_tb[IS_DVFS_VAL_QOS];
	lv = qos_tb[IS_DVFS_VAL_LEV];
	if (qos > 0 &&
	    !pablo_dvfs_ext_func.pm_qos_ops->request_active(&pablo_exynos_pm_qos[dvfs_t])) {
		pablo_dvfs_ext_func.pm_qos_ops->add_request(&pablo_exynos_pm_qos[dvfs_t], qos_thput,
							    qos);
		dvfs_ctrl->cur_lv[dvfs_t] = lv;
	}
}

static inline void _is_remove_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, u32 dvfs_t)
{
	u32 *qos_tb;
	u32 qos;

	qos_tb = is_get_qos_table(dvfs_ctrl, dvfs_t, scenario_id);
	if (!qos_tb)
		return;

	qos = qos_tb[IS_DVFS_VAL_QOS];
	if (qos > 0)
		pablo_dvfs_ext_func.pm_qos_ops->remove_request(&pablo_exynos_pm_qos[dvfs_t]);
}

static inline void _is_set_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, u32 *qos_tb, u32 dvfs_t)
{
	u32 qos, lv;
	pablo_clock_change_cb callback;

	if (!qos_tb)
		return;

	qos = qos_tb[IS_DVFS_VAL_QOS];
	lv = qos_tb[IS_DVFS_VAL_LEV];

	if (qos && dvfs_ctrl->cur_lv[dvfs_t] != lv) {
		pablo_dvfs_ext_func.pm_qos_ops->update_request(&pablo_exynos_pm_qos[dvfs_t], qos);
		dvfs_ctrl->cur_lv[dvfs_t] = lv;
	}

	callback = clock_change_cb[dvfs_t];

	if (callback)
		callback(qos_tb[IS_DVFS_VAL_FRQ]);
}

int is_add_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, u32 *qos_thput)
{
	u32 dvfs_t;

	info("[RSC] %s", __func__);
	FIMC_BUG(!dvfs_ctrl);
	FIMC_BUG(!qos_thput);

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++)
		_is_add_dvfs(dvfs_ctrl, scenario_id, qos_thput[dvfs_t], dvfs_t);

	return 0;
}
EXPORT_SYMBOL_GPL(is_add_dvfs);

int is_remove_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	u32 dvfs_t;

	info("[RSC] %s", __func__);
	FIMC_BUG(!dvfs_ctrl);

	cancel_delayed_work_sync(&dvfs_ctrl->dec_dwork);

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++)
		_is_remove_dvfs(dvfs_ctrl, scenario_id, dvfs_t);

	return 0;
}
EXPORT_SYMBOL_GPL(is_remove_dvfs);

static bool _is_get_enabled_thrott_domain(struct is_dvfs_ctrl *dvfs_ctrl, u32 dvfs_t)
{
	if (test_bit(IS_DVFS_TICK_THROTT, &dvfs_ctrl->state)) {
		if (dvfs_t == IS_DVFS_INT_CAM && IS_ENABLED(CONFIG_THROTTLING_INTCAM_ENABLE))
			return true;
		if (dvfs_t == IS_DVFS_MIF && IS_ENABLED(CONFIG_THROTTLING_MIF_ENABLE))
			return true;
		if (dvfs_t == IS_DVFS_INT && IS_ENABLED(CONFIG_THROTTLING_INT_ENABLE))
			return true;
	}

	return false;
}

static u32 *_is_dvfs_iter_calc_qos(u32 qos_dt_tb[][IS_DVFS_LV_END][IS_DVFS_VAL_END], u32 *qos_tb,
				   u32 type)
{
	u32 lv, n_lv = 0, n_freq, clk;
	u32 *n_qos_tb;

	switch (type) {
	case IS_DVFS_CAM:
	case IS_DVFS_ISP:
	case IS_DVFS_INT_CAM:
	case IS_DVFS_MIF:
		n_freq = qos_tb[IS_DVFS_VAL_FRQ] * 2;

		for (lv = 0; lv < IS_DVFS_LV_END; lv++) {
			clk = qos_dt_tb[type][lv][IS_DVFS_VAL_FRQ];

			if (clk >= n_freq)
				n_lv = lv;
			else
				break;
		}

		pr_cont("->[(%d:%d)]", n_lv, qos_dt_tb[type][n_lv][IS_DVFS_VAL_FRQ]);
		n_qos_tb = qos_dt_tb[type][n_lv];
		break;
	default:
		n_qos_tb = qos_tb;
		break;
	}

	return n_qos_tb;
}

static int is_set_dvfs_by_path(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, bool otf_path)
{
	struct is_sysfs_debug *sysfs_debug = is_get_sysfs_debug();
	u32 dvfs_t;
	u32 *qos_tb[IS_DVFS_END];
	u32 lv;
	int t_scenario_id;

	if (!IS_ENABLED(ENABLE_DVFS) || !sysfs_debug->en_dvfs)
		return -EINVAL;

	if (pablo_dvfs_ext_func.pm_qos_ops->request_active(&dvfs_ctrl->user_qos))
		return -EINVAL;

	memset(qos_tb, 0, sizeof(qos_tb));

	info("[DVFS] %s", __func__);

	/* General PM QOS */
	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++) {
		if (dvfs_ctrl->dvfs_info.qos_otf[dvfs_t] != otf_path)
			continue;

		if (!dvfs_ctrl->dvfs_info.qos_name[dvfs_t])
			continue;

		/* when thrott_ctrl is not OFF, the value of thrott_ctrl is thrott scenario id */
		t_scenario_id = scenario_id;
		if (dvfs_ctrl->thrott_ctrl && _is_get_enabled_thrott_domain(dvfs_ctrl, dvfs_t))
			t_scenario_id = dvfs_ctrl->thrott_ctrl;

		qos_tb[dvfs_t] = is_get_qos_table(dvfs_ctrl, dvfs_t, t_scenario_id);
		if (!qos_tb[dvfs_t]) {
			lv = hw_dvfs_lv_getter(dvfs_ctrl, dvfs_t);

			if (otf_path && lv > dvfs_ctrl->cur_lv[dvfs_t] + 1) {
				lv = dvfs_ctrl->cur_lv[dvfs_t] + 1;
				schedule_delayed_work(&dvfs_ctrl->dec_dwork,
						      msecs_to_jiffies(dvfs_ctrl->dec_dtime));
			}

			if (lv < IS_DVFS_LV_END) {
				qos_tb[dvfs_t] = dvfs_ctrl->dvfs_info.qos_tb[dvfs_t][lv];
				pr_cont("[%s(%d:%d)]", dvfs_ctrl->dvfs_info.qos_name[dvfs_t], lv,
					qos_tb[dvfs_t][IS_DVFS_VAL_FRQ]);
			}
		}

		if (dvfs_ctrl->iter_mode->iter_svhist && otf_path == false)
			qos_tb[dvfs_t] = _is_dvfs_iter_calc_qos(dvfs_ctrl->dvfs_info.qos_tb,
								qos_tb[dvfs_t], dvfs_t);
	}

	for (dvfs_t = 0; dvfs_t < IS_DVFS_END; dvfs_t++)
		_is_set_dvfs(dvfs_ctrl, qos_tb[dvfs_t], dvfs_t);

	return 0;
}

static void _is_dvfs_update_emstune(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	int hpg_qos = is_get_qos_lv(dvfs_ctrl, IS_DVFS_HPG, scenario_id);
	/* hpg_qos : number of minimum online CPU */
	if (hpg_qos > 0 && (dvfs_ctrl->cur_hpg_qos != hpg_qos)) {
		dvfs_ctrl->cur_hpg_qos = hpg_qos;

		/* for migration to big core */
		if (hpg_qos > 4) {
			if (!dvfs_ctrl->cur_hmp_bst) {
				info("[DVFS] BOOST on\n");
				pablo_dvfs_ext_func.emstune_ops->boost(1);
				dvfs_ctrl->cur_hmp_bst = 1;
			}
		} else {
			if (dvfs_ctrl->cur_hmp_bst) {
				info("[DVFS] BOOST off\n");
				pablo_dvfs_ext_func.emstune_ops->boost(0);
				dvfs_ctrl->cur_hmp_bst = 0;
			}
		}
	}
}

int is_set_dvfs_otf(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	return is_set_dvfs_by_path(dvfs_ctrl, scenario_id, true);
}
EXPORT_SYMBOL_GPL(is_set_dvfs_otf);

int is_set_dvfs_m2m(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, bool reprocessing_mode)
{
	int hpg_qos;
	const char *cpus;

	is_set_dvfs_by_path(dvfs_ctrl, scenario_id, false);

	/* Others */
	hpg_qos = is_get_qos_lv(dvfs_ctrl, IS_DVFS_HPG, scenario_id);
	if (!reprocessing_mode)
		_is_dvfs_update_emstune(dvfs_ctrl, scenario_id);

	cpus = is_get_cpus(dvfs_ctrl, dvfs_ctrl->static_ctrl->cur_scenario_id);
	if (cpus && (!dvfs_ctrl->cur_cpus || strcmp(dvfs_ctrl->cur_cpus, cpus)))
		dvfs_ctrl->cur_cpus = cpus;

	info("[DVFS] [HPG(%d, %d),CPU(%s),THROTT_CTRL(%d)]\n", hpg_qos, dvfs_ctrl->cur_hmp_bst,
	     cpus ? cpus : "", dvfs_ctrl->thrott_ctrl);

	return 0;
}
EXPORT_SYMBOL_GPL(is_set_dvfs_m2m);

int pablo_set_static_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, const char *suffix, int scenario_id,
			  int path, bool reprocessing_mode)
{
	int scn;

	mutex_lock(&dvfs_ctrl->lock);

	scn = is_dvfs_sel_static(dvfs_ctrl, scenario_id);
	if (scn >= IS_SCENARIO_DEFAULT) {
		info("[DVFS] static scenario(%d)-[%s%s]\n", scn,
		     dvfs_ctrl->static_ctrl->scenario_nm, suffix);

		if (path == IS_DVFS_PATH_OTF)
			is_set_dvfs_otf(dvfs_ctrl, scn); /* TODO: OTF scenario */
		else
			is_set_dvfs_m2m(dvfs_ctrl, scn, reprocessing_mode);
	}

	mutex_unlock(&dvfs_ctrl->lock);

	return scn;
}
EXPORT_SYMBOL_GPL(pablo_set_static_dvfs);

static int _is_set_dvfs_fast_launch(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id)
{
	int scn;

	mutex_lock(&dvfs_ctrl->lock);

	scn = is_dvfs_sel_static(dvfs_ctrl, scenario_id);
	if (scn >= IS_SCENARIO_DEFAULT) {
		info("[DVFS] static scenario(%d)-[%sFAST_LAUNCH]\n", scn,
		     dvfs_ctrl->static_ctrl->scenario_nm);
		/* In fast launch time, set default dvfs scenario */
		is_set_dvfs_m2m(dvfs_ctrl, IS_SCENARIO_DEFAULT, false);
		/* reset cur_frame_tick to restore to the detected scenario after fast launch */
		dvfs_ctrl->dynamic_ctrl->cur_frame_tick = KEEP_FRAME_TICK_FAST_LAUNCH;
	}

	mutex_unlock(&dvfs_ctrl->lock);

	return scn;
}

int is_set_static_dvfs(
	struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, bool stream_on, bool fast_launch)
{
	int scn;

	if ((scenario_id < 0) ||
	    (scenario_id > 0 && scenario_id >= dvfs_ctrl->dvfs_info.scenario_count))
		scenario_id = IS_SCENARIO_DEFAULT;

	if (stream_on) {
		if (fast_launch)
			scn = _is_set_dvfs_fast_launch(dvfs_ctrl, scenario_id);
		else
			scn = pablo_set_static_dvfs(
				dvfs_ctrl, "", scenario_id, IS_DVFS_PATH_M2M, false);
	} else {
		scn = pablo_set_static_dvfs(dvfs_ctrl, "", scenario_id, IS_DVFS_PATH_OTF, false);
	}

	return scn;
}
EXPORT_SYMBOL_GPL(is_set_static_dvfs);

unsigned int is_get_bit_count(unsigned long bits)
{
	unsigned int count = 0;

	while (bits) {
		bits &= (bits - 1);
		count++;
	}

	return count;
}
EXPORT_SYMBOL_GPL(is_get_bit_count);

int pablo_register_change_dvfs_cb(u32 dvfs_t, pablo_clock_change_cb cb)
{
	if (clock_change_cb[dvfs_t] && cb) {
		err("clock_change_cb[%d] is already registered", dvfs_t);
		return -1;
	}

	clock_change_cb[dvfs_t] = cb;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_register_change_dvfs_cb);

int is_dvfs_ems_init(void)
{
	pablo_dvfs_ext_func.emstune_ops->add_request();
	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_ems_init);

int is_dvfs_ems_reset(struct is_dvfs_ctrl *dvfs_ctrl)
{
	if (dvfs_ctrl->cur_hmp_bst)
		pablo_dvfs_ext_func.emstune_ops->boost(0);

	pablo_dvfs_ext_func.emstune_ops->remove_request();
	return 0;
}
EXPORT_SYMBOL_GPL(is_dvfs_ems_reset);
#endif
