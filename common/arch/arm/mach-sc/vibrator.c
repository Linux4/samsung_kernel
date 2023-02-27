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

#include <linux/module.h>
#include <linux/kernel.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif /*CONFIG_HAS_WAKELOCK*/

#ifdef CONFIG_ARCH_SCX30G
#include <mach/chip_x30g/__regs_ana_sc2723_glb.h>
#define ANA_VIBRATOR_CTRL0      (ANA_REG_GLB_VIBR_CTRL0)
#define BIT_VIBR_PON			(BIT(8))
#endif

#define VIB_ON 1
#define VIB_OFF 0
#define MIN_TIME_MS 50

#if defined(CONFIG_HAS_WAKELOCK)
static struct wake_lock vib_wl;
#endif /*CONFIG_HAS_WAKELOCK*/

static struct workqueue_struct *vib_workqueue;
static struct delayed_work	vibrator_off_work;
extern int get_hw_rev();

static void set_vibrator(int on)
{
	if (on)
		sci_adi_write(ANA_VIBRATOR_CTRL0, 0, BIT_VIBR_PON);
	else
		sci_adi_write(ANA_VIBRATOR_CTRL0, BIT_VIBR_PON, BIT_VIBR_PON);
}

static void vibrator_hw_init(void)
{

	sci_adi_write(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_VIBR_EN, BIT_RTC_VIBR_EN);

	sci_adi_write(ANA_VIBRATOR_CTRL0, 0x03D2, 0xffff);

}
static void vibrator_off_worker(struct work_struct *work)
{
	printk(KERN_NOTICE "Vibrator: %s\n", __func__);

	set_vibrator(VIB_OFF);
#if defined(CONFIG_HAS_WAKELOCK)
	if(wake_lock_active(&vib_wl))
	wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
	int timeout)
{
	 int extend_time = 0;

	printk(KERN_NOTICE "Vibrator: Set duration: %dms + %dms\n", timeout, extend_time);
	if( timeout > 0 )
	{
#if defined(CONFIG_HAS_WAKELOCK)
		if(!wake_lock_active(&vib_wl))
	wake_lock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
		set_vibrator(VIB_ON);
		if(timeout < MIN_TIME_MS)
			extend_time = MIN_TIME_MS - timeout;
		else
			extend_time = 0;
	if(cancel_delayed_work_sync(&vibrator_off_work))
		printk(KERN_NOTICE "Vibrator: Cancel off work to re-start\n");
	
	queue_delayed_work(vib_workqueue, &vibrator_off_work, msecs_to_jiffies(timeout + extend_time));
}
	else if(!timeout)
	{
		extend_time = 0;
		set_vibrator(VIB_OFF);
#if defined(CONFIG_HAS_WAKELOCK)
		if(wake_lock_active(&vib_wl))
			wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
	}

}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	int retTime = jiffies_to_msecs(jiffies - vibrator_off_work.timer.expires);
	printk(KERN_NOTICE "Vibrator: Current duration: %dms\n", retTime);
	return retTime;
}

static struct timed_output_dev sprd_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_remaining_time,
	.enable = vibrator_enable_set_timeout,
};

static int __init sprd_init_vibrator(void)
{
	int ret = 0;
	
	vibrator_hw_init();

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_init(&vib_wl, WAKE_LOCK_SUSPEND, __stringify(vib_wl));
#endif

	ret = timed_output_dev_register(&sprd_vibrator);
	if (ret < 0) {
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		goto error;
	}

	vib_workqueue = create_workqueue("vib_wq");
	INIT_DELAYED_WORK(&vibrator_off_work, vibrator_off_worker);

	return 0;

error:
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&vib_wl);
#endif
	return ret;
}

static void __exit sprd_exit_vibrator(void)
{
	timed_output_dev_unregister(&sprd_vibrator);
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&vib_wl);
#endif
	destroy_workqueue(vib_workqueue);
}

module_init(sprd_init_vibrator);
module_exit(sprd_exit_vibrator);

MODULE_DESCRIPTION("vibrator driver for spreadtrum Processors");
MODULE_LICENSE("GPL");
