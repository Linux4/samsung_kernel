/* linux/drivers/video/exynos/decon_display/decon_board.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include "../../../pinctrl/core.h"

#include "decon_board.h"

/*
*
0. There are pre-defined 3 nodes. and each-node has it's sequence list
- enable_display_driver_power
- disable_display_driver_power
- reset_display_driver_panel

1. type
There are 4 pre-defined types
- GPIO has 2 kinds of subtype: HIGH, LOW
- REGULATOR has 2 kinds of subtype: ENABLE, DISABLE
- DELAY has 3 kinds of subtype: MDELAY, MSLEEP, USLEEP
- PINCTRL has no pre-defined subtype, it needs pinctrl name as string in subtype position.

2. subtype and property
- GPIO(HIGH, LOW) search gpios-property for gpio position. 1 at a time.
- REGULATOR(ENABLE, DISABLE) search regulator-property for regulator name. 1 at a time.
- DELAY(MDELAY, MSLEEP) search delay-property for delay duration. 1 at a time.
- DELAY(USLEEP) search delay-property for delay duration. 2 at a time.

3. property type
- type, subtype and desc property type is string
- gpio-property type is phandle
- regulator-property type is string
- delay-property type is u32

4. check rule
- number of type = number of subtype
- number of each type = number of each property.
But If subtype is USLEEP, it needs 2 parameter. So we check 1 USLEEP = 2 * u32 delay-property
- desc-property is for debugging message description. It's not essential.

5. example:
enable_display_driver_power {
	type = "REGULATOR", "GPIO", "REGULATOR", "DELAY", "PINCTRL", "PINCTRL", "DELAY", "DELAY";
	subtype = "ENABLE", "HIGH", "ENABLE", "USLEEP", "default", "turnon_tes", "MDELAY", "MSLEEP";
	desc = "ldo1 enable", "gpio high", "", "Wait 10ms", "", "te pin configuration", "", "30ms";
	gpios = <&gpf1 5 0x1>;
	delay = <10000 11000>, <1>, <30>;
	regulator = "ldo1", "ldo2";
};
*/

#ifdef CONFIG_LIST_DEBUG	/* this is not defconfig */
#define list_dbg(format, arg...)	printk(format, ##arg)
#else
#define list_dbg(format, arg...)
#endif

struct action_t {
	const char			*type;
	const char			*subtype;
	const char			*desc;

	unsigned int			idx;
	int				gpio;
	unsigned int			delay[2];
	struct regulator_bulk_data	*supply;
	struct pinctrl			*pins;
	struct pinctrl_state		*state;
	struct list_head		node;
};

enum {
	ACTION_DUMMY,
	ACTION_GPIO_HIGH,
	ACTION_GPIO_LOW,
	ACTION_REGULATOR_ENABLE,
	ACTION_REGULATOR_DISABLE,
	ACTION_DELAY_MDELAY,
	ACTION_DELAY_MSLEEP,
	ACTION_DELAY_USLEEP,	/* usleep_range */
	ACTION_PINCTRL,
	ACTION_MAX
};

const char *action_list[ACTION_MAX][2] = {
	[ACTION_GPIO_HIGH] = {"GPIO", "HIGH"},
	{"GPIO", "LOW"},
	{"REGULATOR", "ENABLE"},
	{"REGULATOR", "DISABLE"},
	{"DELAY", "MDELAY"},
	{"DELAY", "MSLEEP"},
	{"DELAY", "USLEEP"},
	{"PINCTRL", ""}
};

static LIST_HEAD(enable_list);
static LIST_HEAD(disable_list);
static LIST_HEAD(reset_list);

static int print_action(struct action_t *info)
{
	list_dbg("[%2d] ", info->idx);

	if (!IS_ERR_OR_NULL(info->desc))
		list_dbg("%s, ", info->desc);

	switch (info->idx) {
	case ACTION_GPIO_HIGH:
		list_dbg("gpio(%d) high\n", info->gpio);
		break;
	case ACTION_GPIO_LOW:
		list_dbg("gpio(%d) low\n", info->gpio);
		break;
	case ACTION_REGULATOR_ENABLE:
		list_dbg("regulator(%s) enable\n", info->supply->supply);
		break;
	case ACTION_REGULATOR_DISABLE:
		list_dbg("regulator(%s) disable\n", info->supply->supply);
		break;
	case ACTION_DELAY_MDELAY:
		list_dbg("mdelay(%d)\n", info->delay[0]);
		break;
	case ACTION_DELAY_MSLEEP:
		list_dbg("msleep(%d)\n", info->delay[0]);
		break;
	case ACTION_DELAY_USLEEP:
		list_dbg("usleep(%d %d)\n", info->delay[0], info->delay[1]);
		break;
	case ACTION_PINCTRL:
		list_dbg("pinctrl(%s)\n", info->state->name);
		break;
	default:
		list_dbg("unknown idx(%d)\n", info->idx);
		break;
	}

	return 0;
}

static void dump_list(struct list_head *list)
{
	struct action_t *info;

	list_for_each_entry(info, list, node) {
		print_action(info);
	}
}

static int decide_action(const char *type, const char *subtype)
{
	int i;
	int action = ACTION_DUMMY;

	if (type == NULL || *type == '\0')
		return 0;
	if (subtype == NULL || *subtype == '\0')
		return 0;

	if (!strcmp("PINCTRL", type)) {
		action = ACTION_PINCTRL;
		goto exit;
	}

	for (i = ACTION_GPIO_HIGH; i < ACTION_MAX; i++) {
		if (!strcmp(type, action_list[i][0]) && !strcmp(subtype, action_list[i][1])) {
			action = i;
			break;
		}
	}

exit:
	if (action == ACTION_DUMMY)
		pr_err("no valid action for %s %s\n", type, subtype);

	return action;
}

static int check_dt(struct device_node *np)
{
	struct property *prop;
	const char *s;
	int type, subtype, desc;
	int gpio = 0, delay = 0, regulator = 0, pinctrl = 0, delay_property = 0;

	of_property_for_each_string(np, "type", prop, s) {
		if (!strcmp("GPIO", s))
			gpio++;
		else if (!strcmp("REGULATOR", s))
			regulator++;
		else if (!strcmp("DELAY", s))
			delay++;
		else if (!strcmp("PINCTRL", s))
			pinctrl++;
		else
			pr_err("there is no valid type for %s\n", s);
	}

	of_property_for_each_string(np, "subtype", prop, s) {
		if (!strcmp("USLEEP", s))
			delay++;
	}

	type = of_property_count_strings(np, "type");
	subtype = of_property_count_strings(np, "subtype");

	WARN(type != subtype, "type(%d) and subtype(%d) is not match\n", type, subtype);

	if (of_find_property(np, "desc", NULL)) {
		desc = of_property_count_strings(np, "desc");
		WARN(type != desc, "type(%d) and desc(%d) is not match\n", type, desc);
	}

	if (of_find_property(np, "gpios", NULL))
		WARN(gpio != of_gpio_count(np), "gpio(%d %d) is not match\n", gpio, of_gpio_count(np));

	if (of_find_property(np, "regulator", NULL)) {
		WARN(regulator != of_property_count_strings(np, "regulator"),
			"regulator(%d %d) is not match\n", regulator, of_property_count_strings(np, "regulator"));
	}

	if (of_find_property(np, "delay", &delay_property)) {
		delay_property /= sizeof(u32);
		WARN(delay != delay_property, "delay(%d %d) is not match\n", delay, delay_property);
	}

	pr_info("%s: gpio: %d, regulator: %d, delay: %d, pinctrl: %d\n", __func__, gpio, regulator, delay, pinctrl);

	return 0;
}

static struct list_head *decide_list(const char *name)
{
	struct list_head *list = NULL;

	if (!strcmp(name, "enable_display_driver_power"))
		list = &enable_list;
	else if (!strcmp(name, "disable_display_driver_power"))
		list = &disable_list;
	else if (!strcmp(name, "reset_display_driver_panel"))
		list = &reset_list;
	else
		BUG();

	return list;
}

static int make_list(struct device *dev, struct list_head *list, const char *name)
{
	struct device_node *np = NULL;
	struct action_t *info;
	int i, count;
	int gpio = 0, delay = 0, regulator = 0, ret = 0;

	np = of_find_node_by_name(dev->of_node, name);
	if (!np) {
		pr_err("%s node does not exist, so create dummy\n", name);
		info = kzalloc(sizeof(struct action_t), GFP_KERNEL);
		list_add_tail(&info->node, list);
		return -EINVAL;
	}

	check_dt(np);

	count = of_property_count_strings(np, "type");

	for (i = 0; i < count; i++) {
		info = kzalloc(sizeof(struct action_t), GFP_KERNEL);

		of_property_read_string_index(np, "type", i, &info->type);
		of_property_read_string_index(np, "subtype", i, &info->subtype);

		if (of_property_count_strings(np, "desc") == count)
			of_property_read_string_index(np, "desc", i, &info->desc);

		info->idx = decide_action(info->type, info->subtype);

		list_add_tail(&info->node, list);
	}

	list_for_each_entry(info, list, node) {
		switch (info->idx) {
		case ACTION_GPIO_HIGH:
		case ACTION_GPIO_LOW:
			info->gpio = of_get_gpio(np, gpio);
			gpio++;
			break;
		case ACTION_REGULATOR_ENABLE:
		case ACTION_REGULATOR_DISABLE:
			info->supply = kzalloc(sizeof(struct regulator_bulk_data), GFP_KERNEL);
			of_property_read_string_index(np, "regulator", regulator, &info->supply->supply);
			ret = regulator_bulk_get(NULL, 1, info->supply);
			regulator++;
			break;
		case ACTION_DELAY_MDELAY:
		case ACTION_DELAY_MSLEEP:
			of_property_read_u32_index(np, "delay", delay, &info->delay[0]);
			delay++;
			break;
		case ACTION_DELAY_USLEEP:
			of_property_read_u32_index(np, "delay", delay, &info->delay[0]);
			delay++;
			of_property_read_u32_index(np, "delay", delay, &info->delay[1]);
			delay++;
			break;
		case ACTION_PINCTRL:
			info->pins = devm_pinctrl_get(dev);
			info->state = pinctrl_lookup_state(info->pins, info->subtype);
			break;
		default:
			pr_err("%d %s %s error\n", info->idx, info->type, info->subtype);
			break;
		}
	}

	return 0;
}

static int do_list(struct list_head *list)
{
	struct action_t *info;
	int ret = 0;

	list_for_each_entry(info, list, node) {
		switch (info->idx) {
		case ACTION_GPIO_HIGH:
			ret = gpio_request_one(info->gpio, GPIOF_OUT_INIT_HIGH, NULL);
			gpio_free(info->gpio);
			break;
		case ACTION_GPIO_LOW:
			ret = gpio_request_one(info->gpio, GPIOF_OUT_INIT_LOW, NULL);
			gpio_free(info->gpio);
			break;
		case ACTION_REGULATOR_ENABLE:
			ret = regulator_enable(info->supply->consumer);
			break;
		case ACTION_REGULATOR_DISABLE:
			ret = regulator_disable(info->supply->consumer);
			break;
		case ACTION_DELAY_MDELAY:
			mdelay(info->delay[0]);
			break;
		case ACTION_DELAY_MSLEEP:
			msleep(info->delay[0]);
			break;
		case ACTION_DELAY_USLEEP:
			usleep_range(info->delay[0], info->delay[1]);
			break;
		case ACTION_PINCTRL:
			pinctrl_select_state(info->pins, info->state);
			break;
		case ACTION_DUMMY:
			break;
		default:
			pr_err("unknown idx(%d)\n", info->idx);
			break;
		}
	}

	return 0;
}

void run_list(struct device *dev, const char *name)
{
	struct list_head *list = decide_list(name);

	if (unlikely(list_empty(list))) {
		pr_info("%s is empty, so create it\n", name);
		make_list(dev, list, name);
		dump_list(list);
	}

	do_list(list);
}

