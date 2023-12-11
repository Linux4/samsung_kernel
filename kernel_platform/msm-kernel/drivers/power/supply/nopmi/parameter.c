/*****************************************************************************
* Copyright(c) BMT, 2021. All rights reserved.
*       
* BMT [oz8806] Source Code Reference Design
* File:[bmulib.c]
*       
* This Source Code Reference Design for BMT [oz8806] access 
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY, 
* and contains confidential and privileged information of BMT International 
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY, 
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any 
* modification or attempted modification of the Reference Design by any party, or 
* (ii) the combination, operation or use of the Reference Design with non-BMT 
* Reference Design. Use of the Reference Design is at user's discretion to qualify 
* the final work result.
*****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include "parameter.h"
#include "table.h"
static int32_t battery_model = BATTERY_MODEL_UNKNOWN;
extern int32_t oz8806_get_battery_model(void);

one_latitude_data_t			cell_temp_data[TEMPERATURE_DATA_NUM] = {
			{672,   115}, {758,   113}, {858,   105},
			{974,   100}, {1110,   95}, {1268,   90},
			{1452,   85}, {1669,   80}, {1925,   75},
			{2228,   70}, {2586,   65}, {3014,   60},
			{3535,   55}, {4161,   50}, {4917,   45},
			{5834,   40}, {6948,   35}, {8315,   30},
			{10000,  25}, {12081,  20}, {14674,  15},
			{17926,  10}, {22021,   5},	{27219,   0},
			{33892,  -5}, {42506, -10}, {53650, -15},
			{68237, -20},
};

config_data_t config_data = {
       	.fRsense = 			O2_CONFIG_RSENSE,
		.temp_pull_up 		= O2_TEMP_PULL_UP_R,
		.temp_ref_voltage = O2_TEMP_REF_VOLTAGE,
		.temp_parrel_res    = O2_TEMP_PARREL_R,
		.dbCARLSB =			5,
		.dbCurrLSB =    	781	,
		.fVoltLSB =	  		250,

		.design_capacity =			O2_CONFIG_CAPACITY,
		.charge_cv_voltage = 		OZ8806_VOLTAGE,
		.charge_end_current =		O2_CONFIG_EOC,
		.discharge_end_voltage =	OZ8806_EOD,
		.board_offset = 			O2_CONFIG_BOARD_OFFSET,
		.debug = 1, 
};

void bmt_print(const char *fmt, ...)
{
	if(config_data.debug)
	{
		va_list args;
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
	}
}
/*****************************************************************************
 * bmu_init_parameter:
 *		this implements a interface for customer initiating bmu parameters
 * Return: NULL
 *****************************************************************************/
void bmu_init_parameter(parameter_data_t *parameter_customer)
{
	battery_model = oz8806_get_battery_model();

	switch (battery_model) {
		case BATTERY_MODEL_MAIN:
			parameter_customer->ocv = ocv_data;
			parameter_customer->ocv_data_num = OCV_DATA_NUM;
			break;
		case BATTERY_MODEL_SECOND:
			parameter_customer->ocv = ocv_data_second;
			parameter_customer->ocv_data_num = OCV_DATA_NUM_SECOND;
			break;
		default:
			parameter_customer->ocv = ocv_data;
			parameter_customer->ocv_data_num = OCV_DATA_NUM;
			break;
	}

	parameter_customer->config = &config_data;
	parameter_customer->temperature = cell_temp_data;
	parameter_customer->ocv_data_num = OCV_DATA_NUM;
	parameter_customer->cell_temp_num = TEMPERATURE_DATA_NUM;
	parameter_customer->charge_pursue_step = 10;
 	parameter_customer->discharge_pursue_step = 6;
	parameter_customer->discharge_pursue_th = 10;
}
EXPORT_SYMBOL(bmu_init_parameter);

/*****************************************************************************
 * Description:
 *		bmu_init_gg
 * Return: NULL
 *****************************************************************************/
void bmu_init_gg(gas_gauge_t *gas_gauge)
{
	battery_model = oz8806_get_battery_model();

	switch (battery_model)
	{
		case BATTERY_MODEL_MAIN:
			gas_gauge->ri = 18;
			gas_gauge->rc_x_num = XAxis;
			gas_gauge->rc_y_num = YAxis;
			gas_gauge->rc_z_num = ZAxis;
			gas_gauge->fcc_data = config_data.design_capacity = BATTERY_DESIGN_CAPACITY_MAIN;
			config_data.charge_cv_voltage = BATTERY_CV_MAIN;
			bmt_dbg("battery_id is %s\n", battery_id[0]);
			break;

		case BATTERY_MODEL_SECOND:
			gas_gauge->ri = 18;
			gas_gauge->rc_x_num = XAxis_SECOND;
			gas_gauge->rc_y_num = YAxis_SECOND;
			gas_gauge->rc_z_num = ZAxis_SECOND;
			gas_gauge->fcc_data = config_data.design_capacity = BATTERY_DESIGN_CAPACITY_SECOND;
			config_data.charge_cv_voltage = BATTERY_CV_SECOND;
			bmt_dbg("battery_id_second is %s\n", battery_id_second[0]);
			break;

		default:
			gas_gauge->ri = 18;
			gas_gauge->rc_x_num = XAxis;
			gas_gauge->rc_y_num = YAxis;
			gas_gauge->rc_z_num = ZAxis;
			gas_gauge->fcc_data = config_data.design_capacity = BATTERY_DESIGN_CAPACITY_MAIN;
			config_data.charge_cv_voltage = BATTERY_CV_MAIN;
			bmt_dbg("battery_id_default is %s\n", battery_id[0]);
			break;
	}
//	gas_gauge->ri = 50;
	gas_gauge->line_impedance = 5;
	gas_gauge->power_on_100_vol = O2_OCV_100_VOLTAGE;

#if 0
	//add this for oinom
	gas_gauge->lower_capacity_reserve = 5;
	gas_gauge->lower_capacity_soc_start =30;
	//gas_gauge->percent_10_reserve = 66;
#endif
}
EXPORT_SYMBOL(bmu_init_gg);

void bmu_init_table(int32_t **x, int32_t **y, int32_t **z, int32_t **rc)
{
	battery_model = oz8806_get_battery_model();

	switch (battery_model)
	{
		case BATTERY_MODEL_MAIN:
			*x = (int32_t *)((uint8_t *)XAxisElement);
			*y = (int32_t *)((uint8_t *)YAxisElement);
			*z = (int32_t *)((uint8_t *)ZAxisElement);
			*rc = (int32_t *)((uint8_t *)RCtable);
			break;
		case BATTERY_MODEL_SECOND:
			*x = (int *)((uint8_t *)XAxisElement_second);
			*y = (int *)((uint8_t *)YAxisElement_second);
			*z = (int *)((uint8_t *)ZAxisElement_second);
			*rc = (int *)((uint8_t *)RCtable_second);
			break;
		default:
			*x = (int32_t *)((uint8_t *)XAxisElement);
			*y = (int32_t *)((uint8_t *)YAxisElement);
			*z = (int32_t *)((uint8_t *)ZAxisElement);
			*rc = (int32_t *)((uint8_t *)RCtable);
			break;
	}
}
EXPORT_SYMBOL(bmu_init_table);

const char * get_table_version(void)
{
	battery_model = oz8806_get_battery_model();
	switch (battery_model)
	{
		case BATTERY_MODEL_MAIN:
			return table_version;
		case BATTERY_MODEL_SECOND:
			return table_version_second;
		default:
			return table_version;
	}
}
EXPORT_SYMBOL(get_table_version);

MODULE_DESCRIPTION("FG Charger Driver");
MODULE_LICENSE("GPL v2");