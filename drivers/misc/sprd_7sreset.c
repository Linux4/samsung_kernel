/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci_glb_regs.h>

static inline int pbint_7s_rst_disable(int disable)
{
	if (disable) {
		sci_adi_set(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_DISABLE);
	} else {
		sci_adi_clr(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_DISABLE);
	}
	return 0;
}

static inline int pbint_7s_rst_is_disable(void)
{
	return ! !(sci_adi_read(ANA_REG_GLB_POR_7S_CTRL) &
		   BIT_PBINT_7S_RST_DISABLE);
}

static inline int pbint_7s_rst_set_keymode(int mode)
{
	if (!mode) {
		sci_adi_set(ANA_REG_GLB_SWRST_CTRL, BIT_KEY2_7S_RST_EN);
	} else {
		sci_adi_clr(ANA_REG_GLB_SWRST_CTRL, BIT_KEY2_7S_RST_EN);
	}
	return 0;
}

static inline int pbint_7s_rst_get_keymode(void)
{
	return !(sci_adi_read(ANA_REG_GLB_SWRST_CTRL) & BIT_KEY2_7S_RST_EN);	//0 is 2Key mode
}

static inline int pbint_7s_rst_set_resetmode(int mode)
{
	if (mode) {
		sci_adi_set(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_MODE);	//hardreset
	} else {
		sci_adi_clr(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_MODE);
	}
	return 0;
}

static inline int pbint_7s_rst_get_resetmode(void)
{
	return ! !(sci_adi_read(ANA_REG_GLB_POR_7S_CTRL) &
		   BIT_PBINT_7S_RST_MODE);
}

static inline int pbint_7s_rst_set_shortmode(int mode)
{
	if (mode) {
		sci_adi_set(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_SWMODE);
	} else {
		sci_adi_clr(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_SWMODE);
	}
	return 0;
}

static inline int pbint_7s_rst_get_shortmode(void)
{
	return ! !(sci_adi_read(ANA_REG_GLB_POR_7S_CTRL) & BIT_PBINT_7S_RST_SWMODE);	// 1 is short
}

static inline int pbint_7s_rst_set_threshold(unsigned int th)
{
	int shft = __ffs(BITS_PBINT_7S_RST_THRESHOLD(~0));
	unsigned int max = BITS_PBINT_7S_RST_THRESHOLD(~0) >> shft;

	if (th > 0)
		th--;

	if (th > max)
		th = max;

	sci_adi_write(ANA_REG_GLB_POR_7S_CTRL, BITS_PBINT_7S_RST_THRESHOLD(th),
		      BITS_PBINT_7S_RST_THRESHOLD(~0));
	return 0;
}

static inline int pbint_7s_rst_get_threshold(void)
{
	int shft = __ffs(BITS_PBINT_7S_RST_THRESHOLD(~0));
	return ((sci_adi_read(ANA_REG_GLB_POR_7S_CTRL) &
		 BITS_PBINT_7S_RST_THRESHOLD(~0)) >> shft) + 1;
}

static ssize_t sprd_7sreset_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);
static ssize_t sprd_7sreset_show(struct device *dev,
				 struct device_attribute *attr, char *buf);

#define SPRD_7SRESET_ATTR(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP, },  \
	.show = sprd_7sreset_show,                  \
	.store = sprd_7sreset_store,                              \
}
#define SPRD_7SRESET_ATTR_RO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO, },  \
	.show = sprd_7sreset_show,                  \
}
#define SPRD_7SRESET_ATTR_WO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IWUSR | S_IWGRP, },  \
	.store = sprd_7sreset_store,                              \
}

static struct device_attribute sprd_7sreset_attr[] = {
	SPRD_7SRESET_ATTR(module_disable),
	SPRD_7SRESET_ATTR(hard_mode),
	SPRD_7SRESET_ATTR(short_mode),
	SPRD_7SRESET_ATTR(keypad_mode),
	SPRD_7SRESET_ATTR(threshold_time),
};

enum {
	MODULE_DISABLE = 0,
	HARD_MODE,
	SHORT_MODE,
	KEYPAD_MODE,
	THRESHOLD_TIME,
};

static ssize_t sprd_7sreset_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;
	const ptrdiff_t off = attr - sprd_7sreset_attr;

	set_value = simple_strtoul(buf, NULL, 10);
	pr_info("sprd_7sreset_store value %d %lu\n", (int)off, set_value);

	switch (off) {
	case MODULE_DISABLE:
		pbint_7s_rst_disable(set_value);
		break;
	case HARD_MODE:
		pbint_7s_rst_set_resetmode(set_value);
		break;
	case SHORT_MODE:
		pbint_7s_rst_set_shortmode(set_value);
		break;
	case KEYPAD_MODE:
		pbint_7s_rst_set_keymode(set_value);
		break;
	case THRESHOLD_TIME:
		pbint_7s_rst_set_threshold(set_value);
		break;
	default:
		count = -EINVAL;
		break;
	}

	return count;
}

static ssize_t sprd_7sreset_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - sprd_7sreset_attr;
	int value;

	pr_info("sprd_7sreset_show value %d\n", (int)off);
	switch (off) {
	case MODULE_DISABLE:
		value = pbint_7s_rst_is_disable();
		break;
	case HARD_MODE:
		value = pbint_7s_rst_get_resetmode();
		break;
	case SHORT_MODE:
		value = pbint_7s_rst_get_shortmode();
		break;
	case KEYPAD_MODE:
		value = pbint_7s_rst_get_keymode();
		break;
	case THRESHOLD_TIME:
		value = pbint_7s_rst_get_threshold();
		break;
	default:
		i = -EINVAL;
		return i;
	}

	i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value);
	return i;
}

static int sprd_device_delete_attributes(struct device *dev,
					 struct device_attribute *dev_attrs,
					 int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		device_remove_file(dev, &dev_attrs[i]);
	}

	return 0;
}

static int sprd_device_create_attributes(struct device *dev,
					 struct device_attribute *dev_attrs,
					 int len)
{
	int i = 0, rc = 0;

	for (i = 0; i < len; i++) {
		rc = device_create_file(dev, &dev_attrs[i]);
		if (rc)
			break;
	}

	if (rc) {
		for (; i >= 0; --i)
			device_remove_file(dev, &dev_attrs[i]);
	}

	return rc;
}

static struct miscdevice sprd_7sreset_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_7sreset",
	.fops = NULL,
};

static int __init sprd_7sreset_init(void)
{
	int ret;
	ret = misc_register(&sprd_7sreset_miscdev);
	if (ret)
		return ret;

	ret =
	    sprd_device_create_attributes(sprd_7sreset_miscdev.this_device,
					  sprd_7sreset_attr,
					  ARRAY_SIZE(sprd_7sreset_attr));
	if (ret) {
		pr_err("%s failed to create device attributes.\n", __func__);
		return ret;
	}

	return ret;
}

static void __exit sprd_7sreset_exit(void)
{
	sprd_device_delete_attributes(sprd_7sreset_miscdev.this_device,
				      sprd_7sreset_attr,
				      ARRAY_SIZE(sprd_7sreset_attr));
	misc_deregister(&sprd_7sreset_miscdev);
}

module_init(sprd_7sreset_init);
module_exit(sprd_7sreset_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum 7SReset Driver");
MODULE_AUTHOR("Mingwei <Mingwei.Zhang@spreadtrum.com>");
MODULE_VERSION("0.1");
