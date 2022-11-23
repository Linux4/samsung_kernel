/* drivers/battery/rt5033_fuelgauge.c
 * RT5033 Voltage Tracking Fuelgauge Driver
 *
 * Copyright (C) 2013
 * Author: Dongik Sin <dongik.sin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#define RT5033_FG_DEVICE_NAME "rt5033-fg"
#define ALIAS_NAME "rt5033-fuelgauge"

#define FG_DET_BAT_PRESENT 1

static inline int rt5033_fg_read_device(struct i2c_client *client,
				      int reg, int bytes, void *dest)
{
	int ret;

	if (bytes > 1)
		ret = i2c_smbus_read_i2c_block_data(client, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

static int32_t rt5033_fg_i2c_read_word(struct i2c_client *client,
                        uint8_t reg_addr)
{
	uint16_t data = 0;
	int ret;
	ret = rt5033_fg_read_device(client, reg_addr, 2, &data);
	if (ret < 0)
		return ret;
	else
		return (int32_t)be16_to_cpu(data);
}


static int32_t rt5033_fg_i2c_write_word(struct i2c_client *client,
                            uint8_t reg_addr,uint16_t data)
{
	int ret;

	data = cpu_to_be16(data);
	ret = i2c_smbus_write_i2c_block_data(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

#if 0 //never used
static int32_t rt5033_fg_reg_read(struct i2c_client *client, int reg)
{
	unsigned char data = 0;
	int ret;
	ret = rt5033_fg_read_device(client, reg, 1, (uint8_t *)&data);

	if (ret < 0)
		return ret;
	else
		return (int)data;
}


static int32_t rt5033_fg_reg_write(struct i2c_client *client, int reg,
		unsigned char data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client,reg,data);
	return ret;
}
#endif

static unsigned int fg_get_vbat(struct i2c_client *client);
static unsigned int fg_get_avg_volt(struct i2c_client *client);

static void rt5033_get_version(struct i2c_client *client)
{
    dev_info(&client->dev, "RT5033 Fuel-Gauge Ver %s\n", GG_VERSION);
}

unsigned int fg_get_soc(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int ret;
	int volt;
	int temp;
	int offset = 0;
	gain_table_prop GainAnswer;

	#if (ENABLE_SOC_OFFSET_COMP)
	offset_table_prop OffsAnswer;
	#endif /*	ENABLE_SOC_OFFSET	*/

	unsigned int soc_val;

	// read rt5033 fg registers for debugging.
	ret = fg_get_vbat(client);
	pr_info("%s : FG_VBAT = %d\n", __func__, ret);
	ret = rt5033_fg_i2c_read_word(client, RT5033_FG_IRQ_CTRL);
	pr_info("%s : FG_IRQ_CTL_FLAG = 0x%04x\n", __func__, ret);
	ret = rt5033_fg_i2c_read_word(client, RT5033_VOLT_ALRT_TH);
	pr_info("%s : FG_VMIN_SMIN_FLAG = 0x%04x\n", __func__, ret);
	ret = rt5033_fg_i2c_read_word(client,RT5033_CRATE);
	pr_info("%s : FG_CRATE_DEVICEID = 0x%04x\n", __func__, ret);

	/* write soc = 100% when EOC occurs */
	if(fuelgauge->info.flag_full_charge == true) {
		if (fuelgauge->info.flag_once_full_soc == true) {
#if 0
			rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_EOC_SOC);
#endif
			fuelgauge->info.flag_once_full_soc = false;
		}
	} else
		fuelgauge->info.flag_once_full_soc = true;

	/* clear ri */
	ret = rt5033_fg_i2c_read_word(client, RT5033_CONFIG_MSB);

	// read FG_CONFIG  for debugging.
	pr_info("%s : FG_CONFIG = 0x%04x\n", __func__, ret);

	if (ret & 0x0040) {
		ret &= ~(0x0040);
		rt5033_fg_i2c_write_word(client, RT5033_CONFIG_MSB, ret);
	}

	/* cvs */
	temp = fuelgauge->info.temperature/10;
	if (temp < R5_TEMP)
		rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_R5_LOW_TEMP);
	else
		rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_R5_DEFAULT);

	rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, 0x8085);
	ret = rt5033_fg_i2c_read_word(client, RT5033_MFA_MSB);
	pr_info("%s : FG_CVS = 0x%04x\n", __func__, ret);

	/* vgcomp */
	volt = fg_get_avg_volt(client);
	temp = fuelgauge->info.temperature/10;

	// report FG_AVG_VOLT & FG_TEMP for debugging.
	pr_info("%s : FG_AVG_VOLT = %d, FG_TEMP = %d\n", __func__, volt, temp);

	GainAnswer = Gain_Search(client, temp, volt);
	ret = (GainAnswer.y1<<8) + GainAnswer.y2;
	rt5033_fg_i2c_write_word(client, RT5033_VGCOMP1, ret);
	ret = (GainAnswer.y3<<8) + GainAnswer.y4;
	rt5033_fg_i2c_write_word(client, RT5033_VGCOMP3, ret);

	ret = rt5033_fg_i2c_read_word(client, RT5033_VGCOMP1);
	pr_info("%s : FG_VGCOMP1_2 = 0x%04x\n", __func__, ret);
	ret = rt5033_fg_i2c_read_word(client, RT5033_VGCOMP3);
	pr_info("%s : FG_VGCOMP3_4 = 0x%04x\n", __func__, ret);

	#if (ENABLE_SOC_OFFSET_COMP)
	/* offset */
	OffsAnswer = Offs_Search(client, fuelgauge->info.offs_speci_case, fuelgauge->info.flag_chg_status, temp, volt);

	/* move average offset */
	if (fuelgauge->info.init_once == true) {
		int i;
		for(i=0; i<6; i++)
			fuelgauge->info.move_avg_offset[i] = OffsAnswer.y;
		offset = OffsAnswer.y;
	} else {
			int i;
			i = fuelgauge->info.move_avg_offset_cnt % 6;
			fuelgauge->info.move_avg_offset[i] = OffsAnswer.y;
			for(i=0; i<6; i++)
				offset += fuelgauge->info.move_avg_offset[i];
			offset = offset/6;
			fuelgauge->info.move_avg_offset_cnt++;
			if(fuelgauge->info.move_avg_offset_cnt > 5)
				fuelgauge->info.move_avg_offset_cnt = 0;
	}
	#endif /* ENABLE_SOC_OFFSET_COMP */
	/* read soc */
	ret = rt5033_fg_i2c_read_word(client, RT5033_SOC_MSB);

	// report FG_SOC_RAW for debugging.
	pr_info("%s : FG_SOC_RAW = 0x%04x\n", __func__, ret);

	if (ret<0) {
		pr_err("%s: read soc reg fail", __func__);
		soc_val = 500;
	} else {
		soc_val = ((ret)>>8)*10 + (ret&0x00ff)*10/256 + offset;
	}
	fuelgauge->info.batt_soc = soc_val;
	// report FG_SOC for debugging.
	pr_info("%s : FG_SOC = %d\n", __func__,soc_val);
	return soc_val;
}

unsigned int fg_get_ocv(struct i2c_client *client)
{
	int ret;
	unsigned int ocv;// = 3500; /*3500 means 3500mV*/
	ret = rt5033_fg_i2c_read_word(client, RT5033_OCV_MSB);
	if (ret<0) {
		pr_err("%s: read soc reg fail", __func__);
		ocv = 4000;
	} else {
		ocv = (ret&0xfff0)>>4;
		ocv = ocv * 125 / 100;
	}
	return ocv;
}

static unsigned int fg_get_vbat(struct i2c_client *client)
{
	int ret;
	unsigned int vbat;/* = 3500; 3500 means 3500mV*/
	ret = rt5033_fg_i2c_read_word(client, RT5033_VBAT_MSB);
	if (ret<0) {
		pr_err("%s: read vbat fail", __func__);
		vbat = 4000;
	} else {
		vbat = (ret&0xfff0)>>4;
		vbat = vbat * 125 / 100;
	}
	return vbat;
}

unsigned int fg_get_avg_volt(struct i2c_client *client)
{
	int ret;
	unsigned int avg_volt;/* = 3500; 3500 means 3500mV*/
	ret = rt5033_fg_i2c_read_word(client, RT5033_AVG_VOLT_MSB);
	if (ret<0) {
		pr_err("%s: read vbat fail", __func__);
		avg_volt = 4000;
	} else {
		avg_volt = (ret&0xfff0)>>4;
		avg_volt = avg_volt * 125 / 100;
	}
	return avg_volt;
}

static unsigned int fg_get_device_id(struct i2c_client *client)
{
	int ret;
	ret = rt5033_fg_i2c_read_word(client, RT5033_CRATE);
	ret &= 0x00ff;
	return ret;
}

static bool rt5033_fg_get_batt_present(struct i2c_client *client)
{
    int ret = rt5033_fg_i2c_read_word(client,RT5033_CONFIG_MSB);
    if (ret<0)
        return false;
    return (ret & 0x02)? true : false;
}

gain_table_prop Gain_Search(struct i2c_client *client, int nTemp, int nVolt)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int nTargetIdx1 = 0, nTargetIdx2 = 0, i;

	gain_table_prop GainReturn[2];
	gain_table_prop *rt5033_battery_param1 =  get_battery_data(fuelgauge).param1;
	gain_table_prop *rt5033_battery_param2 =  get_battery_data(fuelgauge).param2;
	gain_table_prop *rt5033_battery_param3 =  get_battery_data(fuelgauge).param3;
	gain_table_prop *rt5033_battery_param4 =  get_battery_data(fuelgauge).param4;

	for(i=0;i<2;i++) {
		GainReturn[i].x = 0;
		GainReturn[i].y1 = 0;
		GainReturn[i].y2 = 0;
		GainReturn[i].y3 = 0;
		GainReturn[i].y4 = 0;
	}

	if(nTemp <= GAIN_RANGE1) {	/* use table1 */
		/* Calculate Table1 */
		nTargetIdx1 = my_gain_search(rt5033_battery_param1, nVolt,  get_battery_data(fuelgauge).param1_size);

		/* pr_info("Found Index = %d\n", nTargetIdx1); */
		GainReturn[0].x = nVolt;
		GainReturn[0].y1 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y1, rt5033_battery_param1[nTargetIdx1-1].y1);
		GainReturn[0].y2 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y2, rt5033_battery_param1[nTargetIdx1-1].y2);
		GainReturn[0].y3 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y3, rt5033_battery_param1[nTargetIdx1-1].y3);
		GainReturn[0].y4 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y4, rt5033_battery_param1[nTargetIdx1-1].y4);
		return GainReturn[0];
	} else if(nTemp <= GAIN_RANGE2) {	/* use table1 & table2 interpolation OR table 2 */
		/* Calculate Table1 */
		nTargetIdx1 = my_gain_search(rt5033_battery_param1, nVolt,  get_battery_data(fuelgauge).param1_size);

		GainReturn[0].x = nVolt;
		GainReturn[0].y1 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y1, rt5033_battery_param1[nTargetIdx1-1].y1);
		GainReturn[0].y2 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y2, rt5033_battery_param1[nTargetIdx1-1].y2);
		GainReturn[0].y3 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y3, rt5033_battery_param1[nTargetIdx1-1].y3);
		GainReturn[0].y4 = my_interpolation(nVolt, rt5033_battery_param1[nTargetIdx1].x, rt5033_battery_param1[nTargetIdx1-1].x,
																				rt5033_battery_param1[nTargetIdx1].y4, rt5033_battery_param1[nTargetIdx1-1].y4);

		// Calculate Table2
		nTargetIdx2 = my_gain_search(rt5033_battery_param2, nVolt,  get_battery_data(fuelgauge).param2_size);
		GainReturn[1].x = nVolt;
		GainReturn[1].y1 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx2].x, rt5033_battery_param2[nTargetIdx2-1].x,
																				rt5033_battery_param2[nTargetIdx2].y1, rt5033_battery_param2[nTargetIdx2-1].y1);
		GainReturn[1].y2 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx2].x, rt5033_battery_param2[nTargetIdx2-1].x,
																				rt5033_battery_param2[nTargetIdx2].y2, rt5033_battery_param2[nTargetIdx2-1].y2);
		GainReturn[1].y3 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx2].x, rt5033_battery_param2[nTargetIdx2-1].x,
																				rt5033_battery_param2[nTargetIdx2].y3, rt5033_battery_param2[nTargetIdx2-1].y3);
		GainReturn[1].y4 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx2].x, rt5033_battery_param2[nTargetIdx2-1].x,
																				rt5033_battery_param2[nTargetIdx2].y4, rt5033_battery_param2[nTargetIdx2-1].y4);

		/* Debug
		pr_info("Found Index1 = %d ; Index2 = %d\n", nTargetIdx1, nTargetIdx2);
		pr_info("Table[1/2], x=%d / %d\n", GainReturn[0].x, GainReturn[1].x);
		pr_info("Table[1/2], y1=%d / %d\n", GainReturn[0].y1, GainReturn[1].y1);
		pr_info("Table[1/2], y2=%d / %d\n", GainReturn[0].y2, GainReturn[1].y2);
		pr_info("Table[1/2], y3=%d / %d\n", GainReturn[0].y3, GainReturn[1].y3);
		pr_info("Table[1/2], y4=%d / %d\n", GainReturn[0].y4, GainReturn[1].y4);
		*/

		/* Interpolation or NOT */
		if(nTemp == GAIN_RANGE2)
			return GainReturn[1];
		else {
			GainReturn[0].x = nVolt;
			GainReturn[0].y1 = my_interpolation(nTemp, GAIN_RANGE1, GAIN_RANGE2, GainReturn[0].y1, GainReturn[1].y1);
			GainReturn[0].y2 = my_interpolation(nTemp, GAIN_RANGE1, GAIN_RANGE2, GainReturn[0].y2, GainReturn[1].y2);
			GainReturn[0].y3 = my_interpolation(nTemp, GAIN_RANGE1, GAIN_RANGE2, GainReturn[0].y3, GainReturn[1].y3);
			GainReturn[0].y4 = my_interpolation(nTemp, GAIN_RANGE1, GAIN_RANGE2, GainReturn[0].y4, GainReturn[1].y4);

			return GainReturn[0];
		}
	} else if(nTemp <= GAIN_RANGE3) {	/* use table2 & table3 interpolation OR table 3 */
		/* Calculate Table2 */
		nTargetIdx1 = my_gain_search(rt5033_battery_param2, nVolt,  get_battery_data(fuelgauge).param2_size);

		GainReturn[0].x = nVolt;
		GainReturn[0].y1 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx1].x, rt5033_battery_param2[nTargetIdx1-1].x,
																				rt5033_battery_param2[nTargetIdx1].y1, rt5033_battery_param2[nTargetIdx1-1].y1);
		GainReturn[0].y2 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx1].x, rt5033_battery_param2[nTargetIdx1-1].x,
																				rt5033_battery_param2[nTargetIdx1].y2, rt5033_battery_param2[nTargetIdx1-1].y2);
		GainReturn[0].y3 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx1].x, rt5033_battery_param2[nTargetIdx1-1].x,
																				rt5033_battery_param2[nTargetIdx1].y3, rt5033_battery_param2[nTargetIdx1-1].y3);
		GainReturn[0].y4 = my_interpolation(nVolt, rt5033_battery_param2[nTargetIdx1].x, rt5033_battery_param2[nTargetIdx1-1].x,
																				rt5033_battery_param2[nTargetIdx1].y4, rt5033_battery_param2[nTargetIdx1-1].y4);

		/* Calculate Table3 */
		nTargetIdx2 = my_gain_search(rt5033_battery_param3, nVolt,  get_battery_data(fuelgauge).param3_size);
		GainReturn[1].x = nVolt;
		GainReturn[1].y1 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx2].x, rt5033_battery_param3[nTargetIdx2-1].x,
																				rt5033_battery_param3[nTargetIdx2].y1, rt5033_battery_param3[nTargetIdx2-1].y1);
		GainReturn[1].y2 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx2].x, rt5033_battery_param3[nTargetIdx2-1].x,
																				rt5033_battery_param3[nTargetIdx2].y2, rt5033_battery_param3[nTargetIdx2-1].y2);
		GainReturn[1].y3 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx2].x, rt5033_battery_param3[nTargetIdx2-1].x,
																				rt5033_battery_param3[nTargetIdx2].y3, rt5033_battery_param3[nTargetIdx2-1].y3);
		GainReturn[1].y4 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx2].x, rt5033_battery_param3[nTargetIdx2-1].x,
																				rt5033_battery_param3[nTargetIdx2].y4, rt5033_battery_param3[nTargetIdx2-1].y4);

		/* Debug
		pr_info("Found Index1 = %d ; Index2 = %d\n", nTargetIdx1, nTargetIdx2);
		pr_info("Table[1/2], x=%d / %d\n", GainReturn[0].x, GainReturn[1].x);
		pr_info("Table[1/2], y1=%d / %d\n", GainReturn[0].y1, GainReturn[1].y1);
		pr_info("Table[1/2], y2=%d / %d\n", GainReturn[0].y2, GainReturn[1].y2);
		pr_info("Table[1/2], y3=%d / %d\n", GainReturn[0].y3, GainReturn[1].y3);
		pr_info("Table[1/2], y4=%d / %d\n", GainReturn[0].y4, GainReturn[1].y4);
		*/

		/* Interpolation or NOT */
		if(nTemp == GAIN_RANGE3)
			return GainReturn[1];
		else {
			GainReturn[0].x = nVolt;
			GainReturn[0].y1 = my_interpolation(nTemp, GAIN_RANGE2, GAIN_RANGE3, GainReturn[0].y1, GainReturn[1].y1);
			GainReturn[0].y2 = my_interpolation(nTemp, GAIN_RANGE2, GAIN_RANGE3, GainReturn[0].y2, GainReturn[1].y2);
			GainReturn[0].y3 = my_interpolation(nTemp, GAIN_RANGE2, GAIN_RANGE3, GainReturn[0].y3, GainReturn[1].y3);
			GainReturn[0].y4 = my_interpolation(nTemp, GAIN_RANGE2, GAIN_RANGE3, GainReturn[0].y4, GainReturn[1].y4);

			return GainReturn[0];
		}
	} else if(nTemp <= GAIN_RANGE4) {	/* use table3 & table4 interpolation OR table 4 */
		/* Calculate Table3 */
		nTargetIdx1 = my_gain_search(rt5033_battery_param3, nVolt,  get_battery_data(fuelgauge).param3_size);
		GainReturn[0].x = nVolt;
		GainReturn[0].y1 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx1].x, rt5033_battery_param3[nTargetIdx1-1].x,
																				rt5033_battery_param3[nTargetIdx1].y1, rt5033_battery_param3[nTargetIdx1-1].y1);
		GainReturn[0].y2 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx1].x, rt5033_battery_param3[nTargetIdx1-1].x,
																				rt5033_battery_param3[nTargetIdx1].y2, rt5033_battery_param3[nTargetIdx1-1].y2);
		GainReturn[0].y3 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx1].x, rt5033_battery_param3[nTargetIdx1-1].x,
																				rt5033_battery_param3[nTargetIdx1].y3, rt5033_battery_param3[nTargetIdx1-1].y3);
		GainReturn[0].y4 = my_interpolation(nVolt, rt5033_battery_param3[nTargetIdx1].x, rt5033_battery_param3[nTargetIdx1-1].x,
																				rt5033_battery_param3[nTargetIdx1].y4, rt5033_battery_param3[nTargetIdx1-1].y4);

		/* Calculate Table4 */
		nTargetIdx2 = my_gain_search(rt5033_battery_param4, nVolt,  get_battery_data(fuelgauge).param4_size);
		GainReturn[1].x = nVolt;
		GainReturn[1].y1 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx2].x, rt5033_battery_param4[nTargetIdx2-1].x,
																				rt5033_battery_param4[nTargetIdx2].y1, rt5033_battery_param4[nTargetIdx2-1].y1);
		GainReturn[1].y2 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx2].x, rt5033_battery_param4[nTargetIdx2-1].x,
																				rt5033_battery_param4[nTargetIdx2].y2, rt5033_battery_param4[nTargetIdx2-1].y2);
		GainReturn[1].y3 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx2].x, rt5033_battery_param4[nTargetIdx2-1].x,
																				rt5033_battery_param4[nTargetIdx2].y3, rt5033_battery_param4[nTargetIdx2-1].y3);
		GainReturn[1].y4 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx2].x, rt5033_battery_param4[nTargetIdx2-1].x,
																				rt5033_battery_param4[nTargetIdx2].y4, rt5033_battery_param4[nTargetIdx2-1].y4);

		/* Debug

		pr_info("Found Index1 = %d ; Index2 = %d\n", nTargetIdx1, nTargetIdx2);
		pr_info("Table[1/2], x=%d / %d\n", GainReturn[0].x, GainReturn[1].x);
		pr_info("Table[1/2], y1=%d / %d\n", GainReturn[0].y1, GainReturn[1].y1);
		pr_info("Table[1/2], y2=%d / %d\n", GainReturn[0].y2, GainReturn[1].y2);
		pr_info("Table[1/2], y3=%d / %d\n", GainReturn[0].y3, GainReturn[1].y3);
		pr_info("Table[1/2], y4=%d / %d\n", GainReturn[0].y4, GainReturn[1].y4);
		*/

		/* Interpolation or NOT */
		if(nTemp == GAIN_RANGE4)
			return GainReturn[1];
		else {
			GainReturn[0].x = nVolt;
			GainReturn[0].y1 = my_interpolation(nTemp, GAIN_RANGE3, GAIN_RANGE4, GainReturn[0].y1, GainReturn[1].y1);
			GainReturn[0].y2 = my_interpolation(nTemp, GAIN_RANGE3, GAIN_RANGE4, GainReturn[0].y2, GainReturn[1].y2);
			GainReturn[0].y3 = my_interpolation(nTemp, GAIN_RANGE3, GAIN_RANGE4, GainReturn[0].y3, GainReturn[1].y3);
			GainReturn[0].y4 = my_interpolation(nTemp, GAIN_RANGE3, GAIN_RANGE4, GainReturn[0].y4, GainReturn[1].y4);

			return GainReturn[0];
		}
	} else {
		/* Calculate Table4 */
		nTargetIdx1 = my_gain_search(rt5033_battery_param4, nVolt, get_battery_data(fuelgauge).param4_size);

		GainReturn[0].x = nVolt;
		GainReturn[0].y1 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx1].x, rt5033_battery_param4[nTargetIdx1-1].x,
																				rt5033_battery_param4[nTargetIdx1].y1, rt5033_battery_param4[nTargetIdx1-1].y1);
		GainReturn[0].y2 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx1].x, rt5033_battery_param4[nTargetIdx1-1].x,
																				rt5033_battery_param4[nTargetIdx1].y2, rt5033_battery_param4[nTargetIdx1-1].y2);
		GainReturn[0].y3 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx1].x, rt5033_battery_param4[nTargetIdx1-1].x,
																				rt5033_battery_param4[nTargetIdx1].y3, rt5033_battery_param4[nTargetIdx1-1].y3);
		GainReturn[0].y4 = my_interpolation(nVolt, rt5033_battery_param4[nTargetIdx1].x, rt5033_battery_param4[nTargetIdx1-1].x,
																				rt5033_battery_param4[nTargetIdx1].y4, rt5033_battery_param4[nTargetIdx1-1].y4);
		return GainReturn[0];
	}
	return GainReturn[0];
}

offset_table_prop Offs_Search(struct i2c_client *client, int speci_case_flag, int nChg, int nTemp, int nVolt)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int nTargetIdx1 = 0;
	offset_table_prop OffsReturn;
	offset_table_prop *rt5033_battery_offset1 = get_battery_data(fuelgauge).offset1;
	offset_table_prop *rt5033_battery_offset2 = get_battery_data(fuelgauge).offset2;
	offset_table_prop *rt5033_battery_offset3 = get_battery_data(fuelgauge).offset3;
	offset_table_prop *rt5033_battery_offset4 = get_battery_data(fuelgauge).offset4;

	OffsReturn.x = 0;
	OffsReturn.y = 0;

	if(speci_case_flag == true) {
		nTargetIdx1 = my_offs_search(rt5033_battery_offset4, nVolt, get_battery_data(fuelgauge).offset4_size);
		OffsReturn.x = nVolt;
		OffsReturn.y = my_interpolation(nVolt, rt5033_battery_offset4[nTargetIdx1].x, rt5033_battery_offset4[nTargetIdx1-1].x,
																			rt5033_battery_offset4[nTargetIdx1].y, rt5033_battery_offset4[nTargetIdx1-1].y);
		return OffsReturn;
	} else {
		if(nChg == true) {		/* Handle Charging */
			if((nTemp >= OFFS_THRES_CHG_25-2) && (nTemp <= OFFS_THRES_CHG_25+2)) { /* use table2 */
				/* Calculate Table1 */
				nTargetIdx1 = my_offs_search(rt5033_battery_offset2, nVolt, get_battery_data(fuelgauge).offset2_size);

				OffsReturn.x = nVolt;
				OffsReturn.y = my_interpolation(nVolt, rt5033_battery_offset2[nTargetIdx1].x, rt5033_battery_offset2[nTargetIdx1-1].x,
																			rt5033_battery_offset2[nTargetIdx1].y, rt5033_battery_offset2[nTargetIdx1-1].y);
				return OffsReturn;
			} else
				return OffsReturn;
		} else {	/* Handle Discharging */
			if((nTemp >= OFFS_THRES_DSG_N20-5) && (nTemp <= OFFS_THRES_DSG_N20+5)) { /* use table1 */
				/* nTargetIdx1 = my_binarysearch(rt5033_battery_offset1, nVolt, ReturnOffsArraySize(OFFS_TABLE1_INDEX)); */
				nTargetIdx1 = my_offs_search(rt5033_battery_offset1, nVolt, get_battery_data(fuelgauge).offset1_size);

				OffsReturn.x = nVolt;
				OffsReturn.y = my_interpolation(nVolt, rt5033_battery_offset1[nTargetIdx1].x, rt5033_battery_offset1[nTargetIdx1-1].x,
																				rt5033_battery_offset1[nTargetIdx1].y, rt5033_battery_offset1[nTargetIdx1-1].y);
				return OffsReturn;
			} else if((nTemp >= OFFS_THRES_DSG_25-2) && (nTemp <= OFFS_THRES_DSG_25+2))	 { /* use table3 */
				/* Calculate Table3 */
				/* nTargetIdx1 = my_binarysearch(rt5033_battery_offset3, nVolt, ReturnOffsArraySize(OFFS_TABLE3_INDEX)); */
				nTargetIdx1 = my_offs_search(rt5033_battery_offset3, nVolt, get_battery_data(fuelgauge).offset3_size);

				OffsReturn.x = nVolt;
				OffsReturn.y = my_interpolation(nVolt, rt5033_battery_offset3[nTargetIdx1].x, rt5033_battery_offset3[nTargetIdx1-1].x,
																				rt5033_battery_offset3[nTargetIdx1].y, rt5033_battery_offset3[nTargetIdx1-1].y);
				return OffsReturn;
			} else
				return OffsReturn;
		}
	}

}

int my_gain_search(gain_table_prop* target_list, int nKey, int nSize)
{
    int low = 0, high = nSize - 1;
	int mid = 0;

	while (low <= high) {
		mid = (low + high) / 2;

		// printf("high = %02d ; mid = %02d ; low = %02d ; a[mid] = %04d ; key = %04d\n",high, mid, low, target_list[mid].x, nKey); // Debug
		if (target_list[mid].x == nKey)
			return mid;

		if(high > low + 1) {
			if (target_list[mid].x > nKey) {
				if(target_list[mid-1].x > nKey)
					high = mid-1;
				else
					high = mid;
			} else if (target_list[mid].x < nKey) {
				if(target_list[mid+1].x < nKey)
					low = mid+1;
				else
					low = mid;
			}
		} else
			return high;
	}
	return (mid+1);
}

int my_interpolation(int x_key, int x1, int x2, int y1, int y2)
{
	int y_key = 0;

	y_key = y1 + (((x_key - x1)*(y2 - y1))/(x2 - x1));

	return y_key;
}

int my_offs_search(offset_table_prop* target_list, int nKey, int nSize)
{
    int low = 0, high = nSize - 1;
	int mid = 0;

	while (low <= high) {
        mid = (low + high) / 2;

        if (target_list[mid].x == nKey) {
            return mid;
        }
		if(high > low + 1) {
			if (target_list[mid].x > nKey) {
				if(target_list[mid-1].x > nKey)
					high = mid-1;
				else
					high = mid;
			} else if (target_list[mid].x < nKey) {
				if(target_list[mid+1].x < nKey)
					low = mid+1;
				else
					low = mid;
			}
		} else
			return high;
    }
	return (mid+1);
}



static bool rt5033_fg_init(struct i2c_client *client)
{
	int ret;

	/* disable hibernate mode */
	ret = rt5033_fg_i2c_read_word(client, RT5033_CONFIG_MSB);
	if(ret & 0x8000)
	  rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_EXIT_HIB);

	fg_get_device_id(client);

	return true;
}

/*
 static void rt5033_fg_shutdown(struct rt_fg_info *pinfo)
{
    struct sec_fg_info *info = (struct sec_fg_info *)pinfo;

	int ret;
	// enable hibernate mode
	ret = rt5033_fg_i2c_read_word(info, RT5033_CONFIG_MSB);
	ret |= 0x0100;
	rt5033_fg_i2c_write_word(info, RT5033_CONFIG_MSB, ret);
}
*/

bool sec_hal_fg_init(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
    int i;
	offset_table_prop *rt5033_battery_offset4 = get_battery_data(fuelgauge).offset4;
    rt5033_fg_init(client);
	rt5033_get_version(client);
	fuelgauge->info.init_once = true;
	fuelgauge->info.pre_soc = 50;
	fuelgauge->info.move_avg_offset_cnt = 0;
	fuelgauge->info.volt_alert_flag = false;
	fuelgauge->info.soc_alert_flag = false;
	fuelgauge->info.bat_pres_flag = true;
	fuelgauge->info.offs_speci_case = false;
	fuelgauge->info.flag_once_full_soc = true;
	fuelgauge->info.temperature = 250;
	for(i=0; i<100; i++) {
		if(rt5033_battery_offset4[i].y != 0) {
			fuelgauge->info.offs_speci_case = true;
			break;
		}
	}

	return true;
}

bool sec_hal_fg_suspend(struct i2c_client *client)
{
	int ret;
	/* enable hibernate mode */
	ret = rt5033_fg_i2c_read_word(client, RT5033_CONFIG_MSB);
	if(!(ret & 0x0100)) {
		ret |= 0x0100;
		rt5033_fg_i2c_write_word(client, RT5033_CONFIG_MSB, ret);
	}
	// enter bibernate mode
	rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_ENTRY_HIB);

	return true;
}

bool sec_hal_fg_resume(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int ret;
	/* disable hibernate mode */
	ret = rt5033_fg_i2c_read_word(client, RT5033_CONFIG_MSB);
	if(ret & 0x8000)
		rt5033_fg_i2c_write_word(client,
			RT5033_MFA_MSB, MFA_CMD_EXIT_HIB);

	fuelgauge->info.init_once = true;
	return true;
}

bool sec_hal_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int ret;

	ret = rt5033_fg_i2c_read_word(client, RT5033_FG_IRQ_CTRL);
	/* enable volt, soc and battery presence alert irq; clear volt and soc alert status via i2c */
	ret |= 0x1f00;
	rt5033_fg_i2c_write_word(client,RT5033_FG_IRQ_CTRL,ret);

	/* set volt and soc alert threshold */
	ret = 0;
	ret = VOLT_ALRT_TH << 8;
	ret += soc;
	rt5033_fg_i2c_write_word(client, RT5033_VOLT_ALRT_TH, ret);

	/* reset soc alert flag */
	fuelgauge->info.soc_alert_flag = false;

	return true;
}

bool sec_hal_fg_is_fuelalerted(struct i2c_client *client)
{
	int ret;

	ret = rt5033_fg_i2c_read_word(client, RT5033_FG_IRQ_CTRL);

	if (ret & 0x07)		/* ALRT is asserted */
		return true;

	return false;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
#if 0
	struct sec_fuelgauge_info *fuelgauge = irq_data;

	int ret;
	int ret2;
	int temp;

	ret = rt5033_fg_i2c_read_word(fuelgauge->client, RT5033_FG_IRQ_CTRL);

	if (ret & 0x01)	/* soc alert process */
	{
		temp = ret;
		// disable soc alert
		ret2 = rt5033_fg_i2c_read_word(client, RT5033_VOLT_ALRT_TH);
		ret2 &= 0xff00;
		rt5033_fg_i2c_write_word(client, RT5033_VOLT_ALRT_TH, ret2);
		// clear soc alert status
	  temp &= 0xfffe;
	  rt5033_fg_i2c_write_word(client, RT5033_FG_IRQ_CTRL, temp);
	  // set soc alert flag
	  fuelgauge->info.soc_alert_flag = true;
	}

	if (ret & 0x02) /* voltage alert process */
	{
		temp = ret;
		// disable voltage alert
		ret2 = rt5033_fg_i2c_read_word(client, RT5033_VOLT_ALRT_TH);
		ret2 &= 0x00ff;
		rt5033_fg_i2c_write_word(client, RT5033_VOLT_ALRT_TH, ret2);
		// clear voltage alert status
	  temp &= 0xfffd;
	  rt5033_fg_i2c_write_word(client, RT5033_FG_IRQ_CTRL, temp);
	  rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_VALRT_SOC);
	 fuelgauge->info.volt_alert_flag = true;
	}

	if (ret & 0x04) /* battery alert process */
	{
		temp = ret;
		// check battery status
		ret2 = rt5033_fg_i2c_read_word(client, RT5033_CONFIG_MSB);
		if(ret2&0x0002)
		{
			fuelgauge->info.bat_pres_flag = true;			// battery inserted
			if (ret2 & 0x0800)
			{
				ret2 &= (~0x0800);						// disable real time battery detection
			    rt5033_fg_i2c_write_word(client, RT5033_CONFIG_MSB, ret2);
		   }
		}
		else
		{
			fuelgauge->info.bat_pres_flag = false;		// battery removed
			if (!(ret2 & 0x0800))
			{
				ret2 |= 0x0800;					// enable real time battery detection
			rt5033_fg_i2c_write_word(client, RT5033_CONFIG_MSB, ret2);
			}
			if (ret2 & 0x8000)					// exit hibernate mode
			rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, MFA_CMD_EXIT_HIB);
		}
		// clear battery alert status
	  temp &= 0xfffb;
	  rt5033_fg_i2c_write_word(client, RT5033_FG_IRQ_CTRL, temp);
	}
#endif
	return true;
}

bool sec_hal_fg_full_charged(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);

	 fuelgauge->info.flag_full_charge = true;
	return true;
}

bool sec_hal_fg_reset(struct i2c_client *client)
{
	rt5033_fg_i2c_write_word(client, RT5033_MFA_MSB, 0x5400);
	return true;
}

bool sec_hal_fg_get_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	union power_supply_propval value;
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_STATUS, value);
	fuelgauge->info.flag_full_charge = (value.intval==POWER_SUPPLY_STATUS_FULL)?1:0;
	fuelgauge->info.flag_chg_status = (value.intval==POWER_SUPPLY_STATUS_CHARGING)?1:0;

	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = fg_get_vbat(client);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_AVERAGE:
			val->intval = fg_get_avg_volt(client);
			break;
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = fg_get_ocv(client);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = rt5033_fg_get_batt_present(client);
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
        break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval =
            (fuelgauge->info.batt_soc >= 1000) ? true : false;
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = fg_get_soc(client);
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
        val->intval = fuelgauge->info.temperature;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
    struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	switch (psp) {
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
        fuelgauge->info.temperature = val->intval;
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{

	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
    struct sec_fg_info *info = &fg->info;
	int i = 0;

	switch (offset) {
/*	case FG_REG: */
/*		break; */
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			info->reg_data[1], info->reg_data[0]);
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
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
    struct sec_fg_info *info = &fg->info;
	int ret = 0;
	int x = 0;
	int data;

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			info->reg_addr = x;
			data = rt5033_fg_i2c_read_word(fg->client,
				info->reg_addr);
			info->reg_data[0] = data&0xff;
			info->reg_data[1] = (data>>8)&0xff;
			dev_dbg(info->dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				 __func__, info->reg_addr,
				 info->reg_data[1], info->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			info->reg_data[0] = (x & 0xFF00) >> 8;
			info->reg_data[1] = (x & 0x00FF);
			dev_dbg(info->dev,
				"%s: (write) addr = 0x%x, data = 0x%02x%02x\n",
				__func__, info->reg_addr, info->reg_data[1], info->reg_data[0]);

			rt5033_fg_i2c_write_word(fg->client,
				info->reg_addr, x);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
