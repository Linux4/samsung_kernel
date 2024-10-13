/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "../../../../regulator/internal.h"

#include "dd.h"			/* init_debugfs_param */
#include "smcdsd_board.h"	/* of_get_gpio_with_name */

/*
 *
0. basic tuple type is reg, val

1. if you want to use delay, mask and delay+mask,
define pre-defined tuple type in dtsi which you want.

lcd_generic_supply: lcd-generic-supply {
	tuple_type_default: tuple-default {
		tuple-order = "reg", "val";
	};
	tuple_type_with_delay: tuple-type1 {
		tuple-order = "reg", "val", "delay";
	};
	tuple_type_with_mask: tuple-type2 {
		tuple-order = "reg", "val", "mask";
	};
	tuple_type_with_mask_delay: tuple-type3 {
		tuple-order = "reg", "val", "mask", "delay";
	};
};

support delay is ms.

2. example
--------------------------------------------------
foo: foo {
	compatible = "samsung,generic_lcd_supply";
	regulator-name = "lcd_foo";
	regulator-boot-on;

	disable = /bits/ 8 <
		0x11 0x22
	>;
	enable = /bits/ 8 <
		0x33 0x44
		0x55 0x66
	>;

	gpio = <&pio 1 0>;
};
--------------------------------------------------
foo: foo {
	compatible = "samsung,generic_lcd_supply";
	regulator-name = "lcd_foo";
	regulator-boot-on;
	tuple-type = <&tuple_type_with_delay>;

	enable = /bits/ 8 <
		0x33 0x44 1 <---- 1ms
		0x55 0x66 0
	>;

	gpios = <&pio 1 0 &pio 2 0>;
};
--------------------------------------------------
smcdsd_board_foo: smcdsd_board_foo {
	//gpio_foo = <&gpio 1 0>;
	gpio_bar = <&gpio 2 0>;
};

foo: foo {
	compatible = "samsung,generic_lcd_supply";
	regulator-name = "lcd_foo";
	regulator-boot-on;

	named_gpios = "gpio_foo", "gpio_bar"; <----- gpio_foo is gone
};
--------------------------------------------------
foo: foo {
	compatible = "samsung,generic_lcd_supply";
	regulator-name = "lcd_foo";
	regulator-boot-on;
	tuple-type = <&tuple_type_with_delay>;
#if 0
	disable = /bits/ 8 <
		0x11 0x22 1
	>;
	enable = /bits/ 8 <
		0x33 0x44 1
		0x55 0x66 0
	>;
#endif
};

u8 foo_data[] = { 0x11, 0x22, 1 };
set_reg_data_to_supply("lcd_foo", REGULATOR_EVENT_DISABLE, foo_data, ARRAY_SIZE(foo_data));

struct reg_sequence bar_data = {
	{0x33, 0x44, 1000, (1000 = 1 * USEC_PER_MSEC)},
	{0x55, 0x66, 0},
};
set_reg_sequence_to_supply("lcd_foo", REGULATOR_EVENT_ENABLE, bar_data, ARRAY_SIZE(bar_data));
--------------------------------------------------
 *
 */

#undef pr_fmt
#define pr_fmt(fmt) "smcdsd: %s: " fmt, __func__

//echo 'file smcdsd_supply.c +p' > /d/dynamic_debug/control
//#define dbg_none
#define dbg_none(fmt, ...)		pr_debug(fmt, ##__VA_ARGS__)
//#define dbg_info(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)
#define dbg_once(fmt, ...)		pr_info_once(fmt, ##__VA_ARGS__)
#define dbg_init(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)

#define DTS_TUPLE_TYPE			"tuple-type"
#define DTS_TUPLE_ORDER			"tuple-order"
#define DTS_NAMED_GPIOS			"named-gpios"
#define DTS_SUPPLY_DEFAULT_ON		"supply-default-on"
#define DTS_REGULATOR_NOTIFIER_WITH	"regulator-notifier-with"
#define DTS_GPIO_ENABLE_DELAY		"gpio-enable-delay"

extern unsigned int lcdtype;

static DEFINE_IDR(generic_supply_idr);

static inline int get_boot_lcdtype(void)
{
	return (int)lcdtype;
}

static inline unsigned int get_boot_lcdconnected(void)
{
	return get_boot_lcdtype() ? 1 : 0;
}

enum {
	GENERIC_SUPPLY_EVENT_DISABLE,
	GENERIC_SUPPLY_EVENT_ENABLE,
	GENERIC_SUPPLY_EVENT_MAX,
};

const char *reg_sequence_name[GENERIC_SUPPLY_EVENT_MAX] = {
	"disable",
	"enable",
};

enum {
	REG_TUPLE_TYPE_DEFAULT,
	REG_TUPLE_TYPE_NO_DELAY = REG_TUPLE_TYPE_DEFAULT,
	REG_TUPLE_TYPE_WITH_DELAY,
	REG_TUPLE_TYPE_EXTENDED = REG_TUPLE_TYPE_WITH_DELAY,
	REG_TUPLE_TYPE_WITH_MASK,
	REG_TUPLE_TYPE_WITH_MASK_DELAY,
	REG_TUPLE_TYPE_MAX,
};

enum {
	REG_TUPLE_SIZE_DEFAULT = 2,
/*
 *	REG_TUPLE_SIZE_NO_DELAY = REG_TUPLE_TYPE_DEFAULT,
 *	REG_TUPLE_SIZE_WITH_DELAY = 3,
 *	REG_TUPLE_SIZE_WITH_MASK = 3,
 *	REG_TUPLE_SIZE_WITH_MASK_DELAY = 4,
 *	REG_TUPLE_SIZE_MAX,
 */
};

const char *tuple_type_name[REG_TUPLE_TYPE_MAX] = {
	"tuple-default",
	"tuple-type1",
	"tuple-type2",
	"tuple-type3",
};

enum {
	REG_TUPLE_UNIT_REG,
	REG_TUPLE_UNIT_VAL,
	REG_TUPLE_UNIT_DELAY,
	REG_TUPLE_UNIT_MASK,
	REG_TUPLE_UNIT_MAX,
};

const char *tuple_unit_name[REG_TUPLE_UNIT_MAX] = {
	"reg",
	"val",
	"delay",
	"mask",
};

static inline int tuple_has_delay(int type)
{
	return (type == REG_TUPLE_TYPE_WITH_DELAY || type == REG_TUPLE_TYPE_WITH_MASK_DELAY);
};

static inline int tuple_has_mask(int type)
{
	return (type == REG_TUPLE_TYPE_WITH_MASK || type == REG_TUPLE_TYPE_WITH_MASK_DELAY);
};

struct generic_supply_data {
	struct regulator_desc desc;
	struct regulator_dev *rdev;

	unsigned int enable_counter;

	struct gpio_desc **gpiods;
	int nr_gpios;

	unsigned int gpio_enable_delay_ms;

	struct regmap *rmap;
	struct reg_sequence *regs[GENERIC_SUPPLY_EVENT_MAX];
	int num_regs[GENERIC_SUPPLY_EVENT_MAX];
	u8 *raw_data[GENERIC_SUPPLY_EVENT_MAX];
	u8 *delay_ms[GENERIC_SUPPLY_EVENT_MAX];
	u8 *mask[GENERIC_SUPPLY_EVENT_MAX];

	int tuple_size;
	int tuple_type;
	u8 tuple_offset[REG_TUPLE_UNIT_MAX];

	struct notifier_block notifier;
	const char *notifier_name;

	const char *alias_name;

	struct platform_device *pdev;

	int id;
	int default_on;
	int initialized;
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
static const char *rdev_get_name(struct regulator_dev *rdev)
{
	if (rdev->constraints && rdev->constraints->name)
		return rdev->constraints->name;
	else if (rdev->desc->name)
		return rdev->desc->name;
	else
		return "";
}
#endif

static int is_dummy_regulator(struct regulator *supply)
{
	struct regulator_dev *rdev;
	const struct regulator_desc *desc;
	int ret = 0;

	rdev = supply->rdev;
	desc = rdev->desc;

	//ret = (rdev && rdev != dummy_regulator_rdev) ? 0 : 1;
	ret = (strcmp("regulator-dummy", desc->name) == 0) ? 1 : 0;

	return ret;
}

static struct regulator_dev *get_regulator_rdev(const char *name)
{
	struct generic_supply_data *drvdata;
	int id;

	idr_for_each_entry(&generic_supply_idr, drvdata, id) {
		if (strcmp(name, rdev_get_name(drvdata->rdev)) == 0)
			return drvdata->rdev;
	};

	idr_for_each_entry(&generic_supply_idr, drvdata, id) {
		if (strcmp(name, drvdata->alias_name) == 0)
			return drvdata->rdev;
	};

	return NULL;
}

static void __generic_lcd_supply_delay(unsigned int ms)
{
	unsigned int us = ms * USEC_PER_MSEC;

	if (ms < 20)
		usleep_range(us, us + USEC_PER_MSEC);
	else
		msleep(ms);
}

static int __generic_lcd_supply_reg_write(struct generic_supply_data *drvdata, int index)
{
	int i, ret;

	if (!strlen(drvdata->delay_ms[index]) && !strlen(drvdata->mask[index])) {
		regmap_multi_reg_write(drvdata->rmap, drvdata->regs[index], drvdata->num_regs[index]);
		return 0;
	}

	//todo: mutex, mask
	for (i = 0; i < drvdata->num_regs[index]; i++) {
		ret = regmap_write(drvdata->rmap, drvdata->regs[index][i].reg, drvdata->regs[index][i].def);
		if (ret != 0)
			return ret;

		__generic_lcd_supply_delay(drvdata->delay_ms[index][i]);
	}

	return 0;
}

static u8 get_tuple_value(int tuple_type, u8 *tuple_start, u8 *tuple_offset, int unit_type)
{
	if (unit_type == REG_TUPLE_UNIT_DELAY && !tuple_has_delay(tuple_type))
		return 0;

	if (unit_type == REG_TUPLE_UNIT_MASK && !tuple_has_mask(tuple_type))
		return 0;

	return *(tuple_start + tuple_offset[unit_type]);
}

static int generic_lcd_supply(struct generic_supply_data *drvdata, int index)
{
	int i;
	int num_regs = drvdata->num_regs[index];
	int tuple_type = drvdata->tuple_type;
	int tuple_size = drvdata->tuple_size;
	u8 *tuple_start;

	dbg_none("%s %s\n", drvdata->desc.name, reg_sequence_name[index]);

	if (!drvdata->initialized && !drvdata->default_on) {
		dbg_none("%s %s default_on(%d)\n", drvdata->desc.name, reg_sequence_name[index], drvdata->default_on);
		drvdata->enable_counter = index;
		return 0;
	}

	if (index == GENERIC_SUPPLY_EVENT_ENABLE) {
		for (i = 0; i < drvdata->nr_gpios; i++)
			gpiod_set_value_cansleep(drvdata->gpiods[i], 1);

		if (drvdata->gpio_enable_delay_ms)
			__generic_lcd_supply_delay(drvdata->gpio_enable_delay_ms);
	}

	for (i = 0; i < num_regs; i++) {
		tuple_start = &drvdata->raw_data[index][i * tuple_size];

		drvdata->regs[index][i].reg = get_tuple_value(tuple_type, tuple_start, drvdata->tuple_offset, REG_TUPLE_UNIT_REG);
		drvdata->regs[index][i].def = get_tuple_value(tuple_type, tuple_start, drvdata->tuple_offset, REG_TUPLE_UNIT_VAL);
		drvdata->delay_ms[index][i] = get_tuple_value(tuple_type, tuple_start, drvdata->tuple_offset, REG_TUPLE_UNIT_DELAY);
		drvdata->mask[index][i] = get_tuple_value(tuple_type, tuple_start, drvdata->tuple_offset, REG_TUPLE_UNIT_MASK);

		dbg_none("%s %s: reg(%2x) def(%2x) delay_ms(%d)\n", drvdata->desc.name, reg_sequence_name[index],
			drvdata->regs[index][i].reg, drvdata->regs[index][i].def, drvdata->delay_ms[index][i]);
	}

	if (drvdata->rmap && drvdata->num_regs[index])
		__generic_lcd_supply_reg_write(drvdata, index);

	if (index == GENERIC_SUPPLY_EVENT_DISABLE) {
		for (i = drvdata->nr_gpios - 1; i >= 0; i--)
			gpiod_set_value_cansleep(drvdata->gpiods[i], 0);
	}

	drvdata->enable_counter = index;

	return 0;
}

static int generic_lcd_supply_notifier(struct notifier_block *self,
			unsigned long event, void *unused)
{
	struct generic_supply_data *drvdata =
		container_of(self, struct generic_supply_data, notifier);
	int index;

	switch (event) {
	//not support case REGULATOR_EVENT_PRE_DISABLE:
	case REGULATOR_EVENT_DISABLE:
		index = GENERIC_SUPPLY_EVENT_DISABLE;
		break;
	case REGULATOR_EVENT_ENABLE:
		index = GENERIC_SUPPLY_EVENT_ENABLE;
		break;
	default:
		return NOTIFY_DONE;
	}

	dbg_none("%s %s\n", drvdata->notifier_name, reg_sequence_name[index]);

	generic_lcd_supply(drvdata, index);

	if (index == GENERIC_SUPPLY_EVENT_ENABLE)
		__generic_lcd_supply_delay(DIV_ROUND_UP(drvdata->rdev->constraints->enable_time, USEC_PER_MSEC));

	return NOTIFY_DONE;
}

static int generic_supply_enable(struct regulator_dev *rdev)
{
	struct generic_supply_data *drvdata = rdev_get_drvdata(rdev);

	generic_lcd_supply(drvdata, GENERIC_SUPPLY_EVENT_ENABLE);

	return 0;
}

static int generic_supply_disable(struct regulator_dev *rdev)
{
	struct generic_supply_data *drvdata = rdev_get_drvdata(rdev);

	generic_lcd_supply(drvdata, GENERIC_SUPPLY_EVENT_DISABLE);

	return 0;
}

static int generic_supply_is_enabled(struct regulator_dev *rdev)
{
	struct generic_supply_data *drvdata = rdev_get_drvdata(rdev);

	//dbg_none("%s(%d)\n", rdev_get_name(rdev), drvdata->enable_counter);

	return drvdata->enable_counter > 0;
}

static int set_generic_supply_reg_data(struct device *dev,
			struct generic_supply_data *drvdata, int index,
			u8 *raw_data, int num_data)
{
	int tuple_type, tuple_size;
	int num_regs;
	char *debugfs_name;

	tuple_type = drvdata->tuple_type;
	tuple_size = drvdata->tuple_size;

	if (num_data <= 0 || num_data % tuple_size) {
		dbg_init("%s %s: invalid type(%d) size(%d) num_data(%d)\n",
			drvdata->desc.name, reg_sequence_name[index], tuple_type, tuple_size, num_data);
		return -EINVAL;
	}

	drvdata->num_regs[index] = num_regs = num_data / tuple_size;
	drvdata->raw_data[index] = raw_data;
	drvdata->delay_ms[index] = devm_kcalloc(dev, num_regs, sizeof(u8), GFP_KERNEL);
	drvdata->mask[index] = devm_kcalloc(dev, num_regs, sizeof(u8), GFP_KERNEL);
	drvdata->regs[index] = devm_kcalloc(dev, num_regs, sizeof(struct reg_sequence), GFP_KERNEL);

	dbg_init("%s %s: type(%d) size(%d) num_regs(%d) num_data(%d)\n",
		drvdata->desc.name, reg_sequence_name[index], tuple_type, tuple_size, num_regs, num_data);

	dbg_init("%s %s: %*ph\n", drvdata->desc.name, reg_sequence_name[index], num_data, drvdata->raw_data[index]);

	debugfs_name = kasprintf(GFP_KERNEL, "%s_%s", drvdata->desc.name, reg_sequence_name[index]);

	init_debugfs_param(debugfs_name, drvdata->raw_data[index], U8_MAX, num_data, tuple_size);

	kfree(debugfs_name);

	return 0;
}

int set_reg_sequence_to_supply(const char *id,
			int event, struct reg_sequence *rseq, int num_regs)
{
	struct regulator_dev *rdev;
	struct generic_supply_data *drvdata;
	struct device *dev;
	u8 *raw_data;

	int i;
	int tuple_type, tuple_size, tuple_offset;
	int num_data;
	int index;
	int ret;

	rdev = get_regulator_rdev(id);
	if (!rdev)
		return 0;

	drvdata = rdev_get_drvdata(rdev);
	dev = &rdev->dev;

	switch (event) {
	//not support case REGULATOR_EVENT_PRE_DISABLE:
	case REGULATOR_EVENT_DISABLE:
		index = GENERIC_SUPPLY_EVENT_DISABLE;
		break;
	case REGULATOR_EVENT_ENABLE:
		index = GENERIC_SUPPLY_EVENT_ENABLE;
		break;
	default:
		return NOTIFY_DONE;
	}

	if (drvdata->regs[index]) {
		dbg_init("%s %s reg_sequence exist\n", rdev_get_name(rdev), reg_sequence_name[index]);
		return -EINVAL;
	}

	tuple_type = drvdata->tuple_type;
	tuple_size = drvdata->tuple_size;

	num_data = tuple_size * num_regs;

	raw_data = devm_kcalloc(dev, num_data, sizeof(u8), GFP_KERNEL);

	for (i = 0; i < num_regs; i++) {
		BUG_ON(!tuple_has_delay(tuple_type) && rseq[i].delay_us);

		tuple_offset = (i * tuple_size);
		raw_data[tuple_offset + drvdata->tuple_offset[REG_TUPLE_UNIT_REG]] = rseq[i].reg;
		raw_data[tuple_offset + drvdata->tuple_offset[REG_TUPLE_UNIT_VAL]] = rseq[i].def;

		if (!tuple_has_delay(tuple_type))
			continue;

		raw_data[tuple_offset + drvdata->tuple_offset[REG_TUPLE_UNIT_DELAY]]
			= DIV_ROUND_UP(rseq[i].delay_us, USEC_PER_MSEC);
	}

	ret = set_generic_supply_reg_data(dev, drvdata, index, raw_data, num_data);
	if (ret < 0)
		devm_kfree(dev, raw_data);

	return 0;
}
EXPORT_SYMBOL(set_reg_sequence_to_supply);

int set_reg_data_to_supply(const char *id,
			int event, u8 *data, int num_data)
{
	struct regulator_dev *rdev;
	struct generic_supply_data *drvdata;
	struct device *dev;
	int index;

	rdev = get_regulator_rdev(id);
	if (!rdev)
		return 0;

	drvdata = rdev_get_drvdata(rdev);
	dev = &rdev->dev;

	switch (event) {
	//not support case REGULATOR_EVENT_PRE_DISABLE:
	case REGULATOR_EVENT_DISABLE:
		index = GENERIC_SUPPLY_EVENT_DISABLE;
		break;
	case REGULATOR_EVENT_ENABLE:
		index = GENERIC_SUPPLY_EVENT_ENABLE;
		break;
	default:
		return NOTIFY_DONE;
	}

	if (drvdata->regs[index]) {
		dbg_init("%s %s reg_sequence exist\n", rdev_get_name(rdev), reg_sequence_name[index]);
		return -EINVAL;
	}

	return set_generic_supply_reg_data(dev, drvdata, index, data, num_data);
}
EXPORT_SYMBOL(set_reg_data_to_supply);

static int of_get_tuple_type(struct device *dev)
{
	struct device_node *from = dev->of_node;
	struct device_node *np = NULL;
	int i;

	//dbg_init("%pOF\n", from);

	if (!of_find_property(from, DTS_TUPLE_TYPE, NULL))
		return REG_TUPLE_TYPE_DEFAULT;

	np = of_parse_phandle(from, DTS_TUPLE_TYPE, 0);
	if (!np) {
		dbg_init("invalid phandle(%s) from(%pOF)\n", DTS_TUPLE_TYPE, from);
		BUG();
		return REG_TUPLE_TYPE_DEFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(tuple_type_name); i++) {
		//dbg_init("np->name(%s)\n", np->name);
		if (np->name && of_node_cmp(np->name, tuple_type_name[i]) == 0)
			return i;
	}

	return REG_TUPLE_TYPE_DEFAULT;
}

static int of_get_tuple_size(struct device *dev, u8 *offset)
{
	struct device_node *from = dev->of_node;
	struct device_node *np = NULL;
	int i;
	int count;
	int ret;

	for (i = 0; i < REG_TUPLE_UNIT_MAX; i++)
		offset[i] = i;

	np = of_parse_phandle(from, DTS_TUPLE_TYPE, 0);
	if (!np)
		return REG_TUPLE_SIZE_DEFAULT;

	count = of_property_count_strings(np, DTS_TUPLE_ORDER);
	if (count < REG_TUPLE_SIZE_DEFAULT)
		return 0;

	for (i = 0; i < REG_TUPLE_UNIT_MAX; i++) {
		ret = of_property_match_string(np, DTS_TUPLE_ORDER, tuple_unit_name[i]);
		if (ret < 0 || ret > U8_MAX)
			continue;

		offset[i] = (u8)ret;
	}

	dbg_init("tuple offset is %*ph\n", count, offset);

	return count;
}

static int of_get_generic_supply_regs(struct device *dev,
			struct regulator_config *config)
{
	struct device_node *np = dev->of_node;
	struct generic_supply_data *drvdata = config->driver_data;
	struct regmap *regmap = dev_get_regmap(dev->parent, NULL);
	int i, ret = 0;
	int num_data;
	u8 *raw_data;

	if (!regmap || (regmap && !i2c_verify_client(dev->parent))) {
		dbg_init("%s: no i2c regmap\n", drvdata->desc.name);
		return 0;
	}

	drvdata->tuple_type = of_get_tuple_type(dev);
	drvdata->tuple_size = of_get_tuple_size(dev, drvdata->tuple_offset);

	if (drvdata->tuple_size < REG_TUPLE_TYPE_DEFAULT) {
		dbg_init("%s: invalid tuple_size(%d)\n", drvdata->desc.name, drvdata->tuple_size);
		return ret;
	}

	for (i = 0; i < GENERIC_SUPPLY_EVENT_MAX; i++) {
		if (!of_find_property(np, reg_sequence_name[i], NULL))
			continue;

		num_data = of_property_count_u8_elems(np, reg_sequence_name[i]);
		if (num_data < REG_TUPLE_SIZE_DEFAULT)
			continue;

		raw_data = devm_kcalloc(dev, num_data, sizeof(u8), GFP_KERNEL);

		ret = of_property_read_u8_array(np, reg_sequence_name[i], raw_data, num_data);
		if (ret < 0)
			devm_kfree(dev, raw_data);

		ret = set_generic_supply_reg_data(dev, drvdata, i, raw_data, num_data);
		if (ret < 0)
			devm_kfree(dev, raw_data);
	}

	return ret;
}

static int of_get_generic_supply_gpio(struct device *dev,
			struct regulator_config *config)
{
	int i;
	int ngpios;

	enum gpiod_flags gflags;
	struct generic_supply_data *drvdata = config->driver_data;

	gflags = GPIOD_ASIS;	//GPIOD_FLAGS_BIT_NONEXCLUSIVE;

	ngpios = gpiod_count(dev, NULL);
	if (ngpios <= 0)
		return 0;

	drvdata->gpiods = devm_kzalloc(dev, sizeof(struct gpio_desc *),
				       GFP_KERNEL);
	if (!drvdata->gpiods)
		return -ENOMEM;
	for (i = 0; i < ngpios; i++) {
		drvdata->gpiods[i] = devm_gpiod_get_index(dev,
							NULL,
							i,
							gflags);
		if (IS_ERR(drvdata->gpiods[i]))
			return PTR_ERR(drvdata->gpiods[i]);

		gpiod_set_consumer_name(drvdata->gpiods[i], drvdata->desc.name);
	}
	drvdata->nr_gpios = ngpios;

	dbg_init("ngpios(%d)\n", ngpios);

	return ngpios;
}

static int of_get_generic_supply_named_gpio(struct device *dev,
			struct regulator_config *config)
{
	int i;
	int ngpios;
	int ret;
	int gpio, vgpios = 0;
	const char *gpio_name;

	struct generic_supply_data *drvdata = config->driver_data;
	struct device_node *np = dev->of_node;

	if (!of_find_property(np, DTS_NAMED_GPIOS, NULL))
		return 0;

	if (drvdata->nr_gpios) {
		dbg_init("ngpios(%d) already exist\n");
		return 0;
	}

	ngpios = of_property_count_strings(np, DTS_NAMED_GPIOS);
	if (ngpios <= 0)
		return 0;

	drvdata->gpiods = devm_kzalloc(dev, sizeof(struct gpio_desc *),
					GFP_KERNEL);
	if (!drvdata->gpiods)
		return -ENOMEM;
	for (i = 0; i < ngpios; i++) {
		ret = of_property_read_string_index(np, DTS_NAMED_GPIOS, i, &gpio_name);
		if (ret < 0)
			continue;

		//gpio = of_get_named_gpio_flags(NULL, gpio_name, 0, NULL);
		gpio = of_get_gpio_with_name(gpio_name);
		if (!gpio_is_valid(gpio)) {
			dbg_init("gpio(%d) name(%s) invalid\n", gpio, gpio_name);
			continue;
		}

		drvdata->gpiods[vgpios] = gpio_to_desc(gpio);
		if (IS_ERR(drvdata->gpiods[i])) {
			dbg_init("gpio(%d) name(%s) gpio_to_desc fail\n", gpio, gpio_name);
			return PTR_ERR(drvdata->gpiods[i]);
		}

		vgpios++;

		gpiod_set_consumer_name(drvdata->gpiods[i], drvdata->desc.name);
	}
	drvdata->nr_gpios = vgpios + 1;

	dbg_init("ngpios(%d) vgpios(%d)%s\n", ngpios, drvdata->nr_gpios,
		(ngpios != drvdata->nr_gpios) ? " is different" : "");

	return drvdata->nr_gpios;
}

static int of_get_supply_extra_config(struct device *dev,
			struct regulator_config *config)
{
	struct device_node *np = dev->of_node;
	struct generic_supply_data *drvdata = config->driver_data;
	struct regmap *rmap = dev_get_regmap(dev->parent, NULL);
	struct regulator *notifier_supply;
	const char *notifier_name;

	config->regmap = rmap;
	drvdata->rmap = rmap;

	dbg_init("device(%s) parent(%s)%s\n",
		dev_name(dev), dev_name(dev->parent), dev_get_regmap(dev->parent, NULL) ? " with regmap" : "");

	of_property_read_u32(np, DTS_SUPPLY_DEFAULT_ON, &drvdata->default_on);

	of_get_generic_supply_gpio(dev, config);
	of_get_generic_supply_regs(dev, config);
	of_get_generic_supply_named_gpio(dev, config);

	if (drvdata->nr_gpios)
		of_property_read_u32_index(np, DTS_GPIO_ENABLE_DELAY, 0, &drvdata->gpio_enable_delay_ms);

	if (!of_find_property(np, DTS_REGULATOR_NOTIFIER_WITH, NULL))
		return 0;

	of_property_read_string_index(np, DTS_REGULATOR_NOTIFIER_WITH, 0, &notifier_name);
	if (!notifier_name) {
		dbg_init("notifier supply_name of_property_read_string_index fail\n");
		return 0;
	}

	notifier_supply = regulator_get(NULL, notifier_name);
	if (!notifier_supply) {
		dbg_init("notifier parent(%s) regulator_get fail\n", notifier_name);
		return 0;
	}

	if (is_dummy_regulator(notifier_supply)) {
		dbg_init("notifier parent(%s) is dummy_regulator\n", notifier_name);
		regulator_put(notifier_supply);
		return 0;
	}

	drvdata->notifier_name = notifier_name;
	drvdata->notifier.notifier_call = generic_lcd_supply_notifier;
	drvdata->rdev = notifier_supply->rdev;

	dbg_init("%s regulator_register_notifier with parent(%s)\n",
		drvdata->desc.name, rdev_get_name(notifier_supply->rdev));

	regulator_register_notifier(notifier_supply, &drvdata->notifier);

	regulator_put(notifier_supply);

	return 0;
}

static const struct regulator_ops generic_supply_ops = {
	.enable = generic_supply_enable,
	.disable = generic_supply_disable,
	.is_enabled = generic_supply_is_enabled,
};

int generic_supply_register(struct platform_device *pdev,
		const struct regulator_ops *ops)
{
	struct device *dev = &pdev->dev;
	struct generic_supply_data *drvdata;
	struct regulator_config config = { };
	struct regulator_init_data *init_data;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct generic_supply_data),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		init_data = of_get_regulator_init_data(dev, dev->of_node, &drvdata->desc);
		if (IS_ERR(init_data))
			return PTR_ERR(init_data);
	} else {
		init_data = dev_get_platdata(&pdev->dev);
	}

	if (!init_data)
		return -ENOMEM;

	drvdata->desc.name = devm_kstrdup(&pdev->dev,
					init_data->constraints.name,
					GFP_KERNEL);
	if (drvdata->desc.name == NULL) {
		dev_err(&pdev->dev, "Failed to allocate supply name\n");
		return -ENOMEM;
	}
	drvdata->desc.type = REGULATOR_VOLTAGE;
	drvdata->desc.owner = THIS_MODULE;
	drvdata->desc.ops = ops ? ops : &generic_supply_ops;
	drvdata->pdev = pdev;

	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.driver_data = drvdata;
	config.of_node = pdev->dev.of_node;

	of_get_supply_extra_config(dev, &config);

	platform_set_drvdata(pdev, drvdata);

	if (!drvdata->notifier.notifier_call) {
		char *alias_id;

		drvdata->rdev = devm_regulator_register(&pdev->dev, &drvdata->desc, &config);

		ret = idr_alloc(&generic_supply_idr, drvdata, 0, 0, GFP_KERNEL);
		if (!ret)
			alias_id = kasprintf(GFP_KERNEL, "lcd_supply");
		else
			alias_id = kasprintf(GFP_KERNEL, "lcd_supply.%d", ret);

		drvdata->id = ret;
		drvdata->alias_name = alias_id;

		regulator_register_supply_alias(dev, alias_id, dev, drvdata->desc.name);	/* todo: alias ? */
	}

	if (IS_ERR(drvdata->rdev)) {
		ret = PTR_ERR(drvdata->rdev);
		dev_err(&pdev->dev, "Failed to register regulator: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "%s supplying %duV\n", drvdata->desc.name,
		drvdata->desc.fixed_uV);

	drvdata->initialized = !!get_boot_lcdconnected();

	return 0;
}
EXPORT_SYMBOL(generic_supply_register);

static int generic_supply_probe(struct platform_device *pdev)
{
	return generic_supply_register(pdev, NULL);
}

#if defined(CONFIG_OF)
static const struct of_device_id supply_of_match[] = {
	{
		.compatible = "samsung,generic_lcd_supply",
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, supply_of_match);
#endif

static struct platform_driver generic_supply_driver = {
	.probe		= generic_supply_probe,
	.driver		= {
		.name		= "generic_lcd_supply",
		.of_match_table = of_match_ptr(supply_of_match),
	},
};
builtin_platform_driver(generic_supply_driver);

#if 0
static int __init generic_supply_init(void)
{
	return platform_driver_register(&generic_supply_driver);
}
subsys_initcall(generic_supply_init);

static void __exit generic_supply_exit(void)
{
	platform_driver_unregister(&generic_supply_driver);
}
module_exit(generic_supply_exit);
#endif

