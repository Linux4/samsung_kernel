/*
 *  kernel/driver/char/modem_interface/modem_gpio_drv.c
 *
 *  Generic modem interface gpio handling.
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <mach/modem_interface.h>
#include <mach/globalregs.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"

#define GPIO_INVALID 0xFFFFFFFF

extern void free_irq(unsigned int irq, void *dev_id);
extern void modem_intf_send_GPIO_message(int gpio_no,int status,int index);
extern void modem_intf_send_ctrl_gpio_message(enum MSG_CTRL_GPIO_TYPE gpio_type, int gpio_value);
extern void on_modem_poweron(void);
static unsigned int cp_to_ap_rdy_irq;
static unsigned int cp_alive_gpio_irq;
static unsigned int cp_crash_gpio_irq;
static unsigned int cp_watchdog_gpio_irq;
static unsigned int modem_power_gpio;
static unsigned int modem_reset_gpio;
static int    cp_to_ap_rdy ;
static int    cp_alive_gpio;
static int    cp_crash_gpio;
static int    cp_watchdog_gpio;


int get_alive_status(void)
{
        int status = gpio_get_value(cp_alive_gpio);
        if(status != 0)
                return 1;
        return 0;
}
int get_assert_status(void)
{
        int status = gpio_get_value(cp_crash_gpio);
        if(status != 0)
                return 1;
        return 0;
}
static irqreturn_t cp_to_ap_rdy_handle(int irq, void *handle)
{
        int status ;

        status = gpio_get_value(cp_to_ap_rdy);
        pr_debug("modem_boot_irq: %d \n",status);
        if(status == 0)
                modem_intf_send_GPIO_message(cp_to_ap_rdy,status,0);
        else
                modem_intf_send_GPIO_message(cp_to_ap_rdy,1,0);
        return IRQ_HANDLED;
}

int modem_intf_get_watchdog_gpio_value()
{

        return gpio_get_value(cp_watchdog_gpio);
}
static irqreturn_t cp_watchdog_gpio_handle(int irq, void * handle)
{
	int wdg_reset_status;

	wdg_reset_status = gpio_get_value(cp_watchdog_gpio);
	printk("cp_watchdog_gpio: %d \n",wdg_reset_status);

        if(wdg_reset_status == 0) {
            irq_set_irq_type(cp_watchdog_gpio_irq,  IRQ_TYPE_LEVEL_HIGH);
        } else {
            irq_set_irq_type(cp_watchdog_gpio_irq,  IRQ_TYPE_LEVEL_LOW);
        }

	modem_intf_send_ctrl_gpio_message(GPIO_WATCHDOG, wdg_reset_status);
	return IRQ_HANDLED;
}

static irqreturn_t cp_alive_gpio_handle(int irq, void *handle)
{
        int alive_status;

        alive_status = gpio_get_value(cp_alive_gpio);
        printk("cp_alive_gpio: %d \n",alive_status);

        if(alive_status == 0) {
                irq_set_irq_type(cp_alive_gpio_irq,  IRQ_TYPE_LEVEL_HIGH);
        } else {
                irq_set_irq_type(cp_alive_gpio_irq,  IRQ_TYPE_LEVEL_LOW);
        }

        modem_intf_send_ctrl_gpio_message(GPIO_ALIVE, alive_status);
        return IRQ_HANDLED;
}


static irqreturn_t cp_crash_gpio_handle(int irq, void *handle)
{
        int assert_status;

        assert_status = gpio_get_value(cp_crash_gpio);
        printk("cp_crash_gpio: %d \n",assert_status);

        if(assert_status == 0) {
                irq_set_irq_type(cp_crash_gpio_irq,  IRQ_TYPE_LEVEL_HIGH);
        } else {
                irq_set_irq_type(cp_crash_gpio_irq,  IRQ_TYPE_LEVEL_LOW);
        }

        modem_intf_send_ctrl_gpio_message(GPIO_RESET, assert_status);
        return IRQ_HANDLED;
}

int modem_share_gpio_init(void *para)
{
        struct modem_intf_platform_data *modem_config = (struct modem_intf_platform_data *)para;
        int    error = 0;
        int    status = 0;

        pr_debug("modem_share_gpio_init start!!! \n");
        cp_to_ap_rdy = modem_config->modem_boot_gpio;

        error = gpio_request(cp_to_ap_rdy, "modem_boot_gpio");
        if (error) {
                pr_err("Cannot request GPIO %d\n", cp_to_ap_rdy);
                gpio_free(cp_to_ap_rdy);
                return error;
        }

        gpio_direction_input(cp_to_ap_rdy);
        gpio_export(cp_to_ap_rdy,0);
        cp_to_ap_rdy_irq = gpio_to_irq(cp_to_ap_rdy);
        if (cp_to_ap_rdy_irq < 0)
                return -1;
        error = request_threaded_irq(cp_to_ap_rdy_irq, cp_to_ap_rdy_handle,
                                     NULL, IRQF_DISABLED|IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING, "modem_boot", para);
        if (error) {
                printk("lee :cannot alloc cp_to_ap_rdy_irq, err %d\r\n", error);
                gpio_free(cp_to_ap_rdy);
                return error;
        }

        status = gpio_get_value(cp_to_ap_rdy);
        if(status == 0)
                modem_intf_send_GPIO_message(cp_to_ap_rdy,status,0);
        else
                modem_intf_send_GPIO_message(cp_to_ap_rdy,1,0);

        return 0;
}

int modem_share_gpio_uninit(void *para)
{
        struct modem_intf_platform_data *modem_config = (struct nodem_intf_platform_data *)para;

        pr_debug("modem_gpio_init start!!! \n");
        cp_to_ap_rdy = modem_config->modem_boot_gpio;

        cp_to_ap_rdy_irq = gpio_to_irq(cp_to_ap_rdy);
        if (cp_to_ap_rdy_irq < 0)
                return -1;
        free_irq( cp_to_ap_rdy_irq, para);
        gpio_free(cp_to_ap_rdy);

        return 0;
}

int modem_gpio_irq_init(void *para)
{
        int    error = 0;

		cp_watchdog_gpio_irq = gpio_to_irq(cp_watchdog_gpio);
		if(cp_watchdog_gpio_irq < 0)
			return -1;
		error = request_threaded_irq(cp_watchdog_gpio_irq, cp_watchdog_gpio_handle,
                                     NULL, IRQF_DISABLED|IRQF_NO_SUSPEND |IRQF_TRIGGER_LOW, "modem_watchdog", para);
        if (error) {
                printk("modem_intf :cannot alloc cp_watchdog irq, err %d\r\n", error);
                gpio_free(cp_watchdog_gpio);
                return error;
        }

        enable_irq_wake(cp_watchdog_gpio_irq);

        cp_alive_gpio_irq = gpio_to_irq(cp_alive_gpio);
        if (cp_alive_gpio_irq < 0)
                return -1;
        error = request_threaded_irq(cp_alive_gpio_irq, cp_alive_gpio_handle,
                                     NULL, IRQF_DISABLED|IRQF_NO_SUSPEND |IRQF_TRIGGER_HIGH, "modem_alive", para);
        if (error) {
                printk("modem_intf :cannot alloc cp_alive irq, err %d\r\n", error);
                gpio_free(cp_alive_gpio);
                return error;
        }

        enable_irq_wake(cp_alive_gpio_irq);

        cp_crash_gpio_irq = gpio_to_irq(cp_crash_gpio);
        if (cp_crash_gpio_irq < 0)
                return -1;
        error = request_threaded_irq(cp_crash_gpio_irq, cp_crash_gpio_handle,
                                     NULL, IRQF_DISABLED|IRQF_NO_SUSPEND |IRQF_TRIGGER_HIGH, "modem_crash", para);
        if (error) {
                printk("modem_intf :cannot alloc cp_crash irq, err %d\r\n", error);
                gpio_free(cp_crash_gpio);
                return error;
        }

        enable_irq_wake(cp_crash_gpio_irq);
}


int modem_gpio_init(void *para)
{
        struct modem_intf_platform_data *modem_config = (struct modem_intf_platform_data *)para;
        int    error = 0;

        pr_debug("modem_gpio_init start!!! \n");
        cp_to_ap_rdy = modem_config->modem_boot_gpio;
        cp_watchdog_gpio = modem_config->modem_watchdog_gpio;
        cp_alive_gpio = modem_config->modem_alive_gpio;
        cp_crash_gpio = modem_config->modem_crash_gpio;
        modem_power_gpio = modem_config->modem_power_gpio;
        modem_reset_gpio = modem_config->modem_reset_gpio;

        if(GPIO_INVALID != modem_power_gpio) {
                gpio_request(modem_power_gpio, "modem_power_gpio");
                gpio_export(modem_power_gpio,0);
        }

        if(GPIO_INVALID != modem_reset_gpio) {
                gpio_request(modem_reset_gpio, "modem_reset_gpio");
                gpio_export(modem_reset_gpio,0);
        }

        //watchdog gpio
        error = gpio_request(cp_watchdog_gpio, "modem_watchdog_gpio");
        if (error) {
                pr_err("Cannot request GPIO %d\n", cp_watchdog_gpio);
                gpio_free(cp_watchdog_gpio);
                return error;
        }
        gpio_export(cp_watchdog_gpio,0);
        gpio_direction_input(cp_watchdog_gpio);


//////////////////////////////////////////////////
        //alive gpio
        error = gpio_request(cp_alive_gpio, "modem_alive_gpio");
        if (error) {
                pr_err("Cannot request GPIO %d\n", cp_alive_gpio);
                gpio_free(cp_alive_gpio);
                return error;
        }
        gpio_export(cp_alive_gpio,0);
        gpio_direction_input(cp_alive_gpio);

        //crash gpio
        error = gpio_request(cp_crash_gpio, "modem_crash_gpio");
        if (error) {
                pr_err("Cannot request GPIO %d\n", cp_crash_gpio);
                gpio_free(cp_crash_gpio);
                return error;
        }
        gpio_export(cp_crash_gpio,0);
        gpio_direction_input(cp_crash_gpio);

/////////////////////////////////////////////////

        return 0;
}

int modem_gpio_uninit(void *para)
{
        modem_share_gpio_uninit(para);
        return 0;
}

void modem_poweron_async_step1(void)
{
        if(GPIO_INVALID != modem_power_gpio) {
                printk(KERN_ERR "modem_poweron_async_step1!!! \n");
               gpio_direction_output(modem_power_gpio, 1);
        } else {
                printk(KERN_ERR "modem_poweron_async_step1 modem_power_gpio == GPIO_INVALID!!! \n");
        }
}

void modem_poweron_async_step2(void)
{
        if(GPIO_INVALID != modem_power_gpio) {
                gpio_set_value(modem_power_gpio, 0);
                printk(KERN_ERR "modem_poweron_async_step2!!! \n");
        } else {
                printk(KERN_ERR "modem_poweron_async_step2 modem_power_gpio == GPIO_INVALID!!! \n");
        }
}

void modem_poweron(void)
{
        printk("modem power on!!! \n");
        if(GPIO_INVALID != modem_power_gpio) {
                gpio_direction_output(modem_power_gpio, 1); //2012.1.10
                //gpio_set_value(modem_power_gpio, 1);
                msleep(2000);
                gpio_set_value(modem_power_gpio, 0);
        } else {
                //sprd_greg_clear_bits(REG_TYPE_GLOBAL, 0x00000040, 0x4C);
        }
}
void modem_poweroff(void)
{
        printk("modem power off!!! \n");
        if (GPIO_INVALID != modem_power_gpio) {
                gpio_direction_output(modem_power_gpio, 1); //2012.1.10
                //gpio_set_value(modem_power_gpio, 1);
                msleep(3500);//should be matched with Adie config of cp
                gpio_set_value(modem_power_gpio, 0);
        } else {
                //sprd_greg_set_bits(REG_TYPE_GLOBAL, 0x00000040, 0x4C);
        }
}

void modem_soft_reset(void)
{
        printk(KERN_INFO "modem soft reset enter \n");
        if (GPIO_INVALID != modem_reset_gpio) {
                gpio_direction_output(modem_reset_gpio, 1); //2012.1.10
                //gpio_set_value(modem_power_gpio, 1);
                msleep(50);
                gpio_set_value(modem_reset_gpio, 0);
        }
}

void modem_hard_reset(void)
{
        printk(KERN_INFO "modem hard reset enter \n");
        if (GPIO_INVALID != modem_power_gpio) {
                gpio_direction_output(modem_power_gpio, 1); //2012.1.10
                //gpio_set_value(modem_power_gpio, 1);
                msleep(7100);
                gpio_set_value(modem_power_gpio, 0);
        }
}
