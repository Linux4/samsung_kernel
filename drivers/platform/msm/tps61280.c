/*  Date: 2012/6/13 10:00:00
 *  Revision: 1.4
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file TPS61280.c
   brief This file contains all function implementations for the TPS61280 in linux

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/sensors.h>
#define TPS61280_NAME 			"tps61280"
#define TPS61280_INPUT_DEV_NAME	"TPS61280"
/*HS70 modify by limengxia for modify 0xff to defult at 20200916 start*/
#define TPS61280_REG_NUM		3

static unsigned int  InitDataAddr[TPS61280_REG_NUM] = {0x01, 0x02, 0x03};
static unsigned int InitDataVal[TPS61280_REG_NUM]  = {0x05, 0x0B, 0x1A};
/*HS70 modify by limengxia for modify 0xff to defult at 20200916 end*/
extern char *saved_command_line;

struct tps61280_data {
	struct i2c_client *tps_client;
	unsigned char mode;
	int chip_en;
	int chip_vsel;
	struct input_dev *input;
};

static ssize_t tps61280_show_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tps61280_data *data = dev_get_drvdata(dev);
	int enable;
	enable=gpio_get_value(data->chip_en);
	printk("tps61280_show_status: enable gpio state = %d\n",enable);

	return sprintf(buf,"%d\n",enable);
}
static ssize_t tps61280_store_status(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tps61280_data *data = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if(val == 1) {
		gpio_set_value(data->chip_en, 1);    /*chip_en pin - high : on, low : off*/
		printk(" tps61280 enable \n");
	}
	else if (val == 0) {
		gpio_set_value(data->chip_en, 0);    /*chip_en pin - high : on, low : off*/
		printk("  tps61280 disable\n");
	}
	return count;
}
/*HS60 modify by limengxia for modify 0xff to defult at 20200916 start*/
#if 1
static ssize_t tps61280_show_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tps61280_data *data = dev_get_drvdata(dev);
	int loop,count=0,status;
	unsigned int val[3]={0,0,0};

	for (loop = 0; loop < TPS61280_REG_NUM; loop++) {
		val[loop]=i2c_smbus_read_byte_data(data->tps_client, InitDataAddr[loop]);
		printk(" tps61280 ###### [0x%x]=[0x%x]###\n", InitDataAddr[loop],val[loop]);
	}

    status=i2c_smbus_read_byte_data(data->tps_client,0x05);
	printk(" tps61280 ###### [0x05]=[0x%x]###\n",status);
	count += snprintf(buf+count,PAGE_SIZE,"[01]=0x%x,[02]=0x%x,[03]=0x%x,[05]=0x%x\n",val[0],val[1],val[2],status);

	return count;
}

static ssize_t tps61280_store_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tps61280_data *data = dev_get_drvdata(dev);
	unsigned int loop;

	for (loop = 0; loop < TPS61280_REG_NUM; loop++) {
		i2c_smbus_write_byte_data(data->tps_client, InitDataAddr[loop], InitDataVal[loop]);
		printk(" tps61280 ##[0x%x] write [0x%x]##\n", InitDataAddr[loop], InitDataVal[loop]);
	}
	return count;
}
#endif
static DEVICE_ATTR(tps61280_enable,        0664, tps61280_show_status, tps61280_store_status);
static DEVICE_ATTR(tps61280_reg_ctrl,     0664, tps61280_show_reg, tps61280_store_reg);

static struct attribute *tps61280_attributes[] = {
	&dev_attr_tps61280_enable.attr,
	&dev_attr_tps61280_reg_ctrl.attr,
	NULL,
};
/*HS60 modify by limengxia for modify 0xff to defult at 20200916 end*/
static struct attribute_group tps61280_attribute_group = {
	.attrs = tps61280_attributes
};

static int tps61280_init_func(struct i2c_client *client, unsigned char len)
{
	int ret = 0;
	int i;
	int reg_val=0;

	for(i=0; i<len; i++){
		//msleep(200);
		printk("tps61280 i = %d,start write 0x%x to addr 0x%x\n",i,InitDataVal[i],InitDataAddr[i]);
		ret = i2c_smbus_write_byte_data(client,InitDataAddr[i],InitDataVal[i]);
		if (ret < 0){
			printk("tps61280 i = %d,write 0x%x to addr 0x%x failed, ret = %d\n",i,InitDataVal[i],InitDataAddr[i],ret);
			return ret;
		}
		msleep(10);
		reg_val=i2c_smbus_read_byte_data(client,InitDataAddr[i]);
		printk("tps61280 register 0x0%d=0x%x\n", InitDataAddr[i],reg_val);
	}
	printk("tps61280_init success\n");
	return 0;
}

static int tps61280_parse_dt(struct device *dev,
				struct tps61280_data *pdata)
{
	//struct device_node *np = dev->of_node;
	return 0;
}

#define  TPS_WAIT_INTERVAL     (1000 * HZ)


//static struct of_device_id tps61280_match_table[];

static int tps61280_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tps61280_data *data = {NULL};
	struct input_dev *dev;
	int ret = 0;

	printk("tps61280 probe start\n");
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		printk("tps61280 i2c_check failed\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		data = devm_kzalloc(&client->dev, sizeof(struct tps61280_data), GFP_KERNEL);
		if (!data) {
			return -ENOMEM;
		}
		ret = tps61280_parse_dt(&client->dev, data);
		if (ret) {
			dev_err(&client->dev,"tps61280 Unable to parse platfrom data ret=%d\n", ret);
			return ret;
		}
	}else{
		printk("tps61280 probe, dev.of_node error\n");
		goto kfree_exit;
	}

	data->tps_client = client;
	i2c_set_clientdata(client, data);

	data->chip_en = of_get_named_gpio(client->dev.of_node,"tps61280,chip_enable", 0);
	ret = gpio_request(data->chip_en, "tps61280_en");
/* huaqin modify for HQ000003 release tps61280 by limengxia at 2019/07/18 start */
	if(ret) {
		printk("tps61280_en request fail\n");
		goto gpio_error;
	}
/* huaqin modify for HQ000003 release tps61280 by limengxia at 2019/07/18 end */
	printk("tps61280 enable = %d\n ",gpio_get_value(data->chip_en));
	//mdelay(10);
	gpio_direction_output(data->chip_en, 1);
	//gpio_set_value(data->chip_en, 1);
	//mdelay(10);
	printk("tps61280 enable = %d\n ",gpio_get_value(data->chip_en));
	#if 0
	data->chip_vsel = of_get_named_gpio(client->dev.of_node,"tps61280,chip_vsel",0);
	ret = gpio_request(data->chip_vsel, "tps61280_vsel");
	if(ret) {
		printk("tps61280_en request fail\n");
	}
	printk("tps61280 vsel = %d\n ",gpio_get_value(data->chip_vsel));
	//mdelay(10);
	gpio_direction_output(data->chip_vsel, 0);
	//mdelay(10);
	printk("tps61280 vsel = %d\n ",gpio_get_value(data->chip_vsel));
	#endif
/* huaqin modify for HQ000003 release tps61280 by limengxia at 2019/07/18 start */
	ret = i2c_smbus_write_byte_data(client,0x01,0x80);

	if(ret){
		printk("tps61280 i2c_smbus_write_byte_data(client,0x01,0x80) failed ret=%d\n",ret);
	}
	ret = tps61280_init_func(client,TPS61280_REG_NUM);
	if(ret){
		printk(KERN_INFO "init TPS61280 device error\n");
		goto init_error;
	}

	dev = input_allocate_device();
	if (!dev){
		printk("tps61280 input allocate device error\n");
		return -ENOMEM;
	}
/* huaqin modify for HQ000003 release tps61280 by limengxia at 2019/07/18 end */
	dev->name = TPS61280_INPUT_DEV_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
	}
	data->input = dev;

	ret = sysfs_create_group(&data->input->dev.kobj,&tps61280_attribute_group);
	if (ret < 0)
		goto error_sysfs;

	printk("tps61280 probe OK \n");
	return 0;

error_sysfs:
	input_unregister_device(data->input);
/* huaqin add for HQ000003 release tps61280 by limengxia at 2019/07/18 start */
init_error:
	if (gpio_is_valid(data->chip_en))
		gpio_free(data->chip_en);
gpio_error:
/* huaqin add for HQ000003 release tps61280 by limengxia at 2019/07/18 end */
kfree_exit:
	//kfree(data);
	printk("tps61280 probe failed \n");
	return ret;
}
static int  tps61280_remove(struct i2c_client *client)
{
	struct tps61280_data *data = i2c_get_clientdata(client);

	//sysfs_remove_group(&data->input->dev.kobj, &tps61280_attribute_group);
	//input_unregister_device(data->input);

	kfree(data);
	return 0;
}

static const struct i2c_device_id tps61280_id[] = {
	{ TPS61280_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma2x2_id);

static struct of_device_id tps61280_match_table[] = {
	{ .compatible = "ti,tps61280", },
	{ },
};

static struct i2c_driver tps61280_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= TPS61280_NAME,
		.of_match_table = tps61280_match_table,
	},
	.id_table	= tps61280_id,
	.probe		= tps61280_probe,
	.remove		= tps61280_remove,

};
static int __init tps61280_init(void)
{
		printk("tps61280 init +++\n");
		return i2c_add_driver(&tps61280_driver);
}

static void __exit tps61280_exit(void)
{
	i2c_del_driver(&tps61280_driver);
}

MODULE_AUTHOR("HongTao guo <guohongtao@yeptelecom.com>");
MODULE_DESCRIPTION("TPS61280 davicom ic driver");
MODULE_LICENSE("GPL");

module_init(tps61280_init);
module_exit(tps61280_exit);
