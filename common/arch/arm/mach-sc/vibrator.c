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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/sched.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>

#ifdef CONFIG_SC_VIBRATOR_GPIO
#include <linux/gpio.h>
#include <mach/board.h>
#elif defined(CONFIG_SC_VIBRATOR_POWER)
#include <linux/regulator/consumer.h>
#endif

#ifdef CONFIG_ARCH_SCX35
#include <mach/sci_glb_regs.h>
#define ANA_VIBRATOR_CTRL0      (ANA_REG_GLB_VIBR_CTRL0)
#define ANA_VIBRATOR_CTRL1      (ANA_REG_GLB_VIBR_CTRL1)
#define ANA_VIBRATOR_CTRL2      (ANA_REG_GLB_VIBR_CTRL2)
#define ANA_VIBR_WR_PROT        (ANA_REG_GLB_VIBR_WR_PROT_VALUE)
#define VIBRATOR_REG_UNLOCK     (0x1A2B)
#else
#define SPRD_ANA_BASE           (SPRD_MISC_BASE + 0x600)
#define ANA_REG_BASE            (SPRD_ANA_BASE)
#define ANA_VIBR_WR_PROT        (ANA_REG_BASE + 0x90)
#define ANA_VIBRATOR_CTRL0      (ANA_REG_BASE + 0x74)
#define ANA_VIBRATOR_CTRL1      (ANA_REG_BASE + 0x78)
#define VIBRATOR_REG_UNLOCK     (0xA1B2)
#endif
#define VIBRATOR_REG_LOCK       ((~VIBRATOR_REG_UNLOCK) & 0xffff)
#define VIBRATOR_STABLE_LEVEL   (4)
#ifdef CONFIG_ARCH_SCX15
#define VIBRATOR_INIT_LEVEL     (0x0f)
#define VIBRATOR_INIT_STATE_CNT (0x600)
#else
#define VIBRATOR_INIT_LEVEL     (11)
#define VIBRATOR_INIT_STATE_CNT (10)
#endif

#define VIBR_STABLE_V_SHIFT     (12)
#define VIBR_STABLE_V_MSK       (0x0f << VIBR_STABLE_V_SHIFT)
#define VIBR_INIT_V_SHIFT       (8)
#define VIBR_INIT_V_MSK         (0x0f << VIBR_INIT_V_SHIFT)
#define VIBR_V_BP_SHIFT         (4)
#define VIBR_V_BP_MSK           (0x0f << VIBR_V_BP_SHIFT)
#define VIBR_PD_RST             (1 << 3)
#define VIBR_PD_SET             (1 << 2)
#define VIBR_BP_EN              (1 << 1)
#define VIBR_RTC_EN             (1 << 0)

#ifdef CONFIG_SS_FUNCTION
#define VIBR_PULLDOWN_EN        (1 << 1)
#define LDO_VIBR_PD             (1 << 8)
#endif

#ifdef CONFIG_SC_VIBRATOR_POWER
static struct regulator *vibrator_regulator = NULL;
#endif

static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static int vibe_state = 0;

#ifdef CONFIG_SS_FUNCTION
static void set_vibrator(int on)
{
	if (on)
	{
		printk("###############vibrator on##################\n");
		sci_adi_set(ANA_REG_GLB_BA_CTRL3, VIBR_PULLDOWN_EN);
		sci_adi_clr(ANA_REG_GLB_BA_CTRL2, LDO_VIBR_PD);
	}
	else
	{
		printk("###############vibrator off##################\n");
		sci_adi_set(ANA_REG_GLB_BA_CTRL2, LDO_VIBR_PD);
		sci_adi_clr(ANA_REG_GLB_BA_CTRL3, VIBR_PULLDOWN_EN);
	}
}
static void vibrator_hw_init(void)
{
	return;
}
#else
static void set_vibrator(int on)
{
#ifdef CONFIG_SC_VIBRATOR_GPIO
	gpio_set_value(GPIO_VIBRATOR_INT, on);
#elif defined(CONFIG_SC_VIBRATOR_POWER)
	if (on) {
		regulator_enable(vibrator_regulator);
	}
	else {
		regulator_disable(vibrator_regulator);
	}
#else
	/* unlock vibrator registor */
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_UNLOCK, 0xffff);
#ifdef CONFIG_ARCH_SCX35
	if (on)
		sci_adi_write(ANA_VIBRATOR_CTRL0, BIT_VIBR_PON, BIT_VIBR_PON);
	else
		sci_adi_write(ANA_VIBRATOR_CTRL0, 0, BIT_VIBR_PON);
#else
	sci_adi_clr(ANA_VIBRATOR_CTRL0, VIBR_PD_SET | VIBR_PD_RST);
	if (on)
		sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_PD_RST);
	else
		sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_PD_SET);
#endif
	/* lock vibrator registor */
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_LOCK, 0xffff);
#endif
}

static void vibrator_hw_init(void)
{
#ifdef CONFIG_SC_VIBRATOR_GPIO
	gpio_request(GPIO_VIBRATOR_INT, "vibrator");
	gpio_direction_output(GPIO_VIBRATOR_INT, 0);
#elif defined(CONFIG_SC_VIBRATOR_POWER)
	vibrator_regulator = regulator_get(NULL, "vddcammot");
	regulator_set_voltage(vibrator_regulator, 3300000, 3300000);
#else
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_UNLOCK, 0xffff);
#ifdef CONFIG_ARCH_SCX35
	sci_adi_write(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_VIBR_EN, BIT_RTC_VIBR_EN);
#else
	sci_adi_set(ANA_VIBRATOR_CTRL0, VIBR_RTC_EN);
	sci_adi_clr(ANA_VIBRATOR_CTRL0, VIBR_BP_EN);
#endif
	/* set init current level */
	sci_adi_write(ANA_VIBRATOR_CTRL0,
		      (VIBRATOR_INIT_LEVEL << VIBR_INIT_V_SHIFT),
		      VIBR_INIT_V_MSK);
	sci_adi_write(ANA_VIBRATOR_CTRL0,
		      (VIBRATOR_STABLE_LEVEL << VIBR_STABLE_V_SHIFT),
		      VIBR_STABLE_V_MSK);
	/* set stable current level */
	sci_adi_write(ANA_VIBRATOR_CTRL1, VIBRATOR_INIT_STATE_CNT, 0xffff);
	sci_adi_write(ANA_VIBR_WR_PROT, VIBRATOR_REG_LOCK, 0xffff);
#endif
}

#endif

static void update_vibrator(struct work_struct *work)
{
	set_vibrator(vibe_state);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{

	hrtimer_cancel(&vibe_timer);

	if (value == 0)
		vibe_state = 0;
	else {
		value = (value > 15000) ? 15000 : value;
		vibe_state = 1;
		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}

	schedule_work(&vibrator_work);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	ktime_t re;

	if (hrtimer_active(&vibe_timer)) {
		re = hrtimer_get_remaining(&vibe_timer);
		return ktime_to_ns(re);
	} else
		return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	vibe_state = 0;
	schedule_work(&vibrator_work);

	return HRTIMER_NORESTART;
}

static struct timed_output_dev sprd_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int __init sprd_init_vibrator(void)
{
	vibrator_hw_init();

	INIT_WORK(&vibrator_work, update_vibrator);
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&sprd_vibrator);

	return 0;
}

module_init(sprd_init_vibrator);
MODULE_DESCRIPTION("vibrator driver for spreadtrum Processors");
MODULE_LICENSE("GPL");
