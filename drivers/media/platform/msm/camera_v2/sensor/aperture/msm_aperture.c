/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_aperture.h"
#include "msm_cci.h"
#include "msm_ois_rumba_s4.h"

DEFINE_MSM_MUTEX(msm_aperture_mutex);

#define MSM_DEBUG
#ifdef MSM_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...)  do { } while (0)
#endif

static struct v4l2_file_operations msm_aperture_v4l2_subdev_fops;
static struct i2c_driver msm_aperture_i2c_driver;
struct msm_aperture_ctrl_t *aperture_ctrl;


#define IRIS_VALUE_F15 15
#define IRIS_VALUE_F24 24

extern struct msm_ois_ctrl_t *g_msm_ois_t;
extern int32_t msm_ois_set_mode(struct msm_ois_ctrl_t *a_ctrl,uint16_t mode);

static int msm_aperture_addr8_write8(struct msm_camera_i2c_client *i2c_client,uint32_t addr, uint16_t data)
{
	int rc = 0;
	rc = i2c_client->i2c_func_tbl->i2c_write(i2c_client, addr, data,MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s Failed I2C write Line %d\n", __func__, __LINE__);
		return rc;
	}
	return rc;
}

int msm_aperture_iris_init(struct msm_aperture_ctrl_t *a_ctrl)
{
	int ret = 0;
	int pos = 0;
	u8 data[2] = {0, };

	struct msm_camera_i2c_client *i2c_client = &a_ctrl->i2c_client;

	if(!a_ctrl || !i2c_client)
	{
		pr_err("%s device or i2c client is null ,return \n",__func__);
		return 0;
	}
	mutex_lock(&msm_aperture_mutex);

	a_ctrl->f_number = IRIS_VALUE_F15;
	ret = msm_aperture_addr8_write8(i2c_client, 0xAE, 0x3B);  /* setting mode */
	pos = 511;
	data[0] = pos >> 2 & 0xFF;
	data[1] = pos << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	ret |= msm_aperture_addr8_write8(i2c_client, 0x02, 0x00); /* active mode */

	//OIS fixed mode
	msm_ois_set_mode(g_msm_ois_t,OIS_MODE_FIXED);
	mdelay(15);
#if 0
	//Set F2.4
	ret |= msm_aperture_addr8_write8(i2c_client, 0xA6, 0x7B); /* open mode */
	pos = 1023;
	data[0] = pos >> 2 & 0xFF;
	data[1] = pos << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(10);
	ret |= msm_aperture_addr8_write8(i2c_client, 0xA6, 0x00); /* close code */
	pos = 511;
	data[0] = pos >> 2 & 0xFF;
	data[1] = pos << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(15);
#else
	//Set F1.5
	ret |= msm_aperture_addr8_write8(i2c_client, 0xA6, 0x7B); /* open mode */
	pos = 0;
	data[0] = pos >> 2 & 0xFF;
	data[1] = pos << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(10);
	ret |= msm_aperture_addr8_write8(i2c_client, 0xA6, 0x00); /* close code */
	pos = 511;
	data[0] = pos >> 2 & 0xFF;
	data[1] = pos << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(15);
#endif
	if (ret < 0) {
		pr_err("%s i2c fail occurred.",__func__);
		goto p_err;
	}

p_err:
	mutex_unlock(&msm_aperture_mutex);

	return ret;
}

static int msm_aperture_iris_set_value(struct msm_aperture_ctrl_t *a_ctrl, int value)
{
	int ret = 0;
	int pos1 = 0, pos2 = 0;
	u8 data[2] = {0, };
	struct msm_camera_i2c_client *i2c_client = &a_ctrl->i2c_client;

	if (unlikely(!i2c_client)) {
		pr_err("client is NULL");
		ret = -EINVAL;
		goto exit;
	}
	if (value == a_ctrl->f_number)
		return ret;

	//OIS fixed mode
	msm_ois_set_mode(g_msm_ois_t,OIS_MODE_FIXED);
	mdelay(15);

	switch (value) {
	case IRIS_VALUE_F15 :
		pos1 = 0;
		pos2 = 511;
		break;
	case IRIS_VALUE_F24 :
		pos1 = 1023;
		pos2 = 511;
		break;
	default:
		CDBG("%s: mode is not set.(mode = %d)\n", __func__, value);
		goto exit;
	}

	mutex_lock(&msm_aperture_mutex);
	ret = msm_aperture_addr8_write8(i2c_client, 0xA6, 0x7B); /* open mode */
	data[0] = pos1 >> 2 & 0xFF;
	data[1] = pos1 << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(10);
	ret |= msm_aperture_addr8_write8(i2c_client, 0xA6, 0x00); /* close code */
	data[0] = pos2 >> 2 & 0xFF;
	data[1] = pos2 << 6 & 0xC0;
	ret |= msm_aperture_addr8_write8(i2c_client, 0x00, data[0]); /* position code */
	ret |= msm_aperture_addr8_write8(i2c_client, 0x01, data[1]);
	mdelay(15);

	msm_ois_set_mode(g_msm_ois_t,OIS_MODE_CENTERING);
	mdelay(20);
	if (ret < 0) {
		pr_err("%s i2c fail occurred.",__func__);
		goto exit;
	}
	a_ctrl->f_number = value;

  exit:
	mutex_unlock(&msm_aperture_mutex);
	return ret;
}

static int32_t msm_aperture_vreg_control(struct msm_aperture_ctrl_t *a_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_aperture_vreg *vreg_cfg;

	vreg_cfg = &a_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_APERTURE_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
			rc = msm_camera_config_single_vreg(&(a_ctrl->pdev->dev),
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
		} else if (a_ctrl->act_device_type ==
			MSM_CAMERA_I2C_DEVICE) {
			rc = msm_camera_config_single_vreg(
				&(a_ctrl->i2c_client.client->dev),
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
		}
	}
	return rc;
}

static int32_t msm_aperture_power_down(struct msm_aperture_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("Enter\n");

	if (a_ctrl->aperture_state != ACT_DISABLE_STATE) {
		rc = msm_aperture_vreg_control(a_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		for (gpio = SENSOR_GPIO_AF_PWDM;
			gpio < SENSOR_GPIO_MAX; gpio++) {
			if (a_ctrl->gconf &&
				a_ctrl->gconf->gpio_num_info &&
				a_ctrl->gconf->gpio_num_info->
					valid[gpio] == 1) {

				gpio_set_value_cansleep(
					a_ctrl->gconf->gpio_num_info->
						gpio_num[gpio],
					GPIOF_OUT_INIT_LOW);

				if (a_ctrl->cam_pinctrl_status) {
					rc = pinctrl_select_state(
						a_ctrl->pinctrl_info.pinctrl,
						a_ctrl->pinctrl_info.
							gpio_state_suspend);
					if (rc < 0)
						pr_err("ERR:%s:%d cannot set pin to suspend state: %d",
							__func__, __LINE__, rc);

					devm_pinctrl_put(
						a_ctrl->pinctrl_info.pinctrl);
				}
				a_ctrl->cam_pinctrl_status = 0;
				rc = msm_camera_request_gpio_table(
					a_ctrl->gconf->cam_gpio_req_tbl,
					a_ctrl->gconf->cam_gpio_req_tbl_size,
					0);
				if (rc < 0)
					pr_err("ERR:%s:Failed in selecting state in aperture power down: %d\n",
						__func__, rc);
			}
		}

		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->aperture_state = ACT_OPS_INACTIVE;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_aperture_power_up(struct msm_aperture_ctrl_t *a_ctrl)
{
	int rc = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("power up\n");

	rc = msm_aperture_vreg_control(a_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	for (gpio = SENSOR_GPIO_AF_PWDM; gpio < SENSOR_GPIO_MAX; gpio++) {
		if (a_ctrl->gconf &&
			a_ctrl->gconf->gpio_num_info &&
			a_ctrl->gconf->gpio_num_info->valid[gpio] == 1) {
			rc = msm_camera_request_gpio_table(
				a_ctrl->gconf->cam_gpio_req_tbl,
				a_ctrl->gconf->cam_gpio_req_tbl_size, 1);
			if (rc < 0) {
				pr_err("ERR:%s:Failed in selecting state for aperture: %d\n",
					__func__, rc);
				return rc;
			}
			if (a_ctrl->cam_pinctrl_status) {
				rc = pinctrl_select_state(
					a_ctrl->pinctrl_info.pinctrl,
					a_ctrl->pinctrl_info.gpio_state_active);
				if (rc < 0)
					pr_err("ERR:%s:%d cannot set pin to active state: %d",
						__func__, __LINE__, rc);
			}

			gpio_set_value_cansleep(
				a_ctrl->gconf->gpio_num_info->gpio_num[gpio],
				1);
		}
	}

	/* VREG needs some delay to power up */
	usleep_range(2000, 3000);
	a_ctrl->aperture_state = ACT_ENABLE_STATE;

	CDBG("Exit\n");
	return rc;
}

static int msm_aperture_init(struct msm_aperture_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	a_ctrl->aperture_state = ACT_OPS_ACTIVE;

	CDBG("Exit\n");
	return rc;
}

static int msm_aperture_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_aperture_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if(a_ctrl->f_number != IRIS_VALUE_F15){
		CDBG("%s f_number is %d, default setting is F1.5\n",__func__,a_ctrl->f_number);
		msm_aperture_iris_set_value(a_ctrl,15);
	}
	mutex_lock(a_ctrl->aperture_mutex);

	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		a_ctrl->aperture_state != ACT_DISABLE_STATE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}

	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
	a_ctrl->aperture_state = ACT_DISABLE_STATE;
	mutex_unlock(a_ctrl->aperture_mutex);

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_aperture_config(struct msm_aperture_ctrl_t *a_ctrl,
	void __user *argp)
{
	struct msm_aperture_cfg_data *cdata =
		(struct msm_aperture_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(a_ctrl->aperture_mutex);

	CDBG(" Enter type %d  f_number %d \n",  cdata->cfgtype, cdata->f_number);
	switch (cdata->cfgtype) {
	case CFG_APERTURE_POWERUP:
		rc = a_ctrl->func_tbl->aperture_power_up(a_ctrl);
		break;
	case CFG_APERTURE_POWERDOWN:
		rc = a_ctrl->func_tbl->aperture_power_down(a_ctrl);
		break;
	case CFG_APERTURE_INIT:
		msm_aperture_init(a_ctrl);
		rc = a_ctrl->func_tbl->aperture_iris_init(a_ctrl);
		break;
	case CFG_APERTURE_SET_VALUE:
		rc |= a_ctrl->func_tbl->aperture_iris_set_value(a_ctrl, cdata->f_number);
		break;
	default:
		break;
	}
	mutex_unlock(a_ctrl->aperture_mutex);

	CDBG("Exit  rc=%d \n",rc);
	return rc;
}

static int32_t msm_aperture_get_subdev_id(struct msm_aperture_ctrl_t *a_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = a_ctrl->pdev->id;
	else
		*subdev_id = a_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static long msm_aperture_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc;
	struct msm_aperture_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;

	CDBG("cmd:%d  expect:%lu Enter\n", cmd,VIDIOC_MSM_APERTURE_CFG);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_aperture_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_APERTURE_CFG:
		return msm_aperture_config(a_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		if (!a_ctrl->i2c_client.i2c_func_tbl) {
			pr_err("a_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		mutex_lock(a_ctrl->aperture_mutex);
		rc = a_ctrl->func_tbl->aperture_power_down(a_ctrl);
		if (rc < 0) {
			pr_err("%s:%d aperture Power down failed\n",
					__func__, __LINE__);
		}
		mutex_unlock(a_ctrl->aperture_mutex);
		return msm_aperture_close(sd, NULL);
	default:
		return -ENOIOCTLCMD;
	}

	CDBG("Exit  rc=%d \n",rc);
	return rc;
}

#ifdef CONFIG_COMPAT
static int msm_aperture_config32(struct msm_aperture_ctrl_t *a_ctrl,
	void __user *argp)
{
	int rc = 0;
	struct msm_aperture_cfg_data32 *cdata32 = (struct msm_aperture_cfg_data32 *)argp;
	struct msm_aperture_cfg_data cdata;

	cdata.cfgtype = cdata32->cfgtype;
	cdata.f_number = cdata32->f_number;
	CDBG("Enter: subdevid: %d type %d  f_number = %d \n",a_ctrl->subdev_id, cdata.cfgtype,cdata.f_number);
	switch (cdata.cfgtype) {
	case CFG_APERTURE_POWERUP:
		rc = a_ctrl->func_tbl->aperture_power_up(a_ctrl);
		if (rc < 0)
			pr_err("Failed aperture power up%d\n", rc);
		break;
	case CFG_APERTURE_POWERDOWN:
		rc = a_ctrl->func_tbl->aperture_power_down(a_ctrl);
		if (rc < 0)
			pr_err("Failed msm_aperture_power_down %d\n", rc);
		break;
	case CFG_APERTURE_INIT:
		msm_aperture_init(a_ctrl);
		rc = a_ctrl->func_tbl->aperture_iris_init(a_ctrl);
		if (rc < 0)
			pr_err("Failed aperture init fail %d\n", rc);
		break;
	case CFG_APERTURE_SET_VALUE:
		rc = a_ctrl->func_tbl->aperture_iris_set_value(a_ctrl,cdata.f_number);
		if (rc < 0)
			pr_err("Failed aperture set value f_number = %d \n", cdata.f_number);
		break;
	default:
		break;
	}

	CDBG("X  rc: %d\n", rc);
	return rc;
}

static long msm_aperture_subdev_ioctl32(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	int rc = 0;
	struct msm_aperture_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;

	CDBG("Enter: cmd=0x%x expect:%lx\n",cmd,VIDIOC_MSM_APERTURE_CFG32);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_aperture_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_APERTURE_CFG32:
		return msm_aperture_config32(a_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}

	CDBG("rc=%d X\n",rc);
	return rc;
}

static long msm_aperture_subdev_do_ioctl32(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return msm_aperture_subdev_ioctl32(sd, cmd, arg);
}

static long msm_aperture_subdev_fops_ioctl32(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	CDBG("Enter \n");
	return video_usercopy(file, cmd, arg, msm_aperture_subdev_do_ioctl32);
}
#endif

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static const struct v4l2_subdev_internal_ops msm_aperture_internal_ops = {
	.close = msm_aperture_close,
};

static struct v4l2_subdev_core_ops msm_aperture_subdev_core_ops = {
	.ioctl = msm_aperture_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_aperture_subdev_ops = {
	.core = &msm_aperture_subdev_core_ops,
};

static const struct i2c_device_id msm_aperture_i2c_id[] = {
	{"qcom,aperture", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_aperture_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	uint32_t temp;
	struct msm_aperture_ctrl_t *act_ctrl_t = NULL;
	struct msm_aperture_vreg *vreg_cfg = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_aperture_i2c_probe: client is null\n");
		return -EINVAL;
	}

	act_ctrl_t = kzalloc(sizeof(struct msm_aperture_ctrl_t),
		GFP_KERNEL);
	if (!act_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%pK\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&act_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", act_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	rc = of_property_read_u32(client->dev.of_node, "qcom,slave-addr",
		&temp);
	if (rc < 0) {
		pr_err("%s failed rc %d\n", __func__, rc);
		goto probe_failure;
	}
	act_ctrl_t->slave_addr = temp;
	if (of_find_property(client->dev.of_node,
		"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &act_ctrl_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data(client->dev.of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto probe_failure;
		}
	}

	act_ctrl_t->i2c_driver = &msm_aperture_i2c_driver;
	act_ctrl_t->i2c_client.client = client;

	/* Set device type as I2C */
	act_ctrl_t->act_device_type = MSM_CAMERA_I2C_DEVICE;
	act_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	act_ctrl_t->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	CDBG("slave_addr 0x%x  client_addr 0x%x ",act_ctrl_t->slave_addr,act_ctrl_t-> i2c_client.client->addr );
	if(act_ctrl_t->slave_addr != 0)
	act_ctrl_t-> i2c_client.client->addr = act_ctrl_t->slave_addr;

	act_ctrl_t->act_v4l2_subdev_ops = &msm_aperture_subdev_ops;
	act_ctrl_t->aperture_mutex = &msm_aperture_mutex;
	/* Assign name for sub device */
	snprintf(act_ctrl_t->msm_sd.sd.name, sizeof(act_ctrl_t->msm_sd.sd.name),
		"%s", act_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&act_ctrl_t->msm_sd.sd,
		act_ctrl_t->i2c_client.client,
		act_ctrl_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&act_ctrl_t->msm_sd.sd, act_ctrl_t);
	act_ctrl_t->msm_sd.sd.internal_ops = &msm_aperture_internal_ops;
	act_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&act_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	act_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	act_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_APERTURE;
	act_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&act_ctrl_t->msm_sd);
	act_ctrl_t->func_tbl = &aperture_func_tbl;
	act_ctrl_t->f_number = IRIS_VALUE_F15;

	msm_aperture_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
        msm_aperture_v4l2_subdev_fops.compat_ioctl32 = msm_aperture_subdev_fops_ioctl32;
#endif
	act_ctrl_t->msm_sd.sd.devnode->fops = &msm_aperture_v4l2_subdev_fops;
	act_ctrl_t->aperture_state = ACT_DISABLE_STATE;
	aperture_ctrl = act_ctrl_t;

	CDBG("I2c probe succeeded !\n");
	return 0;

probe_failure:
	kfree(act_ctrl_t);
	return rc;
}

static int32_t msm_aperture_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	uint32_t temp = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_aperture_ctrl_t *msm_aperture_t = NULL;
	struct msm_aperture_vreg *vreg_cfg;

	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_aperture_t = kzalloc(sizeof(struct msm_aperture_ctrl_t),
		GFP_KERNEL);
	if (!msm_aperture_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_aperture_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,slave-addr",
		&temp);
	if (rc < 0) {
		pr_err("%s failed rc %d\n", __func__, rc);
		kfree(msm_aperture_t);
		return rc;
	}
	msm_aperture_t->slave_addr = temp;
	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_aperture_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_aperture_t->cci_master, rc);
	if (rc < 0 || msm_aperture_t->cci_master >= MASTER_MAX) {
		kfree(msm_aperture_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_aperture_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_aperture_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}
	rc = msm_sensor_driver_get_gpio_data(&(msm_aperture_t->gconf),
		(&pdev->dev)->of_node);
	if (rc < 0) {
		pr_err("%s: No/Error Aperture GPIOs\n", __func__);
	} else {
		msm_aperture_t->cam_pinctrl_status = 1;
		rc = msm_camera_pinctrl_init(
			&(msm_aperture_t->pinctrl_info), &(pdev->dev));
		if (rc < 0) {
			pr_err("ERR:%s: Error in reading aperture pinctrl\n",
				__func__);
			msm_aperture_t->cam_pinctrl_status = 0;
		}
	}

	msm_aperture_t->act_v4l2_subdev_ops = &msm_aperture_subdev_ops;
	msm_aperture_t->aperture_mutex = &msm_aperture_mutex;

	/* Set platform device handle */
	msm_aperture_t->pdev = pdev;
	/* Set device type as platform device */
	msm_aperture_t->act_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_aperture_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;

	msm_aperture_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_aperture_t->i2c_client.cci_client) {
		kfree(msm_aperture_t->vreg_cfg.cam_vreg);
		kfree(msm_aperture_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_aperture_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = msm_aperture_t->cci_master;
	v4l2_subdev_init(&msm_aperture_t->msm_sd.sd,
		msm_aperture_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_aperture_t->msm_sd.sd, msm_aperture_t);
	msm_aperture_t->msm_sd.sd.internal_ops = &msm_aperture_internal_ops;
	msm_aperture_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_aperture_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_aperture_t->msm_sd.sd.name), "msm_aperture");
	media_entity_init(&msm_aperture_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_aperture_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_aperture_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_APERTURE;
	msm_aperture_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_aperture_t->msm_sd);
	msm_aperture_t->aperture_state = ACT_DISABLE_STATE;
	CDBG("slave_addr 0x%x  client_addr 0x%x ",msm_aperture_t->slave_addr,cci_client->sid);
	msm_aperture_t->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	msm_aperture_t->func_tbl = &aperture_func_tbl;
	msm_aperture_t->f_number = IRIS_VALUE_F15;
	msm_aperture_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_aperture_v4l2_subdev_fops.compat_ioctl32 = msm_aperture_subdev_fops_ioctl32;
#endif
	msm_aperture_t->msm_sd.sd.devnode->fops = &msm_aperture_v4l2_subdev_fops;

	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_aperture_i2c_dt_match[] = {
	{.compatible = "qcom,aperture"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_aperture_i2c_dt_match);

static struct i2c_driver msm_aperture_i2c_driver = {
	.id_table = msm_aperture_i2c_id,
	.probe  = msm_aperture_i2c_probe,
	.remove = __exit_p(msm_aperture_i2c_remove),
	.driver = {
		.name = "qcom,aperture",
		.owner = THIS_MODULE,
		.of_match_table = msm_aperture_i2c_dt_match,
	},
};

static const struct of_device_id msm_aperture_dt_match[] = {
	{.compatible = "qcom,aperture", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_aperture_dt_match);

static struct platform_driver msm_aperture_platform_driver = {
	.probe = msm_aperture_platform_probe,
	.driver = {
		.name = "qcom,aperture",
		.owner = THIS_MODULE,
		.of_match_table = msm_aperture_dt_match,
	},
};

static int __init msm_aperture_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(&msm_aperture_platform_driver,
		msm_aperture_platform_probe);
	CDBG(" platform probe rc %d\n",rc);
	return i2c_add_driver(&msm_aperture_i2c_driver);
}

struct msm_aperture_func_tbl aperture_func_tbl = {
	.aperture_power_down = msm_aperture_power_down,
	.aperture_power_up = msm_aperture_power_up,
	.aperture_iris_init = msm_aperture_iris_init,
	.aperture_iris_set_value = msm_aperture_iris_set_value,
};

module_init(msm_aperture_init_module);
MODULE_DESCRIPTION("MSM_APERTURE");
MODULE_LICENSE("GPL v2");
