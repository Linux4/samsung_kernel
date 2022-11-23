/******************************************************************************
 * Copyright (C) 2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019-2021 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
#include <linux/regulator/consumer.h>
#endif
#include "common_ese.h"

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
static int nfc_param_lpcharge = LPM_NO_SUPPORT;
module_param(nfc_param_lpcharge, int, 0440);

struct nfc_dev *g_nfc_dev;
static bool g_is_nfc_pvdd_enabled;
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
enum lpm_status nfc_get_lpcharge(void)
{
	return nfc_param_lpcharge;
}
#endif

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
struct nfc_dev g_nfc_dev_for_chrdev;

void nfc_parse_dt_for_platform_device(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct platform_configs *nfc_configs = &g_nfc_dev_for_chrdev.configs;
	struct platform_gpio *nfc_gpio = &nfc_configs->gpio;

	nfc_configs->late_pvdd_en = of_property_read_bool(np, "pn547,late_pvdd_en");
	NFC_LOG_INFO("late_pvdd_en :%d, lpcharge :%d\n", nfc_configs->late_pvdd_en, nfc_get_lpcharge());
	if (nfc_get_lpcharge() == LPM_FALSE)
		nfc_configs->late_pvdd_en = false;

	if (nfc_configs->late_pvdd_en) {
		nfc_gpio->ven = of_get_named_gpio(np, DTS_VEN_GPIO_STR, 0);
		if (!gpio_is_valid(nfc_gpio->ven))
			NFC_LOG_ERR("%s: ven gpio invalid %d\n", __func__, nfc_gpio->ven);
		else
			configure_gpio(nfc_gpio->ven, GPIO_OUTPUT);

		nfc_configs->nfc_pvdd = regulator_get(dev, "nfc_pvdd");
		if (IS_ERR(nfc_configs->nfc_pvdd)) {
			NFC_LOG_ERR("get nfc_pvdd error\n");
			nfc_configs->nfc_pvdd = NULL;
		} else {
			NFC_LOG_INFO("LDO nfc_pvdd: %pK\n", nfc_configs->nfc_pvdd);
		}
	}
}
#endif

int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs,
		 uint8_t interface)
{
	struct device_node *np = dev->of_node;
	struct platform_gpio *nfc_gpio = &nfc_configs->gpio;
#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
	const char *ap_str;
	static int retry_count = 3;
#endif

	if (!np) {
		NFC_LOG_ERR("%s: nfc of_node NULL\n", __func__);
		return -EINVAL;
	}

	nfc_gpio->irq = -EINVAL;
	nfc_gpio->dwl_req = -EINVAL;
	nfc_gpio->ven = -EINVAL;

	/* irq required for i2c based chips only */
	if (interface == PLATFORM_IF_I2C) {
		nfc_gpio->irq = of_get_named_gpio(np, DTS_IRQ_GPIO_STR, 0);
		if ((!gpio_is_valid(nfc_gpio->irq))) {
			NFC_LOG_ERR("%s: irq gpio invalid %d\n", __func__,
				nfc_gpio->irq);
			return -EINVAL;
		}
		NFC_LOG_INFO("%s: irq %d\n", __func__, nfc_gpio->irq);
	}
	nfc_gpio->ven = of_get_named_gpio(np, DTS_VEN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->ven))) {
		NFC_LOG_ERR("%s: ven gpio invalid %d\n", __func__, nfc_gpio->ven);
		return -EINVAL;
	}
	/* some products like sn220 does not required fw dwl pin */
	nfc_gpio->dwl_req = of_get_named_gpio(np, DTS_FWDN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->dwl_req)))
		NFC_LOG_ERR("%s: dwl_req gpio invalid %d\n", __func__,
			nfc_gpio->dwl_req);

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
	nfc_configs->late_pvdd_en = of_property_read_bool(np, "pn547,late_pvdd_en");

	if (nfc_get_lpcharge() == LPM_FALSE)
		nfc_configs->late_pvdd_en = false;

	nfc_gpio->clk_req = of_get_named_gpio(np, "pn547,clk_req-gpio", 0);
	if ((!gpio_is_valid(nfc_gpio->clk_req))) {
		NFC_LOG_ERR("nfc clk_req gpio invalid %d\n", nfc_gpio->clk_req);
	}

	nfc_configs->clk_req_wake = of_property_read_bool(np, "pn547,clk_req_wake");

	if (of_property_read_bool(np, "pn547,clk_req_all_trigger")) {
		nfc_configs->clk_req_all_trigger = true;
		NFC_LOG_INFO("irq_all_trigger\n");
	}
	if (of_property_read_bool(np, "pn547,change_clkreq_for_acpm")) {
		nfc_configs->change_clkreq_for_acpm = true;
		NFC_LOG_INFO("change clkreq for acpm!!\n");
	}

	if (!of_property_read_string(np, "pn547,ap_vendor", &ap_str)) {
		if (!strcmp(ap_str, "slsi"))
			nfc_configs->ap_vendor = AP_VENDOR_SLSI;
		else if (!strcmp(ap_str, "qct") || !strcmp(ap_str, "qualcomm"))
			nfc_configs->ap_vendor = AP_VENDOR_QCT;
		else if (!strcmp(ap_str, "mtk"))
			nfc_configs->ap_vendor = AP_VENDOR_MTK;
	} else {
		NFC_LOG_INFO("AP vendor is not set\n");
	}
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	if (nfc_configs->late_pvdd_en)
		nfc_configs->nfc_pvdd = g_nfc_dev_for_chrdev.configs.nfc_pvdd;
	else
		nfc_configs->nfc_pvdd = regulator_get(dev, "nfc_pvdd");
#else
	nfc_configs->nfc_pvdd = regulator_get(dev, "nfc_pvdd");
#endif
	if (IS_ERR(nfc_configs->nfc_pvdd)) {
		NFC_LOG_ERR("get nfc_pvdd error\n");
		nfc_configs->nfc_pvdd = NULL;
		if (--retry_count > 0)
			return -EPROBE_DEFER;
		else
			return -ENODEV;
	} else {
		NFC_LOG_INFO("LDO nfc_pvdd: %pK\n", nfc_configs->nfc_pvdd);
	}

	if (of_find_property(np, "clocks", NULL)) {
		nfc_configs->nfc_clock = clk_get(dev, "oscclk_nfc");
		if (IS_ERR(nfc_configs->nfc_clock)) {
			NFC_LOG_ERR("probe() clk not found\n");
			nfc_configs->nfc_clock = NULL;
		} else {
			NFC_LOG_INFO("found oscclk_nfc\n");
		}
	}
#endif

	NFC_LOG_INFO("%s: irq %d, ven %d, fw %d\n", __func__, nfc_gpio->irq, nfc_gpio->ven,
		nfc_gpio->dwl_req);
	return 0;
}

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
int nfc_regulator_onoff(struct platform_configs *nfc_configs, int onoff)
{
	int rc = 0;
	struct regulator *regulator_nfc_pvdd;

	/* if nfc probe is called before nfc platform device, nfc_configs->nfc_pvdd is NULL */
	if (!nfc_configs->nfc_pvdd)
		nfc_configs->nfc_pvdd = g_nfc_dev_for_chrdev.configs.nfc_pvdd;

	regulator_nfc_pvdd = nfc_configs->nfc_pvdd;
	if (!regulator_nfc_pvdd) {
		NFC_LOG_ERR("error: null regulator!\n");
		rc = -ENODEV;
		goto done;
	}

	g_is_nfc_pvdd_enabled = regulator_is_enabled(regulator_nfc_pvdd);

	NFC_LOG_INFO("onoff = %d, is_nfc_pvdd_enabled = %d\n", onoff, g_is_nfc_pvdd_enabled);

	if (g_is_nfc_pvdd_enabled == onoff) {
		NFC_LOG_INFO("%s already pvdd %s\n", __func__, onoff ? "enabled" : "disabled");
		goto done;
	}

	if (onoff) {
		rc = regulator_set_load(regulator_nfc_pvdd, 300000);
		if (rc) {
			NFC_LOG_ERR("regulator_uwb_vdd set_load failed, rc=%d\n", rc);
			goto done;
		}
		rc = regulator_enable(regulator_nfc_pvdd);
		if (rc) {
			NFC_LOG_ERR("enable failed, rc=%d\n", rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_nfc_pvdd);
		if (rc) {
			NFC_LOG_ERR("disable failed, rc=%d\n", rc);
			goto done;
		}
	}

	g_is_nfc_pvdd_enabled = !!onoff;

	NFC_LOG_INFO("success\n");
done:
	return rc;
}

bool nfc_check_pvdd_status(void)
{
	return g_is_nfc_pvdd_enabled;
}
#endif

void set_valid_gpio(int gpio, int value)
{
	if (gpio_is_valid(gpio)) {
		NFC_LOG_DBG("%s: gpio %d value %d\n", __func__, gpio, value);
		gpio_set_value(gpio, value);
		/* hardware dependent delay */
		usleep_range(NFC_GPIO_SET_WAIT_TIME_US,
			     NFC_GPIO_SET_WAIT_TIME_US + 100);
	}
}

int get_valid_gpio(int gpio)
{
	int value = -EINVAL;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
		NFC_LOG_DBG("%s: gpio %d value %d\n", __func__, gpio, value);
	}
	return value;
}

void gpio_set_ven(struct nfc_dev *nfc_dev, int value)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (gpio_get_value(nfc_gpio->ven) != value) {
		NFC_LOG_REC("%s: value %d\n", __func__, value);
		/* reset on change in level from high to low */
		if (value)
			ese_cold_reset_release(nfc_dev);

		gpio_set_value(nfc_gpio->ven, value);
		/* hardware dependent delay */
		usleep_range(NFC_GPIO_SET_WAIT_TIME_US,
			     NFC_GPIO_SET_WAIT_TIME_US + 100);
	}
}

int configure_gpio(unsigned int gpio, int flag)
{
	int ret;

	NFC_LOG_DBG("%s: nfc gpio [%d] flag [%01x]\n", __func__, gpio, flag);
	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, "nfc_gpio");
		if (ret) {
			NFC_LOG_ERR("%s: unable to request nfc gpio [%d]\n",
				__func__, gpio);
			return ret;
		}
		/* set direction and value for output pin */
		if (flag & GPIO_OUTPUT) {
			ret = gpio_direction_output(gpio, (GPIO_HIGH & flag));
			NFC_LOG_DBG("%s: nfc o/p gpio %d level %d\n", __func__,
				 gpio, gpio_get_value(gpio));
		} else {
			ret = gpio_direction_input(gpio);
			NFC_LOG_DBG("%s: nfc i/p gpio %d\n", __func__, gpio);
		}

		if (ret) {
			NFC_LOG_ERR("%s: unable to set direction for nfc gpio [%d]\n",
			       __func__, gpio);
			gpio_free(gpio);
			return ret;
		}
		/* Consider value as control for input IRQ pin */
		if (flag & GPIO_IRQ) {
			ret = gpio_to_irq(gpio);
			if (ret < 0) {
				NFC_LOG_ERR("%s: unable to set irq [%d]\n", __func__,
					gpio);
				gpio_free(gpio);
				return ret;
			}
			NFC_LOG_DBG("%s: gpio_to_irq successful [%d]\n", __func__,
				 gpio);
			return ret;
		}
	} else {
		NFC_LOG_ERR("%s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

void gpio_free_all(struct nfc_dev *nfc_dev)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (gpio_is_valid(nfc_gpio->dwl_req))
		gpio_free(nfc_gpio->dwl_req);

	if (gpio_is_valid(nfc_gpio->irq))
		gpio_free(nfc_gpio->irq);

	if (gpio_is_valid(nfc_gpio->ven))
		gpio_free(nfc_gpio->ven);
}

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
void nfc_power_control(struct nfc_dev *nfc_dev)
{
	struct platform_configs *nfc_configs = &nfc_dev->configs;
	int ret;

	ret = nfc_regulator_onoff(nfc_configs, 1);
	if (ret < 0)
		NFC_LOG_ERR("%s pn547 regulator_on fail err = %d\n", __func__, ret);

	ese_set_spi_pinctrl_for_ese_off(NULL);
	usleep_range(15000, 20000); /* spec : VDDIO high -> 15~20 ms -> VEN high*/

	gpio_set_ven(nfc_dev, 1);
	gpio_set_ven(nfc_dev, 0);
	gpio_set_ven(nfc_dev, 1);
}

static ssize_t nfc_support_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	NFC_LOG_INFO("\n");
	return 0;
}
static CLASS_ATTR_RO(nfc_support);

static ssize_t pvdd_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	struct nfc_dev *nfc_dev = g_nfc_dev;
	struct platform_configs *nfc_configs;

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	if (!g_nfc_dev) {
		nfc_dev = &g_nfc_dev_for_chrdev;
		NFC_LOG_INFO("%s called before nfc probe", __func__);
	}
#endif
	if (!nfc_dev) {
		NFC_LOG_ERR("%s nfc_dev is NULL!", __func__);
		return size;
	}

	nfc_configs = &nfc_dev->configs;

	NFC_LOG_INFO("%s val: %c, late_pvdd_en: %d\n", __func__, buf[0], nfc_configs->late_pvdd_en);

	if (buf[0] == '1' && nfc_configs->late_pvdd_en)
		nfc_power_control(nfc_dev);

	return size;
}
static CLASS_ATTR_WO(pvdd);

#ifdef CONFIG_SAMSUNG_NFC_DEBUG
static ssize_t check_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct nfc_dev *nfc_dev = g_nfc_dev;
	char *cmd = nfc_dev->write_kbuf;
	char *rsp = nfc_dev->read_kbuf;
	int timeout = NCI_CMD_RSP_TIMEOUT_MS;
	int size = 0;
	int ret;
	int cmd_length = 4;

	mutex_lock(&nfc_dev->write_mutex);
	*cmd++ = 0x20;
	*cmd++ = 0x00;
	*cmd++ = 0x01;
	*cmd++ = 0x00;

	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, cmd_length, MAX_RETRY_COUNT);
	if (ret != cmd_length) {
		ret = -EIO;
		NFC_LOG_ERR("%s: nfc_write returned %d\n", __func__, ret);
		mutex_unlock(&nfc_dev->write_mutex);
		goto end;
	}
	mutex_unlock(&nfc_dev->write_mutex);

	/* Read data */
	mutex_lock(&nfc_dev->read_mutex);
	cmd_length = 6;
	ret = nfc_dev->nfc_read(nfc_dev, rsp, cmd_length, timeout);

	if (ret < 0 || ret > cmd_length) {
		size = snprintf(buf, SZ_64, "i2c_master_recv returned %d. count : %d\n",
			ret, cmd_length);
		goto end;
	}

	size = snprintf(buf, SZ_64, "test completed!! size: %d, data: %X %X %X %X %X %X\n",
		ret, rsp[0], rsp[1], rsp[2], rsp[3], rsp[4], rsp[5]);
end:
	mutex_unlock(&nfc_dev->read_mutex);
	return size;
}

#ifdef FEATURE_SEC_NFC_TEST
extern void nfc_check_is_core_reset_ntf(u8 *data, int len);
#endif
static ssize_t check_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	if (size > 0) {
		if (buf[0] == '1') {
			NFC_LOG_INFO("%s: test\n", __func__);
			nfc_print_status();
		}
#ifdef FEATURE_SEC_NFC_TEST
		else if (buf[0] == '2') {
			u8 header[3] = {0x60, 0x00, 0x06};
			u8 data[10] = {0x0, };

			nfc_check_is_core_reset_ntf(header, 3);
			data[0] = 0xA0;
			nfc_check_is_core_reset_ntf(data, 6);
		}
#endif
	}

	return size;
}
static CLASS_ATTR_RW(check);
#endif
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
void nfc_probe_done(struct nfc_dev *nfc_dev)
{
	g_nfc_dev = nfc_dev;
}
#endif

void nfc_misc_unregister(struct nfc_dev *nfc_dev, int count)
{
	NFC_LOG_DBG("%s: entry\n", __func__);
#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	if (nfc_dev == NULL)
		nfc_dev = &g_nfc_dev_for_chrdev;
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
	class_remove_file(nfc_dev->nfc_class, &class_attr_nfc_support);
	class_remove_file(nfc_dev->nfc_class, &class_attr_pvdd);
#ifdef CONFIG_SAMSUNG_NFC_DEBUG
	class_remove_file(nfc_dev->nfc_class, &class_attr_check);
#endif
#endif
	device_destroy(nfc_dev->nfc_class, nfc_dev->devno);
	cdev_del(&nfc_dev->c_dev);
	class_destroy(nfc_dev->nfc_class);
	unregister_chrdev_region(nfc_dev->devno, count);
}

int nfc_misc_register(struct nfc_dev *nfc_dev,
			const struct file_operations *nfc_fops, int count,
			char *devname, char *classname)
{
	int ret = 0;

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	if (nfc_dev == NULL)
		nfc_dev = &g_nfc_dev_for_chrdev;
#endif

	ret = alloc_chrdev_region(&nfc_dev->devno, 0, count, devname);
	if (ret < 0) {
		NFC_LOG_ERR("%s: failed to alloc chrdev region ret %d\n", __func__,
			ret);
		return ret;
	}
	nfc_dev->nfc_class = class_create(THIS_MODULE, classname);
	if (IS_ERR(nfc_dev->nfc_class)) {
		ret = PTR_ERR(nfc_dev->nfc_class);
		NFC_LOG_ERR("%s: failed to register device class ret %d\n", __func__,
			ret);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
	ret = class_create_file(nfc_dev->nfc_class, &class_attr_nfc_support);
	if (ret)
		NFC_LOG_ERR("failed to create nfc_support file\n");

	ret = class_create_file(nfc_dev->nfc_class, &class_attr_pvdd);
	if (ret)
		NFC_LOG_ERR("failed to create nfc_support file\n");

#ifdef CONFIG_SAMSUNG_NFC_DEBUG
	ret = class_create_file(nfc_dev->nfc_class, &class_attr_check);
	if (ret)
		NFC_LOG_ERR("failed to create test file\n");
#endif
#endif

	cdev_init(&nfc_dev->c_dev, nfc_fops);
	ret = cdev_add(&nfc_dev->c_dev, nfc_dev->devno, count);
	if (ret < 0) {
		NFC_LOG_ERR("%s: failed to add cdev ret %d\n", __func__, ret);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	nfc_dev->nfc_device = device_create(nfc_dev->nfc_class, NULL,
					    nfc_dev->devno, nfc_dev, devname);
	if (IS_ERR(nfc_dev->nfc_device)) {
		ret = PTR_ERR(nfc_dev->nfc_device);
		NFC_LOG_ERR("%s: failed to create the device ret %d\n", __func__,
			ret);
		cdev_del(&nfc_dev->c_dev);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	return 0;
}

/**
 * nfc_ioctl_power_states() - power control
 * @nfc_dev:    nfc device data structure
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states, refer common.h for args
 *
 * Return: -ENOIOCTLCMD if arg is not supported, 0 if Success(or no issue)
 * and error ret code otherwise
 */
static int nfc_ioctl_power_states(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (arg == NFC_POWER_OFF) {
		/*
		 * We are attempting a hardware reset so let us disable
		 * interrupts to avoid spurious notifications to upper
		 * layers.
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_set_ven(nfc_dev, 0);
		nfc_dev->nfc_ven_enabled = false;
	} else if (arg == NFC_POWER_ON) {
		nfc_dev->nfc_enable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);

		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_ven_enabled = true;
	} else if (arg == NFC_FW_DWL_VEN_TOGGLE) {
		/*
		 * We are switching to download Mode, toggle the enable pin
		 * in order to set the NFCC in the new mode
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
	} else if (arg == NFC_FW_DWL_HIGH) {
		/*
		 * Setting firmware download gpio to HIGH
		 * before FW download start
		 */
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;

	} else if (arg == NFC_VEN_FORCED_HARD_RESET) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
	} else if (arg == NFC_FW_DWL_LOW) {
		/*
		 * Setting firmware download gpio to LOW
		 * FW download finished
		 */
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		nfc_dev->nfc_state = NFC_STATE_NCI;
	} else {
		NFC_LOG_ERR("%s: bad arg %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

/**
 * nfc_dev_ioctl - used to set or get data from upper layer.
 * @pfile   file node for opened device.
 * @cmd     ioctl type from upper layer.
 * @arg     ioctl arg from upper layer.
 *
 * NFC and ESE Device power control, based on the argument value
 *
 * Return: -ENOIOCTLCMD if arg is not supported
 * 0 if Success(or no issue)
 * 0 or 1 in case of arg is ESE_GET_PWR/ESE_POWER_STATE
 * and error ret code otherwise
 */
long nfc_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;

	NFC_LOG_INFO("%s: cmd = %x arg = %zx\n", __func__, cmd, arg);
	switch (cmd) {
	case NFC_SET_PWR:
		NFC_LOG_INFO("%s: NFC_SET_PWR\n", __func__);
		ret = nfc_ioctl_power_states(nfc_dev, arg);
		break;
	case ESE_SET_PWR:
		NFC_LOG_INFO("%s: ESE_SET_PWR\n", __func__);
		ret = nfc_ese_pwr(nfc_dev, arg);
		break;
	case ESE_GET_PWR:
		NFC_LOG_INFO("%s: ESE_GET_PWR\n", __func__);
		ret = nfc_ese_pwr(nfc_dev, ESE_POWER_STATE);
		break;
	default:
		NFC_LOG_ERR("%s: bad cmd %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	};
	return ret;
}

int nfc_dev_open(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	nfc_dev = g_nfc_dev;
#else
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);
#endif
	if (!nfc_dev)
		return -ENODEV;

	NFC_LOG_INFO("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	mutex_lock(&nfc_dev->dev_ref_mutex);

	filp->private_data = nfc_dev;

	if (nfc_dev->dev_ref_count == 0) {
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);

		nfc_dev->nfc_enable_intr(nfc_dev);
#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
		nfc_dev->nfc_enable_clk_intr(nfc_dev);
#endif
	}
	nfc_dev->dev_ref_count = nfc_dev->dev_ref_count + 1;
	mutex_unlock(&nfc_dev->dev_ref_mutex);
	return 0;
}

int nfc_dev_flush(struct file *pfile, fl_owner_t id)
{
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;
	/*
	 * release blocked user thread waiting for pending read during close
	 */
	if (!mutex_trylock(&nfc_dev->read_mutex)) {
		nfc_dev->release_read = true;
		nfc_dev->nfc_disable_intr(nfc_dev);
		wake_up(&nfc_dev->read_wq);
		NFC_LOG_DBG("%s: waiting for release of blocked read\n", __func__);
		mutex_lock(&nfc_dev->read_mutex);
		nfc_dev->release_read = false;
	} else {
		NFC_LOG_DBG("%s: read thread already released\n", __func__);
	}
	mutex_unlock(&nfc_dev->read_mutex);
	return 0;
}

int nfc_dev_close(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;

#ifdef CONFIG_MAKE_NODE_USING_PLATFORM_DEVICE
	nfc_dev = g_nfc_dev;
#else
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);
#endif
	if (!nfc_dev)
		return -ENODEV;

	NFC_LOG_INFO("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));
	mutex_lock(&nfc_dev->dev_ref_mutex);
	if (nfc_dev->dev_ref_count == 1) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);
#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
		nfc_dev->nfc_disable_clk_intr(nfc_dev);
#endif
	}
	if (nfc_dev->dev_ref_count > 0)
		nfc_dev->dev_ref_count = nfc_dev->dev_ref_count - 1;
	else {
		/*
		 * Use "ESE_RST_PROT_DIS" as argument
		 * if eSE calls flow is via NFC driver
		 * i.e. direct calls from SPI HAL to NFC driver
		 */
		nfc_ese_pwr(nfc_dev, ESE_RST_PROT_DIS_NFC);
	}
	filp->private_data = NULL;

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
	nfc_print_status();
#endif

	mutex_unlock(&nfc_dev->dev_ref_mutex);
	return 0;
}

int validate_nfc_state_nci(struct nfc_dev *nfc_dev)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (!gpio_get_value(nfc_gpio->ven)) {
		NFC_LOG_ERR("%s: ven low - nfcc powered off\n", __func__);
		return -ENODEV;
	}
	if (get_valid_gpio(nfc_gpio->dwl_req) == 1) {
		NFC_LOG_ERR("%s: fw download in-progress\n", __func__);
		return -EBUSY;
	}
	if (nfc_dev->nfc_state != NFC_STATE_NCI) {
		NFC_LOG_ERR("%s: fw download state\n", __func__);
		return -EBUSY;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_SAMSUNG_NFC)
void nfc_print_status(void)
{
	struct nfc_dev *nfc_dev = g_nfc_dev;
	struct platform_configs *nfc_configs;
	struct platform_gpio *nfc_gpio;
	int en, firm, irq, pvdd = 0;
	int clk_req_irq = -1;

	if (nfc_dev == NULL)
		return;

	nfc_configs = &nfc_dev->configs;
	nfc_gpio = &nfc_dev->configs.gpio;

	en = get_valid_gpio(nfc_gpio->ven);
	firm = get_valid_gpio(nfc_gpio->dwl_req);
	irq = get_valid_gpio(nfc_gpio->irq);
	if (nfc_configs->nfc_pvdd)
		pvdd = regulator_is_enabled(nfc_configs->nfc_pvdd);
	else
		NFC_LOG_ERR("nfc_pvdd is null\n");

	clk_req_irq = get_valid_gpio(nfc_gpio->clk_req);

	NFC_LOG_INFO("en: %d, firm: %d, pvdd: %d, irq: %d, clk_req: %d\n",
		en, firm, pvdd, irq, clk_req_irq);
#ifdef CONFIG_SEC_NFC_LOGGER_ADD_ACPM_LOG
	nfc_logger_acpm_log_print();
#endif
}
#endif
