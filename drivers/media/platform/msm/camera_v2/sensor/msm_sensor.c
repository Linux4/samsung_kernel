/* Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
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
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>
#include <media/adsp-shmem-device.h>

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29start*/
#include <kernel_project_defines.h>
#ifdef CONFIG_MSM_CAMERA_HS60_ADDBOARD
#include <soc/qcom/smem.h>
u32 sku_version_hq = 0;
#endif
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29 end*/
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 start*/
#ifdef HUAQIN_KERNEL_PROJECT_HS50
#include <soc/qcom/smem.h>
u32 hs50_board_id = 0;
#endif
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 end*/
/* HS60 code for HS60-263 by xuxianwei at 20190729 start */
uint16_t gc5035_module_flag = 0;
uint16_t gc5035_module_id = 0;
uint16_t gc5035_supply_id = 0;
uint16_t gc02m1_supply_id = 0;
/* HS60 code for HS60-263 by xuxianwei at 20190729 end */
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 start*/
#include <linux/gpio.h>
#define  SENSOR_SUB_GPIO_ID          41//add gpio_id
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 end*/
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 start*/
#define  SENSOR_SUB_GPIO_ID_HS70          1//add gpio_id
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 end*/
/*HS70 code for HS70 xxx by gaozhenyu at 2020/08/22 start*/
#define  SENSOR_SUB_GPIO_ID_HS50          1//add camera_id gpio
/*HS70 code for HS70 xxx by gaozhenyu at 2020/08/22 end*/
static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl;
static struct msm_camera_i2c_fn_t msm_sensor_secure_func_tbl;
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29start*/
#ifdef CONFIG_MSM_CAMERA_HS60_ADDBOARD
static int swtp_sku_conf(void)
{
	u32 *sku_addr = NULL;
	u32 sku_size = 0;

	sku_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &sku_size, 0, 0);
	if (sku_addr)
	{
		sku_version_hq = (*sku_addr)&0xf;
		pr_err("%s sku_size=%d, sku_version=0x%x\n",__FUNCTION__, sku_size, sku_version_hq);
		return 0;
	}
	else
	{
		pr_err("%s reading the smem conf fail\n", __FUNCTION__);
		return 1;
	}
}
#endif
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29end*/
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 start*/
#ifdef HUAQIN_KERNEL_PROJECT_HS50
static u32 read_hs50_id(void)
{
	u32 *hs50_id_addr = NULL;
	u32 hs50_id_size = 0;
	u32 id=0;

	hs50_id_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &hs50_id_size, 0, 0);
	if (hs50_id_addr)
	{
		id = (*hs50_id_addr);
		if(id==0)
		{
			pr_err("%s reading board_id from smem conf fail,id error\n", __FUNCTION__);
			return 0;
		}
		else
		{
			pr_err("%s size=%d, board_id=0x%x, id&0xf0=0x%x\n",__FUNCTION__, hs50_id_size, id, id&0xf0);
			return id&0xf0;
		}
	}
	else
	{
		pr_err("%s reading board_id from smem conf fail\n", __FUNCTION__);
		return 0;
	}

}
#endif
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 end*/
static void msm_sensor_adjust_mclk(struct msm_camera_power_ctrl_t *ctrl)
{
	int idx;
	struct msm_sensor_power_setting *power_setting;

	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == SENSOR_CLK &&
			power_setting->seq_val ==  SENSOR_CAM_MCLK) {
			if (power_setting->config_val == 24000000) {
				power_setting->config_val = 23880000;
				CDBG("%s MCLK request adjusted to 23.88MHz\n"
							, __func__);
			}
			break;
		}
	}

}

static void msm_sensor_misc_regulator(
	struct msm_sensor_ctrl_t *sctrl, uint32_t enable)
{
	int32_t rc = 0;

	if (enable) {
		sctrl->misc_regulator = (void *)rpm_regulator_get(
			&sctrl->pdev->dev, sctrl->sensordata->misc_regulator);
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(sctrl->misc_regulator,
				RPM_REGULATOR_MODE_HPM);
			if (rc < 0) {
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
				rpm_regulator_put(sctrl->misc_regulator);
			}
		} else {
			pr_err("%s: Failed to vote for rpm regulator on %s: %d\n",
				__func__,
				sctrl->sensordata->misc_regulator, rc);
		}
	} else {
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(
				(struct rpm_regulator *)sctrl->misc_regulator,
				RPM_REGULATOR_MODE_AUTO);
			if (rc < 0)
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
			rpm_regulator_put(sctrl->misc_regulator);
		}
	}
}

int32_t msm_sensor_free_sensor_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_slave_info *slave_info = NULL;

	if (!s_ctrl->pdev && !s_ctrl->sensor_i2c_client->client)
		return 0;
	kfree(s_ctrl->sensordata->slave_info);
	slave_info = s_ctrl->sensordata->cam_slave_info;
	kfree(slave_info->sensor_id_info.setting.reg_setting);
	kfree(s_ctrl->sensordata->cam_slave_info);
	kfree(s_ctrl->sensordata->actuator_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata->power_info.power_setting);
	kfree(s_ctrl->sensordata->power_info.power_down_setting);
	kfree(s_ctrl->sensordata->csi_lane_params);
	kfree(s_ctrl->sensordata->sensor_info);
	if (s_ctrl->sensor_device_type == MSM_CAMERA_I2C_DEVICE) {
		msm_camera_i2c_dev_put_clk_info(
			&s_ctrl->sensor_i2c_client->client->dev,
			&s_ctrl->sensordata->power_info.clk_info,
			&s_ctrl->sensordata->power_info.clk_ptr,
			s_ctrl->sensordata->power_info.clk_info_size);
	} else {
		msm_camera_put_clk_info(s_ctrl->pdev,
			&s_ctrl->sensordata->power_info.clk_info,
			&s_ctrl->sensordata->power_info.clk_ptr,
			s_ctrl->sensordata->power_info.clk_info_size);
	}

	kfree(s_ctrl->sensordata);
	return 0;
}

int msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_power_ctrl_t *power_info;
	enum msm_camera_device_type_t sensor_device_type;
	struct msm_camera_i2c_client *sensor_i2c_client;

	if (!s_ctrl) {
		pr_err("%s:%d failed: s_ctrl %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	if (s_ctrl->is_csid_tg_mode)
		return 0;

	power_info = &s_ctrl->sensordata->power_info;
	sensor_device_type = s_ctrl->sensor_device_type;
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	if (!power_info || !sensor_i2c_client) {
		pr_err("%s:%d failed: power_info %pK sensor_i2c_client %pK\n",
			__func__, __LINE__, power_info, sensor_i2c_client);
		return -EINVAL;
	}

    /*HS70 code for gc02m1 power leak by zhangpeng at 2020/03/24 start*/
    if ((strcmp(s_ctrl->sensordata->sensor_name, "gc02m1") == 0) ||
      (strcmp(s_ctrl->sensordata->sensor_name, "gc02m1_hs70_xl") == 0) ||
      (strcmp(s_ctrl->sensordata->sensor_name, "gc02m1_hs70_jk") == 0) ||
	  (strcmp(s_ctrl->sensordata->sensor_name, "gc02m1_hs70_mcn") == 0)) {
        pr_err("%s gc02m1 power down write settings for power leak start", __func__);
        sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf9,0x82, MSM_CAMERA_I2C_BYTE_DATA);
        sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf7,0x01, MSM_CAMERA_I2C_BYTE_DATA);
        sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8e, MSM_CAMERA_I2C_BYTE_DATA);
        sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf7,0x00, MSM_CAMERA_I2C_BYTE_DATA);
        sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf9,0x83, MSM_CAMERA_I2C_BYTE_DATA);
        pr_err("%s gc02m1 power down write settings for power leak end", __func__);
    }
    /*HS70 code for gc02m1 power leak by zhangpeng at 2020/03/24 end*/

	/* Power down secure session if it exist*/
	if (s_ctrl->is_secure)
		msm_camera_tz_i2c_power_down(sensor_i2c_client);

	return msm_camera_power_down(power_info, sensor_device_type,
		sensor_i2c_client);
}

int msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct msm_camera_power_ctrl_t *power_info;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;
	uint32_t retry = 0;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}

	if (s_ctrl->is_csid_tg_mode)
		return 0;

	power_info = &s_ctrl->sensordata->power_info;
	sensor_i2c_client = s_ctrl->sensor_i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;

	if (!power_info || !sensor_i2c_client || !slave_info ||
		!sensor_name) {
		pr_err("%s:%d failed: %pK %pK %pK %pK\n",
			__func__, __LINE__, power_info,
			sensor_i2c_client, slave_info, sensor_name);
		return -EINVAL;
	}

	if (s_ctrl->set_mclk_23880000)
		msm_sensor_adjust_mclk(power_info);

	CDBG("Sensor %d tagged as %s\n", s_ctrl->id,
		(s_ctrl->is_secure)?"SECURE":"NON-SECURE");

	for (retry = 0; retry < 3; retry++) {
		if (s_ctrl->is_secure) {
			rc = msm_camera_tz_i2c_power_up(sensor_i2c_client);
			if (rc < 0) {
#ifdef CONFIG_MSM_SEC_CCI_DEBUG
				CDBG("Secure Sensor %d use cci\n", s_ctrl->id);
				/* session is not secure */
				s_ctrl->sensor_i2c_client->i2c_func_tbl =
					&msm_sensor_cci_func_tbl;
#else  /* CONFIG_MSM_SEC_CCI_DEBUG */
				return rc;
#endif /* CONFIG_MSM_SEC_CCI_DEBUG */
			} else {
				/* session is secure */
				s_ctrl->sensor_i2c_client->i2c_func_tbl =
					&msm_sensor_secure_func_tbl;
			}
		}
#if IS_ENABLED(CONFIG_ARCH_QM215)
		msleep(60);
#endif
          	/*HS60 code for HS60-1598 by zhangpeng at 2019/09/16 start*/
          	/*HS70 code for HS70-57 by zhangpeng at 2019/09/30 start*/
		/*HS70 code for HS70-55 by chengzhi at 2019/09/30 start*/
		if((0 == strcmp(sensor_name, "gc2375h")) ||
		    (0 == strcmp(sensor_name, "gc2375h_kg")) ||
		    (0 == strcmp(sensor_name, "gc2375h_cxt")) ||
		    (0 == strcmp(sensor_name, "gc2375h_hs70_jk"))) {
		/*HS70 code for HS70-55 by chengzhi at 2019/09/30 end*/
		    /*HS70 code for HS70-57 by zhangpeng at 2019/09/30 end*/
		    rc = msm_gc2375h_power_up(power_info, s_ctrl->sensor_device_type,
			    sensor_i2c_client);
		}
		else {
		    rc = msm_camera_power_up(power_info, s_ctrl->sensor_device_type,
			    sensor_i2c_client);
		}
		if (rc < 0)
			return rc;
		rc = msm_sensor_check_id(s_ctrl);
		if (rc < 0) {
			msm_camera_power_down(power_info,
				s_ctrl->sensor_device_type, sensor_i2c_client);
			msleep(20);
			continue;
		} else {
			break;
		}
          	/*HS60 code for HS60-1598 by zhangpeng at 2019/09/16 end*/
	}

	return rc;
}

static uint16_t msm_sensor_id_by_mask(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t chipid)
{
	uint16_t sensor_id = chipid;
	int16_t sensor_id_mask = s_ctrl->sensordata->slave_info->sensor_id_mask;

	if (!sensor_id_mask)
		sensor_id_mask = ~sensor_id_mask;

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;
	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}
	return sensor_id;
}

int msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 start*/
	uint16_t msm_2m_id = 0;
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 end*/
/*HS70 code for HS70 xxx by xiayu at 2019/11/13 start*/
	uint16_t gc8034_supply_id = 0;
	uint16_t hi1336_supply_id = 0;
	static enum msm_camera_i2c_reg_addr_type gc8034_type;
	static enum msm_camera_i2c_reg_addr_type hi1336_type;
/*HS70 code for HS70 xxx by xiayu at 2019/11/13 end*/
/*HS70 code for HS70 1111 by xiayu at 2020/03/06 start*/
	static uint16_t s5k3l6_supply_id = 0;
	static enum msm_camera_i2c_reg_addr_type s5k3l6_type;
/*HS70 code for HS70 1111 by xiayu at 2020/03/06 end*/
/*HS70 code for HS70 1111 by xiayu at 2020/01/21 start*/
	uint16_t s5k4h7_supply_id = 0;
	static enum msm_camera_i2c_reg_addr_type s5k4h7_type;
/*HS70 code for HS70 1111 by xiayu at 2020/01/21 end*/
/* HS60 code for HS60-855&HS60-856 by chengzhi at 20190822 start */
	static uint16_t hi556_module_flag = 0;
	static uint16_t hi556_module_id = 0;
	static uint16_t hi556_supply_id = 0;
	uint16_t ov13b10_supply_id = 0;
/* HS60 code for HS60-855&HS60-856 by chengzhi at 20190822 end */
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 start*/
	uint16_t msm_2m_id_hs70 = 0;
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 end*/
/*HS70 code for HS70-515 by xionggengen at 2019/11/13 start*/
	static uint16_t bakeup_i2c_addr;
	static enum msm_camera_i2c_reg_addr_type vendor_addr_type;
	int bakeup_rc = 0;
/*HS70 code for HS70-515 by xionggengen at 2019/11/13 end*/
	if (!s_ctrl) {
		pr_err("%s:%d failed: %pK\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}
	sensor_i2c_client = s_ctrl->sensor_i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;
	if (adsp_shmem_is_initialized()) {
		pr_debug("%s aDSP camera supports sensor_name:%s\n", __func__,
			adsp_shmem_get_sensor_name());
		if (strnstr(sensor_name, adsp_shmem_get_sensor_name(),
			strlen(sensor_name))) {
			pr_debug("%s ARM-side sensor matched with aDSP-side sensor:%s\n",
				__func__, sensor_name);
			return rc;
		}
	}

	if (!sensor_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %pK %pK %pK\n",
			__func__, __LINE__, sensor_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}

	if (slave_info->setting && slave_info->setting->size > 0) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(s_ctrl->sensor_i2c_client,
			slave_info->setting);
		if (rc < 0)
			pr_err("Write array failed prior to probe\n");

	} else {
		CDBG("No writes needed for this sensor before probe\n");
	}

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__, sensor_name);
		return rc;
	}

	pr_debug("%s: read id: 0x%x expected id 0x%x:\n",
			__func__, chipid, slave_info->sensor_id);
	if (msm_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		pr_err("%s chip id %x does not match %x\n",
				__func__, chipid, slave_info->sensor_id);
		return -ENODEV;
	}
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 start*/
    	if(0 == strcmp(sensor_name, "gc2375h")||0 == strcmp(sensor_name, "gc2375h_kg")){
          if(gpio_is_valid(SENSOR_SUB_GPIO_ID)){
                  gpio_direction_input(SENSOR_SUB_GPIO_ID);
                  msm_2m_id = gpio_get_value(SENSOR_SUB_GPIO_ID);
          }
          pr_err("gc2375 msm_2m_id is %d %s", msm_2m_id, sensor_name);
          if((0==strcmp(sensor_name, "gc2375h"))&&(msm_2m_id !=0)){
                  return -ENODEV;
          }else if ((0 == strcmp(sensor_name, "gc2375h_kg"))&&(msm_2m_id !=1)){
                  return -ENODEV;
          }
    	}
/*HS60 code for HS60-368 by xuxianwei at 2019/08/08 end*/
/*HS70 code for HS70-515 by xionggengen at 2019/11/13 start*/
/*HS70 code for HS70-000102 by sunmao at 2020/01/02 start*/
	if(0 == strcmp(sensor_name, "gc5035_hs70_ts")
	   || 0 == strcmp(sensor_name, "gc5035_hs70_ly")
       || 0 == strcmp(sensor_name, "gc5035_hs70_jk")
	   || 0 == strcmp(sensor_name, "gc5035_hs70_par")
	   || 0 == strcmp(sensor_name, "gc5035_hs70_mcn")
	   || 0 == strcmp(sensor_name, "gc5035_hs70_cxt")) {
		    if(0 == strcmp(sensor_name, "gc5035_hs70_ts")){
				bakeup_i2c_addr = sensor_i2c_client->cci_client->sid;
				vendor_addr_type = sensor_i2c_client->addr_type;
				sensor_i2c_client->addr_type = 2;
				sensor_i2c_client->cci_client->sid = 0x52;
			bakeup_rc = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0001,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
			if (bakeup_rc >= 0) {
				sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0002,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->cci_client->sid = bakeup_i2c_addr;
				sensor_i2c_client->addr_type = vendor_addr_type;
				if(gc5035_supply_id !=0x56)
					return -ENODEV;
			} else {
				sensor_i2c_client->cci_client->sid = bakeup_i2c_addr;
				sensor_i2c_client->addr_type = vendor_addr_type;
				return -ENODEV;
				}
			}
/*HS70 code for HS70 xxxxxx by xiayu at 2020/01/20 start*/
/*HS70 code for HS70 by xiayu at 2020/05/12 start*/
		    if(0 == strcmp(sensor_name, "gc5035_hs70_par")||0 == strcmp(sensor_name, "gc5035_hs70_mcn")){
				bakeup_i2c_addr = sensor_i2c_client->cci_client->sid;
				vendor_addr_type = sensor_i2c_client->addr_type;
				sensor_i2c_client->addr_type = 2;
				sensor_i2c_client->cci_client->sid = 0xA6 >> 1;
			bakeup_rc = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0730,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
			if (bakeup_rc >= 0) {
				//sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0730,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->cci_client->sid = bakeup_i2c_addr;
				sensor_i2c_client->addr_type = vendor_addr_type;
			if((0 == strcmp(sensor_name, "gc5035_hs70_par"))&&(gc5035_supply_id != 0x5A)){
					return -ENODEV;
			}
			if((0 == strcmp(sensor_name, "gc5035_hs70_mcn"))&&(gc5035_supply_id != 0x31)){
					return -ENODEV;
			}
			}
			else {
				sensor_i2c_client->cci_client->sid = bakeup_i2c_addr;
				sensor_i2c_client->addr_type = vendor_addr_type;
				return -ENODEV;
			}
		}
/*HS70 code for HS70 by xiayu at 2020/05/12 end*/
/*HS70 code for HS70 xxxxxx by xiayu at 2020/01/20 end*/
		 else {
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x01, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf4,0x40, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf5,0xe9, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf6,0x14, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf8,0x49, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf9,0x82, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfa,0x10, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x81, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x00, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x01, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xd3,0x87, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x00, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf7,0x01,MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f,MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8e, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x02, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x55,0x80, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x65,0x7e, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x66,0x03, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xc0, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x68,0x11, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x00, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xe0,0x1f, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xf0, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x10, MSM_CAMERA_I2C_BYTE_DATA);
				sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd3,&gc5035_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
				if((gc5035_module_flag&0xc) == 0x4){
					sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd4,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
					sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd5,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
				}
				else if((gc5035_module_flag&0x3) == 0x1){
					 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xda,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
					 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xdb,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
				}
				pr_err("gc503570 read eeprom is 0x%x 0x%x", gc5035_supply_id,gc5035_module_id);
				if(0==strcmp(sensor_name, "gc5035_hs70_cxt")){
					if(gc5035_supply_id !=0x54)
						return -ENODEV;
				}
				if(0==strcmp(sensor_name, "gc5035_hs70_jk")){
				//	if(gc5035_module_id != 0x08)
				//		return -ENODEV;
				}
				if(0==strcmp(sensor_name, "gc5035_hs70_ly")){
					if(gc5035_supply_id !=0x57)
						return -ENODEV;
				}
			}
	}
/*HS70 code for HS70-000102 by sunmao at 2020/01/02 end*/
/*HS70 code for HS70-515 by xionggengen at 2019/11/13 end*/
/* HS70 code for HS70-4224 by xiayu at 2020/01/20 start */
if(0 == strcmp(sensor_name, "s5k4h7_hs70_txd")
	||0 == strcmp(sensor_name, "s5k4h7_hs70_par")
	||0 == strcmp(sensor_name, "s5k4h7_hs70_sy")){
		s5k4h7_type=sensor_i2c_client->addr_type;
		sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0x0730,&s5k4h7_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
		sensor_i2c_client->addr_type=s5k4h7_type;
		pr_err("s5k4h7 read eeprom is 0x%x", s5k4h7_supply_id);
/* HS70 code for HS70-4224 by xiayu at 2020/02/13 start */
/* HS70 code for HS70-4632 by xiayu at 2020/03/11 start */
	if((0==strcmp(sensor_name, "s5k4h7_hs70_txd"))&&(((s5k4h7_supply_id ==0x5A))||((s5k4h7_supply_id ==0x32))))
	{
		return -ENODEV;
	}
/* HS70 code for HS70-4224 by xiayu at 2020/02/13 end */
	if(0==strcmp(sensor_name, "s5k4h7_hs70_par")){
		if((s5k4h7_supply_id !=0x5A))
			return -ENODEV;
	}
	if(0==strcmp(sensor_name, "s5k4h7_hs70_sy")){
		if((s5k4h7_supply_id !=0x32))
			return -ENODEV;
	}
/* HS70 code for HS70-4632 by xiayu at 2020/03/11 end */
}
/* HS70 code for HS70-4224 by xiayu at 2020/01/20 end */
/* HS60 code for HS60-263 by xuxianwei at 20190729 start */
	if(0 == strcmp(sensor_name, "gc5035")||0 == strcmp(sensor_name, "gc5035_ly")){
	pr_err("gc5035 read eeprom enter %s", sensor_name);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x01, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf4,0x40, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf5,0xe9, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf6,0x14, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf8,0x49, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf9,0x82, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfa,0x10, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x81, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x01, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xd3,0x87, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf7,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f,MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8e, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x02, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x55,0x80, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x65,0x7e, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x66,0x03, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xc0, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x68,0x11, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xe0,0x1f, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xf0, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x10, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd3,&gc5035_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
	pr_err("gc5035 read   gc5035_module_flag%x", gc5035_module_flag);
/* HS60 code for HS60-263 by xuxianwei at 20190923 start */
	if((gc5035_module_flag&0xc) == 0x4){
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd4,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd5,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if((gc5035_module_flag&0x3) == 0x1){
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xda,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xdb,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
	pr_err("gc5035 read eeprom is 0x%x 0x%x", gc5035_supply_id,gc5035_module_id);
	if(0==strcmp(sensor_name, "gc5035")){
		if((gc5035_supply_id !=0x55)&&(gc5035_module_id !=0x41)){
			return -ENODEV;
		}
	}
	if(0==strcmp(sensor_name, "gc5035_ly")){
		if((gc5035_supply_id !=0x57)&&(gc5035_module_id !=0x61)){
			return -ENODEV;
		}
	}
	}
/* HS60 code for HS60-263 by xuxianwei at 20190923 end */
/* HS60 code for HS60-263 by xuxianwei at 20190729 end */
/* HS70 code for HS70 4632 by xiayu at 2020/03/06 start */
if(0 == strcmp(sensor_name, "s5k3l6_hs70_jk")||0 == strcmp(sensor_name, "s5k3l6_hs70_xl")||0 == strcmp(sensor_name,"s5k3l6_hs70_cam")||0 == strcmp(sensor_name, "s5k3l6_hs70_qt")){
	s5k3l6_type=sensor_i2c_client->addr_type;
	sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
	if(0==strcmp(sensor_name, "s5k3l6_hs70_qt")){
	sensor_i2c_client->cci_client->sid=0xA2 >> 1;
	}else
	{
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
	}
	sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&s5k3l6_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
	sensor_i2c_client->addr_type=s5k3l6_type;
	if((0==strcmp(sensor_name, "s5k3l6_hs70_jk"))&&(s5k3l6_supply_id !=0x08)){
            return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "s5k3l6_hs70_xl"))&&(s5k3l6_supply_id !=0x02)){
            return -ENODEV;
		}else if ((0 == strcmp(sensor_name, "s5k3l6_hs70_cam"))&&(s5k3l6_supply_id !=0x5B)){
			return -ENODEV;
		}else if ((0 == strcmp(sensor_name, "s5k3l6_hs70_qt"))&&(s5k3l6_supply_id !=0x06)){
			return -ENODEV;
		}
    pr_err("s5k3l6 supply_id is %d %s,sensor_slave_addr=%x", s5k3l6_supply_id, sensor_name,slave_info->sensor_slave_addr);
}
/* HS70 code for HS70 4632 by xiayu at 2020/03/06 end */
/*HS70 code for HS70 xxx by xiayu at 2019/11/13 start*/
if(0 == strcmp(sensor_name, "gc8034_hs70_ly")||0 == strcmp(sensor_name, "gc8034_hs70_jk")){
		gc8034_type=sensor_i2c_client->addr_type;
		sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&gc8034_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
		sensor_i2c_client->addr_type=gc8034_type;
		if((0==strcmp(sensor_name, "gc8034_hs70_ly"))&&(gc8034_supply_id !=0x57)){
              return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "gc8034_hs70_jk"))&&(gc8034_supply_id !=0x08)){
             return -ENODEV;
    }
          pr_err("gc8034 supply_id is %d %s,sensor_slave_addr=%x", gc8034_supply_id, sensor_name,slave_info->sensor_slave_addr);
}
/*HS70 code for HS70 xxx by gaozhenyu at 2020/01/17 start*/
if(0 == strcmp(sensor_name, "hi1336_hs70_hlt")||0 == strcmp(sensor_name, "hi1336_hs70_ts")||0 == strcmp(sensor_name, "hi1336_hs70_xl")){
		hi1336_type=sensor_i2c_client->addr_type;
		sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&hi1336_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
		sensor_i2c_client->addr_type=hi1336_type;
		if((0==strcmp(sensor_name, "hi1336_hs70_hlt"))&&(hi1336_supply_id !=0x55)){
              return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "hi1336_hs70_ts"))&&(hi1336_supply_id !=0x56)){
             return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "hi1336_hs70_xl"))&&(hi1336_supply_id !=0x02)){
             return -ENODEV;
        }
        pr_err("hi1336 supply_id is %d %s,sensor_slave_addr=%x", hi1336_supply_id, sensor_name,slave_info->sensor_slave_addr);
}
/*HS70 code for HS70 xxx by gaozhenyu at 2020/01/17 end*/
/*HS70 code for HS70 xxx by xiayu at 2019/11/13 end*/
/* HS60 code for HS60-855&HS60-856 by chengzhi at 20190822 start */
/*HS60 code for HS60-5254  by huangjiwu at 2019/02/25 start*/
	if(0 == strcmp(sensor_name, "ov13b10")||0 == strcmp(sensor_name, "ov13b10_qt") || 0 == strcmp(sensor_name, "ov13b10_change")){
		uint16_t ov13b10_module_id = 0;
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&ov13b10_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0x0002,&ov13b10_module_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
		if((0==strcmp(sensor_name, "ov13b10") || 0==strcmp(sensor_name, "ov13b10_change"))&&(ov13b10_supply_id !=0x58)){
                  return -ENODEV;
          	}else if ((0 == strcmp(sensor_name, "ov13b10_qt"))&&(ov13b10_supply_id !=0x06)){
                  return -ENODEV;
          	}
          pr_err("ov13b10 supply_id is %d %s,sensor_slave_addr=%x", ov13b10_supply_id, sensor_name,slave_info->sensor_slave_addr);
		if((0==strcmp(sensor_name, "ov13b10")) && (ov13b10_module_id != 0x31)) {
			return -ENODEV;
		} else if((0==strcmp(sensor_name, "ov13b10_change")) && (ov13b10_module_id != 0xc1)) {
			return -ENODEV;
		}
/*HS60 code for HS60-5254  by huangjiwu at 2019/02/25 end*/
    	}
    	if(0 == strcmp(sensor_name, "hi556")||0 == strcmp(sensor_name, "hi556_txd")){
		if(hi556_module_flag == 0x0){
			pr_err("hi556 read eeprom enter %s", sensor_name);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0e00,0x0102, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0e02,0x0102, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0e0c,0x0100, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x27fe,0xe000, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0b0e,0x8600, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0d04,0x0100, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0d02,0x0707, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0f30,0x6e25, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0f32,0x7067, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0f02,0x0106, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a04,0x0000, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0e0a,0x0001,MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x004a,0x0100,MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x003e,0x1000, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a00,0x0100, MSM_CAMERA_I2C_WORD_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a02,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a00,0x0,  MSM_CAMERA_I2C_BYTE_DATA);
			msleep(10);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0f02,0x0,  MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x011a,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x011b,0x09, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0d04,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0d02,0x07, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x003e,0x10, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a00,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010a,0x04, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010b,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0102,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
			if(hi556_module_flag == 0x01){
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010a,0x04, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010b,0x02, MSM_CAMERA_I2C_BYTE_DATA);
	        	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0102,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_module_id, MSM_CAMERA_I2C_BYTE_DATA);
			}
			else if(hi556_module_flag == 0x13){
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010a,0x04, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010b,0x13, MSM_CAMERA_I2C_BYTE_DATA);
	        	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0102,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_module_id, MSM_CAMERA_I2C_BYTE_DATA);
			}
			else if(hi556_module_flag == 0x37){
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010a,0x04, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x010b,0x24, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0102,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0x0108,&hi556_module_id, MSM_CAMERA_I2C_BYTE_DATA);
			}
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a00,0x0, MSM_CAMERA_I2C_BYTE_DATA);
			msleep(10);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x004a,0x0, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0d04,0x0, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x003e,0x0, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x004a,0x01, MSM_CAMERA_I2C_BYTE_DATA);
			sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x0a00,0x01, MSM_CAMERA_I2C_BYTE_DATA);
		}
		if(0==strcmp(sensor_name, "hi556")){
			if((hi556_supply_id !=0x08)&&(hi556_module_id !=0x51)){
				pr_err("hi556 return is 0x%x 0x%x", hi556_supply_id,hi556_module_id);
				return -ENODEV;
			}
		}
		else if(0==strcmp(sensor_name, "hi556_txd")){
			if((hi556_supply_id !=0x58)&&(hi556_module_id !=0x81)){
				pr_err("hi556 return is 0x%x 0x%x", hi556_supply_id,hi556_module_id);
				return -ENODEV;
			}
		}
		pr_err("hi556 read eeprom is 0x%x 0x%x", hi556_supply_id,hi556_module_id);
    	}
/* HS60 code for HS60-855&HS60-856 by chengzhi at 20190822 end */
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 start*/
    if(0 == strcmp(sensor_name, "gc2375h_hs70_jk")||0 == strcmp(sensor_name, "gc2375h_cxt")){
        if(gpio_is_valid(SENSOR_SUB_GPIO_ID_HS70)){
		gpio_direction_input(SENSOR_SUB_GPIO_ID_HS70);
                msm_2m_id_hs70 = gpio_get_value(SENSOR_SUB_GPIO_ID_HS70);
        }
        pr_err("gc2375 msm_2m_id_hs70 is %d %s", msm_2m_id_hs70, sensor_name);
        if((0==strcmp(sensor_name, "gc2375h_cxt"))&&(msm_2m_id_hs70 !=0)){
            return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "gc2375h_hs70_jk"))&&(msm_2m_id_hs70 !=1)){
            return -ENODEV;
        }
    }
	if(0 == strcmp(sensor_name, "gc02m1_hs70_xl")||0 == strcmp(sensor_name, "gc02m1_hs70_mcn")){
        if(gpio_is_valid(SENSOR_SUB_GPIO_ID_HS70)){
		gpio_direction_input(SENSOR_SUB_GPIO_ID_HS70);
                msm_2m_id_hs70 = gpio_get_value(SENSOR_SUB_GPIO_ID_HS70);
        }
        pr_err("gc02m1 msm_2m_id_hs70 is %d %s", msm_2m_id_hs70, sensor_name);
        if((0==strcmp(sensor_name, "gc02m1_hs70_mcn"))&&(msm_2m_id_hs70 !=1)){
            return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "gc02m1_hs70_xl"))&&(msm_2m_id_hs70 !=0)){
            return -ENODEV;
        }
    }
/*HS70 code for HS70 xxx by chengzhi at 2019/09/30 end*/
/*HS70 code for HS70 xxx by sunmao at 2020/04/15 start*/
    if(0 == strcmp(sensor_name, "gc02m1")||0 == strcmp(sensor_name, "gc02m1_hs70_jk")){
        if(gpio_is_valid(SENSOR_SUB_GPIO_ID_HS70)){
		gpio_direction_input(SENSOR_SUB_GPIO_ID_HS70);
                msm_2m_id_hs70 = gpio_get_value(SENSOR_SUB_GPIO_ID_HS70);
        }
        pr_err("gc02m1 msm_2m_id_hs70 is %d %s", msm_2m_id_hs70, sensor_name);
        if((0==strcmp(sensor_name, "gc02m1"))&&(msm_2m_id_hs70 !=1)){
            return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "gc02m1_hs70_jk"))&&(msm_2m_id_hs70 !=0)){
            return -ENODEV;
        }
    }
/*HS70 code for HS70 xxx by sunmao at 2020/04/15 end*/
/*HS50 code for HS50 xxx by gaozhenyu at 2020/08/12 start*/
#if defined (HUAQIN_KERNEL_PROJECT_HS50)
if(0 == strcmp(sensor_name, "s5k3l6_hs50_ofilm")||0 == strcmp(sensor_name, "s5k3l6_hs50_sjc")){
		hi1336_type=sensor_i2c_client->addr_type;
		sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
		if(0==strcmp(sensor_name, "s5k3l6_hs50_sjc")){
	sensor_i2c_client->cci_client->sid=0xB0 >> 1;
	}else
	{
		sensor_i2c_client->cci_client->sid=0xA0 >> 1;
}
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&s5k3l6_supply_id,
MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
		sensor_i2c_client->addr_type=hi1336_type;
		if((0==strcmp(sensor_name, "s5k3l6_hs50_ofilm"))&&(s5k3l6_supply_id !=0x07)){
              return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "s5k3l6_hs50_sjc"))&&(s5k3l6_supply_id !=0x10)){
             return -ENODEV;
        }
        CDBG("s5k3l6 supply_id is %d %s,sensor_slave_addr=%x", s5k3l6_supply_id, sensor_name,slave_info->sensor_slave_addr);
}
/*HS50 code for HS50-SR-QL3095-01-97 gc5035com jk otp copatible by wangqi at 2020/10/13 start*/
if(0 == strcmp(sensor_name, "gc5035_hs50_jk")||0 == strcmp(sensor_name, "gc5035_hs50_sjc")||0 == strcmp(sensor_name, "gc5035_com_hs50_jk")){
	pr_err("gc5035 read eeprom enter %s", sensor_name);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x01, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf4,0x40, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf5,0xe9, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf6,0x14, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf8,0x49, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf9,0x82, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfa,0x10, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x81, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x01, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xd3,0x87, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x36,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf7,0x01,MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f,MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8f, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfc,0x8e, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xfe,0x02, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x55,0x80, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x65,0x7e, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x66,0x03, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xc0, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x68,0x11, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xe0,0x1f, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0x67,0xf0, MSM_CAMERA_I2C_BYTE_DATA);
	sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, 0xf3,0x10, MSM_CAMERA_I2C_BYTE_DATA);
if(0==strcmp(sensor_name, "gc5035_hs50_jk")){
	sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd3,&gc5035_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
        if((gc5035_module_flag&0xc) == 0x4){
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd4,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd5,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if((gc5035_module_flag&0x3) == 0x1){
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xda,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xdb,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
}else if(0==strcmp(sensor_name, "gc5035_hs50_sjc"))
	{
        sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xc3,&gc5035_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
        if((gc5035_module_flag&0xc) == 0x4){
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xc4,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xc5,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if((gc5035_module_flag&0x3) == 0x1){
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xcc,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xcd,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
}else{
	sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd3,&gc5035_module_flag, MSM_CAMERA_I2C_BYTE_DATA);
        if((gc5035_module_flag&0xc) == 0x4){
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd4,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xd5,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if((gc5035_module_flag&0x3) == 0x1){
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xda,&gc5035_supply_id, MSM_CAMERA_I2C_BYTE_DATA);
		 sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, 0xdb,&gc5035_module_id, MSM_CAMERA_I2C_BYTE_DATA);
	}
}
	CDBG("gc5035 read   gc5035_module_flag%x", gc5035_module_flag);
	pr_err("gc5035 read eeprom is 0x%x 0x%x", gc5035_supply_id,gc5035_module_id);
	if(0==strcmp(sensor_name, "gc5035_hs50_jk")){
		if((gc5035_supply_id !=0x08)||(gc5035_module_id !=0x3b)){
			return -ENODEV;
		}
	}
	if(0==strcmp(sensor_name, "gc5035_hs50_sjc")){
		if((gc5035_supply_id !=0x10)&&(gc5035_module_id !=0x3b)){
			return -ENODEV;
		}
	}
	if(0==strcmp(sensor_name, "gc5035_com_hs50_jk")){
		if((gc5035_supply_id !=0x08)||(gc5035_module_id !=0x3a)){
/*HS50 code for HS50-SR-QL3095-01-136 rm otp copatible gc5035com for default by wangqi at 2020/10/13 start*/
			//return -ENODEV;
/*HS50 code for HS50-SR-QL3095-01-136 rm otp copatible gc5035com for default by wangqi at 2020/10/13 start*/
		}
	}
	}
/*HS50 code for HS50-SR-QL3095-01-97 gc5035com jk otp copatible by wangqi at 2020/10/13 end*/
/*HS50 code for HS50 xxx by chenjun6 at 2020/08/20 start*/
if(0 == strcmp(sensor_name, "gc02m1_hs50_cxt")||0 == strcmp(sensor_name, "gc02m1_hs50_jk")||0 == strcmp(sensor_name, "gc02m1_hs50_sjc"))
{
 vendor_addr_type=sensor_i2c_client->addr_type;
 sensor_i2c_client->cci_client->sid=0xA4 >> 1;
 sensor_i2c_client->addr_type=MSM_CAMERA_I2C_WORD_ADDR;
 bakeup_rc = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,0X0001,&gc02m1_supply_id,MSM_CAMERA_I2C_BYTE_DATA);
 sensor_i2c_client->cci_client->sid=slave_info->sensor_slave_addr >> 1;
 sensor_i2c_client->addr_type=vendor_addr_type;
 CDBG("supply id is 0x%x" , gc02m1_supply_id);
 if((0==strcmp(sensor_name, "gc02m1_hs50_cxt"))&&(gc02m1_supply_id !=0x54))
 {
              return -ENODEV;
        }
 else if((0 == strcmp(sensor_name, "gc02m1_hs50_jk"))&&(gc02m1_supply_id !=0x08))
 {
              return -ENODEV;
        }
 else if((0 == strcmp(sensor_name, "gc02m1_hs50_sjc"))&&(gc02m1_supply_id !=0x10))
 {
              return -ENODEV;
        }
 CDBG("gc02m1 is %s, supply_id is %d, sensor_slave_addr=%x", sensor_name, gc02m1_supply_id, slave_info->sensor_slave_addr);
}
/*HS50 code for HS50 xxx by chenjun6 at 2020/08/20 end*/
    if(0 == strcmp(sensor_name, "gc2375h_hs50_sjc")||0 == strcmp(sensor_name, "gc2375h_hs50_cxt")){
        if(gpio_is_valid(SENSOR_SUB_GPIO_ID_HS50)){
		gpio_direction_input(SENSOR_SUB_GPIO_ID_HS50);
                msm_2m_id_hs70 = gpio_get_value(SENSOR_SUB_GPIO_ID_HS50);
        }
        pr_err("gc2375h msm_2m_id_hs50 is %d %s", msm_2m_id_hs70, sensor_name);
        if((0==strcmp(sensor_name, "gc2375h_hs50_cxt"))&&(msm_2m_id_hs70 !=1)){
            return -ENODEV;
        }else if ((0 == strcmp(sensor_name, "gc2375h_hs50_sjc"))&&(msm_2m_id_hs70 !=0)){
            return -ENODEV;
        }
    }
#endif
/*HS50 code for HS50 xxx by gaozhenyu at 2020/08/12 end*/
	return rc;
}

static struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(container_of(sd, struct msm_sd_subdev, sd),
		struct msm_sensor_ctrl_t, msm_sd);
}

static void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &s_ctrl->stop_setting);
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;

		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			pr_err("s_ctrl->func_tbl NULL\n");
		}
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
}

static int msm_sensor_get_af_status(struct msm_sensor_ctrl_t *s_ctrl,
			void *argp)
{
	/* TO-DO: Need to set AF status register address and expected value
	 * We need to check the AF status in the sensor register and
	 * set the status in the *status variable accordingly
	 */
	return 0;
}

static long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void *argp = (void *)arg;

	if (!s_ctrl) {
		pr_err("%s s_ctrl NULL\n", __func__);
		return -EBADF;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
#ifdef CONFIG_COMPAT
		if (is_compat_task())
			rc = s_ctrl->func_tbl->sensor_config32(s_ctrl, argp);
		else
#endif
			rc = s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
		return rc;
	case VIDIOC_MSM_SENSOR_GET_AF_STATUS:
		return msm_sensor_get_af_status(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
	case MSM_SD_SHUTDOWN:
		msm_sensor_stop_stream(s_ctrl);
		return 0;
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_UNNOTIFY_FREEZE:
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_sensor_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG32:
		cmd = VIDIOC_MSM_SENSOR_CFG;
	default:
		return msm_sensor_subdev_ioctl(sd, cmd, arg);
	}
}

long msm_sensor_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_sensor_subdev_do_ioctl);
}

static int msm_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
	void *argp)
{
	struct sensorb_cfg_data32 *cdata = (struct sensorb_cfg_data32 *)argp;
	int32_t rc = 0;
	int32_t i = 0;

	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
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
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
			CDBG("%s:%d subdev_intf[%d] %d\n", __func__, __LINE__,
				i, cdata->cfg.sensor_info.subdev_intf[i]);
		}
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_WRITE_I2C_ARRAY:
	case CFG_WRITE_I2C_ARRAY_SYNC:
	case CFG_WRITE_I2C_ARRAY_SYNC_BLOCK:
	case CFG_WRITE_I2C_ARRAY_ASYNC: {
		struct msm_camera_i2c_reg_setting32 conf_array32;
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (adsp_shmem_get_state() != CAMERA_STATUS_END) {
			mutex_unlock(s_ctrl->msm_sensor_mutex);
			return 0;
		}

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array32,
			(void __user *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.data_type = conf_array32.data_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;

		if (!conf_array.size ||
			conf_array.size > I2C_REG_DATA_MAX) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
			(void __user *)
			compat_ptr(conf_array32.reg_setting),
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;

		if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table(s_ctrl->sensor_i2c_client,
				&conf_array);
		else if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY_ASYNC)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_async(s_ctrl->sensor_i2c_client,
				&conf_array);
		else if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY_SYNC_BLOCK)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_sync_block(
				s_ctrl->sensor_i2c_client,
				&conf_array);
		else
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_sync(s_ctrl->sensor_i2c_client,
				&conf_array);

		kfree(reg_setting);
		break;
	}
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		struct msm_camera_i2c_read_config *read_config_ptr = NULL;
		uint16_t local_data = 0;
		uint16_t orig_slave_addr = 0, read_slave_addr = 0;
		uint16_t orig_addr_type = 0, read_addr_type = 0;

		if (adsp_shmem_get_state() != CAMERA_STATUS_END) {
			mutex_unlock(s_ctrl->msm_sensor_mutex);
			return 0;
		}

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		read_config_ptr =
			(__force struct msm_camera_i2c_read_config *)
			compat_ptr(cdata->cfg.setting);

		if (copy_from_user(&read_config,
			(void __user *) read_config_ptr,
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		read_slave_addr = read_config.slave_addr;
		read_addr_type = read_config.addr_type;

		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				read_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				read_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				read_slave_addr >> 1);

		orig_addr_type = s_ctrl->sensor_i2c_client->addr_type;
		s_ctrl->sensor_i2c_client->addr_type = read_addr_type;

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		}
		s_ctrl->sensor_i2c_client->addr_type = orig_addr_type;

		pr_debug("slave_read %x %x %x\n", read_slave_addr,
			read_config.reg_addr, local_data);

		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user((void __user *)(&read_config_ptr->data),
				&local_data, sizeof(local_data))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CFG_SLAVE_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_array_write_config32 write_config32;
		struct msm_camera_i2c_array_write_config write_config;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		uint16_t orig_slave_addr = 0, write_slave_addr = 0;
		uint16_t orig_addr_type = 0, write_addr_type = 0;

		if (adsp_shmem_get_state() != CAMERA_STATUS_END) {
			mutex_unlock(s_ctrl->msm_sensor_mutex);
			return 0;
		}

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (copy_from_user(&write_config32,
				(void __user *)compat_ptr(cdata->cfg.setting),
				sizeof(
				struct msm_camera_i2c_array_write_config32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		write_config.slave_addr = write_config32.slave_addr;
		write_config.conf_array.addr_type =
			write_config32.conf_array.addr_type;
		write_config.conf_array.data_type =
			write_config32.conf_array.data_type;
		write_config.conf_array.delay =
			write_config32.conf_array.delay;
		write_config.conf_array.size =
			write_config32.conf_array.size;

		pr_debug("%s:CFG_SLAVE_WRITE_I2C_ARRAY:\n", __func__);
		pr_debug("%s:slave_addr=0x%x, array_size=%d addr_type=%d data_type=%d\n",
			__func__,
			write_config.slave_addr,
			write_config.conf_array.size,
			write_config.conf_array.addr_type,
			write_config.conf_array.data_type);

		if (!write_config.conf_array.size ||
			write_config.conf_array.size > I2C_REG_DATA_MAX) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(write_config.conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
				(void __user *)
				compat_ptr(
				write_config32.conf_array.reg_setting),
				write_config.conf_array.size *
				sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		write_config.conf_array.reg_setting = reg_setting;
		write_slave_addr = write_config.slave_addr;
		write_addr_type = write_config.conf_array.addr_type;

		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				write_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				write_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.",
				__func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		pr_debug("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x\n",
				__func__, orig_slave_addr,
				write_slave_addr >> 1);
		orig_addr_type = s_ctrl->sensor_i2c_client->addr_type;
		s_ctrl->sensor_i2c_client->addr_type = write_addr_type;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &(write_config.conf_array));

		s_ctrl->sensor_i2c_client->addr_type = orig_addr_type;
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		} else {
			pr_err("%s: error: no i2c/cci client found.\n",
				__func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting32 conf_array32;
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (adsp_shmem_get_state() != CAMERA_STATUS_END) {
			mutex_unlock(s_ctrl->msm_sensor_mutex);
			return 0;
		}

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array32,
			(void __user *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_seq_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;

		if (!conf_array.size ||
			conf_array.size > I2C_SEQ_REG_DATA_MAX) {
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
		if (copy_from_user(reg_setting,
			(void __user *)
			compat_ptr(conf_array32.reg_setting),
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
		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 1);

			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;
	case CFG_POWER_DOWN:
		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;
	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting32 stop_setting32;
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;

		if (adsp_shmem_get_state() != CAMERA_STATUS_END) {
			mutex_unlock(s_ctrl->msm_sensor_mutex);
			return 0;
		}

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (copy_from_user(&stop_setting32,
				(void __user *)compat_ptr((cdata->cfg.setting)),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		stop_setting->addr_type = stop_setting32.addr_type;
		stop_setting->data_type = stop_setting32.data_type;
		stop_setting->delay = stop_setting32.delay;
		stop_setting->size = stop_setting32.size;

		if (!stop_setting->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void __user *)
			compat_ptr(stop_setting32.reg_setting),
			stop_setting->size *
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

	case CFG_SET_I2C_SYNC_PARAM: {
		struct msm_camera_cci_ctrl cci_ctrl;

		s_ctrl->sensor_i2c_client->cci_client->cid =
			cdata->cfg.sensor_i2c_sync_params.cid;
		s_ctrl->sensor_i2c_client->cci_client->id_map =
			cdata->cfg.sensor_i2c_sync_params.csid;

		CDBG("I2C_SYNC_PARAM CID:%d, line:%d delay:%d, cdid:%d\n",
			s_ctrl->sensor_i2c_client->cci_client->cid,
			cdata->cfg.sensor_i2c_sync_params.line,
			cdata->cfg.sensor_i2c_sync_params.delay,
			cdata->cfg.sensor_i2c_sync_params.csid);

		cci_ctrl.cmd = MSM_CCI_SET_SYNC_CID;
		cci_ctrl.cfg.cci_wait_sync_cfg.line =
			cdata->cfg.sensor_i2c_sync_params.line;
		cci_ctrl.cfg.cci_wait_sync_cfg.delay =
			cdata->cfg.sensor_i2c_sync_params.delay;
		cci_ctrl.cfg.cci_wait_sync_cfg.cid =
			cdata->cfg.sensor_i2c_sync_params.cid;
		cci_ctrl.cfg.cci_wait_sync_cfg.csid =
			cdata->cfg.sensor_i2c_sync_params.csid;
		rc = v4l2_subdev_call(s_ctrl->sensor_i2c_client->
				cci_client->cci_subdev,
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		if (rc < 0) {
			pr_err("%s: line %d rc = %d\n", __func__, __LINE__, rc);
			rc = -EFAULT;
			break;
		}
		break;
	}

	default:
		rc = -EFAULT;
		break;
	}

DONE:
	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}
#endif

int msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;

	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
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
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
			CDBG("%s:%d subdev_intf[%d] %d\n", __func__, __LINE__,
				i, cdata->cfg.sensor_info.subdev_intf[i]);
		}
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;

	case CFG_WRITE_I2C_ARRAY:
	case CFG_WRITE_I2C_ARRAY_SYNC:
	case CFG_WRITE_I2C_ARRAY_SYNC_BLOCK:
	case CFG_WRITE_I2C_ARRAY_ASYNC: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void __user *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_REG_DATA_MAX) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
			(void __user *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table(s_ctrl->sensor_i2c_client,
					&conf_array);
		else if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY_ASYNC)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_async(s_ctrl->sensor_i2c_client,
					&conf_array);
		else if (cdata->cfgtype == CFG_WRITE_I2C_ARRAY_SYNC_BLOCK)
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_sync_block(
					s_ctrl->sensor_i2c_client,
					&conf_array);
		else
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table_sync(s_ctrl->sensor_i2c_client,
					&conf_array);

		kfree(reg_setting);
		break;
	}
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		struct msm_camera_i2c_read_config *read_config_ptr = NULL;
		uint16_t local_data = 0;
		uint16_t orig_slave_addr = 0, read_slave_addr = 0;
		uint16_t orig_addr_type = 0, read_addr_type = 0;

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		read_config_ptr =
			(struct msm_camera_i2c_read_config *)cdata->cfg.setting;
		if (copy_from_user(&read_config,
			(void __user *)read_config_ptr,
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		read_slave_addr = read_config.slave_addr;
		read_addr_type = read_config.addr_type;
		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				read_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				read_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				read_slave_addr >> 1);

		orig_addr_type = s_ctrl->sensor_i2c_client->addr_type;
		s_ctrl->sensor_i2c_client->addr_type = read_addr_type;

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		}
		s_ctrl->sensor_i2c_client->addr_type = orig_addr_type;

		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user((void __user *)(&read_config_ptr->data),
				&local_data, sizeof(local_data))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CFG_SLAVE_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_array_write_config write_config;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		uint16_t orig_slave_addr = 0,  write_slave_addr = 0;
		uint16_t orig_addr_type = 0, write_addr_type = 0;

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (copy_from_user(&write_config,
			(void __user *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_array_write_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:CFG_SLAVE_WRITE_I2C_ARRAY:", __func__);
		CDBG("%s:slave_addr=0x%x, array_size=%d\n", __func__,
			write_config.slave_addr,
			write_config.conf_array.size);

		if (!write_config.conf_array.size ||
			write_config.conf_array.size > I2C_REG_DATA_MAX) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(write_config.conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
				(void __user *)
				(write_config.conf_array.reg_setting),
				write_config.conf_array.size *
				sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		write_config.conf_array.reg_setting = reg_setting;
		write_slave_addr = write_config.slave_addr;
		write_addr_type = write_config.conf_array.addr_type;
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				write_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				write_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				write_slave_addr >> 1);
		orig_addr_type = s_ctrl->sensor_i2c_client->addr_type;
		s_ctrl->sensor_i2c_client->addr_type = write_addr_type;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &(write_config.conf_array));
		s_ctrl->sensor_i2c_client->addr_type = orig_addr_type;
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void __user *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_SEQ_REG_DATA_MAX) {
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
		if (copy_from_user(reg_setting,
			(void __user *)conf_array.reg_setting,
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
		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 1);

			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %d\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			CDBG("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (s_ctrl->is_csid_tg_mode)
			goto DONE;

		if (copy_from_user(stop_setting,
			(void __user *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;

		if (!stop_setting->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void __user *)reg_setting,
			stop_setting->size *
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

	case CFG_SET_I2C_SYNC_PARAM: {
		struct msm_camera_cci_ctrl cci_ctrl;

		s_ctrl->sensor_i2c_client->cci_client->cid =
			cdata->cfg.sensor_i2c_sync_params.cid;
		s_ctrl->sensor_i2c_client->cci_client->id_map =
			cdata->cfg.sensor_i2c_sync_params.csid;

		CDBG("I2C_SYNC_PARAM CID:%d, line:%d delay:%d, cdid:%d\n",
			s_ctrl->sensor_i2c_client->cci_client->cid,
			cdata->cfg.sensor_i2c_sync_params.line,
			cdata->cfg.sensor_i2c_sync_params.delay,
			cdata->cfg.sensor_i2c_sync_params.csid);

		cci_ctrl.cmd = MSM_CCI_SET_SYNC_CID;
		cci_ctrl.cfg.cci_wait_sync_cfg.line =
			cdata->cfg.sensor_i2c_sync_params.line;
		cci_ctrl.cfg.cci_wait_sync_cfg.delay =
			cdata->cfg.sensor_i2c_sync_params.delay;
		cci_ctrl.cfg.cci_wait_sync_cfg.cid =
			cdata->cfg.sensor_i2c_sync_params.cid;
		cci_ctrl.cfg.cci_wait_sync_cfg.csid =
			cdata->cfg.sensor_i2c_sync_params.csid;
		rc = v4l2_subdev_call(s_ctrl->sensor_i2c_client->
				cci_client->cci_subdev,
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		if (rc < 0) {
			pr_err("%s: line %d rc = %d\n", __func__, __LINE__, rc);
			rc = -EFAULT;
			break;
		}
		break;
	}

	default:
		rc = -EFAULT;
		break;
	}

DONE:
	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int msm_sensor_check_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc;

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static int msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (!on && s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}
static struct v4l2_subdev_core_ops msm_sensor_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};
static struct v4l2_subdev_ops msm_sensor_subdev_ops = {
	.core = &msm_sensor_subdev_core_ops,
};

static struct msm_sensor_fn_t msm_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = msm_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
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
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
	.i2c_write_table_async = msm_camera_cci_i2c_write_table_async,
	.i2c_write_table_sync = msm_camera_cci_i2c_write_table_sync,
	.i2c_write_table_sync_block = msm_camera_cci_i2c_write_table_sync_block,

};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_write_conf_tbl = msm_camera_qup_i2c_write_conf_tbl,
	.i2c_write_table_async = msm_camera_qup_i2c_write_table,
	.i2c_write_table_sync = msm_camera_qup_i2c_write_table,
	.i2c_write_table_sync_block = msm_camera_qup_i2c_write_table,
};

static struct msm_camera_i2c_fn_t msm_sensor_secure_func_tbl = {
	.i2c_read = msm_camera_tz_i2c_read,
	.i2c_read_seq = msm_camera_tz_i2c_read_seq,
	.i2c_write = msm_camera_tz_i2c_write,
	.i2c_write_table = msm_camera_tz_i2c_write_table,
	.i2c_write_seq_table = msm_camera_tz_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_tz_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_tz_i2c_util,
	.i2c_write_conf_tbl = msm_camera_tz_i2c_write_conf_tbl,
	.i2c_write_table_async = msm_camera_tz_i2c_write_table_async,
	.i2c_write_table_sync = msm_camera_tz_i2c_write_table_sync,
	.i2c_write_table_sync_block = msm_camera_tz_i2c_write_table_sync_block,
};

int32_t msm_sensor_init_default_params(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_cci_client *cci_client = NULL;
	unsigned long mount_pos = 0;
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29start*/
#ifdef CONFIG_MSM_CAMERA_HS60_ADDBOARD
	int ret = 0;
	ret = swtp_sku_conf();
	if (!ret)
	{
		pr_err("swtp_sku_conf fail");
		swtp_sku_conf();
	}
#endif
/*HS60 code for HS60-3438 by xuxianwei at 2019/10/29end*/
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 start*/
#ifdef HUAQIN_KERNEL_PROJECT_HS50
	hs50_board_id = read_hs50_id();
#endif
/*HS50 code for HS50-SR-QL3095-01-97 by chenjun6 at 2020/09/08 end*/
	/* Validate input parameters */
	if (!s_ctrl) {
		pr_err("%s:%d failed: invalid params s_ctrl %pK\n", __func__,
			__LINE__, s_ctrl);
		return -EINVAL;
	}

	if (!s_ctrl->sensor_i2c_client) {
		pr_err("%s:%d failed: invalid params sensor_i2c_client %pK\n",
			__func__, __LINE__, s_ctrl->sensor_i2c_client);
		return -EINVAL;
	}

	/* Initialize cci_client */
	s_ctrl->sensor_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client->cci_client)
		return -ENOMEM;

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = s_ctrl->sensor_i2c_client->cci_client;

		/* Get CCI subdev */
		cci_client->cci_subdev = msm_cci_get_subdev();

		if (s_ctrl->is_secure)
			msm_camera_tz_i2c_register_sensor((void *)s_ctrl);

		/* Update CCI / I2C function table */
		if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
			s_ctrl->sensor_i2c_client->i2c_func_tbl =
				&msm_sensor_cci_func_tbl;
	} else {
		if (!s_ctrl->sensor_i2c_client->i2c_func_tbl) {
			CDBG("%s:%d\n", __func__, __LINE__);
			s_ctrl->sensor_i2c_client->i2c_func_tbl =
				&msm_sensor_qup_func_tbl;
		}
	}

	/* Update function table driven by ioctl */
	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;

	/* Update v4l2 subdev ops table */
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;

	/* Update sensor mount angle and position in media entity flag */
	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	return 0;
}
