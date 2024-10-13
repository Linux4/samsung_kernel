/*
 * Copyright (C) 2020 Samsung Electronics. All rights reserved.
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
 * along with this program;
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
#include <linux/of_gpio.h>
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */

#include "sec_star.h"

#undef USE_INTERNAL_PULLUP //if use external pull-up, not necessary

#define ERR(msg...)		pr_err("[star-k250a] : " msg)
#define INFO(msg...)	pr_info("[star-k250a] : " msg)

struct k250a_dev {
	struct i2c_client *client;
	struct regulator *vdd;
	unsigned int reset_gpio;
#if defined(USE_INTERNAL_PULLUP)
#define SCL_GPIO_NUM	335
#define SDA_GPIO_NUM	320
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_i2c;
	struct pinctrl_state *pin_gpio;
#endif
	sec_star_t *star;
};

static struct k250a_dev g_k250a;

static int k250a_poweron(void)
{
	int ret = 0;

	INFO("k250a_poweron\n");

	if (g_k250a.vdd == NULL) {
		if (g_k250a.reset_gpio == 0) {
			return 0;
		}

		INFO("rest pin control instead of vdd\n");

                /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
                // gpio_set_value(g_k250a.reset_gpio, 0);
                INFO("[%s]l=%d: before gpio=%d\n", __FUNCTION__, __LINE__,
                        gpio_get_value(g_k250a.reset_gpio));
                gpio_direction_output(g_k250a.reset_gpio, 0);
                usleep_range(1000, 2000);
	            // gpio_set_value(g_k250a.reset_gpio, 1);
                gpio_direction_output(g_k250a.reset_gpio, 1);
                INFO("[%s]l=%d: after gpio=%d\n", __FUNCTION__, __LINE__,
                        gpio_get_value(g_k250a.reset_gpio));
                /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */

		usleep_range(15000, 20000);
		return 0;
	}

	ret = regulator_enable(g_k250a.vdd);
	if (ret) {
		ERR("%s - enable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(1000, 2000);

#if defined(USE_INTERNAL_PULLUP)
	if (pinctrl_select_state(g_k250a.pinctrl, g_k250a.pin_i2c) < 0) {
		ERR("failed to pinctrl_select_state for gpio");
	}
#endif

	usleep_range(14000, 15000);

	return 0;
}

static int k250a_poweroff(void)
{
	int ret = 0;

	INFO("k250a_poweroff\n");

        if (g_k250a.vdd == NULL) {
            /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
            if (g_k250a.reset_gpio == 0) {
                return 0;
            }

            INFO("rest pin control instead of vdd\n");

            INFO("[%s]l=%d: before gpio=%d\n", __FUNCTION__, __LINE__,
                    gpio_get_value(g_k250a.reset_gpio));
            gpio_direction_output(g_k250a.reset_gpio, 0);
            INFO("[%s]l=%d: after gpio=%d\n", __FUNCTION__, __LINE__,
                    gpio_get_value(g_k250a.reset_gpio));

            usleep_range(15000, 20000);
            /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
            return 0;
        }

#if defined(USE_INTERNAL_PULLUP)
	if (pinctrl_select_state(g_k250a.pinctrl, g_k250a.pin_gpio) < 0) {
		ERR("failed to pinctrl_select_state for gpio");
	}
#endif

	ret = regulator_disable(g_k250a.vdd);
	if (ret) {
		ERR("%s - disable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(15000, 20000);
	return 0;
}

static int k250a_reset(void)
{
	if (g_k250a.reset_gpio == 0) {
		return -ENODEV;
	}

        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
        // gpio_set_value(g_k250a.reset_gpio, 0);
        INFO("[%s]l=%d: before gpio=%d\n", __FUNCTION__, __LINE__,
                gpio_get_value(g_k250a.reset_gpio));
        gpio_direction_output(g_k250a.reset_gpio, 0);
        usleep_range(1000, 2000);
        // gpio_set_value(g_k250a.reset_gpio, 1);
        gpio_direction_output(g_k250a.reset_gpio, 1);
        INFO("[%s]l=%d: after gpio=%d\n", __FUNCTION__, __LINE__,
                gpio_get_value(g_k250a.reset_gpio));
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
        usleep_range(15000, 20000);
	return 0;
}

static star_dev_t star_dev = {
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
	// .name = "sec-k250a",
        .name = "k250a",
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
	.hal_type = SEC_HAL_I2C,
	.client = NULL,
	.power_on = k250a_poweron,
	.power_off = k250a_poweroff,
	.reset = k250a_reset
};

/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
#define EVB0    "EVB0"
#define EVB1    "EVB1"
static bool k250a_get_cmdline(const char *name)
{
    const char *bootparams = NULL;
    bool result = false;
    struct device_node *np = NULL;

    if (!name) {
        ERR("[%s]l=%d: name is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    np = of_find_node_by_path("/chosen");
    if (!np) {
        ERR("[%s]l=%d: failed to find node\n",
            __FUNCTION__, __LINE__);
        return false;
    }

    if (of_property_read_string(np, "bootargs", &bootparams)) {
        ERR("[%s]l=%d: failed to get bootargs property\n",
            __FUNCTION__, __LINE__);
        of_node_put(np); // Free the node reference
        return false;
    }

    result = strnstr(bootparams, name, strlen(bootparams)) ? true : false;
    if (result) {
        ERR("[%s]l=%d: Successfully matched '%s' in cmdline\n",
            __FUNCTION__, __LINE__, name);
    } else {
        ERR("[%s]l=%d: No '%s' in cmdline\n",
            __FUNCTION__, __LINE__, name);
    }

    of_node_put(np); // Free the node reference

    return result;
}
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */

static int k250a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	INFO("Entry : %s\n", __func__);

        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
        if (k250a_get_cmdline(EVB0) || k250a_get_cmdline(EVB1)) {
            ERR("[%s]l=%d: EVB0, EVB1 don't SMT k250a\n", __FUNCTION__, __LINE__);
            return -1;
        } else {
            ERR("[%s]l=%d: SMT k250a\n", __FUNCTION__, __LINE__);
        }
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */

	if (np) {
		g_k250a.vdd = devm_regulator_get_optional(&(client->dev), "1p8_pvdd");
		if (IS_ERR(g_k250a.vdd)) {
			ERR("%s - 1p8_pvdd can not be used\n", __func__);
			g_k250a.vdd = NULL;
		}

                /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
                // if (of_property_read_u32(np, "reset_gpio", &(g_k250a.reset_gpio)) < 0) {
                g_k250a.reset_gpio = of_get_named_gpio(np, "reset_gpio", 0);
                if (g_k250a.reset_gpio < 0) {
                    ERR("%s - Reset Control can not be used\n", __func__);
                    g_k250a.reset_gpio = 0;
                } else {
                    if (gpio_request(g_k250a.reset_gpio, "sec-reset") < 0) {
                        ERR("%s - Failed to request sec-reset gpio\n", __func__);
                        g_k250a.reset_gpio = 0;
                    } else {
                        INFO("%s - Reset GPIO Num : %u\n", __func__, g_k250a.reset_gpio);
                        if (gpio_direction_output(g_k250a.reset_gpio, 1) < 0) {
                            ERR("Failed to set reset gpio");
                        }
                        INFO("[%s]l=%d: gpio=%d\n", __FUNCTION__, __LINE__,
                                gpio_get_value(g_k250a.reset_gpio));
                    }
                }
                /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
	} else {
		return -ENODEV;
	}

#if defined(USE_INTERNAL_PULLUP)
	g_k250a.pinctrl = devm_pinctrl_get(client->adapter->dev.parent);
	if (IS_ERR(g_k250a.pinctrl)) {
		ERR("failed to devm_pinctrl_get");
		return -1;
	}

	g_k250a.pin_i2c = pinctrl_lookup_state(g_k250a.pinctrl, "default");
	if (IS_ERR(g_k250a.pin_i2c)) {
		ERR("failed to pinctrl_lookup_state for i2c");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	g_k250a.pin_gpio = pinctrl_lookup_state(g_k250a.pinctrl, "gpio");
	if (IS_ERR(g_k250a.pin_gpio)) {
		ERR("failed to pinctrl_lookup_state for gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	if (pinctrl_select_state(g_k250a.pinctrl, g_k250a.pin_gpio) < 0) {
		ERR("failed to pinctrl_select_state for gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	if (gpio_request(SCL_GPIO_NUM, "sec-scl") < 0) {
		ERR("failed to get scl gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	if (gpio_request(SDA_GPIO_NUM, "sec-sda") < 0) {
		ERR("failed to get sda gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	if (gpio_direction_output(SCL_GPIO_NUM, 0) < 0) {
		ERR("failed to set scl gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}

	if (gpio_direction_output(SDA_GPIO_NUM, 0) < 0) {
		ERR("failed to set sda gpio");
		devm_pinctrl_put(g_k250a.pinctrl);
		return -1;
	}
#endif

	g_k250a.client = client;
	star_dev.client = client;
	g_k250a.star = star_open(&star_dev);
	if (g_k250a.star == NULL) {
		return -ENODEV;
	}

	INFO("Exit : %s\n", __func__);
	return 0;
}

static int k250a_remove(struct i2c_client *client)
{
	INFO("Entry : %s\n", __func__);
#if defined(USE_INTERNAL_PULLUP)
	devm_pinctrl_put(g_k250a.pinctrl);
	gpio_free(SCL_GPIO_NUM);
	gpio_free(SDA_GPIO_NUM);
#endif
	if (g_k250a.reset_gpio != 0) {
		gpio_free(g_k250a.reset_gpio);
	}
	star_close(g_k250a.star);
	INFO("Exit : %s\n", __func__);
	return 0;
}

static const struct i2c_device_id k250a_id[] = {
	{"k250a", 0},
	{}
};

static const struct of_device_id k250a_match_table[] = {
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
#if !defined(CONFIG_CUSTOM_FACTORY_BUILD)
	{ .compatible = "sec_k250a",},
#endif
	{},
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
};

static struct i2c_driver k250a_driver = {
	.id_table = k250a_id,
	.probe = k250a_probe,
	.remove = k250a_remove,
	.driver = {
		.name = "k250a",
		.owner = THIS_MODULE,
		.of_match_table = k250a_match_table,
	},
};

static int __init k250a_init(void)
{
	INFO("Entry : %s\n", __func__);
	return i2c_add_driver(&k250a_driver);
}
module_init(k250a_init);

static void __exit k250a_exit(void)
{
	INFO("Entry : %s\n", __func__);
	i2c_del_driver(&k250a_driver);
}

module_exit(k250a_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("K250A driver");
MODULE_LICENSE("GPL");
