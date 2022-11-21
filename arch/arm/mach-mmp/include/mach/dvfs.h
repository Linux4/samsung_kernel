/*
 *  linux/arch/arm/mach-mmp/include/mach/dvfs.h
 *
 *  Author: Xiaoguang Chen chenxg@marvell.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _MACH_MMP_DVFS_H_
#define _MACH_MMP_DVFS_H_
#include <plat/dvfs.h>

#define DVFS_FREQUENCY_NOTIFIER		0
#define DVFS_FREQ_PRECHANGE		0
#define DVFS_FREQ_POSTCHANGE		1

struct dvfs_freqs {
	struct dvfs *dvfs;
	unsigned int old;	/*old frequency */
	unsigned int new;	/*new frequency */
};

extern int dvfs_notifier_frequency(struct dvfs_freqs *freqs,
				   unsigned int state);
extern int dvfs_register_notifier(struct notifier_block *nb, unsigned int list);
extern int dvfs_unregister_notifier(struct notifier_block *nb,
				    unsigned int list);
extern int dvc_flag;
extern unsigned int pxa988_get_vl_num(void);
extern unsigned int pxa988_get_vl(unsigned int vl);

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
#define DVC_WR
#endif

#endif
