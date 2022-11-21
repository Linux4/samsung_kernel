/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - S2MPU (S2MPUFD) fail detector
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *	   Siheung Kim <sheung.kim@samsung.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2MPUFD_H
#define __EXYNOS_S2MPUFD_H

#define MAX_NUM_OF_S2MPUFD_CHANNEL			2

/* Return values from SMC */
#define S2MPUFD_ERROR_INVALID_CH_NUM			(0x600)
#define S2MPUFD_ERROR_INVALID_FAIL_INFO_SIZE		(0x601)

#define S2MPUFD_NEED_FAIL_INFO_LOGGING			(0x603)
#define S2MPUFD_SKIP_FAIL_INFO_LOGGING			(0x604)

/* Flag whether fail read information is logged */
#define STR_INFO_FLAG					(0x45545A43)	/* ETZC */

#ifndef __ASSEMBLY__
/* Registers of S2MPUFD Fail Information */
struct s2mpufd_fail_info {
	unsigned int s2mpufd_intr_stat;
	unsigned int s2mpufd_fail_addr_low;
	unsigned int s2mpufd_fail_addr_high;
	unsigned int s2mpufd_fail_vid;
	unsigned int s2mpufd_rw;
	unsigned int s2mpufd_len;
	unsigned int s2mpufd_axid;
};

/* Data structure for S2MPUFD Fail Information */
struct s2mpufd_info_data {
	struct device *dev;
	struct s2mpufd_fail_info *fail_info;
	dma_addr_t fail_info_pa;
	unsigned int ch_num;
	unsigned int irq[MAX_NUM_OF_S2MPUFD_CHANNEL];
	unsigned int irqcnt;
	unsigned int info_flag;
	int need_log;
};
#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_S2MPUFD_H */
