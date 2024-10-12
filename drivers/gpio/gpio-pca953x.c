/*
 *  PCA953x 4/8/16/24/40 bit I/O ports
 *
 *  Copyright (C) 2005 Ben Gardner <bgardner@wabtec.com>
 *  Copyright (C) 2007 Marvell International Ltd.
 *
 *  Derived from drivers/i2c/chips/pca9539.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

#define pr_fmt(fmt) "[GPIO_EXP] " fmt

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/pca953x.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/pm_wakeirq.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include "../pinctrl/core.h"
#include "../pinctrl/pinctrl-utils.h"
#include <linux/delay.h>

#define PCA953X_INPUT		0x0
#define PCA953X_OUTPUT		0x1
#define PCA953X_INVERT		0x2
#define PCA953X_DIRECTION	0x3
#define PCAL953X_IN_LATCH	0x12
#define PCA953X_PULL_ENABLE	0x13
#define PCA953X_PULL_DIR	0x14
#define PCAL953X_INT_MASK	0x15
#define PCAL953X_INT_STAT	0x16
#define PCAL953X_INT_EDGE	0x18

#define REG_ADDR_AI		0x80

#define PCA957X_IN		0
#define PCA957X_INVRT		1
#define PCA957X_BKEN		2
#define PCA957X_PUPD		3
#define PCA957X_CFG		4
#define PCA957X_OUT		5
#define PCA957X_MSK		6
#define PCA957X_INTS		7

#define PCA_GPIO_MASK		0x00FF
#define PCAL_PINCTRL_MASK	0x60
#define PCA_INT			0x0100
#define PCA_PCAL		0x0200
#define PCA953X_TYPE		0x1000
#define PCA957X_TYPE		0x2000
#define PCA_TYPE_MASK		0xF000

#define PCA_CHIP_TYPE(x)	((x) & PCA_TYPE_MASK)

enum EXP_PINCTRL_FUNC {
	PINCTRL_FUNC_INPUT = 0x0,
	PINCTRL_FUNC_INPUT_WAKEUP,
	PINCTRL_FUNC_OUTPUT,
	PINCTRL_FUNC_OUTPUT_LOW,
	PINCTRL_FUNC_OUTPUT_HIGH,
};

enum EXP_PINCTRL_PULL {
	PINCTRL_PULL_NONE = 0x0,
	PINCTRL_PULL_DOWN,
	PINCTRL_PULL_UP,
};

enum EXP_PINCTRL_DRV_STR {
	PINCTRL_DRV_LV1 = 0x0,
	PINCTRL_DRV_LV2,
	PINCTRL_DRV_LV3,
	PINCTRL_DRV_LV4,
};

#define MAX_BANK	5
#define BANK_SZ		8
#define I2C_RETRY_CNT	3

#define NBANK(chip)	DIV_ROUND_UP(chip->gpio_chip.ngpio, BANK_SZ)

struct pca953x_reg_config {
	int direction;
	int output;
	int input;
	int pull_enable;
	int pull_direction;
};

static const struct pca953x_reg_config pca953x_regs = {
	.direction = PCA953X_DIRECTION,
	.output = PCA953X_OUTPUT,
	.input = PCA953X_INPUT,
	.pull_enable = PCA953X_PULL_ENABLE,
	.pull_direction = PCA953X_PULL_DIR,
};

static const struct pca953x_reg_config pca957x_regs = {
	.direction = PCA957X_CFG,
	.output = PCA957X_OUT,
	.input = PCA957X_IN,
};

struct pca953x_chip {
	unsigned int gpio_start;
	u8 reg_output[MAX_BANK];
	u8 reg_direction[MAX_BANK];
	u8 reg_pull_en[MAX_BANK];
	u8 reg_pull_dir[MAX_BANK];
	struct mutex i2c_lock;

#ifdef CONFIG_GPIO_PCA953X_IRQ
	struct mutex irq_lock;
	u8 irq_mask[MAX_BANK];
	u8 irq_stat[MAX_BANK];
	u8 irq_trig_raise[MAX_BANK];
	u8 irq_trig_fall[MAX_BANK];
	u16 irq_trig_type[MAX_BANK];
#endif

	struct i2c_client *client;
	struct gpio_chip gpio_chip;
	const char *const *names;
	unsigned long driver_data;
	struct regulator *regulator;

	const struct pca953x_reg_config *regs;

	int (*write_regs)(struct pca953x_chip *chip, int reg, u8 *val);
	int (*read_regs)(struct pca953x_chip *chip, int reg, u8 *val);

	struct pinctrl_dev *ctrl;
};

static int pca953x_read_single(struct pca953x_chip *chip, int reg, u32 *val,
				int off)
{
	int ret;
	int bank_shift = fls((chip->gpio_chip.ngpio - 1) / BANK_SZ);
	int offset = off / BANK_SZ;
	int i;

	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(chip->client,
					(reg << bank_shift) + offset);

		if (ret >= 0)
			break;
		else
			pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}

	*val = ret;

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed reading register\n");
		return ret;
	}

	return 0;
}

static int pca953x_write_single(struct pca953x_chip *chip, int reg, u32 val,
				int off)
{
	int ret;
	int bank_shift = fls((chip->gpio_chip.ngpio - 1) / BANK_SZ);
	int offset = off / BANK_SZ;
	int i;

	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_byte_data(chip->client,
					(reg << bank_shift) + offset, val);

		if (ret >= 0)
			break;
		else
			pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed writing register\n");
		return ret;
	}

	return 0;
}

static int pca953x_write_regs_8(struct pca953x_chip *chip, int reg, u8 *val)
{
	return i2c_smbus_write_byte_data(chip->client, reg, *val);
}

static int pca953x_write_regs_16(struct pca953x_chip *chip, int reg, u8 *val)
{
	u16 word = get_unaligned((u16 *)val);

	return i2c_smbus_write_word_data(chip->client, reg << 1, word);
}

static int pca957x_write_regs_16(struct pca953x_chip *chip, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg << 1, val[0]);
	if (ret < 0)
		return ret;

	return i2c_smbus_write_byte_data(chip->client, (reg << 1) + 1, val[1]);
}

static int pca953x_write_regs_24(struct pca953x_chip *chip, int reg, u8 *val)
{
	int bank_shift = fls((chip->gpio_chip.ngpio - 1) / BANK_SZ);

	return i2c_smbus_write_i2c_block_data(chip->client,
					      (reg << bank_shift) | REG_ADDR_AI,
					      NBANK(chip), val);
}

static int pca953x_write_regs(struct pca953x_chip *chip, int reg, u8 *val)
{
	int ret = 0;
	int i;

	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = chip->write_regs(chip, reg, val);

		if (ret >= 0)
			break;
		else
			pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed writing register\n");
		return ret;
	}

	return 0;
}

static int pca953x_read_regs_8(struct pca953x_chip *chip, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	*val = ret;

	return ret;
}

static int pca953x_read_regs_16(struct pca953x_chip *chip, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_word_data(chip->client, reg << 1);
	put_unaligned(ret, (u16 *)val);

	return ret;
}

static int pca953x_read_regs_24(struct pca953x_chip *chip, int reg, u8 *val)
{
	int bank_shift = fls((chip->gpio_chip.ngpio - 1) / BANK_SZ);

	return i2c_smbus_read_i2c_block_data(chip->client,
					     (reg << bank_shift) | REG_ADDR_AI,
					     NBANK(chip), val);
}

static int pca953x_read_regs(struct pca953x_chip *chip, int reg, u8 *val)
{
	int ret;
	int i;

	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = chip->read_regs(chip, reg, val);
		if (ret >= 0)
			break;
		else
			pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}

	if (ret < 0) {
		dev_err(&chip->client->dev, "failed reading register 0x%x\n", reg);
		return ret;
	}

	return 0;
}

static int pca953x_gpio_direction_input(struct gpio_chip *gc, unsigned int off)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u8 reg_val;
	int ret;

	mutex_lock(&chip->i2c_lock);
	reg_val = chip->reg_direction[off / BANK_SZ] | (1u << (off % BANK_SZ));

	ret = pca953x_write_single(chip, chip->regs->direction, reg_val, off);
	if (ret)
		goto exit;

	chip->reg_direction[off / BANK_SZ] = reg_val;
exit:
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int pca953x_gpio_direction_output(struct gpio_chip *gc,
		unsigned int off, int val)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u8 reg_val;
	int ret;

	mutex_lock(&chip->i2c_lock);
	/* set output level */
	if (val)
		reg_val = chip->reg_output[off / BANK_SZ]
			| (1u << (off % BANK_SZ));
	else
		reg_val = chip->reg_output[off / BANK_SZ]
			& ~(1u << (off % BANK_SZ));

	ret = pca953x_write_single(chip, chip->regs->output, reg_val, off);
	if (ret)
		goto exit;

	chip->reg_output[off / BANK_SZ] = reg_val;

	/* then direction */
	reg_val = chip->reg_direction[off / BANK_SZ] & ~(1u << (off % BANK_SZ));
	ret = pca953x_write_single(chip, chip->regs->direction, reg_val, off);
	if (ret)
		goto exit;

	chip->reg_direction[off / BANK_SZ] = reg_val;
exit:
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int pca953x_gpio_get_value(struct gpio_chip *gc, unsigned int off)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u32 reg_val;
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = pca953x_read_single(chip, chip->regs->input, &reg_val, off);
	mutex_unlock(&chip->i2c_lock);
	if (ret < 0) {
		/* NOTE:  diagnostic already emitted; that's all we should
		 * do unless gpio_*_value_cansleep() calls become different
		 * from their nonsleeping siblings (and report faults).
		 */
		return 0;
	}

	return (reg_val & (1u << (off % BANK_SZ))) ? 1 : 0;
}

static void pca953x_gpio_set_value(struct gpio_chip *gc, unsigned int off, int val)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u8 reg_val;
	int ret;

	mutex_lock(&chip->i2c_lock);
	if (val)
		reg_val = chip->reg_output[off / BANK_SZ]
			| (1u << (off % BANK_SZ));
	else
		reg_val = chip->reg_output[off / BANK_SZ]
			& ~(1u << (off % BANK_SZ));

	ret = pca953x_write_single(chip, chip->regs->output, reg_val, off);
	if (ret)
		goto exit;

	chip->reg_output[off / BANK_SZ] = reg_val;
exit:
	mutex_unlock(&chip->i2c_lock);
}

static int pca953x_gpio_get_direction(struct gpio_chip *gc, unsigned int off)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u32 reg_val;
	int ret;

	mutex_lock(&chip->i2c_lock);
	ret = pca953x_read_single(chip, chip->regs->direction, &reg_val, off);
	mutex_unlock(&chip->i2c_lock);
	if (ret < 0)
		return ret;

	return !!(reg_val & (1u << (off % BANK_SZ)));
}

static void pca953x_gpio_set_multiple(struct gpio_chip *gc,
				      unsigned long *mask, unsigned long *bits)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	unsigned int bank_mask, bank_val;
	int bank_shift, bank;
	u8 reg_val[MAX_BANK];
	int ret;
	int i;

	bank_shift = fls((chip->gpio_chip.ngpio - 1) / BANK_SZ);

	mutex_lock(&chip->i2c_lock);
	memcpy(reg_val, chip->reg_output, NBANK(chip));
	for (bank = 0; bank < NBANK(chip); bank++) {
		bank_mask = mask[bank / sizeof(*mask)] >>
			   ((bank % sizeof(*mask)) * 8);
		if (bank_mask) {
			bank_val = bits[bank / sizeof(*bits)] >>
				  ((bank % sizeof(*bits)) * 8);
			bank_val &= bank_mask;
			reg_val[bank] = (reg_val[bank] & ~bank_mask) | bank_val;
		}
	}

	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_i2c_block_data(chip->client,
						     chip->regs->output << bank_shift,
						     NBANK(chip), reg_val);
		if (ret >= 0)
			break;
		else
			pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				__func__, chip->regs->output << bank_shift, ret, i + 1, I2C_RETRY_CNT);
	}

	if (ret)
		goto exit;

	memcpy(chip->reg_output, reg_val, NBANK(chip));
exit:
	mutex_unlock(&chip->i2c_lock);
}

static int pca953x_gpio_set_pull_up_down(struct pca953x_chip *chip,
					 unsigned int offset,
					 unsigned long config)
{
	u8 reg_val;
	int ret;

	/*
	 * pull-up/pull-down configuration requires PCAL extended
	 * registers
	 */
	if (!(chip->driver_data & PCA_PCAL))
		return -ENOTSUPP;

	mutex_lock(&chip->i2c_lock);

	/* Disable pull-up/pull-down */
	reg_val = chip->reg_pull_en[offset / BANK_SZ] &= ~(1 << (offset % BANK_SZ));

	ret = pca953x_write_single(chip, chip->regs->pull_enable, reg_val, offset);
	if (ret)
		goto exit;

	chip->reg_pull_en[offset / BANK_SZ] = reg_val;

	if (config != PIN_CONFIG_BIAS_DISABLE) {
		/* Set bit corresponding to offset in pull direction register */
		if (config == PIN_CONFIG_BIAS_PULL_UP)
			reg_val = chip->reg_pull_dir[offset / BANK_SZ] |= 1 << (offset % BANK_SZ);
		else
			reg_val = chip->reg_pull_dir[offset / BANK_SZ] &= ~(1 << (offset % BANK_SZ));

		ret = pca953x_write_single(chip, chip->regs->pull_direction, reg_val, offset);
		if (ret)
			goto exit;

		chip->reg_pull_dir[offset / BANK_SZ] = reg_val;

		/* Enable pull-up/pull-down in case of PULL UP/DOWN */
		reg_val = chip->reg_pull_en[offset / BANK_SZ] |= 1 << (offset % BANK_SZ);

		ret = pca953x_write_single(chip, chip->regs->pull_enable, reg_val, offset);
		if (ret)
			goto exit;

		chip->reg_pull_en[offset / BANK_SZ] = reg_val;
	}
exit:
	mutex_unlock(&chip->i2c_lock);
	return ret;
}

static int pca953x_gpio_set_config(struct gpio_chip *gc, unsigned int offset,
				   unsigned long config)
{
	struct pca953x_chip *chip = gpiochip_get_data(gc);

	switch (config) {
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		return pca953x_gpio_set_pull_up_down(chip, offset, config);
	default:
		return -ENOTSUPP;
	}
}

static void pca953x_setup_gpio(struct pca953x_chip *chip, int gpios)
{
	struct gpio_chip *gc;

	pr_info("[%s]\n", __func__);

	gc = &chip->gpio_chip;

	gc->direction_input  = pca953x_gpio_direction_input;
	gc->direction_output = pca953x_gpio_direction_output;
	gc->get = pca953x_gpio_get_value;
	gc->set = pca953x_gpio_set_value;
	gc->get_direction = pca953x_gpio_get_direction;
	gc->set_multiple = pca953x_gpio_set_multiple;
	gc->set_config = pca953x_gpio_set_config;
	gc->can_sleep = true;

	gc->base = chip->gpio_start;
	gc->ngpio = gpios;
	gc->label = chip->client->name;
	gc->parent = &chip->client->dev;
	gc->owner = THIS_MODULE;
	gc->names = chip->names;
}

static const char *const expander_gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5",
	"gpio6", "gpio7", "gpio8", "gpio9", "gpio10",
	"gpio11", "gpio12", "gpio13", "gpio14", "gpio15",
	"gpio16", "gpio17", "gpio18", "gpio19", "gpio20",
	"gpio21", "gpio22", "gpio23",
};

/* for a custom params */
#define EXAMPLE_GPIO_CONF_PULL_UP			(PIN_CONFIG_END + 1)

static const struct pinconf_generic_params expander_gpio_bindings[] = {
	{"example,gpio-pull-up",	EXAMPLE_GPIO_CONF_PULL_UP,		0},
};


/* To be used with "function" */
#define EXPANDER_GPIO_FUNC_NORMAL		"normal"

enum pmic_gpio_func_index {
	EXPANDER_GPIO_FUNC_INDEX_NORMAL,
};

static const char *const gpio_expander_functions[] = {
	[EXPANDER_GPIO_FUNC_INDEX_NORMAL]	= EXPANDER_GPIO_FUNC_NORMAL,
};

static int gpio_expander_get_groups_count(struct pinctrl_dev *pctldev)
{
	/* Every PIN is a group */
	return pctldev->desc->npins;
}

static const char *gpio_expander_get_group_name(struct pinctrl_dev *pctldev,
					    unsigned pin)
{
	return pctldev->desc->pins[pin].name;
}

static int gpio_expander_get_group_pins(struct pinctrl_dev *pctldev, unsigned pin,
				    const unsigned **pins, unsigned *num_pins)
{
	*pins = &pctldev->desc->pins[pin].number;
	*num_pins = 1;
	return 0;
}

static const struct pinctrl_ops gpio_expander_pinctrl_ops = {
	.get_groups_count	= gpio_expander_get_groups_count,
	.get_group_name		= gpio_expander_get_group_name,
	.get_group_pins		= gpio_expander_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_group,
	.dt_free_map		= pinctrl_utils_free_map,
};

static int gpio_expander_config_group_get(struct pinctrl_dev *pctldev,
				unsigned int pin, unsigned long *config)
{
	pr_info("%s, called!, %d\n", __func__, pin);
	return 0;
}

static int gpio_expander_config_group_set(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long *configs, unsigned nconfs)
{
	struct pca953x_chip *chip = pinctrl_dev_get_drvdata(pctldev);
	struct gpio_chip *gc = &chip->gpio_chip;
	unsigned param, arg;
	int i, ret = 0;

	for (i = 0; i < nconfs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);
		pr_info("%s, called!, %s, param: %d(%s), arg: %d nconfs: %d\n",
				__func__, expander_gpio_groups[pin], param, param_string(param), arg, nconfs);

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
			ret = pca953x_gpio_set_pull_up_down(chip, pin, param);
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			ret = pca953x_gpio_direction_input(gc, pin);
			break;
		case PIN_CONFIG_OUTPUT_ENABLE:
		case PIN_CONFIG_OUTPUT:
			ret = pca953x_gpio_direction_output(gc, pin, arg);
			break;
		}
	}

	return ret;
}

static const struct pinconf_ops gpio_expander_pinconf_ops = {
	.is_generic			= true,
	.pin_config_group_get		= gpio_expander_config_group_get,
	.pin_config_group_set		= gpio_expander_config_group_set,
};

static int gpio_expander_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(gpio_expander_functions);
}

static const char *gpio_expander_get_function_name(struct pinctrl_dev *pctldev,
					       unsigned function)
{
	return gpio_expander_functions[function];
}

static int gpio_expander_get_function_groups(struct pinctrl_dev *pctldev,
					 unsigned function,
					 const char *const **groups,
					 unsigned *const num_qgroups)
{
	*groups = expander_gpio_groups;
	*num_qgroups = pctldev->desc->npins;
	return 0;
}

static int gpio_expander_set_mux(struct pinctrl_dev *pctldev, unsigned function,
				unsigned pin)
{
	/* TODO: writing a mux function */
	pr_info("%s, called!, %d\n", __func__, pin);
	return 0;
}

static const struct pinmux_ops gpio_expander_pinmux_ops = {
	.get_functions_count	= gpio_expander_get_functions_count,
	.get_function_name	= gpio_expander_get_function_name,
	.get_function_groups	= gpio_expander_get_function_groups,
	.set_mux		= gpio_expander_set_mux,
};

#ifdef CONFIG_GPIO_PCA953X_IRQ
static void pca953x_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);

	pr_info("[%s]\n", __func__);

	chip->irq_mask[d->hwirq / BANK_SZ] &= ~(1 << (d->hwirq % BANK_SZ));
}

static void pca953x_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);

	pr_info("[%s]\n", __func__);

	chip->irq_mask[d->hwirq / BANK_SZ] |= 1 << (d->hwirq % BANK_SZ);
}

static int pca953x_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);

	pr_info("[%s]\n", __func__);

	return irq_set_irq_wake(chip->client->irq, on);
}

static void pca953x_irq_bus_lock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);

	pr_info("[%s]\n", __func__);

	mutex_lock(&chip->irq_lock);
}

static void pca953x_irq_bus_sync_unlock(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	u8 new_irqs;
	int level, i;
	int bank_nb = d->hwirq / BANK_SZ, ret;
	u8 invert_irq_mask[MAX_BANK];
	u16 reg_val;

	pr_info("[%s]\n", __func__);

	if (chip->driver_data & PCA_PCAL) {
		/* Enable latch on interrupt-enabled inputs */
		pca953x_write_regs(chip, PCAL953X_IN_LATCH, chip->irq_mask);

		for (i = 0; i < NBANK(chip); i++)
			invert_irq_mask[i] = ~chip->irq_mask[i];

		/* Unmask enabled interrupts */
		pca953x_write_regs(chip, PCAL953X_INT_MASK, invert_irq_mask);
	}

	/* Look for any newly setup interrupt */
	for (i = 0; i < NBANK(chip); i++) {
		new_irqs = chip->irq_trig_fall[i] | chip->irq_trig_raise[i];
		new_irqs &= ~chip->reg_direction[i];

		while (new_irqs) {
			level = __ffs(new_irqs);
			pca953x_gpio_direction_input(&chip->gpio_chip,
							level + (BANK_SZ * i));
			new_irqs &= ~(1 << level);
		}
	}

	if (d->hwirq % BANK_SZ > 3) /* upper 8 bits */
		reg_val = chip->irq_trig_type[bank_nb] >> 8;
	else /* lower 8 bits */
		reg_val = chip->irq_trig_type[bank_nb] & 0xFF;

	ret = pca953x_write_single(chip, PCAL953X_INT_EDGE, (u8)reg_val, (d->hwirq) * 2);
	if (ret)
		pr_info("[%s] failed to write reg\n", __func__);

	mutex_unlock(&chip->irq_lock);
}

static int pca953x_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct pca953x_chip *chip = gpiochip_get_data(gc);
	int bank_nb = d->hwirq / BANK_SZ;
	u8 mask = 1 << (d->hwirq % BANK_SZ);

	pr_info("[%s] idx: %d, type: %d\n", __func__, d->hwirq, type);

	if (!(type & (IRQ_TYPE_EDGE_BOTH | IRQ_TYPE_LEVEL_MASK))) {
		dev_err(&chip->client->dev, "irq %d: unsupported type %d\n",
			d->irq, type);
		return -EINVAL;
	}

	if ((type & IRQ_TYPE_EDGE_FALLING) || (type & IRQ_TYPE_LEVEL_LOW))
		chip->irq_trig_fall[bank_nb] |= mask;
	else
		chip->irq_trig_fall[bank_nb] &= ~mask;

	if ((type & IRQ_TYPE_EDGE_RISING) || (type & IRQ_TYPE_LEVEL_HIGH))
		chip->irq_trig_raise[bank_nb] |= mask;
	else
		chip->irq_trig_raise[bank_nb] &= ~mask;

	if (type & IRQ_TYPE_EDGE_BOTH) {
		if (type & IRQ_TYPE_EDGE_FALLING) {
			pr_info("[%s] falling edge\n", __func__);
			chip->irq_trig_type[bank_nb] |= IRQ_TYPE_EDGE_FALLING << (d->hwirq % BANK_SZ) * 2;
		}
		if (type & IRQ_TYPE_EDGE_RISING) {
			pr_info("[%s] rising edge\n", __func__);
			chip->irq_trig_type[bank_nb] |= IRQ_TYPE_EDGE_RISING << (d->hwirq % BANK_SZ) * 2;
		}
	}

	return 0;
}

static struct irq_chip pca953x_irq_chip = {
	.name			= "gpio expander",
	.irq_mask		= pca953x_irq_mask,
	.irq_unmask		= pca953x_irq_unmask,
	.irq_set_wake		= pca953x_irq_set_wake,
	.irq_bus_lock		= pca953x_irq_bus_lock,
	.irq_bus_sync_unlock	= pca953x_irq_bus_sync_unlock,
	.irq_set_type		= pca953x_irq_set_type,
};

static bool pca953x_irq_pending(struct pca953x_chip *chip, u8 *pending)
{
	u8 cur_stat[MAX_BANK];
	u8 old_stat[MAX_BANK];
	bool pending_seen = false;
	bool trigger_seen = false;
	u8 trigger[MAX_BANK];
	int ret, i;

	pr_info("[%s]\n", __func__);

	if (chip->driver_data & PCA_PCAL) {
		/* Read the current interrupt status from the device */
		ret = pca953x_read_regs(chip, PCAL953X_INT_STAT, trigger);
		if (ret)
			return false;

		/* Check latched inputs and clear interrupt status */
		ret = pca953x_read_regs(chip, PCA953X_INPUT, cur_stat);
		if (ret)
			return false;

		for (i = 0; i < NBANK(chip); i++) {
			/* Apply filter for rising/falling edge selection */
			pending[i] = (~cur_stat[i] & chip->irq_trig_fall[i]) |
				(cur_stat[i] & chip->irq_trig_raise[i]);
			pending[i] &= trigger[i];
			if (pending[i])
				pending_seen = true;
		}

		return pending_seen;
	}

	ret = pca953x_read_regs(chip, chip->regs->input, cur_stat);
	if (ret)
		return false;

	/* Remove output pins from the equation */
	for (i = 0; i < NBANK(chip); i++)
		cur_stat[i] &= chip->reg_direction[i];

	memcpy(old_stat, chip->irq_stat, NBANK(chip));

	for (i = 0; i < NBANK(chip); i++) {
		trigger[i] = (cur_stat[i] ^ old_stat[i]) & chip->irq_mask[i];
		if (trigger[i])
			trigger_seen = true;
	}

	if (!trigger_seen)
		return false;

	memcpy(chip->irq_stat, cur_stat, NBANK(chip));

	for (i = 0; i < NBANK(chip); i++) {
		pending[i] = (old_stat[i] & chip->irq_trig_fall[i]) |
			(cur_stat[i] & chip->irq_trig_raise[i]);
		pending[i] &= trigger[i];
		if (pending[i])
			pending_seen = true;
	}

	return pending_seen;
}

static irqreturn_t pca953x_irq_handler(int irq, void *devid)
{
	struct pca953x_chip *chip = devid;
	struct gpio_chip *gc = &chip->gpio_chip;
	u8 pending[MAX_BANK];
	u8 level;
	unsigned int nhandled = 0;
	int i;

	pr_info("[%s]\n", __func__);

	if (!pca953x_irq_pending(chip, pending))
		return IRQ_NONE;

	for (i = 0; i < NBANK(chip); i++) {
		while (pending[i]) {
			level = __ffs(pending[i]);
			handle_nested_irq(irq_find_mapping(gc->irq.domain,
							level + (BANK_SZ * i)));
			pending[i] &= ~(1 << level);
			nhandled++;
		}
	}

	return (nhandled > 0) ? IRQ_HANDLED : IRQ_NONE;
}

static int pca953x_irq_setup(struct pca953x_chip *chip,
			     int irq_base)
{
	struct i2c_client *client = chip->client;
	int ret, i;
	u32 reg_val;

	pr_info("[%s]\n", __func__);

	/* trigger type register setting uses 2bit per pin,
	 * so use u16 to save interrupt edge port register status(0x60 ~ 0x65)
	 *
	 * irq_trig_type[x] -> BANK x(Px_0 ~ Px_7)
	 *
	 * irq edge port xB reg | irq edge port xA reg
	 * Px_7 Px_6 Px_5 Px_4  |  Px_3 Px_2 Px_1 Px_0
	 *  00 | 00 | 00 | 00   |   00 | 00 | 00 | 00
	 *
	 * initialize these registers by reading it.
	 */

	for (i = 0; i < NBANK(chip) * 2; i++) {
		pca953x_read_single(chip, PCAL953X_INT_EDGE, &reg_val, i * BANK_SZ);

		if (i % 2 == 1)
			chip->irq_trig_type[i/2] |= reg_val << 8;
		else
			chip->irq_trig_type[i/2] |= reg_val;
	}

	if (client->irq && irq_base != -1
			&& (chip->driver_data & PCA_INT)) {
		ret = pca953x_read_regs(chip,
					chip->regs->input, chip->irq_stat);
		if (ret)
			return ret;

		/*
		 * There is no way to know which GPIO line generated the
		 * interrupt.  We have to rely on the previous read for
		 * this purpose.
		 */
		for (i = 0; i < NBANK(chip); i++)
			chip->irq_stat[i] &= chip->reg_direction[i];
		mutex_init(&chip->irq_lock);

		ret = devm_request_threaded_irq(&client->dev,
					client->irq,
					   NULL,
					   pca953x_irq_handler,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT |
						   IRQF_SHARED,
					   "gpio expander", chip);
		if (ret) {
			dev_err(&client->dev, "failed to request irq %d\n",
				client->irq);
			return ret;
		}

		ret =  gpiochip_irqchip_add_nested(&chip->gpio_chip,
						   &pca953x_irq_chip,
						   irq_base,
						   handle_simple_irq,
						   IRQ_TYPE_NONE);
		if (ret) {
			dev_err(&client->dev,
				"could not connect irqchip to gpiochip\n");
			return ret;
		}

		gpiochip_set_nested_irqchip(&chip->gpio_chip,
					    &pca953x_irq_chip,
					    client->irq);
	}

	return 0;
}

#else /* CONFIG_GPIO_PCA953X_IRQ */
static int pca953x_irq_setup(struct pca953x_chip *chip,
			     int irq_base)
{
	struct i2c_client *client = chip->client;

	if (irq_base != -1 && (chip->driver_data & PCA_INT))
		dev_warn(&client->dev, "interrupt support not compiled in\n");

	return 0;
}
#endif

static int device_pca953x_init(struct pca953x_chip *chip, u32 invert)
{
	int ret;
	u8 val[MAX_BANK];

	pr_info("[%s]\n", __func__);

	chip->regs = &pca953x_regs;

	ret = pca953x_read_regs(chip, chip->regs->output, chip->reg_output);
	if (ret)
		goto out;

	ret = pca953x_read_regs(chip, chip->regs->direction,
				chip->reg_direction);
	if (ret)
		goto out;

	ret = pca953x_read_regs(chip, chip->regs->pull_enable,
				chip->reg_pull_en);
	if (ret)
		goto out;

	ret = pca953x_read_regs(chip, chip->regs->pull_direction,
				chip->reg_pull_dir);
	if (ret)
		goto out;

	/* set platform specific polarity inversion */
	if (invert)
		memset(val, 0xFF, NBANK(chip));
	else
		memset(val, 0, NBANK(chip));

	ret = pca953x_write_regs(chip, PCA953X_INVERT, val);
out:
	return ret;
}

static int device_pca957x_init(struct pca953x_chip *chip, u32 invert)
{
	int ret;
	u8 val[MAX_BANK];

	chip->regs = &pca957x_regs;

	ret = pca953x_read_regs(chip, chip->regs->output, chip->reg_output);
	if (ret)
		goto out;
	ret = pca953x_read_regs(chip, chip->regs->direction,
				chip->reg_direction);
	if (ret)
		goto out;

	/* set platform specific polarity inversion */
	if (invert)
		memset(val, 0xFF, NBANK(chip));
	else
		memset(val, 0, NBANK(chip));
	ret = pca953x_write_regs(chip, PCA957X_INVRT, val);
	if (ret)
		goto out;

	/* To enable register 6, 7 to control pull up and pull down */
	memset(val, 0x02, NBANK(chip));
	ret = pca953x_write_regs(chip, PCA957X_BKEN, val);
	if (ret)
		goto out;

	return 0;
out:
	return ret;
}

static const struct of_device_id pca953x_dt_ids[];

#if defined(CONFIG_OF)
static struct pca953x_platform_data *of_pca953x_parse_dt(struct device *dev)
{
	struct device_node *np;
	struct pca953x_platform_data *pdata;
	u32 temp;
	int ret;

	pr_info("[%s]\n", __func__);

	pdata = devm_kzalloc(dev, sizeof(struct pca953x_platform_data),
			GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	np = dev->of_node;
	if (np == NULL) {
		pr_err("%s: error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

#ifdef CONFIG_GPIO_PCA953X_IRQ
	pdata->irq_gpio = of_get_named_gpio(np, "pca953x,irq-gpio", 0);
	pr_info("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio);
#endif
	pdata->pba_conn_det_gpio = of_get_named_gpio(np, "pca953x,pba_conn_det_gpio", 0);
	if (pdata->pba_conn_det_gpio >= 0)
		pr_info("%s: pba_conn_det_gpio (%d)\n", __func__, pdata->pba_conn_det_gpio);
	else
		pr_info("%s pba_conn_det_gpio not specified\n", __func__);

	ret = of_property_read_u32(np, "pca953x,gpio_start", &temp);
	if (ret) {
		pr_info("%s: gpio base isn't specified\n", __func__);
		pdata->gpio_base = 0;
	} else
		pdata->gpio_base = (int)temp;

	ret = of_property_read_u32(np, "pca953x,gpio_num", &temp);
	if (ret) {
		pr_info("%s: gpio number isn't specified\n", __func__);
		pdata->gpio_num = 0;
	} else
		pdata->gpio_num = (int)temp;

	pr_info("[%s] gpio_base: %d, gpio_num: %d\n", __func__, pdata->gpio_base, pdata->gpio_num);

#if IS_ENABLED(CONFIG_GPIO_PCA953X_RESET)
	pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (pdata->reset_gpio >= 0) {
		pr_info("[%s] reset gpio: %d, start reset\n", __func__, pdata->reset_gpio);
		
		gpio_direction_output(pdata->reset_gpio, 0);
		usleep_range(100, 101);
		gpio_direction_output(pdata->reset_gpio, 1);
	}
#endif

	return pdata;

err_parsing_dt:
	devm_kfree(dev, pdata);
	return NULL;
}
#endif

static int pca953x_probe(struct i2c_client *client,
				   const struct i2c_device_id *i2c_id)
{
	struct pca953x_platform_data *pdata;
	struct pca953x_chip *chip;
	int irq_base = 0;
	int ret;
	u32 invert = 0;
	struct regulator *reg;
	struct pinctrl_pin_desc *pindesc;
	struct pinctrl_desc *pctrldesc;
	int npins, pin_idx;

	pr_info("[%s]\n", __func__);

	chip = devm_kzalloc(&client->dev,
			sizeof(struct pca953x_chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

#if defined(CONFIG_OF)
	pdata = of_pca953x_parse_dt(&client->dev);
#endif
	if (pdata) {
		irq_base = pdata->irq_base;
		chip->gpio_start = pdata->gpio_base;
		invert = pdata->invert;
		chip->names = pdata->names;
	} else {
		struct gpio_desc *reset_gpio;

		chip->gpio_start = -1;
		irq_base = 0;

		/*
		 * See if we need to de-assert a reset pin.
		 *
		 * There is no known ACPI-enabled platforms that are
		 * using "reset" GPIO. Otherwise any of those platform
		 * must use _DSD method with corresponding property.
		 */
		reset_gpio = devm_gpiod_get_optional(&client->dev, "reset",
						     GPIOD_OUT_LOW);
		if (IS_ERR(reset_gpio))
			return PTR_ERR(reset_gpio);
	}

	chip->client = client;

	reg = devm_regulator_get(&client->dev, "pcal6524,vdd");
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		if (ret != -EPROBE_DEFER)
			dev_err(&client->dev, "reg get err: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg);
	if (ret) {
		dev_err(&client->dev, "reg en err: %d\n", ret);
		return ret;
	}
	chip->regulator = reg;

	chip->driver_data = (uintptr_t)of_device_get_match_data(&client->dev);

	mutex_init(&chip->i2c_lock);
	/*
	 * In case we have an i2c-mux controlled by a GPIO provided by an
	 * expander using the same driver higher on the device tree, read the
	 * i2c adapter nesting depth and use the retrieved value as lockdep
	 * subclass for chip->i2c_lock.
	 *
	 * REVISIT: This solution is not complete. It protects us from lockdep
	 * false positives when the expander controlling the i2c-mux is on
	 * a different level on the device tree, but not when it's on the same
	 * level on a different branch (in which case the subclass number
	 * would be the same).
	 *
	 * TODO: Once a correct solution is developed, a similar fix should be
	 * applied to all other i2c-controlled GPIO expanders (and potentially
	 * regmap-i2c).
	 */
	lockdep_set_subclass(&chip->i2c_lock,
			     i2c_adapter_depth(client->adapter));

	/* initialize cached registers from their original values.
	 * we can't share this chip with another i2c master.
	 */
	pca953x_setup_gpio(chip, chip->driver_data & PCA_GPIO_MASK);

	if (chip->gpio_chip.ngpio <= 8) {
		chip->write_regs = pca953x_write_regs_8;
		chip->read_regs = pca953x_read_regs_8;
	} else if (chip->gpio_chip.ngpio >= 24) {
		chip->write_regs = pca953x_write_regs_24;
		chip->read_regs = pca953x_read_regs_24;
	} else {
		if (PCA_CHIP_TYPE(chip->driver_data) == PCA953X_TYPE)
			chip->write_regs = pca953x_write_regs_16;
		else
			chip->write_regs = pca957x_write_regs_16;
		chip->read_regs = pca953x_read_regs_16;
	}

	if (PCA_CHIP_TYPE(chip->driver_data) == PCA953X_TYPE)
		ret = device_pca953x_init(chip, invert);
	else
		ret = device_pca957x_init(chip, invert);
	if (ret)
		goto err_exit;

	ret = devm_gpiochip_add_data(&client->dev, &chip->gpio_chip, chip);
	if (ret)
		goto err_exit;

#ifdef CONFIG_GPIO_PCA953X_IRQ
	if (pdata)
		client->irq = gpio_to_irq(pdata->irq_gpio);
#endif
	ret = pca953x_irq_setup(chip, irq_base);
	if (ret)
		goto err_exit;

	if (pdata && pdata->setup) {
		ret = pdata->setup(client, chip->gpio_chip.base,
				chip->gpio_chip.ngpio, pdata->context);
		if (ret < 0)
			dev_warn(&client->dev, "setup failed, %d\n", ret);
	}

	/* Register pinctrl driver */
	npins = chip->gpio_chip.ngpio;

	pindesc = devm_kcalloc(&client->dev, npins, sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc) {
		ret = -ENOMEM;
		goto err_exit;
	}

	pctrldesc = devm_kzalloc(&client->dev, sizeof(*pctrldesc), GFP_KERNEL);
	if (!pctrldesc) {
		ret = -ENOMEM;
		kfree(pindesc);
		goto err_exit;
	}

	pctrldesc->pctlops = &gpio_expander_pinctrl_ops;
	pctrldesc->pmxops = &gpio_expander_pinmux_ops;
	pctrldesc->confops = &gpio_expander_pinconf_ops;
	pctrldesc->owner = THIS_MODULE;
	pctrldesc->name = dev_name(&client->dev);
	pctrldesc->pins = pindesc;
	pctrldesc->npins = npins;
	/* NOTE: If you need additional custom params for pin configuration */
	/* pctrldesc->num_custom_params = ARRAY_SIZE(expander_gpio_bindings); */
	/* pctrldesc->custom_params = expander_gpio_bindings; */
#ifdef CONFIG_DEBUG_FS
	/* pctrldesc->custom_conf_items = expander_conf_items; */
#endif
	for (pin_idx = 0; pin_idx < npins; pin_idx++, pindesc++) {
		pindesc->number = pin_idx;
		pindesc->name = expander_gpio_groups[pin_idx];
	}

	chip->ctrl = devm_pinctrl_register(&client->dev, pctrldesc, chip);
	if (IS_ERR(chip->ctrl)) {
		ret = PTR_ERR(chip->ctrl);
		dev_err(&client->dev, "failed to register pinctrl device, ret=%d\n", ret);
	}

	i2c_set_clientdata(client, chip);

#ifdef CONFIG_GPIO_PCA953X_IRQ
	irq_set_irq_wake(client->irq, 1);
#endif
	pr_info("[%s done]\n", __func__);

	return 0;

err_exit:
	regulator_disable(chip->regulator);

	/* MAIN/SUB PBA isn't connected in SMD/SUB ASSAY FT, so expander probing fails.
	 * This causes other modules probe failure which is attached to expander(module dependency)
	 * So we made a W/A to check a gpio which indicates if PBA is connected or not.
	 * If this gpio is high, this means it is SMD or SUB ASSAY FT so we just return 0.
	 */

	if (pdata->pba_conn_det_gpio >= 0 && gpio_get_value(pdata->pba_conn_det_gpio)) {
		pr_info("%s this is SMD/SUB assay test, W/A to skip expander probe fail\n", __func__);
		return 0;
	} else {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		panic("expander probe fail, please check device with HW team");
#else
		pr_err("%s: expander probe fail\n", __func__);
#endif
		return ret;
	}
}

static int pca953x_remove(struct i2c_client *client)
{
	struct pca953x_platform_data *pdata = dev_get_platdata(&client->dev);
	struct pca953x_chip *chip = i2c_get_clientdata(client);
	int ret;

	if (pdata && pdata->teardown) {
		ret = pdata->teardown(client, chip->gpio_chip.base,
				chip->gpio_chip.ngpio, pdata->context);
		if (ret < 0)
			dev_err(&client->dev, "%s failed, %d\n",
					"teardown", ret);
	} else {
		ret = 0;
	}

	regulator_disable(chip->regulator);

	return ret;
}

/* convenience to stop overlong match-table lines */
#define OF_953X(__nrgpio, __int) (void *)(__nrgpio | PCA953X_TYPE | __int)
#define OF_957X(__nrgpio, __int) (void *)(__nrgpio | PCA957X_TYPE | __int)

static const struct of_device_id pca953x_dt_ids[] = {
	{ .compatible = "nxp,pca953x", .data = OF_953X(24, PCA_INT|PCA_PCAL), },
	{ }
};

MODULE_DEVICE_TABLE(of, pca953x_dt_ids);

static struct i2c_driver pca953x_driver = {
	.driver = {
		.name	= "pcal6524",
		.of_match_table = pca953x_dt_ids,
	},
	.probe		= pca953x_probe,
	.remove		= pca953x_remove,
};

static int __init pca953x_init(void)
{
	pr_info("[%s]\n", __func__);
	return i2c_add_driver(&pca953x_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(pca953x_init);
MODULE_DESCRIPTION("GPIO expander driver for PCA953x");
MODULE_LICENSE("GPL");
