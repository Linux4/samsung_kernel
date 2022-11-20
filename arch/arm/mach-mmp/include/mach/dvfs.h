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

extern int dvc_flag;
extern unsigned int pxa988_get_vl_num(void);
extern unsigned int pxa988_get_vl(unsigned int vl);

#endif
