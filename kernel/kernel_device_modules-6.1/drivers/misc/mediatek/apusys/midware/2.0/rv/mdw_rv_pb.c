// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_rv_tag.h"

#include "apu_power_budget.h"

/* for power budget */
struct mdw_pb_mgr {
	struct mdw_device *mdev;

	atomic_t ref[PBM_MODE_MAX];
	struct mutex mtx;

	/* put work */
	struct delayed_work put_wk[PBM_MODE_MAX];
	atomic_t put_ref[PBM_MODE_MAX];
};
static struct mdw_pb_mgr g_pb;

/* transfer mdw policy to pbm's mode */
static inline enum mdw_pwrplcy_type mdw_rv_pbmode2policy(enum pbm_mode type)
{
	if (type == PBM_MODE_PERFORMANCE)
		return MDW_POWERPOLICY_PERFORMANCE;

	return MDW_POWERPOLICY_DEFAULT;
}

/* transfer pbm's mode to mdw policy */
static inline enum pbm_mode mdw_rv_policy2pbmode(enum mdw_pwrplcy_type type)
{
	if (type == MDW_POWERPOLICY_PERFORMANCE)
		return PBM_MODE_PERFORMANCE;

	return PBM_MODE_NORMAL;
}

int mdw_rv_pb_get(enum mdw_pwrplcy_type type, uint32_t debounce_ms)
{
	int ret = 0, val = 0;
	enum pbm_mode pb_type = mdw_rv_policy2pbmode(type);

	if (pb_type >= PBM_MODE_MAX) {
		mdw_drv_warn("wrong type(%u->%d)\n", type, pb_type);
		return -EINVAL;
	}

	mutex_lock(&g_pb.mtx);

	/* 0->1, report power */
	val = atomic_inc_return(&g_pb.ref[pb_type]);
	mdw_drv_debug("pb get, type(%u->%d) debounce_ms(%u) ref(%d->%d)\n",
		type, pb_type, debounce_ms, val, val+1);

	if (val == 1) {
		mdw_drv_debug("enable apu power budget(%d)\n", pb_type);
		ret = apu_power_budget(pb_type, 1);
		if (ret)
			mdw_drv_err("enable apu power budget(%d) fail(%d)\n", pb_type, ret);
	}

	/* setup put wk if debounce */
	if (debounce_ms) {
		atomic_inc(&g_pb.put_ref[pb_type]);
		cancel_delayed_work(&g_pb.put_wk[pb_type]);
		schedule_delayed_work(&g_pb.put_wk[pb_type], msecs_to_jiffies(debounce_ms));
	}

	mutex_unlock(&g_pb.mtx);

	return ret;
}

static int mdw_rv_pb_put_cnt(enum mdw_pwrplcy_type type, uint32_t cnt)
{
	int ret = 0, val = 0;
	enum pbm_mode pb_type = mdw_rv_policy2pbmode(type);

	if (pb_type >= PBM_MODE_MAX) {
		mdw_drv_warn("wrong type(%u->%d)\n", type, pb_type);
		return -EINVAL;
	}

	mutex_lock(&g_pb.mtx);

	/* 1->0, report power */
	val = atomic_sub_return(cnt, &g_pb.ref[pb_type]);
	mdw_drv_debug("pb type(%d) ref(%d->%d) cnt(%u)\n", pb_type, val + cnt, val, cnt);

	if (val < 0) {
		mdw_exception("put pb cnt underflow, pb type(%u) ref(->%d) put_cnt(%u)\n",
			pb_type, val, cnt);
	}

	if (val == 0) {
		mdw_drv_debug("disable apu power budget(%d)\n", pb_type);
		ret = apu_power_budget(pb_type, 0);
		if (ret)
			mdw_drv_err("disable power budget(%u) fail(%d)\n", pb_type, ret);
	}

	mutex_unlock(&g_pb.mtx);

	return ret;
}

int mdw_rv_pb_put(enum mdw_pwrplcy_type type)
{
	return mdw_rv_pb_put_cnt(type, 1);
}

static void mdw_rv_pb_put_type(enum pbm_mode pb_type)
{
	int cnt = 0;

	cnt = atomic_read(&g_pb.put_ref[pb_type]);
	if (cnt == 0)
		return;

	atomic_sub(cnt, &g_pb.put_ref[pb_type]);
	mdw_rv_pb_put_cnt(mdw_rv_pbmode2policy(pb_type), cnt);
}

static void mdw_rv_pb_norm_put_func(struct work_struct *wk)
{
	mdw_rv_pb_put_type(PBM_MODE_NORMAL);
}

static void mdw_rv_pb_perf_put_func(struct work_struct *wk)
{
	mdw_rv_pb_put_type(PBM_MODE_PERFORMANCE);
}

/* order should match enum pbm_mode */
static void(*mdw_rv_pb_put_func[PBM_MODE_MAX])(struct work_struct *wk) = {
	mdw_rv_pb_norm_put_func,
	mdw_rv_pb_perf_put_func,
};

void mdw_rv_pb_init(struct mdw_device *mdev)
{
	uint32_t i = 0;

	/* pb mgr */
	g_pb.mdev = mdev;

	mutex_init(&g_pb.mtx);
	for (i = 0; i < PBM_MODE_MAX; i++) {
		INIT_DELAYED_WORK(&g_pb.put_wk[i], mdw_rv_pb_put_func[i]);
		atomic_set(&g_pb.ref[i], 0);
	}
}
