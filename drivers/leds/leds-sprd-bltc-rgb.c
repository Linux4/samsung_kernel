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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <soc/sprd/sci_glb_regs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/leds-sprd-bltc-rgb.h>

#ifdef SPRD_BLTC_RGB_DBG
#define ENTER printk(KERN_INFO "[SPRD_BLTC_RGB_DBG] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[SPRD_BLTC_RGB_DBG] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_BLTC_RGB_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_BLTC_RGB_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_BLTC_RGB_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_BLTC_RGB_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_BLTC_RGB_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_BLTC_RGB_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

//#define SPRD_BLTC_BASE	(SPRD_ADISLAVE_BASE+ 0x3C0)

#define BLTC_CTRL				0x0000
#define BLTC_R_PRESCL			0x0004
#define BLTC_G_PRESCL			0x0014
#define BLTC_B_PRESCL			0x0024
#define BLTC_PRESCL_OFFSET		0x0004
#define BLTC_DUTY_OFFSET		0x0004
#define BLTC_CURVE0_OFFSET		0x0008
#define BLTC_CURVE1_OFFSET		0x000C

#define BLTC_STS				0x0034
#if 0
#define SPRD_SLP_RGB_PD_EN	(SPRD_ADISLAVE_BASE + 0x800 + 0xA0)
#define RGB_V_SHIFT		(8)
#define RGB_V_MSK		(0x1F<<8)

#define SPRD_ARM_MODULE_EN	(SPRD_ADISLAVE_BASE + 0x800)
#define SPRD_CLK_EN			(SPRD_ARM_MODULE_EN + 0x08)
#define SPRD_ARM_RST		(SPRD_ARM_MODULE_EN + 0x0c)
#endif
#define PWM_MOD_COUNTER 0xFF

struct sprd_leds_bltc_rgb {
        struct platform_device *dev;
        struct mutex mutex;
        struct work_struct work;
        struct delayed_work work_bl;
        struct workqueue_struct *work_bl_q;
        spinlock_t value_lock;
        enum led_brightness value;
        struct led_classdev cdev;
        u16 leds_flag;
        u16 enable;
        unsigned long bltc_addr,bltc_addr_rf,bltc_addr_hl,bltc_addr_onoff;
        unsigned long sprd_bltc_base_addr,sprd_arm_module_en_addr;
        u8 rising_time;
        u8 falling_time;
        u8 high_time;
        u8 low_time;
        u16 on_off;
        int suspend;
};

enum sprd_leds_type {
        SPRD_LED_TYPE_R = 0,
        SPRD_LED_TYPE_G,
        SPRD_LED_TYPE_B,
        SPRD_LED_TYPE_R_BL,
        SPRD_LED_TYPE_G_BL,
        SPRD_LED_TYPE_B_BL,
        SPRD_LED_TYPE_TOTAL
};

enum sprd_leds_bl_param {
        R_TIME = 1,
        F_TIME,
        H_TIME,
        L_TIME,
        ONOFF
};

static char *sprd_leds_rgb_name[SPRD_LED_TYPE_TOTAL] = {
        "red",
        "green",
        "blue",
        "red_bl",
        "green_bl",
        "blue_bl",
};

#define to_sprd_bltc_rgb(led_cdev) \
	container_of(led_cdev, struct sprd_leds_bltc_rgb, cdev)

static inline uint32_t sprd_leds_bltc_rgb_read(unsigned long reg)
{
        return sci_adi_read(reg);
}

static void sprd_leds_bltc_rgb_set_brightness(struct sprd_leds_bltc_rgb *brgb)
{
        unsigned long brightness = brgb->value;
        unsigned long pwm_duty;

        pwm_duty = brightness;
        if(pwm_duty > 255)
                pwm_duty = 255;
        sci_adi_write(brgb->bltc_addr, (pwm_duty<<8)|PWM_MOD_COUNTER,0xffff);
        PRINT_INFO("reg:0x%1LX set_val:0x%08X  brightness:%ld  brightness_level:%ld(0~15)\n", \
                   brgb->bltc_addr, sprd_leds_bltc_rgb_read(brgb->bltc_addr),brightness, pwm_duty);
}

static void sprd_bltc_rgb_init(struct sprd_leds_bltc_rgb *brgb)
{
        sci_adi_set(brgb->sprd_arm_module_en_addr, (0x1<<10));//ARM_MODULE_EN-enable pclk
        sci_adi_set(brgb->sprd_arm_module_en_addr + 0x08, (0x1<<8));//RTC_CLK_EN-enable rtc
        sci_adi_clr(brgb->sprd_arm_module_en_addr + 0xec, (0x1<<0));//SW POWERDOWN DISABLE
        sci_adi_clr(brgb->sprd_arm_module_en_addr + 0xec, (0x1f<<4));//CURRENT CONTROL DEFAULT
	sci_adi_set(brgb->sprd_arm_module_en_addr + 0xf0, (0x1<<8));//WHTLED_SERIES_EN=1
	sci_adi_set(brgb->sprd_arm_module_en_addr + 0xf0, (0x1<<0));//WHTLED POWERDOWN ENABLE
}
static void sprd_leds_bltc_rgb_enable(struct sprd_leds_bltc_rgb *brgb)
{
        sprd_bltc_rgb_init(brgb);

        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R]) == 0) {
                sci_adi_set(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<0)|(0x1<<1));
                brgb->bltc_addr = brgb->sprd_bltc_base_addr + BLTC_R_PRESCL + BLTC_DUTY_OFFSET;
                sprd_leds_bltc_rgb_set_brightness(brgb);
        }
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G]) == 0) {
                sci_adi_set(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<4)|(0x1<<5));
                brgb->bltc_addr = brgb->sprd_bltc_base_addr + BLTC_G_PRESCL + BLTC_DUTY_OFFSET;
                sprd_leds_bltc_rgb_set_brightness(brgb);
        }
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B]) == 0) {
                sci_adi_set(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<8)|(0x1<<9));
                brgb->bltc_addr = brgb->sprd_bltc_base_addr + BLTC_B_PRESCL + BLTC_DUTY_OFFSET;
                sprd_leds_bltc_rgb_set_brightness(brgb);
        }
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R_BL]) == 0 || \
            strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G_BL]) == 0 || \
            strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B_BL]) == 0) {
                PRINT_INFO("LEDFLAG:%1d\n",brgb->leds_flag);
                PRINT_INFO("VALUE:%1d\n",brgb->value);

                if(brgb->leds_flag == R_TIME || brgb->leds_flag == F_TIME || brgb->leds_flag == H_TIME || \
                    brgb->leds_flag == L_TIME) {
                        if(brgb->leds_flag == R_TIME)
                                brgb->rising_time = brgb->value;
                        if(brgb->leds_flag == F_TIME)
                                brgb->falling_time = brgb->value;
                        if(brgb->leds_flag == H_TIME)
                                brgb->high_time = brgb->value;
                        if(brgb->leds_flag == L_TIME)
                                brgb->low_time = brgb->value;

                        brgb->leds_flag = 0;
                }

                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R_BL]) == 0) {
                        brgb->bltc_addr_rf = brgb->sprd_bltc_base_addr + BLTC_R_PRESCL + BLTC_CURVE0_OFFSET;
                        brgb->bltc_addr_hl = brgb->sprd_bltc_base_addr + BLTC_R_PRESCL + BLTC_CURVE1_OFFSET;
                        brgb->bltc_addr_onoff = brgb->sprd_bltc_base_addr + BLTC_CTRL;
                        if(brgb->leds_flag == ONOFF)
                                brgb->on_off = (brgb->value << 0);
						sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<0)|(0x1<<1));
                }
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G_BL]) == 0) {
                        brgb->bltc_addr_rf = brgb->sprd_bltc_base_addr + BLTC_G_PRESCL + BLTC_CURVE0_OFFSET;
                        brgb->bltc_addr_hl = brgb->sprd_bltc_base_addr + BLTC_G_PRESCL + BLTC_CURVE1_OFFSET;
                        brgb->bltc_addr_onoff = brgb->sprd_bltc_base_addr + BLTC_CTRL;
                        if(brgb->leds_flag == ONOFF)
                                brgb->on_off = (brgb->value << 4);
						sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<4)|(0x1<<5));
                }
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B_BL]) == 0) {
                        brgb->bltc_addr_rf = brgb->sprd_bltc_base_addr + BLTC_B_PRESCL + BLTC_CURVE0_OFFSET;
                        brgb->bltc_addr_hl = brgb->sprd_bltc_base_addr + BLTC_B_PRESCL + BLTC_CURVE1_OFFSET;
                        brgb->bltc_addr_onoff = brgb->sprd_bltc_base_addr + BLTC_CTRL;
                        if(brgb->leds_flag == ONOFF)
                                brgb->on_off = (brgb->value << 8);
						sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<8)|(0x1<<9));
                }

                sci_adi_write(brgb->bltc_addr_rf,sprd_leds_bltc_rgb_read(brgb->bltc_addr_rf)|(brgb->falling_time<<8)|brgb->rising_time,0xffff);
                sci_adi_write(brgb->bltc_addr_hl,sprd_leds_bltc_rgb_read(brgb->bltc_addr_hl)|(brgb->low_time<<8)|brgb->high_time,0xffff);
                sci_adi_set(brgb->bltc_addr_onoff,brgb->on_off);
        }

        PRINT_INFO("sprd_leds_bltc_rgb_enable\n");
        brgb->enable = 1;
}

static void sprd_leds_bltc_rgb_disable(struct sprd_leds_bltc_rgb *brgb)
{
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R]) == 0 || \
            strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R_BL]) == 0) {
                sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<0));
        }
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G]) == 0 || \
            strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G_BL]) == 0) {
                sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<4));
        }
        if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B]) == 0 || \
            strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B_BL]) == 0) {
                sci_adi_clr(brgb->sprd_bltc_base_addr + BLTC_CTRL, (0x1<<8));
        }
        PRINT_INFO("sprd_leds_bltc_rgb_disable\n");
        PRINT_INFO("reg:0x%08X set_val:0x%08X brightness:%1d\n", brgb->sprd_arm_module_en_addr + 0xa0, \
                   sprd_leds_bltc_rgb_read(brgb->sprd_arm_module_en_addr + 0xa0),brgb->value);
}

static void sprd_leds_rgb_work(struct work_struct *work)
{
        struct sprd_leds_bltc_rgb *brgb;
        unsigned long flags;

        brgb = container_of(work, struct sprd_leds_bltc_rgb, work);
        mutex_lock(&brgb->mutex);
        spin_lock_irqsave(&brgb->value_lock, flags);
        if (brgb->value == LED_OFF) {
                spin_unlock_irqrestore(&brgb->value_lock, flags);
                sprd_leds_bltc_rgb_set_brightness(brgb);
                goto out;
        }
        spin_unlock_irqrestore(&brgb->value_lock, flags);
        sprd_leds_bltc_rgb_enable(brgb);
        PRINT_INFO("sprd_leds_bltc_rgb_work_for rgb!\n");

out:
        mutex_unlock(&brgb->mutex);
}

static void sprd_leds_bltc_work(struct work_struct *work)
{
        struct sprd_leds_bltc_rgb *brgb;
        unsigned long flags;

        brgb = container_of(work, struct sprd_leds_bltc_rgb, work_bl.work);
        mutex_lock(&brgb->mutex);
#if 1
        spin_lock_irqsave(&brgb->value_lock, flags);
        if (brgb->value == LED_OFF) {
                spin_unlock_irqrestore(&brgb->value_lock, flags);
				if(brgb->leds_flag == ONOFF)
                sprd_leds_bltc_rgb_disable(brgb);
                goto out;
        }
        spin_unlock_irqrestore(&brgb->value_lock, flags);
#endif
        sprd_leds_bltc_rgb_enable(brgb);
        PRINT_INFO("sprd_leds_bltc_rgb_work for bltc!\n");

out:
        mutex_unlock(&brgb->mutex);
}

static void sprd_leds_bltc_rgb_set(struct led_classdev *bltc_rgb_cdev,enum led_brightness value)
{
        struct sprd_leds_bltc_rgb *brgb;
        unsigned long flags;

                brgb = to_sprd_bltc_rgb(bltc_rgb_cdev);
                spin_lock_irqsave(&brgb->value_lock, flags);
                brgb->leds_flag = bltc_rgb_cdev->flags;
                brgb->value = value;
                spin_unlock_irqrestore(&brgb->value_lock, flags);

                if(1 == brgb->suspend) {
                        PRINT_WARN("Do NOT change brightness in suspend mode\n");
                        return;
                }
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R]) == 0 || \
                    strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G]) == 0 || \
                    strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B]) == 0)
                        schedule_work(&brgb->work);
                else
                        queue_delayed_work(brgb->work_bl_q, &brgb->work_bl, msecs_to_jiffies(500));
}

#if 0
static int sprd_leds_bltc_rgb_get(struct led_classdev *bltc_rgb_cdev)
{
        struct sprd_leds_bltc_rgb *brgb;
        char i;
        for(i = 0; i < 3; i++) {
                brgb = to_sprd_bltc_rgb(bltc_rgb_cdev);
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_R]) == 0)
                        return brgb->value;
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_G]) == 0)
                        return brgb->value;
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[SPRD_LED_TYPE_B]) == 0)
                        return brgb->value;
                ++brgb;
        }
}
#endif
static void sprd_leds_bltc_rgb_shutdown(struct platform_device  *dev)
{
        struct sprd_leds_bltc_rgb *brgb = platform_get_drvdata(dev);

        mutex_lock(&brgb->mutex);
        sprd_leds_bltc_rgb_disable(brgb);
        mutex_unlock(&brgb->mutex);
}

static u16 r_value,f_value,h_value,l_value,onoff_value;

static ssize_t show_rising_time(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", r_value);
}

static ssize_t store_rising_time(struct device *dev,
                                 struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state;
        ssize_t ret = -EINVAL;

        ret = kstrtoul(buf, 10, &state);
        PRINT_INFO("r_state_value:%1ld\n",state);
        r_value = state;
        led_cdev->flags = R_TIME;
        sprd_leds_bltc_rgb_set(led_cdev,state);
        return size;
}

static ssize_t show_falling_time(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", f_value);
}

static ssize_t store_falling_time(struct device *dev,
                                  struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state;
        ssize_t ret = -EINVAL;

        ret = kstrtoul(buf, 10, &state);
        PRINT_INFO("f_state_value:%1ld\n",state);
        f_value = state;
        led_cdev->flags = F_TIME;
        sprd_leds_bltc_rgb_set(led_cdev,state);
        return size;
}

static ssize_t show_high_time(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", h_value);
}

static ssize_t store_high_time(struct device *dev,
                               struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state;
        ssize_t ret = -EINVAL;

        ret = kstrtoul(buf, 10, &state);
        PRINT_INFO("h_state_value:%1ld\n",state);
        h_value = state;
        led_cdev->flags = H_TIME;
        sprd_leds_bltc_rgb_set(led_cdev,state);
        return size;
}

static ssize_t show_low_time(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", l_value);
}

static ssize_t store_low_time(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state;
        ssize_t ret = -EINVAL;

        ret = kstrtoul(buf, 10, &state);
        PRINT_INFO("l_state_value:%1ld\n",state);
        l_value = state;
        led_cdev->flags = L_TIME;
        sprd_leds_bltc_rgb_set(led_cdev,state);
        return size;
}

static ssize_t show_on_off(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", onoff_value);
}

static ssize_t store_on_off(struct device *dev,
                            struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state;
        ssize_t ret = -EINVAL;

        ret = kstrtoul(buf, 10, &state);
        PRINT_INFO("onoff_state_value:%1ld\n",state);
        onoff_value = state;
        led_cdev->flags = ONOFF;
        sprd_leds_bltc_rgb_set(led_cdev,state);
        return size;
}

static DEVICE_ATTR(rising_time,0664,show_rising_time,store_rising_time);
static DEVICE_ATTR(falling_time,0664,show_falling_time,store_falling_time);
static DEVICE_ATTR(high_time,0664,show_high_time,store_high_time);
static DEVICE_ATTR(low_time,0664,show_low_time,store_low_time);
static DEVICE_ATTR(on_off,0664,show_on_off,store_on_off);

static int sprd_leds_bltc_rgb_probe(struct platform_device *dev)
{
        struct sprd_leds_bltc_rgb *brgb;
        int ret,i;

#ifdef CONFIG_OF
/*
        struct device_node *np = dev->dev.of_node;
        struct resource res;
        unsigned long base_addr0,base_addr1;

        ret = of_address_to_resource(np, 0, &res);
        if(ret) {
                PRINT_ERR("get dts data failed!\n");
                return -ENODEV;
        }
        base_addr0 = (unsigned long)ioremap_nocache(res.start,resource_size(&res));

        ret = of_address_to_resource(np, 1, &res);
        if(ret) {
                PRINT_ERR("get dts data1 failed!\n");
                return -ENODEV;
        }
        base_addr1 = (unsigned long)ioremap_nocache(res.start,resource_size(&res));
*/
//        PRINT_INFO("base_addr0:0x%1lx\n",base_addr0);	/***0x40038440***/
//        PRINT_INFO("base_addr1:0x%1lx\n",base_addr1);	/***0x40038800***/
#else
        struct sprd_leds_bltc_rgb_platform_data *pdata = NULL;

        pdata = dev->dev.platform_data;
        if (!pdata) {
                PRINT_ERR("No kpled_platform data!\n");
                return -ENODEV;
        }
#endif

        for (i = 0; i < SPRD_LED_TYPE_TOTAL; i++) {
                brgb = kzalloc(sizeof(struct sprd_leds_bltc_rgb), GFP_KERNEL);

                if (brgb == NULL) {
                        dev_err(&dev->dev, "No memory for bltc_device\n");
                        ret = -ENOMEM;
                        goto err;
                }

                brgb->cdev.brightness_set = sprd_leds_bltc_rgb_set;
                brgb->cdev.name = sprd_leds_rgb_name[i];
                brgb->cdev.brightness_get = NULL;
                brgb->enable = 0;
#if CONFIG_OF
                brgb->sprd_bltc_base_addr = SPRD_ADI_BASE + 0x8440;		/***0x40038440***/
                brgb->sprd_arm_module_en_addr = SPRD_ADI_BASE + 0x8800;	/***0x40038800***/
#else
                brgb->sprd_bltc_base_addr = pdata->sprd_base_addr + 0x440;		/***0x40038440***/
                brgb->sprd_arm_module_en_addr = pdata->sprd_base_addr + 0x800;	/***0x40038800***/
#endif
                spin_lock_init(&brgb->value_lock);
                mutex_init(&brgb->mutex);
                INIT_WORK(&brgb->work, sprd_leds_rgb_work);
                INIT_DELAYED_WORK(&brgb->work_bl, sprd_leds_bltc_work);
                brgb->work_bl_q = create_singlethread_workqueue("leds_bltc");
                if(brgb->work_bl_q == NULL) {
                        PRINT_ERR("create_singlethread_workqueue for leds_bltc failed!\n");
                        goto failed_to_create_singlethread_workqueue_for_leds_bltc;
                }

                brgb->value = LED_OFF;
                platform_set_drvdata(dev, brgb);

                ret = led_classdev_register(&dev->dev, &brgb->cdev);
                if(strcmp(brgb->cdev.name,sprd_leds_rgb_name[i]) == 0) {
                        ret = device_create_file(brgb->cdev.dev,&dev_attr_rising_time);
                        ret = device_create_file(brgb->cdev.dev,&dev_attr_falling_time);
                        ret = device_create_file(brgb->cdev.dev,&dev_attr_high_time);
                        ret = device_create_file(brgb->cdev.dev,&dev_attr_low_time);
                        ret = device_create_file(brgb->cdev.dev,&dev_attr_on_off);
                }
                if (ret < 0) {
                        goto err;
                }

                brgb->value = 15;//set default brightness
                brgb->suspend = 0;
                ++brgb;
        }
	sci_adi_clr(brgb->sprd_arm_module_en_addr + 0x0c,(0x1<<13));//BLTC SOFT RESET
        return 0;
err:
        if (i) {
                for (i = i-1; i >=0; i--) {
                        if (!brgb)
                                continue;
                        led_classdev_unregister(&brgb->cdev);
                        kfree(brgb);
                        brgb = NULL;
                        --brgb;
                }
        }
failed_to_create_singlethread_workqueue_for_leds_bltc:
        cancel_delayed_work_sync(&brgb->work_bl);
        destroy_workqueue(brgb->work_bl_q);

        return ret;
}

static int sprd_leds_bltc_rgb_remove(struct platform_device *dev)
{
        struct sprd_leds_bltc_rgb *brgb = platform_get_drvdata(dev);

        led_classdev_unregister(&brgb->cdev);
        flush_scheduled_work();
        brgb->value = LED_OFF;
        brgb->enable = 1;
        sprd_leds_bltc_rgb_disable(brgb);
        kfree(brgb);

        return 0;
}

static const struct of_device_id sprd_leds_bltc_rgb_of_match[] = {
        { .compatible = "sprd,sprd-leds-bltc-rgb", },
        { }
};

static struct platform_driver sprd_leds_bltc_rgb_driver = {
        .driver = {
                .name  = "sprd-leds-bltc-rgb",
                .owner = THIS_MODULE,
                .of_match_table = sprd_leds_bltc_rgb_of_match,
        },
        .probe    = sprd_leds_bltc_rgb_probe,
        .remove   = sprd_leds_bltc_rgb_remove,
        .shutdown = sprd_leds_bltc_rgb_shutdown,
};

static int __init sprd_leds_bltc_rgb_init(void)
{
        PRINT_INFO("Locate in sprd_leds_bltc_rgb_init!\n");

        return platform_driver_register(&sprd_leds_bltc_rgb_driver);
}

static void sprd_leds_bltc_rgb_exit(void)
{
        platform_driver_unregister(&sprd_leds_bltc_rgb_driver);
}

module_init(sprd_leds_bltc_rgb_init);
module_exit(sprd_leds_bltc_rgb_exit);

MODULE_AUTHOR("ya huang <ya.huang@spreadtrum.com>");
MODULE_DESCRIPTION("Sprd leds bltc rgb driver for 7731ea");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd_leds_bltc_rgb_7731ea");




