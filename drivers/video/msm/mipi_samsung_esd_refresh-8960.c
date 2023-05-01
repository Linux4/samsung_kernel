/*
 *	Samsung Mipi ESD refresh driver.
 *
 *	Author: Krishna Kishor Jha <krishna.jha@samsung.com>
 *	Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "mipi_samsung_esd_refresh-8960.h"
#include "linux/msm_mdp.h"

static struct mipi_controls mipi_control;
static struct esd_data_t *esd_enable;
#if defined(CONFIG_MACH_M2_SPR)
	static int qc_method = 1;
#else
	static int qc_method = 0;
#endif

static irqreturn_t sec_esd_irq_handler(int irq, void *handle);

#ifdef READ_REGISTER_ESD
static struct completion esd_completion;
#endif

#if  LP11_RECOVERY
static void lcd_LP11_signal(void)
{
	uint32 dsi_lane_ctrl, dsi_video_mode_ctrl;

	dsi_lane_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00A8);
	dsi_video_mode_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00C);
	MIPI_OUTP(MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl | 0x11110000);
	MIPI_OUTP(MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl & 0x0FFFFFFF);
	wmb();
	msleep(20);
	MIPI_OUTP(MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl);
	MIPI_OUTP(MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl);
	wmb();
}

static void lcd_esd_seq(struct esd_data_t *p_esd_data)
{
	lcd_LP11_signal();
	msleep(500);
	lcd_LP11_signal();
	msleep(2000);
	lcd_LP11_signal();
}
#else

static void lcd_esd_seq(struct esd_data_t *p_esd_data)
{

	struct msm_fb_data_type *mfd;
	struct msm_fb_panel_data *pdata;
	mfd = platform_get_drvdata(mipi_control.mipi_dev);
	if (mfd->panel_power_on) {
#ifndef ESD_DEBUG
		/* threaded irq can sleep */
		wake_lock_timeout(&p_esd_data->det_wake_lock, WAKE_LOCK_TIME);
#endif
		p_esd_data->refresh_ongoing = true;
		set_esd_refresh(true);
		p_esd_data->esd_ignore = true;

		mfd->fbi->fbops->fb_blank(FB_BLANK_VSYNC_SUSPEND, mfd->fbi);

		pr_info("Mipi ESD Turn off comple..........\n");
		msleep(100);
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);

		pr_info("Mipi ESD Turn On complete...........\n");

		p_esd_data->refresh_ongoing = false;
		set_esd_refresh(false);
		p_esd_data->esd_processed_count++;

		/* Restore brightness */
		pdata = mipi_control.mipi_dev->dev.platform_data;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) || \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
		reset_gamma_level();
#endif
		pdata->set_backlight(mfd);

	} else {
		 pr_err("Panel is Off Skip ESD Sequence\n");
	}

#ifdef READ_REGISTER_ESD
	complete(&esd_completion);
#endif

}
#endif

static void sec_esd_work_func(struct work_struct *work)
{
	struct esd_data_t *p_esd_data =
		container_of(work, struct esd_data_t, det_work);
	p_esd_data->esd_count++;
	if (p_esd_data->esd_count <= ESD_EXCEPT_CNT)
		pr_info("%s : %d ignore Cnt(%d)\n", __func__,
			p_esd_data->esd_count, ESD_EXCEPT_CNT);
	else if (p_esd_data->esd_ignore)
		pr_info("%s : %d ignore FLAG,esd_processed:%d\n",
			__func__, p_esd_data->esd_count,
			p_esd_data->esd_processed_count);
	else {
		lcd_esd_seq(p_esd_data);
	}
	p_esd_data->esd_irq_enable = true;
	return;
}

#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct esd_data_t *p_esd_data = dev_get_drvdata(dev);
	char temp[20];

	snprintf(temp, 20, "ESD Status:%d\n", p_esd_data->refresh_ongoing);
	strncat(buf, temp, 20);
	return strnlen(buf, 20);
}

static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct esd_data_t *p_esd_data = dev_get_drvdata(dev);

	sec_esd_irq_handler(0, p_esd_data);
	return 1;
}

static DEVICE_ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store);
#endif

static irqreturn_t sec_esd_irq_handler(int irq, void *handle)
{
	struct esd_data_t *p_esd_data = (struct esd_data_t *) handle;
	struct msm_fb_data_type *mfd;
	char *envp[2] = {"PANEL_ALIVE=0", NULL};

	if (!mipi_control.mipi_dev)
		return IRQ_HANDLED;
	else
		mfd = platform_get_drvdata(mipi_control.mipi_dev);
	if(!qc_method) {

	if (!mfd->panel_power_on || p_esd_data->refresh_ongoing
		|| p_esd_data->esd_irq_enable == false) {
		/* Panel is not powered ON So bogus ESD/
		ESD Already executing*/
		return IRQ_HANDLED;
	}
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	/* ESD occurred during Wakeup/Suspend, So ignore */
	if (mfd->resume_state)
		return IRQ_HANDLED;
#endif
	p_esd_data->esd_irq_enable = false;
	schedule_work(&p_esd_data->det_work);
	}else if(!esd_enable->esd_ignore && mfd->panel_power_on ){
		 kobject_uevent_env(&mfd->fbi->dev->kobj, KOBJ_CHANGE, envp);
		 pr_err("%s: Panel has gone bad, sending uevent - %s\n", __func__, envp[0]);
		esd_enable->refresh_ongoing = true;
		esd_enable->esd_ignore = true;
	}
	return IRQ_HANDLED;
}

#ifdef READ_REGISTER_ESD
void esd_execute(void)
{
	if (esd_enable->esd_irq_enable) {

		if (work_busy(&esd_enable->det_work))
			pr_info("%s ESD work queue is working", __func__);
		else {
			pr_info("%s start", __func__);

			INIT_COMPLETION(esd_completion);

			schedule_work(&esd_enable->det_work);

			wait_for_completion_timeout(&esd_completion, 10 * HZ);

			pr_info("%s end", __func__);
		}
	} else
		pr_info("%s ESD is armed from ISR", __func__);
}
#endif

void register_mipi_dev(struct platform_device *mipi_dev)
{
	mipi_control.mipi_dev = mipi_dev;
}
static void set_esd_enable_work_func(struct work_struct *work)
{
	struct msm_fb_panel_data *pdata;
	struct msm_fb_data_type *mfd;

	pr_info("%s is called %d\n", __func__, esd_enable->esd_ignore);
	mfd = platform_get_drvdata(mipi_control.mipi_dev);
	if(qc_method && esd_enable->refresh_ongoing) {
		// ESD has happend restore backlight

		/* Restore brightness */
				pdata = mipi_control.mipi_dev->dev.platform_data;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) || \
			defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
				reset_gamma_level();
#endif
				pdata->set_backlight(mfd);
		esd_enable->refresh_ongoing = false;
	}

	esd_enable->esd_ignore = false;
	pr_info("%s is called %d ---\n", __func__, esd_enable->esd_ignore);

}

void set_esd_enable(void)
{
	pr_info("%s is called\n", __func__);
	if (!esd_enable) {
		pr_err("ESD Driver data is NULL!!\n");
		return;
	}

	schedule_delayed_work(&esd_enable->esd_enable_delay,\
	msecs_to_jiffies(500));
}
void set_esd_disable(void)
{
	pr_info("%s is called\n", __func__);
	if (esd_enable->refresh_ongoing) {
		pr_err("ESD refresh isr is on going!!\n");
		return;
	}
	if (!esd_enable) {
		pr_err("ESD Driver data is NULL!!\n");
		return;
	}
	cancel_delayed_work(&esd_enable->esd_enable_delay);
	esd_enable->esd_ignore = true;

	if (!list_empty(&(esd_enable->det_work.entry))) {
		cancel_work_sync(&(esd_enable->det_work));
		pr_info("%s cancel_work_sync\n", __func__);
	}
}

static void mipi_samsung_esd_early_suspend(struct early_suspend *h)
{
	pr_info("Early Suspend:ESD IRQ is disabled\n");
	disable_irq(esd_enable->pdata->esd_gpio_irq);
}

static void mipi_samsung_esd_late_resume(struct early_suspend *h)
{
	pr_info("Late Resume:ESD IRQ is enabled\n");
	enable_irq(esd_enable->pdata->esd_gpio_irq);

}

static int __devinit mipi_esd_refresh_probe(struct platform_device *pdev)
{
	struct esd_data_t *p_esd_data;
	struct sec_esd_platform_data *pdata = pdev->dev.platform_data;
	unsigned int irq_type;
#ifdef ESD_DEBUG
	struct device  *esd_device;
#endif
	int ret = 0;
	if (pdata == NULL) {
		pr_err("ESD Platform data is Null !!!!!\n");
		return -1;
	}
#ifndef ESD_DEBUG
	if (pdata->esd_gpio_irq == -1) {
		/* Do nothing ESD not supported in this revision */
		return 0;
	}
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	mipi_control.early_suspend.suspend = mipi_samsung_esd_early_suspend;
	mipi_control.early_suspend.resume = mipi_samsung_esd_late_resume;
	mipi_control.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 5;
	register_early_suspend(&mipi_control.early_suspend);
#endif

#if defined(CONFIG_MACH_GOGH) || defined(CONFIG_MACH_INFINITE)
	if (system_rev > 0x01)
		irq_type = IRQF_TRIGGER_FALLING;
	else     {
		/* Gogh 0.1rev  has ESD protection chip irq
			trigger is low.....HIGH...low */
		irq_type = IRQF_TRIGGER_RISING;
	}
#else
	/* ESD irq through VGH, irq trigger is HIGH....low....HIGH */
	irq_type = IRQF_TRIGGER_FALLING;
#endif


#ifdef READ_REGISTER_ESD
		init_completion(&esd_completion);
#endif

	p_esd_data = kzalloc(sizeof(struct esd_data_t), GFP_KERNEL);
	if (p_esd_data == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}
	esd_enable = p_esd_data;
	p_esd_data->pdata = pdata;
	p_esd_data->esd_count = 0;
	p_esd_data->esd_ignore = false;
	p_esd_data->esd_irq_enable = true;
	p_esd_data->esd_processed_count = 0;

	wake_lock_init(&p_esd_data->det_wake_lock,
		 WAKE_LOCK_SUSPEND, "esd_det");

	INIT_WORK(&p_esd_data->det_work, sec_esd_work_func);
	INIT_DELAYED_WORK(&p_esd_data->esd_enable_delay,\
					set_esd_enable_work_func);
	dev_set_drvdata(&pdev->dev, p_esd_data);
#ifdef ESD_DEBUG
	esd_device = device_create(sec_class, NULL, 0, p_esd_data, "sec_esd");
	if (IS_ERR(esd_device)) {
		pr_err("Failed to create device for the factory test\n");
		ret = -ENODEV;
	}

	ret = sysfs_create_file(&esd_device->kobj,
					&dev_attr_esd_check.attr);
	if (ret) {
		pr_err("sysfs create fail-%s\n",
				dev_attr_esd_check.attr.name);
	}
#endif
	ret = request_threaded_irq(pdata->esd_gpio_irq, NULL,
			sec_esd_irq_handler,
			 irq_type |
			IRQF_ONESHOT, "esd_detect", p_esd_data);
	if (ret) {
		pr_err("%s : Failed to request_irq.:ret=%d", __func__, ret);
		goto err_request_detect_irq;
	}
#if defined(CONFIG_SAMSUNG_CMC624)
	if (samsung_has_cmc624()) {
		ret = request_threaded_irq(pdata->esd_gpio_cmc_irq, NULL,
			sec_esd_irq_handler,
			IRQF_TRIGGER_RISING |
			IRQF_ONESHOT, "esd_detect2", p_esd_data);
		if (ret) {
			pr_err("%s:Fail to request_irq.:ret=%d", __func__, ret);
			goto err_request_detect_irq2;
		}
	}
#endif
	set_esd_disable();
	return 0;

#if defined(CONFIG_SAMSUNG_CMC624)
err_request_detect_irq2:
	free_irq(pdata->esd_gpio_irq, p_esd_data);
#endif

err_request_detect_irq:
	wake_lock_destroy(&p_esd_data->det_wake_lock);
	kfree(p_esd_data);
	esd_enable = NULL;
	dev_set_drvdata(&pdev->dev, NULL);

	return -1;
}

static int sec_esd_remove(struct platform_device *pdev)
{

	struct esd_data_t *p_esd_data = dev_get_drvdata(&pdev->dev);
	free_irq(p_esd_data->pdata->esd_gpio_irq, p_esd_data);
	disable_irq_wake(p_esd_data->pdata->esd_gpio_irq);
#if defined(CONFIG_SAMSUNG_CMC624)
	free_irq(p_esd_data->pdata->esd_gpio_cmc_irq, p_esd_data);
	disable_irq_wake(p_esd_data->pdata->esd_gpio_cmc_irq);
#endif
	wake_lock_destroy(&p_esd_data->det_wake_lock);
	kfree(p_esd_data);
	return 0;
}

static struct platform_driver samsung_esd_refresh_driver = {
	.probe  = mipi_esd_refresh_probe,
	.remove = sec_esd_remove,
	.driver = {
		.name   = "samsung_mipi_esd_refresh",
	},
};

static void __exit samsung_mipi_esd_refresh_exit(void)
{
	platform_driver_unregister(&samsung_esd_refresh_driver);
}

static int __init samsung_mipi_esd_refresh_init(void)
{
	return platform_driver_register(&samsung_esd_refresh_driver);
}

MODULE_DESCRIPTION("Samsung ESD refresh driver");
MODULE_AUTHOR("Krishna Kishor Jha <krishna.jha@samsung.com>");
MODULE_LICENSE("GPL");
module_init(samsung_mipi_esd_refresh_init);
module_exit(samsung_mipi_esd_refresh_exit);
