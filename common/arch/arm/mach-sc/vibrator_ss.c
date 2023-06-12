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
#define VIBRATOR_STABLE_LEVEL   (6)
#define VIBRATOR_INIT_LEVEL     (11)
#define VIBRATOR_INIT_STATE_CNT (10)

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

#define VIBR_PULLDOWN_EN        (1 << 1)
#define LDO_VIBR_PD             (1 << 8)

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
#ifdef CONFIG_MACH_CORSICA_VE
	if(get_hw_rev() > 0x01)
#else
	if(1)
#endif
	{
		if (on)
		{
			 printk("###############vibrator on##################\n");
       		 sci_adi_write(ANA_REG_GLB_BA_CTRL3, 0x1,0xffff);
       		 sci_adi_write(ANA_REG_GLB_BA_CTRL2, 0x3ad2,0xffff);
       		 sci_adi_write(ANA_REG_GLB_BA_CTRL3,0x2,0xffff);
		}
		else
		{
			printk("###############vibrator off##################\n");
			sci_adi_write(ANA_REG_GLB_BA_CTRL2, 0x3bd2,0xffff);
		}
	}
	else
	{
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
	}
}

static void vibrator_hw_init(void)
{
#ifdef CONFIG_MACH_CORSICA_VE
	if(get_hw_rev() > 0x01)
#else
	if(1)
#endif
		return;
	else
	{
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
	}
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
