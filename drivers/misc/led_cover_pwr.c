#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#include <linux/sec_sysfs.h>

static struct device *cover_dev;
static int gpio;

static ssize_t show_cover_pwr_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int tmp = gpio_get_value(gpio);
	return sprintf(buf, "%d\n", tmp);
}

static ssize_t store_cover_pwr_en(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	if (gpio_is_valid(gpio)) {
		if(!strncasecmp(buf, "VAL_OFF", 7))
			gpio_set_value(gpio, 0);
		else if(!strncasecmp(buf, "VAL_ON", 6))
			gpio_set_value(gpio, 1);
		else
			return -EINVAL;
	} else {
		dev_err(dev, "gpio_is_invalid\n");
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR(cover_pwr, 0660, show_cover_pwr_en, store_cover_pwr_en);

static struct attribute *sec_cover_pwr_attributes[] = {
	&dev_attr_cover_pwr.attr,
	NULL,
};

static struct attribute_group sec_cover_pwr_attr_group = {
	.attrs = sec_cover_pwr_attributes,
};

static int __init cover_pwr_init(void)
{
	struct device_node *np;
	int ret;

	np = of_find_node_by_path("/led_cover");
	if (!np) {
		pr_info("%s: Property 'led_power' missing or invalid\n", __func__ );
		return -EINVAL;
	}

	gpio = of_get_named_gpio(np, "ledcover_en,gpio", 0);
	if (gpio < 0) {
		pr_info("%s: fail to get ledcover_en \n", __func__ );
		return -EINVAL;
	}

	ret = gpio_request(gpio, "ledcover_en");
	if (ret) {
		pr_info("%s: Failed to request gpio \n", __func__ );
		return -EINVAL;
	}

	cover_dev = sec_device_create(NULL, "ledcover");
	if (IS_ERR(cover_dev)) {
		pr_info("%s: Failed to create device for\
			samsung specific led cover\n", __func__ );
		return -ENODEV;
	}

	ret = sysfs_create_group(&cover_dev->kobj, &sec_cover_pwr_attr_group);
	if (ret) {
		pr_info("%s: Failed to create device for\
			samsung specific led cover\n", __func__ );
		goto exit_sysfs;
	}

	return 0;
exit_sysfs:
	sec_device_destroy(cover_dev->devt);
	return -ENODEV;
}

device_initcall(cover_pwr_init);

