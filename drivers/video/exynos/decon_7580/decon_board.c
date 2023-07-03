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
0. There is a pre-defined property of the name of "decon_board".
decon_board has a phandle value that uniquely identifies the other node
containing subnodes to control gpio, regulator, delay and pinctrl.
If you want make new list, write control sequence list in dts, and call run_list function with subnode name.

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
decon_board = <&node>;
node: node {
	subnode {
		type = "regulator,enable", "gpio,high", "delay,usleep", "pinctrl,turnon_tes", "delay,msleep";
		desc = "ldo1 enable", "gpio high", "Wait 10ms", "te pin configuration", "30ms";
		gpios = <&gpf1 5 0x1>;
		delay = <10000 11000>, <30>;
		regulator = "ldo1";
	};
};

run_list(dev, "subnode");

*/

#ifdef CONFIG_LIST_DEBUG	/* this is not defconfig */
#define list_dbg(format, arg...)	printk(format, ##arg)
#else
#define list_dbg(format, arg...)
#endif

#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)
#define STRNEQ(a, b) (strncmp((a), (b), (strlen(a))) == 0)

#define DECON_BOARD_DTS_NAME	"decon_board"

struct list_info {
	char				*name;
	struct list_head		node;
};

struct action_info {
	char				*type;
	char				*subtype;
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
	[ACTION_GPIO_HIGH] = {"gpio", "high"},
	{"gpio", "low"},
	{"regulator", "enable"},
	{"regulator", "disable"},
	{"delay", "mdelay"},
	{"delay", "msleep"},
	{"delay", "usleep"},
	{"pinctrl", ""}
};

static struct list_info	*lists[10];

static int print_action(struct action_info *info)
{
	if (!IS_ERR_OR_NULL(info->desc))
		list_dbg("[%2d] %s\n", info->idx, info->desc);

	switch (info->idx) {
	case ACTION_GPIO_HIGH:
		list_dbg("[%2d] gpio(%d) high\n", info->idx, info->gpio);
		break;
	case ACTION_GPIO_LOW:
		list_dbg("[%2d] gpio(%d) low\n", info->idx, info->gpio);
		break;
	case ACTION_REGULATOR_ENABLE:
		list_dbg("[%2d] regulator(%s) enable\n", info->idx, info->supply->supply);
		break;
	case ACTION_REGULATOR_DISABLE:
		list_dbg("[%2d] regulator(%s) disable\n", info->idx, info->supply->supply);
		break;
	case ACTION_DELAY_MDELAY:
		list_dbg("[%2d] mdelay(%d)\n", info->idx, info->delay[0]);
		break;
	case ACTION_DELAY_MSLEEP:
		list_dbg("[%2d] msleep(%d)\n", info->idx, info->delay[0]);
		break;
	case ACTION_DELAY_USLEEP:
		list_dbg("[%2d] usleep(%d %d)\n", info->idx, info->delay[0], info->delay[1]);
		break;
	case ACTION_PINCTRL:
		list_dbg("[%2d] pinctrl(%s)\n", info->idx, info->state->name);
		break;
	default:
		list_dbg("%s: unknown idx(%d)\n", __func__, info->idx);
		break;
	}

	return 0;
}

static void dump_list(struct list_head *list)
{
	struct action_info *info;

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

	if (STRNEQ("pinctrl", type)) {
		action = ACTION_PINCTRL;
		goto exit;
	}

	for (i = ACTION_GPIO_HIGH; i < ACTION_MAX; i++) {
		if (STRNEQ(action_list[i][0], type) && STRNEQ(action_list[i][1], subtype)) {
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
	int type, desc;
	int gpio = 0, delay = 0, regulator = 0, pinctrl = 0, delay_property = 0;

	of_property_for_each_string(np, "type", prop, s) {
		if (STRNEQ("gpio", s))
			gpio++;
		else if (STRNEQ("regulator", s))
			regulator++;
		else if (STRNEQ("delay", s))
			delay++;
		else if (STRNEQ("pinctrl", s))
			pinctrl++;
		else {
			pr_err("there is no valid type for %s\n", s);
#ifdef CONFIG_LIST_DEBUG
			BUG();
#endif
		}
	}

	of_property_for_each_string(np, "type", prop, s) {
		if (STRNEQ("delay,usleep", s))
			delay++;
	}

	type = of_property_count_strings(np, "type");

	if (of_find_property(np, "desc", NULL)) {
		desc = of_property_count_strings(np, "desc");
		WARN(type != desc, "type(%d) and desc(%d) is not match\n", type, desc);
#ifdef CONFIG_LIST_DEBUG
		BUG_ON(type != desc);
#endif
	}

	if (of_find_property(np, "gpios", NULL)) {
		WARN(gpio != of_gpio_count(np), "gpio(%d %d) is not match\n", gpio, of_gpio_count(np));
#ifdef CONFIG_LIST_DEBUG
		BUG_ON(gpio != of_gpio_count(np));
#endif
	}

	if (of_find_property(np, "regulator", NULL)) {
		WARN(regulator != of_property_count_strings(np, "regulator"),
			"regulator(%d %d) is not match\n", regulator, of_property_count_strings(np, "regulator"));
#ifdef CONFIG_LIST_DEBUG
		BUG_ON(regulator != of_property_count_strings(np, "regulator"));
#endif
	}

	if (of_find_property(np, "delay", &delay_property)) {
		delay_property /= sizeof(u32);
		WARN(delay != delay_property, "delay(%d %d) is not match\n", delay, delay_property);
#ifdef CONFIG_LIST_DEBUG
		BUG_ON(delay != delay_property);
#endif
	}

	pr_info("%s: gpio: %d, regulator: %d, delay: %d, pinctrl: %d\n", __func__, gpio, regulator, delay, pinctrl);

	return 0;
}

static int make_list(struct device *dev, struct list_head *list, const char *name)
{
	struct device_node *np = NULL;
	struct action_info *info;
	int i, count;
	int gpio = 0, delay = 0, regulator = 0, ret = 0;
	const char *type;

	np = of_parse_phandle(dev->of_node, DECON_BOARD_DTS_NAME, 0);
	np = of_find_node_by_name(np, name);
	if (!np) {
		pr_err("%s node does not exist, so create dummy\n", name);
		info = kzalloc(sizeof(struct action_info), GFP_KERNEL);
		list_add_tail(&info->node, list);
		return -EINVAL;
	}

	check_dt(np);

	count = of_property_count_strings(np, "type");

	for (i = 0; i < count; i++) {
		info = kzalloc(sizeof(struct action_info), GFP_KERNEL);

		of_property_read_string_index(np, "type", i, &type);
		info->subtype = kstrdup(type, GFP_KERNEL);
		info->type = strsep(&info->subtype, ",");

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
			if (!gpio_is_valid(info->gpio)) {
				pr_err("%s: %d: of_get_gpio fail\n", __func__, __LINE__);
				ret = -EINVAL;
			}
			gpio++;
			break;
		case ACTION_REGULATOR_ENABLE:
		case ACTION_REGULATOR_DISABLE:
			info->supply = kzalloc(sizeof(struct regulator_bulk_data), GFP_KERNEL);
			of_property_read_string_index(np, "regulator", regulator, &info->supply->supply);
			ret = regulator_bulk_get(NULL, 1, info->supply);
			if (ret)
				pr_err("%s: %d: regulator_bulk_get fail %s %d\n", __func__, __LINE__, info->supply->supply, ret);
			regulator++;
			break;
		case ACTION_DELAY_MDELAY:
		case ACTION_DELAY_MSLEEP:
			ret = of_property_read_u32_index(np, "delay", delay, &info->delay[0]);
			if (ret)
				pr_err("%s: %d: of_property_read_u32_index\n", __func__, __LINE__);
			delay++;
			break;
		case ACTION_DELAY_USLEEP:
			ret = of_property_read_u32_index(np, "delay", delay, &info->delay[0]);
			delay++;
			ret += of_property_read_u32_index(np, "delay", delay, &info->delay[1]);
			delay++;
			if (ret)
				pr_err("%s: %d: of_property_read_u32_index\n", __func__, __LINE__);
			break;
		case ACTION_PINCTRL:
			info->pins = devm_pinctrl_get(dev);
			if (IS_ERR(info->pins)) {
				pr_err("%s: devm_pinctrl_get fail\n", __func__);
				ret = -EINVAL;
			}
			info->state = pinctrl_lookup_state(info->pins, info->subtype);
			if (IS_ERR(info->pins)) {
				pr_err("%s: pinctrl_lookup_state fail %s\n", __func__, info->subtype);
				ret = -EINVAL;
			}
			break;
		default:
			pr_err("%d %s %s error\n", info->idx, info->type, info->subtype);
			ret = -EINVAL;
			break;
		}
	}

#ifdef CONFIG_LIST_DEBUG
	if (ret)
		BUG();
#endif

	return 0;
}

static int do_list(struct list_head *list)
{
	struct action_info *info;
	int ret = 0;

	list_for_each_entry(info, list, node) {
		switch (info->idx) {
		case ACTION_GPIO_HIGH:
			ret = gpio_request_one(info->gpio, GPIOF_OUT_INIT_HIGH, NULL);
			if (ret)
				pr_err("gpio_request_one fail\n");
			gpio_free(info->gpio);
			break;
		case ACTION_GPIO_LOW:
			ret = gpio_request_one(info->gpio, GPIOF_OUT_INIT_LOW, NULL);
			if (ret)
				pr_err("gpio_request_one fail\n");
			gpio_free(info->gpio);
			break;
		case ACTION_REGULATOR_ENABLE:
			ret = regulator_enable(info->supply->consumer);
			if (ret)
				pr_err("regulator_enable fail, %s\n", info->supply->supply);
			break;
		case ACTION_REGULATOR_DISABLE:
			ret = regulator_disable(info->supply->consumer);
			if (ret)
				pr_err("regulator_disable fail, %s\n", info->supply->supply);
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
			pr_err("%s: unknown idx(%d)\n", __func__, info->idx);
			ret = -EINVAL;
			break;
		}
	}

#ifdef CONFIG_LIST_DEBUG
	if (ret)
		BUG();
#endif

	return 0;
}

static inline struct list_head *find_list(const char *name)
{
	struct list_info *info = NULL;
	int idx = 0;

	list_dbg("%s: %s\n", __func__, name);
	while (!IS_ERR_OR_NULL(lists[idx])) {
		info = lists[idx];
		list_dbg("%s: %dth list name is %s\n", __func__, idx, info->name);
		if (STREQ(info->name, name))
			return &info->node;
		idx++;
		BUG_ON(idx == ARRAY_SIZE(lists));
	};

	pr_info("%s is not exist, so create it\n", name);
	info = kzalloc(sizeof(struct list_info), GFP_KERNEL);
	info->name = kstrdup(name, GFP_KERNEL);
	INIT_LIST_HEAD(&info->node);

	lists[idx] = info;

	return &info->node;
}

void run_list(struct device *dev, const char *name)
{
	struct list_head *list = find_list(name);

	if (unlikely(list_empty(list))) {
		pr_info("%s is empty, so make list\n", name);
		make_list(dev, list, name);
		dump_list(list);
	}

	do_list(list);
}

