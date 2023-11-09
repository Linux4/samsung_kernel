/*
 * Copyright (C) 2015 HUAQIN Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/timer.h>
#include <string.h>
#else
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 start*/
#if defined (CONFIG_HQ_PROJECT_HS04) || defined (CONFIG_HQ_PROJECT_HS03S)
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#endif
/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 end*/
#endif

#include <linux/pinctrl/consumer.h>

#ifndef CONFIG_HQ_EXT_LCD_BIAS
#include "../lcm/inc/lcm_pmic.h"
#endif

#include "lcm_bias.h"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
#ifdef BUILD_LK
static struct mt_i2c_t lcd_bias_i2c;
#else
static struct i2c_client *lcd_bias_i2c_client;
static struct pinctrl *lcd_bias_pctrl; /* static pinctrl instance */

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lcd_bias_dts_probe(struct platform_device *pdev);
static int lcd_bias_dts_remove(struct platform_device *pdev);
static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bias_i2c_remove(struct i2c_client *client);
#endif
/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 start*/
#if defined (CONFIG_HQ_PROJECT_HS04) || defined (CONFIG_HQ_PROJECT_HS03S)
int lcm_bias_state;
#endif
/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 end*/

/*****************************************************************************
 * Extern Area
 *****************************************************************************/
#ifdef CONFIG_HQ_EXT_LCD_BIAS
static void ext_lcd_bias_write_byte(unsigned char addr, unsigned char value)
{
    int ret = 0;
    unsigned char write_data[2] = {0};

    write_data[0] = addr;
    write_data[1] = value;

#ifdef BUILD_LK
    lcd_bias_i2c.id = LCD_BIAS_I2C_BUSNUM;
    lcd_bias_i2c.addr = LCD_BIAS_I2C_ADDR;
    lcd_bias_i2c.mode = LCD_BIAS_ST_MODE;
    lcd_bias_i2c.speed = LCD_BIAS_MAX_ST_MODE_SPEED;
    ret = i2c_write(&lcd_bias_i2c, write_data, 2);
#else
    if (NULL == lcd_bias_i2c_client) {
        LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_i2c_client is null!!\n");
        return ;
    }
    ret = i2c_master_send(lcd_bias_i2c_client, write_data, 2);
#endif

    if (ret < 0)
        LCD_BIAS_PRINT("[LCD][BIAS] i2c write data fail!! ret=%d\n", ret);
}

static void ext_lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value)
{
    unsigned char level;

    level = (value - 4000) / 100;  //eg.  5.0V= 4.0V + Hex 0x0A (Bin 0 1010) * 100mV

#ifdef BUILD_LK
    mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);

    mt_set_gpio_mode(GPIO_LCD_BIAS_ENN_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCD_BIAS_ENN_PIN, GPIO_DIR_OUT);

    if (seq == FIRST_VSP_AFTER_VSN) {
        if (en) {
            mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
            mdelay(1);
            ext_lcd_bias_write_byte(LCD_BIAS_VPOS_ADDR, level);
            mdelay(5);
            mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
            ext_lcd_bias_write_byte(LCD_BIAS_VNEG_ADDR, level);
        } else {
            mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
            mdelay(5);
            mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
        }
    } else if (seq == FIRST_VSN_AFTER_VSP) {
        if (en) {
            mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
            mdelay(1);
            ext_lcd_bias_write_byte(LCD_BIAS_VNEG_ADDR, level);
            mdelay(5);
            mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
            ext_lcd_bias_write_byte(LCD_BIAS_VPOS_ADDR, level);
        } else {
            mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
            mdelay(5);
            mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
        }
    }

#else
    if (seq == FIRST_VSP_AFTER_VSN) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP1);
            mdelay(1);
            ext_lcd_bias_write_byte(LCD_BIAS_VPOS_ADDR, level);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN1);
            ext_lcd_bias_write_byte(LCD_BIAS_VNEG_ADDR, level);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP0);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN0);
        }
    } else if (seq == FIRST_VSN_AFTER_VSP) {
        if (en) {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN1);
            mdelay(1);
            ext_lcd_bias_write_byte(LCD_BIAS_VNEG_ADDR, level);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP1);
            ext_lcd_bias_write_byte(LCD_BIAS_VPOS_ADDR, level);
        } else {
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENN0);
            mdelay(5);
            lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE_ENP0);
        }
    }

#endif
}

#else

static void pmic_lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value)
{
    if (en) {
        pmic_lcd_bias_set_vspn_vol(value);
    }

	pmic_lcd_bias_set_vspn_en(en, seq);
}
#endif

void lcd_bias_set_vspn(unsigned int en, unsigned int seq, unsigned int value)
{
    if ((value <= 4000) || (value > 6200)) {
        LCD_BIAS_PRINT("[LCD][BIAS] unreasonable voltage value:%d\n", value);
        return ;
    }

#ifdef CONFIG_HQ_EXT_LCD_BIAS
    ext_lcd_bias_set_vspn(en, seq, value);
#else
    pmic_lcd_bias_set_vspn(en, seq, value);
#endif
}

#ifndef BUILD_LK
/*****************************************************************************
 * Data Structure
 *****************************************************************************/
static const char *lcd_bias_state_name[LCD_BIAS_GPIO_STATE_MAX] = {
    "lcd_bias_gpio_enp0",
    "lcd_bias_gpio_enp1",
    "lcd_bias_gpio_enn0",
    "lcd_bias_gpio_enn1"
};/* DTS state mapping name */

static const struct of_device_id gpio_of_match[] = {
    { .compatible = "mediatek,gpio_lcd_bias", },
    {},
};

static const struct of_device_id i2c_of_match[] = {
    { .compatible = "mediatek,i2c_lcd_bias", },
    {},
};

static const struct i2c_device_id lcd_bias_i2c_id[] = {
    {"Lcd_Bias_I2C", 0},
    {},
};

static struct platform_driver lcd_bias_platform_driver = {
    .probe = lcd_bias_dts_probe,
    .remove = lcd_bias_dts_remove,
    .driver = {
        .name = "Lcd_Bias_DTS",
        .of_match_table = gpio_of_match,
    },
};

static struct i2c_driver lcd_bias_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
    .id_table = lcd_bias_i2c_id,
    .probe = lcd_bias_i2c_probe,
    .remove = lcd_bias_i2c_remove,
    .driver = {
        .name = "Lcd_Bias_I2C",
        .of_match_table = i2c_of_match,
    },
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static long lcd_bias_set_state(const char *name)
{
    int ret = 0;
    struct pinctrl_state *pState = 0;

    BUG_ON(!lcd_bias_pctrl);

    pState = pinctrl_lookup_state(lcd_bias_pctrl, name);
    if (IS_ERR(pState)) {
        pr_err("set state '%s' failed\n", name);
        ret = PTR_ERR(pState);
        goto exit;
    }

    /* select state! */
    pinctrl_select_state(lcd_bias_pctrl, pState);

exit:
    return ret; /* Good! */
}

void lcd_bias_gpio_select_state(LCD_BIAS_GPIO_STATE s)
{
    BUG_ON(!((unsigned int)(s) < (unsigned int)(LCD_BIAS_GPIO_STATE_MAX)));
    lcd_bias_set_state(lcd_bias_state_name[s]);
}

static int lcd_bias_dts_init(struct platform_device *pdev)
{
    int ret = 0;
    struct pinctrl *pctrl;

    /* retrieve */
    pctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(pctrl)) {
        dev_err(&pdev->dev, "Cannot find disp pinctrl!");
        ret = PTR_ERR(pctrl);
        goto exit;
    }

    lcd_bias_pctrl = pctrl;

exit:
    return ret;
}

static int lcd_bias_dts_probe(struct platform_device *pdev)
{
    int ret = 0;

    ret = lcd_bias_dts_init(pdev);
    if (ret) {
        LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_dts_probe failed\n");
        return ret;
    }

/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 start*/
#if defined (CONFIG_HQ_PROJECT_HS04) || defined (CONFIG_HQ_PROJECT_HS03S)
    lcm_bias_state = of_get_named_gpio(pdev->dev.of_node, "lcm_bias,vsp", 0);
    gpio_request(lcm_bias_state,"vsp_gpio");
#endif
/*hs04 code for DEAL6398A-1875 by zhawei at 20221017 end*/

    LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_dts_probe success\n");

    return 0;
}

static int lcd_bias_dts_remove(struct platform_device *pdev)
{
    platform_driver_unregister(&lcd_bias_platform_driver);

    return 0;
}

static int lcd_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    if (NULL == client) {
        LCD_BIAS_PRINT("[LCD][BIAS] i2c_client is NULL\n");
        return -1;
    }

    lcd_bias_i2c_client = client;
    LCD_BIAS_PRINT("[LCD][BIAS] lcd_bias_i2c_probe success : info==>name=%s addr=0x%x\n",
		client->name, client->addr);
    return 0;
}

static int lcd_bias_i2c_remove(struct i2c_client *client)
{
    lcd_bias_i2c_client = NULL;
    i2c_unregister_device(client);

    return 0;
}

static int __init lcd_bias_init(void)
{
    if (i2c_add_driver(&lcd_bias_i2c_driver)) {
        LCD_BIAS_PRINT("[LCD][BIAS] Failed to register lcd_bias_i2c_driver!\n");
        return -1;
    }

    if (platform_driver_register(&lcd_bias_platform_driver)) {
        LCD_BIAS_PRINT("[LCD][BIAS] Failed to register lcd_bias_platform_driver!\n");
        i2c_del_driver(&lcd_bias_i2c_driver);
        return -1;
    }

    return 0;
}

static void __exit lcd_bias_exit(void)
{
    platform_driver_unregister(&lcd_bias_platform_driver);
    i2c_del_driver(&lcd_bias_i2c_driver);
}

module_init(lcd_bias_init);
module_exit(lcd_bias_exit);

MODULE_AUTHOR("Lucas Gao <gaozhengwei@huaqin.com>");
MODULE_DESCRIPTION("MTK LCD BIAS I2C Driver");
MODULE_LICENSE("GPL");
#endif

