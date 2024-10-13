/*
 * API to parse firmware binary and update codec
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Tushar Behera <tushar.b@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <sound/exynos_regmap_fw.h>

struct fw_private_data {
	/* Device pointer for calling function */
	struct device *dev;
	/* Regmap handle for the target device */
	struct regmap *regmap;
	/* I2C slave address of the target device */
	int i2c_addr;
	/* Function to be called if update is sucessful */
	void (*post_fn) (void *);
	/* Function to be called if update is not sucessful */
	void (*post_fn_failed) (void *);
	/* Argument for post_fn */
	void *post_fn_arg;
	/* Argument for post_fn_failed */
	void *post_fn_failed_arg;
};

/**
 * parse_fw_binary: Parses the firmware binary and updates codec registers
 *
 * The firmware binary contains the default value for codec registers. The data
 * is organized in a group of 4 bytes as below.
 * byte 3: Bus number (not used right now)
 * byte 2: Slave address (used to validate with given i2c address)
 * byte 1: Register offset
 * byte 0: Register value
 *
 * post_fn is called if firmware update is successful.
 * post_fn_failed is called if firmware update is not successful.
 */
static void parse_fw_binary(const struct firmware *fw, void *context)
{
	struct fw_private_data *fw_priv = (struct fw_private_data *)context;
	int i;

	if (fw && fw->size) {
		/* fw data format: 3-bus, 2-addr, 1-reg, 0-val */
		for (i = 0; i < fw->size; i += 4)
			if (fw->data[i+2] == fw_priv->i2c_addr)
				regmap_write(fw_priv->regmap,
						fw->data[i+1], fw->data[i]);
		if (fw_priv->post_fn)
			fw_priv->post_fn(fw_priv->post_fn_arg);
	} else {
		dev_err(fw_priv->dev, "firmware request failed.\n");
		if (fw_priv->post_fn_failed)
			fw_priv->post_fn_failed(fw_priv->post_fn_failed_arg);
	}

	release_firmware(fw);
	kfree(fw_priv);
}

/**
 * exynos_regmap_update_fw: API to update the firmware binary for the codec
 *
 * It loads the firmware binary from user-space and calls an async function to
 * parse and update the Codec registers. Irrespective of the validity of the
 * firmware, it ensures the post update function (post_fn) is called with given
 * argument.
 *
 * Parameters:
 * fw_name: The name of the firmware binary
 * dev: Handle to device structure
 * regmap: Regmap handle for target device
 * i2c_addr: I2C slave address of the target device
 * post_fn: Post update function, called after processing firmware
 * post_fn_arg: Argument to post update function
 */
void exynos_regmap_update_fw(const char *fw_name,
				struct device *dev,
				struct regmap *regmap,
				int i2c_addr,
				void (*post_fn)(void *),
				void *post_fn_arg,
				void (*post_fn_failed)(void *),
				void *post_fn_failed_arg)
{
	struct fw_private_data *fw_priv;

	if (!fw_name || !dev || !regmap){
		pr_err("%s: Some required arguments are NULL\n", __func__);
		goto err;
	}

	/* fw_priv is freed in parse_fw_binary call */
	fw_priv = kzalloc(sizeof(struct fw_private_data), GFP_KERNEL);
	if (!fw_priv) {
		dev_err(dev, "Error allocating memory for fw_priv\n");
		goto err;
	}

	fw_priv->dev = dev;
	fw_priv->regmap = regmap;
	fw_priv->i2c_addr = i2c_addr;
	fw_priv->post_fn = post_fn;
	fw_priv->post_fn_arg = post_fn_arg;
	fw_priv->post_fn_failed = post_fn_failed;
	fw_priv->post_fn_failed_arg = post_fn_failed_arg;

	request_firmware_nowait(THIS_MODULE,
				FW_ACTION_HOTPLUG,
				fw_name,
				dev,
				GFP_KERNEL,
				fw_priv,
				parse_fw_binary);

	return;

err:
	if (post_fn_failed)
		post_fn_failed(post_fn_failed_arg);
}
