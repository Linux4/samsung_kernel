/* ET300 FringerPrint Sensor signal trigger routine
 *
 * Copyright (c) 2000-2014 EgisTec
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */
 
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <asm/io.h>
#include <linux/io.h>
#include <linux/wakelock.h>

#include <soc/sprd/gpio.h>

#include "sprd_fps_trigger.h"


static struct wake_lock et300_lock;

#define Interrupt_Count_Threshold	(6)

int interrupt_count = 0;

int _fIrq = -1;
int et300_wake = 1;
int et300_wake_lock_status = 0;

static volatile int ev_press = 0;

static volatile char interrupt_values[] = {
	'0', '0', '0', '0', '0', '0', '0', '0'
};

static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);

struct delayed_work fingerprint_delay_work;

void fingerprint_eint_work_func(struct work_struct *work)
{
	disable_irq_nosync(_fIrq);

	if (interrupt_count >= Interrupt_Count_Threshold){
		et300_device_change();
		printk( "sprd:%s: interrupt count is %d,interrupt work done!\n",__func__,interrupt_count);
	}

	interrupt_count = 0;

	enable_irq(_fIrq);

}

static irqreturn_t fingerprint_interrupt(int irq, void *dev_id)
{
	if(et300_wake){
		//disable_irq_nosync(_fIrq);
		if (interrupt_count == 0)
			schedule_delayed_work(&fingerprint_delay_work, msecs_to_jiffies(10));

		interrupt_count++;
		printk("sprd:%s:interrupt irq count is %d\n",__func__,interrupt_count);
	}else{
		if(!et300_wake_lock_status){
			wake_lock_timeout(&et300_lock, msecs_to_jiffies(3000));
			et300_wake_lock_status = 1;
		}
	}

	return IRQ_HANDLED;
}

int sprd_et300_parse_dts_gpio(int index)
{
	struct device_node * np;
	enum of_gpio_flags flags;
	int gpio = -1;

	np=of_find_compatible_node(NULL,NULL,"fingerprint_keys");
	if (!np) {
		printk("sprd:%s: can't find fingerprint_key device tree node\n",__func__);
		return -1;
	}

	gpio = of_get_gpio_flags(np, index, &flags);
	if (gpio < 0) {
			printk("sprd:%s:Failed to get gpio flags, error: %d\n",__func__, gpio);
	}

	return gpio;
}

int sprd_et300_3v3ldo_enable(int flag)
{
	int error = 0;
	int gpio_3v3ldo = -1;

	printk("sprd:%s.\n",__func__);

	gpio_3v3ldo = sprd_et300_parse_dts_gpio(1);
	if(gpio_3v3ldo < 0){
		printk("sprd:%s,Failed to get 3v3ldo gpio from dts: %d.\n",__func__,gpio_3v3ldo);
		return gpio_3v3ldo;
	}

	error = gpio_request(gpio_3v3ldo,"gpio_et300_3v3ldo");
	if(error){
		printk("sprd:gpio_request failed.\n");
	}

	if(flag)
		error = gpio_direction_output(gpio_3v3ldo, 1);
	else
		error = gpio_direction_output(gpio_3v3ldo, 0);

	if(error){
		printk("gpio_direction_output failed.\n");
		return error;
	}

	return error;
}

int sprd_et300_get_int_version(void)
{
	int gpio_version = -1;
	int error = 0;

	gpio_version = sprd_et300_parse_dts_gpio(4);
	if(gpio_version < 0){
		printk("sprd:%s,Failed to get version gpio from dts: %d.\n",__func__,gpio_version);
		return gpio_version;
	}

	error = gpio_request(gpio_version,"gpio_et300_int_version");
	if (error < 0){
		printk("sprd:%s:gpio_request failed!\n",__func__);
		return error;
	}

	gpio_direction_input(gpio_version);

	return gpio_get_value(gpio_version);

}

int sprd_et300_request_irq(int gpio_int)
{
	int error = 0;

	error = gpio_request(gpio_int,"gpio_et300_int");
	if (error < 0){
		printk("sprd:%s:gpio_request failed!\n",__func__);
		return error;
	}

	gpio_direction_input(gpio_int);

	_fIrq = gpio_to_irq(gpio_int);
	if(_fIrq < 0){
		printk("sprd:%s:gpio_to_irq failed: %d.\n",__func__,_fIrq);
		return _fIrq;
	}

	error = request_irq(_fIrq, fingerprint_interrupt, IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "firq-fi", NULL );
	if(error != 0){
		printk( "sprd:%s: request interrupt fail\n",__func__);
		return -1;
	}

	return 0;
}

int Interrupt_Init(void)
{
	int rv = 0;
	int gpio_int = -1;
	int version = 1;

	INIT_DELAYED_WORK(&fingerprint_delay_work, fingerprint_eint_work_func);
	wake_lock_init(&et300_lock, WAKE_LOCK_SUSPEND, "et300_wakelock");

	version = sprd_et300_get_int_version();
	if(version == 1)
		gpio_int = sprd_et300_parse_dts_gpio(0);	//eic async pin
	else if (version == 0)
		gpio_int = sprd_et300_parse_dts_gpio(3);	//gpio 85

	if(gpio_int < 0){
		printk("sprd:%s,Failed to get interrupt gpio from dts: %d.\n",__func__,gpio_int);
		return gpio_int;
	}

	rv = sprd_et300_request_irq(gpio_int);
	if(rv <0){
		printk("sprd:%s,sprd_et300_request_irq failed.\n",__func__);
		return rv;
	}

	return 0;
}


int Interrupt_Free(void)
{
	printk("%s.\n",__func__);
	return 0;
}


int fps_interrupt_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	unsigned long err;

	if (filp->f_flags & O_NONBLOCK){
		return -EAGAIN;
	}else{
		wait_event_interruptible(interrupt_waitq, ev_press);
	}

	printk(KERN_ERR "interrupt read condition %d\n",ev_press);
	printk(KERN_ERR "interrupt value  %d\n",interrupt_values[0]);

	err = copy_to_user((void *)buff, (const void *)(&interrupt_values),min(sizeof(interrupt_values), count));

	return err ? -EFAULT : min(sizeof(interrupt_values), count);
}


int Interrupt_Exit(void)
{
//	ev_press=1;
	printk("%s.\n",__func__);
	return 0;
}


unsigned int fps_interrupt_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &interrupt_waitq, wait);
	if (ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}


