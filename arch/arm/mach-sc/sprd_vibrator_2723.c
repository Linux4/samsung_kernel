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

#include <mach/sci_glb_regs.h>
#define ANA_VIBRATOR_CTRL0      (ANA_REGS_GLB_BASE + 0xf8)

#define CUR_DRV_CAL_SEL	(0x0 << 12)
#define SLP_LDOVIBR_PD_EN	(0x1 << 9)
#define LDO_VIBR_PD	(0x1 << 8)
#define LDO_VIBR_V	(0xD2)

static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static int vibe_state = 0;

static inline uint32_t vibrator_read(uint32_t reg)
{
	return sci_adi_read(reg);
}

static void set_vibrator(int on)
{
	if (on){
		sci_adi_clr(ANA_VIBRATOR_CTRL0, LDO_VIBR_PD);
		sci_adi_clr(ANA_VIBRATOR_CTRL0, SLP_LDOVIBR_PD_EN);
		printk("v_reg:0x%08x\n",ANA_VIBRATOR_CTRL0);
		printk("v_reg_value:0x%08x\n",vibrator_read(ANA_VIBRATOR_CTRL0));

		}
	else{
			sci_adi_write(ANA_VIBRATOR_CTRL0, LDO_VIBR_PD, LDO_VIBR_PD);
			sci_adi_write(ANA_VIBRATOR_CTRL0, SLP_LDOVIBR_PD_EN, SLP_LDOVIBR_PD_EN);
		}
}

static void vibrator_hw_init(void)
{
	sci_adi_write(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_VIBR_EN, BIT_RTC_VIBR_EN);
	sci_adi_write(ANA_VIBRATOR_CTRL0,CUR_DRV_CAL_SEL,CUR_DRV_CAL_SEL);
    
      sci_adi_clr(ANA_VIBRATOR_CTRL0, 0xFF);
	sci_adi_write(ANA_VIBRATOR_CTRL0,LDO_VIBR_V,LDO_VIBR_V);
}

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
		vibe_state = 1;
		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
		printk("vibrator_enable:value = %d\n",value);
		printk("vibrator_enable:vibe_state = %d\n",vibe_state);

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

	printk("locate in sprd_init_vibrator!\n");
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

