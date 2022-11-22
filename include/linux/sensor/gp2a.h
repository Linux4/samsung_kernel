#ifndef __GP2A_H__
#define __GP2A_H__

#include <linux/types.h>

#ifdef __KERNEL__

struct gp2a_platform_data {
	int p_out;  /* proximity-sensor-output gpio */
	int irq;
	void (*power)(bool); /* power to the chip */
	void (*led_on)(bool); /* power to the led */
	void (*ldo_on)(bool); /* power to the sensor ldo */
	int (*light_adc_value)(void); /* get light level from adc */
};
#endif /* __KERNEL__ */
#endif
