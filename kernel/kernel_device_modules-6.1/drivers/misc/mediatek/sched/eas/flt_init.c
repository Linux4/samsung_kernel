// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_utility.h"
#include "flt_cal.h"
#include <sugov/cpufreq.h>
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif

#ifdef FLT_INIT_DEBUG
#define FLT_LOGI(...)	pr_info("FLT:" __VA_ARGS__)
#else
#define FLT_LOGI(...)
#endif

#define GKEL	4
#define XLO	33
#define YLO	30
#define ZLO	20
#define TLO	(XLO + YLO + ZLO)

#define RX	11
#define RY	10
#define RZ	7
#define RT	(RX + RY + RZ)

#define AGK	12
#define XLA	23
#define YLA	56
#define ZLA	97

#define L1	10
#define L2	20
#define L3	30

#define ECG	0x1
#define IBS	0x15070a
#define TSH	0x2481c6
#define LFP	0x1f
#define KOL	0x1dc
#define AMI	0x3ff

unsigned int EKV[GKEL][RT] = {0};

static u32 flt_mode = FLT_MODE_0;
static void __iomem *flt_xrg;
static unsigned long long flt_xrg_size;
static bool is_flt_io_enable;

void __iomem *get_flt_xrg(void)
{
	return flt_xrg;
}
EXPORT_SYMBOL(get_flt_xrg);

unsigned long long get_flt_xrg_size(void)
{
	return flt_xrg_size;
}
EXPORT_SYMBOL(get_flt_xrg_size);

void  flt_set_mode(u32 mode)
{
	flt_mode = mode;
	if (is_flt_io_enable)
		flt_update_data(mode, AP_FLT_CTL);
}
EXPORT_SYMBOL(flt_set_mode);

u32 flt_get_mode(void)
{
	return flt_mode;
}
EXPORT_SYMBOL(flt_get_mode);
void flt_kh(unsigned int XHR[], int SS, int LA, unsigned int KK[], int ctp)
{
	int i = 0, RD = 0;
	unsigned int tmp = 0;

	switch (LA) {
	case XLA:
		i = 0;
		RD = 0;
		break;
	case YLA:
		i = XLO;
		RD = RX;
		break;
	case ZLA:
		i = XLO + YLO;
		RD = RX + RY;
		break;
	default:
		return;
	}

	for (; i + 2 < SS; ++i, RD++) {
		tmp = XHR[i];
		++i;
		tmp += XHR[i] << L1;
		++i;
		tmp +=  XHR[i] << L2;
		if (i == SS - 1)
			tmp += ctp << L3;
		KK[RD] = tmp;
	}

	if (LA == ZLA && i + 1 < SS) {
		tmp = XHR[i];
		++i;
		tmp = tmp + (XHR[i] << L1) + (ctp << L2);
		KK[RD] = tmp;
	}
}

static void flt_fei(int wl, int ctp)
{
	struct mtk_em_perf_state *ps = NULL;
	int opp = -1, cpu, i = 0, GG = 0, KY = 0, GK = 0, j = 0, pol = 0, HA = 0;
	struct cpumask *gear_cpus;
	unsigned int nr_gear, gear_idx, MF, MU, LF, LU, TT;
	unsigned int XHR[TLO] = {0};
	unsigned int XU[TLO] = {0};
	unsigned int XF[TLO] = {
		0x3b24a0,
		0x398e60,
		0x37f820,
		0x3661e0,
		0x34cba0,
		0x333560,
		0x319f20,
		0x3008e0,
		0x2e72a0,
		0x2cdc60,
		0x2b4620,
		0x29afe0,
		0x2819a0,
		0x268360,
		0x24ed20,
		0x2356e0,
		0x21c0a0,
		0x202a60,
		0x1e9420,
		0x1cfde0,
		0x1b67a0,
		0x19d160,
		0x183b20,
		0x16a4e0,
		0x150ea0,
		0x137860,
		0x11e220,
		0x104be0,
		0xeb5a0,
		0xd1f60,
		0xb8920,
		0x9f2e0,
		0x85ca0,
		0x30d400,
		0x2f3dc0,
		0x2da780,
		0x2c1140,
		0x2a7b00,
		0x28e4c0,
		0x274e80,
		0x25b840,
		0x242200,
		0x228bc0,
		0x20f580,
		0x1f5f40,
		0x1dc900,
		0x1c32c0,
		0x1a9c80,
		0x190640,
		0x177000,
		0x15d9c0,
		0x144380,
		0x12ad40,
		0x111700,
		0xf80c0,
		0xdea80,
		0xc5440,
		0xabe00,
		0x927c0,
		0x79180,
		0x5fb40,
		0x46500,
		0x2cec0,
		0x225510,
		0x20bed0,
		0x1f2890,
		0x1d9250,
		0x1bfc10,
		0x1a65d0,
		0x18cf90,
		0x173950,
		0x15a310,
		0x140cd0,
		0x127690,
		0x10e050,
		0xf4a10,
		0xdb3d0,
		0xc1d90,
		0xa8750,
		0x8f110,
		0x75ad0,
		0x5c490,
		0x42e50,
	};
	unsigned int X_RR[TLO] = {
		0x1d92,
		0x1cc7,
		0x1bfc,
		0x1b30,
		0x1a65,
		0x199a,
		0x18cf,
		0x1804,
		0x1739,
		0x166e,
		0x15a3,
		0x14d7,
		0x140c,
		0x1341,
		0x1276,
		0x11ab,
		0x10e0,
		0x1015,
		0xf4a,
		0xe7e,
		0xdb3,
		0xce8,
		0xc1d,
		0xb52,
		0xa87,
		0x9bc,
		0x8f1,
		0x825,
		0x75a,
		0x68f,
		0x5c4,
		0x4f9,
		0x42e,
		0x186a,
		0x179e,
		0x16d3,
		0x1608,
		0x153d,
		0x1472,
		0x13a7,
		0x12dc,
		0x1211,
		0x1145,
		0x107a,
		0xfaf,
		0xee4,
		0xe19,
		0xd4e,
		0xc83,
		0xbb8,
		0xaec,
		0xa21,
		0x956,
		0x88b,
		0x7c0,
		0x6f5,
		0x62a,
		0x55f,
		0x493,
		0x3c8,
		0x2fd,
		0x232,
		0x167,
		0x112a,
		0x105f,
		0xf94,
		0xec9,
		0xdfe,
		0xd32,
		0xc67,
		0xb9c,
		0xad1,
		0xa06,
		0x93b,
		0x870,
		0x7a5,
		0x6d9,
		0x60e,
		0x543,
		0x478,
		0x3ad,
		0x2e2,
		0x217,
	};

	nr_gear = get_nr_gears();
	for (gear_idx = 0; gear_idx < nr_gear; gear_idx++) {
		gear_cpus = get_gear_cpumask(gear_idx);
		cpu = cpumask_first(gear_cpus);
		ps = pd_get_opp_ps(wl, cpu, 0, false);
		MF = ps->freq;
		MU = ps->capacity;
		ps = pd_get_opp_ps(wl, cpu, 0xffff, false);
		LF = ps->freq;
		LU = ps->capacity;
		MU = clamp_t(unsigned int, MU, 0, AMI);
		if (gear_idx == 2) {
			KY = 0;
			GK = XLO;
		} else if (gear_idx == 1) {
			KY = XLO;
			GK = XLO + YLO;
		} else if (gear_idx == 0) {
			KY = XLO + YLO;
			GK = TLO;
		} else {
			break;
		}
		for (i = KY; i < GK; i++) {
			TT = XF[i];
			if (TT >= MF) {
				XU[i] = MU;
			} else if (TT <= LF) {
				XU[i] = LU;
			} else {
				ps = pd_get_freq_ps(wl, cpu, TT, &opp);
				XU[i] = ps->capacity;
			}
			FLT_LOGI("wl %d XF[%d] XU[i]%d", wl, XF[i], XU[i]);
		}
	}
	for (i = 0 ; i < TLO; ++i) {
		GG = XU[i] << AGK;
		if (X_RR[i])
			XHR[i] = (GG + (X_RR[i] >> 1)) / X_RR[i];
		XHR[i] = clamp_t(unsigned int, XHR[i], 0, AMI);
	}

	for (i = 0; i < (XLO - 1); ++i) {
		if (XU[i] >= AMI)
			HA = i;
		else
			break;
	}

	if (HA > 0) {
		for (i = (HA + 1); i > 0; --i) {
			j = i - 1;
			if (likely(XHR[i] > XHR[j]))
				pol = XHR[i] - XHR[j];
			else
				pol = XHR[j] - XHR[i];
			pol = (pol >> 2) + (pol >> 3) + 1;
			XHR[j] = XHR[j] - pol;
		}
	}

	flt_kh(XHR, XLO, XLA, EKV[ctp], ctp);
	flt_kh(XHR, XLO + YLO, YLA, EKV[ctp], ctp);
	flt_kh(XHR, TLO, ZLA, EKV[ctp], ctp);

	for (i = 0 ; i < RT; ++i)
		FLT_LOGI("EKV[%d] 0x%x", ctp, EKV[ctp][i]);
}

int flt_init_ekg(void)
{
	struct device_node *flt_node;
	struct platform_device *pdev_temp;
	struct resource *res;

	flt_node = of_find_node_by_name(NULL, "flt");
	if (!flt_node) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(flt_node);
	if (!pdev_temp) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);

	if (res) {
		flt_xrg = ioremap(res->start, resource_size(res));
	} else {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	if (!flt_xrg) {
		pr_info("flt_xrg info failed\n");
		return -EIO;
	}
	flt_xrg_size = resource_size(res);
	FLT_LOGI("xrg %pa size %llu\n", &res->start, resource_size(res));
	is_flt_io_enable = true;
	return 0;
}

void flt_mi(int ctp)
{
	int i = 0, offset = KOL + ((ctp * LFP) << 2);

	/* sanity check */
	if (ctp < 0 || ctp >= GKEL)
		return;

	for (i = 0 ; i < RT; ++i) {
		iowrite32(EKV[ctp][i], flt_xrg + offset);
		FLT_LOGI("mi reg 0x%x offset %d\n", ioread32(flt_xrg + offset), offset);
		offset += 4;
	}

	iowrite32(ECG, flt_xrg + offset);
	FLT_LOGI("mi xrg 0x%x offset %d\n", ioread32(flt_xrg + offset), offset);
	offset += 4;
	iowrite32(IBS, flt_xrg + offset);
	FLT_LOGI("mi xrg 0x%x offset %d\n", ioread32(flt_xrg + offset), offset);
	offset += 4;
	iowrite32(TSH, flt_xrg + offset);
	FLT_LOGI("mi xrg 0x%x offset %d\n", ioread32(flt_xrg + offset), offset);
}

int flt_init_res(void)
{
	int wl = 0, nr_wl = 0, ctp = 0, nr_cpu, ret;
	bool BKV[GKEL] = {false};

	flt_cal_init();
	nr_wl = get_nr_wl_type();
	nr_cpu = get_nr_cpu_type();
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return -1;
	else if (unlikely(flt_get_mode() == FLT_MODE_2)) {
		for (wl = 0; wl < nr_wl; ++wl) {
			ctp = get_cpu_type(wl);
			FLT_LOGI("nr_cpu %d wl %d get_cpu_type %d\n", nr_cpu, wl, ctp);
			if (ctp >= 0 && ctp < GKEL && !BKV[ctp]) {
				flt_fei(wl, ctp);
				BKV[ctp] = true;
			}
		}
		ret = flt_init_ekg();
		if (ret)
			return ret;
		for (ctp = 0; ctp < GKEL && ctp < nr_cpu; ctp++)
			flt_mi(ctp);
		flt_mode2_register_api_hooks();
		flt_mode2_init_res();
	}

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	register_sugov_hooks();
#endif
	return 0;
}
