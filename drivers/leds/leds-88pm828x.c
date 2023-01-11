/*
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Guoqing Li <ligq@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-88pm828x.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define MODULE_NAME "leds_88pm828x"

static unsigned trace_suspend = 1;
module_param(trace_suspend, uint, 0664);
#define printk_suspend(fmt, args...)                 \
({                                                   \
	if (trace_suspend)                           \
		pr_info(fmt, ##args);                \
})

#ifdef PM828X_DEBUG
static unsigned trace_brightness;
module_param(trace_brightness, uint, 0664);
#define printk_br(fmt, args...)                     \
({                                                  \
	if (trace_brightness)                       \
		pr_info(fmt, ##args);               \
})

static unsigned trace_write;
module_param(trace_write, uint, 0664);
#define printk_write(fmt, args...)               \
({                                               \
	if (trace_write)                         \
		pr_info(fmt, ##args);            \
})
#else
#define printk_br(fmt, args...)
#define printk_write(fmt, args...)
#endif

struct pm828x_data {
	struct i2c_client *client;
	struct pm828x_platform_data *pdata;
	struct led_classdev led_dev;
	u8 revision;
	atomic_t suspended;
	u32 bvalue;		/* Current brightness register value */
};

static DEFINE_MUTEX(pm828x_mutex);

struct pm828x_reg {
	const char *name;
	uint8_t reg;
} pm828x_regs[] = {
	{
	"CHIP_ID_REG", PM828X_CHIP_ID}, {
	"REV_REG", PM828X_REV_ID}, {
	"STATUS_REG", PM828X_STATUS}, {
	"STATUS_FLAG_REG", PM828X_STATUS_FLAG}, {
	"IDAC_CTRL0_REG", PM828X_IDAC_CTRL0}, {
	"IDAC_CTRL1_REG", PM828X_IDAC_CTRL1}, {
	"IDAC_RAMP_CLK_REG", PM828X_IDAC_RAMP_CLK_CTRL0}, {
	"MISC_CTRL_REG", PM828X_MISC_CTRL}, {
	"IDAC_OUT_CURRENT0_REG", PM828X_IDAC_OUT_CURRENT0}, {
"IDAC_OUT_CURRENT1_REG", PM828X_IDAC_OUT_CURRENT1},};

#ifdef PM828X_DEBUG
static const char *pm828x_reg_name(struct pm828x_data *driver_data, int reg)
{
	unsigned reg_count;
	int i;

	reg_count = sizeof(pm828x_regs) / sizeof(pm828x_regs[0]);
	for (i = 0; i < reg_count; i++)
		if (reg == pm828x_regs[i].reg)
			return pm828x_regs[i].name;

	return "UNKNOWN";
}
#endif

static int pm828x_read_reg(struct i2c_client *client,
			   unsigned reg, uint8_t *value)
{
	uint8_t buf[1];
	int ret = 0;

	if (!value)
		return -EINVAL;
	buf[0] = reg;
	ret = i2c_master_send(client, buf, 1);
	if (ret > 0) {
		msleep_interruptible(1);
		ret = i2c_master_recv(client, buf, 1);
		if (ret > 0)
			*value = buf[0];
	}
	return ret;
}

static int pm828x_write_reg(struct i2c_client *client,
			    unsigned reg, uint8_t value, const char *caller)
{
	uint8_t buf[2] = { reg, value };
	int ret = 0;
#ifdef PM828X_DEBUG
	struct pm828x_data *driver_data = i2c_get_clientdata(client);

	printk_write("%s: reg 0x%X (%s) = 0x%x\n", caller, buf[0],
		     pm828x_reg_name(driver_data, reg), buf[1]);
#endif
	ret = i2c_master_send(client, buf, 2);
	if (ret < 0)
		pr_err("%s: i2c_master_send error %d\n", caller, ret);
	return ret;
}

static void pm828x_brightness_set(struct led_classdev *led_cdev,
				  enum led_brightness value)
{
	struct pm828x_data *driver_data =
	    container_of(led_cdev, struct pm828x_data, led_dev);
	struct i2c_client *client = driver_data->client;
	u32 cur;
	u8 val;

	if (driver_data->bvalue == value)
		return;

	cur = 0xA00 * value / LED_FULL;
	mutex_lock(&pm828x_mutex);
	/* set lower 8-bit of the current */
	val = cur & 0xFF;
	pm828x_write_reg(client, PM828X_IDAC_CTRL0, val, __func__);

	/* select ramp mode and upper 4-bit of the current */
	pm828x_read_reg(client, PM828X_IDAC_CTRL1, &val);
	val &= ~0xF;
	val |= ((cur >> 8) & 0xF);
	pm828x_write_reg(client, PM828X_IDAC_CTRL1, val, __func__);

	printk_br("%s:0x%x => 0x%x\n", __func__, driver_data->bvalue, value);
	driver_data->bvalue = value;
	mutex_unlock(&pm828x_mutex);
}

static enum led_brightness pm828x_brightness_get(struct led_classdev *led_cdev)
{
	struct pm828x_data *driver_data =
	    container_of(led_cdev, struct pm828x_data, led_dev);

	return driver_data->bvalue * LED_FULL / 0xA00;
}

static ssize_t pm828x_registers_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent,
						 struct i2c_client, dev);
	struct pm828x_data *driver_data = i2c_get_clientdata(client);
	int reg_count = sizeof(pm828x_regs) / sizeof(pm828x_regs[0]);
	int i, n = 0;
	uint8_t value;

	if (atomic_read(&driver_data->suspended)) {
		pr_info("%s: can't read: driver suspended\n",
		       __func__);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "Unable to read 88PM828X registers: driver suspended\n");
	} else {
		pr_info("%s: reading registers\n", __func__);
		for (i = 0, n = 0; i < reg_count; i++) {
			pm828x_read_reg(client, pm828x_regs[i].reg, &value);
			n += scnprintf(buf + n, PAGE_SIZE - n,
				       "%-20s (0x%x) = 0x%02X\n",
				       pm828x_regs[i].name, pm828x_regs[i].reg,
				       value);
		}
	}

	return n;
}

static ssize_t pm828x_registers_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
						 dev);
	struct pm828x_data *driver_data = i2c_get_clientdata(client);
	unsigned reg;
	unsigned value;

	if (atomic_read(&driver_data->suspended)) {
		pr_info("%s: can't write: driver suspended\n",
		       __func__);
		return -ENODEV;
	}
	sscanf(buf, "%x %x", &reg, &value);
	if (value > 0xFF)
		return -EINVAL;
	pr_info("%s: writing reg 0x%x = 0x%x\n", __func__, reg, value);
	mutex_lock(&pm828x_mutex);
	pm828x_write_reg(client, reg, (uint8_t) value, __func__);
	mutex_unlock(&pm828x_mutex);

	return size;
}

static DEVICE_ATTR(registers, 0664, pm828x_registers_show,
		   pm828x_registers_store);

static ssize_t pm828x_pwm_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent,
						 struct i2c_client, dev);
	struct pm828x_data *driver_data = i2c_get_clientdata(client);
	uint8_t value;
	uint8_t pwm_mask = 0x4;
	int ret;

	if (atomic_read(&driver_data->suspended)) {
		pr_info("%s: unable to read pwm: driver suspended\n",
		       __func__);
		return -ENODEV;
	}
	mutex_lock(&pm828x_mutex);
	ret = pm828x_read_reg(client, PM828X_MISC_CTRL, &value);
	mutex_unlock(&pm828x_mutex);
	if (ret < 0) {
		pr_err("%s: unable to read PWM register: %d\n",
		       __func__, ret);
		return ret;
	}
	sprintf(buf, "%d\n", (value & pwm_mask) >> 6);
	return strlen(buf) + 1;
}

static ssize_t pm828x_pwm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
						 dev);
	struct pm828x_data *driver_data = i2c_get_clientdata(client);
	unsigned pwm_value;
	uint8_t value;
	uint8_t pwm_mask = 1 << 6;
	int ret;

	if (atomic_read(&driver_data->suspended)) {
		pr_info("%s: unable to write pwm: driver suspended\n",
		       __func__);
		return -ENODEV;
	}
	sscanf(buf, "%d", &pwm_value);
	if (pwm_value != 0 && pwm_value != 1) {
		pr_err("%s: invalid value %d, should be 0 or 1\n",
		       __func__, pwm_value);
		return -EINVAL;
	}
	pwm_value = pwm_value << 6;	/* Bit 6 is PWM enable bit */
	mutex_lock(&pm828x_mutex);
	ret = pm828x_read_reg(client, PM828X_MISC_CTRL, &value);
	if (ret < 0) {
		pr_err("%s: unable to read PWM register: %d\n",
		       __func__, ret);
		mutex_unlock(&pm828x_mutex);
		return ret;
	}
	/* If PWM value is the same as requested do nothing */
	if ((value & pwm_mask) == pwm_value)
		pr_info("%s: %d, PWM value is as requested; nothing to do\n", __func__,
			pwm_value >> 2);
	else {
		/*
		 * Now we just need to change the PWM enable bit to
		 * the opposite of what it was
		 */
		pr_info("%s: %s PWM; CTRL_A_PWM = 0x%x\n",
		       __func__, pwm_value ? "enabling" : "disabling",
		       value ^ pwm_mask);
		pm828x_write_reg(client, PM828X_MISC_CTRL, value ^ pwm_mask,
				 __func__);
	}
	mutex_unlock(&pm828x_mutex);

	return size;
}

static DEVICE_ATTR(pwm, 0664, pm828x_pwm_show, pm828x_pwm_store);

#ifdef CONFIG_OF
static void pm828x_power(struct i2c_client *client, int on)
{
	static struct regulator *bl_avdd;
	struct device_node *np = client->dev.of_node;
	int ret = 0;
	const void *avdd_en = NULL;

	avdd_en = of_get_property(np, "avdd-supply", 0);
	if (avdd_en && !bl_avdd) {
		bl_avdd = regulator_get(&client->dev, "avdd");
		if (IS_ERR(bl_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			return;
		}
	}

	if (on) {
		if (avdd_en) {
			regulator_set_voltage(bl_avdd, 2800000, 2800000);
			ret = regulator_enable(bl_avdd);
		}

		if (!avdd_en || ret < 0)
			bl_avdd = NULL;

	} else if (avdd_en)
			regulator_disable(bl_avdd);
}
#else
static void pm828x_power(struct i2c_client *client, int on)
{
}
#endif

static void pm828x_configure(struct pm828x_data *driver_data)
{
	u8 val;

	/* set lower 8-bit of the current */
	val = driver_data->pdata->idac_current & 0xFF;
	pm828x_write_reg(driver_data->client, PM828X_IDAC_CTRL0, val, __func__);

	/* select ramp mode and upper 4-bit of the current */
	val = (driver_data->pdata->ramp_mode << 5) |
	    ((driver_data->pdata->idac_current >> 8) & 0xF);
	pm828x_write_reg(driver_data->client, PM828X_IDAC_CTRL1, val, __func__);

	/* select ramp clk */
	val = driver_data->pdata->ramp_clk;
	pm828x_write_reg(driver_data->client,
			 PM828X_IDAC_RAMP_CLK_CTRL0, val, __func__);

	/* set string configuration */
	val = (driver_data->pdata->str_config << 1) |
	    (driver_data->pdata->str_config ? 0 : 1);
	/*
	 * FIXME: enable shut down mode, pwm input and sleep mode.
	 * shutdown mode maybe cound not enter,
	 * most time the i2c bus were shared and pulled high.
	 * Fortunately, we could call power on/off instead.
	 */
	val |= PM828X_EN_SHUTDN | PM828X_EN_PWM | PM828X_EN_SLEEP;
	pm828x_write_reg(driver_data->client, PM828X_MISC_CTRL, val, __func__);
}

static int pm828x_setup(struct pm828x_data *driver_data)
{
	int ret;
	u8 value;

	ret = pm828x_read_reg(driver_data->client, PM828X_CHIP_ID, &value);
	if (ret < 0) {
		pr_err("%s: unable to read chip id: %d\n",
		       __func__, ret);
		return ret;
	}
	if (value != PM828X_ID) {
		pr_err("%s: 88pm828x not detected\n", __func__);
		return -EINVAL;
	}

	ret = pm828x_read_reg(driver_data->client, PM828X_REV_ID, &value);
	if (ret < 0) {
		pr_err("%s: unable to read from chip: %d\n",
		       __func__, ret);
		return ret;
	}
	driver_data->revision = value;
	pr_info("%s: revision 0x%X\n", __func__,
	       driver_data->revision);

	pm828x_configure(driver_data);
	driver_data->bvalue = driver_data->pdata->idac_current;

	return ret;
}

#ifdef CONFIG_OF
static int pm828x_probe_dt(struct i2c_client *client)
{
	struct pm828x_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	client->dev.platform_data = pdata;

	if (of_property_read_u32(np, "ramp_mode", &pdata->ramp_mode)) {
		dev_err(&client->dev, "failed to get ramp_mode property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "idac_current", &pdata->idac_current)) {
		dev_err(&client->dev, "failed to get idac_current property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "ramp_clk", &pdata->ramp_clk)) {
		dev_err(&client->dev, "failed to get ramp_clk_mode property\n");
		return -EINVAL;
	}
	if (of_property_read_u32(np, "str_config", &pdata->str_config)) {
		dev_err(&client->dev, "failed to str_config property\n");
		return -EINVAL;
	}

	return 0;
}
#endif

static int pm828x_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct pm828x_platform_data *pdata;
	struct pm828x_data *driver_data;

	pr_info("%s: enter, I2C address = 0x%x, flags = 0x%x\n",
	       __func__, client->addr, client->flags);

#ifdef CONFIG_OF
	ret = pm828x_probe_dt(client);
	if (ret == -ENOMEM) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for platform_data\n",
			__func__);
		return ret;
	} else if (ret == -EINVAL) {
		kfree(client->dev.platform_data);
		dev_err(&client->dev,
			"%s: Probe device tree data failed\n", __func__);
		return ret;
	}
#endif
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -EINVAL;
	}
	/* We should be able to read and write byte data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENOTSUPP;
	}
	driver_data = kzalloc(sizeof(struct pm828x_data), GFP_KERNEL);
	if (driver_data == NULL) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset(driver_data, 0, sizeof(*driver_data));

	driver_data->client = client;
	driver_data->pdata = pdata;
	if (driver_data->pdata->idac_current > 0xA00)
		driver_data->pdata->idac_current = 0xA00;

	i2c_set_clientdata(client, driver_data);

	/* Initialize chip */
	if (pdata->init)
		pdata->init();

	if (pdata->power_on)
		pdata->power_on();
	pm828x_power(client, 1);

	ret = pm828x_setup(driver_data);
	if (ret < 0)
		goto setup_failed;

	/* Register LED class */
	driver_data->led_dev.name = PM828X_LED_NAME;
	driver_data->led_dev.brightness_set = pm828x_brightness_set;
	driver_data->led_dev.brightness_get = pm828x_brightness_get;
	ret = led_classdev_register(&client->dev, &driver_data->led_dev);
	if (ret) {
		pr_err("%s: led_classdev_register %s failed: %d\n",
		       __func__, PM828X_LED_NAME, ret);
		goto led_register_failed;
	}

	atomic_set(&driver_data->suspended, 0);

	ret = device_create_file(driver_data->led_dev.dev, &dev_attr_registers);
	if (ret) {
		pr_err("%s: unable to create registers device file for %s: %d\n", __func__,
		       PM828X_LED_NAME, ret);
		goto device_create_file_failed;
	}

	ret = device_create_file(driver_data->led_dev.dev, &dev_attr_pwm);
	if (ret) {
		pr_err("%s: unable to create pwm device file for %s: %d\n", __func__,
		       PM828X_LED_NAME, ret);
		goto device_create_file_failed1;
	}
	dev_set_drvdata(&client->dev, driver_data);

	pm_runtime_enable(&client->dev);
	pm_runtime_forbid(&client->dev);
	return 0;

device_create_file_failed1:
	device_remove_file(driver_data->led_dev.dev, &dev_attr_registers);
device_create_file_failed:
	led_classdev_unregister(&driver_data->led_dev);
led_register_failed:
setup_failed:
#ifdef CONFIG_OF
	kfree(driver_data->pdata);
#endif
	kfree(driver_data);
	return ret;
}

static int pm828x_remove(struct i2c_client *client)
{
	struct pm828x_data *driver_data = i2c_get_clientdata(client);

	pm_runtime_disable(&client->dev);

	device_remove_file(driver_data->led_dev.dev, &dev_attr_registers);
	device_remove_file(driver_data->led_dev.dev, &dev_attr_pwm);
	led_classdev_unregister(&driver_data->led_dev);
#ifdef CONFIG_OF
	kfree(driver_data->pdata);
#endif
	kfree(driver_data);
	return 0;
}

static const struct i2c_device_id pm828x_id[] = {
	{PM828X_NAME, 0},
	{}
};

#ifdef CONFIG_PM_RUNTIME
static int pm828x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct pm828x_data *driver_data = i2c_get_clientdata(client);

	printk_suspend("%s: called with pm message %d\n", __func__, mesg.event);

	atomic_set(&driver_data->suspended, 1);
	led_classdev_suspend(&driver_data->led_dev);
	if (driver_data->pdata->power_off)
		driver_data->pdata->power_off();
	pm828x_power(client, 0);

	return 0;
}

static int pm828x_resume(struct i2c_client *client)
{
	struct pm828x_data *driver_data = i2c_get_clientdata(client);

	printk_suspend("%s: resuming\n", __func__);
	if (driver_data->pdata->power_on)
		driver_data->pdata->power_on();
	pm828x_power(client, 1);

	mutex_lock(&pm828x_mutex);
	pm828x_configure(driver_data);
	mutex_unlock(&pm828x_mutex);
	atomic_set(&driver_data->suspended, 0);
	led_classdev_resume(&driver_data->led_dev);

	printk_suspend("%s: driver resumed\n", __func__);

	return 0;
}

static int pm828x_runtime_suspend(struct device *dev)
{
	struct pm828x_data *driver_data = dev_get_drvdata(dev);

	return pm828x_suspend(driver_data->client, PMSG_SUSPEND);
}

static int pm828x_runtime_resume(struct device *dev)
{
	struct pm828x_data *driver_data = dev_get_drvdata(dev);

	return pm828x_resume(driver_data->client);
}
#else
static int pm828x_runtime_suspend(struct device *dev)
{
	return 0;
}
static int pm828x_runtime_resume(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static UNIVERSAL_DEV_PM_OPS(pm828x_pm_ops, pm828x_runtime_suspend,
			    pm828x_runtime_resume, NULL);

static struct of_device_id pm828x_dt_ids[] = {
	{.compatible = "marvell,88pm828x",},
	{}
};

/* This is the I2C driver that will be inserted */
static struct i2c_driver pm828x_driver = {
	.driver = {
		   .name = PM828X_NAME,
		   .pm = &pm828x_pm_ops,
		   .of_match_table = of_match_ptr(pm828x_dt_ids),
		   },
	.id_table = pm828x_id,
	.probe = pm828x_probe,
	.remove = pm828x_remove,
};

static int __init pm828x_init(void)
{
	int ret;

	pr_info("%s: enter\n", __func__);
	ret = i2c_add_driver(&pm828x_driver);
	if (ret)
		pr_err("%s: i2c_add_driver failed, error %d\n",
		       __func__, ret);

	return ret;
}

static void __exit pm828x_exit(void)
{
	i2c_del_driver(&pm828x_driver);
}

module_init(pm828x_init);
module_exit(pm828x_exit);

MODULE_DESCRIPTION("88pm828x DISPLAY BACKLIGHT DRIVER");
MODULE_AUTHOR("Guoqing Li, ligq@marvell.com");
MODULE_LICENSE("GPL v2");
