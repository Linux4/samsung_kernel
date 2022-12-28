/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PCA953X_H
#define _LINUX_PCA953X_H

#include <linux/types.h>
#include <linux/i2c.h>

/* platform data for the PCA9539 16-bit I/O expander driver */

struct pca953x_platform_data {
	/* number of the first GPIO */
	unsigned	gpio_base;

	/* initial polarity inversion setting */
	u32		invert;

	/* interrupt base */
	int		irq_base;

	void		*context;	/* param to setup/teardown */

	int		(*setup)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	int		(*teardown)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	const char	*const *names;

	int		irq_gpio;
	int		gpio_num;
	int		reset_gpio;
	int		pba_conn_det_gpio;
};

char *param_string(enum pin_config_param param)
{
	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:	return "no pull";
	case PIN_CONFIG_BIAS_PULL_UP:	return "pull up";
	case PIN_CONFIG_BIAS_PULL_DOWN:	return "pull down";
	case PIN_CONFIG_DRIVE_STRENGTH:	return "drive strength";
	case PIN_CONFIG_INPUT_ENABLE:	return "input enable";
	case PIN_CONFIG_OUTPUT_ENABLE:	return "output enable";
	case PIN_CONFIG_OUTPUT:		return "output";
	default:
		return "not supported";
	}
}

#endif /* _LINUX_PCA953X_H */
