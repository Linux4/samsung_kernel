/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "MSM-SENSOR-DRIVER %s:%d " fmt "\n", __func__, __LINE__

/* Header file declaration */
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

#if defined(CONFIG_MACH_MILLET3G_EUR)
#include "sr352_yuv.h"
#include "sr130pc20_yuv.h"

struct yuv_userset {
    unsigned int metering;
    unsigned int exposure;
    unsigned int wb;
    unsigned int iso;
    unsigned int effect;
    unsigned int scenemode;
};

struct yuv_ctrl {
    struct yuv_userset settings;
    int op_mode;
	int prev_mode;
};

static struct yuv_ctrl sr352_ctrl;
static struct yuv_ctrl sr130pc20_ctrl;

static int32_t streamon = 0;
static int32_t resolution = MSM_SENSOR_RES_FULL;

int32_t sr130pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr130pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);

int32_t sr352_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr352_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);

static struct msm_sensor_fn_t sr352_sensor_func_tbl = {
	.sensor_config = sr352_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr352_sensor_native_control,
};

static struct msm_sensor_fn_t sr130pc20_sensor_func_tbl = {
	.sensor_config = sr130pc20_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr130pc20_sensor_native_control,
};

#endif

#define MSM_SENSOR_DRIVER_DEBUG
/* Logging macro */
#undef CDBG
#ifdef MSM_SENSOR_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

/* Static declaration */
static struct msm_sensor_ctrl_t *g_sctrl[MAX_CAMERAS];

static const struct of_device_id msm_sensor_driver_dt_match[] = {
	{.compatible = "qcom,camera"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_sensor_driver_dt_match);

static struct platform_driver msm_sensor_platform_driver = {
	.driver = {
		.name = "qcom,camera",
		.owner = THIS_MODULE,
		.of_match_table = msm_sensor_driver_dt_match,
	},
};

static struct v4l2_subdev_info msm_sensor_driver_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

/* static function definition */
int32_t msm_sensor_driver_probe(void *setting)
{
    int32_t                              rc = 0;
    int32_t                              is_power_off = 0;
    uint16_t                             i = 0, size = 0, off_size = 0;
    uint32_t                             session_id = 0;
    struct msm_sensor_ctrl_t            *s_ctrl = NULL;
    struct msm_camera_cci_client        *cci_client = NULL;
    struct msm_camera_sensor_slave_info *slave_info = NULL;
    struct msm_sensor_power_setting     *power_setting = NULL;
    struct msm_sensor_power_setting     *power_off_setting = NULL;
    struct msm_camera_slave_info        *camera_info = NULL;
    struct msm_camera_power_ctrl_t      *power_info = NULL;

    /* Validate input parameters */
    if (!setting) {
        pr_err("failed: slave_info %p", setting);
        return -EINVAL;
    }

    /* Allocate memory for slave info */
    slave_info = kzalloc(sizeof(*slave_info), GFP_KERNEL);
    if (!slave_info) {
        pr_err("failed: no memory slave_info %p", slave_info);
        return -ENOMEM;
    }

    if (copy_from_user(slave_info, (void *)setting, sizeof(*slave_info))) {
        pr_err("failed: copy_from_user");
        rc = -EFAULT;
        goto FREE_SLAVE_INFO;
    }

    /* Print slave info */
    pr_err("camera id %d", slave_info->camera_id);
    pr_err("slave_addr %x", slave_info->slave_addr);
    CDBG("addr_type %d", slave_info->addr_type);
    CDBG("sensor_id_reg_addr %x",
         slave_info->sensor_id_info.sensor_id_reg_addr);
    CDBG("sensor_id %x", slave_info->sensor_id_info.sensor_id);
    CDBG("size %x", slave_info->power_setting_array.size);

    /* Validate camera id */
    if (slave_info->camera_id >= MAX_CAMERAS) {
        pr_err("failed: invalid camera id %d max %d",
               slave_info->camera_id, MAX_CAMERAS);
        rc = -EINVAL;
        goto FREE_SLAVE_INFO;
    }

    /* Extract s_ctrl from camera id */
    s_ctrl = g_sctrl[slave_info->camera_id];
    if (!s_ctrl) {
        pr_err("failed: s_ctrl %p for camera_id %d", s_ctrl,
               slave_info->camera_id);
        rc = -EINVAL;
        goto FREE_SLAVE_INFO;
    }

#if defined(CONFIG_MACH_MILLET3G_EUR)
    if(slave_info->camera_id == CAMERA_2){
		s_ctrl->func_tbl = &sr130pc20_sensor_func_tbl ;
		
    }else if (slave_info->camera_id == CAMERA_0){
    		s_ctrl->func_tbl = &sr352_sensor_func_tbl ;
    }


#endif 

    CDBG("s_ctrl[%d] %p", slave_info->camera_id, s_ctrl);

    if (s_ctrl->is_probe_succeed == 1) {
        /*
         * Different sensor on this camera slot has been connected
         * and probe already succeeded for that sensor. Ignore this
         * probe
         */
        pr_err("slot %d has some other sensor", slave_info->camera_id);
        kfree(slave_info);
        return 0;
    }

    size = slave_info->power_setting_array.size;
    /* Allocate memory for power setting */
    power_setting = kzalloc(sizeof(*power_setting) * size, GFP_KERNEL);
    if (!power_setting) {
        pr_err("failed: no memory power_setting %p", power_setting);
        rc = -ENOMEM;
        goto FREE_SLAVE_INFO;
    }

    if (copy_from_user(power_setting,
                       (void *)slave_info->power_setting_array.power_setting,
                       sizeof(*power_setting) * size)) {
        pr_err("failed: copy_from_user");
        rc = -EFAULT;
        goto FREE_POWER_SETTING;
    }

    /* Print power setting */
    for (i = 0; i < size; i++) {
        CDBG("seq_type %d seq_val %d config_val %ld delay %d",
             power_setting[i].seq_type, power_setting[i].seq_val,
             power_setting[i].config_val, power_setting[i].delay);
    }

    off_size = slave_info->power_setting_array.off_size;
    if (off_size > 0) {
        /* Allocate memory for power setting */
        power_off_setting = kzalloc(sizeof(*power_off_setting) * off_size, GFP_KERNEL);
        if (!power_off_setting) {
            pr_err("failed: no memory power_setting %p", power_off_setting);
            rc = -ENOMEM;
            goto FREE_POWER_SETTING;
        }

        if (copy_from_user(power_off_setting,
                           (void *)slave_info->power_setting_array.power_off_setting,
                           sizeof(*power_off_setting) * off_size)) {
            pr_err("failed: copy_from_user");
            rc = -EFAULT;
            goto FREE_POWER_OFF_SETTING;
        }

        /* Print power setting */
        for (i = 0; i < off_size; i++) {
            CDBG("seq_type %d seq_val %d config_val %ld delay %d",
                 power_off_setting[i].seq_type, power_off_setting[i].seq_val,
                 power_off_setting[i].config_val, power_off_setting[i].delay);
        }
        is_power_off = 1;
    }

    camera_info = kzalloc(sizeof(struct msm_camera_slave_info), GFP_KERNEL);
    if (!camera_info) {
        pr_err("failed: no memory slave_info %p", camera_info);
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }

    /* Fill power up setting and power up setting size */
    power_info = &s_ctrl->sensordata->power_info;
    power_info->power_setting = power_setting;
    power_info->power_setting_size = size;
    power_info->power_off_setting = power_off_setting;
    power_info->power_off_setting_size = off_size;
    
    s_ctrl->sensordata->slave_info = camera_info;

    /* Fill sensor slave info */
    camera_info->sensor_slave_addr = slave_info->slave_addr;
    camera_info->sensor_id_reg_addr =
        slave_info->sensor_id_info.sensor_id_reg_addr;
    camera_info->sensor_id = slave_info->sensor_id_info.sensor_id;

	pr_err("rajeshb in %s line %d camera_info->sensor_id = 0x%X = 0x%X\n",
		__func__, __LINE__, camera_info->sensor_id, s_ctrl->sensordata->slave_info->sensor_id);
    /* Fill CCI master, slave address and CCI default params */
    if (!s_ctrl->sensor_i2c_client) {
        pr_err("failed: sensor_i2c_client %p",
               s_ctrl->sensor_i2c_client);
        rc = -EINVAL;
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }
    /* Fill sensor address type */
    s_ctrl->sensor_i2c_client->addr_type = slave_info->addr_type;
    s_ctrl->sensor_i2c_client->data_type = slave_info->data_type;

    cci_client = s_ctrl->sensor_i2c_client->cci_client;
    if (!cci_client) {
        pr_err("failed: cci_client %p", cci_client);
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }
    cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
    cci_client->sid = slave_info->slave_addr >> 1;
    cci_client->retries = 3;
    cci_client->id_map = 0;

    /* Parse and fill vreg params */
    rc = msm_camera_fill_vreg_params(
                                     power_info->cam_vreg,
                                     power_info->num_vreg,
                                     power_info->power_setting,
                                     power_info->power_setting_size);
    if (rc < 0) {
        pr_err("failed: msm_camera_get_dt_power_setting_data rc %d",
               rc);
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }

    if (power_info->power_off_setting && (power_info->power_off_setting_size > 0)) {
        /* Parse and fill vreg params */
        rc = msm_camera_fill_vreg_params(
                                         power_info->cam_vreg,
                                         power_info->num_vreg,
                                         power_info->power_off_setting,
                                         power_info->power_off_setting_size);
        if (rc < 0) {
            pr_err("failed: msm_camera_get_dt_power_setting_data rc %d",
                   rc);
            if (is_power_off)
                goto FREE_POWER_OFF_SETTING;
            else
                goto FREE_POWER_SETTING;
        }
    }
    /* remove this code for DFMS test for MS01 */
#if defined(CONFIG_MACH_ULC83G_EUR) || defined(CONFIG_MACH_MILLET3G_EUR)  // Added for YUV bringup 
    /* Power up and probe sensor */
    rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl,
                                           &s_ctrl->sensordata->power_info,
                                           s_ctrl->sensor_i2c_client,
                                           s_ctrl->sensordata->slave_info,
                                           slave_info->sensor_name);
    if (rc < 0) {
        pr_err("%s power up failed", slave_info->sensor_name);
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }
#endif

    /* Update sensor name in sensor control structure */
    s_ctrl->sensordata->sensor_name = slave_info->sensor_name;

    /*
      Set probe succeeded flag to 1 so that no other camera shall
      * probed on this slot
      */
    s_ctrl->is_probe_succeed = 1;

    /*
     * Create /dev/videoX node, comment for now until dummy /dev/videoX
     * node is created and used by HAL
     */
    rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
    if (rc < 0) {
        pr_err("failed: camera_init_v4l2 rc %d", rc);
        if (is_power_off)
            goto FREE_POWER_OFF_SETTING;
        else
            goto FREE_POWER_SETTING;
    }
    s_ctrl->sensordata->sensor_info->session_id = session_id;

    /* Create /dev/v4l-subdevX device */
    v4l2_subdev_init(&s_ctrl->msm_sd.sd, s_ctrl->sensor_v4l2_subdev_ops);
    snprintf(s_ctrl->msm_sd.sd.name, sizeof(s_ctrl->msm_sd.sd.name), "%s",
             s_ctrl->sensordata->sensor_name);
    v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, s_ctrl->pdev);
    s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
    s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
    s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
    s_ctrl->msm_sd.sd.entity.name = s_ctrl->msm_sd.sd.name;
    pr_err("%s: name : %s, type = %d group_id = %d\n", __func__, s_ctrl->msm_sd.sd.entity.name, s_ctrl->msm_sd.sd.entity.type, s_ctrl->msm_sd.sd.entity.group_id);
    s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;


    pr_err("FAILED- 3: %d\n", msm_sd_register(&s_ctrl->msm_sd));

    memcpy(slave_info->subdev_name, s_ctrl->msm_sd.sd.entity.name,
           sizeof(slave_info->subdev_name));
    slave_info->is_probe_succeed = 1;

    slave_info->sensor_info.session_id =
        s_ctrl->sensordata->sensor_info->session_id;
    for (i = 0; i < SUB_MODULE_MAX; i++)
        slave_info->sensor_info.subdev_id[i] =
            s_ctrl->sensordata->sensor_info->subdev_id[i];
    slave_info->sensor_info.is_mount_angle_valid =
        s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
    slave_info->sensor_info.sensor_mount_angle =
        s_ctrl->sensordata->sensor_info->sensor_mount_angle;
    CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
         slave_info->sensor_info.sensor_name);
    CDBG("%s:%d session id %d\n", __func__, __LINE__,
         slave_info->sensor_info.session_id);
    for (i = 0; i < SUB_MODULE_MAX; i++)
        CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
             slave_info->sensor_info.subdev_id[i]);
    CDBG("%s:%d mount angle valid %d value %d\n", __func__,
         __LINE__, slave_info->sensor_info.is_mount_angle_valid,
         slave_info->sensor_info.sensor_mount_angle);

    if (copy_to_user((void __user *)setting,
                     (void *)slave_info, sizeof(*slave_info))) {
        pr_err("%s:%d copy failed\n", __func__, __LINE__);
        rc = -EFAULT;
    }

    pr_warn("rc %d session_id %d", rc, session_id);
    pr_warn("%s probe succeeded", slave_info->sensor_name);

    /* remove this code for DFMS test for MS01*/
#if defined(CONFIG_MACH_ULC83G_EUR) || defined(CONFIG_MACH_MILLET3G_EUR) // Added for YUV bringup ToDo.
    /* Power down */
    s_ctrl->func_tbl->sensor_power_down(
                                        s_ctrl,
                                        &s_ctrl->sensordata->power_info,
                                        s_ctrl->sensor_device_type,
                                        s_ctrl->sensor_i2c_client);
#endif

    return rc;

FREE_POWER_OFF_SETTING:
    kfree(power_off_setting);
FREE_POWER_SETTING:
    kfree(power_setting);
FREE_SLAVE_INFO:
    kfree(slave_info);
    return rc;
}

static int32_t msm_sensor_driver_get_gpio_data(
	struct msm_camera_sensor_board_info *sensordata,
	struct device_node *of_node)
{
	int32_t                      rc = 0, i = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	uint16_t                    *gpio_array = NULL;
	uint16_t                     gpio_array_size = 0;

	/* Validate input paramters */
	if (!sensordata || !of_node) {
		pr_err("failed: invalid params sensordata %p of_node %p",
			sensordata, of_node);
		return -EINVAL;
	}

	sensordata->power_info.gpio_conf = kzalloc(
			sizeof(struct msm_camera_gpio_conf), GFP_KERNEL);
	if (!sensordata->power_info.gpio_conf) {
		pr_err("failed");
		return -ENOMEM;
	}
	gconf = sensordata->power_info.gpio_conf;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("gpio count %d", gpio_array_size);
	if (!gpio_array_size)
		return 0;

	gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size, GFP_KERNEL);
	if (!gpio_array) {
		pr_err("failed");
		goto FREE_GPIO_CONF;
	}
	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CDBG("gpio_array[%d] = %d", i, gpio_array[i]);
	}

	rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_GPIO_CONF;
	}

	rc = msm_camera_init_gpio_pin_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_GPIO_REQ_TBL;
	}

	kfree(gpio_array);
	return rc;

FREE_GPIO_REQ_TBL:
	kfree(sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
FREE_GPIO_CONF:
	kfree(sensordata->power_info.gpio_conf);
	kfree(gpio_array);
	return rc;
}

static int32_t msm_sensor_driver_get_dt_data(struct msm_sensor_ctrl_t *s_ctrl,
	struct platform_device *pdev)
{
	int32_t                              rc = 0;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct device_node                  *of_node = pdev->dev.of_node;

	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("failed: no memory");
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;


	/*
	 * Read cell index - this cell index will be the camera slot where
	 * this camera will be mounted
	 */
	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc < 0) {
		pr_err("failed: cell-index rc %d", rc);
		goto FREE_SENSOR_DATA;
	}

	/* Validate pdev->id */
	if (pdev->id >= MAX_CAMERAS) {
		pr_err("failed: invalid pdev->id %d", pdev->id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Check whether g_sctrl is already filled for this pdev id */
	if (g_sctrl[pdev->id]) {
		pr_err("failed: sctrl already filled for id %d", pdev->id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Read subdev info */
	rc = msm_sensor_get_sub_module_index(of_node, &sensordata->sensor_info);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_SENSOR_DATA;
	}

	/* Read vreg information */
	rc = msm_camera_get_dt_vreg_data(of_node,
		&sensordata->power_info.cam_vreg,
		&sensordata->power_info.num_vreg);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_vreg_data rc %d", rc);
		goto FREE_SUB_MODULE_DATA;
	}

	/* Read gpio information */
	rc = msm_sensor_driver_get_gpio_data(sensordata, of_node);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_get_gpio_data rc %d", rc);
		goto FREE_VREG_DATA;
	}

	/* Get CCI master */
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("qcom,cci-master %d, rc %d", s_ctrl->cci_i2c_master, rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	/* Get mount angle */
	rc = of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_info->sensor_mount_angle);
	CDBG("%s qcom,mount-angle %d, rc %d\n", __func__,
		sensordata->sensor_info->sensor_mount_angle, rc);
	if (rc < 0) {
		sensordata->sensor_info->is_mount_angle_valid = 0;
		sensordata->sensor_info->sensor_mount_angle = 0;
		rc = 0;
	} else {
		sensordata->sensor_info->is_mount_angle_valid = 1;
	}

	/* Get vdd-cx regulator */
	/*Optional property, don't return error if absent */
	of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("qcom,misc_regulator %s", sensordata->misc_regulator);

	return rc;

FREE_VREG_DATA:
	kfree(sensordata->power_info.cam_vreg);
FREE_SUB_MODULE_DATA:
	kfree(sensordata->sensor_info);
FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

static int32_t msm_sensor_driver_parse(struct platform_device *pdev)
{
	int32_t                   rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = NULL;

	CDBG("Enter");
	/* Validate input parameters */
	if (!pdev || !pdev->dev.of_node) {
		pr_err("failed: invalid params");
		return -EINVAL;
	}

	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl) {
		pr_err("failed: no memory s_ctrl %p", s_ctrl);
		return -ENOMEM;
	}

	/* Fill platform device */
	s_ctrl->pdev = pdev;

	/* Allocate memory for sensor_i2c_client */
	s_ctrl->sensor_i2c_client = kzalloc(sizeof(*s_ctrl->sensor_i2c_client),
		GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: no memory sensor_i2c_client %p",
			s_ctrl->sensor_i2c_client);
		goto FREE_SCTRL;
	}

	/* Allocate memory for mutex */
	s_ctrl->msm_sensor_mutex = kzalloc(sizeof(*s_ctrl->msm_sensor_mutex),
		GFP_KERNEL);
	if (!s_ctrl->msm_sensor_mutex) {
		pr_err("failed: no memory msm_sensor_mutex %p",
			s_ctrl->msm_sensor_mutex);
		goto FREE_SENSOR_I2C_CLIENT;
	}

	/* Parse dt information and store in sensor control structure */
	rc = msm_sensor_driver_get_dt_data(s_ctrl, pdev);
	if (rc < 0) {
		pr_err("failed: rc %d", rc);
		goto FREE_MUTEX;
	}

	/* Fill device in power info */
	s_ctrl->sensordata->power_info.dev = &pdev->dev;

	/* Initialize mutex */
	mutex_init(s_ctrl->msm_sensor_mutex);

	/* Initilize v4l2 subdev info */
	s_ctrl->sensor_v4l2_subdev_info = msm_sensor_driver_subdev_info;
	s_ctrl->sensor_v4l2_subdev_info_size =
		ARRAY_SIZE(msm_sensor_driver_subdev_info);

	/* Initialize default parameters */
	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_init_default_params rc %d", rc);
		goto FREE_DT_DATA;
	}

	/* Store sensor control structure in static database */
	g_sctrl[pdev->id] = s_ctrl;
	pr_warn("g_sctrl[%d] %p", pdev->id, g_sctrl[pdev->id]);

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata);
FREE_MUTEX:
	kfree(s_ctrl->msm_sensor_mutex);
FREE_SENSOR_I2C_CLIENT:
	kfree(s_ctrl->sensor_i2c_client);
FREE_SCTRL:
	kfree(s_ctrl);
	return rc;
}

static int __init msm_sensor_driver_init(void)
{
	int32_t rc = 0;

	pr_warn("%s : Enter", __func__);
	rc = platform_driver_probe(&msm_sensor_platform_driver,
		msm_sensor_driver_parse);
	if (!rc)
		pr_warn("probe success");
	return rc;
}


static void __exit msm_sensor_driver_exit(void)
{
	CDBG("Enter");
	return;
}

#if defined(CONFIG_MACH_MILLET3G_EUR)


int32_t sr352_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- EV is %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_M4,
				ARRAY_SIZE(sr352_brightness_M4),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M3:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_M3,
				ARRAY_SIZE(sr352_brightness_M3),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M2:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_M2,
				ARRAY_SIZE(sr352_brightness_M2),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M1:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_M1,
				ARRAY_SIZE(sr352_brightness_M1),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_DEFAULT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_default,
				ARRAY_SIZE(sr352_brightness_default),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P1:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_P1,
				ARRAY_SIZE(sr352_brightness_P1),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P2:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_P2,
				ARRAY_SIZE(sr352_brightness_P2),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P3:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_P3,
				ARRAY_SIZE(sr352_brightness_P3),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P4:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_brightness_P4,
				ARRAY_SIZE(sr352_brightness_P4),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- effect is %d", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_effect_none,
				ARRAY_SIZE(sr352_effect_none),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_MONO:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_effect_gray,
				ARRAY_SIZE(sr352_effect_gray),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_effect_negative,
				ARRAY_SIZE(sr352_effect_negative),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_effect_sepia,
				ARRAY_SIZE(sr352_effect_sepia),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- scene is %d", mode);
	switch (mode) {
	case CAMERA_SCENE_AUTO:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_Scene_Off,
				ARRAY_SIZE(sr352_Scene_Off),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_LANDSCAPE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Landscape,
				ARRAY_SIZE(sr352_scene_Landscape),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_SPORT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Sports,
				ARRAY_SIZE(sr352_scene_Sports),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_PARTY:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Party,
				ARRAY_SIZE(sr352_scene_Party),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_BEACH:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Beach,
				ARRAY_SIZE(sr352_scene_Beach),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_SUNSET:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Sunset,
				ARRAY_SIZE(sr352_scene_Sunset),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_DAWN:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Dawn,
				ARRAY_SIZE(sr352_scene_Dawn),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_FALL:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Fall,
				ARRAY_SIZE(sr352_scene_Fall),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_CANDLE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Candle,
				ARRAY_SIZE(sr352_scene_Candle),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_FIRE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Firework,
				ARRAY_SIZE(sr352_scene_Firework),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_AGAINST_LIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Backlight,
				ARRAY_SIZE(sr352_scene_Backlight),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SCENE_NIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_scene_Nightshot,
				ARRAY_SIZE(sr352_scene_Nightshot),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;

	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}


int32_t sr352_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_wb_auto,
				ARRAY_SIZE(sr352_wb_auto),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_wb_incandescent,
				ARRAY_SIZE(sr352_wb_incandescent),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_wb_fluorescent,
				ARRAY_SIZE(sr352_wb_fluorescent),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_wb_sunny,
				ARRAY_SIZE(sr352_wb_sunny),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_wb_cloudy,
				ARRAY_SIZE(sr352_wb_cloudy),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_metering(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- metering is %d", mode);
	switch (mode) {
	case CAMERA_AVERAGE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_metering_matrix,
				ARRAY_SIZE(sr352_metering_matrix),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_CENTER_WEIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_metering_center,
				ARRAY_SIZE(sr352_metering_center),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_SPOT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_metering_spot,
				ARRAY_SIZE(sr352_metering_spot),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- resolution is %d", mode);
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_Capture_2048_1536,
				ARRAY_SIZE(sr352_Capture_2048_1536),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_QTR:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_preview_1024_768,
				ARRAY_SIZE(sr352_preview_1024_768),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_2:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_preview_1024_576,
				ARRAY_SIZE(sr352_preview_1024_576),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_3:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_recording_50Hz_HD,
				ARRAY_SIZE(sr352_recording_50Hz_HD),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_4:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_preview_800_480,
				ARRAY_SIZE(sr352_preview_800_480),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_5:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_preview_640_480,
				ARRAY_SIZE(sr352_preview_640_480),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case MSM_SENSOR_RES_6:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_preview_176_144,
				ARRAY_SIZE(sr352_preview_176_144),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}


int32_t sr352_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);

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
		 CDBG("CFG_SET_INIT_SETTING\n");
		break;
	case CFG_SET_RESOLUTION:
		resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION  res = %d\n " , resolution);
		break;
	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr352_stop_stream \n");
		if(streamon == 1){
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_stop_stream,
				ARRAY_SIZE(sr352_stop_stream),
				MSM_CAMERA_I2C_BYTE_DATA);
			streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr352_start_stream start   \n");
		if( sr352_ctrl.op_mode == CAMERA_MODE_RECORDING ){
			if( resolution == MSM_SENSOR_RES_3 ){
				CDBG("writing *** sr352_recording_50Hz_HD\n");
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr352_recording_50Hz_HD,
					ARRAY_SIZE(sr352_recording_50Hz_HD),
					MSM_CAMERA_I2C_BYTE_DATA);
			} else {
				CDBG("writing *** VGA recording\n");
				sr352_set_resolution( s_ctrl , resolution);
			}
			sr352_ctrl.prev_mode = CAMERA_MODE_RECORDING;
		} else if (sr352_ctrl.op_mode == CAMERA_MODE_CAPTURE ){
			CDBG("writing *** sr352_Capture_2048_1536\n");
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr352_Capture_2048_1536,
				ARRAY_SIZE(sr352_Capture_2048_1536),
				MSM_CAMERA_I2C_BYTE_DATA);
			sr352_ctrl.prev_mode = CAMERA_MODE_CAPTURE;
		} else {
		CDBG("prev_mode = %d\n", sr352_ctrl.prev_mode);
			if((sr352_ctrl.prev_mode == CAMERA_MODE_RECORDING) || (sr352_ctrl.prev_mode == CAMERA_MODE_INIT) ){
				CDBG("writing *** sr352_Init_Reg\n");
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
						i2c_write_conf_tbl(
						s_ctrl->sensor_i2c_client, sr352_Init_Reg,
						ARRAY_SIZE(sr352_Init_Reg),
						MSM_CAMERA_I2C_BYTE_DATA);
				sr352_set_exposure_compensation(s_ctrl, sr352_ctrl.settings.exposure);
				sr352_set_effect(s_ctrl , sr352_ctrl.settings.effect);
				sr352_set_white_balance(s_ctrl , sr352_ctrl.settings.wb);
				sr352_set_metering(s_ctrl, sr352_ctrl.settings.metering );
			}
			CDBG("writing *** sr352_preview\n");
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr352_preview,
					ARRAY_SIZE(sr352_preview),
					MSM_CAMERA_I2C_BYTE_DATA);
			sr352_set_resolution( s_ctrl , resolution);
			sr352_ctrl.prev_mode = CAMERA_MODE_PREVIEW;
		}
		streamon = 1;
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		CDBG("CFG_SET_SLAVE_INFO  \n");
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
		CDBG("CFG_SET_SLAVE_INFO EXIT \n");
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
		streamon = 0;
		sr352_ctrl.op_mode = CAMERA_MODE_INIT;
		sr352_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr352_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
		sr352_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
		sr352_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
		sr352_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
		sr352_ctrl.settings.effect = CAMERA_EFFECT_OFF;
		sr352_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
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

	return rc;
}




int32_t sr352_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;

	mutex_lock(s_ctrl->msm_sensor_mutex);

	CDBG("ENTER \n ");
	/*pr_err("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);*/
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr352_ctrl.settings.exposure = (cam_info->value_1);
		sr352_set_exposure_compensation(s_ctrl, sr352_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr352_ctrl.settings.wb = (cam_info->value_1);
		sr352_set_white_balance(s_ctrl, sr352_ctrl.settings.wb);
		break;
	case EXT_CAM_METERING:
		sr352_ctrl.settings.metering = (cam_info->value_1);
		sr352_set_metering(s_ctrl, sr352_ctrl.settings.metering);
		break;
	case EXT_CAM_EFFECT:
		sr352_ctrl.settings.effect = (cam_info->value_1);
		sr352_set_effect(s_ctrl, sr352_ctrl.settings.effect);
		break;
	case EXT_CAM_SCENE_MODE:
		sr352_ctrl.settings.scenemode = (cam_info->value_1);
		sr352_set_scene_mode(s_ctrl, sr352_ctrl.settings.scenemode);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr352_ctrl.op_mode = (cam_info->value_1);
		pr_err("EXT_CAM_SENSOR_MODE = %d", sr352_ctrl.op_mode);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("EXIT \n ");

	return rc;
}

int32_t sr130pc20_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- EV is %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_M4,
				ARRAY_SIZE(sr130pc20_brightness_M4),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M3:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_M3,
				ARRAY_SIZE(sr130pc20_brightness_M3),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M2:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_M2,
				ARRAY_SIZE(sr130pc20_brightness_M2),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_M1:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_M1,
				ARRAY_SIZE(sr130pc20_brightness_M1),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_DEFAULT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_default,
				ARRAY_SIZE(sr130pc20_brightness_default),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P1:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_P1,
				ARRAY_SIZE(sr130pc20_brightness_P1),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P2:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_P2,
				ARRAY_SIZE(sr130pc20_brightness_P2),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P3:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_P3,
				ARRAY_SIZE(sr130pc20_brightness_P3),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EV_P4:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_brightness_P4,
				ARRAY_SIZE(sr130pc20_brightness_P4),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr130pc20_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_auto,
				ARRAY_SIZE(sr130pc20_wb_auto),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_tungsten,
				ARRAY_SIZE(sr130pc20_wb_tungsten),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_fluorescent,
				ARRAY_SIZE(sr130pc20_wb_fluorescent),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_sunny,
				ARRAY_SIZE(sr130pc20_wb_sunny),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_wb_cloudy,
				ARRAY_SIZE(sr130pc20_wb_cloudy),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr130pc20_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- resolution is %d", mode);
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_Snapshot,
				ARRAY_SIZE(sr130pc20_Snapshot),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_Preview,
				ARRAY_SIZE(sr130pc20_Preview),
				MSM_CAMERA_I2C_BYTE_DATA);
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}

int32_t sr130pc20_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("JAI -- effect is %d", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_effect_none,
				ARRAY_SIZE(sr130pc20_effect_none),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_MONO:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_effect_gray,
				ARRAY_SIZE(sr130pc20_effect_gray),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_effect_negative,
				ARRAY_SIZE(sr130pc20_effect_negative),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr130pc20_effect_sepia,
				ARRAY_SIZE(sr130pc20_effect_sepia),
				MSM_CAMERA_I2C_BYTE_DATA);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}



int32_t sr130pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
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
		resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION  res = %d\n " , resolution);
		break;
	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr130pc20_stop_stream \n");
		if(streamon == 1){
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr130pc20_stop_stream,
					ARRAY_SIZE(sr130pc20_stop_stream),
					MSM_CAMERA_I2C_BYTE_DATA);
				streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr130pc20_start_stream start   \n");
			sr130pc20_set_effect( s_ctrl , sr130pc20_ctrl.settings.effect);
			sr130pc20_set_white_balance( s_ctrl, sr130pc20_ctrl.settings.wb);
			sr130pc20_set_exposure_compensation( s_ctrl , sr130pc20_ctrl.settings.exposure );
			sr130pc20_set_resolution(s_ctrl , resolution );
		streamon = 1;
		CDBG("CFG_SET_START_STREAM : sr130pc20_start_stream rc = %d \n", rc);
		break;
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
		streamon = 0;
		sr130pc20_ctrl.op_mode = CAMERA_MODE_INIT;
		sr130pc20_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr130pc20_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
		sr130pc20_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
		sr130pc20_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
		sr130pc20_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
		sr130pc20_ctrl.settings.effect = CAMERA_EFFECT_OFF;
		sr130pc20_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
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


int32_t sr130pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;

	mutex_lock(s_ctrl->msm_sensor_mutex);

	CDBG("ENTER \n ");
	/*pr_err("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);*/
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr130pc20_ctrl.settings.exposure = cam_info->value_1;
		sr130pc20_set_exposure_compensation(s_ctrl, sr130pc20_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr130pc20_ctrl.settings.wb = cam_info->value_1;
		sr130pc20_set_white_balance(s_ctrl, sr130pc20_ctrl.settings.wb);
		break;
	case EXT_CAM_EFFECT:
		sr130pc20_ctrl.settings.effect = cam_info->value_1;
		sr130pc20_set_effect(s_ctrl, sr130pc20_ctrl.settings.effect);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr130pc20_ctrl.op_mode = cam_info->value_1;
		pr_err("EXT_CAM_SENSOR_MODE = %d", sr130pc20_ctrl.op_mode);
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("EXIT \n ");

	return rc;
}

#endif

module_init(msm_sensor_driver_init);
module_exit(msm_sensor_driver_exit);
MODULE_DESCRIPTION("msm_sensor_driver");
MODULE_LICENSE("GPL v2");
