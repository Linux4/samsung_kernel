// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "regulator.h"

//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
static bool regulator_status[IMGSENSOR_SENSOR_IDX_MAX_NUM][REGULATOR_TYPE_MAX_NUM] = {{false}};
static void check_for_regulator_get(struct REGULATOR *preg, struct device *pdevice, unsigned int sensor_index,unsigned int regulator_index);
static void check_for_regulator_put(struct REGULATOR *preg, unsigned int sensor_index, unsigned int regulator_index);
static struct device_node *of_node_record;
static struct IMGSENSOR_HW_DEVICE_COMMON *ghw_device_common;

static DEFINE_MUTEX(g_regulator_state_mutex);
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on

static const int regulator_voltage[] = {
	REGULATOR_VOLTAGE_0,
	REGULATOR_VOLTAGE_1000,
	//+w3 add 1.05V manager code
	REGULATOR_VOLTAGE_1050,
	//-w3 add 1.05V manager code
	REGULATOR_VOLTAGE_1100,
	REGULATOR_VOLTAGE_1200,
	REGULATOR_VOLTAGE_1210,
	REGULATOR_VOLTAGE_1220,
	REGULATOR_VOLTAGE_1500,
	REGULATOR_VOLTAGE_1800,
	REGULATOR_VOLTAGE_2500,
	REGULATOR_VOLTAGE_2800,
	REGULATOR_VOLTAGE_2900,
};

struct REGULATOR_CTRL regulator_control[REGULATOR_TYPE_MAX_NUM] = {
	{"vcama"},
	{"vcama1"},
	{"vcamaf"},
	{"vcamd"},
	{"vcamio"},
};

static struct REGULATOR reg_instance;

static enum IMGSENSOR_RETURN regulator_init(
	void *pinstance,
	struct IMGSENSOR_HW_DEVICE_COMMON *pcommon)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int type, idx, ret = 0;
	char str_regulator_name[LENGTH_FOR_SNPRINTF];

//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
	ghw_device_common = pcommon;
	of_node_record = pcommon->pplatform_device->dev.of_node;
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on

	for (idx = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		idx++) {
		for (type = 0;
			type < REGULATOR_TYPE_MAX_NUM;
			type++) {
			memset(str_regulator_name, 0,
				sizeof(str_regulator_name));
			ret = snprintf(str_regulator_name,
				sizeof(str_regulator_name),
				"cam%d_%s",
				idx,
				regulator_control[type].pregulator_type);
			if (ret < 0)
				PK_DBG("NOTICE: %s, snprintf err, %d\n",
					__func__, ret);

			preg->pregulator[idx][type] = regulator_get_optional(
					&pcommon->pplatform_device->dev,
					str_regulator_name);

			if (preg->pregulator[idx][type] == NULL ||
				IS_ERR(preg->pregulator[idx][type])) {
				PK_DBG("NOTICE: %s, regulator[%d][%d] err: %s\n",
					__func__,
					idx, type, str_regulator_name);
				preg->pregulator[idx][type] = NULL;
			}
			atomic_set(&preg->enable_cnt[idx][type], 0);
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_release(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int type, idx;
	struct regulator *pregulator = NULL;
	atomic_t *enable_cnt = NULL;

	for (idx = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		idx++) {

		for (type = 0; type < REGULATOR_TYPE_MAX_NUM; type++) {
			pregulator = preg->pregulator[idx][type];
			enable_cnt = &preg->enable_cnt[idx][type];
			if (pregulator != NULL) {
				for (; atomic_read(enable_cnt) > 0; ) {
					regulator_disable(pregulator);
					atomic_dec(enable_cnt);
				}
			}
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static enum IMGSENSOR_RETURN regulator_set(
	void *pinstance,
	enum IMGSENSOR_SENSOR_IDX   sensor_idx,
	enum IMGSENSOR_HW_PIN       pin,
	enum IMGSENSOR_HW_PIN_STATE pin_state)
{
	struct regulator     *pregulator;
	struct REGULATOR     *preg = (struct REGULATOR *)pinstance;
	int reg_type_offset;
	atomic_t             *enable_cnt;


	if (pin > IMGSENSOR_HW_PIN_DOVDD   ||
	    pin < IMGSENSOR_HW_PIN_AVDD    ||
	    pin_state < IMGSENSOR_HW_PIN_STATE_LEVEL_0 ||
	    pin_state >= IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH)
		return IMGSENSOR_RETURN_ERROR;

	reg_type_offset = REGULATOR_TYPE_VCAMA;

//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
	if (ghw_device_common)
	check_for_regulator_get(preg, &ghw_device_common->pplatform_device->dev, sensor_idx,(reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on

	pregulator = preg->pregulator[(unsigned int)sensor_idx][
		reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD];

	enable_cnt = &preg->enable_cnt[(unsigned int)sensor_idx][
		reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD];

	if (pregulator) {
		if (pin_state != IMGSENSOR_HW_PIN_STATE_LEVEL_0) {
			if (regulator_set_voltage(pregulator,
				regulator_voltage[
				pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0],
				regulator_voltage[
				pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0])) {

				PK_DBG(
				  "[regulator]fail to regulator_set_voltage, powertype:%d powerId:%d\n",
				  pin,
				  regulator_voltage[
				  pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
			}
			if (regulator_enable(pregulator)) {
				PK_DBG(
				"[regulator]fail to regulator_enable, powertype:%d powerId:%d\n",
				pin,
				regulator_voltage[
				  pin_state - IMGSENSOR_HW_PIN_STATE_LEVEL_0]);
//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
				check_for_regulator_put(preg, sensor_idx,(reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
				return IMGSENSOR_RETURN_ERROR;
			}
			atomic_inc(enable_cnt);
		} else {
			if (regulator_is_enabled(pregulator))
				PK_DBG("[regulator]%d is enabled\n", pin);

			if (regulator_disable(pregulator)) {
				PK_DBG(
					"[regulator]fail to regulator_disable, powertype: %d\n",
					pin);
//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
				check_for_regulator_put(preg, sensor_idx,(reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
				return IMGSENSOR_RETURN_ERROR;
			}
//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
			check_for_regulator_put(preg, sensor_idx,(reg_type_offset + pin - IMGSENSOR_HW_PIN_AVDD));
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
			atomic_dec(enable_cnt);
		}
	} else {
		PK_DBG("regulator == NULL %d %d %d\n",
				reg_type_offset,
				pin,
				IMGSENSOR_HW_PIN_AVDD);
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

//+S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on
static void check_for_regulator_get(struct REGULATOR *preg, struct device *pdevice, unsigned int sensor_index, unsigned int regulator_index)
{
	struct device_node *pof_node = NULL;
	char str_regulator_name[LENGTH_FOR_SNPRINTF];

	if (!preg || !pdevice) {
		pr_err("Fatal: Null ptr.preg:%pK,pdevice:%pK\n", preg, pdevice);
	return;
	}

	if (sensor_index >= IMGSENSOR_SENSOR_IDX_MAX_NUM ||
	regulator_index >= REGULATOR_TYPE_MAX_NUM ) {
		pr_err("[%s]Invalid sensor_idx:%d regulator_idx: %d\n",
			__func__, sensor_index, regulator_index);
	return;
	}

	mutex_lock(&g_regulator_state_mutex);

	if (regulator_status[sensor_index][regulator_index] == false) {
		pof_node = pdevice->of_node;
		pdevice->of_node = of_node_record;

		snprintf(str_regulator_name,sizeof(str_regulator_name),"cam%d_%s",sensor_index,regulator_control[regulator_index].pregulator_type);
		preg->pregulator[sensor_index][regulator_index] =regulator_get(pdevice, str_regulator_name);

		if (preg != NULL)
			regulator_status[sensor_index][regulator_index] = true;
		else
			pr_err("get regulator failed.\n");
		pdevice->of_node = pof_node;
	}

	mutex_unlock(&g_regulator_state_mutex);

	return;
}

static void check_for_regulator_put(struct REGULATOR *preg,
unsigned int sensor_index, unsigned int regulator_index)
{
	if (!preg) {
		pr_err("Fatal: Null ptr.\n");
	return;
	}

	if (sensor_index >= IMGSENSOR_SENSOR_IDX_MAX_NUM ||
	regulator_index >= REGULATOR_TYPE_MAX_NUM ) {
		pr_err("[%s]Invalid sensor_idx:%d regulator_idx: %d\n",__func__, sensor_index, regulator_index);
	return;
}

	mutex_lock(&g_regulator_state_mutex);

	if (regulator_status[sensor_index][regulator_index] == true) {
	regulator_put(preg->pregulator[sensor_index][regulator_index]);
		preg->pregulator[sensor_index][regulator_index] = NULL;
		regulator_status[sensor_index][regulator_index] = false;
	}

	mutex_unlock(&g_regulator_state_mutex);

	return;
}
//-S98901AA1-12465 wusikai.wt, ADD, 2024/08/19, modify regulator power on

static enum IMGSENSOR_RETURN regulator_dump(void *pinstance)
{
	struct REGULATOR *preg = (struct REGULATOR *)pinstance;
	int i, j;
	int enable = 0;

	for (j = IMGSENSOR_SENSOR_IDX_MIN_NUM;
		j < IMGSENSOR_SENSOR_IDX_MAX_NUM;
		j++) {

		for (i = REGULATOR_TYPE_VCAMA;
		i < REGULATOR_TYPE_MAX_NUM;
		i++) {
			if (!preg->pregulator[j][i])
				continue;

			if (regulator_is_enabled(preg->pregulator[j][i]) &&
				atomic_read(&preg->enable_cnt[j][i]) != 0)
				enable = 1;
			else
				enable = 0;

			PK_DBG("[sensor_dump][regulator] index= %d, %s = %d, enable = %d\n",
				j,
				regulator_control[i].pregulator_type,
				regulator_get_voltage(preg->pregulator[j][i]),
				enable);
		}
	}
	return IMGSENSOR_RETURN_SUCCESS;
}

static struct IMGSENSOR_HW_DEVICE device = {
	.id        = IMGSENSOR_HW_ID_REGULATOR,
	.pinstance = (void *)&reg_instance,
	.init      = regulator_init,
	.set       = regulator_set,
	.release   = regulator_release,
	.dump      = regulator_dump
};

enum IMGSENSOR_RETURN imgsensor_hw_regulator_open(
	struct IMGSENSOR_HW_DEVICE **pdevice)
{
	*pdevice = &device;
	return IMGSENSOR_RETURN_SUCCESS;
}

