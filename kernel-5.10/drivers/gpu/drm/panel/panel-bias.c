
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

struct _lcm_i2c_dev {
    struct i2c_client *client;
};

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

static int _lcm_i2c_probe(struct i2c_client *client,
              const struct i2c_device_id *id)
{
    pr_info("[LCM][I2C] bias %s\n", __func__);
    pr_info("[LCM][I2C] bias NT: info==>name=%s addr=0x%x\n", client->name,
         client->addr);
    _lcm_i2c_client = client;
    return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
    pr_info("[LCM][I2C] bias %s\n", __func__);
    _lcm_i2c_client = NULL;
    i2c_unregister_device(client);
    return 0;
}

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
    int ret = 0;
    struct i2c_client *client = _lcm_i2c_client;
    char write_data[2] = { 0 };

    if (client == NULL) {
        pr_info("ERROR!! _lcm_i2c_client is null\n");
        return 0;
    }

    write_data[0] = addr;
    write_data[1] = value;
    ret = i2c_master_send(client, write_data, 2);
    if (ret < 0)
        pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

    return ret;
}
EXPORT_SYMBOL(_lcm_i2c_write_bytes);

static const struct of_device_id _lcm_i2c_of_match[] = {
    {
        .compatible = "mediatek,i2c_lcd_bias",
    },
    {}
};
MODULE_DEVICE_TABLE(of, _lcm_i2c_of_match);

static const struct i2c_device_id _lcm_i2c_id[] = {
    { LCM_I2C_ID_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, _lcm_i2c_id);

static struct i2c_driver _lcm_i2c_driver = {
    .id_table = _lcm_i2c_id,
    .probe = _lcm_i2c_probe,
    .remove = _lcm_i2c_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = LCM_I2C_ID_NAME,
        .of_match_table = _lcm_i2c_of_match,
    },
};

module_i2c_driver(_lcm_i2c_driver);

MODULE_AUTHOR("bias");
MODULE_DESCRIPTION("i2c driver");
MODULE_LICENSE("GPL v2");


