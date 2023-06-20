/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "sr130pc20_yuv.h"
#define SR130PC20_SENSOR_NAME "sr130pc20"
#define PLATFORM_DRIVER_NAME "msm_camera_sr130pc20"
#define sr130pc20_obj sr130pc20_##obj

#define CAMERA_MODE_CAPTURE		0
#define CAMERA_MODE_PREVIEW		1
#define CAMERA_MODE_RECORDING	2



#define SR130PC20_DEBUG 1
#undef CDBG
#ifdef SR130PC20_DEBUG
#define CDBG(fmt, args...) printk("sr130pc20 : %s:%d : "fmt,   __FUNCTION__, __LINE__, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(sr130pc20_mut);
static struct msm_sensor_ctrl_t sr130pc20_s_ctrl;
static int sensor_mode = CAMERA_MODE_PREVIEW;
static int32_t streamon = 0;

#if 1
static struct msm_sensor_power_setting sr130pc20_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},

	{	//	CAM_IO_EN --> CAM_IO_1.8V
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAMIO_EN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{	//	CAM_ANALOG_EN --> CAM_AVDD_2.8V
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VANA_POWER,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{	//	VTCAM_EN -> VTCAM_1.8
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VIO_POWER,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 12,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_sensor_power_setting sr130pc20_power_off_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 2,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{	//	VTCAM_EN -> VTCAM_1.8
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VIO_POWER,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{	//	CAM_ANALOG_EN --> CAM_AVDD_2.8V
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VANA_POWER,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{	//	CAM_IO_EN --> CAM_IO_1.8V
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAMIO_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

#endif

static struct v4l2_subdev_info sr130pc20_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};


static const struct i2c_device_id sr130pc20_i2c_id[] = {
	{SR130PC20_SENSOR_NAME, (kernel_ulong_t)&sr130pc20_s_ctrl},
	{ }
};

static int32_t msm_sr130pc20_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &sr130pc20_s_ctrl);
}

static struct i2c_driver sr130pc20_i2c_driver = {
	.id_table = sr130pc20_i2c_id,
	.probe  = msm_sr130pc20_i2c_probe,
	.driver = {
		.name = SR130PC20_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client sr130pc20_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id sr130pc20_dt_match[] = {
	{.compatible = "qcom,sr130pc20", .data = &sr130pc20_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, sr130pc20_dt_match);

static struct platform_driver sr130pc20_platform_driver = {
	.driver = {
		.name = "qcom,sr130pc20",
		.owner = THIS_MODULE,
		.of_match_table = sr130pc20_dt_match,
	},
};

static int32_t sr130pc20_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(sr130pc20_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init sr130pc20_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&sr130pc20_platform_driver,
		sr130pc20_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&sr130pc20_i2c_driver);
}

static void __exit sr130pc20_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (sr130pc20_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&sr130pc20_s_ctrl);
		platform_driver_unregister(&sr130pc20_platform_driver);
	} else
		i2c_del_driver(&sr130pc20_i2c_driver);
	return;
}

int32_t sr130pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	static int32_t resolution = CAMERA_MODE_PREVIEW;
	mutex_lock(s_ctrl->msm_sensor_mutex);

	CDBG("ENTER \n ");

	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		CDBG(" CFG_GET_SENSOR_INFO \n");
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_SET_INIT_SETTING:
		CDBG("CFG_SET_INIT_SETTING writing INIT registers: sr130pc20_Init_Reg \n");
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, sr130pc20_Init_Reg,
			ARRAY_SIZE(sr130pc20_Init_Reg),
			MSM_CAMERA_I2C_BYTE_DATA);

		break;
	case CFG_SET_RESOLUTION:
		CDBG("CFG_SET_RESOLUTION \n ");
		resolution = *((int32_t  *)cdata->cfg.setting);

		CDBG("CFG_SET_RESOLUTION  res = %d\n " , resolution);		
		
		break;
	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr130pc20_stop_stream \n");
		if(streamon == 1){
			if(sensor_mode  == CAMERA_MODE_CAPTURE){
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr130pc20_stop_stream,
					ARRAY_SIZE(sr130pc20_stop_stream),
					MSM_CAMERA_I2C_BYTE_DATA);
				streamon = 0;
			}
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr130pc20_start_stream start   \n");

		if((resolution  == CAMERA_MODE_PREVIEW  ||resolution  == CAMERA_MODE_RECORDING)
			&& sensor_mode == CAMERA_MODE_PREVIEW){			

			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_effect_none,
				ARRAY_SIZE(sr130pc20_effect_none),
				MSM_CAMERA_I2C_BYTE_DATA);

			CDBG(" CFG_SET_START_STREAM : sr130pc20_effect_none rc = %d \n", rc);

			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_auto,
				ARRAY_SIZE(sr130pc20_wb_auto),
				MSM_CAMERA_I2C_BYTE_DATA);

			CDBG("CFG_SET_START_STREAM : sr130pc20_wb_auto rc = %d \n", rc);

			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_M4,
				ARRAY_SIZE(sr130pc20_brightness_M4),
				MSM_CAMERA_I2C_BYTE_DATA);

			CDBG("CFG_SET_START_STREAM : sr130pc20_brightness_M4 rc = %d \n", rc);
		}

		if(resolution  == CAMERA_MODE_PREVIEW  ||resolution  == CAMERA_MODE_RECORDING){
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_Preview,
				ARRAY_SIZE(sr130pc20_Preview),
				MSM_CAMERA_I2C_BYTE_DATA);
			
			sensor_mode = CAMERA_MODE_PREVIEW;
			
		}else{
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_Snapshot,
				ARRAY_SIZE(sr130pc20_Snapshot),
				MSM_CAMERA_I2C_BYTE_DATA);
			sensor_mode = CAMERA_MODE_CAPTURE;
		}
		streamon = 1;
		CDBG("CFG_SET_START_STREAM : sr130pc20_start_stream rc = %d \n", rc);
		break;
#if 0		
	case CFG_GET_SENSOR_INIT_PARAMS:
		CDBG("CFG_GET_SENSOR_INIT_PARAMS  \n");
		cdata->cfg.sensor_init_params =
			*s_ctrl->sensordata->sensor_init_params;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
#endif	
#if 0
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_sensor_power_setting_array *power_setting_array;
		int slave_index = 0;
		CDBG("CFG_SET_SLAVE_INFO  \n");
		if (copy_from_user(&sensor_slave_info,
		    (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;
		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
		    (void *)sensor_slave_info.power_setting_array.power_setting,
		    power_setting_array->size *
		    sizeof(struct msm_sensor_power_setting))) {
			kfree(power_setting_array->power_setting);
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->free_power_setting = true;
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				power_setting_array->power_setting[slave_index].
				seq_type,
				power_setting_array->power_setting[slave_index].
				seq_val,
				power_setting_array->power_setting[slave_index].
				config_val,
				power_setting_array->power_setting[slave_index].
				delay);
		}
		kfree(power_setting_array->power_setting);
		break;
	}
#endif
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
				sizeof(sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info.power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(*tmp) * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
			rc = -ENOMEM;
			break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;
		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info.power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(sensor_slave_info.power_setting_array.
				power_setting);
			rc = -EFAULT;
			break;
		}
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG(" CFG_WRITE_I2C_ARRAY  \n");

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		CDBG("CFG_WRITE_I2C_SEQ_ARRAY  \n");

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		CDBG(" CFG_POWER_UP  \n");
		sensor_mode = CAMERA_MODE_PREVIEW;
		streamon = 0;
		if (s_ctrl->func_tbl->sensor_power_up) {
                        CDBG("CFG_POWER_UP");
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_i2c_client,
				s_ctrl->sensordata->slave_info,
				s_ctrl->sensordata->sensor_name);
                } else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		CDBG("CFG_POWER_DOWN  \n");
		 if (s_ctrl->func_tbl->sensor_power_down) {
                        CDBG("CFG_POWER_DOWN");
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_device_type,
				s_ctrl->sensor_i2c_client);
                } else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG("CFG_SET_STOP_STREAM_SETTING  \n");
		
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("EXIT \n ");

	return rc;
}

#if 1

int32_t sr130pc20_sensor_match_id(struct msm_camera_i2c_client *sensor_i2c_client,
	struct msm_camera_slave_info *slave_info,
	const char *sensor_name)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
			sensor_i2c_client,
			slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		CDBG("%s: %s: read id failed\n", __func__,
			sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		 slave_info->sensor_id);
	if (chipid !=  slave_info->sensor_id) {
		CDBG("sr352_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	return rc;
}

#endif

static struct msm_sensor_fn_t sr130pc20_sensor_func_tbl = {
	.sensor_config = sr130pc20_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = sr130pc20_sensor_match_id,
};

static struct msm_sensor_ctrl_t sr130pc20_s_ctrl = {
	.sensor_i2c_client = &sr130pc20_sensor_i2c_client,
#if 1		
	.power_setting_array.power_setting = sr130pc20_power_setting,
	.power_setting_array.size = ARRAY_SIZE(sr130pc20_power_setting),
	.power_setting_array.power_off_setting = sr130pc20_power_off_setting,
	.power_setting_array.off_size = ARRAY_SIZE(sr130pc20_power_off_setting),
#endif	
	.msm_sensor_mutex = &sr130pc20_mut,
	.sensor_v4l2_subdev_info = sr130pc20_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sr130pc20_subdev_info),
	.func_tbl = &sr130pc20_sensor_func_tbl,
};

module_init(sr130pc20_init_module);
module_exit(sr130pc20_exit_module);
MODULE_DESCRIPTION("Siliconfile 1.3MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
