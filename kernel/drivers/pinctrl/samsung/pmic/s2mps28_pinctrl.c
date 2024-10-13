// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
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
#include <linux/samsung/pmic/s2mps28.h>
#include <linux/samsung/pmic/s2mps28-regulator.h>

/* VGPIO regs by AP sides */
#define SYSREG_ALIVE				(0x12820000)
#define VGPIO_TX_R11				(0x82C)
#define SYSREG_ENABLE				(0x4)

/* S2MPS28 GPIO(0x0B) Address */
#define S2MPS28_GPIO_GPIO_INT1			0x00
#define S2MPS28_GPIO_GPIO_INT2			0x01
#define S2MPS28_GPIO_GPIO_INTM1			0x02
#define S2MPS28_GPIO_GPIO_INTM2			0x03
#define S2MPS28_GPIO_GPIO_STATUS1		0x04
#define S2MPS28_GPIO_GPIO0_SET			0x05
#define S2MPS28_GPIO_GPIO1_SET			0x06
#define S2MPS28_GPIO_GPIO2_SET			0x07
#define S2MPS28_GPIO_GPIO3_SET			0x08
#define S2MPS28_GPIO_GPIO4_SET			0x09
#define S2MPS28_GPIO_GPIO5_SET			0x0A
#define S2MPS28_GPIO_GPIO_TX_MASK		0x0B
#define S2MPS28_GPIO_GPIO_MODE1			0x0C
#define S2MPS28_GPIO_GPIO_PD_OFF		0x0D

#define GPIO0_IDX				(0)
#define GPIO5_IDX				(5)
#define GPIO_VGI_RANGE				(4)
#define GPIO_RANGE				(8)

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
#define S2MPS28_GPIO_CONF_DISABLE                  (PIN_CONFIG_END + 1)
#define S2MPS28_GPIO_CONF_PULL_DOWN                (PIN_CONFIG_END + 2)
#define S2MPS28_GPIO_CONF_PULL_UP                  (PIN_CONFIG_END + 3)
#define S2MPS28_GPIO_CONF_INPUT_ENABLE             (PIN_CONFIG_END + 4)
#define S2MPS28_GPIO_CONF_OUTPUT_ENABLE            (PIN_CONFIG_END + 5)
#define S2MPS28_GPIO_CONF_OUTPUT                   (PIN_CONFIG_END + 6)
#define S2MPS28_GPIO_CONF_DRIVE_STRENGTH           (PIN_CONFIG_END + 7)

enum s2mps28_gpio_vgi_idx {
	SUB1_GPIO1_4,
	SUB2_GPIO1_4,
	SUB3_GPIO1_4,
	SUB4_GPIO1_4,
	SUB5_GPIO1_4,
	SUB_GPIO5,
	SUB_GPIO_NUM,
};

static const uint32_t s2mps28_gpio_group[SUB_GPIO_NUM] = {
	SUB1_GPIO1_4, SUB2_GPIO1_4, SUB3_GPIO1_4,
	SUB4_GPIO1_4, SUB5_GPIO1_4, SUB_GPIO5,
};

enum s2mps28_gpio_io_param {
	PMIC_GPIO_MODE_DIGITAL_INPUT = (0 << PMIC_GPIO_OEN_SHIFT),
	PMIC_GPIO_MODE_DIGITAL_OUTPUT = (1 << PMIC_GPIO_OEN_SHIFT),
};

enum s2mps28_gpio_pull_param {
	PMIC_GPIO_PULL_DISABLE = (0 << PMIC_GPIO_PULL_SHIFT),
	PMIC_GPIO_PULL_DOWN = (1 << PMIC_GPIO_PULL_SHIFT),
	PMIC_GPIO_PULL_UP = (2 << PMIC_GPIO_PULL_SHIFT),
};

static const char *const s2mps28_biases[] = {
	"no-pull", "pull-down", "pull-up"
};

static const char *const s2mps28_strengths[] = {
	"1mA", "2mA", "3mA", "4mA", "5mA", "6mA"
};

static const struct pinconf_generic_params s2mps28_gpio_bindings[] = {
	{"pmic-gpio,pull-disable",	S2MPS28_GPIO_CONF_DISABLE,		0},
	{"pmic-gpio,pull-down",		S2MPS28_GPIO_CONF_PULL_DOWN,	1},
	{"pmic-gpio,pull-up",		S2MPS28_GPIO_CONF_PULL_UP,		2},
	{"pmic-gpio,input-enable",	S2MPS28_GPIO_CONF_INPUT_ENABLE,	0},
	{"pmic-gpio,output-enable",	S2MPS28_GPIO_CONF_OUTPUT_ENABLE,	1},
	{"pmic-gpio,output-low",	S2MPS28_GPIO_CONF_OUTPUT,		0},
	{"pmic-gpio,output-high",	S2MPS28_GPIO_CONF_OUTPUT,		1},
	{"pmic-gpio,drive-strength",	S2MPS28_GPIO_CONF_DRIVE_STRENGTH,	1},
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item s2mps28_pin_config_item[ARRAY_SIZE(s2mps28_gpio_bindings)] = {
	PCONFDUMP(S2MPS28_GPIO_CONF_DISABLE,		"pull-disable", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_PULL_DOWN,		"pull-down", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_PULL_UP,		"pull-up", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_INPUT_ENABLE,	"input-enable", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_OUTPUT_ENABLE,	"output-enable", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_OUTPUT,		"output-high", NULL, true),
	PCONFDUMP(S2MPS28_GPIO_CONF_DRIVE_STRENGTH,	"drive-strength", NULL, true),
};
#endif

struct s2mps28_gpio_state {
	struct device *dev;
	struct s2mps28_dev *iodev;
	struct pinctrl_dev *ctrl;
	struct gpio_chip chip;
	struct irq_chip irq;
	struct i2c_client *i2c;
	uint32_t npins;
	const struct pingroup *groups;
	uint32_t ngroups;
	void __iomem *sysreg_base;
};

/*
 * struct s2mps28_gpio_pad - keep current GPIO settings
 * @output_enabled: Set to true if GPIO output buffer logic is enabled.
 * @output: Cached pin output value
 * @pull: Constant current which flow trough GPIO output buffer.
 * @strength: 1mA, 2mA, 3mA, 4mA, 5mA, 6mA (from 0 to 5)
 * @function: See s2mps28_gpio_functions[]
 */
struct s2mps28_gpio_pad {
	bool		output_enabled;
	uint32_t	output;
	uint32_t	pull;
	uint32_t	strength;
	uint32_t	function;
};

static const unsigned int sub_pmic_gpio_pins[] = {0, 1, 2, 3, 4, 5};

#define SUB_PMIC_GROUP_SIZE	1

static const struct pingroup sub_pmic_pin_groups[] = {
	PINCTRL_PINGROUP("SUB1_PMIC_GPIO", sub_pmic_gpio_pins, ARRAY_SIZE(sub_pmic_gpio_pins)),
	PINCTRL_PINGROUP("SUB2_PMIC_GPIO", sub_pmic_gpio_pins, ARRAY_SIZE(sub_pmic_gpio_pins)),
	PINCTRL_PINGROUP("SUB3_PMIC_GPIO", sub_pmic_gpio_pins, ARRAY_SIZE(sub_pmic_gpio_pins)),
	PINCTRL_PINGROUP("SUB4_PMIC_GPIO", sub_pmic_gpio_pins, ARRAY_SIZE(sub_pmic_gpio_pins)),
	PINCTRL_PINGROUP("SUB5_PMIC_GPIO", sub_pmic_gpio_pins, ARRAY_SIZE(sub_pmic_gpio_pins)),
};

static const char *const s2mps28_gpio_groups[] = {
	"gpio_s0", "gpio_s1", "gpio_s2", "gpio_s3", "gpio_s4", "gpio_s5",
};

static struct pinctrl_pin_desc s2mps28_pinctrl_pin_desc[] = {
	PINCTRL_PIN(0, "gpio_s0"),
	PINCTRL_PIN(1, "gpio_s1"),
	PINCTRL_PIN(2, "gpio_s2"),
	PINCTRL_PIN(3, "gpio_s3"),
	PINCTRL_PIN(4, "gpio_s4"),
	PINCTRL_PIN(5, "gpio_s5"),
};

/* The index of each function in s2mps28_gpio_functions[] array */
#define PMIC_GPIO_FUNC_NORMAL		"normal"

enum s2mps28_gpio_func_index {
	PMIC_GPIO_FUNC_INDEX_NORMAL,
};

static const char *const s2mps28_gpio_functions[] = {
	[PMIC_GPIO_FUNC_INDEX_NORMAL]	= PMIC_GPIO_FUNC_NORMAL,
};

static int s2mps28_gpio_read(const struct s2mps28_gpio_state *state, const uint8_t addr)
{
	uint8_t val = 0;
	int ret = 0;

	ret = s2mps28_read_reg(state->i2c, addr, &val);
	if (ret)
		dev_err(state->dev, "[SUB%d_PMIC] %s: read %#x failed\n",
			state->iodev->device_type + 1, __func__, addr);
	else
		ret = val;

	return ret;
}

static void s2mps28_vgpio_write(struct s2mps28_gpio_state *state, const uint32_t pin, uint32_t val)
{
	u32 reg = 0;
	u32 group = pin / 4;
	u32 offset = pin % 4;

	val &= 0x1;
	reg = readl(state->sysreg_base + group * 4);
	reg = (val) ? (reg | (1 << offset)) : (reg & ~(1 << offset));
	reg |= 1 << (offset + SYSREG_ENABLE);
	writel(reg, state->sysreg_base + group * 4);
}

static int s2mps28_gpio_write(struct s2mps28_gpio_state *state, const uint8_t addr, const uint8_t val)
{
	return s2mps28_write_reg(state->i2c, addr, val);
}

static bool get_s2mps28_gpio_mode(const int val)
{
	return ((val & PMIC_GPIO_OEN_MASK) >> PMIC_GPIO_OEN_SHIFT) ? true : false;
}

static uint32_t get_s2mps28_gpio_output(const uint32_t pin, const int val)
{
	return (val >> (pin % GPIO_RANGE)) & 0x1;
}

static uint32_t get_s2mps28_gpio_pull(const int val)
{
	return ((val & PMIC_GPIO_PULL_MASK) >> PMIC_GPIO_PULL_SHIFT);
}

static uint32_t get_s2mps28_gpio_strength(const int val)
{
	return ((val & PMIC_GPIO_DRV_STR_MASK) >> PMIC_GPIO_DRV_STR_SHIFT);
}

static uint8_t get_s2mps28_set_reg(const uint32_t pin)
{
	return S2MPS28_GPIO_GPIO0_SET + pin;
}

static uint8_t get_s2mps28_status_reg(const uint32_t pin)
{
	return S2MPS28_GPIO_GPIO_STATUS1;
}

static int get_s2mps28_gpio_info(struct s2mps28_gpio_state *state, const uint32_t pin)
{
	struct s2mps28_gpio_pad *pad = s2mps28_pinctrl_pin_desc[pin].drv_data;
	const uint8_t reg_set = get_s2mps28_set_reg(pin);
	const uint8_t reg_status = get_s2mps28_status_reg(pin);
	int val_set = 0, val_status = 0;

	val_set = s2mps28_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = s2mps28_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	pad->output_enabled = get_s2mps28_gpio_mode(val_set);
	pad->output = get_s2mps28_gpio_output(pin, val_status);
	pad->pull = get_s2mps28_gpio_pull(val_set);
	pad->strength = get_s2mps28_gpio_strength(val_set);

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: (%#02hhx:%#02hhx), (%#02hhx:%#02hhx)\n",
				state->iodev->device_type + 1, __func__, pin, reg_status,
				val_status, reg_set, val_set);
	return 0;
}

static int s2mps28_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);

	return state->ngroups;
}

static const char *s2mps28_gpio_get_group_name(struct pinctrl_dev *pctldev,
					    uint32_t selector)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);

	return state->groups[selector].name;
}


static int s2mps28_gpio_get_group_pins(struct pinctrl_dev *pctldev, uint32_t selector,
					const uint32_t **pins, uint32_t *num_pins)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);

	if (selector >= state->ngroups)
		return -EINVAL;

	*pins = state->groups[selector].pins;
	*num_pins = state->groups[selector].npins;

	return 0;
}

static void s2mps28_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct s2mps28_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;

	if (get_s2mps28_gpio_info(state, pin) < 0) {
		seq_printf(s, "get_s2mps28_gpio_info fail(%d)", __LINE__);
		return;
	}

	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			s2mps28_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2mps28_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);
}

static const struct pinctrl_ops s2mps28_gpio_pinctrl_ops = {
	.get_groups_count	= s2mps28_gpio_get_groups_count,
	.get_group_name		= s2mps28_gpio_get_group_name,
	.get_group_pins		= s2mps28_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= s2mps28_gpio_pin_dbg_show,
};

static int s2mps28_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(s2mps28_gpio_functions);
}

static const char *s2mps28_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       uint32_t function)
{
	return s2mps28_gpio_functions[function];
}

static int s2mps28_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 uint32_t selector,
					 const char *const **groups,
					 uint32_t *const num_groups)
{
	*groups = s2mps28_gpio_groups;
	*num_groups = ARRAY_SIZE(s2mps28_gpio_groups);

	return 0;
}

static int s2mps28_gpio_set_mux(struct pinctrl_dev *pctldev, uint32_t func_selector,
				uint32_t group_selector)
{
	const struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	unsigned npins;
	uint32_t i = 0;
	struct s2mps28_gpio_pad *pad = NULL;

	npins = state->groups[group_selector].npins;

	for (i = 0; i < npins; i++) {
		pad = pctldev->desc->pins[i].drv_data;
		pad->function = func_selector;
	}

	return 0;
}

static const struct pinmux_ops s2mps28_gpio_pinmux_ops = {
	.get_functions_count	= s2mps28_gpio_get_functions_count,
	.get_function_name	= s2mps28_gpio_get_function_name,
	.get_function_groups	= s2mps28_gpio_get_function_groups,
	.set_mux		= s2mps28_gpio_set_mux,
};

static int s2mps28_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct s2mps28_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	const uint32_t param = pinconf_to_config_param(*config);
	uint32_t arg = 0;

	if (get_s2mps28_gpio_info(state, pin) < 0)
		return -EINVAL;

	switch (param) {
	case S2MPS28_GPIO_CONF_DISABLE:
	case S2MPS28_GPIO_CONF_PULL_DOWN:
	case S2MPS28_GPIO_CONF_PULL_UP:
		arg = pad->pull;
		break;
	case S2MPS28_GPIO_CONF_INPUT_ENABLE:
	case S2MPS28_GPIO_CONF_OUTPUT_ENABLE:
		arg = pad->output_enabled;
		break;
	case S2MPS28_GPIO_CONF_OUTPUT:
		arg = pad->output;
		break;
	case S2MPS28_GPIO_CONF_DRIVE_STRENGTH:
		arg = pad->strength;
		break;
	default:
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: param(%#x), arg(%#x)\n",
		state->iodev->device_type + 1, __func__, pin, param, arg);

	return 0;
}

static int s2mps28_gpio_pin_config_set(struct pinctrl_dev *pctldev, uint32_t pin,
					unsigned long *configs, uint32_t num_configs)
{
	struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct s2mps28_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	const uint8_t reg_set = get_s2mps28_set_reg(pin);
	uint32_t param = 0, arg = 0, i = 0;
	int ret = 0, val_set = 0, cnt = 0, dev_type = state->iodev->device_type;
	char buf[1024] = {0, };
	uint32_t vgpio_group = (pin == GPIO5_IDX) ? s2mps28_gpio_group[SUB_GPIO5] : s2mps28_gpio_group[dev_type];

	vgpio_group *= GPIO_VGI_RANGE;
	val_set = s2mps28_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case S2MPS28_GPIO_CONF_DISABLE:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DISABLE;
			break;
		case S2MPS28_GPIO_CONF_PULL_DOWN:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DOWN;
			break;
		case S2MPS28_GPIO_CONF_PULL_UP:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_UP;
			break;
		case S2MPS28_GPIO_CONF_INPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK) | PMIC_GPIO_MODE_DIGITAL_INPUT;
			break;
		case S2MPS28_GPIO_CONF_OUTPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK) | PMIC_GPIO_MODE_DIGITAL_OUTPUT;
			break;
		case S2MPS28_GPIO_CONF_OUTPUT:
			if (pin == GPIO0_IDX) {
				arg = (arg << PMIC_GPIO_OUT_SHIFT) & PMIC_GPIO_OUT_MASK;
				val_set = (val_set & ~PMIC_GPIO_OUT_MASK) | arg;
			} else if (pin == GPIO5_IDX) {
				s2mps28_vgpio_write(state, vgpio_group + s2mps28_gpio_group[dev_type], arg);
			} else
				s2mps28_vgpio_write(state, (pin + vgpio_group) - 1, arg);
			break;
		case S2MPS28_GPIO_CONF_DRIVE_STRENGTH:
			arg = (arg << PMIC_GPIO_DRV_STR_SHIFT) & PMIC_GPIO_DRV_STR_MASK;
			val_set = (val_set & ~PMIC_GPIO_DRV_STR_MASK) | arg;
			break;
		default:
			return -ENOTSUPP;
		}
	}

	ret = s2mps28_gpio_write(state, reg_set, val_set);
	if (ret < 0) {
		dev_err(state->dev, "[SUB%d_PMIC] %s: s2mps28_gpio_write fail\n", dev_type + 1, __func__);
		return ret;
	}

	if (get_s2mps28_gpio_info(state, pin) < 0)
		return -EINVAL;

	cnt += snprintf(buf + cnt, sizeof(buf) - 1,
			"reg(%#02x%02hhx:%#02hhx), %s(%#x), MODE(%s), DRV_STR(%s_%#x), DAT(%s_%#x)",
			state->i2c->addr, reg_set, val_set,
			s2mps28_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2mps28_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: %s\n", dev_type + 1, __func__, pin, buf);

	return 0;
}

static int s2mps28_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					uint32_t selector, unsigned long *config)
{
	const struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mps28_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int s2mps28_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, uint32_t selector,
					unsigned long *configs, uint32_t num_configs)
{
	const struct s2mps28_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mps28_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static const struct pinconf_ops s2mps28_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= s2mps28_gpio_pin_config_get,
	.pin_config_set			= s2mps28_gpio_pin_config_set,
	.pin_config_group_get		= s2mps28_gpio_pin_config_group_get,
	.pin_config_group_set		= s2mps28_gpio_pin_config_group_set,
	.pin_config_dbg_show		= s2mps28_gpio_pin_dbg_show,
};

static int s2mps28_gpio_direction_input(struct gpio_chip *chip, uint32_t pin)
{
	const struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;
	int ret = 0;

	config = pinconf_to_config_packed(S2MPS28_GPIO_CONF_INPUT_ENABLE, 1);
	ret = s2mps28_gpio_pin_config_set(state->ctrl, pin, &config, 1);

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: ret(%#x)\n",
		state->iodev->device_type + 1, __func__, pin, ret);

	return ret;
}

static int s2mps28_gpio_direction_output(struct gpio_chip *chip, uint32_t pin, int val)
{
	const struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	unsigned long configs[] = {S2MPS28_GPIO_CONF_OUTPUT_ENABLE, S2MPS28_GPIO_CONF_OUTPUT};
	int ret = 0;
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(configs); i++)
		configs[i] = pinconf_to_config_packed(configs[i], val);

	ret = s2mps28_gpio_pin_config_set(state->ctrl, pin, configs, ARRAY_SIZE(configs));

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: val(%#x), ret(%#x)\n",
		state->iodev->device_type + 1, __func__, pin, val, ret);

	return ret;
}

static int s2mps28_gpio_get(struct gpio_chip *chip, uint32_t pin)
{
	struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	struct s2mps28_gpio_pad *pad = state->ctrl->desc->pins[pin].drv_data;

	if (get_s2mps28_gpio_info(state, pin) < 0)
		return -EINVAL;

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: DAT(%s)\n",
		state->iodev->device_type + 1, __func__, pin, pad->output ? "high" : "low");

	return pad->output;
}

static void s2mps28_gpio_set(struct gpio_chip *chip, uint32_t pin, int value)
{
	const struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: Set DAT(%s_%#x)\n",
		state->iodev->device_type + 1, __func__, pin, value ? "high" : "low", value);

	config = pinconf_to_config_packed(S2MPS28_GPIO_CONF_OUTPUT, value);
	s2mps28_gpio_pin_config_set(state->ctrl, pin, &config, 1);
}

static int s2mps28_gpio_pin_xlate(const uint32_t gpio)
{
	/* Translate a DT GPIO specifier into a PMIC GPIO number */
	if (gpio <= 10)
		return gpio;
	else
		return -EINVAL;
}

static int s2mps28_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	int ret = 0;

	if (chip->of_gpio_n_cells < 2)
		return -EINVAL;

	if (flags)
		*flags = gpio_desc->args[1];

	ret = s2mps28_gpio_pin_xlate(gpio_desc->args[0]);
	if (ret < 0) {
		dev_err(state->dev, "%s: Invalid GPIO pin(%d)\n", __func__, gpio_desc->args[0]);
		return ret;
	}

	return ret;
}

static void s2mps28_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	const struct s2mps28_gpio_state *state = gpiochip_get_data(chip);
	uint32_t i;

	for (i = 0; i < chip->ngpio; i++) {
		s2mps28_gpio_pin_dbg_show(state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int s2mps28_gpio_set_config(struct gpio_chip *chip, uint32_t pin, unsigned long config)
{
	const struct s2mps28_gpio_state *state = gpiochip_get_data(chip);

	dev_err(state->dev, "[SUB%d_PMIC] %s: pin%d: config(%#lx)\n",
		state->iodev->device_type + 1, __func__, pin, config);

	return s2mps28_gpio_pin_config_set(state->ctrl, pin, &config, 1);
}

static const struct gpio_chip s2mps28_gpio_chip = {
	.direction_input	= s2mps28_gpio_direction_input,
	.direction_output	= s2mps28_gpio_direction_output,
	.get			= s2mps28_gpio_get,
	.set			= s2mps28_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= s2mps28_gpio_of_xlate,
	.dbg_show		= s2mps28_gpio_dbg_show,
	.set_config		= s2mps28_gpio_set_config,
};

static int s2mps28_gpio_populate(const struct s2mps28_gpio_state *state,
			      struct s2mps28_gpio_pad *pad, const uint32_t pin)
{
	const uint8_t reg_set = get_s2mps28_set_reg(pin);
	const uint8_t reg_status = get_s2mps28_status_reg(pin);
	int val_set = 0, val_status = 0;

	val_set = s2mps28_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = s2mps28_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	pad->output_enabled = get_s2mps28_gpio_mode(val_set);
	pad->output = get_s2mps28_gpio_output(pin, val_status);
	pad->pull = get_s2mps28_gpio_pull(val_set);
	pad->strength = get_s2mps28_gpio_strength(val_set);

	dev_info(state->dev, "[SUB%d_PMIC] %s: pin%d: %s(%#x), MODE(%s), "
			"DRV_STR(%s_%#x), DAT(%s_%#x)\n", state->iodev->device_type + 1,
			__func__, pin, s2mps28_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2mps28_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	return 0;
}

static int set_s2mps28_gpio_pad(const struct s2mps28_gpio_state *state)
{
	struct s2mps28_gpio_pad *pad, *pads;
	int i = 0, ret = 0;

	pads = devm_kcalloc(state->dev, state->npins, sizeof(*pads), GFP_KERNEL);
	if (!pads)
		return -ENOMEM;

	for (i = 0; i < state->npins; i++) {
		pad = &pads[i];
		s2mps28_pinctrl_pin_desc[i].drv_data = pad;

		ret = s2mps28_gpio_populate(state, pad, i);
		if (ret < 0)
			return -EINVAL;
	}

	return 0;
}

static void set_s2mps28_gpio_chip(struct s2mps28_gpio_state *state)
{
	state->chip = s2mps28_gpio_chip;
	state->chip.parent = state->dev;
	state->chip.base = -1;
	state->chip.ngpio = state->npins;
	state->chip.label = dev_name(state->dev);
	state->chip.of_gpio_n_cells = 2;
	state->chip.can_sleep = false;
	state->chip.of_node = state->dev->of_node;
}

static struct pinctrl_desc s2mps28_pinctrl_desc = {
	.pctlops		= &s2mps28_gpio_pinctrl_ops,
	.pmxops			= &s2mps28_gpio_pinmux_ops,
	.confops		= &s2mps28_gpio_pinconf_ops,
	.owner			= THIS_MODULE,
	.pins			= s2mps28_pinctrl_pin_desc,
	.npins			= ARRAY_SIZE(s2mps28_pinctrl_pin_desc),
	.custom_params		= s2mps28_gpio_bindings,
	.num_custom_params	= ARRAY_SIZE(s2mps28_gpio_bindings),
#ifdef CONFIG_DEBUG_FS
	.custom_conf_items	= s2mps28_pin_config_item,
#endif
};

static int s2mps28_gpio_parse_dt(const struct s2mps28_dev *iodev, struct s2mps28_gpio_state *state)
{
	struct device_node *mfd_np, *gpio_np;
	uint32_t val;
	int ret, dev_type = iodev->device_type;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		return -ENODEV;
	}

	mfd_np = iodev->dev->of_node;
	if (!mfd_np) {
		pr_err("%s: could not find parent_node\n", __func__);
		return -ENODEV;
	}

	switch (dev_type) {
	case TYPE_S2MPS28_1:
		gpio_np = of_find_node_by_name(mfd_np, "s2mps28-1-gpio");
		break;
	case TYPE_S2MPS28_2:
		gpio_np = of_find_node_by_name(mfd_np, "s2mps28-2-gpio");
		break;
	case TYPE_S2MPS28_3:
		gpio_np = of_find_node_by_name(mfd_np, "s2mps28-3-gpio");
		break;
	case TYPE_S2MPS28_4:
		gpio_np = of_find_node_by_name(mfd_np, "s2mps28-4-gpio");
		break;
	case TYPE_S2MPS28_5:
		gpio_np = of_find_node_by_name(mfd_np, "s2mps28-5-gpio");
		break;
	default:
		pr_err("%s: could not find current_node\n", __func__);
		return -ENODEV;
	}

	state->dev->of_node = gpio_np;

	ret = of_property_read_u32(gpio_np, "samsung,npins", &val);
	if (ret)
		state->npins = ARRAY_SIZE(s2mps28_pinctrl_pin_desc);
	else
		state->npins = val;

	return 0;
}

static int s2mps28_set_vgpio(struct s2mps28_gpio_state *state)
{
	/* VGPIO setting by AP */
	state->sysreg_base = ioremap(SYSREG_ALIVE + VGPIO_TX_R11, 0x24);
	if (!state->sysreg_base) {
		dev_err(state->dev, "sysreg vgpio ioremap failed\n");
		return -EINVAL;
	}

	return 0;
}

static int s2mps28_gpio_probe(struct platform_device *pdev)
{
	struct s2mps28_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps28_gpio_state *state;
	int ret = 0, dev_type = iodev->device_type;

	dev_info(&pdev->dev, "[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);
	state->dev = &pdev->dev;
	state->iodev = iodev;
	state->i2c = iodev->gpio;
	state->groups = &sub_pmic_pin_groups[dev_type];
	state->ngroups = SUB_PMIC_GROUP_SIZE;
	s2mps28_pinctrl_desc.name = dev_name(&pdev->dev);

	ret = s2mps28_gpio_parse_dt(iodev, state);
	if (ret < 0)
		return ret;

	ret = s2mps28_set_vgpio(state);
	if (ret < 0)
		return ret;

	ret = set_s2mps28_gpio_pad(state);
	if (ret < 0)
		return ret;

	set_s2mps28_gpio_chip(state);

	state->ctrl = devm_pinctrl_register(state->dev, &s2mps28_pinctrl_desc, state);
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

	dev_info(&pdev->dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;
}

static const struct platform_device_id s2mps28_gpio_id[] = {
	{ "s2mps28-1-gpio", TYPE_S2MPS28_1},
	{ "s2mps28-2-gpio", TYPE_S2MPS28_2},
	{ "s2mps28-3-gpio", TYPE_S2MPS28_3},
	{ "s2mps28-4-gpio", TYPE_S2MPS28_4},
	{ "s2mps28-5-gpio", TYPE_S2MPS28_5},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps28_gpio_id);

static struct platform_driver s2mps28_gpio_driver = {
	.driver = {
		   .name = "s2mps28-gpio",
		   .owner = THIS_MODULE,
	},
	.probe	= s2mps28_gpio_probe,
	.id_table = s2mps28_gpio_id,
};

module_platform_driver(s2mps28_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");
