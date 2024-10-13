/*
 * s2mps27-irq.c - Interrupt controller support for S2MPS27
 *
 * Copyright (C) 2023 Samsung Electronics Co.Ltd
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
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/samsung/pmic/s2mps27.h>
#include <linux/samsung/pmic/s2mps27-regulator.h>
#include <linux/input.h>

/* VGPIO_RX_MONITOR ADDR. */
#define SPMI_MASTER_PMIC		0x12960000
#define VGPIO_MONITOR_ADDR		0x0000
#define VGPIO_MONITOR_ADDR2		0x0004

/* VGPIO_SYSREG ADDR. */
#define INTCOMB_VGPIO2AP		0x12930000
#define INTCOMB_VGPIO2PMU		0x12950000
#define INTC0_IPEND			0x0290

#define S2MPS27_VGI_CNT			6

static uint8_t irq_reg[S2MPS27_IRQ_GROUP_NR] = {0};

static const uint8_t s2mps27_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[S2MPS27_PMIC_INT1] = S2MPS27_PM1_INT1M,
	[S2MPS27_PMIC_INT2] = S2MPS27_PM1_INT2M,
	[S2MPS27_PMIC_INT3] = S2MPS27_PM1_INT3M,
	[S2MPS27_PMIC_INT4] = S2MPS27_PM1_INT4M,
	[S2MPS27_PMIC_INT5] = S2MPS27_PM1_INT5M,
	[S2MPS27_ADC_INTP]  = S2MPS27_ADC_ADC_INTM,
};

static struct i2c_client *get_i2c(struct s2mps27_dev *s2mps27,
				enum s2mps27_irq_source src)
{
	switch (src) {
	case S2MPS27_PMIC_INT1 ... S2MPS27_PMIC_INT5:
		return s2mps27->pm1;
	case S2MPS27_ADC_INTP:
		return s2mps27->adc;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mps27_irq_data {
	int mask;
	enum s2mps27_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
/* Note : 'PWRONP' should be go faster than 'PWRONR' */
static const struct s2mps27_irq_data s2mps27_irqs[] = {
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRONP_INT1,		S2MPS27_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRONR_INT1,		S2MPS27_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_JIGONBF_INT1,		S2MPS27_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_JIGONBR_INT1,		S2MPS27_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_ACOKBF_INT1,		S2MPS27_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_ACOKBR_INT1,		S2MPS27_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRON1S_INT1,		S2MPS27_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_MRB_INT1,			S2MPS27_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTC60S_INT2,		S2MPS27_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTCA1_INT2,		S2MPS27_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTCA0_INT2,		S2MPS27_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_SMPL_INT2,			S2MPS27_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTC1S_INT2,		S2MPS27_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WTSR_INT2,			S2MPS27_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_SPMI_LDO_OK_FAIL_INT2,	S2MPS27_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WRSTB_INT2,		S2MPS27_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_INT120C_INT3,		S2MPS27_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_INT140C_INT3,		S2MPS27_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_TSD_INT3,			S2MPS27_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OVP_INT3,			S2MPS27_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_TX_TRAN_FAIL_INT3,		S2MPS27_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OTP_CSUM_ERR_INT3,		S2MPS27_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_VOLDNR_INT3,		S2MPS27_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_VOLDNP_INT3,		S2MPS27_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OCP_SR1_INT4,		S2MPS27_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OCP_BB1_INT4,		S2MPS27_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_SR1_INT4,		S2MPS27_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_BB1_INT4,		S2MPS27_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR0_INT4,		S2MPS27_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR1_INT4,		S2MPS27_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR2_INT4,		S2MPS27_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR3_INT4,		S2MPS27_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L1_INT5,		S2MPS27_PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L7_INT5,		S2MPS27_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L8_INT5,		S2MPS27_PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L9_INT5,		S2MPS27_PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L10_INT5,		S2MPS27_PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L11_INT5,		S2MPS27_PMIC_INT5, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WDT_INT5,			S2MPS27_PMIC_INT5, 1 << 6),

};

static void s2mps27_irq_lock(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mps27->irqlock);
}

static void s2mps27_irq_sync_unlock(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mps27_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mps27, i);

		if (mask_reg == S2MPS27_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mps27->irq_masks_cache[i] = s2mps27->irq_masks_cur[i];

		s2mps27_write_reg(i2c, s2mps27_mask_reg[i],
				s2mps27->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mps27->irqlock);
}

static const inline struct s2mps27_irq_data *
irq_to_s2mps27_irq(struct s2mps27_dev *s2mps27, int irq)
{
	return &s2mps27_irqs[irq - s2mps27->irq_base];
}

static void s2mps27_irq_mask(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	const struct s2mps27_irq_data *irq_data =
	    irq_to_s2mps27_irq(s2mps27, data->irq);

	if (irq_data->group >= S2MPS27_IRQ_GROUP_NR)
		return;

	s2mps27->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mps27_irq_unmask(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	const struct s2mps27_irq_data *irq_data =
	    irq_to_s2mps27_irq(s2mps27, data->irq);

	if (irq_data->group >= S2MPS27_IRQ_GROUP_NR)
		return;

	s2mps27->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mps27_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mps27_irq_lock,
	.irq_bus_sync_unlock	= s2mps27_irq_sync_unlock,
	.irq_mask		= s2mps27_irq_mask,
	.irq_unmask		= s2mps27_irq_unmask,
};

static void s2mps27_report_irq(struct s2mps27_dev *s2mps27)
{
	int i;

	/* Apply masking */
	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mps27->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPS27_IRQ_NR; i++)
		if (irq_reg[s2mps27_irqs[i].group] & s2mps27_irqs[i].mask)
			handle_nested_irq(s2mps27->irq_base + i);
}

static void s2mps27_clear_irq_regs(void)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++)
		irq_reg[i] &= 0x00;
}

static bool s2mps27_is_duplicate_key(uint8_t reg_int, uint8_t mask_status)
{
	if ((irq_reg[reg_int] & mask_status) == mask_status)
		return true;

	return false;
}

static int s2mps27_get_pmic_key(void)
{
	if ((irq_reg[S2MPS27_PMIC_INT3] & 0xC0) == 0xC0)
		return KEY_VOLUMEDOWN;
	else
		return KEY_POWER;
}

static int s2mps27_key_detection(struct s2mps27_dev *s2mps27)
{
	int ret = 0, key = 0;
	uint8_t val, reg_int, reg_status, mask_status, flag_status, key_release, key_press;

	key = s2mps27_get_pmic_key();

	switch (key) {
	case KEY_POWER:
		reg_int = S2MPS27_PMIC_INT1;
		reg_status = S2MPS27_PM1_STATUS1;
		mask_status = 0x03;
		flag_status = S2MPS27_STATUS1_PWRON;
		key_release = S2MPS27_PWRKEY_RELEASE;
		key_press = S2MPS27_PWRKEY_PRESS;
		break;
	case KEY_VOLUMEDOWN:
		reg_int = S2MPS27_PMIC_INT3;
		reg_status = S2MPS27_PM1_STATUS2;
		mask_status = 0xC0;
		flag_status = S2MPS27_STATUS1_MR1B;
		key_release = S2MPS27_VOLDN_RELEASE;
		key_press = S2MPS27_VOLDN_PRESS;
		break;
	default:
		return 0;
	}

	/* Determine release/press edge for PWR_ON key */
	if (s2mps27_is_duplicate_key(reg_int, mask_status)) {
		ret = s2mps27_read_reg(s2mps27->pm1, reg_status, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[reg_int] &= (~mask_status);

		if (val & flag_status) {
			irq_reg[reg_int] = key_release;

			s2mps27_report_irq(s2mps27);
			s2mps27_clear_irq_regs();

			irq_reg[reg_int] = key_press;
		} else {
			irq_reg[reg_int] = key_press;
			s2mps27_report_irq(s2mps27);
			s2mps27_clear_irq_regs();

			irq_reg[reg_int] = key_release;
		}
	}
	return 0;

power_key_err:
	return -1;
}
#if 0
static void s2mps27_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	int i, cnt = 0;

	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++) {
		if (i == S2MPS27_ADC_INTP)
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "ADC_INTP(%#02hhx) ", irq_reg[i]);
		else
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(%#02hhx) ", i + 1, irq_reg[i]);
	}

	pr_info("[PMIC] %s: main pmic interrupt %s\n", __func__, buf);
}
#endif
static void s2mps27_clear_sysreg(struct s2mps27_dev *s2mps27)
{
	uint32_t val = 0;

	/* Clear interrupt pending bit (to AP)*/
	val = readl(s2mps27->sysreg_vgpio2ap);
	writel(val, s2mps27->sysreg_vgpio2ap);

	/* Clear interrupt pending bit (to PMU)*/
	val = readl(s2mps27->sysreg_vgpio2pmu);
	writel(val, s2mps27->sysreg_vgpio2pmu);
}


static void s2mps27_read_vgpio_rx_monitor(struct s2mps27_dev *s2mps27, uint8_t *vgi_src)
{
	uint32_t val = 0, i = 0;

	val = readl(s2mps27->mem_base);

	for (i = 0; i < S2MPS27_VGI_CNT; i++) {
		if (i == 4) /* VOL_DN */
			val = readl(s2mps27->mem_base2);

		vgi_src[i] = val & 0x0F;
		val = (val >> 8);
	}
}

static irqreturn_t s2mps27_irq_thread(int irq, void *data)
{
	struct s2mps27_dev *s2mps27 = data;
	uint8_t vgi_src[S2MPS27_VGI_CNT] = {0};
	int ret = 0;

	s2mps27_clear_sysreg(s2mps27);
	s2mps27_read_vgpio_rx_monitor(s2mps27, vgi_src);

	if (!(vgi_src[4] & S2MPS27_VGI4_IRQ_M) && !(vgi_src[1] & S2MPS27_VGI1_IRQ_S1)
		&& !(vgi_src[1] & S2MPS27_VGI1_IRQ_S2) && !(vgi_src[2] & S2MPS27_VGI2_IRQ_S3)
	       	&& !(vgi_src[4] & S2MPS27_VGI4_IRQ_S4) && !(vgi_src[5] & S2MPS27_VGI5_IRQ_S5)
		&& !(vgi_src[1] & S2MPS27_VGI1_IRQ_EXTRA)) {
		pr_info("[PMIC] %s: r0(%#hhx) r1(%#hhx) r2(%#hhx) r3(%#hhx) r4(%#hhx), r5(%#hhx)\n",
			__func__, vgi_src[0], vgi_src[1], vgi_src[2], vgi_src[3], vgi_src[4], vgi_src[5]);
		return IRQ_HANDLED;
	}

	/* notify Main PMIC */
	if (vgi_src[4] & S2MPS27_VGI4_IRQ_M) {
		ret = s2mps27_bulk_read(s2mps27->pm1, S2MPS27_PM1_INT1,
				S2MPS27_NUM_IRQ_PMIC_REGS, &irq_reg[S2MPS27_PMIC_INT1]);
		if (ret) {
			pr_err("%s: %s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}
		// not use, but Enable when used later
		//ret = s2mps27_bulk_read(s2mps27->adc, S2MPS27_ADC_ADC_INTP,
		//		S2MPS27_NUM_IRQ_ADC_REGS, &irq_reg[S2MPS27_ADC_INTP]);
		//if (ret) {
		//	pr_err("%s: %s Failed to read adc interrupt: %d\n",
		//		MFD_DEV_NAME, __func__, ret);
		//	return IRQ_HANDLED;
		//}

		// queue_delayed_work(s2mps27->irq_wqueue, &s2mps27->irq_work, 0);
		pr_info("[PMIC] %s: %#hhx %#hhx %#hhx %#hhx %#hhx\n",
				__func__, irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3], irq_reg[4]);

		ret = s2mps27_key_detection(s2mps27);
		if (ret < 0)
			pr_err("%s: KEY detection error\n", __func__);

		/* Report IRQ */
		s2mps27_report_irq(s2mps27);
	}

#if IS_ENABLED(CONFIG_MFD_S2MPS28)
	if (vgi_src[1] & S2MPS27_VGI1_IRQ_S1) {
		pr_info("%s: IBI from sub-1 pmic\n", __func__);
		s2mps28_1_call_notifier();
	}

	if (vgi_src[1] & S2MPS27_VGI1_IRQ_S2) {
		pr_info("%s: IBI from sub-2 pmic\n", __func__);
		s2mps28_2_call_notifier();
	}

	if (vgi_src[2] & S2MPS27_VGI2_IRQ_S3) {
		pr_info("%s: IBI from sub-3 pmic\n", __func__);
		s2mps28_3_call_notifier();
	}

	if (vgi_src[4] & S2MPS27_VGI4_IRQ_S4) {
		pr_info("%s: IBI from sub-4 pmic\n", __func__);
		s2mps28_4_call_notifier();
	}

	if (vgi_src[5] & S2MPS27_VGI5_IRQ_S5) {
		pr_info("%s: IBI from sub-5 pmic\n", __func__);
		s2mps28_5_call_notifier();
	}
#endif

#if IS_ENABLED(CONFIG_MFD_S2MPA05_9945)
	if (vgi_src[1] & S2MPS27_VGI1_IRQ_EXTRA) {
		pr_info("%s: IBI from extra pmic\n", __func__);
		s2mpa05_call_notifier();
	}
#endif
	return IRQ_HANDLED;
}

#if 0
static int s2mps27_set_wqueue(struct s2mps27_dev *s2mps27)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mps27->dev), dev_name(s2mps27->dev));

	s2mps27->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mps27->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return 1;
	}

	INIT_DELAYED_WORK(&s2mps27->irq_work, s2mps27_irq_work_func);

	return 0;
}
#endif

static void s2mps27_set_vgpio_monitor(struct s2mps27_dev *s2mps27)
{
	s2mps27->mem_base = ioremap(SPMI_MASTER_PMIC + VGPIO_MONITOR_ADDR, SZ_32);
	if (s2mps27->mem_base == NULL)
		pr_info("%s: fail to allocate mem_base\n", __func__);

	/* VOL_DN */
	s2mps27->mem_base2 = ioremap(SPMI_MASTER_PMIC + VGPIO_MONITOR_ADDR2, SZ_32);
	if (s2mps27->mem_base2 == NULL)
		pr_info("%s: fail to allocate mem_base2\n", __func__);
}

static void s2mps27_set_sysreg(struct s2mps27_dev *s2mps27)
{
	s2mps27->sysreg_vgpio2ap = ioremap(INTCOMB_VGPIO2AP + INTC0_IPEND, SZ_32);
	if (s2mps27->sysreg_vgpio2ap == NULL)
		pr_info("%s: fail to allocate sysreg_vgpio2ap\n", __func__);

	s2mps27->sysreg_vgpio2pmu = ioremap(INTCOMB_VGPIO2PMU + INTC0_IPEND, SZ_32);
	if (s2mps27->sysreg_vgpio2pmu == NULL)
		pr_info("%s: fail to allocate sysreg_vgpio2pmu\n", __func__);
}

int s2mps27_irq_init(struct s2mps27_dev *s2mps27)
{
	int cur_irq, ret, i;
	static char irq_name[32] = {0, };

	if (!s2mps27->irq_base) {
		dev_err(s2mps27->dev, "[PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mps27->irqlock);

	s2mps27_set_vgpio_monitor(s2mps27);
	s2mps27_set_sysreg(s2mps27);

#if 0	/* Set workqueue */
	s2mps27_set_wqueue(s2mps27);
#endif
	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mps27->irq_masks_cur[i] = 0xff;
		s2mps27->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mps27, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mps27_mask_reg[i] == S2MPS27_REG_INVALID)
			continue;

		s2mps27_write_reg(i2c, s2mps27_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPS27_IRQ_NR; i++) {
		cur_irq = i + s2mps27->irq_base;
		irq_set_chip_data(cur_irq, s2mps27);
		irq_set_chip_and_handler(cur_irq, &s2mps27_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	/* Dynamic allocation for device name */
	snprintf(irq_name, sizeof(irq_name) - 1, "%s-irq@%s",
		 dev_driver_string(s2mps27->dev), dev_name(s2mps27->dev));

	s2mps27->irq = irq_of_parse_and_map(s2mps27->dev->of_node, 0);
	ret = devm_request_threaded_irq(s2mps27->dev, s2mps27->irq, NULL, s2mps27_irq_thread,
				   IRQF_ONESHOT, irq_name, s2mps27);

	if (ret) {
		dev_err(s2mps27->dev, "Failed to request IRQ %d: %d\n",
			s2mps27->irq, ret);
		destroy_workqueue(s2mps27->irq_wqueue);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps27_irq_init);

void s2mps27_irq_exit(struct s2mps27_dev *s2mps27)
{
	if (s2mps27->irq)
		free_irq(s2mps27->irq, s2mps27);

	iounmap(s2mps27->mem_base);
	iounmap(s2mps27->mem_base2);
	iounmap(s2mps27->sysreg_vgpio2ap);
	iounmap(s2mps27->sysreg_vgpio2pmu);

	// cancel_delayed_work_sync(&s2mps27->irq_work);
	// destroy_workqueue(s2mps27->irq_wqueue);
}
EXPORT_SYMBOL_GPL(s2mps27_irq_exit);
