/*
 * leds-s2mf301.c - LED class driver for S2MF301 FLASH LEDs.
 *
 * Copyright (C) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mfd/samsung/s2mf301.h>
#include <linux/leds-s2mf301.h>
#include <linux/platform_device.h>

struct s2mf301_fled_data *g_fled_data;

extern void pdo_ctrl_by_flash(bool mode);

static char *s2mf301_fled_mode_string[] = {
	"OFF",
	"TORCH",
	"FLASH",
};

static char *s2mf301_fled_operating_mode_string[] = {
	"AUTO",
	"BOOST",
	"TA",
	"SYS",
};

/* IC current limit */
static int s2mf301_fled_torch_curr_max[] = {
	0, 320
};

/* IC current limit */
static int s2mf301_fled_flash_curr_max[] = {
	0, 1600
};

#if IS_ENABLED(DEBUG_TEST_READ)
static void s2mf301_fled_test_read(struct s2mf301_fled_data *fled)
{
	u8 data;
	char str[1016] = {0,};
	int i;
	struct i2c_client *i2c = fled->i2c;

	s2mf301_read_reg(i2c, 0x09, &data);
	pr_err("%s: %s0x09:0x%02x\n", __func__, str, data);

	for (i = 0x10; i <= 0x11; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x12; i <= 0x14; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x15; i <= 0x17; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
}
#endif

static int s2mf301_fled_get_flash_curr(struct s2mf301_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL0;
		break;
	}

	s2mf301_read_reg(fled->i2c, dest, &data);

	data = data & S2MF301_CHX_FLASH_IOUT;
	curr = (data * 50) + 50;

	pr_info("%s: CH%02d flash curr. = %dmA\n", __func__, chan, curr);

	return curr;
}

static int s2mf301_fled_set_flash_curr(struct s2mf301_fled_data *fled, int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL0;
		break;
	}

	if (curr < 50)
		curr = 50;
	else if (curr > s2mf301_fled_flash_curr_max[chan])
		curr = s2mf301_fled_flash_curr_max[chan];

	data = (curr - 50)/50;

	s2mf301_update_reg(fled->i2c, dest, data, S2MF301_CHX_FLASH_IOUT);

	curr_set = s2mf301_fled_get_flash_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	return ret;
}

static int s2mf301_fled_get_torch_curr(struct s2mf301_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL1;
		break;
	}

	s2mf301_read_reg(fled->i2c, dest, &data);

	data = data & S2MF301_CHX_TORCH_IOUT;
	curr = data * 10 + 10;

	pr_info("%s: CH%02d torch curr. = %dmA\n", __func__, chan, curr);

	return curr;
}

static int s2mf301_fled_set_torch_curr(struct s2mf301_fled_data *fled, int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL1;
		break;
	}

	if (curr < 10)
		curr = 10;
	else if (curr > s2mf301_fled_torch_curr_max[chan])
		curr = s2mf301_fled_torch_curr_max[chan];

	data = (curr - 10)/10;

	s2mf301_update_reg(fled->i2c, dest, data, S2MF301_CHX_TORCH_IOUT);

	curr_set = s2mf301_fled_get_torch_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	ret = 0;

	return ret;

}

static void s2mf301_fled_operating_mode(struct s2mf301_fled_data *fled, int mode)
{
	u8 value;

	if (mode < 0 || mode > 3) {
		pr_info("%s, wrong mode\n", __func__);
		mode = AUTO_MODE;
	}

	pr_info("%s = %s\n", __func__, s2mf301_fled_operating_mode_string[mode]);

	value = mode << 6;
	s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0, value, 0xC0);
}

static int s2mf301_fled_get_mode(struct s2mf301_fled_data *fled, int chan)
{
	u8 status;
	int ret = -1;

	s2mf301_read_reg(fled->i2c, S2MF301_FLED_STATUS1, &status);

	pr_info("%s: S2MF301_FLED_STATUS1: 0x%02x\n", __func__, status);

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		if (status & S2MF301_CH1_FLASH_ON)
			ret = S2MF301_FLED_MODE_FLASH;
		else if (status & S2MF301_CH1_TORCH_ON)
			ret = S2MF301_FLED_MODE_TORCH;
		else
			ret = S2MF301_FLED_MODE_OFF;
		break;
	}
	return ret;
}

static int s2mf301_fled_set_mode(struct s2mf301_fled_data *fled, int chan, int mode)
{
	u8 dest = 0, bit = 0, mask = 0, status = 0, chg_sts = 0;

	if ((chan <= 0) || (chan > S2MF301_CH_MAX) || (mode < 0) || (mode > S2MF301_FLED_MODE_MAX)) {
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -EFAULT;
	}

	pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);

	/* 0b000: OFF, 0b101: i2c bit control(on) */
	switch (mode) {
	case S2MF301_FLED_MODE_OFF:
		mask = S2MF301_CHX_FLASH_FLED_EN | S2MF301_CHX_TORCH_FLED_EN;
		bit = 0;
		break;
	case S2MF301_FLED_MODE_FLASH:
		mask = S2MF301_CHX_FLASH_FLED_EN;
		bit = S2MF301_FLED_EN << S2MF301_FALSH_FLED_EN_SHIFT;
		break;
	case S2MF301_FLED_MODE_TORCH:
		s2mf301_fled_operating_mode(fled, SYS_MODE);
		mask = S2MF301_CHX_TORCH_FLED_EN;
		bit = S2MF301_FLED_EN;
		break;
	default:
		return -EFAULT;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CTRL1;
		break;
	}

	/* Need to set EN_FLED_PRE bit before mode change */
	if (mode != S2MF301_FLED_MODE_OFF && mode != S2MF301_FLED_MODE_SHUTDOWN_OFF) {
		s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0, S2MF301_EN_FLED_PRE, S2MF301_EN_FLED_PRE);
		if (mode == S2MF301_FLED_MODE_FLASH) {
			pdo_ctrl_by_flash(1);
			mdelay(350);
		}
	}
	else {
		/* If no LED is on, clear EN_FLED_PRE */
		s2mf301_read_reg(fled->i2c, S2MF301_FLED_STATUS1, &status);
		if (!(status & S2MF301_FLED_ON_CHECK))
			s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0, 0, S2MF301_EN_FLED_PRE);
        if (mode != S2MF301_FLED_MODE_SHUTDOWN_OFF)
    		pdo_ctrl_by_flash(0);
	}
	s2mf301_update_reg(fled->i2c, dest, bit, mask);

	if (mode == S2MF301_FLED_MODE_OFF) {
		s2mf301_fled_operating_mode(fled, AUTO_MODE);

		/* W/A Reactivate charger mode */
		s2mf301_read_reg(fled->chg, 0x0A, &chg_sts);
		/* flash -> off, TA exist */
		if ((status & S2MF301_CH1_FLASH_ON) && (chg_sts & 1 << 3)) {
			/* charger off */
			s2mf301_update_reg(fled->chg, 0x18, 0x0, 0x0F);
			mdelay(1);
			/* charger mode */
			s2mf301_update_reg(fled->chg, 0x18, 0x03, 0x0F);
		}
	}
	return 0;
}

int s2mf301_mode_change_cam_to_leds(enum cam_flash_mode cam_mode)
{
	int mode = -1;

	switch (cam_mode) {
	case CAM_FLASH_MODE_OFF:
		mode = S2MF301_FLED_MODE_OFF;
		break;
	case CAM_FLASH_MODE_SINGLE:
		mode = S2MF301_FLED_MODE_FLASH;
		break;
	case CAM_FLASH_MODE_TORCH:
		mode = S2MF301_FLED_MODE_TORCH;
		break;
	default:
		mode = S2MF301_FLED_MODE_OFF;
		break;
	}

	return mode;
}

int s2mf301_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	if ((chan <= 0) || (chan > S2MF301_CH_MAX) || (mode < 0) || (mode >= S2MF301_FLED_MODE_MAX)) {
		pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -1;
	}

	s2mf301_fled_set_mode(fled, chan, mode);
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_fled_set_mode_ctrl);

int s2mf301_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
	case S2MF301_FLED_MODE_TORCH:
		/* Set curr. */
		s2mf301_fled_set_torch_curr(fled, chan, curr);
		break;
	case S2MF301_FLED_MODE_FLASH:
		/* Set curr. */
		s2mf301_fled_set_flash_curr(fled, chan, curr);
		break;
	default:
		return -1;
	}
	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_fled_set_curr);

int s2mf301_fled_get_curr(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;
	int curr = 0;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
	case S2MF301_FLED_MODE_TORCH:
		curr = s2mf301_fled_get_torch_curr(fled, chan);
		break;
	case S2MF301_FLED_MODE_FLASH:
		curr = s2mf301_fled_get_flash_curr(fled, chan);
		break;
	default:
		return -1;
	}
	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return curr;
}

static ssize_t fled_flash_curr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int cnt = 0;
	int curr = 0;
	int i;
	char str[1016] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MF301_CH_MAX; i++) {
		curr = s2mf301_fled_get_flash_curr(fled, i);
		pr_info("%s: channel: %d, curr: %dmA\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

	return cnt;
}

static ssize_t fled_flash_curr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int chan = -1;
	int curr = -1;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &chan, &curr);
	if (ret != 2)
		pr_err("%s: sscanf fail\n", __func__);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	/* Set curr. */
	s2mf301_fled_set_flash_curr(fled, chan, curr);

	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return size;
}


static ssize_t fled_torch_curr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int cnt = 0;
	int curr = 0;
	int i;
	char str[1016] = {0,};

	/* Read curr. */
	for (i = 1; i <= S2MF301_CH_MAX; i++) {
		curr = s2mf301_fled_get_torch_curr(fled, i);
		pr_info("%s: channel: %d, curr: %dmA\n", __func__, i, curr);
		if (curr >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %dmA, ", i, curr);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

	return cnt;
}

static ssize_t fled_torch_curr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int chan = -1;
	int curr = -1;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &chan, &curr);
	if (ret != 2)
		pr_err("%s: sscanf fail\n", __func__);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	/* Set curr. */
	s2mf301_fled_set_torch_curr(fled, chan, curr);

	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return size;
}

static ssize_t fled_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int cnt = 0;
	int mode = 0;
	int i;
	char str[1016] = {0,};

#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	for (i = 1; i <= S2MF301_CH_MAX; i++) {
		mode = s2mf301_fled_get_mode(fled, i);
		if (mode >= 0)
			cnt += sprintf(str+strlen(str), "CH%02d: %s, ", i, s2mf301_fled_mode_string[mode]);
	}

	cnt += sprintf(str+strlen(str), "\n");

	strcpy(buf, str);

	return cnt;
}

static ssize_t fled_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct s2mf301_fled_data *fled = container_of(led_cdev, struct s2mf301_fled_data, cdev);
	int chan = -1;
	int mode = -1;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &chan, &mode);
	if (ret != 2)
		pr_err("%s: sscanf fail\n", __func__);

	if ((chan <= 0) || (chan > S2MF301_CH_MAX) || (mode < 0) || (mode >= S2MF301_FLED_MODE_MAX)) {
		pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -EFAULT;
	}

	s2mf301_fled_set_mode(fled, chan, mode);
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return size;
}

static DEVICE_ATTR_RW(fled_mode);
static DEVICE_ATTR_RW(fled_flash_curr);
static DEVICE_ATTR_RW(fled_torch_curr);

static struct attribute *s2mf301_fled_attrs[] = {
	&dev_attr_fled_mode.attr,
	&dev_attr_fled_flash_curr.attr,
	&dev_attr_fled_torch_curr.attr,
	NULL
};
ATTRIBUTE_GROUPS(s2mf301_fled);

static void s2mf301_fled_init(struct s2mf301_fled_data *fled)
{
	int i;
	struct i2c_client *i2c = fled->i2c;
	u8 data;

	pr_info("%s: s2mf301_fled init start\n", __func__);

	for (i = 1; i <= S2MF301_CH_MAX; i++) {
		s2mf301_fled_set_flash_curr(fled, i, fled->pdata->default_current);
		s2mf301_fled_set_torch_curr(fled, i, fled->pdata->default_current);
	}
	s2mf301_write_reg(i2c, 0x24, 0xC8);

	s2mf301_read_reg(i2c, S2MF301_FLED_PMIC_ID, &data);
	fled->rev_id = data & S2MF301_FLED_REV_NO;
	pr_info("%s: PMIC_ID = 0x%02x, Rev. No. = %d\n", __func__, data, fled->rev_id);

	if (fled->rev_id >= 1) {
		/* Set Auto source change for flash mode boosting
		 * VBUS_DET(0x1D[5] = 1), T_VBUS_DET_MASK_B(0x1E[6] = 1)
		 */
		s2mf301_update_reg(i2c, 0x1D, 1 << 5, 1 << 5);
		s2mf301_update_reg(i2c, 0x1E, 1 << 6, 1 << 6);
	}

#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif
}

#if IS_ENABLED(CONFIG_OF)
static int s2mf301_led_dt_parse_pdata(struct device *dev, struct s2mf301_fled_platform_data *pdata)
{
	struct device_node *led_np, *np, *c_np;
	int ret;
	u32 temp;
	u32 index;

	led_np = dev->parent->of_node;

	if (!led_np) {
		pr_err("<%s> could not find led sub-node led_np\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(led_np, "flash_led");
	if (!np) {
		pr_err("%s : could not find led sub-node np\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "default_current", &pdata->default_current);
	if (ret < 0)
		pr_err("%s : could not find default_current\n", __func__);

	ret = of_property_read_u32(np, "max_current", &pdata->max_current);
	if (ret < 0)
		pr_err("%s : could not find max_current\n", __func__);

	ret = of_property_read_u32(np, "default_timer", &pdata->default_timer);
	if (ret < 0)
		pr_err("%s : could not find default_timer\n", __func__);


#if IS_ENABLED(FLED_EN)
	ret = pdata->fled-en1-pin = of_get_named_gpio(np, "fled-en1-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en1-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en2-pin = of_get_named_gpio(np, "fled-en2-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en2-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en3-pin = of_get_named_gpio(np, "fled-en3-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en3-pin\n", __func__);
		return ret;
	}

	ret = pdata->fled-en4-pin = of_get_named_gpio(np, "fled-en4-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get fled-en4-pin\n", __func__);
		return ret;
	}
#endif

	pdata->chan_num = of_get_child_count(np);

	if (pdata->chan_num > S2MF301_CH_MAX)
		pdata->chan_num = S2MF301_CH_MAX;

	pdata->channel = devm_kzalloc(dev, sizeof(struct s2mf301_fled_chan) * pdata->chan_num, GFP_KERNEL);

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &temp);
		if (ret < 0)
			goto dt_err;
		index = temp;

		pr_info("%s: temp = %d, index = %d\n", __func__, temp, index);

		if (index < S2MF301_CH_MAX) {
			pdata->channel[index].id = index;

			ret = of_property_read_u32_index(np, "current", index, &pdata->channel[index].curr);
			if (ret < 0) {
				pr_err("%s : could not find current for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].curr = pdata->default_current;
			}

			ret = of_property_read_u32_index(np, "timer", index, &pdata->channel[index].timer);
			if (ret < 0) {
				pr_err("%s : could not find timer for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].timer = pdata->default_timer;
			}
		}
	}
	return 0;
dt_err:
	pr_err("%s: DT parsing finish. ret = %d\n", __func__, ret);
	return ret;
}
#endif /* CONFIG_OF */

static int s2mf301_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_fled_data *fled_data;

	pr_info("%s: start\n", __func__);

	if (!s2mf301) {
		dev_err(&pdev->dev, "drvdata->dev.parent not supplied\n");
		return -ENODEV;
	}

	fled_data = devm_kzalloc(&pdev->dev, sizeof(struct s2mf301_fled_data), GFP_KERNEL);
	fled_data->dev = &pdev->dev;
	fled_data->i2c = s2mf301->i2c;
	fled_data->chg = s2mf301->chg;
	fled_data->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fled_data->pdata)), GFP_KERNEL);
	if (!fled_data->pdata) {
		pr_err("%s: failed to allocate platform data\n", __func__);
		return -ENOMEM;
	}

	if (s2mf301->dev->of_node) {
		ret = s2mf301_led_dt_parse_pdata(&pdev->dev, fled_data->pdata);
		if (ret < 0) {
			pr_err("%s: not found leds dt! ret=%d\n", __func__, ret);
			return -1;
		}
	}

	platform_set_drvdata(pdev, fled_data);

	s2mf301_fled_init(fled_data);

	/* Store fled_data for EXPORT_SYMBOL */
	g_fled_data = fled_data;
	fled_data->cdev.name = "fled-s2mf301";
	fled_data->cdev.groups = s2mf301_fled_groups;

	ret = devm_led_classdev_register(&pdev->dev, &fled_data->cdev);
	if (ret < 0) {
		pr_err("%s: unable to register LED class dev\n", __func__);
		return ret;
	}

	pr_info("%s: end\n", __func__);
	return 0;
}

static int s2mf301_led_remove(struct platform_device *pdev)
{
	return 0;
}

static void s2mf301_led_shutdown(struct platform_device *pdev)
{
	struct s2mf301_fled_data *fled_data =
		platform_get_drvdata(pdev);
	int chan;

	if (!fled_data->i2c) {
		pr_err("%s: no i2c client\n", __func__);
		return;
	}

	/* Turn off all leds when power off */
	pr_info("%s: turn off all leds\n", __func__);
	for (chan = 1; chan <= S2MF301_CH_MAX; chan++)
		s2mf301_fled_set_mode(fled_data, chan, S2MF301_FLED_MODE_SHUTDOWN_OFF);
}

static struct platform_driver s2mf301_led_driver = {
	.driver = {
		.name  = "leds-s2mf301",
		.owner = THIS_MODULE,
		},
	.probe  = s2mf301_led_probe,
	.remove = s2mf301_led_remove,
	.shutdown = s2mf301_led_shutdown,
};

static int __init s2mf301_led_driver_init(void)
{
	return platform_driver_register(&s2mf301_led_driver);
}
module_init(s2mf301_led_driver_init);

static void __exit s2mf301_led_driver_exit(void)
{
	platform_driver_unregister(&s2mf301_led_driver);
}
module_exit(s2mf301_led_driver_exit);

MODULE_AUTHOR("Keunho Hwang <keunho.hwang@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG s2mf301 flash LED Driver");
MODULE_LICENSE("GPL");
