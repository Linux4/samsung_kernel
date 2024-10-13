/*
 * wingtch charger manage driver
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
//#include <linux/afc/afc.h>
#include <linux/power_supply.h>
#include "gpio_afc.h"
#include "gpio_afc_iio.h"

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
//+temp-add
//#include <linux/sec_sysfs.h>
//-temp-add
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE


static struct afc_dev *gafc = NULL;


static int afc_get_vbus(void);
static int afc_mode = 0;



/**********************************************************************
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
	ret = power_supply_set_property(psy_bat,
			(enum power_supply_property)POWER_SUPPLY_PROP_AFC_RESULT, &value);
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
********************************************************************************/

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
		//bug537103,gudi.wt,20200612,AFC modify udelay for some fail charge
		udelay(150);
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

	pr_err("%s - Start, afc_disabled is %d\n", __func__, gafc->afc_disable);

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

	gafc->afc_type = AFC_9V;
	pr_err("%s,afc_comm success\n",__func__);
	return 0;

afc_fail:
	pr_info("%s, AFC communication has been failed\n", __func__);
	spin_unlock_irq(&gafc->afc_spin_lock);

	return -1;
}

static void afc_set_voltage_workq(struct work_struct *work)
{
	int i = 0, ret = 0, retry_cnt = 0, vbus = 0, vbus_retry = 0;
	//int i = 0, ret = 0, retry_cnt = 0, vbus = 0;

	pr_err("%s, afc_used is :%d, afc_disable is %d\n", __func__, gafc->afc_used, gafc->afc_disable);

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
			pr_err("afc_comm fail, ret:%d\n", ret);
			vbus = afc_get_vbus();
			if (vbus > 3500) {
				pr_info("%s - fail, retry_cnt(%d), err_case(%d)\n", __func__, retry_cnt++, gafc->afc_error);
				gafc->afc_error = 0;
				i = -1;
				/*+P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/
				if (retry_cnt != AFC_RETRY_MAX) {
					msleep(300);
				}
				/*-P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/
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

	msleep(100);

send_result:
	if ((gafc->vol == SET_5V) && (vbus < 5500)) {
		pr_err("%s, Succeed AFC 5V\n", __func__);
		//send_afc_result(SET_5V);
	} else if ((gafc->vol == SET_9V) && (7500 < vbus)) {
		pr_err("%s, Succeed AFC 9V\n", __func__);
		//send_afc_result(SET_9V);
	} else {
		pr_err("%s, Failed AFC\n", __func__);
		//send_afc_result(AFC_FAIL);
	}

end_afc:
	gpio_direction_output(gafc->afc_switch_gpio, 0);
}



int get_afc_mode(void)
{
	pr_info("%s: afc_mode is 0x%02x\n", __func__, (afc_mode & 0x1));

	if (afc_mode & 0x1)
		return 1;
	else
		return 0;
}

int afc_set_voltage(int vol)
{

	pr_info("%s, set_voltage(%d)\n", __func__, vol);

	if (!gafc) {
		pr_info("%s, zjcset_voltage(%d)\n", __func__, vol);
		return 0;
	}

	gafc->vol = vol;
	pr_info("%s, set_voltage(%d)\n", __func__, vol);
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
		pr_info("%s\n", __func__);
		gafc->afc_type = AFC_FAIL;
		gafc->afc_used = false;
		cancel_work_sync(&gafc->afc_set_voltage_work);
		gpio_direction_output(gafc->afc_switch_gpio, 0);

}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */

static int __init set_afc_mode(char *str)
{
	get_option(&str, &afc_mode);
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("afc_disable", set_afc_mode);


//////////////////////////////////////////////below check OK 1 /////////////////////
static int gpio_afc_parse_dt(struct afc_dev *gafc)
{
	struct device_node *np = gafc->dev->of_node;
	int ret = 0;

	if (!np) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
	if (ret < 0) {
		pr_info("%s, could not get afc_switch_gpio\n", __func__);
		return ret;
	} else {
		gafc->afc_switch_gpio = ret;
		pr_info("%s, gafc->afc_switch_gpio: %d\n", __func__, gafc->afc_switch_gpio);
		ret = gpio_request(gafc->afc_switch_gpio, "GPIO5");
	}

	ret = of_get_named_gpio(np,"afc_data_gpio", 0);
	if (ret < 0) {
		pr_info("%s, could not get afc_data_gpio\n", __func__);
		return ret;
	} else {
		gafc->afc_data_gpio = ret;
		pr_info("%s, gafc->afc_data_gpio: %d\n", __func__, gafc->afc_data_gpio);
		ret = gpio_request(gafc->afc_data_gpio, "GPIO37");
	}

	gafc->pinctrl = devm_pinctrl_get(&gafc->pdev->dev);
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

	return ret;
};


struct iio_channel ** afc_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}



static bool  afc_is_wtchg_chan_valid(struct afc_dev *gafc,
		enum wtcharge_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!gafc->wtchg_ext_iio_chans) {
		iio_list = afc_get_ext_channels(gafc->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(gafc->dev, "Failed to get channels, %d\n",
					rc);
				gafc->wtchg_ext_iio_chans = NULL;
			}
			return false;
		}
		gafc->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}


/*static int afc_write_iio_prop(struct afc_dev *gafc,
				int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	if (!afc_is_wtchg_chan_valid(gafc, channel)) {
		return -ENODEV;
	}

	iio_chan_list = gafc->wtchg_ext_iio_chans[channel];
	ret = iio_write_channel_raw(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}*/

static int afc_read_iio_prop(struct afc_dev *gafc,
				int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	if (!afc_is_wtchg_chan_valid(gafc, channel)) {
		return -ENODEV;
	}

	iio_chan_list = gafc->wtchg_ext_iio_chans[channel];
	ret = iio_read_channel_processed(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}

static int afc_get_vbus(void)
{
	int val;
	int ret;

	ret = afc_read_iio_prop(gafc, WTCHG_VBUS_VAL_NOW, &val);
	if (ret < 0) {
		//dev_err(gafc, "%s: fail: %d\n", __func__, ret);
		pr_err(" afc_get_vbus error \n");
		val = 0;
	}
	pr_err(" afc_get_vbus :%d \n", val);
	return val;
}


static int afc_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct afc_dev *gafc = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_AFC_DETECT:
		//gafc->afc_type = AFC_5V;
		printk("afc_iio_write_rawPSY_IIO_AFC_DETECT\n");
		rc = afc_set_voltage(SET_9V);
		break;
	case PSY_IIO_AFC_SET_VOL:
		if (val1 < 7500) {
			rc = afc_set_voltage(SET_5V);
		} else if (val1 >= 7500) {
			rc = afc_set_voltage(SET_9V);
		}
		break;
	case PSY_IIO_AFC_DETECH:
		detach_afc();
		break;
	case PSY_IIO_AFC_TYPE:
		gafc->afc_type = val1;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int afc_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct afc_dev *gafc = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;
	switch (chan->channel) {
	case PSY_IIO_AFC_DETECT:
	case PSY_IIO_AFC_SET_VOL:
	case PSY_IIO_AFC_DETECH:
		break;
	case PSY_IIO_AFC_TYPE:
		*val1 = gafc->afc_type;
		break;
	default:
		pr_debug("Unsupported gpio afc IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static ssize_t afc_flag_show(struct class *c,
			  struct class_attribute *attr, char *buf)
{ 
	struct afc_dev *gafc = container_of(c, struct afc_dev,afc_class);
	int ret = 0;
	int vbus = 0;
	vbus = afc_get_vbus();
	if((vbus > 7000)&&(gafc->afc_type == AFC_9V))
		ret = 1;

	return scnprintf(buf, PAGE_SIZE, "%d\n",ret);
}

static CLASS_ATTR_RO(afc_flag);

static struct attribute *afc_attributes[] = {
	&class_attr_afc_flag.attr,

	NULL,
};
static const struct attribute_group afc_attr_group = {
  .attrs = afc_attributes,
};

static const struct attribute_group *afc_attr_groups[] = {
  &afc_attr_group,
  NULL,
};

static struct attribute *afc_class_attrs[] = {
	&class_attr_afc_flag.attr,
	NULL,
};

ATTRIBUTE_GROUPS(afc_class);

static int afc_dev_class(struct afc_dev *gafc)
{
  int rc = -EINVAL;
  if(!gafc){
	pr_err("gafc err, NULL%d\n", rc);
	return rc;
  }
  
  gafc->afc_class.name = "afc";
  gafc->afc_class.class_groups = afc_class_groups;
  rc = class_register(&gafc->afc_class);
  if (rc < 0) {
	  pr_err("Failed to create afc_class rc=%d\n", rc);
  }

  gafc->dev->class = &gafc->afc_class;
  dev_set_name(&gafc->afc_device, "afc_node");
  gafc->afc_device.parent = gafc->dev;
  gafc->afc_device.groups = afc_attr_groups;

  rc = device_register(&gafc->afc_device);
  if (rc < 0) {
	  pr_err("afc device_register  rc=%d\n", rc);
  }

  return rc;
}

static int afc_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct afc_dev *gafc = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = gafc->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(gafc_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}


static const struct iio_info afc_iio_info = {
	.read_raw	= afc_iio_read_raw,
	.write_raw	= afc_iio_write_raw,
	.of_xlate	= afc_iio_of_xlate,
};

static int afc_init_ext_iio_psy(struct afc_dev *gafc)
{
	struct iio_channel **iio_list;

	if (!gafc)
		return -ENOMEM;

	iio_list = afc_get_ext_channels(gafc->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
	if (!IS_ERR(iio_list))
		gafc->wtchg_ext_iio_chans = iio_list;
	else {
		pr_err(" wtchg_iio_chan_name error \n");
		gafc->wtchg_ext_iio_chans = NULL;
		return -ENOMEM;
	}

	return 0;
} 


static int afc_init_iio_psy(struct afc_dev *gafc)
{
	struct iio_dev *indio_dev = gafc->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(gafc_iio_psy_channels);
	int rc, i;

	pr_err("afc_init_iio_psy start\n");
	gafc->iio_chan = devm_kcalloc(gafc->dev, num_iio_channels,
				sizeof(*gafc->iio_chan), GFP_KERNEL);
	if (!gafc->iio_chan)
		return -ENOMEM;

	gafc->int_iio_chans = devm_kcalloc(gafc->dev,
				num_iio_channels,
				sizeof(*gafc->int_iio_chans),
				GFP_KERNEL);
	if (!gafc->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &afc_iio_info;
	indio_dev->dev.parent = gafc->dev;
	indio_dev->dev.of_node = gafc->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = gafc->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "gpio_afc";

	for (i = 0; i < num_iio_channels; i++) {
		gafc->int_iio_chans[i].indio_dev = indio_dev;
		chan = &gafc->iio_chan[i];
		gafc->int_iio_chans[i].channel = chan;

		chan->address = i;
		chan->channel = gafc_iio_psy_channels[i].channel_num;
		chan->type = gafc_iio_psy_channels[i].type;
		chan->datasheet_name =
			gafc_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			gafc_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			gafc_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(gafc->dev, indio_dev);
	if (rc)
		pr_err("Failed to register gpio afc IIO device, rc=%d\n", rc);

	pr_err("gpio afc IIO device, rc=%d\n", rc);
	return rc;
}

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#if defined(CONFIG_DRV_SAMSUNG) && defined(CONFIG_QGKI_BUILD)
//#ifdef CONFIG_QGKI_BUILD
 static ssize_t afc_disable_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	struct afc_dev *gafc = dev_get_drvdata(dev);

	if (gafc->afc_disable) {
		pr_err("%s AFC DISABLE --\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_err("%s AFC ENABLE --", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t afc_disable_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct afc_dev *gafc = dev_get_drvdata(dev);
	int ret = 0;
	union power_supply_propval psy_val;
	struct power_supply *psy = power_supply_get_by_name("battery");

	if (!strncasecmp(buf, "1", 1)) {
		gafc->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		gafc->afc_disable = false;
	} else {
		pr_err("%s invalid value --\n", __func__);
		return count;
	}

/*
#if defined(CONFIG_SEC_PARAM)
	ret = sec_set_param(CM_OFFSET + 1, gafc->afc_disable ? '1' : '0');
	if (ret < 0)
		pr_err("%s:sec_set_param failed --\n", __func__);
#endif
*/
	psy_val.intval = gafc->afc_disable;
	if (!IS_ERR_OR_NULL(psy)) {
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_HV_DISABLE, &psy_val);
		if (ret < 0) {
			pr_err("%s: Fail to set POWER_SUPPLY_PROP_HV_DISABLE (%d) --\n", __func__, ret);
		} else {
			pr_err("%s: Success to set POWER_SUPPLY_PROP_HV_DISABLE (%d), val is %d --\n", __func__, ret, psy_val.intval);
		}
	} else {
		pr_err("%s: power_supply battery is NULL --\n", __func__);
	}

	pr_err("%s afc_disable(%d) --\n", __func__, gafc->afc_disable);

	return count;
}

static DEVICE_ATTR_RW(afc_disable);
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

static int gpio_afc_probe(struct platform_device *pdev)
{

	//struct afc_dev *gafc_chg = NULL;
	struct iio_dev *indio_dev;
	int ret;

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
/*
#ifdef CONFIG_QGKI_BUILD
	union power_supply_propval psy_val;
	struct power_supply *psy = power_supply_get_by_name("battery");
#endif
*/
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

	pr_err("gpio afc probe start\n");
	if (!pdev->dev.of_node)
		return -ENODEV;

	pr_err("gpio_afc probe start 1\n");

	gafc = devm_kzalloc(&pdev->dev, sizeof(*gafc), GFP_KERNEL);
	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*gafc));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}
	//gafc = gafc_chg;

	gafc = iio_priv(indio_dev);
	gafc->indio_dev = indio_dev;
	gafc->dev = &pdev->dev;
	gafc->pdev = pdev;
	platform_set_drvdata(pdev, gafc);

	pr_err("wt_chg probe start 2\n");

	ret = afc_init_iio_psy(gafc);
	if (ret < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", ret);
	}

	ret = afc_init_ext_iio_psy(gafc);
	if (ret < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", ret);
	}

	ret = afc_dev_class(gafc);
	if (ret < 0) {
		pr_err("Couldn't afc_dev_class rc=%d\n", ret);
		return ret;
	}

	ret = gpio_afc_parse_dt(gafc);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	get_afc_mode();

	spin_lock_init(&gafc->afc_spin_lock);
	INIT_WORK(&gafc->afc_set_voltage_work, afc_set_voltage_workq);

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#if defined(CONFIG_DRV_SAMSUNG) && defined(CONFIG_QGKI_BUILD)
//#ifdef CONFIG_QGKI_BUILD
/*
	gafc->afc_disable = (get_afc_mode() == '1' ? 1 : 0);
	psy_val.intval = gafc->afc_disable;
	pr_err("%s afc_disable(%d)-1\n", __func__, gafc->afc_disable);
	if (!IS_ERR_OR_NULL(psy)) {
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_HV_DISABLE, &psy_val);
		if (ret < 0) {
			pr_err("%s: Fail to set POWER_SUPPLY_PROP_HV_DISABLE (%d)\n", __func__, ret);
		}
	} else {
		pr_err("%s: power_supply battery is NULL\n", __func__);
	}
*/
	gafc->switch_device = sec_device_create(gafc, "switch");
	if (IS_ERR(gafc->switch_device)) {
		pr_err("%s Failed to create device(switch)! --\n", __func__);
	} else {
		ret = sysfs_create_file(&gafc->switch_device->kobj,
								&dev_attr_afc_disable.attr);
		if (ret) {
			pr_err("%s Failed to create afc_disable sysfs --\n", __func__);
			return ret;
		}
	}
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

	return 0;
}

static int gpio_afc_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static const struct of_device_id gpio_afc_of_match_table[] = {
	{.compatible = "gpio_afc"},
	{},
};

static struct platform_driver gpio_afc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "gpio_afc",
		.of_match_table = gpio_afc_of_match_table,
	},
	.probe = gpio_afc_probe,
	.remove = gpio_afc_remove,
};

static int __init gpio_afc_init(void)
{
	return platform_driver_register(&gpio_afc_driver);
	pr_err("gpio_afc init end\n");
    return 0;
}

static void __exit gpio_afc_exit(void)
{
	platform_driver_unregister(&gpio_afc_driver);
}

module_init(gpio_afc_init);
module_exit(gpio_afc_exit);

MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("GPIO AFC DRIVER");
MODULE_LICENSE("GPL v2");

