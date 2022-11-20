/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define SENSOR_DRIVER_I2C "camera"
/* Header file declaration */
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

/* Logging macro */
/*#define MSM_SENSOR_DRIVER_DEBUG*/
#undef CDBG
#ifdef MSM_SENSOR_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define SENSOR_MAX_MOUNTANGLE (360)

/* Static declaration */
static struct msm_sensor_ctrl_t *g_sctrl[MAX_CAMERAS];

bool init_setting_write = FALSE;

#if defined(CONFIG_SEC_ROSSA_PROJECT) || defined(CONFIG_SEC_J1_PROJECT)
#include "sr200pc20_yuv.h"

//#define CONFIG_LOAD_FILE

#if !defined CONFIG_LOAD_FILE
#define msm_sensor_driver_WRT_LIST(s_ctrl,A)\
    s_ctrl->sensor_i2c_client->i2c_func_tbl->\
    i2c_write_conf_tbl(\
    s_ctrl->sensor_i2c_client, A,\
    ARRAY_SIZE(A),\
    MSM_CAMERA_I2C_BYTE_DATA);
#else
#define msm_sensor_driver_WRT_LIST(s_ctrl,A)\
   sr200pc20_sensor_write_list(s_ctrl,#A)
#endif

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define TUNING_FILE_PATH "/data/sr200pc20_yuv.h"

struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;

static int sr200pc20_write_regs_from_sd(struct msm_sensor_ctrl_t *s_ctrl,char *name);
static int sr200pc20_sensor_write(struct msm_sensor_ctrl_t *s_ctrl,uint16_t addr, uint16_t data);
#endif

#ifdef CONFIG_LOAD_FILE
/**
 * sr200pc20_sensor_write: Write (I2C) multiple bytes to the camera sensor
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int sr200pc20_sensor_write(struct msm_sensor_ctrl_t *s_ctrl,uint16_t addr, uint16_t data)
{
	int rc = 0;
	//printk("[sr200pc20]addr 0x%04x, value 0x%04x   => 0x%0x  0x%0x\n",addr, data,addr, data);

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, addr, data, MSM_CAMERA_I2C_BYTE_DATA);

	return rc;
}

void sr200pc20_regs_table_init(void)
{
	struct file *fp = NULL;
	struct test *nextBuf = NULL;

	u8 *nBuf = NULL;
	size_t file_size = 0, max_size = 0, testBuf_size = 0;
	ssize_t nread = 0;
	s32 check = 0, starCheck = 0;
	s32 tmp_large_file = 0;
	s32 i = 0;
	int ret = 0;
	loff_t pos;

	/*Get the current address space */
	mm_segment_t fs = get_fs();
	CDBG("CONFIG_LOAD_FILE is enable!!\n");
	CDBG("%s %d", __func__, __LINE__);

	/*Set the current segment to kernel data segment */
	set_fs(get_ds());

	fp = filp_open(TUNING_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("failed to open %s", TUNING_FILE_PATH);
		return ;
	}

	file_size = (size_t) fp->f_path.dentry->d_inode->i_size;
	max_size = file_size;
	CDBG("file_size = %d", file_size);
	nBuf = kmalloc(file_size, GFP_ATOMIC);
	if (nBuf == NULL) {
		pr_err("Fail to 1st get memory");
		nBuf = vmalloc(file_size);
		if (nBuf == NULL) {
			pr_err("ERR: nBuf Out of Memory");
			ret = -ENOMEM;
			goto error_out;
		}
		tmp_large_file = 1;
	}

	testBuf_size = sizeof(struct test) * file_size;
	if (tmp_large_file) {
		testBuf = vmalloc(testBuf_size);
		large_file = 1;
	} else {
		testBuf = kmalloc(testBuf_size, GFP_ATOMIC);
		if (testBuf == NULL) {
			pr_err("Fail to get mem(%d bytes)", testBuf_size);
			testBuf = vmalloc(testBuf_size);
			large_file = 1;
		}
	}
	if (testBuf == NULL) {
		pr_err("ERR: Out of Memory");
		ret = -ENOMEM;
		goto error_out;
	}

	pos = 0;
	memset(nBuf, 0, file_size);
	memset(testBuf, 0, file_size * sizeof(struct test));
	nread = vfs_read(fp, (char __user *)nBuf, file_size, &pos);
	if (nread != file_size) {
		pr_err("failed to read file ret = %d", nread);
		ret = -1;
		goto error_out;
	}

	set_fs(fs);

	i = max_size;

	CDBG("i = %d", i);

	while (i) {
		testBuf[max_size - i].data = *nBuf;
		if (i != 1) {
			testBuf[max_size - i].nextBuf =
			&testBuf[max_size - i + 1];
		} else {
			testBuf[max_size - i].nextBuf = NULL;
			break;
		}
		i--;
		nBuf++;
	}
	i = max_size;
	nextBuf = &testBuf[0];

	while (i - 1) {
		if (!check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data	== '/') {
						check = 1;/* when find '//' */
						i--;
					} else if (testBuf[max_size-i].nextBuf->data == '*') {
						starCheck = 1;/*when'/ *' */
						i--;
					}
				} else
					break;
			}
			if (!check && !starCheck) {
				/* ignore '\t' */
				if (testBuf[max_size - i].data != '\t') {
					nextBuf->nextBuf = &testBuf[max_size-i];
					nextBuf = &testBuf[max_size - i];
				}
			}
		} else if (check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data == '*') {
						starCheck = 1; /*when '/ *' */
						check = 0;
						i--;
						}
					} else
						break;
			}

			/* when find '\n' */
			if (testBuf[max_size - i].data == '\n' && check) {
				check = 0;
				nextBuf->nextBuf = &testBuf[max_size - i];
				nextBuf = &testBuf[max_size - i];
			}
		} else if (!check && starCheck) {
			if (testBuf[max_size - i].data == '*') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data == '/') {
						starCheck = 0; /*when'* /' */
						i--;
					}
				} else
					break;
			}
		}

		i--;

		if (i < 2) {
			nextBuf = NULL;
			break;
		}

		if (testBuf[max_size - i].nextBuf == NULL) {
			nextBuf = NULL;
			break;
		}
	}

#if 0//FOR_DEBUG /* for print */
	CDBG("i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1) {
		if (nextBuf->nextBuf == NULL)
			break;
		//CDBG("DATA---%c\n", nextBuf->data);
		nextBuf = nextBuf->nextBuf;
	}
#endif
	tmp_large_file ? vfree(nBuf) : kfree(nBuf);

error_out:
	if (fp)
		filp_close(fp, current->files);

	return;
}

void sr200pc20_regs_table_exit(void)
{
	CDBG("%s %d", __func__, __LINE__);
	if (testBuf == NULL)
		return;
	else {
		large_file ? vfree(testBuf) : kfree(testBuf);
		large_file = 0;
		testBuf = NULL;
	}
}

static int sr200pc20_write_regs_from_sd(struct msm_sensor_ctrl_t *s_ctrl,char *name)
{
	struct test *tempData = NULL;

	u8 buf[5];
	unsigned short addr = 0;
	unsigned short data = 0;
	u16 line_count = 0;

	s32 searched = 0;
	s32 searched_addr = 0;
	s32 searched_data = 0;
	size_t size = strlen(name);
	s32 i;

	pr_err("%s : reg name = %s\n", __func__, name);

	tempData = &testBuf[0];

	*(buf + 4) = '\0';

	/* Table Name Search */
	while (!searched) {
		searched = 1;
		for (i = 0; i < size; i++) {
			if (tempData != NULL) {
				if (tempData->data != name[i]) {
					searched = 0;
					break;
				}
			} else {
				pr_err("tempData is NULL");
				return -1;
			}
			tempData = tempData->nextBuf;
		}
		tempData = tempData->nextBuf;
	}

	/* structure is get..*/
	while (1) {
		if (tempData->data == '{')
			break;
		else
			tempData = tempData->nextBuf;
	}

	while (1) {
		searched = 0;
		searched_addr = 0;
		searched_data = 0;
		line_count++;
		while (1) {
			if (tempData->data == 'x') {
				if (searched_addr == 0) {
					/* get 3 strings.*/
					buf[0] = '0';
					for (i = 1; i<4; i++) {
						buf[i] = tempData->data;
						tempData = tempData->nextBuf;
					}
					addr = (unsigned short)simple_strtoul(buf, NULL, 16);
					searched_addr = 1;
					//CDBG("addr buf %s\n", buf);
					//CDBG("strtoul data = 0x%02x\n", addr);
				} else if (searched_data == 0) {
					/* get 3 strings.*/
					buf[0] = '0';
					for (i = 1; i<4; i++) {
						buf[i] = tempData->data;
						tempData = tempData->nextBuf;
					}
					data = (unsigned short)simple_strtoul(buf, NULL, 16);
					searched_data = 1;
					//CDBG("data buf %s\n", buf);
					//CDBG("strtoul data = 0x%02x\n", data);
				}

				if ((searched_addr == 1) && (searched_data == 1))
					break;

			}else if ((tempData->data == '}') && (tempData->nextBuf->data == ';')) {
				searched = 1;
				CDBG("Complete to search last of register set '};' \n");
				break;
			} else
				tempData = tempData->nextBuf;

			if (tempData->nextBuf == NULL) {
				pr_err("ERROR : tempData->nextBuf == NULL \n");
				return -1;
			}
		}

		if (searched) {
			break;
		}

		//CDBG("I2C(W) addr = 0x%x, data = 0x%x\n", addr, data);
		if (sr200pc20_sensor_write(s_ctrl, addr, data) < 0) {
			pr_err("%s fail on sensor_write -- 1st \n",__func__);

			/* In error circumstances */
			/* Give second shot */
			if (sr200pc20_sensor_write(s_ctrl, addr, data) < 0) {
				pr_err("%s fail on sensor_write -- 2nd \n",__func__);

				/* Give it one more shot */
				if (sr200pc20_sensor_write(s_ctrl, addr, data) < 0) {
					pr_err("%s fail on sensor_write -- 3rd \n",__func__);
					return -EIO;
				}
			}
		}
	}

	CDBG("sr200pc20_write_regs_from_sd() : total line %d - X\n", line_count);
	return 0;
}

static int sr200pc20_sensor_write_list(struct msm_sensor_ctrl_t *s_ctrl,char *name)
{
	sr200pc20_write_regs_from_sd(s_ctrl, name);
	return 0;
}
#endif 

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

static struct yuv_ctrl sr200pc20_ctrl;
static exif_data_t sr200pc20_exif;

static int32_t streamon = 0;
static int32_t recording = 0;
static int32_t resolution = MSM_SENSOR_RES_FULL;

int32_t sr200pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
void sr200pc20_get_exif(struct msm_sensor_ctrl_t *s_ctrl);
int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info);
static int sr200pc20_exif_shutter_speed(struct msm_sensor_ctrl_t *s_ctrl);
static int sr200pc20_exif_iso(struct msm_sensor_ctrl_t *s_ctrl);

static struct msm_sensor_fn_t sr200pc20_sensor_func_tbl = {
	.sensor_config = sr200pc20_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr200pc20_sensor_native_control,
};
#endif


static int msm_sensor_platform_remove(struct platform_device *pdev)
{
	struct msm_sensor_ctrl_t  *s_ctrl;

	pr_err("%s: sensor FREE\n", __func__);

	s_ctrl = g_sctrl[pdev->id];
	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);
	g_sctrl[pdev->id] = NULL;

	return 0;
}


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
	.remove = msm_sensor_platform_remove,
};

static struct v4l2_subdev_info msm_sensor_driver_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static int32_t msm_sensor_driver_create_i2c_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;
	struct i2c_client *client = s_ctrl->sensor_i2c_client->client;

	CDBG("%s %s I2c probe succeeded\n", __func__, client->name);
	rc = camera_init_v4l2(&client->dev, &session_id);
	if (rc < 0) {
		pr_err("failed: camera_init_i2c_v4l2 rc %d", rc);
		return rc;
	}
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_i2c_subdev_init(&s_ctrl->msm_sd.sd, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, client);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =	s_ctrl->msm_sd.sd.name;
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_sensor_driver_create_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;

	rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
	if (rc < 0) {
		pr_err("failed: camera_init_v4l2 rc %d", rc);
		return rc;
	}
	CDBG("rc %d session_id %d", rc, session_id);
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
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	return rc;
}

static int32_t msm_sensor_fill_eeprom_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	const char *eeprom_name;
	struct device_node *src_node = NULL;
	uint32_t val = 0, count = 0, eeprom_name_len;
	int i;
	int32_t *eeprom_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;
	const void *p;

	if (!s_ctrl->sensordata->eeprom_name || !of_node)
		return -EINVAL;

	eeprom_name_len = strlen(s_ctrl->sensordata->eeprom_name);
	if (eeprom_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	eeprom_subdev_id = &sensor_info->subdev_id[SUB_MODULE_EEPROM];
	/*
	 * string for eeprom name is valid, set sudev id to -1
	 *  and try to found new id
	 */
	*eeprom_subdev_id = -1;

	if (0 == eeprom_name_len)
		return 0;

	CDBG("Try to find eeprom subdev for %s\n",
			s_ctrl->sensordata->eeprom_name);
	p = of_get_property(of_node, "qcom,eeprom-src", &count);
	if (!p || !count)
		return 0;

	count /= sizeof(uint32_t);
	for (i = 0; i < count; i++) {
		eeprom_name = NULL;
		src_node = of_parse_phandle(of_node, "qcom,eeprom-src", i);
		if (!src_node) {
			pr_err("eeprom src node NULL\n");
			continue;
		}
		rc = of_property_read_string(src_node, "qcom,eeprom-name",
			&eeprom_name);
		if (rc < 0) {
			pr_err("failed\n");
			of_node_put(src_node);
			continue;
		}
		if (strcmp(eeprom_name, s_ctrl->sensordata->eeprom_name))
			continue;

		rc = of_property_read_u32(src_node, "cell-index", &val);

		CDBG("%s qcom,eeprom cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("failed\n");
			of_node_put(src_node);
			continue;
		}

		*eeprom_subdev_id = val;
		CDBG("Done. Eeprom subdevice id is %d\n", val);
		of_node_put(src_node);
		src_node = NULL;
		break;
	}

	return rc;
}

static int32_t msm_sensor_fill_actuator_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0, actuator_name_len;
	int32_t *actuator_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!s_ctrl->sensordata->actuator_name || !of_node)
		return -EINVAL;

	actuator_name_len = strlen(s_ctrl->sensordata->actuator_name);
	if (actuator_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	actuator_subdev_id = &sensor_info->subdev_id[SUB_MODULE_ACTUATOR];
	/*
	 * string for actuator name is valid, set sudev id to -1
	 * and try to found new id
	 */
	*actuator_subdev_id = -1;

	if (0 == actuator_name_len)
		return 0;

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return -EINVAL;
		}
		*actuator_subdev_id = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	return rc;
}

static int32_t msm_sensor_fill_slave_info_init_params(
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_sensor_info_t *sensor_info)
{
	struct msm_sensor_init_params *sensor_init_params;
	if (!slave_info ||  !sensor_info)
		return -EINVAL;

	if (!slave_info->is_init_params_valid)
		return 0;

	sensor_init_params = &slave_info->sensor_init_params;
	if (INVALID_CAMERA_B != sensor_init_params->position)
		sensor_info->position =
			sensor_init_params->position;

	if (SENSOR_MAX_MOUNTANGLE > sensor_init_params->sensor_mount_angle) {
		sensor_info->sensor_mount_angle =
			sensor_init_params->sensor_mount_angle;
		sensor_info->is_mount_angle_valid = 1;
	}

	if (CAMERA_MODE_INVALID != sensor_init_params->modes_supported)
		sensor_info->modes_supported =
			sensor_init_params->modes_supported;

	return 0;
}


static int32_t msm_sensor_validate_slave_info(
	struct msm_sensor_info_t *sensor_info)
{
	if (INVALID_CAMERA_B == sensor_info->position) {
		sensor_info->position = BACK_CAMERA_B;
		CDBG("%s:%d Set default sensor position\n",
			__func__, __LINE__);
	}
	if (CAMERA_MODE_INVALID == sensor_info->modes_supported) {
		sensor_info->modes_supported = CAMERA_MODE_2D_B;
		CDBG("%s:%d Set default sensor modes_supported\n",
			__func__, __LINE__);
	}
	if (SENSOR_MAX_MOUNTANGLE <= sensor_info->sensor_mount_angle) {
		sensor_info->sensor_mount_angle = 0;
		CDBG("%s:%d Set default sensor mount angle\n",
			__func__, __LINE__);
		sensor_info->is_mount_angle_valid = 1;
	}
	return 0;
}

/* static function definition */
int32_t msm_sensor_driver_probe(void *setting)
{
	int32_t                              rc = 0;
	uint16_t                             i = 0, size = 0, size_down = 0;
	struct msm_sensor_ctrl_t            *s_ctrl = NULL;
	struct msm_camera_cci_client        *cci_client = NULL;
	struct msm_camera_sensor_slave_info *slave_info = NULL;
	struct msm_sensor_power_setting     *power_setting = NULL;
	struct msm_sensor_power_setting     *power_down_setting = NULL;
	struct msm_camera_slave_info        *camera_info = NULL;
	struct msm_camera_power_ctrl_t      *power_info = NULL;
	int c, end;
	struct msm_sensor_power_setting     power_down_setting_t;
	unsigned long mount_pos = 0;
	int probe_fail = 0;

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
	CDBG("camera id %d", slave_info->camera_id);
	CDBG("slave_addr 0x%x", slave_info->slave_addr);
	CDBG("addr_type %d", slave_info->addr_type);
	CDBG("sensor_id_reg_addr 0x%x",
		slave_info->sensor_id_info.sensor_id_reg_addr);
	CDBG("sensor_id 0x%x", slave_info->sensor_id_info.sensor_id);
	CDBG("size %d", slave_info->power_setting_array.size);
	CDBG("size down %d", slave_info->power_setting_array.size_down);
	CDBG("sensor_name %s", slave_info->sensor_name);

	if (slave_info->is_init_params_valid) {
		CDBG("position %d",
			slave_info->sensor_init_params.position);
		CDBG("mount %d",
			slave_info->sensor_init_params.sensor_mount_angle);
	}

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

#if defined(CONFIG_SEC_ROSSA_PROJECT) || defined(CONFIG_SEC_J1_PROJECT)
	if(slave_info->camera_id == CAMERA_2){
		s_ctrl->func_tbl = &sr200pc20_sensor_func_tbl ;
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
		rc = 0;
		goto FREE_SLAVE_INFO;
	}

	size = slave_info->power_setting_array.size;
	/* Allocate memory for power up setting */
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
		CDBG("UP seq_type %d seq_val %d config_val %ld delay %d",
			power_setting[i].seq_type, power_setting[i].seq_val,
			power_setting[i].config_val, power_setting[i].delay);
	}
	/*DOWN*/
	size_down = slave_info->power_setting_array.size_down;
	if (!size_down)
		size_down = size;
	/* Allocate memory for power down setting */
	power_down_setting =
		kzalloc(sizeof(*power_setting) * size_down, GFP_KERNEL);
	if (!power_down_setting) {
		pr_err("failed: no memory power_setting %p",
						power_down_setting);
		rc = -ENOMEM;
		goto FREE_POWER_SETTING;
	}

	if (slave_info->power_setting_array.power_down_setting) {
		if (copy_from_user(power_down_setting,
			(void *)slave_info->power_setting_array.
						power_down_setting,
			sizeof(*power_down_setting) * size_down)) {
			pr_err("failed: copy_from_user");
			rc = -EFAULT;
			goto FREE_POWER_DOWN_SETTING;
		}
	} else {
		pr_err("failed: no power_down_setting");
		if (copy_from_user(power_down_setting,
			(void *)slave_info->power_setting_array.
						power_setting,
			sizeof(*power_down_setting) * size_down)) {
			pr_err("failed: copy_from_user");
			rc = -EFAULT;
			goto FREE_POWER_DOWN_SETTING;
		}

		/*reverce*/
		end = size_down - 1;
		for (c = 0; c < size_down/2; c++) {
			power_down_setting_t = power_down_setting[c];
			power_down_setting[c] = power_down_setting[end];
			power_down_setting[end] = power_down_setting_t;
			end--;
		}

	}

	/* Print power setting */
	for (i = 0; i < size_down; i++) {
		CDBG("DOWN seq_type %d seq_val %d config_val %ld delay %d",
			power_down_setting[i].seq_type,
			power_down_setting[i].seq_val,
			power_down_setting[i].config_val,
			power_down_setting[i].delay);
	}

	camera_info = kzalloc(sizeof(struct msm_camera_slave_info), GFP_KERNEL);
	if (!camera_info) {
		pr_err("failed: no memory slave_info %p", camera_info);
		goto FREE_POWER_DOWN_SETTING;

	}

	/* Fill power up setting and power up setting size */
	power_info = &s_ctrl->sensordata->power_info;
	power_info->power_setting = power_setting;
	power_info->power_setting_size = size;
	power_info->power_down_setting = power_down_setting;
	power_info->power_down_setting_size = size_down;

	s_ctrl->sensordata->slave_info = camera_info;

	/* Fill sensor slave info */
	camera_info->sensor_slave_addr = slave_info->slave_addr;
	camera_info->sensor_id_reg_addr =
		slave_info->sensor_id_info.sensor_id_reg_addr;
	camera_info->sensor_id = slave_info->sensor_id_info.sensor_id;

	/* Fill CCI master, slave address and CCI default params */
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: sensor_i2c_client %p",
			s_ctrl->sensor_i2c_client);
		rc = -EINVAL;
		goto FREE_CAMERA_INFO;
	}
	/* Fill sensor address type */
	s_ctrl->sensor_i2c_client->addr_type = slave_info->addr_type;
	if (s_ctrl->sensor_i2c_client->client)
		s_ctrl->sensor_i2c_client->client->addr =
			camera_info->sensor_slave_addr;

	cci_client = s_ctrl->sensor_i2c_client->cci_client;
	if (!cci_client) {
		pr_err("failed: cci_client %p", cci_client);
		goto FREE_CAMERA_INFO;
	}
	cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
	cci_client->sid = slave_info->slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = slave_info->i2c_freq_mode;

	/* Parse and fill vreg params for powerup settings */
	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg,
		power_info->num_vreg,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_power_setting_data rc %d",
			rc);
		goto FREE_CAMERA_INFO;
	}

	/* Parse and fill vreg params for powerdown settings*/
	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg,
		power_info->num_vreg,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_fill_vreg_params for PDOWN rc %d",
			rc);
		goto FREE_CAMERA_INFO;
	}

	/* Update sensor, actuator and eeprom name in
	*  sensor control structure */
	s_ctrl->sensordata->sensor_name = slave_info->sensor_name;
	s_ctrl->sensordata->eeprom_name = slave_info->eeprom_name;
	s_ctrl->sensordata->actuator_name = slave_info->actuator_name;

	/*
	 * Update eeporm subdevice Id by input eeprom name
	 */
	rc = msm_sensor_fill_eeprom_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_CAMERA_INFO;
	}
	/*
	 * Update actuator subdevice Id by input actuator name
	 */
	rc = msm_sensor_fill_actuator_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_CAMERA_INFO;
	}

	/* Power up and probe sensor */
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s power up failed", slave_info->sensor_name);
		probe_fail = rc;
//		goto FREE_CAMERA_INFO;
	}

	pr_err("%s probe succeeded", slave_info->sensor_name);

	/*
	  Set probe succeeded flag to 1 so that no other camera shall
	 * probed on this slot
	 */
	s_ctrl->is_probe_succeed = 1;

	/*
	 * Create /dev/videoX node, comment for now until dummy /dev/videoX
	 * node is created and used by HAL
	 */

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		rc = msm_sensor_driver_create_v4l_subdev(s_ctrl);
	else
		rc = msm_sensor_driver_create_i2c_v4l_subdev(s_ctrl);
	if (rc < 0) {
		pr_err("failed: camera creat v4l2 rc %d", rc);
		goto CAMERA_POWER_DOWN;
	}
	memcpy(slave_info->subdev_name, s_ctrl->msm_sd.sd.entity.name,
		sizeof(slave_info->subdev_name));
	slave_info->is_probe_succeed = 1;
	slave_info->sensor_info.session_id = s_ctrl->sensordata->sensor_info->session_id;
	for (i = 0; i < SUB_MODULE_MAX; i++) {
		slave_info->sensor_info.subdev_id[i] =
			s_ctrl->sensordata->sensor_info->subdev_id[i];
		pr_err("sensor_subdev_id = %d i = %d\n",slave_info->sensor_info.subdev_id[i], i); 
	}
	if (copy_to_user((void __user *)setting,
		(void *)slave_info, sizeof(*slave_info))) {
		pr_err("%s:%d copy failed\n", __func__, __LINE__);
		rc = -EFAULT;
	}

	/* Power down */
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);

	rc = msm_sensor_fill_slave_info_init_params(
		slave_info,
		s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Fill slave info failed", slave_info->sensor_name);
		goto FREE_CAMERA_INFO;
	}
	rc = msm_sensor_validate_slave_info(s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Validate slave info failed",
			slave_info->sensor_name);
		goto FREE_CAMERA_INFO;
	}
	/* Update sensor mount angle and position in media entity flag */
	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
		sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	/*Save sensor info*/
	s_ctrl->sensordata->cam_slave_info = slave_info;

	if(probe_fail) {
		rc = probe_fail;
	}

	return rc;

CAMERA_POWER_DOWN:
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
FREE_CAMERA_INFO:
	kfree(camera_info);
FREE_POWER_DOWN_SETTING:
	kfree(power_down_setting);
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

static int32_t msm_sensor_driver_get_dt_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                              rc = 0;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct device_node                  *of_node = s_ctrl->of_node;
	uint32_t cell_id;

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
	rc = of_property_read_u32(of_node, "cell-index", &cell_id);
	if (rc < 0) {
		pr_err("failed: cell-index rc %d", rc);
		goto FREE_SENSOR_DATA;
	}
	s_ctrl->id = cell_id;

	/* Validate cell_id */
	if (cell_id >= MAX_CAMERAS) {
		pr_err("failed: invalid cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Check whether g_sctrl is already filled for this cell_id */
	if (g_sctrl[cell_id]) {
		pr_err("failed: sctrl already filled for cell_id %d", cell_id);
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
	if (0 > of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_info->sensor_mount_angle)) {
		/* Invalidate mount angle flag */
		sensordata->sensor_info->is_mount_angle_valid = 0;
		sensordata->sensor_info->sensor_mount_angle = 0;
	} else {
		sensordata->sensor_info->is_mount_angle_valid = 1;
	}
	CDBG("%s qcom,mount-angle %d\n", __func__,
		sensordata->sensor_info->sensor_mount_angle);
	if (0 > of_property_read_u32(of_node, "qcom,sensor-position",
		&sensordata->sensor_info->position)) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->sensor_info->position = INVALID_CAMERA_B;
	}
	if (0 > of_property_read_u32(of_node, "qcom,sensor-mode",
		&sensordata->sensor_info->modes_supported)) {
		CDBG("%s:%d Invalid sensor mode supported\n",
			__func__, __LINE__);
		sensordata->sensor_info->modes_supported = CAMERA_MODE_INVALID;
	}
	/* Get vdd-cx regulator */
	/*Optional property, don't return error if absent */
	of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("qcom,misc_regulator %s", sensordata->misc_regulator);

	s_ctrl->set_mclk_23880000 = of_property_read_bool(of_node,
						"qcom,mclk-23880000");

	CDBG("%s qcom,mclk-23880000 = %d\n", __func__,
		s_ctrl->set_mclk_23880000);

	return rc;

FREE_VREG_DATA:
	kfree(sensordata->power_info.cam_vreg);
FREE_SUB_MODULE_DATA:
	kfree(sensordata->sensor_info);
FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

static int32_t msm_sensor_driver_parse(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                   rc = 0;

	CDBG("%s Enter", __func__);
	/* Validate input parameters */


	/* Allocate memory for sensor_i2c_client */
	s_ctrl->sensor_i2c_client = kzalloc(sizeof(*s_ctrl->sensor_i2c_client),
		GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: no memory sensor_i2c_client %p",
			s_ctrl->sensor_i2c_client);
		return -ENOMEM;
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
	rc = msm_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		pr_err("failed: rc %d", rc);
		goto FREE_MUTEX;
	}

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
	g_sctrl[s_ctrl->id] = s_ctrl;
	pr_err("g_sctrl[%d] %p", s_ctrl->id, g_sctrl[s_ctrl->id]);

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
	return rc;
}

static int32_t msm_sensor_driver_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = NULL;


	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl) {
		pr_err("failed: no memory s_ctrl %p", s_ctrl);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	s_ctrl->of_node = pdev->dev.of_node;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	/* Fill platform device */
	pdev->id = s_ctrl->id;
	s_ctrl->pdev = pdev;

	/* Fill device in power info */
	s_ctrl->sensordata->power_info.dev = &pdev->dev;

	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int32_t msm_sensor_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("\n\nEnter: msm_sensor_driver_i2c_probe");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl) {
		pr_err("failed: no memory s_ctrl %p", s_ctrl);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_I2C_DEVICE;
	s_ctrl->of_node = client->dev.of_node;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		s_ctrl->sensordata->power_info.dev = &client->dev;

	}

	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int msm_sensor_driver_i2c_remove(struct i2c_client *client)
{
	struct msm_sensor_ctrl_t  *s_ctrl = i2c_get_clientdata(client);

	pr_err("%s: sensor FREE\n", __func__);

	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	g_sctrl[s_ctrl->id] = NULL;
	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);

	return 0;
}

static const struct i2c_device_id i2c_id[] = {
	{SENSOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver msm_sensor_driver_i2c = {
	.id_table = i2c_id,
	.probe  = msm_sensor_driver_i2c_probe,
	.remove = msm_sensor_driver_i2c_remove,
	.driver = {
		.name = SENSOR_DRIVER_I2C,
	},
};

static int __init msm_sensor_driver_init(void)
{
	int32_t rc = 0;

	CDBG("Enter");
	rc = platform_driver_probe(&msm_sensor_platform_driver,
		msm_sensor_driver_platform_probe);
	if (!rc) {
		CDBG("probe success");
		return rc;
	} else {
		CDBG("probe i2c");
		rc = i2c_add_driver(&msm_sensor_driver_i2c);
	}

	return rc;
}


static void __exit msm_sensor_driver_exit(void)
{
	CDBG("Enter");
	platform_driver_unregister(&msm_sensor_platform_driver);
	i2c_del_driver(&msm_sensor_driver_i2c);
	return;
}

#if defined(CONFIG_SEC_ROSSA_PROJECT) || defined(CONFIG_SEC_J1_PROJECT)
int32_t sr200pc20_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING -- EV is %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M4);
		break;
	case CAMERA_EV_M3:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M3);
		break;
	case CAMERA_EV_M2:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M2);
		break;
	case CAMERA_EV_M1:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M1);
		break;
	case CAMERA_EV_DEFAULT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_default);
		break;
	case CAMERA_EV_P1:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P1);
		break;
	case CAMERA_EV_P2:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P2);
		break;
	case CAMERA_EV_P3:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P3);
		break;
	case CAMERA_EV_P4:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P4);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr200pc20_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING -- WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_OFF:
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Auto);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Incandescent);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Fluorescent);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Daylight);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Cloudy);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr200pc20_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING-- resolution is %d", mode);
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Capture);
		//to get exif data
		sr200pc20_get_exif(s_ctrl);
		break;
	default:
		if (init_setting_write) {
			rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_640x480_Preview_for_initial);
			init_setting_write = FALSE;
		} else {
			rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_640x480_Preview_for_Return);
		}
		//rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_800x600_Preview);
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	rc=0;
	}
	return rc;
}

int32_t sr200pc20_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING-- effect is %d", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Normal);
		break;
	case CAMERA_EFFECT_MONO:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Gray);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Negative);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Sepia);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}

int32_t sr200pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);

	pr_err("%s ENTER %d \n ",__func__,cdata->cfgtype);

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


		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		cdata->cfg.sensor_info.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_info.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;

		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		pr_err("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
		case CFG_SET_INIT_SETTING:
			CDBG("CFG_SET_INIT_SETTING writing INIT registers: sr200pc20_Init_Reg \n");
			init_setting_write = TRUE;
			rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Init_Reg);
			break;
		case CFG_SET_RESOLUTION:
			resolution = *((int32_t  *)cdata->cfg.setting);
			CDBG("CFG_SET_RESOLUTION *** res = %d\n " , resolution);
			if( sr200pc20_ctrl.op_mode == CAMERA_MODE_RECORDING ){
					 CDBG("writing *** sr200pc20_24fps_Camcoder\n");
				rc = msm_sensor_driver_WRT_LIST(s_ctrl, sr200pc20_24fps_Camcoder);
				recording = 1;
			}else{
			if(recording == 1){
				CDBG("CFG_SET_RESOLUTION recording START recording =1 *** res = %d\n " , resolution);
				rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Auto_fps);  
				sr200pc20_set_effect( s_ctrl , sr200pc20_ctrl.settings.effect);
				sr200pc20_set_white_balance( s_ctrl, sr200pc20_ctrl.settings.wb);
				sr200pc20_set_exposure_compensation( s_ctrl , sr200pc20_ctrl.settings.exposure);
				recording = 0;
			}else{
				sr200pc20_set_resolution(s_ctrl , resolution );
				CDBG("CFG_SET_RESOLUTION END *** res = %d\n " , resolution);
			}
			}
			break;

	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr200pc20_stop_stream \n");
		if(streamon == 1){
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr200pc20_stop_stream,
					ARRAY_SIZE(sr200pc20_stop_stream),
					MSM_CAMERA_I2C_BYTE_DATA);
				rc=0;
				streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr200pc20_start_stream start   \n");
		if( sr200pc20_ctrl.op_mode != CAMERA_MODE_CAPTURE && sr200pc20_ctrl.op_mode != CAMERA_MODE_PREVIEW){
			sr200pc20_set_effect( s_ctrl , sr200pc20_ctrl.settings.effect);
			sr200pc20_set_white_balance( s_ctrl, sr200pc20_ctrl.settings.wb);
			sr200pc20_set_exposure_compensation( s_ctrl , sr200pc20_ctrl.settings.exposure);
		}
		streamon = 1;
		CDBG("CFG_SET_START_STREAM : sr200pc20_start_stream rc = %d \n", rc);
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
	#ifdef CONFIG_LOAD_FILE
		sr200pc20_regs_table_init();
       #endif
		streamon = 0;
		sr200pc20_ctrl.op_mode = CAMERA_MODE_INIT;
		sr200pc20_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr200pc20_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
		sr200pc20_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
		sr200pc20_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
		sr200pc20_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
		sr200pc20_ctrl.settings.effect = CAMERA_EFFECT_OFF;
		sr200pc20_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
		if (s_ctrl->func_tbl->sensor_power_up) {
                        CDBG("CFG_POWER_UP");
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
                } else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		CDBG("CFG_POWER_DOWN  \n");
	#ifdef CONFIG_LOAD_FILE
		sr200pc20_regs_table_exit();
      #endif
		 if (s_ctrl->func_tbl->sensor_power_down) {
                        CDBG("CFG_POWER_DOWN");
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
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
		rc = 0;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("EXIT \n ");

	return 0;
}

int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("ENTER \n ");
	pr_err("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr200pc20_ctrl.settings.exposure = cam_info->value_1;
		sr200pc20_set_exposure_compensation(s_ctrl, sr200pc20_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr200pc20_ctrl.settings.wb = cam_info->value_1;
		sr200pc20_set_white_balance(s_ctrl, sr200pc20_ctrl.settings.wb);
		break;
	case EXT_CAM_EFFECT:
		sr200pc20_ctrl.settings.effect = cam_info->value_1;
		sr200pc20_set_effect(s_ctrl, sr200pc20_ctrl.settings.effect);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr200pc20_ctrl.op_mode = cam_info->value_1;
		pr_err("EXT_CAM_SENSOR_MODE = %d", sr200pc20_ctrl.op_mode);
		break;
	case EXT_CAM_EXIF:
		sr200pc20_get_exif_info(cam_info);
		if (!copy_to_user((void *)argp,
			(const void *)&cam_info,
			sizeof(cam_info)))
		pr_err("copy failed");
		break;
	default:
		rc = 0;
		break;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	CDBG("EXIT \n ");
	return 0;
}

void sr200pc20_get_exif(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("sr200pc20_get_exif: E");

	/*Exif data*/
	sr200pc20_exif_shutter_speed(s_ctrl);
	sr200pc20_exif_iso(s_ctrl);
	CDBG("exp_time : %d\niso_value : %d\n",sr200pc20_exif.shutterspeed, sr200pc20_exif.iso);
	return;
}

int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info)
{
	exif_info->value_1 = 1;	// equals 1 to update the exif value in the user level.
	exif_info->value_2 = sr200pc20_exif.iso;
	exif_info->value_3 = sr200pc20_exif.shutterspeed;
	return 0;
}

static int sr200pc20_exif_shutter_speed(struct msm_sensor_ctrl_t *s_ctrl)
{
	u16 read_value1 = 0;
	u16 read_value2 = 0;
	u16 read_value3 = 0;
	int OPCLK = 26000000;

	/* Exposure Time */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(s_ctrl->sensor_i2c_client, 0x03,0x20,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x80,&read_value1,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x81,&read_value2,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x82,&read_value3,MSM_CAMERA_I2C_BYTE_DATA);

	sr200pc20_exif.shutterspeed = OPCLK / ((read_value1 << 19)
			+ (read_value2 << 11) + (read_value3 << 3));
	CDBG("Exposure time = %d\n", sr200pc20_exif.shutterspeed);

	return 0;
}

static int sr200pc20_exif_iso(struct msm_sensor_ctrl_t *s_ctrl)
{
	u16 read_value4 = 0;
	unsigned short gain_value;

	/* ISO*/
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(s_ctrl->sensor_i2c_client, 0x03,0x20,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0xb0,&read_value4,MSM_CAMERA_I2C_BYTE_DATA);

	gain_value = (read_value4 / 32) * 1000 +500;
	if (gain_value < 1140)
		sr200pc20_exif.iso = 50;
	else if (gain_value < 2140)
		sr200pc20_exif.iso = 100;
	else if (gain_value < 2640)
		sr200pc20_exif.iso = 200;
	else if (gain_value < 7520)
		sr200pc20_exif.iso = 400;
	else
		sr200pc20_exif.iso = 800;

	CDBG("ISO = %d\n", sr200pc20_exif.iso);

	return 0;
}

#endif


module_init(msm_sensor_driver_init);
module_exit(msm_sensor_driver_exit);
MODULE_DESCRIPTION("msm_sensor_driver");
MODULE_LICENSE("GPL v2");
