
#include <linux/device.h>

#include <linux/kernel.h>

#include <linux/i2c.h>

#include "ntrig_i2c_low.h"

#define DRIVER_NAME	"ntrig_i2c"

#define NTRIG_I2C_ID

#ifdef CONFIG_PM
static ntrig_i2c_suspend_callback suspend_callback;
static ntrig_i2c_resume_callback resume_callback;

void ntrig_i2c_register_pwr_mgmt_callbacks(
	ntrig_i2c_suspend_callback s, ntrig_i2c_resume_callback r)
{
	suspend_callback = s;
	resume_callback = r;
}
EXPORT_SYMBOL_GPL(ntrig_i2c_register_pwr_mgmt_callbacks);

static struct i2c_device_data s_i2c_device_data;

struct i2c_device_data *get_ntrig_i2c_device_data(void)
{
	return &s_i2c_device_data;
}
EXPORT_SYMBOL_GPL(get_ntrig_i2c_device_data);


static int ntrig_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *dev_id)
{
	printk(KERN_DEBUG "in %s\n", __func__);

	s_i2c_device_data.m_i2c_client = client;
	s_i2c_device_data.m_platform_data =
		*(struct ntrig_i2c_platform_data *)client->dev.platform_data;

	return 0;
}

static int ntrig_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	if (suspend_callback)
		return suspend_callback(client, mesg);
	return 0;
}

static int ntrig_i2c_resume(struct i2c_client *client)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	if (resume_callback)
		return resume_callback(client);
	return 0;
}

#endif	/* CONFIG_PM */

static int ntrig_i2c_remove(struct i2c_client *client)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	return 0;
}

static struct i2c_device_id ntrig_i2c_idtable[] = {
		{ DRIVER_NAME, NTRIG_I2C_ID },
		{ }
};

MODULE_DEVICE_TABLE(i2c, ntrig_i2c_idtable);

static struct i2c_driver ntrig_i2c_driver = {
		.driver = {
				.name	= DRIVER_NAME,
				.owner	= THIS_MODULE,
		},
		.class = I2C_CLASS_HWMON,
		.id_table = ntrig_i2c_idtable,
		.suspend = ntrig_i2c_suspend,
		.resume = ntrig_i2c_resume,
		.probe		= ntrig_i2c_probe,
		.remove		= __devexit_p(ntrig_i2c_remove),
};

static int __init ntrig_i2c_init(void)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	return i2c_add_driver(&ntrig_i2c_driver);
}

static void __exit ntrig_i2c_exit(void)
{
	i2c_del_driver(&ntrig_i2c_driver);
}

module_init(ntrig_i2c_init);
module_exit(ntrig_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("N-Trig I2C driver");
