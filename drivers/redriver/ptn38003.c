/*
 * combo redriver for ptn38003.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sec_class.h>
#include <linux/combo_redriver/ptn38003.h>

struct ptn38003_data *redrv_data;
struct device *combo_redriver_device;

static char REDRV_MODE_Print[5][19] = {
	{"INIT_MODE"},
	{"USB3_ONLY_MODE"},
	{"DP4_LANE_MODE"},
	{"DP2_LANE_USB3_MODE"},
	{"SAFE_STATE"},
};

void ptn38003_config(int config, int is_DFP)
{
	int is_front = 0;
	u8 value;

	if (!redrv_data) {
		pr_err("%s: Invalid redrv_data\n", __func__);
		return;
	}
	
	if (!redrv_data->i2c) {
		pr_err("%s: Invalid redrv_data->i2c\n", __func__);
		return;
	}
	
	is_front = !gpio_get_value(redrv_data->con_sel);
	pr_info("%s: config(%s)(%s)(%s)\n", __func__, REDRV_MODE_Print[config], (is_DFP ? "DFP":"UFP"), (is_front ? "FRONT":"REAR"));

	switch (config) {
	case INIT_MODE:
		// USB3 Settings
		i2c_smbus_write_byte_data(redrv_data->i2c, LOS_Detector_Threshold, 0x00);
		i2c_smbus_write_byte_data(redrv_data->i2c, USB_Downstream_RX_Control, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, USB_Upstream_RX_Control, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, USB_Upstream_TX_Control, 0x03);
		i2c_smbus_write_byte_data(redrv_data->i2c, USB_Downstream_TX_Control, 0x03);
		// Display Port Settings
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Link_Control, 0x2f);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane0_Control1, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane1_Control1, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane2_Control1, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane3_Control1, 0x0e);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane0_Control2, 0x03);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane1_Control2, 0x03);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane2_Control2, 0x03);
		i2c_smbus_write_byte_data(redrv_data->i2c, DP_Lane3_Control2, 0x03);
		// Configure DFP or UFP application
		i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_DFP ? 0x00:0x10);
		break;

	case SAFE_STATE:
		if (is_DFP)
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x00:0x10);
		else
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x20:0x30);
		break;

	case USB3_ONLY_MODE:
		if (is_DFP)
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x01:0x11);
		else
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x31:0x21);
		break;

	case DP2_LANE_USB3_MODE:
		if (is_DFP)
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x12:0x02);
		else
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x22:0x32);
		break;

	case DP4_LANE_MODE:
		if (is_DFP)
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x03:0x13);
		else
			i2c_smbus_write_byte_data(redrv_data->i2c, Mode_Control, is_front ? 0x23:0x33);
		break;
	}
	value = i2c_smbus_read_byte_data(redrv_data->i2c, Mode_Control);
	pr_info("%s: read Mode_Control(0x04) command as (%x)\n", __func__, value);
}
EXPORT_SYMBOL(ptn38003_config);


static int ptn38003_set_gpios(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (!redrv_data) {
		pr_err("%s: Invalid redrv_data\n", __func__);
		return -ENOMEM;
	}
	
	redrv_data->combo_redriver_en = -1;
	redrv_data->redriver_en = -1;
	redrv_data->con_sel = 0;

	redrv_data->combo_redriver_en = of_get_named_gpio(np, "combo,ptn_en", 0);
	redrv_data->redriver_en = of_get_named_gpio(np, "combo,redriver_en", 0);
	redrv_data->con_sel = of_get_named_gpio(np, "combo,con_sel", 0);
	
	np = of_find_all_nodes(NULL);

	if (gpio_is_valid(redrv_data->combo_redriver_en)) {
		ret = gpio_request(redrv_data->combo_redriver_en, "ptn38003_en");
		pr_info("%s: combo_redriver_en ret gpio_request (%d)\n", __func__, ret);
		ret = gpio_direction_input(redrv_data->combo_redriver_en);
		pr_info("%s: combo_redriver_en ret gpio_direction_input (%d)\n", __func__, ret);
	} else
		pr_info("%s: combo_redriver_en gpio is invalid\n", __func__);

	msleep(1000);

	if (gpio_is_valid(redrv_data->redriver_en)) {
		ret = gpio_request(redrv_data->redriver_en, "ap7341d_redriver_en");
		pr_info("%s: ap7341d_redriver_en ret gpio_request (%d)\n", __func__, ret);
		ret = gpio_direction_output(redrv_data->redriver_en, 1);
		pr_info("%s: ap7341d_redriver_en ret gpio_direction_output (%d)\n", __func__, ret);
	} else
		pr_info("%s: ap7341d_redriver_en gpio is invalid\n", __func__);

	msleep(1000);

	if (gpio_is_valid(redrv_data->redriver_en)) {
		pr_info("%s: after set values (%d) (%d)\n",
			__func__, gpio_get_value(redrv_data->combo_redriver_en), gpio_get_value(redrv_data->redriver_en));
	}

	return 0;
}
#ifdef REDRIVER_TUNE
static void init_usb_control(void)
{
	redrv_data->usbControl_US.BITS.RxEq = 2;
	redrv_data->usbControl_US.BITS.Swing = 1;
	redrv_data->usbControl_US.BITS.DeEmpha = 2;
	pr_info("%s: usbControl_US (%x)\n", __func__, redrv_data->usbControl_US.data);

	redrv_data->usbControl_DS.BITS.RxEq = 2;
	redrv_data->usbControl_DS.BITS.Swing = 1;
	redrv_data->usbControl_DS.BITS.DeEmpha = 1;
	pr_info("%s: usbControl_DS (%x)\n", __func__, redrv_data->usbControl_DS.data);
}
#endif

int ptn38003_i2c_read(u8 command)
{
	if (!redrv_data) {
		pr_err("%s: Invalid redrv_data\n", __func__);
		return -ENOMEM;
	}
	return i2c_smbus_read_byte_data(redrv_data->i2c, command);
}

#ifdef REDRIVER_TUNE
static ssize_t ptn_us_tune_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	return sprintf(buff, "US: 0x%x 0x%x 0x%x Total(0x%x)\n",
		       redrv_data->usbControl_US.BITS.RxEq,
		       redrv_data->usbControl_US.BITS.Swing,
		       redrv_data->usbControl_US.BITS.DeEmpha,
		       redrv_data->usbControl_US.data
		       );
}

static ssize_t ptn_us_tune_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t size)
{

	char *name;
	char *field;
	char *value;
	int	tmp = 0;
	char buf[256], *b, *c;
	char read_data;

	pr_info("%s buff=%s\n", __func__, buff);
	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;

		c =  strim(name);

		field = strsep(&c, "=");
		value = strsep(&c, "=");
		pr_info("usb: %s field=%s  value=%s\n", __func__, field, value);

		if (!strcmp(field, "eq")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_US.BITS.RxEq = tmp;
			pr_info("RxEq value=%d\n", redrv_data->usbControl_US.BITS.RxEq);
			tmp = 0;
		} else if (!strcmp(field, "swing")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_US.BITS.Swing = tmp;
			pr_info("Swing value=%d\n", redrv_data->usbControl_US.BITS.Swing);
			tmp = 0;
		} else if (!strcmp(field, "emp")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_US.BITS.DeEmpha = tmp;
			pr_info("DeEmpha value=%d\n", redrv_data->usbControl_US.BITS.DeEmpha);
			tmp = 0;
		}
	}
	i2c_smbus_write_byte_data(redrv_data->i2c, USB_TXRX_Control, redrv_data->usbControl_US.data);
	read_data = i2c_smbus_read_byte_data(redrv_data->i2c, USB_TXRX_Control);
	pr_info("%s: usbControl_US as (%x)\n", __func__, read_data);
	return size;
}

static DEVICE_ATTR(ptn_us_tune, S_IRUGO | S_IWUSR, ptn_us_tune_show,
					       ptn_us_tune_store);

static ssize_t ptn_ds_tune_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	return sprintf(buff, "DS : 0x%x 0x%x 0x%x Total(0x%x)\n",
		       redrv_data->usbControl_DS.BITS.RxEq,
		       redrv_data->usbControl_DS.BITS.Swing,
		       redrv_data->usbControl_DS.BITS.DeEmpha,
		       redrv_data->usbControl_DS.data
		       );
}

static ssize_t ptn_ds_tune_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t size)
{

	char *name;
	char *field;
	char *value;
	int	tmp = 0;
	char buf[256], *b, *c;
	char read_data;

	pr_info("%s buff=%s\n", __func__, buff);
	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;

		c =  strim(name);

		field = strsep(&c, "=");
		value = strsep(&c, "=");
		pr_info("usb: %s field=%s  value=%s\n", __func__, field, value);

		if (!strcmp(field, "eq")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_DS.BITS.RxEq = tmp;
			pr_info("RxEq value=%d\n", redrv_data->usbControl_DS.BITS.RxEq);
			tmp = 0;
		} else if (!strcmp(field, "swing")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_DS.BITS.Swing = tmp;
			pr_info("Swing value=%d\n", redrv_data->usbControl_DS.BITS.Swing);
			tmp = 0;
		} else if (!strcmp(field, "emp")) {
			sscanf(value, "%d\n", &tmp);
			redrv_data->usbControl_DS.BITS.DeEmpha = tmp;
			pr_info("DeEmpha value=%d\n", redrv_data->usbControl_DS.BITS.DeEmpha);
			tmp = 0;
		}
	}
	i2c_smbus_write_byte_data(redrv_data->i2c, DS_TXRX_Control, redrv_data->usbControl_DS.data);
	read_data = i2c_smbus_read_byte_data(redrv_data->i2c, DS_TXRX_Control);
	pr_info("%s: usbControl_DS as (%x)\n", __func__, read_data);
	return size;
}

static DEVICE_ATTR(ptn_ds_tune, S_IRUGO | S_IWUSR, ptn_ds_tune_show,
					       ptn_ds_tune_store);
#endif

static int ptn38003_probe(struct i2c_client *i2c,
			       const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&i2c->dev, "i2c functionality check error\n");
		return -EIO;
	}

	redrv_data = devm_kzalloc(&i2c->dev, sizeof(*redrv_data), GFP_KERNEL);

	if (!redrv_data)
		return -ENOMEM;

	if (i2c->dev.of_node)
		ptn38003_set_gpios(&i2c->dev);
	else
		dev_err(&i2c->dev, "not found ptn38003 dt ret:%d\n", ret);

	redrv_data->dev = &i2c->dev;
	redrv_data->i2c = i2c;
	i2c_set_clientdata(i2c, redrv_data);

	mutex_init(&redrv_data->i2c_mutex);

#ifdef REDRIVER_TUNE
	init_usb_control();
#endif
	ptn38003_config(INIT_MODE, 0);
	ptn38003_config(SAFE_STATE, 0);

	return ret;
}

static int ptn38003_remove(struct i2c_client *i2c)
{
	struct ptn38003_data *redrv_data = i2c_get_clientdata(i2c);

	if (redrv_data->i2c) {
		mutex_destroy(&redrv_data->i2c_mutex);
		i2c_set_clientdata(redrv_data->i2c, NULL);
	}
	return 0;
}


static void ptn38003_shutdown(struct i2c_client *i2c)
{
	;
}

#define PTN38003_DEV_NAME  "ptn38003"


static const struct i2c_device_id ptn38003_id[] = {
	{ PTN38003_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ptn38003_id);


static const struct of_device_id ptn38003_i2c_dt_ids[] = {
	{ .compatible = "ptn38003_driver" },
	{ }
};


static struct i2c_driver ptn38003_driver = {
	.driver = {
		.name	= PTN38003_DEV_NAME,
		.of_match_table	= ptn38003_i2c_dt_ids,
	},
	.probe		= ptn38003_probe,
	.remove		= ptn38003_remove,
	.shutdown	= ptn38003_shutdown,
	.id_table	= ptn38003_id,
};

#ifdef REDRIVER_TUNE
static struct attribute *ptn38003_attributes[] = {
	&dev_attr_ptn_us_tune.attr,
	&dev_attr_ptn_ds_tune.attr,
	NULL
};

const struct attribute_group ptn38003_sysfs_group = {
	.attrs = ptn38003_attributes,
};
#endif

static int __init ptn38003_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ptn38003_driver);
	pr_info("%s ret is %d\n", __func__, ret);

	combo_redriver_device = sec_device_create(0, NULL, "combo");
	if (IS_ERR(combo_redriver_device))
		pr_err("%s Failed to create device(combo)!\n", __func__);

#ifdef REDRIVER_TUNE
	ret = sysfs_create_group(&combo_redriver_device->kobj, &ptn38003_sysfs_group);
	if (ret)
		pr_err("%s: sysfs_create_group fail, ret %d", __func__, ret);
#endif

	return ret;
}
module_init(ptn38003_init);

static void __exit ptn38003_exit(void)
{
	i2c_del_driver(&ptn38003_driver);
}
module_exit(ptn38003_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ptn38003 combo redriver driver");
MODULE_AUTHOR("lucky29.park <lucky29.park@samsung.com>");



