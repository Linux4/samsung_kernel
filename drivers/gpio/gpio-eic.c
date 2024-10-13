/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/of_device.h>
#include <linux/irqdomain.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/gpio.h>
#include <mach/adi.h>

/* 16 GPIO share a group of registers */
#define	GPIO_GROUP_NR		(16)
#define GPIO_GROUP_MASK		(0xFFFF)

#define	GPIO_GROUP_OFFSET	(0x80)
#define	ANA_GPIO_GROUP_OFFSET	(0x40)

/* registers definitions for GPIO controller */
#define REG_GPIO_DATA		(0x0000)
#define REG_GPIO_DMSK		(0x0004)
#define REG_GPIO_DIR		(0x0008)	/* only for gpio */
#define REG_GPIO_IS		(0x000c)	/* only for gpio */
#define REG_GPIO_IBE		(0x0010)	/* only for gpio */
#define REG_GPIO_IEV		(0x0014)
#define REG_GPIO_IE		(0x0018)
#define REG_GPIO_RIS		(0x001c)
#define REG_GPIO_MIS		(0x0020)
#define REG_GPIO_IC		(0x0024)
#define REG_GPIO_INEN		(0x0028)	/* only for gpio */

/* 8 EIC share a group of registers */
#define	EIC_GROUP_NR		(8)
#define EIC_GROUP_MASK		(0xFF)

/* registers definitions for EIC controller */
#define REG_EIC_DATA		REG_GPIO_DATA
#define REG_EIC_DMSK		REG_GPIO_DMSK
#define REG_EIC_IEV		REG_GPIO_IEV
#define REG_EIC_IE		REG_GPIO_IE
#define REG_EIC_RIS		REG_GPIO_RIS
#define REG_EIC_MIS		REG_GPIO_MIS
#define REG_EIC_IC		REG_GPIO_IC
#define REG_EIC_TRIG		(0x0028)	/* only for eic */
#define REG_EIC_0CTRL		(0x0040)
#define REG_EIC_1CTRL		(0x0044)
#define REG_EIC_2CTRL		(0x0048)
#define REG_EIC_3CTRL		(0x004c)
#define REG_EIC_4CTRL		(0x0050)
#define REG_EIC_5CTRL		(0x0054)
#define REG_EIC_6CTRL		(0x0058)
#define REG_EIC_7CTRL		(0x005c)
#define REG_EIC_DUMMYCTRL	(0x0000)

/* bits definitions for register REG_EIC_DUMMYCTRL */
#define BIT_FORCE_CLK_DBNC	BIT(15)
#define BIT_EIC_DBNC_EN		BIT(14)
#define SHIFT_EIC_DBNC_CNT	(0)
#define MASK_EIC_DBNC_CNT	(0xFFF)
#define BITS_EIC_DBNC_CNT(_x_)	((_x) & 0xFFF)

struct sci_gpio_chip {
	struct gpio_chip chip;

	uint32_t base_addr;
	uint32_t group_offset;
	struct irq_domain *irq_domain;
	int is_adi_gpio;

	 uint32_t(*read_reg) (uint32_t addr);
	void (*write_reg) (uint32_t value, uint32_t addr);
	void (*set_bits) (uint32_t bits, uint32_t addr);
	void (*clr_bits) (uint32_t bits, uint32_t addr);
};

#define	to_sci_gpio(c)		container_of(c, struct sci_gpio_chip, chip)

/* D-Die regs ops */
static uint32_t d_read_reg(uint32_t addr)
{
	return __raw_readl(addr);
}

static void d_write_reg(uint32_t value, uint32_t addr)
{
	__raw_writel(value, addr);
}

static void d_set_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl(addr) | bits, addr);
}

static void d_clr_bits(uint32_t bits, uint32_t addr)
{
	__raw_writel(__raw_readl(addr) & ~bits, addr);
}

/* A-Die regs ops */
static uint32_t a_read_reg(uint32_t addr)
{
	return sci_adi_read(addr);
}

static void a_write_reg(uint32_t value, uint32_t addr)
{
	sci_adi_raw_write(addr, value);
}

static void a_set_bits(uint32_t bits, uint32_t addr)
{
	sci_adi_set(addr, bits);
}

static void a_clr_bits(uint32_t bits, uint32_t addr)
{
	sci_adi_clr(addr, bits);
}

static int sci_gpio_read(struct gpio_chip *chip, uint32_t offset, uint32_t reg)
{
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group = offset / GPIO_GROUP_NR;
	int bitof = offset & (GPIO_GROUP_NR - 1);
	int addr = sci_gpio->base_addr + sci_gpio->group_offset * group + reg;
	int value = sci_gpio->read_reg(addr) & GPIO_GROUP_MASK;

	return (value >> bitof) & 0x1;
}

static void sci_gpio_write(struct gpio_chip *chip, uint32_t offset,
			   uint32_t reg, int value)
{
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group = offset / GPIO_GROUP_NR;
	int bitof = offset & (GPIO_GROUP_NR - 1);
	int addr = sci_gpio->base_addr + sci_gpio->group_offset * group + reg;

	if (value) {
		sci_gpio->set_bits(1 << bitof, addr);
	} else {
		sci_gpio->clr_bits(1 << bitof, addr);
	}
}

/* GPIO/EIC libgpio interfaces */
static int sci_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DMSK, 1);
	return 0;
}

static void sci_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DMSK, 0);
}

static int sci_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_DIR, 0);
	sci_gpio_write(chip, offset, REG_GPIO_INEN, 1);
	return 0;
}

static int sci_eic_direction_input(struct gpio_chip *chip, unsigned offset)
{
	/* do nothing */
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return 0;
}

static int sci_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
				     int value)
{
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, value);
	sci_gpio_write(chip, offset, REG_GPIO_DIR, 1);
	sci_gpio_write(chip, offset, REG_GPIO_INEN, 0);
	sci_gpio_write(chip, offset, REG_GPIO_DATA, value);
	return 0;
}

static int sci_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return sci_gpio_read(chip, offset, REG_GPIO_DATA);
}

static void sci_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, value);
	sci_gpio_write(chip, offset, REG_GPIO_DATA, value);
}

static int sci_gpio_set_debounce(struct gpio_chip *chip, unsigned offset,
				 unsigned debounce)
{
	/* not supported */
	pr_err("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return -EINVAL;
}

static int sci_eic_set_debounce(struct gpio_chip *chip, unsigned offset,
				unsigned debounce)
{
	/* TODO */
	pr_info("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	return 0;
}

#ifdef CONFIG_OF
static int sci_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct irq_domain *irq_domain ;
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);

	irq_domain = sci_gpio->irq_domain;
	return irq_find_mapping(irq_domain, offset);
}

static int sci_irq_to_gpio(struct gpio_chip *chip, unsigned irq)
{
	int base_irq;
	struct irq_domain *irq_domain ;
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);

	irq_domain = sci_gpio->irq_domain;
	base_irq = irq_find_mapping(irq_domain, 0);
	return irq - base_irq;
}
#else
static int sci_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return chip->base + offset + GPIO_IRQ_START;
}

static int sci_irq_to_gpio(struct gpio_chip *chip, unsigned irq)
{
	return irq - GPIO_IRQ_START - chip->base;
}

#endif

static struct sci_gpio_chip d_sci_gpio = {
	.chip.label = "sprd-d-gpio",
	.chip.request = sci_gpio_request,
	.chip.free = sci_gpio_free,
	.chip.direction_input = sci_gpio_direction_input,
	.chip.get = sci_gpio_get,
	.chip.direction_output = sci_gpio_direction_output,
	.chip.set = sci_gpio_set,
	.chip.set_debounce = sci_gpio_set_debounce,
	.chip.to_irq = sci_gpio_to_irq,

	.group_offset = GPIO_GROUP_OFFSET,
	.read_reg = d_read_reg,
	.write_reg = d_write_reg,
	.set_bits = d_set_bits,
	.clr_bits = d_clr_bits,
	.is_adi_gpio = 0,
};

static struct sci_gpio_chip a_sci_gpio = {
	.chip.label = "sprd-a-gpio",
	.chip.request = sci_gpio_request,
	.chip.free = sci_gpio_free,
	.chip.direction_input = sci_gpio_direction_input,
	.chip.get = sci_gpio_get,
	.chip.direction_output = sci_gpio_direction_output,
	.chip.set = sci_gpio_set,
	.chip.set_debounce = sci_gpio_set_debounce,
	.chip.to_irq = sci_gpio_to_irq,

	.group_offset = ANA_GPIO_GROUP_OFFSET,
	.read_reg = a_read_reg,
	.write_reg = a_write_reg,
	.set_bits = a_set_bits,
	.clr_bits = a_clr_bits,
	.is_adi_gpio = 1,
};

/*
 * EIC has the same register layout with GPIO when it's used as GPI.
 * So most implementation of GPIO can be shared by EIC.
 */
static struct sci_gpio_chip d_sci_eic = {
	.chip.label = "sprd-d-eic",
	.chip.request = sci_gpio_request,
	.chip.free = sci_gpio_free,
	.chip.direction_input = sci_eic_direction_input,
	.chip.get = sci_gpio_get,
	.chip.direction_output = NULL,
	.chip.set = NULL,
	.chip.set_debounce = sci_eic_set_debounce,
	.chip.to_irq = sci_gpio_to_irq,

	.group_offset = GPIO_GROUP_OFFSET,
	.read_reg = d_read_reg,
	.write_reg = d_write_reg,
	.set_bits = d_set_bits,
	.clr_bits = d_clr_bits,
	.is_adi_gpio = 0,
};

static struct sci_gpio_chip a_sci_eic = {
	.chip.label = "sprd-a-eic",
	.chip.request = sci_gpio_request,
	.chip.free = sci_gpio_free,
	.chip.direction_input = sci_eic_direction_input,
	.chip.get = sci_gpio_get,
	.chip.direction_output = NULL,
	.chip.set = NULL,
	.chip.set_debounce = sci_eic_set_debounce,
	.chip.to_irq = sci_gpio_to_irq,

	.group_offset = ANA_GPIO_GROUP_OFFSET,
	.read_reg = a_read_reg,
	.write_reg = a_write_reg,
	.set_bits = a_set_bits,
	.clr_bits = a_clr_bits,
	.is_adi_gpio = 1,
};

/* GPIO/EIC irq interfaces */
static void sci_gpio_irq_ack(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IC, 1);
}

static void sci_gpio_irq_mask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IE, 0);
}

static void sci_gpio_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_GPIO_IE, 1);
}

static void sci_eic_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d\n", __FUNCTION__, chip->base, offset);
	sci_gpio_write(chip, offset, REG_EIC_IE, 1);

	/* TODO: the interval of two EIC trigger needs be longer than 2ms */
	sci_gpio_write(chip, offset, REG_EIC_TRIG, 1);
}

static int sci_gpio_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, flow_type);
	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 1);
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 1);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sci_gpio_write(chip, offset, REG_GPIO_IS, 1);
		sci_gpio_write(chip, offset, REG_GPIO_IBE, 0);
		sci_gpio_write(chip, offset, REG_GPIO_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sci_eic_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	struct gpio_chip *chip = (struct gpio_chip *)data->chip_data;
	int offset = sci_irq_to_gpio(chip, data->irq);
	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, offset, flow_type);
	switch (flow_type) {
	case IRQ_TYPE_LEVEL_HIGH:
		sci_gpio_write(chip, offset, REG_EIC_IEV, 1);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sci_gpio_write(chip, offset, REG_EIC_IEV, 0);
		__irq_set_handler_locked(data->irq, handle_level_irq);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sci_gpio_irq_set_wake(struct irq_data *data, unsigned int on)
{
	return on ? 0 : -EPERM;
}

static struct irq_chip d_gpio_irq_chip = {
	.name = "irq-d-gpio",
	.irq_disable = sci_gpio_irq_mask,
	.irq_ack = sci_gpio_irq_ack,
	.irq_mask = sci_gpio_irq_mask,
	.irq_unmask = sci_gpio_irq_unmask,
	.irq_set_type = sci_gpio_irq_set_type,
	.irq_set_wake = sci_gpio_irq_set_wake,
};

static struct irq_chip a_gpio_irq_chip = {
	.name = "irq-a-gpio",
	.irq_disable = sci_gpio_irq_mask,
	.irq_ack = sci_gpio_irq_ack,
	.irq_mask = sci_gpio_irq_mask,
	.irq_unmask = sci_gpio_irq_unmask,
	.irq_set_type = sci_gpio_irq_set_type,
	.irq_set_wake = sci_gpio_irq_set_wake,
};

/*
 * EIC has the same register layout with GPIO when it's used as GPI.
 * So most implementation of GPIO can be shared by EIC.
 */
static struct irq_chip d_eic_irq_chip = {
	.name = "irq-d-eic",
	.irq_disable = sci_gpio_irq_mask,
	.irq_ack = sci_gpio_irq_ack,
	.irq_mask = sci_gpio_irq_mask,
	.irq_unmask = sci_eic_irq_unmask,
	.irq_set_type = sci_eic_irq_set_type,
};

static struct irq_chip a_eic_irq_chip = {
	.name = "irq-a-eic",
	.irq_disable = sci_gpio_irq_mask,
	.irq_ack = sci_gpio_irq_ack,
	.irq_mask = sci_gpio_irq_mask,
	.irq_unmask = sci_eic_irq_unmask,
	.irq_set_type = sci_eic_irq_set_type,
};

static void gpio_eic_handler(int irq, struct gpio_chip *chip)
{
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(chip);
	int group, n, addr, value, count = 0;

	pr_debug("%s %d+%d %d\n", __FUNCTION__, chip->base, chip->ngpio, irq);
	for (group = 0; group * GPIO_GROUP_NR < chip->ngpio; group++) {
		addr =
		    sci_gpio->base_addr + sci_gpio->group_offset * group +
		    REG_GPIO_MIS;
		value = sci_gpio->read_reg(addr) & GPIO_GROUP_MASK;

		while (value) {
			n = __ffs(value);
			value &= ~(1 << n);
			n = chip->to_irq(chip, group * GPIO_GROUP_NR + n);
			pr_debug("%s generic_handle_n %d\n", __FUNCTION__, n);
			count++;
			generic_handle_irq(n);
		}
	}

}

static irqreturn_t gpio_muxed_handler(int irq, void *dev_id)
{
	struct gpio_chip *chip = dev_id;
	gpio_eic_handler(irq, chip);
	return IRQ_HANDLED;
}

/* gpio/eic cascaded irq handler */
static void gpio_muxed_flow_handler(unsigned int irq, struct irq_desc *desc)
{
	struct gpio_chip *chip = irq_get_handler_data(irq);
	gpio_eic_handler(irq, chip);
}

static struct irqaction __d_gpio_irq = {
	.name = "gpio",
	.flags = IRQF_DISABLED | IRQF_NO_SUSPEND,
	.handler = gpio_muxed_handler,
	.dev_id = &d_sci_gpio.chip,
};

static struct irqaction __d_eic_irq = {
	.name = "eic",
	.flags = IRQF_DISABLED | IRQF_NO_SUSPEND,
	.handler = gpio_muxed_handler,
	.dev_id = &d_sci_eic.chip,
};

#if (NR_GPIO_IRQS < ARCH_NR_GPIOS)
#error "NR_GPIO_IRQS is not match with the sum of builtin/SoC GPIOs and EICs"
#endif

#ifdef CONFIG_OF
static void gpio_irq_init(int irq, struct gpio_chip *gpiochip,
			  struct irq_chip *irqchip)
{
	int n, i;
	struct sci_gpio_chip *sci_gpio = to_sci_gpio(gpiochip);
	struct irq_domain *irq_domain = sci_gpio->irq_domain;

	/* setup the cascade irq handlers */
	if (sci_gpio->is_adi_gpio) { /*TODO*/
		irq_set_chained_handler(irq, gpio_muxed_flow_handler);
		irq_set_handler_data(irq, gpiochip);
	}

	for (i = 0; i < gpiochip->ngpio; i++) {
		n = irq_create_mapping(irq_domain, i);
		irq_set_chip_and_handler(n, irqchip, handle_level_irq);
		irq_set_chip_data(n, gpiochip);
		set_irq_flags(n, IRQF_VALID);
	}
}
#else
static void gpio_irq_init(int irq, struct gpio_chip *gpiochip,
			  struct irq_chip *irqchip)
{
	int n = gpiochip->to_irq(gpiochip, 0);
	int irqend = n + gpiochip->ngpio;

	/* setup the cascade irq handlers */
	if (irq >= NR_SCI_PHY_IRQS) {
		irq_set_chained_handler(irq, gpio_muxed_flow_handler);
		irq_set_handler_data(irq, gpiochip);
	}

	for (; n < irqend; n++) {
		irq_set_chip_and_handler(n, irqchip, handle_level_irq);
		irq_set_chip_data(n, gpiochip);
		set_irq_flags(n, IRQF_VALID);
	}
}

#endif

#ifdef CONFIG_OF
struct sprd_gpio_match_data{
	struct sci_gpio_chip *sci_gpio_chip;
	struct irq_chip *irq_chip;
	struct irqaction *irqaction;
};
static struct sprd_gpio_match_data d_eic_match = {
	.sci_gpio_chip = &d_sci_eic,
	.irq_chip = &d_eic_irq_chip,
	.irqaction = &__d_eic_irq,
};

static struct sprd_gpio_match_data d_gpio_match = {
	.sci_gpio_chip = &d_sci_gpio,
	.irq_chip = &d_gpio_irq_chip,
	.irqaction = &__d_gpio_irq,
};
static struct sprd_gpio_match_data a_eic_match = {
	.sci_gpio_chip = &a_sci_eic,
	.irq_chip = &a_eic_irq_chip,
	.irqaction = NULL,
};
static struct sprd_gpio_match_data a_gpio_match = {
	.sci_gpio_chip = &a_sci_gpio,
	.irq_chip = &a_gpio_irq_chip,
	.irqaction = NULL,
};
static struct of_device_id eic_gpio_match_table[] = {
	{ .compatible = "sprd,d-eic-gpio", .data = &d_eic_match},
	{ .compatible = "sprd,d-gpio-gpio", .data = &d_gpio_match},
	{ .compatible = "sprd,a-eic-gpio", .data = &a_eic_match},
	{ .compatible = "sprd,a-gpio-gpio", .data = &a_gpio_match},
	{ },
};

static int eic_gpio_probe(struct platform_device *pdev)
{
	struct eic_gpio_resource *r = pdev->dev.platform_data;
	struct of_device_id *match;
	struct sprd_gpio_match_data *match_data;
	struct device_node *np = pdev->dev.of_node;
	struct sci_gpio_chip *sgc;
	struct irq_chip *ic;
	struct irqaction *ia;
	static bool init_done = false;
	struct resource *res;
	struct irq_domain *irq_domain;
	int irq;

	if (!np && !r)
		BUG();

	if(!init_done){
	/* enable EIC */
#if defined(CONFIG_ARCH_SC8825)
	sci_glb_set(REG_GLB_GEN0, BIT_EIC_EB);
	sci_glb_set(REG_GLB_GEN0, BIT_GPIO_EB);
	sci_glb_set(REG_GLB_GEN0, BIT_RTC_EIC_EB);
	sci_adi_set(ANA_REG_GLB_ANA_APB_CLK_EN,
		    BIT_ANA_EIC_EB | BIT_ANA_GPIO_EB | BIT_ANA_RTC_EIC_EB);
#elif (defined(CONFIG_ARCH_SCX15)||defined(CONFIG_ADIE_SC2723S)||defined(CONFIG_ADIE_SC2723))
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_EIC_EN);
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_EIC_EN);
#elif defined(CONFIG_ARCH_SCX35)
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN,
		    BIT_ANA_EIC_EN | BIT_ANA_GPIO_EN);
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_EIC_EN);
#endif
	init_done = true;
	}

	match = of_match_device(eic_gpio_match_table, &pdev->dev);
	match_data = match->data;
	sgc = match_data->sci_gpio_chip;
	ic = match_data->irq_chip;
	ia = match_data->irqaction;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res){
		dev_err(&pdev->dev, "No reg of property specified\n");
		return -ENODEV;
	}

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "memory resource busy\n");
		return -EBUSY;
	}

	sgc->base_addr = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(!res)
		irq = -1;
	else
		irq = res->start;

	if (of_property_read_u32(np, "ngpios", &sgc->chip.ngpio)) {
		dev_err(&pdev->dev, "No ngpios of property specified\n");
		release_mem_region(res->start, resource_size(res));
		return -ENODEV;
	}
	if (of_property_read_u32(np, "gpiobase", &sgc->chip.base)) {
		dev_err(&pdev->dev, "No gpiobase of property specified\n");
		sgc->chip.base = -1;
	}

	dev_info(&pdev->dev, "base_addr %p, gpio base %d, ngpio %d, irq %d", \
		sgc->base_addr, sgc->chip.base, sgc->chip.ngpio, irq);

#ifdef CONFIG_OF
	sgc->chip.of_node = np; 
#endif
	gpiochip_add(&sgc->chip);

	irq_domain = irq_domain_add_linear(np, sgc->chip.ngpio,
			&irq_domain_simple_ops, NULL);
	if(!irq_domain){
		dev_err(&pdev->dev, "failed to add irq domain\n");
		gpiochip_remove(&sgc->chip);
		release_mem_region(res->start, resource_size(res));
		return -ENODEV;
	}
	sgc->irq_domain = irq_domain;

	if (-1 != irq) {
		gpio_irq_init(irq, &sgc->chip, ic);
		if(ia != NULL)
			setup_irq(irq, ia);
	}

	return 0;
}

static int eic_gpio_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct of_device_id *match;
	struct sprd_gpio_match_data *match_data;
	struct sci_gpio_chip *sgc;

	match = of_match_device(eic_gpio_match_table, &pdev->dev);
	match_data = match->data;
	sgc = match_data->sci_gpio_chip;
	ret = gpiochip_remove(&sgc->chip);

	return ret;
}
#else
static int eic_gpio_probe(struct platform_device *pdev)
{
	struct eic_gpio_resource *r = pdev->dev.platform_data;
	struct irq_domain *irq_domain;
	if (!r)
		BUG();

	/* enable EIC */
#if defined(CONFIG_ARCH_SC8825)
	sci_glb_set(REG_GLB_GEN0, BIT_EIC_EB);
	sci_glb_set(REG_GLB_GEN0, BIT_GPIO_EB);
	sci_glb_set(REG_GLB_GEN0, BIT_RTC_EIC_EB);
	sci_adi_set(ANA_REG_GLB_ANA_APB_CLK_EN,
		    BIT_ANA_EIC_EB | BIT_ANA_GPIO_EB | BIT_ANA_RTC_EIC_EB);
#elif defined(CONFIG_ARCH_SCX15)
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_EIC_EN);
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_EIC_EN);
#elif defined(CONFIG_ARCH_SCX35)
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN,
		    BIT_ANA_EIC_EN | BIT_ANA_GPIO_EN);
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_EIC_EN);
#endif

	d_sci_gpio.base_addr = r[ENUM_ID_D_GPIO].base_addr;
	d_sci_gpio.chip.base = r[ENUM_ID_D_GPIO].chip_base;
	d_sci_gpio.chip.ngpio = r[ENUM_ID_D_GPIO].chip_ngpio;

	d_sci_eic.base_addr = r[ENUM_ID_D_EIC].base_addr;
	d_sci_eic.chip.base = r[ENUM_ID_D_EIC].chip_base;
	d_sci_eic.chip.ngpio = r[ENUM_ID_D_EIC].chip_ngpio;

	a_sci_gpio.base_addr = r[ENUM_ID_A_GPIO].base_addr;
	a_sci_gpio.chip.base = r[ENUM_ID_A_GPIO].chip_base;
	a_sci_gpio.chip.ngpio = r[ENUM_ID_A_GPIO].chip_ngpio;

	a_sci_eic.base_addr = r[ENUM_ID_A_EIC].base_addr;
	a_sci_eic.chip.base = r[ENUM_ID_A_EIC].chip_base;
	a_sci_eic.chip.ngpio = r[ENUM_ID_A_EIC].chip_ngpio;

	if (d_sci_eic.chip.ngpio > 0)
		gpiochip_add(&d_sci_eic.chip);

	if (d_sci_gpio.chip.ngpio > 0)
		gpiochip_add(&d_sci_gpio.chip);

	if (a_sci_eic.chip.ngpio > 0)
		gpiochip_add(&a_sci_eic.chip);

	if (a_sci_gpio.chip.ngpio > 0)
		gpiochip_add(&a_sci_gpio.chip);

	if (-1 != r[ENUM_ID_D_GPIO].irq) {
		gpio_irq_init(r[ENUM_ID_D_GPIO].irq, &d_sci_gpio.chip,
			      &d_gpio_irq_chip);
		setup_irq(r[ENUM_ID_D_GPIO].irq, &__d_gpio_irq);
	}

	if (-1 != r[ENUM_ID_D_EIC].irq) {
		gpio_irq_init(r[ENUM_ID_D_EIC].irq, &d_sci_eic.chip,
			      &d_eic_irq_chip);
		setup_irq(r[ENUM_ID_D_EIC].irq, &__d_eic_irq);
	}

	if (-1 != r[ENUM_ID_A_GPIO].irq)
		gpio_irq_init(r[ENUM_ID_A_GPIO].irq, &a_sci_gpio.chip,
			      &a_gpio_irq_chip);

	if (-1 != r[ENUM_ID_A_EIC].irq)
		gpio_irq_init(r[ENUM_ID_A_EIC].irq, &a_sci_eic.chip,
			      &a_eic_irq_chip);

	return 0;
}

static int eic_gpio_remove(struct platform_device *pdev)
{
	int ret = 0;

	if (d_sci_eic.chip.ngpio > 0)
		ret += gpiochip_remove(&d_sci_eic.chip);
	if (d_sci_gpio.chip.ngpio > 0)
		ret += gpiochip_remove(&d_sci_gpio.chip);
	if (a_sci_eic.chip.ngpio > 0)
		ret += gpiochip_remove(&a_sci_eic.chip);
	if (a_sci_gpio.chip.ngpio > 0)
		ret += gpiochip_remove(&a_sci_gpio.chip);

	return ret;
}
#endif

static struct platform_driver eic_gpio_driver = {
	.driver.name = "eic-gpio",
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(eic_gpio_match_table),
	.probe = eic_gpio_probe,
	.remove = eic_gpio_remove,
};

static int __init eic_gpio_init(void)
{
	return platform_driver_register(&eic_gpio_driver);
}

postcore_initcall(eic_gpio_init);

static void __exit eic_gpio_exit(void)
{
	return platform_driver_unregister(&eic_gpio_driver);
}

module_exit(eic_gpio_exit);
