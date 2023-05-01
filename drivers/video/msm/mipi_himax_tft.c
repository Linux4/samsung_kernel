/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/lcd.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <mach/msm8930-gpio.h>
#include "mipi_himax_tft.h"
#include "mdp4.h"

#if defined(CONFIG_FB_MDP4_ENHANCE)
#include "mdp4_video_enhance.h"
#elif defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif

#define DDI_VIDEO_ENHANCE_TUNING /*for HX8369B*/
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#endif

#define USE_READ_ID
static struct mipi_samsung_driver_data msd;
static unsigned int recovery_boot_mode;

#define WA_FOR_FACTORY_MODE
#define READ_MTP_ONCE

struct dsi_buf mdnie_tune_tx_buf;

int is_lcd_connected = 1;
struct mutex dsi_tx_mutex;


#if defined(CONFIG_BACKLIGHT_IC_KTD253)
spinlock_t bl_ctrl_lock;
static int lcd_brightness = -1;
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct work_struct  err_fg_work;
static int err_fg_gpio;	/* GPIO 19 */
static int esd_count;
static int err_fg_working;
#endif

#ifdef __HIMAX8394A__
static char WRDISBV[] = {
	0x51,
	0xC3
};
static struct dsi_cmd_desc himax_video_backlight_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1,
		sizeof(WRDISBV), WRDISBV},
};
#endif

#ifdef USE_READ_ID
static char manufacture_id1[2] = {0xDA, 0x00}; /* DTYPE_DCS_READ */
static char manufacture_id2[2] = {0xDB, 0x00}; /* DTYPE_DCS_READ */
static char manufacture_id3[2] = {0xDC, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc samsung_manufacture_id1_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id1), manufacture_id1};
static struct dsi_cmd_desc samsung_manufacture_id2_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id2), manufacture_id2};
static struct dsi_cmd_desc samsung_manufacture_id3_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id3), manufacture_id3};

#if 1
static uint32 mipi_samsung_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint32 id;

	mutex_lock(&dsi_tx_mutex);

	tp = &msd.samsung_tx_buf;
	rp = &msd.samsung_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &samsung_manufacture_id1_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	pr_info("%s: manufacture_id1=%x\n", __func__, *rp->data);
	id = *((uint8 *)rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	cmd = &samsung_manufacture_id2_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	pr_info("%s: manufacture_id2=%x\n", __func__, *rp->data);
	id |= *((uint8 *)rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	cmd = &samsung_manufacture_id3_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	pr_info("%s: manufacture_id3=%x\n", __func__, *rp->data);
	id |= *((uint8 *)rp->data);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

#ifdef FACTORY_TEST
	if (id == 0x00) {
		pr_info("Lcd is not connected\n");
		is_lcd_connected = 0;
	}
#endif
	mutex_unlock(&dsi_tx_mutex);

	return id;
}

#else
static uint32 mipi_samsung_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint32 id, id1;

	tp = &msd.samsung_tx_buf;
	rp = &msd.samsung_rx_buf;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	/*read Id1 two time since fist time id read is always returning 0*/
	cmd = &samsung_manufacture_id1_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	id1 = *((uint8 *)rp->data);

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	cmd = &samsung_manufacture_id1_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	id = *((uint8 *)rp->data) | id1;
	pr_info("%s: manufacture_id1=%x\n", __func__, *rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	cmd = &samsung_manufacture_id2_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	pr_info("%s: manufacture_id2=%x\n", __func__, *rp->data);
	id |= *((uint8 *)rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);
	cmd = &samsung_manufacture_id3_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	pr_info("%s: manufacture_id3=%x\n", __func__, *rp->data);
	id |= *((uint8 *)rp->data);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

#ifdef FACTORY_TEST
	if (id == 0x00) {
		pr_info("Lcd is not connected\n");
		is_lcd_connected = 0;
	}
#endif
	return id;
}
#endif
#endif

static char SETCHESEL[] = {
	0xE6,
	0x01,
};

static char SETCHEMODE_DYN[] = {
	0xE4,
	0x22,
};
static char SETCHEMODE_DYN_UI[] = {
	0xE4,
	0x02,
};

static struct dsi_cmd_desc samsung_display_mode_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(SETCHESEL), SETCHESEL},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(SETCHEMODE_DYN), SETCHEMODE_DYN},

};

static struct dsi_cmd_desc samsung_display_mode_cmds_ui[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(SETCHESEL), SETCHESEL},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(SETCHEMODE_DYN_UI), SETCHEMODE_DYN_UI},

};

void mipi_samsung_tune_mode(int mode)
{
	struct msm_fb_data_type *mfd;
	struct dcs_cmd_req cmdreq;

	pr_info("send tuning cmd mode(%d)!!\n", mode);

	mfd = (struct msm_fb_data_type *) registered_fb[0]->par;

	mutex_lock(&dsi_tx_mutex);

	if (mode){

		cmdreq.cmds = samsung_display_mode_cmds_ui;
		cmdreq.cmds_cnt = ARRAY_SIZE(samsung_display_mode_cmds_ui);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		mipi_dsi_cmdlist_put(&cmdreq);
	}
	else{

		cmdreq.cmds = samsung_display_mode_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(samsung_display_mode_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		mipi_dsi_cmdlist_put(&cmdreq);

	}
	mutex_unlock(&dsi_tx_mutex);

}

static int mipi_samsung_disp_send_cmd(struct msm_fb_data_type *mfd,
				       enum mipi_samsung_cmd_list cmd,
				       unsigned char lock)
{
	struct dsi_cmd_desc *cmd_desc;
	struct dcs_cmd_req cmdreq;
	int cmd_size = 0;

/*	wake_lock(&idle_wake_lock);*//*temp*/

	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_lock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_lock(&mfd->dma->ov_mutex);
	}

	pr_info("%s cmd = 0x%x\n", __func__, cmd);

	switch (cmd) {
	case PANEL_READY_TO_ON: /*work*/
		cmd_desc = msd.mpd->ready_to_on.cmd;
		cmd_size = msd.mpd->ready_to_on.size;
		break;
	case PANEL_READY_TO_OFF:
		cmd_desc = msd.mpd->ready_to_off.cmd;
		cmd_size = msd.mpd->ready_to_off.size;
		break;
	case PANEL_ON:
		cmd_desc = msd.mpd->on.cmd;
		cmd_size = msd.mpd->on.size;
		break;
	case PANEL_OFF:
		cmd_desc = msd.mpd->off.cmd;
		cmd_size = msd.mpd->off.size;
		break;
	case PANEL_LATE_ON:
		cmd_desc = msd.mpd->late_on.cmd;
		cmd_size = msd.mpd->late_on.size;
		break;
	case PANEL_EARLY_OFF:
		cmd_desc = msd.mpd->early_off.cmd;
		cmd_size = msd.mpd->early_off.size;
		break;
	case MTP_READ_ENABLE:
		cmd_desc = msd.mpd->mtp_read_enable.cmd;
		cmd_size = msd.mpd->mtp_read_enable.size;
		break;
/*
	case PANEL_TUNE_CTRL:
		cmd_desc = msd.mpd->tune.cmd;
		cmd_size = msd.mpd->tune.size;
		break;
*/
	default:
		goto unknown_command;
		;
	}

	if (!cmd_size)
		goto unknown_command;

		cmdreq.cmds = cmd_desc;
		cmdreq.cmds_cnt = cmd_size;
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);

	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_unlock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_unlock(&mfd->dma->ov_mutex);
	}

	return 0;

unknown_command:
	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_unlock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_unlock(&mfd->dma->ov_mutex);
	}

	return 0;
}

static unsigned char first_on;

extern void dumpreg(int );
static int mipi_samsung_disp_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	static int boot_on;

#ifdef __DEBUG__
	static int test=0 ;
#endif

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi = &mfd->panel_info.mipi;

#ifdef USE_READ_ID
	if (unlikely(!boot_on)) {
		msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
		/*Display was initialized in bootloader,same settings
			are carried when splash is enabled*/
		}
#endif

#if defined(CONFIG_FB_MDP4_ENHANCE)
	is_negativeMode_on();
#endif

#ifdef __DEBUG__
	if(test==0)
        if(0)
	dumpreg(1);
		test++;
#endif		

	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_ON, false);

	if(!boot_on)
		boot_on = 1;
	else
		msleep(100);

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	enable_irq(err_fg_gpio);
#endif

#ifndef CONFIG_MACH_CANE_EUR_3G
	pr_info("%s: DISP_BL_CONT_GPIO High\n", __func__);
	gpio_set_value(DISP_BL_CONT_GPIO, 1);
#endif	

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);
#endif

#if defined(CONFIG_MACH_CANE_EUR_3G)
	mfd->resume_state = MIPI_RESUME_STATE;
#endif

	return 0;
}

static int mipi_samsung_disp_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!err_fg_working) {
		disable_irq_nosync(err_fg_gpio);
		cancel_work_sync(&err_fg_work);
	}
#endif

	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;
	pr_debug("%s: DISP_BL_CONT_GPIO low-block\n", __func__);

#if !defined(CONFIG_ESD_ERR_FG_RECOVERY)
	gpio_set_value(DISP_BL_CONT_GPIO, 0);
#endif

	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_OFF, false);
	mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);

	mfd->resume_state = MIPI_SUSPEND_STATE;

	return 0;
}

static void __devinit mipi_samsung_disp_shutdown(struct platform_device *pdev)
{
	static struct mipi_dsi_platform_data *mipi_dsi_pdata;

	if (pdev->id != 0)
		return;

	 mipi_dsi_pdata = pdev->dev.platform_data;
	if (mipi_dsi_pdata == NULL) {
		pr_err("LCD Power off failure: No Platform Data\n");
		return;
	}
}

static void mipi_samsung_disp_backlight(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;
#ifndef CONFIG_BACKLIGHT_IC_KTD253
	struct dcs_cmd_req cmdreq;
#endif	
	static int bl_level_old;

	pr_info("%s Back light level:%d\n", __func__, mfd->bl_level);

	mipi  = &mfd->panel_info.mipi;
	if (bl_level_old == mfd->bl_level)
		goto end;
	if (!msd.mpd->set_brightness_level ||  !mfd->panel_power_on)
		goto end;

#if defined(CONFIG_BACKLIGHT_IC_KTD253)

	ktd253_set_brightness(msd.mpd->set_brightness_level(mfd->bl_level));
	bl_level_old = mfd->bl_level;

#else /*for himax ldi HX8394-A*/
	mutex_lock(&dsi_tx_mutex);
	/* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
	mipi_dsi_mdp_busy_wait();

	WRDISBV[1] = (unsigned char)
		(msd.mpd->set_brightness_level(mfd->bl_level));

	cmdreq.cmds = himax_video_backlight_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(himax_video_backlight_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	bl_level_old = mfd->bl_level;
	mutex_unlock(&dsi_tx_mutex);
#endif

end:
	return;
	
}

#if defined(CONFIG_BACKLIGHT_IC_KTD253)

void ktd253_set_brightness(int level)
{
	int pulse;
	int tune_level = 0;

	spin_lock(&bl_ctrl_lock);	
	tune_level = level;

	pr_debug("[KTD253] tune_level : %d, lcd_brightness : %d \n",tune_level,lcd_brightness);	
	if(tune_level != lcd_brightness)
	{
		if (!tune_level) {
			gpio_set_value(DISP_BL_CONT_GPIO, 0);
			mdelay(3);
			lcd_brightness = tune_level;
		} else {
			if (unlikely(lcd_brightness < 0)) {
				int val = gpio_get_value(DISP_BL_CONT_GPIO);
				if (val) {
					lcd_brightness = 0;
				gpio_set_value(DISP_BL_CONT_GPIO, 0);
				mdelay(3);
					pr_info("LCD Baklight init in boot time on kernel\n");
				}
			}
			if (!lcd_brightness) {
				gpio_set_value(DISP_BL_CONT_GPIO, 1);
				udelay(3);
				lcd_brightness = 1;
			}

			pulse = (tune_level - lcd_brightness + MAX_BRIGHTNESS_IN_BLU)
							% MAX_BRIGHTNESS_IN_BLU;

			for (; pulse > 0; pulse--) {
				gpio_set_value(DISP_BL_CONT_GPIO, 0);
				udelay(3);
				gpio_set_value(DISP_BL_CONT_GPIO, 1);
				udelay(3);
			}

			lcd_brightness = tune_level;
		}
	}
	mdelay(1);
	spin_unlock(&bl_ctrl_lock);
}
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi_samsung_disp_early_suspend(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;
	pr_info("%s", __func__);

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

}

static void mipi_samsung_disp_late_resume(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

	mfd->resume_state = MIPI_RESUME_STATE;
	pr_info("%s", __func__);
}
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
#ifdef WA_FOR_FACTORY_MODE
static ssize_t mipi_samsung_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct msm_fb_data_type *mfd;
	int rc;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->panel_power_on);
	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	unsigned int power;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, true);
		mipi_samsung_disp_backlight(mfd);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");

	return size;
}
#else
static int mipi_samsung_disp_get_power(struct lcd_device *dev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return mfd->panel_power_on;
}

static int mipi_samsung_disp_set_power(struct lcd_device *dev, int power)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, true);
		mipi_samsung_disp_backlight(mfd);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");
	return 0;
}
#endif

static ssize_t mipi_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, strnlen(msd.mpd->panel_name, 20) + 1,
						msd.mpd->panel_name);
	strlcat(buf, temp, 20);
	return strnlen(buf, 20);
}

static ssize_t mipi_samsung_disp_gamma_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.gamma_mode);
	pr_info("gamma_mode: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_gamma_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (sysfs_streq(buf, "1") && !msd.dstat.gamma_mode) {
		/* 1.9 gamma */
		msd.dstat.gamma_mode = GAMMA_1_9;
	} else if (sysfs_streq(buf, "0") && msd.dstat.gamma_mode) {
		/* 2.2 gamma */
		msd.dstat.gamma_mode = GAMMA_2_2;
	} else {
		pr_info("%s: Invalid argument!!", __func__);
	}

	return size;
}

static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (!mfd->panel_power_on) {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on)
			msd.dstat.acl_on = true;
		else if (sysfs_streq(buf, "0") && msd.dstat.acl_on)
			msd.dstat.acl_on = false;
		else
			pr_info("%s: Invalid argument!!", __func__);
	} else {

#ifdef CONFIG_ACL_FUNCTION /*temp*/
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on) {
			if (msd.mpd->set_acl(mfd->bl_level))
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_OFF, true);
			else {
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_ON, true);
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_UPDATE, true);
			}
			msd.dstat.acl_on = true;
		} else if (sysfs_streq(buf, "0") && msd.dstat.acl_on) {
			mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);
			msd.dstat.acl_on = false;
		} else {
			pr_info("%s: Invalid argument!!", __func__);
		}
#endif
	}

	return size;
}

static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("auot_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else
		pr_info("%s: Invalid argument!!", __func__);

	return size;
}


static struct lcd_ops mipi_samsung_disp_props = {
#ifdef WA_FOR_FACTORY_MODE
	.get_power = NULL,
	.set_power = NULL,
#else
	.get_power = mipi_samsung_disp_get_power,
	.set_power = mipi_samsung_disp_set_power,
#endif
};

#ifdef WA_FOR_FACTORY_MODE
static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);
#endif
static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_samsung_disp_lcdtype_show, NULL);
static DEVICE_ATTR(gamma_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_gamma_mode_show,
			mipi_samsung_disp_gamma_mode_store);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
static irqreturn_t err_fg_irq_handler(int irq, void *handle)
{
	pr_info("%s : handler start", __func__);
	disable_irq_nosync(err_fg_gpio);
	schedule_work(&err_fg_work);
	pr_info("%s : handler end", __func__);

	return IRQ_HANDLED;
}
static void err_fg_work_func(struct work_struct *work)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		pr_info("[%s] mipi suspend state!! return!\n",__func__);
		return;
	}

	pr_info("%s : start", __func__);
	err_fg_working = 1;
	esd_recovery();
	esd_count++;
	err_fg_working = 0;

	pr_info("%s : end", __func__);
	return;
}
#endif


#ifdef DDI_VIDEO_ENHANCE_TUNING
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
#define TUNE_SIZE 113
static char tuning_file[MAX_FILE_NAME];
static char mdni_tuning[TUNE_SIZE];

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning), mdni_tuning},
};

static char char_to_dec(char data1, char data2)
{
	char dec;

	dec = 0;

	if (data1 >= 'a') {
		data1 -= 'a';
		data1 += 10;
	} else if (data1 >= 'A') {
		data1 -= 'A';
		data1 += 10;
	} else
		data1 -= '0';

	dec = data1 << 4;

	if (data2 >= 'a') {
		data2 -= 'a';
		data2 += 10;
	} else if (data2 >= 'A') {
		data2 -= 'A';
		data2 += 10;
	} else
		data2 -= '0';

	dec |= data2;

	return dec;
}
static void sending_tune_cmd(char *src, int len)
{
	struct msm_fb_data_type *mfd;
	struct dcs_cmd_req cmdreq;

	int data_pos;
	int cmd_pos;

	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
					mdni_tuning[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));

				data_pos += 3;
				cmd_pos++;
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_SIZE ; data_pos++)
		printk(KERN_INFO"0x%x ", mdni_tuning[data_pos]);
	printk(KERN_INFO "\n");

	mfd = platform_get_drvdata(msd.msm_pdev);

	mutex_lock(&dsi_tx_mutex);

	cmdreq.cmds = mdni_tune_cmd;
	cmdreq.cmds_cnt = ARRAY_SIZE(mdni_tune_cmd);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	mutex_unlock(&dsi_tx_mutex);
}

static void load_tuning_file(char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs;

	pr_info("%s called loading file name : [%s]\n", __func__,
	       filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		return;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return;
	}
	pos = 0;
	memset(dp, 0, l);

	pr_info("%s before vfs_read()\n", __func__);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	pr_info("%s after vfs_read()\n", __func__);

	if (ret != l) {
		pr_info("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dp, l);

	kfree(dp);
}


static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n",
								tuning_file);

	return ret;
}

static ssize_t tuning_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
	char *pt;
	memset(tuning_file, 0, sizeof(tuning_file));
	snprintf(tuning_file, MAX_FILE_NAME, "%s%s", TUNING_FILE_PATH, buf);

	pt = tuning_file;
	while (*pt) {
		if (*pt == '\r' || *pt == '\n') {
			*pt = 0;
			break;
		}
		pt++;
	}

	pr_info("%s:%s\n", __func__, tuning_file);

	load_tuning_file(tuning_file);

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif


static int __devinit mipi_samsung_disp_probe(struct platform_device *pdev)
{

	struct platform_device *msm_fb_added_dev;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
	int ret;
#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd;
#endif
	msd.dstat.acl_on = false;

	if (pdev->id == 0) {
		msd.mipi_samsung_disp_pdata = pdev->dev.platform_data;

		first_on = false;
		return 0;
	}

	msm_fb_added_dev = msm_fb_add_device(pdev);

	mutex_init(&dsi_tx_mutex);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_LCD_CLASS_DEVICE)
	msd.msm_pdev = msm_fb_added_dev;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi_samsung_disp_early_suspend;
	msd.early_suspend.resume = mipi_samsung_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&msd.early_suspend);

#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	INIT_WORK(&err_fg_work, err_fg_work_func);

	err_fg_gpio = MSM_GPIO_TO_INT(GPIO_ESD_VGH_DET);

	ret = gpio_request(GPIO_ESD_VGH_DET, "err_fg");
	if (ret) {
		pr_err("request gpio GPIO_LCD_ESD_DET failed, ret=%d\n",ret);
		gpio_free(GPIO_ESD_VGH_DET);
		return ret;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_ESD_VGH_DET,  0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);

	ret = request_threaded_irq(err_fg_gpio, NULL, err_fg_irq_handler, 
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "esd_detect", NULL);
	if (ret) {
		pr_err("%s : Failed to request_irq. :ret=%d", __func__, ret);
	}

	disable_irq(err_fg_gpio);
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		ret = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return ret;
	}

#ifdef WA_FOR_FACTORY_MODE
	sysfs_remove_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_power.attr.name);
	}
#endif

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_gamma_mode.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_gamma_mode.attr.name);
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_power_reduce.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_power_reduce.attr.name);
	}
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return ret;
	}

	ret = sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}
#endif
#endif
#if defined(CONFIG_MDNIE_LITE_TUNING) \
	|| defined(CONFIG_FB_MDP4_ENHANCE)
	/*	mdnie sysfs create */
	init_mdnie_class();
#endif

	mipi_dsi_buf_alloc(&mdnie_tune_tx_buf, DSI_BUF_SIZE);

#if defined(DDI_VIDEO_ENHANCE_TUNING)
	ret = sysfs_create_file(&lcd_device->dev.kobj,
				&dev_attr_tuning.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_tuning.attr.name);
	}
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_samsung_disp_probe,
	.driver = {
		.name   = "mipi_samsung_oled",
	},
	.shutdown = mipi_samsung_disp_shutdown
};

static struct msm_fb_panel_data samsung_panel_data = {
	.on		= mipi_samsung_disp_on,
	.off		= mipi_samsung_disp_off,
	.set_backlight	= mipi_samsung_disp_backlight,
};

static int ch_used[3];

int mipi_samsung_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel,
					struct mipi_panel_data *mpd)
{
	struct platform_device *pdev = NULL;
	int ret = 0;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_samsung_oled",
					   (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	samsung_panel_data.panel_info = *pinfo;
	msd.mpd = mpd;
	if (!msd.mpd) {
		printk(KERN_ERR
		  "%s: get mipi_panel_data failed!\n", __func__);
		goto err_device_put;
	}
	mpd->msd = &msd;
	ret = platform_device_add_data(pdev, &samsung_panel_data,
		sizeof(samsung_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return ret;

err_device_put:
	platform_device_put(pdev);
	return ret;
}


static int __init current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		recovery_boot_mode = 1;
	else
		recovery_boot_mode = 0;

	pr_debug("%s %s", __func__, recovery_boot_mode == 1 ?
						"recovery" : "normal");
	return 1;
}
__setup("androidboot.boot_recovery=", current_boot_mode);

static int __init mipi_samsung_disp_init(void)
{
	mipi_dsi_buf_alloc(&msd.samsung_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&msd.samsung_rx_buf, DSI_BUF_SIZE);

/*	wake_lock_init(&idle_wake_lock,
WAKE_LOCK_IDLE, "MIPI idle lock");*/ /*temp*/

	return platform_driver_register(&this_driver);
}

module_init(mipi_samsung_disp_init);
