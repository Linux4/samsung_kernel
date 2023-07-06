#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>

static struct mutex i2c_lock;
static struct i2c_client *lm36274_client;

int panel_i2c_write(unsigned char reg, unsigned char value)
{
	int ret;

	mutex_lock(&i2c_lock);
	ret = i2c_smbus_write_byte_data(lm36274_client, reg, value);
	mutex_unlock(&i2c_lock);

	if (ret < 0)
		pr_err("%s: error!(%d)\n", __func__, ret);

	return ret;
}

static int panel_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	mutex_init(&i2c_lock);

	lm36274_client = client;

	dev_info(&client->dev, "i2c name : %s\n", lm36274_client->name);
	dev_info(&client->dev, "i2c addr : 0x%02x\n", lm36274_client->addr);

	dev_info(&client->dev, "%s: (%s)(%s)\n", __func__, dev_name(&client->adapter->dev),
			of_node_full_name(client->dev.of_node));

	return 0;
}

static int panel_i2c_remove(struct i2c_client *client)
{
	return 0;
}

MODULE_DEVICE_TABLE(i2c, panel_i2c_id);
static struct i2c_device_id panel_i2c_id[] = {
	{"lm36274", 0},
	{},
};

static const struct of_device_id panel_i2c_dt_ids[] = {
	{ .compatible = "i2c,lm36274" },
	{ }
};
MODULE_DEVICE_TABLE(of, panel_i2c_dt_ids);

static struct i2c_driver panel_i2c_driver = {
	.driver = {
		.name   = "panel_i2c",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(panel_i2c_dt_ids),
	},
	.id_table	= panel_i2c_id,
	.probe		= panel_i2c_probe,
	.remove		= panel_i2c_remove,
};

static int __init panel_i2c_init(void)
{
	return i2c_add_driver(&panel_i2c_driver);
}
module_init(panel_i2c_init);

static void __exit panel_i2c_exit(void)
{
	i2c_del_driver(&panel_i2c_driver);
}
module_exit(panel_i2c_exit);

MODULE_DESCRIPTION("simple panel i2c driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
