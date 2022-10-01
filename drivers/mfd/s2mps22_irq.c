/*
 * s2mps22-irq.c - Interrupt controller support for S2MPS22
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mps22.h>
#include <linux/mfd/samsung/s2mps22-regulator.h>

static const u8 s2mps22_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[S2MPS22_PMIC_INT1] = S2MPS22_REG_INT1M,
	[S2MPS22_PMIC_INT2] = S2MPS22_REG_INT2M,
	[S2MPS22_PMIC_INT3] = S2MPS22_REG_INT3M,
	[S2MPS22_PMIC_INT4] = S2MPS22_REG_INT4M,
	[S2MPS22_PMIC_INT5] = S2MPS22_REG_INT5M,
	[S2MPS22_PMIC_INT6] = S2MPS22_REG_INT6M,
	[S2MPS22_PMIC_INT7] = S2MPS22_REG_INT7M,
	[S2MPS22_PMIC_INT8] = S2MPS22_REG_INT8M,
};

static struct i2c_client *get_i2c(struct s2mps22_dev *s2mps22,
				  enum s2mps22_irq_source src)
{
	switch (src) {
	case S2MPS22_PMIC_INT1 ... S2MPS22_PMIC_INT8:
		return s2mps22->pmic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mps22_irq_data {
	int mask;
	enum s2mps22_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mps22_irq_data s2mps22_irqs[] = {
	DECLARE_IRQ(S2MPS22_IRQ_PWRONF_INT1,		S2MPS22_PMIC_INT1, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_PWRONR_INT1,		S2MPS22_PMIC_INT1, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_INT120C_INT1,		S2MPS22_PMIC_INT1, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_INT140C_INT1,		S2MPS22_PMIC_INT1, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_TSD_INT1,		S2MPS22_PMIC_INT1, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_WTSR_INT1,		S2MPS22_PMIC_INT1, SetBit(5)),
	DECLARE_IRQ(S2MPS22_IRQ_WRSTB_INT1,		S2MPS22_PMIC_INT1, SetBit(6)),
	DECLARE_IRQ(S2MPS22_IRQ_ADCDONE_INT1,		S2MPS22_PMIC_INT1, SetBit(7)),

	DECLARE_IRQ(S2MPS22_IRQ_OC1_INT2,		S2MPS22_PMIC_INT2, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_OC2_INT2,		S2MPS22_PMIC_INT2, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_OC3_INT2,		S2MPS22_PMIC_INT2, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_OC4_INT2,		S2MPS22_PMIC_INT2, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_OC5_INT2,		S2MPS22_PMIC_INT2, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_OC6_INT2,		S2MPS22_PMIC_INT2, SetBit(5)),
	DECLARE_IRQ(S2MPS22_IRQ_OC7_INT2,		S2MPS22_PMIC_INT2, SetBit(6)),
	DECLARE_IRQ(S2MPS22_IRQ_OC8_INT2,		S2MPS22_PMIC_INT2, SetBit(7)),

	DECLARE_IRQ(S2MPS22_IRQ_OCP_B1_INT3,		S2MPS22_PMIC_INT3, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_OCP_B2_INT3,		S2MPS22_PMIC_INT3, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_OCP_B3_INT3,		S2MPS22_PMIC_INT3, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_OCP_B4_INT3,		S2MPS22_PMIC_INT3, SetBit(3)),

	DECLARE_IRQ(S2MPS22_IRQ_OI_B1_INT4,		S2MPS22_PMIC_INT4, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_OI_B2_INT4,		S2MPS22_PMIC_INT4, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_OI_B3_INT4,		S2MPS22_PMIC_INT4, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_OI_B4_INT4,		S2MPS22_PMIC_INT4, SetBit(3)),

	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO1_INT5,		S2MPS22_PMIC_INT5, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO2_INT5,		S2MPS22_PMIC_INT5, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO3_INT5,		S2MPS22_PMIC_INT5, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO4_INT5,		S2MPS22_PMIC_INT5, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO5_INT5,		S2MPS22_PMIC_INT5, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO6_INT5,		S2MPS22_PMIC_INT5, SetBit(5)),
	DECLARE_IRQ(S2MPS22_IRQ_SC_LDO7_INT5,		S2MPS22_PMIC_INT5, SetBit(6)),

	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT0_F_INT6,	S2MPS22_PMIC_INT6, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT0_R_INT6,	S2MPS22_PMIC_INT6, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT1_F_INT6,	S2MPS22_PMIC_INT6, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT1_R_INT6,	S2MPS22_PMIC_INT6, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT2_F_INT6,	S2MPS22_PMIC_INT6, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_EXT2_R_INT6,	S2MPS22_PMIC_INT6, SetBit(5)),

	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC0_F_INT7,	S2MPS22_PMIC_INT7, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC0_R_INT7,	S2MPS22_PMIC_INT7, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC1_F_INT7,	S2MPS22_PMIC_INT7, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC1_R_INT7,	S2MPS22_PMIC_INT7, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC2_F_INT7,	S2MPS22_PMIC_INT7, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC2_R_INT7,	S2MPS22_PMIC_INT7, SetBit(5)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC3_F_INT7,	S2MPS22_PMIC_INT7, SetBit(6)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC3_R_INT7,	S2MPS22_PMIC_INT7, SetBit(7)),

	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC4_F_INT8,	S2MPS22_PMIC_INT8, SetBit(0)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC4_R_INT8,	S2MPS22_PMIC_INT8, SetBit(1)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC5_F_INT8,	S2MPS22_PMIC_INT8, SetBit(2)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC5_R_INT8,	S2MPS22_PMIC_INT8, SetBit(3)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC6_F_INT8,	S2MPS22_PMIC_INT8, SetBit(4)),
	DECLARE_IRQ(S2MPS22_IRQ_GPIO_GPADC6_R_INT8,	S2MPS22_PMIC_INT8, SetBit(5)),
};

static void s2mps22_irq_lock(struct irq_data *data)
{
	struct s2mps22_dev *s2mps22 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mps22->irqlock);
}

static void s2mps22_irq_sync_unlock(struct irq_data *data)
{
	struct s2mps22_dev *s2mps22 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPS22_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mps22_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mps22, i);

		if (mask_reg == S2MPS22_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mps22->irq_masks_cache[i] = s2mps22->irq_masks_cur[i];

		s2mps22_write_reg(i2c, s2mps22_mask_reg[i],
				  s2mps22->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mps22->irqlock);
}

static const inline struct s2mps22_irq_data *
irq_to_s2mps22_irq(struct s2mps22_dev *s2mps22, int irq)
{
	return &s2mps22_irqs[irq - s2mps22->irq_base];
}

static void s2mps22_irq_mask(struct irq_data *data)
{
	struct s2mps22_dev *s2mps22 = irq_get_chip_data(data->irq);
	const struct s2mps22_irq_data *irq_data = irq_to_s2mps22_irq(s2mps22,
								     data->irq);

	if (irq_data->group >= S2MPS22_IRQ_GROUP_NR)
		return;

	s2mps22->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mps22_irq_unmask(struct irq_data *data)
{
	struct s2mps22_dev *s2mps22 = irq_get_chip_data(data->irq);
	const struct s2mps22_irq_data *irq_data = irq_to_s2mps22_irq(s2mps22,
								     data->irq);

	if (irq_data->group >= S2MPS22_IRQ_GROUP_NR)
		return;

	s2mps22->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mps22_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mps22_irq_lock,
	.irq_bus_sync_unlock	= s2mps22_irq_sync_unlock,
	.irq_mask		= s2mps22_irq_mask,
	.irq_unmask		= s2mps22_irq_unmask,
};

static irqreturn_t s2mps22_irq_thread(int irq, void *data)
{
	struct s2mps22_dev *s2mps22 = data;
	u8 irq_reg[S2MPS22_IRQ_GROUP_NR] = {0}, irq_src;
	int i, ret;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
		 gpio_get_value(s2mps22->irq_gpio));

	ret = s2mps22_read_reg(s2mps22->i2c,
			       S2MPS22_PMIC_REG_INTSRC, &irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & S2MPS22_IRQSRC_PMIC) {
		/* PMIC_INT */
		ret = s2mps22_bulk_read(s2mps22->pmic, S2MPS22_REG_INT1,
					S2MPS22_NUM_IRQ_PMIC_REGS, &irq_reg[S2MPS22_PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s: PMIC interrupt(", __func__);
		for (i = S2MPS22_PMIC_INT1; i <= S2MPS22_PMIC_INT8; i++) {
			pr_info(" 0x%02x", irq_reg[i]);
		}
		pr_info(")\n");
	}

	/* Apply masking */
	for (i = 0; i < S2MPS22_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mps22->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPS22_IRQ_NR; i++) {
		if (irq_reg[s2mps22_irqs[i].group] & s2mps22_irqs[i].mask)
			handle_nested_irq(s2mps22->irq_base + i);
	}

	return IRQ_HANDLED;
}

int s2mps22_irq_init(struct s2mps22_dev *s2mps22)
{
	int i, ret, cur_irq;
	u8 i2c_data;

	if (!s2mps22->irq_gpio) {
		dev_warn(s2mps22->dev, "No interrupt specified.\n");
		s2mps22->irq_base = 0;
		return 0;
	}

	if (!s2mps22->irq_base) {
		dev_err(s2mps22->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mps22->irqlock);

	s2mps22->irq = gpio_to_irq(s2mps22->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
		s2mps22->irq, s2mps22->irq_gpio);

	ret = gpio_request(s2mps22->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(s2mps22->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mps22->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mps22->irq_gpio);
	gpio_free(s2mps22->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPS22_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mps22->irq_masks_cur[i] = 0xff;
		s2mps22->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mps22, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mps22_mask_reg[i] == S2MPS22_REG_INVALID)
			continue;

		s2mps22_write_reg(i2c, s2mps22_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPS22_IRQ_NR; i++) {
		cur_irq = i + s2mps22->irq_base;
		irq_set_chip_data(cur_irq, s2mps22);
		irq_set_chip_and_handler(cur_irq, &s2mps22_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mps22_write_reg(s2mps22->i2c, S2MPS22_PMIC_REG_INTSRC_MASK, 0xff);
	ret = s2mps22_read_reg(s2mps22->i2c, S2MPS22_PMIC_REG_INTSRC_MASK,
			       &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
		       MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(S2MPS22_IRQSRC_PMIC);	/* Unmask pmic interrupt */
	s2mps22_write_reg(s2mps22->i2c, S2MPS22_PMIC_REG_INTSRC_MASK, i2c_data);

	pr_info("%s:%s s2mps22_PMIC_REG_INTSRC_MASK=0x%02x\n",
		MFD_DEV_NAME, __func__, i2c_data);

	ret = request_threaded_irq(s2mps22->irq, NULL, s2mps22_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "s2mps22-irq", s2mps22);

	if (ret) {
		dev_err(s2mps22->dev, "Failed to request IRQ %d: %d\n",
			s2mps22->irq, ret);
		return ret;
	}

	return 0;
}

void s2mps22_irq_exit(struct s2mps22_dev *s2mps22)
{
	if (s2mps22->irq)
		free_irq(s2mps22->irq, s2mps22);
}
