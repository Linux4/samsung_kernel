/*
 * Filename: lcm_cust_common.c
 * date:20201209
 * Description: cust common source file
 * Author:samir.liu
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include "lcm_cust_common.h"

#define LOG_TAG "LCM_COMMON"

#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("[ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

int _lcm_i2c_read_bytes(unsigned char cmd, unsigned char *returnData)
{
	char readData = 0;
	int ret = 0;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap = _lcm_i2c_client->adapter;

	msg[0].addr = _lcm_i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &cmd;
	msg[1].addr = _lcm_i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &readData;
	ret = i2c_transfer(adap, msg, 2);
	if(ret < 0) {
		pr_err("[ERROR] _lcm_i2c_read_byte i2c transf err \n");
		return 0;
	}
	*returnData = readData;

	return 1;
}

static int _lcm_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	LCM_LOGD("%s\n", __func__);
	LCM_LOGD("info==>name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	LCM_LOGD("%s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "WT,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

static int __init lcm_i2c_init(void)
{
	LCM_LOGD("%s\n", __func__);
	i2c_add_driver(&lcm_i2c_driver);
	LCM_LOGD("%s success\n", __func__);
	return 0;
}

static void __exit lcm_i2c_exit(void)
{
	LCM_LOGD("%s\n", __func__);
	i2c_del_driver(&lcm_i2c_driver);
}

module_init(lcm_i2c_init);
module_exit(lcm_i2c_exit);

MODULE_DESCRIPTION("lcm cust common Ctrl Funtion Driver");
MODULE_LICENSE("GPL v2");

