/*
 * s2mpu15-irq.c - Interrupt controller support for S2MPU15
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
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
#include <linux/samsung/pmic/s2mpu15.h>
#include <linux/samsung/pmic/s2mpu15-regulator-8835.h>
#include <linux/input.h>

/* AP: VGPIO_RX_MONITOR ADDR. */
#define SPMI_MASTER_PMIC		0x11960000
#define VGPIO_MONITOR_ADDR		0x0000

/* AP: VGPIO_SYSREG ADDR. */
#define INTCOMB_VGPIO2AP		0x11930000
#define INTCOMB_VGPIO2PMU		0x11950000
#define INTC0_IPEND			0x0290

#define S2MPU15_VGI_CNT		5

static uint8_t irq_reg[S2MPU15_IRQ_GROUP_NR] = {0};

static const uint8_t s2mpu15_mask_reg[] = {
	[S2MPU15_PMIC_INT1] = S2MPU15_PM1_INT1M,
	[S2MPU15_PMIC_INT2] = S2MPU15_PM1_INT2M,
	[S2MPU15_PMIC_INT3] = S2MPU15_PM1_INT3M,
	[S2MPU15_PMIC_INT4] = S2MPU15_PM1_INT4M,
	[S2MPU15_PMIC_INT5] = S2MPU15_PM1_INT5M,
	[S2MPU15_PMIC_INT6] = S2MPU15_PM1_INT6M,
	[S2MPU15_PMIC_INT7] = S2MPU15_PM1_INT7M,
	[S2MPU15_PMIC_INT8] = S2MPU15_PM1_INT8M,
};

static struct i2c_client *get_i2c(struct s2mpu15_dev *s2mpu15,
				enum s2mpu15_irq_source src)
{
	switch (src) {
	case S2MPU15_PMIC_INT1 ... S2MPU15_PMIC_INT8:
		return s2mpu15->pmic1;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpu15_irq_data {
	int mask;
	enum s2mpu15_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mpu15_irq_data s2mpu15_irqs[] = {
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PWRONP_INT1,		S2MPU15_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PWRONR_INT1,		S2MPU15_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_JIGONBF_INT1,		S2MPU15_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_JIGONBR_INT1,		S2MPU15_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_ACOKBF_INT1,		S2MPU15_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_ACOKBR_INT1,		S2MPU15_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PWRON1S_INT1,		S2MPU15_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_MRB_INT1,			S2MPU15_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_RTC60S_INT2,		S2MPU15_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_RTCA1_INT2,		S2MPU15_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_RTCA0_INT2,		S2MPU15_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_SMPL_INT2,			S2MPU15_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_RTC1S_INT2,		S2MPU15_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_WTSR_INT2,			S2MPU15_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,	S2MPU15_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_WRSTB_INT2,		S2MPU15_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_120C_INT3,			S2MPU15_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_140C_INT3,			S2MPU15_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_TSD_INT3,			S2MPU15_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OVP_INT3,			S2MPU15_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_TX_TRAN_FAIL_INT3,		S2MPU15_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OTP_CSUM_ERR_INT3,		S2MPU15_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_VOLDNR_INT3,		S2MPU15_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_VOLDNP_INT3,		S2MPU15_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B1M_INT4,		S2MPU15_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B2M_INT4,		S2MPU15_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B3M_INT4,		S2MPU15_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B4M_INT4,		S2MPU15_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B5M_INT4,		S2MPU15_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B6M_INT4,		S2MPU15_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B7M_INT4,		S2MPU15_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_B8M_INT4,		S2MPU15_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_SR1M_INT5,		S2MPU15_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_SR2M_INT5,		S2MPU15_PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_SR3M_INT5,		S2MPU15_PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OCP_SR4M_INT5,		S2MPU15_PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PARITY_ERR0_INT5,		S2MPU15_PMIC_INT5, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PARITY_ERR1_INT5,		S2MPU15_PMIC_INT5, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PARITY_ERR2_INT5,		S2MPU15_PMIC_INT5, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B1M_INT6,		S2MPU15_PMIC_INT6, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B2M_INT6,		S2MPU15_PMIC_INT6, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B3M_INT6,		S2MPU15_PMIC_INT6, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B4M_INT6,		S2MPU15_PMIC_INT6, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B5M_INT6,		S2MPU15_PMIC_INT6, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B6M_INT6,		S2MPU15_PMIC_INT6, 1 << 5),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B7M_INT6,		S2MPU15_PMIC_INT6, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_B8M_INT6,		S2MPU15_PMIC_INT6, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_SR1M_INT7,		S2MPU15_PMIC_INT7, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_SR2M_INT7,		S2MPU15_PMIC_INT7, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_SR3M_INT7,		S2MPU15_PMIC_INT7, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_SR4M_INT7,		S2MPU15_PMIC_INT7, 1 << 4),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_WDT_INT7,			S2MPU15_PMIC_INT7, 1 << 6),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_PARITY_ERR3_INT7,		S2MPU15_PMIC_INT7, 1 << 7),

	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_L10M_INT8,		S2MPU15_PMIC_INT8, 1 << 0),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_L11M_INT8,		S2MPU15_PMIC_INT8, 1 << 1),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_L29M_INT8,		S2MPU15_PMIC_INT8, 1 << 2),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_L30M_INT8,		S2MPU15_PMIC_INT8, 1 << 3),
	DECLARE_IRQ(S2MPU15_PMIC_IRQ_OI_L31M_INT8,		S2MPU15_PMIC_INT8, 1 << 4),
};

static void s2mpu15_irq_lock(struct irq_data *data)
{
	struct s2mpu15_dev *s2mpu15 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpu15->irqlock);
}

static void s2mpu15_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpu15_dev *s2mpu15 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPU15_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mpu15_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpu15, i);

		if (mask_reg == S2MPU15_REG_INVALID || IS_ERR_OR_NULL(i2c))
			continue;
		s2mpu15->irq_masks_cache[i] = s2mpu15->irq_masks_cur[i];

		s2mpu15_write_reg(i2c, s2mpu15_mask_reg[i],
				s2mpu15->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpu15->irqlock);
}

static const inline struct s2mpu15_irq_data *
irq_to_s2mpu15_irq(struct s2mpu15_dev *s2mpu15, int irq)
{
	return &s2mpu15_irqs[irq - s2mpu15->irq_base];
}

static void s2mpu15_irq_mask(struct irq_data *data)
{
	struct s2mpu15_dev *s2mpu15 = irq_get_chip_data(data->irq);
	const struct s2mpu15_irq_data *irq_data =
	    irq_to_s2mpu15_irq(s2mpu15, data->irq);

	if (irq_data->group >= S2MPU15_IRQ_GROUP_NR)
		return;

	s2mpu15->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpu15_irq_unmask(struct irq_data *data)
{
	struct s2mpu15_dev *s2mpu15 = irq_get_chip_data(data->irq);
	const struct s2mpu15_irq_data *irq_data =
	    irq_to_s2mpu15_irq(s2mpu15, data->irq);

	if (irq_data->group >= S2MPU15_IRQ_GROUP_NR)
		return;

	s2mpu15->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpu15_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpu15_irq_lock,
	.irq_bus_sync_unlock	= s2mpu15_irq_sync_unlock,
	.irq_mask		= s2mpu15_irq_mask,
	.irq_unmask		= s2mpu15_irq_unmask,
};

static void s2mpu15_report_irq(struct s2mpu15_dev *s2mpu15)
{
	int i;

	/* Apply masking */
	for (i = 0; i < S2MPU15_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpu15->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPU15_IRQ_NR; i++)
		if (irq_reg[s2mpu15_irqs[i].group] & s2mpu15_irqs[i].mask)
			handle_nested_irq(s2mpu15->irq_base + i);
}

static void s2mpu15_clear_irq_regs(void)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPU15_IRQ_GROUP_NR; i++)
		irq_reg[i] &= 0x00;
}

static bool s2mpu15_is_duplicate_key(uint8_t reg_int, uint8_t mask_status)
{
	if ((irq_reg[reg_int] & mask_status) == mask_status)
		return true;

	return false;
}

static int s2mpu15_get_pmic_key(void)
{
	if ((irq_reg[S2MPU15_PMIC_INT3] & 0xC0) == 0xC0)
		return KEY_VOLUMEDOWN;
	else
		return KEY_POWER;
}

static int s2mpu15_key_detection(struct s2mpu15_dev *s2mpu15)
{
	int ret = 0, key = 0;
	uint8_t val, reg_int, reg_status, mask_status, flag_status, key_release, key_press;

	key = s2mpu15_get_pmic_key();

	switch (key) {
	case KEY_POWER:
		reg_int = S2MPU15_PMIC_INT1;
		reg_status = S2MPU15_PM1_STATUS1;
		mask_status = 0x03;
		flag_status = S2MPU15_STATUS1_PWRON;
		key_release = S2MPU15_PWRKEY_RELEASE;
		key_press = S2MPU15_PWRKEY_PRESS;
		break;
	case KEY_VOLUMEDOWN:
		reg_int = S2MPU15_PMIC_INT3;
		reg_status = S2MPU15_PM1_STATUS1;
		mask_status = 0xC0;
		flag_status = S2MPU15_STATUS1_MRB;
		key_release = S2MPU15_VOLDN_RELEASE;
		key_press = S2MPU15_VOLDN_PRESS;
		break;
	default:
		return 0;
	}

	/* Determine release/press edge for PWR_ON key */
	if (s2mpu15_is_duplicate_key(reg_int, mask_status)) {
		ret = s2mpu15_read_reg(s2mpu15->pmic1, reg_status, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[reg_int] &= (~mask_status);

		if (val & flag_status) {
			irq_reg[reg_int] = key_release;

			s2mpu15_report_irq(s2mpu15);
			s2mpu15_clear_irq_regs();

			irq_reg[reg_int] = key_press;
		} else {
			irq_reg[reg_int] = key_press;

			s2mpu15_report_irq(s2mpu15);
			s2mpu15_clear_irq_regs();

			irq_reg[reg_int] = key_release;
		}
	}
	return 0;

power_key_err:
	return -EINVAL;
}
#if 0
static void s2mpu15_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	int i, cnt = 0;

	for (i = 0; i < S2MPU15_IRQ_GROUP_NR; i++) {
		if (i == S2MPU15_ADC_INTP)
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "ADC_INTP(%#02hhx) ", irq_reg[i]);
		else
			cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(%#02hhx) ", i + 1, irq_reg[i]);
	}

	pr_info("[PMIC] %s: main pmic interrupt %s\n", __func__, buf);
}
#endif
static void s2mpu15_clear_sysreg(struct s2mpu15_dev *s2mpu15)
{
	uint32_t val = 0;

	/* Clear interrupt pending bit (to AP)*/
	val = readl(s2mpu15->sysreg_vgpio2ap);
	writel(val, s2mpu15->sysreg_vgpio2ap);

	/* Clear interrupt pending bit (to PMU)*/
	val = readl(s2mpu15->sysreg_vgpio2pmu);
	writel(val, s2mpu15->sysreg_vgpio2pmu);
}

static void s2mpu15_read_vgpio_rx_monitor(struct s2mpu15_dev *s2mpu15, uint8_t *vgi_src)
{
	uint32_t val = 0, i = 0;

	val = readl(s2mpu15->mem_base);

	for (i = 0; i < S2MPU15_VGI_CNT; i++) {
		vgi_src[i] = val & 0x0F;
		val = (val >> 8);
	}
}

static irqreturn_t s2mpu15_irq_thread(int irq, void *data)
{
	struct s2mpu15_dev *s2mpu15 = data;
	uint8_t vgi_src[S2MPU15_VGI_CNT] = {0};
	int ret = 0;

	s2mpu15_clear_sysreg(s2mpu15);
	s2mpu15_read_vgpio_rx_monitor(s2mpu15, vgi_src);

	if (!(vgi_src[1] & S2MPU15_VGI0_IRQ_M) && !(vgi_src[1] & S2MPU15_VGI0_IRQ_S1)) {
		pr_info("[PMIC] %s: r0(%#hhx) r1(%#hhx) r2(%#hhx) r3(%#hhx) r4(%#hhx)\n",
			__func__, vgi_src[0], vgi_src[1], vgi_src[2], vgi_src[3], vgi_src[4]);
		return IRQ_HANDLED;
	}

	/* notify Main PMIC */
	if (vgi_src[1] & S2MPU15_VGI0_IRQ_M) {
		ret = s2mpu15_bulk_read(s2mpu15->pmic1, S2MPU15_PM1_INT1,
				S2MPU15_NUM_IRQ_PMIC_REGS, &irq_reg[S2MPU15_PMIC_INT1]);
		if (ret) {
			pr_err("%s: %s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}
#if 0
		queue_delayed_work(s2mpu15->irq_wqueue, &s2mpu15->irq_work, 0);
#endif
		pr_info("[PMIC] %s: %#hhx %#hhx %#hhx %#hhx %#hhx\n",
				__func__, irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3], irq_reg[4]);

		ret = s2mpu15_key_detection(s2mpu15);
		if (ret < 0)
			pr_err("%s: KEY detection error\n", __func__);

		s2mpu15_report_irq(s2mpu15);
	}
#if IS_ENABLED(CONFIG_MFD_S2MPU16_8835)
	if (vgi_src[1] & S2MPU15_VGI0_IRQ_S1) {
		pr_info("%s: IBI from sub pmic\n", __func__);
		s2mpu16_call_notifier();
	}
#endif
	return IRQ_HANDLED;
}
#if 0
static int s2mpu15_set_wqueue(struct s2mpu15_dev *s2mpu15)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mpu15->dev), dev_name(s2mpu15->dev));

	s2mpu15->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mpu15->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return 1;
	}

	INIT_DELAYED_WORK(&s2mpu15->irq_work, s2mpu15_irq_work_func);

	return 0;
}
#endif
static void s2mpu15_set_vgpio_monitor(struct s2mpu15_dev *s2mpu15)
{
	s2mpu15->mem_base = ioremap(SPMI_MASTER_PMIC + VGPIO_MONITOR_ADDR, SZ_32);
	if (s2mpu15->mem_base == NULL)
		pr_info("%s: fail to allocate mem_base\n", __func__);
}

static void s2mpu15_set_sysreg(struct s2mpu15_dev *s2mpu15)
{
	s2mpu15->sysreg_vgpio2ap = ioremap(INTCOMB_VGPIO2AP + INTC0_IPEND, SZ_32);
	if (s2mpu15->sysreg_vgpio2ap == NULL)
		pr_info("%s: fail to allocate sysreg_vgpio2ap\n", __func__);

	s2mpu15->sysreg_vgpio2pmu = ioremap(INTCOMB_VGPIO2PMU + INTC0_IPEND, SZ_32);
	if (s2mpu15->sysreg_vgpio2pmu == NULL)
		pr_info("%s: fail to allocate sysreg_vgpio2pmu\n", __func__);
}

static void s2mpu15_voldn_irq_enable(struct s2mpu15_dev *s2mpu15)
{
	s2mpu15_update_reg(s2mpu15->pmic2, S2MPU15_PM2_OFF_CTRL1, 0x20, 0x20);
}

static void s2mpu15_irq_enable(struct s2mpu15_dev *s2mpu15)
{
	uint8_t i2c_data = 0;

	s2mpu15_voldn_irq_enable(s2mpu15);

	s2mpu15_write_reg(s2mpu15->i2c, S2MPU15_COMMON_TX_MASK, 0xB6);
	s2mpu15_read_reg(s2mpu15->i2c, S2MPU15_COMMON_TX_MASK, &i2c_data);
	dev_info(s2mpu15->dev, "[PMIC] %s: S2MPU15_COMMON_TX_MASK=0x%02hhx\n", __func__, i2c_data);
}

int s2mpu15_irq_init(struct s2mpu15_dev *s2mpu15)
{
	int cur_irq, ret, i;
	static char irq_name[32] = {0, };

	if (!s2mpu15->irq_base) {
		dev_err(s2mpu15->dev, "[PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mpu15->irqlock);

	s2mpu15_set_vgpio_monitor(s2mpu15);
	s2mpu15_set_sysreg(s2mpu15);
#if 0
	s2mpu15_set_wqueue(s2mpu15);
#endif
	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPU15_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpu15->irq_masks_cur[i] = 0xff;
		s2mpu15->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpu15, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpu15_mask_reg[i] == S2MPU15_REG_INVALID)
			continue;

		s2mpu15_write_reg(i2c, s2mpu15_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPU15_IRQ_NR; i++) {
		cur_irq = i + s2mpu15->irq_base;
		irq_set_chip_data(cur_irq, s2mpu15);
		irq_set_chip_and_handler(cur_irq, &s2mpu15_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpu15_irq_enable(s2mpu15);

	/* Dynamic allocation for device name */
	snprintf(irq_name, sizeof(irq_name) - 1, "%s-irq@%s",
		 dev_driver_string(s2mpu15->dev), dev_name(s2mpu15->dev));

	s2mpu15->irq = irq_of_parse_and_map(s2mpu15->dev->of_node, 0);
	ret = devm_request_threaded_irq(s2mpu15->dev, s2mpu15->irq, NULL, s2mpu15_irq_thread,
				   IRQF_ONESHOT, irq_name, s2mpu15);

	if (ret) {
		dev_err(s2mpu15->dev, "Failed to request IRQ %d: %d\n", s2mpu15->irq, ret);
		destroy_workqueue(s2mpu15->irq_wqueue);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu15_irq_init);

void s2mpu15_irq_exit(struct s2mpu15_dev *s2mpu15)
{
	if (s2mpu15->irq)
		free_irq(s2mpu15->irq, s2mpu15);

	iounmap(s2mpu15->mem_base);
	iounmap(s2mpu15->sysreg_vgpio2ap);
	iounmap(s2mpu15->sysreg_vgpio2pmu);
#if 0
	cancel_delayed_work_sync(&s2mpu15->irq_work);
	destroy_workqueue(s2mpu15->irq_wqueue);
#endif
}
EXPORT_SYMBOL_GPL(s2mpu15_irq_exit);
