/*
 * driver/irled/irled-common.c
 *
 * Copyright (C) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include "irled-common.h"

#define GPIO_LEVEL_LOW	0
#define GPIO_LEVEL_HIGH	1
#define GPIO_LEVEL_NONO	2

int of_irled_power_parse_dt(struct device *dev, struct irled_power *irled_power)
{
	struct device_node *np_irled = dev->of_node;
	int rv = 0;
	const char *temp_str;

	pr_info("%s\n", __func__);

	rv = of_property_read_string(np_irled, "irled,power_type", &temp_str);
	if (rv) {
		pr_info("%s: cannot get power type(%d)\n", __func__, rv);
		goto err;
	}

	pr_info("%s: power type(%s)\n", __func__, temp_str);

	if (!strncasecmp(temp_str, "LDO", 3)) {
		unsigned ldo;

		rv = of_get_named_gpio(np_irled, "irled,ldo", 0);
		if (rv < 0) {
			pr_info("%s: ldo get error(%d)" , __func__, rv);
			goto err;
		}
		ldo = (unsigned)rv;

		rv = gpio_request_one(ldo, GPIOF_OUT_INIT_LOW, "IRLED_LDO");
		if (rv) {
			pr_info("%s: ldo request error(%d)", __func__, rv);
			goto err;
		}

		irled_power->type = PWR_LDO;
		irled_power->ldo = ldo;
	} else if (!strncasecmp(temp_str, "REG", 3)) {
		struct regulator *regulator;

		rv = of_property_read_string(np_irled, "irled,regulator", &temp_str);
		if (rv) {
			pr_err("%s: cannot get regulator_name(%d)\n", __func__, rv);
			goto err;
		}
		regulator = regulator_get(NULL, temp_str);
		if (IS_ERR(regulator)) {
			pr_info("%s: regulator get(%s) error\n", __func__, temp_str);
			rv = -ENODEV;
			goto err;
		}

		irled_power->type = PWR_REG;
		irled_power->regulator = regulator;
	} else if (!strncasecmp(temp_str, "ALW", 3)) {
		irled_power->type = PWR_ALW;
	} else {
		pr_info("%s: unused power type(%s)\n", __func__, temp_str);
		rv = -EINVAL;
		goto err;
	}

	return 0;
err:
	irled_power->type = PWR_NONE;
	return rv;
}

int irled_power_on(struct irled_power *irled_power)
{
	int rv = 0;

	if (irled_power->type == PWR_REG) {
		rv = regulator_enable(irled_power->regulator);
		if (rv)
			pr_info("%s: fail to turn on regulator\n", __func__);
	} else if (irled_power->type == PWR_LDO) {
		gpio_set_value(irled_power->ldo, GPIO_LEVEL_HIGH);
		if (gpio_get_value(irled_power->ldo) != GPIO_LEVEL_HIGH) {
			pr_info("%s: fail to turn on ldo\n", __func__);
			rv = -EFAULT;
		}
	}

	return rv;
}

int irled_power_off(struct irled_power *irled_power)
{
	int rv = 0;

	if (irled_power->type == PWR_REG) {
		rv = regulator_disable(irled_power->regulator);
		if (rv)
			pr_info("%s: fail to turn off regulator\n", __func__);
	} else if (irled_power->type == PWR_LDO) {
		gpio_set_value(irled_power->ldo, GPIO_LEVEL_LOW);
		if (gpio_get_value(irled_power->ldo) != GPIO_LEVEL_LOW) {
			pr_info("%s: fail to turn off ldo\n", __func__);
			rv = -EFAULT;
		}
	}

	return rv;
}
