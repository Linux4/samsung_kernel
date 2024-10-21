// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpuppm.c
 * @brief   Performance power management for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/mutex.h>

#include <gpufreq_v2.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_history_common.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static void __gpuppm_sort_limit(void);
static int __gpuppm_limit_effective(enum gpufreq_target target);
static int __gpuppm_convert_limit_to_idx(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, int *ceiling_idx, int *floor_idx);
static void __gpuppm_update_gpuppm_info(void);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static DEFINE_MUTEX(gpuppm_lock);
static struct gpuppm_status g_ppm;
static unsigned int g_gpueb_support;
static unsigned int g_stress_test;
static unsigned int g_stress_direction;
static int g_stress_oppidx;
static struct gpufreq_shared_status *g_shared_status;

static struct gpuppm_limit_info g_limit_table[] = {
	LIMITOP(LIMIT_SEGMENT, "SEGMENT", GPUPPM_PRIO_9,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_DEBUG, "DEBUG", GPUPPM_PRIO_8,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_GPM3, "GPM_3.0", GPUPPM_PRIO_7,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_PEAK_POWER, "PEAK_POWER", GPUPPM_PRIO_7,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_AP, "THERMAL_AP", GPUPPM_PRIO_6,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_EB, "THERMAL_EB", GPUPPM_PRIO_6,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_SRAMRC, "SRAMRC", GPUPPM_PRIO_5,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_BATT_OC, "BATT_OC", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_BATT_PERCENT, "BATT_PERCENT", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_LOW_BATT, "LOW_BATT", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_PBM, "PBM", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_APIBOOST, "APIBOOST", GPUPPM_PRIO_3,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_FPSGO, "FPSGO", GPUPPM_PRIO_2,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
};

static struct gpuppm_platform_fp platform_ap_fp = {
	.set_shared_status = gpuppm_set_shared_status,
	.limited_commit = gpuppm_limited_commit,
	.limited_dual_commit = gpuppm_limited_dual_commit,
	.set_limit = gpuppm_set_limit,
	.switch_limit = gpuppm_switch_limit,
	.get_ceiling = gpuppm_get_ceiling,
	.get_floor = gpuppm_get_floor,
	.get_c_limiter = gpuppm_get_c_limiter,
	.get_f_limiter = gpuppm_get_f_limiter,
};

static struct gpuppm_platform_fp platform_eb_fp = {};

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
static void __gpuppm_sort_limit(void)
{
	struct gpuppm_limit_info *limit_table = NULL;
	int cur_ceiling = 0, cur_floor = 0;
	int max_oppidx = 0, min_oppidx = 0;
	unsigned int cur_c_limiter = LIMIT_NUM;
	unsigned int cur_f_limiter = LIMIT_NUM;
	unsigned int cur_c_priority = GPUPPM_PRIO_NONE;
	unsigned int cur_f_priority = GPUPPM_PRIO_NONE;
	int i = 0;

	limit_table = g_limit_table;
	cur_ceiling = -1;
	cur_floor = g_ppm.opp_num;
	max_oppidx = 0;
	min_oppidx = g_ppm.opp_num - 1;

	/* sort ceiling among valid limiters except SEGMENT  */
	for (i = 1; i < LIMIT_NUM; i++) {
		/* skip default value and check enable */
		if (limit_table[i].ceiling != GPUPPM_DEFAULT_IDX &&
			limit_table[i].ceiling != max_oppidx &&
			limit_table[i].c_enable == LIMIT_ENABLE) {
			/* use the largest ceiling with its limiter */
			if (limit_table[i].ceiling > cur_ceiling) {
				cur_ceiling = limit_table[i].ceiling;
				cur_c_limiter = limit_table[i].limiter;
			}
			/* use the largest priority to cover all valid limiters */
			if (limit_table[i].priority > cur_c_priority)
				cur_c_priority = limit_table[i].priority;
		}
	}
	/* sort floor among valid limiters except SEGMENT  */
	for (i = 1; i < LIMIT_NUM; i++) {
		/* skip default value and check enable */
		if (limit_table[i].floor != GPUPPM_DEFAULT_IDX &&
			limit_table[i].floor != min_oppidx &&
			limit_table[i].f_enable == LIMIT_ENABLE) {
			/* use the smallest floor with its limiter */
			if (limit_table[i].floor < cur_floor) {
				cur_floor = limit_table[i].floor;
				cur_f_limiter = limit_table[i].limiter;
			}
			/* use the largest priority to cover all valid limiters */
			if (limit_table[i].priority > cur_f_priority)
				cur_f_priority = limit_table[i].priority;
		}
	}

	/* if no valid limitation, use SEGMENT */
	if (cur_ceiling == -1) {
		cur_ceiling = limit_table[LIMIT_SEGMENT].ceiling;
		cur_c_limiter = limit_table[LIMIT_SEGMENT].limiter;
		cur_c_priority = limit_table[LIMIT_SEGMENT].priority;
	}
	if (cur_floor == g_ppm.opp_num) {
		cur_floor = limit_table[LIMIT_SEGMENT].floor;
		cur_f_limiter = limit_table[LIMIT_SEGMENT].limiter;
		cur_f_priority = limit_table[LIMIT_SEGMENT].priority;
	}

	/* if limit interval with intersection */
	if (cur_ceiling > cur_floor) {
		GPUFREQ_LOGD("exist intersection in limited interval: [%d, %d]",
			cur_ceiling, cur_floor);
		/*
		 * floor_priority > ceiling_priority: align floor
		 * floor_priority < ceiling_priority: align ceiling
		 * floor_priority = ceiling_priority: align ceiling
		 */
		if (cur_f_priority > cur_c_priority)
			cur_ceiling = cur_floor;
		else
			cur_floor = cur_ceiling;
	}

	/* only update if both limiter of ceiling/floor are found */
	if (cur_c_limiter != LIMIT_NUM && cur_f_limiter != LIMIT_NUM) {
		GPUFREQ_LOGD("[ceiling] index: %d, limiter: %s, priority: %d",
			cur_ceiling, limit_table[cur_c_limiter].name, cur_c_priority);
		GPUFREQ_LOGD("[floor] index: %d, limiter: %s, priority: %d",
			cur_floor, limit_table[cur_f_limiter].name, cur_f_priority);

		g_ppm.ceiling = cur_ceiling;
		g_ppm.c_limiter = cur_c_limiter;
		g_ppm.c_priority = cur_c_priority;
		g_ppm.floor = cur_floor;
		g_ppm.f_limiter = cur_f_limiter;
		g_ppm.f_priority = cur_f_priority;
	}
}

static int __gpuppm_limit_effective(enum gpufreq_target target)
{
	int cur_ceiling = 0, cur_floor = 0;
	int cur_oppidx_gpu = 0, cur_oppidx_stack = 0;
	int oppidx_gpu = 0, oppidx_stack = 0;
	int ret = GPUFREQ_SUCCESS;

	cur_ceiling = g_ppm.ceiling;
	cur_floor = g_ppm.floor;
	cur_oppidx_gpu = __gpufreq_get_cur_idx_gpu();
	cur_oppidx_stack = __gpufreq_get_cur_idx_stack();

	if (target == TARGET_STACK) {
		/* GPU */
		if (cur_oppidx_gpu < cur_ceiling)
			oppidx_gpu = cur_ceiling;
		else if (cur_oppidx_gpu > cur_floor)
			oppidx_gpu = cur_floor;
		else
			oppidx_gpu = cur_oppidx_gpu;
		/* STACK */
		if (cur_oppidx_stack < cur_ceiling)
			oppidx_stack = cur_ceiling;
		else if (cur_oppidx_stack > cur_floor)
			oppidx_stack = cur_floor;
		else
			oppidx_stack = cur_oppidx_stack;

		if (cur_oppidx_gpu != oppidx_gpu || cur_oppidx_stack != oppidx_stack)
			ret = __gpufreq_generic_commit_dual(oppidx_gpu, oppidx_stack, DVFS_FREE);
	} else {
		/* GPU */
		if (cur_oppidx_gpu < cur_ceiling)
			oppidx_gpu = cur_ceiling;
		else if (cur_oppidx_gpu > cur_floor)
			oppidx_gpu = cur_floor;
		else
			oppidx_gpu = cur_oppidx_gpu;
		/* commit */
		if (cur_oppidx_gpu != oppidx_gpu)
			ret = __gpufreq_generic_commit_gpu(oppidx_gpu, DVFS_FREE);
	}

	return ret;
}

static int __gpuppm_convert_limit_to_idx(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, int *ceiling_idx, int *floor_idx)
{
	int ret = GPUFREQ_SUCCESS;

	switch (limiter) {
	case LIMIT_SEGMENT:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = floor_info;
		break;
	case LIMIT_DEBUG:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = floor_info;
		break;
	case LIMIT_GPM3:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_PEAK_POWER:
		/* limit info: freq */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_fstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_fgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = ceiling_info;
		*floor_idx = floor_info;
		break;
	case LIMIT_THERMAL_AP:
		/* limit info: freq */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_fstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_fgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = GPUPPM_RESET_IDX;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_THERMAL_EB:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_SRAMRC:
		/* limit info: volt */
		*ceiling_idx = GPUPPM_KEEP_IDX;
		if (floor_info > 0) {
			if (target == TARGET_STACK)
				*floor_idx = __gpufreq_get_idx_by_vstack(
					(unsigned int)floor_info);
			else
				*floor_idx = __gpufreq_get_idx_by_vgpu(
					(unsigned int)floor_info);
		} else
			*floor_idx = GPUPPM_RESET_IDX;
		break;
	case LIMIT_BATT_OC:
		/* limit info: freq */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_fstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_fgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = ceiling_info;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_BATT_PERCENT:
		/* limit info: freq */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_fstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_fgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = ceiling_info;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_LOW_BATT:
		/* limit info: freq */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_fstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_fgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = ceiling_info;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_PBM:
		/* limit info: power */
		if (ceiling_info > 0) {
			if (target == TARGET_STACK)
				*ceiling_idx = __gpufreq_get_idx_by_pstack(
					(unsigned int)ceiling_info);
			else
				*ceiling_idx = __gpufreq_get_idx_by_pgpu(
					(unsigned int)ceiling_info);
		} else
			*ceiling_idx = GPUPPM_RESET_IDX;
		*floor_idx = GPUPPM_KEEP_IDX;
		break;
	case LIMIT_APIBOOST:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = floor_info;
		break;
	case LIMIT_FPSGO:
		/* limit info: OPP index */
		*ceiling_idx = ceiling_info;
		*floor_idx = floor_info;
		break;
	default:
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		break;
	}

	GPUFREQ_LOGD("[%s limiter: %d] ceiling_info: %d (idx: %d), floor_info: %d, (idx: %d)",
		(target == TARGET_STACK) ? "STACK" : "GPU",
		limiter, ceiling_info, *ceiling_idx, floor_info, *floor_idx);

	return ret;
}

static void __gpuppm_update_gpuppm_info(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* update current status to shared memory */
	g_shared_status->cur_ceiling = g_ppm.ceiling;
	g_shared_status->cur_floor = g_ppm.floor;
	g_shared_status->cur_c_limiter = g_ppm.c_limiter;
	g_shared_status->cur_f_limiter = g_ppm.f_limiter;
	g_shared_status->cur_c_priority = g_ppm.c_priority;
	g_shared_status->cur_f_priority = g_ppm.f_priority;

	copy_size = sizeof(struct gpuppm_limit_info) * LIMIT_NUM;
	memcpy(g_shared_status->limit_table, g_limit_table, copy_size);
}

void gpuppm_set_stress_test(unsigned int val)
{
	g_stress_test = val;
	g_stress_oppidx = 0;

	GPUFREQ_LOGD("stress test mode: %d", g_stress_test);

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->stress_test = g_stress_test;
}

int gpuppm_get_ceiling(void)
{
	return g_ppm.ceiling;
}

int gpuppm_get_floor(void)
{
	return g_ppm.floor;
}

unsigned int gpuppm_get_c_limiter(void)
{
	return g_ppm.c_limiter;
}

unsigned int gpuppm_get_f_limiter(void)
{
	return g_ppm.f_limiter;
}

int gpuppm_set_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, unsigned int instant_dvfs)
{
	struct gpuppm_limit_info *limit_table = NULL;
	int opp_num = 0, ceiling_idx = 0, floor_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, limiter=%d, ceiling_info=%d, floor_info=%d",
		target, limiter, ceiling_info, floor_info);

	if (limiter < 0 || limiter >= LIMIT_NUM) {
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	limit_table = g_limit_table;
	opp_num = g_ppm.opp_num;

	mutex_lock(&gpuppm_lock);

	/* convert input limit info to OPP index */
	ret = __gpuppm_convert_limit_to_idx(target, limiter,
		ceiling_info, floor_info, &ceiling_idx, &floor_idx);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to convert limit info to OPP index (%d)", ret);
		goto done;
	}

	if (ceiling_idx == GPUPPM_RESET_IDX || ceiling_idx == GPUPPM_DEFAULT_IDX)
		limit_table[limiter].ceiling = GPUPPM_DEFAULT_IDX;
	else if (ceiling_idx >= 0 && ceiling_idx < opp_num)
		limit_table[limiter].ceiling = ceiling_idx;

	if (floor_idx == GPUPPM_RESET_IDX || floor_idx == GPUPPM_DEFAULT_IDX)
		limit_table[limiter].floor = GPUPPM_DEFAULT_IDX;
	else if (floor_idx >= 0 && floor_idx < opp_num)
		limit_table[limiter].floor = floor_idx;

	/* update current limit status */
	__gpuppm_sort_limit();

	/* update current status to shared memory */
	if (g_shared_status)
		__gpuppm_update_gpuppm_info();

	/* update current OPP if necessary */
	if (instant_dvfs)
		ret = __gpuppm_limit_effective(target);

	mutex_unlock(&gpuppm_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_switch_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int c_enable, int f_enable, unsigned int instant_dvfs)
{
	struct gpuppm_limit_info *limit_table = NULL;
	int ret = GPUFREQ_SUCCESS;

	if (limiter < 0 || limiter >= LIMIT_NUM) {
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	limit_table = g_limit_table;

	mutex_lock(&gpuppm_lock);

	if (c_enable == LIMIT_ENABLE)
		limit_table[limiter].c_enable = LIMIT_ENABLE;
	else if (c_enable == LIMIT_DISABLE)
		limit_table[limiter].c_enable = LIMIT_DISABLE;

	if (f_enable == LIMIT_ENABLE)
		limit_table[limiter].f_enable = LIMIT_ENABLE;
	else if (f_enable == LIMIT_DISABLE)
		limit_table[limiter].f_enable = LIMIT_DISABLE;

	/* update current limit status */
	__gpuppm_sort_limit();

	/* update current status to shared memory */
	if (g_shared_status)
		__gpuppm_update_gpuppm_info();

	/* update current OPP if necessary */
	if (instant_dvfs)
		ret = __gpuppm_limit_effective(target);

	mutex_unlock(&gpuppm_lock);

done:
	return ret;
}

int gpuppm_limited_commit(enum gpufreq_target target, int oppidx)
{
	int limited_idx = 0;
	int cur_ceiling = 0, cur_floor = 0, min_oppidx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, oppidx=%d", target, oppidx);

	mutex_lock(&gpuppm_lock);

	/* fit to limited interval */
	cur_ceiling = g_ppm.ceiling;
	cur_floor = g_ppm.floor;
	min_oppidx = g_ppm.opp_num - 1;

	/* replace target OPP index to do stress test */
	if (g_stress_test == STRESS_RANDOM) {
		/* random */
		get_random_bytes(&oppidx, sizeof(oppidx));
		oppidx = oppidx < 0 ? (oppidx * -1) : oppidx;
		oppidx = (oppidx % (cur_floor - cur_ceiling + 1)) + cur_ceiling;
	} else if (g_stress_test == STRESS_TRAVERSE) {
		/* 0 <-> 1 <-> ... <-> opp_num-2 <-> opp_num-1 */
		oppidx = g_stress_oppidx;
		oppidx = (oppidx % (cur_floor - cur_ceiling + 1)) + cur_ceiling;
		if (oppidx == cur_ceiling)
			g_stress_direction = SCALE_UP;
		else if (oppidx == cur_floor)
			g_stress_direction = SCALE_DOWN;
		if (g_stress_direction == SCALE_UP)
			g_stress_oppidx++;
		else if (g_stress_direction == SCALE_DOWN)
			g_stress_oppidx--;
	} else if (g_stress_test == STRESS_MAX_MIN) {
		/* 0 <-> opp_num-1 */
		oppidx = g_stress_oppidx;
		g_stress_oppidx = g_stress_oppidx ? 0 : min_oppidx;
	}

	if (oppidx < cur_ceiling)
		limited_idx = cur_ceiling;
	else if (oppidx > cur_floor)
		limited_idx = cur_floor;
	else
		limited_idx = oppidx;

	GPUFREQ_LOGD("restrict %s OPP index: (%d->%d), limited interval: [%d, %d]",
		(target == TARGET_STACK) ? "STACK" : "GPU",
		oppidx, limited_idx, cur_ceiling, cur_floor);

#if GPUFREQ_HISTORY_ENABLE
	gpufreq_set_history_target_opp(target, oppidx);
#endif /* GPUFREQ_HISTORY_ENABLE */

	if (target == TARGET_STACK)
		ret = __gpufreq_generic_commit_stack(limited_idx, DVFS_FREE);
	else
		ret = __gpufreq_generic_commit_gpu(limited_idx, DVFS_FREE);

	mutex_unlock(&gpuppm_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_limited_dual_commit(int gpu_oppidx, int stack_oppidx)
{
	int limited_idx_gpu = 0, limited_idx_stack = 0;
	int cur_ceiling = 0, cur_floor = 0, min_oppidx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("gpu_oppidx=%d, stack_oppidx=%d", gpu_oppidx, stack_oppidx);

	mutex_lock(&gpuppm_lock);

	/* fit to limited interval */
	cur_ceiling = g_ppm.ceiling;
	cur_floor = g_ppm.floor;
	min_oppidx = g_ppm.opp_num - 1;

	/* replace target OPP index to do stress test */
	if (g_stress_test == STRESS_RANDOM) {
		/* random */
		get_random_bytes(&gpu_oppidx, sizeof(gpu_oppidx));
		gpu_oppidx = gpu_oppidx < 0 ? (gpu_oppidx * -1) : gpu_oppidx;
		gpu_oppidx = (gpu_oppidx % (cur_floor - cur_ceiling + 1)) + cur_ceiling;
		stack_oppidx = gpu_oppidx;
	} else if (g_stress_test == STRESS_TRAVERSE) {
		/* 0 <-> 1 <-> ... <-> opp_num-2 <-> opp_num-1 */
		gpu_oppidx = g_stress_oppidx;
		gpu_oppidx = (gpu_oppidx % (cur_floor - cur_ceiling + 1)) + cur_ceiling;
		stack_oppidx = gpu_oppidx;
		if (gpu_oppidx == cur_ceiling)
			g_stress_direction = SCALE_UP;
		else if (gpu_oppidx == cur_floor)
			g_stress_direction = SCALE_DOWN;
		if (g_stress_direction == SCALE_UP)
			g_stress_oppidx++;
		else if (g_stress_direction == SCALE_DOWN)
			g_stress_oppidx--;
	} else if (g_stress_test == STRESS_MAX_MIN) {
		/* 0 <-> opp_num-1 */
		gpu_oppidx = g_stress_oppidx;
		stack_oppidx = gpu_oppidx;
		g_stress_oppidx = g_stress_oppidx ? 0 : min_oppidx;
	}

	/* GPU */
	if (gpu_oppidx < cur_ceiling)
		limited_idx_gpu = cur_ceiling;
	else if (gpu_oppidx > cur_floor)
		limited_idx_gpu = cur_floor;
	else
		limited_idx_gpu = gpu_oppidx;
	/* STACK */
	if (stack_oppidx < cur_ceiling)
		limited_idx_stack = cur_ceiling;
	else if (stack_oppidx > cur_floor)
		limited_idx_stack = cur_floor;
	else
		limited_idx_stack = stack_oppidx;

	GPUFREQ_LOGD("restrict GPU/STACK OPP index: (%d->%d)/(%d->%d), limited interval: [%d, %d]",
		gpu_oppidx, limited_idx_gpu, stack_oppidx, limited_idx_stack,
		cur_ceiling, cur_floor);

#if GPUFREQ_HISTORY_ENABLE
	gpufreq_set_history_target_opp(TARGET_GPU, gpu_oppidx);
	gpufreq_set_history_target_opp(TARGET_STACK, stack_oppidx);
#endif /* GPUFREQ_HISTORY_ENABLE */

	ret = __gpufreq_generic_commit_dual(limited_idx_gpu, limited_idx_stack, DVFS_FREE);

	mutex_unlock(&gpuppm_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

void gpuppm_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	mutex_lock(&gpuppm_lock);

	if (shared_status)
		g_shared_status = shared_status;
	else
		__gpufreq_abort("null gpufreq shared status: 0x%llx", shared_status);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->stress_test = g_stress_test;
		__gpuppm_update_gpuppm_info();
	}

	mutex_unlock(&gpuppm_lock);
}

int gpuppm_init(enum gpufreq_target target, unsigned int gpueb_support)
{
	int max_oppidx = 0, min_oppidx = 0, opp_num = 0;
	int ret = GPUFREQ_SUCCESS;

	g_gpueb_support = gpueb_support;

	if (g_gpueb_support)
		gpufreq_register_gpuppm_fp(&platform_eb_fp);
	/* init only in AP mode */
	else {
		if (target == TARGET_STACK)
			opp_num = __gpufreq_get_opp_num_stack();
		else
			opp_num = __gpufreq_get_opp_num_gpu();

		max_oppidx = 0;
		min_oppidx = opp_num - 1;
		g_ppm.opp_num = opp_num;

		/* set basic limit at boot time */
		gpuppm_set_limit(target, LIMIT_SEGMENT, max_oppidx, min_oppidx, false);

		gpufreq_register_gpuppm_fp(&platform_ap_fp);
	}

	return ret;
}
