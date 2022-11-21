/*
 * s2mpw01-irq.c - Interrupt controller support for S2MPW01
 *
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
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
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>

#if defined(CONFIG_SEC_CHARGER_S2MPW01)
#include <linux/power/s2mpw01_charger.h>
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
#include <linux/power/s2mpw01_fuelgauge.h>
#endif

static const u8 s2mpw01_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[PMIC_INT1] = S2MPW01_PMIC_REG_INT1M,
	[PMIC_INT2] = S2MPW01_PMIC_REG_INT2M,
	[PMIC_INT3] = S2MPW01_PMIC_REG_INT3M,
#if defined(CONFIG_SEC_CHARGER_S2MPW01)
	[CHG_INT1] = S2MPW01_CHG_REG_INT1M,
	[CHG_INT2] = S2MPW01_CHG_REG_INT2M,
	[CHG_INT3] = S2MPW01_CHG_REG_INT3M,
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	[FG_INT] = S2MPW01_FG_REG_INTM,
#endif
};

static struct i2c_client *get_i2c(struct s2mpw01_dev *s2mpw01,
				enum s2mpw01_irq_source src)
{
	switch (src) {
	case PMIC_INT1 ... PMIC_INT3:
		return s2mpw01->pmic;
#if defined(CONFIG_SEC_CHARGER_S2MPW01)
	case CHG_INT1 ... CHG_INT3:
		return s2mpw01->charger;
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	case FG_INT:
		return s2mpw01->fuelgauge;
#endif
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpw01_irq_data {
	int mask;
	enum s2mpw01_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)	\
	[(idx)] = {.group = (_group), .mask = (_mask)}
static const struct s2mpw01_irq_data s2mpw01_irqs[] = {
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_PWRONR_INT1,	PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_PWRONF_INT1,	PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_JIGONBF_INT1,	PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_JIGONBR_INT1,	PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_ACOKBF_INT1,	PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_ACOKBR_INT1,	PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_PWRON1S_INT1,	PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_MRB_INT1,		PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPW01_PMIC_IRQ_RTC60S_INT2,	PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_RTCA1_INT2,	PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_RTCA0_INT2,	PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_SMPL_INT2,		PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_RTC1S_INT2,	PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_WTSR_INT2,		PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_WRSTB_INT2,	PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPW01_PMIC_IRQ_120C_INT3,		PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_140C_INT3,		PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPW01_PMIC_IRQ_TSD_INT3,		PMIC_INT3, 1 << 2),
#if defined(CONFIG_SEC_CHARGER_S2MPW01)
	DECLARE_IRQ(S2MPW01_CHG_IRQ_RECHG_INT1,		CHG_INT1, 1 << 0),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CHGDONE_INT1,	CHG_INT1, 1 << 1),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_TOPOFF_INT1,	CHG_INT1, 1 << 2),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_PREECHG_INT1,	CHG_INT1, 1 << 3),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CHGSTS_INT1,	CHG_INT1, 1 << 4),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CIN2BAT_INT1,	CHG_INT1, 1 << 5),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CHGVINOVP_INT1,	CHG_INT1, 1 << 6),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CHGVIN_INT1,	CHG_INT1, 1 << 7),

	DECLARE_IRQ(S2MPW01_CHG_IRQ_ADPATH_INT2,	CHG_INT2, 1 << 4),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_FCHG_INT2,		CHG_INT2, 1 << 5),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_BATDET_INT2,	CHG_INT2, 1 << 7),

	DECLARE_IRQ(S2MPW01_CHG_IRQ_CVOK_INT3,		CHG_INT3, 1 << 0),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_TMROUT_INT3,	CHG_INT3, 1 << 4),
	DECLARE_IRQ(S2MPW01_CHG_IRQ_CHGTSDINT3,		CHG_INT3, 1 << 5),
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	DECLARE_IRQ(S2MPW01_FG_IRQ_VBAT_L_INT,		FG_INT, 1 << 0),
	DECLARE_IRQ(S2MPW01_FG_IRQ_SOC_L_INT,		FG_INT, 1 << 1),
	DECLARE_IRQ(S2MPW01_FG_IRQ_IDLE_ST_INT,		FG_INT, 1 << 2),
	DECLARE_IRQ(S2MPW01_FG_IRQ_INIT_ST_INT,		FG_INT, 1 << 3),
#endif
};

static void s2mpw01_irq_lock(struct irq_data *data)
{
	struct s2mpw01_dev *s2mpw01 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpw01->irqlock);
}

static void s2mpw01_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpw01_dev *s2mpw01 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPW01_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpw01_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpw01, i);

		if (mask_reg == S2MPW01_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mpw01->irq_masks_cache[i] = s2mpw01->irq_masks_cur[i];

		s2mpw01_write_reg(i2c, s2mpw01_mask_reg[i],
				s2mpw01->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpw01->irqlock);
}

static const inline struct s2mpw01_irq_data *
irq_to_s2mpw01_irq(struct s2mpw01_dev *s2mpw01, int irq)
{
	return &s2mpw01_irqs[irq - s2mpw01->irq_base];
}

static void s2mpw01_irq_mask(struct irq_data *data)
{
	struct s2mpw01_dev *s2mpw01 = irq_get_chip_data(data->irq);
	const struct s2mpw01_irq_data *irq_data =
	    irq_to_s2mpw01_irq(s2mpw01, data->irq);

	if (irq_data->group >= S2MPW01_IRQ_GROUP_NR)
		return;

	s2mpw01->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpw01_irq_unmask(struct irq_data *data)
{
	struct s2mpw01_dev *s2mpw01 = irq_get_chip_data(data->irq);
	const struct s2mpw01_irq_data *irq_data =
	    irq_to_s2mpw01_irq(s2mpw01, data->irq);

	if (irq_data->group >= S2MPW01_IRQ_GROUP_NR)
		return;

	s2mpw01->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpw01_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpw01_irq_lock,
	.irq_bus_sync_unlock	= s2mpw01_irq_sync_unlock,
	.irq_mask		= s2mpw01_irq_mask,
	.irq_unmask		= s2mpw01_irq_unmask,
};

static irqreturn_t s2mpw01_irq_thread(int irq, void *data)
{
	struct s2mpw01_dev *s2mpw01 = data;
	struct device *dev = s2mpw01->dev;
	u8 irq_reg[S2MPW01_IRQ_GROUP_NR] = {[0 ... S2MPW01_IRQ_GROUP_NR-1] = 0};
	u8 irq_src;
	int i, ret;

	ret = s2mpw01_read_reg(s2mpw01->i2c,
					S2MPW01_PMIC_REG_INTSRC, &irq_src);
	if (ret) {
		dev_err(dev, "%s() Failed to read interrupt source: %d\n",
				__func__, ret);
		return IRQ_NONE;
	}

	dev_info(dev, "%s() intrrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & S2MPW01_IRQSRC_PMIC) {
		/* PMIC_INT */
		ret = s2mpw01_bulk_read(s2mpw01->pmic, S2MPW01_PMIC_REG_INT1,
				S2MPW01_NUM_IRQ_PMIC_REGS, &irq_reg[PMIC_INT1]);
		if (ret) {
			dev_err(dev, "%s() Failed to read pmic interrupt: %d\n",
				__func__, ret);
			return IRQ_NONE;
		}

		dev_info(dev, "%s() pmic intrrupt(0x%02x, 0x%02x, 0x%02x)\n", __func__,
			irq_reg[PMIC_INT1], irq_reg[PMIC_INT2], irq_reg[PMIC_INT3]);
	}
#ifdef CONFIG_SEC_CHARGER_S2MPW01
	if (irq_src & S2MPW01_IRQSRC_CHG) {
		/* CHG_INT */
		ret = s2mpw01_bulk_read(s2mpw01->charger, S2MPW01_CHG_REG_INT1,
				S2MPW01_NUM_IRQ_CHG_REGS, &irq_reg[CHG_INT1]);
		if (ret) {
			dev_err(dev, "%s() Failed to read charger interrupt: %d\n",
				__func__, ret);
			return IRQ_NONE;
		}

		dev_info(dev, "%s() CHAGER intrrupt(0x%02x, 0x%02x, 0x%02x)\n",
				__func__, irq_reg[CHG_INT1], irq_reg[CHG_INT2],
				irq_reg[CHG_INT3]);
	}
#endif
#ifdef CONFIG_SEC_FUELGAUGE_S2MPW01
	if (irq_src & S2MPW01_IRQSRC_FG) {
		/* FG_INT */
		ret = s2mpw01_read_reg(s2mpw01->fuelgauge, S2MPW01_FG_REG_IRQ,
				&irq_reg[FG_INT]);
		if (ret) {
			pr_err("%s:%s Failed to read charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: fuelgauge interrupt(0x%02x)\n", __func__,
			irq_reg[FG_INT]);

	}
#endif
	/* Apply masking */
	for (i = 0; i < S2MPW01_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpw01->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPW01_IRQ_NR; i++) {
		if (irq_reg[s2mpw01_irqs[i].group] & s2mpw01_irqs[i].mask)
			handle_nested_irq(s2mpw01->irq_base + i);
	}

	return IRQ_HANDLED;
}

int s2mpw01_irq_init(struct s2mpw01_dev *s2mpw01)
{
	struct device *dev = s2mpw01->dev;
	int i;
	int ret;
	u8 i2c_data;

	if (!s2mpw01->irq_gpio) {
		dev_warn(dev, "No interrupt specified.\n");
		s2mpw01->irq_base = 0;
		return 0;
	}

	if (!s2mpw01->irq_base) {
		dev_err(dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mpw01->irqlock);

	s2mpw01->irq = gpio_to_irq(s2mpw01->irq_gpio);

	dev_info(dev, "%s() irq_base=%d, irq->gpio=%d\n", __func__,
			s2mpw01->irq_base, s2mpw01->irq_gpio);

	ret = gpio_request(s2mpw01->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(dev, "%s: failed requesting gpio %d\n",
			__func__, s2mpw01->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mpw01->irq_gpio);
	gpio_free(s2mpw01->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPW01_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpw01->irq_masks_cur[i] = 0xff;
		s2mpw01->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpw01, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpw01_mask_reg[i] == S2MPW01_REG_INVALID)
			continue;

		s2mpw01_write_reg(i2c, s2mpw01_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPW01_IRQ_NR; i++) {
		int cur_irq;
		cur_irq = i + s2mpw01->irq_base;
		irq_set_chip_data(cur_irq, s2mpw01);
		irq_set_chip_and_handler(cur_irq, &s2mpw01_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_INTSRC_MASK, 0xFF);
	ret = s2mpw01_read_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n", MFD_DEV_NAME, __func__);
		return ret;
	}

	/* Unmask pmic interrupt : already sub-masked its section. */
	i2c_data &= ~(S2MPW01_IRQSRC_PMIC);
#if defined(CONFIG_SEC_CHARGER_S2MPW01)
	/* Unmask charger interrupt */
	i2c_data &= ~(S2MPW01_IRQSRC_CHG);
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	/* Unmask charger interrupt */
	i2c_data &= ~(S2MPW01_IRQSRC_FG);
#endif

	s2mpw01_write_reg(s2mpw01->i2c, S2MPW01_PMIC_REG_INTSRC_MASK, i2c_data);

	ret = request_threaded_irq(s2mpw01->irq, NULL, s2mpw01_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "s2mpw01-irq", s2mpw01);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d: %d\n",
			s2mpw01->irq, ret);
		return ret;
	}

	return 0;
}

void s2mpw01_irq_exit(struct s2mpw01_dev *s2mpw01)
{
	if (s2mpw01->irq)
		free_irq(s2mpw01->irq, s2mpw01);
}
