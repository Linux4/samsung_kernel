/* gpio_afc.c
 *
 * Copyright (C) 2019 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/afc/afc.h>
#include <linux/power_supply.h>
#include <linux/sec_class.h>
#include <linux/sec_param.h>

static struct afc_dev *gafc;

int send_afc_result(int res)
{
	static struct power_supply *psy_bat = NULL;
	union power_supply_propval value = {0, };
	int ret = 0;

	if (psy_bat == NULL) {
		psy_bat = power_supply_get_by_name("battery");
		if (!psy_bat) {
			pr_err("%s: Failed to Register psy_bat\n", __func__);
			return 0;
		}
	}

	value.intval = res;
#if defined(CONFIG_AFC_ENUM)
	ret = power_supply_set_property(psy_bat,
			(enum power_supply_property)POWER_SUPPLY_PROP_AFC_RESULT, &value);
#endif
	if (ret < 0)
		pr_err("%s: POWER_SUPPLY_EXT_PROP_AFC_RESULT set failed\n", __func__);

	return 0;
}

int afc_get_vbus(void)
{
	static struct power_supply *psy_usb = NULL;
	union power_supply_propval value = {0, };
	int ret = 0, input_vol_mV = 0;

	if (psy_usb == NULL) {
		psy_usb = power_supply_get_by_name("usb");
		if (!psy_usb) {
			pr_err("%s: Failed to Register psy_usb\n", __func__);
			return -1;
		}
	}
	ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
	if(ret < 0) {
		pr_err("%s: Fail to get usb Input Voltage %d\n",__func__,ret);
	} else {  
		input_vol_mV = value.intval/1000;
		pr_info("%s: Input Voltage(%d) \n", __func__, input_vol_mV);                
		return input_vol_mV;
	}

	return ret;
}

void cycle(int ui)
{
	if (!gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_active);
		gafc->pin_state = true;
	}

	gpio_direction_output(gafc->afc_data_gpio, 1);
	udelay(ui);
	gpio_direction_output(gafc->afc_data_gpio, 0);
	udelay(ui);
}

void afc_send_Mping(void)
{
	if (!gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_active);
		gafc->pin_state = true;
	}

	gpio_direction_output(gafc->afc_data_gpio, 1);
	udelay(16*UI);
	gpio_direction_output(gafc->afc_data_gpio, 0);
}

int afc_recv_Sping(void)
{
	int i = 0, sp_cnt = 0;

	if (gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_suspend);
		gafc->pin_state = false;
	}

	while (1)
	{
		udelay(UI);

		if (gpio_get_value(gafc->afc_data_gpio)) {
			pr_info("%s, waiting recv sping(%d)...\n", __func__, i);
			break;
		}

		if (WAIT_SPING_COUNT < i++) {
			pr_info("%s, waiting time is over!\n", __func__);
			goto Sping_err;
		}
	}

	do {
		if (SPING_MAX_UI < sp_cnt++)
			goto Sping_err;
		udelay(UI);
	} while (gpio_get_value(gafc->afc_data_gpio));

	if (sp_cnt < SPING_MIN_UI)
		goto Sping_err;

	pr_info("%s, Done - len(%d)\n", __func__, sp_cnt);

	return 0;

Sping_err:
	pr_info("%s, Sping err - len(%d)\n", __func__, sp_cnt);

	return -1;
}

int afc_send_parity_bit(int data)
{
	int cnt = 0, odd = 0;

	for (; data > 0; data = data >> 1) {
		if (data & 0x1)	
			cnt++;
	}

	odd = cnt % 2;

	if (!odd)
		gpio_direction_output(gafc->afc_data_gpio, 1);
	else
		gpio_direction_output(gafc->afc_data_gpio, 0);

	udelay(UI);

	return odd;
}

void afc_send_data(int data)
{
	int i = 0;

	if (!gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_active);
		gafc->pin_state = true;
	}

	udelay(160);

	if (data & 0x80)
		cycle(UI/4);
	else {
		cycle(UI/4);
		gpio_direction_output(gafc->afc_data_gpio, 1);
		udelay(UI/4);
	}

	for (i = 0x80; i > 0; i = i >> 1)
	{
		gpio_direction_output(gafc->afc_data_gpio, data & i);
		udelay(UI);
	}

	if (afc_send_parity_bit(data)) {
		gpio_direction_output(gafc->afc_data_gpio, 0);
		udelay(UI/4);
		cycle(UI/4);
	} else {
		udelay(UI/4);
		cycle(UI/4);
	}
}

int afc_recv_data(void)
{
	int ret = 0;

	if (gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_suspend);
		gafc->pin_state = false;
	}

	ret = afc_recv_Sping();
	if (ret < 0) {
		gafc->afc_error = SPING_ERR_2;
		return -1;
	}

	mdelay(2);

	ret = afc_recv_Sping();
	if (ret < 0) {
		gafc->afc_error = SPING_ERR_3;
		return -1;
	}

	return 0;
}

int afc_communication(void)
{
	int ret = 0;

	pr_info("%s - Start\n", __func__);

	spin_lock_irq(&gafc->afc_spin_lock);

	afc_send_Mping();

	ret = afc_recv_Sping();
	if (ret < 0) {
		gafc->afc_error = SPING_ERR_1;
		goto afc_fail;
	}

	if (gafc->vol == SET_9V)
		afc_send_data(0x46);
	else
		afc_send_data(0x08);

	afc_send_Mping();

	ret = afc_recv_data();
	if (ret < 0)
		goto afc_fail;

	udelay(200);

	afc_send_Mping();

	ret = afc_recv_Sping();
	if (ret < 0) {
		gafc->afc_error = SPING_ERR_4;
		goto afc_fail;
	}

	spin_unlock_irq(&gafc->afc_spin_lock);

	return 0;

afc_fail:
	pr_info("%s, AFC communication has been failed\n", __func__);
	spin_unlock_irq(&gafc->afc_spin_lock);

	return -1;
}

static void afc_set_voltage_workq(struct work_struct *work)
{
	int i = 0, ret = 0, retry_cnt = 0, vbus = 0, vbus_retry = 0;

	pr_info("%s\n", __func__);

	if (!gafc->afc_used)
		msleep(1500);
	else {
		vbus = afc_get_vbus();

		if (((gafc->vol == SET_5V) && (vbus < 5500)) 
		|| ((gafc->vol == SET_9V) && (7500 < vbus))) {
			pr_info("%s, Duplicated requestion, just return!\n", __func__);
			return;
		}
	}

	gpio_direction_output(gafc->afc_switch_gpio, 1);
	if (!gafc->afc_used) 
		gafc->afc_used = true;

	for (i = 0; (i < AFC_COMM_CNT) && (retry_cnt < AFC_RETRY_MAX); i++) {
		ret = afc_communication();
		msleep(38);
		if (ret < 0) {
			vbus = afc_get_vbus();
			if (vbus > 3500) {
				pr_info("%s - fail, retry_cnt(%d), err_case(%d)\n", __func__, retry_cnt++, gafc->afc_error);
				gafc->afc_error = 0;
				i = -1;
				msleep(300);
				continue;
			} else {
				pr_info("%s, No vbus! just return!\n", __func__);
				goto end_afc;
			}
		}
	}

	if (retry_cnt == AFC_RETRY_MAX) {
		pr_info("%s, retry count is over!\n", __func__);
		goto send_result;
	}

	do {
		vbus = afc_get_vbus();
		pr_info("%s, (%d) retry...(%d)\n", __func__, vbus, vbus_retry);

		if (vbus < 3500) {
			pr_info("%s, No vbus! just return!\n", __func__);
			goto end_afc;
		}

		if (((gafc->vol == SET_5V) && (vbus < 5500)) 
		|| ((gafc->vol == SET_9V) && (7500 < vbus)))
			break;
		else
			usleep_range(20000, 22000);
	} while(vbus_retry++ < VBUS_RETRY_MAX);

send_result:
	if ((gafc->vol == SET_5V) && (vbus < 5500)) {
		pr_info("%s, Succeed AFC 5V\n", __func__);
		send_afc_result(AFC_5V);
	} else if ((gafc->vol == SET_9V) && (7500 < vbus)) {
		pr_info("%s, Succeed AFC 9V\n", __func__);
		send_afc_result(AFC_9V);
	} else {
		pr_info("%s, Failed AFC\n", __func__);
		send_afc_result(AFC_FAIL);
	}

end_afc:
	gpio_direction_output(gafc->afc_switch_gpio, 0);
}

int afc_set_voltage(int vol)
{
	if (!gafc) {
		pr_info("%s, afc is not ready, return\n",__func__);
		return -ENODEV;
	}

	pr_info("%s, set_voltage(%d)\n", __func__, vol);

	gafc->vol = vol;
	if(vol == SET_9V && gafc->afc_disable){
		gafc->vol = SET_5V;
	}
	
	schedule_work(&gafc->afc_set_voltage_work);

	return 0;
}

void afc_reset(void)
{
	if (!gafc->pin_state) {
		pinctrl_select_state(gafc->pinctrl, gafc->pin_active);
		gafc->pin_state = true;
	}

	gpio_direction_output(gafc->afc_data_gpio, 1);
	udelay(100*UI);
	gpio_direction_output(gafc->afc_data_gpio, 0);
}

void detach_afc(void)
{
	if(!gafc)
		return;
	if (gafc->afc_used) {
		pr_info("%s\n", __func__);

		gafc->afc_used = false;
		cancel_work(&gafc->afc_set_voltage_work);
		gpio_direction_output(gafc->afc_switch_gpio, 0);
	}
}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0;
static int __init set_afc_mode(char *str)
{
	get_option(&str, &afc_mode);
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("afc_disable", set_afc_mode);

int get_afc_mode(void)
{
	pr_info("%s: afc_mode is 0x%02x\n", __func__, (afc_mode & 0x1));

	if (afc_mode & 0x1)
		return 1;
	else
		return 0;
}

static ssize_t afc_disable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("%s\n", __func__);

	if (gafc->afc_disable)
		return sprintf(buf, "AFC is disabled\n");
	else
		return sprintf(buf, "AFC is enabled\n");
}

static ssize_t afc_disable_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{

	static struct power_supply *psy_usb = NULL;
	union power_supply_propval value = {0, };
	int val = 0, ret = 0;

	sscanf(buf, "%d", &val);
	
	pr_info("%s, val: %d\n", __func__, val);

	if (!strncasecmp(buf, "1", 1)){
		gafc->afc_disable = true;
		val = '1';
	}
	else if (!strncasecmp(buf, "0", 1)){
		gafc->afc_disable = false;
		val = '0';
	}
	else {
		pr_warn("%s: invalid value\n", __func__);
		return count;
	}

	if (psy_usb == NULL) {
		psy_usb = power_supply_get_by_name("battery");
		if (!psy_usb) {
			pr_err("%s: Failed to Register psy_usb\n", __func__);
		}
	}
	value.intval = val == '1' ? HV_DISABLE : HV_ENABLE;
#if defined(CONFIG_AFC_ENUM)
	ret = power_supply_set_property(psy_usb,
			(enum power_supply_property)POWER_SUPPLY_PROP_HV_DISABLE, &value);
#endif
	if(ret < 0) {
		pr_err("%s: Fail to set voltage max limit%d\n", __func__, ret);
	} else {     
		pr_info("%s: voltage max limit set to (%d) \n", __func__, value.intval);                
	}

	ret = sec_set_param(param_index_afc_disable, &val); 
	if (ret == false) {
		pr_err("%s: set_param failed!!\n", __func__);
		return ret;
	} else
		pr_info("%s: afc_disable:%d (AFC %s)\n", __func__, 
			gafc->afc_disable, gafc->afc_disable ? "Disabled": "Enabled");

	return count;
}
static DEVICE_ATTR(afc_disable, 0664, afc_disable_show, afc_disable_store);

static struct attribute *afc_attributes[] = {
	&dev_attr_afc_disable.attr,
	NULL
};

static const struct attribute_group afc_group = {
	.attrs = afc_attributes,
};

static int afc_sysfs_init(void)
{
	static struct device *afc;
	int ret = 0;

	/* sysfs */
	afc = sec_device_create(0, NULL, "afc");
	if (IS_ERR(afc)) {
		pr_err("%s Failed to create device(afc)!\n", __func__);
		ret = -ENODEV;
	}

	ret = sysfs_create_group(&afc->kobj, &afc_group);
	if (ret) {
		pr_err("%s: afc sysfs fail, ret %d", __func__, ret);
	}

	return 0;
}

static int gpio_afc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np= dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

	/* sysfs */
	afc_sysfs_init();

	gafc = devm_kzalloc(&pdev->dev, sizeof(*gafc), GFP_KERNEL);
	if (!gafc) {
		pr_info("%s, gafc allocation fail, just return!\n", __func__);
		return -ENOMEM;
	}

	ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
	if (ret < 0) {
		pr_info("%s, could not get afc_switch_gpio\n", __func__);
		return ret;
	} else {
		gafc->afc_switch_gpio = ret;
		pr_info("%s, gafc->afc_switch_gpio: %d\n", __func__, gafc->afc_switch_gpio);
		ret = gpio_request(gafc->afc_switch_gpio, "GPIO44");
	}

	ret = of_get_named_gpio(np,"afc_data_gpio", 0);
	if (ret < 0) {
		pr_info("%s, could not get afc_data_gpio\n", __func__);
		return ret;
	} else {
		gafc->afc_data_gpio = ret;
		pr_info("%s, gafc->afc_data_gpio: %d\n", __func__, gafc->afc_data_gpio);
		ret = gpio_request(gafc->afc_data_gpio, "GPIO01");
	}

	gafc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gafc->pinctrl)) {
		pr_info("%s, No pinctrl config specified\n", __func__);
		return -EINVAL;
	}

	gafc->pin_active = pinctrl_lookup_state(gafc->pinctrl, "active");
	if (IS_ERR(gafc->pin_active)) {
		pr_info("%s, could not get pin_active\n", __func__);
		return -EINVAL;
	}

	gafc->pin_suspend = pinctrl_lookup_state(gafc->pinctrl, "sleep");
	if (IS_ERR(gafc->pin_suspend)) {
		pr_info("%s, could not get pin_suspend\n", __func__);
		return -EINVAL;
	}

	gafc->afc_disable = get_afc_mode();
	spin_lock_init(&gafc->afc_spin_lock);
	INIT_WORK(&gafc->afc_set_voltage_work, afc_set_voltage_workq);

	return 0;
}

static int gpio_afc_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id gpio_afc_of_match_table[] = {
    {
        .compatible = "gpio_afc",
    },
    { },
};
MODULE_DEVICE_TABLE(of, gpio_afc_of_match_table);
#endif

static struct platform_driver gpio_afc_driver = {
	.probe      = gpio_afc_probe,
	.remove		= gpio_afc_remove,
	.driver     = {
		.name   = "gpio_afc",
#if defined(CONFIG_OF)
		.of_match_table = gpio_afc_of_match_table,
#endif
	}
};

static int __init gpio_afc_init(void)
{
	return platform_driver_register(&gpio_afc_driver);
}
module_init(gpio_afc_init);

static void __exit gpio_afc_exit(void)
{
	platform_driver_unregister(&gpio_afc_driver);
}
module_exit(gpio_afc_exit);

MODULE_DESCRIPTION("GPIO AFC DRIVER");
