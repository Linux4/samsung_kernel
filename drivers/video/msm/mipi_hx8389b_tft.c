 /* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG

#include <mach/socinfo.h>
#include <linux/lcd.h>
#include "msm_fb.h"
#include <linux/gpio.h>
#include "mipi_dsi.h"
#include "mipi_hx8389b_tft.h"
#if defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif

static struct dsi_buf hx8389b_tx_buf;
static struct dsi_buf hx8389b_rx_buf;

#if defined(CONFIG_BACKLIGHT_IC_KTD3102)
spinlock_t bl_ctrl_lock;
static int lcd_brightness = -1;
#endif

struct mutex dsi_tx_mutex;

/* common setting */
static char B9h[] = {
	0xB9,
	0xFF, 0x83, 0x89
};
static char CCh[] = {
	0xCC,
	0x0E,
};
static char DEh[] = {
	0xDE,
	0x05, 0x58
};
static char B1h[] = {
	0xB1,
	0x00, 0x00, 0x07, 0xEF, 0x50,
	0x05, 0x11, 0x74, 0xF1, 0x2A,
	0x34, 0x26, 0x26, 0x42, 0x01,
	0x3A, 0xF5, 0x20, 0x80
};
static char B2h[] = {
	0xB2,
	0x00, 0x00, 0x78, 0x04,
	0x07, 0x3F, 0x40
};
static char B4h[] = {
	0xB4,
	0x80, 0x08, 0x00, 0x32, 0x10, 0x00, 0x32, 0x13,
	0xC7, 0x00, 0x00, 0x00, 0x35, 0x00, 0x40, 0x04,
	0x37, 0x0A, 0x40, 0x1E, 0x52, 0x52, 0x0A, 0x0A,
	0x40, 0x0A, 0x40, 0x14, 0x46, 0x50, 0x0A,
};
static char D5h[] = {
	0xD5,
	0x80, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x60, 0x88, 0x88, 0x99, 0x88, 0x01, 0x45,
	0x88, 0x88, 0x01, 0x45, 0x23, 0x67, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x99, 0x54, 0x10, 0x88, 0x88, 0x76, 0x32, 0x54,
	0x10, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};
static char BAh[] = {
	0xBA,
	0x41, 0x93
};
static char _35h[] = {
	0x35, 0x0,
};
static char C0h[] = {
	0xC0,
	0x43, 0x17
};
static char CBh[] = {
	0xCB,
	0x07, 0x07
};
static char E0h[] = {
	0xE0
	,0x00
	,0x10
	,0x18
	,0x3A
	,0x3D
	,0x3F
	,0x26
	,0x46
	,0x07
	,0x0C
	,0x0E
	,0x12
	,0x14
	,0x12
	,0x13
	,0x11
	,0x18
	,0x00
	,0x10
	,0x18
	,0x3A
	,0x3D
	,0x3F
	,0x26
	,0x46
	,0x07
	,0x0C
	,0x0E
	,0x12
	,0x14
	,0x12
	,0x13
	,0x11
	,0x18
};
static char C1h[] = {
	0xC1
	,0x01
	,0x00
	,0x08
	,0x10
	,0x1A
	,0x21
	,0x29
	,0x31
	,0x37
	,0x3F
	,0x47
	,0x50
	,0x58
	,0x60
	,0x68
	,0x70
	,0x78
	,0x81
	,0x88
	,0x90
	,0x99
	,0xA0
	,0xA7
	,0xAF
	,0xB7
	,0xC0
	,0xC9
	,0xCE
	,0xD6
	,0xE0
	,0xE7
	,0xF1
	,0xF8
	,0xFF
	,0xFB
	,0x63
	,0xA1
	,0x2A
	,0x7D
	,0x69
	,0x8E
	,0x80
	,0x00
	,0x00
	,0x08
	,0x10
	,0x1A
	,0x21
	,0x29
	,0x31
	,0x37
	,0x3F
	,0x47
	,0x50
	,0x58
	,0x60
	,0x68
	,0x70
	,0x78
	,0x81
	,0x88
	,0x90
	,0x99
	,0xA0
	,0xA7
	,0xAF
	,0xB7
	,0xC0
	,0xC9
	,0xCE
	,0xD6
	,0xE0
	,0xE7
	,0xF1
	,0xF8
	,0xFF
	,0xFB
	,0x63
	,0xA1
	,0x2A
	,0x7D
	,0x69
	,0x8E
	,0x80
	,0x00
	,0x00
	,0x08
	,0x10
	,0x1A
	,0x21
	,0x29
	,0x31
	,0x37
	,0x3F
	,0x47
	,0x50
	,0x58
	,0x60
	,0x68
	,0x70
	,0x78
	,0x81
	,0x88
	,0x90
	,0x99
	,0xA0
	,0xA7
	,0xAF
	,0xB7
	,0xC0
	,0xC9
	,0xCE
	,0xD6
	,0xE0
	,0xE7
	,0xF1
	,0xF8
	,0xFF
	,0xFB
	,0x63
	,0xA1
	,0x2A
	,0x7D
	,0x69
	,0x8E
	,0x80
	,0x00
};

static char display_on[] = {0x29, 0x0,};
static char exit_sleep[] =  {0x11, 0x0,};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

static struct dsi_cmd_desc hx8389b_video_display_on_cmds[] = {

{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(B9h), B9h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(DEh), DEh},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(B1h), B1h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(B2h), B2h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 10,sizeof(B4h), B4h},

{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(B9h), B9h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(CCh), CCh},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(D5h), D5h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(BAh), BAh},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(_35h), _35h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(C0h), C0h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(CBh), CBh},

{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(E0h), E0h},
{DTYPE_GEN_LWRITE, 1, 0, 0, 0,sizeof(C1h), C1h},

{DTYPE_DCS_WRITE, 0, 0, 0, 120,sizeof(exit_sleep), exit_sleep},
{DTYPE_DCS_WRITE, 0, 0, 0, 10,sizeof(display_on), display_on},
};

static struct dsi_cmd_desc hx8389b_video_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};

static int mipi_hx8389b_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
#ifdef KEEP_BRIGHTNESS_ON_FIRST_BOOT
	static bool once = true;
#endif

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;
	if (!mfd->cont_splash_done) {
		mfd->cont_splash_done = 1;
		return 0;
	}

	printk(KERN_ERR "[LCD] mipi_hx8389b_lcd_on\n");

#if defined(CONFIG_BACKLIGHT_IC_KTD3102)
	pr_info("%s: DISP_BL_CONT_GPIO low\n", __func__);
#ifdef KEEP_BRIGHTNESS_ON_FIRST_BOOT
	if(!once)
#endif
	gpio_set_value(DISP_BL_CONT_GPIO, 0);
#endif

	if (mipi->mode == DSI_VIDEO_MODE) {
		mutex_lock(&dsi_tx_mutex);
		mipi_dsi_cmds_tx(&hx8389b_tx_buf,
			hx8389b_video_display_on_cmds,
			ARRAY_SIZE(hx8389b_video_display_on_cmds));
		mutex_unlock(&dsi_tx_mutex);	
	}

#if defined(CONFIG_BACKLIGHT_IC_KTD3102)
	pr_info("%s: DISP_BL_CONT_GPIO High\n", __func__);
#ifdef KEEP_BRIGHTNESS_ON_FIRST_BOOT
	if(!once)
#endif
	gpio_set_value(DISP_BL_CONT_GPIO, 1);
#ifdef KEEP_BRIGHTNESS_ON_FIRST_BOOT
	once = false;
#endif
#endif
	gpio_set_value(69, 1);
	return 0;
}

static int mipi_hx8389b_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	mipi  = &mfd->panel_info.mipi;

	printk(KERN_ERR "[LCD] mipi_hx8389b_lcd_off\n");
	if (mipi->mode == DSI_VIDEO_MODE) {
		mutex_lock(&dsi_tx_mutex);
		mipi_dsi_cmds_tx(&hx8389b_tx_buf,
			hx8389b_video_display_off_cmds,
			ARRAY_SIZE(hx8389b_video_display_off_cmds));
		mutex_unlock(&dsi_tx_mutex);	
	}

#if defined(CONFIG_BACKLIGHT_IC_KTD3102)	
	pr_info("%s: DISP_BL_CONT_GPIO low-block\n", __func__);
	gpio_set_value(DISP_BL_CONT_GPIO, 0);
        lcd_brightness = -1;
#endif
	gpio_set_value(7, 0);
	mdelay(1);
	gpio_set_value(69, 0);

	return 0;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static struct lcd_ops mipi_lcd_props;
static ssize_t mipi_hx8389b_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[30];
	sprintf(temp, "INH_%d\n", 534490);
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_hx8389b_lcdtype_show, NULL);
#endif

static int __devinit mipi_hx8389b_lcd_probe(struct platform_device *pdev)
{
	struct platform_device *pthisdev = NULL;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
	int ret;
#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd;
#endif
	pr_debug("%s\n", __func__);

	pthisdev = msm_fb_add_device(pdev);
	mutex_init(&dsi_tx_mutex);

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mipi_lcd_props);

	if (IS_ERR(lcd_device)) {
		int ret = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return ret;
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return ret;
	}
#endif
#if defined(CONFIG_MDNIE_LITE_TUNING)
		/*	mdnie sysfs create */
		init_mdnie_class();
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_hx8389b_lcd_probe,
	.driver = {
		.name   = "mipi_hx8389b",
	},
};

struct brt_value brt_table_aat[] = {
	{	255 ,	9	},	/*Max 255*/
	{	239 ,	9	},
	{	223 ,	9	},
	{	207 ,	10	},
	{	191 ,	11	},
	{	175 ,	12	},
	{	159 ,	13	},
	{	143 ,	14	},
	{	127 ,	15	},	/*Default 127*/
	{	114 ,	17	},
	{	102 ,	19	},
	{	90	,	21	},
	{	78	,	23	},
	{	66	,	25	},
	{	54	,	27	},
	{	42	,	29	},
	{	30	,	31	},	/*Dim 30 Min 10*/
	{	0	,	32	},	/*Off*/
};
#define MAX_BRT_STAGE (int)(sizeof(brt_table_aat)/sizeof(struct brt_value))
static int mipi_hx8389b_set_brightness_level(int level)
{
	int tune_level = 0;

	if (level > 0) {
		if (level < MIN_BRIGHTNESS_VALUE) {
			tune_level = AAT_DIMMING_VALUE; /* DIMMING */
		} else {
			int i;

			for (i = 0; i < MAX_BRT_STAGE; i++) {
				if (level <= brt_table_aat[i].level
					&& level > brt_table_aat[i+1].level) {
					tune_level = brt_table_aat[i].tune_level;
					break;
				}
			}
		}
	}
	return tune_level;
}

void mipi_hx8389b_set_backlight(int level)
{
	int tune_level = 0;

	spin_lock(&bl_ctrl_lock);
	tune_level = level;

	if(tune_level != lcd_brightness)
	{
		if (!tune_level) {
			gpio_set_value(DISP_BL_CONT_GPIO, 0);
			mdelay(3);
			lcd_brightness = tune_level;
		} else {
		       int pulse;
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
				lcd_brightness = MAX_BRIGHTNESS_IN_BLU;
			}
			pulse = (tune_level - lcd_brightness + MAX_BRIGHTNESS_IN_BLU)
							% MAX_BRIGHTNESS_IN_BLU;

			pr_debug("[KTD3102] tune_level: %d, pulse : %d, lcd_brightness : %d \n",tune_level,pulse,lcd_brightness); 

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

static void mipi_hx8389b_disp_backlight(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;

	static int bl_level_old;

	pr_info("%s Back light level:%d\n", __func__, mfd->bl_level);

	mipi  = &mfd->panel_info.mipi;
	if (bl_level_old == mfd->bl_level)
		goto end;
	if (!mfd->panel_power_on)
		goto end;

	mipi_hx8389b_set_backlight(mipi_hx8389b_set_brightness_level(mfd->bl_level));
	bl_level_old = mfd->bl_level;
end:
	return;
}

static struct msm_fb_panel_data hx8389b_panel_data = {
	.on	= mipi_hx8389b_lcd_on,
	.off = mipi_hx8389b_lcd_off,
	#if defined(CONFIG_BACKLIGHT_IC_KTD3102)
	.set_backlight = mipi_hx8389b_disp_backlight,
	#else
	.set_backlight = mipi_hx8389b_set_backlight,
	#endif
};

static int mipi_hx8389b_lcd_init(void)
{
	mipi_dsi_buf_alloc(&hx8389b_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&hx8389b_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}

int mipi_hx8389b_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;
	static int ch_used[3];

	printk(KERN_ERR "[LCD] mipi_hx8389b_device_register\n");
	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_hx8389b_lcd_init();
	if (ret) {
		pr_err("mipi_hx8389b_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_hx8389b", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	hx8389b_panel_data.panel_info = *pinfo;
	ret = platform_device_add_data(pdev, &hx8389b_panel_data,
				sizeof(hx8389b_panel_data));
	if (ret) {
		pr_debug("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_debug("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

