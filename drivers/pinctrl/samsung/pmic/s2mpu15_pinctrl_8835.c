// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 */

#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/../../drivers/pinctrl/core.h>
#include <linux/../../drivers/pinctrl/pinctrl-utils.h>

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif
#include <linux/samsung/pmic/s2mpu15.h>
#include <linux/samsung/pmic/s2mpu15-regulator-8835.h>

/* PMIC ADDRESS: OOTP1_bitmap */
#define S2MPU15_PM1_GPIO0_SET			0xEB // Check bitmap and name
#define S2MPU15_PM1_GPIO1_SET			0xEC
#define S2MPU15_PM1_GPIO_STATUS			0xEE
#define S2MPU15_PM1_GPIO_INTM			0xEF
#define S2MPU15_PM1_GPIO_INT			0xF0

/* Samsung specific pin configurations */
#define PMIC_GPIO_OEN_SHIFT			(6)
#define PMIC_GPIO_OEN_MASK			(0x01 << PMIC_GPIO_OEN_SHIFT)

#define PMIC_GPIO_OUT_SHIFT			(5)
#define PMIC_GPIO_OUT_MASK			(0x01 << PMIC_GPIO_OUT_SHIFT)

#define PMIC_GPIO_PULL_SHIFT			(3)
#define PMIC_GPIO_PULL_MASK			(0x03 << PMIC_GPIO_PULL_SHIFT)

#define PMIC_GPIO_DRV_STR_SHIFT			(0)
#define PMIC_GPIO_DRV_STR_MASK			(0x07 << PMIC_GPIO_DRV_STR_SHIFT)

/* Samsung possible pin configuration parameters */
#define PMIC_GPIO_CONF_DISABLE                  (PIN_CONFIG_END + 1)
#define PMIC_GPIO_CONF_PULL_DOWN                (PIN_CONFIG_END + 2)
#define PMIC_GPIO_CONF_PULL_UP                  (PIN_CONFIG_END + 3)
#define PMIC_GPIO_CONF_INPUT_ENABLE             (PIN_CONFIG_END + 4)
#define PMIC_GPIO_CONF_OUTPUT_ENABLE            (PIN_CONFIG_END + 5)
#define PMIC_GPIO_CONF_OUTPUT                   (PIN_CONFIG_END + 6)
#define PMIC_GPIO_CONF_DRIVE_STRENGTH           (PIN_CONFIG_END + 7)

enum pmic_gpio_io_param {
	PMIC_GPIO_MODE_DIGITAL_INPUT = (0 << PMIC_GPIO_OEN_SHIFT),
	PMIC_GPIO_MODE_DIGITAL_OUTPUT = (1 << PMIC_GPIO_OEN_SHIFT),
};

enum pmic_gpio_pull_param {
	PMIC_GPIO_PULL_DISABLE = (0 << PMIC_GPIO_PULL_SHIFT),
	PMIC_GPIO_PULL_DOWN = (1 << PMIC_GPIO_PULL_SHIFT),
	PMIC_GPIO_PULL_UP = (2 << PMIC_GPIO_PULL_SHIFT),
};

static const char *const biases[] = {
	"no-pull", "pull-down", "pull-up"
};

static const char *const strengths[] = {
	"1mA", "2mA", "3mA", "4mA", "5mA", "6mA"
};

static const struct pinconf_generic_params pmic_gpio_bindings[] = {
	{"pmic-gpio,pull-disable",	PMIC_GPIO_CONF_DISABLE,		0},
	{"pmic-gpio,pull-down",		PMIC_GPIO_CONF_PULL_DOWN,	1},
	{"pmic-gpio,pull-up",		PMIC_GPIO_CONF_PULL_UP,		2},
	{"pmic-gpio,input-enable",	PMIC_GPIO_CONF_INPUT_ENABLE,	0},
	{"pmic-gpio,output-enable",	PMIC_GPIO_CONF_OUTPUT_ENABLE,	1},
	{"pmic-gpio,output-low",	PMIC_GPIO_CONF_OUTPUT,		0},
	{"pmic-gpio,output-high",	PMIC_GPIO_CONF_OUTPUT,		1},
	{"pmic-gpio,drive-strength",	PMIC_GPIO_CONF_DRIVE_STRENGTH,	0},
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item pmic_conf_items[ARRAY_SIZE(pmic_gpio_bindings)] = {
	PCONFDUMP(PMIC_GPIO_CONF_DISABLE,	"pull-disable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_PULL_DOWN,	"pull-down", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_PULL_UP,	"pull-up", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_INPUT_ENABLE,	"input-enable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_OUTPUT_ENABLE,	"output-enable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_OUTPUT,	"output-high", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_DRIVE_STRENGTH,"drive-strength", NULL, true),
};
#endif

struct pmic_gpio_state {
	struct device *dev;
	struct pinctrl_dev *ctrl;
	struct gpio_chip chip;
	struct irq_chip irq;
	struct i2c_client *i2c;
	uint32_t npins;
};

/*
 * struct pmic_gpio_pad - keep current GPIO settings
 * @output_enabled: Set to true if GPIO output buffer logic is enabled.
 * @output: Cached pin output value
 * @pull: Constant current which flow trough GPIO output buffer.
 * @strength: 1mA, 2mA, 3mA, 4mA, 5mA, 6mA (from 0 to 5)
 * @function: See pmic_gpio_functions[]
 */
struct pmic_gpio_pad {
	bool		output_enabled;
	uint32_t	output;
	uint32_t	pull;
	uint32_t	strength;
	uint32_t	function;
};

static const char *const pmic_gpio_groups[] = {
	"gpio_m0", "gpio_m1",
};

static struct pinctrl_pin_desc pmic_pin_desc[] = {
	PINCTRL_PIN(0, "gpio_m0"),
	PINCTRL_PIN(1, "gpio_m1"),
};

/* The index of each function in pmic_gpio_functions[] array */
#define PMIC_GPIO_FUNC_NORMAL		"normal"

enum pmic_gpio_func_index {
	PMIC_GPIO_FUNC_INDEX_NORMAL,
};

static const char *const pmic_gpio_functions[] = {
	[PMIC_GPIO_FUNC_INDEX_NORMAL]	= PMIC_GPIO_FUNC_NORMAL,
};

static int pmic_gpio_read(const struct pmic_gpio_state *state, const uint8_t addr)
{
	uint8_t val = 0;
	int ret = 0;

	ret = s2mpu15_read_reg(state->i2c, addr, &val);
	if (ret)
		dev_err(state->dev, "[MAIN_PMIC] %s: read %#x failed\n", __func__, addr);
	else
		ret = val;

	return ret;
}

static int pmic_gpio_write(struct pmic_gpio_state *state, const uint8_t addr, const uint8_t val)
{
	return s2mpu15_write_reg(state->i2c, addr, val);
}

static bool get_gpio_mode(const int val)
{
	return ((val & PMIC_GPIO_OEN_MASK) >> PMIC_GPIO_OEN_SHIFT) ? true : false;
}

static uint32_t get_gpio_output(const uint32_t pin, const int val)
{
	return (val >> pin) & 0x1;
}

static uint32_t get_gpio_pull(const int val)
{
	return ((val & PMIC_GPIO_PULL_MASK) >> PMIC_GPIO_PULL_SHIFT);
}

static uint32_t get_gpio_strength(const int val)
{
	return ((val & PMIC_GPIO_DRV_STR_MASK) >> PMIC_GPIO_DRV_STR_SHIFT);
}

static uint8_t get_set_reg(const uint32_t pin)
{
	return S2MPU15_PM1_GPIO0_SET + pin;
}

static uint8_t get_status_reg(void)
{
	return S2MPU15_PM1_GPIO_STATUS;
}

static int get_pmic_gpio_info(struct pmic_gpio_state *state, const uint32_t pin)
{
	struct pmic_gpio_pad *pad = pmic_pin_desc[pin].drv_data;
	const uint8_t reg_set = get_set_reg(pin);
	const uint8_t reg_status = get_status_reg();
	int val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	pad->output_enabled = get_gpio_mode(val_set);
	pad->output = get_gpio_output(pin, val_status);
	pad->pull = get_gpio_pull(val_set);
	pad->strength = get_gpio_strength(val_set);

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: (%#02hhx:%#02hhx), (%#02hhx:%#02hhx)\n",
				__func__, pin, reg_status, val_status, reg_set, val_set);

	return 0;
}

static int pmic_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	return pctldev->desc->npins;
}

static const char *pmic_gpio_get_group_name(struct pinctrl_dev *pctldev,
					    uint32_t pin)
{
	return pctldev->desc->pins[pin].name;
}

static int pmic_gpio_get_group_pins(struct pinctrl_dev *pctldev, uint32_t pin,
				    const uint32_t **pins, uint32_t *num_pins)
{
	*pins = &pctldev->desc->pins[pin].number;
	*num_pins = 1;

	return 0;
}

static void pmic_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;

	if (get_pmic_gpio_info(state, pin) < 0) {
		seq_printf(s, "get_pmic_gpio_info fail(%d)", __LINE__);
		return;
	}

	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);
}

static const struct pinctrl_ops pmic_gpio_pinctrl_ops = {
	.get_groups_count	= pmic_gpio_get_groups_count,
	.get_group_name		= pmic_gpio_get_group_name,
	.get_group_pins		= pmic_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= pmic_gpio_pin_dbg_show,
};

static int pmic_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(pmic_gpio_functions);
}

static const char *pmic_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       uint32_t function)
{
	return pmic_gpio_functions[function];
}

static int pmic_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 uint32_t function,
					 const char *const **groups,
					 uint32_t *const num_qgroups)
{
	*groups = pmic_gpio_groups;
	*num_qgroups = pctldev->desc->npins;

	return 0;
}

static int pmic_gpio_set_mux(struct pinctrl_dev *pctldev, uint32_t function,
				uint32_t pin)
{
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;

	pad->function = function;

	return 0;
}

static const struct pinmux_ops pmic_gpio_pinmux_ops = {
	.get_functions_count	= pmic_gpio_get_functions_count,
	.get_function_name	= pmic_gpio_get_function_name,
	.get_function_groups	= pmic_gpio_get_function_groups,
	.set_mux		= pmic_gpio_set_mux,
};

static int pmic_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	const uint32_t param = pinconf_to_config_param(*config);
	uint32_t arg = 0;

	if (get_pmic_gpio_info(state, pin) < 0)
		return -EINVAL;

	switch (param) {
	case PMIC_GPIO_CONF_DISABLE:
	case PMIC_GPIO_CONF_PULL_DOWN:
	case PMIC_GPIO_CONF_PULL_UP:
		arg = pad->pull;
		break;
	case PMIC_GPIO_CONF_INPUT_ENABLE:
	case PMIC_GPIO_CONF_OUTPUT_ENABLE:
		arg = pad->output_enabled;
		break;
	case PMIC_GPIO_CONF_OUTPUT:
		arg = pad->output;
		break;
	case PMIC_GPIO_CONF_DRIVE_STRENGTH:
		arg = pad->strength;
		break;
	default:
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: param(%#x), arg(%#x)\n", __func__, pin, param, arg);

	return 0;
}

static int pmic_gpio_pin_config_set(struct pinctrl_dev *pctldev, uint32_t pin,
					unsigned long *configs, uint32_t num_configs)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	const uint8_t reg_set = get_set_reg(pin);
	uint32_t param = 0, arg = 0, i = 0;
	int ret = 0, val_set = 0, cnt = 0;
	char buf[1024] = {0, };

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case PMIC_GPIO_CONF_DISABLE:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DISABLE;
			break;
		case PMIC_GPIO_CONF_PULL_DOWN:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DOWN;
			break;
		case PMIC_GPIO_CONF_PULL_UP:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_UP;
			break;
		case PMIC_GPIO_CONF_INPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK) | PMIC_GPIO_MODE_DIGITAL_INPUT;
			break;
		case PMIC_GPIO_CONF_OUTPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK) | PMIC_GPIO_MODE_DIGITAL_OUTPUT;
			break;
		case PMIC_GPIO_CONF_OUTPUT:
			arg = (arg << PMIC_GPIO_OUT_SHIFT) & PMIC_GPIO_OUT_MASK;
			val_set = (val_set & ~PMIC_GPIO_OUT_MASK) | arg;
			break;
		case PMIC_GPIO_CONF_DRIVE_STRENGTH:
			arg = (arg << PMIC_GPIO_DRV_STR_SHIFT) & PMIC_GPIO_DRV_STR_MASK;
			val_set = (val_set & ~PMIC_GPIO_DRV_STR_MASK) | arg;
			break;
		default:
			return -ENOTSUPP;
		}
	}

	ret = pmic_gpio_write(state, reg_set, val_set);
	if (ret < 0) {
		dev_err(state->dev, "[MAIN_PMIC] %s: pmic_gpio_write fail\n", __func__);
		return ret;
	}

	if (get_pmic_gpio_info(state, pin) < 0)
		return -EINVAL;

	cnt += snprintf(buf + cnt, sizeof(buf) - 1,
			"reg(%#02hhx%02hhx:%#02hhx), %s(%#x), MODE(%s), DRV_STR(%s_%#x), DAT(%s_%#x)",
			state->i2c->addr, reg_set, val_set,
			biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: %s\n", __func__, pin, buf);

	return 0;
}

static int pmic_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	const struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t i = 0;
	int ret = 0;

	for (i = 0; i < state->npins; i++) {
		ret = pmic_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int pmic_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, uint32_t selector,
					unsigned long *configs, uint32_t num_configs)
{
	const struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t i = 0;
	int ret = 0;

	for (i = 0; i < state->npins; i++) {
		ret = pmic_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void pmic_gpio_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;

	if (get_pmic_gpio_info(state, pin) < 0) {
		seq_printf(s, "get_pmic_gpio_info fail(%d)", __LINE__);
		return;
	}

	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);
}

static const struct pinconf_ops pmic_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= pmic_gpio_pin_config_get,
	.pin_config_set			= pmic_gpio_pin_config_set,
	.pin_config_group_get		= pmic_gpio_pin_config_group_get,
	.pin_config_group_set		= pmic_gpio_pin_config_group_set,
	.pin_config_dbg_show		= pmic_gpio_pin_config_dbg_show,
};

static int pmic_gpio_direction_input(struct gpio_chip *chip, uint32_t pin)
{
	const struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;
	int ret = 0;

	config = pinconf_to_config_packed(PMIC_GPIO_CONF_INPUT_ENABLE, 1);
	ret = pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: ret(%#x)\n", __func__, pin, ret);

	return ret;
}

static int pmic_gpio_direction_output(struct gpio_chip *chip, uint32_t pin, int val)
{
	const struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long configs[] = {PMIC_GPIO_CONF_OUTPUT_ENABLE, PMIC_GPIO_CONF_OUTPUT};
	int ret = 0;
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(configs); i++)
		configs[i] = pinconf_to_config_packed(configs[i], val);

	ret = pmic_gpio_pin_config_set(state->ctrl, pin, configs, ARRAY_SIZE(configs));

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: val(%#x), ret(%#x)\n", __func__, pin, val, ret);

	return ret;
}

static int pmic_gpio_get(struct gpio_chip *chip, uint32_t pin)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	struct pmic_gpio_pad *pad = state->ctrl->desc->pins[pin].drv_data;

	if (get_pmic_gpio_info(state, pin) < 0)
		return -EINVAL;

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: DAT(%s)\n",
				__func__, pin, pad->output ? "high" : "low");

	return pad->output;
}

static void pmic_gpio_set(struct gpio_chip *chip, uint32_t pin, int value)
{
	const struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: Set DAT(%s_%#x)\n",
				__func__, pin, value ? "high" : "low", value);

	config = pinconf_to_config_packed(PMIC_GPIO_CONF_OUTPUT, value);
	pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);
}

static int pmic_gpio_pin_xlate(const uint32_t gpio)
{
	/* Translate a DT GPIO specifier into a PMIC GPIO number */
	if (gpio == 0)
		return 0;
	else if (gpio == 1)
		return 1;
	else
		return -EINVAL;
}

static int pmic_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	int ret = 0;

	if (chip->of_gpio_n_cells < 2)
		return -EINVAL;

	if (flags)
		*flags = gpio_desc->args[1];

	ret = pmic_gpio_pin_xlate(gpio_desc->args[0]);
	if (ret < 0) {
		dev_err(state->dev, "%s: Invalid GPIO pin(%d)\n", __func__, gpio_desc->args[0]);
		return ret;
	}

	return ret;
}

static void pmic_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	const struct pmic_gpio_state *state = gpiochip_get_data(chip);
	uint32_t i;

	for (i = 0; i < chip->ngpio; i++) {
		pmic_gpio_pin_config_dbg_show(state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int pmic_gpio_set_config(struct gpio_chip *chip, uint32_t pin, unsigned long config)
{
	const struct pmic_gpio_state *state = gpiochip_get_data(chip);

	dev_err(state->dev, "[MAIN_PMIC] %s: pin%d: config(%#x)\n", __func__, pin, config);

	return pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);
}

static const struct gpio_chip pmic_gpio_gpio_template = {
	.direction_input	= pmic_gpio_direction_input,
	.direction_output	= pmic_gpio_direction_output,
	.get			= pmic_gpio_get,
	.set			= pmic_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= pmic_gpio_of_xlate,
	.dbg_show		= pmic_gpio_dbg_show,
	.set_config		= pmic_gpio_set_config,
};

static int pmic_gpio_populate(const struct pmic_gpio_state *state,
			      struct pmic_gpio_pad *pad, const uint32_t pin)
{
	const uint8_t reg_set = get_set_reg(pin);
	const uint8_t reg_status = get_status_reg();
	int val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	pad->output_enabled = get_gpio_mode(val_set);
	pad->output = get_gpio_output(pin, val_status);
	pad->pull = get_gpio_pull(val_set);
	pad->strength = get_gpio_strength(val_set);

	dev_info(state->dev, "[MAIN_PMIC] %s: pin%d: %s(%#x), MODE(%s), "
			"DRV_STR(%s_%#x), DAT(%s_%#x)\n", __func__, pin,
			biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	return 0;
}

static int set_pmic_gpio_pad(const struct pmic_gpio_state *state)
{
	struct pmic_gpio_pad *pad, *pads;
	int i = 0, ret = 0;

	pads = devm_kcalloc(state->dev, state->npins, sizeof(*pads), GFP_KERNEL);
	if (!pads)
		return -ENOMEM;

	for (i = 0; i < state->npins; i++) {
		pad = &pads[i];
		pmic_pin_desc[i].drv_data = pad;

		ret = pmic_gpio_populate(state, pad, i);
		if (ret < 0)
			return -EINVAL;
	}

	return 0;
}

static void set_pmic_gpio_chip(struct pmic_gpio_state *state)
{
	state->chip = pmic_gpio_gpio_template;
	state->chip.parent = state->dev;
	state->chip.base = -1;
	state->chip.ngpio = state->npins;
	state->chip.label = dev_name(state->dev);
	state->chip.of_gpio_n_cells = 2;
	state->chip.can_sleep = false;
	state->chip.of_node = state->dev->of_node;
}

static int pmic_gpio_parse_dt(const struct s2mpu15_dev *iodev, struct pmic_gpio_state *state)
{
	struct device_node *mfd_np, *gpio_np;
	uint32_t val;
	int ret;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		return -ENODEV;
	}

	mfd_np = iodev->dev->of_node;
	if (!mfd_np) {
		pr_err("%s: could not find parent_node\n", __func__);
		return -ENODEV;
	}

	gpio_np = of_find_node_by_name(mfd_np, "s2mpu15-gpio");
	if (!gpio_np) {
		pr_err("%s: could not find current_node\n", __func__);
		return -ENODEV;
	}
	state->dev->of_node = gpio_np;

	ret = of_property_read_u32(gpio_np, "samsung,npins", &val);
	if (ret)
		state->npins = ARRAY_SIZE(pmic_pin_desc);
	else
		state->npins = val;

	return 0;
}

static struct pinctrl_desc pmic_pinctrl_desc = {
	.name			= "s2mpu15-gpio",
	.pctlops		= &pmic_gpio_pinctrl_ops,
	.pmxops			= &pmic_gpio_pinmux_ops,
	.confops		= &pmic_gpio_pinconf_ops,
	.owner			= THIS_MODULE,
	.pins			= pmic_pin_desc,
	.npins			= ARRAY_SIZE(pmic_pin_desc),
	.custom_params		= pmic_gpio_bindings,
	.num_custom_params	= ARRAY_SIZE(pmic_gpio_bindings),
#ifdef CONFIG_DEBUG_FS
	.custom_conf_items	= pmic_conf_items,
#endif
};

static int pmic_gpio_probe(struct platform_device *pdev)
{
	const struct s2mpu15_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct pmic_gpio_state *state;
	int ret = 0;

	dev_info(&pdev->dev, "[MAIN_PMIC] %s: start\n", __func__);

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);
	state->dev = &pdev->dev;
	state->i2c = iodev->gpio;

	ret = pmic_gpio_parse_dt(iodev, state);
	if (ret < 0)
		return ret;

	ret = set_pmic_gpio_pad(state);
	if (ret < 0)
		return ret;

	set_pmic_gpio_chip(state);

	state->ctrl = devm_pinctrl_register(state->dev, &pmic_pinctrl_desc, state);
	if (IS_ERR(state->ctrl))
		return PTR_ERR(state->ctrl);

	ret = devm_gpiochip_add_data(state->dev, &state->chip, state);
	if (ret) {
		dev_err(state->dev, "can't add gpio chip\n");
		return ret;
	}

	/*
	 * For DeviceTree-supported systems, the gpio core checks the
	 * pinctrl's device node for the "gpio-ranges" property.
	 * If it is present, it takes care of adding the pin ranges
	 * for the driver. In this case the driver can skip ahead.
	 *
	 * In order to remain compatible with older, existing DeviceTree
	 * files which don't set the "gpio-ranges" property or systems that
	 * utilize ACPI the driver has to call gpiochip_add_pin_range().
	 */
	if (!of_property_read_bool(state->dev->of_node, "gpio-ranges")) {
		ret = gpiochip_add_pin_range(&state->chip, dev_name(state->dev), 0, 0,
					     state->npins);
		if (ret) {
			dev_err(&pdev->dev, "failed to add pin range\n");
			return ret;
		}
	}

	dev_info(&pdev->dev, "[MAIN_PMIC] %s: end\n", __func__);

	return 0;
}

static const struct of_device_id pmic_gpio_of_match[] = {
	{ .compatible = "s2mpu15-gpio" },
	{ },
};

MODULE_DEVICE_TABLE(of, pmic_gpio_of_match);

static struct platform_driver pmic_gpio_driver = {
	.driver = {
		   .name = "s2mpu15-gpio",
		   .of_match_table = pmic_gpio_of_match,
	},
	.probe	= pmic_gpio_probe,
};

module_platform_driver(pmic_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");
