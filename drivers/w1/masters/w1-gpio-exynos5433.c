/*
 * w1-gpio - GPIO w1 bus master driver
 *
 * Copyright (C) 2007 Ville Syrjala <syrjala@sci.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/w1-gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/input.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#endif /* CONFIG_OF */

#include "../w1.h"
#include "../w1_int.h"

#define USING_DIRECT_GPIO_REG_CONTROL

#if defined(USING_DIRECT_GPIO_REG_CONTROL)
#include <linux/io.h>

#include <linux/irqflags.h>
#include <linux/interrupt.h>

static void __iomem *g_addr;
static void __iomem *g_addr_data;

static unsigned int reg_mask;
static unsigned int control_reg_output_mask;
static unsigned int control_reg_input_mask;
static unsigned int data_reg_mask;

static int temp_irq = -1;

#endif

extern void w1_master_search(void);
extern bool w1_attached;

static DEFINE_SPINLOCK(w1_gpio_lock);

static int w1_delay_parm = 1;
static void w1_delay(unsigned long tm)
{
	udelay(tm * w1_delay_parm);
}

static void w1_gpio_write_bit(void *data, u8 bit);
static u8 w1_gpio_read_bit(void *data);

static u8 w1_gpio_set_pullup(void *data, int duration)
{
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);
	else {
		gpio_direction_output(pdata->pin, 1);
		gpio_set_value(pdata->pin, 1);
	}
	return 0;
}

static void w1_gpio_write_bit_dir(void *data, u8 bit)
{
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (bit)
		gpio_direction_input(pdata->pin);
	else
		gpio_direction_output(pdata->pin, 0);
}

static void w1_gpio_write_bit_val(void *data, u8 bit)
{
#if !defined(USING_DIRECT_GPIO_REG_CONTROL)
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	gpio_set_value(pdata->pin, bit);
#else
	unsigned int tmp;
	unsigned int tmp_con;
	/*setting data register via control register*/
	tmp = __raw_readl(g_addr + 0x4);

	tmp_con = __raw_readl(g_addr);
	tmp_con = tmp_con | control_reg_output_mask;
	__raw_writel(tmp_con, (g_addr));

	if (bit)
		tmp = tmp | data_reg_mask;
	else
		tmp = tmp & ~data_reg_mask;

	__raw_writel(tmp, (g_addr_data));
#endif
}

static u8 w1_gpio_read_bit_val(void *data)
{
	int ret;

#if !defined(USING_DIRECT_GPIO_REG_CONTROL)
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	ret = gpio_get_value(pdata->pin) ? 1 : 0;
	if (ret) {
		pr_err("%s: Unknown w1 gpio pin\n", __func__);
		return -EINVAL;
	}
#else
	unsigned int tmp;
	unsigned int tmp_con;

	tmp_con = __raw_readl(g_addr);
	tmp_con = tmp_con & control_reg_input_mask;
	__raw_writel(tmp_con, g_addr);

	tmp = __raw_readl(g_addr_data);
	ret = (tmp >> reg_mask) & 0x1;
#endif /* USING_DIRECT_GPIO_REG_CONTROL */
	return ret;
}

/**
 * Generates a write-0 or write-1 cycle and samples the level.
 */
static u8 w1_gpio_touch_bit(void *data, u8 bit)
{
	if (bit) {
		return w1_gpio_read_bit(data);
	} else {
		w1_gpio_write_bit(data, 0);
		return 0;
	}
}

/**
 * Generates a write-0 or write-1 cycle.
 * Only call if dev->bus_master->touch_bit is NULL
 */
static void w1_gpio_write_bit(void *data, u8 bit)
{
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;
	unsigned long irq_flags;

	unsigned int tmp_data;
	unsigned int tmp_con;
	unsigned int tmp_data_0;
	unsigned int tmp_data_1;

	tmp_data = __raw_readl(g_addr_data);
	tmp_data_1 = tmp_data | data_reg_mask;
	tmp_data_0 = tmp_data & ~data_reg_mask;

	tmp_con = __raw_readl(g_addr);
	tmp_con = tmp_con | control_reg_output_mask;
	__raw_writel(tmp_con, (g_addr));

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);

	if (bit) {
		__raw_writel(tmp_data_0, (g_addr_data));
		__raw_writel(tmp_data_1, (g_addr_data));
		/*originally overdriver mode needs 12us delay*/
		(pdata->slave_speed == 0) ? w1_delay(64) : w1_delay(16);
	} else {
		__raw_writel(tmp_data_0, (g_addr_data));
		(pdata->slave_speed == 0) ? w1_delay(60) : w1_delay(6);
		__raw_writel(tmp_data_1, (g_addr_data));
		w1_delay(10);
	}
	spin_unlock_irqrestore(&w1_gpio_lock, irq_flags);
}

/**
 * Pre-write operation, currently only supporting strong pullups.
 * Program the hardware for a strong pullup, if one has been requested and
 * the hardware supports it.
 *
 */
static void w1_gpio_pre_write(void *data)
{
	w1_gpio_set_pullup(data, 0);
}

/**
 * Post-write operation, currently only supporting strong pullups.
 * If a strong pullup was requested, clear it if the hardware supports
 * them, or execute the delay otherwise, in either case clear the request.
 *
 */
static void w1_gpio_post_write(void *data)
{
	w1_gpio_set_pullup(data, 0);
}

/**
 * Writes 8 bits.
 *
 * @param data    the master device data
 * @param byte    the byte to write
 */
static void w1_gpio_write_8(void *data, u8 byte)
{
	int i;

	w1_gpio_pre_write(data);
	for (i = 0; i < 8; ++i) {
		if (i == 7)
			w1_gpio_pre_write(data);
		w1_gpio_write_bit(data, (byte >> i) & 0x1);
	}
	w1_gpio_post_write(data);
}

/**
 * Generates a write-1 cycle and samples the level.
 * Only call if dev->bus_master->touch_bit is NULL
 */
static u8 w1_gpio_read_bit(void *data)
{
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;
	unsigned long irq_flags;
	unsigned int tmp_data;
	unsigned int tmp_con;
	int result;

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);

	/*get gpio control register value*/
	tmp_con = __raw_readl(g_addr);
	/*control register set direction input*/
	tmp_con = tmp_con & control_reg_input_mask;

	/*set gpio data register as 0*/
	tmp_data = __raw_readl(g_addr_data);
	tmp_data = tmp_data & ~data_reg_mask;
	__raw_writel(tmp_data, (g_addr_data));

	/*change gpio direction input*/
	__raw_writel(tmp_con, (g_addr));

	/*read gpio data register from slave*/
	tmp_data = __raw_readl(g_addr_data);
	result = (tmp_data >> reg_mask) & 0x1;

	/*set gpio data register as 1*/
	tmp_data = tmp_data | data_reg_mask;
	__raw_writel(tmp_data, (g_addr_data));

	/*change gpio direction output*/
	tmp_con = tmp_con | control_reg_output_mask;
	__raw_writel(tmp_con, (g_addr));

	(pdata->slave_speed == 0) ? w1_delay(55) : w1_delay(16);

	spin_unlock_irqrestore(&w1_gpio_lock, irq_flags);

	return result & 0x1;
}
/**
 * Does a triplet - used for searching ROM addresses.
 * Return bits:
 *  bit 0 = id_bit
 *  bit 1 = comp_bit
 *  bit 2 = dir_taken
 * If both bits 0 & 1 are set, the search should be restarted.
 *
 * @param data     the master device data
 * @param bdir    the bit to write if both id_bit and comp_bit are 0
 * @return        bit fields - see above
 */
static u8 w1_gpio_triplet(void *data, u8 bdir)
{
	u8 id_bit   = w1_gpio_touch_bit(data, 1);
	u8 comp_bit = w1_gpio_touch_bit(data, 1);
	u8 retval;

	if (id_bit && comp_bit)
		return 0x03;  /* error */

	if (!id_bit && !comp_bit) {
		/* Both bits are valid, take the direction given */
		retval = bdir ? 0x04 : 0;
	} else {
		/* Only one bit is valid, take that direction */
		bdir = id_bit;
		retval = id_bit ? 0x05 : 0x02;
	}

	w1_gpio_touch_bit(data, bdir);
	return retval;
}

/**
 * Reads 8 bits.
 *
 * @param data     the master device data
 * @return        the byte read
 */
static u8 w1_gpio_read_8(void *data)
{
	int i;
	u8 res = 0;

	for (i = 0; i < 8; ++i)
		res |= (w1_gpio_touch_bit(data, 1) << i);

	return res;
}

/**
 * Writes a series of bytes.
 *
 * @param data     the master device data
 * @param buf     pointer to the data to write
 * @param len     the number of bytes to write
 */
static void w1_gpio_write_block(void *data, const u8 *buf, int len)
{
	int i;

	w1_gpio_pre_write(data);
	for (i = 0; i < len; ++i)
		w1_gpio_write_8(data, buf[i]); /* calls w1_pre_write */
	w1_gpio_post_write(data);
}

/**
 * Reads a series of bytes.
 *
 * @param data     the master device data
 * @param buf     pointer to the buffer to fill
 * @param len     the number of bytes to read
 * @return        the number of bytes read
 */
static u8 w1_gpio_read_block(void *data, u8 *buf, int len)
{
	int i;
	u8 ret;

	for (i = 0; i < len; ++i)
		buf[i] = w1_gpio_read_8(data);
	ret = len;

	return ret;
}

/**
 * Issues a reset bus sequence.
 *
 * @param  dev The bus master pointer
 * @return     0=Device present, 1=No device present or error
 */
static u8 w1_gpio_reset_bus(void *data)
{
	struct platform_device *pdev = data;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct w1_bus_master *master = platform_get_drvdata(pdev);
	unsigned long irq_flags;
	int result;

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);
	master->write_bit(pdata, 0);
	/* minimum 480, max ? us
	 * be nice and sleep, except 18b20 spec lists 960us maximum,
	 * so until we can sleep with microsecond accuracy, spin.
	 * Feel free to come up with some other way to give up the
	 * cpu for such a short amount of time AND get it back in
	 * the maximum amount of time.
	 */
	(pdata->slave_speed == 0) ? w1_delay(480) : w1_delay(48);
	master->write_bit(pdata, 1);

	/*originally overdriver mode need 8us delay*/
	(pdata->slave_speed == 0) ? w1_delay(70) : w1_delay(7);

	result = w1_gpio_read_bit_val(data) & 0x1;

	/* minmum 70 (above) + 410 = 480 us
	 * There aren't any timing requirements between a reset and
	 * the following transactions.  Sleeping is safe here.
	 */
	/* w1_delay(410); min required time */
	(pdata->slave_speed == 0) ? msleep(1) : w1_delay(40);

	spin_unlock_irqrestore(&w1_gpio_lock, irq_flags);
	return result;
}

static int hall_open(struct input_dev *input)
{
	return 0;
}

static void hall_close(struct input_dev *input)
{
}

#if defined(USING_DIRECT_GPIO_REG_CONTROL)
static void set_register_mask(unsigned int mask)
{
	reg_mask = mask;
	data_reg_mask = 0x1 << mask;
	control_reg_output_mask = 0x1 << (mask * 4);
	control_reg_input_mask = ~(0xf << (mask * 4));
}
#endif

int w1_read_detect_state(void)
{
	return gpio_get_value(temp_irq);
}

#if defined(CONFIG_OF)
static int of_w1_gpio_dt(struct device *dev, struct w1_gpio_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp;

	of_property_read_u32(np, "is_open_drain", &temp);
	pdata->is_open_drain = (unsigned int)temp;
	of_property_read_u32(np, "slave_speed", &temp);
	pdata->slave_speed = (unsigned int)temp;
	of_property_read_u32(np, "register_addr", &temp);
	pdata->register_addr = (unsigned int)temp;
	of_property_read_u32(np, "register_num", &temp);
	pdata->register_num = (unsigned int)temp;

	pdata->pin = of_get_named_gpio(np, "w1,gpio", 0);
	pdata->irq_gpio = of_get_named_gpio(np, "w1,irq-gpio", 0);
	pr_info("%s : irq num (%d)\n", __func__,pdata->irq_gpio);
	if((int)pdata->irq_gpio < 0)
		pdata->irq_gpio = -1;

	/* not used, get it from dt? */
	pdata->ext_pullup_enable_pin = -1;

	return 0;
}
#endif /* CONFIG_OF */

void w1_irqwork(struct work_struct *irqwork)
{
	int i=0;

	while(i<5) {
		pr_info("%s : Inside W1 While Loop(%d)\n", __func__, ++i);
		msleep(50);
		w1_master_search();
		if(w1_attached)
			break;
	}
}
static irqreturn_t w1_detect_irq(int irq, void *dev_id)
{
#if !defined(CONFIG_SEC_FACTORY)
	struct w1_bus_master *dev = dev_id;

	pr_info("%s : Inside W1 IRQ Handler\n", __func__);
	schedule_delayed_work(&dev->w1_irqwork, 100);
#else
	pr_info("%s : Factory mode Inside W1 IRQ Handler do nothing\n", __func__);
#endif
	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _name)					\
do {									\
	ret = request_threaded_irq(_irq, NULL, w1_detect_irq,		\
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |		\
		IRQF_DISABLED | IRQF_ONESHOT, _name, master);		\
	if (ret < 0) {							\
		pr_err("Failed to request IRQ #%d: %d\n", _irq, ret);	\
		return ret;						\
	}								\
} while (0)

static int __init w1_gpio_probe(struct platform_device *pdev)
{
	struct w1_bus_master *master;
	struct w1_gpio_platform_data *pdata;
	struct input_dev *input;
	int err;
	int ret, irq=0;

	pr_info("%s : start\n", __func__);
#if defined(CONFIG_OF)
	pdata = kzalloc(sizeof(struct w1_gpio_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	err = of_w1_gpio_dt(&pdev->dev, pdata);
	pdev->dev.platform_data = pdata;
#else
	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -ENXIO;
#endif /* CONFIG_OF */

	master = kzalloc(sizeof(struct w1_bus_master), GFP_KERNEL);
	if (!master) {
		err = -ENOMEM;
		goto free_pdata;
	}

	temp_irq = pdata->irq_gpio;
	if (temp_irq >=0) {
		pr_info("%s: IRQ mod is enabled\n", __func__);
		irq = gpio_to_irq(pdata->irq_gpio);
		INIT_DELAYED_WORK(&master->w1_irqwork, w1_irqwork);
		master->irq_mode=true;
	}

	/* add for sending uevent */
	input = input_allocate_device();
	if (!input) {
		err = -ENOMEM;
		goto free_master;
	}

	master->input = input;

	input_set_drvdata(input, master);

	input->name = "w1";
	input->phys = "w1";
	input->dev.parent = &pdev->dev;

	/* need to change */
	input->evbit[0] |= BIT_MASK(EV_SW);
	input_set_capability(input, EV_SW, SW_W1);

	input->open = hall_open;
	input->close = hall_close;
	/* need to change */

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	err = input_register_device(input);
	if (err)
		goto free_input;
	/* add for sending uevent */

	spin_lock_init(&w1_gpio_lock);

	if (master->irq_mode) {
		REQUEST_IRQ(irq, "w1-detect");
		enable_irq_wake(irq);
	}
	err = gpio_request(pdata->pin, "w1");
	if (err)
		goto unregister_input;

	if (gpio_is_valid(pdata->ext_pullup_enable_pin)) {
		err = gpio_request_one(pdata->ext_pullup_enable_pin,
				       GPIOF_INIT_LOW, "w1 pullup");
		if (err < 0) {
			dev_err(&pdev->dev,
			"gpio_request_one (ext_pullup_enable_pin) failed\n");
			goto free_gpio;
		}
	}

	master->data = pdev;
	master->read_bit = w1_gpio_read_bit_val;
	master->touch_bit = w1_gpio_touch_bit;
	master->read_byte = w1_gpio_read_8;
	master->write_byte = w1_gpio_write_8;
	master->read_block = w1_gpio_read_block;
	master->write_block = w1_gpio_write_block;
	master->triplet = w1_gpio_triplet;
	master->reset_bus =  w1_gpio_reset_bus;
	master->set_pullup = w1_gpio_set_pullup;

#if defined(USING_DIRECT_GPIO_REG_CONTROL)
	g_addr = ioremap(pdata->register_addr, 0x10);
	g_addr_data = g_addr + 0x4;
	set_register_mask(pdata->register_num);
#endif

	if (pdata->is_open_drain) {
		gpio_direction_output(pdata->pin, 1);
		master->write_bit = w1_gpio_write_bit_val;
	} else {
		gpio_direction_input(pdata->pin);
		master->write_bit = w1_gpio_write_bit_dir;
	}

	platform_set_drvdata(pdev, master);

	err = w1_add_master_device(master);
	if (err)
		goto free_gpio_ext_pu;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	if (gpio_is_valid(pdata->ext_pullup_enable_pin))
		gpio_set_value(pdata->ext_pullup_enable_pin, 1);

	return 0;

free_gpio_ext_pu:
	if (gpio_is_valid(pdata->ext_pullup_enable_pin))
		gpio_free(pdata->ext_pullup_enable_pin);
free_gpio:
	gpio_free(pdata->pin);
unregister_input:
	input_unregister_device(input);
free_input:
	input_free_device(input);
free_master:
	kfree(master);
free_pdata:
#if defined(CONFIG_OF)
	kfree(pdata);
#endif
	return err;
}

static int __exit w1_gpio_remove(struct platform_device *pdev)
{
	struct w1_bus_master *master = platform_get_drvdata(pdev);
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(0);

	if (gpio_is_valid(pdata->ext_pullup_enable_pin))
		gpio_set_value(pdata->ext_pullup_enable_pin, 0);

	w1_remove_master_device(master);
	gpio_free(pdata->pin);
	kfree(master);

	return 0;
}

#ifdef CONFIG_PM

static int w1_gpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(0);

	gpio_direction_input(pdata->pin);

#ifdef CONFIG_W1_WORKQUEUE
	cancel_delayed_work_sync(&w1_gdev->w1_dwork);
#endif
	return 0;
}

bool w1_is_suspended;

static int w1_gpio_resume(struct platform_device *pdev)
{
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	gpio_direction_output(pdata->pin, 1);

	w1_is_suspended = true;
#ifdef CONFIG_W1_WORKQUEUE
	schedule_delayed_work(&w1_gdev->w1_dwork, HZ * 2);
#endif

	return 0;
}

#else
#define w1_gpio_suspend NULL
#define w1_gpio_resume  NULL
#endif

#if defined(CONFIG_OF)
static struct of_device_id w1_gpio_dt_ids[] = {
	{ .compatible = "w1_gpio_exynos" },
	{ },
};
MODULE_DEVICE_TABLE(of, w1_gpio_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver w1_gpio_driver = {
	.driver = {
		.name   = "w1_gpio_exynos",
		.owner  = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = w1_gpio_dt_ids,
#endif /* CONFIG_OF */
	},
	.remove = __exit_p(w1_gpio_remove),
	.suspend = w1_gpio_suspend,
	.resume = w1_gpio_resume,
};

static int __init w1_gpio_init(void)
{
	return platform_driver_probe(&w1_gpio_driver, w1_gpio_probe);
}

static void __exit w1_gpio_exit(void)
{
	platform_driver_unregister(&w1_gpio_driver);
}

late_initcall(w1_gpio_init);
module_exit(w1_gpio_exit);

MODULE_DESCRIPTION("GPIO w1 bus master driver");
MODULE_AUTHOR("clark.kim@maximintegrated.com");
MODULE_LICENSE("GPL");
