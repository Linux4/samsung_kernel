/* drivers/leds/rt5033_fled.c
 * RT5033 Flash LED Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/leds/rtfled.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mfd/rt5033.h>
#include <linux/mfd/rt5033_irq.h>
#include <linux/leds/rt5033_fled.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#define ALIAS_NAME "rt5033-fled"

#define RT5033_FLED_INFO(format, args...) \
    printk(KERN_INFO "%s:%s() line-%d: " format, \
            ALIAS_NAME, __FUNCTION__, __LINE__, ## args)
#define RT5033_FLED_WARN(format, args...) \
    printk(KERN_WARNING "%s:%s() line-%d: " format, \
            ALIAS_NAME, __FUNCTION__, __LINE__, ## args)
#define RT5033_FLED_ERR(format, args...) \
    printk(KERN_ERR "%s:%s() line-%d: " format, \
            ALIAS_NAME, __FUNCTION__, __LINE__, ## args)

#define EN_FLED_IRQ 0

typedef struct rt5033_fled_info {
    rt_fled_info_t base;
    const rt5033_fled_platform_data_t *pdata;
    rt5033_mfd_chip_t *chip;
    struct mutex io_lock;
    struct i2c_client *i2c_client;
    int torch_current;
    int strobe_current;
    struct power_supply *psy_chg;
    int charging_current;
    int ta_good;
    int led_count;
} rt5033_fled_info_t;


static struct platform_device rt_fled_pdev = {
	.name = "rt-flash-led",
	.id = -1,
};

#define RT5033_TA_NOT_EXIST 0
#define RT5033_TA_EXIST     1

#define RT5033_TA_NOT_GOOD  0
#define RT5033_TA_GOOD      1



int check_ta_good_enough(rt5033_fled_info_t *info, int cur_uA)
{
#ifdef CONFIG_CHARGER_RT5033
    int cur_mA = info->led_count * cur_uA / 1000;
    pr_info("chg current = %d (mA), fled consumption %d mA\n",
                info->charging_current, cur_mA);
    return (info->charging_current >= (cur_mA + 100))
                ? RT5033_TA_GOOD : RT5033_TA_NOT_GOOD;

#else
        return RT5033_TA_NOT_GOOD;
#endif // CONFIG_CHARGER_RT5033
}


static int check_ta_status(struct rt_fled_info *fled_info)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    union power_supply_propval val;
    val.intval = 0;
#ifdef CONFIG_CHARGER_RT5033
    if (info->psy_chg == NULL)
        info->psy_chg = get_power_supply_by_name("sec-charger");
    if (info->psy_chg)
    {
        info->psy_chg->get_property(info->psy_chg,
                                       POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    }
#endif // CONFIG_CHARGER_RT5033
    info->charging_current = val.intval;
    pr_info("chg current = %d (mA)\n", val.intval);
    return (info->charging_current) ? RT5033_TA_EXIST : RT5033_TA_NOT_EXIST;
}

static int rt5033_set_fled_osc_en(struct i2c_client *iic, int en)
{
    return (en?rt5033_set_bits:rt5033_clr_bits)(iic, 0x1a, (1 << 5));
}

static int rt5033_fled_init(struct rt_fled_info *fled_info)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rt5033_mfd_platform_data_t *mfd_pdata;
    BUG_ON(info == NULL);
    mfd_pdata = info->chip->pdata;
    mutex_lock(&info->io_lock);
    rt5033_set_bits(info->i2c_client, RT5033_FLED_RESET, 0x80);
    if (!info->pdata->fled1_en)
        rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x01);
    if (!info->pdata->fled2_en)
        rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x02);
    if (info->pdata->fled_mid_track_alive)
        rt5033_set_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 6));
    if (info->pdata->fled_mid_auto_track_en)
        rt5033_set_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 7));
    rt5033_reg_write(info->i2c_client, RT5033_FLED_STROBE_CONTROL1,
                     (info->pdata->fled_timeout_current_level << 5) |
                     info->pdata->fled_strobe_current);
    info->base.flashlight_dev->props.strobe_brightness =
                            info->pdata->fled_strobe_current;

    rt5033_reg_write(info->i2c_client, RT5033_FLED_STROBE_CONTROL2,
                     info->pdata->fled_strobe_timeout);
    info->base.flashlight_dev->props.strobe_timeout =
                    info->base.hal->fled_strobe_timeout_list(fled_info,
                        info->pdata->fled_strobe_timeout);
    RT5033_FLED_INFO("Strobe timeout = %d ms\n",
           info->base.flashlight_dev->props.strobe_timeout);
    rt5033_reg_write(info->i2c_client, RT5033_FLED_CONTROL1,
                     (info->pdata->fled_torch_current << 4) |
                     info->pdata->fled_lv_protection);
    info->base.flashlight_dev->props.torch_brightness =
                            info->pdata->fled_torch_current;
    rt5033_assign_bits(info->i2c_client, RT5033_FLED_CONTROL2,
                       0x3f, info->pdata->fled_mid_level);
    info->led_count = info->pdata->fled1_en + info->pdata->fled2_en;
    mutex_unlock(&info->io_lock);
    return 0;
}

static int rt5033_fled_suspend(struct rt_fled_info *info, pm_message_t state)
{
    RT5033_FLED_INFO("Suspend\n");
    return 0;
}

static int rt5033_fled_resume(struct rt_fled_info *info)
{
    RT5033_FLED_INFO("Resume\n");
    return 0;
}

int rt5033_fled_set_ta_status(struct i2c_client *iic, int ta_good, int ta_exist)
{
    return rt5033_assign_bits(iic, RT5033_FLED_CONTROL5, (0x03 << 6),
                       (ta_good << 6) | (ta_exist << 7));
}
EXPORT_SYMBOL(rt5033_fled_set_ta_status);

inline static int rt5033_set_uug_status(struct i2c_client *iic,int uug)
{
    RT5033_FLED_INFO("Set uug = %d\n", uug);
    return rt5033_assign_bits(iic, 0x19, 0x02, uug);
}

static int rt5033_fled_set_mode(struct rt_fled_info *fled_info, flashlight_mode_t mode)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    int ta_exist;
    switch (mode)
    {
        case FLASHLIGHT_MODE_OFF:
            rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION2, 0x80);
            usleep_range(500,1000);
            rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION2, 0x01);
            rt5033_fled_set_ta_status(info->i2c_client, 0, 0);
            rt5033_set_uug_status(info->i2c_client, 0x02);
            rt5033_set_fled_osc_en(info->i2c_client, 0);
            break;
        case FLASHLIGHT_MODE_TORCH:
        case FLASHLIGHT_MODE_MIXED:
            info->torch_current = fled_info->hal->fled_get_torch_current(fled_info);
            ta_exist = check_ta_status(fled_info);
            if (ta_exist)
                info->ta_good = check_ta_good_enough(info, info->torch_current);
            else
                info->ta_good = 0;
            rt5033_fled_set_ta_status(info->i2c_client, info->ta_good, ta_exist);
            rt5033_set_uug_status(info->i2c_client, (info->ta_good) ? 0x02 : 0x00);
            rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x04);
            rt5033_set_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 7)); // Enable AUTO TRACK
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x01);
            break;
        case FLASHLIGHT_MODE_FLASH:
            info->strobe_current = fled_info->hal->fled_get_torch_current(fled_info);
            ta_exist = check_ta_status(fled_info);
            if (ta_exist)
                info->ta_good = check_ta_good_enough(info, info->strobe_current);
            else
                info->ta_good = 0;
            rt5033_fled_set_ta_status(info->i2c_client, info->ta_good, ta_exist);
            rt5033_set_uug_status(info->i2c_client, (info->ta_good) ? 0x02 : 0x00);
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x00);
            rt5033_set_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x04);
            rt5033_clr_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 7)); // DISABLE AUTO TRACK
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x01);
            break;
        default:
            return -EINVAL;
    }
    info->base.flashlight_dev->props.mode = mode;
    return 0;
}

static int rt5033_fled_get_mode(struct rt_fled_info *info)
{
    rt5033_fled_info_t *rt5033_fled_info = (rt5033_fled_info_t *)info;
    return rt5033_fled_info->base.flashlight_dev->props.mode;
}

static int rt5033_fled_strobe(struct rt_fled_info *fled_info)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rt5033_set_fled_osc_en(info->i2c_client, 1);
    /* delay 1500 ~ 1660 if ta_good = 1 to
     * make delay time same with ta_good = 0 */
    if (info->ta_good)
        usleep_range(1500, 1660);
    switch (info->base.flashlight_dev->props.mode)
    {
        case FLASHLIGHT_MODE_FLASH:
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x81);
            break;
        case FLASHLIGHT_MODE_MIXED:
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x01);
            rt5033_set_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x04);
            rt5033_clr_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 7)); // DISABLE AUTO TRACK
            rt5033_reg_write(info->i2c_client, RT5033_FLED_FUNCTION2, 0x81);
            break;
        default:
            RT5033_FLED_ERR("Error : not flash / mixed mode\n");
            return -EINVAL;
    }
    return 0;
}

static int torch_current[] = {
    12500,
    25000,
    37500,
    50000,
    62500,
    75000,
    87500,
    100000,
    112500,
    125000,
    137500,
    150000,
    162500,
    175000,
    187500,
    200000,
};


/* Return value : -EINVAL => selector parameter is out of range, otherwise current in mA*/
static int rt5033_fled_troch_current_list(struct rt_fled_info *info, int selector)
{
    if (selector < 0 || selector >= ARRAY_SIZE(torch_current))
        return -EINVAL;
    return torch_current[selector];
}


static int rt5033_fled_strobe_current_list(struct rt_fled_info *info, int selector)
{
    if (selector <0 || selector>= 31)
        return -EINVAL;
    return (50 + selector*25)*1000;
}

static int strobe_timeout_level[] = {
    50000,
    75000,
    100000,
    125000,
    150000,
    175000,
    200000,
};


static int rt5033_fled_timeout_level_list(struct rt_fled_info *info, int selector)
{
    if (selector < 0 || selector >= ARRAY_SIZE(strobe_timeout_level))
        return -EINVAL;
    return strobe_timeout_level[selector];
}
static int lv_protection[] = {
    2900,
    3000,
    3100,
    3200,
    3300,
    3400,
    3500,
    3600,
};


static struct flashlight_properties rt5033_fled_props =
{
	.type = FLASHLIGHT_TYPE_LED,
    .torch_brightness = 0,
    .torch_max_brightness = ARRAY_SIZE(torch_current) - 1,
    .strobe_brightness = 0,
    .strobe_max_brightness = 31 - 1,
    .strobe_delay = 2,
    .strobe_timeout = 64,
    .alias_name = "rt5033-fled",
};

/* Return value : -EINVAL => selector parameter is out of range, otherwise voltage in mV*/
static int rt5033_fled_lv_protection_list(struct rt_fled_info *info, int selector)
{
    if (selector < 0 || selector >= ARRAY_SIZE(lv_protection))
        return -EINVAL;
    return lv_protection[selector];
}
/* Return value : -EINVAL => selector parameter is out of range, otherwise time in ms*/
static int rt5033_fled_strobe_timeout_list(struct rt_fled_info *info, int selector)
{
    if (selector <0 || selector>= 37)
        return -EINVAL;
    return (64 + selector*32);
}

static int rt5033_fled_set_torch_current_sel(struct rt_fled_info *fled_info,
                                              int selector)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    RT5033_FLED_INFO("Set torch current to %d\n", selector);
    if (selector < 0 || selector >  info->
                base.flashlight_dev->props.torch_max_brightness)
        return -EINVAL;
    rc = rt5033_assign_bits(info->i2c_client, RT5033_FLED_CONTROL1,
                            0xf0, selector<<4);
    if (rc == 0)
        info->base.flashlight_dev->props.torch_brightness = selector;
    return rc;
}
static int rt5033_fled_set_strobe_current_sel(struct rt_fled_info *fled_info,
                                               int selector)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    RT5033_FLED_INFO("Set strobe current to %d\n", selector);
    if (selector < 0 || selector >  info->
                base.flashlight_dev->props.strobe_max_brightness)
        return -EINVAL;
    rc = rt5033_assign_bits(info->i2c_client, RT5033_FLED_STROBE_CONTROL1,
                            0x1f, selector);
    if (rc == 0)
        info->base.flashlight_dev->props.strobe_brightness = selector;
    return 0;
}
static int rt5033_fled_set_timeout_level_sel(struct rt_fled_info *fled_info,
                                               int selector)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    RT5033_FLED_INFO("Set timeout level to %d\n", selector);
    if (selector < 0 || selector >=  ARRAY_SIZE(strobe_timeout_level))
        return -EINVAL;
    return rt5033_assign_bits(info->i2c_client, RT5033_FLED_STROBE_CONTROL1,
                            0xe0, selector << 5);

}


static int rt5033_fled_set_lv_protection_sel(struct rt_fled_info *fled_info,
                                            int selector)
{
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    RT5033_FLED_INFO("Set lv protection to %d\n", selector);
    if (selector <0 || selector >=  ARRAY_SIZE(lv_protection))
        return -EINVAL;

    return rt5033_assign_bits(info->i2c_client, RT5033_FLED_CONTROL1,
                            0x07, selector);
}
static int rt5033_fled_set_strobe_timeout_sel(struct rt_fled_info *fled_info,
                                               int selector)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    RT5033_FLED_INFO("Set strobe timeout to %d\n", selector);
    if (selector < 0 || selector >=  37)
        return -EINVAL;
    rc = rt5033_assign_bits(info->i2c_client, RT5033_FLED_STROBE_CONTROL2,
                            0x3f, selector);
    if (rc == 0)
        fled_info->flashlight_dev->props.strobe_timeout =
                rt5033_fled_strobe_timeout_list(fled_info, selector);
    return rc;
}

static int rt5033_fled_get_torch_current_sel(struct rt_fled_info *fled_info)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rc = rt5033_reg_read(info->i2c_client, RT5033_FLED_CONTROL1);
    if (rc<0)
        return rc;
    return (rc & 0xf0) >> 4;
}

static int rt5033_fled_get_strobe_current_sel(struct rt_fled_info *fled_info)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rc = rt5033_reg_read(info->i2c_client, RT5033_FLED_STROBE_CONTROL1);
    if (rc<0)
        return rc;
    return rc & 0x1f;
}

static int rt5033_fled_get_timeout_level_sel(struct rt_fled_info *fled_info)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rc = rt5033_reg_read(info->i2c_client, RT5033_FLED_STROBE_CONTROL1);
    if (rc<0)
        return rc;
    return (rc & 0xe0) >> 5;
}

static int rt5033_fled_get_lv_protection_sel(struct rt_fled_info *fled_info)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rc = rt5033_reg_read(info->i2c_client, RT5033_FLED_CONTROL1);
    if (rc<0)
        return rc;
    return rc & 0x07;
}

static int rt5033_fled_get_strobe_timeout_sel(struct rt_fled_info *fled_info)
{
    int rc;
    rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
    rc = rt5033_reg_read(info->i2c_client, RT5033_FLED_STROBE_CONTROL2);
    if (rc<0)
        return rc;
    return rc & 0x3f;
}

static void rt5033_fled_shutdown(struct rt_fled_info *info)
{
    flashlight_set_mode(info->flashlight_dev, FLASHLIGHT_MODE_OFF);
    return;
}

static struct rt_fled_hal rt5033_fled_hal = {
    .fled_init = rt5033_fled_init,
    .fled_suspend = rt5033_fled_suspend,
    .fled_resume = rt5033_fled_resume,
    .fled_set_mode = rt5033_fled_set_mode,
    .fled_get_mode = rt5033_fled_get_mode,
    .fled_strobe = rt5033_fled_strobe,
    .fled_troch_current_list = rt5033_fled_troch_current_list,
    .fled_strobe_current_list = rt5033_fled_strobe_current_list,
    .fled_timeout_level_list = rt5033_fled_timeout_level_list,
    .fled_lv_protection_list = rt5033_fled_lv_protection_list,
    .fled_strobe_timeout_list = rt5033_fled_strobe_timeout_list,
    /* method to set */
    .fled_set_torch_current_sel = rt5033_fled_set_torch_current_sel,
    .fled_set_strobe_current_sel = rt5033_fled_set_strobe_current_sel,
    .fled_set_timeout_level_sel = rt5033_fled_set_timeout_level_sel,

    .fled_set_lv_protection_sel = rt5033_fled_set_lv_protection_sel,
    .fled_set_strobe_timeout_sel = rt5033_fled_set_strobe_timeout_sel,

    /* method to get */
    .fled_get_torch_current_sel = rt5033_fled_get_torch_current_sel,
    .fled_get_strobe_current_sel = rt5033_fled_get_strobe_current_sel,
    .fled_get_timeout_level_sel = rt5033_fled_get_timeout_level_sel,
    .fled_get_lv_protection_sel = rt5033_fled_get_lv_protection_sel,
    .fled_get_strobe_timeout_sel = rt5033_fled_get_strobe_timeout_sel,
    /* PM shutdown, optional */
    .fled_shutdown = rt5033_fled_shutdown,

};


const static rt5033_fled_platform_data_t rt5033_default_fled_pdata = {
    .fled1_en = 1,
    .fled2_en = 1,
    .fled_mid_track_alive = 0,
    .fled_mid_auto_track_en = 1,
    .fled_timeout_current_level = RT5033_TIMEOUT_LEVEL(50),
    .fled_strobe_current = RT5033_STROBE_CURRENT(750),
    .fled_strobe_timeout = RT5033_STROBE_TIMEOUT(544),
    .fled_torch_current = RT5033_TORCH_CURRENT(38),
    .fled_lv_protection = RT5033_LV_PROTECTION(3200),
    .fled_mid_level = RT5033_MID_REGULATION_LEVEL(4800),
};


#define FLAG_HIGH           (0x01)
#define FLAG_LOW            (0x02)
#define FLAG_LOW_TO_HIGH    (0x04)
#define FLAG_HIGH_TO_LOW    (0x08)
#define FLAG_CHANGED        (FLAG_LOW_TO_HIGH|FLAG_HIGH_TO_LOW)

struct rt5033_fled_irq_handler
{
    const char *name;
    int irq_index;
    irqreturn_t (*handler)(int irq, void *data);
};
#if EN_FLED_IRQ
static irqreturn_t rt5033_vf_l_irq_handler(int irq, void *data)
{
    rt5033_fled_info_t *info = data;
    RT5033_FLED_WARN("LED VF Low\n");
    BUG_ON(info == NULL);
    return IRQ_HANDLED;
}

static irqreturn_t rt5033_ledcs2_short_irq_handler(int irq, void *data)
{
    rt5033_fled_info_t *info = data;
    RT5033_FLED_WARN("LEDCS2 short\n");
    BUG_ON(info == NULL);
    return IRQ_HANDLED;
}

static irqreturn_t rt5033_ledcs1_short_irq_handler(int irq, void *data)
{
    rt5033_fled_info_t *info = data;
    RT5033_FLED_WARN("LEDCS1 short\n");
    BUG_ON(info == NULL);
    return IRQ_HANDLED;
}
#endif //#EN_FLED_IRQ

const struct rt5033_fled_irq_handler rt5033_fled_irq_handlers[] = {
#if EN_FLED_IRQ
    {
        .name = "VF_L",
        .handler = rt5033_vf_l_irq_handler,
        .irq_index = RT5033_VF_L_IRQ,

    },
    {
        .name = "LEDCS2_SHORT",
        .handler = rt5033_ledcs2_short_irq_handler,
        .irq_index = RT5033_LEDCS2_SHORT_IRQ,
    },
    {
        .name = "LEDCS1_SHORT",
        .handler = rt5033_ledcs1_short_irq_handler,
        .irq_index = RT5033_LEDCS1_SHORT_IRQ,
    },
#endif //EN_FLED_IRQ
};

static int register_irq(struct platform_device *pdev,
                rt5033_fled_info_t *info)
{
    int irq;
    int i, j;
    int ret;
    const struct rt5033_fled_irq_handler *irq_handler = rt5033_fled_irq_handlers;
    const char *irq_name;
    for (i = 0; i < ARRAY_SIZE(rt5033_fled_irq_handlers); i++) {
        irq_name = rt5033_get_irq_name_by_index(irq_handler[i].irq_index);
        irq = platform_get_irq_byname(pdev, irq_name);
        ret = request_threaded_irq(irq, NULL, irq_handler[i].handler,
                       IRQF_ONESHOT, irq_name, info);
        if (ret < 0) {
            RT5033_FLED_ERR("Failed to request IRQ (%s): #%d: %d\n", irq_name, irq, ret);
            goto err_irq;
        }
    }

    return 0;
err_irq:
    for (j = 0; j < i; j++) {
        irq_name = rt5033_get_irq_name_by_index(irq_handler[j].irq_index);
        irq = platform_get_irq_byname(pdev, irq_name);
        free_irq(irq, info);
    }
    return ret;
}

static void unregister_irq(struct platform_device *pdev,
                rt5033_fled_info_t *info)
{
    int irq;
    int i;
    const char *irq_name;
    const struct rt5033_fled_irq_handler *irq_handler = rt5033_fled_irq_handlers;
    for (i = 0; i < ARRAY_SIZE(rt5033_fled_irq_handlers); i++) {
        irq_name = rt5033_get_irq_name_by_index(irq_handler[i].irq_index);
        irq = platform_get_irq_byname(pdev, irq_name);
        free_irq(irq, info);
    }
}

static __devinit int rt5033_fled_probe(struct platform_device *pdev)
{
    int ret;
	struct rt5033_mfd_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct rt5033_mfd_platform_data *mfd_pdata = chip->dev->platform_data;
	const struct rt5033_fled_platform_data* pdata;
    rt5033_fled_info_t *fled_info;
    RT5033_FLED_INFO("Richtek RT5033 FlashLED driver probing...\n");
    BUG_ON(mfd_pdata == NULL);
    if (mfd_pdata->fled_platform_data)
        pdata = mfd_pdata->fled_platform_data;
    else
        pdata = &rt5033_default_fled_pdata;
    fled_info = kzalloc(sizeof(*fled_info), GFP_KERNEL);
    if (!fled_info) {
        ret = -ENOMEM;
        goto err_fled_nomem;
    }
    mutex_init(&fled_info->io_lock);
    fled_info->i2c_client = chip->i2c_client;
    fled_info->base.init_props = &rt5033_fled_props;
    fled_info->base.hal = &rt5033_fled_hal;
    fled_info->pdata = pdata;
    fled_info->chip = chip;
    chip->fled_info = fled_info;
    platform_set_drvdata(pdev, fled_info);

    rt_fled_pdev.dev.parent = &(pdev->dev);
    ret = platform_device_register(&rt_fled_pdev);
    if (ret<0)
        goto err_register_pdev;
    ret = register_irq(pdev, fled_info);
    if (ret < 0) {
        RT5033_FLED_ERR("Error : can't register irq\n");
        goto err_register_irq;

    }
    return 0;
err_register_irq:
err_register_pdev:
    kfree(fled_info);
err_fled_nomem:
    return ret;
}

static __devexit int rt5033_fled_remove(struct platform_device *pdev)
{
    struct rt5033_fled_info *fled_info;
    RT5033_FLED_INFO("Richtek RT5033 FlashLED driver removing...\n");
    fled_info = platform_get_drvdata(pdev);
    unregister_irq(pdev, fled_info);
    platform_device_unregister(&rt_fled_pdev);
    mutex_destroy(&fled_info->io_lock);
    kfree(fled_info);
	return 0;
}

static struct platform_driver rt5033_fled_driver = {
	.probe	= rt5033_fled_probe,
	.remove	= __devexit_p(rt5033_fled_remove),
	.driver	= {
		.name	= "rt5033-fled",
	},
};


static int __init rt5033_fled_module_init(void)
{
	return platform_driver_register(&rt5033_fled_driver);
}

static void __exit rt5033_fled_module_exit(void)
{
	platform_driver_unregister(&rt5033_fled_driver);
}

device_initcall(rt5033_fled_module_init);
module_exit(rt5033_fled_module_exit);

MODULE_DESCRIPTION("Richtek RT5033 FlashLED Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_VERSION(RT5033_DRV_VER);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rt5033-flashLED");
