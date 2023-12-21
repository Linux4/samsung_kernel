#ifndef __LINUX_CM36652_H

#define __CM36652_H__



#include <linux/types.h>



#ifdef __KERNEL__

struct cm36652_platform_data {
	int irq;		/* proximity-sensor irq gpio */
	int default_hi_thd;
	int default_low_thd;
	int cancel_hi_thd;
	int cancel_low_thd;
	int trim;
	int leden_gpio;
	struct regulator *vio_1p8;
	struct regulator *vdd_2p8;
	const char *sub_ldo6;
	const char *sub_ldo10;
};

extern struct class *sensors_class;

#endif

#endif

