/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/lcd.h>
#include <linux/leds.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "mipi_ql_dsi2lvds.h"


#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <mach/msm8930-gpio.h>
#if defined(CONFIG_FB_MDP4_ENHANCE)
#include "mdp4_video_enhance.h"
#endif

static struct mipi_dsi2lvds_driver_data msd;
static unsigned int recovery_boot_mode;
extern unsigned int system_rev;
static int lcd_panel; 

struct mutex cabc_lock;

enum {
    LCD_STATUS_OFF = 0,
    LCD_STATUS_ON,
};

#define WA_FOR_FACTORY_MODE
#define READ_MTP_ONCE

#define DEFAULT_USLEEP 5

struct i2c_gpio {
	unsigned int scl;
	unsigned int sda;
};


static struct i2c_gpio gpio_i2c = {
		.scl	= 41,
		.sda	= 40,
};

#define GEN_QL_CSR_WRITE  {\
	START_BYTE, \
	CONTROL_BYTE_GEN, \
	0x29,  /* Data ID */\
	0x05,  /* Vendor Id 1 */\
	0x01,  /* Vendor Id 2 */\
	0x40,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* data LS */\
	0x00, \
	0x00, \
        0x00,  /* data MS */\
}

#define GEN_QL_CSR_OFFSET_LENGTH  {\
	START_BYTE, \
	CONTROL_BYTE_GEN, \
        0x29,  /* Data ID */\
        0x05,  /* Vendor Id 1 */\
        0x01,  /* Vendor Id 2 */\
        0x41,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* Length LS */\
        0x00,  /* Length MS */\
    }

#define GPIO_I2C_SCL_HIGH		gpio_set_value(gpio_i2c.scl, 1);
#define GPIO_I2C_SCL_LOW		gpio_set_value(gpio_i2c.scl, 0);
#define GPIO_I2C_SDA_HIGH		gpio_set_value(gpio_i2c.sda, 1);
#define GPIO_I2C_SDA_LOW		gpio_set_value(gpio_i2c.sda, 0);


#define GPIO_I2C_DELAY 1

#define START_BYTE       (0xc8)
#define START_READ_BYTE       (0xc9)
#define CONTROL_BYTE_GEN       (0x09u)

static void SCLH_SDAH(uint32_t delay)
{
	GPIO_I2C_SCL_HIGH;
	GPIO_I2C_SDA_HIGH;
	udelay(delay);
}

static void SCLH_SDAL(uint32_t delay)
{
	GPIO_I2C_SCL_HIGH;
	GPIO_I2C_SDA_LOW;
	udelay(delay);
}

static void SCLL_SDAH(uint32_t delay)
{
	GPIO_I2C_SCL_LOW;
	GPIO_I2C_SDA_HIGH;
	udelay(delay);
}

static void SCLL_SDAL(uint32_t delay)
{
	GPIO_I2C_SCL_LOW;
	GPIO_I2C_SDA_LOW;
	udelay(delay);
}

static void GPIO_I2C_LOW(uint32_t delay)
{
	SCLL_SDAL(delay);
	SCLH_SDAL(delay);
	SCLH_SDAL(delay);
	SCLL_SDAL(delay);
	}
	
static void GPIO_I2C_HIGH(uint32_t delay)
{
	SCLL_SDAH(delay);
	SCLH_SDAH(delay);
	SCLH_SDAH(delay);
	SCLL_SDAH(delay);
}

static void GPIO_I2C_START(uint32_t delay)
{
	SCLH_SDAH(5);
	SCLH_SDAL(5);
	udelay(5);
	SCLL_SDAL(30);
}


static void GPIO_I2C_END(uint32_t delay)
{
	SCLL_SDAH(50);
	SCLL_SDAL(5);
	SCLH_SDAL(5);
	SCLH_SDAH(5);
	udelay(30);
}

static void GPIO_I2C_ACK(uint32_t delay)
{
	uint32_t ack;

	GPIO_I2C_SCL_LOW;
	udelay(delay);

	/* SDA -> Input */
	gpio_tlmm_config(GPIO_CFG(gpio_i2c.sda, 0, GPIO_CFG_INPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	GPIO_CFG_DISABLE); 
	gpio_direction_input(gpio_i2c.sda);

	GPIO_I2C_SCL_HIGH;
	udelay(delay);
	ack = gpio_get_value(gpio_i2c.sda); 
	GPIO_I2C_SCL_HIGH;
	udelay(delay);

	/* SDA -> Onput Low */
	gpio_tlmm_config(GPIO_CFG(gpio_i2c.sda, 0, GPIO_CFG_OUTPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	GPIO_CFG_DISABLE); 
	gpio_set_value(gpio_i2c.sda, 0);
	udelay(delay);

	GPIO_I2C_SCL_LOW;
	udelay(delay);

	if (ack)
		pr_debug("%s :FUEL I2C  -> No ACK\n", __func__);

	}

#if 0 /*moved to mipi_dsi2lvds_cdp_panel_power()*/
void lvds_i2c_init(void)
{
	gpio_tlmm_config(GPIO_CFG(gpio_i2c.scl, 0, GPIO_CFG_OUTPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	GPIO_CFG_ENABLE); 
	gpio_tlmm_config(GPIO_CFG(gpio_i2c.sda, 0, GPIO_CFG_OUTPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	GPIO_CFG_ENABLE); 

	gpio_set_value(gpio_i2c.scl, 1);
	gpio_set_value(gpio_i2c.sda, 1);

}
#endif
void lvds_i2c_burst_write(uint8_t *Data, uint8_t length)
{
	uint32_t i, j;

	GPIO_I2C_START(GPIO_I2C_DELAY);

	for(j=0; j<length; j++)
	{
		for (i = 8; i > 0; i--) {
			if ((*Data >> (i - 1)) & 0x1){
				GPIO_I2C_HIGH(GPIO_I2C_DELAY);
				}
			else{
				GPIO_I2C_LOW(GPIO_I2C_DELAY);
				}
		}
		Data++;

		GPIO_I2C_ACK(GPIO_I2C_DELAY);

	}
	
	GPIO_I2C_END(GPIO_I2C_DELAY);

	}

void WriteRegister(u16 addr, u32 w_data)
{
	char buf[] = GEN_QL_CSR_WRITE;

	buf[6] = (uint8_t)addr & 0xff;  /* Address LS */
	buf[7] = (uint8_t)(addr >> 8) & 0xff;	/* Address MS */

	buf[8] = w_data & 0xff;
	buf[9] = (w_data >> 8) & 0xff;
	buf[10] = (w_data >> 16) & 0xff;
	buf[11] =(w_data >> 24) & 0xff;

	lvds_i2c_burst_write((char *)(&buf[0]), (uint8_t)12);

	}
static int first_boot = 1;
static int bl_reg_old = 0;
extern int lcd_id_get_adc_value(void);

static void send_i2c_lvds_data(void)
{
	gpio_set_value(LCD_BLIC_ON, 1);
	msleep(1);

	WriteRegister(0x700, 0x6C900040);	/*clk reset 1*/
#if defined(CONFIG_MACH_LT02_SPR) && !defined(CONFIG_MACH_LT02_SEA)
	WriteRegister(0x704, 0x30340);
#else
	WriteRegister(0x704, 0x30313);  	/*clk reset 2*/
#endif
	WriteRegister(0x70C, 0x00004604); 	/*low pwr regiset*/
	WriteRegister(0x710, 0x54D000B); 	/*use case register*/
	WriteRegister(0x714, 0x20); 		/*mux register*/
	WriteRegister(0x718, 0x00000102); 	/*pll power down control*/
	WriteRegister(0x71C, 0xA8002F);
	WriteRegister(0x720, 0x0);

	WriteRegister(0x154, 0x00000000);
	WriteRegister(0x154, 0x80000000);
	mdelay(1); /*For pll locking*/

	WriteRegister(0x700, 0x6C900840);
	WriteRegister(0x70C, 0x5E56);
	WriteRegister(0x718, 0x00000202);

	WriteRegister(0x154, 0x00000000);
	WriteRegister(0x154, 0x80000000);
	mdelay(1); /*For pll locking*/

	WriteRegister(0x37C, 0x00001063);
	WriteRegister(0x380, 0x82A86030);
	WriteRegister(0x384, 0x2861408B);
	WriteRegister(0x388, 0x00130285);
	WriteRegister(0x38C, 0x10630009);
	WriteRegister(0x394, 0x400B82A8);
	WriteRegister(0x600, 0x16CC78C);
	WriteRegister(0x604, 0x3FFFFFFF);	/*lvds disable*/
	WriteRegister(0x608, 0xD8C);

	WriteRegister(0x154, 0x00000000);
	WriteRegister(0x154, 0x80000000);

	WriteRegister(0x120, 0x5);
#if defined(CONFIG_MACH_LT02_SPR) && !defined(CONFIG_MACH_LT02_SEA)
	WriteRegister(0x124, 0x652c400);
	WriteRegister(0x128, 0x104010);
	WriteRegister(0x12C, 0xeb);
#else
	WriteRegister(0x124, 0x1D2C400);
	WriteRegister(0x128, 0x10300F);
	WriteRegister(0x12C, 0xD8);
#endif
	WriteRegister(0x130, 0x3C18);
	WriteRegister(0x134, 0x15);
	WriteRegister(0x138, 0xFF8000);
	WriteRegister(0x13C, 0x0);
	/*WriteRegister(0x114, 0xc6302);*/
	WriteRegister(0x140, 0x10000);

	/*enable VEE block*/
	WriteRegister(0x174, 0xff);
	WriteRegister(0x404, 0x55550822);
	WriteRegister(0x418, 0x555502ff);
	WriteRegister(0x410, 0x5E50300);

	WriteRegister(0x20C, 0x124);
	WriteRegister(0x21C, 0x0);
	WriteRegister(0x224, 0x7);
	WriteRegister(0x228, 0x50001);
	WriteRegister(0x22C, 0xFF03);
	WriteRegister(0x230, 0x1);
	WriteRegister(0x234, 0xCA033E10);
	WriteRegister(0x238, 0x00000060);
	WriteRegister(0x23C, 0x82E86030);
	WriteRegister(0x244, 0x001E0285);
#if defined(CONFIG_MACH_LT02_SPR) && !defined(CONFIG_MACH_LT02_SEA)
	WriteRegister(0x258, 0x30019);
#else	
	WriteRegister(0x258, 0x20007);
#endif

	/*backlight duty ration control when device is first bring up.*/
	/*vee strenght initialization*/
	WriteRegister(0x400, 0x0);

	WriteRegister(0x158, 0x0);
	WriteRegister(0x158, 0x1);

	mdelay(1); /*For pll locking*/
}

static void send_i2c_lvds_data2(void)
{
	int lcd_id_value = 0;

	WriteRegister(0x160, 0x8f0/*0xff*/);	/*pwm freq.*/
	WriteRegister(0x604, 0x3FFFFFE0);	/*lvds enable*/
	msleep(200);

	WriteRegister(0x138, 0x3fff0000);	/*gpio*/
	WriteRegister(0x15c, 0x5);	/*pwm enable*/

	if((first_boot == 1) || recovery_boot_mode){
		WriteRegister(0x164, 381);/*75*5078/1000*/
        	/*
		sdc
		 960000~990000
		 */

		lcd_id_value = lcd_id_get_adc_value();
		if ((lcd_id_value > 900000) && (lcd_id_value < 1000000) )
			lcd_panel = SDC_PANEL;
		else 
			lcd_panel = BOE_PANEL;
		pr_info(" - %s:---------------->:lcd_id_value:%d (%d)\n", __func__, lcd_id_value, lcd_panel);

		first_boot = 0;
	}else {
		WriteRegister(0x164, bl_reg_old);	/*clock for bl level*/
		pr_debug("** %s (%d) **\n", __func__,bl_reg_old);
	}
}

/**
 * LCD ON.
 *
 * Set LCD On via MIPI interface .
 * Set Backlight on.*/

static int mipi2lvds_disp_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	pr_info("**+ %s **\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi = &mfd->panel_info.mipi;

#ifdef CONFIG_FB_MDP4_ENHANCE
	is_negativeMode_on();
#endif

	if (mipi->mode == DSI_VIDEO_MODE)
			send_i2c_lvds_data();

	pr_info("**-%s:lcd_id\n", __func__);

return 0;
}

static int mipi2lvds_disp_late_init(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
        struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	pr_info("**+ %s **\n", __func__);
        mipi = &mfd->panel_info.mipi;
        
        if (mipi->mode == DSI_VIDEO_MODE)
	send_i2c_lvds_data2();

	mfd->resume_state = MIPI_RESUME_STATE;

	pr_info("**-%s:lcd_id: (%d)\n", __func__, lcd_panel);

return 0;
}

static int mipi2lvds_disp_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	pr_info("+%s", __func__);

	mfd = platform_get_drvdata(msd.msm_pdev);

	WriteRegister(0x164, 0x00);
	mdelay(1);
	WriteRegister(0x15c, 0x0);
	mdelay(200);

	mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("-%s", __func__);
	
	return 0;
}

static void __devinit mipi2lvds_disp_shutdown(struct platform_device *pdev)
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

static void mipi2lvds_disp_set_pwm_duty(int level)
{

	int vx5b3d_level = 0;
	u32 vee_strenght = 0;
	static u32 prev_vee_strenght=0;
	
	/* brightness tuning*/
	if (level > MAX_BRIGHTNESS_LEVEL)
		level = MAX_BRIGHTNESS_LEVEL;

	if (lcd_panel == SDC_PANEL ){
	if (level >= MID_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - MID_BRIGHTNESS_LEVEL) *
			(V5D3BX_MAX_BRIGHTNESS_LEVEL_SDC - V5D3BX_MID_BRIGHTNESS_LEVEL_SDC) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + V5D3BX_MID_BRIGHTNESS_LEVEL_SDC;
	} else if (level >= LOW_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - LOW_BRIGHTNESS_LEVEL) *
			(V5D3BX_MID_BRIGHTNESS_LEVEL_SDC - V5D3BX_LOW_BRIGHTNESS_LEVEL) / (MID_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + V5D3BX_LOW_BRIGHTNESS_LEVEL;
	} else if (level >= DIM_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - DIM_BRIGHTNESS_LEVEL) *
		(V5D3BX_LOW_BRIGHTNESS_LEVEL - V5D3BX_DIM_BRIGHTNESS_LEVEL) / (LOW_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + V5D3BX_DIM_BRIGHTNESS_LEVEL;
	} else if (level > 0)
		vx5b3d_level  = V5D3BX_DIM_BRIGHTNESS_LEVEL;
	else {
		vx5b3d_level = 0;
		pr_info("level = [%d]: vx5b3d_level = [%d]\n",\
			level,vx5b3d_level);	
	}
	}
	else if (lcd_panel == BOE_PANEL){
		if (level >= MID_BRIGHTNESS_LEVEL) {
			vx5b3d_level  = (level - MID_BRIGHTNESS_LEVEL) *
			(V5D3BX_MAX_BRIGHTNESS_LEVEL_BOE - V5D3BX_MID_BRIGHTNESS_LEVEL_BOE) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + V5D3BX_MID_BRIGHTNESS_LEVEL_BOE;
		} else if (level >= LOW_BRIGHTNESS_LEVEL) {
			vx5b3d_level  = (level - LOW_BRIGHTNESS_LEVEL) *
			(V5D3BX_MID_BRIGHTNESS_LEVEL_BOE - V5D3BX_LOW_BRIGHTNESS_LEVEL) / (MID_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + V5D3BX_LOW_BRIGHTNESS_LEVEL;
		} else if (level >= DIM_BRIGHTNESS_LEVEL) {
			vx5b3d_level  = (level - DIM_BRIGHTNESS_LEVEL) *
			(V5D3BX_LOW_BRIGHTNESS_LEVEL - V5D3BX_DIM_BRIGHTNESS_LEVEL) / (LOW_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + V5D3BX_DIM_BRIGHTNESS_LEVEL;
		} else if (level > 0)
			vx5b3d_level  = V5D3BX_DIM_BRIGHTNESS_LEVEL;
		else {
			vx5b3d_level = 0;
			pr_info("level = [%d]: vx5b3d_level = [%d]\n",\
				level,vx5b3d_level);	
		}
	}
	
	if (msd.dstat.cabc) {

		switch (msd.dstat.auto_brightness) {

		case	0 ... 3:
			vee_strenght = V5D3BX_DEFAULT_STRENGHT;				
			break;
		case	4 ... 5:
			vee_strenght = V5D3BX_DEFAULT_LOW_STRENGHT;
			break;
		case	6 ... 8:
			vee_strenght = V5D3BX_DEFAULT_HIGH_STRENGHT;
			break;	
		default:
			vee_strenght = V5D3BX_DEFAULT_STRENGHT;
		}
		vee_strenght = V5D3BX_VEESTRENGHT | ((vee_strenght) << 27);

	if (!(msd.dstat.auto_brightness >= 5))
		vx5b3d_level = (vx5b3d_level * V5D3BX_CABCBRIGHTNESSRATIO) / 1000;

	} else {
		vee_strenght = V5D3BX_VEESTRENGHT | (V5D3BX_VEEDEFAULTVAL << 27);
	}

	if((vee_strenght != prev_vee_strenght)&& vx5b3d_level) {
		WriteRegister(0x400,vee_strenght);
		prev_vee_strenght = vee_strenght;
	}
	
	if (vx5b3d_level != 0) {
		pr_info("[MIPI2LVDS]:level=%d vx5b3d_level:%d auto_brightness:%d CABC:%d \n",\
			level,vx5b3d_level,msd.dstat.auto_brightness,msd.dstat.cabc);

		WriteRegister(0x164,(vx5b3d_level*V5D3BX_10KHZ_DEFAULT_RATIO)/1000);
		bl_reg_old = (vx5b3d_level*V5D3BX_10KHZ_DEFAULT_RATIO)/1000 ;
	}

}

static void mipi2lvds_disp_set_backlight(struct msm_fb_data_type *mfd)
{
	pr_info("%s : level (%d)\n", __func__, mfd->bl_level);

	if (mfd->resume_state == MIPI_RESUME_STATE) 
	mipi2lvds_disp_set_pwm_duty(mfd->bl_level);
	return;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi2lvds_disp_early_suspend(struct early_suspend *h)
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

static void mipi2lvds_disp_late_resume(struct early_suspend *h)
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

	pr_info("%s", __func__);
}
#endif

unsigned char mipi2lvds_show_cabc(void )
{
	return msd.dstat.cabc;
}

void mipi2lvds_store_cabc(unsigned char cabc)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if(msd.dstat.auto_brightness)
		return;
	msd.dstat.cabc=cabc;
	mutex_lock(&cabc_lock);
	mipi2lvds_disp_set_backlight(mfd);
	mutex_unlock(&cabc_lock);
	pr_info("%s :[MIPI2LVDS] CABC: %d\n", __func__,msd.dstat.cabc);

}

static ssize_t siop_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",msd.dstat.cabc);
	pr_info("%s :[MIPI2LVDS] CABC: %d\n", __func__, msd.dstat.cabc);
	return rc;
}

static ssize_t siop_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	
	mfd = platform_get_drvdata(msd.msm_pdev);

	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return size;
	}

	if (msd.dstat.auto_brightness == 0){
	if (sysfs_streq(buf, "1") && !msd.dstat.cabc)
		msd.dstat.cabc = true;
	else if (sysfs_streq(buf, "0") && msd.dstat.cabc)
		msd.dstat.cabc = false;
	else{
			pr_info("[%s]:do nothing!!:already set cabc(%d)!!", __func__, msd.dstat.cabc);
		return size;
	}

		mutex_lock(&cabc_lock);
		mipi2lvds_disp_set_backlight(mfd);
		mutex_unlock(&cabc_lock);
		pr_info("[%s] set cabc by siop in manual bl : %d\n", __func__, msd.dstat.cabc);
		
	} else {
		pr_info("[%s] do nothing:cabc already controlled by auto bl : %d\n", __func__, msd.dstat.auto_brightness);
	}

	return size;

}

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			siop_enable_show,
			siop_enable_store);

static struct lcd_ops mipi2lvds_disp_props = {

	.get_power = NULL,
	.set_power = NULL,

};

static ssize_t mipi2lvds_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("%s :[MIPI2LVDS] auto_brightness : %d\n", __func__, msd.dstat.auto_brightness);

	return rc;
}

static ssize_t mipi2lvds_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	unsigned char prev_auto_brightness;

	mfd = platform_get_drvdata(msd.msm_pdev);
	

	prev_auto_brightness = msd.dstat.auto_brightness;
	if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		msd.dstat.auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		msd.dstat.auto_brightness = 3;	
	else if (sysfs_streq(buf, "4"))
		msd.dstat.auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		msd.dstat.auto_brightness = 5;	
	else if (sysfs_streq(buf, "6"))
		msd.dstat.auto_brightness = 6;	
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (msd.dstat.auto_brightness == 0) {		
		WriteRegister(0x710,0x054D000B );
		mdelay(1);
		WriteRegister(0x174,0x0);

	} else {
		WriteRegister(0x710,0x054D004B );
		mdelay(1);
		WriteRegister(0x174,0xff);
	}
	
	mdelay(1);
	
	mutex_lock(&cabc_lock);

	if(msd.dstat.auto_brightness)
		msd.dstat.cabc = true;
	else
		msd.dstat.cabc = false;

	mipi2lvds_disp_set_backlight(mfd);
	mutex_unlock(&cabc_lock);
	
	return size;
}



static ssize_t mipi2lvds_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, strnlen(msd.mpd->panel_name, 20) + 1,
						msd.mpd->panel_name);
	strncat(buf, temp, 20);
	return strnlen(buf, 20);
}

static DEVICE_ATTR(lcd_type, S_IRUGO, mipi2lvds_disp_lcdtype_show, NULL);


static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi2lvds_auto_brightness_show,
			mipi2lvds_auto_brightness_store);

static int __devinit mipi2lvds_vx5b3d_disp_probe(struct platform_device *pdev)
{
	struct platform_device *msm_fb_added_dev;
	int ret;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
	struct backlight_device *bd = NULL;
#endif

	if (pdev->id == 0) {
		msd.mipi_dsi2lvds_disp_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_added_dev = msm_fb_add_device(pdev);
	mutex_init(&cabc_lock);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_LCD_CLASS_DEVICE)
	msd.msm_pdev = msm_fb_added_dev;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi2lvds_disp_early_suspend;
	msd.early_suspend.resume = mipi2lvds_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&msd.early_suspend);

#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mipi2lvds_disp_props);

	if (IS_ERR(lcd_device)) {
		ret = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return ret;
	}


	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return ret;
	}
	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_siop_enable.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_siop_enable.attr.name);
	}

	ret = sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}
#endif
#if defined(CONFIG_FB_MDP4_ENHANCE)
		init_mdnie_class();
#endif

	return 0;

}

static struct platform_driver this_driver = {
	.probe  = mipi2lvds_vx5b3d_disp_probe,
	.driver = {
		.name   = "mipi2lvds_vx5b3d",
	},
	.shutdown = mipi2lvds_disp_shutdown
};

static struct msm_fb_panel_data vx5b3d_panel_data = {
	.late_init = mipi2lvds_disp_late_init,
	.on		= mipi2lvds_disp_on,
	.off		= mipi2lvds_disp_off,
	.set_backlight	= mipi2lvds_disp_set_backlight,
};

static int ch_used[3];

int mipi2lvds_vx5b3d_disp_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel,
					struct mipi_panel_data *mpd)
{
	struct platform_device *pdev = NULL;
	int ret = 0;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi2lvds_vx5b3d",
					   (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	vx5b3d_panel_data.panel_info = *pinfo;
	msd.mpd = mpd;
	if (!msd.mpd) {
		printk(KERN_ERR
		  "%s: get mipi_panel_data failed!\n", __func__);
		goto err_device_put;
	}
	mpd->msd = &msd;
	ret = platform_device_add_data(pdev, &vx5b3d_panel_data,
		sizeof(vx5b3d_panel_data));
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

static int __init mipi2lvds_disp_init(void)
{
	mipi_dsi_buf_alloc(&msd.dsi2lvds_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&msd.dsi2lvds_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
module_init(mipi2lvds_disp_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Quick Logic VX5B3D LCD driver");
