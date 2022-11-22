/*
 * nxp_tfa9897.c -- nxp ALSA SoC Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "nxp_tfa9897.h"
#include <linux/version.h>
#include <linux/regulator/consumer.h>
static int nxp_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct nxp_priv *nxp;
	nxp = kzalloc(sizeof(struct nxp_priv), GFP_KERNEL);
	if (nxp == NULL)
		return -ENOMEM;
	nxp->devtype = id->driver_data;
	i2c_set_clientdata(i2c, nxp);
	nxp->control_data = i2c;
	return 0;
}
static int __devexit nxp_i2c_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}
static struct of_device_id tfa9897_match_table[] = {
	{ .compatible = "nxp,tfa9897",},
	{},
};
static const struct i2c_device_id nxp_i2c_id[] = {
	{ "nxp", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nxp_i2c_id);
static struct i2c_driver nxp_i2c_driver = {
	.driver = {
		.name = "nxp",
		.owner = THIS_MODULE,
		.of_match_table = tfa9897_match_table,
	},
	.probe  = nxp_i2c_probe,
	.remove = __devexit_p(nxp_i2c_remove),
	.id_table = nxp_i2c_id,
};
static int __init nxp_init(void)
{
	int ret;
	ret = i2c_add_driver(&nxp_i2c_driver);
	if (ret)
		pr_err("Failed to register nxp I2C driver: %d\n", ret);
	else
		pr_info("nxp driver built on %s at %s\n",
			__DATE__,
			__TIME__);
	return ret;
}
module_init(nxp_init);
static void __exit nxp_exit(void)
{
	i2c_del_driver(&nxp_i2c_driver);
}
module_exit(nxp_exit);
MODULE_DESCRIPTION("ALSA SoC nxp driver");
MODULE_AUTHOR("Ryan Lee");
MODULE_LICENSE("GPL");

