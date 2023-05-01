/* drivers/leds/richtek/leds-rt8547.c
 * Driver to Richtek RT8547 LED Flash IC
 *
 * Copyright (C) 2013 Richtek Technology Corporation
 * Author: CY Huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/gpio.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#endif  /* #ifdef CONFIG_DEBUG_FS */

#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/leds-rt8547.h>

#ifdef CONFIG_LEDS_RT8547_DBG
#define RT_DBG(fmt, args...) pr_info(fmt, ##args)
#else
#define RT_DBG(fmt, args...)
#endif

#define RT8547_ADDR	0x99
#define LONG_DELAY	9
#define SHORT_DELAY	4
#define START_DELAY	10
#define STOP_DELAY	1500

static struct rt8547_chip *g_chip_ptr = NULL;
//default set to initial value
static unsigned char reg_value[4] =
{
	0x00,    // Low Vin Protection 3.0V
	0x12,
	0x02,
	0x0f,
};

static inline int rt8547_send_bit(struct rt8547_platform_data *pdata, unsigned char bit)
{
	if (bit > 0)
	{
		gpio_set_value(pdata->flset_gpio, 0);
		udelay(SHORT_DELAY);
		gpio_set_value(pdata->flset_gpio, 1);
		udelay(LONG_DELAY);
	}
	else
	{
		gpio_set_value(pdata->flset_gpio, 0);
		udelay(LONG_DELAY);
		gpio_set_value(pdata->flset_gpio, 1);
		udelay(SHORT_DELAY);
	}
	return 0;
}

static inline int rt8547_send_byte(struct rt8547_platform_data *pdata, unsigned char byte)
{
	int i;
	//send order is high bit to low bit
	for (i=7; i>=0; i--)
		rt8547_send_bit(pdata, byte&(0x1<<i));
	return 0;
}

static inline int rt8547_send_special_byte(struct rt8547_platform_data *pdata, unsigned char byte)
{
	int i;
	//only send three bit for register address
	for (i=2; i>=0; i--)
		rt8547_send_bit(pdata, byte&(0x1<<i));
	return 0;
}

static inline int rt8547_start_xfer(struct rt8547_platform_data *pdata)
{
	gpio_set_value(pdata->flset_gpio, 1);
	udelay(START_DELAY);
	return 0;
}

static inline int rt8547_stop_xfer(struct rt8547_platform_data *pdata)
{
	//redundant 1 bit as the stop condition
	rt8547_send_bit(pdata, 1);
	udelay(STOP_DELAY);
	return 0;
}

static int rt8547_send_data(int reg, unsigned char data)
{
	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;
	unsigned long flags;
	unsigned char xfer_data[3]; // 0: adddr, 1: reg, 2: reg data
	xfer_data[0] = RT8547_ADDR;
	xfer_data[1] = (unsigned char)reg;
	xfer_data[2] = (unsigned char)data;
	RT_DBG("rt8547-> 0: 0x%02x, 1: 0x%02x, 2: 0x%02x\n", xfer_data[0], xfer_data[1], xfer_data[2]);
	spin_lock_irqsave(&g_chip_ptr->io_lock, flags);
	rt8547_start_xfer(pdata);
	rt8547_send_byte(pdata, xfer_data[0]);
	rt8547_send_special_byte(pdata, xfer_data[1]);
	rt8547_send_byte(pdata, xfer_data[2]);
	rt8547_stop_xfer(pdata);
	spin_unlock_irqrestore(&g_chip_ptr->io_lock, flags);

	//write back to reg array
	reg_value[reg-1] = data;

	return 0;
}

static inline int rt8547_control_en(int en)
{
	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;
	if (en > 0)
		gpio_set_value(pdata->ctl_gpio, 1);
	else
		gpio_set_value(pdata->ctl_gpio, 0);
	RT_DBG("%s: en = %d\n", __func__, en);

	return 0;
}

static inline int rt8547_flash_en(int en)
{
	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;
	if (en > 0)
		gpio_set_value(pdata->flen_gpio, 1);
	else
		gpio_set_value(pdata->flen_gpio, 0);
	RT_DBG("%s: en = %d\n", __func__, en);

	return 0;
}

static inline int rt8547_power_on(void)
{
/*	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;
	gpio_set_value(pdata->flset_gpio, 1);
*/
	return 0;
}

static inline int rt8547_power_off(void)
{
/*	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;
	gpio_set_value(pdata->flset_gpio, 0);
*/
	return 0;
}

int rt8547_open_handle(void)
{
	int ret = 0;
	RT_DBG("%s: trace\n", __func__);
	//check ref_cnt
	if (g_chip_ptr->ref_cnt == 0)
		g_chip_ptr->ref_cnt++;
	else
	{
		dev_err(g_chip_ptr->dev, "previous ref_cnt hasn't been released\n");
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(rt8547_open_handle);

int rt8547_release_handle(void)
{
	int ret = 0;
	RT_DBG("%s: trace\n", __func__);
	//check ref_cnt
	if (g_chip_ptr->ref_cnt > 0)
		g_chip_ptr->ref_cnt--;
	else
	{
		dev_err(g_chip_ptr->dev, "ref_cnt has reached to zero\n");
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(rt8547_release_handle);

int rt8547_set_led_low(void)
{
	int reg;
	unsigned char reg_val;
	RT_DBG("%s: trace\n", __func__);
	// always set ctlen low & flashen low first to ensure the state is really off
	rt8547_flash_en(0);
	rt8547_control_en(0);
	// set Low Vin Protection
	reg = 0x01;
	reg_val = reg_value[reg-1];
	rt8547_send_data(reg, reg_val);
	// set torch current & set torch mode
	reg = 0x03;
	reg_val = (reg_value[reg-1] | LED_MODE_MASK);
	rt8547_send_data(reg, reg_val);
	// set ctlen high & flashen high
	rt8547_control_en(1);
	rt8547_flash_en(1);
	return 0;
}
EXPORT_SYMBOL(rt8547_set_led_low);

int rt8547_set_led_high(void)
{
	int reg;
	unsigned char reg_val;
	RT_DBG("%s: trace\n", __func__);
	// always set ctlen low & flashen low first to ensure the state is really off
	rt8547_flash_en(0);
	rt8547_control_en(0);
	// set Low Vin Protection
	reg = 0x01;
	reg_val = reg_value[reg-1];
	rt8547_send_data(reg, reg_val);
	// set strobe current
	reg = 0x02;
	reg_val = reg_value[reg-1];
	rt8547_send_data(reg, reg_val);
	// set strobe mode
	reg = 0x03;
	reg_val = (reg_value[reg-1] & ~LED_MODE_MASK);
	rt8547_send_data(reg, reg_val);
	// set strobe timing to maximum time 0.5s
	reg = 0x04;
	reg_val = reg_value[reg-1];
	rt8547_send_data(reg, reg_val);
	// set ctlen high & flashen high
	rt8547_control_en(1);
	rt8547_flash_en(1);
	return 0;
}
EXPORT_SYMBOL(rt8547_set_led_high);

int rt8547_set_led_off(void)
{
	struct rt8547_platform_data *pdata = g_chip_ptr->dev->platform_data;

	RT_DBG("%s: trace\n", __func__);
	//set ctlen low & flashen low
	rt8547_flash_en(0);
	rt8547_control_en(0);
	gpio_set_value(pdata->flset_gpio, 0);
	return 0;
}
EXPORT_SYMBOL(rt8547_set_led_off);

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_rt_dent;
static struct dentry *debugfs_ctlen;
static struct dentry *debugfs_flen;
static struct dentry *debugfs_poke;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_pwr_suspend;

static unsigned char read_data;

static int reg_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t reg_debug_read(struct file *filp, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[8];

	if (!strcmp(access_str, "pwr_suspend"))
	{
		read_data = (unsigned char)g_chip_ptr->suspend;
	}
	else if (!strcmp(access_str, "peek"))
	{
		// do nothing
		// read_data has been writen after reg_debug_write;
	}
	else
		read_data = 0;

	snprintf(lbuf, sizeof(lbuf), "0x%02x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t reg_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	long int param[5];
	unsigned char data = 0;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strcmp(access_str, "poke"))
	{
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] >= 0x01 && param[0] <= 0x04) && (param[1] <= 0xFF) && (rc == 0))
		{
			data = (unsigned char)param[1];
			rt8547_send_data((int)param[0], data);
		}
		else
			rc = -EINVAL;
	}
	else if (!strcmp(access_str, "peek"))
	{
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] >= 0x01 && param[0] <=0x04) && (rc == 0))
		{
			read_data = reg_value[param[0]-1];
		}
		else
			rc = -EINVAL;
	}
	else if (!strcmp(access_str, "ctlen"))
	{
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xff) && (rc == 0))
		{
			if (param[0] == 1)
				rt8547_control_en(1);
			else if (param[0] == 0)
				rt8547_control_en(0);
			else
				return -EINVAL;
		}
		else
			rc = -EINVAL;
	}
	else if (!strcmp(access_str, "flashen"))
	{
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xff) && (rc == 0))
		{
			if (param[0] == 1)
				rt8547_flash_en(1);
			else if (param[0] == 0)
				rt8547_flash_en(0);
			else
				return -EINVAL;
		}
		else
			rc = -EINVAL;
	}
	else if (!strcmp(access_str, "pwr_suspend"))
	{
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xff) && (rc == 0))
		{
			if (param[0] == 1)
			{
				rt8547_power_off();
				g_chip_ptr->suspend = 1;
			}
			else if (param[0] == 0)
			{
				rt8547_power_on();
				g_chip_ptr->suspend = 0;
			}
			else
				return -EINVAL;
		}
	}

	if (rc == 0)
		rc = cnt;

	return rc;
}

static const struct file_operations reg_debug_ro_ops = {
	.open = reg_debug_open,
	.write = reg_debug_write,
};

static const struct file_operations reg_debug_rw_ops = {
	.open = reg_debug_open,
	.write = reg_debug_write,
	.read = reg_debug_read
};
#endif /* #ifdef CONFIG_DEBUG_FS */

static void __devexit rt8547_chip_io_deinit(struct rt8547_chip* chip)
{
	struct rt8547_platform_data *pdata = chip->dev->platform_data;
	gpio_direction_input(pdata->flen_gpio);
	gpio_free(pdata->flen_gpio);
	gpio_direction_input(pdata->ctl_gpio);
	gpio_free(pdata->ctl_gpio);
	gpio_direction_input(pdata->flset_gpio);
	gpio_free(pdata->flset_gpio);
}

static int __devinit rt8547_chip_io_init(struct rt8547_chip *chip)
{
	struct rt8547_platform_data *pdata = chip->dev->platform_data;
	int ret;

	if (!gpio_is_valid(pdata->flen_gpio) || !gpio_is_valid(pdata->ctl_gpio) || !gpio_is_valid(pdata->flset_gpio))
	{
		dev_err(chip->dev, "not valid gpio for one of three\n");
		return -1;
	}
	else
	{
		ret = gpio_request_one(pdata->flen_gpio, GPIOF_OUT_INIT_LOW, "rt8547_flen");
		if (ret < 0)
		{
			dev_err(chip->dev, "request flen gpio fail\n");
			goto request_gpio1_fail;
		}

		ret = gpio_request_one(pdata->ctl_gpio, GPIOF_OUT_INIT_LOW, "rt8547_ctl");
		if (ret < 0)
		{
			dev_err(chip->dev, "request ctl gpio fail\n");
			goto request_gpio2_fail;
		}

		ret = gpio_request_one(pdata->flset_gpio, GPIOF_OUT_INIT_LOW, "rt8547_flset");
		if (ret < 0)
		{
			dev_err(chip->dev, "request flset gpio fail\n");
			goto request_gpio3_fail;
		}
	}

	return 0;

request_gpio3_fail:
	gpio_free(pdata->ctl_gpio);
request_gpio2_fail:
	gpio_free(pdata->flen_gpio);
request_gpio1_fail:
	return ret;
}

static int __devinit rt8547_chip_reg_init(struct rt8547_chip *chip)
{
	int reg;
	struct rt8547_platform_data *pdata = chip->dev->platform_data;
	// strobe current
	reg = 0x02;
	reg_value[reg-1] = ((reg_value[reg-1]&~STROBE_CURRENT_MASK)|pdata->strobe_current);
	// torch current
	reg = 0x03;
	reg_value[reg-1] = ((reg_value[reg-1]&~TORCH_CURRENT_MASK)|pdata->torch_current);
	// strobe timing
	reg = 0x04;
	reg_value[reg-1] = ((reg_value[reg-1]&~STROBE_TIMING_MASK)|pdata->strobe_timing);

	return 0;
}

static int __devinit rt8547_led_probe(struct platform_device *pdev)
{
	struct rt8547_platform_data *pdata = pdev->dev.platform_data;
	struct rt8547_chip *chip;
	int ret = 0;

	if (!pdata)
		return -EINVAL;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	spin_lock_init(&chip->io_lock);

	ret = rt8547_chip_io_init(chip);
	if (ret<0)
	{
		goto IO_INIT_FAIL;
	}

	g_chip_ptr = chip;
	rt8547_power_on();

	ret = rt8547_chip_reg_init(chip);
	if (ret<0)
	{
		goto REG_INIT_FAIL;
	}


	#ifdef CONFIG_DEBUG_FS
	dev_info(chip->dev, "add debugfs for rt8547 debug\n");
	debugfs_rt_dent = debugfs_create_dir("rt8547_dbg", 0);
	if (!IS_ERR(debugfs_rt_dent)) {
		debugfs_ctlen = debugfs_create_file("ctlen",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "ctlen", &reg_debug_ro_ops);

		debugfs_flen = debugfs_create_file("flashen",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "flashen", &reg_debug_ro_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "poke", &reg_debug_ro_ops);

		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "peek", &reg_debug_rw_ops);

		debugfs_pwr_suspend = debugfs_create_file("pwr_suspend",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "pwr_suspend", &reg_debug_rw_ops);
	}
	#endif  /* #ifdef CONFIG_DEBUG_FS */

	dev_set_drvdata(chip->dev, chip);
	pr_info("RT8547 LED Driver successfully load\n");
	return 0;

REG_INIT_FAIL:
	g_chip_ptr = NULL;
	rt8547_chip_io_deinit(chip);
IO_INIT_FAIL:
	kfree(chip);
	return ret;
}

static int __devexit rt8547_led_remove(struct platform_device *pdev)
{
	struct rt8547_chip *chip = dev_get_drvdata(&pdev->dev);

	#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_ctlen);
	debugfs_remove(debugfs_flen);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_pwr_suspend);
	debugfs_remove(debugfs_rt_dent);
	#endif /* #ifdef CONFIG_DEBUG_FS */
	g_chip_ptr = NULL;
	rt8547_chip_io_deinit(chip);
	kfree(chip);

	return 0;
}

static int rt8547_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rt8547_chip *chip = dev_get_drvdata(&pdev->dev);
	if (chip->ref_cnt == 0)
	{
		rt8547_power_off();
		chip->suspend = 1;
	}
	return 0;
}

static int rt8547_led_resume(struct platform_device *pdev)
{
	struct rt8547_chip *chip = dev_get_drvdata(&pdev->dev);
	if (chip->ref_cnt == 0)
	{
		rt8547_power_on();
		chip->suspend = 0;
	}
	return 0;
}

static struct platform_driver rt8547_led_driver = {
	.probe = rt8547_led_probe,
	.remove = __devexit_p(rt8547_led_remove),
	.suspend = rt8547_led_suspend,
	.resume = rt8547_led_resume,
	.driver = {
		.name = "leds-rt8547",
		.owner = THIS_MODULE,
	},
};

static int  __init rt8547_chip_init(void)
{
	return platform_driver_register(&rt8547_led_driver);
}
module_init(rt8547_chip_init);

static void __exit rt8547_chip_exit(void)
{
	platform_driver_unregister(&rt8547_led_driver);
}
module_exit(rt8547_chip_exit);

MODULE_LICENSE(GPL);
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Richtek RT8547 LED driver");
MODULE_ALIAS("platform:leds-rt8547");
