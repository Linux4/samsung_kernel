#ifndef __LINUX_I2C_FT5306_TOUCH_H
#define __LINUX_I2C_FT5306_TOUCH_H

#include <linux/input.h>

/*
 * power func:	power on / off touch
 * reset func:	reset touch
 * keypad_func: get virtual key code
 * abs_x_max:	max value of abs_x
 * abs_y_max:	max value of abs_y
 * abs_flag:	convert the frame of axes
 *		0: no change
 *		1: convert the frame of axes 90 degree by clockwise
 *		2: convert the frame of axes 180 degree by clockwise
 *		3: convert the frame of axes 270 degree by clockwise
 * virtual_key: enable/disable virtual key flag
 *		0: disable virtual key
 *		1: enable virtual key
 */
struct ft5306_touch_platform_data {
	int (*power)(struct device *dev, int);
	void (*reset)(void);
	u32 (*keypad)(u16, u16, u16, u16);
	int abs_x_max;
	int abs_y_max;
	int abs_flag;
	int virtual_key;
	int (*set_virtual_key)(struct input_dev *);
};

#endif
