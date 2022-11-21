/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/kernel.h>
#include <linux/err.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/io.h>
#include <soc/sprd/adi.h>
#include "thm.h"
#include <linux/sprd_thm.h>
#include <soc/sprd/arch_misc.h>
#include <soc/sprd/hardware.h>
#include "thermal_core.h"
#include <soc/sprd/adc.h>

#define SPRD_THM_BOARD_DEBUG
#ifdef SPRD_THM_BOARD_DEBUG
#define THM_BOARD_DEBUG(format, arg...) printk("sprd board thm: " "@@@" format, ## arg)
#else
#define THM_BOARD_DEBUG(format, arg...)
#endif


struct sprd_board_sensor_config *pthm_config;
extern uint16_t sprdchg_bat_adc_to_vol(uint16_t adcvalue);

int sprdthm_read_temp_adc(void)
{
#define SAMPLE_NUM  15
	int cnt = pthm_config->temp_adc_sample_cnt;

	if (cnt > SAMPLE_NUM) {
		cnt = SAMPLE_NUM;
	} else if (cnt < 1) {
		cnt = 1;
	}

	if (pthm_config->temp_support) {
		int ret, i, j, temp;
		int adc_val[cnt];
		struct adc_sample_data data = {
			.channel_id = pthm_config->temp_adc_ch,
			.channel_type = 0,	/*sw */
			.hw_channel_delay = 0,	/*reserved */
			.scale = pthm_config->temp_adc_scale,	/*small scale */
			.pbuf = &adc_val[0],
			.sample_num = cnt,
			.sample_bits = 1,
			.sample_speed = 0,	/*quick mode */
			.signal_mode = 0,	/*resistance path */
		};

		ret = sci_adc_get_values(&data);
		WARN_ON(0 != ret);

		for (j = 1; j <= cnt - 1; j++) {
			for (i = 0; i < cnt - j; i++) {
				if (adc_val[i] > adc_val[i + 1]) {
					temp = adc_val[i];
					adc_val[i] = adc_val[i + 1];
					adc_val[i + 1] = temp;
				}
			}
		}
		THM_BOARD_DEBUG("sprdthm: channel:%d,sprdthm_read_temp_adc:%d\n",
		       data.channel_id, adc_val[cnt / 2]);
		return adc_val[cnt / 2];
	} else {
		return 3000;
	}
}

int sprdthm_interpolate(int x, int n, struct sprdboard_table_data *tab)
{
	int index;
	int y;

	if (x >= tab[0].x)
		y = tab[0].y;
	else if (x <= tab[n - 1].x)
		y = tab[n - 1].y;
	else {
		/*  find interval */
		for (index = 1; index < n; index++)
			if (x > tab[index].x)
				break;
		/*  interpolate */
		y = (tab[index - 1].y - tab[index].y) * (x - tab[index].x)
		    * 2 / (tab[index - 1].x - tab[index].x);
		y = (y + 1) / 2;
		y += tab[index].y;
	}
	return y;
}
static uint16_t sprdthm_adc_to_vol(uint16_t channel, int scale,
				   uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vthm_vol = sprdchg_bat_adc_to_vol(adcvalue);
	uint32_t m, n;
	uint32_t thm_numerators, thm_denominators;
	uint32_t numerators, denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &thm_numerators,
			      &thm_denominators);
	sci_adc_get_vol_ratio(channel, scale, &numerators, &denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = thm_denominators * numerators;
	m = vthm_vol * thm_numerators * (denominators);
	result = (m + n / 2) / n;
	return result;
}

int sprdthm_search_temp_tab(int val)
{
	return sprdthm_interpolate(val, pthm_config->temp_tab_size,
				   pthm_config->temp_tab);
}

int sprd_board_thm_get_reg_base(struct sprd_thermal_zone *pzone ,struct resource *regs)
{
	return 0;
}
int sprd_board_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip )
{
	return 0;
}

int sprd_board_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	if (pthm_config->temp_support) {
		int val = sprdthm_read_temp_adc();
		//voltage mode
		if (pthm_config->temp_table_mode) {
			val =sprdthm_adc_to_vol(pthm_config->temp_adc_ch,
					       pthm_config->temp_adc_scale, val);
			THM_BOARD_DEBUG("sprdthm: sprdthm_read_temp voltage:%d\n", val);
		}
        THM_BOARD_DEBUG("sprd_board_thm_temp_read y=%d\n",sprdthm_search_temp_tab(val));
		return sprdthm_search_temp_tab(val);
	} else {
		return -35;
	}
	
}

int sprd_board_thm_hw_init(struct sprd_thermal_zone *pzone)
{
    THM_BOARD_DEBUG("sprd_board_thm_hw_init\n");
	pthm_config=pzone->sensor_config;

	return 0;
}


int sprd_board_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
     THM_BOARD_DEBUG("sprd_board_thm_hw_suspend\n");
	
	return 0;
}
int sprd_board_thm_hw_resume(struct sprd_thermal_zone *pzone)
{

    THM_BOARD_DEBUG("sprd_board_thm_hw_resume\n");

	return 0;
}

int sprd_board_thm_hw_irq_handle(struct sprd_thermal_zone *pzone)
{
	return 0;
}
struct thm_handle_ops sprd_boardthm_ops =
{
	.hw_init = sprd_board_thm_hw_init,
	.get_reg_base = sprd_board_thm_get_reg_base,
	.read_temp = sprd_board_thm_temp_read,
	.irq_handle = sprd_board_thm_hw_irq_handle,
	.suspend = sprd_board_thm_hw_suspend,
	.resume = sprd_board_thm_hw_resume,
};
static int __init sprd_board_thermal_init(void)
{
	sprd_boardthm_add(&sprd_boardthm_ops,"sprd_board_thm",SPRD_BOARD_SENSOR);
	return 0;
}

static void __exit sprd_board_thermal_exit(void)
{
	sprd_boardthm_delete(SPRD_BOARD_SENSOR);
}

module_init(sprd_board_thermal_init);
module_exit(sprd_board_thermal_exit);




