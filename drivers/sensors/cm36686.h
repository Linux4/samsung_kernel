#ifndef __LINUX_CM36686_H
#define __CM36686_H__

#include <linux/types.h>

#ifdef __KERNEL__
struct cm36686_platform_data {
	int irq;		/* proximity-sensor irq gpio */

	int default_hi_thd;
	int default_low_thd;
	int cancel_hi_thd;
	int cancel_low_thd;
	int default_trim;

	int vdd_always_on; /* 1: vdd is always on, 0: enable only when proximity is on */
	int vled_ldo; /*0: vled(anode) source regulator, other: get power by LDO control */
	int vled_same_vdd; /* 1: vled source is same with vdd source, 0: vled source is different with vdd source */
};
#endif
#endif
