/*
 *  adc_fuelgauge.c
 *  Samsung ADC Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/battery/sec_fuelgauge.h>

static int adc_write_reg(struct sec_fuelgauge_info *fuelgauge, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(fuelgauge->client, reg, value);

	if (ret < 0)
		dev_err(&fuelgauge->client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int adc_read_reg(struct sec_fuelgauge_info *fuelgauge, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(fuelgauge->client, reg);

	if (ret < 0)
		dev_err(&fuelgauge->client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int adc_read_word(struct sec_fuelgauge_info *fuelgauge, int reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(fuelgauge->client, reg);
	if (ret < 0)
		dev_err(&fuelgauge->client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}


/* 
 * Name : chk_lut
 *
 */
static int chk_lut (u16* x, u16* y, u16 v, u16 l) {
	int i;
	//u32 ret;
	int ret;

	if (v < x[0])
		ret = y[0];
	else if (v >= x[l-1])
		ret = y[l-1]; 
	else {          
		for (i = 1; i < l; i++) {          
			if (v < x[i])               
				break;      
		}       
		ret = y[i-1];       
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);   
	}   
	//return (u16) ret;
	return ret;
}

/* 
 * Name : chk_lut_temp
 * return : The return value is Kelvin degree
 */
static int chk_lut_temp (u16* x, u16* y, u16 v, u16 l) {
	int i, ret;

	if (v >= x[0])
		ret = y[0];
	else if (v < x[l-1])
		ret = y[l-1]; 
	else {			
		for (i=1; i < l; i++) { 		 
			if (v > x[i])				
				break;		
		}		
		ret = y[i-1];		
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);	
	}
	
	return ret;
}

/* 
 * Name : adc_to_degree
 *
 */
int adc_to_degree_k(u16 adc) {
    return (chk_lut_temp(adc2temp_lut.adc, adc2temp_lut.temp, adc, temp_lut_length));
}

int degree_k2c(u16 k) {
	return (K2C(k));
}

/* 
 * Name : get_adc_offset
 *
 */
int get_adc_offset(u16 adc) {	
    return (chk_lut(adc2vbat_lut.adc, adc2vbat_lut.offset, adc, adc2vbat_lut_length));
}

/* 
 * Name : adc_to_vbat
 *
 */
u16 adc_to_vbat(u16 adc, u8 is_charging) {
	u16 a = adc;

	if(is_charging)
		a = adc - get_adc_offset(adc); // deduct charging offset
	return (2500 + ((a * 2000) >> 12));
}

/* 
 * Name : do_interpolation
 */
int do_interpolation(int x0, int x1, int y0, int y1, int x)
{
	int y = 0;
	
	if(!(x1 - x0 )) {
		pr_err("%s. Divied by Zero. Plz check x1(%d), x0(%d) value \n",
				__func__, x1, x0);
		return 0;
	}

	y = y0 + (x - x0)*(y1 - y0)/(x1 - x0);
	pr_debug("# %s. X_0 = %d, X_VALUE = %d, X_1 = %d, Y_0 = %d, Y_1 = %d\n",
					__func__, x0, x, x1, y0, y1);
	pr_debug("%s. Interpolated y_value is = %d\n", __func__, y);

	return y;
}

/*
 * Name : adc_to_soc_with_temp_compensat
 *
 */
u32 adc_to_soc_with_temp_compensat(u16 adc, u16 temp) {
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_ht, adc2soc_lut.soc, adc, adc2soc_lut_length);    
		sl = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
			/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc, adc, adc2soc_lut_length);		
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
			/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);	
	} else {		
		sh = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc, adc, adc2soc_lut_length);		 
		sl = chk_lut(adc2soc_lut.adc_llt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
	}

	return sh;
}

/*
 * Name : soc_to_adc_with_temp_compensat
 *
 */
u32 soc_to_adc_with_temp_compensat(u16 soc, u16 temp) {
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	pr_debug("%s. Parameter. SOC = %d. temp = %d\n", __func__, soc, temp);

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_ht, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
			/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
			/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
	} else {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_llt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
			/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE);
	}

	return sh;
}

/*
 * Name : d2199_get_weight_from_lookup
 */
static u16 d2199_get_weight_from_lookup(u16 tempk,
										u16 average_adc,
										u8 is_charging,
										int diff)
{
	u8 i = 0;
	u16 *plut = NULL;
	int diff_offset, weight, weight_1c, weight_2c = 0;

	// Sanity check.
	if (tempk < BAT_LOW_LOW_TEMPERATURE)
		tempk = BAT_LOW_LOW_TEMPERATURE;
	else if (tempk > BAT_HIGH_TEMPERATURE)
		tempk = BAT_HIGH_TEMPERATURE;

	// Get the SOC look-up table
	if(tempk >= BAT_HIGH_TEMPERATURE) {
		plut = &adc2soc_lut.adc_ht[0];
	} else if(tempk < BAT_HIGH_TEMPERATURE && tempk >= BAT_ROOM_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rt[0];
	} else if(tempk < BAT_ROOM_TEMPERATURE && tempk >= BAT_ROOM_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rlt[0];
	} else if (tempk < BAT_ROOM_LOW_TEMPERATURE && tempk >= BAT_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lt[0];
	} else if(tempk < BAT_LOW_TEMPERATURE && tempk >= BAT_LOW_MID_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lmt[0];
	} else 
		plut = &adc2soc_lut.adc_llt[0];

	for(i = adc2soc_lut_length - 1; i; i--) {
		if(plut[i] <= average_adc)
			break;
	}

#ifdef CONFIG_D2199_MULTI_WEIGHT
	// 0.1C, 0.2C
	if(i <= (MULTI_WEIGHT_SIZE-1) && is_charging == 0) {
		if(dischg_diff_tbl.c1_diff[i] > diff)
			diff_offset = dischg_diff_tbl.c1_diff[i];
		else if(dischg_diff_tbl.c2_diff[i] < diff)
			diff_offset = dischg_diff_tbl.c2_diff[i];
		else
			diff_offset = diff;
		pr_debug("%s. diff = %d, diff_offset = %d\n", __func__, diff, diff_offset);
	} else {
		pr_debug("%s. diff = %d\n", __func__, diff);
	}
#endif /* CONFIG_D2199_MULTI_WEIGHT */

	if((tempk <= BAT_HIGH_TEMPERATURE) && (tempk > BAT_ROOM_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) {
				// under 1% -> fast charging
				weight = adc_weight_section_charge[0];
			} else
				weight = adc_weight_section_charge[i];
		} else {
#ifdef CONFIG_D2199_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight = adc_weight_section_discharge_1c[i] -
							((diff_offset - dischg_diff_tbl.c1_diff[i])
							 * (adc_weight_section_discharge_1c[i] -
							 adc_weight_section_discharge_2c[i]) /
							 (dischg_diff_tbl.c2_diff[i] -
							 dischg_diff_tbl.c1_diff[i]));
				//cond = 0;
			} else 
#endif /* CONFIG_D2199_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_1c[i];
				//cond = 1;
			}
		}
	} else if((tempk <= BAT_ROOM_TEMPERATURE) && (tempk > BAT_ROOM_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_rlt[i];
			weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
				*(adc_weight_section_charge[i]-adc_weight_section_charge_rlt[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
		} else {
#ifdef CONFIG_D2199_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);

			} else 
#endif /* CONFIG_D2199_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_rlt_1c[i];
				weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
					*(adc_weight_section_discharge_1c[i]-adc_weight_section_discharge_rlt_1c[i]))
								/(BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
				pr_debug("%s(line.%d). weight = %d\n", __func__, __LINE__, weight);
			}
		}
	} else if((tempk <= BAT_ROOM_LOW_TEMPERATURE) && (tempk > BAT_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_lt[i];
			weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
				*(adc_weight_section_charge_rlt[i]-adc_weight_section_charge_lt[i]))
								/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
		} else {
#ifdef CONFIG_D2199_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);

			} else 
#endif /* CONFIG_D2199_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_lt_1c[i];
				weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
					*(adc_weight_section_discharge_rlt_1c[i]-adc_weight_section_discharge_lt_1c[i]))
									/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
				pr_debug("%s(line.%d). weight = %d\n", __func__, __LINE__, weight);
			}
		}
	} else if((tempk <= BAT_LOW_TEMPERATURE) && (tempk > BAT_LOW_MID_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_lmt[i];
			weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
				*(adc_weight_section_charge_lt[i]-adc_weight_section_charge_lmt[i]))
								/(BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
		} else {
#ifdef CONFIG_D2199_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);
			} else 
#endif /* CONFIG_D2199_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_lmt_1c[i];
				weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
					*(adc_weight_section_discharge_lt_1c[i]-adc_weight_section_discharge_lmt_1c[i]))
									/(BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
				pr_debug("%s(line.%d). weight = %d\n", __func__, __LINE__, weight);
			}
		}
	} else {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_llt[i];
			weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
				*(adc_weight_section_charge_lmt[i]-adc_weight_section_charge_llt[i]))
								/(BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE);
		} else {
#ifdef CONFIG_D2199_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);
			} else 
#endif /* CONFIG_D2199_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_llt_1c[i];
				weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
					*(adc_weight_section_discharge_lmt_1c[i]-adc_weight_section_discharge_llt_1c[i]))
									/(BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE);
				pr_debug("%s(line.%d). weight = %d\n", __func__, __LINE__, weight);
			}
		}
	}

	if(weight < 0)
		weight = 0;

	return weight;
}

/*
 * Name : d2199_get_calibration_offset
 */
static int d2199_get_calibration_offset(int voltage, int y1, int y0)
{
	int x1 = D2199_CAL_HIGH_VOLT, x0 = D2199_CAL_LOW_VOLT;
	int x = voltage, y = 0;

	y = y0 + ((x-x0)*y1 - (x-x0)*y0) / (x1-x0);

	return y;
}

#if defined(CONFIG_BACKLIGHT_KTD253) || \
	defined(CONFIG_BACKLIGHT_KTD3102)
extern u8 ktd_backlight_is_dimming(void);
#else
#define LCD_DIM_ADC			1895
#endif
/* 
 * Name : d2153_reset_sw_fuelgauge
 */
static int d2199_reset_sw_fuelgauge(struct sec_fuelgauge_info *fuelgauge)
{
	u8 i, j = 0;
	int read_adc = 0;
	u32 average_adc, sum_read_adc = 0;

	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}

	pr_info("++++++ Reset Software Fuelgauge +++++++++++\n");
	fuelgauge->info.volt_adc_init_done = FALSE;

	/* Initialize ADC buffer */
	memset(fuelgauge->info.voltage_adc, 0x0, ARRAY_SIZE(fuelgauge->info.voltage_adc));
	fuelgauge->info.sum_voltage_adc = 0;
	fuelgauge->info.soc = 0;
	fuelgauge->info.prev_soc = 0;

	/* Read VBAT_S ADC */
	for(i = 8, j = 0; i; i--) {
		read_adc = fuelgauge->info.d2199_read_adc(fuelgauge, D2199_ADC_VOLTAGE);
		if(fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].is_adc_eoc) {
			read_adc = fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].read_adc;
			if(read_adc > 0) {
				sum_read_adc += read_adc;
				j++;
			}
		}
		msleep(10);
	}
	average_adc = read_adc = sum_read_adc / j;
	pr_info("%s. average = %d, j = %d \n", __func__, average_adc, j);

	/* To be compensated a read ADC */
#if defined(CONFIG_BACKLIGHT_KTD253) || \
	defined(CONFIG_BACKLIGHT_KTD3102)
	if(ktd_backlight_is_dimming())
		average_adc += fg_reset_drop_offset[1];
	else
		average_adc += fg_reset_drop_offset[0];
#else
	if(average_adc >= LCD_DIM_ADC )
		average_adc += fg_reset_drop_offset[0];
	else
		average_adc += fg_reset_drop_offset[1];
#endif /* CONFIG_BACKLIGHT_KTD253 ||  CONFIG_BACKLIGHT_KTD3102 */

	if(average_adc > MAX_FULL_CHARGED_ADC) {
		average_adc = MAX_FULL_CHARGED_ADC;
		pr_info("Set average_adc to max. ADC value = %d \n", average_adc);
	}

	pr_info("%s. average = %d\n", __func__, average_adc);

	fuelgauge->info.origin_volt_adc = read_adc;
	fuelgauge->info.average_volt_adc = average_adc;
	fuelgauge->info.current_voltage = adc_to_vbat(fuelgauge->info.current_volt_adc,
						      fuelgauge->is_charging);
	fuelgauge->info.average_voltage = adc_to_vbat(fuelgauge->info.average_volt_adc,
						      fuelgauge->is_charging);
	fuelgauge->info.volt_adc_init_done = TRUE;

	pr_info("%s. Average. ADC = %d, Voltage =  %d\n",
		__func__, fuelgauge->info.average_volt_adc,
		fuelgauge->info.average_voltage);

	return 0;
}

/*
 * Name : d2199_get_soc
 */
static int d2199_get_soc(struct sec_fuelgauge_info *fuelgauge)
{
	int soc;

	if((!fuelgauge) || (!fuelgauge->info.volt_adc_init_done)) {
		pr_err("%s. Invalid parameter. \n", __func__);
		return -EINVAL;
	}

	if(fuelgauge->info.soc)
		fuelgauge->info.prev_soc = fuelgauge->info.soc;

	soc = adc_to_soc_with_temp_compensat(fuelgauge->info.average_volt_adc,
					     C2K(fuelgauge->info.average_temperature));

	if(soc >= FULL_CAPACITY) {
		soc = FULL_CAPACITY;
		if(fuelgauge->info.virtual_battery_full == 1) {
			fuelgauge->info.virtual_battery_full = 0;
			fuelgauge->info.soc = FULL_CAPACITY;
		}
	}

	/* Don't allow soc goes up when battery is dicharged.
	 and also don't allow soc goes down when battey is charged. */
	if(fuelgauge->is_charging != TRUE
	   && (soc > fuelgauge->info.prev_soc)
	   && fuelgauge->info.prev_soc) {
		soc = fuelgauge->info.prev_soc;
	}
	else if(fuelgauge->is_charging
		&& (soc < fuelgauge->info.prev_soc)
		&& fuelgauge->info.prev_soc) {
		soc = fuelgauge->info.prev_soc;
	}
	fuelgauge->info.soc = soc;

	adc_write_reg(fuelgauge, D2199_GP_ID_2_REG, (0xFF & soc));
	adc_write_reg(fuelgauge, D2199_GP_ID_3_REG, (0x0F & (soc>>8)));

	adc_write_reg(fuelgauge, D2199_GP_ID_4_REG,
		      (0xFF & fuelgauge->info.average_volt_adc));
	adc_write_reg(fuelgauge, D2199_GP_ID_5_REG,
		      (0xF & (fuelgauge->info.average_volt_adc>>8)));

	return soc;
}

static int d2199_read_voltage(struct sec_fuelgauge_info *fuelgauge)
{
	int new_vol_adc, base_weight, new_vol_orign = 0;
	int offset_with_new = 0;
	int ret = 0;
	//static int calOffset_4P2, calOffset_3P4 = 0;
	int num_multi = 0;
	int orign_offset = 0;
	u8  ta_status = 0;

	if(fuelgauge->info.volt_adc_init_done == FALSE ) {
		ta_status = adc_read_reg(fuelgauge, D2199_STATUS_C_REG);
		pr_debug("%s. STATUS_C register = 0x%X\n", __func__, ta_status);
		if(ta_status & D2199_GPI_3_TA_MASK)
			fuelgauge->is_charging = 1;
		else
			fuelgauge->is_charging = 0;
	}
	pr_debug("##### is_charging mode = %d\n", fuelgauge->is_charging);

	// Read voltage ADC
	ret = fuelgauge->info.d2199_read_adc(fuelgauge, D2199_ADC_VOLTAGE);

	pr_debug("[%s]READ_EOC = %d\n", __func__,
		fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].is_adc_eoc);
	pr_debug("[%s]READ_ADC = %d\n", __func__,
		fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].read_adc);

	if(ret < 0)
		return ret;

	if(fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].is_adc_eoc) {
		int offset = 0;

		new_vol_orign = new_vol_adc = fuelgauge->info.adc_res[D2199_ADC_VOLTAGE].read_adc;

		if(fuelgauge->info.volt_adc_init_done) {
			// Initialization is done

			if(fuelgauge->is_charging) {
				// In case of charge.
				orign_offset = new_vol_adc - fuelgauge->info.average_volt_adc;
				base_weight = d2199_get_weight_from_lookup(
									C2K(fuelgauge->info.average_temperature),
									fuelgauge->info.average_volt_adc,
									fuelgauge->is_charging,
									orign_offset);
				offset_with_new = orign_offset * base_weight;

				fuelgauge->info.sum_total_adc += offset_with_new;
				num_multi = fuelgauge->info.sum_total_adc / NORM_CHG_NUM;
				if(num_multi) {
					fuelgauge->info.average_volt_adc += num_multi;
					fuelgauge->info.sum_total_adc = fuelgauge->info.sum_total_adc
																% NORM_CHG_NUM;
				} else {
					new_vol_adc = fuelgauge->info.average_volt_adc;
				}

				fuelgauge->info.current_volt_adc = new_vol_adc;
			} else {
				// In case of discharging.
				orign_offset = fuelgauge->info.average_volt_adc - new_vol_adc;
				base_weight = d2199_get_weight_from_lookup(
									C2K(fuelgauge->info.average_temperature),
									fuelgauge->info.average_volt_adc,
									fuelgauge->is_charging,
									orign_offset);
				offset_with_new = orign_offset * base_weight;

				if(fuelgauge->info.reset_total_adc) {
					fuelgauge->info.sum_total_adc =
											fuelgauge->info.sum_total_adc / 10;
					fuelgauge->info.reset_total_adc = 0;
					pr_debug("%s. sum_toal_adc was divided by 10\n", __func__);
				}

				fuelgauge->info.sum_total_adc += offset_with_new;
				pr_debug("Discharging. Recalculated base_weight = %d\n", base_weight);

				num_multi = fuelgauge->info.sum_total_adc / NORM_NUM;
				if(num_multi) {
					fuelgauge->info.average_volt_adc -= num_multi;
					fuelgauge->info.sum_total_adc =
										fuelgauge->info.sum_total_adc % NORM_NUM;
				} else {
					new_vol_adc = fuelgauge->info.average_volt_adc;
				}

				if(is_called_by_ticker == 0) {
					fuelgauge->info.current_volt_adc = new_vol_adc;
				} else {
					fuelgauge->info.current_volt_adc = new_vol_adc;
					is_called_by_ticker=0;
				}
			}
		} else {
			// Before initialization
			u8 i = 0;
			u8 res_msb, res_lsb, res_msb_adc, res_lsb_adc = 0;
			u32 capacity = 0, vbat_adc = 0;
			int convert_vbat_adc, X1, X0;
			int Y1, Y0 = FIRST_VOLTAGE_DROP_ADC;
			int X = C2K(fuelgauge->info.average_temperature);

			// If there is SOC data in the register
			// the SOC(capacity of battery) will be used as initial SOC
			res_lsb = adc_read_reg(fuelgauge, D2199_GP_ID_2_REG);
			res_msb = adc_read_reg(fuelgauge, D2199_GP_ID_3_REG);
			capacity = (((res_msb & 0x0F) << 8) | (res_lsb & 0xFF));

			res_lsb_adc = adc_read_reg(fuelgauge, D2199_GP_ID_4_REG);
			res_msb_adc = adc_read_reg(fuelgauge, D2199_GP_ID_5_REG);
			vbat_adc = (((res_msb_adc & 0x0F) << 8) | (res_lsb_adc & 0xFF));

			pr_debug("%s. capacity = %d, vbat_adc = %d \n", __func__,
															capacity,
															vbat_adc);

			if(capacity) {
					convert_vbat_adc = vbat_adc;
				pr_info("!#!#!# Boot BY Normal Power Off !#!#!# \n");
			} else {
				// Initial ADC will be decided in here.
				pr_info("!#!#!# Boot BY Battery insert !#!#!# \n");

			fuelgauge->info.pd2199->average_vbat_init_adc =
								(fuelgauge->info.pd2199->vbat_init_adc[0] +
								 fuelgauge->info.pd2199->vbat_init_adc[1] +
								 fuelgauge->info.pd2199->vbat_init_adc[2]) / 3;

				vbat_adc =	fuelgauge->info.pd2199->average_vbat_init_adc;
				pr_debug("%s (L_%d). vbat_init_adc = %d, new_vol_orign = %d\n",
								__func__, __LINE__,
								fuelgauge->info.pd2199->average_vbat_init_adc,
								new_vol_orign);

			if(fuelgauge->is_charging) {
					if(vbat_adc < CHARGE_ADC_KRNL_F) {
						// In this case, vbat_adc is bigger than OCV
						// So, subtract a interpolated value
						// from initial average value(vbat_adc)
						u16 temp_adc = 0;

						if(vbat_adc < CHARGE_ADC_KRNL_H)
							vbat_adc = CHARGE_ADC_KRNL_H;

						X0 = CHARGE_ADC_KRNL_H;    X1 = CHARGE_ADC_KRNL_L;
						Y0 = CHARGE_OFFSET_KRNL_H; Y1 = CHARGE_OFFSET_KRNL_L;
						temp_adc = do_interpolation(X0, X1, Y0, Y1, vbat_adc);
						convert_vbat_adc = vbat_adc - temp_adc;
				} else {
						convert_vbat_adc = new_vol_orign + CHARGE_OFFSET_KRNL2;
				}
			} else {
					vbat_adc = new_vol_orign;
				pr_debug("[L%d] %s discharging new_vol_adc = %d\n",
						__LINE__, __func__, vbat_adc);

				Y0 = FIRST_VOLTAGE_DROP_ADC;
				if(C2K(fuelgauge->info.average_temperature)
											<= BAT_LOW_LOW_TEMPERATURE) {
						convert_vbat_adc = vbat_adc
											+ (Y0 + FIRST_VOLTAGE_DROP_LL_ADC);
				} else if(C2K(fuelgauge->info.average_temperature)
											>= BAT_ROOM_TEMPERATURE) {
						convert_vbat_adc = vbat_adc + Y0;
				} else {
					if(C2K(fuelgauge->info.average_temperature)
											<= BAT_LOW_MID_TEMPERATURE) {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_ADC;
							Y0 = Y0 + FIRST_VOLTAGE_DROP_LL_ADC;
						X0 = BAT_LOW_LOW_TEMPERATURE;
						X1 = BAT_LOW_MID_TEMPERATURE;
					} else if(C2K(fuelgauge->info.average_temperature)
											<= BAT_LOW_TEMPERATURE) {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_L_ADC;
							Y0 = Y0 + FIRST_VOLTAGE_DROP_ADC;
						X0 = BAT_LOW_MID_TEMPERATURE;
						X1 = BAT_LOW_TEMPERATURE;
					} else {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_RL_ADC;
							Y0 = Y0 + FIRST_VOLTAGE_DROP_L_ADC;
						X0 = BAT_LOW_TEMPERATURE;
						X1 = BAT_ROOM_LOW_TEMPERATURE;
					}
						convert_vbat_adc = vbat_adc + Y0
										+ ((X - X0) * (Y1 - Y0)) / (X1 - X0);
				}
			}
			}

				new_vol_adc = convert_vbat_adc;
			pr_info("%s. # Calculated initial new_vol_adc = %d\n", __func__, new_vol_adc);

			if(new_vol_adc > MAX_FULL_CHARGED_ADC) {
				new_vol_adc = MAX_FULL_CHARGED_ADC;
				pr_debug("%s. Set new_vol_adc to max. ADC value\n", __func__);
			}

			for(i = AVG_SIZE; i ; i--) {
				fuelgauge->info.voltage_adc[i-1] = new_vol_adc;
				fuelgauge->info.sum_voltage_adc += new_vol_adc;
			}

			fuelgauge->info.current_volt_adc = new_vol_adc;
			fuelgauge->info.volt_adc_init_done = TRUE;
			fuelgauge->info.average_volt_adc = new_vol_adc;

		}

		fuelgauge->info.origin_volt_adc = new_vol_orign;
		fuelgauge->info.current_voltage = adc_to_vbat(fuelgauge->info.current_volt_adc,
							      fuelgauge->is_charging);
		fuelgauge->info.average_voltage = adc_to_vbat(fuelgauge->info.average_volt_adc,
							      fuelgauge->is_charging);
	}else {
		pr_err("%s. Voltage ADC read failure \n", __func__);
		ret = -EIO;
	}

	pr_debug("[%s]current volt_adc = %d, average_volt_adc = %d\n",
		__func__,
		fuelgauge->info.current_volt_adc,
		fuelgauge->info.average_volt_adc);

	pr_debug("[%s]current voltage = %d, average_voltage = %d\n",
		__func__,
		fuelgauge->info.current_voltage,
		fuelgauge->info.average_voltage);

	return ret;
}

/*
 * Name : d2199_read_temperature
 */
static int d2199_read_temperature(struct sec_fuelgauge_info *fuelgauge)
{
	u16 new_temp_adc = 0;
	int ret = 0;

	/* Read temperature ADC
	 * Channel : D2199_ADC_TEMPERATURE_1 -> TEMP_BOARD
	 * Channel : D2199_ADC_TEMPERATURE_2 -> TEMP_RF
	 */

	// To read a temperature ADC of BOARD
	ret = fuelgauge->info.d2199_read_adc(fuelgauge, D2199_ADC_TEMPERATURE_1);
	if(fuelgauge->info.adc_res[D2199_ADC_TEMPERATURE_1].is_adc_eoc) {
		new_temp_adc = fuelgauge->info.adc_res[D2199_ADC_TEMPERATURE_1].read_adc;

		fuelgauge->info.current_temp_adc = new_temp_adc;

		if(fuelgauge->info.temp_adc_init_done) {
			fuelgauge->info.sum_temperature_adc += new_temp_adc;
			fuelgauge->info.sum_temperature_adc -= 
				fuelgauge->info.temperature_adc[fuelgauge->info.temperature_idx];
			fuelgauge->info.temperature_adc[fuelgauge->info.temperature_idx] = new_temp_adc;
		} else {
			u8 i;

			for(i = 0; i < AVG_SIZE; i++) {
				fuelgauge->info.temperature_adc[i] = new_temp_adc;
				fuelgauge->info.sum_temperature_adc += new_temp_adc;
			}
			fuelgauge->info.temp_adc_init_done = TRUE;
		}

		fuelgauge->info.average_temp_adc =
			fuelgauge->info.sum_temperature_adc >> AVG_SHIFT;
		fuelgauge->info.temperature_idx = (fuelgauge->info.temperature_idx+1) % AVG_SIZE;
		fuelgauge->info.average_temperature = 
			degree_k2c(adc_to_degree_k(fuelgauge->info.average_temp_adc));
		fuelgauge->info.current_temperature = 
			degree_k2c(adc_to_degree_k(new_temp_adc)); 

	}
	else {
		pr_err("%s. Temperature ADC read failed \n", __func__);
		ret = -EIO;
	}

	return ret;
}

/* 
 * Name : d2199_read_temperature_adc
 */
int d2199_read_temperature_adc(int *tbat, int channel)
{
	u8 i, j;
	int sum_temp_adc, ret = 0;
	struct sec_fuelgauge_info *fuelgauge = gbat;

	if(fuelgauge == NULL) {
		pr_err("%s. battery_data is NULL\n", __func__);
		return -EINVAL;
	}

	/* To read a temperature2 ADC */
	sum_temp_adc = 0;
	for(i = 5, j = 0; i; i--) {
		ret = fuelgauge->info.d2199_read_adc(fuelgauge, channel);
		if(ret == 0) {
			sum_temp_adc += fuelgauge->info.adc_res[channel].read_adc;
			if(++j == 2)
				break;
		} else
			msleep(20);
	}
	if (j) {
		*tbat = (sum_temp_adc / j);
		pr_info("%s. Channel(%d) ADC = %d\n", __func__, channel, *tbat);
		return 0;
	} else {
		pr_err("%s. Error in reading channel(%d) temperature.\n", __func__, channel);
		return -EIO;
	}

	return 30;
}
EXPORT_SYMBOL(d2199_read_temperature_adc);

#if 0
static int adc_get_avg_vcell(struct sec_fuelgauge_info *fuelgauge)
{
	int voltage_avg;

	voltage_avg = (int)adc_calculate_average(fuelgauge,
		ADC_CHANNEL_VOLTAGE_AVG, fuelgauge->info.voltage_now);

	return voltage_avg;
}

static int adc_get_ocv(struct sec_fuelgauge_info *fuelgauge)
{
	int voltage_ocv;

	voltage_ocv = (int)adc_calculate_average(fuelgauge,
		ADC_CHANNEL_VOLTAGE_OCV, fuelgauge->info.voltage_avg);

	return voltage_ocv;
}

/* soc should be 0.01% unit */
static int adc_get_soc(struct sec_fuelgauge_info *fuelgauge)
{
	int soc;

	soc = adc_get_data_by_adc(fuelgauge,
		get_battery_data(fuelgauge).ocv2soc_table,
		get_battery_data(fuelgauge).ocv2soc_table_size,
		fuelgauge->info.voltage_ocv);

	return soc; 
}

static int adc_get_current(struct sec_fuelgauge_info *fuelgauge)
{
	union power_supply_d
		propval value;

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);

	return value.intval;
}

/* judge power off or not by current_avg */
static int adc_get_current_average(struct sec_fuelgauge_info *fuelgauge)
{
	union power_supply_propval value_bat;
	union power_supply_propval value_chg;
	int vcell, soc, curr_avg;

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value_chg);
	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_HEALTH, value_bat);
	vcell = adc_get_vcell(client);
	soc = adc_get_soc(client) / 100;

	/* if 0% && under 3.4v && low power charging(1000mA), power off */
	if (!fuelgauge->pdata->is_lpm() && (soc <= 0) && (vcell < 3400) &&
			((value_chg.intval < 1000) ||
			((value_bat.intval == POWER_SUPPLY_HEALTH_OVERHEAT) ||
			(value_bat.intval == POWER_SUPPLY_HEALTH_COLD)))) {
		dev_info(&client->dev, "%s: SOC(%d), Vnow(%d), Inow(%d)\n",
			__func__, soc, vcell, value_chg.intval);
		curr_avg = -1;
	} else {
		curr_avg = value_chg.intval;
	}

	return curr_avg;
}

static void adc_reset(struct sec_fuelgauge_info *fuelgauge)
{
	int i;

	for (i = 0; i < ADC_CHANNEL_NUM; i++) {
		fuelgauge->info.adc_sample[i].cnt = 0;
		fuelgauge->info.adc_sample[i].total_adc = 0;
	}

	cancel_delayed_work(&fuelgauge->info.monitor_work);
	schedule_delayed_work(&fuelgauge->info.monitor_work, 0);
}
#endif

static void adc_volt_monitor_work(struct work_struct *work)
{
	struct sec_fuelgauge_info *fuelgauge =
		container_of(work, struct sec_fuelgauge_info,
		info.monitor_volt_work.work);
	int ret = 0;

	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid driver data\n", __func__);
		goto err_adc_read;
	}

	ret = d2199_read_voltage(fuelgauge);

	if(ret < 0) {
		pr_err("%s. Read voltage ADC failure\n", __func__);
		goto err_adc_read;
	}

	fuelgauge->info.soc = d2199_get_soc(fuelgauge);

#if 0
	if(fuelgauge->is_charging ==0) {
		schedule_delayed_work(&fuelgauge->info.monitor_volt_work, D2199_VOLTAGE_MONITOR_NORMAL);
	} else {
		schedule_delayed_work(&fuelgauge->info.monitor_volt_work, D2199_VOLTAGE_MONITOR_FAST);
	}
#endif
	pr_info("# SOC = %4d, ADC(org) = %4d, ADC(current) = %4d, ADC(avg) = %4d, Voltage(avg) = %4d mV, ADC(VF) = %4d, Charging = %d\n",
		fuelgauge->info.soc,
		fuelgauge->info.origin_volt_adc,
		fuelgauge->info.current_volt_adc,
		fuelgauge->info.average_volt_adc,
		fuelgauge->info.average_voltage,
		fuelgauge->info.vf_adc,
		fuelgauge->is_charging);
	return;

err_adc_read:
	schedule_delayed_work(&fuelgauge->info.monitor_volt_work, D2199_VOLTAGE_MONITOR_START);
	return;
}

static void adc_temp_monitor_work(struct work_struct *work)
{
	int ret = 0;
	struct sec_fuelgauge_info *fuelgauge =
								container_of(work, struct sec_fuelgauge_info,
								info.monitor_temp_work.work);

	ret = d2199_read_temperature(fuelgauge);
	if(ret < 0) {
		pr_err("%s. Failed to read_temperature\n", __func__);
		schedule_delayed_work(&fuelgauge->info.monitor_temp_work,
											D2199_TEMPERATURE_MONITOR_FAST);
		return;
	}
#if 0
	if(fuelgauge->info.temp_adc_init_done) {
		schedule_delayed_work(&fuelgauge->info.monitor_temp_work,
											D2199_TEMPERATURE_MONITOR_NORMAL);
	}
	else {
		schedule_delayed_work(&fuelgauge->info.monitor_temp_work,
											D2199_TEMPERATURE_MONITOR_FAST);
	}
#endif
	pr_info("# TEMP_BOARD(ADC) = %4d, Board Temperauter(Celsius) = %3d.%d\n",
		fuelgauge->info.average_temp_adc,
		(fuelgauge->info.average_temperature/10),
		(fuelgauge->info.average_temperature%10));

	return ;
}

static void d2199_adc_temp_monitor(fuelgauge_variable_t *fuelgauge)
{
	int ret = 0;

	ret = d2199_read_temperature(fuelgauge);

	if(ret < 0) {
		pr_err("%s. Failed to read_temperature\n", __func__);
		schedule_delayed_work(&fuelgauge->info.monitor_temp_work, D2199_TEMPERATURE_MONITOR_FAST);
		return;
	}

	pr_info("# TEMP_BOARD(ADC) = %4d, Board Temperauter(Celsius) = %3d.%d\n",
		fuelgauge->info.average_temp_adc,
		(fuelgauge->info.average_temperature/10),
		(fuelgauge->info.average_temperature%10));

	return ;
}

static void d2199_adc_volt_monitor(fuelgauge_variable_t *fuelgauge)
{
	int ret = 0;

	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid driver data\n", __func__);
		goto err_adc_read;
	}

	ret = d2199_read_voltage(fuelgauge);

	if(ret < 0) {
		pr_err("%s. Read voltage ADC failure\n", __func__);
		goto err_adc_read;
	}

	fuelgauge->info.soc = d2199_get_soc(fuelgauge);

	pr_info("# SOC = %4d, ADC(org) = %4d, ADC(current) = %4d, ADC(avg) = %4d, Voltage(avg) = %4d mV, ADC(VF) = %4d, Charging = %d\n",
		fuelgauge->info.soc,
		fuelgauge->info.origin_volt_adc,
		fuelgauge->info.current_volt_adc,
		fuelgauge->info.average_volt_adc,
		fuelgauge->info.average_voltage,
		fuelgauge->info.vf_adc,
		fuelgauge->is_charging);

	return;

err_adc_read:
	schedule_delayed_work(&fuelgauge->info.monitor_volt_work, D2199_VOLTAGE_MONITOR_START);
	return;
}

/*
 * Name : read_VF
 */
static int d2199_battery_read_vf(struct sec_fuelgauge_info *fuelgauge)
{
	u8 is_first = TRUE;
	int j, i, ret = 0;
	unsigned int sum = 0, read_adc;

	if(!fuelgauge) {
		pr_err("%s. Invalid Parameter \n", __func__);
		return -EINVAL;
	}
	
	// Read VF ADC
re_measure:
	sum = 0;
	read_adc = 0;
	for(i = 4, j = 0; i; i--) {
		ret = fuelgauge->info.d2199_read_adc(fuelgauge, D2199_ADC_VF);
		if(fuelgauge->info.adc_res[D2199_ADC_VF].is_adc_eoc) {
			read_adc = fuelgauge->info.adc_res[D2199_ADC_VF].read_adc;
			sum += read_adc;
			j++;
		}
		else {
			pr_err("%s. VF ADC read failure \n", __func__);
			ret = -EIO;
		}
	}

	if(j) {
		read_adc = (sum / j);
	}

	if(is_first == TRUE && (read_adc == 0xFFF)) {
		msleep(50);
		is_first = FALSE;
		goto re_measure;
	} else {
		fuelgauge->info.vf_adc = read_adc;
	}

	pr_debug("%s. j = %d, ADC(VF) = %d\n", __func__, j, fuelgauge->info.vf_adc);

	if((fuelgauge->info.vf_adc < fuelgauge->pdata->check_adc_max) &&
	   (fuelgauge->info.vf_adc > fuelgauge->pdata->check_adc_min))
		return true;
    else
		return false;
}

/* 
 * Name : d2199_read_adc_in_auto
 * Desc : Read ADC raw data for each channel.
 * Param : 
 *    - d2199 : 
 *    - channel : voltage, temperature 1, temperature 2, VF and TJUNC* Name : d2199_set_end_of_charge
 */
static int d2199_read_adc_in_auto(struct sec_fuelgauge_info *fuelgauge, adc_channel channel)
{
	u8 msb_res, lsb_res;
	int ret = 0;

	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}

	// The valid channel is from ADC_VOLTAGE to ADC_AIN in auto mode.
	if(channel >= D2199_ADC_CHANNEL_MAX - 1) {
		pr_err("%s. Invalid channel(%d) in auto mode\n", __func__, channel);
		return -EINVAL;
	}

	mutex_lock(&fuelgauge->info.adclock);

	fuelgauge->info.adc_res[channel].is_adc_eoc = FALSE;
	fuelgauge->info.adc_res[channel].read_adc = 0;

	// Set ADC_CONT register to select a channel.
	if(adc_cont_inven[channel].adc_preset_val) {
		ret = adc_write_reg(fuelgauge, D2199_ADC_CONT_REG,
				    adc_cont_inven[channel].adc_preset_val);
		msleep(1);
		ret |= adc_write_reg(fuelgauge, D2199_ADC_CONT_REG,
				     adc_cont_inven[channel].adc_cont_val);
		if(ret < 0)
			goto out;
	} else {
		ret = adc_write_reg(fuelgauge, D2199_ADC_CONT_REG,
				    adc_cont_inven[channel].adc_cont_val);
		if(ret < 0)
			goto out;
	}
	msleep(2);

	// Read result register for requested adc channel
	msb_res = adc_read_reg(fuelgauge, adc_cont_inven[channel].adc_msb_res);
	lsb_res = adc_read_reg(fuelgauge, adc_cont_inven[channel].adc_lsb_res);
	lsb_res &= adc_cont_inven[channel].adc_lsb_mask;
	if((ret = adc_write_reg(fuelgauge, D2199_ADC_CONT_REG, 0x00)) < 0)
		goto out;

	// Make ADC result
	fuelgauge->info.adc_res[channel].is_adc_eoc = TRUE;
	fuelgauge->info.adc_res[channel].read_adc =
		((msb_res << 4) | (lsb_res >>
				   (adc_cont_inven[channel].adc_lsb_mask == ADC_RES_MASK_MSB ? 4 : 0)));

	if (!fuelgauge->info.adc_res[channel].is_adc_eoc) {
	pr_debug("[%s]READ_EOC = %d\n", __func__,
		fuelgauge->info.adc_res[channel].is_adc_eoc);
	pr_debug("[%s]READ_ADC = %d\n", __func__,
		fuelgauge->info.adc_res[channel].read_adc);
	}
out:
	mutex_unlock(&fuelgauge->info.adclock);

	return ret;
}

/*
 * Name : d2199_read_adc_in_manual
 */
static int d2199_read_adc_in_manual(struct sec_fuelgauge_info *fuelgauge, adc_channel channel)
{
	u8 mux_sel, flag = FALSE;
	int ret, retries = D2199_MANUAL_READ_RETRIES;

	mutex_lock(&fuelgauge->info.adclock);

	fuelgauge->info.adc_res[channel].is_adc_eoc = FALSE;
	fuelgauge->info.adc_res[channel].read_adc = 0;

	switch(channel) {
		case D2199_ADC_VOLTAGE:
			mux_sel = D2199_MUXSEL_VBAT;
			break;
		case D2199_ADC_TEMPERATURE_1:
			mux_sel = D2199_MUXSEL_TEMP1;
			break;
		case D2199_ADC_TEMPERATURE_2:
			mux_sel = D2199_MUXSEL_TEMP2;
			break;
		case D2199_ADC_VF:
			mux_sel = D2199_MUXSEL_VF;
			break;
		case D2199_ADC_TJUNC:
			mux_sel = D2199_MUXSEL_TJUNC;
			break;
		default :
			pr_err("%s. Invalid channel(%d) \n", __func__, channel);
			ret = -EINVAL;
			goto out;
	}

	mux_sel |= D2199_MAN_CONV_MASK;
	if((ret = adc_write_reg(fuelgauge, D2199_ADC_MAN_REG, mux_sel)) < 0)
		goto out;

	do {
		schedule_timeout_interruptible(msecs_to_jiffies(1));
		flag = fuelgauge->info.adc_res[channel].is_adc_eoc;
	} while(retries-- && (flag == FALSE));

	if(flag == FALSE) {
		pr_warn("%s. Failed manual ADC conversion. channel(%d)\n", __func__, channel);
		ret = -EIO;
	}

out:
	mutex_unlock(&fuelgauge->info.adclock);
	
	return ret;    
}

static int d2199_set_adc_mode(struct sec_fuelgauge_info *fuelgauge, adc_mode type)
{
	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid parameter.\n", __func__);
		return -EINVAL;
	}

	if(fuelgauge->info.adc_mode != type)
	{
		if(type == D2199_ADC_IN_AUTO) {
			fuelgauge->info.d2199_read_adc = d2199_read_adc_in_auto;
		        fuelgauge->info.adc_mode = D2199_ADC_IN_AUTO;
		}
		else if(type == D2199_ADC_IN_MANUAL) {
			fuelgauge->info.d2199_read_adc = d2199_read_adc_in_manual;
			fuelgauge->info.adc_mode = D2199_ADC_IN_MANUAL;
		}
	}
	else {
		pr_debug("%s: ADC mode is same before was set \n", __func__);
	}

	return 0;
}

static void d2199_fuelgauge_data_init(struct sec_fuelgauge_info *fuelgauge)
{
	if(unlikely(!fuelgauge)) {
		pr_err("%s. Invalid platform data\n", __func__);
		return;
	}

	gbat = fuelgauge;

	fuelgauge->info.adc_mode = D2199_ADC_MODE_MAX;

	fuelgauge->info.sum_total_adc = 0;
	fuelgauge->info.vdd_hwmon_level = 0;
	fuelgauge->info.volt_adc_init_done = FALSE;
	fuelgauge->info.temp_adc_init_done = FALSE;
	fuelgauge->info.battery_present = TRUE;

	wake_lock_init(&fuelgauge->info.sleep_monitor_wakeup,
							WAKE_LOCK_SUSPEND,
							"sleep_monitor");

	return;
}

bool sec_hal_fg_init(fuelgauge_variable_t *fuelgauge)
{
	mutex_init(&fuelgauge->info.adclock);

	d2199_fuelgauge_data_init(fuelgauge);
	d2199_set_adc_mode(fuelgauge, D2199_ADC_IN_AUTO);

	adc_write_reg(fuelgauge, D2199_ADC_MAN_REG, 0x00);

	INIT_DELAYED_WORK_DEFERRABLE(&fuelgauge->info.monitor_volt_work,
		adc_volt_monitor_work);
	INIT_DELAYED_WORK_DEFERRABLE(&fuelgauge->info.monitor_temp_work,
		adc_temp_monitor_work);

	schedule_delayed_work(&fuelgauge->info.monitor_temp_work, 0);
	schedule_delayed_work(&fuelgauge->info.monitor_volt_work, 1*HZ);

	return true;
}

bool sec_hal_fg_suspend(fuelgauge_variable_t *fuelgauge)
{
	return true;
}

bool sec_hal_fg_resume(fuelgauge_variable_t *fuelgauge)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(fuelgauge_variable_t *fuelgauge, int soc)
{
	pr_debug("[%s]\n",__func__);
	return true;
}

bool sec_hal_fg_is_fuelalerted(fuelgauge_variable_t *fuelgauge)
{
	return false;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

bool sec_hal_fg_full_charged(fuelgauge_variable_t *fuelgauge)
{
	return true;
}

bool sec_hal_fg_reset(fuelgauge_variable_t *fuelgauge)
{
	d2199_reset_sw_fuelgauge(fuelgauge);
	return true;
}

bool sec_hal_fg_get_property(fuelgauge_variable_t *fuelgauge,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{

	pr_debug("#### [%s][%d] ####\n", __func__, psp);

	pr_debug("#### [%d][%d][%d] ####\n",
		fuelgauge->info.current_voltage,
		fuelgauge->info.average_voltage,
		fuelgauge->info.soc);

	switch (psp) {
/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = d2199_battery_read_vf(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		d2199_adc_volt_monitor(fuelgauge);

		val->intval = fuelgauge->info.current_voltage;
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_AVERAGE:
			val->intval = fuelgauge->info.average_voltage;
			break;
		case SEC_BATTEY_VOLTAGE_OCV:
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW)
			val->intval = fuelgauge->info.soc;
		else
			val->intval = fuelgauge->info.soc;
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		d2199_adc_temp_monitor(fuelgauge);

		val->intval = fuelgauge->info.average_temperature;
		break;
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = fuelgauge->info.average_temperature;
		break;

	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(fuelgauge_variable_t *fuelgauge,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		break;
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	int i = 0;

	switch (offset) {
	case FG_DATA:
	case FG_REGS:
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	int ret = 0;

	switch (offset) {
	case FG_REG:
	case FG_DATA:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
