/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "imgsensor_sensor.h"

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "kd_camera_typedef.h"
#include "kd_camera_feature.h"
#include "kd_imgsensor.h"

#include "imgsensor_hw.h"

int system_hw_rev = -1;
static int board_rev_setup(char *str)
{
	int n;
	if (!get_option(&str, &n)) {
		pr_err("%s:fail \n", __func__);
		return -1;
	}

	system_hw_rev = n;
	return 0;
}
__setup("androidboot.revision=", board_rev_setup);

int get_hw_board_version(void)
{
	return system_hw_rev;
}

struct IMGSENSOR_HW_CFG *get_custom_power_cfg(void)
{
	int i=0, config_rev = -1;
	struct IMGSENSOR_HW_CFG *config = NULL;
	int rev = get_hw_board_version();
	for(i=0; NULL != imgsensor_custom_config_hw_list[i].p_imgsensor_custom_config_hw; ++i) {
		if(rev < imgsensor_custom_config_hw_list[i].revison)
			break;
		config_rev = imgsensor_custom_config_hw_list[i].revison;
		config = imgsensor_custom_config_hw_list[i].p_imgsensor_custom_config_hw;
	}

	pr_info("%s: selected power config revision=%d, config=%p", __func__, config_rev, config);
	return config;
}

struct IMGSENSOR_HW_POWER_SEQ *get_platform_pwr_seq(void)
{
	struct IMGSENSOR_HW_POWER_SEQ *config = NULL;
	switch (get_hw_board_version()) {
	case 0:
		config = platform_power_sequence;
		break;
	case 1: // falling through
	default:
		config = platform_power_sequence;
		break;
	}
	return config;
}

struct IMGSENSOR_HW_POWER_SEQ *get_sensor_pwr_seq(void)
{
	struct IMGSENSOR_HW_POWER_SEQ *config = NULL;
	switch (get_hw_board_version()) {
	case 0:
		config = sensor_power_sequence;
		break;
	case 1: // falling through
	default:
		config = sensor_power_sequence;
		break;
	}
	return config;
}

enum IMGSENSOR_RETURN imgsensor_hw_release_all(struct IMGSENSOR_HW *phw)
{
	int i;

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (phw->pdev[i]->release != NULL)
			(phw->pdev[i]->release)(phw->pdev[i]->pinstance);
	}
	return IMGSENSOR_RETURN_SUCCESS;
}
enum IMGSENSOR_RETURN imgsensor_hw_init(struct IMGSENSOR_HW *phw)
{
	struct IMGSENSOR_HW_SENSOR_POWER      *psensor_pwr;
	struct IMGSENSOR_HW_CFG               *pcust_pwr_cfg;
	struct IMGSENSOR_HW_CUSTOM_POWER_INFO *ppwr_info;
	int i, j;
	char str_prop_name[LENGTH_FOR_SNPRINTF];
	struct device_node *of_node
		= of_find_compatible_node(NULL, NULL, "mediatek,camera_hw");

	for (i = 0; i < IMGSENSOR_HW_ID_MAX_NUM; i++) {
		if (hw_open[i] != NULL)
			(hw_open[i])(&phw->pdev[i]);

		if (phw->pdev[i]->init != NULL)
			(phw->pdev[i]->init)(phw->pdev[i]->pinstance);
	}

	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		psensor_pwr = &phw->sensor_pwr[i];

		pcust_pwr_cfg = get_custom_power_cfg();
		while (pcust_pwr_cfg->sensor_idx != i &&
		       pcust_pwr_cfg->sensor_idx != IMGSENSOR_SENSOR_IDX_NONE)
			pcust_pwr_cfg++;

		if (pcust_pwr_cfg->sensor_idx == IMGSENSOR_SENSOR_IDX_NONE)
			continue;

		ppwr_info = pcust_pwr_cfg->pwr_info;
		while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE) {
			for (j = 0; j < IMGSENSOR_HW_ID_MAX_NUM; j++)
				if (ppwr_info->id == phw->pdev[j]->id)
					break;

			psensor_pwr->id[ppwr_info->pin] = j;
			ppwr_info++;
		}
	}

	for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
		memset(str_prop_name, 0, sizeof(str_prop_name));
		snprintf(str_prop_name,
					sizeof(str_prop_name),
					"cam%d_%s",
					i,
					"enable_sensor");
		if (of_property_read_string(
			of_node,
			str_prop_name,
			&phw->enable_sensor_by_index[i]) < 0) {
			pr_info("Property cust-sensor not defined\n");
			phw->enable_sensor_by_index[i] = NULL;
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN imgsensor_hw_power_sequence(
	struct IMGSENSOR_HW             *phw,
	enum   IMGSENSOR_SENSOR_IDX      sensor_idx,
	enum   IMGSENSOR_HW_POWER_STATUS pwr_status,
	struct IMGSENSOR_HW_POWER_SEQ   *ppower_sequence,
	char *pcurr_idx)
{
	struct IMGSENSOR_HW_SENSOR_POWER *psensor_pwr =
	    &phw->sensor_pwr[sensor_idx];

	struct IMGSENSOR_HW_POWER_SEQ    *ppwr_seq = ppower_sequence;
	struct IMGSENSOR_HW_POWER_INFO   *ppwr_info;
	struct IMGSENSOR_HW_DEVICE       *pdev;
	int                               pin_cnt = 0;

	while (ppwr_seq < ppower_sequence + IMGSENSOR_HW_SENSOR_MAX_NUM &&
		ppwr_seq->name != NULL) {
		if (!strcmp(ppwr_seq->name, PLATFORM_POWER_SEQ_NAME)) {
			if (sensor_idx == ppwr_seq->_idx)
				break;
		} else {
			if (!strcmp(ppwr_seq->name, pcurr_idx))
				break;
		}
		ppwr_seq++;
	}

	if (ppwr_seq->name == NULL)
		return IMGSENSOR_RETURN_ERROR;

	ppwr_info = ppwr_seq->pwr_info;

	while (ppwr_info->pin != IMGSENSOR_HW_PIN_NONE &&
		ppwr_info < ppwr_seq->pwr_info + IMGSENSOR_HW_POWER_INFO_MAX) {

		if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON &&
		   ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
			pdev = phw->pdev[psensor_pwr->id[ppwr_info->pin]];
		/*pr_debug(
		 *  "sensor_idx = %d, pin=%d, pin_state_on=%d, hw_id =%d\n",
		 *  sensor_idx,
		 *  ppwr_info->pin,
		 *  ppwr_info->pin_state_on,
		 * psensor_pwr->id[ppwr_info->pin]);
		 */

			if (pdev->set != NULL)
				pdev->set(
				    pdev->pinstance,
				    sensor_idx,
				    ppwr_info->pin,
				    ppwr_info->pin_state_on);

				if (ppwr_info->pin_on_delay)
					mDELAY(ppwr_info->pin_on_delay);
		}

		ppwr_info++;
		pin_cnt++;
	}

	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_OFF) {
		while (pin_cnt) {
			ppwr_info--;
			pin_cnt--;

			if (ppwr_info->pin != IMGSENSOR_HW_PIN_UNDEF) {
				pdev =
				    phw->pdev[psensor_pwr->id[ppwr_info->pin]];

				if (ppwr_info->pin_on_delay)
					mDELAY(ppwr_info->pin_on_delay);

				if (pdev->set != NULL)
					pdev->set(
					    pdev->pinstance,
					    sensor_idx,
					    ppwr_info->pin,
					    ppwr_info->pin_state_off);
			}
		}
	}

	/* wait for power stable */
/*
	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON)
		mdelay(5);
*/
	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_hw_power(
	struct IMGSENSOR_HW     *phw,
	struct IMGSENSOR_SENSOR *psensor,
	char *curr_sensor_name,
	enum IMGSENSOR_HW_POWER_STATUS pwr_status)
{
	enum IMGSENSOR_SENSOR_IDX sensor_idx = psensor->inst.sensor_idx;
	char str_index[LENGTH_FOR_SNPRINTF];

	pr_info(
		"HW rev: %d, sensor_idx %d, power %d curr_sensor_name %s, enable list %s\n",
		get_hw_board_version(),
		sensor_idx,
		pwr_status,
		curr_sensor_name,
		phw->enable_sensor_by_index[sensor_idx] == NULL
		? "NULL"
		: phw->enable_sensor_by_index[sensor_idx]);

	if (phw->enable_sensor_by_index[sensor_idx] &&
	!strstr(phw->enable_sensor_by_index[sensor_idx], curr_sensor_name))
		return IMGSENSOR_RETURN_ERROR;

	mutex_lock(&psensor->inst.i2c_cfg.pinst->lock);
	if (pwr_status && psensor->inst.i2c_cfg.pinst->pi2c_state_on) {
		psensor->inst.i2c_cfg.pinst->refcnt++;
		if (psensor->inst.i2c_cfg.pinst->refcnt == 1)
			pinctrl_select_state(
				psensor->inst.i2c_cfg.pinst->pi2c_pinctrl,
				psensor->inst.i2c_cfg.pinst->pi2c_state_on);
	} else if (!pwr_status && psensor->inst.i2c_cfg.pinst->pi2c_state_off) {
		psensor->inst.i2c_cfg.pinst->refcnt--;
		if (psensor->inst.i2c_cfg.pinst->refcnt == 0)
			pinctrl_select_state(
				psensor->inst.i2c_cfg.pinst->pi2c_pinctrl,
				psensor->inst.i2c_cfg.pinst->pi2c_state_off);
	}
	mutex_unlock(&psensor->inst.i2c_cfg.pinst->lock);

	snprintf(str_index, sizeof(str_index), "%d", sensor_idx);
	imgsensor_hw_power_sequence(
	    phw,
	    sensor_idx,
	    pwr_status,
	    get_platform_pwr_seq(),
	    str_index);

	imgsensor_hw_power_sequence(
	    phw,
	    sensor_idx,
	    pwr_status,
	    get_sensor_pwr_seq(),
	    curr_sensor_name);

	/* wait for power stable */
	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON)
		mDELAY(5);

	return IMGSENSOR_RETURN_SUCCESS;
}

