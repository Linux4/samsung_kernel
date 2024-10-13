/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include "is-device-sensor-peri.h"
#include "is-ixc-config.h"
#include "is-core.h"
#include "is-vender-specific.h"
#include "exynos-is-module.h"

#if defined(CONFIG_I2C) || defined(CONFIG_I3C)
static int is_ixc_pin_config(void *ixc_device, int state, enum ixc_type ixc_type)
{
	char *pin_ctrl;
	struct device *ixc_dev;
	struct pinctrl *pinctrl_ixc;
	struct i3c_device *device_i3c;
	struct i2c_client *client_i2c;

	if (!ixc_device)
		return 0;

	switch (ixc_type) {
	case I2C_TYPE:
		client_i2c = (struct i2c_client *)ixc_device;
		ixc_dev = client_i2c->dev.parent->parent;
		break;
	case I3C_TYPE:
		device_i3c = (struct i3c_device *)ixc_device;
		ixc_dev = device_i3c->dev.parent->parent;
		break;
	default:
		ixc_dev = NULL;
		pr_err("%s: Failed to get ixc_device\n", __func__);
		break;
	}

	pin_ctrl = __getname();
	if (!pin_ctrl) {
		pr_err("%s: Failed to get memory for pin_ctrl\n", __func__);
		return -ENOMEM;
	}

	switch (state) {
	case I2C_PIN_STATE_ON:
		snprintf(pin_ctrl, PATH_MAX, "on_i2c");
		break;
	case I2C_PIN_STATE_OFF:
		snprintf(pin_ctrl, PATH_MAX, "off_i2c");
		break;
	default:
		snprintf(pin_ctrl, PATH_MAX, "default");
		break;
	}

	pinctrl_ixc = devm_pinctrl_get_select(ixc_dev, pin_ctrl);
	if (IS_ERR_OR_NULL(pinctrl_ixc))
		pr_err("%s: Failed to configure ixc pin\n", __func__);
	else
		devm_pinctrl_put(pinctrl_ixc);

	__putname(pin_ctrl);

	return 0;
}

int is_ixc_pin_control(struct is_module_enum *module, u32 scenario, u32 value)
{
	int ret = 0;
	struct is_device_sensor_peri *sensor_peri;
	struct is_device_sensor *device;
	struct is_core *core;
	int i2c_config_state = 0;
	u32 i2c_channel = 0;
	struct is_vender_specific *specific;
	enum ixc_type cis_ixc_type = module->pdata->cis_ixc_type;
	int position;
	int sensor_id;
	int dualized_sensor_id;

	if (value)
		i2c_config_state = I2C_PIN_STATE_ON;
	else
		i2c_config_state = I2C_PIN_STATE_OFF;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	device = v4l2_get_subdev_hostdata(module->subdev);
	core = is_get_is_core();

	specific = core->vender.private_data;

	switch (scenario) {
	case SENSOR_SCENARIO_NORMAL:
	case SENSOR_SCENARIO_VISION:
	case SENSOR_SCENARIO_HW_INIT:
		ret = is_ixc_pin_config(device->client, i2c_config_state, cis_ixc_type);
		if (sensor_peri->cis.client) {
			info("%s[%d] cis ixc config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret = is_ixc_pin_config(sensor_peri->cis.client, i2c_config_state, cis_ixc_type);
		}
		if (device->ois) {
			i2c_channel = module->pdata->ois_i2c_ch;

			if (i2c_config_state == I2C_PIN_STATE_ON)
				atomic_inc(&core->ixc_rsccount[i2c_channel]);
			else if (i2c_config_state == I2C_PIN_STATE_OFF)
				atomic_dec(&core->ixc_rsccount[i2c_channel]);

			if (atomic_read(&core->ixc_rsccount[i2c_channel]) == value) {
				info("%s[%d] ois i2c config(%d), position(%d), scenario(%d), i2c_channel(%d)\n",
					__func__, __LINE__,
					i2c_config_state, module->position, scenario, i2c_channel);
				ret |= is_ixc_pin_config(device->ois->client, i2c_config_state, I2C_TYPE);
			}
		}
		if (device->actuator[module->device]) {
			i2c_channel = module->pdata->af_i2c_ch;

			if (i2c_config_state == I2C_PIN_STATE_ON)
				atomic_inc(&core->ixc_rsccount[i2c_channel]);
			else if (i2c_config_state == I2C_PIN_STATE_OFF)
				atomic_dec(&core->ixc_rsccount[i2c_channel]);

			if (atomic_read(&core->ixc_rsccount[i2c_channel]) == value) {
				info("%s[%d] actuator i2c config(%d), position(%d), scenario(%d), i2c_channel(%d)\n",
					__func__, __LINE__,
					i2c_config_state, module->position, scenario, i2c_channel);
				ret |= is_ixc_pin_config(device->actuator[module->device]->client,
							i2c_config_state, I2C_TYPE);
			}
		}
		if (device->aperture) {
			info("%s[%d] aperture i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_ixc_pin_config(device->aperture->client, i2c_config_state, 0);
		}
		if (device->mcu && scenario != SENSOR_SCENARIO_HW_INIT && device->mcu->name != MCU_NAME_INTERNAL) {
			info("%s[%d] mcu i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_ixc_pin_config(device->mcu->client, i2c_config_state, I2C_TYPE);
		}
		break;
	case SENSOR_SCENARIO_OIS_FACTORY:
		if (device->ois) {
			info("%s[%d] ois i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_ixc_pin_config(device->ois->client, i2c_config_state, I2C_TYPE);
		}
		if (device->mcu && device->mcu->name != MCU_NAME_INTERNAL) {
			info("%s[%d] mcu i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_ixc_pin_config(device->mcu->client, i2c_config_state, I2C_TYPE);
		}
		if (device->actuator[module->device]) {
			info("%s[%d] actuator i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret |= is_ixc_pin_config(device->actuator[module->device]->client, i2c_config_state, I2C_TYPE);
		}
		break;
	case SENSOR_SCENARIO_READ_ROM:
		if (module->pdata->rom_id < ROM_ID_MAX) {
			info("%s[%d] eeprom i2c config(%d), rom_id(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->pdata->rom_id, scenario);
			position = is_vendor_get_position_from_rom_id(module->pdata->rom_id);
			sensor_id = is_vendor_get_sensor_id_from_position(position);
			dualized_sensor_id = is_vender_get_dualized_sensorid(position);
			if (specific->rom_valid[module->pdata->rom_id][1] &&
				sensor_id == dualized_sensor_id)
				ret |= is_ixc_pin_config(specific->rom_client[module->pdata->rom_id][1], i2c_config_state, I2C_TYPE);
			else
				ret |= is_ixc_pin_config(specific->rom_client[module->pdata->rom_id][0], i2c_config_state, I2C_TYPE);
		}
		break;
	case SENSOR_SCENARIO_SECURE:
		if (module->client) {
			info("%s[%d] cis i2c config(%d), position(%d), scenario(%d)\n",
				__func__, __LINE__, i2c_config_state, module->position, scenario);
			ret = is_ixc_pin_config(module->client, i2c_config_state, I2C_TYPE);
		}
		break;
	default:
		err("%s[%d] undefined scenario, i2c config(%d), position(%d), scenario(%d)\n",
			__func__, __LINE__, i2c_config_state, module->position, scenario);
		break;
	}

	return ret;
}
#endif
