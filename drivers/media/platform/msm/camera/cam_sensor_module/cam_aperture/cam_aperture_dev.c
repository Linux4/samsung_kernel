/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_aperture_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_aperture_soc.h"
#include "cam_aperture_core.h"
#include "cam_trace.h"
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_ois_core.h"
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
extern struct cam_ois_ctrl_t *g_o_ctrl;
#endif

struct cam_aperture_ctrl_t *g_cam_aperture_t;

extern struct class *camera_class; /*sys/class/camera*/

static long cam_aperture_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_aperture_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_aperture_driver_cmd(a_ctrl, arg);
		break;
	default:
		CAM_ERR(CAM_APERTURE, "Invalid ioctl cmd");
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_aperture_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_APERTURE,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		cmd = VIDIOC_CAM_CONTROL;
		rc = cam_aperture_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc < 0) {
			CAM_ERR(CAM_APERTURE,
				"Failed in aperture subdev handling");
			return rc;
		}
	break;
	default:
		CAM_ERR(CAM_APERTURE, "Invalid compat ioctl: %d", cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_APERTURE,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}
#endif

static int cam_aperture_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_aperture_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	if (!a_ctrl) {
		CAM_ERR(CAM_APERTURE, "a_ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->aperture_mutex));
	cam_aperture_shutdown(a_ctrl);
	mutex_unlock(&(a_ctrl->aperture_mutex));

	return 0;
}

static struct v4l2_subdev_core_ops cam_aperture_subdev_core_ops = {
	.ioctl = cam_aperture_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_aperture_init_subdev_do_ioctl,
#endif
};

static struct v4l2_subdev_ops cam_aperture_subdev_ops = {
	.core = &cam_aperture_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_aperture_internal_ops = {
	.close = cam_aperture_subdev_close,
};
static int cam_aperture_init_subdev(struct cam_aperture_ctrl_t *a_ctrl)
{
	int rc = 0;

	a_ctrl->v4l2_dev_str.internal_ops =
		&cam_aperture_internal_ops;
	a_ctrl->v4l2_dev_str.ops =
		&cam_aperture_subdev_ops;
	strlcpy(a_ctrl->device_name, CAMX_APERTURE_DEV_NAME,
		sizeof(a_ctrl->device_name));
	a_ctrl->v4l2_dev_str.name =
		a_ctrl->device_name;
	a_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	a_ctrl->v4l2_dev_str.ent_function =
		CAM_APERTURE_DEVICE_TYPE;
	a_ctrl->v4l2_dev_str.token = a_ctrl;

	rc = cam_register_subdev(&(a_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_SENSOR, "Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

static int32_t cam_aperture_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	int32_t i = 0;
	struct cam_aperture_ctrl_t *a_ctrl;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_aperture_soc_private *soc_private = NULL;

#if 0//TEMP_845
	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_APERTURE, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}
#endif
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_APERTURE, "%s :: i2c_check_functionality failed",
			 client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	a_ctrl = kzalloc(sizeof(*a_ctrl), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	i2c_set_clientdata(client, a_ctrl);

	soc_private = kzalloc(sizeof(struct cam_aperture_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_ctrl;
	}
	a_ctrl->soc_info.soc_private = soc_private;

	a_ctrl->io_master_info.client = client;
	soc_info = &a_ctrl->soc_info;
	soc_info->dev = &client->dev;
	soc_info->dev_name = client->name;
	a_ctrl->io_master_info.master_type = I2C_MASTER;

	rc = cam_aperture_parse_dt(a_ctrl, &client->dev);
	if (rc < 0) {
		CAM_ERR(CAM_APERTURE, "failed: cam_sensor_parse_dt rc %d", rc);
		goto free_soc;
	}

    /* Fill sub device id*/
	//a_ctrl->id = a_ctrl->soc_info.index;


	rc = cam_aperture_init_subdev(a_ctrl);
	if (rc)
		goto free_soc;

	if (soc_private->i2c_info.slave_addr != 0)
		a_ctrl->io_master_info.client->addr =
			soc_private->i2c_info.slave_addr;

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.link_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_aperture_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_aperture_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_aperture_apply_request;
	a_ctrl->last_flush_req = 0;

	v4l2_set_subdevdata(&(a_ctrl->v4l2_dev_str.sd), a_ctrl);

	a_ctrl->cam_act_state = CAM_APERTURE_INIT;

	g_cam_aperture_t = a_ctrl; // added
	a_ctrl->is_initialized = false;

#ifdef APERTURE_THREAD
	a_ctrl->is_thread_started = false;
	a_ctrl->aperture_thread = NULL;
	INIT_LIST_HEAD(&(a_ctrl->list_head_thread.list));
	init_waitqueue_head(&(a_ctrl->wait));
	mutex_init(&(a_ctrl->aperture_thread_mutex));
#endif

	return rc;

free_soc:
	kfree(soc_private);
free_ctrl:
	kfree(a_ctrl);
	return rc;
}

static int32_t cam_aperture_platform_remove(struct platform_device *pdev)
{
	struct cam_aperture_ctrl_t  *a_ctrl;
	int32_t rc = 0;

	a_ctrl = platform_get_drvdata(pdev);
	if (!a_ctrl) {
		CAM_ERR(CAM_APERTURE, "Aperture device is NULL");
		return 0;
	}

	kfree(a_ctrl->io_master_info.cci_client);
	a_ctrl->io_master_info.cci_client = NULL;
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	devm_kfree(&pdev->dev, a_ctrl);

	return rc;
}

static int32_t cam_aperture_driver_i2c_remove(struct i2c_client *client)
{
	struct cam_aperture_ctrl_t  *a_ctrl = i2c_get_clientdata(client);
	int32_t rc = 0;

	/* Handle I2C Devices */
	if (!a_ctrl) {
		CAM_ERR(CAM_APERTURE, "Aperture device is NULL");
		return -EINVAL;
	}
	/*Free Allocated Mem */
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	kfree(a_ctrl);
	return rc;
}

static const struct of_device_id cam_aperture_driver_dt_match[] = {
	{.compatible = "qcom,aperture"},
	{}
};


ssize_t cam_aperture_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	if ((g_o_ctrl == NULL) ||
		((g_o_ctrl->io_master_info.master_type == CCI_MASTER) &&
		(g_o_ctrl->io_master_info.cci_client == NULL)) ||
		((g_o_ctrl->io_master_info.master_type == I2C_MASTER) &&
		(g_o_ctrl->io_master_info.client == NULL)))
		return size;
#else
	if ((g_cam_aperture_t == NULL) ||
		((g_cam_aperture_t->io_master_info.master_type == CCI_MASTER) &&
		(g_cam_aperture_t->io_master_info.cci_client == NULL)) ||
		((g_cam_aperture_t->io_master_info.master_type == I2C_MASTER) &&
		(g_cam_aperture_t->io_master_info.client == NULL)))
		return size;
#endif

	switch (buf[0]) {
	case '0':
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
		cam_ois_power_down(g_o_ctrl);
#else
		cam_aperture_power_down(g_cam_aperture_t);
#endif
		CAM_INFO(CAM_APERTURE, "cam_aperture_power_down");
		break;
	case '1':
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
		cam_ois_power_up(g_o_ctrl);
#else
		cam_aperture_power_up(g_cam_aperture_t);
#endif
		CAM_INFO(CAM_APERTURE, "cam_aperture_power_up");
		break;

	default:
		break;
	}

	return size;
}


ssize_t cam_aperture_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if ((g_cam_aperture_t == NULL) ||
		((g_cam_aperture_t->io_master_info.master_type == CCI_MASTER) &&
		(g_cam_aperture_t->io_master_info.cci_client == NULL)) ||
		((g_cam_aperture_t->io_master_info.master_type == I2C_MASTER) &&
		(g_cam_aperture_t->io_master_info.client == NULL)))
		return size;

	switch (buf[0]) {
	case '0':
		cam_aperture_set_mode(&g_cam_aperture_t->io_master_info, CAM_APERTURE_2P4);
		CAM_INFO(CAM_APERTURE, "CAM_APERTURE_2P4");
		break;
	case '1':
		cam_aperture_set_mode(&g_cam_aperture_t->io_master_info, CAM_APERTURE_1P5);
		CAM_INFO(CAM_APERTURE, "CAM_APERTURE_1P5");
		break;
	case '2':
		cam_aperture_set_mode(&g_cam_aperture_t->io_master_info, CAM_APERTURE_1P8);
		CAM_INFO(CAM_APERTURE, "CAM_APERTURE_1P8");
		break;
	default:
		break;
	}
	return size;
}

static DEVICE_ATTR(aperture_power, S_IWUSR, NULL, cam_aperture_power_store);
static DEVICE_ATTR(aperture_test, S_IWUSR, NULL, cam_aperture_test);

static int32_t cam_aperture_driver_platform_probe(
	struct platform_device *pdev)
{
	int32_t rc = 0;
	int32_t i = 0;
	struct cam_aperture_ctrl_t *a_ctrl = NULL;
	struct cam_aperture_soc_private *soc_private = NULL;

	/* Create aperture control structure */
	a_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_aperture_ctrl_t), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	/*fill in platform device*/
	a_ctrl->v4l2_dev_str.pdev = pdev;
	a_ctrl->soc_info.pdev = pdev;
	a_ctrl->soc_info.dev = &pdev->dev;
	a_ctrl->soc_info.dev_name = pdev->name;
	a_ctrl->io_master_info.master_type = CCI_MASTER;

	a_ctrl->io_master_info.cci_client = kzalloc(sizeof(
		struct cam_sensor_cci_client), GFP_KERNEL);
	if (!(a_ctrl->io_master_info.cci_client)) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	soc_private = kzalloc(sizeof(struct cam_aperture_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_cci_client;
	}
	a_ctrl->soc_info.soc_private = soc_private;
	soc_private->power_info.dev = &pdev->dev;

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto free_soc;
	}

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	rc = cam_aperture_parse_dt(a_ctrl, &(pdev->dev));
	if (rc < 0) {
		CAM_ERR(CAM_APERTURE, "Paring aperture dt failed rc %d", rc);
		goto free_mem;
	}

	/* Fill platform device id*/
	pdev->id = a_ctrl->soc_info.index;

	rc = cam_aperture_init_subdev(a_ctrl);
	if (rc)
		goto free_mem;

	if (soc_private->i2c_info.slave_addr != 0)
		a_ctrl->io_master_info.cci_client->sid =
			soc_private->i2c_info.slave_addr >> 1;

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.link_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_aperture_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_aperture_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_aperture_apply_request;
	a_ctrl->bridge_intf.ops.flush_req =
		cam_aperture_flush_request;
	a_ctrl->last_flush_req = 0;

	platform_set_drvdata(pdev, a_ctrl);
	v4l2_set_subdevdata(&a_ctrl->v4l2_dev_str.sd, a_ctrl);
	a_ctrl->cam_act_state = CAM_APERTURE_INIT;

	g_cam_aperture_t = a_ctrl; // added

	return rc;

free_mem:
	kfree(a_ctrl->i2c_data.per_frame);
free_soc:
	kfree(soc_private);
free_cci_client:
	kfree(a_ctrl->io_master_info.cci_client);
free_ctrl:
	devm_kfree(&pdev->dev, a_ctrl);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_aperture_driver_dt_match);

static struct platform_driver cam_aperture_platform_driver = {
	.probe = cam_aperture_driver_platform_probe,
	.driver = {
		.name = "qcom,aperture",
		.owner = THIS_MODULE,
		.of_match_table = cam_aperture_driver_dt_match,
	},
	.remove = cam_aperture_platform_remove,
};

static const struct i2c_device_id i2c_id[] = {
	{APERTURE_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver cam_aperture_driver_i2c = {
	.id_table = i2c_id,
	.probe  = cam_aperture_driver_i2c_probe,
	.remove = cam_aperture_driver_i2c_remove,
	.driver = {
		.name = APERTURE_DRIVER_I2C,
		.owner = THIS_MODULE,
		.of_match_table = cam_aperture_driver_dt_match,
	},
};

static int __init cam_aperture_driver_init(void) // test for aperture i2c writ
{
	int32_t rc = 0;
	struct device *cam_aperture = NULL;

	rc = platform_driver_register(&cam_aperture_platform_driver);
	if (rc < 0) {
		CAM_ERR(CAM_APERTURE,
			"platform_driver_register failed rc = %d", rc);
		return rc;
	}
	rc = i2c_add_driver(&cam_aperture_driver_i2c);
	if (rc)
		CAM_ERR(CAM_APERTURE, "i2c_add_driver failed rc = %d", rc);

	if (camera_class != NULL) {
		cam_aperture = device_create(camera_class, NULL, 0, NULL, "aperture");

		if (cam_aperture != NULL) {
			if (device_create_file(cam_aperture, &dev_attr_aperture_power) < 0) {
				CAM_ERR(CAM_APERTURE, "failed to create device file, %s",
					dev_attr_aperture_power.attr.name);
				rc = -ENOENT;
			}
			if (device_create_file(cam_aperture, &dev_attr_aperture_test) < 0) {
				CAM_ERR(CAM_APERTURE, "Failed to create device file!(%s)!",
					dev_attr_aperture_test.attr.name);
				rc = -ENODEV;
			}
		}
	}
	return rc;
}

static void __exit cam_aperture_driver_exit(void)
{
	platform_driver_unregister(&cam_aperture_platform_driver);
	i2c_del_driver(&cam_aperture_driver_i2c);
}

module_init(cam_aperture_driver_init);
module_exit(cam_aperture_driver_exit);
MODULE_DESCRIPTION("cam_aperture_driver");
MODULE_LICENSE("GPL v2");
