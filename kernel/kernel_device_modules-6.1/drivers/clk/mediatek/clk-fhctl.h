/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#ifndef __CLK_FHCTL_H
#define __CLK_FHCTL_H

#if IS_ENABLED(CONFIG_KALLSYMS_ALL)
#if defined(__clang__)
#define NO_OPTIMIZE __attribute__((optnone))
#elif defined(__GNUC__) || defined(__GNUG__)
#define NO_OPTIMIZE __attribute__((optimize(0)))
#else
#define NO_OPTIMIZE
#endif
#endif

struct fh_operation {
	int (*hopping)(void *data,
			char *domain, unsigned int fh_id,
			unsigned int new_dds, int postdiv);
	int (*ssc_enable)(void *data,
			char *domain, unsigned int fh_id, int rate);
	int (*ssc_disable)(void *data,
			char *domain, unsigned int fh_id);
};
struct fh_hdlr {
	void *data;
	struct fh_operation *ops;
};
struct pll_dts {
	char *comp;
	int num_pll;
	char *domain;
	char *method;
	char *pll_name;
	unsigned int fh_id;
	int perms;
	int ssc_rate;
	void __iomem *fhctl_base;
	void __iomem *apmixed_base;
	struct fh_hdlr *hdlr;
};

#define PERM_DRV_HOP BIT(0)
#define PERM_DRV_SSC BIT(1)
#define PERM_DBG_HOP BIT(2)
#define PERM_DBG_SSC BIT(3)
#define PERM_DBG_DUMP BIT(4)

#define FHCTL_AP "fhctl-ap"
#define FHCTL_MCUPM "fhctl-mcupm"
#define FHCTL_GPUEB "fhctl-gpueb"
#define FHCTL_VCP "fhctl-vcp"

extern int fhctl_ap_init(struct pll_dts *array);
extern int fhctl_mcupm_init(struct pll_dts *array);
extern int fhctl_gpueb_init(struct pll_dts *array);
extern int fhctl_vcp_init(struct pll_dts *array);
extern int fhctl_debugfs_init(struct pll_dts *array);
#endif
