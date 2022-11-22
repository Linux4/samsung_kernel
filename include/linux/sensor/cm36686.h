#ifndef __LINUX_CM36686_H
#define __CM36686_H__

#include <linux/types.h>

#ifdef __KERNEL__
struct cm36686_platform_data {
	void (*cm36686_light_power)(bool);
	void (*cm36686_proxi_power)(bool);
	void (*cm36686_led_on)(bool);
	int irq;		/* proximity-sensor irq gpio */
	int default_hi_thd;
	int default_low_thd;
	int cancel_hi_thd;
	int cancel_low_thd;
};
#endif
#endif
