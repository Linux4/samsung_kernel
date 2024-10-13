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

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#include "lcm_cust_common.h"

#define LOG_TAG "LCM_COMMON"

#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)

#define LCM_GPIO_DEVICES	"lcm_gpio"
#define LCM_GPIO_MODE_00	0
#define MAX_LCM_GPIO_MODE	8

static struct pinctrl *_lcm_gpio;
static struct pinctrl_state *_lcm_gpio_mode[MAX_LCM_GPIO_MODE];
static unsigned char _lcm_gpio_mode_list[MAX_LCM_GPIO_MODE][128] = {
	 "state_enp_output0",
	 "state_enp_output1",
	 "state_enn_output0",
	 "state_enn_output1",
	 "state_reset_output0",
	 "state_reset_output1",
     "state_vio_output0",
     "state_vio_output1",
 };

static int _lcm_gpio_probe(struct platform_device *pdev){
	int ret;
	unsigned int mode;
	struct device *dev = &pdev->dev;
	 
    pr_debug("[LCM][GPIO] enter %s, %d\n", __func__, __LINE__);
	 
	_lcm_gpio = devm_pinctrl_get(dev);
	if (IS_ERR(_lcm_gpio)) {
		ret = PTR_ERR(_lcm_gpio);
		pr_err("[LCM][ERROR] Cannot find _lcm_gpio!\n");
		return ret;
    }
	for (mode = LCM_GPIO_MODE_00; mode < MAX_LCM_GPIO_MODE; mode++) {
		_lcm_gpio_mode[mode] = pinctrl_lookup_state(_lcm_gpio,_lcm_gpio_mode_list[mode]);
		if (IS_ERR(_lcm_gpio_mode[mode]))
            pr_err("[LCM][ERROR] Cannot find lcm_mode:%d! skip it.\n",mode);
		 }
	pr_debug("[LCM][GPIO] exit %s, %d\n", __func__, __LINE__);
	return 0;
}

 static int _lcm_gpio_remove(struct platform_device *pdev)
 {
	 return 0;
 }

int wingtech_bright_to_bl(int level,int max_bright,int min_bright,int bl_max,int bl_min)
{
	if(!level){
		return 0;
	}else{
		if(level <= min_bright){
			return bl_min;
		}else{
			return ((((int)bl_max - (int)bl_min)*level + ((int)max_bright*(int)bl_min - (int)min_bright*(int)bl_max))/((int)max_bright - (int)min_bright));
		}
	}
}

void lcm_reset_pin(unsigned int mode) 
{
    pr_debug("[LCM][GPIO]lcm_reset_pin mode:%d !\n",mode);
	if((mode==0)||(mode==1)){
		switch (mode){
			case LOW :
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[4]);
				 break;
			case HIGH:
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[5]);
				 break;
		 }
 	}
}

void lcm_bais_enp_enable(unsigned int mode) 
{
    pr_debug("[LCM][GPIO]lcm_bais_enp_enable mode:%d !\n",mode);
	if((mode==0)||(mode==1)){
		switch (mode){
			case LOW :
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[0]);
				 break;
			case HIGH:
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[1]);
				 break;
		 }
 	}
}

void lcm_bais_enn_enable(unsigned int mode) 
{
    pr_debug("[LCM][GPIO]lcm_bais_enn_enable mode:%d !\n",mode);
	if((mode==0)||(mode==1)){
		switch (mode){
			case LOW :
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[2]);
				 break;
			case HIGH:
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[3]);
				 break;
		 }
 	}
}

void lcm_vio_enable(unsigned int mode) 
{
    pr_debug("[LCM][GPIO]lcm_vio_enable mode:%d !\n",mode);
	if((mode==0)||(mode==1)){
		switch (mode){
			case LOW :
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[6]);
				 break;
			case HIGH:
				 pinctrl_select_state(_lcm_gpio, _lcm_gpio_mode[7]);
				 break;
		 }
 	}
}

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
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

static int _lcm_i2c_read_bytes(unsigned char cmd, unsigned char *returnData)
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


int lcm_set_power_reg(unsigned char addr, unsigned char val, unsigned char mask)
{
    unsigned char data = 0;
    unsigned int ret = 0;

    ret = _lcm_i2c_read_bytes(addr, &data);
    if(ret<0){
        LCM_LOGD("[LCM_COMMON][ERROR] %s: %d\n", __func__, ret);
    }
    data &= ~mask;
    data |= val;

    ret = _lcm_i2c_write_bytes(addr, data);
    if(ret<0){
        LCM_LOGD("[LCM_COMMON][ERROR] %s: %d\n", __func__, ret);
    }

    return ret;
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

 static const struct of_device_id _lcm_gpio_of_idss[] = {
	 {.compatible = "mediatek,lcm_gpio",},
	 {},
 };

 static struct platform_driver lcm_gpio_driver = {
	 .driver = {
		 .name = LCM_GPIO_DEVICES,
		 .owner  = THIS_MODULE,
		 .of_match_table = of_match_ptr(_lcm_gpio_of_idss),
	 },
	 .probe = _lcm_gpio_probe,
	 .remove = _lcm_gpio_remove,
 };


static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { 
    { LCM_I2C_ID_NAME, 0 },
	{} 
};

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

static int __init lcm_cust_common_init(void)
{
	LCM_LOGD("%s\n", __func__);
    if (platform_driver_register(&lcm_gpio_driver) != 0) {
		pr_err("[LCM]unable to register LCM GPIO driver.\n");
		return -1;
	}
	if (i2c_add_driver(&lcm_i2c_driver) != 0){
        pr_err("[LCM][I2C] lcm_i2c_driver fail.\n");
		return -1;
    }
	LCM_LOGD("%s success\n", __func__);
	return 0;
}

static void __exit lcm_cust_common_exit(void)
{
	LCM_LOGD("%s\n", __func__);
    platform_driver_unregister(&lcm_gpio_driver);
	i2c_del_driver(&lcm_i2c_driver);
}

module_init(lcm_cust_common_init);
module_exit(lcm_cust_common_exit);

MODULE_DESCRIPTION("lcm cust common Ctrl Funtion Driver");
MODULE_LICENSE("GPL v2");

