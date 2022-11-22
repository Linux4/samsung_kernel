/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>

#include "sprd_battery.h"
#include <linux/gpio.h>

#ifdef SPRD_APTIMER1_BASE
#undef SPRD_APTIMER1_BASE
#endif
static unsigned long timer_base = 0;
#define SPRD_APTIMER1_BASE timer_base
#define TIMER_LOAD ((__iomem void *)SPRD_APTIMER1_BASE + 0x0000)
#define TIMER_VALUE ((__iomem void *)SPRD_APTIMER1_BASE + 0x0004)
#define TIMER_CTL ((__iomem void *)SPRD_APTIMER1_BASE + 0x0008)
#define TIMER_INT ((__iomem void *)SPRD_APTIMER1_BASE + 0x000C)

#define	ONETIME_MODE	(0 << 6)
#define	PERIOD_MODE	(1 << 6)

#define	TIMER_DISABLE	(0 << 7)
#define	TIMER_ENABLE	(1 << 7)

#define	TIMER_INT_EN	(1 << 0)
#define	TIMER_INT_STS	(1 << 2)
#define	TIMER_INT_CLR	(1 << 3)
#define	TIMER_INT_BUSY	(1 << 4)

static struct sprd_battery_platform_data *pbat_data;
void sprdchg_timer_enable(uint32_t cycles)
{
//#if !(defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8)) //mingwei TODO
	__raw_writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL);
	__raw_writel(32768 * cycles, TIMER_LOAD);
	__raw_writel(TIMER_ENABLE | PERIOD_MODE, TIMER_CTL);
	__raw_writel(TIMER_INT_EN, TIMER_INT);
//#endif
}

void sprdchg_timer_disable(void)
{
//#if !(defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8)) //mingwei TODO
	__raw_writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL);
//#endif
}

static int (*sprdchg_tm_cb) (void *data) = NULL;
static irqreturn_t _sprdchg_timer_interrupt(int irq, void *dev_id)
{
//#if !(defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8)) //mingwei TODO
	unsigned int value;

	printk("_sprdchg_timer_interrupt\n");

	value = __raw_readl(TIMER_INT);
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT);
	if (sprdchg_tm_cb) {
		sprdchg_tm_cb(dev_id);
	}
//#endif
	return IRQ_HANDLED;
}
extern int sprd_request_timer(int timer_id,int sub_id,unsigned long *base);
int sprdchg_timer_init(int (*fn_cb) (void *data), void *data)
{
	int ret = -ENODEV;
#if !(defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8))    //mingwei TODO
	if(sprd_request_timer(1,1,&timer_base))
		BUG_ON(1);
#else
	timer_base = ioremap_nocache(0X40320000 + 0x20, 0x80);
#endif

	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AP_TMR1_EB);
	sprdchg_timer_disable();
	sprdchg_tm_cb = fn_cb;

	ret =
	    request_irq(((struct sprdbat_drivier_data *)data)->
			pdata->irq_chg_timer, _sprdchg_timer_interrupt,
			IRQF_NO_SUSPEND | IRQF_TIMER, "battery_timer", data);

	if (ret) {
		printk(KERN_ERR "request battery timer irq %d failed\n",
		       IRQ_AONTMR0_INT);
	}
	return 0;
}

struct sprdbat_auxadc_cal adc_cal = {
	4200, 3310,
	3600, 2832,
	SPRDBAT_AUXADC_CAL_NO,
};
struct sprdbat_auxadc_cal temp_adc_cal = {
	1000, 3413,
	100, 341,
	SPRDBAT_AUXADC_CAL_NO,
};

static void sprdchg_get_temp_efuse_cal(void)
{
	extern int sci_temp_efuse_calibration_get(unsigned int *p_cal_data);
	unsigned int efuse_cal_data[2] = { 0 };
	if (sci_temp_efuse_calibration_get(efuse_cal_data)) {
		temp_adc_cal.p0_vol = efuse_cal_data[0] & 0xffff;
		temp_adc_cal.p0_adc = (efuse_cal_data[0] >> 16) & 0xffff;
		temp_adc_cal.p1_vol = efuse_cal_data[1] & 0xffff;
		temp_adc_cal.p1_adc = (efuse_cal_data[1] >> 16) & 0xffff;
		temp_adc_cal.cal_type = SPRDBAT_AUXADC_CAL_CHIP;
	}
	printk("sprdchg_temp_adc_to_vol %d,%d,%d,%d,cal_type:%d\n",
	       temp_adc_cal.p0_vol, temp_adc_cal.p0_adc, temp_adc_cal.p1_vol,
	       temp_adc_cal.p1_adc, temp_adc_cal.cal_type);
}

uint16_t sprdchg_small_scale_to_vol(uint16_t adcvalue)
{
	int32_t temp;
	temp = temp_adc_cal.p0_vol - temp_adc_cal.p1_vol;
	temp = temp * (adcvalue - temp_adc_cal.p0_adc);
	temp = temp / (temp_adc_cal.p0_adc - temp_adc_cal.p1_adc);
	temp = temp + temp_adc_cal.p0_vol;

	return temp;
}
static int __init adc_cal_start(char *str)
{
	unsigned int adc_data[2] = { 0 };
	char *cali_data = &str[1];
	if (str) {
		pr_info("adc_cal%s!\n", str);
		sscanf(cali_data, "%d,%d", &adc_data[0], &adc_data[1]);
		pr_info("adc_data: 0x%x 0x%x!\n", adc_data[0], adc_data[1]);
		adc_cal.p0_vol = adc_data[0] & 0xffff;
		adc_cal.p0_adc = (adc_data[0] >> 16) & 0xffff;
		adc_cal.p1_vol = adc_data[1] & 0xffff;
		adc_cal.p1_adc = (adc_data[1] >> 16) & 0xffff;
		adc_cal.cal_type = SPRDBAT_AUXADC_CAL_NV;
		printk
		    ("auxadc cal from cmdline ok!!! adc_data[0]: 0x%x, adc_data[1]:0x%x\n",
		     adc_data[0], adc_data[1]);
	}
	return 1;
}

__setup("adc_cal", adc_cal_start);

void sprdchg_init(struct sprd_battery_platform_data *pdata)
{
	pbat_data = pdata;

	BUG_ON(NULL == pbat_data);
	if (adc_cal.cal_type == SPRDBAT_AUXADC_CAL_NO) {

		#ifdef CONFIG_OTP_SPRD
		extern int sci_efuse_calibration_get(unsigned int *p_cal_data);
		unsigned int efuse_cal_data[2] = { 0 };
		if (sci_efuse_calibration_get(efuse_cal_data)) {
			adc_cal.p0_vol = efuse_cal_data[0] & 0xffff;
			adc_cal.p0_adc = (efuse_cal_data[0] >> 16) & 0xffff;
			adc_cal.p1_vol = efuse_cal_data[1] & 0xffff;
			adc_cal.p1_adc = (efuse_cal_data[1] >> 16) & 0xffff;
			adc_cal.cal_type = SPRDBAT_AUXADC_CAL_CHIP;
			printk
			    ("auxadc cal from efuse ok!!! efuse_cal_data[0]: 0x%x, efuse_cal_data[1]:0x%x\n",
			     efuse_cal_data[0], efuse_cal_data[1]);
		}
		#endif
	}
	sprdchg_get_temp_efuse_cal();
#if !(defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8)) //mingwei TODO
	sci_adi_write((ANA_CTL_EIC_BASE + 0x50), 1, (0xFFF));	//eic debunce
	printk("ANA_CTL_EIC_BASE0x%x\n", sci_adi_read(ANA_CTL_EIC_BASE + 0x50));
#endif

}
uint16_t sprdchg_bat_adc_to_vol(uint16_t adcvalue)
{
	int32_t temp;

	temp = adc_cal.p0_vol - adc_cal.p1_vol;
	temp = temp * (adcvalue - adc_cal.p0_adc);
	temp = temp / (adc_cal.p0_adc - adc_cal.p1_adc);
	temp = temp + adc_cal.p0_vol;

	return temp;
}

static uint16_t sprdchg_adc_to_vol(uint16_t channel, int scale,
				   uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vol;
	uint32_t m, n;
	uint32_t bat_numerators, bat_denominators;
	uint32_t numerators, denominators;

#if defined(CONFIG_ADIE_SC2723S) ||defined(CONFIG_ADIE_SC2723)
	vol = sprdchg_small_scale_to_vol(adcvalue);
	bat_numerators = 1;
	bat_denominators = 1;
#else
	vol = sprdchg_bat_adc_to_vol(adcvalue);
	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
#endif
	sci_adc_get_vol_ratio(channel, scale, &numerators, &denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = bat_denominators * numerators;
	m = vol * bat_numerators * (denominators);
	result = (m + n / 2) / n;
	return result;
}


int sprdchg_read_temp_adc(void)
{
#define SAMPLE_NUM  15
	int cnt = pbat_data->temp_adc_sample_cnt;

	if (cnt > SAMPLE_NUM) {
		cnt = SAMPLE_NUM;
	} else if (cnt < 1) {
		cnt = 1;
	}

	if (pbat_data->temp_support) {
		int ret, i, j, temp;
		int adc_val[cnt];
		struct adc_sample_data data = {
			.channel_id = pbat_data->temp_adc_ch,
			.channel_type = 0,	/*sw */
			.hw_channel_delay = 0,	/*reserved */
			.scale = pbat_data->temp_adc_scale,	/*small scale */
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
		printk("sprdchg: channel:%d,sprdchg_read_temp_adc:%d\n",
		       data.channel_id, adc_val[cnt / 2]);
		return adc_val[cnt / 2];
	} else {
		return 3000;
	}
}

static int sprdchg_temp_vol_comp(int vol)
{
	int bat_cur = sprdfgu_read_batcurrent();
	int res_comp = pbat_data->temp_comp_res;
	int vol_comp = 0;

	printk("sprdchg: sprdchg_temp_vol_comp bat_cur:%d\n",
				   bat_cur);
	vol_comp = (bat_cur * res_comp)/1000;
	vol = vol - vol_comp + ((vol_comp *(vol - vol_comp))/(1800 - vol_comp));
	if(vol < 0) vol = 0;

	return (vol);
}

#define TEMP_BUFF_CNT 5
static int temp_buff[TEMP_BUFF_CNT] = {200,200,200,200,200};
static void sprdchg_update_temp_buff(int temp)
{
	static int pointer = 0;
	if(pointer >= TEMP_BUFF_CNT) pointer = 0;

	temp_buff[pointer++] = temp;
}
static int sprdchg_get_temp_from_buff(void)
{
	int i = 0,j = 0,temp;
	int t_temp_buff[TEMP_BUFF_CNT] = {0};

	for(i = 0; i < TEMP_BUFF_CNT; i++) {
		t_temp_buff[i] = temp_buff[i];
		printk("sprdchg: temp_buff[%d]:%d\n",
		       i, temp_buff[i]);
	}

	for (j = 1; j <= TEMP_BUFF_CNT - 1; j++) {
		for (i = 0; i < TEMP_BUFF_CNT - j; i++) {
			if (t_temp_buff[i] > t_temp_buff[i + 1]) {
				temp = t_temp_buff[i];
				t_temp_buff[i] = t_temp_buff[i + 1];
				t_temp_buff[i + 1] = temp;
			}
		}
	}
#if 0
	for(i = 0; i < TEMP_BUFF_CNT;i++ ){
		printk("sprdchg: t_temp_buff[%d]:%d\n",
		       i, t_temp_buff[i]);
	}
#endif
	return t_temp_buff[TEMP_BUFF_CNT / 2];
}
int sprdchg_search_temp_tab(int val)
{
	return sprdbat_interpolate(val, pbat_data->temp_tab_size,
				   pbat_data->temp_tab);
}

#define TEMP_BUFF_EN
int sprdchg_read_temp(void)
{
	if (pbat_data->temp_support) {
		int temp;
		int val = sprdchg_read_temp_adc();
		//voltage mode
		if (pbat_data->temp_table_mode) {
			val =
			    sprdchg_adc_to_vol(pbat_data->temp_adc_ch,
					       pbat_data->temp_adc_scale, val);
			printk("sprdchg: sprdchg_read_temp voltage:%d,temp raw:%d\n", val,sprdchg_search_temp_tab(val));
			val = sprdchg_temp_vol_comp(val);
			printk("sprdchg: sprdchg_read_temp comp voltage:%d\n", val);
		}
		temp = sprdchg_search_temp_tab(val);
		//printk("sprdchg: sprdchg_read_temp temp comp:%d\n", temp);
#ifdef TEMP_BUFF_EN
		sprdchg_update_temp_buff(temp);
		temp = sprdchg_get_temp_from_buff();
#endif
		printk("sprdchg: sprdchg_read_temp temp result:%d\n", temp);
		return temp;
	} else {
		return 200;
	}
}

uint32_t sprdchg_read_vchg_vol(void)
{
	int vchg_value;
	vchg_value = sci_adc_get_value(SPRDBAT_ADC_CHANNEL_VCHG, false);
	return sprdchg_adc_to_vol(SPRDBAT_ADC_CHANNEL_VCHG, 0, vchg_value);	//sprdbat_charger_adc_to_vol(vchg_value);
}
uint32_t sprdchg_read_vbat_vol(void)
{
	uint32_t voltage;
	voltage =
	    sprdchg_bat_adc_to_vol(sci_adc_get_value(ADC_CHANNEL_VBAT, false));
	return voltage;
}
static uint32_t _sprdchg_read_chg_current(void)
{
	uint32_t vbat, isense;
	uint32_t cnt = 0;

	for (cnt = 0; cnt < 3; cnt++) {
		isense =
		    sprdchg_bat_adc_to_vol(sci_adc_get_value(ADC_CHANNEL_ISENSE,
							     false));
		vbat =
		    sprdchg_bat_adc_to_vol(sci_adc_get_value(ADC_CHANNEL_VBAT,
							     false));
		if (isense >= vbat) {
			break;
		}
	}
	if (isense > vbat) {
		uint32_t temp = ((isense - vbat) * 1000) / 68;	//(vol/68mohm)
		//printk(KERN_ERR "sprdchg: sprdchg_read_chg_current:%d\n", temp);
		return temp;
	} else {
		printk(KERN_ERR
		       "chg_current warning....isense:%d....vbat:%d\n",
		       isense, vbat);
		return 0;
	}
}
uint32_t sprdchg_read_chg_current(void)
{
#define CUR_RESULT_NUM 4

	int i, temp;
	volatile int j;
	uint32_t cur_result[CUR_RESULT_NUM];

	for (i = 0; i < CUR_RESULT_NUM; i++) {
		cur_result[i] = _sprdchg_read_chg_current();
	}

	for (j = 1; j <= CUR_RESULT_NUM - 1; j++) {
		for (i = 0; i < CUR_RESULT_NUM - j; i++) {
			if (cur_result[i] > cur_result[i + 1]) {
				temp = cur_result[i];
				cur_result[i] = cur_result[i + 1];
				cur_result[i + 1] = temp;
			}
		}
	}

	return cur_result[CUR_RESULT_NUM / 2];
}

uint32_t chg_cur_buf[SPRDBAT_AVERAGE_COUNT];
void sprdchg_put_chgcur(uint32_t chging_current)
{
	static uint32_t cnt = 0;

	if (cnt == SPRDBAT_AVERAGE_COUNT) {
		cnt = 0;
	}
	chg_cur_buf[cnt++] = chging_current;
}

uint32_t sprdchg_get_chgcur_ave(void)
{
	uint32_t i, sum = 0;
	for (i = 0; i < SPRDBAT_AVERAGE_COUNT; i++) {
		sum = sum + chg_cur_buf[i];
	}
	return sum / SPRDBAT_AVERAGE_COUNT;
}

int sprdchg_charger_is_adapter(void)
{
	int ret = ADP_TYPE_SDP;
	int charger_status;

	charger_status = sci_adi_read(ANA_REG_GLB_CHGR_STATUS)
	    & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);

	switch (charger_status) {
	case BIT_CDP_INT:
		ret = ADP_TYPE_CDP;
		break;
	case BIT_DCP_INT:
		ret = ADP_TYPE_DCP;
		break;
	case BIT_SDP_INT:
		ret = ADP_TYPE_SDP;
		break;
	default:
		ret = ADP_TYPE_SDP;
		break;
	}
	return ret;
}
#ifdef CONFIG_LEDS_TRIGGERS
void sprdchg_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brightness)
{
	if (brightness == LED_FULL) {
		sci_adi_clr(ANA_REG_GLB_ANA_DRV_CTRL, BIT_KPLED_PD);
	} else {
		sci_adi_set(ANA_REG_GLB_ANA_DRV_CTRL, BIT_KPLED_PD);
	}
}
#endif

