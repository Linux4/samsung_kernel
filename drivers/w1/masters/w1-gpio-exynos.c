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

#define	USING_DIRECT_GPIO_REG_CONTROL

#if defined(USING_DIRECT_GPIO_REG_CONTROL)
#include <asm/io.h>
#if defined(CONFIG_SOC_EXYNOS5422)
#define	BASE_ADDRESS	0x140100A0
#define SHIFT_OFFSET	1
#define DATA_MASK	(1 << SHIFT_OFFSET)
#elif defined(CONFIG_SOC_EXYNOS5430)
#define	BASE_ADDRESS	0x14CC0120
#endif

static void __iomem *g_addr;
unsigned int tmp;
#endif

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
	struct w1_gpio_platform_data *pdata = data;
//	unsigned long irq_flags;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);
	else {
//		spin_lock_irqsave(&w1_gpio_lock, irq_flags);
		gpio_direction_output(pdata->pin, 1);
		gpio_set_value(pdata->pin, 1);
//		spin_unlock_irqrestore(&w1_gpio_lock, irq_flags);
	}

	return 0;
}

static void w1_gpio_write_bit_dir(void *data, u8 bit)
{
	struct w1_gpio_platform_data *pdata = data;

	if (bit)
		gpio_direction_input(pdata->pin);
	else
		gpio_direction_output(pdata->pin, 0);
}

static void w1_gpio_write_bit_val(void *data, u8 bit)
{
#if !defined(USING_DIRECT_GPIO_REG_CONTROL)
	struct w1_gpio_platform_data *pdata = data;

	gpio_set_value(pdata->pin, bit);
#else
	tmp = __raw_readl(g_addr + 0x4);
	if (bit)
#if defined(CONFIG_SOC_EXYNOS5422)
		tmp = tmp | 0x00000002;
#elif defined(CONFIG_SOC_EXYNOS5430)
		tmp = tmp | 0x00000020;
#endif
	else
#if defined(CONFIG_SOC_EXYNOS5422)
		tmp = tmp & 0xfffffffd;
#elif defined(CONFIG_SOC_EXYNOS5430)
		tmp = tmp & 0xffffffdf;
#endif

	__raw_writel(tmp, (g_addr + 0x4));
#endif
}

static u8 w1_gpio_read_bit_val(void *data)
{
	int ret;
#if !defined(USING_DIRECT_GPIO_REG_CONTROL)
	struct w1_gpio_platform_data *pdata = data;

	ret = gpio_get_value(pdata->pin) ? 1 : 0;
	if (ret) {
		pr_err("%s: Unknown w1 gpio pin\n", __func__);
		return -EINVAL;
	}
#else
	tmp = __raw_readl(g_addr + 0x4);
#if defined(CONFIG_SOC_EXYNOS5422)
	ret = (tmp >> 1) & 0x1;
#elif defined(CONFIG_SOC_EXYNOS5430)
	ret = (tmp >> 5) & 0x1;
#else
	ret = -ENODEV;
#endif
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
	struct w1_gpio_platform_data *pdata = data;
	void	(*write_bit)(void *, u8);
	unsigned long irq_flags;

	if (pdata->is_open_drain) {
		write_bit = w1_gpio_write_bit_val;
	} else {
		write_bit = w1_gpio_write_bit_dir;
	}

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);

	if (bit) {
		write_bit(data, 0);
//		originally overdriver mode needs 1us delay
		if (pdata->slave_speed == 0)
			w1_delay(6);
//		(pdata->slave_speed == 0) ? w1_delay(6) : w1_delay(1);
		write_bit(data, 1);

//		originally overdriver mode needs 12us delay
//		(pdata->slave_speed == 0)? w1_delay(64) : w1_delay(12);
		(pdata->slave_speed == 0)? w1_delay(64) : w1_delay(10);
	} else {
		write_bit(data, 0);
		(pdata->slave_speed == 0)? w1_delay(60) : w1_delay(8);
		write_bit(data, 1);
		(pdata->slave_speed == 0)? w1_delay(10) : w1_delay(5);
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
//		w1_gpio_write_bit(data, (byte >> i) & 0x1);
		w1_gpio_touch_bit(data, (byte >> i) & 0x1);
	}
	w1_gpio_post_write(data);
}

/**
 * Generates a write-1 cycle and samples the level.
 * Only call if dev->bus_master->touch_bit is NULL
 */
static u8 w1_gpio_read_bit(void *data)
{
	struct w1_gpio_platform_data *pdata = data;
	int result;
	void	(*write_bit)(void *, u8);
	unsigned long irq_flags;
	unsigned int data_low, data_high;

	if (pdata->is_open_drain) {
		write_bit = w1_gpio_write_bit_val;
	} else {
		write_bit = w1_gpio_write_bit_dir;
	}

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);

	/* sample timing is critical here */
	tmp = __raw_readl(g_addr + 0x4);

	data_low = tmp & ~DATA_MASK;
	data_high = tmp | DATA_MASK;

	__raw_writel(data_low, (g_addr + 0x4));
	__raw_writel(data_high, (g_addr + 0x4));

	tmp = __raw_readl(g_addr + 0x4);
	result = (tmp >> SHIFT_OFFSET) & 0x1;

	(pdata->slave_speed == 0)? w1_delay(55) : w1_delay(8);

	spin_unlock_irqrestore(&w1_gpio_lock, irq_flags);

	return result;
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
		res |= (w1_gpio_touch_bit(data,1) << i);

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
	int result = 0;
	struct w1_gpio_platform_data *pdata = data;
	void	(*write_bit)(void *, u8);
	unsigned long irq_flags;

	if (pdata->is_open_drain)
		write_bit = w1_gpio_write_bit_val;
	else
		write_bit = w1_gpio_write_bit_dir;

	spin_lock_irqsave(&w1_gpio_lock, irq_flags);
	write_bit(data, 0);
		/* minimum 480, max ? us
		 * be nice and sleep, except 18b20 spec lists 960us maximum,
		 * so until we can sleep with microsecond accuracy, spin.
		 * Feel free to come up with some other way to give up the
		 * cpu for such a short amount of time AND get it back in
		 * the maximum amount of time.
		 */
	w1_delay(48);
	write_bit(data, 1);

	w1_delay(7);

	tmp = __raw_readl(g_addr + 0x4);
	result = (tmp >> 1) & 0x1;

	/* re-check */
	w1_delay(5);

	tmp = __raw_readl(g_addr + 0x4);
	if (result)
		result = (tmp >> 1) & 0x1;

	/* minmum 70 (above) + 410 = 480 us
	 * There aren't any timing requirements between a reset and
	 * the following transactions.  Sleeping is safe here.
	 */
	/* w1_delay(410); min required time */
	w1_delay(40);

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

#if defined(CONFIG_OF)
static int of_w1_gpio_dt(struct device *dev, struct w1_gpio_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp;

	of_property_read_u32(np, "is_open_drain", &temp);
	pdata->is_open_drain = (unsigned int)temp;
	of_property_read_u32(np, "slave_speed", &temp);
	pdata->slave_speed = (unsigned int)temp;

	pdata->pin = of_get_named_gpio(np, "w1,gpio", 0);

        /* not used, get it from dt? */
        pdata->ext_pullup_enable_pin = -1;

	return 0;
}
#endif /* CONFIG_OF */

static int __init w1_gpio_probe(struct platform_device *pdev)
{
	struct w1_bus_master *master;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int err;

	pr_info("%s : start\n", __func__);
#if defined(CONFIG_OF)
	pdata = kzalloc(sizeof(struct w1_gpio_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENXIO;

	err = of_w1_gpio_dt(&pdev->dev, pdata);
	pdev->dev.platform_data = pdata;
#endif /* CONFIG_OF */
	if (!pdata)
		return -ENXIO;

	master = kzalloc(sizeof(struct w1_bus_master), GFP_KERNEL);
	if (!master)
		goto free_pdata;

	/* add for sending uevent */
	input = input_allocate_device();
	if (!input)
		goto free_master;

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

	err = gpio_request(pdata->pin, "w1");
	if (err)
		goto free_input;

	if (gpio_is_valid(pdata->ext_pullup_enable_pin)) {
		err = gpio_request_one(pdata->ext_pullup_enable_pin,
			  GPIOF_INIT_LOW, "w1 pullup");
		if (err < 0) {
			dev_err(&pdev->dev, "gpio_request_one "
				   "(ext_pullup_enable_pin) failed\n");
			goto free_gpio;
		}
	}

	master->data = pdata;
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
	g_addr = ioremap(BASE_ADDRESS, 0x10);
#endif

	if (pdata->is_open_drain) {
		gpio_direction_output(pdata->pin, 1);
		master->write_bit = w1_gpio_write_bit_val;
	} else {
		gpio_direction_input(pdata->pin);
		master->write_bit = w1_gpio_write_bit_dir;
	}

	err = w1_add_master_device(master);
	if (err)
		goto free_gpio_ext_pu;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	if (gpio_is_valid(pdata->ext_pullup_enable_pin))
		gpio_set_value(pdata->ext_pullup_enable_pin, 1);

	platform_set_drvdata(pdev, master);

	return 0;

free_gpio_ext_pu:
	if (gpio_is_valid(pdata->ext_pullup_enable_pin))
		gpio_free(pdata->ext_pullup_enable_pin);
free_gpio:
	gpio_free(pdata->pin);
free_input:
	kfree(input);
free_master:
	kfree(master);
free_pdata:
	kfree(pdata);

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

static int w1_gpio_resume(struct platform_device *pdev)
{
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	gpio_direction_output(pdata->pin, 1);

#ifdef CONFIG_W1_WORKQUEUE
	schedule_delayed_work(&w1_gdev->w1_dwork, HZ * 2);
#endif

	return 0;
}

#else
#define w1_gpio_suspend	NULL
#define w1_gpio_resume	NULL
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
		.name	= "w1_gpio_exynos",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= w1_gpio_dt_ids,
#endif /* CONFIG_OF */
	},
	.remove	= __exit_p(w1_gpio_remove),
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
